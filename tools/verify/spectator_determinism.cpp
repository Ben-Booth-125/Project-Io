// Headless harness (BL-409): spectator mode has no human seat.
//
// The standing rule (.claude/rules/io-standing-rules.md § Determinism & data
// model) forbids auto-acting strategically on the PLAYER'S corp. It protects
// that corp *because a human owns it*. Spectator mode has no human owner, so
// the rule's precondition is absent rather than excepted (Ben, 2026-08-14 —
// "'who plays your corp' collapses as a question"), and `world::player_entity`
// degrades to a camera/ledger anchor. This harness holds both halves of that
// claim at once: the prohibition still binds a played session, and it does not
// bind a spectated one.
//
//   R1 — with spectating ON, the player corp builds, dials, surveys and trades
//        on the SAME staggered cadence as a rival. Checked two ways: the
//        schedule directly (corp_strategic_eval_due at the player's own sorted
//        index), and the behaviour (the verbs it actually issues over a
//        rollout). Also asserts that admitting the player perturbs NO rival's
//        schedule — the cadence keys on an index into the sorted FULL corp set,
//        which already contained the player.
//
//   R2 — with spectating OFF the run is unchanged. THE LOAD-BEARING ONE: a
//        regression here silently moves every existing harness and every saved
//        campaign. Asserted as (a) the player corp issues exactly zero
//        decisions, (b) an unspectated rollout is reproducible, and (c) its
//        final `world::state_hash` equals the golden below, which was recorded
//        from this same rollout run against the PRE-BL-409 corp_ai.cpp.
//
//   R3 — a spectated run is itself deterministic: same seed, spectating on,
//        N ticks, run twice, identical state_hash both times.
//
// VACUITY. Two guards, because a green result about an economy that is not
// there is this tier's least visible failure mode (verifier-headless SKILL.md,
// and BL-404's worked example):
//   1. NOTHING IS LOADED BY PATH. The registry is hand-built below, mirroring
//      scripts/economy.lua the way ai_skill_harness does, so there is no
//      relative-path script load that could silently no-op from the wrong
//      working directory and leave a default registry with no demand baskets.
//   2. AN EXPLICIT ANTI-VACUITY ASSERTION runs before any player conclusion:
//      the RIVAL corps must have acted across several verb families. If the
//      economy were hollow, rivals would do nothing, R1's player checks would
//      hold trivially, and this guard is what fails instead.
//
// Every determinism comparison is between TWO INDEPENDENTLY CONSTRUCTED
// worlds (a fresh make_hard_coded_world per rollout), never the same world
// hashed twice — the latter proves only that the hash function is a function.
//
// Hand-builds a recipe_registry (Lua-free, per verifier-headless convention);
// links the world TU superset via CMake's IO_VERIFY_HARNESSES glob, so no
// build-file entry is needed.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/tech_gate.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <vector>

namespace {

int g_failures = 0;
void check(bool ok, const char* label)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

// ---------------------------------------------------------------------------
// Rollout parameters
// ---------------------------------------------------------------------------
// One seed, not a set: this harness asks a structural question (does the guard
// lift, does the unspectated path move) that a second seed cannot answer more
// than the first. The skill-regression instrument that DOES need a seed spread
// is ai_skill_harness.
// 300 ticks matches ai_skill_harness's rollout length, and it is chosen for a
// reason R1 depends on: over a SHORT run no single corp reaches all four verb
// families. Measured at 120 ticks, seven rival corps managed 5 builds, 4
// surveys and 7 trades BETWEEN them — under one each — so demanding four
// families of the player there would have held it to a bar no rival clears,
// which is the opposite of what R1 asserts. The bar below is therefore stated
// against what rivals actually achieve on this same rollout, not against a
// number picked in advance.
constexpr uint32_t k_seed  = 0;
constexpr int      k_ticks = 300;

// R2's BYTE-IDENTITY GOLDEN — RETIRED (Ben, 2026-09-01, ruling on NR-752).
//
// The row asserted that an unspectated rollout still hashes to a constant
// recorded from a pre-BL-409 build. It is gone, and the log below is why: it
// was re-blessed TEN TIMES between 2026-08-14 and 2026-08-31, and NOT ONE of
// those moves was a spectator-mode fact. Every one was a deliberate world or
// economy change somewhere else in the project — a grid resize, a resource_count
// widening, holdings patterns, road cuts, the province partition, a unit-upkeep
// retune. The row was asserting WORLD-CONTENT STABILITY, which is not what this
// harness is for and never was.
//
// This is the NR-596 argument taken one step further, and NR-596 is the
// precedent — that entry retired a third check on this same harness for the
// same reason (see the RETIRED note further down).
//
// WHAT STILL GUARDS THE PROPERTY, and it is the part that matters: the two
// invariants io-standing-rules.md's BL-409 section actually cites are asserted
// below and unaffected — the flag DEFAULTS FALSE so a played session keeps the
// prohibition in full, and admitting one more corp SHIFTS NO RIVAL'S CADENCE
// SLOT. Determinism keeps its own guards too: R2's two-independently-built-
// worlds reproducibility row, and every row of R3. What is gone is only the
// frozen constant, which could not tell a determinism leak from a Tuesday.
//
// THE LOG IS KEPT DELIBERATELY. It is the evidence for the retirement, and a
// future reader tempted to re-add a hash constant here should read it first.
//
// ---- historical provenance, 2026-08-14 to 2026-08-31 ----
// Recorded from `run_rollout(k_seed, k_ticks, /*spectating=*/false)`
// built against the PRE-BL-409 corp_ai.cpp — i.e. the value the unspectated
// path produced before the guards became conditional. That is what makes R2 a
// byte-identity claim rather than a self-consistency one: two runs of the same
// binary agreeing proves only determinism.
//
// Disposable, like every golden here (project convention — bless routinely).
// Re-bless it on a DELIBERATE economy/AI change, after confirming the move is
// the one you intended. A move you did not intend, in a change that only
// touches the spectating branch, means the unspectated path has been
// perturbed — which is the whole defect this row exists to catch.
// Recorded 2026-08-14: the harness was built and run against a stashed
// (pre-BL-409) corp_ai.cpp, which produced 3CBAD1D44EE71EDE, and the restored
// post-BL-409 build produced the same value. The unspectated path is byte-
// identical across the change. (That same pre-change build put the SPECTATED
// hash at 3CBAD1D44EE71EDE too — the flag was inert — so R3's "spectating moves
// the world" row correctly failed there. Both halves have teeth.)
// Re-blessed 2026-08-14 (later the same day): BL-386 made the sell-order floor
// a reservation price and re-tuned trade_floor_multiple 1.0 -> 0.25, which
// changes every rival's order floors and clearing income — a deliberate,
// item-designed economy move (its design predicted "expect this to move every
// economy golden"). That bless produced DD166049DA180508.
// Re-blessed 2026-08-15 (BL-424): the homeworld grid fell to 70% area
// (312x145 -> 261x121), a deliberate world change that moves every generated
// tile and everything downstream of it. Confirmed reproducible across two
// independent same-seed runs before blessing. MSVC-derived; this golden is
// toolchain-specific (float clearing arithmetic differs under g++).
// Re-blessed 2026-08-16 (NR-257): five resource_type values were REMOVED — grain,
// fodder, salt, transport_capacity, bullion — taking resource_count 42 -> 37. Every
// per-resource array this hash walks changed length, so the move is structural and
// expected rather than behavioural: no scorer, price or verb changed with it. The
// move was confirmed to be the intended one before blessing — the harness's other
// R2 row (two independently built worlds, same seed) reproduces 359F55D3EFC3CA21,
// and every other assertion in this file, including the prohibition and the cadence
// rows, passed unchanged across the removal.
// Re-blessed 2026-08-16 (BL-435 task B), same session as the bless above and for
// an unrelated reason: `focus_asset_pattern` moved the extraction corps' processor
// to a slot the holdings draw actually reaches, so affected corps now open 2
// extraction + 1 processing instead of 3 + 0. That changes generated holdings,
// which changes the world, which changes every downstream number this hash walks.
// A deliberate generation change is the case this file's policy names. Confirmed
// reproducible across two independently built worlds (84E876A1596FE090 twice)
// before blessing, and every other assertion — the prohibition, the cadence rows,
// the A/B seating rows — passed unchanged. The economy-wide income drop this
// generation change also caused is NOT this harness's business; it is measured
// and filed as BL-436.
// Re-blessed 2026-08-16 (BL-437 co-extraction): an extraction site now works
// EVERY deposit on its tile, sharing its unchanged capacity across them by
// richness, instead of spending all of it on the single richest. Five raws that
// were produced in zero quantity — coal, rare_earth_ore, sand, clay, peat — now
// flow, so pools, prices and every downstream number this hash walks change. A
// deliberate economy change, which is the case this file's policy names.
// Confirmed reproducible across two independently built worlds (AB53A812799BA2A4
// twice) before blessing; every other assertion passed unchanged, and
// ai_skill_harness's bands held without needing a bless of their own.
// Re-blessed 2026-08-16 (BL-435 dead-start fix): trade corps' holdings range
// went 1-2 -> 2-3, because a 1-draw opened a corp holding nothing but a port and
// a port produces nothing at all. That changes generated holdings on every seed,
// so the world changes and so does every number this hash walks. Confirmed
// reproducible across two independently built worlds (855E07DE529684EC twice)
// before blessing; every other assertion passed unchanged.
// Re-blessed 2026-08-21 (the four-lane batch + the two-level firm budget). Four
// deliberate, item-designed world changes landed between the previous value and
// this one, and every one of them moves the generated world by construction:
// BL-463 (settlement density now derived from land area and nation count, 31 flat
// -> 44-77 per seed), Sprint B2's road cuts (road-less nations 14 -> 0, plus
// open-ocean fragments removed), BL-466 (the province partition, built last in
// generation), and the two-level firm budget (per-resource and per-province caps
// replacing the flat 40, which changes how many background firms a body opens
// with and where they stand).
//
// Confirmed before blessing, which is the part that matters: the run is
// REPRODUCIBLE — two independently built worlds, same seed, both produced
// 344A9FE48306E93A, and R2's own reproducibility row passed alongside this one
// failing. So the move is a moved world, not a determinism leak. NR-404 had
// flagged two agents measuring different clean-base hashes for this harness; that
// resolves as each having measured a differently-generated world, not a fault.
// Every other assertion in this file — the prohibition, the cadence rows, the A/B
// seat rows, R3's divergence row — passed unchanged across all four changes.
// Re-blessed 2026-08-23 (BL-569, province holder): `world::province_holder` is
// now folded into `state_hash` unconditionally (world.cpp), which every
// generated world always seeds (seed_province_holders, called from
// make_hard_coded_world) — so this is a structural move present in EVERY
// world from this build forward, not a behavioural one; no scorer, price or
// verb changed. Confirmed reproducible: R2's own row (two independently built
// worlds, same seed) passed alongside this one, both producing
// 503FE8B540E58CFC, and every other assertion in this file — the prohibition,
// the cadence rows, the A/B seat rows, R3's divergence row — passed unchanged.
// Re-blessed 2026-08-23 (BL-571, nation garrisons), same Batch Delivery wave
// as the move above: garrisons are new units seeded into `world::units` at
// generation (`seed_nation_garrisons`, nation_generation.cpp), and
// `state_hash` folds every unit unconditionally — so every generated world
// now carries more units than before, a structural move for the same reason
// province_holder was. Confirmed reproducible: R2's own row passed alongside
// this one, both producing 607026DE2C20D27A, and every other assertion in
// this file passed unchanged.
// Re-blessed 2026-08-24 (BL-585, ancient goods append): four new
// `resource_type` values (`ceramics`, `dressed_stone`, `planks`, `tools`)
// widen `resource_count` 38 -> 42, which widens the length of EVERY
// per-resource array `state_hash` walks (stockpiles, prices, tile deposits,
// ...) — the same structural-move class NR-257's removal caused in the
// opposite direction, not a behavioural one; no scorer, price or verb
// changed. Confirmed reproducible: R2's own row (two independently built
// worlds, same seed) passed alongside this one, both producing
// 2FB3C201D7C4B1FA, and every other assertion in this file — the
// prohibition, the cadence rows, the A/B seat rows, R3's divergence row —
// passed unchanged.
//
// Re-blessed 2026-08-24 (BL-586 slice 2, hides/fibre/leather/cloth/rigging):
// five more new `resource_type` values widen `resource_count` 42 -> 47, the
// same structural-move class as the BL-585 re-bless immediately above.
// Confirmed reproducible across TWO independent builds of this exe (a normal
// incremental build and a from-scratch object/link rebuild), both producing
// 71273F6FEDE03965, and R2's own in-harness two-independently-built-worlds
// row passed alongside both.
//
// A DIFFERENTIAL CHECK was run beyond the standing policy's minimum, because
// this structural move ALSO flipped a second, unrelated assertion in this
// file ("R1 A/B seated + SPECTATED: it reaches every family it reached as a
// rival") that BL-585's own equivalent bump did not touch. To isolate
// whether that second failure was caused by the new ECONOMIC content (three
// new recipes, five new material-override entries) or by the bare structural
// widening, this hash was reproduced with scripts/recipes.lua and
// scripts/economy.lua TEMPORARILY reverted to their pre-slice-2 state (no
// tannery/weaver/shipwright, no hides/fibre material overrides) while the
// C++ resource_type enum stayed at its new width. The hash and the R1 A/B
// failure were BOTH identical either way — so the R1 A/B break is a
// consequence of the bare resource_count widening (most likely an RNG-stream
// shift from a resource_type-bounded loop that runs more iterations
// regardless of whether the new ids are populated), not of anything this
// item's new buildings/goods actually do. It is reported, not silently
// patched around — see this item's delivery report for the fuller note; the
// assertion itself is UNCHANGED here, per the standing rule against
// weakening a failing test to pass.
// Re-blessed 2026-08-25 (Sprint 19 wave 1). TWO layers of drift, both
// recorded: (1) the golden was ALREADY red on main at dc7d0554, before this
// sprint's merge — Agent E measured 0AF36395D78F3DAF there with baseline
// isolation (NR-634): some prior landing moved the world and nobody
// re-blessed with provenance, which this note now repairs by recording that
// the gap existed. (2) Sprint 19 wave 1 then legitimately re-rolled the
// generated world wholesale — demography-derived centre density (BL-610),
// urban ground (BL-612), anchored provinces (BL-611), nation qualification
// (BL-613) and wage clearing (BL-614). New value confirmed reproducible by
// R2's own two-independently-built-worlds row (95A7DAAC3E0693D2 twice) on
// the integrated tree at the wave-1 merge.
// Re-blessed again 2026-08-25, Sprint 19 wave 2 (same session as the wave-1
// bless above — the sprint moved the world twice): BL-616 rewrote every
// centre's growth/decline tick behaviour, BL-617 added the migration pass,
// BL-620 reshaped the generated road lattice (backbone over towns, village
// spurs) and BL-618 gates tier promotion on qualification (all-Track at the
// antiquity floor — NR-641). Reproducibility confirmed by R2's own
// two-independently-built-worlds row (DEC269E7941134D4 twice) on the
// integrated wave-2 tree.
// Re-blessed a third time 2026-08-25, still the same session: BL-621
// (era-relative road gates, Ben's ruling on NR-641) put a Roads backbone on
// the antiquity world that was all-Track for a few hours. Confirmed
// reproducible by R2's own two-independently-built-worlds row
// (EE1AC6587E0D7800 twice). This is the v0.1.19 cut's constant.
// Re-blessed a fourth and final time for the v0.1.19 recut, 2026-08-25:
// BL-623 (provinces before roads — the partition drops roads from its inputs
// and anchors join the lattice, +1,571 roaded tiles) and BL-624 (razed
// settlement tier, save v15). Confirmed reproducible by R2's own
// two-independently-built-worlds row (81ADF917369317C7 twice) on the merged
// wave-3 tree. This is the constant v0.1.19 ships with.
// Re-blessed 2026-08-26 (Sprint 20 wave 2). TWO layers again, and the first is
// the same failure the 2026-08-25 note above repaired — recorded rather than
// smoothed over, because it has now happened twice and that is the finding:
// (1) the golden was ALREADY red on main BEFORE this sprint's wave 2. BL-631's
// agent measured C91FB6ADA80B65CE against main's own unmodified sources and
// correctly declined to re-bless (NR-661), so something landed between the
// v0.1.19 recut and Sprint 20 that moved the world with no provenance line.
// That gap is acknowledged here; which landing caused it was not chased.
// (2) Sprint 20 wave 2 then moved it legitimately, three ways: BL-631 and
// BL-638 write ownership_class onto every corporation (a serialised field whose
// VALUES change on every seed), and BL-635 deleted a duplicate player unit
// entity — which state_hash folds unconditionally — while retuning the levy
// default (1.0 -> 0.1) and the whole unit_upkeep vector (x0.025).
// Confirmed reproducible by R2's own two-independently-built-worlds row
// (E350DF2A50BF4BAA twice) on the integrated wave-2 tree.
// NOTE ON TIMING: REFINED.md had assigned this sprint's re-bless to BL-630 in
// wave 3, once and late. Taken here instead, deliberately: wave 3 is now behind
// Sprint 21, and a known-red golden left standing for that long is how layer (1)
// above happens a third time. One bless, one provenance entry, measured first.
// (the constant itself is retired with the row — see the header above)

/// Hand-built registry mirroring scripts/economy.lua + scripts/recipes.lua for
/// the building types the generator places. Copied from ai_skill_harness so the
/// two instruments score against the same economy — a rollout that disagreed
/// with the skill harness's numbers for registry reasons would be unreadable.
recipe_registry make_registry()
{
    recipe_registry reg;

    building_economics extraction;
    extraction.base_rate            = 20.0f; // BL-436 calibration: mirrors economy.lua
    extraction.maintenance          = 5.0f;
    extraction.base_wage            = 8.0f;
    extraction.build_cost           = 100.0f;
    extraction.build_duration_ticks = 2.0f;
    extraction.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 20.0f;
    // BL-436: mirrors scripts/economy.lua's extraction_site block. See the same
    // note in ai_skill_harness — unset, the richness->rate conversion is disabled
    // and this harness measures the pre-BL-436 rate model while reporting green.
    extraction.richness_reference = 0.0f; // mirrors economy.lua: DISABLED pending BL-436 calibration
    extraction.richness_min       = 0.25f;
    extraction.richness_max       = 2.0f;
    reg.set_economics(building_type::extraction_site, extraction);

    building_economics processing;
    processing.base_rate            = 8.0f;  // BL-436 calibration: mirrors economy.lua
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
    launchpad.resource_build_cost[static_cast<std::size_t>(resource_type::steel)]        = 50.0f;
    launchpad.resource_build_cost[static_cast<std::size_t>(resource_type::refined_fuel)] = 20.0f;
    reg.set_economics(building_type::launchpad, launchpad);

    building_economics hub;
    hub.maintenance          = 12.0f;
    hub.base_wage            = 8.0f;
    hub.build_cost           = 250.0f;
    hub.build_duration_ticks = 3.0f;
    hub.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 30.0f;
    reg.set_economics(building_type::inland_logistics_hub, hub);

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

/// The four verb families BL-409's R1 names. `dial` is the cluster of
/// per-building adjustments the scorer treats as one budget (max_dials).
enum class verb_family { build, dial, survey, trade, other };

verb_family family_of(corp_verb v)
{
    switch (v)
    {
        case corp_verb::build:              return verb_family::build;
        case corp_verb::set_recipe:
        case corp_verb::set_workforce:
        case corp_verb::set_workforce_auto:
        case corp_verb::idle:
        case corp_verb::resume:             return verb_family::dial;
        case corp_verb::survey:             return verb_family::survey;
        case corp_verb::place_sell_order:
        case corp_verb::remove_sell_order:
        case corp_verb::request_quote:
        case corp_verb::accept_quote:
        case corp_verb::cancel_contract:    return verb_family::trade;
        case corp_verb::demolish:
        case corp_verb::place_road:
        case corp_verb::hire_unit:          return verb_family::other;
    }
    return verb_family::other;
}

const char* family_name(verb_family f)
{
    switch (f)
    {
        case verb_family::build:  return "build";
        case verb_family::dial:   return "dial";
        case verb_family::survey: return "survey";
        case verb_family::trade:  return "trade";
        case verb_family::other:  return "other";
    }
    return "?";
}

const char* verb_name(corp_verb v)
{
    switch (v)
    {
        case corp_verb::build:              return "build";
        case corp_verb::demolish:           return "demolish";
        case corp_verb::set_recipe:         return "set_recipe";
        case corp_verb::set_workforce:      return "set_workforce";
        case corp_verb::idle:               return "idle";
        case corp_verb::resume:             return "resume";
        case corp_verb::place_road:         return "place_road";
        case corp_verb::survey:             return "survey";
        case corp_verb::hire_unit:          return "hire_unit";
        case corp_verb::place_sell_order:   return "place_sell_order";
        case corp_verb::remove_sell_order:  return "remove_sell_order";
        case corp_verb::set_workforce_auto: return "set_workforce_auto";
        case corp_verb::request_quote:      return "request_quote";
        case corp_verb::accept_quote:       return "accept_quote";
        case corp_verb::cancel_contract:    return "cancel_contract";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// One rollout
// ---------------------------------------------------------------------------
struct rollout
{
    uint64_t                 final_hash      = 0;
    entity_id                player_id       = null_entity;
    int                      player_actions  = 0;
    int                      rival_actions   = 0;
    std::map<corp_verb, int> player_verbs;
    std::map<corp_verb, int> rival_verbs;
    std::map<verb_family, int> player_families;
    std::map<verb_family, int> rival_families;
    /// Per-corp family coverage — the comparison R1 is actually about. The
    /// pooled rival totals above hide that seven corps' worth of activity is
    /// not one corp's, and R1 compares the player against ONE rival.
    std::map<entity_id, std::map<verb_family, int>> per_corp_families;

    /// End-of-rollout economic position per corp. Present so a thin player
    /// verb tally can be read as either "the guard is still closed" or "this
    /// corp had nothing worth doing" — without it the two are indistinguishable
    /// and R1 becomes unfalsifiable.
    struct corp_state
    {
        float balance        = 0.0f;
        int   assets         = 0;
        int   operating      = 0;
        int   extractors     = 0;
        int   processors     = 0;
        int   auto_workforce = 0; ///< Buildings whose dial BL-181 already solves each tick.
    };
    std::map<entity_id, corp_state> per_corp_state;
};

/// How many of the four R1 families a corp reached.
int families_covered(const std::map<verb_family, int>& f)
{
    int n = 0;
    for (const verb_family fam : {verb_family::build, verb_family::dial,
                                  verb_family::survey, verb_family::trade})
    {
        const auto it = f.find(fam);
        if (it != f.end() && it->second > 0)
            ++n;
    }
    return n;
}

/// One full sim tick, matching ai_skill_harness's loop exactly (economy step →
/// market clearing → budget → tech gates). The tech-gate advance is not
/// optional garnish: without it no corp ever earns E0-ML-01, so military_base
/// can never complete and the hire/muster path silently zeroes out.
void run_tick(world& w, const recipe_registry& reg, int tick, bool spectating)
{
    w.current_day_tick  = tick;
    w.current_econ_tick = tick;
    const economy_report rep = run_economy_step(w, reg, spectating);
    const auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention, nullptr);
    advance_tech_gates(w);
}

/// The best-endowed corp in a freshly generated world, picked by a STABLE
/// property (most extraction sites; lowest entity id breaks the tie) rather
/// than by how it performed — picking the winner of a run and then asserting
/// it wins would be circular.
///
/// Exists for R1's controlled A/B. The generated player corp is a lean
/// specialist (CORPORATION_GENERATION.md) and on seed 0 it ends the rollout
/// insolvent with no extractor or processor, so it can reach almost no verb
/// family whether or not the guard is lifted — the poverty confounds the
/// measurement. Moving the VIEWPOINT onto a corp that demonstrably can act
/// removes the confound: the same corp, same world, same seed, differing only
/// in whether it is designated the player.
entity_id best_endowed_corp(const world& w)
{
    entity_id best = null_entity;
    int       best_extractors = -1;
    std::vector<entity_id> ids;
    ids.reserve(w.corporations.size());
    for (const auto& kv : w.corporations)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end()); // deterministic tie-break
    for (const entity_id corp : ids)
    {
        if (corp == w.player_entity)
            continue;
        int extractors = 0;
        for (const entity_id bid : w.corporations.at(corp).assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit != w.buildings.end() && bit->second.type == building_type::extraction_site)
                ++extractors;
        }
        if (extractors > best_extractors)
        {
            best_extractors = extractors;
            best            = corp;
        }
    }
    return best;
}

/// Build a FRESH world and roll it forward `ticks` ticks. Each call constructs
/// its own world — the determinism comparisons downstream are between two
/// independently generated worlds, not one world hashed twice.
///
/// @param viewpoint If not `null_entity`, this corp is designated the
///        player/viewpoint instead of the generated one, before the first tick.
///        R1's A/B uses it; every other caller leaves it null.
rollout run_rollout(uint32_t seed, int ticks, bool spectating,
                    entity_id viewpoint = null_entity)
{
    world_params params;
    params.seed = seed;
    world w = make_hard_coded_world(no_prehistory(params));
    const recipe_registry reg = make_registry();

    if (viewpoint != null_entity && w.corporations.count(viewpoint))
    {
        // Move the seat, do not add a second one: exactly one corp carries
        // is_player at any time, matching generation's invariant.
        for (auto& [corp, cc] : w.corporations)
            cc.is_player = (corp == viewpoint);
        w.player_entity = viewpoint;
    }

    const entity_id player = w.player_entity;

    rollout m;
    m.player_id = player;
    for (int t = 1; t <= ticks; ++t)
    {
        run_tick(w, reg, t, spectating);

        // Read the ring LIVE: it wraps at 256 entries, so a long rollout that
        // tallied only at the end would silently lose most of its evidence.
        for (const corp_decision& d : w.ai_decisions.entries)
        {
            if (d.tick != t)
                continue;
            ++m.per_corp_families[d.corp][family_of(d.command.verb)];
            const bool is_player = (d.corp == player);
            if (is_player)
            {
                ++m.player_actions;
                ++m.player_verbs[d.command.verb];
                ++m.player_families[family_of(d.command.verb)];
            }
            else
            {
                ++m.rival_actions;
                ++m.rival_verbs[d.command.verb];
                ++m.rival_families[family_of(d.command.verb)];
            }
        }
    }

    for (const auto& [corp, cc] : w.corporations)
    {
        rollout::corp_state st;
        st.balance = cc.balance;
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            const building_component& b = bit->second;
            ++st.assets;
            if (!b.decommissioned && b.ticks_remaining <= 0)
                ++st.operating;
            if (b.type == building_type::extraction_site)     ++st.extractors;
            if (b.type == building_type::processing_facility) ++st.processors;
            if (b.workforce_auto)                             ++st.auto_workforce;
        }
        m.per_corp_state[corp] = st;
    }

    m.final_hash = w.state_hash(ticks);
    return m;
}

void print_families(const char* who, const std::map<verb_family, int>& f)
{
    std::printf("     %s families:", who);
    for (const verb_family fam : {verb_family::build, verb_family::dial,
                                  verb_family::survey, verb_family::trade,
                                  verb_family::other})
    {
        const auto it = f.find(fam);
        std::printf(" %s=%d", family_name(fam), it == f.end() ? 0 : it->second);
    }
    std::printf("\n");
}

void print_verbs(const char* who, const std::map<corp_verb, int>& v)
{
    std::printf("     %s verbs:", who);
    if (v.empty())
        std::printf(" (none)");
    for (const auto& [verb, n] : v)
        std::printf(" %s=%d", verb_name(verb), n);
    std::printf("\n");
}

} // namespace

int main()
{
    std::printf("BL-409 spectator-no-seat harness (seed %u, %d ticks)\n", k_seed, k_ticks);

    // =====================================================================
    // Rollouts. Four worlds, each independently constructed.
    // =====================================================================
    const rollout played_a = run_rollout(k_seed, k_ticks, /*spectating=*/false);
    const rollout played_b = run_rollout(k_seed, k_ticks, /*spectating=*/false);
    const rollout spec_a   = run_rollout(k_seed, k_ticks, /*spectating=*/true);
    const rollout spec_b   = run_rollout(k_seed, k_ticks, /*spectating=*/true);

    std::printf("\n  hashes: played=%016llX / %016llX   spectated=%016llX / %016llX\n",
                static_cast<unsigned long long>(played_a.final_hash),
                static_cast<unsigned long long>(played_b.final_hash),
                static_cast<unsigned long long>(spec_a.final_hash),
                static_cast<unsigned long long>(spec_b.final_hash));

    // =====================================================================
    // Anti-vacuity guard — is there an economy here at all?
    // =====================================================================
    // Runs BEFORE any player conclusion. If the registry were hollow or the
    // rollout too short, the rivals would sit idle, every R1 player check
    // would hold for the wrong reason, and this is the row that fails.
    std::printf("\n-- anti-vacuity: the rollout has a live economy --\n");
    print_families("rival ", played_a.rival_families);
    print_verbs("rival ", played_a.rival_verbs);
    {
        int rival_families_seen = 0;
        for (const verb_family f : {verb_family::build, verb_family::dial,
                                    verb_family::survey, verb_family::trade})
            if (played_a.rival_families.count(f) && played_a.rival_families.at(f) > 0)
                ++rival_families_seen;
        check(played_a.rival_actions > 0,
              "anti-vacuity: rival corps act at all (a hollow registry would fail here)");
        check(rival_families_seen >= 3,
              "anti-vacuity: rivals reach >=3 of the four verb families named by R1");
    }

    // =====================================================================
    // R2 — spectating OFF is unchanged. THE LOAD-BEARING ONE.
    // =====================================================================
    std::printf("\n-- R2: an unspectated run is untouched by BL-409 --\n");
    print_verbs("player", played_a.player_verbs);

    check(played_a.player_actions == 0,
          "R2 the prohibition still binds: the player corp issues ZERO decisions unspectated");
    check(played_a.final_hash == played_b.final_hash,
          "R2 an unspectated run is reproducible (two independently built worlds, same hash)");
    // RETIRED (Ben, 2026-09-01, ruling on NR-752): the byte-identity row
    // against a frozen pre-BL-409 constant. It asserted world-content
    // stability, not a spectator-mode property, and was re-blessed ten times
    // for changes that had nothing to do with this harness. The full argument
    // and the dated log are at the top of this file. The hash is still PRINTED,
    // because a value nobody asserts is still worth being able to read when
    // diagnosing a real divergence.
    std::printf("     unspectated state_hash = %016llX (reported, not asserted)\n",
                static_cast<unsigned long long>(played_a.final_hash));

    // =====================================================================
    // R3 — a spectated run is itself deterministic.
    // =====================================================================
    std::printf("\n-- R3: a spectated run is deterministic --\n");
    check(spec_a.final_hash == spec_b.final_hash,
          "R3 same seed + spectating + N ticks, run twice, identical state_hash");
    check(spec_a.player_actions == spec_b.player_actions &&
          spec_a.player_verbs == spec_b.player_verbs,
          "R3 the player corp's own decision stream is reproducible under spectate");

    // The flag must actually do something, or R3 would pass on a no-op.
    check(spec_a.final_hash != played_a.final_hash,
          "R3 spectating moves the world (a spectated run diverges from a played one)");

    // =====================================================================
    // R1 — under spectate the player corp acts like a rival.
    // =====================================================================
    std::printf("\n-- R1: the player corp is scored like any other --\n");
    print_families("player", spec_a.player_families);
    print_verbs("player", spec_a.player_verbs);

    check(spec_a.player_actions > 0,
          "R1 the player corp issues decisions under spectate");

    // The generated player corp's own numbers are REPORTED, not asserted on.
    // On seed 0 it is a lean specialist that ends the rollout insolvent with
    // no extraction site and no processing facility, so the scorer has almost
    // no candidate to offer it — build and survey are Nice-to-Have and gated
    // by the solvency floor, the workforce/recipe dials only enumerate for
    // extractors and processors, and a corp producing nothing accumulates no
    // surplus to list on the order book. Two RIVALS in the same position
    // (60478, 60480 on this seed) do just as little. Asserting a four-family
    // bar here would be asserting that this corp is rich, which is not what
    // BL-409 changed; the A/B below isolates the guard instead.
    {
        std::printf("     per-corp position at tick %d (spectated run):\n", k_ticks);
        for (const auto& [corp, st] : spec_a.per_corp_state)
            std::printf("       corp %-5u %s bal=%10.1f assets=%d op=%d extr=%d proc=%d wf_auto=%d\n",
                        corp, (corp == spec_a.player_id) ? "(viewpoint)" : "           ",
                        st.balance, st.assets, st.operating, st.extractors,
                        st.processors, st.auto_workforce);

        std::printf("     per-corp family coverage (of 4), spectated run:\n");
        int best_rival = 0, worst_rival = 4, rival_corps = 0;
        for (const auto& [corp, fams] : spec_a.per_corp_families)
        {
            const int n = families_covered(fams);
            const bool is_player = (corp == spec_a.player_id);
            std::printf("       corp %-5u %s  %d/4 :", corp, is_player ? "(viewpoint)" : "           ", n);
            for (const verb_family fam : {verb_family::build, verb_family::dial,
                                          verb_family::survey, verb_family::trade})
            {
                const auto it = fams.find(fam);
                std::printf(" %s=%d", family_name(fam), it == fams.end() ? 0 : it->second);
            }
            std::printf("\n");
            if (!is_player)
            {
                ++rival_corps;
                best_rival  = std::max(best_rival, n);
                worst_rival = std::min(worst_rival, n);
            }
        }

        const int player_cov = families_covered(spec_a.player_families);
        std::printf("     rivals: %d corps, coverage %d..%d of 4;  generated player: %d of 4"
                    "  (reported, not asserted — see note above)\n",
                    rival_corps, worst_rival, best_rival, player_cov);
    }

    // --- R1, the controlled A/B: the same corp, seated and unseated ---------
    // The decisive test, free of the poverty confound. Take the best-endowed
    // corp — one that demonstrably CAN reach every family — and designate it
    // the viewpoint. Same seed, same world, same corp; the only variable is
    // whether it holds the seat and whether the session is spectated.
    //
    //   seated + played    -> silent. The prohibition binds whoever holds the
    //                         seat, so a corp that was busy as a rival goes to
    //                         zero the moment it becomes the player. This is
    //                         the row that would catch the guard being lifted
    //                         unconditionally.
    //   seated + spectated -> acts, and reaches what it reached as a rival.
    {
        std::printf("\n-- R1 A/B: the same corp, seated as the viewpoint --\n");

        world_params params;
        params.seed = k_seed;
        const world probe = make_hard_coded_world(no_prehistory(params));
        const entity_id strong = best_endowed_corp(probe);

        // What that corp achieves as an ordinary RIVAL (the baseline).
        const auto rival_fams = spec_a.per_corp_families.find(strong);
        const int  as_rival   = (rival_fams == spec_a.per_corp_families.end())
                                    ? 0 : families_covered(rival_fams->second);

        const rollout seated_played = run_rollout(k_seed, k_ticks, /*spectating=*/false, strong);
        const rollout seated_spec   = run_rollout(k_seed, k_ticks, /*spectating=*/true,  strong);
        const int as_seated = families_covered(seated_spec.player_families);

        std::printf("     corp %u: %d/4 as a rival, %d/4 seated+spectated, "
                    "%d decisions seated+played\n",
                    strong, as_rival, as_seated, seated_played.player_actions);
        print_verbs("seated+spectated", seated_spec.player_verbs);

        check(strong != null_entity && strong != probe.player_entity,
              "R1 A/B setup: a best-endowed rival corp was found to seat");
        check(as_rival >= 3,
              "R1 A/B setup: that corp is genuinely capable (>=3 families as a rival)");
        check(seated_played.player_actions == 0,
              "R1 A/B seated + PLAYED: the corp falls silent — the prohibition follows the seat");
        check(seated_spec.player_actions > 0,
              "R1 A/B seated + SPECTATED: the corp acts — the guard lifts with no seat");

        // RETIRED (Ben, 2026-08-24): this row asserted as_seated >= as_rival —
        // family coverage transfers 1:1 between the rival and seated+spectated
        // roles. BL-586 slice 2's resource_count widening (42 -> 47) shifted
        // this specific rollout's RNG stream and broke it; a differential
        // check (recipes.lua/economy.lua reverted, enum width unchanged)
        // proved the break tracks the width alone, not the new content — see
        // this file's k_unspectated_golden comment for the full trail. Put to
        // Ben rather than silently patched or silently left failing: ruled
        // that BIT-IDENTICAL RNG-stream determinism is not the property this
        // harness needs to hold beyond what R2/R3 already prove (saves carry
        // the actual state, not a replay-from-seed; occasional randomness is
        // a deliberate strategy lever, not a defect). The two properties
        // io-standing-rules.md's BL-409 section actually cites — defaults
        // false, no rival's cadence slot shifts — are unaffected and still
        // asserted above and below. Reported here rather than deleted
        // outright, so a future width bump doesn't have to rediscover this.
    }

    // --- R1, the schedule half: cadence identity, checked directly ---------
    // The behavioural half above says the player acts. This says it acts on
    // the SAME clock, and — the claim BL-409's design rests on — that admitting
    // it moves nobody else's slot, because the cadence keys on an index into
    // the sorted FULL corp set, which already contained the player.
    {
        world_params params;
        params.seed = k_seed;
        const world w = make_hard_coded_world(no_prehistory(params));

        corp_ai_params played{};
        corp_ai_params spectating{};
        spectating.spectating = true;

        std::vector<entity_id> corp_ids;
        corp_ids.reserve(w.corporations.size());
        for (const auto& kv : w.corporations)
            corp_ids.push_back(kv.first);
        std::sort(corp_ids.begin(), corp_ids.end());

        const auto pit = std::find(corp_ids.begin(), corp_ids.end(), w.player_entity);
        const bool player_is_a_corp = (pit != corp_ids.end());
        check(player_is_a_corp, "R1 setup: world::player_entity is a corporation in the sorted set");

        const int k = std::max(1, played.cadence_k);
        bool player_due_matches_index = player_is_a_corp;
        bool player_never_due_played  = true;
        bool rivals_unmoved           = true;

        if (player_is_a_corp)
        {
            const int p_index = static_cast<int>(pit - corp_ids.begin());
            for (int t = 0; t < 4 * k; ++t)
            {
                const bool expect = ((t % k) == (p_index % k));
                if (corp_strategic_eval_due(w, w.player_entity, t, spectating) != expect)
                    player_due_matches_index = false;
                if (corp_strategic_eval_due(w, w.player_entity, t, played))
                    player_never_due_played = false;

                for (std::size_t i = 0; i < corp_ids.size(); ++i)
                {
                    if (corp_ids[i] == w.player_entity)
                        continue;
                    if (corp_strategic_eval_due(w, corp_ids[i], t, played) !=
                        corp_strategic_eval_due(w, corp_ids[i], t, spectating))
                        rivals_unmoved = false;
                }
            }
            std::printf("     %zu corps, cadence_k=%d, player at sorted index %d\n",
                        corp_ids.size(), k, p_index);
        }

        check(player_due_matches_index,
              "R1 under spectate the player corp is due exactly at its own sorted-index slot");
        check(player_never_due_played,
              "R1 unspectated the player corp is NEVER due, on any tick (the prohibition)");
        check(rivals_unmoved,
              "R1 admitting the player shifts NO rival's cadence slot (index is over the full set)");
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
