#include "economy_panel.hpp"

#include "format.hpp"
#include "icons.hpp"
#include "foldout_column.hpp" // shell fold-out column host (BL-122)
#include "plot_history.hpp"
#include "presentation.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace ui {

namespace {

/// Corp display name, or a synthetic "Corp #id" if the entry is missing.
std::string corp_label(const world& w, entity_id corp)
{
    const auto it = w.corporations.find(corp);
    if (it != w.corporations.end() && !it->second.name.empty())
        return it->second.name;
    return "Corp #" + std::to_string(corp);
}

/// Body display name, or a synthetic fallback.
std::string body_label(const world& w, entity_id body)
{
    const auto it = w.bodies.find(body);
    if (it != w.bodies.end())
        return it->second.name;
    return "Body #" + std::to_string(body);
}

/// A resource icon pip + name, so every listing carries a symbol the player can
/// learn to recognise on sight, not just a colour-tinted word (playtest patch,
/// 2026-07-06 — ICONS.md's per-resource glyph vocabulary is still one shape
/// (the diamond pip) differentiated only by identity colour; distinct per-resource
/// glyph shapes are a larger follow-on, not this quick pass).
void resource_text(resource_type r)
{
    const resource_presentation& rp = presentation_of(r);
    const float sz = ImGui::GetTextLineHeight() * 0.4f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    icons::resource(ImGui::GetWindowDrawList(), ImVec2(p.x + sz, p.y + sz), sz, r);
    ImGui::Dummy(ImVec2(sz * 2.0f, sz * 2.0f));
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(rp.colour), "%s", rp.name);
}

// --- corporation balances ----------------------------------------------------
void draw_balances(const world& w)
{
    if (!ImGui::CollapsingHeader("Corporation balances", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    std::vector<entity_id> corps;
    corps.reserve(w.corporations.size());
    for (const auto& [id, _] : w.corporations)
        corps.push_back(id);
    std::sort(corps.begin(), corps.end());

    // BL-081: the name column stretches so the corp identity is never clipped;
    // Focus/Balance get comfortable fixed widths so the full numeric value shows.
    if (ImGui::BeginTable("##balances", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Corporation", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Focus", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Balance", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableHeadersRow();

        for (entity_id id : corps)
        {
            const corporation_component& cc = w.corporations.at(id);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            if (id == w.player_entity)
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::pinned), "%s", cc.name.c_str());
            else
                ImGui::TextUnformatted(cc.name.c_str());

            ImGui::TableSetColumnIndex(1);
            const char* focus =
                cc.focus == industrial_focus::extraction ? "Extraction" :
                cc.focus == industrial_focus::processing ? "Processing" : "Trade";
            ImGui::TextDisabled("%s", focus);

            ImGui::TableSetColumnIndex(2);
            // Negative balances are flagged red (palette::negative).
            const ImU32 col = (cc.balance < 0.0f) ? palette::negative : palette::positive;
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%.1f", cc.balance);
        }
        ImGui::EndTable();
    }
}

// --- (corp, body) pools ------------------------------------------------------
void draw_pools(const world& w)
{
    if (!ImGui::CollapsingHeader("Stockpile pools (corp x body)", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (w.corp_body_pools.empty())
    {
        ImGui::TextDisabled("No pools yet.");
        return;
    }

    for (const auto& [key, pool] : w.corp_body_pools)
    {
        const std::string title =
            corp_label(w, key.first) + "  @  " + body_label(w, key.second);

        // Skip wholly-empty pools to keep the panel legible.
        bool any = false;
        for (std::size_t r = 0; r < resource_count && !any; ++r)
            any = pool.quantities[r] > 0.0f;

        if (ImGui::TreeNodeEx(title.c_str(), any ? ImGuiTreeNodeFlags_DefaultOpen : 0))
        {
            if (!any)
            {
                ImGui::TextDisabled("(empty)");
            }
            // BL-081: resource name stretches, quantity gets a fixed width, so the
            // cells never collapse to a leading glyph (as the balances table did).
            else if (ImGui::BeginTable("##pool", 2, ImGuiTableFlags_Borders))
            {
                ImGui::TableSetupColumn("Resource", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Quantity", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableHeadersRow();
                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    if (pool.quantities[r] <= 0.0f)
                        continue;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    resource_text(static_cast<resource_type>(r));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.1f", pool.quantities[r]);
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }
}

// --- workforce contention ----------------------------------------------------
// Surfaces the (corp, body) labour-pool contention from this tick's report. Only
// the throttled pools (scalar < 1.0) are listed — an uncontended economy shows the
// reassuring "all fully staffed" line (POPULATION.md § Workforce model, step 1).
void draw_workforce(const world& w, const economy_report& report)
{
    if (!ImGui::CollapsingHeader("Workforce (corp x body)", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    bool any = false;
    for (const auto& [key, scalar] : report.workforce_contention)
        if (scalar < 1.0f) { any = true; break; }

    if (!any)
    {
        ImGui::TextDisabled("All buildings fully staffed (no labour contention).");
        return;
    }

    // BL-081: the corp@body label stretches, the staffing figure gets a fixed
    // width, so neither collapses to a single leading character.
    if (ImGui::BeginTable("##workforce", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Corp @ body", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Staffing", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableHeadersRow();
        for (const auto& [key, scalar] : report.workforce_contention)
        {
            if (scalar >= 1.0f)
                continue;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const std::string title =
                corp_label(w, key.first) + "  @  " + body_label(w, key.second);
            ImGui::TextUnformatted(title.c_str());
            ImGui::TableSetColumnIndex(1);
            // Throttled labour reads as a warning colour and a percentage.
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                "%.0f%% (labour short)", scalar * 100.0f);
        }
        ImGui::EndTable();
    }
}

// --- player balance trend (BL-063) -------------------------------------------
void draw_balance_trend(const player_plot_history& hist)
{
    if (!ImGui::CollapsingHeader("Player balance trend"))
        return;

    ImGui::TextDisabled("Balance");
    draw_plot("##balance", hist.balance, {-1.0f, 55.0f}, &plot_col_blue);

    if (!hist.income.empty() || !hist.expenditure.empty())
    {
        const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        draw_plot("##income", hist.income, {half, 40.0f}, &plot_col_green, "income");
        ImGui::SameLine();
        draw_plot("##expend", hist.expenditure, {half, 40.0f}, &plot_col_orange, "spend");
    }
}

// --- markets -----------------------------------------------------------------
void draw_markets(const world& w)
{
    if (!ImGui::CollapsingHeader("Markets (supply / demand)", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (w.markets.empty())
    {
        ImGui::TextDisabled("No markets.");
        return;
    }

    std::vector<entity_id> mids;
    mids.reserve(w.markets.size());
    for (const auto& [id, _] : w.markets)
        mids.push_back(id);
    std::sort(mids.begin(), mids.end());

    for (entity_id mid : mids)
    {
        const market_component& mc = w.markets.at(mid);
        if (ImGui::TreeNodeEx(body_label(w, mc.body).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            // BL-081: resource name stretches; the three numeric columns get fixed
            // widths so the values stay legible instead of collapsing.
            if (ImGui::BeginTable("##market", 4, ImGuiTableFlags_Borders))
            {
                ImGui::TableSetupColumn("Resource", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Supply", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Demand", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Price",  ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableHeadersRow();
                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    // Show only resources that trade here (a non-zero base price).
                    if (mc.base_price[r] <= 0.0f)
                        continue;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    resource_text(static_cast<resource_type>(r));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.1f", mc.supply[r]);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%.1f", mc.demand[r]);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%.2f", mc.price[r]);
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }
}

} // namespace

void draw_economy_panel(const world& w,
                        const recipe_registry& reg,
                        const economy_report& report,
                        const player_plot_history& history,
                        bool* p_open)
{
    if (p_open && !*p_open)
        return;

    // Re-hosted into the shell fold-out column (BL-122): pinned + borderless, no floating
    // spawn. Closing is via the nav rail (accordion), not a title-bar 'x'.
    ui::foldout_begin("Economy");

    // BL-081: per-building profitability now lives in the Corp Dashboard (BL-074);
    // the recipe registry is no longer read here.
    (void)reg;

    draw_balance_trend(history);
    draw_balances(w);
    draw_workforce(w, report);
    draw_pools(w);
    draw_markets(w);

    ui::foldout_end();
}

} // namespace ui
