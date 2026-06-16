#include "balance_ledger.hpp"

#include "ledger_chrome.hpp"
#include "presentation.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace ui {

namespace {

std::string corp_label(const world& w, entity_id corp)
{
    const auto it = w.corporations.find(corp);
    if (it != w.corporations.end() && !it->second.name.empty())
        return it->second.name;
    return "Corp #" + std::to_string(corp);
}

} // namespace

void draw_balance_ledger(const world& w, const ui_state& /*s*/, bool& open)
{
    if (!open)
        return;

    ImGui::SetNextWindowPos(ledger_window_spawn, ImGuiCond_Once);
    ImGui::SetNextWindowSize(ledger_window_size, ImGuiCond_Once);

    if (!ImGui::Begin("Balance Ledger", &open))
    {
        ImGui::End();
        return;
    }

    // Build a stable, sorted list of corporation ids for the combo.
    static entity_id selected_corp = null_entity;

    std::vector<entity_id> corp_ids;
    corp_ids.reserve(w.corporations.size());
    for (const auto& [id, _] : w.corporations)
        corp_ids.push_back(id);
    std::sort(corp_ids.begin(), corp_ids.end());

    // Default selection to the player's corporation on first open or when the
    // previously-selected id has been removed (empty world edge case).
    if (selected_corp == null_entity || w.corporations.find(selected_corp) == w.corporations.end())
        selected_corp = w.player_entity;

    // Corporation selector combo.
    const std::string preview = corp_label(w, selected_corp);
    if (ImGui::BeginCombo("Corporation", preview.c_str()))
    {
        for (entity_id id : corp_ids)
        {
            const bool is_sel = (id == selected_corp);
            const std::string label = corp_label(w, id);
            if (ImGui::Selectable(label.c_str(), is_sel))
                selected_corp = id;
            if (is_sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    const auto cit = w.corporations.find(selected_corp);
    if (cit == w.corporations.end())
    {
        ImGui::TextDisabled("No corporation data.");
        ImGui::End();
        return;
    }

    const corporation_component& cc = cit->second;

    // --- Treasury ---
    ImGui::SeparatorText("Treasury");

    const ImU32 bal_col = (cc.balance < 0.0f) ? palette::negative : palette::positive;
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(bal_col),
        "Balance:  %.2f", cc.balance);
    if (cc.balance < 0.0f)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(insolvent — no consequence in prototype)");
    }

    // --- Cashflow ---
    ImGui::SeparatorText("Cashflow (last tick)");
    ImGui::TextDisabled(
        "Per-category income and expenditure are resolved each tick and not\n"
        "retained. Run an economy tick and read the Economy panel for per-\n"
        "building output rates and the full (corp, body) pool breakdown.");

    // Summary table using balance as the single available persistent figure.
    if (ImGui::BeginTable("##cashflow", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Line");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupColumn("Notes");
        ImGui::TableHeadersRow();

        // Treasury balance — the only persistent financial field.
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Treasury balance");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(bal_col), "%.2f", cc.balance);
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("Running total; may go negative");

        // Starting capital for reference.
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Starting capital");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%.2f", cc.starting_capital);
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("Set at generation");

        // Net change since start.
        const float net = cc.balance - cc.starting_capital;
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Net since start");
        ImGui::TableSetColumnIndex(1);
        {
            const ImU32 net_col = (net < 0.0f) ? palette::negative
                                : (net > 0.0f) ? palette::positive
                                               : palette::neutral;
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(net_col), "%.2f", net);
        }
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("balance − starting capital");

        ImGui::EndTable();
    }

    // --- Asset count ---
    ImGui::SeparatorText("Assets");
    ImGui::Text("Buildings owned: %d", static_cast<int>(cc.assets.size()));

    ImGui::End();
}

} // namespace ui
