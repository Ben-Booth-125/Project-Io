// Headless harness for the province partition (BL-466).
//
// Two halves:
//   P — the partition's own invariants (coverage, no ocean, connectivity, size
//       band, ascending-id contract, seeded determinism, jitter did not no-op).
//   D — the SIZE DISTRIBUTION across a seed sweep. Ben's 2026-08-21 ruling put
//       the target at 7-12 tiles, and a mean says nothing about whether the
//       floor HOLDS where coastline cuts a block up — only the spread does. D
//       reports floor, ceiling, mean and the out-of-band count per seed.
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
    std::printf("Province partition harness (BL-466)\n\n");

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

    // P4 — every province is hex-connected on its body's grid (columns wrap).
    // P5 — size band (Ben, 2026-08-21): the target is 7-12. A full 3x3 block is
    //      9; the merge pass lifts coastal fragments to the floor, and only a
    //      true islet — land with no land neighbour — may ship under it.
    {
        std::map<entity_id, std::vector<entity_id>> grids;
        std::map<entity_id, std::pair<int, int>>    dims;
        for (const auto& [bid, bc] : w.bodies)
        {
            grids[bid] = body_tile_grid(w, bid);
            dims[bid]  = { bc.grid_width, bc.grid_height };
        }

        bool                   all_connected = true;
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
        check(all_connected, "P4  every province is hex-connected (no province spans water)");

        std::size_t in_band = 0, total = 0, under_floor = 0, islets = 0;
        std::printf("        size histogram:");
        for (const auto& [size, count] : hist)
        {
            std::printf(" %zu:%d", size, count);
            total += static_cast<std::size_t>(count);
            if (size >= k_province_min_tiles && size <= k_province_max_tiles)
                in_band += static_cast<std::size_t>(count);
            if (size < k_province_min_tiles)
            {
                under_floor += static_cast<std::size_t>(count);
                if (size == 1)
                    islets += static_cast<std::size_t>(count);
            }
        }
        std::printf("\n");
        // The band is a shape claim, not a clamp: the coast bends it, and the
        // merge cannot invent land for a true islet. The interior must dominate.
        check(total > 0 && (in_band * 100 / total) >= 50,
              "P5a the majority of provinces sit in the 7-12 tile band");
        std::printf("        %zu of %zu below the %zu-tile floor (%zu single-tile islets"
                    " with no land neighbour)\n",
                    under_floor, total, k_province_min_tiles, islets);
        // P5b is the claim the 2026-08-21 ruling actually turns on: a fragment
        // ships ONLY when there was nothing to merge it into. Anything else
        // under the floor means the merge pass missed a case.
        check(under_floor * 20 < total,
              "P5b sub-floor provinces are rare (the merge pass holds the floor)");

        // P5c — the STRONGEST form of the floor claim, and the one that would
        // catch a merge-pass bug that P5b's 5% slack would hide: a province
        // below the floor must have NO land neighbour at all. If it had one,
        // the merge had somewhere to put it and failed to.
        {
            std::size_t stranded = 0;
            for (const province& p : part.provinces)
            {
                if (p.tiles.size() >= k_province_min_tiles)
                    continue;
                const auto& grid = grids.at(p.body);
                const int   gw   = dims.at(p.body).first;
                const int   gh   = dims.at(p.body).second;
                bool        has_neighbour = false;
                for (const entity_id t : p.tiles)
                {
                    const tile_component& tc = w.tiles.at(t);
                    for (int s = 0; s < 6 && !has_neighbour; ++s)
                    {
                        const auto c = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, s);
                        if (c.gy < 0 || c.gy >= gh)
                            continue;
                        int nx = c.gx % gw;
                        if (nx < 0)
                            nx += gw;
                        const entity_id n = grid[static_cast<std::size_t>(c.gy) * gw + nx];
                        if (n == null_entity)
                            continue;
                        const uint32_t owner = part.province_of(n);
                        if (owner != 0 && owner != p.id)
                            has_neighbour = true;
                    }
                }
                if (has_neighbour)
                    ++stranded;
            }
            std::printf("        %zu sub-floor provinces still had a neighbour to merge into\n",
                        stranded);
            check(stranded == 0,
                  "P5c every sub-floor province is a true island (nothing to merge into)");
        }
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

    // P7 — a different seed produces a different partition. Proves the seed is
    // actually load-bearing rather than decorative.
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

    // P8 — jitter did not silently no-op: at least one province holds a tile
    // whose block-derived home was a DIFFERENT province, i.e. a border moved.
    // Compared against a jitter-free reference: the pure 2x2 block assignment.
    {
        std::size_t moved = 0;
        for (const auto& [bid, bc] : w.bodies)
        {
            const int gw = bc.grid_width, gh = bc.grid_height;
            if (gw <= 0 || gh <= 0)
                continue;
            // Recover this body's (dx, dy) from any province: a tile's block
            // coords must agree with its province id's block field under the
            // right offset. Try all nine (the 3x3 block edge, 2026-08-21) and
            // take the one that explains the most.
            int best_dx = 0, best_dy = 0;
            std::size_t best_hits = 0;
            const int   e = k_province_block_edge;
            for (int dx = 0; dx < e; ++dx)
                for (int dy = 0; dy < e; ++dy)
                {
                    const int   cols = (gw + dx + e - 1) / e;
                    std::size_t hits = 0;
                    for (const province& p : part.provinces)
                    {
                        if (p.body != bid)
                            continue;
                        const uint32_t block = (p.id >> 3) & 0x1FFFFu;
                        for (const entity_id t : p.tiles)
                        {
                            const tile_component& tc = w.tiles.at(t);
                            const uint32_t bx = static_cast<uint32_t>((tc.grid_x + dx) / e);
                            const uint32_t by = static_cast<uint32_t>((tc.grid_y + dy) / e);
                            if (by * static_cast<uint32_t>(cols) + bx == block)
                                ++hits;
                        }
                    }
                    if (hits > best_hits)
                    {
                        best_hits = hits;
                        best_dx   = dx;
                        best_dy   = dy;
                    }
                }

            const int cols = (gw + best_dx + e - 1) / e;
            for (const province& p : part.provinces)
            {
                if (p.body != bid)
                    continue;
                const uint32_t block = (p.id >> 3) & 0x1FFFFu;
                for (const entity_id t : p.tiles)
                {
                    const tile_component& tc = w.tiles.at(t);
                    const uint32_t bx = static_cast<uint32_t>((tc.grid_x + best_dx) / e);
                    const uint32_t by = static_cast<uint32_t>((tc.grid_y + best_dy) / e);
                    if (by * static_cast<uint32_t>(cols) + bx != block)
                        ++moved;
                }
            }
        }
        std::printf("        %zu tiles sit outside their base block (merge + jitter)\n", moved);
        check(moved > 0, "P8  merge/jitter moved at least one border off the raw block grid");
    }

    // -----------------------------------------------------------------------
    // D — THE SIZE DISTRIBUTION ACROSS SEEDS. The ruling is about a spread, not
    // a mean: a partition averaging 9 tiles while shipping a tail of 2s has not
    // met it. So report floor, ceiling, mean and the out-of-band count for each
    // seed, and assert on the aggregate.
    std::printf("\nD — size distribution across seeds (target %zu-%zu)\n",
                k_province_min_tiles, k_province_max_tiles);
    {
        constexpr int k_seeds = 6;
        std::size_t   all_total = 0, all_in_band = 0, all_under = 0, all_over = 0;
        std::size_t   all_tiles = 0;
        std::size_t   worst_floor = 0;
        std::size_t   worst_ceiling = 0;
        bool          floor_seen = false;

        std::printf("  seed | provinces |  min  max   mean | under over | %% in band\n");
        for (int sd = 0; sd < k_seeds; ++sd)
        {
            world_params wp;
            wp.seed = static_cast<uint32_t>(sd);
            world sw = make_hard_coded_world(no_prehistory(wp));

            std::size_t n = 0, tiles = 0, lo = 0, hi = 0, under = 0, over = 0, in_band = 0;
            bool        lo_seen = false;
            for (const province& p : sw.provinces.provinces)
            {
                const std::size_t sz = p.tiles.size();
                ++n;
                tiles += sz;
                if (!lo_seen || sz < lo) { lo = sz; lo_seen = true; }
                if (sz > hi) hi = sz;
                if (sz < k_province_min_tiles)      ++under;
                else if (sz > k_province_max_tiles) ++over;
                else                                ++in_band;
            }
            std::printf("  %4d | %9zu | %4zu %4zu %6.2f | %5zu %4zu | %6.2f%%\n", sd, n, lo, hi,
                        n ? double(tiles) / double(n) : 0.0, under, over,
                        n ? 100.0 * double(in_band) / double(n) : 0.0);
            std::fflush(stdout);

            all_total   += n;
            all_tiles   += tiles;
            all_in_band += in_band;
            all_under   += under;
            all_over    += over;
            if (!floor_seen || lo < worst_floor) { worst_floor = lo; floor_seen = true; }
            if (hi > worst_ceiling) worst_ceiling = hi;
        }

        std::printf("  ALL  | %9zu | %4zu %4zu %6.2f | %5zu %4zu | %6.2f%%\n", all_total,
                    worst_floor, worst_ceiling,
                    all_total ? double(all_tiles) / double(all_total) : 0.0, all_under, all_over,
                    all_total ? 100.0 * double(all_in_band) / double(all_total) : 0.0);

        const double mean = all_total ? double(all_tiles) / double(all_total) : 0.0;
        check(mean >= double(k_province_min_tiles) && mean <= double(k_province_max_tiles),
              "D1  the mean province sits inside the 7-12 band");
        check(all_total > 0 && all_in_band * 4 >= all_total * 3,
              "D2  at least three quarters of provinces are inside the band");
        check(all_total > 0 && all_under * 20 < all_total,
              "D3  under-floor provinces stay under 5%% of the world");
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
