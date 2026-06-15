#include "budget_system.hpp"

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

            delta -= e.maintenance;
            delta -= b.workforce_assigned * scalar * e.base_wage;
        }

        cc.balance += delta; // may go negative — allowed in the prototype
    }
}
