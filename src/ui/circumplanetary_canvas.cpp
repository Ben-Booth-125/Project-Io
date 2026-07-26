#define IMGUI_DEFINE_MATH_OPERATORS
#include "circumplanetary_canvas.hpp"

#include "canvas_scale.hpp"
#include "highlight.hpp"
#include "nav_pane.hpp"
#include "presentation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace ui {

namespace {

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

/// Market lens, Circumplanetary rung (LENSES.md § Market lens): a compact good→price
/// strip for the anchor body's per-body market, the lens-selected good highlighted.
/// Lists every traded good (non-zero base price) with its resolved price; top-left,
/// pure ImDrawList. No-op when the anchor has no market or no traded goods.
void draw_market_price_strip(ImDrawList* dl, const world& w, entity_id anchor,
                             const ui_state& state, ImVec2 origin, ImVec2 size)
{
    const market_component* mk = nullptr;
    for (const auto& [mid, m] : w.markets)
        if (m.body == anchor) { mk = &m; break; }
    if (!mk)
        return;

    std::vector<std::pair<resource_type, float>> rows;
    for (std::size_t i = 0; i < resource_count; ++i)
        if (mk->base_price[i] > 0.0f)
            rows.push_back({ static_cast<resource_type>(i), mk->price[i] });
    if (rows.empty())
        return;

    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float box_w  = 152.0f;
    const float body_h = pad + line_h + 3.0f +
                         static_cast<float>(rows.size()) * (line_h + 2.0f) + pad;
    // Left edge inset past the nav rail, vertically centred — clear of the Selection
    // panel (top-left), the header (top-centre), and the comms log/minimap (right).
    const ImVec2 p0 = { origin.x + nav_pane_width + pad,
                        origin.y + std::max(pad, (size.y - body_h) * 0.5f) };
    const ImVec2 p1 = { p0.x + box_w, p0.y + body_h };
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 210), 4.0f);
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255), 4.0f);

    const float x = p0.x + pad;
    float       y = p0.y + pad * 0.5f;
    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Market prices");
    y += line_h + 3.0f;

    for (const auto& [r, price] : rows)
    {
        const bool sel = (r == state.lens_resource);
        if (sel)
            dl->AddRectFilled({p0.x + 2.0f, y}, {p1.x - 2.0f, y + line_h}, IM_COL32(60, 70, 90, 255));
        dl->AddRectFilled({x, y + 3.0f}, {x + 8.0f, y + 11.0f}, presentation_of(r).colour);
        dl->AddText({x + 12.0f, y}, sel ? IM_COL32(255, 255, 255, 255) : IM_COL32(210, 210, 215, 255),
                    presentation_of(r).abbrev);
        char pbuf[32];
        std::snprintf(pbuf, sizeof(pbuf), "%.1f", static_cast<double>(price));
        const ImVec2 pts = ImGui::CalcTextSize(pbuf);
        dl->AddText({p1.x - pad - pts.x, y},
                    sel ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 185, 195, 255), pbuf);
        y += line_h + 2.0f;
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

    // Zoom bounds shared by the scale/zoom slider and the scroll-wheel handler.
    // The deepest zoom is capped so the most zoomed-in framing spans ~0.3 AU
    // across: at zoom z the visible half-extent is max_moon_au / z AU, so the full
    // visible width is 2 * max_moon_au / z; setting that to 0.3 AU gives
    // z = max_moon_au / 0.15. Derived from the per-anchor scale rather than a flat
    // constant so the cap tracks each anchor's local extent. A moonless anchor
    // uses the 0.5 AU floor above, giving a finite (~3.3) cap.
    constexpr float zoom_min = 0.2f;
    const float     zoom_max = max_moon_au / 0.15f;

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

    const ImVec2 mouse = state.mouse.active
                         ? ImVec2{state.mouse.x, state.mouse.y}
                         : ImVec2{-1.0f, -1.0f};

    // Draw the anchor first, then the moons on top. Each entry is (id, is_anchor).
    std::vector<std::pair<entity_id, bool>> draw_list;
    draw_list.reserve(moons.size() + 1);
    draw_list.push_back({anchor, true});
    for (entity_id id : moons)
        draw_list.push_back({id, false});

    // Resolve the single hovered body up front so a moon overlapping the anchor
    // settles on one stable choice rather than both drawing a ring (see the
    // highlight tie convention in highlight.hpp). Nearest centre to the cursor
    // wins; the strict `<` keeps an exact tie on the first entry (the anchor).
    entity_id hovered_body = null_entity;
    if (input_enabled)
    {
        float best_d2 = std::numeric_limits<float>::max();
        for (const auto& [id, is_anchor] : draw_list)
        {
            const float  radius = body_radius(id, is_anchor);
            const ImVec2 pos    = to_screen(local_pos(id, is_anchor));
            const float  dx = mouse.x - pos.x;
            const float  dy = mouse.y - pos.y;
            const float  d2 = dx * dx + dy * dy;
            const float  hit = radius + 3.0f;
            if (d2 <= hit * hit && d2 < best_d2)
            {
                best_d2 = d2;
                hovered_body = id;
            }
        }
    }

    for (const auto& [id, is_anchor] : draw_list)
    {
        const body_component& body = w.bodies.at(id);
        const body_style style  = style_for(body.type);
        const float      radius = body_radius(id, is_anchor);
        const ImVec2     pos    = to_screen(local_pos(id, is_anchor));

        const bool this_hovered = (id == hovered_body);

        dl->AddCircleFilled(pos, radius, style.colour);

        // Shared selection / hover / pinned ring (pinning not yet wired).
        draw_body_highlight(dl, pos, radius,
            resolve_highlight(id == state.selected_entity, this_hovered, /*pinned=*/false));

        if (draw_labels)
        {
            const ImVec2 ts = ImGui::CalcTextSize(body.name.c_str());
            dl->AddText({pos.x - ts.x * 0.5f, pos.y + radius + 2.0f},
                        IM_COL32(255, 255, 255, 255), body.name.c_str());
        }
    }

    // Market lens: the per-body price strip for the anchor's market (top-left).
    // Primary view only — the minimap is too small for a legible strip.
    if (apply_view && state.overlay == overlay_mode::market)
        draw_market_price_strip(dl, w, anchor, state, origin, size);

    // Supply lens: draw a convoy count badge next to each body's label.
    // w.convoys is empty until the dispatch system lands; the loop is a no-op
    // in the meantime. Primary view only — the minimap is too small.
    if (apply_view && state.overlay == overlay_mode::supply)
    {
        constexpr ImU32 badge_col  = IM_COL32(80, 200, 255, 200);
        constexpr ImU32 badge_bg   = IM_COL32(18, 18, 24, 200);
        for (const auto& [id, is_anchor_body] : draw_list)
        {
            // Count convoys whose source or destination body matches this body.
            int count = 0;
            for (const auto& cv : w.convoys)
            {
                entity_id src_body = null_entity;
                entity_id dst_body = null_entity;
                const auto smk = w.markets.find(cv.source_market);
                const auto dmk = w.markets.find(cv.dest_market);
                if (smk != w.markets.end()) src_body = smk->second.body;
                if (dmk != w.markets.end()) dst_body = dmk->second.body;
                if (src_body == id || dst_body == id)
                    ++count;
            }
            if (count == 0)
                continue;

            const float  radius = body_radius(id, is_anchor_body);
            const ImVec2 pos    = to_screen(local_pos(id, is_anchor_body));
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%d", count);
            const ImVec2 ts  = ImGui::CalcTextSize(buf);
            const ImVec2 bp  = { pos.x + radius + 4.0f, pos.y - ts.y * 0.5f };
            dl->AddRectFilled({ bp.x - 2.0f, bp.y - 1.0f },
                              { bp.x + ts.x + 2.0f, bp.y + ts.y + 1.0f },
                              badge_bg, 2.0f);
            dl->AddText(bp, badge_col, buf);
        }
    }

    // Scale bar + zoom slider — primary view only, pinned to the bottom centre.
    // Shared with the Solar canvas via draw_scale_zoom_overlay. Drawn before the
    // input_enabled early-out so it stays put while an ImGui panel captures the
    // mouse. `scale * zoom` is the live pixels-per-AU.
    if (apply_view)
        draw_scale_zoom_overlay("circum", origin, size, scale * zoom,
                                state.circum_zoom, zoom_min, zoom_max);

    if (!input_enabled)
        return;

    if (hovered_body != null_entity)
    {
        const body_component& body = w.bodies.at(hovered_body);
        ImGui::SetTooltip("%s\n%s\n%.2f AU",
            body.name.c_str(), body_type_name(body.type), body.orbital_radius_au);
    }

    // Click handling. Single-click selects (no view change); double-click
    // navigates (descend to the body's surface). On the minimap a single click
    // ascends. See SELECTION.md / CANVASES.md.
    if (is_minimap)
    {
        // Circumplanetary is the minimap (Planetary is primary): ascend.
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            state.primary_level = canvas_level::circumplanetary;
    }
    else
    {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            state.selected_entity = hovered_body;
            // A fresh click re-shows a dismissed panel, even on re-selection
            // of the same body (close hides, does not destroy — SELECTION.md).
            state.selection_hidden_for = null_entity;
            // Freeze the sticky card at the click position (BL-194).
            if (state.selected_entity != null_entity)
                state.card_anchor = { mouse.x, mouse.y };
        }

        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
            hovered_body != null_entity)
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
            const float new_zoom = std::clamp(zoom * std::pow(1.1f, io.MouseWheel), zoom_min, zoom_max);
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
