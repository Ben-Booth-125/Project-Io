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
//   C7-C10 BL-1142: a leg against its current delivers less, after sizing,
//       and only where the SEA line carries the goods; a binding is worth what
//       arrives; meeting by sea reads the sea line and the seller's navy alone
//   F1-F5 BL-1142, a resumed two-island span: the far penalty a pair reads is
//       the class its contact recorded at the meeting -- formation and the
//       break agree, a seat that moves across the water changes nothing, a far
//       pair on one landmass still reads the land's 700 -- and an out-of-range
//       cargo loss is rejected, never clamped
//   W8  BL-1140: on the real body, trade BY SEA between realms on different
//       landmasses writes sea-leg uses (a road between them writes none),
//       only such trade does, and each use is read against the current
//   W1-W7 the same properties on a real body (seed 32 by default, --seed N),
//       plus the sim: a fixture re-run at generation's own weight reproduces
//       generation's Exploration span, weight 0 builds no field, a weighted
//       span is deterministic, a rejected weight prices nothing and says so,
//       the writer record sums to the leg table.
//   W9  BL-1142: generation's Industrialisation span meets and binds realms
//       across water and loses cargo against the current (the Exploration
//       span's loss printed beside it, on its own)
//   W10 BL-1142: on the Industrialisation span resumed from generation's 1660
//       handoff, a cargo loss outside [0, 1000] is rejected, never clamped
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
// --synthetic — the C, F and N rows alone (no real body, no data layer).
//
// --naval — BL-1147's census: per seed of the library (or --seeds), one
//   generation, then the naval ledger the Empires round left at 1200 -- each
//   deed's raw tally and weighted points over the living polities, what the
//   polities dead by 1200 held, the polities that lost their coast -- and the
//   ledger's conservation across both later handoffs. --out path writes JSON.
//   A rung prefix "e:" (--rung) applies a dial to the Exploration re-run only:
//   the conversion `naval_points_navy_per_1000` is read there.
//
// BL-1147 rows: N0-N9 on a resumed island of nine cells (the coast cells; a
//   polity that lost its coast keeps its points; a dead polity carries none;
//   at 0 nothing moves; an out-of-domain constant is rejected; no later span
//   adds to the ledger; a province counts one coastal year per year held where
//   its cell touches the sea, whatever its port_q; only the Exploration open
//   converts), N7-N8 on the real body (the Empires round tallies deeds and the
//   ledger crosses both handoffs; the conversion opens every living polity at
//   exactly its points' fleet). C11 and W11 (BL-1147 review): a corridor walked
//   across sea is marked wet exactly where its line crosses sea, and offers no
//   land line. A row labelled "(guard)" passes by construction today and
//   guards a regression; every other row can fail.
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
    // touching only at a corner is TWO landmasses (every traveller walks the
    // four cardinal steps), and a seat on the shoreline reads the landmass it
    // borders.
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
        check(mc[44] >= 0 && mc[55] >= 0 && mc[44] != mc[55],
              "C6  ground touching only at a corner is two landmasses (traversal is four-cardinal)");
    }

    // C7 (BL-1142): a leg run against its current delivers less. Basin A again:
    // the west seat (6,25) sells farm goods east against the trades and the
    // east seat (42,25) sells ore west with them, both opened by one
    // trade-access clause; the loss is applied after the flows are sized.
    {
        std::vector<region> rg(2);
        rg[0].col = 6;  rg[0].row = 25; rg[0].anchor = 25 * ww + 6;  rg[0].nation = 0;
        rg[1].col = 42; rg[1].row = 25; rg[1].anchor = 25 * ww + 42; rg[1].nation = 1;
        rg[0].dominant = region_class::farm; rg[1].dominant = region_class::ore;
        for (region& r : rg) r.port_stock_q = 400;
        rg[1].scarcity_raw_q[0] = 900; // the east wants farm goods
        rg[0].scarcity_raw_q[1] = 900; // the west wants ore
        std::vector<polity> qs(2);
        for (int k = 0; k < 2; ++k) { qs[k].id = k; qs[k].alive = true; qs[k].capital = k; qs[k].navy_stock = 500; }
        const std::vector<int32_t> mass = landmass_labels(two, ww, wh);
        std::vector<dated_object> treaties = {
            dated_object{2000, static_cast<int32_t>(treaty_clause::trade_access), 0, 1} };
        trade_context ctx = build_trade_context(rg, qs, {});
        ctx.currents = &fb; ctx.current_weight_q = 500;
        const std::vector<trade_flow> sized = compute_trade_flows(ctx, rg, qs, treaties);
        int64_t lost = 0;
        // Each polity's seat landmass, as the sim reads it once per round.
        const std::vector<int32_t> seats = { landmass_at(mass, ww, wh, 6, 25), landmass_at(mass, ww, wh, 42, 25) };
        ctx.seat_landmass = &seats; ctx.cargo_loss_q = 500; ctx.cargo_lost = &lost;
        const std::vector<trade_flow> delivered = compute_trade_flows(ctx, rg, qs, treaties);
        const auto vol = [](const std::vector<trade_flow>& f, int s, int b) {
            for (const trade_flow& x : f) if (x.seller == s && x.buyer == b) return x.volume_q;
            return 0;
        };
        const int east_sized = vol(sized, 0, 1), east_got = vol(delivered, 0, 1);
        const int west_sized = vol(sized, 1, 0), west_got = vol(delivered, 1, 0);
        const int align_east = ocean_current_alignment_q(fb, 6, 25, 42, 25);
        const int expect_east = static_cast<int>((static_cast<int64_t>(east_sized)
                                * (1000 - (500 * (align_east < 0 ? -align_east : 0)) / 1000)) / 1000);
        std::printf("      cargo across basin A at loss 500: east (against, alignment %d) sized %d delivered %d;"
                    " west (with) sized %d delivered %d; lost %lld\n",
                    align_east, east_sized, east_got, west_sized, west_got, static_cast<long long>(lost));
        check(align_east < 0 && east_sized > 0 && east_got == expect_east && east_got < east_sized,
              "C7  a flow across water run against its current delivers volume x (1 - loss x against), after sizing");
        check(west_got == west_sized && west_sized > 0,
              "C7  a flow run with its current delivers exactly what was sized, never more");
        check(lost == static_cast<int64_t>(east_sized - east_got),
              "C7  the cargo lost is counted, and is the whole of the difference");
        ctx.cargo_loss_q = 0;
        check(compute_trade_flows(ctx, rg, qs, treaties).size() == sized.size()
              && vol(compute_trade_flows(ctx, rg, qs, treaties), 0, 1) == east_sized,
              "C7  a loss of 0 delivers every flow as sized");

        // C8 (BL-1142 review): THE LOSS RIDES THE SEA LINE ONLY. The same pair
        // with a land corridor of 800 -- wider than either priced sea line --
        // moves its goods by road, and a road has no current: nothing is lost.
        // With a corridor of 100 the sea still carries them and the loss is
        // C7's exactly.
        {
            trade_context wide = ctx;
            wide.land_lines = { trade_context::land_line{0, 1, 800} };
            wide.cargo_loss_q = 0;
            const std::vector<trade_flow> by_road_sized = compute_trade_flows(wide, rg, qs, treaties);
            int64_t lost_road = 0;
            wide.cargo_loss_q = 500; wide.cargo_lost = &lost_road;
            const std::vector<trade_flow> by_road = compute_trade_flows(wide, rg, qs, treaties);
            trade_context narrow = ctx;
            narrow.land_lines = { trade_context::land_line{0, 1, 100} };
            int64_t lost_narrow = 0;
            narrow.cargo_loss_q = 500; narrow.cargo_lost = &lost_narrow;
            const std::vector<trade_flow> by_sea = compute_trade_flows(narrow, rg, qs, treaties);
            std::printf("      a road of 800 beside the sea: east sized %d delivered %d, lost %lld;"
                        " a road of 100: east delivered %d (C7 %d), lost %lld\n",
                        vol(by_road_sized, 0, 1), vol(by_road, 0, 1), static_cast<long long>(lost_road),
                        vol(by_sea, 0, 1), east_got, static_cast<long long>(lost_narrow));
            check(vol(by_road_sized, 0, 1) == 800 && vol(by_road, 0, 1) == 800 && lost_road == 0,
                  "C8  a flow a road carries loses nothing to the current, though its seats stand across water");
            check(vol(by_sea, 0, 1) == east_got && lost_narrow == lost,
                  "C8  a flow the sea still carries past a narrower road loses what C7's does");
            // The same line test marks the flow for the fourth sea-leg writer:
            // a flow the road carries is not trade across water.
            bool road_marked_land = !by_road.empty(), sea_marked_sea = !by_sea.empty();
            for (const trade_flow& f : by_road) if (f.by_sea != 0) road_marked_land = false;
            for (const trade_flow& f : by_sea)  if (f.by_sea != 1) sea_marked_sea = false;
            check(road_marked_land && sea_marked_sea,
                  "C8  compute_trade_flows marks a flow by sea only where its sea line carries it (the lane's writer reads the mark)");
            // THE TIE GOES TO THE ROAD: a corridor exactly as wide as the priced
            // sea line east (C7's sized 302) carries the east flow by road --
            // unmarked, and it loses nothing -- while west, whose sea line the
            // current widens past it, still goes by sea.
            trade_context tie = ctx;
            tie.land_lines = { trade_context::land_line{0, 1, east_sized} };
            int64_t lost_tie = 0;
            tie.cargo_loss_q = 500; tie.cargo_lost = &lost_tie;
            const std::vector<trade_flow> by_tie = compute_trade_flows(tie, rg, qs, treaties);
            int east_mark = -1, west_mark = -1;
            for (const trade_flow& f : by_tie)
            {
                if (f.seller == 0 && f.buyer == 1) east_mark = f.by_sea;
                if (f.seller == 1 && f.buyer == 0) west_mark = f.by_sea;
            }
            std::printf("      a road exactly as wide as the east sea line (%d): east marked %d, delivered %d, lost %lld;"
                        " west marked %d\n", east_sized, east_mark, vol(by_tie, 0, 1),
                        static_cast<long long>(lost_tie), west_mark);
            check(east_mark == 0 && vol(by_tie, 0, 1) == east_sized && lost_tie == 0 && west_mark == 1,
                  "C8  a tie between the sea line and the road goes to the road: unmarked, and no cargo lost");
        }

        // C11 (BL-1147 review): A CORRIDOR WALKED ACROSS SEA IS NO ROAD. The
        // same pair joined by one corridor record between their seats: dry, it
        // is a land line of the weaker end's reach; marked wet (a wet
        // campaign's, or a line over a strait), it offers none.
        {
            history_corridor dry{0, 1, 5};
            history_corridor wet{0, 1, 5};
            wet.wet = 1;
            const trade_context with_dry = build_trade_context(rg, qs, { dry });
            const trade_context with_wet = build_trade_context(rg, qs, { wet });
            check(with_dry.land_lines.size() == 1 && with_dry.land_lines[0].line_q == 1000
                  && with_wet.land_lines.empty(),
                  "C11 a corridor walked across sea offers no land line to trade; a dry one does");
        }

        // C9 (BL-1142 review): A BINDING IS WORTH WHAT ARRIVES. The pair's
        // trade value at loss 500 is the eastward volume's delivered share
        // plus the whole westward volume (which runs with the current);
        // at loss 0 it is both volumes whole.
        {
            trade_context v = ctx;
            v.cargo_lost = nullptr;
            v.cargo_loss_q = 0;
            const int value_whole = pair_trade_value_q(v, rg, qs, 0, 1, {});
            v.cargo_loss_q = 500;
            const int value_arrives = pair_trade_value_q(v, rg, qs, 0, 1, {});
            const int east_v = trade_flow_volume_q(v, rg, qs, 0, 1, 0);
            const int west_v = trade_flow_volume_q(v, rg, qs, 1, 0, 1);
            const int east_share = 1000 - (500 * (align_east < 0 ? -align_east : 0)) / 1000;
            const int expect = static_cast<int>((static_cast<int64_t>(east_v) * east_share) / 1000) + west_v;
            std::printf("      pair trade value: whole %d, at loss 500 %d (expected %d = %d x %d/1000 + %d)\n",
                        value_whole, value_arrives, expect, east_v, east_share, west_v);
            check(value_whole == east_v + west_v && value_arrives == expect && value_arrives < value_whole,
                  "C9  pair_trade_value_q reads the delivered share: the leg against the current is worth what arrives");
        }

        // C10 (BL-1142 review): MEETING BY SEA READS THE SEA LINE ALONE.
        // `trade_sea_volume_q` is what the meeting walk asks: a road of 800
        // opens a trade (`trade_flow_volume_q`) but never a meeting; a seller
        // with no navy meets no one, whatever the buyer's fleet; a seat with
        // no port meets no one.
        {
            trade_context m = build_trade_context(rg, qs, {});
            m.land_lines = { trade_context::land_line{0, 1, 800} };
            std::vector<polity> no_fleet_west = qs;
            no_fleet_west[0].navy_stock = 0;
            std::vector<region> no_port_east = rg;
            no_port_east[1].port_stock_q = 0;
            const int road   = trade_flow_volume_q(m, rg, no_fleet_west, 0, 1, 0);
            const int sea_w  = trade_sea_volume_q(m, rg, no_fleet_west, 0, 1, 0);
            const int sea_e  = trade_sea_volume_q(m, rg, no_fleet_west, 1, 0, 1);
            const int sea_np = trade_sea_volume_q(m, no_port_east, qs, 0, 1, 0);
            std::printf("      meeting reads: road %d; sea, west selling without a fleet %d; east selling with one %d;"
                        " to a seat with no port %d\n", road, sea_w, sea_e, sea_np);
            check(road == 800 && sea_w == 0 && sea_e == 400 && sea_np == 0
                  && trade_flow_volume_q(m, no_port_east, qs, 0, 1, 0) == 800,
                  "C10 meeting by sea needs the SELLER's navy and both seats' ports: a road's volume never opens it");
        }
    }
}

// ---------------------------------------------------------------------------
// F1-F5 (BL-1142 review): the far penalty is the class the meeting recorded
// ---------------------------------------------------------------------------

/// Two islands on open ocean, 64 x 21: West spans columns 2-21 and East
/// 34-53, both rows 5-15. Every seat stands inland on row 10, so its
/// landmass is its own tile's.
struct far_world
{
    static constexpr int gw = 64, gh = 21;
    std::vector<terrain_substrate> ground;
    settlement_state               ss;
    std::vector<polity>            polities;
    std::vector<contact>           contacts;
    std::vector<dated_object>      objects;
};

/// Polity 0 seated at column 4 (West); polity 1 seated at @p seat1_col (West
/// below 22, East from 34) and, when @p old_seat_col >= 0, holding the ground
/// there too -- the seat it moved from. Both lean 600 on aggression, so a
/// binding is worth 1000 - 150 = 850 before the far penalty: 850 at the sea's
/// 0, 150 at the land's 700 -- over the formation bar (400) at the first,
/// under the break bar (200) at the second. No ports, fleets or corridors:
/// no trade, so nothing else moves the value. They met in 1300 -- a FAR pair
/// (the near-home cutoff is 1200) -- with @p across_water as the meeting
/// recorded it; when @p bound they hold the four mutual clauses to 1800.
far_world make_far_world(int seat1_col, int old_seat_col, bool across_water, bool bound)
{
    far_world w;
    w.ground = make_body(far_world::gw, far_world::gh, [](int c, int r) {
        return r >= 5 && r <= 15 && ((c >= 2 && c <= 21) || (c >= 34 && c <= 53));
    });
    const auto add_region = [&](int col, int culture, int nation, bool seat, int seat_region) {
        region r;
        r.col = col; r.row = 10; r.anchor = 10 * far_world::gw + col;
        r.culture = culture_shares::pure(culture); r.founding_culture = culture;
        r.farm_q = 600; r.ore_q = 300; r.energy_q = 200; r.port_q = 0;
        r.settle_score_q = 800; r.population = 120000;
        r.nation = nation; r.is_seat = seat; r.seat_region = seat_region; r.has_market = seat;
        r.name = "Isle " + std::to_string(w.ss.regions.size());
        w.ss.regions.push_back(r);
    };
    add_region(4, 0, 0, true, 0);                                     // region 0: polity 0's seat
    add_region(seat1_col, 1, 1, true, 1);                             // region 1: polity 1's seat
    if (old_seat_col >= 0) add_region(old_seat_col, 1, 1, false, 1);  // region 2: the ground it left
    for (int k = 0; k < 2; ++k)
    {
        polity q;
        q.id = k; q.culture = k; q.capital = k; q.aggression_q = 600; q.alive = true;
        w.polities.push_back(q);
    }
    contact_event e;
    e.year = 1300; e.region = 0; e.kind = contact_kind::campaign;
    e.across_water = across_water ? 1 : 0;
    contact c01; c01.from = 0; c01.to = 1; c01.first = e;
    contact c10; c10.from = 1; c10.to = 0; c10.first = e;
    w.contacts = { c01, c10 };
    if (bound)
        for (treaty_clause tc : { treaty_clause::non_aggression, treaty_clause::trade_access,
                                  treaty_clause::sphere_of_claim, treaty_clause::mutual_defence })
            w.objects.push_back(dated_object{ 1800, static_cast<int32_t>(tc), 0, 1 });
    return w;
}

/// One decision round (1700) of Exploration's params resumed on @p w, at the
/// given land and sea far penalties and cargo loss.
history_sim_state run_far(const far_world& w, int land_pen, int sea_pen, int loss = 0)
{
    history_sim_params p = exploration_sim_params(world_params{});
    p.start_year = 1700;
    p.stop_year  = 1704;
    p.tick_bands[0]   = { p.stop_year, 4 };
    p.tick_band_count = 1;
    p.trace_battles   = false;
    p.treaty_far_penalty_q     = land_pen;
    p.treaty_far_sea_penalty_q = sea_pen;
    p.far_pairs_meet_by_sea    = true;
    p.sea_current_cargo_loss_q = loss;
    p.resume_polities      = &w.polities;
    p.resume_contacts      = &w.contacts;
    p.resume_dated_objects = &w.objects;
    settlement_state ss = w.ss;
    sim_terrain_view view;
    view.substrate = &w.ground;
    return run_history_sim(ss, nullptr, view, far_world::gw, far_world::gh, p, 4242u);
}

void far_pair_rows()
{
    std::printf("\n--- F1-F5: the far penalty is the class the meeting recorded ---\n");
    const auto bound = [](const history_sim_state& hs) {
        return has_treaty_clause(hs, 0, 1, treaty_clause::non_aggression);
    };
    const auto say = [&](const char* what, const history_sim_state& hs) {
        std::printf("      %-58s formed %lld (across water %lld, far %lld), broken %lld, bound at the close %d\n",
                    what, static_cast<long long>(hs.treaties_formed),
                    static_cast<long long>(hs.treaties_formed_across_water),
                    static_cast<long long>(hs.far_treaties_formed_across_water),
                    static_cast<long long>(hs.treaties_broken), bound(hs) ? 1 : 0);
    };

    // F1: formation against the break.
    const history_sim_state f1 = run_far(make_far_world(36, -1, true, false), 700, 0);
    say("F1 met across water, seats across water, unbound:", f1);
    check(f1.treaties_formed == 1 && f1.far_treaties_formed_across_water == 1
          && f1.treaties_broken == 0 && bound(f1),
          "F1  a far pair met across water binds on the sea's penalty, and the break re-score reads the same: it holds");

    // F2: the seat moves.
    const history_sim_state f2 = run_far(make_far_world(19, 36, true, true), 700, 0);
    const history_sim_state f2_land = run_far(make_far_world(19, 36, false, true), 700, 0);
    say("F2 bound across water, polity 1's seat now on West:", f2);
    say("F2 control -- the same pair recorded as met on land:", f2_land);
    bool kept_class = false;
    for (const contact& c : f2.contacts)
        if (c.from == 0 && c.to == 1) kept_class = c.first.across_water == 1;
    check(f2.treaties_broken == 0 && bound(f2) && kept_class,
          "F2  a pair bound across water whose seat moves onto the other landmass does not break for that");
    check(f2_land.treaties_broken == 1 && !bound(f2_land),
          "F2  ... where the same pair recorded as met on land breaks on the land's penalty (the row can fail)");

    // F3: a far pair on one landmass.
    const history_sim_state f3 = run_far(make_far_world(19, -1, false, false), 700, 0);
    const history_sim_state f3_free = run_far(make_far_world(19, -1, false, false), 0, 0);
    say("F3 met on land, both seats on West, land penalty 700:", f3);
    say("F3 control -- the land penalty at 0:", f3_free);
    check(f3.treaties_formed == 0 && !bound(f3) && f3_free.treaties_formed == 1 && bound(f3_free),
          "F3  a far pair on the SAME landmass still reads 700: it does not bind, where a penalty of 0 would bind it");

    // F4: the mirror of F2 at formation.
    const history_sim_state f4 = run_far(make_far_world(36, -1, false, false), 700, 0);
    say("F4 met on land, polity 1's seat since moved to East:", f4);
    check(f4.treaties_formed == 0 && f4.treaties_formed_across_water == 0,
          "F4  a pair that met on one landmass keeps the land's penalty after a seat crosses the water");

    // F5: the cargo loss's domain.
    const far_world w5 = make_far_world(36, -1, true, false);
    const history_sim_state over  = run_far(w5, 700, 0, 1001);
    const history_sim_state under = run_far(w5, 700, 0, -1);
    const history_sim_state top   = run_far(w5, 700, 0, 1000);
    const history_sim_state none  = run_far(w5, 700, 0, 0);
    check(over.sea_cargo_loss_rejected && under.sea_cargo_loss_rejected
          && !top.sea_cargo_loss_rejected && !none.sea_cargo_loss_rejected,
          "F5  a cargo loss outside [0, 1000] is rejected at the open and says so; 0 and 1000 are in its domain");
}

// ---------------------------------------------------------------------------
// N0-N9 (BL-1147): the naval ledger and the fleet it carries
// ---------------------------------------------------------------------------

/// ONE ISLAND (cols 2-30, rows 2-18 of the far world's ocean) cut into nine
/// cells by a 3 x 3 lattice of regions at cols 6/16/26, rows 5/10/15. The
/// centre cell (16,10) spans cols 11-21, rows 8-12 and TOUCHES NO SEA; the
/// eight around it do. Polity 0 holds the centre only -- an INLAND realm with
/// a port window on it (`port_q` 350, the 70% a settled daughter inherits) --
/// and did 3,000 coastal province-years, 12 crossings and 2 sea techs: it
/// LOST ITS COAST. Polity 1 holds the eight coastal cells, port_q 0 on each,
/// and did nothing. Polity 2 did 500 / 3 / 1 and is DEAD, holding nothing.
far_world make_naval_world()
{
    far_world w;
    w.ground = make_body(far_world::gw, far_world::gh, [](int c, int r) {
        return r >= 2 && r <= 18 && c >= 2 && c <= 30;
    });
    const int cols[3] = { 6, 16, 26 }, rows[3] = { 5, 10, 15 };
    for (int ry = 0; ry < 3; ++ry)
        for (int cx = 0; cx < 3; ++cx)
        {
            const bool centre = cx == 1 && ry == 1;
            region r;
            r.col = cols[cx]; r.row = rows[ry]; r.anchor = r.row * far_world::gw + r.col;
            const int owner = centre ? 0 : 1;
            r.culture = culture_shares::pure(owner); r.founding_culture = owner;
            r.farm_q = 600; r.ore_q = 300; r.energy_q = 200; r.port_q = centre ? 350 : 0;
            r.settle_score_q = 800; r.population = 1000;
            r.nation = owner;
            r.name = "Isle " + std::to_string(w.ss.regions.size());
            w.ss.regions.push_back(r);
        }
    // Seats: the centre (index 4) for polity 0, the first corner (index 0) for polity 1.
    for (std::size_t i = 0; i < w.ss.regions.size(); ++i)
    {
        region& r = w.ss.regions[i];
        const bool seat = i == 4 || i == 0;
        r.is_seat = seat; r.has_market = seat;
        r.seat_region = r.nation == 0 ? 4 : 0;
    }
    polity p0; p0.id = 0; p0.culture = 0; p0.capital = 4; p0.alive = true; p0.aggression_q = 600;
    p0.naval_coastal_years = 3000; p0.naval_crossings = 12; p0.naval_sea_techs = 2;
    polity p1; p1.id = 1; p1.culture = 1; p1.capital = 0; p1.alive = true; p1.aggression_q = 600;
    polity dead; dead.id = 2; dead.culture = 2; dead.capital = -1; dead.alive = false; dead.aggression_q = 600;
    dead.naval_coastal_years = 500; dead.naval_crossings = 3; dead.naval_sea_techs = 1;
    w.polities = { p0, p1, dead };
    return w;
}

history_sim_state run_naval_span(const far_world& w, int64_t per_1000, bool accrue, int64_t stop = 1704,
                                 bool exploration_open = true)
{
    history_sim_params p = exploration_open ? exploration_sim_params(world_params{})
                                            : industrialisation_sim_params(world_params{});
    p.start_year = 1700;
    p.stop_year  = stop;
    p.tick_bands[0]   = { stop, 4 };
    p.tick_band_count = 1;
    p.trace_battles   = false;
    p.naval_points_navy_per_1000 = per_1000;
    p.naval_points_accrue        = accrue;
    p.capture_year               = 1700; // the OPENING state, after the conversion
    p.resume_polities      = &w.polities;
    p.resume_contacts      = &w.contacts;
    p.resume_dated_objects = &w.objects;
    settlement_state ss = w.ss;
    sim_terrain_view view;
    view.substrate = &w.ground;
    return run_history_sim(ss, nullptr, view, far_world::gw, far_world::gh, p, 4242u);
}

void naval_rows()
{
    std::printf("\n--- N0-N9: the naval ledger and the fleet it carries ---\n");
    const far_world w = make_naval_world();
    const history_sim_params dp = exploration_sim_params(world_params{});
    const auto pts = [&](const polity& q) { return naval_points_of(q, dp).total; };
    const int64_t k = 250;
    const history_sim_state on = run_naval_span(w, k, false);
    const auto opening = [](const history_sim_state& hs, int id) -> int64_t {
        return hs.capture.captured && static_cast<std::size_t>(id) < hs.capture.polities.size()
                   ? hs.capture.polities[static_cast<std::size_t>(id)].navy_stock : -1;
    };
    coast_cells cc;
    update_coast_cells(cc, w.ss.regions, w.ground, far_world::gw, far_world::gh);
    const bool centre_inland = !cell_touches_sea(cc, 4);
    bool ring_coastal = true;
    for (std::size_t i = 0; i < 9; ++i) if (i != 4 && !cell_touches_sea(cc, i)) ring_coastal = false;
    const int64_t p0 = pts(w.polities[0]), p2 = pts(w.polities[2]);
    const int64_t expect0 = (p0 / 1000) * k + ((p0 % 1000) * k) / 1000;
    std::printf("      the centre cell touches the sea: %s; the eight around it: %s\n",
                centre_inland ? "no" : "YES", ring_coastal ? "all" : "NOT ALL");
    std::printf("      at %lld hulls per 1000 points: polity 0 (%lld points, holding only the inland centre) opens with %lld;"
                " polity 1 (0 points) %lld; dead polity 2 (%lld points) %lld; carried %lld, died %lld,"
                " fleets opened %lld\n",
                static_cast<long long>(k), static_cast<long long>(p0), static_cast<long long>(opening(on, 0)),
                static_cast<long long>(opening(on, 1)), static_cast<long long>(p2),
                static_cast<long long>(opening(on, 2)), static_cast<long long>(on.naval_points_carried),
                static_cast<long long>(on.naval_points_died), static_cast<long long>(on.naval_fleets_opened));
    check(centre_inland && ring_coastal, "N0  the coast cells: the inland centre touches no sea, the ring around it does");
    check(p0 > 0 && opening(on, 0) == expect0 && opening(on, 0) > 0,
          "N1  a polity that lost its coast keeps its points: holding only inland ground, it opens with points x rate / 1000");
    check(opening(on, 2) == 0 && on.naval_points_died == p2 && on.naval_points_carried == p0
          && on.naval_fleets_opened == 1 && opening(on, 1) == 0,
          "N2  a polity dead at the open carries nothing: its points die with it, counted");
    // N3: at 0 nothing moves -- the same world with its ledgers wiped runs identically.
    far_world wiped = w;
    for (polity& q : wiped.polities) { q.naval_coastal_years = 0; q.naval_crossings = 0; q.naval_sea_techs = 0; }
    const history_sim_state off = run_naval_span(w, 0, false), off_wiped = run_naval_span(wiped, 0, false);
    bool same = off.battles == off_wiped.battles && off.treaties_formed == off_wiped.treaties_formed
             && off.polities.size() == off_wiped.polities.size()
             && off.naval_fleets_opened == 0 && opening(off, 0) == 0;
    for (std::size_t i = 0; same && i < off.polities.size(); ++i)
        if (off.polities[i].navy_stock != off_wiped.polities[i].navy_stock) same = false;
    check(same, "N3  at a conversion of 0 the ledger moves nothing: the world runs as if it were empty");
    // N4: rejected, never clamped.
    const history_sim_state neg = run_naval_span(w, -1, false), big = run_naval_span(w, 100001, false);
    check(neg.naval_points_params_rejected && big.naval_points_params_rejected
          && opening(neg, 0) == 0 && opening(big, 0) == 0 && !on.naval_points_params_rejected,
          "N4  a conversion outside [0, 100000] is rejected at the open and says so: no fleet opens");
    // N5 (construction guard): nothing but the accrual writes the ledger.
    bool kept = on.polities.size() == w.polities.size();
    for (std::size_t i = 0; kept && i < w.polities.size(); ++i)
        kept = on.polities[i].naval_coastal_years == w.polities[i].naval_coastal_years
            && on.polities[i].naval_crossings == w.polities[i].naval_crossings
            && on.polities[i].naval_sea_techs == w.polities[i].naval_sea_techs;
    check(kept, "N5  (construction guard) a span that does not accrue ends with the ledger it resumed");
    // N6: the deed reads the TERRAIN, not port_q -- one round, off the opening map.
    const history_sim_state acc = run_naval_span(w, 0, true, 1704);
    const int64_t d0 = acc.polities[0].naval_coastal_years - w.polities[0].naval_coastal_years;
    const int64_t d1 = acc.polities[1].naval_coastal_years - w.polities[1].naval_coastal_years;
    std::printf("      accruing over one round (1700-1704): polity 0 (the inland centre, port_q 350) +%lld;"
                " polity 1 (eight coastal cells, port_q 0) +%lld\n",
                static_cast<long long>(d0), static_cast<long long>(d1));
    check(d0 == 0 && d1 == 8 * 4 && acc.polities[2].naval_coastal_years == w.polities[2].naval_coastal_years,
          "N6  a held region counts one coastal year per year held where its cell touches the sea, whatever its port_q");
    // N9: the conversion is the Exploration open's alone -- a later span's
    // resumed open converts nothing, whatever the rate a sweep sets on it.
    const history_sim_state later = run_naval_span(w, k, false, 1704, /*exploration_open=*/false);
    std::printf("      the Industrialisation span's open at %lld per 1000: fleets opened %lld, polity 0 opens with %lld\n",
                static_cast<long long>(k), static_cast<long long>(later.naval_fleets_opened),
                static_cast<long long>(opening(later, 0)));
    check(later.naval_fleets_opened == 0 && opening(later, 0) == 0 && !later.naval_points_params_rejected,
          "N9  only the Exploration open converts: a later resumed span opens no fleet at any rate");
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

/// --set NAME=V (both spans) and --set-i NAME=V (the Industrialisation span
/// only): BL-1142's measurement dials, applied to the re-runs after every
/// other default. A name outside the list refuses the run.
std::vector<std::pair<std::string, int>> g_set_both, g_set_ind;

bool apply_set(history_sim_params& p, const std::string& name, int v)
{
    if (name == "far_pairs_meet_by_sea")         { p.far_pairs_meet_by_sea = v != 0; return true; }
    if (name == "treaty_far_sea_penalty_q")      { p.treaty_far_sea_penalty_q = v; return true; }
    if (name == "sea_current_cargo_loss_q")      { p.sea_current_cargo_loss_q = v; return true; }
    if (name == "treaty_far_penalty_q")          { p.treaty_far_penalty_q = v; return true; }
    // BL-1147: the conversion and the three deeds' weights.
    if (name == "naval_points_navy_per_1000")    { p.naval_points_navy_per_1000 = v; return true; }
    if (name == "naval_points_per_coastal_year") { p.naval_points_per_coastal_year = v; return true; }
    if (name == "naval_points_per_crossing")     { p.naval_points_per_crossing = v; return true; }
    if (name == "naval_points_per_sea_tech")     { p.naval_points_per_sea_tech = v; return true; }
    return false;
}

void apply_sets(history_sim_params& p, const std::vector<std::pair<std::string, int>>& sets)
{
    for (const auto& kv : sets) apply_set(p, kv.first, kv.second);
}

/// --rung SPEC (repeatable): one ladder rung per flag, all at the first
/// --weights value, each a comma list of NAME=V; a NAME prefixed "i:" applies
/// to the Industrialisation re-run only. An empty SPEC ("-") is the baseline.
struct rung_spec
{
    std::string label;
    std::vector<std::pair<std::string, int>> both, ind;
    std::vector<std::pair<std::string, int>> expl; ///< BL-1147: "e:" -- the Exploration re-run only
};
std::vector<rung_spec> g_rungs;

bool parse_rung(const std::string& spec, rung_spec& r)
{
    r.label = spec;
    if (spec == "-") return true;
    std::size_t i = 0;
    while (i < spec.size())
    {
        const std::size_t j = std::min(spec.find(',', i), spec.size());
        std::string kv = spec.substr(i, j - i);
        i = j + 1;
        bool ind = false, expl = false;
        if (kv.rfind("i:", 0) == 0) { ind = true; kv = kv.substr(2); }
        else if (kv.rfind("e:", 0) == 0) { expl = true; kv = kv.substr(2); }
        const std::size_t eq = kv.find('=');
        if (eq == std::string::npos) return false;
        history_sim_params probe;
        const std::string name = kv.substr(0, eq);
        const int v = std::atoi(kv.c_str() + eq + 1);
        if (!apply_set(probe, name, v)) return false;
        (ind ? r.ind : expl ? r.expl : r.both).push_back({name, v});
    }
    return true;
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
    int64_t tr_volume_road = 0; ///< BL-1147 review: the same pairs' volume a dry corridor carried
    int64_t with = 0, against = 0, slack = 0, align_sum = 0;
    /// The same with/against reading taken off the battle traces against the
    /// body's field, so it exists at weight 0 too (where the sim builds none):
    /// every traced wet battle, staging hub to target.
    int64_t t_with = 0, t_against = 0, t_slack = 0, t_align_sum = 0;
    int64_t subjections = 0, bought = 0, freed = 0;
    uint64_t field = 0;
    /// BL-1142: pairs met by sea, treaties formed across water (and of them far
    /// pairs), cargo lost against the current; and at the close, realm pairs on
    /// different landmasses in contact / bound by trade access.
    int64_t met_by_sea = 0, treaties_across = 0, far_treaties_across = 0, cargo_lost = 0;
    int64_t cross_contacted = 0, cross_bound = 0, cross_far_bound = 0;
    /// BL-1147: the span's first crossing (the year of its first traced wet
    /// campaign; 0 = none); the fleets standing at its close (living polities
    /// with navy > 0), their hulls in total and the largest; the navy steps the
    /// span bought and the treasury it spent keeping fleets.
    int64_t first_crossing_year = 0;
    int64_t fleets_close = 0, navy_close = 0, navy_close_max = 0;
    int64_t navy_steps = 0, navy_upkeep_spent = 0;
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
    s.tr_volume_road = hs.cross_landmass_volume_by_road_q;
    s.opened         = hs.sea_lanes_opened;
    s.with = hs.sea_campaigns_with_current; s.against = hs.sea_campaigns_against_current;
    s.slack = hs.sea_campaigns_slack_current; s.align_sum = hs.sea_campaign_alignment_sum_q;
    s.subjections = hs.subjections_formed; s.bought = hs.provinces_bought; s.freed = hs.subjections_freed;
    s.field = hs.sea_current_field_digest;
    s.met_by_sea = hs.contacts_met_by_sea;
    s.treaties_across = hs.treaties_formed_across_water;
    s.far_treaties_across = hs.far_treaties_formed_across_water;
    s.cargo_lost = hs.sea_trade_cargo_lost_q;
    for (const battle_trace& bt : hs.battle_traces)
        if (!bt.exec_dry && (s.first_crossing_year == 0 || bt.year < s.first_crossing_year))
            s.first_crossing_year = bt.year;
    for (const polity& q : hs.polities)
    {
        if (!q.alive || q.navy_stock <= 0) continue;
        ++s.fleets_close;
        s.navy_close += q.navy_stock;
        s.navy_close_max = std::max<int64_t>(s.navy_close_max, q.navy_stock);
    }
    s.navy_steps = hs.navy_steps_bought;
    s.navy_upkeep_spent = hs.treasury_spent_on_navy_upkeep;
    return s;
}

/// BL-1147: the fleets at one moment -- how many living polities hold one,
/// their hulls in total, the largest and the median (over those holding one),
/// the annual bill for keeping them (`navy_upkeep_per_1000_units_year_q`) and
/// the treasuries at their capitals (every living polity's).
struct fleet_read
{
    int64_t fleets = 0, navy = 0, max = 0, median = 0, bill_year = 0, treasury = 0;
};

/// BL-1147: the naval ledger at 1200 -- the living polities' raw deeds and
/// weighted points by deed; how many hold any; what the polities already dead
/// held (they carry nothing); and the living polities that LOST THEIR COAST
/// (a coastal tally, but no held region whose cell touches the sea at 1200,
/// `coast_cells`), with their points.
struct naval_read
{
    int64_t alive = 0, with_points = 0;
    int64_t raw_coast = 0, raw_crossings = 0, raw_techs = 0;
    int64_t pts_coast = 0, pts_crossings = 0, pts_techs = 0, pts_total = 0, pts_max = 0;
    int64_t dead_with_points = 0, dead_points = 0;
    int64_t lost_coast = 0, lost_coast_points = 0;
};

naval_read read_naval(const std::vector<polity>& ps, const std::vector<region>& regions, const history_sim_params& p,
                      const std::vector<terrain_substrate>* substrate = nullptr, int gw = 0, int gh = 0)
{
    naval_read n;
    std::vector<uint8_t> holds_window(ps.size(), 0); // holds a region whose cell touches the sea
    if (substrate != nullptr)
    {
        coast_cells cc;
        update_coast_cells(cc, regions, *substrate, gw, gh);
        for (std::size_t ri = 0; ri < regions.size(); ++ri)
        {
            const region& r = regions[ri];
            if (cell_touches_sea(cc, ri) && r.nation >= 0 && static_cast<std::size_t>(r.nation) < ps.size())
                holds_window[static_cast<std::size_t>(r.nation)] = 1;
        }
    }
    for (std::size_t i = 0; i < ps.size(); ++i)
    {
        const polity& q = ps[i];
        const naval_points_split sp = naval_points_of(q, p);
        if (!q.alive)
        {
            if (sp.total > 0) { ++n.dead_with_points; n.dead_points += sp.total; }
            continue;
        }
        ++n.alive;
        if (sp.total > 0) ++n.with_points;
        n.raw_coast += q.naval_coastal_years; n.raw_crossings += q.naval_crossings; n.raw_techs += q.naval_sea_techs;
        n.pts_coast += sp.coast; n.pts_crossings += sp.crossings; n.pts_techs += sp.techs; n.pts_total += sp.total;
        n.pts_max = std::max(n.pts_max, sp.total);
        if (q.naval_coastal_years > 0 && !holds_window[i]) { ++n.lost_coast; n.lost_coast_points += sp.total; }
    }
    return n;
}

fleet_read read_fleets(const std::vector<polity>& ps, const std::vector<region>* regions, int64_t rate_per_1000)
{
    fleet_read f;
    std::vector<int64_t> sizes;
    for (const polity& q : ps)
    {
        if (!q.alive) continue;
        if (regions != nullptr && q.capital >= 0 && static_cast<std::size_t>(q.capital) < regions->size())
            f.treasury += (*regions)[static_cast<std::size_t>(q.capital)].treasury;
        if (q.navy_stock <= 0) continue;
        ++f.fleets;
        f.navy += q.navy_stock;
        sizes.push_back(q.navy_stock);
        f.bill_year += (q.navy_stock * rate_per_1000) / 1000;
    }
    std::sort(sizes.begin(), sizes.end());
    if (!sizes.empty()) { f.max = sizes.back(); f.median = sizes[sizes.size() / 2]; }
    return f;
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
    std::printf("\n--- W1-W10: the field and the sim on seed %u (weight %d) ---\n", seed, weight);
    const auto t0 = std::chrono::steady_clock::now();
    world_params wp;
    wp.seed = seed;
    generation_report rep;
    era_minus_one_fixture fx;
    world_gen_config cfg = shipped.cfg;
    cfg.stop_after_industrialisation = true; // W9 reads generation's own 1660-1960 span
    const world w = make_hard_coded_world(wp, &rep, cfg, nullptr, &shipped.works, &fx);
    std::printf("      generated to the Industrialisation close in %.1f s\n", seconds_since(t0));
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
    // The price MOVES the sim: the history's sea-leg record at the weight is
    // not the record in still water (a price the sim ignored would pass every
    // row above).
    check(!same_legs(weighted_a.sea_legs, still.sea_legs)
          || weighted_a.sea_legs_noted_campaign != still.sea_legs_noted_campaign,
          "W5  the price moves the sim: the weighted run's sea-leg record differs from still water's");

    // DIRECTION AT THE CALL SITE, not only the primitive: every wet campaign is
    // priced hub -> target. A traced re-run at the weight: the alignment the sim
    // summed at launch must equal the alignment read off its own battle traces,
    // staging hub to target region, launch by launch -- a call site that priced
    // target -> hub would read the negation.
    {
        history_sim_params ep = exploration_rerun_params(fx, wq, /*trace=*/true);
        settlement_state ss = fx.pre_exploration_settlement;
        creed_state      cs = fx.pre_exploration_creeds;
        const history_sim_state traced = run_history_sim(ss, &cs, fx.terrain.view(), fx.gw, fx.gh, ep,
                                                         fx.exploration_seed, nullptr, fx.works, nullptr);
        int64_t sum = 0, with = 0, against = 0, wet = 0;
        for (const battle_trace& bt : traced.battle_traces)
        {
            if (bt.exec_dry || bt.exec_hub < 0) continue;
            if (static_cast<std::size_t>(bt.exec_hub) >= ss.regions.size() || bt.region >= ss.regions.size()) continue;
            const region& h = ss.regions[static_cast<std::size_t>(bt.exec_hub)];
            const region& t = ss.regions[bt.region];
            const int a = ocean_current_alignment_q(f, h.col, h.row, t.col, t.row);
            sum += a; ++wet;
            if (a > 0) ++with; else if (a < 0) ++against;
        }
        std::printf("      wet campaigns traced %lld: alignment summed off the traces (hub -> target) %lld,"
                    " by the sim at launch %lld; with/against %lld/%lld vs %lld/%lld\n",
                    static_cast<long long>(wet), static_cast<long long>(sum),
                    static_cast<long long>(traced.sea_campaign_alignment_sum_q),
                    static_cast<long long>(with), static_cast<long long>(against),
                    static_cast<long long>(traced.sea_campaigns_with_current),
                    static_cast<long long>(traced.sea_campaigns_against_current));
        check(wet > 0 && sum == traced.sea_campaign_alignment_sum_q
              && with == traced.sea_campaigns_with_current && against == traced.sea_campaigns_against_current,
              "W5  the campaign call site prices hub -> target: the sim's launch reading equals the traces'");
    }

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

    // W8 (BL-1140): the fourth writer, on the Exploration re-run: every
    // trade-written leg joins seats on different landmasses and its uses are
    // read against the current, the same way twice. Only trade that GOES TO
    // SEA writes (`trade_flow::by_sea`), and a span whose trade across water
    // crosses by road where two realms' ground meets writes none -- seed 32's
    // Exploration span, printed below. The positive half is read on the span
    // whose trade does go to sea, in W10's resumed Industrialisation runs.
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
        std::printf("      Exploration trade between realms on different landmasses: %lld by sea, %lld by a dry corridor\n",
                    static_cast<long long>(weighted_a.sea_trade_volume_q),
                    static_cast<long long>(weighted_a.cross_landmass_volume_by_road_q));
        std::printf("      trade across water: %lld uses on %lld legs (%lld with / %lld against / %lld slack, "
                    "mean alignment %.0f); legs joining one landmass: %lld\n",
                    static_cast<long long>(weighted_a.sea_legs_noted_trade), static_cast<long long>(trade_legs),
                    static_cast<long long>(weighted_a.sea_trade_with_current),
                    static_cast<long long>(weighted_a.sea_trade_against_current),
                    static_cast<long long>(weighted_a.sea_trade_slack_current),
                    tn > 0 ? static_cast<double>(weighted_a.sea_trade_alignment_sum_q) / tn : 0.0,
                    static_cast<long long>(same_mass));
        // Exploration's half is a REGRESSION GUARD where the span writes no
        // trade use (0 == 0 passes by construction); the label says which.
        const bool expl_trades = weighted_a.sea_legs_noted_trade > 0;
        check(same_mass == 0 && (trade_legs > 0) == expl_trades,
              expl_trades ? "W8  only trade between realms on different landmasses writes sea-leg uses (Exploration)"
                          : "W8  (guard: no trade use this span) only trade between realms on different landmasses"
                            " writes sea-leg uses (Exploration)");
        check(tn == weighted_a.sea_legs_noted_trade
              && weighted_a.sea_trade_with_current == weighted_b.sea_trade_with_current
              && weighted_a.sea_trade_alignment_sum_q == weighted_b.sea_trade_alignment_sum_q,
              expl_trades ? "W8  every trade use is read against the current, the same way twice (Exploration)"
                          : "W8  (guard: no trade use this span) every trade use is read against the current (Exploration)");
    }

    // W9 (BL-1142): generation's own Industrialisation span. Realms across
    // water met by sea (every such contact joins seats on different
    // landmasses), far pairs across water bound, and cargo was lost against
    // the current -- or, where the span's params switch a half off, it did not.
    if (fx.industrialisation_ran)
    {
        const history_sim_state& hi = fx.industrialisation_state;
        const history_sim_params& dp = fx.industrialisation_params;
        const std::vector<region>& R = fx.industrialisation_handoff.regions;
        const std::vector<int32_t> mass = landmass_labels(sub, fx.gw, fx.gh);
        int by_sea = 0, by_sea_same_mass = 0, by_sea_unrecorded = 0;
        for (const contact& k : hi.contacts)
        {
            if (k.from >= k.to || k.first.kind != contact_kind::trade) continue;
            ++by_sea;
            if (k.first.across_water != 1) ++by_sea_unrecorded;
            const auto m = [&](int pid) -> int32_t {
                const int cap = hi.polities[static_cast<std::size_t>(pid)].capital;
                if (cap < 0 || static_cast<std::size_t>(cap) >= R.size()) return -2;
                return landmass_at(mass, fx.gw, fx.gh, R[static_cast<std::size_t>(cap)].col,
                                   R[static_cast<std::size_t>(cap)].row);
            };
            // Capitals can move after the meeting; a pair whose seats now share a
            // landmass is counted and printed, not failed.
            if (m(k.from) == m(k.to)) ++by_sea_same_mass;
        }
        std::printf("      Industrialisation: met by sea %lld (in the table at 1960: %d, %d of them now on one landmass);"
                    " treaties formed across water %lld (far %lld); cargo lost against the current %lld"
                    " of %lld moved across water\n",
                    static_cast<long long>(hi.contacts_met_by_sea), by_sea, by_sea_same_mass,
                    static_cast<long long>(hi.treaties_formed_across_water),
                    static_cast<long long>(hi.far_treaties_formed_across_water),
                    static_cast<long long>(hi.sea_trade_cargo_lost_q),
                    static_cast<long long>(hi.sea_trade_volume_q));
        std::printf("      Exploration (its own span, 1200 -> 1660): cargo lost against the current %lld"
                    " of %lld moved across water; treaties formed across water %lld (far %lld)\n",
                    static_cast<long long>(fx.exploration_state.sea_trade_cargo_lost_q),
                    static_cast<long long>(fx.exploration_state.sea_trade_volume_q),
                    static_cast<long long>(fx.exploration_state.treaties_formed_across_water),
                    static_cast<long long>(fx.exploration_state.far_treaties_formed_across_water));
        check(!dp.far_pairs_meet_by_sea || hi.contacts_met_by_sea > 0,
              "W9  with meeting by sea on, realms across water meet in the Industrialisation span");
        check(by_sea_unrecorded == 0,
              "W9  every pair that met by sea recorded, at the meeting, that it met across water");
        check(dp.treaty_far_sea_penalty_q >= dp.treaty_far_penalty_q || !dp.far_pairs_meet_by_sea
              || hi.far_treaties_formed_across_water > 0,
              "W9  and far pairs across water bind");
        check(fx.exploration_params.far_pairs_meet_by_sea == false
              && fx.exploration_state.contacts_met_by_sea == 0,
              "W9  meeting by sea is the Industrialisation span's alone: Exploration meets no one by sea");
        check(dp.sea_current_cargo_loss_q <= 0 || hi.sea_trade_cargo_lost_q > 0,
              "W9  with a loss set, the Industrialisation span's trades across water lose cargo against the current");
        // The Exploration span's loss is PRINTED above, not bound: it is lost
        // only on flows the SEA carries (C8), and a span whose trade across
        // water crosses at a shared border -- a colony beside the other realm,
        // a road between them -- loses none, which is a fact of its map.
    }

    // W10 (BL-1142 review): a cargo loss outside its domain is REJECTED on the
    // real body, never clamped. Read on the Industrialisation span resumed
    // from generation's own 1660 handoff (opened as generation opens it, the
    // sweep's own resume): that is the span whose trade across water the SEA
    // carries, so generation's loss of 500 loses cargo there -- the Exploration
    // span's on this seed crosses at shared borders and loses none (W9), where
    // a rejected run and a clamped one would read alike. A clamp to 1000 would
    // lose more than 500 does; the rejected run loses none and runs exactly as
    // a loss of 0 does.
    if (fx.industrialisation_ran)
    {
        const exploration_output& eo = fx.exploration_handoff;
        const std::vector<entity_id> ids = body_tile_ids(w, fx.body, fx.gw, fx.gh);
        const auto run_loss = [&](int loss) {
            history_sim_params dp = fx.industrialisation_params;
            dp.trace_battles           = false;
            dp.resume_polities         = &eo.polities;
            dp.resume_grudges          = &eo.grudges;
            dp.resume_contacts         = &eo.contacts;
            dp.resume_corridors        = &eo.surviving_corridors;
            dp.resume_sea_legs         = &eo.sea_legs;
            dp.resume_dated_objects    = &eo.dated_objects;
            dp.resume_civilisations    = &eo.civilisations;
            dp.resume_universal_creeds = &eo.universal_creeds;
            dp.sea_current_cargo_loss_q = loss;
            settlement_state ss;
            ss.regions = eo.regions;
            survey_regions_at_span_open(w, ids, fx.gw, fx.gh, ss.regions);
            creed_state cs;
            cs.cultures = eo.cultures; // the sim reads nothing else of it
            return run_history_sim(ss, &cs, fx.terrain.view(), fx.gw, fx.gh, dp, fx.industrialisation_seed,
                                   nullptr, fx.works, nullptr);
        };
        const int gen_loss = fx.industrialisation_params.sea_current_cargo_loss_q;
        const history_sim_state gen = run_loss(gen_loss), over = run_loss(1001), zero = run_loss(0);
        // W8, the positive half: the span whose trade goes to sea writes trade
        // uses, only between realms on different landmasses, read against the
        // current the same way twice -- and in still water too.
        {
            const std::vector<int32_t> mass = landmass_labels(sub, fx.gw, fx.gh);
            const std::vector<region>& R = fx.industrialisation_handoff.regions;
            int64_t legs = 0, same = 0;
            for (const sea_leg_writer_row& r : gen.sea_leg_writers)
            {
                if (r.trade <= 0) continue;
                ++legs;
                const auto m = [&](int ri) -> int32_t {
                    if (ri < 0 || static_cast<std::size_t>(ri) >= R.size()) return -1;
                    return landmass_at(mass, fx.gw, fx.gh, R[static_cast<std::size_t>(ri)].col,
                                       R[static_cast<std::size_t>(ri)].row);
                };
                if (m(r.a) < 0 || m(r.a) == m(r.b)) ++same;
            }
            const int64_t tn_i = gen.sea_trade_with_current + gen.sea_trade_against_current + gen.sea_trade_slack_current;
            const auto run_still = [&]() {
                history_sim_params dp = fx.industrialisation_params;
                dp.trace_battles           = false;
                dp.sea_current_weight_q    = 0;
                dp.resume_polities         = &eo.polities;
                dp.resume_grudges          = &eo.grudges;
                dp.resume_contacts         = &eo.contacts;
                dp.resume_corridors        = &eo.surviving_corridors;
                dp.resume_sea_legs         = &eo.sea_legs;
                dp.resume_dated_objects    = &eo.dated_objects;
                dp.resume_civilisations    = &eo.civilisations;
                dp.resume_universal_creeds = &eo.universal_creeds;
                settlement_state ss;
                ss.regions = eo.regions;
                survey_regions_at_span_open(w, ids, fx.gw, fx.gh, ss.regions);
                creed_state cs;
                cs.cultures = eo.cultures;
                return run_history_sim(ss, &cs, fx.terrain.view(), fx.gw, fx.gh, dp, fx.industrialisation_seed,
                                       nullptr, fx.works, nullptr);
            };
            const history_sim_state still_i = run_still();
            std::printf("      Industrialisation trade by sea: %lld uses on %lld legs (%lld with / %lld against / %lld slack),"
                        " volume %lld; legs joining one landmass: %lld; in still water %lld uses\n",
                        static_cast<long long>(gen.sea_legs_noted_trade), static_cast<long long>(legs),
                        static_cast<long long>(gen.sea_trade_with_current),
                        static_cast<long long>(gen.sea_trade_against_current),
                        static_cast<long long>(gen.sea_trade_slack_current),
                        static_cast<long long>(gen.sea_trade_volume_q), static_cast<long long>(same),
                        static_cast<long long>(still_i.sea_legs_noted_trade));
            check(gen.sea_legs_noted_trade > 0 && legs > 0 && same == 0,
                  "W8  trade by sea between realms on different landmasses writes sea-leg uses, and only such trade does"
                  " (Industrialisation)");
            check(tn_i == gen.sea_legs_noted_trade
                  && gen.sea_trade_with_current == fx.industrialisation_state.sea_trade_with_current
                  && gen.sea_trade_alignment_sum_q == fx.industrialisation_state.sea_trade_alignment_sum_q,
                  "W8  every trade use is read against the current, the same way twice (Industrialisation)");
            check(still_i.sea_legs_noted_trade > 0 && still_i.sea_trade_with_current == 0,
                  "W8  the writer runs in still water too (it is the record, not the price)");
        }
        std::printf("      Industrialisation resumed from the 1660 handoff: at generation's %d cargo lost %lld"
                    " (generation's own span %lld), battles %lld (%lld); at 1001 rejected %d, lost %lld,"
                    " volume across water %lld; at 0 volume %lld\n",
                    gen_loss, static_cast<long long>(gen.sea_trade_cargo_lost_q),
                    static_cast<long long>(fx.industrialisation_state.sea_trade_cargo_lost_q),
                    static_cast<long long>(gen.battles), static_cast<long long>(fx.industrialisation_state.battles),
                    over.sea_cargo_loss_rejected ? 1 : 0, static_cast<long long>(over.sea_trade_cargo_lost_q),
                    static_cast<long long>(over.sea_trade_volume_q), static_cast<long long>(zero.sea_trade_volume_q));
        check(same_run(gen, fx.industrialisation_state)
              && gen.sea_trade_cargo_lost_q == fx.industrialisation_state.sea_trade_cargo_lost_q,
              "W10 a resume from generation's 1660 handoff reproduces generation's Industrialisation span");
        check(over.sea_cargo_loss_rejected && !zero.sea_cargo_loss_rejected && !gen.sea_cargo_loss_rejected
              && gen.sea_trade_cargo_lost_q > 0 && over.sea_trade_cargo_lost_q == 0 && same_run(over, zero)
              && over.sea_trade_volume_q == zero.sea_trade_volume_q,
              "W10 a cargo loss outside [0, 1000] is rejected whole: where 500 loses cargo, the run loses none,"
              " runs as a loss of 0 does, and says so");
    }

    // W11 (BL-1147 review): THE CORRIDOR RECORD KNOWS ITS CROSSINGS. Every row
    // of the 1200 record is marked wet exactly where its anchor-to-anchor line
    // crosses sea -- the harness's own sampling of the line (2 x radius steps,
    // the sim's `line_crosses_sea` recomputed here, not called) -- and such a
    // row offers no land line to trade.
    {
        const std::vector<region>& R = fx.pre_exploration_settlement.regions;
        const int steps = fx.params.neighbour_radius * 2;
        const auto crosses = [&](const region& a, const region& b) {
            for (int t = 1; t < steps; ++t)
            {
                const int c = a.col + (b.col - a.col) * t / steps;
                const int r = a.row + (b.row - a.row) * t / steps;
                if (c < 0 || r < 0 || c >= fx.gw || r >= fx.gh) continue;
                if (is_sea(fx.terrain.substrate[static_cast<std::size_t>(r) * static_cast<std::size_t>(fx.gw)
                                                + static_cast<std::size_t>(c)])) return true;
            }
            return false;
        };
        int64_t rows = 0, wet = 0, disagree = 0, wet_lines = 0;
        for (const history_corridor& c : fx.pre_exploration_corridors)
        {
            if (c.a >= R.size() || c.b >= R.size()) continue;
            ++rows;
            if (c.wet) ++wet;
            // Both ways: the sampler's integer steps are not symmetric, and a
            // row is an edge, not a direction.
            if ((c.wet != 0) != (crosses(R[c.a], R[c.b]) || crosses(R[c.b], R[c.a]))) ++disagree;
        }
        const trade_context ctx = build_trade_context(R, fx.pre_exploration_polities, fx.pre_exploration_corridors);
        std::vector<history_corridor> dry_only;
        for (history_corridor c : fx.pre_exploration_corridors) { c.wet = 0; dry_only.push_back(c); }
        const trade_context ctx_all = build_trade_context(R, fx.pre_exploration_polities, dry_only);
        wet_lines = static_cast<int64_t>(ctx_all.land_lines.size()) - static_cast<int64_t>(ctx.land_lines.size());
        std::printf("      the 1200 corridor record: %lld rows, %lld wet (the line crosses sea), %lld disagreeing with the"
                    " line test; land lines between realms %zu (with the wet rows read as roads: %zu)\n",
                    static_cast<long long>(rows), static_cast<long long>(wet), static_cast<long long>(disagree),
                    ctx.land_lines.size(), ctx_all.land_lines.size());
        check(rows > 0 && wet > 0 && disagree == 0,
              "W11 every corridor walked across sea is marked wet, exactly where its line crosses sea");
        check(wet_lines >= 0 && ctx.land_lines.size() <= ctx_all.land_lines.size(),
              "W11 (guard) a wet corridor adds no land line: the lines without them are a subset");
    }

    // N7-N8 (BL-1147): the naval ledger on the real body.
    {
        const std::vector<polity>& P = fx.pre_exploration_polities;
        const auto same_ledger = [](const polity& a, const polity& b) {
            return a.naval_coastal_years == b.naval_coastal_years && a.naval_crossings == b.naval_crossings
                && a.naval_sea_techs == b.naval_sea_techs;
        };
        int64_t moved = 0, coast = 0, cross = 0, techs = 0;
        for (std::size_t i = 0; i < P.size(); ++i)
        {
            coast += P[i].naval_coastal_years; cross += P[i].naval_crossings; techs += P[i].naval_sea_techs;
            if (i < fx.exploration_handoff.polities.size() && !same_ledger(P[i], fx.exploration_handoff.polities[i])) ++moved;
            if (fx.industrialisation_ran && i < fx.industrialisation_handoff.polities.size()
             && !same_ledger(P[i], fx.industrialisation_handoff.polities[i])) ++moved;
        }
        std::printf("      naval ledger at 1200: %zu polities, coastal province-years %lld, crossings %lld, sea techs %lld;"
                    " ledgers moved at the 1660 or 1960 handoff: %lld\n",
                    P.size(), static_cast<long long>(coast), static_cast<long long>(cross),
                    static_cast<long long>(techs), static_cast<long long>(moved));
        check(coast > 0 && cross > 0, "N7  the Empires round tallies naval deeds on the real body");
        check(moved == 0, "N7  (construction guard) the ledger crosses both later handoffs unchanged");

        const int64_t k = 100;
        history_sim_params ep = exploration_rerun_params(fx, wq, /*trace=*/false);
        ep.naval_points_navy_per_1000 = k;
        ep.capture_year = ep.start_year; // the OPENING state, after the conversion
        settlement_state ss = fx.pre_exploration_settlement;
        creed_state      cs = fx.pre_exploration_creeds;
        const history_sim_state hk = run_history_sim(ss, &cs, fx.terrain.view(), fx.gw, fx.gh, ep, fx.exploration_seed,
                                                     nullptr, fx.works, nullptr);
        bool exact = hk.capture.captured && hk.capture.polities.size() == P.size();
        int64_t fleets = 0, hulls = 0;
        for (std::size_t i = 0; exact && i < P.size(); ++i)
        {
            const int64_t add = naval_opening_fleet(P[i], ep);
            if (hk.capture.polities[i].navy_stock != P[i].navy_stock + add) exact = false;
            if (add > 0) { ++fleets; hulls += add; }
        }
        const naval_read n = read_naval(P, fx.pre_exploration_settlement.regions, ep, &fx.terrain.substrate, fx.gw, fx.gh);
        std::printf("      at %lld hulls per 1000 points: %lld fleets open (%lld hulls); %lld polities lost their coast"
                    " (%lld points); dead polities held %lld points; battles %lld (at 0: %lld), navy steps %lld (%lld)\n",
                    static_cast<long long>(k), static_cast<long long>(fleets), static_cast<long long>(hulls),
                    static_cast<long long>(n.lost_coast), static_cast<long long>(n.lost_coast_points),
                    static_cast<long long>(n.dead_points), static_cast<long long>(hk.battles),
                    static_cast<long long>(weighted_a.battles), static_cast<long long>(hk.navy_steps_bought),
                    static_cast<long long>(weighted_a.navy_steps_bought));
        check(exact && fleets > 0 && fleets == hk.naval_fleets_opened && hulls == hk.naval_hulls_opened
              && hk.naval_points_died == n.dead_points,
              "N8  the conversion opens every living polity at exactly its points' fleet, the dead at nothing");
        check(!same_run(hk, weighted_a) || hk.navy_steps_bought != weighted_a.navy_steps_bought
              || hk.treasury_spent_on_navy_upkeep != weighted_a.treasury_spent_on_navy_upkeep,
              "N8  (guard) the carried fleet reaches the sim: the span at a conversion above 0 is not the span at 0");
    }
    (void)known; (void)lane_tier;
}

// ---------------------------------------------------------------------------
// --sweep
// ---------------------------------------------------------------------------

/// WHAT BOUNDS TRADE ACROSS WATER, read at a span's close off its last round
/// of flows (a snapshot, so a diagnosis rather than a total): each flow whose
/// seats stand on different landmasses, filed under the term its volume meets
/// -- the buyer's want or the seller's holding as the close reads them --
/// and otherwise under the LINE THAT CARRIED IT, read off the flow's own mark
/// (`trade_flow::by_sea`, set when it was sized; never re-derived here, so the
/// census and the sim cannot disagree on which line a flow rode). Only a flow
/// the SEA line bounds is one a current can move.
struct binding_census
{
    int64_t flows = 0, by_want = 0, by_holding = 0, by_land = 0, by_sea = 0;
};

binding_census census_cross_water(const history_sim_state& hs, const std::vector<region>& regions,
                                  const ocean_current_field& field, const std::vector<int32_t>& mass,
                                  int gw, int gh, int weight)
{
    (void)field; (void)weight;
    binding_census c;
    const trade_context ctx = build_trade_context(regions, hs.polities, hs.supply_corridors);
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
        if (f.volume_q >= want && want <= hold) ++c.by_want;
        else if (f.volume_q >= hold)             ++c.by_holding;
        else                                     (f.by_sea ? ++c.by_sea : ++c.by_land);
    }
    return c;
}

struct weight_row
{
    int weight = 0;
    std::string rung; ///< BL-1142: the --rung spec this row ran, empty without one
    binding_census bind_e, bind_i; ///< what bounds trade across water at each span's close
    span_read e, i;
    lane_writers lw;     ///< lanes at the 1960 close, attributed over both spans
    lane_writers lw1660; ///< lanes at the 1660 close, attributed over Exploration's notes
    int64_t lanes_1660 = 0;
    double secs = 0.0;
    /// BL-1147: the fleets the Exploration span opened with (1200), stood at
    /// in 1300 (a capture of the traced re-run, read by nothing in the sim)
    /// and closed with (1660).
    fleet_read fleets_open, fleets_1300, fleets_1660;
    naval_read naval; ///< BL-1147: the ledger at 1200, weighted with this rung's weights
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

void print_span(const char* tag, const span_read& s);
void print_span_1142(const char* tag, const span_read& s)
{
    std::printf("    %-4s across water: pairs in contact %lld, bound %lld (far %lld) | met by sea %lld | treaties formed %lld"
                " (far %lld) | cargo lost %lld\n",
                tag, static_cast<long long>(s.cross_contacted), static_cast<long long>(s.cross_bound),
                static_cast<long long>(s.cross_far_bound), static_cast<long long>(s.met_by_sea),
                static_cast<long long>(s.treaties_across), static_cast<long long>(s.far_treaties_across),
                static_cast<long long>(s.cargo_lost));
}

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
                    "\"subjections\": %lld, \"bought\": %lld, \"freed\": %lld, \"conquests\": %lld, \"foundings\": %lld, "
                    "\"met_by_sea\": %lld, \"treaties_across\": %lld, \"far_treaties_across\": %lld, \"cargo_lost\": %lld, "
                    "\"cross_contacted\": %lld, \"cross_bound\": %lld, \"cross_far_bound\": %lld, "
                    "\"tr_volume_road\": %lld, "
                    "\"first_crossing_year\": %lld, \"fleets_close\": %lld, \"navy_close\": %lld, "
                    "\"navy_close_max\": %lld, \"navy_steps\": %lld, \"navy_upkeep_spent\": %lld}",
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
                 static_cast<long long>(s.conquests), static_cast<long long>(s.foundings),
                 static_cast<long long>(s.met_by_sea), static_cast<long long>(s.treaties_across),
                 static_cast<long long>(s.far_treaties_across), static_cast<long long>(s.cargo_lost),
                 static_cast<long long>(s.cross_contacted), static_cast<long long>(s.cross_bound),
                 static_cast<long long>(s.cross_far_bound),
                 static_cast<long long>(s.tr_volume_road),
                 static_cast<long long>(s.first_crossing_year), static_cast<long long>(s.fleets_close),
                 static_cast<long long>(s.navy_close), static_cast<long long>(s.navy_close_max),
                 static_cast<long long>(s.navy_steps), static_cast<long long>(s.navy_upkeep_spent));
}

void put_naval(std::FILE* f, const char* key, const naval_read& n)
{
    std::fprintf(f, "\"%s\": {\"alive\": %lld, \"with_points\": %lld, \"raw_coast\": %lld, \"raw_crossings\": %lld, "
                    "\"raw_techs\": %lld, \"pts_coast\": %lld, \"pts_crossings\": %lld, \"pts_techs\": %lld, "
                    "\"pts_total\": %lld, \"pts_max\": %lld, \"dead_with_points\": %lld, \"dead_points\": %lld, "
                    "\"lost_coast\": %lld, \"lost_coast_points\": %lld}",
                 key, static_cast<long long>(n.alive), static_cast<long long>(n.with_points),
                 static_cast<long long>(n.raw_coast), static_cast<long long>(n.raw_crossings),
                 static_cast<long long>(n.raw_techs), static_cast<long long>(n.pts_coast),
                 static_cast<long long>(n.pts_crossings), static_cast<long long>(n.pts_techs),
                 static_cast<long long>(n.pts_total), static_cast<long long>(n.pts_max),
                 static_cast<long long>(n.dead_with_points), static_cast<long long>(n.dead_points),
                 static_cast<long long>(n.lost_coast), static_cast<long long>(n.lost_coast_points));
}

void put_fleets(std::FILE* f, const char* key, const fleet_read& r)
{
    std::fprintf(f, "\"%s\": {\"fleets\": %lld, \"navy\": %lld, \"max\": %lld, \"median\": %lld, "
                    "\"bill_year\": %lld, \"treasury\": %lld}",
                 key, static_cast<long long>(r.fleets), static_cast<long long>(r.navy),
                 static_cast<long long>(r.max), static_cast<long long>(r.median),
                 static_cast<long long>(r.bill_year), static_cast<long long>(r.treasury));
}

// ---------------------------------------------------------------------------
// --pairs (BL-1142): who meets and who binds, across water and not, at each
// span's close, off generation's own run
// ---------------------------------------------------------------------------

struct pair_census
{
    int64_t alive_pairs = 0, cross = 0;
    int64_t contact_near[2] = {0, 0}, contact_far[2] = {0, 0}; ///< [same, cross]
    int64_t bound_near[2] = {0, 0}, bound_far[2] = {0, 0};     ///< trade_access standing
    int64_t far_unbound_cross = 0, far_cross_clears_without_penalty = 0;
    int64_t cross_uncontacted_seaworthy = 0; ///< no contact, both seats ported, a navy between them
    int64_t cross_flows = 0, cross_volume = 0;
    std::vector<int> far_cross_values; ///< min of the two sides' treaty value, unbound far cross pairs
};

pair_census census_pairs(const history_sim_state& hs, const std::vector<region>& regions,
                         const history_sim_params& p, const std::vector<int32_t>& mass, int gw, int gh,
                         const ocean_current_field& field)
{
    pair_census c;
    const std::size_t np = hs.polities.size();
    const auto mass_of = [&](int pid) -> int32_t {
        if (pid < 0 || static_cast<std::size_t>(pid) >= np) return -1;
        const int cap = hs.polities[static_cast<std::size_t>(pid)].capital;
        if (cap < 0 || static_cast<std::size_t>(cap) >= regions.size()) return -1;
        const region& r = regions[static_cast<std::size_t>(cap)];
        return landmass_at(mass, gw, gh, r.col, r.row);
    };
    std::map<std::pair<int, int>, int64_t> first_year;
    for (const contact& k : hs.contacts)
    {
        const int a = std::min<int>(k.from, k.to), b = std::max<int>(k.from, k.to);
        auto it = first_year.find({a, b});
        if (it == first_year.end() || k.first.year < it->second) first_year[{a, b}] = k.first.year;
    }
    trade_context ctx = build_trade_context(regions, hs.polities, hs.supply_corridors);
    if (!field.empty()) { ctx.currents = &field; ctx.current_weight_q = p.sea_current_weight_q; }
    for (std::size_t a = 0; a < np; ++a)
    {
        if (!hs.polities[a].alive) continue;
        for (std::size_t b = a + 1; b < np; ++b)
        {
            if (!hs.polities[b].alive) continue;
            ++c.alive_pairs;
            const int32_t ma = mass_of(static_cast<int>(a)), mb = mass_of(static_cast<int>(b));
            if (ma < 0 || mb < 0) continue;
            const int cross = ma != mb ? 1 : 0;
            if (cross) ++c.cross;
            const auto fy = first_year.find({static_cast<int>(a), static_cast<int>(b)});
            const bool bound = has_treaty_clause(hs, static_cast<int>(a), static_cast<int>(b), treaty_clause::trade_access);
            if (fy == first_year.end())
            {
                if (cross)
                {
                    const region& ra = regions[static_cast<std::size_t>(hs.polities[a].capital)];
                    const region& rb = regions[static_cast<std::size_t>(hs.polities[b].capital)];
                    if (ra.port_stock_q > 0 && rb.port_stock_q > 0
                     && (hs.polities[a].navy_stock > 0 || hs.polities[b].navy_stock > 0))
                        ++c.cross_uncontacted_seaworthy;
                }
                continue;
            }
            const bool near = fy->second < p.near_home_cutoff_year;
            if (near) { ++c.contact_near[cross]; if (bound) ++c.bound_near[cross]; }
            else      { ++c.contact_far[cross];  if (bound) ++c.bound_far[cross]; }
            if (!near && cross && !bound)
            {
                ++c.far_unbound_cross;
                const int ia = static_cast<int>(a), ib = static_cast<int>(b);
                const polity& pa = hs.polities[a];
                const polity& pb = hs.polities[b];
                const int ga = grudge_between(hs, ia, ib), gb = grudge_between(hs, ib, ia);
                const int trade = pair_trade_value_q(ctx, regions, hs.polities, ia, ib, hs.trade_flows);
                const int va = treaty_value_q(p, ga, gb, pb.treaties_broken, pa.aggression_q, 0, false, trade);
                const int vb = treaty_value_q(p, gb, ga, pa.treaties_broken, pb.aggression_q, 0, false, trade);
                c.far_cross_values.push_back(std::min(va, vb));
                const int pen = std::clamp(p.treaty_far_penalty_q, 0, 1000);
                if (std::min(va, vb) + pen >= p.treaty_formation_threshold_q) ++c.far_cross_clears_without_penalty;
            }
        }
    }
    for (const trade_flow& f : hs.trade_flows)
    {
        const int32_t ms = mass_of(f.seller), mb = mass_of(f.buyer);
        if (ms >= 0 && mb >= 0 && ms != mb) { ++c.cross_flows; c.cross_volume += f.volume_q; }
    }
    std::sort(c.far_cross_values.begin(), c.far_cross_values.end());
    return c;
}

void print_census(const char* tag, const pair_census& c)
{
    const int median = c.far_cross_values.empty() ? -1 : c.far_cross_values[c.far_cross_values.size() / 2];
    std::printf("    %s alive pairs %lld, cross-landmass %lld | contacted near same/cross %lld/%lld, far same/cross %lld/%lld"
                " | bound near same/cross %lld/%lld, far same/cross %lld/%lld | far cross unbound %lld (median value %d,"
                " clear without the far penalty %lld) | cross uncontacted with ports and a navy %lld | cross flows %lld volume %lld\n",
                tag, static_cast<long long>(c.alive_pairs), static_cast<long long>(c.cross),
                static_cast<long long>(c.contact_near[0]), static_cast<long long>(c.contact_near[1]),
                static_cast<long long>(c.contact_far[0]), static_cast<long long>(c.contact_far[1]),
                static_cast<long long>(c.bound_near[0]), static_cast<long long>(c.bound_near[1]),
                static_cast<long long>(c.bound_far[0]), static_cast<long long>(c.bound_far[1]),
                static_cast<long long>(c.far_unbound_cross), median,
                static_cast<long long>(c.far_cross_clears_without_penalty),
                static_cast<long long>(c.cross_uncontacted_seaworthy),
                static_cast<long long>(c.cross_flows), static_cast<long long>(c.cross_volume));
}

void run_pairs(shipped_inputs& shipped, const std::vector<uint32_t>& seeds)
{
    std::printf("\n=== --pairs: who meets and who binds, at 1660 and 1960 (generation's own run) ===\n");
    for (uint32_t s : seeds)
    {
        world_params wp;
        wp.seed = s;
        generation_report rep;
        era_minus_one_fixture fx;
        world_gen_config cfg = shipped.cfg;
        cfg.stop_after_industrialisation = true;
        const world w = make_hard_coded_world(wp, &rep, cfg, nullptr, &shipped.works, &fx);
        (void)w;
        if (!fx.exploration_ran || !fx.industrialisation_ran) { std::printf("  seed %u: spans did not run\n", s); continue; }
        const std::vector<int32_t> mass = landmass_labels(fx.terrain.substrate, fx.gw, fx.gh);
        const ocean_current_field field = build_ocean_currents(fx.terrain.substrate, fx.gw, fx.gh, 1);
        std::printf("  seed %u\n", s);
        print_census("1660", census_pairs(fx.exploration_state, fx.exploration_handoff.regions, fx.exploration_params,
                                          mass, fx.gw, fx.gh, field));
        print_census("1960", census_pairs(fx.industrialisation_state, fx.industrialisation_handoff.regions,
                                          fx.industrialisation_params, mass, fx.gw, fx.gh, field));
        std::fflush(stdout);
    }
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

    const std::size_t n_rungs = g_rungs.empty() ? weights.size() : g_rungs.size();
    for (std::size_t ri = 0; ri < n_rungs; ++ri)
    {
        const int wgt = g_rungs.empty() ? weights[ri] : weights.front();
        const rung_spec* rung = g_rungs.empty() ? nullptr : &g_rungs[ri];
        const auto t1 = std::chrono::steady_clock::now();
        weight_row wr;
        wr.weight = wgt;
        wr.rung = rung != nullptr ? rung->label : std::string();

        history_sim_params ep = exploration_rerun_params(fx, wgt, /*trace=*/true);
        apply_sets(ep, g_set_both); // BL-1142 measurement dials
        if (rung != nullptr) { apply_sets(ep, rung->both); apply_sets(ep, rung->expl); }
        ep.capture_year = 1300; // BL-1147: the fleets a century in (read by nothing in the sim)
        settlement_state ss = fx.pre_exploration_settlement;
        creed_state      cs = fx.pre_exploration_creeds;
        const history_sim_state he = run_history_sim(ss, &cs, fx.terrain.view(), fx.gw, fx.gh, ep,
                                                     fx.exploration_seed, nullptr, fx.works, nullptr);
        {
            const int64_t rate = ep.navy_upkeep_per_1000_units_year_q;
            // The fleets the open converts: `naval_opening_fleet` on each
            // living polity, the very function the resumed open adds (N8).
            std::vector<polity> opened = fx.pre_exploration_polities;
            for (polity& q : opened) q.navy_stock += naval_opening_fleet(q, ep);
            wr.fleets_open = read_fleets(opened, &fx.pre_exploration_settlement.regions, rate);
            wr.naval = read_naval(fx.pre_exploration_polities, fx.pre_exploration_settlement.regions, ep, &fx.terrain.substrate, fx.gw, fx.gh);
            if (he.capture.captured)
                wr.fleets_1300 = read_fleets(he.capture.polities, &he.capture.regions, rate);
            wr.fleets_1660 = read_fleets(he.polities, &ss.regions, rate);
        }
        const exploration_output eo = make_exploration_output(ss, he, &cs);
        wr.e = read_span(he, known_1200, lane_tier, &field, &ss.regions);
        wr.bind_e = census_cross_water(he, ss.regions, field, mass, fx.gw, fx.gh, wgt);
        {
            const pair_census pc = census_pairs(he, ss.regions, ep, mass, fx.gw, fx.gh, field);
            wr.e.cross_contacted = pc.contact_near[1] + pc.contact_far[1];
            wr.e.cross_bound = pc.bound_near[1] + pc.bound_far[1];
            wr.e.cross_far_bound = pc.bound_far[1];
        }
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
        apply_sets(dp, g_set_both); // BL-1142 measurement dials
        apply_sets(dp, g_set_ind);
        if (rung != nullptr) { apply_sets(dp, rung->both); apply_sets(dp, rung->ind); }
        ss.regions = eo.regions;
        survey_regions_at_span_open(w, ids, fx.gw, fx.gh, ss.regions);
        const history_sim_state hi = run_history_sim(ss, &cs, fx.terrain.view(), fx.gw, fx.gh, dp,
                                                     dseed, nullptr, fx.works, nullptr);
        wr.i  = read_span(hi, contact_set(eo.contacts), lane_tier, &field, &ss.regions);
        wr.bind_i = census_cross_water(hi, ss.regions, field, mass, fx.gw, fx.gh, wgt);
        {
            const pair_census pc = census_pairs(hi, ss.regions, dp, mass, fx.gw, fx.gh, field);
            wr.i.cross_contacted = pc.contact_near[1] + pc.contact_far[1];
            wr.i.cross_bound = pc.bound_near[1] + pc.bound_far[1];
            wr.i.cross_far_bound = pc.bound_far[1];
        }
        wr.lw = attribute_lanes(hi.sea_legs, he.sea_leg_writers, hi.sea_leg_writers, lane_tier);
        wr.secs = seconds_since(t1);

        // FIDELITY: the rung at generation's own weight must reproduce
        // generation's own untraced runs, or the sweep measured another world.
        if (wgt == fx.exploration_params.sea_current_weight_q
            && (rung == nullptr || (rung->both.empty() && rung->ind.empty() && rung->expl.empty())))
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

/// BL-1147's census (--naval): the ledger generation's own Empires round left.
void run_naval(shipped_inputs& shipped, const std::vector<uint32_t>& seeds, const std::string& out_path)
{
    std::printf("\n=== --naval: the naval ledger at 1200, %zu seeds (timings indicative on a shared machine) ===\n",
                seeds.size());
    std::FILE* f = out_path.empty() ? nullptr : std::fopen(out_path.c_str(), "wb");
    if (f) std::fprintf(f, "{\"seeds\": [\n");
    bool first = true;
    for (uint32_t seed : seeds)
    {
        const auto t0 = std::chrono::steady_clock::now();
        world_params wp;
        wp.seed = seed;
        generation_report rep;
        era_minus_one_fixture fx;
        world_gen_config cfg = shipped.cfg;
        cfg.stop_after_industrialisation = true;
        const world w = make_hard_coded_world(wp, &rep, cfg, nullptr, &shipped.works, &fx);
        (void)w;
        if (!fx.ran || !fx.exploration_ran) { std::printf("  seed %u: spans did not run\n", seed); continue; }
        const history_sim_params& ep = fx.exploration_params;
        const naval_read n = read_naval(fx.pre_exploration_polities, fx.pre_exploration_settlement.regions, ep, &fx.terrain.substrate, fx.gw, fx.gh);
        // Conservation: every polity's ledger at 1660 and 1960 is its 1200 one.
        int64_t moved_1660 = 0, moved_1960 = 0;
        const auto same_ledger = [](const polity& a, const polity& b) {
            return a.naval_coastal_years == b.naval_coastal_years && a.naval_crossings == b.naval_crossings
                && a.naval_sea_techs == b.naval_sea_techs;
        };
        for (std::size_t i = 0; i < fx.pre_exploration_polities.size(); ++i)
        {
            if (i < fx.exploration_handoff.polities.size()
             && !same_ledger(fx.pre_exploration_polities[i], fx.exploration_handoff.polities[i])) ++moved_1660;
            if (fx.industrialisation_ran && i < fx.industrialisation_handoff.polities.size()
             && !same_ledger(fx.pre_exploration_polities[i], fx.industrialisation_handoff.polities[i])) ++moved_1960;
        }
        std::printf("  seed %2u (%.0f s): %lld alive at 1200, %lld with points | raw deeds coast-years %lld,"
                    " crossings %lld, sea techs %lld | points coast %lld / crossings %lld / techs %lld = %lld"
                    " (max %lld) | dead: %lld with %lld points | lost their coast: %lld with %lld points |"
                    " ledgers moved at 1660 %lld, at 1960 %lld\n",
                    seed, seconds_since(t0), static_cast<long long>(n.alive), static_cast<long long>(n.with_points),
                    static_cast<long long>(n.raw_coast), static_cast<long long>(n.raw_crossings),
                    static_cast<long long>(n.raw_techs), static_cast<long long>(n.pts_coast),
                    static_cast<long long>(n.pts_crossings), static_cast<long long>(n.pts_techs),
                    static_cast<long long>(n.pts_total), static_cast<long long>(n.pts_max),
                    static_cast<long long>(n.dead_with_points), static_cast<long long>(n.dead_points),
                    static_cast<long long>(n.lost_coast), static_cast<long long>(n.lost_coast_points),
                    static_cast<long long>(moved_1660), static_cast<long long>(moved_1960));
        std::fflush(stdout);
        if (f)
        {
            std::fprintf(f, "%s {\"seed\": %u, ", first ? "" : ",\n", seed);
            put_naval(f, "naval", n);
            std::fprintf(f, ", \"moved_1660\": %lld, \"moved_1960\": %lld, \"polities\": [",
                         static_cast<long long>(moved_1660), static_cast<long long>(moved_1960));
            bool fp = true;
            for (const polity& q : fx.pre_exploration_polities)
            {
                if (q.naval_coastal_years == 0 && q.naval_crossings == 0 && q.naval_sea_techs == 0) continue;
                std::fprintf(f, "%s[%d, %d, %lld, %lld, %lld]", fp ? "" : ", ", q.id, q.alive ? 1 : 0,
                             static_cast<long long>(q.naval_coastal_years), static_cast<long long>(q.naval_crossings),
                             static_cast<long long>(q.naval_sea_techs));
                fp = false;
            }
            std::fprintf(f, "]}");
            first = false;
        }
    }
    if (f) { std::fprintf(f, "\n]}\n"); std::fclose(f); }
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
                if (!wr.rung.empty()) std::printf("   RUNG %s\n", wr.rung.c_str());
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
                print_span_1142("  E", wr.e);
                print_span_1142("  I", wr.i);
                const auto fl = [](const fleet_read& f) {
                    char b[160];
                    std::snprintf(b, sizeof b, "%lld fleets, %lld hulls (max %lld, median %lld), bill %lld/yr of treasury %lld",
                                  static_cast<long long>(f.fleets), static_cast<long long>(f.navy),
                                  static_cast<long long>(f.max), static_cast<long long>(f.median),
                                  static_cast<long long>(f.bill_year), static_cast<long long>(f.treasury));
                    return std::string(b);
                };
                std::printf("        fleets: open %s | 1300 %s | 1660 %s | 1960 %lld fleets, %lld hulls;"
                            " first crossing E %lld, I %lld; navy steps E %lld I %lld\n",
                            fl(wr.fleets_open).c_str(), fl(wr.fleets_1300).c_str(), fl(wr.fleets_1660).c_str(),
                            static_cast<long long>(wr.i.fleets_close), static_cast<long long>(wr.i.navy_close),
                            static_cast<long long>(wr.e.first_crossing_year),
                            static_cast<long long>(wr.i.first_crossing_year),
                            static_cast<long long>(wr.e.navy_steps), static_cast<long long>(wr.i.navy_steps));
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
                std::fprintf(f, "   {\"rung\": \"%s\", ", wr.rung.c_str());
                std::fprintf(f, "\"weight\": %d, \"secs\": %.1f, \"lanes_1660\": %lld, \"lanes_1960\": %lld, "
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
                put_fleets(f, "fleets_open", wr.fleets_open); std::fprintf(f, ", ");
                put_naval(f, "naval", wr.naval); std::fprintf(f, ", ");
                put_fleets(f, "fleets_1300", wr.fleets_1300); std::fprintf(f, ", ");
                put_fleets(f, "fleets_1660", wr.fleets_1660); std::fprintf(f, ", ");
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
    bool sweep = false, picture = false, pairs = false, explicit_weights = false, synthetic_only = false;
    bool naval = false;
    std::vector<uint32_t> seeds(std::begin(k_library), std::end(k_library));
    std::vector<int> weights = {0, 150, 300, 500, 700, 900};
    std::string out_path, png_path = "ocean_currents.png";
    uint32_t seed = 32;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--sweep") sweep = true;
        else if (a == "--pairs") pairs = true;
        else if (a == "--synthetic") synthetic_only = true;
        else if (a == "--naval") naval = true;
        else if (a == "--picture") picture = true;
        else if (a == "--seeds" && i + 1 < argc)
        {
            seeds.clear();
            for (int x : parse_ints(argv[++i])) seeds.push_back(static_cast<uint32_t>(x));
        }
        else if (a == "--weights" && i + 1 < argc) { weights = parse_ints(argv[++i]); explicit_weights = true; }
        else if (a == "--out" && i + 1 < argc) out_path = argv[++i];
        else if (a == "--png" && i + 1 < argc) png_path = argv[++i];
        else if (a == "--seed" && i + 1 < argc) seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        else if (a == "--rung" && i + 1 < argc)
        {
            rung_spec r;
            if (!parse_rung(argv[++i], r)) { std::printf("bad --rung %s\n", argv[i]); return 2; }
            g_rungs.push_back(r);
        }
        else if ((a == "--set" || a == "--set-i") && i + 1 < argc)
        {
            const std::string kv = argv[++i];
            const std::size_t eq = kv.find('=');
            history_sim_params probe;
            if (eq == std::string::npos || !apply_set(probe, kv.substr(0, eq), std::atoi(kv.c_str() + eq + 1)))
            { std::printf("unknown --set %s\n", kv.c_str()); return 2; }
            (a == "--set" ? g_set_both : g_set_ind).push_back({kv.substr(0, eq), std::atoi(kv.c_str() + eq + 1)});
        }
        else { std::printf("unknown argument %s\n", a.c_str()); return 2; }
    }

    if (synthetic_only)
    {
        synthetic_rows();
        far_pair_rows();
        naval_rows();
        std::printf("\n%s: %d failure(s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
        return g_failures == 0 ? 0 : 1;
    }

    shipped_inputs shipped;
    load_shipped_inputs(shipped); // every mode generates at least one real world

    if (picture)
    {
        run_picture(shipped, seed, png_path);
        return 0;
    }
    if (pairs)
    {
        run_pairs(shipped, seeds);
        return 0;
    }
    if (naval)
    {
        run_naval(shipped, seeds, out_path);
        return 0;
    }
    if (sweep)
    {
        // Still water rides the default ladder; an explicit --weights list is taken as given.
        if (!explicit_weights && std::find(weights.begin(), weights.end(), 0) == weights.end()) weights.insert(weights.begin(), 0);
        // Generation's own weight always rides the ladder: it is the rung the
        // fidelity check reads.
        const int gen_w = exploration_sim_params(world_params{}).sea_current_weight_q;
        if (std::find(weights.begin(), weights.end(), gen_w) == weights.end()) weights.push_back(gen_w);
        run_sweep(shipped, seeds, weights, out_path);
        return 0;
    }

    synthetic_rows();
    far_pair_rows();
    naval_rows();
    const int shipped_weight = exploration_sim_params(world_params{}).sea_current_weight_q;
    real_body_rows(shipped, seed, shipped_weight > 0 ? shipped_weight : 300);
    std::printf("\n%s: %d failure(s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
