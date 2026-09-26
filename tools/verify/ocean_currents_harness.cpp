// ---------------------------------------------------------------------------
// ocean_currents_harness — BL-1120 (ocean currents: a field generated from the
// planet, and every sea leg priced with it).
//
// docs/generation/EXPLORATION.md § Currents are a force, not a picture;
// src/world/ocean_currents.hpp carries the model.
//
// DEFAULT MODE — the assertions:
//   C1  the field is deterministic: the same ground builds the same field
//   C2  open water carries the band's wind and nothing else: westward in the
//       tropics, eastward in the mid-latitudes, westward under the polar
//       easterlies, no meridional current where there is no coast to turn it
//   C3  A BASIN CIRCULATES: between two continents the water runs poleward up
//       the western shore, east across the mid-latitudes, equatorward down the
//       eastern shore and west along the trades -- clockwise in the north,
//       counter-clockwise in the south, both reversed on a retrograde spin
//   C4  a leg is priced by ONE weight: exactly antisymmetric alignment (the
//       return leg reads the outbound's negation), a leg along a current
//       cheaper than still water and the same leg against it dearer, weight 0
//       prices nothing, the domain is [0, 999]
//   C5  BL-1140: a trade's SEA LINE is priced with its current -- goods
//       riding the trades arrive in greater volume than goods beating
//       against them, one weight, exactly antisymmetric; no field is still
//       water
//   C6  BL-1140: landmasses are what the sea separates (a lake does not
//       split one, a corner joins one) and a shore seat reads the landmass it
//       borders
//   W8  BL-1140: on the real body, trade between realms on different
//       landmasses writes sea-leg uses, only such trade does, and each use is
//       read against the current
//   W1-W7 the same properties on a real body (seed 32 by default, --seed N),
//       plus the sim: a fixture re-run at generation's own weight reproduces
//       generation's Exploration span, weight 0 builds no field, a weighted
//       span is deterministic, a rejected weight prices nothing and says so,
//       the writer record sums to the leg table.
//
// --sweep — the measurement (reports, never gates):
//   Per seed of the curated library (or --seeds a,b,c), ONE generation to the
//   Industrialisation close with the shipped data layer, then for each weight
//   of the ladder (--weights, default 0,150,300,500,700,900) a traced re-run
//   of the Exploration span (1200 -> 1660) from the fixture, its fold, the
//   span-open survey on the generated tiles, and a traced re-run of the
//   Industrialisation span (1660 -> 1960) on that fold -- generation's own
//   chain, with the weight the only change. Per seed per weight: the sea-leg
//   table, lanes at 1660 and 1960 and which writer earned them (campaign /
//   purchase / tribute / trade), trade across water (uses, legs, volume, and
//   how much of it ran with the current), what bounds each cross-water flow
//   at a span's close (want / holding / land line / sea line -- only the sea
//   line is one a current can move), the launches with and against the
//   current, battles, and displacement (frontier / neighbour battles,
//   neighbour = a pair already in contact at the span's open). The rung at
//   generation's own weight (always added to the ladder) is checked against
//   generation's own untraced run (FIDELITY): a sweep whose control does not
//   reproduce the shipped world measured nothing. Weight 0 (always added) is
//   still water: the world before currents.
//   --out path writes the table as JSON.
//
// --picture — a headless map of the field: --picture writes the body's ground
//   and one arrow per ocean region to --png path (default ocean_currents.png).
//
// Build (needs the shipped data layer, so a live Lua state):
//   bash tools/verify/build_lua_harness.sh ocean_currents_harness
// Run from the repo root (it loads scripts/*.lua).
// ---------------------------------------------------------------------------

#include "world/ocean_currents.hpp"

#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"
#include "world/works_roster.hpp"
#include "scripting/lua_state.hpp"

// The dependency-free PNG writer the visual harness uses, compiled in
// directly: neither headless builder links src/core, and this TU needs nothing
// of it but the one function.
#include "core/png_writer.cpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

double seconds_since(std::chrono::steady_clock::time_point t0)
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

/// The curated library (node tools/session/seed_library.js --seed-list).
const uint32_t k_library[] = {46, 28, 11, 31, 40, 12, 37, 13, 41, 43, 32, 10, 25, 38, 9, 0};

// ---------------------------------------------------------------------------
// Synthetic bodies
// ---------------------------------------------------------------------------

/// A body of open ocean with land only where @p land(col, row) says so.
template <class F>
std::vector<terrain_substrate> make_body(int gw, int gh, F land)
{
    std::vector<terrain_substrate> s(static_cast<std::size_t>(gw) * gh, terrain_substrate::ocean);
    for (int r = 0; r < gh; ++r)
        for (int c = 0; c < gw; ++c)
            if (land(c, r)) s[static_cast<std::size_t>(r) * gw + c] = terrain_substrate::sedimentary;
    return s;
}

int region_east(const ocean_current_field& f, int cx, int cy)  { return f.east_q[static_cast<std::size_t>(cy * f.cells_w + cx)]; }
int region_north(const ocean_current_field& f, int cx, int cy) { return f.north_q[static_cast<std::size_t>(cy * f.cells_w + cx)]; }

/// Clockwise circulation around a block of ocean regions [cx0..cx1] x [cy0..cy1]
/// (cy grows southward): the top row's eastward current, the right column's
/// southward, the bottom row's westward, the left column's northward. Positive
/// is clockwise.
int64_t clockwise_circulation(const ocean_current_field& f, int cx0, int cx1, int cy0, int cy1)
{
    int64_t c = 0;
    for (int cx = cx0; cx <= cx1; ++cx)
    {
        c += region_east(f, cx, cy0);
        c -= region_east(f, cx, cy1);
    }
    for (int cy = cy0; cy <= cy1; ++cy)
    {
        c += region_north(f, cx0, cy);
        c -= region_north(f, cx1, cy);
    }
    return c;
}

void synthetic_rows()
{
    std::printf("\n--- C1-C4: synthetic bodies ---\n");

    // A water world: no coast anywhere, so the current is the band's wind.
    const int ww = 84, wh = 61; // 60 row steps: 10 rows per 30-degree band
    const std::vector<terrain_substrate> water = make_body(ww, wh, [](int, int) { return false; });
    const ocean_current_field fw = build_ocean_currents(water, ww, wh, 1);
    const ocean_current_field fw2 = build_ocean_currents(water, ww, wh, 1);
    check(!fw.empty() && ocean_current_digest(fw) == ocean_current_digest(fw2)
          && fw.east_q == fw2.east_q && fw.north_q == fw2.north_q,
          "C1  the field is deterministic: one ground, one field, twice");

    bool no_meridional = true;
    for (int16_t v : fw.north_q) if (v != 0) no_meridional = false;
    // Region rows at 7 tiles: cy 0 = rows 0-6 (90-72 N, polar easterly), cy 1 =
    // rows 7-13 (69-51 N, westerly), cy 2 = rows 14-20 (48-30 N, westerly),
    // cy 3 = rows 21-27 (27-9 N, easterly), cy 4 = rows 28-34 (6 N - 6 S,
    // doldrums straddling the equator), cy 5 = 9-27 S easterly, cy 6 =
    // 30-48 S westerly, cy 7 = 51-69 S westerly, cy 8 = 72-90 S polar.
    const int e_polar_n = region_east(fw, 3, 0), e_mid_n = region_east(fw, 3, 2);
    const int e_trop_n  = region_east(fw, 3, 3), e_trop_s = region_east(fw, 3, 5);
    const int e_mid_s   = region_east(fw, 3, 6), e_polar_s = region_east(fw, 3, 8);
    std::printf("      water world, zonal current by region row (per mille): polar N %d, mid N %d, "
                "trop N %d, trop S %d, mid S %d, polar S %d\n",
                e_polar_n, e_mid_n, e_trop_n, e_trop_s, e_mid_s, e_polar_s);
    check(no_meridional, "C2  open water with no coast carries no meridional current at all");
    check(e_trop_n < 0 && e_trop_s < 0 && e_mid_n > 0 && e_mid_s > 0 && e_polar_n < 0 && e_polar_s < 0,
          "C2  open water carries the band's wind: trades west, westerlies east, polar easterlies west, both hemispheres");
    bool zonal_uniform = true; // no coast, so every column of a row reads alike
    for (int cy = 0; cy < fw.cells_h; ++cy)
        for (int cx = 1; cx < fw.cells_w; ++cx)
            if (region_east(fw, cx, cy) != region_east(fw, 0, cy)) zonal_uniform = false;
    check(zonal_uniform, "C2  with no coast the current is the same all the way round a latitude");

    // Two meridional continents, aligned to the region lattice: land at
    // columns [0, 7) and [42, 49), so basin A spans regions cx 1..5 and
    // basin B cx 7..11.
    const std::vector<terrain_substrate> two = make_body(ww, wh, [](int c, int) {
        return c < 7 || (c >= 42 && c < 49);
    });
    const ocean_current_field fb  = build_ocean_currents(two, ww, wh, 1);
    const ocean_current_field fbr = build_ocean_currents(two, ww, wh, -1);

    // Northern subtropical gyre: cy 2 (48-30 N, westerlies) over cy 3 (27-9 N, trades).
    const int west_n  = region_north(fb, 1, 2), east_n = region_north(fb, 5, 2);
    const int top_e   = region_east(fb, 3, 2),  bot_e  = region_east(fb, 3, 3);
    const int64_t cw_n = clockwise_circulation(fb, 1, 5, 2, 3);
    // Southern: cy 5 (9-27 S, trades) over cy 6 (30-48 S, westerlies).
    const int west_s  = region_north(fb, 1, 6), east_s = region_north(fb, 5, 6);
    const int64_t cw_s = clockwise_circulation(fb, 1, 5, 5, 6);
    std::printf("      basin A, northern gyre: western shore north %d, eastern shore north %d, "
                "westerlies east %d, trades east %d, clockwise circulation %lld\n",
                west_n, east_n, top_e, bot_e, static_cast<long long>(cw_n));
    std::printf("      basin A, southern gyre: western shore north %d, eastern shore north %d, "
                "clockwise circulation %lld\n", west_s, east_s, static_cast<long long>(cw_s));
    check(west_n > 0 && east_n < 0 && top_e > 0 && bot_e < 0,
          "C3  a northern basin circulates: poleward up the western shore, east on the westerlies, "
          "equatorward down the eastern shore, west on the trades");
    check(cw_n > 0, "C3  ... which is clockwise in the north for a prograde spin");
    check(west_s < 0 && east_s > 0 && cw_s < 0,
          "C3  a southern basin circulates the other way: poleward (south) up the western shore, "
          "counter-clockwise");
    bool reversed = !fbr.empty() && fbr.east_q.size() == fb.east_q.size();
    for (std::size_t k = 0; reversed && k < fb.east_q.size(); ++k)
        if (fbr.east_q[k] != -fb.east_q[k] || fbr.north_q[k] != -fb.north_q[k]) reversed = false;
    check(reversed, "C3  a retrograde spin reverses every region's current exactly");
    check(clockwise_circulation(fb, 7, 11, 2, 3) > 0 && clockwise_circulation(fb, 7, 11, 5, 6) < 0,
          "C3  the second basin circulates as the first (the rule is the ground's, not the column's)");

    // Coasts are streamlines: a region with no sea reads no current, and the
    // land regions are exactly the land columns.
    bool land_still = true;
    for (int cy = 0; cy < fb.cells_h; ++cy)
        if (region_east(fb, 0, cy) != 0 || region_north(fb, 0, cy) != 0 || fb.sea_tiles[static_cast<std::size_t>(cy * fb.cells_w)] != 0)
            land_still = false;
    check(land_still, "C3  ground carries no current");

    // C4: legs. Up the western shore of basin A through the tropics and into
    // the mid-latitudes: col 8, row 26 -> row 14, northward along the
    // boundary current.
    const int a_out = ocean_current_alignment_q(fb, 8, 26, 8, 14);
    const int a_back = ocean_current_alignment_q(fb, 8, 14, 8, 26);
    const int w = 300;
    const int c_out = ocean_current_leg_cost_q(w, a_out), c_back = ocean_current_leg_cost_q(w, a_back);
    std::printf("      leg up the western shore: alignment %d, back %d; cost at w=%d: %d out, %d back\n",
                a_out, a_back, w, c_out, c_back);
    check(a_out > 0 && a_back == -a_out, "C4  the return leg reads the exact negation of the outbound");
    check(c_out < 1000 && c_back > 1000, "C4  a leg along its current costs less than still water, against it more");
    // Along the trades, east to west, versus west to east.
    const int trade_w = ocean_current_alignment_q(fb, 38, 24, 12, 24);
    check(trade_w > 0 && ocean_current_leg_cost_q(w, trade_w) < ocean_current_leg_cost_q(w, -trade_w),
          "C4  sailing west on the trades is cheaper than beating east against them");

    // Exact antisymmetry across many legs, including ones that wrap the seam.
    uint32_t lcg = 12345u;
    const auto next = [&]() { lcg = lcg * 1664525u + 1013904223u; return lcg >> 8; };
    bool anti = true;
    int nonzero = 0;
    for (int i = 0; i < 4000; ++i)
    {
        const int ac = static_cast<int>(next() % ww), ar = static_cast<int>(next() % wh);
        const int bc = static_cast<int>(next() % ww), br = static_cast<int>(next() % wh);
        const int x = ocean_current_alignment_q(fb, ac, ar, bc, br);
        const int y = ocean_current_alignment_q(fb, bc, br, ac, ar);
        if (x != -y) anti = false;
        if (x != 0) ++nonzero;
    }
    check(anti && nonzero > 1000, "C4  alignment is exactly antisymmetric on 4000 sampled legs (wrapping ones included)");

    bool still = true;
    for (int a = -1000; a <= 1000; a += 50) if (ocean_current_leg_cost_q(0, a) != 1000) still = false;
    check(still, "C4  weight 0 prices every leg at still water");
    check(ocean_current_weight_valid(0) && ocean_current_weight_valid(999)
          && !ocean_current_weight_valid(1000) && !ocean_current_weight_valid(-1)
          && ocean_current_leg_cost_q(999, 1000) >= 1,
          "C4  the weight's domain is [0, 999] and no leg is ever free");
    check(build_ocean_currents(two, ww, wh, 0).empty() && build_ocean_currents(two, ww - 1, wh, 1).empty(),
          "C4  a sense other than +/-1, or a raster of the wrong size, builds no field");

    // C5 (BL-1140): a trade's sea line is priced with its current. Two seats
    // either side of basin A on the trades' row: the western continent's east
    // coast (6, 25) and the eastern continent's west coast (42, 25). Each
    // sells the other what it holds, over a sea line of 400 (both ports 400,
    // both navies up). The trades run west, so goods sailing west ride the
    // current and goods sailing east beat against it.
    {
        std::vector<region> rg(2);
        rg[0].col = 6;  rg[0].row = 25; rg[0].anchor = 25 * ww + 6;  rg[0].nation = 0;
        rg[1].col = 42; rg[1].row = 25; rg[1].anchor = 25 * ww + 42; rg[1].nation = 1;
        rg[0].dominant = region_class::farm; rg[1].dominant = region_class::ore;
        rg[0].port_stock_q = 400; rg[1].port_stock_q = 400;
        rg[1].scarcity_raw_q[0] = 900; // the east wants the west's farm goods
        rg[0].scarcity_raw_q[1] = 900; // the west wants the east's ore
        std::vector<polity> qs(2);
        for (int k = 0; k < 2; ++k) { qs[k].id = k; qs[k].alive = true; qs[k].capital = k; qs[k].navy_stock = 500; }
        trade_context ctx = build_trade_context(rg, qs, {});
        const int east_still = trade_flow_volume_q(ctx, rg, qs, 0, 1, 0); // west sells farm, sails east
        const int west_still = trade_flow_volume_q(ctx, rg, qs, 1, 0, 1); // east sells ore, sails west
        ctx.currents = &fb;
        ctx.current_weight_q = 500;
        const int east_priced = trade_flow_volume_q(ctx, rg, qs, 0, 1, 0);
        const int west_priced = trade_flow_volume_q(ctx, rg, qs, 1, 0, 1);
        const int a_west = ocean_current_alignment_q(fb, 42, 25, 6, 25);
        std::printf("      trade across basin A at w=500: still water %d / %d; priced, sailing east %d, sailing west %d"
                    " (alignment west %d)\n", east_still, west_still, east_priced, west_priced, a_west);
        check(east_still == 400 && west_still == 400,
              "C5  in still water the sea line is the smaller built port, both ways");
        check(a_west > 0 && west_priced > 400 && east_priced < 400,
              "C5  priced with the current: goods riding the trades arrive in greater volume than goods beating against them");
        check(west_priced == std::min(1000, 400 * 1000 / ocean_current_leg_cost_q(500, a_west))
              && east_priced == 400 * 1000 / ocean_current_leg_cost_q(500, -a_west),
              "C5  the line is divided by the leg's cost, one weight, exactly antisymmetric in direction");
        ctx.currents = nullptr;
        check(trade_flow_volume_q(ctx, rg, qs, 0, 1, 0) == 400,
              "C5  a context with no field is still water");
    }

    // C6 (BL-1140): landmasses are what the SEA separates. The two-continent
    // body has two; a lake inside one continent does not split it, ground
    // touching at a corner is one landmass, and a seat on the shoreline reads
    // the landmass it borders.
    {
        std::vector<terrain_substrate> body = two;
        body[static_cast<std::size_t>(30) * ww + 3] = terrain_substrate::lake;   // a lake in the west
        body[static_cast<std::size_t>(30) * ww + 4] = terrain_substrate::lake;
        const std::vector<int32_t> m = landmass_labels(body, ww, wh);
        const int32_t west = landmass_at(m, ww, wh, 2, 30), east = landmass_at(m, ww, wh, 45, 30);
        int32_t top = -1;
        for (int32_t v : m) top = std::max(top, v);
        check(top == 1 && west >= 0 && east >= 0 && west != east && landmass_at(m, ww, wh, 5, 30) == west,
              "C6  the sea separates two landmasses; a lake inside one does not split it");
        check(landmass_at(m, ww, wh, 8, 20) == west && landmass_at(m, ww, wh, 41, 20) == east
              && landmass_at(m, ww, wh, 24, 20) == -1,
              "C6  a place on the shore reads the landmass it borders; open sea reads none");
        std::vector<terrain_substrate> corner(static_cast<std::size_t>(10) * 10, terrain_substrate::ocean);
        corner[static_cast<std::size_t>(4) * 10 + 4] = terrain_substrate::sedimentary;
        corner[static_cast<std::size_t>(5) * 10 + 5] = terrain_substrate::sedimentary;
        const std::vector<int32_t> mc = landmass_labels(corner, 10, 10);
        check(mc[44] == 0 && mc[55] == 0, "C6  ground touching at a corner is one landmass (movement is eight-connected)");
    }
}

// ---------------------------------------------------------------------------
// The shipped data layer (the app's order, as exploration_sweep loads it)
// ---------------------------------------------------------------------------

struct shipped_inputs
{
    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
};

void load_shipped_inputs(shipped_inputs& in)
{
    in.lua.load("scripts/recipes.lua");
    in.lua.load("scripts/economy.lua");
    in.lua.load("scripts/world_gen.lua");
    in.cfg.load_from_lua(in.lua);
    in.lua.load("scripts/works.lua");
    in.works.load_from_lua(in.lua);
    std::printf("generation inputs: scripts/world_gen.lua + scripts/works.lua (%zu works rows)\n",
                in.works.size());
}

/// The body's tiles in raster order, for the span-open survey.
std::vector<entity_id> body_tile_ids(const world& w, entity_id body, int gw, int gh)
{
    std::vector<entity_id> ids(static_cast<std::size_t>(gw) * gh, null_entity);
    for (const auto& kv : w.tiles)
    {
        const tile_component& t = kv.second;
        if (t.body != body) continue;
        if (t.grid_x < 0 || t.grid_y < 0 || t.grid_x >= gw || t.grid_y >= gh) continue;
        ids[static_cast<std::size_t>(t.grid_y) * gw + t.grid_x] = kv.first;
    }
    return ids;
}

/// Exploration's params as generation ran them, resumed off the fixture.
history_sim_params exploration_rerun_params(const era_minus_one_fixture& fx, int weight, bool trace)
{
    history_sim_params ep = fx.exploration_params;
    ep.trace_battles    = trace;
    ep.resume_polities  = &fx.pre_exploration_polities;
    ep.resume_grudges   = &fx.pre_exploration_grudges;
    ep.resume_contacts  = &fx.pre_exploration_contacts;
    ep.resume_corridors = &fx.pre_exploration_corridors;
    ep.resume_civilisations    = &fx.pre_exploration_civilisations;
    ep.resume_universal_creeds = &fx.pre_exploration_universal_creeds;
    if (weight >= -1) ep.sea_current_weight_q = weight; // -2 keeps the captured weight
    return ep;
}

struct span_read
{
    int64_t battles = 0, conquests = 0, foundings = 0;
    int64_t wet_battles = 0;          ///< traced battles whose staging line crossed sea
    int64_t neighbour = 0, frontier = 0, ambiguous = 0;
    int64_t table = 0;                ///< legs in the span's closing table
    int64_t lanes = 0;                ///< of which at or over the lane tier
    int64_t noted_campaign = 0, noted_purchase = 0, noted_tribute = 0, opened = 0;
    int64_t noted_trade = 0;          ///< BL-1140: the fourth writer's uses this span
    int64_t trade_legs = 0;           ///< legs this span's trade wrote at least one use to
    int64_t tr_with = 0, tr_against = 0, tr_slack = 0, tr_align_sum = 0; ///< trade uses vs the current
    int64_t tr_volume = 0, tr_volume_with = 0, tr_volume_against = 0;     ///< volume across water, by direction
    int64_t with = 0, against = 0, slack = 0, align_sum = 0;
    /// The same with/against reading taken off the battle traces against the
    /// body's field, so it exists at weight 0 too (where the sim builds none):
    /// every traced wet battle, staging hub to target.
    int64_t t_with = 0, t_against = 0, t_slack = 0, t_align_sum = 0;
    int64_t subjections = 0, bought = 0, freed = 0;
    uint64_t field = 0;
};

bool contact_known(const std::set<std::pair<int, int>>& known, int a, int b)
{
    return known.count({std::min(a, b), std::max(a, b)}) != 0;
}

span_read read_span(const history_sim_state& hs, const std::set<std::pair<int, int>>& known_at_open,
                    int lane_tier, const ocean_current_field* field = nullptr,
                    const std::vector<region>* regions = nullptr)
{
    span_read s;
    s.battles = hs.battles; s.conquests = hs.conquests; s.foundings = hs.foundings;
    for (const battle_trace& bt : hs.battle_traces)
    {
        if (!bt.exec_dry) ++s.wet_battles;
        if (!bt.exec_dry && field != nullptr && regions != nullptr && bt.exec_hub >= 0
         && static_cast<std::size_t>(bt.exec_hub) < regions->size() && bt.region < regions->size())
        {
            const region& h = (*regions)[static_cast<std::size_t>(bt.exec_hub)];
            const region& t = (*regions)[bt.region];
            const int a = ocean_current_alignment_q(*field, h.col, h.row, t.col, t.row);
            if (a > 0) ++s.t_with; else if (a < 0) ++s.t_against; else ++s.t_slack;
            s.t_align_sum += a;
        }
        if (bt.attacker == 0 && bt.defender == 0) { ++s.ambiguous; continue; }
        if (contact_known(known_at_open, bt.attacker, bt.defender)) ++s.neighbour;
        else                                                        ++s.frontier;
    }
    s.table = static_cast<int64_t>(hs.sea_legs.size());
    for (const sea_leg& l : hs.sea_legs) if (l.uses >= lane_tier) ++s.lanes;
    s.noted_campaign = hs.sea_legs_noted_campaign;
    s.noted_purchase = hs.sea_legs_noted_purchase;
    s.noted_tribute  = hs.sea_legs_noted_tribute;
    s.noted_trade    = hs.sea_legs_noted_trade;
    for (const sea_leg_writer_row& r : hs.sea_leg_writers) if (r.trade > 0) ++s.trade_legs;
    s.tr_with = hs.sea_trade_with_current; s.tr_against = hs.sea_trade_against_current;
    s.tr_slack = hs.sea_trade_slack_current; s.tr_align_sum = hs.sea_trade_alignment_sum_q;
    s.tr_volume = hs.sea_trade_volume_q; s.tr_volume_with = hs.sea_trade_volume_with_q;
    s.tr_volume_against = hs.sea_trade_volume_against_q;
    s.opened         = hs.sea_lanes_opened;
    s.with = hs.sea_campaigns_with_current; s.against = hs.sea_campaigns_against_current;
    s.slack = hs.sea_campaigns_slack_current; s.align_sum = hs.sea_campaign_alignment_sum_q;
    s.subjections = hs.subjections_formed; s.bought = hs.provinces_bought; s.freed = hs.subjections_freed;
    s.field = hs.sea_current_field_digest;
    return s;
}

std::set<std::pair<int, int>> contact_set(const std::vector<contact>& cs)
{
    std::set<std::pair<int, int>> s;
    for (const contact& c : cs) s.insert({std::min<int>(c.from, c.to), std::max<int>(c.from, c.to)});
    return s;
}

/// Lanes at the arc's close, attributed to the writer that noted most of their
/// uses over both spans (ties go campaign < purchase < tribute < trade, the enum order).
struct lane_writers
{
    int64_t lanes = 0;
    int64_t by_writer[4] = {0, 0, 0, 0};    ///< lanes whose uses are mostly this writer's (c/p/t/trade)
    int64_t uses_by_writer[4] = {0, 0, 0, 0}; ///< every use on a lane, by writer
};

lane_writers attribute_lanes(const std::vector<sea_leg>& table,
                             const std::vector<sea_leg_writer_row>& w1,
                             const std::vector<sea_leg_writer_row>& w2, int lane_tier)
{
    std::map<std::pair<int, int>, std::array<int64_t, 4>> by_leg;
    for (const auto* rows : {&w1, &w2})
        for (const sea_leg_writer_row& r : *rows)
        {
            std::array<int64_t, 4>& a = by_leg[{r.a, r.b}];
            a[0] += r.campaign; a[1] += r.purchase; a[2] += r.tribute; a[3] += r.trade;
        }
    lane_writers out;
    for (const sea_leg& l : table)
    {
        if (l.uses < lane_tier) continue;
        ++out.lanes;
        const auto it = by_leg.find({l.a, l.b});
        if (it == by_leg.end()) continue;
        const std::array<int64_t, 4>& a = it->second;
        int best = 0;
        for (int k = 1; k < 4; ++k) if (a[k] > a[best]) best = k;
        ++out.by_writer[best];
        for (int k = 0; k < 4; ++k) out.uses_by_writer[k] += a[k];
    }
    return out;
}

// ---------------------------------------------------------------------------
// Real-body rows (W1-W7)
// ---------------------------------------------------------------------------

void real_body_rows(shipped_inputs& shipped, uint32_t seed, int weight)
{
    std::printf("\n--- W1-W7: the field and the sim on seed %u (weight %d) ---\n", seed, weight);
    const auto t0 = std::chrono::steady_clock::now();
    world_params wp;
    wp.seed = seed;
    generation_report rep;
    era_minus_one_fixture fx;
    world_gen_config cfg = shipped.cfg;
    cfg.stop_after_exploration = true;
    const world w = make_hard_coded_world(wp, &rep, cfg, nullptr, &shipped.works, &fx);
    (void)w;
    std::printf("      generated to the Exploration close in %.1f s\n", seconds_since(t0));
    if (!fx.ran || !fx.exploration_ran || fx.terrain.substrate.empty())
    {
        check(false, "W0  the seed ran its Exploration span (nothing to measure otherwise)");
        return;
    }

    const std::vector<terrain_substrate>& sub = fx.terrain.substrate;
    const ocean_current_field f  = build_ocean_currents(sub, fx.gw, fx.gh, 1);
    const ocean_current_field f2 = build_ocean_currents(sub, fx.gw, fx.gh, 1);
    int with_current = 0, sea_regions = 0;
    for (std::size_t k = 0; k < f.east_q.size(); ++k)
    {
        if (f.sea_tiles[k] == 0) continue;
        ++sea_regions;
        if (f.east_q[k] != 0 || f.north_q[k] != 0) ++with_current;
    }
    std::printf("      field %dx%d regions of %d tiles, %d hold sea, %d carry a current; digest %016llx\n",
                f.cells_w, f.cells_h, f.cell, sea_regions, with_current,
                static_cast<unsigned long long>(ocean_current_digest(f)));
    check(!f.empty() && ocean_current_digest(f) == ocean_current_digest(f2) && f.east_q == f2.east_q
          && f.north_q == f2.north_q && with_current > sea_regions / 2,
          "W1  the real body's field is deterministic and most of its sea carries a current");

    // W2: the prevailing winds, read off open water (regions wholly sea).
    int64_t band_e[4] = {0, 0, 0, 0}; int band_n[4] = {0, 0, 0, 0};
    for (int cy = 0; cy < f.cells_h; ++cy)
        for (int cx = 0; cx < f.cells_w; ++cx)
        {
            const std::size_t k = static_cast<std::size_t>(cy * f.cells_w + cx);
            if (f.sea_tiles[k] < f.cell * f.cell) continue;
            const int row = cy * f.cell + f.cell / 2;
            const int lat = ((fx.gh - 1) - 2 * row) * 90 / (fx.gh - 1);
            const int a = lat < 0 ? -lat : lat;
            int band = -1;
            if (a >= 8 && a <= 25)       band = lat > 0 ? 0 : 2; // trades
            else if (a >= 36 && a <= 54) band = lat > 0 ? 1 : 3; // westerlies
            if (band < 0) continue;
            band_e[band] += f.east_q[k]; ++band_n[band];
        }
    const auto mean = [&](int b) { return band_n[b] ? static_cast<double>(band_e[b]) / band_n[b] : 0.0; };
    std::printf("      open-water mean zonal current: trades N %.0f (%d regions), westerlies N %.0f (%d), "
                "trades S %.0f (%d), westerlies S %.0f (%d)\n",
                mean(0), band_n[0], mean(1), band_n[1], mean(2), band_n[2], mean(3), band_n[3]);
    check(band_n[0] > 0 && band_n[1] > 0 && band_n[2] > 0 && band_n[3] > 0
          && mean(0) < 0 && mean(2) < 0 && mean(1) > 0 && mean(3) > 0,
          "W2  the real body's open water runs west on the trades and east on the westerlies, both hemispheres");

    // W3: legs between sea tiles within a campaign's reach and beyond it.
    std::vector<int> sea_tiles;
    for (int i = 0; i < fx.gw * fx.gh; ++i) if (f.sea[static_cast<std::size_t>(i)]) sea_tiles.push_back(i);
    uint32_t lcg = seed * 2654435761u + 7u;
    const auto next = [&]() { lcg = lcg * 1664525u + 1013904223u; return lcg >> 8; };
    bool anti = true, ordered = true;
    int legs = 0, strong = 0, differs = 0;
    const int wq = weight > 0 ? weight : 300;
    for (int i = 0; i < 4000 && !sea_tiles.empty(); ++i)
    {
        const int ta = sea_tiles[next() % sea_tiles.size()];
        const int tb = sea_tiles[next() % sea_tiles.size()];
        const int ac = ta % fx.gw, ar = ta / fx.gw, bc = tb % fx.gw, br = tb / fx.gw;
        const int x = ocean_current_alignment_q(f, ac, ar, bc, br);
        const int y = ocean_current_alignment_q(f, bc, br, ac, ar);
        ++legs;
        if (x != -y) anti = false;
        if (x != 0)
        {
            // Strictly ordered wherever the weight resolves the alignment to at
            // least one per mille of cost; a whisper of current below that is
            // still water at this weight, never the wrong way round.
            const int cw = ocean_current_leg_cost_q(wq, x > 0 ? x : y);
            const int ca = ocean_current_leg_cost_q(wq, x > 0 ? y : x);
            const bool resolved = (static_cast<int64_t>(x < 0 ? -x : x) * wq) >= 1000;
            if (resolved ? !(cw < 1000 && ca > 1000) : !(cw <= 1000 && ca >= 1000)) ordered = false;
            if (ocean_current_leg_cost_q(wq, x) != ocean_current_leg_cost_q(wq, y)) ++differs;
        }
        if (x >= 100 || x <= -100) ++strong;
    }
    std::printf("      %d sampled sea legs: %d with |alignment| >= 100, %d whose return costs differently at w=%d\n",
                legs, strong, differs, wq);
    check(anti, "W3  every sampled leg's return reads the exact negation of its outbound");
    check(ordered && differs > legs / 4,
          "W3  a basin circulates: where a current runs, the leg with it costs less than still water and its return more");

    // W4-W7: the sim.
    const int lane_tier = fx.exploration_params.sea_lane_tier1_uses;
    const std::set<std::pair<int, int>> known = contact_set(fx.pre_exploration_contacts);
    const auto rerun = [&](int wgt, int sense, history_sim_state& hs) {
        history_sim_params ep = exploration_rerun_params(fx, wgt, /*trace=*/false);
        ep.sea_current_rotation_sense = sense;
        settlement_state ss = fx.pre_exploration_settlement;
        creed_state      cs = fx.pre_exploration_creeds;
        hs = run_history_sim(ss, &cs, fx.terrain.view(), fx.gw, fx.gh, ep, fx.exploration_seed,
                             nullptr, fx.works, nullptr);
    };
    const auto same_legs = [](const std::vector<sea_leg>& a, const std::vector<sea_leg>& b) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (a[i].a != b[i].a || a[i].b != b[i].b || a[i].uses != b[i].uses) return false;
        return true;
    };
    const auto same_run = [&](const history_sim_state& a, const history_sim_state& b) {
        return a.battles == b.battles && a.conquests == b.conquests && a.foundings == b.foundings
            && a.subjections_formed == b.subjections_formed && a.provinces_bought == b.provinces_bought
            && a.sea_legs_noted_campaign == b.sea_legs_noted_campaign
            && a.sea_legs_noted_purchase == b.sea_legs_noted_purchase
            && a.sea_legs_noted_tribute == b.sea_legs_noted_tribute
            && a.sea_lanes_opened == b.sea_lanes_opened && same_legs(a.sea_legs, b.sea_legs)
            && a.history.size() == b.history.size();
    };

    history_sim_state still, as_generated, weighted_a, weighted_b, rejected, retro;
    const auto t1 = std::chrono::steady_clock::now();
    rerun(0, 1, still);
    std::printf("      Exploration re-run at weight 0: %.1f s\n", seconds_since(t1));
    rerun(-2, fx.exploration_params.sea_current_rotation_sense, as_generated); // the captured weight
    std::printf("      generation runs Exploration at weight %d\n", fx.exploration_params.sea_current_weight_q);
    check(same_run(as_generated, fx.exploration_state)
          && as_generated.sea_current_field_digest == fx.exploration_state.sea_current_field_digest,
          "W4  a re-run from the fixture at generation's own weight reproduces generation's Exploration span");
    check(still.sea_current_field_digest == 0 && still.sea_campaigns_with_current == 0
          && !still.sea_current_params_rejected,
          "W4  at weight 0 no field is built and no leg is counted against one");

    rerun(wq, 1, weighted_a);
    rerun(wq, 1, weighted_b);
    std::printf("      weight %d: battles %lld (still %lld), legs %zu (still %zu), launches with/against/slack %lld/%lld/%lld\n",
                wq, static_cast<long long>(weighted_a.battles), static_cast<long long>(still.battles),
                weighted_a.sea_legs.size(), still.sea_legs.size(),
                static_cast<long long>(weighted_a.sea_campaigns_with_current),
                static_cast<long long>(weighted_a.sea_campaigns_against_current),
                static_cast<long long>(weighted_a.sea_campaigns_slack_current));
    check(same_run(weighted_a, weighted_b)
          && weighted_a.sea_campaigns_with_current == weighted_b.sea_campaigns_with_current
          && weighted_a.sea_campaign_alignment_sum_q == weighted_b.sea_campaign_alignment_sum_q,
          "W5  a span priced with its currents is deterministic: same seed, same fixture, twice");
    check(weighted_a.sea_current_field_digest == ocean_current_digest(f),
          "W5  the sim priced its legs with the very field the body builds");

    rerun(1000, 1, rejected);
    check(rejected.sea_current_params_rejected && rejected.sea_current_field_digest == 0
          && same_run(rejected, still),
          "W6  a weight outside [0, 999] is rejected whole: the run prices nothing and says so");
    rerun(wq, -1, retro);
    check(retro.sea_current_field_digest != 0 && retro.sea_current_field_digest != weighted_a.sea_current_field_digest,
          "W6  the rotation sense reaches the field the sim builds");

    // W7: the writer record against the table (Exploration inherits no legs).
    bool sums = true;
    int64_t tc = 0, tp = 0, tt = 0, tr = 0;
    {
        std::map<std::pair<int, int>, int64_t> uses;
        for (const sea_leg& l : weighted_a.sea_legs) uses[{l.a, l.b}] = l.uses;
        for (const sea_leg_writer_row& r : weighted_a.sea_leg_writers)
        {
            tc += r.campaign; tp += r.purchase; tt += r.tribute; tr += r.trade;
            const auto it = uses.find({r.a, r.b});
            if (it == uses.end() || it->second != r.campaign + r.purchase + r.tribute + r.trade) sums = false;
        }
        if (weighted_a.sea_leg_writers.size() != weighted_a.sea_legs.size()) sums = false;
    }
    check(sums && tc == weighted_a.sea_legs_noted_campaign && tp == weighted_a.sea_legs_noted_purchase
          && tt == weighted_a.sea_legs_noted_tribute && tr == weighted_a.sea_legs_noted_trade,
          "W7  the writer record sums to the leg table, leg by leg and writer by writer (four writers)");

    // W8 (BL-1140): the fourth writer. Every trade-written leg joins seats on
    // different landmasses; trade writes on this body; its uses were read
    // against the current, and the span with them is deterministic (W5).
    {
        std::vector<region> regions_after;
        {
            history_sim_params ep = exploration_rerun_params(fx, wq, /*trace=*/false);
            settlement_state ss = fx.pre_exploration_settlement;
            creed_state      cs = fx.pre_exploration_creeds;
            (void)run_history_sim(ss, &cs, fx.terrain.view(), fx.gw, fx.gh, ep, fx.exploration_seed,
                                  nullptr, fx.works, nullptr);
            regions_after = ss.regions;
        }
        const std::vector<int32_t> mass = landmass_labels(sub, fx.gw, fx.gh);
        const auto mass_of = [&](int ri) -> int32_t {
            if (ri < 0 || static_cast<std::size_t>(ri) >= regions_after.size()) return -1;
            const region& r = regions_after[static_cast<std::size_t>(ri)];
            return landmass_at(mass, fx.gw, fx.gh, r.col, r.row);
        };
        int64_t trade_legs = 0, same_mass = 0;
        for (const sea_leg_writer_row& r : weighted_a.sea_leg_writers)
        {
            if (r.trade <= 0) continue;
            ++trade_legs;
            if (mass_of(r.a) < 0 || mass_of(r.a) == mass_of(r.b)) ++same_mass;
        }
        const int64_t tn = weighted_a.sea_trade_with_current + weighted_a.sea_trade_against_current
                         + weighted_a.sea_trade_slack_current;
        std::printf("      trade across water: %lld uses on %lld legs (%lld with / %lld against / %lld slack, "
                    "mean alignment %.0f); legs joining one landmass: %lld\n",
                    static_cast<long long>(weighted_a.sea_legs_noted_trade), static_cast<long long>(trade_legs),
                    static_cast<long long>(weighted_a.sea_trade_with_current),
                    static_cast<long long>(weighted_a.sea_trade_against_current),
                    static_cast<long long>(weighted_a.sea_trade_slack_current),
                    tn > 0 ? static_cast<double>(weighted_a.sea_trade_alignment_sum_q) / tn : 0.0,
                    static_cast<long long>(same_mass));
        check(weighted_a.sea_legs_noted_trade > 0 && trade_legs > 0 && same_mass == 0,
              "W8  trade between realms on different landmasses writes sea-leg uses, and only such trade does");
        check(tn == weighted_a.sea_legs_noted_trade
              && weighted_a.sea_trade_with_current == weighted_b.sea_trade_with_current
              && weighted_a.sea_trade_alignment_sum_q == weighted_b.sea_trade_alignment_sum_q,
              "W8  every trade use is read against the current, the same way twice");
        check(still.sea_legs_noted_trade > 0 && still.sea_trade_with_current == 0,
              "W8  the writer runs in still water too (it is the record, not the price)");
    }
    (void)known; (void)lane_tier;
}

// ---------------------------------------------------------------------------
// --sweep
// ---------------------------------------------------------------------------

/// WHAT BOUNDS TRADE ACROSS WATER, read at a span's close off its last round
/// of flows (a snapshot, so a diagnosis rather than a total): each flow whose
/// seats stand on different landmasses, sized again from the close's own
/// regions and polities as `trade_flow_volume_q` sizes it -- min(want,
/// holding, max(land line, sea line priced with the current)) -- and filed
/// under the term that sets that minimum. Only a flow the SEA line bounds is
/// one a current can move.
struct binding_census
{
    int64_t flows = 0, by_want = 0, by_holding = 0, by_land = 0, by_sea = 0;
};

binding_census census_cross_water(const history_sim_state& hs, const std::vector<region>& regions,
                                  const ocean_current_field& field, const std::vector<int32_t>& mass,
                                  int gw, int gh, int weight)
{
    binding_census c;
    const trade_context ctx = build_trade_context(regions, hs.polities, hs.supply_corridors);
    const auto land_line = [&](int a, int b) {
        const uint16_t lo = static_cast<uint16_t>(std::min(a, b)), hi = static_cast<uint16_t>(std::max(a, b));
        for (const trade_context::land_line& l : ctx.land_lines)
            if (l.lo == lo && l.hi == hi) return static_cast<int>(l.line_q);
        return 0;
    };
    for (const trade_flow& f : hs.trade_flows)
    {
        if (f.seller >= hs.polities.size() || f.buyer >= hs.polities.size() || f.good >= 4) continue;
        const polity& ps = hs.polities[f.seller];
        const polity& pb = hs.polities[f.buyer];
        if (ps.capital < 0 || pb.capital < 0 || static_cast<std::size_t>(ps.capital) >= regions.size()
         || static_cast<std::size_t>(pb.capital) >= regions.size()) continue;
        const region& s = regions[static_cast<std::size_t>(ps.capital)];
        const region& b = regions[static_cast<std::size_t>(pb.capital)];
        const int32_t ms = landmass_at(mass, gw, gh, s.col, s.row), mb = landmass_at(mass, gw, gh, b.col, b.row);
        if (ms < 0 || mb < 0 || ms == mb) continue;
        ++c.flows;
        const int want = b.scarcity_raw_q[f.good];
        const int hold = ctx.holding_q[f.seller][f.good];
        const int land = land_line(f.seller, f.buyer);
        int sea = ps.navy_stock > 0 ? std::clamp(std::min(s.port_stock_q, b.port_stock_q), 0, 1000) : 0;
        if (sea > 0 && weight > 0 && !field.empty())
        {
            const int cost = ocean_current_leg_cost_q(weight, ocean_current_alignment_q(field, s.col, s.row, b.col, b.row));
            sea = std::clamp(static_cast<int>((static_cast<int64_t>(sea) * 1000) / std::max(1, cost)), 0, 1000);
        }
        const int line = std::max(land, sea);
        const int m = std::min({want, hold, line});
        if (m == line && line > 0) (sea >= land ? ++c.by_sea : ++c.by_land);
        else if (m == want)        ++c.by_want;
        else                       ++c.by_holding;
    }
    return c;
}

struct weight_row
{
    int weight = 0;
    binding_census bind_e, bind_i; ///< what bounds trade across water at each span's close
    span_read e, i;
    lane_writers lw;     ///< lanes at the 1960 close, attributed over both spans
    lane_writers lw1660; ///< lanes at the 1660 close, attributed over Exploration's notes
    int64_t lanes_1660 = 0;
    double secs = 0.0;
};

struct seed_row
{
    uint32_t seed = 0;
    bool ok = false;
    bool fidelity_e = false, fidelity_i = false;
    int gen_weight = 0; ///< the weight generation's own spans ran at
    span_read shipped_e, shipped_i; ///< generation's own untraced runs
    std::vector<weight_row> weights;
    double gen_secs = 0.0;
};

void print_span(const char* tag, const span_read& s)
{
    const double disp = s.neighbour > 0 ? static_cast<double>(s.frontier) / s.neighbour : -1.0;
    std::printf("    %-4s bat %6lld wet %5lld nb %5lld fr %5lld disp %6.2f | legs %4lld lanes %3lld opened %3lld"
                " | notes c/p/t/trade %5lld/%4lld/%5lld/%5lld (trade legs %3lld, w/a/s %4lld/%4lld/%4lld mean %5.0f, volume %6lld with %6lld against %6lld) | wet battles w/a/s %4lld/%4lld/%4lld mean %5.0f | subj %3lld bought %3lld freed %3lld\n",
                tag, static_cast<long long>(s.battles), static_cast<long long>(s.wet_battles),
                static_cast<long long>(s.neighbour), static_cast<long long>(s.frontier), disp,
                static_cast<long long>(s.table), static_cast<long long>(s.lanes), static_cast<long long>(s.opened),
                static_cast<long long>(s.noted_campaign), static_cast<long long>(s.noted_purchase),
                static_cast<long long>(s.noted_tribute), static_cast<long long>(s.noted_trade),
                static_cast<long long>(s.trade_legs),
                static_cast<long long>(s.tr_with), static_cast<long long>(s.tr_against), static_cast<long long>(s.tr_slack),
                (s.tr_with + s.tr_against + s.tr_slack) > 0
                    ? static_cast<double>(s.tr_align_sum) / (s.tr_with + s.tr_against + s.tr_slack) : 0.0,
                static_cast<long long>(s.tr_volume), static_cast<long long>(s.tr_volume_with),
                static_cast<long long>(s.tr_volume_against),
                static_cast<long long>(s.t_with), static_cast<long long>(s.t_against), static_cast<long long>(s.t_slack),
                (s.t_with + s.t_against + s.t_slack) > 0
                    ? static_cast<double>(s.t_align_sum) / (s.t_with + s.t_against + s.t_slack) : 0.0,
                static_cast<long long>(s.subjections), static_cast<long long>(s.bought), static_cast<long long>(s.freed));
}

void put_span(std::FILE* f, const char* key, const span_read& s)
{
    std::fprintf(f, "\"%s\": {\"battles\": %lld, \"wet_battles\": %lld, \"neighbour\": %lld, \"frontier\": %lld, "
                    "\"ambiguous\": %lld, \"legs\": %lld, \"lanes\": %lld, \"opened\": %lld, "
                    "\"noted_campaign\": %lld, \"noted_purchase\": %lld, \"noted_tribute\": %lld, "
                    "\"with\": %lld, \"against\": %lld, \"slack\": %lld, \"align_sum\": %lld, "
                    "\"t_with\": %lld, \"t_against\": %lld, \"t_slack\": %lld, \"t_align_sum\": %lld, "
                    "\"noted_trade\": %lld, \"trade_legs\": %lld, \"tr_with\": %lld, \"tr_against\": %lld, "
                    "\"tr_slack\": %lld, \"tr_align_sum\": %lld, "
                    "\"tr_volume\": %lld, \"tr_volume_with\": %lld, \"tr_volume_against\": %lld, "
                    "\"subjections\": %lld, \"bought\": %lld, \"freed\": %lld, \"conquests\": %lld, \"foundings\": %lld}",
                 key, static_cast<long long>(s.battles), static_cast<long long>(s.wet_battles),
                 static_cast<long long>(s.neighbour), static_cast<long long>(s.frontier),
                 static_cast<long long>(s.ambiguous), static_cast<long long>(s.table),
                 static_cast<long long>(s.lanes), static_cast<long long>(s.opened),
                 static_cast<long long>(s.noted_campaign), static_cast<long long>(s.noted_purchase),
                 static_cast<long long>(s.noted_tribute), static_cast<long long>(s.with),
                 static_cast<long long>(s.against), static_cast<long long>(s.slack),
                 static_cast<long long>(s.align_sum),
                 static_cast<long long>(s.t_with), static_cast<long long>(s.t_against),
                 static_cast<long long>(s.t_slack), static_cast<long long>(s.t_align_sum),
                 static_cast<long long>(s.noted_trade), static_cast<long long>(s.trade_legs),
                 static_cast<long long>(s.tr_with), static_cast<long long>(s.tr_against),
                 static_cast<long long>(s.tr_slack), static_cast<long long>(s.tr_align_sum),
                 static_cast<long long>(s.tr_volume), static_cast<long long>(s.tr_volume_with),
                 static_cast<long long>(s.tr_volume_against),
                 static_cast<long long>(s.subjections),
                 static_cast<long long>(s.bought), static_cast<long long>(s.freed),
                 static_cast<long long>(s.conquests), static_cast<long long>(s.foundings));
}

seed_row sweep_seed(shipped_inputs& shipped, uint32_t seed, const std::vector<int>& weights)
{
    seed_row row;
    row.seed = seed;
    const auto t0 = std::chrono::steady_clock::now();
    world_params wp;
    wp.seed = seed;
    generation_report rep;
    era_minus_one_fixture fx;
    world_gen_config cfg = shipped.cfg;
    cfg.stop_after_industrialisation = true;
    const world w = make_hard_coded_world(wp, &rep, cfg, nullptr, &shipped.works, &fx);
    row.gen_secs = seconds_since(t0);
    if (!fx.ran || !fx.exploration_ran || !fx.industrialisation_ran)
    {
        std::printf("  seed %u: a span did not run (empires %d, exploration %d, industrialisation %d) -- skipped\n",
                    seed, fx.ran, fx.exploration_ran, fx.industrialisation_ran);
        return row;
    }
    row.ok = true;
    row.gen_weight = fx.exploration_params.sea_current_weight_q;
    const int lane_tier = fx.exploration_params.sea_lane_tier1_uses;
    const std::set<std::pair<int, int>> known_1200 = contact_set(fx.pre_exploration_contacts);
    const std::set<std::pair<int, int>> known_1660 = contact_set(fx.exploration_handoff.contacts);
    row.shipped_e = read_span(fx.exploration_state, known_1200, lane_tier);
    row.shipped_i = read_span(fx.industrialisation_state, known_1660, lane_tier);
    const std::vector<entity_id> ids = body_tile_ids(w, fx.body, fx.gw, fx.gh);
    const ocean_current_field field = build_ocean_currents(fx.terrain.substrate, fx.gw, fx.gh, 1);
    const std::vector<int32_t> mass = landmass_labels(fx.terrain.substrate, fx.gw, fx.gh);

    // Industrialisation's params as generation derived them (captured without
    // their resume pointers); the fixture's own copy when it has one.
    const history_sim_params dp_base = fx.industrialisation_params;
    const uint32_t dseed = fx.industrialisation_seed;

    for (int wgt : weights)
    {
        const auto t1 = std::chrono::steady_clock::now();
        weight_row wr;
        wr.weight = wgt;

        history_sim_params ep = exploration_rerun_params(fx, wgt, /*trace=*/true);
        settlement_state ss = fx.pre_exploration_settlement;
        creed_state      cs = fx.pre_exploration_creeds;
        const history_sim_state he = run_history_sim(ss, &cs, fx.terrain.view(), fx.gw, fx.gh, ep,
                                                     fx.exploration_seed, nullptr, fx.works, nullptr);
        const exploration_output eo = make_exploration_output(ss, he, &cs);
        wr.e = read_span(he, known_1200, lane_tier, &field, &ss.regions);
        wr.bind_e = census_cross_water(he, ss.regions, field, mass, fx.gw, fx.gh, wgt);
        for (const sea_leg& l : eo.sea_legs) if (l.uses >= lane_tier) ++wr.lanes_1660;
        wr.lw1660 = attribute_lanes(eo.sea_legs, he.sea_leg_writers, {}, lane_tier);

        // The Industrialisation span on this fold, exactly as generation opens
        // it: the handoff's tables as the resume, its regions surveyed off the
        // generated tiles, the live creeds.
        history_sim_params dp = dp_base;
        dp.trace_battles           = true;
        dp.sea_current_weight_q    = wgt;
        dp.resume_polities         = &eo.polities;
        dp.resume_grudges          = &eo.grudges;
        dp.resume_contacts         = &eo.contacts;
        dp.resume_corridors        = &eo.surviving_corridors;
        dp.resume_sea_legs         = &eo.sea_legs;
        dp.resume_dated_objects    = &eo.dated_objects;
        dp.resume_civilisations    = &eo.civilisations;
        dp.resume_universal_creeds = &eo.universal_creeds;
        ss.regions = eo.regions;
        survey_regions_at_span_open(w, ids, fx.gw, fx.gh, ss.regions);
        const history_sim_state hi = run_history_sim(ss, &cs, fx.terrain.view(), fx.gw, fx.gh, dp,
                                                     dseed, nullptr, fx.works, nullptr);
        wr.i  = read_span(hi, contact_set(eo.contacts), lane_tier, &field, &ss.regions);
        wr.bind_i = census_cross_water(hi, ss.regions, field, mass, fx.gw, fx.gh, wgt);
        wr.lw = attribute_lanes(hi.sea_legs, he.sea_leg_writers, hi.sea_leg_writers, lane_tier);
        wr.secs = seconds_since(t1);

        // FIDELITY: the rung at generation's own weight must reproduce
        // generation's own untraced runs, or the sweep measured another world.
        if (wgt == fx.exploration_params.sea_current_weight_q)
        {
            const span_read& a = wr.e; const span_read& b = row.shipped_e;
            row.fidelity_e = a.battles == b.battles && a.conquests == b.conquests && a.foundings == b.foundings
                          && a.table == b.table && a.lanes == b.lanes && a.noted_tribute == b.noted_tribute
                          && a.noted_campaign == b.noted_campaign && a.noted_purchase == b.noted_purchase;
            const span_read& c = wr.i; const span_read& d = row.shipped_i;
            row.fidelity_i = c.battles == d.battles && c.conquests == d.conquests && c.foundings == d.foundings
                          && c.table == d.table && c.lanes == d.lanes && c.noted_tribute == d.noted_tribute
                          && c.noted_campaign == d.noted_campaign && c.noted_purchase == d.noted_purchase;
        }
        row.weights.push_back(wr);
    }
    return row;
}

void run_sweep(shipped_inputs& shipped, const std::vector<uint32_t>& seeds, const std::vector<int>& weights,
               const std::string& out_path)
{
    std::printf("\n=== --sweep: %zu seeds x %zu weights (timings indicative on a shared machine) ===\n",
                seeds.size(), weights.size());
    std::vector<seed_row> rows;
    for (uint32_t s : seeds)
    {
        seed_row r = sweep_seed(shipped, s, weights);
        if (r.ok)
        {
            std::printf("\n  seed %u (generated in %.0f s)  FIDELITY at generation's weight %d: exploration %s, industrialisation %s\n",
                        s, r.gen_secs, r.gen_weight, r.fidelity_e ? "MATCH" : "MISMATCH", r.fidelity_i ? "MATCH" : "MISMATCH");
            print_span("gen E", r.shipped_e);
            print_span("gen I", r.shipped_i);
            for (const weight_row& wr : r.weights)
            {
                std::printf("   w=%3d (%.0f s)  lanes 1660 %lld (c/p/t/trade %lld/%lld/%lld/%lld), 1960 %lld (c/p/t/trade"
                            " %lld/%lld/%lld/%lld), lane uses c/p/t/trade %lld/%lld/%lld/%lld\n",
                            wr.weight, wr.secs, static_cast<long long>(wr.lanes_1660),
                            static_cast<long long>(wr.lw1660.by_writer[0]), static_cast<long long>(wr.lw1660.by_writer[1]),
                            static_cast<long long>(wr.lw1660.by_writer[2]), static_cast<long long>(wr.lw1660.by_writer[3]),
                            static_cast<long long>(wr.lw.lanes),
                            static_cast<long long>(wr.lw.by_writer[0]), static_cast<long long>(wr.lw.by_writer[1]),
                            static_cast<long long>(wr.lw.by_writer[2]), static_cast<long long>(wr.lw.by_writer[3]),
                            static_cast<long long>(wr.lw.uses_by_writer[0]), static_cast<long long>(wr.lw.uses_by_writer[1]),
                            static_cast<long long>(wr.lw.uses_by_writer[2]), static_cast<long long>(wr.lw.uses_by_writer[3]));
                print_span("  E", wr.e);
                print_span("  I", wr.i);
                std::printf("        cross-water flows at the close, bounded by want/holding/land/sea: "
                            "1660 %lld of %lld/%lld/%lld/%lld, 1960 %lld of %lld/%lld/%lld/%lld",
                            static_cast<long long>(wr.bind_e.flows), static_cast<long long>(wr.bind_e.by_want),
                            static_cast<long long>(wr.bind_e.by_holding), static_cast<long long>(wr.bind_e.by_land),
                            static_cast<long long>(wr.bind_e.by_sea), static_cast<long long>(wr.bind_i.flows),
                            static_cast<long long>(wr.bind_i.by_want), static_cast<long long>(wr.bind_i.by_holding),
                            static_cast<long long>(wr.bind_i.by_land), static_cast<long long>(wr.bind_i.by_sea));
                std::printf("%c", 10);
            }
            std::fflush(stdout);
        }
        rows.push_back(r);
    }

    if (!out_path.empty())
    {
        std::FILE* f = std::fopen(out_path.c_str(), "wb");
        if (f == nullptr) { std::printf("could not write %s\n", out_path.c_str()); return; }
        std::fprintf(f, "{\"weights\": [");
        for (std::size_t k = 0; k < weights.size(); ++k) std::fprintf(f, "%s%d", k ? ", " : "", weights[k]);
        std::fprintf(f, "], \"seeds\": [\n");
        for (std::size_t n = 0; n < rows.size(); ++n)
        {
            const seed_row& r = rows[n];
            std::fprintf(f, " {\"seed\": %u, \"ok\": %s, \"fidelity_e\": %s, \"fidelity_i\": %s, \"gen_secs\": %.1f, ",
                         r.seed, r.ok ? "true" : "false", r.fidelity_e ? "true" : "false",
                         r.fidelity_i ? "true" : "false", r.gen_secs);
            put_span(f, "shipped_e", r.shipped_e); std::fprintf(f, ", ");
            put_span(f, "shipped_i", r.shipped_i); std::fprintf(f, ", \"runs\": [\n");
            for (std::size_t k = 0; k < r.weights.size(); ++k)
            {
                const weight_row& wr = r.weights[k];
                std::fprintf(f, "   {\"weight\": %d, \"secs\": %.1f, \"lanes_1660\": %lld, \"lanes_1960\": %lld, "
                                "\"lanes_by_writer\": [%lld, %lld, %lld, %lld], \"lane_uses_by_writer\": [%lld, %lld, %lld, %lld], "
                                "\"lanes_1660_by_writer\": [%lld, %lld, %lld, %lld], "
                                "\"bind_e\": [%lld, %lld, %lld, %lld, %lld], \"bind_i\": [%lld, %lld, %lld, %lld, %lld], ",
                             wr.weight, wr.secs, static_cast<long long>(wr.lanes_1660), static_cast<long long>(wr.lw.lanes),
                             static_cast<long long>(wr.lw.by_writer[0]), static_cast<long long>(wr.lw.by_writer[1]),
                             static_cast<long long>(wr.lw.by_writer[2]), static_cast<long long>(wr.lw.by_writer[3]),
                             static_cast<long long>(wr.lw.uses_by_writer[0]), static_cast<long long>(wr.lw.uses_by_writer[1]),
                             static_cast<long long>(wr.lw.uses_by_writer[2]), static_cast<long long>(wr.lw.uses_by_writer[3]),
                             static_cast<long long>(wr.lw1660.by_writer[0]), static_cast<long long>(wr.lw1660.by_writer[1]),
                             static_cast<long long>(wr.lw1660.by_writer[2]), static_cast<long long>(wr.lw1660.by_writer[3]),
                             static_cast<long long>(wr.bind_e.flows), static_cast<long long>(wr.bind_e.by_want),
                             static_cast<long long>(wr.bind_e.by_holding), static_cast<long long>(wr.bind_e.by_land),
                             static_cast<long long>(wr.bind_e.by_sea), static_cast<long long>(wr.bind_i.flows),
                             static_cast<long long>(wr.bind_i.by_want), static_cast<long long>(wr.bind_i.by_holding),
                             static_cast<long long>(wr.bind_i.by_land), static_cast<long long>(wr.bind_i.by_sea));
                put_span(f, "e", wr.e); std::fprintf(f, ", ");
                put_span(f, "i", wr.i);
                std::fprintf(f, "}%s\n", k + 1 < r.weights.size() ? "," : "");
            }
            std::fprintf(f, "  ]}%s\n", n + 1 < rows.size() ? "," : "");
        }
        std::fprintf(f, "]}\n");
        std::fclose(f);
        std::printf("\nwrote %s\n", out_path.c_str());
    }
}

// ---------------------------------------------------------------------------
// --picture
// ---------------------------------------------------------------------------

void put_px(std::vector<unsigned char>& img, int W, int H, int x, int y, unsigned char r, unsigned char g,
            unsigned char b)
{
    if (x < 0 || y < 0 || x >= W || y >= H) return;
    unsigned char* p = &img[(static_cast<std::size_t>(y) * W + x) * 4];
    p[0] = r; p[1] = g; p[2] = b; p[3] = 255;
}

void draw_line(std::vector<unsigned char>& img, int W, int H, double x0, double y0, double x1, double y1,
               unsigned char r, unsigned char g, unsigned char b, int thick)
{
    const double len = std::max(std::fabs(x1 - x0), std::fabs(y1 - y0));
    const int n = static_cast<int>(len * 2) + 1;
    for (int i = 0; i <= n; ++i)
    {
        const double t = static_cast<double>(i) / n;
        const int x = static_cast<int>(std::lround(x0 + (x1 - x0) * t));
        const int y = static_cast<int>(std::lround(y0 + (y1 - y0) * t));
        for (int dy = -thick / 2; dy <= thick / 2; ++dy)
            for (int dx = -thick / 2; dx <= thick / 2; ++dx)
                put_px(img, W, H, x + dx, y + dy, r, g, b);
    }
}

void draw_field(const ocean_current_field& f, const std::vector<terrain_substrate>& sub, const std::string& path,
                uint32_t seed)
{
    const int S = 6; // pixels per tile
    const int W = f.gw * S, H = f.gh * S;
    std::vector<unsigned char> img(static_cast<std::size_t>(W) * H * 4, 255);
    for (int r = 0; r < f.gh; ++r)
        for (int c = 0; c < f.gw; ++c)
        {
            const terrain_substrate s = sub[static_cast<std::size_t>(r) * f.gw + c];
            unsigned char cr, cg, cb;
            if (s == terrain_substrate::ocean)      { cr = 24;  cg = 52;  cb = 96;  }
            else if (s == terrain_substrate::coast) { cr = 44;  cg = 84;  cb = 132; }
            else if (s == terrain_substrate::lake)  { cr = 90;  cg = 140; cb = 180; }
            else                                    { cr = 150; cg = 138; cb = 104; }
            for (int y = 0; y < S; ++y)
                for (int x = 0; x < S; ++x) put_px(img, W, H, c * S + x, r * S + y, cr, cg, cb);
        }
    // Band edges at 30 and 60 degrees, and the equator.
    for (int k = -3; k <= 3; ++k)
    {
        const int row = static_cast<int>(std::lround((f.gh - 1) * (1.0 - k / 3.0) / 2.0));
        for (int x = 0; x < W; x += (k == 0 ? 3 : 8))
            put_px(img, W, H, x, row * S + S / 2, 200, 200, 200);
    }
    // One arrow per ocean region, from its centre along its current; length
    // and colour scale with strength (pale = weak, yellow, red = 1000).
    const double cell_px = f.cell * S;
    for (int cy = 0; cy < f.cells_h; ++cy)
        for (int cx = 0; cx < f.cells_w; ++cx)
        {
            const std::size_t k = static_cast<std::size_t>(cy * f.cells_w + cx);
            if (f.sea_tiles[k] == 0) continue;
            const double e = f.east_q[k] / 1000.0, n = f.north_q[k] / 1000.0;
            const double mag = std::sqrt(e * e + n * n);
            const double x0 = (cx + 0.5) * cell_px, y0 = (cy + 0.5) * cell_px;
            if (mag < 0.02) { put_px(img, W, H, static_cast<int>(x0), static_cast<int>(y0), 230, 230, 230); continue; }
            const double m = std::min(1.0, mag);
            const double L = cell_px * (0.2 + 0.3 * m);
            const double ux = e / mag, uy = -n / mag; // screen y grows downward
            const double x1 = x0 + ux * L, y1 = y0 + uy * L;
            const unsigned char cr = 255;
            const unsigned char cg = static_cast<unsigned char>(255 - 200 * m);
            const unsigned char cb = static_cast<unsigned char>(std::max(0.0, 200 - 400 * m));
            draw_line(img, W, H, x0 - ux * L * 0.5, y0 - uy * L * 0.5, x1, y1, cr, cg, cb, 2);
            const double hx = -ux, hy = -uy, a = 0.5;
            const double h = std::max(5.0, L * 0.35);
            draw_line(img, W, H, x1, y1, x1 + (hx * std::cos(a) - hy * std::sin(a)) * h,
                      y1 + (hx * std::sin(a) + hy * std::cos(a)) * h, cr, cg, cb, 2);
            draw_line(img, W, H, x1, y1, x1 + (hx * std::cos(-a) - hy * std::sin(-a)) * h,
                      y1 + (hx * std::sin(-a) + hy * std::cos(-a)) * h, cr, cg, cb, 2);
        }
    const bool ok = write_png_rgba(path, W, H, img.data(), W * 4);
    std::printf("%s the current field of seed %u (%dx%d px, one arrow per %d-tile ocean region) to %s\n",
                ok ? "wrote" : "FAILED to write", seed, W, H, f.cell, path.c_str());
}

void run_picture(shipped_inputs& shipped, uint32_t seed, const std::string& path)
{
    world_params wp;
    wp.seed = seed;
    generation_report rep;
    era_minus_one_fixture fx;
    world_gen_config cfg = shipped.cfg;
    cfg.stop_after_exploration = true;
    const world w = make_hard_coded_world(wp, &rep, cfg, nullptr, &shipped.works, &fx);
    (void)w;
    if (fx.terrain.substrate.empty()) { std::printf("seed %u: no terrain captured\n", seed); return; }
    const ocean_current_field f = build_ocean_currents(fx.terrain.substrate, fx.gw, fx.gh, 1);
    draw_field(f, fx.terrain.substrate, path, seed);
}

std::vector<int> parse_ints(const char* s)
{
    std::vector<int> v;
    const char* p = s;
    while (*p)
    {
        char* end = nullptr;
        const long x = std::strtol(p, &end, 10);
        if (end == p) break;
        v.push_back(static_cast<int>(x));
        p = (*end == ',') ? end + 1 : end;
    }
    return v;
}

} // namespace

int main(int argc, char** argv)
{
    bool sweep = false, picture = false;
    std::vector<uint32_t> seeds(std::begin(k_library), std::end(k_library));
    std::vector<int> weights = {0, 150, 300, 500, 700, 900};
    std::string out_path, png_path = "ocean_currents.png";
    uint32_t seed = 32;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--sweep") sweep = true;
        else if (a == "--picture") picture = true;
        else if (a == "--seeds" && i + 1 < argc)
        {
            seeds.clear();
            for (int x : parse_ints(argv[++i])) seeds.push_back(static_cast<uint32_t>(x));
        }
        else if (a == "--weights" && i + 1 < argc) weights = parse_ints(argv[++i]);
        else if (a == "--out" && i + 1 < argc) out_path = argv[++i];
        else if (a == "--png" && i + 1 < argc) png_path = argv[++i];
        else if (a == "--seed" && i + 1 < argc) seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        else { std::printf("unknown argument %s\n", a.c_str()); return 2; }
    }

    shipped_inputs shipped;
    load_shipped_inputs(shipped); // every mode generates at least one real world

    if (picture)
    {
        run_picture(shipped, seed, png_path);
        return 0;
    }
    if (sweep)
    {
        if (std::find(weights.begin(), weights.end(), 0) == weights.end()) weights.insert(weights.begin(), 0);
        // Generation's own weight always rides the ladder: it is the rung the
        // fidelity check reads.
        const int gen_w = exploration_sim_params(world_params{}).sea_current_weight_q;
        if (std::find(weights.begin(), weights.end(), gen_w) == weights.end()) weights.push_back(gen_w);
        run_sweep(shipped, seeds, weights, out_path);
        return 0;
    }

    synthetic_rows();
    const int shipped_weight = exploration_sim_params(world_params{}).sea_current_weight_q;
    real_body_rows(shipped, seed, shipped_weight > 0 ? shipped_weight : 300);
    std::printf("\n%s: %d failure(s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
