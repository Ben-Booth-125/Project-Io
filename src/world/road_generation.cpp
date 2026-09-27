#include "road_generation.hpp"

#include "components.hpp"
#include "hard_coded_world.hpp" // generation_progress -- the BL-1072 loading-bar tap
#include "logistics.hpp"
#include "ocean_currents.hpp" // BL-1098: the current field a lane's walk is priced with

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#include <queue>
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

// BL-618 as amended by BL-621 (era-relative road gates; Ben, 2026-08-25, ruling on
// NR-641): the gates read the nation's PERCENTILE among the world's nations — mid-rank
// on ties — never the absolute qualification fraction. Rationale: qualification's
// seeding is industrialisation timing, which is zero-signal at an antiquity epoch —
// every nation ties at the floor, and the absolute gates produced an all-Track default
// world. Relative standing is era-portable: an all-tied antiquity world grades every
// nation at 0.5 — Roads for every Town+ backbone edge, Highways for none, the
// Roman-roads-analogue backbone Ben asked to keep — while a spread industrial world
// promotes its leaders to Highways and demotes its laggards to Track-only lattices.
//   - Tier promotion is GATED: a Highway needs two major endpoints AND percentile >=
//     kHighwayPercentile; a Road needs one Town+ endpoint AND percentile >=
//     kRoadPercentile; a gated-out edge demotes one rung, never disappears.
//   - Redundancy loops are RATIONED cheapest-first: floor(count * percentile) kept, so
//     the median nation keeps half its loops and the MST is never rationed.
// Spurs, local streets and border links are Tracks already and are not modulated.
// BL-1119: the loops rationed are the ones the DETOUR TEST admits (kDetourRatio,
// road_generation.hpp) — the relative-neighbour redundancy edges that laid the lattice
// are gone. A delegated reading that keeps BL-618/BL-621 intact; Ben may overturn it.
constexpr float kRoadPercentile    = 0.40f;
constexpr float kHighwayPercentile = 0.80f;

// Tier for a backbone edge between two centres of the given scales (BL-172), gated by the
// owning nation's qualification PERCENTILE (BL-618/BL-621).
std::uint8_t edge_tier(int scale_a, int scale_b, float percentile)
{
    if (scale_a >= kMajorScale && scale_b >= kMajorScale
        && percentile >= kHighwayPercentile)
        return kHighway;
    if ((scale_a >= kMidScale || scale_b >= kMidScale)
        && percentile >= kRoadPercentile)
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

// BL-768 — THE ANCIENT TIER RULE. Its own rule, not the industrial one: the
// gates above read a nation's qualification percentile, a field derived from
// industrialisation timing that does not exist in antiquity and has no spread to
// read when it does. What an ancient corridor has instead is how hard it was
// USED and what its two ends BUILT.
//
// kAncientRoadUses is MEASURED, not chosen, and the distribution turned out to
// be genuinely bimodal rather than merely skewed. `history_sweep 8 --epoch 1960`
// (2026-09-06) records 3,185 distinct corridors over eight worlds: 3,119 walked
// exactly ONCE — a founding party reaches new ground and never comes back — then
// six corridors in the whole sweep at two or three uses, then a tail of 60
// walked four or more, the busiest 218 times. There is a real gap, so the
// threshold is not a percentile dressed up as a constant: 4 is where the tail
// begins, and a Road therefore means "this line carried repeat traffic" rather
// than "this line existed". `history_sweep`'s BL-768 block prints the histogram,
// so a re-measure is a row to read rather than an argument to reopen.
constexpr int kAncientRoadUses = 4;

/// Tier for one recorded corridor. Integer throughout, matching the sim's own
/// fixed-point idiom rather than the float gates the modern pass uses.
///
/// The works promotion needs BOTH ends. A corridor is only as good as its worse
/// terminus — a paved trunk with a station at one end and nothing at the other
/// is a road that stops — and requiring both is what stops a single Way Station
/// promoting every line radiating out of one region.
std::uint8_t ancient_tier(int uses, int reach_a, int reach_b)
{
    std::uint8_t t = (uses >= kAncientRoadUses) ? kRoad : kTrack;
    if (reach_a > 0 && reach_b > 0 && t < kHighway)
        t = static_cast<std::uint8_t>(t + 1);
    return t;
}

/// The network's route cost between towns @p from and @p to over @p adj (the tree plus
/// the loops admitted so far; each link weighted by its DIRECT A* cost), or a value
/// above @p bound when that route costs more than @p bound or does not exist. A
/// bounded Dijkstra over the town graph — n is a nation's towns, so this is cheap
/// integer-indexed work, never a tile search.
///
/// Deterministic: adjacency lists are appended in the fixed edge order, the queue
/// orders on (cost, node index), and a node's settled cost is final.
double network_route_cost(const std::vector<std::vector<std::pair<int, double>>>& adj,
                          int from, int to, double bound)
{
    const double over = bound * 2.0 + 1.0;
    if (from == to)
        return 0.0;
    std::vector<double> dist(adj.size(), std::numeric_limits<double>::infinity());
    std::vector<char>   done(adj.size(), 0);
    using item = std::pair<double, int>;
    std::priority_queue<item, std::vector<item>, std::greater<item>> pq;
    dist[static_cast<std::size_t>(from)] = 0.0;
    pq.push({ 0.0, from });
    while (!pq.empty())
    {
        const auto [dc, u] = pq.top();
        pq.pop();
        if (done[static_cast<std::size_t>(u)])
            continue;
        done[static_cast<std::size_t>(u)] = 1;
        if (dc > bound)
            return over; // every remaining route is longer still
        if (u == to)
            return dc;
        for (const auto& [v, c] : adj[static_cast<std::size_t>(u)])
        {
            const double nd = dc + c;
            if (nd < dist[static_cast<std::size_t>(v)])
            {
                dist[static_cast<std::size_t>(v)] = nd;
                pq.push({ nd, v });
            }
        }
    }
    return over; // no route at all: a different component of the tree
}

} // namespace

long long village_spur_size(const world& w, entity_id centre)
{
    if (const auto sit = w.gen_carve_centres.find(centre); sit != w.gen_carve_centres.end())
        return static_cast<long long>(sit->second.key);
    if (const auto pit = w.population_centres.find(centre); pit != w.population_centres.end())
        return static_cast<long long>(pit->second.population) * 1000;
    return 0;
}

void generate_roads(world& w, entity_id body, generation_progress* progress,
                    long long spur_floor_heads, road_generation_stats* stats,
                    road_generation_trace* trace)
{
    road_generation_stats st{}; // BL-1119 D1: filled as the pass goes, copied out at the end
    // BL-1119 round 4: floods built so far — the per-site deltas in the stats (write-only).
    const auto floods_now = [&w]() {
        return static_cast<long long>(w.logistics_flood_fields.size());
    };
    // BL-1119 round 3: a laid route, whole, into the caller's trace (write-only). The
    // path is the one stamp_edge just laid — the pair cache answers it again unchanged.
    const auto record = [&](road_generation_trace::kind k, entity_id from, entity_id to,
                            entity_id nation) {
        if (trace == nullptr)
            return;
        road_generation_trace::route r;
        r.k      = k;
        r.from   = from;
        r.to     = to;
        r.nation = nation;
        r.path   = intra_body_path(w, body, from, to).tiles;
        trace->routes.push_back(std::move(r));
    };
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
    {
        if (stats != nullptr) *stats = st;
        return;
    }

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

    // BL-621: each nation's qualification PERCENTILE among the world's nations,
    // mid-rank on ties — (count below + half the tied group) / N. Computed once,
    // over ascending nation ids (float-free ranking, but the order is fixed
    // anyway so a future float term stays deterministic). All-tied -> 0.5 each.
    std::map<entity_id, float> qual_percentile;
    {
        std::vector<std::pair<entity_id, float>> qs;
        for (const auto& [nid, nc] : w.nations)
            qs.push_back({ nid, nc.qualification });
        std::sort(qs.begin(), qs.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        const int nn = static_cast<int>(qs.size());
        for (const auto& [nid, q] : qs)
        {
            int below = 0, tied = 0;
            for (const auto& [nid2, q2] : qs)
            {
                below += q2 < q;
                tied  += q2 == q;
            }
            qual_percentile[nid] =
                nn > 0 ? (static_cast<float>(below) + 0.5f * static_cast<float>(tied))
                             / static_cast<float>(nn)
                       : 0.0f;
        }
    }

    // BL-1072 -- THE LOADING BAR'S COUNT. This pass is most of a world build's
    // wall clock (30-57 s Release, measured 2026-09-24), and nearly all of that
    // is the village spur walk, so the bar counts the work the loops below
    // naturally walk: one unit per backbone A* pair, one per village, and
    // `kBorderUnitsPerNation` per nation for the border links (reported in
    // proportion as that loop goes -- measured at about a dozen villages' worth
    // of A* per nation, 1.1 s against 5,873 villages' 27.5 s on seed 28).
    // Counted from the same `by_nation` the loops read, so it ends exactly on
    // its total. WRITE-ONLY: nothing below reads it back.
    //
    // BL-1119: the floor is decided here, once per village, and the count reads
    // it — a village below the floor lays no spur, so it is no unit of work.
    std::vector<char> spurs(nodes.size(), 0); // 1 = a village at/above the spur floor
    // 1 = a centre ON its nation's network: every town, and a village once its spur has
    // reached a tile joined to a town (the spur targets are only such tiles, BL-1119
    // round 3). The border links end only on these (Ben, 2026-09-25).
    std::vector<char> on_network(nodes.size(), 0);
    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        if (nodes[i].scale >= kMajorScale)
            ++st.majors;
        if (nodes[i].scale >= kMidScale)
        {
            on_network[i] = 1;
            continue;
        }
        ++st.villages;
        if (village_spur_size(w, nodes[i].centre) >= spur_floor_heads)
            spurs[i] = 1;
        else
            ++st.villages_below_floor;
    }

    constexpr long long kBorderUnitsPerNation = 24; // fitted 2026-09-25: 21 / 18 / 45 on seeds 0 / 28 / 46 (BL-1119)
    long long units_done = 0, units_total = 0;
    {
        for (const auto& [nation, members] : by_nation)
        {
            long long t = 0, v = 0;
            for (const int m : members)
            {
                if (nodes[m].scale >= kMidScale) ++t;
                else if (spurs[static_cast<std::size_t>(m)]) ++v;
            }
            if (t >= 2) st.units_backbone += t * (t - 1) / 2;
            if (gw > 0) st.units_spurs += v;
        }
        st.nation_groups = static_cast<int>(by_nation.size());
        st.units_border  = kBorderUnitsPerNation * static_cast<long long>(by_nation.size());
        units_total     = st.units_backbone + st.units_spurs + st.units_border;
    }
    const auto report_units = [&](long long done) {
        if (progress == nullptr || units_total <= 0) return;
        // Scaled into int range for the sink; exact at both ends.
        constexpr long long cap = 1000000;
        const long long total = std::min(units_total, cap);
        progress->report_sub(static_cast<int>(done * total / units_total),
                             static_cast<int>(total));
    };
    report_units(0);

    // 2+3. Per nation: a BACKBONE over towns-and-up (the Kruskal tree, then the loops the
    //      detour test admits — BL-1119), then each village at or above the spur floor
    //      joins the network locally with a Track spur.
    for (const auto& [nation, members] : by_nation)
    {
        // BL-618/BL-621: the nation's qualification PERCENTILE gates tier promotion
        // and rations the redundancy loops below. Unowned centres (null nation)
        // read 0 — below every gate, Track-only.
        float qualification = 0.0f;
        if (const auto pit = qual_percentile.find(nation); pit != qual_percentile.end())
            qualification = pit->second;

        // Spur target set: this nation's tiles ALREADY JOINED TO THE BACKBONE (BL-1119
        // round 3; LOGISTICS.md § 4) — every town's own street, then below the backbone
        // raster, then each spur once it has reached one of these. The std::set only
        // dedupes; candidate order never depends on it (the nearest-target scan is over
        // the vector with a strict (distance^2, tile-id) comparison).
        //
        // No village's street is a target until its own spur has joined, below-floor or
        // not: a spur ending on an unjoined village joined nothing, and two such villages
        // could each spur onto the other and both read as on the network while neither
        // touched a town. So a village is on its nation's network exactly when its road
        // reaches a town, and the spur walk can only extend what is already joined.
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
            if (nodes[m].scale >= kMidScale)
                add_target(nodes[m].tile);

        // --- Backbone: towns-and-up only (BL-620) ---------------------------------
        std::vector<int> towns;
        for (const int m : members)
            if (nodes[m].scale >= kMidScale)
                towns.push_back(m);
        const int n = static_cast<int>(towns.size());
        st.towns += n;
        if (n >= 2)
        {
            // Pairwise terrain-weighted A* costs; collect the reachable pairs as
            // candidate edges (a,b index into `towns`).
            //
            // DIRECTION (BL-1119 round 4; a path is directed and answered from its
            // destination's flood, BL-1126): a town pair is priced FROM THE LOWER TILE ID
            // TO THE HIGHER — `towns` is in tile order and a < b — and the link the tree or
            // the detour test chooses is laid in that same direction, so laying it reads
            // the cached answer. The nation's n towns cost n - 1 floods (every town but the
            // lowest is a destination), the fewest any direction can: every pair needs one
            // of its two ends flooded. The detour test's direct costs are these same
            // answers; it asks nothing of its own.
            struct edge { float cost; int a; int b; };
            std::vector<edge> edges;
            const long long floods_pairs_from = floods_now();
            for (int a = 0; a < n; ++a)
                for (int b = a + 1; b < n; ++b)
                {
                    const logistics_path& p =
                        intra_body_path(w, body, nodes[towns[a]].tile, nodes[towns[b]].tile);
                    // ONLY A LINK THAT CAN BE LAID IS A CANDIDATE (NR-945, LOGISTICS.md
                    // § 4). The flood crosses water at sea-leg cost, so an open-sea pair is
                    // "reachable" — and stamp_edge refuses it. Kept as a candidate it became
                    // a tree link that stranded a town, or an admitted loop in the test
                    // network that refused the real land loops beside it. Filtered here with
                    // stamp_edge's own rule, so a nation the sea divides builds one tree per
                    // landmass.
                    if (p.reachable)
                    {
                        if (p.crosses_ocean && !crossings_are_straits(w, p))
                            ++st.candidates_unlayable;
                        else
                            edges.push_back({ p.cost, a, b });
                    }
                    report_units(++units_done); // BL-1072
                }
            st.floods_town_pairs += floods_now() - floods_pairs_from;

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
            // The network as the detour test sees it: towns joined by the links laid so
            // far, each weighted by its DIRECT A* cost (BL-1119). Appended in edge order.
            std::vector<std::vector<std::pair<int, double>>> net(static_cast<std::size_t>(n));
            for (const edge& e : edges)
            {
                const int ra = find(e.a), rb = find(e.b);
                if (ra != rb)
                {
                    parent[ra] = rb;
                    chosen.emplace_back(e.a, e.b);
                    in_mst[e.a][e.b] = in_mst[e.b][e.a] = true;
                    net[static_cast<std::size_t>(e.a)].push_back({ e.b, static_cast<double>(e.cost) });
                    net[static_cast<std::size_t>(e.b)].push_back({ e.a, static_cast<double>(e.cost) });
                    ++st.mst_links;
                }
            }

            // THE DETOUR TEST (BL-1119; LOGISTICS.md § 4). The tree comes first, whole;
            // then every other reachable town pair, cheapest-first in the same (cost, lo,
            // hi) order, is laid only when the network's route between its two towns costs
            // more than kDetourRatio x its direct route. An admitted loop joins the network
            // at once, so a later candidate running beside it finds a serviceable route and
            // is refused — a second road beside a serviceable one is never built, and a loop
            // exists only where the tree forces a long way round. This replaces the
            // relative-neighbour redundancy edges, which laid the lattice.
            std::vector<std::pair<int, int>> loops;
            for (const edge& e : edges)
            {
                if (in_mst[e.a][e.b])
                    continue;
                ++st.loop_candidates;
                const double direct = static_cast<double>(e.cost);
                const double bound  = kDetourRatio * direct;
                if (network_route_cost(net, e.a, e.b, bound) > bound)
                {
                    loops.emplace_back(e.a, e.b);
                    net[static_cast<std::size_t>(e.a)].push_back({ e.b, direct });
                    net[static_cast<std::size_t>(e.b)].push_back({ e.a, direct });
                }
            }
            st.loops_admitted += static_cast<int>(loops.size());
            // BL-618/BL-621: ration the loops by qualification PERCENTILE, cheapest-first
            // (`loops` inherits the deterministic (cost, lo, hi) edge order). The MST is
            // never rationed — a nation's towns connect regardless; loops are the
            // qualified-labour luxury, and the median nation keeps half of them.
            //
            // BL-1119: the loops rationed are the ones the detour test admitted. Keeping a
            // cheapest-first PREFIX is consistent with the admission walk: every kept loop
            // was tested against a network holding only cheaper loops, all of them kept.
            const float loop_frac = std::clamp(qualification, 0.0f, 1.0f);
            const int loops_kept =
                static_cast<int>(static_cast<float>(loops.size()) * loop_frac);
            st.loops_kept += loops_kept;
            const std::size_t tree_count = chosen.size(); // chosen[0, tree_count) is the tree
            for (int i = 0; i < loops_kept; ++i)
                chosen.push_back(loops[static_cast<std::size_t>(i)]);

            // Rasterise: tier by the two towns' scales gated by qualification (BL-618),
            // and feed this nation's stamped tiles into the spur target set.
            std::vector<entity_id> stamped;
            const long long floods_lay_from = floods_now();
            for (std::size_t ci = 0; ci < chosen.size(); ++ci)
            {
                const auto [a, b] = chosen[ci];
                const bool is_tree = ci < tree_count;
                const std::uint8_t tier =
                    edge_tier(nodes[towns[a]].scale, nodes[towns[b]].scale, qualification);
                stamped.clear();
                const bool laid =
                    stamp_edge(w, body, nodes[towns[a]].tile, nodes[towns[b]].tile, tier, &stamped);
                if (!laid)
                    ++(is_tree ? st.tree_links_refused : st.loops_refused);
                else
                    record(is_tree ? road_generation_trace::kind::tree
                                   : road_generation_trace::kind::loop,
                           nodes[towns[a]].tile, nodes[towns[b]].tile, nation);
                if (laid && nodes[towns[a]].scale >= kMajorScale
                    && nodes[towns[b]].scale >= kMajorScale)
                {
                    ++st.links_two_major;
                    if (tier == kHighway) ++st.links_highway;
                }
                for (const entity_id t : stamped)
                    if (nation_of(w, t) == nation)
                        add_target(t);
            }
            st.floods_backbone_lay += floods_now() - floods_lay_from;
        }

        // --- Village spurs (BL-620) -----------------------------------------------
        // Villages in tile-id order (members are already sorted), each laying one Track
        // to its nearest same-nation tile already JOINED to the backbone. A stamped spur's
        // tiles join the target set — they are joined, since the spur ends on a joined
        // tile — so later villages branch off earlier feeders rather than each running its
        // own long track; the incremental order is deterministic because the walk is.
        //
        // DIRECTION (BL-1119 round 4): a spur is priced from the village TO its target —
        // origin to destination, as LOGISTICS.md § 2 reads a path — so the flood is the
        // target's, and a target that is a town reuses the flood its backbone already
        // built. Measured 2026-09-25: pricing toward the village instead would build one
        // flood per spurring village (791 on seed 0 against this direction's 569).
        if (gw > 0)
        {
            constexpr long long kMaxSpurD2 =
                static_cast<long long>(kMaxSpurGridDist) * kMaxSpurGridDist;
            std::vector<entity_id> stamped;
            const long long floods_spurs_from = floods_now();
            for (const int m : members)
            {
                if (nodes[m].scale >= kMidScale)
                    continue; // towns are backbone members, not spur clients
                if (!spurs[static_cast<std::size_t>(m)])
                    continue; // BL-1119: below the spur floor, the street alone
                report_units(++units_done); // BL-1072: one village, before its A*
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
                bool laid = false;
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
                    record(road_generation_trace::kind::spur, nodes[m].tile, best[s].tile, nation);
                    laid = true;
                    break;
                }
                ++(laid ? st.spurs_laid : st.spurs_failed);
                if (laid) on_network[static_cast<std::size_t>(m)] = 1;
            }
            st.floods_spurs += floods_now() - floods_spurs_from;
        }
    }

    // 5. Border links: one local road between the nearest centre pair of each
    //    territorially-adjacent nation pair, connecting the per-nation lattices.
    if (bit == w.bodies.end())
    {
        if (stats != nullptr) *stats = st;
        return;
    }
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

    // A BORDER LINK ENDS ONLY ON A TOWN OR A SPURRING VILLAGE (Ben, 2026-09-25, the
    // density form; LOGISTICS.md § 4). Under the spur floor most villages keep a bare
    // street, and a link ending on one joined nothing — 106 of seed 46's 151 links did.
    // So each side's candidates are the centres ON their nation's network (`on_network`),
    // in the same tile order; a nation with none (every centre a bare street) offers no
    // endpoint, and that pair lays no link. `street_only` is kept as the stats' check
    // that no link ever ends off the network (write-only).
    std::map<entity_id, std::vector<int>> border_nodes;
    std::set<entity_id> street_only;
    for (const auto& [nation, members] : by_nation)
        for (const int m : members)
        {
            if (on_network[static_cast<std::size_t>(m)])
                border_nodes[nation].push_back(m);
            else
                street_only.insert(nodes[m].tile);
        }
    st.flood_fields_before_border = static_cast<long long>(w.logistics_flood_fields.size());

    // BL-1072: the border links carry the last units, spread over however
    // many adjacent pairs this map has.
    const long long border_units = kBorderUnitsPerNation * static_cast<long long>(by_nation.size());
    const long long border_base  = units_total - border_units;
    const long long border_pairs = static_cast<long long>(adjacency.size());
    long long       border_done  = 0;
    for (const auto& [na, nb] : adjacency)
    {
        report_units(border_base + border_done * border_units / std::max(1LL, border_pairs));
        ++border_done;
        if (by_nation.find(na) == by_nation.end() || by_nation.find(nb) == by_nation.end())
            continue; // a side with no centre at all on this body
        ++st.border_pairs;
        const auto ia = border_nodes.find(na);
        const auto ib = border_nodes.find(nb);
        if (ia == border_nodes.end() || ib == border_nodes.end())
        {
            ++st.border_pairs_no_endpoint; // one side is bare streets only
            continue;
        }

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

        // WHICH WAY THE PROBES ARE PRICED (BL-1119 round 4; a path is directed, BL-1126,
        // and answered from its destination's flood). Every probe of one nation pair is
        // priced the SAME way, so their costs compare like for like, and that way is
        // toward the side offering FEWER distinct probe endpoints — the fewer floods. A
        // tie keeps A -> B (the lower-id nation toward the higher), the direction the pass
        // always used. The chosen link is laid in the direction it was priced. A pure
        // function of the probe set, never of what a cache holds.
        std::set<entity_id> probe_a, probe_b;
        for (const border_pair& bp : probe)
        {
            probe_a.insert(bp.ta);
            probe_b.insert(bp.tb);
        }
        const bool toward_a = probe_a.size() < probe_b.size();

        float best = kUnreachable;
        entity_id best_from = null_entity, best_to = null_entity;
        for (const border_pair& bp : probe)
        {
            const entity_id from = toward_a ? bp.tb : bp.ta;
            const entity_id to   = toward_a ? bp.ta : bp.tb;
            const logistics_path& p = intra_body_path(w, body, from, to);
            if (p.reachable && p.cost < best)
            {
                best = p.cost;
                best_from = from;
                best_to   = to;
            }
        }
        const entity_id best_a = toward_a ? best_to : best_from; // nation A's endpoint
        const entity_id best_b = toward_a ? best_from : best_to; // nation B's endpoint
        if (best_from != null_entity && stamp_edge(w, body, best_from, best_to, kTrack))
        {
            record(road_generation_trace::kind::border, best_from, best_to, na);
            ++st.border_links;
            if (street_only.count(best_a) != 0 || street_only.count(best_b) != 0)
                ++st.border_links_street_only;
        }
    }

    report_units(units_total); // BL-1072: whole, whatever the border walk found
    st.floods_border = floods_now() - st.flood_fields_before_border;
    st.flood_fields = static_cast<long long>(w.logistics_flood_fields.size());
    if (trace != nullptr)
        for (std::size_t i = 0; i < nodes.size(); ++i)
            if (on_network[i])
                trace->on_network.push_back(nodes[i].centre);
    if (stats != nullptr)
        *stats = st; // BL-1119 D1: write-only, read by nothing in this pass

    // The A* cost cache (world.astar_cost_cache) was populated road-free while this
    // pass measured centre-pair costs to lay the network out — correct for the MST
    // decision, but now stale: the stamped roads lower those same lanes' costs. Drop
    // it so the gameplay dispatch loop recomputes against the final road_level field
    // (the "invalidated when road_level changes" contract, world.hpp). The raster
    // index is road-independent and stays. Generation-time, so clearing all is cheap.
    // Roads change traversal cost, so they change reach and the nearest-anchor field
    // too: ONE call, the owner of the set (logistics.hpp), so a cache added later is
    // never missed here — the hand-written clear this replaced missed lp_anchor_fields.
    invalidate_logistics_caches(w);
}

// ---------------------------------------------------------------------------
// Ancient roads, stamped from the history's record (BL-768)
// ---------------------------------------------------------------------------

void stamp_history_roads(world& w, entity_id body,
                         const std::vector<history_road_node>& nodes,
                         const std::vector<history_corridor>&  corridors,
                         generation_progress* progress, history_road_stats* stats)
{
    history_road_stats hs{};
    const long long floods_at_entry = static_cast<long long>(w.logistics_flood_fields.size());
    if (corridors.empty() || nodes.empty())
        return; // A world with no Era -1 pass. The whole call is a no-op.

    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end())
        return;
    const int gw = std::max(1, bit->second.grid_width);
    const int gh = std::max(1, bit->second.grid_height);

    // grid_y*gw + grid_x -> tile, the same raster index the region anchors use
    // (settlement.hpp § region::anchor) and the same one generate_roads' border
    // pass reads. Built once here, not per corridor.
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    if (static_cast<int>(grid.size()) < gw * gh)
        return;

    auto tile_of = [&](const history_road_node& n) -> entity_id {
        if (n.col < 0 || n.col >= gw || n.row < 0 || n.row >= gh)
            return null_entity;
        return grid[static_cast<std::size_t>(n.row) * gw + n.col];
    };

    // WHICH WAY A CORRIDOR IS PRICED (BL-1119 round 4, after BL-1126 made a path
    // directed: `intra_body_path(O, D)` is O -> D, answered from D's flood field). A
    // corridor is an unordered pair (`a < b` by region index), so its direction is a
    // choice, and the choice is the pass's cost: every distinct destination is one
    // whole-body flood. The history's corridors radiate from hubs — a parent region
    // and its foundings, a staging holding and its objectives — so each corridor is
    // priced TOWARD ITS BUSIER END: the endpoint tile more corridors touch, counted
    // over this call's own corridor set; a tie keeps the higher-index region `b`, the
    // direction the pass always used. One flood at a hub then answers every line into
    // it. A pure function of the corridor set — never of what a cache happens to hold.
    std::map<entity_id, int> corridor_degree;
    for (const history_corridor& c : corridors)
    {
        if (c.a >= nodes.size() || c.b >= nodes.size())
            continue;
        const entity_id ta = tile_of(nodes[c.a]);
        const entity_id tb = tile_of(nodes[c.b]);
        if (ta == null_entity || tb == null_entity || ta == tb)
            continue;
        ++corridor_degree[ta];
        ++corridor_degree[tb];
    }

    // `corridors` arrives sorted by (a, b) and stamping takes the max per tile,
    // so this walk is order-independent: a tile shared by two corridors ends at
    // the higher of the two tiers whichever is stamped first.
    // BL-1072: one unit per corridor on the loading bar. Write-only.
    const int corridor_total = static_cast<int>(std::min<std::size_t>(corridors.size(), 1000000));
    int       corridor_done  = 0;
    std::set<entity_id> destinations; // BL-1119 round 4 stats: the tiles priced toward
    for (const history_corridor& c : corridors)
    {
        if (progress != nullptr && corridor_done < corridor_total)
            progress->report_sub(++corridor_done, corridor_total);
        if (c.a >= nodes.size() || c.b >= nodes.size())
            continue; // A record written against a shorter node array.
        const history_road_node& na = nodes[c.a];
        const history_road_node& nb = nodes[c.b];

        const entity_id ta = tile_of(na);
        const entity_id tb = tile_of(nb);
        if (ta == null_entity || tb == null_entity || ta == tb)
            continue;
        // Toward the busier end; the tie keeps a -> b.
        const bool toward_a = corridor_degree[ta] > corridor_degree[tb];
        const entity_id from = toward_a ? tb : ta;
        const entity_id to   = toward_a ? ta : tb;

        // A region anchored on coastal water is legitimate (BL-777, the water
        // ownership ruling), and it simply carries no road: stamp_edge skips
        // every water tile, and the strait rule refuses a route that crosses
        // open ocean. Neither is special-cased here — the ancient network obeys
        // the same land rule the national lattice does.
        // BL-949: a corridor the sim carried at its THIRD rung (a Post Road,
        // `history_corridor::tier` 3 -- bought with capital, not walked) stamps
        // at the campaign ladder's top grade whatever its traffic reads. Read
        // off the carried rung, never off `uses`: a purchase adds one walk.
        std::uint8_t tier = ancient_tier(c.uses, na.reach_mod, nb.reach_mod);
        if (c.tier >= 3) tier = kHighway;
        ++hs.corridors;
        destinations.insert(to);
        if (stamp_edge(w, body, from, to, tier))
            ++hs.laid;
    }
    hs.destinations = static_cast<int>(destinations.size());
    hs.floods = static_cast<long long>(w.logistics_flood_fields.size()) - floods_at_entry;
    if (stats != nullptr)
        *stats = hs; // BL-1119 round 4: write-only

    // Same contract as generate_roads' tail: road_level moved, so every cache
    // keyed on traversal cost is stale — the whole set, through its one owner.
    invalidate_logistics_caches(w);
}


// ---------------------------------------------------------------------------
// Sea lanes, stamped from the lane record (BL-1098)
// ---------------------------------------------------------------------------

int sea_lane_port(const std::vector<std::uint8_t>& sea, int gw, int gh, int col, int row, int radius)
{
    if (gw <= 0 || gh <= 0 || sea.size() != static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
        return -1;
    if (col < 0 || row < 0 || col >= gw || row >= gh)
        return -1;
    // Ring by ring outward, so the first ring holding any sea is the nearest; inside
    // it the lowest raster index wins, which no scan order can change.
    for (int r = 0; r <= radius; ++r)
    {
        int best = -1;
        for (int dr = -r; dr <= r; ++dr)
        {
            const int rr = row + dr;
            if (rr < 0 || rr >= gh)
                continue;
            for (int dc = -r; dc <= r; ++dc)
            {
                if (std::max(std::abs(dc), std::abs(dr)) != r)
                    continue; // the ring's rim only
                const int cc = ((col + dc) % gw + gw) % gw;
                const int idx = rr * gw + cc;
                if (sea[static_cast<std::size_t>(idx)] && (best < 0 || idx < best))
                    best = idx;
            }
        }
        if (best >= 0)
            return best;
    }
    return -1;
}

std::vector<int> sea_lane_walk(const std::vector<std::uint8_t>& sea, int gw, int gh,
                               const ocean_current_field* currents, int weight_q,
                               int from, int to)
{
    std::vector<int> path;
    const int n = gw * gh;
    if (gw <= 0 || gh <= 0 || sea.size() != static_cast<std::size_t>(n))
        return path;
    if (from < 0 || to < 0 || from >= n || to >= n || !sea[static_cast<std::size_t>(from)]
        || !sea[static_cast<std::size_t>(to)])
        return path;
    if (from == to)
    {
        path.push_back(from);
        return path;
    }
    const bool priced = currents != nullptr && !currents->empty() && weight_q > 0
                     && currents->gw == gw && currents->gh == gh;

    // Dijkstra on (cost, raster index): the pair is unique, so the frontier's order,
    // and with it every tie between equal-cost routes, is a property of the integers.
    constexpr int64_t kInf = std::numeric_limits<int64_t>::max();
    std::vector<int64_t> dist(static_cast<std::size_t>(n), kInf);
    std::vector<int>     prev(static_cast<std::size_t>(n), -1);
    using node = std::pair<int64_t, int>;
    std::priority_queue<node, std::vector<node>, std::greater<node>> frontier;
    dist[static_cast<std::size_t>(from)] = 0;
    frontier.push({0, from});
    while (!frontier.empty())
    {
        const node top = frontier.top();
        frontier.pop();
        const int u = top.second;
        if (top.first > dist[static_cast<std::size_t>(u)])
            continue; // a superseded entry
        if (u == to)
            break;
        const int uc = u % gw, ur = u / gw;
        // FOUR CARDINAL STEPS, columns wrapping and rows not -- the pathfinder's own
        // grid (logistics.cpp, LOGISTICS.md sec 2). A lane walked eight ways leaves
        // no two laned tiles side by side on a diagonal, so a four-way traveller
        // rides it at about x0.75 instead of x0.50; walked on the traveller's grid,
        // every step of the lane is a step the traveller can take.
        constexpr int kStepDc[4] = { 0, 0, -1, 1 };
        constexpr int kStepDr[4] = { -1, 1, 0, 0 };
        for (int k4 = 0; k4 < 4; ++k4)
        {
            const int dc = kStepDc[k4], dr = kStepDr[k4];
            const int vr = ur + dr;
            if (vr < 0 || vr >= gh)
                continue; // rows do not wrap
            const int vc = ((uc + dc) % gw + gw) % gw;
            const int v = vr * gw + vc;
            if (!sea[static_cast<std::size_t>(v)])
                continue; // water only
            constexpr int64_t len = 1000;
            int64_t step = len;
            if (priced)
            {
                // The step's direction (east = dc, north = -dr) against the entered
                // tile's ocean-region current, per mille of a full current along it.
                const std::size_t k = static_cast<std::size_t>(currents->region_of(vc, vr));
                const int64_t dot = static_cast<int64_t>(currents->east_q[k]) * dc
                                  + static_cast<int64_t>(currents->north_q[k]) * (-dr);
                const int align = static_cast<int>(std::clamp<int64_t>(dot, -1000, 1000));
                step = (len * ocean_current_leg_cost_q(weight_q, align)) / 1000;
                if (step < 1)
                    step = 1; // no step is ever free
            }
            const int64_t cand = dist[static_cast<std::size_t>(u)] + step;
            if (cand < dist[static_cast<std::size_t>(v)])
            {
                dist[static_cast<std::size_t>(v)] = cand;
                prev[static_cast<std::size_t>(v)] = u;
                frontier.push({cand, v});
            }
        }
    }
    if (dist[static_cast<std::size_t>(to)] == kInf)
        return path;
    for (int t = to; t >= 0; t = prev[static_cast<std::size_t>(t)])
    {
        path.push_back(t);
        if (t == from)
            break;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

namespace {

/// The sim's own region measure, restated on the flattened node: `region_distance`
/// (history_sim.cpp) -- Chebyshev between the two seats, columns wrapping because the
/// map is a cylinder, rows not. Restated rather than called because the stamp holds
/// `history_road_node`s, not `region`s; sea_lane_stamp_harness binds the pick against
/// `region_distance` itself.
int sea_lane_seat_distance(const history_road_node& a, const history_road_node& b, int gw)
{
    int dc = a.col - b.col;
    if (dc < 0) dc = -dc;
    if (gw > 0 && dc > gw / 2) dc = gw - dc;
    int dr = a.row - b.row;
    if (dr < 0) dr = -dr;
    return dc > dr ? dc : dr;
}

} // namespace

void stamp_sea_lanes(world& w, entity_id body,
                     const std::vector<history_road_node>& nodes,
                     const std::vector<sea_leg>&           legs,
                     const std::vector<int>&               region_realm,
                     int lane_tier_uses, int current_weight_q, int rotation_sense,
                     sea_lane_stats* stats, sea_lane_trace* trace)
{
    sea_lane_stats st{};
    if (legs.empty() || nodes.empty())
        return; // no span ran: the whole call is a no-op
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end())
        return;
    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;
    if (gw <= 0 || gh <= 0)
        return;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    if (static_cast<int>(grid.size()) < gw * gh)
        return;

    // The body's substrate raster, the one input the sea mask and the current field
    // both read -- the same ground the spans built their field from.
    std::vector<terrain_substrate> substrate(static_cast<std::size_t>(gw) * gh, terrain_substrate::sedimentary);
    std::vector<std::uint8_t>      sea(static_cast<std::size_t>(gw) * gh, 0);
    for (int i = 0; i < gw * gh; ++i)
    {
        const auto it = w.tiles.find(grid[static_cast<std::size_t>(i)]);
        if (it == w.tiles.end())
            continue;
        substrate[static_cast<std::size_t>(i)] = it->second.substrate;
        sea[static_cast<std::size_t>(i)] = is_sea(it->second.substrate) ? 1 : 0;
    }
    const ocean_current_field currents =
        (ocean_current_weight_valid(current_weight_q) && current_weight_q > 0)
            ? build_ocean_currents(substrate, gw, gh, rotation_sense)
            : ocean_current_field{};

    // The earned lanes, both seats on the body, and how many of them touch each seat
    // (the busier-end direction rule). `legs` arrives sorted by (a, b).
    std::vector<const sea_leg*> earned;
    std::map<int, int> degree;
    for (const sea_leg& l : legs)
    {
        if (l.uses < lane_tier_uses || l.a == l.b)
            continue;
        if (l.a >= nodes.size() || l.b >= nodes.size())
            continue;
        const history_road_node& na = nodes[l.a];
        const history_road_node& nb = nodes[l.b];
        if (na.col < 0 || na.row < 0 || na.col >= gw || na.row >= gh
         || nb.col < 0 || nb.row < 0 || nb.col >= gw || nb.row >= gh)
            continue;
        earned.push_back(&l);
        ++degree[l.a];
        ++degree[l.b];
    }
    st.earned = static_cast<int>(earned.size());

    // A REALM'S PORT IS ITS NEAREST COASTAL REGION'S SEAT (BL-1153; LOGISTICS.md
    // § 4b, NR-955 B). Each region's own port is looked up once and kept; each
    // realm's regions are listed once, ascending, so the nearest coastal one is a
    // walk over a vector in index order and a tie falls to the lower region by
    // construction. Lookups are lazy: only a realm with an inland lane end is
    // ever searched.
    const std::size_t region_count = nodes.size();
    const bool realms_known = region_realm.size() == region_count;
    std::vector<int> own_port(region_count, -2); // -2: not looked up yet
    const auto port_of = [&](int ri) -> int {
        int& p = own_port[static_cast<std::size_t>(ri)];
        if (p == -2)
        {
            const history_road_node& n = nodes[static_cast<std::size_t>(ri)];
            p = sea_lane_port(sea, gw, gh, n.col, n.row, kSeaLanePortRadius);
        }
        return p;
    };
    std::map<int, std::vector<int>> realm_regions; // realm -> its regions, ascending
    if (realms_known)
        for (std::size_t ri = 0; ri < region_count; ++ri)
            if (region_realm[ri] >= 0)
                realm_regions[region_realm[ri]].push_back(static_cast<int>(ri));

    // Where one lane end's port comes from: the region whose seat it is taken at
    // (the end itself, or its realm's nearest coastal region), or a reason there is
    // none. Memoised per region -- a seat that ends several lanes resolves once.
    enum class end_fail : std::uint8_t { none, unheld, no_coast };
    struct end_pick { int seat = -1; int port = -1; end_fail fail = end_fail::none; };
    std::map<int, end_pick> picked;
    const auto pick_end = [&](int ri) -> end_pick {
        const auto hit = picked.find(ri);
        if (hit != picked.end())
            return hit->second;
        end_pick e;
        if (port_of(ri) >= 0)
        {
            e.seat = ri;
            e.port = port_of(ri);
        }
        else if (!realms_known || region_realm[static_cast<std::size_t>(ri)] < 0)
            e.fail = end_fail::unheld;
        else
        {
            const history_road_node& from = nodes[static_cast<std::size_t>(ri)];
            // `ri` is held (realm >= 0), so its realm is listed: it lists `ri` itself.
            const std::vector<int>& mine = realm_regions.find(region_realm[static_cast<std::size_t>(ri)])->second;
            int best_d = std::numeric_limits<int>::max();
            for (const int rj : mine)
            {
                if (port_of(rj) < 0)
                    continue; // inland too
                const int d = sea_lane_seat_distance(from, nodes[static_cast<std::size_t>(rj)], gw);
                if (d < best_d) // strict: ascending walk, so a tie keeps the lower region
                {
                    best_d = d;
                    e.seat = rj;
                }
            }
            if (e.seat < 0)
                e.fail = end_fail::no_coast;
            else
                e.port = port_of(e.seat);
        }
        picked.emplace(ri, e);
        return e;
    };

    for (const sea_leg* lp : earned)
    {
        const sea_leg& l = *lp;
        const bool toward_a = degree[l.a] > degree[l.b];
        const int from_region = toward_a ? l.b : l.a;
        const int to_region   = toward_a ? l.a : l.b;
        const end_pick ef = pick_end(from_region);
        const end_pick et = pick_end(to_region);
        if (ef.fail != end_fail::none || et.fail != end_fail::none)
        {
            ++st.no_port;
            // An unheld end outranks a coastless realm, so the split does not
            // depend on which end the walk would have started from.
            if (ef.fail == end_fail::unheld || et.fail == end_fail::unheld)
                ++st.no_port_unheld;
            else
                ++st.no_port_no_coast;
            continue;
        }
        const int from_port = ef.port;
        const int to_port   = et.port;
        if (ef.seat != from_region) ++st.moved_ends;
        if (et.seat != to_region)   ++st.moved_ends;
        std::vector<int> path = sea_lane_walk(sea, gw, gh, currents.empty() ? nullptr : &currents,
                                              current_weight_q, from_port, to_port);
        if (path.empty())
        {
            ++st.unreachable;
            continue;
        }
        for (const int idx : path)
        {
            const auto it = w.tiles.find(grid[static_cast<std::size_t>(idx)]);
            if (it == w.tiles.end() || !is_sea(it->second.substrate))
                continue; // the walk is water-only; belt and braces
            it->second.lane_level = std::max<std::uint8_t>(it->second.lane_level, 1);
        }
        ++st.laid;
        st.path_tiles += static_cast<long long>(path.size());
        if (trace != nullptr)
        {
            sea_lane_trace::lane tl;
            tl.a = l.a;
            tl.b = l.b;
            tl.uses = l.uses;
            tl.from_port = from_port;
            tl.to_port = to_port;
            tl.from_seat = ef.seat;
            tl.to_seat = et.seat;
            tl.path = std::move(path);
            trace->lanes.push_back(std::move(tl));
        }
    }
    for (int i = 0; i < gw * gh; ++i)
    {
        const auto it = w.tiles.find(grid[static_cast<std::size_t>(i)]);
        if (it != w.tiles.end() && it->second.lane_level > 0)
            ++st.lane_tiles;
    }
    if (stats != nullptr)
        *stats = st;

    // lane_level moved traversal cost on the water: every cache keyed on it is stale.
    if (st.laid > 0)
        invalidate_logistics_caches(w);
}
