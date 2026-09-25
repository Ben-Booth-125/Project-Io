// centre_census — how many population centres a generated world carries, how
// much of its land they cover, how large they stand, and whether each one fits
// the ground of the region that grew it.
//
//   BL-1130 (centres consolidate), task 1 — THE CENSUS the rule is measured
//   against (Rule 0b: measure first). The roads lane found (BL-1119 D1) that
//   centres covered 49-100% of land on the curated seeds, so every centre being
//   a street made the road field a lattice. POPULATION.md § Generation, "Growth
//   consolidates", wants a few cities over many towns over a train of villages,
//   and no region carrying more centres than its own cell of the settlement
//   partition holds. Per curated seed, on the home body:
//     * THE SIM RECORD — regions, living regions, regions standing centres; the
//       sum of `region::centres` (what the carve will try to materialise) and of
//       `urban_population`; the largest region count and how many regions sit
//       at `region_centre_limit`.
//     * THE CARVE — carved centres founded (`world::gen_carve_centres`), carved
//       centres dropped (`world::gen_carve_dropped`, by reason), and the two
//       region-blind passes' foundings (province anchors / national coverage).
//     * DENSITY — every centre on the body, the land tiles hosting one (the
//       share of land that is a centre, i.e. a street), and the land stamped
//       urban by the footprints (`land_use` urban).
//     * SCALE — every centre by `k_population_for_scale` rung: village (10k),
//       town (50k), city (200k), metropolis (1M), megacity (5M).
//     * FIT — SPILLS: carved centres standing outside their source region's
//       cell (`nearest_region` of the centre's tile differs from the carve
//       slot's region). OVERFULL: regions whose carved centres' urban
//       footprints (`k_urban_footprint_tiles` by the CARVE's scale, which is
//       the body-wide rank-size's) add up to more land tiles than the
//       region's own cell holds — split out, the regions carrying ONE carved
//       centre, which the rule's floor keeps whatever its footprint; regions
//       carrying more carved centres than their cell has placeable tiles;
//       PLACELESS regions (living, standing centres, a cell with no placeable
//       tile — the only regions the carve could ever spill); GROUNDLESS
//       regions (living, a cell with no land at all — Ben's 2026-09-25 ruling
//       gives them no centre); and the SIM RECORD against the final
//       partition: living regions with more than one centre whose OWN
//       hierarchy's footprint outruns their cell's land (`hierarchy_footprint`
//       below, a mirror of settlement.cpp's `region_centre_footprint` — the
//       quantity `region_centres_fit` judges — restated so the census still
//       builds on a tree without BL-1130, which is where its BEFORE reading is
//       taken; on such a tree the row reads the old count as if it were a
//       hierarchy and means nothing).
//     * THE HISTORY — per span (Empires, Exploration, Industrialisation), the
//       battles, conquests and foundings the generation report carries, since
//       the Era -1 sim reads `region::centres` (BL-1130 round 2 attributes the
//       move the rule makes in them).
//     * WHERE THE DENSITY IS — living regions whose cell is ONE land tile
//       (the settle verb founds on the nearest free tile, so settled cores
//       pack a region to a tile), and how many carved centres stand in such
//       cells, against those in larger cells and the region-blind foundings.
//     * WHAT FOLLOWS — road tiles on the body (`road_level > 0`), split into
//       STREETS (a road on a centre's own tile) and the network; and markets on
//       the body (`world::markets`), which the carve gates on centres.
//
// A READING, NOT A GATE. Nothing here asserts a centre count, a land share or a
// scale mix: those are what the rule is ruled against, and a harness that
// pinned them would be making that call. The one failing row (exit 1) is the
// instrument's own honesty check: a world with no settlement record, or no
// carved centre, would make every fit row vacuous.
//
// READ-ONLY OVER src/world/*. It calls the world's own functions and nothing in
// src/ changes for it.
//
// ---------------------------------------------------------------------------
// THE WORLD — harness_params' app-order builder, first half
// ---------------------------------------------------------------------------
// `build_app_base_world` (harness_params.hpp): the parsed world_gen config and
// the works table handed to `make_hard_coded_world`, then setup_world's two
// world writes and load_economy's recipe pass — the app's own sequence up to
// the landscape search. The search and the settle are NOT run: every quantity
// read here (the centre set, the footprints, the road field, the carve's
// markets) is written by generation and is what the search starts from. The
// search lays firms and a road tier over it; it founds no centre.
//
// CELL. A region is an anchor, not a stored tile set; its extent is the
// Voronoi `nearest_region` reads (the SAME call the carve partitions its
// candidates with), taken on the FINAL region table (`world::gen_settlement`).
// A cell's LAND is its non-water tiles; its PLACEABLE ground is the tiles that
// pass `placement_rules::can_place_population_centre`, the carve's own gate.
//
// Build:  bash tools/verify/build_lua_harness.sh centre_census
// Run:    ./build_gen/verify/centre_census.exe [--seeds 46,28] [--map DIR]
//         (from the repo root: it loads scripts/ and the seed library by
//         relative path). --map DIR writes roads_seed<S>.ppm, one pixel a
//         tile, in gen_step_costs' --roads-map colours: water dark blue, bare
//         land tan, a STREET (a road on a centre's tile) orange, Track brown,
//         Road red, Highway black; and centres_seed<S>.ppm, every centre by
//         origin: carved village orange, town red, city or larger dark red,
//         province anchor yellow, national coverage violet.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/placement_rules.hpp"
#include "world/population_generation.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using clk = std::chrono::steady_clock;

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

/// MIRROR of settlement.cpp `region_centre_footprint` (BL-1130), restated so the
/// census builds on a tree that predates it (see the header). Centre k of n
/// stands U/(k*H_n) heads, banded to the nearest `k_population_for_scale` rung
/// in log space (the geometric midpoints between the rungs), and paves
/// `k_urban_footprint_tiles` of that rung.
int hierarchy_footprint(int64_t urban_heads, int n)
{
    constexpr int64_t bands[4] = { 22360, 100000, 447213, 2236067 };
    if (n <= 0) return 0;
    n = std::min(n, 32);
    const int64_t u = std::clamp<int64_t>(urban_heads, 0, int64_t{1} << 40);
    int64_t h = 0;
    for (int i = 1; i <= n; ++i) h += 1000000 / i;
    int tiles = 0;
    for (int k = 1; k <= n; ++k)
    {
        const int64_t share = (u * 1000000) / (static_cast<int64_t>(k) * h);
        int s = 1;
        for (int i = 0; i < 4; ++i)
            if (share >= bands[i]) s = i + 2;
        tiles += k_urban_footprint_tiles[s - 1];
    }
    return tiles;
}

std::vector<uint32_t> parse_seed_list(const std::string& s)
{
    std::vector<uint32_t> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ','))
        if (!tok.empty())
            out.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
    return out;
}

struct seed_record
{
    uint32_t seed = 0;
    double   build_s = 0.0;
    bool     ok = false;

    // The sim record.
    int     regions = 0, living = 0, standing = 0;
    int64_t sim_centres = 0, sim_urban = 0;
    int     max_region_centres = 0, at_limit = 0;
    int     region_centre_hist[6] = {}; // 0, 1, 2-4, 5-9, 10-19, 20+

    // The body.
    int land = 0, placeable = 0;
    int cell_land_median = 0, cell_land_max = 0;

    // The carve and the two region-blind passes.
    int carved = 0, dropped_built_out = 0, dropped_no_tile = 0;
    int anchors = 0, coverage = 0;

    // Density.
    int centres = 0, centre_tiles = 0, urban_tiles = 0;
    int scale_count[5] = {};

    // Fit.
    int spilled = 0;
    int overfull_regions = 0;       ///< carved footprint sum > cell land
    int overfull_single = 0;        ///< ...of which the region carries ONE carved centre
    int64_t overfull_excess = 0;    ///< sum over those regions of (footprint - cell land)
    int overcount_regions = 0;      ///< carved count > cell placeable tiles
    int placeless_regions = 0;      ///< living, standing centres, cell with no placeable tile
    int placeless_water = 0;        ///< ...whose anchor tile is water (a shore or lake founding)
    int placeless_stacked = 0;      ///< ...whose anchor tile a lower-index region already holds
    int sim_over_cell = 0;          ///< living, centres > 1, own-hierarchy footprint > final cell land
    int groundless_regions = 0;     ///< living, cell with no land at all
    int groundless_standing = 0;    ///< ...whose sim record still carries a centre

    // The history, per span: Empires (the report's prehistory), Exploration,
    // Industrialisation.
    int64_t battles[3] = {}, conquests[3] = {}, foundings[3] = {};

    // Where the density is.
    int single_tile_cells = 0;     ///< living regions whose cell holds exactly one land tile
    int carved_in_single = 0;      ///< carved centres whose source region's cell is one land tile

    // What follows.
    int road_tiles = 0, street_tiles = 0;
    int markets = 0;
};

/// The centre map: every centre by origin, so a reader can see what the density
/// is made of. Water dark blue, bare land tan; a CARVED centre by scale —
/// village orange, town red, city and larger dark red; a province anchor yellow;
/// a national-coverage founding violet.
void write_centre_map(const world& w, entity_id body, const std::string& path)
{
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end()) return;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    if (gw <= 0 || gh <= 0) return;
    std::vector<unsigned char> px(static_cast<std::size_t>(gw) * gh * 3, 0);
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body || tc.grid_x < 0 || tc.grid_x >= gw || tc.grid_y < 0 || tc.grid_y >= gh)
            continue;
        unsigned char r = 200, g = 188, b = 150;
        if (is_water(tc.substrate)) { r = 28; g = 48; b = 88; }
        const std::size_t i = (static_cast<std::size_t>(tc.grid_y) * gw + tc.grid_x) * 3;
        px[i] = r; px[i + 1] = g; px[i + 2] = b;
    }
    for (const auto& [cid, tid] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tid);
        const auto pit = w.population_centres.find(cid);
        if (tit == w.tiles.end() || pit == w.population_centres.end()) continue;
        const tile_component& tc = tit->second;
        if (tc.body != body || tc.grid_x < 0 || tc.grid_x >= gw || tc.grid_y < 0 || tc.grid_y >= gh)
            continue;
        unsigned char r, g, b;
        if (w.gen_carve_centres.count(cid) != 0)
        {
            const int s = pit->second.scale;
            if (s <= 1)      { r = 235; g = 140; b = 30; }
            else if (s == 2) { r = 210; g = 40;  b = 30; }
            else             { r = 110; g = 0;   b = 10; }
        }
        else if (pit->second.province_anchor) { r = 250; g = 225; b = 40; }
        else                                  { r = 150; g = 60;  b = 200; }
        const std::size_t i = (static_cast<std::size_t>(tc.grid_y) * gw + tc.grid_x) * 3;
        px[i] = r; px[i + 1] = g; px[i + 2] = b;
    }
    if (FILE* f = std::fopen(path.c_str(), "wb"))
    {
        std::fprintf(f, "P6\n%d %d\n255\n", gw, gh);
        std::fwrite(px.data(), 1, px.size(), f);
        std::fclose(f);
    }
}

void write_road_map(const world& w, entity_id body, const std::set<entity_id>& centre_tiles,
                    const std::string& path)
{
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end()) return;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    if (gw <= 0 || gh <= 0) return;
    std::vector<unsigned char> px(static_cast<std::size_t>(gw) * gh * 3, 0);
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body || tc.grid_x < 0 || tc.grid_x >= gw || tc.grid_y < 0 || tc.grid_y >= gh)
            continue;
        unsigned char r = 200, g = 188, b = 150;                    // bare land
        if (is_water(tc.substrate)) { r = 28; g = 48; b = 88; }     // water
        if (tc.road_level > 0)
        {
            if (centre_tiles.count(tid) != 0) { r = 235; g = 140; b = 30; } // street
            else if (tc.road_level == 1)      { r = 95;  g = 60;  b = 25; } // Track
            else if (tc.road_level == 2)      { r = 200; g = 25;  b = 25; } // Road
            else                              { r = 0;   g = 0;   b = 0;  } // Highway
        }
        const std::size_t i = (static_cast<std::size_t>(tc.grid_y) * gw + tc.grid_x) * 3;
        px[i] = r; px[i + 1] = g; px[i + 2] = b;
    }
    if (FILE* f = std::fopen(path.c_str(), "wb"))
    {
        std::fprintf(f, "P6\n%d %d\n255\n", gw, gh);
        std::fwrite(px.data(), 1, px.size(), f);
        std::fclose(f);
    }
}

seed_record run_seed(lua_state& lua, uint32_t seed, const std::string& map_dir)
{
    seed_record r;
    r.seed = seed;

    world_params params{};
    params.seed = seed;
    app_start_world out;
    const clk::time_point t0 = clk::now();
    build_app_base_world(lua, params, out);
    r.build_s = std::chrono::duration<double>(clk::now() - t0).count();
    r.battles[0]   = out.report.prehistory_battles;
    r.conquests[0] = out.report.prehistory_conquests;
    r.foundings[0] = out.report.prehistory_foundings;
    r.battles[1]   = out.report.exploration_battles;
    r.conquests[1] = out.report.exploration_conquests;
    r.foundings[1] = out.report.exploration_foundings;
    r.battles[2]   = out.report.industrialisation_battles;
    r.conquests[2] = out.report.industrialisation_conquests;
    r.foundings[2] = out.report.industrialisation_foundings;

    const world& w = out.w;
    const entity_id body = w.home_body;
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end() || !w.gen_settlement)
        return r;
    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;
    const settlement_state& ss = *w.gen_settlement;

    // --- the sim record -----------------------------------------------------
    r.regions = static_cast<int>(ss.regions.size());
    for (const region& p : ss.regions)
    {
        if (p.population > 0) ++r.living;
        if (p.population > 0 && p.centres > 0) ++r.standing;
        if (p.population > 0) { r.sim_centres += p.centres; r.sim_urban += p.urban_population; }
        r.max_region_centres = std::max(r.max_region_centres, p.centres);
        if (p.centres >= region_centre_limit) ++r.at_limit;
        const int c = p.population > 0 ? p.centres : 0;
        const int b = c <= 0 ? 0 : c == 1 ? 1 : c <= 4 ? 2 : c <= 9 ? 3 : c <= 19 ? 4 : 5;
        ++r.region_centre_hist[b];
    }

    // --- the body, in raster order, and each tile's cell ---------------------
    std::vector<const tile_component*> grid(static_cast<std::size_t>(gw) * gh, nullptr);
    std::map<entity_id, int> tile_raster;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body) continue;
        const int idx = tc.grid_y * gw + tc.grid_x;
        if (idx < 0 || idx >= gw * gh) continue;
        grid[static_cast<std::size_t>(idx)] = &tc;
        tile_raster[tid] = idx;
    }
    std::vector<int> cell_land(ss.regions.size(), 0), cell_place(ss.regions.size(), 0);
    for (int idx = 0; idx < gw * gh; ++idx)
    {
        const tile_component* tc = grid[static_cast<std::size_t>(idx)];
        if (tc == nullptr || is_water(tc->substrate)) continue;
        ++r.land;
        const bool place = placement_rules::can_place_population_centre(*tc);
        if (place) ++r.placeable;
        const int ri = nearest_region(ss, idx % gw, idx / gw, gw);
        if (ri < 0 || ri >= static_cast<int>(ss.regions.size())) continue;
        ++cell_land[static_cast<std::size_t>(ri)];
        if (place) ++cell_place[static_cast<std::size_t>(ri)];
    }
    {
        std::vector<int> sorted;
        for (std::size_t i = 0; i < ss.regions.size(); ++i)
            if (ss.regions[i].population > 0)
            {
                sorted.push_back(cell_land[i]);
                if (cell_land[i] == 1) ++r.single_tile_cells;
            }
        std::sort(sorted.begin(), sorted.end());
        if (!sorted.empty())
        {
            r.cell_land_median = sorted[sorted.size() / 2];
            r.cell_land_max    = sorted.back();
        }
    }

    // --- every centre on the body ---------------------------------------------
    std::set<entity_id> centre_tiles;
    std::vector<int64_t> region_footprint(ss.regions.size(), 0);
    std::vector<int>     region_carved(ss.regions.size(), 0);
    for (const auto& [cid, tid] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tid);
        if (tit == w.tiles.end() || tit->second.body != body) continue;
        const auto pit = w.population_centres.find(cid);
        if (pit == w.population_centres.end()) continue;
        ++r.centres;
        centre_tiles.insert(tid);
        const int scale = std::clamp(pit->second.scale, 1, 5);
        ++r.scale_count[scale - 1];

        const auto cit = w.gen_carve_centres.find(cid);
        if (cit == w.gen_carve_centres.end())
        {
            if (pit->second.province_anchor) ++r.anchors; else ++r.coverage;
            continue;
        }
        ++r.carved;
        const int src = cit->second.region;
        const int stood = nearest_region(ss, tit->second.grid_x, tit->second.grid_y, gw);
        if (stood != src) ++r.spilled;
        if (src >= 0 && src < static_cast<int>(ss.regions.size()))
        {
            region_footprint[static_cast<std::size_t>(src)] += k_urban_footprint_tiles[scale - 1];
            ++region_carved[static_cast<std::size_t>(src)];
            if (cell_land[static_cast<std::size_t>(src)] == 1) ++r.carved_in_single;
        }
    }
    r.centre_tiles = static_cast<int>(centre_tiles.size());
    for (const carve_dropped_slot& d : w.gen_carve_dropped)
    {
        if (d.reason == carve_drop_reason::body_built_out) ++r.dropped_built_out;
        else ++r.dropped_no_tile;
    }
    for (std::size_t i = 0; i < ss.regions.size(); ++i)
    {
        if (region_footprint[i] > cell_land[i])
        {
            ++r.overfull_regions;
            if (region_carved[i] == 1) ++r.overfull_single;
            r.overfull_excess += region_footprint[i] - cell_land[i];
        }
        if (region_carved[i] > cell_place[i]) ++r.overcount_regions;
        const region& p = ss.regions[i];
        if (p.population > 0 && p.centres > 0 && cell_place[i] == 0)
        {
            ++r.placeless_regions;
            const int a = (p.row >= 0 && p.row < gh) ? p.row * gw + ((p.col % gw) + gw) % gw : -1;
            const tile_component* at = (a >= 0) ? grid[static_cast<std::size_t>(a)] : nullptr;
            bool stacked = false;
            for (std::size_t j = 0; j < i && !stacked; ++j)
                stacked = ss.regions[j].col == p.col && ss.regions[j].row == p.row;
            if (stacked) ++r.placeless_stacked;
            else if (at != nullptr && is_water(at->substrate)) ++r.placeless_water;
        }
        // The rule on the sim record itself: the region's OWN hierarchy (the
        // one `region_centres_fit` judges) against the FINAL partition's land.
        if (p.population > 0 && p.centres > 1
            && hierarchy_footprint(p.urban_population, p.centres) > cell_land[i])
            ++r.sim_over_cell;
        if (p.population > 0 && cell_land[i] == 0)
        {
            ++r.groundless_regions;
            if (p.centres > 0) ++r.groundless_standing;
        }
    }

    // --- urban ground, roads, markets -----------------------------------------
    for (const auto& [tid, lu] : w.land_use)
    {
        if (lu.use != land_use_component::type::urban) continue;
        const auto tit = w.tiles.find(tid);
        if (tit != w.tiles.end() && tit->second.body == body) ++r.urban_tiles;
    }
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body || tc.road_level == 0) continue;
        ++r.road_tiles;
        if (centre_tiles.count(tid) != 0) ++r.street_tiles;
    }
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body) ++r.markets;

    if (!map_dir.empty())
    {
        char name[64];
        std::snprintf(name, sizeof name, "/roads_seed%u.ppm", seed);
        write_road_map(w, body, centre_tiles, map_dir + name);
        std::snprintf(name, sizeof name, "/centres_seed%u.ppm", seed);
        write_centre_map(w, body, map_dir + name);
    }

    r.ok = ss.urban_map_drawn && r.carved > 0;
    return r;
}

double pct(int64_t a, int64_t b) { return b > 0 ? 100.0 * static_cast<double>(a) / static_cast<double>(b) : 0.0; }

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    std::string map_dir;
    bool from_args = false;
    for (int a = 1; a < argc; ++a)
    {
        const std::string s = argv[a];
        if (s == "--seeds" && a + 1 < argc)    { seeds = parse_seed_list(argv[++a]); from_args = true; }
        else if (s == "--map" && a + 1 < argc) map_dir = argv[++a];
        else
        {
            std::printf("usage: centre_census [--seeds a,b,c] [--map DIR]\n");
            return 2;
        }
    }
    if (seeds.empty())
    {
        seeds = library_seeds("docs/generation/seed_library.json");
        if (seeds.empty())
        {
            std::printf("FATAL  docs/generation/seed_library.json not found or carries no seeds "
                        "(run from the repo root)\n");
            return 2;
        }
    }

    std::printf("centre_census — BL-1130 (centres consolidate): density, scale and fit of the "
                "population centres\n");
    std::printf("world: the shipped arc (world_params{}), harness_params' build_app_base_world "
                "(generation + setup + recipes; no search)\n");
    std::printf("seeds (%s, %zu):", from_args ? "--seeds" : "docs/generation/seed_library.json",
                seeds.size());
    for (const uint32_t s : seeds) std::printf(" %u", s);
    std::printf("\n");
    std::fflush(stdout);

    lua_state lua; // one long-lived state, as the app keeps m_lua
    std::vector<seed_record> recs;
    for (const uint32_t seed : seeds)
    {
        recs.push_back(run_seed(lua, seed, map_dir));
        const seed_record& r = recs.back();
        std::printf("  seed %u built in %.1f s: %d centres on %d land tiles\n", r.seed, r.build_s,
                    r.centres, r.land);
        std::fflush(stdout);
    }

    std::printf("\n=== C0 the history (generation report, per span) ===\n");
    std::printf("seed | empires: battles conquests foundings | exploration: battles conquests "
                "foundings | industrialisation: battles conquests foundings | regions\n");
    for (const seed_record& r : recs)
        std::printf("%4u | %16" PRId64 " %9" PRId64 " %9" PRId64 " | %20" PRId64 " %9" PRId64
                    " %9" PRId64 " | %26" PRId64 " %9" PRId64 " %9" PRId64 " | %7d\n",
                    r.seed, r.battles[0], r.conquests[0], r.foundings[0], r.battles[1],
                    r.conquests[1], r.foundings[1], r.battles[2], r.conquests[2], r.foundings[2],
                    r.regions);

    std::printf("\n=== C1 the sim record (living regions) ===\n");
    std::printf("seed  regions  living  standing  sum_centres   urban_heads  max  @limit | "
                "per-region centres: 0  1  2-4  5-9  10-19  20+\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %7d  %6d  %8d  %11" PRId64 "  %12" PRId64 "  %3d  %6d | %22d %2d %4d %4d %6d %4d\n",
                    r.seed, r.regions, r.living, r.standing, r.sim_centres, r.sim_urban,
                    r.max_region_centres, r.at_limit, r.region_centre_hist[0],
                    r.region_centre_hist[1], r.region_centre_hist[2], r.region_centre_hist[3],
                    r.region_centre_hist[4], r.region_centre_hist[5]);

    std::printf("\n=== C2 density on the home body ===\n");
    std::printf("seed   land  placeable  cell_land(med/max) | centres  carved  anchors  coverage  "
                "dropped(built_out/no_tile) | centre_tiles  land%%   urban_tiles  urban%%\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %5d  %9d  %8d/%-8d | %7d  %6d  %7d  %8d  %13d/%-7d | %12d  %5.1f  %11d  %5.1f\n",
                    r.seed, r.land, r.placeable, r.cell_land_median, r.cell_land_max, r.centres,
                    r.carved, r.anchors, r.coverage, r.dropped_built_out, r.dropped_no_tile,
                    r.centre_tiles, pct(r.centre_tiles, r.land), r.urban_tiles,
                    pct(r.urban_tiles, r.land));

    std::printf("\n=== C3 scale (every centre, by k_population_for_scale rung) ===\n");
    std::printf("seed  village(10k)  town(50k)  city(200k)  metropolis(1M)  megacity(5M)\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %12d  %9d  %10d  %14d  %12d\n", r.seed, r.scale_count[0],
                    r.scale_count[1], r.scale_count[2], r.scale_count[3], r.scale_count[4]);

    std::printf("\n=== C4 fit (carved centres against their source region's cell) ===\n");
    std::printf("seed  spilled  spilled%%  | overfull_regions (one centre)  footprint_excess_tiles | "
                "regions_over_placeable  placeless(water/stacked/other) | groundless (standing) | "
                "sim_record_over_cell\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %7d  %7.1f  | %16d (%10d)  %22" PRId64 " | %22d  %9d (%d/%d/%d) | "
                    "%10d (%8d) | %20d\n",
                    r.seed, r.spilled, pct(r.spilled, r.carved), r.overfull_regions,
                    r.overfull_single, r.overfull_excess, r.overcount_regions, r.placeless_regions,
                    r.placeless_water, r.placeless_stacked,
                    r.placeless_regions - r.placeless_water - r.placeless_stacked,
                    r.groundless_regions, r.groundless_standing, r.sim_over_cell);

    std::printf("\n=== C5 where the density is: regions packed one to a tile ===\n");
    std::printf("seed  living  single_tile_cells  | centres  carved_in_single_tile_cells  "
                "carved_elsewhere  anchors+coverage\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %6d  %17d  | %7d  %27d  %16d  %16d\n", r.seed, r.living,
                    r.single_tile_cells, r.centres, r.carved_in_single,
                    r.carved - r.carved_in_single, r.anchors + r.coverage);

    std::printf("\n=== C6 what follows: roads and markets on the home body ===\n");
    std::printf("seed  road_tiles  road%%land  streets  network | markets\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %10d  %9.1f  %7d  %7d | %7d\n", r.seed, r.road_tiles,
                    pct(r.road_tiles, r.land), r.street_tiles, r.road_tiles - r.street_tiles,
                    r.markets);

    // Pooled.
    int64_t land = 0, centres = 0, ctiles = 0, spilled = 0, carved = 0, roads = 0, markets = 0;
    int64_t scales[5] = {};
    int fails = 0;
    for (const seed_record& r : recs)
    {
        land += r.land; centres += r.centres; ctiles += r.centre_tiles;
        spilled += r.spilled; carved += r.carved; roads += r.road_tiles; markets += r.markets;
        for (int s = 0; s < 5; ++s) scales[s] += r.scale_count[s];
        if (!r.ok) ++fails;
    }
    std::printf("\n=== pooled over %zu seeds ===\n", recs.size());
    std::printf("centres %" PRId64 " on %" PRId64 " land tiles (%.1f%% of land hosts a centre); "
                "scales %" PRId64 "/%" PRId64 "/%" PRId64 "/%" PRId64 "/%" PRId64
                "; spilled %.1f%% of carved; road tiles %" PRId64 "; markets %" PRId64 "\n",
                centres, land, pct(ctiles, land), scales[0], scales[1], scales[2], scales[3],
                scales[4], pct(spilled, carved), roads, markets);

    for (const seed_record& r : recs)
        if (!r.ok)
            std::printf("FAIL  seed %u: no settlement record / urban map, or no carved centre — "
                        "every fit row is vacuous\n", r.seed);
    std::printf("\n%s\n", fails == 0 ? "centre_census: OK (a reading; no density is asserted)"
                                     : "centre_census: FAIL (the instrument could not see its subject)");
    return fails == 0 ? 0 : 1;
}
