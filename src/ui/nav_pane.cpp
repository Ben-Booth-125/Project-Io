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

    // Nine slots from MENU.md § Menu set and ordering (slot 10 is a spare placeholder).
    // Slots 1–9 match the MENU.md curated sequence; live slots toggle their panel;
    // placeholder slots (Research, Workforce, Corp Strategy, Diplomacy, History) are
    // disabled — a neutral glyph keeps them legible.
    constexpr int tab_count = 10;

    // Square slots; Selectable treats a nonzero size as literal, so derive the
    // rail width explicitly rather than passing -1.
    const float slot_size = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl        = ImGui::GetWindowDrawList();

    for (int slot = 1; slot <= tab_count; ++slot)
    {
        const ImVec2 p0 = ImGui::GetCursorScreenPos();

        char id[16];
        std::snprintf(id, sizeof(id), "##nav%d", slot);

        switch (slot)
        {
        case 1: // Corporation overview (BL-022)
            if (ImGui::Selectable(id, state.show_corporation_panel, 0, {slot_size, slot_size}))
                state.show_corporation_panel = !state.show_corporation_panel;
            ImGui::SetItemTooltip("Corporations");
            break;
        case 2: // Budget (BL-028)
            if (ImGui::Selectable(id, state.show_balance_ledger, 0, {slot_size, slot_size}))
                state.show_balance_ledger = !state.show_balance_ledger;
            ImGui::SetItemTooltip("Budget");
            break;
        case 3: // Workforce / Population — placeholder (BL-042 step 2 feeds this)
            ImGui::BeginDisabled();
            ImGui::Selectable(id, false, 0, {slot_size, slot_size});
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("Workforce (coming)");
            break;
        case 4: // Research — placeholder
            ImGui::BeginDisabled();
            ImGui::Selectable(id, false, 0, {slot_size, slot_size});
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("Research (coming)");
            break;
        case 5: // Market Ledger (BL-027)
            if (ImGui::Selectable(id, state.show_market_ledger, 0, {slot_size, slot_size}))
                state.show_market_ledger = !state.show_market_ledger;
            ImGui::SetItemTooltip("Market Ledger");
            break;
        case 6: // Construction / Buildings (BL-029)
            if (ImGui::Selectable(id, state.show_construction_panel, 0, {slot_size, slot_size}))
                state.show_construction_panel = !state.show_construction_panel;
            ImGui::SetItemTooltip("Construction");
            break;
        case 7: // Corp. Strategy — placeholder
            ImGui::BeginDisabled();
            ImGui::Selectable(id, false, 0, {slot_size, slot_size});
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("Corp. Strategy (coming)");
            break;
        case 8: // Diplomacy — placeholder
            ImGui::BeginDisabled();
            ImGui::Selectable(id, false, 0, {slot_size, slot_size});
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("Diplomacy (coming)");
            break;
        case 9: // History (Tile Ledger lives here per MENU.md renaming)
            if (ImGui::Selectable(id, state.show_tile_ledger, 0, {slot_size, slot_size}))
                state.show_tile_ledger = !state.show_tile_ledger;
            ImGui::SetItemTooltip("History");
            break;
        default: // Slot 10 — spare placeholder
            ImGui::BeginDisabled();
            ImGui::Selectable(id, false, 0, {slot_size, slot_size});
            ImGui::EndDisabled();
            break;
        }

        // Glyph centred on the slot: a table for the wired ledger, a neutral
        // Glyph centred on the slot.
        const ImVec2 centre = {p0.x + slot_size * 0.5f, p0.y + slot_size * 0.5f};
        const float  r      = slot_size * 0.30f;
        const ImU32  live   = IM_COL32(225, 228, 235, 255);
        const ImU32  dim    = IM_COL32(110, 116, 132, 255);
        switch (slot)
        {
        case 1: icons::corporation(dl, centre, r, live); break;
        case 2: icons::ledger(dl, centre, r, live);      break;
        case 5: icons::market(dl, centre, r, live);      break;
        case 6: icons::building(dl, centre, r, building_type::processing_facility, live); break;
        case 9: icons::ledger(dl, centre, r, live);      break;
        default: icons::placeholder(dl, centre, r, dim); break;
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace ui
