// ---------------------------------------------------------------------------
// BL-1453 — land recon (MILITARY.md § Recon, Ben 2026-10-29). No SDL/Lua.
// ---------------------------------------------------------------------------
// A unit damages only the share of its target its side can see: visible =
// min(1, side recon / target stealth), and every hit on the target is scaled by it.
// Two-way, no floor; the defender's recon counts 1.5x. A side's recon is its
// fighting units' kit, its nation's units adjacent to the defended tile, time
// (target stealth x min(1, day / 20)) and its air over the province.
//
//   R1  ZERO RECON, DAY 1, LANDS ONLY THE TIME TERM: 1/20 of the full-vision hits
//       for the attacker, 1.5/20 for the defender (pure resolver, same stream).
//   R2  DAY 20 IS FULL VISION from time alone; day 19 is not; the world's battle
//       day counts rounds fought.
//   R3  THE DEFENDER'S RECON COUNTS 1.5x.
//   R4  ADJACENCY ADDS: an own unit beside the defended tile, not in the battle,
//       adds its recon; one far away, or the enemy's, does not.
//   R5  A WING OVER THE PROVINCE ADDS: recon class 40 points, any other 10, x
//       strength / provinces; flying superiority adds 20 x its share; a wing over
//       another province, or one still mustering, adds nothing.
//   R6  STEALTH KIT LOWERS HITS TAKEN: the same target with more stealth is seen
//       less and takes fewer hits (pure, isolated); end to end, an infantry unit in
//       a ceramic plate carrier loses fewer men than one in a steel helmet.
//   R7  DETERMINISM: two identical worlds fight byte-identical battles.
//   M1  MEASURED (reported, not asserted): base-infantry battles over 200 seeds at
//       1:1, 1.5:1, 2:1 and 3:1 (two defending units of 500 against 2, 3, 4, 6),
//       full vision vs recon, with no round cap (Ben, 2026-10-29) — end types, mean
//       and longest days, men lost; a fight still live at the 2000-round guard is
//       counted, not hidden. And day-1 damage on typical kit.
//
// Run: land_recon_harness
// ---------------------------------------------------------------------------

#include "world/air_combat.hpp"
#include "world/battle_system.hpp"
#include "world/campaign_battle.hpp"
#include "world/logistics.hpp" // body_tile_grid
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/unit_designs.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;
int checks   = 0;

void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

constexpr uint32_t PARTITION_SEED = 0xB1453u;

// ---- the pure resolver ------------------------------------------------------

land_combatant infantry(entity_id id, int men = 500)
{
    land_combatant c;
    c.unit  = id;
    c.men   = men;
    c.stats = land_class_stats(land_class::infantry);
    return c;
}

campaign_battle_state open_pure(uint32_t seed, const std::vector<land_combatant>& a,
                                const std::vector<land_combatant>& d)
{
    campaign_battle_identity id;
    id.attacker = 1; id.defender = 2; id.tile = 99; id.tick = 0; id.world_seed = seed;
    return begin_campaign_battle(id, a, d, terrain_substrate::sedimentary, terrain_cover::grass,
                                 150u, terrain_landform::plains);
}

/// Set both sides' visibility for the round about to be fought, as the battle
/// pass does (kit recon of the fighting units only — no adjacency or air here).
void set_vision(const campaign_battle_state& st, std::vector<land_combatant>& a,
                std::vector<land_combatant>& d, const land_recon_params& rp)
{
    const int day = st.rounds_fought + 1;
    int64_t ar = 0, dr = 0;
    for (const land_combatant& c : a) if (land_combatant_fighting(c)) ar += int64_t{c.stats.recon} * 1000;
    for (const land_combatant& c : d) if (land_combatant_fighting(c)) dr += int64_t{c.stats.recon} * 1000;
    for (land_combatant& c : d) c.visible = visible_share_permille(ar, c.stats.stealth, day, rp.time_full_days, 1000);
    for (land_combatant& c : a) c.visible = visible_share_permille(dr, c.stats.stealth, day, rp.time_full_days, rp.defender_permille);
}

int men_of(const std::vector<land_combatant>& s)
{
    int n = 0;
    for (const land_combatant& c : s) n += c.men;
    return n;
}

// ---- the world ----------------------------------------------------------------

struct fixture
{
    world     w;
    entity_id body = null_entity;
    entity_id att = null_entity, def = null_entity;
    entity_id ua = null_entity, ub = null_entity;
    entity_id tile = null_entity; ///< the defended tile
    uint32_t  province = 0;
};

entity_id tile_at(world& w, entity_id body, int c, int r)
{
    const auto& g = body_tile_grid(w, body);
    return g[static_cast<std::size_t>(r * w.bodies.at(body).grid_width + c)];
}

entity_id add_nation(world& w, const char* name)
{
    const entity_id n = w.create_entity();
    nation_component nc{};
    nc.name = name; nc.treasury = 10000.0f;
    w.nations[n] = nc;
    return n;
}

entity_id add_unit(world& w, entity_id owner, entity_id tile, int count = 500)
{
    const entity_id u = w.create_entity();
    unit_component uc{};
    uc.owner = owner; uc.position = tile; uc.count = count; uc.establishment = count;
    uc.type = static_cast<uint16_t>(std::max(0, modern_row_for(land_class::infantry)));
    w.units[u] = uc;
    return u;
}

void build(fixture& f)
{
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.grid_width = 12; bc.grid_height = 8;
    w.bodies[f.body] = bc;
    for (int r = 0; r < bc.grid_height; ++r)
        for (int c = 0; c < bc.grid_width; ++c)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body = f.body; tc.grid_x = c; tc.grid_y = r;
            tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass;
            tc.cover_density = 150; tc.landform = terrain_landform::plains; tc.habitability = 0.5f;
            w.tiles[t] = tc;
        }
    build_province_partition(w, PARTITION_SEED);
    f.att  = add_nation(w, "Attacker");
    f.def  = add_nation(w, "Defender");
    f.ua   = add_unit(w, f.att, tile_at(w, f.body, 4, 4));
    f.tile = tile_at(w, f.body, 5, 4);
    f.ub   = add_unit(w, f.def, f.tile);
    f.province = w.provinces.province_of(f.tile);
    declare_hostile(w, f.att, f.def);
}

const active_battle* the_battle(const fixture& f)
{
    for (const active_battle& b : f.w.battles)
        if (b.tile == f.tile) return &b;
    return nullptr;
}

entity_id add_airbase(fixture& f, entity_id owner, int c, int r)
{
    const entity_id id = f.w.create_entity();
    building_component b{};
    b.tile = tile_at(f.w, f.body, c, r);
    b.type = building_type::airbase;
    b.ticks_remaining = 0;
    f.w.buildings[id] = b;
    f.w.nations.at(owner).state_assets.push_back(id);
    return id;
}

uint16_t air_row(air_class c)
{
    const auto& t = air_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
        if (t[i].cls == c) return static_cast<uint16_t>(i);
    return 0;
}

entity_id add_wing(fixture& f, entity_id owner, entity_id base, air_class cls, air_mission m,
                   std::vector<uint32_t> provs)
{
    const entity_id id = f.w.create_entity();
    air_wing wg;
    wg.owner = owner; wg.airbase = base; wg.type = air_row(cls);
    wg.mission = m; wg.provinces = std::move(provs);
    std::sort(wg.provinces.begin(), wg.provinces.end());
    f.w.wings.emplace(id, wg);
    return id;
}

/// One measured sweep cell.
struct cell
{
    int a_broken = 0, d_broken = 0, live = 0, longest = 0;
    long long rounds = 0, a_lost = 0, d_lost = 0;
};

/// A HARNESS guard, not a game rule: the resolver has no round cap, so a fight
/// that lands nothing would run forever here. One still live at it is counted.
constexpr int k_guard_rounds = 2000;

cell sweep(int attackers, int defenders, bool recon, const land_recon_params& rp)
{
    cell out;
    const campaign_battle_params p{};
    for (uint32_t seed = 1; seed <= 200; ++seed)
    {
        std::vector<land_combatant> a, d;
        for (int k = 0; k < attackers; ++k) a.push_back(infantry(10 + k));
        for (int k = 0; k < defenders; ++k) d.push_back(infantry(100 + k));
        campaign_battle_state st = open_pure(seed, a, d);
        const int a0 = men_of(a), d0 = men_of(d);
        while (st.end == campaign_battle_end::in_progress && st.rounds_fought < k_guard_rounds)
        {
            if (recon) set_vision(st, a, d, rp);
            step_campaign_battle(st, a, d, p);
        }
        if (st.end == campaign_battle_end::attacker_broken) ++out.a_broken;
        else if (st.end == campaign_battle_end::defender_broken) ++out.d_broken;
        else ++out.live;
        out.longest = std::max(out.longest, st.rounds_fought);
        out.rounds += st.rounds_fought;
        out.a_lost += a0 - men_of(a);
        out.d_lost += d0 - men_of(d);
    }
    return out;
}

} // namespace

int main()
{
    std::printf("=== land recon (BL-1453; MILITARY.md § Recon) ===\n");
    const recipe_registry reg;
    const land_recon_params& rp = reg.military().recon;
    std::printf("  rates: time_full_days %d, defender %d permille, recon wing %d, other wing %d, superiority %d\n",
                rp.time_full_days, rp.defender_permille, rp.recon_wing_points, rp.wing_points,
                rp.superiority_points);
    const land_combat_stats inf = land_class_stats(land_class::infantry);
    std::printf("  base infantry: recon %d, stealth %d\n", inf.recon, inf.stealth);

    // -----------------------------------------------------------------------
    std::printf("\nR1 - zero recon on day 1 lands only the time term\n");
    {
        check(visible_share_permille(0, 40, 1, 20, 1000) == 50, "R1a zero recon, day 1: 50 permille visible (1/20)");
        check(visible_share_permille(0, 40, 0, 20, 1000) == 0, "R1b zero recon before any day: nothing seen (no floor)");
        check(visible_share_permille(0, 40, 1, 20, 1500) == 75, "R1c the defender's day-1 time term counts 1.5x: 75");

        std::vector<land_combatant> a_full{infantry(1)}, d_full{infantry(2)};
        std::vector<land_combatant> a_dark{infantry(1)}, d_dark{infantry(2)};
        for (auto* s : {&a_dark, &d_dark})
            for (land_combatant& c : *s) c.stats.recon = 0;
        campaign_battle_state full = open_pure(7, a_full, d_full);
        campaign_battle_state dark = open_pure(7, a_dark, d_dark);
        set_vision(dark, a_dark, d_dark, rp);
        step_campaign_battle(full, a_full, d_full);
        step_campaign_battle(dark, a_dark, d_dark);
        const int fa = full.rounds[0].attacker_power, fd = full.rounds[0].defender_power;
        const int da = dark.rounds[0].attacker_power, dd = dark.rounds[0].defender_power;
        std::printf("        full vision landed atk %d / def %d; zero recon landed atk %d / def %d\n", fa, fd, da, dd);
        check(fa > 0 && da == static_cast<int>(int64_t{fa} * 50 / 1000),
              "R1d the attacker lands exactly 1/20 of its full-vision hits");
        check(fd > 0 && dd == static_cast<int>(int64_t{fd} * 75 / 1000),
              "R1e the defender lands exactly 1.5/20 of its full-vision hits");
        check(da > 0 && dd > 0, "R1f ...so a fresh battle with no recon still lands something on day 1");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR2 - day 20 is full vision from time alone\n");
    {
        check(visible_share_permille(0, 40, 20, 20, 1000) == 1000, "R2a zero recon, day 20: fully seen");
        check(visible_share_permille(0, 40, 19, 20, 1000) == 950, "R2b day 19: 950");
        check(visible_share_permille(0, 40, 60, 20, 1000) == 1000, "R2c capped at 1000 past day 20");
        check(visible_share_permille(0, 0, 1, 20, 1000) == 1000, "R2d a target with no stealth is fully seen");
        active_battle b;
        b.state.rounds_fought = 0;
        const int d1 = battle_day(b);
        b.state.rounds_fought = 19;
        const int d20 = battle_day(b);
        check(d1 == 1 && d20 == 20, "R2e the world's battle day is rounds fought + 1 (one round a day)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR3 - the defender's recon counts 1.5x\n");
    {
        const int a = visible_share_permille(10000, 40, 1, 20, 1000);
        const int d = visible_share_permille(10000, 40, 1, 20, 1500);
        std::printf("        10 points vs stealth 40, day 1: attacker sees %d, defender %d\n", a, d);
        check(a == 300 && d == 450, "R3a (10 + 2) / 40 = 300 for the attacker, x1.5 = 450 for the defender");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR4 - adjacency adds\n");
    {
        fixture f;
        build(f);
        check(open_tile_battle(f.w, 0, f.tile, f.att, f.def, {f.ua}), "R4 setup: the battle opens");
        const active_battle* b = the_battle(f);
        const int64_t a0 = battle_side_recon_milli(f.w, reg, *b, true);
        const int64_t d0 = battle_side_recon_milli(f.w, reg, *b, false);
        std::printf("        attacker %lld, defender %lld milli-points at open\n",
                    static_cast<long long>(a0), static_cast<long long>(d0));
        check(a0 == int64_t{inf.recon} * 1000 && d0 == int64_t{inf.recon} * 1000,
              "R4a each side's fighting infantry gives its kit recon");
        // A second attacker unit beside the defended tile, arriving after the open.
        add_unit(f.w, f.att, tile_at(f.w, f.body, 5, 5));
        // One far away, and a defender unit beside the tile.
        add_unit(f.w, f.att, tile_at(f.w, f.body, 0, 0));
        add_unit(f.w, f.def, tile_at(f.w, f.body, 6, 4));
        b = the_battle(f);
        const int64_t a1 = battle_side_recon_milli(f.w, reg, *b, true);
        const int64_t d1 = battle_side_recon_milli(f.w, reg, *b, false);
        check(a1 == a0 + int64_t{inf.recon} * 1000, "R4b an own unit beside the tile, not in the battle, adds its recon; a far one does not");
        check(d1 == d0 + int64_t{inf.recon} * 1000, "R4c the defender's unit beside its tile adds to the defender, not the attacker");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR5 - a wing over the province adds\n");
    {
        fixture f;
        build(f);
        open_tile_battle(f.w, 0, f.tile, f.att, f.def, {f.ua});
        const active_battle* b = the_battle(f);
        const int64_t a0 = battle_side_recon_milli(f.w, reg, *b, true);
        const entity_id base = add_airbase(f, f.att, 0, 7);
        const entity_id rw = add_wing(f, f.att, base, air_class::recon, air_mission::recon, {f.province});
        const int64_t a1 = battle_side_recon_milli(f.w, reg, *b, true);
        check(a1 - a0 == int64_t{rp.recon_wing_points} * 1000, "R5a a recon wing over the province adds recon_wing_points");
        add_wing(f, f.att, base, air_class::fighter, air_mission::superiority, {f.province});
        const int64_t a2 = battle_side_recon_milli(f.w, reg, *b, true);
        check(a2 - a1 == int64_t{rp.wing_points} * 1000, "R5b any other wing over it adds the base wing_points");
        f.w.air_superiority[{f.province, f.att}] = 100;
        const int64_t a3 = battle_side_recon_milli(f.w, reg, *b, true);
        check(a3 - a2 == int64_t{rp.superiority_points} * 1000, "R5c flying superiority uncontested adds superiority_points x 1000 share");
        f.w.air_superiority[{f.province, f.def}] = 300;
        const int64_t a4 = battle_side_recon_milli(f.w, reg, *b, true);
        check(a4 - a2 == int64_t{rp.superiority_points} * 250, "R5d ...in proportion to its share (100 vs 300: 250)");
        const int64_t d4 = battle_side_recon_milli(f.w, reg, *b, false);
        check(d4 == int64_t{inf.recon} * 1000 + int64_t{rp.superiority_points} * 750,
              "R5e the defender, flying superiority there, adds its share (750) and no wing it lacks");
        // A wing over two provinces gives half; one mustering gives none.
        f.w.wings.at(rw).provinces.push_back(f.province + 100000u);
        std::sort(f.w.wings.at(rw).provinces.begin(), f.w.wings.at(rw).provinces.end());
        const int64_t a5 = battle_side_recon_milli(f.w, reg, *b, true);
        check(a4 - a5 == int64_t{rp.recon_wing_points} * 500, "R5f a wing over two provinces gives each half");
        f.w.wings.at(rw).muster_days = 5;
        const int64_t a6 = battle_side_recon_milli(f.w, reg, *b, true);
        check(a4 - a6 == int64_t{rp.recon_wing_points} * 1000, "R5g a wing still mustering gives nothing");
        // In the step: the wing raises what the attacker lands on day 1.
        fixture g, h;
        build(g); build(h);
        open_tile_battle(g.w, 0, g.tile, g.att, g.def, {g.ua});
        open_tile_battle(h.w, 0, h.tile, h.att, h.def, {h.ua});
        const entity_id hb = add_airbase(h, h.att, 0, 7);
        add_wing(h, h.att, hb, air_class::recon, air_mission::recon, {h.province});
        run_battles(g.w, reg, 0);
        run_battles(h.w, reg, 0);
        const active_battle* bg = the_battle(g);
        const active_battle* bh = the_battle(h);
        const int pg = bg ? bg->state.rounds.back().attacker_power : -1;
        const int ph = bh ? bh->state.rounds.back().attacker_power : -1;
        std::printf("        day-1 attacker landed: %d without the wing, %d with it\n", pg, ph);
        check(pg > 0 && ph > pg, "R5h the recon wing raises the attacker's day-1 hits in the battle itself");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR6 - stealth kit lowers hits taken\n");
    {
        std::vector<land_combatant> a1{infantry(1)}, d1{infantry(2)};
        std::vector<land_combatant> a2{infantry(1)}, d2{infantry(2)};
        d2[0].stats.stealth += 10; // the ceramic plate carrier's stealth, alone
        campaign_battle_state s1 = open_pure(11, a1, d1), s2 = open_pure(11, a2, d2);
        set_vision(s1, a1, d1, rp);
        set_vision(s2, a2, d2, rp);
        std::printf("        attacker sees %d of the plain target, %d of the stealthier one\n",
                    d1[0].visible, d2[0].visible);
        step_campaign_battle(s1, a1, d1);
        step_campaign_battle(s2, a2, d2);
        check(d2[0].visible < d1[0].visible, "R6a more stealth: a smaller visible share");
        check(s2.rounds[0].attacker_power < s1.rounds[0].attacker_power,
              "R6b ...and fewer hits land on it (stealth isolated, same stream)");

        const uint16_t plates = kit_item_by_id("inf.armour.ceramic_plates");
        check(plates != k_no_kit_item, "R6 setup: the ceramic plate carrier is in the catalogue");
        const int st_delta = plates != k_no_kit_item ? kit_catalogue()[plates].delta.stealth : 0;
        check(st_delta > 0, "R6c better infantry armour carries stealth (+" + std::to_string(st_delta) + ")");
        fixture g, h;
        build(g); build(h);
        h.w.units.at(h.ub).kit.fit[1] = kit_variant_of(plates);
        check(unit_land_stats(h.w.units.at(h.ub)).stealth == inf.stealth + st_delta,
              "R6d a unit raised in it fights with the class stealth plus the kit's");
        open_tile_battle(g.w, 0, g.tile, g.att, g.def, {g.ua});
        open_tile_battle(h.w, 0, h.tile, h.att, h.def, {h.ua});
        for (int d = 0; d < 3; ++d) { run_battles(g.w, reg, d); run_battles(h.w, reg, d); }
        const int lost_g = 500 - g.w.units.at(g.ub).count;
        const int lost_h = 500 - h.w.units.at(h.ub).count;
        std::printf("        defender men lost over three days: %d in a steel helmet, %d in ceramic plates\n",
                    lost_g, lost_h);
        check(lost_h < lost_g, "R6e end to end, the plate carrier's defender loses fewer men (stealth and defence together)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR7 - determinism\n");
    {
        fixture g, h;
        build(g); build(h);
        add_unit(g.w, g.att, tile_at(g.w, g.body, 5, 5));
        add_unit(h.w, h.att, tile_at(h.w, h.body, 5, 5));
        open_tile_battle(g.w, 0, g.tile, g.att, g.def, {g.ua});
        open_tile_battle(h.w, 0, h.tile, h.att, h.def, {h.ua});
        for (int d = 0; d < 6; ++d) { run_battles(g.w, reg, d); run_battles(h.w, reg, d); }
        bool same = g.w.units.size() == h.w.units.size();
        for (const auto& [uid, u] : g.w.units)
        {
            const auto it = h.w.units.find(uid);
            same = same && it != h.w.units.end() && it->second.count == u.count
                && it->second.readiness_permille == u.readiness_permille;
        }
        check(same, "R7a two identical worlds fight identical battles");
    }

    // -----------------------------------------------------------------------
    std::printf("\nM1 - measured: base infantry, 200 seeds (reported, not asserted)\n");
    {
        std::printf("        %-24s %8s %8s %6s %8s %8s %9s %9s\n", "case (days)", "atk_brk", "def_brk",
                    "live", "mean", "longest", "atk_lost", "def_lost");
        int live_total = 0;
        for (const int n : {2, 3, 4, 6})
            for (const bool rc : {false, true})
            {
                const cell c = sweep(n, 2, rc, rp);
                live_total += c.live;
                char label[64];
                std::snprintf(label, sizeof label, "%.1f:1 %s", n / 2.0, rc ? "recon" : "full vision");
                std::printf("        %-24s %8d %8d %6d %8.2f %8d %9.1f %9.1f\n", label, c.a_broken, c.d_broken,
                            c.live, c.rounds / 200.0, c.longest, c.a_lost / 200.0, c.d_lost / 200.0);
            }
        check(live_total == 0, "M1b every measured fight ends on a break (none reaches the 2000-round guard)");
        // Day-1 damage on typical kit, 1v1.
        std::vector<land_combatant> a{infantry(1)}, d{infantry(2)};
        campaign_battle_state st = open_pure(3, a, d);
        set_vision(st, a, d, rp);
        std::printf("        day 1, 1v1 base infantry: attacker sees %d, defender sees %d\n", d[0].visible, a[0].visible);
        step_campaign_battle(st, a, d);
        std::vector<land_combatant> fa{infantry(1)}, fd{infantry(2)};
        campaign_battle_state fs = open_pure(3, fa, fd);
        step_campaign_battle(fs, fa, fd);
        std::printf("        day 1 landed (recon / full vision): attacker %d / %d, defender %d / %d\n",
                    st.rounds[0].attacker_power, fs.rounds[0].attacker_power,
                    st.rounds[0].defender_power, fs.rounds[0].defender_power);
        std::printf("        day 1 men lost (recon / full): attacker %d / %d, defender %d / %d; "
                    "readiness lost: attacker %d / %d, defender %d / %d\n",
                    500 - a[0].men, 500 - fa[0].men, 500 - d[0].men, 500 - fd[0].men,
                    1000 - a[0].readiness, 1000 - fa[0].readiness, 1000 - d[0].readiness, 1000 - fd[0].readiness);
        check(st.rounds[0].attacker_power > 0 && st.rounds[0].defender_power > 0
                  && a[0].readiness < 1000 && d[0].readiness < 1000,
              "M1a on typical kit, day 1 still lands damage on both sides (readiness spent)");
    }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    std::printf(failures == 0 ? "ALL PASS\n" : "FAILURES PRESENT\n");
    return failures == 0 ? 0 : 1;
}
