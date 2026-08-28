// Throughput-field census — what the Throughput lens's ramp is actually asked to
// spread (Ben, 2026-08-28: "throughput needs to be severely scaled. Perhaps we can
// use a quadratic rather than linear scaling").
//
// The lens fills every tile by its REACH COST normalised against the body's worst
// case, so how the map reads is decided entirely by the SHAPE OF THAT
// DISTRIBUTION — and that shape has moved twice without the ramp moving with it.
// The square-root compression in body_surface_canvas.cpp was calibrated in
// Sprint 18 against 57 anchors and a stated median of 20.8 / max 101.8; a live
// look on 2026-08-28 showed 1917 anchors and a near-uniform wash, which is what a
// ramp reads like when almost every tile sits in the same slice of its input.
//
// Report-first by design. It measures and prints; it asserts only the two things
// that would make the measurement meaningless (a warmed field, and a max that is
// not degenerate). Choosing the curve is Ben's call, and this exists so the call
// is made against numbers rather than against a screenshot.
//
// PRINTS, for the home body:
//   A1 anchors            — how many LP generators the body carries.
//   A2 cost distribution  — count, min, median, mean, max, and deciles of the
//                           finite reach costs, plus the unreachable count.
//   A3 ramp occupancy     — for each candidate curve, how many tiles land in each
//                           tenth of the OUTPUT range. A curve that is doing its
//                           job spreads tiles across the ten buckets; the failure
//                           this census exists to catch is most of the grid piling
//                           into one or two of them, which is a flat map.
//
// Candidate curves, all over d = cost / max (0 at an anchor, 1 at the furthest):
//   linear      t = 1 - d
//   sqrt        t = 1 - sqrt(d)        (what ships today)
//   quadratic   t = (1 - d)^2
//   quartic     t = (1 - d)^4
//   d-squared   t = 1 - d^2

#include "world/hard_coded_world.hpp"
#include "world/logistics.hpp"
#include "world/recipe_registry.hpp"
#include "world/works_roster.hpp"
#include "world/corporation_generation.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"
#include "scripting/lua_state.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{

int    g_pass = 0;
int    g_fail = 0;

void check(bool ok, const std::string& what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    ok ? ++g_pass : ++g_fail;
}

/// The five candidates, named so the report reads as a comparison rather than a
/// column of numbers. `d` is the normalised cost, 0 at an anchor.
float curve_value(int which, float d)
{
    d = std::clamp(d, 0.0f, 1.0f);
    switch (which)
    {
    case 0: return 1.0f - d;                       // linear
    case 1: return 1.0f - std::sqrt(d);            // sqrt (ships today)
    case 2: return (1.0f - d) * (1.0f - d);        // quadratic
    case 3: { const float q = (1.0f - d) * (1.0f - d); return q * q; }  // quartic
    case 4: return 1.0f - d * d;                   // d-squared
    case 5: return 1.0f - std::cbrt(d);            // cube root — stronger than sqrt
    default: return 0.0f;
    }
}

const char* curve_name(int which)
{
    static const char* names[6] = { "linear  1-d", "sqrt    1-sqrt(d)  [ships]",
                                    "quad    (1-d)^2", "quart   (1-d)^4",
                                    "d^2     1-d^2", "cbrt    1-cbrt(d)" };
    return names[which];
}

} // namespace

int main(int argc, char** argv)
{
    uint32_t seed = 0;
    if (argc > 1)
        seed = static_cast<uint32_t>(std::atoi(argv[1]));

    // The REAL authored inputs, loaded exactly as substrate_census does. The
    // anchor set this field is seeded from is generated content — ports, hubs and
    // city centres — so a registry-less world would census a map the game never
    // builds, which is the whole failure mode this style of harness exists to
    // avoid. Run from the repo root; the Lua loads are relative.
    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");
    lua.load("scripts/works.lua");

    recipe_registry reg;
    reg.load_from_lua(lua);
    world_gen_config gen_cfg;
    gen_cfg.load_from_lua(lua);
    works_registry works;
    works.load_from_lua(lua);

    if (reg.recipe_count(building_type::processing_facility) == 0)
    {
        std::printf("FATAL: no recipes loaded - run from the repo root.\n");
        return 2;
    }

    world_params wp;
    wp.seed = seed;
    world w = make_hard_coded_world(wp, nullptr, gen_cfg, nullptr, &works);
    assign_default_recipes(w, reg);
    generate_background_firms(w, reg, wp.seed ^ 0x8A21F00Du);
    assign_default_recipes(w, reg);

    const entity_id body = w.home_body;

    // Count the anchors the field is seeded from, so A1 is a measured row rather
    // than a pointer at the lens.
    int anchors = 0;
    for (const auto& [id, tile] : w.tiles)
        if (tile.body == body && is_supply_anchor(w, id))
            ++anchors;

    // The mutable half builds the field; the const half below is what the canvas
    // reads. Calling it here is what makes tile_reach_cost return anything but -1.
    body_reach_field(w, body);

    std::vector<float> costs;
    int unreachable = 0;
    for (const auto& [id, tile] : w.tiles)
    {
        if (tile.body != body)
            continue;
        const float rc = tile_reach_cost(w, id);
        if (rc < 0.0f)
            continue;               // not computed — excluded, not counted as far
        if (std::isinf(rc))
            ++unreachable;
        else
            costs.push_back(rc);
    }

    std::sort(costs.begin(), costs.end());
    check(!costs.empty(), "the reach field is warmed and has finite costs to census");
    if (costs.empty())
        return 1;

    const float cmin = costs.front();
    const float cmax = costs.back();
    const float cmed = costs[costs.size() / 2];
    double sum = 0.0;
    for (float c : costs) sum += c;
    const float cmean = static_cast<float>(sum / static_cast<double>(costs.size()));

    check(cmax > cmin, "the cost field is not degenerate (a flat field cannot be ramped)");

    std::printf("\nA1  anchors on the home body: %d\n", anchors);
    std::printf("A2  cost distribution over %zu finite tiles  (+%d unreachable)\n",
                costs.size(), unreachable);
    std::printf("      min %.2f   median %.2f   mean %.2f   max %.2f\n",
                cmin, cmed, cmean, cmax);
    std::printf("      deciles:");
    for (int i = 1; i <= 9; ++i)
        std::printf(" %.1f", costs[(costs.size() * static_cast<std::size_t>(i)) / 10]);
    std::printf("\n");

    // The median as a SHARE of the max is the single number that predicts a flat
    // map: the closer it is to 0, the more of the grid the ramp squeezes into its
    // hot end, whatever curve is applied.
    std::printf("      median / max = %.3f   (near 0 = most of the grid sits near an anchor)\n",
                cmed / cmax);

    std::printf("\nA3  ramp occupancy — tiles per tenth of the OUTPUT range\n");
    std::printf("    %-28s %s\n", "curve", "[0.0-0.1) ... [0.9-1.0]   worst bucket");
    for (int c = 0; c < 6; ++c)
    {
        int bucket[10] = {0};
        for (float cost : costs)
        {
            const float t = curve_value(c, cost / cmax);
            int b = static_cast<int>(t * 10.0f);
            b = std::clamp(b, 0, 9);
            ++bucket[b];
        }
        int worst = 0;
        for (int b = 0; b < 10; ++b) worst = std::max(worst, bucket[b]);
        std::printf("    %-28s", curve_name(c));
        for (int b = 0; b < 10; ++b)
            std::printf(" %5d", bucket[b]);
        std::printf("   %5.1f%%\n",
                    100.0 * static_cast<double>(worst) / static_cast<double>(costs.size()));
    }
    std::printf("\n    'worst bucket' is the share of the grid in the single most crowded\n"
                "    tenth. Lower is a map with more visible structure; a curve at 80%%+ is\n"
                "    the flat wash this census was written to catch.\n");

    // TWO CANDIDATES THAT ARE NOT CURVES OVER d. Every row above rescales the
    // same ratio cost/max, and with a median at 8%% of the max there is a floor on
    // how well any of them can do — the input is bunched, so the output is bunched.
    // These change what the cost is measured AGAINST instead.
    {
        const float p90 = costs[(costs.size() * 9) / 10];

        int bucket[10] = {0};
        for (float cost : costs)
        {
            const float t = 1.0f - std::sqrt(std::clamp(cost / p90, 0.0f, 1.0f));
            ++bucket[std::clamp(static_cast<int>(t * 10.0f), 0, 9)];
        }
        int worst = 0;
        for (int b = 0; b < 10; ++b) worst = std::max(worst, bucket[b]);
        std::printf("\n    %-28s", "p90     1-sqrt(c/p90)");
        for (int b = 0; b < 10; ++b) std::printf(" %5d", bucket[b]);
        std::printf("   %5.1f%%\n",
                    100.0 * static_cast<double>(worst) / static_cast<double>(costs.size()));

        // Histogram equalisation: colour by the tile's RANK in the distribution
        // rather than by its value. Spreads perfectly by construction (10%% a
        // bucket), which is both its whole appeal and the thing to be suspicious
        // of — it makes every map look equally varied whether or not it is.
        int eq[10] = {0};
        for (std::size_t i = 0; i < costs.size(); ++i)
        {
            const float t = 1.0f - static_cast<float>(i) / static_cast<float>(costs.size());
            ++eq[std::clamp(static_cast<int>(t * 10.0f), 0, 9)];
        }
        int eworst = 0;
        for (int b = 0; b < 10; ++b) eworst = std::max(eworst, eq[b]);
        std::printf("    %-28s", "rank    equalised");
        for (int b = 0; b < 10; ++b) std::printf(" %5d", eq[b]);
        std::printf("   %5.1f%%\n",
                    100.0 * static_cast<double>(eworst) / static_cast<double>(costs.size()));

        std::printf("\n    p90 = %.2f against max %.2f: the top decile of cost is %.0f%% of the\n"
                    "    range and holds a tenth of the tiles, so normalising against the MAX\n"
                    "    spends most of the ramp on ground almost nothing sits on.\n",
                    p90, cmax, 100.0 * (1.0 - p90 / cmax));
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
