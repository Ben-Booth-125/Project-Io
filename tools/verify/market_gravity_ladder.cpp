// market_gravity_ladder — BL-1125 (markets can die), the gravity fold's reach,
// read on a ladder of rungs over the curated seeds.
//
// THE CALIBRATION READING, NOT A GATE. MARKETS.md § Market centres and seeding:
// "reach" is a traversal cost calibrated so a 1960 world carries roughly 20-40
// markets, about one per major city — an aim for the ONE constant
// (`market_carving_params::gravity_reach`), measured here, never a count the
// rule enforces. Exit is 0 unless the instrument itself fails (C1, the folds'
// conservation and the catchment check, on every rung and on the legacy arc).
//
// HOW. Per seed, the shipped-arc world is generated ONCE with the gravity fold
// OFF (`gravity_reach = 0`; the twins fold and conquest still run), exactly as
// make_hard_coded_world builds it. Each rung then folds a FAITHFUL COPY of that
// world with `fold_markets_by_gravity` — the very function the carve calls —
// and reads:
//   * home-body markets standing, and how many of them are capital shells
//     (a shell is told by its price: the carve template x the 1.25 capital
//     premium on most goods, as market_census's I2 votes it);
//   * catchment population (thousands) and catchment tiles per market,
//     min / median / max, routed with `market_for_tile`;
//   * C1, with one unit of every good seeded into every home market's
//     inventory first (the shipped world has no corporation before the search,
//     so no pool exists to move): body-wide stock agrees before and after, no
//     pool is left keyed by a market that folded, and EVERY TILE OF A FOLDED
//     MARKET ROUTES TO ITS ABSORBER (`market_fold_tally::catchment_misrouted`
//     is 0);
//   * the water gate: folds whose reach crossed water, under the port gate the
//     carve applies (its own ported set, from generation_report::ported_market_centres), and -- at the committed reach -- with water ungated, so
//     the gate's effect is a number;
//   * MAJOR cities (market_readings.hpp: scale >= k_major_city_scale) and how
//     many have no market centre within k_major_city_radius grid tiles.
//
// THE LEGACY ARC (non-budget: corporations and their opening pools exist
// before the carve). Each seed is also generated there through the REAL path,
// gravity at the committed constant, and the generation report's own goods
// row must be non-zero and conserved and its catchment row must read 0
// misrouted -- the pools on folding shells exercised where they exist.
//
// THE ONE APPROXIMATION, stated. The carve folds at the carve; this folds the
// FINISHED generation world. Between the two, generation writes prices, other
// bodies, laws, provinces and pools — none of which the fold reads (it reads
// market centres, population centres and traversal cost, all fixed by then). So
// a rung's count here is the carve's count at that reach; market_census on the
// committed constant is the confirmation through the real path. The one thing
// generation writes after the fold that the fold WOULD read is the market roads
// (BL-1138, road_generation.hpp), and those are undone before any rung folds.
//
// Build:  bash tools/verify/build_lua_harness.sh market_gravity_ladder
// Run (repo root): ./build_gen/verify/market_gravity_ladder.exe
//                  [--seeds 46,28,...] [--rungs 0,6,8,10,12,14,16,20] [--no-legacy]

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/hard_coded_world.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/market_fold.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"
#include "market_readings.hpp"
#include <set>

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {


std::vector<uint32_t> library_seeds(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    std::vector<uint32_t> out;
    std::size_t pos = text.find("\"seeds\"");
    if (pos == std::string::npos) return out;
    const std::string key = "\"seed\"";
    while ((pos = text.find(key, pos)) != std::string::npos)
    {
        pos += key.size();
        std::size_t p = pos;
        while (p < text.size() && (text[p] == ' ' || text[p] == ':' || text[p] == '\t')) ++p;
        if (p < text.size() && text[p] >= '0' && text[p] <= '9')
            out.push_back(static_cast<uint32_t>(std::strtoul(text.c_str() + p, nullptr, 10)));
    }
    return out;
}

template <typename T>
std::vector<T> parse_list(const std::string& s)
{
    std::vector<T> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ','))
        if (!tok.empty()) out.push_back(static_cast<T>(std::strtod(tok.c_str(), nullptr)));
    return out;
}

struct reading
{
    int     markets = 0, shells = 0;
    double  pop_min = 0, pop_med = 0, pop_max = 0;
    double  tiles_med = 0, tiles_max = 0;
    int     major = 0, major_no_market = 0;
    bool    conserved = true;
    int     folds = 0;
    int     across_water = 0;
};

double nearest_rank(std::vector<double> v, double p)
{
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    return v[static_cast<std::size_t>(p * static_cast<double>(v.size() - 1))];
}

double grid_dist(const tile_component& a, const tile_component& b, int gw)
{
    int dc = std::abs(a.grid_x - b.grid_x);
    if (dc > gw / 2) dc = gw - dc;
    const int dr = std::abs(a.grid_y - b.grid_y);
    return std::sqrt(static_cast<double>(dc) * dc + static_cast<double>(dr) * dr);
}

reading read_world(world& w, entity_id home, const world_gen_config& cfg)
{
    reading r;
    std::vector<entity_id> ids;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == home) ids.push_back(mid);
    std::sort(ids.begin(), ids.end());
    r.markets = static_cast<int>(ids.size());

    for (const entity_id mid : ids)
    {
        const market_component& mc = w.markets.at(mid);
        int prem = 0, plain = 0;
        for (std::size_t k = 0; k < resource_count; ++k)
        {
            const float tmpl = cfg.kepler_base_price[k];
            if (tmpl <= 0.0f) continue;
            if (mc.base_price[k] == tmpl * 1.25f) ++prem;
            else if (mc.base_price[k] == tmpl) ++plain;
        }
        if (prem > plain) ++r.shells;
    }

    std::map<entity_id, double> pop, tiles;
    for (const entity_id mid : ids) { pop[mid] = 0; tiles[mid] = 0; }
    const std::vector<entity_id>& grid = body_tile_grid(w, home);
    for (const entity_id tid : grid)
    {
        if (tid == null_entity) continue;
        const entity_id m = market_for_tile(w, tid);
        if (tiles.count(m)) tiles[m] += 1;
    }
    const int gw = w.bodies.at(home).grid_width;
    std::vector<entity_id> centres;
    for (const auto& [cid, pcc] : w.population_centres) centres.push_back(cid);
    std::sort(centres.begin(), centres.end());
    for (const entity_id cid : centres)
    {
        const auto tit = w.population_centre_tile.find(cid);
        if (tit == w.population_centre_tile.end()) continue;
        const auto tc = w.tiles.find(tit->second);
        if (tc == w.tiles.end() || tc->second.body != home) continue;
        const population_centre_component& pcc = w.population_centres.at(cid);
        const entity_id m = market_for_tile(w, tit->second);
        if (pop.count(m)) pop[m] += pcc.population;
        if (pcc.scale >= k_major_city_scale)
        {
            ++r.major;
            bool near = false;
            for (const entity_id mid : ids)
            {
                const auto ct = w.tiles.find(w.markets.at(mid).centre_tile);
                if (ct != w.tiles.end() && grid_dist(ct->second, tc->second, gw) <= k_major_city_radius) { near = true; break; }
            }
            if (!near) ++r.major_no_market;
        }
    }
    std::vector<double> pv, tv;
    for (const auto& [m, v] : pop) pv.push_back(v);
    for (const auto& [m, v] : tiles) tv.push_back(v);
    r.pop_min = nearest_rank(pv, 0.0);
    r.pop_med = nearest_rank(pv, 0.5);
    r.pop_max = nearest_rank(pv, 1.0);
    r.tiles_med = nearest_rank(tv, 0.5);
    r.tiles_max = nearest_rank(tv, 1.0);
    return r;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    std::vector<float>    rungs = { 0.0f, 6.0f, 8.0f, 10.0f, 12.0f, 14.0f, 16.0f, 20.0f };
    bool legacy = true;
    for (int a = 1; a < argc; ++a)
    {
        const std::string s = argv[a];
        if (s == "--seeds" && a + 1 < argc)      seeds = parse_list<uint32_t>(argv[++a]);
        else if (s == "--rungs" && a + 1 < argc) rungs = parse_list<float>(argv[++a]);
        else if (s == "--no-legacy")             legacy = false;
    }
    if (seeds.empty())
        seeds = library_seeds("docs/generation/seed_library.json");
    if (seeds.empty())
    {
        std::printf("FATAL  no seeds (run from the repo root)\n");
        return 2;
    }

    std::printf("market_gravity_ladder — BL-1125, the gravity fold's reach on %zu seeds, %zu rungs\n",
                seeds.size(), rungs.size());
    lua_state lua;
    int failures = 0;
    // results[rung][seed]
    std::vector<std::vector<reading>> results(rungs.size());

    for (const uint32_t seed : seeds)
    {
        world_params p = arc_params(world_arc::shipped);
        p.seed = seed;
        world_gen_config cfg;
        works_registry   works;
        load_app_generation_inputs(lua, cfg, works);
        cfg.market_carving.gravity_reach = 0.0f; // the base: twins and conquest only
        generation_report rep;
        world base = make_hard_coded_world(p, &rep, cfg, /*progress=*/nullptr, &works);
        // BL-1138: generation lays the market roads AFTER the folds, for the markets
        // that survive them -- here, with the fold off, for the unfolded set. The
        // shipped fold reads the network as it stood BEFORE that pass, so the pass's
        // raises are undone (its trace records each one) and every rung folds on the
        // network the carve's own fold read.
        undo_market_roads(base, rep.market_road_links);
        const entity_id home = base.home_body;
        // The carve's own port set, as it computed it (bound by each market's
        // seeding population tile), carried on the report.
        const std::set<entity_id> ports(rep.ported_market_centres.begin(), rep.ported_market_centres.end());
        {
            int hist[6] = { 0, 0, 0, 0, 0, 0 };
            for (const auto& [cid, pcc] : base.population_centres)
            {
                const auto tit = base.population_centre_tile.find(cid);
                if (tit == base.population_centre_tile.end()) continue;
                const auto tc = base.tiles.find(tit->second);
                if (tc == base.tiles.end() || tc->second.body != home) continue;
                ++hist[std::clamp(pcc.scale, 0, 5)];
            }
            std::printf("seed %u (home %u): base %zu markets (%zu on ported centres); centres by scale 1:%d 2:%d 3:%d 4:%d 5:%d"
                        "; conquest destroyed %" PRId64 "/%" PRId64 "/%" PRId64 ", twins folded %" PRId64 "\n",
                        seed, home, base.markets.size(), ports.size(), hist[1], hist[2], hist[3], hist[4], hist[5],
                        rep.markets_destroyed_by_conquest[0], rep.markets_destroyed_by_conquest[1],
                        rep.markets_destroyed_by_conquest[2], rep.markets_folded_twins);
        }
        for (std::size_t k = 0; k < rungs.size(); ++k)
        {
            world w = base; // faithful copy (faithful_unordered_map)
            // C1 WITH STOCK IN IT. At the carve the markets hold no inventory
            // and no pool is keyed by them yet, so the generation path's own
            // conservation row reads 0 -> 0. Here every home market is handed
            // one unit of every good in its inventory and in a pool of the
            // lowest-id corporation before the fold, so the row tests that a
            // fold MOVES stock rather than that there was none to lose; and
            // no pool may stay keyed by a market that folded.
            {
                entity_id corp = null_entity;
                for (const auto& [cid, cc] : w.corporations)
                    if (corp == null_entity || cid < corp) corp = cid;
                std::vector<entity_id> mids;
                for (const auto& [mid, mc] : w.markets)
                    if (mc.body == home) mids.push_back(mid);
                std::sort(mids.begin(), mids.end());
                for (const entity_id mid : mids)
                    for (std::size_t g = 0; g < resource_count; ++g)
                    {
                        w.markets.at(mid).inventory[g] += 1.0f;
                        (void)corp; // BL-1265: no pools to stock
                    }
            }
            if (rungs[k] == world_gen_config{}.market_carving.gravity_reach)
            {
                world u = w; // the same rung with water UNGATED: what the port gate refuses
                const market_fold_tally tu = fold_markets_by_gravity(u, home, rungs[k], nullptr);
                std::printf("  reach %5.1f  water ungated: markets %d, folds across water %d\n",
                            static_cast<double>(rungs[k]), read_world(u, home, cfg).markets,
                            tu.folds_across_water);
            }
            const market_fold_tally t = fold_markets_by_gravity(w, home, rungs[k], &ports);
            bool orphan_pool = false; // BL-1265: corporations hold no pools to orphan
            reading r = read_world(w, home, cfg);
            r.folds = t.folds;
            r.across_water = t.folds_across_water;
            const double gb = t.inventory_before + t.pools_before;
            const double ga = t.inventory_after + t.pools_after;
            r.conserved = std::fabs(ga - gb) <= 1e-6 * std::max(1.0, std::fabs(gb))
                       && t.catchment_misrouted == 0
                       && gb > 0.0 && !orphan_pool;
            if (k + 1 == rungs.size() || rungs[k] == world_gen_config{}.market_carving.gravity_reach)
                std::printf("  reach %5.1f  C1 stock %.1f -> %.1f; %" PRId64 " tiles moved with their market, "
                            "%" PRId64 " misrouted; pools left on folded markets: %s\n",
                            static_cast<double>(rungs[k]), gb, ga, t.catchment_tiles_moved,
                            t.catchment_misrouted, orphan_pool ? "SOME" : "none");
            if (!r.conserved) ++failures;
            results[k].push_back(r);
            std::printf("  reach %5.1f  markets %4d (shells %3d)  pop(k) min/med/max %6.0f/%7.0f/%8.0f  "
                        "tiles med/max %5.0f/%6.0f  major %3d no-mkt %3d  water %2d  %s\n",
                        static_cast<double>(rungs[k]), r.markets, r.shells, r.pop_min, r.pop_med,
                        r.pop_max, r.tiles_med, r.tiles_max, r.major, r.major_no_market, r.across_water,
                        r.conserved ? "C1 ok" : "C1 FAIL");
            std::fflush(stdout);
        }
    }

    std::printf("\n=== per rung, per seed: markets standing ===\nreach ");
    for (const uint32_t s : seeds) std::printf("%6u", s);
    std::printf("   median   in 20-40\n");
    for (std::size_t k = 0; k < rungs.size(); ++k)
    {
        std::printf("%5.1f ", static_cast<double>(rungs[k]));
        std::vector<double> m;
        int in_band = 0;
        for (const reading& r : results[k])
        {
            std::printf("%6d", r.markets);
            m.push_back(r.markets);
            if (r.markets >= 20 && r.markets <= 40) ++in_band;
        }
        std::printf("   %6.0f   %d/%zu\n", nearest_rank(m, 0.5), in_band, results[k].size());
    }
    std::printf("\n=== per rung, pooled: catchment population (k) median of seed medians / max, major cities without a market ===\n");
    for (std::size_t k = 0; k < rungs.size(); ++k)
    {
        std::vector<double> med;
        double mx = 0; int major = 0, nomk = 0, shells = 0, water = 0;
        for (const reading& r : results[k])
        {
            med.push_back(r.pop_med); mx = std::max(mx, r.pop_max);
            major += r.major; nomk += r.major_no_market; shells += r.shells; water += r.across_water;
        }
        std::printf("reach %5.1f  pop med %7.0f  pop max %8.0f  shells %4d  major %4d  no market within %.0f tiles %4d  folds across water %3d\n",
                    static_cast<double>(rungs[k]), nearest_rank(med, 0.5), mx, shells, major,
                    k_major_city_radius, nomk, water);
    }

    if (legacy)
    {
        std::printf("\n=== the legacy arc (non-budget): the real path, gravity at the committed %.1f ===\n",
                    static_cast<double>(world_gen_config{}.market_carving.gravity_reach));
        for (const uint32_t seed : seeds)
        {
            world_params p = arc_params(world_arc::legacy);
            p.seed = seed;
            world_gen_config cfg;
            works_registry   works;
            load_app_generation_inputs(lua, cfg, works);
            generation_report rep;
            world w = make_hard_coded_world(p, &rep, cfg, /*progress=*/nullptr, &works);
            const double gb = rep.market_fold_goods_before, ga = rep.market_fold_goods_after;
            const bool ok = gb > 0.0 && std::fabs(ga - gb) <= 1e-6 * std::max(1.0, std::fabs(gb))
                         && rep.market_fold_misrouted == 0;
            if (!ok) ++failures;
            std::printf("  seed %4u  markets %4zu  corps %4zu  folds twins %3" PRId64 " gravity %4" PRId64
                        " (shells %3" PRId64 ", across water %2" PRId64 ")  goods %.1f -> %.1f  tiles moved %" PRId64
                        " misrouted %" PRId64 "  %s\n",
                        seed, w.markets.size(), w.corporations.size(), rep.markets_folded_twins,
                        rep.markets_folded_gravity, rep.shells_folded, rep.markets_folded_across_water,
                        gb, ga, rep.market_fold_tiles_moved, rep.market_fold_misrouted,
                        ok ? "C1 ok" : "C1 FAIL");
            std::fflush(stdout);
        }
    }
    std::printf("\n%s (%d conservation failures)\n", failures ? "FAIL" : "OK", failures);
    return failures ? 1 : 0;
}
