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
// Pass 4 — composition
// ---------------------------------------------------------------------------

// Tuned 2026-06-14 (Kepler biome balance): the high-moisture cutoff was lowered
// from 0.65 to 0.55 so more tiles reach the wet (mc==2) branches of
// composition_atmospheric() that produce forest and wetland — these stayed sparse
// (~1% / ~0.5%) on Kepler even after the Pass 2 ocean-bias reduction.
int moisture_column(float m) { return m < 0.35f ? 0 : (m < 0.55f ? 1 : 2); }

// 60/40 weighting toward `a`, resolved against the per-tile RNG.
terrain_composition pick_60_40(std::mt19937& rng, terrain_composition a, terrain_composition b)
{
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    return (u(rng) < 0.6f) ? a : b;
}

terrain_composition pick_weighted2(std::mt19937& rng,
                                   terrain_composition a, int wa,
                                   terrain_composition b, int wb)
{
    std::uniform_int_distribution<int> d(0, wa + wb - 1);
    return (d(rng) < wa) ? a : b;
}

terrain_composition pick_weighted3(std::mt19937& rng,
                                   terrain_composition a, int wa,
                                   terrain_composition b, int wb,
                                   terrain_composition c, int wc)
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
// Mirrors composition_atmospheric's RNG consumption draw-for-draw, so the two
// branches stay stream-aligned and switching between them cannot shift any
// downstream pass.
terrain_composition composition_abiotic(lat_band band, float moisture, std::mt19937& rng)
{
    using tc = terrain_composition;
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
terrain_composition composition_atmospheric(lat_band band, float moisture, std::mt19937& rng)
{
    using tc = terrain_composition;
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
        case feature_kind::mountain: return { 3, 0.65f };
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
        case feature_kind::mountain:
            if (ring == 0) return terrain_landform::mountain;
            if (ring == 1) return (u(rng) < 0.7f) ? terrain_landform::highland : terrain_landform::mountain;
            return (u(rng) < 0.5f) ? terrain_landform::highland : terrain_landform::plains;
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
                            const std::vector<terrain_composition>& comp,
                            const std::vector<lat_band>& band,
                            const std::vector<bool>& is_ocean,
                            std::mt19937& rng)
{
    using tc = terrain_composition;
    std::vector<int> preferred;
    std::vector<int> any_land;
    const int total = gw * gh;
    for (int idx = 0; idx < total; ++idx)
    {
        if (is_ocean[idx])
            continue;
        any_land.push_back(idx);

        bool ok = false;
        switch (kind)
        {
            case feature_kind::mountain:
                ok = height[idx] > 0.65f && (comp[idx] == tc::rocky || comp[idx] == tc::barren);
                break;
            case feature_kind::rift:
                ok = (comp[idx] == tc::volcanic || comp[idx] == tc::barren)
                     && (band[idx] == lat_band::subtropical || band[idx] == lat_band::tropical);
                break;
            case feature_kind::crater:
                ok = (comp[idx] == tc::regolith || comp[idx] == tc::barren);
                break;
        }
        if (ok)
            preferred.push_back(idx);
    }

    std::vector<int>& pool = (static_cast<int>(preferred.size()) >= n_seeds) ? preferred : any_land;
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

int seed_count(feature_kind kind, geological_activity g, bool airless, int total_tiles)
{
    int base = 0;
    switch (kind)
    {
        case feature_kind::mountain:
            base = g == geological_activity::high ? 5
                 : g == geological_activity::moderate ? 4
                 : g == geological_activity::low ? 2 : 0;
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

void generate_deposits(terrain_composition comp, terrain_landform lf,
                       std::array<float, resource_count>& dep, std::mt19937& rng,
                       std::mt19937& rare_rng,
                       const std::array<float, resource_count>& rarity)
{
    using tc = terrain_composition;
    using r  = resource_type;
    auto put = [&](resource_type res, float v) { dep[static_cast<std::size_t>(res)] = v; };

    const bool mountain = lf == terrain_landform::mountain;
    const bool rift     = lf == terrain_landform::rift;
    const bool valley   = lf == terrain_landform::valley;
    const bool plains   = lf == terrain_landform::plains;
    const bool canyon   = lf == terrain_landform::canyon;

    // --- ambient resources (guarantee at least one extractable per tile) ---
    if (comp != tc::ocean && comp != tc::icy)
        put(r::stone, roll(rng, 10.0f, 30.0f));
    if (comp == tc::forest || comp == tc::wetland)
        put(r::timber, roll(rng, 15.0f, 40.0f));
    if (comp == tc::barren && (plains || canyon))
        put(r::sand, roll(rng, 10.0f, 25.0f));
    if (comp == tc::wetland || valley)
        put(r::clay, roll(rng, 8.0f, 20.0f));
    if (comp == tc::tundra && (plains || valley))
        put(r::peat, roll(rng, 5.0f, 15.0f));

    // --- primary deposits (prototype subset; other resources stay zero) ---
    switch (comp)
    {
        case tc::barren:
            put(r::iron_ore,  roll_mod(rng, 0.0f, 150.0f, mountain ? 1.4f : rift ? 1.2f : 1.0f));
            put(r::petroleum, roll_mod(rng, 0.0f, 120.0f, valley ? 1.2f : 1.0f));
            break;
        case tc::rocky:
            put(r::iron_ore,  roll_mod(rng, 0.0f, 200.0f, mountain ? 1.5f : 1.0f));
            break;
        case tc::volcanic:
            put(r::iron_ore,  roll_mod(rng, 0.0f, 150.0f, rift ? 1.3f : 1.0f));
            break;
        case tc::icy:
            put(r::water,     roll(rng, 0.0f, 400.0f));
            break;
        case tc::grassland:
            put(r::agricultural_produce, roll_mod(rng, 40.0f, 180.0f, valley ? 1.3f : 1.0f));
            break;
        case tc::forest:
            put(r::agricultural_produce, roll_mod(rng, 10.0f, 80.0f, valley ? 1.15f : 1.0f));
            break;
        case tc::wetland:
            put(r::agricultural_produce, roll(rng, 40.0f, 200.0f));
            break;
        case tc::tundra:
            put(r::iron_ore,  roll_mod(rng, 0.0f, 60.0f, mountain ? 1.3f : 1.0f));
            break;
        case tc::metallic:
            // Prototype maps the metallic primary to iron_ore (the 7-resource
            // subset); iron-nickel ore and PGM are authored in a later pass.
            put(r::iron_ore, roll(rng, 50.0f, 250.0f));
            put(r::regolith, roll(rng, 20.0f, 50.0f));
            break;
        case tc::regolith:
            put(r::regolith, roll(rng, 20.0f, 50.0f));
            break;
        case tc::ocean:
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

    switch (comp)
    {
        case tc::barren:
            put_rare(r::coal,   30.0f, 140.0f);     // sedimentary carbon
            put_rare(r::silica, 20.0f,  90.0f);
            break;
        case tc::rocky:
            put_rare(r::silica,         20.0f, 100.0f);
            put_rare(r::copper_ore,     30.0f, 160.0f);
            put_rare(r::rare_earth_ore, 10.0f,  70.0f);
            break;
        case tc::volcanic:
            put_rare(r::copper_ore,     30.0f, 180.0f);
            put_rare(r::rare_earth_ore, 20.0f, 100.0f);
            break;
        case tc::metallic:
            put_rare(r::iron_nickel_ore,       60.0f, 260.0f);
            put_rare(r::platinum_group_metals, 20.0f, 120.0f);
            break;
        default:
            break;
    }
}

// Hazard and habitability are not authored in the design tables; they are derived
// here from composition (a base ceiling) and landform (a slope/exposure modifier)
// so the existing tile_component fields stay meaningful after the model change.
void derive_environment(terrain_composition comp, terrain_landform lf,
                        float& hazard, float& habitability, std::mt19937& rng)
{
    using tc = terrain_composition;

    float base_haz = 0.25f;
    float base_hab = 0.25f;
    switch (comp)
    {
        case tc::grassland: base_haz = 0.15f; base_hab = 0.80f; break;
        case tc::forest:    base_haz = 0.15f; base_hab = 0.80f; break;
        case tc::wetland:   base_haz = 0.15f; base_hab = 0.65f; break;
        case tc::tundra:    base_haz = 0.30f; base_hab = 0.40f; break;
        case tc::barren:    base_haz = 0.25f; base_hab = 0.25f; break;
        case tc::rocky:     base_haz = 0.35f; base_hab = 0.25f; break;
        case tc::icy:       base_haz = 0.40f; base_hab = 0.20f; break;
        case tc::regolith:  base_haz = 0.30f; base_hab = 0.20f; break;
        case tc::metallic:  base_haz = 0.30f; base_hab = 0.10f; break;
        case tc::volcanic:  base_haz = 0.70f; base_hab = 0.10f; break;
        case tc::ocean:     base_haz = 0.10f; base_hab = 0.50f; break;
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
    const std::vector<float>* continent_bias)
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

    std::vector<terrain_composition> comp(total, terrain_composition::barren);
    std::vector<terrain_landform>    land(total, terrain_landform::plains);
    std::vector<bool>                is_ocean(total, false);

    // --- Pass 2: ocean placement ---
    float ocean_threshold = 0.0f;
    int   ocean_tiles     = 0;
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
        // the forest/wetland branches of composition_atmospheric(). Total ocean
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
                comp[idx]     = terrain_composition::ocean;
                ++ocean_tiles;
            }
        }
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
        terrain_composition c;

        if (profile.bias == composition_bias::metallic)
        {
            c = pick_weighted3(comp_rng, terrain_composition::metallic, 55,
                                         terrain_composition::rocky,    30,
                                         terrain_composition::regolith, 15);
        }
        else if (airless)
        {
            if (profile.hydrology == hydrological_state::polar_frozen && b == lat_band::polar)
            {
                c = terrain_composition::icy; // polar override for frozen-pole moons
            }
            else if (profile.temperature == temperature_class::scorching
                  || profile.temperature == temperature_class::hot)
            {
                c = pick_weighted3(comp_rng, terrain_composition::volcanic, 45,
                                             terrain_composition::barren,   40,
                                             terrain_composition::rocky,    15);
            }
            else
            {
                c = pick_weighted2(comp_rng, terrain_composition::regolith, 65,
                                             terrain_composition::rocky,    30);
            }
        }
        else
        {
            const bool volcanic_band = (b == lat_band::subtropical || b == lat_band::tropical);
            if (volcanic_band && volc_p > 0.0f && u01(comp_rng) < volc_p)
                c = terrain_composition::volcanic;
            else
                c = biotic ? composition_atmospheric(b, moisture[idx], comp_rng)
                           : composition_abiotic(b, moisture[idx], comp_rng);
        }

        comp[idx] = c;
    }

    // --- Pass 5: landform clusters ---
    std::mt19937 cluster_rng(seed_cluster);
    std::vector<bool> claimed(total, false);
    for (int idx = 0; idx < total; ++idx)
        if (is_ocean[idx])
            claimed[idx] = true; // ocean blocks clusters and the valley fill

    for (feature_kind kind : { feature_kind::mountain, feature_kind::rift, feature_kind::crater })
    {
        const int n = seed_count(kind, profile.geology, airless, total);
        const std::vector<int> seeds = pick_seeds(kind, n, gw, gh, height, comp, band, is_ocean, cluster_rng);
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
                generate_deposits(comp[idx], land[idx], deposits, tile_rng, rare_rng, rarity);

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
                    const terrain_composition c = comp[idx];
                    bool suitable = false;
                    switch (e.good)
                    {
                        case resource_type::tobacco: suitable = (c == terrain_composition::grassland); break;
                        case resource_type::spices:  suitable = (c == terrain_composition::wetland
                                                             || c == terrain_composition::forest); break;
                        case resource_type::coffee:  suitable = (c == terrain_composition::forest); break;
                        case resource_type::furs:    suitable = (c == terrain_composition::tundra); break;
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
            derive_environment(comp[idx], land[idx], hazard, habitability, tile_rng);

            const entity_id tile_id = w.create_entity();
            w.tiles[tile_id] = tile_component{
                .body               = body_id,
                .grid_x             = col,
                .grid_y             = row,
                .composition        = comp[idx],
                .landform           = land[idx],
                .resource_deposit   = deposits,
                .resource_remaining = remaining,
                .hazard_level       = hazard,
                .habitability       = habitability,
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
            if (id != null_entity && w.tiles.at(id).composition != terrain_composition::ocean)
                result.push_back(id);
        }
    return result;
}
