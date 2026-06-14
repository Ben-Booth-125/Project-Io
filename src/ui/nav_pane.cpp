#include "nav_pane.hpp"

#include "icons.hpp"

#include <imgui.h>

#include <cstdio>

namespace ui {

void draw_nav_pane(ui_state& state, float top_offset)
{
    const ImVec2 disp = ImGui::GetIO().DisplaySize;

    ImGui::SetNextWindowPos({0.0f, top_offset});
    ImGui::SetNextWindowSize({nav_pane_width, disp.y - top_offset});

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoScrollbar         |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    // Tight padding so square icon slots fill the narrow rail.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{6.0f, 8.0f});
    ImGui::Begin("##nav_pane", nullptr, flags);

    // Ten icon slots, each a square selectable filling the rail width with a
    // centred vector glyph instead of a worded label; the menu name lives in a
    // hover tooltip. Slot placement is temporary for Layer 2 — only the Tile
    // Ledger is wired, parked at slot 8 — while canvas work takes priority over
    // the menu layout.
    constexpr int tab_count         = 10;
    constexpr int tile_ledger_slot  = 8; // 1-based position of the Tile Ledger
    constexpr int economy_panel_slot = 7; // 1-based position of the Economy panel

    // Square slots; Selectable treats a nonzero size as literal, so derive the
    // rail width explicitly rather than passing -1.
    const float slot_size = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl        = ImGui::GetWindowDrawList();

    for (int slot = 1; slot <= tab_count; ++slot)
    {
        const bool   is_ledger  = (slot == tile_ledger_slot);
        const bool   is_economy = (slot == economy_panel_slot);
        const ImVec2 p0         = ImGui::GetCursorScreenPos();

        char id[16];
        std::snprintf(id, sizeof(id), "##nav%d", slot);

        if (is_ledger)
        {
            // The one wired slot. Toggles the ledger window open/closed.
            if (ImGui::Selectable(id, state.show_tile_ledger, 0, {slot_size, slot_size}))
                state.show_tile_ledger = !state.show_tile_ledger;
            ImGui::SetItemTooltip("Tile Ledger");
        }
        else if (is_economy)
        {
            // Toggles the Layer 3 economy panel open/closed.
            if (ImGui::Selectable(id, state.show_economy_panel, 0, {slot_size, slot_size}))
                state.show_economy_panel = !state.show_economy_panel;
            ImGui::SetItemTooltip("Economy");
        }
        else
        {
            // Reserved placeholder. Disabled, but a glyph keeps the slot legible.
            ImGui::BeginDisabled();
            ImGui::Selectable(id, false, 0, {slot_size, slot_size});
            ImGui::EndDisabled();
        }

        // Glyph centred on the slot: a table for the wired ledger, a neutral
        // hollow square for reserved slots.
        const ImVec2 centre = {p0.x + slot_size * 0.5f, p0.y + slot_size * 0.5f};
        const float  r      = slot_size * 0.30f;
        if (is_ledger)
            icons::ledger(dl, centre, r, IM_COL32(225, 228, 235, 255));
        else if (is_economy)
            icons::market(dl, centre, r, IM_COL32(225, 228, 235, 255));
        else
            icons::placeholder(dl, centre, r, IM_COL32(110, 116, 132, 255));
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace ui
