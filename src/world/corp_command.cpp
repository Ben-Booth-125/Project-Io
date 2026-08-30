#include "corp_command.hpp"

#include "battle_system.hpp" // request_withdraw, unit_in_battle (BL-467)

#include "condition_set.hpp" // BL-350: request_quote's embargo decline condition
#include "construction.hpp"
#include "economy_system.hpp" // BL-430: try_switch_recipe, the shared recipe-switch gate
#include "logistics.hpp" // invalidate_logistics_caches (idle/resume flips the anchor set); intra_body_path (BL-470)
// BL-470 included placement_rules.hpp here for is_water_tile, march_unit's old
// destination check. BL-511 retargeted the verb to a province, and while the
// partition was LAND-ONLY the water test had no remaining caller. BL-516 draws
// provinces over the water, so the check is back — but it asks the PROVINCE its
// domain (`province_kind_of`, province.hpp) rather than asking a tile its
// substrate, so placement_rules is still not a dependency of this file.
#include "province.hpp"       // BL-511: march_unit's destination is a province
#include "recipe_registry.hpp"
#include "stance.hpp" // BL-448: corp stance verbs (declare_hostile / offer_friendship / accept_friendship / return_to_neutral)
#include "supply_system.hpp" // BL-452: the shared dispatch (price_convoy_leg / commit_convoy)
#include "survey_system.hpp"
#include "unit_roster.hpp"
#include "world.hpp"

#include <algorithm>
#include <cassert> // BL-388: build-seam recipe-forwarding assertion
#include <cmath>
#include <cstddef> // BL-628: std::ptrdiff_t in the dissolution walk's vector erases
#include <iterator> // BL-628: std::next, over the erase-while-walking sets
#include <limits>  // BL-628: the HQ recompute's nearest-centroid search
#include <set>     // BL-628: the stance tables the dissolution walk drops from
#include <utility> // BL-628: std::pair / std::make_pair over the (corp, body) keys
#include <vector>

namespace {

/// True iff `corp` owns `building` (the corp's `assets` list is the ownership
/// authority — world.hpp § Ownership accessors).
bool owns(const world& w, entity_id corp, entity_id building)
{
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return false;
    const auto& a = cit->second.assets;
    return std::find(a.begin(), a.end(), building) != a.end();
}

// ---------------------------------------------------------------------------
// hire_unit's cost debit (BL-324)
//
// The campaign roster gate (unit_roster.hpp) asks a yes/no question: does the
// corp hold ANY of the resource an axis needs, summed across its (corp, body)
// pools — the live L3 store (world.hpp § corp_body_pools).
// The debit below spends a flat, first-cut draw (hire_axis_cost) from that
// same resource preference order — not final balance, just enough that
// hiring is a real spend rather than a free unlock. Two-phase (check every
// axis affordable, THEN debit) so a mid-hire failure never leaves the corp
// partially charged.
//
// BL-394: this leg alone was NOT enough — an ungated row (all-zero gate,
// e.g. row 0) tested and debited nothing, so the cheapest units were free
// and unbounded for any seam caller. A credit cost now sits alongside it,
// computed in the verb from economy.lua's military table (recipe_registry's
// military_capability_params) and debited from the corp's balance.
// ---------------------------------------------------------------------------

/// The resource an axis actually spends: the first candidate on the SHARED
/// table (unit_roster.hpp § hire_axis_table) the corp holds enough of. The
/// gate reads the same table, so a hire can never spend a resource the gate
/// didn't already confirm access to — BL-498 made that structural rather than
/// a comment asking two hand-written lists to stay in step.
resource_type hire_axis_resource(const world& w, entity_id corp, hire_axis axis)
{
    const hire_axis_entry& e = hire_axis_resources(axis);
    for (const resource_type r : e)
        if (corp_stockpile_total(w, corp, r) >= hire_axis_cost)
            return r;
    return e.candidates[0]; // unreachable if affordability already passed; a safe fallback.
}

/// Can @p corp cover one axis's hire cost from a SINGLE candidate resource?
bool corp_can_afford_axis(const world& w, entity_id corp, hire_axis axis)
{
    for (const resource_type r : hire_axis_resources(axis))
        if (corp_stockpile_total(w, corp, r) >= hire_axis_cost)
            return true;
    return false;
}

/// Debit @p amount of @p res from @p corp's (corp, body) pools, draining in
/// ascending body-id order (the map's own key order, so deterministic). Only
/// called after affordability is confirmed, so this should never under-run —
/// but walks defensively rather than assuming a single pool holds the whole
/// amount.
void debit_from_corp(world& w, entity_id corp, resource_type res, float amount)
{
    for (auto it = w.corp_body_pools.lower_bound({corp, entity_id{0}});
         it != w.corp_body_pools.end() && it->first.first == corp; ++it)
    {
        if (amount <= 0.0f) return;
        float& q = it->second.quantities[static_cast<std::size_t>(res)];
        const float take = std::min(q, amount);
        q      -= take;
        amount -= take;
    }
}

/// Debit the roster row's gated axes from the corp's pools. All-or-nothing:
/// checks every gated axis is affordable before debiting any of them.
bool debit_hire_cost(world& w, entity_id corp, const roster_row& row)
{
    // Walk the axes in table order — ore, farm, energy — checking every gated
    // one is affordable BEFORE debiting any, so a part-paid hire is impossible.
    // port_q is a building check (corp_owns_port), not a resource: it is not on
    // the table and there is nothing to debit for it.
    for (std::size_t i = 0; i < hire_axis_count; ++i)
    {
        const hire_axis axis = static_cast<hire_axis>(i);
        if (gate_axis_q(row.gate, axis) > 0 && !corp_can_afford_axis(w, corp, axis))
            return false;
    }

    for (std::size_t i = 0; i < hire_axis_count; ++i)
    {
        const hire_axis axis = static_cast<hire_axis>(i);
        if (gate_axis_q(row.gate, axis) > 0)
            debit_from_corp(w, corp, hire_axis_resource(w, corp, axis), hire_axis_cost);
    }
    return true;
}

/// The batch size one hire_unit command raises — matches the campaign's
/// original Kepler unit stub (hard_coded_world.cpp), which used the same
/// figure before BL-324 gave hiring a real seam.
constexpr int hire_batch_manpower = 50;

/// The market a body's orders would list against, or `null_entity` if the body
/// carries none. A body may host several markets (BL-096); an order is placed at
/// body scope and routes to a centre at clearing time (market_for_tile), so this
/// only answers "is there a market here at all", which is the UI's own
/// precondition ("This body has no market.").
entity_id any_market_on_body(const world& w, entity_id body)
{
    entity_id best = null_entity;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body && (best == null_entity || mid < best))
            best = mid; // lowest id, so the answer does not depend on map order
    return best;
}

/// BL-350 Q2, decline condition 1 ("no capacity"): does `supplier` own a
/// COMPLETED building that can produce `resource` at all? Extraction: a site
/// targeting the resource. Processing: a recipe whose sole authored output is
/// the resource. Returns the producing building's body via `out_body` (the
/// contract fulfils there) — null on no match.
bool supplier_has_capacity(const world& w, const recipe_registry& reg, entity_id supplier,
                           resource_type resource, entity_id& out_body)
{
    const auto cit = w.corporations.find(supplier);
    if (cit == w.corporations.end())
        return false;
    for (const entity_id bid : cit->second.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const building_component& b = bit->second;
        if (b.ticks_remaining > 0 || b.decommissioned)
            continue; // still under construction, or torn down
        if (b.type == building_type::extraction_site && b.target_resource == resource)
        {
            const auto tit = w.tiles.find(b.tile);
            out_body = (tit != w.tiles.end()) ? tit->second.body : null_entity;
            return out_body != null_entity;
        }
        if (b.type == building_type::processing_facility)
        {
            const recipe* r = reg.get_recipe(b.recipe);
            if (r != nullptr && r->outputs[static_cast<std::size_t>(resource)] > 0.0f)
            {
                const auto tit = w.tiles.find(b.tile);
                out_body = (tit != w.tiles.end()) ? tit->second.body : null_entity;
                return out_body != null_entity;
            }
        }
    }
    return false;
}

/// BL-392: the body a contract's goods should LAND on — the buyer's own centre
/// of gravity, taken as the body carrying the lowest-id building the buyer owns.
/// Lowest id rather than "first in `assets`" because `assets` is an append order
/// that a demolish can permute; the id is stable, so the same world quotes the
/// same delivery body every time (the determinism invariant).
///
/// Returns null_entity when the buyer owns nothing anywhere — request_quote then
/// falls back to the supplier's fulfilment body, which is the pre-BL-392
/// behaviour and the only case with nowhere better to put the goods.
entity_id buyer_delivery_body(const world& w, entity_id buyer)
{
    const auto cit = w.corporations.find(buyer);
    if (cit == w.corporations.end())
        return null_entity;
    entity_id best_building = null_entity;
    entity_id best_body     = null_entity;
    for (const entity_id bid : cit->second.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const auto tit = w.tiles.find(bit->second.tile);
        if (tit == w.tiles.end() || tit->second.body == null_entity)
            continue;
        if (best_building == null_entity || bid < best_building)
        {
            best_building = bid;
            best_body     = tit->second.body;
        }
    }
    return best_body;
}

/// BL-392: the supplier's ACTUAL per-tick throughput of `resource`, summed over
/// every completed, active building they own that makes it — extraction sites
/// targeting it at their own base rate, processing facilities at their recipe's
/// yield of it times theirs.
///
/// The lead time used to be `base_lead_ticks * ceil(quantity / a GLOBAL
/// processing constant)`, so a supplier with ten facilities quoted exactly the
/// same term as one with a single stalled site. Reading their real capacity is
/// what makes the number mean anything — and what makes shopping on lead time
/// possible once BL-390 puts the term on screen.
///
/// Zero is possible in principle (a corp whose only producing site was
/// decommissioned between the capacity check and here); the caller floors it.
float supplier_throughput(const world& w, const recipe_registry& reg, entity_id supplier,
                          resource_type resource)
{
    const auto cit = w.corporations.find(supplier);
    if (cit == w.corporations.end())
        return 0.0f;
    const std::size_t ri = static_cast<std::size_t>(resource);

    // Ascending building id: this is a float sum over the corp's assets, and
    // float addition is not associative, so the ORDER has to be fixed by
    // something stable rather than by `assets`' append history.
    std::vector<entity_id> owned(cit->second.assets.begin(), cit->second.assets.end());
    std::sort(owned.begin(), owned.end());

    float total = 0.0f;
    for (const entity_id bid : owned)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const building_component& b = bit->second;
        if (b.ticks_remaining > 0 || b.decommissioned)
            continue;
        const float rate = reg.economics(b.type).base_rate;
        if (b.type == building_type::extraction_site && b.target_resource == resource)
        {
            total += rate;
        }
        else if (b.type == building_type::processing_facility)
        {
            const recipe* r = reg.get_recipe(b.recipe);
            if (r != nullptr && r->outputs[ri] > 0.0f)
                total += rate * r->outputs[ri];
        }
    }
    return total;
}

/// BL-350 Q2, decline condition 2 ("no input access"): for a PROCESSED good
/// (the recipe found by supplier_has_capacity), can the supplier's own local
/// market actually be bought from for every non-zero input? Extraction has no
/// recipe inputs, so it always passes this leg — capacity alone gates it.
/// Reuses run_construction's own "unpriced == unbuyable" reading rather than
/// modelling live supply, matching BL-340's asymmetry fix.
bool supplier_has_input_access(const world& w, const recipe_registry& reg, entity_id supplier,
                               resource_type resource, entity_id body)
{
    const auto cit = w.corporations.find(supplier);
    if (cit == w.corporations.end())
        return true;
    for (const entity_id bid : cit->second.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end() || bit->second.type != building_type::processing_facility)
            continue;
        const recipe* r = reg.get_recipe(bit->second.recipe);
        if (r == nullptr || r->outputs[static_cast<std::size_t>(resource)] <= 0.0f)
            continue; // not the producing recipe
        const entity_id mid = any_market_on_body(w, body);
        if (mid == null_entity)
            return false; // no market to buy inputs from at all
        const market_component& mc = w.markets.at(mid);
        for (std::size_t i = 0; i < resource_count; ++i)
            if (r->inputs[i] > 0.0f && mc.base_price[i] <= 0.0f)
                return false; // an input this recipe needs cannot be bought here
        return true;
    }
    return true; // extraction (or no recipe found) — nothing to check
}

/// Number of standing sell orders `corp` currently holds, across all bodies.
std::size_t corp_order_count(const world& w, entity_id corp)
{
    std::size_t n = 0;
    for (const sell_order& o : w.sell_orders)
        if (o.corp == corp)
            ++n;
    return n;
}

// ---------------------------------------------------------------------------
// BL-628 — the whole-firm buyout's dissolution walk
// ---------------------------------------------------------------------------
// THIS IS THE ONLY PLACE IN THE CODEBASE THAT ERASES AN ACTOR. Every other verb
// moves goods, credits, units, sentiment or one building; `demolish_building`
// (construction.cpp) is the nearest precedent and it erases exactly three things
// (the building, the corp asset, the building stockpile) with a fourth —
// orphaned units — swept up later by the upkeep pass (FINANCE.md § Standing-force
// upkeep). A corporation is referenced by far more stores than that, and the
// sweep cannot be deferred to a later pass the way an orphaned unit's can: once
// the corp id is gone, no pass can recognise a row that names it.
//
// THE INVENTORY BELOW IS THE REQUIREMENT (R4). It was built by walking every
// container in world.hpp that holds an `entity_id` naming a corporation. If a
// future change adds another, it belongs here — and `whole_firm_buyout.cpp`
// row R4 is the check that will notice, because it re-scans the world for ANY
// surviving reference rather than asserting against a hand-written list.
//
// Three dispositions, and which one a store gets is a design decision, not a
// convenience:
//
//   TRANSFER — the thing is property, and property is what was bought. Holdings,
//              (corp, body) pools, balance, filed returns, units, convoys in
//              flight, trade routes, accepted procurement contracts, accepted
//              mercenary contracts.
//   CANCEL   — the thing is a PROMISE made by a party that no longer exists, and
//              there is nobody left to keep it. Open sell/buy orders (FINANCE.md
//              says this in as many words), live procurement quotes, live battles.
//   DROP     — the thing is an OPINION, a permission or a capability that was the
//              dissolved firm's alone, and inheriting it would invent a fact.
//              Sentiment rows, stance rows, the embargo predicate, earned techs,
//              scalar modifiers.

/// Odd-r offset (col,row) -> unit-hex local centre (hex_size = 1).
///
/// DUPLICATED from `corporation_generation.cpp`'s file-local `hex_unit_centre`;
/// see `recompute_hq` below for why, and for what is owed.
std::pair<float, float> buyout_hex_unit_centre(int col, int row)
{
    constexpr float kSqrt3 = 1.7320508075688772f;
    const float x = kSqrt3 * static_cast<float>(col)
                  + ((row & 1) ? kSqrt3 * 0.5f : 0.0f);
    const float y = 1.5f * static_cast<float>(row);
    return { x, y };
}

/// Re-designate @p cc's HQ and influence range over its (now merged) holding
/// set, by Pass 3b's rule: the HQ is the holding nearest the holdings centroid
/// on the corp's home body, and the range is the furthest holding's distance
/// from that HQ plus a fixed projected reach. A corp with no holdings on the
/// home body keeps `{null_entity, 0}` — no border.
///
/// The home body is the body of the corp's FIRST surviving holding, which after
/// a buyout is one of the ACQUIRER's own (the target's holdings are appended
/// after them) — so an acquisition never relocates the acquirer's seat to the
/// bought firm's world. That is deliberate: the buyer's identity survives the
/// purchase.
///
/// **OWED: this duplicates `designate_hq` / `corp_home_body`, both file-local to
/// `corporation_generation.cpp`.** Hoisting the pair into a shared seam is the
/// right fix and was NOT taken here because that file is owned by a concurrent
/// task in this same wave. The constant below must stay in step with
/// `kProjectedReachUnits` there until it is.
void recompute_hq(const world& w, corporation_component& cc)
{
    constexpr float kProjectedReachUnits = 2.5f; // == corporation_generation.cpp's

    cc.hq_building     = null_entity;
    cc.influence_range = 0.0f;

    // Home body = the body of the first holding that still resolves to a tile.
    entity_id home_body = null_entity;
    for (const entity_id b : cc.assets)
    {
        const auto bit = w.buildings.find(b);
        if (bit == w.buildings.end())
            continue;
        const auto tit = w.tiles.find(bit->second.tile);
        if (tit != w.tiles.end()) { home_body = tit->second.body; break; }
    }
    if (home_body == null_entity)
        return;

    // Walk `assets` in its own order — a vector, so this is deterministic
    // whatever the building map's layout.
    std::vector<std::pair<entity_id, std::pair<float, float>>> pts;
    pts.reserve(cc.assets.size());
    for (const entity_id bid : cc.assets)
    {
        const auto b = w.buildings.find(bid);
        if (b == w.buildings.end())
            continue;
        const auto t = w.tiles.find(b->second.tile);
        if (t == w.tiles.end() || t->second.body != home_body)
            continue;
        pts.emplace_back(bid, buyout_hex_unit_centre(t->second.grid_x, t->second.grid_y));
    }
    if (pts.empty())
        return;

    float cx = 0.0f, cy = 0.0f;
    for (const auto& p : pts) { cx += p.second.first; cy += p.second.second; }
    cx /= static_cast<float>(pts.size());
    cy /= static_cast<float>(pts.size());

    entity_id               hq   = pts.front().first;
    std::pair<float, float> hq_c = pts.front().second;
    float                   best = std::numeric_limits<float>::max();
    for (const auto& p : pts)
    {
        const float dx = p.second.first - cx, dy = p.second.second - cy;
        const float d2 = dx * dx + dy * dy;
        if (d2 < best) { best = d2; hq = p.first; hq_c = p.second; }
    }

    float max_d = 0.0f;
    for (const auto& p : pts)
    {
        const float dx = p.second.first - hq_c.first, dy = p.second.second - hq_c.second;
        max_d = std::max(max_d, std::sqrt(dx * dx + dy * dy));
    }
    cc.hq_building     = hq;
    cc.influence_range = max_d + kProjectedReachUnits;
}

/// Merge @p src's filed returns into @p dst's, PAIRWISE FROM THE NEWEST END.
///
/// **This is an interpretive call and it is flagged as one.** FINANCE.md says the
/// target's filed returns "move to the acquirer" and does not say how two
/// histories combine. Concatenation is not available: `quarterly_return` carries
/// no tick field (by design — one row per corp per tick, appended in order), so
/// there is no key to interleave two histories by, and appending would produce a
/// record with two rows for the same quarter. Both vectors ARE contemporaneous —
/// oldest first, newest last, one row per economy tick — so aligning them at the
/// newest end is the only join the data structure actually supports.
///
/// What the merge means: the result is a PRO-FORMA combined history — what the
/// merged firm's last N quarters would have read as, had it always been one firm.
/// Flows and `net` sum; `balance` sums (combined cash); `holdings` and
/// `book_value` sum (they are stocks, and the merged firm's holdings ARE the
/// sum). The length is the longer of the two, capped at the retention window.
///
/// The consequence worth stating: the acquirer's own `trailing_net` afterwards
/// reflects both firms' earnings, so a second buyer prices the merged firm off
/// the merged record. That is the behaviour you want, and it is why summation is
/// preferred to simply discarding the target's history.
void merge_returns(std::vector<quarterly_return>& dst,
                   const std::vector<quarterly_return>& src)
{
    if (src.empty())
        return;

    const std::size_t n = std::max(dst.size(), src.size());
    std::vector<quarterly_return> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        // `back` counts BACK from the newest row of each history; 1 == newest.
        const std::size_t back = n - i;
        quarterly_return  row{};
        const quarterly_return* a =
            (dst.size() >= back) ? &dst[dst.size() - back] : nullptr;
        const quarterly_return* b =
            (src.size() >= back) ? &src[src.size() - back] : nullptr;
        if (a) row = *a;
        if (b)
        {
            row.income      += b->income;
            row.expenditure += b->expenditure;
            row.maintenance += b->maintenance;
            row.wages       += b->wages;
            row.interest    += b->interest;
            row.levies      += b->levies;
            row.upkeep      += b->upkeep;
            row.net         += b->net;
            row.balance     += b->balance;
            row.holdings    += b->holdings;
            row.book_value  += b->book_value;
        }
        out.push_back(row);
    }
    if (out.size() > k_quarterly_return_retention)
        out.erase(out.begin(),
                  out.begin()
                      + static_cast<std::ptrdiff_t>(out.size()
                                                    - k_quarterly_return_retention));
    dst = std::move(out);
}

/// Transfer everything @p target owns to @p acquirer, then erase @p target from
/// the world. Both ids name live corporations; the caller has already priced the
/// firm and taken the payment. Reads no clock, no RNG, and no unordered
/// container's ORDER — only its contents.
void dissolve_into(world& w, entity_id acquirer, entity_id target)
{
    corporation_component& acq = w.corporations.at(acquirer);
    // By VALUE: the source row is erased at the end of this function, and half the
    // walk below would otherwise be reading through a reference into a map it is
    // about to invalidate.
    const corporation_component tgt = w.corporations.at(target);

    // --- TRANSFER: holdings -------------------------------------------------
    // Appended AFTER the acquirer's own, so `assets` order stays "the buyer's
    // first" and `recompute_hq` keeps the buyer's home body.
    acq.assets.insert(acq.assets.end(), tgt.assets.begin(), tgt.assets.end());

    // --- TRANSFER: balance --------------------------------------------------
    // Signed: buying a firm buys its cash AND its debts (FINANCE.md).
    acq.balance += tgt.balance;

    // --- TRANSFER: filed returns -------------------------------------------
    merge_returns(acq.returns, tgt.returns);

    // --- TRANSFER: (corp, body) stockpile pools ----------------------------
    // Collected first, then merged, then erased: `pool_for` INSERTS, and mutating
    // the map mid-walk is the kind of thing that reads fine and is wrong. The
    // walk is over a std::map, so it is a sorted walk by (corp, body) — BL-158.
    {
        std::vector<std::pair<entity_id, stockpile_component>> moving;
        for (auto it = w.corp_body_pools.lower_bound({target, entity_id{0}});
             it != w.corp_body_pools.end() && it->first.first == target; ++it)
            moving.emplace_back(it->first.second, it->second);
        for (const auto& mv : moving)
        {
            w.corp_body_pools.erase(std::make_pair(target, mv.first));
            stockpile_component& dst = w.pool_for(acquirer, mv.first);
            for (std::size_t r = 0; r < resource_count; ++r)
                dst.quantities[r] += mv.second.quantities[r];
        }
    }

    // --- TRANSFER: workforce supply overrides ------------------------------
    // The acquirer now runs the target's buildings on those bodies, so the
    // authored labour supply follows the holdings. Where BOTH corps had a row on
    // one body the acquirer's own is KEPT rather than summed: the override is
    // "labour available to this corp here", and two firms drawing on one body's
    // labour were never drawing on disjoint pools, so adding them would mint
    // workers the body does not have.
    {
        std::vector<std::pair<entity_id, float>> moving;
        for (auto it = w.workforce_supply_overrides.lower_bound({target, entity_id{0}});
             it != w.workforce_supply_overrides.end() && it->first.first == target; ++it)
            moving.emplace_back(it->first.second, it->second);
        for (const auto& mv : moving)
        {
            w.workforce_supply_overrides.erase(std::make_pair(target, mv.first));
            w.workforce_supply_overrides.emplace(std::make_pair(acquirer, mv.first), mv.second);
        }
    }

    // --- TRANSFER: units, re-pointed THROUGH THEIR MUSTER BASE -------------
    // Not a blanket `owner = acquirer`: the unit follows the corp that now owns
    // the base it was raised at (`unit_component::muster_base`, the same key the
    // upkeep pass's orphan sweep reads). After the holdings transfer above that
    // resolves to the acquirer for every one of the target's bases — but going
    // THROUGH the base rather than around it is what keeps this correct if a
    // holding ever moves by some other route. A unit whose base has already been
    // demolished has no owner to inherit from and falls back to the acquirer; the
    // upkeep pass then disbands it exactly as it would have.
    // Order-independent: a per-element assignment, so the unordered walk is safe.
    for (auto& uit : w.units)
    {
        unit_component& uc = uit.second;
        if (uc.owner != target)
            continue;
        const entity_id via = (uc.muster_base != null_entity)
                                  ? owner_corp_of(w, uc.muster_base)
                                  : null_entity;
        uc.owner = (via != null_entity) ? via : acquirer;
    }

    // --- TRANSFER: convoys in flight ---------------------------------------
    // A convoy is CARGO, not a promise: the goods left the source pool at
    // dispatch and the haul is already paid for (`cost_paid`). Cancelling one
    // would have to invent a return leg or destroy the stock — the same argument
    // `hold_convoy` makes for not being a cancel.
    for (convoy_component& c : w.convoys)
        if (c.corp == target)
            c.corp = acquirer;

    // --- TRANSFER: trade routes, merging any pair that collides ------------
    // A route is keyed (unordered body pair, corp), so re-pointing can produce
    // two rows with the same key. They are folded into the FIRST such row —
    // counts summed, last-traffic stamp taken as the later — so the fog reads one
    // lane, not two. Walked in vector order; the erase is by index, descending,
    // so no surviving row's position shifts under a pending removal.
    for (trade_route& r : w.trade_routes)
        if (r.corp == target)
            r.corp = acquirer;
    {
        std::vector<std::size_t> drop;
        for (std::size_t i = 0; i < w.trade_routes.size(); ++i)
        {
            if (std::find(drop.begin(), drop.end(), i) != drop.end())
                continue;
            for (std::size_t j = i + 1; j < w.trade_routes.size(); ++j)
            {
                if (std::find(drop.begin(), drop.end(), j) != drop.end())
                    continue;
                const trade_route& a = w.trade_routes[i];
                const trade_route& b = w.trade_routes[j];
                const bool same_pair = (a.body_a == b.body_a && a.body_b == b.body_b)
                                    || (a.body_a == b.body_b && a.body_b == b.body_a);
                if (!same_pair || a.corp != b.corp)
                    continue;
                w.trade_routes[i].convoy_count += b.convoy_count;
                w.trade_routes[i].last_tick = std::max(a.last_tick, b.last_tick);
                drop.push_back(j);
            }
        }
        std::sort(drop.begin(), drop.end());
        for (auto it = drop.rbegin(); it != drop.rend(); ++it)
            w.trade_routes.erase(w.trade_routes.begin() + static_cast<std::ptrdiff_t>(*it));
    }

    // --- CANCEL: the order book --------------------------------------------
    // FINANCE.md, in as many words: open market orders are CANCELLED rather than
    // reassigned — an order is a promise made by a party that no longer exists.
    // Both sides of the book, though only the sell side has a press today.
    w.sell_orders.erase(std::remove_if(w.sell_orders.begin(), w.sell_orders.end(),
                                       [&](const sell_order& o) { return o.corp == target; }),
                        w.sell_orders.end());
    w.buy_orders.erase(std::remove_if(w.buy_orders.begin(), w.buy_orders.end(),
                                      [&](const buy_order& o) { return o.corp == target; }),
                       w.buy_orders.end());

    // --- CANCEL: live procurement quotes -----------------------------------
    // A quote is an unaccepted OFFER — the order book's argument exactly. Nothing
    // has been paid and nobody is owed anything; the counterparty simply asks
    // again if it still wants the goods.
    w.procurement_quotes.erase(
        std::remove_if(w.procurement_quotes.begin(), w.procurement_quotes.end(),
                       [&](const procurement_quote& q)
                       { return q.buyer == target || q.supplier == target; }),
        w.procurement_quotes.end());

    // --- TRANSFER: accepted procurement contracts --------------------------
    // An ACCEPTED contract is different from a quote and from an order: a deposit
    // has changed hands and delivery is part-paid, so it is an asset (or a
    // liability) of the firm that was bought and it transfers with the rest. The
    // one case that cannot transfer is a contract that would leave the acquirer on
    // BOTH sides — there is no buying from yourself — and those are dropped, the
    // deposit already paid staying paid.
    for (procurement_contract& c : w.procurement_contracts)
    {
        if (c.buyer == target)    c.buyer    = acquirer;
        if (c.supplier == target) c.supplier = acquirer;
    }
    w.procurement_contracts.erase(
        std::remove_if(w.procurement_contracts.begin(), w.procurement_contracts.end(),
                       [](const procurement_contract& c) { return c.buyer == c.supplier; }),
        w.procurement_contracts.end());

    // --- TRANSFER: mercenary contracts -------------------------------------
    // The committed force transferred above (the `units` array is unchanged and
    // those units are now the acquirer's), so the contractor follows it. The
    // client is a NATION and is untouched. A terminal row is re-pointed too: it is
    // the ledger's record of work done, and the firm that did it is now part of
    // the acquirer.
    for (mercenary_contract& c : w.mercenary_contracts)
        if (c.contractor == target)
            c.contractor = acquirer;

    // --- CANCEL: live battles ----------------------------------------------
    // A battle is a fight between two parties; one of them ceasing to exist ends
    // it, and there is no honest way to substitute a combatant mid-engagement.
    // The stance rows that opened it are dropped just below, so nothing re-opens
    // it on the next tick.
    w.battles.erase(std::remove_if(w.battles.begin(), w.battles.end(),
                                   [&](const active_battle& b)
                                   { return b.attacker == target || b.defender == target; }),
                    w.battles.end());

    // --- DROP: stance ------------------------------------------------------
    // A declaration is about a party. Inheriting the target's hostilities would
    // hand the acquirer wars it never declared, and inheriting its friendships
    // would mint agreement the other side never gave (a friendship row is
    // evidence BOTH corps chose it — stance.hpp's invariant 1).
    const auto drop_pairs = [&](std::set<std::pair<entity_id, entity_id>>& s)
    {
        for (auto it = s.begin(); it != s.end();)
            it = (it->first == target || it->second == target) ? s.erase(it) : std::next(it);
    };
    drop_pairs(w.corp_hostile_pairs);
    drop_pairs(w.corp_friend_pairs);
    drop_pairs(w.corp_friend_offers);

    // --- DROP: sentiment rows ----------------------------------------------
    // Sentiment is an OPINION held BY someone ABOUT someone. Both directions go:
    // nobody has an opinion of a firm that no longer exists, and the dissolved
    // firm's own opinions die with it. Merging them into the acquirer's rows would
    // invent feelings neither party ever had.
    for (auto it = w.sentiment.pairs.begin(); it != w.sentiment.pairs.end();)
        it = (it->first.first == target || it->first.second == target)
                 ? w.sentiment.pairs.erase(it)
                 : std::next(it);

    // --- DROP: permissions and capabilities that were the target's alone ---
    w.corp_embargo_conditions.erase(target); // a refusal-to-deal predicate is the firm's own
    w.earned_techs.erase(target);            // FINANCE.md lists what transfers; research is not on it
    w.corp_modifiers.erase(target);          // scalar modifiers were applied TO that firm

    // --- The seat, recomputed over the merged holding set -------------------
    recompute_hq(w, acq);

    // --- The actor is gone --------------------------------------------------
    w.corporations.erase(target);
}

corp_command_result map_construction(construction_result r)
{
    switch (r)
    {
        case construction_result::placed:                 return corp_command_result::applied;
        case construction_result::insufficient_funds:     return corp_command_result::rejected_funds;
        case construction_result::no_corp:                return corp_command_result::rejected_no_corp;
        case construction_result::no_tile:                return corp_command_result::rejected_invalid;
        case construction_result::tech_locked:            return corp_command_result::rejected_tech_locked;
        case construction_result::era_locked:             return corp_command_result::rejected_era_locked;
        case construction_result::invalid_tile:
        case construction_result::out_of_range:
        case construction_result::slot_occupied:
        case construction_result::insufficient_materials: return corp_command_result::rejected_placement;
    }
    return corp_command_result::rejected_invalid;
}

} // namespace

// ---------------------------------------------------------------------------
// BL-628 — the acquisition price (docs/economy/FINANCE.md § Whole-firm acquisition)
// ---------------------------------------------------------------------------

float corp_trailing_net(const corporation_component& c)
{
    if (c.returns.empty())
        return 0.0f;

    const std::size_t n = std::min(c.returns.size(), k_acquisition_trailing_quarters);
    // Accumulated in DOUBLE and over the newest n rows in filed order, so the sum
    // is a pure function of the record rather than of how the window happened to
    // be walked. n is at most 8, so this cannot overflow anything.
    double sum = 0.0;
    for (std::size_t i = c.returns.size() - n; i < c.returns.size(); ++i)
        sum += static_cast<double>(c.returns[i].net);
    return static_cast<float>(sum / static_cast<double>(n));
}

float corp_acquisition_price(const corporation_component& target, float multiple)
{
    if (target.returns.empty())
        return 0.0f; // never filed — the seam refuses to price it at all

    const double book    = static_cast<double>(target.returns.back().book_value);
    const double profit  = static_cast<double>(multiple)
                         * static_cast<double>(corp_trailing_net(target));
    const double cash    = static_cast<double>(target.balance);
    const double priced  = book + profit + cash;

    // A NON-FINITE PRICE IS PROPAGATED, NEVER FLOORED. This ordering is
    // load-bearing and was put here by a harness failure, not by foresight: with
    // the floor applied first, `NaN > 0.0` is false, so a corrupt filed return
    // priced the firm at exactly ZERO and the seam's own `isfinite` guard saw a
    // perfectly finite number. A corrupt record must reject the command, not
    // hand the caller a free corporation.
    if (!std::isfinite(priced))
        return static_cast<float>(priced);

    // The floor is ZERO and deliberately not book value — FINANCE.md § Whole-firm
    // acquisition: there is no salvage in the prototype, so book value is not a
    // redemption anyone could take, and the price is a SINK rather than a payment
    // to a modelled seller. A firm priced at zero is worthless, stated plainly.
    return (priced > 0.0) ? static_cast<float>(priced) : 0.0f;
}

corp_command_result apply_corp_command(world& w, const recipe_registry& reg,
                                       const corp_command& cmd,
                                       entity_id* out_building)
{
    if (out_building)
        *out_building = null_entity;

    if (w.corporations.find(cmd.corp) == w.corporations.end())
        return corp_command_result::rejected_no_corp;

    switch (cmd.verb)
    {
        case corp_verb::build:
        {
            // BL-388: a stated recipe must reach the built processor — reject
            // rather than silently coerce. construct_building substitutes steel
            // for no_recipe (right for a caller with no opinion, wrong for one
            // that named a recipe), so before this forward every seam-built
            // processor answered `applied` and ran steel whatever was asked.
            if (cmd.recipe != no_recipe)
            {
                if (cmd.type != building_type::processing_facility)
                    return corp_command_result::rejected_invalid;
                if (reg.get_recipe(cmd.recipe) == nullptr)
                    return corp_command_result::rejected_invalid;
            }
            entity_id built = null_entity;
            const construction_result r =
                construct_building(w, reg, cmd.corp, cmd.tile, cmd.type, cmd.target, built,
                                   cmd.recipe);
            if (r == construction_result::placed)
            {
                // The command's argument must survive into the world it created;
                // nothing else checks that the seam kept its word (BL-388).
                assert(cmd.recipe == no_recipe ||
                       w.buildings.at(built).recipe == cmd.recipe);
                if (out_building)
                    *out_building = built;
            }
            return map_construction(r);
        }

        case corp_verb::demolish:
        {
            // demolish_building validates existence + ownership and mutates only
            // on success; distinguish the refusal reasons for the log.
            if (w.buildings.find(cmd.subject) == w.buildings.end())
                return corp_command_result::rejected_invalid;
            if (!owns(w, cmd.corp, cmd.subject))
                return corp_command_result::rejected_not_owner;
            return demolish_building(w, cmd.corp, cmd.subject)
                       ? corp_command_result::applied
                       : corp_command_result::rejected_invalid;
        }

        case corp_verb::set_recipe:
        {
            const auto bit = w.buildings.find(cmd.subject);
            if (bit == w.buildings.end())
                return corp_command_result::rejected_invalid;
            if (!owns(w, cmd.corp, cmd.subject))
                return corp_command_result::rejected_not_owner;
            building_component& b = bit->second;
            if (b.type != building_type::processing_facility)
                return corp_command_result::rejected_invalid;
            if (reg.get_recipe(cmd.recipe) == nullptr)
                return corp_command_result::rejected_invalid;
            if (b.recipe == cmd.recipe)
                return corp_command_result::rejected_state; // no-op
            // BL-430: gated by economy.recipe_switch's cost/cooldown, through the
            // one implementation this seam shares with construction_panel's method
            // dropdown — the AI's dial_recipe margin-chase (corp_ai.cpp) reaches
            // here too, and pays the same cost, since it is the same seam.
            switch (try_switch_recipe(w, reg, cmd.corp, b, cmd.recipe))
            {
                case recipe_switch_result::applied:            return corp_command_result::applied;
                case recipe_switch_result::on_cooldown:         return corp_command_result::rejected_cooldown;
                case recipe_switch_result::insufficient_funds:  return corp_command_result::rejected_funds;
                case recipe_switch_result::invalid:             return corp_command_result::rejected_invalid;
                // BL-434 retraction: a cross-group target is not a legal switch at
                // all (see try_switch_recipe's comment) — same bucket as `invalid`,
                // the closest existing generic refusal, rather than new plumbing.
                case recipe_switch_result::cross_group:         return corp_command_result::rejected_invalid;
                // BL-588: reuses the SAME typed code the build door's
                // structure-level tech lock returns, so an agent cannot tell
                // a structure lock from a recipe lock apart on the seam.
                case recipe_switch_result::tech_locked:         return corp_command_result::rejected_tech_locked;
            }
            return corp_command_result::rejected_invalid;
        }

        case corp_verb::set_workforce:
        {
            const auto bit = w.buildings.find(cmd.subject);
            if (bit == w.buildings.end())
                return corp_command_result::rejected_invalid;
            if (!owns(w, cmd.corp, cmd.subject))
                return corp_command_result::rejected_not_owner;
            if (cmd.workforce < 0 || cmd.workforce > 200)
                return corp_command_result::rejected_invalid;
            building_component& b = bit->second;
            // A manual target PINS the dial — the press has always cleared
            // workforce_auto (selection_panel.cpp, construction_panel.cpp) and
            // the seam never did, so a command-driven player's target was
            // silently re-solved by the auto-solver on the next tick. So the
            // no-op test is "already there AND already pinned": re-asserting a
            // target the solver happens to be holding is a real change of
            // control, not a no-op. (BL-293; the dictionary described this
            // behaviour before the seam implemented it.)
            if (b.workforce_target == cmd.workforce && !b.workforce_auto)
                return corp_command_result::rejected_state;
            b.workforce_target = cmd.workforce;
            b.workforce_auto   = false;
            return corp_command_result::applied;
        }

        case corp_verb::set_workforce_auto:
        {
            const auto bit = w.buildings.find(cmd.subject);
            if (bit == w.buildings.end())
                return corp_command_result::rejected_invalid;
            if (!owns(w, cmd.corp, cmd.subject))
                return corp_command_result::rejected_not_owner;
            building_component& b = bit->second;
            if (b.workforce_auto)
                return corp_command_result::rejected_state; // already auto
            b.workforce_auto = true;
            return corp_command_result::applied;
        }

        case corp_verb::idle:
        case corp_verb::resume:
        {
            const auto bit = w.buildings.find(cmd.subject);
            if (bit == w.buildings.end())
                return corp_command_result::rejected_invalid;
            if (!owns(w, cmd.corp, cmd.subject))
                return corp_command_result::rejected_not_owner;
            building_component& b     = bit->second;
            const bool          want  = (cmd.verb == corp_verb::idle);
            if (b.decommissioned == want)
                return corp_command_result::rejected_state; // already there
            b.decommissioned = want;
            b.loss_streak    = 0;
            // An idled port/hub stops anchoring supply (is_supply_anchor);
            // resume restores it — either way the reach field is stale. Other
            // types change nothing cached, and the AI issues these at tick
            // rate (2026-08-12 — see invalidate_logistics_caches).
            if (building_affects_logistics(b.type))
                invalidate_logistics_caches(w);
            return corp_command_result::applied;
        }

        case corp_verb::place_road:
            return map_construction(place_road(w, reg, cmd.corp, cmd.tile, cmd.road_tier));

        case corp_verb::survey:
        {
            switch (dispatch_survey(w, cmd.subject, cmd.corp))
            {
                case survey_dispatch_result::success:            return corp_command_result::applied;
                case survey_dispatch_result::insufficient_funds: return corp_command_result::rejected_funds;
                case survey_dispatch_result::already_surveyed:
                case survey_dispatch_result::in_progress:        return corp_command_result::rejected_state;
                case survey_dispatch_result::invalid:            return corp_command_result::rejected_invalid;
            }
            return corp_command_result::rejected_invalid;
        }

        case corp_verb::hire_unit:
        {
            const auto& table = unit_roster_table();
            if (cmd.unit_type >= table.size())
                return corp_command_result::rejected_invalid;
            const roster_row& row = table[cmd.unit_type];

            // Re-check availability against the LIVE gate rather than trusting a
            // caller's cached list — mirrors construct_building's own re-validation
            // of placement rather than the tile the UI last showed as buildable.
            const auto avail = available_rows(w, cmd.corp, campaign_roster_band_for(reg.era()));
            if (std::find(avail.begin(), avail.end(), &row) == avail.end())
                return corp_command_result::rejected_invalid;

            if (w.tiles.find(cmd.tile) == w.tiles.end())
                return corp_command_result::rejected_invalid;

            // BL-325 S2: hire moves onto the base. cmd.tile must carry the corp's
            // own COMPLETED military_base — supersedes hire-anywhere (BL-324). A
            // base under construction (ticks_remaining > 0) or a decommissioned
            // one does not qualify, mirroring the completed-hub idiom
            // (supply_system.cpp's inland_logistics_hub check).
            entity_id muster_base = null_entity;
            {
                // BL-454: the WINNING base id is captured, not just a bool — it
                // becomes the unit's `muster_base`, the key the upkeep pass uses
                // to disband a unit whose base has been demolished. Lowest id
                // wins rather than first-found, because w.buildings is an
                // unordered_map and "first" is not a defined answer.
                for (const auto& [bid, bc] : w.buildings)
                {
                    if (bc.tile == cmd.tile && bc.type == building_type::military_base
                        && bc.ticks_remaining <= 0 && !bc.decommissioned
                        && owns(w, cmd.corp, bid))
                    {
                        if (muster_base == null_entity || bid < muster_base)
                            muster_base = bid;
                    }
                }
                if (muster_base == null_entity)
                    return corp_command_result::rejected_placement;
            }

            // BL-394: the credit cost. Row 0's gate is all-zero, so the
            // resource debit below tested and spent nothing for the cheapest
            // rows — a seam player raised ten free units in one tick (the
            // scorer's one-hire-per-eval cap is corp_ai policy, not a rule of
            // the game). Every row now costs credits: a floor plus a
            // power-scaled component, authored in economy.lua (§ military).
            // Checked BEFORE the resource debit so a refusal on either leg
            // leaves the corp wholly uncharged, preserving debit_hire_cost's
            // own all-or-nothing contract.
            const military_capability_params& mil = reg.military();
            const float hire_cost = mil.hire_base_cost
                + mil.hire_cost_per_power * static_cast<float>(row.power_mod);
            corporation_component& payer = w.corporations.at(cmd.corp);
            if (payer.balance < hire_cost)
                return corp_command_result::rejected_funds;

            if (!debit_hire_cost(w, cmd.corp, row))
                return corp_command_result::rejected_funds;
            payer.balance -= hire_cost;

            const entity_id unit = w.create_entity();
            // BL-459: no `strength` — it was set to the same value as `count`,
            // a literal duplicate kept in sync by hand. Strength is derived
            // (unit_strength, unit_roster.hpp) from count x the row's quality x
            // the supply factor, so a heavy row is genuinely worth more than a
            // levy at equal headcount and an unsupplied army is measurably
            // weaker in the resolver.
            w.units[unit] = unit_component{
                .position = cmd.tile,
                .owner    = cmd.corp,
                .count    = hire_batch_manpower,
                .type     = cmd.unit_type,
                .supply_factor_permille = 1000, // raised fully supplied (BL-454)
                .muster_base            = muster_base,
            };
            if (out_building)
                *out_building = unit;
            return corp_command_result::applied;
        }

        // --- BL-293: the order book -----------------------------------------
        // There is no `place_sell_order(...)` seam elsewhere to delegate to, the
        // way build delegates to construct_building — the order book had no
        // world-side home at all before this item. So the validation the Market
        // Ledger's form performs (a market on the body, a resource that market
        // prices, a positive quantity) is performed HERE, once, and the UI now
        // routes its press through this same path. That is the point: the
        // player's press and the AI's command cannot diverge if there is only one
        // implementation of what the press means.

        case corp_verb::place_sell_order:
        {
            const entity_id body = cmd.subject;
            if (w.bodies.find(body) == w.bodies.end())
                return corp_command_result::rejected_invalid;

            const entity_id mid = any_market_on_body(w, body);
            if (mid == null_entity)
                return corp_command_result::rejected_invalid; // "This body has no market."

            const std::size_t r = static_cast<std::size_t>(cmd.target);
            if (r >= resource_count)
                return corp_command_result::rejected_invalid;
            if (w.markets.at(mid).base_price[r] <= 0.0f)
                return corp_command_result::rejected_invalid; // unpriced here

            if (!(cmd.quantity > 0.0f) || cmd.floor_price < 0.0f)
                return corp_command_result::rejected_invalid;

            if (corp_order_count(w, cmd.corp) >= max_sell_orders_per_corp)
                return corp_command_result::rejected_state; // book full

            sell_order o;
            o.id          = w.allocate_order_id();
            o.corp        = cmd.corp;
            o.body        = body;
            o.resource    = cmd.target;
            o.quantity    = cmd.quantity;
            o.floor_price = cmd.floor_price;
            w.sell_orders.push_back(o); // appended: insertion order is time priority
            return corp_command_result::applied;
        }

        case corp_verb::remove_sell_order:
        {
            if (cmd.order == 0)
                return corp_command_result::rejected_invalid;

            const auto it = std::find_if(w.sell_orders.begin(), w.sell_orders.end(),
                                         [&](const sell_order& o) { return o.id == cmd.order; });
            // A nonexistent order and someone ELSE'S order answer identically
            // (BL-397). Distinguishing them (`rejected_invalid` vs
            // `rejected_not_owner`) made the result an existence oracle: order
            // ids are one global monotonic sequence, so sweeping the id space
            // mapped the whole book — whose orders exist, corp by corp. Any id
            // not in the caller's own book is simply invalid. In-process
            // callers only ever remove their own orders, so they never see the
            // collapsed case.
            if (it == w.sell_orders.end() || it->corp != cmd.corp)
                return corp_command_result::rejected_invalid;

            // Erase by id, never by index: every surviving order keeps its handle,
            // so a removal cannot invalidate a command already composed against
            // another order.
            w.sell_orders.erase(it);
            return corp_command_result::applied;
        }

        case corp_verb::request_quote:
        {
            if (w.corporations.find(cmd.counterparty) == w.corporations.end())
                return corp_command_result::rejected_invalid;
            if (cmd.counterparty == cmd.corp)
                return corp_command_result::rejected_invalid; // no self-contracting
            if (!(cmd.quantity > 0.0f))
                return corp_command_result::rejected_invalid;

            // Q2, in the order the design names them: capacity, input access,
            // embargo, reputation.
            entity_id body = null_entity;
            if (!supplier_has_capacity(w, reg, cmd.counterparty, cmd.target, body))
                return corp_command_result::rejected_no_capacity;
            if (!supplier_has_input_access(w, reg, cmd.counterparty, cmd.target, body))
                return corp_command_result::rejected_no_input_access;

            const auto embargo_it = w.corp_embargo_conditions.find(cmd.counterparty);
            if (embargo_it != w.corp_embargo_conditions.end() &&
                !evaluate(embargo_it->second, w, cmd.corp))
                return corp_command_result::rejected_embargo;

            // BL-546: reputation is a VIEW of sentiment's Trust dimension, not
            // a store of its own. THE FLOOR IS NO LONGER PERMANENT (BL-391) —
            // the row decays toward neutral, so a pair below the floor climbs
            // back out with time and this refusal is a temporary state. It is
            // the ONLY consumer of the floor in the tree; any future one must
            // read it the same way, as a condition of today and not a verdict.
            const float reputation = w.procurement_reputation(cmd.corp, cmd.counterparty);
            if (reputation < reg.procurement().reputation_floor)
                return corp_command_result::rejected_reputation;

            // Price: the supplier's local market price (or base_price with no
            // live resolution yet), same "what the good actually costs here"
            // reading run_construction's auto-buy uses — LESS the commitment
            // discount (BL-392). Spot with a forfeitable deposit on top is
            // strictly worse than buying on the market, which is why the
            // measured 20-unit iron round trip settled at -0.14 credits and no
            // rational agent ever contracted. Lead time: derived, not authored
            // (BL-350's own framing) — a bigger order takes longer, a supplier
            // with real capacity is faster.
            const entity_id mid = any_market_on_body(w, body);
            const float spot_price = (mid != null_entity)
                ? (w.markets.at(mid).price[static_cast<std::size_t>(cmd.target)] > 0.0f
                       ? w.markets.at(mid).price[static_cast<std::size_t>(cmd.target)]
                       : w.markets.at(mid).base_price[static_cast<std::size_t>(cmd.target)])
                : 0.0f;

            const procurement_params& pp = reg.procurement();

            // Asymptotic in the order size, so no order however large drives the
            // price to zero, and the half-quantity is where the curve's knee sits.
            const float half_q    = std::max(1.0f, pp.volume_discount_half_quantity);
            const float discount  = pp.volume_discount_max *
                                    (cmd.quantity / (cmd.quantity + half_q));
            const float unit_price = spot_price * (1.0f - std::clamp(discount, 0.0f, 0.95f));

            // BL-392 fault 1: deliver to the BUYER's body. Crediting the
            // supplier's body left the stock somewhere the buyer had no
            // processor reservation, and the auto-surplus path liquidated it in
            // the tick it landed — twenty completed contracts, not one
            // detectable delivery.
            const entity_id delivery = [&]{
                const entity_id d = buyer_delivery_body(w, cmd.corp);
                return d != null_entity ? d : body;
            }();

            // Carriage, when the goods actually cross bodies. Paid to the
            // supplier at delivery, so it is a transfer between two balances and
            // conserves credits exactly.
            const float freight = (delivery != body)
                ? spot_price * cmd.quantity * pp.offbody_freight_fraction
                : 0.0f;

            // BL-392 fault 3: the SUPPLIER's throughput for this good, not a
            // global processing constant. Floored at a token rate so a supplier
            // whose last producing site vanished between the capacity check and
            // here still quotes a finite term rather than dividing by zero.
            const float throughput =
                std::max(0.01f, supplier_throughput(w, reg, cmd.counterparty, cmd.target));
            // Clamp before narrowing. A float-to-int conversion whose value does
            // not fit the destination is undefined, not saturating, and
            // `quantity` is caller-supplied — the only guard above is
            // `> 0.0f`, which an arbitrarily large order passes. A quote whose
            // lead time is INT_MIN would then be paced every tick for the rest
            // of the campaign. The ceiling is a century of quarters: far past any
            // real contract, and finite.
            constexpr double max_lead_ticks = 400.0;
            const double raw_lead = static_cast<double>(pp.base_lead_ticks) *
                                    std::ceil(static_cast<double>(cmd.quantity) / throughput);
            // Floored at 1: a contract that completes in zero ticks would pay
            // its whole remainder in a single tick and deliver instantly, which
            // is a spot purchase wearing a contract's name.
            const int lead_time =
                static_cast<int>(std::clamp(raw_lead, 1.0, max_lead_ticks));

            procurement_quote q;
            q.id              = w.allocate_procurement_id();
            q.buyer           = cmd.corp;
            q.supplier        = cmd.counterparty;
            q.body            = body;
            q.delivery_body   = delivery;
            q.resource        = cmd.target;
            q.quantity        = cmd.quantity;
            q.unit_price      = unit_price;
            q.lead_time_ticks = lead_time;
            q.freight_cost    = freight;
            w.procurement_quotes.push_back(q);
            return corp_command_result::applied;
        }

        case corp_verb::accept_quote:
        {
            if (cmd.order == 0)
                return corp_command_result::rejected_invalid;
            const auto it = std::find_if(w.procurement_quotes.begin(), w.procurement_quotes.end(),
                                         [&](const procurement_quote& q) { return q.id == cmd.order; });
            if (it == w.procurement_quotes.end())
                return corp_command_result::rejected_invalid; // no such quote (or already consumed)
            if (it->buyer != cmd.corp)
                return corp_command_result::rejected_not_owner;

            // BL-392: the contract's total is goods PLUS carriage — the buyer is
            // paying for delivery to their own body, and the freight is the
            // honest price of that distance.
            const float total   = it->quantity * it->unit_price + it->freight_cost;
            const float deposit = total * reg.procurement().deposit_fraction;
            corporation_component& buyer = w.corporations.at(cmd.corp);
            if (buyer.balance < deposit)
                return corp_command_result::rejected_funds;
            // The supplier must still exist to be paid. Every credit this seam
            // moves is a TRANSFER; there is no counterparty-less debit here.
            const auto sit = w.corporations.find(it->supplier);
            if (sit == w.corporations.end())
                return corp_command_result::rejected_invalid;

            procurement_contract c;
            c.id              = w.allocate_procurement_id();
            c.buyer           = it->buyer;
            c.supplier        = it->supplier;
            c.body            = it->body;
            c.delivery_body   = it->delivery_body;
            c.resource        = it->resource;
            c.quantity        = it->quantity;
            c.unit_price      = it->unit_price;
            c.lead_time_ticks = it->lead_time_ticks;
            c.ticks_elapsed   = 0;
            c.deposit_paid    = deposit;
            c.freight_cost    = it->freight_cost;

            // Debit and credit in the same breath, same magnitude: the deposit
            // moves from the buyer to the supplier. It used to leave the buyer's
            // balance and reach nobody, so accepting a quote destroyed credits
            // outright.
            buyer.balance       -= deposit;
            sit->second.balance += deposit;
            w.procurement_contracts.push_back(c);
            w.procurement_quotes.erase(it); // consumed: cannot accept twice
            return corp_command_result::applied;
        }

        case corp_verb::cancel_contract:
        {
            if (cmd.order == 0)
                return corp_command_result::rejected_invalid;
            const auto it = std::find_if(w.procurement_contracts.begin(), w.procurement_contracts.end(),
                                         [&](const procurement_contract& c) { return c.id == cmd.order; });
            if (it == w.procurement_contracts.end())
                return corp_command_result::rejected_invalid;
            if (it->buyer != cmd.corp)
                return corp_command_result::rejected_not_owner;

            // Forfeits the deposit (no refund) and moves reputation down —
            // BL-350 Q1/Q3. Erased, not flagged: a cancelled contract has
            // nothing left to pace.
            //
            // BL-546: the move is now one `contract_cancelled` occurrence
            // folded into the substrate, at the weight `economy.lua` authors
            // (seeded from `reputation_on_cancel`, so the magnitude is
            // unchanged). Applied here and now, exactly as the old `+=` was.
            w.note_conduct(reg.sentiment(), it->buyer, it->supplier,
                           sentiment_factor_kind::contract_cancelled);
            w.procurement_contracts.erase(it);
            return corp_command_result::applied;
        }

        // --- BL-452: the logistics layer ------------------------------------
        // Layer 5 had no player verb at all: supply_system, logistics, four
        // rendering paths and the only coupling between two markets' prices,
        // entirely automatic. These two verbs are the seam onto it.
        //
        // dispatch_convoy is deliberately NOT a second implementation of a
        // dispatch. It is the auto-dispatcher's own body with the shortfall
        // scan removed — price_convoy_leg + commit_convoy (supply_system.hpp),
        // the same two calls dispatch_convoys makes with the same arguments.
        // Anything the player's convoy does differently from a rival's would be
        // a divergence, not a feature.

        case corp_verb::dispatch_convoy:
        {
            // UNTRUSTED INPUT BOUNDARY (io-standing-rules.md, 2026-08-14). Every
            // field is validated as the value that lands in the destination,
            // before anything is read from it, and the whole command is refused
            // on violation — never clamped, truncated or wrapped. Nothing below
            // mutates until the single commit at the end.
            const auto src_it = w.markets.find(cmd.subject);
            if (src_it == w.markets.end())
                return corp_command_result::rejected_invalid; // subject is not a market
            const auto dest_it = w.markets.find(cmd.counterparty);
            if (dest_it == w.markets.end())
                return corp_command_result::rejected_invalid; // counterparty is not a market

            const std::size_t r = static_cast<std::size_t>(cmd.target);
            if (r >= resource_count)
                return corp_command_result::rejected_invalid;

            // Finiteness FIRST: an infinity or a NaN passes `> 0.0f` (or fails
            // it) without meaning anything, and would reach the cost product
            // and the pool debit as a value no later comparison can catch.
            if (!std::isfinite(cmd.quantity) || !(cmd.quantity > 0.0f))
                return corp_command_result::rejected_invalid;

            // The corp must actually HOLD the cargo. The source pool is keyed
            // by (corp, body), so reading the acting corp's own pool is the
            // ownership check — there is no way to name someone else's stock.
            const entity_id src_body = src_it->second.body;
            const auto      pit      = w.corp_body_pools.find({cmd.corp, src_body});
            const float     stock    =
                (pit != w.corp_body_pools.end()) ? pit->second.quantities[r] : 0.0f;
            if (cmd.quantity > stock)
                return corp_command_result::rejected_state; // the goods are not there

            // Price the leg through the shared path. `viable == false` is the
            // lane refusing the haul: no production anchor, no reachable route,
            // no launchpad on the source body, no propellant to launch with, or
            // a cost that is not a finite number.
            const logistics_nodes nodes = collect_logistics_nodes(w);
            const convoy_leg      leg   = price_convoy_leg(
                w, reg, nodes, cmd.corp, src_body, cmd.counterparty, r, cmd.quantity,
                reg.logistics_cost(convoy_mode::space));
            if (!leg.viable)
                return corp_command_result::rejected_placement;

            // The solvency gate, and (BL-597) the passive-LP admissibility
            // gate, live inside commit_convoy, in one place for both
            // callers; a refusal there mutates nothing. `refused_no_lp`
            // distinguishes "no passive LP at the source anchor" from plain
            // insolvency so the player sees the right reason.
            bool refused_no_lp = false;
            if (!commit_convoy(w, reg, cmd.corp, src_body, cmd.subject, cmd.counterparty,
                               r, cmd.quantity, leg, nullptr, &refused_no_lp))
                return refused_no_lp ? corp_command_result::rejected_no_lp
                                      : corp_command_result::rejected_funds;
            return corp_command_result::applied;
        }

        case corp_verb::hold_convoy:
        {
            // NOT a cancel, and there is no verb that is one. Cargo left the
            // source pool at dispatch (SUPPLY.md § Convoy entity: "goods in
            // transit are committed"), so a cancel would have to invent a
            // return leg or mint the goods back at the source — neither is in
            // scope. A held convoy stops advancing (advance_convoys skips it)
            // and pays nothing further, because the haul was paid once at
            // dispatch. Issuing the verb again releases it: the toggle rule.
            if (cmd.order == 0)
                return corp_command_result::rejected_invalid;

            const auto it = std::find_if(w.convoys.begin(), w.convoys.end(),
                                         [&](const convoy_component& c) { return c.id == cmd.order; });
            // A convoy that does not exist and one belonging to another corp
            // answer identically, for BL-397's reason: convoy ids are one
            // global monotonic sequence, so a distinguishable rejection would
            // let an id sweep map every rival's cargo in flight — precisely the
            // intelligence the BL-068 visibility rule keeps private.
            if (it == w.convoys.end() || it->corp != cmd.corp)
                return corp_command_result::rejected_invalid;
            if (it->arrived)
                return corp_command_result::rejected_state; // already landed; nothing to hold

            it->held = !it->held;
            return corp_command_result::applied;
        }

        // --- BL-448: corp stance -------------------------------------------
        // Data model + verbs only. `is_hostile`/`are_friends`/etc. and the
        // mutators themselves live in stance.{hpp,cpp}; the seam here is only
        // the untrusted-boundary translation from stance_result to
        // corp_command_result (io-standing-rules.md — reject the whole
        // command on violation, never partial-apply; stance.cpp's mutators
        // already guarantee this on their own, so there is nothing more to
        // validate here beyond passing the right corp on each side).

        case corp_verb::declare_hostile:
        {
            const stance_result r = declare_hostile(w, cmd.corp, cmd.counterparty);
            return (r == stance_result::applied) ? corp_command_result::applied
                 : (r == stance_result::rejected_invalid) ? corp_command_result::rejected_invalid
                                                            : corp_command_result::rejected_state;
        }

        case corp_verb::offer_friendship:
        {
            const stance_result r = offer_friendship(w, cmd.corp, cmd.counterparty);
            return (r == stance_result::applied) ? corp_command_result::applied
                 : (r == stance_result::rejected_invalid) ? corp_command_result::rejected_invalid
                                                            : corp_command_result::rejected_state;
        }

        case corp_verb::accept_friendship:
        {
            // cmd.corp is the ACCEPTOR; cmd.counterparty is the corp whose
            // offer is being accepted — the offer was made counterparty ->
            // corp, so the underlying call takes (offerer, target) as
            // (cmd.counterparty, cmd.corp).
            const stance_result r = accept_friendship(w, cmd.counterparty, cmd.corp);
            return (r == stance_result::applied) ? corp_command_result::applied
                 : (r == stance_result::rejected_invalid) ? corp_command_result::rejected_invalid
                                                            : corp_command_result::rejected_state;
        }

        case corp_verb::return_to_neutral:
        {
            const stance_result r = return_to_neutral(w, cmd.corp, cmd.counterparty);
            return (r == stance_result::applied) ? corp_command_result::applied
                 : (r == stance_result::rejected_invalid) ? corp_command_result::rejected_invalid
                                                            : corp_command_result::rejected_state;
        }

        // --- BL-470: the unit march seam -------------------------------------
        // Extends THIS seam (BL-470's ruling 2) — no parallel unit_command
        // type. A rejection mutates nothing (io-standing-rules.md's untrusted-
        // seam rule): every check below runs before the single `order`/`erase`
        // write that applies the verb.

        case corp_verb::march_unit:
        {
            const auto uit = w.units.find(cmd.subject);
            if (uit == w.units.end())
                return corp_command_result::rejected_invalid;
            if (uit->second.owner != cmd.corp)
                return corp_command_result::rejected_not_owner;

            // BL-467 (2026-08-21) FILLED THE GAP THIS COMMENT RESERVED. The
            // "reject march while in-battle" rule from BL-470's design was
            // unreachable while nothing recorded that a unit was in contact;
            // `world::battles` now does. A unit in a fight may not be given a
            // new movement order — walking away from contact is a WITHDRAWAL,
            // which is priced, not a march, which is not. The verb for it is
            // corp_verb::withdraw_from_battle.
            //
            // Note the state is on the BATTLE, not on unit_component: the flag
            // that comment predicted would have been a second place for the same
            // fact to live, and a second place to forget to clear.
            if (unit_in_battle(w, cmd.subject))
                return corp_command_result::rejected_state;

            // BL-511: the destination is a PROVINCE, not a tile. THIS is the
            // domain check the untrusted-input rule asks for and the one a
            // wire range gate cannot stand in for: `find` binary-searches the
            // built partition, so an id that merely FITS a uint32 — including
            // the 0 default of an omitted field — is refused here rather than
            // being coerced into some nearby province.
            const province* dp = w.provinces.find(cmd.province);
            if (dp == nullptr || dp->tiles.empty())
                return corp_command_result::rejected_invalid;

            // BL-516 RESTORED A TEST THIS VERB HAD BEEN ABLE TO DROP. Until
            // water gained provinces, "the partition covers land only by
            // construction" meant a province id could never name water and no
            // check was needed. Sea provinces exist now, they are addressable,
            // and units are land-bound — so marching into one is refused HERE,
            // explicitly, rather than left to fail obscurely in the path solve.
            // Rejection mutates nothing, as every path above and below does.
            if (province_kind_of(w, *dp) != province_kind::land)
                return corp_command_result::rejected_invalid;

            const auto sit = w.tiles.find(uit->second.position);
            if (sit == w.tiles.end())
                return corp_command_result::rejected_invalid; // unit's own tile is gone

            if (dp->body != sit->second.body)
                return corp_command_result::rejected_invalid; // BL-470's ruling 1: intra-body path march only

            if (w.provinces.province_of(uit->second.position) == cmd.province)
                return corp_command_result::rejected_state; // already there — halt_unit is the stop verb

            // Solve to the LOWEST-ID reachable member tile. `province::tiles`
            // is ascending by contract, so first-reachable-wins is a stable,
            // container-independent choice; and because the march ENDS on
            // entering the province (run_unit_march), which member tile was
            // picked only decides the route's tail, never where the unit
            // stops. Every lookup below is read-only — nothing is written
            // until the single `order` assignment at the end, so a rejection
            // on any path here mutates nothing.
            entity_id       chosen = null_entity;
            std::vector<entity_id> chosen_path;
            for (const entity_id cand : dp->tiles)
            {
                if (cand == uit->second.position)
                    continue; // degenerate; the already-there test above covers the real case
                const logistics_path& p = intra_body_path(w, sit->second.body, uit->second.position, cand);
                if (p.reachable && !p.tiles.empty())
                {
                    chosen      = cand;
                    chosen_path = p.tiles;
                    break;
                }
            }
            if (chosen == null_entity)
                return corp_command_result::rejected_invalid; // no member tile is reachable

            movement_order mo;
            mo.dest          = chosen;
            mo.dest_province = cmd.province;
            mo.path = chosen_path; // canonical (lo->hi) order — see world.hpp's logistics_path comment
            if (mo.path.front() != uit->second.position)
                std::reverse(mo.path.begin(), mo.path.end());
            mo.next_index = 1; // path[0] is the tile the unit already occupies
            mo.progress   = 0.0f;
            uit->second.order = mo;
            return corp_command_result::applied;
        }

        case corp_verb::halt_unit:
        {
            const auto uit = w.units.find(cmd.subject);
            if (uit == w.units.end())
                return corp_command_result::rejected_invalid;
            if (uit->second.owner != cmd.corp)
                return corp_command_result::rejected_not_owner;
            if (uit->second.order.dest == null_entity)
                return corp_command_result::rejected_state; // nothing to halt
            uit->second.order = movement_order{};
            return corp_command_result::applied;
        }

        case corp_verb::disband_unit:
        {
            const auto uit = w.units.find(cmd.subject);
            if (uit == w.units.end())
                return corp_command_result::rejected_invalid;
            if (uit->second.owner != cmd.corp)
                return corp_command_result::rejected_not_owner;
            // BL-573: a unit committed to a LIVE mercenary contract cannot be
            // disbanded out from under it — the committed-unit lock the
            // item's own requirement names. A unit whose contract has already
            // gone terminal (completed/failed/abandoned) is free again; the
            // check reads `state` rather than needing a separate release step.
            for (const mercenary_contract& c : w.mercenary_contracts)
            {
                if (c.state != mercenary_contract_state::active)
                    continue;
                if (mercenary_contract_has_unit(c, cmd.subject))
                    return corp_command_result::rejected_state;
            }
            w.units.erase(uit); // no refund — manpower walks away (BL-470's design)
            return corp_command_result::applied;
        }

        case corp_verb::withdraw_from_battle:
        {
            // The DOMAIN CHECK the untrusted-input rule asks for, and the reason
            // it cannot be a wire range gate: `province` is a real id in a built
            // partition, so a value that merely fits a uint32 — including the 0
            // default of an omitted field — must be refused rather than coerced
            // into some nearby province. request_withdraw resolves against the
            // live battle list and mutates NOTHING when it finds no match, so a
            // rejection here leaves the world byte-identical.
            if (!request_withdraw(w, cmd.corp, cmd.province, cmd.counterparty))
                return corp_command_result::rejected_state;
            return corp_command_result::applied;
        }

        // --- BL-573: the mercenary-contract seam ----------------------------
        // UNTRUSTED INPUT BOUNDARY (io-standing-rules.md). Every field is
        // validated as the value that lands in the destination, before
        // anything is read from it, and the whole command is refused on
        // violation. Nothing below mutates until the single block of writes
        // at the end of each case.

        case corp_verb::accept_offer:
        {
            if (cmd.order == 0)
                return corp_command_result::rejected_invalid;
            const auto oit = std::find_if(w.mercenary_offers.begin(), w.mercenary_offers.end(),
                                          [&](const mercenary_offer& o) { return o.id == cmd.order; });
            if (oit == w.mercenary_offers.end())
                return corp_command_result::rejected_invalid; // no such offer (or already consumed)

            // `counterparty` names the offer's CLIENT NATION for this verb,
            // not a corp — the seam's counterparty check forks on verb
            // (BL-573's own design text). Validated against w.nations, not
            // w.corporations: a value that merely fits an entity_id must
            // still name a REAL nation, and it must be the one this offer
            // actually belongs to — an offer id is not secret, so a caller
            // must also know (or be told) who it targets.
            if (w.nations.find(cmd.counterparty) == w.nations.end())
                return corp_command_result::rejected_invalid;
            if (oit->client != cmd.counterparty)
                return corp_command_result::rejected_invalid;

            // Expired: this offer's eventual contract could not possibly
            // complete in time. A self-contained check against the offer's
            // OWN deadline field — no contract_offer_params dependency needed
            // here, and none of the tunables that decide an OFFER's own TTL
            // (a different clock — see mercenary_offer::deadline's own
            // comment) belong on this seam.
            if (w.current_econ_tick >= oit->deadline)
                return corp_command_result::rejected_invalid;

            // Not yet fully escrowed: CONTRACTS.md's "the fee this offer's
            // escrow must clear before the contract it names is postable".
            // A partially-funded offer is not yet a real acceptable contract.
            if (oit->offer_escrow < oit->fee)
                return corp_command_result::rejected_state;

            // The committed force: every named slot must be a REAL unit,
            // OWNED by the acting corp, and not already committed to another
            // ACTIVE contract (no double-commit — the item's own requirement).
            // At least one unit must be named; an empty force is refused
            // rather than accepted into a contract nothing can prosecute.
            std::array<entity_id, mercenary_contract_max_units> committed{};
            std::size_t                                          committed_n = 0;
            for (const entity_id u : cmd.units)
            {
                if (u == null_entity)
                    continue;
                const auto uit = w.units.find(u);
                if (uit == w.units.end())
                    return corp_command_result::rejected_invalid;
                if (uit->second.owner != cmd.corp)
                    return corp_command_result::rejected_not_owner;
                for (const mercenary_contract& c : w.mercenary_contracts)
                {
                    if (c.state != mercenary_contract_state::active)
                        continue;
                    if (mercenary_contract_has_unit(c, u))
                        return corp_command_result::rejected_state; // already committed
                }
                committed[committed_n++] = u;
            }
            if (committed_n == 0)
                return corp_command_result::rejected_invalid;

            // The supplier side of the transfer must still exist — every
            // credit this seam moves is a TRANSFER, never a debit that reaches
            // nobody (the accept_quote precedent, above).
            const auto cit = w.corporations.find(cmd.corp);
            if (cit == w.corporations.end())
                return corp_command_result::rejected_invalid;

            // --- everything above is a pure check; mutation starts here -----
            //
            // The deposit is paid straight from the offer's own escrow, NOT
            // via a fresh nation-budget claim: `derive_contract_offers`
            // (nation_step.cpp) already debited `oit->client`'s treasury,
            // tick by tick, as the escrow accumulated toward `fee` — the full
            // fee has therefore ALREADY left the nation's books by the time an
            // offer is fully escrowed (the precondition above). Routing the
            // deposit (or, on completion, the remainder) through
            // run_national_budget's claim/gather machinery a second time
            // would debit the SAME nation's treasury for the SAME contract
            // twice — a real conservation leak, not a stylistic choice. Both
            // halves of the split payment are therefore direct transfers,
            // exactly like `run_nation_garrison_upkeep`'s "direct expenditure"
            // precedent for a line with no corp claimant of its own.
            const float deposit = oit->fee * reg.procurement().deposit_fraction;

            mercenary_contract mc;
            mc.id             = w.allocate_contract_id();
            mc.client         = oit->client;
            mc.contractor     = cmd.corp;
            mc.template_index = oit->template_index;
            mc.province       = oit->target_province;
            mc.fee            = oit->fee;
            mc.deposit_paid   = deposit;
            mc.deadline       = oit->deadline;
            mc.accepted_tick  = w.current_econ_tick;
            for (std::size_t i = 0; i < committed_n; ++i)
                mc.units[i] = committed[i];
            mc.state = mercenary_contract_state::active;

            cit->second.balance += deposit;
            w.mercenary_contracts.push_back(mc);
            w.mercenary_offers.erase(oit); // consumed: cannot accept twice
            return corp_command_result::applied;
        }

        case corp_verb::abandon_contract:
        {
            if (cmd.order == 0)
                return corp_command_result::rejected_invalid;
            const auto it = std::find_if(w.mercenary_contracts.begin(), w.mercenary_contracts.end(),
                                         [&](const mercenary_contract& c) { return c.id == cmd.order; });
            if (it == w.mercenary_contracts.end())
                return corp_command_result::rejected_invalid;
            if (it->contractor != cmd.corp)
                return corp_command_result::rejected_not_owner;
            if (it->state != mercenary_contract_state::active)
                return corp_command_result::rejected_state; // already terminal

            // Same money outcome as a failure — the deposit already paid at
            // accept_offer is not clawed back, and the reserved remainder is
            // simply never disbursed (CONTRACTS.md § Q2: "you are not paid
            // for trying"). A DISTINCT, LESSER sentiment magnitude is what
            // tells the two apart: the contractor CHOSE this one. Reuses
            // `contract_cancelled` — the procurement seam's own "walked away
            // early" kind — rather than minting a mercenary-only twin: the
            // conduct it names ("the counterparty backed out") is identical
            // across both contract families, and CONTRACTS.md's Q2 table
            // already reads "cancelled" as this state's other name.
            it->state = mercenary_contract_state::abandoned;
            w.note_conduct(reg.sentiment(), it->client, it->contractor,
                           sentiment_factor_kind::contract_cancelled);
            return corp_command_result::applied;
        }

        case corp_verb::raze_centre:
        {
            // BL-616/BL-624 (docs/economy/POPULATION.md § Growth, decline and
            // razing): the deliberate destruction act. Passive decline only
            // ever shrinks a centre; razing one is an agent's choice, in
            // occupation, and it should be rare because the occupier almost
            // always prefers to occupy. Since BL-624 (razed settlement tier)
            // it DEMOTES rather than erases — see below.
            const auto cit = w.population_centres.find(cmd.subject);
            if (cit == w.population_centres.end())
                return corp_command_result::rejected_invalid;
            if (cit->second.razed)
                return corp_command_result::rejected_state; // already ash — nothing to raze
            const auto tit = w.population_centre_tile.find(cmd.subject);
            if (tit == w.population_centre_tile.end())
                return corp_command_result::rejected_invalid; // no tile record — corrupt centre
            const auto tc_it = w.tiles.find(tit->second);
            if (tc_it == w.tiles.end())
                return corp_command_result::rejected_invalid;
            const entity_id body = tc_it->second.body;

            // OCCUPATION PRECONDITION (first cut, stated in BL-616's report):
            // the acting corp owns at least one unit whose position tile lies
            // on the centre's body. A unit is the only mobile military presence
            // and is complete from the moment hire_unit raises it (which itself
            // required a COMPLETED military_base), so "completed military
            // presence" reduces to unit-on-body. Pure existence predicate over
            // w.units — order-independent, so the unordered walk is safe.
            bool present = false;
            for (const auto& [uid, uc] : w.units)
            {
                if (uc.owner != cmd.corp)
                    continue;
                const auto ut = w.tiles.find(uc.position);
                if (ut != w.tiles.end() && ut->second.body == body)
                {
                    present = true;
                    break;
                }
            }
            if (!present)
                return corp_command_result::rejected_state; // no occupation, no razing

            // DEMOTE, never erase (BL-624, razed settlement tier; Ben,
            // 2026-08-25: "keep razed settlement as a settlement tier, so
            // it's cheap to rebuild there"). Population zeroed and the razed
            // flag set; the entity, its name, its tile record and its urban
            // land-use stamp (BL-612) all STAY — razed ground reads as
            // historied, the extraction it blocked stays blocked, and the
            // centre still anchors its province (scale set to 1 so the
            // strictly-positive anchor-scale scan still finds it; a ruin can
            // be captured, so razing never deletes a conquest handle).
            // `w.province_holder` is NOT touched here: the holder is live
            // state seeded from the anchor's nation (BL-611), and the
            // conquest consequence of taking an anchor belongs to BL-518,
            // not this verb. The growth pass re-settles a razed centre at a
            // reduced gate (economy_system.cpp § k_resettle_window_ticks).
            cit->second.razed              = true;
            cit->second.population         = 0;
            cit->second.scale              = 1;
            cit->second.growth_accumulator = 0;
            return corp_command_result::applied;
        }

        case corp_verb::buy_corporation:
        {
            // BL-628, docs/economy/FINANCE.md § Whole-firm acquisition. The whole
            // firm, never a fractional stake: there is no equity relation, no
            // share count, no controlling-holder threshold and no dividend split
            // anywhere in the model, so the verb moves a corporation in one step
            // or not at all.
            //
            // EVERY CHECK BELOW RUNS BEFORE ANY MUTATION, and they are ordered
            // cheapest-and-most-structural first. R2/R6 both turn on this: a
            // refusal must leave the world byte-identical, and the one verb that
            // erases an actor is the worst possible place to discover a
            // half-applied command.
            const entity_id target = cmd.counterparty;

            // (1) Domain. `counterparty` is the field the wire already parses and
            // range-checks against entity_id's own domain (agent_protocol.cpp);
            // this is the seam's half of that pair, which the wire cannot do —
            // fitting is not existing.
            if (target == null_entity || target == cmd.corp)
                return corp_command_result::rejected_invalid;
            const auto tit = w.corporations.find(target);
            if (tit == w.corporations.end())
                return corp_command_result::rejected_invalid;

            // (2) THE PLAYER'S CORP IS NOT BUYABLE THROUGH THIS SEAM.
            //
            // FINANCE.md's "who may be bought" rule is about ownership CLASS and
            // says nothing about the player, because the design is written from
            // the buyer's side. Read literally it would let a public player corp
            // be erased — and erasing `w.player_entity`'s corporation leaves that
            // field dangling, which is precisely the failure R4 exists to prevent,
            // quite apart from ending the campaign without a rule granting it.
            // Refused here rather than in `corp_ai.cpp` because the seam is where
            // an invariant belongs: a scorer-side guard would not bind a wire
            // caller. `is_player` OR the anchor id, since spectator mode (BL-409)
            // degrades the flag while the anchor stays.
            //
            // ASSUMPTION, flagged for Ben: if the intended design is that a rival
            // CAN buy the player out, this gate is the line to delete, and the
            // dangling `player_entity` needs a rule of its own.
            if (tit->second.is_player || target == w.player_entity)
                return corp_command_result::rejected_state;

            // (3) The class gate. A public firm files, so it can be priced; a
            // private or closed one cannot be priced and has no negotiation verb.
            if (tit->second.ownership_class != ownership_class::publicly_held)
                return corp_command_result::rejected_state;

            // (4) It must actually have filed. A public corporation that has not
            // yet reached its first `apply_budget` has no book value, no trailing
            // net and no disclosed record at all — pricing it at `max(0, balance)`
            // would be inventing a price from an empty ledger, and pricing it at
            // zero would make every brand-new public firm free. Refused instead.
            if (tit->second.returns.empty())
                return corp_command_result::rejected_state;

            const float price = corp_acquisition_price(tit->second, reg.acquisition().multiple);
            // Defence in depth against a NaN reaching a balance: the authored
            // multiple is validated finite at load and every input is a
            // serialised float, so this should be unreachable — but a NaN price
            // would pass a `balance < price` test and silently poison the
            // acquirer's balance forever.
            if (!std::isfinite(price))
                return corp_command_result::rejected_invalid;

            // (5) The solvency gate, exactly as every other spend takes it
            // (io-standing-rules § "Availability is cash-free; spending is not").
            corporation_component& acq = w.corporations.at(cmd.corp);
            if (acq.balance < price)
                return corp_command_result::rejected_funds;

            // --- COMMIT. Nothing below may fail. -----------------------------
            // The price is a SINK, not a transfer: a public firm's sellers are a
            // diffuse shareholder base, not a modelled actor, so there is nobody
            // to credit. Same treatment construction already gives a build cost
            // (a levy is explicitly a transfer BECAUSE an ordinary spend is not).
            acq.balance -= price;
            dissolve_into(w, cmd.corp, target);

            // (6) The political cost. `equity_taken` — "the observer wanted a firm
            // and the subject took it" (RELATIONS.md § The factors). Every corp
            // that still exists and is not the acquirer is an observer: the
            // acquisition market is open, so any of them could have bought the
            // firm, and the design's own sentence puts the cost "on the rivals who
            // wanted the same firm". Modelling WHICH rival wanted it would need a
            // want the model does not carry.
            //
            // A sorted walk over `w.corporations` keys, not the unordered map's
            // own order — the fold is order-independent by construction
            // (sentiment.cpp canonicalises its batch), but the walk that GATHERS
            // the events must still be deterministic. Weight is authored and
            // currently zero, so this is inert until `economy.sentiment.factors`
            // gives `equity_taken` a number; that inertness is BL-545's rule, not
            // an omission.
            {
                std::vector<entity_id> observers;
                observers.reserve(w.corporations.size());
                for (const auto& kv : w.corporations)
                    if (kv.first != cmd.corp)
                        observers.push_back(kv.first);
                std::sort(observers.begin(), observers.end());
                for (const entity_id o : observers)
                    w.note_conduct(reg.sentiment(), o, cmd.corp,
                                   sentiment_factor_kind::equity_taken, 1.0f);
            }

            return corp_command_result::applied;
        }
    }
    return corp_command_result::rejected_invalid;
}
