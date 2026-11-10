#include "solar_storm.hpp"

#include "stations.hpp"
#include "world.hpp"

#include <map>

// See solar_storm.hpp for the rule (EVENTS.md § Four families, Solar storms).

void run_solar_storm_report(const world& w, const station_params& stp, int econ_tick,
                            solar_storm_report* out, const solar_storm_params& p)
{
    if (out == nullptr)
        return;
    *out = {};
    if (w.space_assets.empty() && w.stations.empty())
        return; // nothing in space: nothing to strike, nobody to warn
    solar_storm_report rep;
    rep.storm      = solar_storm_at(w, econ_tick, p);
    rep.storm_next = solar_storm_at(w, econ_tick + 1, p);
    if (!rep.storm && !rep.storm_next)
    {
        *out = rep;
        return;
    }

    std::map<entity_id, solar_storm_nation_row> rows; // ascending nation id
    const auto row = [&](entity_id n) -> solar_storm_nation_row& {
        solar_storm_nation_row& r = rows[n];
        r.nation = n;
        return r;
    };

    if (rep.storm)
        for (const auto& [id, a] : w.space_assets) // ascending id
        {
            (void)id;
            if (solar_storm_satellite_dark(w, a, econ_tick, p))
                ++row(a.owner).satellites_dark;
        }

    for (const auto& [sid, st] : w.stations) // ascending id
    {
        const station_budget b = evaluate_station_budget(w, stp, sid, econ_tick);
        if (!b.core_in_place)
            continue;
        if (b.storm_struck)
        {
            solar_storm_nation_row& r = row(st.owner);
            ++r.stations_struck;
            r.modules_stopped += static_cast<int>(b.stormed.size());
        }
        if (rep.storm_next)
            for (const entity_id aid : b.working)
            {
                const auto ait = w.space_assets.find(aid);
                if (ait != w.space_assets.end()
                    && ait->second.module == static_cast<uint8_t>(station_module_kind::observatory))
                {
                    row(st.owner).warned = true;
                    break;
                }
            }
    }

    for (const auto& [n, r] : rows)
    {
        (void)n;
        rep.nations.push_back(r);
    }
    *out = rep;
}
