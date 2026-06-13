#include "nav_pane.hpp"

#include <imgui.h>

#include <cstdio>

namespace ui {

void draw_nav_pane(ui_state& state)
{
    const ImVec2 disp = ImGui::GetIO().DisplaySize;

    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize({nav_pane_width, disp.y});

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##nav_pane", nullptr, flags);

    ImGui::TextDisabled("MENUS");
    ImGui::Separator();
    ImGui::Spacing();

    // Ten numbered navigation slots. Each tab is a full-width selectable; the
    // selected state reflects whether that menu is open. Slot placement is
    // temporary for Layer 2 — only the Tile Ledger is wired, parked at slot 8
    // while the canvas work takes priority over the menu layout.
    constexpr int    tab_count       = 10;
    constexpr int    tile_ledger_slot = 8; // 1-based position of the Tile Ledger
    // Selectable treats a nonzero size.x as a literal width, so derive the full
    // pane width explicitly rather than passing -1 (which yields a zero-width,
    // label-clipping box).
    const     ImVec2 tab_size        = {ImGui::GetContentRegionAvail().x, 28.0f};

    for (int slot = 1; slot <= tab_count; ++slot)
    {
        if (slot == tile_ledger_slot)
        {
            // The one wired tab. Toggles the ledger window open/closed.
            if (ImGui::Selectable("8. Tile Ledger", state.show_tile_ledger, 0, tab_size))
                state.show_tile_ledger = !state.show_tile_ledger;
        }
        else
        {
            // Reserved placeholder. Disabled but numbered so the slot is visible.
            char label[32];
            std::snprintf(label, sizeof(label), "%d.##nav%d", slot, slot);
            ImGui::BeginDisabled();
            ImGui::Selectable(label, false, 0, tab_size);
            ImGui::EndDisabled();
        }
    }

    ImGui::End();
}

} // namespace ui
