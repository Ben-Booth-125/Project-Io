#include "standing.hpp"

#include <algorithm>
#include <unordered_set>

const char* standing_band_label(standing_band b)
{
    switch (b)
    {
        case standing_band::negligible: return "Negligible";
        case standing_band::minor:      return "Minor";
        case standing_band::notable:    return "Notable";
        case standing_band::major:      return "Major";
        case standing_band::dominant:   return "Dominant";
    }
    return "Negligible";
}

// Boundary rule: a value exactly ON a documented boundary lands in the HIGHER band. Walked
// bottom-up with strict '<' tests so the first test that fails to exclude wins the band.
standing_band band_reach(int bodies)
{
    if (bodies <= 0) return standing_band::negligible;
    if (bodies == 1) return standing_band::minor;
    if (bodies == 2) return standing_band::notable;
    if (bodies == 3) return standing_band::major;
    return standing_band::dominant; // 4+
}

standing_band band_capital(float balance)
{
    if (balance < 0.0f)      return standing_band::negligible;
    if (balance < 5000.0f)   return standing_band::minor;
    if (balance < 20000.0f)  return standing_band::notable;
    if (balance < 50000.0f)  return standing_band::major;
    return standing_band::dominant; // >= 50000
}

standing_band band_market_share(float share01)
{
    if (share01 < 0.05f) return standing_band::negligible;
    if (share01 < 0.15f) return standing_band::minor;
    if (share01 < 0.30f) return standing_band::notable;
    if (share01 < 0.50f) return standing_band::major;
    return standing_band::dominant; // >= 0.50
}

std::vector<corp_standing> compute_corp_standings(
    const world& w,
    const std::unordered_map<entity_id, corp_cash_flow>& cash_flow)
{
    // Sorted corp id walk for deterministic output (same pattern corporation_panel.cpp uses —
    // w.corporations is an unordered_map).
    std::vector<entity_id> corp_ids;
    corp_ids.reserve(w.corporations.size());
    for (const auto& [id, _] : w.corporations)
        corp_ids.push_back(id);
    std::sort(corp_ids.begin(), corp_ids.end());

    // Tick-total income denominator for market share (0 if no trade this tick — avoids /0).
    float total_income = 0.0f;
    for (const auto& [id, flow] : cash_flow)
        total_income += flow.income;

    std::vector<corp_standing> out;
    out.reserve(corp_ids.size());

    for (const entity_id id : corp_ids)
    {
        const corporation_component& cc = w.corporations.at(id);

        corp_standing cs;
        cs.corp      = id;
        cs.is_player = (id == w.player_entity);

        // Reach: distinct bodies with >= 1 owned building. Building ownership/location is
        // visible per BL-068, so walking the corp's own asset list is honest for any corp.
        std::unordered_set<entity_id> bodies;
        for (const entity_id bld_id : cc.assets)
        {
            const auto bit = w.buildings.find(bld_id);
            if (bit == w.buildings.end())
                continue;
            const auto tit = w.tiles.find(bit->second.tile);
            if (tit == w.tiles.end())
                continue;
            bodies.insert(tit->second.body);
        }
        cs.reach_bodies = static_cast<int>(bodies.size());

        cs.capital_balance = cc.balance;

        const auto fit = cash_flow.find(id);
        const float own_income = (fit != cash_flow.end()) ? fit->second.income : 0.0f;
        cs.market_share = (total_income > 0.0f) ? (own_income / total_income) : 0.0f;

        cs.reach_band   = band_reach(cs.reach_bodies);
        cs.capital_band = band_capital(cs.capital_balance);
        cs.share_band   = band_market_share(cs.market_share);

        out.push_back(cs);
    }

    return out;
}
