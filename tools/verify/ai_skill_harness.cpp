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
/// commands every due non-player corp) -> market clearing -> budget.
void run_tick(world& w, const recipe_registry& reg, int tick)
{
    w.current_day_tick = tick;
    const economy_report rep = run_economy_step(w, reg);
    const auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention, nullptr);
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
        run_tick(w, reg, t);

        // Tally every decision applied THIS tick (the ring wraps at 256 entries
        // over a long rollout, so read it live rather than at the end).
        for (const corp_decision& d : w.ai_decisions.entries)
            if (d.tick == t)
                ++m.action_counts[d.command.verb];

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
// The GCC set below is deliberately NOT touched — re-bless it from a fresh Linux
// run, per the rule above.
const std::vector<seed_golden> goldens = {
    { 0, {480000.0f, 1150000.0f}, {160000.0f, 380000.0f}, 12, {0.45f, 0.95f},  5, 260 },
    { 1, {235000.0f,  570000.0f}, { 72000.0f, 175000.0f},  5, {0.60f, 1.00f},  5, 290 },
    { 2, {400000.0f,  960000.0f}, {108000.0f, 260000.0f}, 12, {0.45f, 0.95f},  5, 260 },
    { 3, {250000.0f,  610000.0f}, { 88000.0f, 213000.0f}, 14, {0.45f, 0.95f},  5, 260 },
    { 4, {370000.0f,  895000.0f}, {118000.0f, 284000.0f}, 12, {0.60f, 1.00f},  5, 310 },
};
#elif defined(__GNUC__)
// --- Linux / GCC -O2 — blessed 2026-08-01. Observed:
//   seed 0: net-worth final=395143.0 min=123180.0 solvency=5/30  survival=0.71  build=0 dial=174
//   seed 1: net-worth final=550394.2 min=177619.3 solvency=0/30  survival=0.71  build=0 dial=170
//   seed 2: net-worth final=505318.6 min=155243.2 solvency=0/30  survival=0.71  build=0 dial=244
//   seed 3: net-worth final=305209.8 min=93584.9  solvency=3/30  survival=0.71  build=0 dial=109
//   seed 4: net-worth final=182745.5 min=51070.9  solvency=4/30  survival=0.71  build=0 dial=122
const std::vector<seed_golden> goldens = {
    { 0, {235000.0f, 555000.0f}, { 70000.0f, 175000.0f}, 12, {0.45f, 0.95f},  5, 240 },
    { 1, {330000.0f, 770000.0f}, {105000.0f, 250000.0f},  5, {0.45f, 0.95f},  5, 240 },
    { 2, {300000.0f, 710000.0f}, { 90000.0f, 220000.0f},  5, {0.45f, 0.95f},  5, 330 },
    { 3, {180000.0f, 430000.0f}, { 55000.0f, 130000.0f}, 10, {0.45f, 0.95f},  5, 160 },
    { 4, {110000.0f, 255000.0f}, { 30000.0f,  72000.0f}, 10, {0.45f, 0.95f},  5, 180 },
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
            else if (verb != corp_verb::survey) dial_total += count;
        }

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
