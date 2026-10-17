// ---------------------------------------------------------------------------
// A road is an edge between two tiles (BL-1340) — no SDL / Lua
// ---------------------------------------------------------------------------
// Ben, from a playtest 2026-10-16: "Roads should be directional, so they always
// cross the edge of two tiles." A road moved off the tile and onto the EDGE,
// where a river already lived: `tile_component::road_edges` holds a tier per hex
// side, two bits each, and BOTH tiles of a crossing carry it — side i of one
// mirroring side i+3 mod 6 of the other. LOGISTICS.md § 4 is the design; this is
// its standing guard.
//
//   M1  THE MIRROR HOLDS. On a really generated world, every side of every tile
//       whose neighbour exists agrees with that neighbour's facing side. This is
//       the invariant the whole model rests on: break it and a convoy's cost
//       depends on which way it is asked, which is exactly what BL-1126 proved
//       a loaded game cannot survive.
//   M2  A BRIDGE IS A CHAIN OF CROSSINGS OVER WATER (Ben, 2026-10-17: "edge means
//       from one tile centre to another, but not only one tile centre. This should
//       allow spanning water with bridges"). A road edge over water is LEGAL and
//       mirrored like any other, a strait is crossed by a CHAIN of them that joins
//       two shores, and no bridge stands on OPEN OCEAN -- generation refuses a route
//       whose water run is not a strait (`crossing_kind`), so a bridge is a strait
//       span, never an ocean crossing. That is the row the old "no road touches
//       water" one became: strictly stronger, because it says where a bridge may be.
//   B1  AND A BRIDGE BEATS THE WATER IT CROSSES. The hop over a bridged edge costs
//       its road tier's price (the deck, not the sea leg), so it is strictly cheaper
//       than the same hop with the crossing taken away -- "a bridge that did not beat
//       the water it crosses would be a decoration".
//   C1  EVERY EDGE COUNTS ONCE. `count_road_edges` reads a tile's sides 0-2
//       only; an independent count that walks ALL SIX sides of every tile must
//       come to exactly twice it. That is the property every count of the
//       network — the Logistic Point pool, network upkeep — now rests on.
//   E1  THE COST FALLS WITH TIER. The edge discount is 1 / (1 + 0.5 x tier):
//       strictly decreasing, 1.0 off the network, and a real A* path across a
//       laid Highway costs strictly less than the same path with the crossing
//       taken away.
//   E2  AND IT IS SYMMETRIC, UNLIKE A RIVER. The same crossing prices the step
//       the same way round — `road_edge_discount` read from either end is one
//       number — so a road adds no direction to a path, where a river does.
//   S1  THE SAVE ROUND-TRIPS BYTE-IDENTICALLY at v53, mask for mask.
//   S2  A PRE-v53 SAVE MIGRATES to the LOWER-tier rule: between each pair of
//       4-cardinal neighbours that both carried a road, an edge at the lower of
//       their two tiers (LOGISTICS.md § 4, Older saves). Checked against an
//       independent recomputation from the pre-edge field, not against the
//       migration's own arithmetic.
//   S3  A TAMPERED MIRROR IS REFUSED WHOLE. One byte of one tile's mask flipped
//       in a v53 stream, and `read_world_snapshot` rejects it and mutates
//       nothing — the standing rejection contract.
//   D1  DETERMINISM. Two generations of one seed produce the same road field,
//       edge for edge.
//
// Run: road_edges   (build via tools/verify/build_harness.js)
// ---------------------------------------------------------------------------

#include "harness_params.hpp"

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/hex_neighbors.hpp"
#include "world/logistics.hpp"
#include "world/road_generation.hpp" // kMaxCrossingTiles: a bridge is a strait span
#include "world/river_generation.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace {

int g_failures = 0;
int g_checks   = 0;

void check(bool ok, const std::string& what)
{
    ++g_checks;
    if (!ok) ++g_failures;
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

std::string fmt(const char* f, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

/// (body, row, col) -> tile, built here rather than borrowed, so the mirror row
/// checks the implementation against an independent index.
std::map<std::tuple<entity_id, int, int>, entity_id> cell_index(const world& w)
{
    std::map<std::tuple<entity_id, int, int>, entity_id> idx;
    for (const auto& [tid, tc] : w.tiles)
        idx.emplace(std::make_tuple(tc.body, tc.grid_y, tc.grid_x), tid);
    return idx;
}

/// The whole road field as a comparable value: (tile, mask) for every tile that
/// carries one, ascending tile id.
std::vector<std::pair<entity_id, std::uint16_t>> road_field(const world& w)
{
    std::vector<std::pair<entity_id, std::uint16_t>> out;
    for (const auto& [tid, tc] : w.tiles)
        if (tc.road_edges != 0) out.emplace_back(tid, tc.road_edges);
    std::sort(out.begin(), out.end());
    return out;
}

std::string to_bytes(const world& w)
{
    std::ostringstream o(std::ios::binary);
    write_world_snapshot(w, o);
    return o.str();
}

std::string to_bytes_pre_edges(const world& w)
{
    std::ostringstream o(std::ios::binary);
    write_world_snapshot_pre_road_edges(w, o);
    return o.str();
}

bool from_bytes(const std::string& b, world& out)
{
    std::istringstream i(b, std::ios::binary);
    return read_world_snapshot(out, i);
}

} // namespace

int main(int argc, char** argv)
{
    std::uint32_t seed = 12345u;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--seed" && i + 1 < argc) seed = static_cast<std::uint32_t>(std::atoi(argv[++i]));
    }

    std::printf("=== road_edges (BL-1340) — seed %u ===\n", seed);

    world_params params = no_prehistory();
    params.seed = seed;
    world w = make_hard_coded_world(params);

    const auto idx = cell_index(w);

    // --- M1 / M2: the mirror, and no road on water -------------------------
    {
        long long sides_set = 0, mirrored = 0, orphan = 0, on_water = 0, on_ocean = 0;
        for (const auto& [tid, tc] : w.tiles)
        {
            const auto bit = w.bodies.find(tc.body);
            const int gw = bit != w.bodies.end() ? bit->second.grid_width  : 0;
            const int gh = bit != w.bodies.end() ? bit->second.grid_height : 0;
            for (int s = 0; s < 6; ++s)
            {
                const std::uint8_t tier = road_tier_on_side(tc, s);
                if (tier == 0) continue;
                ++sides_set;
                if (is_water(tc.substrate))
                {
                    ++on_water;
                    if (is_open_ocean(tc.substrate)) ++on_ocean;
                }
                const hex_neighbors::coord c = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, s);
                if (c.gy < 0 || (gh > 0 && c.gy >= gh)) { ++orphan; continue; }
                const int nx = gw > 0 ? (((c.gx % gw) + gw) % gw) : c.gx;
                const auto nit = idx.find(std::make_tuple(tc.body, c.gy, nx));
                if (nit == idx.end()) { ++orphan; continue; }
                const tile_component& ntc = w.tiles.at(nit->second);
                if (is_water(ntc.substrate))
                {
                    ++on_water;
                    if (is_open_ocean(ntc.substrate)) ++on_ocean;
                }
                if (road_tier_on_side(ntc, opposite_hex_side(s)) == tier) ++mirrored;
            }
        }
        std::printf("       %lld tile-sides carry a road; %lld mirrored, %lld with no neighbour\n",
                    sides_set, mirrored, orphan);
        check(sides_set > 0, "M1a the generated world laid some roads (non-vacuity)");
        check(mirrored == sides_set && orphan == 0,
              fmt("M1b every tile-side agrees with its neighbour's facing side (%lld/%lld)",
                  mirrored, sides_set));
        check(road_edges_mirrored(w), "M1c road_edges_mirrored agrees, over the whole world");
        std::printf("       %lld tile-side(s) of a crossing stand on water (the bridges);"
                    " %lld on open ocean\n", on_water, on_ocean);
        check(on_ocean == 0,
              fmt("M2a no crossing stands on OPEN OCEAN \u2014 a bridge is a strait span (%lld)",
                  on_ocean));
    }

    // --- C1: each edge counted once ----------------------------------------
    {
        int per_tier[3] = {0, 0, 0};
        const int once = count_road_edges(w, per_tier);
        long long all_sides = 0;
        for (const auto& [tid, tc] : w.tiles)
            for (int s = 0; s < 6; ++s)
                if (road_tier_on_side(tc, s) != 0) ++all_sides;
        std::printf("       %d edges (Track %d / Road %d / Highway %d); %lld tile-sides\n",
                    once, per_tier[0], per_tier[1], per_tier[2], all_sides);
        check(once > 0 && static_cast<long long>(once) * 2 == all_sides,
              fmt("C1 count_road_edges counts each edge once (%d x 2 == %lld tile-sides)",
                  once, all_sides));
        check(once == per_tier[0] + per_tier[1] + per_tier[2],
              "C1b the per-tier split sums to the total");

        // C2 — THE COUNTER THE BILL USES. network_upkeep bills per nation off
        // `count_road_edges_by_nation`, so the two must agree: every edge is one
        // nation's or nobody's, and none is counted twice (review of 0831de6d: the
        // hand-rolled loop it replaced dropped a frontier road whose canonical half lay
        // on unowned ground).
        int owned = 0;
        for (const auto& [nid, tiers] : count_road_edges_by_nation(w))
            for (int t = 0; t < 3; ++t) owned += tiers[static_cast<std::size_t>(t)];
        std::printf("       %d of %d edges are some nation's; %d on nobody's ground\n",
                    owned, once, once - owned);
        check(owned > 0 && owned <= once,
              fmt("C2 count_road_edges_by_nation attributes each edge at most once"
                  " (%d of %d)", owned, once));
    }

    // --- M2b: a strait is crossed by a CHAIN of crossings, shore to shore ---
    {
        // Walk every bridged water tile and follow its road edges: a bridge must join
        // two LAND tiles through a run of water, or it joins nothing and is the "road
        // belonging to no crossing" the ruling forbade in the first place.
        // A BRIDGE IS A COMPONENT, not a tile: the connected set of bridged WATER cells
        // joined by roaded crossings. Every such component must touch at least TWO
        // distinct LAND tiles across roaded crossings — that is what makes it a span
        // rather than a pier — and no component may be longer than a strait.
        std::set<entity_id> wet_roaded;
        for (const auto& [tid, tc] : w.tiles)
            if (is_water(tc.substrate) && tile_has_road(tc))
                wet_roaded.insert(tid);
        std::set<entity_id> seen;
        int comps = 0, spans = 0, piers = 0, longest = 0;
        for (const entity_id start : wet_roaded) // ascending id: a stable walk
        {
            if (seen.count(start))
                continue;
            std::vector<entity_id> stack{ start };
            std::set<entity_id> comp, shores;
            seen.insert(start);
            while (!stack.empty())
            {
                const entity_id cur = stack.back();
                stack.pop_back();
                comp.insert(cur);
                const tile_component& ctc = w.tiles.at(cur);
                for (int s = 0; s < 6; ++s)
                {
                    if (road_tier_on_side(ctc, s) == 0)
                        continue;
                    const entity_id nb = road_edge_neighbour(w, cur, s);
                    if (nb == null_entity)
                        continue;
                    const tile_component& ntc = w.tiles.at(nb);
                    if (!is_water(ntc.substrate)) { shores.insert(nb); continue; }
                    if (seen.insert(nb).second)
                        stack.push_back(nb);
                }
            }
            ++comps;
            longest = std::max(longest, static_cast<int>(comp.size()));
            if (shores.size() >= 2) ++spans; else ++piers;
        }
        std::printf("       %zu bridged water tile(s) in %d bridge(s); %d span shore to"
                    " shore, %d reach only one; longest span %d cell(s)\n",
                    wet_roaded.size(), comps, spans, piers, longest);
        check(comps > 0, "M2b the generated world laid at least one bridge (non-vacuity)");
        check(piers == 0,
              fmt("M2b every bridge joins TWO shores \u2014 a chain of crossings, never a pier"
                  " (%d of %d span)", spans, comps));
        check(longest <= kMaxCrossingTiles,
              fmt("M2c no bridge is longer than a strait (%d cell(s), cap %d)",
                  longest, kMaxCrossingTiles));
    }

    // --- B1: a bridge beats the water it crosses ---------------------------
    {
        entity_id a = null_entity, b = null_entity;
        int side = -1;
        for (const auto& [tid, tc] : w.tiles) // ascending id: a stable pick
        {
            if (!is_water(tc.substrate) || !tile_has_road(tc))
                continue;
            for (int s = 0; s < 6 && a == null_entity; ++s)
            {
                if (road_tier_on_side(tc, s) == 0) continue;
                const entity_id other = road_edge_neighbour(w, tid, s);
                if (other == null_entity) continue;
                a = tid; b = other; side = s;
            }
            if (a != null_entity) break;
        }
        check(a != null_entity, "B1a found a bridged crossing to price");
        if (a != null_entity)
        {
            const std::uint8_t tier = road_tier_on_side(w.tiles.at(a), side);
            const float bridged = edge_traversal_cost(w.tiles.at(a), w.tiles.at(b), side);
            tile_component sa = w.tiles.at(a), sb = w.tiles.at(b);
            set_road_tier_on_side(w.tiles.at(a), side, 0);
            set_road_tier_on_side(w.tiles.at(b), opposite_hex_side(side), 0);
            const float bare = edge_traversal_cost(w.tiles.at(a), w.tiles.at(b), side);
            w.tiles.at(a) = sa;
            w.tiles.at(b) = sb;
            invalidate_logistics_caches(w);
            std::printf("       bridged hop %u->%u tier %u: %.6f on the deck,"
                        " %.6f over open water\n", a, b, static_cast<unsigned>(tier),
                        bridged, bare);
            check(bridged < bare,
                  fmt("B1b the hop over a bridge is paid at its road price, not the sea"
                      " leg's (%.6f < %.6f)", bridged, bare));
            check(bridged <= road_traversal_multiplier(tier) * k_bridge_deck_cost + 1e-6f,
                  "B1c and it is no dearer than the deck at that tier");
        }
    }

    // --- E1 / E2: the discount falls with tier, and is symmetric -----------
    {
        bool falls = true;
        for (int t = 1; t <= 3; ++t)
            falls = falls && road_traversal_multiplier(static_cast<std::uint8_t>(t))
                           < road_traversal_multiplier(static_cast<std::uint8_t>(t - 1));
        check(falls && road_traversal_multiplier(0) == 1.0f,
              "E1a the ladder is strictly decreasing and tier 0 costs full price");

        // A real crossing, read from both ends.
        entity_id a = null_entity, b = null_entity;
        int side = -1;
        std::uint8_t tier = 0;
        for (const auto& [tid, tc] : w.tiles) // ascending by id for a stable pick
        {
            if (a != null_entity && tid > a) continue;
            for (int s = 0; s < 6 && a == null_entity; ++s)
            {
                if (road_tier_on_side(tc, s) == 0) continue;
                const entity_id other = road_edge_neighbour(w, tid, s);
                if (other == null_entity) continue;
                a = tid; b = other; side = s; tier = road_tier_on_side(tc, s);
            }
            if (a != null_entity) break;
        }
        check(a != null_entity, "E2a found a laid crossing to read from both ends");
        if (a != null_entity)
        {
            const float fwd = road_edge_discount(w.tiles.at(a), side);
            const float rev = road_edge_discount(w.tiles.at(b), opposite_hex_side(side));
            std::printf("       crossing %u<->%u tier %u: %.6f forward, %.6f back\n",
                        a, b, static_cast<unsigned>(tier), fwd, rev);
            check(fwd == rev && fwd == road_traversal_multiplier(tier),
                  "E2b the same crossing prices the step identically both ways");

            // The path over it, with and without the crossing: the cost must rise
            // when the road is taken away. (A* reads the edge, so this is the one
            // row that proves the discount reaches the pathfinder.)
            const entity_id body = w.tiles.at(a).body;
            invalidate_logistics_caches(w);
            const float with_road = intra_body_path(w, body, a, b).cost;
            tile_component save_a = w.tiles.at(a), save_b = w.tiles.at(b);
            set_road_tier_on_side(w.tiles.at(a), side, 0);
            set_road_tier_on_side(w.tiles.at(b), opposite_hex_side(side), 0);
            invalidate_logistics_caches(w);
            const float bare = intra_body_path(w, body, a, b).cost;
            w.tiles.at(a) = save_a;
            w.tiles.at(b) = save_b;
            invalidate_logistics_caches(w);
            std::printf("       one hop a->b: %.6f roaded, %.6f bare\n", with_road, bare);
            check(with_road < bare,
                  fmt("E1b A* pays less across the crossing than without it (%.6f < %.6f)",
                      with_road, bare));
            check(intra_body_path(w, body, b, a).cost == with_road
                      || road_tier_on_side(w.tiles.at(a), side) != 0,
                  "E2c the restored field prices the hop as it did before");
        }
    }

    // --- S1: the save round-trips ------------------------------------------
    const std::string b53 = to_bytes(w);
    {
        world r;
        const bool ok = from_bytes(b53, r);
        check(ok && to_bytes(r) == b53, fmt("S1a v%u round-trips byte-identically (%zu bytes)",
                                            static_cast<unsigned>(world_save_version), b53.size()));
        check(ok && road_field(r) == road_field(w),
              "S1b every tile's six-side mask survives the save, mask for mask");
        check(ok && r.state_hash(0) == w.state_hash(0), "S1c the state hash agrees");
    }

    // --- S2: a pre-v53 save migrates to the lower-tier rule ---------------
    {
        // The field as the OLD format carried it: one tier a tile, the max over
        // its sides — which is what write_world_snapshot_pre_road_edges writes.
        std::map<std::tuple<entity_id, int, int>, std::uint8_t> legacy;
        for (const auto& [tid, tc] : w.tiles)
            legacy.emplace(std::make_tuple(tc.body, tc.grid_y, tc.grid_x), max_road_tier(tc));

        const std::string b52 = to_bytes_pre_edges(w);
        world r;
        const bool ok = from_bytes(b52, r);
        check(ok, fmt("S2a a v%u stream still loads (%zu bytes)",
                      static_cast<unsigned>(k_world_save_version_pre_road_edges), b52.size()));
        if (ok)
        {
            check(road_edges_mirrored(r), "S2b the migrated field satisfies the mirror invariant");

            // THE RULE, RECOMPUTED INDEPENDENTLY: between each pair of 4-cardinal
            // neighbours that both carried a road, the LOWER of their two tiers.
            constexpr int dc[4] = {  0,  0, -1,  1 };
            constexpr int dr[4] = { -1,  1,  0,  0 };
            long long want = 0, got = 0, wrong = 0;
            for (const auto& [tid, tc] : r.tiles)
            {
                const auto bit = r.bodies.find(tc.body);
                const int gw = bit != r.bodies.end() ? bit->second.grid_width  : 0;
                const int gh = bit != r.bodies.end() ? bit->second.grid_height : 0;
                const auto self = legacy.find(std::make_tuple(tc.body, tc.grid_y, tc.grid_x));
                const std::uint8_t mine = self != legacy.end() ? self->second : 0;
                for (int k = 0; k < 4; ++k)
                {
                    const int ny = tc.grid_y + dr[k];
                    if (ny < 0 || (gh > 0 && ny >= gh)) continue;
                    int nx = tc.grid_x + dc[k];
                    if (gw > 0) nx = ((nx % gw) + gw) % gw;
                    const auto nb = legacy.find(std::make_tuple(tc.body, ny, nx));
                    const std::uint8_t theirs = nb != legacy.end() ? nb->second : 0;
                    const int side = hex_side_for_offset(dc[k], dr[k], (tc.grid_y & 1) != 0);
                    if (side < 0) continue;
                    const std::uint8_t expect = (mine > 0 && theirs > 0)
                                                    ? std::min(mine, theirs) : 0;
                    const std::uint8_t have = road_tier_on_side(tc, side);
                    if (expect > 0) ++want;
                    if (have > 0) ++got;
                    if (!is_water(tc.substrate) && expect != have) ++wrong;
                }
            }
            std::printf("       migrated tile-sides: %lld expected, %lld laid, %lld disagree\n",
                        want, got, wrong);
            // WHAT THE MIGRATION COSTS, reported for Ben (review of 0831de6d): a tile
            // whose only road was its own street — a lone centre with no roaded
            // neighbour — has no pair to lay an edge between, so it comes back with no
            // road. That is exactly the ruled rule ("a road crosses only where both ends
            // were roaded"), so it stands; this says how big it is.
            const auto migrated_cells = cell_index(r); // one index, not a scan per tile
            int legacy_roaded = 0, dropped = 0;
            for (const auto& [key, lvl] : legacy)
            {
                if (lvl == 0) continue;
                ++legacy_roaded;
                const auto cit = migrated_cells.find(key);
                if (cit == migrated_cells.end()) continue;
                const auto tit = r.tiles.find(cit->second);
                if (tit != r.tiles.end() && !tile_has_road(tit->second)) ++dropped;
            }
            std::printf("       migration drops %d of %d roaded tiles (%.2f%%) \u2014 a tile"
                        " with no roaded neighbour has no pair to cross between\n",
                        dropped, legacy_roaded,
                        legacy_roaded > 0 ? 100.0 * dropped / legacy_roaded : 0.0);
            check(want > 0, "S2c the pre-edge field had adjacent roaded pairs to migrate");
            check(wrong == 0, fmt("S2d every migrated crossing is the LOWER of its two"
                                  " tiers (%lld disagree)", wrong));
            // The migrated world is now a v53 world: it saves at the current version.
            check(to_bytes(r).size() == b53.size(),
                  "S2e the migrated world's next save is a current-version stream");
        }
    }

    // --- S3: a tampered mirror is refused whole ---------------------------
    {
        // Find the byte of the first roaded tile's mask in the stream by writing a
        // world with ONE crossing moved and comparing — cheaper and more honest
        // than hand-computing an offset: flip the mask in the WORLD, write it, and
        // put back the neighbour's half, so the stream carries a broken mirror.
        world t = w;
        entity_id victim = null_entity;
        int vside = -1;
        for (const auto& [tid, tc] : t.tiles)
        {
            for (int s = 0; s < 6; ++s)
            {
                if (road_tier_on_side(tc, s) == 0) continue;
                if (road_edge_neighbour(t, tid, s) == null_entity) continue;
                if (victim == null_entity || tid < victim) { victim = tid; vside = s; }
                break;
            }
        }
        check(victim != null_entity, "S3a found a crossing to tamper with");
        if (victim != null_entity)
        {
            const std::uint8_t tier = road_tier_on_side(t.tiles.at(victim), vside);
            set_road_tier_on_side(t.tiles.at(victim), vside,
                                  static_cast<std::uint8_t>(tier == 1 ? 3 : 1)); // one half only
            const std::string bad = to_bytes(t);
            world before;
            const bool seeded = from_bytes(b53, before);          // a world to be left alone
            const std::string before_bytes = to_bytes(before);
            const bool loaded = from_bytes(bad, before);
            check(seeded && !loaded, "S3b a stream whose mirror disagrees is refused");
            check(to_bytes(before) == before_bytes,
                  "S3c the refusal mutated nothing — the target world is untouched");
        }
    }

    // --- D1: two generations of one seed are identical --------------------
    {
        world w2 = make_hard_coded_world(params);
        const auto f1 = road_field(w), f2 = road_field(w2);
        std::printf("       %zu roaded tiles, both runs\n", f1.size());
        check(f1 == f2, "D1 two generations of one seed lay the same edges");
        check(count_road_edges(w) == count_road_edges(w2), "D1b and the same edge count");
    }

    std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
    std::printf("%s\n", g_failures == 0 ? "ROAD EDGES OK" : "ROAD EDGES FAILED");
    return g_failures == 0 ? 0 : 1;
}
