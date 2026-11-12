// ---------------------------------------------------------------------------
// propellant_loop_harness — BL-1604 (propellant loop)
// ---------------------------------------------------------------------------
// docs/economy/ERAS.md § The propellant loop; docs/economy/PRODUCTION.md
// § Extraction buildings and § Propellant Plant; docs/military/SPACE_FLEETS.md
// § Thrust — the Δv budget; docs/economy/SPACE_ASSETS.md (the Fuel depot).
//
// A hand-built fixture on base_harness's pattern: a star, a home planet (all
// Alpha's, a centre, a stocked market, a staffed Launchpad), its airless moon
// with an ice deposit, and a base Alpha founds there through the space command
// seam. Alpha holds every far-space key; a row that needs one missing clears its
// node and restores it. Rows (multi-tick wherever a number is produced):
//
//   I1  the Ice Extractor (an extraction site drawing water off an ice deposit) on
//       base ground is refused `tech_locked` without Ice Extraction; nothing written.
//   I2  ...and on the HOME body it needs no key (the deposit route stays open there).
//   I3  with the key it is placed, and over six ticks of production and clearing it
//       extracts water every tick onto the moon's own shelf.
//   E1  the Electrolysis Plant (`propellant_electrolysis`, the Chemical Plant's
//       airless route) is refused `tech_locked` on base ground without Electrolysis
//       Plants; the atmospheric route is refused `wrong_air`; nothing written. The
//       retool door refuses the same switch `tech_locked`.
//   E2  with the key it is placed, and over six ticks it turns the extractor's water
//       (and refined fuel stocked on the shelf, the shipped half) into propellant on
//       the moon's market.
//   R1  a Hauler idle in the moon's low orbit, its owner holding a staffed pad on
//       base ground, refuels off the MOON's shelf: the moon's propellant falls by the
//       top-up and the home shelf is untouched.
//   D1  a station in the moon's low orbit with a working Fuel depot is stocked off
//       the moon's shelf; the home shelf is untouched.
//   D2  the same station with no base below is stocked off the home shelf (the
//       rule without a base is unchanged).
//   X1  two fixtures driven identically end with equal hash and equal stream.
//
// Build: node tools/verify/build_harness.js propellant_loop_harness --run
// ---------------------------------------------------------------------------

#include "world/construction.hpp"
#include "world/early_space_tree_data.hpp"
#include "world/digitisation_tree_data.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/nation_tech.hpp"
#include "world/offworld_base.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/space_assets.hpp"
#include "world/space_command.hpp"
#include "world/space_tech_keys.hpp"
#include "world/space_units.hpp"
#include "world/stations.hpp"
#include "world/survey_system.hpp"
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

namespace {

int g_fail = 0;
int g_pass = 0;
bool g_diag = false;

void check(bool ok, const char* row, const std::string& what)
{
    std::printf("  [%s] %s %s\n", ok ? "PASS" : "FAIL", row, what.c_str());
    if (ok) ++g_pass; else ++g_fail;
}

std::string fmt(const char* f, ...)
{
    char buf[700];
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

constexpr std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }
constexpr std::size_t WA = ri(resource_type::water);
constexpr std::size_t RF = ri(resource_type::refined_fuel);
constexpr std::size_t PR = ri(resource_type::propellant);

constexpr int GW = 16, GH = 8;
constexpr int k_ticks = 4; ///< Production ticks the programme drives (see build()).

struct fixture
{
    world     w;
    entity_id star = null_entity, home = null_entity, moon = null_entity;
    entity_id a = null_entity;
    entity_id home_centre = null_entity, home_mkt = null_entity, home_ice = null_entity;
    entity_id moon_tile[GW * GH] = {};
    entity_id passenger = null_entity, carrier = null_entity;
};

entity_id add_body(world& w, const char* name, body_type t, entity_id parent, float au, float mass,
                   int gw, int gh, atmosphere_class atm)
{
    const entity_id id = w.create_entity();
    body_component bc;
    bc.name = name;
    bc.type = t;
    bc.parent = parent;
    bc.orbital_radius_au = au;
    bc.orbital_angle_rad = 0.0f;
    bc.mass_earths = mass;
    bc.grid_width = gw;
    bc.grid_height = gh;
    bc.survey = {};
    bc.atmosphere = atm;
    w.bodies.emplace(id, bc);
    return id;
}

entity_id add_tile(world& w, entity_id body, int x, int y, terrain_substrate sub, float hab)
{
    const entity_id id = w.create_entity();
    tile_component tc{};
    tc.body = body;
    tc.grid_x = x;
    tc.grid_y = y;
    tc.substrate = sub;
    tc.landform = terrain_landform::plains;
    tc.habitability = hab;
    tc.hazard_level = 0.0f;
    if (sub != terrain_substrate::ocean)
        tc.road_edges = road_edges_on_every_side(1); // BL-1347: a building needs a road
    w.tiles.emplace(id, tc);
    return id;
}

void stock(market_component& m, float qty)
{
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        m.base_price[r] = 10.0f;
        m.inventory[r] = qty;
    }
    m.base_price[PR] = 64.0f;
    m.base_price[ri(resource_type::spacecraft_components)] = 310.0f;
    m.price = m.base_price;
}

entity_id add_pad(world& w, entity_id nation, entity_id tile)
{
    const entity_id pad = w.create_entity();
    building_component bc{};
    bc.tile = tile;
    bc.type = building_type::launchpad;
    bc.workforce_assigned = 1.0f;
    bc.ticks_remaining = 0;
    w.buildings.emplace(pad, bc);
    w.nations.at(nation).state_assets.push_back(pad);
    return pad;
}

/// The Early Space node carrying @p key's mechanic, or -1.
int es_node_of(std::string_view key)
{
    for (const io::es::effect& e : io::es::effects)
        if (e.kind == io::es::effect_kind::mechanic && key == e.target)
            return e.node;
    return -1;
}

void grant_far_space(world& w, entity_id nation)
{
    nation_tech_state& st = w.nation_tech[nation];
    for (const io::es::effect& e : io::es::effects)
        if (e.kind == io::es::effect_kind::mechanic && e.node >= 0 && e.node < io::es::node_count)
            st.es_held.set(static_cast<std::size_t>(e.node));
    // The Digitisation tree's near-space keys (the station core's launch gate), one
    // of each exclusive pair, as space_units_harness grants them.
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

void set_key(world& w, entity_id nation, std::string_view key, bool on)
{
    const int n = es_node_of(key);
    if (n < 0)
        return;
    if (on) w.nation_tech[nation].es_held.set(static_cast<std::size_t>(n));
    else    w.nation_tech[nation].es_held.reset(static_cast<std::size_t>(n));
}

space_design design_of(std::initializer_list<std::pair<space_unit_slot, const char*>> items)
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

entity_id stage_unit(world& w, entity_id owner, space_hull hull, const char* bay, orbital_node node,
                     space_unit_state state)
{
    const entity_id id = w.create_entity();
    space_unit u;
    u.id = id;
    u.owner = owner;
    u.hull = hull;
    u.design = design_of({{space_unit_slot::drive, "drive.chemical"}, {space_unit_slot::tanks, "tanks.extended"},
                          {space_unit_slot::bay, bay}});
    u.state = state;
    u.node = node;
    u.dest = node;
    u.hull_points = evaluate_space_design(u.hull, u.design).hull_points;
    w.space_units.emplace(id, u);
    return id;
}

/// The registry: extraction and processing economics, and the two propellant
/// routes as scripts/recipes.lua authors them (the airless one names its key).
struct regs
{
    recipe_registry reg;
    uint16_t elec = no_recipe, atmo = no_recipe;
};

regs make_reg()
{
    regs r;
    building_economics ex;
    ex.base_rate = 10.0f;
    ex.build_cost = 100.0f;
    ex.build_duration_ticks = 1.0f;
    ex.richness_reference = 24.9f;
    r.reg.set_economics(building_type::extraction_site, ex);
    building_economics pf;
    pf.base_rate = 2.0f;
    pf.build_cost = 100.0f;
    pf.build_duration_ticks = 1.0f;
    r.reg.set_economics(building_type::processing_facility, pf);
    building_economics hub;
    hub.build_cost = 50.0f; // the Logistic Points the base's producers run on
    r.reg.set_economics(building_type::inland_logistics_hub, hub);
    recipe ra;
    ra.name = "propellant_atmospheric";
    ra.group = "Propellant Plant";
    ra.air = recipe_air::atmosphere;
    ra.inputs[RF] = 2.0f;
    ra.outputs[PR] = 1.0f;
    r.atmo = r.reg.add_recipe(ra);
    recipe rv;
    rv.name = "propellant_electrolysis";
    rv.group = "Propellant Plant";
    rv.air = recipe_air::airless;
    rv.mechanic = std::string(space_keys::electrolysis_plant);
    rv.inputs[WA] = 3.0f;
    rv.inputs[RF] = 1.0f;
    rv.outputs[PR] = 1.0f;
    r.elec = r.reg.add_recipe(rv);
    return r;
}

void build(fixture& f)
{
    world& w = f.w;
    f.star = add_body(w, "Star", body_type::star, null_entity, 0.0f, 1.0f, 0, 0, atmosphere_class::none);
    f.home = add_body(w, "Home", body_type::planet, null_entity, 1.0f, 1.0f, GW, GH, atmosphere_class::moderate);
    f.moon = add_body(w, "Moon", body_type::moon, f.home, 0.0026f, 0.012f, GW, GH, atmosphere_class::none);
    w.star_body = f.star;
    w.home_body = f.home;
    w.bodies.at(f.home).survey.phase = survey_phase::surveyed;
    f.a = w.create_entity();
    {
        nation_component nc;
        nc.name = "Alpha";
        nc.treasury = 5000000.0f;
        nc.astronauts = 60;
        w.nations.emplace(f.a, nc);
    }
    // A seat that is not Alpha: the game steps the seated player's workforce, and the
    // rows pin it. Alpha is therefore scored; its first evaluation falls at econ tick 5
    // on the staggered cadence, so the programme drives ticks 1-4 (k_ticks) and no
    // scored build lands inside a row.
    {
        const entity_id seat = w.create_entity();
        w.nations.emplace(seat, nation_component{});
        w.player_entity = seat;
    }

    entity_id home_tile = null_entity;
    for (int y = 0; y < GH; ++y)
        for (int x = 0; x < GW; ++x)
        {
            const entity_id t = add_tile(w, f.home, x, y, terrain_substrate::sedimentary, 1.0f);
            if (x == 2 && y == 2) home_tile = t;
            if (x == 9 && y == 0) f.home_ice = t;
            w.set_tile_nation(t, f.a);
            w.nations.at(f.a).tiles.push_back(t);
        }
    w.tiles.at(f.home_ice).resource_deposit[WA] = 24.9f;   // polar ice on the home world
    w.tiles.at(f.home_ice).resource_remaining[WA] = 1.0e6f;
    w.nations.at(f.a).capital_tile = home_tile;
    f.home_centre = w.create_entity();
    {
        population_centre_component pc;
        pc.scale = 3;
        pc.population = 50;
        w.population_centres[f.home_centre] = pc;
        w.population_centre_tile[f.home_centre] = home_tile;
    }
    f.home_mkt = w.create_entity();
    {
        market_component m{};
        m.body = f.home;
        m.centre_tile = home_tile;
        stock(m, 5000.0f);
        w.markets.emplace(f.home_mkt, m);
    }
    add_pad(w, f.a, home_tile);
    build_province_partition(w, 0x1604u);
    seed_province_holders(w);

    // The moon: regolith at habitability 0; an ice deposit at (5, 3).
    for (int y = 0; y < GH; ++y)
        for (int x = 0; x < GW; ++x)
            f.moon_tile[y * GW + x] = add_tile(w, f.moon, x, y, terrain_substrate::regolith, 0.0f);
    {
        tile_component& tc = w.tiles.at(f.moon_tile[3 * GW + 5]);
        tc.resource_deposit[WA] = 24.9f;
        tc.resource_remaining[WA] = 1.0e6f;
    }
    w.body_tile_index.clear();
    grant_far_space(w, f.a);

    f.passenger = stage_unit(w, f.a, space_hull::lander, "bay.passenger", {f.home, orbital_node_kind::low_orbit},
                             space_unit_state::idle);
    f.carrier = stage_unit(w, f.a, space_hull::lander, "bay.cargo", {f.moon, orbital_node_kind::surface},
                           space_unit_state::landed);
    space_unit& cu = w.space_units.at(f.carrier);
    cu.cargo = 20.0f;
    space_haul h;
    h.owner = f.a;
    h.phase = space_haul_phase::held;
    h.cargo_res = resource_type::machinery;
    h.src_market = f.home_mkt;
    h.target = cu.node;
    h.home = cu.node;
    w.space_hauls.emplace(f.carrier, h);
}

entity_id mt(const fixture& f, int x, int y) { return f.moon_tile[y * GW + x]; }

/// Board four, land them, map the moon, found the base at (4, 3). The base's market.
entity_id found_base(fixture& f, const recipe_registry& reg)
{
    space_command c;
    c.nation = f.a;
    c.verb = static_cast<uint8_t>(space_verb::embark_colonists);
    c.subject = f.passenger;
    c.slot = 4;
    apply_space_command(f.w, reg, c);
    space_unit& u = f.w.space_units.at(f.passenger);
    u.state = space_unit_state::landed;
    u.node = u.dest = {f.moon, orbital_node_kind::surface};
    survey_map_regions(f.w, f.a, f.moon, 99);
    space_command fb;
    fb.nation = f.a;
    fb.verb = static_cast<uint8_t>(space_verb::found_base);
    fb.subject = mt(f, 4, 3);
    const space_command_outcome o = apply_space_command(f.w, reg, fb);
    if (o.result != space_command_result::applied)
        return null_entity;
    const entity_id m = market_for_tile(f.w, mt(f, 4, 3));
    if (m != null_entity)
        stock(f.w.markets.at(m), 0.0f);
    return m;
}

/// A finished, staffed building, its workforce pinned.
void finish(world& w, entity_id b)
{
    building_component& bc = w.buildings.at(b);
    bc.ticks_remaining = 0;
    bc.days_remaining = 0;
    bc.workforce_assigned = 0.5f;
    bc.workforce_auto = false;
    bc.workforce_target = 100;
}

/// One tick: production, then the clear that moves landings onto the shelves.
economy_report tick(world& w, const recipe_registry& reg, int t)
{
    w.current_econ_tick = t;
    w.current_day_tick = t;
    const economy_report rep = run_economy_step(w, reg);
    (void)clear_markets(w, reg, rep);
    return rep;
}

float output_of(const economy_report& rep, entity_id b)
{
    for (const building_report& br : rep.buildings)
        if (br.building == b)
            return br.output_quantity;
    return 0.0f;
}

/// The moon programme through E2: the base, the extractor and the plant, six ticks.
struct loop_state
{
    entity_id moon_mkt = null_entity, ice = null_entity, plant = null_entity;
    float water_out[k_ticks] = {}, prop_out[k_ticks] = {};
};

loop_state run_loop(fixture& f, const regs& r)
{
    loop_state s;
    s.moon_mkt = found_base(f, r.reg);
    const construction_result ci = construct_nation(f.w, r.reg, f.a, mt(f, 5, 3), building_type::extraction_site,
                                                    resource_type::water, no_recipe, s.ice);
    const construction_result cp = construct_nation(f.w, r.reg, f.a, mt(f, 3, 3), building_type::processing_facility,
                                                    resource_type::propellant, r.elec, s.plant);
    if (g_diag)
        std::printf("    diag build ice %d plant %d market %u\n", static_cast<int>(ci), static_cast<int>(cp), s.moon_mkt);
    // The base's Logistic Points: a hub on its ground (LOGISTICS.md § Logistic
    // Points - production consumes them, and only a building makes them).
    entity_id hub = null_entity;
    const construction_result ch = construct_nation(f.w, r.reg, f.a, mt(f, 4, 4), building_type::inland_logistics_hub,
                                                    resource_type::iron_ore, no_recipe, hub);
    if (hub != null_entity)
    {
        building_component& hb = f.w.buildings.at(hub);
        hb.ticks_remaining = 0;
        hb.days_remaining = 0;
    }
    if (g_diag)
        std::printf("    diag hub %d\n", static_cast<int>(ch));
    if (s.ice != null_entity) finish(f.w, s.ice);
    if (s.plant != null_entity) finish(f.w, s.plant);
    for (int t = 1; t <= k_ticks; ++t)
    {
        if (s.moon_mkt != null_entity)
            f.w.markets.at(s.moon_mkt).inventory[RF] = 100.0f; // the shipped half of the loop
        const economy_report rep = tick(f.w, r.reg, t);
        s.water_out[t - 1] = output_of(rep, s.ice);
        s.prop_out[t - 1]  = output_of(rep, s.plant);
        if (g_diag && s.moon_mkt != null_entity)
        {
            const market_component& mm = f.w.markets.at(s.moon_mkt);
            std::printf("    diag t%d shelf water %.3f price %.3f base %.3f | fuel %.3f price %.3f | prop %.3f\n", t,
                        mm.inventory[WA], mm.price[WA], mm.base_price[WA], mm.inventory[RF], mm.price[RF], mm.inventory[PR]);
        }
        if (g_diag)
            for (const building_report& br : rep.buildings)
                if (br.building == s.ice || br.building == s.plant)
                {
                    const building_component& b = f.w.buildings.at(br.building);
                    std::printf("    diag t%d b%u type %d active %d idle %d exh %d out %.3f effw %.3f | ticks %d days %d "
                                "sidle %d decom %d wf %.2f arrears %d\n",
                                t, br.building, static_cast<int>(br.type), br.active, br.idle, br.exhausted,
                                br.output_quantity, br.effective_workforce, b.ticks_remaining, b.days_remaining,
                                b.state_idle, b.decommissioned, b.workforce_assigned, b.build_arrears_milli);
                }
        if (g_diag && t >= 4)
        {
            for (const auto& [cid, pc] : f.w.population_centres)
                std::printf("    diag centre %u pop %d scale %d razed %d\n", cid, pc.population, pc.scale, pc.razed);
            for (const auto& [k, v] : rep.workforce_contention)
                std::printf("    diag contention owner %u body %u -> %.3f\n", k.first, k.second, v);
            for (const auto& [k, v] : rep.building_labour)
                std::printf("    diag labour b%u -> %.3f share %d\n", k, v,
                            f.w.buildings.count(k) ? f.w.buildings.at(k).output_share_permille : -1);
            for (const auto& [k, v] : rep.logistic_points.fraction)
                std::printf("    diag lp b%u -> %.3f\n", k, v);
            const auto lit = f.w.buildings.find(s.ice);
            std::printf("    diag assets %zu moon %u owner-tile %u\n", f.w.nations.at(f.a).state_assets.size(), f.moon,
                        lit != f.w.buildings.end() ? f.w.tile_controller(lit->second.tile) : 0u);
        }
        if (g_diag && t == 1)
            std::printf("    diag ice %u plant %u reports %zu market %u\n", s.ice, s.plant, rep.buildings.size(), s.moon_mkt);
    }
    return s;
}

int slot_of(int c, int r) { return r * station_grid_width(0) + c; }

/// The station's working Fuel depots, read off its pure budget.
int working_depots(const world& w, const station_params& stp, entity_id station)
{
    const station_budget b = evaluate_station_budget(w, stp, station, w.current_econ_tick);
    int n = 0;
    for (const entity_id e : b.working)
        if (const auto it = w.space_assets.find(e);
            it != w.space_assets.end() && it->second.module == static_cast<uint8_t>(station_module_kind::fuel_depot))
            ++n;
    return n;
}

/// A station in @p node with power, habitation, life support, a Dock and a Fuel
/// depot, walked until its modules arrive.
entity_id build_station(world& w, entity_id nation, orbital_node node)
{
    const space_asset_params sp{};
    const station_params stp{};
    const station_op s = found_station(w, sp, stp, nation, node);
    if (s.result != station_result::ok)
        return null_entity;
    fit_station_module(w, sp, stp, nation, s.station, slot_of(0, 0), station_module_kind::dock);
    fit_station_module(w, sp, stp, nation, s.station, slot_of(3, 0), station_module_kind::power);
    fit_station_module(w, sp, stp, nation, s.station, slot_of(4, 0), station_module_kind::habitation);
    fit_station_module(w, sp, stp, nation, s.station, slot_of(0, 2), station_module_kind::life_support);
    fit_station_module(w, sp, stp, nation, s.station, slot_of(1, 2), station_module_kind::fuel_depot);
    for (int t = w.current_econ_tick; t < w.current_econ_tick + 8; ++t)
    {
        run_space_asset_step(w, sp, t);
        run_station_step(w, stp, t);
    }
    return s.station;
}

void rows_ice()
{
    std::printf("\nI the Ice Extractor\n");
    const regs r = make_reg();
    fixture f; build(f);
    found_base(f, r.reg);
    set_key(f.w, f.a, space_keys::ice_extractor, false);
    const std::string b0 = to_bytes(f.w);
    const std::uint64_t h0 = f.w.state_hash(0);
    entity_id out = null_entity;
    const construction_result c0 = construct_nation(f.w, r.reg, f.a, mt(f, 5, 3), building_type::extraction_site,
                                                    resource_type::water, no_recipe, out);
    check(c0 == construction_result::tech_locked && out == null_entity && to_bytes(f.w) == b0 && f.w.state_hash(0) == h0,
          "I1", fmt("on base ground without Ice Extraction: refused %d (tech_locked %d); nothing written",
                    static_cast<int>(c0), static_cast<int>(construction_result::tech_locked)));
    entity_id home_ice = null_entity;
    const construction_result c1 = construct_nation(f.w, r.reg, f.a, f.home_ice, building_type::extraction_site,
                                                    resource_type::water, no_recipe, home_ice);
    check(c1 == construction_result::placed && home_ice != null_entity, "I2",
          fmt("on the home body's ice, the same nation without the key: placed (%d)", static_cast<int>(c1)));

    fixture g; build(g);
    const loop_state s = run_loop(g, r);
    bool every = true;
    for (const float q : s.water_out) every = every && q > 0.0f;
    const float shelf = s.moon_mkt != null_entity ? g.w.markets.at(s.moon_mkt).inventory[WA] : -1.0f;
    check(s.ice != null_entity && every && g.w.buildings.at(s.ice).tile == mt(g, 5, 3)
              && market_for_tile(g.w, mt(g, 5, 3)) == s.moon_mkt,
          "I3", fmt("with the key: placed on the ice; water every tick %.3f %.3f %.3f %.3f, landing on the moon's own "
                    "market (shelf now %.3f, net of the plant's draw)",
                    s.water_out[0], s.water_out[1], s.water_out[2], s.water_out[3], shelf));
}

void rows_electrolysis()
{
    std::printf("\nE the Electrolysis Plant\n");
    const regs r = make_reg();
    {
        fixture f; build(f);
        found_base(f, r.reg);
        set_key(f.w, f.a, space_keys::electrolysis_plant, false);
        const std::string b0 = to_bytes(f.w);
        const std::uint64_t h0 = f.w.state_hash(0);
        entity_id o1 = null_entity, o2 = null_entity;
        const construction_result c0 = construct_nation(f.w, r.reg, f.a, mt(f, 3, 3), building_type::processing_facility,
                                                        resource_type::propellant, r.elec, o1);
        const construction_result c1 = construct_nation(f.w, r.reg, f.a, mt(f, 3, 3), building_type::processing_facility,
                                                        resource_type::propellant, r.atmo, o2);
        const bool none = to_bytes(f.w) == b0 && f.w.state_hash(0) == h0;
        // The retool door: a plant on the airless route, its key then withdrawn.
        set_key(f.w, f.a, space_keys::electrolysis_plant, true);
        entity_id p = null_entity;
        construct_nation(f.w, r.reg, f.a, mt(f, 3, 3), building_type::processing_facility, resource_type::propellant,
                         r.elec, p);
        set_key(f.w, f.a, space_keys::electrolysis_plant, false);
        recipe_switch_result sw = recipe_switch_result::applied;
        if (p != null_entity)
        {
            building_component& b = f.w.buildings.at(p);
            b.recipe = r.atmo; // stage it on the sibling route, then ask to switch back
            sw = try_switch_recipe(f.w, r.reg, f.a, b, r.elec);
        }
        check(c0 == construction_result::tech_locked && c1 == construction_result::wrong_air && o1 == null_entity
                  && o2 == null_entity && none && sw == recipe_switch_result::tech_locked,
              "E1", fmt("without Electrolysis Plants: the airless route refused %d (tech_locked), the atmospheric %d "
                        "(wrong_air), nothing written; the retool refused %d (tech_locked)",
                        static_cast<int>(c0), static_cast<int>(c1), static_cast<int>(sw)));
    }
    fixture g; build(g);
    const loop_state s = run_loop(g, r);
    // Tick 1 the plant finds no water: the extractor's first landing reaches the shelf
    // at that tick's clear. From tick 2 it runs every tick.
    float made = 0.0f;
    bool every = s.prop_out[0] == 0.0f;
    for (int t = 1; t < k_ticks; ++t) every = every && s.prop_out[t] > 0.0f;
    for (const float q : s.prop_out) made += q;
    const float shelf = s.moon_mkt != null_entity ? g.w.markets.at(s.moon_mkt).inventory[PR] : -1.0f;
    const float home = g.w.markets.at(g.home_mkt).inventory[PR];
    check(s.plant != null_entity && every && shelf > 0.0f && market_for_tile(g.w, mt(g, 3, 3)) == s.moon_mkt,
          "E2", fmt("with the key: placed; propellant %.3f (no water yet) then %.3f %.3f %.3f (%.3f made), on the "
                    "moon's shelf %.3f (home shelf %.1f)",
                    s.prop_out[0], s.prop_out[1], s.prop_out[2], s.prop_out[3], made, shelf, home));
}

void rows_refuel()
{
    std::printf("\nR refuelling over the moon\n");
    const regs r = make_reg();
    fixture f; build(f);
    const loop_state s = run_loop(f, r);
    add_pad(f.w, f.a, mt(f, 4, 3)); // a staffed pad on base ground
    const entity_id h = stage_unit(f.w, f.a, space_hull::hauler, "bay.cargo", {f.moon, orbital_node_kind::low_orbit},
                                   space_unit_state::idle);
    // A top-up the moon's own production can fill (refuelling is refused whole on a
    // short shelf): the tank 4 short.
    {
        space_unit& u = f.w.space_units.at(h);
        u.propellant = std::max(0.0f, evaluate_space_design(u.hull, u.design).tank - 4.0f);
    }
    const float moon0 = f.w.markets.at(s.moon_mkt).inventory[PR];
    const float home0 = f.w.markets.at(f.home_mkt).inventory[PR];
    const space_unit_params p{};
    const space_unit_op op = refuel_space_unit(f.w, p, station_params{}, f.a, h);
    const float moon1 = f.w.markets.at(s.moon_mkt).inventory[PR];
    const float home1 = f.w.markets.at(f.home_mkt).inventory[PR];
    check(op.result == space_unit_result::ok && op.market == s.moon_mkt && op.propellant > 0.0f
              && std::fabs((moon0 - moon1) - op.propellant) < 1e-3f && home1 == home0,
          "R1", fmt("refuel %d: %.3f bought off the moon's shelf (%.3f -> %.3f), %.2f paid; home shelf %.1f -> %.1f",
                    static_cast<int>(op.result), op.propellant, moon0, moon1, op.credits, home0, home1));
}

void rows_depot()
{
    std::printf("\nD the Fuel depot\n");
    const regs r = make_reg();
    const space_unit_params p{};
    const station_params stp{};

    fixture f; build(f);
    const loop_state s = run_loop(f, r);
    f.w.markets.at(s.moon_mkt).inventory[PR] = 1000.0f; // enough to fill the cap
    const entity_id st = build_station(f.w, f.a, {f.moon, orbital_node_kind::low_orbit});
    const int depots = st != null_entity ? working_depots(f.w, stp, st) : 0;
    const float moon0 = f.w.markets.at(s.moon_mkt).inventory[PR];
    const float home0 = f.w.markets.at(f.home_mkt).inventory[PR];
    space_unit_quarter_report qr;
    run_space_unit_quarter(f.w, p, stp, f.w.current_econ_tick + 1, &qr);
    const float stock = f.w.station_depot_stock.count(st) ? f.w.station_depot_stock.at(st) : 0.0f;
    const float moon1 = f.w.markets.at(s.moon_mkt).inventory[PR];
    const float home1 = f.w.markets.at(f.home_mkt).inventory[PR];
    check(st != null_entity && depots == 1 && stock > 0.0f && std::fabs((moon0 - moon1) - stock) < 1e-2f && home1 == home0,
          "D1", fmt("a station over the base (working depots %d) stocked %.1f off the moon's shelf (%.1f -> %.1f); home "
                    "shelf %.1f -> %.1f",
                    depots, stock, moon0, moon1, home0, home1));

    // D2: the same, with the base record gone - no base below, the home shelf.
    fixture g; build(g);
    const loop_state t = run_loop(g, r);
    g.w.markets.at(t.moon_mkt).inventory[PR] = 1000.0f;
    g.w.offworld_bases.clear();
    const entity_id st2 = build_station(g.w, g.a, {g.moon, orbital_node_kind::low_orbit});
    const float m0 = g.w.markets.at(t.moon_mkt).inventory[PR];
    const float h0 = g.w.markets.at(g.home_mkt).inventory[PR];
    run_space_unit_quarter(g.w, p, stp, g.w.current_econ_tick + 1, &qr);
    const float stock2 = g.w.station_depot_stock.count(st2) ? g.w.station_depot_stock.at(st2) : 0.0f;
    const float m1 = g.w.markets.at(t.moon_mkt).inventory[PR];
    const float h1 = g.w.markets.at(g.home_mkt).inventory[PR];
    check(st2 != null_entity && stock2 > 0.0f && m1 == m0 && std::fabs((h0 - h1) - stock2) < 1e-2f, "D2",
          fmt("no base below: stocked %.1f off the home shelf (%.1f -> %.1f); the moon's untouched (%.1f -> %.1f)",
              stock2, h0, h1, m0, m1));
}

void rows_determinism()
{
    std::printf("\nX determinism\n");
    const regs r = make_reg();
    fixture f; build(f);
    fixture g; build(g);
    run_loop(f, r);
    run_loop(g, r);
    check(f.w.state_hash(0) == g.w.state_hash(0) && to_bytes(f.w) == to_bytes(g.w), "X1",
          fmt("two identical programmes: hash %016llx both, streams equal",
              static_cast<unsigned long long>(f.w.state_hash(0))));
}

} // namespace

int main()
{
    std::printf("propellant_loop_harness — BL-1604 (propellant loop)\n");
    rows_ice();
    rows_electrolysis();
    rows_refuel();
    rows_depot();
    rows_determinism();
    std::printf("\n=== %s (%d pass, %d fail) ===\n", g_fail == 0 ? "ALL PASS" : "FAILURES", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
