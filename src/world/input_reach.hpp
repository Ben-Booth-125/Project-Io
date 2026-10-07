#pragma once

// ---------------------------------------------------------------------------
// input_reach — WITHIN REACH and OBTAINABLE (BL-1187, build only what runs; the
// reach is BL-1185's, chain-feasible placement)
// ---------------------------------------------------------------------------
// A neutral home for "can a processor at market C get good r?". The play-time
// scorer (corp_ai.cpp: build, recipe switch, resume) asks it here. It is written
// to be the one definition generation's placement (corporation_generation.cpp,
// BL-1185) calls too, so the two cannot disagree: placement asks
// `building_produces` and `market_within_reach` with no report (the generation
// form), over the buildings standing when each processor is decided.
//
// WITHIN REACH (the BL-1186 diagnosis, sprint-49-shipment-diagnosis.md § For
// BL-1185): a producer in market P is within reach of a consumer in market C when
//   (a) P == C, or
//   (b) the dispatcher's own market leg `price_market_export_leg(P, C)` is viable
//       AND the dispatcher's own export gate passes, in one of two forms:
//         * PLAY (a report with rows): price_C - haul > (1 + dispatch_margin) x
//           price_P on each market's current resolved price
//           (`dispatch_market_price`, literally what the dispatcher tests);
//         * GENERATION / hand-built (no report): reservation_mult x base_C -
//           haul > (1 + dispatch_margin) x base_P, each market's own base — the
//           dispatcher ships because the destination is short, and a short
//           market prices up to the ceiling (ceil_mult when the ceiling is OFF).
//           Equal bases and a cheap haul are therefore in reach.
// Both halves are the dispatcher's, CALLED or restated term for term, so a lane
// called in reach is one a convoy would actually run. (The earlier bound —
// haul <= (reservation_mult - 1 - dispatch_margin) x base — was looser than the
// gate: it admitted lanes that never ship.) The fair-price ceiling is NOT part of
// reach; it is the separate landed test below, so reach survives the ceiling
// being OFF. A grid good (BL-708) is never cargo, so for one only (a) holds.
//
// A PRODUCER of r and its OUTPUT of r:
//   * a processing facility whose recipe outputs r, or an extraction site that
//     yields r — its primary, or any other extractable deposit on its tile with
//     reserve left, in the richness share run_extraction gives it (BL-437). A
//     site whose PRIMARY reserve is spent yields nothing at all, co-extracts
//     included (run_extraction returns before the basket).
//   * never one that is decommissioned, under construction, or unlaboured
//     (no workforce assigned, or a zero workforce target).
//   * IN PLAY, given this tick's economy_report with rows (`input_reach::report`),
//     the output is what the building ACTUALLY produced this tick — a starved
//     mill or an idle mine supplies nothing, and a plant that switched recipe
//     since its row supplies nothing of its new good yet. Without a report
//     (generation, a hand-built world) the output is NOMINAL: rate x labour.
//
// SPARE OUTPUT: a producer market P's output of r, less the NOMINAL draw of r of
// every standing consumer (a live, laboured processor whose recipe takes r) in a
// market P is within reach of. Counting a consumer against every producer that
// can feed it is deliberately conservative: one producer in reach does not admit
// every consumer in reach of it.
//
// OBTAINABLE: an input r of a processor at market C, needing `need` units a tick,
// is obtainable when
//   (1) STOCK: the corp's own (corp, C) pool plus C's shelf — the shelf only where
//       the fair-price ceiling admits it (`shelf_admits`, the production tick's own
//       test) — covers `need` at the idle threshold `t_idle`; or
//   (2) SUPPLY: the spare output of r over the producer markets within reach of
//       C, each counted only if its unit LANDS at C at a price the ceiling admits
//       (producer market's posted price + haul <= reservation_mult x base at C),
//       covers `need` at `t_idle`. The asking building's own output and its own
//       standing draw are taken out of the sum first.
// Its OBTAINABLE COST per unit is C's posted price when (1) holds; otherwise the
// cheapest landed cost over the producer markets with spare.
//
// Deterministic: producers and draws are gathered into per-resource SORTED
// vectors, every scan is ascending by id with strict comparisons, and every memo
// is an ordered map. The leg calls warm the logistics path caches only.

#include "entity.hpp"
#include "components.hpp"    // resource_count
#include "supply_system.hpp" // logistics_nodes, price_market_export_leg

#include <array>
#include <map>
#include <utility>
#include <vector>

struct world;
struct recipe;
struct economy_report;
class recipe_registry;

/// The reach context one pass shares. The haul memo is valid for as long as the
/// markets and the logistics nodes stand; the producer/draw index and the spare
/// memos describe the buildings when they were first asked for, so a caller
/// whose world changes between questions calls `input_reach_invalidate`.
struct input_reach
{
    logistics_nodes                                  nodes;
    /// Per-unit haul of a market pair (src, dst); < 0 = no viable leg.
    std::map<std::pair<entity_id, entity_id>, float> haul;
    /// The dispatcher's export margin (`logistics.dispatch_margin`).
    float                                            dispatch_margin = 0.0f;
    /// Fair-price ceiling multiple (`price_band().reservation_mult`).
    float                                            reservation_mult = 0.0f;
    /// Generation-side destination multiple: reservation_mult, or the price
    /// band's ceil_mult when the ceiling is OFF.
    float                                            gen_price_mult = 0.0f;

    /// Optional, play only: this tick's economy report. When it carries rows,
    /// outputs are this tick's ACTUAL production. Null or empty: nominal.
    const economy_report*                            report = nullptr;

    struct producer { entity_id market; entity_id building; float out; };
    struct supply   { entity_id market; float spare; float landed; };

    bool index_built = false;
    /// Per resource: every producer, sorted by (market, building).
    std::array<std::vector<producer>, resource_count>                      producers;
    /// Per resource: (market, summed nominal draw of standing consumers), sorted.
    std::array<std::vector<std::pair<entity_id, float>>, resource_count>   draws;
    /// (producer market, r) -> spare output of r there.
    std::map<std::pair<entity_id, std::size_t>, float>                     spare_memo;
    /// (consumer market, r) -> the producer markets that can supply it.
    std::map<std::pair<entity_id, std::size_t>, std::vector<supply>>       supply_memo;
};

/// Read the node set and the dispatch margin. The producer index is built lazily.
input_reach make_input_reach(const world& w, const recipe_registry& reg);

/// Forget the producer/draw index and the spare memos (the haul memo stays):
/// call when buildings have changed since the context was first asked.
void input_reach_invalidate(input_reach& ir);

/// Output of resource @p r by building @p b per tick (see PRODUCER): actual when
/// @p report carries rows, nominal otherwise; 0 for a non-producer.
float building_output(const world& w, const recipe_registry& reg, entity_id bid,
                      const building_component& b, std::size_t r,
                      const economy_report* report);

/// True when @p b yields resource @p r at its nominal rate (the generation
/// reading: no report).
bool building_produces(const world& w, const recipe_registry& reg,
                       const building_component& b, std::size_t r);

/// Nominal per-tick draw of resource @p r by a standing consumer @p b; 0 when it
/// is decommissioned, under construction, unlaboured, or does not take @p r.
float building_draw(const recipe_registry& reg, const building_component& b, std::size_t r);

/// Per-unit haul from market @p src to market @p dst (memoised); 0 when they
/// are the same market; < 0 when no viable leg exists.
float input_reach_haul(world& w, const recipe_registry& reg, input_reach& ir,
                       entity_id src_market, entity_id dst_market);

/// Whether a producer standing in @p src_market is WITHIN REACH of a consumer
/// in @p dst_market for good @p r. Writes the per-unit haul when it is.
bool market_within_reach(world& w, const recipe_registry& reg, input_reach& ir,
                         entity_id src_market, entity_id dst_market, std::size_t r,
                         float* out_haul = nullptr);

/// Reachable spare supply of @p r at @p consumer_market (see SUPPLY), with the
/// asking building @p self's own output and draw taken out (null_entity: none).
struct reachable_spare
{
    float spare  = 0.0f;  ///< summed spare over producer markets within reach
    float landed = -1.0f; ///< cheapest landed unit over those with spare; < 0 none
};
reachable_spare reachable_supply(world& w, const recipe_registry& reg, input_reach& ir,
                                 entity_id consumer_market, std::size_t r, entity_id self);

/// One input's answer at a consumer market.
struct input_access
{
    bool  obtainable = false;
    float unit_cost  = 0.0f; ///< what a unit would cost the processor (see OBTAINABLE)
};

/// True when building @p bid MAKES resource @p r by what it is — a processor
/// whose recipe outputs r, or an extraction site targeting r or co-extracting it
/// from its tile — whether or not it is running. The STOCK clause of
/// `input_obtainable` does not count the asker's own good as its own cover
/// (BL-1206): its pool and shelf hold what it made, which stops arriving when it
/// switches. The SUPPLY clause then decides, with the asker's output taken out.
bool building_makes(const world& w, const recipe_registry& reg, entity_id bid, std::size_t r);

/// The batches a processor decision judges a building's inputs at (BL-1206):
/// `base_rate x labour` — labour as this file's producer test reads it,
/// assigned x the workforce target scalar, which is what run_processing draws
/// at. An UNSTAFFED building (labour 0) is judged at the staffing a placed
/// building is authored with (half assigned, target 100 %), never at zero — a
/// zero need passes every input and would admit any recipe.
float judged_batches(const recipe_registry& reg, const building_component& b);

/// Is input @p r obtainable at @p consumer_market for a run needing @p need units?
/// @p pool is the corp's (corp, market) pool, may be null. @p allow_supply false
/// asks the STOCK clause alone.
input_access input_obtainable(world& w, const recipe_registry& reg, input_reach& ir,
                              entity_id consumer_market, const stockpile_component* pool,
                              std::size_t r, float need, entity_id self,
                              bool allow_supply = true);

/// Every input of recipe @p rc at once: true when each is obtainable; fills
/// @p unit_cost[r] for each input with its obtainable cost (the posted price for
/// one that is not). @p batches sizes the run.
bool recipe_inputs_obtainable(world& w, const recipe_registry& reg, input_reach& ir,
                              entity_id consumer_market, const stockpile_component* pool,
                              const recipe& rc, float batches, entity_id self,
                              std::array<float, resource_count>& unit_cost,
                              bool allow_supply = true);
