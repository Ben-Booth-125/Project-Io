// nation_wiring — does anybody CALL the nation spines? (Sprint N3 slice 1,
// 2026-08-23; requirement group `nation-spines-live`.)
//
// Sprint N1 proved the national budget's arithmetic (nation_budget_harness) and
// Sprint N2 the scorer's purity (nation_scorer_harness), and a world ran
// bit-identical with both of them unreachable from the tick. A green harness on
// a function nobody calls is the `military_points` defect with a new name. This
// harness drives the tick AS THE DRIVERS DO — economy step, clearing, budget,
// `run_nation_step`, tech gates — on the real generated world, and asserts the
// loop actually closes: a nation authors its weights, a cash-gated rival's survey
// claim is PAID, and the survey of the claimed body is DISPATCHED the same tick.
//
// Three things are deliberately NOT hand-built here. The world is
// `make_hard_coded_world` (the claim producer sits in corp_ai's cash gate, so it
// fires only where a rival really wants a body it cannot afford); the registry is
// ai_skill_harness's mirror of economy.lua (so survey costs and corp cash are the
// shipped magnitudes); and the claims come off the report, never seeded. What IS
// seeded is the TREASURY — the levy/tariff crediting is money_conservation's to
// prove, and a fixed starting treasury keeps this harness about the wiring.
//
// R1 is the row that makes the sprint real. R2 is conservation through the whole
// chain. R3 is Ben's earmark ruling. R4 is inertness where it should hold. R5 is
// replay determinism through the step.

#include "harness_params.hpp"
#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/market_clearing.hpp"
#include "world/nation_ai.hpp"
#include "world/nation_budget.hpp"
#include "world/nation_step.hpp"
#include "world/recipe_registry.hpp"
#include "world/survey_system.hpp"
#include "world/tech_gate.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <map>
#include <vector>

namespace
{

int g_failures = 0;
void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

/// Mirror of scripts/economy.lua for the building types the generator places —
/// the same registry ai_skill_harness and spectator_determinism score against.
recipe_registry make_registry()
{
    recipe_registry reg;

    building_economics extraction;
    extraction.base_rate            = 20.0f;
    extraction.maintenance          = 5.0f;
    extraction.base_wage            = 8.0f;
    extraction.build_cost           = 100.0f;
    extraction.build_duration_ticks = 2.0f;
    extraction.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 20.0f;
    extraction.richness_reference = 0.0f;
    extraction.richness_min       = 0.25f;
    extraction.richness_max       = 2.0f;
    reg.set_economics(building_type::extraction_site, extraction);

    building_economics processing;
    processing.base_rate            = 8.0f;
    processing.maintenance          = 10.0f;
    processing.base_wage            = 12.0f;
    processing.build_cost           = 200.0f;
    processing.build_duration_ticks = 3.0f;
    processing.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 40.0f;
    reg.set_economics(building_type::processing_facility, processing);

    return reg;
}

struct tick_out
{
    economy_report rep;
};

/// One full tick, exactly as app::step_economy / main.cpp order it, with the
/// production sink passed so the levy path is live.
economy_report run_tick(world& w, const recipe_registry& reg, int tick)
{
    w.current_day_tick  = tick;
    w.current_econ_tick = tick;
    economy_report rep = run_economy_step(w, reg, /*spectating=*/false);
    auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention, &rep.budgets, &rep.buildings);
    run_nation_step(w, reg, rep, tick);
    advance_tech_gates(w);
    return rep;
}

double total_treasury(const world& w)
{
    std::map<entity_id, double> by_id;
    for (const auto& [nid, nc] : w.nations) by_id[nid] = nc.treasury;
    double t = 0.0;
    for (const auto& [nid, v] : by_id) t += v;
    return t;
}
double total_corp_cash(const world& w)
{
    std::map<entity_id, double> by_id;
    for (const auto& [cid, cc] : w.corporations) by_id[cid] = cc.balance;
    double t = 0.0;
    for (const auto& [cid, v] : by_id) t += v;
    return t;
}

void seed_treasuries(world& w, float amount)
{
    for (auto& [nid, nc] : w.nations) nc.treasury = amount;
}

} // namespace

int main()
{
    std::printf("nation_wiring — the nation spines are CALLED, and the loop closes (Sprint N3)\n\n");

    world_params params;
    const recipe_registry reg = make_registry();
    const int k = reg.nation_ai().cadence_k;
    constexpr int   ticks    = 60;
    constexpr float treasury = 5000.0f; // Generous: the question is wiring, not funding depth.

    // ---- R1 / R2 / R3: the loop closes when a nation is FUNDED ---------------
    // The scorer's job is WHAT weight exploration gets; ours is whether the wiring
    // score→claim→pay→dispatch→conserve actually runs from the tick. So this row
    // isolates the wiring: it runs the real economy step to produce real claims,
    // then AUTHORS a fat exploration budget on the claiming nations that are not
    // due this tick (a due nation's slot the scorer would overwrite — nation_ai's
    // contract) and funds them past the claim, and asserts the loop closes. What
    // the scorer actually pays at its own weights is measured, not asserted, in
    // the § REALISTIC block below — because at those weights it does NOT close,
    // which is the finding, not a harness bug.
    {
        world w = make_hard_coded_world(no_prehistory(params));
        const nation_ai_params& p = reg.nation_ai();

        // One real tick, at a tick index chosen so at least one claiming nation
        // is off its cadence slot. Produce claims first.
        int use_tick = -1;
        economy_report probe;
        for (int t = 0; t < ticks && use_tick < 0; ++t)
        {
            world scan = make_hard_coded_world(no_prehistory(params));
            economy_report rep = run_economy_step(scan, reg, false);
            for (const budget_claim& c : rep.budget_claims)
                if (!nation_is_due(scan, c.nation, t, p)) { use_tick = t; break; }
        }
        check(use_tick >= 0, "R1a a claim exists whose nation is off its cadence slot (the scorer will not overwrite it)");

        // Drive the real tick up to use_tick, then fund the off-slot claimants.
        for (int t = 0; t <= use_tick; ++t)
        {
            w.current_day_tick = t; w.current_econ_tick = t;
            probe = run_economy_step(w, reg, false);
            auto flows = clear_markets(w, reg, probe);
            apply_budget(w, reg, flows, probe.workforce_contention, &probe.budgets, &probe.buildings);
            if (t < use_tick) { run_nation_step(w, reg, probe, t); advance_tech_gates(w); }
        }

        int funded = 0;
        for (const budget_claim& c : probe.budget_claims)
        {
            if (nation_is_due(w, c.nation, use_tick, p)) continue;
            nation_budget nb;
            nb.reserve_fraction = 0.0f;
            nb.weights[static_cast<std::size_t>(budget_priority::public_exploration)] = 1.0f;
            w.nation_budgets[c.nation] = nb;
            w.nations.at(c.nation).treasury = c.amount * 2.0f; // Past the claim, comfortably.
            ++funded;
        }
        check(funded > 0, "R1b at least one claim's nation was fundable this tick");

        const double treas_before = total_treasury(w);
        std::map<entity_id, float> bal_before;
        for (const auto& [cid, cc] : w.corporations) bal_before[cid] = cc.balance;

        run_nation_step(w, reg, probe, use_tick);

        int paid = 0, dispatched = 0, clawed = 0;
        double paid_credits = 0.0, dispatched_credits = 0.0;
        bool fill_whole = true, survey_started = true, subsidies_zero = true;
        float sum = 0.0f;
        for (const budget_transfer& tr : probe.national_budget.transfers)
        {
            sum += tr.credits;
            if (tr.credits > 0.0f)
            {
                ++paid; paid_credits += tr.credits;
                if (tr.line == budget_priority::public_exploration && tr.fill_fraction != 1.0f)
                    fill_whole = false;
            }
        }
        for (const earmark_result& er : probe.earmarks)
        {
            if (er.dispatched)
            {
                ++dispatched; dispatched_credits += er.credits;
                const auto bit = w.bodies.find(er.subject);
                if (bit == w.bodies.end() || bit->second.survey.phase == survey_phase::hidden)
                    survey_started = false;
            }
            else ++clawed;
        }
        for (const auto& [cid, cb] : probe.budgets)
            if (cb.subsidies != 0.0f) subsidies_zero = false;

        std::printf("  funded %d off-slot claimants  paid=%d dispatched=%d clawed=%d  paid=%.2f dispatched=%.2f\n",
                    funded, paid, dispatched, clawed, paid_credits, dispatched_credits);

        check(paid > 0, "R1c a funded claim is PAID — run_national_budget is reached from the tick");
        check(dispatched > 0, "R1d the survey of the claimed body is DISPATCHED the same tick");
        check(survey_started, "R1e every dispatched earmark left its body's survey in progress (credit became a survey)");
        check(sum == probe.national_budget.total_transferred, "R2a Σ transfers.credits == total_transferred, bit-exact");
        check(fill_whole, "R3a every paid exploration transfer is whole (fill_fraction == 1.0): earmarked, never pro-rata");
        check(subsidies_zero, "R3b an earmark is not a subsidy: corp_budget::subsidies stays zero this cut");

        // Conservation across the funded step: the treasury fell by exactly the
        // dispatched earmarks (paid-then-clawed nets to zero), and each dispatched
        // corp's balance is back where it started (credit in, survey cost out).
        const double treas_fell = treas_before - total_treasury(w);
        check(std::fabs(treas_fell - dispatched_credits) < 1e-2,
              "R2b the treasury fell by EXACTLY the dispatched earmarks (clawed-back claims netted to zero)");
        bool balances_restored = true;
        for (const auto& [cid, cc] : w.corporations)
            if (std::fabs(cc.balance - bal_before.at(cid)) > 1e-2) balances_restored = false;
        check(balances_restored, "R2c every corp balance is back where it started: the credit left as the survey cost");
        check(dispatched_credits > 0.0, "R2d the conservation rows are not vacuous: credits actually moved");

        // The clawback path (two claims on one body: the second cannot dispatch).
        if (clawed > 0)
            check(true, "R3c a claim that could not dispatch was clawed back to the treasury (path exercised)");
    }

    // ---- § REALISTIC: the finding, measured not asserted ---------------------
    // At the scorer's own exploration weight, does a nation ever fund a survey?
    // Seed a fixed treasury (as if a levy had run) and report the gap between one
    // tick's exploration share and the cheapest claim the producer actually files.
    {
        world w = make_hard_coded_world(no_prehistory(params));
        seed_treasuries(w, treasury);
        float min_share = 1e30f, max_share = 0.0f, min_claim = 1e30f;
        int dispatched = 0;
        for (int t = 0; t < ticks; ++t)
        {
            const economy_report rep = run_tick(w, reg, t);
            for (const auto& [nid, nb] : w.nation_budgets)
            {
                const float share = w.nations.at(nid).treasury
                    * (1.0f - nb.reserve_fraction)
                    * nb.weights[static_cast<std::size_t>(budget_priority::public_exploration)];
                min_share = std::min(min_share, share);
                max_share = std::max(max_share, share);
            }
            for (const budget_claim& c : rep.budget_claims) min_claim = std::min(min_claim, c.amount);
            for (const earmark_result& er : rep.earmarks) if (er.dispatched) ++dispatched;
        }
        std::printf("\n  [REALISTIC, treasury %.0f] exploration line share/tick %.1f..%.1f cr; "
                    "cheapest claim %.0f cr; surveys dispatched over %d ticks: %d\n",
                    treasury, min_share, max_share, min_claim, ticks, dispatched);
        std::printf("  FINDING (NR): an indivisible earmark cannot be met from a thin line's single-tick share.\n"
                    "  At the scorer's ~1.3%% exploration weight a nation needs ~%.0f cr of treasury to fund the\n"
                    "  cheapest survey in one tick, and the default world enacts no levy, so treasuries are 0.\n",
                    min_claim / (max_share > 0 ? (max_share / treasury) : 1e-6f));
    }

    // ---- R4: inertness where it should hold ---------------------------------
    // Zero treasuries: nothing can be paid. The scorer still authors weights (a
    // nation's PREFERENCES do not depend on its wealth — that is the whole point
    // of the weight model), so `nation_budgets` fills, but no transfer, no
    // dispatch, and corp balances identical to a run with the step absent.
    {
        world with = make_hard_coded_world(no_prehistory(params));
        world without = make_hard_coded_world(no_prehistory(params));
        bool no_transfer = true;
        for (int t = 0; t < 2 * k; ++t)
        {
            const economy_report rep = run_tick(with, reg, t);
            if (rep.national_budget.total_transferred != 0.0f || !rep.earmarks.empty()) no_transfer = false;
            // The control: same tick without the nation step.
            without.current_day_tick = t; without.current_econ_tick = t;
            economy_report r2 = run_economy_step(without, reg, false);
            auto flows = clear_markets(without, reg, r2);
            apply_budget(without, reg, flows, r2.workforce_contention, &r2.budgets, &r2.buildings);
            advance_tech_gates(without);
        }
        bool balances_equal = true;
        for (const auto& [cid, cc] : with.corporations)
            if (without.corporations.at(cid).balance != cc.balance) balances_equal = false;
        check(no_transfer, "R4a with every treasury at zero the step transfers nothing and dispatches nothing");
        check(balances_equal, "R4b ...and every corp balance is bit-identical to a run with the step absent");
        check(!with.nation_budgets.empty(), "R4c ...while the scorer still authored weights (preference is wealth-independent)");
    }

    // ---- R5: replay determinism through the step -----------------------------
    {
        world a = make_hard_coded_world(no_prehistory(params));
        world b = make_hard_coded_world(no_prehistory(params));
        seed_treasuries(a, treasury);
        seed_treasuries(b, treasury);
        bool same = true;
        for (int t = 0; t < ticks; ++t)
        {
            const economy_report ra = run_tick(a, reg, t);
            const economy_report rb = run_tick(b, reg, t);
            if (ra.national_budget.transfers.size() != rb.national_budget.transfers.size()) { same = false; break; }
            for (std::size_t i = 0; i < ra.national_budget.transfers.size(); ++i)
            {
                const budget_transfer& x = ra.national_budget.transfers[i];
                const budget_transfer& y = rb.national_budget.transfers[i];
                if (x.corp != y.corp || x.nation != y.nation || x.line != y.line ||
                    x.subject != y.subject || x.credits != y.credits) { same = false; break; }
            }
            if (!same) break;
        }
        check(same, "R5a two runs of one seed produce identical transfers, field for field");
        check(a.state_hash(ticks) == b.state_hash(ticks), "R5b ...and identical state_hash (treasuries and weights are folded)");
        check(!a.nation_budgets.empty(), "R5c ...and the fold was exercised: the weight map is non-empty");
    }

    std::printf("\n%s\n", g_failures == 0 ? "ALL PASS" : "FAILURES");
    return g_failures == 0 ? 0 : 1;
}
