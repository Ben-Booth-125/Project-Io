#include "budget_system.hpp"

#include "law.hpp"         // BL-343: evaluate_laws at the enforcement seam
#include "economy_system.hpp" // BL-1335: corp_budget, nation_tax_due
#include "nation_tech.hpp"    // BL-1335: research_age_open (no tax in the settle)
#include "unit_roster.hpp" // BL-454: resolve_unit_upkeep (the credit half)

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <tuple>
#include <vector>

namespace {

/// body -> its population centres in ASCENDING ID ORDER, built once and kept on
/// the world (`world::body_centre_index`, declared there with the staleness
/// rule). The same shape and the same stamp `markets_by_body` (market_clearing.cpp)
/// uses for markets.
///
/// WHY IT IS AN INDEX AND NOT A LOOP (BL-1050 cold review). `body_mean_habitability`
/// below is called per building per tick (economy_system.cpp), per building AND
/// per build candidate (building_profit.cpp), by the corp AI and by two UI panels
/// — and every one of those calls used to walk EVERY centre in the world and hash
/// twice per centre to keep the few on one body. budget_system's own `hab_cache`
/// exists to blunt exactly that at the one call site that could. The index pays
/// the walk once per world instead of once per call, and it hands back the sorted
/// order the sum needs, so the sort is paid once too.
const faithful_unordered_map<entity_id, std::vector<entity_id>>& centres_by_body(const world& w)
{
    // THE STAMP IS O(1), and it has to be: `markets_by_body` re-derives its max
    // id by walking all 136 markets per call, which is affordable there and is
    // the whole cost being removed here (5417 centres on seed 28). So the stamp
    // is the centre count plus the ENTITY-ALLOCATOR CURSOR — monotonic, and it
    // moves whenever any entity anywhere is created. Count alone would already
    // be sound today (centres are written once by generate_population_centres
    // and nothing erases one — a razed centre keeps its entry), but an erase
    // paired with an insert would slip past it, and the cursor closes that
    // without a walk: an insert cannot happen without the cursor moving.
    const std::size_t   n      = w.population_centres.size();
    const std::uint32_t cursor = w.next_entity_id();

    if (w.body_centre_index_count != n || w.body_centre_index_cursor != cursor)
    {
        auto& map = w.body_centre_index;
        map.clear();
        for (const auto& [cid, pcc] : w.population_centres)
        {
            (void)pcc;
            const auto tile_it = w.population_centre_tile.find(cid);
            if (tile_it == w.population_centre_tile.end())
                continue;
            const auto tc_it = w.tiles.find(tile_it->second);
            if (tc_it == w.tiles.end())
                continue;
            map[tc_it->second.body].push_back(cid);
        }
        // The one line the determinism rests on: whatever order the walk above
        // reached them in, each body's list comes out ascending.
        for (auto& [b, ids] : map)
        {
            (void)b;
            std::sort(ids.begin(), ids.end());
        }
        w.body_centre_index_count  = n;
        w.body_centre_index_cursor = cursor;
    }
    return w.body_centre_index;
}

} // namespace

float body_mean_habitability(const world& w, entity_id body)
{
    // Mirrors the accumulation apply_budget's batch used, filtered to one body, so
    // the estimate and the live budget loop read an identical mean (bit-for-bit).
    //
    // ASCENDING CENTRE ID (BL-1050). This is a float sum and float addition does
    // not associate, so the summation order must be a property of the ids and
    // never of `population_centres`' bucket layout: a save/load re-inserts that
    // store in id order (world_save.cpp) and lays it out differently, and another
    // standard library lays it out differently again — so the unordered walk that
    // was here made a LOADED world tick differently from the one it was saved
    // from. BL-1034's faithful_unordered_map keeps a COPY in its source's order
    // and stays as the tripwire; it never was the fix for a load or a port.
    // A reordered read moved one body's mean two ULP on seed 28 and every wage
    // built on it with it, which is exactly how much this is worth.
    //
    // The order arrives from the index above; the arithmetic here is unchanged.
    const auto& by_body = centres_by_body(w);
    const auto  it      = by_body.find(body);
    if (it == by_body.end() || it->second.empty())
        return 1.0f;

    float sum = 0.0f;
    for (const entity_id cid : it->second)
        sum += w.population_centres.at(cid).habitability;
    return sum / static_cast<float>(it->second.size());
}

building_opex compute_building_opex(const building_component& b,
                                    const building_economics& e,
                                    float contention_scalar,
                                    float mean_hab,
                                    float idle_floor)
{
    // Labour/material cost split (BL-049).
    // Material cost: the authored idle floor (BL-739 — was a hard-coded 30%),
    // charged even when decommissioned.
    // Labour cost: scales with workforce_target; zero when decommissioned.
    //
    // BL-1183 C4 (Ben, 2026-10-05; FINANCE.md � Building operating cost): a site
    // UNDER CONSTRUCTION (`ticks_remaining > 0`, the one flag every pass reads as
    // "not yet built") is charged exactly what a decommissioned building is � the
    // idle floor, no labour maintenance, no wages � until the tick it completes.
    // Labour demand already skips a site (economy_system.cpp, the labour claims),
    // so a wage here paid a crew nobody supplied. One test, here, so the budget
    // loop and every estimate that asks this function agree.
    const bool  idle          = b.decommissioned || b.ticks_remaining > 0;
    const float wt_scalar     = std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);
    const float material_cost = e.maintenance * idle_floor;
    // Construction is measured in days (Ben, 2026-10-24; PRODUCTION.md): a building
    // that completed mid-quarter pays labour maintenance and wages this tick for the
    // share of the quarter it stood complete; the material floor, which a site pays
    // too, stays whole. Exactly 1.0f for every other building.
    const float share         = building_output_share(b);
    const float labour_cost   = idle ? 0.0f
                                : (e.maintenance * wt_scalar - material_cost) * share;
    const float hab           = std::clamp(mean_hab, 0.1f, 2.0f);

    building_opex o;
    // Guard against negative labour_cost when wt_scalar < idle_floor (the
    // material floor already covers more than the scaled total in that edge case).
    o.maintenance = material_cost + std::max(0.0f, labour_cost);
    // BL-614: wages are paid AT THE OFFERED RATE — base_wage × (1 + wage_bid) —
    // on the labour actually allocated. A zero bid multiplies by exactly 1.0f,
    // so every pre-BL-614 wage is bit-identical.
    o.wages       = idle
                    ? 0.0f
                    : b.workforce_assigned * contention_scalar * e.base_wage
                          * (1.0f + b.wage_bid) * wt_scalar * hab * share;
    return o;
}

void apply_budget(world& w,
                  const recipe_registry& reg,
                  const std::unordered_map<entity_id, corp_cash_flow>& flows,
                  const std::map<std::pair<entity_id, entity_id>, float>& contention,
                  std::map<entity_id, corp_budget>* breakdown,
                  const std::vector<building_report>* production,
                  const std::map<entity_id, float>* building_labour)
{
    // BL-1407 (Ben, 2026-10-23; FINANCE.md § The money loop): THE MONEY LOOP IS
    // THE NATION'S. The corporation pass that stood here — each firm's balance,
    // its levies and its force upkeep — is DELETED with the type; the state pass
    // below is every owner's. Levies (a nation charging a firm) and debt interest
    // retired with it (FINANCE.md § Levies, § Debt interest).
    (void)production;

    // BL-042B: mean habitability per body (from population centres) scales wages.
    // Cached per body — body_mean_habitability scans every centre, so compute each
    // body once; building_profit.hpp's estimate reads the same helper (no drift).
    std::map<entity_id, float> hab_cache;
    const auto mean_hab_of = [&](entity_id body) -> float {
        const auto it = hab_cache.find(body);
        if (it != hab_cache.end())
            return it->second;
        const float v = body_mean_habitability(w, body);
        hab_cache.emplace(body, v);
        return v;
    };

    // BL-1403 (Ben, 2026-10-23: "Taxes are taken from wages, which are also paid
    // for by the nation"; NATIONS.md § Taxes and Discontent): the quarter's tax is
    // the nation's tier share of the WAGES its own buildings paid, credited back to
    // its treasury in the state pass below. Never in the campaign settle
    // (`research_age_open`): every nation opens play at tier III with Discontent 0
    // and nothing taxed.
    const bool      tax_pass = research_age_open(w);
    const tax_params& taxp   = reg.tax();

    // BL-1255 (LOGISTICS.md § 4c; NATIONS.md § State industry: "upkeep per
    // tile"): the TRACK'S UPKEEP, a maintenance charge per owned track tile.
    // Integer tile counts per owner (std::map, ascending), so the bill is a pure
    // function of the track field. Unowned track bills no one. Empty, and every
    // term below adds nothing, on a world with no rail.
    std::map<entity_id, int> rail_tiles_of;
    for (const auto& [tile, owner] : w.rail_track)
    {
        (void)tile;
        if (owner != null_entity)
            ++rail_tiles_of[owner];
    }
    const float rail_upkeep = reg.rail().track_upkeep_per_tile;

    // BL-1296 — THE STATE PASS (NATIONS.md § What a nation owns; § 2). A nation's
    // buildings trade on the market with the treasury: what they landed was sold
    // at the clear (`flows[nation].income`), what they drew off the shelf was
    // billed there (`flows[nation].expenditure`), and their maintenance and wages
    // are charged here.
    //
    // NEVER DEBT. A building the tick's funding gate could not pay for
    // (`state_idle`) ran nothing and is charged NOTHING — not even the idle
    // floor; every funded building's whole cost fit inside the treasury before
    // anything was charged (`plan_state_funding`), so this settlement cannot
    // take a treasury below zero. Ascending nation id, each nation's buildings
    // in stored order: a fixed float order.
    //
    // BL-1407 — THE NATION FILES THE QUARTERLY RETURN (FINANCE.md § The quarterly
    // return): every nation, ascending id, one row a tick, a RETAIN of the flows
    // this pass just applied. Its opening is taken before a single flow lands (and
    // before an unbooked refund, which the row books as `refunds`), and its `net`
    // is the treasury's difference across the pass in DOUBLE (BL-1337), so a run's
    // returns telescope onto the treasury exactly.
    std::map<entity_id, quarterly_return> filed_returns;
    {
        std::vector<entity_id> nation_ids;
        for (const auto& kv : w.nations)
            nation_ids.push_back(kv.first);
        std::sort(nation_ids.begin(), nation_ids.end());
        for (const entity_id nid : nation_ids)
        {
            nation_component& nc = w.nations.at(nid);

            const float refunds         = nc.refund_unbooked;
            const bool  exact           = refunds != 0.0f
                                       && nc.treasury == nc.refund_opening + refunds;
            const float opening_balance = exact ? nc.refund_opening : nc.treasury - refunds;
            nc.refund_unbooked          = 0.0f;
            nc.refund_opening           = 0.0f;

            corp_budget bud;
            float book_value = 0.0f;
            const bool in_pass = !nc.state_assets.empty() || rail_tiles_of.count(nid) != 0; // BL-1255: or track
            if (in_pass)
            {
                world::state_tick_ledger& led = w.state_ledger[nid];
                led.treasury_settle_from = nc.treasury;
                float delta = 0.0f;

                if (const auto fit = flows.find(nid); fit != flows.end())
                {
                    led.income      = fit->second.income;
                    led.expenditure = fit->second.expenditure;
                    delta += fit->second.income - fit->second.expenditure;
                }
                for (const entity_id bid : nc.state_assets)
                {
                    const auto bit = w.buildings.find(bid);
                    if (bit == w.buildings.end())
                        continue;
                    const building_component& b = bit->second;
                    // Historical cost over the holdings (FINANCE.md: book value),
                    // in stored order; touches no term of `delta`.
                    book_value += reg.economics(b.type).build_cost;
                    if (b.state_idle)
                        continue; // unfunded: ran nothing, pays nothing
                    entity_id body = null_entity;
                    if (const auto tit = w.tiles.find(b.tile); tit != w.tiles.end())
                        body = tit->second.body;
                    float scalar = 1.0f;
                    if (const auto cit = contention.find(std::make_pair(nid, body)); cit != contention.end())
                        scalar = cit->second;
                    if (building_labour != nullptr)
                        if (const auto bl = building_labour->find(bid); bl != building_labour->end())
                            scalar = bl->second;
                    const building_opex opex = compute_building_opex(b, reg.economics(b.type), scalar,
                                                                     mean_hab_of(body),
                                                                     reg.idle_maintenance_floor());
                    led.maintenance += opex.maintenance;
                    delta           -= opex.maintenance;
                    led.wages       += opex.wages;
                    delta           -= opex.wages;
                }
                // BL-1403: the wage tax — the tier's share of the wages the nation's
                // own buildings paid this tick comes back to its treasury (a higher
                // tier lowers the net wage bill; Discontent is its cost, nation_step).
                // Taken on what was actually paid, after the wages, so it can never
                // carry the treasury below where the wages left it.
                if (tax_pass && led.wages > 0.0f)
                {
                    const float tax = nation_wage_tax(led.wages, nc.tax_tier, taxp);
                    led.taxes = tax;
                    delta    += tax;
                }
                // BL-1255: the nation's track, after its buildings — FUNDED by this
                // tick's gate (plan_state_funding committed it), or IDLE
                // (`w.rail_idle`) and charged nothing. The clamp is the never-debt
                // backstop the building settlement also relies on.
                if (const auto rt = rail_tiles_of.find(nid);
                    rt != rail_tiles_of.end() && rail_upkeep > 0.0f && w.rail_idle.count(nid) == 0)
                {
                    const float bill = rail_upkeep * static_cast<float>(rt->second);
                    const float paid = std::clamp(bill, 0.0f, std::max(0.0f, nc.treasury + delta));
                    led.maintenance += paid;
                    delta           -= paid;
                }
                nc.treasury       += delta;
                led.treasury_close = nc.treasury;
                led.committed      = 0.0f; // settled: nothing this tick is still owed

                bud.income      = led.income;
                bud.expenditure = led.expenditure;
                bud.maintenance = led.maintenance;
                bud.wages       = led.wages;
                bud.taxes       = led.taxes;
            }
            if (breakdown && in_pass)
                (*breakdown)[nid] = bud; // BL-1407: the nation's own breakdown (the Budget ledger reads it)

            quarterly_return qr;
            qr.income      = bud.income;
            qr.expenditure = bud.expenditure;
            qr.maintenance = bud.maintenance;
            qr.wages       = bud.wages;
            qr.upkeep      = bud.upkeep;
            qr.taxes       = bud.taxes;
            qr.refunds     = refunds;
            qr.net         = static_cast<double>(nc.treasury) - static_cast<double>(opening_balance);
            qr.balance     = nc.treasury;
            qr.holdings    = static_cast<uint32_t>(nc.state_assets.size());
            qr.book_value  = book_value;
            filed_returns.emplace(nid, qr);
        }
    }

    // BL-626 / BL-1407: file the returns. Ascending nation id (the map's own
    // order), one row per nation per tick, rolling retention of the last
    // `k_quarterly_return_retention` quarters with the oldest dropped first.
    for (const auto& [nid, qr] : filed_returns)
    {
        std::vector<quarterly_return>& hist = w.nations.at(nid).returns;
        hist.push_back(qr);
        if (hist.size() > k_quarterly_return_retention)
            hist.erase(hist.begin(),
                       hist.begin()
                           + static_cast<std::ptrdiff_t>(hist.size()
                                                         - k_quarterly_return_retention));
    }
}
