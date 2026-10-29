// ---------------------------------------------------------------------------
// BL-1453 — land recon (MILITARY.md § Recon, Ben 2026-10-29). No SDL/Lua.
// ---------------------------------------------------------------------------
// Recon is the ATTACKER's: its hits on a defending unit are scaled by
// min(1, its recon stock / the unit's stealth); the defender's hits are always
// full. The stock opens at the recon of the attacker's units that faced the tile
// a whole day (only haste attacks blind) and grows every day by the recon of its
// units in the battle, its own units adjacent to the defended tile (at once,
// nothing stored) and its air, until it covers the stealthiest defender.
//
//   R1  THE SHARE: min(1, stock / stealth); no stock sees nothing; no stealth is
//       fully seen. (Replaces the time-term rows R1a-R1f and R2: the 20-day ramp
//       is retired by the stock, Ben 2026-10-29.)
//   R2  A BLIND ATTACK lands nothing on day 1 and sees in full within a few days
//       (reported, asserted <= 5) — while the defender hits in full from day 1.
//   R3  A FACED ATTACK opens at its facing units' recon (reported share).
//   R4  ADJACENCY ADDS AT ONCE: an own unit beside the tile adds its recon to the
//       day's growth while it stands there, and nothing once it leaves.
//   R5  A WING OVER THE PROVINCE ADDS to the attacker's growth (recon class 40,
//       others 10, x strength / provinces; superiority x its share).
//   R6  STEALTH KIT LOWERS HITS TAKEN.
//   R7  DETERMINISM: two identical worlds fight byte-identical battles.
//   M1  MEASURED (reported): base-infantry battles at 1:1 .. 3:1, recon vs full
//       vision; blind and faced days to full.
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

land_combatant infantry(entity_id id, int men = 500)
{
    land_combatant c;
    c.unit  = id;
    c.men   = men;
    c.stats = land_class_stats(land_class::infantry);
    return c;
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

/// One measured cell of the pure sweep: the resolver with the attacker's stock
/// emulated as the battle pass keeps it (opening, then + the attackers' recon a
/// day), or with full vision.
struct cell
{
    int a_broken = 0, d_broken = 0, live = 0, rounds = 0, a_lost = 0, d_lost = 0;
};

/// A HARNESS guard, not a game rule: the round budget of the headless runner.
constexpr int k_guard_rounds = 3650;

cell fight_pure(int attackers, int defenders, bool recon, bool faced)
{
    cell out;
    campaign_battle_identity id;
    id.attacker = 1; id.defender = 2; id.tile = 99; id.world_seed = 7;
    std::vector<land_combatant> a, d;
    for (int k = 0; k < attackers; ++k) a.push_back(infantry(10 + k));
    for (int k = 0; k < defenders; ++k) d.push_back(infantry(100 + k));
    campaign_battle_state st = begin_campaign_battle(id, a, d, terrain_substrate::sedimentary,
                                                     terrain_cover::grass, 150u, terrain_landform::plains);
    const int a0 = men_of(a), d0 = men_of(d);
    int64_t daily = 0;
    for (const land_combatant& c : a) daily += int64_t{c.stats.recon} * 1000;
    int64_t stock = faced ? daily : 0;
    const int64_t cap = int64_t{land_class_stats(land_class::infantry).stealth} * 1000;
    while (st.end == campaign_battle_end::in_progress && st.rounds_fought < k_guard_rounds)
    {
        for (land_combatant& c : d) c.visible = recon ? visible_share_permille(stock, c.stats.stealth) : 1000;
        step_campaign_battle(st, a, d);
        stock = std::min(cap, stock + daily);
    }
    if (st.end == campaign_battle_end::attacker_broken) out.a_broken = 1;
    else if (st.end == campaign_battle_end::defender_broken) out.d_broken = 1;
    else out.live = 1;
    out.rounds = st.rounds_fought;
    out.a_lost = a0 - men_of(a);
    out.d_lost = d0 - men_of(d);
    return out;
}

} // namespace

int main()
{
    std::printf("=== land recon (BL-1453; MILITARY.md § Recon) ===\n");
    const recipe_registry reg;
    const land_recon_params& rp = reg.military().recon;
    const land_combat_stats inf = land_class_stats(land_class::infantry);
    std::printf("  base infantry: recon %d, stealth %d; air: recon wing %d, other wing %d, superiority %d\n",
                inf.recon, inf.stealth, rp.recon_wing_points, rp.wing_points, rp.superiority_points);
    bool every_class_sees = true;
    for (int c = 0; c < land_class_count; ++c)
        every_class_sees = every_class_sees && land_class_stats(static_cast<land_class>(c)).recon > 0;
    check(every_class_sees, "R0 every land class has a base recon above zero");

    // -----------------------------------------------------------------------
    std::printf("\nR1 - the share: min(1, stock / stealth)\n");
    {
        check(visible_share_permille(0, 40) == 0, "R1a no stock sees nothing of a stealthy unit");
        check(visible_share_permille(14000, 40) == 350, "R1b 14 recon against stealth 40 sees 350 permille");
        check(visible_share_permille(40000, 40) == 1000 && visible_share_permille(90000, 40) == 1000,
              "R1c a stock that covers the stealth sees it all, capped at 1");
        check(visible_share_permille(0, 0) == 1000, "R1d a target with no stealth is fully seen");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR2 - a blind attack (haste: the column arrived today)\n");
    int blind_full_day = -1;
    {
        fixture f;
        build(f);
        f.w.units.at(f.ua).tile_since_day = 0; // it came onto its tile on the contact's day
        check(open_tile_battle(f.w, 0, f.tile, f.att, f.def, {f.ua}), "R2 setup: the battle opens");
        const active_battle* b = the_battle(f);
        check(b != nullptr && b->attacker_recon_milli == 0, "R2a only haste attacks blind: the stock opens at 0");
        std::printf("        day  stock  attacker landed  defender landed\n");
        for (int day = 1; day <= 12 && the_battle(f) != nullptr; ++day)
        {
            run_battles(f.w, reg, day);
            const active_battle* bb = the_battle(f);
            if (bb == nullptr) break;
            const campaign_battle_round& r = bb->state.rounds.back();
            std::printf("        %3d  %5d  %15d  %15d\n", day, bb->attacker_recon_milli, r.attacker_power, r.defender_power);
            if (day == 1)
            {
                check(r.attacker_power == 0, "R2b blind on day 1: the attacker lands nothing");
                check(r.defender_power > 0, "R2c the defender's hits are always full: it lands on day 1");
            }
            if (blind_full_day < 0 && visible_share_permille(bb->attacker_recon_milli, inf.stealth) >= 1000)
                blind_full_day = day + 1; // the stock after day d is what day d+1 fights on
        }
        std::printf("        a blind 1v1 infantry attack fights in full vision from day %d\n", blind_full_day);
        check(blind_full_day > 0 && blind_full_day <= 5, "R2d recon builds very quickly: a blind attack sees in full within five days");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR3 - a faced attack (the line stood a day before the contact)\n");
    {
        fixture f;
        build(f);
        f.w.units.at(f.ua).tile_since_day = -1; // on its tile before any stamp
        add_unit(f.w, f.att, tile_at(f.w, f.body, 5, 5), 500); // a second unit facing the tile
        open_tile_battle(f.w, 10, f.tile, f.att, f.def, {f.ua});
        const active_battle* b = the_battle(f);
        const int share = b ? visible_share_permille(b->attacker_recon_milli, inf.stealth) : -1;
        std::printf("        two facing infantry open at stock %d: day-1 visible share %d permille\n",
                    b ? b->attacker_recon_milli : -1, share);
        check(b != nullptr && b->attacker_recon_milli == 2 * inf.recon * 1000,
              "R3a the opening stock is the recon of the attacker's units that faced the tile");
        fixture g;
        build(g);
        g.w.units.at(g.ua).tile_since_day = 9; // arrived yesterday: stood a whole day
        open_tile_battle(g.w, 10, g.tile, g.att, g.def, {g.ua});
        const active_battle* gb = the_battle(g);
        check(gb != nullptr && gb->attacker_recon_milli == inf.recon * 1000,
              "R3b a unit that took its tile the day before the contact counts toward the opening");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR4 - adjacency adds at once, nothing stored\n");
    {
        fixture f;
        build(f);
        open_tile_battle(f.w, 0, f.tile, f.att, f.def, {f.ua});
        const active_battle* b = the_battle(f);
        const int64_t a0 = battle_side_recon_milli(f.w, reg, *b, true);
        const entity_id beside = add_unit(f.w, f.att, tile_at(f.w, f.body, 5, 5));
        add_unit(f.w, f.att, tile_at(f.w, f.body, 0, 0)); // far away
        const int64_t a1 = battle_side_recon_milli(f.w, reg, *b, true);
        check(a0 == int64_t{inf.recon} * 1000 && a1 == a0 + int64_t{inf.recon} * 1000,
              "R4a an own unit beside the tile, not in the battle, adds its recon to the day's growth; a far one does not");
        f.w.units.at(beside).position = tile_at(f.w, f.body, 0, 1);
        check(battle_side_recon_milli(f.w, reg, *b, true) == a0, "R4b ...and nothing once it has left");
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
        f.w.air_superiority[{f.province, f.def}] = 300;
        const int64_t a3 = battle_side_recon_milli(f.w, reg, *b, true);
        check(a3 - a2 == int64_t{rp.superiority_points} * 250, "R5c superiority adds in proportion to its share (100 vs 300: 250)");
        f.w.wings.at(rw).muster_days = 5;
        check(a3 - battle_side_recon_milli(f.w, reg, *b, true) == int64_t{rp.recon_wing_points} * 1000,
              "R5d a wing still mustering gives nothing");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR6 - stealth kit lowers hits taken\n");
    {
        const uint16_t plates = kit_item_by_id("inf.armour.ceramic_plates");
        const int st_delta = plates != k_no_kit_item ? kit_catalogue()[plates].delta.stealth : 0;
        check(st_delta > 0, "R6a better infantry armour carries stealth (+" + std::to_string(st_delta) + ")");
        check(visible_share_permille(28000, inf.stealth + st_delta) < visible_share_permille(28000, inf.stealth),
              "R6b more stealth: a smaller share for the same stock");
        fixture g, h;
        build(g); build(h);
        g.w.units.at(g.ua).tile_since_day = 0;
        h.w.units.at(h.ua).tile_since_day = 0;
        h.w.units.at(h.ub).kit.fit[1] = kit_variant_of(plates);
        open_tile_battle(g.w, 0, g.tile, g.att, g.def, {g.ua});
        open_tile_battle(h.w, 0, h.tile, h.att, h.def, {h.ua});
        long long lg = 0, lh = 0;
        for (int d = 1; d <= 5; ++d) { run_battles(g.w, reg, d); run_battles(h.w, reg, d); }
        if (const active_battle* bg = the_battle(g)) for (const auto& r : bg->state.rounds) lg += r.attacker_power;
        if (const active_battle* bh = the_battle(h)) for (const auto& r : bh->state.rounds) lh += r.attacker_power;
        std::printf("        hits landed on the defender over a blind attack's first five days: %lld plain, %lld in plates\n", lg, lh);
        check(lg > 0 && lh < lg, "R6c end to end, the plate carrier's defender takes fewer hits (stealth and defence together)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nR7 - determinism\n");
    {
        fixture g, h;
        build(g); build(h);
        open_tile_battle(g.w, 0, g.tile, g.att, g.def, {g.ua});
        open_tile_battle(h.w, 0, h.tile, h.att, h.def, {h.ua});
        for (int d = 1; d <= 20; ++d) { run_battles(g.w, reg, d); run_battles(h.w, reg, d); }
        check(g.w.state_hash(20) == h.w.state_hash(20), "R7a two identical worlds fight identical battles (state hash)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nM1 - measured, base infantry (one fight a ratio: no randomness)\n");
    {
        std::printf("        %-26s %-16s %6s %9s %9s\n", "case", "end", "days", "atk_lost", "def_lost");
        for (const int n : {2, 3, 4, 6})
            for (const int mode : {0, 1, 2}) // full vision, recon faced, recon blind
            {
                const cell c = fight_pure(n, 2, mode != 0, mode == 1);
                char label[64];
                std::snprintf(label, sizeof label, "%.1f:1 %s", n / 2.0,
                              mode == 0 ? "full vision" : mode == 1 ? "recon, faced" : "recon, blind");
                std::printf("        %-26s %-16s %6d %9d %9d\n", label,
                            c.a_broken ? "attacker broken" : c.d_broken ? "defender broken" : "live at budget",
                            c.rounds, c.a_lost, c.d_lost);
            }
    }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    std::printf(failures == 0 ? "ALL PASS\n" : "FAILURES PRESENT\n");
    return failures == 0 ? 0 : 1;
}
