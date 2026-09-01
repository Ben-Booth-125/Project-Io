#include "overlay.hpp"

#include "icons.hpp"
#include "presentation.hpp"

#include <algorithm>

namespace ui {

namespace {

/// Draw the lens glyph for @p m centred in @p rect with the given @p colour. The
/// glyph half-extent is derived from the rect so the three icons read at one
/// consistent size in the control strip.
void draw_lens_icon(ImDrawList* dl, overlay_mode m, ImVec2 rect_min, ImVec2 rect_max, ImU32 colour)
{
    const ImVec2 centre = (rect_min + rect_max) * 0.5f;
    const float  r      = std::min(rect_max.x - rect_min.x, rect_max.y - rect_min.y) * 0.5f - 2.0f;
    switch (m)
    {
        case overlay_mode::supply:      icons::supply     (dl, centre, r, colour); break;
        case overlay_mode::market:      icons::market     (dl, centre, r, colour); break;
        case overlay_mode::corporation: icons::corporation(dl, centre, r, colour); break;
        case overlay_mode::resource:    icons::resource   (dl, centre, r, colour); break;
        case overlay_mode::population:  icons::population (dl, centre, r, colour); break;
        case overlay_mode::scarcity:    icons::scarcity   (dl, centre, r, colour); break;
        case overlay_mode::industry:    icons::industry   (dl, centre, r, colour); break;
        case overlay_mode::continent:   icons::continent  (dl, centre, r, colour); break;
        // Reach/Supply-routes (BL-011/BL-014) reuse the existing convoy/supply
        // glyphs rather than adding new ones — dedicated glyphs are an open TODO
        // in ui::icons (src/ui/icons.{hpp,cpp}, out of this lens work's file
        // scope) for whenever these lenses join the on-screen strip.
        case overlay_mode::reach:         icons::convoy(dl, centre, r, colour); break;
        case overlay_mode::supply_routes: icons::supply(dl, centre, r, colour); break;
        // Throughput has its OWN glyph (BL-605). It borrowed Reach's convoy
        // chevron while it was keyboard-only, on the grounds that the cycle names
        // off-strip lenses apart; joining the strip ends that, because an
        // on-screen lens carries one distinct glyph (LENSES.md) and two strip
        // neighbours sharing a mark is exactly what that rule forbids.
        case overlay_mode::throughput:    icons::throughput(dl, centre, r, colour); break;
        // Company (BL: corporation/company split, Ben 2026-08-28) borrows the
        // corporation glyph while it is KEYBOARD-ONLY, on the same precedent
        // Reach and Supply-routes set above. It must earn a distinct mark before
        // it joins the strip — two strip neighbours sharing a glyph is exactly
        // what LENSES.md forbids, and Corp would be its neighbour.
        case overlay_mode::company:       icons::corporation(dl, centre, r, colour); break;
        default: break;
    }
}

} // namespace

const char* overlay_mode_name(overlay_mode m)
{
    switch (m)
    {
        case overlay_mode::supply:      return "Supply routes";
        case overlay_mode::market:      return "Market catchment boundaries";
        case overlay_mode::corporation: return "Corporation ownership";
        case overlay_mode::resource:    return "Resource deposits";
        case overlay_mode::population:  return "Workforce efficiency";
        case overlay_mode::scarcity:    return "Market scarcity";
        case overlay_mode::industry:    return "Industry density";
        case overlay_mode::continent:   return "Continents (tectonic plates)";
        case overlay_mode::reach:         return "Reach (commercial connectivity)";
        case overlay_mode::supply_routes: return "Supply-routes graph";
        case overlay_mode::throughput:    return "Throughput (active Logistic Points)";
        case overlay_mode::company:       return "Company holdings (background firms)";
        default:                        return "None";
    }
}

const char* overlay_mode_short_name(overlay_mode m)
{
    switch (m)
    {
        case overlay_mode::supply:      return "Supply";
        case overlay_mode::market:      return "Market";
        case overlay_mode::corporation: return "Corp";
        case overlay_mode::resource:    return "Resource";
        case overlay_mode::population:  return "Population";
        case overlay_mode::scarcity:    return "Scarcity";
        case overlay_mode::industry:    return "Industry";
        case overlay_mode::continent:   return "Continent";
        case overlay_mode::reach:         return "Reach";
        case overlay_mode::supply_routes: return "Supply routes";
        case overlay_mode::throughput:    return "Throughput";
        case overlay_mode::company:       return "Company";
        default:                        return "None";
    }
}

void toggle_overlay(ui_state& ui, overlay_mode m)
{
    ui.overlay = (ui.overlay == m) ? overlay_mode::none : m;
}

void draw_overlay_controls(ui_state& ui, float x, float top_y, float w)
{
    // THE STRIP IS KEYED ON THE RUNG (Ben, 2026-08-28: "add any missing lenses to
    // the strip if that lens focuses on the planetary view. We can rotate lenses
    // for circumplanetary or solar views").
    //
    // It used to be one hand-kept array of six, and the six were chosen by what
    // FIT — the header here said outright that Scarcity, Industry, Reach and
    // Supply-routes were keyboard-only because "they do not fit the 240 px minimap
    // bar this row now lives on". That is a layout accident deciding a discovery
    // question, and it left six of the twelve built lenses reachable only by a key
    // nothing on screen mentions.
    //
    // Keying on the rung dissolves it, because no rung wants all twelve. Each row
    // below is LENSES.md's own per-rung representation table, read straight off:
    // a lens appears where it draws something. Supply, Reach and Supply-routes are
    // body-to-body reads whose home is Solar; Market and Scarcity carry a
    // per-body mark at Circumplanetary; the sub-body lenses are Planetary-only
    // because their unit of meaning is a tile, a deposit or a plate.
    //
    // A lens absent from every row would be unreachable except by keyboard, so
    // adding one here is part of adding a lens — the same duty ACTIONS.json
    // carries for its press.
    static constexpr overlay_mode planetary_modes[] = {
        overlay_mode::corporation, overlay_mode::company,
        overlay_mode::resource,    overlay_mode::market,
        overlay_mode::scarcity,    overlay_mode::industry,
        overlay_mode::population,  overlay_mode::continent,
        overlay_mode::throughput };

    static constexpr overlay_mode circumplanetary_modes[] = {
        overlay_mode::market, overlay_mode::scarcity, overlay_mode::supply };

    static constexpr overlay_mode solar_modes[] = {
        overlay_mode::supply, overlay_mode::reach, overlay_mode::supply_routes };

    const overlay_mode* modes_begin = planetary_modes;
    std::size_t         modes_count = std::size(planetary_modes);
    switch (ui.primary_level)
    {
    case canvas_level::solar:
        modes_begin = solar_modes;   modes_count = std::size(solar_modes);   break;
    case canvas_level::circumplanetary:
        modes_begin = circumplanetary_modes;
        modes_count = std::size(circumplanetary_modes);                      break;
    default:
        break; // planetary — the full sub-body set above
    }

    const float bar_h = ImGui::GetFrameHeight() + 6.0f;
    ImGui::SetNextWindowPos({x, top_y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({w, bar_h}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f); // transparent — sits over the minimap's own bar fill
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoScrollbar         |
        ImGuiWindowFlags_NoNav               |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {2.0f, 2.0f});
    ImGui::Begin("##overlay_controls", nullptr, flags);

    ImDrawList*  dl        = ImGui::GetWindowDrawList();
    const float  icon_size = ImGui::GetFrameHeight();
    const float  spacing   = ImGui::GetStyle().ItemSpacing.x;
    // Left-aligned row from a small pad, leaving room at the right for the
    // resource-selector popup button when a resource-keyed lens is active.
    float       cursor_x = x + 6.0f;
    const float cursor_y = top_y + 3.0f;

    int idx = 0;
    for (std::size_t mi = 0; mi < modes_count; ++mi)
    {
        const overlay_mode m = modes_begin[mi];
        ImGui::SetCursorScreenPos({cursor_x, cursor_y});
        const bool active = (ui.overlay == m);

        // Invisible button carries the click + hover; the glyph is drawn over its
        // rect. The active lens gets a highlighted backing and a brighter glyph so
        // the selected state reads without a worded label.
        ImGui::PushID(idx++);
        const bool clicked = ImGui::InvisibleButton("##lens", {icon_size, icon_size});
        const ImVec2 rmin = ImGui::GetItemRectMin();
        const ImVec2 rmax = ImGui::GetItemRectMax();
        const bool   hovered = ImGui::IsItemHovered();

        if (active || hovered)
            dl->AddRectFilled(rmin, rmax,
                ImGui::GetColorU32(active ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered), 2.0f);

        draw_lens_icon(dl, m, rmin, rmax, active ? palette::selection : palette::neutral);

        if (hovered)
            ImGui::SetTooltip("%s", overlay_mode_name(m));
        if (clicked)
            toggle_overlay(ui, m);
        ImGui::PopID();
        cursor_x += icon_size + spacing;
    }

    // The resource/good selector (Resource / Market / Scarcity share `lens_resource`)
    // moved into the on-canvas lens legend (BL-134) — it no longer docks here.

    ImGui::End();
    ImGui::PopStyleVar();
}

void draw_canvas_overlay(const world& w, const ui_state& ui, canvas_level level,
                         ImVec2 origin, ImVec2 size, ImDrawList* dl)
{
    (void)w;      // overlay data source — used once lenses draw real geometry
    (void)level;  // lenses differ per rung — switched on here by later layers
    (void)origin; // lens geometry is positioned in canvas space here by later layers
    (void)size;
    (void)dl;

    if (ui.overlay == overlay_mode::none)
        return;

    // No on-canvas geometry yet. The active lens is named by the bottom overlay
    // control strip (draw_overlay_controls); this pass is the extension point
    // where Layer 5 supply routes and later lenses draw real geometry, keyed by
    // ui.overlay and level.
}

} // namespace ui
