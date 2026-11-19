// ---------------------------------------------------------------------------
// station_harness — BL-1583 (station model) + BL-1585 (astronauts), the proof
// ---------------------------------------------------------------------------
// docs/economy/SPACE_ASSETS.md § Space stations, § Astronauts. A hand-built
// fixture: a star, a home planet at 1.0 AU, its moon, a far planet at 2.0 AU, and
// two nations on the home planet. Rows:
//
//   S1  FOUND: a core launches through the space-asset path (treasury debited the
//       quote), draws its crew from the pool, opens a 5x3 grid with the core central.
//   S2  CREWLESS: a nation whose pool cannot crew the core is refused whole —
//       hash, treasury, allocator, stores untouched; a surface node is refused too.
//   S3  FIT RULES: a module fills an empty slot (crew drawn); an occupied slot, an
//       edge-only module off the outer ring, an unknown station, another nation's
//       station, an out-of-range slot, a truss and a crewless module are each
//       refused whole.
//   S4  REMOVE: frees the slot, refunds nothing; the crew stay aboard on the core.
//   S5  REFIT: launch-free (no debit, no new asset), the module idle for one
//       quarter in its new slot, working the next.
//   S6  BUDGET: a power shortfall stops the NEWEST-fitted module first (fit order,
//       not cost); a crew-room shortfall likewise; the rest keep working.
//   S7  SOLAR: a station at 2.0 AU sees (1/2)^2 of its power; a moon station reads
//       its planet's radius (factor 1).
//   S8  UPGRADE: 5x3 -> 7x5 -> 9x7, a truss launch each, centre preserved, modules
//       keep their place relative to the core; an enclosed dock goes idle and a
//       refit to an edge brings it back; level 3 is refused.
//   S9  TEMPLATE: saved and validated; applied, it fits every unfilled slot as its
//       own launch, skips a matching slot silently, skips and reports a slot holding
//       a different module and a slot it cannot crew.
//   S10 TRAINING: a staffed, completed centre adds astronauts per quarter up to its
//       cap; an unstaffed or unfinished one adds none; through run_nation_step too.
//   S11 LOSS: an unpaid core is lost and its station with it: every module lost,
//       its crew lost, the pool untouched; a lost module frees its slot.
//   S12 SAVE: a fitted, upgraded, templated station world with astronauts
//       round-trips (equal hash, byte-identical re-serialise); the v80 prefix of a
//       station-free world reads with no templates or pools; a v80 stream carrying
//       a station_core is refused; tampered tails refuse the stream whole.
//   S13 DETERMINISM: two fixtures driven identically end with equal hash and stream.
//
// BL-1581 / BL-1582 / BL-1587 (sprint 65): the fixture stages a nation that may
// launch (space_fixture.hpp). S2 now refuses a core without its key and admits a
// far L4; S6's overdraw is a Microgravity factory (the Shipyard's key is far
// space's); S8's 9x7 ring is staged (its key is closed in Era 1). New rows:
//   S14 KEYS: a module kind's key, the Shipyard's closed key, the 7x5 upgrade's
//       key — each refused whole without it; the Astronaut Training Centre's
//       construction opens only with astronaut_training.
//   S15 SLOTS: a core holds one slot of its node; a second core to the full
//       node is refused whole; a module rides inside the core's slot.
//   S16 SIGNAL DELAY: station verbs in coverage land at once; outside it they
//       queue, round-trip the save, wait exactly one quarter and land in issue
//       order; a no-longer-legal order is dropped and reported; templates save
//       at once; malformed commands refuse whole.
//
// Build: node tools/verify/build_harness.js station_harness
// Run:   build_gen/verify/station_harness.exe
// ---------------------------------------------------------------------------

#include "world/economy_system.hpp"
#include "world/nation_step.hpp"
#include "world/recipe_registry.hpp"
#include "world/nation_tech.hpp"
#include "world/orbital_graph.hpp"
#include "world/space_assets.hpp"
#include "world/space_command.hpp"
#include "world/space_tech_keys.hpp"
#include "world/stations.hpp"
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
    entity_id a = null_entity, b = null_entity;
    entity_id market = null_entity; // the home shelf (BL-1582)
};

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
    f.far  = add_body(w, "Far", body_type::planet, null_entity, 2.0f);
    w.star_body = f.star;
    w.home_body = f.home;
    for (entity_id* n : {&f.a, &f.b})
    {
        *n = w.create_entity();
        nation_component nc;
        nc.name = (n == &f.a) ? "Alpha" : "Beta";
        w.nations.emplace(*n, nc);
    }
    w.nations.at(f.a).treasury = 2000000.0f;
    w.nations.at(f.b).treasury = 2000000.0f;
    w.nations.at(f.a).astronauts = 20;
    w.nations.at(f.b).astronauts = 0;
    // BL-1581: a one-Earth-mass home under moderate air; a Luna-mass airless moon.
    w.bodies.at(f.moon).mass_earths = 0.0123f;
    w.bodies.at(f.moon).atmosphere  = atmosphere_class::none;
    // BL-1582: both may launch — every node held, a staffed pad each, a stocked shelf.
    space_fixture::hold_every_node(w, f.a);
    space_fixture::hold_every_node(w, f.b);
    const entity_id tile = space_fixture::add_tile(w, f.home);
    space_fixture::add_pad(w, f.a, tile, 10.0f);
    space_fixture::add_pad(w, f.b, tile, 10.0f);
    f.market = space_fixture::add_shelf(w, f.home);
}

struct snap
{
    std::uint64_t hash;
    std::size_t   assets, stations, templates, orders;
    std::uint32_t cursor;
    float         ta, tb;
    int32_t       pa, pb;
    float         comp, prop; // BL-1582: the home shelf's launch goods
};

snap take(const fixture& f)
{
    return {f.w.state_hash(0), f.w.space_assets.size(), f.w.stations.size(), f.w.station_templates.size(),
            f.w.space_orders.size(), f.w.next_entity_id(), f.w.nations.at(f.a).treasury,
            f.w.nations.at(f.b).treasury, f.w.nations.at(f.a).astronauts, f.w.nations.at(f.b).astronauts,
            space_fixture::shelf_of(f.w, f.market, resource_type::spacecraft_components),
            space_fixture::shelf_of(f.w, f.market, resource_type::propellant)};
}

bool same(const snap& x, const snap& y)
{
    return x.hash == y.hash && x.assets == y.assets && x.stations == y.stations
        && x.templates == y.templates && x.orders == y.orders && x.cursor == y.cursor && x.ta == y.ta
        && x.tb == y.tb && x.pa == y.pa && x.pb == y.pb && x.comp == y.comp && x.prop == y.prop;
}

bool contains(const std::vector<entity_id>& v, entity_id e)
{
    return std::find(v.begin(), v.end(), e) != v.end();
}

/// Step the space assets then the stations for one quarter, as the nation step does.
void quarter(world& w, const space_asset_params& sp, const station_params& stp, int tick,
             station_step_report* r = nullptr)
{
    w.current_econ_tick = tick;
    run_space_asset_step(w, sp, tick);
    run_station_step(w, stp, tick, r);
}

int slot_of(int level, int c, int r) { return r * station_grid_width(level) + c; }

entity_id add_centre(world& w, entity_id nation, float staffed, int ticks_remaining)
{
    const entity_id bid = w.create_entity();
    building_component b{};
    b.tile = null_entity;
    b.type = building_type::astronaut_training_centre;
    b.workforce_assigned = staffed;
    b.ticks_remaining = ticks_remaining;
    w.buildings.emplace(bid, b);
    w.nations.at(nation).state_assets.push_back(bid);
    return bid;
}

/// Drive a fixture through a scripted programme (S13 runs it twice).
void programme(fixture& f, const space_asset_params& sp, const station_params& stp)
{
    world& w = f.w;
    w.current_econ_tick = 3;
    const station_op s = found_station(w, sp, stp, f.a, {f.home, orbital_node_kind::high_orbit});
    fit_station_module(w, sp, stp, f.a, s.station, slot_of(0, 0, 0), station_module_kind::power);
    fit_station_module(w, sp, stp, f.a, s.station, slot_of(0, 1, 0), station_module_kind::dock);
    fit_station_module(w, sp, stp, f.a, s.station, slot_of(0, 3, 1), station_module_kind::research);
    station_template t;
    t.level = 0;
    t.kinds.assign(15, k_no_station_module);
    t.kinds[static_cast<std::size_t>(slot_of(0, 4, 2))] = static_cast<uint8_t>(station_module_kind::habitation);
    save_station_template(w, f.a, "Outpost", t);
    for (int q = 3; q < 6; ++q)
        quarter(w, sp, stp, q);
    w.current_econ_tick = 6;
    upgrade_station(w, sp, f.a, s.station);
    apply_station_template(w, sp, stp, f.a, s.station, "Outpost");
    for (int q = 6; q < 9; ++q)
        quarter(w, sp, stp, q);
}

} // namespace

int main()
{
    const space_asset_params sp{};
    const station_params     stp{};
    std::printf("station_harness — BL-1583 + BL-1585\n");
    std::printf("dials: core power %.1f crew %d room %d life %d; refit %d q; training %d/q cap %d/centre;\n"
                "       core launch %.0f upkeep %.0f; module launch %.0f upkeep %.0f\n",
                stp.core_power_supply, stp.core_crew_needed, stp.core_crew_room, stp.core_life_support,
                stp.refit_quarters, stp.astronauts_per_centre, stp.astronaut_cap_per_centre,
                sp.launch_cost[4], sp.upkeep[4], sp.launch_cost[3], sp.upkeep[3]);

    // ---- S1 found ---------------------------------------------------------
    std::printf("--- S1 found a station ---\n");
    fixture f;
    build(f);
    world& w = f.w;
    w.current_econ_tick = 10;
    const orbital_node hi{f.home, orbital_node_kind::high_orbit};
    const space_launch_quote cq = quote_space_launch(w, sp, f.a, space_asset_class::station_core, hi);
    const float t0 = w.nations.at(f.a).treasury;
    const station_op s1 = found_station(w, sp, stp, f.a, hi);
    entity_id st_id = s1.station;
    {
        const bool ok = s1.result == station_result::ok && w.stations.count(st_id) == 1;
        check(ok, "S1", "found_station returns ok and stores the station");
        if (!ok)
        {
            std::printf("  aborting: no station\n");
            return 1;
        }
        const space_station& st = w.stations.at(st_id);
        const space_asset& core = w.space_assets.at(st.core);
        check(st.core == st_id && core.cls == space_asset_class::station_core && core.station == st_id
                  && st.width() == 5 && st.height() == 3 && st.centre() == 7
                  && st.slots[7] == st_id && core.station_slot == 7,
              "S1", "a station_core asset; grid 5x3; the core in the centre slot (7)");
        check(std::count(st.slots.begin(), st.slots.end(), null_entity) == 14, "S1", "14 empty slots remain");
        check(w.nations.at(f.a).treasury == t0 - cq.cost && core.launch_cost == cq.cost,
              "S1", fmt("the treasury is debited exactly the space-asset quote (%.0f)", cq.cost));
        check(w.nations.at(f.a).astronauts == 20 - stp.core_crew_needed && core.crew == stp.core_crew_needed,
              "S1", fmt("the core's crew (%d) leave the pool (20 -> %d) and ride the asset",
                        stp.core_crew_needed, w.nations.at(f.a).astronauts));
        check(st.node == hi && core.dest.body == f.home && core.state == space_asset_state::in_transit,
              "S1", "the station sits at the home high orbit; the core is in transit");
    }

    // ---- S2 crewless ------------------------------------------------------
    std::printf("--- S2 crewless and illegal founds are refused whole ---\n");
    {
        const snap before = take(f);
        const station_op r = found_station(w, sp, stp, f.b, hi);
        check(r.result == station_result::rejected_no_crew && same(before, take(f)), "S2",
              "Beta (0 astronauts) founding a station: rejected_no_crew, nothing moved (hash, treasury, allocator, stores)");
        const station_op r2 = found_station(w, sp, stp, f.a, {f.home, orbital_node_kind::surface});
        check(r2.result == station_result::rejected_invalid && same(before, take(f)), "S2",
              "a surface node is refused whole");
        {
            // BL-1582: without the station core's key the core is refused through the
            // launch quote, whole (a copy of the fixture, the key released).
            fixture c = f;
            space_fixture::drop_key(c.w, c.a, space_keys::station_core);
            const snap cb = take(c);
            const station_op r3 = found_station(c.w, sp, stp, c.a, hi);
            check(r3.result == station_result::rejected_launch
                      && r3.launch == space_launch_result::rejected_tech_locked && same(cb, take(c)),
                  "S2", "a core without the station-core key is refused through the launch quote, whole");
        }
        const station_op q3 = quote_found_station(w, sp, stp, f.a, {f.far, orbital_node_kind::l4});
        check(q3.result == station_result::ok && same(before, take(f)), "S2",
              "BL-1581: a core may go to any orbital node — a far planet's L4 quotes");
        const station_op r4 = found_station(w, sp, stp, 999999u, hi);
        check(r4.result == station_result::rejected_unknown_nation && same(before, take(f)), "S2",
              "an unknown nation is refused whole");
    }

    // ---- S3 fit rules -----------------------------------------------------
    std::printf("--- S3 fit rules ---\n");
    entity_id research = null_entity;
    {
        const int s_in = slot_of(0, 1, 1); // interior (6)
        const int pool = w.nations.at(f.a).astronauts;
        const float t = w.nations.at(f.a).treasury;
        const station_op r = fit_station_module(w, sp, stp, f.a, st_id, s_in, station_module_kind::research);
        research = r.asset;
        check(r.result == station_result::ok && w.stations.at(st_id).slots[6] == research
                  && w.space_assets.at(research).module == static_cast<uint8_t>(station_module_kind::research)
                  && w.space_assets.at(research).station_slot == 6
                  && w.space_assets.at(research).cls == space_asset_class::station_module,
              "S3", "a Research module launches into empty slot 6 and holds it while in transit");
        check(w.nations.at(f.a).astronauts == pool - 1 && w.space_assets.at(research).crew == 1
                  && w.nations.at(f.a).treasury < t,
              "S3", "it draws its crew (1) from the pool and is paid through the launch path");

        const snap before = take(f);
        struct rc { const char* what; station_op op; station_result want; };
        const rc cases[] = {
            {"an occupied slot (6)", fit_station_module(w, sp, stp, f.a, st_id, 6, station_module_kind::power),
             station_result::rejected_slot_occupied},
            {"the core's slot (7)", fit_station_module(w, sp, stp, f.a, st_id, 7, station_module_kind::power),
             station_result::rejected_slot_occupied},
            {"a Dock in an interior slot (8)", fit_station_module(w, sp, stp, f.a, st_id, 8, station_module_kind::dock),
             station_result::rejected_edge_only},
            {"a Shipyard in an interior slot (8)", fit_station_module(w, sp, stp, f.a, st_id, 8, station_module_kind::shipyard),
             station_result::rejected_edge_only},
            {"an unknown station", fit_station_module(w, sp, stp, f.a, 999999u, 0, station_module_kind::power),
             station_result::rejected_unknown_station},
            {"another nation's station (Beta into Alpha's)", fit_station_module(w, sp, stp, f.b, st_id, 0, station_module_kind::power),
             station_result::rejected_not_owner},
            {"slot 15 (out of range)", fit_station_module(w, sp, stp, f.a, st_id, 15, station_module_kind::power),
             station_result::rejected_invalid},
            {"slot -1", fit_station_module(w, sp, stp, f.a, st_id, -1, station_module_kind::power),
             station_result::rejected_invalid},
            {"a truss as a module", fit_station_module(w, sp, stp, f.a, st_id, 0, station_module_kind::truss),
             station_result::rejected_invalid},
            {"kind 200", fit_station_module(w, sp, stp, f.a, st_id, 0, static_cast<station_module_kind>(200)),
             station_result::rejected_invalid},
        };
        bool all = true;
        for (const rc& c : cases)
        {
            const bool ok = c.op.result == c.want;
            all = all && ok;
            if (!ok)
                std::printf("    %s: got %d want %d\n", c.what, static_cast<int>(c.op.result), static_cast<int>(c.want));
        }
        check(all && same(before, take(f)), "S3",
              "occupied, core slot, edge-only interior (dock, shipyard), unknown, not-owner, out-of-range, truss, bad kind: each refused, nothing moved");

        // A crewless module: drain the pool, try a crewed module, restore.
        const int keep = w.nations.at(f.a).astronauts;
        w.nations.at(f.a).astronauts = 0;
        const snap b0 = take(f);
        const station_op rc0 = fit_station_module(w, sp, stp, f.a, st_id, 0, station_module_kind::observatory);
        check(rc0.result == station_result::rejected_no_crew && same(b0, take(f)), "S3",
              "a crewed module (observatory) with an empty pool: rejected_no_crew, nothing moved");
        const station_op rc1 = fit_station_module(w, sp, stp, f.a, st_id, 0, station_module_kind::power);
        check(rc1.result == station_result::ok && w.space_assets.at(rc1.asset).crew == 0, "S3",
              "an uncrewed module (power) launches with an empty pool");
        w.nations.at(f.a).astronauts = keep;
        const station_op rd = fit_station_module(w, sp, stp, f.a, st_id, slot_of(0, 4, 0), station_module_kind::dock);
        check(rd.result == station_result::ok, "S3", "a Dock on the outer ring (slot 4) is accepted");
        // Undo the power and dock for the rows below.
        remove_station_module(w, f.a, st_id, 0);
        remove_station_module(w, f.a, st_id, 4);
    }

    // Arrive everything.
    quarter(w, sp, stp, 10);
    quarter(w, sp, stp, 11);

    // ---- S4 remove --------------------------------------------------------
    std::printf("--- S4 remove ---\n");
    {
        const station_op hab = fit_station_module(w, sp, stp, f.a, st_id, slot_of(0, 0, 2), station_module_kind::observatory);
        w.current_econ_tick = 12;
        quarter(w, sp, stp, 12);
        const space_station& st = w.stations.at(st_id);
        const int core_crew = w.space_assets.at(st.core).crew;
        const int pool = w.nations.at(f.a).astronauts;
        const float t = w.nations.at(f.a).treasury;
        const station_op r = remove_station_module(w, f.a, st_id, slot_of(0, 0, 2));
        check(r.result == station_result::ok && w.stations.at(st_id).slots[10] == null_entity
                  && w.space_assets.count(hab.asset) == 0,
              "S4", "removing the observatory frees slot 10 and removes its asset");
        check(w.nations.at(f.a).treasury == t && w.nations.at(f.a).astronauts == pool, "S4",
              "nothing is refunded: treasury and pool unchanged");
        check(w.space_assets.at(st.core).crew == core_crew + 1, "S4",
              "its crew (1) stay aboard, moved onto the core");
        const snap b = take(f);
        check(remove_station_module(w, f.a, st_id, 10).result == station_result::rejected_slot_empty
                  && remove_station_module(w, f.a, st_id, 7).result == station_result::rejected_slot_empty
                  && remove_station_module(w, f.b, st_id, 6).result == station_result::rejected_not_owner
                  && same(b, take(f)),
              "S4", "an empty slot, the core and another nation's station are refused, nothing moved");
    }

    // ---- S5 refit ---------------------------------------------------------
    std::printf("--- S5 refit ---\n");
    {
        w.current_econ_tick = 13;
        const std::size_t n = w.space_assets.size();
        const std::uint32_t cursor = w.next_entity_id();
        const float t = w.nations.at(f.a).treasury;
        const station_op r = refit_station_module(w, stp, f.a, st_id, 6, slot_of(0, 1, 2));
        check(r.result == station_result::ok && w.stations.at(st_id).slots[11] == research
                  && w.stations.at(st_id).slots[6] == null_entity && w.space_assets.at(research).station_slot == 11,
              "S5", "the research module moves 6 -> 11");
        check(w.space_assets.size() == n && w.next_entity_id() == cursor && w.nations.at(f.a).treasury == t,
              "S5", "launch-free: no asset, no id, no debit");
        const station_budget b13 = evaluate_station_budget(w, stp, st_id, 13);
        check(contains(b13.refitting, research) && !contains(b13.working, research), "S5",
              "in the refit quarter it does not work");
        // BL-1642: the refit takes refit_quarters econ ticks - 3 months, the one quarter.
        const int back = 13 + stp.refit_quarters;
        for (int t = 13; t <= back; ++t)
            quarter(w, sp, stp, t);
        const station_budget b14 = evaluate_station_budget(w, stp, st_id, back);
        check(contains(b14.working, research) && w.space_assets.at(research).refit_done_tick == -1, "S5",
              "one quarter (three months) later it works again and the refit mark is cleared");
        const snap b = take(f);
        check(refit_station_module(w, stp, f.a, st_id, 11, 7).result == station_result::rejected_slot_occupied
                  && refit_station_module(w, stp, f.a, st_id, 6, 0).result == station_result::rejected_slot_empty
                  && same(b, take(f)),
              "S5", "a refit onto the core or from an empty slot is refused, nothing moved");
    }

    // ---- S6 budget --------------------------------------------------------
    std::printf("--- S6 budget: newest first ---\n");
    {
        // Core power 4 at 1 AU. Research (draw 3) works. Add an observatory (draw 2):
        // power 5 > 4 -> the observatory (newest) stops, not the research.
        const station_op ob = fit_station_module(w, sp, stp, f.a, st_id, slot_of(0, 3, 1), station_module_kind::observatory);
        quarter(w, sp, stp, 15);
        quarter(w, sp, stp, 16);
        const station_budget b = evaluate_station_budget(w, stp, st_id, 16);
        std::printf("    power %.1f / %.1f, crew need %d room %d life %d aboard %d\n", b.power_draw,
                    b.power_supply, b.crew_needed, b.crew_room, b.life_support, b.crew_aboard);
        check(contains(b.working, research) && contains(b.stopped, ob.asset) && b.stopped.size() == 1,
              "S6", "power 3+2 > 4: the observatory (newest) stops, the research keeps working");
        // Fit a Power module (supply 6, no crew): power covers both, but crew needs
        // 1 core + 1 research + 1 observatory = 3 > room 2 -> still the observatory stops.
        const station_op pw = fit_station_module(w, sp, stp, f.a, st_id, slot_of(0, 0, 0), station_module_kind::power);
        quarter(w, sp, stp, 17);
        const station_budget b2 = evaluate_station_budget(w, stp, st_id, 17);
        check(contains(b2.working, pw.asset) && contains(b2.working, research) && contains(b2.stopped, ob.asset),
              "S6", "with a newer Power module, crew room 2 < need 3: the newest CREWED module (observatory) stops; the power module, drawing on no short budget, is passed over");
        // Now the power module is newest. Remove the research so the observatory alone
        // has room; then a newer Microgravity factory (draw 4, crew 2) overdraws and stops first.
        const station_op hab = fit_station_module(w, sp, stp, f.a, st_id, slot_of(0, 2, 0), station_module_kind::habitation);
        const station_op ls  = fit_station_module(w, sp, stp, f.a, st_id, slot_of(0, 2, 2), station_module_kind::life_support);
        quarter(w, sp, stp, 18);
        const station_budget b3 = evaluate_station_budget(w, stp, st_id, 18);
        std::printf("    + hab + life: power %.1f / %.1f, crew need %d room %d life %d aboard %d; working %zu stopped %zu\n",
                    b3.power_draw, b3.power_supply, b3.crew_needed, b3.crew_room, b3.life_support,
                    b3.crew_aboard, b3.working.size(), b3.stopped.size());
        // Supply 4+6=10; draw 3+2+1+2=8; need 3; room 6; life 6; aboard: core 2 + res 1 + obs 1 = 4.
        check(contains(b3.working, ob.asset) && contains(b3.working, hab.asset) && contains(b3.working, ls.asset)
                  && b3.stopped.empty(),
              "S6", "habitation + life support cover the crew and power covers the draw: everything works");
        const station_op sy = fit_station_module(w, sp, stp, f.a, st_id, slot_of(0, 4, 0), station_module_kind::microgravity_factory); // BL-1582: the Shipyard is far space's (closed); the factory draws as much
        quarter(w, sp, stp, 19);
        const station_budget b4 = evaluate_station_budget(w, stp, st_id, 19);
        check(b4.stopped.size() == 1 && b4.stopped[0] == sy.asset && b4.working.size() == b3.working.size(),
              "S6", fmt("a newer Microgravity factory overdraws power (%.0f > %.0f): it alone stops", 8.0 + 4.0,
                        static_cast<double>(b4.power_supply)));
        remove_station_module(w, f.a, st_id, slot_of(0, 4, 0));
    }

    // ---- S7 solar ---------------------------------------------------------
    std::printf("--- S7 solar falloff ---\n");
    {
        fixture g;
        build(g);
        g.w.current_econ_tick = 0;
        const station_op m = found_station(g.w, sp, stp, g.a, {g.moon, orbital_node_kind::low_orbit});
        const station_op mp = fit_station_module(g.w, sp, stp, g.a, m.station, 0, station_module_kind::power);
        // BL-1642: the core and module travel their months to the moon; step until both stand.
        const int up = std::max(g.w.space_assets.at(m.station).arrival_tick, g.w.space_assets.at(mp.asset).arrival_tick) + 1;
        for (int q = 0; q <= up; ++q)
            quarter(g.w, sp, stp, q);
        const station_budget bm = evaluate_station_budget(g.w, stp, m.station, up);
        check(m.result == station_result::ok && mp.result == station_result::ok && bm.solar_factor == 1.0f
                  && bm.power_supply == stp.core_power_supply + stp.modules[2].power_supply,
              "S7", fmt("a moon station reads its planet's radius: factor %.3f, supply %.2f", bm.solar_factor, bm.power_supply));
        // Stage the same station at the far planet (2.0 AU). The launch legality of
        // far nodes is BL-1581's; the falloff is read off the node's body alone.
        space_station& st = g.w.stations.at(m.station);
        st.node = {g.far, orbital_node_kind::low_orbit};
        const station_budget bf = evaluate_station_budget(g.w, stp, m.station, up);
        const float want = (stp.core_power_supply + stp.modules[2].power_supply) * 0.25f;
        check(std::fabs(bf.solar_factor - 0.25f) < 1e-6f && std::fabs(bf.power_supply - want) < 1e-4f,
              "S7", fmt("at 2.0 AU: factor (1/2)^2 = %.3f, supply %.2f (vs %.2f at home)", bf.solar_factor,
                        bf.power_supply, bm.power_supply));
    }

    // ---- S8 upgrade -------------------------------------------------------
    std::printf("--- S8 upgrade ---\n");
    {
        fixture g;
        build(g);
        g.w.current_econ_tick = 0;
        const station_op s = found_station(g.w, sp, stp, g.a, hi);
        const station_op pw = fit_station_module(g.w, sp, stp, g.a, s.station, slot_of(0, 0, 0), station_module_kind::power);
        const station_op dk = fit_station_module(g.w, sp, stp, g.a, s.station, slot_of(0, 1, 0), station_module_kind::dock);
        for (int q = 0; q < 2; ++q)
            quarter(g.w, sp, stp, q);
        const station_budget b0 = evaluate_station_budget(g.w, stp, s.station, 1);
        check(contains(b0.working, dk.asset), "S8", "at 5x3 the dock (slot 1, top row) works");
        g.w.current_econ_tick = 2;
        const std::size_t n = g.w.space_assets.size();
        const float t = g.w.nations.at(g.a).treasury;
        const station_op u = upgrade_station(g.w, sp, g.a, s.station);
        const space_station& st = g.w.stations.at(s.station);
        check(u.result == station_result::ok && st.level == 1 && st.width() == 7 && st.height() == 5
                  && st.slots.size() == 35,
              "S8", "upgrade: 5x3 -> 7x5");
        check(g.w.space_assets.size() == n + 1 && g.w.space_assets.at(u.asset).module
                      == static_cast<uint8_t>(station_module_kind::truss)
                  && g.w.space_assets.at(u.asset).station_slot == -1 && g.w.nations.at(g.a).treasury < t,
              "S8", "it is a launch: one truss asset in no slot, paid through the launch path");
        check(st.slots[static_cast<std::size_t>(st.centre())] == s.station && st.centre() == 17
                  && st.slots[static_cast<std::size_t>(slot_of(1, 1, 1))] == pw.asset
                  && st.slots[static_cast<std::size_t>(slot_of(1, 2, 1))] == dk.asset
                  && g.w.space_assets.at(dk.asset).station_slot == slot_of(1, 2, 1)
                  && g.w.space_assets.at(s.station).station_slot == 17,
              "S8", "the core stays central (17); the power (0,0)->(1,1) and dock (1,0)->(2,1) keep their place");
        const station_budget b1 = evaluate_station_budget(g.w, stp, s.station, 2);
        check(contains(b1.idle_edge, dk.asset) && !contains(b1.working, dk.asset) && contains(b1.working, pw.asset),
              "S8", "the enclosed dock is idle; the power module (not edge-only) still works");
        const station_op rf = refit_station_module(g.w, stp, g.a, s.station, slot_of(1, 2, 1), slot_of(1, 2, 0));
        const int back = 2 + stp.refit_quarters; // BL-1642: three months
        for (int t = 2; t <= back; ++t)
            quarter(g.w, sp, stp, t);
        const station_budget b2 = evaluate_station_budget(g.w, stp, s.station, back);
        check(rf.result == station_result::ok && contains(b2.working, dk.asset), "S8",
              "refitted to the new edge (2,0), it works a quarter (three months) later");
        check(refit_station_module(g.w, stp, g.a, s.station, slot_of(1, 2, 0), slot_of(1, 2, 1)).result
                  == station_result::rejected_edge_only,
              "S8", "a refit of the dock back into the interior is refused");
        g.w.current_econ_tick = 4;
        const snap b9 = take(g);
        const station_op u2 = upgrade_station(g.w, sp, g.a, s.station);
        check(u2.result == station_result::rejected_tech_locked && same(b9, take(g)), "S8",
              "BL-1582: a second upgrade (7x5 -> 9x7) needs space_grid_9x7, far space's key: refused whole in Era 1");
        {
            // Stage the 9x7 ring as the verb would grow it, (c, r) -> (c + 1, r + 1), to
            // hold the geometry and the level ceiling to account.
            space_station& s2 = g.w.stations.at(s.station);
            const int ow = s2.width();
            s2.level = 2;
            const int nw = s2.width();
            std::vector<entity_id> grown(static_cast<std::size_t>(nw * s2.height()), null_entity);
            for (int i = 0; i < static_cast<int>(s2.slots.size()); ++i)
            {
                const entity_id aid = s2.slots[static_cast<std::size_t>(i)];
                if (aid == null_entity)
                    continue;
                const int ni = (i / ow + 1) * nw + (i % ow + 1);
                grown[static_cast<std::size_t>(ni)] = aid;
                g.w.space_assets.at(aid).station_slot = static_cast<int16_t>(ni);
            }
            s2.slots = std::move(grown);
        }
        const space_station& st2 = g.w.stations.at(s.station);
        check(st2.width() == 9 && st2.height() == 7 && st2.centre() == 31
                  && st2.slots[31] == s.station && st2.slots[static_cast<std::size_t>(slot_of(2, 2, 2))] == pw.asset,
              "S8", "a 9x7 grid (staged): core at 31, power at (2,2)");
        const snap b = take(g);
        check(upgrade_station(g.w, sp, g.a, s.station).result == station_result::rejected_max_level && same(b, take(g)),
              "S8", "a third upgrade is refused whole");
    }
    // ---- S9 template ------------------------------------------------------
    std::printf("--- S9 templates ---\n");
    {
        fixture g;
        build(g);
        g.w.nations.at(g.a).astronauts = 3;
        g.w.current_econ_tick = 0;
        const station_op s = found_station(g.w, sp, stp, g.a, hi); // pool 3 -> 2
        fit_station_module(g.w, sp, stp, g.a, s.station, slot_of(0, 3, 1), station_module_kind::defence); // pool 1
        station_template t;
        t.level = 0;
        t.kinds.assign(15, k_no_station_module);
        auto put = [&](int c, int r, station_module_kind k) {
            t.kinds[static_cast<std::size_t>(slot_of(0, c, r))] = static_cast<uint8_t>(k);
        };
        put(0, 0, station_module_kind::power);
        put(1, 0, station_module_kind::dock);        // crew 1
        put(1, 1, station_module_kind::research);    // crew 1 — the pool will be dry
        put(3, 1, station_module_kind::observatory); // the station holds a defence here
        put(4, 2, station_module_kind::cargo_hold);
        check(save_station_template(g.w, g.a, "Outpost", t) == station_result::ok
                  && g.w.station_templates.at(g.a).count("Outpost") == 1,
              "S9", "a 5x3 template saves");
        station_template bad = t;
        bad.kinds[7] = static_cast<uint8_t>(station_module_kind::power);
        station_template bad2 = t;
        bad2.kinds.pop_back();
        station_template bad3 = t;
        bad3.kinds[0] = static_cast<uint8_t>(station_module_kind::truss);
        const snap bs = take(g);
        check(save_station_template(g.w, g.a, "Centre", bad) == station_result::rejected_invalid
                  && save_station_template(g.w, g.a, "Short", bad2) == station_result::rejected_invalid
                  && save_station_template(g.w, g.a, "Truss", bad3) == station_result::rejected_invalid
                  && save_station_template(g.w, g.a, "", t) == station_result::rejected_invalid
                  && save_station_template(g.w, 999999u, "X", t) == station_result::rejected_unknown_nation
                  && same(bs, take(g)),
              "S9", "a filled centre, a wrong size, a truss, an empty name, an unknown nation: refused, nothing moved");

        g.w.current_econ_tick = 1;
        const std::size_t n = g.w.space_assets.size();
        const station_template_report rep = apply_station_template(g.w, sp, stp, g.a, s.station, "Outpost");
        std::printf("    fitted:");
        for (const int i : rep.fitted) std::printf(" %d", i);
        std::printf("; skipped:");
        for (const auto& [i, why] : rep.skipped) std::printf(" %d(%d)", i, static_cast<int>(why));
        std::printf("\n");
        const space_station& st = g.w.stations.at(s.station);
        // Ascending slot: 0 power (ok), 1 dock (crew 1: pool 1 -> 0), 6 research (no crew),
        // 8 observatory vs defence (occupied), 14 cargo (ok).
        const bool fitted_ok = rep.fitted == std::vector<int>{0, 1, 14};
        const bool skipped_ok = rep.skipped.size() == 2
            && rep.skipped[0] == std::make_pair(6, station_result::rejected_no_crew)
            && rep.skipped[1] == std::make_pair(8, station_result::rejected_slot_occupied);
        check(rep.result == station_result::ok && fitted_ok && g.w.space_assets.size() == n + 3, "S9",
              "apply fits power, dock and cargo hold, each its own launch (3 assets)");
        check(skipped_ok && g.w.space_assets.at(st.slots[8]).module == static_cast<uint8_t>(station_module_kind::defence),
              "S9", "skipped and reported: the research (no crew) and the observatory slot holding a defence");
        g.w.nations.at(g.a).astronauts = 5;
        const station_template_report rep2 = apply_station_template(g.w, sp, stp, g.a, s.station, "Outpost");
        check(rep2.fitted == std::vector<int>{6} && rep2.skipped.size() == 1, "S9",
              "applied again with crew: only the research slot fits; matching slots are left alone");
        // Upgrade, then apply a 5x3 template to a 7x5 station: centred, shifted by (1,1).
        g.w.current_econ_tick = 2;
        upgrade_station(g.w, sp, g.a, s.station);
        station_template t2;
        t2.level = 0;
        t2.kinds.assign(15, k_no_station_module);
        t2.kinds[static_cast<std::size_t>(slot_of(0, 0, 2))] = static_cast<uint8_t>(station_module_kind::fuel_depot);
        save_station_template(g.w, g.a, "Depot", t2);
        const station_template_report rep3 = apply_station_template(g.w, sp, stp, g.a, s.station, "Depot");
        check(rep3.fitted == std::vector<int>{slot_of(1, 1, 3)}, "S9",
              fmt("a 5x3 template on a 7x5 station is centred: (0,2) lands at (1,3) = slot %d", slot_of(1, 1, 3)));
        const snap b = take(g);
        check(apply_station_template(g.w, sp, stp, g.a, s.station, "Nope").result == station_result::rejected_unknown_template
                  && apply_station_template(g.w, sp, stp, g.b, s.station, "Outpost").result == station_result::rejected_not_owner
                  && same(b, take(g)),
              "S9", "an unknown template or another nation's station is refused whole");
        check(delete_station_template(g.w, g.a, "Depot") == station_result::ok
                  && delete_station_template(g.w, g.a, "Depot") == station_result::rejected_unknown_template,
              "S9", "a template deletes once");
    }

    // ---- S10 training -----------------------------------------------------
    std::printf("--- S10 astronaut training ---\n");
    {
        fixture g;
        build(g);
        g.w.nations.at(g.a).astronauts = 0;
        add_centre(g.w, g.a, 0.5f, 0);  // running
        add_centre(g.w, g.b, 0.0f, 0);  // unstaffed
        add_centre(g.w, g.b, 0.5f, 2);  // unfinished
        // BL-1642: training is a QUARTER's count spread over its months (it reads
        // w.current_econ_tick), so each row runs a quarter - ticks 3q, 3q+1, 3q+2.
        const auto train_quarter = [&](int q) {
            int trained = 0;
            for (int m = 0; m < k_econ_ticks_per_quarter; ++m)
            {
                g.w.current_econ_tick = q * k_econ_ticks_per_quarter + m;
                station_step_report rr;
                run_station_step(g.w, stp, g.w.current_econ_tick, &rr);
                trained += rr.astronauts_trained;
            }
            return trained;
        };
        const int t0 = train_quarter(0);
        check(g.w.nations.at(g.a).astronauts == stp.astronauts_per_centre && t0 == stp.astronauts_per_centre
                  && g.w.nations.at(g.b).astronauts == 0,
              "S10", fmt("one staffed centre trains %d a quarter; an unstaffed and an unfinished one train none",
                         stp.astronauts_per_centre));
        for (int q = 1; q < 20; ++q)
            (void)train_quarter(q);
        check(g.w.nations.at(g.a).astronauts == stp.astronaut_cap_per_centre
                  && astronaut_cap(g.w, stp, g.a) == stp.astronaut_cap_per_centre,
              "S10", fmt("the pool stops at its cap (%d)", stp.astronaut_cap_per_centre));
        add_centre(g.w, g.a, 0.5f, 0);
        (void)train_quarter(20);
        check(g.w.nations.at(g.a).astronauts == stp.astronaut_cap_per_centre + 2 * stp.astronauts_per_centre,
              "S10", "a second centre raises the cap and both train");
        // Through the nation step: a quarter of monthly nation steps.
        recipe_registry reg;
        // The space pick runs every month (Ben, 2026-11-14: national policy monthly) and a
        // rival's crewed pick spends from the pool this row counts; seat a as the player,
        // whose nation no pick acts for, so the row reads training alone.
        const entity_id seat_was = g.w.player_entity;
        g.w.player_entity = g.a;
        const int before = g.w.nations.at(g.a).astronauts;
        int filed = 0;
        for (int m = 0; m < k_econ_ticks_per_quarter; ++m)
        {
            economy_report rep;
            const int tick = 21 * k_econ_ticks_per_quarter + m;
            g.w.current_econ_tick = tick;
            run_nation_step(g.w, reg, rep, tick);
            filed += rep.stations.astronauts_trained;
        }
        check(g.w.nations.at(g.a).astronauts == before + 2 * stp.astronauts_per_centre
                  && filed == 2 * stp.astronauts_per_centre,
              "S10", fmt("a quarter of run_nation_step trains them and files the count on economy_report::stations"
                         " (pool %d -> %d, filed %d, want +%d)", before, g.w.nations.at(g.a).astronauts, filed,
                         2 * stp.astronauts_per_centre));
        g.w.player_entity = seat_was;
    }

    // ---- S11 loss ---------------------------------------------------------
    std::printf("--- S11 loss ---\n");
    {
        fixture g;
        build(g);
        g.w.current_econ_tick = 0;
        const station_op s = found_station(g.w, sp, stp, g.a, hi);
        const station_op rs = fit_station_module(g.w, sp, stp, g.a, s.station, 6, station_module_kind::research);
        const station_op dk = fit_station_module(g.w, sp, stp, g.a, s.station, 1, station_module_kind::dock);
        for (int q = 0; q < 2; ++q)
            quarter(g.w, sp, stp, q);
        const int pool = g.w.nations.at(g.a).astronauts;
        const int aboard = g.w.space_assets.at(s.station).crew + g.w.space_assets.at(rs.asset).crew
                         + g.w.space_assets.at(dk.asset).crew;
        g.w.nations.at(g.a).treasury = 0.0f; // nothing can be paid
        station_step_report r{}, last{};
        int lost_total = 0;
        for (int q = 2; q < 2 + sp.quarters_to_loss; ++q)
        {
            quarter(g.w, sp, stp, q, &r);
            lost_total += r.crew_lost;
            if (r.stations_lost) last = r;
        }
        check(g.w.stations.count(s.station) == 0 && last.stations_lost == 1, "S11",
              fmt("after %d unpaid quarters the core is lost and the station with it", sp.quarters_to_loss));
        check(g.w.space_assets.at(rs.asset).state == space_asset_state::lost
                  && g.w.space_assets.at(dk.asset).state == space_asset_state::lost,
              "S11", "every module is lost with it");
        check(lost_total == aboard && g.w.nations.at(g.a).astronauts == pool, "S11",
              fmt("its crew (%d) are lost; the pool is untouched (%d)", aboard, pool));
        const station_op again = fit_station_module(g.w, sp, stp, g.a, s.station, 0, station_module_kind::power);
        check(again.result == station_result::rejected_unknown_station, "S11", "the lost station takes no fit");

        // A lost module alone frees its slot.
        fixture h;
        build(h);
        h.w.current_econ_tick = 0;
        const station_op hs = found_station(h.w, sp, stp, h.a, hi);
        const station_op hm = fit_station_module(h.w, sp, stp, h.a, hs.station, 6, station_module_kind::research);
        quarter(h.w, sp, stp, 0);
        h.w.space_assets.at(hm.asset).state = space_asset_state::lost; // as the asset step would
        h.w.space_assets.at(hm.asset).unpaid_quarters = sp.quarters_to_loss;
        station_step_report hr;
        quarter(h.w, sp, stp, 1, &hr);
        check(h.w.stations.at(hs.station).slots[6] == null_entity && hr.slots_freed == 1 && hr.crew_lost == 1,
              "S11", "a lost module frees its slot; its crew (1) are lost");
        check(fit_station_module(h.w, sp, stp, h.a, hs.station, 6, station_module_kind::power).result == station_result::ok,
              "S11", "the freed slot takes a new module");
        const std::string hb = to_bytes(h.w);
        world hback;
        check(from_bytes(hb, hback) && to_bytes(hback) == hb, "S11", "a world holding a lost module record round-trips");
    }

    // ---- S12 save ---------------------------------------------------------
    std::printf("--- S12 save ---\n");
    {
        fixture g;
        build(g);
        programme(g, sp, stp);
        station_template t;
        t.level = 1;
        t.kinds.assign(35, k_no_station_module);
        t.kinds[0] = static_cast<uint8_t>(station_module_kind::comms_relay);
        save_station_template(g.w, g.a, "Big", t);
        g.w.current_econ_tick = 9;
        const station_op extra = fit_station_module(g.w, sp, stp, g.a, g.w.stations.begin()->first, 0,
                                                    station_module_kind::comms_relay);
        const entity_id dk = g.w.stations.begin()->second.slots[static_cast<std::size_t>(slot_of(1, 2, 1))];
        refit_station_module(g.w, stp, g.a, g.w.stations.begin()->first, slot_of(1, 2, 1), slot_of(1, 6, 0));
        g.w.nations.at(g.b).astronauts = 7;
        const world& w2 = g.w;
        const std::string b = to_bytes(w2);
        world r;
        const bool ok = from_bytes(b, r);
        bool eq = ok && r.stations.size() == w2.stations.size() && r.station_templates == w2.station_templates
                  && r.nations.at(g.a).astronauts == w2.nations.at(g.a).astronauts
                  && r.nations.at(g.b).astronauts == 7;
        if (eq)
            for (const auto& [sid, st] : w2.stations)
            {
                const space_station& x = r.stations.at(sid);
                eq = eq && x.owner == st.owner && x.core == st.core && x.node == st.node && x.level == st.level
                     && x.slots == st.slots;
            }
        if (eq)
            for (const auto& [aid, a] : w2.space_assets)
            {
                const space_asset& x = r.space_assets.at(aid);
                eq = eq && x.station == a.station && x.station_slot == a.station_slot && x.module == a.module
                     && x.crew == a.crew && x.refit_done_tick == a.refit_done_tick && x.cls == a.cls;
            }
        std::printf("    %zu stations, %zu assets, %zu template nations; refit pending on %u; stream %zu bytes, "
                    "station section %zu bytes, world_save_version %u\n",
                    w2.stations.size(), w2.space_assets.size(), w2.station_templates.size(),
                    static_cast<unsigned>(dk), b.size(), world_save_station_section_bytes(w2),
                    static_cast<unsigned>(world_save_version));
        check(extra.result == station_result::ok && eq && r.state_hash(0) == w2.state_hash(0), "S12",
              "a fitted, upgraded, refitting, templated station world with astronauts round-trips field by field, equal hash");
        check(ok && to_bytes(r) == b, "S12", "re-serialising reproduces the stream byte for byte");

        // The v80 prefix of a station-free world (templates and pools only).
        fixture q;
        build(q);
        save_station_template(q.w, q.a, "Plan", t);
        std::ostringstream o80(std::ios::binary);
        write_world_snapshot_pre_stations(q.w, o80);
        const std::string b81 = to_bytes(q.w);
        const std::string s80 = o80.str();
        world old;
        const bool ok80 = from_bytes(s80, old);
        {
            std::size_t diff = 0;
            while (diff < s80.size() && diff < b81.size() && s80[diff] == b81[diff]) ++diff;
            std::printf("    v80 %zu bytes, v81 %zu, section %zu, ok80 %d, first differing byte %zu\n", s80.size(),
                        b81.size(), world_save_station_section_bytes(q.w), ok80 ? 1 : 0, diff);
        }
        check(ok80 && old.station_templates.empty() && old.nations.at(q.a).astronauts == 0
                  && s80.size() + world_save_space_line_gap_bytes(q.w) /* the space merge: main's naval..missile sections */ + world_save_space_tail_bytes_after(k_world_save_version_research_books, q.w) /* the space merge: a v89 stream carries no space section */ == b81.size(),
              // The space merge: v80 is no longer a byte prefix of today - main's v81 widened the resource arrays.
              "S12", "v80 is today less ship hulls, the naval..missile sections and every space section: it reads as no templates and empty pools");
        std::ostringstream o80s(std::ios::binary);
        write_world_snapshot_pre_stations(g.w, o80s);
        world old2;
        check(!from_bytes(o80s.str(), old2), "S12", "a v80 stream carrying a station_core class is refused (v80 never held one)");

        // Tamper the tail. Station record 0 starts the section: id 0, owner 4, core 8,
        // body 12, kind 16, level 17, count 18, slots 22.
        const std::size_t sec = b.size() - world_save_station_section_bytes(w2) - world_save_early_space_section_bytes(w2)
                              - (world_save_space_node_section_bytes(w2) + (world_save_space_unit_section_bytes(w2) + (world_save_space_battle_section_bytes(w2) + (world_save_prospecting_section_bytes(w2) + (world_save_space_convoy_section_bytes(w2) + (world_save_debris_section_bytes(w2) + (world_save_space_bombard_section_bytes(w2) + (world_save_offworld_base_section_bytes(w2) + (world_save_space_firsts_section_bytes(w2) + world_save_transit_origin_section_bytes(w2) + world_save_launch_boost_section_bytes(w2) + world_save_province_names_section_bytes(w2) /* BL-1708 (v97) */))))))))); // v82 + v83 follow
        const std::size_t r0 = sec + 4;
        auto refused = [&](std::size_t off, const void* v, std::size_t n) {
            std::string tb = b;
            std::memcpy(&tb[off], v, n);
            world vv;
            vv.current_econ_tick = 77;
            return !from_bytes(tb, vv) && vv.current_econ_tick == 77 && vv.nations.empty();
        };
        const std::uint8_t kind_surface = 0, kind_bad = 9, level_bad = 3;
        const entity_id junk = 0x7FFFFFF0u;
        const space_station& st0 = w2.stations.begin()->second;
        // A slot index of a module: swap it to name another station's-less asset (the core).
        std::size_t first_mod = 0;
        for (std::size_t i = 0; i < st0.slots.size(); ++i)
            if (st0.slots[i] != null_entity && st0.slots[i] != st0.core) { first_mod = i; break; }
        check(refused(r0 + 16, &kind_surface, 1), "S12", "a station at a SURFACE node refuses the stream whole");
        check(refused(r0 + 16, &kind_bad, 1), "S12", "an out-of-range node kind refuses the stream whole");
        check(refused(r0 + 17, &level_bad, 1), "S12", "level 3 refuses the stream whole");
        check(refused(r0 + 4, &junk, 4), "S12", "an owner naming no nation refuses the stream whole");
        check(refused(r0 + 12, &g.star, 4), "S12", "a station at the star refuses the stream whole");
        check(refused(r0 + 22 + 4 * first_mod, &junk, 4), "S12", "a slot naming no asset refuses the stream whole");
        check(refused(r0 + 22 + 4 * first_mod, &st0.core, 4), "S12", "the core in a second slot refuses the stream whole");
        {
            // Two slots swapped: each module's own slot index then disagrees.
            std::size_t second = 0;
            for (std::size_t i = first_mod + 1; i < st0.slots.size(); ++i)
                if (st0.slots[i] != null_entity && st0.slots[i] != st0.core) { second = i; break; }
            std::string tb = b;
            std::memcpy(&tb[r0 + 22 + 4 * first_mod], &st0.slots[second], 4);
            std::memcpy(&tb[r0 + 22 + 4 * second], &st0.slots[first_mod], 4);
            world vv;
            check(second != 0 && !from_bytes(tb, vv), "S12", "two modules' slots swapped refuses the stream whole");
        }
        // The asset-field part: first record after the station records.
        std::size_t a0 = r0;
        for (const auto& [sid, st] : w2.stations)
            a0 += 22 + 4 * st.slots.size();
        a0 += 4; // its count; record: id 0, station 4, slot 8, module 12, crew 13, refit 17
        const std::int32_t neg = -5, slot_bad = 3;
        const std::uint8_t mod_bad = 77;
        check(refused(a0 + 13, &neg, 4), "S12", "negative crew refuses the stream whole");
        check(refused(a0 + 17, &neg, 4), "S12", "a refit tick below -1 refuses the stream whole");
        check(refused(a0 + 8, &slot_bad, 4), "S12", "a core claiming a non-centre slot refuses the stream whole");
        check(refused(a0 + 12, &mod_bad, 1), "S12", "a core carrying a module kind refuses the stream whole");
        // The astronaut part: the last 4 + 8n bytes; the last record's count.
        const std::int32_t over = k_astronaut_pool_max + 1, zero = 0;
        const std::size_t pools_end = b.size() - world_save_early_space_section_bytes(w2)
                                    - (world_save_space_node_section_bytes(w2) + (world_save_space_unit_section_bytes(w2) + (world_save_space_battle_section_bytes(w2) + (world_save_prospecting_section_bytes(w2) + (world_save_space_convoy_section_bytes(w2) + (world_save_debris_section_bytes(w2) + (world_save_space_bombard_section_bytes(w2) + (world_save_offworld_base_section_bytes(w2) + (world_save_space_firsts_section_bytes(w2) + world_save_transit_origin_section_bytes(w2) + world_save_launch_boost_section_bytes(w2) + world_save_province_names_section_bytes(w2) /* BL-1708 (v97) */))))))))); // v82 + v83 follow
        check(refused(pools_end - 4, &over, 4), "S12", "a pool past the ceiling refuses the stream whole");
        check(refused(pools_end - 4, &zero, 4), "S12", "a listed pool of 0 refuses the stream whole");
        {
            // A template whose centre slot is filled: rebuild the stream from a world
            // whose template was written raw.
            world raw = w2;
            raw.station_templates.at(g.a).at("Big").kinds[17] = 0;
            world vv;
            check(!from_bytes(to_bytes(raw), vv), "S12", "a template with its centre filled refuses the stream whole");
            world raw2 = w2;
            raw2.space_assets.at(extra.asset).station = null_entity; // a live module claiming no station
            world vv2;
            check(!from_bytes(to_bytes(raw2), vv2), "S12", "a slotted module naming no station refuses the stream whole");
            // A satellite carrying crew: plant one.
            world raw4 = w2;
            const space_launch_quote sq = launch_space_asset(raw4, sp, g.a, space_asset_class::satellite, {g.home, orbital_node_kind::low_orbit}, space_satellite_role::comms);
            raw4.space_assets.at(sq.asset).crew = 2;
            world vv4;
            check(!from_bytes(to_bytes(raw4), vv4), "S12", "a satellite carrying crew refuses the stream whole");
        }
    }

    // ---- S14 keys (BL-1582) -----------------------------------------------
    std::printf("--- S14 module, upgrade and centre keys ---\n");
    {
        fixture g;
        build(g);
        g.w.current_econ_tick = 0;
        const station_op s = found_station(g.w, sp, stp, g.a, hi);
        space_fixture::drop_key(g.w, g.a, space_keys::module_research);
        const snap b0 = take(g);
        const station_op r = fit_station_module(g.w, sp, stp, g.a, s.station, 6, station_module_kind::research);
        check(s.result == station_result::ok && r.result == station_result::rejected_tech_locked && same(b0, take(g)),
              "S14", "a Research module without space_module_research: rejected_tech_locked, nothing moved");
        const station_op pw = fit_station_module(g.w, sp, stp, g.a, s.station, 6, station_module_kind::power);
        check(pw.result == station_result::ok, "S14", "a module whose key IS held still fits (power)");
        const snap b1 = take(g);
        const station_op sy = fit_station_module(g.w, sp, stp, g.a, s.station, 0, station_module_kind::shipyard);
        check(sy.result == station_result::rejected_tech_locked && same(b1, take(g)), "S14",
              "a Shipyard on the edge: its key is far space's, closed to every nation — refused whole");
        space_fixture::drop_key(g.w, g.a, space_keys::grid_7x5);
        const snap b2 = take(g);
        check(upgrade_station(g.w, sp, g.a, s.station).result == station_result::rejected_tech_locked && same(b2, take(g)),
              "S14", "the first upgrade without space_grid_7x5: refused whole");
        // The Astronaut Training Centre's construction gate.
        fixture h;
        build(h);
        const bool open_before = structure_unlocked(h.w, h.a, building_type::astronaut_training_centre);
        space_fixture::drop_key(h.w, h.a, space_keys::astronaut_training);
        const bool open_after = structure_unlocked(h.w, h.a, building_type::astronaut_training_centre);
        const bool pad_open = structure_unlocked(h.w, h.a, building_type::launchpad);
        check(open_before && !open_after && pad_open, "S14",
              "the Astronaut Training Centre may be built only with astronaut_training held (other types unaffected)");
    }

    // ---- S15 slots (BL-1581) ----------------------------------------------
    std::printf("--- S15 a core holds a slot ---\n");
    {
        fixture g;
        build(g);
        g.w.current_econ_tick = 0;
        const int cap = orbital_slot_capacity(g.w, hi);
        int sats = 0;
        for (int i = 0; i < cap - 1; ++i)
            sats += launch_space_asset(g.w, sp, g.a, space_asset_class::satellite, hi, space_satellite_role::comms).result
                    == space_launch_result::launched ? 1 : 0;
        const station_op s = found_station(g.w, sp, stp, g.a, hi);
        check(sats == cap - 1 && s.result == station_result::ok && orbital_slots_held(g.w, hi) == cap, "S15",
              fmt("%d satellites and a core fill the home high orbit (%d slots)", sats, cap));
        g.w.nations.at(g.b).astronauts = 5;
        const snap b0 = take(g);
        const station_op s2 = found_station(g.w, sp, stp, g.b, hi);
        check(s2.result == station_result::rejected_launch && s2.launch == space_launch_result::rejected_no_slot
                  && same(b0, take(g)),
              "S15", "a second core to the full node is refused through the launch quote, whole");
        const station_op m = fit_station_module(g.w, sp, stp, g.a, s.station, 6, station_module_kind::power);
        check(m.result == station_result::ok && orbital_slots_held(g.w, hi) == cap, "S15",
              "a module rides inside its core's slot: it fits at the full node and holds none");
        const space_asset& core = g.w.space_assets.at(s.station);
        const space_asset& mod = g.w.space_assets.at(m.asset);
        check(core.node == hi && mod.node == hi, "S15", "the core and its module sit at the station's node");
    }

    // ---- S16 signal delay (BL-1587) ----------------------------------------
    std::printf("--- S16 signal delay on station orders ---\n");
    {
        fixture g;
        build(g);
        recipe_registry reg;
        g.w.current_econ_tick = 0;
        const station_op home = found_station(g.w, sp, stp, g.a, hi);                                  // covered
        const station_op moon = found_station(g.w, sp, stp, g.a, {g.moon, orbital_node_kind::low_orbit}); // not covered
        space_command fit;
        fit.nation = g.a;
        fit.verb   = static_cast<uint8_t>(space_verb::fit_module);
        fit.subject = home.station;
        fit.slot   = 6;
        fit.module = static_cast<uint8_t>(station_module_kind::power);
        const space_command_outcome fh = apply_space_command(g.w, reg, fit);
        check(home.result == station_result::ok && fh.result == space_command_result::applied
                  && g.w.stations.at(home.station).slots[6] == fh.asset && fh.asset != null_entity,
              "S16", "a fit to a station in coverage (home high orbit) lands at once");

        g.w.current_econ_tick = 3;
        fit.subject = moon.station;
        const snap b0 = take(g);
        const space_command_outcome fm = apply_space_command(g.w, reg, fit);
        const snap b1 = take(g);
        check(moon.result == station_result::ok && fm.result == space_command_result::queued
                  && g.w.stations.at(moon.station).slots[6] == null_entity && b1.assets == b0.assets
                  && b1.ta == b0.ta && b1.comp == b0.comp && b1.orders == b0.orders + 1
                  && g.w.space_orders.back().due_tick == 4,
              "S16", "a fit to a station outside coverage (moon low orbit) is queued: nothing launched, nothing paid, due next quarter");
        space_command up;
        up.nation = g.a;
        up.verb = static_cast<uint8_t>(space_verb::upgrade_station);
        up.subject = moon.station;
        const space_command_outcome um = apply_space_command(g.w, reg, up);
        check(um.result == space_command_result::queued && g.w.stations.at(moon.station).level == 0, "S16",
              "an upgrade outside coverage is queued too");
        {
            // The queue round-trips the save.
            const std::string b = to_bytes(g.w);
            world back;
            check(from_bytes(b, back) && back.space_orders.size() == 2 && back.state_hash(0) == g.w.state_hash(0)
                      && to_bytes(back) == b,
                  "S16", "two pending station orders round-trip (equal hash, byte-identical)");
        }
        economy_report r3;
        run_nation_step(g.w, reg, r3, 3);
        check(g.w.stations.at(moon.station).slots[6] == null_entity && g.w.space_orders.size() == 2
                  && r3.space_orders.applied == 0,
              "S16", "the issuing quarter's nation step (3) leaves both waiting");
        g.w.current_econ_tick = 4;
        economy_report r4;
        run_nation_step(g.w, reg, r4, 4);
        const space_station& ms = g.w.stations.at(moon.station);
        const entity_id landed = ms.level == 1 ? ms.slots[static_cast<std::size_t>(slot_of(1, 2, 2))] : null_entity;
        check(g.w.space_orders.empty() && r4.space_orders.applied == 2 && ms.level == 1 && landed != null_entity
                  && g.w.space_assets.at(landed).module == static_cast<uint8_t>(station_module_kind::power)
                  && g.w.space_assets.at(landed).launch_tick == 4,
              "S16", "exactly one quarter later (4) the fit and then the upgrade land, in issue order (the module keeps its place)");

        // A pending order that is no longer legal is dropped and reported.
        g.w.current_econ_tick = 5;
        space_command f2 = fit;
        f2.slot = 0;
        const space_command_outcome q2 = apply_space_command(g.w, reg, f2);
        fit_station_module(g.w, sp, stp, g.a, moon.station, 0, station_module_kind::cargo_hold); // the slot fills first (a direct, in-process fit)
        g.w.current_econ_tick = 6;
        economy_report r6;
        run_nation_step(g.w, reg, r6, 6);
        check(q2.result == space_command_result::queued && r6.space_orders.dropped == 1
                  && r6.space_orders.dropped_orders.size() == 1
                  && r6.space_orders.dropped_orders[0].second.station == station_result::rejected_slot_occupied
                  && g.w.space_orders.empty(),
              "S16", "a fit whose slot was filled before its signal is dropped and reported (rejected_slot_occupied)");

        // Template verbs on the ground are never delayed; apply to a far station is.
        space_command sv;
        sv.nation = g.a;
        sv.verb = static_cast<uint8_t>(space_verb::save_template);
        sv.name = "Ring";
        sv.layout.level = 0;
        sv.layout.kinds.assign(15, k_no_station_module);
        sv.layout.kinds[1] = static_cast<uint8_t>(station_module_kind::power);
        const space_command_outcome so = apply_space_command(g.w, reg, sv);
        space_command ap;
        ap.nation = g.a;
        ap.verb = static_cast<uint8_t>(space_verb::apply_template);
        ap.subject = moon.station;
        ap.name = "Ring";
        const space_command_outcome ao = apply_space_command(g.w, reg, ap);
        ap.subject = home.station;
        const space_command_outcome ah = apply_space_command(g.w, reg, ap);
        check(so.result == space_command_result::applied && ao.result == space_command_result::queued
                  && ah.result == space_command_result::applied && ah.fitted == 1,
              "S16", "saving a template applies at once; applying it waits outside coverage and lands at once inside");

        // Validation on the station verbs: refused whole.
        auto refused_whole = [&](space_command c, space_command_result want, const char* what) {
            const snap s0 = take(g);
            const space_command_outcome o = apply_space_command(g.w, reg, c);
            check(o.result == want && same(s0, take(g)), "S16", fmt("%s: refused whole (code %d)", what, static_cast<int>(o.result)));
        };
        space_command c = fit;
        c.slot = 99;
        refused_whole(c, space_command_result::rejected_invalid, "a slot index past the grid");
        c = fit;
        c.slot = -1;
        refused_whole(c, space_command_result::rejected_invalid, "slot -1");
        c = fit;
        c.module = static_cast<uint8_t>(station_module_kind::truss);
        refused_whole(c, space_command_result::rejected_invalid, "a truss as a fitted module");
        c = fit;
        c.subject = 999999u;
        refused_whole(c, space_command_result::rejected_invalid, "an unknown station");
        c = fit;
        c.nation = g.b;
        refused_whole(c, space_command_result::rejected_not_owner, "another nation's station (not queued either)");
        c = sv;
        c.name = std::string(65, 'x');
        refused_whole(c, space_command_result::rejected_invalid, "a 65-byte template name");
        c = sv;
        c.layout.kinds.pop_back();
        refused_whole(c, space_command_result::rejected_invalid, "a layout one slot short");
        c = fit;
        c.verb = static_cast<uint8_t>(space_verb::refit_module);
        c.slot = 6;
        c.slot_to = 6;
        refused_whole(c, space_command_result::rejected_invalid, "a refit onto its own slot");
    }

    // ---- S13 determinism --------------------------------------------------
    std::printf("--- S13 determinism ---\n");
    {
        fixture x, y;
        build(x);
        build(y);
        programme(x, sp, stp);
        programme(y, sp, stp);
        const std::uint64_t hx = x.w.state_hash(0), hy = y.w.state_hash(0);
        check(hx == hy && to_bytes(x.w) == to_bytes(y.w), "S13",
              fmt("two fixtures driven identically: equal state_hash %016llx and equal streams",
                  static_cast<unsigned long long>(hx)));
        world m1 = x.w;
        m1.nations.at(x.a).astronauts += 1;
        world m2 = x.w;
        m2.station_templates.at(x.a).at("Outpost").kinds[0] = 0;
        world m3 = x.w;
        {
            std::vector<entity_id>& sl = m3.stations.begin()->second.slots;
            const auto full  = std::find_if(sl.begin(), sl.end(), [&](entity_id e) {
                return e != null_entity && e != m3.stations.begin()->first; });
            const auto empty = std::find(sl.begin(), sl.end(), null_entity);
            std::iter_swap(full, empty);
        }
        check(m1.state_hash(0) != hx && m2.state_hash(0) != hx && m3.state_hash(0) != hx, "S13",
              "the hash sees an astronaut, a template slot and a station slot");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
