#include "nation_step.hpp"

#include "components.hpp"
#include "economy_system.hpp"
#include "nation_ai.hpp"
#include "nation_budget.hpp"
#include "recipe_registry.hpp"
#include "survey_system.hpp"
#include "unit_roster.hpp" // BL-571: resolve_unit_upkeep, unit_upkeep_params
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <map>
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

        // An unearmarked transfer (no line has one yet — BL-538's other lines
        // will) IS a subsidy: credit that stays on the balance. `net()` carries
        // it so the delta is explained rather than appearing as income from
        // nowhere.
        report.budgets[t.corp].subsidies += t.credits;
    }

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
