#include "nation_step.hpp"

#include "components.hpp"
#include "economy_system.hpp"
#include "nation_ai.hpp"
#include "nation_budget.hpp"
#include "recipe_registry.hpp"
#include "survey_system.hpp"
#include "world.hpp"

#include <algorithm>
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
}
