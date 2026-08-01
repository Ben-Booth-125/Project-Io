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
    ui.why_note_open        = 0;  // the overlay opens in its resting state
    ui.corp_rollup_drill    = -1; // ...and at its roll-up, never a stale drill
}

void fold(ui_state& ui)
{
    ui.expanded.surface  = detail_surface::none;
    ui.expanded.key      = 0;
    ui.why_note_open     = 0;
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

bool why_note(ui_state& ui, const char* answers, const char* because)
{
    return why_note(ui, ImGui::GetID("##why"), answers, because);
}

bool why_note(ui_state& ui, unsigned int id_raw, const char* answers, const char* because)
{
    // Optional as a pair: an unlabelled chart draws no toggle rather than an empty
    // note. Settles the item's open question 1 — forcing every chart to carry one
    // would manufacture prose, and a missing pair is a review signal, not a defect.
    if (answers == nullptr || because == nullptr || answers[0] == '\0' || because[0] == '\0')
        return false;

    const ImGuiID id = static_cast<ImGuiID>(id_raw);
    if (ui.why_note_open == why_note_first)
        ui.why_note_open = id; // the verify sentinel claims the first note drawn
    const bool open = (ui.why_note_open == id);

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(palette::text_secondary));
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 24));
    if (ImGui::SmallButton(open ? "? Why this chart  -" : "? Why this chart  +"))
        ui.why_note_open = open ? 0u : id; // one note at a time; re-click closes
    ImGui::PopStyleColor(3);

    if (open)
    {
        // Exactly two lines and nothing more. "Answers" is the question a player
        // would actually ask; "Because" is why this evidence settles it.
        ImGui::Indent();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("Answers:");
        ImGui::SameLine();
        ImGui::TextUnformatted(answers);
        ImGui::TextDisabled("Because:");
        ImGui::SameLine();
        ImGui::TextUnformatted(because);
        ImGui::PopTextWrapPos();
        ImGui::Unindent();
    }
    return open;
}

} // namespace ui
