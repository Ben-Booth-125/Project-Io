// convoy_cargo_census — is `Agricultural Produce x0` an empty convoy, or a
// rounded one? (BL-689, the Convoys ledger's inherited defect)
//
// WHY THIS EXISTS. The Convoys view rendered a row reading `Agricultural
// Produce x0`: a convoy in flight, haul cost paid, progress bar running, and
// an apparently EMPTY hold. `convoys.md` records it as "either a real defect in
// dispatch or a state nothing explains", and the brief for BL-689 required it
// be MEASURED before the surface was rebuilt around it — if dispatch can
// legitimately commit an empty convoy the row must say why, and if it cannot,
// the ledger has revealed a supply-layer bug.
//
// Reading the code answers half of it. Neither dispatch path can commit a
// zero-quantity convoy:
//
//   - the auto path (`dispatch_convoys`, supply_system.cpp) skips a source
//     whose `surplus <= 0.0f` and skips a destination whose `shortfall <= 0`,
//     then takes `qty = min(surplus, shortfall)` — strictly positive by
//     construction;
//   - the directed verb (`corp_verb::dispatch_convoy`, corp_command.cpp)
//     rejects the whole command on `!(cmd.quantity > 0.0f)`, finiteness first.
//
// So `cargo_qty == 0` should be unreachable. What the code does NOT settle is
// what the ledger was actually looking at, because `cargo_qty` is a FLOAT and
// the row printed it with `"x%.0f"` — which renders every cargo below 0.5 as
// "x0". A 0.3-unit convoy and an empty one are the same six pixels.
//
// WHAT THIS MEASURES, on the real generated world over a real tick loop:
//   1. how many in-flight convoys carry EXACTLY zero (the dispatch-bug case);
//   2. how many carry a positive cargo that `%.0f` renders as "0" (the
//      formatting case) — i.e. 0 < cargo_qty < 0.5;
//   3. the cargo distribution generally, so the format string can be chosen
//      against the real magnitudes rather than guessed.
//
// It ASSERTS (1) is zero: no convoy in flight may carry nothing. That is the
// supply-layer invariant both dispatch paths are written to hold, and it had
// never been checked. (2) is reported, not asserted — it is a presentation
// choice, and the count is the evidence for which format string is right.
//
// Run: .\build_gen\verify\convoy_cargo_census.exe [seeds] [ticks]

#include "scripting/lua_state.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/hard_coded_world.hpp"
#include "world/budget_system.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/supply_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"
#include "harness_params.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok)
        ++g_failures;
}

double quantile(std::vector<double> v, double q)
{
    if (v.empty())
        return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t i = static_cast<std::size_t>(q * static_cast<double>(v.size() - 1) + 0.5);
    return v[std::min(i, v.size() - 1)];
}

} // namespace

int main(int argc, char** argv)
{
    const int n_seeds = (argc > 1) ? std::atoi(argv[1]) : 5;
    const int n_ticks = (argc > 2) ? std::atoi(argv[2]) : 20;
    if (n_seeds <= 0 || n_ticks <= 0)
    {
        std::printf("usage: %s [seeds] [ticks]  (both positive)\n", argv[0]);
        return 2;
    }

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    world_gen_config gen_cfg;
    gen_cfg.load_from_lua(lua);
    if (reg.recipe_count(building_type::processing_facility) == 0)
    {
        std::printf("FATAL: no recipes loaded - run from the repo root.\n");
        return 2;
    }

    std::printf("=== convoy_cargo_census (BL-689) - is `x0` empty, or rounded? ===\n");
    std::printf("%d seeds x %d ticks, real generated world\n\n", n_seeds, n_ticks);

    // Every cargo quantity observed at dispatch, pooled over seeds.
    std::vector<double> cargo;
    long n_exact_zero = 0;   // cargo_qty == 0        -> a dispatch bug
    long n_renders_as_zero = 0; // 0 < cargo_qty < 0.5 -> a format bug
    long n_sub_one = 0;      // 0 < cargo_qty < 1.0    -> "x1" would also lie
    long n_total = 0;

    for (int s = 0; s < n_seeds; ++s)
    {
        world_params p = no_prehistory();
        p.seed = static_cast<uint32_t>(s);
        world w = make_hard_coded_world(p, nullptr, gen_cfg);
        assign_default_recipes(w, reg);
        generate_background_firms(w, reg, static_cast<uint32_t>(s) ^ 0x8A21F00Du);
        assign_default_recipes(w, reg);

        std::size_t seen = 0;
        for (int t = 1; t <= n_ticks; ++t)
        {
            dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                             reg.logistics_cost(convoy_mode::space));
            // Only convoys appended THIS tick — w.convoys is append-only within
            // a tick and compacted by advance/credit, so walking from the
            // previous size is the honest dispatch count (haulage_measure.cpp
            // uses the same walk).
            for (std::size_t i = seen; i < w.convoys.size(); ++i)
            {
                const convoy_component& c = w.convoys[i];
                const double q = static_cast<double>(c.cargo_qty);
                ++n_total;
                cargo.push_back(q);
                if (!(q > 0.0))
                    ++n_exact_zero;
                else
                {
                    if (q < 0.5) ++n_renders_as_zero;
                    if (q < 1.0) ++n_sub_one;
                }
            }
            advance_convoys(w);
            const economy_report report = run_economy_step(w, reg);
            const auto flows = clear_markets(w, reg, report);
            apply_budget(w, reg, flows, report.workforce_contention, nullptr);
            credit_arrived_convoys(w, t);
            seen = w.convoys.size();
        }
    }

    std::printf("--- cargo quantity at dispatch ---\n");
    std::printf("  convoys dispatched                : %ld\n", n_total);
    std::printf("  cargo_qty == 0 (EMPTY hold)       : %ld\n", n_exact_zero);
    std::printf("  0 < cargo_qty < 0.5 (prints \"x0\") : %ld\n", n_renders_as_zero);
    std::printf("  0 < cargo_qty < 1.0               : %ld\n", n_sub_one);
    if (n_total > 0)
    {
        std::printf("  share printing as \"x0\"            : %.1f%%\n",
                    100.0 * static_cast<double>(n_renders_as_zero)
                          / static_cast<double>(n_total));
        std::printf("\n  distribution: min %.4f  p10 %.4f  median %.4f  p90 %.4f  max %.4f\n",
                    quantile(cargo, 0.0), quantile(cargo, 0.10), quantile(cargo, 0.50),
                    quantile(cargo, 0.90), quantile(cargo, 1.0));
    }

    std::printf("\n--- assertions ---\n");
    // The invariant both dispatch paths are written to hold, never checked
    // until now. If this fails, `x0` is a supply-layer bug and not a format one.
    check(n_exact_zero == 0,
          "no convoy is dispatched with an empty hold (cargo_qty > 0 always)");
    // Vacuity guard: a green run over a world that dispatched nothing proves
    // nothing at all.
    check(n_total > 0, "the measured world actually dispatched convoys");

    std::printf("\n=== convoy_cargo_census: %d failure(s) ===\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
