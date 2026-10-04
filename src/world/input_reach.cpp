#include "input_reach.hpp"

#include "components.hpp"
#include "economy_system.hpp"  // extraction_nominal
#include "market_clearing.hpp" // market_for_tile
#include "placement_rules.hpp" // k_extractable
#include "recipe_registry.hpp"
#include "world.hpp"

#include <algorithm>
#include <cmath>

input_reach make_input_reach(const world& w, const recipe_registry& reg)
{
    input_reach ir;
    ir.nodes            = collect_logistics_nodes(w);
    ir.reservation_mult = reg.price_band().reservation_mult;
    ir.headroom         = ir.reservation_mult - 1.0f - reg.dispatch_margin();
    return ir;
}

bool building_produces(const world& w, const recipe_registry& reg,
                       const building_component& b, std::size_t r)
{
    if (b.decommissioned || r >= resource_count)
        return false;
    if (b.type == building_type::processing_facility)
    {
        const recipe* rc = reg.get_recipe(b.recipe);
        return rc != nullptr && rc->outputs[r] > 0.0f;
    }
    if (b.type != building_type::extraction_site)
        return false;
    // A site that cannot draw its primary yields nothing at all (run_extraction
    // returns before the co-extraction basket).
    if (!(extraction_nominal(w, reg, b, 1.0f) > 0.0f))
        return false;
    const auto tit = w.tiles.find(b.tile);
    if (tit == w.tiles.end())
        return false;
    const tile_component& tc = tit->second;
    // BL-437: the site works every extractable deposit on its tile.
    for (const resource_type x : placement_rules::k_extractable)
        if (static_cast<std::size_t>(x) == r)
            return tc.resource_deposit[r] > 0.0f && tc.resource_remaining[r] > 0.0f;
    return false;
}

namespace {

void build_producer_index(const world& w, const recipe_registry& reg, input_reach& ir)
{
    if (ir.producers_built)
        return;
    ir.producers_built = true;
    for (auto& v : ir.producers)
        v.clear();
    // Ascending building id: the store is unordered, so gather then sort.
    std::vector<entity_id> ids;
    ids.reserve(w.buildings.size());
    for (const auto& [bid, b] : w.buildings)
    {
        (void)b;
        ids.push_back(bid);
    }
    std::sort(ids.begin(), ids.end());
    // RUNNING EVIDENCE (play only): when the caller hands in this tick's
    // economy_report and it carries rows, a PROCESSOR counts only if it
    // produced this tick — a starved mill is not a supplier, and counting it
    // let a consumer-goods plant resume on steel nobody was making. An
    // extraction site needs no inputs, so it counts while it stands (under
    // construction included). With no report, or an empty one (generation, a
    // hand-built harness world), every standing producer counts: BL-1185's
    // reading, where nothing has run yet.
    const economy_report* rep =
        (ir.report != nullptr && !ir.report->buildings.empty()) ? ir.report : nullptr;
    const auto produced_this_tick = [&](entity_id bid) -> bool {
        if (const auto it = rep->building_row.find(bid);
            it != rep->building_row.end() && it->second < rep->buildings.size())
            return rep->buildings[it->second].active;
        if (rep->building_row.empty())
            for (const building_report& br : rep->buildings)
                if (br.building == bid)
                    return br.active;
        return false;
    };
    for (const entity_id bid : ids)
    {
        const building_component& b = w.buildings.at(bid);
        if (b.decommissioned)
            continue;
        if (b.type != building_type::processing_facility
            && b.type != building_type::extraction_site)
            continue;
        if (rep != nullptr && b.type == building_type::processing_facility
            && !produced_this_tick(bid))
            continue;
        const entity_id mp = market_for_tile(w, b.tile);
        if (mp == null_entity)
            continue;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (building_produces(w, reg, b, r))
                ir.producers[r].emplace_back(mp, bid);
    }
    for (auto& v : ir.producers)
        std::sort(v.begin(), v.end());
}

} // namespace

float input_reach_haul(world& w, const recipe_registry& reg, input_reach& ir,
                       entity_id src_market, entity_id dst_market)
{
    if (src_market == dst_market)
        return 0.0f;
    const auto key = std::make_pair(src_market, dst_market);
    if (const auto it = ir.haul.find(key); it != ir.haul.end())
        return it->second;
    const convoy_leg leg = price_market_export_leg(w, reg, ir.nodes, src_market, dst_market, 1.0f);
    const float h = (leg.viable && std::isfinite(leg.cost) && leg.cost >= 0.0f) ? leg.cost : -1.0f;
    ir.haul.emplace(key, h);
    return h;
}

bool market_within_reach(world& w, const recipe_registry& reg, input_reach& ir,
                         entity_id src_market, entity_id dst_market, std::size_t r,
                         float* out_haul)
{
    if (src_market == null_entity || dst_market == null_entity || r >= resource_count)
        return false;
    if (src_market == dst_market)
    {
        if (out_haul) *out_haul = 0.0f;
        return true;
    }
    if (reg.grid_goods().grid(r)) // BL-708: a grid good is never cargo
        return false;
    const auto dit = w.markets.find(dst_market);
    if (dit == w.markets.end())
        return false;
    const float base = dit->second.base_price[r];
    if (!(base > 0.0f))
        return false;
    const float bound = ir.headroom * base;
    if (!(bound >= 0.0f))
        return false;
    const float h = input_reach_haul(w, reg, ir, src_market, dst_market);
    if (h < 0.0f || h > bound)
        return false;
    if (out_haul) *out_haul = h;
    return true;
}

float reachable_supply_cost(world& w, const recipe_registry& reg, input_reach& ir,
                            entity_id consumer_market, std::size_t r, entity_id self)
{
    if (consumer_market == null_entity || r >= resource_count)
        return -1.0f;
    build_producer_index(w, reg, ir);
    const auto cit = w.markets.find(consumer_market);
    if (cit == w.markets.end())
        return -1.0f;

    float     best      = -1.0f;
    entity_id last_mkt  = null_entity;
    bool      last_seen = false;
    // Sorted (market, building): each market is priced once, at its first
    // producer that is not `self`.
    for (const auto& [mp, bid] : ir.producers[r])
    {
        if (bid == self)
            continue;
        if (last_seen && mp == last_mkt)
            continue;
        last_mkt  = mp;
        last_seen = true;
        float haul = 0.0f;
        if (!market_within_reach(w, reg, ir, mp, consumer_market, r, &haul))
            continue;
        const auto pit = w.markets.find(mp);
        if (pit == w.markets.end())
            continue;
        const float landed = posted_price(pit->second, r) + haul;
        if (!std::isfinite(landed))
            continue;
        // The fair-price ceiling binds this draw as it binds the shelf
        // (BL-1172): a unit that lands dearer than `reservation_mult x base`
        // at the consumer can never be bought there. At base prices this is
        // the reach bound itself (base + 0.95 base < 2 base); it bites only
        // where the producer's own market is priced over the ceiling — in
        // the consumer's own market, exactly when its shelf is closed.
        if (ir.reservation_mult > 0.0f && landed > ir.reservation_mult * cit->second.base_price[r])
            continue;
        if (best < 0.0f || landed < best)
            best = landed;
    }
    return best;
}

input_access input_obtainable(world& w, const recipe_registry& reg, input_reach& ir,
                              entity_id consumer_market, const stockpile_component* pool,
                              std::size_t r, float need, entity_id self,
                              bool allow_supply)
{
    input_access out;
    if (r >= resource_count)
        return out;
    const auto mit = (consumer_market != null_entity) ? w.markets.find(consumer_market)
                                                      : w.markets.end();
    const market_component* mkt = (mit != w.markets.end()) ? &mit->second : nullptr;

    // (1) STOCK — the production tick's own coverage question (run_processing:
    // pool + the shelf the fair-price ceiling admits, at the idle threshold).
    const bool  shelf = mkt && shelf_admits(*mkt, r, ir.reservation_mult, /*off_buys=*/true);
    const float avail = (pool ? std::max(0.0f, pool->quantities[r]) : 0.0f)
                      + (shelf ? std::max(0.0f, mkt->inventory[r]) : 0.0f);
    if (need <= 0.0f || avail / need >= reg.t_idle())
    {
        out.obtainable = true;
        out.unit_cost  = mkt ? posted_price(*mkt, r) : 0.0f;
        return out;
    }

    // (2) SUPPLY — a producer within reach.
    const float landed = allow_supply
        ? reachable_supply_cost(w, reg, ir, consumer_market, r, self)
        : -1.0f;
    if (landed >= 0.0f)
    {
        out.obtainable = true;
        out.unit_cost  = landed;
        return out;
    }
    out.unit_cost = mkt ? posted_price(*mkt, r) : 0.0f;
    return out;
}

bool recipe_inputs_obtainable(world& w, const recipe_registry& reg, input_reach& ir,
                              entity_id consumer_market, const stockpile_component* pool,
                              const recipe& rc, float batches, entity_id self,
                              std::array<float, resource_count>& unit_cost,
                              bool allow_supply)
{
    bool all = true;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        unit_cost[r] = 0.0f;
        const float in = rc.inputs[r];
        if (!(in > 0.0f))
            continue;
        const input_access a = input_obtainable(w, reg, ir, consumer_market, pool, r,
                                                in * batches, self, allow_supply);
        unit_cost[r] = a.unit_cost;
        if (!a.obtainable)
            all = false;
    }
    return all;
}
