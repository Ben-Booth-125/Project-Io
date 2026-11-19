// ---------------------------------------------------------------------------
// space_convoy_harness — BL-1596 (space convoys as Haulers), the proof
// ---------------------------------------------------------------------------
// docs/military/SPACE_FLEETS.md § Space convoys, § Raising and launching;
// docs/economy/SUPPLY.md § A shipment. The fixture is space_units_harness's: a star,
// a home planet (1 Earth mass, moderate air) at 1.0 AU, its moon (STAGED at Luna's
// 0.012 Earth masses, airless), a far planet at 2.0 AU with no pad; a market on the
// home planet and one on the moon; nation A holding every far-space key and the
// Digitisation near-space keys, a staffed Launchpad on the home planet and one on the
// moon; nation B holding no far-space key. Rows:
//
//   C1  LEGACY: A with no Hauler design prices the inter-body leg as the straight-line
//       lane (hauler false), as before.
//   C2  QUOTE: with a Hauler design the leg is a Hauler leg; its cap — the laden
//       outbound and the empty return fitting one tank — recomputed independently
//       from the rocket equation; the days hop by hop.
//   C3  CLIMB: the lift off the moon costs a fraction (< 1/4) of the lift off home,
//       per unit of cargo and in the raise's climb.
//   C4  DISPATCH: commit_trade_shipment raises a Hauler at the home pad and loads the
//       cargo: the home shelf loses the cargo, the kit and the tank; the home pad's
//       boost pays the climb and the lift (BL-1637); the treasury pays the space side;
//       the convoy rides aboard at speed 0.
//   C5  FLIGHT: the Hauler flies hop by hop, standing at every node of the cheapest
//       path; unloads at the moon's low orbit (descent propellant off the tank, BL-1637);
//       the convoy lands and the landing equals the cargo bought (conserved); it flies
//       home empty and the trip ends.
//   C6  REUSE: a second trade reuses the idle Hauler (no raise), refuelled over the pad.
//   C7  INTERDICTION: an armed unit of a nation at war at a node of the path destroys
//       an unescorted Hauler and its cargo: unit, record and convoy gone, nothing lands.
//   C8  ESCORT: with an armed unit of its own at that node the Hauler waits there;
//       once the war ends it flies on and delivers.
//   C9  HAND VERBS: load and unload move goods between a shelf and a low orbit at the
//       body's climb cost, conserved through the landing; every refusal mutates nothing.
//   C10 SAVE: mid-flight round-trip (equal hash, byte-identical); the v84 prefix plus
//       the section's measured bytes is the stream; a hauler-free world's v84 prefix
//       reads; tampered tails refuse the stream whole.
//   C11 DETERMINISM: two fixtures driven identically end with equal hash and stream.
//
// Build: node tools/verify/build_harness.js space_convoy_harness --run
// ---------------------------------------------------------------------------

#include "world/digitisation_tree_data.hpp"
#include "world/early_space_tree_data.hpp"
#include "world/economy_system.hpp"
#include "world/nation_tech.hpp"
#include "world/orbital_graph.hpp"
#include "world/recipe_registry.hpp"
#include "world/space_assets.hpp"
#include "world/space_convoy.hpp"
#include "world/space_tech_keys.hpp"
#include "world/space_units.hpp"
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
#include <set>
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

constexpr std::size_t PR = static_cast<std::size_t>(resource_type::propellant);
constexpr std::size_t SC = static_cast<std::size_t>(resource_type::spacecraft_components);
constexpr std::size_t ORE = static_cast<std::size_t>(resource_type::iron_ore);

struct fixture
{
    world     w;
    entity_id star = null_entity, home = null_entity, moon = null_entity, far = null_entity;
    entity_id a = null_entity, b = null_entity;
    entity_id home_tile = null_entity, moon_tile = null_entity, far_tile = null_entity;
    entity_id home_mkt = null_entity, moon_mkt = null_entity, far_mkt = null_entity;
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
    b.boost = 100000.0f; // BL-1637: banked, so a trip check is not also a boost check
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
    f.moon = add_body(w, "Moon", body_type::moon, f.home, 0.0026f, 0.012f, atmosphere_class::none); // STAGED
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
    f.home_mkt = add_market(w, f.home, f.home_tile);
    f.moon_mkt = add_market(w, f.moon, f.moon_tile);
    f.far_mkt  = add_market(w, f.far, f.far_tile);
    for (entity_id n : {f.a, f.b})
    {
        w.nations.at(n).capital_tile = f.home_tile;
        w.nations.at(n).treasury = 5000000.0f;
        w.nations.at(n).astronauts = 60;
    }
    f.home_pad = add_pad(w, f.a, f.home_tile, 1.0f);
    f.moon_pad = add_pad(w, f.a, f.moon_tile, 1.0f);
    grant_far_space(w, f.a);
    w.nation_tech[f.b];
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
    w.current_econ_tick = 1;
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
space_design frigate_d()
{
    return design_of({{sk::drive, "drive.nuclear_thermal"}, {sk::tanks, "tanks.extended"},
                      {sk::armament, "armament.kinetic"}, {sk::armour, "armour.whipple"}});
}

/// Stage an idle frigate of @p nation at @p node (no raise: a fixture's warship).
entity_id stage_frigate(world& w, entity_id nation, orbital_node node)
{
    const space_design d = frigate_d();
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

/// Walk @p n days; the convoys' share summed.
space_convoy_day_report days(world& w, const space_unit_params& p, int n,
                             std::vector<orbital_node>* stood = nullptr, entity_id watch = null_entity)
{
    const station_params stp{};
    space_convoy_day_report sum;
    for (int i = 0; i < n; ++i)
    {
        space_unit_day_report r;
        run_space_unit_day(w, p, stp, &r);
        sum.departed += r.convoy.departed;
        sum.unloaded += r.convoy.unloaded;
        sum.home     += r.convoy.home;
        sum.lost     += r.convoy.lost;
        sum.stopped  += r.convoy.stopped;
        sum.stranded += r.convoy.stranded;
        sum.waiting  += r.convoy.waiting;
        sum.cargo_lost += r.convoy.cargo_lost;
        if (stood != nullptr && watch != null_entity)
            if (const auto it = w.space_units.find(watch); it != w.space_units.end())
                if (stood->empty() || !(stood->back() == it->second.node))
                    stood->push_back(it->second.node);
    }
    return sum;
}

const convoy_component* convoy_of(const world& w, uint32_t id)
{
    for (const convoy_component& c : w.convoys)
        if (c.id == id)
            return &c;
    return nullptr;
}

/// One trade shipment of @p qty iron ore home -> moon by @p f.a; the units sent.
float ship(fixture& f, const recipe_registry& reg, float qty, convoy_leg* out_leg = nullptr)
{
    logistics_nodes nodes;
    const convoy_leg leg = price_trade_leg(f.w, reg, nodes, f.a, f.home_mkt, f.moon_mkt, ORE, qty);
    if (out_leg != nullptr)
        *out_leg = leg;
    economy_report report;
    float sent = 0.0f;
    if (!commit_trade_shipment(f.w, reg, report, f.a, f.home_mkt, f.moon_mkt, ORE, qty, leg, nullptr, &sent))
        return 0.0f;
    return sent;
}

struct snap
{
    std::uint64_t hash;
    std::size_t   units, hauls, convoys;
    std::uint32_t cursor, convoy_cursor;
    float ta, hp, ho, mp, mo;
};

snap take(const fixture& f)
{
    const world& w = f.w;
    return {w.state_hash(0), w.space_units.size(), w.space_hauls.size(), w.convoys.size(), w.next_entity_id(),
            w.next_convoy_id, w.nations.at(f.a).treasury, w.markets.at(f.home_mkt).inventory[PR],
            w.markets.at(f.home_mkt).inventory[ORE], w.markets.at(f.moon_mkt).inventory[PR],
            w.markets.at(f.moon_mkt).inventory[ORE]};
}

bool same(const snap& x, const snap& y)
{
    return x.hash == y.hash && x.units == y.units && x.hauls == y.hauls && x.convoys == y.convoys
        && x.cursor == y.cursor && x.convoy_cursor == y.convoy_cursor && x.ta == y.ta && x.hp == y.hp
        && x.ho == y.ho && x.mp == y.mp && x.mo == y.mo;
}

/// The scripted programme C11 runs twice: a trade dispatched, ten days flown, a hand load.
void programme(fixture& f, const recipe_registry& reg)
{
    world& w = f.w;
    const space_unit_params& p = reg.space_units();
    set_space_unit_design(w, f.a, space_hull::hauler, hauler_d());
    ship(f, reg, 1000.0f);
    days(w, p, 3);
    const space_unit_op h = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.home);
    load_space_cargo(w, p, f.a, h.unit, ORE, 10.0f);
    days(w, p, 2); // the trade still outbound
}

} // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const recipe_registry reg;
    const space_unit_params& p = reg.space_units();
    std::printf("space_convoy_harness — BL-1596\n");
    std::printf("dials: launch_propellant_per_dv %.3f  reservation_mult %.2f\n",
                p.launch_propellant_per_dv, p.reservation_mult);

    fixture f;
    build(f);
    world& w = f.w;
    const orbital_node home_low{f.home, orbital_node_kind::low_orbit};
    const orbital_node moon_low{f.moon, orbital_node_kind::low_orbit};

    // ---- C1 legacy lane -----------------------------------------------------
    std::printf("--- C1 legacy ---\n");
    {
        logistics_nodes nodes;
        const convoy_leg leg = price_trade_leg(w, reg, nodes, f.a, f.home_mkt, f.moon_mkt, ORE, 10.0f);
        check(!leg.hauler && (!leg.viable || leg.mode == convoy_mode::space) && !space_haul_eligible(w, f.a, f.a),
              "C1", fmt("no Hauler design: leg viable %d mode %d hauler %d (the straight-line lane)",
                        leg.viable ? 1 : 0, static_cast<int>(leg.mode), leg.hauler ? 1 : 0));
    }

    const space_unit_result dr = set_space_unit_design(w, f.a, space_hull::hauler, hauler_d());
    const space_design_stats hs = evaluate_space_design(space_hull::hauler, hauler_d());
    std::printf("hauler design: set %d; dry %.1f tank %.1f ve %.1f bay %.1f crew %d dv(empty, full) %.3f\n",
                static_cast<int>(dr), hs.dry_mass, hs.tank, hs.exhaust, hs.cargo, hs.crew, hs.dv_capacity);

    // ---- C2 quote -----------------------------------------------------------
    std::printf("--- C2 quote ---\n");
    space_haul_quote q0;
    {
        q0 = quote_space_haul(w, p, f.a, f.home_mkt, f.moon_mkt);
        const orbital_path_result out  = orbital_path(w, orbital_graph_params{}, home_low, moon_low);
        const orbital_path_result back = orbital_path(w, orbital_graph_params{}, moon_low, home_low);
        // Independently: the largest cargo c with burn_out(c) + descent(c) <= (P - R) x 0.999
        // (BL-1637: the descent burns the tank).
        const double ve = hs.exhaust, dry = hs.dry_mass, P = hs.tank;
        const double R = dry * (std::exp(back.dv / ve) - 1.0);
        const double d = orbital_climb_dv(w, orbital_graph_params{}, f.moon) * p.launch_propellant_per_dv;
        double lo = 0.0, hi = hs.cargo;
        for (int i = 0; i < 80; ++i)
        {
            const double mid = 0.5 * (lo + hi);
            const double burn = (dry + mid + P) * (1.0 - std::exp(-out.dv / ve)) + d * mid;
            (burn <= (P - R) * 0.999 ? lo : hi) = mid;
        }
        logistics_nodes nodes;
        const convoy_leg leg = price_trade_leg(w, reg, nodes, f.a, f.home_mkt, f.moon_mkt, ORE, 10.0f);
        std::printf("  path out dv %.3f (%zu nodes), back dv %.3f; return reserve %.3f; cap %.3f (bisect %.3f); "
                    "days %.1f; leg cost %.1f ticks %d\n",
                    out.dv, out.nodes.size(), back.dv, R, q0.cap, lo, q0.days, leg.cost, leg.travel_ticks);
        check(q0.viable && q0.unit == null_entity && std::fabs(q0.cap - lo) < 1.0e-3 && q0.cap > 0.0f
                  && q0.cap <= hs.cargo && leg.viable && leg.hauler && leg.mode == convoy_mode::space,
              "C2", fmt("Hauler leg: cap %.3f matches the rocket equation with the descent aboard (%.3f); raise quoted (no idle hull)",
                        q0.cap, lo));
        // A far planet with no pad: no descent, no trip.
        const space_haul_quote qf = quote_space_haul(w, p, f.a, f.home_mkt, f.far_mkt);
        check(!qf.viable, "C2b", "no staffed Launchpad on the destination body: not viable");
    }

    // ---- C3 climb -----------------------------------------------------------
    std::printf("--- C3 climb ---\n");
    {
        const space_haul_quote qm = quote_space_haul(w, p, f.a, f.moon_mkt, f.home_mkt);
        const float home_climb = static_cast<float>(orbital_climb_dv(w, orbital_graph_params{}, f.home));
        const float moon_climb = static_cast<float>(orbital_climb_dv(w, orbital_graph_params{}, f.moon));
        std::printf("  climb dv home %.3f moon %.3f; lift/unit home %.4f moon %.4f; raise climb boost home %.2f moon %.2f\n",
                    home_climb, moon_climb, q0.lift_per_unit, qm.lift_per_unit, q0.fixed_boost, qm.fixed_boost);
        check(qm.viable && qm.lift_per_unit < 0.25f * q0.lift_per_unit && qm.fixed_boost < q0.fixed_boost
                  && q0.fixed[PR] == hs.tank && qm.fixed[PR] == hs.tank
                  && std::fabs(q0.lift_per_unit - home_climb * p.launch_propellant_per_dv) < 1.0e-4f
                  && std::fabs(q0.drop_per_unit - moon_climb * p.launch_propellant_per_dv) < 1.0e-4f,
              "C3", fmt("a moon launch lifts a unit for %.4f boost, home %.4f (ratio %.3f); the raise's climb too; the shelf sells only the tank",
                        qm.lift_per_unit, q0.lift_per_unit, qm.lift_per_unit / q0.lift_per_unit));
    }

    // ---- C4 dispatch --------------------------------------------------------
    std::printf("--- C4 dispatch ---\n");
    entity_id hauler = null_entity;
    uint32_t  cid = 0;
    float     sent = 0.0f;
    float     home_ore0 = 0.0f, moon_prop0 = 0.0f;
    {
        const snap s0 = take(f);
        home_ore0 = s0.ho;
        const float sc0 = w.markets.at(f.home_mkt).inventory[SC];
        const float boost0 = space_pad_boost(w, f.home_pad);
        sent = ship(f, reg, 1000.0f);
        for (const auto& [id, h] : w.space_hauls)
            hauler = id, cid = h.convoy;
        const convoy_component* c = convoy_of(w, cid);
        const space_unit* u = hauler != null_entity ? &w.space_units.at(hauler) : nullptr;
        const float prop_drawn = s0.hp - w.markets.at(f.home_mkt).inventory[PR];
        const float expect_prop = hs.tank; // BL-1637: the climb and the lift are the pad's boost
        const float boost_spent = boost0 - space_pad_boost(w, f.home_pad);
        const float expect_boost = q0.lift_per_unit * (hs.dry_mass + hs.tank) + q0.lift_per_unit * sent;
        const float spent = s0.ta - w.nations.at(f.a).treasury;
        std::printf("  sent %.3f (cap %.3f); ore off the shelf %.3f; propellant drawn %.3f (expect %.3f); "
                    "components %.1f; treasury -%.1f, convoy cost_paid %.1f\n",
                    sent, q0.cap, s0.ho - w.markets.at(f.home_mkt).inventory[ORE], prop_drawn, expect_prop,
                    sc0 - w.markets.at(f.home_mkt).inventory[SC], spent, c ? c->cost_paid : -1.0f);
        check(sent > 0.0f && std::fabs(sent - q0.cap) < 1.0e-3f && u != nullptr && u->state == space_unit_state::idle
                  && u->node == home_low && u->cargo == sent && c != nullptr && c->speed == 0.0f && !c->arrived
                  && c->mode == convoy_mode::space && c->cargo_qty == sent
                  && std::fabs((s0.ho - w.markets.at(f.home_mkt).inventory[ORE]) - sent) < 1.0e-3f
                  && std::fabs(prop_drawn - expect_prop) < 1.0e-2f && std::fabs(boost_spent - expect_boost) < 1.0e-2f
                  && std::fabs(spent - c->cost_paid) < 0.5f
                  && w.space_hauls.at(hauler).phase == space_haul_phase::outbound,
              "C4", "a Hauler raised at the home pad and laden to its cap; the home shelf paid cargo, kit and tank; "
                    "the pad's boost the climb and the lift; the treasury the space side; the convoy rides aboard at speed 0");
    }

    // ---- C5 flight ----------------------------------------------------------
    std::printf("--- C5 flight ---\n");
    {
        moon_prop0 = w.markets.at(f.moon_mkt).inventory[PR];
        std::vector<orbital_node> stood;
        stood.push_back(w.space_units.at(hauler).node);
        int day = 0;
        space_convoy_day_report sum;
        while (day < 60)
        {
            const space_convoy_day_report r = days(w, p, 1, &stood, hauler);
            sum.departed += r.departed;
            sum.unloaded += r.unloaded;
            ++day;
            const convoy_component* c = convoy_of(w, cid);
            if (c != nullptr && c->arrived)
                break;
        }
        const orbital_path_result out = orbital_path(w, orbital_graph_params{}, home_low, moon_low);
        bool path_walked = stood.size() >= out.nodes.size();
        for (std::size_t i = 0; path_walked && i < out.nodes.size(); ++i)
            path_walked = stood[i] == out.nodes[i];
        const convoy_component* c = convoy_of(w, cid);
        const float drop = moon_prop0 - w.markets.at(f.moon_mkt).inventory[PR];
        // BL-1637: the descent burns the tank; the top-up over A's moon pad then refills
        // the outbound burn, laden, and the descent - all off the moon shelf.
        const float burn_out = static_cast<float>((hs.dry_mass + sent + hs.tank) * (1.0 - std::exp(-out.dv / hs.exhaust)));
        const float expect_drop = q0.drop_per_unit * sent + burn_out;
        std::printf("  unloaded on day %d (quoted %.1f); stood at %zu nodes (path %zu); hops %d; moon shelf propellant %.4f (descent %.4f + top-up %.4f)\n",
                    day, q0.days, stood.size(), out.nodes.size(), sum.departed, drop, q0.drop_per_unit * sent, burn_out);
        check(c != nullptr && c->arrived && path_walked && w.space_units.at(hauler).cargo == 0.0f
                  && std::fabs(drop - expect_drop) < 1.0e-2f
                  && day <= static_cast<int>(q0.days) + 1
                  && w.space_hauls.at(hauler).phase == space_haul_phase::returning,
              "C5a", "hop by hop along the cheapest path, standing at every node; unloaded at the moon's low orbit, "
                     "descent off the tank, then a top-up over the moon pad; the convoy released arrived");
        credit_arrived_convoys(w, 1);
        const float landed = w.landed(f.a, f.moon_mkt, ORE);
        check(std::fabs(landed - sent) < 1.0e-4f && std::fabs((home_ore0 - w.markets.at(f.home_mkt).inventory[ORE]) - landed) < 1.0e-3f
                  && convoy_of(w, cid) == nullptr,
              "C5b", fmt("conserved: %.3f bought at home, %.3f landed at the moon; the convoy retired", sent, landed));
        const space_convoy_day_report back = days(w, p, 40);
        const space_unit& u = w.space_units.at(hauler);
        std::printf("  home again: %d; propellant left %.3f\n", back.home, u.propellant);
        check(back.home == 1 && w.space_hauls.count(hauler) == 0 && u.state == space_unit_state::idle
                  && u.node == home_low && u.propellant >= 0.0f && back.stranded == 0,
              "C5c", "flew home empty on what the tank held; the trip ended idle at the home low orbit");
        w.landed_this_tick.clear();
    }

    // ---- C6 reuse -----------------------------------------------------------
    std::printf("--- C6 reuse ---\n");
    {
        const std::size_t units0 = w.space_units.size();
        const float prop_before = w.space_units.at(hauler).propellant;
        const float home_prop0 = w.markets.at(f.home_mkt).inventory[PR];
        const space_haul_quote q = quote_space_haul(w, p, f.a, f.home_mkt, f.moon_mkt);
        const float b0 = space_pad_boost(w, f.home_pad);
        const float s = ship(f, reg, 5.0f);
        const float drawn = home_prop0 - w.markets.at(f.home_mkt).inventory[PR];
        const float expect = hs.tank - prop_before; // BL-1637: the lift is boost
        const bool boost_ok = std::fabs((b0 - space_pad_boost(w, f.home_pad)) - q.lift_per_unit * s) < 1.0e-3f;
        check(q.unit == hauler && s == 5.0f && w.space_units.size() == units0 && w.space_hauls.count(hauler) == 1
                  && w.space_units.at(hauler).propellant == hs.tank && std::fabs(drawn - expect) < 1.0e-2f && boost_ok,
              "C6", fmt("the idle Hauler reused (no raise), refuelled over the pad: %.3f propellant (expect %.3f)", drawn, expect));
        days(w, p, 60);
        credit_arrived_convoys(w, 2);
        w.landed_this_tick.clear();
    }

    // ---- C7 interdiction ----------------------------------------------------
    std::printf("--- C7 interdiction ---\n");
    {
        const orbital_path_result out = orbital_path(w, orbital_graph_params{}, home_low, moon_low);
        const orbital_node mid = out.nodes[out.nodes.size() / 2];
        const entity_id raider = stage_frigate(w, f.b, mid);
        stage_war(w, f.b, f.a);
        const float s = ship(f, reg, 20.0f);
        uint32_t c7 = 0;
        for (const auto& [id, h] : w.space_hauls)
            if (id == hauler)
                c7 = h.convoy;
        const space_convoy_day_report r = days(w, p, 30);
        credit_arrived_convoys(w, 3);
        std::printf("  raider at node (body %u kind %d); sent %.1f; lost %d cargo %.1f\n", mid.body,
                    static_cast<int>(mid.kind), s, r.lost, r.cargo_lost);
        check(s > 0.0f && r.lost == 1 && std::fabs(r.cargo_lost - s) < 1.0e-4f && w.space_units.count(hauler) == 0
                  && w.space_hauls.empty() && convoy_of(w, c7) == nullptr && w.landed(f.a, f.moon_mkt, ORE) == 0.0f
                  && w.space_units.count(raider) == 1,
              "C7", "an unescorted Hauler meeting a warring armed unit is lost with its cargo; nothing lands");
        w.space_units.erase(raider);
        w.wars.clear();
        w.landed_this_tick.clear();
    }

    // ---- C8 escort ----------------------------------------------------------
    std::printf("--- C8 escort ---\n");
    {
        const orbital_path_result out = orbital_path(w, orbital_graph_params{}, home_low, moon_low);
        const orbital_node mid = out.nodes[out.nodes.size() / 2];
        const entity_id raider = stage_frigate(w, f.b, mid);
        const entity_id escort = stage_frigate(w, f.a, mid);
        stage_war(w, f.b, f.a);
        const float s = ship(f, reg, 20.0f);
        entity_id h8 = null_entity;
        for (const auto& [id, h] : w.space_hauls)
            h8 = id;
        const space_convoy_day_report r = days(w, p, 30);
        const bool waiting = w.space_units.count(h8) == 1 && w.space_units.at(h8).node == mid
                          && w.space_units.at(h8).state == space_unit_state::idle && r.stopped > 0 && r.lost == 0;
        w.wars.clear(); // peace: the battle lane's outcome stands in
        days(w, p, 30);
        credit_arrived_convoys(w, 4);
        const float landed = w.landed(f.a, f.moon_mkt, ORE);
        std::printf("  stopped %d days at the escorted node; after the peace landed %.1f of %.1f\n", r.stopped, landed, s);
        check(waiting && std::fabs(landed - s) < 1.0e-4f, "C8",
              "escorted, the Hauler waits at the contested node; at peace it flies on and delivers");
        w.space_units.erase(raider);
        w.space_units.erase(escort);
        days(w, p, 40);
        w.landed_this_tick.clear();
    }

    // ---- C9 hand verbs ------------------------------------------------------
    std::printf("--- C9 hand verbs ---\n");
    entity_id hand = null_entity;
    {
        const space_unit_op raised = raise_space_unit_at_pad(w, p, f.a, space_hull::hauler, f.home);
        hand = raised.unit;
        const snap s0 = take(f);
        const float lb0 = space_pad_boost(w, f.home_pad);
        const space_unit_op ld = load_space_cargo(w, p, f.a, hand, ORE, 12.0f);
        const float lift = static_cast<float>(orbital_climb_dv(w, orbital_graph_params{}, f.home)) * p.launch_propellant_per_dv * 12.0f;
        const float paid = s0.ta - w.nations.at(f.a).treasury;
        check(ld.result == space_unit_result::ok && w.space_units.at(hand).cargo == 12.0f
                  && std::fabs((s0.ho - w.markets.at(f.home_mkt).inventory[ORE]) - 12.0f) < 1.0e-4f
                  && s0.hp == w.markets.at(f.home_mkt).inventory[PR]
                  && std::fabs((lb0 - space_pad_boost(w, f.home_pad)) - lift) < 1.0e-3f && ld.climb_boost == lift
                  && std::fabs(paid - 12.0f * 50.0f) < 0.05f
                  && w.space_hauls.at(hand).phase == space_haul_phase::held,
              "C9a", fmt("load: 12 ore off the home shelf for %.1f; the %.2f lift off the pad's boost", paid, lift));

        // Refusals, each mutating nothing.
        bool all = true;
        std::string why;
        const auto refuse = [&](const char* what, space_unit_result want, auto&& fn) {
            const snap a0 = take(f);
            const space_unit_op op = fn();
            const bool ok = op.result == want && same(a0, take(f));
            all = all && ok;
            if (!ok)
                why += fmt(" %s(%d)", what, static_cast<int>(op.result));
        };
        const entity_id frig = stage_frigate(w, f.a, home_low);
        refuse("over-bay", space_unit_result::rejected_cargo, [&] { return load_space_cargo(w, p, f.a, hand, ORE, 1000.0f); });
        refuse("other-good", space_unit_result::rejected_cargo, [&] { return load_space_cargo(w, p, f.a, hand, PR, 1.0f); });
        refuse("not-carrier", space_unit_result::rejected_cargo, [&] { return load_space_cargo(w, p, f.a, frig, ORE, 1.0f); });
        refuse("unknown", space_unit_result::rejected_unknown_unit, [&] { return load_space_cargo(w, p, f.a, 0x00FFFFFFu, ORE, 1.0f); });
        refuse("not-owner", space_unit_result::rejected_not_owner, [&] { return load_space_cargo(w, p, f.b, hand, ORE, 1.0f); });
        refuse("nan", space_unit_result::rejected_invalid, [&] { return load_space_cargo(w, p, f.a, hand, ORE, std::nanf("")); });
        refuse("bad-good", space_unit_result::rejected_invalid, [&] { return load_space_cargo(w, p, f.a, hand, resource_count, 1.0f); });
        refuse("neg", space_unit_result::rejected_invalid, [&] { return load_space_cargo(w, p, f.a, hand, ORE, -1.0f); });
        {
            const float keep = w.markets.at(f.home_mkt).inventory[ORE];
            w.markets.at(f.home_mkt).inventory[ORE] = 1.0f;
            refuse("shelf", space_unit_result::rejected_shelf_short, [&] { return load_space_cargo(w, p, f.a, hand, ORE, 5.0f); });
            w.markets.at(f.home_mkt).inventory[ORE] = keep;
            const float t = w.nations.at(f.a).treasury;
            w.nations.at(f.a).treasury = 10.0f;
            refuse("treasury", space_unit_result::rejected_insufficient_treasury, [&] { return load_space_cargo(w, p, f.a, hand, ORE, 5.0f); });
            w.nations.at(f.a).treasury = t;
            const float bk = w.buildings.at(f.home_pad).boost;
            w.buildings.at(f.home_pad).boost = 0.0f;
            refuse("boost", space_unit_result::rejected_boost_short, [&] { return load_space_cargo(w, p, f.a, hand, ORE, 5.0f); });
            w.buildings.at(f.home_pad).boost = bk;
            w.buildings.at(f.home_pad).workforce_assigned = 0.0f;
            refuse("no-pad", space_unit_result::rejected_no_pad, [&] { return load_space_cargo(w, p, f.a, hand, ORE, 5.0f); });
            w.buildings.at(f.home_pad).workforce_assigned = 1.0f;
            const entity_id raider = stage_frigate(w, f.b, home_low);
            stage_war(w, f.b, f.a);
            refuse("hostile-load", space_unit_result::rejected_hostile, [&] { return load_space_cargo(w, p, f.a, hand, ORE, 5.0f); });
            refuse("hostile-unload", space_unit_result::rejected_hostile, [&] { return unload_space_cargo(w, p, f.a, hand); });
            w.space_units.erase(raider);
            w.wars.clear();
        }
        refuse("nothing-aboard", space_unit_result::rejected_cargo, [&] { return unload_space_cargo(w, p, f.a, frig); });
        w.space_units.erase(frig);
        check(all, "C9b", "load/unload refusals (bay, good, carrier, unknown, owner, nan, range, negative, shelf, "
                          "treasury, boost, pad, hostile x2, nothing aboard) mutate nothing" + (why.empty() ? std::string() : " — FAILED:" + why));

        // Fly it to the moon by order and set it down there.
        const space_unit_op mv = order_space_unit(w, p, f.a, hand, moon_low);
        days(w, p, 20);
        const snap s1 = take(f);
        const float tank0 = w.space_units.at(hand).propellant;
        const space_unit_op ul = unload_space_cargo(w, p, f.a, hand);
        const float drop = static_cast<float>(orbital_climb_dv(w, orbital_graph_params{}, f.moon)) * p.launch_propellant_per_dv * 12.0f;
        const float burned = tank0 - w.space_units.at(hand).propellant;
        credit_arrived_convoys(w, 5);
        std::printf("  move %d unload %d cargo %.2f record %zu moon propellant -%.4f landed %.3f\n",
                    static_cast<int>(mv.result), static_cast<int>(ul.result), w.space_units.at(hand).cargo,
                    w.space_hauls.count(hand), s1.mp - w.markets.at(f.moon_mkt).inventory[PR], w.landed(f.a, f.moon_mkt, ORE));
        check(mv.result == space_unit_result::ok && ul.result == space_unit_result::ok && w.space_units.at(hand).cargo == 0.0f
                  && w.space_hauls.count(hand) == 0 && s1.mp == w.markets.at(f.moon_mkt).inventory[PR]
                  && std::fabs(burned - drop) < 1.0e-3f && ul.credits == 0.0f
                  && std::fabs(w.landed(f.a, f.moon_mkt, ORE) - 12.0f) < 1.0e-4f,
              "C9c", fmt("unload at the moon: %.4f descent propellant off the tank, the moon shelf untouched; 12 ore landed (conserved)", drop));
        w.landed_this_tick.clear();
        // Setting down where no pad stands below: the far planet's low orbit (staged there).
        load_space_cargo(w, p, f.a, hand, ORE, 2.0f);
        w.space_units.at(hand).node = w.space_units.at(hand).dest = {f.far, orbital_node_kind::low_orbit};
        const snap s2 = take(f);
        const space_unit_op nopad = unload_space_cargo(w, p, f.a, hand);
        check(nopad.result == space_unit_result::rejected_no_pad && same(s2, take(f)), "C9d",
              fmt("no Launchpad on the body below: unload refused (%d), nothing written", static_cast<int>(nopad.result)));
    }

    // ---- C10 save -----------------------------------------------------------
    std::printf("--- C10 save ---\n");
    {
        fixture g;
        build(g);
        programme(g, reg);
        world& x = g.w;
        const std::string b = to_bytes(x);
        world x2;
        const bool ok = from_bytes(b, x2);
        const std::string b2 = ok ? to_bytes(x2) : std::string();
        int phases[3] = {0, 0, 0};
        for (const auto& [id, h] : x.space_hauls)
            ++phases[static_cast<int>(h.phase)];
        check(ok && b2 == b && x2.state_hash(0) == x.state_hash(0) && x2.space_hauls.size() == x.space_hauls.size()
                  && phases[0] > 0 && phases[1] > 0,
              "C10a", fmt("round-trip mid-flight: %zu records (held %d, outbound %d, returning %d); equal hash, byte-identical",
                          x.space_hauls.size(), phases[0], phases[1], phases[2]));
        const std::size_t sec = world_save_space_convoy_section_bytes(x);
        std::ostringstream o84(std::ios::binary);
        write_world_snapshot_pre_space_convoys(x, o84);
        check(o84.str().size() + world_save_space_line_gap_bytes(x) /* the space merge: main's naval..missile sections */ + sec + (world_save_debris_section_bytes(x) + (world_save_space_bombard_section_bytes(x) + (world_save_offworld_base_section_bytes(x) + (world_save_space_firsts_section_bytes(x) + world_save_transit_origin_section_bytes(x) + world_save_launch_boost_section_bytes(x) + world_save_province_names_section_bytes(x) /* BL-1708 (v97) */)))) == b.size() && sec == 4 + 32 * x.space_hauls.size(), "C10b", // BL-1598 (v88) follows
              fmt("section %zu bytes; v84 prefix %zu + %zu == %zu", sec, o84.str().size(), sec, b.size()));
        fixture e;
        build(e);
        for (auto& [bid, bb] : e.w.buildings) // BL-1637: a v84 stream predates the boost
            bb.boost = 0.0f;
        std::ostringstream oe(std::ios::binary);
        write_world_snapshot_pre_space_convoys(e.w, oe);
        world we;
        check(from_bytes(oe.str(), we) && we.space_hauls.empty() && we.state_hash(0) == e.w.state_hash(0)
                  && world_save_space_convoy_section_bytes(e.w) == 4,
              "C10c", "a hauler-free world's v84 prefix reads with nothing aboard; equal hash");

        const std::size_t s0 = b.size() - sec - (world_save_debris_section_bytes(x) + (world_save_space_bombard_section_bytes(x) + (world_save_offworld_base_section_bytes(x) + (world_save_space_firsts_section_bytes(x) + world_save_transit_origin_section_bytes(x) + world_save_launch_boost_section_bytes(x) + world_save_province_names_section_bytes(x) /* BL-1708 (v97) */)))) + 4; // the first record (BL-1598's v88 section follows)
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
        bool all = true;
        std::string why;
        const auto expect = [&](bool r, const char* what) { all = all && r; if (!r) { why += " "; why += what; } };
        const uint8_t bad_phase = 9, bad_good = 200, star_kind = 9;
        const uint32_t ghost = 0x00FFFFFFu;
        // Find an outbound and a held record's offsets.
        std::size_t at = s0, out_at = 0, held_at = 0;
        for (const auto& [id, h] : x.space_hauls)
        {
            if (h.phase == space_haul_phase::outbound && out_at == 0) out_at = at;
            if (h.phase == space_haul_phase::held && held_at == 0) held_at = at;
            at += 32;
        }
        expect(refused(poke(s0 + 8, &bad_phase, 1)), "phase");
        expect(refused(poke(s0 + 9, &bad_good, 1)), "good");
        expect(refused(poke(s0 + 4, &ghost, 4)), "owner");
        expect(refused(poke(s0 + 0, &ghost, 4)), "unit");
        expect(refused(poke(s0 + 10, &ghost, 4)), "src-market");
        expect(refused(poke(s0 + 26, &star_kind, 1)), "target-kind");
        expect(out_at != 0 && refused(poke(out_at + 14, &ghost, 4)), "outbound-convoy");
        expect(out_at != 0 && refused(poke(out_at + 18, &ghost, 4)), "outbound-dest");
        {
            const uint8_t held = 0;
            expect(out_at != 0 && refused(poke(out_at + 8, &held, 1)), "outbound-as-held");
        }
        expect(held_at != 0 && refused(poke(held_at + 14, &ghost, 4)), "held-convoy");
        expect(refused(b.substr(0, b.size() - 1)), "truncated");
        check(all, "C10d", "tampered tails refused whole (phase, good, owner, unit, source, node kind, outbound convoy/"
                           "destination, phase flip, held convoy, truncation)" + (why.empty() ? std::string() : " — FAILED:" + why));
    }

    // ---- C11 determinism ----------------------------------------------------
    std::printf("--- C11 determinism ---\n");
    {
        fixture x, y;
        build(x);
        build(y);
        programme(x, reg);
        programme(y, reg);
        check(x.w.state_hash(0) == y.w.state_hash(0) && to_bytes(x.w) == to_bytes(y.w) && !x.w.space_hauls.empty(),
              "C11", fmt("two runs: equal hash %016llx, identical stream, %zu records",
                         static_cast<unsigned long long>(x.w.state_hash(0)), x.w.space_hauls.size()));
    }

    std::printf("\nspace_convoy_harness: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
