#define IMGUI_DEFINE_MATH_OPERATORS
#include "solar_system_canvas.hpp"

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

    const float scale = (min_dim * 0.45f) / max_radius_au;

    // Orbital rings first, so bodies and labels draw on top of them.
    for (const auto& [id, body] : w.bodies)
        dl->AddCircle(centre, body.orbital_radius_au * scale, IM_COL32(38, 42, 52, 255), 0, 1.0f);

    // Star at centre.
    const float star_radius = std::max(3.0f, 12.0f * element_scale);
    dl->AddCircleFilled(centre, star_radius, IM_COL32(255, 220, 80, 255));

    const ImVec2 mouse = ImGui::GetIO().MousePos;

    // Track which body the mouse is over, resolved after drawing so the tooltip
    // and click read the same hit.
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

        // y is negated so angle 0 points right and angle increases CCW.
        const ImVec2 pos = {
            centre.x + std::cos(body.orbital_angle_rad) * body.orbital_radius_au * scale,
            centre.y - std::sin(body.orbital_angle_rad) * body.orbital_radius_au * scale,
        };

        dl->AddCircleFilled(pos, radius, style.colour);

        // Selection outline, 3 px larger than the body.
        if (id == state.active_body)
            dl->AddCircle(pos, radius + 3.0f, IM_COL32(255, 255, 255, 255), 0, 1.5f);

        if (draw_labels)
        {
            const ImVec2 text_size = ImGui::CalcTextSize(body.name.c_str());
            const ImVec2 label_pos = { pos.x - text_size.x * 0.5f, pos.y + radius + 2.0f };
            dl->AddText(label_pos, IM_COL32(255, 255, 255, 255), body.name.c_str());
        }

        // Hit test with a small tolerance so small minimap dots stay clickable.
        if (input_enabled)
        {
            const float dx = mouse.x - pos.x;
            const float dy = mouse.y - pos.y;
            if (dx * dx + dy * dy <= (radius + 3.0f) * (radius + 3.0f))
                hovered_body = id;
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
            // Select the body and bring its surface forward in a single action.
            state.active_body        = hovered_body;
            state.surface_is_primary = true;
        }
        else if (is_minimap)
        {
            // Empty click on the minimap swaps the Solar System Canvas to primary.
            state.surface_is_primary = false;
        }
    }
}

} // namespace ui
