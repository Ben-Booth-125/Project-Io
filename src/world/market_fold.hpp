#pragma once

// BL-1125 (markets can die) — the generation-time folds that remove a market
// from the home body (MARKETS.md § Market centres and seeding). A folded
// market's catchment, inventory and pools pass to the market that absorbs it;
// nothing is lost. GENERATION ONLY: in play a market never disappears.
//
// THE CATCHMENT PASSES WHOLE. A folded market leaves a routing record in
// `world::folded_markets` (its centre and its absorber), and `market_for_tile`
// routes a tile to its nearest ORIGINAL centre, folded ones included, then
// follows the record. So every tile a folded market held goes to the market
// that absorbed it — never to some third market that happens to be nearer.
//
// Pure functions of the world they are handed, deterministic: every walk is in
// ascending market id order (or an explicitly tie-broken order derived from
// it), never in hash-map order, and a pass's folds are applied in ascending
// (absorber, folded) order.

#include "world.hpp"

#include <cstdint>
#include <set>
#include <vector>

/// One fold, as it happened: @p folded is gone, @p into absorbed it.
struct market_fold_record
{
    entity_id folded = null_entity;
    entity_id into   = null_entity;
    bool      across_water = false; ///< Gravity only: reached only by a path that crosses water.
};

/// What a fold pass moved, read before and after so a reader can check it.
/// GOODS: every unit in the folded markets' inventories and in every
/// corporation's pool keyed by them arrives in the absorber's (double totals
/// over a fixed ascending order). CATCHMENT: every tile of the body that routed
/// to a market the pass folded routes, after the pass, to that market's
/// absorber — `catchment_misrouted` counts the tiles that do not, and must be 0.
struct market_fold_tally
{
    int     folds              = 0;
    int     folds_across_water = 0;
    double  inventory_before   = 0.0; ///< Body-wide market inventory before the pass.
    double  inventory_after    = 0.0; ///< ...and after; equal to within float rounding.
    double  pools_before       = 0.0; ///< Body-wide corp pool goods (keys on the body's markets).
    double  pools_after        = 0.0;
    int64_t catchment_tiles_moved = 0; ///< Tiles whose market folded in this pass.
    int64_t catchment_misrouted   = 0; ///< Of every tile, those not routed to their absorber.
    std::vector<market_fold_record> records; ///< In application order: (into, folded) ascending.
};

/// Fold market @p folded into market @p into: its inventory adds to the
/// absorber's, every corporation's pool keyed by it adds to that corporation's
/// pool keyed by the absorber, it is erased from `world::markets`, and a
/// routing record (`folded_market`) is written so its catchment passes to
/// @p into. Every earlier record that named @p folded as its absorber is
/// re-pointed at @p into, so a record always names a standing market. No-op if
/// either id is not a standing market or they are equal. The order book names a
/// body, never a market, so nothing there moves.
void fold_market_into(world& w, entity_id folded, entity_id into);

/// TWINS FOLD. Every market on @p body centred on the same tile as a lower-id
/// market folds into the lowest-id market on that tile. Unanchored markets are
/// never twins. Returns what moved.
market_fold_tally fold_twin_markets(world& w, entity_id body);

/// GRAVITY FOLD. A market inside a LARGER market's reach folds into it.
/// "Larger" is catchment population (`market_for_tile` over the body's
/// population centres), ties to the lower id. "Reach" is the directed cost of
/// travel from the smaller market's centre to the larger's, over the edge
/// weights `intra_body_path` prices (landform x road x river; a water hop at
/// the sea weight) — but A FOLD MUST BE ONE A CONVOY COULD MAKE: a convoy's sea
/// leg needs a port at both ends (SUPPLY.md § Infrastructure gates), so a path
/// may cross water only when both centres are in @p port_centres; otherwise the
/// reach is measured over land alone. @p port_centres null = water is never
/// gated (a READING mode, for counting what the gate refuses; generation always
/// passes the set). Markets are walked largest first; each surviving market
/// absorbs every smaller surviving market within @p reach of it. Catchment
/// populations are read once, before any fold. @p reach <= 0 folds nothing.
market_fold_tally fold_markets_by_gravity(world& w, entity_id body, float reach,
                                          const std::set<entity_id>* port_centres);
