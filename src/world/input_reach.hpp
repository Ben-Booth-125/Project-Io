#pragma once

// ---------------------------------------------------------------------------
// input_reach — WITHIN REACH and OBTAINABLE, the one definition (BL-1187, build
// only what runs; the reach is BL-1185's, chain-feasible placement)
// ---------------------------------------------------------------------------
// A neutral home for "can a processor at market M get good r?", so that the
// play-time scorer (corp_ai.cpp: build, recipe switch, resume) and generation's
// placement (corporation_generation.cpp, BL-1185) ask ONE question and cannot
// disagree.
//
// WITHIN REACH (the BL-1186 diagnosis, sprint-49-shipment-diagnosis.md § For
// BL-1185): a producer in market P is within reach of a consumer in market C when
//   (a) P == C, or
//   (b) the dispatcher's own market leg `price_market_export_leg(P, C)` is viable
//       AND its per-unit haul is at most `(reservation_mult - 1 - dispatch_margin)
//       x base(r)` at C — above that a landed cargo can never be bought at a price
//       the fair-price ceiling admits.
// The leg is CALLED, never restated: whatever widens the dispatcher's routing
// widens this reach with it. A grid good (BL-708) is never cargo, so for one only
// (a) holds.
//
// A PRODUCER of r: a live (not decommissioned) processing facility whose recipe
// outputs r, or a live extraction site that yields r — its target or any other
// extractable deposit on its tile with reserve left, because a site works the
// whole tile (BL-437, run_extraction). Under construction counts: it is a producer
// the moment it completes, and BL-1185 counts what stands. IN PLAY, given this
// tick's economy_report (`input_reach::report`), a PROCESSOR counts only if it
// produced this tick: a starved mill supplies nothing.
//
// OBTAINABLE (BL-1187): an input r of a processor at market C is obtainable when
//   (1) STOCK: the corp's own (corp, C) pool plus C's shelf — the shelf only where
//       the fair-price ceiling admits it (`shelf_admits`, the production tick's own
//       test) — covers the run at the idle threshold `t_idle`; or
//   (2) SUPPLY: a producer of r stands within reach of C, and its unit LANDS at
//       C at a price the fair-price ceiling admits (producer market's posted
//       price + haul <= reservation_mult x base at C). At base prices that is
//       the reach bound itself; it bites where the producer's market is priced
//       over the ceiling — in C itself, exactly when C's shelf is closed.
// Its OBTAINABLE COST per unit is what the processor would pay: C's posted price
// when (1) holds; otherwise the cheapest landed cost over the producers within
// reach — C's price for a producer in C, the producer market's price plus the
// per-unit haul for one elsewhere.
//
// Deterministic: producers are gathered into per-resource SORTED vectors, every
// scan is ascending by id with strict comparisons, and the haul memo is an
// ordered map. The leg calls warm the logistics path caches only.

#include "entity.hpp"
#include "components.hpp"   // resource_count
#include "supply_system.hpp" // logistics_nodes, price_market_export_leg

#include <array>
#include <map>
#include <utility>
#include <vector>

struct world;
struct recipe;
struct economy_report;
class recipe_registry;

/// The reach context one pass shares: the node set the legs price against, a
/// memo of per-unit hauls, and (built on first use) the producer index. Valid
/// for as long as the world's buildings and markets are not moved under it; a
/// caller that builds one per tick or per placement pass is safe.
struct input_reach
{
    logistics_nodes                                  nodes;
    /// Per-unit haul of a market pair (src, dst); < 0 = no viable leg.
    std::map<std::pair<entity_id, entity_id>, float> haul;
    /// `reservation_mult - 1 - dispatch_margin`: the share of base a haul may eat.
    float                                            headroom = 0.0f;
    /// Fair-price ceiling multiple (`price_band().reservation_mult`).
    float                                            reservation_mult = 0.0f;

    /// Optional, play only: this tick's economy report. When set and non-empty,
    /// a PROCESSOR counts as a producer only if its row produced this tick (a
    /// starved mill supplies nothing). Null at generation (BL-1185's reading).
    const economy_report*                            report = nullptr;

    /// Per resource: (market, building) of every producer, sorted ascending.
    std::array<std::vector<std::pair<entity_id, entity_id>>, resource_count> producers;
    bool                                             producers_built = false;
};

/// Read the node set and the reach headroom. The producer index is built lazily.
input_reach make_input_reach(const world& w, const recipe_registry& reg);

/// True when @p b yields resource @p r (see the header note: PRODUCER).
bool building_produces(const world& w, const recipe_registry& reg,
                       const building_component& b, std::size_t r);

/// Per-unit haul from market @p src to market @p dst (memoised); 0 when they
/// are the same market; < 0 when no viable leg exists.
float input_reach_haul(world& w, const recipe_registry& reg, input_reach& ir,
                       entity_id src_market, entity_id dst_market);

/// Whether a producer standing in @p src_market is WITHIN REACH of a consumer
/// in @p dst_market for good @p r. Writes the per-unit haul when it is.
bool market_within_reach(world& w, const recipe_registry& reg, input_reach& ir,
                         entity_id src_market, entity_id dst_market, std::size_t r,
                         float* out_haul = nullptr);

/// The cheapest landed per-unit cost of @p r at @p consumer_market over every
/// producer within reach, ignoring building @p self (a building never feeds
/// itself). < 0 when no producer is within reach.
float reachable_supply_cost(world& w, const recipe_registry& reg, input_reach& ir,
                            entity_id consumer_market, std::size_t r, entity_id self);

/// One input's answer at a consumer market.
struct input_access
{
    bool  obtainable = false;
    float unit_cost  = 0.0f; ///< what a unit would cost the processor (see OBTAINABLE)
};

/// Is input @p r obtainable at @p consumer_market for a run needing @p need units?
/// @p pool is the corp's (corp, market) pool, may be null.
/// @p allow_supply false asks the STOCK clause alone (see the corp_ai resume:
/// a plant that runs next tick needs the input at hand, not a producer in reach).
input_access input_obtainable(world& w, const recipe_registry& reg, input_reach& ir,
                              entity_id consumer_market, const stockpile_component* pool,
                              std::size_t r, float need, entity_id self,
                              bool allow_supply = true);

/// Every input of recipe @p rc at once: true when each is obtainable; fills
/// @p unit_cost[r] for each input with its obtainable cost (the posted price for
/// one that is not). @p batches sizes the run the stock clause must cover.
bool recipe_inputs_obtainable(world& w, const recipe_registry& reg, input_reach& ir,
                              entity_id consumer_market, const stockpile_component* pool,
                              const recipe& rc, float batches, entity_id self,
                              std::array<float, resource_count>& unit_cost,
                              bool allow_supply = true);
