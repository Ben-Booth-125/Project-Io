// ---------------------------------------------------------------------------
// lake_census — BL-1200 (lake size cap): the "measure first" the item demands.
//
// NR-974 (Ben, 2026-10-05) ruled that a lake is an enclosed water body BELOW a
// measured size cap; a larger enclosed body is a sea. This probe is the
// measurement: on each seed, on every body that carries water, it flood-fills
// the water into connected components on the generator's own hex adjacency
// (odd-r offset, columns wrap, rows open at the poles) and, for every component
// OTHER than the largest (the one classify_water_kinds always made the sea),
// reports:
//
//   size        tiles in the component
//   shore       distinct LAND tiles bordering it (the Well sites a lake makes)
//   habitable   how many of those have habitability > 0 (placement_rules'
//               own test for buildable ground)
//   hospitable  how many have habitability >= 0.5 (on the home body every
//               shore tile passes the > 0 test, so this column discriminates)
//   kind        what the tiles currently REPORT (lake / coast / ocean), so the
//               output also shows where the cap sends each body
//
// It then prints the pooled sorted size distribution, a per-seed summary, and a
// candidate-cap table: for each cap, how many enclosed bodies stay lakes and how
// many become seas.
//
// The water mask is re-derived from `tile_component::substrate` (any of ocean /
// coast / lake), not from the classifier under test, so a change to the cap
// cannot change what this probe measures — only the `kind` column moves.
//
// REPORTS, NEVER GATES. There is no expected count; the world moves.
//
//   node tools/verify/build_harness.js lake_census
//   build_gen/verify/lake_census.exe [--seeds 46,28,...] [--all-bodies]
//
// Default seeds: the 16 curated seeds (docs/generation/seed_library.json).
// Default scope: the home body only (the generated homeworld — where every
// settlement, Well and wharf lives); --all-bodies widens it.
// ---------------------------------------------------------------------------

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace
{

// Restated from tile_generation.cpp (anonymous namespace there): six lines, no
// state. If it ever grows, promote it instead.
void hex_neighbours(int col, int row, int gw, int gh, std::pair<int, int> out[6], int& count)
{
    static constexpr int even_dc[6] = {  0, -1,  1, -1,  0, -1 };
    static constexpr int even_dr[6] = { -1, -1,  0,  0,  1,  1 };
    static constexpr int odd_dc[6]  = {  0,  1,  1, -1,  0, -1 };
    static constexpr int odd_dr[6]  = { -1, -1,  0,  0,  1,  1 };
    const int* dc = (row & 1) ? odd_dc : even_dc;
    const int* dr = (row & 1) ? odd_dr : even_dr;
    count = 0;
    for (int i = 0; i < 6; ++i)
    {
        const int nr = row + dr[i];
        if (nr < 0 || nr >= gh) continue;
        const int nc = ((col + dc[i]) % gw + gw) % gw;
        out[count++] = { nc, nr };
    }
}

bool is_water_sub(terrain_substrate s)
{
    return s == terrain_substrate::ocean || s == terrain_substrate::coast
        || s == terrain_substrate::lake;
}

struct component_row
{
    unsigned    seed = 0;
    std::string body;
    int         size = 0;
    int         shore = 0;
    int         habitable = 0;
    int         hospitable = 0; ///< shore tiles with habitability >= 0.5
    int         largest = 0; ///< the body's largest component, for scale
    const char* kind = "?";
};

world_params no_era(unsigned seed)
{
    world_params p;
    p.seed             = seed;
    p.prehistory_years = 0; // the water mask is fixed at tile generation
    return p;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<unsigned> seeds = { 46, 28, 11, 31, 40, 12, 37, 13, 41, 43, 32, 10, 25, 38, 9, 0 };
    bool all_bodies = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            seeds.clear();
            std::string s = argv[++i];
            std::size_t pos = 0;
            while (pos <= s.size())
            {
                const std::size_t comma = s.find(',', pos);
                const std::string tok = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                if (!tok.empty()) seeds.push_back(static_cast<unsigned>(std::strtoul(tok.c_str(), nullptr, 10)));
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
        }
        else if (std::strcmp(argv[i], "--all-bodies") == 0)
            all_bodies = true;
    }

    std::printf("lake_census (BL-1200 lake size cap) - enclosed water bodies, largest excluded\n");
    std::printf("scope: %s\n\n", all_bodies ? "every body with water" : "home body");

    std::vector<component_row> rows;
    std::map<unsigned, int> body_tiles_by_seed;

    for (unsigned seed : seeds)
    {
        world w = make_hard_coded_world(no_era(seed));

        // Deterministic body order: ascending entity id.
        std::vector<entity_id> body_ids;
        for (const auto& [bid, b] : w.bodies) body_ids.push_back(bid);
        std::sort(body_ids.begin(), body_ids.end());

        for (entity_id bid : body_ids)
        {
            if (!all_bodies && bid != w.home_body) continue;
            const body_component& b = w.bodies.at(bid);
            const int gw = b.grid_width, gh = b.grid_height;
            if (gw <= 0 || gh <= 0) continue;
            const int total = gw * gh;

            std::vector<signed char> water(static_cast<std::size_t>(total), -1); // -1 absent
            std::vector<float> hab(static_cast<std::size_t>(total), 0.0f);
            std::vector<terrain_substrate> sub(static_cast<std::size_t>(total), terrain_substrate::ocean);
            for (const auto& [tid, t] : w.tiles)
            {
                if (t.body != bid) continue;
                const int idx = t.grid_x + t.grid_y * gw;
                if (idx < 0 || idx >= total) continue;
                water[static_cast<std::size_t>(idx)] = is_water_sub(t.substrate) ? 1 : 0;
                hab[static_cast<std::size_t>(idx)]   = t.habitability;
                sub[static_cast<std::size_t>(idx)]   = t.substrate;
            }
            if (bid == w.home_body) body_tiles_by_seed[seed] = total;

            std::vector<int> comp(static_cast<std::size_t>(total), -1);
            std::vector<int> sizes;
            std::vector<int> first_tile;
            std::vector<int> stack;
            for (int s = 0; s < total; ++s)
            {
                if (water[static_cast<std::size_t>(s)] != 1 || comp[static_cast<std::size_t>(s)] >= 0) continue;
                const int id = static_cast<int>(sizes.size());
                int n = 0;
                stack.assign(1, s);
                comp[static_cast<std::size_t>(s)] = id;
                while (!stack.empty())
                {
                    const int cur = stack.back(); stack.pop_back(); ++n;
                    std::pair<int, int> nb[6]; int cnt = 0;
                    hex_neighbours(cur % gw, cur / gw, gw, gh, nb, cnt);
                    for (int k = 0; k < cnt; ++k)
                    {
                        const int ni = nb[k].first + nb[k].second * gw;
                        if (water[static_cast<std::size_t>(ni)] != 1 || comp[static_cast<std::size_t>(ni)] >= 0) continue;
                        comp[static_cast<std::size_t>(ni)] = id;
                        stack.push_back(ni);
                    }
                }
                sizes.push_back(n);
                first_tile.push_back(s);
            }
            if (sizes.empty()) continue;

            int sea = 0;
            for (int i = 1; i < static_cast<int>(sizes.size()); ++i)
                if (sizes[static_cast<std::size_t>(i)] > sizes[static_cast<std::size_t>(sea)]) sea = i;

            // Shore: distinct land tiles bordering each component.
            std::vector<std::vector<int>> shore(sizes.size());
            for (int idx = 0; idx < total; ++idx)
            {
                if (water[static_cast<std::size_t>(idx)] != 0) continue; // land only
                std::pair<int, int> nb[6]; int cnt = 0;
                hex_neighbours(idx % gw, idx / gw, gw, gh, nb, cnt);
                int seen[6]; int ns = 0;
                for (int k = 0; k < cnt; ++k)
                {
                    const int c = comp[static_cast<std::size_t>(nb[k].first + nb[k].second * gw)];
                    if (c < 0) continue;
                    bool dup = false;
                    for (int j = 0; j < ns; ++j) dup = dup || seen[j] == c;
                    if (dup) continue;
                    seen[ns++] = c;
                    shore[static_cast<std::size_t>(c)].push_back(idx);
                }
            }

            for (int c = 0; c < static_cast<int>(sizes.size()); ++c)
            {
                if (c == sea) continue;
                component_row r;
                r.seed    = seed;
                r.body    = b.name;
                r.size    = sizes[static_cast<std::size_t>(c)];
                r.largest = sizes[static_cast<std::size_t>(sea)];
                r.shore   = static_cast<int>(shore[static_cast<std::size_t>(c)].size());
                for (int t : shore[static_cast<std::size_t>(c)])
                {
                    if (hab[static_cast<std::size_t>(t)] > 0.0f) ++r.habitable;
                    if (hab[static_cast<std::size_t>(t)] >= 0.5f) ++r.hospitable;
                }
                const terrain_substrate k = sub[static_cast<std::size_t>(first_tile[static_cast<std::size_t>(c)])];
                r.kind = k == terrain_substrate::lake ? "lake" : (k == terrain_substrate::coast ? "coast" : "ocean");
                rows.push_back(r);
            }
        }
    }

    // --- Per seed ---------------------------------------------------------
    std::printf("Per seed (enclosed components, sorted by size descending)\n");
    for (unsigned seed : seeds)
    {
        std::vector<component_row> mine;
        for (const component_row& r : rows) if (r.seed == seed) mine.push_back(r);
        std::stable_sort(mine.begin(), mine.end(),
                         [](const component_row& a, const component_row& b) { return a.size > b.size; });
        int tiles = 0; for (const component_row& r : mine) tiles += r.size;
        std::printf("seed %3u: %3zu enclosed bodies, %5d tiles", seed, mine.size(), tiles);
        if (!mine.empty()) std::printf(" (largest sea %d)", mine.front().largest);
        std::printf("\n   size/shore/hab[kind]:");
        int shown = 0;
        for (const component_row& r : mine)
        {
            if (shown++ >= 12) { std::printf(" ..."); break; }
            std::printf(" %d/%d/%d%s", r.size, r.shore, r.habitable,
                        std::strcmp(r.kind, "lake") == 0 ? "" : (std::string("[") + r.kind + "]").c_str());
        }
        std::printf("\n");
        // The seed's own natural break: the widest size RATIO between
        // consecutive enclosed bodies of 10+ tiles (smaller ones are puddles
        // whose ratios are noise).
        double best = 0.0; int hi = 0, lo = 0;
        for (std::size_t i = 0; i + 1 < mine.size(); ++i)
        {
            if (mine[i + 1].size < 10) break;
            const double ratio = static_cast<double>(mine[i].size) / mine[i + 1].size;
            if (ratio > best) { best = ratio; hi = mine[i].size; lo = mine[i + 1].size; }
        }
        if (best > 0.0)
            std::printf("   widest break (10+ tiles): %d -> %d (x%.2f)\n", hi, lo, best);
    }

    // --- Pooled distribution ----------------------------------------------
    std::vector<component_row> pooled = rows;
    std::stable_sort(pooled.begin(), pooled.end(),
                     [](const component_row& a, const component_row& b) { return a.size > b.size; });
    std::printf("\nPooled, sorted by size (top 120): seed body size shore habitable hospitable kind\n");
    for (std::size_t i = 0; i < pooled.size() && i < 120; ++i)
        std::printf("  %3u %-10s %5d %5d %5d %5d %s\n", pooled[i].seed, pooled[i].body.c_str(),
                    pooled[i].size, pooled[i].shore, pooled[i].habitable, pooled[i].hospitable,
                    pooled[i].kind);

    // Size histogram.
    const int edges[] = { 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987, 1597, 2584, 4181, 1 << 30 };
    std::printf("\nSize histogram (pooled): [lo, hi) count tiles shore habitable\n");
    for (std::size_t e = 0; e + 1 < sizeof(edges) / sizeof(edges[0]); ++e)
    {
        int n = 0, t = 0, sh = 0, hb = 0;
        for (const component_row& r : rows)
            if (r.size >= edges[e] && r.size < edges[e + 1]) { ++n; t += r.size; sh += r.shore; hb += r.habitable; }
        if (n == 0) continue;
        std::printf("  [%5d, %5d)  %4d  %6d  %6d  %6d\n", edges[e], edges[e + 1] == (1 << 30) ? 99999 : edges[e + 1], n, t, sh, hb);
    }

    // Candidate caps.
    const int caps[] = { 8, 13, 21, 34, 50, 55, 89, 100, 144, 200, 233, 300, 377, 500, 610, 1000 };
    std::printf("\nCandidate caps (size >= cap becomes sea): cap lakes_kept lake_tiles lake_shore_hab | seas seas_tiles seas_shore_hab\n");
    for (int cap : caps)
    {
        int lk = 0, lt = 0, lh = 0, sk = 0, st = 0, sh = 0;
        for (const component_row& r : rows)
        {
            if (r.size < cap) { ++lk; lt += r.size; lh += r.habitable; }
            else              { ++sk; st += r.size; sh += r.habitable; }
        }
        std::printf("  %5d  %4d %6d %6d | %4d %6d %6d\n", cap, lk, lt, lh, sk, st, sh);
    }
    return 0;
}
