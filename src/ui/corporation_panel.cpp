#include "corporation_panel.hpp"

#include "ledger_chrome.hpp"
#include "presentation.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace ui {

namespace {

const char* focus_label(industrial_focus f)
{
    switch (f)
    {
        case industrial_focus::extraction: return "Extraction";
        case industrial_focus::processing: return "Processing";
        case industrial_focus::trade:      return "Trade";
    }
    return "Unknown";
}

std::string nation_label(const world& w, entity_id nation_id)
{
    const auto it = w.nations.find(nation_id);
    if (it != w.nations.end() && !it->second.name.empty())
        return it->second.name;
    return "Nation #" + std::to_string(nation_id);
}

} // namespace

void draw_corporation_panel(const world& w, ui_state& s, bool& open)
{
    ImGui::SetNextWindowSize(ledger_window_size, ImGuiCond_Once);
    ImGui::SetNextWindowPos(ledger_window_spawn, ImGuiCond_Once);

    if (!ImGui::Begin("Corporations", &open))
    {
        ImGui::End();
        return;
    }

    // Sorted corporation list for deterministic row order.
    std::vector<entity_id> corps;
    corps.reserve(w.corporations.size());
    for (const auto& [id, _] : w.corporations)
        corps.push_back(id);
    std::sort(corps.begin(), corps.end());

    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg   |
        ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable("##corps", 6, table_flags))
    {
        ImGui::TableSetupColumn("Corporation");
        ImGui::TableSetupColumn("Focus");
        ImGui::TableSetupColumn("Home Nation");
        ImGui::TableSetupColumn("Cash Balance");
        ImGui::TableSetupColumn("Buildings");
        ImGui::TableSetupColumn("Status");
        ImGui::TableHeadersRow();

        for (entity_id id : corps)
        {
            const corporation_component& cc = w.corporations.at(id);
            const bool is_player = (id == w.player_entity);

            ImGui::TableNextRow();

            if (is_player)
            {
                const ImVec4 tint = ImGui::ColorConvertU32ToFloat4(
                    (palette::selection & 0x00FFFFFFu) | 0x28000000u);
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                    IM_COL32(255, 255, 255, 40));
            }

            // Selectable spanning all columns via the first column.
            ImGui::TableSetColumnIndex(0);
            const bool selected = (s.selected_entity == id);
            if (ImGui::Selectable(cc.name.c_str(), selected,
                    ImGuiSelectableFlags_SpanAllColumns))
            {
                s.selected_entity = id;
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("%s", focus_label(cc.focus));

            ImGui::TableSetColumnIndex(2);
            {
                const std::string nation = nation_label(w, cc.home_nation);
                ImGui::TextUnformatted(nation.c_str());
            }

            ImGui::TableSetColumnIndex(3);
            {
                const ImU32 col = (cc.balance < 0.0f) ? palette::negative : palette::positive;
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%.1f", cc.balance);
            }

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", static_cast<int>(cc.assets.size()));

            ImGui::TableSetColumnIndex(5);
            if (is_player)
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::pinned), "Player");
            else
                ImGui::TextDisabled("Active");
        }

        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Corporations: %d", static_cast<int>(w.corporations.size()));

    ImGui::End();
}

} // namespace ui
