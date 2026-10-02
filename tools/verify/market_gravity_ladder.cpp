// market_gravity_ladder — BL-1125 (markets can die), the gravity fold's reach,
// read on a ladder of rungs over the curated seeds.
//
// THE CALIBRATION READING, NOT A GATE. MARKETS.md § Market centres and seeding:
// "reach" is a traversal cost calibrated so a 1960 world carries roughly 20-40
// markets, about one per major city — an aim for the ONE constant
// (`market_carving_params::gravity_reach`), measured here, never a count the
// rule enforces. Exit is 0 unless the instrument itself fails (C1, the folds'
// conservation, on every rung).
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
//   * C1, conservation, with one unit of every good seeded into every home
//     market's inventory and into a pool keyed by it first (the carve's own
//     markets hold none, so its row reads 0 -> 0): body-wide stock and routed
//     population agree before and after, and no pool is left keyed by a market
//     that folded;
//   * MAJOR cities (population centres at scale >= k_major_scale, 3) and how many of them
//     have no market centre within 8 grid tiles (the carve's proxy radius).
//
// THE ONE APPROXIMATION, stated. The carve folds at the carve; this folds the
// FINISHED generation world. Between the two, generation writes prices, other
// bodies, laws, provinces and pools — none of which the fold reads (it reads
// market centres, population centres and traversal cost, all fixed by then). So
// a rung's count here is the carve's count at that reach; market_census on the
// committed constant is the confirmation through the real path.
//
// Build:  bash tools/verify/build_lua_harness.sh market_gravity_ladder
// Run (repo root): ./build_gen/verify/market_gravity_ladder.exe
//                  [--seeds 46,28,...] [--rungs 0,2,4,6,8,10]

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/hard_coded_world.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/market_fold.hpp"
#include "world/world.hpp"

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

/// A MAJOR city: scale 3 (city) and up -- the curated seeds carry few or no
/// metropolises (4), so "a market per major city" is read at 3. The base line
/// prints the scale histogram so the reader can see why.
constexpr int k_major_scale = 3;

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
        if (pcc.scale >= k_major_scale)
        {
            ++r.major;
            bool near = false;
            for (const entity_id mid : ids)
            {
                const auto ct = w.tiles.find(w.markets.at(mid).centre_tile);
                if (ct != w.tiles.end() && grid_dist(ct->second, tc->second, gw) <= 8.0) { near = true; break; }
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
    std::vector<float>    rungs = { 0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f };
    for (int a = 1; a < argc; ++a)
    {
        const std::string s = argv[a];
        if (s == "--seeds" && a + 1 < argc)      seeds = parse_list<uint32_t>(argv[++a]);
        else if (s == "--rungs" && a + 1 < argc) rungs = parse_list<float>(argv[++a]);
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
        const entity_id home = base.home_body;
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
            std::printf("seed %u (home %u): base %zu markets; centres by scale 1:%d 2:%d 3:%d 4:%d 5:%d"
                        "; conquest destroyed %" PRId64 "/%" PRId64 "/%" PRId64 ", twins folded %" PRId64 "\n",
                        seed, home, base.markets.size(), hist[1], hist[2], hist[3], hist[4], hist[5],
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
                        if (corp != null_entity) w.pool_at(corp, mid).quantities[g] += 1.0f;
                    }
            }
            const market_fold_tally t = fold_markets_by_gravity(w, home, rungs[k]);
            bool orphan_pool = false;
            for (const market_fold_record& fr : t.records)
                for (const auto& [key, pool] : w.corp_market_pools)
                    if (key.second == fr.folded) orphan_pool = true;
            reading r = read_world(w, home, cfg);
            r.folds = t.folds;
            const double gb = t.inventory_before + t.pools_before;
            const double ga = t.inventory_after + t.pools_after;
            r.conserved = std::fabs(ga - gb) <= 1e-6 * std::max(1.0, std::fabs(gb))
                       && t.catchment_pop_before == t.catchment_pop_after
                       && gb > 0.0 && !orphan_pool;
            if (k + 1 == rungs.size() || rungs[k] == 14.0f)
                std::printf("  reach %5.1f  C1 stock %.1f -> %.1f, catchment %" PRId64 "k -> %" PRId64
                            "k, pools left on folded markets: %s\n",
                            static_cast<double>(rungs[k]), gb, ga, t.catchment_pop_before,
                            t.catchment_pop_after, orphan_pool ? "SOME" : "none");
            if (!r.conserved) ++failures;
            results[k].push_back(r);
            std::printf("  reach %5.1f  markets %4d (shells %3d)  pop(k) min/med/max %6.0f/%7.0f/%8.0f  "
                        "tiles med/max %5.0f/%6.0f  major %3d no-mkt %3d  %s\n",
                        static_cast<double>(rungs[k]), r.markets, r.shells, r.pop_min, r.pop_med,
                        r.pop_max, r.tiles_med, r.tiles_max, r.major, r.major_no_market,
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
        double mx = 0; int major = 0, nomk = 0, shells = 0;
        for (const reading& r : results[k])
        {
            med.push_back(r.pop_med); mx = std::max(mx, r.pop_max);
            major += r.major; nomk += r.major_no_market; shells += r.shells;
        }
        std::printf("reach %5.1f  pop med %7.0f  pop max %8.0f  shells %4d  major %4d  no market within 8 tiles %4d\n",
                    static_cast<double>(rungs[k]), nearest_rank(med, 0.5), mx, shells, major, nomk);
    }
    std::printf("\n%s (%d conservation failures)\n", failures ? "FAIL" : "OK", failures);
    return failures ? 1 : 0;
}
