#define IMGUI_DEFINE_MATH_OPERATORS
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
        case overlay_mode::faction:     icons::faction    (dl, centre, r, colour); break;
        case overlay_mode::corporation: icons::corporation(dl, centre, r, colour); break;
        case overlay_mode::resource:    icons::resource   (dl, centre, r, colour); break;
        default: break;
    }
}

/// Draw the lens-local resource/good selector for the Resource and Market lenses.
/// Both lenses pick "which resource" from the same `lens_resource` field (LENSES.md
/// says the two selectors share a form), so one combo serves both; the Resource lens
/// additionally exposes its highest-value / single-resource mode toggle. Drawn inline
/// in the control strip only while one of those two lenses is active.
void draw_lens_selector(ui_state& ui)
{
    if (ui.overlay != overlay_mode::resource && ui.overlay != overlay_mode::market)
        return;

    // Resource lens: a mode toggle. Highest-value needs no selection; single-resource
    // reveals the picker. The Market lens always shows the picker (it has no "all goods").
    if (ui.overlay == overlay_mode::resource)
    {
        ImGui::SameLine();
        ImGui::Checkbox("Single", &ui.resource_lens_single);
    }

    const bool show_picker =
        (ui.overlay == overlay_mode::market) ||
        (ui.overlay == overlay_mode::resource && ui.resource_lens_single);
    if (!show_picker)
        return;

    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    const char* current = presentation_of(ui.lens_resource).name;
    if (ImGui::BeginCombo("##lens_resource", current))
    {
        for (std::size_t i = 0; i < resource_count; ++i)
        {
            const resource_type r = static_cast<resource_type>(i);
            const bool selected = (r == ui.lens_resource);
            if (ImGui::Selectable(presentation_of(r).name, selected))
                ui.lens_resource = r;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

} // namespace

const char* overlay_mode_name(overlay_mode m)
{
    switch (m)
    {
        case overlay_mode::supply:      return "Supply routes";
        case overlay_mode::market:      return "Market prices";
        case overlay_mode::faction:     return "Faction presence";
        case overlay_mode::corporation: return "Corporation ownership";
        case overlay_mode::resource:    return "Resource density";
        default:                        return "None";
    }
}

const char* overlay_mode_short_name(overlay_mode m)
{
    switch (m)
    {
        case overlay_mode::supply:      return "Supply";
        case overlay_mode::market:      return "Market";
        case overlay_mode::faction:     return "Faction";
        case overlay_mode::corporation: return "Corp";
        case overlay_mode::resource:    return "Resource";
        default:                        return "None";
    }
}

void toggle_overlay(ui_state& ui, overlay_mode m)
{
    ui.overlay = (ui.overlay == m) ? overlay_mode::none : m;
}

void draw_overlay_controls(ui_state& ui, float left_x, float bottom_y)
{
    // The selectable lenses, in mode-bar order. overlay_mode::none is not a
    // button — clicking the active lens clears back to it (toggle_overlay).
    constexpr overlay_mode modes[5] = {
        overlay_mode::supply, overlay_mode::market, overlay_mode::faction,
        overlay_mode::corporation, overlay_mode::resource };

    ImGui::SetNextWindowPos({left_x, bottom_y}, ImGuiCond_Always, {0.0f, 1.0f});
    ImGui::SetNextWindowBgAlpha(0.65f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoScrollbar         |
        ImGuiWindowFlags_NoNav               |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings     |
        ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin("##overlay_controls", nullptr, flags);

    ImGui::TextUnformatted("Lens:");

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float icon_size = ImGui::GetFrameHeight(); // square button matching control height

    int idx = 0;
    for (overlay_mode m : modes)
    {
        ImGui::SameLine();
        const bool active = (ui.overlay == m);

        // Invisible button carries the click + hover; the glyph is drawn over its
        // rect. The active lens gets a highlighted backing and a brighter glyph so
        // the selected state reads without the worded label.
        ImGui::PushID(idx++);
        const bool clicked = ImGui::InvisibleButton("##lens", {icon_size, icon_size});
        const ImVec2 rmin = ImGui::GetItemRectMin();
        const ImVec2 rmax = ImGui::GetItemRectMax();
        const bool   hovered = ImGui::IsItemHovered();

        if (active || hovered)
            dl->AddRectFilled(rmin, rmax,
                ImGui::GetColorU32(active ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered), 2.0f);

        const ImU32 glyph = active ? palette::selection : palette::neutral;
        draw_lens_icon(dl, m, rmin, rmax, glyph);

        if (hovered)
            ImGui::SetTooltip("%s", overlay_mode_name(m));
        if (clicked)
            toggle_overlay(ui, m);
        ImGui::PopID();
    }

    // Lens-local controls: the Resource/Market resource-or-good selector (and the
    // Resource mode toggle) appear inline after the lens buttons when their lens is active.
    draw_lens_selector(ui);

    ImGui::End();
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
