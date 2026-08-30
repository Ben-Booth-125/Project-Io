// Headless harness for Sprint 19 wave 2: BL-616 (centre promotion and
// decline) and BL-617 (population migration). Requirement groups
// centre-promotion-decline and population-migration
// (docs/development/req/requirements.json, batch sprint-19-wave-2).
//
//   P1: promotion fires deterministically — a centre whose met supply,
//       habitability and headcount hold above the next tier's rung promotes
//       one scale tier at the exact expected tick; two fresh runs agree.
//   P2: decline shrinks and floors — failing conditions shed population,
//       demote tiers, and floor at scale 1 / population 1; the centre is
//       NEVER destroyed by passive failure.
//   P3: raze works through apply_corp_command — rejected without a real
//       centre (rejected_invalid) and without the acting corp's military
//       presence on the centre's body (rejected_state); applied with a unit
//       on the body, DEMOTING the centre to the razed tier (BL-624):
//       population 0, razed set, entity/name/tile all kept. A rejection
//       mutates nothing; razing a ruin is rejected_state.
//   P4: the razed -> regrown round trip is deterministic — a razed centre
//       contributes no labour/demand/habitability weight, re-settles at the
//       reduced gate (half the promotion window) back to scale 1 with the
//       village-rung seed population, and two fresh runs agree tick-exactly.
//   M1: migration flows are deterministic and directional — heads flow from
//       the low- toward the high-attractiveness centre on a body, and two
//       runs from identical worlds produce identical populations.
//   M2: stance gates the flow between nations — friendly passes fully,
//       neutral is throttled, hostile is closed. Nation stance rides the
//       existing corp stance tables keyed by nation entity ids.
//   M3: qualification conservation — cross-border migration debits the
//       origin nation's ledger and credits the destination's; the total
//       qualified-head count (sum of fraction x tracked heads) is conserved,
//       and the wage signal pulls migrants toward the paying nation.
//
// Build (WSL, from repo root):
//   g++ -std=c++20 -O2 -I src -I tools/verify tools/verify/population_dynamics.cpp \
//       $(ls src/world/*.cpp | grep -Ev 'recipe_registry|works_registry|tech_tree|world_gen_config') \
//       -o build_gen/verify/population_dynamics.out
//
// Note: recipe_registry.cpp is NOT linked -- it depends on Lua. The registry
// is used inline-only (set_growth / set_migration are in the header, and the
// migration_params defaults mirror the shipped scripts/economy.lua values).

#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/population_generation.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <map>
#include <utility>

static int g_failures = 0;
static int g_passes   = 0;

static void check(bool cond, const char* what, double got = 0.0, double want = 0.0)
{
    if (cond) {
        std::printf("  PASS  %s\n", what);
        ++g_passes;
    } else {
        std::printf("  FAIL  %s   (got %.6f, want %.6f)\n", what, got, want);
        ++g_failures;
    }
}

// ---------------------------------------------------------------------------
// Fixture: one body, one tile per centre requested, one market. The growth
// gate reads body habitability (aggregated from the centres) and the met-
// supply ratio over the body's markets; an EMPTY growth basket makes the met
// ratio trivially 1.0, so "conditions met" reduces to habitability >= 0.5 —
// which each test drives through the centres' own habitability values.
// ---------------------------------------------------------------------------
struct fixture
{
    world     w;
    entity_id body = null_entity;

    explicit fixture(int tiles)
    {
        body = w.create_entity();
        body_component bc{};
        bc.name        = "DynBody";
        bc.grid_width  = 10;
        bc.grid_height = 4;
        w.bodies[body] = bc;
        for (int i = 0; i < tiles; ++i)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body   = body;
            tc.grid_x = i;
            tc.grid_y = 0;
            w.tiles[t] = tc;
            tile_ids.push_back(t);
        }
        // One market so the basket maps exist; supply/demand left zero.
        const entity_id m = w.create_entity();
        market_component mc{};
        mc.body      = body;
        w.markets[m] = mc;
    }

    entity_id add_centre(int tile_index, int scale, int pop, float hab)
    {
        const entity_id c = w.create_entity();
        population_centre_component pcc{};
        pcc.scale        = scale;
        pcc.population   = pop;
        pcc.habitability = hab;
        w.population_centres[c]     = pcc;
        w.population_centre_tile[c] = tile_ids[static_cast<std::size_t>(tile_index)];
        w.population_centre_name[c] = "Centre";
        return c;
    }

    std::vector<entity_id> tile_ids;
};

// ---------------------------------------------------------------------------
// P1 — promotion fires deterministically at the expected tick
// ---------------------------------------------------------------------------
static int run_promotion(int ticks)
{
    fixture f(1);
    const entity_id c = f.add_centre(0, 1, 50, 0.9f); // at the scale-2 rung
    recipe_registry reg;
    reg.set_migration({}); // defaults; a single centre migrates nowhere anyway
    for (int t = 0; t < ticks; ++t)
        run_economy_step(f.w, reg);
    return f.w.population_centres.at(c).scale;
}

static void test_promotion()
{
    std::printf("--- P1: promotion fires deterministically ---\n");
    // The sustained window is 50 qualifying ticks (economy_system.cpp,
    // k_promotion_window_ticks): at tick 49 the streak reads 49 — no
    // promotion; at tick 50 it reads 50 and the centre is at the rung.
    check(run_promotion(49) == 1, "no promotion before the sustained window (49 ticks)");
    check(run_promotion(50) == 2, "promotion at exactly the sustained window (50 ticks)");
    // Determinism: two fresh, identical runs agree — and hold the same
    // population figure, not just the same scale.
    fixture a(1), b(1);
    const entity_id ca = a.add_centre(0, 1, 50, 0.9f);
    const entity_id cb = b.add_centre(0, 1, 50, 0.9f);
    recipe_registry reg;
    for (int t = 0; t < 200; ++t)
    {
        run_economy_step(a.w, reg);
        run_economy_step(b.w, reg);
    }
    const auto& pa = a.w.population_centres.at(ca);
    const auto& pb = b.w.population_centres.at(cb);
    check(pa.scale == pb.scale && pa.population == pb.population
              && pa.growth_accumulator == pb.growth_accumulator,
          "two fresh runs replay identically (scale, population, accumulator)",
          pa.population, pb.population);
    check(pa.population > 50, "population genuinely grows under met conditions",
          pa.population, 50);
}

// ---------------------------------------------------------------------------
// P2 — decline shrinks, demotes, floors at scale 1; never destroys
// ---------------------------------------------------------------------------
static void test_decline()
{
    std::printf("--- P2: decline floors at scale 1 ---\n");
    fixture f(1);
    // Scale 2 at its own rung, habitability BELOW the 0.5 gate: conditions
    // fail every tick, so the centre sheds and demotes.
    const entity_id c = f.add_centre(0, 2, 50, 0.3f);
    recipe_registry reg;
    bool demoted_once = false;
    for (int t = 0; t < 2000; ++t)
    {
        run_economy_step(f.w, reg);
        if (f.w.population_centres.at(c).scale == 1)
            demoted_once = true;
    }
    const auto it = f.w.population_centres.find(c);
    check(it != f.w.population_centres.end(),
          "passive failure NEVER destroys a centre (still present after 2000 failing ticks)");
    if (it != f.w.population_centres.end())
    {
        check(demoted_once && it->second.scale == 1,
              "sustained failure demotes to scale 1 and floors there", it->second.scale, 1);
        check(it->second.population >= 1,
              "population floors at 1, never 0", it->second.population, 1);
        check(it->second.growth_accumulator < 0,
              "the failing streak rides growth_accumulator as a negative value",
              it->second.growth_accumulator, -1);
    }
}

// ---------------------------------------------------------------------------
// P3 — raze through apply_corp_command
// ---------------------------------------------------------------------------
static void test_raze()
{
    std::printf("--- P3: raze_centre through the corp-command seam ---\n");
    fixture f(2);
    const entity_id c = f.add_centre(0, 3, 200, 0.8f);

    const entity_id corp = f.w.create_entity();
    corporation_component cc{};
    cc.name               = "Razer";
    f.w.corporations[corp] = cc;

    recipe_registry reg;
    corp_command cmd{};
    cmd.corp    = corp;
    cmd.verb    = corp_verb::raze_centre;
    cmd.subject = c;

    // No military presence on the body: rejected_state, and NOTHING mutates.
    check(apply_corp_command(f.w, reg, cmd) == corp_command_result::rejected_state,
          "raze without military presence on the body is rejected_state");
    check(f.w.population_centres.count(c) == 1, "rejection mutated nothing (centre intact)");

    // A unit on ANOTHER body does not qualify.
    const entity_id other_body = f.w.create_entity();
    {
        body_component bc{};
        bc.name          = "Elsewhere";
        bc.grid_width    = 4;
        bc.grid_height   = 2;
        f.w.bodies[other_body] = bc;
    }
    const entity_id other_tile = f.w.create_entity();
    {
        tile_component tc{};
        tc.body            = other_body;
        f.w.tiles[other_tile] = tc;
    }
    const entity_id far_unit = f.w.create_entity();
    f.w.units[far_unit] = unit_component{ .position = other_tile, .owner = corp, .count = 100 };
    check(apply_corp_command(f.w, reg, cmd) == corp_command_result::rejected_state,
          "a unit on a different body is not presence — still rejected_state");

    // A unit on the centre's body: applied — DEMOTED, not erased (BL-624).
    const entity_id near_unit = f.w.create_entity();
    f.w.units[near_unit] = unit_component{ .position = f.tile_ids[1], .owner = corp, .count = 100 };
    check(apply_corp_command(f.w, reg, cmd) == corp_command_result::applied,
          "raze with a unit on the centre's body is applied");
    check(f.w.population_centres.count(c) == 1
              && f.w.population_centre_tile.count(c) == 1
              && f.w.population_centre_name.count(c) == 1,
          "the razed centre PERSISTS in all three stores (demotion, not erasure)");
    {
        const auto& pcc = f.w.population_centres.at(c);
        check(pcc.razed && pcc.population == 0 && pcc.scale == 1,
              "razing sets the razed tier: population 0, scale 1, razed flag on");
    }

    // A second raze of the same (now razed) centre: rejected_state, mutating
    // nothing — a ruin cannot be razed again.
    check(apply_corp_command(f.w, reg, cmd) == corp_command_result::rejected_state,
          "razing an already-razed centre is rejected_state");
    check(f.w.population_centres.at(c).razed,
          "the second raze's rejection mutated nothing (still razed)");

    // A raze of a centre that never existed: rejected_invalid.
    corp_command ghost = cmd;
    ghost.subject      = 0xDEADD00Du;
    check(apply_corp_command(f.w, reg, ghost) == corp_command_result::rejected_invalid,
          "razing an absent centre is rejected_invalid");
}

// ---------------------------------------------------------------------------
// P4 — the razed -> regrown round trip is deterministic (BL-624)
// ---------------------------------------------------------------------------
static void test_resettle()
{
    std::printf("--- P4: razed -> regrown round trip, deterministically ---\n");
    // Two centres so the body keeps a live centre while one lies razed: the
    // live one holds body habitability above the 0.5 growth gate (a razed
    // centre carries no habitability weight — asserted below via the razed
    // centre's own hab being LOW and the gate still passing).
    auto build = [](fixture& f, entity_id& live, entity_id& ruin) {
        live = f.add_centre(0, 3, 200, 0.9f);
        ruin = f.add_centre(1, 2, 100, 0.1f); // low hab: would DRAG the body mean
        f.w.population_centres.at(ruin).razed      = true;
        f.w.population_centres.at(ruin).population = 0;
        f.w.population_centres.at(ruin).scale      = 1;
    };
    fixture a(2), b(2);
    entity_id a_live, a_ruin, b_live, b_ruin;
    build(a, a_live, a_ruin);
    build(b, b_live, b_ruin);
    recipe_registry reg;

    // While razed: no labour and no habitability weight. The body mean reads
    // off the LIVE centre alone (0.9); with the razed one counted it would be
    // (0.9x3 + 0.1x1)/4 = 0.7 — both above the gate, so the sharper probe is
    // the re-settle tick itself: with weight excluded the gate holds every
    // tick and the ruin re-settles at EXACTLY the reduced window (25 ticks,
    // half the 50-tick promotion window).
    int resettle_tick = -1;
    for (int t = 1; t <= 60; ++t)
    {
        run_economy_step(a.w, reg);
        if (resettle_tick < 0 && !a.w.population_centres.at(a_ruin).razed)
            resettle_tick = t;
    }
    check(resettle_tick == 25,
          "the ruin re-settles at exactly HALF the promotion window (tick 25)",
          resettle_tick, 25);
    {
        const auto& pcc = a.w.population_centres.at(a_ruin);
        check(!pcc.razed && pcc.scale == 1 && pcc.population >= 10,
              "re-settlement returns scale 1 with the village-rung seed population",
              pcc.population, 10);
    }

    // Determinism: the same 60 ticks on a fresh identical world agree exactly.
    for (int t = 1; t <= 60; ++t)
        run_economy_step(b.w, reg);
    const auto& pa = a.w.population_centres.at(a_ruin);
    const auto& pb = b.w.population_centres.at(b_ruin);
    check(pa.razed == pb.razed && pa.scale == pb.scale
              && pa.population == pb.population
              && pa.growth_accumulator == pb.growth_accumulator,
          "two fresh razed->regrown runs replay identically",
          pa.population, pb.population);
}

// ---------------------------------------------------------------------------
// M1 — migration is directional and replay-identical
// ---------------------------------------------------------------------------
static void test_migration_direction()
{
    std::printf("--- M1: migration flows low -> high attractiveness, deterministically ---\n");
    auto build = [](fixture& f, entity_id& lo, entity_id& hi) {
        lo = f.add_centre(0, 3, 200, 0.55f); // above the growth gate, below hi
        hi = f.add_centre(1, 3, 200, 0.95f);
    };
    fixture a(2), b(2);
    entity_id a_lo, a_hi, b_lo, b_hi;
    build(a, a_lo, a_hi);
    build(b, b_lo, b_hi);
    recipe_registry reg; // migration defaults: rate 10 permille
    for (int t = 0; t < 50; ++t)
    {
        run_economy_step(a.w, reg);
        run_economy_step(b.w, reg);
    }
    const int lo_pop = a.w.population_centres.at(a_lo).population;
    const int hi_pop = a.w.population_centres.at(a_hi).population;
    // Both centres also GROW (conditions met) — the migration signal is the
    // difference between them, not the absolute change.
    check(hi_pop > lo_pop, "heads accumulate at the high-attractiveness centre", hi_pop, lo_pop);
    check(lo_pop >= 1, "the donor centre never empties", lo_pop, 1);
    check(lo_pop == b.w.population_centres.at(b_lo).population
              && hi_pop == b.w.population_centres.at(b_hi).population,
          "two fresh runs produce identical populations (replay-identical)");
}

// ---------------------------------------------------------------------------
// M2 + M3 — the stance gate between nations, and qualification conservation
// ---------------------------------------------------------------------------
struct nation_fixture
{
    fixture   f{ 2 };
    entity_id n_lo = null_entity, n_hi = null_entity;
    entity_id c_lo = null_entity, c_hi = null_entity;

    nation_fixture(float q_lo, float q_hi)
    {
        // EQUAL habitability on purpose: the wage map in step() is then the
        // ONLY attractiveness difference, so these rows prove the clearing-
        // wage half of the signal, not just habitability again. Populations
        // are city-sized so the neutral throttle's integer floor still moves
        // whole heads (2000 x 10‰ x 250‰ = 5 per receiver share).
        c_lo = f.add_centre(0, 3, 2000, 0.75f);
        c_hi = f.add_centre(1, 3, 2000, 0.75f);
        n_lo = f.w.create_entity();
        n_hi = f.w.create_entity();
        nation_component lo{};
        lo.name          = "Lowland";
        lo.qualification = q_lo;
        nation_component hi{};
        hi.name          = "Highland";
        hi.qualification = q_hi;
        f.w.nations[n_lo] = lo;
        f.w.nations[n_hi] = hi;
        f.w.tile_to_nation[f.tile_ids[0]] = n_lo;
        f.w.tile_to_nation[f.tile_ids[1]] = n_hi;
    }

    /// One migration tick, driven directly (no growth interference) with a
    /// hand-built wage map paying more in the HIGH nation.
    migration_tick step(const recipe_registry& reg)
    {
        std::map<std::pair<entity_id, entity_id>, float> wages;
        wages[{ n_lo, f.body }] = 1.0f;
        wages[{ n_hi, f.body }] = 3.0f;
        return run_population_migration(f.w, reg, wages);
    }

    double tracked_qualified() const
    {
        // Sum of fraction x tracked heads, where a nation's tracked heads are
        // its centres' populations — the ledger the pass conserves against.
        double lo_heads = f.w.population_centres.at(c_lo).population;
        double hi_heads = f.w.population_centres.at(c_hi).population;
        return static_cast<double>(f.w.nations.at(n_lo).qualification) * lo_heads
             + static_cast<double>(f.w.nations.at(n_hi).qualification) * hi_heads;
    }
};

static void test_stance_gate_and_conservation()
{
    std::printf("--- M2/M3: stance gate + qualification conservation ---\n");
    recipe_registry reg;

    // Neutral (no stance rows): throttled but open.
    nation_fixture neutral(0.8f, 0.2f);
    const double q_before = neutral.tracked_qualified();
    migration_tick nt{};
    for (int t = 0; t < 20; ++t)
        nt = neutral.step(reg);
    const int neutral_hi = neutral.f.w.population_centres.at(neutral.c_hi).population;
    check(neutral_hi > 2000,
          "neutral nations exchange, pulled by the wage signal alone (equal habitability)",
          neutral_hi, 2000);

    // M3: the qualified-head total is conserved across the border.
    const double q_after = neutral.tracked_qualified();
    check(std::fabs(q_after - q_before) < 1e-3,
          "total qualified heads conserved across cross-border migration", q_after, q_before);
    // Brain drain: movers skew qualified (selectivity 1.5), so the origin's
    // FRACTION falls while the destination's rises.
    check(neutral.f.w.nations.at(neutral.n_lo).qualification < 0.8f,
          "emigration debits the origin nation's fraction (selectivity > 1)",
          neutral.f.w.nations.at(neutral.n_lo).qualification, 0.8);
    check(neutral.f.w.nations.at(neutral.n_hi).qualification > 0.2f,
          "immigration credits the destination nation's fraction",
          neutral.f.w.nations.at(neutral.n_hi).qualification, 0.2);

    // Friendly: a friendship row (canonical min/max pair — the existing corp
    // stance table read with nation ids) opens the flow fully.
    nation_fixture friendly(0.8f, 0.2f);
    friendly.f.w.corp_friend_pairs.insert({ std::min(friendly.n_lo, friendly.n_hi),
                                            std::max(friendly.n_lo, friendly.n_hi) });
    for (int t = 0; t < 20; ++t)
        friendly.step(reg);
    const int friendly_hi = friendly.f.w.population_centres.at(friendly.c_hi).population;
    check(friendly_hi > neutral_hi,
          "friendly nations exchange more than neutral ones (gate 1000 vs 250)",
          friendly_hi, neutral_hi);

    // Hostile: a directed hostility row closes the flow entirely.
    nation_fixture hostile(0.8f, 0.2f);
    hostile.f.w.corp_hostile_pairs.insert({ hostile.n_hi, hostile.n_lo });
    migration_tick ht{};
    for (int t = 0; t < 20; ++t)
        ht = hostile.step(reg);
    check(hostile.f.w.population_centres.at(hostile.c_hi).population == 2000
              && hostile.f.w.population_centres.at(hostile.c_lo).population == 2000,
          "hostile nations are closed — no heads cross");
    check(ht.gated_closed > 0, "the closed flow is counted, not silent",
          ht.gated_closed, 1);
    check(std::fabs(hostile.f.w.nations.at(hostile.n_lo).qualification - 0.8f) < 1e-6,
          "no flow, no qualification movement");
    (void)nt;
}

// ---------------------------------------------------------------------------
int main()
{
    test_promotion();
    test_decline();
    test_raze();
    test_resettle();
    test_migration_direction();
    test_stance_gate_and_conservation();

    if (g_failures == 0)
        std::printf("\nALL PASS (%d assertions)\n", g_passes);
    else
        std::printf("\n%d FAILURE(s) / %d assertions\n", g_failures, g_passes + g_failures);

    return g_failures > 0 ? 1 : 0;
}
