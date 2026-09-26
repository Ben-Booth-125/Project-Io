// ---------------------------------------------------------------------------
// sea_lane_stamp_harness — BL-1098 (sea lanes are stamped on the water, bending
// with the current).
//
// LOGISTICS.md § 4b; EXPLORATION.md § The colonial tie is a sea lane and
// § Currents are a force ("Where currents bite": the lane stamp's water walker
// prices its path with the current). src/world/road_generation.hpp carries the
// stamp's design.
//
//   S1  the walker is water-only: round a concave coast the straight line
//       crosses land and the walk does not; every step is to an adjacent sea
//       tile and no step cuts a land corner
//   S2  the walker prices its steps with the current: across a basin the
//       priced walk leaves the still-water one, and the water it crosses runs
//       more with it; the walk is deterministic
//   S3  a seat's port is its nearest sea tile within the radius, ties low;
//       an inland seat beyond it has none
//   W1  on a generated 1960 world lanes are stamped (count reported), only on
//       sea tiles, and no road lies on water (the road lens has none to draw)
//   W2  re-stamping the world's own lane record reproduces its lane field
//       exactly (the stamp is a pure function of the record and the ground)
//   W3  the A* cost between two lane-joined ports falls below the unlaned cost
//   W4  reach crosses the lane: tiles whose reach cost falls with the lanes,
//       and tiles the shipped reach budget (24) takes in only because of them
//   W5  a save round-trips lane_level, tile by tile
//   B   how far each lane bends: mean lateral offset of its path from the
//       straight line port to port (tiles), the still-water walk's own offset,
//       and the mean separation between the two walks -- per seed, reported
//
// --picture SEED --png PATH  draws the body's ground, the current field (one
//   arrow per ocean region) and the stamped lanes.
//
// Build (the shipped data layer, so a live Lua state):
//   bash tools/verify/build_lua_harness.sh sea_lane_stamp_harness
// Run from the repo root. Default seeds 32 and 46 (--seeds a,b).
// ---------------------------------------------------------------------------

#include "world/road_generation.hpp"
#include "world/ocean_currents.hpp"

#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/logistics.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"
#include "world/world_save.hpp"
#include "world/works_roster.hpp"
#include "scripting/lua_state.hpp"

// The dependency-free PNG writer, compiled in (neither headless builder links src/core).
#include "core/png_writer.cpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <sstream>
#include <string>
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

/// Chebyshev distance with columns wrapping.
int cheb(int a, int b, int gw)
{
    const int ac = a % gw, ar = a / gw, bc = b % gw, br = b / gw;
    int dc = std::abs(ac - bc);
    if (dc > gw / 2) dc = gw - dc;
    return std::max(dc, std::abs(ar - br));
}

/// True iff consecutive tiles are eight-neighbours, all are sea, and no diagonal
/// step cuts a land corner.
bool walk_is_honest(const std::vector<int>& path, const std::vector<uint8_t>& sea, int gw)
{
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        if (!sea[static_cast<std::size_t>(path[i])]) return false;
        if (i == 0) continue;
        const int a = path[i - 1], b = path[i];
        if (cheb(a, b, gw) != 1) return false;
        const int ac = a % gw, ar = a / gw, bc = b % gw, br = b / gw;
        if (ac != bc && ar != br)
        {
            const int side_a = ar * gw + bc, side_b = br * gw + ac;
            if (!sea[static_cast<std::size_t>(side_a)] && !sea[static_cast<std::size_t>(side_b)]) return false;
        }
    }
    return true;
}

/// Mean perpendicular distance (tiles) of @p path from the straight segment
/// joining its two ends, columns unwrapped the short way from the start.
double mean_lateral_offset(const std::vector<int>& path, int gw)
{
    if (path.size() < 2) return 0.0;
    const int c0 = path.front() % gw, r0 = path.front() / gw;
    auto unwrap = [&](int c) { int d = c - c0; if (d > gw / 2) d -= gw; if (d < -gw / 2) d += gw; return d; };
    const double sx = unwrap(path.back() % gw), sy = (path.back() / gw) - r0;
    const double len = std::sqrt(sx * sx + sy * sy);
    if (len <= 0.0) return 0.0;
    double sum = 0.0;
    for (const int t : path)
    {
        const double x = unwrap(t % gw), y = (t / gw) - r0;
        sum += std::fabs(sx * y - sy * x) / len;
    }
    return sum / static_cast<double>(path.size());
}

/// Mean, over @p a's tiles, of the Chebyshev distance to the nearest tile of @p b.
double mean_separation(const std::vector<int>& a, const std::vector<int>& b, int gw)
{
    if (a.empty() || b.empty()) return 0.0;
    double sum = 0.0;
    for (const int t : a)
    {
        int best = std::numeric_limits<int>::max();
        for (const int u : b) best = std::min(best, cheb(t, u, gw));
        sum += best;
    }
    return sum / static_cast<double>(a.size());
}

/// The mean current along a walk's own steps (per mille), read off @p f.
double mean_step_alignment(const std::vector<int>& path, const ocean_current_field& f, int gw)
{
    if (path.size() < 2) return 0.0;
    double sum = 0.0;
    for (std::size_t i = 1; i < path.size(); ++i)
    {
        const int a = path[i - 1], b = path[i];
        int dc = (b % gw) - (a % gw);
        if (dc > 1) dc -= gw;
        if (dc < -1) dc += gw;
        const int dr = (b / gw) - (a / gw);
        const std::size_t k = static_cast<std::size_t>(f.region_of(b % gw, b / gw));
        const double len = (dc != 0 && dr != 0) ? std::sqrt(2.0) : 1.0;
        sum += (f.east_q[k] * dc + f.north_q[k] * (-dr)) / len;
    }
    return sum / static_cast<double>(path.size() - 1);
}

// ---------------------------------------------------------------------------
// S1-S3: synthetic ground
// ---------------------------------------------------------------------------

void synthetic_rows()
{
    std::printf("\n--- S1-S3: the walker on synthetic ground ---\n");
    // S1: a bay whose mouth faces north. Sea everywhere except a U of land:
    // a bar along row 20 from col 10 to 30, and two arms down cols 10 and 30
    // from row 5 to 20. Inside the U (cols 11-29, rows 5-19) is a bay open to
    // the north; the straight line from inside the bay (20, 15) to the sea
    // south of the bar (20, 25) crosses the bar.
    const int gw = 60, gh = 40;
    std::vector<uint8_t> sea(static_cast<std::size_t>(gw) * gh, 1);
    for (int c = 10; c <= 30; ++c) sea[static_cast<std::size_t>(20) * gw + c] = 0;
    for (int r = 5; r <= 20; ++r) { sea[static_cast<std::size_t>(r) * gw + 10] = 0; sea[static_cast<std::size_t>(r) * gw + 30] = 0; }
    const int from = 15 * gw + 20, to = 25 * gw + 20;
    bool straight_crosses_land = false;
    for (int r = 15; r <= 25; ++r) if (!sea[static_cast<std::size_t>(r) * gw + 20]) straight_crosses_land = true;
    const std::vector<int> walk = sea_lane_walk(sea, gw, gh, nullptr, 0, from, to);
    int lowest_row = 99;
    for (const int t : walk) lowest_row = std::min(lowest_row, t / gw);
    std::printf("      concave bay: straight line crosses land %s; walk %zu tiles, reaches row %d (the bay mouth is row 4)\n",
                straight_crosses_land ? "yes" : "no", walk.size(), lowest_row);
    check(straight_crosses_land && !walk.empty() && walk.front() == from && walk.back() == to
          && walk_is_honest(walk, sea, gw) && lowest_row <= 4,
          "S1  round a concave coast the walk leaves by the bay mouth, water-only, never cutting a land corner");
    // A corner the water may not squeeze through: two land tiles touching diagonally.
    {
        std::vector<uint8_t> s2(static_cast<std::size_t>(10) * 10, 1);
        for (int r = 0; r < 10; ++r) for (int c = 0; c < 10; ++c)
            if ((r < 5 && c == 5) || (r >= 5 && c == 4)) s2[static_cast<std::size_t>(r) * 10 + c] = 0; // a wall with a corner joint at (4,5)/(5,4)
        // Rows 0-9 walled except where the two halves meet diagonally; no column wrap
        // path around because the wall spans every row. The walk must fail.
        const std::vector<int> w2 = sea_lane_walk(s2, 10, 10, nullptr, 0, 2 * 10 + 2, 7 * 10 + 7);
        // With wrap, the sea is joined round the cylinder's seam, so allow that: the
        // walk must then not use the corner step.
        bool used_corner = false;
        for (std::size_t i = 1; i < w2.size(); ++i)
        {
            const int a = w2[i - 1], b = w2[i];
            if ((a == 4 * 10 + 4 && b == 5 * 10 + 5) || (a == 5 * 10 + 5 && b == 4 * 10 + 4)) used_corner = true;
        }
        check(!used_corner && walk_is_honest(w2, s2, 10),
              "S1  the water never squeezes through a joint where two land tiles touch at a corner");
    }

    // S2: the two-continent basin the currents harness uses (land cols [0,7) and
    // [42,49) on an 84 x 61 body). A lane across basin A through the northern gyre.
    {
        const int bw = 84, bh = 61;
        std::vector<terrain_substrate> sub(static_cast<std::size_t>(bw) * bh, terrain_substrate::ocean);
        std::vector<uint8_t> bsea(static_cast<std::size_t>(bw) * bh, 1);
        for (int r = 0; r < bh; ++r)
            for (int c = 0; c < bw; ++c)
                if (c < 7 || (c >= 42 && c < 49))
                {
                    sub[static_cast<std::size_t>(r) * bw + c] = terrain_substrate::sedimentary;
                    bsea[static_cast<std::size_t>(r) * bw + c] = 0;
                }
        const ocean_current_field f = build_ocean_currents(sub, bw, bh, 1);
        // From the western shore at the trades' latitude to the eastern shore at the
        // westerlies': a still-water walk runs the diagonal; the priced one should ride
        // the western boundary current north and the westerlies east.
        const int a = 26 * bw + 7, b = 14 * bw + 41;
        const std::vector<int> still  = sea_lane_walk(bsea, bw, bh, nullptr, 0, a, b);
        const std::vector<int> priced = sea_lane_walk(bsea, bw, bh, &f, 500, a, b);
        const std::vector<int> again  = sea_lane_walk(bsea, bw, bh, &f, 500, a, b);
        const std::vector<int> back   = sea_lane_walk(bsea, bw, bh, &f, 500, b, a);
        const double sep = mean_separation(priced, still, bw);
        const double al_still = mean_step_alignment(still, f, bw), al_priced = mean_step_alignment(priced, f, bw);
        std::printf("      basin A, west shore (7,26) -> east shore (41,14): still %zu tiles, priced %zu; separation %.2f tiles;"
                    " mean current along the steps %.0f still, %.0f priced; the return walk %zu tiles, separation %.2f\n",
                    still.size(), priced.size(), sep, al_still, al_priced, back.size(),
                    mean_separation(back, priced, bw));
        check(!priced.empty() && walk_is_honest(priced, bsea, bw) && priced == again,
              "S2  the priced walk is water-only and deterministic");
        check(sep > 0.5 && al_priced > al_still,
              "S2  the current bends the walk: it leaves the still-water line for water that runs with it");
        check(!back.empty() && mean_separation(back, priced, bw) > 0.5,
              "S2  a basin circulates: the return walk takes other water than the outbound");
    }

    // S3: ports.
    {
        std::vector<uint8_t> s3(static_cast<std::size_t>(20) * 20, 0);
        s3[static_cast<std::size_t>(10) * 20 + 14] = 1; // sea at (14,10)
        s3[static_cast<std::size_t>(6) * 20 + 10] = 1;  // sea at (10,6): same ring (4) from (10,10), lower index
        const int p = sea_lane_port(s3, 20, 20, 10, 10, 9);
        check(p == 6 * 20 + 10, "S3  a port is the nearest sea tile, ties to the lower raster index");
        check(sea_lane_port(s3, 20, 20, 10, 10, 3) == -1, "S3  a seat with no sea within the radius has no port");
        check(sea_lane_port(s3, 20, 20, 14, 10, 9) == 10 * 20 + 14, "S3  a seat on the sea is its own port");
    }
}

// ---------------------------------------------------------------------------
// The shipped data layer
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
    std::printf("generation inputs: scripts/world_gen.lua + scripts/works.lua (%zu works rows)\n", in.works.size());
}

struct lane_world
{
    world w;
    era_minus_one_fixture fx;
    entity_id body = null_entity;
    int gw = 0, gh = 0;
    std::vector<sea_leg> legs;
    std::vector<history_road_node> nodes;
    std::vector<uint8_t> sea;
    std::vector<terrain_substrate> sub;
    std::vector<entity_id> grid;
};

bool build_lane_world(shipped_inputs& shipped, uint32_t seed, lane_world& L)
{
    world_params wp;
    wp.seed = seed;
    generation_report rep;
    L.w = make_hard_coded_world(wp, &rep, shipped.cfg, nullptr, &shipped.works, &L.fx);
    if (!L.fx.ran || !L.fx.exploration_ran) return false;
    L.body = L.fx.body;
    L.gw = L.fx.gw;
    L.gh = L.fx.gh;
    L.legs = L.fx.industrialisation_ran ? L.fx.industrialisation_handoff.sea_legs : L.fx.exploration_handoff.sea_legs;
    if (L.w.gen_settlement)
        for (const region& p : L.w.gen_settlement->regions)
            L.nodes.push_back(history_road_node{ p.col, p.row, p.work_reach_mod });
    L.grid = body_tile_grid(L.w, L.body);
    L.sea.assign(static_cast<std::size_t>(L.gw) * L.gh, 0);
    L.sub.assign(static_cast<std::size_t>(L.gw) * L.gh, terrain_substrate::sedimentary);
    for (int i = 0; i < L.gw * L.gh; ++i)
    {
        const auto it = L.w.tiles.find(L.grid[static_cast<std::size_t>(i)]);
        if (it == L.w.tiles.end()) continue;
        L.sub[static_cast<std::size_t>(i)] = it->second.substrate;
        L.sea[static_cast<std::size_t>(i)] = is_sea(it->second.substrate) ? 1 : 0;
    }
    return true;
}

std::vector<uint8_t> lane_field(const lane_world& L)
{
    std::vector<uint8_t> f(static_cast<std::size_t>(L.gw) * L.gh, 0);
    for (int i = 0; i < L.gw * L.gh; ++i)
    {
        const auto it = L.w.tiles.find(L.grid[static_cast<std::size_t>(i)]);
        if (it != L.w.tiles.end()) f[static_cast<std::size_t>(i)] = it->second.lane_level;
    }
    return f;
}

void set_lane_field(lane_world& L, const std::vector<uint8_t>& f)
{
    for (int i = 0; i < L.gw * L.gh; ++i)
    {
        const auto it = L.w.tiles.find(L.grid[static_cast<std::size_t>(i)]);
        if (it != L.w.tiles.end()) it->second.lane_level = f[static_cast<std::size_t>(i)];
    }
    invalidate_logistics_caches(L.w);
}

// ---------------------------------------------------------------------------
// W1-W5 and B, per seed
// ---------------------------------------------------------------------------

void world_rows(shipped_inputs& shipped, uint32_t seed)
{
    std::printf("\n--- W1-W5, B: seed %u ---\n", seed);
    const auto t0 = std::chrono::steady_clock::now();
    lane_world L;
    if (!build_lane_world(shipped, seed, L))
    {
        check(false, "W0  the seed ran its spans");
        return;
    }
    std::printf("      generated the whole world in %.0f s (timings indicative on a shared machine)\n", seconds_since(t0));
    const history_sim_params lp = exploration_sim_params(world_params{});
    const std::vector<uint8_t> generated = lane_field(L);

    // W1
    int lane_tiles = 0, lane_on_land = 0, road_on_water = 0;
    for (int i = 0; i < L.gw * L.gh; ++i)
    {
        const auto it = L.w.tiles.find(L.grid[static_cast<std::size_t>(i)]);
        if (it == L.w.tiles.end()) continue;
        if (it->second.lane_level > 0) { ++lane_tiles; if (!is_sea(it->second.substrate)) ++lane_on_land; }
        if (is_water(it->second.substrate) && it->second.road_level > 0) ++road_on_water;
    }
    int earned = 0;
    for (const sea_leg& l : L.legs) if (l.uses >= lp.sea_lane_tier1_uses) ++earned;
    std::printf("      lane record: %zu legs, %d earned a lane; lane tiles on the 1960 world: %d\n",
                L.legs.size(), earned, lane_tiles);
    check(lane_on_land == 0, "W1  lanes lie on sea tiles only");
    check(road_on_water == 0, "W1  no road lies on water, so the road lens has none to draw there");

    // W2: re-stamp from the record on cleared water.
    set_lane_field(L, std::vector<uint8_t>(generated.size(), 0));
    sea_lane_stats st;
    sea_lane_trace tr;
    stamp_sea_lanes(L.w, L.body, L.nodes, L.legs, lp.sea_lane_tier1_uses, lp.sea_current_weight_q,
                    lp.sea_current_rotation_sense, &st, &tr);
    const std::vector<uint8_t> restamped = lane_field(L);
    std::printf("      stamp: earned %d, laid %d, no port %d, unreachable %d; %lld path tiles, %d lane tiles\n",
                st.earned, st.laid, st.no_port, st.unreachable, st.path_tiles, st.lane_tiles);
    check(restamped == generated && st.lane_tiles == lane_tiles,
          "W2  re-stamping the world's own lane record reproduces its lane field exactly");
    // A world whose record earned no lane with a port at both ends legitimately
    // stamps none; one that laid any carries lane tiles on its 1960 map.
    check((lane_tiles > 0) == (st.laid > 0),
          "W1  lanes are stamped on the 1960 world exactly where the record earned one with ports at both ends");
    bool honest = true;
    for (const sea_lane_trace::lane& ln : tr.lanes) if (!walk_is_honest(ln.path, L.sea, L.gw)) honest = false;
    check(honest && st.laid == static_cast<int>(tr.lanes.size()), "W2  every laid lane is a water-only walk");

    // W3: A* across the longest lane, with and without.
    if (!tr.lanes.empty())
    {
        const sea_lane_trace::lane* longest = &tr.lanes.front();
        for (const sea_lane_trace::lane& ln : tr.lanes) if (ln.path.size() > longest->path.size()) longest = &ln;
        const entity_id pa = L.grid[static_cast<std::size_t>(longest->from_port)];
        const entity_id pb = L.grid[static_cast<std::size_t>(longest->to_port)];
        invalidate_logistics_caches(L.w);
        const float with_lane = intra_body_path(L.w, L.body, pa, pb).cost;
        const std::vector<float> reach_with = body_reach_field(L.w, L.body);
        set_lane_field(L, std::vector<uint8_t>(generated.size(), 0));
        const float without = intra_body_path(L.w, L.body, pa, pb).cost;
        const std::vector<float> reach_without = body_reach_field(L.w, L.body);
        set_lane_field(L, generated);
        std::printf("      A* between the longest lane's ports (%zu-tile lane, %d uses): %.2f with the lane, %.2f without\n",
                    longest->path.size(), longest->uses, with_lane, without);
        check(with_lane < without, "W3  the A* cost between two lane-joined ports falls below the unlaned cost");

        // W4: reach.
        constexpr float kBudget = 24.0f; // scripts/economy.lua construction.max_logistics_reach
        int fell = 0, newly = 0, fell_land = 0;
        for (std::size_t i = 0; i < reach_with.size() && i < reach_without.size(); ++i)
        {
            if (reach_with[i] < reach_without[i])
            {
                ++fell;
                if (!L.sea[i]) ++fell_land;
                if (reach_with[i] <= kBudget && !(reach_without[i] <= kBudget)) ++newly;
            }
        }
        std::printf("      reach: %d tiles cost less to supply with the lanes (%d of them land); %d come inside the"
                    " shipped budget (%.0f) only because of them\n", fell, fell_land, newly, kBudget);
        check(fell > 0, "W4  reach crosses the lane: tiles beyond it cost less to supply once it is laid");
    }

    // W5: save round trip.
    {
        std::stringstream buf(std::ios::in | std::ios::out | std::ios::binary);
        write_world_snapshot(L.w, buf);
        world back;
        buf.seekg(0);
        const bool read = read_world_snapshot(back, buf);
        int same = 0, differ = 0, lanes_back = 0;
        for (const auto& [tid, tc] : L.w.tiles)
        {
            const auto it = back.tiles.find(tid);
            if (it == back.tiles.end() || it->second.lane_level != tc.lane_level) ++differ;
            else { ++same; if (tc.lane_level > 0) ++lanes_back; }
        }
        std::printf("      save round trip at world_save_version %u: read %s, %d tiles equal, %d differ, %d lane tiles back\n",
                    static_cast<unsigned>(world_save_version), read ? "ok" : "REFUSED", same, differ, lanes_back);
        check(read && differ == 0 && lanes_back == lane_tiles, "W5  a save round-trips lane_level, tile by tile");
    }

    // B: the bend.
    {
        const ocean_current_field f = build_ocean_currents(L.sub, L.gw, L.gh, lp.sea_current_rotation_sense);
        double off_priced = 0.0, off_still = 0.0, sep = 0.0, al_p = 0.0, al_s = 0.0;
        int n = 0, bent = 0;
        std::printf("      lane  uses  tiles  offset(priced)  offset(still)  separation  current along priced/still\n");
        for (const sea_lane_trace::lane& ln : tr.lanes)
        {
            if (ln.path.size() < 2) continue;
            const std::vector<int> still = sea_lane_walk(L.sea, L.gw, L.gh, nullptr, 0, ln.from_port, ln.to_port);
            const double op = mean_lateral_offset(ln.path, L.gw), os = mean_lateral_offset(still, L.gw);
            const double s = mean_separation(ln.path, still, L.gw);
            const double ap = mean_step_alignment(ln.path, f, L.gw), as = mean_step_alignment(still, f, L.gw);
            off_priced += op; off_still += os; sep += s; al_p += ap; al_s += as; ++n;
            if (s > 0.0) ++bent;
            std::printf("      %4d-%-4d %4d  %5zu  %14.2f  %13.2f  %10.2f  %6.0f / %6.0f\n",
                        ln.a, ln.b, ln.uses, ln.path.size(), op, os, s, ap, as);
        }
        if (n > 0)
            std::printf("      MEAN over %d lanes: lateral offset from the straight line %.2f tiles priced, %.2f still water;"
                        " separation priced vs still %.2f tiles (%d of %d lanes moved); current along the steps %.0f priced,"
                        " %.0f still\n", n, off_priced / n, off_still / n, sep / n, bent, n, al_p / n, al_s / n);
        check(n == 0 || al_p / n >= al_s / n,
              "B   the lanes run with more current than the still-water walks between the same ports");
    }
}

// ---------------------------------------------------------------------------
// --picture
// ---------------------------------------------------------------------------

void put_px(std::vector<unsigned char>& img, int W, int H, int x, int y, unsigned char r, unsigned char g, unsigned char b)
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
            for (int dx = -thick / 2; dx <= thick / 2; ++dx) put_px(img, W, H, x + dx, y + dy, r, g, b);
    }
}

void run_picture(shipped_inputs& shipped, uint32_t seed, const std::string& path)
{
    lane_world L;
    if (!build_lane_world(shipped, seed, L)) { std::printf("seed %u: spans did not run\n", seed); return; }
    const history_sim_params lp = exploration_sim_params(world_params{});
    const ocean_current_field f = build_ocean_currents(L.sub, L.gw, L.gh, lp.sea_current_rotation_sense);
    const int S = 6, W = L.gw * S, H = L.gh * S;
    std::vector<unsigned char> img(static_cast<std::size_t>(W) * H * 4, 255);
    for (int r = 0; r < L.gh; ++r)
        for (int c = 0; c < L.gw; ++c)
        {
            const terrain_substrate s = L.sub[static_cast<std::size_t>(r) * L.gw + c];
            unsigned char cr, cg, cb;
            if (s == terrain_substrate::ocean)      { cr = 24;  cg = 52;  cb = 96;  }
            else if (s == terrain_substrate::coast) { cr = 44;  cg = 84;  cb = 132; }
            else if (s == terrain_substrate::lake)  { cr = 90;  cg = 140; cb = 180; }
            else                                    { cr = 150; cg = 138; cb = 104; }
            for (int y = 0; y < S; ++y) for (int x = 0; x < S; ++x) put_px(img, W, H, c * S + x, r * S + y, cr, cg, cb);
        }
    // Faint arrows: the current field.
    const double cell_px = f.cell * S;
    for (int cy = 0; cy < f.cells_h; ++cy)
        for (int cx = 0; cx < f.cells_w; ++cx)
        {
            const std::size_t k = static_cast<std::size_t>(cy * f.cells_w + cx);
            if (f.sea_tiles[k] == 0) continue;
            const double e = f.east_q[k] / 1000.0, n = f.north_q[k] / 1000.0;
            const double mag = std::sqrt(e * e + n * n);
            if (mag < 0.02) continue;
            const double m = std::min(1.0, mag), L2 = cell_px * (0.2 + 0.3 * m);
            const double ux = e / mag, uy = -n / mag;
            const double x0 = (cx + 0.5) * cell_px, y0 = (cy + 0.5) * cell_px;
            const double x1 = x0 + ux * L2 * 0.5, y1 = y0 + uy * L2 * 0.5;
            draw_line(img, W, H, x0 - ux * L2 * 0.5, y0 - uy * L2 * 0.5, x1, y1, 120, 150, 190, 1);
            const double a = 0.5, h = std::max(4.0, L2 * 0.3);
            draw_line(img, W, H, x1, y1, x1 + (-ux * std::cos(a) + uy * std::sin(a)) * h,
                      y1 + (-ux * std::sin(a) - uy * std::cos(a)) * h, 120, 150, 190, 1);
            draw_line(img, W, H, x1, y1, x1 + (-ux * std::cos(-a) + uy * std::sin(-a)) * h,
                      y1 + (-ux * std::sin(-a) - uy * std::cos(-a)) * h, 120, 150, 190, 1);
        }
    // The stamped lanes, tile by tile, bright.
    int lanes = 0;
    for (int i = 0; i < L.gw * L.gh; ++i)
    {
        const auto it = L.w.tiles.find(L.grid[static_cast<std::size_t>(i)]);
        if (it == L.w.tiles.end() || it->second.lane_level == 0) continue;
        ++lanes;
        const int c = i % L.gw, r = i / L.gw;
        for (int y = 1; y < S - 1; ++y) for (int x = 1; x < S - 1; ++x) put_px(img, W, H, c * S + x, r * S + y, 255, 200, 40);
    }
    // The seats the lanes join, as white squares.
    for (const sea_leg& l : L.legs)
    {
        if (l.uses < lp.sea_lane_tier1_uses) continue;
        for (const int ri : { static_cast<int>(l.a), static_cast<int>(l.b) })
        {
            if (ri < 0 || static_cast<std::size_t>(ri) >= L.nodes.size()) continue;
            const int c = L.nodes[static_cast<std::size_t>(ri)].col, r = L.nodes[static_cast<std::size_t>(ri)].row;
            for (int y = -S; y < 2 * S; ++y) for (int x = -S; x < 2 * S; ++x)
                if (y == -S || y == 2 * S - 1 || x == -S || x == 2 * S - 1) put_px(img, W, H, c * S + x, r * S + y, 255, 255, 255);
        }
    }
    const bool ok = write_png_rgba(path, W, H, img.data(), W * 4);
    std::printf("%s seed %u's stamped lanes (%d lane tiles, gold) over its current field (%dx%d px) to %s\n",
                ok ? "wrote" : "FAILED to write", seed, lanes, W, H, path.c_str());
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
    std::vector<uint32_t> seeds = {32, 46};
    bool picture = false;
    uint32_t picture_seed = 32;
    std::string png = "sea_lanes.png";
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--seeds" && i + 1 < argc)
        {
            seeds.clear();
            for (int x : parse_ints(argv[++i])) seeds.push_back(static_cast<uint32_t>(x));
        }
        else if (a == "--picture" && i + 1 < argc) { picture = true; picture_seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10)); }
        else if (a == "--png" && i + 1 < argc) png = argv[++i];
        else { std::printf("unknown argument %s\n", a.c_str()); return 2; }
    }
    shipped_inputs shipped;
    load_shipped_inputs(shipped);
    if (picture)
    {
        run_picture(shipped, picture_seed, png);
        return 0;
    }
    synthetic_rows();
    for (const uint32_t s : seeds) world_rows(shipped, s);
    std::printf("\n%s: %d failure(s)\n", g_failures == 0 ? "ALL PASS" : "FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
