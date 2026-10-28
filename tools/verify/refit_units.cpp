// ---------------------------------------------------------------------------
// Refit to a newer type (Ben, 2026-10-27). No SDL/Lua.
// ---------------------------------------------------------------------------
// MILITARY.md § The muster interface, "Refit to a newer type": a nation refits
// one of its own units, on its own ground, to a newer model of the same land
// class it fields now. Paid now (the price difference plus the target's gated
// goods); takes the target's muster days; the unit keeps its old type, and fights
// as it, until then, and may not march.
//
//   R1  A COMPATIBLE REFIT APPLIES: it charges the price difference (and, for a
//       gated target, the goods off the shelf), keeps the type, records the target
//       and the target's muster days, and clears a standing march order.
//   R2  IT COMPLETES ON ITS DAYS: the type changes when the days pass (one tick for
//       90 days, two for 135), keeping men, establishment, readiness and morale.
//   R3  REFUSALS MUTATE NOTHING: an incompatible target (another class, the same
//       row, an older row, no such row), a foreign unit, a unit in a battle, a unit
//       off its own ground, an unaffordable refit (credits, or goods) and a second
//       refit are each refused with the world byte-identical.
//   R4  A REFITTING UNIT CANNOT MARCH (rejected_state).
//   R5  THE REFIT SURVIVES A SAVE by value; a v60 stream reads as no refit, and a
//       v60 / v59 stream with units keeps their combat state.
//   R6  THE HASH: a world where nothing refits is untouched by the refit pass; a
//       refit moves the hash; twins hash alike.
//
// Run: refit_units
// ---------------------------------------------------------------------------

#include "world/battle_system.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

namespace {

int failures = 0, checks = 0;

void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

std::string to_bytes(const world& w)
{
    std::ostringstream o(std::ios::binary);
    write_world_snapshot(w, o);
    return o.str();
}

int row_named(const char* name)
{
    const auto& t = unit_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
        if (std::string(t[i].name) == name) return static_cast<int>(i);
    return -1;
}

struct fixture
{
    world     w;
    entity_id body   = null_entity;
    entity_id home   = null_entity; ///< the nation's own tile, with its base
    entity_id abroad = null_entity; ///< a tile the other nation owns
    entity_id nation = null_entity;
    entity_id other  = null_entity;
    entity_id base   = null_entity;
    entity_id market = null_entity;
};

/// One body, two land tiles: `home` the nation's (with a completed military base),
/// `abroad` the other nation's. A market on `home` holds @p shelf of steel and coal.
void build(fixture& f, float treasury = 100000.0f, float shelf = 0.0f)
{
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Home"; bc.type = body_type::planet; bc.grid_width = 2; bc.grid_height = 1;
    w.bodies[f.body] = bc;
    for (int x = 0; x < 2; ++x)
    {
        const entity_id t = w.create_entity();
        tile_component tc{};
        tc.body = f.body; tc.grid_x = x; tc.grid_y = 0;
        tc.substrate = terrain_substrate::rocky; tc.landform = terrain_landform::plains;
        w.tiles[t] = tc;
        (x == 0 ? f.home : f.abroad) = t;
    }

    f.nation = w.create_entity();
    nation_component nc{};
    nc.name = "Hosts"; nc.treasury = treasury;
    f.base = w.create_entity();
    building_component b{};
    b.tile = f.home; b.type = building_type::military_base; b.ticks_remaining = 0;
    w.buildings[f.base] = b;
    nc.state_assets.push_back(f.base);
    w.nations[f.nation] = nc;

    f.other = w.create_entity();
    nation_component oc{};
    oc.name = "Others"; oc.treasury = treasury;
    w.nations[f.other] = oc;
    w.player_entity = f.nation;

    w.set_tile_nation(f.home, f.nation);
    w.set_tile_nation(f.abroad, f.other);

    if (shelf > 0.0f)
    {
        f.market = w.create_entity();
        market_component mc{};
        mc.body = f.body; mc.centre_tile = f.home;
        for (const resource_type r : { resource_type::steel, resource_type::coal })
        {
            const std::size_t i = static_cast<std::size_t>(r);
            mc.base_price[i] = 10.0f; mc.price[i] = 10.0f; mc.inventory[i] = shelf;
        }
        w.markets[f.market] = mc;
    }
}

entity_id add_unit(fixture& f, int row, entity_id owner, entity_id tile)
{
    const entity_id u = f.w.create_entity();
    unit_component uc{};
    uc.position = tile; uc.owner = owner; uc.count = 37; uc.type = static_cast<uint16_t>(row);
    uc.muster_base = f.base; uc.establishment = 50; uc.readiness_permille = 600; uc.morale_permille = 700;
    f.w.units[u] = uc;
    return u;
}

corp_command_result refit(fixture& f, const recipe_registry& reg, entity_id actor, entity_id unit, int row)
{
    corp_command cmd;
    cmd.tick = 1; cmd.corp = actor; cmd.verb = corp_verb::refit_unit;
    cmd.subject = unit; cmd.unit_type = static_cast<uint16_t>(row);
    return apply_corp_command(f.w, reg, cmd);
}

/// Refused, and the world is byte- and hash-identical to before.
void refused(fixture& f, const recipe_registry& reg, entity_id actor, entity_id unit, int row,
             corp_command_result want, const std::string& what)
{
    const std::string before = to_bytes(f.w);
    const uint64_t    h      = f.w.state_hash(1);
    const auto r = refit(f, reg, actor, unit, row);
    check(r == want && to_bytes(f.w) == before && f.w.state_hash(1) == h, what);
}

} // namespace

int main()
{
    std::printf("=== refit to a newer type (2026-10-27) ===\n");
    const recipe_registry reg{};
    const float per_power = reg.military().hire_cost_per_power;
    const int levy   = row_named("Levy Spear");
    const int rifle  = row_named("Rifle Infantry");
    const int breech = row_named("Breech Artillery");
    const int gun    = row_named("Field Gun Battery");
    const int armour = row_named("Medium Armour");
    check(levy >= 0 && rifle >= 0 && breech >= 0 && gun >= 0 && armour >= 0, "R0 the fixture's rows exist");
    const auto& t = unit_roster_table();

    std::printf("\nR1/R2 - an ungated refit: Levy Spear to Rifle Infantry\n");
    {
        fixture f;
        build(f);
        const entity_id u = add_unit(f, levy, f.nation, f.home);
        f.w.units.at(u).order.dest = f.abroad; // a standing march order
        const float before = f.w.nations.at(f.nation).treasury;
        const auto r = refit(f, reg, f.nation, u, rifle);
        const unit_component& uc = f.w.units.at(u);
        const float paid = before - f.w.nations.at(f.nation).treasury;
        const float want = per_power * static_cast<float>(t[rifle].power_mod - t[levy].power_mod);
        check(r == corp_command_result::applied, "R1a the refit applies");
        check(near(paid, want) && want > 0.0f,
              "R1b it charges the price difference (" + std::to_string(paid) + " cr = "
                  + std::to_string(want) + ")");
        check(uc.type == levy && uc.refit_type == rifle && uc.refit_days == muster_days_for(static_cast<uint16_t>(rifle))
                  && uc.refit_days == 90,
              "R1c the unit keeps its type, the target and its 90 days recorded");
        check(uc.order.dest == null_entity, "R1d a standing march order is cleared");
        refused(f, reg, f.nation, u, rifle, corp_command_result::rejected_state,
                "R3a a second refit while refitting is refused, nothing changed");

        // R4: a refitting unit cannot march (refused before the destination is read).
        corp_command m;
        m.tick = 1; m.corp = f.nation; m.verb = corp_verb::march_unit; m.subject = u;
        const std::string pre = to_bytes(f.w);
        check(apply_corp_command(f.w, reg, m) == corp_command_result::rejected_state && to_bytes(f.w) == pre,
              "R4a a refitting unit's march is refused rejected_state");
        fixture g;
        build(g);
        const entity_id v = add_unit(g, levy, g.nation, g.home);
        m.corp = g.nation; m.subject = v;
        check(apply_corp_command(g.w, reg, m) != corp_command_result::rejected_state,
              "R4b the same march from a unit not refitting is not refused for its state");

        advance_refits(f.w);
        const unit_component& done = f.w.units.at(u);
        check(done.type == rifle && done.refit_type == k_no_refit && done.refit_days == 0,
              "R2a one tick later it is Rifle Infantry, the refit cleared");
        check(done.count == 37 && done.establishment == 50 && done.readiness_permille == 600
                  && done.morale_permille == 700 && done.position == f.home && done.owner == f.nation,
              "R2b men, establishment, readiness, morale, place and owner kept");
        refused(f, reg, f.nation, u, levy, corp_command_result::rejected_invalid,
                "R3b an older row is refused, nothing changed");
    }

    std::printf("\nR1/R2 - a gated refit: Breech Artillery to Field Gun Battery\n");
    {
        fixture f;
        build(f, 100000.0f, /*shelf=*/50.0f);
        const entity_id u = add_unit(f, breech, f.nation, f.home);
        const float before = f.w.nations.at(f.nation).treasury;
        const auto r = refit(f, reg, f.nation, u, gun);
        const float paid = before - f.w.nations.at(f.nation).treasury;
        const float credits = per_power * static_cast<float>(t[gun].power_mod - t[breech].power_mod);
        const market_component& mc = f.w.markets.at(f.market);
        const float steel = mc.inventory[static_cast<std::size_t>(resource_type::steel)];
        const float coal  = mc.inventory[static_cast<std::size_t>(resource_type::coal)];
        check(r == corp_command_result::applied, "R1e the gated refit applies");
        check(near(steel, 50.0f - hire_axis_cost) && near(coal, 50.0f - hire_axis_cost),
              "R1f it buys the target's goods off the shelf (5 steel, 5 coal)");
        check(near(paid, credits + 2.0f * hire_axis_cost * 10.0f),
              "R1g and pays the difference plus the goods (" + std::to_string(paid) + " cr)");
        check(f.w.units.at(u).refit_days == 135, "R1h artillery refits in 135 days");
        advance_refits(f.w);
        check(f.w.units.at(u).type == breech && f.w.units.at(u).refit_days == 45,
              "R2c after one tick it is still Breech Artillery, 45 days left");
        advance_refits(f.w);
        check(f.w.units.at(u).type == gun && !unit_refitting(f.w.units.at(u)),
              "R2d after two it is a Field Gun Battery");
    }

    std::printf("\nR3 - refusals mutate nothing\n");
    {
        fixture f;
        build(f);
        const entity_id u = add_unit(f, levy, f.nation, f.home);
        refused(f, reg, f.nation, u, armour, corp_command_result::rejected_invalid,
                "R3c another land class (Medium Armour) is refused");
        refused(f, reg, f.nation, u, levy, corp_command_result::rejected_invalid,
                "R3d the unit's own row is refused");
        refused(f, reg, f.nation, u, static_cast<int>(t.size()), corp_command_result::rejected_invalid,
                "R3e no such row is refused");
        refused(f, reg, f.nation, u, gun, corp_command_result::rejected_invalid,
                "R3f an artillery row for infantry is refused");

        const entity_id theirs = add_unit(f, levy, f.other, f.abroad);
        refused(f, reg, f.nation, theirs, rifle, corp_command_result::rejected_not_owner,
                "R3g another nation's unit is refused rejected_not_owner");
        refused(f, reg, f.nation, null_entity, rifle, corp_command_result::rejected_invalid,
                "R3h no unit is refused");

        const entity_id away = add_unit(f, levy, f.nation, f.abroad);
        refused(f, reg, f.nation, away, rifle, corp_command_result::rejected_placement,
                "R3i a unit on foreign ground is refused rejected_placement");

        active_battle b;
        b.attacker = f.nation; b.defender = f.other; b.tile = f.abroad;
        b.attacker_units = { u };
        f.w.battles.push_back(b);
        refused(f, reg, f.nation, u, rifle, corp_command_result::rejected_state,
                "R3j a unit in a battle is refused rejected_state");
        f.w.battles.clear();

        fixture g;
        build(g, /*treasury=*/100.0f); // the difference is 220 cr
        const entity_id p = add_unit(g, levy, g.nation, g.home);
        refused(g, reg, g.nation, p, rifle, corp_command_result::rejected_funds,
                "R3k a refit the treasury cannot pay is refused rejected_funds");

        fixture h;
        build(h, 100000.0f, /*shelf=*/2.0f); // the gate opens (stock > 0); 5 is needed
        const entity_id q = add_unit(h, breech, h.nation, h.home);
        refused(h, reg, h.nation, q, gun, corp_command_result::rejected_funds,
                "R3l a refit whose goods the shelf cannot supply is refused rejected_funds");

        fixture k;
        build(k); // no shelf: the gated row is not fielded at all
        const entity_id z = add_unit(k, breech, k.nation, k.home);
        refused(k, reg, k.nation, z, gun, corp_command_result::rejected_invalid,
                "R3m a row the nation does not field now is refused rejected_invalid");
    }

    std::printf("\nR5 - the refit survives a save\n");
    {
        fixture f;
        build(f);
        const entity_id a = add_unit(f, levy, f.nation, f.home);
        const entity_id b = add_unit(f, levy, f.nation, f.home);
        refit(f, reg, f.nation, b, rifle);
        f.w.units.at(b).refit_days = 37;
        const std::string bytes = to_bytes(f.w);
        world back;
        std::istringstream in(bytes, std::ios::binary);
        const bool ok = read_world_snapshot(back, in);
        check(ok && back.units.at(b).refit_type == rifle && back.units.at(b).refit_days == 37
                  && !unit_refitting(back.units.at(a)) && back.state_hash(4) == f.w.state_hash(4)
                  && to_bytes(back) == bytes,
              "R5a the refit round-trips by value (hash and bytes equal)");

        std::string v60;
        {
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_pre_refit(f.w, o);
            v60 = o.str();
        }
        world old;
        std::istringstream in60(v60, std::ios::binary);
        const bool ok60 = read_world_snapshot(old, in60);
        // BL-1427 (v62): today's stream carries the fronts section, two empty counts
        // here (8 bytes) after the refits; BL-1443 (v63) ends it with the attacking-stack
        // section, an empty battle count (4 bytes).
        // BL-1434 (v64): then the designs section, three empty counts (12 bytes);
        // BL-1445 (v65): then the march-grant section, an empty count (4 bytes).
        constexpr std::size_t fronts_tail = 8 + 4 + 12 + 4;
        check(ok60 && v60.size() == bytes.size() - (4 + 10) - fronts_tail && !unit_refitting(old.units.at(b))
                  && old.units.at(b).type == levy,
              "R5b a v60 stream (14 bytes shorter) reads as no refit");
        check(ok60 && old.units.at(a).establishment == 50 && old.units.at(a).readiness_permille == 600
                  && old.units.at(a).morale_permille == 700,
              "R5c and its units keep their combat state");
        std::string v59;
        {
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_pre_obsolete(f.w, o);
            v59 = o.str();
        }
        world older;
        std::istringstream in59(v59, std::ios::binary);
        check(read_world_snapshot(older, in59) && older.units.at(a).readiness_permille == 600
                  && older.units.size() == 2,
              "R5d a v59 stream with units reads, combat state kept");

        std::string bad = bytes;
        const int16_t no_row = 0x7FFF;
        std::memcpy(&bad[bad.size() - fronts_tail - 6], &no_row, sizeof no_row); // the row's target
        world sentinel;
        std::istringstream inb(bad, std::ios::binary);
        check(!read_world_snapshot(sentinel, inb) && sentinel.units.empty(),
              "R5e a refit naming no real row is REFUSED, destination untouched");
    }

    std::printf("\nR6 - the hash\n");
    {
        fixture a, b, c;
        build(a); build(b); build(c);
        const entity_id ua = add_unit(a, levy, a.nation, a.home);
        const entity_id ub = add_unit(b, levy, b.nation, b.home);
        add_unit(c, levy, c.nation, c.home);
        const uint64_t idle = c.w.state_hash(0);
        advance_refits(c.w);
        check(c.w.state_hash(0) == idle, "R6a the refit pass leaves a world nobody refits untouched");
        refit(a, reg, a.nation, ua, rifle);
        refit(b, reg, b.nation, ub, rifle);
        check(a.w.state_hash(1) == b.w.state_hash(1), "R6b twin refits hash alike");
        // Same world with the refit undone by hand (treasury restored): the refit fields alone move the hash.
        fixture d;
        build(d);
        const entity_id ud = add_unit(d, levy, d.nation, d.home);
        refit(d, reg, d.nation, ud, rifle);
        const uint64_t with = d.w.state_hash(2);
        d.w.units.at(ud).refit_type = k_no_refit;
        d.w.units.at(ud).refit_days = 0;
        check(d.w.state_hash(2) != with, "R6c a refitting unit moves the hash");
    }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    std::printf("%s\n", failures == 0 ? "ALL PASS (0 failures)" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
