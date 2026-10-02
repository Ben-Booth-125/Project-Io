#pragma once

// BL-1125 (markets can die) — the generation-time folds that remove a market
// from the home body (MARKETS.md § Market centres and seeding). A folded
// market's catchment, inventory and pools pass to the market that absorbs it;
// nothing is lost. GENERATION ONLY: in play a market never disappears.
//
// Pure functions of the world they are handed, deterministic: every walk is in
// ascending market id order (or an explicitly tie-broken order derived from
// it), never in hash-map order.

#include "world.hpp"

#include <cstdint>
#include <vector>

/// One fold, as it happened: @p folded is gone, @p into absorbed it.
struct market_fold_record
{
    entity_id folded = null_entity;
    entity_id into   = null_entity;
};

/// What a fold pass moved, summed over every fold it made, read before and
/// after so a reader can check conservation: the goods held in the folded
/// markets' inventories and in every corporation's pools keyed by them arrive
/// in the absorber's. Totals in double over a fixed (ascending key) order.
struct market_fold_tally
{
    int    folds              = 0;
    double inventory_before   = 0.0; ///< Body-wide market inventory before the pass.
    double inventory_after    = 0.0; ///< ...and after; equal to within float rounding.
    double pools_before       = 0.0; ///< Body-wide corp pool goods before the pass.
    double pools_after        = 0.0;
    int64_t catchment_pop_before = 0; ///< Population (thousands) routed to some market.
    int64_t catchment_pop_after  = 0;
    std::vector<market_fold_record> records; ///< In fold order.
};

/// Fold market @p folded into market @p into: its inventory adds to the
/// absorber's, every corporation's pool keyed by it adds to that corporation's
/// pool keyed by the absorber, and the folded market is erased. Its catchment
/// passes by construction — `market_for_tile` routes every tile it held to the
/// nearest surviving centre. No-op if either id is not a market or they are
/// equal. The order book names a body, never a market, so nothing there moves.
void fold_market_into(world& w, entity_id folded, entity_id into);

/// TWINS FOLD. Every market on @p body centred on the same tile as a lower-id
/// market folds into the lowest-id market on that tile (a twin could never win
/// a tile anyway: catchment ties go to the lowest id). Unanchored markets are
/// never twins. Returns what moved.
market_fold_tally fold_twin_markets(world& w, entity_id body);

/// GRAVITY FOLD. A market inside a LARGER market's reach folds into it.
/// "Larger" is catchment population (`market_for_tile` over the body's
/// population centres), ties to the lower id; "reach" is the traversal cost of
/// travel from the smaller market's centre to the larger's, the convoy's own
/// edge price (landform x road x river, LOGISTICS.md § 1). Markets are walked
/// largest first; each surviving market absorbs every smaller surviving market
/// whose centre lies within @p reach of it. Catchment populations are read
/// once, before any fold. @p reach <= 0 folds nothing. Returns what moved.
market_fold_tally fold_markets_by_gravity(world& w, entity_id body, float reach);
