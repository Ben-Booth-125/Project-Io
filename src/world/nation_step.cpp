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
                     int econ_tick, const contract_template_registry& templates)
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

    // ---- 6. Contract offers (BL-572) — the contracted_force line's first
    // consumer, the SAME shape as step 5 one line over. See nation_step.hpp's
    // own comment on derive_contract_offers for why, and for the four moves.
    // No stated ordering against step 5 beyond "after the budget pass"
    // (CONTRACTS.md § Where offers come from); placed after it here because
    // it landed second — the two lines draw the same treasury independently
    // and neither reads the other's spend this tick.
    derive_contract_offers(w, reg, econ_tick);

    // ---- 7. Mercenary contract evaluation (BL-573) — pay or fail -----------
    // See nation_step.hpp's own comment on run_mercenary_contract_tick for
    // the four moves and why "after run_battles" is already satisfied by
    // this pass's position in the tick.
    run_mercenary_contract_tick(w, reg, templates, econ_tick);
}

void run_mercenary_contract_tick(world& w, const recipe_registry& reg,
                                 const contract_template_registry& templates,
                                 int econ_tick)
{
    if (w.mercenary_contracts.empty())
        return; // EMPTY IN EVERY GENERATED WORLD until accept_offer is issued.

    // Ascending id — a plain sort of what is, in practice, a short vector;
    // determinism is the requirement, not throughput (see the file comment).
    std::vector<std::size_t> order(w.mercenary_contracts.size());
    for (std::size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return w.mercenary_contracts[a].id < w.mercenary_contracts[b].id;
    });

    for (const std::size_t idx : order)
    {
        mercenary_contract& c = w.mercenary_contracts[idx];
        if (c.state != mercenary_contract_state::active)
            continue;

        // A stale/out-of-range template index cannot have been written by
        // accept_offer (it copies `mercenary_offer::template_index`
        // verbatim, and derive_contract_offers only ever issues offers
        // against a real registry row) — but a corrupt or hand-built world
        // is not this pass's job to diagnose, so it is read defensively
        // rather than asserted: an unresolvable contract simply never
        // settles, which is the least surprising failure mode short of a
        // crash.
        if (c.template_index < 0 ||
            static_cast<std::size_t>(c.template_index) >= templates.size())
            continue;
        const contract_template& tmpl = templates.at(static_cast<std::size_t>(c.template_index));

        condition pred = tmpl.predicate;
        pred.province  = c.province; // bound per accepted offer — never authored

        const bool holds_now = evaluate_condition(pred, w, c.contractor);

        bool settle_completed = false;
        bool settle_failed    = false;

        if (tmpl.continuous)
        {
            // "hold": the predicate must hold on EVERY tick up to the
            // deadline, or the contract fails EARLY — it does not wait.
            if (!holds_now)
                settle_failed = true;
            else if (econ_tick >= c.deadline)
                settle_completed = true; // held throughout, to and including this tick
        }
        else
        {
            // "take": the predicate need only hold AT the deadline — losing
            // and retaking the province before then still completes it.
            if (econ_tick >= c.deadline)
                settle_completed = holds_now;
            settle_failed = (econ_tick >= c.deadline) && !holds_now;
        }

        if (settle_completed)
        {
            const float remainder = c.fee - c.deposit_paid;
            // See accept_offer's own comment (corp_command.cpp) for why this
            // is a direct transfer: the full fee already left the client
            // nation's treasury when the originating offer's escrow filled,
            // so this is disbursing money already reserved, not a fresh
            // budget claim.
            if (remainder > 0.0f)
            {
                const auto cit = w.corporations.find(c.contractor);
                if (cit != w.corporations.end())
                    cit->second.balance += remainder;
            }
            c.state = mercenary_contract_state::completed;
            w.note_conduct(reg.sentiment(), c.client, c.contractor,
                           sentiment_factor_kind::contract_completed);
        }
        else if (settle_failed)
        {
            // Nothing paid beyond the deposit already credited at
            // accept_offer — CONTRACTS.md § Q2: "you are not paid for
            // trying". The reserved remainder is not disbursed to anyone; it
            // was already spent, from the client's perspective, the moment
            // the escrow that funded it left the treasury.
            c.state = mercenary_contract_state::failed;
            w.note_conduct(reg.sentiment(), c.client, c.contractor,
                           sentiment_factor_kind::contract_failed);
        }
        // else: still active, judged again next tick.
    }
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

void derive_contract_offers(world& w, const recipe_registry& reg, int econ_tick,
                            const contract_offer_params& params)
{
    if (w.nations.empty() || w.provinces.provinces.empty())
        return;

    // ---- Move 1: expire, over EVERY nation's offers at once ----------------
    // Erase-remove over the vector's own storage order preserves the
    // survivors' relative (append) order, which "oldest issued first" in
    // move 4 depends on. Refunding before the per-nation loop below means a
    // freed slot and a freed credit are both visible to this SAME tick's
    // targeting and funding, not next tick's.
    if (!w.mercenary_offers.empty())
    {
        std::vector<mercenary_offer> kept;
        kept.reserve(w.mercenary_offers.size());
        for (const mercenary_offer& off : w.mercenary_offers)
        {
            const bool expired = (econ_tick - off.issued_tick) >= params.offer_ttl_ticks;
            if (expired)
            {
                const auto nit = w.nations.find(off.client);
                if (nit != w.nations.end())
                    nit->second.treasury += off.offer_escrow;
                continue; // dropped — CONTRACTS.md: "returning its escrow"
            }
            kept.push_back(off);
        }
        w.mercenary_offers = std::move(kept);
    }

    const nation_ai_params& ai_p = reg.nation_ai();

    // Ascending nation ids — `w.nations` is an unordered_map.
    std::vector<entity_id> nation_ids;
    nation_ids.reserve(w.nations.size());
    for (const auto& kv : w.nations)
        nation_ids.push_back(kv.first);
    std::sort(nation_ids.begin(), nation_ids.end());

    for (const entity_id nid : nation_ids)
    {
        if (w.nations.at(nid).tiles.empty())
            continue; // a landless nation has no border and no niche (nation_ai.hpp)

        // ---- Move 2: this tick's contracted_force share --------------------
        // Gates moves 3 and 4 both: no money behind the line means no new
        // offer is worth opening (see nation_step.hpp's own comment on why
        // this is the inert-by-construction choice) and nothing to fund.
        const float share = contracted_force_share(w, nid);
        if (!(share > 0.0f))
            continue;

        // ---- Move 3: target ---------------------------------------------------
        const entity_id best_neighbour = nation_highest_grudge_neighbour(w, nid, ai_p);
        if (best_neighbour != null_entity)
        {
            std::set<uint32_t> already_offered;
            for (const mercenary_offer& off : w.mercenary_offers)
                if (off.client == nid)
                    already_offered.insert(off.target_province);

            uint32_t target        = 0;
            int64_t  best_strength = -1;
            for (const province& pr : w.provinces.provinces) // ascending id: deterministic
            {
                if (province_holder_for(w, pr.id) != best_neighbour)
                    continue;
                if (already_offered.count(pr.id) != 0)
                    continue;
                if (!province_touches_nation(w, pr, nid))
                    continue;

                const int64_t strength = garrison_strength_in(w, pr.id);
                if (target == 0 || strength < best_strength) // strict '<': ascending-id tie-break
                {
                    best_strength = strength;
                    target        = pr.id;
                }
            }

            if (target != 0)
            {
                mercenary_offer off;
                off.id              = w.allocate_offer_id();
                off.client          = nid;
                off.target_province = target;
                off.template_index  = params.template_index;
                off.fee             = params.base_fee;
                off.issued_tick     = econ_tick;
                off.deadline        = econ_tick + params.deadline_ticks;
                off.offer_escrow    = 0.0f;
                w.mercenary_offers.push_back(off);
            }
        }

        // ---- Move 4: fund this nation's open offers, oldest-issued-first ----
        std::vector<std::size_t> idxs;
        for (std::size_t k = 0; k < w.mercenary_offers.size(); ++k)
            if (w.mercenary_offers[k].client == nid)
                idxs.push_back(k);
        if (idxs.empty())
            continue;

        std::stable_sort(idxs.begin(), idxs.end(), [&](std::size_t a, std::size_t b) {
            const mercenary_offer& oa = w.mercenary_offers[a];
            const mercenary_offer& ob = w.mercenary_offers[b];
            if (oa.issued_tick != ob.issued_tick)
                return oa.issued_tick < ob.issued_tick;
            return oa.id < ob.id;
        });

        nation_component& nc        = w.nations.at(nid);
        float             remaining = share;
        for (const std::size_t idx : idxs)
        {
            if (!(remaining > 0.0f))
                break;
            mercenary_offer& off  = w.mercenary_offers[idx];
            const float      need = off.fee - off.offer_escrow;
            if (!(need > 0.0f))
                continue; // already clears the fee — stops asking, per NR-568

            const float pay = std::min(need, remaining);
            off.offer_escrow += pay;
            nc.treasury      -= pay;
            remaining        -= pay;
        }
    }
}
