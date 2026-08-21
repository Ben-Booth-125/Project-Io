#include "tile_generation.hpp"

#include "planetology.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

namespace {

constexpr double k_two_pi = 6.283185307179586;

// Finite-deposit reserve size: resource_remaining is seeded to richness × this
// factor. With extraction at ~base_rate × richness per tick, the reserve lasts on
// the order of (factor / base_rate / workforce) ticks before depletion tapers in
// (see economy_system.cpp). A hard-coded sensible estimate, iterated by playtest.
constexpr float deposit_reserve_factor = 400.0f;

// ---------------------------------------------------------------------------
// Seedable 3D simplex noise
//
// A minimal single-file port of Stefan Gustavson's public-domain simplex noise.
// 3D is used so the heightmap can be sampled on a cylinder — wrapping the column
// axis around a circle — which makes the surface seamless across the horizontal
// wrap with no antimeridian seam. The permutation table is derived from the body
// seed, so each body gets an independent but reproducible field.
// ---------------------------------------------------------------------------

struct simplex_noise
{
    std::array<uint8_t, 512> perm{};

    explicit simplex_noise(uint32_t seed)
    {
        std::array<uint8_t, 256> p{};
        for (int i = 0; i < 256; ++i)
            p[i] = static_cast<uint8_t>(i);

        // Fisher–Yates shuffle of 0..255 keyed by the body seed.
        std::mt19937 rng(seed);
        for (int i = 255; i > 0; --i)
        {
            std::uniform_int_distribution<int> pick(0, i);
            std::swap(p[i], p[static_cast<std::size_t>(pick(rng))]);
        }
        for (int i = 0; i < 512; ++i)
            perm[i] = p[i & 255];
    }

    static int fast_floor(float v)
    {
        const int i = static_cast<int>(v);
        return (v < static_cast<float>(i)) ? i - 1 : i;
    }

    static float grad(int hash, float x, float y, float z)
    {
        const int   h = hash & 15;
        const float u = h < 8 ? x : y;
        const float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
        return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
    }

    /// 3D simplex noise in approximately [-1, 1].
    float noise(float x, float y, float z) const
    {
        constexpr float f3 = 1.0f / 3.0f;
        constexpr float g3 = 1.0f / 6.0f;

        const float s  = (x + y + z) * f3;
        const int   i  = fast_floor(x + s);
        const int   j  = fast_floor(y + s);
        const int   k  = fast_floor(z + s);
        const float t  = static_cast<float>(i + j + k) * g3;
        const float x0 = x - (static_cast<float>(i) - t);
        const float y0 = y - (static_cast<float>(j) - t);
        const float z0 = z - (static_cast<float>(k) - t);

        int i1, j1, k1, i2, j2, k2;
        if (x0 >= y0)
        {
            if (y0 >= z0)      { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
            else if (x0 >= z0) { i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1; }
            else               { i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1; }
        }
        else
        {
            if (y0 < z0)       { i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1; }
            else if (x0 < z0)  { i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1; }
            else               { i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0; }
        }

        const float x1 = x0 - static_cast<float>(i1) + g3;
        const float y1 = y0 - static_cast<float>(j1) + g3;
        const float z1 = z0 - static_cast<float>(k1) + g3;
        const float x2 = x0 - static_cast<float>(i2) + 2.0f * g3;
        const float y2 = y0 - static_cast<float>(j2) + 2.0f * g3;
        const float z2 = z0 - static_cast<float>(k2) + 2.0f * g3;
        const float x3 = x0 - 1.0f + 3.0f * g3;
        const float y3 = y0 - 1.0f + 3.0f * g3;
        const float z3 = z0 - 1.0f + 3.0f * g3;

        const int ii = i & 255;
        const int jj = j & 255;
        const int kk = k & 255;

        auto corner = [&](float xc, float yc, float zc, int gi) -> float {
            float tt = 0.6f - xc * xc - yc * yc - zc * zc;
            if (tt < 0.0f)
                return 0.0f;
            tt *= tt;
            return tt * tt * grad(gi, xc, yc, zc);
        };

        const float n0 = corner(x0, y0, z0, perm[ii      + perm[jj      + perm[kk]]]);
        const float n1 = corner(x1, y1, z1, perm[ii + i1 + perm[jj + j1 + perm[kk + k1]]]);
        const float n2 = corner(x2, y2, z2, perm[ii + i2 + perm[jj + j2 + perm[kk + k2]]]);
        const float n3 = corner(x3, y3, z3, perm[ii + 1  + perm[jj + 1  + perm[kk + 1]]]);

        return 32.0f * (n0 + n1 + n2 + n3);
    }
};

/// Fractal-Brownian-motion sample on a cylinder, so the column axis wraps cleanly.
///
/// `base_cycles` is the number of full noise cycles around the equator at the
/// lowest octave; it is resolution-independent, so the same value yields the same
/// feature *scale relative to the globe* regardless of grid width. The row axis is
/// scaled to match the column arc length, keeping features isotropic.
float fbm_cylinder(const simplex_noise& s, int col, int row, int gw,
                   float base_cycles, int octaves)
{
    constexpr float lacunarity = 2.0f;
    constexpr float gain       = 0.5f;

    const double theta = k_two_pi * static_cast<double>(col) / static_cast<double>(gw);
    const double ct    = std::cos(theta);
    const double st    = std::sin(theta);

    double amp = 1.0, freq = 1.0, sum = 0.0, norm = 0.0;
    for (int o = 0; o < octaves; ++o)
    {
        const double cycles = static_cast<double>(base_cycles) * freq;
        const double radius = cycles / k_two_pi;
        const double npt    = cycles / static_cast<double>(gw); // noise units per tile (row)
        const double nx     = radius * ct;
        const double ny     = radius * st;
        const double nz     = static_cast<double>(row) * npt;
        sum  += amp * static_cast<double>(s.noise(static_cast<float>(nx),
                                                  static_cast<float>(ny),
                                                  static_cast<float>(nz)));
        norm += amp;
        amp  *= gain;
        freq *= lacunarity;
    }
    return static_cast<float>(sum / norm);
}

/// Normalise a field in place to [0, 1] by remapping its observed min/max.
void normalise(std::vector<float>& field)
{
    float lo = field.empty() ? 0.0f : field[0];
    float hi = lo;
    for (float v : field) { lo = std::min(lo, v); hi = std::max(hi, v); }
    const float inv = (hi > lo) ? 1.0f / (hi - lo) : 1.0f;
    for (float& v : field) v = (v - lo) * inv;
}

// ---------------------------------------------------------------------------
// Hex grid helpers
// ---------------------------------------------------------------------------

// The 6 hex neighbours of (col, row) in odd-r offset coordinates. Column wraps at
// gw; rows outside [0, gh) are omitted (the poles are open edges).
void hex_neighbours(int col, int row, int gw, int gh,
                    std::pair<int, int> out[6], int& count)
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
        if (nr < 0 || nr >= gh)
            continue;
        const int nc = ((col + dc[i]) % gw + gw) % gw;
        out[count++] = { nc, nr };
    }
}

// ---------------------------------------------------------------------------
// Pass 4e (BL-516) — WATER KINDS. Lake, coast and ocean, decided structurally
// ---------------------------------------------------------------------------
// Runs on the finished water mask and NOTHING ELSE: no RNG stream is touched,
// no threshold is invented, and the answer is a pure function of which tiles
// are water and how they are joined. Three rules, in order:
//
//   1. Flood-fill the water into connected components on the same odd-r hex
//      adjacency (columns wrap) every other body-grid pass walks.
//   2. THE SEA is the LARGEST component; ties break on the lowest tile index,
//      so no container or scan order reaches the decision. Every other
//      component is a LAKE — "does not reach the sea" is the whole definition,
//      and it needs no size cut-off.
//   3. Within the sea: a tile with at least one LAND neighbour is COAST (the
//      shoreline ring); a tile with none is OPEN OCEAN.
//
// A body with no water leaves the output untouched. A body that is ALL water
// has one component, no land neighbours anywhere, and is therefore all ocean —
// which is the right answer rather than a special case.
//
// IT SITS AFTER PASS 4D, and it holds 4c/4d's own contract: no RNG stream is
// touched, so every downstream draw is the draw it was before. It goes further
// and writes to a SEPARATE array (`reported_sub`) rather than refining `sub[]`
// in place, so Passes 5 and 6 — clusters, deposits, derived environment — are
// still handed the coarse `ocean` they were written against. Only what the tile
// REPORTS about its water changed, which is what makes the generated surface
// bit-identical to the pre-BL-516 build rather than merely intended to be.
void classify_water_kinds(const std::vector<bool>& is_ocean, int gw, int gh,
                          std::vector<terrain_substrate>& reported_sub)
{
    const int total = gw * gh;
    if (total <= 0)
        return;

    std::vector<int> component(static_cast<std::size_t>(total), -1);
    std::vector<int> sizes;
    std::vector<int> stack;

    for (int start = 0; start < total; ++start)
    {
        if (!is_ocean[static_cast<std::size_t>(start)]
            || component[static_cast<std::size_t>(start)] >= 0)
            continue;

        const int id = static_cast<int>(sizes.size());
        int       n  = 0;
        stack.clear();
        stack.push_back(start);
        component[static_cast<std::size_t>(start)] = id;
        while (!stack.empty())
        {
            const int cur = stack.back();
            stack.pop_back();
            ++n;
            const int col = cur % gw;
            const int row = cur / gw;
            std::pair<int, int> nbrs[6];
            int                 cnt = 0;
            hex_neighbours(col, row, gw, gh, nbrs, cnt);
            for (int k = 0; k < cnt; ++k)
            {
                const int nidx = nbrs[k].first + nbrs[k].second * gw;
                if (!is_ocean[static_cast<std::size_t>(nidx)]
                    || component[static_cast<std::size_t>(nidx)] >= 0)
                    continue;
                component[static_cast<std::size_t>(nidx)] = id;
                stack.push_back(nidx);
            }
        }
        sizes.push_back(n);
    }

    if (sizes.empty())
        return; // a dry body

    // The sea: largest component, lowest id on a tie. Component ids are handed
    // out in ascending start-tile order, so "lowest id" IS "lowest tile index".
    int sea = 0;
    for (int i = 1; i < static_cast<int>(sizes.size()); ++i)
        if (sizes[static_cast<std::size_t>(i)] > sizes[static_cast<std::size_t>(sea)])
            sea = i;

    for (int idx = 0; idx < total; ++idx)
    {
        if (!is_ocean[static_cast<std::size_t>(idx)])
            continue;
        if (component[static_cast<std::size_t>(idx)] != sea)
        {
            reported_sub[static_cast<std::size_t>(idx)] = terrain_substrate::lake;
            continue;
        }
        const int col = idx % gw;
        const int row = idx / gw;
        std::pair<int, int> nbrs[6];
        int                 cnt = 0;
        hex_neighbours(col, row, gw, gh, nbrs, cnt);
        bool touches_land = false;
        for (int k = 0; k < cnt && !touches_land; ++k)
            touches_land = !is_ocean[static_cast<std::size_t>(nbrs[k].first + nbrs[k].second * gw)];
        reported_sub[static_cast<std::size_t>(idx)] =
            touches_land ? terrain_substrate::coast : terrain_substrate::ocean;
    }
}

// ---------------------------------------------------------------------------
// Pass 3 — latitude bands
// ---------------------------------------------------------------------------

enum class lat_band : uint8_t { polar, subpolar, temperate, subtropical, tropical };

// Latitude band for a row, with boundaries shifted by temperature class. `d` is
// distance from the equator in [0, 1] (0 at the equator, 1 at a pole). Boundaries
// follow the row-percent table in TILE_GENERATION.md § Pass 3.
lat_band band_for_row(int row, int gh, temperature_class temp)
{
    const double p = (gh > 1) ? static_cast<double>(row) / static_cast<double>(gh - 1) : 0.5;
    const double d = std::abs(p - 0.5) * 2.0;

    switch (temp)
    {
        case temperature_class::frozen:
            return lat_band::polar;

        case temperature_class::cold:
            // Tuned 2026-06-14: polar boundary tightened from d >= 0.50 to
            // d >= 0.70 (outer 30% of rows instead of outer 50%). At 0.50 the
            // polar band covered half the grid and every polar-band row on a
            // polar_frozen body (Selene) received icy composition, yielding ~52%
            // icy coverage. At 0.70 the cap should fall to roughly ~30%, which
            // reads as a credible but not dominant polar feature. The subpolar
            // boundary is also shifted out (0.30 → 0.10 retained) to keep the
            // subpolar belt proportionally sized.
            if (d >= 0.70) return lat_band::polar;
            if (d >= 0.30) return lat_band::subpolar;
            return lat_band::temperate;

        case temperature_class::scorching:
        case temperature_class::hot:
            if (d >= 0.60) return lat_band::temperate;
            if (d >= 0.20) return lat_band::subtropical;
            return lat_band::tropical;

        case temperature_class::temperate:
        default:
            if (d >= 0.80) return lat_band::polar;
            if (d >= 0.56) return lat_band::subpolar;
            if (d >= 0.16) return lat_band::temperate;
            if (d >= 0.06) return lat_band::subtropical;
            return lat_band::tropical;
    }
}

// ---------------------------------------------------------------------------
// Pass 4 — biome, then the two axes it decomposes into (BL-519)
// ---------------------------------------------------------------------------
//
// BL-519 split `terrain_composition` into `terrain_substrate` × `terrain_cover`.
// This pass is where the split is PRODUCED, and its structure is chosen to make
// the change auditable rather than merely correct:
//
//   4a  BIOME. The (band, moisture) tables below, UNCHANGED in their values and
//       — critically — in their RNG consumption, draw for draw. They now return
//       an internal `biome` rather than a `terrain_composition`, which is the
//       same twelve-valued vocabulary under a name that admits it was never one
//       axis. Keeping this stream bit-identical is what lets every downstream
//       pass (deposits, environment, clusters, nations, provinces) reproduce
//       exactly, so anything that DID move is attributable to the new axis
//       rather than to stream drift.
//
//   4b  DRAINAGE (BL-338, pre-existing). Still operates on the biome, so it is
//       unchanged: it converts lowland grassland/forest to wetland before the
//       axes are derived, which is the same decision it always made.
//
//   4c  DECOMPOSE. `decompose_biome` maps each biome to (substrate, cover,
//       density). It consumes NO RNG — density varies per tile through a
//       stateless fold of (seed, index, moisture), the same idiom province.cpp
//       uses for its edge jitter. A graded cover (Ben's call, 2026-08-21) that
//       cost a new random stream would have perturbed every later pass for a
//       cosmetic gain; a fold costs nothing and is just as varied.
//
//   4d  REFINE. `refine_cover` is the only genuinely NEW terrain behaviour, and
//       it is what Ben's brief asked for: "a mountain might have a forest or
//       not". It may add cover to ground the biome table left bare — a forest on
//       a wet rocky upland, snow on cold high ground, dunes on dry barren — and
//       it NEVER touches the substrate. Also stateless; also no stream.
//
// The invariant that makes 4d safe: it only writes where `cover == none`. A tile
// the biome table gave a cover to keeps it.
// ---------------------------------------------------------------------------

// The twelve-value vocabulary the (band, moisture) tables speak. Internal to this
// pass: it is what the world model USED to store, and it survives only as the
// intermediate the tables produce before decomposition. Nothing outside this file
// sees it.
enum class biome : uint8_t
{
    barren, rocky, volcanic, icy, tundra, grassland,
    forest, wetland, ocean, regolith, metallic
};


// Tuned 2026-06-14 (Kepler biome balance): the high-moisture cutoff was lowered
// from 0.65 to 0.55 so more tiles reach the wet (mc==2) branches of
// biome_atmospheric() that produce forest and wetland — these stayed sparse
// (~1% / ~0.5%) on Kepler even after the Pass 2 ocean-bias reduction.
int moisture_column(float m) { return m < 0.35f ? 0 : (m < 0.55f ? 1 : 2); }

// 60/40 weighting toward `a`, resolved against the per-tile RNG.
biome pick_60_40(std::mt19937& rng, biome a, biome b)
{
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    return (u(rng) < 0.6f) ? a : b;
}

biome pick_weighted2(std::mt19937& rng,
                     biome a, int wa,
                     biome b, int wb)
{
    std::uniform_int_distribution<int> d(0, wa + wb - 1);
    return (d(rng) < wa) ? a : b;
}

biome pick_weighted3(std::mt19937& rng,
                     biome a, int wa,
                     biome b, int wb,
                     biome c, int wc)
{
    std::uniform_int_distribution<int> d(0, wa + wb + wc - 1);
    const int r = d(rng);
    if (r < wa)      return a;
    if (r < wa + wb) return b;
    return c;
}

// The abiotic partner of the same (band, moisture) cell, for a world that HELD an
// atmosphere but whose biosphere never reached land (BL-167).
//
// Deliberately not a substitution table applied after the fact: tundra's abiotic
// partner is rocky, not icy, so a blanket "replace tundra with icy" would repaint
// every subpolar band. Each cell falls back to its own inorganic member instead —
// rocky in the cool bands, barren in the warm ones.
//
// Mirrors biome_atmospheric's RNG consumption draw-for-draw, so the two
// branches stay stream-aligned and switching between them cannot shift any
// downstream pass.
biome biome_abiotic(lat_band band, float moisture, std::mt19937& rng)
{
    using tc = biome;
    const int mc = moisture_column(moisture);
    switch (band)
    {
        case lat_band::polar:
            return tc::icy;
        case lat_band::subpolar:
            if (mc == 0) return tc::rocky;
            if (mc == 1) return pick_60_40(rng, tc::rocky, tc::rocky);
            return tc::rocky;
        case lat_band::temperate:
            if (mc == 0) return pick_60_40(rng, tc::barren, tc::rocky);
            if (mc == 1) return pick_60_40(rng, tc::rocky, tc::barren);
            return pick_60_40(rng, tc::rocky, tc::barren);
        case lat_band::subtropical:
            if (mc == 0) return tc::barren;
            if (mc == 1) return pick_60_40(rng, tc::barren, tc::rocky);
            return pick_60_40(rng, tc::barren, tc::rocky);
        case lat_band::tropical:
            if (mc == 0) return tc::barren;
            if (mc == 1) return tc::barren;
            return pick_60_40(rng, tc::barren, tc::rocky);
    }
    return tc::barren;
}

// Atmosphere-present composition from the (band, moisture) table. Only reached on
// bodies that support biology (atmosphere moderate or thick).
biome biome_atmospheric(lat_band band, float moisture, std::mt19937& rng)
{
    using tc = biome;
    const int mc = moisture_column(moisture);
    switch (band)
    {
        case lat_band::polar:
            return tc::icy;
        case lat_band::subpolar:
            if (mc == 0) return tc::rocky;
            if (mc == 1) return pick_60_40(rng, tc::rocky, tc::tundra);
            return tc::tundra;
        case lat_band::temperate:
            if (mc == 0) return pick_60_40(rng, tc::barren, tc::rocky);
            if (mc == 1) return pick_60_40(rng, tc::rocky, tc::grassland);
            return pick_60_40(rng, tc::grassland, tc::forest);
        case lat_band::subtropical:
            if (mc == 0) return tc::barren;
            if (mc == 1) return pick_60_40(rng, tc::barren, tc::grassland);
            return pick_60_40(rng, tc::grassland, tc::wetland);
        case lat_band::tropical:
            if (mc == 0) return tc::barren;
            if (mc == 1) return tc::barren;
            return pick_60_40(rng, tc::forest, tc::wetland);
    }
    return tc::barren;
}

// ---------------------------------------------------------------------------
// Pass 4c/4d — the axes (BL-519)
// ---------------------------------------------------------------------------

/// One tile's terrain on the three axes, as this pass produces it.
struct tile_axes
{
    terrain_substrate substrate = terrain_substrate::barren;
    terrain_cover     cover     = terrain_cover::none;
    std::uint8_t      density   = 0;
};

/// Stateless per-tile fold in [0, 1). NO RNG STREAM IS CONSUMED — this is the
/// campaign_battle / province.cpp idiom, and using it here is what lets a graded
/// cover and a whole new cover pass land WITHOUT perturbing a single downstream
/// draw. `salt` mixes two independent uses of the fold on the same tile apart.
float axis_fold(uint32_t seed, int idx, uint32_t salt)
{
    uint32_t h = seed ^ (static_cast<uint32_t>(idx) * 2654435761u) ^ (salt * 2246822519u);
    h ^= h >> 15; h *= 2246822519u;
    h ^= h >> 13; h *= 3266489917u;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0x1000000u);
}

/// Grade a cover: a centre value for the kind, spread by moisture and the fold.
/// Density is 0 if and only if the cover is `none` — the invariant
/// `tile_axes_harness` asserts.
std::uint8_t grade_cover(terrain_cover c, float moisture, uint32_t seed, int idx)
{
    if (c == terrain_cover::none)
        return 0;

    // Centre and half-spread per cover kind, in density units. Urban is pinned at
    // full: a paved tile is not "sparsely paved", and BL-520 must not draw it as
    // a thinning pattern.
    int centre = 128, spread = 40;
    switch (c)
    {
        case terrain_cover::forest: centre = 205; spread = 45; break;
        case terrain_cover::marsh:  centre = 170; spread = 50; break;
        case terrain_cover::grass:  centre = 150; spread = 55; break;
        case terrain_cover::scrub:  centre =  75; spread = 40; break;
        case terrain_cover::snow:   centre = 160; spread = 60; break;
        case terrain_cover::dunes:  centre = 140; spread = 60; break;
        case terrain_cover::ash:    centre = 110; spread = 50; break;
        case terrain_cover::salt:   centre = 120; spread = 45; break;
        case terrain_cover::urban:  return k_cover_density_max;
        case terrain_cover::none:   return 0;
    }

    // Moisture pushes a biotic cover thicker and a dry cover thinner, so the two
    // families grade in opposite directions off the same field. Half the
    // variation is terrain-driven and half is the fold, which keeps a wet region
    // visibly denser than a dry one instead of dissolving into noise.
    const float wet  = std::clamp(moisture, 0.0f, 1.0f);
    const float lean = is_biotic_cover(c) ? (wet - 0.5f) : (0.5f - wet);
    const float f    = axis_fold(seed, idx, 0x0C0FFEEu) * 2.0f - 1.0f;
    const int   v    = centre + static_cast<int>(static_cast<float>(spread) * (0.5f * f + lean));
    return static_cast<std::uint8_t>(std::clamp(v, 1, static_cast<int>(k_cover_density_max)));
}

/// Pass 4c — map the biome the (band, moisture) table drew onto the two axes.
///
/// This is the whole of the OLD model expressed in the NEW one, and every row is
/// a 1:1 restatement rather than a re-tuning. The four biotic biomes all sit on
/// `sedimentary` — which is the finding the item turned on: grassland, forest,
/// wetland and tundra were never four kinds of GROUND, they were one kind of
/// ground under four kinds of cover, and spending the slot on the cover is what
/// made "a rocky mountain that happens to be forested" inexpressible.
///
/// `sedimentary + scrub` is exactly the old `tundra`, and nothing else produces
/// that pair (refinement only writes where cover is `none`, and sedimentary
/// always carries a cover from this table). Deposit rules that used to key on
/// tundra key on the pair, and stay bit-identical.
tile_axes decompose_biome(biome b, float moisture, uint32_t seed, int idx)
{
    using su = terrain_substrate;
    using cv = terrain_cover;

    tile_axes a;
    switch (b)
    {
        case biome::barren:    a.substrate = su::barren;      a.cover = cv::none;   break;
        case biome::rocky:     a.substrate = su::rocky;       a.cover = cv::none;   break;
        case biome::volcanic:  a.substrate = su::volcanic;    a.cover = cv::none;   break;
        case biome::icy:       a.substrate = su::icy;         a.cover = cv::none;   break;
        case biome::regolith:  a.substrate = su::regolith;    a.cover = cv::none;   break;
        case biome::metallic:  a.substrate = su::metallic;    a.cover = cv::none;   break;
        case biome::ocean:     a.substrate = su::ocean;       a.cover = cv::none;   break;
        case biome::tundra:    a.substrate = su::sedimentary; a.cover = cv::scrub;  break;
        case biome::grassland: a.substrate = su::sedimentary; a.cover = cv::grass;  break;
        case biome::forest:    a.substrate = su::sedimentary; a.cover = cv::forest; break;
        case biome::wetland:   a.substrate = su::sedimentary; a.cover = cv::marsh;  break;
    }
    a.density = grade_cover(a.cover, moisture, seed, idx);
    return a;
}

/// Pass 4d — THE NEW BEHAVIOUR, and the only part of this pass that says
/// something the old model could not.
///
/// Ben's brief: "a mountain might have a forest or not." A mountain WITH a forest
/// was always expressible; a ROCKY mountain that happens to be forested was not,
/// because the single slot had been spent. This pass is where that tile now comes
/// from — it dresses bare ground with what the climate would actually put there,
/// and it never rewrites the geology underneath.
///
/// THE GUARD THAT KEEPS IT HONEST: it writes only where `cover == none`. A tile
/// the biome table already dressed is left exactly as it was, so this pass can
/// add tiles to a cover class but can never take one away — which is why the
/// old biotic distributions are a floor rather than a moving target.
///
/// Stateless throughout: latitude, moisture, the retained height (BL-517) and a
/// fold. No RNG stream, so adding it perturbs nothing downstream.
void refine_cover(tile_axes& a, lat_band band, float moisture, float height,
                  bool biotic, uint32_t seed, int idx)
{
    using su = terrain_substrate;
    using cv = terrain_cover;

    if (a.cover != cv::none || a.substrate == su::ocean)
        return;

    const float f    = axis_fold(seed, idx, 0x5EEDu);
    const bool  cold = band == lat_band::polar || band == lat_band::subpolar;

    // SNOW — lying snow on cold high ground. Reads latitude × BL-517's retained
    // height and needs no generation input of its own, which is the reason it
    // was worth admitting to the roster at all. Checked before the biotic cases:
    // a snowline is above a treeline, not beside it.
    if (cold && height > 0.62f && f < 0.75f)
    {
        a.cover   = cv::snow;
        a.density = grade_cover(cv::snow, moisture, seed, idx);
        return;
    }
    if (a.substrate == su::icy && f < 0.45f)
    {
        a.cover   = cv::snow;
        a.density = grade_cover(cv::snow, moisture, seed, idx);
        return;
    }

    // ASH — volcanic fall. A hazard reading rather than a resource one, so it is
    // common on volcanic ground and appears nowhere else. GATED ON DRYNESS so it
    // does not pre-empt the vegetation case below: wet volcanic ground is some of
    // the most fertile there is, and ash claiming every volcanic tile would have
    // made that unreachable.
    if (a.substrate == su::volcanic && moisture < 0.45f && f < 0.55f)
    {
        a.cover   = cv::ash;
        a.density = grade_cover(cv::ash, moisture, seed, idx);
        return;
    }

    // THE CASE BEN ASKED FOR — vegetation on ground that is not soil. Needs a
    // biosphere (a dead world grows nothing, however wet), and it grades with
    // moisture: woodland where it is wet enough, scrub as the half-step that
    // makes a treeline gradual instead of an edge.
    //
    // THE THRESHOLD IS 0.45, NOT 0.55, AND THAT NUMBER IS THE WHOLE FEATURE.
    // Measured first: at 0.55 this branch fired ZERO times on a homeworld, and
    // the reason is structural rather than a tuning miss. The biome table routes
    // ground by moisture, so `rocky` is only ever drawn in the dry and middling
    // columns (mc 0 and 1, i.e. moisture below 0.55) — wet rocky ground does not
    // exist for the branch to find. Reading the top half of the middling column
    // instead is both reachable and the honest claim: 0.55 is the moisture at
    // which SOIL grows a forest, and a slope is not soil.
    if (biotic && !cold && (a.substrate == su::rocky || a.substrate == su::volcanic))
    {
        if (moisture >= 0.45f && f < 0.35f)
        {
            a.cover   = cv::forest;
            a.density = grade_cover(cv::forest, moisture, seed, idx);
            return;
        }
        if (moisture >= 0.30f && f < 0.50f)
        {
            a.cover   = cv::scrub;
            a.density = grade_cover(cv::scrub, moisture, seed, idx);
            return;
        }
    }
    // Cold rocky ground gets scrub rather than forest — the tundra-as-cover case,
    // reached from the substrate side now that tundra is no longer a terrain of
    // its own (Ben, 2026-08-21).
    if (biotic && cold && a.substrate == su::rocky && moisture >= 0.45f && f < 0.40f)
    {
        a.cover   = cv::scrub;
        a.density = grade_cover(cv::scrub, moisture, seed, idx);
        return;
    }

    // DUNES and SALT — the dry pair on barren ground, and the reason two barren
    // tiles can now read differently. Dunes are wind-blown sand and want the
    // driest ground; salt is a dry-basin crust and wants a LOW dry basin, which
    // is why it reads height as well as moisture.
    if (a.substrate == su::barren && moisture < 0.30f)
    {
        if (height < 0.42f && f > 0.86f)
        {
            a.cover   = cv::salt;
            a.density = grade_cover(cv::salt, moisture, seed, idx);
            return;
        }
        if (f < 0.42f)
        {
            a.cover   = cv::dunes;
            a.density = grade_cover(cv::dunes, moisture, seed, idx);
            return;
        }
    }
    // Everything else stays BARE, and that is a real answer rather than a gap —
    // `none` being first-class is half the point of the axis.
}

float volcanic_probability(geological_activity g)
{
    switch (g)
    {
        case geological_activity::high:     return 0.375f;
        case geological_activity::moderate: return 0.125f;
        case geological_activity::low:      return 0.04f;
        case geological_activity::none:     return 0.0f;
    }
    return 0.0f;
}

// ---------------------------------------------------------------------------
// Pass 5 — landform clusters
// ---------------------------------------------------------------------------

enum class feature_kind : uint8_t { mountain, rift, crater };

struct cluster_shape { int max_ring; float decay; };

cluster_shape shape_of(feature_kind kind)
{
    // Tuned 2026-06-14: increased max_ring and decay to make landform clusters
    // more prominent. Old values: mountain {2, 0.55}, rift {2, 0.55},
    // crater {1, 0.45}. Mountains and rifts now reach a 4th ring (0-indexed),
    // decay raised so the BFS frontier survives further from the seed. Craters
    // gain a second ring so they read as actual impact features rather than
    // single-tile stamps. Conservative step — integrator should histogram and
    // adjust further if coverage is still too sparse or overshoots plains.
    switch (kind)
    {
        // Mountains reach further than the other two (2026-08-04, T3 calibration).
        // The tile census measured relief — mountain plus highland — at 6.8% of
        // land against Earth's ~24% mountainous land, i.e. roughly 3.5x short.
        // Grown rather than seeded more thickly: Earth's relief is a few long
        // chains (Andes, Himalaya, Rockies), not many small blobs, so a larger
        // cluster is the more Earth-like lever than a higher seed count.
        case feature_kind::mountain: return { 5, 0.72f };
        case feature_kind::rift:     return { 3, 0.60f };
        case feature_kind::crater:   return { 2, 0.55f };
    }
    return { 1, 0.5f };
}

// Landform assigned to a cluster tile at ring distance `ring` from the seed. The
// frontier transitions follow TILE_GENERATION.md § Pass 5.
terrain_landform landform_at_ring(feature_kind kind, int ring, std::mt19937& rng)
{
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    switch (kind)
    {
        // A range should read as peaks with shoulders, not one peak with a wide
        // plain around it. Ring 1 used to be 70% highland and rings 2+ half
        // plains, so a cluster contributed barely more than a single mountain
        // tile — the reason the census measured mountain at 1.0% of land. Ring 1
        // is now mountain-dominant, ring 2 mixes, and the outer rings stay
        // highland rather than dissolving straight back into plains.
        case feature_kind::mountain:
            if (ring == 0) return terrain_landform::mountain;
            if (ring == 1) return (u(rng) < 0.55f) ? terrain_landform::mountain : terrain_landform::highland;
            if (ring == 2) return (u(rng) < 0.25f) ? terrain_landform::mountain : terrain_landform::highland;
            return (u(rng) < 0.65f) ? terrain_landform::highland : terrain_landform::plains;
        case feature_kind::rift:
            if (ring == 0) return terrain_landform::rift;
            if (ring == 1) return (u(rng) < 0.6f) ? terrain_landform::canyon : terrain_landform::rift;
            return (u(rng) < 0.3f) ? terrain_landform::canyon : terrain_landform::plains;
        case feature_kind::crater:
            if (ring == 0) return terrain_landform::crater;
            if (ring == 1) return (u(rng) < 0.5f) ? terrain_landform::crater : terrain_landform::highland;
            return terrain_landform::plains;
    }
    return terrain_landform::plains;
}

// Grow one cluster outward from a seed by ring-layered BFS, claiming land tiles
// and stamping their landform. Ocean (already claimed) tiles block growth.
void grow_cluster(feature_kind kind, int seed_col, int seed_row, int gw, int gh,
                  std::vector<terrain_landform>& land, std::vector<bool>& claimed,
                  std::mt19937& rng)
{
    const cluster_shape shape = shape_of(kind);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);

    std::vector<std::pair<int, int>> frontier{ { seed_col, seed_row } };
    for (int ring = 0; ring <= shape.max_ring && !frontier.empty(); ++ring)
    {
        std::vector<std::pair<int, int>> next;
        for (const auto& [col, row] : frontier)
        {
            const int idx = col + row * gw;
            if (claimed[idx])
                continue;
            claimed[idx] = true;
            land[idx]    = landform_at_ring(kind, ring, rng);

            if (ring == shape.max_ring)
                continue;

            std::pair<int, int> nbrs[6];
            int n = 0;
            hex_neighbours(col, row, gw, gh, nbrs, n);
            for (int i = 0; i < n; ++i)
            {
                const int nidx = nbrs[i].first + nbrs[i].second * gw;
                if (claimed[nidx])
                    continue;
                // Rifts grow weakly east–west to read as linear features: the two
                // purely vertical neighbours (same column) are damped.
                float p = shape.decay;
                if (kind == feature_kind::rift && nbrs[i].first == col)
                    p *= 0.5f;
                if (u(rng) < p)
                    next.push_back(nbrs[i]);
            }
        }
        frontier.swap(next);
    }
}

// Pick up to `n_seeds` cluster seeds preferring `kind`-appropriate tiles, falling
// back to any non-ocean tile when the preferred pool is too small.
std::vector<int> pick_seeds(feature_kind kind, int n_seeds, int gw, int gh,
                            const std::vector<float>& height,
                            const std::vector<terrain_substrate>& sub,
                            const std::vector<lat_band>& band,
                            const std::vector<bool>& is_ocean,
                            std::mt19937& rng,
                            const std::vector<uint8_t>* convergent)
{
    using su = terrain_substrate;
    std::vector<int> preferred;
    std::vector<int> any_land;
    // Tiles on a classified convergent plate boundary — where mountains actually
    // form. Tried BEFORE the height/substrate rule for mountains, so a range
    // follows the collision that raised it. Seeding on "already high and rocky"
    // instead is what made ranges pool into blobs: every extra seed landed in
    // the same uplands and overlapped a range already there, which is why raising
    // the seed count returned sublinearly (1.5x seeds bought 1.13x relief).
    std::vector<int> on_boundary;
    const bool have_boundary = (kind == feature_kind::mountain)
                               && convergent != nullptr
                               && convergent->size() == static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh);
    const int total = gw * gh;
    for (int idx = 0; idx < total; ++idx)
    {
        if (is_ocean[idx])
            continue;
        any_land.push_back(idx);
        if (have_boundary && (*convergent)[static_cast<std::size_t>(idx)] != 0u)
            on_boundary.push_back(idx);

        bool ok = false;
        switch (kind)
        {
            case feature_kind::mountain:
                // Landform clustering reads the SUBSTRATE alone (BL-519): a range
                // is raised by geology, and whether a forest happens to be growing
                // on it is not a fact about where mountains form.
                ok = height[idx] > 0.65f && (sub[idx] == su::rocky || sub[idx] == su::barren);
                break;
            case feature_kind::rift:
                ok = (sub[idx] == su::volcanic || sub[idx] == su::barren)
                     && (band[idx] == lat_band::subtropical || band[idx] == lat_band::tropical);
                break;
            case feature_kind::crater:
                ok = (sub[idx] == su::regolith || sub[idx] == su::barren);
                break;
        }
        if (ok)
            preferred.push_back(idx);
    }

    // Three tiers, most-meaningful first: the collision zone, then merely-high
    // rocky ground, then anywhere on land. A body with no classified convergent
    // boundary (stagnant lid, or a draw where no pair closed) simply falls
    // through to the old behaviour.
    std::vector<int>& pool =
        (static_cast<int>(on_boundary.size()) >= n_seeds)  ? on_boundary
      : (static_cast<int>(preferred.size()) >= n_seeds)    ? preferred
                                                           : any_land;
    std::shuffle(pool.begin(), pool.end(), rng);

    std::vector<int> seeds;
    const int take = std::min(n_seeds, static_cast<int>(pool.size()));
    seeds.assign(pool.begin(), pool.begin() + take);
    return seeds;
}

// The design tables give seed counts for a reference-scale globe; taken as
// absolutes they collapse to near-zero coverage on the prototype's large grids.
// Scale a base count up with grid area so feature *density* stays consistent
// across body sizes, never below the authored count (so small bodies are
// unaffected).
//
// Tuned 2026-06-14: reference_tiles lowered from 1800 to 1200 to yield a
// higher seed count on the 180×84 prototype grids (~15k tiles). Old reference
// gave a ~8.4× scale factor; new gives ~12.6×. Pairs with the larger
// shape_of() clusters to produce more prominent and more numerous features.
// Integrator should re-histogram and back this off if density overshoots.
int scale_to_area(int base, int total_tiles)
{
    if (base <= 0)
        return base;
    constexpr int reference_tiles = 1200;
    const long scaled = std::lround(static_cast<double>(base)
                                    * static_cast<double>(total_tiles)
                                    / static_cast<double>(reference_tiles));
    return std::max(base, static_cast<int>(scaled));
}

// ---------------------------------------------------------------------------
// ore fields (PLANETOLOGY.md § Open calls 4)
// ---------------------------------------------------------------------------
// "A body-level 1.4x copper smears evenly and reads as noise. Real ore is
// region-scale — the Hamersley is ONE basin, a handful of tiles at any
// resolution."
//
// The planetology endowment says how much of a resource a world's HISTORY
// produced; it says nothing about where. Applied as a flat multiplier it makes
// every eligible tile equally rich, so a biosphere history that the model
// computes in real detail arrives on the map as uniform grey. A region field
// puts it somewhere: the oil in the basin that was an anoxic sea, the porphyry
// copper along the boundary that was subducting.
//
// CONCENTRATE, DO NOT INFLATE. Each field is normalised to mean 1.0 over the
// tiles that bear the resource, so a region moves ore around the body without
// changing how much of it the world has. The endowment stays the sole authority
// on quantity; regions are purely a redistribution. That keeps this pass
// orthogonal to S8 and to BL-114's deposit_scalar, both of which own magnitude.
struct ore_field { int centre; float radius; };

// Where each region-forming resource actually forms. Anything not listed here
// keeps the flat endowment — a region model is only honest for resources with
// a real concentrating mechanism.
std::vector<ore_field> ore_fields_for(resource_type res, int n, int gw, int gh,
                                        const std::vector<float>& height,
                                        const std::vector<bool>& is_ocean,
                                        const std::vector<terrain_cover>& cov,
                                        const std::vector<uint8_t>* convergent,
                                        std::mt19937& rng)
{
    using cv = terrain_cover;
    const int total = gw * gh;
    const bool have_conv = convergent != nullptr
                           && convergent->size() == static_cast<std::size_t>(total);

    // "Low-lying" has to be a percentile of the LAND range, not an absolute
    // height. Pass 2 puts the ocean threshold at the water_fraction percentile of
    // the height field, so EVERY land tile sits above it — on a 55%-ocean world
    // an absolute cutoff like 0.45 selects nothing at all. That was exactly the
    // bug the regions-off comparison caught: petroleum and iron concentration
    // came back bit-identical because no region ever formed.
    std::vector<float> land_h;
    land_h.reserve(static_cast<std::size_t>(total));
    for (int idx = 0; idx < total; ++idx)
        if (!is_ocean[idx]) land_h.push_back(height[idx]);
    if (land_h.empty()) return {};
    std::sort(land_h.begin(), land_h.end());
    const auto land_pct = [&](float q) {
        const std::size_t k = static_cast<std::size_t>(q * static_cast<float>(land_h.size() - 1));
        return land_h[k];
    };
    const float marine_cut = land_pct(0.40f); // lowest 40% of land: old shelf and basin
    const float swamp_cut  = land_pct(0.55f); // a little higher: the coal measures

    std::vector<int> cand;
    for (int idx = 0; idx < total; ++idx)
    {
        if (is_ocean[idx]) continue;
        bool ok = false;
        switch (res)
        {
            // Porphyry sits over a subduction arc. S8 already says so in prose
            // ("porphyry needs sustained subduction"); this is the same claim
            // made spatial, using the boundary mask the continents pass records.
            case resource_type::copper_ore:
                ok = have_conv && (*convergent)[static_cast<std::size_t>(idx)] != 0u;
                break;
            // Oil and banded iron are MARINE legacies: they want ground that sat
            // low — a shelf or an epicontinental basin — not today's uplands.
            case resource_type::petroleum:
            case resource_type::iron_ore:
                ok = height[idx] <= marine_cut;
                break;
            // Coal wants the swamp: low, wet, vegetated ground.
            case resource_type::coal:
                // "Vegetated" is a claim about the COVER (BL-519), and saying so
                // is what the axis split buys here: the old list named three
                // compositions to mean one thing. Scrub is excluded deliberately —
                // sparse woody cover is not the swamp that lays down a seam.
                ok = height[idx] <= swamp_cut
                     && (cov[idx] == cv::marsh || cov[idx] == cv::forest
                         || cov[idx] == cv::grass);
                break;
            default: break;
        }
        if (ok) cand.push_back(idx);
    }
    if (static_cast<int>(cand.size()) < n * 4) return {}; // too few sites to be a region

    std::shuffle(cand.begin(), cand.end(), rng);
    std::uniform_real_distribution<float> rad(5.0f, 11.0f);
    std::vector<ore_field> out;
    out.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        out.push_back(ore_field{ cand[static_cast<std::size_t>(i)], rad(rng) });
    return out;
}

// Per-tile multiplier for one resource's regions, normalised to mean 1.0 over
// the tiles that can bear it. Falls off smoothly from each centre so a region
// reads as a basin with margins rather than a stamped disc.
/// @param share Fraction of the world's total that should end up inside the
///              regions. This, not a peak multiplier, is the honest control:
///              normalising a peak to mean 1.0 sounds like concentration but is
///              not. With regions covering ~5% of land, a mean-1.0 field
///              leaves every tile outside them at ~0.97 — measurably identical
///              to no regions at all, which is what the first version did.
///              Fixing the share instead makes the statement directly: "most of
///              this world's copper is in two places."
/// @param bears 1 on tiles that actually carry this resource. The budget MUST be
///              conserved over these, not over all land: a region that lands on
///              ground which cannot bear the resource wastes its boost while the
///              reduction outside still applies, and the world quietly loses ore.
///              Measured at -47% petroleum, -19% copper, -17% coal, -10% iron
///              before this was keyed to the bearing set.
std::vector<float> ore_field_map(const std::vector<ore_field>& prov,
                                  int gw, int gh, const std::vector<uint8_t>& bears,
                                  float share)
{
    const int total = gw * gh;
    std::vector<float> f(static_cast<std::size_t>(total), 1.0f);
    if (prov.empty()) return f;

    // Raw shape: t^2 inside a region (a core with margins, not a stamped disc),
    // zero outside.
    std::vector<float> shape(static_cast<std::size_t>(total), 0.0f);
    for (int idx = 0; idx < total; ++idx)
    {
        if (!bears[static_cast<std::size_t>(idx)]) continue;
        const int c = idx % gw, r = idx / gw;
        float best = 0.0f;
        for (const ore_field& p : prov)
        {
            const int pc = p.centre % gw, pr = p.centre / gw;
            int dc = std::abs(c - pc);
            if (dc > gw / 2) dc = gw - dc;          // columns wrap
            const int dr = std::abs(r - pr);
            const float d = std::sqrt(static_cast<float>(dc * dc + dr * dr));
            if (d < p.radius)
            {
                const float t = 1.0f - d / p.radius; // 1 at centre, 0 at rim
                best = std::max(best, t * t);
            }
        }
        shape[static_cast<std::size_t>(idx)] = best;
    }

    // Split the land budget: `share` of it distributed by shape, the rest spread
    // evenly over the tiles the regions do not reach. Land-only, because ocean
    // tiles carry no land deposits and would otherwise make region strength
    // depend on how wet the world happens to be.
    double shape_sum = 0.0;
    int n_land = 0, n_outside = 0;
    for (int idx = 0; idx < total; ++idx)
    {
        if (!bears[static_cast<std::size_t>(idx)]) continue;
        ++n_land;
        const float s = shape[static_cast<std::size_t>(idx)];
        shape_sum += s;
        if (s <= 0.0f) ++n_outside;
    }
    if (shape_sum <= 0.0 || n_outside == 0) return f; // degenerate: leave it flat

    // Total land budget is n_land (mean 1.0), so the sum is conserved exactly.
    const double in_budget  = share * static_cast<double>(n_land);
    const double out_budget = (1.0 - share) * static_cast<double>(n_land);
    const float  out_value  = static_cast<float>(out_budget / static_cast<double>(n_outside));
    const float  in_scale   = static_cast<float>(in_budget / shape_sum);

    for (int idx = 0; idx < total; ++idx)
    {
        const float s = shape[static_cast<std::size_t>(idx)];
        f[static_cast<std::size_t>(idx)] = (s > 0.0f) ? s * in_scale : out_value;
    }
    return f;
}

int seed_count(feature_kind kind, geological_activity g, bool airless, int total_tiles)
{
    int base = 0;
    switch (kind)
    {
        case feature_kind::mountain:
            // Raised 2026-08-04 alongside the larger clusters in shape_of. Growing
            // the clusters alone took relief from 6.8% to 15.5% of land against
            // Earth's ~24%; the remaining shortfall is range COUNT, not range
            // size — going further on max_ring would produce blobs rather than
            // the chains the shapes are meant to read as.
            base = g == geological_activity::high ? 13
                 : g == geological_activity::moderate ? 11
                 : g == geological_activity::low ? 5 : 0;
            break;
        case feature_kind::rift:
            base = g == geological_activity::high ? 3
                 : g == geological_activity::moderate ? 1 : 0;
            break;
        case feature_kind::crater:
        {
            const int atmospheric = g == geological_activity::high ? 1
                                  : g == geological_activity::moderate ? 2
                                  : g == geological_activity::low ? 1 : 0;
            const int airless_bonus = !airless ? 0
                                    : g == geological_activity::high ? 2
                                    : g == geological_activity::moderate ? 3
                                    : g == geological_activity::low ? 3 : 4;
            base = atmospheric + airless_bonus;
            break;
        }
    }
    return scale_to_area(base, total_tiles);
}

// ---------------------------------------------------------------------------
// Pass 6 — deposits and derived environment
// ---------------------------------------------------------------------------

float roll(std::mt19937& rng, float lo, float hi)
{
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    return lo + u(rng) * (hi - lo);
}

// Modifiers apply multiplicatively to the upper bound of the base range, leaving
// the lower bound fixed (TILE_GENERATION.md § Pass 6).
float roll_mod(std::mt19937& rng, float lo, float hi, float upper_mod)
{
    return roll(rng, lo, hi * upper_mod);
}

// BL-040 — per-resource rarity scalar [0, 1], raw-tier resources only (refined and
// product goods are made, not mined). Seeded so a campaign's exact distribution
// varies, but each resource's BASE rarity is fixed and ordered to match its
// RESOURCES.md base-price rarity, so rare goods stay rare across seeds.
//
// The v0.0.4 seven-resource subset is pinned at 1.0 — the "near-universal ambient
// floor" end of the scale the design describes — so its hand-calibrated authoring
// (and the economy tuned on it) is left bit-for-bit unchanged. The scalar's
// behavioural modulation (frequency gate + magnitude scale) is realised on the
// resources this pass adds to complete the full raw set.
// See docs/economy/RESOURCES.md § Deposit rarity & scarcity.
std::array<float, resource_count> build_rarity_profile(uint32_t seed)
{
    using r = resource_type;
    std::array<float, resource_count> rarity{}; // 0 for non-raw / unauthored slots
    auto set = [&](resource_type res, float v) { rarity[static_cast<std::size_t>(res)] = v; };

    // Subset commons — pinned at the floor; authoring below is unchanged (×1.0).
    set(r::iron_ore, 1.0f); set(r::petroleum, 1.0f); set(r::water, 1.0f);
    set(r::agricultural_produce, 1.0f); set(r::regolith, 1.0f);
    set(r::stone, 1.0f); set(r::timber, 1.0f); set(r::sand, 1.0f);
    set(r::clay, 1.0f); set(r::peat, 1.0f);

    // Full-set additions — fixed base rarity, ordered by base-price rarity, plus a
    // small seeded jitter that varies the campaign without disturbing the ordering.
    struct base_rarity { resource_type res; float val; };
    static constexpr base_rarity bases[] = {
        { r::silica,                0.65f },
        { r::coal,                  0.60f },
        { r::iron_nickel_ore,       0.55f },
        { r::copper_ore,            0.50f },
        { r::rare_earth_ore,        0.30f },
        { r::platinum_group_metals, 0.15f }, // ultra-rare belt good
    };
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> jitter(-0.06f, 0.06f);
    for (const base_rarity& b : bases)
        set(b.res, std::clamp(b.val + jitter(rng), 0.05f, 1.0f));

    return rarity;
}

// Deposits, keyed on the axis that actually decides each one (BL-519).
//
// THIS SPLIT IS THE ITEM PAYING FOR ITSELF. Ore follows the SUBSTRATE, timber and
// produce follow the COVER, and separating them is what makes a forested metallic
// mountain carry both — which the single overloaded slot made impossible, since
// naming the forest cost you the metal.
//
// Every row below is the old table restated, not re-tuned: each old composition
// maps to exactly one (substrate, cover) pair, the draws happen in the same order
// with the same magnitudes, and the tiles that were bearing before are bearing
// now. What is genuinely new is that MORE tiles reach the biotic rows, because
// Pass 4d can dress rocky and volcanic ground the biome table left bare.
void generate_deposits(terrain_substrate sub, terrain_cover cov, std::uint8_t density,
                       terrain_landform lf,
                       std::array<float, resource_count>& dep, std::mt19937& rng,
                       std::mt19937& rare_rng,
                       const std::array<float, resource_count>& rarity)
{
    using su = terrain_substrate;
    using cv = terrain_cover;
    using r  = resource_type;
    auto put = [&](resource_type res, float v) { dep[static_cast<std::size_t>(res)] = v; };

    const bool mountain = lf == terrain_landform::mountain;
    const bool rift     = lf == terrain_landform::rift;
    const bool valley   = lf == terrain_landform::valley;
    const bool plains   = lf == terrain_landform::plains;
    const bool canyon   = lf == terrain_landform::canyon;

    // Cover thickness as a yield scalar, 0.6x at the sparse end to 1.15x at
    // closed canopy. This is the second consumer `cover_density` was added for
    // (BL-520's texture is the first): a wood that LOOKS thin should CUT thin,
    // and one number is what keeps those two from drifting apart. Centred so the
    // typical forest (density ~205) lands near 1.0 and the old magnitudes stand.
    const float thickness = 0.6f + 0.55f * cover_fraction(density);

    // --- ambient resources (guarantee at least one extractable per tile) ---
    if (sub != su::ocean && sub != su::icy)
        put(r::stone, roll(rng, 10.0f, 30.0f));
    // TIMBER FOLLOWS THE COVER, not the ground. The old rule read
    // `forest || wetland`; those are covers, and saying so is what admits a
    // forested rocky upland to the timber set.
    if (cov == cv::forest || cov == cv::marsh)
        put(r::timber, roll(rng, 15.0f, 40.0f) * thickness);
    if (sub == su::barren && (plains || canyon))
        put(r::sand, roll(rng, 10.0f, 25.0f));
    if (cov == cv::marsh || valley)
        put(r::clay, roll(rng, 8.0f, 20.0f));
    // PEAT is the old `tundra` row, and `sedimentary + scrub` is exactly that
    // biome — nothing else produces the pair, because Pass 4c writes only onto
    // bare ground and sedimentary always leaves the biome table dressed.
    if (sub == su::sedimentary && cov == cv::scrub && (plains || valley))
        put(r::peat, roll(rng, 5.0f, 15.0f));

    // --- primary deposits (prototype subset; other resources stay zero) ---
    // ORE FOLLOWS THE SUBSTRATE. Every mineral row keys on the ground alone, so
    // dressing a tile with vegetation no longer costs it its geology.
    switch (sub)
    {
        case su::barren:
            put(r::iron_ore,  roll_mod(rng, 0.0f, 150.0f, mountain ? 1.4f : rift ? 1.2f : 1.0f));
            put(r::petroleum, roll_mod(rng, 0.0f, 120.0f, valley ? 1.2f : 1.0f));
            break;
        case su::rocky:
            put(r::iron_ore,  roll_mod(rng, 0.0f, 200.0f, mountain ? 1.5f : 1.0f));
            break;
        case su::volcanic:
            put(r::iron_ore,  roll_mod(rng, 0.0f, 150.0f, rift ? 1.3f : 1.0f));
            break;
        case su::icy:
            put(r::water,     roll(rng, 0.0f, 400.0f));
            break;
        case su::metallic:
            // Prototype maps the metallic primary to iron_ore (the 7-resource
            // subset); iron-nickel ore and PGM are authored in a later pass.
            put(r::iron_ore, roll(rng, 50.0f, 250.0f));
            put(r::regolith, roll(rng, 20.0f, 50.0f));
            break;
        case su::regolith:
            put(r::regolith, roll(rng, 20.0f, 50.0f));
            break;
        // SEDIMENTARY is the one substrate whose primary is decided by what grows
        // on it — which is the honest reading of the old table, where grassland,
        // forest, wetland and tundra were four covers on one kind of ground.
        case su::sedimentary:
            switch (cov)
            {
                case cv::grass:
                    put(r::agricultural_produce,
                        roll_mod(rng, 40.0f, 180.0f, valley ? 1.3f : 1.0f) * thickness);
                    break;
                case cv::forest:
                    put(r::agricultural_produce,
                        roll_mod(rng, 10.0f, 80.0f, valley ? 1.15f : 1.0f) * thickness);
                    break;
                case cv::marsh:
                    put(r::agricultural_produce, roll(rng, 40.0f, 200.0f) * thickness);
                    break;
                case cv::scrub:
                    // The old `tundra` row: thin ground with surface iron.
                    put(r::iron_ore, roll_mod(rng, 0.0f, 60.0f, mountain ? 1.3f : 1.0f));
                    break;
                default:
                    break;
            }
            break;
        case su::ocean:
        case su::coast: // BL-516: unreachable — Pass 4e refines the water kinds
        case su::lake:  // into `reported_sub` after this pass reads `sub`.
            break;
    }

    // --- full raw-set deposits (BL-040) ---
    // The raw resources beyond the v0.0.4 subset, authored here so the map carries
    // the complete Tier-1 set. Drawn from an INDEPENDENT rng stream (rare_rng) so
    // the calibrated block above — and derive_environment's draws, which share the
    // tile rng — stay bit-for-bit unchanged. Each resource's seeded rarity scalar
    // gates presence (frequency) and scales magnitude on top of terrain affinity;
    // affinities follow docs/economy/RESOURCES.md Tier 1.
    std::uniform_real_distribution<float> gate(0.0f, 1.0f);
    auto put_rare = [&](resource_type res, float lo, float hi)
    {
        const float s = rarity[static_cast<std::size_t>(res)];
        if (gate(rare_rng) >= s) return;            // frequency: sparse when rare
        put(res, roll(rare_rng, lo, hi) * s);       // magnitude: small when rare
    };

    switch (sub)
    {
        case su::barren:
            put_rare(r::coal,   30.0f, 140.0f);     // buried carbon
            put_rare(r::silica, 20.0f,  90.0f);
            break;
        case su::rocky:
            put_rare(r::silica,         20.0f, 100.0f);
            put_rare(r::copper_ore,     30.0f, 160.0f);
            put_rare(r::rare_earth_ore, 10.0f,  70.0f);
            break;
        case su::volcanic:
            put_rare(r::copper_ore,     30.0f, 180.0f);
            put_rare(r::rare_earth_ore, 20.0f, 100.0f);
            break;
        case su::metallic:
            put_rare(r::iron_nickel_ore,       60.0f, 260.0f);
            put_rare(r::platinum_group_metals, 20.0f, 120.0f);
            break;
        default:
            break;
    }
}

// Hazard and habitability are not authored in the design tables; they are derived
// here from the SUBSTRATE (a base ceiling), the COVER (what living there is
// actually like) and the landform (a slope/exposure modifier).
//
// BL-519 split the first of those in two, and the split is legible in the numbers:
// the old table's four biotic rows differed from each other by the cover, not the
// ground — grassland and forest scored identically at 0.80, wetland a little lower
// for the mire, tundra much lower for the cold. So the substrate row now carries
// the geology's contribution and the cover row carries the rest, and each old
// composition reproduces its old pair exactly.
void derive_environment(terrain_substrate sub, terrain_cover cov, terrain_landform lf,
                        float& hazard, float& habitability, std::mt19937& rng)
{
    using su = terrain_substrate;
    using cv = terrain_cover;

    float base_haz = 0.25f;
    float base_hab = 0.25f;
    switch (sub)
    {
        // Sedimentary is the settlement substrate; its cover decides how much of
        // that potential is realised, immediately below.
        case su::sedimentary: base_haz = 0.15f; base_hab = 0.80f; break;
        case su::barren:      base_haz = 0.25f; base_hab = 0.25f; break;
        case su::rocky:       base_haz = 0.35f; base_hab = 0.25f; break;
        case su::icy:         base_haz = 0.40f; base_hab = 0.20f; break;
        case su::regolith:    base_haz = 0.30f; base_hab = 0.20f; break;
        case su::metallic:    base_haz = 0.30f; base_hab = 0.10f; break;
        case su::volcanic:    base_haz = 0.70f; base_hab = 0.10f; break;
        case su::ocean:
        case su::coast:       // BL-516: unreachable here — Pass 4e refines the water
        case su::lake:        base_haz = 0.10f; base_hab = 0.50f; break;
                              // kinds into `reported_sub` AFTER this pass reads `sub`.
    }

    // The cover's own contribution. On sedimentary ground these reproduce the old
    // per-composition numbers exactly (grass/forest 0.80, marsh 0.65, scrub — the
    // old tundra — 0.40 at a raised hazard); elsewhere they modulate a substrate
    // that used to have no way to say a cover was there at all.
    switch (cov)
    {
        case cv::marsh: base_hab = std::min(base_hab, 0.65f); break;
        case cv::scrub: base_hab = std::min(base_hab, 0.40f); base_haz = std::max(base_haz, 0.30f); break;
        case cv::snow:  base_hab *= 0.55f; base_haz += 0.10f; break;
        case cv::dunes: base_hab *= 0.60f; base_haz += 0.05f; break;
        case cv::ash:   base_hab *= 0.70f; base_haz += 0.10f; break;
        case cv::salt:  base_hab *= 0.55f; break;
        // Vegetation on ground that is not soil is a mild IMPROVEMENT — shelter,
        // fuel and forage where there was none. It cannot lift a tile past the
        // sedimentary ceiling, so a forested crag stays worse than a meadow.
        case cv::forest:
        case cv::grass: if (sub != su::sedimentary) base_hab = std::min(0.80f, base_hab * 1.30f); break;
        // Urban: the BL-366 ceiling is applied by maybe_transform_to_urban at the
        // moment of transform, not here — no tile generates urban.
        case cv::urban:
        case cv::none:  break;
    }

    switch (lf)
    {
        case terrain_landform::mountain: base_haz += 0.25f; base_hab *= 0.70f; break;
        case terrain_landform::rift:     base_haz += 0.30f; base_hab *= 0.60f; break;
        case terrain_landform::canyon:   base_haz += 0.10f; break;
        case terrain_landform::crater:   base_haz += 0.05f; break;
        case terrain_landform::valley:   base_hab *= 1.15f; break;
        case terrain_landform::highland: base_hab *= 0.90f; break;
        case terrain_landform::plains:   break;
    }

    std::uniform_real_distribution<float> jitter(-0.05f, 0.05f);
    hazard       = std::clamp(base_haz + jitter(rng), 0.0f, 1.0f);
    habitability = std::clamp(base_hab + jitter(rng), 0.0f, 1.0f);
}

} // namespace

std::vector<entity_id> generate_body_tiles(
    world& w,
    entity_id body_id,
    int gw, int gh,
    const body_profile& profile,
    uint32_t seed,
    float deposit_scalar,
    const planetology_state* pl,
    generation_record* record,
    const std::vector<float>* continent_bias,
    const std::vector<uint8_t>* convergent)
{
    const int total = gw * gh;

    // Distinct prime-ish seed offsets keep each stochastic pass independent, so
    // pass order does not bleed into another pass's draws.
    const uint32_t seed_height   = seed ^ 0x9E3779B1u;
    const uint32_t seed_moisture = seed ^ 0x85EBCA6Bu;
    const uint32_t seed_comp     = seed ^ 0xC2B2AE35u;
    const uint32_t seed_cluster  = seed ^ 0x27D4EB2Fu;
    const uint32_t seed_deposit  = seed ^ 0x165667B1u;

    // --- Pass 1: heightmap ---
    simplex_noise height_noise(seed_height);
    std::vector<float> height(total);
    for (int row = 0; row < gh; ++row)
        for (int col = 0; col < gw; ++col)
            height[col + row * gw] = fbm_cylinder(height_noise, col, row, gw, /*base_cycles=*/4.0f, /*octaves=*/5);

    // Continents/Drift (BL-210): plate-boundary uplift/rift is a CONSEQUENCE of
    // Planetology's S3 Engine output, added before normalisation so it shapes
    // the same heightmap a pure-noise pass would otherwise produce, not a
    // second competing terrain source.
    if (continent_bias && continent_bias->size() == static_cast<std::size_t>(total))
    {
        for (int idx = 0; idx < total; ++idx)
            height[static_cast<std::size_t>(idx)] += (*continent_bias)[static_cast<std::size_t>(idx)];
    }

    normalise(height);

    // Moisture (Pass 3's second axis): independent of latitude.
    simplex_noise moisture_noise(seed_moisture);
    std::vector<float> moisture(total);
    for (int row = 0; row < gh; ++row)
        for (int col = 0; col < gw; ++col)
            moisture[col + row * gw] = fbm_cylinder(moisture_noise, col, row, gw, /*base_cycles=*/3.0f, /*octaves=*/4);
    normalise(moisture);

    // The three terrain axes as parallel arrays (BL-519). `bio` is the internal
    // Pass 4a intermediate; `sub`/`cov`/`dens` are what the tiles are built from.
    std::vector<biome>             bio(total, biome::barren);
    std::vector<terrain_substrate> sub(total, terrain_substrate::barren);
    std::vector<terrain_cover>     cov(total, terrain_cover::none);
    std::vector<std::uint8_t>      dens(total, 0u);
    std::vector<terrain_landform>    land(total, terrain_landform::plains);
    std::vector<bool>                is_ocean(total, false);

    // --- Pass 2: ocean placement ---
    float ocean_threshold = 0.0f;
    int   ocean_tiles     = 0;
    // The biased heights the threshold is tested against, kept only when a record
    // was asked for (BL-303). Stays empty on a body with no ocean pass.
    std::vector<float> ocean_score;
    // Poorly-drained ground: the lowest slice of LAND, measured against this
    // body's own sea level rather than an absolute height. Filled by Pass 2 and
    // consumed by Pass 4b (drainage). Empty on a body with no standing water,
    // which is exactly the case where "poorly drained" has no meaning.
    std::vector<uint8_t> lowland(total, 0u);
    if (profile.hydrology == hydrological_state::liquid && profile.water_fraction > 0.0f)
    {
        // Bias the heightmap downward toward the equator so ocean concentrates
        // there without enforcing a uniform band; the noise keeps the coastline
        // irregular.
        //
        // Tuned 2026-06-14: bias_amp lowered 0.15 → 0.07 → 0.05. The old value
        // drowned most tropical/subtropical land, collapsing forest (~1%) and
        // wetland (~0%) on Kepler. Reducing the bias widens the equatorial
        // landmass belt and lets high-moisture tropical/subtropical tiles reach
        // the forest/wetland branches of biome_atmospheric(). Total ocean
        // fraction is still set by the percentile threshold against water_fraction
        // (0.60 for Kepler), so ocean coverage stays correct; only the
        // land-to-water *distribution* across latitudes changes.
        constexpr float bias_amp = 0.05f;
        std::vector<float> biased(total);
        for (int row = 0; row < gh; ++row)
        {
            const float lat  = (gh > 1)
                ? (static_cast<float>(row) - static_cast<float>(gh - 1) * 0.5f) / (static_cast<float>(gh - 1) * 0.5f)
                : 0.0f;
            const float bias = bias_amp * (1.0f - lat * lat);
            for (int col = 0; col < gw; ++col)
            {
                const int idx = col + row * gw;
                biased[idx] = height[idx] - bias;
            }
        }

        std::vector<float> sorted = biased;
        std::sort(sorted.begin(), sorted.end());
        const int k = std::clamp(static_cast<int>(profile.water_fraction * static_cast<float>(total)), 0, total - 1);
        ocean_threshold = sorted[static_cast<std::size_t>(k)];

        for (int idx = 0; idx < total; ++idx)
        {
            if (biased[idx] < ocean_threshold)
            {
                is_ocean[idx] = true;
                bio[idx]      = biome::ocean;
                ++ocean_tiles;
            }
        }

        // Land elevation runs from `ocean_threshold` (the shoreline) up to
        // sorted.back(). Take the bottom `lowland_share` of that ordering — a
        // PERCENTILE, like the ocean threshold above it, so the coastal-plain
        // slice exists on every seed instead of depending on where a noise blob
        // happened to land. An absolute cut cannot do this job: the Pass 5 valley
        // fill uses height < 0.35 and finds literally zero Kepler tiles, because
        // the ocean already took the bottom 60% of the heightmap.
        //
        // 0.15 is the ground genuinely near base level — coastal plain and delta,
        // where water has nowhere further to go. Widening it to 0.20-0.25 starts
        // taking ordinary inland plain that drains perfectly well, which is not
        // what this pass is for; measured, those values push Kepler's wetland to
        // 3.2%/4.0% of land by marshing ground that has no reason to be marsh.
        constexpr float lowland_share = 0.15f;
        if (k < total - 1)
        {
            const int lk = k + static_cast<int>(lowland_share * static_cast<float>(total - 1 - k));
            const float lowland_threshold = sorted[static_cast<std::size_t>(std::clamp(lk, k, total - 1))];
            for (int idx = 0; idx < total; ++idx)
                if (!is_ocean[idx] && biased[idx] <= lowland_threshold)
                    lowland[static_cast<std::size_t>(idx)] = 1u;
        }

        // Last use of `biased`, so the capture costs a move rather than a copy.
        if (record)
            ocean_score = std::move(biased);
    }

    // --- Pass 3: latitude bands ---
    std::vector<lat_band> band(total);
    for (int row = 0; row < gh; ++row)
    {
        const lat_band b = band_for_row(row, gh, profile.temperature);
        for (int col = 0; col < gw; ++col)
            band[col + row * gw] = b;
    }

    // --- Pass 4: composition ---
    std::mt19937 comp_rng(seed_comp);
    const bool  airless = profile.atmosphere == atmosphere_class::none
                       || profile.atmosphere == atmosphere_class::thin;
    const float volc_p  = volcanic_probability(profile.geology);
    // BL-167: a world whose biosphere never reached land cannot carry grassland,
    // forest, wetland or tundra. This one condition is what makes a dead world
    // LOOK dead on the canvas rather than merely reading poorer in a ledger.
    const bool  biotic  = (pl == nullptr) || pl->stage >= life_stage::land;
    std::uniform_real_distribution<float> u01(0.0f, 1.0f);

    for (int idx = 0; idx < total; ++idx)
    {
        if (is_ocean[idx])
            continue;

        const lat_band b = band[idx];
        biome c;

        if (profile.bias == composition_bias::metallic)
        {
            c = pick_weighted3(comp_rng, biome::metallic, 55,
                                         biome::rocky,    30,
                                         biome::regolith, 15);
        }
        else if (airless)
        {
            if (profile.hydrology == hydrological_state::polar_frozen && b == lat_band::polar)
            {
                c = biome::icy; // polar override for frozen-pole moons
            }
            else if (profile.temperature == temperature_class::scorching
                  || profile.temperature == temperature_class::hot)
            {
                c = pick_weighted3(comp_rng, biome::volcanic, 45,
                                             biome::barren,   40,
                                             biome::rocky,    15);
            }
            else
            {
                c = pick_weighted2(comp_rng, biome::regolith, 65,
                                             biome::rocky,    30);
            }
        }
        else
        {
            const bool volcanic_band = (b == lat_band::subtropical || b == lat_band::tropical);
            if (volcanic_band && volc_p > 0.0f && u01(comp_rng) < volc_p)
                c = biome::volcanic;
            else
                c = biotic ? biome_atmospheric(b, moisture[idx], comp_rng)
                           : biome_abiotic(b, moisture[idx], comp_rng);
        }

        bio[idx] = c;
    }

    // --- Pass 4b: drainage (BL-338) ---
    //
    // Wetland is a DRAINAGE feature, not a climate zone. Until now it was
    // reachable only from two cells of the (band, moisture) table — subtropical
    // and tropical at high moisture — which on Kepler is 16% of rows, the same
    // rows the equatorial ocean bias drowns hardest. Moisture is latitude-blind
    // noise, so whether the home planet has any wetland at all was decided by
    // where one noise blob happened to sit. On the shipped seed it sat off the
    // equator: the subtropical/tropical bands came out DRY (437 of 624 land tiles
    // in the lowest moisture column) and the world generated 12 wetland tiles.
    // Meanwhile the wet ground the world does have — 2,332 high-moisture land
    // tiles in the temperate and subpolar bands — could not produce a marsh,
    // because latitude forbade it.
    //
    // Marshes form where low ground cannot drain: coastal plains, deltas, the
    // ground a river has nowhere to leave. That is height + water + moisture, and
    // height had no say in composition at all. So: low-lying, high-moisture ground
    // carrying an already-biotic cover becomes wetland.
    //
    // EXTENDS the table rather than contradicting it. The override adds the axis
    // the table lacks (elevation) to the two bands whose wet cell the table leaves
    // as ordinary vegetation, and stays out of the cells where the table has
    // already NAMED what wet ground is:
    //   - polar wet ground is icy, and a cap does not thaw because it is low;
    //   - subpolar wet ground is TUNDRA, which the table states deliberately.
    // Peat bog is a real subpolar landform and arguably belongs here, but taking
    // tundra would overrule the table's own answer for that band — and tundra
    // scores 9 for farm quality against wetland's 58 (settlement.cpp), so
    // converting it swings habitability hard enough to redraw the region map.
    // Left alone; if subpolar peatland is wanted it should be the table's call.
    //
    // Deliberately an OVERRIDE applied after the table, drawing no RNG of its own
    // — the same shape as the deposit_scalar / endowment post-multiplies in Pass
    // 6. comp_rng's stream is untouched, so this cannot shift any later draw, and
    // Pass 5 is bit-for-bit unchanged (pick_seeds prefers rocky/barren/regolith
    // on HIGH ground; this converts only biotic cover on the lowest ground).
    if (biotic && !airless && ocean_tiles > 0)
    {
        for (int idx = 0; idx < total; ++idx)
        {
            if (is_ocean[idx] || lowland[static_cast<std::size_t>(idx)] == 0u)
                continue;
            if (moisture_column(moisture[idx]) < 2)
                continue;
            const lat_band b = band[idx];
            if (b == lat_band::polar || b == lat_band::subpolar)
                continue;
            // Only ground that already carries a land biosphere: a marsh needs
            // vegetation. Barren, rocky and volcanic are excluded on their own
            // terms, and grassland/forest are the two covers the warm bands' wet
            // cells actually produce.
            const biome c = bio[idx];
            if (c == biome::grassland || c == biome::forest)
                bio[idx] = biome::wetland;
        }
    }

    // --- Pass 4c: decompose the biome onto the two axes (BL-519) ---
    //
    // No RNG stream is touched here or in Pass 4d, and that is the property the
    // whole change was structured around: every downstream pass — clusters,
    // deposits, environment, rivers, roads, nations, provinces — sees the same
    // draws it saw before, so any measurement that moved is attributable to the
    // new cover axis rather than to stream drift.
    for (int idx = 0; idx < total; ++idx)
    {
        const tile_axes a = decompose_biome(bio[idx], moisture[idx], seed_comp, idx);
        sub[idx]  = a.substrate;
        cov[idx]  = a.cover;
        dens[idx] = a.density;
    }

    // --- Pass 4d: cover refinement — Ben's "a mountain might have a forest" ---
    //
    // The only genuinely new terrain behaviour in BL-519. It dresses ground the
    // biome table left BARE and never rewrites a substrate, so it can add tiles
    // to a cover class but never remove one: the pre-split biotic distributions
    // are a floor, not a moving target.
    for (int idx = 0; idx < total; ++idx)
    {
        if (is_ocean[idx])
            continue;
        tile_axes a{ sub[idx], cov[idx], dens[idx] };
        refine_cover(a, band[idx], moisture[idx], height[idx], biotic, seed_comp, idx);
        cov[idx]  = a.cover;
        dens[idx] = a.density;
    }

    // --- Pass 4e: water kinds — lake, coast and ocean (BL-516) ---
    //
    // `sub` itself is left alone, deliberately: Passes 5 and 6 below still see
    // the coarse `ocean`, so no cluster, deposit or environment draw moves.
    // `reported_sub` is what the tile carries. See classify_water_kinds.
    std::vector<terrain_substrate> reported_sub = sub;
    classify_water_kinds(is_ocean, gw, gh, reported_sub);

    // --- Pass 5: landform clusters ---
    std::mt19937 cluster_rng(seed_cluster);
    std::vector<bool> claimed(total, false);
    for (int idx = 0; idx < total; ++idx)
        if (is_ocean[idx])
            claimed[idx] = true; // ocean blocks clusters and the valley fill

    for (feature_kind kind : { feature_kind::mountain, feature_kind::rift, feature_kind::crater })
    {
        const int n = seed_count(kind, profile.geology, airless, total);
        const std::vector<int> seeds = pick_seeds(kind, n, gw, gh, height, sub, band, is_ocean,
                                                  cluster_rng, convergent);
        for (int idx : seeds)
            grow_cluster(kind, idx % gw, idx / gw, gw, gh, land, claimed, cluster_rng);
    }

    // Remaining low ground becomes valley; everything else stays plains.
    for (int idx = 0; idx < total; ++idx)
        if (!claimed[idx] && !is_ocean[idx] && height[idx] < 0.35f)
        {
            land[idx]    = terrain_landform::valley;
            claimed[idx] = true;
        }

    // --- Pass 6: deposits, derived environment, entity creation ---
    // Per-body rarity field (BL-040): one seeded draw, stable across the body's
    // tiles, so each campaign varies while the rare-stays-rare ordering holds.
    const std::array<float, resource_count> rarity = build_rarity_profile(seed ^ 0x68E31DA4u);

    // ore fields (Open calls 4). Own RNG stream, so adding this pass leaves
    // every earlier draw untouched; skipped entirely without a planetology state,
    // which keeps the null-pl identity contract exact.
    std::vector<std::pair<resource_type, std::vector<float>>> ore_field_maps;
    if (pl)
    {
        std::mt19937 prov_rng(seed ^ 0x0BE0F1E1u);
        struct spec { resource_type res; int count; float share; };
        // Counts sit inside Open calls 4's "2-5 seeded region records" per
        // resource. `share` is the fraction of the world's total that ends up in
        // those regions, ordered by how region-bound the real material is:
        // porphyry copper is the extreme (a handful of districts supply most of
        // world production), coal the mildest (workable seams are widespread even
        // though the great basins dominate tonnage).
        static constexpr spec k_specs[] = {
            { resource_type::copper_ore, 2, 0.65f },
            { resource_type::petroleum,  2, 0.60f },
            { resource_type::iron_ore,   2, 0.55f },
            { resource_type::coal,       3, 0.45f },
        };
        // Which tiles will actually bear each of these. generate_deposits is a
        // pure function of (substrate, cover, landform, per-tile seeds), so replaying
        // it here is exact and costs one extra table-driven pass. Necessary
        // because the region budget has to be conserved over the BEARING set:
        // normalising over all land instead silently cost a world 10-47% of its
        // ore wherever a region landed on ground that carries none.
        std::array<std::vector<uint8_t>, 4> bears;
        for (auto& b : bears) b.assign(static_cast<std::size_t>(total), 0u);
        for (int idx = 0; idx < total; ++idx)
        {
            if (is_ocean[idx]) continue;
            std::mt19937 tr(seed_deposit ^ (static_cast<uint32_t>(idx) * 2654435761u));
            std::mt19937 rr(seed_deposit ^ (static_cast<uint32_t>(idx) * 40503u) ^ 0x5BD1E995u);
            std::array<float, resource_count> d{};
            generate_deposits(sub[idx], cov[idx], dens[idx], land[idx], d, tr, rr, rarity);
            for (std::size_t k = 0; k < 4; ++k)
                if (d[static_cast<std::size_t>(k_specs[k].res)] > 0.0f)
                    bears[k][static_cast<std::size_t>(idx)] = 1u;
        }

        for (std::size_t k = 0; k < 4; ++k)
        {
            const spec& s = k_specs[k];
            if (pl->endowment[static_cast<std::size_t>(s.res)] <= 0.0f)
                continue; // the world's history never made this — nothing to place
            const auto prov = ore_fields_for(s.res, s.count, gw, gh, height, is_ocean,
                                            cov, convergent, prov_rng);
            if (prov.empty()) continue;
            ore_field_maps.emplace_back(s.res, ore_field_map(prov, gw, gh, bears[k], s.share));
        }
    }

    std::vector<entity_id> tile_ids(total, null_entity);
    for (int row = 0; row < gh; ++row)
    {
        for (int col = 0; col < gw; ++col)
        {
            const int idx = col + row * gw;

            // Per-tile RNG keyed on body seed + tile index: deposits are stable
            // across runs and independent of neighbouring tiles.
            std::mt19937 tile_rng(seed_deposit ^ (static_cast<uint32_t>(idx) * 2654435761u));
            // Independent stream for the full raw-set additions, so they cannot
            // perturb the calibrated subset draws or derive_environment (BL-040).
            std::mt19937 rare_rng(seed_deposit ^ (static_cast<uint32_t>(idx) * 40503u) ^ 0x5BD1E995u);

            std::array<float, resource_count> deposits{};
            if (!is_ocean[idx])
                generate_deposits(sub[idx], cov[idx], dens[idx], land[idx], deposits, tile_rng, rare_rng, rarity);

            // BL-114: resource-abundance scalar. A pure post-multiply on the filled
            // deposit array — it draws no RNG, so deposit_scalar == 1.0f reproduces the
            // unscaled surface bit-for-bit. Earth-like is the ceiling (1.0); leaner
            // worlds pass a value below 1 (GENERATION_STRATEGY.md § The resource
            // ceiling). Applied before the reserve below so both scale consistently.
            for (std::size_t r = 0; r < resource_count; ++r)
                if (deposits[r] > 0.0f)
                    deposits[r] *= deposit_scalar;

            // BL-167: the Planetology endowment. Same pure post-multiply shape as
            // deposit_scalar above — it draws no RNG, so a null planetology state
            // reproduces the unscaled surface bit-for-bit. This is where "no life,
            // no coal" actually lands: a channel at 0.0 removes the resource
            // outright rather than merely thinning it.
            if (pl)
                for (std::size_t r = 0; r < resource_count; ++r)
                    if (deposits[r] > 0.0f)
                        deposits[r] *= pl->endowment[r];

            // ore fields: a third pure post-multiply in the same shape as the
            // two above, and equally RNG-free at this point (the placement drew
            // its randomness once, before the tile loop). The field is mean-1.0
            // over land, so this redistributes the endowment without changing
            // the world's total.
            for (const auto& [res, field] : ore_field_maps)
            {
                const std::size_t ri = static_cast<std::size_t>(res);
                if (deposits[ri] > 0.0f)
                    deposits[ri] *= field[static_cast<std::size_t>(idx)];
            }

            // C -> D: endemic trade goods (BL-191). Unlike the endowment above
            // this ADDS a deposit rather than scaling one, because an endemic good
            // has no base distribution to scale — it exists only where it evolved.
            //
            // A tile qualifies only if it is inside the good's latitude band AND
            // its longitude sector AND carries a composition the crop can grow on.
            // The sector test is what makes it endemic rather than merely
            // climatic, and it wraps, since the surface does.
            if (pl && !pl->endemics.empty() && !is_ocean[idx])
            {
                const float lat = std::fabs(static_cast<float>(row) / static_cast<float>(gh - 1) - 0.5f) * 2.0f;
                const float lon = static_cast<float>(col) / static_cast<float>(gw);

                for (const endemic_good& e : pl->endemics)
                {
                    if (lat < e.lat_lo || lat > e.lat_hi)
                        continue;

                    // Wrapped angular separation from the origin region's centre.
                    float d = std::fabs(lon - e.sector_centre);
                    if (d > 0.5f) d = 1.0f - d;
                    if (d > e.sector_width * 0.5f)
                        continue;

                    // Composition the crop actually grows on. All of these are
                    // biotic, so they only exist on a world that reached a land
                    // biosphere in the first place — which is the same gate the
                    // endemic set itself sits behind.
                    // CROPS FOLLOW THE COVER (BL-519). Each of these is a
                    // claim about what grows here, not about the geology — which
                    // is why the old list had to name compositions to say it.
                    const terrain_cover c = cov[idx];
                    bool suitable = false;
                    switch (e.good)
                    {
                        case resource_type::tobacco: suitable = (c == terrain_cover::grass); break;
                        case resource_type::spices:  suitable = (c == terrain_cover::marsh
                                                             || c == terrain_cover::forest); break;
                        case resource_type::coffee:  suitable = (c == terrain_cover::forest); break;
                        // Furs were the old `tundra` row. Tundra is gone as a
                        // terrain (Ben, 2026-08-21) and its ground is scrub, which
                        // is where the trapping actually happens.
                        case resource_type::furs:    suitable = (c == terrain_cover::scrub); break;
                        default: break;
                    }
                    if (!suitable)
                        continue;

                    // Densest at the heart of the range, thinning toward its edge —
                    // so a origin region has a centre worth holding rather than a
                    // hard border.
                    const float falloff = 1.0f - (d / std::max(e.sector_width * 0.5f, 1e-4f));
                    std::uniform_real_distribution<float> u(0.0f, 1.0f);
                    const float amount = (30.0f + 90.0f * u(tile_rng)) * e.richness * falloff;
                    if (amount > 1.0f)
                        deposits[static_cast<std::size_t>(e.good)] = amount * deposit_scalar;
                }
            }

            // Seed the finite extraction reserve from richness. Richness stays the
            // rate multiplier; the reserve is what depletion (economy_system.cpp)
            // draws down. Scaled so a typical deposit lasts dozens of economy ticks.
            std::array<float, resource_count> remaining{};
            for (std::size_t r = 0; r < resource_count; ++r)
                if (deposits[r] > 0.0f)
                    remaining[r] = deposits[r] * deposit_reserve_factor;

            float hazard = 0.0f, habitability = 0.0f;
            derive_environment(sub[idx], cov[idx], land[idx], hazard, habitability, tile_rng);

            const entity_id tile_id = w.create_entity();
            w.tiles[tile_id] = tile_component{
                .body               = body_id,
                .grid_x             = col,
                .grid_y             = row,
                .substrate          = reported_sub[idx], // BL-516 water kind; == sub[idx] on land
                .cover              = cov[idx],
                .cover_density      = dens[idx],
                .landform           = land[idx],
                .resource_deposit   = deposits,
                .resource_remaining = remaining,
                .hazard_level       = hazard,
                .habitability       = habitability,
                // BL-517: retain Pass 1's heightmap value. A pure CAPTURE of the float
                // this function already computed — the same element that goes on to fill
                // generation_record::height below. It reads no RNG, derives nothing, and
                // changes no terrain, deposit or placement rule, so the generated surface
                // is bit-identical to the pre-BL-517 build.
                .height             = height[static_cast<std::size_t>(idx)],
            };
            tile_ids[idx] = tile_id;
        }
    }

    if (record)
    {
        record->gw = gw;
        record->gh = gh;
        record->height   = std::move(height);
        record->moisture = std::move(moisture);
        record->band.resize(static_cast<std::size_t>(total));
        for (int idx = 0; idx < total; ++idx)
            record->band[static_cast<std::size_t>(idx)] = static_cast<uint8_t>(band[idx]);
        record->ocean_score     = std::move(ocean_score);
        record->ocean_threshold = ocean_threshold;
        record->ocean_tiles     = ocean_tiles;
    }

    return tile_ids;
}

std::vector<entity_id> first_land_tiles(const std::vector<entity_id>& tile_ids,
                                        const world& w, int gw, int gh, int n)
{
    std::vector<entity_id> result;
    result.reserve(static_cast<std::size_t>(n));
    for (int row = 0; row < gh && static_cast<int>(result.size()) < n; ++row)
        for (int col = 0; col < gw && static_cast<int>(result.size()) < n; ++col)
        {
            const entity_id id = tile_ids[col + row * gw];
            if (id != null_entity && !is_water(w.tiles.at(id).substrate))
                result.push_back(id);
        }
    return result;
}
