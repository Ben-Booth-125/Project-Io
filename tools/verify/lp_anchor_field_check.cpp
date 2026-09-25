// ---------------------------------------------------------------------------
// lp_anchor_field_check — is the nearest Logistic Point anchor a pure function?
// (BL-1117, settle tick one; needs Lua: build with tools/verify/build_lua_harness.{sh,bat})
// ---------------------------------------------------------------------------
// `nearest_lp_anchor` answers from ONE multi-source field per body
// (`lp_anchor_field`, logistics.cpp § build_lp_anchor_field) instead of one
// whole-body flood per anchor. This harness holds it to its contract on real,
// settled worlds, per seed:
//
//   L1  (REPORTED, not counted) THE LOAD CONTINUATION. After --copy-after K
//       settle ticks the world is saved and loaded (a flat-binary snapshot, the
//       load contract world_copy_determinism's `--copy-by snapshot` uses), and
//       both worlds settle the remaining ticks in lockstep with the BL-1031
//       D_settle digest after every tick. A load empties every derived cache
//       (clear_derived_state), so any reader whose ANSWER depends on what was
//       cached shows up here as the first tick the two part. world_copy_
//       determinism copies before the settle's first tick, when the caches are
//       cold on both sides; this copies mid-settle, when they are not.
//       `--cold-original logistics|derived` is the attribution control: the
//       unsaved side's caches are emptied at the save point too, so if the two
//       then agree, what parted them was cache state and not the save's bytes.
//   P1  (REPORTED) THE FLOODS AND THE TIME: flood fields and pairs alive after
//       settle ticks 1 and 11, then a play year (four live ticks through
//       run_app_live_window, spectating off, day ticks advancing) with its
//       per-tick milliseconds and what it left alive (BL-1126 measures the
//       floods reading one way costs).
//   F1  PURITY: on the warm world (settle and play year done), every body's
//       nearest-anchor field is read, `invalidate_logistics_caches` is called,
//       and the field is read again — nearest anchor and cost, every cell, bit
//       for bit. NOT VACUOUS: the warm read must REUSE the field the ticks
//       themselves built (its anchor set compared to the pool's keys BEFORE the
//       call); a body whose cached field is missing or over another anchor set
//       is reported and not counted, and a seed with no reused field FAILS F1.
//       A timed sample of `nearest_lp_anchor` calls must read the same field
//       (the per-call cost is the pool walk `built_over` pays).
//   F2  EXACTNESS against the per-pair rule it replaced, on every body with an
//       anchor: one flood per anchor through `intra_body_path(probe, anchor)`
//       itself (the anchor's own, destination-side field), folding min over anchors of
//       (cost, anchor tile id) per cell. GATES: cost bit for bit, and the field's
//       anchor AT the minimum cost (its own flood distance equals the minimum).
//       The contract is "an anchor at least cost, a fixed choice among exact
//       ties", so a cell where an equal-cost anchor with a higher id won is
//       within it — COUNTED and printed on the verdict line, never hidden.
//   F3  NO PER-ANCHOR FLOODS: building the field and answering the sample, from
//       cold, leaves `logistics_flood_fields` and the pair cache empty.
//
// A seed PASSES on F1-F3. L1 is gated on nothing here; its verdict line says
// MATCH or DIFFER (BL-1126's done-when: MATCH on every tick, no control).
//
// USAGE (repo root):
//   ./build_gen/verify/lp_anchor_field_check.exe [--seeds 0,28] [--copy-after 2] [--no-brute]

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world_digest.hpp"
#include "world/logistics.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

using clk = std::chrono::steady_clock;
double secs(clk::time_point a, clk::time_point b)
{
    return std::chrono::duration<double>(b - a).count();
}

int g_failures = 0;
void check(bool ok, const char* what, std::uint32_t seed)
{
    std::printf("  %s: %s (seed %u)\n", ok ? "PASS" : "FAIL", what, seed);
    if (!ok)
        ++g_failures;
}

/// The load, exactly as world_copy_determinism's `copy_world(..., snapshot)`:
/// a flat-binary round trip, then the fields a save deliberately omits.
std::unique_ptr<world> load_copy(const world& src)
{
    auto dst = std::make_unique<world>();
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    write_world_snapshot(src, ss);
    ss.seekg(0);
    if (!read_world_snapshot(*dst, ss))
    {
        std::printf("FATAL: read_world_snapshot refused a snapshot it had just written\n");
        std::exit(1);
    }
    dst->current_econ_tick = src.current_econ_tick;
    dst->current_day_tick  = src.current_day_tick;
    dst->gen_settlement    = src.gen_settlement;
    dst->gen_carve_centres = src.gen_carve_centres;
    dst->gen_carve_dropped = src.gen_carve_dropped;
    return dst;
}

std::vector<entity_id> sorted_body_ids(const world& w)
{
    std::vector<entity_id> ids;
    for (const auto& [bid, bc] : w.bodies)
        if (bc.grid_width > 0 && bc.grid_height > 0)
            ids.push_back(bid);
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool same_bits(float a, float b)
{
    return std::memcmp(&a, &b, sizeof a) == 0;
}

struct body_rows
{
    bool f1 = true, f2 = true, f3 = true;
    bool f1_counted = false; ///< the warm read reused the ticks' own field
    long long tie_cells = 0, wrong_cells = 0, cost_cells = 0;
};

/// True when @p w already caches a field for @p body built over exactly @p pool's
/// keys — i.e. body_lp_anchor_field will REUSE it rather than rebuild.
bool cached_over_pool(const world& w, entity_id body, const std::unordered_map<entity_id, float>& pool)
{
    const auto it = w.lp_anchor_fields.find(body);
    if (it == w.lp_anchor_fields.end() || it->second.anchors.size() != pool.size())
        return false;
    std::vector<entity_id> keys;
    keys.reserve(pool.size());
    for (const auto& kv : pool) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    return keys == it->second.anchors;
}

body_rows check_body(world& w, entity_id body, std::uint32_t seed, bool brute)
{
    body_rows out;
    // The tick's pool: `active_lp_anchor_pools` is what lp_pool_for_body builds
    // each tick (any positive rate: only the KEY SET reaches the field).
    const std::unordered_map<entity_id, float> pool = active_lp_anchor_pools(w, body, 1.0f);
    if (pool.empty())
        return out;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    const std::size_t cells = grid.size();

    // --- F1: warm, then cold ---
    // Compared BEFORE the call: a field built here would make F1 compare two
    // fresh builds of the same thing, which proves nothing (the S2 review).
    const bool was_built = w.lp_anchor_fields.count(body) != 0;
    const bool reused    = cached_over_pool(w, body, pool);
    const lp_anchor_field warm = body_lp_anchor_field(w, body, pool); // a copy
    const std::size_t floods_warm = w.logistics_flood_fields.size();
    const std::size_t pairs_warm  = w.astar_cost_cache.size();
    invalidate_logistics_caches(w);
    const lp_anchor_field& cold = body_lp_anchor_field(w, body, pool);
    long long nearest_diff = 0, cost_diff = 0;
    for (std::size_t i = 0; i < cells && i < warm.nearest.size() && i < cold.nearest.size(); ++i)
    {
        if (warm.nearest[i] != cold.nearest[i]) ++nearest_diff;
        if (!same_bits(warm.cost[i], cold.cost[i])) ++cost_diff;
    }
    const bool sizes = warm.nearest.size() == cold.nearest.size() && warm.nearest.size() == cells;
    // The API reads the field: a strided, timed sample of real calls (each one
    // walks the pool in built_over before it reads a cell).
    long long api_diff = 0, api_n = 0;
    const std::size_t stride = std::max<std::size_t>(1, cells / 997);
    const clk::time_point t_api = clk::now();
    for (std::size_t i = 0; i < cells; i += stride)
    {
        const entity_id t = grid[i];
        if (t == null_entity)
            continue;
        ++api_n;
        const entity_id got  = nearest_lp_anchor(w, body, t, pool);
        const entity_id want = (cold.cost[i] < 1e30f) ? cold.nearest[i] : null_entity;
        if (got != want) ++api_diff;
    }
    const double api_us = api_n > 0 ? secs(t_api, clk::now()) * 1e6 / static_cast<double>(api_n) : 0.0;
    long long reached = 0;
    for (std::size_t i = 0; i < cells; ++i)
        if (cold.cost[i] < 1e30f) ++reached;
    std::printf("  body %u: %zu anchors, %zu cells, %lld reached; warm world held %zu flood fields, "
                "%zu pairs; its cached field: %s\n", static_cast<unsigned>(body), pool.size(),
                cells, reached, floods_warm, pairs_warm,
                reused      ? "REUSED (the ticks' own, over the pool's anchor set)"
                : was_built ? "REBUILT (cached over a different anchor set) -- F1 not counted"
                            : "none cached -- built here, F1 not counted");
    std::printf("    F1 warm vs cold: %lld nearest differ, %lld costs differ; API sample %lld/%lld "
                "differ, %.1f us per call (the pool walk included)\n", nearest_diff, cost_diff,
                api_diff, api_n, api_us);
    out.f1_counted = reused;
    out.f1 = sizes && nearest_diff == 0 && cost_diff == 0 && api_diff == 0;

    // --- F3: nothing per-anchor was built answering from cold ---
    std::printf("    F3 after the cold build and %lld calls: %zu flood fields, %zu pairs\n", api_n,
                w.logistics_flood_fields.size(), w.astar_cost_cache.size());
    out.f3 = w.logistics_flood_fields.empty() && w.astar_cost_cache.empty();

    // --- F2: brute force, one flood per anchor through intra_body_path ---
    if (brute)
    {
        const lp_anchor_field field = cold; // a copy: the loop below clears the caches
        std::vector<float>     best(cells, 1e30f);
        std::vector<entity_id> best_a(cells, null_entity);
        std::vector<float>     d_of_field(cells, 1e30f); // the field's anchor's own flood distance
        std::vector<entity_id> anchors;
        for (const auto& kv : pool) anchors.push_back(kv.first);
        std::sort(anchors.begin(), anchors.end());
        entity_id probe_a = null_entity, probe_b = null_entity;
        for (const entity_id t : grid)
        {
            if (t == null_entity) continue;
            if (probe_a == null_entity) probe_a = t;
            else { probe_b = t; break; }
        }
        const clk::time_point t0 = clk::now();
        for (const entity_id a : anchors)
        {
            invalidate_logistics_caches(w); // no field at the probe: the pair floods from `a`
            const entity_id probe = (a == probe_a) ? probe_b : probe_a;
            (void)intra_body_path(w, body, probe, a);
            const auto fit = w.logistics_flood_fields.find(std::make_pair(body, a));
            if (fit == w.logistics_flood_fields.end())
                continue; // an anchor the path code refuses (off-grid): the field skips it too
            const std::vector<float>& d = fit->second.dist;
            for (std::size_t i = 0; i < cells && i < d.size(); ++i)
            {
                if (!(d[i] < 1e30f))
                    continue;
                if (d[i] < best[i] || (d[i] == best[i] && a < best_a[i]))
                {
                    best[i]   = d[i];
                    best_a[i] = a;
                }
                if (field.nearest[i] == a)
                    d_of_field[i] = d[i];
            }
        }
        invalidate_logistics_caches(w);
        for (std::size_t i = 0; i < cells; ++i)
        {
            if (!same_bits(best[i], field.cost[i]))
            {
                ++out.cost_cells;
                continue;
            }
            if (best[i] >= 1e30f || best_a[i] == field.nearest[i])
                continue;
            if (same_bits(d_of_field[i], best[i]))
                ++out.tie_cells;   // an equal-cost anchor, higher id: the named float case
            else
                ++out.wrong_cells; // the field's anchor is not at the minimum: a defect
        }
        std::printf("    F2 brute force (%zu floods, %.1f s): %lld cost cells differ, %lld wrong "
                    "anchors, %lld equal-cost higher-id ties\n", anchors.size(),
                    secs(t0, clk::now()), out.cost_cells, out.wrong_cells, out.tie_cells);
        out.f2 = out.cost_cells == 0 && out.wrong_cells == 0;
    }
    (void)seed;
    return out;
}

std::vector<std::uint32_t> parse_seeds(const std::string& csv)
{
    std::vector<std::uint32_t> seeds;
    std::size_t at = 0;
    while (at <= csv.size())
    {
        std::size_t comma = csv.find(',', at);
        if (comma == std::string::npos) comma = csv.size();
        if (comma > at)
            seeds.push_back(static_cast<std::uint32_t>(std::strtoul(csv.substr(at, comma - at).c_str(),
                                                                    nullptr, 10)));
        at = comma + 1;
    }
    return seeds;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds = {0u, 28u};
    int  copy_after    = 2;
    bool brute         = true;
    int  cold_original = 0; // 0 none, 1 logistics caches, 2 clear_derived_state
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        const bool more = i + 1 < argc;
        if (a == "--seeds" && more) seeds = parse_seeds(argv[++i]);
        else if (a == "--copy-after" && more) copy_after = std::atoi(argv[++i]);
        else if (a == "--no-brute") brute = false;
        else if (a == "--cold-original" && more)
        {
            const std::string v = argv[++i];
            cold_original = v == "logistics" ? 1 : v == "derived" ? 2 : -1;
            if (cold_original < 0)
            {
                std::printf("--cold-original logistics|derived\n");
                return 2;
            }
        }
        else
        {
            std::printf("usage: %s [--seeds 0,28] [--copy-after K] [--no-brute] "
                        "[--cold-original logistics|derived]\n", argv[0]);
            return 2;
        }
    }
    const int ticks = k_app_validation_ticks;
    copy_after = std::clamp(copy_after, 0, ticks);
    std::printf("lp_anchor_field_check (BL-1117): %zu seed(s), load after %d of %d settle ticks, "
                "brute force %s, unsaved side %s at the save\n", seeds.size(), copy_after, ticks,
                brute ? "on" : "off",
                cold_original == 1   ? "cleared of the logistics caches"
                : cold_original == 2 ? "cleared of every derived cache"
                                     : "kept warm");

    lua_state lua; // app::m_lua: one long-lived state the scripts are (re)loaded into
    int l1_diverged = 0;
    for (const std::uint32_t seed : seeds)
    {
        const clk::time_point t_start = clk::now();
        world_params p{};
        p.seed = seed;
        app_start_world a;
        build_app_start_world(lua, p, a);
        std::printf("\nseed %u  (build + landscape %.1f s)\n", seed, secs(t_start, clk::now()));

        // --- L1: settle K ticks, load, continue both in lockstep ---
        std::vector<std::size_t> floods_after(static_cast<std::size_t>(ticks), 0);
        std::vector<std::size_t> pairs_after(static_cast<std::size_t>(ticks), 0);
        for (int t = 0; t < copy_after; ++t)
        {
            run_app_validation_tick(a.w, a.reg, t);
            floods_after[static_cast<std::size_t>(t)] = a.w.logistics_flood_fields.size();
            pairs_after[static_cast<std::size_t>(t)]  = a.w.astar_cost_cache.size();
        }
        std::printf("  L1 at the save: %zu flood fields, %zu pairs, %zu nearest-anchor fields\n",
                    a.w.logistics_flood_fields.size(), a.w.astar_cost_cache.size(),
                    a.w.lp_anchor_fields.size());
        std::unique_ptr<world> loaded = load_copy(a.w);
        // --cold-original: the ATTRIBUTION control. Empty the unsaved world's
        // caches at the save point too, so the two sides differ only in what the
        // save carries. `logistics` clears the four logistics caches; `derived`
        // is the load's whole clear_derived_state (day tick restored, as the
        // load copy restores it).
        if (cold_original == 1)
            invalidate_logistics_caches(a.w);
        else if (cold_original == 2)
        {
            const int day = a.w.current_day_tick;
            clear_derived_state(a.w);
            a.w.current_day_tick = day;
        }
        int first_diff = -1;
        for (int t = copy_after; t < ticks; ++t)
        {
            run_app_validation_tick(a.w, a.reg, t);
            floods_after[static_cast<std::size_t>(t)] = a.w.logistics_flood_fields.size();
            pairs_after[static_cast<std::size_t>(t)]  = a.w.astar_cost_cache.size();
            run_app_validation_tick(*loaded, a.reg, t);
            const std::uint64_t d_o = world_state_digest(a.w);
            const std::uint64_t d_l = world_state_digest(*loaded);
            const bool same = d_o == d_l;
            if (!same && first_diff < 0) first_diff = t;
            std::printf("  L1 tick %2d  unsaved %016" PRIX64 "  loaded %016" PRIX64 "  %s\n", t, d_o,
                        d_l, same ? "match" : "DIFFER");
        }
        if (first_diff >= 0)
        {
            ++l1_diverged;
            std::printf("  L1 DIFFER: the loaded world parts from the unsaved one at tick %d "
                        "(saved after tick %d)\n", first_diff, copy_after - 1);
        }
        else
            std::printf("  L1 MATCH: the loaded world ticks as the unsaved one on every tick %d-%d\n",
                        copy_after, ticks - 1);
        loaded.reset();

        // --- P1: floods over the settle, then a play year ---
        if (ticks > 11)
            std::printf("  P1 settle: flood fields alive after tick 1 %zu, after tick 11 %zu; pairs "
                        "%zu / %zu\n", floods_after[1], floods_after[11], pairs_after[1],
                        pairs_after[11]);
        {
            app_tick_timing year;
            const std::size_t f0 = a.w.logistics_flood_fields.size();
            const clk::time_point y0 = clk::now();
            run_app_live_window(a.w, a.reg, /*first_econ_step=*/ticks, /*ticks=*/4, &year);
            const double year_s = secs(y0, clk::now());
            std::printf("  P1 play year (4 live ticks): %.1f s; tick ms", year_s);
            for (const double ms : year.tick_ms) std::printf(" %.0f", ms);
            std::printf("; flood fields alive %zu -> %zu, pairs %zu, convoys %zu\n", f0,
                        a.w.logistics_flood_fields.size(), a.w.astar_cost_cache.size(),
                        a.w.convoys.size());
        }

        // --- F1-F3 per body, on the settled (warm) world ---
        body_rows all;
        int f1_bodies = 0;
        for (const entity_id body : sorted_body_ids(a.w))
        {
            const body_rows r = check_body(a.w, body, seed, brute);
            if (r.f1_counted)
            {
                ++f1_bodies;
                all.f1 = all.f1 && r.f1;
            }
            all.f2 = all.f2 && r.f2;
            all.f3 = all.f3 && r.f3;
            all.tie_cells += r.tie_cells;
        }
        std::printf("  F1 counted on %d body(ies) whose warm field the ticks built\n", f1_bodies);
        check(all.f1 && f1_bodies > 0,
              "F1 the ticks' own nearest-anchor field is identical after invalidate_logistics_caches",
              seed);
        check(all.f3, "F3 answering from cold builds no per-anchor flood field and no pair", seed);
        if (brute)
        {
            char f2_label[256];
            std::snprintf(f2_label, sizeof f2_label,
                          "F2 the field equals min over per-anchor floods (cost bit for bit; anchor at "
                          "the minimum; %lld equal-cost higher-id tie cell(s), within the contract)",
                          all.tie_cells);
            check(all.f2, f2_label, seed);
        }
        std::printf("  seed %u done in %.1f s\n", seed, secs(t_start, clk::now()));
        std::fflush(stdout);
    }

    std::printf("\n%s (%d failures); L1: %d of %zu seed(s) diverged after a load\n",
                g_failures == 0 ? "ALL PASS" : "FAILED", g_failures, l1_diverged, seeds.size());
    return g_failures == 0 ? 0 : 1;
}
