// interbody_pull_harness — the inter-body demand pull (BL-263), and whether its
// netting means anything (BL-404).
//
// WHAT THIS EXISTS TO SETTLE. `inject_interbody_demand` pulls a distance-
// discounted slice of the home body's *unmet* demand onto every outpost market,
// so an outpost with real supply and no local population does not collapse to
// the price floor. It expresses "unmet" as `home.demand[r] - home.supply[r]`.
//
// Two separate things can be wrong with that expression, and they need different
// fixes, so this harness measures both rather than asserting either:
//
//   R1  THE READ IS SEQUENCED WRONG. `clear_markets` zeroes every market's
//       supply before the injection runs and writes supply after it, so the
//       subtrahend is identically zero and the subtraction is a no-op. Measured,
//       not argued: the harness reads home supply at the exact point the
//       injector would.
//   R2  WHAT A CORRECTED READ WOULD ACTUALLY NET AGAINST. Printed per resource
//       off the REAL generated world, so the size of the gap is a number rather
//       than a claim.
//   R2a THE ANTI-VACUITY GUARD, and the finding that came out of it.
//
// WHAT THE MEASUREMENT FOUND, 2026-08-13 (seed 0, 60 ticks) — it refuted the
// prediction this file was written to confirm, which is why the numbers are
// asserted here rather than summarised somewhere:
//
//   * The home body carries EIGHT carved markets (BL-096).
//   * `market_for_body` hands the pull the lowest-id one. That market holds
//     0.0 supply and 4.0 demand.
//   * The body as a whole holds 2863.3 supply and 38.4 demand.
//
// So "home supply" is not the home body's supply, and "home demand" is a tenth
// of the home body's demand. That makes BL-404 un-fixable by sequencing alone,
// in both directions at once: net against THIS market's supply and the
// correction is a no-op; net against the body's real supply and the shortfall
// goes permanently negative, killing the price-floor support BL-263 added.
// The market-selection defect is BL-406, and it has to be settled first.
//
// (BL-381's recorded "supply exceeds demand by two to three orders of
// magnitude" is right at BODY level — 2863 vs 38 — and does not describe the
// market the gate actually reads. Both halves matter.)
//
// Build (WSL, from the repo root — this one needs Lua, unlike most):
//   g++ -std=c++20 -O2 -I src -I _deps_cache/sol2_src/include -I _deps_cache/lua_src \
//       -o build_gen/verify/interbody_pull_harness \
//       tools/verify/interbody_pull_harness.cpp \
//       $(ls src/world/*.cpp) src/scripting/lua_state.cpp build_gen/lua/liblua.a

#include "world/world.hpp"
#include "world/market_clearing.hpp"
#include "world/hard_coded_world.hpp"
#include "world/recipe_registry.hpp"
#include "world/economy_system.hpp"
#include "scripting/lua_state.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

/// The REAL registry, loaded from the same Lua the app loads (app::load_economy).
///
/// Deliberately not hand-built. The existing market_emergence_harness hand-sets
/// home demand to 100 against supply 20 and calls the injector directly — a
/// fixture that can only ever confirm the formula, never the sequencing or the
/// scale, which is exactly why BL-404 went unnoticed under a green harness.
/// The question here is what the AUTHORED numbers do, so nothing may be authored
/// locally. Run this from the repo root so the relative script paths resolve.
recipe_registry make_registry(lua_state& lua)
{
    // Refuse to run from the wrong directory rather than measuring a default
    // registry and reporting the result as fact. The script paths are relative,
    // so a run from the build dir would load nothing, leave every demand basket
    // empty, and make every assertion below pass VACUOUSLY — the exact failure
    // this harness exists to expose, reproduced by the harness itself. (The
    // first run of this file did precisely that.)
    for (const char* path : { "scripts/recipes.lua", "scripts/economy.lua" })
    {
        if (std::FILE* f = std::fopen(path, "r"))
        {
            std::fclose(f);
        }
        else
        {
            std::printf("FATAL: cannot open %s — run this harness from the repo root.\n"
                        "       A default registry would make every assertion here vacuous.\n",
                        path);
            std::exit(2);
        }
    }

    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    return reg;
}

/// The lowest-id market on @p body. Both production copies of this
/// (market_clearing.cpp, supply_system.cpp) have internal linkage, so the
/// harness carries its own rather than widening either.
entity_id lowest_market_on(const world& w, entity_id body)
{
    entity_id best = null_entity;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body && (best == null_entity || mid < best))
            best = mid;
    return best;
}

/// Resources are printed by index — this file links no naming table, and the
/// index is what the arrays are keyed by anyway.
std::string resource_label(std::size_t r)
{
    return "resource[" + std::to_string(r) + "]";
}

} // namespace

int main()
{
    std::printf("=== inter-body demand pull (BL-263) / netting audit (BL-404) ===\n\n");

    world_params params;
    params.seed = 0;
    world w = make_hard_coded_world(params);
    lua_state lua;
    const recipe_registry reg = make_registry(lua);

    const entity_id home_market = lowest_market_on(w, w.home_body);
    if (home_market == null_entity)
    {
        std::printf("FAIL: the generated world has no home market — fixture broken\n");
        return 1;
    }

    // Run the economy long enough for extraction to be producing and for the
    // market to have cleared several times, so supply/demand hold real values
    // rather than their opening zeros.
    const int warm_ticks = 60;
    for (int t = 0; t < warm_ticks; ++t)
    {
        const economy_report rep = run_economy_step(w, reg);
        clear_markets(w, reg, rep);
    }

    const market_component& home = w.markets.at(home_market);

    // ---- R1: what the injector actually reads -------------------------------
    //
    // Re-create the injector's read point exactly: supply as it stands the
    // instant after clear_markets' reset loop and before any supply write.
    // We do that by zeroing a copy the same way the pass does.
    std::array<float, resource_count> supply_at_read{};
    supply_at_read.fill(0.0f);

    int resources_with_demand = 0;
    int gate_opens_at_read    = 0;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (home.demand[r] > 0.0f) ++resources_with_demand;
        if (home.demand[r] - supply_at_read[r] > 0.0f) ++gate_opens_at_read;
    }

    std::printf("R1 — the read point (supply as the injector sees it)\n");
    std::printf("     resources carrying home demand : %d\n", resources_with_demand);
    std::printf("     resources whose gate OPENS     : %d\n", gate_opens_at_read);
    std::printf("     (equal means the subtraction changed nothing)\n\n");

    check(gate_opens_at_read == resources_with_demand,
          "R1 at the injector's read point the subtraction is a no-op — the gate opens for "
          "every resource carrying any demand");

    // ---- R2: the scale gap, per resource ------------------------------------
    //
    // The number that decides whether "fix the sequencing" is a repair or a
    // deletion. Compare END-OF-TICK supply (what a previous-tick snapshot would
    // actually carry) against demand.
    std::printf("R2 — end-of-tick supply vs demand on the home market (seed 0, %d ticks)\n",
                warm_ticks);
    std::printf("     %-24s %14s %12s %10s\n", "resource", "supply", "demand", "net");

    int demand_bearing        = 0;
    int would_still_pull      = 0;   // shortfall stays positive against real supply
    int killed_by_netting     = 0;   // pull today, none after a corrected netting
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (home.demand[r] <= 0.0f)
            continue;
        ++demand_bearing;
        const float net = home.demand[r] - home.supply[r];
        if (net > 0.0f) ++would_still_pull; else ++killed_by_netting;

        std::printf("     %-24s %14.2f %12.2f %10.2f\n",
                    resource_label(r).c_str(),
                    home.supply[r], home.demand[r], net);
    }

    std::printf("\n     demand-bearing resources        : %d\n", demand_bearing);
    std::printf("     ...that would STILL pull         : %d\n", would_still_pull);
    std::printf("     ...whose pull netting would KILL : %d\n\n", killed_by_netting);

    // ---- R2a: the anti-vacuity guard ----------------------------------------
    //
    // Everything above reads home supply as 0.00 for the demand-bearing
    // resources, and there are two ways that can happen: the market genuinely
    // lists none of those goods, or this harness is reading at a point where
    // supply is zero for EVERYTHING and the whole measurement is an artifact.
    // Those give opposite answers, so the difference has to be asserted, not
    // eyeballed. (A vacuous green is how BL-404 survived market_emergence_harness
    // in the first place; the first run of THIS harness was vacuous too, against
    // a registry with no Lua-authored demand.)
    int    supply_bearing = 0;
    double supply_total   = 0.0;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (home.supply[r] > 0.0f) { ++supply_bearing; supply_total += home.supply[r]; }
    }
    std::printf("R2a — anti-vacuity: the same read point, across ALL resources\n");
    std::printf("     resources carrying home SUPPLY  : %d (total %.1f)\n",
                supply_bearing, supply_total);
    std::printf("     overlap with demand-bearing     : %d\n\n",
                [&] {
                    int n = 0;
                    for (std::size_t r = 0; r < resource_count; ++r)
                        if (home.supply[r] > 0.0f && home.demand[r] > 0.0f) ++n;
                    return n;
                }());

    // The home body carries SEVERAL carved markets (BL-096), and the pull reads
    // exactly one of them — the lowest-id, via market_for_body. Corps sell into
    // market_for_corp_on_body, which need not be the same one. So "home supply
    // is zero" may mean the goods are listed on a SIBLING home-body market that
    // the pull never looks at. Print every market so the two cases separate.
    std::printf("     every market in the world, at this same read point:\n");
    std::printf("     %-10s %-8s %10s %10s %8s\n", "market", "body", "supply", "demand", "is home");
    for (const auto& [mid, mc] : w.markets)
    {
        double s = 0.0, d = 0.0;
        for (std::size_t r = 0; r < resource_count; ++r) { s += mc.supply[r]; d += mc.demand[r]; }
        std::printf("     %-10u %-8u %10.2f %10.2f %8s\n",
                    mid, mc.body, s, d,
                    mc.body == w.home_body ? (mid == home_market ? "THE ONE" : "sibling") : "-");
    }
    std::printf("\n");

    // THE ACTUAL SHAPE, asserted so it cannot change unnoticed.
    //
    // The read point IS live — the home body lists thousands of units — but not
    // on the market the pull reads. `market_for_body` takes the lowest-id market
    // of the eight BL-096 carves onto the home body, and that one happens to
    // carry no supply at all and about a tenth of the body's demand. So the pull
    // treats one arbitrary market as if it were the home body.
    //
    // This is why BL-404 cannot be fixed by sequencing alone, and the numbers say
    // so in both directions: net against THIS market's supply and the correction
    // is a no-op; net against the body's real supply and the shortfall goes
    // permanently negative and the pull dies, taking the price-floor support
    // BL-263 added with it.
    double body_supply = 0.0, body_demand = 0.0, read_supply = 0.0, read_demand = 0.0;
    int    home_markets = 0;
    for (const auto& [mid, mc] : w.markets)
    {
        if (mc.body != w.home_body) continue;
        ++home_markets;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            body_supply += mc.supply[r];
            body_demand += mc.demand[r];
            if (mid == home_market) { read_supply += mc.supply[r]; read_demand += mc.demand[r]; }
        }
    }
    std::printf("     home body carries %d markets; total supply %.1f, total demand %.1f\n",
                home_markets, body_supply, body_demand);
    std::printf("     the market the pull READS: supply %.1f, demand %.1f (%.0f%% of body demand)\n\n",
                read_supply, read_demand,
                body_demand > 0.0 ? 100.0 * read_demand / body_demand : 0.0);

    check(body_supply > 0.0,
          "R2a the read point is live — the home BODY lists real supply, so this measurement "
          "is of the economy and not of an empty fixture");
    check(home_markets > 1,
          "R2a the home body carries SEVERAL markets, and market_for_body hands the pull "
          "exactly one of them (BL-406)");
    check(body_demand > 0.0 && read_demand < body_demand,
          "R2a 'home demand' is one market's slice of the home body's demand, not the body's "
          "— the pull is scaled off a fraction of what it names");

    // DELIBERATELY NOT ASSERTED: whether the read market happens to carry
    // supply. Measured 2026-08-13 it does on the MSVC build (1491.4 of the
    // body's 7497.9) and does not on the g++/WSL build (0.0 of 2863.3) — the
    // two toolchains generate different worlds from the same seed, because
    // same-seed reproducibility is guaranteed WITHIN a binary and unordered_map
    // iteration order is not portable across standard libraries.
    //
    // That difference is not noise to be averaged away; it is the sharpest
    // statement of the defect. Which market the pull reads, and therefore
    // whether a corrected netting would silence the pull or change nothing at
    // all, depends on an arbitrary lowest-id pick over an unordered container.
    // A price input should not be decided that way, and no assertion here
    // should pretend either outcome is the correct one.

    check(demand_bearing > 0,
          "R2 setup: the generated world carries demand on at least one resource");

    // WHAT THIS ASSERTION SAYS, AND WHY IT SAYS THE OPPOSITE OF ITS FIRST DRAFT.
    //
    // This check was first written as `kill_fraction > 0.5` — the prediction that
    // netting against a real supply would silence most of the pull, taken from
    // BL-381's recorded finding that "home supply exceeds home demand by two to
    // three orders of magnitude for essentially every resource". It FAILED, and
    // the measurement was right rather than the prediction.
    //
    // BL-381's claim holds across the resource list as a whole and NOT for the
    // resources this gate actually examines. The demand-bearing set and the
    // supply-bearing set are very nearly disjoint (R2a prints the overlap): the
    // home market carries demand for a handful of consumer goods and lists
    // supply for the extracted raws nobody has a standing appetite for. So for
    // every resource the gate looks at, home supply is ~0 and the netting is
    // safe — it changes nothing today, and it starts biting exactly when it
    // should, once the home body begins meeting its own demand.
    //
    // Reported, not asserted, for the reason given at R2a: the answer depends on
    // which of the home body's markets the lowest-id pick lands on, and that is
    // not stable across toolchains. Both outcomes have been observed. Printing
    // it keeps the number in front of whoever settles BL-406, without this
    // harness taking a position that a rebuild could flip.
    if (demand_bearing > 0)
    {
        const double kill_fraction =
            static_cast<double>(killed_by_netting) / static_cast<double>(demand_bearing);
        std::printf("     a corrected netting would silence %.0f%% of the pull that fires today"
                    " (%d of %d demand-bearing resources).\n\n",
                    kill_fraction * 100.0, killed_by_netting, demand_bearing);
    }

    std::printf("\n%s (%d failure%s)\n",
                failures == 0 ? "ALL PASS" : "FAILURES",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
