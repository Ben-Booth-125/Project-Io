// lp_coverage_census — BL-1351 (does generation's seeded Logistic Point
// sourcing cover every nation?). A PURE READER: it builds the shipped campaign
// start, times the fold's `seed_logistics_sources` pass, and then measures what
// that pass left behind. Nothing here writes the world beyond the fold calls the
// app itself makes (LOGISTICS.md § Logistic Points, "One pool").
//
// The questions it answers, per nation and per seed:
//   * sources by network (road / rail / sea), and of the hubs THIS pass placed,
//     how many landed on a market centre_tile against a consumer's own tile.
//     That ratio decides whether a "centralised capital" picture can appear.
//   * pool against claim, and the realised margin against k_pool_margin (2.0).
//   * the coverage distribution over the nation's ground: the share of its tiles
//     at coverage 1, in the fade band (0 < c < 1) and at 0 — and the same over
//     its BUILDING tiles only, since those are what consume.
//   * points incident at the capital tile against the most distant covered tile,
//     plus mean points per tile in distance rings from the capital: the
//     diminishing-with-distance reading.
//   * landlocked against coastal, majors against minors.
//   * every nation whose pool falls short of its claim, named, with the shortfall.
//
// POINTS INCIDENT AT A TILE is this harness's own derived quantity, defined
// here and nowhere in src/world: the sum, over the host nation's running
// sources, of that source's made points x its reach at that tile
// (`lp_source_points[net] * building_supply_scalar(b) * lp_reach(net, d)`). The
// pool is not spatial — a nation spends one pool anywhere on its ground — so
// this is NOT a cap on what a tile can draw. It is the *shape of the source
// field*, which is the only thing a "points diminish with distance" question can
// be about.
//
// THE BEFORE BASELINE for three merged items (roads as edges; road tier reach
// 6/8/10 fading to double; rail 20 point-to-point). Re-runnable unchanged
// afterwards: every line is one record with named `key=value` fields, so a diff
// of two runs is a diff of numbers.
//
// Build:  bash tools/verify/build_lua_harness.sh lp_coverage_census
// Run:    build_gen/verify/lp_coverage_census.exe [--seeds 0,28,46]
//         bash tools/verify/par_seeds.sh lp_coverage_census --build

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/logistic_points.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/state_industry.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct ring_acc
{
    double points = 0.0;
    long   tiles  = 0;
    void add(double p) { points += p; ++tiles; }
    double mean() const { return tiles > 0 ? points / static_cast<double>(tiles) : 0.0; }
};

/// Four distance rings from the capital, in grid steps.
constexpr int k_ring_edge[4] = {5, 10, 20, 1 << 30};

int ring_of(int d)
{
    for (int i = 0; i < 4; ++i)
        if (d <= k_ring_edge[i])
            return i;
    return 3;
}

struct nation_census
{
    entity_id   nation = null_entity;
    std::string name;
    bool        major    = false;
    bool        coastal  = false;
    int         tiles    = 0;
    int         sources[lp_network_count] = {0, 0, 0};
    int         hubs_centre = 0; ///< hubs this pass placed on a market centre_tile
    int         hubs_own    = 0; ///< ... and on a consumer's own tile
    double      pool  = 0.0;
    double      claim = 0.0;
    int         cov_full = 0, cov_fade = 0, cov_zero = 0;   ///< over its ground
    int         dry_tiles = 0;                              ///< ... of which DRY
    int         dry_full = 0, dry_fade = 0, dry_zero = 0;   ///< over its dry ground
    int         b_full = 0, b_fade = 0, b_zero = 0;         ///< over its BUILDING tiles
    int         c_full = 0, c_fade = 0, c_zero = 0;         ///< over its consumers
    double      cap_points = 0.0;
    double      max_points = 0.0;
    int         far_dist   = -1;
    double      far_points = 0.0;
    ring_acc    rings[4];
};

/// Manhattan grid distance, x wrapping as the LP walk's neighbour step wraps.
int grid_dist(const world& w, entity_id a, entity_id b)
{
    const auto ia = w.tiles.find(a), ib = w.tiles.find(b);
    if (ia == w.tiles.end() || ib == w.tiles.end() || ia->second.body != ib->second.body)
        return -1;
    const auto bit = w.bodies.find(ia->second.body);
    const int gw = bit != w.bodies.end() ? bit->second.grid_width : 0;
    int dx = std::abs(ia->second.grid_x - ib->second.grid_x);
    if (gw > 0 && dx > gw - dx)
        dx = gw - dx;
    return dx + std::abs(ia->second.grid_y - ib->second.grid_y);
}

struct bucket
{
    const char* label;
    int   nations = 0;
    double pool = 0.0, claim = 0.0;
    long  tiles = 0, cov_full = 0, cov_fade = 0, cov_zero = 0;
    long  dry_tiles = 0, dry_full = 0, dry_fade = 0, dry_zero = 0;
    long  b_full = 0, b_fade = 0, b_zero = 0;
    long  c_full = 0, c_fade = 0, c_zero = 0;
    int   hubs_centre = 0, hubs_own = 0;
    int   no_pool = 0;   ///< nations holding ground with no pool at all
    int   no_claim = 0;  ///< ... with no consumer claiming anything
    int   all_dark = 0;  ///< ... with not one tile at coverage 1
    int   sources[lp_network_count] = {0, 0, 0};
    double cap_points = 0.0, far_points = 0.0;
    long   cap_n = 0;
    ring_acc rings[4];

    void add(const nation_census& n)
    {
        ++nations;
        pool += n.pool; claim += n.claim;
        tiles += n.tiles;
        cov_full += n.cov_full; cov_fade += n.cov_fade; cov_zero += n.cov_zero;
        dry_tiles += n.dry_tiles;
        dry_full += n.dry_full; dry_fade += n.dry_fade; dry_zero += n.dry_zero;
        b_full += n.b_full; b_fade += n.b_fade; b_zero += n.b_zero;
        c_full += n.c_full; c_fade += n.c_fade; c_zero += n.c_zero;
        hubs_centre += n.hubs_centre; hubs_own += n.hubs_own;
        if (n.pool <= 0.0)  ++no_pool;
        if (n.claim <= 0.0) ++no_claim;
        if (n.cov_full == 0) ++all_dark;
        for (std::size_t k = 0; k < lp_network_count; ++k)
            sources[k] += n.sources[k];
        if (n.far_dist >= 0)
        {
            cap_points += n.cap_points;
            far_points += n.far_points;
            ++cap_n;
        }
        for (int i = 0; i < 4; ++i)
        {
            rings[i].points += n.rings[i].points;
            rings[i].tiles  += n.rings[i].tiles;
        }
    }
};

double pct(long a, long b) { return b > 0 ? 100.0 * static_cast<double>(a) / static_cast<double>(b) : 0.0; }

void print_bucket(const char* seed, const bucket& b)
{
    std::printf("B seed=%s class=%-10s nations=%d road=%d rail=%d sea=%d hub_centre=%d hub_own=%d "
                "centre_share=%.1f%% nopool=%d noclaim=%d alldark=%d pool=%.0f claim=%.0f margin=%.2f "
                "ground_full=%.1f%% fade=%.1f%% zero=%.1f%% dry_full=%.1f%% dry_fade=%.1f%% dry_zero=%.1f%% dry_tiles=%ld bld_full=%.1f%% bld_fade=%.1f%% bld_zero=%.1f%% "
                "cons_full=%.1f%% cons_fade=%.1f%% cons_zero=%.1f%% "
                "cap_pts=%.0f far_pts=%.0f ring0=%.1f ring1=%.1f ring2=%.1f ring3=%.1f "
                "rt0=%ld rt1=%ld rt2=%ld rt3=%ld rp0=%.0f rp1=%.0f rp2=%.0f rp3=%.0f\n",
                seed, b.label, b.nations, b.sources[0], b.sources[1], b.sources[2],
                b.hubs_centre, b.hubs_own,
                pct(b.hubs_centre, b.hubs_centre + b.hubs_own),
                b.no_pool, b.no_claim, b.all_dark,
                b.pool, b.claim, b.claim > 0.0 ? b.pool / b.claim : 0.0,
                pct(b.cov_full, b.tiles), pct(b.cov_fade, b.tiles), pct(b.cov_zero, b.tiles),
                pct(b.dry_full, b.dry_tiles), pct(b.dry_fade, b.dry_tiles),
                pct(b.dry_zero, b.dry_tiles), b.dry_tiles,
                pct(b.b_full, b.b_full + b.b_fade + b.b_zero),
                pct(b.b_fade, b.b_full + b.b_fade + b.b_zero),
                pct(b.b_zero, b.b_full + b.b_fade + b.b_zero),
                pct(b.c_full, b.c_full + b.c_fade + b.c_zero),
                pct(b.c_fade, b.c_full + b.c_fade + b.c_zero),
                pct(b.c_zero, b.c_full + b.c_fade + b.c_zero),
                b.cap_n > 0 ? b.cap_points / static_cast<double>(b.cap_n) : 0.0,
                b.cap_n > 0 ? b.far_points / static_cast<double>(b.cap_n) : 0.0,
                b.rings[0].mean(), b.rings[1].mean(), b.rings[2].mean(), b.rings[3].mean(),
                b.rings[0].tiles, b.rings[1].tiles, b.rings[2].tiles, b.rings[3].tiles,
                b.rings[0].points, b.rings[1].points, b.rings[2].points, b.rings[3].points);
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds{0};
    bool per_nation = true;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc)
        {
            seeds.clear();
            std::stringstream ss(argv[++i]);
            std::string t;
            while (std::getline(ss, t, ','))
                if (!t.empty())
                    seeds.push_back(static_cast<std::uint32_t>(std::strtoul(t.c_str(), nullptr, 10)));
        }
        else if (!std::strcmp(argv[i], "--no-nations"))
        {
            per_nation = false;
        }
        else
        {
            std::fprintf(stderr, "usage: lp_coverage_census [--seeds a,b,c] [--no-nations]\n");
            return 2;
        }
    }

    std::vector<bucket> all;
    all.push_back({"ALL"});
    all.push_back({"major"});
    all.push_back({"minor"});
    all.push_back({"coastal"});
    all.push_back({"landlocked"});
    long   total_hubs = 0, total_shortfall_nations = 0;
    double total_seed_ms = 0.0;

    for (const std::uint32_t seed : seeds)
    {
        lua_state lua;
        world_params p;
        p.seed = seed;
        auto start = std::make_unique<app_start_world>();
        // The shipped start, with the fold's last three calls HELD BACK so the
        // seeding pass can be timed on its own. `skip_state_handover` holds the
        // rail handover and the LP seeding with it (harness_params.hpp), and the
        // three are then made here in finish_campaign_world's own order.
        harness_charter_input in;
        in.skip_state_handover = true;
        build_app_base_world(lua, p, *start);
        apply_app_start_landscape(*start, in);
        world& w = start->w;
        hand_over_state_industry(w, start->reg);
        hand_over_rail(w);

        // Hubs that already stood before the seeding pass: generation's, not this
        // pass's. Everything added below is the pass's own placement.
        std::set<entity_id> pre_hubs;
        for (const auto& [bid, b] : w.buildings)
            if (b.type == building_type::inland_logistics_hub)
                pre_hubs.insert(bid);

        const auto t0 = std::chrono::steady_clock::now();
        const lp_seed_report rep = seed_logistics_sources(w);
        const auto t1 = std::chrono::steady_clock::now();
        const double seed_ms =
            std::chrono::duration<double, std::milli>(t1 - t0).count();
        total_seed_ms += seed_ms;
        total_hubs += rep.hubs_placed;

        // --- the reading -----------------------------------------------------
        const lp_tick t = resolve_logistic_points(w);

        std::set<entity_id> centre_tiles;
        for (const auto& [mid, m] : w.markets)
            if (m.centre_tile != null_entity)
                centre_tiles.insert(m.centre_tile);

        // How many centre tiles the centre route could EVER have used: the
        // dedupe in the pass (`hub_tiles`) allows at most one hub per centre, so
        // this is the ceiling on `hub_centre` for the whole world.
        int centres_on_land = 0;
        for (const entity_id ct : centre_tiles)
        {
            const auto tit = w.tiles.find(ct);
            if (tit != w.tiles.end() && !is_water(tit->second.substrate)
                && w.tile_to_nation.count(ct) != 0)
                ++centres_on_land;
        }

        std::map<entity_id, nation_census> cen;
        for (const auto& [nid, n] : w.nations)
        {
            nation_census& c = cen[nid];
            c.nation = nid;
            c.name   = n.name;
            c.major  = is_major_nation(w, nid);
        }

        // Ground: tile counts, coastal flag, coverage per tile.
        std::map<entity_id, std::vector<entity_id>> ground; // nation -> tiles ascending
        for (const auto& [tile, nid] : w.tile_to_nation)
            ground[nid].push_back(tile);
        for (auto& [nid, tiles] : ground)
            std::sort(tiles.begin(), tiles.end());

        lp_reach_memo memo;
        for (const auto& [nid, tiles] : ground)
        {
            nation_census& c = cen[nid];
            c.nation = nid;
            for (const entity_id tile : tiles)
            {
                ++c.tiles;
                const float cov = lp_coverage_at(w, memo, tile);
                if (cov >= 1.0f)      ++c.cov_full;
                else if (cov > 0.0f)  ++c.cov_fade;
                else                  ++c.cov_zero;
                // DRY ground only. A nation owns its shelf water and its lakes
                // (`derive_water_ownership`, nation_generation.cpp), which no road
                // or rail field can ever enter, so the whole-ground share above
                // has an unreachable floor built into it.
                if (!is_water(w.tiles.at(tile).substrate))
                {
                    ++c.dry_tiles;
                    if (cov >= 1.0f)      ++c.dry_full;
                    else if (cov > 0.0f)  ++c.dry_fade;
                    else                  ++c.dry_zero;
                }
                // Coastal: a dry tile with a water neighbour, or a water tile held.
                if (!c.coastal)
                {
                    const tile_component& tc = w.tiles.at(tile);
                    if (is_water(tc.substrate))
                        c.coastal = true;
                    else
                    {
                        const auto bit = w.bodies.find(tc.body);
                        if (bit != w.bodies.end())
                        {
                            const int gw = bit->second.grid_width, gh = bit->second.grid_height;
                            const std::vector<entity_id>& grid = body_tile_grid(w, tc.body);
                            const int x = tc.grid_x, y = tc.grid_y;
                            const int cand[4] = {y > 0 ? (y - 1) * gw + x : -1,
                                                 y * gw + (x + 1) % gw,
                                                 y + 1 < gh ? (y + 1) * gw + x : -1,
                                                 y * gw + (x + gw - 1) % gw};
                            for (const int k : cand)
                            {
                                if (k < 0 || static_cast<std::size_t>(k) >= grid.size())
                                    continue;
                                const entity_id nt = grid[static_cast<std::size_t>(k)];
                                if (nt != null_entity && is_water(w.tiles.at(nt).substrate))
                                {
                                    c.coastal = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        // Pools and claims, and the sources as the resolver read them.
        for (const lp_nation_row& r : t.nations)
        {
            nation_census& c = cen[r.nation];
            c.nation = r.nation;
            c.pool   = r.pool;
            c.claim  = static_cast<double>(r.claim[0]) + r.claim[1] + r.claim[2];
            for (std::size_t k = 0; k < lp_network_count; ++k)
                c.sources[k] = r.sources[k];
        }

        // Where this pass's hubs landed: a market centre_tile, or a consumer's own.
        for (const auto& [bid, b] : w.buildings)
        {
            if (b.type != building_type::inland_logistics_hub || pre_hubs.count(bid) != 0)
                continue;
            const auto nit = w.tile_to_nation.find(b.tile);
            if (nit == w.tile_to_nation.end())
                continue;
            nation_census& c = cen[nit->second];
            c.nation = nit->second;
            if (centre_tiles.count(b.tile) != 0)
                ++c.hubs_centre;
            else
                ++c.hubs_own;
        }

        // Building tiles (every building, whatever its type) and consumers.
        for (const auto& [bid, b] : w.buildings)
        {
            const auto nit = w.tile_to_nation.find(b.tile);
            if (nit == w.tile_to_nation.end())
                continue;
            nation_census& c = cen[nit->second];
            c.nation = nit->second;
            const float cov = lp_coverage_at(w, memo, b.tile);
            if (cov >= 1.0f)     ++c.b_full;
            else if (cov > 0.0f) ++c.b_fade;
            else                 ++c.b_zero;
        }
        for (const lp_building_row& row : t.buildings)
        {
            if (row.nation == null_entity)
                continue;
            nation_census& c = cen[row.nation];
            if (row.coverage >= 1.0f)     ++c.c_full;
            else if (row.coverage > 0.0f) ++c.c_fade;
            else                          ++c.c_zero;
        }

        // The POINTS FIELD: per-source reach, accumulated on the source nation's
        // own ground only (a foreign tile draws on its own host's pool).
        std::unordered_map<entity_id, double> points;
        {
            std::vector<lp_source_row> srcs = t.sources;
            std::sort(srcs.begin(), srcs.end(),
                      [](const lp_source_row& a, const lp_source_row& b) { return a.building < b.building; });
            for (const lp_source_row& s : srcs)
            {
                if (s.nation == null_entity)
                    continue;
                const auto tit = w.tiles.find(s.tile);
                if (tit == w.tiles.end())
                    continue;
                const std::size_t ni = static_cast<std::size_t>(s.network);
                const float made = lp_source_points[ni]
                                 * building_supply_scalar(w.buildings.at(s.building));
                const auto field = lp_distance_field(w, tit->second.body, s.network, {s.tile});
                for (const auto& [tile, d] : field)
                {
                    const auto hit = w.tile_to_nation.find(tile);
                    if (hit == w.tile_to_nation.end() || hit->second != s.nation)
                        continue;
                    const float reach = lp_reach(s.network, d);
                    if (reach > 0.0f)
                        points[tile] += static_cast<double>(made) * reach;
                }
            }
        }

        // The capital reading, and the rings from it.
        for (auto& [nid, c] : cen)
        {
            const auto nit = w.nations.find(nid);
            if (nit == w.nations.end())
                continue;
            const entity_id cap = nit->second.capital_tile;
            const auto pit = points.find(cap);
            c.cap_points = pit != points.end() ? pit->second : 0.0;
            const auto git = ground.find(nid);
            if (git == ground.end())
                continue;
            for (const entity_id tile : git->second)
            {
                const auto p2 = points.find(tile);
                const double pv = p2 != points.end() ? p2->second : 0.0;
                c.max_points = std::max(c.max_points, pv);
                if (cap == null_entity)
                    continue;
                const int d = grid_dist(w, cap, tile);
                if (d < 0)
                    continue;
                c.rings[ring_of(d)].add(pv);
                // The most distant COVERED tile (coverage > 0 == points > 0).
                if (pv > 0.0 && d > c.far_dist)
                {
                    c.far_dist   = d;
                    c.far_points = pv;
                }
            }
        }

        // --- print ------------------------------------------------------------
        std::printf("=== seed %u ===\n", seed);
        std::printf("K seed=%u markets=%zu centres_on_land=%d\n", seed, w.markets.size(),
                    centres_on_land);
        std::printf("W seed=%u nations=%d hubs_placed=%d of_max=%.1f%% gen_hubs=%zu "
                    "src_road=%d src_rail=%d src_sea=%d consumers=%d full=%d part=%d none=%d "
                    "nobody=%d unconnectable=%d cut_off=%d ports=%d bridged=%d need=%.0f pool=%.0f granted=%.0f seed_ms=%.0f\n",
                    seed, rep.nations, rep.hubs_placed, 100.0 * rep.hubs_placed / 4000.0,
                    pre_hubs.size(), rep.sources_by_net[0], rep.sources_by_net[1],
                    rep.sources_by_net[2], rep.consumers, rep.fully_covered,
                    rep.partly_covered, rep.uncovered, rep.nobodys_ground, rep.unconnectable, rep.unconnectable_cut_off, rep.ports_placed, rep.islands_bridged,
                    static_cast<double>(rep.need_total), static_cast<double>(rep.pool_total),
                    static_cast<double>(rep.granted_total), seed_ms);

        char seed_label[32];
        std::snprintf(seed_label, sizeof seed_label, "%u", seed);
        bucket seed_all{"ALL"}, seed_major{"major"}, seed_minor{"minor"},
               seed_coast{"coastal"}, seed_land{"landlocked"};
        for (const auto& [nid, c] : cen)
        {
            if (c.tiles == 0 && c.pool == 0.0 && c.claim == 0.0)
                continue; // a nation holding no ground this harness can read
            seed_all.add(c);
            (c.major ? seed_major : seed_minor).add(c);
            (c.coastal ? seed_coast : seed_land).add(c);
            all[0].add(c);
            all[c.major ? 1 : 2].add(c);
            all[c.coastal ? 3 : 4].add(c);
            if (per_nation)
                std::printf("N seed=%u nation=%u name=%s major=%d coastal=%d tiles=%d "
                            "road=%d rail=%d sea=%d hub_centre=%d hub_own=%d "
                            "pool=%.0f claim=%.0f margin=%.2f "
                            "g_full=%.3f g_fade=%.3f g_zero=%.3f dry=%d "
                            "d_full=%d d_fade=%d d_zero=%d "
                            "b_full=%d b_fade=%d b_zero=%d c_full=%d c_fade=%d c_zero=%d "
                            "cap_pts=%.0f max_pts=%.0f far_d=%d far_pts=%.0f\n",
                            seed, static_cast<unsigned>(nid), c.name.c_str(), c.major ? 1 : 0,
                            c.coastal ? 1 : 0, c.tiles, c.sources[0], c.sources[1], c.sources[2],
                            c.hubs_centre, c.hubs_own, c.pool, c.claim,
                            c.claim > 0.0 ? c.pool / c.claim : 0.0,
                            c.tiles > 0 ? static_cast<double>(c.cov_full) / c.tiles : 0.0,
                            c.tiles > 0 ? static_cast<double>(c.cov_fade) / c.tiles : 0.0,
                            c.tiles > 0 ? static_cast<double>(c.cov_zero) / c.tiles : 0.0,
                            c.dry_tiles, c.dry_full, c.dry_fade, c.dry_zero,
                            c.b_full, c.b_fade, c.b_zero, c.c_full, c.c_fade, c.c_zero,
                            c.cap_points, c.max_points, c.far_dist, c.far_points);
        }
        print_bucket(seed_label, seed_all);
        print_bucket(seed_label, seed_major);
        print_bucket(seed_label, seed_minor);
        print_bucket(seed_label, seed_coast);
        print_bucket(seed_label, seed_land);

        // Short pools, named. A pool below its claim produces below need even
        // where coverage is 1 (`granted / need` is the output scalar).
        for (const auto& [nid, c] : cen)
        {
            if (c.claim > 0.0 && c.pool < c.claim)
            {
                std::printf("S seed=%u SHORT nation=%u name=%s major=%d pool=%.0f claim=%.0f "
                            "short=%.0f ratio=%.2f\n",
                            seed, static_cast<unsigned>(nid), c.name.c_str(), c.major ? 1 : 0,
                            c.pool, c.claim, c.claim - c.pool, c.pool / c.claim);
                ++total_shortfall_nations;
            }
        }
        std::fflush(stdout);
    }

    std::printf("\n=== lp_coverage_census over %zu seed(s) ===\n", seeds.size());
    std::printf("T seeds=%zu hubs_placed=%ld seed_ms_total=%.0f seed_ms_mean=%.0f "
                "short_nations=%ld\n",
                seeds.size(), total_hubs, total_seed_ms,
                total_seed_ms / static_cast<double>(seeds.size()), total_shortfall_nations);
    for (const bucket& b : all)
        print_bucket("ALL", b);
    std::fflush(stdout);
    return 0;
}
