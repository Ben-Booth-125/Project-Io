#define IMGUI_DEFINE_MATH_OPERATORS
#include "solar_system_canvas.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
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
        default:                  return "?";
    }
}

/// Visual appearance of a body, before applying the per-canvas scale factor.
struct body_style
{
    float base_radius; ///< Radius in pixels at full canvas size.
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
        default:                  return { 4.0f, IM_COL32(200, 200, 200, 255) };
    }
}

} // namespace

void draw_solar_system_canvas(const world& w, ui_state& state, ImVec2 origin, ImVec2 size, bool input_enabled)
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Background fill.
    dl->AddRectFilled(origin, origin + size, IM_COL32(8, 10, 20, 255));

    // This canvas is the minimap when the surface canvas holds the primary slot.
    const bool is_minimap = state.surface_is_primary;

    const ImVec2 centre = origin + size * 0.5f;
    const float  min_dim = std::min(size.x, size.y);

    // Element sizes scale with the region so the same code reads well at both
    // primary and minimap dimensions. 720 is the nominal full-canvas short edge.
    const float element_scale = min_dim / 720.0f;
    const bool  draw_labels    = min_dim > 320.0f; // suppress label clutter on the minimap

    // Find the outermost orbit so all bodies fit with a margin.
    float max_radius_au = 0.0f;
    for (const auto& [id, body] : w.bodies)
        max_radius_au = std::max(max_radius_au, body.orbital_radius_au);
    if (max_radius_au <= 0.0f)
        max_radius_au = 1.0f; // guard against an empty or degenerate world

    // AU-to-pixel scale for the auto-fit framing (zoom 1). The view transform
    // below applies pan and zoom on top of this.
    const float scale = (min_dim * 0.45f) / max_radius_au;

    // --- View transform: pan and zoom -------------------------------------
    // Applied only when this canvas is the primary view; the minimap always
    // shows the default auto-fit framing. Positions scale with zoom; element
    // sizes (body/star radii, labels, outlines) do not — bodies keep the same
    // pixel footprint as the player zooms. `world` coordinates are AU-pixels
    // relative to the system centre at zoom 1; `to_screen` maps them to pixels.
    const bool   apply_view  = !is_minimap;
    const float  zoom        = apply_view ? std::max(0.05f, state.solar_zoom) : 1.0f;
    const ImVec2 view_origin = apply_view
        ? ImVec2{ centre.x + state.solar_pan_x, centre.y + state.solar_pan_y }
        : centre;
    auto to_screen = [&](ImVec2 wp) -> ImVec2 {
        return { view_origin.x + wp.x * zoom, view_origin.y + wp.y * zoom };
    };

    // Resolve a body's position in world (AU-pixel) space, before the view
    // transform. A body orbits the system centre unless it has a parent, in
    // which case it orbits the parent's position — so moons track their planet
    // as the planet sweeps its own orbit. Recurses one level for the prototype
    // (moon -> planet); deeper chains resolve too. y is negated so angle 0
    // points right and angle increases CCW.
    auto body_world = [&](entity_id id, auto&& self) -> ImVec2 {
        const body_component& b = w.bodies.at(id);
        const ImVec2 anchor = (b.parent != null_entity && w.bodies.count(b.parent))
                            ? self(b.parent, self)
                            : ImVec2{0.0f, 0.0f};
        return {
            anchor.x + std::cos(b.orbital_angle_rad) * b.orbital_radius_au * scale,
            anchor.y - std::sin(b.orbital_angle_rad) * b.orbital_radius_au * scale,
        };
    };

    // Orbital rings first, so bodies and labels draw on top of them. Star-orbit
    // bodies ring the centre; moons ring their parent's current position.
    for (const auto& [id, body] : w.bodies)
    {
        const ImVec2 ring_world_centre =
            (body.parent != null_entity && w.bodies.count(body.parent))
                ? body_world(body.parent, body_world)
                : ImVec2{0.0f, 0.0f};
        dl->AddCircle(to_screen(ring_world_centre), body.orbital_radius_au * scale * zoom,
                      IM_COL32(38, 42, 52, 255), 0, 1.0f);
    }

    // Star at the system centre. 1.5x the planet-scale reference radius.
    const float star_radius = std::max(4.0f, 18.0f * element_scale);
    dl->AddCircleFilled(to_screen({0.0f, 0.0f}), star_radius, IM_COL32(255, 220, 80, 255));

    const ImVec2 mouse = ImGui::GetIO().MousePos;

    // Track which body the mouse is over, resolved during the draw so the
    // tooltip and click read the same hit.
    entity_id hovered_body = null_entity;

    // Iterate bodies in a stable id order for deterministic overlap behaviour.
    std::vector<entity_id> body_ids;
    body_ids.reserve(w.bodies.size());
    for (const auto& [id, _] : w.bodies)
        body_ids.push_back(id);
    std::sort(body_ids.begin(), body_ids.end());

    for (entity_id id : body_ids)
    {
        const body_component& body = w.bodies.at(id);
        const body_style style = style_for(body.type);
        const float radius = std::max(2.0f, style.base_radius * element_scale);

        const ImVec2 pos = to_screen(body_world(id, body_world));

        // Hit test with a small tolerance so small minimap dots stay clickable.
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

        // Selection outline, 3 px larger than the body.
        if (id == state.active_body)
            dl->AddCircle(pos, radius + 3.0f, IM_COL32(255, 255, 255, 255), 0, 1.5f);

        // Labelling: planets (and other notable bodies) carry a permanent
        // label; moons are labelled only while hovered, to keep the inner
        // system uncluttered.
        if (draw_labels)
        {
            const bool show_label = (body.type != body_type::moon) || this_hovered;
            if (show_label)
            {
                // The label tracks the live body position every frame.
                const ImVec2 text_size = ImGui::CalcTextSize(body.name.c_str());
                const ImVec2 label_pos = {
                    pos.x - text_size.x * 0.5f,
                    pos.y + radius + 2.0f,
                };
                dl->AddText(label_pos, IM_COL32(255, 255, 255, 255), body.name.c_str());
            }
        }
    }

    if (!input_enabled)
        return;

    // Hover tooltip for the resolved body.
    if (hovered_body != null_entity)
    {
        const body_component& body = w.bodies.at(hovered_body);
        ImGui::SetTooltip("%s\n%s\n%.2f AU",
            body.name.c_str(), body_type_name(body.type), body.orbital_radius_au);
    }

    // Click handling.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (hovered_body != null_entity)
        {
            // Select the body; the planetary minimap updates in place. The solar
            // canvas remains primary — the player navigates to the planetary view
            // by clicking the minimap.
            state.active_body = hovered_body;
        }
        else if (is_minimap)
        {
            // Empty click on the minimap swaps the Solar System Canvas to primary.
            state.surface_is_primary = false;
        }
    }

    // Zoom indicator — primary view only, bottom centre of the canvas.
    if (apply_view)
    {
        // Visible radius in AU at the current zoom level.
        const float visible_au = max_radius_au / zoom;
        char label[32];
        std::snprintf(label, sizeof(label), "%.1f AU", visible_au);
        const ImVec2 text_sz = ImGui::CalcTextSize(label);
        const ImVec2 text_pos = {
            origin.x + size.x * 0.5f - text_sz.x * 0.5f,
            origin.y + size.y - text_sz.y - 8.0f,
        };
        dl->AddText(text_pos, IM_COL32(160, 165, 180, 200), label);
    }

    // Pan and zoom — primary view only. Pan with the middle mouse button; zoom
    // with the scroll wheel, anchored at the cursor so the point under the
    // mouse stays put. Element sizes are unaffected; only the framing changes.
    if (apply_view)
    {
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            state.solar_pan_x += io.MouseDelta.x;
            state.solar_pan_y += io.MouseDelta.y;
        }

        if (io.MouseWheel != 0.0f)
        {
            // Min zoom caps the view at 50 AU; max zoom is the existing 20x limit.
            const float zoom_min = std::max(0.2f, max_radius_au / 50.0f);
            const float new_zoom = std::clamp(zoom * std::pow(1.1f, io.MouseWheel), zoom_min, 20.0f);
            // World point under the cursor, kept fixed across the zoom change.
            const ImVec2 wp = { (mouse.x - view_origin.x) / zoom,
                                (mouse.y - view_origin.y) / zoom };
            state.solar_pan_x = mouse.x - wp.x * new_zoom - centre.x;
            state.solar_pan_y = mouse.y - wp.y * new_zoom - centre.y;
            state.solar_zoom  = new_zoom;
        }
    }
}

} // namespace ui
