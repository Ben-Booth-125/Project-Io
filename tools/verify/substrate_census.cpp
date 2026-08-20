// ---------------------------------------------------------------------------
// substrate_census (NEW-10) — how CIVILISED is the generated world, in numbers?
// ---------------------------------------------------------------------------
//
// WHY THIS EXISTS. Ben, looking at a live campaign: "the current world does not
// appear physically civilised, and there is much land that is untamed." An
// ad-hoc audit put the default world at roughly 0.2-0.5% built and 0.1%
// settled — but nothing in the tree asserted, or even printed, any of it. The
// complaint was therefore a FEELING, and a feeling cannot be compared against
// next month's world. This harness turns it into a number.
//
// THIS REPORTS; IT DOES NOT GATE ON DENSITY. That is deliberate and follows
// BL-224's own settled discipline (see history_sweep.cpp's header, which makes
// the same argument at length): the first job is to show the raw spread across
// seeds, including the ugly cases, BEFORE anybody writes a threshold that hides
// them. "Built share >= X%" is a tuning target nobody has chosen yet, and
// choosing one here by implication would be the harness making a design call.
// The only PASS/FAIL rows below are STRUCTURAL — did every seed produce a
// world, is there land on it, is anything built at all — and they are stated so
// loosely that tightening them later is a deliberate act rather than a side
// effect. A fourth check ("every nation holds at least one population centre")
// was written, MEASURED, and found not to hold at 61%; it was reported as
// finding F1 rather than loosened into a green one. That is the point of the
// exercise — and on 2026-08-20 BL-463 fixed the generator instead of the test,
// so F1 is now check S4 and it gates.
//
// WHAT IS MEASURED, AND WHEN
// --------------------------
// The census runs at the point the PLAYER first sees the world, which is not
// the point `make_hard_coded_world` returns. `app::start_new_game` orders it:
//
//     setup_world (make_hard_coded_world)
//       -> load_economy            (recipes/economy/world_gen from Lua)
//       -> assign_default_recipes
//       -> generate_background_firms   <-- BL-365: the world's REAL industry
//       -> assign_default_recipes
//       -> pre-game warm start
//
// A census taken before `generate_background_firms` reports a near-empty world
// and is simply wrong: the eight generated specialist corps are the SPACE-
// interested minority, and the mundane industry that saturates the base is
// authored by that pass. So this harness mirrors the ordering above exactly,
// and stops short of the warm start — those are the GENERATED conditions, the
// ground the player is handed, before any tick has moved anything.
//
// LIVE LUA, NOT A HAND-BUILT REGISTRY, and this is load-bearing rather than
// tidiness. `generate_background_firms`'s stop condition is measured against
// `population_demand` + `background_demand`, whose baskets are authored in
// scripts/economy.lua and are ALL ZERO on a default-constructed registry. With
// an empty registry the pass reads "demand fully met" on its first iteration
// and places ZERO firms — verified, not assumed (seed_sweep_probe prints
// `firms=0` for exactly this reason). Run from the repo root.
//
// PREHISTORY AT ITS DEFAULT (400 years). Every other world-shaped harness opts
// out via `no_prehistory()` because the era is not their subject. Here it is:
// the region set the Era -1 sim leaves behind is what the population centres,
// roads and markets are carved onto, so a census run without it would describe
// a world nobody plays. That makes this harness STRUCTURALLY long-running, not
// slow by accident. Measured 2026-08-18, MSVC 14.44, one box:
//   Release  ~1.0 s/world   (5 seeds in 5.0 s)
//   Debug    ~62  s/world   (1 seed in 62.1 s)
// The three-seed default therefore costs ~186 s in a Debug tree — past the 60 s
// default ctest tier and within 25% of the 240 s long tier, which is the
// "flaky by luck" shape BL-288 re-tiered the suite to get rid of. It is
// registered as a `sweep` for that reason; see its CMake block.
//
// Usage:  substrate_census [seed_count]      (default 3)
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/hard_coded_world.hpp"
#include "world/recipe_registry.hpp"
#include "world/works_roster.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
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

constexpr int kNumComposition = 12; // terrain_composition, barren..urban
constexpr int kNumLandform    = 7;  // terrain_landform, plains..rift

const char* const kCompositionName[kNumComposition] = {
    "barren", "rocky", "volcanic", "icy", "tundra", "grassland",
    "forest", "wetland", "ocean", "regolith", "metallic", "urban" };

const char* const kLandformName[kNumLandform] = {
    "plains", "highland", "mountain", "canyon", "valley", "crater", "rift" };

const char* const kBuildingName[building_type_count] = {
    "none", "extraction_site", "processing_facility", "port",
    "launchpad", "inland_logistics_hub", "military_base", "research_institute" };

const char* focus_name(industrial_focus f)
{
    switch (f)
    {
        case industrial_focus::extraction: return "extraction";
        case industrial_focus::processing: return "processing";
        case industrial_focus::trade:      return "trade";
    }
    return "?";
}

/// The proximity ladder the "civilised" proxy is reported at. One number would
/// hide the shape: a world whose reach jumps from 5% to 40% between N=1 and N=5
/// is a world of tight dense pockets, and a world that crawls from 5% to 9% is
/// one of scattered specks. Those want different fixes.
constexpr int kReachRings[] = { 0, 1, 2, 3, 5, 8 };
constexpr int kReachRingCount = static_cast<int>(sizeof(kReachRings) / sizeof(kReachRings[0]));
constexpr int kReachMax = 8;

/// min / median / max of a per-nation distribution, plus how many nations sit
/// at zero. Median rather than mean throughout: one metropolis-heavy nation
/// would drag a mean somewhere no nation actually is.
struct spread
{
    int lo = 0, med = 0, hi = 0, zeros = 0, n = 0;
};

spread spread_of(std::vector<int> v)
{
    spread s;
    s.n = static_cast<int>(v.size());
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    s.lo  = v.front();
    s.hi  = v.back();
    s.med = v[v.size() / 2];
    for (int x : v) if (x == 0) ++s.zeros;
    return s;
}

/// One world's census. Every field is a figure NEW-10 named at filing, plus the
/// three the cross-seed summary is built from.
struct census_row
{
    uint32_t seed = 0;
    int64_t  ms   = 0;

    std::string home_name;
    int home_w = 0, home_h = 0;
    int tiles_total = 0, land = 0, ocean = 0;
    std::array<int, kNumComposition> comp{};
    std::array<int, kNumLandform>    landform{}; // land only

    int nations = 0;

    int    pop_centres = 0;
    int64_t population = 0;   // thousands, as population_centre_component stores it
    spread pop_per_nation;

    int corps = 0, corps_player = 0, corps_specialist = 0, corps_background = 0;
    std::array<int, 3> corp_focus{};

    int buildings = 0;
    std::array<int, building_type_count> building_by_type{};
    spread buildings_per_nation;

    int road_tiles = 0;
    std::array<int, 4> road_by_tier{}; // 0 unused; 1 Track, 2 Road, 3 Highway
    int nations_no_road = 0;

    int markets_total = 0, markets_home = 0;
    int nations_no_market = 0;

    /// Share of home-body LAND within ring i of any building / settlement /
    /// road tile, per-mille. Index parallels kReachRings.
    std::array<int, kReachRingCount> reach_q{};
};

/// Cardinal neighbours on a body raster, columns wrapped and rows not — the
/// same adjacency world_audit's fragmentation pass and the road generator use,
/// so a "distance" here means the same thing it means elsewhere in the tree.
int neighbours_of(int idx, int gw, int gh, int out[4])
{
    const int col = idx % gw;
    const int row = idx / gw;
    int n = 0;
    out[n++] = row * gw + ((col + 1) % gw);
    out[n++] = row * gw + ((col - 1 + gw) % gw);
    if (row > 0)      out[n++] = (row - 1) * gw + col;
    if (row < gh - 1) out[n++] = (row + 1) * gw + col;
    return n;
}

census_row measure(const world& w, uint32_t seed)
{
    census_row r;
    r.seed = seed;

    const entity_id home = w.home_body;
    const auto      hit  = w.bodies.find(home);
    if (hit == w.bodies.end())
        return r;
    r.home_name = hit->second.name;
    r.home_w    = hit->second.grid_width;
    r.home_h    = hit->second.grid_height;
    const int total = r.home_w * r.home_h;
    if (total <= 0)
        return r;

    // --- Home-body rasters -------------------------------------------------
    // Built once and shared by the terrain histogram, the road count and the
    // reach BFS: three walks of an unordered_map with 31k entries is the
    // dominant cost of this function otherwise.
    std::vector<char>      is_land(static_cast<std::size_t>(total), 0);
    std::vector<char>      is_civil(static_cast<std::size_t>(total), 0); // building | settlement | road

    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != home) continue;
        ++r.tiles_total;
        const int ci = static_cast<int>(tc.composition);
        if (ci >= 0 && ci < kNumComposition) ++r.comp[static_cast<std::size_t>(ci)];

        const bool ocean = (tc.composition == terrain_composition::ocean);
        if (ocean) { ++r.ocean; }
        else
        {
            ++r.land;
            const int li = static_cast<int>(tc.landform);
            if (li >= 0 && li < kNumLandform) ++r.landform[static_cast<std::size_t>(li)];
        }

        const int idx = tc.grid_y * r.home_w + tc.grid_x;
        if (idx < 0 || idx >= total) continue;
        if (!ocean) is_land[static_cast<std::size_t>(idx)] = 1;

        if (tc.road_level > 0)
        {
            ++r.road_tiles;
            const int t = std::min<int>(tc.road_level, 3);
            ++r.road_by_tier[static_cast<std::size_t>(t)];
            if (!ocean) is_civil[static_cast<std::size_t>(idx)] = 1;
        }
    }

    // --- Nations -----------------------------------------------------------
    r.nations = static_cast<int>(w.nations.size());
    std::vector<entity_id> nation_ids;
    nation_ids.reserve(w.nations.size());
    for (const auto& [nid, nc] : w.nations) { (void)nc; nation_ids.push_back(nid); }
    std::sort(nation_ids.begin(), nation_ids.end()); // unordered_map -> sorted walk

    std::map<entity_id, int> pop_by_nation, buildings_by_nation;
    std::set<entity_id>      nations_with_road, nations_with_market;
    for (const entity_id nid : nation_ids)
    {
        pop_by_nation[nid]       = 0;
        buildings_by_nation[nid] = 0;
    }

    auto nation_of_tile = [&](entity_id tid) -> entity_id {
        const auto it = w.tile_to_nation.find(tid);
        return (it == w.tile_to_nation.end()) ? null_entity : it->second;
    };

    for (const entity_id nid : nation_ids)
        for (const entity_id tid : w.nations.at(nid).tiles)
        {
            const auto tit = w.tiles.find(tid);
            if (tit != w.tiles.end() && tit->second.road_level > 0)
            {
                nations_with_road.insert(nid);
                break;
            }
        }
    r.nations_no_road = r.nations - static_cast<int>(nations_with_road.size());

    // --- Population centres ------------------------------------------------
    for (const auto& [cid, pcc] : w.population_centres)
    {
        ++r.pop_centres;
        r.population += pcc.population;
        const auto tile_it = w.population_centre_tile.find(cid);
        if (tile_it == w.population_centre_tile.end()) continue;
        const auto tit = w.tiles.find(tile_it->second);
        if (tit == w.tiles.end()) continue;
        const entity_id nid = nation_of_tile(tile_it->second);
        if (nid != null_entity && pop_by_nation.count(nid)) ++pop_by_nation[nid];
        if (tit->second.body == home)
        {
            const int idx = tit->second.grid_y * r.home_w + tit->second.grid_x;
            if (idx >= 0 && idx < total && is_land[static_cast<std::size_t>(idx)])
                is_civil[static_cast<std::size_t>(idx)] = 1;
        }
    }
    {
        std::vector<int> v;
        v.reserve(pop_by_nation.size());
        for (const auto& [nid, n] : pop_by_nation) { (void)nid; v.push_back(n); }
        r.pop_per_nation = spread_of(std::move(v));
    }

    // --- Corporations ------------------------------------------------------
    for (const auto& [cid, cc] : w.corporations)
    {
        ++r.corps;
        if (cc.is_player || cid == w.player_entity) ++r.corps_player;
        else if (cc.is_background)                  ++r.corps_background;
        else                                        ++r.corps_specialist;
        const int f = static_cast<int>(cc.focus);
        if (f >= 0 && f < 3) ++r.corp_focus[static_cast<std::size_t>(f)];
    }

    // --- Buildings ---------------------------------------------------------
    // Walked from `w.buildings` rather than from each corp's `assets`: the
    // world is the authority on what stands on the ground, and a building whose
    // owner lost track of it is still a building on a tile.
    for (const auto& [bid, bc] : w.buildings)
    {
        (void)bid;
        ++r.buildings;
        const int t = static_cast<int>(bc.type);
        if (t >= 0 && t < building_type_count) ++r.building_by_type[static_cast<std::size_t>(t)];

        const entity_id nid = nation_of_tile(bc.tile);
        if (nid != null_entity && buildings_by_nation.count(nid)) ++buildings_by_nation[nid];

        const auto tit = w.tiles.find(bc.tile);
        if (tit != w.tiles.end() && tit->second.body == home)
        {
            const int idx = tit->second.grid_y * r.home_w + tit->second.grid_x;
            if (idx >= 0 && idx < total && is_land[static_cast<std::size_t>(idx)])
                is_civil[static_cast<std::size_t>(idx)] = 1;
        }
    }
    {
        std::vector<int> v;
        v.reserve(buildings_by_nation.size());
        for (const auto& [nid, n] : buildings_by_nation) { (void)nid; v.push_back(n); }
        r.buildings_per_nation = spread_of(std::move(v));
    }

    // --- Markets -----------------------------------------------------------
    for (const auto& [mid, mc] : w.markets)
    {
        (void)mid;
        ++r.markets_total;
        if (mc.body == home) ++r.markets_home;
        if (mc.centre_tile == null_entity) continue;
        const entity_id nid = nation_of_tile(mc.centre_tile);
        if (nid != null_entity) nations_with_market.insert(nid);
    }
    r.nations_no_market = r.nations - static_cast<int>(nations_with_market.size());

    // --- The civilised-reach proxy ----------------------------------------
    // Multi-source BFS from every land tile carrying a building, a settlement
    // or a road, over LAND ONLY. Ocean blocks: the question is how much of the
    // ground a person could walk is near something built, and a coastal strip
    // is not "reached" by a town across a sea.
    //
    // Deliberately the closest single proxy for the complaint and nothing more
    // — it counts PRESENCE, not scale, so one hut and one metropolis light the
    // same tile. That is the honest reading of "much land that is untamed".
    {
        std::vector<int16_t> dist(static_cast<std::size_t>(total), -1);
        std::vector<int>     frontier, next;
        for (int i = 0; i < total; ++i)
            if (is_civil[static_cast<std::size_t>(i)])
            {
                dist[static_cast<std::size_t>(i)] = 0;
                frontier.push_back(i);
            }
        for (int d = 1; d <= kReachMax && !frontier.empty(); ++d)
        {
            next.clear();
            for (const int idx : frontier)
            {
                int nb[4];
                const int n = neighbours_of(idx, r.home_w, r.home_h, nb);
                for (int i = 0; i < n; ++i)
                {
                    const std::size_t k = static_cast<std::size_t>(nb[i]);
                    if (!is_land[k] || dist[k] >= 0) continue;
                    dist[k] = static_cast<int16_t>(d);
                    next.push_back(nb[i]);
                }
            }
            frontier.swap(next);
        }
        for (int ri = 0; ri < kReachRingCount; ++ri)
        {
            int within = 0;
            for (int i = 0; i < total; ++i)
            {
                const std::size_t k = static_cast<std::size_t>(i);
                if (!is_land[k]) continue;
                if (dist[k] >= 0 && dist[k] <= kReachRings[ri]) ++within;
            }
            r.reach_q[static_cast<std::size_t>(ri)] =
                r.land ? static_cast<int>((static_cast<int64_t>(within) * 1000) / r.land) : 0;
        }
    }

    return r;
}

void print_row(const census_row& r)
{
    const double land_pc = r.land ? 100.0 / r.land : 0.0;

    std::printf("\n================ seed %u ================\n", r.seed);
    std::printf("home body %s  %dx%d = %d tiles   land %d (%.1f%%)   ocean %d (%.1f%%)   [%lld ms]\n",
                r.home_name.c_str(), r.home_w, r.home_h, r.tiles_total,
                r.land,  r.tiles_total ? 100.0 * r.land  / r.tiles_total : 0.0,
                r.ocean, r.tiles_total ? 100.0 * r.ocean / r.tiles_total : 0.0,
                static_cast<long long>(r.ms));

    std::printf("  composition (of all tiles):");
    for (int i = 0; i < kNumComposition; ++i)
        if (r.comp[static_cast<std::size_t>(i)] > 0)
            std::printf("  %s %d (%.1f%%)", kCompositionName[i], r.comp[static_cast<std::size_t>(i)],
                        r.tiles_total ? 100.0 * r.comp[static_cast<std::size_t>(i)] / r.tiles_total : 0.0);
    std::printf("\n  landform    (of land):");
    for (int i = 0; i < kNumLandform; ++i)
        std::printf("  %s %d (%.1f%%)", kLandformName[i], r.landform[static_cast<std::size_t>(i)],
                    r.landform[static_cast<std::size_t>(i)] * land_pc);
    std::printf("\n");

    std::printf("\nnations %d\n", r.nations);

    std::printf("population centres %d  (%lld thousand people)\n",
                r.pop_centres, static_cast<long long>(r.population));
    std::printf("  per nation: min %d  median %d  max %d   |  nations with none: %d of %d\n",
                r.pop_per_nation.lo, r.pop_per_nation.med, r.pop_per_nation.hi,
                r.pop_per_nation.zeros, r.pop_per_nation.n);
    std::printf("  settled share of land: %.3f%% (%d of %d land tiles)\n",
                r.pop_centres * land_pc, r.pop_centres, r.land);

    std::printf("corporations %d  —  player %d, rival specialist %d, background (BL-365) %d\n",
                r.corps, r.corps_player, r.corps_specialist, r.corps_background);
    std::printf("  industrial_focus:");
    for (int i = 0; i < 3; ++i)
        std::printf("  %s %d", focus_name(static_cast<industrial_focus>(i)),
                    r.corp_focus[static_cast<std::size_t>(i)]);
    std::printf("\n");

    std::printf("buildings %d\n", r.buildings);
    std::printf("  by type:");
    for (int i = 0; i < building_type_count; ++i)
        if (r.building_by_type[static_cast<std::size_t>(i)] > 0)
            std::printf("  %s %d", kBuildingName[i], r.building_by_type[static_cast<std::size_t>(i)]);
    std::printf("\n  per nation: min %d  median %d  max %d   |  nations with none: %d of %d\n",
                r.buildings_per_nation.lo, r.buildings_per_nation.med, r.buildings_per_nation.hi,
                r.buildings_per_nation.zeros, r.buildings_per_nation.n);
    std::printf("  built share of land: %.3f%% (%d of %d land tiles)\n",
                r.buildings * land_pc, r.buildings, r.land);

    std::printf("roads %d tiles (%.3f%% of land)  —  Track %d  Road %d  Highway %d\n",
                r.road_tiles, r.road_tiles * land_pc,
                r.road_by_tier[1], r.road_by_tier[2], r.road_by_tier[3]);
    std::printf("  nations with NO road tile at all: %d of %d\n", r.nations_no_road, r.nations);

    std::printf("markets %d (home body %d, elsewhere %d)\n",
                r.markets_total, r.markets_home, r.markets_total - r.markets_home);
    std::printf("  nations containing NO market centre: %d of %d\n", r.nations_no_market, r.nations);

    std::printf("\nCIVILISED REACH — share of home-body LAND within N tiles of any\n"
                "building, settlement or road (land-only BFS, cardinal, columns wrap):\n");
    for (int i = 0; i < kReachRingCount; ++i)
        std::printf("    N=%-2d  %5.1f%%%s\n", kReachRings[i],
                    r.reach_q[static_cast<std::size_t>(i)] / 10.0,
                    kReachRings[i] == 0 ? "   (the built/settled/roaded tiles themselves)" : "");
}

int median_i(std::vector<int> v)
{
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

} // namespace

int main(int argc, char** argv)
{
    int seed_count = 3;
    if (argc > 1)
    {
        const int n = std::atoi(argv[1]);
        if (n > 0) seed_count = n;
    }

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");
    lua.load("scripts/works.lua");

    recipe_registry reg;
    reg.load_from_lua(lua);
    world_gen_config gen_cfg;
    gen_cfg.load_from_lua(lua);
    works_registry works;
    works.load_from_lua(lua);

    // FAIL LOUDLY rather than measure a world nobody ships. An empty registry
    // silently places zero background firms (see the header), which would make
    // this report a picture of a world the game never builds.
    if (reg.recipe_count(building_type::processing_facility) == 0)
    {
        std::printf("FATAL: no recipes loaded — run from the repo root.\n");
        return 2;
    }
    if (works.size() == 0)
        std::printf("WARNING: works.lua loaded no rows — the Era -1 run-up is works-free.\n");

    const world_params defaults;
    std::printf("=== substrate_census (NEW-10) — %d seed%s, prehistory %d years, "
                "post-background-firms ===\n",
                seed_count, seed_count == 1 ? "" : "s", defaults.prehistory_years);
    std::printf("REPORTED, NOT GATED on density — see the harness header.\n");
    std::fflush(stdout);

    std::vector<census_row> rows;
    rows.reserve(static_cast<std::size_t>(seed_count));

    const auto t_all = std::chrono::steady_clock::now();
    for (int i = 0; i < seed_count; ++i)
    {
        const auto t0 = std::chrono::steady_clock::now();

        world_params p;                       // prehistory_years stays at its default (400)
        p.seed = static_cast<uint32_t>(i);
        world w = make_hard_coded_world(p, nullptr, gen_cfg, nullptr, &works);

        // The shipped ordering, mirrored — see the header. Both
        // assign_default_recipes calls are the live ones from app::start_new_game;
        // dropping the second leaves every background processor recipe-less.
        assign_default_recipes(w, reg);
        generate_background_firms(w, reg, p.seed ^ 0x8A21F00Du);
        assign_default_recipes(w, reg);

        census_row r = measure(w, p.seed);
        const auto t1 = std::chrono::steady_clock::now();
        r.ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        print_row(r);
        std::fflush(stdout);
        rows.push_back(std::move(r));
    }
    const int64_t total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t_all).count();

    // --- The cross-seed spread ---------------------------------------------
    std::printf("\n\n=== distributions over %d world%s ===\n",
                static_cast<int>(rows.size()), rows.size() == 1 ? "" : "s");
    if (!rows.empty())
    {
        std::printf("seed   land  nations  settlements  corps(bg)  buildings  roads  markets  "
                    "built%%  settled%%  reach N=3\n");
        std::printf("----  -----  -------  -----------  ---------  ---------  -----  -------  "
                    "------  --------  ---------\n");
        for (const census_row& r : rows)
        {
            const double land_pc = r.land ? 100.0 / r.land : 0.0;
            std::printf("%4u  %5d  %7d  %11d  %4d(%3d)  %9d  %5d  %7d  %6.3f  %8.3f  %8.1f%%\n",
                        r.seed, r.land, r.nations, r.pop_centres,
                        r.corps, r.corps_background, r.buildings, r.road_tiles, r.markets_total,
                        r.buildings * land_pc, r.pop_centres * land_pc,
                        r.reach_q[3] / 10.0); // kReachRings[3] == 3
        }

        std::vector<int> land, nats, pops, blds, roads, mkts, reach3, noroad, nomkt, bg;
        for (const census_row& r : rows)
        {
            land.push_back(r.land);           nats.push_back(r.nations);
            pops.push_back(r.pop_centres);    blds.push_back(r.buildings);
            roads.push_back(r.road_tiles);    mkts.push_back(r.markets_total);
            reach3.push_back(r.reach_q[3]);   noroad.push_back(r.nations_no_road);
            nomkt.push_back(r.nations_no_market); bg.push_back(r.corps_background);
        }
        auto span = [](std::vector<int> v) {
            std::sort(v.begin(), v.end());
            return std::pair<int, int>{ v.front(), v.back() };
        };
        auto line = [&](const char* label, const std::vector<int>& v) {
            const auto s = span(v);
            std::printf("  %-26s median %6d   range %d..%d\n",
                        label, median_i(v), s.first, s.second);
        };
        std::printf("\n");
        line("land tiles",            land);
        line("nations",               nats);
        line("population centres",    pops);
        line("background firms",      bg);
        line("buildings",             blds);
        line("road tiles",            roads);
        line("markets",               mkts);
        line("nations with no road",  noroad);
        line("nations with no market", nomkt);
        {
            const auto s = span(reach3);
            std::printf("  %-26s median %5.1f%%   range %.1f%%..%.1f%%\n",
                        "LAND WITHIN 3 OF ANYTHING", median_i(reach3) / 10.0,
                        s.first / 10.0, s.second / 10.0);
        }
        std::printf("\n  (Every figure above is REPORTED. Ben's complaint — \"the world does not\n"
                    "   appear physically civilised, and there is much land that is untamed\" —\n"
                    "   is now the three numbers in the last three columns of the table, and a\n"
                    "   later sprint is judged against them rather than against an impression.)\n");
    }

    // --- Structural checks only --------------------------------------------
    //
    // Deliberately NOT thresholds on density. Each of these is a property the
    // generator either has or is broken, verified to hold on the seeds swept
    // before it was written down (see the harness header).
    std::printf("\n");
    check(static_cast<int>(rows.size()) == seed_count,
          "S1   every seed produced a world and completed its census");

    bool land_present = true, anything_built = true;
    for (const census_row& r : rows)
    {
        if (r.land <= 0)                              land_present = false;
        if (r.buildings <= 0 || r.markets_total <= 0) anything_built = false;
    }
    check(land_present,
          "S2   every world's home body carries land");
    check(anything_built,
          "S3   every world carries at least one building and one market");

    // S4 — the acceptance test for BL-463, promoted from finding F1 on 2026-08-20.
    //
    // It was written here on 2026-08-18, MEASURED at 61% of nations failing it,
    // and left non-gating: a false statement must not be made green by loosening
    // it. The fix (population_generation.cpp — a land-derived primary target plus
    // ensure_national_population_centres, a per-nation coverage pass that runs
    // once borders exist) makes it true by construction, so it gates now.
    //
    // A failure here is not a density question. It means a nation exists that the
    // coverage pass could not seat anybody in — either the pass did not run, or a
    // whole territory has no tile passing can_place_population_centre. Both are
    // worth stopping for, and the per-seed count is printed so which one is
    // visible without a rebuild.
    {
        int uncovered = 0;
        for (const census_row& r : rows)
            uncovered += r.pop_per_nation.zeros;
        if (uncovered != 0)
            for (const census_row& r : rows)
                if (r.pop_per_nation.zeros > 0)
                    std::printf("     seed %u: %d of %d nations hold no population centre\n",
                                r.seed, r.pop_per_nation.zeros, r.nations);
        check(uncovered == 0,
              "S4   every nation holds at least one population centre (BL-463)");
    }

    // --- FINDINGS: measured, mostly NOT assertions --------------------------
    //
    // F1 was a finding until 2026-08-20 and is now check S4 above; the figure is
    // still printed here because "0 of N" is the number the item is judged on and
    // a bare PASS line does not carry it.
    if (!rows.empty())
    {
        std::printf("\n--- findings (measured, not gated) ---\n");

        int worst_share = 0, worst_seed = 0, total_unsettled = 0, total_nations = 0;
        for (const census_row& r : rows)
        {
            total_unsettled += r.pop_per_nation.zeros;
            total_nations   += r.nations;
            const int share = r.nations ? (r.pop_per_nation.zeros * 100) / r.nations : 0;
            if (share > worst_share) { worst_share = share; worst_seed = static_cast<int>(r.seed); }
        }
        std::printf("F1  NATIONS WITH NO POPULATION CENTRE AT ALL: %d of %d across the sweep "
                    "(%.0f%%); worst seed %d at %d%%.\n",
                    total_unsettled, total_nations,
                    total_nations ? 100.0 * total_unsettled / total_nations : 0.0,
                    worst_seed, worst_share);

        // Until BL-463 the count was `clamp(gw * gh / 1000, 20, 40)` — a function
        // of GRID AREA alone, which is a compile-time constant, so every swept
        // seed landed on the same number and settlement density could not respond
        // to how much land a world had. It is now land-derived plus one per
        // otherwise-unsettled nation, so a constant across a multi-seed sweep
        // would mean the derivation has been short-circuited again.
        bool constant_centres = true;
        int land_lo = rows.front().land,    land_hi = rows.front().land;
        int nat_lo  = rows.front().nations, nat_hi  = rows.front().nations;
        int pc_lo   = rows.front().pop_centres, pc_hi = rows.front().pop_centres;
        for (const census_row& r : rows)
        {
            if (r.pop_centres != rows.front().pop_centres) constant_centres = false;
            land_lo = std::min(land_lo, r.land);        land_hi = std::max(land_hi, r.land);
            nat_lo  = std::min(nat_lo,  r.nations);     nat_hi  = std::max(nat_hi,  r.nations);
            pc_lo   = std::min(pc_lo,   r.pop_centres); pc_hi   = std::max(pc_hi,   r.pop_centres);
        }
        if (constant_centres)
            std::printf("F2  POPULATION CENTRES ARE SEED-INVARIANT at %d on all %d worlds, over land\n"
                        "    areas of %d..%d tiles and %d..%d nations — settlement density is not\n"
                        "    responding to the world it is placed on. BL-463 removed the grid-area\n"
                        "    formula that caused this; if it is back, something is short-circuiting\n"
                        "    the land-area + nation-count derivation in population_generation.cpp.\n",
                        rows.front().pop_centres, static_cast<int>(rows.size()),
                        land_lo, land_hi, nat_lo, nat_hi);
        else
            std::printf("F2  population centres vary across seeds (%d..%d) — the area-derived formula\n"
                        "    in population_generation.cpp is no longer the binding term.\n",
                        pc_lo, pc_hi);
    }

    std::printf("\nruntime: %lld ms total for %d seed%s (%lld ms/world)\n",
                static_cast<long long>(total_ms), static_cast<int>(rows.size()),
                rows.size() == 1 ? "" : "s",
                rows.empty() ? 0LL : static_cast<long long>(total_ms) / static_cast<long long>(rows.size()));
    std::printf("%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
