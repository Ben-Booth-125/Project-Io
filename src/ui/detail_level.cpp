#include "detail_level.hpp"

#include "presentation.hpp" // palette::selection / text_secondary
#include "ui_state.hpp"

#include <imgui.h>

namespace ui {

namespace {

/// The chevron glyph, in the font's ASCII range. The atlas carries a narrow
/// codepoint set (BL-234), so the control draws its own triangle through the
/// draw list rather than relying on a arrow character being present at all.
void draw_chevron(ImDrawList* dl, ImVec2 centre, float r, bool up, ImU32 col)
{
    const float dy = up ? -r * 0.45f : r * 0.45f;
    dl->AddTriangleFilled({centre.x - r * 0.75f, centre.y - dy},
                          {centre.x + r * 0.75f, centre.y - dy},
                          {centre.x,             centre.y + dy}, col);
}

} // namespace

bool is_expanded(const ui_state& ui, detail_surface s, int key)
{
    return ui.expanded.surface == s && ui.expanded.key == key;
}

bool any_expanded(const ui_state& ui)
{
    return ui.expanded.surface != detail_surface::none;
}

void expand(ui_state& ui, detail_surface s, int key)
{
    ui.expanded.surface     = s;
    ui.expanded.key         = key;
    ui.corp_rollup_drill    = -1; // ...and at its roll-up, never a stale drill
}

void fold(ui_state& ui)
{
    ui.expanded.surface  = detail_surface::none;
    ui.expanded.key      = 0;
    ui.corp_rollup_drill = -1;
}

bool fold_chevron(ui_state& ui, detail_surface s, int key)
{
    const bool  open = is_expanded(ui, s, key);
    const float h    = ImGui::GetFrameHeight();

    ImGui::PushID(static_cast<int>(s) * 1024 + key);
    const bool clicked = ImGui::InvisibleButton("##fold", {h, h});
    const bool hot     = ImGui::IsItemHovered();
    ImGui::PopID();

    const ImVec2 mn = ImGui::GetItemRectMin();
    draw_chevron(ImGui::GetWindowDrawList(), {mn.x + h * 0.5f, mn.y + h * 0.5f},
                 h * 0.30f, open, hot ? palette::hover : palette::text_secondary);
    if (hot)
        ImGui::SetTooltip(open ? "Fold up" : "Expand - full screen");

    if (clicked)
    {
        // A symmetric two-state control, so it IS a toggle (the standing rule's
        // default), unlike the superseded three-segment stepper which had no null
        // member to undo to.
        if (open) fold(ui);
        else      expand(ui, s, key);
    }
    return clicked;
}

bool fold_overlay_begin(ui_state& ui, detail_surface s, int key, const char* title)
{
    if (!is_expanded(ui, s, key))
        return false;

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({0.0f, 0.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(disp, ImGuiCond_Always);
    ImGui::SetNextWindowFocus();

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar   |
        ImGuiWindowFlags_NoResize     |
        ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoCollapse   |
        ImGuiWindowFlags_NoSavedSettings;

    // OPAQUE, and generously inset. Both were found by looking at the first
    // captures: a translucent scrim let the whole shell read through the overlay,
    // so a deliberate mode switch looked like a ghost drawn over the game, and
    // zero-inset content sat jammed into the top-left corner of a 1280 px screen.
    // A mode switch has to look like one.
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(14, 15, 19, 255));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {36.0f, 28.0f});
    ImGui::Begin("##fold_overlay", nullptr, flags);
    ImGui::PopStyleVar();   // WindowPadding is consumed by Begin
    ImGui::PopStyleColor();

    // Header: title left, fold-up chevron right. Identical on every surface, so the
    // way out of the overlay is in the same place whatever opened it.
    const float bar_w = ImGui::GetContentRegionAvail().x;
    const float h     = ImGui::GetFrameHeight();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", title);
    ImGui::SameLine(bar_w - h);
    fold_chevron(ui, s, key);
    ImGui::Separator();
    ImGui::Spacing();
    return true;
}

void fold_overlay_end(ui_state&)
{
    ImGui::End();
}

// The per-chart question log (BL-247) used to live here: a "? Why this chart"
// toggle revealing an Answers/Because pair. REMOVED 2026-08-02 (Ben, NEEDS_REVIEW
// NR-018) — "the why shouldn't leak into the UI, players don't need us telling them
// what they ought to ask of the game." The log survives as DEVELOPMENT documentation
// only; BL-260 relocates the authored pairs (recoverable from commit d143aa4) into a
// JSON store tied to the backlog item that demanded each surface. Do not reinstate a
// draw path here without reopening NR-018.

} // namespace ui
