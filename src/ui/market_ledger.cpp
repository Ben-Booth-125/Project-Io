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

namespace {

// --- Sell orders (player) ----------------------------------------------------
// Relocated from the Construction/Building panel (BL-159 — "how do I sell what
// I make?" is a market question, so it now lives on the market surface rather
// than the building surface). Columns/actions carried over faithfully from the
// old draw_sell_orders_section (construction_panel.cpp, pre-BL-159).
void draw_sell_orders_tab(const world& w, std::vector<sell_order>& orders,
                          entity_id body, const market_component* market)
{
    const entity_id corp = w.player_entity;

    bool any = false;
    for (std::size_t i = 0; i < orders.size(); ++i)
    {
        const sell_order& o = orders[i];
        if (o.corp != corp || o.body != body)
            continue;
        any = true;
        ImGui::PushID(static_cast<int>(i));
        ImGui::Text("%s  x%.0f  >= %.1f",
            resource_name(o.resource), o.quantity, o.floor_price);
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove"))
        {
            orders.erase(orders.begin() + static_cast<long>(i));
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (!any)
        ImGui::TextDisabled("No sell orders on this body.");

    if (market == nullptr)
    {
        ImGui::TextDisabled("This body has no market.");
        return;
    }

    ImGui::Separator();
    static int   add_resource = -1;
    static float add_quantity = 10.0f;
    static float add_floor    = 0.0f;

    // Reset the add-order form when the selected market's body changes — the
    // statics otherwise carry one market's resource/quantity onto another.
    static entity_id form_body = null_entity;
    if (form_body != body)
    {
        form_body    = body;
        add_resource = -1;
        add_quantity = 10.0f;
        add_floor    = 0.0f;
    }

    if (add_resource < 0 || market->base_price[static_cast<std::size_t>(add_resource)] <= 0.0f)
    {
        for (std::size_t r = 0; r < resource_count; ++r)
            if (market->base_price[r] > 0.0f) { add_resource = static_cast<int>(r); break; }
    }

    const char* preview = (add_resource >= 0)
        ? resource_name(static_cast<resource_type>(add_resource)) : "-";
    if (ImGui::BeginCombo("Resource", preview))
    {
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (market->base_price[r] <= 0.0f)
                continue;
            const bool sel = (add_resource == static_cast<int>(r));
            if (ImGui::Selectable(resource_name(static_cast<resource_type>(r)), sel))
                add_resource = static_cast<int>(r);
        }
        ImGui::EndCombo();
    }
    ImGui::InputFloat("Quantity / qtr", &add_quantity, 1.0f, 10.0f, "%.0f");
    ImGui::InputFloat("Floor price",     &add_floor,    0.1f, 1.0f,  "%.1f");
    if (add_quantity < 0.0f) add_quantity = 0.0f;
    if (add_floor    < 0.0f) add_floor    = 0.0f;

    ImGui::BeginDisabled(add_resource < 0 || add_quantity <= 0.0f);
    if (ImGui::Button("Add sell order"))
    {
        sell_order o;
        o.corp        = corp;
        o.body        = body;
        o.resource    = static_cast<resource_type>(add_resource);
        o.quantity    = add_quantity;
        o.floor_price = add_floor;
        orders.push_back(o);
    }
    ImGui::EndDisabled();
}

} // namespace

void draw_market_ledger(const world& w, ui_state& s,
                        std::vector<sell_order>& sell_orders,
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

    static entity_id selected_body        = null_entity;
    static entity_id last_seen_selection  = null_entity;

    // Selecting a market elsewhere (canvas click / Selection element) routes
    // here (BL-159): when the player's current selection is a market that
    // differs from the one we last picked up, jump the body/market selectors
    // to it. `last_seen_selection` guards this so the player can still browse
    // away with the combos afterward without being yanked back every frame.
    entity_id pending_focus_market = null_entity;
    if (s.selected_entity != last_seen_selection)
    {
        last_seen_selection = s.selected_entity;
        const auto fit = w.markets.find(s.selected_entity);
        if (fit != w.markets.end())
        {
            selected_body        = fit->second.body;
            pending_focus_market = s.selected_entity;
        }
    }

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
    if (pending_focus_market != null_entity &&
        std::find(body_markets.begin(), body_markets.end(), pending_focus_market) != body_markets.end())
    {
        selected_market = pending_focus_market;
    }
    pending_focus_market = null_entity;
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

    // --- View tabs (BL-159): Prices (the original view) / Sell Orders (relocated
    // from the Construction/Building panel — a market question belongs on the
    // market surface). Same button-strip idiom as the other split ledgers.
    ui::nav_button("Prices",      0, s.market_ledger_view, &open);
    ImGui::SameLine();
    ui::nav_button("Sell Orders", 1, s.market_ledger_view, &open);
    ImGui::Separator();
    ImGui::Spacing();

    if (s.market_ledger_view == 1)
    {
        draw_sell_orders_tab(w, sell_orders, selected_body, &mc);
        ui::foldout_end();
        return;
    }

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
