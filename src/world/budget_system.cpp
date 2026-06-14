#include "budget_system.hpp"

void apply_budget(world& w,
                  const recipe_registry& reg,
                  const std::unordered_map<entity_id, corp_cash_flow>& flows)
{
    for (auto& [corp, cc] : w.corporations)
    {
        float delta = 0.0f;

        // Market cash flow (sales income less input purchases), valued at base_price.
        const auto fit = flows.find(corp);
        if (fit != flows.end())
            delta += fit->second.income - fit->second.expenditure;

        // Operating costs: per-building maintenance + wages.
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            const building_component&  b = bit->second;
            const building_economics&  e = reg.economics(b.type);
            delta -= e.maintenance;
            delta -= b.workforce_assigned * e.base_wage;
        }

        cc.balance += delta; // may go negative — allowed in the prototype
    }
}
