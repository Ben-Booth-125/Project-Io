#include "building_profit.hpp"

#include "budget_system.hpp"   // compute_building_opex, body_mean_habitability
#include "components.hpp"
#include "market_clearing.hpp" // market_for_tile
#include "recipe_registry.hpp"
#include "world.hpp"

building_profit estimate_building_profit(const world& w, const recipe_registry& reg,
                                         const economy_report& report, entity_id building_id)
{
    building_profit out;

    const auto bit = w.buildings.find(building_id);
    if (bit == w.buildings.end())
        return out;
    const building_component& b = bit->second;

    // Locate this building's report entry (its production this tick).
    const building_report* br = nullptr;
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
    else if (b.type == building_type::processing_facility ||
             b.type == building_type::hydroponics_bay || // BL-166
             b.type == building_type::fishing_wharf)     // BL-168
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
