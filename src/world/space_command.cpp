#include "space_command.hpp"

#include "recipe_registry.hpp"
#include "space_coverage.hpp"
#include "world.hpp"

#include <algorithm>
#include <utility>

// BL-1588 / BL-1587 — see space_command.hpp for the design.

namespace {

bool name_ok(const std::string& n)
{
    return !n.empty() && n.size() <= k_station_template_name_max;
}

bool node_ok(const world& w, const space_command& c)
{
    if (c.node_kind >= orbital_node_kind_count)
        return false;
    const auto bit = w.bodies.find(c.node_body);
    return bit != w.bodies.end() && bit->second.type != body_type::star;
}

orbital_node node_of(const space_command& c)
{
    return orbital_node{c.node_body, static_cast<orbital_node_kind>(c.node_kind)};
}

bool layout_ok(const station_template& t)
{
    if (t.level > k_station_max_level)
        return false;
    const int lw = station_grid_width(t.level), lh = station_grid_height(t.level);
    if (t.kinds.size() != static_cast<std::size_t>(lw * lh))
        return false;
    for (const uint8_t k : t.kinds)
        if (k != k_no_station_module && k >= station_fittable_kind_count)
            return false;
    return t.kinds[static_cast<std::size_t>((lh / 2) * lw + lw / 2)] == k_no_station_module;
}

/// The params a command launches with: the registry's, with the world's
/// fair-price ceiling copied in (BL-1172: it governs every goods draw).
space_asset_params launch_params(const recipe_registry& reg)
{
    space_asset_params sp = reg.space_assets();
    sp.reservation_mult = reg.price_band().reservation_mult;
    return sp;
}

space_command_outcome from_station(const station_op& op)
{
    space_command_outcome o;
    o.station    = op.result;
    o.launch     = op.launch;
    o.asset      = op.asset;
    o.station_id = op.station;
    o.result     = op.result == station_result::ok ? space_command_result::applied
                                                   : space_command_result::rejected_refused;
    return o;
}

space_command_outcome from_satellite(const satellite_order& r, entity_id asset)
{
    space_command_outcome o;
    o.satellite = r.result;
    o.launch    = r.launch;
    o.asset     = asset;
    o.result    = r.result == satellite_order_result::ok ? space_command_result::applied
                                                         : space_command_result::rejected_refused;
    return o;
}

/// The node an order acts at, for coverage: the station's, or the satellite's.
orbital_node target_node(const world& w, const space_command& c)
{
    const space_verb v = static_cast<space_verb>(c.verb);
    if (v == space_verb::retask_satellite || v == space_verb::move_satellite)
        return w.space_assets.at(c.subject).node;
    return w.stations.at(c.subject).node;
}

/// Apply a VALIDATED command now (no coverage check).
space_command_outcome execute(world& w, const recipe_registry& reg, const space_command& c)
{
    const space_asset_params sp  = launch_params(reg);
    const station_params&    stp = reg.stations();
    switch (static_cast<space_verb>(c.verb))
    {
        case space_verb::launch:
        {
            const space_launch_quote q =
                launch_space_asset(w, sp, c.nation, static_cast<space_asset_class>(c.cls), node_of(c),
                                   static_cast<space_satellite_role>(c.role));
            space_command_outcome o;
            o.launch = q.result;
            o.asset  = q.asset;
            o.result = q.result == space_launch_result::launched ? space_command_result::applied
                                                                 : space_command_result::rejected_refused;
            return o;
        }
        case space_verb::found_station:
            return from_station(found_station(w, sp, stp, c.nation, node_of(c)));
        case space_verb::fit_module:
            return from_station(fit_station_module(w, sp, stp, c.nation, c.subject, c.slot,
                                                   static_cast<station_module_kind>(c.module)));
        case space_verb::remove_module:
            return from_station(remove_station_module(w, c.nation, c.subject, c.slot));
        case space_verb::refit_module:
            return from_station(refit_station_module(w, stp, c.nation, c.subject, c.slot, c.slot_to));
        case space_verb::upgrade_station:
            return from_station(upgrade_station(w, sp, c.nation, c.subject));
        case space_verb::apply_template:
        {
            const station_template_report rep = apply_station_template(w, sp, stp, c.nation, c.subject, c.name);
            space_command_outcome o;
            o.station    = rep.result;
            o.station_id = c.subject;
            o.fitted     = static_cast<int>(rep.fitted.size());
            o.skipped    = static_cast<int>(rep.skipped.size());
            o.result     = rep.result == station_result::ok ? space_command_result::applied
                                                            : space_command_result::rejected_refused;
            return o;
        }
        case space_verb::save_template:
        {
            space_command_outcome o;
            o.station = save_station_template(w, c.nation, c.name, c.layout);
            o.result  = o.station == station_result::ok ? space_command_result::applied
                                                        : space_command_result::rejected_refused;
            return o;
        }
        case space_verb::delete_template:
        {
            space_command_outcome o;
            o.station = delete_station_template(w, c.nation, c.name);
            o.result  = o.station == station_result::ok ? space_command_result::applied
                                                        : space_command_result::rejected_refused;
            return o;
        }
        case space_verb::retask_satellite:
            return from_satellite(retask_satellite(w, c.nation, c.subject,
                                                   static_cast<space_satellite_role>(c.role)),
                                  c.subject);
        case space_verb::move_satellite:
            return from_satellite(move_satellite(w, sp, c.nation, c.subject, node_of(c)), c.subject);
    }
    return {}; // unreachable: validated
}

} // namespace

space_command_result validate_space_command(const world& w, const space_command& c)
{
    if (c.nation == null_entity || w.nations.count(c.nation) == 0)
        return space_command_result::rejected_unknown_nation;
    if (c.verb >= space_verb_count)
        return space_command_result::rejected_invalid;

    // The two subject kinds, existence then ownership.
    const auto station_check = [&]() {
        const auto it = w.stations.find(c.subject);
        if (c.subject == null_entity || it == w.stations.end())
            return space_command_result::rejected_invalid;
        if (it->second.owner != c.nation)
            return space_command_result::rejected_not_owner;
        return space_command_result::applied;
    };
    const auto slot_in = [&](int32_t s) {
        const auto& slots = w.stations.at(c.subject).slots;
        return s >= 0 && static_cast<std::size_t>(s) < slots.size();
    };
    const auto satellite_check = [&]() {
        const auto it = w.space_assets.find(c.subject);
        if (c.subject == null_entity || it == w.space_assets.end()
            || it->second.cls != space_asset_class::satellite)
            return space_command_result::rejected_invalid;
        if (it->second.owner != c.nation)
            return space_command_result::rejected_not_owner;
        return space_command_result::applied;
    };

    switch (static_cast<space_verb>(c.verb))
    {
        case space_verb::launch:
            // Station classes go through found_station / fit_module, never here.
            if (c.cls > static_cast<uint8_t>(space_asset_class::crewed_mission)
                || c.role >= space_satellite_role_count || !node_ok(w, c))
                return space_command_result::rejected_invalid;
            return space_command_result::applied;
        case space_verb::found_station:
            return node_ok(w, c) ? space_command_result::applied : space_command_result::rejected_invalid;
        case space_verb::fit_module:
        {
            const space_command_result r = station_check();
            if (r != space_command_result::applied)
                return r;
            return slot_in(c.slot) && c.module < station_fittable_kind_count
                       ? space_command_result::applied : space_command_result::rejected_invalid;
        }
        case space_verb::remove_module:
        {
            const space_command_result r = station_check();
            if (r != space_command_result::applied)
                return r;
            return slot_in(c.slot) ? space_command_result::applied : space_command_result::rejected_invalid;
        }
        case space_verb::refit_module:
        {
            const space_command_result r = station_check();
            if (r != space_command_result::applied)
                return r;
            return slot_in(c.slot) && slot_in(c.slot_to) && c.slot != c.slot_to
                       ? space_command_result::applied : space_command_result::rejected_invalid;
        }
        case space_verb::apply_template:
        {
            const space_command_result r = station_check();
            if (r != space_command_result::applied)
                return r;
            return name_ok(c.name) ? space_command_result::applied : space_command_result::rejected_invalid;
        }
        case space_verb::save_template:
            return name_ok(c.name) && layout_ok(c.layout) ? space_command_result::applied
                                                          : space_command_result::rejected_invalid;
        case space_verb::delete_template:
            return name_ok(c.name) ? space_command_result::applied : space_command_result::rejected_invalid;
        case space_verb::upgrade_station:
            return station_check();
        case space_verb::retask_satellite:
        {
            const space_command_result r = satellite_check();
            if (r != space_command_result::applied)
                return r;
            return c.role >= 1 && c.role < space_satellite_role_count ? space_command_result::applied
                                                                      : space_command_result::rejected_invalid;
        }
        case space_verb::move_satellite:
        {
            const space_command_result r = satellite_check();
            if (r != space_command_result::applied)
                return r;
            return node_ok(w, c) ? space_command_result::applied : space_command_result::rejected_invalid;
        }
    }
    return space_command_result::rejected_invalid;
}

space_command_outcome apply_space_command(world& w, const recipe_registry& reg, const space_command& c)
{
    space_command_outcome o;
    o.result = validate_space_command(w, c);
    if (o.result != space_command_result::applied)
        return o;
    const space_verb v = static_cast<space_verb>(c.verb);
    if (space_verb_delayed(v) && !space_node_covered(w, c.nation, target_node(w, c)))
    {
        if (w.space_orders.size() >= k_space_orders_max || w.next_space_order_id == UINT32_MAX)
        {
            o.result = space_command_result::rejected_queue_full;
            return o;
        }
        space_order ord;
        ord.id       = w.next_space_order_id++;
        ord.due_tick = w.current_econ_tick + 1;
        ord.cmd      = c;
        o.order_id   = ord.id;
        w.space_orders.push_back(std::move(ord));
        o.result = space_command_result::queued;
        return o;
    }
    return execute(w, reg, c);
}

void run_space_orders(world& w, const recipe_registry& reg, int econ_tick, space_orders_report* out)
{
    space_orders_report r;
    if (!w.space_orders.empty())
    {
        std::vector<space_order> pending = std::move(w.space_orders);
        w.space_orders.clear();
        for (space_order& ord : pending) // stored order: ids ascend
        {
            if (ord.due_tick > econ_tick)
            {
                w.space_orders.push_back(std::move(ord));
                continue;
            }
            // Re-validated as it lands: the station may be gone, the slot filled.
            space_command_outcome o;
            o.result = validate_space_command(w, ord.cmd);
            if (o.result == space_command_result::applied)
                o = execute(w, reg, ord.cmd);
            if (o.result == space_command_result::applied)
                ++r.applied;
            else
            {
                ++r.dropped;
                r.dropped_orders.emplace_back(ord.id, o);
            }
        }
    }
    if (out != nullptr)
        *out = std::move(r);
}
