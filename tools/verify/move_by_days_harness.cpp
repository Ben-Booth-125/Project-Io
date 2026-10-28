// move_by_days_harness.cpp — BL-1445 (move by days; MILITARY.md § Marching, "Units march
// by the day, at their class's speed", and § Battles ruling 3, "Pacing is by the day").
//
// The march and the battle step once per DAY, not once per quarterly economy tick. This
// harness measures what that means, against the AUTHORED data (scripts/economy.lua's
// `economy.military.march_days_per_tile` and `march_days_per_tile_by_cls`), so a number
// printed here is the number the game runs:
//
//   D1  each class's days per plains tile, measured over a twelve-tile march;
//   D2  terrain multiplies (a mountain doubles), a road divides (the road ladder's own
//       discount, `road_traversal_multiplier`);
//   D3  partial progress banks from day to day in `order.progress`;
//   D4  Logistic Points: granted once a quarter, spent tile by tile; a unit whose grant is
//       spent halts and resumes on the next quarter's grant; the credit price is charged
//       per tile at the rate x tile cost x men, the same total per tile as before;
//   D5  contact opens a battle on the day of contact;
//   D6  battle length in days (infantry on plains, by force ratio);
//   D7  determinism: the same fixture twice is byte-identical;
//   D8  a save taken mid-march round-trips, and the loaded world marches on identically;
//   D9  a world stepped day by day (the app) and one caught up by the economy step (every
//       headless driver) end the quarter identically;
//   D10 the loader rejects a non-positive or non-finite speed.
//
// Build: bash tools/verify/build_lua_harness.sh move_by_days_harness (it loads
// scripts/recipes.lua and scripts/economy.lua through a live Lua state). Run from the
// repo root.

#include "world/battle_system.hpp"
#include "world/campaign_battle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/logistic_points.hpp"
#include "world/logistics.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include "scripting/lua_state.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

bool approx(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

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

/// A gw-wide single-row land body.
entity_id make_row_body(world& w, int gw, terrain_landform lf = terrain_landform::plains)
{
    const entity_id body = w.create_entity();
    body_component bc{};
    bc.grid_width  = gw;
    bc.grid_height = 1;
    w.bodies[body] = bc;
    for (int c = 0; c < gw; ++c)
    {
        const entity_id t = w.create_entity();
        tile_component tc{};
        tc.body      = body;
        tc.grid_x    = c;
        tc.grid_y    = 0;
        tc.substrate = terrain_substrate::sedimentary;
        tc.cover     = terrain_cover::grass;
        tc.cover_density = 150;
        tc.landform  = lf;
        w.tiles[t]   = tc;
    }
    return body;
}

entity_id tile_at(world& w, entity_id body, int c)
{
    return body_tile_grid(w, body)[static_cast<std::size_t>(c)];
}

/// A nation holding columns [from, to] of @p body's row.
entity_id add_nation(world& w, entity_id body, int from, int to, float treasury = 100000.0f)
{
    const entity_id n = w.create_entity();
    nation_component nc{};
    nc.name     = "Fixture";
    nc.treasury = treasury;
    for (int c = from; c <= to; ++c)
    {
        const entity_id t = tile_at(w, body, c);
        nc.tiles.push_back(t);
        w.tile_to_nation[t] = n;
    }
    w.nations[n] = nc;
    return n;
}

entity_id add_hub(world& w, entity_id tile)
{
    const entity_id b = w.create_entity();
    building_component bc{};
    bc.type            = building_type::inland_logistics_hub;
    bc.tile            = tile;
    bc.ticks_remaining = 0;
    w.buildings[b]     = bc;
    return b;
}

entity_id add_unit(world& w, entity_id owner, entity_id tile, uint16_t type, int count = 100)
{
    const entity_id u = w.create_entity();
    unit_component uc{};
    uc.owner    = owner;
    uc.position = tile;
    uc.count    = count;
    uc.type     = type;
    w.units[u]  = uc;
    return u;
}

/// A raw tile order along the row, from @p from_col to @p to_col.
void order_along_row(world& w, entity_id unit, entity_id body, int from_col, int to_col)
{
    std::vector<entity_id> path;
    const int step = (to_col >= from_col) ? 1 : -1;
    for (int c = from_col;; c += step)
    {
        path.push_back(tile_at(w, body, c));
        if (c == to_col)
            break;
    }
    movement_order mo;
    mo.dest       = path.back();
    mo.path       = path;
    mo.next_index = 1;
    mo.progress   = 0.0f;
    w.units.at(unit).order = mo;
}

int col_of(const world& w, entity_id tile) { return w.tiles.at(tile).grid_x; }

/// Lay a road of @p tier along the whole row (every east side).
void road_row(world& w, entity_id body, int gw, std::uint8_t tier)
{
    for (int c = 0; c + 1 < gw; ++c)
        lay_road_edge(w, tile_at(w, body, c), road_side_between(w, tile_at(w, body, c),
                                                                tile_at(w, body, c + 1)), tier);
    invalidate_logistics_caches(w);
}

/// The roster row a modern unit of @p c fields, or -1.
uint16_t modern(land_class c) { return static_cast<uint16_t>(modern_row_for(c)); }

/// The first roster row of @p cls in a pre-modern band (an Era -1 row, declaring no land class).
int ancient_row(unit_class cls)
{
    const auto& t = unit_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
        if (t[i].cls == cls && t[i].land == k_land_class_from_cls)
            return static_cast<int>(i);
    return -1;
}

/// Days for a unit of @p type to cross @p tiles tiles of the row (ungated), or -1 if it never does.
int days_to_cross(const recipe_registry& reg, uint16_t type, int tiles,
                  terrain_landform lf = terrain_landform::plains, std::uint8_t road = 0)
{
    world w;
    const int gw = tiles + 4;
    const entity_id body = make_row_body(w, gw, lf);
    if (road > 0)
        road_row(w, body, gw, road);
    const entity_id n = add_nation(w, body, 0, gw - 1);
    const entity_id u = add_unit(w, n, tile_at(w, body, 0), type);
    order_along_row(w, u, body, 0, tiles);
    for (int day = 1; day <= 2000; ++day)
    {
        (void)run_unit_march(w, reg, /*lp_gated=*/false, day);
        if (col_of(w, w.units.at(u).position) >= tiles)
            return day;
    }
    return -1;
}

const char* land_name(land_class c)
{
    switch (c)
    {
        case land_class::infantry:     return "foot infantry";
        case land_class::motorised:    return "motorised infantry";
        case land_class::mechanised:   return "mechanised infantry";
        case land_class::armour:       return "medium armour";
        case land_class::light_armour: return "light armour";
        case land_class::artillery:    return "towed artillery";
        case land_class::air_defence:  return "air defence";
        default:                       return "none";
    }
}

/// Two hostile nations on one row; the attacker's stack at col 0, the defender's at col @p gap.
struct duel
{
    world     w;
    entity_id body = null_entity, att = null_entity, def = null_entity;
    entity_id ua = null_entity, ud = null_entity;
};

void build_duel(duel& d, int gap, int att_men, int def_men, uint16_t type)
{
    const int gw = gap + 6;
    d.body = make_row_body(d.w, gw);
    d.att  = add_nation(d.w, d.body, 0, gap - 1);
    d.def  = add_nation(d.w, d.body, gap, gw - 1);
    d.w.provinces.seed = 0x1445u;
    build_province_partition(d.w, 0x1445u);
    (void)declare_hostile(d.w, d.att, d.def);
    d.ua = add_unit(d.w, d.att, tile_at(d.w, d.body, 0), type, att_men);
    d.ud = add_unit(d.w, d.def, tile_at(d.w, d.body, gap), type, def_men);
    order_along_row(d.w, d.ua, d.body, 0, gap);
}

} // namespace

int main()
{
    std::printf("=== move by days (BL-1445; MILITARY.md § Marching, § Battles ruling 3) ===\n");

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    const military_capability_params& mil = reg.military();

    // -----------------------------------------------------------------------
    std::printf("\nD1 - each class's days per plains tile, measured over twelve tiles\n");
    {
        struct expect { land_class c; float days; };
        const expect rows[] = {
            { land_class::infantry, 3.0f },     { land_class::motorised, 1.0f },
            { land_class::mechanised, 1.0f },   { land_class::light_armour, 1.0f },
            { land_class::armour, 1.5f },       { land_class::artillery, 2.0f },
            { land_class::air_defence, 1.5f },
        };
        bool all = true;
        for (const expect& e : rows)
        {
            const uint16_t row = modern(e.c);
            const float authored = march_days_per_tile(mil, row);
            const int days = days_to_cross(reg, row, 12);
            const float measured = days > 0 ? static_cast<float>(days) / 12.0f : -1.0f;
            std::printf("      %-20s authored %.2f  measured %d days / 12 tiles = %.2f days a tile\n",
                        land_name(e.c), authored, days, measured);
            all = all && approx(authored, e.days) && approx(measured, e.days);
        }
        check(all, "D1a every modern land class crosses plains at its authored days per tile "
                   "(3 / 1 / 1 / 1 / 1.5 / 2 / 1.5)");

        struct pre { unit_class cls; const char* name; float days; };
        const pre olds[] = {
            { unit_class::infantry, "Era -1 infantry", 3.0f }, { unit_class::cavalry, "Era -1 cavalry", 1.5f },
            { unit_class::ranged, "Era -1 ranged", 3.0f },     { unit_class::siege, "Era -1 siege", 3.0f },
        };
        bool olds_ok = true;
        for (const pre& p : olds)
        {
            const int r = ancient_row(p.cls);
            if (r < 0) { olds_ok = false; continue; }
            const int days = days_to_cross(reg, static_cast<uint16_t>(r), 12);
            std::printf("      %-20s (row %d) authored %.2f  measured %d days / 12 tiles\n", p.name, r,
                        march_days_per_tile(mil, static_cast<uint16_t>(r)), days);
            olds_ok = olds_ok && days == static_cast<int>(std::lround(p.days * 12.0f));
        }
        check(olds_ok, "D1b a pre-modern row marches by its cls (infantry 3, cavalry 1.5, ranged 3, siege 3)");
        const int naval = ancient_row(unit_class::naval);
        check(naval >= 0 && march_days_per_tile(mil, static_cast<uint16_t>(naval)) <= 0.0f
                  && days_to_cross(reg, static_cast<uint16_t>(naval), 2) < 0,
              "D1c a naval row has no land speed and never marches on this table");
    }

    // -----------------------------------------------------------------------
    std::printf("\nD2 - terrain multiplies, a road divides\n");
    {
        const uint16_t inf = modern(land_class::infantry);
        const uint16_t mot = modern(land_class::motorised);
        const int plains   = days_to_cross(reg, inf, 12);
        const int mountain = days_to_cross(reg, inf, 12, terrain_landform::mountain);
        const int road     = days_to_cross(reg, inf, 12, terrain_landform::plains, 2);
        const int highway  = days_to_cross(reg, inf, 12, terrain_landform::plains, 3);
        const int mtn_road = days_to_cross(reg, inf, 12, terrain_landform::mountain, 2);
        const int mot_road = days_to_cross(reg, mot, 12, terrain_landform::plains, 2);
        std::printf("      foot infantry, 12 tiles: plains %d, mountain %d, road %d, highway %d, "
                    "mountain road %d days; motorised on a road %d days\n",
                    plains, mountain, road, highway, mtn_road, mot_road);
        check(mountain == 2 * plains, "D2a a mountain (traversal cost 2.0) doubles the days: 6 a tile");
        check(road == 18, "D2b a road (tier 2, x0.50 - road_traversal_multiplier) halves them: 1.5 a tile");
        check(highway < road && highway >= 14, "D2c a highway (x0.40) cuts them further: 1.2 a tile");
        check(mtn_road == plains, "D2d a road over a mountain prices as plains: 2.0 x 0.5 = 1.0 cost, 3 days");
        check(mot_road == 6, "D2e motorised on a road: 0.5 a tile, two tiles a day");
    }

    // -----------------------------------------------------------------------
    std::printf("\nD3 - partial progress banks from day to day\n");
    {
        world w;
        const entity_id body = make_row_body(w, 10);
        const entity_id n = add_nation(w, body, 0, 9);
        const entity_id ui = add_unit(w, n, tile_at(w, body, 0), modern(land_class::infantry));
        const entity_id ua = add_unit(w, n, tile_at(w, body, 0), modern(land_class::armour));
        order_along_row(w, ui, body, 0, 6);
        order_along_row(w, ua, body, 0, 6);
        std::vector<float> pi, pa;
        std::vector<int> ci, ca;
        for (int day = 1; day <= 3; ++day)
        {
            (void)run_unit_march(w, reg, false, day);
            pi.push_back(w.units.at(ui).order.progress); ci.push_back(col_of(w, w.units.at(ui).position));
            pa.push_back(w.units.at(ua).order.progress); ca.push_back(col_of(w, w.units.at(ua).position));
        }
        std::printf("      infantry: day1 col %d banked %.3f, day2 col %d banked %.3f, day3 col %d banked %.3f\n",
                    ci[0], pi[0], ci[1], pi[1], ci[2], pi[2]);
        std::printf("      armour:   day1 col %d banked %.3f, day2 col %d banked %.3f, day3 col %d banked %.3f\n",
                    ca[0], pa[0], ca[1], pa[1], ca[2], pa[2]);
        check(ci[0] == 0 && approx(pi[0], 1.0f / 3.0f) && ci[1] == 0 && approx(pi[1], 2.0f / 3.0f)
                  && ci[2] == 1 && approx(pi[2], 0.0f, 1e-3f),
              "D3a foot infantry banks a third of a tile a day and steps on day 3");
        check(ca[0] == 0 && approx(pa[0], 2.0f / 3.0f) && ca[1] == 1 && approx(pa[1], 1.0f / 3.0f, 1e-3f)
                  && ca[2] == 2 && approx(pa[2], 0.0f, 1e-3f),
              "D3b medium armour steps on day 2, banks the third left over, and steps again on day 3");
    }

    // -----------------------------------------------------------------------
    std::printf("\nD4 - Logistic Points: granted quarterly, spent tile by tile; a spent grant halts\n");
    {
        world w;
        const entity_id body = make_row_body(w, 60);
        const entity_id n = add_nation(w, body, 0, 59);
        add_hub(w, tile_at(w, body, 0));
        const entity_id u = add_unit(w, n, tile_at(w, body, 0), modern(land_class::infantry), 100);
        order_along_row(w, u, body, 0, 5);

        const float rate = mil.active_lp_credit_per_unit_distance;
        const lp_tick q1 = resolve_logistic_points(w, collect_march_claims(w, reg));
        apply_march_grants(w, q1);
        const float grant1 = w.units.at(u).march_grant;
        std::printf("      quarter 1: a five-tile order claims %.2f points and is granted %.2f "
                    "(pool %.0f)\n", q1.marches.empty() ? -1.0f : q1.marches.front().need, grant1,
                    q1.nations.empty() ? 0.0f : q1.nations.front().pool);
        check(approx(grant1, 5.0f), "D4a the claim is the path the quarter can walk - five plains tiles, 5 points");

        const float t0 = w.nations.at(n).treasury;
        int arrived_day = -1;
        for (int day = 1; day <= 20; ++day)
        {
            (void)run_unit_march(w, reg, /*lp_gated=*/true, day);
            if (arrived_day < 0 && w.units.at(u).order.dest == null_entity)
                arrived_day = day;
        }
        const float paid = t0 - w.nations.at(n).treasury;
        std::printf("      arrived day %d; grant left %.3f; credits paid %.3f (rate %.4f x 5 tiles x 100 men = %.3f)\n",
                    arrived_day, w.units.at(u).march_grant, paid, rate, rate * 5.0f * 100.0f);
        check(arrived_day == 15, "D4b ...and spends it day by day: five tiles in fifteen days");
        check(approx(w.units.at(u).march_grant, 0.0f, 1e-3f), "D4c the grant is spent, one point a plains tile");
        check(approx(paid, rate * 5.0f * 100.0f, 1e-2f),
              "D4d the credit price is charged per tile moved: rate x tile cost x men");

        // A new, longer order mid-quarter: no grant is left, so the unit halts.
        order_along_row(w, u, body, 5, 40);
        int moved_mid = 0;
        int refused = 0;
        for (int day = 21; day <= 90; ++day)
        {
            const int before = col_of(w, w.units.at(u).position);
            refused += run_unit_march(w, reg, true, day).refused_no_lp;
            moved_mid += col_of(w, w.units.at(u).position) - before;
        }
        std::printf("      a 35-tile order on day 21: moved %d tiles to the quarter's end (%d day-refusals)\n",
                    moved_mid, refused);
        check(moved_mid == 0 && refused == 70, "D4e a unit whose grant is spent halts until the next grant");

        const lp_tick q2 = resolve_logistic_points(w, collect_march_claims(w, reg));
        apply_march_grants(w, q2);
        const float grant2 = w.units.at(u).march_grant;
        int moved_q2 = 0;
        for (int day = 91; day <= 180; ++day)
        {
            const int before = col_of(w, w.units.at(u).position);
            (void)run_unit_march(w, reg, true, day);
            moved_q2 += col_of(w, w.units.at(u).position) - before;
        }
        std::printf("      quarter 2: granted %.2f; moved %d tiles (a quarter of foot is 30)\n", grant2, moved_q2);
        check(approx(grant2, 30.0f) && moved_q2 == 30,
              "D4f the next quarter's grant resumes it: capped at the 30 tiles ninety days of foot can walk");

        // No pool: nobody's ground and no hub.
        world z;
        const entity_id zb = make_row_body(z, 8);
        const entity_id zn = add_nation(z, zb, 0, 7);
        const entity_id zu = add_unit(z, zn, tile_at(z, zb, 0), modern(land_class::motorised));
        order_along_row(z, zu, zb, 0, 4);
        apply_march_grants(z, resolve_logistic_points(z, collect_march_claims(z, reg)));
        for (int day = 1; day <= 30; ++day) (void)run_unit_march(z, reg, true, day);
        check(col_of(z, z.units.at(zu).position) == 0, "D4g a nation that makes no points grants nothing: no march");
    }

    // -----------------------------------------------------------------------
    std::printf("\nD5 - contact opens a battle on the day of contact\n");
    {
        duel d;
        build_duel(d, 4, 300, 300, modern(land_class::infantry));
        int opened_day = -1, adjacent_day = -1;
        for (int day = 1; day <= 30 && opened_day < 0; ++day)
        {
            const military_day_result r = run_military_day(d.w, reg, day, /*lp_gated=*/false);
            if (adjacent_day < 0 && col_of(d.w, d.w.units.at(d.ua).position) == 3)
                adjacent_day = day;
            if (r.march.battles_opened > 0)
                opened_day = day;
        }
        std::printf("      foot infantry four tiles off: adjacent on day %d, battle opened on day %d\n",
                    adjacent_day, opened_day);
        check(adjacent_day == 9 && opened_day == 9,
              "D5 the day the march reaches the hostile stack is the day the battle opens (day 9: three tiles x 3)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nD6 - battle length in days (infantry on plains, one round a day)\n");
    {
        const campaign_battle_params bp{};
        std::printf("      rounds_per_day %d, max_rounds %d\n", bp.rounds_per_day, bp.max_rounds);
        double mean_parity = 0.0, mean_two = 0.0;
        for (const int ratio_x100 : { 100, 150, 200, 300 })
        {
            const int trials = 40;
            int days_total = 0, decided = 0, att_won = 0;
            for (int s = 0; s < trials; ++s)
            {
                duel d;
                build_duel(d, 1, 300 * ratio_x100 / 100, 300, modern(land_class::infantry));
                const int base = 1000 * s; // a different opening day is a different stream
                int open = -1, end = -1;
                for (int day = 1; day <= 60 && end < 0; ++day)
                {
                    const military_day_result r = run_military_day(d.w, reg, base + day, false);
                    if (open < 0 && r.march.battles_opened > 0)
                        open = day;
                    if (open >= 0 && r.battles.concluded > 0)
                    {
                        end = day;
                        for (const battle_dispatch& bd : r.battles.dispatches)
                            if (bd.end != campaign_battle_end::in_progress)
                            {
                                decided += bd.end != campaign_battle_end::stalemate ? 1 : 0;
                                att_won += bd.field_held_by == d.att ? 1 : 0;
                            }
                    }
                }
                days_total += (end >= open && open >= 0) ? (end - open) : 60;
            }
            const double mean = static_cast<double>(days_total) / trials;
            std::printf("      %.2f:1  mean %.1f days from contact to close; decisive %d/%d, attacker held %d/%d\n",
                        ratio_x100 / 100.0, mean, decided, trials, att_won, trials);
            if (ratio_x100 == 100) mean_parity = mean;
            if (ratio_x100 == 200) mean_two = mean;
        }
        check(mean_parity >= 4.0 && mean_parity <= 6.0,
              "D6a infantry against infantry at parity on plains is fought out in about five days (4-6)");
        check(mean_two < mean_parity, "D6b a 2:1 assault ends sooner");
    }

    // -----------------------------------------------------------------------
    std::printf("\nD7 - determinism: the same fixture twice\n");
    {
        duel a, b;
        build_duel(a, 6, 400, 300, modern(land_class::infantry));
        build_duel(b, 6, 400, 300, modern(land_class::infantry));
        const entity_id ma = add_unit(a.w, a.att, tile_at(a.w, a.body, 1), modern(land_class::motorised));
        const entity_id mb = add_unit(b.w, b.att, tile_at(b.w, b.body, 1), modern(land_class::motorised));
        order_along_row(a.w, ma, a.body, 1, 4);
        order_along_row(b.w, mb, b.body, 1, 4);
        for (int day = 1; day <= 60; ++day)
        {
            (void)run_military_day(a.w, reg, day, false);
            (void)run_military_day(b.w, reg, day, false);
        }
        check(a.w.state_hash(60) == b.w.state_hash(60) && to_bytes(a.w) == to_bytes(b.w),
              "D7 sixty days of marches and a battle, twice: equal state_hash and equal save bytes");
    }

    // -----------------------------------------------------------------------
    std::printf("\nD8 - a save mid-march round-trips\n");
    {
        world w;
        const entity_id body = make_row_body(w, 40);
        const entity_id n = add_nation(w, body, 0, 39);
        add_hub(w, tile_at(w, body, 0));
        const entity_id u = add_unit(w, n, tile_at(w, body, 0), modern(land_class::armour));
        order_along_row(w, u, body, 0, 30);
        apply_march_grants(w, resolve_logistic_points(w, collect_march_claims(w, reg)));
        for (int day = 1; day <= 4; ++day) (void)run_unit_march(w, reg, true, day);
        const unit_component& src = w.units.at(u);
        std::printf("      day 4: col %d, banked %.3f, grant left %.3f\n", col_of(w, src.position),
                    src.order.progress, src.march_grant);
        world back;
        const bool ok = from_bytes(to_bytes(w), back);
        const unit_component& got = back.units.at(u);
        check(ok && got.position == src.position && approx(got.order.progress, src.order.progress, 0.0f)
                  && approx(got.march_grant, src.march_grant, 0.0f) && got.order.path == src.order.path,
              "D8a the banked progress and the unspent grant survive the save exactly");
        check(ok && back.state_hash(4) == w.state_hash(4), "D8b the loaded world hashes as the original");
        for (int day = 5; day <= 40; ++day)
        {
            (void)run_unit_march(w, reg, true, day);
            (void)run_unit_march(back, reg, true, day);
        }
        check(back.state_hash(40) == w.state_hash(40) && back.units.at(u).position == w.units.at(u).position,
              "D8c ...and marches on to the same place and the same hash");
    }

    // -----------------------------------------------------------------------
    std::printf("\nD9 - day by day (the app) and caught up by the economy step (headless) agree\n");
    {
        const auto make = [&](world& w) {
            const entity_id body = make_row_body(w, 50);
            const entity_id n = add_nation(w, body, 0, 49);
            add_hub(w, tile_at(w, body, 0));
            const entity_id u = add_unit(w, n, tile_at(w, body, 0), modern(land_class::infantry));
            order_along_row(w, u, body, 0, 40);
            return u;
        };
        world a, b;
        const entity_id ua = make(a);
        (void)make(b);
        // One quarter grants, then a quarter is walked: `a` a day at a time as the app's frame
        // loop does, `b` all at once inside the next economy step, as every headless driver does.
        (void)run_economy_step(a, reg);
        (void)run_economy_step(b, reg);
        const int base = a.current_econ_tick * 90;
        for (int d = 1; d <= 90; ++d) (void)advance_quarter_to(a, reg, d, base);
        const int col_mid = col_of(a, a.units.at(ua).position);
        (void)run_economy_step(a, reg);
        (void)run_economy_step(b, reg);
        std::printf("      after the walked quarter: col %d; both stepped again\n", col_mid);
        check(col_mid == 30, "D9a the walked quarter moved foot infantry thirty tiles");
        check(a.state_hash(2) == b.state_hash(2), "D9b the day-by-day world and the caught-up world hash alike");
    }

    // -----------------------------------------------------------------------
    std::printf("\nD10 - the loader rejects a speed that is not positive and finite\n");
    {
        const auto rejects = [](const char* patch) {
            lua_state l;
            l.load("scripts/recipes.lua");
            l.load("scripts/economy.lua");
            l.state().safe_script(patch);
            recipe_registry r;
            try { r.load_from_lua(l); }
            catch (const std::exception& e) { std::printf("      rejected: %s\n", e.what()); return true; }
            return false;
        };
        check(rejects("economy.military.march_days_per_tile.infantry = 0"), "D10a zero days is rejected");
        check(rejects("economy.military.march_days_per_tile.armour = -1.5"), "D10b a negative speed is rejected");
        check(rejects("economy.military.march_days_per_tile_by_cls.cavalry = 1/0"), "D10c infinity is rejected");
        check(rejects("economy.military.march_days_per_tile.tank = 1"), "D10d an unknown class key is rejected");
    }

    std::printf("\n%d checks, %d failures\n", g_pass + g_fail, g_fail);
    std::printf(g_fail == 0 ? "ALL PASS (0 failures)\n" : "FAILURES\n");
    return g_fail == 0 ? 0 : 1;
}
