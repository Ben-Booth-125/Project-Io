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

#include "world/air_combat.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/sea_combat.hpp"
#include "world/stance.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cstdio>
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
                                  resource_type::iron_ore, resource_type::petroleum})
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
        check(o.str().size() + world_save_ships_section_bytes(f.w) == b.size(),
              "R5f v79 is a strict prefix of v80, the section its measured size");
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
        check(corrupt_n(43, 1u, 1), "R5u a kit fit before the ship designer is refused");
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

    std::printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
