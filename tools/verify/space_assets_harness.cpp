// ---------------------------------------------------------------------------
// space_assets_harness — BL-1327 (space asset model), the headless proof
// ---------------------------------------------------------------------------
// docs/economy/SPACE_ASSETS.md § Era 1 near space (Ben, 2026-10-15). A hand-
// built fixture: a star, a home planet at 1.0 AU, its moon, a far planet at
// 1.5 AU, and two nations on the home planet. Rows:
//
//   A1  LAUNCH: a legal launch debits the treasury exactly the quote, allocates
//       one id, stores the asset in transit with arrival = launch + travel.
//   A2  REACH: further is costlier and slower (home orbit < moon orbit <
//       moon surface; far orbit costs the per-AU term and its extra quarters).
//   A3  ARRIVAL: the step leaves an asset in transit before its arrival tick and
//       makes it active on it; no upkeep is drawn in transit. The tick contract:
//       the launch tick is w.current_econ_tick, and travel 0 vs travel 1 differ by
//       exactly one quarter; the step reports the quarter's launches and cost.
//   A4  UPKEEP: an active asset pays its upkeep whole each quarter.
//   A5  DECAY: an unpaid quarter makes it decaying; a paid one restores it;
//       quarters_to_loss consecutive unpaid quarters lose it; a lost asset never
//       steps or pays again.
//   A6  REJECTION MUTATES NOTHING: unaffordable, illegal pairs, the home surface,
//       the star, an unknown nation/body, an out-of-range class — each refused
//       with state_hash, treasury, the store and the allocator cursor unchanged.
//   A7  SAVE: a store holding decaying, lost and in-transit assets round-trips with an equal state_hash
//       and re-serialises byte for byte; a v50 stream (v51 less its section)
//       reads with no assets, and a SET share rule in a v50 stream survives; a
//       tampered owner, an illegal slot, the star, a state/unpaid mismatch, an
//       unpaid count past the ceiling or an id another entity owns each refuse
//       the stream whole.
//   A8  WIRING: run_nation_step steps the store (an asset arrives through it) and
//       files the launch and upkeep debit on economy_report::space_assets; an
//       empty store hashes exactly as a world that never had one.
//   A9  DETERMINISM: two independently built fixtures driven identically end
//       with equal state_hash.
//
// BL-1581 / BL-1582 / BL-1588 / BL-1587 (sprint 65). The fixture now stages a
// nation that MAY launch (space_fixture.hpp: every node held, a staffed pad, a
// stocked home shelf), destinations are orbital nodes, and A2 prices off the
// orbital path. New rows:
//
//   A10 SLOTS: a full node refuses a launch whole; in transit holds a slot; a
//       lost asset frees its slot at once; crewed missions hold none.
//   A11 GATE: no Rocketry, a missing role key, no / unstaffed / unfinished /
//       off-body pad, an empty or short shelf, no home market, a short treasury
//       — each refused with nothing touched (hash, treasuries, shelf, store).
//   A12 GOODS: a launch draws the shelf and debits the treasury the same priced lot.
//   A13 SEAM + SIGNAL DELAY: apply_space_command applies in coverage, queues
//       outside it; the queued order waits exactly one quarter, lands, and a
//       no-longer-legal one is dropped and reported; a move frees and takes
//       slots and buys only propellant; every malformed field refuses whole.
//   A14 SAVE v83: node, role and pending orders round-trip; tampered tails
//       refuse; the v81 prefix reads with derived nodes and no queue.
//
// Build: node tools/verify/build_harness.js space_assets_harness
// Run:   build_gen/verify/space_assets_harness.exe
// ---------------------------------------------------------------------------

#include "world/economy_system.hpp"
#include "world/nation_step.hpp"
#include "world/recipe_registry.hpp"
#include "world/orbital_graph.hpp"
#include "world/space_assets.hpp"
#include "world/space_command.hpp"
#include "world/space_tech_keys.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include "space_fixture.hpp"

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
    char buf[600];
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

struct fixture
{
    world     w;
    entity_id star = null_entity, home = null_entity, moon = null_entity, far = null_entity;
    entity_id a = null_entity, b = null_entity; // two nations
    entity_id market = null_entity;             // the home shelf (BL-1582)
    entity_id pad_a = null_entity, pad_b = null_entity;
};

// BL-1581: a destination is an orbital node.
orbital_node LO(entity_id b)   { return {b, orbital_node_kind::low_orbit}; }
orbital_node HO(entity_id b)   { return {b, orbital_node_kind::high_orbit}; }
orbital_node SURF(entity_id b) { return {b, orbital_node_kind::surface}; }

// A satellite launches with a role (comms); every other class with none.
space_satellite_role role_for(space_asset_class c)
{
    return c == space_asset_class::satellite ? space_satellite_role::comms : space_satellite_role::none;
}
space_launch_quote Q(const world& w, const space_asset_params& p, entity_id n, space_asset_class c, orbital_node d)
{
    return quote_space_launch(w, p, n, c, d, role_for(c));
}
space_launch_quote L(world& w, const space_asset_params& p, entity_id n, space_asset_class c, orbital_node d)
{
    return launch_space_asset(w, p, n, c, d, role_for(c));
}

entity_id add_body(world& w, const char* name, body_type t, entity_id parent, float au)
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
    w.bodies.emplace(id, bc);
    return id;
}

void build(fixture& f)
{
    world& w = f.w;
    f.star = add_body(w, "Star", body_type::star, null_entity, 0.0f);
    f.home = add_body(w, "Home", body_type::planet, null_entity, 1.0f);
    f.moon = add_body(w, "Moon", body_type::moon, f.home, 0.0026f);
    f.far  = add_body(w, "Far", body_type::planet, null_entity, 1.5f);
    w.star_body = f.star;
    w.home_body = f.home;
    for (entity_id* n : {&f.a, &f.b})
    {
        *n = w.create_entity();
        nation_component nc;
        nc.name = (n == &f.a) ? "Alpha" : "Beta";
        nc.treasury = 0.0f;
        w.nations.emplace(*n, nc);
    }
    w.nations.at(f.a).treasury = 200000.0f;
    w.nations.at(f.b).treasury = 5000.0f;
    // BL-1581: the home world one Earth mass under moderate air (climb ~10 Δv);
    // the moon a Luna-mass airless body (climb ~1.7).
    w.bodies.at(f.home).mass_earths = 1.0f;
    w.bodies.at(f.home).atmosphere  = atmosphere_class::moderate;
    w.bodies.at(f.moon).mass_earths = 0.0123f;
    w.bodies.at(f.moon).atmosphere  = atmosphere_class::none;
    // BL-1582: both may launch — every node held, a staffed pad each, a stocked shelf.
    space_fixture::hold_every_node(w, f.a);
    space_fixture::hold_every_node(w, f.b);
    const entity_id tile = space_fixture::add_tile(w, f.home);
    f.pad_a  = space_fixture::add_pad(w, f.a, tile, 10.0f);
    f.pad_b  = space_fixture::add_pad(w, f.b, tile, 10.0f);
    f.market = space_fixture::add_shelf(w, f.home);
}

struct snap
{
    std::uint64_t hash;
    std::size_t   assets;
    std::uint32_t cursor;
    float         ta, tb;
    float         comp, prop; // BL-1582: the home shelf's launch goods
};

snap take(const fixture& f)
{
    return {f.w.state_hash(0), f.w.space_assets.size(), f.w.next_entity_id(),
            f.w.nations.at(f.a).treasury, f.w.nations.at(f.b).treasury,
            space_fixture::shelf_of(f.w, f.market, resource_type::spacecraft_components),
            space_fixture::shelf_of(f.w, f.market, resource_type::propellant)};
}

bool same(const snap& x, const snap& y)
{
    return x.hash == y.hash && x.assets == y.assets && x.cursor == y.cursor && x.ta == y.ta
        && x.tb == y.tb && x.comp == y.comp && x.prop == y.prop;
}

const char* state_name(space_asset_state s)
{
    switch (s)
    {
        case space_asset_state::in_transit: return "in_transit";
        case space_asset_state::active:     return "active";
        case space_asset_state::decaying:   return "decaying";
        case space_asset_state::lost:       return "lost";
    }
    return "?";
}

} // namespace

int main()
{
    const space_asset_params p{};
    std::printf("space_assets_harness — BL-1327 + BL-1581 + BL-1582 + BL-1588\n");
    std::printf("calibration: base sat %.0f rover %.0f crewed %.0f module %.0f core %.0f; upkeep %.0f/%.0f/%.0f/%.0f/%.0f;\n"
                "             %.0f credits/dv; components %.0f/%.0f/%.0f/%.0f/%.0f; propellant/dv %.0f/%.0f/%.0f/%.0f/%.0f;\n"
                "             floor q %d/%d/%d/%d/%d (+%.1f/AU far); %.0f days/quarter; loss at %d unpaid\n",
                p.launch_cost[0], p.launch_cost[1], p.launch_cost[2], p.launch_cost[3], p.launch_cost[4],
                p.upkeep[0], p.upkeep[1], p.upkeep[2], p.upkeep[3], p.upkeep[4], p.credits_per_dv,
                p.components_lot[0], p.components_lot[1], p.components_lot[2], p.components_lot[3], p.components_lot[4],
                p.propellant_per_dv[0], p.propellant_per_dv[1], p.propellant_per_dv[2], p.propellant_per_dv[3], p.propellant_per_dv[4],
                p.travel_quarters[0], p.travel_quarters[1], p.travel_quarters[2],
                p.travel_quarters[3], p.travel_quarters[4], p.far_quarters_per_au, p.days_per_quarter,
                p.quarters_to_loss);

    fixture f;
    build(f);
    world& w = f.w;

    // ---- A2 reach ladder (quotes are pure) ---------------------------------
    std::printf("--- A2 reach: the path prices the launch ---\n");
    {
        const space_launch_quote ho = Q(w, p, f.a, space_asset_class::satellite, LO(f.home));
        const space_launch_quote hh = Q(w, p, f.a, space_asset_class::satellite, HO(f.home));
        const space_launch_quote mo = Q(w, p, f.a, space_asset_class::satellite, LO(f.moon));
        const space_launch_quote ms = Q(w, p, f.a, space_asset_class::rover, SURF(f.moon));
        const space_launch_quote fo = Q(w, p, f.a, space_asset_class::satellite, LO(f.far));
        const space_launch_quote fs = Q(w, p, f.a, space_asset_class::rover, SURF(f.far));
        std::printf("    home LO sat %.0f (%.2f dv)/%dq  home HO sat %.0f (%.2f dv)  moon LO sat %.0f (%.2f dv)/%dq  moon surface rover %.0f/%dq  far LO sat %.0f (%.2f dv)/%dq  far surface rover %.0f/%dq\n",
                    ho.cost, ho.dv, ho.travel_quarters, hh.cost, hh.dv, mo.cost, mo.dv, mo.travel_quarters, ms.cost,
                    ms.travel_quarters, fo.cost, fo.dv, fo.travel_quarters, fs.cost, fs.travel_quarters);
        check(ho.result == space_launch_result::launched && mo.result == space_launch_result::launched
                  && ms.result == space_launch_result::launched && fo.result == space_launch_result::launched
                  && fs.result == space_launch_result::launched,
              "A2", "every legal destination quotes (the gate is open in the fixture)");
        check(ho.reach == space_reach::home_orbit && mo.reach == space_reach::moon_orbit
                  && ms.reach == space_reach::moon_surface && fo.reach == space_reach::far_orbit
                  && fs.reach == space_reach::far_surface && hh.reach == space_reach::home_orbit,
              "A2", "each node resolves to its reach tier (the travel floor)");
        const orbital_path_result pm = orbital_path(w, orbital_graph_params{}, SURF(f.home), LO(f.moon));
        check(ho.cost < hh.cost && hh.cost < mo.cost && mo.dv == pm.dv && ho.dv > 0.0
                  && mo.propellant > ho.propellant && ho.travel_quarters <= mo.travel_quarters
                  && mo.cost < fo.cost && mo.travel_quarters < fo.travel_quarters,
              "A2", fmt("a moon node costs more than home low orbit (%.0f > %.0f; %.2f dv > %.2f, the path's own), and burns more propellant; far costs more again",
                        mo.cost, ho.cost, mo.dv, ho.dv));
        const float expect = p.launch_cost[0] + p.credits_per_dv * static_cast<float>(ho.dv)
                           + p.components_lot[0] * space_fixture::k_components_price
                           + p.propellant_per_dv[0] * static_cast<float>(ho.dv) * space_fixture::k_propellant_price;
        check(std::fabs(ho.cost - expect) <= 1e-3f * expect && ho.cost == ho.credits + ho.goods_value
                  && ho.market == f.market,
              "A2", fmt("home LO = base + credits/dv x dv + components x price + propellant/dv x dv x price (%.0f), off the home shelf", expect));
        check(ho.travel_quarters == 0 && mo.travel_quarters == p.travel_quarters[1] && fo.travel_quarters == p.travel_quarters[3]
                  && fs.travel_quarters == p.travel_quarters[4],
              "A2", "travel never undercuts the old per-reach floor (home 0, moon 1, far 2, far surface 3)");
        // A further far body: 3.0 AU away from home.
        const entity_id outer = add_body(w, "Outer", body_type::planet, null_entity, 4.0f);
        const space_launch_quote oo = Q(w, p, f.a, space_asset_class::satellite, LO(outer));
        check(oo.cost > fo.cost && oo.travel_quarters == p.travel_quarters[3] + 3,
              "A2", fmt("a body 3 AU out costs more (%.0f) and takes 3 more floor quarters (%d)", oo.cost, oo.travel_quarters));
    }

    // ---- A6 rejection mutates nothing --------------------------------------
    std::printf("--- A6 rejection ---\n");
    {
        struct reject_case { const char* what; entity_id nation; space_asset_class cls; orbital_node d; space_satellite_role role; space_launch_result want; };
        const auto C = space_satellite_role::comms;
        const auto N = space_satellite_role::none;
        const reject_case cases[] = {
            {"unaffordable (Beta, 5000, crewed to the moon)", f.b, space_asset_class::crewed_mission, SURF(f.moon), N, space_launch_result::rejected_insufficient_treasury},
            {"satellite to a surface", f.a, space_asset_class::satellite, SURF(f.moon), C, space_launch_result::rejected_illegal_destination},
            {"rover to an orbit", f.a, space_asset_class::rover, LO(f.moon), N, space_launch_result::rejected_illegal_destination},
            {"crewed mission to the home surface", f.a, space_asset_class::crewed_mission, SURF(f.home), N, space_launch_result::rejected_illegal_destination},
            {"station core to a surface", f.a, space_asset_class::station_core, SURF(f.moon), N, space_launch_result::rejected_illegal_destination},
            {"anything to the home surface", f.a, space_asset_class::rover, SURF(f.home), N, space_launch_result::rejected_illegal_destination},
            {"anything to the star", f.a, space_asset_class::satellite, LO(f.star), C, space_launch_result::rejected_illegal_destination},
            {"an unknown nation", 999999u, space_asset_class::satellite, LO(f.home), C, space_launch_result::rejected_unknown_nation},
            {"a body as nation", f.home, space_asset_class::satellite, LO(f.home), C, space_launch_result::rejected_unknown_nation},
            {"an unknown body", f.a, space_asset_class::satellite, LO(999999u), C, space_launch_result::rejected_unknown_body},
            {"an out-of-range class", f.a, static_cast<space_asset_class>(7), LO(f.home), N, space_launch_result::rejected_invalid},
            {"an out-of-range node kind", f.a, space_asset_class::satellite, {f.home, static_cast<orbital_node_kind>(9)}, C, space_launch_result::rejected_invalid},
            {"an out-of-range role", f.a, space_asset_class::satellite, LO(f.home), static_cast<space_satellite_role>(9), space_launch_result::rejected_invalid},
            {"a satellite with no role", f.a, space_asset_class::satellite, LO(f.home), N, space_launch_result::rejected_invalid},
            {"a rover with a role", f.a, space_asset_class::rover, SURF(f.moon), C, space_launch_result::rejected_invalid},
        };
        for (const reject_case& c : cases)
        {
            const snap before = take(f);
            w.current_econ_tick = 3;
            const space_launch_quote q = launch_space_asset(w, p, c.nation, c.cls, c.d, c.role);
            const snap after = take(f);
            check(q.result == c.want && q.asset == null_entity && same(before, after), "A6",
                  fmt("%s: refused (code %d), hash/treasuries/shelf/store/cursor unchanged", c.what, static_cast<int>(q.result)));
        }
        // The boundary: a treasury exactly equal to the cost is affordable; one
        // credit short is not.
        const float keep = w.nations.at(f.b).treasury;
        const space_launch_quote q0 = Q(w, p, f.a, space_asset_class::satellite, LO(f.home));
        w.nations.at(f.b).treasury = q0.cost;
        const space_launch_quote qe = Q(w, p, f.b, space_asset_class::satellite, LO(f.home));
        w.nations.at(f.b).treasury = q0.cost - 1.0f;
        const space_launch_quote qs = Q(w, p, f.b, space_asset_class::satellite, LO(f.home));
        w.nations.at(f.b).treasury = keep;
        check(qe.result == space_launch_result::launched
                  && qs.result == space_launch_result::rejected_insufficient_treasury,
              "A6", fmt("treasury == cost (%.0f) is affordable; one credit short is refused", q0.cost));
    }

    // ---- A1 launch ---------------------------------------------------------
    std::printf("--- A1 launch ---\n");
    entity_id sat = null_entity, rov = null_entity;
    {
        const int t0 = 10;
        const float before = w.nations.at(f.a).treasury;
        const std::uint32_t cursor = w.next_entity_id();
        const space_launch_quote qq = Q(w, p, f.a, space_asset_class::satellite, LO(f.moon));
        const snap s0 = take(f);
        w.current_econ_tick = t0; // the tick about to run: the launch tick
        const space_launch_quote q = L(w, p, f.a, space_asset_class::satellite, LO(f.moon));
        sat = q.asset;
        const auto it = w.space_assets.find(sat);
        {
            const snap s1 = take(f);
            const float price = qq.components * space_fixture::k_components_price
                              + qq.propellant * space_fixture::k_propellant_price;
            check(s1.comp == s0.comp - qq.components && s1.prop == s0.prop - qq.propellant
                      && qq.components == p.components_lot[0] && qq.propellant > 0.0f
                      && std::fabs(qq.goods_value - price) <= 1e-3f * price
                      && s1.ta == s0.ta - qq.cost && qq.cost == qq.credits + qq.goods_value,
                  "A12", fmt("the launch draws the home shelf (%.0f components, %.1f propellant) and debits the treasury the SAME priced lot (%.0f goods + %.0f credits)",
                             qq.components, qq.propellant, qq.goods_value, qq.credits));
        }
        check(q.result == space_launch_result::launched && it != w.space_assets.end()
                  && w.nations.at(f.a).treasury == before - qq.cost && sat == cursor
                  && w.next_entity_id() == cursor + 1,
              "A1", fmt("a satellite to moon orbit launches: treasury %.0f -> %.0f (cost %.0f), id %u from the allocator",
                        before, w.nations.at(f.a).treasury, qq.cost, static_cast<unsigned>(sat)));
        if (it != w.space_assets.end())
        {
            const space_asset& a = it->second;
            check(a.owner == f.a && a.cls == space_asset_class::satellite && a.dest.body == f.moon
                      && a.dest.slot == space_slot::orbit && a.state == space_asset_state::in_transit
                      && a.launch_tick == t0 && a.arrival_tick == t0 + qq.travel_quarters
                      && a.upkeep == p.upkeep[0] && a.unpaid_quarters == 0
                      && a.node == LO(f.moon) && a.role == space_satellite_role::comms,
                  "A1", fmt("record: owner, class, destination, in_transit, launch %d, arrival %d, upkeep %.0f",
                            a.launch_tick, a.arrival_tick, a.upkeep));
        }
        const space_launch_quote r = L(w, p, f.a, space_asset_class::rover, SURF(f.far));
        rov = r.asset;
        check(r.result == space_launch_result::launched && w.space_assets.at(rov).arrival_tick == t0 + r.travel_quarters
                  && r.travel_quarters == p.travel_quarters[4],
              "A1", fmt("a rover to the far surface arrives after %d quarters", r.travel_quarters));
    }

    // ---- A3/A4 arrival and upkeep ------------------------------------------
    std::printf("--- A3/A4 arrival and upkeep ---\n");
    {
        const float t_before = w.nations.at(f.a).treasury;
        space_asset_step_report r;
        run_space_asset_step(w, p, 10, &r); // launch quarter: the sat (1q) is still travelling
        check(w.space_assets.at(sat).state == space_asset_state::in_transit && r.arrived == 0
                  && w.nations.at(f.a).treasury == t_before && r.upkeep_paid == 0.0f,
              "A3", "before its arrival tick the asset is in transit and draws no upkeep");
        run_space_asset_step(w, p, 11, &r);
        check(w.space_assets.at(sat).state == space_asset_state::active && r.arrived == 1
                  && w.space_assets.at(rov).state == space_asset_state::in_transit,
              "A3", "on its arrival tick it becomes active; the rover (3q) is still travelling");
        check(w.nations.at(f.a).treasury == t_before - p.upkeep[0] && r.paid == 1,
              "A4", fmt("the arrived satellite pays its quarter whole: %.0f", p.upkeep[0]));
        const float t1 = w.nations.at(f.a).treasury;
        run_space_asset_step(w, p, 12, &r);
        run_space_asset_step(w, p, 13, &r); // rover arrives
        check(w.space_assets.at(rov).state == space_asset_state::active
                  && w.nations.at(f.a).treasury == t1 - 2 * p.upkeep[0] - p.upkeep[1],
              "A4", fmt("two more quarters: sat x2 + the arrived rover x1 = %.0f debited", 2 * p.upkeep[0] + p.upkeep[1]));
    }

    // ---- A3b the tick contract ---------------------------------------------
    std::printf("--- A3b tick contract ---\n");
    {
        fixture k;
        build(k);
        k.w.current_econ_tick = 5;
        const space_launch_quote q0 = L(k.w, p, k.a, space_asset_class::satellite, LO(k.home));
        const space_launch_quote q1 = L(k.w, p, k.a, space_asset_class::satellite, LO(k.moon));
        const space_asset& a0 = k.w.space_assets.at(q0.asset);
        const space_asset& a1 = k.w.space_assets.at(q1.asset);
        check(a0.launch_tick == 5 && a1.launch_tick == 5 && q0.travel_quarters == 0 && q1.travel_quarters == 1
                  && a0.launch_cost == q0.cost && a1.launch_cost == q1.cost,
              "A3b", "the launch tick is w.current_econ_tick; the record carries the quoted cost");
        space_asset_step_report r5, r6;
        run_space_asset_step(k.w, p, 5, &r5);
        const bool at5 = a0.state == space_asset_state::active && a1.state == space_asset_state::in_transit;
        run_space_asset_step(k.w, p, 6, &r6);
        const bool at6 = a1.state == space_asset_state::active;
        check(at5 && at6, "A3b", "travel 0 is active in the launch quarter's step; travel 1 exactly one step later");
        check(r5.launched == 2 && r5.launch_spent == q0.cost + q1.cost && r5.upkeep_paid == p.upkeep[0]
                  && r6.launched == 0 && r6.launch_spent == 0.0f && r6.upkeep_paid == 2 * p.upkeep[0],
              "A3b", fmt("the step reports the quarter's launches (2, %.0f) then none; upkeep %.0f then %.0f",
                         q0.cost + q1.cost, p.upkeep[0], 2 * p.upkeep[0]));
    }

    // ---- A5 decay ----------------------------------------------------------
    std::printf("--- A5 decay ---\n");
    {
        w.nations.at(f.a).treasury = 0.0f;
        space_asset_step_report r;
        run_space_asset_step(w, p, 14, &r);
        const space_asset& s = w.space_assets.at(sat);
        check(s.state == space_asset_state::decaying && s.unpaid_quarters == 1 && r.unpaid == 2
                  && w.nations.at(f.a).treasury == 0.0f,
              "A5", "an unpaid quarter: decaying, 1 unpaid, nothing debited (treasury stays 0, never negative)");
        // A partial treasury pays the sat (lower id) and not the rover: whole or nothing, ascending id.
        w.nations.at(f.a).treasury = p.upkeep[0] + 1.0f;
        run_space_asset_step(w, p, 15, &r);
        check(w.space_assets.at(sat).state == space_asset_state::active
                  && w.space_assets.at(sat).unpaid_quarters == 0
                  && w.space_assets.at(rov).state == space_asset_state::decaying
                  && w.space_assets.at(rov).unpaid_quarters == 2
                  && w.nations.at(f.a).treasury == 1.0f,
              "A5", "a paid quarter restores the satellite; the rover (higher id) finds 1 credit and stays decaying");
        w.nations.at(f.a).treasury = 0.0f;
        int q = 16;
        while (w.space_assets.at(rov).state != space_asset_state::lost && q < 40)
            run_space_asset_step(w, p, q++, &r);
        check(w.space_assets.at(rov).state == space_asset_state::lost
                  && w.space_assets.at(rov).unpaid_quarters == p.quarters_to_loss,
              "A5", fmt("the rover is lost at %d consecutive unpaid quarters", p.quarters_to_loss));
        const int sat_unpaid = w.space_assets.at(sat).unpaid_quarters;
        check(w.space_assets.at(sat).state == space_asset_state::decaying && sat_unpaid == 2,
              "A5", fmt("the satellite, one paid quarter later, is decaying at %d unpaid", sat_unpaid));
        w.nations.at(f.a).treasury = 100000.0f;
        run_space_asset_step(w, p, q++, &r);
        check(w.space_assets.at(rov).state == space_asset_state::lost
                  && w.space_assets.at(rov).unpaid_quarters == p.quarters_to_loss
                  && w.nations.at(f.a).treasury == 100000.0f - p.upkeep[0],
              "A5", "a lost asset never pays or recovers; only the satellite drew upkeep");
    }

    // ---- A7 save -----------------------------------------------------------
    std::printf("--- A7 save ---\n");
    {
        // Add one of each remaining state: an in-transit station module.
        w.current_econ_tick = 50;
        const space_launch_quote st = L(w, p, f.a, space_asset_class::station_module, LO(f.home));
        check(st.result == space_launch_result::launched, "A7", "fixture: a station module in transit");
        // states present: active sat, lost rover, in-transit station; decaying via a fourth
        w.nations.at(f.a).treasury = 500000.0f;
        w.current_econ_tick = 20;
        const space_launch_quote cm = L(w, p, f.a, space_asset_class::crewed_mission, SURF(f.moon));
        check(cm.result == space_launch_result::launched, "A7", "fixture: a crewed mission to the moon surface");
        if (cm.result == space_launch_result::launched)
        {
            // Drive it to decaying the honest way: one step with an empty treasury.
            // (The station, arriving at 50, is untouched at tick 21.)
            const float keep = w.nations.at(f.a).treasury;
            w.nations.at(f.a).treasury = 0.0f;
            run_space_asset_step(w, p, 21);
            w.nations.at(f.a).treasury = keep;
        }
        const std::string b = to_bytes(w);
        world r;
        const bool ok = from_bytes(b, r);
        bool eq = ok && r.space_assets.size() == w.space_assets.size();
        if (eq)
            for (const auto& [id, a] : w.space_assets)
            {
                const space_asset& x = r.space_assets.at(id);
                eq = eq && x.id == a.id && x.owner == a.owner && x.cls == a.cls && x.dest.body == a.dest.body
                     && x.dest.slot == a.dest.slot && x.state == a.state && x.launch_tick == a.launch_tick
                     && x.arrival_tick == a.arrival_tick && x.upkeep == a.upkeep
                     && x.unpaid_quarters == a.unpaid_quarters && x.launch_cost == a.launch_cost
                     && x.node == a.node && x.role == a.role; // BL-1581 / BL-1586 (v83)
            }
        std::printf("    %zu assets:", w.space_assets.size());
        for (const auto& [id, a] : w.space_assets)
            std::printf(" %u=%s", static_cast<unsigned>(id), state_name(a.state));
        std::printf("; stream %zu bytes at world_save_version %u\n", b.size(), static_cast<unsigned>(world_save_version));
        check(eq && r.state_hash(0) == w.state_hash(0), "A7",
              "v51 round-trips every asset field by field, with an equal state_hash");
        check(ok && to_bytes(r) == b, "A7", "re-serialising the loaded world reproduces the stream byte for byte");
        check(ok && r.next_entity_id() == w.next_entity_id(), "A7", "the allocator cursor round-trips (no id re-issued)");

        // v50: the stream less the space section (count + n x 35-byte records), stamped 50.
        const std::size_t rec = 4 + 4 + 1 + 4 + 1 + 1 + 4 + 4 + 4 + 4 + 4;
        // BL-1335 (v52): the tax section follows the space section (count + 9
        // bytes a nation whose tier or Discontent moved), so both come off; the
        // record offsets below still count from the space section's start.
        // BL-1337 (v54): and the exact-net section after the tax section.
        // BL-1407 (v56): no corporation store, so a v56 stream is no longer a
        // v50 one with sections appended. The v50 prefix is cut from a v55 stream
        // (the verification writer, empty corporation store): the space section,
        // the v52 tax section (with its empty corporation half), the v54 net
        // section and the v55 majors. The tamper rows below read the v56 stream,
        // whose tax section carries the nation rows only, followed by the majors
        // and the v56 nation books.
        const auto v55_of = [](const world& x) {
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_legacy(x, {}, o, k_world_save_version_pre_books);
            return o.str();
        };
        const auto tail55_of = [&](const world& x) {
            return 4 + x.space_assets.size() * rec + world_save_tax_section_bytes(x)
                 + world_save_net_section_bytes(x) + world_save_major_section_bytes(x);
        };
        const std::size_t tail = 4 + w.space_assets.size() * rec + (world_save_tax_section_bytes(w) - 4)
                               + world_save_major_section_bytes(w) + world_save_books_section_bytes(w)
                               + 12  // BL-1434 (v64): the designs section, three empty counts
                               + 24  // BL-1449 (v65): the air section, six empty counts
                               + 4   // BL-1452 (v66): the carried-supplies section, one empty count
                               + world_save_battle_carry_section_bytes(w) // BL-1453 (v67)
                               + world_save_alarm_section_bytes(w)       // BL-1432 (v68)
                               + world_save_war_section_bytes(w)          // BL-1459 (v69)
                               + world_save_embargo_section_bytes(w) + world_save_gen_capital_section_bytes(w)     // BL-1418 (v70)
                               + world_save_recruits_section_bytes(w) + world_save_sea_recruits_section_bytes(w) + world_save_refusals_section_bytes(w) /* BL-1480 (v75) */
                               + world_save_contributors_section_bytes(w) + world_save_occupation_section_bytes(w) // v77, v78
                               + world_save_defend_section_bytes(w) + world_save_research_books_section_bytes(w)   // v79, v80
                               + world_save_station_section_bytes(w) + world_save_early_space_section_bytes(w) // BL-1583 (v81), BL-1612 (v82)
                               + (world_save_space_node_section_bytes(w) + (world_save_space_unit_section_bytes(w) + (world_save_space_battle_section_bytes(w) + (world_save_prospecting_section_bytes(w) + (world_save_space_convoy_section_bytes(w) + (world_save_debris_section_bytes(w) + (world_save_space_bombard_section_bytes(w) + world_save_offworld_base_section_bytes(w)))))))); // BL-1581 (v83), last
        // BL-1582: the launch-gate staging (a gridless tile, its pad, the shelf) predates
        // v53 tiles; the v50 rows read the world without it.
        const auto unstaged = [](world x) {
            x.tiles.clear();
            x.buildings.clear();
            x.markets.clear();
            for (auto& [nid, nc] : x.nations) nc.state_assets.clear();
            return x;
        };
        const world w50 = unstaged(w);
        const std::string b55 = v55_of(w50);
        std::string v50 = b55.substr(0, b55.size() - tail55_of(w50));
        const std::uint32_t pre = k_world_save_version_pre_space;
        std::memcpy(&v50[4], &pre, sizeof pre);
        world old;
        const bool ok50 = from_bytes(v50, old);
        check(ok50 && old.space_assets.empty() && old.nations.size() == w.nations.size(),
              "A7", "a v50 stream (the v51 prefix) reads, as a world with no space assets");

        // Tampered: the first record's owner -> an id no nation holds.
        std::string bad = b;
        const entity_id junk = 0x7FFFFFF0u;
        std::memcpy(&bad[b.size() - tail + 4 + 4], &junk, sizeof junk);
        world victim;
        victim.current_econ_tick = 77;
        check(!from_bytes(bad, victim) && victim.current_econ_tick == 77 && victim.nations.empty(),
              "A7", "an owner naming no nation refuses the stream whole, destination untouched");
        // Tampered: a state byte out of range.
        std::string bad2 = b;
        bad2[b.size() - tail + 4 + 4 + 4 + 1 + 4 + 1] = static_cast<char>(9);
        world victim2;
        check(!from_bytes(bad2, victim2) && victim2.nations.empty(), "A7", "an out-of-range state byte refuses the stream whole");
        // Record field offsets from the first record's start: id 0, owner 4,
        // class 8, body 9, slot 13, state 14, launch 15, arrival 19, upkeep 23,
        // unpaid 27, launch cost 31. The first record is the satellite (decaying, 1 unpaid).
        const std::size_t r0 = b.size() - tail + 4;
        auto refused = [&](std::size_t off, const void* v, std::size_t n) {
            std::string t = b;
            std::memcpy(&t[r0 + off], v, n);
            world vv;
            return !from_bytes(t, vv) && vv.nations.empty();
        };
        const space_asset& first = w.space_assets.begin()->second;
        check(first.cls == space_asset_class::satellite && first.state == space_asset_state::decaying
                  && first.unpaid_quarters == 1,
              "A7", "fixture: the first record is a decaying satellite at 1 unpaid");
        const std::uint8_t surface = 1, active = 1, lost = 3, transit = 0;
        const std::int32_t over = k_space_quarters_to_loss_ceiling + 1, zero = 0;
        check(refused(13, &surface, 1), "A7", "a satellite stored on a SURFACE slot refuses the stream whole");
        check(refused(9, &f.star, sizeof f.star), "A7", "an asset stored at the STAR refuses the stream whole");
        check(refused(14, &active, 1), "A7", "state active with 1 unpaid quarter refuses the stream whole");
        check(refused(14, &transit, 1), "A7", "state in_transit with 1 unpaid quarter refuses the stream whole");
        check(refused(27, &zero, 4), "A7", "state decaying with 0 unpaid quarters refuses the stream whole");
        {
            std::string t = b;
            std::memcpy(&t[r0 + 14], &lost, 1);
            std::memcpy(&t[r0 + 27], &over, 4);
            world vv;
            check(!from_bytes(t, vv), "A7", fmt("state lost with %d unpaid (past the ceiling %d) refuses the stream whole",
                                                over, k_space_quarters_to_loss_ceiling));
        }
        {
            // An id another entity kind owns: a corporation keyed by the first
            // asset's id. The control — the same corporation under a fresh id — loads.
            world okc = w;
            const entity_id fresh = okc.create_entity();
            okc.nations.emplace(fresh, nation_component{});
            world back_ok;
            const bool control = from_bytes(to_bytes(okc), back_ok);
            world clash = w;
            clash.nations.emplace(w.space_assets.begin()->first, nation_component{});
            world back_clash;
            const bool clash_refused = !from_bytes(to_bytes(clash), back_clash);
            check(control && clash_refused, "A7",
                  "an asset id a nation also owns refuses the stream whole (control: a fresh-id nation loads)");
        }
        {
            // A SET share rule in a v50 stream survives: the share section is read
            // for v50 as for v51.
            world ws = unstaged(w);
            ws.nations.at(f.a).lp_shares.set   = true;
            ws.nations.at(f.a).lp_shares.share = {0.2f, 0.5f, 0.3f};
            const std::string bs = v55_of(ws); // BL-1407: the v55 stream, as above
            std::string s50 = bs.substr(0, bs.size() - tail55_of(ws));
            std::memcpy(&s50[4], &pre, sizeof pre);
            world back;
            const bool ok_s = from_bytes(s50, back);
            check(ok_s && back.nations.at(f.a).lp_shares.set
                      && back.nations.at(f.a).lp_shares.share == ws.nations.at(f.a).lp_shares.share
                      && back.space_assets.empty(),
                  "A7", "a v50 stream carrying a SET share rule reads with the rule intact (and no assets)");
        }
        // A version past the current one is refused.
        std::string next = b;
        const std::uint32_t nv = world_save_version + 1;
        std::memcpy(&next[4], &nv, sizeof nv);
        world victim3;
        check(!from_bytes(next, victim3), "A7", "a stream stamped world_save_version + 1 is refused");
    }

    // ---- A10 orbital slots (BL-1581) ---------------------------------------
    std::printf("--- A10 orbital slots ---\n");
    {
        fixture k;
        build(k);
        world& kw = k.w;
        kw.nations.at(k.a).treasury = 1000000.0f;
        kw.nations.at(k.b).treasury = 1000000.0f;
        kw.current_econ_tick = 0;
        const orbital_node hi = HO(k.home);
        const int cap = orbital_slot_capacity(kw, hi);
        int ok = 0;
        for (int i = 0; i < cap - 1; ++i)
            ok += L(kw, p, k.a, space_asset_class::satellite, hi).result == space_launch_result::launched ? 1 : 0;
        const space_launch_quote bq = L(kw, p, k.b, space_asset_class::satellite, hi);
        check(cap == 4 && ok == cap - 1 && bq.result == space_launch_result::launched
                  && orbital_slots_held(kw, hi) == cap,
              "A10", fmt("a planet's high orbit holds %d: Alpha takes 3, Beta the 4th; held = %d", cap, orbital_slots_held(kw, hi)));
        {
            const snap before = take(k);
            const space_launch_quote full = L(kw, p, k.a, space_asset_class::satellite, hi);
            check(full.result == space_launch_result::rejected_no_slot && same(before, take(k)), "A10",
                  "a launch to the full node is refused whole (hash, treasuries, shelf, store, cursor)");
            const space_launch_quote crewed = Q(kw, p, k.a, space_asset_class::crewed_mission, hi);
            check(crewed.result == space_launch_result::launched, "A10",
                  "a crewed mission holds no slot: it may still go to the full node");
        }
        // Beta stops paying: its satellite decays and is lost, freeing the slot at once.
        kw.nations.at(k.b).treasury = 0.0f;
        int q = 0, held_after_loss = -1, lost_at = -1;
        while (q < 20)
        {
            run_space_asset_step(kw, p, q);
            if (kw.space_assets.at(bq.asset).state == space_asset_state::lost)
            {
                held_after_loss = orbital_slots_held(kw, hi);
                lost_at = q;
                break;
            }
            ++q;
        }
        check(lost_at >= 0 && held_after_loss == cap - 1, "A10",
              fmt("Beta's satellite is lost at quarter %d and its slot frees at once (held %d)", lost_at, held_after_loss));
        kw.current_econ_tick = q + 1;
        check(L(kw, p, k.a, space_asset_class::satellite, hi).result == space_launch_result::launched
                  && orbital_slots_held(kw, hi) == cap,
              "A10", "the freed slot takes the next launch (first come, first held)");
        // In transit holds a slot too: a moon's high orbit holds 2.
        const orbital_node mh = HO(k.moon);
        L(kw, p, k.a, space_asset_class::satellite, mh);
        const space_launch_quote m2 = L(kw, p, k.b, space_asset_class::satellite, mh);
        kw.nations.at(k.b).treasury = 1000000.0f;
        const space_launch_quote m2b = L(kw, p, k.b, space_asset_class::satellite, mh);
        const space_launch_quote m3 = Q(kw, p, k.a, space_asset_class::satellite, mh);
        check(orbital_slot_capacity(kw, mh) == 2 && m2.result == space_launch_result::rejected_insufficient_treasury
                  && m2b.result == space_launch_result::launched && m3.result == space_launch_result::rejected_no_slot
                  && kw.space_assets.at(m2b.asset).state == space_asset_state::in_transit,
              "A10", "two satellites IN TRANSIT to a moon's high orbit (2 slots) fill it: a third is refused");
        // A station core holds one slot; a module rides inside it (station_harness owns stations).
        check(space_class_holds_slot(space_asset_class::station_core)
                  && !space_class_holds_slot(space_asset_class::station_module)
                  && !space_class_holds_slot(space_asset_class::rover),
              "A10", "satellites and station cores hold slots; modules, rovers and crewed missions do not");
    }

    // ---- A11 the launch gate (BL-1582) -------------------------------------
    std::printf("--- A11 the launch gate ---\n");
    {
        auto run_case = [&](const char* what, space_launch_result want, auto&& stage,
                            space_asset_class cls = space_asset_class::satellite) {
            fixture k;
            build(k);
            stage(k);
            k.w.current_econ_tick = 4;
            const snap before = take(k);
            const space_launch_quote q = L(k.w, p, k.a, cls, LO(k.home));
            check(q.result == want && q.asset == null_entity && same(before, take(k)), "A11",
                  fmt("%s: refused (code %d), nothing touched", what, static_cast<int>(q.result)));
        };
        run_case("no Rocketry (space_dispatch)", space_launch_result::rejected_tech_locked,
                 [](fixture& k) { space_fixture::drop_key(k.w, k.a, space_keys::dispatch); });
        run_case("no Rocketry: a rover too", space_launch_result::rejected_tech_locked,
                 [](fixture& k) { space_fixture::drop_key(k.w, k.a, space_keys::dispatch); },
                 space_asset_class::crewed_mission);
        run_case("the comms role's key not held", space_launch_result::rejected_tech_locked,
                 [](fixture& k) { space_fixture::drop_key(k.w, k.a, space_keys::role_comms); });
        run_case("no Launchpad", space_launch_result::rejected_no_launchpad,
                 [](fixture& k) {
                     auto& sa = k.w.nations.at(k.a).state_assets;
                     sa.erase(std::remove(sa.begin(), sa.end(), k.pad_a), sa.end());
                     k.w.buildings.erase(k.pad_a);
                 });
        run_case("an unstaffed Launchpad", space_launch_result::rejected_no_launchpad,
                 [](fixture& k) { k.w.buildings.at(k.pad_a).workforce_assigned = 0.0f; });
        run_case("an unfinished Launchpad", space_launch_result::rejected_no_launchpad,
                 [](fixture& k) { k.w.buildings.at(k.pad_a).ticks_remaining = 3; });
        run_case("a Launchpad off the home body", space_launch_result::rejected_no_launchpad,
                 [](fixture& k) { k.w.buildings.at(k.pad_a).tile = space_fixture::add_tile(k.w, k.moon); });
        run_case("an empty propellant shelf", space_launch_result::rejected_shelf,
                 [](fixture& k) { k.w.markets.at(k.market).inventory[static_cast<std::size_t>(resource_type::propellant)] = 10.0f; });
        run_case("a components shelf one short", space_launch_result::rejected_shelf,
                 [&](fixture& k) { k.w.markets.at(k.market).inventory[static_cast<std::size_t>(resource_type::spacecraft_components)] = p.components_lot[0] - 1.0f; });
        run_case("no market on the home body", space_launch_result::rejected_shelf,
                 [](fixture& k) { k.w.markets.at(k.market).body = k.moon; });
        run_case("a short treasury", space_launch_result::rejected_insufficient_treasury,
                 [](fixture& k) { k.w.nations.at(k.a).treasury = 100.0f; });
        {
            // The role key gates only its role: without comms, a navigation satellite and a rover launch.
            fixture k;
            build(k);
            space_fixture::drop_key(k.w, k.a, space_keys::role_comms);
            const space_launch_quote nav = quote_space_launch(k.w, p, k.a, space_asset_class::satellite, LO(k.home),
                                                              space_satellite_role::navigation);
            const space_launch_quote rov = Q(k.w, p, k.a, space_asset_class::rover, SURF(k.moon));
            check(nav.result == space_launch_result::launched && rov.result == space_launch_result::launched, "A11",
                  "a missing role key closes only that role: a navigation satellite and a rover still quote");
        }
    }

    // ---- A13 the command seam and signal delay (BL-1588 / BL-1587) ----------
    std::printf("--- A13 command seam, signal delay ---\n");
    {
        fixture k;
        build(k);
        world& kw = k.w;
        recipe_registry reg;
        kw.nations.at(k.a).treasury = 1000000.0f;
        kw.current_econ_tick = 7;
        space_command launch;
        launch.nation = k.a;
        launch.verb = static_cast<uint8_t>(space_verb::launch);
        launch.cls = static_cast<uint8_t>(space_asset_class::satellite);
        launch.role = static_cast<uint8_t>(space_satellite_role::comms);
        launch.node_body = k.home;
        launch.node_kind = static_cast<uint8_t>(orbital_node_kind::low_orbit);
        const space_command_outcome lh = apply_space_command(kw, reg, launch);
        launch.node_body = k.moon;
        const space_command_outcome lm = apply_space_command(kw, reg, launch);
        check(lh.result == space_command_result::applied && lm.result == space_command_result::applied
                  && kw.space_orders.empty(),
              "A13", "a launch through the seam applies at once, to any node (the ground sees the pad)");

        space_command retask;
        retask.nation = k.a;
        retask.verb = static_cast<uint8_t>(space_verb::retask_satellite);
        retask.subject = lh.asset;
        retask.role = static_cast<uint8_t>(space_satellite_role::navigation);
        const space_command_outcome rh = apply_space_command(kw, reg, retask);
        check(rh.result == space_command_result::applied
                  && kw.space_assets.at(lh.asset).role == space_satellite_role::navigation && kw.space_orders.empty(),
              "A13", "retasking a satellite in coverage (home low orbit) lands at once");

        retask.subject = lm.asset;
        retask.role = static_cast<uint8_t>(space_satellite_role::survey);
        const space_command_outcome rm = apply_space_command(kw, reg, retask);
        check(rm.result == space_command_result::queued && kw.space_orders.size() == 1
                  && kw.space_orders[0].due_tick == 8
                  && kw.space_assets.at(lm.asset).role == space_satellite_role::comms,
              "A13", "retasking a satellite outside coverage (moon low orbit) is queued: role unchanged, due next quarter");

        // Save carries the pending order.
        {
            const std::string b = to_bytes(kw);
            world back;
            const bool ok = from_bytes(b, back);
            check(ok && back.space_orders.size() == 1 && back.space_orders[0].id == kw.space_orders[0].id
                      && back.space_orders[0].due_tick == 8 && back.space_orders[0].cmd.subject == lm.asset
                      && back.space_orders[0].cmd.role == retask.role && back.next_space_order_id == kw.next_space_order_id
                      && back.state_hash(0) == kw.state_hash(0) && to_bytes(back) == b,
                  "A14", "a pending order round-trips (fields, equal state_hash, byte-identical re-serialise)");
            const std::size_t sec = b.size() - (world_save_space_node_section_bytes(kw) + (world_save_space_unit_section_bytes(kw) + (world_save_space_battle_section_bytes(kw) + (world_save_prospecting_section_bytes(kw) + (world_save_space_convoy_section_bytes(kw) + (world_save_debris_section_bytes(kw) + (world_save_space_bombard_section_bytes(kw) + world_save_offworld_base_section_bytes(kw))))))));
            const std::size_t ord = sec + 4 + kw.space_assets.size() * 10 + 4 + 4; // the first order
            auto refused = [&](std::size_t off, const void* v, std::size_t n) {
                std::string t = b;
                std::memcpy(&t[off], v, n);
                world vv;
                return !from_bytes(t, vv) && vv.nations.empty();
            };
            const std::uint8_t launch_verb = 0, bad_kind = 9, bad_role = 9;
            const entity_id junk = 0x7FFFFFF0u;
            const std::int32_t negative = -1;
            const std::uint32_t far_id = 999999u;
            check(refused(ord + 12, &launch_verb, 1), "A14", "a pending order with a verb the signal never delays refuses the stream whole");
            check(refused(ord + 8, &junk, 4), "A14", "a pending order naming no nation refuses the stream whole");
            check(refused(ord + 19, &bad_kind, 1), "A14", "a pending order with an out-of-range node kind refuses the stream whole");
            check(refused(ord + 4, &negative, 4), "A14", "a pending order due at a negative tick refuses the stream whole");
            check(refused(ord + 0, &far_id, 4), "A14", "a pending order id at or past next_space_order_id refuses the stream whole");
            // The node rows: the first asset (the home satellite) at index 0.
            check(refused(sec + 4 + 4, &k.moon, 4), "A14", "an asset node naming another body than its own refuses the stream whole");
            check(refused(sec + 4 + 8, &bad_kind, 1), "A14", "an out-of-range node kind refuses the stream whole");
            check(refused(sec + 4 + 9, &bad_role, 1), "A14", "an out-of-range role refuses the stream whole");
            const std::uint8_t surface = 0;
            check(refused(sec + 4 + 8, &surface, 1), "A14", "a satellite node on the surface (disagreeing with its orbit slot) refuses the stream whole");
            // v81: the prefix without the section reads, nodes derived, no queue.
            std::ostringstream o81(std::ios::binary);
            write_world_snapshot_pre_space_nodes(kw, o81);
            world old;
            const bool ok81 = from_bytes(o81.str(), old);
            check(ok81 && o81.str().size() + (world_save_space_node_section_bytes(kw) + (world_save_space_unit_section_bytes(kw) + (world_save_space_battle_section_bytes(kw) + (world_save_prospecting_section_bytes(kw) + (world_save_space_convoy_section_bytes(kw) + (world_save_debris_section_bytes(kw) + (world_save_space_bombard_section_bytes(kw) + world_save_offworld_base_section_bytes(kw)))))))) == b.size()
                      && old.space_orders.empty() && old.space_assets.at(lm.asset).node == LO(k.moon)
                      && old.space_assets.at(lm.asset).role == space_satellite_role::none,
                  "A14", "a v82 stream (the v83 prefix) reads: nodes derived (moon low orbit), roles none, no pending orders");
        }

        economy_report r7;
        run_nation_step(kw, reg, r7, 7);
        check(kw.space_orders.size() == 1 && kw.space_assets.at(lm.asset).role == space_satellite_role::comms
                  && r7.space_orders.applied == 0,
              "A13", "the launch quarter's nation step (tick 7) leaves it waiting");
        kw.current_econ_tick = 8;
        economy_report r8;
        run_nation_step(kw, reg, r8, 8);
        check(kw.space_orders.empty() && kw.space_assets.at(lm.asset).role == space_satellite_role::survey
                  && r8.space_orders.applied == 1 && r8.space_orders.dropped == 0,
              "A13", "exactly one quarter later (tick 8) the order lands and leaves the queue");

        // A pending order no longer legal when it lands is dropped and reported.
        retask.role = static_cast<uint8_t>(space_satellite_role::observation);
        const space_command_outcome rq = apply_space_command(kw, reg, retask);
        kw.space_assets.at(lm.asset).state = space_asset_state::lost; // lost before the signal arrives
        kw.space_assets.at(lm.asset).unpaid_quarters = 4;
        kw.current_econ_tick = 9;
        economy_report r9;
        run_nation_step(kw, reg, r9, 9);
        check(rq.result == space_command_result::queued && r9.space_orders.dropped == 1
                  && r9.space_orders.dropped_orders.size() == 1
                  && r9.space_orders.dropped_orders[0].first == rq.order_id
                  && r9.space_orders.dropped_orders[0].second.satellite == satellite_order_result::rejected_state
                  && kw.space_orders.empty() && kw.space_assets.at(lm.asset).role == space_satellite_role::survey,
              "A13", "an order whose satellite was lost before its signal is dropped, reported (rejected_state), and leaves the queue");

        // Move: the home satellite, in coverage, to home high orbit — at once.
        kw.current_econ_tick = 10;
        const snap before = take(k);
        const int lo_held = orbital_slots_held(kw, LO(k.home)), hi_held = orbital_slots_held(kw, HO(k.home));
        space_launch_quote mq;
        const satellite_order mv_quote = quote_move_satellite(kw, reg.space_assets(), k.a, lh.asset, HO(k.home), &mq);
        space_command move;
        move.nation = k.a;
        move.verb = static_cast<uint8_t>(space_verb::move_satellite);
        move.subject = lh.asset;
        move.node_body = k.home;
        move.node_kind = static_cast<uint8_t>(orbital_node_kind::high_orbit);
        const space_command_outcome mo = apply_space_command(kw, reg, move);
        const snap after = take(k);
        check(mv_quote.result == satellite_order_result::ok && mo.result == space_command_result::applied
                  && kw.space_assets.at(lh.asset).node == HO(k.home)
                  && kw.space_assets.at(lh.asset).state == space_asset_state::in_transit
                  && orbital_slots_held(kw, LO(k.home)) == lo_held - 1 && orbital_slots_held(kw, HO(k.home)) == hi_held + 1,
              "A13", "a move in coverage lands at once: in transit to the new node, the old slot freed, the new one held");
        check(mq.components == 0.0f && after.comp == before.comp && after.prop == before.prop - mq.propellant
                  && mq.propellant > 0.0f && after.ta == before.ta - mq.cost,
              "A13", fmt("a move buys only the path's propellant (%.1f) and pays its thrust (%.0f total)", mq.propellant, mq.cost));

        // Validation: each malformed field refuses the whole command, mutating nothing.
        auto refused_whole = [&](space_command c, space_command_result want, const char* what) {
            const snap s0 = take(k);
            const std::size_t q0 = kw.space_orders.size();
            const space_command_outcome o = apply_space_command(kw, reg, c);
            check(o.result == want && same(s0, take(k)) && kw.space_orders.size() == q0, "A13",
                  fmt("%s: refused whole (code %d)", what, static_cast<int>(o.result)));
        };
        space_command c = retask;
        c.verb = 200;
        refused_whole(c, space_command_result::rejected_invalid, "an out-of-range verb");
        c = retask;
        c.role = 9;
        refused_whole(c, space_command_result::rejected_invalid, "an out-of-range role");
        c = retask;
        c.role = 0;
        refused_whole(c, space_command_result::rejected_invalid, "a retask to no role");
        c = move;
        c.node_kind = 7;
        refused_whole(c, space_command_result::rejected_invalid, "an out-of-range node kind");
        c = move;
        c.node_body = k.star;
        refused_whole(c, space_command_result::rejected_invalid, "a node at the star");
        c = retask;
        c.subject = k.home;
        refused_whole(c, space_command_result::rejected_invalid, "a subject that is no satellite");
        c = retask;
        c.nation = k.b;
        refused_whole(c, space_command_result::rejected_not_owner, "another nation's satellite");
        c = retask;
        c.nation = 999999u;
        refused_whole(c, space_command_result::rejected_unknown_nation, "an unknown nation");
        c = launch;
        c.cls = static_cast<uint8_t>(space_asset_class::station_core);
        refused_whole(c, space_command_result::rejected_invalid, "a station core through the launch verb (found_station's)");
        c = launch;
        c.cls = 5;
        refused_whole(c, space_command_result::rejected_invalid, "an out-of-range class");
    }

    // ---- A8 wiring ---------------------------------------------------------
    std::printf("--- A8 wiring through run_nation_step ---\n");
    {
        fixture g;
        build(g);
        const std::uint64_t empty_hash = g.w.state_hash(0);
        recipe_registry reg;
        economy_report rep;
        g.w.current_econ_tick = 1;
        run_nation_step(g.w, reg, rep, 1);
        check(g.w.space_assets.empty(), "A8", "an empty store stays empty through the nation step");
        {
            // The fold is inert when empty: a world whose store held an asset and
            // was emptied again hashes exactly as it did before (no allocation, so
            // nothing else moved).
            fixture h;
            build(h);
            const std::uint64_t before = h.w.state_hash(0);
            check(before == empty_hash, "A8", "two fresh fixtures hash equal");
            space_asset probe;
            probe.id = 999;
            probe.owner = h.a;
            probe.dest = {h.home, space_slot::orbit};
            probe.node = LO(h.home);
            h.w.space_assets.emplace(999, probe);
            const std::uint64_t with = h.w.state_hash(0);
            h.w.space_assets.clear();
            check(with != empty_hash && h.w.state_hash(0) == empty_hash, "A8",
                  "an emptied store hashes as one that never held an asset (the fold is inert when empty)");
        }
        g.w.current_econ_tick = 2; // the tick about to run
        const space_launch_quote q = L(g.w, reg.space_assets(), g.a, space_asset_class::satellite, LO(g.home));
        const float t = g.w.nations.at(g.a).treasury;
        economy_report rep2;
        run_nation_step(g.w, reg, rep2, 2);
        const space_asset& a = g.w.space_assets.at(q.asset);
        check(q.result == space_launch_result::launched && a.state == space_asset_state::active
                  && g.w.nations.at(g.a).treasury == t - reg.space_assets().upkeep[0],
              "A8", fmt("a home-orbit satellite (0 quarters) arrives and pays %.0f through run_nation_step",
                        reg.space_assets().upkeep[0]));
        const space_asset_step_report& sr = rep2.space_assets;
        check(sr.launched == 1 && sr.launch_spent == q.cost && sr.arrived == 1 && sr.paid == 1
                  && sr.upkeep_paid == reg.space_assets().upkeep[0],
              "A8", fmt("economy_report::space_assets files the quarter: 1 launch at %.0f, 1 arrival, %.0f upkeep",
                        q.cost, reg.space_assets().upkeep[0]));
        // The fold sees the store: flipping one asset's state moves the hash.
        const std::uint64_t h0 = g.w.state_hash(0);
        g.w.space_assets.at(q.asset).state = space_asset_state::decaying;
        const std::uint64_t h1 = g.w.state_hash(0);
        g.w.space_assets.at(q.asset).state = space_asset_state::active;
        check(h0 != h1 && g.w.state_hash(0) == h0, "A8", "state_hash folds the store (a state flip moves it, the flip back restores it)");
    }

    // ---- A9 determinism ----------------------------------------------------
    std::printf("--- A9 determinism ---\n");
    {
        auto drive = [&](fixture& x) {
            x.w.current_econ_tick = 0;
            L(x.w, p, x.a, space_asset_class::satellite, LO(x.moon));
            L(x.w, p, x.a, space_asset_class::rover, SURF(x.far));
            x.w.current_econ_tick = 1;
            L(x.w, p, x.b, space_asset_class::satellite, LO(x.home));
            for (int t = 0; t < 30; ++t)
            {
                if (t == 12) x.w.nations.at(x.a).treasury = 0.0f;
                run_space_asset_step(x.w, p, t);
            }
            return x.w.state_hash(0);
        };
        fixture x, y;
        build(x);
        build(y);
        const std::uint64_t hx = drive(x), hy = drive(y);
        check(hx == hy && to_bytes(x.w) == to_bytes(y.w), "A9",
              fmt("two fixtures driven identically: equal state_hash %016llx and equal streams",
                  static_cast<unsigned long long>(hx)));
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
