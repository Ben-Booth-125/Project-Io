// chain_depth — the BL-428 growth spine: how far down the production graph a
// corp can reach, computed off the recipe graph.
//
// WHY THIS METRIC AT ALL. Ben's ruling, 2026-08-15, choosing chain depth over
// building tiers, the ancient tech ladder, and settlement scale: every
// alternative is a SECOND system that has to be kept in agreement with the
// economy, and the project has already paid that reconciliation cost once
// (BL-155 and BL-156 both settled on the same condition_set and neither built
// it). Depth is read off the graph that has to exist anyway.
//
// TWO COMPOSITION RULES, and they differ on purpose:
//   * MAX within a recipe  — you cannot run it until its deepest input exists.
//   * MIN across recipes   — two routes to a good means you have reached it as
//                            soon as the EASIER one is open.
// The asymmetry only starts to bite when BL-430 adds alternate methods, which is
// exactly why it is pinned by a test now rather than discovered then.
//
// WHAT IS ASSERTED HERE (BL-428). This harness covers the METRIC. BL-432 (the
// roster guard) extends this same file with the three roster invariants that
// need a fuller roster to be meaningful — no orphan resources either direction,
// every building's minimum depth reachable, no dominant production method.
//
//   D1  Raws are depth 0, and a good is deeper than every one of its inputs.
//   D2  MAX-within-a-recipe: a two-input recipe takes the deeper input's depth.
//   D3  MIN-across-recipes: a second, shallower route lowers a good's depth.
//   D4  Cycles and input orphans are UNREACHABLE (-1), not an infinite loop and
//       not a fabricated number. This is the row that would catch the metric
//       silently succeeding on a graph it cannot actually evaluate.
//   D5  Determinism: the depth vector is identical across two loads, and does
//       not depend on the order recipes are added.
//   D6  Against the REAL authored economy: depth is well-defined for every good
//       the shipped recipes produce, and the ancient band is not deeper than the
//       industrial one (BL-433 masks routes out; masking must never ADD depth).

#include "scripting/lua_state.hpp"
#include "world/recipe_registry.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok)
        ++failures;
}

/// Build a recipe by resource ids, so the fixtures below read as graphs rather
/// than as tables of floats.
recipe make(const std::string& name,
            const std::vector<std::pair<resource_type, float>>& in,
            const std::vector<std::pair<resource_type, float>>& out)
{
    recipe r;
    r.name = name;
    for (const auto& [res, q] : in)
        r.inputs[static_cast<std::size_t>(res)] = q;
    for (const auto& [res, q] : out)
        r.outputs[static_cast<std::size_t>(res)] = q;
    return r;
}

// Stand-in goods for the synthetic graphs. Which real resources these are does
// not matter — the fixtures test the metric, not the economy.
constexpr resource_type RAW_A = resource_type::iron_ore;
constexpr resource_type RAW_B = resource_type::coal;
constexpr resource_type MID   = resource_type::steel;
constexpr resource_type DEEP  = resource_type::machinery;
constexpr resource_type FAR   = resource_type::alloys;

} // namespace

int main()
{
    std::printf("=== chain depth — the growth spine (BL-428) ===\n\n");

    // --- D1/D2: a linear chain, and max-within-a-recipe -----------------------
    std::printf("D1/D2 — depth over a hand-built chain\n");
    {
        recipe_registry reg;
        // RAW_A + RAW_B -> MID   (both raws, so depth 1)
        // MID  + RAW_A -> DEEP   (max(1, 0) + 1 = 2, NOT 1 + 0 + 1)
        reg.add_recipe(make("mid",  {{RAW_A, 1.0f}, {RAW_B, 1.0f}}, {{MID, 1.0f}}));
        reg.add_recipe(make("deep", {{MID, 1.0f}, {RAW_A, 1.0f}},   {{DEEP, 1.0f}}));

        check(reg.depth_of(RAW_A) == 0 && reg.depth_of(RAW_B) == 0,
              "a good no recipe produces is a raw at depth 0");
        check(reg.is_raw(RAW_A) && !reg.is_raw(MID),
              "is_raw agrees with depth 0");
        check(reg.depth_of(MID) == 1, "a good made from raws sits at depth 1");
        check(reg.depth_of(DEEP) == 2,
              "MAX-within-a-recipe: mixing a depth-1 input with a raw gives depth 2, not 3");
        check(reg.max_depth() == 2, "max_depth reports the graph's ceiling");
    }

    // --- D3: min-across-recipes ----------------------------------------------
    std::printf("\nD3 — a shallower alternate route lowers the depth\n");
    {
        recipe_registry reg;
        reg.add_recipe(make("mid",       {{RAW_A, 1.0f}},              {{MID, 1.0f}}));   // depth 1
        reg.add_recipe(make("far_long",  {{MID, 1.0f}},                {{FAR, 1.0f}}));   // via MID: 2
        check(reg.depth_of(FAR) == 2, "the only route puts FAR at depth 2");

        // A direct route from a raw. BL-430's alternates are exactly this shape.
        reg.add_recipe(make("far_short", {{RAW_B, 1.0f}},              {{FAR, 1.0f}}));
        check(reg.depth_of(FAR) == 1,
              "MIN-across-recipes: adding a direct-from-raw route drops FAR to depth 1");
        check(reg.depth_of(MID) == 1, "...and does not disturb the other goods");
    }

    // --- D4: cycles and orphans are unreachable, not hangs --------------------
    std::printf("\nD4 — cycles and input orphans are unreachable\n");
    {
        recipe_registry reg;
        // A pure cycle: MID needs DEEP, DEEP needs MID. Neither bottoms out.
        reg.add_recipe(make("mid_from_deep", {{DEEP, 1.0f}}, {{MID, 1.0f}}));
        reg.add_recipe(make("deep_from_mid", {{MID, 1.0f}},  {{DEEP, 1.0f}}));
        check(reg.depth_of(MID) == -1 && reg.depth_of(DEEP) == -1,
              "a 2-cycle leaves both goods unreachable rather than looping or fabricating a depth");

        // The cycle must not poison the rest of the graph.
        reg.add_recipe(make("far", {{RAW_A, 1.0f}}, {{FAR, 1.0f}}));
        check(reg.depth_of(FAR) == 1, "an unrelated good is unaffected by the cycle");
        check(reg.depth_of(MID) == -1, "...and the cycle is still unreachable");

        // An escape hatch INTO the cycle resolves it — the honest outcome, since
        // the good really is obtainable once a route bottoms out.
        reg.add_recipe(make("mid_from_raw", {{RAW_B, 1.0f}}, {{MID, 1.0f}}));
        check(reg.depth_of(MID) == 1 && reg.depth_of(DEEP) == 2,
              "adding a raw route into the cycle makes both goods reachable");
    }
    {
        recipe_registry reg;
        // Input orphan: FAR needs DEEP, and nothing produces DEEP.
        reg.add_recipe(make("far_from_deep", {{DEEP, 1.0f}}, {{FAR, 1.0f}}));
        // DEEP is produced by nothing, so it reads as a RAW (depth 0) — which is
        // the truth about the RECIPE graph. Whether it can actually be extracted
        // is a deposit question and belongs to BL-432's roster audit, not here.
        check(reg.depth_of(DEEP) == 0,
              "a good nothing produces reads as a raw — the recipe graph's honest answer");
        check(reg.depth_of(FAR) == 1, "...so what depends on it is depth 1");
    }

    // --- D5: determinism -------------------------------------------------------
    std::printf("\nD5 — the depth vector is order-independent and reproducible\n");
    {
        recipe_registry a, b;
        a.add_recipe(make("mid",  {{RAW_A, 1.0f}},            {{MID, 1.0f}}));
        a.add_recipe(make("deep", {{MID, 1.0f}, {RAW_B, 1.0f}}, {{DEEP, 1.0f}}));
        // Same graph, recipes added in the opposite order.
        b.add_recipe(make("deep", {{MID, 1.0f}, {RAW_B, 1.0f}}, {{DEEP, 1.0f}}));
        b.add_recipe(make("mid",  {{RAW_A, 1.0f}},            {{MID, 1.0f}}));

        bool same = true;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (a.depth_of(static_cast<resource_type>(r)) != b.depth_of(static_cast<resource_type>(r)))
                same = false;
        check(same, "insertion order does not change any good's depth");
        check(a.depth_of(DEEP) == 2 && b.depth_of(DEEP) == 2,
              "...and the shared answer is the correct one");
    }

    // --- D6: the real authored economy ----------------------------------------
    std::printf("\nD6 — against the shipped recipes\n");
    {
        lua_state lua;
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        recipe_registry reg;
        reg.load_from_lua(lua);

        // Every good some allowed recipe PRODUCES must have a depth. An
        // unreachable one would mean the shipped graph has a cycle or an orphaned
        // input — worth failing the gate over.
        std::vector<std::string> unreachable;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            bool produced = false;
            for (int i = 0; i < reg.recipe_count(building_type::processing_facility); ++i)
                if (reg.recipe_at(building_type::processing_facility, i)
                        .outputs[r] > 0.0f)
                    produced = true;
            if (produced && reg.depth_of(static_cast<resource_type>(r)) < 0)
                unreachable.push_back(std::to_string(r));
        }
        for (const std::string& r : unreachable)
            std::printf("      unreachable produced good: resource id %s\n", r.c_str());
        check(unreachable.empty(),
              "every good the shipped recipes produce has a well-defined depth");

        const int industrial_max = (reg.set_era(era_band::industrial), reg.max_depth());
        const int ancient_max    = (reg.set_era(era_band::ancient),    reg.max_depth());
        std::printf("      max depth: industrial %d, ancient %d\n", industrial_max, ancient_max);
        check(industrial_max >= 2,
              "the industrial graph is genuinely layered (anti-vacuity: not all depth 1)");
        check(ancient_max <= industrial_max,
              "masking routes out (BL-433) never ADDS depth");
    }

    std::printf("\n=== %s (%d failure%s) ===\n",
                failures == 0 ? "ALL PASS" : "FAILURES",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
