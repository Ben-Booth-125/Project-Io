#include "ground_bake.hpp"

#include "terrain_palette.hpp"
#include "world/world.hpp"
#include "world/survey_system.hpp" // survey_tile_visible — the region mask (BL-067)
#include "world/hex_neighbors.hpp" // the shared odd-r side table — landform runs, river chains

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ui::ground {

namespace {

constexpr double kSqrt3 = 1.7320508075688772;

/// Lock fill for survey-masked ground — PLANETARY.md's flat dark "locked" value.
constexpr std::uint32_t k_lock_colour = palette::col32(12, 14, 20, 255);

/// Deterministic 2-D lattice hash → [0, 1). FNV-1a over the lattice coords plus
/// a salt — the same spatial idiom the tile texture pass uses (grid-keyed,
/// never screen-keyed), so nothing in the bake can crawl or disagree between
/// wrap copies.
float hash01(int x, int y, std::uint32_t salt)
{
    std::uint32_t h = 2166136261u;
    auto mix = [&h](std::uint32_t v) { h ^= v; h *= 16777619u; h ^= h >> 13; };
    mix(static_cast<std::uint32_t>(x) * 73856093u);
    mix(static_cast<std::uint32_t>(y) * 19349663u);
    mix(salt * 83492791u);
    h ^= h >> 16;
    return static_cast<float>(h & 0x00FFFFFFu) / 16777216.0f;
}

/// One octave of value noise on a lattice of @p cell canonical units, bilinear,
/// wrap-periodic in x with period @p period_cells lattice cells. Smoothstep on
/// the fractions keeps it C1 — a hard lattice reads as a grid, which is the one
/// thing the ground must never do again.
float value_noise(double x, double y, double cell, int period_cells, std::uint32_t salt)
{
    const double fx = x / cell, fy = y / cell;
    int ix = static_cast<int>(std::floor(fx));
    int iy = static_cast<int>(std::floor(fy));
    float tx = static_cast<float>(fx - ix);
    float ty = static_cast<float>(fy - iy);
    tx = tx * tx * (3.0f - 2.0f * tx);
    ty = ty * ty * (3.0f - 2.0f * ty);
    auto wrap = [&](int v) { return ((v % period_cells) + period_cells) % period_cells; };
    const float v00 = hash01(wrap(ix),     iy,     salt);
    const float v10 = hash01(wrap(ix + 1), iy,     salt);
    const float v01 = hash01(wrap(ix),     iy + 1, salt);
    const float v11 = hash01(wrap(ix + 1), iy + 1, salt);
    const float a = v00 + (v10 - v00) * tx;
    const float b = v01 + (v11 - v01) * tx;
    return a + (b - a) * ty; // [0, 1)
}

inline float height_at(const bake_source& s, int c, int r)
{
    const int cw = ((c % s.gw) + s.gw) % s.gw;
    const int rc = std::clamp(r, 0, s.gh - 1);
    const std::size_t i = static_cast<std::size_t>(rc) * s.gw + cw;
    // Water flattens to its own height so a shoreline's slope lives on the land
    // side; a void tile contributes the clamped neighbour instead.
    return s.height[i];
}

// ---------------------------------------------------------------------------
// Landform and river features (BL-1242) — shared geometry.
// ---------------------------------------------------------------------------

/// Centre-to-centre offset to the neighbour across hex side i, canonical units
/// (pointy-top, odd-r; side order 0=E, 1=NE, 2=NW, 3=W, 4=SW, 5=SE — the
/// hex_neighbors table). Row-parity independent, which is what lets a pass
/// carry a pixel's tile-relative position from one tile to the next exactly.
constexpr double kNbDx[6] = { kSqrt3, kSqrt3 * 0.5, -kSqrt3 * 0.5, -kSqrt3, -kSqrt3 * 0.5, kSqrt3 * 0.5 };
constexpr double kNbDy[6] = { 0.0, -1.5, -1.5, 0.0, 1.5, 1.5 };

constexpr std::uint8_t k_near_river    = 1u;
constexpr std::uint8_t k_near_landform = 2u;

/// Raster index of the neighbour across @p side of tile @p i, or -1 past the
/// top/bottom row. Columns wrap (the cylinder).
inline int nb_index(const bake_source& s, int i, int side)
{
    const int r = i / s.gw, c = i % s.gw;
    const hex_neighbors::coord nb = hex_neighbors::neighbour(c, r, side);
    if (nb.gy < 0 || nb.gy >= s.gh)
        return -1;
    const int cw = ((nb.gx % s.gw) + s.gw) % s.gw;
    return nb.gy * s.gw + cw;
}

/// The four landforms that bake a form of their own (the dramatic set,
/// PLANETARY.md § Terrain channels).
inline bool dramatic(std::uint8_t lf)
{
    const auto l = static_cast<terrain_landform>(lf);
    return l == terrain_landform::mountain || l == terrain_landform::canyon
        || l == terrain_landform::crater   || l == terrain_landform::rift;
}

/// Bridged-run links, accumulated river flow and the per-tile feature cull,
/// derived once per source. Pure function of the tile arrays.
void derive_features(bake_source& s, const std::vector<std::uint8_t>& raw_in,
                     const std::vector<std::uint8_t>& raw_out)
{
    const int n = s.gw * s.gh;
    const auto land = static_cast<std::uint8_t>(bake_source::tile_class::land);

    // Same-landform links. Crater never spans: a basin is a blob, not a line.
    for (int i = 0; i < n; ++i)
    {
        if (s.cls[i] != land || !dramatic(s.landform[i])
            || static_cast<terrain_landform>(s.landform[i]) == terrain_landform::crater)
            continue;
        std::uint8_t links = 0;
        for (int side = 0; side < 6; ++side)
        {
            const int j = nb_index(s, i, side);
            if (j >= 0 && s.cls[j] == land && s.landform[j] == s.landform[i])
                links |= static_cast<std::uint8_t>(1u << side);
        }
        s.lf_links[i] = links;
    }

    // Accumulated flow over the whole river graph: flow(t) = 1 + sum of the
    // flows of the tiles draining into it. The graph is a forest (the
    // generator lays it as a strictly-descending walk), walked iteratively in
    // post-order so a long river cannot blow the stack. Raster-order roots
    // make it deterministic.
    std::vector<std::uint8_t> state(static_cast<std::size_t>(n), 0); // 0 new, 1 open, 2 done
    std::vector<int> stack;
    for (int root = 0; root < n; ++root)
    {
        if (!(raw_in[root] | raw_out[root]) || state[root])
            continue;
        stack.push_back(root);
        while (!stack.empty())
        {
            const int t = stack.back();
            if (state[t] == 0)
            {
                state[t] = 1;
                for (int side = 0; side < 6; ++side)
                    if (raw_in[t] & (1u << side))
                    {
                        const int u = nb_index(s, t, side);
                        if (u >= 0 && state[u] == 0)
                            stack.push_back(u);
                    }
                continue;
            }
            stack.pop_back();
            if (state[t] == 2)
                continue;
            float f = 1.0f;
            for (int side = 0; side < 6; ++side)
                if (raw_in[t] & (1u << side))
                {
                    const int u = nb_index(s, t, side);
                    if (u >= 0 && state[u] == 2)
                        f += s.river_flow[u];
                }
            s.river_flow[t] = f;
            state[t] = 2;
        }
    }

    // The cull: a pixel only pays for a pass when its owner tile or one of its
    // neighbours carries the feature (every form stays within one tile of its
    // skeleton, so this is exact, not a heuristic).
    for (int i = 0; i < n; ++i)
    {
        std::uint8_t bits = 0;
        const auto mark = [&](int j)
        {
            if (j < 0)
                return;
            if (s.river_in[j] | s.river_out[j])
                bits |= k_near_river;
            if (s.cls[j] == land && dramatic(s.landform[j]))
                bits |= k_near_landform;
        };
        mark(i);
        for (int side = 0; side < 6; ++side)
            mark(nb_index(s, i, side));
        s.near_feature[i] = bits;
    }
}

/// Anisotropic value noise (BL-1243): cell @p cu along the periodic coordinate
/// u (@p period_cells lattice cells per wrap) and @p cv along v. A streak
/// field along direction (k, 1) samples u = x - k*y, v = y: the shear moves
/// only u's offset, so the field stays exactly periodic in x.
float value_noise_aniso(double u, double v, double cu, int period_cells, double cv,
                        std::uint32_t salt)
{
    const double fu = u / cu, fv = v / cv;
    const int iu = static_cast<int>(std::floor(fu));
    const int iv = static_cast<int>(std::floor(fv));
    float tu = static_cast<float>(fu - iu);
    float tv = static_cast<float>(fv - iv);
    tu = tu * tu * (3.0f - 2.0f * tu);
    tv = tv * tv * (3.0f - 2.0f * tv);
    auto wrap = [&](int q) { return ((q % period_cells) + period_cells) % period_cells; };
    const float v00 = hash01(wrap(iu),     iv,     salt);
    const float v10 = hash01(wrap(iu + 1), iv,     salt);
    const float v01 = hash01(wrap(iu),     iv + 1, salt);
    const float v11 = hash01(wrap(iu + 1), iv + 1, salt);
    const float a = v00 + (v10 - v00) * tu;
    const float b = v01 + (v11 - v01) * tu;
    return a + (b - a) * tv;
}

// ---------------------------------------------------------------------------
// Terrain variant families (BL-1243, RENDERING.md § Mountains, rivers and
// terrain variety — "More tile sets"). A family is a substrate x cover pair
// grouped by how its ground reads; each carries k_variant_count procedural
// variants. A variant is a VECTOR of character — a tonal and hue shift, and a
// mix over a small bank of shared texture fields (broad patches, three
// directions of streak, crack lines, close-tier stipple), plus the relief
// roughness and the rock threshold. Because the fields are shared and only
// their mix varies, a pixel blends the vectors of the tiles around it (the
// same interpolation the colour uses, with a softer, wider falloff) and the
// variants CROSS-FADE: no variant edge is ever drawn, and the cost is fixed
// whatever the number of variants a pixel sees.
// ---------------------------------------------------------------------------

enum vfam : std::uint8_t
{
    vf_grass = 0, vf_scrub, vf_forest, vf_marsh, vf_bare, vf_sand, vf_volcanic, vf_snow, vf_urban,
    vf_count,
    vf_none = 255 ///< Water, masked, void: the neutral vector.
};

enum vparam_slot : int
{
    vp_tone = 0,   ///< Luminance offset.
    vp_warm,       ///< Warm (+) / cool (-) hue drift.
    vp_patch,      ///< Broad tonal patches (clearings, thickets, damp hollows).
    vp_patch_hue,  ///< How far the patches shift hue (greener + / drier -).
    vp_streak_h,   ///< Streaks along x (strata, wind-combed sward, sastrugi).
    vp_streak_a,   ///< Streaks along (+0.577, 1) — gullies, ripples, flows.
    vp_streak_b,   ///< Streaks along (-0.577, 1).
    vp_crack,      ///< Crack lines (plates, crevasses, fissured ash). Never on a vegetated
                   ///< family: on grass a crack reads as contour ink.
    vp_stipple,    ///< Close-tier stipple (gravel, tussock, undergrowth).
    vp_detail,     ///< Relief roughness multiplier (1 = the family's base).
    vp_rock,       ///< Rock-exposure threshold shift (+ = rock shows sooner).
};
static_assert(vp_rock + 1 == k_vparam_count, "vparam slots and k_vparam_count disagree");

/// The neutral vector: what a tile bakes with variant_strength 0.
constexpr float k_vneutral[k_vparam_count] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 };

/// [family][variant][slot]. Set by eye against the terrain_variants captures.
/// Amplitudes are luminance fractions; the tonal swing is held to a few
/// percent so a variant reads as the same terrain, never as another one.
constexpr float k_vtable[vf_count][k_variant_count][k_vparam_count] = {
    //  tone     warm   patch  phue    st_h   st_a   st_b   crack  stip   detail rock
    { // grass — meadow, dry steppe (wind-combed), hummocky, tussock sward
        {  0.050f, -1.0f, 0.12f,  1.2f,  0.00f, 0.00f, 0.00f, 0.00f, 0.04f, 0.55f, 0.00f },
        {  0.070f,  2.0f, 0.06f, -1.0f,  0.06f, 0.00f, 0.00f, 0.00f, 0.05f, 0.45f, 0.00f },
        { -0.060f,  0.0f, 0.16f,  0.6f,  0.00f, 0.00f, 0.00f, 0.00f, 0.12f, 1.80f, 0.05f },
        { -0.015f,  0.8f, 0.10f, -0.4f,  0.00f, 0.05f, 0.00f, 0.00f, 0.08f, 1.25f, 0.03f },
    },
    { // scrub — heath, thicket, dry brush with gullies, combed scrub
        {  0.035f,  0.9f, 0.12f, -0.5f,  0.00f, 0.00f, 0.00f, 0.00f, 0.08f, 0.80f, 0.00f },
        { -0.050f, -0.4f, 0.15f,  0.8f,  0.00f, 0.00f, 0.00f, 0.00f, 0.10f, 1.30f, 0.00f },
        {  0.050f,  1.4f, 0.09f, -0.6f,  0.00f, 0.00f, 0.06f, 0.00f, 0.06f, 0.90f, 0.05f },
        {  0.000f,  0.0f, 0.10f,  0.3f,  0.045f,0.00f, 0.00f, 0.00f, 0.12f, 1.10f, 0.00f },
    },
    { // forest — dense dark stand, mixed (warm-tinged), glades, young stand
        { -0.060f, -0.8f, 0.07f,  0.5f,  0.00f, 0.00f, 0.00f, 0.00f, 0.06f, 1.00f, 0.00f },
        {  0.000f,  1.3f, 0.11f, -0.8f,  0.00f, 0.00f, 0.00f, 0.00f, 0.08f, 1.15f, 0.00f },
        {  0.045f,  0.2f, 0.17f,  0.9f,  0.00f, 0.00f, 0.00f, 0.00f, 0.05f, 0.85f, 0.00f },
        {  0.015f, -0.2f, 0.09f,  0.3f,  0.00f, 0.00f, 0.00f, 0.00f, 0.12f, 1.20f, 0.00f },
    },
    { // marsh — pools and channels, reedbed, sedge flats, fen
        { -0.045f, -0.5f, 0.17f,  0.8f,  0.045f,0.00f, 0.00f, 0.00f, 0.04f, 0.70f, 0.00f },
        {  0.030f,  0.9f, 0.12f, -0.6f,  0.00f, 0.00f, 0.00f, 0.00f, 0.08f, 0.85f, 0.00f },
        { -0.015f,  0.0f, 0.15f,  0.3f,  0.00f, 0.045f,0.00f, 0.00f, 0.06f, 0.75f, 0.00f },
        {  0.015f, -1.0f, 0.13f,  0.9f,  0.00f, 0.00f, 0.045f,0.00f, 0.05f, 0.80f, 0.00f },
    },
    { // bare ground (rocky, barren, sedimentary, metallic) — cracked plates, strata, gullied, weathered
        {  0.000f, -0.4f, 0.07f,  0.0f,  0.00f, 0.00f, 0.00f, 0.14f, 0.06f, 1.00f, 0.05f },
        {  0.030f,  0.9f, 0.06f,  0.0f,  0.11f, 0.00f, 0.00f, 0.00f, 0.05f, 1.15f, 0.00f },
        { -0.045f,  0.0f, 0.07f,  0.0f,  0.00f, 0.10f, 0.00f, 0.04f, 0.05f, 1.45f, 0.15f },
        {  0.045f,  1.2f, 0.13f,  0.0f,  0.00f, 0.00f, 0.03f, 0.00f, 0.08f, 0.70f, -0.05f },
    },
    { // sand (dunes, salt, regolith) — ripples three ways, a flat pan
        {  0.030f,  0.7f, 0.06f,  0.0f,  0.00f, 0.09f, 0.00f, 0.00f, 0.04f, 0.85f, 0.00f },
        {  0.000f,  0.2f, 0.06f,  0.0f,  0.00f, 0.00f, 0.09f, 0.00f, 0.04f, 0.85f, 0.00f },
        { -0.030f,  1.0f, 0.07f,  0.0f,  0.08f, 0.00f, 0.00f, 0.00f, 0.05f, 1.00f, 0.00f },
        {  0.045f, -0.4f, 0.11f,  0.0f,  0.00f, 0.00f, 0.00f, 0.06f, 0.07f, 0.65f, 0.00f },
    },
    { // volcanic (volcanic substrate, ash) — flows, cinder field, ash plain, fissured
        { -0.055f, -0.3f, 0.07f,  0.0f,  0.00f, 0.10f, 0.00f, 0.00f, 0.05f, 1.15f, 0.00f },
        {  0.000f,  0.6f, 0.06f,  0.0f,  0.00f, 0.00f, 0.00f, 0.08f, 0.14f, 1.30f, 0.05f },
        {  0.045f, -0.5f, 0.11f,  0.0f,  0.00f, 0.00f, 0.00f, 0.00f, 0.05f, 0.80f, 0.00f },
        { -0.030f,  0.2f, 0.07f,  0.0f,  0.00f, 0.00f, 0.06f, 0.14f, 0.06f, 1.20f, 0.10f },
    },
    { // snow and ice — sastrugi, drift, crevassed (blue), smooth
        {  0.030f, -0.4f, 0.06f,  0.0f,  0.07f, 0.00f, 0.00f, 0.00f, 0.03f, 0.85f, 0.00f },
        {  0.040f, -0.1f, 0.05f,  0.0f,  0.00f, 0.06f, 0.00f, 0.00f, 0.03f, 1.00f, 0.00f },
        { -0.030f, -1.0f, 0.07f,  0.0f,  0.00f, 0.00f, 0.00f, 0.08f, 0.04f, 1.15f, 0.05f },
        {  0.000f, -0.3f, 0.08f,  0.0f,  0.00f, 0.00f, 0.045f,0.00f, 0.03f, 0.75f, 0.00f },
    },
    { // urban — the structures carry the read; only a whisper of tone
        {  0.015f,  0.2f, 0.03f,  0.0f,  0.00f, 0.00f, 0.00f, 0.00f, 0.03f, 1.00f, 0.00f },
        { -0.015f, -0.2f, 0.03f,  0.0f,  0.00f, 0.00f, 0.00f, 0.00f, 0.03f, 1.00f, 0.00f },
        {  0.000f,  0.4f, 0.04f,  0.0f,  0.00f, 0.00f, 0.00f, 0.00f, 0.03f, 1.00f, 0.00f },
        {  0.010f, -0.4f, 0.03f,  0.0f,  0.00f, 0.00f, 0.00f, 0.00f, 0.03f, 1.00f, 0.00f },
    },
};

/// Which family a revealed land tile's ground belongs to.
std::uint8_t variant_family(terrain_substrate sub, terrain_cover cov)
{
    switch (cov)
    {
    case terrain_cover::grass:  return vf_grass;
    case terrain_cover::scrub:  return vf_scrub;
    case terrain_cover::forest: return vf_forest;
    case terrain_cover::marsh:  return vf_marsh;
    case terrain_cover::snow:   return vf_snow;
    case terrain_cover::dunes:
    case terrain_cover::salt:   return vf_sand;
    case terrain_cover::ash:    return vf_volcanic;
    case terrain_cover::urban:  return vf_urban;
    default: break;
    }
    switch (sub)
    {
    case terrain_substrate::icy:      return vf_snow;
    case terrain_substrate::regolith: return vf_sand;
    case terrain_substrate::volcanic: return vf_volcanic;
    default:                          return vf_bare;
    }
}

/// Tree scatter per forest/scrub variant: {count multiplier, clumping toward a
/// grove centre (glades open where it pulls the trees away), canopy size,
/// warm tint}. Row 0 of a neutral tile is {1, 0, 1, 0}.
constexpr float k_tree_var[k_variant_count][4] = {
    { 1.15f, 0.00f, 1.00f, -0.4f }, // dense dark stand
    { 1.00f, 0.15f, 1.08f,  0.8f }, // mixed, warm-tinged
    { 0.75f, 0.45f, 1.00f,  0.1f }, // glades
    { 1.25f, 0.10f, 0.82f, -0.1f }, // young stand
};

/// The landform forms' variants: five floats each, read per landform —
///   mountain {crest mix (broad -> close-set peaks), spur, crag, rock tint (-grey/+warm), rock cover}
///   canyon   {strata bands, warm rock, floor depth, rim height, -}
///   rift     {fissure width, scorch, -, -, -}
///   crater   {ejecta, bowl depth, rim height, -, -}
/// k_lf_neutral is the BL-1242 form exactly (variant_strength 0).
constexpr int k_lf_params = 5;
constexpr float k_lf_neutral[4][k_lf_params] = {
    { 0.0f, 1.0f, 1.0f, 0.0f, 1.0f },
    { 5.0f, 1.0f, 1.0f, 1.0f, 0.0f },
    { 1.0f, 1.0f, 0.0f, 0.0f, 0.0f },
    { 1.0f, 1.0f, 1.0f, 0.0f, 0.0f },
};
constexpr float k_lf_var[4][k_variant_count][k_lf_params] = {
    { { 0.0f, 1.00f, 1.0f,  0.0f, 1.00f },   // mountain: alpine
      { 1.0f, 1.40f, 1.5f, -0.6f, 1.10f },   //           jagged, grey
      { 0.3f, 0.55f, 0.5f,  0.7f, 0.80f },   //           worn, warm
      { 0.6f, 1.10f, 0.9f,  0.2f, 1.00f } }, //           broken
    { { 5.0f, 1.00f, 1.0f,  1.0f, 0.0f },    // canyon: banded
      { 3.5f, 1.25f, 1.1f,  1.2f, 0.0f },    //         broad red beds
      { 7.0f, 0.80f, 0.9f,  0.8f, 0.0f },    //         fine pale beds
      { 4.2f, 1.10f, 1.2f,  1.0f, 0.0f } },  //         deep
    { { 1.00f, 1.00f, 0, 0, 0 },             // rift
      { 1.25f, 1.20f, 0, 0, 0 },
      { 0.80f, 0.85f, 0, 0, 0 },
      { 1.10f, 1.35f, 0, 0, 0 } },
    { { 1.00f, 1.00f, 1.00f, 0, 0 },         // crater
      { 1.40f, 0.90f, 1.10f, 0, 0 },
      { 0.70f, 1.20f, 0.90f, 0, 0 },
      { 1.15f, 1.05f, 1.25f, 0, 0 } },
};

/// The grid's variant colouring: raster order, each tile taking its hashed
/// preference unless an already-assigned neighbour holds it (W, NW, NE; on
/// the last column also E, across the wrap), else the next free index — so
/// no two neighbours share a variant (bar the rare wrap-corner case where all
/// four are taken). A pure function of the grid dimensions: it never moves
/// with terrain or the survey, and carries no information about either.
void assign_variants(bake_source& s)
{
    const int n = s.gw * s.gh;
    s.variant.assign(static_cast<std::size_t>(n), 0);
    for (int r = 0; r < s.gh; ++r)
        for (int c = 0; c < s.gw; ++c)
        {
            const int i = r * s.gw + c;
            const auto held = [&](int side) -> unsigned
            {
                const int j = nb_index(s, i, side);
                return j >= 0 ? 1u << s.variant[static_cast<std::size_t>(j)] : 0u;
            };
            // W (unassigned on column 0 — its west is the last column), NW
            // and NE (the row above: always assigned). Three neighbours, four
            // indices: a free one always exists.
            const unsigned used  = (c > 0 ? held(3) : 0u) | held(2) | held(1);
            // The last column also meets column 0 across the wrap; honour it
            // when an index is left, else the wrap seam is the one place a
            // pair may share.
            const unsigned used_e = c == s.gw - 1 ? (used | held(0)) : used;
            const int pref = static_cast<int>(hash01(c, r, 0x7A41u) * k_variant_count) % k_variant_count;
            const auto first_free = [&](unsigned mask) -> int
            {
                for (int k = 0; k < k_variant_count; ++k)
                {
                    const int v = (pref + k) % k_variant_count;
                    if (!(mask & (1u << v)))
                        return v;
                }
                return -1;
            };
            int pick = first_free(used_e);
            if (pick < 0)
                pick = first_free(used);
            s.variant[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(pick);
        }
}

} // namespace

geometry make_geometry(int gw, int gh, double target_px_per_r, double tilt_sy)
{
    geometry g;
    g.gw = gw;
    g.gh = gh;
    const double period = gw * kSqrt3; // canonical wrap period
    g.W = std::max(1, static_cast<int>(std::lround(period * target_px_per_r)));
    g.s = g.W / period; // re-derived so wrap copies abut exactly
    g.tilt_sy = std::clamp(tilt_sy, 0.3, 1.0);
    if (g.tilt_sy < 1.0)
    {
        const double tilt = std::acos(g.tilt_sy);
        g.lift = 0.9 * std::tan(tilt);
    }
    // Vertical extent: hex tops of row 0 reach y = -1, hex bottoms of the last
    // row reach 1.5 * (gh - 1) + 1. A tilted bake additionally holds displaced
    // peaks and standing trees above row 0 (lift + tree headroom).
    g.y_min = -1.0 - (g.tilt_sy < 1.0 ? g.lift + 0.5 / g.tilt_sy : 0.0);
    const double y_max = 1.5 * (gh - 1) + 1.0;
    g.H = std::max(1, static_cast<int>(std::ceil((y_max - g.y_min) * g.s)));
    return g;
}

bake_source prepare_source(const world& w, entity_id body, bool reveal_all,
                           const recipe_registry* reg)
{
    bake_source s;
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end())
        return s;
    const body_component& b = bit->second;
    s.gw = b.grid_width;
    s.gh = b.grid_height;
    const std::size_t n = static_cast<std::size_t>(s.gw) * s.gh;
    s.cls.assign(n, static_cast<std::uint8_t>(bake_source::tile_class::void_));
    s.colour.assign(n, 0u);
    s.height.assign(n, 0.0f);
    s.grad_x.assign(n, 0.0f);
    s.grad_y.assign(n, 0.0f);
    s.relief_bias.assign(n, 0.0f);
    s.jitter.assign(n, 0.0f);
    s.cover.assign(n, static_cast<std::uint8_t>(terrain_cover::none));
    s.density.assign(n, 0);
    s.landform.assign(n, static_cast<std::uint8_t>(terrain_landform::plains));
    s.lf_links.assign(n, 0);
    s.river_in.assign(n, 0);
    s.river_out.assign(n, 0);
    s.river_flow.assign(n, 0.0f);
    s.near_feature.assign(n, 0);
    // BL-1243: the variant colouring is the grid's alone; family and vector
    // are filled per revealed land tile below (everything else stays neutral).
    assign_variants(s);
    s.family.assign(n, static_cast<std::uint8_t>(vf_none));
    s.vparam.resize(n * k_vparam_count);
    for (std::size_t i = 0; i < n; ++i)
        std::memcpy(&s.vparam[i * k_vparam_count], k_vneutral, sizeof k_vneutral);
    // The river graph whole, mask-blind: accumulated flow is a property of the
    // river, and a visible reach's width must not change when ground upstream
    // is surveyed.
    std::vector<std::uint8_t> raw_in(n, 0), raw_out(n, 0);

    for (const auto& [id, t] : w.tiles)
    {
        if (t.body != body)
            continue;
        if (t.grid_x < 0 || t.grid_x >= s.gw || t.grid_y < 0 || t.grid_y >= s.gh)
            continue;
        const std::size_t i = static_cast<std::size_t>(t.grid_y) * s.gw + t.grid_x;
        raw_in[i]  = static_cast<std::uint8_t>(t.river_edges & ~t.river_downstream & 0x3Fu);
        raw_out[i] = static_cast<std::uint8_t>(t.river_edges &  t.river_downstream & 0x3Fu);

        const bool water = t.substrate == terrain_substrate::ocean
                        || t.substrate == terrain_substrate::coast
                        || t.substrate == terrain_substrate::lake;
        const bool seen  = reveal_all
                        || survey_tile_visible(b.survey, s.gw, s.gh, t.grid_x, t.grid_y);

        if (!seen)
        {
            // Masked ground bakes as the flat lock colour and joins no blend:
            // the lock fill is a statement about knowledge, not terrain, and an
            // interpolated lock edge would leak the shape of unsurveyed ground.
            s.cls[i]    = static_cast<std::uint8_t>(bake_source::tile_class::masked);
            s.colour[i] = k_lock_colour;
            continue;
        }

        s.cls[i]    = static_cast<std::uint8_t>(water ? bake_source::tile_class::water
                                                      : bake_source::tile_class::land);
        s.colour[i] = palette::tile_colour(t.substrate, t.cover, t.cover_density);
        s.height[i] = t.height;
        s.relief_bias[i] = palette::relief_amount(t.landform);
        s.jitter[i] = hash01(t.grid_x, t.grid_y, 0xB732u) * 2.0f - 1.0f;
        s.cover[i]   = static_cast<std::uint8_t>(t.cover);
        s.density[i] = t.cover_density;
        s.landform[i]  = static_cast<std::uint8_t>(t.landform);
        s.river_in[i]  = raw_in[i];
        s.river_out[i] = raw_out[i];
        if (!water)
        {
            const std::uint8_t fam = variant_family(t.substrate, t.cover);
            s.family[i] = fam;
            std::memcpy(&s.vparam[i * k_vparam_count], k_vtable[fam][s.variant[i]],
                        sizeof(float) * k_vparam_count);
        }
    }
    derive_features(s, raw_in, raw_out);

    // Height gradient from neighbour differences — symmetric central
    // differences over the raster (columns wrap, rows clamp). Computed on the
    // tile grain and interpolated per pixel, which is both cheaper and smoother
    // than per-pixel finite differences of the interpolated field.
    for (int r = 0; r < s.gh; ++r)
        for (int c = 0; c < s.gw; ++c)
        {
            const std::size_t i = static_cast<std::size_t>(r) * s.gw + c;
            // col_step is sqrt(3) canonical units; row_step 1.5.
            s.grad_x[i] = static_cast<float>(
                (height_at(s, c + 1, r) - height_at(s, c - 1, r)) / (2.0 * kSqrt3));
            s.grad_y[i] = static_cast<float>(
                (height_at(s, c, r + 1) - height_at(s, c, r - 1)) / (2.0 * 1.5));
        }
    extract_installations(w, body, reg, s); // BL-1241 (structures baked)
    return s;
}

namespace {

/// Stamp individual tree canopies over the baked ground (the close tiers'
/// "individual trees"). Forest and scrub tiles scatter hash-positioned
/// canopies — count from cover density, positions/sizes/tints per-tile-hashed
/// so every wrap copy and every chunk agrees; the window is walked with a
/// margin so a canopy spanning a chunk edge renders identically in both
/// chunks. Each tree is a soft drop shadow toward the SE plus a canopy blob
/// lit from the NW — the same light every other pass uses. Stamps respect the
/// tag buffer: a lock-fill or transparent pixel is never painted, so nothing
/// leaks through the survey mask.
void stamp_trees(const bake_source& src, const geometry& g, const bake_params& p,
                 int px0, int py0, int pw, int ph, std::uint32_t* out,
                 const std::uint8_t* tag)
{
    // An (optionally elliptical) soft blob: bry > brx bakes the pre-stretched
    // verticals an oblique geometry needs, so a canopy squashes back to round
    // under the camera. The distance metric scales dy into the x radius, so
    // the AA edge stays ~1 px on every axis.
    //
    // The centre (bx, by) is in ABSOLUTE bake pixels, and each pixel's offset
    // from it is taken from its absolute index (BL-1243): a centre held
    // relative to the window origin rounds differently in two windows, which
    // moved a canopy's anti-aliased rim by a level across a chunk edge.
    const auto blob = [&](double bx, double by, float brx, float bry,
                          float cr2, float cg2, float cb2, float alpha, bool lit)
    {
        const double lbx = bx - px0, lby = by - py0; // window-local, bounds only
        const int x0i = std::max(0, static_cast<int>(std::floor(lbx - brx - 1.0)));
        const int x1i = std::min(pw - 1, static_cast<int>(std::ceil(lbx + brx + 1.0)));
        const int y0i = std::max(0, static_cast<int>(std::floor(lby - bry - 1.0)));
        const int y1i = std::min(ph - 1, static_cast<int>(std::ceil(lby + bry + 1.0)));
        if (bry <= 0.0f || brx <= 0.0f)
            return;
        const float yscale = brx / bry;
        for (int py_ = y0i; py_ <= y1i; ++py_)
            for (int px_ = x0i; px_ <= x1i; ++px_)
            {
                const std::size_t idx = static_cast<std::size_t>(py_) * pw + px_;
                if (!tag[idx])
                    continue; // lock fill / transparent margin stays untouched
                const float dx = static_cast<float>((px0 + px_ + 0.5) - bx);
                const float dy = static_cast<float>((py0 + py_ + 0.5) - by) * yscale;
                const float d  = std::sqrt(dx * dx + dy * dy);
                if (d >= brx + 0.8f)
                    continue;
                const float a = alpha * std::clamp((brx + 0.8f - d) / 1.6f, 0.0f, 1.0f);
                float rr = cr2, gg = cg2, bb = cb2;
                if (lit)
                {
                    // Highlight offset toward the NW light, shadowed SE rim.
                    const float lx = dx + brx * 0.35f, ly = dy + brx * 0.35f;
                    const float lt = std::clamp(
                        1.28f - 0.75f * std::sqrt(lx * lx + ly * ly) / brx, 0.55f, 1.28f);
                    rr *= lt; gg *= lt; bb *= lt;
                }
                std::uint32_t& dst = out[idx];
                const float ir = static_cast<float>(palette::col_r(dst));
                const float ig = static_cast<float>(palette::col_g(dst));
                const float ib = static_cast<float>(palette::col_b(dst));
                dst = palette::col32(
                    std::clamp(static_cast<int>(ir + (rr - ir) * a + 0.5f), 0, 255),
                    std::clamp(static_cast<int>(ig + (gg - ig) * a + 0.5f), 0, 255),
                    std::clamp(static_cast<int>(ib + (bb - ib) * a + 0.5f), 0, 255), 255);
            }
    };

    // Max canopy + shadow reach, canonical units; a tilted bake's standing
    // trees additionally rise by the height displacement plus their own
    // stretched height, so the walk margin grows with them.
    const double margin = 0.6 + (g.lift > 0.0 ? g.lift + 1.4 / g.tilt_sy : 0.0);
    const double wx0 = px0 / g.s - margin, wx1 = (px0 + pw) / g.s + margin;
    const double wy0 = py0 / g.s + g.y_min - margin;
    const double wy1 = (py0 + ph) / g.s + g.y_min + margin;
    const int r_lo = std::max(0, static_cast<int>(std::floor(wy0 / 1.5)));
    const int r_hi = std::min(src.gh - 1, static_cast<int>(std::ceil(wy1 / 1.5)));
    for (int r = r_lo; r <= r_hi; ++r)
    {
        const double odd = (r & 1) ? 0.5 : 0.0;
        const int c_lo = static_cast<int>(std::floor(wx0 / kSqrt3 - odd)) - 1;
        const int c_hi = static_cast<int>(std::ceil (wx1 / kSqrt3 - odd)) + 1;
        for (int c = c_lo; c <= c_hi; ++c)
        {
            const int cw = ((c % src.gw) + src.gw) % src.gw;
            const std::size_t i = static_cast<std::size_t>(r) * src.gw + cw;
            if (src.cls[i] != static_cast<std::uint8_t>(bake_source::tile_class::land))
                continue;
            const auto cov = static_cast<terrain_cover>(src.cover[i]);
            const bool forest = cov == terrain_cover::forest;
            const bool scrub  = cov == terrain_cover::scrub;
            if (!forest && !scrub)
                continue;
            const float dens = src.density[i] / 255.0f;
            // BL-1243: the tile's variant scatters its own stand — denser or
            // sparser, groved around a clearing, larger or younger crowns,
            // warmer or darker leaf. variant_strength 0 is the neutral stand.
            const float* tv = k_tree_var[src.variant[i]];
            const float  vs = p.variant_strength;
            const float  v_count = 1.0f + (tv[0] - 1.0f) * vs;
            const float  v_clump = tv[1] * vs;
            const float  v_size  = 1.0f + (tv[2] - 1.0f) * vs;
            const float  v_warm  = tv[3] * vs;
            const int   n = static_cast<int>(std::lround(
                (forest ? 6.0f + 13.0f * dens : 2.0f + 4.0f * dens) * p.tree_density * v_count));
            // The grove centre a clumped stand gathers around (tile-relative).
            const double g_ang = hash01(cw, r, 0x7E90u) * 6.283185307;
            const double g_rad = 0.45 * hash01(cw, r, 0x7E91u); // off-centre, so groves do not sit on the lattice
            const double g_x = g_rad * std::cos(g_ang), g_y = g_rad * std::sin(g_ang) * 0.9;
            // Hashes key on the WRAPPED coordinate, positions on the unwrapped
            // centre: every wrap copy grows the same trees in the same places.
            const double hx = kSqrt3 * (c + odd);
            const double hy = 1.5 * r;
            const std::uint32_t tc = src.colour[i];
            const float clear_r = installation_clear_radius(src, i); // BL-1241: works stand on cleared ground
            for (int k = 0; k < n; ++k)
            {
                const float a1 = hash01(cw, r, 0x7E00u + static_cast<std::uint32_t>(k) * 3u);
                const float a2 = hash01(cw, r, 0x7E01u + static_cast<std::uint32_t>(k) * 3u);
                const float a3 = hash01(cw, r, 0x7E02u + static_cast<std::uint32_t>(k) * 3u);
                const double ang = a1 * 6.283185307;
                const double rad = 0.82 * std::sqrt(a2);
                if (rad < clear_r)
                    continue;
                double ox_ = rad * std::cos(ang);
                double oy_ = rad * std::sin(ang) * 0.9;
                if (v_clump > 0.0f)
                {
                    // Pull toward the grove: the stand gathers, a glade opens.
                    ox_ += (g_x + 0.45 * ox_ - ox_) * v_clump;
                    oy_ += (g_y + 0.45 * oy_ - oy_) * v_clump;
                    if (std::sqrt(ox_ * ox_ + (oy_ / 0.9) * (oy_ / 0.9)) < clear_r)
                        continue; // the grove still keeps clear of the works
                }
                const double tx  = hx + ox_;
                const double ty  = hy + oy_;
                const float  cr  = (0.085f + 0.055f * a3) * (forest ? 1.0f : 0.62f) * v_size;
                const double pxc = tx * g.s;               // absolute bake pixels
                const double pyc = (ty - g.y_min) * g.s;
                const float  pr  = cr * static_cast<float>(g.s);
                // Canopy ink: the tile's own colour pushed toward deep leaf,
                // varied per tree so a wood is a crowd, not a pattern.
                const float vr = 0.86f + 0.28f * hash01(cw, r, 0x7F00u + static_cast<std::uint32_t>(k));
                const float cr_ = (palette::col_r(tc) * 0.45f + 20.0f * 0.55f) * vr * (1.0f + 0.10f * v_warm);
                const float cg_ = (palette::col_g(tc) * 0.45f + 62.0f * 0.55f) * vr * (1.0f + 0.01f * v_warm);
                const float cb_ = (palette::col_b(tc) * 0.45f + 26.0f * 0.55f) * vr * (1.0f - 0.08f * v_warm);
                if (g.lift > 0.0)
                {
                    // OBLIQUE: the tree STANDS. Its ground point rides the
                    // height displacement; the trunk and canopy bake with
                    // verticals stretched by 1/tilt_sy so the camera squash
                    // returns them to true proportion. Shadow stays on the
                    // ground plane — the depth cue that sells the tilt.
                    const float invsy = static_cast<float>(1.0 / g.tilt_sy);
                    const double ygr  = (ty - src.height[i] * g.lift - g.y_min) * g.s;
                    const float th = pr * 1.1f * invsy; // trunk height, px
                    blob(pxc + pr * 0.35f, ygr + pr * 0.22f, pr * 0.95f, pr * 0.62f,
                         10.0f, 14.0f, 10.0f, 0.30f, false);         // ground shadow
                    blob(pxc, ygr - th * 0.5f, std::max(1.2f, pr * 0.16f), th * 0.5f,
                         46.0f, 36.0f, 26.0f, 0.90f, false);         // trunk
                    blob(pxc, ygr - th - pr * invsy * 0.75f, pr, pr * invsy,
                         cr_, cg_, cb_, 0.95f, true);                // canopy, upright
                }
                else
                {
                    blob(pxc + pr * 0.45f, pyc + pr * 0.42f, pr * 1.0f, pr * 1.0f,
                         10.0f, 14.0f, 10.0f, 0.30f, false);         // drop shadow, SE
                    blob(pxc, pyc, pr, pr, cr_, cg_, cb_, 0.94f, true); // canopy, lit NW
                }
            }
        }
    }
}

} // namespace

namespace {

/// Where a baked pixel stands on the ground (BL-1242): its owner tile and its
/// position relative to that tile's centre, canonical units, at the WARPED and
/// (on an oblique tier) height-displaced sample point — so a feature pass draws
/// on the same organic edges the ground resolved, rides the lifted terrain, and
/// is wrap-exact (relative to a wrapped owner, never an absolute x).
struct feature_px
{
    float rx = 0.0f, ry = 0.0f;
    std::int32_t owner = -1; ///< -1: no ground here (margin, lock fill).
};

/// The per-pixel base bake for one window: interpolated colour, hillshade,
/// grain, mottle — UNGRADED, stamps and post passes are the orchestrator's
/// (bake_region below). Fills @p tag (1 = a terrain pixel later passes may
/// touch; 0 = transparent margin or the survey lock fill, which must stay
/// EXACT) and @p cover_out (0 = untouchable; 100 = water; 1+cover = land
/// cover class — the edge-ink pass reads it, and because it is resolved at
/// the WARPED sample point, an inked boundary follows the organic edge for
/// free).
void bake_window(const bake_source& src, const geometry& g, const bake_params& p,
                 int px0, int py0, int pw, int ph, std::uint32_t* out,
                 std::uint8_t* tag, std::uint8_t* cover_out, feature_px* fpx)
{
    const double period   = g.gw * kSqrt3;
    // Resolution-adaptive character (wave 2). The interpolation radius and the
    // detail amplitudes are CANONICAL-scale, so the same numbers that read as
    // painterly at the 24 px tier read as plain blur at 48/96 — a colour field
    // 1.4 tiles soft is 70 px soft up close. As the bake resolution grows past
    // the play tier, tighten the field and lift the detail: res_t is 0 at
    // 24 px/r and 1 at 96.
    const float  res_t = static_cast<float>(std::clamp((nominal_s(g) - 24.0) / 48.0, 0.0, 1.0));
    const double R     = p.blend_radius * (1.0 - 0.22 * res_t); // stays > 1 (corner coverage)
    const double R2    = R * R;
    const float  detail_mul = 1.0f + 1.1f * res_t;
    const float  noise_mul  = 1.0f + 1.2f * res_t;
    // Weight EXPONENT sharpening: the coverage radius cannot drop below one
    // tile, so patch crispness at the close tiers comes from steepening the
    // falloff instead — a higher power hands the pixel to its nearest centre
    // and the colour field stops reading as mist without losing coverage.
    const int wpow = 2 + static_cast<int>(std::lround(5.0f * res_t));
    // Light from the north-west, the hillshade convention every panel-C read
    // leans on (and the same warm-up/cool-down direction the relief tint set).
    const float Lx = -0.554700196f, Ly = -0.832050323f;

    // Noise lattices must divide the wrap period EXACTLY or the grain carries a
    // seam at the cylinder join: pick the cell count nearest the target size,
    // then re-derive the cell from the period.
    const auto periodic_cell = [&](double target, int& cells_out) -> double
    {
        cells_out = std::max(1, static_cast<int>(std::lround(period / target)));
        return period / cells_out;
    };
    int noise_cells_a, noise_cells_b, warp_cells, warp_cells2, detail_cells, detail_cells2;
    const double noise_cell_a = periodic_cell(0.90, noise_cells_a);
    const double noise_cell_b = periodic_cell(0.37, noise_cells_b);
    const double warp_cell    = periodic_cell(p.warp_cell, warp_cells);
    const double warp_cell2   = periodic_cell(p.warp_cell * 0.29, warp_cells2);
    const double detail_cell  = periodic_cell(p.detail_cell, detail_cells);
    const double detail_cell2 = periodic_cell(p.detail_cell * 0.41, detail_cells2);
    // Close-tier grain: at high bake resolutions (the 48/96 px zoom tiers) the
    // standard octaves span many texels and the ground reads under-detailed up
    // close — one finer octave keys in on resolution alone.
    int fine_cells = 1;
    const double fine_cell = periodic_cell(0.155, fine_cells);
    const bool   fine_on   = nominal_s(g) >= 40.0;

    // Terrain variant families (BL-1243). The variant vectors blend with
    // their OWN falloff — (Rv^2 - d^2)^2, Rv = 1.5, wider and softer than the
    // colour's sharpened one — so a variant fades across most of a tile and
    // the hex mosaic the colour exponent steepens toward never shows in the
    // variety. The gather reaches Rv for this (candidates past R carry no
    // colour weight, and only a centre inside R can own a pixel, so the
    // colour field and the grid's silhouette are untouched).
    const float  vs    = p.variant_strength;
    const bool   vary  = vs > 0.0f;
    const double Rv2   = 1.5 * 1.5;
    const double Rg    = vary ? std::max(R, 1.5) : R;
    const double Rg2   = Rg * Rg;
    const float  ns    = static_cast<float>(nominal_s(g));
    // Streaks and cracks are sub-tile line work: they would alias on the far
    // tiers, so they fade in from 12 to 24 (streaks) and 16 to 32 (cracks)
    // nominal px per hex; the stipple is close-tier only, like the fine octave.
    const float  streak_gate = std::clamp((ns - 12.0f) / 12.0f, 0.0f, 1.0f);
    const float  crack_gate  = std::clamp((ns - 16.0f) / 16.0f, 0.0f, 1.0f);
    int patch_cells, patch_cells2, streak_cells_l, streak_cells_s, crack_cells, stip_cells;
    const double patch_cell    = periodic_cell(0.85, patch_cells);
    const double patch_cell2   = periodic_cell(0.33, patch_cells2);
    const double streak_long   = periodic_cell(0.50, streak_cells_l);
    const double streak_short  = periodic_cell(0.12, streak_cells_s);
    const double crack_cell    = periodic_cell(0.30, crack_cells);
    const double stip_cell     = periodic_cell(0.10, stip_cells); // finer reads as a value-noise checker
    constexpr double kStreakK  = 0.57735026918962576; // tan 30 degrees: the two oblique streak directions

    for (int py = 0; py < ph; ++py)
    {
        const double y0_ = (py0 + py + 0.5) / g.s + g.y_min;
        std::uint32_t* row_out = out + static_cast<std::size_t>(py) * pw;

        for (int px = 0; px < pw; ++px)
        {
            const double x0_ = (px0 + px + 0.5) / g.s;

            // Domain warp: displace the sample point by a two-octave vector
            // field BEFORE resolving the owning tile. A class boundary then
            // follows the warped field instead of the hex lattice — the big
            // octave meanders the coastline, the small one FRAYS the straight
            // hex edges the big one merely translates. Periodic in x like
            // every other lattice here. The warp applies to the TILE lookup
            // only; the detail and grain fields below sample unwarped
            // coordinates, so a strong warp cannot swirl the brushwork.
            const double uy = y0_ - g.y_min;
            const double wax =
                (value_noise(x0_, uy, warp_cell,  warp_cells,  0xA11Cu) - 0.5) * 2.0
              + (value_noise(x0_, uy, warp_cell2, warp_cells2, 0xA21Cu) - 0.5) * 0.9;
            const double way =
                (value_noise(x0_, uy, warp_cell,  warp_cells,  0xB22Du) - 0.5) * 2.0
              + (value_noise(x0_, uy, warp_cell2, warp_cells2, 0xB32Du) - 0.5) * 0.9;
            const double x = x0_ + wax * p.warp_amp;
            const double y = y0_ + way * p.warp_amp;

            // Gather tile-centre candidates within the blend radius of a
            // sample point. Weights are a Wendland-style (R² − d²)^wpow
            // falloff — smooth, compact, cheap; the OWNER (nearest centre)
            // decides the pixel's class, and only candidates of that class
            // blend, so a coastline and a mask edge stay hard while
            // everything inside a class is continuous. A lambda because an
            // oblique bake resolves TWICE: once flat to learn the height,
            // then again at the height-displaced point.
            double best_d2 = 1e30;
            int    owner   = -1;
            std::uint8_t owner_cls = 0;
            struct cand { std::size_t i; double w; double d2; };
            cand cands[24];
            int  ncand = 0;
            double last_sy = y; // the y the owner was resolved at (feature passes)
            const auto gather = [&](double sx, double sy_)
            {
                last_sy = sy_;
                best_d2 = 1e30;
                owner   = -1;
                ncand   = 0;
                const int r_lo = std::max(0, static_cast<int>(std::ceil((sy_ - Rg) / 1.5)));
                const int r_hi = std::min(src.gh - 1,
                                          static_cast<int>(std::floor((sy_ + Rg) / 1.5)));
                for (int r = r_lo; r <= r_hi; ++r)
                {
                    const double cy   = 1.5 * r;
                    const double dy   = sy_ - cy;
                    const double odd  = (r & 1) ? 0.5 : 0.0;
                    const int    c0   = static_cast<int>(std::floor(sx / kSqrt3 - odd));
                    for (int dc = -1; dc <= 2; ++dc)
                    {
                        const int c  = c0 + dc;
                        const double cx = kSqrt3 * (c + odd);
                        double dx = sx - cx;
                        dx -= period * std::round(dx / period); // cylinder wrap
                        const double d2 = dx * dx + dy * dy;
                        if (d2 >= Rg2)
                            continue;
                        const int cw = ((c % src.gw) + src.gw) % src.gw;
                        const std::size_t i = static_cast<std::size_t>(r) * src.gw + cw;
                        if (src.cls[i] == static_cast<std::uint8_t>(bake_source::tile_class::void_))
                            continue;
                        double wgt = 0.0; // past R: a variant neighbour only
                        if (d2 < R2)
                        {
                            const double t = R2 - d2;
                            wgt = t * t;
                            for (int e = 2; e < wpow; ++e)
                                wgt *= t;
                        }
                        if (ncand < 24)
                            cands[ncand++] = { i, wgt, d2 };
                        if (d2 < R2 && d2 < best_d2)
                        {
                            best_d2 = d2;
                            owner   = static_cast<int>(i);
                        }
                    }
                }
            };

            gather(x, y);
            if (owner < 0)
            {
                row_out[px] = 0u; // outside the grid: transparent, canvas bg shows
                continue;
            }
            owner_cls = src.cls[static_cast<std::size_t>(owner)];
            if (owner_cls == static_cast<std::uint8_t>(bake_source::tile_class::masked))
            {
                row_out[px] = k_lock_colour; // flat, unblended, no leak
                continue;
            }

            // OBLIQUE (BL-737): displace by the smoothed height at this point
            // and re-resolve — the ground the camera sees at this row is the
            // terrain standing lift·h higher, so hills grow real silhouettes.
            // A masked re-resolve locks (a peak truncates at the mask edge
            // rather than leaking); the lift is modest, so the projection
            // stays monotonic and needs no occlusion handling.
            if (g.lift > 0.0)
            {
                double hw = 0.0, ww = 0.0;
                for (int k = 0; k < ncand; ++k)
                    if (src.cls[cands[k].i] == owner_cls)
                    {
                        hw += cands[k].w * src.height[cands[k].i];
                        ww += cands[k].w;
                    }
                const double h_est = ww > 0.0
                    ? hw / ww
                    : src.height[static_cast<std::size_t>(owner)];
                gather(x, y + h_est * g.lift);
                if (owner < 0)
                {
                    row_out[px] = 0u;
                    continue;
                }
                owner_cls = src.cls[static_cast<std::size_t>(owner)];
                if (owner_cls == static_cast<std::uint8_t>(bake_source::tile_class::masked))
                {
                    row_out[px] = k_lock_colour;
                    continue;
                }
            }

            cover_out[static_cast<std::size_t>(py) * pw + px] =
                owner_cls == static_cast<std::uint8_t>(bake_source::tile_class::water)
                    ? 100
                    : static_cast<std::uint8_t>(1 + src.cover[static_cast<std::size_t>(owner)]);
            if (fpx)
            {
                // The owner's resolve point: (x, y) flat, or the displaced
                // re-resolve on an oblique tier (the last gather's point).
                const int    orow = owner / src.gw, ocol = owner % src.gw;
                const double ocx  = kSqrt3 * (ocol + ((orow & 1) ? 0.5 : 0.0));
                const double sy_  = last_sy;
                double rdx = x - ocx;
                rdx -= period * std::round(rdx / period);
                feature_px& f = fpx[static_cast<std::size_t>(py) * pw + px];
                f.rx    = static_cast<float>(rdx);
                f.ry    = static_cast<float>(sy_ - 1.5 * orow);
                f.owner = owner;
            }
            double wsum = 0.0, cr = 0.0, cg = 0.0, cb = 0.0;
            double hsum = 0.0, gx = 0.0, gy = 0.0, rb = 0.0, jt = 0.0;

            for (int k = 0; k < ncand; ++k)
            {
                const std::size_t i = cands[k].i;
                if (src.cls[i] != owner_cls)
                    continue;
                const double wgt = cands[k].w;
                const std::uint32_t col = src.colour[i];
                wsum += wgt;
                cr += wgt * palette::col_r(col);
                cg += wgt * palette::col_g(col);
                cb += wgt * palette::col_b(col);
                hsum += wgt * src.height[i];
                gx   += wgt * src.grad_x[i];
                gy   += wgt * src.grad_y[i];
                rb   += wgt * src.relief_bias[i];
                jt   += wgt * src.jitter[i];
            }
            if (wsum <= 0.0)
            {
                const std::uint32_t col = src.colour[static_cast<std::size_t>(owner)];
                row_out[px] = col;
                tag[static_cast<std::size_t>(py) * pw + px] = 1;
                continue;
            }
            const double inv = 1.0 / wsum;
            float r_ = static_cast<float>(cr * inv);
            float g_ = static_cast<float>(cg * inv);
            float b_ = static_cast<float>(cb * inv);
            const float h    = static_cast<float>(hsum * inv);
            const float gxx  = static_cast<float>(gx * inv);
            const float gyy  = static_cast<float>(gy * inv);
            const float bias = static_cast<float>(rb * inv);
            const float jtt  = static_cast<float>(jt * inv);

            const bool land = owner_cls == static_cast<std::uint8_t>(bake_source::tile_class::land);

            // BL-1243: this pixel's variant vector — the land candidates'
            // vectors under the soft Rv falloff, scaled from neutral by
            // variant_strength. Blending the VECTOR (never picking one) is
            // the cross-fade: tone, hue and texture mix slide continuously
            // from one tile's variant to the next, so no variant edge exists.
            float vv[k_vparam_count];
            const bool vland = vary && land;
            if (vland)
            {
                double acc[k_vparam_count] = {};
                double vw = 0.0;
                for (int k = 0; k < ncand; ++k)
                {
                    if (src.cls[cands[k].i] != owner_cls || cands[k].d2 >= Rv2)
                        continue;
                    const double t = Rv2 - cands[k].d2;
                    const double w = t * t;
                    vw += w;
                    const float* vp = &src.vparam[cands[k].i * k_vparam_count];
                    for (int j = 0; j < k_vparam_count; ++j)
                        acc[j] += w * vp[j];
                }
                const double vinv = vw > 0.0 ? 1.0 / vw : 0.0;
                for (int j = 0; j < k_vparam_count; ++j)
                {
                    const float blended = vw > 0.0 ? static_cast<float>(acc[j] * vinv) : k_vneutral[j];
                    vv[j] = k_vneutral[j] + (blended - k_vneutral[j]) * vs;
                }
            }
            float vpatch = 0.0f; // the broad patch field, reused by the hue drift below

            float lum = 1.0f;
            if (land)
            {
                // Fractal height detail: a noise field ADDED to the interpolated
                // tile height before shading, its amplitude growing with the
                // landform bias — plains stay calm, a range reads craggy. The
                // hillshade then runs on the combined gradient (tile gradient +
                // finite-difference gradient of the detail field), which is
                // what gives the ground painterly terrain texture at sub-tile
                // scale instead of a per-hex mosaic.
                const float amp = p.detail_amp * detail_mul
                                * (0.25f + p.landform_accent * std::fabs(bias) + 0.35f * h)
                                * (vland ? vv[vp_detail] : 1.0f);
                // The GRADIENT reads the low octave only, central-differenced
                // at half a cell: a fine octave in the slope is per-pixel
                // speckle, not terrain. The fine octave still contributes to
                // the VALUE below, where it reads as surface variation.
                //
                // RIDGED MIX (the "sharper hills" ruling, Ben 2026-09-01): on
                // strongly-biased landforms the smooth field folds toward
                // ridged noise (0.25 − |n − 0.5|), whose |·| kink puts a hard
                // crease at every crest — a range then shades as ridge lines
                // instead of soft blobs. The finite differences pick the
                // crease up for free.
                const float ridge_w = std::min(1.0f, std::fabs(bias) * p.landform_accent)
                                    * p.ridge_strength;
                const auto detail_lo = [&](double sx, double sy) -> float
                {
                    const float n = value_noise(sx, sy, detail_cell, detail_cells, 0xD371u);
                    const float smooth = n - 0.5f;
                    const float ridged = (0.25f - std::fabs(n - 0.5f)) * 2.0f;
                    return smooth + (ridged - smooth) * ridge_w;
                };
                const double eps = detail_cell * 0.5;
                const float ddx = (detail_lo(x0_ + eps, uy) - detail_lo(x0_ - eps, uy))
                                  / static_cast<float>(2.0 * eps);
                const float ddy = (detail_lo(x0_, uy + eps) - detail_lo(x0_, uy - eps))
                                  / static_cast<float>(2.0 * eps);
                const float d0 = detail_lo(x0_, uy) * 0.7f
                    + (value_noise(x0_, uy, detail_cell2, detail_cells2, 0xD372u) - 0.5f) * 0.3f;

                // Two shade terms with their own scales: tile slopes are tiny
                // (heights are 0-1 across a whole continent) and take the big
                // gain; the detail slope is order-1 and takes amp alone. The
                // clamp widens with resolution — a close tier is allowed
                // deeper shadow.
                const float tgx = gxx + ddx * amp;
                const float tgy = gyy + ddy * amp;
                const float shade = (gxx * Lx + gyy * Ly) * p.relief_gain
                                  + (ddx * Lx + ddy * Ly) * amp * 0.75f;
                lum += std::clamp(shade, -(0.60f + 0.15f * res_t), 0.60f + 0.15f * res_t);

                // Slope rock exposure: steep ground sheds its cover colour
                // toward bare rock, which is what makes a hillside read as a
                // HILL rather than as shaded grass. Strongest at close tiers.
                const float slope = std::sqrt(tgx * tgx + tgy * tgy);
                const float rock_t = std::clamp((slope - (0.55f - (vland ? vv[vp_rock] : 0.0f))) * 1.2f,
                                                0.0f, 1.0f)
                                   * p.rock_exposure * (0.35f + 0.65f * res_t);
                if (rock_t > 0.0f)
                {
                    r_ += (122.0f - r_) * rock_t;
                    g_ += (112.0f - g_) * rock_t;
                    b_ += (100.0f - b_) * rock_t;
                }
                // Altitude lift, the landform's own signed bias, and the detail
                // field's own value (a crag's top is lit even side-on).
                lum += (h - 0.45f) * p.altitude_gain + bias * 0.30f + d0 * amp * 0.8f;
                // Painterly patchiness: a WHISPER of per-tile jitter (this is
                // the hex-mosaic dial — the detail field carries the texture
                // now), then fine grain.
                lum += jtt * p.jitter;
                const float n2 = value_noise(x0_, uy, noise_cell_b, noise_cells_b, 0xFAB1u);
                lum += (n2 - 0.5f) * 2.0f * p.noise_strength * noise_mul;
                if (fine_on)
                {
                    const float n3 = value_noise(x0_, uy, fine_cell, fine_cells, 0x51D3u);
                    lum += (n3 - 0.5f) * (1.2f + 2.2f * res_t) * p.noise_strength;
                }
                if (vland)
                {
                    // The variant's texture: its tone, then its mix over the
                    // shared field bank. Each field is sampled only where the
                    // blended mix gives it weight, so a plain pays for its
                    // patches and stipple, never for a dune's ripples.
                    lum += vv[vp_tone];
                    vpatch = (value_noise(x0_, uy, patch_cell,  patch_cells,  0x7B01u) - 0.5f) * 1.6f
                           + (value_noise(x0_, uy, patch_cell2, patch_cells2, 0x7B02u) - 0.5f) * 0.6f;
                    lum += vv[vp_patch] * vpatch;
                    // Streaks: a soft band plus a darker crease along each
                    // band's centre line — strata, gullies, ripples, combing.
                    const auto streak = [](float n) -> float
                    {
                        const float c  = 1.0f - std::fabs(2.0f * n - 1.0f);
                        const float c2 = c * c, c6 = c2 * c2 * c2;
                        return (n - 0.5f) * 1.5f - 0.8f * (c6 - 0.14f);
                    };
                    const float sh = vv[vp_streak_h] * streak_gate;
                    const float sa = vv[vp_streak_a] * streak_gate;
                    const float sb = vv[vp_streak_b] * streak_gate;
                    if (sh > 1e-4f)
                        lum += sh * streak(value_noise_aniso(x0_, uy, streak_long, streak_cells_l,
                                                             streak_short, 0x7B03u));
                    if (sa > 1e-4f)
                        lum += sa * streak(value_noise_aniso(x0_ - kStreakK * uy, uy, streak_short,
                                                             streak_cells_s, streak_long, 0x7B04u));
                    if (sb > 1e-4f)
                        lum += sb * streak(value_noise_aniso(x0_ + kStreakK * uy, uy, streak_short,
                                                             streak_cells_s, streak_long, 0x7B05u));
                    const float ck = vv[vp_crack] * crack_gate;
                    if (ck > 1e-4f)
                    {
                        // Crack lines: the n = 0.5 level set of a mid octave,
                        // a thin dark line wandering across the ground.
                        const float n  = value_noise(x0_, uy, crack_cell, crack_cells, 0x7B06u);
                        const float c  = 1.0f - std::fabs(2.0f * n - 1.0f);
                        const float c2 = c * c, c4 = c2 * c2, c8 = c4 * c4;
                        lum -= ck * (2.2f * c8 * c2 - 0.2f);
                    }
                    if (fine_on && vv[vp_stipple] > 1e-4f)
                    {
                        const float n = value_noise(x0_, uy, stip_cell, stip_cells, 0x7B07u);
                        lum += vv[vp_stipple] * (n - 0.5f) * 1.2f;
                    }
                }
            }
            else
            {
                // Water: keep it calm — depth already varies by kind (BL-516's
                // family), so only a whisper of grain so it does not read flat-shaded.
                const float n1 = value_noise(x0_, uy, noise_cell_a, noise_cells_a, 0x5EA0u);
                lum += (n1 - 0.5f) * 2.0f * p.water_noise;
                lum += (h - 0.45f) * 0.06f;
            }
            r_ *= lum; g_ *= lum; b_ *= lum;

            if (land)
            {
                // Colour mottle: a mid-frequency warm/cool swing so ground
                // varies in HUE as well as luminance — luminance noise alone
                // reads as mist over the smooth colour field, most of all at
                // the close tiers (hence the res_t growth).
                const float mot = (value_noise(x0_, uy, noise_cell_a, noise_cells_a, 0xC01Au)
                                   - 0.5f) * (0.6f + 0.9f * res_t);
                r_ *= 1.0f + mot * 0.14f;
                g_ *= 1.0f + mot * 0.04f;
                b_ *= 1.0f - mot * 0.10f;
                if (vland)
                {
                    // The variant's hue: a warm/cool drift across the whole
                    // tile, and patches that green or dry within it.
                    const float wm = vv[vp_warm];
                    const float hp = vv[vp_patch_hue] * vpatch;
                    r_ *= (1.0f + 0.045f * wm) * (1.0f - 0.03f * hp);
                    g_ *= (1.0f + 0.010f * wm) * (1.0f + 0.06f * hp);
                    b_ *= (1.0f - 0.050f * wm) * (1.0f - 0.03f * hp);
                }
            }

            // UNGRADED write: the near-future grade moved to a final buffer
            // sweep (below) so the feature stamps drawn between base and
            // grade take the grade exactly as the ground under them does —
            // the grade stays a separable pass, per the settled ruling.
            row_out[px] = palette::col32(
                std::clamp(static_cast<int>(r_ + 0.5f), 0, 255),
                std::clamp(static_cast<int>(g_ + 0.5f), 0, 255),
                std::clamp(static_cast<int>(b_ + 0.5f), 0, 255), 255);
            tag[static_cast<std::size_t>(py) * pw + px] = 1;
        }
    }
}

/// Darken the pixel where its right/down neighbour resolves a different cover
/// class — a deterministic 1 px ink line on every cover boundary and shoreline.
/// Runs on the apron buffer, so a line never breaks at a chunk edge; class 0
/// (lock fill / transparent) never inks and is never inked against.
void edge_ink(std::uint32_t* buf, const std::uint8_t* cover, int pw, int ph,
              float cover_k, float shore_k)
{
    for (int y = 0; y < ph; ++y)
        for (int x = 0; x < pw; ++x)
        {
            const std::size_t i = static_cast<std::size_t>(y) * pw + x;
            const std::uint8_t id = cover[i];
            if (!id)
                continue;
            float k = 0.0f;
            const std::uint8_t right = x + 1 < pw ? cover[i + 1] : id;
            const std::uint8_t down  = y + 1 < ph ? cover[i + pw] : id;
            if (right && right != id)
                k = std::max(k, (id == 100 || right == 100) ? shore_k : cover_k);
            if (down && down != id)
                k = std::max(k, (id == 100 || down == 100) ? shore_k : cover_k);
            if (k <= 0.0f)
                continue;
            const float m = 1.0f - k;
            buf[i] = palette::col32(
                static_cast<int>(palette::col_r(buf[i]) * m),
                static_cast<int>(palette::col_g(buf[i]) * m),
                static_cast<int>(palette::col_b(buf[i]) * m), 255);
        }
}

/// Unsharp mask: out += amount * (out − boxblur5(out)), applied only to tagged
/// pixels whose kernel reach is itself fully tagged — the lock fill and the
/// mask boundary stay byte-exact. Separable 5-tap box, run on the apron so
/// chunk edges cannot halo.
void unsharp(std::uint32_t* buf, const std::uint8_t* tag, int pw, int ph, float amount)
{
    const std::size_t n = static_cast<std::size_t>(pw) * ph;
    std::vector<float> br(n), bg(n), bb(n), tr(n), tg(n), tb(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        br[i] = static_cast<float>(palette::col_r(buf[i]));
        bg[i] = static_cast<float>(palette::col_g(buf[i]));
        bb[i] = static_cast<float>(palette::col_b(buf[i]));
    }
    for (int y = 0; y < ph; ++y)
        for (int x = 0; x < pw; ++x)
        {
            const std::size_t i = static_cast<std::size_t>(y) * pw + x;
            float sr = 0, sg = 0, sb = 0;
            for (int d = -2; d <= 2; ++d)
            {
                const int xx = std::clamp(x + d, 0, pw - 1);
                const std::size_t j = static_cast<std::size_t>(y) * pw + xx;
                sr += br[j]; sg += bg[j]; sb += bb[j];
            }
            tr[i] = sr / 5.0f; tg[i] = sg / 5.0f; tb[i] = sb / 5.0f;
        }
    for (int y = 0; y < ph; ++y)
        for (int x = 0; x < pw; ++x)
        {
            const std::size_t i = static_cast<std::size_t>(y) * pw + x;
            if (!tag[i])
                continue;
            // Kernel-reach tag check: the blur below reads ±2 in both axes.
            const int xm = std::max(0, x - 2), xp = std::min(pw - 1, x + 2);
            const int ym = std::max(0, y - 2), yp = std::min(ph - 1, y + 2);
            if (!tag[static_cast<std::size_t>(y) * pw + xm]
                || !tag[static_cast<std::size_t>(y) * pw + xp]
                || !tag[static_cast<std::size_t>(ym) * pw + x]
                || !tag[static_cast<std::size_t>(yp) * pw + x])
                continue;
            float sr = 0, sg = 0, sb = 0;
            for (int d = -2; d <= 2; ++d)
            {
                const int yy = std::clamp(y + d, 0, ph - 1);
                const std::size_t j = static_cast<std::size_t>(yy) * pw + x;
                sr += tr[j]; sg += tg[j]; sb += tb[j];
            }
            const float mr = sr / 5.0f, mg = sg / 5.0f, mb = sb / 5.0f;
            buf[i] = palette::col32(
                std::clamp(static_cast<int>(br[i] + amount * (br[i] - mr) + 0.5f), 0, 255),
                std::clamp(static_cast<int>(bg[i] + amount * (bg[i] - mg) + 0.5f), 0, 255),
                std::clamp(static_cast<int>(bb[i] + amount * (bb[i] - mb) + 0.5f), 0, 255), 255);
        }
}

// ---------------------------------------------------------------------------
// BL-1242 — landform relief and carved rivers (RENDERING.md § Mountains,
// rivers and terrain variety). Both passes are ANALYTIC: each pixel asks how
// far it stands from a feature skeleton built out of its owner tile and that
// tile's six neighbours, in tile-relative coordinates (feature_px), so the
// result is pure, wrap-exact and chunk-seamless without an apron, and a
// masked tile (no landform, no river bits in the source) contributes nothing.
// ---------------------------------------------------------------------------

inline double sq(double v) { return v * v; }

inline double smooth01(double e0, double e1, double x)
{
    const double t = std::clamp((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

/// Distance from (px, py) to segment a-b; @p t receives the closest parameter.
inline double seg_dist(double px, double py, double ax, double ay, double bx, double by,
                       double& t)
{
    const double vx = bx - ax, vy = by - ay;
    const double l2 = vx * vx + vy * vy;
    t = l2 > 0.0 ? std::clamp(((px - ax) * vx + (py - ay) * vy) / l2, 0.0, 1.0) : 0.0;
    const double dx = px - (ax + vx * t), dy = py - (ay + vy * t);
    return std::sqrt(dx * dx + dy * dy);
}

/// Canonical-space centre of raster tile @p i (wrapped column).
inline void tile_centre(const bake_source& s, int i, double& cx, double& cy)
{
    const int r = i / s.gw, c = i % s.gw;
    cx = kSqrt3 * (c + ((r & 1) ? 0.5 : 0.0));
    cy = 1.5 * r;
}

inline void blend_px(std::uint32_t& dst, float r, float g, float b)
{
    dst = palette::col32(std::clamp(static_cast<int>(r + 0.5f), 0, 255),
                         std::clamp(static_cast<int>(g + 0.5f), 0, 255),
                         std::clamp(static_cast<int>(b + 0.5f), 0, 255), 255);
}

/// LANDFORM RELIEF. Each dramatic tile contributes a skeleton — its centre
/// plus its half of every shared edge to a same-landform neighbour (the
/// bridged run: the halves meet at the edge midpoint, so a run is one form) —
/// and the pass builds an analytic height field from the distance to it:
///   mountain — a massif tent along the ridge, its crest broken into peaks and
///              spurs by noise, rock-coloured toward the crest;
///   canyon   — a cut: a dark floor between steep walls, raised paired rims;
///   rift     — a near-black jagged fissure between raised, scorched lips;
///   crater   — a raised-rim bowl with a pale ejecta apron (never spans).
/// The field is shaded from the NW light by its own gradient. Widths floor at
/// a pixel scale so every form still reads on the far page.
void bake_landforms(const bake_source& src, const geometry& g, const bake_params& p,
                    int pw, int ph, std::uint32_t* out, const std::uint8_t* tag,
                    const std::uint8_t* cover, const feature_px* fpx)
{
    const double period = g.gw * kSqrt3;
    const auto periodic_cell = [&](double target, int& cells_out) -> double
    {
        cells_out = std::max(1, static_cast<int>(std::lround(period / target)));
        return period / cells_out;
    };
    int jag_n, fine_n, crest_n, spur_n;
    const double jag_c   = periodic_cell(0.55, jag_n);
    const double fine_c  = periodic_cell(0.17, fine_n);
    const double crest_c = periodic_cell(0.50, crest_n);
    const double spur_c  = periodic_cell(0.21, spur_n);
    int crag_n;
    const double crag_c  = periodic_cell(0.085, crag_n);
    int crest2_n;
    const double crest2_c = periodic_cell(0.30, crest2_n); // BL-1243: close-set peaks
    const float  vs = p.variant_strength;
    const bool   close   = nominal_s(g) >= 40.0; // the crag octave would alias below this (nominal: BL-1244)
    // The far tiers carry the read on fewer pixels: lift the form's light there
    // (x1.4 at the 6 px far page, easing to x1 by 24 px), so an expensive tile is
    // never an invisible one at the whole-grid view.
    const double far_boost = 1.0 + 0.4 * (1.0 - std::clamp((nominal_s(g) - 6.0) / 18.0, 0.0, 1.0));
    const double pxc = 1.0 / g.s;                      // one baked pixel, canonical (edge coverage)
    const double pxn = 1.0 / nominal_s(g);             // one NOMINAL pixel: width floors survive the 2x downsample
    const double eps = std::max(0.6 * pxc, 0.012);     // gradient step
    const float  Lx = -0.554700196f, Ly = -0.832050323f; // toward the NW light
    const float  k  = p.landform_strength;
    const auto   land = static_cast<std::uint8_t>(bake_source::tile_class::land);
    const auto   L_mountain = static_cast<std::uint8_t>(terrain_landform::mountain);
    const auto   L_canyon   = static_cast<std::uint8_t>(terrain_landform::canyon);
    const auto   L_rift     = static_cast<std::uint8_t>(terrain_landform::rift);
    const auto   L_crater   = static_cast<std::uint8_t>(terrain_landform::crater);

    struct seg { double ax, ay, bx, by; std::uint8_t lf; };
    struct bowl { double x, y, rc; };

    const std::size_t n = static_cast<std::size_t>(pw) * ph;
    for (std::size_t i = 0; i < n; ++i)
    {
        if (!tag[i] || cover[i] == 100)
            continue;
        const feature_px& f = fpx[i];
        if (f.owner < 0 || !(src.near_feature[f.owner] & k_near_landform))
            continue;
        const int o = f.owner;

        // Skeleton, owner-relative. Up to 7 tiles x 6 half-edges.
        seg  segs[48];
        int  nseg = 0;
        bowl bowls[7];
        int  nbowl = 0;
        bool has_m = false, has_c = false, has_r = false;
        // BL-1243: each form's variant parameters, cross-faded over the
        // dramatic tiles around the pixel by the same soft (Rv^2 - d^2)^2
        // falloff the ground's variants use, one blend per landform kind.
        double lacc[4][k_lf_params] = {};
        double lw[4] = {};
        for (int kk = -1; kk < 6; ++kk)
        {
            const int t = kk < 0 ? o : nb_index(src, o, kk);
            if (t < 0 || src.cls[t] != land || !dramatic(src.landform[t]))
                continue;
            const double ox = kk < 0 ? 0.0 : kNbDx[kk];
            const double oy = kk < 0 ? 0.0 : kNbDy[kk];
            const std::uint8_t lf = src.landform[t];
            if (vs > 0.0f)
            {
                const double vd2 = sq(f.rx - ox) + sq(f.ry - oy);
                if (vd2 < 2.25)
                {
                    const int grp = lf == L_mountain ? 0 : lf == L_canyon ? 1 : lf == L_rift ? 2 : 3;
                    const double w = sq(2.25 - vd2);
                    lw[grp] += w;
                    for (int j = 0; j < k_lf_params; ++j)
                        lacc[grp][j] += w * k_lf_var[grp][src.variant[t]][j];
                }
            }
            const int tr = t / src.gw, tc = t % src.gw;
            if (lf == L_crater)
            {
                bowls[nbowl++] = { ox, oy, 0.44 + 0.14 * hash01(tc, tr, 0xC4A7u) };
                continue;
            }
            has_m |= lf == L_mountain;
            has_c |= lf == L_canyon;
            has_r |= lf == L_rift;
            const std::uint8_t links = src.lf_links[t];
            if (links)
            {
                for (int s = 0; s < 6; ++s)
                    if (links & (1u << s))
                        segs[nseg++] = { ox, oy, ox + 0.5 * kNbDx[s], oy + 0.5 * kNbDy[s], lf };
            }
            else if (lf == L_mountain)
            {
                segs[nseg++] = { ox, oy, ox, oy, lf }; // a lone peak: a cone
            }
            else
            {
                // A lone cut or fissure keeps a centred form along one of the
                // three hex axes, chosen by the tile's hash.
                const int ax_side = static_cast<int>(hash01(tc, tr, 0xA715u) * 3.0f) % 3;
                const double hl = 0.55;
                const double ux = kNbDx[ax_side] / kSqrt3, uy = kNbDy[ax_side] / kSqrt3;
                segs[nseg++] = { ox - hl * ux, oy - hl * uy, ox + hl * ux, oy + hl * uy, lf };
            }
        }
        if (nseg == 0 && nbowl == 0)
            continue;
        float lp[4][k_lf_params];
        for (int grp = 0; grp < 4; ++grp)
            for (int j = 0; j < k_lf_params; ++j)
            {
                const float neutral = k_lf_neutral[grp][j];
                lp[grp][j] = lw[grp] > 0.0
                    ? neutral + (static_cast<float>(lacc[grp][j] / lw[grp]) - neutral) * vs
                    : neutral;
            }
        const double m_crest_mix = lp[0][0], m_spur = lp[0][1], m_crag = lp[0][2];
        const float  m_tint = lp[0][3], m_cover = lp[0][4];
        const double c_bands = lp[1][0], c_warm = lp[1][1], c_floor = lp[1][2], c_rim = lp[1][3];
        const double r_width = lp[2][0], r_scorch = lp[2][1];
        const double k_ejecta = lp[3][0], k_bowl = lp[3][1], k_rim = lp[3][2];

        double ocx, ocy;
        tile_centre(src, o, ocx, ocy);
        const double abx = ocx + f.rx, aby = ocy + f.ry;
        // Organic displacement of the query point, sampled once per pixel: a
        // large octave bends the forms, a fine one roughens a fissure's edge.
        const double jx = (value_noise(abx, aby, jag_c,  jag_n,  0x1A61u) - 0.5) * 2.0;
        const double jy = (value_noise(abx, aby, jag_c,  jag_n,  0x1A62u) - 0.5) * 2.0;
        const double fx = (value_noise(abx, aby, fine_c, fine_n, 0x1A63u) - 0.5) * 2.0;
        const double fy = (value_noise(abx, aby, fine_c, fine_n, 0x1A64u) - 0.5) * 2.0;
        // Up close a fissure's edge cracks at a finer grain still.
        const double ux = close ? (value_noise(abx, aby, crag_c, crag_n, 0x1A69u) - 0.5) * 2.0 : 0.0;
        const double uy = close ? (value_noise(abx, aby, crag_c, crag_n, 0x1A6Au) - 0.5) * 2.0 : 0.0;
        const double rift_w = std::max((0.05 + 0.04 * value_noise(abx, aby, fine_c, fine_n, 0x1A67u)) * r_width,
                                       0.75 * pxn);
        const double fw = std::max(0.10, 0.9 * pxn); // canyon floor half-width
        const double ww = 0.20 + pxn;                // canyon wall run, rim to floor

        struct mat { double tm = 0, cut = 0, wall = 0, core = 0, scorch = 0, bowl = 0, ejecta = 0;
                     bool any = false; };
        const auto field = [&](double qx, double qy, mat* m) -> double
        {
            double dm = 1e9, dc = 1e9, dr = 1e9, tt;
            for (int s = 0; s < nseg; ++s)
            {
                const seg& sg = segs[s];
                if (sg.lf == L_mountain)
                    dm = std::min(dm, seg_dist(qx + 0.14 * jx + 0.03 * fx, qy + 0.14 * jy + 0.03 * fy,
                                               sg.ax, sg.ay, sg.bx, sg.by, tt));
                else if (sg.lf == L_canyon)
                    dc = std::min(dc, seg_dist(qx + 0.09 * jx + 0.025 * fx, qy + 0.09 * jy + 0.025 * fy,
                                               sg.ax, sg.ay, sg.bx, sg.by, tt));
                else
                    dr = std::min(dr, seg_dist(qx + 0.05 * jx + 0.05 * fx + 0.03 * ux,
                                               qy + 0.05 * jy + 0.05 * fy + 0.03 * uy,
                                               sg.ax, sg.ay, sg.bx, sg.by, tt));
            }
            double h = 0.0;
            // Support: past these distances every term below is zero (or under
            // 1e-5), so a pixel outside all of them skips the gradient and the
            // paint — the cull that keeps the pass cheap beside a feature.
            if (m)
            {
                m->any = dm < 1.05 || dc < fw + ww + 0.30 || dr < rift_w + 0.45;
                for (int b = 0; b < nbowl && !m->any; ++b)
                    m->any = std::sqrt(sq(qx - bowls[b].x) + sq(qy - bowls[b].y)) < bowls[b].rc + 0.85;
                if (!m->any)
                    return 0.0;
            }
            if (dm < 1e8)
            {
                // Concave flanks under a sharp crest (t^1.5 of a tent), the
                // crest broken into peaks, the flanks into spurs and, up
                // close, crags — sampled INSIDE the field so the gradient,
                // and so the light, sees them.
                const double t_ = std::max(0.0, 1.0 - dm / 1.0);
                if (t_ > 0.0)
                {
                    const double ax_ = ocx + qx, ay_ = ocy + qy;
                    double crest = value_noise(ax_, ay_, crest_c, crest_n, 0x1A65u);
                    if (m_crest_mix > 0.0) // a variant's close-set peaks
                        crest += (value_noise(ax_, ay_, crest2_c, crest2_n, 0x1A6Bu) - crest) * m_crest_mix;
                    const double spur  = 1.0 - std::fabs(
                        2.0 * value_noise(ax_, ay_, spur_c, spur_n, 0x1A66u) - 1.0);
                    double crag = 0.5;
                    if (close)
                        crag = 1.0 - std::fabs(
                            2.0 * value_noise(ax_, ay_, crag_c, crag_n, 0x1A68u) - 1.0);
                    h += std::pow(t_, 1.5) * (0.6 + 0.8 * crest)
                       + 0.11 * m_spur * t_ * (spur - 0.5) + 0.045 * m_crag * t_ * (crag - 0.5);
                }
                if (m) m->tm = t_;
            }
            if (dc < 1e8)
            {
                const double cut = 1.0 - smooth01(fw, fw + ww, dc);
                const double rim = std::exp(-sq((dc - (fw + ww + 0.05)) / 0.07));
                h += -0.25 * cut + 0.04 * c_rim * rim;
                if (m)
                {
                    m->cut  = cut;
                    // Position up the wall, 0 at the floor, 1 at the rim — the
                    // strata banding reads it.
                    m->wall = std::clamp((dc - fw) / ww, 0.0, 1.0);
                }
            }
            if (dr < 1e8)
            {
                const double core = std::clamp((rift_w - dr) / pxc + 0.5, 0.0, 1.0);
                const double lips = std::exp(-sq((dr - rift_w - 0.09) / 0.08));
                h += 0.05 * lips; // the core is painted, not lit
                if (m)
                {
                    m->core   = core;
                    m->scorch = 1.0 - smooth01(rift_w, rift_w + 0.38, dr);
                }
            }
            for (int b = 0; b < nbowl; ++b)
            {
                const double d = std::sqrt(sq(qx + 0.03 * jx - bowls[b].x) + sq(qy + 0.03 * jy - bowls[b].y));
                const double rc = bowls[b].rc;
                const double in = std::max(0.0, 1.0 - sq(d / rc));
                h += 0.5 * k_rim * std::exp(-sq((d - rc) / 0.09)) - 0.55 * in;
                if (m)
                {
                    m->bowl   = std::max(m->bowl, in);
                    m->ejecta = std::max(m->ejecta, d > rc ? 0.5 * std::exp(-(d - rc) / 0.22) : 0.0);
                }
            }
            return h;
        };

        mat m;
        field(f.rx, f.ry, &m);
        if (!m.any)
            continue;
        const double hx = (field(f.rx + eps, f.ry, nullptr) - field(f.rx - eps, f.ry, nullptr)) / (2.0 * eps);
        const double hy = (field(f.rx, f.ry + eps, nullptr) - field(f.rx, f.ry - eps, nullptr)) / (2.0 * eps);
        const float shade = static_cast<float>(
            std::clamp(-(hx * Lx + hy * Ly) * 0.50 * far_boost, -0.55, 0.50));

        std::uint32_t& dst = out[i];
        float r_ = static_cast<float>(palette::col_r(dst));
        float g_ = static_cast<float>(palette::col_g(dst));
        float b_ = static_cast<float>(palette::col_b(dst));
        // Mountain rock toward the crest, then the crest catches light.
        const float rock = static_cast<float>(std::min(1.0, m.tm * 1.3) * 0.62) * k * m_cover;
        r_ += (142.0f + 11.0f * m_tint - r_) * rock; // the variant's rock: greyer (-) or warmer (+)
        g_ += (133.0f +  2.0f * m_tint - g_) * rock;
        b_ += (120.0f - 10.0f * m_tint - b_) * rock;
        float lum = 1.0f + shade * k + static_cast<float>(0.05 * m.tm * m.tm) * k;
        // Canyon floor: deep, warm shadow. Crater bowl: shade. Ejecta: pale.
        lum *= 1.0f - static_cast<float>(0.30 * c_floor * m.cut + 0.16 * k_bowl * m.bowl
                                         + 0.24 * r_scorch * m.scorch) * k;
        if (close && m.cut > 0.0 && m.cut < 1.0)
        {
            // Canyon walls show their strata up close: banded light down the cut.
            const double band = std::sin(m.wall * 3.14159265 * c_bands);
            lum *= 1.0f + static_cast<float>(0.10 * band * 4.0 * m.cut * (1.0 - m.cut)) * k;
        }
        // A canyon's exposed rock runs warm: the floor and walls redden.
        {
            const float warm = static_cast<float>(0.45 * c_warm * m.cut) * k;
            r_ += (152.0f - r_) * warm;
            g_ += (100.0f - g_) * warm;
            b_ += (74.0f - b_) * warm;
        }
        lum *= 1.0f + static_cast<float>(0.10 * k_ejecta * m.ejecta) * k;
        r_ *= lum;
        g_ *= lum;
        b_ *= lum;
        // Rift: the fissure itself, near-black.
        const float core = static_cast<float>(m.core) * k;
        r_ += (24.0f - r_) * core;
        g_ += (18.0f - g_) * core;
        b_ += (20.0f - b_) * core;
        blend_px(dst, r_, g_, b_);
    }
}

/// River half-width (canonical units) for an accumulated flow — the downstream
/// widening. sqrt so a long trunk widens without the headwaters vanishing.
inline double river_hw(float flow)
{
    return std::min(0.20, 0.028 + 0.026 * std::sqrt(static_cast<double>(std::max(1.0f, flow))));
}

/// CARVED RIVERS. A river tile draws, for each inflow, the quadratic from the
/// inflow edge midpoint to the outflow edge midpoint with its own centre as
/// control — the roads' B-spline rule, so consecutive tiles meet
/// tangent-continuous and a confluence's two curves converge into one. A
/// source is a tapering spoke from the centre; a mouth's sea tile extends the
/// course a little way into the water as a widening estuary (painted only
/// where it overlies land, so it closes any gap the warped coast leaves).
/// Width runs from the upstream tile's flow to this tile's along the curve:
/// the width gradient says which way the water goes. Around the water, a wet
/// margin darkens the ground into the bank; inside, the course shelves from
/// shallow at the edge to deep in the channel.
void bake_rivers(const bake_source& src, const geometry& g, const bake_params& p,
                 int pw, int ph, std::uint32_t* out, const std::uint8_t* tag,
                 const std::uint8_t* cover, const feature_px* fpx)
{
    const double period = g.gw * kSqrt3;
    int mea_n = 1;
    mea_n = std::max(1, static_cast<int>(std::lround(period / 0.42)));
    const double mea_c = period / mea_n;
    const double pxc = 1.0 / g.s;            // one baked pixel (edge coverage)
    const double pxn = 1.0 / nominal_s(g);   // one nominal pixel: the width floor (BL-1244)
    const float  k   = p.river_strength;
    const auto   water = static_cast<std::uint8_t>(bake_source::tile_class::water);
    constexpr int kSeg = 10; // polyline steps per quadratic

    struct curve { double p0x, p0y, p1x, p1y, p2x, p2y, hw0, hw1; };

    const std::size_t n = static_cast<std::size_t>(pw) * ph;
    for (std::size_t i = 0; i < n; ++i)
    {
        if (!tag[i] || cover[i] == 100)
            continue;
        const feature_px& f = fpx[i];
        if (f.owner < 0 || !(src.near_feature[f.owner] & k_near_river))
            continue;
        const int o = f.owner;

        double ocx, ocy;
        tile_centre(src, o, ocx, ocy);
        const double abx = ocx + f.rx, aby = ocy + f.ry;
        // Meander: the query point wanders, so the course does.
        const double qx = f.rx + (value_noise(abx, aby, mea_c, mea_n, 0x21F1u) - 0.5) * 0.14;
        const double qy = f.ry + (value_noise(abx, aby, mea_c, mea_n, 0x21F2u) - 0.5) * 0.14;

        curve cv[48];
        int   ncv = 0;
        for (int kk = -1; kk < 6; ++kk)
        {
            const int t = kk < 0 ? o : nb_index(src, o, kk);
            if (t < 0 || !(src.river_in[t] | src.river_out[t]))
                continue;
            const double ox = kk < 0 ? 0.0 : kNbDx[kk];
            const double oy = kk < 0 ? 0.0 : kNbDy[kk];
            const double hw_t = river_hw(src.river_flow[t]);
            int out_side = -1;
            for (int s = 0; s < 6; ++s)
                if (src.river_out[t] & (1u << s)) { out_side = s; break; }
            const bool wet = src.cls[t] == water;
            bool any_in = false;
            for (int s = 0; s < 6 && ncv < 47; ++s)
            {
                if (!(src.river_in[t] & (1u << s)))
                    continue;
                any_in = true;
                const int u = nb_index(src, t, s);
                const double hw_u = u >= 0 ? river_hw(src.river_flow[u]) : hw_t;
                const double mx = ox + 0.5 * kNbDx[s], my = oy + 0.5 * kNbDy[s];
                if (wet)
                {
                    // Estuary: on into the sea, widening.
                    const double ex = ox + 0.2 * kNbDx[s], ey = oy + 0.2 * kNbDy[s];
                    cv[ncv++] = { mx, my, (mx + ex) * 0.5, (my + ey) * 0.5, ex, ey, hw_u, hw_u * 1.8 };
                }
                else if (out_side >= 0)
                {
                    cv[ncv++] = { mx, my, ox, oy,
                                  ox + 0.5 * kNbDx[out_side], oy + 0.5 * kNbDy[out_side], hw_u, hw_t };
                }
                else
                {
                    cv[ncv++] = { mx, my, (mx + ox) * 0.5, (my + oy) * 0.5, ox, oy, hw_u, hw_u };
                }
            }
            if (!any_in && out_side >= 0 && !wet)
            {
                // Source: a spoke from the centre, tapering to its spring.
                const double ex = ox + 0.5 * kNbDx[out_side], ey = oy + 0.5 * kNbDy[out_side];
                cv[ncv++] = { ox, oy, (ox + ex) * 0.5, (oy + ey) * 0.5, ex, ey, hw_t * 0.35, hw_t };
            }
        }
        if (ncv == 0)
            continue;

        // Nearest point over every curve: signed distance to the bank e.
        double best_e = 1e9, best_hw = 0.0;
        for (int c = 0; c < ncv; ++c)
        {
            const curve& q = cv[c];
            // The control hull bounds the curve: a pixel farther from it than the
            // widest bank can reach cannot be painted by this curve.
            {
                const double reach = std::max({ q.hw0, q.hw1, 0.6 * pxn }) * 1.6 + 0.06 + pxn;
                const double lo_x = std::min({ q.p0x, q.p1x, q.p2x }) - reach;
                const double hi_x = std::max({ q.p0x, q.p1x, q.p2x }) + reach;
                const double lo_y = std::min({ q.p0y, q.p1y, q.p2y }) - reach;
                const double hi_y = std::max({ q.p0y, q.p1y, q.p2y }) + reach;
                if (qx < lo_x || qx > hi_x || qy < lo_y || qy > hi_y)
                    continue;
            }
            double prx = q.p0x, pry = q.p0y;
            for (int sgi = 1; sgi <= kSeg; ++sgi)
            {
                const double t1 = static_cast<double>(sgi) / kSeg;
                const double a = (1.0 - t1) * (1.0 - t1), b = 2.0 * (1.0 - t1) * t1, cc = t1 * t1;
                const double nx = a * q.p0x + b * q.p1x + cc * q.p2x;
                const double ny = a * q.p0y + b * q.p1y + cc * q.p2y;
                double tt;
                const double d = seg_dist(qx, qy, prx, pry, nx, ny, tt);
                const double tc = (sgi - 1 + tt) / kSeg;
                const double hw = std::max(q.hw0 + (q.hw1 - q.hw0) * tc, 0.6 * pxn);
                const double e  = d - hw;
                if (e < best_e)
                {
                    best_e  = e;
                    best_hw = hw;
                }
                prx = nx;
                pry = ny;
            }
        }
        const double bank = 0.05 + 0.6 * best_hw + pxn;
        if (best_e >= bank)
            continue;

        std::uint32_t& dst = out[i];
        float r_ = static_cast<float>(palette::col_r(dst));
        float g_ = static_cast<float>(palette::col_g(dst));
        float b_ = static_cast<float>(palette::col_b(dst));
        // Wet margin: the ground darkens and greens into the bank.
        const float mg = static_cast<float>(sq(std::clamp(1.0 - best_e / bank, 0.0, 1.0))) * k;
        r_ *= 1.0f - 0.34f * mg;
        g_ *= 1.0f - 0.26f * mg;
        b_ *= 1.0f - 0.30f * mg;
        // The water: shelving from shallow at the bank to deep mid-channel.
        const float a = static_cast<float>(std::clamp(0.5 - best_e / pxc, 0.0, 1.0)) * k;
        if (a > 0.0f)
        {
            const float depth = static_cast<float>(std::clamp(-best_e / best_hw, 0.0, 1.0));
            const float dd = std::sqrt(depth);
            const float wr = 84.0f + (34.0f - 84.0f) * dd;
            const float wg = 150.0f + (92.0f - 150.0f) * dd;
            const float wb = 205.0f + (182.0f - 205.0f) * dd;
            r_ += (wr - r_) * a;
            g_ += (wg - g_) * a;
            b_ += (wb - b_) * a;
        }
        blend_px(dst, r_, g_, b_);
    }
}

/// The terrain-feature passes, in the bake's table order (RENDERING.md § The
/// bake, pass by pass): landform relief, then water and rivers — after the
/// biome brushes, before installations and the grade.
void bake_terrain_features(const bake_source& src, const geometry& g, const bake_params& p,
                           int pw, int ph, std::uint32_t* out, const std::uint8_t* tag,
                           const std::uint8_t* cover, const feature_px* fpx)
{
    if (p.landform_strength > 0.0f)
        bake_landforms(src, g, p, pw, ph, out, tag, cover, fpx);
    if (p.river_strength > 0.0f)
        bake_rivers(src, g, p, pw, ph, out, tag, cover, fpx);
}

/// One bake of a window at geometry @p g — which, under supersampling, is the
/// 2x geometry bake_region derives (g.ss > 1): every pass runs here at the
/// actual resolution, keyed on the NOMINAL one for its character. @p tag_out
/// (optional, pw*ph) receives the terrain tag of each output pixel.
void bake_region_at(const bake_source& src, const geometry& g, const bake_params& p,
                    int px0, int py0, int pw, int ph, std::uint32_t* out,
                    std::uint8_t* tag_out)
{
    const double ns = nominal_s(g);
    // APRON (BL-736): the post passes below read neighbours (ink 1 px,
    // unsharp ±2 px), so the window bakes with a margin and crops — a chunk
    // edge can then never seam or halo, because every chunk computed the same
    // overlap. Below 20 px/r no post pass runs and the apron is skipped.
    const int  A   = ns >= 20.0 ? 6 : 0;
    const int  apw = pw + 2 * A, aph = ph + 2 * A;
    const std::size_t an = static_cast<std::size_t>(apw) * aph;
    std::vector<std::uint32_t> abuf(an);
    std::vector<std::uint8_t>  atag(an, 0u), acov(an, 0u);
    std::vector<feature_px>    afpx(an);
    bake_window(src, g, p, px0 - A, py0 - A, apw, aph,
                abuf.data(), atag.data(), acov.data(), afpx.data());

    // Cover-boundary ink BEFORE the stamps: a tree may straddle an inked
    // boundary and should occlude the line, never carry it.
    // Under supersampling the 1 px ink line is 1/ss of a nominal pixel after
    // the downsample, so its strength scales by ss to keep the same weight.
    if (A > 0)
        edge_ink(abuf.data(), acov.data(), apw, aph,
                 std::min(0.9f, p.edge_ink  * static_cast<float>(g.ss)),
                 std::min(0.9f, p.shore_ink * static_cast<float>(g.ss)));

    if (ns >= 40.0 && p.tree_density > 0.0f)
        stamp_trees(src, g, p, px0 - A, py0 - A, apw, aph, abuf.data(), atag.data());

    // Landform relief and carved rivers (BL-1242).
    bake_terrain_features(src, g, p, apw, aph, abuf.data(), atag.data(), acov.data(), afpx.data());
    if (p.installations) // BL-1241: structures, after the trees, before the grade
        stamp_installations(src, g, p, px0 - A, py0 - A, apw, aph, abuf.data(), atag.data());

    // The separable near-future grade. Its haze lift falls and its contrast
    // rises with resolution (BL-736): haze is blur-adjacent, and the close
    // tiers need their local contrast more than their atmosphere.
    if (p.grade_enabled)
    {
        const float res_t = static_cast<float>(std::clamp((ns - 24.0) / 48.0, 0.0, 1.0));
        const float lift     = p.grade_lift * (1.0f - 0.5f * res_t);
        const float contrast = p.grade_contrast + 0.045f * res_t;
        const float haze_r = 15.0f, haze_g = 15.0f, haze_b = 20.0f;
        for (std::size_t i = 0; i < an; ++i)
        {
            if (!atag[i])
                continue;
            float r_ = static_cast<float>(palette::col_r(abuf[i]));
            float g_ = static_cast<float>(palette::col_g(abuf[i]));
            float b_ = static_cast<float>(palette::col_b(abuf[i]));
            const float luma = 0.2126f * r_ + 0.7152f * g_ + 0.0722f * b_;
            r_ = r_ + (luma - r_) * p.grade_desat;
            g_ = g_ + (luma - g_) * p.grade_desat;
            b_ = b_ + (luma - b_) * p.grade_desat;
            r_ *= p.grade_cool[0]; g_ *= p.grade_cool[1]; b_ *= p.grade_cool[2];
            r_ = r_ + (haze_r - r_) * lift;
            g_ = g_ + (haze_g - g_) * lift;
            b_ = b_ + (haze_b - b_) * lift;
            r_ = (r_ - 128.0f) * contrast + 128.0f;
            g_ = (g_ - 128.0f) * contrast + 128.0f;
            b_ = (b_ - 128.0f) * contrast + 128.0f;
            abuf[i] = palette::col32(
                std::clamp(static_cast<int>(r_ + 0.5f), 0, 255),
                std::clamp(static_cast<int>(g_ + 0.5f), 0, 255),
                std::clamp(static_cast<int>(b_ + 0.5f), 0, 255), 255);
        }
    }

    // Unsharp last: it sees the graded, inked, stamped image — the thing the
    // player sees — and restores the edge contrast the interpolation lacks.
    // A supersampled bake defers it: bake_region runs it ONCE, at nominal
    // resolution, after the downsample (never stacked on both).
    if (g.ss <= 1.0 && ns >= 40.0 && p.unsharp_amount > 0.0f)
        unsharp(abuf.data(), atag.data(), apw, aph, p.unsharp_amount);

    for (int y = 0; y < ph; ++y)
    {
        std::memcpy(out + static_cast<std::size_t>(y) * pw,
                    abuf.data() + static_cast<std::size_t>(y + A) * apw + A,
                    static_cast<std::size_t>(pw) * 4u);
        if (tag_out)
            std::memcpy(tag_out + static_cast<std::size_t>(y) * pw,
                        atag.data() + static_cast<std::size_t>(y + A) * apw + A,
                        static_cast<std::size_t>(pw));
    }
}

} // namespace

void bake_region(const bake_source& src, const geometry& g, const bake_params& p,
                 int px0, int py0, int pw, int ph, std::uint32_t* out)
{
    if (src.gw <= 0 || src.gh <= 0)
    {
        std::memset(out, 0, static_cast<std::size_t>(pw) * ph * 4u);
        return;
    }
    const int ss = std::max(1, p.supersample);
    if (ss == 1)
    {
        bake_region_at(src, g, p, px0, py0, pw, ph, out, nullptr);
        return;
    }

    // SUPERSAMPLED (BL-1244): bake the same window at ss x the pixels per hex
    // — the ss geometry is the nominal one scaled exactly (W, H and s all x ss,
    // y_min and lift unchanged), so pixel (ss*x .. ss*x+ss-1) of it covers
    // nominal pixel x precisely and chunks stay seamless and wrap-exact —
    // then box-downsample to nominal. The unsharp pass runs once, on the
    // nominal result, inside a margin M it can read across without seaming.
    geometry gs = g;
    gs.s  = g.s * ss;
    gs.W  = g.W * ss;
    gs.H  = g.H * ss;
    gs.ss = g.ss * ss;
    const bool sharpen = nominal_s(g) >= 40.0 && p.unsharp_amount_ss > 0.0f;
    const int  M  = sharpen ? 3 : 0;
    const int  mw = pw + 2 * M, mh = ph + 2 * M;
    const int  hw = mw * ss,    hh = mh * ss;
    std::vector<std::uint32_t> hi(static_cast<std::size_t>(hw) * hh);
    std::vector<std::uint8_t>  htag(static_cast<std::size_t>(hw) * hh, 0u);
    bake_region_at(src, gs, p, (px0 - M) * ss, (py0 - M) * ss, hw, hh,
                   hi.data(), htag.data());

    // Box downsample, alpha-weighted so the transparent margin never darkens
    // a grid edge. A nominal pixel is TAGGED only when every sub-sample is
    // terrain: the unsharp pass must leave a lock-fill or mask-edge pixel
    // exactly as averaged (a pixel wholly inside the lock fill averages to
    // the lock colour byte-exact).
    std::vector<std::uint32_t> lo(static_cast<std::size_t>(mw) * mh);
    std::vector<std::uint8_t>  ltag(static_cast<std::size_t>(mw) * mh);
    const int n = ss * ss;
    for (int y = 0; y < mh; ++y)
        for (int x = 0; x < mw; ++x)
        {
            int sr = 0, sg = 0, sb = 0, sa = 0;
            std::uint8_t all = 1;
            for (int dy = 0; dy < ss; ++dy)
            {
                const std::size_t row = static_cast<std::size_t>(y * ss + dy) * hw;
                for (int dx = 0; dx < ss; ++dx)
                {
                    const std::size_t i = row + static_cast<std::size_t>(x * ss + dx);
                    const std::uint32_t c = hi[i];
                    const int a = palette::col_a(c);
                    sr += palette::col_r(c) * a;
                    sg += palette::col_g(c) * a;
                    sb += palette::col_b(c) * a;
                    sa += a;
                    all &= htag[i];
                }
            }
            const std::size_t o = static_cast<std::size_t>(y) * mw + x;
            ltag[o] = all;
            lo[o] = sa == 0 ? 0u
                  : palette::col32((sr + sa / 2) / sa, (sg + sa / 2) / sa,
                                   (sb + sa / 2) / sa, (sa + n / 2) / n);
        }

    if (sharpen)
        unsharp(lo.data(), ltag.data(), mw, mh, p.unsharp_amount_ss);

    for (int y = 0; y < ph; ++y)
        std::memcpy(out + static_cast<std::size_t>(y) * pw,
                    lo.data() + static_cast<std::size_t>(y + M) * mw + M,
                    static_cast<std::size_t>(pw) * 4u);
}

int choose_tier(double draw_r, double far_ppr, const double* tier_ppr, int n_tiers)
{
    if (draw_r <= far_ppr || n_tiers <= 0)
        return -1;
    for (int t = 0; t < n_tiers; ++t)
        if (tier_ppr[t] >= draw_r)
            return t;
    return n_tiers - 1; // past the top tier: magnified, the ladder's bound
}

std::uint64_t region_hash(const bake_source& src, const geometry& g,
                          int px0, int py0, int pw, int ph)
{
    // FNV-1a 64 over the tile fields the bake reads, for every tile whose
    // centre could influence the window (the window rect grown by the blend
    // radius). Cheap: a few hundred tiles per chunk.
    std::uint64_t h = 14695981039346656037ull;
    auto mix = [&h](std::uint64_t v) { h ^= v; h *= 1099511628211ull; };
    if (src.gw <= 0 || src.gh <= 0)
        return h;
    // 4.5: the blend radius plus what the feature and structure passes read
    // beyond their owner tile — a river's neighbour-of-neighbour flow (BL-1242)
    // and a port's water-facing neighbours across the structure reach (BL-1241);
    // it also covers the variant falloff plus domain warp (~2.15, BL-1243).
    const double margin = 4.5;
    const double x0 = px0 / g.s - margin,            x1 = (px0 + pw) / g.s + margin;
    const double y0 = py0 / g.s + g.y_min - margin,  y1 = (py0 + ph) / g.s + g.y_min + margin;
    const int r_lo = std::max(0, static_cast<int>(std::floor(y0 / 1.5)));
    const int r_hi = std::min(src.gh - 1, static_cast<int>(std::ceil(y1 / 1.5)));
    const int c_lo = static_cast<int>(std::floor(x0 / kSqrt3)) - 1;
    const int c_hi = static_cast<int>(std::ceil(x1 / kSqrt3)) + 1;
    for (int r = r_lo; r <= r_hi; ++r)
        for (int c = c_lo; c <= c_hi; ++c)
        {
            const int cw = ((c % src.gw) + src.gw) % src.gw;
            const std::size_t i = static_cast<std::size_t>(r) * src.gw + cw;
            mix(src.cls[i]);
            mix(src.colour[i]);
            mix(src.cover[i]);   // the feature stamps read these two, so a
            mix(src.density[i]); // cover change must move the hash
            std::uint32_t hb; static_assert(sizeof(float) == 4);
            std::memcpy(&hb, &src.height[i], 4);       mix(hb);
            std::memcpy(&hb, &src.relief_bias[i], 4);  mix(hb);
            // BL-1242: the landform and river passes read these.
            mix(src.landform[i]);
            mix(src.lf_links[i]);
            mix(static_cast<std::uint64_t>(src.river_in[i]) | (static_cast<std::uint64_t>(src.river_out[i]) << 8));
            std::memcpy(&hb, &src.river_flow[i], 4);   mix(hb);
            // BL-1243: the variant passes read the family (a substrate change
            // can move it without moving the colour) and the variant index.
            mix(static_cast<std::uint64_t>(src.family[i]) | (static_cast<std::uint64_t>(src.variant[i]) << 8));
        }
    mix(installation_hash(src, g, px0, py0, pw, ph)); // BL-1241: builds, razes, scale steps
    return h;
}

} // namespace ui::ground
