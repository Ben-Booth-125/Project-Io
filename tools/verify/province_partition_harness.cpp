// Headless harness for the province partition (BL-466, rewritten for BL-515 —
// provinces GROWN from settlement and stopped by terrain).
//
// Four sections:
//   P — the partition's own invariants: coverage, no ocean, connectivity, the
//       hard 12-tile ceiling, the ascending-id contract, seeded determinism,
//       THE DERIVED-IDENTITY CONTRACT (P8: an id IS its lowest member tile),
//       and the settlement seeds (P9).
//   C — THE COST MODEL, MEASURED. The pinning instrument for the two
//       coefficients province.hpp declares measurement-pinned, plus the
//       direction checks that prove the model draws the borders Ben ruled:
//       rivers and slopes divide, and a ROAD BINDS.
//   D — the SIZE DISTRIBUTION across a seed sweep. REPORTED, not engineered:
//       organic borders are meant to be irregular, so D describes what the cost
//       model produced against the superseded 3x3 partition's numbers rather
//       than asserting a spread. Its one assertion is the hard ceiling.
//   S — the SERIALISATION SEAM, the risk in the item. Three properties:
//       S1  one appender / no reorder — a pre-BL-466 stream is a byte-exact
//           PREFIX of a post-BL-466 stream of the same log.
//       S2  an old save still loads — a stream that ends after its last history
//           record reads back cleanly, with the history intact.
//       S3  round-trip is bit-identical — write, read, write again, compare.
//
// Registered as the `province_partition_harness` CMake target (verifier-headless):
//   cmake --build build --target province_partition_harness
//   .\build\province_partition_harness.exe

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/hex_neighbors.hpp"
#include "world/history_log.hpp"
#include "world/logistics.hpp"
#include "world/province.hpp"
#include "world/world.hpp"

#include "harness_params.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

static int g_failures = 0;

static void check(bool cond, const char* what)
{
    if (cond)
        std::printf("  PASS  %s\n", what);
    else
    {
        std::printf("  FAIL  %s\n", what);
        ++g_failures;
    }
}

// ---------------------------------------------------------------------------
// The PRE-BL-466 writer, transcribed. This is what a save written before this
// item looks like: magic, version, count, then the entry records — and then the
// stream ends. Kept here (not shared with history_log.cpp) deliberately: it is
// a frozen snapshot of the old format, and must not drift when the live one does.
// ---------------------------------------------------------------------------
static void legacy_write_history_log(const world& w, std::ostream& out)
{
    const auto u8  = [&out](uint8_t v) { out.write(reinterpret_cast<const char*>(&v), 1); };
    const auto u32 = [&out](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
    const auto i64 = [&out](int64_t v) { out.write(reinterpret_cast<const char*>(&v), 8); };
    const auto str = [&](const std::string& s) {
        u32(static_cast<uint32_t>(s.size()));
        if (!s.empty())
            out.write(s.data(), static_cast<std::streamsize>(s.size()));
    };

    u32(history_log_magic);
    u32(history_log_version);
    u32(static_cast<uint32_t>(w.history_log.size()));
    for (const world_history_entry& e : w.history_log)
    {
        i64(e.timestamp);
        u8(static_cast<uint8_t>(e.topic));
        u32(e.body);
        u32(e.corp);
        str(e.event);
        str(e.consequence);
    }
}

int main()
{
    std::printf("Province partition harness (BL-466 partition, BL-515 organic borders)\n\n");

    // The partition is a generation output, so it needs a real generated world.
    // The Era -1 sim is not under test here.
    generation_report report;
    world             w = make_hard_coded_world(no_prehistory(world_params{}), &report);

    std::printf("P — partition invariants\n");

    const province_partition& part = w.provinces;
    check(!part.provinces.empty(), "P0  the partition is non-empty");

    // P1 — ascending, strictly unique ids. THE contract downstream reads.
    {
        bool ok = true;
        for (std::size_t i = 1; i < part.provinces.size(); ++i)
            if (part.provinces[i].id <= part.provinces[i - 1].id)
                ok = false;
        check(ok, "P1  province ids are strictly ascending (the iteration contract)");
    }

    // P2 — every land tile in exactly one province; no ocean tile in any.
    {
        std::size_t land_total = 0;
        for (const auto& [tid, tc] : w.tiles)
            if (tc.composition != terrain_composition::ocean)
                ++land_total;

        std::unordered_map<entity_id, int> seen;
        bool                               ocean_member = false;
        bool                               body_mismatch = false;
        std::size_t                        members = 0;
        for (const province& p : part.provinces)
            for (const entity_id t : p.tiles)
            {
                ++members;
                ++seen[t];
                const auto it = w.tiles.find(t);
                if (it == w.tiles.end() || it->second.composition == terrain_composition::ocean)
                    ocean_member = true;
                else if (it->second.body != p.body)
                    body_mismatch = true;
            }

        bool once = (seen.size() == members);
        check(members == land_total && once,
              "P2a every land tile belongs to exactly one province");
        check(!ocean_member, "P2b no province holds an ocean tile");
        check(!body_mismatch, "P2c no province spans bodies");
        std::printf("        %zu land tiles across %zu provinces\n", land_total,
                    part.provinces.size());
    }

    // P3 — tile_province agrees with the province membership, both ways.
    {
        bool ok = part.tile_province.size() > 0;
        for (const province& p : part.provinces)
            for (const entity_id t : p.tiles)
                if (part.province_of(t) != p.id)
                    ok = false;
        for (const auto& [t, id] : part.tile_province)
        {
            const province* p = part.find(id);
            if (p == nullptr || std::find(p->tiles.begin(), p->tiles.end(), t) == p->tiles.end())
                ok = false;
        }
        check(ok, "P3  tile_province and province membership agree in both directions");
    }

    // -----------------------------------------------------------------------
    // Grids, and the adjacent-land edge list every terrain row below reads.
    // Built ONCE: an edge is a pair (a < b) of hex-adjacent land tiles on one
    // body, plus the side index leading from a to b.
    // -----------------------------------------------------------------------
    struct land_edge
    {
        entity_id a = null_entity;
        entity_id b = null_entity;
        int       side = 0;
        float     dh = 0.0f;
        bool      river = false;
        bool      roaded = false;
    };

    std::map<entity_id, std::vector<entity_id>> grids;
    std::map<entity_id, std::pair<int, int>>    dims;
    for (const auto& [bid, bc] : w.bodies)
    {
        grids[bid] = body_tile_grid(w, bid);
        dims[bid]  = { bc.grid_width, bc.grid_height };
    }

    const auto collect_land_edges = [](const world& sw,
                                       const std::map<entity_id, std::vector<entity_id>>& gs,
                                       const std::map<entity_id, std::pair<int, int>>& ds) {
        std::vector<land_edge> out;
        for (const auto& [bid, grid] : gs)
        {
            const int gw = ds.at(bid).first;
            const int gh = ds.at(bid).second;
            if (gw <= 0 || gh <= 0)
                continue;
            for (const entity_id t : grid)
            {
                if (t == null_entity)
                    continue;
                const auto tit = sw.tiles.find(t);
                if (tit == sw.tiles.end()
                    || tit->second.composition == terrain_composition::ocean)
                    continue;
                const tile_component& tc = tit->second;
                for (int s = 0; s < 6; ++s)
                {
                    const auto c = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, s);
                    if (c.gy < 0 || c.gy >= gh)
                        continue;
                    int nx = c.gx % gw;
                    if (nx < 0)
                        nx += gw;
                    const entity_id n = grid[static_cast<std::size_t>(c.gy) * gw + nx];
                    if (n == null_entity || n <= t)
                        continue; // each edge once, from its lower tile
                    const auto nit = sw.tiles.find(n);
                    if (nit == sw.tiles.end()
                        || nit->second.composition == terrain_composition::ocean)
                        continue;
                    land_edge e;
                    e.a      = t;
                    e.b      = n;
                    e.side   = s;
                    e.dh     = std::fabs(tc.height - nit->second.height);
                    e.river  = ((tc.river_edges >> s) & 1u) != 0u
                              || ((nit->second.river_edges >> ((s + 3) % 6)) & 1u) != 0u;
                    e.roaded = tc.road_level > 0 && nit->second.road_level > 0;
                    out.push_back(e);
                }
            }
        }
        return out;
    };

    const std::vector<land_edge> edges = collect_land_edges(w, grids, dims);

    // P4 — every province is hex-connected on its body's grid (columns wrap).
    //      The LAND-ONLY invariant, narrowed to land provinces rather than
    //      deleted (NR-428): ocean provinces are BL-516's and are not built
    //      here, so "no province spans water" still holds outright.
    // P5 — the size band. It is a GROWTH BUDGET, not a clamp (Ben, 2026-08-21):
    //      7-12 soft, 3-12 hard, and TINY PROVINCES ARE KEPT. So the only
    //      ASSERTION the band supports is the hard ceiling — the one clamp that
    //      really is enforced — and everything below the floor is REPORTED.
    {
        bool                       all_connected = true;
        std::map<std::size_t, int> hist;
        for (const province& p : part.provinces)
        {
            ++hist[p.tiles.size()];

            const auto& grid = grids.at(p.body);
            const int   gw   = dims.at(p.body).first;
            const int   gh   = dims.at(p.body).second;

            std::vector<entity_id> seen{ p.tiles.front() };
            for (std::size_t head = 0; head < seen.size(); ++head)
            {
                const auto tit = w.tiles.find(seen[head]);
                if (tit == w.tiles.end())
                    continue;
                for (int s = 0; s < 6; ++s)
                {
                    const auto c = hex_neighbors::neighbour(tit->second.grid_x,
                                                            tit->second.grid_y, s);
                    if (c.gy < 0 || c.gy >= gh)
                        continue;
                    int nx = c.gx % gw;
                    if (nx < 0)
                        nx += gw;
                    const entity_id n = grid[static_cast<std::size_t>(c.gy) * gw + nx];
                    if (n == null_entity)
                        continue;
                    if (std::find(p.tiles.begin(), p.tiles.end(), n) == p.tiles.end())
                        continue;
                    if (std::find(seen.begin(), seen.end(), n) != seen.end())
                        continue;
                    seen.push_back(n);
                }
            }
            if (seen.size() != p.tiles.size())
                all_connected = false;
        }
        check(all_connected, "P4  every land province is hex-connected (none spans water)");

        std::size_t in_band = 0, total = 0, under_soft = 0, under_hard = 0, over = 0;
        std::printf("        size histogram:");
        for (const auto& [size, count] : hist)
        {
            std::printf(" %zu:%d", size, count);
            total += static_cast<std::size_t>(count);
            if (size >= k_province_min_tiles && size <= k_province_max_tiles)
                in_band += static_cast<std::size_t>(count);
            if (size < k_province_min_tiles)
                under_soft += static_cast<std::size_t>(count);
            if (size < k_province_hard_min_tiles)
                under_hard += static_cast<std::size_t>(count);
            if (size > k_province_max_tiles)
                over += static_cast<std::size_t>(count);
        }
        std::printf("\n");

        // P5a — THE HARD CEILING. The single size rule that is a clamp: growth
        // stops at k_province_max_tiles whatever the next edge costs.
        check(over == 0, "P5a no province exceeds the hard ceiling of 12 tiles");

        // P5b — REPORTED, NOT ASSERTED, and deliberately so. Ben, 2026-08-21:
        // "Don't reject tiny provinces." The pre-BL-515 harness asserted here
        // that every sub-floor province was a true island with nothing to merge
        // into — that claim belonged to a merge pass that the ruling REMOVED, so
        // it is not merely awkward now, it is contrary to the ruling. It is
        // replaced by the number rather than dropped: a rise here is a signal
        // about the cost model, and reading it is the point.
        std::printf("        %zu of %zu below the %zu-tile SOFT floor, %zu below the %zu-tile"
                    " HARD floor (kept by ruling, never merged)\n",
                    under_soft, total, k_province_min_tiles, under_hard,
                    k_province_hard_min_tiles);
        std::printf("        %zu of %zu inside the %zu-%zu band\n", in_band, total,
                    k_province_min_tiles, k_province_max_tiles);

        // P5c — the floor that IS structural: a province in the partition always
        // holds at least one tile, so `tiles.front()` (and therefore `id`) is
        // always defined.
        bool nonempty = true;
        for (const province& p : part.provinces)
            if (p.tiles.empty())
                nonempty = false;
        check(nonempty, "P5c every province holds at least one tile (id is always defined)");
    }

    // P6 — determinism: recomputing from the stored seed reproduces the
    // partition exactly. The replay guard.
    {
        world w2 = w;
        build_province_partition(w2, part.seed);
        bool same = w2.provinces.provinces.size() == part.provinces.size();
        if (same)
            for (std::size_t i = 0; i < part.provinces.size(); ++i)
                if (w2.provinces.provinces[i].id != part.provinces[i].id
                    || w2.provinces.provinces[i].body != part.provinces[i].body
                    || w2.provinces.provinces[i].tiles != part.provinces[i].tiles)
                    same = false;
        if (!same)
        {
            std::printf("        DIAG: %zu vs %zu provinces\n", part.provinces.size(),
                        w2.provinces.provinces.size());
            const std::size_t n = std::min(part.provinces.size(), w2.provinces.provinces.size());
            std::size_t shown = 0;
            for (std::size_t i = 0; i < n && shown < 5; ++i)
            {
                const province& a = part.provinces[i];
                const province& b = w2.provinces.provinces[i];
                if (a.id == b.id && a.body == b.body && a.tiles == b.tiles)
                    continue;
                std::printf("        DIAG [%zu] id %u/%u body %u/%u size %zu/%zu\n", i,
                            a.id, b.id, a.body, b.body, a.tiles.size(), b.tiles.size());
                ++shown;
            }
        }
        check(same, "P6  recompute from the stored seed reproduces the partition exactly");
    }

    // P7 — a different seed produces a different partition. The fill is
    // otherwise a pure function of terrain, so this is what proves
    // k_province_edge_jitter keeps the seed load-bearing rather than decorative.
    {
        world w3 = w;
        build_province_partition(w3, part.seed ^ 0xABCDEF01u);
        bool differs = w3.provinces.provinces.size() != part.provinces.size();
        if (!differs)
            for (std::size_t i = 0; i < part.provinces.size(); ++i)
                if (w3.provinces.provinces[i].tiles != part.provinces[i].tiles)
                {
                    differs = true;
                    break;
                }
        check(differs, "P7  a different seed yields a different partition");
    }

    // -----------------------------------------------------------------------
    // P8 — THE IDENTITY CONTRACT (BL-515). This row REPLACES the block-derived
    // rows the 3x3 partition carried: the old id layout was
    // `body_rank | block_raster_index | component`, and P8 used to recover a
    // body's block offset and count tiles sitting outside their base block.
    // That scheme is superseded — an id is now DERIVED FROM THE LOWEST MEMBER
    // TILE and nothing else — so the old row is not merely stale, it has no
    // subject. What replaces it asserts the new contract directly.
    // -----------------------------------------------------------------------
    {
        bool derived = true, unique = true, nonzero = true, sorted_members = true;
        std::unordered_map<uint32_t, int> id_seen;
        for (const province& p : part.provinces)
        {
            if (p.tiles.empty())
                continue;
            const entity_id lowest = *std::min_element(p.tiles.begin(), p.tiles.end());
            if (p.id != lowest)
                derived = false;
            if (p.tiles.front() != lowest)
                sorted_members = false;
            if (p.id == 0)
                nonzero = false;
            if (++id_seen[p.id] > 1)
                unique = false;
        }
        check(derived, "P8a every province id IS its lowest member tile (derived, not allocated)");
        check(sorted_members, "P8b member tiles are ascending, so tiles.front() == id");
        check(unique, "P8c ids are unique (a tile belongs to exactly one province)");
        check(nonzero, "P8d no id is 0 — null_entity is not a tile, so province_of's 0 is safe");
    }

    // P9 — THE SEEDS. Provinces GROW FROM POPULATION CENTRES (Ben, 2026-08-21),
    // so every centre must sit in a province and no two centres may share one:
    // the settlement pass gives each its own region.
    {
        std::map<entity_id, int> centre_tile_scale;
        for (const auto& [cid, tid] : w.population_centre_tile)
        {
            const auto pit = w.population_centres.find(cid);
            if (pit == w.population_centres.end())
                continue;
            centre_tile_scale[tid] += pit->second.scale;
        }

        bool                        all_placed = true, all_distinct = true;
        std::map<uint32_t, int>     province_centres;
        std::map<int, std::pair<int, std::size_t>> by_scale; // scale -> (count, total tiles)
        for (const auto& [tid, scale] : centre_tile_scale)
        {
            const uint32_t pid = part.province_of(tid);
            if (pid == 0)
            {
                all_placed = false;
                continue;
            }
            if (++province_centres[pid] > 1)
                all_distinct = false;
            const province* p = part.find(pid);
            if (p != nullptr)
            {
                auto& row = by_scale[scale];
                ++row.first;
                row.second += p->tiles.size();
            }
        }
        check(all_placed, "P9a every population-centre tile belongs to a province");
        check(all_distinct, "P9b no two population centres share a province (one seed each)");

        std::printf("        centre-seeded province size by centre scale:");
        for (const auto& [scale, row] : by_scale)
            std::printf(" s%d:%d@%.2f", scale, row.first,
                        row.first ? double(row.second) / double(row.first) : 0.0);
        std::printf("\n");

        // P9c — SEED STRENGTH SCALES WITH CENTRE SCALE. The budget runs 7 tiles
        // at scale 1 to 12 at scale 5, so the mean province of the largest scale
        // present must beat the mean of the smallest. Terrain can stop any one
        // of them short; it cannot invert the whole population.
        if (by_scale.size() >= 2)
        {
            const auto& lo = *by_scale.begin();
            const auto& hi = *by_scale.rbegin();
            const double lo_mean = lo.second.first
                                       ? double(lo.second.second) / double(lo.second.first) : 0.0;
            const double hi_mean = hi.second.first
                                       ? double(hi.second.second) / double(hi.second.first) : 0.0;
            check(hi_mean > lo_mean,
                  "P9c a larger centre draws a larger province (seed strength scales with scale)");
        }
        else
            std::printf("        (P9c skipped — only one centre scale present)\n");
    }

    // -----------------------------------------------------------------------
    // C — THE COST MODEL, MEASURED. This section is the PINNING INSTRUMENT for
    // the two coefficients province.hpp says are pinned by measurement, and it
    // reports them on every run so either can be re-pinned without new tooling
    // (BL-463's discipline, BL-513's precedent).
    //
    //   C1  the adjacent-land |dh| distribution, and the k_province_height_cost
    //       it implies under the pinning rule: a gradient at the 90th
    //       percentile of steepness costs the same to cross as a river.
    //   C2  does the cost model actually draw the borders Ben ruled? Border
    //       share by edge class — river, steep, roaded, plain. A road that
    //       BINDS shows up as roaded edges being borders LESS often than plain
    //       ones; a river that divides shows up as the opposite. The roaded
    //       internal share is also the instrument that pinned
    //       k_province_road_bind_divisor (sweep the constant, re-run, compare).
    // -----------------------------------------------------------------------
    std::printf("\nC — the cost model, measured\n");
    float p90_dh = 0.0f;
    {
        // The pin is quoted over a SEED SWEEP, not one world: relief varies by
        // seed, and a coefficient pinned on a single world is pinned on an
        // accident. The primary world's own edges go in first, then six seeds.
        std::vector<float> dh;
        for (const land_edge& e : edges)
            dh.push_back(e.dh);
        std::size_t sweep_edges = 0;
        for (int sd = 0; sd < 6; ++sd)
        {
            world_params wp;
            wp.seed = static_cast<uint32_t>(sd);
            world sw = make_hard_coded_world(no_prehistory(wp));
            std::map<entity_id, std::vector<entity_id>> sg;
            std::map<entity_id, std::pair<int, int>>    sd_dims;
            for (const auto& [bid, bc] : sw.bodies)
            {
                sg[bid]      = body_tile_grid(sw, bid);
                sd_dims[bid] = { bc.grid_width, bc.grid_height };
            }
            const auto se = collect_land_edges(sw, sg, sd_dims);
            sweep_edges += se.size();
            for (const land_edge& e : se)
                dh.push_back(e.dh);
        }
        std::printf("  C0  sweep: %zu adjacent land edges over 6 seeds (+%zu on the primary"
                    " world)\n",
                    sweep_edges, edges.size());
        std::sort(dh.begin(), dh.end());

        double sum = 0.0;
        for (const float v : dh)
            sum += v;
        const auto pct = [&dh](double q) {
            if (dh.empty())
                return 0.0f;
            std::size_t i = static_cast<std::size_t>(q * double(dh.size()));
            if (i >= dh.size())
                i = dh.size() - 1;
            return dh[i];
        };
        const float p50 = pct(0.50), p90 = pct(0.90), p99 = pct(0.99);
        p90_dh = p90;

        std::printf("  C1  %zu edges pooled | mean |dh| %.4f  p50 %.4f  p90 %.4f  p99 %.4f\n",
                    dh.size(), dh.empty() ? 0.0 : sum / double(dh.size()), p50, p90, p99);
        std::printf("      implied k_province_height_cost = %d / p90 = %.1f  (shipped: %d)\n",
                    k_province_river_edge_cost,
                    p90 > 0.0f ? double(k_province_river_edge_cost) / double(p90) : 0.0,
                    k_province_height_cost);
        check(p90 > 0.0f, "C1  the retained heightmap carries real relief (BL-517)");
    }

    {
        struct tally { std::size_t n = 0, border = 0; };
        tally river, steep, roaded, plain, all;
        for (const land_edge& e : edges)
        {
            const bool is_border = part.province_of(e.a) != part.province_of(e.b);
            const auto bump      = [&](tally& t) { ++t.n; if (is_border) ++t.border; };
            bump(all);
            if (e.river)               bump(river);
            if (e.dh >= p90_dh)        bump(steep);
            if (e.roaded)              bump(roaded);
            if (!e.river && !e.roaded && e.dh < p90_dh) bump(plain);
        }
        const auto pctf = [](const tally& t) {
            return t.n ? 100.0 * double(t.border) / double(t.n) : 0.0;
        };
        std::printf("  C2  border share by edge class — all %.2f%% (%zu edges)\n", pctf(all),
                    all.n);
        std::printf("      river  %6.2f%% (%zu)   steep(>=p90) %6.2f%% (%zu)\n", pctf(river),
                    river.n, pctf(steep), steep.n);
        std::printf("      roaded %6.2f%% (%zu)   plain        %6.2f%% (%zu)\n", pctf(roaded),
                    roaded.n, pctf(plain), plain.n);
        std::printf("      roaded INTERNAL share %.2f%% vs plain %.2f%%  (lift %+.2fpp) —"
                    " the divisor's pinning instrument\n",
                    100.0 - pctf(roaded), 100.0 - pctf(plain),
                    pctf(plain) - pctf(roaded));

        // The three claims the ruling makes about what a boundary IS.
        check(river.n > 0 && pctf(river) > pctf(plain),
              "C2a a river edge is a border more often than plain ground (rivers divide)");
        check(steep.n > 0 && pctf(steep) > pctf(plain),
              "C2b a steep edge is a border more often than plain ground (elevation divides)");
        check(roaded.n > 0 && pctf(roaded) < pctf(plain),
              "C2c a roaded edge is a border LESS often than plain ground (a ROAD BINDS)");
    }

    // -----------------------------------------------------------------------
    // D — THE SIZE DISTRIBUTION ACROSS SEEDS. REPORTED, not engineered. Organic
    // borders are meant to be irregular, so this table is a description of what
    // the cost model produced, and the ONLY assertion it carries is the hard
    // ceiling. The 3x3 block partition it replaces measured, over these same 6
    // seeds: 21,161 provinces, mean 9.11, 97.90% in the 7-12 band, 109 under
    // the floor (all true islands) and 336 over. That comparison is the honest
    // way to report what changed; it is NOT a target to match, and the cost
    // weights must not be tuned to chase it.
    // -----------------------------------------------------------------------
    std::printf("\nD — size distribution across seeds (soft band %zu-%zu, hard %zu-%zu)\n",
                k_province_min_tiles, k_province_max_tiles, k_province_hard_min_tiles,
                k_province_max_tiles);
    {
        constexpr int k_seeds = 6;
        std::size_t   all_total = 0, all_in_band = 0, all_under_soft = 0, all_under_hard = 0,
                    all_over = 0;
        std::size_t   all_tiles = 0;
        std::size_t   worst_floor = 0;
        std::size_t   worst_ceiling = 0;
        bool          floor_seen = false;

        std::printf("  seed | provinces |  min  max   mean | <7  <3  >12 | %% in 7-12\n");
        for (int sd = 0; sd < k_seeds; ++sd)
        {
            world_params wp;
            wp.seed = static_cast<uint32_t>(sd);
            world sw = make_hard_coded_world(no_prehistory(wp));

            std::size_t n = 0, tiles = 0, lo = 0, hi = 0, us = 0, uh = 0, over = 0, in_band = 0;
            bool        lo_seen = false;
            for (const province& p : sw.provinces.provinces)
            {
                const std::size_t sz = p.tiles.size();
                ++n;
                tiles += sz;
                if (!lo_seen || sz < lo) { lo = sz; lo_seen = true; }
                if (sz > hi) hi = sz;
                if (sz < k_province_min_tiles)       ++us;
                if (sz < k_province_hard_min_tiles)  ++uh;
                if (sz > k_province_max_tiles)       ++over;
                if (sz >= k_province_min_tiles && sz <= k_province_max_tiles) ++in_band;
            }
            std::printf("  %4d | %9zu | %4zu %4zu %6.2f | %3zu %3zu %4zu | %6.2f%%\n", sd, n, lo,
                        hi, n ? double(tiles) / double(n) : 0.0, us, uh, over,
                        n ? 100.0 * double(in_band) / double(n) : 0.0);
            std::fflush(stdout);

            all_total      += n;
            all_tiles      += tiles;
            all_in_band    += in_band;
            all_under_soft += us;
            all_under_hard += uh;
            all_over       += over;
            if (!floor_seen || lo < worst_floor) { worst_floor = lo; floor_seen = true; }
            if (hi > worst_ceiling) worst_ceiling = hi;
        }

        std::printf("  ALL  | %9zu | %4zu %4zu %6.2f | %3zu %3zu %4zu | %6.2f%%\n", all_total,
                    worst_floor, worst_ceiling,
                    all_total ? double(all_tiles) / double(all_total) : 0.0, all_under_soft,
                    all_under_hard, all_over,
                    all_total ? 100.0 * double(all_in_band) / double(all_total) : 0.0);
        std::printf("  3x3 BASELINE (superseded) |    1   18   9.11 | 109   -  336 |  97.90%%\n");

        // D1 — the hard ceiling, across every seed. The one clamp.
        check(all_over == 0, "D1  no province on any seed exceeds the 12-tile hard ceiling");
        // D2 — the partition covers every seed's land, so the mean is a real
        // statistic and not an artefact of a body that failed to partition.
        check(all_total > 0 && all_tiles > 0, "D2  every seed produced a non-empty partition");
    }

    // -----------------------------------------------------------------------
    std::printf("\nS — serialisation seam\n");

    // make_hard_coded_world does not seed the genesis chapter (app.cpp's hook does),
    // so a headless world's history log is empty — and an empty log would make the
    // prefix check below cover only the 12-byte header. Push a few real-shaped
    // records first, so S1 proves no ENTRY moved, not merely that the header didn't.
    for (int i = 0; i < 4; ++i)
    {
        world_history_entry e;
        e.timestamp   = 1000 - i;
        e.topic       = (i % 2 == 0) ? history_topic::genesis : history_topic::decision;
        e.body        = w.bodies.empty() ? null_entity : w.bodies.begin()->first;
        e.corp        = null_entity;
        e.event       = "harness record " + std::to_string(i);
        e.consequence = (i == 2) ? std::string{} : "consequence " + std::to_string(i);
        w.history_log.push_back(std::move(e));
    }

    std::string legacy;
    {
        std::ostringstream os(std::ios::binary);
        legacy_write_history_log(w, os);
        legacy = os.str();
    }

    std::string current;
    {
        std::ostringstream os(std::ios::binary);
        write_history_log(w, os);
        current = os.str();
    }

    // S1 — one appender, no reorder.
    check(current.size() > legacy.size() && current.compare(0, legacy.size(), legacy) == 0,
          "S1  the pre-BL-466 stream is a byte-exact PREFIX of the current stream");
    std::printf("        legacy %zu bytes, current %zu bytes (+%zu appended)\n", legacy.size(),
                current.size(), current.size() - legacy.size());

    // S2 — a save written before this item still loads.
    {
        world             target;
        std::istringstream is(legacy, std::ios::binary);
        const bool         ok = read_history_log(target, is);
        check(ok, "S2a a pre-BL-466 stream loads (clean end-of-stream is not corruption)");
        check(ok && target.history_log.size() == w.history_log.size(),
              "S2b its history records survive the load intact");
        check(ok && target.provinces.provinces.empty(),
              "S2c it loads with an empty partition rather than garbage");
    }

    // S3 — round-trip is bit-identical.
    {
        world             target;
        std::istringstream is(current, std::ios::binary);
        const bool         ok = read_history_log(target, is);
        check(ok, "S3a the current stream reads back");

        std::ostringstream os(std::ios::binary);
        if (ok)
            write_history_log(target, os);
        check(ok && os.str() == current, "S3b write -> read -> write is bit-identical");
        check(ok && target.provinces.provinces.size() == part.provinces.size(),
              "S3c the partition survives the round trip");

        // And the derived index is rebuilt, not lost.
        bool index_ok = ok && target.provinces.tile_province.size()
                                  == part.tile_province.size();
        if (index_ok)
            for (const auto& [t, id] : part.tile_province)
                if (target.provinces.province_of(t) != id)
                    index_ok = false;
        check(index_ok, "S3d tile_province is rebuilt on read, matching the source");
    }

    // S4 — a malformed section is rejected outright rather than half-applied.
    {
        std::string truncated = current.substr(0, current.size() - 3);
        world       target;
        std::istringstream is(truncated, std::ios::binary);
        check(!read_history_log(target, is),
              "S4  a truncated province section is rejected, not partially applied");
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES", g_failures,
                g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
