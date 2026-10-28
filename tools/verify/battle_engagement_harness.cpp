// ---------------------------------------------------------------------------
// BL-467 — battle state in world: the engagement trigger and the step. No SDL/Lua
// ---------------------------------------------------------------------------
// RE-POINTED 2026-10-27 (BL-1443, tile contact; MILITARY.md § Battles). The
// province co-presence trigger these rows were written against is gone: a battle
// now opens when a march tries to step onto a tile a hostile nation's units hold,
// and it is fought over that DEFENDED tile. So the fixture stands the two forces
// on ADJACENT tiles, not on one (two nations never share a tile), and a battle is
// opened by a march (B1) or by `open_tile_battle` directly (the rest — the march
// that makes the attack is tile_contact_harness's subject). Every row still
// asserts what it asserted; where it named the province key it names the tile.
//
//   B1  A FIGHT ACTUALLY HAPPENS: a march onto the defender's tile opens it, and
//       the real tick (run_economy_step) kills men.
//   B1c ...and a march toward a NON-hostile stack opens nothing.
//   B2  OPENING IS ORDER-INDEPENDENT: twin worlds produce identical battle sets.
//   B3  ONE BATTLE PER NATION PAIR PER TILE; mutual hostility does not open two.
//   B4  THE SEED FOLDS THE TILE, and role order still matters.
//   B5  PER-UNIT HITS CONSERVE: distributed losses equal the requested total.
//   B6  DETERMINISM BY IN-MEMORY REPLAY.
//   B7  THE GROUND IS THE DEFENDED TILE — not the defender's best tile nearby.
//   B8  WITHDRAWAL IS NAMED BY TILE, AND A REJECTED REQUEST MUTATES NOTHING.
//   B9  A UNIT IN CONTACT CANNOT MARCH.
//   B10-B14 the dispatch record and its text.
//   B15 BL-569: A DECISIVE CLOSE MOVES world::province_holder (the defended
//       tile's province) to the winner.
//   B16 BL-569: A STALEMATE LEAVES IT UNTOUCHED.
//   B17 open_tile_battle is ENTITY-ID-AGNOSTIC: it opens a battle with no stance
//       row (the capability the retired mercenary trigger used).
//   B18-B22 retreat, morale, recovery, a quiet world, a unit in two battles.
//
// Run: battle_engagement_harness
// ---------------------------------------------------------------------------

#include "core/battle_dispatch_text.hpp" // BL-468: the phrase bank, testable because it is its own TU
#include "world/battle_system.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/terrain_combat.hpp"
#include "world/unit_roster.hpp" // unit_to_stack_entry -- B15/B16's hand-built battle state
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <set>
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

constexpr uint32_t PARTITION_SEED = 0xB467u;
constexpr uint16_t ROW_INFANTRY   = 0;

entity_id make_grid_body(world& w, int gw, int gh)
{
    const entity_id body = w.create_entity();
    body_component bc{};
    bc.grid_width  = gw;
    bc.grid_height = gh;
    w.bodies[body] = bc;
    for (int r = 0; r < gh; ++r)
        for (int c = 0; c < gw; ++c)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body          = body;
            tc.grid_x        = c;
            tc.grid_y        = r;
            tc.substrate     = terrain_substrate::sedimentary;
            tc.cover         = terrain_cover::grass;
            tc.cover_density = 150;
            tc.landform      = terrain_landform::plains;
            tc.habitability  = 0.5f;
            w.tiles[t]       = tc;
        }
    return body;
}

entity_id add_corp(world& w, const char* name)
{
    // BL-1407: every belligerent is a nation (the corporation retired).
    const entity_id c = w.create_entity();
    nation_component cc{};
    cc.name     = name;
    cc.treasury = 10000.0f;
    w.nations[c] = cc;
    return c;
}

entity_id add_unit(world& w, entity_id corp, entity_id tile, int count = 500)
{
    const entity_id u = w.create_entity();
    unit_component uc{};
    uc.owner    = corp;
    uc.position = tile;
    uc.count    = count;
    uc.type     = ROW_INFANTRY;
    w.units[u]  = uc;
    return u;
}

entity_id tile_at(world& w, entity_id body, int c, int r)
{
    const auto& g = body_tile_grid(w, body);
    const int   gw = w.bodies.at(body).grid_width;
    return g[static_cast<std::size_t>(r * gw + c)];
}

/// A body with two nations' units on ADJACENT tiles: the attacker's at (2,2), the
/// defender's at (3,2) — a step a march takes. `tile` is the defended tile.
struct fixture
{
    world     w;
    entity_id body = null_entity;
    entity_id att = null_entity, def = null_entity;
    entity_id ua = null_entity, ub = null_entity;
    entity_id tile = null_entity;   ///< the defender's tile: the battle's identity and ground
    uint32_t  province = 0;         ///< the province that tile lies in
};

void build_fixture(fixture& f, bool declare = true)
{
    f.body = make_grid_body(f.w, 6, 6);
    build_province_partition(f.w, PARTITION_SEED);
    f.att = add_corp(f.w, "Attacker");
    f.def = add_corp(f.w, "Defender");
    f.ua   = add_unit(f.w, f.att, tile_at(f.w, f.body, 2, 2));
    f.tile = tile_at(f.w, f.body, 3, 2);
    f.ub   = add_unit(f.w, f.def, f.tile);
    f.province = f.w.provinces.province_of(f.tile);
    if (declare)
        declare_hostile(f.w, f.att, f.def);
}

/// Open the fixture's battle the way a march's attack does — `ua` attacking `tile`.
bool engage(fixture& f, int tick = 0)
{
    return open_tile_battle(f.w, tick, f.tile, f.att, f.def, { f.ua });
}

/// The authored per-class march points (scripts/economy.lua; MILITARY.md §
/// Marching) — a default registry carries zeros, which marches nothing.
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

int total_men(const world& w)
{
    int n = 0;
    for (const auto& kv : w.units) n += std::max(0, kv.second.count);
    return n;
}

corp_command march_cmd(entity_id corp, entity_id unit, entity_id tile)
{
    corp_command cmd{};
    cmd.corp = corp; cmd.as_nation = corp;
    cmd.verb = corp_verb::march_unit; cmd.subject = unit; cmd.tile = tile;
    return cmd;
}

} // namespace

int main()
{
    std::printf("=== battle engagement (BL-467; tile contact, BL-1443) ===\n");
    const recipe_registry reg = make_registry();

    // -----------------------------------------------------------------------
    std::printf("\nB1 — a fight actually happens: a march onto the defender's tile, then the real tick\n");
    {
        fixture f;
        build_fixture(f);
        const int before = total_men(f.w);
        check(f.w.battles.empty(), "B1 setup: no battle exists before the march");
        check(apply_corp_command(f.w, reg, march_cmd(f.att, f.ua, f.tile)) == corp_command_result::applied,
              "B1 setup: the attacker is ordered onto the defender's tile");
        // The march pass with no Logistic Point resolve handed in (the fixture's
        // nations make none); the real tick then steps the battle it opened.
        const unit_march_tick mt = run_unit_march(f.w, reg, false);
        check(mt.attacks == 1 && mt.battles_opened == 1 && f.w.battles.size() == 1
                  && f.w.battles.front().tile == f.tile,
              "B1a THE TRIGGER FIRED — the step onto a hostile stack's tile opened a battle over it");
        check(f.w.units.at(f.ua).position == tile_at(f.w, f.body, 2, 2),
              "B1b ...and the attacker stopped on its own tile (two nations never share one)");
        run_economy_step(f.w, reg);
        const int after = total_men(f.w);
        std::printf("        men %d -> %d over one tick\n", before, after);
        check(after < before,
              "B1c MEN DIED. Losses reached unit_component::count through the tick, not a direct resolver call");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB1d — and it does NOT fire without hostility\n");
    {
        fixture f;
        build_fixture(f, /*declare=*/false);
        const int before = total_men(f.w);
        (void)apply_corp_command(f.w, reg, march_cmd(f.att, f.ua, f.tile));
        const unit_march_tick mt = run_unit_march(f.w, reg, false);
        run_economy_step(f.w, reg);
        check(f.w.battles.empty() && total_men(f.w) == before && mt.attacks == 0 && mt.blocked == 1
                  && f.w.units.at(f.ua).position != f.tile,
              "B1d a march onto a NEUTRAL stack is blocked, not an attack (the trigger is stance, not contact)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB2 — opening is order-independent\n");
    {
        fixture a, b;
        build_fixture(a);
        build_fixture(b);
        engage(a); engage(b);
        run_economy_step(a.w, reg);
        run_economy_step(b.w, reg);
        bool same = a.w.battles.size() == b.w.battles.size();
        for (std::size_t i = 0; same && i < a.w.battles.size(); ++i)
            same = a.w.battles[i].tile == b.w.battles[i].tile
                && a.w.battles[i].attacker == b.w.battles[i].attacker
                && a.w.battles[i].defender == b.w.battles[i].defender;
        check(same, "B2  twin worlds produce the same battle set, in the same order");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB3 — one battle per nation pair per tile\n");
    {
        fixture f;
        build_fixture(f);
        // A second unit for each side, and mutual hostility, must not open a second
        // battle over the tile — in either role order.
        const entity_id ua2 = add_unit(f.w, f.att, tile_at(f.w, f.body, 2, 2));
        add_unit(f.w, f.def, f.tile);
        declare_hostile(f.w, f.def, f.att); // now mutual
        check(engage(f), "B3 setup: the first attack opens");
        check(!open_tile_battle(f.w, 0, f.tile, f.att, f.def, { ua2 }) && f.w.battles.size() == 1,
              "B3a a second attack onto the same tile by the same pair does not reopen");
        check(!open_tile_battle(f.w, 0, f.tile, f.def, f.att, { ua2 }) && f.w.battles.size() == 1,
              "B3b ...nor does the reverse direction");
        const active_battle& b = f.w.battles.front();
        check(b.attacker_units.size() == 2 && b.defender_units.size() == 2,
              "B3c the stack on the tile defends whole, and the attacker's stack beside it joined");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB4 — the seed folds the tile, and role order still matters\n");
    {
        campaign_battle_identity a; a.attacker = 7; a.defender = 9; a.tile = 11; a.tick = 3; a.world_seed = 5;
        campaign_battle_identity b = a; b.tile = 12;
        campaign_battle_identity c = a; std::swap(c.attacker, c.defender);
        check(campaign_battle_seed(a) != campaign_battle_seed(b),
              "B4a two battles differing only in TILE are different streams");
        check(campaign_battle_seed(a) != campaign_battle_seed(c),
              "B4b swapping attacker and defender is a different stream (role order preserved)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB5 — per-unit hits conserve\n");
    {
        fixture f;
        build_fixture(f);
        engage(f);
        int before = total_men(f.w);
        int reported = 0, drift = 0;
        for (int i = 0; i < 6; ++i)
        {
            const int pre = total_men(f.w);
            const battle_tick bt = run_battles(f.w, reg, i);
            const int post = total_men(f.w);
            reported += bt.losses;
            if ((pre - post) != bt.losses)
                ++drift;
        }
        const int after = total_men(f.w);
        std::printf("        men %d -> %d over six ticks; pass reported %d losses\n",
                    before, after, reported);
        check(after >= 0, "B5a no unit count went negative");
        check(drift == 0,
              "B5b EXACT CONSERVATION: every tick, men removed == losses the pass reported");
        check(before - after == reported,
              "B5c ...and the totals agree end to end — no men minted or leaked");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB6 — determinism, by in-memory replay\n");
    {
        fixture a, b;
        build_fixture(a);
        build_fixture(b);
        engage(a); engage(b);
        for (int i = 0; i < 5; ++i) { run_economy_step(a.w, reg); run_economy_step(b.w, reg); }
        check(total_men(a.w) == total_men(b.w),
              "B6a same fixture, five ticks, twice: identical surviving strength");
        check(a.w.state_hash(5) == b.w.state_hash(5),
              "B6b ...and identical state_hash, which folds the battle list");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB7 — the ground is the defended tile\n");
    {
        fixture f;
        build_fixture(f);
        // The defender also stands BESIDE the tile on far better ground. Under the
        // retired envelope the battle anchored on the defender's best tile; now it is
        // fought over the tile attacked, whatever lies next to it.
        const entity_id t_hard = tile_at(f.w, f.body, 4, 2);
        auto& hard = f.w.tiles.at(t_hard);
        hard.substrate = terrain_substrate::rocky;
        hard.cover = terrain_cover::forest; hard.cover_density = 205;
        hard.landform = terrain_landform::mountain;
        add_unit(f.w, f.def, t_hard);
        const tile_component& soft = f.w.tiles.at(f.tile);
        const int d_soft = terrain_defence(soft.substrate, soft.cover, soft.cover_density, soft.landform);
        const int d_hard = terrain_defence(hard.substrate, hard.cover, hard.cover_density, hard.landform);
        std::printf("        defence: defended tile %d, the defender's tile beside it %d\n", d_soft, d_hard);
        check(d_hard > d_soft, "B7a the tile beside really is better ground");

        terrain_substrate gs{}; terrain_cover gc{}; std::uint8_t gd{}; terrain_landform gl{};
        const bool got = battle_ground(f.w, f.tile, gs, gc, gd, gl);
        check(got && terrain_defence(gs, gc, gd, gl) == d_soft,
              "B7b battle_ground is the defended tile's own terrain");
        check(engage(f), "B7 setup: the attack opens");
        const active_battle& b = f.w.battles.front();
        check(b.ground_tile == f.tile && b.state.terrain_lf == soft.landform
                  && b.state.terrain_cov == soft.cover && b.state.terrain_sub == soft.substrate,
              "B7c THE BATTLE IS FOUGHT ON THE DEFENDED TILE, not on the better ground its joiner holds");
        check(!battle_ground(f.w, 0x7FFFFFF0u, gs, gc, gd, gl),
              "B7d a tile that does not exist has no ground");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB8 — withdrawal is named by tile, and a rejection mutates nothing\n");
    {
        fixture f;
        build_fixture(f);
        engage(f);
        // BL-1445: ONE DAY's fight. A whole economy step is ninety days of rounds now,
        // and would end the battle before B8c could ask to leave it.
        run_battles(f.w, reg, 0);

        const uint64_t h_before = f.w.state_hash(1);
        const bool bogus = request_withdraw(f.w, f.att, 0x7FFFFFF0u);
        check(!bogus && f.w.state_hash(1) == h_before,
              "B8a a withdraw naming no live battle's tile is refused and leaves the world byte-identical");
        const bool wrong = request_withdraw(f.w, f.att, tile_at(f.w, f.body, 2, 2));
        check(!wrong && f.w.state_hash(1) == h_before,
              "B8a2 ...so is one naming the attacker's own tile (the battle is named by the DEFENDED tile)");

        const entity_id stranger = add_corp(f.w, "Stranger");
        const uint64_t h2 = f.w.state_hash(1);
        const bool notmine = request_withdraw(f.w, stranger, f.tile);
        check(!notmine && f.w.state_hash(1) == h2,
              "B8b a non-participant cannot withdraw someone else's force");

        if (!f.w.battles.empty())
        {
            const bool ok = request_withdraw(f.w, f.att, f.tile);
            check(ok, "B8c a participant's withdraw request is accepted");
            const int rounds_before = f.w.battles.front().state.rounds_fought;
            const battle_tick next = run_battles(f.w, reg, 1); // honoured at the day boundary
            check(rounds_before >= 0 && next.withdrew == 1,
                  "B8d the request is honoured on the following day, not instantly");
        }
        else
        {
            std::printf("        (battle concluded within one tick; B8c/B8d not exercised)\n");
        }
    }

    // -----------------------------------------------------------------------
    std::printf("\nB9 — a unit in contact cannot march\n");
    {
        fixture f;
        build_fixture(f);
        engage(f);
        check(unit_in_battle(f.w, f.ua), "B9a the attacking unit reads as in-battle");
        const uint64_t h = f.w.state_hash(1);
        const auto r = apply_corp_command(f.w, reg, march_cmd(f.att, f.ua, tile_at(f.w, f.body, 0, 0)));
        check(r == corp_command_result::rejected_state && f.w.state_hash(1) == h,
              "B9b march_unit is refused for a unit in contact — walking away is a priced withdrawal");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB10 — the dispatch record (BL-468/BL-469)\n");
    {
        fixture f;
        build_fixture(f);
        engage(f);
        const battle_tick t1 = run_battles(f.w, reg, 0);
        check(!t1.dispatches.empty(),
              "B10a a battle that fought emits a dispatch record");
        if (!t1.dispatches.empty())
        {
            const battle_dispatch& d = t1.dispatches.front();
            check(d.tile == f.tile && d.province == f.province && d.attacker == f.att && d.defender == f.def,
                  "B10b the record names the fight (tile, attacker, defender) and the tile's province");
            check(d.rounds_this_tick > 0 && d.max_rounds > 0,
                  "B10c it carries round/max so a surface need not know campaign_battle_params");
            check(d.stream_seed != 0,
                  "B10d it carries the battle's stream seed, so phrasing can vary per fight without a draw");
        }
    }

    // -----------------------------------------------------------------------
    std::printf("\nB11 — THE AFTERMATH SURVIVES THE ERASURE\n");
    {
        // The load-bearing property of making this a REPORT rather than a read.
        // A concluded battle is erased inside the pass, so a surface that read
        // world::battles would find nothing — and "who held the field and what it
        // cost" is the one line a player most needs.
        fixture f;
        build_fixture(f);
        engage(f);
        bool saw_conclusion = false;
        for (int i = 0; i < 10 && !saw_conclusion; ++i)
        {
            const battle_tick t = run_battles(f.w, reg, i);
            for (const battle_dispatch& d : t.dispatches)
            {
                if (d.end == campaign_battle_end::in_progress)
                    continue;
                saw_conclusion = true;
                check(d.field_held_by == f.att || d.field_held_by == f.def,
                      "B11a the concluded dispatch names WHO HELD THE FIELD");
                check(f.w.battles.empty(),
                      "B11b ...and the battle record itself is already gone, so nothing else could have said it");
                check(d.phase == battle_phase::concluded || d.phase == battle_phase::broke_off,
                      "B11c its phase reads as an ending");
            }
        }
        check(saw_conclusion, "B11  a battle concluded within ten ticks (the row was exercised)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB12 — the quoted withdrawal price is the price actually charged\n");
    {
        // A player shown one number and charged another has been lied to about
        // the only decision the fight contains. So the quote is asserted against
        // the charge rather than against arithmetic repeated in the test.
        fixture f;
        build_fixture(f);
        engage(f);
        run_battles(f.w, reg, 0);
        if (!f.w.battles.empty())
        {
            const active_battle& b = f.w.battles.front();
            const campaign_battle_params params{};
            const withdrawal_price q = quote_withdrawal(b, withdrawing_side::attacker, params);
            check(q.total == std::clamp(q.base + q.per_round + q.pursuit, 0, 1000),
                  "B12a the three terms sum to the total (they are legible, not decorative)");

            // BL-1421: the charge is IN MEN — the quoted per-mille of the side's
            // standing men, removed exactly.
            const int before = f.w.units.at(f.ua).count;
            campaign_battle_state copy = b.state;
            std::vector<land_combatant> side(1);
            side[0].unit = f.ua; side[0].men = before;
            side[0].readiness = f.w.units.at(f.ua).readiness_permille;
            const int charged = withdraw_campaign_battle(copy, withdrawing_side::attacker, side, params);
            const int quoted  = before * q.total / 1000;
            check(charged == quoted && side[0].men == before - charged,
                  "B12b THE QUOTE MATCHES THE CHARGE — the card cannot mis-state what leaving costs");
            std::printf("        quoted %d permille (base %d + per-round %d + pursuit %d)"
                        " -> %d men; charged %d\n",
                        q.total, q.base, q.per_round, q.pursuit, quoted, charged);
        }
        else
        {
            std::printf("        (battle concluded within one tick; B12 not exercised)\n");
        }
    }

    // -----------------------------------------------------------------------
    std::printf("\nB13 — the dispatch layer cannot move the simulation\n");
    {
        // BL-468 requires the text be a PURE FUNCTION of the trace, with no extra
        // draws. Asserted structurally: two runs produce identical dispatches AND
        // identical world state, so nothing about reporting can perturb the sim.
        fixture a, b;
        build_fixture(a);
        build_fixture(b);
        engage(a); engage(b);
        std::vector<battle_dispatch> da, db;
        for (int i = 0; i < 5; ++i)
        {
            const battle_tick ta = run_battles(a.w, reg, i);
            const battle_tick tb = run_battles(b.w, reg, i);
            da.insert(da.end(), ta.dispatches.begin(), ta.dispatches.end());
            db.insert(db.end(), tb.dispatches.begin(), tb.dispatches.end());
        }
        bool same = da.size() == db.size();
        for (std::size_t i = 0; same && i < da.size(); ++i)
            same = da[i].tile == db[i].tile
                && da[i].phase == db[i].phase
                && da[i].rounds_fought == db[i].rounds_fought
                && da[i].attacker_men_lost == db[i].attacker_men_lost
                && da[i].stream_seed == db[i].stream_seed;
        check(same, "B13a twin worlds emit byte-identical dispatch sequences");
        check(a.w.state_hash(5) == b.w.state_hash(5),
              "B13b ...and the worlds themselves stayed identical, so reporting moved nothing");
        std::printf("        %zu dispatches over five ticks\n", da.size());
    }

    // -----------------------------------------------------------------------
    std::printf("\nB14 — the dispatch TEXT (BL-468)\n");
    {
        // Assertable at all only because the phrase bank lives in its own TU
        // rather than in session_history.cpp, which reaches <SDL3/SDL.h> and can
        // neither be syntax-checked nor linked headless. A dispatch line is
        // exactly the sort of thing that goes wrong silently — a phase with no
        // bank, a hole where a name should be, a line crediting the wrong side.
        fixture f;
        build_fixture(f);
        engage(f);
        std::vector<battle_dispatch> seen;
        for (int i = 0; i < 10; ++i)
        {
            const battle_tick t = run_battles(f.w, reg, i);
            seen.insert(seen.end(), t.dispatches.begin(), t.dispatches.end());
        }
        check(!seen.empty(), "B14 setup: dispatches were produced");

        bool all_named = true, all_nonempty = true, no_holes = true;
        for (const battle_dispatch& d : seen)
        {
            const std::string line = battle_dispatch_line(f.w, d);
            if (line.empty()) all_nonempty = false;
            // Every token must have been substituted. NOT "contains no %" — a
            // percentage sign is legitimate output ("89%"), and asserting its
            // absence failed on correct text. The real defect is a % followed by
            // a TOKEN LETTER, which is a bank with a typo reaching the player as
            // gibberish.
            for (std::size_t i = 0; i + 1 < line.size(); ++i)
                if (line[i] == '%' && std::string("ADPadrWF").find(line[i + 1]) != std::string::npos)
                    no_holes = false;
            // Both corps are named by the corp NAME, never by a raw id.
            if (line.find("Attacker") == std::string::npos
                && line.find("Defender") == std::string::npos
                && line.find("province") == std::string::npos)
                all_named = false;
        }
        check(all_nonempty, "B14a every phase produces a line — no bank has a hole in it");
        check(no_holes, "B14b every token substitutes — no unfilled %%TOKEN reaches the player");
        check(all_named, "B14c lines name the fight rather than printing raw ids");

        // PURE FUNCTION: the same record must always produce the same line, and
        // producing one must not touch the world.
        const uint64_t h_before = f.w.state_hash(99);
        const std::string a1 = battle_dispatch_line(f.w, seen.front());
        const std::string a2 = battle_dispatch_line(f.w, seen.front());
        check(a1 == a2, "B14d the same record always says the same thing");
        check(f.w.state_hash(99) == h_before,
              "B14e saying it changed nothing — the dispatch layer cannot move the sim");

        // And it VARIES: a bank that always picks index 0 is a bank that is not
        // being folded at all, which would pass every check above.
        std::set<std::string> distinct;
        for (const battle_dispatch& d : seen) distinct.insert(battle_dispatch_line(f.w, d));
        std::printf("        %zu dispatches -> %zu distinct lines\n", seen.size(), distinct.size());
        check(distinct.size() > 1 || seen.size() <= 1,
              "B14f wording actually varies across a fight (the seed fold is live)");

        const std::string sample = battle_dispatch_line(f.w, seen.front());
        std::printf("        e.g. \"%s\"\n", sample.c_str());
    }

    // -----------------------------------------------------------------------
    std::printf("\nB15 — BL-569: a decisive close moves the province holder\n");
    {
        fixture f;
        build_fixture(f, /*declare=*/false); // discovery stays off -- the battle below is hand-pushed
        seed_province_holders(f.w);
        check(f.w.province_holder.size() == f.w.provinces.provinces.size(),
              "B15 setup: province_holder is sized to the partition (no nations in this fixture -> all no_entity)");
        check(province_holder_for(f.w, f.province) == null_entity,
              "B15 setup: the fixture's province starts with no recorded holder");

        // Hand-built battle with the DEFENDER'S UNIT SPENT (BL-1421: readiness 0).
        // A side with no unit fighting at the start of a round deals nothing and
        // is broken by it, so defender_broken is guaranteed on round 1 whatever
        // the swing draws, and the fresh attacker cannot break. By construction,
        // not by luck.
        f.w.units.at(f.ub).readiness_permille = 0;
        active_battle b;
        b.tile            = f.tile;
        b.province        = f.province;
        b.ground_tile     = f.tile;
        b.attacker        = f.att;
        b.defender        = f.def;
        b.attacker_units  = { f.ua };
        b.defender_units  = { f.ub };
        b.state.attacker_strength_permille = 1000;
        b.state.defender_strength_permille = 0;
        b.state.rounds_fought = 0;
        b.state.end           = campaign_battle_end::in_progress;
        b.state.stream_seed   = 0xB569D0u;
        b.state.rng_state     = b.state.stream_seed;
        f.w.battles.assign(1, b); // bypass discovery entirely

        const battle_tick t = run_battles(f.w, reg, 0);
        check(!t.dispatches.empty(), "B15a the hand-seeded battle stepped and reported");
        if (!t.dispatches.empty())
        {
            const battle_dispatch& d = t.dispatches.front();
            check(d.end == campaign_battle_end::defender_broken,
                  "B15b the defender broke -- guaranteed by its spent unit, not the swing draw");
            check(d.field_held_by == f.att, "B15c the attacker held the field");
            check(province_holder_for(f.w, f.province) == f.att,
                  "B15d THE PROVINCE HOLDER MOVED to the winner (the defended tile's province)");
        }
    }
    {
        // B15e (Ben, 2026-10-27): a battle is one tile, so the holder moves only
        // when the beaten side has NO unit left in the province. Same hand-pushed
        // win as B15, with a second defender stack elsewhere in the province.
        fixture f;
        build_fixture(f, /*declare=*/false);
        seed_province_holders(f.w);
        entity_id other = null_entity;
        if (const province* pr = f.w.provinces.find(f.province))
            for (const entity_id t : pr->tiles)
                if (t != f.tile && t != f.w.units.at(f.ua).position)
                {
                    other = t;
                    break;
                }
        check(other != null_entity, "B15e setup: the province has a third tile for a second defender stack");
        if (other != null_entity)
        {
            add_unit(f.w, f.def, other);
            f.w.units.at(f.ub).readiness_permille = 0;
            active_battle b;
            b.tile = f.tile; b.province = f.province; b.ground_tile = f.tile;
            b.attacker = f.att; b.defender = f.def;
            b.attacker_units = { f.ua }; b.defender_units = { f.ub };
            b.state.attacker_strength_permille = 1000;
            b.state.defender_strength_permille = 0;
            b.state.end         = campaign_battle_end::in_progress;
            b.state.stream_seed = 0xB569D1u;
            b.state.rng_state   = b.state.stream_seed;
            f.w.battles.assign(1, b);
            const battle_tick t = run_battles(f.w, reg, 0);
            check(!t.dispatches.empty() && t.dispatches.front().field_held_by == f.att,
                  "B15e the attacker held the field");
            check(province_holder_for(f.w, f.province) == null_entity,
                  "B15e the holder did NOT move: a defender stack still stands in the province");
        }
    }

    // -----------------------------------------------------------------------
    std::printf("\nB16 — BL-569: a stalemate leaves the province holder untouched\n");
    {
        fixture f;
        build_fixture(f, /*declare=*/false);
        seed_province_holders(f.w);

        // Seed a KNOWN holder distinct from either combatant, so "unchanged"
        // is a real assertion rather than "still no_entity, coincidentally".
        const entity_id third = add_corp(f.w, "ThirdParty");
        const province*  pr   = f.w.provinces.find(f.province);
        check(pr != nullptr, "B16 setup: the fixture's province resolves in the partition");
        if (pr != nullptr)
        {
            const auto idx = static_cast<std::size_t>(pr - f.w.provinces.provinces.data());
            f.w.province_holder[idx] = third;
        }

        // Hand-built battle, ONE ROUND SHORT of max_rounds (6), both units fresh
        // and equal. One round at the widest swing spends at most a few hundred
        // per-mille of either side's readiness (campaign_battle_harness C6 prints
        // a whole parity fight spending 1000 over six), so neither side can be
        // spent by round 6 and it concludes as a stalemate by construction.
        active_battle b;
        b.tile            = f.tile;
        b.province        = f.province;
        b.ground_tile     = f.tile;
        b.attacker        = f.att;
        b.defender        = f.def;
        b.attacker_units  = { f.ua };
        b.defender_units  = { f.ub };
        b.state.attacker_strength_permille = 900;
        b.state.defender_strength_permille = 900;
        b.state.rounds_fought = 5;
        b.state.end           = campaign_battle_end::in_progress;
        b.state.stream_seed   = 0xB569E0u;
        b.state.rng_state     = b.state.stream_seed;
        f.w.battles.assign(1, b);

        const battle_tick t = run_battles(f.w, reg, 0);
        check(!t.dispatches.empty(), "B16a the hand-seeded battle stepped and reported");
        if (!t.dispatches.empty())
        {
            const battle_dispatch& d = t.dispatches.front();
            check(d.end == campaign_battle_end::stalemate,
                  "B16b round 6 elapsed with both sides still standing -- guaranteed by two fresh equal units");
            check(province_holder_for(f.w, f.province) == third,
                  "B16c THE PROVINCE HOLDER STAYED PUT -- a stalemate mutates nothing here");
        }
    }

    // -----------------------------------------------------------------------
    std::printf("\nB17 — open_tile_battle is entity-id-agnostic: no stance row needed\n");
    {
        // The capability the retired mercenary trigger used (NR-731): a battle
        // between two owners with NO declare_hostile row, opened directly. The
        // march only ever opens one toward a nation it is hostile to; the opener
        // itself reads the sides as unit owners and nothing else.
        fixture f;
        build_fixture(f, /*declare=*/false);
        check(f.w.battles.empty(), "B17 setup: no battle exists before opening one");
        const bool opened = engage(f);
        check(opened, "B17a open_tile_battle opens a battle with no stance row");
        check(f.w.battles.size() == 1, "B17b exactly one battle is live");
        if (!f.w.battles.empty())
        {
            const active_battle& b = f.w.battles.front();
            check(b.attacker == f.att && b.defender == f.def && b.tile == f.tile,
                  "B17c the attacker and defender are the owners named, over the tile named");
            check(b.attacker_units == std::vector<entity_id>{ f.ua }
                      && b.defender_units == std::vector<entity_id>{ f.ub }
                      && b.attacking_units == std::vector<entity_id>{ f.ua },
                  "B17d each side carries exactly its one unit; the attacking stack is the attacker's");
        }
        const int before = total_men(f.w);
        bool concluded = false;
        for (int i = 0; i < 10 && !concluded; ++i)
        {
            const battle_tick bt = run_battles(f.w, reg, i);
            for (const battle_dispatch& d : bt.dispatches)
                if (d.end != campaign_battle_end::in_progress)
                    concluded = true;
        }
        check(concluded, "B17e the battle RESOLVED within ten ticks");
        check(f.w.battles.empty(), "B17f ...and is erased on conclusion");
        check(total_men(f.w) < before, "B17g men died through the real step");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB18 — BL-1421: a spent unit retreats one tile away and leaves the fight\n");
    {
        fixture f;
        build_fixture(f);
        engage(f);
        const entity_id start = f.w.units.at(f.ua).position;
        bool retreated = false;
        int morale_after = 1000;
        for (int i = 0; i < 10 && !retreated; ++i)
        {
            run_battles(f.w, reg, i);
            const unit_component& u = f.w.units.at(f.ua);
            if (u.position != start)
            {
                retreated   = true;
                morale_after = u.morale_permille;
            }
        }
        // Parity assault on open ground: the attacker is spent (C6), so it goes.
        check(retreated, "B18a the spent attacker moved off its tile (R2: readiness 0 retreats)");
        if (retreated)
        {
            const unit_component& u = f.w.units.at(f.ua);
            const int side = road_side_between(f.w, start, u.position);
            check(side >= 0 && road_side_is_crossable(side, f.w.tiles.at(start).grid_y),
                  "B18b ...exactly one tile, across a side a march can cross");
            check(!unit_in_battle(f.w, f.ua), "B18c ...and it is out of the fight");
            check(morale_after < 1000, "B18d retreating cost it morale (R4)");
        }
    }

    // -----------------------------------------------------------------------
    std::printf("\nB19 — BL-1421: the winner gains morale, the loser loses it\n");
    {
        // B15's construction: a spent defender, so the attacker wins on round 1.
        fixture f;
        build_fixture(f, /*declare=*/false);
        f.w.units.at(f.ua).morale_permille = 800;
        f.w.units.at(f.ub).morale_permille = 800;
        f.w.units.at(f.ub).readiness_permille = 0;
        active_battle b;
        b.tile = f.tile; b.province = f.province; b.attacker = f.att; b.defender = f.def;
        b.attacker_units = { f.ua }; b.defender_units = { f.ub };
        b.state.end = campaign_battle_end::in_progress;
        b.state.stream_seed = 0xB1421u; b.state.rng_state = b.state.stream_seed;
        f.w.battles.assign(1, b);
        const battle_tick t = run_battles(f.w, reg, 0);
        const campaign_battle_params p{};
        check(!t.dispatches.empty() && t.dispatches.front().end == campaign_battle_end::defender_broken,
              "B19 setup: the spent defender broke");
        // The winner's +win lands (recovery is quarterly since BL-1445, so nothing
        // moves it back within the day) — so it is above where it started.
        check(f.w.units.at(f.ua).morale_permille >= 800 + p.morale_win,
              "B19a the field's holder gained morale");
        check(f.w.units.at(f.ub).morale_permille < 800,
              "B19b the broken side lost morale");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB20 — BL-1421: out of contact, readiness recovers toward morale\n");
    {
        fixture f;
        build_fixture(f, /*declare=*/false);
        unit_component& u = f.w.units.at(f.ua);
        u.readiness_permille = 0;
        u.morale_permille    = 700;
        u.supply_factor_permille = 1000;
        int prev = 0;
        bool rising = true;
        for (int i = 0; i < 12; ++i)
        {
            // BL-1445: recovery is QUARTERLY (the economy step), not a step of the
            // daily battle pass: a quarter's recovery, twelve times.
            recover_out_of_contact(f.w);
            const int r = f.w.units.at(f.ua).readiness_permille;
            if (r < prev) rising = false;
            prev = r;
        }
        const unit_component& after = f.w.units.at(f.ua);
        std::printf("        readiness 0 -> %d, morale 700 -> %d over twelve idle quarters\n",
                    after.readiness_permille, after.morale_permille);
        check(rising && after.readiness_permille > 0, "B20a readiness climbs back while out of contact");
        check(after.readiness_permille <= after.morale_permille,
              "B20b ...never above morale");
        check(after.morale_permille > 700, "B20c a fully supplied unit's morale climbs toward its supply");

        unit_component& hungry = f.w.units.at(f.ub);
        hungry.supply_factor_permille = 400;
        recover_out_of_contact(f.w);
        check(f.w.units.at(f.ub).morale_permille < 1000, "B20d an unsupplied unit's morale falls toward its supply");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB21 — BL-1421: a world that never fights is unmoved\n");
    {
        fixture f;
        build_fixture(f, /*declare=*/false);
        const uint64_t h0 = f.w.state_hash(0);
        for (int i = 0; i < 5; ++i) { run_battles(f.w, reg, i); recover_out_of_contact(f.w); }
        check(f.w.state_hash(0) == h0,
              "B21a fully supplied units at full readiness and morale hash exactly as before (R6)");

        // The counter-case, stated so R6's scope is honest: an UNSUPPLIED unit's
        // morale drifts toward its supply even with no fight, and the hash moves.
        f.w.units.at(f.ua).supply_factor_permille = 900;
        const uint64_t h1 = f.w.state_hash(0);
        recover_out_of_contact(f.w); // a quarter's recovery
        check(f.w.state_hash(0) != h1 && f.w.units.at(f.ua).morale_permille == 990,
              "B21b an unsupplied unit's morale moves without a fight (10 toward its supply), so the hash does");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB22 — a unit in two battles retreats once, not twice\n");
    {
        // One spent unit of A listed in two battles at once, (P,A,B) and (P,A,C).
        fixture f;
        build_fixture(f, /*declare=*/false);
        const entity_id third = add_corp(f.w, "Third");
        const entity_id t = f.w.units.at(f.ua).position;
        const entity_id uc = add_unit(f.w, third, tile_at(f.w, f.body, 2, 3)); // its own tile
        f.w.units.at(f.ua).readiness_permille = 0;
        auto battle_vs = [&](entity_id def, entity_id du, uint64_t seed) {
            active_battle b;
            b.tile = f.w.units.at(du).position; b.province = f.province;
            b.attacker = f.att; b.defender = def;
            b.attacker_units = { f.ua }; b.defender_units = { du };
            b.state.end = campaign_battle_end::in_progress;
            b.state.stream_seed = seed; b.state.rng_state = seed;
            return b;
        };
        f.w.battles = { battle_vs(f.def, f.ub, 0xB22A), battle_vs(third, uc, 0xB22B) };
        std::sort(f.w.battles.begin(), f.w.battles.end(), battle_order_less);
        run_battles(f.w, reg, 0);
        const campaign_battle_params p{};
        const unit_component& u = f.w.units.at(f.ua);
        const int side = road_side_between(f.w, t, u.position);
        std::printf("        morale 1000 -> %d; moved %s\n", u.morale_permille, side >= 0 ? "one tile" : "elsewhere");
        check(u.morale_permille == 1000 - p.morale_loss,
              "B22a it paid the morale loss ONCE (the daily pass recovers nothing, BL-1445)");
        check(side >= 0, "B22b it moved one tile, not two");
        const std::set<entity_id> foes = { f.w.units.at(f.ub).position, f.w.units.at(uc).position };
        check(foes.count(u.position) == 0, "B22c it did not retreat onto either foe's tile");
        recover_out_of_contact(f.w);
        check(f.w.units.at(f.ua).morale_permille == 1000 - p.morale_loss + p.morale_recovery_permille,
              "B22d ...and the quarter's recovery then moves it one step back, once");
    }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    std::printf("%s\n", failures == 0 ? "ALL PASS (0 failures)" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
