#include "input_reach.hpp"

#include "components.hpp"
#include "economy_system.hpp"  // extraction_nominal, economy_report
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
    ir.dispatch_margin  = reg.dispatch_margin();
    // Generation-side reach uses the dearest price a short destination can
    // post: the fair-price ceiling, or the price band's cap when it is OFF.
    ir.gen_price_mult   = ir.reservation_mult > 0.0f ? ir.reservation_mult
                                                     : reg.price_band().ceil_mult;
    return ir;
}

void input_reach_invalidate(input_reach& ir)
{
    ir.index_built = false;
    for (auto& v : ir.producers) v.clear();
    for (auto& v : ir.draws) v.clear();
    ir.spare_memo.clear();
    ir.supply_memo.clear();
}

namespace {

/// Labour the building is actually staffed with: assigned x its target scalar.
float labour(const building_component& b)
{
    return b.workforce_assigned * std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);
}

/// A building that can produce or draw at all: standing, complete, staffed.
bool standing(const building_component& b)
{
    return !b.decommissioned && b.ticks_remaining <= 0 && labour(b) > 0.0f;
}

/// The report row of @p bid, or null.
const building_report* row_of(const economy_report& rep, entity_id bid)
{
    if (const auto it = rep.building_row.find(bid);
        it != rep.building_row.end() && it->second < rep.buildings.size())
        return &rep.buildings[it->second];
    if (rep.building_row.empty())
        for (const building_report& br : rep.buildings)
            if (br.building == bid)
                return &br;
    return nullptr;
}

/// The share of an extraction site's basket that is resource @p r (BL-437):
/// richness over the tile's extractable richness, 0 for a spent deposit.
float extract_share(const tile_component& tc, std::size_t r)
{
    bool extractable = false;
    float total = 0.0f;
    for (const resource_type x : placement_rules::k_extractable)
    {
        const std::size_t xi = static_cast<std::size_t>(x);
        total += tc.resource_deposit[xi];
        if (xi == r) extractable = true;
    }
    if (!extractable || !(total > 0.0f) || !(tc.resource_deposit[r] > 0.0f)
        || !(tc.resource_remaining[r] > 0.0f))
        return 0.0f;
    return tc.resource_deposit[r] / total;
}

} // namespace

float building_output(const world& w, const recipe_registry& reg, entity_id bid,
                      const building_component& b, std::size_t r,
                      const economy_report* report)
{
    if (r >= resource_count || !standing(b))
        return 0.0f;
    const bool actual = report != nullptr && !report->buildings.empty();

    if (b.type == building_type::processing_facility)
    {
        const recipe* rc = reg.get_recipe(b.recipe);
        if (rc == nullptr || !(rc->outputs[r] > 0.0f))
            return 0.0f;
        if (!actual)
            return reg.economics(b.type).base_rate * labour(b) * building_supply_scalar(b)
                 * rc->outputs[r];
        const building_report* br = row_of(*report, bid);
        // Produced this tick, and produced THIS recipe (a plant switched since its
        // row makes nothing of its new good yet).
        if (br == nullptr || !br->active || br->recipe != b.recipe)
            return 0.0f;
        float total = 0.0f;
        for (const float o : rc->outputs) total += o;
        return total > 0.0f ? rc->outputs[r] * (br->output_quantity / total) : 0.0f;
    }
    if (b.type == building_type::extraction_site)
    {
        const auto tit = w.tiles.find(b.tile);
        if (tit == w.tiles.end())
            return 0.0f;
        const tile_component& tc = tit->second;
        const std::size_t pr = static_cast<std::size_t>(b.target_resource);
        // BL-1198: a Well draws no reserve and yields its target alone
        // (economy_system.cpp § run_extraction), so the reserve/share reading
        // below would call it a non-producer and hide its water from the reach.
        if (placement_rules::is_well_site(w, b.tile, b.target_resource))
        {
            if (r != pr)
                return 0.0f;
            if (!actual)
                return extraction_nominal(w, reg, b, 1.0f);
            const building_report* br = row_of(*report, bid);
            return (br != nullptr && br->active) ? br->output_quantity : 0.0f;
        }
        // A spent PRIMARY yields nothing, co-extracts included.
        if (!(tc.resource_remaining[pr] > 0.0f))
            return 0.0f;
        const float share = extract_share(tc, r);
        if (!(share > 0.0f))
            return 0.0f;
        if (!actual)
            return extraction_nominal(w, reg, b, 1.0f) * share;
        const building_report* br = row_of(*report, bid);
        if (br == nullptr || !br->active)
            return 0.0f;
        return br->output_quantity * share;
    }
    return 0.0f;
}

bool building_produces(const world& w, const recipe_registry& reg,
                       const building_component& b, std::size_t r)
{
    return building_output(w, reg, null_entity, b, r, nullptr) > 0.0f;
}

float building_draw(const recipe_registry& reg, const building_component& b, std::size_t r)
{
    if (r >= resource_count || b.type != building_type::processing_facility || !standing(b))
        return 0.0f;
    const recipe* rc = reg.get_recipe(b.recipe);
    if (rc == nullptr || !(rc->inputs[r] > 0.0f))
        return 0.0f;
    return reg.economics(b.type).base_rate * labour(b) * rc->inputs[r];
}

namespace {

void build_index(const world& w, const recipe_registry& reg, input_reach& ir)
{
    if (ir.index_built)
        return;
    input_reach_invalidate(ir);
    ir.index_built = true;
    // Ascending building id: the store is unordered, so gather then sort.
    std::vector<entity_id> ids;
    ids.reserve(w.buildings.size());
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::processing_facility
            || b.type == building_type::extraction_site)
            ids.push_back(bid);
    std::sort(ids.begin(), ids.end());

    std::array<std::map<entity_id, float>, resource_count> draw_by_market;
    for (const entity_id bid : ids)
    {
        const building_component& b = w.buildings.at(bid);
        if (!standing(b))
            continue;
        const entity_id m = market_for_tile(w, b.tile);
        if (m == null_entity)
            continue;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float o = building_output(w, reg, bid, b, r, ir.report);
            if (o > 0.0f)
                ir.producers[r].push_back({m, bid, o});
            const float d = building_draw(reg, b, r);
            if (d > 0.0f)
                draw_by_market[r][m] += d;
        }
    }
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        std::sort(ir.producers[r].begin(), ir.producers[r].end(),
                  [](const input_reach::producer& a, const input_reach::producer& b) {
                      return a.market != b.market ? a.market < b.market : a.building < b.building;
                  });
        ir.draws[r].assign(draw_by_market[r].begin(), draw_by_market[r].end());
    }
}

/// Spare output of @p r in producer market @p p: its producers' output less the
/// standing draw of every consumer market @p p is within reach of.
float spare_at(world& w, const recipe_registry& reg, input_reach& ir, entity_id p, std::size_t r)
{
    const auto key = std::make_pair(p, r);
    if (const auto it = ir.spare_memo.find(key); it != ir.spare_memo.end())
        return it->second;
    float out = 0.0f;
    for (const input_reach::producer& pr : ir.producers[r])
        if (pr.market == p) out += pr.out;
    float drawn = 0.0f;
    for (const auto& [q, d] : ir.draws[r])
        if (market_within_reach(w, reg, ir, p, q, r))
            drawn += d;
    const float spare = out - drawn;
    ir.spare_memo.emplace(key, spare);
    return spare;
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
    const auto sit = w.markets.find(src_market);
    const auto dit = w.markets.find(dst_market);
    if (sit == w.markets.end() || dit == w.markets.end())
        return false;
    const float h = input_reach_haul(w, reg, ir, src_market, dst_market);
    if (h < 0.0f)
        return false;
    // THE DISPATCHER'S OWN GATE (export_market_shelves / dispatch_convoys):
    // a unit moves from src to dst only when dst's price, net of the haul,
    // beats src's by the dispatch margin.
    //  * PLAY (a report with rows): literally that test, on each market's
    //    current resolved price (dispatch_market_price, what the dispatcher
    //    reads).
    //  * GENERATION / a hand-built world (no report): no price has resolved,
    //    and the dispatcher ships because the destination is SHORT, which
    //    lifts its price. So the destination is taken at the dearest price a
    //    draw there will pay — reservation_mult x base_dst (the price band's
    //    ceil_mult when the ceiling is OFF) — against the source's base:
    //    reservation_mult x base_dst - haul > (1 + margin) x base_src.
    // The ceiling's landed test is separate (reachable_supply).
    const bool play = ir.report != nullptr && !ir.report->buildings.empty();
    float ps = 0.0f, pd = 0.0f;
    if (play)
    {
        ps = dispatch_market_price(sit->second, r);
        pd = dispatch_market_price(dit->second, r);
    }
    else
    {
        ps = sit->second.base_price[r];
        pd = ir.gen_price_mult * dit->second.base_price[r];
    }
    if (!(pd > 0.0f) || !(pd - h > (1.0f + ir.dispatch_margin) * ps))
        return false;
    if (out_haul) *out_haul = h;
    return true;
}

reachable_spare reachable_supply(world& w, const recipe_registry& reg, input_reach& ir,
                                 entity_id consumer_market, std::size_t r, entity_id self)
{
    reachable_spare out;
    if (consumer_market == null_entity || r >= resource_count)
        return out;
    const auto cit = w.markets.find(consumer_market);
    if (cit == w.markets.end())
        return out;
    build_index(w, reg, ir);

    // The producer markets that can supply C at all — memoised per (C, r).
    const auto key = std::make_pair(consumer_market, r);
    auto mit = ir.supply_memo.find(key);
    if (mit == ir.supply_memo.end())
    {
        std::vector<input_reach::supply> list;
        entity_id last = null_entity;
        for (const input_reach::producer& pr : ir.producers[r]) // sorted by market
        {
            if (pr.market == last)
                continue;
            last = pr.market;
            float haul = 0.0f;
            if (!market_within_reach(w, reg, ir, pr.market, consumer_market, r, &haul))
                continue;
            const auto pit = w.markets.find(pr.market);
            if (pit == w.markets.end())
                continue;
            const float landed = posted_price(pit->second, r) + haul;
            if (!std::isfinite(landed))
                continue;
            // The fair-price ceiling binds this draw as it binds the shelf
            // (BL-1172): a unit that lands dearer than `reservation_mult x
            // base` at C can never be bought there.
            if (ir.reservation_mult > 0.0f && landed > ir.reservation_mult * cit->second.base_price[r])
                continue;
            list.push_back({pr.market, spare_at(w, reg, ir, pr.market, r), landed});
        }
        mit = ir.supply_memo.emplace(key, std::move(list)).first;
    }

    // The asking building is never its own supplier, and its own standing draw
    // is not a competitor for what it asks (a switching plant already counts
    // against its producers).
    float     self_out  = 0.0f;
    float     self_draw = 0.0f;
    entity_id self_mkt  = null_entity;
    if (self != null_entity)
        if (const auto sit = w.buildings.find(self); sit != w.buildings.end())
        {
            self_out  = building_output(w, reg, self, sit->second, r, ir.report);
            self_draw = building_draw(reg, sit->second, r);
            self_mkt  = market_for_tile(w, sit->second.tile);
        }
    for (const input_reach::supply& s : mit->second)
    {
        float spare = s.spare;
        if (s.market == self_mkt)
            spare -= self_out;
        if (self_draw > 0.0f && market_within_reach(w, reg, ir, s.market, self_mkt, r))
            spare += self_draw;
        if (!(spare > 0.0f))
            continue;
        out.spare += spare;
        if (out.landed < 0.0f || s.landed < out.landed)
            out.landed = s.landed;
    }
    return out;
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
    const float floor_need = need * reg.t_idle();

    // (1) STOCK — the production tick's own coverage question (run_processing:
    // pool + the shelf the fair-price ceiling admits, at the idle threshold).
    const bool  shelf = mkt && shelf_admits(*mkt, r, ir.reservation_mult, /*off_buys=*/true);
    const float avail = (pool ? std::max(0.0f, pool->quantities[r]) : 0.0f)
                      + (shelf ? std::max(0.0f, mkt->inventory[r]) : 0.0f);
    if (need <= 0.0f || avail >= floor_need)
    {
        out.obtainable = true;
        out.unit_cost  = mkt ? posted_price(*mkt, r) : 0.0f;
        return out;
    }

    // (2) SUPPLY — enough spare output within reach.
    if (allow_supply)
    {
        const reachable_spare s = reachable_supply(w, reg, ir, consumer_market, r, self);
        if (s.landed >= 0.0f && s.spare >= floor_need)
        {
            out.obtainable = true;
            out.unit_cost  = s.landed;
            return out;
        }
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
