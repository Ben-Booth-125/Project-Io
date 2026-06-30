#include "market_ledger.hpp"

#include "ledger_chrome.hpp"
#include "plot_history.hpp"
#include "presentation.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace ui {

namespace {

/// Body display name, or a synthetic fallback.
std::string body_label(const world& w, entity_id body)
{
    const auto it = w.bodies.find(body);
    if (it != w.bodies.end())
        return it->second.name;
    return "Body #" + std::to_string(body);
}

/// Market display label: "Region (col, row)" based on centre tile, or "Market N".
std::string market_label(const world& w, entity_id mid, int index)
{
    const market_component& mc = w.markets.at(mid);
    const auto tit = w.tiles.find(mc.centre_tile);
    if (tit != w.tiles.end())
        return "Region (" + std::to_string(tit->second.grid_x) + ","
                          + std::to_string(tit->second.grid_y) + ")";
    return "Market " + std::to_string(index + 1);
}

/// Markets on a given body, sorted by ascending entity id (deterministic).
std::vector<entity_id> markets_on_body(const world& w, entity_id body)
{
    std::vector<entity_id> result;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body)
            result.push_back(mid);
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace

void draw_market_ledger(const world& w, const ui_state& /*s*/,
                        const market_plot_history& history, bool& open)
{
    if (!open)
        return;

    ImGui::SetNextWindowPos(ledger_window_spawn, ImGuiCond_Once);
    ImGui::SetNextWindowSize(ledger_window_size, ImGuiCond_Once);

    if (!ImGui::Begin("Market Ledger", &open))
    {
        ImGui::End();
        return;
    }

    if (w.markets.empty())
    {
        ImGui::TextDisabled("No markets.");
        ImGui::End();
        return;
    }

    // --- Body selector ---
    // Collect distinct bodies that have at least one market.
    std::vector<entity_id> bodies;
    for (const auto& [mid, mc] : w.markets)
    {
        if (std::find(bodies.begin(), bodies.end(), mc.body) == bodies.end())
            bodies.push_back(mc.body);
    }
    std::sort(bodies.begin(), bodies.end());

    static entity_id selected_body = null_entity;
    if (selected_body == null_entity || w.bodies.find(selected_body) == w.bodies.end())
        selected_body = (w.home_body != null_entity && w.bodies.count(w.home_body))
                        ? w.home_body : bodies.front();

    const std::string body_preview = body_label(w, selected_body);
    if (ImGui::BeginCombo("Body", body_preview.c_str()))
    {
        for (entity_id bid : bodies)
        {
            const bool sel = (bid == selected_body);
            if (ImGui::Selectable(body_label(w, bid).c_str(), sel))
                selected_body = bid;
            if (sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    const std::vector<entity_id> body_markets = markets_on_body(w, selected_body);
    if (body_markets.empty())
    {
        ImGui::TextDisabled("No markets on this body.");
        ImGui::End();
        return;
    }

    // --- Dashboard: one summary row per market on the body ---
    static entity_id selected_market = null_entity;
    // Validate selection against the current body's markets.
    if (selected_market == null_entity ||
        std::find(body_markets.begin(), body_markets.end(), selected_market) == body_markets.end())
        selected_market = body_markets.front();

    ImGui::TextDisabled("Markets on %s (%zu)", body_label(w, selected_body).c_str(),
                        body_markets.size());

    constexpr ImGuiTableFlags dash_flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_RowBg;

    if (ImGui::BeginTable("##market_dash", 4, dash_flags))
    {
        ImGui::TableSetupColumn("Market");
        ImGui::TableSetupColumn("Supply");
        ImGui::TableSetupColumn("Demand");
        ImGui::TableSetupColumn("Turnover");
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(body_markets.size()); ++i)
        {
            const entity_id mid = body_markets[static_cast<std::size_t>(i)];
            const market_component& mc = w.markets.at(mid);
            const bool is_sel = (mid == selected_market);

            float total_supply = 0.0f, total_demand = 0.0f, total_cleared = 0.0f;
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                total_supply  += mc.supply[r];
                total_demand  += mc.demand[r];
                total_cleared += std::min(mc.supply[r], mc.demand[r]);
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            {
                const std::string lbl = market_label(w, mid, i);
                if (ImGui::Selectable(lbl.c_str(), is_sel,
                                      ImGuiSelectableFlags_SpanAllColumns))
                    selected_market = mid;
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.0f", total_supply);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.0f", total_demand);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.0f", total_cleared);
        }

        ImGui::EndTable();
    }

    ImGui::Separator();

    // --- Detail: selected market's per-resource table ---
    if (w.markets.find(selected_market) == w.markets.end())
    {
        ImGui::End();
        return;
    }

    const market_component& mc = w.markets.at(selected_market);
    ImGui::TextDisabled("%s — resource detail",
                        market_label(w, selected_market,
                            static_cast<int>(std::find(body_markets.begin(),
                                body_markets.end(), selected_market) - body_markets.begin())).c_str());

    constexpr ImGuiTableFlags detail_flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp;

    float detail_cleared = 0.0f;

    if (ImGui::BeginTable("##market_detail", 5, detail_flags))
    {
        ImGui::TableSetupColumn("Resource");
        ImGui::TableSetupColumn("Supply");
        ImGui::TableSetupColumn("Demand");
        ImGui::TableSetupColumn("Price");
        ImGui::TableSetupColumn("Net");
        ImGui::TableHeadersRow();

        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (mc.base_price[r] <= 0.0f)
                continue; // resource not traded at this market

            const float supply = mc.supply[r];
            const float demand = mc.demand[r];
            const float net    = supply - demand;
            detail_cleared += std::min(supply, demand);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            {
                const resource_presentation& rp =
                    presentation_of(static_cast<resource_type>(r));
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(rp.colour), "%s", rp.name);
            }

            ImGui::TableSetColumnIndex(1);
            {
                ImU32 col = (supply > 0.0f) ? palette::positive : palette::neutral;
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%.1f", supply);
            }

            ImGui::TableSetColumnIndex(2);
            {
                ImU32 col = (demand > supply) ? palette::negative : palette::neutral;
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%.1f", demand);
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f", mc.price[r]);

            ImGui::TableSetColumnIndex(4);
            {
                ImU32 col;
                if (net > 0.0f)       col = palette::positive;
                else if (net < 0.0f)  col = palette::negative;
                else                  col = palette::neutral;
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%+.1f", net);
            }
        }

        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::Text("Turnover this tick: %.1f", detail_cleared);

    // --- Price / supply / demand trends (BL-063) ----------------------------
    // Show trend plots for each traded resource in the selected market.
    // A resource selector lets the player focus on one resource at a time.
    const auto hist_it = history.find(selected_market);
    if (hist_it != history.end())
    {
        ImGui::Separator();
        ImGui::TextDisabled("Trends (selected market)");

        // Build a list of traded resources for the selector.
        static int selected_resource_idx = 0;
        std::vector<std::size_t> traded;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (mc.base_price[r] > 0.0f)
                traded.push_back(r);

        if (!traded.empty())
        {
            if (selected_resource_idx >= static_cast<int>(traded.size()))
                selected_resource_idx = 0;

            const resource_presentation& cur_rp =
                presentation_of(static_cast<resource_type>(traded[static_cast<std::size_t>(selected_resource_idx)]));
            if (ImGui::BeginCombo("##trend_res", cur_rp.name))
            {
                for (int i = 0; i < static_cast<int>(traded.size()); ++i)
                {
                    const resource_presentation& rp =
                        presentation_of(static_cast<resource_type>(traded[static_cast<std::size_t>(i)]));
                    const bool sel = (i == selected_resource_idx);
                    ImGui::PushStyleColor(ImGuiCol_Text,
                        ImGui::ColorConvertU32ToFloat4(rp.colour));
                    if (ImGui::Selectable(rp.name, sel))
                        selected_resource_idx = i;
                    ImGui::PopStyleColor();
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            const std::size_t r = traded[static_cast<std::size_t>(selected_resource_idx)];
            const auto& series  = hist_it->second[r];
            const float third   = (ImGui::GetContentRegionAvail().x
                                   - 2.0f * ImGui::GetStyle().ItemSpacing.x) / 3.0f;

            draw_plot("##price",  series.price,  {third, 50.0f}, &plot_col_blue,   "price");
            ImGui::SameLine();
            draw_plot("##supply", series.supply, {third, 50.0f}, &plot_col_green,  "supply");
            ImGui::SameLine();
            draw_plot("##demand", series.demand, {third, 50.0f}, &plot_col_orange, "demand");
        }
    }

    ImGui::End();
}

} // namespace ui
