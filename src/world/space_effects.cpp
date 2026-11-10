#include "space_effects.hpp"

#include "components.hpp"
#include "market_clearing.hpp" // market_for_tile
#include "nation_tech.hpp"     // nation_has_mechanic, nation_tech_state, k_milli_per_rp
#include "space_assets.hpp"
#include "space_coverage.hpp"
#include "space_tech_keys.hpp"
#include "stations.hpp"
#include "survey_system.hpp"
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

// See space_effects.hpp for the rule and the step order (SPACE_ASSETS.md
// § Satellites, § Space stations, § Signal delay).

namespace {

bool is_active_sat(const space_asset& a, entity_id nation, space_satellite_role role)
{
    return a.owner == nation && a.cls == space_asset_class::satellite && a.role == role
        && a.state == space_asset_state::active;
}

const station_module_spec& spec_of(const station_params& stp, uint8_t kind)
{
    static const station_module_spec none{};
    return kind < station_module_kind_count ? stp.modules[kind] : none;
}

/// Buy @p qty of @p good off @p market's shelf for @p nation at the posted
/// price, whole or nothing, under the fair-price ceiling. Returns the credits
/// paid (0 for qty <= 0), or a negative value when the buy was refused.
float buy_off_shelf(world& w, entity_id nation, entity_id market, resource_type good, float qty,
                    float reservation_mult)
{
    if (!(qty > 0.0f))
        return 0.0f;
    const auto nit = w.nations.find(nation);
    const auto mit = w.markets.find(market);
    if (nit == w.nations.end() || mit == w.markets.end())
        return -1.0f;
    const std::size_t ri = static_cast<std::size_t>(good);
    market_component& mc = mit->second;
    if (!shelf_admits(mc, ri, reservation_mult, /*off_buys=*/true))
        return -1.0f;
    const float unit = posted_price(mc, ri);
    const float cost = unit * qty;
    if (!std::isfinite(cost) || !(unit > 0.0f))
        return -1.0f;
    if (mc.inventory[ri] < qty || nit->second.treasury < cost)
        return -1.0f;
    mc.inventory[ri]     -= qty;
    nit->second.treasury -= cost;
    return cost;
}

/// Sell @p qty of @p good onto @p market's shelf for @p nation at the posted
/// price: the market is the counterparty, bid or no bid (MARKETS.md § the
/// clearing tick, "landings sold"). Done in one statement here rather than
/// through `landed_this_tick`, which is transient and already cleared by the
/// time the nation step runs. Returns the credits earned.
float sell_onto_shelf(world& w, entity_id nation, entity_id market, resource_type good, float qty)
{
    if (!(qty > 0.0f))
        return 0.0f;
    const auto nit = w.nations.find(nation);
    const auto mit = w.markets.find(market);
    if (nit == w.nations.end() || mit == w.markets.end())
        return 0.0f;
    const std::size_t ri = static_cast<std::size_t>(good);
    market_component& mc = mit->second;
    const float unit = posted_price(mc, ri);
    const float earned = unit * qty;
    if (!std::isfinite(earned) || !(unit > 0.0f))
        return 0.0f; // an unpriced good is never listed
    mc.inventory[ri]     += qty;
    nit->second.treasury += earned;
    return earned;
}

/// Map up to @p k regions of @p body (DISCOVERY.md § Mapping, then prospecting:
/// orbit maps). Returns the regions mapped. Never the home world or the star.
int map_regions(world& w, entity_id body, int k)
{
    if (k <= 0 || body == null_entity || body == w.home_body || body == w.star_body)
        return 0;
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end() || bit->second.type == body_type::star)
        return 0;
    survey_state& s = bit->second.survey;
    if (s.phase == survey_phase::surveyed || s.phase == survey_phase::in_transit)
        return 0;
    if (s.phase == survey_phase::hidden)
    {
        // Start the scan as a dispatched probe's would on arrival: the schedule's
        // region count, and the day gap to its first region boundary.
        const survey_schedule sched = survey_compute_schedule(w, body);
        if (sched.regions_total <= 0)
            return 0;
        s.phase           = survey_phase::scanning;
        s.regions_total   = sched.regions_total;
        s.regions_done    = 0;
        s.ticks_remaining = static_cast<int>(
            std::lround(1.0 / sched.regions_total * static_cast<double>(sched.scan_ticks)));
    }
    if (s.regions_total <= 0)
        return 0;
    const int before = s.regions_done;
    s.regions_done = std::min(s.regions_total, s.regions_done + k);
    if (s.regions_done >= s.regions_total)
    {
        s.phase           = survey_phase::surveyed;
        s.ticks_remaining = 0;
    }
    return s.regions_done - before;
}

/// The bodies with any node in @p cov, ascending.
std::vector<entity_id> covered_bodies(const std::set<orbital_node>& cov)
{
    std::vector<entity_id> out;
    for (const orbital_node& n : cov)
        if (out.empty() || out.back() != n.body)
            out.push_back(n.body);
    return out; // the set orders by body first, so this is ascending and unique
}

} // namespace

entity_id space_home_market_of(const world& w, entity_id nation)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return null_entity;
    if (nit->second.capital_tile != null_entity)
    {
        const entity_id m = market_for_tile(w, nit->second.capital_tile);
        if (m != null_entity && w.markets.find(m) != w.markets.end())
            return m;
    }
    const entity_id home = space_home_body_of(w, nation);
    entity_id best = null_entity;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == home && home != null_entity && (best == null_entity || mid < best))
            best = mid;
    return best;
}

float space_navigation_factor(const world& w, entity_id nation, entity_id body,
                              const space_effects_params& p)
{
    if (w.space_assets.empty() || nation == null_entity || body == null_entity)
        return 1.0f;
    for (const auto& [id, a] : w.space_assets)
    {
        (void)id;
        if (is_active_sat(a, nation, space_satellite_role::navigation)
            && space_served_body(a.node) == body)
        {
            const float f = p.navigation_logistics_factor;
            return (std::isfinite(f) && f > 0.0f) ? f : 1.0f;
        }
    }
    return 1.0f;
}

bool space_observation_lifts_vision(const world& w, entity_id nation, entity_id body)
{
    if (w.space_assets.empty() || nation == null_entity || body == null_entity)
        return false;
    bool any = false;
    for (const auto& [id, a] : w.space_assets)
    {
        (void)id;
        if (is_active_sat(a, nation, space_satellite_role::observation)
            && space_served_body(a.node) == body)
        {
            any = true;
            break;
        }
    }
    if (!any)
        return false;
    const std::set<orbital_node> cov = space_coverage(w, nation);
    for (const auto& [id, a] : w.space_assets)
    {
        (void)id;
        if (is_active_sat(a, nation, space_satellite_role::observation)
            && space_served_body(a.node) == body && cov.count(a.node) != 0)
            return true;
    }
    return false;
}

bool space_comms_lifts_activity_fog(const world& w, entity_id nation)
{
    if (w.space_assets.empty() || nation == null_entity)
        return false;
    bool sat = false;
    for (const auto& [id, a] : w.space_assets)
    {
        (void)id;
        if (is_active_sat(a, nation, space_satellite_role::comms))
        {
            sat = true;
            break;
        }
    }
    return sat && nation_has_mechanic(w, nation, space_keys::role_comms);
}

void run_space_effects(world& w, const space_effects_params& p, const station_params& stp,
                       float reservation_mult, int econ_tick, space_effects_report* out)
{
    if (w.space_assets.empty() && w.stations.empty())
        return; // a world that never launched ticks as before
    space_effects_report rep;

    // Coverage per nation, computed once for the step (before any survey moves:
    // coverage reads assets and stations only, which this step never touches).
    std::map<entity_id, std::set<orbital_node>> cov_of;
    const auto coverage = [&](entity_id nation) -> const std::set<orbital_node>& {
        auto it = cov_of.find(nation);
        if (it == cov_of.end())
            it = cov_of.emplace(nation, space_coverage(w, nation)).first;
        return it->second;
    };

    // ---- The working set per station, and life support (1). ----------------
    struct station_work
    {
        entity_id              id    = null_entity;
        entity_id              owner = null_entity;
        orbital_node           node{};
        std::vector<entity_id> working; ///< After starvation; ascending id.
    };
    std::vector<station_work> works;
    for (const auto& [sid, st] : w.stations) // ascending id
    {
        const station_budget b = evaluate_station_budget(w, stp, sid, econ_tick);
        if (!b.core_in_place)
            continue;
        station_work sw;
        sw.id    = sid;
        sw.owner = st.owner;
        sw.node  = st.node;
        sw.working = b.working;

        bool has_life_support = false;
        for (const entity_id aid : b.working)
            if (w.space_assets.at(aid).module == static_cast<uint8_t>(station_module_kind::life_support))
                has_life_support = true;
        if (has_life_support)
        {
            const float qty = std::max(0.0f, p.rations_per_crew) * static_cast<float>(b.crew_aboard);
            const float paid = buy_off_shelf(w, st.owner, space_home_market_of(w, st.owner),
                                             resource_type::food_rations, qty, reservation_mult);
            if (paid >= 0.0f)
            {
                ++rep.stations_fed;
                rep.rations_bought += qty;
                rep.rations_spent  += paid;
            }
            else
            {
                // Starved: the station's life support counts zero, so every
                // module that needs crew stops (evaluate_station_budget's rule
                // with life = 0: the crew shortfall never clears, and only
                // crewless modules are left standing).
                ++rep.stations_starved;
                std::vector<entity_id> kept;
                for (const entity_id aid : sw.working)
                    if (spec_of(stp, w.space_assets.at(aid).module).crew_needed <= 0)
                        kept.push_back(aid);
                sw.working.swap(kept);
            }
        }
        works.push_back(std::move(sw));
    }

    // ---- Modules: research (2) and the microgravity factory (3). -----------
    const std::int64_t rp_milli = static_cast<std::int64_t>(
        std::llround(std::max(0.0f, p.research_rp_per_module) * static_cast<double>(k_milli_per_rp)));
    for (const station_work& sw : works)
    {
        const bool covered = coverage(sw.owner).count(sw.node) != 0;
        for (const entity_id aid : sw.working)
        {
            const uint8_t kind = w.space_assets.at(aid).module;
            switch (static_cast<station_module_kind>(kind))
            {
            case station_module_kind::research:
                if (!covered)
                {
                    ++rep.research_unsignalled; // the data waits for a signal
                    break;
                }
                if (rp_milli > 0 && w.nations.find(sw.owner) != w.nations.end())
                {
                    w.nation_tech[sw.owner].pool_milli += rp_milli;
                    rep.research_milli += rp_milli;
                }
                break;
            case station_module_kind::microgravity_factory:
            {
                const entity_id m = space_home_market_of(w, sw.owner);
                const float paid = buy_off_shelf(w, sw.owner, m, resource_type::silicon,
                                                 p.factory_input_lot, reservation_mult);
                if (paid < 0.0f)
                    break; // no input lot: the factory idles this quarter
                rep.factory_spent  += paid;
                rep.factory_earned += sell_onto_shelf(w, sw.owner, m, resource_type::electronics,
                                                      p.factory_output_lot);
                ++rep.factory_runs;
                break;
            }
            // The rest of the roster's effects, by module:
            case station_module_kind::defence:      break; // space combat (SPACE_FLEETS.md), v0.5.0
            case station_module_kind::dock:         break; // docking space units (SPACE_FLEETS.md), v0.5.0
            case station_module_kind::fuel_depot:   break; // refuelling docked units (SPACE_FLEETS.md), v0.5.0
            case station_module_kind::shipyard:     break; // building space units (SPACE_FLEETS.md), v0.5.0
            case station_module_kind::cargo_hold:   break; // an orbital stockpile for space convoys, v0.5.0
            case station_module_kind::market:       break; // a market centre traded by convoy, v0.5.0
            case station_module_kind::power:        break; // a budget term (stations.cpp)
            case station_module_kind::habitation:   break; // a budget term (stations.cpp)
            case station_module_kind::life_support: break; // a budget term, and step 1 above
            case station_module_kind::comms_relay:  break; // coverage (space_coverage.cpp)
            case station_module_kind::observatory:  break; // step 5 below
            case station_module_kind::truss:        break; // rides in no slot
            }
        }
    }

    // ---- Survey satellites (4). --------------------------------------------
    {
        struct sat_map { entity_id owner; orbital_node node; };
        std::vector<sat_map> sats;
        for (const auto& [id, a] : w.space_assets) // ascending id
        {
            (void)id;
            if (a.cls == space_asset_class::satellite && a.role == space_satellite_role::survey
                && a.state == space_asset_state::active)
                sats.push_back({a.owner, a.node});
        }
        for (const sat_map& s : sats)
            if (coverage(s.owner).count(s.node) != 0)
                rep.survey_regions += map_regions(w, space_served_body(s.node),
                                                  p.survey_regions_per_quarter);
    }

    // ---- Observatories (5). -------------------------------------------------
    for (const station_work& sw : works)
        for (const entity_id aid : sw.working)
            if (w.space_assets.at(aid).module == static_cast<uint8_t>(station_module_kind::observatory))
                for (const entity_id body : covered_bodies(coverage(sw.owner)))
                    rep.observatory_regions += map_regions(w, body, p.observatory_regions_per_quarter);

    if (out)
        *out = rep;
}
