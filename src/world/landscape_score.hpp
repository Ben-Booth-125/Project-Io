#pragma once

// ---------------------------------------------------------------------------
// landscape_score — BL-770 slice 1. What phase 6 searches ON.
//
// Ben's phase 6 (GENERATION_STRATEGY.md § The eight phases) picks a corporate
// landscape that is VIABLE BUT UNEVEN, by a STATIC search: roads, borders,
// markets, deposits and population are all settled by the end of phase 4 and
// none of them moves during phase 6, so there is nothing to step and no clock.
//
// THE OBJECTIVE HAS THREE TERMS (Ben, 2026-09-06, on the market-work form):
//
//   1. CHAIN COMPLETENESS  — terminals closed over terminals total, per market.
//      Already built: measure_market_completeness (BL-775).
//   2. THE SUPPLY-TO-DEMAND RATIO per resource per market. The static
//      price-feedback proxy. Completeness says a chain CAN close; it cannot see
//      a good pinned at a band edge, and a good pinned at the ceiling is what
//      actually killed the industrial field — processors buying inputs at 10x
//      and being idled as loss-making.
//   3. THE SPREAD of the first two across markets, REWARDED FOR UNEVENNESS.
//      Scored for, not tolerated. An even map is the outcome
//      § Asymmetry is the deliverable exists to prevent, and an objective that
//      is merely neutral about evenness will drift into it.
//
// RECIPE MARGIN IS DELIBERATELY NOT A TERM. It exists (evaluate_margin, in
// market_saturation.hpp) and it stays the AUTHORING GATE that every recipe can
// pay — a precondition of the search, not an axis it trades against. It is
// near-constant across candidates that differ only in roster, placement and road
// tier, so scoring it would add a constant to every candidate and pull the
// objective back toward "most profitable", which is the reading Ben's point 4
// rejected.
//
// THE PROXY'S CALIBRATION DOES NOT HAVE TO BE RIGHT, and that is the point worth
// understanding before tuning any constant below. This scorer RANKS candidates
// against one fixed world. Every candidate is scored by the same weights, so a
// weight that is wrong in absolute terms shifts every candidate's score together
// and leaves the ORDER — the only output that matters — untouched. What would
// break the ranking is a weight that is wrong DIFFERENTLY for different
// candidates, which is why nothing here reads a price, a tick or a corp balance.
// ---------------------------------------------------------------------------

#include "market_saturation.hpp"

#include <array>
#include <cstddef>
#include <vector>

class recipe_registry;

/// Weights for the static demand proxy. See the header note: these need to be
/// STABLE across candidates, not calibrated against reality.
struct landscape_score_params
{
    /// Household demand per head, for a resource the band's household basket names.
    double household_per_head = 1.0;
    /// Flat weight for each other structural market sink (process, construct,
    /// background, unit upkeep, industry upkeep, endemic).
    double sink_weight = 250.0;

    /// A resource is BALANCED when its supply:demand ratio sits inside
    /// [1/pin_ratio, pin_ratio]. Outside it the good is pinned — glutted below,
    /// starved above — which is the static shadow of a price at a band edge.
    /// 4.0 against the authored band of [0.25x, 10x] is deliberately INSIDE the
    /// band: a good does not have to reach the clamp to be a broken market.
    double pin_ratio = 4.0;

    /// How hard unevenness is rewarded in the composite. 0 scores viability
    /// alone; the composite is viability * (1 + unevenness_gain * spread).
    double unevenness_gain = 0.5;
};

/// One market's static reading.
struct market_score
{
    entity_id market      = null_entity;
    entity_id body        = null_entity;
    double completeness   = 0.0;  ///< POTENTIAL: terminals closed / terminals total
    /// ACTUAL: the same closure, seeded from what buildings in this catchment
    /// really extract and really run, rather than from what the ground could
    /// yield and the era permits. See § The roster-aware term in the .cpp.
    double actual         = 0.0;
    int    actual_closed  = 0;
    int    extractors     = 0;    ///< extraction buildings in the catchment
    int    processors     = 0;    ///< processing buildings running a real recipe
    int    balanced       = 0;    ///< resources whose ratio sits inside the pin band
    int    glutted        = 0;    ///< supply >> demand
    int    starved        = 0;    ///< demand >> supply (the ceiling shadow)
    int    rated          = 0;    ///< resources with any signal at all
    double balance        = 0.0;  ///< balanced / rated
};

/// The whole landscape, as one comparable record.
struct landscape_score
{
    std::vector<market_score> markets;

    // --- term 1 and 2, as levels ---
    double mean_completeness = 0.0;  ///< potential
    double mean_actual       = 0.0;  ///< the roster-aware term (BL-770 slice 2)
    double mean_balance      = 0.0;

    /// THE RATIO THAT MAKES THE OBJECTIVE ROSTER-AWARE: actual / potential. How
    /// much of the opportunity this world offers does THIS roster take up? A
    /// landscape with rich ground and no firms scores near 0; one whose firms
    /// close every chain the ground allows scores 1.
    double realisation = 0.0;

    // --- term 3, as spread. Population-free: these are spreads OVER MARKETS,
    //     so a landscape with rich and poor markets scores above a flat one at
    //     the same mean.
    double completeness_spread = 0.0; ///< coefficient of variation over markets
    double balance_spread      = 0.0;
    double spread              = 0.0; ///< the two combined

    /// viability * (1 + unevenness_gain * spread). The total order phase 6's
    /// argmax will eventually run on.
    double composite = 0.0;

    int market_count = 0;
};

/// Score one candidate landscape against a fixed world.
///
/// DETERMINISTIC AND SIDE-EFFECT-FREE ON THE SCORE, BUT NOT `const world&`, and
/// the distinction matters for the parallel search this is eventually for.
/// `measure_market_completeness` calls `body_reach_field`, which MEMOISES the
/// per-body reach field into the world. So scoring warms a cache: the score is a
/// pure function of the world's content and never varies with call order, but
/// the call mutates shared state.
///
/// THE CONSEQUENCE FOR PHASE 6: candidates must not share one `world` object
/// across threads. Either give each candidate its own copy, or warm the reach
/// fields once, single-threaded, before any candidate is scored. This was
/// documented as "writes nothing" in the first cut, which was simply wrong and
/// would have been found the expensive way.
landscape_score score_landscape(world& w, const recipe_registry& reg,
                                const landscape_score_params& p = {});

/// The same, when the caller already holds a classification (the harness scores
/// several candidates against one band and should not re-derive it each time).
landscape_score score_landscape(world& w, const recipe_registry& reg,
                                const std::array<resource_classification, resource_count>& cls,
                                const landscape_score_params& p);
