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
//   S2  the walker prices its steps with the current: sailing east against
//       the trades, the priced walk leaves the one still-water line for water
//       that runs with it; across a basin the return takes other water than
//       the outbound; the walk is deterministic
//   S3  a seat's port is its nearest sea tile within the radius, ties low;
//       an inland seat beyond it has none
//   R1  (BL-1153) a seat inland of the radius lays its lane from its REALM's
//       nearest coastal seat -- the sim's Chebyshev region measure, the same
//       realm only, ties to the lower region
//   R2  a realm with no coastal region lays nothing, and is counted so
//   R3  an inland seat no realm holds lays nothing, and is counted so
//   R4  a leg whose two ends land on one port tile lays nothing, counted apart
//   R5  a moved lane walks toward the busier PICKED port; no port pair is
//       walked both ways
//   R6  the pick measures across the column seam
//   R7  no realm table handed: counted as its own reason, never as unheld
//   W0  the realms handed the stamp are the last close's region owners
//   W1  on a generated 1960 world lanes are stamped (count reported), only on
//       sea tiles, and no road lies on water (the road lens has none to draw);
//       the stamp's earned / laid / no-port (no realms, unheld, no coast) /
//       same-port / unreachable / moved-end split matches the harness's own
//       count off the record, the realms, the ports and the sea's components;
//       every laid lane starts at the seat the harness picks (by
//       `region_distance` itself) and walks toward the busier port by the
//       harness's own degree; no laid lane is one tile and no port pair is
//       walked both ways (short coastal hops reported)
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

/// True iff consecutive tiles are four-cardinal neighbours (columns wrapping)
/// and every tile is sea -- the grid the pathfinder walks (logistics.cpp).
bool walk_is_honest(const std::vector<int>& path, const std::vector<uint8_t>& sea, int gw)
{
    for (std::size_t i = 0; i < path.size(); ++i)
    {
        if (!sea[static_cast<std::size_t>(path[i])]) return false;
        if (i == 0) continue;
        const int a = path[i - 1], b = path[i];
        if (cheb(a, b, gw) != 1) return false;
        const int ac = a % gw, ar = a / gw, bc = b % gw, br = b / gw;
        if (ac != bc && ar != br) return false; // a diagonal step: not on the pathfinder's grid
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
// R1-R7: a realm's port is its nearest coastal region's seat (BL-1153)
// ---------------------------------------------------------------------------
//
// LOGISTICS.md § 4b (Ben, 2026-09-27, NR-955 B). One synthetic body, 60 x 30: a
// channel of sea in columns [24, 44), land either side, and the columns wrap (the
// east shore's land runs on across the seam into the west's). A seat is COASTAL
// when a sea tile lies within kSeaLanePortRadius (9) of it -- columns 15..23 west of
// the channel, 44..52 east of it. The regions, and what each one is there to catch:
//
//   0  realm 7   (4,15)   INLAND: the lane end under test (sea 20 tiles off)
//   1  realm 7   (16,6)   coastal, 12 from region 0 -- THE ANSWER (port 24)
//   2  realm 9   (46,15)  the partner, on the east shore (port 763)
//   3  realm 7   (16,24)  coastal, 12 from region 0 too: the TIE, lost to the lower index
//   4  realm 8   (15,15)  coastal and NEARER (11), but another realm's
//   5  realm 7   (18,15)  coastal, 14 by the sim's Chebyshev -- but 14 against 21 by
//                         Manhattan and 14 against 15 by Euclid, so any other
//                         measure picks it
//   6  realm 10  (4,4)    inland, and realm 10 holds no coastal region at all
//   7  realm 10  (6,26)   inland, realm 10's only other region
//   8  no realm  (8,15)   inland and held by nobody
//   9  realm 11  (2,15)   INLAND, the seam case: sea 19 tiles off either way
//  10  realm 11  (50,15)  coastal on the east shore, 12 from region 9 ACROSS THE SEAM
//                         (48 without the wrap) -- THE ANSWER for region 9
//  11  realm 11  (17,2)   coastal, 15 from region 9 without crossing the seam, so a
//                         measure that forgets the wrap picks it
//
// Each row stamps its own legs on a fresh copy of the body, so each count is that
// row's alone.

struct realm_port_case
{
    sea_lane_stats st;
    sea_lane_trace tr;
    std::vector<uint8_t> sea;
};

realm_port_case stamp_realm_legs(const std::vector<sea_leg>& legs, bool hand_realms = true)
{
    constexpr int gw = 60, gh = 30;
    world fw;
    const entity_id fbody = fw.create_entity();
    body_component bc{};
    bc.grid_width = gw;
    bc.grid_height = gh;
    fw.bodies[fbody] = bc;
    realm_port_case out;
    out.sea.assign(static_cast<std::size_t>(gw) * gh, 0);
    for (int r = 0; r < gh; ++r)
        for (int c = 0; c < gw; ++c)
        {
            const bool wet = c >= 24 && c < 44;
            out.sea[static_cast<std::size_t>(r) * gw + c] = wet ? 1 : 0;
            const entity_id id = fw.create_entity();
            tile_component t{};
            t.body = fbody; t.grid_x = c; t.grid_y = r;
            t.substrate = wet ? terrain_substrate::ocean : terrain_substrate::sedimentary;
            t.landform = terrain_landform::plains;
            t.habitability = wet ? 0.0f : 0.5f;
            fw.tiles.emplace(id, t);
        }
    const std::vector<history_road_node> nodes = {
        {4, 15, 0}, {16, 6, 0}, {46, 15, 0}, {16, 24, 0}, {15, 15, 0},
        {18, 15, 0}, {4, 4, 0}, {6, 26, 0}, {8, 15, 0},
        {2, 15, 0}, {50, 15, 0}, {17, 2, 0} };
    const std::vector<int> realms = { 7, 7, 9, 7, 8, 7, 10, 10, -1, 11, 11, 11 };
    stamp_sea_lanes(fw, fbody, nodes, legs, hand_realms ? realms : std::vector<int>{}, 4, /*weight=*/0, 1,
                    &out.st, &out.tr);
    return out;
}

const sea_lane_trace::lane* traced(const realm_port_case& k, int a, int b)
{
    for (const sea_lane_trace::lane& ln : k.tr.lanes)
        if (ln.a == a && ln.b == b) return &ln;
    return nullptr;
}

void realm_port_rows()
{
    std::printf("\n--- R1-R7: a realm's port is its nearest coastal region's seat (BL-1153) ---\n");
    constexpr int gw = 60, gh = 30;

    // R1: region 0's seat is inland; the leg 0-2 lays from region 1's seat.
    {
        const realm_port_case k = stamp_realm_legs({ sea_leg{0, 2, 10} });
        const sea_lane_trace::lane* ln = k.tr.lanes.empty() ? nullptr : &k.tr.lanes.front();
        const int want_port = sea_lane_port(k.sea, gw, gh, 16, 6, kSeaLanePortRadius);
        std::printf("      leg 0-2: laid %d, moved ends %d, no port %d; the inland end's port taken at region %d"
                    " (want 1), tile %d (want %d); %zu-tile lane\n",
                    k.st.laid, k.st.moved_ends, k.st.no_port, ln ? ln->from_seat : -1,
                    ln ? ln->from_port : -1, want_port, ln ? ln->path.size() : static_cast<std::size_t>(0));
        check(sea_lane_port(k.sea, gw, gh, 4, 15, kSeaLanePortRadius) < 0 && k.st.laid == 1 && k.st.moved_ends == 1
              && ln != nullptr && ln->from_seat == 1 && ln->to_seat == 2 && ln->from_port == want_port
              && !ln->path.empty() && ln->path.front() == want_port,
              "R1  a seat inland of nine tiles lays its lane from its realm's nearest coastal seat"
              " (the sim's Chebyshev measure, same realm only, ties to the lower region)");
    }

    // R2: region 6's realm holds no coastal region, so the leg 2-6 lays nothing.
    {
        const realm_port_case k = stamp_realm_legs({ sea_leg{2, 6, 10} });
        std::printf("      leg 2-6: laid %d, no port %d (no coast %d, unheld %d, no realms %d), lane tiles %d\n",
                    k.st.laid, k.st.no_port, k.st.no_port_no_coast, k.st.no_port_unheld, k.st.no_port_no_realms,
                    k.st.lane_tiles);
        check(k.st.earned == 1 && k.st.laid == 0 && k.st.lane_tiles == 0 && k.st.no_port == 1
              && k.st.no_port_no_coast == 1 && k.st.no_port_unheld == 0 && k.st.no_port_no_realms == 0,
              "R2  a realm with no coastal region lays nothing, and is counted as such");
    }

    // R3: region 8 is held by no realm, so there is no realm's coast to move to.
    {
        const realm_port_case k = stamp_realm_legs({ sea_leg{2, 8, 10} });
        std::printf("      leg 2-8: laid %d, no port %d (no coast %d, unheld %d, no realms %d), lane tiles %d\n",
                    k.st.laid, k.st.no_port, k.st.no_port_no_coast, k.st.no_port_unheld, k.st.no_port_no_realms,
                    k.st.lane_tiles);
        check(k.st.earned == 1 && k.st.laid == 0 && k.st.lane_tiles == 0 && k.st.no_port == 1
              && k.st.no_port_unheld == 1 && k.st.no_port_no_coast == 0 && k.st.no_port_no_realms == 0,
              "R3  an inland seat no realm holds lays nothing, and is counted as such");
    }

    // R4: the leg 0-1 -- region 0 moves to region 1's seat, which is the leg's
    // other end (an annexed target leaves both ends in one realm): both ends land
    // on port 24, so there is no water between them and nothing is laid.
    {
        const realm_port_case k = stamp_realm_legs({ sea_leg{0, 1, 10} });
        std::printf("      leg 0-1: laid %d, same port %d, lane tiles %d\n", k.st.laid, k.st.same_port, k.st.lane_tiles);
        check(k.st.earned == 1 && k.st.laid == 0 && k.st.same_port == 1 && k.st.lane_tiles == 0 && k.tr.lanes.empty(),
              "R4  a leg whose two ends land on one port tile lays nothing, and is counted as its own reason");
    }

    // R5: THE BUSIER END IS THE PICKED PORT. Legs 0-2, 1-2, 1-3, 1-4: region 0
    // moves onto region 1's port 24, which four lanes then land on (763 two, 984
    // and 384 one each). By the RECORD's ends region 0 is the quietest (one leg)
    // and region 1 the busiest (three), so a degree read before the move walks
    // 0-2 from 24 out to 763 and 1-2 from 763 in to 24: one port pair both ways.
    // Read off the ports, both walk 763 -> 24.
    {
        const realm_port_case k = stamp_realm_legs(
            { sea_leg{0, 2, 10}, sea_leg{1, 2, 10}, sea_leg{1, 3, 10}, sea_leg{1, 4, 10} });
        const sea_lane_trace::lane* m = traced(k, 0, 2);
        const sea_lane_trace::lane* u = traced(k, 1, 2);
        std::map<std::pair<int, int>, std::pair<int, int>> dir;
        bool both_ways = false;
        for (const sea_lane_trace::lane& ln : k.tr.lanes)
        {
            const std::pair<int, int> key{ std::min(ln.from_port, ln.to_port), std::max(ln.from_port, ln.to_port) };
            const auto it = dir.find(key);
            if (it == dir.end()) dir[key] = { ln.from_port, ln.to_port };
            else if (it->second.first != ln.from_port) both_ways = true;
        }
        std::printf("      legs 0-2, 1-2, 1-3, 1-4: laid %d; the moved lane 0-2 walks %d -> %d, lane 1-2 %d -> %d"
                    " (want 763 -> 24 both); a port pair walked both ways: %s\n",
                    k.st.laid, m ? m->from_port : -1, m ? m->to_port : -1, u ? u->from_port : -1,
                    u ? u->to_port : -1, both_ways ? "YES" : "no");
        check(k.st.laid == 4 && m != nullptr && m->from_port == 763 && m->to_port == 24 && m->to_seat == 1
              && u != nullptr && u->from_port == 763 && u->to_port == 24 && !both_ways,
              "R5  a moved lane walks toward the busier PICKED port, and one port pair is never walked both ways");
    }

    // R6: the column wrap. Region 9's nearest coastal seat of its realm is region
    // 10, across the seam (12 tiles), not region 11 (15 tiles, no seam to cross).
    {
        const realm_port_case k = stamp_realm_legs({ sea_leg{1, 9, 10} });
        const sea_lane_trace::lane* ln = traced(k, 1, 9);
        const int want_port = sea_lane_port(k.sea, gw, gh, 50, 15, kSeaLanePortRadius);
        const int seat9 = ln == nullptr ? -1 : (ln->from_seat == 1 ? ln->to_seat : ln->from_seat);
        const int port9 = ln == nullptr ? -1 : (ln->from_seat == 1 ? ln->to_port : ln->from_port);
        std::printf("      leg 1-9: laid %d, moved ends %d; region 9's port taken at region %d (want 10), tile %d (want %d)\n",
                    k.st.laid, k.st.moved_ends, seat9, port9, want_port);
        check(sea_lane_port(k.sea, gw, gh, 2, 15, kSeaLanePortRadius) < 0 && k.st.laid == 1 && k.st.moved_ends == 1
              && seat9 == 10 && port9 == want_port,
              "R6  the pick measures across the seam: the columns wrap, as region_distance's do");
    }

    // R7: no realm table handed at all. The inland end cannot move, and that is a
    // caller fault, counted as its own reason -- never as an unheld seat.
    {
        const realm_port_case k = stamp_realm_legs({ sea_leg{0, 2, 10} }, /*hand_realms=*/false);
        std::printf("      leg 0-2 with no realms handed: laid %d, no port %d (no realms %d, unheld %d, no coast %d)\n",
                    k.st.laid, k.st.no_port, k.st.no_port_no_realms, k.st.no_port_unheld, k.st.no_port_no_coast);
        check(k.st.laid == 0 && k.st.no_port == 1 && k.st.no_port_no_realms == 1 && k.st.no_port_unheld == 0
              && k.st.no_port_no_coast == 0,
              "R7  a stamp handed no realm table counts an inland end as no-realms-handed, not as unheld");
    }
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
        // THE BEND, ONE DIRECTION, ON A LEG WITH ONE STILL-WATER LINE. The
        // diagonal leg above cannot carry it on a four-way grid: every walk of
        // its Manhattan length ties in still water, and the tie-break lands on
        // the very staircase the current favours (priced == still there), so
        // the eight-way walker's one-directional reading could only have been
        // kept by reading both directions. A leg along ONE ROW has exactly one
        // shortest still-water walk -- the row itself -- so a priced walk off
        // that row is the current's doing and nothing else's. East along the
        // trades' row (the trades run west): the priced walk must leave the row
        // for water running more with it.
        {
            const int ea = 24 * bw + 7, eb = 24 * bw + 41;
            const std::vector<int> row_still  = sea_lane_walk(bsea, bw, bh, nullptr, 0, ea, eb);
            const std::vector<int> row_priced = sea_lane_walk(bsea, bw, bh, &f, 500, ea, eb);
            bool still_is_row = !row_still.empty();
            for (const int t : row_still) if (t / bw != 24) still_is_row = false;
            const double row_sep = mean_separation(row_priced, row_still, bw);
            const double al_row_still = mean_step_alignment(row_still, f, bw);
            const double al_row_priced = mean_step_alignment(row_priced, f, bw);
            std::printf("      east along the trades (7,24) -> (41,24): still %zu tiles (on the row: %s), priced %zu;"
                        " separation %.2f tiles; mean current along the steps %.0f still, %.0f priced\n",
                        row_still.size(), still_is_row ? "yes" : "no", row_priced.size(), row_sep,
                        al_row_still, al_row_priced);
            check(still_is_row && walk_is_honest(row_priced, bsea, bw) && row_sep > 0.5
                  && al_row_priced > al_row_still,
                  "S2  the current bends the walk: sailing east against the trades it leaves the one still-water line"
                  " for water that runs with it");
        }
        check(!back.empty() && mean_separation(back, priced, bw) > 0.5,
              "S2  a basin circulates: the return walk takes other water than the outbound");
    }

    // S4: THE REALISED DISCOUNT. A lane stamped between two ports on an
    // all-ocean body, then the traversal cost between the same ports with the
    // lane and without: along a laid lane the pathfinder must ride it at the
    // full x0.50 -- on a straight lane and on a DIAGONAL one. The pathfinder is
    // four-cardinal (logistics.cpp), so a lane walked eight ways leaves no two
    // laned tiles side by side on a diagonal and is ridden at about x0.75: this
    // row is the one that catches it. Still water (weight 0), so the walk is a
    // shortest path and the laned route is the cheapest there is.
    {
        struct fixture_case { const char* name; int ac, ar, bc, br; };
        const fixture_case cases[2] = { {"straight", 4, 15, 26, 15}, {"diagonal", 10, 10, 20, 20} };
        for (const fixture_case& fc : cases)
        {
            constexpr int fgw = 30, fgh = 30;
            world fw;
            const entity_id fbody = fw.create_entity();
            body_component bc{};
            bc.grid_width = fgw;
            bc.grid_height = fgh;
            fw.bodies[fbody] = bc;
            for (int r = 0; r < fgh; ++r)
                for (int c = 0; c < fgw; ++c)
                {
                    const entity_id id = fw.create_entity();
                    tile_component t{};
                    t.body = fbody; t.grid_x = c; t.grid_y = r;
                    t.substrate = terrain_substrate::ocean;
                    t.landform = terrain_landform::plains;
                    t.habitability = 0.0f;
                    fw.tiles.emplace(id, t);
                }
            const std::vector<entity_id> fgrid = body_tile_grid(fw, fbody);
            const std::vector<history_road_node> nodes = {
                history_road_node{fc.ac, fc.ar, 0}, history_road_node{fc.bc, fc.br, 0} };
            const std::vector<sea_leg> legs = { sea_leg{0, 1, 10} };
            sea_lane_stats fst;
            stamp_sea_lanes(fw, fbody, nodes, legs, /*region_realm=*/{}, 4, /*weight=*/0, 1, &fst);
            const entity_id ta = fgrid[static_cast<std::size_t>(fc.ar) * fgw + fc.ac];
            const entity_id tb = fgrid[static_cast<std::size_t>(fc.br) * fgw + fc.bc];
            invalidate_logistics_caches(fw);
            const float with_lane = intra_body_path(fw, fbody, ta, tb).cost;
            for (auto& kv : fw.tiles) kv.second.lane_level = 0;
            invalidate_logistics_caches(fw);
            const float without = intra_body_path(fw, fbody, ta, tb).cost;
            const double ratio = without > 0.0f ? static_cast<double>(with_lane) / without : 0.0;
            std::printf("      %s lane (%d,%d)->(%d,%d): %d lane tiles; path cost %.2f with the lane, %.2f without,"
                        " ratio %.3f\n", fc.name, fc.ac, fc.ar, fc.bc, fc.br, fst.lane_tiles,
                        with_lane, without, ratio);
            check(fst.laid == 1 && std::fabs(ratio - 0.5) <= 0.01,
                  fc.name[0] == 's'
                      ? "S4  a straight lane is ridden at the full x0.50 of the unlaned water"
                      : "S4  a DIAGONAL lane is ridden at the full x0.50 too (the walk shares the pathfinder's grid)");
        }
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

    realm_port_rows();
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
    std::vector<int> realms; ///< what world setup handed the stamp (`setup_lane_realms`)
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
    L.realms = L.fx.setup_lane_realms;
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

    // W0: THE REALM SNAPSHOT IS THE LAST CLOSE. What world setup handed the stamp
    // (`setup_lane_realms`) must be, region for region, the `nation` field of the
    // last closed span's own handoff -- Industrialisation's when it ran, else
    // Exploration's -- which is the polity holding each region at that close.
    // Everything below re-stamps with that vector, so without this row W1 would
    // only prove the stamp agrees with its own input.
    {
        const exploration_output& close = L.fx.industrialisation_ran
            ? static_cast<const exploration_output&>(L.fx.industrialisation_handoff) : L.fx.exploration_handoff;
        bool same = L.realms.size() == close.regions.size() && L.realms.size() == L.nodes.size();
        int differ = 0, held = 0;
        for (std::size_t i = 0; i < L.realms.size() && i < close.regions.size(); ++i)
        {
            if (L.realms[i] != close.regions[i].nation) ++differ;
            if (L.realms[i] >= 0) ++held;
        }
        if (differ > 0) same = false;
        std::printf("      realm snapshot: %zu realm rows handed, %zu regions at the %s close, %d held, %d differ\n",
                    L.realms.size(), close.regions.size(), L.fx.industrialisation_ran ? "1960" : "1660", held, differ);
        check(same, "W0  the realms handed the stamp are the last closed span's own region owners, region for region");
    }

    // W2: re-stamp from the record on cleared water.
    set_lane_field(L, std::vector<uint8_t>(generated.size(), 0));
    sea_lane_stats st;
    sea_lane_trace tr;
    const auto t_stamp = std::chrono::steady_clock::now();
    stamp_sea_lanes(L.w, L.body, L.nodes, L.legs, L.realms, lp.sea_lane_tier1_uses, lp.sea_current_weight_q,
                    lp.sea_current_rotation_sense, &st, &tr);
    const double stamp_ms = seconds_since(t_stamp) * 1000.0;
    const std::vector<uint8_t> restamped = lane_field(L);
    std::printf("      stamp: earned %d, laid %d, no port %d (no realms %d, unheld %d, no coast %d), same port %d,"
                " unreachable %d; %d ends moved; %lld path tiles, %d lane tiles; the stamp (walker included) took %.1f ms\n",
                st.earned, st.laid, st.no_port, st.no_port_no_realms, st.no_port_unheld, st.no_port_no_coast,
                st.same_port, st.unreachable, st.moved_ends, st.path_tiles, st.lane_tiles, stamp_ms);
    check(restamped == generated && st.lane_tiles == lane_tiles,
          "W2  re-stamping the world's own lane record reproduces its lane field exactly");
    // A world whose record earned no lane with a port at both ends legitimately
    // stamps none; one that laid any carries lane tiles on its 1960 map.
    check((lane_tiles > 0) == (st.laid > 0),
          "W1  lanes are stamped on the 1960 world exactly where the record earned one with ports at both ends");
    // The seeds this harness runs (32 and 46 by default) are chosen because they
    // earn lanes; a run that lays none measured nothing, so that is a failure.
    check(st.laid > 0, "W1  the stamp lays at least one lane on this seed");
    // THE SPLIT, COUNTED AGAIN BY THE HARNESS off the record, the settlement
    // nodes and the sea -- not the stamp's own sum (laid + no_port + same_port +
    // unreachable == earned holds by construction inside the stamp, so it
    // could never fail). Earned: legs at the tier between two on-grid nodes.
    // No port: an end whose seat's nearest sea tile lies beyond
    // kSeaLanePortRadius (`sea_lane_port`) AND whose realm holds no region with
    // one, or which no realm holds, or which no realm table reached (BL-1153,
    // NR-955 B; picked below). Same port: both ends' ports are one tile.
    // Unreachable: the two ports lie on different WATER COMPONENTS -- the
    // harness's own flood of the sea mask on the four cardinal steps, columns
    // wrapping, never the walker's search. Laid is what remains. `no_port` and
    // `unreachable` are facts of the settlement map and the sea's topology,
    // printed, not bounded; what is bound is that the stamp's split is this one.
    {
        std::vector<int> water_comp(L.sea.size(), -1);
        int comps = 0;
        for (int i = 0; i < L.gw * L.gh; ++i)
        {
            if (!L.sea[static_cast<std::size_t>(i)] || water_comp[static_cast<std::size_t>(i)] >= 0) continue;
            std::vector<int> stack{ i };
            water_comp[static_cast<std::size_t>(i)] = comps;
            while (!stack.empty())
            {
                const int u = stack.back();
                stack.pop_back();
                const int uc = u % L.gw, ur = u / L.gw;
                const int nb[4][2] = { {uc, ur - 1}, {uc, ur + 1}, {(uc + L.gw - 1) % L.gw, ur}, {(uc + 1) % L.gw, ur} };
                for (const auto& q : nb)
                {
                    if (q[1] < 0 || q[1] >= L.gh) continue;
                    const int v = q[1] * L.gw + q[0];
                    if (!L.sea[static_cast<std::size_t>(v)] || water_comp[static_cast<std::size_t>(v)] >= 0) continue;
                    water_comp[static_cast<std::size_t>(v)] = comps;
                    stack.push_back(v);
                }
            }
            ++comps;
        }
        // BL-1153: an inland end's port is its REALM's nearest coastal seat, counted
        // again here with the sim's own `region_distance` over the settlement's own
        // `region`s (the stamp restates the measure on the flattened node) and the
        // realms world setup handed the stamp (bound to the last close by W0 above).
        // A pick is a region index, -1 for an end no realm holds, -2 for a realm
        // with no coastal region, -3 for an end no realm table reached.
        const std::vector<region>* regs = L.w.gen_settlement ? &L.w.gen_settlement->regions : nullptr;
        const bool regs_ok = regs != nullptr && regs->size() == L.nodes.size();
        std::vector<int> port_of(L.nodes.size(), -1);
        for (std::size_t j = 0; j < L.nodes.size(); ++j)
            port_of[j] = sea_lane_port(L.sea, L.gw, L.gh, L.nodes[j].col, L.nodes[j].row, kSeaLanePortRadius);
        const auto pick = [&](int ri) -> int {
            if (port_of[static_cast<std::size_t>(ri)] >= 0) return ri;
            if (!regs_ok || static_cast<std::size_t>(ri) >= L.realms.size()) return -3;
            if (L.realms[static_cast<std::size_t>(ri)] < 0) return -1;
            int best = -2, best_d = std::numeric_limits<int>::max();
            for (std::size_t j = 0; j < L.nodes.size() && j < L.realms.size(); ++j)
            {
                if (L.realms[j] != L.realms[static_cast<std::size_t>(ri)] || port_of[j] < 0) continue;
                const int d = region_distance((*regs)[static_cast<std::size_t>(ri)], (*regs)[j], L.gw);
                if (d < best_d) { best_d = d; best = static_cast<int>(j); }
            }
            return best;
        };
        int h_earned = 0, h_no_port = 0, h_no_realms = 0, h_unheld = 0, h_no_coast = 0, h_same = 0, h_same_moved = 0;
        int h_unreachable = 0, h_laid = 0, h_moved = 0;
        int moved_laid = 0, moved_unreachable = 0, move_max = 0;
        long long move_sum = 0;
        struct walk_leg { int a, b, sa, sb, pa, pb; bool reach; bool moved; };
        std::vector<walk_leg> walkable;
        for (const sea_leg& l : L.legs)
        {
            if (l.uses < lp.sea_lane_tier1_uses || l.a == l.b) continue;
            if (l.a >= L.nodes.size() || l.b >= L.nodes.size()) continue;
            const history_road_node& na = L.nodes[l.a];
            const history_road_node& nb = L.nodes[l.b];
            if (na.col < 0 || na.row < 0 || na.col >= L.gw || na.row >= L.gh
             || nb.col < 0 || nb.row < 0 || nb.col >= L.gw || nb.row >= L.gh) continue;
            ++h_earned;
            const int sa = pick(l.a), sb = pick(l.b);
            if (sa < 0 || sb < 0)
            {
                ++h_no_port;
                if (sa == -3 || sb == -3) ++h_no_realms;
                else if (sa == -1 || sb == -1) ++h_unheld;
                else ++h_no_coast;
                continue;
            }
            const int pa = port_of[static_cast<std::size_t>(sa)], pb = port_of[static_cast<std::size_t>(sb)];
            const int moved_here = (sa != l.a ? 1 : 0) + (sb != l.b ? 1 : 0);
            if (pa == pb) { ++h_same; if (moved_here > 0) ++h_same_moved; continue; }
            h_moved += moved_here;
            for (const auto& [end, seat] : { std::pair<int, int>{l.a, sa}, std::pair<int, int>{l.b, sb} })
                if (end != seat)
                {
                    const int d = region_distance((*regs)[static_cast<std::size_t>(end)], (*regs)[static_cast<std::size_t>(seat)], L.gw);
                    move_sum += d;
                    move_max = std::max(move_max, d);
                }
            const bool reach = water_comp[static_cast<std::size_t>(pa)] == water_comp[static_cast<std::size_t>(pb)];
            walkable.push_back(walk_leg{ l.a, l.b, sa, sb, pa, pb, reach, moved_here > 0 });
            if (!reach)
            {
                ++h_unreachable;
                if (moved_here > 0) ++moved_unreachable;
                continue;
            }
            ++h_laid;
            if (moved_here > 0) ++moved_laid;
        }
        std::printf("      the harness's own count: earned %d, laid %d, no port %d (%d no realm table, %d held by no realm,"
                    " %d of a realm with no coast), same port %d (%d with a moved end), unreachable %d (%d water components)\n",
                    h_earned, h_laid, h_no_port, h_no_realms, h_unheld, h_no_coast, h_same, h_same_moved, h_unreachable, comps);
        std::printf("      realm ports: %d lane ends moved to their realm's nearest coastal seat (mean %.1f, max %d tiles"
                    " away); legs with a moved end: %d laid, %d unreachable\n",
                    h_moved, h_moved > 0 ? static_cast<double>(move_sum) / h_moved : 0.0, move_max, moved_laid,
                    moved_unreachable);
        check(h_earned == st.earned && h_laid == st.laid && h_no_port == st.no_port && h_unreachable == st.unreachable
              && h_no_realms == st.no_port_no_realms && h_unheld == st.no_port_unheld
              && h_no_coast == st.no_port_no_coast && h_same == st.same_port && h_moved == st.moved_ends,
              "W1  the stamp's split matches the harness's own count off the record, the realms, the ports and the"
              " sea's components");

        // THE DIRECTION, counted again: degree by the PORT each walkable lane lands
        // on (after the moves), toward the busier port, a tie from the lower index.
        std::map<int, int> degree;
        for (const walk_leg& wl : walkable) { ++degree[wl.pa]; ++degree[wl.pb]; }
        std::map<std::pair<int, int>, std::pair<int, int>> want; // (a, b) -> (from port, to port)
        std::map<std::pair<int, int>, std::pair<int, int>> want_seat; // (a, b) -> (from seat, to seat)
        for (const walk_leg& wl : walkable)
        {
            if (!wl.reach) continue;
            const int da = degree[wl.pa], db = degree[wl.pb];
            const bool from_a = da != db ? da < db : wl.pa < wl.pb;
            want[{wl.a, wl.b}] = from_a ? std::pair<int, int>{wl.pa, wl.pb} : std::pair<int, int>{wl.pb, wl.pa};
            want_seat[{wl.a, wl.b}] = from_a ? std::pair<int, int>{wl.sa, wl.sb} : std::pair<int, int>{wl.sb, wl.sa};
        }
        bool seats_agree = static_cast<int>(want.size()) == static_cast<int>(tr.lanes.size());
        bool direction_agrees = seats_agree;
        int same_port = 0, short_hops = 0;
        std::map<std::pair<int, int>, int> pair_from; // unordered port pair -> the from port it was walked with
        int both_ways = 0;
        for (const sea_lane_trace::lane& ln : tr.lanes)
        {
            if (ln.from_port == ln.to_port) ++same_port;
            const auto it = want.find({ln.a, ln.b});
            if (it == want.end()) { seats_agree = false; direction_agrees = false; continue; }
            const auto& ws = want_seat[{ln.a, ln.b}];
            if (ln.from_seat != ws.first || ln.to_seat != ws.second) seats_agree = false;
            if (ln.from_port != it->second.first || ln.to_port != it->second.second) direction_agrees = false;
            const std::pair<int, int> key{ std::min(ln.from_port, ln.to_port), std::max(ln.from_port, ln.to_port) };
            const auto pf = pair_from.find(key);
            if (pf == pair_from.end()) pair_from[key] = ln.from_port;
            else if (pf->second != ln.from_port) ++both_ways;
            const bool moved = ln.from_seat != ln.a && ln.from_seat != ln.b ? true
                             : (ln.to_seat != ln.a && ln.to_seat != ln.b);
            if (moved && cheb(ln.from_port, ln.to_port, L.gw) <= 2) ++short_hops;
        }
        check(regs_ok && seats_agree,
              "W1  every laid lane starts at the seat the harness picks: its own, or its realm's nearest coastal one"
              " by region_distance, ties low");
        check(direction_agrees,
              "W1  every laid lane walks toward its busier PORT (the harness's own degree, after the moves), a tie"
              " from the lower raster index");
        std::printf("      laid lanes whose two ends share one port tile: %d; port pairs walked both ways: %d;"
                    " short coastal hops (a moved end, the two ports within 2 tiles): %d\n",
                    same_port, both_ways, short_hops);
        check(same_port == 0, "W1  no laid lane has both ends on one port tile (a one-tile lane is never laid)");
        check(both_ways == 0, "W1  no port pair is walked both ways (no braid)");
    }
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
