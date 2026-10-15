#include "space_assets.hpp"

#include "components.hpp"
#include "world.hpp"

#include <cmath>

// BL-1327 — see space_assets.hpp for the design. This is the arithmetic.

bool space_launch_legal(space_asset_class cls, space_reach reach)
{
    const bool orbit = reach == space_reach::home_orbit || reach == space_reach::moon_orbit
                    || reach == space_reach::far_orbit;
    const bool home  = reach == space_reach::home_orbit;
    switch (cls)
    {
        case space_asset_class::satellite:      return orbit;
        case space_asset_class::rover:          return !orbit; // surfaces are never home
        case space_asset_class::crewed_mission: return !home;
        case space_asset_class::station_module:
            return reach == space_reach::home_orbit || reach == space_reach::moon_orbit;
    }
    return false;
}

entity_id space_home_body_of(const world& w, entity_id nation)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return null_entity;
    if (nit->second.capital_tile != null_entity)
    {
        const auto tit = w.tiles.find(nit->second.capital_tile);
        if (tit != w.tiles.end() && w.bodies.count(tit->second.body) != 0)
            return tit->second.body;
    }
    return w.bodies.count(w.home_body) != 0 ? w.home_body : null_entity;
}

namespace {

/// A body's distance from the star in AU: its own orbital radius if it orbits
/// the star, else its top-level ancestor's (a moon reads its planet's). The
/// walk is bounded, so a malformed parent cycle cannot hang the tick.
float heliocentric_au(const world& w, entity_id body)
{
    entity_id cur = body;
    for (int hop = 0; hop < 8; ++hop)
    {
        const auto bit = w.bodies.find(cur);
        if (bit == w.bodies.end())
            return 0.0f;
        const body_component& b = bit->second;
        if (b.parent == null_entity || w.bodies.count(b.parent) == 0)
            return std::isfinite(b.orbital_radius_au) ? b.orbital_radius_au : 0.0f;
        cur = b.parent;
    }
    return 0.0f;
}

} // namespace

space_launch_quote quote_space_launch(const world& w, const space_asset_params& p,
                                      entity_id nation, space_asset_class cls,
                                      space_destination dest)
{
    space_launch_quote q;
    // Untrusted-input discipline: the enum values are range-checked as values,
    // so a caller passing a cast integer is refused, not indexed out of bounds.
    if (static_cast<std::size_t>(cls) >= space_asset_class_count
        || static_cast<uint8_t>(dest.slot) > static_cast<uint8_t>(space_slot::surface))
    {
        q.result = space_launch_result::rejected_invalid;
        return q;
    }
    const auto nit = w.nations.find(nation);
    if (nation == null_entity || nit == w.nations.end())
    {
        q.result = space_launch_result::rejected_unknown_nation;
        return q;
    }
    const entity_id home = space_home_body_of(w, nation);
    const auto bit = w.bodies.find(dest.body);
    if (home == null_entity || dest.body == null_entity || bit == w.bodies.end())
    {
        q.result = space_launch_result::rejected_unknown_body;
        return q;
    }
    if (bit->second.type == body_type::star
        || (dest.body == home && dest.slot == space_slot::surface))
    {
        q.result = space_launch_result::rejected_illegal_destination;
        return q;
    }

    const bool surface = dest.slot == space_slot::surface;
    float au = 0.0f;
    if (dest.body == home)
        q.reach = space_reach::home_orbit;
    else if (bit->second.parent == home)
        q.reach = surface ? space_reach::moon_surface : space_reach::moon_orbit;
    else
    {
        q.reach = surface ? space_reach::far_surface : space_reach::far_orbit;
        au = std::fabs(heliocentric_au(w, dest.body) - heliocentric_au(w, home));
    }

    if (!space_launch_legal(cls, q.reach))
    {
        q.result = space_launch_result::rejected_illegal_destination;
        return q;
    }

    const std::size_t ci = static_cast<std::size_t>(cls);
    const std::size_t ri = static_cast<std::size_t>(q.reach);
    q.cost = p.launch_cost[ci] + p.reach_cost[ri] + p.far_cost_per_au * au;
    const float extra_q = std::floor(p.far_quarters_per_au * au);
    q.travel_quarters = p.travel_quarters[ri]
                      + (std::isfinite(extra_q) && extra_q > 0.0f ? static_cast<int32_t>(extra_q) : 0);
    if (!std::isfinite(q.cost) || q.cost < 0.0f || q.travel_quarters < 0
        || !std::isfinite(p.upkeep[ci]) || p.upkeep[ci] < 0.0f)
    {
        q.result = space_launch_result::rejected_invalid;
        return q;
    }
    if (!(nit->second.treasury >= q.cost))
    {
        q.result = space_launch_result::rejected_insufficient_treasury;
        return q;
    }
    q.result = space_launch_result::launched;
    return q;
}

space_launch_quote launch_space_asset(world& w, const space_asset_params& p,
                                      entity_id nation, space_asset_class cls,
                                      space_destination dest, int econ_tick)
{
    space_launch_quote q = quote_space_launch(w, p, nation, cls, dest);
    if (q.result != space_launch_result::launched)
        return q; // rejected whole: nothing above wrote to `w`

    // Every check has passed; only now does the world move.
    space_asset a;
    a.id    = w.create_entity();
    a.owner = nation;
    a.cls   = cls;
    a.dest  = dest;
    a.state = space_asset_state::in_transit;
    a.launch_tick  = econ_tick;
    a.arrival_tick = econ_tick + q.travel_quarters;
    a.upkeep       = p.upkeep[static_cast<std::size_t>(cls)];
    a.unpaid_quarters = 0;

    w.nations.at(nation).treasury -= q.cost;
    w.space_assets.emplace(a.id, a);
    q.asset = a.id;
    return q;
}

void run_space_asset_step(world& w, const space_asset_params& p, int econ_tick,
                          space_asset_step_report* out)
{
    space_asset_step_report r;
    for (auto& [id, a] : w.space_assets) // std::map: ascending id
    {
        if (a.state == space_asset_state::lost)
            continue;
        if (a.state == space_asset_state::in_transit)
        {
            if (econ_tick < a.arrival_tick)
                continue; // still travelling; no upkeep in transit
            a.state = space_asset_state::active;
            ++r.arrived;
        }

        // In place: pay the quarter whole, or pay nothing.
        const auto nit = w.nations.find(a.owner);
        const bool can_pay = nit != w.nations.end() && std::isfinite(a.upkeep) && a.upkeep >= 0.0f
                          && nit->second.treasury >= a.upkeep;
        if (can_pay)
        {
            nit->second.treasury -= a.upkeep;
            r.upkeep_paid += a.upkeep;
            a.unpaid_quarters = 0;
            a.state = space_asset_state::active;
            ++r.paid;
        }
        else
        {
            ++a.unpaid_quarters;
            ++r.unpaid;
            if (a.unpaid_quarters >= p.quarters_to_loss)
            {
                a.state = space_asset_state::lost;
                ++r.lost;
            }
            else
                a.state = space_asset_state::decaying;
        }
    }
    if (out != nullptr)
        *out = r;
}
