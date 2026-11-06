// sea_theatre_harness — BL-1430 (sea theatre), MILITARY.md § Sea.
//
// Ships exist: the four classes, the port door, sailing, the daily detect-and-hit pass,
// upkeep and refit, the save section. Requirement group `sea-theatre`, R1..R6.
//
// The field: a body 80 wide and 8 high. Columns 0-7 are A's land, 72-79 B's, the rest open
// water, in vertical strips of four columns, one province each. A's port stands at (7, 3),
// its mouth at (8, 3); B's at (72, 3), its mouth at (71, 3).
//
// Run: node tools/verify/build_harness.js sea_theatre_harness --run

#include "core/battle_dispatch_text.cpp" // BL-1529: interception_line; the builder compiles only src/world, and this TU reads only the world
#include "world/campaign_settle.hpp"
#include "world/market_clearing.hpp"
#include "world/trade.hpp"
#include "world/nation_tech.hpp"
#include "world/supply_system.hpp"
#include "world/air_combat.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/sea_combat.hpp"
#include "world/stance.hpp"
#include "world/unit_designs.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace {

int failures = 0, checks = 0;

void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

std::string to_bytes(const world& w)
{
    std::ostringstream o(std::ios::binary);
    write_world_snapshot(w, o);
    return o.str();
}

bool from_bytes(const std::string& b, world& w)
{
    std::istringstream in(b, std::ios::binary);
    return read_world_snapshot(w, in);
}

constexpr int GW = 80, GH = 8, STRIP = 4;
constexpr uint16_t SUB = 0, DES = 1, CAP = 2, TRA = 3;

struct fixture
{
    world     w;
    entity_id body = null_entity;
    entity_id a = null_entity, b = null_entity;
    entity_id a_port = null_entity, b_port = null_entity;
    entity_id a_market = null_entity, b_market = null_entity;
    std::vector<entity_id> grid;
    std::vector<uint32_t>  strip_id;
};

entity_id tile(const fixture& f, int c, int r) { return f.grid[static_cast<std::size_t>(r * GW + c)]; }
uint32_t  strip(const fixture& f, int k) { return f.strip_id[static_cast<std::size_t>(k)]; }

entity_id add_building(fixture& f, entity_id owner, int c, int r, building_type t, int ticks = 0)
{
    const entity_id id = f.w.create_entity();
    building_component b{};
    b.tile = tile(f, c, r);
    b.type = t;
    b.ticks_remaining = ticks;
    f.w.buildings[id] = b;
    f.w.nations.at(owner).state_assets.push_back(id);
    return id;
}

entity_id add_market(fixture& f, int c, int r, float shelf)
{
    const entity_id m = f.w.create_entity();
    market_component mc{};
    mc.body = f.body;
    mc.centre_tile = tile(f, c, r);
    for (const resource_type g : {resource_type::steel, resource_type::machinery, resource_type::alloys,
                                  resource_type::electronics, resource_type::refined_fuel,
                                  resource_type::iron_ore, resource_type::petroleum,
                                  resource_type::ship_hulls, resource_type::guided_weapons,
                                  resource_type::artillery_pieces})
    {
        const std::size_t i = static_cast<std::size_t>(g);
        mc.base_price[i] = 10.0f; mc.price[i] = 10.0f; mc.inventory[i] = shelf;
    }
    f.w.markets[m] = mc;
    return m;
}

void build(fixture& f, float shelf = 5000.0f)
{
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Field"; bc.type = body_type::planet; bc.grid_width = GW; bc.grid_height = GH;
    w.bodies[f.body] = bc;
    for (int r = 0; r < GH; ++r)
        for (int c = 0; c < GW; ++c)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body = f.body; tc.grid_x = c; tc.grid_y = r;
            const bool land = c < 8 || c >= 72;
            tc.substrate = land ? terrain_substrate::sedimentary : terrain_substrate::ocean;
            tc.cover = terrain_cover::grass;
            tc.cover_density = land ? 150 : 0;
            tc.landform = terrain_landform::plains;
            tc.habitability = 0.5f;
            w.tiles[t] = tc;
            f.grid.push_back(t);
        }
    w.provinces.provinces.clear();
    for (int k = 0; k < GW / STRIP; ++k)
    {
        province p;
        p.body = f.body;
        for (int r = 0; r < GH; ++r)
            for (int c = k * STRIP; c < (k + 1) * STRIP; ++c)
                p.tiles.push_back(tile(f, c, r));
        std::sort(p.tiles.begin(), p.tiles.end());
        p.id = p.tiles.front();
        for (const entity_id t : p.tiles)
            w.provinces.tile_province[t] = p.id;
        f.strip_id.push_back(p.id);
        w.provinces.provinces.push_back(std::move(p));
    }
    f.a = w.create_entity();
    nation_component na{}; na.name = "Aerie"; na.treasury = 1000000.0f; na.sea_recruits = 100000;
    w.nations[f.a] = na;
    f.b = w.create_entity();
    nation_component nb{}; nb.name = "Bastion"; nb.treasury = 1000000.0f; nb.sea_recruits = 100000;
    w.nations[f.b] = nb;
    w.player_entity = f.a;
    for (int r = 0; r < GH; ++r)
        for (int c = 0; c < GW; ++c)
            if (c < 8) w.set_tile_nation(tile(f, c, r), f.a);
            else if (c >= 72) w.set_tile_nation(tile(f, c, r), f.b);
    f.a_market = add_market(f, 5, 3, shelf);
    f.b_market = add_market(f, 74, 3, shelf);
    f.a_port = add_building(f, f.a, 7, 3, building_type::port);
    f.b_port = add_building(f, f.b, 72, 3, building_type::port);
}

recipe_registry make_registry() { return recipe_registry{}; }

corp_command_result hire(fixture& f, const recipe_registry& reg, entity_id actor, entity_id t,
                         uint16_t row, entity_id* out = nullptr)
{
    corp_command cmd;
    cmd.tick = 1; cmd.corp = actor; cmd.verb = corp_verb::hire_ship; cmd.tile = t; cmd.unit_type = row;
    return apply_corp_command(f.w, reg, cmd, out);
}

corp_command_result assign(fixture& f, const recipe_registry& reg, entity_id actor, entity_id ship,
                           sea_mission m, std::vector<uint32_t> provs)
{
    corp_command cmd;
    cmd.tick = 1; cmd.corp = actor; cmd.verb = corp_verb::assign_sea_mission;
    cmd.subject = ship; cmd.order = static_cast<uint32_t>(m);
    for (std::size_t i = 0; i < provs.size() && i < cmd.air_provinces.size(); ++i)
        cmd.air_provinces[i] = provs[i];
    return apply_corp_command(f.w, reg, cmd);
}

/// A raised ship of @p row, standing at (@p c, @p r), direct (fixture).
entity_id add_ship(fixture& f, entity_id owner, entity_id port, uint16_t row, int c, int r)
{
    const entity_id id = f.w.create_entity();
    sea_ship s;
    s.owner = owner; s.home_port = port; s.type = row; s.tile = tile(f, c, r);
    f.w.ships.emplace(id, std::move(s));
    return id;
}

const sea_ship& add_ship_ref(fixture& f, entity_id owner, entity_id port, uint16_t row, int c, int r)
{
    return f.w.ships.at(add_ship(f, owner, port, row, c, r));
}

void days(fixture& f, const recipe_registry& reg, int n, int from = 0)
{
    for (int d = 0; d < n; ++d)
        run_sea_day(f.w, reg, from + d);
}

/// A refusal mutates nothing: the digest of the world is the same either side.
template <typename F>
void refused(fixture& f, F press, corp_command_result want, const std::string& what)
{
    const std::string before = to_bytes(f.w);
    const corp_command_result r = press();
    check(r == want && to_bytes(f.w) == before, what);
}

/// BL-1529: a cargo of @p owner's whose sweep this tick crosses the whole sea lane.
convoy_component& add_crossing(fixture& f, entity_id owner, float qty = 100.0f)
{
    convoy_component cv;
    cv.source_market = f.a_market; cv.dest_market = f.b_market;
    cv.cargo_resource = resource_type::steel; cv.cargo_qty = qty;
    cv.corp = owner; cv.id = f.w.allocate_convoy_id();
    cv.origin_tile = tile(f, 5, 3);
    cv.port_a = tile(f, 7, 3); cv.port_b = tile(f, 72, 3);
    cv.progress_before = 0.0f; cv.progress = 1.0f; cv.speed = 0.5f; cv.arrived = true;
    f.w.convoys.push_back(cv);
    return f.w.convoys.back();
}

/// A ship on station over @p strips, direct (fixture).
entity_id add_on_station(fixture& f, entity_id owner, entity_id port, uint16_t row, sea_mission m,
                         std::vector<int> strips)
{
    const entity_id id = add_ship(f, owner, port, row, strips.front() * STRIP + 1, 3);
    sea_ship& s = f.w.ships.at(id);
    s.mission = m;
    for (const int k : strips)
        s.provinces.push_back(strip(f, k));
    std::sort(s.provinces.begin(), s.provinces.end());
    return id;
}

/// How many of @p n cargoes of b's, each sweeping the ONE tile of its lane in column @p col,
/// a's fleet takes, one tick each. One tile is few enough rolls to read a rate from.
int takes(fixture& f, const recipe_registry& reg, int n, int col = 41, int tick0 = 100)
{
    int taken = 0;
    for (int i = 0; i < n; ++i)
    {
        convoy_component& cv = add_crossing(f, f.b);
        const convoy_route lane = convoy_route_tiles(f.w, cv);
        for (std::size_t li = 1; li < lane.tiles.size(); ++li)
            if (f.w.tiles.at(lane.tiles[li]).grid_x == col)
            {
                cv.progress_before = lane.at[li - 1];
                cv.progress = lane.at[li];
                cv.arrived = false;
                break;
            }
        taken += static_cast<int>(raid_convoys(f.w, reg, tick0 + i).size());
        f.w.convoys.clear();
    }
    return taken;
}

} // namespace

int main()
{
    std::printf("=== sea theatre (BL-1430) ===\n");
    const recipe_registry reg = make_registry();
    check(ship_roster_table().size() == 4, "S0 four sea rows, one per class");

    // -----------------------------------------------------------------------
    std::printf("\nR6 - every illegal hire and assignment is refused and mutates nothing\n");
    {
        fixture f;
        build(f);
        refused(f, [&] { return hire(f, reg, f.a, tile(f, 5, 1), DES); },
                corp_command_result::rejected_placement, "R6a a bare tile is no door");
        refused(f, [&] { return hire(f, reg, f.a, tile(f, 72, 3), DES); },
                corp_command_result::rejected_placement, "R6b a rival's port is no door");
        add_building(f, f.a, 7, 5, building_type::port, 3);
        refused(f, [&] { return hire(f, reg, f.a, tile(f, 7, 5), DES); },
                corp_command_result::rejected_placement, "R6c an unfinished port is no door");
        add_building(f, f.a, 1, 1, building_type::port);
        refused(f, [&] { return hire(f, reg, f.a, tile(f, 1, 1), DES); },
                corp_command_result::rejected_placement, "R6d a port with no water at its mouth is no door");
        refused(f, [&] { return hire(f, reg, f.a, tile(f, 7, 3), 99); },
                corp_command_result::rejected_invalid, "R6e an unknown row is refused");
        {
            fixture g;
            build(g, 0.0f);
            refused(g, [&] { return hire(g, reg, g.a, tile(g, 7, 3), DES); },
                    corp_command_result::rejected_invalid, "R6f a row the shelves cannot field is refused");
        }
        f.w.nations.at(f.a).sea_recruits = 0;
        refused(f, [&] { return hire(f, reg, f.a, tile(f, 7, 3), DES); },
                corp_command_result::rejected_no_recruits, "R6g an empty sea pool is refused");
        f.w.nations.at(f.a).sea_recruits = 100000;
        f.w.nations.at(f.a).treasury = 10.0f;
        refused(f, [&] { return hire(f, reg, f.a, tile(f, 7, 3), DES); },
                corp_command_result::rejected_funds, "R6h a short treasury is refused");
        f.w.nations.at(f.a).treasury = 1000000.0f;

        const entity_id d = add_ship(f, f.a, f.a_port, DES, 8, 3);
        const entity_id t = add_ship(f, f.a, f.a_port, TRA, 8, 4);
        const entity_id s = add_ship(f, f.a, f.a_port, SUB, 8, 2);
        const uint32_t near = strip(f, 3);
        refused(f, [&] { return assign(f, reg, f.a, 999999, sea_mission::patrol, {near}); },
                corp_command_result::rejected_invalid, "R6i no such ship");
        refused(f, [&] { return assign(f, reg, f.b, d, sea_mission::patrol, {near}); },
                corp_command_result::rejected_not_owner, "R6j another nation's ship");
        refused(f, [&] { return assign(f, reg, f.a, d, static_cast<sea_mission>(9), {near}); },
                corp_command_result::rejected_invalid, "R6k no such mission");
        refused(f, [&] { return assign(f, reg, f.a, t, sea_mission::patrol, {near}); },
                corp_command_result::rejected_invalid, "R6l a transport works no mission");
        refused(f, [&] { return assign(f, reg, f.a, s, sea_mission::patrol, {near}); },
                corp_command_result::rejected_invalid, "R6m a submarine does not patrol");
        refused(f, [&] { return assign(f, reg, f.a, d, sea_mission::patrol, {}); },
                corp_command_result::rejected_invalid, "R6n a mission with no province");
        refused(f, [&] { return assign(f, reg, f.a, d, sea_mission::none, {near}); },
                corp_command_result::rejected_invalid, "R6o none with a province");
        refused(f, [&] { return assign(f, reg, f.a, d, sea_mission::patrol, {near, near}); },
                corp_command_result::rejected_invalid, "R6p a repeated province");
        refused(f, [&] { return assign(f, reg, f.a, d, sea_mission::patrol, {123456789u}); },
                corp_command_result::rejected_invalid, "R6q an unknown province");
        refused(f, [&] { return assign(f, reg, f.a, d, sea_mission::patrol, {strip(f, 0)}); },
                corp_command_result::rejected_placement, "R6r a land province is no zone");
        refused(f, [&] { return assign(f, reg, f.a, d, sea_mission::patrol, {strip(f, 15)}); },
                corp_command_result::rejected_placement, "R6s a province past the class's range");
        check(assign(f, reg, f.a, d, sea_mission::patrol, {near}) == corp_command_result::applied,
              "R6t a legal assignment lands");
        refused(f, [&] { return assign(f, reg, f.a, d, sea_mission::patrol, {near}); },
                corp_command_result::rejected_state, "R6u the same assignment again changes nothing");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR1 - hire, muster, sail, work, recall\n");
    {
        fixture f;
        build(f);
        const nation_component before = f.w.nations.at(f.a);
        const float steel_before = f.w.markets.at(f.a_market).inventory[static_cast<std::size_t>(resource_type::steel)];
        entity_id d = null_entity;
        check(hire(f, reg, f.a, tile(f, 7, 3), DES, &d) == corp_command_result::applied && d != null_entity,
              "R1a a destroyer is hired at the port");
        const sea_ship* s = &f.w.ships.at(d);
        const ship_class_stats& cs = reg.military().sea.cls[static_cast<std::size_t>(ship_class::destroyer)];
        check(f.w.nations.at(f.a).treasury < before.treasury - cs.hire_cost + 1.0f, "R1b the credits are spent");
        check(f.w.nations.at(f.a).sea_recruits == before.sea_recruits - cs.crew, "R1c the crew leaves the sea pool");
        check(f.w.markets.at(f.a_market).inventory[static_cast<std::size_t>(resource_type::steel)] < steel_before,
              "R1d the hire goods leave the shelf");
        check(s->tile == tile(f, 8, 3) && s->muster_days == cs.muster_days, "R1e it stands at the mouth, mustering");

        const uint32_t zone = strip(f, 4); // columns 16-19
        check(assign(f, reg, f.a, d, sea_mission::patrol, {zone}) == corp_command_result::applied, "R1f assigned while mustering");
        const entity_id at_hire = s->tile;
        days(f, reg, cs.muster_days - 1);
        check(s->tile == at_hire, "R1g it does not sail while mustering");
        days(f, reg, 1, cs.muster_days);
        check(s->muster_days == 0, "R1h raised");

        const int steps = static_cast<int>(s->route.size());
        check(steps == 8, "R1i the course is the eight water steps to the zone's near edge");
        const int pace = cs.days_per_tile_permille;
        days(f, reg, 1, 1000);
        const int first_day = steps - static_cast<int>(s->route.size());
        check(first_day == 1000 / pace, "R1j one day sails the class's pace");
        bool all_water = true;
        for (const entity_id t : s->route)
            all_water = all_water && is_water(f.w.tiles.at(t).substrate);
        check(all_water, "R1k every step is water");
        days(f, reg, 10, 1001);
        check(s->route.empty() && f.w.provinces.province_of(s->tile) == zone, "R1l it arrives and stays in the zone");

        check(assign(f, reg, f.a, d, sea_mission::none, {}) == corp_command_result::applied, "R1m recalled");
        days(f, reg, 15, 1100);
        check(s->tile == tile(f, 8, 3) && s->mission == sea_mission::none, "R1n it sails home to the mouth");

        for (const uint16_t row : {SUB, CAP, TRA})
            check(hire(f, reg, f.a, tile(f, 7, 3), row) == corp_command_result::applied,
                  std::string("R1o the ") + ship_roster_table()[row].name + " is hired too");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR2 - detection, the mask, hostility\n");
    {
        // Peace: nothing happens however close.
        fixture f;
        build(f);
        const entity_id a = add_ship(f, f.a, f.a_port, CAP, 30, 3);
        const entity_id b = add_ship(f, f.b, f.b_port, CAP, 31, 3);
        days(f, reg, 200);
        check(f.w.ships.at(a).hp_ppm == k_ship_hp_full && f.w.ships.at(b).hp_ppm == k_ship_hp_full,
              "R2a at peace no ship is hit");
    }
    {
        // War: each side finds and hits the other, by the mask.
        fixture f;
        build(f);
        declare_hostile(f.w, f.a, f.b);
        const entity_id a = add_ship(f, f.a, f.a_port, CAP, 30, 3);
        const entity_id b = add_ship(f, f.b, f.b_port, CAP, 31, 3);
        days(f, reg, 200);
        const bool a_hit = f.w.ships.count(a) == 0 || f.w.ships.at(a).hp_ppm < k_ship_hp_full;
        const bool b_hit = f.w.ships.count(b) == 0 || f.w.ships.at(b).hp_ppm < k_ship_hp_full;
        check(a_hit && b_hit, "R2b hostility either way opens the fight, and both sides hit");
    }
    {
        // A capital ship never hits a submarine, however often it finds one.
        fixture f;
        build(f);
        declare_hostile(f.w, f.a, f.b);
        const entity_id sub = add_ship(f, f.a, f.a_port, SUB, 30, 3);
        const entity_id cap = add_ship(f, f.b, f.b_port, CAP, 31, 3);
        days(f, reg, 300);
        check(f.w.ships.count(sub) == 1 && f.w.ships.at(sub).hp_ppm == k_ship_hp_full,
              "R2c a capital ship's mask never reaches a submarine");
        check(f.w.ships.count(cap) == 0 || f.w.ships.at(cap).hp_ppm < k_ship_hp_full,
              "R2d the submarine hits the capital ship");
    }
    {
        fixture f;
        build(f);
        declare_hostile(f.w, f.a, f.b);
        const entity_id sub = add_ship(f, f.a, f.a_port, SUB, 30, 3);
        add_ship(f, f.b, f.b_port, CAP, 31, 3);
        f.w.ships.at(sub).morale_permille = 0; // its side detects nothing: every find is the capital's
        int finds = 0;
        for (int d = 0; d < 300; ++d)
            finds += run_sea_day(f.w, reg, d).detections;
        check(finds > 0 && f.w.ships.at(sub).hp_ppm == k_ship_hp_full,
              "R2c2 found " + std::to_string(finds) + " times, the submarine is still untouched");
    }
    {
        // A submarine hits no destroyer; and a destroyer's hits land only on detection days.
        fixture f;
        build(f);
        declare_hostile(f.w, f.a, f.b);
        const entity_id sub = add_ship(f, f.a, f.a_port, SUB, 30, 3);
        const entity_id des = add_ship(f, f.b, f.b_port, DES, 31, 3);
        int hit_days = 0;
        const int total = 20;
        for (int d = 0; d < total && f.w.ships.count(sub) != 0; ++d)
        {
            const int32_t before = f.w.ships.at(sub).hp_ppm;
            run_sea_day(f.w, reg, d);
            if (f.w.ships.count(sub) != 0 && f.w.ships.at(sub).hp_ppm < before)
                ++hit_days;
        }
        check(f.w.ships.at(des).hp_ppm == k_ship_hp_full, "R2e a submarine's mask does not reach a destroyer");
        check(hit_days > 0 && hit_days < total,
              "R2f a destroyer hits a submarine only on the days it finds it (" + std::to_string(hit_days) + " of 20)");
    }
    {
        // Patrol raises detection: a patrolling side finds the submarine more often.
        const auto finds = [&](sea_mission m) {
            int n = 0;
            fixture f;
            build(f);
            declare_hostile(f.w, f.a, f.b);
            const uint32_t zone = f.w.provinces.province_of(tile(f, 28, 3));
            add_ship(f, f.a, f.a_port, SUB, 30, 3);
            f.w.ships.begin()->second.hp_ppm = k_ship_hp_full;
            const entity_id d = add_ship(f, f.b, f.b_port, DES, 28, 3);
            if (m != sea_mission::none)
            {
                f.w.ships.at(d).mission = m;
                f.w.ships.at(d).provinces = {zone};
            }
            for (int day = 0; day < 400; ++day)
            {
                f.w.ships.begin()->second.hp_ppm = k_ship_hp_full; // kept afloat to count
                f.w.ships.begin()->second.morale_permille = 1000;
                f.w.ships.begin()->second.returning = false;
                n += run_sea_day(f.w, reg, day).detections;
            }
            return n;
        };
        const int idle = finds(sea_mission::none), patrol = finds(sea_mission::patrol);
        check(patrol > idle, "R2g a patrol finds more (" + std::to_string(patrol) + " against " + std::to_string(idle) + ")");
        // The same ship with its patrol zone elsewhere is not on station: no raised detection.
        const auto off_station = [&]() {
            int n = 0;
            fixture f;
            build(f);
            declare_hostile(f.w, f.a, f.b);
            add_ship(f, f.a, f.a_port, SUB, 30, 3);
            const entity_id d = add_ship(f, f.b, f.b_port, DES, 28, 3);
            f.w.ships.at(d).mission = sea_mission::patrol;
            f.w.ships.at(d).provinces = {strip(f, 10)};
            for (int day = 0; day < 400; ++day)
            {
                f.w.ships.begin()->second.hp_ppm = k_ship_hp_full;
                f.w.ships.begin()->second.morale_permille = 1000;
                f.w.ships.begin()->second.returning = false;
                n += run_sea_day(f.w, reg, day).detections;
            }
            return n;
        };
        check(off_station() == idle, "R2g2 a patrol off its zone reads as any other ship");
    }
    {
        // A tactical wing hits only a ship its side has found, and takes no fire.
        fixture f;
        build(f);
        declare_hostile(f.w, f.b, f.a);
        const entity_id airbase = add_building(f, f.b, 74, 5, building_type::airbase);
        const uint32_t zone = strip(f, 16); // columns 64-67
        const entity_id wid = f.w.create_entity();
        air_wing wg;
        wg.owner = f.b; wg.airbase = airbase; wg.type = 1; // Strike Wing
        wg.mission = air_mission::tactical; wg.provinces = {zone};
        f.w.wings.emplace(wid, std::move(wg));
        const entity_id cap = add_ship(f, f.a, f.a_port, CAP, 65, 3);
        days(f, reg, 3);
        check(f.w.ships.at(cap).hp_ppm == k_ship_hp_full, "R2h a wing with no detection of its own finds nothing");
        add_ship(f, f.b, f.b_port, TRA, 66, 3); // attack 0: only the wing can hurt the capital ship
        const int32_t hp_before = f.w.wings.at(wid).hp_ppm;
        for (int d = 0; d < 40; ++d)
            (void)run_military_day(f.w, reg, 10 + d); // the game's own door, the air pass included
        check(f.w.ships.count(cap) == 0 || f.w.ships.at(cap).hp_ppm < k_ship_hp_full, "R2i with a spotter the wing's hits land");
        check(f.w.wings.at(wid).hp_ppm == hp_before, "R2j a ship cannot shoot back at a wing");
    }
    {
        // Bombing is the land arithmetic: its directed hostility, and the air share's penalty.
        const auto bombed = [&](bool wing_side_hostile, bool lost_the_air) {
            fixture f;
            build(f);
            if (wing_side_hostile) declare_hostile(f.w, f.b, f.a);
            else                   declare_hostile(f.w, f.a, f.b);
            const entity_id airbase = add_building(f, f.b, 74, 5, building_type::airbase);
            const uint32_t zone = strip(f, 16);
            const entity_id wid = f.w.create_entity();
            air_wing wg;
            wg.owner = f.b; wg.airbase = airbase; wg.type = 1;
            wg.mission = air_mission::tactical; wg.provinces = {zone};
            f.w.wings.emplace(wid, std::move(wg));
            const entity_id cap = add_ship(f, f.a, f.a_port, CAP, 65, 3);
            const entity_id spotter = add_ship(f, f.b, f.b_port, TRA, 66, 3);
            if (lost_the_air)
                f.w.air_superiority[{zone, f.a}] = 1000;
            for (int d = 0; d < 40; ++d)
            {
                // Kept whole, so the spotter's finds do not fade as the capital ship hits it.
                f.w.ships.at(spotter).hp_ppm = k_ship_hp_full;
                f.w.ships.at(spotter).morale_permille = 1000;
                f.w.ships.at(spotter).returning = false;
                run_sea_day(f.w, reg, d);
            }
            return f.w.ships.count(cap) == 0 ? 0 : f.w.ships.at(cap).hp_ppm;
        };
        const int32_t own_air = bombed(true, false), lost = bombed(true, true);
        check(own_air < k_ship_hp_full && lost > own_air,
              "R2k a wing with no air share lands less (" + std::to_string(k_ship_hp_full - lost) + " against "
                  + std::to_string(k_ship_hp_full - own_air) + " ppm)");
        check(bombed(false, false) == k_ship_hp_full, "R2l a wing whose own side has not declared does not bomb");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR3 - break off, refit, sail again; sunk; orphans\n");
    {
        fixture f;
        build(f);
        const uint32_t zone = strip(f, 4);
        const entity_id d = add_ship(f, f.a, f.a_port, DES, 17, 3);
        sea_ship& s = f.w.ships.at(d);
        s.mission = sea_mission::patrol; s.provinces = {zone};
        declare_hostile(f.w, f.a, f.b);
        const entity_id cap = add_ship(f, f.b, f.b_port, CAP, 18, 3);
        s.hp_ppm = 310000; // just above the floor
        bool broke = false;
        for (int day = 0; day < 60 && !broke && f.w.ships.count(d) != 0; ++day)
        {
            run_sea_day(f.w, reg, day);
            broke = f.w.ships.count(d) != 0 && f.w.ships.at(d).returning;
        }
        check(broke, "R3a past its floor it breaks off");
        f.w.ships.erase(cap);
        sea_ship& back = f.w.ships.at(d);
        check(!back.route.empty(), "R3b it has a course home");
        check(back.mission == sea_mission::patrol, "R3c it keeps its mission");
        days(f, reg, 30, 100);
        check(back.tile == tile(f, 8, 3), "R3d it reaches its mouth");
        check(back.returning, "R3e damaged, it does not sail again yet");

        economy_report rep;
        const float steel_before = f.w.markets.at(f.a_market).inventory[static_cast<std::size_t>(resource_type::steel)];
        const int32_t hp0 = back.hp_ppm;
        run_ship_upkeep(f.w, reg, rep);
        check(back.hp_ppm > hp0, "R3f it refits at home");
        check(f.w.markets.at(f.a_market).inventory[static_cast<std::size_t>(resource_type::steel)] < steel_before,
              "R3g refit draws goods");
        for (int q = 0; q < 6; ++q)
            run_ship_upkeep(f.w, reg, rep);
        days(f, reg, 90, 300);
        check(!back.returning, "R3h mended, it sails again");
        check(!back.route.empty() || f.w.provinces.province_of(back.tile) == zone, "R3i back to its zone");
    }
    {
        fixture f;
        build(f);
        declare_hostile(f.w, f.a, f.b);
        const entity_id t = add_ship(f, f.a, f.a_port, TRA, 30, 3);
        f.w.ships.at(t).hp_ppm = 1000;
        add_ship(f, f.b, f.b_port, DES, 31, 3);
        days(f, reg, 200);
        check(f.w.ships.count(t) == 0, "R3j a ship at 0 HP is gone");
    }
    {
        fixture f;
        build(f);
        entity_id m = null_entity;
        check(hire(f, reg, f.a, tile(f, 7, 3), DES, &m) == corp_command_result::applied, "R3k a hire stands mustering");
        const entity_id second = add_building(f, f.a, 7, 6, building_type::port);
        const entity_id raised = add_ship(f, f.a, f.a_port, DES, 9, 3);
        const int pool = f.w.nations.at(f.a).sea_recruits;
        f.w.buildings.erase(f.a_port);
        economy_report rep;
        run_ship_upkeep(f.w, reg, rep);
        check(f.w.ships.count(m) == 0
                  && f.w.nations.at(f.a).sea_recruits == pool + ship_crew(reg, DES),
              "R3l a hire at a lost port is dropped and refunds its crew");
        check(f.w.ships.count(raised) == 1 && f.w.ships.at(raised).home_port == second,
              "R3m a raised ship takes up the other port");
        f.w.buildings.erase(second);
        run_ship_upkeep(f.w, reg, rep);
        check(f.w.ships.count(raised) == 0, "R3n with no own port left it disbands");
    }

    {
        // Working a zone that holds its own mouth is on station, not in port.
        fixture f;
        build(f);
        const entity_id d = add_ship(f, f.a, f.a_port, DES, 8, 3);
        sea_ship& s = f.w.ships.at(d);
        s.mission = sea_mission::patrol;
        s.provinces = {f.w.provinces.province_of(tile(f, 8, 3))};
        s.hp_ppm = 800000;
        s.morale_permille = 800;
        economy_report rep;
        run_ship_upkeep(f.w, reg, rep);
        days(f, reg, 5);
        check(s.hp_ppm == 800000 && s.morale_permille == 800, "R3o a ship on station at its own mouth does not mend");
        s.mission = sea_mission::none;
        s.provinces.clear();
        run_ship_upkeep(f.w, reg, rep);
        days(f, reg, 1, 10);
        check(s.hp_ppm > 800000 && s.morale_permille > 800, "R3p recalled, it does");
    }
    {
        // A land wall cuts the sea: the nearer port by air is no home to a ship that cannot reach it.
        const auto cut = [](fixture& f) {
            for (int r = 0; r < GH; ++r)
                for (const int c : {40, 41})
                    f.w.tiles.at(tile(f, c, r)).substrate = terrain_substrate::sedimentary;
        };
        fixture f;
        build(f);
        cut(f);
        const entity_id beyond = add_building(f, f.a, 41, 3, building_type::port);
        const entity_id home2  = add_building(f, f.a, 7, 6, building_type::port);
        const entity_id ship   = add_ship(f, f.a, f.a_port, DES, 38, 3);
        check(port_mouth_tile(f.w, beyond) != null_entity, "R3q the port beyond the wall has a mouth");
        f.w.buildings.erase(f.a_port);
        economy_report rep;
        run_ship_upkeep(f.w, reg, rep);
        check(f.w.ships.at(ship).home_port == home2, "R3r re-homed by water, past the nearer port it cannot reach");

        fixture g;
        build(g);
        cut(g);
        add_building(g, g.a, 41, 3, building_type::port);
        const entity_id alone = add_ship(g, g.a, g.a_port, DES, 38, 3);
        g.w.buildings.erase(g.a_port);
        economy_report rep2;
        run_ship_upkeep(g.w, reg, rep2);
        check(g.w.ships.count(alone) == 0, "R3s no own port it can sail to: it disbands");
    }
    {
        // A lake is not sea.
        fixture f;
        build(f);
        f.w.tiles.at(tile(f, 8, 3)).substrate = terrain_substrate::lake;
        const auto mouth = f.w.tiles.at(port_mouth_tile(f.w, f.a_port)).substrate;
        check(is_sea(mouth), "R3t a port beside a lake and the sea has its mouth on the sea");
        const entity_id t = tile(f, 4, 3);
        f.w.tiles.at(t).substrate = terrain_substrate::lake;
        add_building(f, f.a, 3, 3, building_type::port);
        refused(f, [&] { return hire(f, reg, f.a, tile(f, 3, 3), DES); },
                corp_command_result::rejected_placement, "R3u a port with only a lake at its mouth is no door");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR7 - through the game's own doors\n");
    {
        fixture f;
        build(f);
        entity_id d = null_entity;
        check(hire(f, reg, f.a, tile(f, 7, 3), DES, &d) == corp_command_result::applied, "R7a hired through the seam");
        f.w.ships.at(d).muster_days = 0;
        check(assign(f, reg, f.a, d, sea_mission::patrol, {strip(f, 5)}) == corp_command_result::applied, "R7b assigned through the seam");
        const std::size_t before = f.w.ships.at(d).route.size();
        const military_day_result md = run_military_day(f.w, reg, 1);
        check(md.sea.sailed > 0 && f.w.ships.at(d).route.size() < before,
              "R7c run_military_day sails a ship in a world with no unit, wing or battle");
        const std::size_t after_day = f.w.ships.at(d).route.size();
        f.w.current_day_tick = f.w.current_econ_tick = 2;
        const economy_report rep = run_economy_step(f.w, reg);
        check(rep.ship_upkeep.ships == 1, "R7d run_economy_step runs the ship's upkeep");
        check(f.w.ships.at(d).route.size() < after_day, "R7e the quarter's catch-up days sail it");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR4 - determinism\n");
    {
        const auto run = [&]() {
            fixture f;
            build(f);
            declare_hostile(f.w, f.a, f.b);
            const uint32_t z = strip(f, 8);
            for (const uint16_t row : {SUB, DES, CAP, TRA})
            {
                add_ship(f, f.a, f.a_port, row, 30, 2);
                add_ship(f, f.b, f.b_port, row, 34, 4);
            }
            for (auto& [sid, s] : f.w.ships)
                if (ship_class_of(s) == ship_class::destroyer)
                    set_ship_mission(f.w, s, sea_mission::patrol, {z});
            days(f, reg, 120);
            return to_bytes(f.w) + std::to_string(f.w.state_hash(1));
        };
        check(run() == run(), "R4a the same world run twice is byte and digest identical");

        fixture a, b;
        build(a);
        build(b);
        const std::uint64_t h0 = a.w.state_hash(1);
        days(a, reg, 30);
        check(a.w.state_hash(1) == h0, "R4b with no ship the sea day changes nothing");
        check(b.w.state_hash(1) == h0, "R4c no ship hashes as before");
        add_ship(b, b.a, b.a_port, DES, 8, 3);
        check(b.w.state_hash(1) != h0, "R4d a ship reaches the digest");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR5 - the save\n");
    {
        fixture f;
        build(f);
        const uint32_t z = strip(f, 5);
        const entity_id airbase = add_building(f, f.a, 3, 3, building_type::airbase);
        const entity_id d = add_ship(f, f.a, f.a_port, DES, 8, 3);
        check(assign(f, reg, f.a, d, sea_mission::patrol, {z}) == corp_command_result::applied, "R5a a ship under way");
        const entity_id d2 = add_ship(f, f.a, f.a_port, CAP, 9, 3);
        f.w.ships.at(d).sail_bank_permille = 123;
        f.w.ships.at(d).hp_ppm = 555555;
        f.w.ships.at(d).returning = false;
        const std::string b = to_bytes(f.w);
        world loaded;
        check(from_bytes(b, loaded), "R5b the stream reads");
        check(loaded.state_hash(1) == f.w.state_hash(1), "R5c the digest survives the trip");
        check(to_bytes(loaded) == b, "R5d the bytes survive the trip");
        check(loaded.ships.at(d).route == f.w.ships.at(d).route && loaded.ships.at(d).provinces == f.w.ships.at(d).provinces,
              "R5e the course and the zone survive");

        std::ostringstream o(std::ios::binary);
        write_world_snapshot_pre_ships(f.w, o);
        check(o.str().size() + world_save_ships_section_bytes(f.w) + world_save_ship_hulls_bytes(f.w) == b.size(),
              "R5f v79 is v81 less the tail and ship hulls' column, each its measured size");
        world older;
        std::istringstream in(o.str(), std::ios::binary);
        check(read_world_snapshot(older, in) && older.ships.empty(), "R5g v79 reads as no ship");

        // A ship outside its domain refuses the whole stream. Offsets from the section's start.
        const std::size_t at = b.size() - world_save_ships_section_bytes(f.w);
        const auto corrupt_n = [&](std::size_t off, uint32_t v, int width) {
            std::string c = b;
            for (int i = 0; i < width; ++i)
                c[at + off + static_cast<std::size_t>(i)] = static_cast<char>((v >> (8 * i)) & 0xFF);
            world t;
            return !from_bytes(c, t);
        };
        const auto corrupt = [&](std::size_t off, uint32_t v) { return corrupt_n(off, v, 4); };
        // count(4) id(4) owner(4) home(4) tile(4) type(2) hp(4) ...
        check(corrupt(8, 999999u), "R5h an owner no nation holds is refused");
        check(corrupt(12, airbase), "R5i a home that is another kind of building is refused");
        check(corrupt(12, 0x7FFFFFF0u) && corrupt(12, 0u), "R5i2 a home past the entity counter, or null, is refused");
        check(corrupt(16, tile(f, 3, 3)), "R5j a ship on land is refused");
        check(corrupt(22, 0u), "R5k a ship at 0 HP is refused");
        check(corrupt(22, static_cast<uint32_t>(k_ship_hp_full + 1)), "R5l HP over full is refused");
        check(corrupt(26, 1001u), "R5m morale over full is refused");
        const std::size_t K = sizeof(sea_ship{}.kit.fit);
        const sea_ship& ds = f.w.ships.at(d);
        check(corrupt(4, 0u), "R5n a null ship id is refused");
        check(corrupt_n(20, 99u, 2), "R5o an unknown row is refused");
        check(corrupt_n(20, TRA, 2), "R5p a class that cannot work its saved mission is refused");
        check(corrupt(30, 1001u), "R5q supply over full is refused");
        check(corrupt(34, 0xFFFFFFFFu) && corrupt(34, 3651u), "R5r a muster out of domain is refused");
        check(corrupt(38, 0xFFFFFFFFu), "R5s a negative sail bank is refused");
        check(corrupt_n(42, 2u, 1), "R5t a returning flag past 1 is refused");
        check(corrupt_n(43, 9u, 1), "R5u a kit variant the class lacks is refused");
        check(corrupt_n(43 + K, 9u, 1), "R5v an unknown mission is refused");
        check(corrupt(44 + K, 0u) && corrupt(44 + K, 9u), "R5w a zone empty under a mission, or past eight, is refused");
        check(corrupt(48 + K, strip(f, 0)), "R5x a land province as a zone is refused");
        check(corrupt(52 + K, 0x7FFFFFF0u), "R5y a route longer than the world is refused");
        check(corrupt(56 + K, tile(f, 3, 3)), "R5z a route step on land is refused");
        check(corrupt(56 + K, tile(f, 8, 7)), "R5aa a route step not next to the ship is refused");
        const std::size_t rec = 3 * 4 + 4 + 2 + 5 * 4 + 1 + K + 1 + 4 + 4 * ds.provinces.size() + 4 + 4 * ds.route.size();
        check(corrupt(4 + rec, d), "R5ab ids out of order are refused");
        check(corrupt(4 + rec + 4, d2), "R5ac (control) the second record's owner is where the walk says");
    }
    {
        // A save between a port's erase and the next upkeep changes nothing about the game.
        fixture f;
        build(f);
        entity_id m = null_entity;
        check(hire(f, reg, f.a, tile(f, 7, 3), DES, &m) == corp_command_result::applied, "R5ad a hire stands mustering");
        add_building(f, f.a, 7, 6, building_type::port);
        const entity_id raised = add_ship(f, f.a, f.a_port, DES, 9, 3);
        // As demolition does: the building and the nation's title to it go together.
        f.w.buildings.erase(f.a_port);
        std::vector<entity_id>& assets = f.w.nations.at(f.a).state_assets;
        assets.erase(std::remove(assets.begin(), assets.end(), f.a_port), assets.end());
        const std::string b = to_bytes(f.w);
        world loaded;
        check(from_bytes(b, loaded) && loaded.ships.count(m) == 1 && loaded.ships.count(raised) == 1,
              "R5ae both ships survive a save in the window");
        check(loaded.state_hash(1) == f.w.state_hash(1) && to_bytes(loaded) == b, "R5af the digest and the bytes survive it");
        economy_report r1, r2;
        run_ship_upkeep(f.w, reg, r1);
        run_ship_upkeep(loaded, reg, r2);
        check(r1.ship_upkeep.rehomed == r2.ship_upkeep.rehomed && to_bytes(loaded) == to_bytes(f.w), "R5ag the quarter plays the same either side");
    }


    // ---- BL-1527: the ship kit (MILITARY.md § Sea) ------------------------------------
    {
        check(ship_kit_catalogue_first_error() == nullptr && ship_kit_catalogue().size() == 33,
              "K1 the catalogue is well-formed, 33 items");
        check(ship_kit_items_for(ship_class::transport, 1).empty() && ship_kit_items_for(ship_class::transport, 0).size() == 3,
              "K1b a transport has no armament, and a hull");
        bool cap_never = true;
        for (const ship_kit_item& it : ship_kit_catalogue())
            if (it.cls == ship_class::capital && (it.delta.targets & ship_class_bit(ship_class::submarine)))
                cap_never = false;
        check(cap_never, "K1c no capital item can hit a submarine");
        bool hulls_draw = true;
        for (const uint16_t id : ship_kit_items_for(ship_class::destroyer, 0))
        {
            const ship_kit_item& it = ship_kit_catalogue()[id];
            if (it.research_node != nullptr && it.goods[0].res != resource_type::ship_hulls)
                hulls_draw = false;
        }
        check(hulls_draw && std::string(ship_kit_slot_name(0)) == "Hull" && std::string(ship_kit_slot_name(2)) == "Sensors",
              "K1d later hulls draw ship hulls; slots are named");
    }
    {
        // The designer's refusals, on the seam. The lock first: no waiver.
        const recipe_registry reg = make_registry();
        fixture f; build(f);
        const uint16_t gas = ship_kit_item_by_id("des.hull.gas_turbine");
        const auto fit = [&](entity_id actor, uint32_t cls, uint16_t slot, uint16_t item) {
            corp_command c;
            c.tick = 1; c.corp = actor; c.verb = corp_verb::set_ship_design;
            c.order = cls; c.recipe = slot; c.unit_type = item;
            return apply_corp_command(f.w, reg, c);
        };
        const uint32_t D = static_cast<uint32_t>(ship_class::destroyer);
        const uint32_t T = static_cast<uint32_t>(ship_class::transport);
        refused(f, [&] { return fit(f.a, D, 0, gas); }, corp_command_result::rejected_tech_locked, "K2 a later item is locked until researched");
        set_kit_research_waived_for_harness(true);
        refused(f, [&] { return fit(f.b, D, 0, gas); }, corp_command_result::rejected_not_owner, "K2b a rival cannot design");
        refused(f, [&] { return fit(f.a, 9, 0, gas); }, corp_command_result::rejected_invalid, "K2c no such class");
        refused(f, [&] { return fit(f.a, D, 9, gas); }, corp_command_result::rejected_invalid, "K2d no such slot");
        refused(f, [&] { return fit(f.a, D, 1, gas); }, corp_command_result::rejected_invalid, "K2e the item fits another slot");
        refused(f, [&] { return fit(f.a, D, 0, 60000); }, corp_command_result::rejected_invalid, "K2f no such item");
        refused(f, [&] { return fit(f.a, T, 1, ship_kit_item_by_id("des.armament.rocket")); }, corp_command_result::rejected_invalid, "K2g a transport takes no armament");
        refused(f, [&] { return fit(f.a, D, 0, ship_kit_item_by_id("des.hull.flush_deck")); }, corp_command_result::rejected_state, "K2h the slot already holds base");
        const std::uint64_t h0 = f.w.state_hash(1);
        check(fit(f.a, D, 0, gas) == corp_command_result::applied && ship_design_fitted(f.w, f.a, ship_class::destroyer, 0) == gas,
              "K3 a researched item fits");
        check(f.w.state_hash(1) != h0, "K3b the design is in the digest");
        refused(f, [&] { return fit(f.a, D, 0, gas); }, corp_command_result::rejected_state, "K3c fitting it twice is refused");

        // The hire records the design and buys its kit.
        const std::size_t SH = static_cast<std::size_t>(resource_type::ship_hulls);
        const float sh0 = f.w.markets.at(f.a_market).inventory[SH];
        entity_id d = null_entity;
        check(hire(f, reg, f.a, tile(f, 7, 3), DES, &d) == corp_command_result::applied
                  && f.w.ships.at(d).kit.fit[0] == ship_kit_variant_of(gas),
              "K4 a hire records the live design");
        check(f.w.markets.at(f.a_market).inventory[SH] < sh0, "K4b the hire draws ship hulls");
        const ship_live_stats base = ship_stats(reg, add_ship_ref(f, f.b, f.b_port, DES, 40, 3));
        const ship_live_stats kitted = ship_stats(reg, f.w.ships.at(d));
        const ship_class_stats& cs = reg.military().sea.cls[static_cast<std::size_t>(ship_class::destroyer)];
        check(base.hp == cs.hp && base.days_per_tile_permille == cs.days_per_tile_permille && base.attack == cs.attack
                  && base.detection == cs.detection && base.stealth == cs.stealth && base.targets == cs.targets,
              "K5 base kit fights on the class's numbers");
        check(kitted.hp == cs.hp + 10 && kitted.days_per_tile_permille == cs.days_per_tile_permille - 50, "K5b kit moves HP and pace");

        // A later design does not reach a ship already built, until it refits.
        const uint16_t quiet = ship_kit_item_by_id("des.hull.quiet");
        check(fit(f.a, D, 0, quiet) == corp_command_result::applied && f.w.ships.at(d).kit.fit[0] == ship_kit_variant_of(gas),
              "K6 a ship keeps the kit it recorded");
        const auto refit = [&](entity_id actor, entity_id ship) {
            corp_command c;
            c.tick = 1; c.corp = actor; c.verb = corp_verb::refit_ship; c.subject = ship;
            return apply_corp_command(f.w, reg, c);
        };
        refused(f, [&] { return refit(f.a, 999999); }, corp_command_result::rejected_invalid, "K7 no such ship");
        refused(f, [&] { return refit(f.b, d); }, corp_command_result::rejected_not_owner, "K7b not your ship");
        refused(f, [&] { return refit(f.a, d); }, corp_command_result::rejected_state, "K7c a ship still raising cannot refit");
        f.w.ships.at(d).muster_days = 0;
        f.w.ships.at(d).mission = sea_mission::patrol;
        refused(f, [&] { return refit(f.a, d); }, corp_command_result::rejected_state, "K7d a ship on a mission cannot refit");
        f.w.ships.at(d).returning = true; // broken off, home, and still holding the mission
        refused(f, [&] { return refit(f.a, d); }, corp_command_result::rejected_state, "K7d2 a broken-off ship still holds its mission");
        f.w.ships.at(d).returning = false;
        f.w.ships.at(d).mission = sea_mission::none;
        const entity_id mouth = f.w.ships.at(d).tile;
        f.w.ships.at(d).tile = tile(f, 40, 3);
        refused(f, [&] { return refit(f.a, d); }, corp_command_result::rejected_placement, "K7e a ship away from its port cannot refit");
        f.w.ships.at(d).tile = mouth;
        const float keep = f.w.markets.at(f.a_market).inventory[SH];
        f.w.markets.at(f.a_market).inventory[SH] = 0.0f;
        refused(f, [&] { return refit(f.a, d); }, corp_command_result::rejected_funds, "K7f no hulls on the shelf");
        f.w.markets.at(f.a_market).inventory[SH] = keep;
        f.w.buildings.at(f.w.ships.at(d).home_port).decommissioned = true;
        refused(f, [&] { return refit(f.a, d); }, corp_command_result::rejected_placement, "K7g a port no longer standing refits nothing");
        f.w.buildings.at(f.w.ships.at(d).home_port).decommissioned = false;
        f.w.ships.at(d).hp_ppm = k_ship_hp_full / 2;
        check(refit(f.a, d) == corp_command_result::applied && f.w.ships.at(d).kit.fit[0] == ship_kit_variant_of(quiet),
              "K8 a refit takes the live design");
        check(f.w.markets.at(f.a_market).inventory[SH] < keep && f.w.ships.at(d).muster_days == cs.muster_days
                  && f.w.ships.at(d).hp_ppm == k_ship_hp_full / 2,
              "K8b it buys the goods, stands out of action, and stays damaged");
        check(f.w.ships.at(d).refitting, "K8d it is marked as a refit, not a hire");
        refused(f, [&] { return refit(f.a, d); }, corp_command_result::rejected_state, "K8e a refit under way cannot be ordered again");
        {
            // A fielded ship in dock: supplied, not mended, and it survives losing its port.
            fixture g = f;
            const entity_id old_port = g.w.ships.at(d).home_port;
            const entity_id second = add_building(g, g.a, 7, 6, building_type::port);
            const float fuel0 = g.w.markets.at(g.a_market).inventory[static_cast<std::size_t>(resource_type::refined_fuel)];
            const int32_t hp0 = g.w.ships.at(d).hp_ppm;
            const int pool0 = g.w.nations.at(g.a).sea_recruits;
            economy_report rep;
            run_ship_upkeep(g.w, reg, rep);
            check(g.w.ships.at(d).hp_ppm == hp0
                      && g.w.markets.at(g.a_market).inventory[static_cast<std::size_t>(resource_type::refined_fuel)] < fuel0,
                  "K8f in dock it draws its supply and does not mend");
            g.w.buildings.erase(old_port);
            run_ship_upkeep(g.w, reg, rep);
            check(g.w.ships.count(d) == 1 && g.w.ships.at(d).home_port == second && g.w.ships.at(d).refitting
                      && g.w.nations.at(g.a).sea_recruits == pool0,
                  "K8g a refit whose port is lost takes up another, no crew refunded");
            const int left = g.w.ships.at(d).muster_days;
            for (int day = 0; day < left; ++day)
                run_sea_day(g.w, reg, 2 + day);
            check(g.w.ships.at(d).muster_days == 0 && !g.w.ships.at(d).refitting, "K8h the refit ends with its days");
        }

        // The save: v81 round trips designs and kit; v80 reads as no design; a bad section refuses whole.
        const std::string b = to_bytes(f.w);
        world loaded;
        check(from_bytes(b, loaded) && loaded.nations.at(f.a).ship_design_kit == f.w.nations.at(f.a).ship_design_kit
                  && loaded.ships.at(d).kit.fit == f.w.ships.at(d).kit.fit && loaded.ships.at(d).refitting
                  && to_bytes(loaded) == b && loaded.state_hash(1) == f.w.state_hash(1),
              "K9 v81 round trips the designs and a ship's kit");
        const std::size_t sec = b.size() - 14 - 4; // count, nation, n, one id, then the closed ports' count
        const auto spoil = [&](std::size_t off, uint32_t v, int width) {
            std::string c = b;
            for (int i = 0; i < width; ++i)
                c[sec + off + static_cast<std::size_t>(i)] = static_cast<char>((v >> (8 * i)) & 0xFF);
            world x;
            return from_bytes(c, x);
        };
        check(spoil(12, ship_kit_item_by_id("des.hull.quiet"), 2), "K9b (control) the id is where the walk says");
        check(!spoil(12, 60000, 2), "K9c an unknown item is refused");
        check(!spoil(12, ship_kit_item_by_id("des.hull.flush_deck"), 2), "K9d a base item stored is refused");
        check(!spoil(4, 999999, 4), "K9e a nation the stream lacks is refused");
        check(!spoil(0, 99999999, 4), "K9f a count past the nations is refused");
        {
            std::string c = b;
            const std::size_t kit_at = b.size() - world_save_ships_section_bytes(f.w) + 4 + 4 * 4 + 2 + 4 * 5 + 1;
            // The first ship is the destroyer; a variant its slot lacks refuses the whole stream.
            c[kit_at] = static_cast<char>(9);
            world x;
            check(!from_bytes(c, x), "K9g a kit variant the class lacks is refused");
        }
        std::ostringstream o80(std::ios::binary);
        write_world_snapshot_pre_ship_kit(f.w, o80);
        world old;
        std::istringstream in80(o80.str(), std::ios::binary);
        check(read_world_snapshot(old, in80) && old.nations.at(f.a).ship_design_kit.empty()
                  && old.markets.at(f.a_market).inventory[SH] == 0.0f,
              "K10 a v80 stream reads 55 wide: no design, no ship hulls");
        check(old.ships.at(d).kit.fit == unit_design{}.fit && !old.ships.at(d).refitting, "K10b its ships hold base kit, none in refit");
        {
            // The v80 tail is today's less the design section (14 bytes here), the closed ports' count
            // and each ship's refit byte.
            std::string c = o80.str();
            const std::size_t tail = world_save_ships_section_bytes(f.w) - 14 - 4 - f.w.ships.size();
            c[c.size() - tail + 4 + 4 * 4 + 2 + 4 * 5 + 1] = static_cast<char>(1);
            std::istringstream in(c, std::ios::binary);
            world x;
            check(!read_world_snapshot(x, in), "K10c a v80 stream holding later kit is refused whole");
        }
        {
            // The refit byte closes the last ship's record, just before the 14-byte design section.
            std::string c = b;
            c[b.size() - 14 - 1] = static_cast<char>(2);
            world x;
            check(!from_bytes(c, x), "K9h a refit flag out of domain is refused");
        }
        set_kit_research_waived_for_harness(false);
    }
    {
        // Armament widens the mask; the fight reads it.
        const recipe_registry reg = make_registry();
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        const entity_id a = add_ship(f, f.a, f.a_port, SUB, 40, 3);
        check((ship_stats(reg, f.w.ships.at(a)).targets & ship_class_bit(ship_class::submarine)) == 0, "K11 a base submarine cannot hit a submarine");
        f.w.ships.at(a).kit.fit[1] = 2; // Wire-guided hunter torpedoes
        const ship_live_stats k = ship_stats(reg, f.w.ships.at(a));
        check((k.targets & ship_class_bit(ship_class::submarine)) != 0
                  && k.attack == reg.military().sea.cls[0].attack + 1500, "K11b hunter torpedoes can, and hit harder");
        const entity_id t = add_ship(f, f.b, f.b_port, SUB, 40, 3);
        f.w.ships.at(a).kit.fit[2] = 2;
        int hit_day = -1;
        for (int d = 0; d < 400 && hit_day < 0; ++d)
        {
            run_sea_day(f.w, reg, d);
            if (f.w.ships.count(t) == 0 || f.w.ships.at(t).hp_ppm < k_ship_hp_full)
                hit_day = d;
        }
        check(hit_day >= 0, "K11c the widened mask lands on a submarine in the daily pass");
    }
    {
        // BL-1528 (naval branch): the tree's own node opens the item, with no waiver.
        fixture f; build(f);
        set_kit_research_waived_for_harness(false);
        const uint16_t turbine = ship_kit_item_by_id("des.hull.gas_turbine");
        const uint16_t quiet = ship_kit_item_by_id("des.hull.quiet");
        const int a1 = io::dg::find_node("DG-ST-NV-A1");
        check(a1 >= 0 && !ship_kit_item_researched(f.w, f.a, turbine), "K14 the Naval node exists and its item starts locked");
        f.w.nation_tech[f.a].held.set(static_cast<std::size_t>(a1));
        check(ship_kit_item_researched(f.w, f.a, turbine) && !ship_kit_item_researched(f.w, f.a, quiet),
              "K14b holding the node opens its item and no other");
    }
    {
        // The daily pass reads kit: pace on a course, HP against the same fire.
        const recipe_registry reg = make_registry();
        const auto tiles_sailed = [&](uint8_t hull) {
            fixture f; build(f);
            const entity_id s = add_ship(f, f.a, f.a_port, DES, 8, 3);
            f.w.ships.at(s).kit.fit[0] = hull;
            for (int c = 9; c <= 38; ++c)
                f.w.ships.at(s).route.push_back(tile(f, c, 3));
            const std::size_t n = f.w.ships.at(s).route.size();
            for (int day = 0; day < 8; ++day)
                run_sea_day(f.w, reg, day);
            return n - f.w.ships.at(s).route.size();
        };
        check(tiles_sailed(1) > tiles_sailed(0), "K13 a faster hull sails further in the daily pass");
        const auto damage_taken = [&](uint8_t hull) {
            fixture f; build(f);
            declare_hostile(f.w, f.a, f.b);
            const entity_id t = add_ship(f, f.a, f.a_port, DES, 40, 3);
            f.w.ships.at(t).kit.fit[0] = hull;
            add_ship(f, f.b, f.b_port, DES, 40, 3);
            for (int day = 0; day < 6 && f.w.ships.count(t) != 0; ++day)
                run_sea_day(f.w, reg, day);
            return f.w.ships.count(t) == 0 ? k_ship_hp_full : k_ship_hp_full - f.w.ships.at(t).hp_ppm;
        };
        const int taken_base = damage_taken(0);
        check(taken_base > 0 && damage_taken(1) < taken_base, "K13b more hull loses less of itself to the same fire");
    }
    {
        // A sensor raises a side's detection; a stealth hull lowers the odds of being found.
        const recipe_registry reg = make_registry();
        fixture f; build(f);
        const entity_id s = add_ship(f, f.a, f.a_port, SUB, 40, 3);
        const ship_class_stats& cs = reg.military().sea.cls[0];
        f.w.ships.at(s).kit.fit = {2, 0, 2};
        const ship_live_stats k = ship_stats(reg, f.w.ships.at(s));
        check(k.stealth == cs.stealth + 150 && k.detection == cs.detection + 60
                  && k.days_per_tile_permille == cs.days_per_tile_permille - 200 && k.hp == cs.hp + 20,
              "K12 hull and sensors add up on the class");
        const entity_id tr = add_ship(f, f.a, f.a_port, TRA, 41, 3);
        f.w.ships.at(tr).kit.fit = {2, 0, 0};
        check(ship_stats(reg, f.w.ships.at(tr)).capacity == 2, "K12b a transport's hull sets its capacity");
    }

    // -----------------------------------------------------------------------
    std::printf("\nBL-1529 - the raid\n");
    {
        fixture f; build(f);
        add_on_station(f, f.a, f.a_port, SUB, sea_mission::raid, {10});
        check(takes(f, reg, 300) == 0, "P1 no declaration, no raid");
        declare_hostile(f.w, f.b, f.a);
        check(takes(f, reg, 300) == 0, "P1b hostility is directed: the victim's declaration does not arm the raider");
        {
            // A friend is never raided, even by the one sequence that leaves a friendship
            // and a declaration standing together: a declares, b offers, a accepts.
            fixture g; build(g);
            add_on_station(g, g.a, g.a_port, SUB, sea_mission::raid, {10});
            declare_hostile(g.w, g.a, g.b);
            offer_friendship(g.w, g.b, g.a); accept_friendship(g.w, g.b, g.a);
            check(are_friends(g.w, g.a, g.b) && is_hostile(g.w, g.a, g.b) && takes(g, reg, 300) == 0, "P1c a friend's cargo is not raided");
        }
        declare_hostile(f.w, f.a, f.b);
        fixture g = f;
        const int alone = takes(f, reg, 1000);
        check(alone > 60, "P2 an unescorted cargo is taken, 1000 one-tile sweeps: " + std::to_string(alone));
        check(takes(g, reg, 1000) == alone, "P3 the same seed, day and cargo roll the same");
        add_crossing(f, f.a);
        check(raid_convoys(f.w, reg, 7).empty() && f.w.convoys.size() == 1, "P4 a raider leaves its own nation's cargo alone");
        f.w.convoys.clear();
    }
    {
        // Mission and station gates.
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        const entity_id s = add_on_station(f, f.a, f.a_port, DES, sea_mission::raid, {10});
        check(takes(f, reg, 300) > 0, "P5 control: on station, the same raider takes");
        f.w.ships.at(s).returning = true;
        check(takes(f, reg, 300) == 0, "P5 a broken-off raider takes nothing");
        f.w.ships.at(s).returning = false;
        f.w.ships.at(s).mission = sea_mission::patrol;
        check(takes(f, reg, 300) == 0, "P5b a patrol takes nothing");
        f.w.ships.at(s).mission = sea_mission::raid;
        f.w.ships.at(s).route.push_back(tile(f, 42, 3));
        check(takes(f, reg, 300) == 0, "P5c a raider still sailing takes nothing");
    }
    {
        // Sink and prize, and that neither mints.
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        add_on_station(f, f.a, f.a_port, SUB, sea_mission::raid, {10});
        std::vector<interception_record> cuts;
        for (int t = 0; t < 400 && cuts.empty(); ++t)
        {
            add_crossing(f, f.b, 40.0f);
            cuts = raid_convoys(f.w, reg, t);
            if (cuts.empty()) f.w.convoys.clear();
        }
        check(cuts.size() == 1 && cuts[0].outcome == interception_outcome::raid_sunk && f.w.convoys.empty(),
              "P6 a submarine sinks what it takes, and the convoy is gone");
        check(f.w.landed_this_tick.empty(), "P6b a sunk cargo lands nowhere");
        check(cuts.size() == 1 && cuts[0].unseen, "P6c no ship of the owner's in the province, so an unseen raider");
    }
    {
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        add_on_station(f, f.a, f.a_port, DES, sea_mission::raid, {3});
        std::vector<interception_record> cuts;
        for (int t = 0; t < 400 && cuts.empty(); ++t)
        {
            add_crossing(f, f.b, 40.0f);
            cuts = raid_convoys(f.w, reg, t);
            if (cuts.empty()) f.w.convoys.clear();
        }
        const entity_id m = cuts.empty() ? null_entity : cuts[0].prize_market;
        check(cuts.size() == 1 && cuts[0].outcome == interception_outcome::raid_prize && m == market_for_tile(f.w, tile(f, 7, 3)),
              "P7 a surface raider lands a prize at the market of its nearest own port");
        check(f.w.convoys.empty(), "P7a a taken prize's convoy is gone, so it cannot also arrive");
        check(m != null_entity && f.w.landed(f.a, m, static_cast<std::size_t>(resource_type::steel)) == 40.0f && f.w.landed_this_tick.size() == 1,
              "P7b the whole cargo lands there for the raider's nation, no more and no less");
    }
    {
        // A prize with no own port in range is sunk. Strip 16 is 57+ steps from a's port; range 25.
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        add_on_station(f, f.a, f.a_port, DES, sea_mission::raid, {16});
        std::vector<interception_record> cuts;
        for (int t = 0; t < 400 && cuts.empty(); ++t)
        {
            add_crossing(f, f.b, 40.0f);
            cuts = raid_convoys(f.w, reg, t);
            if (cuts.empty()) f.w.convoys.clear();
        }
        check(cuts.size() == 1 && cuts[0].outcome == interception_outcome::raid_sunk && f.w.landed_this_tick.empty(),
              "P8 a surface raider with no own port in range sinks its prize");
    }
    {
        // The escort: only one that found the raider, and whose mask reaches it.
        fixture base; build(base);
        declare_hostile(base.w, base.a, base.b);
        add_on_station(base, base.a, base.a_port, DES, sea_mission::raid, {10});
        fixture lone = base;
        const int unescorted = takes(lone, reg, 2000);
        fixture guarded = base;
        for (int i = 0; i < 4; ++i)
            add_on_station(guarded, guarded.b, guarded.b_port, CAP, sea_mission::escort, {10});
        const int escorted = takes(guarded, reg, 2000);
        std::printf("  (raider alone %d, with four capital escorts %d, of 2000)\n", unescorted, escorted);
        check(escorted * 2 < unescorted, "P9 escorts slow a raider they find: " + std::to_string(unescorted) + " to " + std::to_string(escorted));

        fixture sub; build(sub);
        declare_hostile(sub.w, sub.a, sub.b);
        add_on_station(sub, sub.a, sub.a_port, SUB, sea_mission::raid, {10});
        fixture sub_lone = sub;
        const int sub_alone = takes(sub_lone, reg, 2000);
        fixture sub_cap = sub;
        for (int i = 0; i < 4; ++i)
            add_on_station(sub_cap, sub_cap.b, sub_cap.b_port, CAP, sea_mission::escort, {10});
        check(takes(sub_cap, reg, 2000) == sub_alone, "P10 a capital escort cannot hit a submarine, so it does not slow one");
        fixture sub_blind = sub;
        for (int i = 0; i < 4; ++i)
            add_on_station(sub_blind, sub_blind.b, sub_blind.b_port, DES, sea_mission::escort, {10});
        const int sub_escorted = takes(sub_blind, reg, 2000);
        check(sub_escorted < sub_alone, "P11 destroyers that find a submarine do slow it: " + std::to_string(sub_alone) + " to " + std::to_string(sub_escorted));
        check(sub_escorted > sub_alone / 8, "P11b it is not stopped, since most days they do not find it");
        {
            // Roll by roll on one cargo: an unfound submarine rolls exactly as it does alone.
            fixture one_a = sub, one_b = sub;
            for (int i = 0; i < 4; ++i)
                add_on_station(one_b, one_b.b, one_b.b_port, DES, sea_mission::escort, {10});
            int unseen_takes = 0, not_taken_alone = 0;
            for (int t = 0; t < 2000; ++t)
            {
                add_crossing(one_a, one_a.b);
                add_crossing(one_b, one_b.b);
                const auto ca = raid_convoys(one_a.w, reg, 5000 + t);
                const auto cb = raid_convoys(one_b.w, reg, 5000 + t);
                one_a.w.convoys.clear();
                one_b.w.convoys.clear();
                for (const interception_record& r : cb)
                {
                    unseen_takes += r.unseen ? 1 : 0;
                    not_taken_alone += ca.empty() ? 1 : 0;
                }
            }
            check(unseen_takes > 0 && not_taken_alone == 0,
                  "P11c an undetected submarine ignores the escort; an escort never adds a take: " + std::to_string(unseen_takes) + " unseen");
        }

        fixture wide = base;
        wide.w.ships.begin()->second.provinces = {strip(wide, 10), strip(wide, 30)};
        std::sort(wide.w.ships.begin()->second.provinces.begin(), wide.w.ships.begin()->second.provinces.end());
        const int spread = takes(wide, reg, 2000);
        check(spread < unescorted * 3 / 4 && spread > 0, "P12 a raider spread over two provinces takes less in each: " + std::to_string(unescorted) + " to " + std::to_string(spread));
    }
    {
        // A hostile land unit on a sea tile cuts nothing; the same cargo on land is unchanged (interdiction harness).
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        unit_component u{};
        u.owner = f.a; u.count = 100; u.position = tile(f, 40, 3);
        f.w.units[f.w.create_entity()] = u;
        add_crossing(f, f.b);
        f.w.convoys.back().arrived = false;
        const convoy_route lane = convoy_route_tiles(f.w, f.w.convoys.back());
        check(std::find(lane.tiles.begin(), lane.tiles.end(), u.position) != lane.tiles.end(), "P13 control: the unit stands on the lane");
        check(lane.sea_first > 0 && lane.tiles[static_cast<std::size_t>(lane.sea_first)] == tile(f, 7, 3)
                  && lane.tiles[static_cast<std::size_t>(lane.sea_last)] == tile(f, 72, 3),
              "P13a the lane reports its sea leg, port to port");
        check(intercept_convoys(f.w, 3).empty() && f.w.convoys.size() == 1, "P13b a unit on a sea leg's tile cuts nothing");
    }
    {
        // Lane order: a hostile unit on the land leg before the water cuts it before a raider can.
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        add_on_station(f, f.a, f.a_port, SUB, sea_mission::raid, {10, 11, 12, 13, 14});
        unit_component u{};
        u.owner = f.a; u.count = 100;
        add_crossing(f, f.b);
        f.w.convoys.back().arrived = false;
        const convoy_route lane = convoy_route_tiles(f.w, f.w.convoys.back());
        u.position = lane.tiles[1];
        const bool on_land = lane.sea_first > 1;
        f.w.units[f.w.create_entity()] = u;
        int raided = 0;
        for (int t = 0; t < 200; ++t)
        {
            f.w.convoys.back().progress_before = 0.0f;
            raided += static_cast<int>(raid_convoys(f.w, reg, t).size());
        }
        check(on_land && raided == 0, "P15 a cargo cut on the land leg first never reaches the raider");
        const auto cuts = intercept_convoys(f.w, 3);
        check(cuts.size() == 1 && cuts[0].interceptor_unit != null_entity, "P15b the land pass cuts it, on a convoy with ports");
    }
    {
        // The real order, advance -> raid -> arrivals: every cargo lands once, or not at all.
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        add_on_station(f, f.a, f.a_port, DES, sea_mission::raid, {3});
        const std::size_t steel = static_cast<std::size_t>(resource_type::steel);
        int prizes = 0, arrivals = 0, wrong = 0;
        for (int t = 0; t < 300; ++t)
        {
            f.w.landed_this_tick.clear();
            convoy_component& cv = add_crossing(f, f.b, 40.0f);
            cv.progress_before = -1.0f; cv.progress = 0.0f; cv.speed = 2.0f; cv.arrived = false;
            advance_convoys(f.w);
            std::vector<interception_record> cuts = raid_convoys(f.w, reg, t);
            credit_arrived_convoys(f.w, t, &cuts);
            const float home = f.w.landed(f.b, f.b_market, steel);
            float prize = 0.0f;
            for (const auto& [key, stock] : f.w.landed_this_tick)
                if (key.first == f.a) prize += stock.quantities[steel];
            if (cuts.empty()) { ++arrivals; wrong += home == 40.0f && prize == 0.0f ? 0 : 1; }
            else { ++prizes; wrong += home == 0.0f && prize == 40.0f ? 0 : 1; }
            wrong += f.w.convoys.empty() ? 0 : 1;
        }
        check(prizes > 0 && arrivals > 0 && wrong == 0, "P16 through the real pass order each cargo lands exactly once: " + std::to_string(prizes) + " prizes, " + std::to_string(arrivals) + " arrivals");
    }
    {
        // A held cargo sits the whole tick on its tile: in the zone it is exposed, out of it not.
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        add_on_station(f, f.a, f.a_port, SUB, sea_mission::raid, {10});
        const auto held_takes = [&](int col) {
            int n = 0;
            for (int t = 0; t < 100; ++t)
            {
                convoy_component& cv = add_crossing(f, f.b);
                const convoy_route lane = convoy_route_tiles(f.w, cv);
                for (std::size_t li = 1; li < lane.tiles.size(); ++li)
                    if (f.w.tiles.at(lane.tiles[li]).grid_x == col)
                    {
                        cv.progress = lane.at[li];
                        cv.progress_before = lane.at[li];
                        break;
                    }
                cv.held = true; cv.arrived = false;
                n += static_cast<int>(raid_convoys(f.w, reg, t).size());
                f.w.convoys.clear();
            }
            return n;
        };
        check(held_takes(41) > 90 && held_takes(60) == 0, "P17 a cargo held at sea in the zone is exposed all tick, and out of the zone not at all");
    }
    {
        // Who reads as seen, off a real detected take: the owner's destroyers find a destroyer.
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        add_on_station(f, f.a, f.a_port, DES, sea_mission::raid, {10});
        add_on_station(f, f.b, f.b_port, DES, sea_mission::escort, {10});
        int named = 0, unseen = 0;
        for (int t = 0; t < 2000; ++t)
        {
            add_crossing(f, f.b);
            for (const interception_record& r : raid_convoys(f.w, reg, t))
                (r.unseen ? unseen : named) += 1;
            f.w.convoys.clear();
        }
        check(named > 0 && named > unseen, "P18 a raider the owner's side has found is named: " + std::to_string(named) + " named, " + std::to_string(unseen) + " unseen");
    }
    {
        // The real tick carries the record to the report the app posts from.
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        add_on_station(f, f.a, f.a_port, SUB, sea_mission::raid, {10, 11, 12, 13, 14});
        std::size_t reported = 0;
        for (int t = 0; t < 40 && reported == 0; ++t)
        {
            convoy_component& cv = add_crossing(f, f.b, 40.0f);
            cv.progress_before = -1.0f; cv.progress = 0.0f; cv.speed = 2.0f; cv.arrived = false;
            reported = run_settle_tick(f.w, reg, t, t, false).report.interceptions.size();
        }
        check(reported > 0, "P19 run_settle_tick reports the raid on the tick's report");
    }
    {
        // The announcement.
        fixture f; build(f);
        interception_record r;
        r.victim_corp = f.a; r.interceptor_corp = f.b; r.cargo_resource = resource_type::steel; r.cargo_qty = 40.0f;
        r.outcome = interception_outcome::raid_sunk; r.unseen = true;
        const std::string seen_not = interception_line(f.w, r, f.a);
        check(seen_not.find("an unseen raider") != std::string::npos && seen_not.find("Bastion") == std::string::npos,
              "P14 an undetected raider is named as an unseen raider, not by its nation: " + seen_not);
        r.unseen = false;
        check(interception_line(f.w, r, f.a).find("Bastion") != std::string::npos, "P14b a detected one is named");
        check(interception_line(f.w, r, f.b).find("Aerie") != std::string::npos, "P14c the raider's side reads whose cargo it was");
        const entity_id third = f.w.create_entity();
        check(interception_line(f.w, r, third).empty(), "P14d a third party's feed stays quiet");
    }

    // -----------------------------------------------------------------------
    std::printf("\nBL-1530 - sea control and the blockade\n");
    {
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        const uint32_t p = strip(f, 10);
        check(sea_control_share_permille(f.w, reg, p, f.a) == 0 && !sea_holds(f.w, reg, p, f.a), "C1 no ship, no share");
        add_on_station(f, f.a, f.a_port, SUB, sea_mission::raid, {10});
        check(!sea_holds(f.w, reg, p, f.a), "C2 a submarine holds no water");
        add_on_station(f, f.a, f.a_port, TRA, sea_mission::none, {10});
        check(!sea_holds(f.w, reg, p, f.a), "C2b nor does a transport");
        const entity_id da = add_on_station(f, f.a, f.a_port, DES, sea_mission::patrol, {10});
        check(sea_control_share_permille(f.w, reg, p, f.a) == 1000 && sea_holds(f.w, reg, p, f.a), "C3 uncontested, a patrol holds all of it");
        f.w.ships.at(da).route.push_back(tile(f, 42, 3));
        check(!sea_holds(f.w, reg, p, f.a), "C3b a patrol still sailing holds nothing");
        f.w.ships.at(da).route.clear();
        const entity_id db = add_on_station(f, f.b, f.b_port, DES, sea_mission::escort, {10});
        check(sea_control_share_permille(f.w, reg, p, f.a) == 500 && !sea_holds(f.w, reg, p, f.a) && !sea_holds(f.w, reg, p, f.b),
              "C4 equal forces split it and neither reaches the bar");
        add_on_station(f, f.a, f.a_port, CAP, sea_mission::blockade, {10});
        check(sea_holds(f.w, reg, p, f.a) && !sea_holds(f.w, reg, p, f.b), "C5 a capital ship tips it, and only one side holds");
        (void)db;
    }
    {
        // A held province reveals hostile surface ships, and never a submarine.
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        add_on_station(f, f.a, f.a_port, CAP, sea_mission::patrol, {10});
        f.w.ships.at(f.w.ships.rbegin()->first).provinces = {strip(f, 10)};
        // A worn-out holder: its detection is a sliver, so only the hold can find anything daily.
        f.w.ships.begin()->second.supply_factor_permille = 30;
        const entity_id ghost = add_ship(f, f.b, f.b_port, TRA, 41, 3);
        int found_days = 0;
        for (int d = 0; d < 20; ++d)
            found_days += run_sea_day(f.w, reg, 3 + d).detections > 0 ? 1 : 0;
        check(found_days == 20 && f.w.ships.at(ghost).hp_ppm < k_ship_hp_full, "C6 a held province reveals a surface ship every day: " + std::to_string(found_days));
    }
    {
        // Never a submarine: a worn-out destroyer holds the water, and it alone can hit one, so
        // a day the submarine takes a hit is a day it was found. It stays on its stealth.
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        add_on_station(f, f.a, f.a_port, DES, sea_mission::patrol, {10});
        f.w.ships.begin()->second.supply_factor_permille = 30;
        add_ship(f, f.b, f.b_port, SUB, 41, 3);
        int sub_days = 0;
        for (int d = 0; d < 20; ++d)
            sub_days += run_sea_day(f.w, reg, 100 + d).hits > 0 ? 1 : 0;
        check(sea_holds(f.w, reg, strip(f, 10), f.a) && sub_days < 5, "C6b and never a submarine, which stays on its stealth: " + std::to_string(sub_days));
    }
    {
        // Control for C6: no hold, a contested province, and the same sliver finds little.
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        add_on_station(f, f.a, f.a_port, CAP, sea_mission::patrol, {10});
        f.w.ships.begin()->second.supply_factor_permille = 30;
        add_on_station(f, f.b, f.b_port, CAP, sea_mission::patrol, {10});
        f.w.ships.rbegin()->second.supply_factor_permille = 30;
        check(!sea_holds(f.w, reg, strip(f, 10), f.a), "C6c control: an even fight holds nothing");
    }
    {
        // The verb: a blockade only at a hostile port's mouth.
        fixture f; build(f);
        const entity_id d = add_ship(f, f.a, f.a_port, DES, 8, 3);
        const uint32_t mouth = strip(f, 71 / STRIP);
        refused(f, [&] { return assign(f, reg, f.a, d, sea_mission::blockade, {mouth}); },
                corp_command_result::rejected_placement, "C14 no declaration, no blockade to assign");
        declare_hostile(f.w, f.a, f.b);
        refused(f, [&] { return assign(f, reg, f.a, d, sea_mission::blockade, {strip(f, 5)}); },
                corp_command_result::rejected_placement, "C14b open water with no port is no blockade");
    }
    {
        // The blockade: closes the port on the holding edge and opens it on the losing one.
        fixture f; build(f);
        declare_hostile(f.w, f.a, f.b);
        // The field wraps, so a and b touch by land; cut that isthmus so only the water joins them.
        for (int r = 0; r < GH; ++r)
            for (const int c : {76, 77, 78, 79, 0, 1, 2, 3})
                f.w.tiles.at(tile(f, c, r)).substrate = terrain_substrate::ocean;
        invalidate_logistics_caches(f.w);
        const std::size_t strip_mouth = 71 / STRIP; // b's port mouth at (71, 3)
        const entity_id b_port_tile = tile(f, 72, 3);
        const auto ports = [&] { return body_active_port_tiles(f.w, f.body); };
        check(ports().size() == 2, "C7 both ports load before any blockade");
        const convoy_leg open_leg = price_market_leg(f.w, reg, collect_logistics_nodes(f.w), f.a_market, f.b_market, 1.0f, f.a);
        check(open_leg.viable && open_leg.port_a == tile(f, 7, 3) && open_leg.port_b == tile(f, 72, 3),
              "C7b control: before the blockade the route prices over water, port to port");
        const entity_id bl = add_on_station(f, f.a, f.a_port, DES, sea_mission::blockade, {static_cast<int>(strip_mouth)});
        run_sea_day(f.w, reg, 1);
        check(f.w.closed_ports.count(b_port_tile) == 1 && ports().size() == 1 && ports()[0] == tile(f, 7, 3),
              "C8 a blockade held at the mouth closes the port, and the route set reads it as lost the same day");
        const convoy_leg leg = price_market_leg(f.w, reg, collect_logistics_nodes(f.w), f.a_market, f.b_market, 1.0f, f.a);
        check(!leg.viable, "C8b the route pricer reads the port as lost: with no land route, no route");
        recipe_registry treg = reg;
        trade_params tp = treg.trade();
        tp.port_points = 10.0f;
        treg.set_trade(tp);
        check(market_trade_points(f.w, treg, nullptr, f.b).empty() && !market_trade_points(f.w, treg, nullptr, f.a).empty(),
              "C8c the blockaded port makes no trade points, and the other still does");
        // A cargo already at sea finishes its leg.
        convoy_component& cv = add_crossing(f, f.a, 40.0f);
        cv.progress_before = 0.4f; cv.progress = 0.5f; cv.arrived = false;
        const convoy_route lane = convoy_route_tiles(f.w, cv);
        check(!lane.tiles.empty() && lane.sea_first >= 0, "C9 a convoy at sea keeps its lane through a closed port");
        const float quay = lane.at[static_cast<std::size_t>(lane.sea_first)];
        f.w.convoys.clear();
        {
            // Bound through b's closed port, ashore: it waits at its quay however fast it goes.
            convoy_component& ashore = add_crossing(f, f.a, 40.0f);
            ashore.progress_before = -1.0f; ashore.progress = 0.0f; ashore.speed = 0.3f; ashore.arrived = false;
            for (int k = 0; k < 8; ++k)
                advance_convoys(f.w);
            const convoy_component& w0 = f.w.convoys.back();
            check(!w0.arrived && w0.progress == quay, "C9b a cargo not yet at sea waits at its loading port");
            f.w.convoys.clear();
            // Already past the quay: it finishes, into the closed port, and lands.
            convoy_component& at_sea = add_crossing(f, f.a, 40.0f);
            at_sea.progress_before = -1.0f; at_sea.progress = quay + 0.01f; at_sea.speed = 0.4f; at_sea.arrived = false;
            for (int k = 0; k < 4 && !f.w.convoys.empty(); ++k)
            {
                advance_convoys(f.w);
                credit_arrived_convoys(f.w, k);
            }
            check(f.w.convoys.empty() && f.w.landed(f.a, f.b_market, static_cast<std::size_t>(resource_type::steel)) == 40.0f,
                  "C9c a cargo already at sea finishes its leg and lands");
            f.w.landed_this_tick.clear();
        }
        // Round trip.
        const std::string saved = to_bytes(f.w);
        world back;
        check(from_bytes(saved, back) && back.closed_ports == f.w.closed_ports && to_bytes(back) == saved, "C10 a closed port saves and loads, byte for byte");
        check(!price_market_leg(back, reg, collect_logistics_nodes(back), f.a_market, f.b_market, 1.0f, f.a).viable
                  && body_active_port_tiles(back, f.body).size() == 1,
              "C10a the loaded world reads the port closed before any sea day");
        {
            // Untrusted: a closed id that is no tile of the stream.
            std::string c = saved;
            const entity_id bare = f.a;
            std::memcpy(&c[c.size() - 4], &bare, 4);
            world x;
            check(!from_bytes(c, x), "C10b a closed id that is no tile is refused");
        }
        {
            // A port idled since the last sea day is a legal state: the save must load.
            world idled = f.w;
            idled.buildings.at(f.b_port).state_idle = true;
            idled.buildings.at(f.b_port).decommissioned = true;
            const std::string s2 = to_bytes(idled);
            world x;
            check(from_bytes(s2, x) && to_bytes(x) == s2, "C10c a port taken out of service while closed still saves and loads");
        }
        // Lose the water: the port opens that day, for hostility or for force.
        const entity_id guard = add_on_station(f, f.b, f.b_port, CAP, sea_mission::patrol, {static_cast<int>(strip_mouth)});
        (void)guard;
        run_sea_day(f.w, reg, 2);
        check(f.w.closed_ports.empty() && ports().size() == 2, "C11 lose the province and the port reopens that day");
        (void)bl;
        // Hostility ends: the port opens on the next day.
        f.w.ships.erase(guard);
        run_sea_day(f.w, reg, 3);
        check(f.w.closed_ports.size() == 1, "C11b the guard gone, the blockade closes it again");
        return_to_neutral(f.w, f.a, f.b);
        run_sea_day(f.w, reg, 4);
        check(f.w.closed_ports.empty(), "C11c peace opens it");
        // The last ship goes while the port is closed, through the real military day.
        declare_hostile(f.w, f.a, f.b);
        run_sea_day(f.w, reg, 5);
        check(f.w.closed_ports.size() == 1, "C11d closed again");
        f.w.ships.clear();
        run_military_day(f.w, reg, 2000);
        check(f.w.closed_ports.empty(), "C11e no ship left: the military day still runs and opens it");
    }
    {
        // Who may blockade.
        fixture f; build(f);
        const std::size_t strip_mouth = 71 / STRIP;
        add_on_station(f, f.a, f.a_port, DES, sea_mission::blockade, {static_cast<int>(strip_mouth)});
        run_sea_day(f.w, reg, 1);
        check(f.w.closed_ports.empty(), "C12 no declaration, no blockade");
        declare_hostile(f.w, f.b, f.a);
        run_sea_day(f.w, reg, 2);
        check(f.w.closed_ports.empty(), "C12b the port's nation declaring does not arm the blockader");
        declare_hostile(f.w, f.a, f.b);
        f.w.ships.begin()->second.mission = sea_mission::patrol;
        run_sea_day(f.w, reg, 3);
        check(f.w.closed_ports.empty(), "C12c holding the water without a blockade closes nothing");
        f.w.ships.begin()->second.mission = sea_mission::blockade;
        run_sea_day(f.w, reg, 4);
        check(f.w.closed_ports.size() == 1, "C12d control: the same ship on blockade closes it");
        // A blockading ship raids as a raider does.
        int taken = 0;
        for (int t = 0; t < 300; ++t)
        {
            convoy_component& cv = add_crossing(f, f.b);
            cv.port_b = tile(f, 72, 3);
            taken += static_cast<int>(raid_convoys(f.w, reg, t).size());
            f.w.convoys.clear();
        }
        check(taken > 0, "C13 a blockading ship may raid its hostile cargo: " + std::to_string(taken));
    }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
