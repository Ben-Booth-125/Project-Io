#include "selection_card.hpp"

#include "hover_content.hpp"
#include "ledger_chrome.hpp" // ledger_window_spawn — the shared clear-of-chrome anchor

#include <algorithm>

namespace ui {

namespace {

constexpr float kCardWidth   = 320.0f;
constexpr float kCardMaxH    = 420.0f;
constexpr float kPad         = 10.0f;
constexpr float kRounding    = 6.0f;
constexpr float kMargin      = 16.0f; ///< Clearance from the canvas edge.
constexpr float kCloseBtnW   = 20.0f;

} // namespace

void draw_selection_card(const world& w, ui_state& ui,
                         ImVec2 canvas_origin, ImVec2 canvas_size)
{
    const entity_id sel = ui.selected_entity;
    const bool      open = sel != null_entity && sel != ui.selection_hidden_for;
    if (!open)
        return;

    // Position: the same clear-of-chrome anchor the ledger family spawns at
    // (shell column's right edge, below the profile tile) — avoids the
    // profile/header/minimap/explorer chrome the same way those windows do.
    // Clamped so the whole card stays inside canvas_origin/canvas_size.
    const ImVec2 anchor = ledger_window_spawn(canvas_origin.x + canvas_size.x);
    const ImVec2 pos    = { anchor.x, anchor.y };
    const ImVec2 maxh   = { kCardWidth,
                            std::min(kCardMaxH, canvas_origin.y + canvas_size.y - pos.y - kMargin) };

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ maxh.x, maxh.y }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.96f);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                                 | ImGuiWindowFlags_NoResize
                                 | ImGuiWindowFlags_NoMove
                                 | ImGuiWindowFlags_NoSavedSettings
                                 | ImGuiWindowFlags_NoNav
                                 | ImGuiWindowFlags_NoCollapse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  { kPad, kPad });

    bool dismissed = false;
    if (ImGui::Begin("##selection_card", nullptr, flags))
    {
        // Close (x) — top-right corner, same hide-not-destroy semantics the
        // former fold-out Selection element used (SELECTION.md).
        const float avail_w = ImGui::GetContentRegionAvail().x;
        ImGui::SameLine(std::max(0.0f, avail_w - kCloseBtnW));
        if (ImGui::SmallButton("x"))
            dismissed = true;

        ImGui::Separator();

        // Content — the shared entity-summary dispatch. BL-195 relocates the
        // full Selection element's content here in place of this call; BL-196
        // makes the content itself clickable to open a child card.
        draw_hover_content(w, ui, sel);
    }
    ImGui::End();

    ImGui::PopStyleVar(2);

    // Esc dismisses the card (unwinding from the root, per BL-196, closes it
    // entirely — with no child stack yet, every Esc is a root-unwind).
    if (!dismissed && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        dismissed = true;

    if (dismissed)
        ui.selection_hidden_for = sel;
}

} // namespace ui
