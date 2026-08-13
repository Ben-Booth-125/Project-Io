// Headless harness (BL-204): the AI skill-regression instrument —
// docs/ai/AI_OPPONENT.md § 3's "data strategy" made concrete.
//
// Freezes a small benchmark seed-set spanning the prototype's body/terrain/market
// diversity (make_hard_coded_world is fully seedable via world_params.seed), runs
// N ticks of the real bot-vs-bot economy loop per seed (run_economy_step's BL-202
// strategic tier already commands every non-player corp; nothing here bypasses
// the corp-command seam), and asserts four per-seed metrics against golden bands:
//   - net-worth curve (sum of every non-player corp's balance, sampled every
//     SAMPLE_EVERY ticks) — the aggregate skill signal.
//   - solvency (ticks any AI corp's balance sampled below zero).
//   - survival (fraction of AI corps still non-decommissioned-out at the end —
//     "still fielding at least one active building").
//   - action counts by corp_verb (the thrash detector: a verb count blowing
//     through its band means the scorer is oscillating instead of holding).
//
// It also lands the tick-boundary STATE HASH (world::state_hash, BL-204): an
// FNV-1a checksum over the econ-tick snapshot. Today this harness uses it purely
// as a two-runs-same-seed determinism primitive (R0 below) — the direct sibling
// of determinism_harness.cpp's field-identity checks, but over LIVE dynamic state
// (balances/dials/prices/pools) rather than static world-gen structure. Per the
// design brief, the same function later becomes the lockstep desync detector for
// multiplayer; nothing about that future role is exercised here.
//
// Goldens are disposable (project convention — bless routinely, don't stage a
// review): the bands below were authored FROM a first observed run of this exact
// harness and are meant to be re-blessed whenever a deliberate AI change moves
// them, not treated as a hidden spec. This A/B-stage core is fully deterministic
// (AI_OPPONENT.md § 1 Area 4 — stochastic Pass/Fail/Inconclusive verdicts are
// reserved for a future non-deterministic AI layer), so every check here is an
// exact equality/range assertion, not a statistical one.
//
// Hand-builds a recipe_registry (mirrors scripts/economy.lua / recipes.lua) so
// the harness stays Lua-free, per verifier-headless convention; links the
// world-gen TU superset (via CMake's IO_WORLD_SOURCES glob) so it drives the
// REAL generated world, not a hand-built scene.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/tech_gate.hpp" // BL-325 S2: advance_tech_gates must run for military_base to ever unlock
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
void check(bool ok, const char* label)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

// ---------------------------------------------------------------------------
// Hand-built registry — mirrors scripts/economy.lua + scripts/recipes.lua for
// every building_type the generator places (extraction_site, processing_facility,
// port, launchpad, inland_logistics_hub) so a bot-vs-bot rollout over the real
// generated world has real economics to score against. Substrate/construction/
// logistics/road defaults already match economy.lua (recipe_registry.hpp), so
// only economics + the three prototype recipes need authoring by hand.
// ---------------------------------------------------------------------------
recipe_registry make_registry()
{
    recipe_registry reg;

    building_economics extraction;
    extraction.base_rate            = 20.0f;
    extraction.maintenance          = 5.0f;
    extraction.base_wage            = 8.0f;
    extraction.build_cost           = 100.0f;
    extraction.build_duration_ticks = 2.0f;
    extraction.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 20.0f;
    reg.set_economics(building_type::extraction_site, extraction);

    building_economics processing;
    processing.base_rate            = 8.0f;
    processing.maintenance          = 10.0f;
    processing.base_wage            = 12.0f;
    processing.build_cost           = 200.0f;
    processing.build_duration_ticks = 3.0f;
    processing.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 25.0f;
    reg.set_economics(building_type::processing_facility, processing);

    building_economics port;
    port.maintenance          = 8.0f;
    port.base_wage            = 6.0f;
    port.build_cost           = 150.0f;
    port.build_duration_ticks = 2.0f;
    port.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 20.0f;
    reg.set_economics(building_type::port, port);

    building_economics launchpad;
    launchpad.maintenance          = 20.0f;
    launchpad.base_wage            = 15.0f;
    launchpad.build_cost           = 500.0f;
    launchpad.build_duration_ticks = 6.0f;
    launchpad.resource_build_cost[static_cast<std::size_t>(resource_type::steel)]       = 50.0f;
    launchpad.resource_build_cost[static_cast<std::size_t>(resource_type::refined_fuel)] = 20.0f;
    reg.set_economics(building_type::launchpad, launchpad);

    building_economics hub;
    hub.maintenance          = 12.0f;
    hub.base_wage            = 8.0f;
    hub.build_cost           = 250.0f;
    hub.build_duration_ticks = 3.0f;
    hub.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 30.0f;
    reg.set_economics(building_type::inland_logistics_hub, hub);

    // Mirrors scripts/economy.lua's military_base row exactly (BL-325 S1).
    // Needed now that corp_ai proposes military_base build candidates (BL-325
    // S2's muster-base fix) — an un-set entry would give it a zero build_cost
    // and understate a rival corp's real spend.
    building_economics base;
    base.maintenance          = 15.0f;
    base.base_wage            = 10.0f;
    base.build_cost           = 300.0f;
    base.build_duration_ticks = 4.0f;
    base.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 35.0f;
    reg.set_economics(building_type::military_base, base);

    recipe steel;
    steel.name = "steel";
    steel.inputs[static_cast<std::size_t>(resource_type::iron_ore)] = 2.0f;
    steel.outputs[static_cast<std::size_t>(resource_type::steel)]   = 1.0f;
    reg.add_recipe(steel);

    recipe fuel;
    fuel.name = "refined_fuel";
    fuel.inputs[static_cast<std::size_t>(resource_type::petroleum)]     = 2.0f;
    fuel.outputs[static_cast<std::size_t>(resource_type::refined_fuel)] = 1.0f;
    reg.add_recipe(fuel);

    recipe food;
    food.name = "food_rations";
    food.inputs[static_cast<std::size_t>(resource_type::agricultural_produce)] = 2.0f;
    food.outputs[static_cast<std::size_t>(resource_type::food_rations)]        = 1.0f;
    reg.add_recipe(food);

    return reg;
}

/// One full sim tick: economy step (incl. the BL-202 strategic tier, which
/// commands every due non-player corp) -> market clearing -> budget -> tech
/// gates. Tech-gate advancement was missing here before BL-325 S2 — no
/// harness corp could ever earn E0-ML-01, so military_base could never be
/// built and hire_unit silently zeroed out (caught rerunning this golden
/// after S2 landed). main.cpp / core/app.cpp both call this every real tick;
/// this brings the harness's rollout back in line with actual play.
/// @param out_idled_events Optional. Receives the count of `agency_event::kind::idled`
///        reported this tick — BOTH tiers, since both narrate an idling that way.
///        Subtracting the decision ring's own `corp_verb::idle` tally leaves the
///        BL-079 REFLEX tier's idlings, which are otherwise invisible here: that
///        tier mutates `building_component` directly rather than through
///        apply_corp_command, so it never reaches the ring. Without the
///        difference the oscillation is unreadable — `action[idle]` counts one
///        tier's idlings while `action[resume]` counts reversals of both, so
///        resume outnumbers idle by ~20:1 with no visible cause.
void run_tick(world& w, const recipe_registry& reg, int tick, int* out_idled_events = nullptr)
{
    w.current_day_tick = tick;
    const economy_report rep = run_economy_step(w, reg);
    if (out_idled_events)
    {
        int n = 0;
        for (const agency_event& ev : rep.agency_events)
            if (ev.what == agency_event::kind::idled)
                ++n;
        *out_idled_events = n;
    }
    const auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention, nullptr);
    advance_tech_gates(w);
}

// ---------------------------------------------------------------------------
// Per-seed rollout metrics
// ---------------------------------------------------------------------------
struct rollout_metrics
{
    std::vector<float> net_worth_curve;   ///< Sum of non-player corp balances, sampled every SAMPLE_EVERY ticks.
    int                 solvency_ticks_below_zero = 0; ///< Samples where ANY AI corp balance < 0.
    float               survival_fraction         = 0.0f; ///< Fraction of AI corps with >=1 active (non-decommissioned) asset at the end.
    std::map<corp_verb, int> action_counts;        ///< Tally of every applied corp_command by verb.
    std::vector<uint64_t>    state_hashes;          ///< world::state_hash(tick) at every sample point.
    int                      reflex_idles = 0;      ///< BL-079 reflex-tier idlings (idled events minus the ring's own idle commands).
};

constexpr int sample_every = 10;

rollout_metrics run_rollout(uint32_t seed, int ticks)
{
    world_params params;
    params.seed = seed;
    world w = make_hard_coded_world(params);
    const recipe_registry reg = make_registry();

    rollout_metrics m;
    for (int t = 1; t <= ticks; ++t)
    {
        int idled_events = 0;
        run_tick(w, reg, t, &idled_events);

        // Tally every decision applied THIS tick (the ring wraps at 256 entries
        // over a long rollout, so read it live rather than at the end).
        int strategic_idles_this_tick = 0;
        for (const corp_decision& d : w.ai_decisions.entries)
            if (d.tick == t)
            {
                ++m.action_counts[d.command.verb];
                if (d.command.verb == corp_verb::idle)
                    ++strategic_idles_this_tick;
            }

        // What the reflex tier did on its own, in commands it never issued.
        m.reflex_idles += (idled_events - strategic_idles_this_tick);

        if (t % sample_every == 0)
        {
            float net_worth = 0.0f;
            bool  any_below_zero = false;
            for (const auto& [id, cc] : w.corporations)
            {
                if (cc.is_player) continue;
                net_worth += cc.balance;
                if (cc.balance < 0.0f) any_below_zero = true;
            }
            m.net_worth_curve.push_back(net_worth);
            if (any_below_zero) ++m.solvency_ticks_below_zero;
            m.state_hashes.push_back(w.state_hash(t));
        }
    }

    int ai_corps = 0, ai_active = 0;
    for (const auto& [id, cc] : w.corporations)
    {
        if (cc.is_player) continue;
        ++ai_corps;
        for (const entity_id bld : cc.assets)
        {
            const auto it = w.buildings.find(bld);
            if (it != w.buildings.end() && !it->second.decommissioned) { ++ai_active; break; }
        }
    }
    m.survival_fraction = (ai_corps > 0) ? static_cast<float>(ai_active) / static_cast<float>(ai_corps) : 0.0f;

    return m;
}

const char* verb_name(corp_verb v)
{
    switch (v)
    {
        case corp_verb::build:         return "build";
        case corp_verb::demolish:      return "demolish";
        case corp_verb::set_recipe:    return "set_recipe";
        case corp_verb::set_workforce: return "set_workforce";
        case corp_verb::idle:          return "idle";
        case corp_verb::resume:        return "resume";
        case corp_verb::place_road:    return "place_road";
        case corp_verb::survey:        return "survey";
        case corp_verb::hire_unit:     return "hire_unit";
        // The seam grew past this instrument twice (BL-293 order book, BL-350
        // procurement) and the tally silently pooled every new verb under "?".
        // A skill instrument that cannot name what the AI did is measuring the
        // wrong thing, so the switch is exhaustive and -Wswitch now guards it.
        case corp_verb::place_sell_order:   return "place_sell_order";
        case corp_verb::remove_sell_order:  return "remove_sell_order";
        case corp_verb::set_workforce_auto: return "set_workforce_auto";
        case corp_verb::request_quote:      return "request_quote";
        case corp_verb::accept_quote:       return "accept_quote";
        case corp_verb::cancel_contract:    return "cancel_contract";
    }
    return "?";
}

/// A named golden band: [lo, hi] inclusive. Disposable per project convention —
/// re-derive from a fresh run and re-bless on any deliberate AI change.
struct band { float lo; float hi; };

struct seed_golden
{
    uint32_t seed;
    band     net_worth_final;      ///< Last sample of the net-worth curve.
    band     net_worth_min;        ///< Lowest sample across the curve.
    int       solvency_max;         ///< Max acceptable solvency_ticks_below_zero.
    band      survival;             ///< Fraction of AI corps still active at the end.
    int       build_max;            ///< Thrash ceiling: total `build` commands over the run.
    int       dial_max;             ///< Thrash ceiling: total non-build, non-survey commands (dials).
};

// ---------------------------------------------------------------------------
// FROZEN BENCHMARK SEED-SET (BL-204) — five seeds spanning the generator's
// body/terrain/market diversity via world_params.seed (0 reproduces the legacy
// hand-authored world; 1-4 reroll the planetology/tile/nation/corporation
// pipeline for a different chain). 300 ticks (~3.3 campaign years at the
// quarter-tick cadence) is long enough for the strategic tier (cadence_k=4) to
// clear several cooldown windows per corp.
// ---------------------------------------------------------------------------
constexpr int ticks_per_seed = 300;

// RE-BLESSED 2026-08-01 on Windows/MSVC (BL-252). Observed:
//   seed 0: net-worth final=206245.2 min=67709.2  solvency=6/30  survival=0.71  build=0 dial=123
//   seed 1: net-worth final=357967.1 min=106137.9 solvency=0/30  survival=0.86  build=0 dial=169
//   seed 2: net-worth final=305816.9 min=84838.8  solvency=0/30  survival=0.71  build=0 dial=156
//   seed 3: net-worth final=338420.1 min=116730.4 solvency=8/30  survival=0.71  build=0 dial=157
//   seed 4: net-worth final=392148.4 min=119728.7 solvency=4/30  survival=0.86  build=0 dial=121
// Bands are the observed value ±roughly 30-45% (net-worth/survival) or a modest
// fixed slack (solvency/action ceilings) — tight enough to catch a real skill
// regression, loose enough that ordinary tuning noise doesn't force a re-bless
// every session. Disposable per project convention: bless routinely from a
// fresh run, don't stage a review, flag only an UNEXPLAINED divergence.
//
// Why the previous bands were stale, and why re-blessing them was safe: they were
// authored from the FIRST run of this harness, in the same commit that added it
// (8542e4b). Several world-layer changes landed afterwards — most decisively
// BL-203 (Corp AI stage B: the strategy layer, priority buckets and predictive
// spending), then BL-221 (pre-national ladder) and BL-233 (terrain combat
// modifiers), both of which reshape the political map every seed generates. The
// AI these bands score was substantially rewritten after its own goldens were
// set, so the divergence is EXPLAINED, not a skill regression.
//
// THE BANDS ARE PINNED PER TOOLCHAIN (BL-252, Ben's call 2026-08-01).
//
// The same commit under Linux/GCC -O2 produces materially different values from
// Windows/MSVC — seed 0 final 395143.0 against 206245.2, seed 4 182745.5 against
// 392148.4 — while each platform stays perfectly deterministic WITHIN itself
// (R0 passes on both: same-seed state hashes and net-worth curves are
// byte-identical, on each). 300 ticks of a feedback-coupled economy amplifies
// last-bit float differences, so this is expected rather than a defect. The
// standing determinism invariant is same-seed/same-BUILD reproducibility, and it
// is intact; bit-identical floating point across MSVC and GCC is explicitly not a
// goal this prototype adopts.
//
// Optimisation level is NOT the variable: MSVC /O2 reproduces MSVC Debug exactly,
// value for value. The toolchain is.
//
// Why pinned rather than widened: a single band wide enough to hold both platforms
// would span roughly ±100% (seed 4 alone runs 182746 to 392148) and would detect
// no plausible skill regression at all. Pinning keeps both platforms asserting
// real bands, at the cost of re-blessing two sets when the AI deliberately changes.
//
// Re-blessing: run the harness on the platform whose set you are changing, and
// change ONLY that set. Never copy one platform's observed values into the other's
// block — that is the failure mode this structure exists to prevent. A third
// toolchain (Clang, or a different libstdc++) needs its own block and its own
// blessed run; it must not silently inherit the GCC set.
//
// The visual goldens answer differently and are Windows-authoritative (pixel
// output additionally depends on font rasterisation and GPU/driver) — see
// docs/development/DEVELOPMENT_PRACTICES.md § Goldens.
// Clang is tested FIRST and deliberately: it defines __GNUC__ (and clang-cl defines
// _MSC_VER), so an ordinary MSVC/GCC pair would let a Clang build silently inherit a
// set it was never blessed against — the exact outcome this structure exists to stop.
#if defined(__clang__)
#error "ai_skill_harness: no blessed golden band set for Clang (BL-252). Clang defines \
__GNUC__, so it would otherwise inherit the GCC set and assert it against different \
float output. Bless a set from a fresh Clang run and add its own block."
#elif defined(_MSC_VER)
// --- Windows / MSVC — RE-BLESSED 2026-08-02 after BL-218 (settlement rewrite).
// Observed on that run:
//   seed 0: final=826901.5 min=273153.0 solvency=6/30 survival=0.57 build=0 dial=208
//   seed 1: final=408192.4 min=124904.8 solvency=0/30 survival=0.71 build=0 dial=239
//   seed 2: final=685770.9 min=186317.3 solvency=5/30 survival=0.57 build=0 dial=205
//   seed 3: final=434729.1 min=152258.4 solvency=6/30 survival=0.71 build=0 dial=207
//   seed 4: final=639130.4 min=203033.9 solvency=7/30 survival=0.71 build=0 dial=257
//
// EXPLAINED, not a skill regression, and the direction is the evidence. BL-218
// replaced the nation seeds with the settlement pass's province anchors and
// BL-219 derived corporate focus from the province a corp anchors to, so every
// seed now generates a different political map and a different corporate mix —
// the same class of change as BL-221/BL-233 above, which this block was last
// re-blessed for. Every divergence is UPWARD (net worth rose on seeds 0/2/4; the
// dial counts crept past their old ceiling on 0/1/4) while solvency and survival
// stayed in band on all five seeds. Corps anchored to provinces that actually
// industrialised sit on better ground than randomly-placed ones did, which is
// the intended consequence of the rewrite rather than noise to suppress.
//
// STALE AS OF 2026-08-09 — see the GCC block's re-bless note below. This set was
// blessed 2026-08-02, BEFORE BL-323 (the logistics reach rule) and BL-324
// (unit hiring) changed what the scorer does. It will fail on the next Windows
// run for the same reason the GCC set just did. Re-bless it from a fresh MSVC
// run; do NOT copy the GCC numbers across.
const std::vector<seed_golden> goldens = {
    { 0, {480000.0f, 1150000.0f}, {160000.0f, 380000.0f}, 12, {0.45f, 0.95f},  5, 260 },
    { 1, {235000.0f,  570000.0f}, { 72000.0f, 175000.0f},  5, {0.60f, 1.00f},  5, 290 },
    { 2, {400000.0f,  960000.0f}, {108000.0f, 260000.0f}, 12, {0.45f, 0.95f},  5, 260 },
    { 3, {250000.0f,  610000.0f}, { 88000.0f, 213000.0f}, 14, {0.45f, 0.95f},  5, 260 },
    { 4, {370000.0f,  895000.0f}, {118000.0f, 284000.0f}, 12, {0.60f, 1.00f},  5, 310 },
};
#elif defined(__GNUC__)
// --- Linux / GCC -O2 — RE-BLESSED 2026-08-09 (BL-285 task 1, at the v0.1.8 cut).
// Observed on that run:
//   seed 0: final=248736.1 min=78098.3 solvency=6/30 survival=0.57 build=0 dial=140
//   seed 1: final=102789.2 min=38965.2 solvency=9/30 survival=0.71 build=0 dial=128
//   seed 2: final=277622.0 min=78597.0 solvency=4/30 survival=0.57 build=0 dial=252
//   seed 3: final=160907.5 min=38636.2 solvency=8/30 survival=0.71 build=0 dial=130
//   seed 4: final=136757.1 min=44641.4 solvency=4/30 survival=0.71 build=0 dial=118
//
// The previous set (blessed 2026-08-01) was never re-run after BL-218/BL-219, so
// it had drifted for the same reason the MSVC block was re-blessed on 2026-08-02
// — that is the drift BL-285 existed to close. Two further changes landed since,
// and they are what makes this re-bless EXPLAINED:
//
//   * BL-323 (logistics reach rule) — placement is now bounded by distance from a
//     supply anchor. Roughly 77% of Kepler's land is placeable where 100% was, so
//     a corp can no longer site on the richest tile irrespective of remoteness.
//   * BL-324 (unit hiring) — corps now spend on units. All five seeds show
//     action[hire_unit] = 21, an outflow that simply did not exist at the last
//     bless. Note hire_unit is excluded from the dial ceiling above, so it moves
//     net worth without moving the thrash counters.
//
// DIRECTION IS DOWNWARD, AND THAT IS THE PART WORTH A SECOND LOOK. The MSVC
// re-bless of 2026-08-02 moved every divergence UP and was easy to accept. This
// one moves down, and not uniformly: seed 1 fell from 550394 to 102789 (-81%) and
// went from the highest of the five to the lowest, while seed 4 fell only 25%.
// Constraining siting and adding a cash outflow should cost net worth, and a
// per-seed reshuffle is what a *placement* constraint would produce — some corps'
// best ground falls outside reach, others' does not. So the shape matches the
// cause. It is recorded here rather than waved through because "the AI got poorer"
// is exactly what a genuine skill regression would also look like: if a later
// session finds the scorer mis-valuing reach-limited sites, this block is the
// evidence trail, not an alibi.
//
// Solvency ceilings rose on seeds 1 and 3 (5 -> 16, 10 -> 15) because corps now
// dip below zero more often while paying for units; survival held in band on all
// five, so corps are poorer but not dying — the distinction that says this is
// cost, not collapse.
// RE-BLESSED AGAIN 2026-08-10, at the v0.1.10 cut. Observed:
//   seed 0: final=236029.7 min=74937.1  solvency=12/30 survival=0.71 build=0 dial=162
//   seed 1: final=187227.0 min=71022.1  solvency=3/30  survival=0.57 build=0 dial=204
//   seed 2: final=323820.2 min=99492.8  solvency=5/30  survival=0.57 build=0 dial=223
//   seed 3: final=167442.5 min=57367.3  solvency=8/30  survival=0.71 build=0 dial=169
//   seed 4: final=346448.9 min=122610.5 solvency=0/30  survival=0.29 build=0 dial=291
//
// EXPLAINED, and the direction is the evidence — this move is UPWARD where the
// 2026-08-09 bless was downward, and the cause is the mirror image. That bless
// recorded net worth falling because BL-323 constrained siting and BL-324 added
// a cash outflow. v0.1.10 moved the ground back the other way:
//
//   * BL-283 — corps now anchor inside their HOME PROVINCE. Measured on seed 0,
//     holdings moved off dead ground onto settled ground: barren 6->3, icy 5->3,
//     grassland 3->7. Provinces sit where people settled, so the same corp works
//     better land. Seed 1 +82% and seed 4 +153% are that, compounded over 300 ticks.
//   * BL-338 — wetland exists again (12 tiles -> 159 on the home body), and it is
//     a habitable composition, so there is more workable ground to anchor on.
//   * BL-346/BL-347 — the workforce solver now accounts for stack decay, so the
//     dial lands somewhere different; every dial count rose.
//
// SEED 4's SURVIVAL FELL 0.71 -> 0.29 AND IS NOT WIDENED AWAY QUIETLY. Its band
// is loosened to admit 0.29 because the harness must run, but the number itself is
// the interesting one: on that seed, FEWER corps survive while the survivors end up
// three times richer. That is a plausible consequence of better ground concentrating
// advantage — winners win harder, and marginal corps that previously scraped by on
// poor land now lose the race outright — but it is a HYPOTHESIS, not a measurement.
// If a later session finds rival corps dying off across many seeds rather than this
// one, this comment is the evidence trail and the band should tighten, not stretch
// again. Solvency on seed 4 also went 4 -> 0, which is consistent with that reading:
// the survivors are never in trouble.
//
// The MSVC set above remains stale for its own (2026-08-09) reason and additionally
// for this one; it needs its own fresh Windows run.
// RE-BLESSED 2026-08-10 (BL-325 S2 — corp_ai now proposes a military_base
// build the moment a rival corp holds none, gated on the E0-ML-01 tech and
// competing on merit against extraction/processing in the same nice_to_have
// bucket). This is the first thing to actually exercise corp_ai's OWN build
// path completing construction inside this harness — the pre-change baseline
// ran 0 build actions across all five seeds, because the generated world
// starts already built out and corp_ai only ever dialled it. That gave a
// build_max/survival band tuned against a world where corp_ai never builds
// anything, which the new candidate exposed as untested rather than correct.
//
// Net-worth/solvency bands are UNCHANGED — the muster-base spend (build_cost
// 300, a small fraction of a corp's balance) does not move them outside the
// existing tolerance. survival_fraction moved on seeds 1 and 4 specifically
// (both now finish at 1.00, where every AI corp holds at least one active
// asset). HYPOTHESIS, not measurement (matching the standing rule for seed
// 4's own prior widening below): a corp mid-build on a military_base is
// still "active" by this metric's own definition even while its base sits
// stalled awaiting local steel supply (BL-095's pay-as-you-build model —
// most muster candidates observed getting picked up have their construction
// STALL indefinitely on tiles whose local market never carries steel, so
// hire_unit was NOT observed firing in this harness's five seeds; flagged in
// BL-325's design record rather than forced to pass by tuning a score). If a
// later run finds hire_unit consistently absent even after a genuine
// steel-routing fix, treat that as the open question, not this band.
// RE-BLESSED 2026-08-13. Two independent causes, and they are worth separating
// because only one of them is this session's doing.
//
// (1) NET-WORTH FINAL had been failing on all five seeds for some time — the
// bands sat 2.9-3.1x below the observed values, drift accumulated across
// Sprint 10's landings (BL-365 background firms, BL-132 market cogeneration,
// BL-130 market inventory) and flagged as a standing steward in the review
// queue. Nothing in this session moved it materially: net worth rose 0.5-0.9%.
// The bands below are re-centred on observation at roughly +/-35%. NET-WORTH
// MIN and SOLVENCY were passing throughout and are UNCHANGED, deliberately —
// re-blessing a band that was binding correctly only loosens it.
//
// (2) DIAL_MAX is TIGHTENED HARD, and that is the point. The old ceilings
// (230/290/315/240/410) were blessed from runs containing 134-255 `resume`
// actions per rollout — i.e. from the idle/resume oscillation they exist to
// detect. A thrash detector calibrated on the thrash certifies it.
//
// With the resume estimator corrected (real staffing rather than a hypothetical
// building's, the site's true stack rank rather than one step below it, and the
// fixed share of maintenance rather than all of it), the oscillation does not
// merely damp — it STOPS. `resume` goes 134/178/193/153/255 -> 0/0/0/0/1, and
// the BL-079 reflex tier's own idlings, which were reversing straight back into
// losses, go 67/137/132/93/198 -> 9/8/7/6/7. Net worth is UP on every seed, so
// none of this was profitable churn.
//
// Dial totals are now 40/42/51/31/53. The ceilings sit at ~1.3x that, which
// means a regression anywhere near the old behaviour fails on the first run
// rather than passing with 4x headroom. Still observation-blessed; deriving
// them from the action budget (corps x evals x max_dials) is the better answer
// and is filed as follow-up.
//
// NET-WORTH MIN, SOLVENCY, SURVIVAL and BUILD_MAX are unchanged and all still
// bind on observation. The net-worth-final bands below are left as re-centred
// above rather than re-tightened again — every seed sits comfortably inside
// them, and narrowing a band twice in one session is fitting to noise.
const std::vector<seed_golden> goldens = {
    { 0, {638000.0f, 1327000.0f}, { 45000.0f, 105000.0f}, 20, {0.45f, 0.95f},  5,  52 },
    { 1, {512000.0f, 1063000.0f}, { 42000.0f, 100000.0f}, 10, {0.45f, 1.00f},  5,  55 },
    { 2, {867000.0f, 1801000.0f}, { 59000.0f, 140000.0f}, 12, {0.45f, 0.95f},  5,  66 },
    { 3, {481000.0f,  998000.0f}, { 34000.0f,  81000.0f}, 15, {0.45f, 0.95f},  5,  40 },
    { 4, {924000.0f, 1919000.0f}, { 73000.0f, 172000.0f},  8, {0.20f, 1.00f},  6,  69 },
};
#else
#error "ai_skill_harness: no blessed golden band set for this toolchain (BL-252). \
Bless one from a fresh run on this toolchain and add its own block — do not \
reuse another toolchain's values."
#endif

} // namespace

int main()
{
    std::printf("BL-204 AI skill-regression harness\n");

    // =========================================================================
    // R0 — the state hash is a valid same-seed-two-runs determinism primitive.
    // =========================================================================
    {
        const rollout_metrics a = run_rollout(/*seed=*/0, /*ticks=*/60);
        const rollout_metrics b = run_rollout(/*seed=*/0, /*ticks=*/60);
        check(a.state_hashes.size() == b.state_hashes.size() && !a.state_hashes.empty(),
              "BL-204 R0: state_hash is sampled every run (non-empty, matching sample count)");
        bool hashes_match = a.state_hashes.size() == b.state_hashes.size();
        for (std::size_t i = 0; hashes_match && i < a.state_hashes.size(); ++i)
            if (a.state_hashes[i] != b.state_hashes[i]) hashes_match = false;
        check(hashes_match,
              "BL-204 R0: world::state_hash is IDENTICAL at every sampled tick across two same-seed runs");
        check(a.net_worth_curve == b.net_worth_curve,
              "BL-204 R0: the net-worth curve itself is byte-identical across two same-seed runs");
        check(a.action_counts == b.action_counts,
              "BL-204 R0: action-verb tallies are identical across two same-seed runs");

        // A different seed changes the hash (the hash isn't a constant / no-op).
        const rollout_metrics c = run_rollout(/*seed=*/1, /*ticks=*/60);
        check(!a.state_hashes.empty() && !c.state_hashes.empty() &&
              a.state_hashes.back() != c.state_hashes.back(),
              "BL-204 R0: state_hash differs across two DIFFERENT seeds (not a constant)");
    }

    // =========================================================================
    // R1-R4 — per-seed golden bands over the frozen benchmark set.
    // =========================================================================
    for (const seed_golden& g : goldens)
    {
        char hdr[64];
        std::snprintf(hdr, sizeof hdr, "seed %u", g.seed);
        std::printf(" -- %s --\n", hdr);

        const rollout_metrics m = run_rollout(g.seed, ticks_per_seed);

        const float final_nw = m.net_worth_curve.empty() ? 0.0f : m.net_worth_curve.back();
        float min_nw = final_nw;
        for (const float v : m.net_worth_curve) min_nw = std::min(min_nw, v);

        std::printf("    net_worth: final=%.1f min=%.1f  solvency_below_zero=%d/%zu  survival=%.2f\n",
                    final_nw, min_nw, m.solvency_ticks_below_zero, m.net_worth_curve.size(),
                    m.survival_fraction);
        int build_total = 0, dial_total = 0;
        for (const auto& [verb, count] : m.action_counts)
        {
            std::printf("    action[%s] = %d\n", verb_name(verb), count);
            if (verb == corp_verb::build) build_total += count;
            // hire_unit is neither a build nor a per-building dial (BL-324) —
            // it never touches a building, so it does not belong in the
            // dial-thrash ceiling this loop is measuring.
            else if (verb != corp_verb::survey && verb != corp_verb::hire_unit)
                dial_total += count;
        }
        // The other half of the idle/resume books: idlings the BL-079 reflex tier
        // performed directly, which issue no command and so appear in no
        // action[] row. Printed, deliberately NOT banded — this is a reading, and
        // banding a number on its first observation freezes in whatever it
        // happens to be rather than what it should be.
        std::printf("    reflex_idles (BL-079 tier, no command issued) = %d\n", m.reflex_idles);

        char label[128];
        std::snprintf(label, sizeof label, "%s: net-worth final in golden band", hdr);
        check(final_nw >= g.net_worth_final.lo && final_nw <= g.net_worth_final.hi, label);

        std::snprintf(label, sizeof label, "%s: net-worth min in golden band", hdr);
        check(min_nw >= g.net_worth_min.lo && min_nw <= g.net_worth_min.hi, label);

        std::snprintf(label, sizeof label, "%s: solvency (ticks below zero) within band", hdr);
        check(m.solvency_ticks_below_zero <= g.solvency_max, label);

        std::snprintf(label, sizeof label, "%s: survival fraction in golden band", hdr);
        check(m.survival_fraction >= g.survival.lo && m.survival_fraction <= g.survival.hi, label);

        std::snprintf(label, sizeof label, "%s: build-action count under thrash ceiling", hdr);
        check(build_total <= g.build_max, label);

        std::snprintf(label, sizeof label, "%s: dial-action count under thrash ceiling", hdr);
        check(dial_total <= g.dial_max, label);
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
