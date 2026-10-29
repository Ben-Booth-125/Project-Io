// ---------------------------------------------------------------------------
// tile_contact_harness — BL-1443 (tile contact orders; MILITARY.md § Marching,
// § Battles). No SDL/Lua.
// ---------------------------------------------------------------------------
// Requirement group `tile-contact-orders`, rows R1-R4 (R5 is the UI lane's).
// Ben, 2026-10-27: "you could simply put units along a province boundary and
// attacks would be impossible. Instead, attacks should take place when an
// attacking unit attempts to move into a defending unit's current tile."
//
//   T1  A MARCH ENDS ON ITS TILE — not on entering the tile's province.     (R1)
//   T2  A PATH AVOIDS FOREIGN STACKS, save its own destination.             (R1)
//   T3  A NON-HOSTILE STACK ON THE NEXT STEP BLOCKS: the march re-paths round
//       it; with no path the order clears.                                  (R1)
//   T4  TWO NATIONS NEVER SHARE A TILE — at generation and after N ticks of a
//       real world at war (rivals on fronts, pushing).                      (R1)
//   T5  HOSTILE CONTACT MID-PATH STOPS THE MARCH and opens a battle over the
//       DEFENDER'S tile, not the march's destination.                       (R2)
//   T6  ADJACENCY ALONE OPENS NOTHING.                                      (R2)
//   T7  ADJACENT STACKS JOIN EACH SIDE — each side's own nation only, and only
//       out of other battles.                                               (R2)
//   T8  THE GROUND IS THE DEFENDED TILE.                                    (R2)
//   T9  ON A WIN THE ATTACKING STACK ADVANCES; JOINED STACKS STAY; never onto a
//       tile another nation still holds.                                    (R3)
//   T10 A UNIT LINED ALONG A PROVINCE BOUNDARY CAN BE ATTACKED.             (R2)
//   T11 RETREAT NEVER LANDS ON A FOREIGN TILE.                              (R3)
//   T12 EVERY VERB REFUSAL IS HASH-IDENTICAL.                               (R1/R2)
//   T13 THE SAVE ROUND-TRIPS A TILE BATTLE, AND A v62 STREAM READS.         (R4)
//   T14 A FINISHED MUSTER WAITS WHILE A FOREIGN STACK STANDS ON ITS BASE.   (R1)
//   T15 A REGISTERED ATTACK KEEPS ITS ORDER UNLESS A BATTLE OPENS WITH IT:
//       the defender walks off in the same pass, or the attacker is pulled
//       into an earlier battle first.                                       (R2)
//   T16 A SPENT UNIT'S MARCH ONTO A HOSTILE STACK IS REFUSED (march_check). (R2)
//   T17 A SAVED BATTLE OVER A TILE THE STREAM DOES NOT HOLD IS REFUSED.     (R4)
//   T18 A PRE-v63 STREAM WITH TWO NATIONS ON ONE TILE LOADS WITH ONE: the
//       lowest-id unit's nation keeps it, the rest step off or disband.     (R4)
//
// Run: node tools/verify/build_harness.js tile_contact_harness --run
// ---------------------------------------------------------------------------

#include "harness_params.hpp"
#include "world/battle_system.hpp"
#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/fronts.hpp"
#include "world/logistics.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/state_industry.hpp"
#include "world/survey_system.hpp"
#include "world/terrain_combat.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
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

recipe_registry make_registry()
{
    recipe_registry reg;
    military_capability_params mp = reg.military();
    // BL-1445 (move by days): a class is authored as DAYS PER PLAINS TILE and the march
    // steps once a DAY. This fixture keeps the per-call speeds its rows were written
    // against (infantry 1.0 of traversal cost a call, cavalry 1.5, ranged 1.0, siege
    // 0.5), so one run_unit_march call - one day - moves a unit as one call always did:
    // days = 1 / those points, and a modern row by the cls it names. These rows are
    // about contact, battles and fronts, not speed; the authored speeds are measured in
    // move_by_days_harness.
    const float per_call[unit_class_count] = { 1.0f, 1.5f, 1.0f, 0.5f, 0.0f }; // inf, cav, ranged, siege, naval
    for (int k = 0; k < unit_class_count; ++k)
        mp.march_days_per_cls[static_cast<std::size_t>(k)] = per_call[k] > 0.0f ? 1.0f / per_call[k] : 0.0f;
    for (int c = 0; c < land_class_count; ++c)
        if (const int row = modern_row_for(static_cast<land_class>(c)); row >= 0)
            mp.march_days_per_land_class[static_cast<std::size_t>(c)] =
                mp.march_days_per_cls[static_cast<std::size_t>(unit_roster_table()[static_cast<std::size_t>(row)].cls)];
    reg.set_military(mp);
    return reg;
}

constexpr uint16_t ROW_INFANTRY = 0;
constexpr int GW = 24, GH = 8; // wide enough that no route used here wraps the cylinder

/// A GW x GH plains body, partitioned, with three nations; nothing hostile.
struct fixture
{
    world     w;
    entity_id body = null_entity;
    entity_id A = null_entity, B = null_entity, C = null_entity;
    std::vector<entity_id> cell;
    entity_id at(int x, int y) const { return cell[static_cast<std::size_t>(y * GW + x)]; }
};

void build(fixture& f)
{
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Flat"; bc.type = body_type::planet; bc.grid_width = GW; bc.grid_height = GH;
    w.bodies[f.body] = bc;
    for (int y = 0; y < GH; ++y)
        for (int x = 0; x < GW; ++x)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body = f.body; tc.grid_x = x; tc.grid_y = y;
            tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass;
            tc.cover_density = 150; tc.landform = terrain_landform::plains; tc.habitability = 0.5f;
            w.tiles[t] = tc;
            f.cell.push_back(t);
        }
    build_province_partition(w, 0x1443u);
    const auto add_nation = [&](const char* name) {
        const entity_id n = w.create_entity();
        nation_component nc{};
        nc.name = name; nc.treasury = 1.0e6f;
        w.nations[n] = nc;
        return n;
    };
    f.A = add_nation("Aster");
    f.B = add_nation("Brael");
    f.C = add_nation("Corvan");
}

entity_id add_unit(fixture& f, entity_id owner, entity_id tile, int count = 500)
{
    const entity_id u = f.w.create_entity();
    unit_component uc{};
    uc.position = tile; uc.owner = owner; uc.count = count; uc.type = ROW_INFANTRY;
    uc.establishment = count;
    f.w.units[u] = uc;
    return u;
}

corp_command_result march(fixture& f, const recipe_registry& reg, entity_id actor, entity_id unit,
                          entity_id tile)
{
    corp_command c; c.corp = actor; c.as_nation = actor; c.verb = corp_verb::march_unit;
    c.subject = unit; c.tile = tile;
    return apply_corp_command(f.w, reg, c);
}

/// A march pass with no Logistic Point resolve (the fixture's nations make none).
unit_march_tick step_march(fixture& f, const recipe_registry& reg, int n = 1)
{
    unit_march_tick last;
    for (int i = 0; i < n; ++i)
        last = run_unit_march(f.w, reg);
    return last;
}

/// Tiles held by more than one nation (count > 0 units), the R1 invariant.
std::size_t shared_tiles(const world& w)
{
    std::map<entity_id, std::set<entity_id>> owners;
    for (const auto& [uid, u] : w.units)
        if (u.count > 0 && u.position != null_entity)
            owners[u.position].insert(u.owner);
    std::size_t n = 0;
    for (const auto& [t, o] : owners)
        n += o.size() > 1 ? 1 : 0;
    return n;
}

bool in_list(const std::vector<entity_id>& v, entity_id x)
{
    return std::find(v.begin(), v.end(), x) != v.end();
}

const active_battle* battle_over(const world& w, entity_id tile)
{
    for (const active_battle& b : w.battles)
        if (b.tile == tile) return &b;
    return nullptr;
}

} // namespace

int main()
{
    std::printf("=== tile contact (BL-1443) ===\n");
    const recipe_registry reg = make_registry();

    // -----------------------------------------------------------------------
    std::printf("\nT1 - a march ends on its tile\n");
    {
        fixture f; build(f);
        const entity_id u = add_unit(f, f.A, f.at(0, 4));
        // A destination two tiles DEEP in its province along row 4, so the march
        // enters the province before it reaches the tile: under the retired grain
        // it would have stopped on entering.
        entity_id dest = null_entity;
        for (int x = 4; x < 16 && dest == null_entity; ++x)
        {
            const uint32_t p = f.w.provinces.province_of(f.at(x, 4));
            if (p != 0 && f.w.provinces.province_of(f.at(x - 1, 4)) == p
                && f.w.provinces.province_of(f.at(x - 2, 4)) == p
                && f.w.provinces.province_of(f.at(0, 4)) != p)
                dest = f.at(x, 4);
        }
        check(dest != null_entity, "T1 setup: a destination two tiles deep in its province");
        check(march(f, reg, f.A, u, dest) == corp_command_result::applied, "T1a the march to a tile applies");
        check(f.w.units.at(u).order.dest == dest && f.w.units.at(u).order.path.front() == f.at(0, 4)
                  && f.w.units.at(u).order.path.back() == dest,
              "T1b the order names the tile, and its path runs from the unit to it");
        // Under the retired grain the march ended on ENTERING the destination's
        // province; count the ticks it walks on inside that province instead.
        const uint32_t dprov = f.w.provinces.province_of(dest);
        int inside_short = 0;
        for (int i = 0; i < 30 && f.w.units.at(u).order.dest != null_entity; ++i)
        {
            step_march(f, reg);
            const unit_component& uu = f.w.units.at(u);
            if (uu.position != dest && f.w.provinces.province_of(uu.position) == dprov)
                ++inside_short;
        }
        check(f.w.units.at(u).position == dest && f.w.units.at(u).order.dest == null_entity,
              "T1c THE MARCH ENDED ON ITS TILE, and the order cleared on arrival");
        std::printf("        (ticks spent inside the destination's province short of the tile: %d)\n", inside_short);
        check(inside_short >= 1,
              "T1e ...having walked ON inside the destination's province (where the retired grain stopped it)");
        corp_command none; none.corp = f.A; none.as_nation = f.A; none.verb = corp_verb::march_unit;
        none.subject = u; none.province = dprov; // a province, and no tile
        const uint64_t h = f.w.state_hash(1);
        check(apply_corp_command(f.w, reg, none) == corp_command_result::rejected_invalid && f.w.state_hash(1) == h,
              "T1d a march naming only a province (no tile) is refused: cmd.province is ignored");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT2 - a path avoids foreign stacks, save its destination\n");
    {
        fixture f; build(f);
        const entity_id u = add_unit(f, f.A, f.at(0, 4));
        // The straight route along row 4 is the cheapest; stand neutral stacks on it.
        add_unit(f, f.C, f.at(3, 4));
        add_unit(f, f.C, f.at(6, 4));
        const entity_id held_dest = f.at(9, 4);
        add_unit(f, f.B, held_dest); // the destination itself is held
        check(march(f, reg, f.A, u, held_dest) == corp_command_result::applied, "T2a the march applies");
        const std::vector<entity_id>& path = f.w.units.at(u).order.path;
        const std::set<entity_id> foreign = foreign_held_tiles(f.w, f.A);
        bool clear = !path.empty() && path.back() == held_dest;
        for (std::size_t i = 1; clear && i + 1 < path.size(); ++i)
            clear = foreign.count(path[i]) == 0;
        check(clear, "T2b the path steps on no tile another nation holds — only its destination is one");
        const logistics_path& raw = intra_body_path(f.w, f.body, f.at(0, 4), held_dest);
        bool raw_crosses = false;
        for (const entity_id t : raw.tiles) raw_crosses = raw_crosses || t == f.at(3, 4) || t == f.at(6, 4);
        check(raw_crosses, "T2c (the unconstrained route did run through them — the avoidance is real)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT3 - a non-hostile stack blocks: re-path, or clear\n");
    {
        fixture f; build(f);
        const entity_id u = add_unit(f, f.A, f.at(0, 4));
        const entity_id dest = f.at(8, 4);
        (void)march(f, reg, f.A, u, dest);
        const std::vector<entity_id> first = f.w.units.at(u).order.path;
        // A neutral stack steps onto the route AFTER the order was given.
        const entity_id in_way = first.size() > 3 ? first[3] : null_entity;
        const entity_id cu = add_unit(f, f.C, in_way);
        int blocks = 0;
        bool never_on = true;
        for (int i = 0; i < 30 && f.w.units.at(u).order.dest != null_entity; ++i)
        {
            blocks += step_march(f, reg).blocked;
            never_on = never_on && f.w.units.at(u).position != f.w.units.at(cu).position;
        }
        check(in_way != null_entity && never_on,
              "T3a the marcher never stepped onto the neutral stack's tile (checked every tick)");
        check(blocks >= 1 && f.w.units.at(u).position == dest && f.w.battles.empty(),
              "T3b blocked, it RE-PATHED round the stack and arrived — no battle");

        // With no path: the destination itself is held by a neutral.
        const entity_id u2 = add_unit(f, f.A, f.at(0, 1));
        add_unit(f, f.C, f.at(4, 1));
        (void)march(f, reg, f.A, u2, f.at(4, 1));
        for (int i = 0; i < 10 && f.w.units.at(u2).order.dest != null_entity; ++i) step_march(f, reg);
        check(f.w.units.at(u2).order.dest == null_entity && f.w.units.at(u2).position != f.at(4, 1)
                  && f.w.battles.empty(),
              "T3c a neutral holding the destination: no path remains, the order CLEARED short of it");
        check(shared_tiles(f.w) == 0, "T3d no tile holds two nations");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT5 - hostile contact mid-path stops the march and opens a battle over the defender's tile\n");
    {
        fixture f; build(f);
        (void)declare_hostile(f.w, f.A, f.B);
        const entity_id u = add_unit(f, f.A, f.at(0, 4));
        const entity_id dest = f.at(10, 4);
        (void)march(f, reg, f.A, u, dest);
        const std::vector<entity_id> path = f.w.units.at(u).order.path;
        const entity_id mid = path[4];
        const entity_id before_mid = path[3];
        const entity_id bu = add_unit(f, f.B, mid); // B steps onto the route afterwards
        bool opened = false;
        for (int i = 0; i < 10 && !opened; ++i)
            opened = step_march(f, reg).battles_opened > 0;
        const active_battle* b = battle_over(f.w, mid);
        check(opened && b != nullptr && b->attacker == f.A && b->defender == f.B,
              "T5a a battle opened over B's tile, A attacking — mid-path, not at the march's end");
        check(battle_over(f.w, dest) == nullptr && f.w.units.at(u).position == before_mid,
              "T5b the march STOPPED on the tile before the stack");
        check(f.w.units.at(u).order.dest == null_entity, "T5c ...and its order is spent");
        check(b != nullptr && in_list(b->attacking_units, u) && in_list(b->defender_units, bu),
              "T5d the marcher is the attacking stack; B's stack defends");
        check(shared_tiles(f.w) == 0, "T5e no tile holds two nations");
        // The directed test: B toward A is not hostile, so B marching onto A is blocked.
        fixture g; build(g);
        (void)declare_hostile(g.w, g.A, g.B);
        const entity_id gb = add_unit(g, g.B, g.at(2, 2));
        add_unit(g, g.A, g.at(3, 2));
        (void)march(g, reg, g.B, gb, g.at(3, 2));
        const unit_march_tick mt = step_march(g, reg);
        check(mt.attacks == 0 && mt.blocked == 1 && g.w.battles.empty(),
              "T5f hostility is DIRECTED: the target marching onto the declarer is blocked, not an attack");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT6 - adjacency alone opens nothing\n");
    {
        fixture f; build(f);
        (void)declare_hostile(f.w, f.A, f.B);
        (void)declare_hostile(f.w, f.B, f.A);
        add_unit(f, f.A, f.at(4, 4));
        add_unit(f, f.B, f.at(5, 4));
        for (int i = 0; i < 5; ++i)
        {
            (void)run_battles(f.w, reg, i);
            step_march(f, reg);
        }
        check(f.w.battles.empty(), "T6a mutually hostile stacks side by side for five ticks: no battle");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT7 - adjacent stacks join each side (own nation only, out of other battles)\n");
    {
        fixture f; build(f);
        (void)declare_hostile(f.w, f.A, f.B);
        (void)declare_hostile(f.w, f.C, f.B);
        (void)offer_friendship(f.w, f.C, f.A);
        (void)accept_friendship(f.w, f.C, f.A);
        const entity_id T = f.at(5, 4);
        const entity_id atk = add_unit(f, f.A, f.at(4, 4));
        const entity_id defu = add_unit(f, f.B, T);
        // Every other hex neighbour of T, ascending.
        std::vector<entity_id> beside;
        for (int s = 0; s < 6; ++s)
            if (const entity_id n = road_edge_neighbour(f.w, T, s); n != null_entity && n != f.at(4, 4))
                beside.push_back(n);
        std::sort(beside.begin(), beside.end());
        beside.erase(std::unique(beside.begin(), beside.end()), beside.end());
        check(beside.size() >= 4, "T7 setup: the tile has free neighbours to stand on");
        const entity_id a_join = add_unit(f, f.A, beside[0]);
        const entity_id b_join = add_unit(f, f.B, beside[1]);
        const entity_id ally   = add_unit(f, f.C, beside[2]);          // A's friend, beside the tile
        const entity_id a_busy = add_unit(f, f.A, beside[3]);          // in another battle already
        const entity_id far_a  = add_unit(f, f.A, f.at(0, 0));         // not adjacent
        // a_busy is already fighting somewhere else.
        const entity_id other = add_unit(f, f.B, f.at(9, 7));
        active_battle ob{};
        ob.tile = f.at(9, 7); ob.attacker = f.A; ob.defender = f.B;
        ob.attacker_units = { a_busy }; ob.defender_units = { other };
        f.w.battles.push_back(ob);
        (void)march(f, reg, f.A, atk, T);
        step_march(f, reg);
        const active_battle* b = battle_over(f.w, T);
        check(b != nullptr, "T7a the attack opened");
        if (b != nullptr)
        {
            check(in_list(b->attacker_units, atk) && in_list(b->attacker_units, a_join),
                  "T7b the attacker's stack beside the tile JOINED the attack");
            check(in_list(b->defender_units, defu) && in_list(b->defender_units, b_join),
                  "T7c the defender's stack beside the tile JOINED the defence");
            check(!in_list(b->attacker_units, ally) && !in_list(b->defender_units, ally),
                  "T7d an ALLY's stack beside the tile does not join: each side draws on its own nation");
            check(!in_list(b->attacker_units, a_busy), "T7e a stack already in another battle does not join");
            check(!in_list(b->attacker_units, far_a), "T7f a stack not beside the tile does not join");
            check(b->attacking_units == std::vector<entity_id>{ atk },
                  "T7g the attacking stack is the marcher alone; the joiner is on the side, not in the stack");
            check(std::is_sorted(b->attacker_units.begin(), b->attacker_units.end())
                      && std::is_sorted(b->defender_units.begin(), b->defender_units.end()),
                  "T7h both sides are stored ascending");
        }
    }

    // -----------------------------------------------------------------------
    std::printf("\nT8 - the ground is the defended tile\n");
    {
        fixture f; build(f);
        (void)declare_hostile(f.w, f.A, f.B);
        const entity_id T = f.at(5, 4);
        tile_component& tt = f.w.tiles.at(T);
        tt.landform = terrain_landform::highland; tt.cover = terrain_cover::forest; tt.cover_density = 200;
        tile_component& from = f.w.tiles.at(f.at(4, 4));
        from.landform = terrain_landform::mountain; // the attacker's own tile, better ground
        const entity_id atk = add_unit(f, f.A, f.at(4, 4));
        add_unit(f, f.B, T);
        (void)march(f, reg, f.A, atk, T);
        step_march(f, reg);
        const active_battle* b = battle_over(f.w, T);
        check(b != nullptr && b->ground_tile == T && b->state.terrain_lf == terrain_landform::highland
                  && b->state.terrain_cov == terrain_cover::forest && b->state.terrain_density == 200,
              "T8a the battle reads the DEFENDED tile's terrain, not the attacker's");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT9 - on a win the attacking stack advances; joined stacks stay\n");
    {
        fixture f; build(f);
        (void)declare_hostile(f.w, f.A, f.B);
        const entity_id T = f.at(5, 4);
        const entity_id atk = add_unit(f, f.A, f.at(4, 4));
        std::vector<entity_id> beside;
        for (int s = 0; s < 6; ++s)
            if (const entity_id n = road_edge_neighbour(f.w, T, s); n != null_entity && n != f.at(4, 4))
                beside.push_back(n);
        std::sort(beside.begin(), beside.end());
        const entity_id joiner = add_unit(f, f.A, beside[0]);
        const entity_id du = add_unit(f, f.B, T);
        f.w.units.at(du).readiness_permille = 0; // spent: broken on round 1, by construction
        (void)march(f, reg, f.A, atk, T);
        step_march(f, reg);
        check(battle_over(f.w, T) != nullptr, "T9 setup: the attack opened");
        const battle_tick bt = run_battles(f.w, reg, 1);
        bool won = false;
        for (const battle_dispatch& d : bt.dispatches) won = won || (d.tile == T && d.field_held_by == f.A);
        check(won, "T9a the attacker held the field");
        check(f.w.units.at(atk).position == T, "T9b THE ATTACKING STACK ADVANCED onto the defended tile");
        check(f.w.units.at(joiner).position == beside[0], "T9c the joined stack STAYED where it stood");
        check(f.w.units.at(du).position != T, "T9d the broken defender left the tile");
        check(shared_tiles(f.w) == 0, "T9e no tile holds two nations");

        // A defender with nowhere to retreat stays on its tile: the advance waits.
        fixture g; build(g);
        (void)declare_hostile(g.w, g.A, g.B);
        const entity_id T2 = g.at(5, 4);
        // Every crossable neighbour of T2 is water, save the attacker's tile.
        const int row = g.w.tiles.at(T2).grid_y;
        for (int s = 0; s < 6; ++s)
            if (const entity_id n = road_edge_neighbour(g.w, T2, s); n != null_entity && n != g.at(4, 4)
                && road_side_is_crossable(s, row))
                g.w.tiles.at(n).substrate = terrain_substrate::ocean;
        invalidate_logistics_caches(g.w);
        const entity_id atk2 = add_unit(g, g.A, g.at(4, 4));
        const entity_id du2 = add_unit(g, g.B, T2);
        g.w.units.at(du2).readiness_permille = 0;
        (void)march(g, reg, g.A, atk2, T2);
        step_march(g, reg);
        check(battle_over(g.w, T2) != nullptr, "T9 setup: the cornered defender is attacked");
        const battle_tick bt2 = run_battles(g.w, reg, 1);
        bool won2 = false;
        for (const battle_dispatch& d : bt2.dispatches) won2 = won2 || d.field_held_by == g.A;
        check(won2 && g.w.units.at(du2).position == T2 && g.w.units.at(atk2).position == g.at(4, 4)
                  && shared_tiles(g.w) == 0,
              "T9f a defender with nowhere to go stays put, and the attacker does NOT advance onto it");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT10 - a unit lined along a province boundary can be attacked\n");
    {
        fixture f; build(f);
        (void)declare_hostile(f.w, f.A, f.B);
        // Two east-west neighbours in DIFFERENT provinces: the boundary the old
        // co-presence trigger could never fight across.
        entity_id s = null_entity, t = null_entity;
        for (int y = 0; y < GH && s == null_entity; ++y)
            for (int x = 0; x + 1 < GW; ++x)
                if (f.w.provinces.province_of(f.at(x, y)) != f.w.provinces.province_of(f.at(x + 1, y))
                    && f.w.provinces.province_of(f.at(x, y)) != 0 && f.w.provinces.province_of(f.at(x + 1, y)) != 0)
                {
                    s = f.at(x, y); t = f.at(x + 1, y);
                    break;
                }
        check(s != null_entity, "T10 setup: the partition has a boundary between two east-west neighbours");
        if (s != null_entity)
        {
            const entity_id a = add_unit(f, f.A, s);
            add_unit(f, f.B, t);
            (void)march(f, reg, f.A, a, t);
            step_march(f, reg);
            const active_battle* b = battle_over(f.w, t);
            check(b != nullptr && b->province == f.w.provinces.province_of(t)
                      && f.w.provinces.province_of(s) != b->province,
                  "T10a THE ATTACK ACROSS THE PROVINCE BOUNDARY OPENED — over the defender's tile, in its province");
        }
    }

    // -----------------------------------------------------------------------
    std::printf("\nT11 - retreat never lands on a foreign tile\n");
    {
        fixture f; build(f);
        const entity_id S = f.at(5, 4);
        const int row = f.w.tiles.at(S).grid_y;
        std::vector<entity_id> crossable;
        for (int s = 0; s < 6; ++s)
            if (road_side_is_crossable(s, row))
                if (const entity_id n = road_edge_neighbour(f.w, S, s); n != null_entity)
                    crossable.push_back(n);
        std::sort(crossable.begin(), crossable.end());
        // The attacker on S, spent; B (its enemy) on one side; a NEUTRAL C on every
        // other crossable side but one — the lowest, so a tie would have taken it.
        check(crossable.size() >= 3, "T11 setup: S has at least three crossable neighbours");
        const entity_id au = add_unit(f, f.A, S);
        const entity_id free_tile = crossable.back();
        const entity_id bu = add_unit(f, f.B, crossable[0]);
        for (std::size_t k = 1; k + 1 < crossable.size(); ++k)
            add_unit(f, f.C, crossable[k]);
        f.w.units.at(au).readiness_permille = 0;
        active_battle b{};
        b.tile = crossable[0]; b.province = f.w.provinces.province_of(crossable[0]);
        b.ground_tile = b.tile; b.attacker = f.A; b.defender = f.B;
        b.attacker_units = { au }; b.defender_units = { bu };
        b.state.stream_seed = 0x1443u; b.state.rng_state = b.state.stream_seed;
        f.w.battles.push_back(b);
        (void)run_battles(f.w, reg, 0);
        check(f.w.units.at(au).position == free_tile,
              "T11a the spent unit retreated onto the one tile no other nation holds (not the neutral's)");
        check(shared_tiles(f.w) == 0, "T11b no tile holds two nations");

        // Every crossable side held: it stays put.
        fixture g; build(g);
        const entity_id S2 = g.at(5, 4);
        const entity_id au2 = add_unit(g, g.A, S2);
        entity_id first = null_entity;
        std::vector<entity_id> held2;
        for (int s = 0; s < 6; ++s)
            if (road_side_is_crossable(s, g.w.tiles.at(S2).grid_y))
                if (const entity_id n = road_edge_neighbour(g.w, S2, s); n != null_entity)
                    held2.push_back(n);
        std::sort(held2.begin(), held2.end());
        first = held2[0];
        const entity_id bu2 = add_unit(g, g.B, first);
        for (std::size_t k = 1; k < held2.size(); ++k) add_unit(g, g.C, held2[k]);
        g.w.units.at(au2).readiness_permille = 0;
        active_battle b2{};
        b2.tile = first; b2.ground_tile = first; b2.attacker = g.A; b2.defender = g.B;
        b2.attacker_units = { au2 }; b2.defender_units = { bu2 };
        b2.state.stream_seed = 0x1444u; b2.state.rng_state = b2.state.stream_seed;
        g.w.battles.push_back(b2);
        (void)run_battles(g.w, reg, 0);
        check(g.w.units.at(au2).position == S2 && !unit_in_battle(g.w, au2),
              "T11c every way out held by another nation: it stays put, out of the fight");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT12 - every verb refusal is hash-identical\n");
    {
        fixture f; build(f);
        (void)declare_hostile(f.w, f.A, f.B);
        const entity_id u  = add_unit(f, f.A, f.at(2, 2));
        const entity_id bu = add_unit(f, f.B, f.at(3, 2));
        // A second body, for the off-body case.
        const entity_id other_body = f.w.create_entity();
        body_component ob{}; ob.grid_width = 1; ob.grid_height = 1; f.w.bodies[other_body] = ob;
        const entity_id off = f.w.create_entity();
        tile_component otc{}; otc.body = other_body; otc.landform = terrain_landform::plains;
        f.w.tiles[off] = otc;
        (void)open_tile_battle(f.w, 0, f.at(3, 2), f.A, f.B, { u }); // u is in a battle
        const entity_id idle = add_unit(f, f.A, f.at(0, 7));
        const entity_id fitting = add_unit(f, f.A, f.at(1, 7));
        f.w.units.at(fitting).refit_type = 1; f.w.units.at(fitting).refit_days = 90;

        const std::string bytes = to_bytes(f.w);
        const uint64_t hash = f.w.state_hash(3);
        struct row { const char* what; entity_id actor, unit, tile; corp_command_result want; };
        const row rows[] = {
            { "unknown unit",               f.A, 0x7FFFFFF0u, f.at(5, 5), corp_command_result::rejected_invalid },
            { "not the actor's unit",       f.B, idle, f.at(5, 5),        corp_command_result::rejected_not_owner },
            { "no tile (0)",                f.A, idle, null_entity,       corp_command_result::rejected_invalid },
            { "a tile that does not exist", f.A, idle, 0x7FFFFFF0u,       corp_command_result::rejected_invalid },
            { "a tile on another body",     f.A, idle, off,               corp_command_result::rejected_invalid },
            { "already there",              f.A, idle, f.at(0, 7),        corp_command_result::rejected_state },
            { "in a battle",                f.A, u, f.at(5, 5),           corp_command_result::rejected_state },
            { "refitting",                  f.A, fitting, f.at(5, 5),     corp_command_result::rejected_state },
        };
        for (const row& r : rows)
        {
            const corp_command_result got = march(f, reg, r.actor, r.unit, r.tile);
            char what[160];
            std::snprintf(what, sizeof what, "T12 march_unit refused (%s) and the world is byte- and hash-identical", r.what);
            check(got == r.want && to_bytes(f.w) == bytes && f.w.state_hash(3) == hash, what);
        }
        const auto withdraw = [&](entity_id actor, entity_id tile, entity_id against) {
            corp_command c; c.corp = actor; c.as_nation = actor; c.verb = corp_verb::withdraw_from_battle;
            c.tile = tile; c.counterparty = against;
            return apply_corp_command(f.w, reg, c);
        };
        check(withdraw(f.A, f.at(2, 2), null_entity) == corp_command_result::rejected_state
                  && to_bytes(f.w) == bytes && f.w.state_hash(3) == hash,
              "T12 withdraw naming the ATTACKER's tile (not the defended one) refused, world identical");
        check(withdraw(f.A, null_entity, null_entity) == corp_command_result::rejected_state
                  && to_bytes(f.w) == bytes,
              "T12 withdraw naming no tile refused, world identical");
        check(withdraw(f.C, f.at(3, 2), null_entity) == corp_command_result::rejected_state
                  && to_bytes(f.w) == bytes,
              "T12 withdraw by a non-participant refused, world identical");
        check(withdraw(f.A, f.at(3, 2), f.C) == corp_command_result::rejected_state && to_bytes(f.w) == bytes,
              "T12 withdraw naming the wrong counterparty refused, world identical");
        check(withdraw(f.A, f.at(3, 2), f.B) == corp_command_result::applied
                  && f.w.battles.front().withdraw_requested == withdrawing_side::attacker,
              "T12 the right request, named by the defended tile, applies");
        (void)bu;
    }

    // -----------------------------------------------------------------------
    std::printf("\nT13 - the save round-trips a tile battle, and a v62 stream reads\n");
    {
        fixture f; build(f);
        (void)declare_hostile(f.w, f.A, f.B);
        const entity_id T = f.at(5, 4);
        const entity_id atk = add_unit(f, f.A, f.at(4, 4));
        std::vector<entity_id> beside;
        for (int s = 0; s < 6; ++s)
            if (const entity_id n = road_edge_neighbour(f.w, T, s); n != null_entity && n != f.at(4, 4))
                beside.push_back(n);
        std::sort(beside.begin(), beside.end());
        const entity_id joiner = add_unit(f, f.A, beside[0]);
        add_unit(f, f.B, T);
        (void)march(f, reg, f.A, atk, T);
        step_march(f, reg);
        check(f.w.battles.size() == 1 && f.w.battles.front().attacking_units == std::vector<entity_id>{ atk }
                  && in_list(f.w.battles.front().attacker_units, joiner),
              "T13 setup: one battle, the marcher its attacking stack, the joiner on its side");
        const std::string bytes = to_bytes(f.w);
        world back;
        const bool ok = from_bytes(bytes, back);
        check(ok && back.battles.size() == 1 && back.battles.front().tile == T
                  && back.battles.front().ground_tile == T
                  && back.battles.front().province == f.w.battles.front().province
                  && back.battles.front().attacking_units == f.w.battles.front().attacking_units
                  && back.battles.front().attacker_units == f.w.battles.front().attacker_units
                  && back.state_hash(4) == f.w.state_hash(4) && to_bytes(back) == bytes,
              "T13a the tile battle round-trips by value: its tile, its stack, hash and bytes equal");
        // And it plays on identically from the loaded copy.
        world a = f.w;
        for (int i = 0; i < 3; ++i) { (void)run_battles(a, reg, i); (void)run_battles(back, reg, i); }
        check(a.state_hash(9) == back.state_hash(9), "T13b three ticks on, the loaded world agrees with the original");

        std::string v62;
        {
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_pre_tile(f.w, o);
            v62 = o.str();
        }
        uint32_t stamped = 0;
        std::memcpy(&stamped, &v62[4], sizeof stamped);
        world old;
        const bool old_ok = from_bytes(v62, old);
        check(old_ok && stamped == k_world_save_version_pre_tile && old.battles.size() == 1
                  && old.battles.front().tile == T && old.battles.front().attacking_units.empty(),
              "T13c a v62 stream reads: its battle keyed by its ground tile, with no attacking stack");
        check(v62.size() == bytes.size() - (4 + 4 + 4) - 12 - 24 // BL-1434: v64's empty designs section; BL-1449: v65's empty air section
                               - world_save_battle_carry_section_bytes(f.w), // BL-1453: v67's battle carry
              "T13d ...and the v62 stream is today's less exactly the stack section (one battle of one unit) and the empty designs section");
        // An attacking stack naming a unit not on the attacker's side is refused.
        std::string bad = bytes;
        const entity_id ghost = 0x7FFFFFF0u;
        std::memcpy(&bad[bytes.size() - 4], &ghost, sizeof ghost);
        world s1;
        check(!from_bytes(bad, s1) && s1.battles.empty() && s1.units.empty(),
              "T13e a stack naming a unit outside the battle's attacker side is REFUSED, destination untouched");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT14 - a finished muster waits while a foreign stack stands on its base\n");
    {
        fixture f; build(f);
        const entity_id base_tile = f.at(2, 6);
        const entity_id base = f.w.create_entity();
        building_component bc{};
        bc.tile = base_tile; bc.type = building_type::military_base; bc.ticks_remaining = 0;
        f.w.buildings[base] = bc;
        f.w.nations.at(f.A).state_assets.push_back(base);
        muster_order m; m.id = 1; m.base = base; m.owner = f.A; m.unit_type = ROW_INFANTRY;
        m.days_total = 90; m.days_remaining = 0;
        f.w.musters.push_back(m);
        const entity_id passer = add_unit(f, f.C, base_tile);
        const std::size_t before = f.w.units.size();
        advance_musters(f.w);
        check(f.w.units.size() == before && f.w.musters.size() == 1 && f.w.musters.front().days_remaining == 0,
              "T14a with C's stack on the base the finished muster WAITS (no unit raised onto it)");
        f.w.units.at(passer).position = f.at(3, 6);
        advance_musters(f.w);
        check(f.w.units.size() == before + 1 && f.w.musters.empty() && shared_tiles(f.w) == 0,
              "T14b the tile clear, it completes on its base");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT15 - a registered attack keeps its order unless a battle opens with it\n");
    {
        fixture f; build(f);
        (void)declare_hostile(f.w, f.A, f.B);
        const entity_id T  = f.at(5, 4);
        const entity_id au = add_unit(f, f.A, f.at(4, 4)); // the lower id: visited first
        const entity_id bu = add_unit(f, f.B, T);
        check(au < bu, "T15 setup: the attacker is the lower id, so it is visited first");
        check(march(f, reg, f.A, au, T) == corp_command_result::applied, "T15 setup: A's attack march applies");
        check(march(f, reg, f.B, bu, f.at(5, 7)) == corp_command_result::applied,
              "T15 setup: B's stack is ordered off the tile");
        const unit_march_tick mt = step_march(f, reg);
        check(mt.attacks == 1 && mt.battles_opened == 0 && f.w.battles.empty()
                  && f.w.units.at(bu).position != T,
              "T15 setup: A registered the attack, then B walked off T in the same pass: nothing opened");
        check(f.w.units.at(au).order.dest == T && f.w.units.at(au).position == f.at(4, 4),
              "T15a THE REFUSED ATTACK HANDED THE ORDER BACK: A still marches on T (it was wiped before)");
        step_march(f, reg);
        check(f.w.units.at(au).position == T && f.w.units.at(au).order.dest == null_entity
                  && shared_tiles(f.w) == 0,
              "T15b next tick A carried on onto the emptied tile and arrived");

        // Pulled into an earlier battle first: a1 stands beside T1 (the lower tile id)
        // and attacks T2; a2's attack on T1 opens first and takes a1 in as a joiner,
        // so a1's own attack finds it busy and is refused - its order must come back.
        fixture g; build(g);
        (void)declare_hostile(g.w, g.A, g.B);
        const entity_id T1 = g.at(4, 4), T2 = g.at(6, 4);
        const entity_id a1 = add_unit(g, g.A, g.at(5, 4));
        const entity_id a2 = add_unit(g, g.A, g.at(3, 4));
        add_unit(g, g.B, T1);
        add_unit(g, g.B, T2);
        check(march(g, reg, g.A, a1, T2) == corp_command_result::applied
                  && march(g, reg, g.A, a2, T1) == corp_command_result::applied && T1 < T2,
              "T15 setup: two attacks, onto T1 and T2 (T1 the lower tile id)");
        const unit_march_tick gt = step_march(g, reg);
        const active_battle* b1 = battle_over(g.w, T1);
        check(gt.attacks == 2 && gt.battles_opened == 1 && b1 != nullptr && in_list(b1->attacker_units, a1)
                  && !in_list(b1->attacking_units, a1) && battle_over(g.w, T2) == nullptr,
              "T15 setup: T1's battle opened and took a1 in beside it; T2's open found a1 busy");
        check(g.w.units.at(a1).order.dest == T2 && g.w.units.at(a2).order.dest == null_entity,
              "T15c the unit pulled into the earlier battle KEEPS its order; the one whose battle opened spent it");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT16 - a spent unit's march onto a hostile stack is refused\n");
    {
        fixture f; build(f);
        (void)declare_hostile(f.w, f.A, f.B);
        const entity_id au = add_unit(f, f.A, f.at(4, 4));
        add_unit(f, f.B, f.at(6, 4));
        add_unit(f, f.C, f.at(4, 6));
        f.w.units.at(au).readiness_permille = 0;
        const std::string bytes = to_bytes(f.w);
        const uint64_t hash = f.w.state_hash(5);
        check(march(f, reg, f.A, au, f.at(6, 4)) == corp_command_result::rejected_state
                  && to_bytes(f.w) == bytes && f.w.state_hash(5) == hash,
              "T16a a spent unit marching onto a HOSTILE stack is refused (rejected_state), world identical");
        check(march_check(f.w, f.A, au, f.at(4, 6), f.C) == corp_command_result::rejected_state
                  && march_check(f.w, f.A, au, f.at(4, 6)) == corp_command_result::applied
                  && to_bytes(f.w) == bytes,
              "T16b march_check reads a named nation as hostile already (the declare-and-attack pre-check)");
        check(march(f, reg, f.A, au, f.at(4, 6)) == corp_command_result::applied,
              "T16c ...but a spent unit marching onto a NEUTRAL stack applies (a block, not an attack)");
        f.w.units.at(au).order = movement_order{};
        check(march(f, reg, f.A, au, f.at(9, 1)) == corp_command_result::applied,
              "T16d ...and a spent unit may still march onto empty ground");
        f.w.units.at(au).order = movement_order{};
        f.w.units.at(au).readiness_permille = 1;
        check(march(f, reg, f.A, au, f.at(6, 4)) == corp_command_result::applied,
              "T16e with any readiness left the attack march applies");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT17 - a saved battle over a tile the stream does not hold is refused\n");
    {
        fixture f; build(f);
        (void)declare_hostile(f.w, f.A, f.B);
        const entity_id T = f.at(5, 4);
        const entity_id atk = add_unit(f, f.A, f.at(4, 4));
        add_unit(f, f.B, T);
        (void)march(f, reg, f.A, atk, T);
        step_march(f, reg);
        check(f.w.battles.size() == 1, "T17 setup: one battle");
        world good;
        check(from_bytes(to_bytes(f.w), good), "T17 setup: the honest stream reads");
        f.w.battles.front().tile = f.w.battles.front().ground_tile = 0x7FFFFFF0u; // names no tile
        const std::string bad = to_bytes(f.w);
        world s1;
        check(!from_bytes(bad, s1) && s1.battles.empty() && s1.units.empty(),
              "T17a a battle whose identity tile is not a tile of the stream is REFUSED whole, destination untouched");
        std::string v62;
        {
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_pre_tile(f.w, o);
            v62 = o.str();
        }
        world s2;
        check(!from_bytes(v62, s2) && s2.battles.empty(), "T17b ...and so is a v62 stream's");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT18 - a pre-v63 stream with two nations on one tile loads with one\n");
    {
        fixture f; build(f);
        const entity_id T = f.at(5, 4);
        const entity_id a  = add_unit(f, f.A, T);  // the lowest id on T: A keeps it
        const entity_id b  = add_unit(f, f.B, T);
        const entity_id c  = add_unit(f, f.C, T);
        const entity_id a2 = add_unit(f, f.A, T);  // A's second unit stays with A's stack
        // B's own ground beside T: the step-off prefers it over a lower-id neutral tile.
        std::vector<entity_id> cross;
        for (int sd = 0; sd < 6; ++sd)
            if (road_side_is_crossable(sd, f.w.tiles.at(T).grid_y))
                if (const entity_id n = road_edge_neighbour(f.w, T, sd); n != null_entity)
                    cross.push_back(n);
        std::sort(cross.begin(), cross.end());
        check(cross.size() >= 3, "T18 setup: T has at least three crossable neighbours");
        const entity_id b_home = cross.back();
        f.w.tile_to_nation[b_home] = f.B;
        // A cornered tile S: every crossable neighbour water, save one A holds. B's unit
        // there has nowhere to go and is disbanded.
        const entity_id S = f.at(15, 4);
        std::vector<entity_id> s_cross;
        for (int sd = 0; sd < 6; ++sd)
            if (road_side_is_crossable(sd, f.w.tiles.at(S).grid_y))
                if (const entity_id n = road_edge_neighbour(f.w, S, sd); n != null_entity)
                    s_cross.push_back(n);
        std::sort(s_cross.begin(), s_cross.end());
        for (std::size_t k = 1; k < s_cross.size(); ++k)
            f.w.tiles.at(s_cross[k]).substrate = terrain_substrate::ocean;
        invalidate_logistics_caches(f.w);
        add_unit(f, f.A, s_cross[0]);
        const entity_id sa = add_unit(f, f.A, S);
        const entity_id sb = add_unit(f, f.B, S);
        // B and A in a (pre-v63, province-keyed) battle that lists the doomed unit.
        active_battle ob{};
        ob.tile = S; ob.ground_tile = S; ob.province = f.w.provinces.province_of(S);
        ob.attacker = f.A; ob.defender = f.B; ob.attacker_units = { sa }; ob.defender_units = { sb };
        ob.state.stream_seed = 0x1445u; ob.state.rng_state = ob.state.stream_seed;
        f.w.battles.push_back(ob);
        (void)march(f, reg, f.B, b, f.at(9, 1)); // a live order, which a step-off must drop
        check(shared_tiles(f.w) == 2, "T18 setup: two tiles each held by two or more nations");
        std::string v62;
        {
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_pre_tile(f.w, o);
            v62 = o.str();
        }
        world back;
        const bool ok = from_bytes(v62, back);
        check(ok && shared_tiles(back) == 0, "T18a THE OLD STREAM LOADS WITH NO TILE HELD BY TWO NATIONS");
        check(ok && back.units.at(a).position == T && back.units.at(a2).position == T,
              "T18b the nation of the lowest-id unit on T kept it, its whole stack in place");
        check(ok && back.units.at(b).position == b_home && back.units.at(b).order.dest == null_entity,
              "T18c B's unit stepped off onto its own ground (preferred over lower ids), its stale order dropped");
        bool c_ok = false;
        if (ok && back.units.count(c) != 0)
        {
            const entity_id ct = back.units.at(c).position;
            c_ok = ct != T && std::find(cross.begin(), cross.end(), ct) != cross.end()
                && !is_water(back.tiles.at(ct).substrate);
        }
        check(c_ok, "T18d C's unit stepped off one crossable land tile");
        check(ok && back.units.count(sb) == 0 && back.units.at(sa).position == S
                  && back.battles.size() == 1 && back.battles.front().defender_units.empty(),
              "T18e the unit with nowhere to go was DISBANDED and struck from its battle");
        // Stable once fixed: save -> load -> save is byte- and hash-identical.
        const std::string once = ok ? to_bytes(back) : std::string();
        world again;
        check(ok && from_bytes(once, again) && to_bytes(again) == once
                  && again.state_hash(6) == back.state_hash(6),
              "T18f the fixed world round-trips byte- and hash-identical");
        world twice;
        check(from_bytes(v62, twice) && to_bytes(twice) == once,
              "T18g the fix-up is deterministic: the same old stream loads to the same bytes twice");
        // A v63 stream is never touched by the fix-up (it cannot hold a shared tile
        // the march made; this one is written by hand to prove the gate).
        world cur;
        check(from_bytes(to_bytes(f.w), cur) && shared_tiles(cur) == 2,
              "T18h a v63 stream is read as written: the fix-up gates on the old version only");
    }

    // -----------------------------------------------------------------------
    std::printf("\nT4 - two nations never share a tile: generation, then a real world at war\n");
    {
        world w = make_hard_coded_world(no_prehistory());
        init_survey_states(w);
        (void)fold_to_ground(w);
        const std::size_t at_gen = shared_tiles(w);
        std::printf("  [info] %zu units at generation; tiles held by two nations: %zu\n", w.units.size(), at_gen);
        check(at_gen == 0, "T4a generation seeds no two nations' units on one tile");
        // War: every nation declares hostility toward each neighbour across its
        // fronts, so the rivals' scorer may push (MILITARY.md § Fronts) and every
        // contact is a tile attack.
        std::set<std::pair<entity_id, entity_id>> pairs;
        for (const auto& [n, fs] : derive_all_fronts(w))
            for (const front& fr : fs)
                pairs.insert({ n, fr.neighbour });
        for (const auto& [a, b] : pairs) (void)declare_hostile(w, a, b);
        // A front's push walks onto EMPTY ground first, and a path never runs through
        // a stack, so contact needs someone to aim at one: each tick, every idle
        // unfronted unit is ordered onto the tile of the lowest-id unit of a nation
        // its own is hostile to, on its body (a legal march_unit, as a player would).
        const auto aim = [&]() {
            std::vector<entity_id> ids;
            for (const auto& [id, u] : w.units) ids.push_back(id);
            std::sort(ids.begin(), ids.end());
            int issued = 0;
            for (const entity_id id : ids)
            {
                const unit_component& u = w.units.at(id);
                if (u.count <= 0 || u.front != null_entity || u.order.dest != null_entity
                    || unit_in_battle(w, id) || w.tiles.count(u.position) == 0)
                    continue;
                for (const entity_id e : ids)
                {
                    const unit_component& v = w.units.at(e);
                    if (v.count <= 0 || !is_hostile(w, u.owner, v.owner) || w.tiles.count(v.position) == 0
                        || w.tiles.at(v.position).body != w.tiles.at(u.position).body)
                        continue;
                    corp_command c; c.corp = u.owner; c.as_nation = u.owner;
                    c.verb = corp_verb::march_unit; c.subject = id; c.tile = v.position;
                    if (apply_corp_command(w, reg, c) == corp_command_result::applied) ++issued;
                    break;
                }
            }
            return issued;
        };
        {
            // The rivals' scorer first, so the fronts take their idle units (and push
            // where they may) before the harness aims the rest.
            corp_ai_params p; p.cadence_k = 1;
            economy_report rr;
            run_corp_strategic_step(w, reg, rr, 1, p);
        }
        std::size_t worst = 0;
        int opened = 0, fought = 0, issued = 0;
        for (int t = 1; t <= 10; ++t)
        {
            issued += aim();
            std::map<entity_id, entity_id> was;
            for (const auto& [id, u] : w.units) was[id] = u.position;
            w.current_day_tick = w.current_econ_tick = t;
            // The military passes in the tick's own order (economy_system.cpp run_economy_step), with
            // the march UNGATED: this harness's registry authors no recipes, so no nation makes
            // the Logistic Points a gated march draws, and a gated tick moves no one.
            advance_fronts(w, reg);
            const battle_tick bt = run_battles(w, reg, t);
            (void)run_unit_march(w, reg);
            reanchor_fronts(w);
            int moved = 0, ordered = 0;
            for (const auto& [id, u] : w.units)
            {
                const auto it = was.find(id);
                moved += (it != was.end() && it->second != u.position) ? 1 : 0;
                ordered += u.order.dest != null_entity ? 1 : 0;
            }
            std::printf("  [tick %d] %d units moved, %d hold orders, %zu live battles\n", t, moved, ordered,
                        w.battles.size());
            fought += static_cast<int>(bt.dispatches.size());
            worst = std::max(worst, shared_tiles(w));
        }
        opened = static_cast<int>(w.battles.size());
        std::printf("  [info] %d attack marches issued over ten ticks\n", issued);
        std::size_t assigned = 0;
        for (const auto& [id, u] : w.units) assigned += u.front != null_entity ? 1 : 0;
        std::printf("  [info] %zu hostile pairs; ten ticks: %zu units on fronts, %d dispatches, %d live battles\n",
                    pairs.size(), assigned, fought, opened);
        check(worst == 0, "T4b ten ticks of a world at war: no tile ever held by two nations");
        check(fought > 0 || opened > 0, "T4c ...and the war is real: battles were fought (the row was exercised)");
    }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    std::printf(failures == 0 ? "ALL PASS (0 failures)\n" : "FAILURES\n");
    return failures == 0 ? 0 : 1;
}
