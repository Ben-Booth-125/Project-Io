#define IMGUI_DEFINE_MATH_OPERATORS
#include "solar_system_canvas.hpp"

#include "canvas_scale.hpp"
#include "highlight.hpp"
#include "icons.hpp"
#include "presentation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>
#include <vector>

namespace ui {

namespace {

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

    // Find the outermost orbit so all bodies fit with a margin. The asteroid
    // belt's outer edge is included so the whole band fits the auto-fit framing.
    float max_radius_au = 0.0f;
    for (const auto& [id, body] : w.bodies)
        max_radius_au = std::max(max_radius_au, body.orbital_radius_au);
    if (w.belt.present())
        max_radius_au = std::max(max_radius_au, w.belt.outer_radius_au);
    if (max_radius_au <= 0.0f)
        max_radius_au = 1.0f; // guard against an empty or degenerate world

    // AU-to-pixel scale for the auto-fit framing (zoom 1). The view transform
    // below applies pan and zoom on top of this.
    const float scale = (min_dim * 0.45f) / max_radius_au;

    // Zoom bounds shared by the scale/zoom slider and the scroll-wheel handler so
    // both agree on the range. The minimum frames at most 50 AU; the maximum is
    // the 20x cap. Defined once here in terms of the auto-fit reference radius.
    const float zoom_min = std::max(0.2f, max_radius_au / 50.0f);
    const float zoom_max = 20.0f;

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

    // Asteroid belt — a thick, translucent textured band between two orbital
    // radii (not a body). Drawn over the orbital rings and under the bodies, so
    // the notable asteroids within it render on top of the band. The annulus is
    // approximated by a thick circle stroke; a deterministic scatter of dusty
    // specks gives it texture and holds still frame-to-frame (a fixed seed, with
    // positions in AU space so they pan and zoom with the view).
    if (w.belt.present())
    {
        const ImVec2 bc       = to_screen({0.0f, 0.0f}); // system centre
        const float  inner_px = w.belt.inner_radius_au * scale * zoom;
        const float  outer_px = w.belt.outer_radius_au * scale * zoom;
        const float  mid_px   = (inner_px + outer_px) * 0.5f;
        const float  band_px  = outer_px - inner_px;

        // Translucent band fill via a thick ring stroke.
        dl->AddCircle(bc, mid_px, IM_COL32(150, 130, 95, 38), 128, band_px);

        std::mt19937 rng(0xA57E0u);
        std::uniform_real_distribution<float> ang(0.0f, 6.2831853f);
        std::uniform_real_distribution<float> rad(w.belt.inner_radius_au, w.belt.outer_radius_au);
        std::uniform_real_distribution<float> bright(0.0f, 1.0f);
        constexpr int speck_count = 420;
        for (int i = 0; i < speck_count; ++i)
        {
            const float  a  = ang(rng);
            const float  rr = rad(rng) * scale * zoom;
            const ImVec2 p  = { bc.x + std::cos(a) * rr, bc.y - std::sin(a) * rr };
            const int    alpha = 70 + static_cast<int>(bright(rng) * 110.0f);
            dl->AddCircleFilled(p, 1.0f, IM_COL32(180, 160, 120, alpha), 4);
        }
    }

    // The star is drawn in the body pass below (body_type::star, at the centre),
    // so it needs no dedicated draw here.

    const ImVec2 mouse = state.mouse.active
                         ? ImVec2{state.mouse.x, state.mouse.y}
                         : ImVec2{-1.0f, -1.0f};

    // Iterate bodies in a stable id order for deterministic overlap behaviour.
    std::vector<entity_id> body_ids;
    body_ids.reserve(w.bodies.size());
    for (const auto& [id, _] : w.bodies)
        body_ids.push_back(id);
    std::sort(body_ids.begin(), body_ids.end());

    // Resolve the single hovered body up front so overlapping markers settle on
    // one stable choice rather than each drawing its own ring (see the highlight
    // tie convention in highlight.hpp). The body whose centre is nearest the
    // cursor wins; the strict `<` over id-sorted bodies keeps an exact-distance
    // tie on the lowest id, which is arbitrary but stable frame-to-frame.
    entity_id hovered_body = null_entity;
    if (input_enabled)
    {
        float best_d2 = std::numeric_limits<float>::max();
        for (entity_id id : body_ids)
        {
            const body_component& body = w.bodies.at(id);
            const float radius = std::max(2.0f, style_for(body.type).base_radius * element_scale);
            const ImVec2 pos = to_screen(body_world(id, body_world));
            const float dx = mouse.x - pos.x;
            const float dy = mouse.y - pos.y;
            const float d2 = dx * dx + dy * dy;
            const float hit = radius + 3.0f; // small tolerance keeps minimap dots clickable
            if (d2 <= hit * hit && d2 < best_d2)
            {
                best_d2 = d2;
                hovered_body = id;
            }
        }
    }

    for (entity_id id : body_ids)
    {
        const body_component& body = w.bodies.at(id);
        const body_style style = style_for(body.type);
        const float radius = std::max(2.0f, style.base_radius * element_scale);

        const ImVec2 pos = to_screen(body_world(id, body_world));

        const bool this_hovered = (id == hovered_body);

        // Activity fog as a dim shadow (BL-150): classify the body's commercial-
        // activity tier once (home is always full-bright), then dim its body + label
        // brightness and cast a translucent dark wash over the dimmer tiers, so a
        // body outside the player's commercial network reads as *fogged* and
        // brightens as commerce reaches it. Derived read only — no sim state. The
        // same `av` feeds the fine-grained lower-left pulse badge below.
        const bool is_home = (id == w.home_body && w.home_body != null_entity);
        const activity_vis av = is_home
                                    ? activity_vis::visible
                                    : body_activity_visibility(w, id, w.current_day_tick);
        const int   av_i       = static_cast<int>(av);
        const float fog_bright = is_home ? 1.0f : palette::activity_fog_brightness[av_i];
        const int   fog_shadow = is_home ? 0    : palette::activity_fog_shadow_alpha[av_i];

        // Home-body halo (BL-085): an always-on "you are here" ring around the
        // player's home world, drawn behind the body so it reads as identity chrome
        // distinct from the survey badge and the selection/hover highlight. Player
        // identity colour (corp slot 0), with a soft glow so it carries at a glance.
        if (id == w.home_body && w.home_body != null_entity)
        {
            const float hr = radius + std::max(3.0f, 4.0f * element_scale);
            dl->AddCircleFilled(pos, hr, IM_COL32(80, 150, 230, 45));
            dl->AddCircle(pos, hr, palette::corp_colour(0), 0, 2.0f);
        }

        dl->AddCircleFilled(pos, radius, palette::dim_rgb(style.colour, fog_bright));

        // Activity-fog shadow wash (BL-150): a translucent dark disc cast over the
        // dimmest tiers (unknown, stale), slightly larger than the body so it reads
        // as a shadow the body sits within. The lit tiers carry brightness alone.
        if (fog_shadow > 0)
            dl->AddCircleFilled(pos, radius + 1.0f, IM_COL32(8, 10, 16, fog_shadow));

        // Shared selection / hover / pinned ring. Pinning is not yet wired, so
        // pinned is always false here.
        draw_body_highlight(dl, pos, radius,
            resolve_highlight(id == state.selected_entity, this_hovered, /*pinned=*/false));

        // Survey badge (BL-067): mark each body's survey status at its upper-right.
        // hidden → dimmed "?"; in_transit / scanning → magnifying glass + k∕N region
        // count; surveyed (home, star, completed) → no badge. The badge tracks the
        // live body position every frame.
        if (body.survey.phase != survey_phase::surveyed)
        {
            const float  br = std::max(4.0f, 6.0f * element_scale);
            const ImVec2 badge_pos{ pos.x + radius * 0.9f + br, pos.y - radius * 0.9f - br };
            if (body.survey.phase == survey_phase::hidden)
            {
                icons::unknown(dl, badge_pos, br, IM_COL32(190, 190, 200, 150));
            }
            else // in_transit / scanning
            {
                icons::survey_badge(dl, badge_pos, br, IM_COL32(120, 215, 255, 230));
                char prog[24];
                if (body.survey.phase == survey_phase::scanning && body.survey.regions_total > 0)
                    std::snprintf(prog, sizeof prog, "%d/%d", body.survey.regions_done, body.survey.regions_total);
                else
                    std::snprintf(prog, sizeof prog, "...");
                dl->AddText({ badge_pos.x + br + 2.0f, badge_pos.y - 6.0f }, // fit-exempt: on-canvas marker label, no containing box
                            IM_COL32(120, 215, 255, 230), prog);
            }
        }

        // Commercial-activity badge (BL-089): the player's trade network lit up.
        // Drawn at the body's lower-LEFT so it never collides with the survey badge
        // (upper-right) — the two fogs (geographic vs activity) read apart. The home
        // body carries its own halo, and unknown bodies stay a plain dot, so a badge
        // shows only for known / stale / visible non-home bodies.
        if (id != w.home_body)
        {
            if (av != activity_vis::unknown)
            {
                const float  ar = std::max(4.0f, 6.0f * element_scale);
                const ImVec2 ap{ pos.x - radius * 0.9f - ar, pos.y + radius * 0.9f + ar };
                const ImU32  ac = (av == activity_vis::visible)     ? palette::activity_visible
                                : (av == activity_vis::known)       ? palette::activity_known
                                                                    : palette::activity_stale;
                icons::activity(dl, ap, ar, ac);
            }
        }

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
                // Dim the label to match the body's activity-fog brightness (BL-150),
                // so an un-networked body's name recedes with it rather than reading
                // full-bright over a shadowed dot.
                dl->AddText(label_pos, palette::dim_rgb(IM_COL32(255, 255, 255, 255), fog_bright), // fit-exempt: on-canvas marker label, no containing box
                            body.name.c_str());
            }
        }
    }

    // Commercial-sphere corridors (BL-089): the player's persistent trade routes
    // drawn as lit lanes so commercial reach is felt on the map — fresh routes glow,
    // stale routes fade to grey. Always-on (independent of lens), primary view only.
    if (apply_view)
    {
        for (const auto& r : w.trade_routes)
        {
            if (r.corp != w.player_entity)
                continue;
            if (w.bodies.find(r.body_a) == w.bodies.end() ||
                w.bodies.find(r.body_b) == w.bodies.end())
                continue;
            const ImVec2 pa = to_screen(body_world(r.body_a, body_world));
            const ImVec2 pb = to_screen(body_world(r.body_b, body_world));
            const bool fresh = (w.current_day_tick - r.last_tick) <= route_fresh_ticks_default;
            const ImU32 col  = fresh ? palette::activity_corridor : IM_COL32(140, 142, 150, 80);
            dl->AddLine(pa, pb, col, fresh ? 2.0f : 1.5f);
        }
    }

    // Supply lens: draw a line between the two bodies for each inter-body convoy.
    // w.convoys is populated by dispatch_convoys (supply_system.cpp) each tick.
    // Primary view only — the minimap is too small.
    if (apply_view && state.overlay == overlay_mode::supply)
    {
        constexpr ImU32 supply_col = IM_COL32(80, 200, 255, 160);
        for (const auto& cv : w.convoys)
        {
            // Resolve the source and destination bodies via their markets.
            entity_id src_body = null_entity;
            entity_id dst_body = null_entity;
            const auto src_mk = w.markets.find(cv.source_market);
            const auto dst_mk = w.markets.find(cv.dest_market);
            if (src_mk != w.markets.end()) src_body = src_mk->second.body;
            if (dst_mk != w.markets.end()) dst_body = dst_mk->second.body;
            if (src_body == null_entity || dst_body == null_entity || src_body == dst_body)
                continue;
            const auto sb = w.bodies.find(src_body);
            const auto db = w.bodies.find(dst_body);
            if (sb == w.bodies.end() || db == w.bodies.end())
                continue;
            const ImVec2 pos_a = to_screen(body_world(src_body, body_world));
            const ImVec2 pos_b = to_screen(body_world(dst_body, body_world));
            dl->AddLine(pos_a, pos_b, supply_col, 2.0f);
        }
    }

    // Scale bar + zoom slider — primary view only, pinned to the bottom centre.
    // Shared with the Circumplanetary canvas via draw_scale_zoom_overlay. Drawn
    // before the input_enabled early-out so it stays put while an ImGui panel
    // (including this slider itself) captures the mouse; the slider is a real
    // ImGui widget and handles its own input regardless. `scale * zoom` is the
    // live pixels-per-AU; the slider writes back to state.solar_zoom.
    if (apply_view)
        draw_scale_zoom_overlay("solar", origin, size, scale * zoom,
                                state.solar_zoom, zoom_min, zoom_max);

    if (!input_enabled)
        return;

    // Hover tooltip for the resolved body.
    if (hovered_body != null_entity)
    {
        const body_component& body = w.bodies.at(hovered_body);

        // Activity line (BL-089): a short commercial-sphere read mirroring the
        // Selection panel's draw_activity_section. Stars carry no commerce, so
        // they get no activity line (the panel early-returns for stars).
        const char* activity_line = nullptr;
        if (body.type != body_type::star)
        {
            switch (body_activity_visibility(w, hovered_body, w.current_day_tick))
            {
                case activity_vis::unknown:     activity_line = "Outside your trade network"; break;
                case activity_vis::known_stale: activity_line = "Commerce: gone cold (stale)"; break;
                case activity_vis::known:       activity_line = "Commerce: market pulse"; break;
                case activity_vis::visible:     activity_line = "Commerce: live lane / your presence"; break;
            }
        }

        char tip[256];
        std::snprintf(tip, sizeof(tip), "%s\n%s\n%.2f AU%s%s",
            body.name.c_str(), body_type_name(body.type), body.orbital_radius_au,
            activity_line ? "\n" : "", activity_line ? activity_line : "");
        ImGui::SetTooltip("%s", tip);
    }

    // Click handling. Single-click selects (fills the Selection info element, no
    // view change); double-click navigates (descend). On the minimap a single
    // click ascends. See SELECTION.md and CANVASES.md (the zoom ladder).
    if (is_minimap)
    {
        // Solar is the minimap (Circumplanetary is primary): any click ascends
        // back to the Solar view. The minimap has no selection semantics.
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            state.primary_level = canvas_level::solar;
    }
    else
    {
        // A single left-click selects whatever is hovered (null clears the
        // selection on empty space) without moving the view.
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            state.selected_entity = hovered_body;
        }

        // A double-click on a body navigates: descend into its circumplanetary
        // view. A moon resolves to its parent planet's view; the star has no
        // view of its own, so it is excluded.
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
            hovered_body != null_entity &&
            w.bodies.at(hovered_body).type != body_type::star)
        {
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
            // Same bounds as the zoom slider (min frames 50 AU; max is the 20x cap).
            const float new_zoom = std::clamp(zoom * std::pow(1.1f, io.MouseWheel), zoom_min, zoom_max);
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
