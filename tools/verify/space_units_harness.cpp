// ---------------------------------------------------------------------------
// space_units_harness — BL-1594 (space-unit designs) + BL-1595 (movement), the proof
// ---------------------------------------------------------------------------
// docs/military/SPACE_FLEETS.md § A space unit is raised to a design, § Thrust,
// § Where a space unit is, § Raising and launching. A hand-built fixture: a star, a
// home planet (1 Earth mass, moderate air) at 1.0 AU, its moon (STAGED at Luna's
// 0.012 Earth masses, airless — the fixture sets it; generation is not consulted), a
// far planet at 2.0 AU; one market on the home planet and one on the moon; nation A
// holding every far-space key, a staffed Launchpad on the home planet and one on the
// moon; nation B holding no key. Rows:
//
//   U1  DESIGN: refused without the hull's / an item's key (B); refused ill-formed
//       (an empty slot the hull has, an item in a slot it lacks, a wrong-slot item);
//       accepted for A. A refusal mutates nothing.
//   U2  STATS: the design's mass, tank, Δv (rocket equation), cargo, crew,
//       firepower, armour, hull points — printed, and Δv recomputed independently.
//   U3  PAD RAISE (home): the unit appears idle at the home low orbit with a full
//       tank; the HOME shelf loses the kit goods, the tank and the climb's propellant;
//       the moon shelf is untouched; the treasury pays the quote; crew drawn.
//   U4  PAD RAISE (moon): the same off the MOON shelf; the climb's propellant is a
//       fraction of the home launch's.
//   U5  REFUSED WHOLE: no pad on the body, an unstaffed pad, a short shelf, a short
//       treasury, no crew, a Cruiser at a pad, no design — hash, allocator, stores,
//       shelves and treasury untouched.
//   U6  LADEN: an empty hauler reaches the moon's low orbit; a laden one (cargo staged
//       — loading is a convoy's, owed) cannot, and the fleet order with both is
//       refused whole.
//   U7  TRANSIT: the move takes ceil(path days) day steps; then idle at the
//       destination; an in-transit unit cannot be ordered.
//   U8  REFUEL OVER A PAD: in the moon's low orbit, off the moon's shelf, to full;
//       refused off a pad-less low orbit and away from a low orbit.
//   U9  DEPOT: the quarter fills a working depot to its cap off the home shelf; a
//       docked unit refuels from the stock, which falls by what it took.
//   U10 BERTHS: one working Dock berths two; a third is refused; undock frees one;
//       another nation's station refuses.
//   U11 SHIPYARD: a Cruiser is refused at a pad and raised at a working Shipyard —
//       docked there, goods off the home shelf, crew drawn; no shipyard refuses.
//   U12 LANDER: a lander lands at the moon's surface (landed); a hauler is refused a
//       surface; the lander flies back up paying the climb.
//   U13 REPAIR: a docked unit regains repair_per_day a day, capped at max.
//   U14 UPKEEP: paid whole each quarter; unpaid counts; lost at quarters_to_loss,
//       crew with it; a paid quarter clears the count.
//   U15 SEAM: the armed-unit list at a node holds frigates, not haulers.
//   U16 SAVE: round-trip (equal hash, byte-identical re-serialise); the v82 prefix of
//       a unit-free world reads; the section's measured length; tampered tails refuse
//       the stream whole.
//   U17 DETERMINISM: two fixtures driven identically end with equal hash and stream.
//
// Build: node tools/verify/build_harness.js space_units_harness --run
// ---------------------------------------------------------------------------

#include "world/digitisation_tree_data.hpp"
#include "world/early_space_tree_data.hpp"
#include "world/nation_tech.hpp"
#include "world/orbital_graph.hpp"
#include "world/space_assets.hpp"
#include "world/space_tech_keys.hpp"
#include "world/space_units.hpp"
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
    char buf[800];
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
    entity_id home_pad = null_entity, moon_pad = null_entity;
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

entity_id add_pad(world& w, entity_id nation, entity_id tile, float staffed)
{
    const entity_id bid = w.create_entity();
    building_component b{};
    b.tile = tile;
    b.type = building_type::launchpad;
    b.workforce_assigned = staffed;
    b.ticks_remaining = 0;
    w.buildings.emplace(bid, b);
    w.nations.at(nation).state_assets.push_back(bid);
    return bid;
}

void grant_far_space(world& w, entity_id nation)
{
    nation_tech_state& st = w.nation_tech[nation];
    for (const io::es::effect& e : io::es::effects)
        if (e.kind == io::es::effect_kind::mechanic && e.node >= 0 && e.node < io::es::node_count)
            st.es_held.set(static_cast<std::size_t>(e.node));
}

void build(fixture& f)
{
    world& w = f.w;
    f.star = add_body(w, "Star", body_type::star, null_entity, 0.0f, 1.0f, atmosphere_class::none);
    f.home = add_body(w, "Home", body_type::planet, null_entity, 1.0f, 1.0f, atmosphere_class::moderate);
    // STAGED: the moon at Luna's mass, airless (the fixture's own body).
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
    w.nations.at(f.a).capital_tile = f.home_tile;
    w.nations.at(f.b).capital_tile = f.home_tile;
    w.nations.at(f.a).treasury = 5000000.0f;
    w.nations.at(f.b).treasury = 5000000.0f;
    w.nations.at(f.a).astronauts = 60;
    w.nations.at(f.b).astronauts = 60;
    f.home_pad = add_pad(w, f.a, f.home_tile, 1.0f);
    f.moon_pad = add_pad(w, f.a, f.moon_tile, 1.0f);
    grant_far_space(w, f.a);
    w.nation_tech[f.b]; // an entry, holding no Early Space node
    // BL-1582's launch gate (Rocketry, a staffed pad, the core key) stands in front of
    // every station core since the merge: both nations hold the Digitisation tree's
    // near-space keys and B gets a staffed home pad, so build_station founds for each.
    for (entity_id n : {f.a, f.b})
    {
        auto& held = w.nation_tech[n].held;
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
    add_pad(w, f.b, f.home_tile, 1.0f);
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

const space_design& hauler_d()
{
    static const space_design d = design_of({{sk::drive, "drive.chemical"}, {sk::tanks, "tanks.extended"}, {sk::bay, "bay.cargo"}});
    return d;
}
const space_design& lander_d()
{
    static const space_design d = design_of({{sk::drive, "drive.chemical"}, {sk::tanks, "tanks.extended"}, {sk::bay, "bay.passenger"}});
    return d;
}
const space_design& frigate_d()
{
    static const space_design d = design_of({{sk::drive, "drive.nuclear_thermal"}, {sk::tanks, "tanks.extended"},
                                             {sk::armament, "armament.kinetic"}, {sk::armour, "armour.whipple"}});
    return d;
}
const space_design& cruiser_d()
{
    static const space_design d = design_of({{sk::drive, "drive.nuclear_thermal"}, {sk::tanks, "tanks.extended"},
                                             {sk::armament, "armament.missile"}, {sk::armour, "armour.composite"}});
    return d;
}
#define k_hauler  hauler_d()
#define k_lander  lander_d()
#define k_frigate frigate_d()
#define k_cruiser cruiser_d()

struct snap
{
    std::uint64_t hash;
    std::size_t   units;
    std::uint32_t cursor;
    float         ta, tb, hp, mp, hs;
    int32_t       pa;
};

snap take(const fixture& f)
{
    return {f.w.state_hash(0), f.w.space_units.size(), f.w.next_entity_id(),
            f.w.nations.at(f.a).treasury, f.w.nations.at(f.b).treasury,
            f.w.markets.at(f.home_mkt).inventory[PR], f.w.markets.at(f.moon_mkt).inventory[PR],
            f.w.markets.at(f.home_mkt).inventory[SC], f.w.nations.at(f.a).astronauts};
}

bool same(const snap& x, const snap& y)
{
    return x.hash == y.hash && x.units == y.units && x.cursor == y.cursor && x.ta == y.ta && x.tb == y.tb
        && x.hp == y.hp && x.mp == y.mp && x.hs == y.hs && x.pa == y.pa;
}

int slot_of(int c, int r) { return r * station_grid_width(0) + c; }

/// A home-low-orbit station for @p nation with power, habitation, life support, a
/// Dock, a Shipyard and a Fuel depot, all arrived and working.
entity_id build_station(fixture& f, entity_id nation, bool yard, bool depot)
{
    world& w = f.w;
    const space_asset_params sp{};
    const station_params stp{};
    w.current_econ_tick = 1;
    const station_op s = found_station(w, sp, stp, nation, {f.home, orbital_node_kind::low_orbit});
    fit_station_module(w, sp, stp, nation, s.station, slot_of(0, 0), station_module_kind::dock);
    if (yard)
        fit_station_module(w, sp, stp, nation, s.station, slot_of(1, 0), station_module_kind::shipyard);
    fit_station_module(w, sp, stp, nation, s.station, slot_of(3, 0), station_module_kind::power);
    fit_station_module(w, sp, stp, nation, s.station, slot_of(4, 0), station_module_kind::habitation);
    fit_station_module(w, sp, stp, nation, s.station, slot_of(0, 2), station_module_kind::life_support);
    if (depot)
        fit_station_module(w, sp, stp, nation, s.station, slot_of(1, 2), station_module_kind::fuel_depot);
    run_space_asset_step(w, sp, 1);
    run_station_step(w, stp, 1);
    return s.station;
}

/// Walk @p n days.
int days(world& w, const space_unit_params& p, int n)
{
    const station_params stp{};
    int arrived = 0;
    for (int i = 0; i < n; ++i)
    {
        space_unit_day_report r;
        run_space_unit_day(w, p, stp, &r);
        arrived += r.arrived;
    }
    return arrived;
}

/// The scripted programme U17 runs twice.
void programme(fixture& f, const space_unit_params& p)
{
    world& w = f.w;
    const station_params stp{};
    set_space_unit_design(w, f.a, space_hull::hauler, k_hauler);
    set_space_unit_design(w, f.a, space_hull::cruiser, k_cruiser);
    const entity_id st = build_station(f, f.a, true, true);
    const space_unit_op h = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.home);
    raise_space_unit_at_shipyard(w, p, stp, f.a, space_hull::cruiser, st);
    order_space_unit(w, p, f.a, h.unit, {f.moon, orbital_node_kind::low_orbit});
    days(w, p, 40);
    refuel_space_unit(w, p, stp, f.a, h.unit);
    run_space_unit_quarter(w, p, stp, 2);
    run_space_unit_quarter(w, p, stp, 3);
}

} // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const space_unit_params p{};
    const station_params    stp{};
    std::printf("space_units_harness — BL-1594 + BL-1595\n");
    std::printf("dials: launch_propellant_per_dv %.3f  repair/day %.2f  depot cap/depot %.0f  "
                "quarters_to_loss %d  berths/dock %d  reservation_mult %.2f\n",
                p.launch_propellant_per_dv, p.repair_per_day, p.depot_stock_per_depot, p.quarters_to_loss,
                p.berths_per_dock, p.reservation_mult);
    std::printf("hull table (mass, hp, crew, upkeep, surface launch, key):\n");
    for (const space_hull_spec& h : space_hull_table())
        std::printf("  %-8s %5.1f %6.1f %2d %7.1f %s %.*s\n", h.name, h.mass, h.hull_points, h.crew, h.upkeep,
                    h.surface_launch ? "yes" : "no ", static_cast<int>(h.key.size()), h.key.data());
    std::printf("item catalogue (id, slot, hulls, mass, ve, tank, cargo, pax, fp, arm, hp, key):\n");
    for (const space_item& it : space_item_catalogue())
        std::printf("  %-22s %u 0x%02X %4.1f %5.1f %5.1f %5.1f %2d %5.1f %5.1f %5.1f %.*s\n", it.id,
                    static_cast<unsigned>(it.slot), it.hull_mask, it.mass, it.exhaust, it.tank, it.cargo,
                    it.passengers, it.firepower, it.armour, it.hull_points,
                    static_cast<int>(it.key.size()), it.key.empty() ? "(opens with the hull)" : it.key.data());

    fixture f;
    build(f);
    world& w = f.w;
    w.current_econ_tick = 1;

    // ---- U1 designs ---------------------------------------------------------
    std::printf("--- U1 designs ---\n");
    {
        const snap s0 = take(f);
        const space_unit_result rb = set_space_unit_design(w, f.b, space_hull::hauler, k_hauler);
        space_design missing = k_hauler;
        missing[static_cast<std::size_t>(sk::tanks)] = k_no_space_item;
        space_design extra = k_hauler;
        extra[static_cast<std::size_t>(sk::armament)] = k_frigate[static_cast<std::size_t>(sk::armament)];
        space_design wrong = k_hauler;
        wrong[static_cast<std::size_t>(sk::drive)] = k_hauler[static_cast<std::size_t>(sk::tanks)];
        const space_unit_result r1 = set_space_unit_design(w, f.a, space_hull::hauler, missing);
        const space_unit_result r2 = set_space_unit_design(w, f.a, space_hull::hauler, extra);
        const space_unit_result r3 = set_space_unit_design(w, f.a, space_hull::hauler, wrong);
        const space_unit_result r4 = set_space_unit_design(w, f.a, static_cast<space_hull>(9), k_hauler);
        check(rb == space_unit_result::rejected_tech_locked && r1 == space_unit_result::rejected_invalid
                  && r2 == space_unit_result::rejected_invalid && r3 == space_unit_result::rejected_invalid
                  && r4 == space_unit_result::rejected_invalid && same(s0, take(f))
                  && w.space_unit_designs.empty(),
              "U1a", fmt("refusals: no keys %d, empty slot %d, foreign slot %d, wrong-slot item %d, bad hull %d; "
                         "world untouched", (int)rb, (int)r1, (int)r2, (int)r3, (int)r4));
        // A key held one by one: B gets the hull but not the drive.
        nation_tech_state& tb = w.nation_tech[f.b];
        for (const io::es::effect& e : io::es::effects)
            if (e.kind == io::es::effect_kind::mechanic && space_keys::hull_hauler == e.target)
                tb.es_held.set(static_cast<std::size_t>(e.node));
        const space_unit_result rb2 = set_space_unit_design(w, f.b, space_hull::hauler, k_hauler);
        tb.es_held.reset();
        check(rb2 == space_unit_result::rejected_tech_locked, "U1b",
              fmt("hull key alone: still refused (%d) — every item's key is needed", (int)rb2));
        const space_unit_result a1 = set_space_unit_design(w, f.a, space_hull::hauler, k_hauler);
        const space_unit_result a2 = set_space_unit_design(w, f.a, space_hull::lander, k_lander);
        const space_unit_result a3 = set_space_unit_design(w, f.a, space_hull::frigate, k_frigate);
        const space_unit_result a4 = set_space_unit_design(w, f.a, space_hull::cruiser, k_cruiser);
        check(a1 == space_unit_result::ok && a2 == space_unit_result::ok && a3 == space_unit_result::ok
                  && a4 == space_unit_result::ok && space_unit_design_of(w, f.a, space_hull::hauler) != nullptr
                  && *space_unit_design_of(w, f.a, space_hull::hauler) == k_hauler
                  && space_unit_design_of(w, f.a, space_hull::tug) == nullptr,
              "U1c", "A sets hauler, lander, frigate, cruiser designs; one live design per hull");
    }

    // ---- U2 stats -----------------------------------------------------------
    std::printf("--- U2 stats ---\n");
    {
        const space_hull hulls[] = {space_hull::hauler, space_hull::lander, space_hull::frigate, space_hull::cruiser};
        const space_design* ds[] = {&k_hauler, &k_lander, &k_frigate, &k_cruiser};
        bool ok = true;
        for (int i = 0; i < 4; ++i)
        {
            const space_design_stats st = evaluate_space_design(hulls[i], *ds[i]);
            const float dv = st.exhaust * std::log((st.dry_mass + st.tank) / st.dry_mass);
            std::printf("  %-8s dry %5.1f tank %5.1f ve %5.1f dv %6.3f cargo %5.1f pax %d crew %d fp %5.1f arm %5.1f hp %6.1f upkeep %6.0f\n",
                        space_hull_table()[static_cast<std::size_t>(hulls[i])].name, st.dry_mass, st.tank,
                        st.exhaust, st.dv_capacity, st.cargo, st.passengers, st.crew, st.firepower, st.armour,
                        st.hull_points, st.upkeep);
            ok = ok && st.valid && std::fabs(dv - st.dv_capacity) < 1e-4f;
        }
        const space_design_stats hs = evaluate_space_design(space_hull::hauler, k_hauler);
        ok = ok && hs.dry_mass == 20.0f && hs.tank == 45.0f && hs.cargo == 40.0f && hs.crew == 1;
        check(ok, "U2", "every design valid; Δv = ve ln((dry+tank)/dry) recomputed; hauler dry 20 tank 45 cargo 40");
    }

    // ---- U3 pad raise (home) ------------------------------------------------
    std::printf("--- U3 raise at the home pad ---\n");
    const double home_climb = orbital_climb_dv(w, orbital_graph_params{}, f.home);
    const double moon_climb = orbital_climb_dv(w, orbital_graph_params{}, f.moon);
    std::printf("  climb dv: home %.3f  moon %.3f\n", home_climb, moon_climb);
    entity_id h1 = null_entity;
    float home_climb_prop = 0.0f;
    {
        const space_design_stats st = evaluate_space_design(space_hull::hauler, k_hauler);
        const snap s0 = take(f);
        const float mp0 = w.markets.at(f.moon_mkt).inventory[PR];
        const space_unit_op op = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.home);
        h1 = op.unit;
        home_climb_prop = op.climb_propellant;
        const float expect_climb = static_cast<float>(home_climb * p.launch_propellant_per_dv * (st.dry_mass + st.tank));
        const space_unit* u = w.space_units.count(h1) ? &w.space_units.at(h1) : nullptr;
        const float drawn_p = s0.hp - w.markets.at(f.home_mkt).inventory[PR];
        const float drawn_sc = s0.hs - w.markets.at(f.home_mkt).inventory[SC];
        double expect_credits = 0.0;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            float q = st.goods[r];
            if (r == PR) q += st.tank + expect_climb;
            expect_credits += q * w.markets.at(f.home_mkt).base_price[r];
        }
        std::printf("  op %d credits %.1f (expect %.1f) prop %.2f climb %.2f (expect %.2f) sc drawn %.1f\n",
                    (int)op.result, op.credits, expect_credits, op.propellant, op.climb_propellant, expect_climb, drawn_sc);
        check(op.result == space_unit_result::ok && u != nullptr && u->state == space_unit_state::idle
                  && u->node == orbital_node{f.home, orbital_node_kind::low_orbit} && u->propellant == st.tank
                  && u->hull_points == st.hull_points && u->crew == 1 && u->design == k_hauler
                  && std::fabs(op.climb_propellant - expect_climb) < 1e-3f
                  && std::fabs(drawn_p - (st.tank + expect_climb)) < 1e-2f && drawn_sc == st.goods[SC]
                  && w.markets.at(f.moon_mkt).inventory[PR] == mp0 && op.market == f.home_mkt
                  && std::fabs((s0.ta - w.nations.at(f.a).treasury) - op.credits) < 1.0f
                  && std::fabs(op.credits - static_cast<float>(expect_credits)) < 1.0f
                  && w.nations.at(f.a).astronauts == s0.pa - 1,
              "U3", "idle at home low orbit, full tank; home shelf drawn kit + tank + climb; moon shelf untouched; treasury paid; crew drawn");
    }

    // ---- U4 pad raise (moon) ------------------------------------------------
    std::printf("--- U4 raise at the moon pad ---\n");
    {
        const snap s0 = take(f);
        const space_unit_op op = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.moon);
        const float drawn_moon = s0.mp - w.markets.at(f.moon_mkt).inventory[PR];
        const space_unit* u = w.space_units.count(op.unit) ? &w.space_units.at(op.unit) : nullptr;
        std::printf("  climb propellant: home %.2f  moon %.2f  (ratio %.3f)\n", home_climb_prop,
                    op.climb_propellant, op.climb_propellant / home_climb_prop);
        check(op.result == space_unit_result::ok && u != nullptr
                  && u->node == orbital_node{f.moon, orbital_node_kind::low_orbit} && op.market == f.moon_mkt
                  && w.markets.at(f.home_mkt).inventory[PR] == s0.hp && drawn_moon > 0.0f
                  && op.climb_propellant < 0.25f * home_climb_prop,
              "U4", "moon launch off the moon shelf; its climb propellant a fraction (< 1/4) of the home launch's");
    }

    // ---- U5 refused whole ---------------------------------------------------
    std::printf("--- U5 refused whole ---\n");
    {
        const snap s0 = take(f);
        const space_unit_result no_pad = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.far).result;
        w.buildings.at(f.moon_pad).workforce_assigned = 0.0f;
        const space_unit_result unstaffed = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.moon).result;
        w.buildings.at(f.moon_pad).workforce_assigned = 1.0f;
        const float keep = w.markets.at(f.moon_mkt).inventory[PR];
        w.markets.at(f.moon_mkt).inventory[PR] = 10.0f;
        const snap s1 = take(f);
        const space_unit_result shelf = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.moon).result;
        const bool shelf_same = same(s1, take(f));
        w.markets.at(f.moon_mkt).inventory[PR] = keep;
        const float tk = w.nations.at(f.a).treasury;
        w.nations.at(f.a).treasury = 100.0f;
        const snap s2 = take(f);
        const space_unit_result poor = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.home).result;
        const bool poor_same = same(s2, take(f));
        w.nations.at(f.a).treasury = tk;
        const int32_t pk = w.nations.at(f.a).astronauts;
        w.nations.at(f.a).astronauts = 0;
        const snap s3 = take(f);
        const space_unit_result crew = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.home).result;
        const bool crew_same = same(s3, take(f));
        w.nations.at(f.a).astronauts = pk;
        const space_unit_result cruiser = raise_space_unit_at_pad(w, p, f.a, space_hull::cruiser, f.home).result;
        const space_unit_result nodesign = raise_space_unit_at_pad(w, p, f.a, space_hull::tug, f.home).result;
        const space_unit_result locked = raise_space_unit_at_pad(w, p, f.b, space_hull::hauler, f.home).result;
        check(no_pad == space_unit_result::rejected_no_pad && unstaffed == space_unit_result::rejected_no_pad
                  && shelf == space_unit_result::rejected_shelf_short && shelf_same
                  && poor == space_unit_result::rejected_insufficient_treasury && poor_same
                  && crew == space_unit_result::rejected_no_crew && crew_same
                  && cruiser == space_unit_result::rejected_not_launchable
                  && nodesign == space_unit_result::rejected_no_design
                  && locked == space_unit_result::rejected_no_design && same(s0, take(f)),
              "U5", fmt("no pad %d, unstaffed %d, shelf %d, treasury %d, crew %d, cruiser %d, no design %d, B %d — nothing written",
                        (int)no_pad, (int)unstaffed, (int)shelf, (int)poor, (int)crew, (int)cruiser, (int)nodesign, (int)locked));
    }

    // ---- U6 laden vs empty --------------------------------------------------
    std::printf("--- U6 laden ---\n");
    const orbital_node home_low{f.home, orbital_node_kind::low_orbit};
    const orbital_node moon_low{f.moon, orbital_node_kind::low_orbit};
    const orbital_path_result path = orbital_path(w, orbital_graph_params{}, home_low, moon_low);
    entity_id h2 = null_entity;
    {
        h2 = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.home).unit;
        w.space_units.at(h2).cargo = 40.0f; // STAGED: loading is a convoy's (owed)
        const float dv_empty = space_unit_dv_remaining(w.space_units.at(h1));
        const float dv_laden = space_unit_dv_remaining(w.space_units.at(h2));
        std::printf("  path home low -> moon low: dv %.3f days %.2f (%zu nodes); empty dv %.3f laden dv %.3f\n",
                    path.dv, path.days, path.nodes.size(), dv_empty, dv_laden);
        const snap s0 = take(f);
        const space_unit_op fleet = order_space_fleet(w, p, f.a, home_low, moon_low);
        const bool fleet_same = same(s0, take(f));
        const space_unit_op laden = order_space_unit(w, p, f.a, h2, moon_low);
        const bool laden_same = same(s0, take(f));
        check(path.ok && dv_empty >= path.dv && dv_laden < path.dv
                  && fleet.result == space_unit_result::rejected_insufficient_dv && fleet_same
                  && laden.result == space_unit_result::rejected_insufficient_dv && laden_same,
              "U6a", "the laden hauler cannot pay the path; it alone and the fleet with it are refused whole");
        w.space_units.at(h2).cargo = 0.0f;
        const space_unit_op fleet2 = order_space_fleet(w, p, f.a, home_low, moon_low);
        const space_unit& u1 = w.space_units.at(h1);
        const space_unit& u2 = w.space_units.at(h2);
        const space_design_stats st = evaluate_space_design(space_hull::hauler, k_hauler);
        const float expect_burn = static_cast<float>((st.dry_mass + st.tank) * (1.0 - std::exp(-path.dv / st.exhaust)));
        check(fleet2.result == space_unit_result::ok && u1.state == space_unit_state::in_transit
                  && u2.state == space_unit_state::in_transit && u1.dest == moon_low
                  && std::fabs((st.tank - u1.propellant) - expect_burn) < 1e-3f
                  && fleet2.days == static_cast<int32_t>(std::ceil(path.days)),
              "U6b", fmt("emptied, the fleet goes: burned %.3f each (expect %.3f), %d days",
                         st.tank - u1.propellant, expect_burn, fleet2.days));
    }

    // ---- U7 transit ---------------------------------------------------------
    std::printf("--- U7 transit ---\n");
    {
        const int need = static_cast<int>(std::ceil(path.days));
        const space_unit_result busy = order_space_unit(w, p, f.a, h1, home_low).result;
        const int a0 = days(w, p, need - 1);
        const bool still = w.space_units.at(h1).state == space_unit_state::in_transit;
        const int a1 = days(w, p, 1);
        const space_unit& u = w.space_units.at(h1);
        check(busy == space_unit_result::rejected_busy && a0 == 0 && still && a1 == 2
                  && u.state == space_unit_state::idle && u.node == moon_low && u.days_remaining == 0
                  && u.dest == moon_low,
              "U7", fmt("in transit refuses orders; %d days then idle at the moon's low orbit", need));
    }

    // ---- U8 refuel over a pad ----------------------------------------------
    std::printf("--- U8 refuel over a pad ---\n");
    {
        const space_design_stats st = evaluate_space_design(space_hull::hauler, k_hauler);
        const float need = st.tank - w.space_units.at(h1).propellant;
        const float m0 = w.markets.at(f.moon_mkt).inventory[PR];
        const float t0 = w.nations.at(f.a).treasury;
        const space_unit_op op = refuel_space_unit(w, p, stp, f.a, h1);
        const float drew = m0 - w.markets.at(f.moon_mkt).inventory[PR];
        check(op.result == space_unit_result::ok && w.space_units.at(h1).propellant == st.tank
                  && std::fabs(drew - need) < 1e-3f && op.market == f.moon_mkt
                  && std::fabs((t0 - w.nations.at(f.a).treasury) - need * 64.0f) < 0.5f,
              "U8a", fmt("moon low orbit: bought %.3f off the moon shelf for %.1f; tank full", drew, op.credits));
        // Away from a low orbit, and over a body with no pad of the nation.
        order_space_unit(w, p, f.a, h2, {f.moon, orbital_node_kind::high_orbit});
        days(w, p, 30);
        const space_unit_result away = refuel_space_unit(w, p, stp, f.a, h2).result;
        w.buildings.at(f.moon_pad).workforce_assigned = 0.0f;
        order_space_unit(w, p, f.a, h2, moon_low);
        days(w, p, 30);
        const space_unit_result nopad = refuel_space_unit(w, p, stp, f.a, h2).result;
        w.buildings.at(f.moon_pad).workforce_assigned = 1.0f;
        check(away == space_unit_result::rejected_wrong_node && nopad == space_unit_result::rejected_no_pad,
              "U8b", fmt("high orbit refused (%d); low orbit with the pad unstaffed refused (%d)", (int)away, (int)nopad));
    }

    // ---- U9 depot -----------------------------------------------------------
    std::printf("--- U9 depot ---\n");
    const entity_id st_a = build_station(f, f.a, true, true);
    entity_id h3 = null_entity;
    {
        const float h0 = w.markets.at(f.home_mkt).inventory[PR];
        space_unit_quarter_report qr;
        run_space_unit_quarter(w, p, stp, 2, &qr);
        const float stock = w.station_depot_stock.count(st_a) ? w.station_depot_stock.at(st_a) : 0.0f;
        check(stock == p.depot_stock_per_depot && std::fabs((h0 - w.markets.at(f.home_mkt).inventory[PR]) - stock) < 1e-2f
                  && qr.depot_bought == stock,
              "U9a", fmt("one working depot filled to %.1f off the home shelf (%.1f credits)", stock, qr.depot_spent));
        h3 = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.home).unit;
        order_space_unit(w, p, f.a, h3, {f.home, orbital_node_kind::high_orbit});
        days(w, p, 30);
        order_space_unit(w, p, f.a, h3, home_low);
        days(w, p, 30);
        const space_unit_result dk = dock_space_unit(w, p, stp, f.a, h3, st_a);
        const float before = w.space_units.at(h3).propellant;
        const float t0 = w.nations.at(f.a).treasury;
        const space_unit_op op = refuel_space_unit(w, p, stp, f.a, h3);
        const float tank = evaluate_space_design(space_hull::hauler, k_hauler).tank;
        const float after_stock = w.station_depot_stock.count(st_a) ? w.station_depot_stock.at(st_a) : 0.0f;
        check(dk == space_unit_result::ok && op.result == space_unit_result::ok
                  && w.space_units.at(h3).propellant == tank
                  && std::fabs((stock - after_stock) - (tank - before)) < 1e-3f && w.nations.at(f.a).treasury == t0,
              "U9b", fmt("docked: drew %.3f from the depot (stock %.1f -> %.1f), no treasury debit", tank - before, stock, after_stock));
    }

    // ---- U10 berths ---------------------------------------------------------
    std::printf("--- U10 berths ---\n");
    {
        const entity_id h4 = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.home).unit;
        const entity_id h5 = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.home).unit;
        const int berths = space_station_berths(w, p, stp, st_a);
        const space_unit_result d4 = dock_space_unit(w, p, stp, f.a, h4, st_a);
        const snap s0 = take(f);
        const space_unit_result d5 = dock_space_unit(w, p, stp, f.a, h5, st_a);
        const bool same5 = same(s0, take(f));
        const space_unit_result un = undock_space_unit(w, f.a, h3);
        const space_unit_result d5b = dock_space_unit(w, p, stp, f.a, h5, st_a);
        // Another nation's station refuses.
        set_space_unit_design(w, f.b, space_hull::hauler, k_hauler); // B holds no key: refused
        const entity_id st_b = build_station(f, f.b, false, false);
        undock_space_unit(w, f.a, h5);
        const space_unit_result foreign = dock_space_unit(w, p, stp, f.a, h5, st_b);
        check(berths == 2 && d4 == space_unit_result::ok && d5 == space_unit_result::rejected_no_dock && same5
                  && un == space_unit_result::ok && d5b == space_unit_result::ok
                  && foreign == space_unit_result::rejected_not_owner && space_station_docked(w, st_a) == 1,
              "U10", fmt("berths %d; third refused (%d); undock frees one; a foreign station refuses (%d)",
                         berths, (int)d5, (int)foreign));
    }

    // ---- U11 shipyard -------------------------------------------------------
    std::printf("--- U11 shipyard ---\n");
    entity_id cruiser = null_entity;
    {
        const space_unit_result pad = raise_space_unit_at_pad(w, p, f.a, space_hull::cruiser, f.home).result;
        const snap s0 = take(f);
        const space_unit_op op = raise_space_unit_at_shipyard(w, p, stp, f.a, space_hull::cruiser, st_a);
        const space_design_stats st = evaluate_space_design(space_hull::cruiser, k_cruiser);
        const space_unit* u = w.space_units.count(op.unit) ? &w.space_units.at(op.unit) : nullptr;
        const float drawn_sc = s0.hs - w.markets.at(f.home_mkt).inventory[SC];
        const float drawn_p = s0.hp - w.markets.at(f.home_mkt).inventory[PR];
        const int32_t pool_after = w.nations.at(f.a).astronauts;
        cruiser = op.unit;
        // A station without a yard (B's) — and a station of another nation.
        const entity_id st_noyard = build_station(f, f.a, false, false);
        const space_unit_result noyard = raise_space_unit_at_shipyard(w, p, stp, f.a, space_hull::cruiser, st_noyard).result;
        check(pad == space_unit_result::rejected_not_launchable && op.result == space_unit_result::ok && u != nullptr
                  && u->state == space_unit_state::docked && u->station == st_a && u->node == home_low
                  && u->crew == st.crew && pool_after == s0.pa - st.crew
                  && drawn_sc == st.goods[SC] && std::fabs(drawn_p - st.tank) < 1e-3f && op.climb_propellant == 0.0f
                  && noyard == space_unit_result::rejected_no_shipyard,
              "U11", fmt("cruiser: pad refused (%d); yard raised docked, crew %d, %.0f components + %.0f propellant off the home shelf; no yard refused",
                         (int)pad, st.crew, drawn_sc, drawn_p));
    }

    // ---- U12 lander ---------------------------------------------------------
    std::printf("--- U12 lander ---\n");
    {
        const entity_id l = raise_space_unit_at_pad(w, p, f.a, space_hull::lander, f.moon).unit;
        const entity_id hh = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.moon).unit;
        const orbital_node surf{f.moon, orbital_node_kind::surface};
        const space_unit_result hauler_down = order_space_unit(w, p, f.a, hh, surf).result;
        const space_unit_op down = order_space_unit(w, p, f.a, l, surf);
        days(w, p, std::max(1, down.days));
        const bool landed = w.space_units.at(l).state == space_unit_state::landed && w.space_units.at(l).node == surf;
        const float before = w.space_units.at(l).propellant;
        const space_unit_op up = order_space_unit(w, p, f.a, l, moon_low);
        days(w, p, std::max(1, up.days));
        check(hauler_down == space_unit_result::rejected_surface && down.result == space_unit_result::ok && landed
                  && up.result == space_unit_result::ok && w.space_units.at(l).state == space_unit_state::idle
                  && w.space_units.at(l).propellant < before && std::fabs(up.dv - static_cast<float>(moon_climb)) < 1e-3f,
              "U12", fmt("hauler refused a surface; lander landed; flew back up paying the climb %.3f", up.dv));
    }

    // ---- U13 repair ---------------------------------------------------------
    std::printf("--- U13 repair ---\n");
    {
        // The cruiser, docked at its yard's station since U11.
        space_unit& u = w.space_units.at(cruiser);
        const space_unit_result dk = u.state == space_unit_state::docked ? space_unit_result::ok
                                                                         : space_unit_result::rejected_busy;
        const float max_hp = space_unit_max_hull_points(u);
        u.hull_points = max_hp - 2.5f;
        days(w, p, 1);
        const float one = u.hull_points;
        days(w, p, 5);
        check(dk == space_unit_result::ok && one == max_hp - 2.5f + p.repair_per_day && u.hull_points == max_hp,
              "U13", fmt("docked: +%.1f a day, capped at %.1f", p.repair_per_day, max_hp));
    }

    // ---- U14 upkeep ---------------------------------------------------------
    std::printf("--- U14 upkeep ---\n");
    {
        fixture g;
        build(g);
        set_space_unit_design(g.w, g.a, space_hull::hauler, k_hauler);
        const entity_id u = raise_space_unit_at_pad(g.w, p, g.a, space_hull::hauler, g.home).unit;
        const float t0 = g.w.nations.at(g.a).treasury;
        space_unit_quarter_report r1;
        run_space_unit_quarter(g.w, p, stp, 2, &r1);
        const bool paid = r1.paid == 1 && g.w.nations.at(g.a).treasury == t0 - g.w.space_units.at(u).upkeep;
        g.w.nations.at(g.a).treasury = 0.0f;
        run_space_unit_quarter(g.w, p, stp, 3);
        run_space_unit_quarter(g.w, p, stp, 4);
        const int32_t unpaid2 = g.w.space_units.at(u).unpaid_quarters;
        g.w.nations.at(g.a).treasury = 1.0e6f;
        run_space_unit_quarter(g.w, p, stp, 5);
        const int32_t cleared = g.w.space_units.at(u).unpaid_quarters;
        g.w.nations.at(g.a).treasury = 0.0f;
        int lost = 0, crew_lost = 0;
        for (int q = 6; q < 6 + p.quarters_to_loss; ++q)
        {
            space_unit_quarter_report r;
            run_space_unit_quarter(g.w, p, stp, q, &r);
            lost += r.lost;
            crew_lost += r.crew_lost;
        }
        check(paid && unpaid2 == 2 && cleared == 0 && lost == 1 && crew_lost == 1 && g.w.space_units.empty(),
              "U14", fmt("paid whole; two unpaid quarters counted; a paid one clears; lost after %d unpaid, crew %d with it",
                         p.quarters_to_loss, crew_lost));
    }

    // ---- U15 seam -----------------------------------------------------------
    std::printf("--- U15 armed seam ---\n");
    {
        const entity_id fr = raise_space_unit_at_pad(w, p, f.a, space_hull::frigate, f.home).unit;
        const std::vector<entity_id> armed = space_armed_units_at(w, f.a, home_low);
        bool frig_in = false, hauler_in = false;
        for (const entity_id e : armed)
        {
            frig_in = frig_in || e == fr;
            hauler_in = hauler_in || w.space_units.at(e).hull == space_hull::hauler;
        }
        const space_unit& u = w.space_units.at(fr);
        check(frig_in && !hauler_in && space_unit_firepower(u) == 10.0f && space_unit_armour(u) == 8.0f
                  && u.hull_points == space_unit_max_hull_points(u),
              "U15", fmt("%zu armed at home low orbit (the frigate and the cruiser); firepower %.0f armour %.0f hp %.0f",
                         armed.size(), space_unit_firepower(u), space_unit_armour(u), u.hull_points));
    }

    // ---- U16 save -----------------------------------------------------------
    std::printf("--- U16 save ---\n");
    {
        // An in-transit unit, so every state but landed is in the stream; then landed.
        order_space_unit(w, p, f.a, h2, {f.moon, orbital_node_kind::l4});
        const std::string b = to_bytes(w);
        world w2;
        const bool ok = from_bytes(b, w2);
        const std::string b2 = ok ? to_bytes(w2) : std::string();
        const bool states = [&] {
            int seen[4] = {0, 0, 0, 0};
            for (const auto& [id, u] : w2.space_units) ++seen[static_cast<int>(u.state)];
            return seen[0] > 0 && seen[1] > 0 && seen[2] > 0;
        }();
        check(ok && w2.state_hash(0) == w.state_hash(0) && b2 == b && w2.space_units.size() == w.space_units.size()
                  && w2.space_unit_designs == w.space_unit_designs && w2.station_depot_stock == w.station_depot_stock
                  && states,
              "U16a", fmt("round-trip: %zu units, %zu design nations, %zu depots; equal hash, byte-identical",
                          w2.space_units.size(), w2.space_unit_designs.size(), w2.station_depot_stock.size()));

        const std::size_t sec = (world_save_space_unit_section_bytes(w) + world_save_prospecting_section_bytes(w));
        std::ostringstream os82(std::ios::binary);
        write_world_snapshot_pre_space_units(w, os82);
        const std::string b82 = os82.str();
        check(b82.size() + sec == b.size(),
              "U16b", fmt("section %zu bytes; v82 prefix %zu + %zu == %zu", sec, b82.size(), sec, b.size()));
        // The v82 prefix of a unit-free world reads, as a world with none.
        fixture e;
        build(e);
        std::ostringstream ose(std::ios::binary);
        write_world_snapshot_pre_space_units(e.w, ose);
        world we;
        const bool ok82 = from_bytes(ose.str(), we);
        check(ok82 && we.space_units.empty() && we.space_unit_designs.empty() && we.station_depot_stock.empty()
                  && we.state_hash(0) == e.w.state_hash(0) && world_save_space_unit_section_bytes(e.w) == 12,
              "U16c", "a unit-free world's v82 prefix reads with no designs, units or depots; equal hash");

        // Tampered tails. Offsets within the section.
        const std::size_t s0 = b.size() - sec;
        std::size_t off_units = s0 + 4;
        for (const auto& [nid, ds] : w.space_unit_designs) off_units += 8 + 7 * ds.size();
        const std::size_t u0 = off_units + 4; // the first unit's record
        const std::size_t depots = off_units + 4 + 58 * w.space_units.size();
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
        const uint8_t bad_hull = 7, bad_state = 9, bad_item = 200, surf = 0;
        const float big = 1.0e6f, neg = -1.0f, nan = std::nanf("");
        const uint32_t ghost = 0x00FFFFFFu;
        const int32_t zero_days = 0;
        bool all = true;
        std::string why;
        const auto expect = [&](bool r, const char* what) { all = all && r; if (!r) why += what; why += r ? "" : " "; };
        expect(refused(poke(s0 + 4 + 4 + 4, &bad_hull, 1)), "design-hull");
        expect(refused(poke(s0 + 4 + 4 + 4 + 1, &bad_item, 1)), "design-item");
        expect(refused(poke(u0 + 8, &bad_hull, 1)), "unit-hull");
        expect(refused(poke(u0 + 9, &bad_item, 1)), "unit-item");
        expect(refused(poke(u0 + 15, &bad_state, 1)), "unit-state");
        expect(refused(poke(u0 + 16, &ghost, 4)), "unit-body");
        {
            std::string bad = poke(u0 + 20, &surf, 1);
            std::memcpy(&bad[u0 + 25], &surf, 1); // dest too: dest == node still holds
            expect(refused(bad), "idle-at-surface");
        }
        expect(refused(poke(u0 + 34, &big, 4)), "propellant>tank");
        expect(refused(poke(u0 + 38, &neg, 4)), "cargo<0");
        expect(refused(poke(u0 + 42, &nan, 4)), "hp-nan");
        expect(refused(poke(u0 + 4, &ghost, 4)), "owner");
        expect(refused(b.substr(0, b.size() - 1)), "truncated");
        if (!w.station_depot_stock.empty())
        {
            expect(refused(poke(depots + 4, &ghost, 4)), "depot-station");
            expect(refused(poke(depots + 8, &neg, 4)), "depot-negative");
        }
        // A docked unit's station rewritten to a ghost.
        std::size_t at = u0;
        for (const auto& [id, u] : w.space_units)
        {
            if (u.state == space_unit_state::docked)
            {
                expect(refused(poke(at + 30, &ghost, 4)), "docked-ghost");
                break;
            }
            at += 58;
        }
        // An in-transit unit's days zeroed.
        at = u0;
        for (const auto& [id, u] : w.space_units)
        {
            if (u.state == space_unit_state::in_transit)
            {
                expect(refused(poke(at + 26, &zero_days, 4)), "transit-days0");
                break;
            }
            at += 58;
        }
        check(all, "U16d", "tampered tails refused whole (design hull/item, unit hull/item/state/body/surface, "
                           "propellant, cargo, hp, owner, truncation, depot station/stock, docked ghost, transit days)"
                           + (why.empty() ? std::string() : " — FAILED: " + why));
    }

    // ---- U17 determinism ----------------------------------------------------
    std::printf("--- U17 determinism ---\n");
    {
        fixture x, y;
        build(x);
        build(y);
        programme(x, p);
        programme(y, p);
        check(x.w.state_hash(0) == y.w.state_hash(0) && to_bytes(x.w) == to_bytes(y.w) && !x.w.space_units.empty(),
              "U17", fmt("two runs: equal hash %016llx, identical stream, %zu units",
                         static_cast<unsigned long long>(x.w.state_hash(0)), x.w.space_units.size()));
    }

    std::printf("\nspace_units_harness: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
