#include "standing.hpp"

#include <algorithm>
#include <unordered_set>

// The disclosure gate (BL-633). Binary by FINANCE.md § Disclosure: a `public` corporation files
// a return the player may read; a `private` or `closed` one does not file at all. There is no
// graded middle — the five bands that used to supply one are retired.
bool corp_files_return(ownership_class oc)
{
    return oc == ownership_class::publicly_held;
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
        // visible on-canvas (DISCOVERY.md § Competitor visibility), so walking the corp's own
        // asset list is honest for any corp and the figure is public for every one of them.
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

        // Capital is a FILED figure: exact where the firm files, absent where it does not.
        // The gate is the firm's own ownership class, never a fact about the reader.
        cs.capital_balance   = cc.balance;
        cs.capital_disclosed = corp_files_return(cc.ownership_class);

        // Market share: derived from the market aggregates, which are the deliberate public
        // signal — public for every corp for the same reason reach is.
        const auto fit = cash_flow.find(id);
        const float own_income = (fit != cash_flow.end()) ? fit->second.income : 0.0f;
        cs.market_share = (total_income > 0.0f) ? (own_income / total_income) : 0.0f;

        out.push_back(cs);
    }

    return out;
}
