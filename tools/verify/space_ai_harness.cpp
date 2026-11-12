// ---------------------------------------------------------------------------
// space_ai_harness — BL-1589 (rival space AI), the headless proof
// ---------------------------------------------------------------------------
// docs/ai/AI_OPPONENT.md § 11, "Rival nations go to space as the player does"
// (Ben, 2026-11-10); docs/economy/SPACE_ASSETS.md § Launching, § Satellites,
// § Space stations. A hand-built fixture: a star and a home planet at 1.0 AU,
// three nations on it — two rivals and the player's — each staged to launch
// (space_fixture.hpp: every Digitisation node held, a staffed Launchpad, a
// stocked home shelf). Rows:
//
//   P1  LAUNCH: a keyed, funded rival's first pick is a comms satellite to its
//       home low orbit, applied through the seam: one asset, role comms, the
//       treasury debited exactly the pick's quoted cost.
//   P2  ORDER: then a station core, then modules in the target layout's order.
//   P3  ONE A QUARTER: over twenty quarters no rival gains more than one asset
//       (satellite, core or module) in any quarter, and every acting pick
//       names exactly one asset gained.
//   P4  NEVER THE PLAYER: the player's nation, keyed, funded and padded, owns
//       nothing in space, its treasury and pool untouched, and no pick row
//       names it.
//   P5  NO KEYS: a rival holding no node takes no action (no_rocketry), and the
//       world hashes exactly as before the pass.
//   P6  NO PAD / RESERVE / CREW: each wall is reported and touches nothing.
//   P7  SETTLE: before the research age opens the pass does nothing.
//   P8  DETERMINISM: two fixtures driven identically end with equal state_hash
//       and byte-identical snapshots.
//   P9  THROUGH THE STEP: run_nation_step files the picks on economy_report.
//
// BL-1615 (rival space groundwork), on a second fixture: three nations owning
// roaded land on the home planet, no pads, no keys (score_nation_space_ground):
//   G1  PAD: a rival with Rocketry and money builds a Launchpad on its own
//       capital tile, paid, staffed; a pad still building counts (no second).
//   G2  PER NATION: a second nation builds its own pad on the same body; a
//       nation's second is refused launchpad_exists with the new wording.
//   G3  NEVER THE PLAYER: the player's nation is never built for.
//   G4  CENTRE: no centre without the astronaut-training key; with it and a
//       completed pad, one centre follows, staffed, and no second.
//   G5  A LAUNCH FOLLOWS: with its pad complete the rival's space pick launches
//       a comms satellite; a nation whose pad still builds meets no_launchpad.
//   G6  WALLS: no Rocketry, the settle, a treasury short of twice the capex -
//       each builds nothing and leaves the hash alone.
//   G7  DETERMINISM: two groundwork runs end byte-identical.
//
// BL-1630 (launch goods supply), on the first fixture (post_space_launch_wants):
//   B1  LOT: a keyed nation with a staffed pad has a standing lot - one satellite's
//       components and its propellant to home low orbit - at its home market.
//   B2  POSTED: every padded nation posts that lot into report.wants; the world
//       is not touched.
//   B3  LOTS: standing_lots 2 bids two lots.
//   B4  CEILING: a good over the fair-price ceiling does not bid; its want goes
//       to report.hauler_wants.
//   B5  WALLS: no Rocketry, an unstaffed pad, the settle, standing_lots 0 - no bid.
//
// BL-1608 (rival far-space AI), on a third fixture: a gridded home planet and an
// airless moon, three nations holding every near and far key (choose_far_space_action):
//   F1  DESIGN: the pick is pure; one live design per researched class, every slot
//       the best item researched.
//   F2  LANDERS: raise, embark, load the kit, move - each chosen when it scores.
//   F3  BASE: a base founded on a prospected moon tile from two Landers; none raised
//       past far_space_bases_max.
//   F4  NEVER THE PLAYER, one verb a nation a quarter; a Hauler design makes the
//       rival's inter-body trade a Hauler convoy (the trade seam's).
//   F5  WALLS: no far key, no Rocketry, the reserve, no mapped body, the settle.
//   F6  WAR: Frigates raised to the war's count, a bombard order on the hostile's
//       building, a fleet away from the war's node ordered to it whole.
//   F7  BATTERY: a hostile Frigate in home orbit: one orbital defence battery built.
//   F8  NR-1206's dial defaults OFF.  F9  two runs byte-identical.
//
// Build: node tools/verify/build_harness.js space_ai_harness --run
// ---------------------------------------------------------------------------

#include "world/campaign_settle.hpp"
#include "world/corp_ai.hpp"
#include "world/placement_rules.hpp"
#include "world/economy_system.hpp"
#include "world/orbital_graph.hpp"
#include "world/nation_ai.hpp"
#include "world/nation_step.hpp"
#include "world/recipe_registry.hpp"
#include "world/space_assets.hpp"
#include "world/space_command.hpp"
#include "world/stations.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include "world/early_space_tree_data.hpp" // BL-1608: the far-space keys
#include "world/nation_tech.hpp"
#include "world/offworld_base.hpp"
#include "world/province.hpp"
#include "world/space_battle.hpp"
#include "world/space_bombard.hpp"
#include "world/space_convoy.hpp"
#include "world/space_effects.hpp"
#include "world/space_units.hpp"
#include "world/stance.hpp"
#include "world/survey_system.hpp"
#include "space_fixture.hpp"

#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
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
    entity_id star = null_entity, home = null_entity;
    entity_id a = null_entity, b = null_entity, player = null_entity;
    entity_id market = null_entity;
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

void build(fixture& f, bool keys = true)
{
    world& w = f.w;
    f.star = add_body(w, "Star", body_type::star, null_entity, 0.0f);
    f.home = add_body(w, "Home", body_type::planet, null_entity, 1.0f);
    w.star_body = f.star;
    w.home_body = f.home;
    const char* names[3] = {"Alpha", "Beta", "Seat"};
    entity_id* ids[3] = {&f.a, &f.b, &f.player};
    const entity_id tile = space_fixture::add_tile(w, f.home);
    for (int i = 0; i < 3; ++i)
    {
        *ids[i] = w.create_entity();
        nation_component nc;
        nc.name = names[i];
        nc.treasury = 2000000.0f;
        nc.astronauts = 20;
        w.nations.emplace(*ids[i], nc);
        if (keys)
            space_fixture::hold_every_node(w, *ids[i]);
        space_fixture::add_pad(w, *ids[i], tile, 10.0f);
    }
    w.player_entity = f.player;
    f.market = space_fixture::add_shelf(w, f.home);
    w.current_econ_tick = k_campaign_settle_ticks; // the research age is open
}

/// Assets (satellites, cores, modules) a nation owns, lost or not.
int assets_of_nation(const world& w, entity_id n)
{
    int c = 0;
    for (const auto& [id, a] : w.space_assets)
        if (a.owner == n)
            ++c;
    return c;
}

/// One quarter, in the nation step's order: the pick, the queue, the assets, the stations.
std::vector<space_ai_pick> quarter(world& w, const recipe_registry& reg, int tick)
{
    w.current_econ_tick = tick;
    std::vector<space_ai_pick> picks;
    run_nation_space_ai(w, reg, &picks);
    run_space_orders(w, reg, tick);
    run_space_asset_step(w, reg.space_assets(), tick);
    run_station_step(w, reg.stations(), tick);
    return picks;
}

const space_ai_pick* pick_for(const std::vector<space_ai_pick>& v, entity_id n)
{
    for (const space_ai_pick& p : v)
        if (p.nation == n)
            return &p;
    return nullptr;
}

// ---- BL-1615 the groundwork fixture --------------------------------------------
// The same star and home planet, three nations each owning three roaded land tiles
// in a row (capital first), no pads, no keys and no shelf: what a rival meets the
// quarter it first holds Rocketry.
struct ground
{
    world     w;
    entity_id star = null_entity, home = null_entity;
    entity_id a = null_entity, b = null_entity, player = null_entity;
    std::vector<entity_id> tiles_a, tiles_b, tiles_p;
};

void build_ground(ground& g)
{
    world& w = g.w;
    g.star = add_body(w, "Star", body_type::star, null_entity, 0.0f);
    g.home = add_body(w, "Home", body_type::planet, null_entity, 1.0f);
    w.star_body = g.star;
    w.home_body = g.home;
    const char* names[3] = {"Alpha", "Beta", "Seat"};
    entity_id* ids[3] = {&g.a, &g.b, &g.player};
    std::vector<entity_id>* tiles[3] = {&g.tiles_a, &g.tiles_b, &g.tiles_p};
    for (int i = 0; i < 3; ++i)
    {
        *ids[i] = w.create_entity();
        nation_component nc;
        nc.name = names[i];
        nc.treasury = 2000000.0f;
        for (int x = 0; x < 3; ++x)
        {
            const entity_id t = space_fixture::add_tile(w, g.home);
            tile_component& tc = w.tiles.at(t);
            tc.grid_x = x;
            tc.grid_y = 5 * i;
            tc.road_edges = 1; // a building needs a road
            tiles[i]->push_back(t);
            nc.tiles.push_back(t);
            w.tile_to_nation[t] = *ids[i];
        }
        nc.capital_tile = (*tiles[i])[0];
        w.nations.emplace(*ids[i], nc);
    }
    w.player_entity = g.player;
    w.current_econ_tick = k_campaign_settle_ticks;
}

/// Hold only the nodes carrying Rocketry's launch key.
void hold_dispatch_only(world& w, entity_id n)
{
    for (int i = 0; i < io::dg::node_count; ++i)
        if (node_has_mechanic(i, space_keys::dispatch))
            w.nation_tech[n].held.set(static_cast<std::size_t>(i));
}

/// The lowest-id building of @p type in @p n's assets, or null.
entity_id own_of_type(const world& w, entity_id n, building_type type)
{
    entity_id best = null_entity;
    for (const entity_id bid : w.nations.at(n).state_assets)
        if (const auto it = w.buildings.find(bid); it != w.buildings.end() && it->second.type == type
            && (best == null_entity || bid < best))
            best = bid;
    return best;
}

int count_of_type(const world& w, entity_id n, building_type type)
{
    int c = 0;
    for (const entity_id bid : w.nations.at(n).state_assets)
        if (const auto it = w.buildings.find(bid); it != w.buildings.end() && it->second.type == type)
            ++c;
    return c;
}

/// Complete a site by hand (the construction pass is not this harness's subject).
void finish(world& w, entity_id bid)
{
    building_component& b = w.buildings.at(bid);
    b.ticks_remaining = 0;
    b.days_remaining = 0;
}

/// The scripted programme (P8 runs it twice): twenty quarters.
void programme(fixture& f, const recipe_registry& reg)
{
    for (int q = 0; q < 20; ++q)
        quarter(f.w, reg, k_campaign_settle_ticks + q);
}

// ---- BL-1608 the far-space fixture --------------------------------------------------
// A star, a gridded home planet (6x2: two tiles each for Alpha, Beta and the Seat, the
// player's nation), and an airless moon (16x8 regolith, one iron deposit at (5, 3)).
// Every nation holds every Digitisation and Early Space key, a staffed pad on its own
// capital tile, a centre of 50 thousand there and two million credits; one home shelf
// holds every good. The moon is unpartitioned, as base_harness stages it.
constexpr int FW = 16, FH = 8;

struct far_fixture
{
    world     w;
    entity_id star = null_entity, home = null_entity, moon = null_entity;
    entity_id a = null_entity, b = null_entity, player = null_entity;
    entity_id market = null_entity;
    entity_id cap[3] = {};
    entity_id moon_tile[FW * FH] = {};
};

entity_id far_body(world& w, const char* name, body_type t, entity_id parent, float au, int gw, int gh)
{
    const entity_id id = add_body(w, name, t, parent, au);
    w.bodies.at(id).grid_width  = gw;
    w.bodies.at(id).grid_height = gh;
    w.bodies.at(id).atmosphere  = t == body_type::moon ? atmosphere_class::none : atmosphere_class::moderate;
    return id;
}

entity_id far_tile(world& w, entity_id body, int x, int y, terrain_substrate sub, float hab)
{
    const entity_id id = w.create_entity();
    tile_component tc{};
    tc.body = body;
    tc.grid_x = x;
    tc.grid_y = y;
    tc.substrate = sub;
    tc.landform = terrain_landform::plains;
    tc.habitability = hab;
    w.tiles.emplace(id, tc);
    return id;
}

void grant_far_space(world& w, entity_id nation)
{
    nation_tech_state& st = w.nation_tech[nation];
    for (const io::es::effect& e : io::es::effects)
        if (e.kind == io::es::effect_kind::mechanic && e.node >= 0 && e.node < io::es::node_count)
            st.es_held.set(static_cast<std::size_t>(e.node));
}

uint8_t item(const char* id)
{
    const auto& cat = space_item_catalogue();
    for (std::size_t i = 0; i < cat.size(); ++i)
        if (std::strcmp(cat[i].id, id) == 0)
            return static_cast<uint8_t>(i);
    return k_no_space_item;
}

void build_far(far_fixture& f, float moon_mass = 0.0123f)
{
    world& w = f.w;
    f.star = add_body(w, "Star", body_type::star, null_entity, 0.0f);
    f.home = far_body(w, "Home", body_type::planet, null_entity, 1.0f, 6, 2);
    f.moon = far_body(w, "Moon", body_type::moon, f.home, 0.0026f, FW, FH);
    w.bodies.at(f.moon).mass_earths = moon_mass;
    w.star_body = f.star;
    w.home_body = f.home;
    w.bodies.at(f.home).survey.phase = survey_phase::surveyed;
    const char* names[3] = {"Alpha", "Beta", "Seat"};
    entity_id* ids[3] = {&f.a, &f.b, &f.player};
    for (int i = 0; i < 3; ++i)
    {
        *ids[i] = w.create_entity();
        nation_component nc;
        nc.name = names[i];
        nc.treasury = 2000000.0f;
        nc.astronauts = 40;
        w.nations.emplace(*ids[i], nc);
    }
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 6; ++x)
        {
            const entity_id t = far_tile(w, f.home, x, y, terrain_substrate::sedimentary, 1.0f);
            const int i = x / 2;
            w.set_tile_nation(t, *ids[i]);
            w.nations.at(*ids[i]).tiles.push_back(t);
            if (y == 0 && x % 2 == 0)
                f.cap[i] = t;
        }
    for (int i = 0; i < 3; ++i)
    {
        w.nations.at(*ids[i]).capital_tile = f.cap[i];
        const entity_id c = w.create_entity();
        population_centre_component pc;
        pc.scale = 3;
        pc.population = 50;
        w.population_centres[c] = pc;
        w.population_centre_tile[c] = f.cap[i];
        space_fixture::hold_every_node(w, *ids[i]);
        grant_far_space(w, *ids[i]);
        space_fixture::add_pad(w, *ids[i], f.cap[i], 10.0f);
    }
    w.player_entity = f.player;
    f.market = space_fixture::add_shelf(w, f.home);
    {
        market_component& m = w.markets.at(f.market);
        for (std::size_t r = 0; r < resource_count; ++r)
            if (m.base_price[r] <= 0.0f)
            {
                m.base_price[r] = m.price[r] = 10.0f;
                m.inventory[r] = space_fixture::k_shelf_stock;
            }
    }
    build_province_partition(w, 0x1608u);
    seed_province_holders(w);
    for (int y = 0; y < FH; ++y)
        for (int x = 0; x < FW; ++x)
            f.moon_tile[y * FW + x] = far_tile(w, f.moon, x, y, terrain_substrate::regolith, 0.0f);
    {
        tile_component& tc = w.tiles.at(f.moon_tile[3 * FW + 5]);
        tc.resource_deposit[static_cast<std::size_t>(resource_type::iron_ore)] = 2.0f;
        tc.resource_remaining[static_cast<std::size_t>(resource_type::iron_ore)] = 1.0e6f;
    }
    w.body_tile_index.clear();
    w.current_econ_tick = k_campaign_settle_ticks;
}

/// One quarter as the step runs it for space units: the far pick, the days (moves
/// land), the effects (a landed Lander prospects), the unit quarter (upkeep).
std::vector<far_space_pick> far_quarter(world& w, const recipe_registry& reg, int tick)
{
    w.current_econ_tick = tick;
    std::vector<far_space_pick> picks;
    run_nation_far_space_ai(w, reg, &picks);
    for (int d = 0; d < 90; ++d)
    {
        run_space_unit_day(w, reg.space_units(), reg.stations());
        run_space_battle_day(w, reg.space_units(), reg.stations(), reg.space_battles());
        run_space_strike_day(w, reg.space_battles());
    }
    run_space_effects(w, space_effects_params{}, reg.stations(), 0.0f, tick);
    run_space_unit_quarter(w, reg.space_units(), reg.stations(), tick);
    return picks;
}

const far_space_pick* far_for(const std::vector<far_space_pick>& v, entity_id n)
{
    for (const far_space_pick& p : v)
        if (p.nation == n)
            return &p;
    return nullptr;
}

int units_of(const world& w, entity_id n, space_hull h)
{
    int c = 0;
    for (const auto& [id, u] : w.space_units)
        if (u.owner == n && u.hull == h)
            ++c;
    return c;
}

bool base_of(const world& w, entity_id n, entity_id body)
{
    return offworld_base_capital(w, n, body) != null_entity;
}

void rows_far_space(const recipe_registry& reg)
{
    std::printf("\n-- BL-1608 the far-space pick --\n");
    const nation_ai_params& p = reg.nation_ai();
    std::printf("dials: bases %d, colonists %d, warships %d per hostile (max %d), keep base home %d\n",
                p.far_space_bases_max, p.far_space_colonists, p.far_space_warships_per_hostile,
                p.far_space_warships_max, p.far_space_keep_base_home);

    // ---- F1..F4 the base programme, end to end ------------------------------------------
    {
        far_fixture f;
        build_far(f);
        world& w = f.w;
        survey_map_regions(w, f.a, f.moon, 99); // Alpha's Survey satellite mapped the moon
        const orbital_path_result path = orbital_path(w, orbital_graph_params{},
                                                      orbital_node{f.home, orbital_node_kind::low_orbit},
                                                      orbital_node{f.moon, orbital_node_kind::surface});
        std::printf("    home low orbit -> moon surface: dv %.2f, %.1f days\n", path.dv, path.days);

        const unsigned long long h0 = w.state_hash(0);
        const far_space_pick pre = choose_far_space_action(w, reg, f.a, p);
        check(pre.action == far_space_action::design && pre.hull == static_cast<uint8_t>(space_hull::hauler)
                  && w.state_hash(0) == h0,
              "F1", "the pick is pure, and a keyed rival's first far-space pick designs its first hull (Hauler)");

        std::map<int, int> seen; // action -> first quarter
        std::vector<int> actions_a;
        bool player_row = false, one_verb = true, player_untouched = true;
        int founded_q = -1;
        for (int q = 0; q < 16 && founded_q < 0; ++q)
        {
            const std::size_t units0 = w.space_units.size();
            const std::vector<far_space_pick> picks = far_quarter(w, reg, k_campaign_settle_ticks + q);
            player_row |= far_for(picks, f.player) != nullptr;
            const far_space_pick* k = far_for(picks, f.a);
            if (k != nullptr)
            {
                actions_a.push_back(static_cast<int>(k->action));
                if (k->action != far_space_action::none && k->block != far_space_block::none)
                    std::printf("    q%d Alpha action %d refused (result %d)\n", q, static_cast<int>(k->action),
                                static_cast<int>(k->result));
                if (k->action != far_space_action::none && seen.count(static_cast<int>(k->action)) == 0)
                    seen[static_cast<int>(k->action)] = q;
            }
            int raised = 0;
            for (const far_space_pick& fp : picks)
                if ((fp.action == far_space_action::raise_lander || fp.action == far_space_action::raise_warship)
                    && fp.block == far_space_block::none)
                    ++raised;
            one_verb = one_verb && w.space_units.size() <= units0 + static_cast<std::size_t>(raised)
                    && picks.size() == 2;
            if (base_of(w, f.a, f.moon))
                founded_q = q;
        }
        std::string trail;
        for (const int a : actions_a)
            trail += std::to_string(a) + " ";
        std::printf("    Alpha's actions by quarter: %s\n", trail.c_str());

        int designs = 0;
        bool designs_valid = true;
        if (const auto it = w.space_unit_designs.find(f.a); it != w.space_unit_designs.end())
            for (const auto& [h, d] : it->second)
            {
                ++designs;
                designs_valid = designs_valid && evaluate_space_design(static_cast<space_hull>(h), d).valid;
            }
        check(designs == static_cast<int>(space_hull_count) && designs_valid, "F1",
              fmt("one live design per researched class, each valid: %d designs", designs));
        const space_design* hauler = space_unit_design_of(w, f.a, space_hull::hauler);
        check(hauler != nullptr && (*hauler)[0] == item("drive.nuclear_thermal") && (*hauler)[1] == item("tanks.extended"),
              "F1", "each slot takes the best researched item: nuclear-thermal drive, extended tanks");
        check(seen.count(static_cast<int>(far_space_action::raise_lander)) && seen.count(static_cast<int>(far_space_action::embark))
                  && seen.count(static_cast<int>(far_space_action::load_kit))
                  && seen.count(static_cast<int>(far_space_action::lander_move)),
              "F2", "the Lander steps each chosen when they score: raise, embark, load the kit, move");
        const bool order_ok = seen.count(static_cast<int>(far_space_action::raise_lander))
            && seen.count(static_cast<int>(far_space_action::embark))
            && seen[static_cast<int>(far_space_action::raise_lander)] < seen[static_cast<int>(far_space_action::embark)];
        check(order_ok, "F2", "a Lander is raised before colonists board it");
        check(founded_q >= 0 && base_of(w, f.a, f.moon) && units_of(w, f.a, space_hull::lander) == 2, "F3",
              fmt("Alpha founds a base on the moon (quarter %d), from two Landers: colonists and kit", founded_q));
        bool prospected_site = false;
        if (const entity_id t = offworld_base_capital(w, f.a, f.moon); t != null_entity)
        {
            const tile_component& tc = w.tiles.at(t);
            prospected_site = survey_tile_prospected(w, f.a, f.moon, tc.grid_x, tc.grid_y);
            std::printf("    base tile (%d, %d), iron %.1f\n", tc.grid_x, tc.grid_y,
                        tc.resource_deposit[static_cast<std::size_t>(resource_type::iron_ore)]);
        }
        check(prospected_site, "F3", "the base stands on a tile Alpha has prospected");
        player_untouched = units_of(w, f.player, space_hull::lander) == 0 && units_of(w, f.player, space_hull::hauler) == 0
                        && w.space_unit_designs.count(f.player) == 0
                        && std::fabs(w.nations.at(f.player).treasury - 2000000.0f) < 1.0f;
        check(!player_row && player_untouched && one_verb, "F4",
              "the player's nation, keyed, padded and funded, has no row, no design, no unit; one verb a nation a quarter");
        check(space_haul_eligible(w, f.a, f.a), "F4b",
              "convoys: with its Hauler design live, Alpha's inter-body trade ships by Hauler (the trade seam's)");

        // After the base: the programme is closed (bases_max 1) - no more Landers raised.
        const int landers = units_of(w, f.a, space_hull::lander);
        for (int q = 0; q < 4; ++q)
            (void)far_quarter(w, reg, k_campaign_settle_ticks + 40 + q);
        check(units_of(w, f.a, space_hull::lander) == landers, "F3b",
              "at far_space_bases_max the rival raises no further Lander");
    }

    // ---- F3a the laden Lander: a moon a laden Lander cannot reach ----------------------
    {
        far_fixture f;
        build_far(f, /*moon_mass=*/1.0f); // an Earth-mass moon: the kit cuts the reach short
        world& w = f.w;
        survey_map_regions(w, f.a, f.moon, 99);
        std::vector<int> trail;
        far_space_block last = far_space_block::none;
        for (int q = 0; q < 16; ++q)
        {
            const std::vector<far_space_pick> picks = far_quarter(w, reg, k_campaign_settle_ticks + q);
            if (const far_space_pick* k = far_for(picks, f.a))
            {
                trail.push_back(static_cast<int>(k->action));
                last = k->block;
            }
        }
        std::string t;
        for (const int x : trail)
            t += std::to_string(x) + " ";
        int cargo_landers = 0;
        for (const auto& [uid, u] : w.space_units)
            if (u.owner == f.a && u.hull == space_hull::lander && evaluate_space_design(u.hull, u.design).cargo > 0.0f)
                ++cargo_landers;
        std::printf("    heavy moon: Alpha's actions %s; last block %d\n", t.c_str(), static_cast<int>(last));
        check(cargo_landers == 0 && w.space_hauls.empty() && last == far_space_block::no_dv && !base_of(w, f.a, f.moon),
              "F3a", "a kit no Lander could fly there laden is never bought, nor a cargo Lander raised: no_dv");
    }

    // ---- F5 walls ------------------------------------------------------------------------
    {
        far_fixture nk;
        build_far(nk);
        nk.w.nation_tech[nk.a].es_held.reset();
        const far_space_pick k0 = choose_far_space_action(nk.w, reg, nk.a, p);
        far_fixture nr;
        build_far(nr);
        space_fixture::drop_key(nr.w, nr.a, space_keys::dispatch);
        const far_space_pick k1 = choose_far_space_action(nr.w, reg, nr.a, p);
        check(k0.block == far_space_block::no_hull && k1.block == far_space_block::no_rocketry, "F5",
              fmt("no far key: no_hull (%d); no Rocketry: no_rocketry (%d)", static_cast<int>(k0.block),
                  static_cast<int>(k1.block)));

        far_fixture rs;
        build_far(rs);
        survey_map_regions(rs.w, rs.a, rs.moon, 99);
        for (int q = 0; q < 5; ++q) // the five designs: free
            (void)far_quarter(rs.w, reg, k_campaign_settle_ticks + q);
        rs.w.nations.at(rs.a).treasury = 100.0f;
        const std::string before = to_bytes(rs.w);
        std::vector<far_space_pick> picks;
        rs.w.current_econ_tick = k_campaign_settle_ticks + 5;
        run_nation_far_space_ai(rs.w, reg, &picks);
        const far_space_pick* k = far_for(picks, rs.a);
        check(k != nullptr && k->action == far_space_action::none && k->block == far_space_block::reserve
                  && units_of(rs.w, rs.a, space_hull::lander) == 0,
              "F5", fmt("a raise the reserve forbids is refused cleanly (block %d), no Lander",
                        k ? static_cast<int>(k->block) : -1));

        far_fixture nt;
        build_far(nt); // nothing mapped: no target
        for (int q = 0; q < 5; ++q)
            (void)far_quarter(nt.w, reg, k_campaign_settle_ticks + q);
        const unsigned long long h = nt.w.state_hash(0);
        std::vector<far_space_pick> picks2;
        run_nation_far_space_ai(nt.w, reg, &picks2);
        const far_space_pick* k2 = far_for(picks2, nt.a);
        check(k2 != nullptr && k2->block == far_space_block::no_target && nt.w.state_hash(0) == h, "F5",
              "with no mapped body a Lander has no target: no_target, and the world hashes as before");

        far_fixture st;
        build_far(st);
        st.w.current_econ_tick = k_campaign_settle_ticks - 1;
        std::vector<far_space_pick> picks3;
        check(run_nation_far_space_ai(st.w, reg, &picks3) == 0 && picks3.empty(), "F5",
              "in the settle the pass does nothing");
    }

    // ---- F6 war: raise, bombard, fleet ---------------------------------------------------
    {
        far_fixture f;
        build_far(f);
        world& w = f.w;
        // Beta's extraction site on its own capital tile: a building a strike wears.
        const entity_id site = w.create_entity();
        {
            building_component b{};
            b.tile = f.cap[1];
            b.type = building_type::extraction_site;
            b.ticks_remaining = 0;
            b.condition_permille = 1000;
            w.buildings.emplace(site, b);
            w.nations.at(f.b).state_assets.push_back(site);
        }
        w.nation_tech[f.b].es_held.reset(); // Beta fields nothing: the subject is Alpha's war, not a battle
        for (int q = 0; q < 5; ++q)
            (void)far_quarter(w, reg, k_campaign_settle_ticks + q);
        const far_space_pick calm = choose_far_space_action(w, reg, f.a, p);
        check(calm.action != far_space_action::raise_warship && units_of(w, f.a, space_hull::frigate) == 0, "F6",
              "at peace no warship is raised");
        declare_hostile(w, f.b, f.a);
        std::map<int, int> seen;
        for (int q = 5; q < 9; ++q)
        {
            const std::vector<far_space_pick> picks = far_quarter(w, reg, k_campaign_settle_ticks + q);
            if (const far_space_pick* k = far_for(picks, f.a); k && k->block == far_space_block::none)
                seen.emplace(static_cast<int>(k->action), q);
        }
        const int frigates = units_of(w, f.a, space_hull::frigate);
        bool order_on_site = false;
        for (const auto& [uid, o] : w.space_bombard_orders)
            if (w.space_units.at(uid).owner == f.a && o.kind == space_bombard_target::building && o.target == site)
                order_on_site = true;
        check(seen.count(static_cast<int>(far_space_action::raise_warship)) && frigates == p.far_space_warships_per_hostile,
              "F6", fmt("at war Alpha raises Frigates to its count: %d", frigates));
        // The order drops itself once the building is worn past striking (space_bombard.hpp).
        check((order_on_site || seen.count(static_cast<int>(far_space_action::bombard)) != 0)
                  && w.buildings.at(site).condition_permille < 1000, "F6",
              fmt("a missile Frigate in the hostile's low orbit bombards its building (condition %d)",
                  w.buildings.at(site).condition_permille));
        check(units_of(w, f.player, space_hull::frigate) == 0, "F6", "the player's nation raises nothing");

        // FLEET: stage Alpha's frigates at the moon's low orbit; the war's node is home low orbit.
        for (auto& [uid, u] : w.space_units)
            if (u.owner == f.a && u.hull == space_hull::frigate)
            {
                u.state = space_unit_state::idle;
                u.node = u.dest = {f.moon, orbital_node_kind::low_orbit};
                u.propellant = evaluate_space_design(u.hull, u.design).tank;
            }
        for (auto it = w.space_bombard_orders.begin(); it != w.space_bombard_orders.end();)
            it = w.space_units.at(it->first).owner == f.a ? w.space_bombard_orders.erase(it) : std::next(it);
        const far_space_pick fm = choose_far_space_action(w, reg, f.a, p);
        check(fm.action == far_space_action::fleet_move && fm.fleet && fm.count == frigates
                  && fm.to == orbital_node{f.home, orbital_node_kind::low_orbit},
              "F6", fmt("the fleet away from the war's node is ordered to it whole (%d units)", fm.count));
        std::vector<far_space_pick> picks;
        w.current_econ_tick = k_campaign_settle_ticks + 9;
        run_nation_far_space_ai(w, reg, &picks);
        int going = 0;
        for (const auto& [uid, u] : w.space_units)
            if (u.owner == f.a && u.hull == space_hull::frigate && u.state == space_unit_state::in_transit
                && u.dest == orbital_node{f.home, orbital_node_kind::low_orbit})
                ++going;
        check(going == frigates, "F6", fmt("applied: %d Frigates in transit home", going));
    }

    // ---- F7 the battery ------------------------------------------------------------------
    {
        recipe_registry greg;
        ground g;
        build_ground(g);
        world& w = g.w;
        space_fixture::hold_every_node(w, g.a);
        grant_far_space(w, g.a);
        // Alpha's pad, built: the battery is the call, not the pad.
        const entity_id pad = space_fixture::add_pad(w, g.a, g.tiles_a[1], 10.0f);
        (void)pad;
        const space_ground_pick calm = score_nation_space_ground(w, greg, g.a, k_campaign_settle_ticks, false);
        declare_hostile(w, g.b, g.a);
        const space_ground_pick no_ship = score_nation_space_ground(w, greg, g.a, k_campaign_settle_ticks, false);
        // A Beta Frigate in the home low orbit.
        const entity_id fid = w.create_entity();
        {
            space_unit u;
            u.id = fid;
            u.owner = g.b;
            u.hull = space_hull::frigate;
            u.design.fill(k_no_space_item);
            u.design[0] = item("drive.chemical");
            u.design[1] = item("tanks.standard");
            u.design[2] = item("armament.missile");
            u.design[3] = item("armour.whipple");
            u.state = space_unit_state::idle;
            u.node = u.dest = {g.home, orbital_node_kind::low_orbit};
            u.hull_points = evaluate_space_design(u.hull, u.design).hull_points;
            w.space_units.emplace(fid, u);
        }
        const space_ground_pick held = score_nation_space_ground(w, greg, g.a, k_campaign_settle_ticks);
        check(calm.type != building_type::orbital_defence_battery && no_ship.type != building_type::orbital_defence_battery
                  && held.type == building_type::orbital_defence_battery && held.block == space_ground_block::none
                  && count_of_type(w, g.a, building_type::orbital_defence_battery) == 1,
              "F7", fmt("a hostile Frigate holding home orbit: Alpha builds an orbital defence battery (block %d)",
                        static_cast<int>(held.block)));
        const space_ground_pick again = score_nation_space_ground(w, greg, g.a, k_campaign_settle_ticks + 1, false);
        check(again.type != building_type::orbital_defence_battery && count_of_type(w, g.player, building_type::orbital_defence_battery) == 0,
              "F7", "one battery on the body is enough; the player's nation is never built for");
    }

    // ---- F8 the keep-home dial -----------------------------------------------------------
    check(nation_ai_params{}.far_space_keep_base_home == 1, "F8",
          "NR-1206 is granted (Ben, 2026-11-10): the keep-base-home dial defaults ON");

    // ---- F9 determinism ------------------------------------------------------------------
    {
        far_fixture f1, f2;
        build_far(f1);
        build_far(f2);
        for (far_fixture* f : {&f1, &f2})
        {
            survey_map_regions(f->w, f->a, f->moon, 99);
            survey_map_regions(f->w, f->b, f->moon, 99);
            declare_hostile(f->w, f->b, f->a);
            for (int q = 0; q < 24; ++q)
                (void)far_quarter(f->w, reg, k_campaign_settle_ticks + q);
        }
        check(f1.w.state_hash(0) == f2.w.state_hash(0) && to_bytes(f1.w) == to_bytes(f2.w) && !f1.w.space_units.empty(),
              "F9", fmt("two far-space runs end byte-identical (%zu units, %zu bases)", f1.w.space_units.size(),
                        f1.w.offworld_bases.size()));
    }
}

} // namespace

int main()
{
    const recipe_registry reg;
    const nation_ai_params& p = reg.nation_ai();
    std::printf("space_ai_harness — BL-1589 (rival space AI)\n");
    std::printf("dials: reserve %.2f of treasury + %d quarters of space upkeep; key relief %.2f\n",
                p.space_reserve_share, p.space_reserve_upkeep_quarters, p.research_space_key_relief);

    // ---- P1 / P2 / P3 / P4 ----------------------------------------------------
    {
        fixture f;
        build(f);
        world& w = f.w;
        const float ta0 = w.nations.at(f.a).treasury;
        const float tp0 = w.nations.at(f.player).treasury;
        const int32_t pp0 = w.nations.at(f.player).astronauts;

        const space_ai_pick pre = choose_space_action(w, reg, f.a, p);
        std::vector<space_ai_pick> q0 = quarter(w, reg, k_campaign_settle_ticks);
        const space_ai_pick* a0 = pick_for(q0, f.a);
        bool comms = false;
        for (const auto& [id, a] : w.space_assets)
            if (a.owner == f.a && a.cls == space_asset_class::satellite && a.role == space_satellite_role::comms
                && a.node.body == f.home && a.node.kind == orbital_node_kind::low_orbit)
                comms = true;
        const float sat_upkeep = reg.space_assets().upkeep[0];
        check(a0 != nullptr && a0->action == space_ai_action::comms_satellite
                  && a0->result == space_command_result::applied && comms
                  && assets_of_nation(w, f.a) == 1,
              "P1", "a keyed, funded rival's first pick is a comms satellite to home low orbit, applied");
        // The quarter's step paid the satellite's first upkeep (travel 0: active now).
        check(a0 != nullptr && pre.cost > 0.0f && pre.cost == a0->cost
                  && std::fabs(w.nations.at(f.a).treasury - (ta0 - a0->cost - sat_upkeep)) < 1.0f,
              "P1", fmt("treasury debited the quote %.0f (+ upkeep %.0f): %.0f -> %.0f",
                        a0 ? a0->cost : 0.0f, sat_upkeep, ta0, w.nations.at(f.a).treasury));

        std::vector<space_ai_pick> q1 = quarter(w, reg, k_campaign_settle_ticks + 1);
        const space_ai_pick* a1 = pick_for(q1, f.a);
        check(a1 != nullptr && a1->action == space_ai_action::station_core
                  && a1->result == space_command_result::applied && w.stations.size() >= 1,
              "P2", "the second quarter founds a station core");

        std::vector<uint8_t> fitted;
        bool one_a_quarter = true, acted_means_one = true;
        int prev_a = assets_of_nation(w, f.a), prev_b = assets_of_nation(w, f.b);
        bool player_row = pick_for(q0, f.player) || pick_for(q1, f.player);
        for (int q = 2; q < 20; ++q)
        {
            std::vector<space_ai_pick> qq = quarter(w, reg, k_campaign_settle_ticks + q);
            player_row |= pick_for(qq, f.player) != nullptr;
            const int na = assets_of_nation(w, f.a), nb = assets_of_nation(w, f.b);
            const space_ai_pick* pa = pick_for(qq, f.a);
            const space_ai_pick* pb = pick_for(qq, f.b);
            one_a_quarter &= (na - prev_a) <= 1 && (nb - prev_b) <= 1;
            if (pa && pa->action != space_ai_action::none)
                acted_means_one &= (na - prev_a) == 1;
            if (pb && pb->action != space_ai_action::none)
                acted_means_one &= (nb - prev_b) == 1;
            if (pa && pa->action == space_ai_action::station_module)
                fitted.push_back(pa->module);
            prev_a = na;
            prev_b = nb;
        }
        // The first four fits follow the target's head: power, research, habitation, life support.
        const bool order_ok = fitted.size() >= 4
            && fitted[0] == static_cast<uint8_t>(station_module_kind::power)
            && fitted[1] == static_cast<uint8_t>(station_module_kind::research)
            && fitted[2] == static_cast<uint8_t>(station_module_kind::habitation)
            && fitted[3] == static_cast<uint8_t>(station_module_kind::life_support);
        check(order_ok, "P2", fmt("modules follow the target layout (%zu fitted; first %d %d %d %d)",
                                   fitted.size(), fitted.size() > 0 ? fitted[0] : -1,
                                   fitted.size() > 1 ? fitted[1] : -1, fitted.size() > 2 ? fitted[2] : -1,
                                   fitted.size() > 3 ? fitted[3] : -1));
        check(one_a_quarter && acted_means_one, "P3",
              "no rival gains more than one asset in a quarter; an acting pick gains exactly one");
        // Station budget: every fitted module works (the cover check held).
        int stopped = 0, working = 0;
        for (const auto& [sid, st] : w.stations)
        {
            const station_budget bud = evaluate_station_budget(w, reg.stations(), sid, w.current_econ_tick);
            stopped += static_cast<int>(bud.stopped.size());
            working += static_cast<int>(bud.working.size());
        }
        check(stopped == 0 && working > 0, "P2",
              fmt("the cover check holds: %d modules working, %d stopped by a shortfall", working, stopped));
        check(assets_of_nation(w, f.player) == 0 && w.nations.at(f.player).treasury == tp0
                  && w.nations.at(f.player).astronauts == pp0 && !player_row,
              "P4", "the player's nation owns nothing in space, its treasury and pool untouched, no pick row");
        std::printf("    rival A ended with %d assets, treasury %.0f, pool %d\n", assets_of_nation(w, f.a),
                    w.nations.at(f.a).treasury, w.nations.at(f.a).astronauts);
    }

    // ---- P5 no keys -------------------------------------------------------------
    {
        fixture f;
        build(f, /*keys=*/false);
        const std::uint64_t h0 = f.w.state_hash(0);
        std::vector<space_ai_pick> picks;
        const int issued = run_nation_space_ai(f.w, reg, &picks);
        bool all_blocked = !picks.empty();
        for (const space_ai_pick& k : picks)
            all_blocked &= k.action == space_ai_action::none && k.block == space_ai_block::no_rocketry;
        check(issued == 0 && all_blocked && f.w.space_assets.empty() && f.w.state_hash(0) == h0,
              "P5", "with no keys nothing happens: no_rocketry, no asset, hash unchanged");
    }

    // ---- P6 the walls -------------------------------------------------------------
    {
        // No pad: the rival's pad goes unstaffed.
        fixture f;
        build(f);
        for (auto& [bid, b] : f.w.buildings)
            b.workforce_assigned = 0.0f;
        const std::uint64_t h0 = f.w.state_hash(0);
        const space_ai_pick k = choose_space_action(f.w, reg, f.a, p);
        run_nation_space_ai(f.w, reg);
        check(k.block == space_ai_block::no_launchpad && f.w.space_assets.empty() && f.w.state_hash(0) == h0,
              "P6", "no staffed pad: no_launchpad, nothing touched");
    }
    {
        // Reserve: a treasury that covers the quote but not the quote plus its share.
        fixture f;
        build(f);
        const space_ai_pick full = choose_space_action(f.w, reg, f.a, p);
        f.w.nations.at(f.a).treasury = full.cost * 1.5f;
        const std::uint64_t h0 = f.w.state_hash(0);
        const float t0 = f.w.nations.at(f.a).treasury;
        std::vector<space_ai_pick> picks;
        run_nation_space_ai(f.w, reg, &picks);
        const space_ai_pick* k = pick_for(picks, f.a);
        check(k && k->action == space_ai_action::none && k->block == space_ai_block::reserve
                  && f.w.nations.at(f.a).treasury == t0 && assets_of_nation(f.w, f.a) == 0,
              "P6", fmt("inside the reserve: reserve, nothing spent (treasury %.0f, quote %.0f)", t0, full.cost));
        (void)h0;
    }
    {
        // Crew: the comms satellite goes, then an empty pool founds nothing.
        fixture f;
        build(f);
        f.w.nations.at(f.a).astronauts = 0;
        quarter(f.w, reg, k_campaign_settle_ticks);
        const std::uint64_t h0 = f.w.state_hash(0);
        const std::size_t n0 = f.w.space_assets.size();
        const space_ai_pick k = choose_space_action(f.w, reg, f.a, p);
        check(k.action == space_ai_action::none && k.block == space_ai_block::crew_short
                  && f.w.state_hash(0) == h0 && f.w.space_assets.size() == n0,
              "P6", "an empty astronaut pool founds no station: crew_short, nothing touched");
    }
    {
        // A pending order waits: a fit queued outside coverage blocks the next pick.
        fixture f;
        build(f);
        space_order o;
        o.id = f.w.next_space_order_id++;
        o.due_tick = f.w.current_econ_tick + 1;
        o.cmd.nation = f.a;
        o.cmd.verb = static_cast<uint8_t>(space_verb::delete_template);
        o.cmd.name = "x";
        f.w.space_orders.push_back(o);
        const space_ai_pick k = choose_space_action(f.w, reg, f.a, p);
        check(k.action == space_ai_action::none && k.block == space_ai_block::pending_order,
              "P6", "a pending near-space order of its own: pending_order, no new pick");
    }

    // ---- P7 settle ------------------------------------------------------------------
    {
        fixture f;
        build(f);
        f.w.current_econ_tick = k_campaign_settle_ticks - 1;
        const std::uint64_t h0 = f.w.state_hash(0);
        std::vector<space_ai_pick> picks;
        const int issued = run_nation_space_ai(f.w, reg, &picks);
        check(issued == 0 && picks.empty() && f.w.state_hash(0) == h0, "P7",
              "in the settle the pass does nothing at all");
    }

    // ---- P8 determinism -----------------------------------------------------------------
    {
        fixture f1, f2;
        build(f1);
        build(f2);
        programme(f1, reg);
        programme(f2, reg);
        const std::uint64_t h1 = f1.w.state_hash(0), h2 = f2.w.state_hash(0);
        check(h1 == h2 && to_bytes(f1.w) == to_bytes(f2.w) && !f1.w.space_assets.empty(), "P8",
              fmt("two runs end equal: hash %016llX, %zu assets, snapshots byte-identical",
                  static_cast<unsigned long long>(h1), f1.w.space_assets.size()));
    }

    // ---- P9 through the step -----------------------------------------------------------
    {
        // BL-1642 (the tick is a month): the pick is one a QUARTER, on the quarter's last
        // month (is_quarter_tick), so the row steps the first such month past the settle.
        int qt = k_campaign_settle_ticks;
        while (!is_quarter_tick(qt))
            ++qt;
        fixture f;
        build(f);
        const int keep_tick = f.w.current_econ_tick;
        if (keep_tick < qt)
            f.w.current_econ_tick = qt;
        economy_report rep;
        run_nation_step(f.w, reg, rep, qt);
        const space_ai_pick* k = pick_for(rep.space_picks, f.a);
        check(k != nullptr && k->action == space_ai_action::comms_satellite && pick_for(rep.space_picks, f.player) == nullptr
                  && rep.space_assets.launched >= 1,
              "P9", fmt("run_nation_step files the picks (%zu rows) and the launch reaches the asset step (%d launched)",
                        rep.space_picks.size(), rep.space_assets.launched));
        // P9b: a month that is not the quarter's last files no pick, near or far.
        fixture g;
        build(g);
        if (g.w.current_econ_tick < qt + 1)
            g.w.current_econ_tick = qt + 1;
        economy_report rg;
        run_nation_step(g.w, reg, rg, qt + 1);
        check(rg.space_picks.empty() && rg.far_space_picks.empty(), "P9b",
              "a month that is not the quarter's last takes no space pick: one a quarter, as ruled");
    }

    // ---- G1..G7 BL-1615 the groundwork: pads and centres --------------------------------
    // A registry with the two buildings priced and timed as scripts/economy.lua
    // authors them (500 / 4 quarters; 300 / 3 quarters): the default one prices
    // every build at zero and builds it instantly.
    recipe_registry greg;
    {
        building_economics e = greg.economics(building_type::launchpad);
        e.build_cost = 500.0f;
        e.build_duration_ticks = 4.0f;
        greg.set_economics(building_type::launchpad, e);
        building_economics c = greg.economics(building_type::astronaut_training_centre);
        c.build_cost = 300.0f;
        c.build_duration_ticks = 3.0f;
        greg.set_economics(building_type::astronaut_training_centre, c);
    }
    {
        ground g;
        build_ground(g);
        world& w = g.w;
        const int tick = k_campaign_settle_ticks;

        // G6 first, on the fresh fixture: nothing is held yet.
        {
            const std::uint64_t h0 = w.state_hash(0);
            const space_ground_pick k = score_nation_space_ground(w, greg, g.a, tick);
            check(k.block == space_ground_block::no_rocketry && w.state_hash(0) == h0 && w.buildings.empty(),
                  "G6", "no Rocketry: no_rocketry, nothing built, hash unchanged");
        }
        hold_dispatch_only(w, g.a);
        hold_dispatch_only(w, g.b);
        hold_dispatch_only(w, g.player);
        {
            const std::uint64_t h0 = w.state_hash(0);
            const space_ground_pick k = score_nation_space_ground(w, greg, g.a, k_campaign_settle_ticks - 1);
            check(k.block == space_ground_block::not_scored && w.state_hash(0) == h0,
                  "G6", "in the settle the pass does nothing, Rocketry or not");
        }
        {
            const float keep = w.nations.at(g.a).treasury;
            w.nations.at(g.a).treasury = 10.0f;
            const std::uint64_t h0 = w.state_hash(0);
            const space_ground_pick k = score_nation_space_ground(w, greg, g.a, tick);
            check(k.block == space_ground_block::treasury && k.type == building_type::launchpad
                      && w.state_hash(0) == h0 && w.buildings.empty(),
                  "G6", fmt("a treasury short of twice the capex (%.0f) builds nothing: treasury", k.cost));
            w.nations.at(g.a).treasury = keep;
        }

        // G1: a rival with Rocketry and money builds its pad, on its own ground, nearest its capital.
        const float ta0 = w.nations.at(g.a).treasury;
        const space_ground_pick a1 = score_nation_space_ground(w, greg, g.a, tick);
        const entity_id pad_a = own_of_type(w, g.a, building_type::launchpad);
        check(a1.block == space_ground_block::none && a1.type == building_type::launchpad
                  && pad_a != null_entity && w.buildings.at(pad_a).tile == g.tiles_a[0]
                  && w.nations.at(g.a).treasury < ta0,
              "G1", fmt("a rival with Rocketry builds a Launchpad on its own capital tile (capex %.0f; treasury %.0f -> %.0f)",
                        a1.cost, ta0, w.nations.at(g.a).treasury));
        check(w.buildings.at(pad_a).workforce_assigned > 0.0f, "G1",
              fmt("the pad is staffed at placement (workforce %.2f), so it counts once complete",
                  w.buildings.at(pad_a).workforce_assigned));
        {
            const space_ground_pick again = score_nation_space_ground(w, greg, g.a, tick + 1);
            check(again.block == space_ground_block::nothing_wanted
                      && count_of_type(w, g.a, building_type::launchpad) == 1,
                  "G1", "a pad building counts: the next evaluation wants nothing (one pad)");
        }

        // G2: a second nation builds its own pad on the same body; a nation's second is refused.
        const space_ground_pick b1 = score_nation_space_ground(w, greg, g.b, tick);
        const entity_id pad_b = own_of_type(w, g.b, building_type::launchpad);
        check(b1.block == space_ground_block::none && pad_b != null_entity && pad_b != pad_a
                  && w.tiles.at(w.buildings.at(pad_b).tile).body == w.tiles.at(w.buildings.at(pad_a).tile).body,
              "G2", "a second nation builds its own Launchpad on the same body");
        {
            const placement_rules::placement_result second =
                placement_rules::can_place_in_world(w, g.tiles_a[1], building_type::launchpad,
                                                    resource_type::iron_ore, -1.0f, g.a);
            const placement_rules::placement_result third =
                placement_rules::can_place_in_world(w, g.tiles_p[1], building_type::launchpad,
                                                    resource_type::iron_ore, -1.0f, g.player);
            check(second.reason == placement_rules::placement_reason::launchpad_exists
                      && std::string(second.message())
                             == "Your nation already has a launchpad on this body"
                      && third.ok(),
                  "G2", fmt("one pad per nation per body: A's second refused (\"%s\"); the player's first admitted",
                            second.message()));
        }

        // G3: the player's nation is never built for, keyed and funded as it is.
        {
            const std::uint64_t h0 = w.state_hash(0);
            const space_ground_pick k = score_nation_space_ground(w, greg, g.player, tick);
            check(k.block == space_ground_block::not_scored && w.state_hash(0) == h0
                      && own_of_type(w, g.player, building_type::launchpad) == null_entity,
                  "G3", "the player's nation is never built for: not_scored, hash unchanged");
        }

        // G4: no centre without the key; then, key held and the pad complete, the centre follows.
        finish(w, pad_a);
        {
            const space_ground_pick k = score_nation_space_ground(w, greg, g.a, tick + 2);
            check(k.block == space_ground_block::nothing_wanted
                      && own_of_type(w, g.a, building_type::astronaut_training_centre) == null_entity,
                  "G4", "a completed pad but no astronaut-training key: no centre");
        }
        space_fixture::hold_every_node(w, g.a);
        {
            // The pad is complete, so one-site-per-tile does not hold the capital tile.
            const space_ground_pick k = score_nation_space_ground(w, greg, g.a, tick + 3);
            const entity_id c = own_of_type(w, g.a, building_type::astronaut_training_centre);
            check(k.block == space_ground_block::none && k.type == building_type::astronaut_training_centre
                      && c != null_entity && w.buildings.at(c).workforce_assigned > 0.0f,
                  "G4", fmt("the key held: an Astronaut Training Centre follows (capex %.0f), staffed", k.cost));
            const space_ground_pick k2 = score_nation_space_ground(w, greg, g.a, tick + 4);
            check(k2.block == space_ground_block::nothing_wanted
                      && count_of_type(w, g.a, building_type::astronaut_training_centre) == 1,
                  "G4", "one centre: the next evaluation wants nothing");
        }

        // G5: the completed pad lets the space pick launch: a comms satellite within 4 quarters.
        {
            space_fixture::add_shelf(w, g.home);
            const space_ai_pick pre = choose_space_action(w, greg, g.a, p);
            std::printf("    rival A before: action %d block %d cost %.0f\n", static_cast<int>(pre.action),
                        static_cast<int>(pre.block), pre.cost);
            int launched_q = -1;
            for (int q = 0; q < 4 && launched_q < 0; ++q)
            {
                const std::vector<space_ai_pick> picks = quarter(w, greg, tick + 5 + q);
                const space_ai_pick* k = pick_for(picks, g.a);
                if (k && k->action == space_ai_action::comms_satellite && k->result == space_command_result::applied)
                    launched_q = q;
            }
            check(launched_q >= 0 && assets_of_nation(w, g.a) >= 1 && assets_of_nation(w, g.player) == 0,
                  "G5", fmt("with its own pad complete, rival A launches a comms satellite (quarter %d after)",
                            launched_q));
            const space_ai_pick kb = choose_space_action(w, greg, g.b, p);
            check(kb.block == space_ai_block::no_launchpad, "G5",
                  "rival B's pad still building: no_launchpad, as the launch gate reads it");
        }
    }

    // ---- G7 determinism of the groundwork ---------------------------------------------------
    {
        ground g1, g2;
        build_ground(g1);
        build_ground(g2);
        for (ground* g : {&g1, &g2})
        {
            space_fixture::hold_every_node(g->w, g->a);
            space_fixture::hold_every_node(g->w, g->b);
            for (int t = 0; t < 3; ++t)
            {
                (void)score_nation_space_ground(g->w, greg, g->a, k_campaign_settle_ticks + t);
                (void)score_nation_space_ground(g->w, greg, g->b, k_campaign_settle_ticks + t);
                for (auto& [bid, b] : g->w.buildings)
                    if (b.type == building_type::launchpad)
                        finish(g->w, bid);
            }
        }
        check(g1.w.state_hash(0) == g2.w.state_hash(0) && to_bytes(g1.w) == to_bytes(g2.w)
                  && g1.w.buildings.size() == 4,
              "G7", fmt("two groundwork runs end equal: %zu buildings, snapshots byte-identical",
                        g1.w.buildings.size()));
    }

    // ---- B1..B5 BL-1630 the standing bid: a staffed pad bids for its launch -----------------
    {
        const space_asset_params& sp = reg.space_assets();
        const float res_mult = 2.0f; // the authored fair-price ceiling (economy.lua price_band)
        const std::size_t ci = static_cast<std::size_t>(resource_type::spacecraft_components);
        const std::size_t pi = static_cast<std::size_t>(resource_type::propellant);
        const auto want_of = [](const std::map<std::pair<entity_id, entity_id>, std::array<float, resource_count>>& m,
                                entity_id n, entity_id mid, std::size_t r) {
            const auto it = m.find(std::make_pair(n, mid));
            return it == m.end() ? 0.0f : it->second[r];
        };

        fixture f;
        build(f);
        world& w = f.w;
        const orbital_path_result path = orbital_path(w, orbital_graph_params{},
                                                      orbital_node{f.home, orbital_node_kind::surface},
                                                      orbital_node{f.home, orbital_node_kind::low_orbit});
        const float want_c = sp.components_lot[0];
        const float want_p = sp.propellant_per_dv[0] * static_cast<float>(path.dv);
        entity_id mid = null_entity;
        float lc = 0.0f, lp = 0.0f;
        const bool has_lot = space_standing_lot(w, sp, f.a, mid, lc, lp);
        check(has_lot && mid == f.market && lc == want_c && lp == want_p && lp > 0.0f, "B1",
              fmt("a padded, keyed nation's standing lot: %.1f components + %.1f propellant at its home market",
                  lc, lp));

        const unsigned long long h0 = w.state_hash(0);
        economy_report rep;
        post_space_launch_wants(w, sp, res_mult, 1.0f, rep);
        bool all = true;
        for (const entity_id n : {f.a, f.b, f.player})
            all = all && want_of(rep.wants, n, f.market, ci) == want_c && want_of(rep.wants, n, f.market, pi) == want_p;
        check(all && rep.hauler_wants.empty() && w.state_hash(0) == h0, "B2",
              "every padded nation posts its lot as a want on its home market; the world is not touched");

        economy_report rep2;
        post_space_launch_wants(w, sp, res_mult, 2.0f, rep2);
        check(want_of(rep2.wants, f.a, f.market, ci) == 2.0f * want_c
                  && want_of(rep2.wants, f.a, f.market, pi) == 2.0f * want_p,
              "B3", "standing_lots 2 bids two lots");

        // Over the fair-price ceiling the components do not bid; their want is silenced.
        w.markets.at(f.market).price[ci] = space_fixture::k_components_price * 2.5f;
        economy_report rep3;
        post_space_launch_wants(w, sp, res_mult, 1.0f, rep3);
        check(want_of(rep3.wants, f.a, f.market, ci) == 0.0f
                  && want_of(rep3.hauler_wants, f.a, f.market, ci) == want_c
                  && want_of(rep3.wants, f.a, f.market, pi) == want_p,
              "B4", "a good over the ceiling does not bid: its want goes to the hauler-only register");
        w.markets.at(f.market).price[ci] = space_fixture::k_components_price;

        // Walls: no keys, an unstaffed pad, the settle, the dial off.
        fixture nk;
        build(nk, /*keys=*/false);
        economy_report r_nk;
        post_space_launch_wants(nk.w, sp, res_mult, 1.0f, r_nk);
        fixture us;
        build(us);
        for (auto& [bid, b] : us.w.buildings)
            if (b.type == building_type::launchpad)
                b.workforce_assigned = 0.0f;
        economy_report r_us;
        post_space_launch_wants(us.w, sp, res_mult, 1.0f, r_us);
        fixture st;
        build(st);
        st.w.current_econ_tick = k_campaign_settle_ticks - 1;
        economy_report r_st;
        post_space_launch_wants(st.w, sp, res_mult, 1.0f, r_st);
        economy_report r_off;
        post_space_launch_wants(w, sp, res_mult, 0.0f, r_off);
        check(r_nk.wants.empty() && r_us.wants.empty() && r_st.wants.empty() && r_off.wants.empty()
                  && r_nk.hauler_wants.empty() && r_us.hauler_wants.empty() && r_st.hauler_wants.empty()
                  && r_off.hauler_wants.empty() && reg.space_programme().standing_lots == 0.0f,
              "B5", "no Rocketry, an unstaffed pad, the settle, standing_lots 0 (the C++ default): no bid");
    }

    rows_far_space(reg); // BL-1608

    std::printf("\nspace_ai_harness: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
