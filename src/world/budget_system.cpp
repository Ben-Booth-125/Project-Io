#include "budget_system.hpp"

#include "law.hpp"         // BL-343: evaluate_laws at the enforcement seam
#include "unit_roster.hpp" // BL-454: resolve_unit_upkeep (the credit half)

#include <algorithm>
#include <map>
#include <vector>

float body_mean_habitability(const world& w, entity_id body)
{
    // Mirrors the accumulation apply_budget's batch used, filtered to one body, so
    // the estimate and the live budget loop read an identical mean (bit-for-bit).
    float sum = 0.0f;
    int   count = 0;
    for (const auto& [cid, pcc] : w.population_centres)
    {
        const auto tile_it = w.population_centre_tile.find(cid);
        if (tile_it == w.population_centre_tile.end())
            continue;
        const auto tc_it = w.tiles.find(tile_it->second);
        if (tc_it == w.tiles.end())
            continue;
        if (tc_it->second.body != body)
            continue;
        sum += pcc.habitability;
        ++count;
    }
    return (count > 0) ? sum / static_cast<float>(count) : 1.0f;
}

building_opex compute_building_opex(const building_component& b,
                                    const building_economics& e,
                                    float contention_scalar,
                                    float mean_hab)
{
    // Labour/material cost split (BL-049).
    // Material cost: fixed 30 % overhead, charged even when decommissioned.
    // Labour cost: scales with workforce_target; zero when decommissioned.
    const float wt_scalar     = std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);
    const float material_cost = e.maintenance * 0.3f;
    const float labour_cost   = b.decommissioned ? 0.0f
                                : e.maintenance * wt_scalar - material_cost;
    const float hab           = std::clamp(mean_hab, 0.1f, 2.0f);

    building_opex o;
    // Guard against negative labour_cost when wt_scalar < 0.3 (the material floor
    // already covers more than the scaled total in that edge case).
    o.maintenance = material_cost + std::max(0.0f, labour_cost);
    o.wages       = b.decommissioned
                    ? 0.0f
                    : b.workforce_assigned * contention_scalar * e.base_wage * wt_scalar * hab;
    return o;
}

void apply_budget(world& w,
                  const recipe_registry& reg,
                  const std::unordered_map<entity_id, corp_cash_flow>& flows,
                  const std::map<std::pair<entity_id, entity_id>, float>& contention,
                  std::map<entity_id, corp_budget>* breakdown,
                  const std::vector<building_report>* production)
{
    // BL-343 law enforcement. Skipped entirely when nothing is enacted, so a
    // world with no laws runs the pre-BL-343 arithmetic untouched.
    bool any_law_enacted = false;
    for (const law& l : w.laws)
        any_law_enacted = any_law_enacted || l.enacted;
    const bool levy_pass = any_law_enacted && production != nullptr;

    // BL-042B: mean habitability per body (from population centres) scales wages.
    // Cached per body — body_mean_habitability scans every centre, so compute each
    // body once; building_profit.hpp's estimate reads the same helper (no drift).
    // BL-454: standing-force upkeep, bucketed by owner ONCE. Ascending unit id
    // within each bucket, because `w.units` is an unordered_map and a float sum
    // over it would otherwise be order-dependent (float addition is not
    // associative) — the same trap condition_set.cpp's military_strength sum
    // avoids by staying integral. std::map, so the buckets themselves are
    // ordered too.
    //
    // Skipped entirely when the corp fields no units, so a unit-free world runs
    // the pre-BL-454 arithmetic untouched.
    const unit_upkeep_params& upkeep_rates = reg.military().upkeep;
    std::map<entity_id, std::vector<entity_id>> units_by_owner;
    {
        std::vector<entity_id> unit_ids;
        unit_ids.reserve(w.units.size());
        for (const auto& kv : w.units)
            unit_ids.push_back(kv.first);
        std::sort(unit_ids.begin(), unit_ids.end());
        for (const entity_id uid : unit_ids)
            units_by_owner[w.units.at(uid).owner].push_back(uid);
    }

    std::map<entity_id, float> hab_cache;
    const auto mean_hab_of = [&](entity_id body) -> float {
        const auto it = hab_cache.find(body);
        if (it != hab_cache.end())
            return it->second;
        const float v = body_mean_habitability(w, body);
        hab_cache.emplace(body, v);
        return v;
    };

    for (auto& [corp, cc] : w.corporations)
    {
        // Capture the flows into `bud` for the BL-072 breakdown, but keep the
        // balance update on the SAME interleaved `delta` the pre-BL-072 code used,
        // so the shipped economy is bit-identical (grouping the sums could drift by
        // a float ULP and, compounded over thousands of ticks, perturb the
        // deterministic sim — see io-standing-rules § Determinism).
        corp_budget bud;
        float delta = 0.0f;

        // Market cash flow (sales income less input purchases), valued at the
        // price resolved this tick by clear_markets.
        const auto fit = flows.find(corp);
        if (fit != flows.end())
        {
            bud.income      = fit->second.income;
            bud.expenditure = fit->second.expenditure;
            delta += bud.income - bud.expenditure;
        }

        // Operating costs: per-building maintenance + wages. Wages are paid on the
        // effective (allocated) workforce — the requested target throttled by the
        // (corp, body) contention scalar — not the requested target itself.
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            const building_component&  b = bit->second;
            const building_economics&  e = reg.economics(b.type);

            entity_id body = null_entity;
            const auto tit = w.tiles.find(b.tile);
            if (tit != w.tiles.end())
                body = tit->second.body;
            float scalar = 1.0f;
            const auto cit = contention.find(std::make_pair(corp, body));
            if (cit != contention.end())
                scalar = cit->second;

            // Maintenance + wages via the shared formula (BL-074), so the balance
            // loop and the per-building profitability estimate never diverge. Kept
            // in the same subtract-then-subtract order as the pre-extraction code,
            // so the deterministic balance is bit-identical.
            const building_opex opex = compute_building_opex(b, e, scalar, mean_hab_of(body));
            bud.maintenance += opex.maintenance;
            delta           -= opex.maintenance;
            bud.wages       += opex.wages;
            delta           -= opex.wages;
        }

        // BL-454: standing-force upkeep — its OWN term, after the buildings and
        // before the levies. Until this, `w.units` appeared in this file exactly
        // zero times: hire_unit debited once and the regiment was free forever
        // while every building beside it paid maintenance and wages every tick.
        //
        // Only the CREDIT half is here. The goods half is a pool debit and lives
        // in the unit pass (economy_system.cpp § run_unit_upkeep), which has also
        // already run this tick and written each unit's supply factor — which is
        // why the force counts below read live state rather than needing to be
        // plumbed through.
        {
            const auto uit = units_by_owner.find(corp);
            if (uit != units_by_owner.end())
            {
                float force_upkeep = 0.0f;
                for (const entity_id uid : uit->second)
                {
                    const unit_component& u = w.units.at(uid);
                    force_upkeep += resolve_unit_upkeep(u, upkeep_rates).credits;
                    ++bud.force_units;
                    if (u.supply_factor_permille < 1000)
                        ++bud.force_unsupplied;
                }
                if (force_upkeep != 0.0f)
                {
                    bud.upkeep = force_upkeep;
                    delta     -= force_upkeep;
                }
            }
        }

        // BL-343: enacted-law levies. The predicates are resolved ONCE per law
        // per corp here (evaluate_laws), before the charge is applied, so the
        // evaluation order is fixed and the money loop stays deterministic. The
        // charge itself walks `production` — a vector in the economy step's own
        // deterministic order — and reads only extraction output, because a levy
        // on raw output is what the extraction levy IS. Processing output is
        // downstream of a levy already paid on its inputs.
        //
        // BL-480: the levy is a TRANSFER, and jurisdiction bounds it. Each
        // schedule carries its author nation; only output extracted on tiles
        // that nation owns (`tile_to_nation`) is charged, and the debit is
        // credited to that nation's treasury in the SAME float, same tick — so
        // the sum is conserved by construction, not by reconciliation. A
        // schedule whose author nation no longer resolves charges NOTHING
        // (debiting with no treasury to credit is the money destruction this
        // item removed).
        if (levy_pass)
        {
            const law_effects fx = evaluate_laws(w, corp);
            if (fx.any)
            {
                float levy_total = 0.0f;
                for (const law_effects::levy_schedule& ls : fx.levies)
                {
                    const auto nat = w.nations.find(ls.enacting_nation);
                    if (nat == w.nations.end())
                        continue; // ill-formed author: no charge, no transfer

                    float levy = 0.0f;
                    for (const building_report& br : *production)
                    {
                        if (br.corp != corp || br.type != building_type::extraction_site)
                            continue;
                        if (br.output_quantity <= 0.0f)
                            continue;

                        // Jurisdiction: the tile the extraction stands on must
                        // belong to the enacting nation. Unclaimed tiles (and
                        // whole bodies without nations) are outside every
                        // jurisdiction and pay nothing.
                        const auto bld = w.buildings.find(br.building);
                        if (bld == w.buildings.end())
                            continue;
                        const auto tn = w.tile_to_nation.find(bld->second.tile);
                        if (tn == w.tile_to_nation.end() || tn->second != ls.enacting_nation)
                            continue;

                        levy += ls.extraction_levy[static_cast<std::size_t>(br.target_resource)]
                              * br.output_quantity;
                    }
                    if (levy != 0.0f)
                    {
                        levy_total          += levy;
                        nat->second.treasury += levy; // the credit half of the transfer
                    }
                }
                if (levy_total != 0.0f)
                {
                    bud.levies = levy_total;
                    delta     -= levy_total; // the debit half
                }
            }
        }

        cc.balance += delta; // bit-identical to the pre-BL-072 arithmetic; may go negative

        // BL-073: debt interest. Charged once per tick on the balance that is now
        // negative (the outstanding debt after this tick's operating flows), so a
        // corp underwater compounds its decline. Pure function of balance × the
        // fixed rate — deterministic, no wall-clock. Zero while solvent.
        if (cc.balance < 0.0f)
        {
            bud.interest = -cc.balance * k_debt_interest_per_quarter;
            cc.balance  -= bud.interest;
        }

        if (breakdown)
            (*breakdown)[corp] = bud;
    }
}
