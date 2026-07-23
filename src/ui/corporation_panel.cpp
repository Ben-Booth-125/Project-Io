#include "corporation_panel.hpp"

#include "foldout_column.hpp" // shell fold-out column host (BL-122)
#include "presentation.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace ui {

namespace {
} // namespace

void draw_corporation_panel(const world& w, ui_state& s, bool& open)
{
    if (!open)
        return;

    // Re-hosted into the shell fold-out column (BL-122); closed via the nav rail.
    if (!ui::foldout_begin("Corporations"))
    {
        ui::foldout_end();
        return;
    }

    // Sorted corporation list for deterministic row order.
    std::vector<entity_id> corps;
    corps.reserve(w.corporations.size());
    for (const auto& [id, _] : w.corporations)
        corps.push_back(id);
    std::sort(corps.begin(), corps.end());

    // BL-111: SizingStretchProp collapsed all six columns to a single leading glyph in
    // the narrow shell column ('C F H C B S' headers, 'F T C 9 1 A' rows). Apply the
    // BL-081 pattern — stretch the identity column, fixed widths for the numerics — and
    // drop the two lowest-value columns so the four that matter fit legibly: Home Nation
    // is reachable by selecting the corp (Selection panel), and Status was only
    // Player/Active, already carried by the row tint + name colour. The reduced table is
    // the single question "how is each corporation doing?" (settles BL-121).
    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

    // In the ~244px shell column the name is the identity and must win the width, so it
    // stretches while Balance takes a tight fixed width (a generous fixed width here
    // is exactly what re-collapsed the name to one glyph). Building count is dropped — a
    // minor stat, still on the Selection panel when a corp is clicked. BL-145: the Focus
    // column is hidden blanket (industrial_focus stays a data-model field for
    // world-gen/economy, just not surfaced in UI).
    if (ImGui::BeginTable("##corps", 2, table_flags))
    {
        ImGui::TableSetupColumn("Corporation", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Balance", ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableHeadersRow();

        for (entity_id id : corps)
        {
            const corporation_component& cc = w.corporations.at(id);
            const bool is_player = (id == w.player_entity);

            ImGui::TableNextRow();

            if (is_player)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                    IM_COL32(255, 255, 255, 40));

            // Selectable spanning all columns via the first; selecting routes full
            // detail (incl. home nation) to the Selection panel.
            ImGui::TableSetColumnIndex(0);
            const bool selected = (s.selected_entity == id);
            if (ImGui::Selectable(cc.name.c_str(), selected,
                    ImGuiSelectableFlags_SpanAllColumns))
            {
                s.selected_entity = id;
                s.card_anchor     = {-1.0f, -1.0f}; // non-canvas selection → centre the card (BL-194)
            }

            ImGui::TableSetColumnIndex(1);
            {
                const ImU32 col = (cc.balance < 0.0f) ? palette::negative : palette::positive;
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%.1f", cc.balance);
            }
        }

        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Corporations: %d", static_cast<int>(w.corporations.size()));

    ui::foldout_end();
}

} // namespace ui
