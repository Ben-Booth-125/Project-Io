// ---------------------------------------------------------------------------
// gen_step_costs -- what each captioned generation step costs, and whether the
// loading bar ever holds still (BL-1072, a wizard wait says what it is doing)
// ---------------------------------------------------------------------------
// STARTUP.md § A wait never looks stopped: the outer bar's steps are weighted
// by what each pass COSTS, measured, and every long pass reports progress
// within itself. This instrument is the measurement and the check.
//
//   1. Builds the world the app builds (world_gen.lua's PARSED config, the
//      works table, the shipped params) once per seed, with a fixture -- so
//      generation prints its own `[gen steps]` line: wall milliseconds per
//      step label, which is what `generation_step_cost_ms` is read from.
//   2. Watches the same `generation_progress` sink the loading screen reads,
//      from a second thread, every 50 ms, exactly as a renderer would: the
//      outer fraction (`generation_progress::fraction`), the caption, and the
//      longest stretch the fraction did not move. That stretch is the item's
//      DONE WHEN ("the bar never holds still for more than ~1 s"), measured.
//
// A REPORT, NOT A GATE. Wall clock varies with the machine and the load; the
// stall figure is read by a human against the ~1 s the item names, and the
// process exits 0 unless the build itself fails. The sink is a write-only tap,
// so a watched build is the same world as an unwatched one (world_determinism
// holds the digests).
//
// Usage:  gen_step_costs.exe [--finish | --roads F1,F2,... [--roads-map DIR]] [seed ...]
//         (seeds hex or decimal; default 0 and 28; floors in heads; DIR receives
//         roads_seed<S>_built.ppm and roads_seed<S>_floor<F>.ppm, one pixel a tile)
// Build:  cmd //c tools\verify\build_lua_harness.bat gen_step_costs
//         Run from the repo root: it loads scripts/world_gen.lua and works.lua.
//
// --finish (BL-1085): after each build, run `finish_campaign_world` on the same
// sink -- the landscape search and the twelve-tick settle round 6 now runs
// after its build -- with the watcher still watching, and print what the two
// steps cost. That is the measurement `generation_step_cost_ms` 16 and 17 are
// read from, and the still-stretch then covers the whole wait Begin shows.
//
// --roads F1,F2,... (BL-1119 D1, the spur floor measured): after each build,
// REPLAY `generate_roads` on the built world once per floor candidate (heads;
// see road_generation.hpp § kVillageSpurFloorHeads) and print, per candidate,
// the road tiles the pass laid, its spurs, the villages it left unspurred and
// its wall time. Before the ladder it prints the village size distribution the
// floor reads, and a REPLAY CHECK: the build's own floor replayed, with the
// history roads re-stamped from the fixture and the market roads re-laid
// (BL-1138, `lay_market_roads`), must reproduce the built world's
// road field tile for tile — otherwise the replay is not measuring the pass the
// build ran, and the row says so. Each replay starts from a road-free body and
// cold A* caches, exactly as the in-generation pass does. Each row is followed by a
// BAR line (BL-1119 round 2): the loading bar's unit plan per phase against the wall
// time of the border walk, read off the pass's own report_sub from a sampler thread,
// and the kBorderUnitsPerNation that would make the bar linear. Not combinable with
// --finish (the finish stamps roads and grows centres; the replays leave the body
// road-free), and it runs after the watcher stops, so it never reads as a still bar.

#include "harness_params.hpp"
#include "world/era_minus_one.hpp"
#include "world/finish_campaign_world.hpp"
#include "world/hard_coded_world.hpp"
#include "world/logistics.hpp" // invalidate_logistics_caches
#include "world/recipe_registry.hpp"
#include "world/road_generation.hpp"
#include "world/works_roster.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

using clk = std::chrono::steady_clock;

double secs_between(clk::time_point a, clk::time_point b)
{
    return std::chrono::duration<double>(b - a).count();
}

/// Every road tier on @p body, by tile id (for the replay check).
std::map<entity_id, std::uint8_t> road_field(const world& w, entity_id body)
{
    std::map<entity_id, std::uint8_t> f;
    for (const auto& [tid, tc] : w.tiles)
        if (tc.body == body && tc.road_level != 0)
            f[tid] = tc.road_level;
    return f;
}

/// --roads-map: the body's road field as a binary PPM, one pixel per tile (raster
/// order, row 0 at the top). Water dark blue, bare land tan; a road on a tile that
/// hosts a centre (its local STREET) orange; a road on any other tile by tier —
/// Track brown, Road red, Highway black. Streets and network apart, because the two
/// answer different questions: how dense the settlement is, and how the roads run.
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

/// BL-1119 round 3 — JOINED TO A TOWN, rebuilt from the trace alone (the same reading
/// road_generation_harness R5f asserts). Connectivity is the laid routes' own paths:
/// consecutive tiles of every tree, loop and spur route are joined (water tiles
/// included, so a strait crossing bridges as the rule says it does); border routes are
/// left out, so "joined" means joined inside the nation's own network. A centre is
/// joined when its tile's component holds a town (scale >= 2) of the same nation.
struct joined_reading
{
    int villages_claimed = 0;     ///< villages the pass counts on their network
    int villages_unjoined = 0;    ///< of them, whose road reaches no same-nation town
    int border_endpoints = 0;     ///< border-link endpoints (two per link)
    int border_unjoined = 0;      ///< of them, not joined to a same-nation town
};

joined_reading read_joined(const world& w, entity_id body, const road_generation_trace& tr)
{
    std::map<entity_id, entity_id> parent;
    auto root = [&](entity_id x) -> entity_id {
        if (parent.find(x) == parent.end()) { parent.emplace(x, x); return x; }
        entity_id r = x;
        while (parent[r] != r) r = parent[r];
        while (parent[x] != r) { const entity_id nx = parent[x]; parent[x] = r; x = nx; }
        return r;
    };
    auto join = [&](entity_id a, entity_id b) {
        const entity_id ra = root(a), rb = root(b);
        if (ra != rb) parent[std::max(ra, rb)] = std::min(ra, rb);
    };
    for (const auto& r : tr.routes)
    {
        if (r.k == road_generation_trace::kind::border) continue;
        for (std::size_t i = 1; i < r.path.size(); ++i) join(r.path[i - 1], r.path[i]);
        if (!r.path.empty()) { join(r.from, r.path.front()); join(r.to, r.path.front()); }
    }
    auto nation_at = [&](entity_id t) {
        const auto it = w.tile_to_nation.find(t);
        return it != w.tile_to_nation.end() ? it->second : null_entity;
    };
    std::map<entity_id, int> scale_at; // centre tile -> scale, on the body
    std::set<std::pair<entity_id, entity_id>> town_roots; // (component root, nation)
    for (const auto& [cid, tile] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tile);
        const auto pit = w.population_centres.find(cid);
        if (tit == w.tiles.end() || tit->second.body != body || pit == w.population_centres.end())
            continue;
        scale_at[tile] = pit->second.scale;
        if (pit->second.scale >= 2) town_roots.insert({ root(tile), nation_at(tile) });
    }
    auto joined = [&](entity_id tile) {
        const auto s = scale_at.find(tile);
        if (s != scale_at.end() && s->second >= 2) return true; // a town is the network
        return town_roots.count({ root(tile), nation_at(tile) }) != 0;
    };
    joined_reading jr;
    for (const entity_id c : tr.on_network)
    {
        const auto ct = w.population_centre_tile.find(c);
        if (ct == w.population_centre_tile.end()) continue;
        const auto s = scale_at.find(ct->second);
        if (s == scale_at.end() || s->second >= 2) continue;
        ++jr.villages_claimed;
        if (!joined(ct->second)) ++jr.villages_unjoined;
    }
    for (const auto& r : tr.routes)
    {
        if (r.k != road_generation_trace::kind::border) continue;
        for (const entity_id e : { r.from, r.to })
        {
            ++jr.border_endpoints;
            if (!joined(e)) ++jr.border_unjoined;
        }
    }
    return jr;
}

/// A road-free body and cold traversal caches: the state `generate_roads` meets in
/// generation (only the two road passes write road_level before the finish).
void clear_roads(world& w, entity_id body)
{
    for (auto& [tid, tc] : w.tiles)
        if (tc.body == body)
            tc.road_level = 0;
    invalidate_logistics_caches(w); // every traversal cache, through its one owner
}

/// One `generate_roads` call with the loading bar's own sub-progress watched from a
/// second thread (BL-1119 round 2, the bar's weights): the wall time of the whole pass
/// and of its BORDER WALK, which starts where the reported units cross the pass's own
/// border base (`units_backbone + units_spurs`). Read from the sink the loading screen
/// reads, so it measures exactly what the bar shows; no clock enters world code.
double timed_generate_roads(world& w, entity_id body, long long floor, road_generation_stats& st,
                            double& border_s, road_generation_trace* trace)
{
    auto bar = std::make_unique<generation_progress>();
    std::atomic<bool> stop{false};
    std::vector<std::pair<double, int>> samples; // (seconds, sub_progress)
    const clk::time_point t0 = clk::now();
    std::thread sampler([&] {
        while (!stop.load(std::memory_order_acquire))
        {
            samples.emplace_back(secs_between(t0, clk::now()),
                                 bar->sub_progress.load(std::memory_order_relaxed));
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    generate_roads(w, body, bar.get(), floor, &st, trace);
    const double total = secs_between(t0, clk::now());
    stop.store(true, std::memory_order_release);
    sampler.join();
    // report_sub scales into int range only past a million units; the plans measured
    // here are far below that, so the reported count IS the unit count.
    const long long border_base = st.units_backbone + st.units_spurs;
    double border_from = total;
    for (const auto& [t, sub] : samples)
        if (sub >= border_base) { border_from = t; break; }
    border_s = total - border_from;
    return total;
}

/// BL-1119 D1: the village size distribution the spur floor reads, then one row per
/// floor candidate (heads). A floor above every village's size is the no-spur
/// asymptote: what the pass costs with the spur walk gone.
void measure_road_floors(world& w, const generation_report& rep, const era_minus_one_fixture& fx,
                         uint32_t seed, const std::vector<long long>& floors,
                         const std::string& map_dir)
{
    const entity_id body = w.home_body;

    // The size distribution, over this body's villages.
    std::vector<long long> sizes;
    std::set<entity_id> centre_tiles;
    int no_slot = 0, anchors = 0, land = 0;
    for (const auto& [tid, tc] : w.tiles)
        if (tc.body == body && !is_water(tc.substrate)) ++land;
    for (const auto& [cid, tile] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tile);
        if (tit != w.tiles.end() && tit->second.body == body) centre_tiles.insert(tile);
        const auto pit = w.population_centres.find(cid);
        if (tit == w.tiles.end() || tit->second.body != body || pit == w.population_centres.end()
            || pit->second.scale >= 2)
            continue;
        sizes.push_back(village_spur_size(w, cid));
        if (w.gen_carve_centres.find(cid) == w.gen_carve_centres.end()) ++no_slot;
        if (pit->second.province_anchor) ++anchors;
    }
    std::sort(sizes.begin(), sizes.end());
    std::printf("  ROADS seed %u body: %d land tiles, %zu centre tiles (every one a street)\n",
                seed, land, centre_tiles.size());
    auto q = [&](double p) -> long long {
        if (sizes.empty()) return 0;
        const std::size_t i = std::min(sizes.size() - 1,
                                       static_cast<std::size_t>(p * static_cast<double>(sizes.size())));
        return sizes[i];
    };
    std::printf("  ROADS seed %u villages %zu (no carve slot %d, of them anchors %d) size heads: "
                "min %lld p10 %lld p25 %lld p50 %lld p75 %lld p90 %lld max %lld\n",
                seed, sizes.size(), no_slot, anchors, sizes.empty() ? 0LL : sizes.front(),
                q(0.10), q(0.25), q(0.50), q(0.75), q(0.90), sizes.empty() ? 0LL : sizes.back());
    for (const long long f : floors)
    {
        const auto at_or_above = sizes.end() - std::lower_bound(sizes.begin(), sizes.end(), f);
        std::printf("  ROADS seed %u floor %lld: %lld of %zu villages at or above\n", seed, f,
                    static_cast<long long>(at_or_above), sizes.size());
    }

    // REPLAY CHECK: the build's own floor, plus the history roads, reproduces the field.
    const std::map<entity_id, std::uint8_t> built = road_field(w, body);
    if (!map_dir.empty()) // the world as built: this pass at the shipped floor + the history roads
        write_road_map(w, body, centre_tiles,
                       map_dir + "/roads_seed" + std::to_string(seed) + "_built.ppm");
    {
        clear_roads(w, body);
        generate_roads(w, body);
        const generation_report::body_entry* be = nullptr;
        for (const auto& b : rep.bodies)
            if (b.id == body) be = &b;
        if (be != nullptr && !fx.setup_corridors.empty())
        {
            std::vector<history_road_node> nodes;
            for (const region& p : be->settlement.regions)
                nodes.push_back(history_road_node{ p.col, p.row, p.work_reach_mod });
            // BL-1119 round 4: the old-road stamp's own cost — the in-generation step 15 —
            // read as floods (load-independent) and wall time (indicative).
            history_road_stats hs{};
            const clk::time_point h0 = clk::now();
            stamp_history_roads(w, body, nodes, fx.setup_corridors, nullptr, &hs);
            std::printf("  ROADS seed %u HISTORY: %d corridors, %d laid, priced toward %d distinct"
                        " tiles | floods %lld | %.2f s\n",
                        seed, hs.corridors, hs.laid, hs.destinations, hs.floods,
                        secs_between(h0, clk::now()));
        }
        {
            // BL-1138: the market pulls and the trunk, which generation lays after the
            // folds (the built world's markets are the folded set, so this replays it).
            market_road_stats ms{};
            const clk::time_point m0 = clk::now();
            lay_market_roads(w, body, &ms);
            std::printf("  ROADS seed %u MARKETS: %d markets, joins %d (%d failed), pulls %d of %d,"
                        " trunk %d of %d pairs (%d refused), %d tiles raised | walks %lld | %.2f s\n",
                        seed, ms.markets, ms.joins_laid, ms.joins_failed, ms.pull_laid,
                        ms.pull_candidates, ms.trunk_laid, ms.trunk_pairs, ms.trunk_refused,
                        ms.tiles_raised, ms.walks, secs_between(m0, clk::now()));
        }
        const std::map<entity_id, std::uint8_t> replay = road_field(w, body);
        std::size_t diff = 0;
        for (const auto& [t, l] : built)
        {
            const auto it = replay.find(t);
            if (it == replay.end() || it->second != l) ++diff;
        }
        for (const auto& [t, l] : replay)
            if (built.find(t) == built.end()) ++diff;
        std::printf("  ROADS seed %u REPLAY CHECK: built %zu road tiles, replay %zu, %zu differ -> %s\n",
                    seed, built.size(), replay.size(), diff, diff == 0 ? "FAITHFUL" : "DIVERGES");
    }

    for (const long long f : floors)
    {
        clear_roads(w, body);
        road_generation_stats st{};
        double border_s = 0.0;
        // The trace costs only cache hits (the pass re-reads paths it just laid), so it
        // moves neither the network nor, measurably, the time.
        road_generation_trace tr;
        const double s = timed_generate_roads(w, body, f, st, border_s, &tr);
        const joined_reading jr = read_joined(w, body, tr);
        int tiers[4] = { 0, 0, 0, 0 };
        int streets = 0; // road tiles hosting a centre: the local streets
        for (const auto& [tid, tc] : w.tiles)
            if (tc.body == body && tc.road_level >= 1 && tc.road_level <= 3)
            {
                ++tiers[0];
                ++tiers[tc.road_level];
                if (centre_tiles.count(tid) != 0) ++streets;
            }
        std::printf("  ROADS seed %u TREE floor %lld: %.2f s floods %lld (%lld before the border walk) | road tiles %d (T1 %d T2 %d T3 %d; "
                    "streets %d network %d) | towns %d mst %d cand %d admitted %d kept %d | "
                    "villages %d below %d spurs %d failed %d unspurred %d | border %d (%d on a "
                    "below-floor street)\n",
                    seed, f, s, st.flood_fields, st.flood_fields_before_border,
                    tiers[0], tiers[1], tiers[2], tiers[3], streets, tiers[0] - streets,
                    st.towns, st.mst_links, st.loop_candidates, st.loops_admitted, st.loops_kept,
                    st.villages, st.villages_below_floor, st.spurs_laid, st.spurs_failed,
                    st.villages_below_floor + st.spurs_failed, st.border_links,
                    st.border_links_street_only);
        // THE BAR (BL-1072 weights): units per phase against its wall time. A linear inner
        // bar wants border units / border seconds == other units / other seconds, so the
        // fitting kBorderUnitsPerNation is border_s * (other units / other s) / nations.
        const double    other_s     = s - border_s;
        const long long other_units = st.units_backbone + st.units_spurs;
        const int       nations     = st.nation_groups;
        const double    fit_k = (other_s > 0.0 && nations > 0)
            ? border_s * (static_cast<double>(other_units) / other_s) / nations : 0.0;
        std::printf("  ROADS seed %u BAR floor %lld: units backbone %lld spurs %lld border %lld (%d nation"
                    " groups) | border pairs %d, %d with no network endpoint | border walk %.2f s of"
                    " %.2f s (%.0f%%), floods %lld of %lld | fitting kBorderUnitsPerNation %.0f\n",
                    seed, f, st.units_backbone, st.units_spurs, st.units_border, nations,
                    st.border_pairs, st.border_pairs_no_endpoint, border_s, s,
                    s > 0.0 ? 100.0 * border_s / s : 0.0,
                    st.flood_fields - st.flood_fields_before_border, st.flood_fields, fit_k);
        // BL-1119 round 4: floods built per call site (a path is answered from its
        // destination's field, so a site's cost is how many destinations it asks).
        std::printf("  ROADS seed %u FLOODS floor %lld: town pairs %lld | backbone lay %lld | spurs %lld"
                    " | border %lld | total %lld\n",
                    seed, f, st.floods_town_pairs, st.floods_backbone_lay, st.floods_spurs,
                    st.floods_border, st.flood_fields);
        // BL-1119 round 3: the two cold-review holes, read per row.
        std::printf("  ROADS seed %u LINKS floor %lld: sea-route candidates %d | refused by the stamp:"
                    " tree %d, loop %d | villages on the network %d, of them NOT joined to a town %d |"
                    " border endpoints %d, not joined %d | spurs failed %d\n",
                    seed, f, st.candidates_unlayable, st.tree_links_refused, st.loops_refused,
                    jr.villages_claimed, jr.villages_unjoined, jr.border_endpoints,
                    jr.border_unjoined, st.spurs_failed);
        std::fflush(stdout);
        if (!map_dir.empty()) // this pass alone, no history roads
            write_road_map(w, body, centre_tiles,
                           map_dir + "/roads_seed" + std::to_string(seed) + "_floor"
                               + std::to_string(f) + ".ppm");
    }
    clear_roads(w, body); // the world is spent; nothing reads it after this
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    bool finish = false;
    std::vector<long long> road_floors; // --roads (BL-1119 D1)
    std::string road_map_dir;           // --roads-map DIR: a PPM of the road field per row
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--finish") { finish = true; continue; }
        if (std::string(argv[i]) == "--roads-map" && i + 1 < argc) { road_map_dir = argv[++i]; continue; }
        if (std::string(argv[i]) == "--roads" && i + 1 < argc)
        {
            const std::string list = argv[++i];
            std::size_t at = 0;
            while (at <= list.size())
            {
                const std::size_t comma = std::min(list.find(',', at), list.size());
                if (comma > at)
                    road_floors.push_back(std::strtoll(list.substr(at, comma - at).c_str(), nullptr, 10));
                at = comma + 1;
            }
            continue;
        }
        seeds.push_back(static_cast<uint32_t>(std::strtoul(argv[i], nullptr, 0)));
    }
    if (seeds.empty()) seeds = {0u, 28u};
    if (finish && !road_floors.empty())
    {
        // The finish stamps roads and grows centres, so a replay after it measures a
        // different world, and the replays leave the body road-free for the finish.
        std::fprintf(stderr, "gen_step_costs: --roads and --finish are separate runs\n");
        return 2;
    }

    lua_state lua;
    lua.load("scripts/world_gen.lua");
    world_gen_config cfg{};
    cfg.load_from_lua(lua);
    works_registry works;
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);
    // --finish: the registry the settle runs on, loaded once here as the app
    // loads it on the main thread; each seed's finish bands its own copy.
    recipe_registry reg;
    if (finish)
    {
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        reg.load_from_lua(lua);
    }

    for (const uint32_t seed : seeds)
    {
        world_params params{};
        params.seed = seed;

        // Heap, as the app holds its wizard sinks: the carve arrays make the
        // struct ~66 KB.
        auto prog = std::make_unique<generation_progress>();
        prog->begin_wait();
        prog->stage_count.store(generation_stage_count(cfg), std::memory_order_relaxed);
        // As round 6 and the cold build publish it: the finish's two steps are
        // in the plan before the worker starts (BL-1085).
        if (finish)
            prog->weight_after.store(finish_campaign_weight_ms(params), std::memory_order_relaxed);

        std::atomic<bool> done{false};
        double longest_still = 0.0, still_since_s = 0.0;
        int    longest_label = -1;
        float  longest_at    = 0.0f;
        std::vector<std::string> caption_trail;

        std::thread watcher([&] {
            const clk::time_point t0 = clk::now();
            float  last_f = -1.0f;
            int    last_label = -1;
            clk::time_point still_from = t0;
            while (!done.load(std::memory_order_acquire))
            {
                const float f  = prog->fraction();
                const int   li = prog->label.load(std::memory_order_relaxed);
                const clk::time_point now = clk::now();
                if (li != last_label && li >= 0 && li < generation_stage_label_count)
                {
                    char line[160];
                    std::snprintf(line, sizeof line, "%6.1f s  %5.1f%%  %s",
                                  secs_between(t0, now), f * 100.0f,
                                  generation_stage_labels[li]);
                    caption_trail.emplace_back(line);
                    last_label = li;
                }
                // "Moved" = a visible step on a 420 px bar: a thousandth.
                if (f > last_f + 0.001f)
                {
                    last_f     = f;
                    still_from = now;
                }
                else
                {
                    const double still = secs_between(still_from, now);
                    if (still > longest_still)
                    {
                        longest_still = still;
                        longest_label = li;
                        longest_at    = f;
                        still_since_s = secs_between(t0, still_from);
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });

        era_minus_one_fixture fx;
        generation_report rep;
        const clk::time_point t0 = clk::now();
        world w = make_hard_coded_world(params, &rep, cfg, prog.get(), &works, &fx);
        const double total_s = secs_between(t0, clk::now());
        // --finish: the same call round 6's worker makes after its build, on
        // the same sink, watched by the same watcher.
        finish_campaign_result fin;
        double finish_s = 0.0;
        if (finish)
        {
            recipe_registry seed_reg = reg; // the worker's copy, banded inside
            const clk::time_point t1 = clk::now();
            fin = finish_campaign_world(w, rep, seed_reg, params, cfg, prog.get());
            finish_s = secs_between(t1, clk::now());
        }
        done.store(true, std::memory_order_release);
        watcher.join();

        std::printf("seed %u: built in %.2f s, final fraction %.3f, stage %d/%d\n", seed,
                    total_s, prog->fraction(), prog->stage.load(), prog->stage_count.load());
        if (finish)
            std::printf("  FINISH: %.2f s -- search %lld ms (label 16), settle %lld ms (label 17); "
                        "%d evaluations\n",
                        finish_s, static_cast<long long>(fin.ms_search),
                        static_cast<long long>(fin.ms_settle), fin.search.evaluations);
        std::printf("  weight total %lld (plan)\n",
                    static_cast<long long>(prog->weight_total.load()));
        for (const std::string& s : caption_trail) std::printf("  %s\n", s.c_str());
        std::printf("  LONGEST STILL: %.2f s from %.1f s at %.1f%% under \"%s\"\n",
                    longest_still, still_since_s, longest_at * 100.0f,
                    longest_label >= 0 && longest_label < generation_stage_label_count
                        ? generation_stage_labels[longest_label] : "?");
        std::printf("  nations %zu, corps %zu\n", w.nations.size(), w.corporations.size());
        std::fflush(stdout);
        // BL-1119 D1: after the watcher has stopped, so the replays cannot read as a
        // still bar; on the built world, before anything else touches it.
        if (!road_floors.empty())
            measure_road_floors(w, rep, fx, seed, road_floors, road_map_dir);
    }
    return 0;
}
