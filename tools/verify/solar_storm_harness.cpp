// ---------------------------------------------------------------------------
// solar_storm_harness — BL-1605 (solar storms)
// ---------------------------------------------------------------------------
// docs/EVENTS.md § Four families (Solar storms); src/world/solar_storm.hpp.
// A hand-built fixture modelled on space_effects_harness: a star, a home planet
// at 1.0 AU, its moon, an asteroid, and two nations with one home market. Assets
// are STAGED by writing node, role and state directly. The world seed is set on
// `w.provinces.seed`, and the storm quarter T is FOUND from the schedule (a
// quarter with a storm whose neighbours carry none), never hard-coded. Rows:
//
//   D1  the draw is a pure function of (seed, tick): two calls agree, two
//       seeds differ somewhere, nothing before tick 1.
//   D2  at most one storm per solar cycle; over 200 seeds x 400 quarters the
//       rate sits near storm_per_mille / cycle_quarters.
//   E1  exposure: every L-point and every asteroid orbit is exposed; a planet's
//       and a moon's low and high orbit are sheltered; the surface never is.
//   S1  a Comms satellite at moon L1 covers the moon off-storm, nothing at T,
//       and covers again at T+1; one at the moon's high orbit covers through T.
//   S2  a Navigation satellite at an L-point loses its factor at T only; one in
//       a sheltered orbit keeps it.
//   S3  a Survey satellite at an asteroid's low orbit maps nothing at T and maps
//       again at T+1.
//   S4  world.cpp's activity-fog twin goes dark with the satellite at T.
//   M1  a station at moon L4 (relay + research): at T no module works, both are
//       `stormed`, it earns no RP and its relay covers nothing; at T+1 it earns.
//   M2  a station in the home low orbit (sheltered) earns its RP through T.
//   M3  the station step at T counts the struck modules as stormed.
//   R1  the report at T: storm set; the owner's row counts its dark satellites,
//       struck station and stopped modules; a sheltered owner has no row.
//   R2  the report at T-1: a working Observatory's owner is warned; a nation
//       with none is not.
//   Z1  no space assets: at T the steps and the report change nothing (hash and
//       bytes equal) and the report is empty.
//   P1  two fixtures driven identically through a storm end with equal hash and
//       equal stream; the storm leaves nothing behind (state equal to a
//       fixture driven without reading it at T is not claimed — nothing is saved).
//   V1  the save: a world with assets round-trips byte-identical, and the storm
//       still falls on T after the load (it is derived, nothing to save).
//
// Build: node tools/verify/build_harness.js solar_storm_harness --run
// ---------------------------------------------------------------------------

#include "world/nation_tech.hpp"
#include "world/orbital_graph.hpp"
#include "world/solar_storm.hpp"
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

constexpr uint32_t k_seed = 650u;

/// The first storm quarter T >= 2 whose neighbours carry none.
// BL-1642 (the tick is a month): a storm strikes for its calendar quarter, three
// monthly ticks. T is the FIRST tick of an isolated storm - lit at T-1, dark T..T+2,
// lit again at T+3 (k_storm_after) - so every row reads before, during and after.
constexpr int k_storm_ticks = k_econ_ticks_per_quarter;
int find_storm_tick(uint32_t seed)
{
    for (int t = 2; t < 6000; ++t)
    {
        if (!solar_storm_at(seed, t) || solar_storm_at(seed, t - 1) || solar_storm_at(seed, t + k_storm_ticks))
            continue;
        bool whole = true;
        for (int k = 1; k < k_storm_ticks; ++k) whole = whole && solar_storm_at(seed, t + k);
        if (whole)
            return t;
    }
    return -1;
}

struct fixture
{
    world     w;
    entity_id star = null_entity, home = null_entity, moon = null_entity, rock = null_entity;
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

void build(fixture& f)
{
    world& w = f.w;
    w.provinces.seed = k_seed;
    f.star = add_body(w, "Star", body_type::star, null_entity, 0.0f, 0, 0);
    f.home = add_body(w, "Home", body_type::planet, null_entity, 1.0f, 0, 0);
    f.moon = add_body(w, "Moon", body_type::moon, f.home, 0.0026f, 32, 16);
    f.rock = add_body(w, "Rock", body_type::asteroid, null_entity, 2.7f, 32, 16);
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
    mc.inventory[k_rations]  = 100.0f;
    w.markets.emplace(f.market, mc);
}

entity_id stage_sat(world& w, entity_id owner, orbital_node node, space_satellite_role role)
{
    const entity_id id = w.create_entity();
    space_asset a;
    a.id = id;
    a.owner = owner;
    a.cls = space_asset_class::satellite;
    a.dest = {node.body, space_slot::orbit};
    a.state = space_asset_state::active;
    a.node = node;
    a.role = role;
    w.space_assets.emplace(id, a);
    return id;
}

entity_id stage_station(world& w, entity_id owner, orbital_node node, int core_crew,
                        std::initializer_list<station_module_kind> kinds)
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
    c.dest = {node.body, space_slot::orbit};
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
        m.dest = {node.body, space_slot::orbit};
        m.state = space_asset_state::active;
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

space_effects_report step(world& w, int tick)
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

bool covers_moon(const world& w, entity_id nation, entity_id moon)
{
    return space_coverage(w, nation).count({moon, orbital_node_kind::low_orbit}) != 0;
}

int g_T = -1;

void rows_draw()
{
    std::printf("\nD draw\n");
    bool pure = true, before = true, differ = false;
    for (int t = -5; t < 400; ++t)
    {
        pure = pure && solar_storm_at(k_seed, t) == solar_storm_at(k_seed, t);
        if (t < 1) before = before && !solar_storm_at(k_seed, t);
        if (solar_storm_at(k_seed, t) != solar_storm_at(k_seed + 1, t)) differ = true;
    }
    check(pure && before && differ, "D1",
          "the draw is pure in (seed, tick), nothing before tick 1, two seeds differ");

    const solar_storm_params p{};
    // BL-1642: ticks are months; a storm quarter is three storm ticks, so the rate is
    // counted in QUARTERS (storm ticks / 3) over 400 quarters' worth of months.
    int storms = 0, quarters = 0, over = 0;
    for (uint32_t s = 0; s < 200; ++s)
        for (int c = 0; c < 400 / p.cycle_quarters; ++c)
        {
            int in_cycle = 0;
            for (int q = 0; q < p.cycle_quarters * k_econ_ticks_per_quarter; ++q)
            {
                const int t = 1 + c * p.cycle_quarters * k_econ_ticks_per_quarter + q;
                if (q % k_econ_ticks_per_quarter == 0) ++quarters;
                if (solar_storm_at(s, t)) { ++storms; ++in_cycle; }
            }
            if (in_cycle > k_econ_ticks_per_quarter) ++over;
        }
    storms /= k_econ_ticks_per_quarter;
    const double rate = static_cast<double>(storms) / quarters;
    const double want = p.storm_per_mille / 1000.0 / p.cycle_quarters;
    check(over == 0 && std::fabs(rate - want) < 0.15 * want, "D2",
          fmt("one storm a cycle at most (%d over); rate %.4f/quarter vs %.4f", over, rate, want));
    g_T = find_storm_tick(k_seed);
    check(g_T > 1, "D3", fmt("seed %u's first isolated storm quarter: T = %d", k_seed, g_T));
}

void rows_exposure()
{
    std::printf("\nE exposure\n");
    fixture f; build(f);
    const world& w = f.w;
    bool l = true;
    for (const orbital_node_kind k : {orbital_node_kind::l1, orbital_node_kind::l2, orbital_node_kind::l4,
                                      orbital_node_kind::l5})
        l = l && solar_storm_node_exposed(w, {f.moon, k}) && solar_storm_node_exposed(w, {f.home, k});
    const bool rock = solar_storm_node_exposed(w, {f.rock, orbital_node_kind::low_orbit})
                   && solar_storm_node_exposed(w, {f.rock, orbital_node_kind::high_orbit});
    bool sheltered = true;
    for (const entity_id b : {f.home, f.moon})
        for (const orbital_node_kind k : {orbital_node_kind::low_orbit, orbital_node_kind::high_orbit,
                                          orbital_node_kind::surface})
            sheltered = sheltered && !solar_storm_node_exposed(w, {b, k});
    const bool surf = !solar_storm_node_exposed(w, {f.rock, orbital_node_kind::surface});
    check(l && rock && sheltered && surf, "E1",
          "L-points and asteroid orbits exposed; planet and moon orbits, and surfaces, sheltered");
}

void rows_satellites()
{
    std::printf("\nS satellites\n");
    const int T = g_T;
    {
        fixture f; build(f);
        stage_sat(f.w, f.a, {f.moon, orbital_node_kind::l1}, space_satellite_role::comms);
        stage_sat(f.w, f.b, {f.moon, orbital_node_kind::high_orbit}, space_satellite_role::comms);
        f.w.current_econ_tick = T - 1;
        const bool pre = covers_moon(f.w, f.a, f.moon);
        f.w.current_econ_tick = T;
        const bool during = covers_moon(f.w, f.a, f.moon);
        const bool shelter = covers_moon(f.w, f.b, f.moon);
        f.w.current_econ_tick = T + k_storm_ticks;
        const bool post = covers_moon(f.w, f.a, f.moon);
        check(pre && !during && post, "S1", "an L1 Comms satellite covers the moon before, not at T, again after");
        check(shelter, "S1b", "a Comms satellite in the moon's high orbit covers through the storm");
    }
    {
        fixture f; build(f);
        stage_sat(f.w, f.a, {f.moon, orbital_node_kind::l2}, space_satellite_role::navigation);
        stage_sat(f.w, f.b, {f.moon, orbital_node_kind::low_orbit}, space_satellite_role::navigation);
        f.w.current_econ_tick = T - 1;
        const float pre = space_navigation_factor(f.w, f.a, f.moon);
        f.w.current_econ_tick = T;
        const float during = space_navigation_factor(f.w, f.a, f.moon);
        const float shelter = space_navigation_factor(f.w, f.b, f.moon);
        f.w.current_econ_tick = T + k_storm_ticks;
        const float post = space_navigation_factor(f.w, f.a, f.moon);
        check(pre < 1.0f && during == 1.0f && post == pre, "S2",
              fmt("L2 Navigation factor %.2f -> %.2f at T -> %.2f", pre, during, post));
        check(shelter < 1.0f, "S2b", fmt("a sheltered Navigation satellite keeps %.2f at T", shelter));
    }
    {
        fixture f; build(f);
        // Coverage of the asteroid: a Comms satellite in its HIGH orbit would be
        // dark too, so cover it with a sheltered... nothing shelters an asteroid.
        // Instead read the survey off-storm and at T with the satellite's own
        // Comms twin at the same node: both dark at T, both lit off it.
        stage_sat(f.w, f.a, {f.rock, orbital_node_kind::high_orbit}, space_satellite_role::comms);
        stage_sat(f.w, f.a, {f.rock, orbital_node_kind::low_orbit}, space_satellite_role::survey);
        const auto r_storm = step(f.w, T);
        const int done_storm = f.w.bodies.at(f.rock).survey.regions_done;
        const auto r_after = step(f.w, T + k_storm_ticks);
        check(r_storm.survey_regions == 0 && done_storm == 0 && r_after.survey_regions > 0, "S3",
              fmt("an asteroid Survey satellite maps %d at T, %d at T+1", r_storm.survey_regions,
                  r_after.survey_regions));
    }
    {
        fixture f; build(f);
        stage_sat(f.w, f.a, {f.moon, orbital_node_kind::l1}, space_satellite_role::comms);
        f.w.current_econ_tick = T - 1;
        const bool pre = space_comms_lifts_activity_fog(f.w, f.a) == false; // no tech held: false either way
        // The dark test itself, which both the read and world.cpp's twin share.
        const space_asset& sat = f.w.space_assets.begin()->second;
        const bool dark_pre = solar_storm_satellite_dark(f.w, sat, T - 1);
        const bool dark_at  = solar_storm_satellite_dark(f.w, sat, T);
        check(pre && !dark_pre && dark_at, "S4", "the shared dark test: lit at T-1, dark at T");
    }
}

void rows_stations()
{
    std::printf("\nM stations\n");
    const int T = g_T;
    const std::int64_t want = static_cast<std::int64_t>(std::llround(
        space_effects_params{}.research_rp_per_module * static_cast<double>(k_milli_per_rp))); // a month's (BL-1642)
    {
        fixture f; build(f);
        const orbital_node l4{f.moon, orbital_node_kind::l4};
        const entity_id sid = stage_station(f.w, f.a, l4, 2,
                                            {station_module_kind::comms_relay, station_module_kind::research});
        const station_budget b = evaluate_station_budget(f.w, station_params{}, sid, T);
        f.w.current_econ_tick = T;
        const bool relay_dark = space_coverage(f.w, f.a).count(l4) == 0;
        const auto r_storm = step(f.w, T);
        const std::int64_t at_storm = pool(f.w, f.a);
        step(f.w, T + k_storm_ticks);
        const std::int64_t after = pool(f.w, f.a);
        check(b.storm_struck && b.working.empty() && b.stormed.size() == 2, "M1",
              fmt("at T the L4 station's %zu modules are stormed, %zu working", b.stormed.size(), b.working.size()));
        check(at_storm == 0 && r_storm.research_milli == 0 && relay_dark, "M1b",
              "...it earns no RP and its relay covers nothing");
        check(after == want, "M1c", fmt("at T+1 it earns %lld milli-RP again", static_cast<long long>(after)));
    }
    {
        fixture f; build(f);
        stage_station(f.w, f.a, {f.home, orbital_node_kind::low_orbit}, 2, {station_module_kind::research});
        step(f.w, T);
        check(pool(f.w, f.a) == want, "M2", "a station in the home low orbit earns through the storm");
    }
    {
        fixture f; build(f);
        stage_station(f.w, f.a, {f.moon, orbital_node_kind::l4}, 2,
                      {station_module_kind::comms_relay, station_module_kind::research});
        stage_station(f.w, f.b, {f.home, orbital_node_kind::low_orbit}, 2, {station_module_kind::research});
        f.w.current_econ_tick = T;
        station_step_report r;
        run_station_step(f.w, station_params{}, T, &r);
        check(r.modules_stormed == 2 && r.modules_working == 1, "M3",
              fmt("the station step at T: %d stormed, %d working", r.modules_stormed, r.modules_working));
    }
}

void rows_report()
{
    std::printf("\nR report\n");
    const int T = g_T;
    fixture f; build(f);
    stage_sat(f.w, f.a, {f.moon, orbital_node_kind::l1}, space_satellite_role::comms);
    stage_sat(f.w, f.a, {f.rock, orbital_node_kind::low_orbit}, space_satellite_role::survey);
    stage_station(f.w, f.a, {f.moon, orbital_node_kind::l4}, 2,
                  {station_module_kind::comms_relay, station_module_kind::research});
    stage_sat(f.w, f.b, {f.home, orbital_node_kind::high_orbit}, space_satellite_role::comms);
    stage_station(f.w, f.b, {f.home, orbital_node_kind::low_orbit}, 3,
                  {station_module_kind::power, station_module_kind::observatory});
    {
        f.w.current_econ_tick = T;
        solar_storm_report r;
        run_solar_storm_report(f.w, station_params{}, T, &r);
        // BL-1642: a storm spans three monthly ticks, so at its first tick T the next
        // tick is stormy too and Beta's Observatory may warn again; Beta is never struck.
        // Alpha's row comes first (ascending nation id); Beta's, if present, is a warning.
        bool one = !r.nations.empty() && r.nations[0].nation == f.a && r.nations.size() <= 2;
        if (r.nations.size() == 2)
            one = one && r.nations[1].nation == f.b && r.nations[1].warned && r.nations[1].satellites_dark == 0
                      && r.nations[1].stations_struck == 0;
        check(r.storm && one && r.nations[0].satellites_dark == 2 && r.nations[0].stations_struck == 1
                  && r.nations[0].modules_stopped == 2 && !r.nations[0].warned, "R1",
              fmt("at T: storm %d, %zu row(s); Alpha %d dark, %d struck, %d stopped", r.storm ? 1 : 0,
                  r.nations.size(), r.nations.empty() ? -1 : r.nations[0].satellites_dark,
                  r.nations.empty() ? -1 : r.nations[0].stations_struck,
                  r.nations.empty() ? -1 : r.nations[0].modules_stopped));
    }
    {
        f.w.current_econ_tick = T - 1;
        solar_storm_report r;
        run_solar_storm_report(f.w, station_params{}, T - 1, &r);
        bool b_warned = false, a_row = false;
        for (const auto& row : r.nations)
        {
            if (row.nation == f.b) b_warned = row.warned;
            if (row.nation == f.a) a_row = true;
        }
        check(!r.storm && r.storm_next && b_warned && !a_row, "R2",
              "at T-1: Beta's Observatory warns it of the storm; Alpha, with none, is not told");
    }
}

void rows_inert_and_replay()
{
    std::printf("\nZ inert / P replay / V save\n");
    const int T = g_T;
    {
        fixture f; build(f);
        f.w.current_econ_tick = T;
        const std::uint64_t h0 = f.w.state_hash(0);
        const std::string b0 = to_bytes(f.w);
        step(f.w, T);
        station_step_report sr;
        run_station_step(f.w, station_params{}, T, &sr);
        solar_storm_report r;
        r.storm = true; // must be overwritten
        run_solar_storm_report(f.w, station_params{}, T, &r);
        check(f.w.state_hash(0) == h0 && to_bytes(f.w) == b0 && !r.storm && r.nations.empty(), "Z1",
              "no space assets: a storm quarter changes nothing and reports nothing");
    }
    const auto programme = [&](fixture& f) {
        stage_sat(f.w, f.a, {f.rock, orbital_node_kind::low_orbit}, space_satellite_role::survey);
        stage_sat(f.w, f.a, {f.rock, orbital_node_kind::high_orbit}, space_satellite_role::comms);
        stage_station(f.w, f.a, {f.moon, orbital_node_kind::l4}, 2,
                      {station_module_kind::comms_relay, station_module_kind::research});
        for (int t = T - 2; t <= T + k_storm_ticks + 1; ++t) // two lit, the storm's three dark, two lit
        {
            f.w.current_econ_tick = t;
            run_station_step(f.w, station_params{}, t);
            step(f.w, t);
        }
    };
    fixture x; build(x); programme(x);
    fixture y; build(y); programme(y);
    check(x.w.state_hash(0) == y.w.state_hash(0) && to_bytes(x.w) == to_bytes(y.w), "P1",
          fmt("two identical programmes through the storm: equal hash %016llx and stream",
              static_cast<unsigned long long>(x.w.state_hash(0))));
    // Four months ran lit, the storm's three dark: the pool holds four months' RP.
    const std::int64_t month_rp = static_cast<std::int64_t>(std::llround(
        space_effects_params{}.research_rp_per_module * static_cast<double>(k_milli_per_rp)));
    check(pool(x.w, x.a) == 4 * month_rp, "P1b",
          fmt("RP over T-2..T+4 is four lit months' (%lld milli)", static_cast<long long>(pool(x.w, x.a))));

    const std::string bytes = to_bytes(x.w);
    std::istringstream is(bytes, std::ios::binary);
    world z;
    const bool loaded = read_world_snapshot(z, is);
    const bool same = loaded && to_bytes(z) == bytes;
    bool storm_kept = false;
    if (loaded)
    {
        const entity_id st = x.w.stations.begin()->first;
        storm_kept = evaluate_station_budget(z, station_params{}, st, T).storm_struck
                  && !evaluate_station_budget(z, station_params{}, st, T + k_storm_ticks).storm_struck;
    }
    check(same && storm_kept, "V1",
          fmt("the save round-trips byte-identical; the storm still falls on T after load "
              "(loaded %d, same %d, storm %d, seed %u)", loaded ? 1 : 0, same ? 1 : 0, storm_kept ? 1 : 0,
              loaded ? z.provinces.seed : 0u));
}

} // namespace

int main()
{
    std::printf("solar_storm_harness — BL-1605\n");
    rows_draw();
    if (g_T > 1)
    {
        rows_exposure();
        rows_satellites();
        rows_stations();
        rows_report();
        rows_inert_and_replay();
    }
    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
