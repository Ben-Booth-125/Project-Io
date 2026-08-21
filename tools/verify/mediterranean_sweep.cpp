// ---------------------------------------------------------------------------
// Mediterranean sweep (no SDL / Lua / ImGui) — a MEASUREMENT tool, not pass/fail
// ---------------------------------------------------------------------------
// Question (Ben, 2026-08-03): across campaign seeds, how often does Kepler's
// ocean pass produce a Mediterranean-like structure — an inland sea enclosed
// (or nearly enclosed, via a narrow strait) by land? TILE_GENERATION.md
// § Deferred already notes the noise-thresholded coastline "lacks enclosed
// seas"; this harness puts a number on it before any tuning/hard-coding call.
//
// Replicates plan()'s Kepler wiring from hard_coded_world.cpp exactly:
//   resolve_preferences(default, seed) -> prototype_body(1) at home orbit ->
//   run_planetology -> run_continents(body_seed ^ 0xC0117E57) ->
//   generate_body_tiles(seed ^ 0xE471001, bias).
//
// Definitions measured per seed:
//   * enclosed sea  — an ocean connected-component (hex adjacency, columns
//     wrap) other than the largest, sized in [30, 1500] tiles (~0.2%-10% of
//     the 180x84 grid; Earth's Mediterranean is ~0.5% of surface).
//   * strait sea    — a region of the MAIN ocean that separates from it when
//     "narrow" ocean tiles (>= 4 of 6 neighbours land) are removed, same size
//     band. Gibraltar-like: connected, but through a chokepoint.
//
// Run: mediterranean_sweep [seeds] [dump_top]   (default 200 sweeps, dump 3
// ASCII maps of the best candidates to stdout).
// ---------------------------------------------------------------------------

#include "world/continents.hpp"
#include "world/planetology.hpp"
#include "world/tile_generation.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr int gw = 180, gh = 84;

int g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

// Odd-r offset hex neighbours (odd rows shifted right), columns wrap.
void neighbours(int c, int r, int out_c[6], int out_r[6])
{
    static const int even_d[6][2] = { {1,0},{-1,0},{0,-1},{-1,-1},{0,1},{-1,1} };
    static const int odd_d[6][2]  = { {1,0},{-1,0},{1,-1},{0,-1},{1,1},{0,1} };
    const auto& d = (r & 1) ? odd_d : even_d;
    for (int i = 0; i < 6; ++i)
    {
        out_c[i] = ((c + d[i][0]) % gw + gw) % gw;
        out_r[i] = r + d[i][1];
    }
}

/// Label 6-connected components over `mask` (true = in-set). Returns component
/// id per cell (-1 outside the set) and fills `sizes` per component.
std::vector<int> label(const std::vector<char>& mask, std::vector<int>& sizes)
{
    std::vector<int> comp(mask.size(), -1);
    sizes.clear();
    std::vector<int> stack;
    for (int i = 0; i < static_cast<int>(mask.size()); ++i)
    {
        if (!mask[i] || comp[i] != -1) continue;
        const int id = static_cast<int>(sizes.size());
        sizes.push_back(0);
        stack.push_back(i);
        comp[i] = id;
        while (!stack.empty())
        {
            const int t = stack.back();
            stack.pop_back();
            ++sizes[id];
            int nc[6], nr[6];
            neighbours(t % gw, t / gw, nc, nr);
            for (int k = 0; k < 6; ++k)
            {
                if (nr[k] < 0 || nr[k] >= gh) continue;
                const int n = nr[k] * gw + nc[k];
                if (mask[n] && comp[n] == -1) { comp[n] = id; stack.push_back(n); }
            }
        }
    }
    return comp;
}

struct sea { int size = 0; int tile = -1; bool strait = false; };

struct seed_result
{
    uint32_t seed = 0;
    float ocean_frac = 0.0f;
    bool basin_stamped = false; // continents pass found a continental divergent pair
    int largest_secondary = 0;  // biggest non-main ocean component, uncapped
    std::vector<sea> seas;      // candidate inland seas, both kinds
    std::vector<char> ocean;    // kept only for dumped seeds
};

seed_result sweep_one(uint32_t campaign_seed, bool keep_map)
{
    const resolved_world rw = resolve_preferences(world_preferences{}, campaign_seed);
    body_inputs home = prototype_body(1);
    home.orbit_au = rw.home_orbit_au;
    const uint32_t body_seed = campaign_seed ^ prototype_body_seed(1);
    const planetology_state st = run_planetology(home, rw.params, body_seed);
    const continent_state cs = run_continents(st, gw, gh, body_seed ^ 0xC0117E57u);

    // Mirror hard_coded_world's BL-276 two-bar acceptance gate exactly:
    // ARENA (>= 300 tiles, attempts 0-2 only) then FLOOR (>= 30, any attempt),
    // attempt 0 kept on full exhaustion. Keep this loop in sync with the gate
    // in hard_coded_world.cpp — the sweep measures the SHIPPED pipeline.
    auto probe_sea = [&](uint32_t tile_seed)
    {
        world scratch;
        const entity_id body = scratch.create_entity();
        auto probe = generate_body_tiles(scratch, body, gw, gh, st.profile,
            tile_seed, 1.0f, &st, nullptr, &cs.height_bias, &cs.convergent);
        std::vector<char> pocean(probe.size(), 0);
        for (std::size_t i = 0; i < probe.size(); ++i)
            if (scratch.tiles.at(probe[i]).substrate == terrain_substrate::ocean) pocean[i] = 1;
        std::vector<int> psizes;
        label(pocean, psizes);
        int pmain = -1, pmain_sz = 0;
        for (int i = 0; i < static_cast<int>(psizes.size()); ++i)
            if (psizes[static_cast<std::size_t>(i)] > pmain_sz) { pmain_sz = psizes[static_cast<std::size_t>(i)]; pmain = i; }
        int best = 0;
        for (int i = 0; i < static_cast<int>(psizes.size()); ++i)
            if (i != pmain) best = std::max(best, psizes[static_cast<std::size_t>(i)]);
        return best;
    };

    uint32_t chosen_seed = campaign_seed ^ 0xE471001u;
    {
        uint32_t floor_seed = 0;
        bool have_floor = false, have_arena = false;
        for (int attempt = 0; attempt < 6; ++attempt)
        {
            const uint32_t candidate = (campaign_seed ^ 0xE471001u) ^ (static_cast<uint32_t>(attempt) * 0x9E3779B9u);
            const int sea = probe_sea(candidate);
            if (attempt < 3 && sea >= 300) { chosen_seed = candidate; have_arena = true; break; }
            if (!have_floor && sea >= 30) { floor_seed = candidate; have_floor = true; }
            if (attempt >= 2 && have_floor) break;
        }
        if (!have_arena && have_floor) chosen_seed = floor_seed;
    }

    world w;
    const entity_id fb = w.create_entity();
    std::vector<entity_id> ids = generate_body_tiles(w, fb, gw, gh, st.profile,
        chosen_seed, 1.0f, &st, nullptr, &cs.height_bias, &cs.convergent);

    std::vector<char> ocean(static_cast<std::size_t>(gw) * gh, 0);
    int ocean_n = 0;
    for (int i = 0; i < static_cast<int>(ids.size()); ++i)
    {
        const auto& tc = w.tiles.at(ids[static_cast<std::size_t>(i)]);
        if (tc.substrate == terrain_substrate::ocean) { ocean[static_cast<std::size_t>(i)] = 1; ++ocean_n; }
    }

    seed_result out;
    out.seed = campaign_seed;
    out.ocean_frac = static_cast<float>(ocean_n) / static_cast<float>(gw * gh);
    for (const float b : cs.height_bias)
        if (b < -0.30f) { out.basin_stamped = true; break; }

    // Fully enclosed seas: every ocean component except the largest.
    std::vector<int> sizes;
    const std::vector<int> comp = label(ocean, sizes);
    int main_id = -1, main_sz = 0;
    for (int i = 0; i < static_cast<int>(sizes.size()); ++i)
        if (sizes[i] > main_sz) { main_sz = sizes[i]; main_id = i; }
    for (int i = 0; i < static_cast<int>(sizes.size()); ++i)
        if (i != main_id) out.largest_secondary = std::max(out.largest_secondary, sizes[i]);
    for (int i = 0; i < static_cast<int>(sizes.size()); ++i)
    {
        if (i == main_id || sizes[i] < 30 || sizes[i] > 1500) continue;
        sea s; s.size = sizes[i]; s.strait = false;
        for (int t = 0; t < static_cast<int>(comp.size()); ++t)
            if (comp[t] == i) { s.tile = t; break; }
        out.seas.push_back(s);
    }

    // Strait-connected seas: erode narrow ocean (>= 4 land neighbours), relabel,
    // and report main-ocean fragments that split off in the size band.
    std::vector<char> eroded = ocean;
    for (int t = 0; t < static_cast<int>(ocean.size()); ++t)
    {
        if (!ocean[t]) continue;
        int nc[6], nr[6], land = 0;
        neighbours(t % gw, t / gw, nc, nr);
        for (int k = 0; k < 6; ++k)
        {
            if (nr[k] < 0 || nr[k] >= gh) { ++land; continue; }
            if (!ocean[static_cast<std::size_t>(nr[k]) * gw + nc[k]]) ++land;
        }
        if (land >= 4) eroded[static_cast<std::size_t>(t)] = 0;
    }
    std::vector<int> esizes;
    const std::vector<int> ecomp = label(eroded, esizes);
    int emain = -1, emain_sz = 0;
    for (int i = 0; i < static_cast<int>(esizes.size()); ++i)
        if (esizes[i] > emain_sz) { emain_sz = esizes[i]; emain = i; }
    for (int i = 0; i < static_cast<int>(esizes.size()); ++i)
    {
        if (i == emain || esizes[i] < 30 || esizes[i] > 1500) continue;
        // Only count fragments that were part of the MAIN ocean before erosion
        // (otherwise they are the enclosed seas already counted above).
        int first = -1;
        for (int t = 0; t < static_cast<int>(ecomp.size()); ++t)
            if (ecomp[t] == i) { first = t; break; }
        if (first < 0 || comp[static_cast<std::size_t>(first)] != main_id) continue;
        sea s; s.size = esizes[i]; s.tile = first; s.strait = true;
        out.seas.push_back(s);
    }

    if (keep_map) out.ocean = ocean;
    return out;
}

void dump_map(const seed_result& r)
{
    std::printf("\n--- seed %u  ocean %.1f%%  seas:", r.seed, r.ocean_frac * 100.0f);
    for (const sea& s : r.seas)
        std::printf(" %d%s@(%d,%d)", s.size, s.strait ? "s" : "e", s.tile % gw, s.tile / gw);
    std::printf(" ---\n");
    for (int row = 0; row < gh; ++row)
    {
        char line[gw + 1];
        for (int col = 0; col < gw; ++col)
            line[col] = r.ocean[static_cast<std::size_t>(row) * gw + col] ? '~' : '#';
        line[gw] = '\0';
        std::printf("%s\n", line);
    }
}

} // namespace

int main(int argc, char** argv)
{
    const int n = (argc > 1) ? std::atoi(argv[1]) : 200;
    const int dump_top = (argc > 2) ? std::atoi(argv[2]) : 3;

    std::vector<seed_result> all;
    int with_enclosed = 0, with_strait = 0, with_any = 0;
    for (int s = 0; s < n; ++s)
    {
        seed_result r = sweep_one(static_cast<uint32_t>(s), false);
        bool enc = false, str = false;
        for (const sea& x : r.seas) (x.strait ? str : enc) = true;
        with_enclosed += enc; with_strait += str; with_any += (enc || str);
        all.push_back(std::move(r));
    }

    std::printf("mediterranean_sweep over %d campaign seeds (Kepler, %dx%d)\n", n, gw, gh);
    std::printf("  enclosed sea (30-1500 tiles, landlocked): %d/%d seeds (%.1f%%)\n",
        with_enclosed, n, 100.0f * with_enclosed / n);
    std::printf("  strait sea (main-ocean arm behind a chokepoint): %d/%d seeds (%.1f%%)\n",
        with_strait, n, 100.0f * with_strait / n);
    std::printf("  any mediterranean-like structure: %d/%d seeds (%.1f%%)\n",
        with_any, n, 100.0f * with_any / n);

    // Diagnosis: does the rift-basin mechanism fire, and does firing deliver?
    {
        int stamped = 0, stamped_300 = 0, unstamped_300 = 0, huge = 0;
        for (const seed_result& r : all)
        {
            if (r.basin_stamped) { ++stamped; if (r.largest_secondary >= 300) ++stamped_300; }
            else if (r.largest_secondary >= 300) ++unstamped_300;
            if (r.largest_secondary > 1500) ++huge;
        }
        std::printf("  basin stamped (continental divergent pair existed): %d/%d (%.1f%%)\n",
            stamped, n, 100.0f * stamped / n);
        if (stamped)
            std::printf("    of stamped: largest secondary >= 300: %d/%d (%.1f%%)\n",
                stamped_300, stamped, 100.0f * stamped_300 / stamped);
        if (n - stamped)
            std::printf("    of unstamped: largest secondary >= 300: %d/%d (%.1f%%)\n",
                unstamped_300, n - stamped, 100.0f * unstamped_300 / (n - stamped));
        std::printf("  secondary component > 1500 tiles (over the sea cap): %d/%d (%.1f%%)\n",
            huge, n, 100.0f * huge / n);
        int floor30 = 0, arena300 = 0;
        for (const seed_result& r : all)
        {
            if (r.largest_secondary >= 30)  ++floor30;
            if (r.largest_secondary >= 300) ++arena300;
        }
        std::printf("  GATE METRICS (uncapped): floor >= 30: %d/%d (%.1f%%)   arena >= 300: %d/%d (%.1f%%)\n",
            floor30, n, 100.0f * floor30 / n, arena300, n, 100.0f * arena300 / n);
    }

    // How BIG is the best sea per seed? A 30-tile pond is not a Mediterranean;
    // Earth's is ~0.5%% of surface (~75 tiles here), but a playable inland sea
    // arena wants a few hundred tiles.
    static const int thresholds[] = { 75, 150, 300, 600, 1000 };
    for (const int th : thresholds)
    {
        int hit = 0;
        for (const seed_result& r : all)
            for (const sea& s : r.seas)
                if (s.size >= th) { ++hit; break; }
        std::printf("  best sea >= %4d tiles: %d/%d seeds (%.1f%%)\n",
            th, hit, n, 100.0f * hit / n);
    }

    // Regression bars (BL-276 R3). The arena band is asserted WIDE of the
    // 88-92% tuning target so seed-set jitter at small n does not flake CTest;
    // a breach here means the mechanism or the gate genuinely moved.
    {
        int floor30 = 0, arena300 = 0;
        for (const seed_result& r : all)
        {
            if (r.largest_secondary >= 30)  ++floor30;
            if (r.largest_secondary >= 300) ++arena300;
        }
        const float floor_pct = 100.0f * floor30 / n;
        const float arena_pct = 100.0f * arena300 / n;
        char what[128];
        std::snprintf(what, sizeof what, "R3 floor: enclosed sea >= 30 tiles on >= 97%% of seeds (got %.1f%%)", floor_pct);
        check(floor_pct >= 97.0f, what);
        std::snprintf(what, sizeof what, "R3 arena: enclosed sea >= 300 tiles on 82-96%% of seeds (got %.1f%%)", arena_pct);
        check(arena_pct >= 82.0f && arena_pct <= 96.0f, what);
    }

    // Dump the best candidates: biggest single sea wins.
    std::sort(all.begin(), all.end(), [](const seed_result& a, const seed_result& b) {
        int am = 0, bm = 0;
        for (const sea& s : a.seas) am = std::max(am, s.size);
        for (const sea& s : b.seas) bm = std::max(bm, s.size);
        return am > bm;
    });
    for (int i = 0; i < dump_top && i < static_cast<int>(all.size()); ++i)
    {
        if (all[i].seas.empty()) break;
        dump_map(sweep_one(all[i].seed, true));
    }
    return g_fail;
}
