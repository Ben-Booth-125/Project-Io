// terrain_class_harness.cpp — BL-1455 (terrain class modifiers; MILITARY.md § Terrain and
// the classes, Ben 2026-10-29).
//
// Ground treats the land classes differently: tanks cannot cross mountains without a road,
// and fight worse in them. This harness measures the authored rows (scripts/economy.lua's
// `economy.military.terrain_class` and `terrain_open_ground_stealth`) through the one
// lookup, `terrain_class_effects`, and through the march, the battle and the retreat that
// read it:
//
//   T1  the lookup, for every cell of the doc's table, plus the open-ground stealth rule;
//   T2  worst-row stacking on a tile that matches two rows (a forested mountain): per
//       effect the worst row, never a product;
//   T3  medium armour refused across a roadless mountain wall (rejected_terrain), routed
//       around it when a way exists, and refused onto a walled destination;
//   T4  the same move over a mountain ROAD succeeds and arrives;
//   T5  the move rows: motorised infantry dismounts to foot pace on mountains, medium
//       armour x1.5 in forest;
//   T6  a unit standing on a blocked tile may leave it;
//   T7  the fight debuff, by each unit's own class on either side, and a road halving it;
//   T8  a retreat never steps onto a tile its class cannot enter;
//   T9  the loader rejects bad rows.
//
// Build: bash tools/verify/build_lua_harness.sh terrain_class_harness (it loads
// scripts/recipes.lua and scripts/economy.lua through a live Lua state). Run from the
// repo root.

#include "world/battle_system.hpp"
#include "world/campaign_battle.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/terrain_class.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"
#include "scripting/lua_state.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

uint16_t modern(land_class c) { return static_cast<uint16_t>(modern_row_for(c)); }

int ancient_row(unit_class cls)
{
    const auto& t = unit_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
        if (t[i].cls == cls && t[i].land == k_land_class_from_cls)
            return static_cast<int>(i);
    return -1;
}

const char* class_name(land_class c)
{
    switch (c)
    {
        case land_class::infantry:     return "infantry";
        case land_class::motorised:    return "motorised";
        case land_class::mechanised:   return "mechanised";
        case land_class::armour:       return "medium armour";
        case land_class::light_armour: return "light armour";
        case land_class::artillery:    return "artillery";
        case land_class::air_defence:  return "air defence";
        default:                       return "none";
    }
}

const char* terrain_name(class_terrain t)
{
    switch (t)
    {
        case class_terrain::mountain: return "mountain";
        case class_terrain::canyon:   return "canyon";
        case class_terrain::rift:     return "rift";
        case class_terrain::marsh:    return "marsh";
        case class_terrain::desert:   return "desert";
        case class_terrain::forest:   return "forest";
        case class_terrain::urban:    return "urban";
        case class_terrain::snow:     return "snow";
    }
    return "?";
}

/// A bare tile matching exactly ONE class row: a landform row on scrub (no cover row,
/// not open ground), a cover row on a valley (no landform row, not open ground).
tile_component tile_for(class_terrain t)
{
    tile_component tc{};
    tc.substrate     = terrain_substrate::sedimentary;
    tc.landform      = terrain_landform::valley;
    tc.cover         = terrain_cover::scrub;
    tc.cover_density = 150;
    switch (t)
    {
        case class_terrain::mountain: tc.landform = terrain_landform::mountain; break;
        case class_terrain::canyon:   tc.landform = terrain_landform::canyon;   break;
        case class_terrain::rift:     tc.landform = terrain_landform::rift;     break;
        case class_terrain::marsh:    tc.cover = terrain_cover::marsh;  break;
        case class_terrain::desert:   tc.cover = terrain_cover::dunes;  break;
        case class_terrain::forest:   tc.cover = terrain_cover::forest; break;
        case class_terrain::urban:    tc.cover = terrain_cover::urban;  break;
        case class_terrain::snow:     tc.cover = terrain_cover::snow;   break;
    }
    return tc;
}

/// One cell of the doc's table, written down INDEPENDENTLY of economy.lua.
struct cell
{
    float move = 1.0f; bool blocked = false; bool dismount = false;
    float fight = 1.0f; float wear = 1.0f; float stealth = 1.0f;
};

cell B()                   { cell c; c.blocked = true; return c; }
cell BF(float f)           { cell c; c.blocked = true; c.fight = f; return c; }
cell M(float m)            { cell c; c.move = m; return c; }
cell F(float f)            { cell c; c.fight = f; return c; }
cell MF(float m, float f)  { cell c; c.move = m; c.fight = f; return c; }
cell MW(float m, float w)  { cell c; c.move = m; c.wear = w; return c; }
cell W(float w)            { cell c; c.wear = w; return c; }
cell S(float s)            { cell c; c.stealth = s; return c; }
cell D()                   { cell c; c.dismount = true; c.move = 3.0f; return c; } // foot 3 / own 1
cell N()                   { return cell{}; }

// ---------------------------------------------------------------------------
// A grid body
// ---------------------------------------------------------------------------

struct grid
{
    world     w;
    entity_id body = null_entity;
    entity_id nation = null_entity;
    int gw = 0, gh = 0;
    entity_id at(int c, int r) { return body_tile_grid(w, body)[static_cast<std::size_t>(r * gw + c)]; }
    tile_component& tc(int c, int r) { return w.tiles.at(at(c, r)); }
};

void build_grid(grid& g, int gw, int gh)
{
    g.gw = gw; g.gh = gh;
    g.body = g.w.create_entity();
    body_component bc{};
    bc.grid_width  = gw;
    bc.grid_height = gh;
    g.w.bodies[g.body] = bc;
    for (int r = 0; r < gh; ++r)
        for (int c = 0; c < gw; ++c)
        {
            const entity_id t = g.w.create_entity();
            tile_component tc{};
            tc.body          = g.body;
            tc.grid_x        = c;
            tc.grid_y        = r;
            tc.substrate     = terrain_substrate::sedimentary;
            tc.cover         = terrain_cover::grass;
            tc.cover_density = 150;
            tc.landform      = terrain_landform::plains;
            g.w.tiles[t]     = tc;
        }
}

entity_id add_nation(grid& g, int from_col, int to_col)
{
    const entity_id n = g.w.create_entity();
    nation_component nc{};
    nc.name     = "Fixture";
    nc.treasury = 100000.0f;
    for (int r = 0; r < g.gh; ++r)
        for (int c = from_col; c <= to_col; ++c)
        {
            const entity_id t = g.at(c, r);
            nc.tiles.push_back(t);
            g.w.tile_to_nation[t] = n;
        }
    g.w.nations[n] = nc;
    return n;
}

void partition(grid& g)
{
    g.w.provinces.seed = 0x1455u;
    build_province_partition(g.w, 0x1455u);
    invalidate_logistics_caches(g.w);
}

entity_id add_unit(world& w, entity_id owner, entity_id tile, uint16_t type, int count = 300)
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

void road_between(world& w, entity_id a, entity_id b, std::uint8_t tier = 2)
{
    lay_road_edge(w, a, road_side_between(w, a, b), tier);
    invalidate_logistics_caches(w);
}

corp_command_result march(world& w, const recipe_registry& reg, entity_id nation, entity_id unit,
                          entity_id tile)
{
    corp_command cmd;
    cmd.corp    = nation;
    cmd.verb    = corp_verb::march_unit;
    cmd.subject = unit;
    cmd.tile    = tile;
    return apply_corp_command(w, reg, cmd);
}

const char* result_name(corp_command_result r)
{
    switch (r)
    {
        case corp_command_result::applied:          return "applied";
        case corp_command_result::rejected_invalid: return "rejected_invalid";
        case corp_command_result::rejected_state:   return "rejected_state";
        case corp_command_result::rejected_terrain: return "rejected_terrain";
        default:                                    return "other";
    }
}

/// A mountain wall down column 4 of a 9 x 5 grid, every tile of it roadless; @p gap_row
/// >= 0 leaves that row's tile plains (the way round). Column 0 is a second, gapless
/// wall: the grid wraps east-west (a cylinder), and without it the way round would be
/// the wrap.
void mountain_wall(grid& g, int gap_row)
{
    for (int r = 0; r < g.gh; ++r)
    {
        g.tc(0, r).landform = terrain_landform::mountain;
        if (r != gap_row)
            g.tc(4, r).landform = terrain_landform::mountain;
    }
}

/// Days for a unit of @p type to cross a 12-tile row of @p lf / @p cov ground on a raw
/// path (no seam), or -1.
int days_to_cross(const recipe_registry& reg, uint16_t type, terrain_landform lf, terrain_cover cov)
{
    grid g;
    build_grid(g, 16, 1);
    for (int c = 0; c < 16; ++c)
    {
        g.tc(c, 0).landform = lf;
        g.tc(c, 0).cover    = cov;
    }
    const entity_id n = add_nation(g, 0, 15);
    const entity_id u = add_unit(g.w, n, g.at(0, 0), type);
    movement_order mo;
    for (int c = 0; c <= 12; ++c)
        mo.path.push_back(g.at(c, 0));
    mo.dest = mo.path.back();
    mo.next_index = 1;
    g.w.units.at(u).order = mo;
    for (int day = 1; day <= 4000; ++day)
    {
        (void)run_unit_march(g.w, reg, day);
        if (g.w.tiles.at(g.w.units.at(u).position).grid_x >= 12)
            return day;
    }
    return -1;
}

/// A one-row duel: the attacker's stack at col 0, the defender's at col 1, hostile. The
/// defended tile's cover is @p cov, with a road on it when @p road. Runs @p days of the
/// daily pass and reports each side's men lost.
struct duel_result { int att_lost = 0; int def_lost = 0; bool opened = false; };

duel_result duel(const recipe_registry& reg, uint16_t att_type, uint16_t def_type,
                 terrain_cover cov, bool road, int days)
{
    grid g;
    build_grid(g, 6, 1);
    g.tc(1, 0).cover = cov;
    const entity_id a = add_nation(g, 0, 0);
    const entity_id d = add_nation(g, 1, 5);
    partition(g);
    if (road)
        road_between(g.w, g.at(1, 0), g.at(2, 0));
    (void)declare_hostile(g.w, a, d);
    const entity_id ua = add_unit(g.w, a, g.at(0, 0), att_type, 600);
    const entity_id ud = add_unit(g.w, d, g.at(1, 0), def_type, 600);
    movement_order mo;
    mo.path = { g.at(0, 0), g.at(1, 0) };
    mo.dest = g.at(1, 0);
    mo.next_index = 1;
    mo.progress = 10.0f; // in contact today
    g.w.units.at(ua).order = mo;
    duel_result out;
    for (int day = 1; day <= days; ++day)
    {
        const military_day_result r = run_military_day(g.w, reg, 7000 + day);
        out.opened = out.opened || r.march.battles_opened > 0;
    }
    const auto men = [&](entity_id u) {
        const auto it = g.w.units.find(u);
        return it == g.w.units.end() ? 0 : it->second.count;
    };
    out.att_lost = 600 - men(ua);
    out.def_lost = 600 - men(ud);
    return out;
}

recipe_registry load_registry(const char* patch = nullptr)
{
    lua_state l;
    l.load("scripts/recipes.lua");
    l.load("scripts/economy.lua");
    if (patch != nullptr)
        l.state().safe_script(patch);
    recipe_registry r;
    r.load_from_lua(l);
    return r;
}

} // namespace

int main()
{
    std::printf("=== terrain and the classes (BL-1455; MILITARY.md § Terrain and the classes) ===\n");

    const recipe_registry reg = load_registry();
    // The control: every class row neutral (the shared terrain table only).
    const recipe_registry reg0 =
        load_registry("economy.military.terrain_class = nil; economy.military.terrain_open_ground_stealth = nil");
    const military_capability_params& mil = reg.military();

    // -----------------------------------------------------------------------
    std::printf("\nT1 - the lookup, every cell of the doc's table\n");
    {
        const land_class classes[] = { land_class::infantry, land_class::motorised, land_class::mechanised,
                                       land_class::light_armour, land_class::armour, land_class::artillery,
                                       land_class::air_defence };
        // Columns: mountain, canyon, rift, marsh, desert (dunes), forest, urban, snow.
        const cell table[7][8] = {
            /* infantry     */ { N(),      N(),      N(),      N(),      N(),               S(1.5f),         S(1.5f),  N() },
            /* motorised    */ { D(),      D(),      D(),      B(),      M(1.25f),          M(1.25f),        N(),      M(1.25f) },
            /* mechanised   */ { D(),      D(),      D(),      B(),      N(),               M(1.25f),        F(0.85f), N() },
            /* light armour */ { BF(0.6f), B(),      B(),      B(),      MW(1.25f, 1.5f),   F(0.75f),        F(0.7f),  M(1.5f) },
            /* armour       */ { BF(0.5f), BF(0.6f), BF(0.6f), BF(0.5f), BF(0.7f),          MF(1.5f, 0.7f),  F(0.6f),  M(1.25f) },
            /* artillery    */ { B(),      B(),      B(),      B(),      M(1.25f),          M(1.5f),         F(0.8f),  M(1.5f) },
            /* air defence  */ { B(),      B(),      B(),      B(),      W(1.5f),           N(),             N(),      M(1.25f) },
        };
        int bad = 0, cells = 0;
        for (int ci = 0; ci < 7; ++ci)
            for (int ti = 0; ti < class_terrain_count; ++ti)
            {
                const cell want = table[ci][ti];
                const class_terrain t = static_cast<class_terrain>(ti);
                const tile_component tc = tile_for(t);
                const terrain_class_effect e = terrain_class_effects(mil, classes[ci], tc);
                const bool ok = approx(e.move, want.move) && e.blocked_unless_roaded == want.blocked
                             && e.walled == want.blocked && e.dismounted == want.dismount
                             && approx(e.fight, want.fight) && approx(e.wear, want.wear)
                             && approx(e.stealth, want.stealth);
                ++cells;
                if (!ok)
                {
                    ++bad;
                    std::printf("      MISMATCH %s / %s: move %.2f blocked %d dismount %d fight %.2f wear %.2f stealth %.2f"
                                "  (want %.2f %d %d %.2f %.2f %.2f)\n",
                                class_name(classes[ci]), terrain_name(t), e.move, e.blocked_unless_roaded,
                                e.dismounted, e.fight, e.wear, e.stealth, want.move, want.blocked,
                                want.dismount, want.fight, want.wear, want.stealth);
                }
            }
        std::printf("      %d cells, %d mismatches\n", cells, bad);
        check(bad == 0, "T1a every (class, terrain) cell returns the doc's move, block, dismount, fight, wear "
                        "and stealth (canyon and rift alike)");

        // The open-ground rule: plains or highland under none, grass, salt, snow or dunes.
        bool open_ok = true;
        const terrain_cover open_covers[] = { terrain_cover::none, terrain_cover::grass, terrain_cover::salt,
                                              terrain_cover::snow, terrain_cover::dunes };
        for (const terrain_landform lf : { terrain_landform::plains, terrain_landform::highland })
            for (const terrain_cover cv : open_covers)
                for (const land_class c : classes)
                {
                    tile_component tc = tile_for(class_terrain::forest);
                    tc.landform = lf;
                    tc.cover    = cv;
                    tc.cover_density = cv == terrain_cover::none ? 0 : 150;
                    open_ok = open_ok && approx(terrain_class_effects(mil, c, tc).stealth, 0.75f);
                }
        check(open_ok, "T1b open ground (plains or highland; none, grass, salt, snow, dunes) gives every class "
                       "stealth x0.75");
        tile_component plains_forest = tile_for(class_terrain::forest);
        plains_forest.landform = terrain_landform::plains;
        tile_component valley_grass = tile_for(class_terrain::forest);
        valley_grass.cover = terrain_cover::grass;
        tile_component plains_scrub = tile_for(class_terrain::forest);
        plains_scrub.landform = terrain_landform::plains;
        plains_scrub.cover    = terrain_cover::scrub;
        check(approx(terrain_class_effects(mil, land_class::infantry, plains_forest).stealth, 1.5f)
                  && approx(terrain_class_effects(mil, land_class::armour, plains_forest).stealth, 1.0f)
                  && approx(terrain_class_effects(mil, land_class::armour, valley_grass).stealth, 1.0f)
                  && approx(terrain_class_effects(mil, land_class::armour, plains_scrub).stealth, 1.0f),
              "T1c concealing cover is not open ground (plains forest: infantry x1.5, armour x1.0), nor is a "
              "valley or a plains scrub");

        // Plains snow: the snow row AND open ground, each on its own effect.
        tile_component plains_snow = tile_for(class_terrain::snow);
        plains_snow.landform = terrain_landform::plains;
        const terrain_class_effect ls = terrain_class_effects(mil, land_class::light_armour, plains_snow);
        check(approx(ls.move, 1.5f) && approx(ls.stealth, 0.75f),
              "T1d light armour on plains snow: snow's move x1.5 and open ground's stealth x0.75");

        // A pre-modern row and a non-fighter get nothing.
        const int era = ancient_row(unit_class::cavalry);
        tile_component mtn = tile_for(class_terrain::mountain);
        const terrain_class_effect pe = era >= 0
            ? terrain_class_effects_for_type(mil, static_cast<uint16_t>(era), mtn) : terrain_class_effect{};
        const terrain_class_effect pg = era >= 0
            ? terrain_class_effects_for_type(mil, static_cast<uint16_t>(era), valley_grass) : terrain_class_effect{};
        tile_component plains_grass = valley_grass;
        plains_grass.landform = terrain_landform::plains;
        const terrain_class_effect po = era >= 0
            ? terrain_class_effects_for_type(mil, static_cast<uint16_t>(era), plains_grass) : terrain_class_effect{};
        check(era >= 0 && !pe.walled && approx(pe.move, 1.0f) && approx(pg.stealth, 1.0f)
                  && approx(po.stealth, 1.0f) && approx(pe.fight, 1.0f),
              "T1e a pre-modern row (no land class) reads no class row and no open-ground rule");
        check(terrain_class_effects_for_type(mil, modern(land_class::armour), mtn).walled
                  && terrain_class_effects_for_type(mil, 0xFFFF, mtn).walled == false,
              "T1f the roster-row lookup reads the row's class; an unknown row gets nothing");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT2 - worst-row stacking: a tile that matches two rows\n");
    {
        tile_component fm = tile_for(class_terrain::mountain);
        fm.cover = terrain_cover::forest;
        const terrain_class_effect ar = terrain_class_effects(mil, land_class::armour, fm);
        const terrain_class_effect la = terrain_class_effects(mil, land_class::light_armour, fm);
        const terrain_class_effect mo = terrain_class_effects(mil, land_class::motorised, fm);
        const terrain_class_effect in = terrain_class_effects(mil, land_class::infantry, fm);
        const terrain_class_effect at = terrain_class_effects(mil, land_class::artillery, fm);
        std::printf("      forested mountain: medium armour move %.2f blocked %d fight %.2f; light armour fight %.2f; "
                    "motorised move %.2f dismounted %d; infantry stealth %.2f; artillery move %.2f blocked %d\n",
                    ar.move, ar.blocked_unless_roaded, ar.fight, la.fight, mo.move, mo.dismounted, in.stealth,
                    at.move, at.blocked_unless_roaded);
        check(ar.blocked_unless_roaded && approx(ar.move, 1.5f) && approx(ar.fight, 0.5f),
              "T2a medium armour: blocked (mountain), forest's move x1.5, mountain's fight -50% - the deeper debuff, "
              "not 0.5 x 0.7 = 0.35");
        check(approx(la.fight, 0.6f) && la.blocked_unless_roaded,
              "T2b light armour: mountain's -40% over forest's -25%, not their product (0.45)");
        check(mo.dismounted && approx(mo.move, 3.0f),
              "T2c motorised: the dismount (foot pace, x3) beats forest's x1.25, never x3.75");
        check(approx(in.stealth, 1.5f) && approx(at.move, 1.5f) && at.blocked_unless_roaded,
              "T2d infantry keeps forest's stealth x1.5; artillery blocked with forest's move x1.5");

        tile_component dc = tile_for(class_terrain::canyon);
        dc.cover = terrain_cover::dunes;
        const terrain_class_effect lw = terrain_class_effects(mil, land_class::light_armour, dc);
        const terrain_class_effect aw = terrain_class_effects(mil, land_class::armour, dc);
        check(lw.blocked_unless_roaded && approx(lw.move, 1.25f) && approx(lw.wear, 1.5f)
                  && approx(aw.fight, 0.6f),
              "T2e a dune canyon: light armour blocked, move x1.25 and wear x1.5; medium armour fight -40% over -30%");

        // A road lifts the wall, never the row.
        tile_component fmr = fm;
        fmr.road_edges = road_edges_on_every_side(2);
        const terrain_class_effect arr = terrain_class_effects(mil, land_class::armour, fmr);
        check(arr.blocked_unless_roaded && !arr.walled && approx(arr.battle_fight, 0.75f) && ar.walled
                  && approx(ar.battle_fight, 0.5f),
              "T2f with a road the tile still names the block but is no wall, and the debuff halves (0.5 -> 0.75)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT3 - medium armour and a roadless mountain wall\n");
    {
        const uint16_t tank = modern(land_class::armour);
        {
            grid g;
            build_grid(g, 9, 5);
            mountain_wall(g, -1);
            const entity_id n = add_nation(g, 0, 8);
            g.nation = n;
            partition(g);
            const entity_id u   = add_unit(g.w, n, g.at(1, 2), tank);
            const entity_id inf = add_unit(g.w, n, g.at(1, 3), modern(land_class::infantry));
            const corp_command_result r  = march(g.w, reg, n, u, g.at(7, 2));
            const corp_command_result ri = march(g.w, reg, n, inf, g.at(7, 3));
            const corp_command_result r0 = march(g.w, reg0, n, u, g.at(7, 2));
            std::printf("      a whole wall: medium armour %s; infantry %s; medium armour without class rows %s\n",
                        result_name(r), result_name(ri), result_name(r0));
            check(r == corp_command_result::rejected_terrain && r0 == corp_command_result::applied,
                  "T3a medium armour across a roadless mountain wall is refused rejected_terrain (the control, "
                  "with neutral rows, is applied)");
            check(ri == corp_command_result::applied, "T3b infantry crosses the same wall");
            grid g2;
            build_grid(g2, 9, 5);
            mountain_wall(g2, -1);
            const entity_id n2 = add_nation(g2, 0, 8);
            partition(g2);
            const entity_id u2 = add_unit(g2.w, n2, g2.at(1, 2), tank);
            const corp_command_result rd = march(g2.w, reg, n2, u2, g2.at(4, 2));
            check(rd == corp_command_result::rejected_terrain && g2.w.units.at(u2).order.dest == null_entity,
                  "T3c a march ONTO a roadless mountain is refused rejected_terrain and mutates nothing");
        }
        {
            grid g;
            build_grid(g, 9, 5);
            mountain_wall(g, 0); // the way round: row 0
            const entity_id n = add_nation(g, 0, 8);
            partition(g);
            const entity_id u = add_unit(g.w, n, g.at(1, 2), tank);
            const corp_command_result r = march(g.w, reg, n, u, g.at(7, 2));
            const std::vector<entity_id> path = g.w.units.at(u).order.path;
            bool clean = !path.empty();
            bool via_gap = false;
            for (const entity_id t : path)
            {
                clean   = clean && g.w.tiles.at(t).landform != terrain_landform::mountain;
                via_gap = via_gap || t == g.at(4, 0);
            }
            std::printf("      a wall with a gap at row 0: %s, path %zu tiles, through the gap %d\n",
                        result_name(r), path.size(), via_gap);
            check(r == corp_command_result::applied && clean && via_gap,
                  "T3d with a way round, medium armour is routed around the wall, through the gap");
            int arrived = -1;
            for (int day = 1; day <= 200 && arrived < 0; ++day)
            {
                (void)run_unit_march(g.w, reg, day);
                if (g.w.units.at(u).position == g.at(7, 2))
                    arrived = day;
                if (g.w.tiles.at(g.w.units.at(u).position).landform == terrain_landform::mountain)
                    clean = false;
            }
            check(arrived > 0 && clean, "T3e ...and marches it, never standing on the mountain");
        }
    }

    // -----------------------------------------------------------------------
    std::printf("\nT4 - the same move over a mountain road\n");
    {
        grid g;
        build_grid(g, 9, 5);
        mountain_wall(g, -1);
        road_between(g.w, g.at(3, 2), g.at(4, 2));
        road_between(g.w, g.at(4, 2), g.at(5, 2));
        const entity_id n = add_nation(g, 0, 8);
        partition(g);
        const entity_id u = add_unit(g.w, n, g.at(1, 2), modern(land_class::armour));
        const corp_command_result r = march(g.w, reg, n, u, g.at(7, 2));
        const std::vector<entity_id> path = g.w.units.at(u).order.path;
        const bool via_road = std::find(path.begin(), path.end(), g.at(4, 2)) != path.end();
        int arrived = -1;
        for (int day = 1; day <= 200 && arrived < 0; ++day)
        {
            (void)run_unit_march(g.w, reg, day);
            if (g.w.units.at(u).position == g.at(7, 2))
                arrived = day;
        }
        std::printf("      a whole wall with a road through (4,2): %s, via the road %d, arrived day %d\n",
                    result_name(r), via_road, arrived);
        check(r == corp_command_result::applied && via_road && arrived > 0,
              "T4 medium armour crosses the mountain on its road, and arrives");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT5 - the move rows: dismount and forest\n");
    {
        const uint16_t mot  = modern(land_class::motorised);
        const uint16_t inf  = modern(land_class::infantry);
        const uint16_t tank = modern(land_class::armour);
        const int mot_plains = days_to_cross(reg, mot, terrain_landform::plains, terrain_cover::grass);
        const int mot_mtn    = days_to_cross(reg, mot, terrain_landform::mountain, terrain_cover::scrub);
        const int mot_mtn0   = days_to_cross(reg0, mot, terrain_landform::mountain, terrain_cover::scrub);
        const int inf_mtn    = days_to_cross(reg, inf, terrain_landform::mountain, terrain_cover::scrub);
        std::printf("      12 tiles: motorised plains %d, mountain %d (neutral rows %d), foot infantry mountain %d days\n",
                    mot_plains, mot_mtn, mot_mtn0, inf_mtn);
        check(mot_plains == 12 && mot_mtn == inf_mtn && mot_mtn == 72 && mot_mtn0 == 24,
              "T5a motorised infantry dismounts on a mountain and marches at foot pace: 72 days, as foot "
              "infantry (24 mounted)");
        const int tank_grass  = days_to_cross(reg, tank, terrain_landform::valley, terrain_cover::grass);
        const int tank_forest = days_to_cross(reg, tank, terrain_landform::valley, terrain_cover::forest);
        const int tank_forest0 = days_to_cross(reg0, tank, terrain_landform::valley, terrain_cover::forest);
        std::printf("      12 valley tiles: medium armour grass %d, forest %d (neutral rows %d) days\n",
                    tank_grass, tank_forest, tank_forest0);
        check(tank_forest0 > 0 && std::abs(tank_forest - static_cast<int>(std::lround(tank_forest0 * 1.5))) <= 1,
              "T5b medium armour takes x1.5 the days in forest");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT6 - a unit standing on a blocked tile may leave it\n");
    {
        grid g;
        build_grid(g, 9, 5);
        mountain_wall(g, -1);
        const entity_id n = add_nation(g, 0, 8);
        partition(g);
        const entity_id u = add_unit(g.w, n, g.at(4, 2), modern(land_class::armour)); // on the wall
        const corp_command_result r = march(g.w, reg, n, u, g.at(6, 2));
        int arrived = -1;
        for (int day = 1; day <= 200 && arrived < 0; ++day)
        {
            (void)run_unit_march(g.w, reg, day);
            if (g.w.units.at(u).position == g.at(6, 2))
                arrived = day;
        }
        std::printf("      medium armour on a roadless mountain, ordered off it: %s, arrived day %d\n",
                    result_name(r), arrived);
        check(r == corp_command_result::applied && arrived > 0,
              "T6 a block refuses entry, never exit: the unit marches off the wall");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT7 - the fight debuff on the battle ground, and a road halving it\n");
    {
        tile_component urb = tile_for(class_terrain::urban);
        const int p_arm  = terrain_fight_permille(terrain_class_effects(mil, land_class::armour, urb));
        const int p_mech = terrain_fight_permille(terrain_class_effects(mil, land_class::mechanised, urb));
        const int p_inf  = terrain_fight_permille(terrain_class_effects(mil, land_class::infantry, urb));
        urb.road_edges = road_edges_on_every_side(1);
        const int r_arm  = terrain_fight_permille(terrain_class_effects(mil, land_class::armour, urb));
        const int r_mech = terrain_fight_permille(terrain_class_effects(mil, land_class::mechanised, urb));
        std::printf("      urban per-mille: medium armour %d (road %d), mechanised %d (road %d), infantry %d\n",
                    p_arm, r_arm, p_mech, r_mech, p_inf);
        check(p_arm == 600 && r_arm == 800 && p_mech == 850 && r_mech == 925 && p_inf == 1000,
              "T7a the fight scale per-mille: armour -40% in a city, -20% with a road; mechanised -15% / -7.5%");

        const uint16_t tank = modern(land_class::armour);
        const uint16_t inf  = modern(land_class::infantry);
        const int days = 3;
        // Armour attacking infantry in a city: the ATTACKER's own class reads the ground.
        const duel_result a_full = duel(reg,  tank, inf, terrain_cover::urban, false, days);
        const duel_result a_road = duel(reg,  tank, inf, terrain_cover::urban, true,  days);
        const duel_result a_none = duel(reg0, tank, inf, terrain_cover::urban, false, days);
        std::printf("      armour attacks infantry in a city, %d days: defender lost %d (rows) / %d (rows, road) / "
                    "%d (neutral); attacker lost %d / %d / %d\n",
                    days, a_full.def_lost, a_road.def_lost, a_none.def_lost, a_full.att_lost, a_road.att_lost,
                    a_none.att_lost);
        check(a_full.opened && a_full.def_lost < a_road.def_lost && a_road.def_lost < a_none.def_lost,
              "T7b the attacking armour's hits fall in a city, and a road on the defended tile halves the fall");
        {
            // THE ROUND reads the scale on hits AND on defence / breakthrough. A defence only
            // shows once hits exceed it (within it, a fixed share lands), so this is measured
            // on the resolver directly, one round, with hits past the absorb.
            const auto one_round = [](int att_terrain, int def_terrain) {
                std::vector<land_combatant> atk(1), def(1);
                atk[0].unit = 1; atk[0].men = 1000;
                atk[0].stats.soft = 1500; atk[0].stats.hard = 1500; atk[0].stats.breakthrough = 1000;
                atk[0].terrain = att_terrain;
                def[0].unit = 2; def[0].men = 1000;
                def[0].stats.soft = 200; def[0].stats.hard = 200; def[0].stats.defence = 1000;
                def[0].terrain = def_terrain;
                campaign_battle_identity id;
                id.attacker = 10; id.defender = 11; id.tile = 12; id.world_seed = 0x1455u;
                campaign_battle_state st = begin_campaign_battle(id, atk, def, terrain_substrate::sedimentary,
                                                                 terrain_cover::grass, 150, terrain_landform::plains);
                (void)step_campaign_battle(st, atk, def);
                return 1000 - def[0].men;
            };
            const int base = one_round(1000, 1000);
            const int def_half = one_round(1000, 500);
            const int att_half = one_round(500, 1000);
            std::printf("      one round, hits past the absorb: defender lost %d at full scale, %d with its own "
                        "defence at 500, %d with the attacker's hits at 500\n", base, def_half, att_half);
            check(def_half > base && att_half < base,
                  "T7c the round scales a unit's defence (it loses more) as well as its hits (it deals fewer)");
        }
        // Infantry attacking armour in a city: the DEFENDER's own class reads the ground.
        const duel_result d_full = duel(reg,  inf, tank, terrain_cover::urban, false, days);
        const duel_result d_none = duel(reg0, inf, tank, terrain_cover::urban, false, days);
        std::printf("      infantry attacks armour in a city: attacker lost %d (rows) / %d (neutral); defender "
                    "lost %d / %d\n", d_full.att_lost, d_none.att_lost, d_full.def_lost, d_none.def_lost);
        check(d_full.opened && d_full.att_lost < d_none.att_lost,
              "T7d the defending armour's hits fall too: the scale reads each unit's own class, both sides");
        // Plains, no row: identical with and without the class rows.
        const duel_result p_full = duel(reg,  inf, inf, terrain_cover::scrub, false, days);
        const duel_result p_none = duel(reg0, inf, inf, terrain_cover::scrub, false, days);
        check(p_full.att_lost == p_none.att_lost && p_full.def_lost == p_none.def_lost && p_full.opened,
              "T7e ground no row names fights exactly as before");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT8 - a retreat never steps onto a tile its class cannot enter\n");
    {
        const auto run = [](const recipe_registry& r, entity_id* out_tile, entity_id* wall_tile) {
            grid g;
            build_grid(g, 6, 1);
            g.tc(2, 0).landform = terrain_landform::mountain; // the only way back, roadless
            const entity_id a = add_nation(g, 0, 0);
            const entity_id d = add_nation(g, 1, 5);
            partition(g);
            (void)declare_hostile(g.w, a, d);
            const entity_id ua = add_unit(g.w, a, g.at(0, 0), modern(land_class::infantry), 600);
            const entity_id ud = add_unit(g.w, d, g.at(1, 0), modern(land_class::armour), 600);
            g.w.units.at(ud).readiness_permille = 1; // spent in the first round: it retreats
            movement_order mo;
            mo.path = { g.at(0, 0), g.at(1, 0) };
            mo.dest = g.at(1, 0);
            mo.next_index = 1;
            mo.progress = 10.0f;
            g.w.units.at(ua).order = mo;
            bool ever_on_wall = false;
            for (int day = 1; day <= 6; ++day)
            {
                (void)run_military_day(g.w, r, 9000 + day);
                const auto it = g.w.units.find(ud);
                if (it != g.w.units.end() && it->second.position == g.at(2, 0))
                    ever_on_wall = true;
            }
            *wall_tile = g.at(2, 0);
            const auto it = g.w.units.find(ud);
            *out_tile = it == g.w.units.end() ? null_entity : it->second.position;
            return ever_on_wall;
        };
        entity_id t_rows = null_entity, t_none = null_entity, wall_a = null_entity, wall_b = null_entity;
        const bool on_rows = run(reg, &t_rows, &wall_a);
        const bool on_none = run(reg0, &t_none, &wall_b);
        std::printf("      spent armour whose only way back is a roadless mountain: with the class rows on the "
                    "wall %d; with neutral rows %d\n", on_rows, on_none);
        check(!on_rows && on_none,
              "T8 the retreat refuses the mountain its class cannot enter (the control, with neutral rows, takes it)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT9 - the loader rejects bad rows\n");
    {
        const auto rejects = [](const char* patch) {
            try { (void)load_registry(patch); }
            catch (const std::exception& e) { std::printf("      rejected: %s\n", e.what()); return true; }
            return false;
        };
        check(rejects("economy.military.terrain_class.armour.mountain.fight = 0/0"), "T9a a NaN fight is rejected");
        check(rejects("economy.military.terrain_class.armour.mountain.fight = 1.5"),
              "T9b a fight scale above 1 is rejected");
        check(rejects("economy.military.terrain_class.armour.forest.move = 0.5"),
              "T9c a move multiplier below 1 is rejected");
        check(rejects("economy.military.terrain_class.light_armour.desert.wear = 1/0"),
              "T9d an infinite wear is rejected");
        check(rejects("economy.military.terrain_class.infantry.forest.stealth = -1"),
              "T9e a negative stealth is rejected");
        check(rejects("economy.military.terrain_class.armour.mountain.blocked_unless_roaded = 1"),
              "T9f a non-boolean block is rejected");
        check(rejects("economy.military.terrain_class.tank = { mountain = { fight = 0.5 } }"),
              "T9g an unknown class key is rejected");
        check(rejects("economy.military.terrain_class.armour.swamp = { fight = 0.5 }"),
              "T9h an unknown terrain key is rejected");
        check(rejects("economy.military.terrain_class.armour.mountain.speed = 2"),
              "T9i an unknown effect key is rejected");
        check(rejects("economy.military.terrain_open_ground_stealth = 0"),
              "T9j a zero open-ground stealth is rejected");
        check(!rejects(nullptr), "T9k the shipped rows load");
    }

    std::printf("\n%d checks, %d failures\n", g_pass + g_fail, g_fail);
    std::printf(g_fail == 0 ? "ALL PASS (0 failures)\n" : "FAILURES\n");
    return g_fail == 0 ? 0 : 1;
}
