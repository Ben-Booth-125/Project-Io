#include "star_map_view.hpp"

#include "world/star_map.hpp"

#include <cmath>
#include <cstdint>

namespace ui {

namespace {

constexpr float k_pi = 3.14159265358979f;

/// A cheap integer hash for the faint background field. Not an RNG and not
/// seeded — the same index always yields the same point, so the sky is
/// identical every campaign and every frame.
uint32_t hash_u32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7FEB352Du;
    x ^= x >> 15; x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}
float hash_unit(uint32_t x) { return static_cast<float>(hash_u32(x) & 0xFFFFFFu) / 16777216.0f; }

ImU32 with_alpha(ImU32 rgb, float a)
{
    const int A = static_cast<int>((a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a)) * 255.0f);
    return (rgb & 0x00FFFFFFu) | (static_cast<ImU32>(A) << 24);
}

void magnitude_to_look(float mag, float& radius, float& alpha)
{
    const float t = 1.0f - (mag < 0.0f ? 0.0f : (mag > 6.0f ? 6.0f : mag)) / 6.0f;
    radius = 0.55f + t * t * 1.95f;   // squared, so the bright few actually stand out
    alpha  = 0.34f + t * 0.66f;
}

/// Height of the galactic band at sky-x, as a y in [0,1]. A shallow sine so the
/// band sweeps across rather than cutting the panel in half, phased so its
/// lowest point sits under the core.
float band_y(float x, float core_x)
{
    return 0.5f + std::sin((x - core_x) * 2.0f * k_pi) * (star_map_band_tilt() * 0.5f);
}

} // namespace

void draw_star_map(ImVec2 origin, ImVec2 size, float day_of_year)
{
    // Background list, matching every other canvas: these draw OUTSIDE any ImGui
    // window, so GetWindowDrawList() would target whatever window is current —
    // which at this call site is none.
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 br{ origin.x + size.x, origin.y + size.y };
    dl->PushClipRect(origin, br, true);

    // THE SKY TURNS. One full revolution per year, so the constellations
    // overhead on a spring evening are not the ones overhead in autumn — the
    // panel shows the sky on THIS day, not a fixed poster. Wrapping in x is why
    // the projection runs all the way round: the seam has to close.
    const float turn = day_of_year / 365.0f;
    const auto wrapx = [](float x) { x -= std::floor(x); return x; };
    const auto at = [&](float x, float y) {
        return ImVec2{ origin.x + wrapx(x - turn) * size.x, origin.y + y * size.y };
    };
    /// True when a segment would cross the wrap seam — drawing it would smear a
    /// line right across the panel, so those are skipped rather than clipped.
    const auto spans_seam = [&](float xa, float xb) {
        return std::fabs(wrapx(xa - turn) - wrapx(xb - turn)) > 0.5f;
    };

    // Night, a touch warm so the panel is not a hole in the UI.
    dl->AddRectFilled(origin, br, IM_COL32(8, 10, 19, 255));

    int obj_count = 0;
    const star_map_object* objs = star_map_objects(obj_count);

    // Where the core sits, so the band can be phased to run through it.
    float core_x = 0.3f;
    for (int i = 0; i < obj_count; ++i)
        if (objs[i].kind == deep_sky_kind::galactic_core) core_x = objs[i].x;

    // --- The galactic band, drawn as a soft arc of stacked segments ---------
    // Sampled across the sky so it follows band_y rather than being a straight
    // strip: from the ground the band is an arc, and a level bar reads as a UI
    // element instead of a sky.
    {
        constexpr int k_seg = 96;
        for (int s = 0; s < k_seg; ++s)
        {
            const float x0 = static_cast<float>(s) / k_seg;
            const float x1 = static_cast<float>(s + 1) / k_seg;
            if (spans_seam(x0, x1)) continue;
            // Thicker and brighter toward the core, thinning away from it.
            float d = std::fabs(x0 - core_x);
            if (d > 0.5f) d = 1.0f - d;                 // shortest way round
            const float near_core = 1.0f - d * 2.0f;    // 1 at the core, 0 opposite
            const float thick = 0.055f + near_core * near_core * 0.075f;
            const float alpha = 0.030f + near_core * near_core * 0.055f;
            for (int layer = 3; layer >= 1; --layer)
            {
                const float lt = static_cast<float>(layer) / 3.0f;
                const ImVec2 a0 = at(x0, band_y(x0, core_x) - thick * lt);
                const ImVec2 a1 = at(x1, band_y(x1, core_x) + thick * lt);
                dl->AddRectFilled({ a0.x, a0.y }, { a1.x, a1.y },
                                  with_alpha(0x7888C0u, alpha * (1.1f - lt * 0.6f)));
            }
        }
    }

    // --- Faint unnamed field, denser near the band -------------------------
    for (int s = 0; s < 260; ++s)
    {
        const uint32_t h = static_cast<uint32_t>(4001 + s);
        const float x = hash_unit(h * 3u + 1u);
        // Pull toward the band: a squared bias concentrates the field where the
        // disc is without ever leaving the poles empty.
        const float u = hash_unit(h * 5u + 2u) * 2.0f - 1.0f;
        const float y = band_y(x, core_x) + u * std::fabs(u) * 0.52f;
        if (y < 0.0f || y > 1.0f) continue;
        const float mag = hash_unit(h * 7u + 3u);
        dl->AddCircleFilled(at(x, y), 0.4f + mag * 0.6f,
                            with_alpha(0xE8EEFFu, 0.10f + mag * 0.30f), 6);
    }

    // --- Deep sky, under the named stars ------------------------------------
    for (int i = 0; i < obj_count; ++i)
    {
        const star_map_object& o = objs[i];
        const ImVec2 p = at(o.x, o.y);
        const float  r = o.size * size.x;
        switch (o.kind)
        {
            case deep_sky_kind::galactic_core:
            {
                // The bulge, and the black hole inside it. Bright, diffuse, and
                // unmistakably the brightest region of the band — from the
                // ground the centre of the galaxy is a DIRECTION, so it is a
                // glow you could point at rather than an object with an edge.
                for (int k = 7; k >= 1; --k)
                {
                    const float t = static_cast<float>(k) / 7.0f;
                    dl->AddCircleFilled(p, r * (0.34f + t * 1.5f),
                                        with_alpha(0xFFD9A8u, (1.0f - t) * (1.0f - t) * 0.115f), 34);
                }
                dl->AddCircleFilled(p, r * 0.18f, with_alpha(0xFFF0D0u, 0.55f), 20);
                // The event horizon. Drawn LARGER than honesty allows — from a
                // hilltop you would see the bulge and never the hole — because
                // at minimap size a truthful dot is two pixels and reads as a
                // stray star. This is the one thing on the panel that is a
                // symbol rather than a picture, and it earns it: the black hole
                // is what the whole galaxy is turning around.
                dl->AddCircleFilled(p, r * 0.095f, IM_COL32(0, 0, 0, 255), 20);
                dl->AddCircle(p, r * 0.095f, with_alpha(0xFFD79Cu, 0.95f), 20, 1.5f);
                dl->AddCircle(p, r * 0.165f, with_alpha(0xFFD79Cu, 0.30f), 22, 1.0f);
                break;
            }
            case deep_sky_kind::quasar:
                dl->AddCircleFilled(p, r * 0.6f, with_alpha(0xD6E2FFu, 0.95f), 10);
                dl->AddLine({ p.x - r * 3.2f, p.y }, { p.x + r * 3.2f, p.y }, with_alpha(0x96B4FFu, 0.40f), 1.0f);
                dl->AddLine({ p.x, p.y - r * 3.2f }, { p.x, p.y + r * 3.2f }, with_alpha(0x96B4FFu, 0.40f), 1.0f);
                break;
            case deep_sky_kind::supernova_remnant:
                // Pale and soft, not a saturated ring. At minimap size a strong
                // hue reads as a UI marker rather than an object in the sky —
                // and these sit among genuinely green-free surroundings, so any
                // saturation at all shouts.
                dl->AddCircle(p, r, with_alpha(0xBFE8E4u, 0.34f), 18, 1.0f);
                dl->AddCircleFilled(p, r * 0.72f, with_alpha(0x9CCFCAu, 0.07f), 16);
                break;
            case deep_sky_kind::nebula:
                dl->AddCircleFilled(p, r,         with_alpha(0x9660A8u, 0.15f), 20);
                dl->AddCircleFilled(p, r * 0.62f, with_alpha(0xB278BEu, 0.14f), 18);
                break;
            case deep_sky_kind::cluster:
                dl->AddCircleFilled(p, r * 0.55f, with_alpha(0xE0E2ECu, 0.16f), 16);
                for (int k = 0; k < 8; ++k)
                {
                    const uint32_t h = static_cast<uint32_t>(i * 331 + k);
                    const float ang = hash_unit(h * 3u + 1u) * 2.0f * k_pi;
                    const float rr  = hash_unit(h * 5u + 2u) * r * 0.8f;
                    dl->AddCircleFilled({ p.x + std::cos(ang) * rr, p.y + std::sin(ang) * rr },
                                        0.7f, with_alpha(0xFFFFFFu, 0.62f), 6);
                }
                break;
            case deep_sky_kind::galaxy:
                // Stacked, brightening inward. A single translucent disc over
                // black renders as dark grey and reads as a HOLE punched in the
                // sky rather than a smudge of distant light.
                for (int k = 4; k >= 1; --k)
                {
                    const float t = static_cast<float>(k) / 4.0f;
                    dl->AddCircleFilled(p, r * (0.22f + t * 0.62f),
                                        with_alpha(0xD8D2ECu, 0.10f), 20);
                }
                dl->AddCircleFilled(p, r * 0.16f, with_alpha(0xF0EAFFu, 0.45f), 14);
                break;
        }
    }

    // --- Constellation lines, under the star discs --------------------------
    int star_count = 0;
    const star_map_star* stars = star_map_stars(star_count);
    int con_count = 0;
    const star_map_constellation* cons = star_map_constellations(con_count);
    for (int i = 0; i < con_count; ++i)
    {
        const star_map_constellation& cst = cons[i];
        for (int e = 0; e < cst.edge_count; ++e)
        {
            const int a = cst.stars[cst.edges[e * 2]];
            const int b = cst.stars[cst.edges[e * 2 + 1]];
            if (a < 0 || a >= star_count || b < 0 || b >= star_count) continue;
            if (spans_seam(stars[a].x, stars[b].x)) continue;
            dl->AddLine(at(stars[a].x, stars[a].y), at(stars[b].x, stars[b].y),
                        with_alpha(0x7084BEu, 0.30f), 1.0f);
        }
    }

    for (int i = 0; i < star_count; ++i)
    {
        const star_map_star& s = stars[i];
        float r = 0.0f, a = 0.0f;
        magnitude_to_look(s.magnitude, r, a);
        const ImVec2 p = at(s.x, s.y);
        if (s.magnitude < 2.0f)
            dl->AddCircleFilled(p, r * 2.1f, with_alpha(star_class_rgb(s.klass), a * 0.16f), 12);
        dl->AddCircleFilled(p, r, with_alpha(star_class_rgb(s.klass), a), 10);
    }

    dl->PopClipRect();
    dl->AddRect(origin, br, IM_COL32(70, 78, 104, 200));
}

} // namespace ui
