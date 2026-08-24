#pragma once

#include "entity.hpp"
#include "nation_budget.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

struct world;

// ---------------------------------------------------------------------------
// The nation scorer (BL-542) — what a nation CARES ABOUT
// ---------------------------------------------------------------------------
// `run_national_budget` (BL-537) consumes a `nation_budget` weight vector and
// nothing authored one. This authors it. It is the FIRST USE anyone has made of
// the 2026-08-18 nation-behaviour grant (io-standing-rules.md § Determinism &
// data model, ruling 4 of NR-331), and the grant's terms are this file's
// contract, not a preamble to it:
//
//   PURE, SEEDED, DETERMINISTIC, REPLAYABLE. `score_national_budgets` takes a
//   const world and returns weights. It writes no world state, reads no clock,
//   draws no random number, and every accumulation whose result reaches a float
//   is walked in a pinned order (ascending entity id; the authored enum order
//   for resources and lines). `w.nations`, `w.tiles`, `w.units` and
//   `w.corporations` are unordered_maps and float addition is not associative,
//   so an unordered walk that summed into a float would make the campaign
//   unreplayable. There is exactly one unordered walk here — building the
//   per-body raster in `raster_index` — and it WRITES each cell once from a
//   unique key, so no accumulation and no ordering can reach the result.
//
//   A SCORED-UTILITY LAYER, NEVER A PLANNER. Three terms are evaluated over
//   facts the world already holds, mapped onto the nine authored lines, and
//   returned. Nothing here looks ahead, holds a goal across ticks, remembers a
//   previous decision, or carries state of its own.
//
//   NOTHING THAT VARIES THE GENERATED WORLD. This runs in the campaign tick and
//   reads generation output; it never writes back into it.
//
// NOT WIRED. No caller in the shipped tick invokes this, deliberately: a world
// runs bit-identical to the pre-BL-542 build. Wiring it is the item that gives
// the weights a consumer.
//
// ---------------------------------------------------------------------------
// THE OBJECTIVE IS POSITIONAL, NOT ACCUMULATIVE
// ---------------------------------------------------------------------------
// (NATIONS.md § Settled 5, Ben 2026-08-22.) A corporation maximises profit —
// unbounded, accumulative, identical for every firm. A nation does not want to
// be rich; it wants to be NEEDED and UNTHREATENED. Three terms, in this fixed
// order, and the order is fixed because the third biases the first two:
//
//   1. NICHE FIT. How well this nation's output profile complements what its
//      NEIGHBOURS lack. Read from `nation_component::resource_abundance` as a
//      SHARE, never a magnitude — a nation scores well by producing what others
//      need and cannot make, never by producing the most. Weighted by the
//      resolved market price on its own ground, so a niche in something nobody
//      values is not a niche.
//
//   2. CONFLICT AVOIDANCE. A NEGATIVE term on the expected cost of war, read
//      from force standing on the far side of its own borders and from
//      `stance.hpp`'s pair-state between the corps involved. A well-played
//      nation in Io stays out of fights; that is intended, and it is much of
//      why the layer is worth having — nations avoiding each other leaves the
//      Conflict pillar to the companies, which is where the player lives.
//
//   3. GRUDGES AS WEIGHTS, NOT GOALS. The pre-history residue does not give a
//      nation a target. It BIASES the first two, multiplicatively and
//      per-neighbour: a grudge makes that neighbour's niche LESS attractive to
//      complement (`niche_grudge_bias`) and a conflict with them LESS costly to
//      accept (`conflict_grudge_bias`). It contributes to no line of its own,
//      which is the property that keeps the pre-history load-bearing instead of
//      turning it into a revenge goal. With terms 1 and 2 both at zero a grudge
//      changes NOTHING — asserted, not assumed.
//
// "TERRESTRIAL" IS A CONSTRAINT, NOT A FLAVOUR WORD (Ben). A nation's horizon
// is its neighbours and its own ground — never the whole world, never off it.
// So: neighbours are the nations sharing a tile edge; prices are read from the
// markets on the bodies this nation actually holds tiles on; force is counted
// only where it stands against this nation's own border. Nothing here sums over
// every nation, every market, or every body. It is also what keeps the scorer
// cheap across ~43 of them, and it is what makes the locality row (R6) a real
// assertion rather than a tautology.
// ---------------------------------------------------------------------------

/// Tunables for the nation scorer. EVERY constant the scorer uses lives here, so
/// tuning it is a data change — the `corp_ai_params` discipline, for the same
/// reason: a number buried in the .cpp is a number nobody re-tunes.
struct nation_ai_params
{
    // --- Cadence ------------------------------------------------------------
    /// Nation n is re-scored on the tick where `tick % cadence_k == index(n) %
    /// cadence_k`, with `index` its position in the ASCENDING-ID nation set —
    /// the `corp_ai.cpp` stagger, and for its reason. Keying on the sorted set
    /// (rather than on the raw id, or on a hash) is what makes admitting a
    /// nation shift nobody else's slot when the new id sorts last, which is the
    /// case a world that grows a nation actually produces. Verified by R2.
    int cadence_k = 4;

    // --- Term 1: niche fit --------------------------------------------------
    /// A neighbour is counted as LACKING resource r by how far its own output
    /// share falls below this nation's. Raising this makes a nation harder to
    /// impress by a neighbour's shortfall; it does not change the ordering.
    float lack_scale = 1.0f;

    /// Floor on the price weight, so a resource with no resolved market price on
    /// this nation's ground still counts for something rather than vanishing.
    /// A niche nobody prices is a weak niche, not a non-existent one.
    float price_floor = 0.25f;

    /// Divisor turning summed complement into the [0, 1] band the line mapping
    /// reads. Set so a nation with two neighbours and a clean complement lands
    /// near 0.4 rather than pinned at the clamp — a term sitting ON a clamp is a
    /// term no mutation can move, which is how a scorer passes a suite while
    /// computing nothing. It is a scale, not a threshold; nothing branches on it.
    float niche_scale = 1.00f;

    /// Same, for the UNFILLABLE half — what neighbours lack that this nation
    /// cannot supply either. That is the term that pushes exploration and the
    /// civil tech ladder rather than charters.
    float gap_scale = 0.50f;

    // --- Term 2: conflict avoidance ----------------------------------------
    /// Force standing on the far side of a border is scaled by this before it is
    /// read as threat. One unit of `count` at full supply contributes 1.0.
    float force_scale = 1.0f;

    /// How much a HOSTILE pair-state amplifies a border force. A neighbour's
    /// army whose owner has declared hostility toward a corp registered here is
    /// worth `1 + hostility_amplifier` of the same army with no declaration —
    /// the declared state is a multiplier on force, never a substitute for it,
    /// so a hostile corp with no army near the border threatens nothing.
    float hostility_amplifier = 2.0f;

    /// Own force standing on this nation's own ground DAMPS the threat rather
    /// than cancelling it: `threat / (1 + deterrence_scale * own_force)`. A
    /// nation that already holds the ground it is worried about spends less on
    /// worrying. Zero disables deterrence entirely.
    float deterrence_scale = 0.5f;

    /// Divisor turning summed threat into the [0, 1] band the mapping reads:
    /// roughly "the supplied unit-equivalents standing hostile on a border at
    /// which this nation feels fully threatened". A power of two so the division
    /// is exact and a low-bit difference upstream survives into the weights
    /// rather than being rounded away — which is what makes the determinism row
    /// able to fail.
    float threat_scale = 1024.0f;

    // --- Term 3: grudges ----------------------------------------------------
    /// A grudge of 1.0 multiplies a neighbour's niche contribution by
    /// `1 - niche_grudge_bias` — "less attractive to complement".
    float niche_grudge_bias = 0.6f;

    /// A grudge of 1.0 multiplies that neighbour's threat contribution by
    /// `1 - conflict_grudge_bias` — "a conflict with them less costly to
    /// accept", so the nation buys less avoidance against an old enemy.
    float conflict_grudge_bias = 0.5f;

    /// The border length (in shared tile edges) at which the border-contest half
    /// of the grudge saturates. See `nation_grudge` in the .cpp for what the
    /// grudge is derived FROM and what it is standing in for.
    float grudge_border_saturation = 24.0f;

    /// Weight of the two grudge halves — contested border, and the pair's
    /// generated `expansionism` posture. They sum to the grudge, clamped to 1.
    float grudge_border_weight  = 0.5f;
    float grudge_posture_weight = 0.5f;

    // --- The line mapping ---------------------------------------------------
    // Which term pushes which of the nine `budget_priority` lines, and why, is
    // the table in nation_ai.cpp § The mapping. These are its coefficients.
    /// Floor every line carries before any term is applied. Keeps the vector
    /// non-degenerate (a nation always has SOME opinion) and sets how strongly
    /// the terms can differentiate two nations. Zero is legal and makes an
    /// unthreatened nation with no niche spend on nothing at all.
    float base_weight = 0.02f;

    float niche_charters   = 1.00f; ///< Pay a corp to exist where the niche is.
    float niche_works      = 0.60f; ///< Build the capacity the niche needs.
    float niche_logistics  = 0.80f; ///< A niche you cannot deliver is not one.
    float gap_exploration  = 0.90f; ///< Look for the ground you do not know you have.
    float gap_academic     = 0.70f; ///< Make what you cannot dig up.
    float calm_schooling   = 0.80f; ///< The long-horizon civil line, deferred under threat.
    float calm_academic    = 0.40f;
    float calm_works       = 0.30f;
    float threat_contracted = 1.20f; ///< Buy force it does not raise (CONTRACTS.md).
    float threat_reserve    = 0.70f; ///< Hold goods against disruption.
    float threat_milres     = 0.60f; ///< Lower the cost of the war you cannot avoid.

    // --- The savings dial ---------------------------------------------------
    /// `reserve_fraction = base_reserve + calm_reserve_bonus * calm`, clamped to
    /// [0, 1]. A nation with nothing to fear withholds more of its treasury; a
    /// threatened one spends. Both are `nation_budget`'s own dial, not a line.
    float base_reserve       = 0.10f;
    float calm_reserve_bonus = 0.25f;
};

/// The three terms, per nation, exactly as they were computed. Reported, never
/// stored: a harness asserts on them and a future surface can say "this nation
/// is spending on force because of the army on its northern border", which is
/// the difference between a legible scorer and an opaque one.
struct nation_score_terms
{
    entity_id nation = null_entity;

    /// Term 1a — complement this nation CAN supply, grudge-biased. [0, 1].
    float niche_fit = 0.0f;
    /// Term 1b — complement its neighbours want that it CANNOT supply. [0, 1].
    float niche_gap = 0.0f;
    /// Term 2 — expected war cost, grudge-biased and deterrence-damped. [0, 1].
    float threat = 0.0f;
    /// `1 - threat`. Named because the mapping reads it directly.
    float calm = 1.0f;

    /// Term 3, reported only — the mean grudge over this nation's neighbours.
    /// It weights nothing on its own; it appears here so a row can assert that
    /// a grudge moved terms 1 and 2 and introduced no line of its own.
    float mean_grudge = 0.0f;

    /// Neighbours sharing at least one tile edge, ascending. THE HORIZON: every
    /// term above is summed over exactly this set and nothing wider.
    std::vector<entity_id> neighbours;

    /// Border force read as threatening, before deterrence damping. Diagnostic.
    float border_force = 0.0f;
    /// This nation's own force on its own ground. Diagnostic.
    float own_force = 0.0f;
};

/// What one sweep did. The anti-vacuity record: a sweep that covered no nation,
/// or covered nations and produced nothing, says so here rather than passing
/// silently as an empty map.
struct nation_scorer_report
{
    /// Nations DUE this tick, ascending — the same set the returned map keys on.
    std::vector<nation_score_terms> scored;

    /// Nations that exist and hold territory, whether due this tick or not.
    /// `scored.size()` should be roughly `eligible / cadence_k`.
    int eligible = 0;

    /// Nations skipped because they hold no tiles (a nation with no ground has
    /// no neighbours, no niche and no border, so it has no opinion).
    int skipped_landless = 0;
};

/// Score one economy tick's national budgets.
///
/// Returns the weight vector for exactly the nations DUE on `econ_tick` under
/// the staggered cadence — not for every nation. That is the `corp_ai` shape and
/// it is deliberate: a caller holding a persistent per-nation map OVERWRITES the
/// returned entries and leaves the rest standing, so re-scoring on a rotation
/// does not make a nation spend one tick in `cadence_k`. A caller that instead
/// passes the returned map straight to `run_national_budget` gets exactly that
/// one-in-k behaviour, and should not.
///
/// Weights are returned already NORMALISED to sum to 1.0 (`nation_budget`
/// normalises on read anyway; doing it here means the vector has one meaning
/// rather than two). `reserve_fraction` is in [0, 1].
///
/// PURE. `w` is not modified, and no static or thread-local state is touched.
///
/// @param w         World. Read-only; nations, tiles, markets, units,
///                  corporations and the stance tables are all read.
/// @param p         Tunables. Defaults are the accepted-design values.
/// @param econ_tick The economy tick, for the cadence stagger only. Nothing
///                  else in the scorer reads it — there is no history term.
/// @param report    Optional sink for the per-nation term breakdown and the
///                  coverage counts. Null for callers that only want weights.
std::map<entity_id, nation_budget> score_national_budgets(const world& w,
                                                          const nation_ai_params& p,
                                                          int econ_tick,
                                                          nation_scorer_report* report = nullptr);

/// True iff nation `n` is re-scored on `econ_tick` under `p`'s cadence. Exposed
/// so a caller (and R2) can ask the question without running the sweep. Returns
/// false for a nation `w` does not hold.
bool nation_is_due(const world& w, entity_id n, int econ_tick, const nation_ai_params& p);

/// The grudge this nation holds toward `other`, in [0, 1]. Exposed so R5 can
/// assert on it directly, and so the SUBSTITUTE it currently is (see the .cpp)
/// is replaceable in one place when BL-541 lands the real Era -1 pair outcomes.
/// Zero for a pair that shares no border, and for `n == other`.
float nation_grudge(const world& w, entity_id n, entity_id other, const nation_ai_params& p);

/// The nation `n` shares its highest grudge with, among nations bordering it
/// on at least one tile edge — deterministic argmax over `nation_grudge`,
/// ties broken by ascending id (the same idiom `seed_nation_garrisons`,
/// nation_generation.cpp, uses for its own highest-grudge pick, and
/// `score_national_budgets`'s own tie-breaks throughout this file).
///
/// Exposed so a second consumer (`derive_contract_offers`, BL-572,
/// nation_step.cpp) can name the same neighbour a garrison would be seeded
/// against, without re-deriving the border walk a third time.
///
/// `null_entity` for a nation `w` does not hold, or one with no bordering
/// nation at all (a landless nation, or an island with no tile-adjacent
/// neighbour).
entity_id nation_highest_grudge_neighbour(const world& w, entity_id n, const nation_ai_params& p);
