#define IMGUI_DEFINE_MATH_OPERATORS
#include "body_surface_canvas.hpp"

#include "highlight.hpp"
#include "icons.hpp"
#include "presentation.hpp"
#include "world/placement_rules.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <limits>
#include <unordered_map>

namespace ui {

namespace {

constexpr float kSqrt3 = 1.7320508f;
constexpr float kPi    = 3.14159265f;

// Zoom is normalised to grid height: at zoom = 1 the grid fills kFitMargin of the
// canvas height. The zoom floor must therefore be derived from these constants,
// not guessed — at the minimum the viewport spans kMinZoomHeadroom of the grid
// height (the full grid plus ~20% headroom), i.e. zoom = 1 / (headroom * margin).
constexpr float kFitMargin       = 0.95f; // grid fills 95% of the canvas height at zoom = 1
constexpr float kMinZoomHeadroom = 1.2f;  // at min zoom the viewport shows ~120% of the grid height
constexpr float kMaxZoom         = 20.0f;
constexpr float kMinZoom         = 1.0f / (kMinZoomHeadroom * kFitMargin); // ~0.877

// Identity fill colour for a tile's composition. The single source of truth for
// surface tinting; landform is conveyed by overlay glyphs (deferred), not hue.
ImU32 terrain_colour(terrain_composition t)
{
    switch (t)
    {
        case terrain_composition::barren:    return IM_COL32(170, 145, 100, 255);
        case terrain_composition::rocky:     return IM_COL32(112, 105,  95, 255);
        case terrain_composition::volcanic:  return IM_COL32(135,  55,  28, 255);
        case terrain_composition::icy:       return IM_COL32(200, 224, 236, 255);
        case terrain_composition::tundra:    return IM_COL32(140, 140, 118, 255);
        case terrain_composition::grassland: return IM_COL32( 96, 150,  72, 255);
        case terrain_composition::forest:    return IM_COL32( 48, 102,  56, 255);
        case terrain_composition::wetland:   return IM_COL32( 78, 120,  92, 255);
        case terrain_composition::ocean:     return IM_COL32( 40,  80, 160, 255);
        case terrain_composition::regolith:  return IM_COL32(138, 130, 120, 255);
        case terrain_composition::metallic:  return IM_COL32(158, 150, 140, 255);
    }
    return IM_COL32( 60,  60,  60, 255);
}

/// Fills `out[6]` with the screen-space vertices of a pointy-top hexagon
/// centred at (cx, cy) with circumradius r.
void hex_vertices(ImVec2 out[6], float cx, float cy, float r)
{
    for (int i = 0; i < 6; ++i)
    {
        const float angle = kPi / 6.0f + kPi / 3.0f * static_cast<float>(i);
        out[i] = { cx + r * std::cos(angle), cy + r * std::sin(angle) };
    }
}

/// World-space centre of a hex at (col, row) in odd-r offset coordinates,
/// relative to the grid top-left, using the given circumradius.
ImVec2 hex_local_centre(int col, int row, float hex_size)
{
    const float col_step = kSqrt3 * hex_size;
    const float row_step = 1.5f * hex_size;
    return {
        col_step * static_cast<float>(col) + ((row & 1) ? col_step * 0.5f : 0.0f),
        row_step * static_cast<float>(row),
    };
}

} // namespace

void draw_body_surface_canvas(const world& w, ui_state& state, ImVec2 origin, ImVec2 size, bool input_enabled)
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    dl->AddRectFilled(origin, origin + size, IM_COL32(18, 18, 24, 255));

    // The surface canvas is the bottom rung of the ladder, so it is only ever
    // drawn as the primary view — never the minimap.
    const float min_dim    = std::min(size.x, size.y);
    const bool  draw_title = min_dim > 320.0f;

    auto body_it = w.bodies.find(state.active_body);
    if (body_it == w.bodies.end())
    {
        if (draw_title)
        {
            const char* msg = "No body selected";
            const ImVec2 ts = ImGui::CalcTextSize(msg);
            dl->AddText(origin + (size - ts) * 0.5f, IM_COL32(150, 150, 150, 255), msg);
        }
        return;
    }

    const body_component& body = body_it->second;

    float title_h = 0.0f;
    if (draw_title)
    {
        title_h = ImGui::GetTextLineHeightWithSpacing();
        char title[128];
        std::snprintf(title, sizeof(title), "%s  -  %s  (%dx%d)",
            body.name.c_str(), body_type_name(body.type), body.grid_width, body.grid_height);
        dl->AddText(origin + ImVec2{4.0f, 2.0f}, IM_COL32(235, 235, 235, 255), title);
    }

    const int gw = std::max(1, body.grid_width);
    const int gh = std::max(1, body.grid_height);

    // Grid occupies the region below the title bar.
    const ImVec2 grid_area_origin = origin + ImVec2{0.0f, title_h};
    const ImVec2 grid_area_size   = { size.x, size.y - title_h };
    const ImVec2 canvas_centre    = grid_area_origin + grid_area_size * 0.5f;

    // --- Hex size at zoom=1 ---
    // zoom=1 is defined as "the full grid height fills the canvas height." The
    // grid width will typically exceed the canvas at this scale — the player pans
    // horizontally. This definition makes zoom=4/3 mean exactly "3/4 of the grid
    // height is visible" regardless of canvas size or grid aspect ratio, so the
    // same zoom value reads correctly on both the primary view and the minimap.
    const float fit_by_y = grid_area_size.y / (1.5f * static_cast<float>(gh) + 0.5f);
    const float hex_size = fit_by_y * kFitMargin;

    // Grid centre in local (unzoomed) world space — used to centre the grid
    // on the canvas at zoom=1 with no pan.
    const float col_step = kSqrt3 * hex_size;
    const float row_step = 1.5f * hex_size;
    const float grid_cx  = (static_cast<float>(gw) - 0.5f) * col_step * 0.5f;
    const float grid_cy  = static_cast<float>(gh - 1) * row_step * 0.5f;

    // --- View transform (pan/zoom) ---
    // The surface canvas is always primary, so pan and zoom always apply.
    const float  zoom        = std::clamp(state.planetary_zoom, kMinZoom, kMaxZoom);

    // Pending centre request (verify.center_tile): now that the exact grid
    // transform is in hand, set the pan that places the requested tile's local
    // centre on the canvas centre — pan = (grid_centre − tile_local) · zoom. This
    // is the one place the centring math lives; Lua callers never replicate it.
    if (state.planetary_center_pending)
    {
        const ImVec2 lc = hex_local_centre(state.planetary_center_col,
                                           state.planetary_center_row, hex_size);
        state.planetary_pan_x = (grid_cx - lc.x) * zoom;
        state.planetary_pan_y = (grid_cy - lc.y) * zoom;
        state.planetary_center_pending = false;
    }

    const ImVec2 view_origin = ImVec2{ canvas_centre.x + state.planetary_pan_x,
                                       canvas_centre.y + state.planetary_pan_y };

    // Local world space → screen space.
    auto to_screen = [&](ImVec2 lp) -> ImVec2 {
        return {
            view_origin.x + (lp.x - grid_cx) * zoom,
            view_origin.y + (lp.y - grid_cy) * zoom,
        };
    };

    // Clip to the grid area so hexes don't overdraw the title bar or the solar canvas.
    dl->PushClipRect(grid_area_origin, grid_area_origin + grid_area_size, true);

    // Tiles that carry a building, mapped to their type so the marker pass can
    // draw the type-specific glyph.
    std::unordered_map<entity_id, building_type> built_tiles;
    for (const auto& [id, bld] : w.buildings)
    {
        auto tile_it = w.tiles.find(bld.tile);
        if (tile_it != w.tiles.end() && tile_it->second.body == state.active_body)
            built_tiles[bld.tile] = bld.type;
    }

    // Spatial index: tile id keyed by its packed (col, row) on the active body,
    // so the Faction-lens border pass can find a tile's grid neighbours without
    // scanning w.tiles. Key packs row-major: row * gw + col (col < gw always).
    std::unordered_map<long long, entity_id> tile_at;
    for (const auto& [id, tile] : w.tiles)
    {
        if (tile.body == state.active_body)
            tile_at[static_cast<long long>(tile.grid_y) * gw + tile.grid_x] = id;
    }

    // Nation owner of a tile, or null_entity when the tile is absent from
    // tile_to_nation (unclaimed). Used by the border pass to compare adjacent
    // owners; "unclaimed" is treated as its own distinct owner.
    auto nation_of = [&](entity_id tile_id) -> entity_id {
        const auto it = w.tile_to_nation.find(tile_id);
        return it != w.tile_to_nation.end() ? it->second : null_entity;
    };

    // Corporation owner of a tile, keyed by tile id. Built only when corporations
    // exist; drives both the building-marker colour and the Corporation lens tint.
    std::unordered_map<entity_id, entity_id> tile_to_corp;
    if (!w.corporations.empty())
    {
        for (const auto& [corp_id, corp] : w.corporations)
        {
            for (entity_id bld_id : corp.assets)
            {
                const auto bld_it = w.buildings.find(bld_id);
                if (bld_it != w.buildings.end())
                    tile_to_corp[bld_it->second.tile] = corp_id;
            }
        }
    }

    // Identity colour for a corporation: the player's corp is faction slot 0;
    // rivals get a stable per-corp slot via a multiplicative hash, kept off slot 0
    // so a rival never collides with the player's colour. Shared by the marker
    // pass and the Corporation lens so the two always agree.
    auto corp_colour = [&](entity_id corp_id) -> ImU32 {
        if (corp_id == w.player_entity)
            return palette::faction_colour(0);
        int slot = static_cast<int>(
            (static_cast<uint32_t>(corp_id) * 2654435761u) % palette::faction_slot_count);
        if (slot == 0)
            slot = 1;
        return palette::faction_colour(slot);
    };

    const ImVec2 mouse = ImGui::GetIO().MousePos;

    // Hover resolves to a single tile copy. Adjacent hexes' circular hit-tests
    // overlap, and the cylinder draws several wrap copies of each tile, so more
    // than one (tile, copy) can satisfy the hover test at once. The nearest hex
    // centre to the cursor wins; its outline is drawn after the grid pass so only
    // one tile highlights (see the highlight tie convention in highlight.hpp).
    entity_id hovered_tile     = null_entity;
    bool      hovered_selected = false;
    bool      have_hover       = false;
    float     best_d2          = std::numeric_limits<float>::max();
    ImVec2    hover_verts[6]   = {};

    // Slightly shrink the drawn hex so the background shows through as a border.
    // Use the full circumradius for hit-testing so small hexes stay clickable.
    const float draw_r = hex_size * zoom - 1.0f;
    const float hit_r  = hex_size * zoom;

    // --- Infinite horizontal scroll ---
    // The grid is a cylinder: column gw wraps onto column 0. In screen space the
    // grid repeats every `period_px = gw * col_step * zoom`. Each tile is drawn
    // (and hit-tested) at every integer wrap offset k whose copy falls within the
    // canvas, so panning past either edge continues seamlessly from the far side.
    const float period_px = static_cast<float>(gw) * col_step * zoom;
    const float visible_left  = grid_area_origin.x - hit_r;
    const float visible_right = grid_area_origin.x + grid_area_size.x + hit_r;

    for (const auto& [id, tile] : w.tiles)
    {
        if (tile.body != state.active_body)
            continue;

        const ImVec2 lc   = hex_local_centre(tile.grid_x, tile.grid_y, hex_size);
        const ImVec2 sc   = to_screen(lc);

        // Fill is the tile's terrain colour, except under the Faction lens where
        // a tile owned by a nation is tinted that nation's identity colour. The
        // tint is a direct replacement (no blend); unclaimed tiles — absent from
        // tile_to_nation, e.g. ocean — keep their terrain hue so the political
        // map still reads as terrain underneath.
        ImU32 fill = terrain_colour(tile.composition);
        if (state.overlay == overlay_mode::faction)
        {
            const auto nat_it = w.tile_to_nation.find(id);
            if (nat_it != w.tile_to_nation.end())
                fill = palette::nation_colour(nat_it->second);
        }
        // Corporation lens: tint a tile that carries a corporate building with its
        // owning corp's colour (direct replacement, like the faction tint). Tiles
        // with no corporate building keep their terrain hue — no nation underlay.
        else if (state.overlay == overlay_mode::corporation)
        {
            const auto corp_it = tile_to_corp.find(id);
            if (corp_it != tile_to_corp.end())
                fill = corp_colour(corp_it->second);
        }
        const auto   built_it  = built_tiles.find(id);
        const bool   built     = built_it != built_tiles.end();
        const building_type built_type = built ? built_it->second : building_type::none;
        const bool   selected  = (id == state.selected_entity);

        // Range of wrap copies that land inside the canvas horizontally.
        const int k_min = (period_px > 0.0f)
            ? static_cast<int>(std::ceil((visible_left  - sc.x) / period_px)) : 0;
        const int k_max = (period_px > 0.0f)
            ? static_cast<int>(std::floor((visible_right - sc.x) / period_px)) : 0;

        for (int k = k_min; k <= k_max; ++k)
        {
            const float cx = sc.x + static_cast<float>(k) * period_px;
            const float cy = sc.y;

            ImVec2 verts[6];
            hex_vertices(verts, cx, cy, draw_r);
            dl->AddConvexPolyFilled(verts, 6, fill);

            // Nation borders (Faction lens only). Draw a dark line on every hex
            // edge shared with a neighbour of a different owner — including the
            // claimed/unclaimed boundary. The grid is odd-r offset, so the six
            // neighbour offsets differ between even and odd rows.
            if (state.overlay == overlay_mode::faction)
            {
                const entity_id own_nation = nation_of(id);

                // Standard odd-r neighbour offsets (col, row deltas).
                static const int even_off[6][2] =
                    {{+1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, +1}, {0, +1}};
                static const int odd_off[6][2] =
                    {{+1, 0}, {+1, -1}, {0, -1}, {-1, 0}, {0, +1}, {+1, +1}};
                const int (*off)[2] = (tile.grid_y & 1) ? odd_off : even_off;

                for (int n = 0; n < 6; ++n)
                {
                    const int nrow = tile.grid_y + off[n][1];
                    if (nrow < 0 || nrow >= gh)
                        continue; // Off the top/bottom edge: no neighbour tile.

                    // Columns wrap on the horizontal cylinder.
                    int ncol = (tile.grid_x + off[n][0]) % gw;
                    if (ncol < 0)
                        ncol += gw;

                    const auto nb_it = tile_at.find(static_cast<long long>(nrow) * gw + ncol);
                    if (nb_it == tile_at.end())
                        continue;
                    if (nation_of(nb_it->second) == own_nation)
                        continue; // Same owner: interior edge, no border.

                    // Draw the shared edge via the midpoint-perpendicular method:
                    // place the segment at the midpoint of the centre-to-centre
                    // line, perpendicular to it, with length equal to one hex side
                    // (== circumradius draw_r for a regular hexagon). This avoids
                    // mapping neighbour directions to per-vertex pairs, which the
                    // offset-row vertex ordering makes error-prone. The neighbour's
                    // screen centre is taken at the SAME wrap offset k as this tile.
                    const ImVec2 nb_lc = hex_local_centre(ncol, nrow, hex_size);
                    ImVec2 nb_sc = to_screen(nb_lc);
                    nb_sc.x += static_cast<float>(k) * period_px;

                    float dirx = nb_sc.x - cx;
                    float diry = nb_sc.y - cy;
                    const float len = std::sqrt(dirx * dirx + diry * diry);
                    if (len <= 0.0f)
                        continue;
                    dirx /= len;
                    diry /= len;

                    const float mx = (cx + nb_sc.x) * 0.5f;
                    const float my = (cy + nb_sc.y) * 0.5f;
                    const float px = -diry; // perpendicular to the centre line
                    const float py =  dirx;
                    const float half = draw_r * 0.5f;

                    dl->AddLine({mx - px * half, my - py * half},
                                {mx + px * half, my + py * half},
                                IM_COL32(20, 20, 20, 200), 1.5f);
                }
            }

            // Corporation lens: outline the player's own tiles so the player's
            // footprint reads at a glance against rivals' flat tints. Rival tiles
            // are distinguished by their fill colour alone (see the fill pass).
            if (state.overlay == overlay_mode::corporation)
            {
                const auto corp_it = tile_to_corp.find(id);
                if (corp_it != tile_to_corp.end() && corp_it->second == w.player_entity)
                    dl->AddPolyline(verts, 6, palette::faction_colour(0),
                                    ImDrawFlags_Closed, 2.0f);
            }

            if (built)
            {
                const float mr = std::max(2.0f, draw_r * 0.22f);

                // Building markers carry their owning corporation's colour (the
                // player's corp gets faction slot 0). Always on, independent of
                // the lens. Tiles with no corporate owner stay white.
                ImU32 marker_col = IM_COL32(255, 255, 255, 255);
                const auto corp_it = tile_to_corp.find(id);
                if (corp_it != tile_to_corp.end())
                    marker_col = corp_colour(corp_it->second);

                icons::building(dl, {cx, cy}, mr, built_type, marker_col);
            }

            // Selection outline is drawn on every visible copy of the selected
            // tile; hover is deferred to a single nearest copy, resolved below.
            draw_hex_highlight(dl, verts,
                resolve_highlight(selected, /*hovered=*/false, /*pinned=*/false));

            // Hit-test: distance to hex centre < circumradius (approximate,
            // sufficient for usability). Scoped per wrap copy so the highlight
            // lands on the copy actually under the cursor; nearest centre wins.
            if (input_enabled)
            {
                const float dx = mouse.x - cx;
                const float dy = mouse.y - cy;
                const float d2 = dx * dx + dy * dy;
                const bool in_area = mouse.x >= grid_area_origin.x &&
                                     mouse.x <= grid_area_origin.x + grid_area_size.x &&
                                     mouse.y >= grid_area_origin.y &&
                                     mouse.y <= grid_area_origin.y + grid_area_size.y;
                if (in_area && d2 <= hit_r * hit_r && d2 < best_d2)
                {
                    best_d2          = d2;
                    hovered_tile     = id;
                    hovered_selected = selected;
                    have_hover       = true;
                    std::copy(std::begin(verts), std::end(verts), std::begin(hover_verts));
                }
            }
        }
    }

    // Hover outline for the single resolved copy. Skipped when the hovered tile is
    // also the selection — selection outranks hover, and its ring is already drawn.
    if (have_hover && !hovered_selected)
        draw_hex_highlight(dl, hover_verts, highlight::hovered);

    // Layer 4 scaffold: building-placement ghost preview. When construction mode
    // is active and a tile is hovered, draw a translucent-intent marker of the
    // chosen building type at the hovered copy's centre, tinted green when the
    // placement-rules seam accepts the tile and red when it rejects it. This is a
    // pure preview — it reads placement_rules::can_place and mutates nothing. The
    // hovered copy's centre is the centroid of its six vertices (the wrap copy the
    // cursor actually resolved to); the marker radius mirrors the built-marker pass.
    if (have_hover && state.construction.active)
    {
        float gx = 0.0f, gy = 0.0f;
        for (const ImVec2& v : hover_verts)
        {
            gx += v.x;
            gy += v.y;
        }
        gx /= 6.0f;
        gy /= 6.0f;

        const float mr = std::max(2.0f, draw_r * 0.22f);

        const tile_component& hovered = w.tiles.at(hovered_tile);
        const bool placeable = placement_rules::can_place(
            hovered, state.construction.type, state.construction.target);
        const ImU32 ghost_col = placeable ? palette::positive : palette::negative;

        icons::building(dl, {gx, gy}, mr, state.construction.type, ghost_col);
    }

    dl->PopClipRect();

    if (!input_enabled)
        return;

    // Hover tooltip.
    if (hovered_tile != null_entity)
    {
        const tile_component& tile = w.tiles.at(hovered_tile);
        ImGui::BeginTooltip();
        ImGui::Text("[%d, %d]", tile.grid_x, tile.grid_y);
        ImGui::Text("%s \xc2\xb7 %s", composition_name(tile.composition),
                                       landform_name(tile.landform));
        ImGui::Text("Hazard: %.2f", tile.hazard_level);
        ImGui::Text("Habitability: %.2f", tile.habitability);
        bool any_deposit = false;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (tile.resource_deposit[r] > 0.0f)
            {
                ImGui::Text("%s: %.1f", resource_name(static_cast<resource_type>(r)), tile.resource_deposit[r]);
                any_deposit = true;
            }
        }
        if (!any_deposit)
            ImGui::TextDisabled("No deposits");
        ImGui::EndTooltip();
    }

    // Click handling. The surface is the bottom rung, so there is nothing to
    // descend into: a single left-click simply selects the hovered tile (null
    // clears the selection on empty space) and fills the Selection info element.
    // No view change; the player ascends via the minimap. See SELECTION.md.
    // Construction mode suppresses selection: in placement mode a left-click is a
    // construction gesture, not a selection one, so it must not retarget the
    // Selection info element.
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (!state.construction.active)
            state.selected_entity = hovered_tile;
        // Layer 4 scaffold: placement-mode click is a non-mutating seam — v0.0.6
        // will construct here. No world/selection mutation in v0.0.5.
    }

    // Pan and zoom. Middle mouse button pans; scroll wheel zooms, anchored at
    // the cursor so the point under the mouse stays fixed.
    {
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            state.planetary_pan_x += io.MouseDelta.x;
            state.planetary_pan_y += io.MouseDelta.y;
        }

        // Keep horizontal pan bounded to one grid period. The grid wraps, so
        // this is visually identical but stops pan_x growing without limit.
        if (period_px > 0.0f)
            state.planetary_pan_x = std::fmod(state.planetary_pan_x, period_px);

        if (io.MouseWheel != 0.0f)
        {
            const float new_zoom = std::clamp(zoom * std::pow(1.1f, io.MouseWheel), kMinZoom, kMaxZoom);
            // World point under the cursor, kept fixed across the zoom change.
            const ImVec2 wp = { (mouse.x - view_origin.x) / zoom + grid_cx,
                                (mouse.y - view_origin.y) / zoom + grid_cy };
            state.planetary_pan_x = mouse.x - (wp.x - grid_cx) * new_zoom - canvas_centre.x;
            state.planetary_pan_y = mouse.y - (wp.y - grid_cy) * new_zoom - canvas_centre.y;
            state.planetary_zoom  = new_zoom;
        }
    }
}

} // namespace ui
