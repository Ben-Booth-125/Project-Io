#include "site_quote.hpp"

#include "components.hpp"
#include "construction.hpp"
#include "economy_system.hpp"
#include "logistics.hpp"
#include "market_clearing.hpp"
#include "recipe_registry.hpp"
#include "world.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>

site_quote_context make_site_quote_context(world& w, const recipe_registry& reg, const economy_report* report)
{
    site_quote_context cx{make_input_reach(w, reg), pending_plant_output(w, reg)};
    // This quarter's production: a plant that made nothing supplies nothing. Without
    // a quarter to read (the first frame of a campaign) producers count at nominal.
    if (report != nullptr && !report->buildings.empty())
        cx.reach.report = report;
    return cx;
}

namespace
{
float market_price_now(const market_component& m, std::size_t r)
{
    return m.price[r] > 0.0f ? m.price[r] : m.base_price[r];
}

/// The good's forecast price over its posted price, once `added_supply` a quarter
/// joins the market and the plants under construction have joined it too. The
/// market's own law (`price_target`), so the forecast is the number clearing aims at.
/// Demand counts the want the ceiling silenced: those buyers bid again as the price
/// falls, so a good priced high because nobody makes it does not read as worthless.
/// A pooled good (power) prices against a grid this market is only part of, so it
/// keeps its post. `self` says the plants under construction already hold this one.
float price_share_after_supply(const recipe_registry& reg, const site_quote_context& cx, const market_component& m,
                               entity_id mid, std::size_t r, float added_supply, entity_id self)
{
    const float posted = market_price_now(m, r);
    if (!(posted > 0.0f) || !(m.base_price[r] > 0.0f) || grid_good_crosses_markets(r))
        return 1.0f;
    const price_band_params& band = reg.price_band();
    float pending = 0.0f;
    if (const auto it = cx.pending.find(std::make_pair(mid, r)); it != cx.pending.end())
        pending = it->second;
    if (self != null_entity)
        pending = std::max(0.0f, pending - added_supply);
    const float supply = pricing_supply(m, r, band.shelf_supply_ticks) + pending + added_supply;
    const float demand = std::max(0.0f, m.demand[r]) + std::max(0.0f, m.hauler_want[r]);
    return price_target(m.base_price[r], supply, demand, band.floor_mult, band.ceil_mult) / posted;
}

/// Does the plant's own want leave the good under the buyer's reservation? The real
/// law: its want also lets more of the shelf count as supply (`pricing_supply`).
bool own_bid_admitted(const recipe_registry& reg, const market_component& m, std::size_t r, float want)
{
    if (!(m.base_price[r] > 0.0f) || grid_good_crosses_markets(r))
        return true;
    const price_band_params& band = reg.price_band();
    const float demand = std::max(0.0f, m.demand[r]) + want;
    const float sells  = band.shelf_supply_ticks * (demand + std::max(0.0f, m.hauler_want[r]));
    const float supply = std::max(0.0f, m.supply[r]) + std::min(std::max(0.0f, m.inventory[r]), sells);
    return price_target(m.base_price[r], supply, demand, band.floor_mult, band.ceil_mult)
           <= band.reservation_mult * m.base_price[r];
}

/// The market as it would read without the plant @p self's last quarter: exactly what
/// its report row says it listed and filed, by the recipe THE ROW ran, since a Switch
/// writes the building at once and the market still holds the old recipe's trade. Its
/// goods already on the shelf stay: they are stock. Nothing to take out without a row.
/// Processing only: the lever is the one caller, and a mine's co-extracted goods are not split.
std::optional<market_component> market_without_plant(const recipe_registry& reg, const site_quote_context& cx,
                                                     const market_component& m, entity_id self,
                                                     const building_component& b)
{
    if (b.type != building_type::processing_facility)
        return std::nullopt;
    if (cx.reach.report == nullptr)
        return std::nullopt;
    const auto& rows = cx.reach.report->buildings;
    const auto row = std::find_if(rows.begin(), rows.end(),
                                  [&](const building_report& br) { return br.building == self; });
    if (row == rows.end())
        return std::nullopt;

    market_component out = m;
    const auto take = [](float& v, float amount) { v = std::max(0.0f, v - amount); };
    const recipe* ran = reg.get_recipe(row->recipe);
    float output_units = 0.0f;
    if (ran != nullptr)
        for (std::size_t r = 0; r < resource_count; ++r)
            output_units += ran->outputs[r];
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (output_units > 0.0f && ran->outputs[r] > 0.0f)
            take(out.supply[r], row->output_quantity * ran->outputs[r] / output_units);
        take(out.demand[r], row->wanted[r]);
        take(out.hauler_want[r], row->suppressed[r]);
    }
    return out;
}
} // namespace

site_quote quote_site(world& w, const recipe_registry& reg, site_quote_context& cx,
                      entity_id tile, building_type type, resource_type target,
                      std::uint16_t recipe_id, float lp_cover, entity_id self,
                      const building_component* running)
{
    site_quote q;
    const entity_id mid = market_for_tile(w, tile);
    const auto mit = (mid != null_entity) ? w.markets.find(mid) : w.markets.end();
    const recipe* rc = (type == building_type::processing_facility) ? reg.get_recipe(recipe_id) : nullptr;
    if (mit == w.markets.end() || (type == building_type::processing_facility && rc == nullptr))
    {
        q.profit = estimate_prospective_profit(w, reg, tile, type, target, recipe_id, running, nullptr, lp_cover);
        return q;
    }
    const std::optional<market_component> netted =
        running != nullptr ? market_without_plant(reg, cx, mit->second, self, *running) : std::nullopt;
    const market_component& m = netted.has_value() ? *netted : mit->second;
    // `self` tells the forecast a plant under construction is already in the pending
    // pool. A running plant is not in it, so it must not be taken back out of it.
    const entity_id pending_self = running != nullptr ? null_entity : self;

    if (rc != nullptr)
    {
        building_component bc{};
        bc.tile               = tile;
        bc.type               = type;
        bc.recipe             = recipe_id;
        bc.workforce_assigned = construction_initial_workforce(type);
        bc.workforce_target   = default_workforce_target(type);
        if (running != nullptr)
        {
            bc.workforce_assigned     = running->workforce_assigned;
            bc.workforce_target       = running->workforce_target;
            bc.supply_factor_permille = running->supply_factor_permille;
            bc.condition_permille     = running->condition_permille;
        }
        float batches = judged_batches(reg, bc) * std::clamp(lp_cover, 0.0f, 1.0f);
        if (running != nullptr)
            batches *= building_supply_scalar(bc);

        // Can it run? Measured against what ordered plants went on to earn (the
        // build_payback_census holdout, 2026-11-06, 549 processors): stock or supply, with
        // the plant's own bid staying under the buyer's reservation, called 58 idle and
        // 7 of those ran. The scorer's stricter test for a new plant, spare supply alone,
        // called 131 idle and 95 ran, because a plant bids goods away from other buyers.
        // That test decides whether a rival builds; this one says what a plant would do.
        std::array<float, resource_count> cost{};
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            cost[r] = market_price_now(m, r);
            if (rc->inputs[r] <= 0.0f)
                continue;
            const float want = rc->inputs[r] * batches;
            float landed = -1.0f;
            bool have = input_supply_covers(w, reg, cx.reach, mid, r, want, self, &landed) && landed >= 0.0f;
            if (!have)
            {
                const input_access access = input_obtainable(w, reg, cx.reach, mid, nullptr, r, want, self);
                have   = access.obtainable;
                landed = access.unit_cost;
            }
            const bool supplied = have
                               && shelf_admits(m, r, reg.price_band().reservation_mult, /*off_buys=*/true)
                               && own_bid_admitted(reg, m, r, want);
            if (supplied)
                cost[r] = landed;
            else if (!q.starved)
            {
                q.starved  = true;
                q.short_of = static_cast<resource_type>(r);
            }
        }
        q.profit = estimate_prospective_profit(w, reg, tile, type, target, recipe_id, running, &cost, lp_cover);
        if (q.starved)
        {
            // It would stand idle: upkeep and nothing else.
            q.profit.revenue    = 0.0f;
            q.profit.input_cost = 0.0f;
            q.price_share       = 0.0f;
            return q;
        }
        // Weighted over the outputs by what each brings in at the post.
        float after = 0.0f, before = 0.0f;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (rc->outputs[r] <= 0.0f)
                continue;
            const float v = rc->outputs[r] * market_price_now(m, r);
            after  += v * price_share_after_supply(reg, cx, m, mid, r, rc->outputs[r] * batches, pending_self);
            before += v;
        }
        q.price_share = before > 0.0f ? after / before : 1.0f;
        q.profit.revenue *= q.price_share;
        return q;
    }

    q.profit = estimate_prospective_profit(w, reg, tile, type, target, recipe_id, running, nullptr, lp_cover);
    if (type == building_type::extraction_site)
    {
        const std::size_t r = static_cast<std::size_t>(target);
        const float p = market_price_now(m, r);
        const float rate = p > 0.0f ? q.profit.revenue / p : 0.0f;
        q.price_share = price_share_after_supply(reg, cx, m, mid, r, rate, pending_self);
        q.profit.revenue *= q.price_share;
    }
    return q;
}
