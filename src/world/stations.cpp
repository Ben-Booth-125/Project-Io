#include "stations.hpp"

#include "components.hpp"
#include "nation_tech.hpp"     // nation_has_mechanic (BL-1582: module and upgrade keys)
#include "solar_storm.hpp"     // BL-1605: a storm stops a struck station's modules
#include "space_tech_keys.hpp"
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

// BL-1583 / BL-1585 — see stations.hpp for the design. This is the arithmetic.

bool station_slot_on_edge(int level, int idx)
{
    const int w = station_grid_width(level), h = station_grid_height(level);
    if (idx < 0 || idx >= w * h)
        return false;
    const int c = idx % w, r = idx / w;
    return c == 0 || r == 0 || c == w - 1 || r == h - 1;
}

namespace {

bool node_valid(const world& w, orbital_node node)
{
    if (static_cast<uint8_t>(node.kind) >= orbital_node_kind_count || !orbital_node_is_orbital(node.kind))
        return false;
    const auto bit = w.bodies.find(node.body);
    return bit != w.bodies.end() && bit->second.type != body_type::star;
}

bool in_place(const space_asset& a)
{
    return a.state == space_asset_state::active || a.state == space_asset_state::decaying;
}

/// BL-1582: the key a fittable module kind needs (space_tech_keys.hpp). The
/// Shipyard's is far space's, so no nation holds it in Era 1.
std::string_view module_key(uint8_t kind)
{
    switch (static_cast<station_module_kind>(kind))
    {
        case station_module_kind::research:             return space_keys::module_research;
        case station_module_kind::defence:              return space_keys::module_defence;
        case station_module_kind::power:                return space_keys::module_power;
        case station_module_kind::dock:                 return space_keys::module_dock;
        case station_module_kind::habitation:           return space_keys::module_habitation;
        case station_module_kind::life_support:         return space_keys::module_life_support;
        case station_module_kind::fuel_depot:           return space_keys::module_fuel_depot;
        case station_module_kind::shipyard:             return space_keys::module_shipyard;
        case station_module_kind::microgravity_factory: return space_keys::module_microgravity_factory;
        case station_module_kind::observatory:          return space_keys::module_observatory;
        case station_module_kind::comms_relay:          return space_keys::module_comms_relay;
        case station_module_kind::cargo_hold:           return space_keys::module_cargo_hold;
        case station_module_kind::market:               return space_keys::module_market;
        case station_module_kind::truss:                break;
    }
    return {};
}

/// BL-1582: the key the upgrade FROM @p level needs: 5x3 -> 7x5 is
/// `space_grid_7x5`, 7x5 -> 9x7 is far space's `space_grid_9x7`.
std::string_view upgrade_key(int level)
{
    return level == 0 ? space_keys::grid_7x5 : space_keys::grid_9x7;
}

/// The pure half of THE ONE STATION LAUNCH: the quote (slot, gate, goods,
/// treasury — space_assets.cpp), then the crew check.
station_op quote_station_launch(const world& w, const space_asset_params& sp, entity_id nation,
                                space_asset_class cls, orbital_node node, int crew)
{
    station_op op;
    const space_launch_quote q = quote_space_launch(w, sp, nation, cls, node);
    op.launch = q.result;
    if (q.result == space_launch_result::rejected_unknown_nation)
    {
        op.result = station_result::rejected_unknown_nation;
        return op;
    }
    if (q.result != space_launch_result::launched)
    {
        op.result = station_result::rejected_launch;
        return op;
    }
    if (crew < 0 || w.nations.at(nation).astronauts < crew)
    {
        op.result = station_result::rejected_no_crew;
        return op;
    }
    op.result = station_result::ok;
    return op;
}

/// THE ONE STATION LAUNCH. Every check first; then the launch (which re-quotes
/// the same pure inputs and so cannot refuse), then the crew leave the pool.
station_op station_launch(world& w, const space_asset_params& sp, entity_id nation,
                          space_asset_class cls, orbital_node node, int crew)
{
    station_op op = quote_station_launch(w, sp, nation, cls, node, crew);
    if (op.result != station_result::ok)
        return op;
    const space_launch_quote q = launch_space_asset(w, sp, nation, cls, node);
    op.launch = q.result;
    if (q.result != space_launch_result::launched) // unreachable: the quote above passed
    {
        op.result = station_result::rejected_launch;
        return op;
    }
    w.nations.at(nation).astronauts -= crew;
    w.space_assets.at(q.asset).crew = crew;
    op.asset = q.asset;
    return op;
}

/// The station @p id if @p nation owns it; @p r receives why not.
space_station* owned_station(world& w, entity_id nation, entity_id id, station_result& r)
{
    if (w.nations.count(nation) == 0)
    {
        r = station_result::rejected_unknown_nation;
        return nullptr;
    }
    const auto it = w.stations.find(id);
    if (it == w.stations.end())
    {
        r = station_result::rejected_unknown_station;
        return nullptr;
    }
    if (it->second.owner != nation)
    {
        r = station_result::rejected_not_owner;
        return nullptr;
    }
    r = station_result::ok;
    return &it->second;
}

const station_module_spec& spec_of(const station_params& stp, uint8_t kind)
{
    static const station_module_spec none{};
    return kind < station_module_kind_count ? stp.modules[kind] : none;
}

/// Mark @p a lost with its crew (the record keeps the count; it is no longer aboard).
void lose_asset(space_asset& a)
{
    a.state = space_asset_state::lost;
    a.unpaid_quarters = std::max<int32_t>(1, a.unpaid_quarters); // the reader's lost-state rule
    a.refit_done_tick = -1;
}

} // namespace

void lose_station_asset(space_asset& a) { lose_asset(a); }

station_op quote_found_station(const world& w, const space_asset_params& sp, const station_params& stp,
                               entity_id nation, orbital_node node)
{
    station_op op;
    if (w.nations.count(nation) == 0)
    {
        op.result = station_result::rejected_unknown_nation;
        return op;
    }
    if (!node_valid(w, node))
    {
        op.result = station_result::rejected_invalid;
        return op;
    }
    return quote_station_launch(w, sp, nation, space_asset_class::station_core, node,
                                std::max(0, stp.core_crew_needed));
}

station_op found_station(world& w, const space_asset_params& sp, const station_params& stp,
                         entity_id nation, orbital_node node)
{
    station_op op = quote_found_station(w, sp, stp, nation, node);
    if (op.result != station_result::ok)
        return op;
    op = station_launch(w, sp, nation, space_asset_class::station_core, node, std::max(0, stp.core_crew_needed));
    if (op.result != station_result::ok)
        return op;
    space_station st;
    st.id    = op.asset;
    st.owner = nation;
    st.core  = op.asset;
    st.node  = node;
    st.level = 0;
    st.slots.assign(static_cast<std::size_t>(st.width() * st.height()), null_entity);
    st.slots[static_cast<std::size_t>(st.centre())] = op.asset;
    space_asset& core = w.space_assets.at(op.asset);
    core.station      = st.id;
    core.station_slot = static_cast<int16_t>(st.centre());
    w.stations.emplace(st.id, std::move(st));
    op.station = op.asset;
    return op;
}

station_op fit_station_module(world& w, const space_asset_params& sp, const station_params& stp,
                              entity_id nation, entity_id station, int slot, station_module_kind kind)
{
    station_op op;
    op.station = station;
    space_station* st = owned_station(w, nation, station, op.result);
    if (st == nullptr)
        return op;
    const uint8_t k = static_cast<uint8_t>(kind);
    if (k >= station_fittable_kind_count || slot < 0 || slot >= static_cast<int>(st->slots.size()))
    {
        op.result = station_result::rejected_invalid;
        return op;
    }
    if (st->slots[static_cast<std::size_t>(slot)] != null_entity)
    {
        op.result = station_result::rejected_slot_occupied;
        return op;
    }
    const station_module_spec& spec = stp.modules[k];
    if (spec.edge_only && !station_slot_on_edge(st->level, slot))
    {
        op.result = station_result::rejected_edge_only;
        return op;
    }
    if (!nation_has_mechanic(w, nation, module_key(k))) // BL-1582
    {
        op.result = station_result::rejected_tech_locked;
        return op;
    }
    const orbital_node node = st->node;
    station_op l = station_launch(w, sp, nation, space_asset_class::station_module, node,
                                  std::max(0, spec.crew_needed));
    l.station = station;
    if (l.result != station_result::ok)
        return l;
    space_asset& a = w.space_assets.at(l.asset);
    a.station      = station;
    a.station_slot = static_cast<int16_t>(slot);
    a.module       = k;
    w.stations.at(station).slots[static_cast<std::size_t>(slot)] = l.asset;
    return l;
}

station_op remove_station_module(world& w, entity_id nation, entity_id station, int slot)
{
    station_op op;
    op.station = station;
    space_station* st = owned_station(w, nation, station, op.result);
    if (st == nullptr)
        return op;
    if (slot < 0 || slot >= static_cast<int>(st->slots.size()))
    {
        op.result = station_result::rejected_invalid;
        return op;
    }
    const entity_id aid = st->slots[static_cast<std::size_t>(slot)];
    if (aid == null_entity || aid == st->core)
    {
        op.result = station_result::rejected_slot_empty;
        return op;
    }
    const auto ait = w.space_assets.find(aid);
    // The crew of an arrived module stay aboard: they move onto the core.
    if (ait != w.space_assets.end() && in_place(ait->second) && ait->second.crew > 0)
    {
        const auto cit = w.space_assets.find(st->core);
        if (cit != w.space_assets.end() && cit->second.state != space_asset_state::lost)
            cit->second.crew = static_cast<int32_t>(std::min<int64_t>(
                static_cast<int64_t>(cit->second.crew) + ait->second.crew, k_astronaut_pool_max));
    }
    if (ait != w.space_assets.end())
        w.space_assets.erase(ait);
    st->slots[static_cast<std::size_t>(slot)] = null_entity;
    op.asset  = aid;
    op.result = station_result::ok;
    return op;
}

station_op refit_station_module(world& w, const station_params& stp, entity_id nation,
                                entity_id station, int from, int to)
{
    station_op op;
    op.station = station;
    space_station* st = owned_station(w, nation, station, op.result);
    if (st == nullptr)
        return op;
    const int n = static_cast<int>(st->slots.size());
    if (from < 0 || from >= n || to < 0 || to >= n || from == to)
    {
        op.result = station_result::rejected_invalid;
        return op;
    }
    const entity_id aid = st->slots[static_cast<std::size_t>(from)];
    if (aid == null_entity || aid == st->core)
    {
        op.result = station_result::rejected_slot_empty;
        return op;
    }
    if (st->slots[static_cast<std::size_t>(to)] != null_entity)
    {
        op.result = station_result::rejected_slot_occupied;
        return op;
    }
    const auto ait = w.space_assets.find(aid);
    if (ait == w.space_assets.end() || !in_place(ait->second))
    {
        op.result = station_result::rejected_not_in_place;
        return op;
    }
    if (spec_of(stp, ait->second.module).edge_only && !station_slot_on_edge(st->level, to))
    {
        op.result = station_result::rejected_edge_only;
        return op;
    }
    st->slots[static_cast<std::size_t>(to)]   = aid;
    st->slots[static_cast<std::size_t>(from)] = null_entity;
    ait->second.station_slot    = static_cast<int16_t>(to);
    ait->second.refit_done_tick = w.current_econ_tick + std::max<int32_t>(1, stp.refit_quarters);
    op.asset  = aid;
    op.result = station_result::ok;
    return op;
}

station_op upgrade_station(world& w, const space_asset_params& sp, entity_id nation, entity_id station)
{
    station_op op;
    op.station = station;
    space_station* st = owned_station(w, nation, station, op.result);
    if (st == nullptr)
        return op;
    if (st->level >= k_station_max_level)
    {
        op.result = station_result::rejected_max_level;
        return op;
    }
    if (!nation_has_mechanic(w, nation, upgrade_key(st->level))) // BL-1582
    {
        op.result = station_result::rejected_tech_locked;
        return op;
    }
    const orbital_node node = st->node;
    station_op l = station_launch(w, sp, nation, space_asset_class::station_module, node, 0);
    l.station = station;
    if (l.result != station_result::ok)
        return l;
    space_asset& truss = w.space_assets.at(l.asset);
    truss.station      = station;
    truss.station_slot = -1;
    truss.module       = static_cast<uint8_t>(station_module_kind::truss);

    // Grow one ring: (c, r) -> (c + 1, r + 1). The core stays central.
    space_station& s = w.stations.at(station);
    const int ow = s.width();
    s.level = static_cast<uint8_t>(s.level + 1);
    const int nw = s.width();
    std::vector<entity_id> grown(static_cast<std::size_t>(nw * s.height()), null_entity);
    for (int i = 0; i < static_cast<int>(s.slots.size()); ++i)
    {
        const entity_id aid = s.slots[static_cast<std::size_t>(i)];
        if (aid == null_entity)
            continue;
        const int ni = (i / ow + 1) * nw + (i % ow + 1);
        grown[static_cast<std::size_t>(ni)] = aid;
        if (const auto ait = w.space_assets.find(aid); ait != w.space_assets.end())
            ait->second.station_slot = static_cast<int16_t>(ni);
    }
    s.slots = std::move(grown);
    return l;
}

station_result save_station_template(world& w, entity_id nation, const std::string& name,
                                     const station_template& t)
{
    if (w.nations.count(nation) == 0)
        return station_result::rejected_unknown_nation;
    if (name.empty() || name.size() > k_station_template_name_max || t.level > k_station_max_level)
        return station_result::rejected_invalid;
    const int lw = station_grid_width(t.level), lh = station_grid_height(t.level);
    if (t.kinds.size() != static_cast<std::size_t>(lw * lh))
        return station_result::rejected_invalid;
    for (const uint8_t k : t.kinds)
        if (k != k_no_station_module && k >= station_fittable_kind_count)
            return station_result::rejected_invalid;
    if (t.kinds[static_cast<std::size_t>((lh / 2) * lw + lw / 2)] != k_no_station_module)
        return station_result::rejected_invalid;
    auto& mine = w.station_templates[nation];
    if (mine.count(name) == 0 && mine.size() >= k_station_templates_per_nation)
    {
        if (mine.empty())
            w.station_templates.erase(nation);
        return station_result::rejected_template_full;
    }
    mine[name] = t;
    return station_result::ok;
}

station_result delete_station_template(world& w, entity_id nation, const std::string& name)
{
    const auto it = w.station_templates.find(nation);
    if (it == w.station_templates.end() || it->second.erase(name) == 0)
        return station_result::rejected_unknown_template;
    if (it->second.empty())
        w.station_templates.erase(it);
    return station_result::ok;
}

station_template_report apply_station_template(world& w, const space_asset_params& sp,
                                               const station_params& stp, entity_id nation,
                                               entity_id station, const std::string& name)
{
    station_template_report rep;
    space_station* st = owned_station(w, nation, station, rep.result);
    if (st == nullptr)
        return rep;
    const auto nit = w.station_templates.find(nation);
    if (nit == w.station_templates.end() || nit->second.count(name) == 0)
    {
        rep.result = station_result::rejected_unknown_template;
        return rep;
    }
    const station_template t = nit->second.at(name); // a copy: the fits below move nothing in it
    const int tw = station_grid_width(t.level), th = station_grid_height(t.level);
    const int sw = st->width(), sh = st->height();
    const int dc = (sw - tw) / 2, dr = (sh - th) / 2; // may be negative: a larger template
    rep.result = station_result::ok;

    // Ascending STATION slot, so the fits launch (and stop, newest first) in grid order.
    std::vector<std::pair<int, int>> order; // (station slot or -1 - tslot, template slot)
    for (int ti = 0; ti < tw * th; ++ti)
    {
        const uint8_t k = t.kinds[static_cast<std::size_t>(ti)];
        if (k == k_no_station_module)
            continue;
        const int c = ti % tw + dc, r = ti / tw + dr;
        if (c < 0 || r < 0 || c >= sw || r >= sh)
            order.emplace_back(-1 - ti, ti);
        else
            order.emplace_back(r * sw + c, ti);
    }
    std::sort(order.begin(), order.end());
    for (const auto& [si, ti] : order)
    {
        if (si < 0)
        {
            rep.skipped.emplace_back(si, station_result::rejected_invalid); // off this station's grid
            continue;
        }
        const uint8_t k = t.kinds[static_cast<std::size_t>(ti)];
        const space_station& cur = w.stations.at(station);
        const entity_id in = cur.slots[static_cast<std::size_t>(si)];
        if (in != null_entity)
        {
            const auto ait = w.space_assets.find(in);
            if (ait != w.space_assets.end() && ait->second.module == k)
                continue; // already matches
            rep.skipped.emplace_back(si, station_result::rejected_slot_occupied);
            continue;
        }
        const station_op op = fit_station_module(w, sp, stp, nation, station, si,
                                                 static_cast<station_module_kind>(k));
        if (op.result == station_result::ok)
            rep.fitted.push_back(si);
        else
            rep.skipped.emplace_back(si, op.result);
    }
    return rep;
}

station_budget evaluate_station_budget(const world& w, const station_params& stp,
                                       entity_id station, int econ_tick)
{
    station_budget b;
    const auto sit = w.stations.find(station);
    if (sit == w.stations.end())
        return b;
    const space_station& st = sit->second;
    const auto cit = w.space_assets.find(st.core);
    b.core_in_place = cit != w.space_assets.end() && in_place(cit->second);

    // Solar falloff: (home AU / station AU)^2, heliocentric both.
    const float home_au = space_heliocentric_au(w, space_home_body_of(w, st.owner));
    const float here_au = space_heliocentric_au(w, st.node.body);
    if (home_au > 0.0f && here_au > 0.0f)
    {
        const float f = (home_au / here_au) * (home_au / here_au);
        if (std::isfinite(f))
            b.solar_factor = f;
    }

    // Every module in a slot, ascending asset id (= fitting order).
    std::vector<entity_id> mods;
    for (const entity_id aid : st.slots)
        if (aid != null_entity && aid != st.core)
            mods.push_back(aid);
    std::sort(mods.begin(), mods.end());

    if (b.core_in_place)
        b.crew_aboard += std::max(0, cit->second.crew);
    std::vector<entity_id> cand;
    for (const entity_id aid : mods)
    {
        const auto ait = w.space_assets.find(aid);
        if (ait == w.space_assets.end())
            continue;
        const space_asset& a = ait->second;
        if (in_place(a))
            b.crew_aboard += std::max(0, a.crew);
        if (!b.core_in_place || a.state != space_asset_state::active)
            b.not_ready.push_back(aid);
        else if (a.refit_done_tick >= 0 && econ_tick < a.refit_done_tick)
            b.refitting.push_back(aid);
        else if (spec_of(stp, a.module).edge_only && !station_slot_on_edge(st.level, a.station_slot))
            b.idle_edge.push_back(aid);
        else
            cand.push_back(aid);
    }
    if (!b.core_in_place)
        return b;

    // BL-1605: a solar storm at the station's node stops every module for the
    // quarter (solar_storm.hpp). The core's own budget is still evaluated below.
    if (solar_storm_strikes(w, st.node, econ_tick))
    {
        b.storm_struck = true;
        b.stormed.swap(cand);
    }

    // Newest first stops, until the rest are covered.
    for (;;)
    {
        float supply = stp.core_power_supply * b.solar_factor, draw = 0.0f;
        int room = stp.core_crew_room, life = stp.core_life_support, need = std::max(0, stp.core_crew_needed);
        for (const entity_id aid : cand)
        {
            const station_module_spec& s = spec_of(stp, w.space_assets.at(aid).module);
            supply += s.power_supply * b.solar_factor;
            draw   += s.power_draw;
            room   += s.crew_room;
            life   += s.life_support;
            need   += std::max(0, s.crew_needed);
        }
        b.power_supply = supply;
        b.power_draw   = draw;
        b.crew_room    = room;
        b.life_support = life;
        b.crew_needed  = need;
        const bool power_short = draw > supply;
        const bool crew_short  = need > room || need > life || need > b.crew_aboard;
        if ((!power_short && !crew_short) || cand.empty())
            break;
        // The newest module that draws on a short budget stops. A module drawing
        // on no short budget (a Power module in a crew shortfall) is skipped:
        // stopping it could only deepen the shortfall.
        std::size_t victim = cand.size();
        for (std::size_t i = cand.size(); i-- > 0;)
        {
            const station_module_spec& s = spec_of(stp, w.space_assets.at(cand[i]).module);
            if ((power_short && s.power_draw > 0.0f) || (crew_short && s.crew_needed > 0))
            {
                victim = i;
                break;
            }
        }
        if (victim == cand.size())
        {
            // Nothing that draws on the shortfall is left: the core alone falls short
            // (no crew aboard) and nothing works.
            for (std::size_t i = cand.size(); i-- > 0;)
                b.stopped.push_back(cand[i]);
            cand.clear();
            break;
        }
        b.stopped.push_back(cand[victim]);
        cand.erase(cand.begin() + static_cast<std::ptrdiff_t>(victim));
    }
    b.working = cand;
    return b;
}

int astronaut_cap(const world& w, const station_params& stp, entity_id nation)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return 0;
    int64_t centres = 0;
    std::vector<entity_id> ids(nit->second.state_assets.begin(), nit->second.state_assets.end());
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    for (const entity_id bid : ids)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const building_component& b = bit->second;
        // Completed, running, staffed, and not on an occupied tile (MILITARY.md § Occupation).
        if (b.type == building_type::astronaut_training_centre && b.ticks_remaining <= 0
            && !b.decommissioned && !b.state_idle && b.workforce_assigned > 0.0f
            && !w.tile_occupied(b.tile))
            ++centres;
    }
    return static_cast<int>(std::min<int64_t>(centres * std::max(0, stp.astronaut_cap_per_centre),
                                              k_astronaut_pool_max));
}

int run_astronaut_training(world& w, const station_params& stp)
{
    const int rate = std::max(0, stp.astronauts_per_centre);
    const int per_cap = std::max(0, stp.astronaut_cap_per_centre);
    int trained = 0;
    if (rate == 0 || per_cap == 0)
        return 0;
    for (auto& [nid, nc] : w.nations) // std::map: ascending
    {
        const int cap = astronaut_cap(w, stp, nid);
        if (cap <= 0 || nc.astronauts >= cap)
            continue;
        const int64_t centres = cap / per_cap;
        const int64_t made = std::min<int64_t>(centres * rate, static_cast<int64_t>(cap) - nc.astronauts);
        nc.astronauts += static_cast<int32_t>(made);
        trained += static_cast<int>(made);
    }
    return trained;
}

void run_station_step(world& w, const station_params& stp, int econ_tick, station_step_report* out)
{
    station_step_report r;
    r.astronauts_trained = run_astronaut_training(w, stp);

    // Losses. A lost (or vanished) core loses its station and every part of it.
    std::vector<entity_id> dead;
    for (auto& [sid, st] : w.stations)
    {
        const auto cit = w.space_assets.find(st.core);
        if (cit == w.space_assets.end() || cit->second.state == space_asset_state::lost)
        {
            dead.push_back(sid);
            if (cit != w.space_assets.end())
                r.crew_lost += std::max(0, cit->second.crew); // lost this quarter with the core
            for (const entity_id slot : st.slots) // modules lost in the same quarter, still slotted
                if (slot != null_entity && slot != st.core)
                    if (const auto ait = w.space_assets.find(slot);
                        ait != w.space_assets.end() && ait->second.state == space_asset_state::lost)
                        r.crew_lost += std::max(0, ait->second.crew);
            continue;
        }
        for (entity_id& slot : st.slots)
        {
            if (slot == null_entity || slot == st.core)
                continue;
            const auto ait = w.space_assets.find(slot);
            if (ait == w.space_assets.end() || ait->second.state == space_asset_state::lost)
            {
                if (ait != w.space_assets.end())
                    r.crew_lost += std::max(0, ait->second.crew);
                slot = null_entity;
                ++r.slots_freed;
            }
        }
    }
    if (!dead.empty())
    {
        for (auto& [aid, a] : w.space_assets) // ascending
        {
            if (a.station == null_entity || a.cls == space_asset_class::station_core
                || a.state == space_asset_state::lost)
                continue;
            if (std::binary_search(dead.begin(), dead.end(), a.station))
            {
                r.crew_lost += std::max(0, a.crew);
                lose_asset(a);
                ++r.modules_lost;
            }
        }
        for (const entity_id sid : dead)
            w.stations.erase(sid);
        r.stations_lost = static_cast<int>(dead.size());
    }

    // Refits that have run their quarter.
    for (auto& [aid, a] : w.space_assets)
        if (a.refit_done_tick >= 0 && econ_tick >= a.refit_done_tick)
        {
            a.refit_done_tick = -1;
            ++r.refits_done;
        }

    // The budgets, counted.
    for (const auto& [sid, st] : w.stations)
    {
        const station_budget b = evaluate_station_budget(w, stp, sid, econ_tick);
        r.modules_working += static_cast<int>(b.working.size());
        r.modules_stopped += static_cast<int>(b.stopped.size());
        r.modules_stormed += static_cast<int>(b.stormed.size());
    }
    if (out != nullptr)
        *out = r;
}
