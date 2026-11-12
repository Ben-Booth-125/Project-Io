// ---------------------------------------------------------------------------
// artillery_harness — BL-1643 (missile silos): artillery fires shells, not missiles.
// docs/military/MILITARY.md § Artillery fires shells, § Fronts. No SDL / Lua / ImGui.
// ---------------------------------------------------------------------------
//   A1  RANGE: the class's base (2 tiles) plus its gun's tiles; the three kit traits
//       (self-propelled, counter-battery radar, precision shells) read off the kit.
//   A2  BOMBARDMENT: a standing battery strikes a hostile unit within its range from where
//       it stands, without entering its province; nothing past its range, nothing of a
//       nation it has not declared hostile, nothing while it marches or fights.
//   A3  COUNTER-BATTERY: with the radar it answers hostile artillery in range first.
//   A4  PRECISION: precision shells land more of each strike.
//   A5  DETERMINISM: the same days twice end byte-identical.
//   A6  FRONT PLACEMENT: a front puts its battery as deep behind the line as its range
//       allows while still reaching across; the line's slots are spaced among the line.
//   A7  THE LINE MOVES: the battery moves with it.
//   A8  THE SCORER'S READ: front_hold_slot gives a battery its battery tile, as the step does.
//   A9  SELF-PROPELLED PACE: an SP battery on a front marches at the force's pace.
//   A10 THE WAKE TEST: a day with a target in range is a military day.
//
// Run: node tools/verify/build_harness.js artillery_harness --run
// ---------------------------------------------------------------------------

#include "world/artillery.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/fronts.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/unit_designs.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cstdio>
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

recipe_registry make_registry()
{
    recipe_registry reg;
    military_capability_params mp = reg.military();
    for (int c = 0; c < land_class_count; ++c)
        mp.march_days_per_land_class[static_cast<std::size_t>(c)] = 1.0f;
    mp.march_days_per_land_class[static_cast<std::size_t>(land_class::artillery)]  = 2.0f;
    mp.march_days_per_land_class[static_cast<std::size_t>(land_class::mechanised)] = 0.5f;
    for (int k = 0; k < unit_class_count; ++k)
        mp.march_days_per_cls[static_cast<std::size_t>(k)] = 1.0f;
    reg.set_military(mp);
    return reg;
}

// A 20 x 10 plains cylinder. A holds rows 0-4, B rows 5-9; every tile its own province.
constexpr int W = 20, H = 10;

struct fixture
{
    world w;
    entity_id body = null_entity, A = null_entity, B = null_entity;
    std::vector<entity_id> cell;
    entity_id at(int x, int y) const { return cell[static_cast<std::size_t>(y * W + x)]; }
};

void build(fixture& f)
{
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Range"; bc.type = body_type::planet; bc.grid_width = W; bc.grid_height = H;
    w.bodies[f.body] = bc;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body = f.body; tc.grid_x = x; tc.grid_y = y;
            tc.substrate = terrain_substrate::rocky; tc.landform = terrain_landform::plains;
            w.tiles[t] = tc;
            f.cell.push_back(t);
        }
    const auto nation = [&](const char* name) {
        const entity_id n = w.create_entity();
        nation_component nc{};
        nc.name = name; nc.treasury = 1.0e6f;
        w.nations[n] = nc;
        return n;
    };
    f.A = nation("Aster");
    f.B = nation("Brael");
    w.player_entity = f.A;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            w.set_tile_nation(f.at(x, y), y < 5 ? f.A : f.B);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            province p; p.id = f.at(x, y); p.body = f.body; p.tiles = { f.at(x, y) };
            w.provinces.provinces.push_back(p);
            w.provinces.tile_province[f.at(x, y)] = p.id;
        }
}

entity_id add_unit(fixture& f, entity_id owner, int x, int y, land_class lc, int men = 500)
{
    const entity_id id = f.w.create_entity();
    unit_component u{};
    u.owner = owner; u.position = f.at(x, y); u.count = men; u.establishment = men;
    u.type = static_cast<uint16_t>(std::max(0, modern_row_for(lc)));
    f.w.units[id] = u;
    return id;
}

/// Fit catalogue item @p id into its artillery slot on unit @p u.
void fit(unit_component& u, const char* id)
{
    const uint16_t item = kit_item_by_id(id);
    const kit_item& it = kit_catalogue().at(item);
    u.kit.fit[it.slot] = kit_variant_of(item);
}

int hex(const world& w, entity_id a, entity_id b)
{
    const tile_component& ta = w.tiles.at(a);
    const tile_component& tb = w.tiles.at(b);
    const auto q = [](int x, int y) { return x - (y - (y & 1)) / 2; };
    int best = 1 << 30;
    for (int k = -1; k <= 1; ++k)
    {
        const int dq = q(tb.grid_x + k * W, tb.grid_y) - q(ta.grid_x, ta.grid_y);
        const int dr = tb.grid_y - ta.grid_y;
        best = std::min(best, (std::abs(dq) + std::abs(dr) + std::abs(dq + dr)) / 2);
    }
    return best;
}

int damage(const world& w, entity_id u, int men0)
{
    const unit_component& x = w.units.at(u);
    return (men0 - x.count) * 1000 + (1000 - x.readiness_permille);
}

const front* front_toward(const std::vector<front>& fs, entity_id n)
{
    for (const front& f : fs)
        if (f.neighbour == n) return &f;
    return nullptr;
}

corp_command_result assign(world& w, const recipe_registry& reg, entity_id nation, entity_id unit, entity_id key)
{
    corp_command c; c.corp = nation; c.as_nation = nation; c.verb = corp_verb::assign_front;
    c.subject = unit; c.tile = key;
    return apply_corp_command(w, reg, c);
}

void days(world& w, const recipe_registry& reg, int n)
{
    for (int i = 0; i < n; ++i)
    {
        advance_fronts(w, reg);
        (void)run_unit_march(w, reg);
        reanchor_fronts(w);
    }
}

/// The depth of @p t behind span @p f: hex steps to its nearest span tile.
int depth(const world& w, const front& f, entity_id t)
{
    int d = 1 << 30;
    for (const entity_id s : f.span)
        d = std::min(d, hex(w, t, s));
    return d;
}

} // namespace

int main()
{
    std::printf("=== artillery (BL-1643) ===\n");
    const recipe_registry reg = make_registry();
    const std::set<entity_id> none;

    // =====================================================================
    std::printf("\nA1 - range and traits from the kit\n");
    {
        fixture f; build(f);
        const entity_id a = add_unit(f, f.A, 5, 2, land_class::artillery);
        const entity_id i = add_unit(f, f.A, 6, 2, land_class::infantry);
        unit_component& u = f.w.units.at(a);
        check(artillery_range_tiles(reg, u) == 2 && artillery_traits(u) == 0 && artillery_range_tiles(reg, f.w.units.at(i)) == 0,
              "A1a base artillery reaches 2 tiles with no trait; infantry has no range");
        fit(u, "art.gun.long_barrel");
        const int r_lb = artillery_range_tiles(reg, u);
        fit(u, "art.gun.guided_rockets");
        const int r_gr = artillery_range_tiles(reg, u);
        fit(u, "art.gun.radar_laid");
        const int r_rl = artillery_range_tiles(reg, u);
        check(r_lb == 3 && r_gr == 4 && r_rl == 3 && (artillery_traits(u) & k_art_counter_battery) != 0,
              "A1b long barrel 3, guided rockets 4, radar-laid 3 with counter-battery radar ("
                  + std::to_string(r_lb) + ", " + std::to_string(r_gr) + ", " + std::to_string(r_rl) + ")");
        fit(u, "art.munitions.guided_shells");
        fit(u, "art.tow.self_propelled");
        check((artillery_traits(u) & k_art_precision) != 0 && (artillery_traits(u) & k_art_keeps_pace) != 0,
              "A1c guided shells are precision shells; the self-propelled chassis keeps pace");
        check(kit_catalogue_first_error() == nullptr, "A1d the catalogue is well-formed with the radar-laid howitzer appended");
    }

    // =====================================================================
    std::printf("\nA2 - a standing battery shells a hostile unit in range, from where it stands\n");
    {
        fixture f; build(f);
        const entity_id a = add_unit(f, f.A, 5, 3, land_class::artillery);
        const entity_id near = add_unit(f, f.B, 5, 5, land_class::infantry); // 2 tiles
        const entity_id far  = add_unit(f, f.B, 5, 8, land_class::infantry); // 5 tiles
        const entity_id pos0 = f.w.units.at(a).position;
        // Not hostile: nothing.
        artillery_day_tick t0{};
        for (int d = 0; d < 10; ++d) { const artillery_day_tick t = run_artillery_day(f.w, reg, none); t0.strikes += t.strikes; }
        check(t0.strikes == 0 && damage(f.w, near, 500) == 0,
              "A2a with no declared hostility the battery fires nothing");
        f.w.corp_hostile_pairs.insert({f.A, f.B});
        check(hex(f.w, f.w.units.at(a).position, f.w.units.at(near).position) == 2
                  && artillery_target(f.w, reg, a, none) == near,
              "A2b the target is the hostile unit 2 tiles off, the one 5 tiles off is out of range");
        int strikes = 0, killed = 0;
        for (int d = 0; d < 30; ++d)
        {
            const artillery_day_tick t = run_artillery_day(f.w, reg, none);
            strikes += t.strikes; killed += t.men_killed;
        }
        std::printf("      30 days: %d strikes, %d men killed; target %d men, readiness %d\n", strikes, killed,
                    f.w.units.at(near).count, f.w.units.at(near).readiness_permille);
        check(strikes == 30 && killed > 0 && f.w.units.at(near).count == 500 - killed,
              "A2c thirty days, thirty strikes; the near unit loses men");
        check(damage(f.w, far, 500) == 0 && f.w.units.at(a).position == pos0,
              "A2d the far unit is untouched and the battery never moved (it does not enter the province)");
        // Marching: no fire. Fighting: no fire.
        f.w.units.at(a).order.dest = f.at(6, 3);
        check(run_artillery_day(f.w, reg, none).strikes == 0, "A2e a battery under a march order does not fire");
        f.w.units.at(a).order.dest = null_entity;
        check(run_artillery_day(f.w, reg, std::set<entity_id>{ a }).strikes == 0, "A2f a battery in a battle does not bombard");
        f.w.units.at(a).refit_days = 5; f.w.units.at(a).refit_type = f.w.units.at(a).type;
        check(run_artillery_day(f.w, reg, none).strikes == 0, "A2g a refitting battery does not fire");
    }

    // =====================================================================
    std::printf("\nA3 - counter-battery radar answers hostile artillery first\n");
    {
        fixture f; build(f);
        const entity_id a = add_unit(f, f.A, 5, 3, land_class::artillery);
        const entity_id inf = add_unit(f, f.B, 5, 5, land_class::infantry);   // 2 tiles
        const entity_id bart = add_unit(f, f.B, 7, 6, land_class::artillery); // 3 tiles
        f.w.corp_hostile_pairs.insert({f.A, f.B});
        fit(f.w.units.at(a), "art.gun.long_barrel");
        const entity_id plain = artillery_target(f.w, reg, a, none);
        fit(f.w.units.at(a), "art.gun.radar_laid");
        const entity_id radar = artillery_target(f.w, reg, a, none);
        check(hex(f.w, f.w.units.at(a).position, f.w.units.at(bart).position) == 3 && plain == inf && radar == bart,
              "A3a without the radar the nearer infantry; with it the hostile battery 3 tiles off");
    }

    // =====================================================================
    std::printf("\nA4 - precision shells land more of each strike\n");
    {
        int dmg[2] = {0, 0};
        for (int p = 0; p < 2; ++p)
        {
            fixture f; build(f);
            const entity_id a = add_unit(f, f.A, 5, 3, land_class::artillery);
            const entity_id t = add_unit(f, f.B, 5, 5, land_class::infantry);
            f.w.corp_hostile_pairs.insert({f.A, f.B});
            if (p == 1)
                fit(f.w.units.at(a), "art.munitions.guided_shells");
            for (int d = 0; d < 20; ++d)
                run_artillery_day(f.w, reg, none);
            dmg[p] = damage(f.w, t, 500);
        }
        std::printf("      20 days' damage (men x 1000 + readiness): shells %d, precision %d\n", dmg[0], dmg[1]);
        check(dmg[1] > dmg[0] && dmg[0] > 0, "A4a precision shells do more than shells");
    }

    // =====================================================================
    std::printf("\nA5 - determinism\n");
    {
        std::string out[2];
        for (int i = 0; i < 2; ++i)
        {
            fixture f; build(f);
            add_unit(f, f.A, 5, 3, land_class::artillery);
            add_unit(f, f.A, 9, 4, land_class::artillery);
            add_unit(f, f.B, 5, 5, land_class::infantry);
            add_unit(f, f.B, 9, 6, land_class::artillery);
            f.w.corp_hostile_pairs.insert({f.A, f.B});
            f.w.corp_hostile_pairs.insert({f.B, f.A});
            for (int d = 0; d < 15; ++d)
                run_artillery_day(f.w, reg, none);
            out[i] = to_bytes(f.w);
        }
        check(out[0] == out[1], "A5a two runs of fifteen days end byte-identical");
    }

    // =====================================================================
    std::printf("\nA6..A8 - the front places its battery at its range behind the line\n");
    {
        fixture f; build(f);
        const entity_id i1 = add_unit(f, f.A, 2, 1, land_class::infantry);
        const entity_id i2 = add_unit(f, f.A, 12, 1, land_class::infantry);
        const entity_id b2 = add_unit(f, f.A, 6, 0, land_class::artillery);  // range 2
        const entity_id b3 = add_unit(f, f.A, 14, 0, land_class::artillery); // range 3
        fit(f.w.units.at(b3), "art.gun.long_barrel");
        reanchor_fronts(f.w);
        const std::vector<front> fs = current_fronts(f.w, f.A);
        const front* fr = front_toward(fs, f.B);
        check(fr != nullptr, "A6 setup: A has a front toward B");
        if (fr != nullptr)
        {
            const entity_id key = fr->key;
            for (const entity_id u : { i1, i2, b2, b3 })
                assign(f.w, reg, f.A, u, key);
            days(f.w, reg, 40);
            const std::vector<front> fs2 = current_fronts(f.w, f.A);
            const front* g = front_toward(fs2, f.B);
            const entity_id p2 = f.w.units.at(b2).position, p3 = f.w.units.at(b3).position;
            const tile_component& t2 = f.w.tiles.at(p2);
            const tile_component& t3 = f.w.tiles.at(p3);
            std::printf("      battery r2 at (%d,%d) depth %d; battery r3 at (%d,%d) depth %d\n", t2.grid_x, t2.grid_y,
                        depth(f.w, *g, p2), t3.grid_x, t3.grid_y, depth(f.w, *g, p3));
            check(depth(f.w, *g, p2) == 1 && depth(f.w, *g, p3) == 2,
                  "A6a the range-2 battery stands 1 behind the line, the range-3 battery 2 behind");
            check(f.w.tile_controller(p2) == f.A && f.w.tile_controller(p3) == f.A,
                  "A6b both stand on their own nation's ground");
            int reach2 = 1 << 30, reach3 = 1 << 30;
            for (int x = 0; x < W; ++x)
            {
                reach2 = std::min(reach2, hex(f.w, p2, f.at(x, 5)));
                reach3 = std::min(reach3, hex(f.w, p3, f.at(x, 5)));
            }
            check(reach2 <= 2 && reach3 <= 3, "A6c each still reaches across the line onto B's ground");
            const entity_id l1 = f.w.units.at(i1).position, l2 = f.w.units.at(i2).position;
            const std::size_t len = g->span.size();
            check(l1 == g->span[len / 4] && l2 == g->span[(3 * len) / 4],
                  "A6d the two infantry hold the line's two slots, spaced among the line alone");
            // A8: the scorer's read agrees with the step.
            std::vector<entity_id> members = { i1, i2, b2, b3 };
            std::sort(members.begin(), members.end());
            bool agree = true;
            for (std::size_t m = 0; m < members.size(); ++m)
                agree = agree && front_hold_slot(f.w, reg, *g, members, m) == f.w.units.at(members[m]).position;
            check(agree, "A8a front_hold_slot gives every member, battery or line, the tile the step put it on");

            // A7: the line moves - A takes B's row 5 - and the battery follows.
            for (int x = 0; x < W; ++x)
                f.w.set_tile_nation(f.at(x, 5), f.A);
            reanchor_fronts(f.w);
            days(f.w, reg, 40);
            const std::vector<front> fs3 = current_fronts(f.w, f.A);
            const front* h = front_toward(fs3, f.B);
            const entity_id q2 = f.w.units.at(b2).position;
            check(h != nullptr && depth(f.w, *h, q2) == 1 && f.w.tiles.at(q2).grid_y == 4,
                  "A7a with the line one row south, the range-2 battery moved to stand 1 behind the new line");
        }
    }

    // =====================================================================
    std::printf("\nA9 - a self-propelled battery keeps the pace of the force it supports\n");
    {
        fixture f; build(f);
        const entity_id m = add_unit(f, f.A, 2, 1, land_class::mechanised);
        const entity_id b = add_unit(f, f.A, 6, 0, land_class::artillery);
        reanchor_fronts(f.w);
        const front* fr = front_toward(current_fronts(f.w, f.A), f.B);
        if (fr != nullptr)
        {
            const entity_id key = fr->key;
            assign(f.w, reg, f.A, m, key);
            assign(f.w, reg, f.A, b, key);
        }
        const float base = 1.0f / 2.0f;
        const float towed = artillery_march_rate(f.w, reg, f.w.units.at(b), base);
        fit(f.w.units.at(b), "art.tow.self_propelled");
        const float sp = artillery_march_rate(f.w, reg, f.w.units.at(b), base);
        f.w.units.at(b).front = null_entity;
        const float sp_alone = artillery_march_rate(f.w, reg, f.w.units.at(b), base);
        std::printf("      rate a day: towed %.2f, self-propelled on the front %.2f, off it %.2f\n", towed, sp, sp_alone);
        check(towed == base && sp == 2.0f && sp_alone == base,
              "A9a towed marches at its class's pace; self-propelled at the mechanised force's on its front");
    }

    // =====================================================================
    std::printf("\nA10 - a battery with a target keeps the military day walking\n");
    {
        fixture f; build(f);
        const entity_id a = add_unit(f, f.A, 5, 3, land_class::artillery);
        (void)a;
        add_unit(f, f.B, 5, 5, land_class::infantry);
        check(!artillery_fire_pending(f.w, reg), "A10a with no hostility nothing is pending");
        f.w.corp_hostile_pairs.insert({f.A, f.B});
        check(artillery_fire_pending(f.w, reg), "A10b with a hostile unit in range a strike is pending");
    }

    std::printf("\nRESULT: %d failures / %d checks\n", failures, checks);
    return failures == 0 ? 0 : 1;
}
