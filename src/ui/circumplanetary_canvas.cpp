#define IMGUI_DEFINE_MATH_OPERATORS
#include "circumplanetary_canvas.hpp"

#include "highlight.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ui {

namespace {

const char* body_type_name(body_type t)
{
    switch (t)
    {
        case body_type::planet:   return "Planet";
        case body_type::moon:     return "Moon";
        case body_type::asteroid: return "Asteroid";
        case body_type::station:  return "Station";
        case body_type::star:     return "Star";
        default:                  return "?";
    }
}

/// Visual appearance of a body, before the per-canvas scale factor. Mirrors the
/// solar canvas styles so a body reads the same across rungs.
struct body_style
{
    float base_radius;
    ImU32 colour;
};

body_style style_for(body_type t)
{
    switch (t)
    {
        case body_type::planet:   return { 7.0f, IM_COL32( 80, 120, 180, 255) };
        case body_type::moon:     return { 5.0f, IM_COL32(148, 145, 140, 255) };
        case body_type::asteroid: return { 4.0f, IM_COL32(140, 110,  80, 255) };
        case body_type::station:  return { 4.0f, IM_COL32( 80, 180, 160, 255) };
        case body_type::star:     return { 18.0f, IM_COL32(255, 220,  80, 255) };
        default:                  return { 4.0f, IM_COL32(200, 200, 200, 255) };
    }
}

} // namespace

entity_id circumplanetary_anchor(const world& w, entity_id active_body)
{
    auto it = w.bodies.find(active_body);
    if (it == w.bodies.end())
        return null_entity;

    // A moon's local view centres on its parent planet; anything orbiting the
    // star directly is its own anchor.
    const body_component& b = it->second;
    if (b.parent != null_entity && w.bodies.count(b.parent))
        return b.parent;
    return active_body;
}

void draw_circumplanetary_canvas(const world& w, ui_state& state, ImVec2 origin, ImVec2 size, bool input_enabled, bool is_minimap)
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Background fill — a touch warmer than the solar canvas.
    dl->AddRectFilled(origin, origin + size, IM_COL32(10, 12, 22, 255));

    const ImVec2 centre  = origin + size * 0.5f;
    const float  min_dim = std::min(size.x, size.y);

    const float element_scale = min_dim / 720.0f;
    const bool  draw_labels    = min_dim > 320.0f;

    const entity_id anchor = circumplanetary_anchor(w, state.active_body);
    if (anchor == null_entity)
    {
        if (draw_labels)
        {
            const char* msg = "No body selected";
            const ImVec2 ts = ImGui::CalcTextSize(msg);
            dl->AddText(origin + (size - ts) * 0.5f, IM_COL32(150, 150, 150, 255), msg);
        }
        if (input_enabled && is_minimap && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            state.primary_level = canvas_level::circumplanetary;
        return;
    }

    // Moons of the anchor, in a stable id order for deterministic overlap.
    std::vector<entity_id> moons;
    for (const auto& [id, body] : w.bodies)
        if (body.parent == anchor)
            moons.push_back(id);
    std::sort(moons.begin(), moons.end());

    // Fit the outermost moon orbit with a margin. A moonless anchor still has a
    // valid (empty) local view; the floor keeps the scale finite.
    float max_moon_au = 0.0f;
    for (entity_id id : moons)
        max_moon_au = std::max(max_moon_au, w.bodies.at(id).orbital_radius_au);
    if (max_moon_au <= 0.0f)
        max_moon_au = 0.5f;
    const float scale = (min_dim * 0.40f) / max_moon_au;

    // View transform — pan/zoom only when primary; the minimap shows the default
    // framing. Element sizes do not scale with zoom.
    const bool   apply_view  = !is_minimap;
    const float  zoom        = apply_view ? std::max(0.05f, state.circum_zoom) : 1.0f;
    const ImVec2 view_origin = apply_view
        ? ImVec2{ centre.x + state.circum_pan_x, centre.y + state.circum_pan_y }
        : centre;
    auto to_screen = [&](ImVec2 wp) -> ImVec2 {
        return { view_origin.x + wp.x * zoom, view_origin.y + wp.y * zoom };
    };

    // World-space position of a body in the local view. The anchor sits at the
    // centre; moons orbit it. y is negated so angle 0 points right (CCW), as on
    // the solar canvas.
    auto local_pos = [&](entity_id id, bool is_anchor) -> ImVec2 {
        if (is_anchor)
            return {0.0f, 0.0f};
        const body_component& b = w.bodies.at(id);
        return {
             std::cos(b.orbital_angle_rad) * b.orbital_radius_au * scale,
            -std::sin(b.orbital_angle_rad) * b.orbital_radius_au * scale,
        };
    };

    auto body_radius = [&](entity_id id, bool is_anchor) -> float {
        const body_style s = style_for(w.bodies.at(id).type);
        // The anchor is enlarged so the local view reads as "zoomed in".
        const float mult  = is_anchor ? 2.0f : 1.0f;
        const float floor = is_anchor ? 6.0f : 2.0f;
        return std::max(floor, s.base_radius * element_scale * mult);
    };

    // Moon orbital rings, centred on the anchor.
    for (entity_id id : moons)
    {
        const float r = w.bodies.at(id).orbital_radius_au * scale * zoom;
        dl->AddCircle(to_screen({0.0f, 0.0f}), r, IM_COL32(38, 42, 52, 255), 0, 1.0f);
    }

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    entity_id hovered_body = null_entity;

    // Draw the anchor first, then the moons on top. Each entry is (id, is_anchor).
    std::vector<std::pair<entity_id, bool>> draw_list;
    draw_list.reserve(moons.size() + 1);
    draw_list.push_back({anchor, true});
    for (entity_id id : moons)
        draw_list.push_back({id, false});

    for (const auto& [id, is_anchor] : draw_list)
    {
        const body_component& body = w.bodies.at(id);
        const body_style style  = style_for(body.type);
        const float      radius = body_radius(id, is_anchor);
        const ImVec2     pos    = to_screen(local_pos(id, is_anchor));

        bool this_hovered = false;
        if (input_enabled)
        {
            const float dx = mouse.x - pos.x;
            const float dy = mouse.y - pos.y;
            if (dx * dx + dy * dy <= (radius + 3.0f) * (radius + 3.0f))
            {
                this_hovered = true;
                hovered_body = id;
            }
        }

        dl->AddCircleFilled(pos, radius, style.colour);

        // Shared selection / hover / pinned ring (pinning not yet wired).
        draw_body_highlight(dl, pos, radius,
            resolve_highlight(id == state.active_body, this_hovered, /*pinned=*/false));

        if (draw_labels)
        {
            const ImVec2 ts = ImGui::CalcTextSize(body.name.c_str());
            dl->AddText({pos.x - ts.x * 0.5f, pos.y + radius + 2.0f},
                        IM_COL32(255, 255, 255, 255), body.name.c_str());
        }
    }

    if (!input_enabled)
        return;

    if (hovered_body != null_entity)
    {
        const body_component& body = w.bodies.at(hovered_body);
        ImGui::SetTooltip("%s\n%s\n%.2f AU",
            body.name.c_str(), body_type_name(body.type), body.orbital_radius_au);
    }

    // Click handling — ascend when this canvas is the minimap, descend (to a
    // body's surface) when it is primary.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (is_minimap)
        {
            // Circumplanetary is the minimap (Planetary is primary): ascend.
            state.primary_level = canvas_level::circumplanetary;
        }
        else if (hovered_body != null_entity)
        {
            // Descend: open the clicked body's surface.
            state.active_body   = hovered_body;
            state.primary_level = canvas_level::planetary;
        }
    }

    // Pan and zoom — primary view only.
    if (apply_view)
    {
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            state.circum_pan_x += io.MouseDelta.x;
            state.circum_pan_y += io.MouseDelta.y;
        }

        if (io.MouseWheel != 0.0f)
        {
            const float new_zoom = std::clamp(zoom * std::pow(1.1f, io.MouseWheel), 0.2f, 20.0f);
            // World point under the cursor, kept fixed across the zoom change.
            const ImVec2 wp = { (mouse.x - view_origin.x) / zoom,
                                (mouse.y - view_origin.y) / zoom };
            state.circum_pan_x = mouse.x - wp.x * new_zoom - centre.x;
            state.circum_pan_y = mouse.y - wp.y * new_zoom - centre.y;
            state.circum_zoom  = new_zoom;
        }
    }
}

} // namespace ui
