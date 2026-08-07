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

void draw_corporation_panel(const world& w, const std::vector<corp_standing>& standings,
                             ui_state& s, bool& open)
{
    if (!open)
        return;

    // Re-hosted into the shell fold-out column (BL-122); closed via the nav rail.
    if (!ui::foldout_begin("Corporations"))
    {
        ui::foldout_end();
        return;
    }

    // BL-111: SizingStretchProp collapsed all six columns to a single leading glyph in
    // the narrow shell column ('C F H C B S' headers, 'F T C 9 1 A' rows). Apply the
    // BL-081 pattern — stretch the identity column, fixed widths for the numerics — and
    // drop the two lowest-value columns so the four that matter fit legibly: Home Nation
    // is reachable by selecting the corp (Selection panel), and Status was only
    // Player/Active, already carried by the row tint + name colour. The reduced table is
    // the single question "how is each corporation doing?" (settles BL-121).
    //
    // BL-262 first slice: Balance was an exact number for every corp, including rivals —
    // a privacy violation against BL-068's competitor-visibility rule (rival internals are
    // private). Replaced with a three-axis standing profile (Reach / Capital / Share); the
    // player's row still shows exact figures, every rival row shows a band label only, no
    // numbers. No totals/aggregate row is added — BL-262's hard rule.
    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

    if (ImGui::BeginTable("##corps", 4, table_flags))
    {
        ImGui::TableSetupColumn("Corporation", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Reach",   ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableSetupColumn("Capital", ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableSetupColumn("Share",   ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableHeadersRow();

        for (const corp_standing& cs : standings)
        {
            const entity_id id = cs.corp;
            const corporation_component& cc = w.corporations.at(id);

            ImGui::TableNextRow();

            if (cs.is_player)
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
            }

            ImGui::TableSetColumnIndex(1);
            if (cs.is_player)
                ImGui::Text("%d bodies", cs.reach_bodies);
            else
                ImGui::TextUnformatted(standing_band_label(cs.reach_band));

            ImGui::TableSetColumnIndex(2);
            if (cs.is_player)
            {
                const ImU32 col = (cs.capital_balance < 0.0f) ? palette::negative : palette::positive;
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%.1f", cs.capital_balance);
            }
            else
            {
                ImGui::TextUnformatted(standing_band_label(cs.capital_band));
            }

            ImGui::TableSetColumnIndex(3);
            if (cs.is_player)
                ImGui::Text("%.0f%%", cs.market_share * 100.0f);
            else
                ImGui::TextUnformatted(standing_band_label(cs.share_band));
        }

        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Corporations: %d", static_cast<int>(w.corporations.size()));

    ui::foldout_end();
}

} // namespace ui
