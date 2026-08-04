#include "star_map_view.hpp"

#include "world/star_map.hpp"

#include <cmath>

namespace ui {

namespace {

ImU32 rgb_to_col(uint32_t rgb, float alpha)
{
    return IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF,
                    static_cast<int>(alpha * 255.0f));
}

/// Apparent magnitude to a drawn radius and alpha. Magnitude runs BACKWARDS —
/// 0 is brilliant, 6 is the naked-eye limit — so both curves invert it.
void magnitude_to_look(float mag, float& radius, float& alpha)
{
    const float t = 1.0f - (mag < 0.0f ? 0.0f : (mag > 6.0f ? 6.0f : mag)) / 6.0f;
    radius = 0.55f + t * t * 1.95f;   // squared so the bright few actually stand out
    alpha  = 0.34f + t * 0.66f;
}

} // namespace

void draw_star_map(ImVec2 origin, ImVec2 size)
{
    // Background list, matching every other canvas: these are drawn OUTSIDE any
    // ImGui window, so GetWindowDrawList() targets whatever window happens to be
    // current — which at this call site is none, and the map silently vanishes.
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImVec2 br{ origin.x + size.x, origin.y + size.y };
    const auto at = [&](float x, float y) {
        return ImVec2{ origin.x + x * size.x, origin.y + y * size.y };
    };

    // Deep sky background, a touch warmer than pure black so the panel does not
    // read as a hole in the UI.
    dl->AddRectFilled(origin, br, IM_COL32(9, 11, 20, 255));

    // The galactic band: a soft brightening across the middle. Two vertical
    // gradients meeting at the centre line — stacking translucent strips instead
    // reads as scanlines at minimap size, where the whole band is only ~90 px
    // tall and every strip edge lands on its own row of pixels.
    {
        const ImU32 edge = IM_COL32(120, 132, 190, 0);
        const ImU32 core = IM_COL32(120, 132, 190, 30);
        dl->AddRectFilledMultiColor(at(0.0f, 0.24f), at(1.0f, 0.50f), edge, edge, core, core);
        dl->AddRectFilledMultiColor(at(0.0f, 0.50f), at(1.0f, 0.76f), core, core, edge, edge);
    }

    int obj_count = 0;
    const star_map_object* objs = star_map_objects(obj_count);
    for (int i = 0; i < obj_count; ++i)
    {
        const star_map_object& o = objs[i];
        const ImVec2 c = at(o.x, o.y);
        const float  r = o.size * size.x;
        switch (o.kind)
        {
            case deep_sky_kind::quasar:
                // Small, violently bright, with a cross flare — it should read as
                // "not a star" at a glance.
                dl->AddCircleFilled(c, r * 0.55f, IM_COL32(214, 226, 255, 255), 10);
                dl->AddLine({ c.x - r * 2.6f, c.y }, { c.x + r * 2.6f, c.y },
                            IM_COL32(150, 180, 255, 110), 1.0f);
                dl->AddLine({ c.x, c.y - r * 2.6f }, { c.x, c.y + r * 2.6f },
                            IM_COL32(150, 180, 255, 110), 1.0f);
                break;
            case deep_sky_kind::supernova_remnant:
                // A ring of gas: what is left, not the flash.
                dl->AddCircle(c, r, IM_COL32(126, 214, 196, 130), 16, 1.2f);
                dl->AddCircle(c, r * 0.62f, IM_COL32(126, 214, 196, 70), 14, 1.0f);
                break;
            case deep_sky_kind::nebula:
                dl->AddCircleFilled(c, r, IM_COL32(150, 96, 168, 34), 18);
                dl->AddCircleFilled(c, r * 0.6f, IM_COL32(178, 120, 190, 30), 16);
                break;
            case deep_sky_kind::cluster:
                dl->AddCircleFilled(c, r * 0.5f, IM_COL32(224, 226, 236, 40), 14);
                break;
            case deep_sky_kind::galaxy:
                // An ellipse, so it does not read as a nebula.
                dl->AddCircleFilled(c, r * 0.42f, IM_COL32(206, 198, 226, 44), 18);
                dl->AddCircle(c, r * 0.85f, IM_COL32(206, 198, 226, 26), 20, 1.0f);
                break;
        }
    }

    // Constellation lines under the stars, so the joints sit beneath the discs.
    int star_count = 0;
    const star_map_star* stars = star_map_stars(star_count);
    int con_count = 0;
    const star_map_constellation* cons = star_map_constellations(con_count);
    for (int i = 0; i < con_count; ++i)
    {
        const star_map_constellation& c = cons[i];
        for (int e = 0; e < c.edge_count; ++e)
        {
            const int a = c.stars[c.edges[e * 2]];
            const int b = c.stars[c.edges[e * 2 + 1]];
            if (a < 0 || a >= star_count || b < 0 || b >= star_count) continue;
            dl->AddLine(at(stars[a].x, stars[a].y), at(stars[b].x, stars[b].y),
                        IM_COL32(112, 132, 190, 64), 1.0f);
        }
    }

    for (int i = 0; i < star_count; ++i)
    {
        const star_map_star& s = stars[i];
        float r = 0.0f, a = 0.0f;
        magnitude_to_look(s.magnitude, r, a);
        const ImVec2 c = at(s.x, s.y);
        // A dim halo under the brightest few reads as glare rather than a bigger dot.
        if (s.magnitude < 2.0f)
            dl->AddCircleFilled(c, r * 2.1f, rgb_to_col(star_class_rgb(s.klass), a * 0.16f), 12);
        dl->AddCircleFilled(c, r, rgb_to_col(star_class_rgb(s.klass), a), 10);
    }

    // A thin frame, matching the other minimap insets.
    dl->AddRect(origin, br, IM_COL32(70, 78, 104, 200));
}

} // namespace ui
