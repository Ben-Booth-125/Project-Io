#include "selection_card.hpp"

#include "selection.hpp"        // selection_kind_of — the open/kind gate
#include "selection_panel.hpp"  // draw_selection_content — the card body

namespace ui {

namespace {

constexpr float kPad      = 10.0f;
constexpr float kRounding = 6.0f;

} // namespace

void draw_selection_band(world& w, const recipe_registry& reg,
                         const economy_report& report,
                         ui_state& ui,
                         ImVec2 band_origin, ImVec2 band_size)
{
    // The band is ALWAYS open (BL-266) — there is no dismissal and no hide state.
    // It never renders selection_kind::none: with no (valid) selection it rests on
    // the player's own corporation instead. The enumerator stays (other callers
    // use it); ui.selected_entity itself is left null, so deselect — "stop looking
    // at that, back to looking at yourself" — remains representable.
    entity_id sel = ui.selected_entity;
    if (sel == null_entity || selection_kind_of(w, sel) == selection_kind::none)
        sel = w.player_entity; // the player corp exists before the first frame

    // A DEPOSIT or PLATE selection reaches here with `selected_entity` null and is
    // substituted above like any other empty selection — harmlessly, because
    // draw_selection_content dispatches on the region fields BEFORE it looks at
    // the entity and returns without reading `sel` (BL-671). Worth stating: this
    // substitution is what made a plate press show the previous card, which is
    // what NR-697 was reporting.

    // ── Placement (BL-213) ──
    // Fixed: fills the exact rect the caller computed (bottom band, between the
    // shell column and the right chrome column). No click-anchoring, no
    // clamping — the band is always the same rect regardless of where the
    // selecting click landed.
    ImGui::SetNextWindowPos(band_origin, ImGuiCond_Always);
    ImGui::SetNextWindowSize(band_size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.96f);

    // Inputs are ALLOWED (unlike the transient hover card): the band hosts action
    // buttons routed through the deferred pending_* seams. No title bar / move /
    // resize / scroll — the content owns its own inner scroll regions.
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                                 | ImGuiWindowFlags_NoResize
                                 | ImGuiWindowFlags_NoMove
                                 | ImGuiWindowFlags_NoCollapse
                                 | ImGuiWindowFlags_NoNav
                                 | ImGuiWindowFlags_NoScrollbar
                                 | ImGuiWindowFlags_NoScrollWithMouse
                                 | ImGuiWindowFlags_NoBringToFrontOnFocus
                                 | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  { kPad, kPad });

    if (ImGui::Begin("##selection_band", nullptr, flags))
    {
        // The content function draws ui.selected_entity, so the resting
        // substitution is applied for the draw and restored after — unless the
        // content itself re-pointed the selection (e.g. a tile card building button, BL-1312).
        const entity_id real_sel = ui.selected_entity;
        ui.selected_entity       = sel;
        draw_selection_content(w, reg, report, ui);
        if (ui.selected_entity == sel)
            ui.selected_entity = real_sel;
    }
    ImGui::End();

    ImGui::PopStyleVar(2);

    // NB: Esc is handled by app.cpp (it must take precedence over the system-menu
    // toggle). The band does not
    // consume Esc itself — doing so would double-fire with the app's SDL handler.
}

} // namespace ui
