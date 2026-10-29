// carried_supplies_harness.cpp — BL-1452 (carried supplies; MILITARY.md § Carried
// supplies, Ben 2026-10-29).
//
// A unit carries food, ammunition and equipment, each per-mille of full, drained daily
// and refilled at its supply factor's share of each stock's daily need. Measured against
// the AUTHORED rates (scripts/economy.lua § economy.military.carried_supplies):
//
//   S1  full cover stays full — in camp for a quarter, and fighting;
//   S2  half cover drains more slowly than its need: share x (need + restock), the
//       restock half a day's full rate (Ben, 2026-10-29);
//   S3  the hunger step: at 999 food the attack and readiness recovery take a flat -15%;
//   S4  starvation kills a unit in exactly 180 days at zero food — alive on day 179,
//       gone from the world by day 181 — and returning food stops the loss;
//   S5  a starving unit's readiness is capped at its men over establishment;
//   S6  ammunition is floored at 20%; equipment is not: at 0 equipment a unit lands no hits;
//   S7  the save round-trips the stocks and the carry; a v64 stream loads full; a
//       corrupt record is refused whole;
//   S8  determinism: the same fixture twice is byte-identical;
//   S9  the loader rejects a rate that is not an integer per-mille;
//   S10 the remainder carry: cover at or past break-even never drains a full stock,
//       a fractional gain accrues exactly, and an exact break-even holds for years;
//   S11 the quiet world: a fed world at peace costs no daily churn.
//
// Build: bash tools/verify/build_lua_harness.sh carried_supplies_harness. Run from the
// repo root.

#include "world/battle_system.hpp"
#include "world/campaign_battle.hpp"
#include "world/carried_supplies.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include "scripting/lua_state.hpp"

#include <algorithm>
#include <cstdio>
#include <set>
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

entity_id make_row_body(world& w, int gw)
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
        tc.body          = body;
        tc.grid_x        = c;
        tc.grid_y        = 0;
        tc.substrate     = terrain_substrate::sedimentary;
        tc.cover         = terrain_cover::grass;
        tc.cover_density = 150;
        tc.landform      = terrain_landform::plains;
        w.tiles[t]       = tc;
    }
    return body;
}

entity_id tile_at(world& w, entity_id body, int c)
{
    return body_tile_grid(w, body)[static_cast<std::size_t>(c)];
}

entity_id add_nation(world& w, entity_id body, int from, int to)
{
    const entity_id n = w.create_entity();
    nation_component nc{};
    nc.name     = "Fixture";
    nc.treasury = 100000.0f;
    for (int c = from; c <= to; ++c)
    {
        const entity_id t = tile_at(w, body, c);
        nc.tiles.push_back(t);
        w.tile_to_nation[t] = n;
    }
    w.nations[n] = nc;
    return n;
}

entity_id add_unit(world& w, entity_id owner, entity_id tile, uint16_t type, int count)
{
    const entity_id u = w.create_entity();
    unit_component uc{};
    uc.owner         = owner;
    uc.position      = tile;
    uc.count         = count;
    uc.establishment = count;
    uc.type          = type;
    w.units[u]       = uc;
    return u;
}

/// The day's net change of a stock in millionths of full: share x (spend + restock) - spend.
int64_t net_u(const carried_supply_params& p, int spend, int full_rate, int share)
{
    return static_cast<int64_t>(share) * (static_cast<int64_t>(spend) * 1000
                                          + static_cast<int64_t>(full_rate) * p.restock_permille)
         - static_cast<int64_t>(spend) * k_supply_remainder_unit;
}

/// A stock after @p days from @p from at a constant net (clamped), per-mille.
int after_days(int from, int64_t net, int days)
{
    const int64_t v = std::clamp<int64_t>(static_cast<int64_t>(from) * k_supply_remainder_unit
                                              + net * days, 0, 1000LL * k_supply_remainder_unit);
    return static_cast<int>(v / k_supply_remainder_unit);
}

uint16_t infantry() { return static_cast<uint16_t>(modern_row_for(land_class::infantry)); }

/// A lone unit in camp on its own ground, at @p supply.
struct camp
{
    world     w;
    entity_id u = null_entity;
};

void build_camp(camp& c, int supply, int men = 1000)
{
    const entity_id body = make_row_body(c.w, 6);
    const entity_id n    = add_nation(c.w, body, 0, 5);
    c.u = add_unit(c.w, n, tile_at(c.w, body, 2), infantry(), men);
    c.w.units.at(c.u).supply_factor_permille = supply;
}

/// Two hostile nations, the attacker's stack adjacent to the defender's and ordered onto it.
struct duel
{
    world     w;
    entity_id ua = null_entity, ud = null_entity;
};

void build_duel(duel& d, int att_men, int def_men)
{
    const entity_id body = make_row_body(d.w, 8);
    const entity_id att  = add_nation(d.w, body, 0, 0);
    const entity_id def  = add_nation(d.w, body, 1, 7);
    d.w.provinces.seed = 0x1452u;
    build_province_partition(d.w, 0x1452u);
    (void)declare_hostile(d.w, att, def);
    d.ua = add_unit(d.w, att, tile_at(d.w, body, 0), infantry(), att_men);
    d.ud = add_unit(d.w, def, tile_at(d.w, body, 1), infantry(), def_men);
    movement_order mo;
    mo.path       = { tile_at(d.w, body, 0), tile_at(d.w, body, 1) };
    mo.dest       = mo.path.back();
    mo.next_index = 1;
    d.w.units.at(d.ua).order = mo;
}

/// The combatant the round builds from @p u (battle_system.cpp `build_combatants`).
land_combatant combatant_of(entity_id id, const unit_component& u)
{
    land_combatant c;
    c.unit      = id;
    c.men       = u.count;
    c.readiness = u.readiness_permille;
    c.morale    = u.morale_permille;
    c.supply    = carried_attack_permille(u);
    c.carried   = carried_combat_permille(u);
    c.stats     = unit_land_stats(u);
    return c;
}

/// The milli-hits an attacker of @p a lands in ONE round against a fresh defender.
int first_round_power(const unit_component& a)
{
    unit_component d = a;
    d.food_permille = d.ammo_permille = d.equipment_permille = 1000;
    std::vector<land_combatant> att = { combatant_of(1, a) };
    std::vector<land_combatant> def = { combatant_of(2, d) };
    campaign_battle_identity id;
    id.attacker = 10; id.defender = 11; id.tile = 12; id.tick = 1;
    campaign_battle_state st = begin_campaign_battle(id, att, def, terrain_substrate::sedimentary,
                                                     terrain_cover::grass, 150,
                                                     terrain_landform::plains);
    (void)step_campaign_battle(st, att, def);
    return st.rounds.empty() ? -1 : st.rounds[0].attacker_power;
}

} // namespace

int main()
{
    std::printf("=== carried supplies (BL-1452; MILITARY.md § Carried supplies) ===\n");

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    const carried_supply_params& sp = reg.military().supplies;
    std::printf("  authored: food %d/day, ammunition %d/battle day, equipment %d/day + %d/battle day, "
                "restock %d permille of the full rate (break-even %.1f permille)\n",
                sp.food_per_day, sp.ammo_per_battle_day, sp.equipment_attrition_per_day,
                sp.equipment_per_battle_day, sp.restock_permille,
                1000.0 * 1000.0 / (1000.0 + sp.restock_permille));
    const carried_supply_params defaults{};
    check(sp.food_per_day == defaults.food_per_day && sp.ammo_per_battle_day == defaults.ammo_per_battle_day
              && sp.equipment_attrition_per_day == defaults.equipment_attrition_per_day
              && sp.equipment_per_battle_day == defaults.equipment_per_battle_day
              && sp.restock_permille == defaults.restock_permille,
          "S0 the C++ defaults match the authored rates");

    // -----------------------------------------------------------------------
    std::printf("\nS1 - full cover stays full\n");
    {
        camp c;
        build_camp(c, 1000);
        (void)advance_quarter_to(c.w, reg, 90, 0);
        const unit_component& u = c.w.units.at(c.u);
        check(u.food_permille == 1000 && u.ammo_permille == 1000 && u.equipment_permille == 1000,
              "S1a a fully supplied unit in camp ends a quarter full");
        check(!carried_supplies_pending(c.w, sp), "S1b and is inert: no day is spent on it");

        duel d;
        build_duel(d, 2000, 2000);
        int fought_days = 0;
        bool full = true;
        for (int day = 1; day <= 6; ++day)
        {
            (void)run_military_day(d.w, reg, day);
            fought_days += d.w.battles.empty() ? 0 : 1;
            for (const entity_id id : { d.ua, d.ud })
                if (d.w.units.count(id) != 0)
                {
                    const unit_component& x = d.w.units.at(id);
                    full = full && x.ammo_permille == 1000 && x.equipment_permille == 1000
                        && x.food_permille == 1000;
                }
        }
        std::printf("      a 2000-a-side fight at full supply: %d days in battle\n", fought_days);
        check(fought_days > 0 && full, "S1c fighting at full cover, every stock stays full");
    }

    // -----------------------------------------------------------------------
    std::printf("\nS2 - half cover drains more slowly than its need\n");
    {
        camp c;
        build_camp(c, 500);
        (void)advance_quarter_to(c.w, reg, 10, 0);
        const unit_component& u = c.w.units.at(c.u);
        const int64_t food_net = net_u(sp, sp.food_per_day, sp.food_per_day, 500);
        std::printf("      after 10 days at 500 supply: food %d (net %.2f a day), ammunition %d, equipment %d\n",
                    u.food_permille, static_cast<double>(food_net) / k_supply_remainder_unit,
                    u.ammo_permille, u.equipment_permille);
        check(food_net < 0 && -food_net < static_cast<int64_t>(sp.food_per_day) * k_supply_remainder_unit / 2
                  && u.food_permille == after_days(1000, food_net, 10),
              "S2a food falls by need - share x (need + restock): less than half the ration a day");
        check(u.ammo_permille == 1000 && u.equipment_permille == 1000,
              "S2b out of battle no ammunition is spent, and half cover more than meets camp wear");

        duel d;
        build_duel(d, 2000, 2000);
        d.w.units.at(d.ua).supply_factor_permille = 500;
        (void)run_military_day(d.w, reg, 1); // the march opens the battle
        (void)run_military_day(d.w, reg, 2); // a day fought
        const unit_component& a = d.w.units.at(d.ua);
        const int64_t ammo_net = net_u(sp, sp.ammo_per_battle_day, sp.ammo_per_battle_day, 500);
        std::printf("      a day fought at 500 supply: ammunition %d, equipment %d\n",
                    a.ammo_permille, a.equipment_permille);
        check(!d.w.battles.empty() && a.ammo_permille == after_days(1000, ammo_net, 1),
              "S2c a day fought at half cover spends a quarter of a battle day's ammunition net");
    }

    // -----------------------------------------------------------------------
    std::printf("\nS3 - the hunger step at 999 food\n");
    {
        unit_component u{};
        u.count = 1000; u.establishment = 1000; u.type = infantry();
        const int full_atk = carried_attack_permille(u);
        const int full_pow = first_round_power(u);
        u.food_permille = 999;
        const int hungry_atk = carried_attack_permille(u);
        const int hungry_pow = first_round_power(u);
        u.food_permille = 1;
        const int low_atk = carried_attack_permille(u);
        std::printf("      attack term: full %d, 999 food %d, 1 food %d; first-round power %d -> %d\n",
                    full_atk, hungry_atk, low_atk, full_pow, hungry_pow);
        check(full_atk == 1000 && hungry_atk == 850 && low_atk == 850,
              "S3a below full food the attack is a flat 850 - a step, not a slope");
        check(full_pow > 0 && hungry_pow < full_pow && hungry_pow >= full_pow * 850 / 1000 - 1000
                  && hungry_pow <= full_pow * 850 / 1000 + 1000,
              "S3b and the round lands ~85% of the hits");

        camp a, b;
        build_camp(a, 1000);
        build_camp(b, 1000);
        a.w.units.at(a.u).readiness_permille = 0;
        b.w.units.at(b.u).readiness_permille = 0;
        b.w.units.at(b.u).food_permille = 999;
        b.w.units.at(b.u).supply_factor_permille = 1000;
        const campaign_battle_params bp{};
        recover_out_of_contact(a.w, bp);
        recover_out_of_contact(b.w, bp);
        const int ra = a.w.units.at(a.u).readiness_permille, rb = b.w.units.at(b.u).readiness_permille;
        std::printf("      one day's readiness recovery: fed %d, hungry %d\n", ra, rb);
        check(ra == bp.readiness_recovery_permille && rb == ra * 850 / 1000,
              "S3c a hungry unit recovers readiness 15% slower");
    }

    // -----------------------------------------------------------------------
    std::printf("\nS4 - starvation: the establishment over 180 days\n");
    {
        bool exact = true;
        for (const int men : { 1000, 997, 180, 7 })
        {
            camp c;
            build_camp(c, 0, men);
            c.w.units.at(c.u).food_permille = 0;
            const std::set<entity_id> none;
            int alive179 = -1, at180 = -1;
            for (int day = 1; day <= 180; ++day)
            {
                (void)run_carried_supplies_day(c.w, sp, none);
                if (day == 179) alive179 = c.w.units.at(c.u).count;
                if (day == 180) at180 = c.w.units.at(c.u).count;
            }
            std::printf("      establishment %4d: %d men on day 179, %d on day 180\n", men, alive179, at180);
            exact = exact && alive179 > 0 && at180 == 0;
        }
        check(exact, "S4a at zero food a unit is alive on day 179 and has no men on day 180");

        camp c;
        build_camp(c, 0, 1000);
        c.w.units.at(c.u).food_permille = 0;
        int gone_day = -1;
        for (int day = 1; day <= 200 && gone_day < 0; ++day)
        {
            (void)run_military_day(c.w, reg, day);
            if (c.w.units.count(c.u) == 0) gone_day = day;
        }
        std::printf("      through the daily pass: removed on day %d\n", gone_day);
        check(gone_day == 180, "S4b the daily pass removes it on day 180 - gone by day 181, not on 179");

        camp f;
        build_camp(f, 0, 1000);
        f.w.units.at(f.u).food_permille = 0;
        for (int day = 1; day <= 30; ++day) (void)run_military_day(f.w, reg, day);
        const int men30 = f.w.units.at(f.u).count;
        f.w.units.at(f.u).supply_factor_permille = 1000; // food returns
        for (int day = 31; day <= 60; ++day) (void)run_military_day(f.w, reg, day);
        const unit_component& fu = f.w.units.at(f.u);
        std::printf("      30 days starving: %d men; 30 more fed: %d men, food %d\n", men30, fu.count, fu.food_permille);
        check(men30 == 1000 - 1000 * 30 / 180 && fu.count == men30 && fu.starvation_carry == 0,
              "S4c returning food stops the loss, and does not return the men");
    }

    // -----------------------------------------------------------------------
    std::printf("\nS5 - a starving unit's readiness is capped at its men over establishment\n");
    {
        // 600 cover, below break-even: food stays at zero, but morale holds at 600, above
        // the HP share, so the ceiling read is starvation's, not morale's.
        camp c;
        build_camp(c, 600, 1000);
        c.w.units.at(c.u).food_permille = 0;
        for (int day = 1; day <= 90; ++day) (void)run_military_day(c.w, reg, day);
        const unit_component& u = c.w.units.at(c.u);
        std::printf("      after 90 days starving: %d of %d men, readiness %d, morale %d\n",
                    u.count, u.establishment, u.readiness_permille, u.morale_permille);
        check(u.count == 500 && u.readiness_permille == 500, "S5a readiness sits at the HP share, 500");
        check(carried_readiness_cap(u) == 500, "S5b the cap reads men over establishment");
    }

    // -----------------------------------------------------------------------
    std::printf("\nS6 - ammunition floored at 20%%, equipment unfloored\n");
    {
        unit_component u{};
        u.count = 1000; u.establishment = 1000; u.type = infantry();
        const int full_pow = first_round_power(u);
        u.ammo_permille = 0;
        const int dry = carried_combat_permille(u);
        const int dry_pow = first_round_power(u);
        u.ammo_permille = 500;
        const int half = carried_combat_permille(u);
        u.ammo_permille = 1000;
        u.equipment_permille = 0;
        const int bare = carried_combat_permille(u);
        const int bare_pow = first_round_power(u);
        u.equipment_permille = 500; u.ammo_permille = 500;
        const int both = carried_combat_permille(u);
        std::printf("      combat term: no ammunition %d, half %d, no equipment %d, half of both %d; "
                    "power full %d, dry %d, bare %d\n", dry, half, bare, both, full_pow, dry_pow, bare_pow);
        check(dry == 200 && dry_pow > 0, "S6a out of ammunition the unit still fights at 20%");
        check(half == 500 && both == 250, "S6b each stock scales in proportion, and the two multiply");
        check(bare == 0 && bare_pow == 0, "S6c at 0 equipment a unit lands no hits at all");

        // And in the world: an attacker with no equipment lands nothing on the defender.
        duel d;
        build_duel(d, 2000, 2000);
        d.w.units.at(d.ua).equipment_permille = 0;
        d.w.units.at(d.ua).supply_factor_permille = 0;
        for (int day = 1; day <= 4; ++day) (void)run_military_day(d.w, reg, day);
        const int def_men = d.w.units.count(d.ud) ? d.w.units.at(d.ud).count : -1;
        std::printf("      a bare attacker after 4 days: defender %d of 2000\n", def_men);
        check(def_men == 2000, "S6d the round reads the stocks: the bare attacker kills no one");
    }

    // -----------------------------------------------------------------------
    std::printf("\nS7 - the save round-trips the stocks\n");
    {
        camp c;
        build_camp(c, 500, 1000); // half cover: food stays at 0, equipment refills with a remainder
        c.w.units.at(c.u).food_permille = 0;
        c.w.units.at(c.u).ammo_permille = 400;
        c.w.units.at(c.u).equipment_permille = 730;
        for (int day = 1; day <= 7; ++day) (void)run_military_day(c.w, reg, day);
        const unit_component& src = c.w.units.at(c.u);
        const std::string bytes = to_bytes(c.w);
        world back;
        const bool ok = from_bytes(bytes, back);
        const bool same = ok && back.units.count(c.u) != 0
            && back.units.at(c.u).food_permille == src.food_permille
            && back.units.at(c.u).ammo_permille == src.ammo_permille
            && back.units.at(c.u).equipment_permille == src.equipment_permille
            && back.units.at(c.u).starvation_carry == src.starvation_carry
            && back.units.at(c.u).equipment_remainder == src.equipment_remainder;
        std::printf("      saved food %d ammunition %d equipment %d (+%d/1e6) carry %d (v%u)\n", src.food_permille,
                    src.ammo_permille, src.equipment_permille, src.equipment_remainder, src.starvation_carry,
                    static_cast<unsigned>(world_save_version));
        check(src.starvation_carry != 0 && src.equipment_remainder != 0 && same,
              "S7a the stocks, their remainders and the starvation carry round-trip");
        check(ok && back.state_hash(7) == c.w.state_hash(7) && to_bytes(back) == bytes,
              "S7b the loaded world hashes and re-saves as the original");
        for (int day = 8; day <= 20; ++day)
        {
            (void)run_military_day(c.w, reg, day);
            (void)run_military_day(back, reg, day);
        }
        check(to_bytes(back) == to_bytes(c.w), "S7c and starves on identically");

        std::ostringstream o(std::ios::binary);
        write_world_snapshot_pre_supplies(c.w, o);
        world old;
        const bool old_ok = from_bytes(o.str(), old);
        const bool old_full = old_ok && old.units.count(c.u) != 0
            && old.units.at(c.u).food_permille == 1000 && old.units.at(c.u).ammo_permille == 1000
            && old.units.at(c.u).equipment_permille == 1000 && old.units.at(c.u).starvation_carry == 0;
        check(old_full, "S7d a v64 stream loads with every unit's stocks full");

        // Corrupt the food of the one record (the section is the stream's tail).
        std::string bad = bytes;
        const std::size_t food_at = bad.size() - 28; // (id, food, ammo, equip, carry, 3 remainders)
        bad[food_at] = static_cast<char>(0xE9); bad[food_at + 1] = static_cast<char>(0x03); // 1001
        world untouched;
        const bool refused = !from_bytes(bad, untouched) && untouched.units.empty();
        check(refused, "S7e a stock past 1000 is refused whole, and the destination is untouched");
    }

    // -----------------------------------------------------------------------
    std::printf("\nS8 - determinism\n");
    {
        duel a, b;
        build_duel(a, 2000, 1500);
        build_duel(b, 2000, 1500);
        for (duel* d : { &a, &b })
        {
            d->w.units.at(d->ua).supply_factor_permille = 300;
            d->w.units.at(d->ud).supply_factor_permille = 700;
            for (int day = 1; day <= 40; ++day) (void)run_military_day(d->w, reg, day);
        }
        check(to_bytes(a.w) == to_bytes(b.w) && a.w.state_hash(40) == b.w.state_hash(40),
              "S8 the same under-supplied fight twice is byte-identical");
    }

    // -----------------------------------------------------------------------
    std::printf("\nS9 - the loader rejects a rate that is not an integer per-mille\n");
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
        check(rejects("economy.military.carried_supplies.food_per_day = -1"), "S9a a negative rate");
        check(rejects("economy.military.carried_supplies.ammo_per_battle_day = 1001"), "S9b a rate past 1000");
        check(rejects("economy.military.carried_supplies.equipment_per_battle_day = 2.5"), "S9c a fraction");
        check(rejects("economy.military.carried_supplies.fuel_per_day = 3"), "S9d an unknown key");
    }

    // -----------------------------------------------------------------------
    std::printf("\nS10 - the remainder carry: no drift from rounding\n");
    {
        const std::set<entity_id> none;
        bool held = true;
        for (const int share : { 999, 900, 700, 667 })
        {
            camp c;
            build_camp(c, share);
            for (int day = 1; day <= 360; ++day) (void)run_carried_supplies_day(c.w, sp, none);
            const unit_component& u = c.w.units.at(c.u);
            held = held && u.food_permille == 1000 && u.equipment_permille == 1000 && u.food_remainder == 0;
        }
        check(held, "S10a at 999, 900, 700 and 667 cover a full stock stays full for a year (floor no longer drains)");

        camp g;
        build_camp(g, 700);
        g.w.units.at(g.u).food_permille = 500;
        for (int day = 1; day <= 100; ++day) (void)run_carried_supplies_day(g.w, sp, none);
        const int64_t gain = net_u(sp, sp.food_per_day, sp.food_per_day, 700);
        std::printf("      500 food at 700 cover, 100 days: %d (+%d/1e6); exact %.2f a day\n",
                    g.w.units.at(g.u).food_permille, g.w.units.at(g.u).food_remainder,
                    static_cast<double>(gain) / k_supply_remainder_unit);
        check(g.w.units.at(g.u).food_permille == after_days(500, gain, 100),
              "S10b a fractional daily gain accrues exactly (1.65 a day, not 1)");

        camp h;
        build_camp(h, 666);
        for (int day = 1; day <= 360; ++day) (void)run_carried_supplies_day(h.w, sp, none);
        const int64_t loss = net_u(sp, sp.food_per_day, sp.food_per_day, 666);
        std::printf("      full food at 666 cover, a year: %d (exact net %.3f a day)\n",
                    h.w.units.at(h.u).food_permille, static_cast<double>(loss) / k_supply_remainder_unit);
        check(h.w.units.at(h.u).food_permille == after_days(1000, loss, 360) && h.w.units.at(h.u).food_permille >= 985,
              "S10c just below break-even the drain is its true 0.033 a day, not a floored 1");

        // An EXACT break-even (rates chosen so share x (need + restock) == need): a
        // half stock holds for ten years, remainder and all.
        carried_supply_params e = sp;
        e.food_per_day = 30; e.restock_permille = 250; // 800 x (30 + 7.5) = 30
        camp x;
        build_camp(x, 800);
        x.w.units.at(x.u).food_permille = 500;
        for (int day = 1; day <= 3600; ++day) (void)run_carried_supplies_day(x.w, e, none);
        check(x.w.units.at(x.u).food_permille == 500 && x.w.units.at(x.u).food_remainder == 0,
              "S10d at exact break-even a half stock holds for ten years, to the millionth");
    }

    // -----------------------------------------------------------------------
    std::printf("\nS11 - the quiet world\n");
    {
        bool quiet = true;
        for (const int share : { 1000, 700 })
        {
            camp c;
            build_camp(c, share);
            c.w.units.at(c.u).morale_permille = share; // morale already at its supply
            c.w.units.at(c.u).readiness_permille = share;
            const uint64_t before = c.w.state_hash(1);
            const std::string bytes = to_bytes(c.w);
            quiet = quiet && !carried_supplies_pending(c.w, sp);
            // The military days alone (advance_quarter_to also moves the construction
            // cursor, which is not the military pass's to keep still).
            for (int day = 1; day <= 90; ++day) (void)run_military_day(c.w, reg, day);
            quiet = quiet && c.w.state_hash(1) == before && to_bytes(c.w) == bytes;
        }
        check(quiet, "S11 a full unit at full or past-break-even cover is inert: no pending day, a quarter writes nothing");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
