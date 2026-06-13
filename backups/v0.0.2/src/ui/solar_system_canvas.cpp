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
        case body_type::star:     return "Star";
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
        case body_type::star:     return { 18.0f, IM_COL32(255, 220,  80, 255) }; // 1.5x the planet reference
        default:                  return { 4.0f, IM_COL32(200, 200, 200, 255) };
    }
}

} // namespace

void draw_solar_system_canvas(const world& w, ui_state& state, ImVec2 origin, ImVec2 size, bool input_enabled, bool is_minimap)
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Background fill.
    dl->AddRectFilled(origin, origin + size, IM_COL32(8, 10, 20, 255));

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
    // bodies ring the centre; moons ring their parent's current position. The
    // star itself has no orbit, so it contributes no ring.
    for (const auto& [id, body] : w.bodies)
    {
        if (body.orbital_radius_au <= 0.0f)
            continue;
        const ImVec2 ring_world_centre =
            (body.parent != null_entity && w.bodies.count(body.parent))
                ? body_world(body.parent, body_world)
                : ImVec2{0.0f, 0.0f};
        dl->AddCircle(to_screen(ring_world_centre), body.orbital_radius_au * scale * zoom,
                      IM_COL32(38, 42, 52, 255), 0, 1.0f);
    }

    // The star is drawn in the body pass below (body_type::star, at the centre),
    // so it needs no dedicated draw here.

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

    // Scale bar + zoom slider — primary view only, pinned to the bottom centre.
    // Drawn before the input_enabled early-out so it stays put while an ImGui
    // panel (including this slider itself) is capturing the mouse. The slider is
    // a real ImGui widget, so it handles its own input regardless.
    if (apply_view)
    {
        // Pixels per AU at the current view. `scale` is the auto-fit AU->pixel
        // factor at zoom 1; multiplying by zoom gives the live factor.
        const float px_per_au = scale * zoom;

        // Fixed-width scale bar (8% of the canvas width); the AU distance it
        // spans is dynamic, shown to two decimals at the current zoom.
        const float bar_px = size.x * 0.08f;
        const float bar_au = bar_px / px_per_au;

        const float slider_w = std::clamp(size.x * 0.12f, 120.0f, 240.0f);

        // Centre the scale bar on the canvas; the slider sits to its right. The
        // window's left edge is placed so the bar (the leading element) is
        // screen-centred, with the slider offset rightward beyond it.
        ImGui::SetNextWindowPos({origin.x + size.x * 0.5f - bar_px * 0.5f,
                                 origin.y + size.y - 8.0f},
                                ImGuiCond_Always, {0.0f, 1.0f});
        ImGui::SetNextWindowBgAlpha(0.0f); // no fill
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
        constexpr ImGuiWindowFlags scale_flags =
            ImGuiWindowFlags_NoTitleBar          |
            ImGuiWindowFlags_NoResize            |
            ImGuiWindowFlags_NoMove              |
            ImGuiWindowFlags_NoCollapse          |
            ImGuiWindowFlags_NoScrollbar         |
            ImGuiWindowFlags_NoNav               |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoSavedSettings     |
            ImGuiWindowFlags_AlwaysAutoResize;
        ImGui::Begin("##solar_scale", nullptr, scale_flags);

        // --- Scale bar: a horizontal rule with end ticks and a centred label.
        const ImVec2 bar_origin = ImGui::GetCursorScreenPos();
        const float  bar_h      = ImGui::GetTextLineHeight() + 8.0f;
        ImGui::Dummy({bar_px, bar_h});

        ImDrawList* wdl = ImGui::GetWindowDrawList();
        const ImU32 bar_col = IM_COL32(200, 205, 220, 220);
        const float by  = bar_origin.y + bar_h - 3.0f; // bar baseline
        const float bx0 = bar_origin.x;
        const float bx1 = bar_origin.x + bar_px;
        wdl->AddLine({bx0, by}, {bx1, by}, bar_col, 1.5f);
        wdl->AddLine({bx0, by - 5.0f}, {bx0, by}, bar_col, 1.5f);
        wdl->AddLine({bx1, by - 5.0f}, {bx1, by}, bar_col, 1.5f);

        char bar_label[32];
        std::snprintf(bar_label, sizeof(bar_label), "%.2f AU", bar_au);
        const ImVec2 lsz = ImGui::CalcTextSize(bar_label);
        wdl->AddText({bar_origin.x + (bar_px - lsz.x) * 0.5f, bar_origin.y}, bar_col, bar_label);

        // --- Zoom slider: visible radius in AU, 0.5 (zoomed in) to 50 (out).
        // Logarithmic so the two-decade range feels even across the track. No
        // value text — the scale bar already reports the distance.
        ImGui::SameLine();
        ImGui::SetNextItemWidth(slider_w);
        float visible_au = std::clamp(max_radius_au / zoom, 0.5f, 50.0f);
        if (ImGui::SliderFloat("##zoom", &visible_au, 0.5f, 50.0f, "",
                               ImGuiSliderFlags_Logarithmic))
        {
            state.solar_zoom = max_radius_au / visible_au;
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
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

    // Click handling — ascend when this canvas is the minimap, descend when it
    // is primary (see MINIMAP.md / CANVASES.md, the zoom ladder).
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (is_minimap)
        {
            // Solar is the minimap (Circumplanetary is primary): any click
            // ascends back to the Solar view.
            state.primary_level = canvas_level::solar;
        }
        else if (hovered_body != null_entity &&
                 w.bodies.at(hovered_body).type != body_type::star)
        {
            // Descend: select the body and open its circumplanetary view. A moon
            // resolves to its parent planet's view; the star has no view, so it
            // is excluded above.
            state.active_body   = hovered_body;
            state.primary_level = canvas_level::circumplanetary;
        }
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
