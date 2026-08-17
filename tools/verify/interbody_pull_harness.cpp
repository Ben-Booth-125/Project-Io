// interbody_pull_harness — the inter-body demand pull (BL-263): which market it
// reads (BL-406) and whether its netting means anything (BL-404).
//
// STATUS 2026-08-17: both landed. Sections R1/R2/R2a below are the ORIGINAL
// MEASUREMENT that found the defects, kept because they are the record of what
// was wrong and of the numbers that decided the fix; they no longer describe the
// shipped code path. Section C is the live guard on the shipped behaviour, and
// every one of its assertions was run against the pre-change build and observed
// to FAIL (3 of 3). Read C for what the code does now.
//
// WHAT THIS EXISTED TO SETTLE. `inject_interbody_demand` pulls a distance-
// discounted slice of the home body's *unmet* demand onto every outpost market,
// so an outpost with real supply and no local population does not collapse to
// the price floor. It expressed "unmet" as `home.demand[r] - home.supply[r]`,
// where `home` was `market_for_body(w, w.home_body)`.
//
// Two separate things were wrong with that expression, and they needed different
// fixes, so this harness measured both rather than asserting either:
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
// So "home supply" was not the home body's supply, and "home demand" was a tenth
// of the home body's demand. That made BL-404 un-fixable by sequencing alone, in
// both directions at once: net against THAT market's supply and the correction is
// a no-op; net against the body's real supply and the shortfall goes permanently
// negative, killing the price-floor support BL-263 added.
//
// HOW IT WAS RESOLVED (2026-08-17). Neither, in the end. Ben's ruling took
// BL-406 option (c): the pull reads a per-resource COUNTERPART market — the
// home-body market that wants that resource most — so nothing is asked to stand
// for a body, and the aggregate-vs-arbitrary dilemma above simply does not
// arise. BL-404 then took option (b) on top, netting against that counterpart's
// PREVIOUS-tick supply. The measured outcome on seed 0: two resources pull at
// 2.8x their old size and one (whose counterpart holds 260.7 supply against 5.5
// demand) is correctly silenced. Section C asserts exactly that.
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

#include <algorithm>
#include <array>
#include <cmath>
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
          "R1 a LIVE supply read at the injector's read point would still be a no-op — the "
          "reason the shipped netting reads a pre-reset snapshot instead (BL-404)");

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
          "R2a the home body carries SEVERAL markets, so no single one can stand for it — the "
          "premise BL-406 dissolved, and the reason section C's counterpart relation is needed");
    check(body_demand > 0.0 && read_demand < body_demand,
          "R2a the lowest-id market holds only a slice of the home body's demand — what the "
          "pull used to be scaled off, kept as the record of the defect");

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

    // ---- C: the counterpart pull, asserted behaviourally --------------------
    //
    // BL-406 (Ben's ruling 2026-08-15, option c) and BL-404 land together, and
    // these checks are written against `inject_interbody_demand`'s OBSERVABLE
    // output rather than against any function it calls internally. That is
    // deliberate: an assertion that named a new helper could not be compiled
    // against the pre-change build, and a guard that has never been seen to fail
    // is not a guard. In this form the identical assertions were run against the
    // pre-change binary — see the recorded failures below each one.
    //
    // The relation: for resource r, an outpost's COUNTERPART is the home-body
    // market with the greatest demand for r (lowest market id breaking ties).
    // No single market stands for the body any more, so BL-406's defect is
    // dissolved rather than repaired — and the pick is a fact about the economy,
    // identical under any container traversal order.
    {
        const market_emergence_params& mp = reg.market_emergence();

        // An outpost body: the lowest-id body that is neither home nor the star
        // and carries at least one tile. Lowest id, so the pick is stable.
        auto radius_au = [&](entity_id id) -> float {
            const auto it = w.bodies.find(id);
            if (it == w.bodies.end()) return 0.0f;
            const body_component* bc = &it->second;
            if (bc->parent != null_entity)
            {
                const auto pit = w.bodies.find(bc->parent);
                if (pit != w.bodies.end()) bc = &pit->second;
            }
            return bc->orbital_radius_au;
        };

        entity_id outpost_body = null_entity;
        entity_id outpost_tile = null_entity;
        for (const auto& [bid, bc] : w.bodies)
        {
            (void)bc;
            if (bid == w.home_body || bid == w.star_body) continue;
            entity_id lowest_tile = null_entity;
            for (const auto& [tid, tc] : w.tiles)
                if (tc.body == bid && (lowest_tile == null_entity || tid < lowest_tile))
                    lowest_tile = tid;
            if (lowest_tile == null_entity) continue;
            if (outpost_body == null_entity || bid < outpost_body)
            {
                outpost_body = bid;
                outpost_tile = lowest_tile;
            }
        }
        check(outpost_body != null_entity,
              "C setup: the generated world carries a non-home body with tiles to host an "
              "outpost market");

        entity_id outpost_market = null_entity;
        if (outpost_body != null_entity)
            outpost_market = maybe_spawn_market(w, reg, outpost_body, outpost_tile);
        check(outpost_market != null_entity,
              "C setup: an outpost market spawns on that body (nothing had built there yet, so "
              "the generated world has no off-home market of its own)");

        if (outpost_market != null_entity)
        {
            const float dist    = std::fabs(radius_au(outpost_body) - radius_au(w.home_body));
            const float falloff = 1.0f + mp.distance_falloff * dist;

            // The two candidate answers to "whose demand is the pull scaled off".
            // `lowest` is the pre-change behaviour (market_for_body's lowest-id
            // pick); `counterpart` is the ruling's per-resource relation. Also
            // capture the counterpart's PRIOR supply — the end-of-tick supply a
            // snapshot taken before clear_markets' reset loop would carry, which
            // is what BL-404's netting must subtract.
            std::array<float, resource_count> lowest_demand{};
            std::array<float, resource_count> counterpart_demand{};
            std::array<float, resource_count> counterpart_supply{};
            std::array<entity_id, resource_count> counterpart_id{};
            std::array<entity_id, resource_count> counterpart_id_reversed{};
            lowest_demand.fill(0.0f);
            counterpart_demand.fill(0.0f);
            counterpart_supply.fill(0.0f);
            counterpart_id.fill(null_entity);
            counterpart_id_reversed.fill(null_entity);

            // Home-body markets, gathered once. Two traversals of the SAME set in
            // opposite id orders re-derive the relation, so a tie-break that
            // silently depended on visit order would disagree with itself.
            std::vector<entity_id> home_ids;
            for (const auto& [mid, mc] : w.markets)
                if (mc.body == w.home_body) home_ids.push_back(mid);
            std::sort(home_ids.begin(), home_ids.end());

            auto derive = [&](const std::vector<entity_id>& order,
                              std::array<entity_id, resource_count>& out) {
                out.fill(null_entity);
                std::array<float, resource_count> best{};
                best.fill(0.0f);
                for (entity_id mid : order)
                {
                    const market_component& mc = w.markets.at(mid);
                    for (std::size_t r = 0; r < resource_count; ++r)
                    {
                        const float d = mc.demand[r];
                        if (out[r] == null_entity || d > best[r] ||
                            (d == best[r] && mid < out[r]))
                        {
                            out[r]  = mid;
                            best[r] = d;
                        }
                    }
                }
            };
            derive(home_ids, counterpart_id);
            std::vector<entity_id> reversed(home_ids.rbegin(), home_ids.rend());
            derive(reversed, counterpart_id_reversed);

            for (std::size_t r = 0; r < resource_count; ++r)
            {
                lowest_demand[r] = w.markets.at(home_market).demand[r];
                if (counterpart_id[r] != null_entity)
                {
                    const market_component& cm = w.markets.at(counterpart_id[r]);
                    counterpart_demand[r] = cm.demand[r];
                    counterpart_supply[r] = cm.supply[r];
                }
            }

            bool order_stable = true;
            for (std::size_t r = 0; r < resource_count; ++r)
                if (counterpart_id[r] != counterpart_id_reversed[r]) order_stable = false;
            check(order_stable,
                  "C1 the counterpart relation is order-independent — re-derived over the "
                  "home-body markets in reverse id order it names the same market for every "
                  "resource (BL-406's cross-toolchain instability closed)");

            // Fire the injector and read what actually landed.
            market_component& om = w.markets.at(outpost_market);
            om.demand.fill(0.0f);
            inject_interbody_demand(w, reg, snapshot_market_supply(w));

            std::printf("C — what the pull is scaled off (outpost market %u, body %u, %.2f AU)\n",
                        outpost_market, outpost_body, dist);
            std::printf("     %-14s %10s %12s %12s %12s %12s\n",
                        "resource", "injected", "cpart short", "lowest short", "cpart demand",
                        "cpart supply");

            int differing        = 0;  // the two candidate sources disagree here
            int matches_cpart    = 0;
            int matches_lowest   = 0;
            int netting_bites    = 0;  // counterpart supply >= its demand -> pull must be silenced
            int netting_silenced = 0;
            int netting_arith_ok = 0;
            int netted_pulls     = 0;
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                if (om.base_price[r] <= 0.0f) continue;              // untradeable, never pulled
                if (counterpart_demand[r] <= 0.0f && lowest_demand[r] <= 0.0f) continue;

                const float injected = om.demand[r];

                // The two candidate sources, each carried through the SAME
                // shortfall-and-falloff arithmetic, so the comparison isolates
                // WHICH MARKET is read and nothing else. Comparing the injected
                // value against a gross demand instead would conflate BL-406's
                // question with BL-404's: a counterpart whose pull the netting
                // silences injects zero, which is the right answer to BL-406 and
                // looks like the wrong one.
                const float cpart_short  = counterpart_demand[r] - counterpart_supply[r];
                const float lowest_short = lowest_demand[r] - w.markets.at(home_market).supply[r];
                const float expect_cpart =
                    cpart_short  > 0.0f ? cpart_short  * mp.pull_fraction / falloff : 0.0f;
                const float expect_lowest =
                    lowest_short > 0.0f ? lowest_short * mp.pull_fraction / falloff : 0.0f;

                std::printf("     %-14s %10.4f %12.4f %12.4f %12.4f %12.2f\n",
                            resource_label(r).c_str(), injected, cpart_short, lowest_short,
                            counterpart_demand[r], counterpart_supply[r]);

                if (std::fabs(expect_cpart - expect_lowest) > 1e-4f)
                {
                    ++differing;
                    if (std::fabs(injected - expect_cpart)  < 1e-3f) ++matches_cpart;
                    if (std::fabs(injected - expect_lowest) < 1e-3f) ++matches_lowest;
                }
                if (counterpart_supply[r] >= counterpart_demand[r] && counterpart_demand[r] > 0.0f)
                {
                    ++netting_bites;
                    if (injected == 0.0f) ++netting_silenced;
                }
                else if (cpart_short > 0.0f)
                {
                    ++netted_pulls;
                    if (std::fabs(injected * falloff / mp.pull_fraction - cpart_short) < 1e-3f)
                        ++netting_arith_ok;
                }
            }
            std::printf("\n     resources where the two sources disagree : %d\n", differing);
            std::printf("     ...pull scaled off the COUNTERPART        : %d\n", matches_cpart);
            std::printf("     ...pull scaled off the lowest-id market   : %d\n", matches_lowest);
            std::printf("     resources the netting must silence        : %d (silenced %d)\n",
                        netting_bites, netting_silenced);
            std::printf("     resources with a positive netted pull     : %d (arithmetic ok %d)\n\n",
                        netted_pulls, netting_arith_ok);

            // C2 — BL-406. The load-bearing one. PRE-CHANGE THIS FAILED: the
            // injector read market_for_body's lowest-id pick, so `implied`
            // matched `lowest-id` on every differing resource and the
            // counterpart on none.
            check(differing > 0,
                  "C2 setup: the counterpart and the lowest-id market imply different pulls for "
                  "at least one resource, so the two answers are distinguishable here");
            check(differing > 0 && matches_cpart == differing && matches_lowest == 0,
                  "C2 the pull is scaled off the per-resource COUNTERPART market's demand, not "
                  "off the lowest-id market market_for_body happens to return (BL-406)");

            // C3 — BL-404. PRE-CHANGE THIS FAILED: the subtrahend was the
            // identically-zero supply at the injector's read point, so a
            // counterpart already supplying its own demand still pulled.
            check(netting_bites == 0 || netting_silenced == netting_bites,
                  "C3 a resource whose counterpart already meets its own demand from prior-tick "
                  "supply pulls NOTHING — the netting is real, not a subtraction of zero (BL-404)");
            check(netted_pulls == 0 || netting_arith_ok == netted_pulls,
                  "C3 where a pull survives, its size is the counterpart's demand LESS its "
                  "prior-tick supply — the shortfall the mechanism has always claimed to use");
        }
    }

    std::printf("\n%s (%d failure%s)\n",
                failures == 0 ? "ALL PASS" : "FAILURES",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
