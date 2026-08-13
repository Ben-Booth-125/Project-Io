#include "building_profit.hpp"

#include "budget_system.hpp"    // compute_building_opex, body_mean_habitability
#include "components.hpp"
#include "economy_system.hpp"   // extraction_nominal (BL-346 stack-aware estimate)
#include "market_clearing.hpp"  // market_for_tile
#include "placement_rules.hpp"  // stack_members, stack_output_scalar (BL-193)
#include "recipe_registry.hpp"
#include "world.hpp"

#include <algorithm> // std::clamp
#include <vector>

building_profit estimate_building_profit(const world& w, const recipe_registry& reg,
                                         const economy_report& report, entity_id building_id)
{
    building_profit out;

    const auto bit = w.buildings.find(building_id);
    if (bit == w.buildings.end())
        return out;
    const building_component& b = bit->second;

    // Locate this building's report entry (its production this tick) — via the
    // per-tick row index (BL-360), falling back to a scan for a hand-built report
    // that never filled it.
    const building_report* br = nullptr;
    if (const auto rit = report.building_row.find(building_id);
        rit != report.building_row.end() && rit->second < report.buildings.size())
        br = &report.buildings[rit->second];
    else if (report.building_row.empty())
        for (const building_report& r : report.buildings)
            if (r.building == building_id) { br = &r; break; }
    if (br == nullptr)
        return out; // no economy tick has reported this building yet
    out.has_data = true;

    // Maintenance + wages via the shared budget formula, with the same inputs
    // apply_budget uses (contention scalar + body mean habitability), so the numbers
    // match the corp budget exactly.
    const entity_id body = br->body;
    float scalar = 1.0f;
    const auto cit = report.workforce_contention.find(std::make_pair(br->corp, body));
    if (cit != report.workforce_contention.end())
        scalar = cit->second;

    const building_opex opex = compute_building_opex(b, reg.economics(b.type), scalar,
                                                     body_mean_habitability(w, body));
    out.maintenance = opex.maintenance;
    out.wages       = opex.wages;

    // Resolve the tile's market once, then value output/inputs at its prices.
    const market_component* mkt = nullptr;
    {
        const entity_id mid = market_for_tile(w, b.tile);
        if (mid != null_entity)
        {
            const auto mit = w.markets.find(mid);
            if (mit != w.markets.end())
                mkt = &mit->second;
        }
    }
    const auto price = [&](resource_type r) -> float {
        if (mkt == nullptr)
            return 0.0f;
        const std::size_t ri = static_cast<std::size_t>(r);
        return mkt->price[ri] > 0.0f ? mkt->price[ri] : mkt->base_price[ri];
    };

    if (b.type == building_type::extraction_site)
    {
        // Extraction sells its whole output; no inputs.
        out.revenue = br->output_quantity * price(br->target_resource);
    }
    else if (b.type == building_type::processing_facility)
    {
        // Value the recipe's outputs and inputs at the number of runs implied by the
        // reported output (output_quantity is the summed output units this tick).
        const recipe* rec = reg.get_recipe(b.recipe);
        if (rec != nullptr)
        {
            float total_out = 0.0f;
            for (float o : rec->outputs)
                total_out += o;
            const float runs = (total_out > 0.0f) ? br->output_quantity / total_out : 0.0f;
            for (std::size_t ri = 0; ri < resource_count; ++ri)
            {
                const float p = price(static_cast<resource_type>(ri));
                out.revenue    += rec->outputs[ri] * runs * p;
                out.input_cost += rec->inputs[ri]  * runs * p;
            }
        }
    }

    return out;
}

// --- Prospective (pre-build) estimate (BL-162) -------------------------------
// Lives here, beside the realised estimator, rather than in the construction ledger's
// UI translation unit: this is world-layer economics, it is the figure that ranks the
// player's build recommendations, and only from src/world/ can a tools/verify harness
// link it (tools/verify/prospective_profit_harness.cpp).
//
// corp_ai.cpp's build scorer keeps its own cruder inline duplicate (it omits
// habitability-scaled wages). Switching it would change AI build scoring, hence world
// evolution, hence every blessed golden — a known, accepted divergence, not an oversight.
building_profit estimate_prospective_profit(const world& w, const recipe_registry& reg,
                                            entity_id tile_id, building_type type,
                                            resource_type target,
                                            std::uint16_t recipe_id,
                                            const building_component* existing)
{
    building_profit out;

    const auto tit = w.tiles.find(tile_id);
    if (tit == w.tiles.end())
        return out;
    const tile_component& tc = tit->second;

    // The tile's market sets every price. Without one there is nothing honest to
    // report, so has_data stays false — a chart of zeros is a lie.
    const market_component* mkt = nullptr;
    {
        const entity_id mid = market_for_tile(w, tile_id);
        if (mid != null_entity)
        {
            const auto mit = w.markets.find(mid);
            if (mit != w.markets.end())
                mkt = &mit->second;
        }
    }
    if (mkt == nullptr)
        return out;
    const auto price = [&](resource_type r) -> float {
        const std::size_t ri = static_cast<std::size_t>(r);
        return mkt->price[ri] > 0.0f ? mkt->price[ri] : mkt->base_price[ri];
    };

    // The building construct_building would actually author for this candidate —
    // unless the caller named a real one, in which case its own staffing is what
    // it would come back at, and modelling the authored defaults instead would
    // price a question nobody asked.
    building_component bc{};
    bc.tile               = tile_id;
    bc.type               = type;
    bc.workforce_assigned =
        (type == building_type::port || type == building_type::inland_logistics_hub) ? 0.0f : 0.5f;
    bc.target_resource    = target;
    bc.recipe             = recipe_id;
    if (existing)
    {
        bc.workforce_assigned = existing->workforce_assigned;
        bc.workforce_target   = existing->workforce_target;
    }
    // `wt_scalar` is workforce_target/100 inside compute_building_opex, and the
    // revenue terms below assume target 100 scales to 1. Carry the real scalar
    // so revenue and wages move together rather than only the wages half.
    const float wt_scalar =
        existing ? std::clamp(existing->workforce_target / 100.0f, 0.0f, 2.0f) : 1.0f;

    out.has_data = true;

    // Opex through the shared budget formula, so the estimate and apply_budget agree
    // exactly — the pre-build number and the post-build Net must be the same quantity.
    const building_economics& econ = reg.economics(type);
    const building_opex opex =
        compute_building_opex(bc, econ, 1.0f, body_mean_habitability(w, tc.body));
    out.maintenance = opex.maintenance;
    out.wages       = opex.wages;

    // Contention 1.0 scales to 1; the labour terms left are the assigned fraction
    // and the target scalar (economy_system.cpp § run_extraction). For the
    // default hypothetical, wt_scalar is 1 and this reduces to the old `wf`.
    const float wf = bc.workforce_assigned * wt_scalar;

    if (type == building_type::extraction_site)
    {
        const std::size_t ri = static_cast<std::size_t>(target);

        // BL-346: the candidate joins whatever stack already works this deposit, and
        // BOTH halves of the BL-193 stack rule bear on its number.
        //
        //   * It is the NEWEST site, so it lands at the bottom of the decay curve —
        //     rank = (sites already here) + 1, yielding 0.8^(rank-1) of a lone site.
        //   * The taper is SHARED and sized against the whole stack's combined draw,
        //     this candidate included, so a stacked deposit's remaining life is
        //     shorter than the same reserve would give one site.
        //
        // Without both, the ledger priced a fourth site on a half-worked deposit as
        // though it were the first site on a fresh one — over-stating its revenue and
        // its remaining life at once, which is the ranking the player builds off.
        const std::vector<entity_id> members = placement_rules::stack_members(
            w, tile_id, building_type::extraction_site, target);
        // An existing subject is ALREADY in `members` (stack_members filters on
        // tile/type/target only). Ranking it at size()+1 would charge it a step of
        // BL-193 decay against itself, so it takes the rank it actually holds and
        // is removed from the combined-draw loop below, which adds `nominal`
        // separately.
        // `members` is left INTACT so every other member keeps the rank its
        // index implies; the subject is identified by id and skipped in the
        // combined-draw loop below instead of being erased, because erasing it
        // would silently renumber everyone after it.
        entity_id self_id = null_entity;
        int       rank    = static_cast<int>(members.size()) + 1;
        if (existing)
        {
            for (std::size_t k = 0; k < members.size(); ++k)
            {
                const auto it = w.buildings.find(members[k]);
                if (it != w.buildings.end() && &it->second == existing)
                {
                    self_id = members[k];
                    rank    = static_cast<int>(k) + 1;
                    break;
                }
            }
        }

        const float nominal = econ.base_rate * tc.resource_deposit[ri] * wf
                              * (1.0f - tc.hazard_level)
                              * placement_rules::stack_output_scalar(rank);

        // Combined draw of the stack this candidate would join, itself included and at
        // its own rank. Members are priced through `extraction_nominal`, the same
        // figure run_extraction draws, at the contention 1.0 this estimator assumes
        // throughout — re-deriving it here is how the two drift apart.
        float combined = nominal;
        for (std::size_t k = 0; k < members.size(); ++k)
        {
            if (members[k] == self_id)
                continue; // already counted as `nominal` above
            const auto mit = w.buildings.find(members[k]);
            if (mit == w.buildings.end())
                continue;
            const building_component& mb = mit->second;
            if (mb.decommissioned || mb.ticks_remaining > 0)
                continue; // holds its rank, contributes nothing to the draw
            const float n = extraction_nominal(w, reg, mb, 1.0f)
                          * placement_rules::stack_output_scalar(static_cast<int>(k) + 1);
            if (n > 0.0f)
                combined += n;
        }

        // Apply run_extraction's depletion taper (economy_system.cpp § run_extraction).
        // resource_deposit is RICHNESS — a fixed property of the tile — while
        // resource_remaining is the reserve, drained by any extraction site that has
        // ever worked this tile. Pricing off richness alone valued a spent tile as
        // though it were untouched: demolish a worked-out mine, reselect the tile, and
        // the ledger ranked that resource FIRST with the tallest bar, for a building
        // that returns `exhausted` on its first tick and earns nothing ever.
        //
        // The estimate is per-tile because the reserve is per-tile. This is the one
        // place the "a building that does not exist has extracted nothing" reasoning
        // does not hold.
        const float taper_band = deposit_taper_ticks * combined;
        const float taper      = taper_band > 0.0f
            ? std::clamp(tc.resource_remaining[ri] / taper_band, 0.0f, 1.0f)
            : 0.0f;

        // Below the floor run_extraction reports `exhausted` and yields nothing. No
        // special flag is needed to say so: revenue 0 against maintenance and wages
        // that still fall due gives a NEGATIVE net, so the candidate draws a red bar
        // and sorts last — which is exactly the truth about building there.
        out.revenue = (nominal > 0.0f && taper < deposit_min_taper)
            ? 0.0f
            : nominal * taper * price(target);
    }
    else if (type == building_type::processing_facility)
    {
        if (const recipe* rcp = reg.get_recipe(recipe_id))
        {
            const float batches = econ.base_rate * wf;
            for (std::size_t ri = 0; ri < resource_count; ++ri)
            {
                const float p = price(static_cast<resource_type>(ri));
                out.revenue    += rcp->outputs[ri] * batches * p;
                out.input_cost += rcp->inputs[ri]  * batches * p;
            }
        }
    }
    // Port / Launchpad / Inland Logistics Hub produce nothing directly: revenue and
    // input cost stay zero, so net() is exactly their upkeep, negative by construction.

    return out;
}
