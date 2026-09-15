#pragma once

// ---------------------------------------------------------------------------
// market_saturation — BL-775. Can a market close its chains, and can each part
// of it pay for itself?
//
// PROMOTED OUT OF A HARNESS, and that is the point. Both measures below were
// written inside the anonymous namespace of tools/verify/demand_census.cpp, so
// nothing in src/ could call them: generation could not use the measures its own
// verification already trusted. Ben's phase 6 (GENERATION_STRATEGY.md § The
// eight phases) needs exactly them — "given a planet with markets and national
// borders, how can we saturate all the resources in said market, so that each
// part makes some profit?" — so they move here and the census calls the promoted
// copy.
//
// TWO CALLERS, ONE IMPLEMENTATION. That is the whole reason to move rather than
// copy. era_minus_one.hpp records what the alternative costs: six divergences
// between what generation ran and what every harness measured, and a seventh
// caller nobody counted. A saturation measure computed one way by generation and
// another by the census would be that defect in a new place.
//
// THE BODIES ARE UNCHANGED. Only names moved — `classification` and `classify`
// were too generic for src/world (both already collide there), so they are
// `resource_classification` and `classify_resources`; `measure_completeness` is
// `measure_market_completeness`. The `terminal_names_out` out-param is gone
// because it existed only for the census's own printing.
// ---------------------------------------------------------------------------

#include "components.hpp"
#include "world.hpp"

#include <array>
#include <cstddef>
#include <vector>

class recipe_registry;

struct resource_classification
{
    // Produced in this band?
    bool produced_by_recipe = false;  ///< an era-allowed recipe outputs it
    bool has_deposit        = false;  ///< some tile in the generated world yields it
    int  depth              = -1;     ///< recipe_registry::depth_of under this band

    // Structural sinks — does ANY pass in this band name it as a want?
    bool sink_household   = false;
    bool sink_background  = false;
    bool sink_process     = false;    ///< input to an era-allowed recipe
    bool sink_construct   = false;    ///< line in an era-available building's basket
    // BL-654 changed what these two ARE. Both upkeep draws used to be pool-only
    // — they consumed a good without ever pricing it — so neither counted as a
    // market sink and both printed in lower case to say so. They now take the
    // ONE goods-draw path: pool first, then a BID onto the local market for the
    // shortfall, up to the buyer's reservation ceiling. A short pool is a real
    // participant in the price, so these are market sinks like any other.
    bool sink_unit_upkeep = false;    ///< BL-454 unit upkeep — pool draw, then a market bid
    bool sink_industry    = false;    ///< BL-641 building upkeep — pool draw, then a market bid
    bool sink_endemic     = false;    ///< BL-647 endemic luxury basket — a market bid, wealth-scaled
    /// BL-644 space programme — a PAID pool purchase that never bids a market.
    /// Lower-case in sink_word by the pre-BL-654 convention: a real consumer
    /// with real money, invisible to the price signal by its own design
    /// ("never on the open market"). Deliberately NOT in any_market_sink().
    bool sink_state       = false;
    /// BL-643 network upkeep — the same paid-pool-purchase shape, pro-rata.
    /// Same lower-case convention, same exclusion from any_market_sink().
    bool sink_infra       = false;

    /// Does ANY market carry a base price for it? Both basket injectors skip a
    /// resource whose `base_price` is 0 ("untradeable -- no base price to anchor
    /// the elasticity curve"), so an unpriced basket entry is a want the engine
    /// silently discards. That is a channel going quiet without saying so, which
    /// is the one thing this census exists to make impossible.
    bool priced = false;

    /// BL-654: the two upkeep draws JOINED this set. Before it they were pool
    /// draws that priced nothing, so counting them here would have called a good
    /// "bought" that no market ever heard of. They now bid, so a resource whose
    /// only consumer is unit or building upkeep genuinely does have a market
    /// sink — and `ordnance`, drawn per head by every standing unit, leaves the
    /// "produced in-band, NO market sink" list because of it.
    bool any_market_sink() const
    {
        return sink_household || sink_background || sink_process || sink_construct
            || sink_unit_upkeep || sink_industry || sink_endemic;
    }
};

struct market_completeness
{
    entity_id market = null_entity;
    entity_id body   = null_entity;
    int       catchment_tiles = 0;   ///< tiles clearing against this market
    int       in_reach_tiles  = 0;   ///< of those, tiles a building could legally take
    long long heads           = 0;   ///< population scale in the catchment (context, not score)
    int       raws_in_reach   = 0;   ///< distinct resources with a deposit on a qualifying tile
    int       terminals_closed = 0;
    int       terminals_total  = 0;
    double    completeness     = 0.0;
};

/// Every resource's structural classification under the registry's current era
/// band — what makes it, what eats it, and how deep it sits.
std::array<resource_classification, resource_count>
classify_resources(const world& w, const recipe_registry& reg);

/// MARKETS.md property 4's terminal set, read off the classification. Processing
/// is excluded by construction — it is a pass-through, and a chain that ends in
/// a processor ends nowhere.
std::vector<std::size_t>
terminal_resources(const std::array<resource_classification, resource_count>& cls);

/// Is @p tile close enough to its market that a building there could legally
/// take it? "Not computed" is permissive, as placement has it.
bool tile_in_reach(const world& w, entity_id tile, float max_reach);

/// Market ids in ascending order. The walk order every deterministic pass over
/// markets needs.
std::vector<entity_id> sorted_market_ids(const world& w);

/// SATURATION, per market: of the chains terminating here, how many can be
/// sourced within reach. `terminals_closed / terminals_total` is the fraction
/// phase 6 searches on, and it is a static property of the world — it steps no
/// ticks and needs no prices.
std::vector<market_completeness>
measure_market_completeness(world& w, const recipe_registry& reg,
                            const std::array<resource_classification, resource_count>& cls);

// ===========================================================================
// The static supply:demand ratio — is the market IN BAND?
// ===========================================================================
//
// THE SECOND HALF OF SATURATION, promoted for the same reason as the first
// (BL-979). Chain completeness says a market CAN close its chains; this says
// whether what the ground offers is in proportion to what the band's sinks
// want. `landscape_score` computed it as its term 2 and nothing else could
// read it, so no instrument could answer "what fraction of markets clear at
// start". The arithmetic moved here VERBATIM — the scorer now calls this and
// the census prints it — so the search and the reading cannot drift.
//
// SUPPLY is deposit magnitude summed over the market's IN-REACH catchment.
// DEMAND is the structural want: heads for a household sink, a flat weight per
// other market sink. Both static; neither reads a price. A priced resource with
// any signal on either side is RATED; it is BALANCED when supply/demand sits
// inside [1/pin_ratio, pin_ratio], GLUTTED above (or supplied with no sink
// here), STARVED below (or wanted with nothing here yielding it).
//
// THE DEFAULTS LIVE HERE, ONCE. `landscape_score_params` initialises from these
// constants rather than restating them, so the search and the census read the
// same band unless a caller says otherwise. The band is not a verdict: 4.0 sits
// deliberately inside the authored price band of [0.25x, 10x], and where the
// line belongs is Ben's to set — this measure REPORTS, it does not gate.

constexpr double k_balance_household_per_head = 1.0;
constexpr double k_balance_sink_weight        = 250.0;
constexpr double k_balance_pin_ratio          = 4.0;

struct market_balance
{
    entity_id market   = null_entity;
    entity_id body     = null_entity;
    int       balanced = 0;    ///< priced resources whose ratio sits inside the band
    int       glutted  = 0;    ///< supply >> demand, or supply with no sink here
    int       starved  = 0;    ///< demand >> supply, or a want nothing here yields
    int       rated    = 0;    ///< priced resources with any signal at all
    double    fraction = 0.0;  ///< balanced / rated; 0 when nothing is rated
};

/// The balance term, per market, under an explicit band and demand weights.
/// Builds the body reach fields it needs (a `world` cache) — which is why @p w
/// is not const, and why a bare call is safe: "not computed" reach is
/// PERMISSIVE, so a caller that skipped the build would count every tile in
/// reach. Deterministic: sorted market walk, sorted tile walk.
std::vector<market_balance>
measure_market_balance(world& w, const recipe_registry& reg,
                       const std::array<resource_classification, resource_count>& cls,
                       double household_per_head, double sink_weight, double pin_ratio);

/// THE HEADLINE READING: per market, the fraction of priced resources whose
/// supply:demand ratio sits within [1/ratio, ratio], at the default demand
/// weights. A convenience over `measure_market_balance` for an instrument that
/// wants the number and not its decomposition.
std::vector<market_balance>
fraction_in_band(world& w, const recipe_registry& reg, double ratio);

// ===========================================================================
// The other half of the question — does each part PAY?
// ===========================================================================
//
// Saturation says a market CAN close its chains. It does not say anyone can
// afford to. Ben's phase 6 asks both at once ("so that each part makes some
// profit"), so the margin computation is promoted alongside the completeness
// measure and for the same reason: it was written inside the anonymous
// namespace of tools/verify/recipe_margin.cpp, where generation could not reach
// it.
//
// PURE, AND IT NEEDS NO WORLD. `evaluate_margin` takes plain doubles — a row's
// revenue, its inputs at base, its wage per batch — so it can price a candidate
// firm without a market, a tick or a price history. That is what makes the
// static search possible: the two halves of the question are both answerable
// from tables plus a generated world, with no clock.

struct margin_eval
{
    double revenue   = 0.0; ///< per batch / per unit, at base
    double inputs    = 0.0; ///< per batch, at base
    double wage_pb   = 0.0; ///< wage per batch / per unit
    double mc        = 0.0; ///< marginal cost = inputs + wage_pb
    double margin    = 0.0; ///< revenue - mc
    double ratio     = 0.0; ///< margin / mc (revenue/mc - 1); +inf when mc == 0
    bool   m1        = false;
    double fixed     = 0.0; ///< maintenance + goods upkeep at base, per tick
    double wages_pt  = 0.0; ///< wages per tick at W
    double base_net  = 0.0; ///< per tick at base (information)
    double floor_net = 0.0; ///< per tick at the floor (M2's quantity)
    bool   m2        = false;
};

/// Price one row — a recipe in a band, or an extraction target — at BASE.
/// `m1` is the margin anchor (margin >= k x marginal cost) and `m2` the floor
/// anchor (a building at typical staffing still covers its fixed costs at the
/// price floor). Both are the authoring-time checks PRODUCTION.md § The recipe
/// margin anchor defines.
margin_eval evaluate_margin(double revenue, double inputs, double wage_pb,
                            double units_per_tick, double wages_pt, double fixed,
                            double floor_mult, double k);
