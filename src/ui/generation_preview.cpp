#include "generation_preview.hpp"

#include "hex_render.hpp" // ui::terrain_colour — the canvas palette, shared so
                          // the preview and the Planetary canvas agree by construction

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace ui {

namespace {

// Deterministic hash for the painting's "texture" decisions (land-blob
// latitudes, scar placement). Seeded from the state's own numbers, so the
// picture changes exactly when a reroll changes the world and never otherwise.
uint32_t fnv1a(const void* data, std::size_t n, uint32_t h = 2166136261u)
{
    const auto* p = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
    return h;
}

uint32_t body_hash(const planetology_state& st)
{
    uint32_t h = 2166136261u;
    h = fnv1a(&st.shore, sizeof st.shore, h);
    h = fnv1a(&st.o2_fraction, sizeof st.o2_fraction, h);
    h = fnv1a(&st.surface_temp_k, sizeof st.surface_temp_k, h);
    h = fnv1a(&st.theta, sizeof st.theta, h);
    return h;
}

// xorshift step — cheap, deterministic, good enough for blob placement.
uint32_t next(uint32_t& s)
{
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}
float frand(uint32_t& s) { return static_cast<float>(next(s) & 0xFFFFFF) / 16777215.0f; }

ImU32 lerp_col(ImU32 a, ImU32 b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const auto ch = [&](int sh) {
        const float av = static_cast<float>((a >> sh) & 0xFF);
        const float bv = static_cast<float>((b >> sh) & 0xFF);
        return static_cast<ImU32>(av + (bv - av) * t) << sh;
    };
    return ch(0) | ch(8) | ch(16) | ch(24);
}

// Star colour from mass: M-dwarf red through sun-warm white to A-type blue.
ImU32 star_colour(float mass)
{
    constexpr ImU32 red   = IM_COL32(255, 140,  90, 255);
    constexpr ImU32 warm  = IM_COL32(255, 244, 214, 255);
    constexpr ImU32 blue  = IM_COL32(190, 210, 255, 255);
    if (mass < 1.0f) return lerp_col(red, warm, (mass - 0.6f) / 0.4f);
    return lerp_col(warm, blue, (mass - 1.0f) / 0.5f);
}

// Base land tint: what the ground reads as, from temperature and how far life got.
ImU32 land_colour(const planetology_state& st)
{
    switch (st.profile.temperature)
    {
        case temperature_class::scorching: return IM_COL32(168, 112,  72, 255);
        case temperature_class::frozen:    return IM_COL32(196, 205, 214, 255);
        default: break;
    }
    // Temperate-ish ground greens with the biosphere.
    if (st.stage >= life_stage::land)    return IM_COL32( 86, 128,  74, 255);
    if (st.stage >= life_stage::complex) return IM_COL32(122, 118,  84, 255);
    if (st.stage >= life_stage::microbial) return IM_COL32(134, 110,  88, 255);
    return IM_COL32(128, 122, 118, 255); // sterile regolith grey
}

ImU32 ocean_colour(const planetology_state& st)
{
    // A mat-world's anoxic sea reads green-black; an oxygenated one reads blue.
    if (st.stage >= life_stage::oxygenated) return IM_COL32( 38,  84, 148, 255);
    if (st.stage >= life_stage::microbial)  return IM_COL32( 44,  92,  86, 255);
    return IM_COL32( 52,  76, 104, 255);
}

// Small archetype disc colour for system-view / thumbnail bodies.
ImU32 archetype_colour(const planetology_state& st)
{
    switch (st.archetype)
    {
        case body_archetype::cradle:        return IM_COL32(110, 150, 190, 255);
        case body_archetype::mat_world:     return IM_COL32( 96, 130, 110, 255);
        case body_archetype::boring_billion:return IM_COL32(120, 130, 120, 255);
        case body_archetype::graveyard:     return IM_COL32(140, 128, 104, 255);
        case body_archetype::waterworld:    return IM_COL32( 70, 110, 170, 255);
        case body_archetype::oven:          return IM_COL32(210, 160,  90, 255);
        case body_archetype::snowball_lock: return IM_COL32(205, 215, 225, 255);
        case body_archetype::relict_wet:    return IM_COL32(180, 120,  90, 255);
        case body_archetype::ocean_vault:   return IM_COL32(150, 180, 205, 255);
        case body_archetype::cryo_organic:  return IM_COL32(190, 170, 110, 255);
        case body_archetype::bake_out:      return IM_COL32(200, 180, 120, 255);
        default:                            return IM_COL32(120, 120, 120, 255);
    }
}

constexpr ImU32 col_dim = IM_COL32(120, 128, 145, 255);

void label_centred(ImDrawList* dl, ImVec2 at, const char* text, ImU32 col)
{
    const ImVec2 ts = ImGui::CalcTextSize(text);
    dl->AddText({at.x - ts.x * 0.5f, at.y}, col, text);
}

// ---------------------------------------------------------------------------
// Round 0 — the system: star, orbits, bodies.
// ---------------------------------------------------------------------------
void paint_system(ImDrawList* dl, ImVec2 p0, ImVec2 sz,
                  const preview_body* bodies, std::size_t count,
                  const planetology_params& params)
{
    const float cy = p0.y + sz.y * 0.45f;

    // The star at the left edge; size and colour from its mass.
    const ImU32 sc     = star_colour(params.star_mass);
    const float star_r = sz.y * (0.085f + 0.05f * (params.star_mass - 0.6f));
    const ImVec2 star_c{p0.x + star_r * 0.4f, cy};
    dl->AddCircleFilled(star_c, star_r * 1.35f, (sc & 0x00FFFFFF) | (22u << 24));
    dl->AddCircleFilled(star_c, star_r, sc);

    // Orbit positions: sqrt spacing so the inner system does not crush together.
    float max_au = 1.0f;
    for (std::size_t i = 0; i < count; ++i)
        max_au = std::max(max_au, bodies[i].orbit_au);
    const float x_lo = p0.x + star_r * 1.4f + 24.0f;
    const float x_hi = p0.x + sz.x - 80.0f;
    const auto orbit_x = [&](float au) {
        return x_lo + (x_hi - x_lo) * std::sqrt(au / max_au);
    };

    for (std::size_t i = 0; i < count; ++i)
    {
        const preview_body& b = bodies[i];
        if (!b.state) continue;
        const bool  moon = b.parent_orbit_au > 0.0f;
        const float x    = orbit_x(b.orbit_au);
        const float r    = std::clamp(6.0f + 10.0f * std::cbrt(b.mass_earths), 4.0f, 20.0f)
                               * (moon ? 0.7f : 1.0f);

        // A moon rides above its primary (which shares its orbit_au), on a small
        // orbit ring of its own; a planet sits on the ecliptic line with a faint
        // arc of its solar track.
        const float y = moon ? cy - 46.0f : cy;
        if (moon)
            dl->AddCircle({x, cy}, 46.0f, IM_COL32(120, 128, 145, 55), 0, 1.0f);
        else
        {
            dl->PathArcTo({star_c.x, cy}, x - star_c.x, -0.35f, 0.35f, 24);
            dl->PathStroke(IM_COL32(120, 128, 145, 40), 0, 1.0f);
        }

        const ImU32 bc = archetype_colour(*b.state);
        dl->AddCircleFilled({x, y}, r, bc);

        // Thin atmosphere rim where there is one to see.
        if (b.state->profile.atmosphere >= atmosphere_class::moderate)
            dl->AddCircle({x, y}, r + 2.5f, (bc & 0x00FFFFFF) | (90u << 24), 0, 1.5f);

        if (b.is_home)
            dl->AddCircle({x, y}, r + 6.0f, IM_COL32(225, 230, 240, 200), 0, 2.0f);

        // Moon labels stack upward so they never collide with the primary's.
        const float ly = moon ? y - r - 10.0f - 2.0f * ImGui::GetTextLineHeight()
                              : y + r + 10.0f;
        label_centred(dl, {x, ly}, b.name,
                      b.is_home ? IM_COL32(225, 230, 240, 255) : col_dim);
        label_centred(dl, {x, ly + ImGui::GetTextLineHeight()},
                      archetype_name(b.state->archetype), col_dim);
    }

    // Metallicity as the nebula's dust: a faint speckle field, denser when rich.
    uint32_t s = fnv1a(&params.metallicity, sizeof params.metallicity, 77u);
    const int specks = static_cast<int>(60.0f * params.metallicity);
    for (int i = 0; i < specks; ++i)
    {
        const ImVec2 q{p0.x + frand(s) * sz.x, p0.y + frand(s) * sz.y};
        dl->AddCircleFilled(q, 1.0f, IM_COL32(200, 205, 220, 45));
    }
}

// ---------------------------------------------------------------------------
// The homeworld — shared by rounds 1 and 2.
//
// Drawn as a HEX-SAMPLED orthographic globe: a pointy-top hex grid covers the
// disc, and each hex centre is inverse-projected to (longitude, latitude) on
// the sphere, then classified land / ocean / ice from continent blobs defined
// in sphere coordinates. Two things fall out of sampling on the sphere rather
// than painting on the disc:
//   * the surface honestly previews the game's hex-tile world (the outlines
//     ARE the reading of the map the player will inherit), and
//   * rotation is just an offset on the sampled longitude — the far side
//     comes around for free.
// ---------------------------------------------------------------------------

/// One continent blob on the sphere, in (lon, lat_n) where lat_n = sin(latitude)
/// in [-1, 1] — the same vertical measure the orthographic projection uses.
struct land_blob
{
    float lon0, lat0;   ///< Centre.
    float dlon, dlat;   ///< Elliptical half-extents.
    float skew;         ///< Coastline drift: lon centre shifts with latitude.
};

constexpr int blob_count = 6;

/// Derive the roll's continents. Deterministic in the state's own numbers, so
/// the same world always grows the same land — rotation or not.
void derive_blobs(const planetology_state& st, land_blob out[blob_count])
{
    uint32_t s = body_hash(st);
    const float water = std::clamp(st.profile.water_fraction, 0.0f, 1.0f);
    const float ls    = 1.0f - water;
    for (int i = 0; i < blob_count; ++i)
    {
        out[i].lon0 = frand(s) * 6.2831853f;
        out[i].lat0 = frand(s) * 1.7f - 0.85f;
        out[i].dlat = (0.14f + 0.34f * ls) * (0.55f + 0.9f * frand(s));
        out[i].dlon = (0.55f + 1.30f * ls) * (0.55f + 0.9f * frand(s));
        out[i].skew = (frand(s) - 0.5f) * 1.2f;
    }
}

bool land_at(const land_blob blobs[blob_count], float lon, float lat_n)
{
    for (int i = 0; i < blob_count; ++i)
    {
        const land_blob& b = blobs[i];
        float dl = lon - (b.lon0 + b.skew * (lat_n - b.lat0));
        while (dl >  3.14159265f) dl -= 6.2831853f;   // shortest way round
        while (dl < -3.14159265f) dl += 6.2831853f;
        const float u = dl / b.dlon;
        const float v = (lat_n - b.lat0) / b.dlat;
        if (u * u + v * v < 1.0f) return true;
    }
    return false;
}

void paint_homeworld(ImDrawList* dl, ImVec2 c, float R, const planetology_state& st,
                     float drawdown_shown, float rot,
                     const preview_surface_view& surface)
{
    const bool real = surface.comp && surface.gw > 0 && surface.gh > 0;
    const bool  has_ocean = st.profile.hydrology == hydrological_state::liquid;
    const bool  frozen    = st.profile.temperature == temperature_class::frozen;
    const float ice_lat   = st.profile.temperature == temperature_class::cold ? 0.58f
                          : st.profile.temperature == temperature_class::temperate ? 0.80f
                          : 2.0f; // hot worlds: no cap
    const ImU32 lc = land_colour(st);
    const ImU32 oc = ocean_colour(st);
    const ImU32 ic = IM_COL32(216, 223, 232, 255);

    land_blob blobs[blob_count];
    derive_blobs(st, blobs);

    // The hex lattice lives ON THE SPHERE — rows of constant latitude, each
    // ring closed by an even longitude step — and every tile is projected to
    // screen per frame. So the tiles themselves travel as the world turns:
    // fresh hexes roll in over the limb, foreshortening as they cross it,
    // and a tile keeps its identity (and its scar) for the whole revolution.
    constexpr float ha       = 0.040f;             // angular circumradius (rad)
    constexpr float half_pi  = 1.5707963f;
    constexpr float two_pi   = 6.2831853f;
    const float     dlat     = ha * 1.5f;
    const int       n_rows   = static_cast<int>(3.14159265f / dlat);

    const uint32_t scar_salt = body_hash(st) ^ 0x9E3779B9u;

    for (int row = 0; row <= n_rows; ++row)
    {
        const float lat = -half_pi + (static_cast<float>(row) + 0.5f) * dlat;
        if (std::fabs(lat) > 1.53f) continue;      // the polar sliver
        const float cl = std::cos(lat);

        // Close each ring with an even step near the target hex width, so the
        // seam at lon 0 never shows; rings shed tiles toward the poles.
        const int n_cols = std::max(3,
            static_cast<int>(two_pi * cl / (ha * 1.7320508f)));
        const float lon_step = two_pi / static_cast<float>(n_cols);
        const float base     = (row & 1) ? lon_step * 0.5f : 0.0f;

        for (int col = 0; col < n_cols; ++col)
        {
            const float lon_abs = base + static_cast<float>(col) * lon_step;
            float lon_rel = lon_abs - rot;
            while (lon_rel >  3.14159265f) lon_rel -= two_pi;
            while (lon_rel < -3.14159265f) lon_rel += two_pi;

            const float zc = cl * std::cos(lon_rel);
            if (zc < 0.02f) continue;              // far hemisphere

            // Classification in ABSOLUTE sphere coordinates: the tile is the
            // same tile whatever the viewing angle. With a real surface the hex
            // samples the actual generated raster (nearest tile: rows run
            // north-pole-first, linear in latitude — band_for_row's own
            // convention); the blob path remains only as the fallback while
            // the first async build is in flight.
            const float lat_n = std::sin(lat);
            ImU32 fill;
            bool  on_land = false;
            if (real)
            {
                float lw = lon_abs;
                while (lw < 0.0f)        lw += two_pi;
                while (lw >= two_pi)     lw -= two_pi;
                const int tc = std::min(surface.gw - 1,
                    static_cast<int>(lw / two_pi * static_cast<float>(surface.gw)));
                const float p  = 0.5f - lat / 3.14159265f;
                const int   tr = std::clamp(
                    static_cast<int>(p * static_cast<float>(surface.gh - 1) + 0.5f),
                    0, surface.gh - 1);
                const auto comp = static_cast<terrain_composition>(
                    surface.comp[static_cast<std::size_t>(tr) * static_cast<std::size_t>(surface.gw)
                                 + static_cast<std::size_t>(tc)]);
                fill    = terrain_colour(comp);
                on_land = comp != terrain_composition::ocean;
            }
            else if (frozen)                         fill = ic;
            else if (std::fabs(lat_n) > ice_lat && st.profile.hydrology
                                                       != hydrological_state::none)
                                                     fill = ic;
            else if (!has_ocean)                   { fill = lc; on_land = true; }
            else if (land_at(blobs, lon_abs, lat_n)) { fill = lc; on_land = true; }
            else                                     fill = oc;

            // Limb darkening from the tile normal's view-depth.
            const float shade  = 0.55f + 0.45f * std::sqrt(zc);
            const ImU32 shaded = lerp_col(IM_COL32(8, 9, 12, 255), fill, shade);

            // Worked-surface scars keyed to the LATTICE cell — stable per tile.
            bool scarred = false;
            if (on_land && drawdown_shown > 0.005f)
            {
                const uint32_t h = ((static_cast<uint32_t>(row) * 92821u
                                     + static_cast<uint32_t>(col)) ^ scar_salt)
                                   * 2654435761u;
                scarred = (h >> 24) < static_cast<uint32_t>(drawdown_shown * 190.0f);
            }

            // Project the six corners individually; a corner past the limb is
            // clamped to it, so boundary tiles flatten against the edge as
            // they arrive rather than folding through it.
            ImVec2 v[6];
            for (int k = 0; k < 6; ++k)
            {
                const float a     = (static_cast<float>(k) * 60.0f - 30.0f)
                                        * 0.01745329f;
                const float lat_v = lat + ha * std::sin(a);
                const float lon_v = std::clamp(
                    lon_rel + ha * std::cos(a) / std::max(cl, 0.05f),
                    -half_pi, half_pi);
                v[k] = {c.x + std::cos(lat_v) * std::sin(lon_v) * R,
                        c.y - std::sin(lat_v) * R};
            }
            dl->AddConvexPolyFilled(v, 6, scarred
                ? lerp_col(shaded, IM_COL32(26, 24, 22, 255), 0.75f) : shaded);
            // The hexagonal outline — the tile grid the campaign will inherit.
            dl->AddPolyline(v, 6, IM_COL32(10, 12, 16, 90), ImDrawFlags_Closed, 1.0f);
        }
    }

    // Atmosphere halo: thickness from pressure, tint from what is in it.
    if (st.profile.atmosphere != atmosphere_class::none)
    {
        const float th = std::clamp(st.surface_pressure_bar, 0.15f, 2.5f) * 4.0f;
        const ImU32 tint = st.o2_fraction > 0.05f
            ? IM_COL32(150, 190, 240, 70)   // oxygenated sky-blue
            : IM_COL32(200, 180, 120, 60);  // anoxic haze
        for (int i = 0; i < 3; ++i)
            dl->AddCircle(c, R + 2.0f + th * (static_cast<float>(i) + 1.0f) / 3.0f,
                          (tint & 0x00FFFFFF) | ((70u - 20u * i) << 24), 0, 2.0f);
    }

    // Terminator highlight — reads as a sphere rather than a flat disc.
    dl->PathArcTo(c, R, -1.9f, 1.25f, 48);
    dl->PathStroke(IM_COL32(255, 255, 255, 26), 0, R * 0.08f);
}

void paint_homeworld_round(ImDrawList* dl, ImVec2 p0, ImVec2 sz,
                           const preview_body* bodies, std::size_t count,
                           std::size_t home, float drawdown_shown, float rot,
                           const preview_surface_view& surface)
{
    const planetology_state& st = *bodies[home].state;

    const float R = std::min(sz.x, sz.y) * 0.34f;
    const ImVec2 c{p0.x + sz.x * 0.5f, p0.y + sz.y * 0.44f};
    paint_homeworld(dl, c, R, st, drawdown_shown, rot, surface);
    label_centred(dl, {c.x, c.y + R + 14.0f}, bodies[home].name,
                  IM_COL32(225, 230, 240, 255));

    // The rest of the system as small thumbnails along the bottom — the reader
    // keeps the context that the homeworld is one draw among siblings.
    std::size_t others = 0;
    for (std::size_t i = 0; i < count; ++i)
        if (i != home && bodies[i].state) ++others;
    const float ty = p0.y + sz.y - 46.0f;
    float tx = p0.x + sz.x - 32.0f - 86.0f * static_cast<float>(others ? others - 1 : 0);
    for (std::size_t i = 0; i < count; ++i)
    {
        if (i == home || !bodies[i].state) continue;
        const float r = 9.0f;
        dl->AddCircleFilled({tx, ty}, r, archetype_colour(*bodies[i].state));
        label_centred(dl, {tx, ty + r + 4.0f}, bodies[i].name, col_dim);
        tx += 86.0f;
    }
}

} // namespace

void draw_generation_preview(const preview_body* bodies, std::size_t count,
                             const planetology_params& params, int round, float rot,
                             const preview_surface_view& surface)
{
    if (!bodies || count == 0) return;

    std::size_t home = 0;
    for (std::size_t i = 0; i < count; ++i)
        if (bodies[i].is_home) { home = i; break; }
    if (!bodies[home].state) return;

    ImDrawList*  dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 sz = ImGui::GetContentRegionAvail();
    if (sz.x < 50.0f || sz.y < 50.0f) return;

    dl->PushClipRect(p0, {p0.x + sz.x, p0.y + sz.y}, true);

    switch (round)
    {
        case 0:
            paint_system(dl, p0, sz, bodies, count, params);
            break;
        case 1:
            // The surface before its industrial history: drawdown not yet shown.
            paint_homeworld_round(dl, p0, sz, bodies, count, home, 0.0f, rot, surface);
            break;
        default:
            // Round 2 and beyond: the same world, worked.
            paint_homeworld_round(dl, p0, sz, bodies, count, home,
                                  bodies[home].state->drawdown, rot, surface);
            break;
    }

    dl->PopClipRect();
    ImGui::Dummy(sz);
}

} // namespace ui
