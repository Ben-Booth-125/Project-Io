#include "nation_step.hpp"

#include "components.hpp"
#include "condition_set.hpp" // BL-573: evaluate_condition, the contract predicate
#include "economy_system.hpp"
#include "hex_neighbors.hpp" // BL-572: border-tile adjacency, mirroring nation_generation.cpp's own
#include "logistics.hpp"     // BL-572: body_tile_grid, for the same
#include "nation_ai.hpp"
#include "nation_budget.hpp"
#include "nation_generation.hpp" // BL-572: garrison_strength_in
#include "province.hpp"          // BL-572: province_holder_for, province lookup
#include "recipe_registry.hpp"
#include "space_programme.hpp"   // BL-644: the space_programme line's consumer
#include "survey_system.hpp"
#include "unit_roster.hpp" // BL-571: resolve_unit_upkeep, unit_upkeep_params
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <vector>

void run_nation_step(world& w, const recipe_registry& reg, economy_report& report,
                     int econ_tick)
{
    // ---- 1. Score: the nations due this tick overwrite their slot -----------
    const nation_ai_params& p = reg.nation_ai();
    const std::map<entity_id, nation_budget> due =
        score_national_budgets(w, p, econ_tick, &report.nation_scores);
    for (const auto& [n, weights] : due)
        w.nation_budgets[n] = weights;

    // ---- 2. Gather: this tick's claims, emitted by corp_ai earlier in the tick
    // `run_national_budget` sorts them into its own walk order; the report's
    // vector is left as emitted so a reader sees the order the scorer produced.
    //
    // ---- 2b. ...plus the state's OWN claims: the space programme (BL-644).
    // The one line whose claimant is the nation itself — the payee is the
    // supplier whose pool holds the launch lot, so the claim/transfer machinery
    // fits it exactly, and the derivation appends its earmarked claims here in
    // the same walk order everything else is pinned to. The returned intents
    // carry what each claim is FOR (good, quantity, pool) and are settled after
    // the spend.
    std::vector<space_purchase> space = derive_space_programme_claims(
        w, w.nation_budgets, reg.space_programme(), report.budget_claims);
    const std::vector<budget_claim>& claims = report.budget_claims;

    // ---- 3. Spend: the pure pass -------------------------------------------
    run_national_budget(w, w.nation_budgets, claims, &report.national_budget);

    // ---- 4. Dispatch the earmarks, in transfer order -----------------------
    // A paid public_exploration transfer credited the corp exactly the survey
    // cost; dispatch debits exactly that. If the survey cannot start, the
    // nation did not buy anything, so the credit goes back — a corp never keeps
    // an earmark it could not spend (NR-568: earmarked, not fungible).
    for (const budget_transfer& t : report.national_budget.transfers)
    {
        if (t.credits <= 0.0f) continue;

        earmark_result er;
        er.corp    = t.corp;
        er.nation  = t.nation;
        er.subject = t.subject;
        er.credits = t.credits;

        if (t.line == budget_priority::public_exploration && t.subject != null_entity)
        {
            const survey_dispatch_result r = dispatch_survey(w, t.subject, t.corp);
            er.dispatched = (r == survey_dispatch_result::success);
            if (!er.dispatched)
            {
                // Claw back: reverse the transfer in the same two places the
                // pass wrote it, so the tick's books still balance.
                auto cit = w.corporations.find(t.corp);
                auto nit = w.nations.find(t.nation);
                if (cit != w.corporations.end()) cit->second.balance -= t.credits;
                if (nit != w.nations.end())      nit->second.treasury += t.credits;
            }
            report.earmarks.push_back(er);
            // An earmark leaves the balance where it found it (credit in, cost
            // out, same tick), so it is NOT a subsidy line on the corp's ledger:
            // `net()` must stay "the delta added to the balance this tick". What
            // the nation bought is on `report.earmarks`, which is what BL-555
            // renders — "who is paying me, on what".
            continue;
        }

        // A space_programme transfer (BL-644) is settled BELOW, goods for
        // credit, and folded onto `subsidies` only once the goods actually
        // left the pool — a clawed-back purchase must not appear as income.
        if (t.line == budget_priority::space_programme)
            continue;

        // An unearmarked transfer (no line has one yet — BL-538's other lines
        // will) IS a subsidy: credit that stays on the balance. `net()` carries
        // it so the delta is explained rather than appearing as income from
        // nowhere.
        report.budgets[t.corp].subsidies += t.credits;
    }

    // ---- 4b. Settle the space programme (BL-644): each paid claim's lump
    // leaves the supplier's pool and ceases to exist — the terminal sink the
    // State demand channel promises (MARKETS.md § Demand channels). Unlike the
    // survey earmark the credit STAYS on the supplier's balance (it is a
    // sale), so a completed purchase is folded onto `subsidies` — the field is
    // "credits received from a nation this tick", and `net()` must explain the
    // delta. A clawed-back purchase moved nothing net and folds nothing.
    settle_space_purchases(w, space, report.national_budget);
    for (const space_purchase& sp : space)
        if (sp.completed)
            report.budgets[sp.supplier].subsidies += sp.credits;
    report.space_purchases = std::move(space);

    // ---- 5. Garrison upkeep (BL-571) — the military_research line's first
    // consumer, and the ONE line this pass debits without going through
    // run_national_budget's claim/transfer machinery. See nation_step.hpp's
    // own comment on run_nation_garrison_upkeep for why.
    run_nation_garrison_upkeep(w, reg);

}


void run_nation_garrison_upkeep(world& w, const recipe_registry& reg)
{
    if (w.units.empty() || w.nations.empty())
        return;

    const unit_upkeep_params& up = reg.military().upkeep;

    // Nation-owned units, bucketed by owner, both levels ascending: `std::map`
    // for the nation walk, and the unit ids within each bucket pushed in the
    // ascending order they were visited (w.units' own ids, pre-sorted).
    std::map<entity_id, std::vector<entity_id>> by_nation;
    {
        std::vector<entity_id> ids;
        ids.reserve(w.units.size());
        for (const auto& kv : w.units)
            ids.push_back(kv.first);
        std::sort(ids.begin(), ids.end());
        for (const entity_id id : ids)
        {
            const unit_component& u = w.units.at(id);
            if (u.count <= 0)
                continue;
            if (w.nations.find(u.owner) == w.nations.end())
                continue; // a corp-owned unit — economy_system.cpp's own pass
            by_nation[u.owner].push_back(id);
        }
    }
    if (by_nation.empty())
        return;

    for (const auto& [nation_id, unit_ids] : by_nation) // std::map: ascending
    {
        nation_component& nc = w.nations.at(nation_id);

        // The bill: the SAME credit-half formula a corp unit's wage uses
        // (budget_system.cpp's own `force_upkeep` accumulation), summed over
        // this nation's garrison alone.
        float bill = 0.0f;
        for (const entity_id uid : unit_ids)
            bill += resolve_unit_upkeep(w.units.at(uid), up).credits;

        // The share: run_national_budget's own formula (nation_budget.cpp),
        // recomputed here rather than read off its report — that pass early-
        // outs whole (touches nothing) whenever this tick's CLAIM list is
        // empty, which it always is for a line with no corp claimant, so its
        // report cannot be trusted to carry this nation's military_research
        // share at all.
        float paid = 0.0f;
        if (bill > 0.0f && nc.treasury > 0.0f)
        {
            const auto bit = w.nation_budgets.find(nation_id);
            if (bit != w.nation_budgets.end())
            {
                const nation_budget& bud = bit->second;
                const float reserve = std::clamp(bud.reserve_fraction, 0.0f, 1.0f);
                const float spendable = nc.treasury * (1.0f - reserve);

                float weight_total = 0.0f;
                for (std::size_t i = 0; i < priority_count; ++i)
                    if (std::isfinite(bud.weights[i]) && bud.weights[i] > 0.0f)
                        weight_total += bud.weights[i];

                const std::size_t mi =
                    static_cast<std::size_t>(budget_priority::military_research);
                if (spendable > 0.0f && weight_total > 0.0f && bud.weights[mi] > 0.0f)
                {
                    const float share = spendable * (bud.weights[mi] / weight_total);
                    paid = std::min(share, bill);
                }
            }
        }

        if (paid > 0.0f)
            nc.treasury -= paid; // a direct expenditure — see the credit-wage
                                  // precedent in this function's own doc comment

        // Shortfall reads exactly like run_unit_upkeep's own decay rule, ONE
        // bill for the whole garrison rather than a per-unit draw: bill <= 0
        // means `paid == bill == 0`, so `unmet` is false and every unit
        // recovers — inert at the shipped (all-zero) rates, matching the corp
        // path.
        const bool unmet = paid < bill;
        for (const entity_id uid : unit_ids)
        {
            unit_component& u = w.units.at(uid);
            if (unmet)
                u.supply_factor_permille =
                    std::max(0, u.supply_factor_permille - up.supply_decay_permille);
            else
                u.supply_factor_permille =
                    std::min(1000, u.supply_factor_permille + up.supply_recovery_permille);
        }
    }
}

// ---------------------------------------------------------------------------
// BL-572 — derive_contract_offers. See nation_step.hpp's own comment for the
// design and the four moves; this is the arithmetic.
// ---------------------------------------------------------------------------

namespace {

/// The hex neighbour of `tile` on `side` (0..5), or null_entity when the step
/// leaves the grid. Columns WRAP, rows do not — the same rule every other
/// tile-adjacency helper in this project walks (nation_ai.cpp's own
/// `neighbour_tile`, nation_generation.cpp's `garrison_neighbour_tile`).
/// A THIRD, file-scoped copy rather than a shared one: nation_ai.cpp's is
/// built over a pure, const raster for its own purity contract, and
/// nation_generation.cpp's is private to that translation unit — reusing
/// either from here would mean exporting machinery neither file's own scope
/// calls for. `w` is non-const only because `body_tile_grid` memoises.
entity_id offer_neighbour_tile(world& w, entity_id tile, int side)
{
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return null_entity;
    const auto bit = w.bodies.find(tit->second.body);
    if (bit == w.bodies.end())
        return null_entity;
    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;
    if (gw <= 0 || gh <= 0)
        return null_entity;

    const hex_neighbors::coord c =
        hex_neighbors::neighbour(tit->second.grid_x, tit->second.grid_y, side);
    if (c.gy < 0 || c.gy >= gh)
        return null_entity;
    int nx = c.gx % gw;
    if (nx < 0)
        nx += gw;

    const std::vector<entity_id>& grid = body_tile_grid(w, tit->second.body);
    const std::size_t idx = static_cast<std::size_t>(c.gy) * static_cast<std::size_t>(gw)
                           + static_cast<std::size_t>(nx);
    return idx < grid.size() ? grid[idx] : null_entity;
}

/// True iff any tile of @p pr is hex-adjacent to a tile @p nid holds — the
/// same border test `seed_nation_garrisons` (nation_generation.cpp) applies
/// with the roles reversed (there: a province of THIS nation touching the
/// neighbour; here: a province of the NEIGHBOUR touching this nation).
bool province_touches_nation(world& w, const province& pr, entity_id nid)
{
    for (const entity_id tile : pr.tiles)
        for (int side = 0; side < 6; ++side)
        {
            const entity_id nb = offer_neighbour_tile(w, tile, side);
            if (nb == null_entity)
                continue;
            const auto tnit = w.tile_to_nation.find(nb);
            if (tnit != w.tile_to_nation.end() && tnit->second == nid)
                return true;
        }
    return false;
}

/// This tick's spendable share of the `contracted_force` line for @p nid — the
/// SAME formula `run_nation_garrison_upkeep` computes inline for
/// `military_research` (spendable = treasury x (1 - reserve); share =
/// spendable x weight / sum(weights)), factored out here rather than shared
/// with that function so each line's consumer stays a self-contained read of
/// `w.nation_budgets`, exactly as BL-538's other line consumers will be.
/// Zero — inertly — for a nation with no treasury, no authored budget, or no
/// weight on this line.
float contracted_force_share(const world& w, entity_id nid)
{
    const auto ncit = w.nations.find(nid);
    if (ncit == w.nations.end() || !(ncit->second.treasury > 0.0f))
        return 0.0f;

    const auto bit = w.nation_budgets.find(nid);
    if (bit == w.nation_budgets.end())
        return 0.0f;

    const nation_budget& bud     = bit->second;
    const float          reserve = std::clamp(bud.reserve_fraction, 0.0f, 1.0f);
    const float          spendable = ncit->second.treasury * (1.0f - reserve);
    if (!(spendable > 0.0f))
        return 0.0f;

    float weight_total = 0.0f;
    for (std::size_t i = 0; i < priority_count; ++i)
        if (std::isfinite(bud.weights[i]) && bud.weights[i] > 0.0f)
            weight_total += bud.weights[i];
    if (!(weight_total > 0.0f))
        return 0.0f;

    const std::size_t li = static_cast<std::size_t>(budget_priority::contracted_force);
    if (!(bud.weights[li] > 0.0f))
        return 0.0f;

    return spendable * (bud.weights[li] / weight_total);
}

} // namespace

