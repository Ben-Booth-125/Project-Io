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
// SPARE OUTPUT is read over a SET, never per producer market (BL-1233 review): for
// a consumer at market C, the REACH SET is the producer markets within reach of C
// (each landing under the ceiling, below); its spare is their summed output of r
// less the NOMINAL draw of r of every standing consumer (a live, laboured
// processor whose recipe takes r) in a market ANY of them is within reach of —
// each consumer market's draw counted ONCE. Counting a consumer the set can feed
// against the whole set is conservative (it may be fed from outside the set too);
// counting it once per producer that reaches it was not conservative but wrong:
// two producers of 1.0 each reaching one consumer of 1.5 read -0.5 apiece and
// summed to nothing, where 0.5 is spare.
//
// OBTAINABLE: an input r of a processor at market C, needing `need` units a tick,
// is obtainable when
//   (1) STOCK: the corp's own (corp, C) pool plus C's shelf — the shelf only where
//       the fair-price ceiling admits it (`shelf_admits`, the production tick's own
//       test) — covers `need` at the idle threshold `t_idle`; or
//   (2) SUPPLY: the spare output of r over C's reach set (producer markets within
//       reach of C, each counted only if its unit LANDS at C at a price the
//       ceiling admits: producer market's posted price + haul <= reservation_mult
//       x base at C) covers `need` at `t_idle`. The asking building's own output
//       and its own standing draw are taken out first, each once.
// Its OBTAINABLE COST per unit is C's posted price when (1) holds; otherwise the
// cheapest landed cost over the set's producer markets with output (other than
// the asker's own).
//
// Deterministic: producers and draws are gathered into per-resource SORTED
// vectors, every scan is ascending by id with strict comparisons, and every memo
// is an ordered map. The leg calls warm the logistics path caches only.

#include "entity.hpp"
#include "components.hpp"    // resource_count
#include "supply_system.hpp" // logistics_nodes, price_market_export_leg

#include <array>
#include <map>
#include <unordered_map>
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
    /// One producer market of a reach set: its summed output and landed cost.
    struct supply   { entity_id market; float out; float landed; };
    /// A consumer market's REACH SET for one good (see SPARE OUTPUT).
    struct reach_set
    {
        std::vector<supply>    markets;      ///< producer markets, ascending id
        std::vector<entity_id> draw_markets; ///< consumer markets any of them reaches, ascending
        float                  out   = 0.0f; ///< summed output of `markets`
        float                  drawn = 0.0f; ///< summed draw of `draw_markets`, each once
    };

    bool index_built = false;
    /// Per resource: every producer, sorted by (market, building).
    std::array<std::vector<producer>, resource_count>                      producers;
    /// Per resource: (market, summed nominal draw of standing consumers), sorted.
    std::array<std::vector<std::pair<entity_id, float>>, resource_count>   draws;
    /// (consumer market, r) -> its reach set.
    std::map<std::pair<entity_id, std::size_t>, reach_set>                 supply_memo;

    // ---- refresh mode (`input_reach_refresh`, generation's placement) ----
    /// What a building was indexed as: the fields its output and draw read.
    struct building_sig
    {
        building_type type;
        std::uint16_t recipe;
        entity_id     tile;
        resource_type target;
        float         assigned, supply;
        int           wt, ticks;
        bool          decom;
        bool operator==(const building_sig& o) const
        {
            return type == o.type && recipe == o.recipe && tile == o.tile && target == o.target
                && assigned == o.assigned && supply == o.supply && wt == o.wt
                && ticks == o.ticks && decom == o.decom;
        }
    };
    struct indexed_building
    {
        building_sig sig;
        entity_id    market = null_entity;
        std::vector<std::pair<std::size_t, float>> out;  ///< (r, output)
        std::vector<std::pair<std::size_t, float>> draw; ///< (r, draw)
    };
    bool refresh_mode = false;
    /// Every processor / extraction site the refresh index holds, by id.
    std::unordered_map<entity_id, indexed_building>                        seen;
    /// Per resource: market -> building -> draw, so a market's sum is re-added in
    /// ascending building id exactly as a full build adds it.
    std::array<std::map<entity_id, std::map<entity_id, float>>, resource_count> draw_parts;
};

/// Read the node set and the dispatch margin. The producer index is built lazily.
input_reach make_input_reach(const world& w, const recipe_registry& reg);

/// Forget the producer/draw index and the spare memos (the haul memo stays):
/// call when buildings have changed since the context was first asked.
void input_reach_invalidate(input_reach& ir);

/// Bring the producer/draw index up to the buildings standing NOW, touching only
/// the processors and extraction sites added, removed or changed (type, recipe,
/// tile, target, labour, supply, construction, decommission) since the last
/// refresh — the index it leaves is the one a full build would make, entry for
/// entry and sum for sum (each market's draw re-added in ascending building
/// id). The reach-set memo is dropped only when something changed. For a pass
/// that decides buildings one by one against what stands (BL-1233, generation's
/// sized placement), where a rebuild per decision cost the landscape search ~5x.
/// Reads deposits and modifiers as a full build does, but does not watch them:
/// a pass that changes a reserve or a modifier calls `input_reach_invalidate`.
void input_reach_refresh(const world& w, const recipe_registry& reg, input_reach& ir);

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

/// The SUPPLY clause of `input_obtainable`, alone: the reachable spare of @p r
/// at @p consumer_market (`reachable_supply`, @p self taken out) covers @p need
/// at `t_idle`, with some producer landing a unit there. Writes the cheapest
/// landed unit cost when given. A non-positive @p need is covered.
bool input_supply_covers(world& w, const recipe_registry& reg, input_reach& ir,
                         entity_id consumer_market, std::size_t r, float need, entity_id self,
                         float* out_landed = nullptr);

/// Every input of recipe @p rc by the SUPPLY clause alone (no stock): the test
/// generation's placement asks (BL-1233, a processor needs SPARE reachable
/// supply) — a new plant's draw is judged against the standing producers and
/// consumers, never against an opening shelf it would eat through.
bool recipe_inputs_supplied(world& w, const recipe_registry& reg, input_reach& ir,
                            entity_id consumer_market, const recipe& rc, float batches,
                            entity_id self);

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
