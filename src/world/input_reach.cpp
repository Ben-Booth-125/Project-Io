#include "input_reach.hpp"

#include "components.hpp"
#include "economy_system.hpp"  // extraction_nominal, economy_report
#include "market_clearing.hpp" // market_for_tile
#include "placement_rules.hpp" // k_extractable
#include "recipe_registry.hpp"
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <set>

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
    ir.supply_memo.clear();
    ir.refresh_mode = false;
    ir.seen.clear();
    for (auto& m : ir.draw_parts) m.clear();
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
        // BL-1198/BL-1199: a Well or Wharf draws no reserve and yields its
        // target alone (economy_system.cpp § run_extraction), so the
        // reserve/share reading below would call it a non-producer and hide
        // its output from the reach.
        if (placement_rules::is_depositless_site(w, b.tile, b.target_resource))
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

bool producer_less(const input_reach::producer& a, const input_reach::producer& b)
{
    return a.market != b.market ? a.market < b.market : a.building < b.building;
}

template <typename F>
void contribute(const world& w, const recipe_registry& reg, const input_reach& ir, entity_id bid,
                const building_component& b, entity_id& m, F&& emit);

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
        entity_id m = null_entity;
        contribute(w, reg, ir, bid, b, m, [&](std::size_t r, float o, float d) {
            if (o > 0.0f)
                ir.producers[r].push_back({m, bid, o});
            if (d > 0.0f)
                draw_by_market[r][m] += d;
        });
    }
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        std::sort(ir.producers[r].begin(), ir.producers[r].end(), producer_less);
        ir.draws[r].assign(draw_by_market[r].begin(), draw_by_market[r].end());
    }
}

/// THE ONE READING of what a building contributes to the index: for each good it
/// can make or take, its output and draw, in ascending r. @p m receives its
/// market (null: none — it contributes nothing).
template <typename F>
void contribute(const world& w, const recipe_registry& reg, const input_reach& ir, entity_id bid,
                const building_component& b, entity_id& m, F&& emit)
{
    m = null_entity;
    {
        if (!standing(b))
            return;
        m = market_for_tile(w, b.tile);
        if (m == null_entity)
            return;
        // BL-1205 (scorer cost at density): only the goods this building CAN
        // make or take are asked — every other r answers 0 from both functions
        // below, so skipping it changes nothing but the time. A processor makes
        // its recipe's outputs and takes its inputs; an extraction site makes
        // its target (a Well) or a deposit on its tile (the BL-437 share) and
        // takes nothing. Per r the walk is still ascending building id, so every
        // producer list and every draw sum is built in the order it was.
        const recipe*         rc      = (b.type == building_type::processing_facility)
                                            ? reg.get_recipe(b.recipe) : nullptr;
        const tile_component* site_tc = nullptr;
        if (b.type == building_type::extraction_site)
            if (const auto tit = w.tiles.find(b.tile); tit != w.tiles.end())
                site_tc = &tit->second;
        if (b.type == building_type::processing_facility && rc == nullptr)
            return; // no recipe: neither makes nor takes anything
        if (b.type == building_type::extraction_site && site_tc == nullptr)
            return; // no tile: building_output answers 0 for every r
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const bool may_make = (rc != nullptr)
                ? (rc->outputs[r] > 0.0f)
                : (r == static_cast<std::size_t>(b.target_resource)
                   || site_tc->resource_deposit[r] > 0.0f);
            const bool may_take = rc != nullptr && rc->inputs[r] > 0.0f;
            if (!may_make && !may_take)
                continue;
            const float o = building_output(w, reg, bid, b, r, ir.report);
            const float d = building_draw(reg, b, r);
            if (o > 0.0f || d > 0.0f)
                emit(r, o, d);
        }
    }
}


input_reach::building_sig sig_of(const building_component& b)
{
    return { b.type, b.recipe, b.tile, b.target_resource, b.workforce_assigned,
             building_supply_scalar(b), b.workforce_target, b.ticks_remaining, b.decommissioned };
}

} // namespace

void input_reach_refresh(const world& w, const recipe_registry& reg, input_reach& ir)
{
    if (!ir.refresh_mode)
    {
        input_reach_invalidate(ir); // a full build's index is not tracked: start empty
        ir.refresh_mode = true;
        ir.index_built  = true;
    }
    // What changed, in ascending id (the stores are unordered).
    std::vector<entity_id> gone, added;
    for (const auto& [bid, ib] : ir.seen)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end() || !(sig_of(bit->second) == ib.sig))
            gone.push_back(bid);
    }
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::processing_facility && b.type != building_type::extraction_site)
            continue;
        const auto sit = ir.seen.find(bid);
        if (sit == ir.seen.end() || !(sit->second.sig == sig_of(b)))
            added.push_back(bid);
    }
    if (gone.empty() && added.empty())
        return;
    std::sort(gone.begin(), gone.end());
    std::sort(added.begin(), added.end());
    ir.supply_memo.clear();

    std::set<std::pair<std::size_t, entity_id>> touched; // (r, market) whose draw sum moves
    for (const entity_id bid : gone)
    {
        const input_reach::indexed_building& ib = ir.seen.at(bid);
        for (const auto& [r, o] : ib.out)
        {
            auto& v = ir.producers[r];
            const input_reach::producer key{ib.market, bid, o};
            const auto it = std::lower_bound(v.begin(), v.end(), key, producer_less);
            if (it != v.end() && it->market == ib.market && it->building == bid)
                v.erase(it);
        }
        for (const auto& [r, d] : ib.draw)
        {
            (void)d;
            auto& parts = ir.draw_parts[r][ib.market];
            parts.erase(bid);
            touched.insert({r, ib.market});
        }
        ir.seen.erase(bid);
    }
    for (const entity_id bid : added)
    {
        const building_component& b = w.buildings.at(bid);
        input_reach::indexed_building ib;
        ib.sig = sig_of(b);
        entity_id m = null_entity;
        contribute(w, reg, ir, bid, b, m, [&](std::size_t r, float o, float d) {
            if (o > 0.0f)
            {
                auto& v = ir.producers[r];
                const input_reach::producer p{m, bid, o};
                v.insert(std::lower_bound(v.begin(), v.end(), p, producer_less), p);
                ib.out.push_back({r, o});
            }
            if (d > 0.0f)
            {
                ir.draw_parts[r][m][bid] = d;
                ib.draw.push_back({r, d});
                touched.insert({r, m});
            }
        });
        ib.market = m;
        ir.seen[bid] = std::move(ib);
    }
    // Each touched market's draw, re-added in ascending building id.
    for (const auto& [r, m] : touched)
    {
        auto& v  = ir.draws[r];
        auto  it = std::lower_bound(v.begin(), v.end(), m,
                                    [](const std::pair<entity_id, float>& a, entity_id k) {
                                        return a.first < k;
                                    });
        const auto pit = ir.draw_parts[r].find(m);
        if (pit == ir.draw_parts[r].end() || pit->second.empty())
        {
            if (pit != ir.draw_parts[r].end())
                ir.draw_parts[r].erase(pit);
            if (it != v.end() && it->first == m)
                v.erase(it);
            continue;
        }
        float sum = 0.0f;
        for (const auto& [bid, d] : pit->second) { (void)bid; sum += d; }
        if (it != v.end() && it->first == m)
            it->second = sum;
        else
            v.insert(it, {m, sum});
    }
}

namespace {
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

    // C's REACH SET — memoised per (C, r): the producer markets within reach of
    // C landing under the ceiling, and every consumer market any of them reaches
    // with the set markets that reach it (see SPARE OUTPUT, input_reach.hpp).
    const auto key = std::make_pair(consumer_market, r);
    auto mit = ir.supply_memo.find(key);
    if (mit == ir.supply_memo.end())
    {
        input_reach::reach_set set;
        for (const input_reach::producer& pr : ir.producers[r]) // sorted by market
        {
            if (!set.markets.empty() && set.markets.back().market == pr.market)
            {
                set.markets.back().out += pr.out; // a market already admitted
                continue;
            }
            // A market refused below is asked again for its next producer; the
            // answers are memoised (haul) and identical, so it is refused again.
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
            set.markets.push_back({pr.market, pr.out, landed});
        }
        for (const auto& [q, d] : ir.draws[r]) // ascending market
        {
            input_reach::reached_draw rd{q, d, {}};
            for (std::size_t i = 0; i < set.markets.size(); ++i)
                if (market_within_reach(w, reg, ir, set.markets[i].market, q, r))
                    rd.reachers.push_back(static_cast<int>(i));
            if (!rd.reachers.empty())
                set.draws.push_back(std::move(rd));
        }
        mit = ir.supply_memo.emplace(key, std::move(set)).first;
    }
    const input_reach::reach_set& set = mit->second;

    // The asking building is never its own supplier, and its own standing draw
    // is not a competitor for what it asks (a switching plant already counts
    // against its producers) — each taken out once.
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
    // Each market's output with the asker's taken out; then each reached
    // consumer's charge, min(its draw, what its reachers make), shared among its
    // reachers by output. A market's spare is its output less its shares.
    std::vector<float> outp(set.markets.size());
    for (std::size_t i = 0; i < set.markets.size(); ++i)
        outp[i] = std::max(0.0f, set.markets[i].out
                                     - (set.markets[i].market == self_mkt ? self_out : 0.0f));
    std::vector<float> spare_p = outp;
    for (const input_reach::reached_draw& rd : set.draws)
    {
        const float d = std::max(0.0f, rd.draw - (rd.market == self_mkt ? self_draw : 0.0f));
        if (!(d > 0.0f))
            continue;
        float feed = 0.0f;
        for (const int i : rd.reachers) feed += outp[static_cast<std::size_t>(i)];
        if (!(feed > 0.0f))
            continue;
        // Approximation, accepted as conservative (review round 3): each consumer
        // is bounded by its own reachers' output, but several consumers sharing
        // one producer can together charge it past its output.
        const float charge = std::min(d, feed);
        for (const int i : rd.reachers)
            spare_p[static_cast<std::size_t>(i)] -= charge * (outp[static_cast<std::size_t>(i)] / feed);
    }
    float spare = 0.0f;
    for (std::size_t i = 0; i < set.markets.size(); ++i)
        spare += spare_p[i];
    out.spare = spare;
    if (!(spare > 0.0f))
        return out;
    for (std::size_t i = 0; i < set.markets.size(); ++i)
    {
        if (!(spare_p[i] > 0.0f))
            continue; // fully drawn: it quotes no price
        if (out.landed < 0.0f || set.markets[i].landed < out.landed)
            out.landed = set.markets[i].landed;
    }
    return out;
}

bool building_makes(const world& w, const recipe_registry& reg, entity_id bid, std::size_t r)
{
    if (r >= resource_count)
        return false;
    const auto bit = w.buildings.find(bid);
    if (bit == w.buildings.end())
        return false;
    const building_component& b = bit->second;
    if (b.type == building_type::processing_facility)
    {
        const recipe* rc = reg.get_recipe(b.recipe);
        return rc != nullptr && rc->outputs[r] > 0.0f;
    }
    if (b.type == building_type::extraction_site)
    {
        if (static_cast<std::size_t>(b.target_resource) == r)
            return true;
        const auto tit = w.tiles.find(b.tile);
        return tit != w.tiles.end() && extract_share(tit->second, r) > 0.0f;
    }
    return false;
}

float judged_batches(const recipe_registry& reg, const building_component& b)
{
    const float rate = reg.economics(b.type).base_rate;
    const float l    = labour(b);
    if (l > 0.0f)
        return rate * l;
    // Unstaffed (no workforce assigned, or a target dialled to zero): judge it
    // at the staffing a placed building is authored with — half its workforce
    // assigned (author_building / construct_building) at the nominal 100 %
    // target — so its need is real rather than zero.
    constexpr float k_authored_assignment = 0.5f;
    return rate * k_authored_assignment;
}

bool input_supply_covers(world& w, const recipe_registry& reg, input_reach& ir,
                         entity_id consumer_market, std::size_t r, float need, entity_id self,
                         float* out_landed)
{
    if (r >= resource_count)
        return false;
    if (need <= 0.0f)
        return true;
    const reachable_spare s = reachable_supply(w, reg, ir, consumer_market, r, self);
    if (out_landed) *out_landed = s.landed;
    return s.landed >= 0.0f && s.spare >= need * reg.t_idle();
}

bool recipe_inputs_supplied(world& w, const recipe_registry& reg, input_reach& ir,
                            entity_id consumer_market, const recipe& rc, float batches,
                            entity_id self)
{
    for (std::size_t r = 0; r < resource_count; ++r)
        if (rc.inputs[r] > 0.0f
            && !input_supply_covers(w, reg, ir, consumer_market, r, rc.inputs[r] * batches, self))
            return false;
    return true;
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
    //
    // BL-1206 (cold review of the rescue gate): THE ASKER'S OWN OUTPUT IS NOT
    // ITS OWN STOCK COVER. A building that makes r and asks whether it could run
    // a recipe eating r sees, in its pool and on its shelf, the r it just made —
    // stock that stops arriving the tick it switches (seed 40: a refined-fuel
    // plant moved onto propellant, fed one tick by its own leftovers, starved
    // from the next). The stock clause cannot tell its units from a third
    // party's, so for such an input it is not asked: the SUPPLY clause decides,
    // and it already takes the asker's own output out.
    const bool  own_good = self != null_entity && building_makes(w, reg, self, r);
    const bool  shelf = mkt && shelf_admits(*mkt, r, ir.reservation_mult, /*off_buys=*/true);
    const float avail = own_good ? 0.0f
                      : (pool ? std::max(0.0f, pool->quantities[r]) : 0.0f)
                      + (shelf ? std::max(0.0f, mkt->inventory[r]) : 0.0f);
    if (need <= 0.0f || (!own_good && avail >= floor_need))
    {
        out.obtainable = true;
        out.unit_cost  = mkt ? posted_price(*mkt, r) : 0.0f;
        return out;
    }

    // (2) SUPPLY — enough spare output within reach.
    if (allow_supply)
    {
        float landed = -1.0f;
        if (input_supply_covers(w, reg, ir, consumer_market, r, need, self, &landed))
        {
            out.obtainable = true;
            out.unit_cost  = landed;
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
