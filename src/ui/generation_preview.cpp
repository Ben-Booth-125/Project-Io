#include "generation_preview.hpp"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace ui {

namespace {

// Deterministic hash for the drawing's one texture decision (the metallicity
// speckle), seeded from the params' own numbers.
uint32_t fnv1a(const void* data, std::size_t n, uint32_t h = 2166136261u)
{
    const auto* p = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
    return h;
}

// xorshift step — cheap, deterministic, good enough for speckle placement.
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

// Small archetype disc colour for the system view.
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
    dl->AddText({at.x - ts.x * 0.5f, at.y}, col, text); // fit-exempt: free-floating canvas label, sized to its own text, no bounding box to overflow
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

} // namespace

void draw_system_preview(const preview_body* bodies, std::size_t count,
                         const planetology_params& params)
{
    if (!bodies || count == 0) return;

    ImDrawList*  dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 sz = ImGui::GetContentRegionAvail();
    if (sz.x < 50.0f || sz.y < 50.0f) return;

    dl->PushClipRect(p0, {p0.x + sz.x, p0.y + sz.y}, true);
    paint_system(dl, p0, sz, bodies, count, params);
    dl->PopClipRect();
    ImGui::Dummy(sz);
}

} // namespace ui
