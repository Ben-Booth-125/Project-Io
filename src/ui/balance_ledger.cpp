#include "balance_ledger.hpp"

#include "foldout_column.hpp" // shell fold-out column host (BL-122)
#include "presentation.hpp"

#include <imgui.h>

#include <algorithm>
#include <vector>

namespace ui {

namespace {

/// Smoothed per-tick net: mean of the last `window` balance deltas (BL-072). A
/// single tick's net is noisy, so the projected runway reads off this trailing
/// average rather than the last delta alone.
float smoothed_net(const std::vector<float>& balance_history, int window = 4)
{
    const int n = static_cast<int>(balance_history.size());
    if (n < 2)
        return 0.0f;
    const int take = std::min(window, n - 1);
    float sum = 0.0f;
    for (int i = 0; i < take; ++i)
        sum += balance_history[n - 1 - i] - balance_history[n - 2 - i];
    return sum / static_cast<float>(take);
}

} // namespace

void draw_balance_ledger(const world& w, const economy_report& report,
                         const std::vector<float>& balance_history, bool& open)
{
    if (!open)
        return;

    // Re-hosted into the shell fold-out column (BL-122); closed via the nav rail.
    if (!ui::foldout_begin("Balance Ledger"))
    {
        ui::foldout_end();
        return;
    }

    // Player-only budget (BL-142): pinned permanently to the player's own corp.
    // Rival treasuries/cashflow stay private per the competitor-visibility rule
    // (BL-068) — no selector to inspect another corp's internals.
    const entity_id selected_corp = w.player_entity;

    const auto cit = w.corporations.find(selected_corp);
    if (cit == w.corporations.end())
    {
        ImGui::TextDisabled("No corporation data.");
        ui::foldout_end();
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
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                           "(in debt - interest accruing)");
    }

    // --- Cashflow (BL-072): the itemised per-tick flows apply_budget now retains. ---
    ImGui::SeparatorText("Cashflow (last tick)");

    const auto bud_it = report.budgets.find(selected_corp);
    if (bud_it == report.budgets.end())
    {
        ImGui::TextDisabled("Run an economy tick to populate the breakdown.");
    }
    else
    {
        const corp_budget& bud = bud_it->second;

        if (ImGui::BeginTable("##cashflow", 2,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            // Explicit stretch weights — leaving the columns to proportional auto-fit
            // collapsed the label column to a few pixels (labels clipped to "In…").
            ImGui::TableSetupColumn("Line",  ImGuiTableColumnFlags_WidthStretch, 3.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableHeadersRow();

            // One signed flow row (income positive, every cost negative). Costs that
            // are exactly zero (e.g. interest while solvent) are hidden to keep the
            // table to the lines that actually moved the balance.
            const auto flow_row = [](const char* label, float value, ImU32 col,
                                     bool show_zero = true)
            {
                if (!show_zero && value == 0.0f)
                    return;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(label);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%+.2f", value);
            };

            flow_row("Income (sales)",  +bud.income,      palette::positive);
            flow_row("Inputs bought",   -bud.expenditure, palette::negative);
            flow_row("Maintenance",     -bud.maintenance, palette::negative);
            flow_row("Wages",           -bud.wages,       palette::negative);
            flow_row("Interest (debt)", -bud.interest,    palette::negative, /*show_zero*/ false);

            // Net row — equals the delta apply_budget added to the balance this tick.
            const float net = bud.net();
            const ImU32 net_col = (net < 0.0f) ? palette::negative
                                : (net > 0.0f) ? palette::positive
                                               : palette::neutral;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Net / tick");
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(net_col), "%+.2f", net);

            ImGui::EndTable();
        }

        // --- Projected runway (BL-072) ---
        // Player-only view (BL-142): always uses the smoothed trailing net
        // (less noisy than a single tick's delta). Runway = balance / burn, in quarters.
        const bool  have_history = balance_history.size() >= 2;
        const float burn = have_history ? smoothed_net(balance_history) : bud.net();
        ImGui::Spacing();
        if (cc.balance < 0.0f)
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                               "Runway: in debt");
        }
        else if (burn >= 0.0f)
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::positive),
                               "Runway: stable - balance holding or growing");
        }
        else
        {
            const float quarters = cc.balance / -burn;
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                               "Runway: ~%.0f quarters (%.1f yr) at current burn",
                               quarters, quarters / 4.0f);
        }
        if (have_history && cc.balance >= 0.0f)
        {
            // The smoothed-burn note only applies to the burn-based runway, not the
            // flat "in debt" read.
            ImGui::SameLine();
            ImGui::TextDisabled("(smoothed)");
        }

        // Persistent since-generation figures, kept for reference below the per-tick view.
        const float since = cc.balance - cc.starting_capital;
        const ImU32 since_col = (since < 0.0f) ? palette::negative
                              : (since > 0.0f) ? palette::positive
                                               : palette::neutral;
        ImGui::Spacing();
        ImGui::TextDisabled("Starting capital: %.2f", cc.starting_capital);
        ImGui::SameLine();
        ImGui::TextDisabled("  |  Since start: ");
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(since_col), "%+.2f", since);
    }

    // --- Asset count ---
    ImGui::SeparatorText("Assets");
    ImGui::Text("Buildings owned: %d", static_cast<int>(cc.assets.size()));

    // --- Policies / Budget laws (BL-142): signpost stubs, no behaviour yet. ---
    ImGui::SeparatorText("Policies");
    ImGui::TextDisabled("(not implemented)");

    ImGui::SeparatorText("Budget laws");
    ImGui::TextDisabled("(not implemented)");

    ui::foldout_end();
}

} // namespace ui
