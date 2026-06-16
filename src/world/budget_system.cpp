#include "budget_system.hpp"

#include <algorithm>

void apply_budget(world& w,
                  const recipe_registry& reg,
                  const std::unordered_map<entity_id, corp_cash_flow>& flows,
                  const std::map<std::pair<entity_id, entity_id>, float>& contention)
{
    for (auto& [corp, cc] : w.corporations)
    {
        float delta = 0.0f;

        // Market cash flow (sales income less input purchases), valued at the
        // price resolved this tick by clear_markets.
        const auto fit = flows.find(corp);
        if (fit != flows.end())
            delta += fit->second.income - fit->second.expenditure;

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

            // Labour/material cost split (BL-049).
            // Material cost: fixed 30 % overhead, charged even when decommissioned.
            // Labour cost: scales with workforce_target; zero when decommissioned.
            const float wt_scalar     = std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);
            const float material_cost = e.maintenance * 0.3f;
            const float labour_cost   = b.decommissioned ? 0.0f
                                        : e.maintenance * wt_scalar - material_cost;
            // Guard against negative labour_cost when wt_scalar < 0.3 (the material
            // floor already covers more than the scaled total in that edge case).
            delta -= material_cost + std::max(0.0f, labour_cost);
            delta -= b.decommissioned ? 0.0f : b.workforce_assigned * scalar * e.base_wage * wt_scalar;
        }

        cc.balance += delta; // may go negative — allowed in the prototype
    }
}
