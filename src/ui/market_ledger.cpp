#include "market_ledger.hpp"

#include "foldout_column.hpp" // shell fold-out column host (BL-122)
#include "plot_history.hpp"
#include "presentation.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>   // std::ceil — the convoy tab's ticks-to-arrival
#include <cstdio>  // std::snprintf — the progress-bar overlay
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
//
// BL-293 (2026-08-07): the orders themselves now live in `world::sell_orders`,
// so this reads the world and ENQUEUES commands rather than mutating a UI vector.
// The press does the same thing the AI's command does because it is the same
// command — the tab composes a `corp_command` and `app::render` applies it
// through `apply_corp_command`.
void draw_sell_orders_tab(const world& w, ui_state& state, entity_id body,
                          const market_component* market)
{
    const entity_id corp = w.player_entity;

    bool any = false;
    for (const sell_order& o : w.sell_orders)
    {
        if (o.corp != corp || o.body != body)
            continue;
        any = true;
        // PushID on the ORDER ID, not the loop index: the id is what the remove
        // command names, and it is stable across the erase that a press causes.
        ImGui::PushID(static_cast<int>(o.id));
        ImGui::Text("%s  x%.0f  >= %.1f",
            resource_name(o.resource), o.quantity, o.floor_price);
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove"))
        {
            corp_command cmd;
            cmd.corp  = corp;
            cmd.verb  = corp_verb::remove_sell_order;
            cmd.order = o.id;
            state.pending_order_commands.push_back(cmd);
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
        corp_command cmd;
        cmd.corp        = corp;
        cmd.verb        = corp_verb::place_sell_order;
        cmd.subject     = body; // place_sell_order's subject is the body
        cmd.target      = static_cast<resource_type>(add_resource);
        cmd.quantity    = add_quantity;
        cmd.floor_price = add_floor;
        state.pending_order_commands.push_back(cmd);
    }
    ImGui::EndDisabled();
}

// --- Convoys (player, in flight) --------------------------------------------
// BL-453. Convoys render on three canvases — a moving beam on the Planetary
// canvas, lines on the Solar canvas, a lens glyph, an aggregated route graph —
// and were LISTED nowhere: what is in flight, carrying what, from where to
// where, arriving when, at what cost had no surface at all.
//
// The number that earns the tab is TICKS TO ARRIVAL. The travel-time model
// landed 2026-08-12 and is load-bearing — a long haul now takes several
// quarters where it used to take one, always — and no surface reported it.
//
// Deliberately NOT scoped to the ledger's selected market, unlike the Sell
// Orders tab beside it: the question is "what is on its way to me", and a
// convoy inbound to a market the player is not currently looking at is exactly
// the one they would otherwise miss. The body/market combos above are
// cross-cutting selectors (the toggle rule exempts them), so leaving them
// unread here is consistent rather than a lapse.
//
// The presses enqueue a corp_command, like the Sell Orders tab: the const
// `world&` cannot be mutated here, and routing through apply_corp_command is
// what keeps the player's Hold and an agent's `hold_convoy` the same act.

const char* convoy_mode_name(convoy_mode m)
{
    switch (m)
    {
        case convoy_mode::land:  return "Land";
        case convoy_mode::sea:   return "Sea";
        case convoy_mode::air:   return "Air";   // never dispatched in the prototype
        case convoy_mode::space: return "Space";
    }
    return "-";
}

/// Econ ticks until this convoy lands, rounded up — the figure the whole tab
/// exists for. A held convoy has no arrival time at all: it is not slowed, it
/// is stopped, so reporting a number would be a lie (hence the caller's "held").
int ticks_to_arrival(const convoy_component& c)
{
    if (c.arrived || c.progress >= 1.0f)
        return 0;
    if (!(c.speed > 0.0f))
        return -1; // unknown: a zero speed never arrives
    const float remaining = 1.0f - c.progress;
    return static_cast<int>(std::ceil(remaining / c.speed));
}

void draw_convoys_tab(const world& w, ui_state& state)
{
    const entity_id corp = w.player_entity;

    // Sorted by soonest arrival, then by id — "what lands next" is the reading
    // order the question implies, and the id tiebreak keeps it stable frame to
    // frame (world::convoys is dispatch-ordered, which is neither).
    std::vector<const convoy_component*> rows;
    for (const convoy_component& c : w.convoys)
        if (c.corp == corp && !c.arrived)
            rows.push_back(&c);
    std::sort(rows.begin(), rows.end(),
              [](const convoy_component* a, const convoy_component* b) {
                  const int ta = ticks_to_arrival(*a);
                  const int tb = ticks_to_arrival(*b);
                  if (ta != tb) return ta < tb;
                  return a->id < b->id;
              });

    if (rows.empty())
    {
        ImGui::TextDisabled("Nothing in flight.");
        return;
    }

    for (const convoy_component* cp : rows)
    {
        const convoy_component& c = *cp;
        // PushID on the CONVOY ID, not the loop index: the id is what the hold
        // command names, and it is stable across the erase an arrival causes.
        ImGui::PushID(static_cast<int>(c.id));

        const resource_presentation& rp = presentation_of(c.cargo_resource);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(rp.colour), "%s", rp.name);
        ImGui::SameLine();
        ImGui::Text("x%.0f", c.cargo_qty);
        ImGui::SameLine();
        ImGui::TextDisabled("%s -> %s  (%s)",
                            market_city_name(w, c.source_market).c_str(),
                            market_city_name(w, c.dest_market).c_str(),
                            convoy_mode_name(c.mode));

        const int eta = ticks_to_arrival(c);
        char overlay[48];
        if (c.held)
            std::snprintf(overlay, sizeof overlay, "held");
        else if (eta < 0)
            std::snprintf(overlay, sizeof overlay, "%.0f%%  (stalled)", c.progress * 100.0f);
        else
            std::snprintf(overlay, sizeof overlay, "%.0f%%  %d qtr%s",
                          c.progress * 100.0f, eta, eta == 1 ? "" : "s");
        ImGui::ProgressBar(c.progress, ImVec2{-1.0f, 0.0f}, overlay);

        ImGui::TextDisabled("Haul cost paid: %.2f", static_cast<double>(c.cost_paid));
        ImGui::SameLine();
        // The toggle rule: the press reads as the state it would move to, and
        // issuing it again undoes it — one verb, `hold_convoy`, both ways.
        if (ImGui::SmallButton(c.held ? "Release" : "Hold"))
        {
            corp_command cmd;
            cmd.corp  = corp;
            cmd.verb  = corp_verb::hold_convoy;
            cmd.order = c.id;
            state.pending_order_commands.push_back(cmd);
        }

        ImGui::Separator();
        ImGui::PopID();
    }
}

} // namespace

void draw_market_ledger(const world& w, ui_state& s,
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
    // BL-453 adds Convoys: the same chrome, the same toggle rule, no new
    // nav-rail slot — "what is on its way to me" is a market question, so it
    // belongs beside the other two rather than in a ledger of its own.
    ui::nav_button("Prices",      0, s.market_ledger_view, &open);
    ImGui::SameLine();
    ui::nav_button("Sell Orders", 1, s.market_ledger_view, &open);
    ImGui::SameLine();
    ui::nav_button("Convoys",     2, s.market_ledger_view, &open);
    ImGui::Separator();
    ImGui::Spacing();

    if (s.market_ledger_view == 1)
    {
        draw_sell_orders_tab(w, s, selected_body, &mc);
        ui::foldout_end();
        return;
    }

    if (s.market_ledger_view == 2)
    {
        draw_convoys_tab(w, s);
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
