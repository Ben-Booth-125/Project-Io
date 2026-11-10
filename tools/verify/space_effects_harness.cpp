// ---------------------------------------------------------------------------
// space_effects_harness — BL-1587 (coverage), BL-1586 (satellite roles),
// BL-1584 (station module effects)
// ---------------------------------------------------------------------------
// docs/economy/SPACE_ASSETS.md § Satellites, § Space stations, § Signal delay.
// A hand-built fixture: a star, a home planet at 1.0 AU, its moon (a 32x16 grid,
// eight survey regions, hidden), a far planet, and two nations on the home
// planet with one home market. Assets are STAGED by writing node, role and state
// directly — the launch path is another lane's. Rows:
//
//   C1  coverage from home: exactly the home low and high orbit.
//   C2  an ACTIVE Comms satellite at the moon's low orbit covers all seven moon
//       nodes; one in transit, decaying or another nation's covers nothing.
//   C3  an L-point Comms satellite (moon L1) covers the moon: the pair's smaller body.
//   C4  a WORKING Comms relay at moon L4 covers its node and every node one link
//       away (checked against orbital_links independently) and nothing else; an
//       unarrived relay covers nothing.
//   R1  a Research module earns RP into its owner's pool only while its station
//       node is covered; out of coverage it earns none and is counted unsignalled.
//   L1  Life support buys rations x crew off the home shelf at the posted price;
//       an empty shelf (or treasury) starves the station: its crewed modules stop
//       for the step (no RP), its crewless relay still covers.
//   F1  a Microgravity factory buys a silicon lot and sells an electronics lot;
//       an empty silicon shelf idles it with nothing moved.
//   S1  a Survey satellite maps its body only while covered: hidden -> scanning,
//       four regions a quarter, surveyed in two quarters; uncovered maps none.
//   O1  an Observatory maps every unsurveyed body in its owner's coverage.
//   N1  a Navigation satellite's factor: 0.9 for its owner on its body, 1 for
//       another nation, another body, or an asset not active.
//   N2  ...wired: on a GENERATED world, an intra-body trade leg priced for a
//       nation costs less once it has an active Navigation satellite at home,
//       by no more than the 10% of its haul; another nation's leg is unchanged.
//   V1  an Observation satellite lifts the vision fog only while covered, only
//       for its owner.
//   A1  the activity fog: a cold route reads known_stale; with an ACTIVE Comms
//       satellite AND Communication Satellites held it reads visible; either
//       alone does not. world.cpp's twin agrees with space_comms_lifts_activity_fog.
//   Z1  a world with no space assets: the step changes nothing (hash equal).
//   D1  two fixtures driven identically end with equal hash and equal stream.
//
// Build: node tools/verify/build_harness.js space_effects_harness --run
// ---------------------------------------------------------------------------

#include "world/hard_coded_world.hpp"
#include "world/nation_tech.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/orbital_graph.hpp"
#include "world/space_assets.hpp"
#include "world/space_coverage.hpp"
#include "world/space_effects.hpp"
#include "world/stations.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
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

struct fixture
{
    world     w;
    entity_id star = null_entity, home = null_entity, moon = null_entity, far = null_entity;
    entity_id a = null_entity, b = null_entity, market = null_entity;
};

entity_id add_body(world& w, const char* name, body_type t, entity_id parent, float au, int gw, int gh)
{
    const entity_id id = w.create_entity();
    body_component bc;
    bc.name = name;
    bc.type = t;
    bc.parent = parent;
    bc.orbital_radius_au = au;
    bc.orbital_angle_rad = 0.0f;
    bc.grid_width = gw;
    bc.grid_height = gh;
    bc.survey = {};
    w.bodies.emplace(id, bc);
    return id;
}

constexpr std::size_t k_rations = static_cast<std::size_t>(resource_type::food_rations);
constexpr std::size_t k_silicon = static_cast<std::size_t>(resource_type::silicon);
constexpr std::size_t k_elec    = static_cast<std::size_t>(resource_type::electronics);

void build(fixture& f)
{
    world& w = f.w;
    f.star = add_body(w, "Star", body_type::star, null_entity, 0.0f, 0, 0);
    f.home = add_body(w, "Home", body_type::planet, null_entity, 1.0f, 0, 0);
    f.moon = add_body(w, "Moon", body_type::moon, f.home, 0.0026f, 32, 16);
    f.far  = add_body(w, "Far", body_type::planet, null_entity, 2.0f, 16, 16);
    w.star_body = f.star;
    w.home_body = f.home;
    w.bodies.at(f.home).survey.phase = survey_phase::surveyed;
    for (entity_id* n : {&f.a, &f.b})
    {
        *n = w.create_entity();
        nation_component nc;
        nc.name = (n == &f.a) ? "Alpha" : "Beta";
        nc.treasury = 10000.0f;
        w.nations.emplace(*n, nc);
    }
    w.player_entity = f.a;

    f.market = w.create_entity();
    market_component mc{};
    mc.body = f.home;
    mc.base_price[k_rations] = 13.6f;
    mc.base_price[k_silicon] = 9.6f;
    mc.base_price[k_elec]    = 41.6f;
    mc.inventory[k_rations]  = 100.0f;
    mc.inventory[k_silicon]  = 100.0f;
    w.markets.emplace(f.market, mc);
}

entity_id stage_sat(world& w, entity_id owner, orbital_node node, space_satellite_role role,
                    space_asset_state state = space_asset_state::active)
{
    const entity_id id = w.create_entity();
    space_asset a;
    a.id = id;
    a.owner = owner;
    a.cls = space_asset_class::satellite;
    a.dest = {node.body, space_slot::orbit};
    a.state = state;
    a.node = node;
    a.role = role;
    w.space_assets.emplace(id, a);
    return id;
}

/// Stage a station at @p node: a core with @p core_crew aboard and one module per
/// kind in @p kinds, each in place in the next free slot. Returns the station id.
entity_id stage_station(world& w, entity_id owner, orbital_node node, int core_crew,
                        std::initializer_list<station_module_kind> kinds,
                        space_asset_state module_state = space_asset_state::active)
{
    const entity_id core = w.create_entity();
    space_station st;
    st.id = core;
    st.owner = owner;
    st.core = core;
    st.node = node;
    st.level = 0;
    st.slots.assign(static_cast<std::size_t>(st.width() * st.height()), null_entity);
    st.slots[static_cast<std::size_t>(st.centre())] = core;

    space_asset c;
    c.id = core;
    c.owner = owner;
    c.cls = space_asset_class::station_core;
    c.state = space_asset_state::active;
    c.node = node;
    c.station = core;
    c.station_slot = static_cast<int16_t>(st.centre());
    c.crew = core_crew;
    w.space_assets.emplace(core, c);

    int slot = 0;
    for (const station_module_kind k : kinds)
    {
        if (slot == st.centre())
            ++slot;
        const entity_id mid = w.create_entity();
        space_asset m;
        m.id = mid;
        m.owner = owner;
        m.cls = space_asset_class::station_module;
        m.state = module_state;
        m.node = node;
        m.station = core;
        m.station_slot = static_cast<int16_t>(slot);
        m.module = static_cast<uint8_t>(k);
        w.space_assets.emplace(mid, m);
        st.slots[static_cast<std::size_t>(slot)] = mid;
        ++slot;
    }
    w.stations.emplace(core, st);
    return core;
}

int comms_node_index()
{
    for (int i = 0; i < io::dg::node_count; ++i)
        if (std::strcmp(io::dg::nodes[static_cast<std::size_t>(i)].id, "DG-NE-MK-B1") == 0)
            return i;
    return -1;
}

std::set<orbital_node> body_nodes(entity_id b)
{
    std::set<orbital_node> s;
    for (int k = 0; k < orbital_node_kind_count; ++k)
        s.insert({b, static_cast<orbital_node_kind>(k)});
    return s;
}

std::set<orbital_node> home_pair(entity_id home)
{
    return {{home, orbital_node_kind::low_orbit}, {home, orbital_node_kind::high_orbit}};
}

space_effects_report step(world& w, int tick = 1)
{
    w.current_econ_tick = tick;
    space_effects_report r;
    run_space_effects(w, space_effects_params{}, station_params{}, 1.5f, tick, &r);
    return r;
}

std::int64_t pool(const world& w, entity_id n)
{
    const auto it = w.nation_tech.find(n);
    return it == w.nation_tech.end() ? 0 : it->second.pool_milli;
}

void rows_coverage()
{
    std::printf("\nC coverage\n");
    {
        fixture f; build(f);
        check(space_coverage(f.w, f.a) == home_pair(f.home), "C1", "home low + high orbit only");
        check(space_coverage(f.w, null_entity).empty(), "C1b", "an unknown nation covers nothing");
    }
    {
        fixture f; build(f);
        stage_sat(f.w, f.a, {f.moon, orbital_node_kind::low_orbit}, space_satellite_role::comms);
        std::set<orbital_node> want = home_pair(f.home);
        for (const auto& n : body_nodes(f.moon)) want.insert(n);
        check(space_coverage(f.w, f.a) == want, "C2", "an active Comms satellite at the moon covers its seven nodes");
        check(space_coverage(f.w, f.b) == home_pair(f.home), "C2b", "...for its owner only");
    }
    {
        bool none = true;
        for (const space_asset_state s : {space_asset_state::in_transit, space_asset_state::decaying,
                                          space_asset_state::lost})
        {
            fixture f; build(f);
            stage_sat(f.w, f.a, {f.moon, orbital_node_kind::low_orbit}, space_satellite_role::comms, s);
            none = none && space_coverage(f.w, f.a) == home_pair(f.home);
        }
        check(none, "C2c", "a Comms satellite in transit, decaying or lost covers nothing");
        fixture f; build(f);
        stage_sat(f.w, f.a, {f.moon, orbital_node_kind::low_orbit}, space_satellite_role::navigation);
        check(space_coverage(f.w, f.a) == home_pair(f.home), "C2d", "a non-Comms satellite covers nothing");
    }
    {
        fixture f; build(f);
        stage_sat(f.w, f.a, {f.moon, orbital_node_kind::l1}, space_satellite_role::comms);
        const auto cov = space_coverage(f.w, f.a);
        check(cov.count({f.moon, orbital_node_kind::low_orbit}) == 1 && cov.size() == 9, "C3",
              fmt("an L-point Comms satellite serves the pair's smaller body (%zu nodes)", cov.size()));
    }
    {
        fixture f; build(f);
        const orbital_node l4{f.moon, orbital_node_kind::l4};
        stage_station(f.w, f.a, l4, 1, {station_module_kind::comms_relay});
        std::set<orbital_node> want = home_pair(f.home);
        want.insert(l4);
        for (const orbital_link& l : orbital_links(f.w))
        {
            if (l.a == l4) want.insert(l.b);
            if (l.b == l4) want.insert(l.a);
        }
        const auto cov = space_coverage(f.w, f.a);
        check(cov == want, "C4", fmt("a working relay at moon L4 covers its node + one hop (%zu nodes)", cov.size()));
        check(cov.count({f.moon, orbital_node_kind::low_orbit}) == 0, "C4b", "...and not two hops (moon low orbit)");
        check(space_node_covered(f.w, f.a, l4) && !space_node_covered(f.w, f.b, l4), "C4c",
              "space_node_covered agrees, for the owner only");
    }
    {
        fixture f; build(f);
        stage_station(f.w, f.a, {f.moon, orbital_node_kind::l4}, 1, {station_module_kind::comms_relay},
                      space_asset_state::in_transit);
        check(space_coverage(f.w, f.a) == home_pair(f.home), "C4d", "a relay not yet arrived covers nothing");
    }
}

void rows_research()
{
    std::printf("\nR research\n");
    const std::int64_t want = 50 * k_milli_per_rp;
    {
        fixture f; build(f);
        // Moon L4, relay + research: covered by the relay itself. Crew 2 covers both.
        stage_station(f.w, f.a, {f.moon, orbital_node_kind::l4}, 2,
                      {station_module_kind::comms_relay, station_module_kind::research});
        const auto r = step(f.w);
        check(pool(f.w, f.a) == want && r.research_milli == want, "R1",
              fmt("a covered Research module earns %lld milli-RP", static_cast<long long>(pool(f.w, f.a))));
    }
    {
        fixture f; build(f);
        stage_station(f.w, f.a, {f.moon, orbital_node_kind::low_orbit}, 2, {station_module_kind::research});
        const auto r = step(f.w);
        check(pool(f.w, f.a) == 0 && r.research_unsignalled == 1, "R1b",
              "out of coverage: no RP, counted unsignalled");
        stage_sat(f.w, f.a, {f.moon, orbital_node_kind::high_orbit}, space_satellite_role::comms);
        step(f.w, 2);
        check(pool(f.w, f.a) == want, "R1c", "a Comms satellite at the moon brings it into coverage: RP earned");
    }
    {
        fixture f; build(f);
        stage_station(f.w, f.a, {f.home, orbital_node_kind::low_orbit}, 1, {station_module_kind::research});
        step(f.w);
        check(pool(f.w, f.a) == 0, "R1d", "a Research module not working (crew short) earns nothing");
    }
}

void rows_life_support()
{
    std::printf("\nL life support\n");
    fixture f; build(f);
    stage_station(f.w, f.a, {f.home, orbital_node_kind::low_orbit}, 2,
                  {station_module_kind::power, station_module_kind::life_support,
                   station_module_kind::research, station_module_kind::comms_relay});
    const float t0 = f.w.nations.at(f.a).treasury;
    const auto r = step(f.w);
    const float drawn = 100.0f - f.w.markets.at(f.market).inventory[k_rations];
    const float paid  = t0 - f.w.nations.at(f.a).treasury;
    check(r.stations_fed == 1 && drawn == 2.0f, "L1", fmt("rations drawn = crew aboard x 1: %.2f", drawn));
    check(std::fabs(paid - 2.0f * 13.6f) < 0.01f && std::fabs(r.rations_spent - 27.2f) < 0.001f, "L1b", fmt("paid at the posted price: %.2f", paid));
    check(pool(f.w, f.a) == 50 * k_milli_per_rp, "L1c", "fed: the Research module works and earns");

    f.w.markets.at(f.market).inventory[k_rations] = 0.0f;
    const std::int64_t p0 = pool(f.w, f.a);
    const float t1 = f.w.nations.at(f.a).treasury;
    const auto r2 = step(f.w, 2);
    check(r2.stations_starved == 1 && pool(f.w, f.a) == p0 && f.w.nations.at(f.a).treasury == t1, "L1d",
          "an empty shelf starves the station: no RP, no debit");
    check(space_node_covered(f.w, f.a, {f.home, orbital_node_kind::surface}), "L1e",
          "...while its crewless relay still covers (the home surface, one hop from low orbit)");

    fixture g; build(g);
    stage_station(g.w, g.a, {g.home, orbital_node_kind::low_orbit}, 2,
                  {station_module_kind::power, station_module_kind::life_support, station_module_kind::research});
    g.w.nations.at(g.a).treasury = 1.0f;
    const auto r3 = step(g.w);
    check(r3.stations_starved == 1 && g.w.markets.at(g.market).inventory[k_rations] == 100.0f
              && pool(g.w, g.a) == 0, "L1f", "a treasury that cannot pay starves it too, nothing drawn");

    fixture h; build(h);
    stage_station(h.w, h.a, {h.home, orbital_node_kind::low_orbit}, 2,
                  {station_module_kind::power, station_module_kind::research});
    const auto r4 = step(h.w);
    check(r4.stations_fed == 0 && h.w.markets.at(h.market).inventory[k_rations] == 100.0f, "L1g",
          "no Life support module: no rations bought (the core's own life support needs none)");
}

void rows_factory()
{
    std::printf("\nF microgravity factory\n");
    fixture f; build(f);
    // crew need 1 + 2 = 3: habitation for room, life support for life.
    stage_station(f.w, f.a, {f.home, orbital_node_kind::low_orbit}, 3,
                  {station_module_kind::power, station_module_kind::habitation,
                   station_module_kind::life_support, station_module_kind::microgravity_factory});
    const float t0 = f.w.nations.at(f.a).treasury;
    const auto r = step(f.w);
    const market_component& m = f.w.markets.at(f.market);
    const float want = -3.0f * 13.6f - 20.0f * 9.6f + 10.0f * 41.6f;
    check(r.factory_runs == 1 && m.inventory[k_silicon] == 80.0f && m.inventory[k_elec] == 10.0f, "F1",
          fmt("a lot of 20 silicon bought, 10 electronics sold (shelf %.0f / %.0f)",
              m.inventory[k_silicon], m.inventory[k_elec]));
    check(std::fabs((f.w.nations.at(f.a).treasury - t0) - want) < 0.01f, "F1b",
          fmt("treasury moves by rations + input - output at posted prices: %.2f", f.w.nations.at(f.a).treasury - t0));

    f.w.markets.at(f.market).inventory[k_silicon] = 0.0f;
    const auto r2 = step(f.w, 2);
    check(r2.factory_runs == 0 && f.w.markets.at(f.market).inventory[k_elec] == 10.0f, "F1c",
          "no silicon on the shelf: the factory idles, nothing sold");
}

void rows_survey()
{
    std::printf("\nS survey satellite / O observatory\n");
    {
        fixture f; build(f);
        stage_sat(f.w, f.a, {f.moon, orbital_node_kind::low_orbit}, space_satellite_role::survey);
        const auto r = step(f.w);
        check(r.survey_regions == 0 && f.w.bodies.at(f.moon).survey.phase == survey_phase::hidden, "S1",
              "uncovered: nothing mapped, the moon stays hidden");
        stage_sat(f.w, f.a, {f.moon, orbital_node_kind::high_orbit}, space_satellite_role::comms);
        const auto r2 = step(f.w, 2);
        const survey_state& s = f.w.bodies.at(f.moon).survey;
        check(r2.survey_regions == 4 && s.phase == survey_phase::scanning && s.regions_done == 4
                  && s.regions_total == 8, "S1b",
              fmt("covered: hidden -> scanning, %d of %d regions", s.regions_done, s.regions_total));
        step(f.w, 3);
        check(f.w.bodies.at(f.moon).survey.phase == survey_phase::surveyed, "S1c", "the last region surveys it");
        const auto r4 = step(f.w, 4);
        check(r4.survey_regions == 0, "S1d", "a surveyed body maps nothing more");
    }
    {
        fixture f; build(f);
        stage_station(f.w, f.a, {f.home, orbital_node_kind::low_orbit}, 2, {station_module_kind::observatory});
        auto r = step(f.w);
        check(r.observatory_regions == 0, "O1", "only the home world in coverage: nothing to map");
        stage_sat(f.w, f.a, {f.moon, orbital_node_kind::low_orbit}, space_satellite_role::comms);
        r = step(f.w, 2);
        check(r.observatory_regions == 2 && f.w.bodies.at(f.moon).survey.regions_done == 2, "O1b",
              "the moon in coverage: the Observatory maps 2 regions");
        check(f.w.bodies.at(f.far).survey.phase == survey_phase::hidden, "O1c", "a body out of coverage is untouched");
    }
}

void rows_reads()
{
    std::printf("\nN navigation / V observation / A activity fog\n");
    {
        fixture f; build(f);
        check(space_navigation_factor(f.w, f.a, f.home) == 1.0f, "N1", "no satellite: factor 1");
        stage_sat(f.w, f.a, {f.home, orbital_node_kind::high_orbit}, space_satellite_role::navigation);
        check(space_navigation_factor(f.w, f.a, f.home) == 0.9f, "N1b", "an active Navigation satellite: 0.9 on its body");
        check(space_navigation_factor(f.w, f.b, f.home) == 1.0f && space_navigation_factor(f.w, f.a, f.moon) == 1.0f,
              "N1c", "1 for another nation, 1 on another body");
        stage_sat(f.w, f.a, {f.home, orbital_node_kind::low_orbit}, space_satellite_role::navigation);
        check(space_navigation_factor(f.w, f.a, f.home) == 0.9f, "N1d", "two do not stack");
        fixture g; build(g);
        stage_sat(g.w, g.a, {g.home, orbital_node_kind::high_orbit}, space_satellite_role::navigation,
                  space_asset_state::decaying);
        check(space_navigation_factor(g.w, g.a, g.home) == 1.0f, "N1e", "a decaying one: factor 1");
    }
    {
        fixture f; build(f);
        stage_sat(f.w, f.a, {f.moon, orbital_node_kind::low_orbit}, space_satellite_role::observation);
        check(!space_observation_lifts_vision(f.w, f.a, f.moon), "V1", "uncovered: the fog stays");
        stage_sat(f.w, f.a, {f.moon, orbital_node_kind::high_orbit}, space_satellite_role::comms);
        check(space_observation_lifts_vision(f.w, f.a, f.moon), "V1b", "covered: the fog lifts over the moon");
        check(!space_observation_lifts_vision(f.w, f.b, f.moon) && !space_observation_lifts_vision(f.w, f.a, f.home),
              "V1c", "for its owner only, over its body only");
    }
    {
        fixture f; build(f);
        trade_route tr;
        tr.body_a = f.home;
        tr.body_b = f.moon;
        tr.corp = f.a;
        tr.last_tick = 0;
        f.w.trade_routes.push_back(tr);
        const int now = 10000;
        const auto vis = [&] { return body_activity_visibility(f.w, f.moon, now); };
        check(vis() == activity_vis::known_stale, "A1", "a cold route: known_stale");
        const int node = comms_node_index();
        f.w.nation_tech[f.a].held.set(static_cast<std::size_t>(node));
        check(vis() == activity_vis::known_stale && !space_comms_lifts_activity_fog(f.w, f.a), "A1b",
              "the technology alone lifts nothing");
        f.w.nation_tech[f.a].held.reset(static_cast<std::size_t>(node));
        stage_sat(f.w, f.a, {f.home, orbital_node_kind::high_orbit}, space_satellite_role::comms);
        check(vis() == activity_vis::known_stale && !space_comms_lifts_activity_fog(f.w, f.a), "A1c",
              "the satellite alone lifts nothing");
        f.w.nation_tech[f.a].held.set(static_cast<std::size_t>(node));
        check(vis() == activity_vis::visible && space_comms_lifts_activity_fog(f.w, f.a), "A1d",
              "both: the market it trades with reads visible (world.cpp twin agrees)");
        check(body_activity_visibility(f.w, f.far, now) == activity_vis::unknown, "A1e",
              "a body it does not trade with stays unknown");
    }
}

void row_navigation_generated()
{
    std::printf("\nN2 navigation on a generated world\n");
    world w = make_hard_coded_world();
    recipe_registry reg;
    const logistics_nodes nodes = collect_logistics_nodes(w);
    std::vector<entity_id> mk;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == w.home_body && mc.centre_tile != null_entity)
            mk.push_back(mid);
    const std::size_t ri = static_cast<std::size_t>(resource_type::food_rations);
    entity_id nat = null_entity, other = null_entity, m1 = null_entity, m2 = null_entity;
    float before = 0.0f, other_before = 0.0f;
    for (const auto& [nid, nc] : w.nations)
    {
        (void)nc;
        for (std::size_t i = 0; i + 1 < mk.size() && m1 == null_entity; ++i)
        {
            const convoy_leg l = price_trade_leg(w, reg, nodes, nid, mk[i], mk[i + 1], ri, 10.0f);
            if (l.viable && l.cost > 0.0f)
            {
                nat = nid; m1 = mk[i]; m2 = mk[i + 1]; before = l.cost;
            }
        }
        if (nat != null_entity)
            break;
    }
    for (const auto& [nid, nc] : w.nations)
    {
        (void)nc;
        if (nid == nat || m1 == null_entity)
            continue;
        const convoy_leg l = price_trade_leg(w, reg, nodes, nid, m1, m2, ri, 10.0f);
        if (l.viable) { other = nid; other_before = l.cost; break; }
    }
    check(nat != null_entity, "N2", fmt("found a viable intra-body leg: %.4f credits", before));
    if (nat == null_entity)
        return;
    stage_sat(w, nat, {w.home_body, orbital_node_kind::high_orbit}, space_satellite_role::navigation);
    const convoy_leg after = price_trade_leg(w, reg, nodes, nat, m1, m2, ri, 10.0f);
    check(after.viable && after.cost < before && after.cost >= 0.9f * before - 1e-4f, "N2b",
          fmt("with a Navigation satellite: %.4f -> %.4f (x%.4f)", before, after.cost, after.cost / before));
    if (other != null_entity)
    {
        const convoy_leg o = price_trade_leg(w, reg, nodes, other, m1, m2, ri, 10.0f);
        check(o.viable && o.cost == other_before, "N2c", fmt("another nation's leg unchanged: %.4f", o.cost));
    }
}

void programme(fixture& f)
{
    world& w = f.w;
    stage_sat(w, f.a, {f.moon, orbital_node_kind::low_orbit}, space_satellite_role::survey);
    stage_sat(w, f.a, {f.moon, orbital_node_kind::high_orbit}, space_satellite_role::comms);
    stage_station(w, f.a, {f.home, orbital_node_kind::low_orbit}, 3,
                  {station_module_kind::power, station_module_kind::habitation, station_module_kind::life_support,
                   station_module_kind::research, station_module_kind::observatory});
    stage_station(w, f.b, {f.moon, orbital_node_kind::l4}, 3,
                  {station_module_kind::power, station_module_kind::habitation, station_module_kind::life_support,
                   station_module_kind::microgravity_factory, station_module_kind::comms_relay});
    for (int t = 1; t <= 6; ++t)
        step(w, t);
}

void rows_inert_and_determinism()
{
    std::printf("\nZ inert / D determinism\n");
    {
        fixture f; build(f);
        const std::uint64_t h0 = f.w.state_hash(0);
        const std::string b0 = to_bytes(f.w);
        const auto r = step(f.w);
        check(f.w.state_hash(0) == h0 && to_bytes(f.w) == b0 && r.stations_fed == 0, "Z1",
              "no space assets: the step changes nothing");
    }
    fixture x; build(x); programme(x);
    fixture y; build(y); programme(y);
    check(x.w.state_hash(0) == y.w.state_hash(0) && to_bytes(x.w) == to_bytes(y.w), "D1",
          fmt("two identical programmes: equal hash %016llx and stream",
              static_cast<unsigned long long>(x.w.state_hash(0))));
    check(x.w.bodies.at(x.moon).survey.phase == survey_phase::surveyed && pool(x.w, x.a) > 0, "D1b",
          "the programme did something (moon surveyed, RP earned)");
}

} // namespace

int main()
{
    std::printf("space_effects_harness — BL-1587 / BL-1586 / BL-1584\n");
    rows_coverage();
    rows_research();
    rows_life_support();
    rows_factory();
    rows_survey();
    rows_reads();
    row_navigation_generated();
    rows_inert_and_determinism();
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
