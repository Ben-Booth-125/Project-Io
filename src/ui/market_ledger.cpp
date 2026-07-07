#include "market_ledger.hpp"

#include "foldout_column.hpp" // shell fold-out column host (BL-122)
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

std::string market_city_name(const world& w, entity_id mid)
{
    const auto mit = w.markets.find(mid);
    if (mit == w.markets.end())
        return "Market #" + std::to_string(mid);
    const market_component& mc = mit->second;
    // Resolve the population centre anchoring the market's centre tile → its city name.
    if (mc.centre_tile != null_entity)
        for (const auto& [cid, tid] : w.population_centre_tile)
            if (tid == mc.centre_tile)
            {
                const auto nit = w.population_centre_name.find(cid);
                if (nit != w.population_centre_name.end() && !nit->second.empty())
                    return nit->second;
                break;
            }
    return body_label(w, mc.body); // unanchored / unnamed fallback
}

void draw_market_ledger(const world& w, const ui_state& /*s*/,
                        const market_plot_history& history, bool& open)
{
    if (!open)
        return;

    // Re-hosted into the shell fold-out column (BL-122); closed via the nav rail.
    if (!ui::foldout_begin("Market Ledger"))
    {
        ui::foldout_end();
        return;
    }

    if (w.markets.empty())
    {
        ImGui::TextDisabled("No markets.");
        ui::foldout_end();
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
        ui::foldout_end();
        return;
    }

    // --- Market / city selector (cascades from the body) ---
    static entity_id selected_market = null_entity;
    if (selected_market == null_entity ||
        std::find(body_markets.begin(), body_markets.end(), selected_market) == body_markets.end())
        selected_market = body_markets.front();

    const std::string mkt_preview = market_city_name(w, selected_market);
    if (ImGui::BeginCombo("Market", mkt_preview.c_str()))
    {
        for (entity_id mid : body_markets)
        {
            const bool sel = (mid == selected_market);
            if (ImGui::Selectable(market_city_name(w, mid).c_str(), sel))
                selected_market = mid;
            if (sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    if (w.markets.find(selected_market) == w.markets.end())
    {
        ui::foldout_end();
        return;
    }
    const market_component& mc = w.markets.at(selected_market);

    // --- Price over time: one small chart per traded good at this market, scrollable ---
    // The whole question this view answers is "how has each good's price moved here?",
    // so every traded good gets its own price sparkline stacked in a scroll region.
    ImGui::SeparatorText("Price over time");
    const auto hist_it = history.find(selected_market);
    if (ImGui::BeginChild("##price_scroll", {0.0f, 0.0f}, false))
    {
        bool any = false;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (mc.base_price[r] <= 0.0f)
                continue; // good not traded at this market
            any = true;
            ImGui::PushID(static_cast<int>(r));

            const resource_presentation& rp = presentation_of(static_cast<resource_type>(r));
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(rp.colour), "%s", rp.name);
            ImGui::SameLine();
            ImGui::TextDisabled("now %.2f", mc.price[r]);

            if (hist_it != history.end())
                draw_plot("##p", hist_it->second[r].price, {-1.0f, 44.0f}, &plot_col_blue);
            else
                ImGui::TextDisabled("(no history yet)");

            ImGui::Spacing();
            ImGui::PopID();
        }
        if (!any)
            ImGui::TextDisabled("No tradeable goods at this market.");
    }
    ImGui::EndChild();

    ui::foldout_end();
}

} // namespace ui
