// ---------------------------------------------------------------------------
// space_battle_harness — BL-1597 (space battles), the proof
// ---------------------------------------------------------------------------
// docs/military/SPACE_FLEETS.md § Battles. The space_units_harness fixture: a star,
// a home planet, its moon, a far planet; one market each on home and moon; nations
// A and B both holding every far-space key and the near-space keys, each with a
// staffed home Launchpad (the launch gate). Every unit is RAISED by the verbs; every
// station FOUNDED and fitted by the verbs. Rows:
//
//   S1  NO HOSTILITY: an A frigate and a B frigate share home low orbit; no stance
//       row -> no battle, no damage, the world untouched by the day.
//   S2  UNARMED: A hostile, A's frigate and B's hauler -> no battle (B is not armed).
//   S3  OPENS: B raises a frigate -> one battle opens that day between A and B at the
//       node, fought once; the hostility is one-directional (A -> B) and still opens it.
//   S4  THE EXCHANGE: two A frigates (pool 20) against B's frigate (armour 8, hp 50)
//       and hauler: 12 a day lands on the frigate, B's 10 lands 2 on A's lower-id
//       frigate — exact, per day, from the catalogue.
//   S5  ARMOUR: the same fire against a composite-armoured B frigate (armour 15)
//       lands 5 a day; the difference is the armour difference.
//   S6  ORDER AND LOSS: the frigate (escort) dies first; the day's leftover fire walks
//       on to the unarmed hauler; units and crew lost are tallied, mass destroyed is
//       dry mass + cargo; B is out (destroyed), the battle decided, the report kept.
//   S7  DEFENCE: B's station with one Defence module and no unit: the module makes B
//       armed (a battle opens), fires 12 a day (4 lands through a whipple frigate),
//       takes damage at defence_armour, is lost at 0; then the station falls (its
//       core lost); modules, stations, crew and mass tallied.
//   S8  WITHDRAWAL: an engaged B frigate ordered away is RECORDED (withdrawing, still
//       at the node, propellant untouched), takes the next day's fire, then leaves in
//       transit paying exactly the move's burn; the battle closes, B withdrew. An
//       engaged move that cannot be paid is refused whole and records nothing.
//   S9  PEACE: return to neutral mid-battle -> the next day closes it as peace with
//       no fire; a battle-free node never opens.
//   S10 SAVE: round-trip mid-battle (a withdrawal recorded, module damage held):
//       equal hash, byte-identical; the v84 prefix + the section == the stream; both
//       worlds fight the next day identically. A battle-free world's v84 prefix reads.
//   S11 TAMPERED: bad state / end / node kind, a ghost nation, sides out of order, a
//       NaN mass, an id at next id, a withdrawal to its own node, a negative module
//       damage, a decided battle with both holding, truncation — each refused whole.
//   S12 DETERMINISM: two fixtures driven identically end with equal hash and stream.
//
// Build: node tools/verify/build_harness.js space_battle_harness --run
// ---------------------------------------------------------------------------

#include "world/digitisation_tree_data.hpp"
#include "world/early_space_tree_data.hpp"
#include "world/nation_tech.hpp"
#include "world/orbital_graph.hpp"
#include "world/space_assets.hpp"
#include "world/space_battle.hpp"
#include "world/space_tech_keys.hpp"
#include "world/space_units.hpp"
#include "world/stance.hpp"
#include "world/stations.hpp"
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

void check(bool ok, const char* row, const std::string& what)
{
    std::printf("  [%s] %s %s\n", ok ? "PASS" : "FAIL", row, what.c_str());
    if (ok) ++g_pass; else ++g_fail;
}

std::string fmt(const char* f, ...)
{
    char buf[900];
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

constexpr std::size_t PR = static_cast<std::size_t>(resource_type::propellant);
constexpr std::size_t SC = static_cast<std::size_t>(resource_type::spacecraft_components);

struct fixture
{
    world     w;
    entity_id star = null_entity, home = null_entity, moon = null_entity, far = null_entity;
    entity_id a = null_entity, b = null_entity;
    entity_id home_tile = null_entity, moon_tile = null_entity;
    entity_id home_mkt = null_entity, moon_mkt = null_entity;
    orbital_node low{}, high{};
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
        m.inventory[r] = 20000.0f;
    }
    m.base_price[PR] = 64.0f;
    m.base_price[SC] = 310.0f;
    w.markets.emplace(id, m);
    return id;
}

entity_id add_pad(world& w, entity_id nation, entity_id tile)
{
    const entity_id bid = w.create_entity();
    building_component b{};
    b.tile = tile;
    b.type = building_type::launchpad;
    b.workforce_assigned = 1.0f;
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
    // The Digitisation tree's near-space keys (the station launch gate), as the
    // space_units_harness stages them.
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

// Function-local statics: the catalogue is another TU's static (init order).
const space_design& hauler_d()
{
    static const space_design d = design_of({{sk::drive, "drive.chemical"}, {sk::tanks, "tanks.extended"}, {sk::bay, "bay.cargo"}});
    return d;
}
const space_design& frigate_d()
{
    static const space_design d = design_of({{sk::drive, "drive.nuclear_thermal"}, {sk::tanks, "tanks.extended"},
                                             {sk::armament, "armament.kinetic"}, {sk::armour, "armour.whipple"}});
    return d;
}
const space_design& frigate_heavy_d()
{
    static const space_design d = design_of({{sk::drive, "drive.nuclear_thermal"}, {sk::tanks, "tanks.extended"},
                                             {sk::armament, "armament.kinetic"}, {sk::armour, "armour.composite"}});
    return d;
}
#define k_hauler        hauler_d()
#define k_frigate       frigate_d()
#define k_frigate_heavy frigate_heavy_d()

void build(fixture& f, bool heavy_b = false)
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
    f.home_mkt = add_market(w, f.home, f.home_tile);
    f.moon_mkt = add_market(w, f.moon, f.moon_tile);
    for (const entity_id n : {f.a, f.b})
    {
        nation_component& nc = w.nations.at(n);
        nc.capital_tile = f.home_tile;
        nc.treasury = 50000000.0f;
        nc.astronauts = 80;
        grant_keys(w, n);
        add_pad(w, n, f.home_tile);
        set_space_unit_design(w, n, space_hull::hauler, k_hauler);
    }
    set_space_unit_design(w, f.a, space_hull::frigate, k_frigate);
    set_space_unit_design(w, f.b, space_hull::frigate, heavy_b ? k_frigate_heavy : k_frigate);
    f.low  = {f.home, orbital_node_kind::low_orbit};
    f.high = {f.home, orbital_node_kind::high_orbit};
    w.current_econ_tick = 1;
}

entity_id raise(fixture& f, entity_id n, space_hull h)
{
    return raise_space_unit_at_pad(f.w, space_unit_params{}, n, h, f.home).unit;
}

int slot_of(int c, int r) { return r * station_grid_width(0) + c; }

/// A home-low-orbit station for @p nation: power, habitation, life support and
/// @p defences Defence modules, all arrived and working.
entity_id defended_station(fixture& f, entity_id nation, int defences)
{
    world& w = f.w;
    const space_asset_params sp{};
    const station_params stp{};
    const station_op s = found_station(w, sp, stp, nation, f.low);
    fit_station_module(w, sp, stp, nation, s.station, slot_of(3, 0), station_module_kind::power);
    fit_station_module(w, sp, stp, nation, s.station, slot_of(4, 0), station_module_kind::habitation);
    fit_station_module(w, sp, stp, nation, s.station, slot_of(0, 2), station_module_kind::life_support);
    fit_station_module(w, sp, stp, nation, s.station, slot_of(4, 2), station_module_kind::power);
    for (int i = 0; i < defences; ++i)
        fit_station_module(w, sp, stp, nation, s.station, slot_of(1 + i, 2), station_module_kind::defence);
    run_space_asset_step(w, sp, 1);
    run_station_step(w, stp, 1);
    return s.station;
}

space_battle_day_report day(fixture& f, int n = 1)
{
    const space_unit_params p{};
    const station_params stp{};
    const space_battle_params bp{};
    space_battle_day_report last;
    for (int i = 0; i < n; ++i)
    {
        run_space_unit_day(f.w, p, stp);
        run_space_battle_day(f.w, p, stp, bp, &last);
    }
    return last;
}

float hp(const fixture& f, entity_id u)
{
    const auto it = f.w.space_units.find(u);
    return it == f.w.space_units.end() ? -1.0f : it->second.hull_points;
}

const space_battle* only_battle(const fixture& f)
{
    return f.w.space_battles.size() == 1 ? &f.w.space_battles.begin()->second : nullptr;
}

/// The S12 script, run twice.
void programme(fixture& f)
{
    const entity_id a1 = raise(f, f.a, space_hull::frigate);
    raise(f, f.a, space_hull::frigate);
    const entity_id b1 = raise(f, f.b, space_hull::frigate);
    raise(f, f.b, space_hull::hauler);
    defended_station(f, f.b, 1);
    declare_hostile(f.w, f.a, f.b);
    day(f, 2);
    order_space_unit(f.w, space_unit_params{}, f.b, b1, f.high);
    order_space_unit(f.w, space_unit_params{}, f.a, a1, f.high);
    day(f, 6);
}

} // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const space_unit_params p{};
    const station_params    stp{};
    const space_battle_params bp{};
    std::printf("space_battle_harness — BL-1597\n");
    std::printf("dials: defence fire %.1f armour %.1f hp %.1f mass %.1f; core mass %.1f; report history %d\n",
                bp.defence_firepower, bp.defence_armour, bp.defence_hull_points, bp.defence_module_mass,
                bp.station_core_mass, bp.report_history);
    {
        const space_design_stats fr = evaluate_space_design(space_hull::frigate, k_frigate);
        const space_design_stats fh = evaluate_space_design(space_hull::frigate, k_frigate_heavy);
        const space_design_stats ha = evaluate_space_design(space_hull::hauler, k_hauler);
        std::printf("frigate: fp %.0f armour %.0f hp %.0f dry %.0f crew %d | heavy: armour %.0f hp %.0f | "
                    "hauler: hp %.0f dry %.0f crew %d\n",
                    fr.firepower, fr.armour, fr.hull_points, fr.dry_mass, fr.crew, fh.armour, fh.hull_points,
                    ha.hull_points, ha.dry_mass, ha.crew);
    }

    // ---- S1 no hostility ----------------------------------------------------
    std::printf("--- S1 no hostility ---\n");
    {
        fixture f;
        build(f);
        const entity_id a1 = raise(f, f.a, space_hull::frigate);
        const entity_id b1 = raise(f, f.b, space_hull::frigate);
        const uint64_t h0 = f.w.state_hash(0);
        const space_battle_day_report r = day(f, 3);
        check(a1 != null_entity && b1 != null_entity && f.w.space_battles.empty() && r.opened == 0
                  && hp(f, a1) == 50.0f && hp(f, b1) == 50.0f && f.w.state_hash(0) == h0 && f.w.next_space_battle_id == 0,
              "S1", "two armed nations at one node, neither hostile: no battle, no damage, the hash unmoved");
    }

    // ---- S2 unarmed ---------------------------------------------------------
    std::printf("--- S2 unarmed ---\n");
    {
        fixture f;
        build(f);
        const entity_id a1 = raise(f, f.a, space_hull::frigate);
        const entity_id bh = raise(f, f.b, space_hull::hauler);
        declare_hostile(f.w, f.a, f.b);
        const space_battle_day_report r = day(f, 3);
        check(bh != null_entity && f.w.space_battles.empty() && r.opened == 0 && hp(f, bh) == 20.0f && hp(f, a1) == 50.0f,
              "S2", "A hostile, B present with a hauler only: B is not armed, no battle");
    }

    // ---- S3..S6 the exchange ------------------------------------------------
    std::printf("--- S3-S6 the exchange ---\n");
    float landed_whipple = 0.0f;
    {
        fixture f;
        build(f);
        const entity_id a1 = raise(f, f.a, space_hull::frigate);
        const entity_id a2 = raise(f, f.a, space_hull::frigate);
        const entity_id bh = raise(f, f.b, space_hull::hauler);
        const entity_id b1 = raise(f, f.b, space_hull::frigate);
        declare_hostile(f.w, f.a, f.b); // one direction only
        const space_battle_day_report r1 = day(f);
        const space_battle* b = only_battle(f);
        check(b != nullptr && r1.opened == 1 && r1.fought == 1 && b->state == space_battle_state::open
                  && b->node == f.low && b->sides[0].nation == f.a && b->sides[1].nation == f.b && b->days == 1,
              "S3", fmt("one battle opened at home low orbit, A vs B, fought day 1 (opened %d, fought %d)",
                        r1.opened, r1.fought));
        landed_whipple = 50.0f - hp(f, b1);
        check(hp(f, b1) == 38.0f && hp(f, bh) == 20.0f && hp(f, a1) == 48.0f && hp(f, a2) == 50.0f
                  && b->sides[0].damage_landed == 12.0f && b->sides[1].damage_landed == 2.0f,
              "S4", fmt("A's pool 20 - armour 8 lands 12 on B's frigate (hp %.0f); B's 10 - 8 lands 2 on A's "
                        "lower-id frigate (hp %.0f / %.0f); the hauler untouched (%.0f)",
                        hp(f, b1), hp(f, a1), hp(f, a2), hp(f, bh)));

        // Day 2..4: 12 a day -> 2 hp left; day 5 kills it and 10 spills onto the hauler.
        day(f, 3);
        const bool before = hp(f, b1) == 2.0f && hp(f, bh) == 20.0f && hp(f, a1) == 42.0f;
        const int crew_b1 = f.w.space_units.at(b1).crew;
        const space_battle_day_report r5 = day(f);
        b = only_battle(f);
        const space_design_stats fs = evaluate_space_design(space_hull::frigate, k_frigate);
        check(before && hp(f, b1) < 0.0f && hp(f, bh) == 10.0f && b != nullptr
                  && b->state == space_battle_state::decided && b->sides[1].end == space_battle_end::destroyed
                  && b->sides[0].end == space_battle_end::holding && b->sides[1].units_lost == 1
                  && b->sides[1].crew_lost == crew_b1 && b->mass_destroyed == fs.dry_mass && b->days == 5
                  && r5.units_lost == 1 && r5.mass_destroyed.size() == 1 && r5.mass_destroyed[0].first == f.low
                  && r5.mass_destroyed[0].second == fs.dry_mass && hp(f, a1) == 40.0f,
              "S6", fmt("day 5: the escort dies first (crew %d lost), 20-8-2 = 10 spills onto the hauler (hp %.0f); "
                        "mass destroyed %.0f (the debris seam); B destroyed, battle decided after %d days",
                        crew_b1, hp(f, bh), b ? b->mass_destroyed : -1.0f, b ? b->days : -1));
        const space_battle_day_report r6 = day(f, 2);
        check(f.w.space_battles.size() == 1 && r6.opened == 0 && hp(f, bh) == 10.0f,
              "S6b", "the report stays; an unarmed survivor beside a hostile fleet opens nothing");
    }
    {
        fixture f;
        build(f, /*heavy_b=*/true);
        raise(f, f.a, space_hull::frigate);
        raise(f, f.a, space_hull::frigate);
        const entity_id b1 = raise(f, f.b, space_hull::frigate);
        declare_hostile(f.w, f.a, f.b);
        day(f);
        const float landed = 60.0f - hp(f, b1);
        check(landed == 5.0f && landed_whipple == 12.0f && landed_whipple - landed == 15.0f - 8.0f,
              "S5", fmt("composite armour 15 lets 5 of 20 land (whipple 8 let %.0f): the difference is the armour's",
                        landed_whipple));
    }

    // ---- S7 defence ---------------------------------------------------------
    std::printf("--- S7 defence ---\n");
    {
        fixture f;
        build(f);
        const entity_id st = defended_station(f, f.b, 1);
        const station_budget sb = evaluate_station_budget(f.w, stp, st, f.w.current_econ_tick);
        entity_id mod = null_entity;
        for (const entity_id e : sb.working)
            if (f.w.space_assets.at(e).module == static_cast<uint8_t>(station_module_kind::defence))
                mod = e;
        const entity_id a1 = raise(f, f.a, space_hull::frigate);
        const entity_id a2 = raise(f, f.a, space_hull::frigate);
        declare_hostile(f.w, f.b, f.a); // the station's owner is the hostile one
        check(mod != null_entity && space_armed_at(f.w, stp, f.b, f.low), "S7a",
              fmt("a station with one working Defence module makes B armed at its node (%zu working modules)",
                  sb.working.size()));
        day(f);
        const space_battle* b = only_battle(f);
        const bool d1 = b != nullptr && b->state == space_battle_state::open && hp(f, a1) == 46.0f
                     && hp(f, a2) == 50.0f && b->module_damage.size() == 1 && b->module_damage[0].first == mod
                     && b->module_damage[0].second == 14.0f;
        check(d1, "S7b", fmt("day 1: the module's 12 - 8 lands 4 on A's frigate (hp %.0f); A's 20 - 6 lands 14 on the "
                             "module", hp(f, a1)));
        int crew_station = 0;
        for (const entity_id e : f.w.stations.at(st).slots)
            if (e != null_entity)
                crew_station += f.w.space_assets.at(e).crew;
        const space_battle_day_report r2 = day(f);
        const space_battle_day_report r3 = day(f);
        b = only_battle(f);
        check(b != nullptr && r2.modules_lost == 0 && r3.modules_lost == 1 && r3.stations_lost == 1
                  && f.w.space_assets.at(mod).state == space_asset_state::lost
                  && f.w.space_assets.at(st).state == space_asset_state::lost
                  && b->state == space_battle_state::decided && b->sides[1].end == space_battle_end::destroyed
                  && b->sides[1].modules_lost == 1 && b->sides[1].stations_lost == 1
                  && b->sides[1].crew_lost == crew_station
                  && b->mass_destroyed == bp.defence_module_mass + bp.station_core_mass
                  && hp(f, a1) == 38.0f && b->module_damage.empty(),
              "S7c", fmt("day 3: the module is lost (14 + 14 + 12 = 40), then the station falls: core lost, %d crew "
                         "aboard lost, mass %.0f; A's frigate took 4 a day (hp %.0f)",
                         crew_station, b ? b->mass_destroyed : -1.0f, hp(f, a1)));
        run_station_step(f.w, stp, 2);
        check(f.w.stations.count(st) == 0, "S7d", "the quarterly station step takes the fallen station");
    }

    // ---- S8 withdrawal ------------------------------------------------------
    std::printf("--- S8 withdrawal ---\n");
    {
        fixture f;
        build(f);
        raise(f, f.a, space_hull::frigate);
        raise(f, f.a, space_hull::frigate);
        const entity_id b1 = raise(f, f.b, space_hull::frigate);
        declare_hostile(f.w, f.a, f.b);
        day(f);
        const float prop0 = f.w.space_units.at(b1).propellant;
        // Refused whole: an engaged unit ordered where its budget cannot reach.
        // STAGED: a nearly dry tank, so the move cannot be paid; restored after.
        f.w.space_units.at(b1).propellant = 0.01f;
        const uint64_t h0 = f.w.state_hash(0);
        const space_unit_op bad = order_space_unit(f.w, p, f.b, b1, f.high);
        const bool refused = bad.result == space_unit_result::rejected_insufficient_dv && f.w.state_hash(0) == h0
                          && only_battle(f)->withdrawing.empty();
        f.w.space_units.at(b1).propellant = prop0;
        const space_unit_op op = order_space_unit(f.w, p, f.b, b1, f.high);
        const space_unit& u = f.w.space_units.at(b1);
        const float burn = space_unit_burn_for(u, op.dv);
        check(refused && op.result == space_unit_result::ok && op.withdrawing && u.state == space_unit_state::idle
                  && u.node == f.low && u.propellant == prop0 && only_battle(f)->withdrawing.size() == 1
                  && space_unit_engaged(f.w, b1),
              "S8a", fmt("the engaged move is recorded, not flown (withdrawing; still at the node; propellant %.2f "
                         "unchanged); with a staged dry tank the same move is refused whole (%d)",
                         u.propellant, (int)bad.result));
        const space_battle_day_report r = day(f);
        const space_battle* b = only_battle(f);
        const space_unit& v = f.w.space_units.at(b1);
        check(r.departed == 1 && v.state == space_unit_state::in_transit && v.dest == f.high && v.hull_points == 26.0f
                  && std::fabs(v.propellant - (prop0 - burn)) < 1e-4f && b->state == space_battle_state::decided
                  && b->sides[1].end == space_battle_end::withdrew && b->sides[1].units_lost == 0,
              "S8b", fmt("it took one more day of fire (hp 38 -> %.0f), then left for high orbit burning %.3f "
                         "(%.3f -> %.3f); B withdrew", v.hull_points, burn, prop0, v.propellant));
    }

    // ---- S9 peace -----------------------------------------------------------
    std::printf("--- S9 peace ---\n");
    {
        fixture f;
        build(f);
        const entity_id a1 = raise(f, f.a, space_hull::frigate);
        const entity_id b1 = raise(f, f.b, space_hull::frigate);
        declare_hostile(f.w, f.a, f.b);
        day(f);
        const float ha = hp(f, a1), hb = hp(f, b1);
        const stance_result sr = return_to_neutral(f.w, f.a, f.b);
        const space_battle_day_report r = day(f);
        const space_battle* b = only_battle(f);
        const space_battle_day_report r2 = day(f, 3);
        check(sr == stance_result::applied && b != nullptr && b->state == space_battle_state::peace && r.closed == 1
                  && r.fought == 0 && hp(f, a1) == ha && hp(f, b1) == hb && b->sides[0].end == space_battle_end::holding
                  && r2.opened == 0 && f.w.space_battles.size() == 1,
              "S9", fmt("return to neutral: the next day closes it as peace with no fire (hp %.0f / %.0f held); "
                        "nothing reopens", hp(f, a1), hp(f, b1)));
    }

    // ---- S10 save -----------------------------------------------------------
    std::printf("--- S10 save ---\n");
    std::string mid_bytes;
    fixture saved;
    {
        fixture& f = saved;
        build(f);
        defended_station(f, f.b, 1);
        const entity_id a1 = raise(f, f.a, space_hull::frigate);
        raise(f, f.a, space_hull::frigate);
        raise(f, f.b, space_hull::hauler);
        declare_hostile(f.w, f.a, f.b);
        day(f);
        order_space_unit(f.w, p, f.a, a1, f.high);
        const space_battle* b = only_battle(f);
        const bool staged = b != nullptr && b->withdrawing.size() == 1 && b->module_damage.empty()
                         && b->state == space_battle_state::open;
        // The hauler (unarmed, ranked before modules) soaked day 1's fire; day 2's
        // lands on the module, and the first withdrawal flies.
        day(f);
        // Re-stage a withdrawal after day 2 so the stream carries one.
        entity_id a_left = null_entity;
        for (const auto& [id, u] : f.w.space_units)
            if (u.owner == f.a && u.state != space_unit_state::in_transit)
                a_left = id;
        if (a_left != null_entity)
            order_space_unit(f.w, p, f.a, a_left, f.high);
        b = only_battle(f);
        mid_bytes = to_bytes(f.w);
        world w2;
        const bool ok = from_bytes(mid_bytes, w2);
        const std::string again = ok ? to_bytes(w2) : std::string();
        check(staged && b != nullptr && b->state == space_battle_state::open && b->withdrawing.size() == 1
                  && b->module_damage.size() == 1 && ok && again == mid_bytes && w2.state_hash(0) == f.w.state_hash(0)
                  && w2.space_battles == f.w.space_battles && w2.next_space_battle_id == f.w.next_space_battle_id,
              "S10a", fmt("mid-battle round-trip (day %d, %zu withdrawal, %zu damaged module): equal hash, byte-identical",
                          b ? b->days : -1, b ? b->withdrawing.size() : 0u, b ? b->module_damage.size() : 0u));
        std::ostringstream o84(std::ios::binary);
        write_world_snapshot_pre_space_battles(f.w, o84);
        const std::size_t sec = world_save_space_battle_section_bytes(f.w);
        check(o84.str().size() + sec + world_save_prospecting_section_bytes(f.w) + (world_save_space_convoy_section_bytes(f.w) + (world_save_debris_section_bytes(f.w) + world_save_space_bombard_section_bytes(f.w))) == mid_bytes.size() && sec == 8 + 80 + 9 + 8,
              "S10b", fmt("the section is %zu bytes (8 + 80 + 9 + 8); v84 prefix %zu + %zu == %zu", sec,
                          o84.str().size(), sec, mid_bytes.size()));
        // Both fight the next day identically.
        fixture g;
        g.w = w2;
        g.low = f.low;
        day(f);
        day(g);
        check(to_bytes(f.w) == to_bytes(g.w) && f.w.state_hash(0) == g.w.state_hash(0),
              "S10c", "the loaded world fights the next day exactly as the original");
        // A battle-free world's v84 prefix reads as a world with no battles.
        fixture e;
        build(e);
        std::ostringstream oe(std::ios::binary);
        write_world_snapshot_pre_space_battles(e.w, oe);
        world we;
        const bool oke = from_bytes(oe.str(), we);
        check(oke && we.space_battles.empty() && we.next_space_battle_id == 0 && we.state_hash(0) == e.w.state_hash(0)
                  && world_save_space_battle_section_bytes(e.w) == 8,
              "S10d", "a battle-free world's v84 prefix reads with no battles; equal hash; empty section 8 bytes");
    }

    // ---- S11 tampered -------------------------------------------------------
    std::printf("--- S11 tampered ---\n");
    {
        const std::string& b = mid_bytes;
        // `saved` has fought one more day since mid_bytes; the section's shape is
        // the one S10b measured: 8 + 80 + 9 + 8.
        const std::size_t s0 = b.size() - (8 + 80 + 9 + 8) - 4 - 4 - 12 - 4; // the v86 survey and v87 convoy sections follow, both empty here (4 bytes each), then the v88 debris section (BL-1598), empty: 12 bytes, then the v89 bombard section (BL-1599), empty: 4
        const std::size_t b0 = s0 + 8;
        const auto refused = [&](std::string bad) {
            world t;
            t.nations.emplace(999, nation_component{});
            return !from_bytes(bad, t) && t.nations.size() == 1 && t.nations.count(999) == 1;
        };
        const auto poke = [&](std::size_t at, const void* v, std::size_t n) {
            std::string bad = b;
            std::memcpy(&bad[at], v, n);
            return bad;
        };
        const uint8_t bad_state = 4 /* BL-1598: 3 is the standoff */, bad_end = 3, bad_kind = 9, decided = 1;
        const uint32_t ghost = 0x00FFFFFFu, zero = 0;
        const float nan = std::nanf(""), neg = -1.0f;
        uint32_t next_id = 0, a_id = 0, b_id = 0;
        std::memcpy(&next_id, &b[s0], 4);
        std::memcpy(&a_id, &b[b0 + 22], 4);
        std::memcpy(&b_id, &b[b0 + 47], 4);
        const uint32_t own_body = saved.home;
        const uint8_t own_kind = static_cast<uint8_t>(orbital_node_kind::low_orbit);
        bool all = true;
        std::string why;
        const auto expect = [&](bool r, const char* what) { all = all && r; if (!r) { why += what; why += " "; } };
        expect(!refused(b), "clean-refused");
        expect(refused(poke(s0, &zero, 4)), "next-id-0");
        expect(refused(poke(b0 + 8, &bad_kind, 1)), "node-kind");
        expect(refused(poke(b0 + 9, &bad_state, 1)), "state");
        expect(refused(poke(b0 + 9, &decided, 1)), "decided-both-holding");
        expect(refused(poke(b0 + 4, &ghost, 4)), "node-body");
        expect(refused(poke(b0 + 18, &nan, 4)), "mass-nan");
        expect(refused(poke(b0 + 22, &ghost, 4)), "ghost-nation");
        {
            std::string bad = poke(b0 + 22, &b_id, 4);
            std::memcpy(&bad[b0 + 47], &a_id, 4);
            expect(refused(bad), "sides-out-of-order");
        }
        expect(refused(poke(b0 + 46, &bad_end, 1)), "end-enum");
        expect(refused(poke(b0 + 26, &neg, 4)), "units-lost<0"); // -1.0f bits as an i32: negative
        {
            std::string bad = poke(b0 + 76 + 4, &own_body, 4);
            std::memcpy(&bad[b0 + 76 + 8], &own_kind, 1);
            expect(refused(bad), "withdraw-to-own-node");
        }
        expect(refused(poke(b0 + 76 + 9 + 4 + 4, &neg, 4)), "module-damage<0");
        expect(refused(poke(b0 + 76 + 9 + 4 + 4, &nan, 4)), "module-damage-nan");
        {
            const uint32_t at_next = next_id;
            expect(refused(poke(b0, &at_next, 4)), "id-at-next");
        }
        expect(refused(b.substr(0, b.size() - 1)), "truncated");
        check(all, "S11", "tampered battle sections refused whole (next id, node kind/body, state, decided-holding, "
                          "mass NaN, ghost nation, side order, end, negative tally, withdrawal to its node, module "
                          "damage <0 / NaN, id at next, truncation); the clean stream reads"
                          + (why.empty() ? std::string() : " — FAILED: " + why));
    }

    // ---- S12 determinism ----------------------------------------------------
    std::printf("--- S12 determinism ---\n");
    {
        fixture x, y;
        build(x);
        build(y);
        programme(x);
        programme(y);
        // Both withdrawals go to home high orbit, so the two meet there and fight again.
        const space_battle* bx = x.w.space_battles.empty() ? nullptr : &x.w.space_battles.begin()->second;
        const bool second = x.w.space_battles.size() == 2
                         && std::next(x.w.space_battles.begin())->second.node == x.high;
        check(x.w.state_hash(0) == y.w.state_hash(0) && to_bytes(x.w) == to_bytes(y.w) && bx != nullptr && second
                  && x.w.space_battles == y.w.space_battles,
              "S12", fmt("two fixtures driven identically: equal hash and stream; the two withdrawals met at high "
                         "orbit and fought a second battle (first: state %d, %d days, "
                         "A lost %d, B lost %d units / %d modules / %d stations, mass %.0f)",
                         bx ? (int)bx->state : -1, bx ? bx->days : -1, bx ? bx->sides[0].units_lost : -1,
                         bx ? bx->sides[1].units_lost : -1, bx ? bx->sides[1].modules_lost : -1,
                         bx ? bx->sides[1].stations_lost : -1, bx ? bx->mass_destroyed : -1.0f));
    }

    std::printf("space_battle_harness: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
