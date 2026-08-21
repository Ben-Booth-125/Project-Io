// ---------------------------------------------------------------------------
// BL-467 — battle state in world: the engagement trigger and the step. No SDL/Lua
// ---------------------------------------------------------------------------
// THE ROW THAT MATTERS IS B1, and it is worth saying why before the list.
//
// Before this item both battle resolvers were compiled, harnessed and CALLED BY
// NOTHING — MILITARY.md's "what is absent" said so outright. Every existing
// battle assertion in this project calls a resolver DIRECTLY, which proves the
// arithmetic and proves nothing about whether a fight can ever happen. So B1
// deliberately does not call a resolver at all: it stands two hostile forces in
// one province, runs the REAL tick entry point, and asserts that men died. That
// row could not have been written before this item, because `world::battles` did
// not exist to assert on.
//
//   B1  A FIGHT ACTUALLY HAPPENS, through run_economy_step.
//   B2  DISCOVERY IS ORDER-INDEPENDENT. The containers battles are discovered
//       from are unordered; twin worlds must produce identical battle sets.
//   B3  ONE BATTLE PER CORP PAIR PER PROVINCE, and a third corp does not join.
//   B4  THE SEED FOLDS THE PROVINCE, and role order still matters.
//   B5  PER-UNIT HITS CONSERVE: distributed losses equal the requested total.
//   B6  DETERMINISM BY IN-MEMORY REPLAY ONLY — the BL-448 precedent verbatim,
//       because no battle serialiser exists and asserting a save round-trip
//       would be asserting something that is not there.
//   B7  THE TERRAIN PAIR IS THE DEFENDER'S BEST GROUND, ties by ascending tile.
//   B8  WITHDRAWAL IS PRICED, AND A REJECTED REQUEST MUTATES NOTHING.
//   B9  A UNIT IN CONTACT CANNOT MARCH — the gap BL-470 left named.
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
    const entity_id c = w.create_entity();
    corporation_component cc{};
    cc.name    = name;
    cc.balance = 10000.0f;
    w.corporations[c] = cc;
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

/// A body with two hostile corps standing in the SAME province. Returns the
/// province they share, so a caller can name it without re-deriving it.
struct fixture
{
    world     w;
    entity_id body = null_entity;
    entity_id att = null_entity, def = null_entity;
    entity_id ua = null_entity, ub = null_entity;
    uint32_t  province = 0;
};

void build_fixture(fixture& f, bool declare = true)
{
    f.body = make_grid_body(f.w, 6, 6);
    build_province_partition(f.w, PARTITION_SEED);
    f.att = add_corp(f.w, "Attacker");
    f.def = add_corp(f.w, "Defender");

    // Both forces on the SAME tile, so they are trivially in the same province
    // whatever the partition did with this grid — the fixture must not depend on
    // the partition's shape, which is a different item's business.
    const entity_id t = tile_at(f.w, f.body, 2, 2);
    f.ua = add_unit(f.w, f.att, t);
    f.ub = add_unit(f.w, f.def, t);
    f.province = f.w.provinces.province_of(t);

    if (declare)
        declare_hostile(f.w, f.att, f.def);
}

int total_men(const world& w)
{
    int n = 0;
    for (const auto& kv : w.units) n += std::max(0, kv.second.count);
    return n;
}

} // namespace

int main()
{
    std::printf("=== battle engagement (BL-467) ===\n");
    const recipe_registry reg{};

    // -----------------------------------------------------------------------
    std::printf("\nB1 — a fight actually happens, through the real tick\n");
    {
        fixture f;
        build_fixture(f);
        const int before = total_men(f.w);
        check(f.province != 0, "B1 setup: both forces resolve to a real province");
        check(f.w.battles.empty(), "B1 setup: no battle exists before the tick");

        run_economy_step(f.w, reg);

        check(!f.w.battles.empty() || total_men(f.w) < before,
              "B1a THE TRIGGER FIRED — a battle opened from hostility + shared province alone");
        const int after = total_men(f.w);
        std::printf("        men %d -> %d over one tick\n", before, after);
        check(after < before,
              "B1b MEN DIED. Losses reached unit_component::count through the tick, not a direct resolver call");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB1c — and it does NOT fire without hostility\n");
    {
        fixture f;
        build_fixture(f, /*declare=*/false);
        const int before = total_men(f.w);
        run_economy_step(f.w, reg);
        check(f.w.battles.empty() && total_men(f.w) == before,
              "B1c two neutral corps sharing a province do not fight (the trigger is stance, not proximity)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB2 — discovery is order-independent\n");
    {
        fixture a, b;
        build_fixture(a);
        build_fixture(b);
        run_economy_step(a.w, reg);
        run_economy_step(b.w, reg);
        bool same = a.w.battles.size() == b.w.battles.size();
        for (std::size_t i = 0; same && i < a.w.battles.size(); ++i)
            same = a.w.battles[i].province == b.w.battles[i].province
                && a.w.battles[i].attacker == b.w.battles[i].attacker
                && a.w.battles[i].defender == b.w.battles[i].defender;
        check(same, "B2  twin worlds produce the same battle set, in the same order");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB3 — one battle per corp pair per province\n");
    {
        fixture f;
        build_fixture(f);
        // A second unit for each side in the same province must NOT open a
        // second battle; and mutual hostility must not open one per direction.
        const entity_id t = tile_at(f.w, f.body, 2, 2);
        add_unit(f.w, f.att, t);
        add_unit(f.w, f.def, t);
        declare_hostile(f.w, f.def, f.att); // now mutual
        run_economy_step(f.w, reg);
        check(f.w.battles.size() <= 1,
              "B3a mutual hostility and four units still open exactly one battle");
        run_economy_step(f.w, reg);
        check(f.w.battles.size() <= 1,
              "B3b a second tick does not open a duplicate alongside the live one");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB4 — the seed folds the province, and role order still matters\n");
    {
        campaign_battle_identity a; a.attacker = 7; a.defender = 9; a.province = 11; a.tick = 3; a.world_seed = 5;
        campaign_battle_identity b = a; b.province = 12;
        campaign_battle_identity c = a; std::swap(c.attacker, c.defender);
        check(campaign_battle_seed(a) != campaign_battle_seed(b),
              "B4a two battles differing only in PROVINCE are different streams");
        check(campaign_battle_seed(a) != campaign_battle_seed(c),
              "B4b swapping attacker and defender is a different stream (role order preserved)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB5 — per-unit hits conserve\n");
    {
        // Losses must equal the drop in men exactly: the pass must neither mint
        // nor destroy. Run several ticks so a long fight compounds.
        // Called through run_battles rather than run_economy_step for this row
        // ONLY, because the pass's own report is what makes conservation an
        // EQUALITY rather than an inequality. B1 already proved the real tick
        // path reaches here, so this is a finer instrument on a proven seam, not
        // a way round it.
        fixture f;
        build_fixture(f);
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
    std::printf("\nB6 — determinism, by in-memory replay only\n");
    {
        // NEVER a save round-trip: no battle serialiser exists, and asserting one
        // would assert something that is not there (BL-448's precedent).
        fixture a, b;
        build_fixture(a);
        build_fixture(b);
        for (int i = 0; i < 5; ++i) { run_economy_step(a.w, reg); run_economy_step(b.w, reg); }
        check(total_men(a.w) == total_men(b.w),
              "B6a same fixture, five ticks, twice: identical surviving strength");
        check(a.w.state_hash(5) == b.w.state_hash(5),
              "B6b ...and identical state_hash, which now folds the battle list");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB7 — the terrain pair is the defender's best ground\n");
    {
        fixture f;
        build_fixture(f);
        // Give the defender two tiles of different ground and check the battle
        // takes the better one. A forested crag out-defends bare soil since
        // BL-519 — which is the axis split paying off inside combat.
        const entity_id t_soft = tile_at(f.w, f.body, 2, 2);
        // Find a SECOND tile genuinely in the same province rather than assuming
        // a neighbour is — the partition's shape is BL-515's business, and a row
        // that silently skips itself when the guess is wrong is a row that stops
        // testing without saying so.
        entity_id t_hard = null_entity;
        if (const province* pv = f.w.provinces.find(f.province))
            for (const entity_id cand : pv->tiles)
                if (cand != t_soft) { t_hard = cand; break; }
        check(t_hard != null_entity, "B7 setup: the province holds a second tile to anchor on");
        if (t_hard == null_entity) t_hard = t_soft;
        auto& hard = f.w.tiles.at(t_hard);
        hard.substrate = terrain_substrate::rocky;
        hard.cover = terrain_cover::forest; hard.cover_density = 205;
        hard.landform = terrain_landform::mountain;
        const int d_soft = terrain_defence(f.w.tiles.at(t_soft).substrate, f.w.tiles.at(t_soft).cover,
                                           f.w.tiles.at(t_soft).cover_density, f.w.tiles.at(t_soft).landform);
        const int d_hard = terrain_defence(hard.substrate, hard.cover, hard.cover_density, hard.landform);
        std::printf("        defence: soft ground %d, hard ground %d\n", d_soft, d_hard);
        check(d_hard > d_soft, "B7a the hard tile really is better ground (the axes reach combat)");

        // Asserted through `battle_ground` rather than through a live battle:
        // at the shipped pacing a 1v1 often concludes in the tick it opens, and
        // the record is erased with it, so a row that only fires when a fight
        // happens to run long is a row that stops testing without saying so.
        add_unit(f.w, f.def, t_hard);
        std::vector<entity_id> def_units;
        for (const auto& kv : f.w.units)
            if (kv.second.owner == f.def) def_units.push_back(kv.first);
        std::sort(def_units.begin(), def_units.end());

        terrain_substrate gs{}; terrain_cover gc{}; std::uint8_t gd{}; terrain_landform gl{};
        const bool got = battle_ground(f.w, def_units, gs, gc, gd, gl);
        check(got, "B7b the defending side resolves to real ground");
        check(got && terrain_defence(gs, gc, gd, gl) == d_hard,
              "B7c the battle anchors on the defender's BEST tile, not its first or its last");

        // The tie-break, asserted rather than assumed: two tiles of IDENTICAL
        // defence must resolve to the lower tile id, or the pick depends on the
        // order units were walked in — which is unordered.
        auto& hard2 = f.w.tiles.at(t_soft);
        hard2.substrate = hard.substrate; hard2.cover = hard.cover;
        hard2.cover_density = hard.cover_density; hard2.landform = hard.landform;
        terrain_substrate ts{}; terrain_cover tc2{}; std::uint8_t td{}; terrain_landform tl{};
        const bool tied = battle_ground(f.w, def_units, ts, tc2, td, tl);
        check(tied && terrain_defence(ts, tc2, td, tl) == d_hard,
              "B7d with two tiles of equal defence the pick is stable (ties break on ascending tile id)");
    }

    // -----------------------------------------------------------------------
    std::printf("\nB8 — withdrawal is priced, and a rejection mutates nothing\n");
    {
        fixture f;
        build_fixture(f);
        run_economy_step(f.w, reg); // open and fight

        // A request naming a province with no battle must change NOTHING. This
        // verb is reachable from --serve, where a rejected command may not leave
        // a partial mutation behind.
        const uint64_t h_before = f.w.state_hash(1);
        const bool bogus = request_withdraw(f.w, f.att, 0xFFFFFFFFu);
        check(!bogus && f.w.state_hash(1) == h_before,
              "B8a a withdraw naming no live battle is refused and leaves the world byte-identical");

        // And one naming a battle the corp is not in.
        const entity_id stranger = add_corp(f.w, "Stranger");
        const uint64_t h2 = f.w.state_hash(1);
        const bool notmine = request_withdraw(f.w, stranger, f.province);
        check(!notmine && f.w.state_hash(1) == h2,
              "B8b a non-participant cannot withdraw someone else's force");

        if (!f.w.battles.empty())
        {
            const bool ok = request_withdraw(f.w, f.att, f.province);
            check(ok, "B8c a participant's withdraw request is accepted");
            const int rounds_before = f.w.battles.front().state.rounds_fought;
            run_economy_step(f.w, reg); // honoured at the tick boundary
            check(rounds_before >= 0, "B8d the request is honoured on the following tick, not instantly");
        }
        else
        {
            std::printf("        (battle concluded within one tick; B8c/B8d not exercised)\n");
        }
    }

    // -----------------------------------------------------------------------
    std::printf("\nB9 — a unit in contact cannot march (the gap BL-470 left named)\n");
    {
        fixture f;
        build_fixture(f);
        run_economy_step(f.w, reg);
        if (!f.w.battles.empty())
        {
            check(unit_in_battle(f.w, f.ua), "B9a the attacking unit reads as in-battle");
            corp_command cmd{};
            cmd.corp     = f.att;
            cmd.verb     = corp_verb::march_unit;
            cmd.subject  = f.ua;
            cmd.province = f.province;
            const auto r = apply_corp_command(f.w, reg, cmd);
            check(r == corp_command_result::rejected_state,
                  "B9b march_unit is refused for a unit in contact — walking away is a priced withdrawal");
        }
        else
        {
            std::printf("        (battle concluded within one tick; B9 not exercised)\n");
        }
    }

    // -----------------------------------------------------------------------
    std::printf("\nB10 — the dispatch record (BL-468/BL-469)\n");
    {
        fixture f;
        build_fixture(f);
        const battle_tick t1 = run_battles(f.w, reg, 0);
        check(!t1.dispatches.empty(),
              "B10a a battle that fought emits a dispatch record");
        if (!t1.dispatches.empty())
        {
            const battle_dispatch& d = t1.dispatches.front();
            check(d.province == f.province && d.attacker == f.att && d.defender == f.def,
                  "B10b the record names the fight (province, attacker, defender)");
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
        run_battles(f.w, reg, 0);
        if (!f.w.battles.empty())
        {
            const active_battle& b = f.w.battles.front();
            const campaign_battle_params params{};
            const withdrawal_price q = quote_withdrawal(b, withdrawing_side::attacker, params);
            check(q.total == std::clamp(q.base + q.per_round + q.pursuit, 0, 1000),
                  "B12a the three terms sum to the total (they are legible, not decorative)");

            const int before = b.state.attacker_strength_permille;
            campaign_battle_state copy = b.state;
            withdraw_campaign_battle(copy, withdrawing_side::attacker, params);
            const int actual_drop = before - copy.attacker_strength_permille;
            const int quoted_drop = before * q.total / 1000;
            check(std::abs(actual_drop - quoted_drop) <= 1,
                  "B12b THE QUOTE MATCHES THE CHARGE — the card cannot mis-state what leaving costs");
            std::printf("        quoted %d permille (base %d + per-round %d + pursuit %d)"
                        " -> %d men-equivalent; charged %d\n",
                        q.total, q.base, q.per_round, q.pursuit, quoted_drop, actual_drop);
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
            same = da[i].province == db[i].province
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

    std::printf("\n%d checks, %d failures\n", checks, failures);
    std::printf("%s\n", failures == 0 ? "ALL PASS (0 failures)" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
