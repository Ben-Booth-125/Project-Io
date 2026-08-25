#include "road_generation.hpp"

#include "components.hpp"
#include "logistics.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace {

// Road tiers (BL-172 three-tier ladder; BL-146 shipped local/trunk). road_traversal_multiplier
// = 1/(1+0.5*tier): Track(1) x0.67, Road(2) x0.50, Highway(3) x0.40. Generation assigns a tier
// per edge from the two centres' scales; the player may then upgrade any tile (BL-172 place_road).
constexpr std::uint8_t kTrack   = 1;
constexpr std::uint8_t kRoad    = 2;
constexpr std::uint8_t kHighway = 3;
// Scale thresholds: a Highway joins two major centres (City=3 .. Metropolis=5); a Road needs at
// least one Town+ (scale 2); everything smaller (and every cross-nation border link) is a Track.
constexpr int          kMajorScale = 3;
constexpr int          kMidScale   = 2;

// Water crossing (Sprint B2, narrowed by BL-516). Roads stay a LAND feature — no water tile is
// ever stamped, so a crossing always leaves a gap in the raster; what a port does with that gap
// is BL-188's, not this pass's. What this rule decides is which crossings the pass will lay a
// road TOWARD: a strait, where both shores are stamped so a coastal or island nation joins the
// lattice and the corridor resumes on the far side — never open sea, where stamping would
// scatter disconnected road fragments across distant shores.
//
// BL-516 MADE THE DISTINCTION DATA. Before water had kinds, "is this a strait" could only be
// INFERRED from the length of the contiguous water run, because a three-tile clip across the
// corner of an ocean and a three-tile channel between two shores were the same tiles. Now they
// are not: a channel is made of `coast` (water with land beside it) and the open sea is
// `ocean`. So the rule is the conjunction — a crossing is a strait when it is SHORT and made
// of SHORE. The length bound stays because it is a second, independent claim (a road bridges a
// channel, it does not run twenty tiles along a shelf), and dropping it would let a long
// shallow shelf read as a crossing.
constexpr int          kMaxCrossingTiles = 3;

// BL-620 (road generation scales to density). At demography-derived density (BL-610) the home
// body carries ~1,500-2,000 centres, almost all villages; the old pass ran pairwise A* between
// ALL centres per nation (O(n^2) A*, ~50k searches) plus cross-nation ALL-PAIRS A* for every
// border link (~100k more), and became ~56s of a ~58s world build. The restructure: the
// backbone lattice is built over TOWNS-AND-UP only (scale >= kMidScale, exactly the old MST +
// relative-neighbour shape), and each village joins LOCALLY — one Track spur to its nearest
// already-roaded same-nation tile, chosen from a grid-distance-prefiltered candidate set,
// never all-pairs. Low-stratum settlements get spur tracks, not lattice membership, which is
// also the historically honest shape.
//
// A village tries the kSpurCandidates nearest targets (a candidate can be unreachable, or its
// route can cross open sea and be refused by the strait rule) and gives up past
// kMaxSpurGridDist tiles — an isolated village keeps only its local street.
constexpr int kSpurCandidates  = 3;
constexpr int kMaxSpurGridDist = 40;

// Border links keep their contract — one Track between the nearest reachable centre pair of
// each territorially-adjacent nation pair — but the nearest pair is found by running A* over
// only the kBorderProbePairs closest pairs by wrapped grid distance, not over all n_a * n_b
// pairs. Terrain weight varies far less than a factor of kBorderProbePairs, so the true
// cheapest pair is in the probe set in practice; the probe order is deterministic
// (distance^2, then tile ids).
constexpr int kBorderProbePairs = 24;

// BL-618 (roads scale with qualification; LOGISTICS.md § Roads): the nation's qualification
// fraction (nation_component::qualification, POPULATION.md § Qualification) modulates the
// backbone. First-cut mapping, tune-not-restructure:
//   - Tier promotion is GATED: a Highway needs two major endpoints AND qualification >=
//     kHighwayQualification; a Road needs one Town+ endpoint AND qualification >=
//     kRoadQualification; a gated-out edge demotes one rung, never disappears. The floor
//     (0.05, a nation that never industrialised) sits below both gates, so a pre-industrial
//     world generates an all-Track lattice — engineering above the track is a qualified-
//     labour product. 0.10 clears the late-industrialiser base (0.12); 0.30 needs the early
//     base (0.35), or the mid base (0.22) with broad industrialisation.
//   - Redundancy loops are RATIONED: the relative-neighbour edges are kept cheapest-first,
//     floor(count * qualification / kFullRedundancyQualification) of them (clamped to all),
//     so a low-qualification nation keeps the MST but few loops.
// Spurs, local streets and border links are Tracks already and are not modulated.
constexpr float kRoadQualification           = 0.10f;
constexpr float kHighwayQualification        = 0.30f;
constexpr float kFullRedundancyQualification = 0.40f;

// Tier for a backbone edge between two centres of the given scales (BL-172), gated by the
// owning nation's qualification (BL-618).
std::uint8_t edge_tier(int scale_a, int scale_b, float qualification)
{
    if (scale_a >= kMajorScale && scale_b >= kMajorScale
        && qualification >= kHighwayQualification)
        return kHighway;
    if ((scale_a >= kMidScale || scale_b >= kMidScale)
        && qualification >= kRoadQualification)
        return kRoad;
    return kTrack;
}

constexpr float kUnreachable = std::numeric_limits<float>::max();

/// A road node: one population centre on the body, tagged with its nation, scale and
/// grid position (BL-620: the spur / border prefilters need coordinates without a
/// tiles-map lookup per comparison).
struct road_node
{
    entity_id centre;
    entity_id tile;
    entity_id nation;
    int       scale;
    int       gx;
    int       gy;
};

entity_id nation_of(const world& w, entity_id tile)
{
    const auto it = w.tile_to_nation.find(tile);
    return (it != w.tile_to_nation.end()) ? it->second : null_entity;
}

/// True if every contiguous water run along @p p is a strait: SHORT (<= kMaxCrossingTiles) and
/// made entirely of SHORE (`coast`, or a lake — enclosed water a causeway crosses; never open
/// `ocean`). See kMaxCrossingTiles for why both halves are needed.
bool crossings_are_straits(const world& w, const logistics_path& p)
{
    int run = 0;
    for (const entity_id t : p.tiles)
    {
        const auto it = w.tiles.find(t);
        if (it == w.tiles.end() || !is_water(it->second.substrate))
        {
            run = 0;
            continue;
        }
        // BL-516: one open-sea tile anywhere in the run disqualifies it outright,
        // however short the run is. That is the case the old length-only rule could
        // not see — a path clipping the corner of an ocean in three tiles.
        if (is_open_ocean(it->second.substrate))
            return false;
        if (++run > kMaxCrossingTiles)
            return false;
    }
    return true;
}

/// Stamp a road of @p level along the A* path between two tiles, taking the max on
/// overlap and skipping WATER OF EVERY KIND (roads are a land feature). No-op if unreachable, or
/// if the route crosses open sea rather than a strait (see kMaxCrossingTiles). Returns whether
/// the edge was laid; when @p stamped is given, appends every land tile of the route (BL-620:
/// the village-spur pass feeds these back as future spur targets).
bool stamp_edge(world& w, entity_id body, entity_id ta, entity_id tb, std::uint8_t level,
                std::vector<entity_id>* stamped = nullptr)
{
    const logistics_path& p = intra_body_path(w, body, ta, tb);
    if (!p.reachable)
        return false;
    if (p.crosses_ocean && !crossings_are_straits(w, p))
        return false;
    for (const entity_id t : p.tiles)
    {
        const auto it = w.tiles.find(t);
        if (it == w.tiles.end() || is_water(it->second.substrate)) // BL-516
            continue;
        it->second.road_level = std::max(it->second.road_level, level);
        if (stamped)
            stamped->push_back(t);
    }
    return true;
}

} // namespace

void generate_roads(world& w, entity_id body)
{
    // Grid geometry (BL-620: the spur and border prefilters measure wrapped grid
    // distance, so they need the body's dimensions up front).
    const auto bit = w.bodies.find(body);
    const int  gw  = (bit != w.bodies.end()) ? std::max(1, bit->second.grid_width) : 0;
    const int  gh  = (bit != w.bodies.end()) ? std::max(1, bit->second.grid_height) : 0;

    // Squared grid distance with the east-west column wrap (cylinder topology).
    auto wrapped_d2 = [&](int ax, int ay, int bx, int by) -> long long {
        int dx = std::abs(ax - bx);
        if (gw > 0)
            dx = std::min(dx, gw - dx);
        const int dy = ay - by;
        return static_cast<long long>(dx) * dx + static_cast<long long>(dy) * dy;
    };

    // 1. Collect this body's centres, ordered by tile id so the whole pass is
    //    independent of the unordered_map iteration order (determinism).
    std::vector<road_node> nodes;
    for (const auto& [centre, tile] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tile);
        if (tit == w.tiles.end() || tit->second.body != body)
            continue;
        int scale = 1;
        if (const auto pit = w.population_centres.find(centre); pit != w.population_centres.end())
            scale = pit->second.scale;
        nodes.push_back({ centre, tile, nation_of(w, tile), scale,
                          tit->second.grid_x, tit->second.grid_y });
    }
    std::sort(nodes.begin(), nodes.end(),
              [](const road_node& a, const road_node& b) { return a.tile < b.tile; });
    if (nodes.empty())
        return;

    // 1b. Every centre's own tile carries at least a Track (Sprint B2 cut 1). Previously a
    //     nation with a single centre on this body fell straight through the backbone pass
    //     below and ended generation with no roaded tile anywhere in its territory — the
    //     census measured that as the ONLY cause of a road-less nation. A settlement has
    //     streets whether or not it has a neighbour to drive to, and this puts the nation on
    //     the lattice, gives the cross-nation border link below a roaded endpoint to reach,
    //     and gives the player's place_road something to extend from. Uniform (no
    //     single-centre special case) and order-independent: std::max means a centre that
    //     later sits on a Highway keeps the higher tier.
    for (const road_node& node : nodes)
    {
        const auto it = w.tiles.find(node.tile);
        if (it != w.tiles.end() && !is_water(it->second.substrate)) // BL-516
            it->second.road_level = std::max(it->second.road_level, kTrack);
    }

    // Group node indices by nation (std::map → nation ids ascending, deterministic;
    // members inherit the tile-id sort above, so each list is tile-ordered).
    std::map<entity_id, std::vector<int>> by_nation;
    for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
        by_nation[nodes[i].nation].push_back(i);

    // 2+3. Per nation: a BACKBONE over towns-and-up (MST + relative-neighbour redundancy,
    //      the pre-BL-620 shape, now over the town set only), then each village joins the
    //      lattice locally with a Track spur.
    for (const auto& [nation, members] : by_nation)
    {
        // BL-618: the nation's qualification fraction gates tier promotion and rations
        // the redundancy loops below. Unowned centres (null nation) read 0 — the floor.
        float qualification = 0.0f;
        if (const auto nit = w.nations.find(nation); nit != w.nations.end())
            qualification = nit->second.qualification;

        // Spur target set: this nation's already-roaded tiles — every member centre's own
        // street (stamped in 1b) plus, below, the backbone raster and earlier spurs. The
        // std::set only dedupes; candidate order never depends on it (the nearest-target
        // scan is over the vector with a strict (distance^2, tile-id) comparison).
        struct spur_target { entity_id tile; int gx; int gy; };
        std::vector<spur_target> targets;
        std::set<entity_id>      target_seen;
        auto add_target = [&](entity_id t) {
            if (!target_seen.insert(t).second)
                return;
            const auto it = w.tiles.find(t);
            if (it == w.tiles.end())
                return;
            targets.push_back({ t, it->second.grid_x, it->second.grid_y });
        };
        for (const int m : members)
            add_target(nodes[m].tile);

        // --- Backbone: towns-and-up only (BL-620) ---------------------------------
        std::vector<int> towns;
        for (const int m : members)
            if (nodes[m].scale >= kMidScale)
                towns.push_back(m);
        const int n = static_cast<int>(towns.size());
        if (n >= 2)
        {
            // Pairwise terrain-weighted A* costs (symmetric matrix); collect the
            // reachable pairs as candidate edges (a,b index into `towns`).
            struct edge { float cost; int a; int b; };
            std::vector<edge> edges;
            std::vector<std::vector<float>> d(n, std::vector<float>(n, kUnreachable));
            for (int a = 0; a < n; ++a)
                for (int b = a + 1; b < n; ++b)
                {
                    const logistics_path& p =
                        intra_body_path(w, body, nodes[towns[a]].tile, nodes[towns[b]].tile);
                    const float c = p.reachable ? p.cost : kUnreachable;
                    d[a][b] = d[b][a] = c;
                    if (p.reachable)
                        edges.push_back({ c, a, b });
                }

            // Deterministic edge order: cost, then lo-tile-id, then hi-tile-id.
            auto lo_tile = [&](const edge& e) { return std::min(nodes[towns[e.a]].tile, nodes[towns[e.b]].tile); };
            auto hi_tile = [&](const edge& e) { return std::max(nodes[towns[e.a]].tile, nodes[towns[e.b]].tile); };
            std::sort(edges.begin(), edges.end(), [&](const edge& e, const edge& f) {
                if (e.cost != f.cost) return e.cost < f.cost;
                if (lo_tile(e) != lo_tile(f)) return lo_tile(e) < lo_tile(f);
                return hi_tile(e) < hi_tile(f);
            });

            // Kruskal MST with union-find.
            std::vector<int> parent(n);
            for (int i = 0; i < n; ++i) parent[i] = i;
            std::function<int(int)> find = [&](int x) {
                while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
                return x;
            };
            std::vector<std::pair<int, int>> chosen;
            std::vector<std::vector<bool>> in_mst(n, std::vector<bool>(n, false));
            for (const edge& e : edges)
            {
                const int ra = find(e.a), rb = find(e.b);
                if (ra != rb)
                {
                    parent[ra] = rb;
                    chosen.emplace_back(e.a, e.b);
                    in_mst[e.a][e.b] = in_mst[e.b][e.a] = true;
                }
            }

            // Relative-neighbour redundancy: keep a non-MST edge (a,b) only if no third
            // town c is closer to BOTH endpoints than they are to each other — i.e.
            // max(d[a][c], d[b][c]) < d[a][b] for some c disqualifies it. Adds the short
            // loops a bare MST misses without cluttering the lattice.
            std::vector<std::pair<int, int>> loops;
            for (const edge& e : edges)
            {
                if (in_mst[e.a][e.b])
                    continue;
                bool keep = true;
                for (int c = 0; c < n; ++c)
                {
                    if (c == e.a || c == e.b)
                        continue;
                    if (std::max(d[e.a][c], d[e.b][c]) < d[e.a][e.b])
                    {
                        keep = false;
                        break;
                    }
                }
                if (keep)
                    loops.emplace_back(e.a, e.b);
            }
            // BL-618: ration the loops by qualification, cheapest-first (`loops` inherits
            // the deterministic (cost, lo, hi) edge order). The MST is never rationed —
            // a nation's towns connect regardless; loops are the qualified-labour luxury.
            const float loop_frac =
                std::clamp(qualification / kFullRedundancyQualification, 0.0f, 1.0f);
            const int loops_kept =
                static_cast<int>(static_cast<float>(loops.size()) * loop_frac);
            for (int i = 0; i < loops_kept; ++i)
                chosen.push_back(loops[static_cast<std::size_t>(i)]);

            // Rasterise: tier by the two towns' scales gated by qualification (BL-618),
            // and feed this nation's stamped tiles into the spur target set.
            std::vector<entity_id> stamped;
            for (const auto& [a, b] : chosen)
            {
                const std::uint8_t tier =
                    edge_tier(nodes[towns[a]].scale, nodes[towns[b]].scale, qualification);
                stamped.clear();
                stamp_edge(w, body, nodes[towns[a]].tile, nodes[towns[b]].tile, tier, &stamped);
                for (const entity_id t : stamped)
                    if (nation_of(w, t) == nation)
                        add_target(t);
            }
        }

        // --- Village spurs (BL-620) -----------------------------------------------
        // Villages in tile-id order (members are already sorted), each laying one Track
        // to its nearest same-nation roaded tile. A stamped spur's tiles join the target
        // set, so later villages branch off earlier feeders rather than each running its
        // own long track — the incremental order is deterministic because the walk is.
        if (gw > 0)
        {
            constexpr long long kMaxSpurD2 =
                static_cast<long long>(kMaxSpurGridDist) * kMaxSpurGridDist;
            std::vector<entity_id> stamped;
            for (const int m : members)
            {
                if (nodes[m].scale >= kMidScale)
                    continue; // towns are backbone members, not spur clients
                // The kSpurCandidates nearest targets by (distance^2, tile id), capped.
                struct cand { long long d2; entity_id tile; };
                std::array<cand, kSpurCandidates> best;
                best.fill({ kMaxSpurD2 + 1, null_entity });
                for (const spur_target& t : targets)
                {
                    if (t.tile == nodes[m].tile)
                        continue;
                    const long long d2 = wrapped_d2(nodes[m].gx, nodes[m].gy, t.gx, t.gy);
                    if (d2 > kMaxSpurD2)
                        continue;
                    cand c{ d2, t.tile };
                    for (int s = 0; s < kSpurCandidates; ++s)
                        if (best[s].tile == null_entity || c.d2 < best[s].d2
                            || (c.d2 == best[s].d2 && c.tile < best[s].tile))
                            std::swap(c, best[s]);
                }
                for (int s = 0; s < kSpurCandidates; ++s)
                {
                    if (best[s].tile == null_entity)
                        break;
                    stamped.clear();
                    if (!stamp_edge(w, body, nodes[m].tile, best[s].tile, kTrack, &stamped))
                        continue; // unreachable or open-sea route: try the next-nearest
                    for (const entity_id t : stamped)
                        if (nation_of(w, t) == nation)
                            add_target(t);
                    break;
                }
            }
        }
    }

    // 5. Border links: one local road between the nearest centre pair of each
    //    territorially-adjacent nation pair, connecting the per-nation lattices.
    if (bit == w.bodies.end())
        return;
    const std::vector<entity_id>& grid = body_tile_grid(w, body); // grid_y*gw + grid_x

    // Territorial adjacency (sorted nation pair → adjacent), from a 4-cardinal
    // neighbour scan with east-west column wrap (matching the logistics topology).
    std::set<std::pair<entity_id, entity_id>> adjacency;
    auto note_adjacent = [&](entity_id na, entity_id nb) {
        if (na == null_entity || nb == null_entity || na == nb)
            return;
        adjacency.insert({ std::min(na, nb), std::max(na, nb) });
    };
    //
    // Cross-water adjacency (Sprint B2 cut 3): the scan used to look only at the immediate
    // 4-cardinal neighbour, so two nations facing each other across a single strait tile were
    // never "adjacent" and no border link was ever attempted — the shape that leaves a coastal
    // or island nation off the continental lattice entirely. The walk below instead scans each
    // row and column for the NEXT owned tile, tolerating a gap of up to kMaxCrossingTiles
    // unowned tiles (water, or unclaimed land) between them. A gap of 0 is the old direct-
    // neighbour case, so this strictly widens the relation rather than replacing it. The
    // stamp_edge strait bound then decides whether a road can actually follow the crossing.
    //
    // Deterministic: fixed row-major scan order, and `adjacency` is a std::set of sorted pairs,
    // so neither the discovery order nor the number of times a pair is found can vary the
    // result.
    auto scan_line = [&](auto at, int len, bool wraps) {
        for (int i = 0; i < len; ++i)
        {
            const entity_id na = nation_of(w, at(i));
            if (na == null_entity)
                continue;
            for (int step = 1; step <= kMaxCrossingTiles + 1; ++step)
            {
                const int j = i + step;
                if (j >= len && !wraps)
                    break;
                const entity_id nb = nation_of(w, at(wraps ? (j % len) : j));
                if (nb == null_entity)
                    continue;
                note_adjacent(na, nb);
                break; // the first owned tile past the gap is the neighbour; stop there
            }
        }
    };
    for (int r = 0; r < gh; ++r)
        scan_line([&](int c) { return grid[static_cast<std::size_t>(r) * gw + c]; }, gw, true);
    for (int cc = 0; cc < gw; ++cc)
        scan_line([&](int r) { return grid[static_cast<std::size_t>(r) * gw + cc]; }, gh, false);

    for (const auto& [na, nb] : adjacency)
    {
        const auto ia = by_nation.find(na);
        const auto ib = by_nation.find(nb);
        if (ia == by_nation.end() || ib == by_nation.end())
            continue;

        // BL-620 prefilter: rank all cross pairs by wrapped grid distance (cheap integer
        // work), then A* only the kBorderProbePairs closest. The pair chosen is the
        // cheapest-by-A* among the probe set — deterministic: the probe order is
        // (distance^2, lo tile, hi tile) and the cost comparison is strict, so the first
        // minimum is canonical.
        struct border_pair { long long d2; entity_id ta; entity_id tb; };
        std::vector<border_pair> probe;
        probe.reserve(ia->second.size() * ib->second.size());
        for (const int a : ia->second)
            for (const int b : ib->second)
                probe.push_back({ wrapped_d2(nodes[a].gx, nodes[a].gy, nodes[b].gx, nodes[b].gy),
                                  nodes[a].tile, nodes[b].tile });
        std::sort(probe.begin(), probe.end(), [](const border_pair& x, const border_pair& y) {
            if (x.d2 != y.d2) return x.d2 < y.d2;
            if (x.ta != y.ta) return x.ta < y.ta;
            return x.tb < y.tb;
        });
        if (static_cast<int>(probe.size()) > kBorderProbePairs)
            probe.resize(kBorderProbePairs);

        float best = kUnreachable;
        entity_id best_a = null_entity, best_b = null_entity;
        for (const border_pair& bp : probe)
        {
            const logistics_path& p = intra_body_path(w, body, bp.ta, bp.tb);
            if (p.reachable && p.cost < best)
            {
                best = p.cost;
                best_a = bp.ta;
                best_b = bp.tb;
            }
        }
        if (best_a != null_entity)
            stamp_edge(w, body, best_a, best_b, kTrack);
    }

    // The A* cost cache (world.astar_cost_cache) was populated road-free while this
    // pass measured centre-pair costs to lay the network out — correct for the MST
    // decision, but now stale: the stamped roads lower those same lanes' costs. Drop
    // it so the gameplay dispatch loop recomputes against the final road_level field
    // (the "invalidated when road_level changes" contract, world.hpp). The raster
    // index is road-independent and stays. Generation-time, so clearing all is cheap.
    w.astar_cost_cache.clear();
    // Roads change traversal cost, so they change reach too (BL-323 S2).
    w.body_reach_cost.clear();
}

