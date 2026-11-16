// ---------------------------------------------------------------------------
// blockade_harness — BL-1599 (blockade and bombardment), the proof
// ---------------------------------------------------------------------------
// docs/military/SPACE_FLEETS.md § Blockade, § Bombardment and fire at orbit. The
// space_convoy_harness fixture: a star, a home planet, its moon, a far planet; one
// market each; nation A holding the home and moon ground (tile_to_nation), with a
// staffed Launchpad on each, a Hauler design and every space key; nation B, every
// space key, with warships STAGED where a row needs them. Rows:
//
//   B1  BLOCKADE: a B missile frigate in home low orbit, B hostile to A, A unarmed
//       there -> home is blockaded for A (not for B; the home market is, the moon's
//       is not). A's raise at the pad, a hand LOAD, a hand UNLOAD, a refuel over the
//       pad and an asset launch are each refused `blockaded`, mutating nothing; a
//       Hauler trade out of home, and one into it, is not priced.
//   B1f CONTESTED: an A frigate there lifts the blockade; the load goes through.
//   B2  ESCORT: an escorted trade Hauler meeting a warring B frigate at a node of its
//       path OPENS the battle there (the convoy day, before any battle day), waits,
//       and the battle day then fights it.
//   B3  BOMBARD: the verb's refusals (unknown unit, not owner, no key, kinetic
//       armament, wrong node, a friendly target, a zero target, a Launchpad - whose
//       condition bites nothing), then an order on
//       A's plant: one day takes firepower x the dial (18) off its condition and
//       queues force_used A -> B (the Alarm), magnitude 18 x 0.1. A province order
//       strikes the hostile building in best condition.
//   B4  BATTERY: A's Orbital Defence Battery lands 12 - armour 8 = 4 on the B frigate
//       a day; at half condition its 6 is absorbed whole; a frigate at 3 hp is
//       destroyed and its standing order goes with it.
//   B5  PEACE: return to neutral -> no blockade, the raise goes through, every
//       bombard order is dropped the next day and the battery fires nothing.
//   B6  SAVE: a world holding two orders and a battery round-trips (equal hash,
//       byte-identical); the v87 prefix + 4 + 9n == the stream and reads with no
//       orders; tampered tails (kind, zero target, ghost unit, order, count,
//       truncation) are refused whole.
//   B7  DETERMINISM: two fixtures driven identically end with equal hash and stream.
//
// Build: node tools/verify/build_harness.js blockade_harness --run
// ---------------------------------------------------------------------------

#include "world/digitisation_tree_data.hpp"
#include "world/early_space_tree_data.hpp"
#include "world/economy_system.hpp"
#include "world/nation_tech.hpp"
#include "world/orbital_graph.hpp"
#include "world/recipe_registry.hpp"
#include "world/sentiment.hpp"
#include "world/space_assets.hpp"
#include "world/space_battle.hpp"
#include "world/space_bombard.hpp"
#include "world/space_convoy.hpp"
#include "world/space_tech_keys.hpp"
#include "world/space_units.hpp"
#include "world/stance.hpp"
#include "world/stations.hpp"
#include "world/supply_system.hpp"
#include "world/war.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
int g_pass = 0;

void check(bool ok, const char* row, const std::string& what)
{
    std::printf("  [%s] %s %s\n", ok ? "PASS" : "FAIL", row, what.c_str());
    if (ok) ++g_pass; else ++g_fail;
}

std::string fmt(const char* f, ...)
{
    char buf[1000];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

std::string to_bytes(const world& w)
{
    std::ostringstream os(std::ios::binary);
    write_world_snapshot(w, os);
    return os.str();
}

bool from_bytes(const std::string& b, world& w)
{
    std::istringstream is(b, std::ios::binary);
    return read_world_snapshot(w, is);
}

constexpr std::size_t PR  = static_cast<std::size_t>(resource_type::propellant);
constexpr std::size_t SC  = static_cast<std::size_t>(resource_type::spacecraft_components);
constexpr std::size_t ORE = static_cast<std::size_t>(resource_type::iron_ore);

struct fixture
{
    world     w;
    entity_id star = null_entity, home = null_entity, moon = null_entity, far = null_entity;
    entity_id a = null_entity, b = null_entity;
    entity_id home_tile = null_entity, moon_tile = null_entity, far_tile = null_entity;
    entity_id home_mkt = null_entity, moon_mkt = null_entity, far_mkt = null_entity;
    entity_id home_pad = null_entity, moon_pad = null_entity;
    orbital_node home_low{}, home_high{}, moon_low{};
};

entity_id add_body(world& w, const char* name, body_type t, entity_id parent, float au, float mass,
                   atmosphere_class atm)
{
    const entity_id id = w.create_entity();
    body_component bc;
    bc.name = name;
    bc.type = t;
    bc.parent = parent;
    bc.orbital_radius_au = au;
    bc.orbital_angle_rad = 0.0f;
    bc.grid_width = 0;
    bc.grid_height = 0;
    bc.survey = {};
    bc.mass_earths = mass;
    bc.atmosphere = atm;
    w.bodies.emplace(id, bc);
    return id;
}

entity_id add_tile(world& w, entity_id body)
{
    const entity_id id = w.create_entity();
    tile_component tc{};
    tc.body = body;
    tc.grid_x = 0;
    tc.grid_y = 0;
    w.tiles.emplace(id, tc);
    return id;
}

entity_id add_market(world& w, entity_id body, entity_id centre)
{
    const entity_id id = w.create_entity();
    market_component m{};
    m.body = body;
    m.centre_tile = centre;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        m.base_price[r] = 50.0f;
        m.price[r] = 0.0f; // posted = base
        m.inventory[r] = 5000.0f;
    }
    m.base_price[PR] = 64.0f;
    m.base_price[SC] = 310.0f;
    w.markets.emplace(id, m);
    return id;
}

entity_id add_building(world& w, entity_id nation, entity_id tile, building_type t, float staffed = 1.0f)
{
    const entity_id bid = w.create_entity();
    building_component b{};
    b.tile = tile;
    b.type = t;
    b.workforce_assigned = staffed;
    b.ticks_remaining = 0;
    w.buildings.emplace(bid, b);
    w.nations.at(nation).state_assets.push_back(bid);
    return bid;
}

void grant_keys(world& w, entity_id nation)
{
    nation_tech_state& st = w.nation_tech[nation];
    for (const io::es::effect& e : io::es::effects)
        if (e.kind == io::es::effect_kind::mechanic && e.node >= 0 && e.node < io::es::node_count)
            st.es_held.set(static_cast<std::size_t>(e.node));
    auto& held = st.held;
    held.set();
    for (int i = 0; i < io::dg::node_count; ++i)
    {
        const int ex = io::dg::nodes[static_cast<std::size_t>(i)].excludes;
        if (ex < 0 || !held.test(static_cast<std::size_t>(i)) || !held.test(static_cast<std::size_t>(ex)))
            continue;
        bool keyed = false;
        for (const std::string_view k : space_keys::near_space)
            keyed = keyed || node_has_mechanic(i, k);
        held.reset(static_cast<std::size_t>(keyed ? ex : i));
    }
}

using sk = space_unit_slot;

space_design design_of(std::initializer_list<std::pair<sk, const char*>> items)
{
    space_design d;
    d.fill(k_no_space_item);
    const auto& cat = space_item_catalogue();
    for (const auto& [slot, id] : items)
        for (std::size_t i = 0; i < cat.size(); ++i)
            if (std::strcmp(cat[i].id, id) == 0)
                d[static_cast<std::size_t>(slot)] = static_cast<uint8_t>(i);
    return d;
}

space_design hauler_d()
{
    return design_of({{sk::drive, "drive.nuclear_thermal"}, {sk::tanks, "tanks.extended"}, {sk::bay, "bay.cargo"}});
}
space_design kinetic_d()
{
    return design_of({{sk::drive, "drive.nuclear_thermal"}, {sk::tanks, "tanks.extended"},
                      {sk::armament, "armament.kinetic"}, {sk::armour, "armour.whipple"}});
}
space_design missile_d()
{
    return design_of({{sk::drive, "drive.nuclear_thermal"}, {sk::tanks, "tanks.extended"},
                      {sk::armament, "armament.missile"}, {sk::armour, "armour.whipple"}});
}

/// Stage an idle frigate of @p nation at @p node (no raise: a fixture's warship).
entity_id stage_frigate(world& w, entity_id nation, orbital_node node, const space_design& d)
{
    const space_design_stats st = evaluate_space_design(space_hull::frigate, d);
    space_unit u;
    u.id = w.create_entity();
    u.owner = nation;
    u.hull = space_hull::frigate;
    u.design = d;
    u.state = space_unit_state::idle;
    u.node = node;
    u.dest = node;
    u.propellant = st.tank;
    u.hull_points = st.hull_points;
    u.crew = st.crew;
    u.upkeep = st.upkeep;
    w.space_units.emplace(u.id, u);
    return u.id;
}

void stage_war(world& w, entity_id x, entity_id y)
{
    war_record r;
    r.declarer = x;
    r.defender = y;
    w.wars[{x, y}] = r;
}

void build(fixture& f)
{
    world& w = f.w;
    f.star = add_body(w, "Star", body_type::star, null_entity, 0.0f, 1.0f, atmosphere_class::none);
    f.home = add_body(w, "Home", body_type::planet, null_entity, 1.0f, 1.0f, atmosphere_class::moderate);
    f.moon = add_body(w, "Moon", body_type::moon, f.home, 0.0026f, 0.012f, atmosphere_class::none);
    f.far  = add_body(w, "Far", body_type::planet, null_entity, 2.0f, 0.8f, atmosphere_class::thin);
    w.star_body = f.star;
    w.home_body = f.home;
    for (entity_id* n : {&f.a, &f.b})
    {
        *n = w.create_entity();
        nation_component nc;
        nc.name = (n == &f.a) ? "Alpha" : "Beta";
        w.nations.emplace(*n, nc);
    }
    f.home_tile = add_tile(w, f.home);
    f.moon_tile = add_tile(w, f.moon);
    f.far_tile  = add_tile(w, f.far);
    w.tile_to_nation[f.home_tile] = f.a;
    w.tile_to_nation[f.moon_tile] = f.a;
    f.home_mkt = add_market(w, f.home, f.home_tile);
    f.moon_mkt = add_market(w, f.moon, f.moon_tile);
    f.far_mkt  = add_market(w, f.far, f.far_tile);
    for (entity_id n : {f.a, f.b})
    {
        w.nations.at(n).capital_tile = f.home_tile;
        w.nations.at(n).treasury = 5000000.0f;
        w.nations.at(n).astronauts = 60;
        grant_keys(w, n);
    }
    f.home_pad = add_building(w, f.a, f.home_tile, building_type::launchpad);
    f.moon_pad = add_building(w, f.a, f.moon_tile, building_type::launchpad);
    w.buildings.at(f.home_pad).boost = 100000.0f; // BL-1637: banked, so a launch check is not also a boost check
    w.buildings.at(f.moon_pad).boost = 100000.0f;
    set_space_unit_design(w, f.a, space_hull::hauler, hauler_d());
    f.home_low  = {f.home, orbital_node_kind::low_orbit};
    f.home_high = {f.home, orbital_node_kind::high_orbit};
    f.moon_low  = {f.moon, orbital_node_kind::low_orbit};
    w.current_econ_tick = 1;
}

float ship(fixture& f, const recipe_registry& reg, float qty)
{
    logistics_nodes nodes;
    const convoy_leg leg = price_trade_leg(f.w, reg, nodes, f.a, f.home_mkt, f.moon_mkt, ORE, qty);
    economy_report report;
    float sent = 0.0f;
    if (!commit_trade_shipment(f.w, reg, report, f.a, f.home_mkt, f.moon_mkt, ORE, qty, leg, nullptr, &sent))
        return 0.0f;
    return sent;
}

struct snap
{
    std::uint64_t hash;
    std::size_t   units, hauls, assets;
    std::uint32_t cursor;
    float ta, hp, ho;
};

snap take(const fixture& f)
{
    const world& w = f.w;
    return {w.state_hash(0), w.space_units.size(), w.space_hauls.size(), w.space_assets.size(), w.next_entity_id(),
            w.nations.at(f.a).treasury, w.markets.at(f.home_mkt).inventory[PR], w.markets.at(f.home_mkt).inventory[ORE]};
}

bool same(const snap& x, const snap& y)
{
    return x.hash == y.hash && x.units == y.units && x.hauls == y.hauls && x.assets == y.assets
        && x.cursor == y.cursor && x.ta == y.ta && x.hp == y.hp && x.ho == y.ho;
}

/// One whole space day, as the economy runs it.
space_strike_day_report day(world& w, const recipe_registry& reg, space_unit_day_report* ur = nullptr)
{
    space_unit_day_report u;
    run_space_unit_day(w, reg.space_units(), reg.stations(), &u);
    run_space_battle_day(w, reg.space_units(), reg.stations(), reg.space_battles());
    space_strike_day_report s;
    run_space_strike_day(w, reg.space_battles(), &s);
    if (ur != nullptr)
        *ur = u;
    return s;
}

/// The B7 script, run twice.
void programme(fixture& f, const recipe_registry& reg)
{
    world& w = f.w;
    const entity_id plant = add_building(w, f.a, f.home_tile, building_type::processing_facility);
    add_building(w, f.a, f.home_tile, building_type::orbital_defence_battery, 0.0f);
    const entity_id m1 = stage_frigate(w, f.b, f.home_low, missile_d());
    stage_frigate(w, f.b, f.home_low, kinetic_d());
    declare_hostile(w, f.b, f.a);
    order_space_bombard(w, f.b, m1, space_bombard_target::building, plant);
    for (int i = 0; i < 6; ++i)
        day(w, reg);
}

} // namespace

int main()
{
    std::printf("blockade_harness — BL-1599\n");
    const recipe_registry reg;
    const space_unit_params& p = reg.space_units();
    const space_battle_params& bp = reg.space_battles();
    const space_design_stats ms = evaluate_space_design(space_hull::frigate, missile_d());
    const space_design_stats ks = evaluate_space_design(space_hull::frigate, kinetic_d());
    std::printf("dials: battery %.1f, bombard %.2f permille/fp, force %.2f/permille | missile frigate fp %.0f armour %.0f hp %.0f"
                " | kinetic fp %.0f\n",
                bp.battery_firepower, bp.bombard_condition_per_firepower, bp.bombard_force_per_permille, ms.firepower,
                ms.armour, ms.hull_points, ks.firepower);

    fixture f;
    build(f);
    world& w = f.w;

    // A's hand Hauler, raised and loaded before any hostility.
    const space_unit_op raised = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.home);
    const entity_id hand = raised.unit;
    const space_unit_op loaded = load_space_cargo(w, p, f.a, hand, ORE, 5.0f);
    w.space_units.at(hand).propellant *= 0.5f; // a half tank, so a refuel would buy
    const bool trade_open = quote_space_haul(w, p, f.a, f.home_mkt, f.moon_mkt).viable;
    std::printf("setup: hauler raised %d, loaded %d, trade quoted %d\n", static_cast<int>(raised.result),
                static_cast<int>(loaded.result), trade_open ? 1 : 0);

    // ---- B1 blockade ---------------------------------------------------------
    std::printf("--- B1 blockade ---\n");
    const entity_id m1 = stage_frigate(w, f.b, f.home_low, missile_d());
    declare_hostile(w, f.b, f.a);
    {
        check(space_blockaded(w, f.a, f.home) && !space_blockaded(w, f.b, f.home) && !space_blockaded(w, f.a, f.moon)
                  && space_market_blockaded(w, f.home_mkt) && !space_market_blockaded(w, f.moon_mkt),
              "B1a", "home is blockaded for A (not B, not the moon); the home market is, the moon's is not");
        const snap s0 = take(f);
        const space_unit_op r1 = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.home);
        const space_unit_op r2 = load_space_cargo(w, p, f.a, hand, ORE, 1.0f);
        const space_unit_op r3 = unload_space_cargo(w, p, f.a, hand);
        const space_unit_op r4 = refuel_space_unit(w, p, reg.stations(), f.a, hand);
        const space_launch_quote r5 = quote_space_launch(w, space_asset_params{}, f.a, space_asset_class::satellite,
                                                         f.home_low, space_satellite_role::comms);
        const space_launch_quote r5l = launch_space_asset(w, space_asset_params{}, f.a, space_asset_class::satellite,
                                                          f.home_low, space_satellite_role::comms);
        const snap s1 = take(f);
        constexpr auto B = space_unit_result::rejected_blockaded;
        check(r1.result == B && r2.result == B && r3.result == B && r4.result == B
                  && r5.result == space_launch_result::rejected_blockaded
                  && r5l.result == space_launch_result::rejected_blockaded && same(s0, s1),
              "B1b", fmt("raise %d, load %d, unload %d, refuel %d, launch %d/%d all refused blockaded; nothing written",
                         static_cast<int>(r1.result), static_cast<int>(r2.result), static_cast<int>(r3.result),
                         static_cast<int>(r4.result), static_cast<int>(r5.result), static_cast<int>(r5l.result)));
        logistics_nodes nodes;
        const convoy_leg out = price_trade_leg(w, reg, nodes, f.a, f.home_mkt, f.moon_mkt, ORE, 5.0f);
        const convoy_leg in  = price_trade_leg(w, reg, nodes, f.a, f.moon_mkt, f.home_mkt, ORE, 5.0f);
        check(trade_open && !quote_space_haul(w, p, f.a, f.home_mkt, f.moon_mkt).viable
                  && !quote_space_haul(w, p, f.a, f.moon_mkt, f.home_mkt).viable && !out.viable && !in.viable,
              "B1c", "a Hauler trade out of the blockaded body and one into it are not priced (both legs refused)");

        // Contested: an armed A unit there lifts it.
        const entity_id guard = stage_frigate(w, f.a, f.home_low, kinetic_d());
        const bool lifted = !space_blockaded(w, f.a, f.home) && !space_market_blockaded(w, f.home_mkt);
        const space_unit_op l2 = load_space_cargo(w, p, f.a, hand, ORE, 1.0f);
        check(lifted && l2.result == space_unit_result::ok, "B1d",
              fmt("an armed A frigate in the orbit contests it: not blockaded; the load goes through (%d)",
                  static_cast<int>(l2.result)));
        w.space_units.erase(guard);
    }

    // ---- B2 escort opens the battle -------------------------------------------
    std::printf("--- B2 escort ---\n");
    {
        fixture g;
        build(g);
        world& x = g.w;
        const orbital_path_result path = orbital_path(x, orbital_graph_params{}, g.home_low, g.moon_low);
        const orbital_node mid = path.nodes[path.nodes.size() / 2];
        const entity_id raider = stage_frigate(x, g.b, mid, kinetic_d());
        const entity_id escort = stage_frigate(x, g.a, mid, kinetic_d());
        const float sent = ship(g, reg, 10.0f);
        stage_war(x, g.b, g.a);
        declare_hostile(x, g.b, g.a);
        int battles = 0, stopped = 0, days_run = 0;
        for (; days_run < 40 && battles == 0; ++days_run)
        {
            space_unit_day_report r;
            run_space_unit_day(x, reg.space_units(), reg.stations(), &r); // the convoy day only
            battles += r.convoy.battles;
            stopped += r.convoy.stopped;
        }
        const space_battle* bt = x.space_battles.empty() ? nullptr : &x.space_battles.begin()->second;
        const bool opened = battles == 1 && x.space_battles.size() == 1 && bt->state == space_battle_state::open
                         && bt->node == mid && bt->days == 0 && bt->sides[0].nation == std::min(g.a, g.b)
                         && bt->sides[1].nation == std::max(g.a, g.b);
        space_battle_day_report br;
        run_space_battle_day(x, reg.space_units(), reg.stations(), reg.space_battles(), &br);
        check(sent > 0.0f && opened && stopped == 1 && br.opened == 0 && br.fought == 1 && bt->days == 1,
              "B2", fmt("sent %.1f; on day %d the escorted Hauler met the raider: the convoy day opened the battle "
                        "(%d), it waits (stopped %d); the battle day fought it (opened %d, fought %d)",
                        sent, days_run, battles, stopped, br.opened, br.fought));
        (void)raider;
        (void)escort;
    }

    // ---- B3 bombard -------------------------------------------------------------
    std::printf("--- B3 bombard ---\n");
    const entity_id plant  = add_building(w, f.a, f.home_tile, building_type::processing_facility);
    const entity_id k1     = stage_frigate(w, f.b, f.home_high, kinetic_d());
    const entity_id m_high = stage_frigate(w, f.b, f.home_high, missile_d());
    {
        const space_bombard_target BT = space_bombard_target::building;
        const snap s0 = take(f);
        const auto r_unknown = order_space_bombard(w, f.b, 0x00FFFFF0u, BT, plant);
        const auto r_owner   = order_space_bombard(w, f.a, m1, BT, plant);
        const auto r_zero    = order_space_bombard(w, f.b, m1, BT, 0);
        const auto r_kind    = order_space_bombard(w, f.b, m1, static_cast<space_bombard_target>(7), plant);
        const auto r_ghost   = order_space_bombard(w, f.b, m1, BT, 0x00FFFFF1u);
        nation_tech_state saved_tech = w.nation_tech[f.b];
        w.nation_tech[f.b].es_held.reset();
        const auto r_key = order_space_bombard(w, f.b, m1, BT, plant);
        w.nation_tech[f.b] = saved_tech;
        const auto r_kin  = order_space_bombard(w, f.b, k1, BT, plant);
        const auto r_node = order_space_bombard(w, f.b, m_high, BT, plant);
        const entity_id own = add_building(w, f.b, f.home_tile, building_type::processing_facility);
        const auto r_own  = order_space_bombard(w, f.b, m1, BT, own);
        w.buildings.erase(own);
        w.nations.at(f.b).state_assets.clear();
        const auto r_clear = order_space_bombard(w, f.b, m1, BT, plant, true);
        const auto r_pad   = order_space_bombard(w, f.b, m1, BT, f.home_pad); // a pad's condition bites nothing
        const snap s1 = take(f);
        using R = space_unit_result;
        check(r_unknown == R::rejected_unknown_unit && r_owner == R::rejected_not_owner && r_zero == R::rejected_invalid
                  && r_kind == R::rejected_invalid && r_ghost == R::rejected_invalid && r_key == R::rejected_tech_locked
                  && r_kin == R::rejected_not_armed && r_node == R::rejected_wrong_node && r_own == R::rejected_target
                  && r_clear == R::rejected_invalid && r_pad == R::rejected_target && w.space_bombard_orders.empty()
                  && s0.units == s1.units,
              "B3a", fmt("refused: unknown %d, owner %d, zero %d, kind %d, ghost %d, key %d, kinetic %d, node %d, "
                         "own building %d, clear-nothing %d, a Launchpad %d; no order written",
                         int(r_unknown), int(r_owner), int(r_zero), int(r_kind), int(r_ghost), int(r_key), int(r_kin),
                         int(r_node), int(r_own), int(r_clear), int(r_pad)));

        const auto ok = order_space_bombard(w, f.b, m1, BT, plant);
        w.sentiment_pending.clear();
        const space_strike_day_report r = day(w, reg);
        const int expect = static_cast<int>(std::floor(ms.firepower * bp.bombard_condition_per_firepower));
        bool alarm = false;
        for (const sentiment_event& e : w.sentiment_pending)
            alarm = alarm || (e.observer == f.a && e.subject == f.b && e.kind == sentiment_factor_kind::force_used
                              && std::fabs(e.magnitude - expect * bp.bombard_force_per_permille) < 1.0e-4f);
        check(ok == R::ok && r.strikes == 1 && r.condition_lost == expect
                  && w.buildings.at(plant).condition_permille == 1000 - expect && alarm && w.space_bombard_orders.size() == 1,
              "B3b", fmt("one day: condition %d (-%d = fp %.0f x %.2f); force_used A -> B queued, magnitude %.2f (the "
                         "Alarm); the order stands",
                         w.buildings.at(plant).condition_permille, r.condition_lost, ms.firepower,
                         bp.bombard_condition_per_firepower, expect * bp.bombard_force_per_permille));

        // A province order: the hostile building in best condition, ties to the lowest id.
        fixture h;
        build(h);
        province pr;
        pr.id = h.home_tile;
        pr.body = h.home;
        pr.tiles = {h.home_tile};
        h.w.provinces.provinces.push_back(pr);
        h.w.provinces.tile_province[h.home_tile] = h.home_tile;
        const entity_id worn = add_building(h.w, h.a, h.home_tile, building_type::processing_facility);
        const entity_id whole = add_building(h.w, h.a, h.home_tile, building_type::processing_facility);
        h.w.buildings.at(worn).condition_permille = 900;
        const entity_id hm = stage_frigate(h.w, h.b, h.home_low, missile_d());
        declare_hostile(h.w, h.b, h.a);
        const auto pok = order_space_bombard(h.w, h.b, hm, space_bombard_target::province, h.home_tile);
        const entity_id picked = space_bombard_pick(h.w, h.b, h.w.space_bombard_orders.at(hm));
        const space_strike_day_report pr_r = day(h.w, reg);
        std::printf("  province day: strikes %d lost %d dropped %d orders %zu\n", pr_r.strikes, pr_r.condition_lost, pr_r.orders_dropped, h.w.space_bombard_orders.size());
        check(pok == R::ok && picked == whole && h.w.buildings.at(whole).condition_permille == 1000 - expect
                  && h.w.buildings.at(worn).condition_permille == 900,
              "B3c", fmt("a province order strikes the building in best condition (%u at 1000, not %u at 900) -> %d",
                         whole, worn, h.w.buildings.at(whole).condition_permille));
    }

    // ---- B4 battery ---------------------------------------------------------------
    std::printf("--- B4 battery ---\n");
    const entity_id battery = add_building(w, f.a, f.home_tile, building_type::orbital_defence_battery, 0.0f);
    {
        const float hp0 = w.space_units.at(m1).hull_points;
        const space_strike_day_report r = day(w, reg);
        const float hp1 = w.space_units.at(m1).hull_points;
        const float lands = bp.battery_firepower - ms.armour;
        check(r.battery_shots == 1 && std::fabs((hp0 - hp1) - lands) < 1.0e-4f && std::fabs(r.battery_landed - lands) < 1.0e-4f,
              "B4a", fmt("the battery's %.0f - armour %.0f lands %.0f on the missile frigate (hp %.0f -> %.0f); the high-orbit "
                         "frigates are out of its reach",
                         bp.battery_firepower, ms.armour, lands, hp0, hp1));
        w.buildings.at(battery).condition_permille = 500;
        const space_strike_day_report r2 = day(w, reg);
        check(r2.battery_shots == 1 && r2.battery_landed == 0.0f && w.space_units.at(m1).hull_points == hp1,
              "B4b", "at half condition its 6 is absorbed whole by armour 8: nothing lands");
        w.buildings.at(battery).condition_permille = 1000;
        w.space_units.at(m1).hull_points = 3.0f;
        const float before_cond = static_cast<float>(w.buildings.at(plant).condition_permille);
        const space_strike_day_report r3 = day(w, reg);
        check(r3.units_lost == 1 && w.space_units.count(m1) == 0 && w.space_bombard_orders.empty() && r3.strikes == 1
                  && r3.mass_destroyed.size() == 1 && r3.mass_destroyed[0].first == f.home_low
                  && std::fabs(r3.mass_destroyed[0].second - ms.dry_mass) < 1.0e-4f,
              "B4c", fmt("a frigate at 3 hp is destroyed (crew %d, mass %.0f to the debris seam); its standing order goes with "
                         "it - its last strike, fired the same day, still lands (condition %.0f -> %d)",
                         r3.crew_lost, ms.dry_mass, before_cond, w.buildings.at(plant).condition_permille));
    }

    // ---- B5 peace ------------------------------------------------------------------
    std::printf("--- B5 peace ---\n");
    {
        const entity_id m2 = stage_frigate(w, f.b, f.home_low, missile_d());
        const auto o2 = order_space_bombard(w, f.b, m2, space_bombard_target::building, plant);
        const bool blockaded = space_blockaded(w, f.a, f.home);
        const stance_result sr = return_to_neutral(w, f.b, f.a);
        const bool lifted = !space_blockaded(w, f.a, f.home) && !space_market_blockaded(w, f.home_mkt);
        const int cond = w.buildings.at(plant).condition_permille;
        const float hp = w.space_units.at(m2).hull_points;
        const space_strike_day_report r = day(w, reg);
        const space_unit_op raise = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.home);
        const space_unit_op load = load_space_cargo(w, p, f.a, hand, ORE, 1.0f);
        check(o2 == space_unit_result::ok && blockaded && sr == stance_result::applied && lifted && r.orders_dropped == 1
                  && r.strikes == 0 && r.battery_shots == 0 && w.space_bombard_orders.empty()
                  && w.buildings.at(plant).condition_permille == cond && w.space_units.at(m2).hull_points == hp
                  && raise.result == space_unit_result::ok && load.result == space_unit_result::ok
                  && quote_space_haul(w, p, f.a, f.home_mkt, f.moon_mkt).viable,
              "B5", "peace lifts it all: no blockade, the raise, load and trade go through, the order is dropped, "
                    "nothing strikes and the battery holds its fire");
        declare_hostile(w, f.b, f.a);
        order_space_bombard(w, f.b, m2, space_bombard_target::building, plant);
        const entity_id m3 = stage_frigate(w, f.b, f.home_low, missile_d());
        order_space_bombard(w, f.b, m3, space_bombard_target::building, battery);
    }

    // ---- B6 save -------------------------------------------------------------------
    std::printf("--- B6 save ---\n");
    {
        const std::string b = to_bytes(w);
        world w2;
        const bool ok = from_bytes(b, w2);
        const std::string b2 = ok ? to_bytes(w2) : std::string();
        check(ok && b2 == b && w2.state_hash(0) == w.state_hash(0) && w2.space_bombard_orders == w.space_bombard_orders
                  && w.space_bombard_orders.size() == 2
                  && w2.buildings.at(battery).type == building_type::orbital_defence_battery,
              "B6a", fmt("two orders and a battery round-trip: equal hash, byte-identical (%zu bytes)", b.size()));
        const std::size_t sec = world_save_space_bombard_section_bytes(w);
        std::ostringstream o87(std::ios::binary);
        write_world_snapshot_pre_space_bombard(w, o87);
        fixture e;
        build(e);
        std::ostringstream oe(std::ios::binary);
        write_world_snapshot_pre_space_bombard(e.w, oe);
        world we;
        check(o87.str().size() + world_save_space_tail_bytes_after(k_world_save_version_research_books, w) /* the space merge: a v89 stream carries no space section */ == b.size() && sec == 4 + 9 * 2 && from_bytes(oe.str(), we)
                  && we.space_bombard_orders.empty(), // the space merge: v89 drops every space section, so not the space world's hash
              "B6b", fmt("v89 (no space section) %zu, section %zu, today %zu; an order-free v89 stream reads with none",
                         o87.str().size(), sec, b.size()));
        // The v88 day after a load is the original's day.
        day(w2, reg);
        world w3 = w;
        day(w3, reg);
        check(w2.state_hash(0) == w3.state_hash(0) && to_bytes(w2) == to_bytes(w3), "B6c",
              "the loaded world strikes and fires the next day exactly as the original");

        const std::size_t s0 = b.size() - sec - world_save_space_tail_bytes_after(k_world_save_version_space_bombard, w); // BL-1632: the v90 base section (BL-1601) and every later one follow
        const auto refused = [&](const std::string& bad) {
            world t;
            t.nations.emplace(999, nation_component{});
            return !from_bytes(bad, t) && t.nations.size() == 1 && t.nations.count(999) == 1;
        };
        const auto poke = [&](std::size_t at, const void* v, std::size_t n) {
            std::string bad = b;
            std::memcpy(&bad[at], v, n);
            return bad;
        };
        bool all = true;
        std::string why;
        const auto expect = [&](bool r, const char* what) { all = all && r; if (!r) { why += " "; why += what; } };
        const uint8_t bad_kind = 2;
        const uint32_t zero = 0, ghost = 0x00FFFFF2u, many = 0x7FFFFFFFu;
        uint32_t first = 0, second = 0;
        std::memcpy(&first, &b[s0 + 4], 4);
        std::memcpy(&second, &b[s0 + 13], 4);
        expect(refused(poke(s0 + 8, &bad_kind, 1)), "kind");
        expect(refused(poke(s0 + 9, &zero, 4)), "zero-target");
        expect(refused(poke(s0 + 4, &ghost, 4)), "ghost-unit");
        expect(refused(poke(s0 + 4, &second, 4)), "out-of-order");
        expect(refused(poke(s0 + 0, &many, 4)), "count");
        expect(refused(b.substr(0, b.size() - 1)), "truncated");
        expect(!refused(b), "clean");
        check(all && first < second, "B6d", "tampered tails refused whole (kind, zero target, ghost unit, order, count, "
                                            "truncation); the clean stream reads" +
                                                (why.empty() ? std::string() : " — FAILED:" + why));
    }

    // ---- B7 determinism --------------------------------------------------------------
    std::printf("--- B7 determinism ---\n");
    {
        fixture x, y;
        build(x);
        build(y);
        programme(x, reg);
        programme(y, reg);
        check(x.w.state_hash(0) == y.w.state_hash(0) && to_bytes(x.w) == to_bytes(y.w) && !x.w.space_bombard_orders.empty(),
              "B7", fmt("two runs: equal hash %016llx, identical stream, %zu orders standing",
                        static_cast<unsigned long long>(x.w.state_hash(0)), x.w.space_bombard_orders.size()));
    }

    std::printf("\nblockade_harness: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
