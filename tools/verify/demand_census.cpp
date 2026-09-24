// demand_census — BL-649. Per RESOURCE and per ERA BAND: how much demand the
// world models, and WHICH PASS injects it. Requirement group `demand-census`,
// rows R1-R4, plus R5 (BL-706, chain completeness) and R6 (BL-652, unpriced basket).
//
// WHY THIS EXISTS. Sprint 21 runs several viability passes, and "did that change
// help" has no answer without a before. More sharply: the 2026-08-26 session
// found BY HAND that the ancient band has only two live market-demand sinks, and
// that seven goods are produced in-band with no buyer at all. That finding took a
// person an hour. It should have been a report, and this is the report.
//
// IT REPORTS. IT DOES NOT ASSERT A TARGET (R2, and it is the whole discipline of
// the file). No row here fails on a magnitude. The moment a census asserts a
// number, people tune against the census instead of reading it. The only things
// that can fail are INTERNAL INCONSISTENCIES — a channel it cannot enumerate, a
// resource it cannot name, an attribution that does not reconcile — plus R4,
// which is a regression check on the INSTRUMENT (does it still reproduce the
// hand-built finding), not on the economy; plus R6, which fails on a basket
// naming a good no market prices. R6 is content, not magnitude: it asserts no
// number and admits no tuning, because that combination has exactly two causes
// and both are faults (BL-652).
//
// WHY IT LOADS LUA. The demand baskets ARE Lua data (economy.population_demand,
// economy.background_demand, economy.military.unit_upkeep, economy.building_upkeep),
// the recipe roster and
// its era tags are Lua data, and so are the per-building construction baskets.
// Restating any of it in C++ would make the census an answer about a world nobody
// plays. Build it with:
//
//     cmd //c tools\verify\build_lua_harness.bat demand_census
//
// HOW THE ATTRIBUTION IS TAKEN, and why it is exact rather than inferred.
// `clear_markets` zeroes every market's demand and then runs its injections in a
// fixed order; the price feedback each injection reads (`mc.price`) is not
// updated until `resolve_price` runs at the END of that same call. So the census
// runs the tick up to — but not into — `clear_markets`, and then performs
// `clear_markets`' OWN demand phase itself, snapshotting between the passes:
//
//     dispatch_convoys / advance_convoys / run_economy_step   (the tick, as app runs it)
//     snapshot_market_supply                                  (what clear_markets snapshots)
//     zero demand
//     inject_population_demand      -> HOUSEHOLD
//     inject_background_demand      -> BACKGROUND-INDUSTRIAL (a stopgap; see the register)
//     inject_interbody_demand       -> INTER-BODY PULL (redistribution, not a new want)
//     economy_report::wants         -> CONSTRUCTION + PROCESSING INPUTS + UPKEEP BID
//
// Those are the same functions, in the same order, reading the same prices
// `clear_markets` would read on this tick. Nothing is re-derived and nothing is
// modelled twice. The census tick is the LAST thing the harness does to a world,
// so leaving `mc.demand` holding the measured figures perturbs nothing.
//
// THE CONSTRUCTION / PROCESSING SPLIT. `economy_report::wants` merges both
// consumers into one register (that is the register's job — it is the bid to the
// market, and the market does not care who bid). The construction half is
// recovered from the world state at the top of the census tick using the
// registry's own `resource_build_cost_for` accessor and `run_construction`'s own
// per-tick expression (`cost[r] / build_duration_ticks`), and the processing half
// is the remainder. If the remainder goes negative the attribution is wrong, and
// that is an internal inconsistency — R2's kind of failure, and it is asserted.
//
// BL-654 PUT A THIRD CONSUMER IN THAT REGISTER: a short upkeep pool now bids its
// shortfall. That share is NOT re-derived — it is read off
// `economy_report::upkeep_wants`, the attribution mirror the same statement
// writes — and subtracted out, because "the remainder" would otherwise report
// the Industry channel as processing demand. The `upkeep/bd` column is that
// share; `upkeep/pl` and `indust/pl` remain the GROSS per-tick need, so the
// pool-covered part is the difference between them.
//
// STRUCTURAL SINK vs OBSERVED DEMAND. The per-resource table carries both, and
// the distinction matters. OBSERVED demand can be zero merely because nobody has
// built the consumer yet on this seed. A STRUCTURAL sink is a statement about the
// content: does any pass in this band NAME this resource at all — a household
// basket entry, a background basket entry, an input to an era-allowed recipe, a
// line in an era-available building's construction basket, a line in its BL-641
// operating-upkeep basket. A good with no
// structural sink is dead however rich the world gets, and that is the failure
// MARKETS.md § Demand channels says the census must catch.
//
// THE `px` COLUMN. Both basket injectors skip a resource whose market base_price
// is 0 ("untradeable -- no base price to anchor the elasticity curve"). A basket
// entry that is not priced is therefore a want the engine discards in silence,
// and it looks identical in the totals to a channel that was never authored. `px`
// says which it is, and the "NO market prices it" line below the table names them.
//
//   R1  Per resource, per band: total modelled demand and the passes injecting
//       it. Every one of MARKETS.md's eight channels represented or explicitly
//       reported ABSENT. REPORTED.
//   R2  No magnitude assertion anywhere. Fails only on internal inconsistency.
//   R3  Diffable: stable ordering, fixed-width formatting, no wall-clock, no
//       run-dependent noise. Run it twice and byte-compare.
//   R4  Reproduces the hand-built finding at the 0 CE band: the produced-with-
//       no-sink set. A regression check on the instrument.
//   R5  BL-706. Per MARKET: the fraction of the band's TERMINAL chains that can
//       be sourced WITHIN REACH of it, and — the actual deliverable —  the
//       SPREAD of that fraction across the world. GENERATION_STRATEGY.md
//       § Asymmetry is the deliverable. REPORTED, with one loose assertion on
//       the spread and none on any market's value. See the block above
//       `market_completeness` for the grain and the reach definition.
//   R6  BL-652: FAILS if a demand basket names a good NO market prices. The one
//       row here that fails on content rather than on the instrument, and it
//       does not breach R2 — it is not a magnitude. Both injectors skip such an
//       entry in silence, and the combination is always a missing script or an
//       authoring error, never intent. Runtime counterpart:
//       `unpriced_basket_entries` (src/world/market_clearing.cpp), which the
//       app reports at campaign start.
//
// Usage:  demand_census [--seed N] [--ticks N] [--fast] [--band ancient|industrial|both]
//   --fast  zero the pre-epoch year-tick sim. Cheaper, and NOT the shipped
//           spawn — the world it leaves has no settlement history, so every
//           magnitude in it is a different quantity wearing the same name.
//
// Exits 0 on PASS, non-zero on any failure.

#include "scripting/lua_state.hpp"

#include "harness_params.hpp"
#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/nation_step.hpp"
#include "world/market_saturation.hpp" // BL-775: the promoted saturation measure
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/network_upkeep.hpp"  // BL-643: the Infrastructure channel's derivation, re-run for the census
#include "world/space_programme.hpp" // BL-644: the State channel's derivation, re-run for the census
#include "world/supply_system.hpp"
#include "world/survey_system.hpp" // init_survey_states - the app runs it, this census did not
#include "world/tech_gate.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* row, const char* what)
{
    std::printf("  [%s] %-3s %s\n", ok ? "PASS" : "FAIL", row, what);
    if (!ok)
        ++g_failures;
}

// ---------------------------------------------------------------------------
// The resource name table — and the reason it is written out longhand
// ---------------------------------------------------------------------------
// `resource_names` maps NAME -> enum and has no inverse; a census has to print
// names. Rather than a second hand-authored table free to drift from the first
// (exactly the drift BL-414 existed to end), this list is VERIFIED against
// `resource_from_name` entry by entry, and its length against `resource_count`.
// Append a resource_type without touching this and the harness fails on R2 as an
// internal inconsistency — "a resource it cannot classify" — rather than
// printing a blank name.
const char* const k_resource_names[] = {
    "iron_ore", "coal", "petroleum", "silica", "copper_ore",
    "rare_earth_ore", "agricultural_produce", "water", "iron_nickel_ore",
    "platinum_group_metals", "regolith", "stone", "timber", "sand", "clay",
    "peat", "tobacco", "spices", "coffee", "furs",
    "steel", "refined_fuel", "food_rations", "charcoal", "iron_blooms",
    "trade_goods_misc", "propellant", "silicon", "refined_copper", "ree_alloy",
    "machinery", "alloys", "electronics", "spacecraft_components",
    "clean_water", "consumer_goods", "medical_supplies", "ordnance",
    "ceramics", "dressed_stone", "planks", "tools",
    "hides", "fibre", "leather", "cloth", "rigging",
    "power", // BL-708 — the grid good
    "construction_capacity", // BL-709 — the construction sector's product
};
constexpr std::size_t k_named = sizeof(k_resource_names) / sizeof(k_resource_names[0]);

const char* rname(std::size_t r)
{
    return (r < k_named) ? k_resource_names[r] : "<UNNAMED>";
}

/// R2's first internal-consistency gate: the print table and the canonical
/// lookup agree, entry for entry, and cover the whole enum.
bool verify_name_table()
{
    bool ok = (k_named == resource_count);
    if (!ok)
        std::printf("      name table has %zu entries, resource_count is %zu\n",
                    k_named, resource_count);
    for (std::size_t i = 0; i < k_named && i < resource_count; ++i)
    {
        bool found = false;
        const resource_type got = resource_names::resource_from_name(k_resource_names[i], found);
        if (!found || static_cast<std::size_t>(got) != i)
        {
            std::printf("      name table entry %zu (\"%s\") resolves to %s\n",
                        i, k_resource_names[i],
                        found ? "a different id" : "nothing");
            ok = false;
        }
    }
    return ok;
}

// ---------------------------------------------------------------------------
// The channel register — MARKETS.md § Demand channels, all eight
// ---------------------------------------------------------------------------
// AUTHORED HERE, deliberately, and checked for completeness rather than for
// content. The eight channels are a DESIGN register; the code cannot enumerate
// them because six of them have no code to enumerate. That is precisely the fact
// worth reporting: an ABSENT row is the most valuable row in this table, so it is
// printed rather than omitted, with what would have to exist for it to be present.
//
// `state` is one of PRESENT / ABSENT / STOPGAP, and R2 fails if the table does
// not carry exactly eight `channel_row`s or carries a state outside that set —
// "a channel it cannot enumerate".
//
// Provenance for every ABSENT verdict is a code read of src/world at 503afc4d
// (2026-08-26): the named search found no pass adding to `market_component::
// demand` or drawing from a pool for that actor. Where a channel has a partial
// or stand-in implementation it is called out on the row, never silently upgraded
// to PRESENT.
enum class ch_state { present, absent, stopgap };

struct channel_row
{
    const char* channel;
    ch_state    state;
    const char* injector;   ///< the pass, or what is missing
};

const channel_row k_channels[] = {
    { "Household",      ch_state::present,
      "inject_population_demand (market_clearing.cpp) <- economy.population_demand basket" },
    { "Industry",       ch_state::present,
      "run_building_upkeep (economy_system.cpp) <- economy.building_upkeep.goods, per building type "
      "and ERA-BANDED, scaling with BUILDING COUNT (MARKETS.md property 1). BL-654: it BIDS now -- "
      "the pool is drawn first and the shortfall goes to market at up to price_band.reservation_mult "
      "x base, so the draw prices what it wants (see the `upkeep/bd` column). BL-738's stage-1 "
      "repair rates (industrial timber/stone) were landed, MEASURED over a full campaign cell, and "
      "withdrawn the same day - cost without the income side (broke nations, BL-741/BL-742); the "
      "record is at the scripts/economy.lua goods table. Only power carries a non-zero rate" },
    { "Construction",   ch_state::present,
      "run_construction -> economy_report::wants (economy_system.cpp) <- economy.buildings.resource_costs "
      "/ material_overrides. Fires only where something is BUILDING (owner BL-642)" },
    { "Infrastructure", ch_state::present,
      "derive_network_upkeep_claims / settle_network_purchases (network_upkeep.cpp) <- "
      "economy.network_upkeep, through the logistics_maintenance budget line - pro-rata, a PAID "
      "POOL PURCHASE like the State channel, never a market bid; realised purchases readable from "
      "economy_report::network_purchases; the census's infra/pl column re-derives the bill" },
    { "State",          ch_state::present,
      "derive_space_programme_claims / settle_space_purchases (space_programme.cpp) <- "
      "economy.space_programme lumps, through the budget's claim/transfer machinery. A PAID POOL "
      "PURCHASE, never a market bid ('never on the open market' is the design's own rule) — "
      "realised purchases readable from economy_report::space_purchases; the census's state/pl "
      "column re-derives the would-be claims" },
    { "Research",       ch_state::absent,
      "research_institute credits corporation_component::science per tick; nothing draws goods "
      "(owner BL-645)" },
    { "Conflict",       ch_state::absent,
      "battle resolution consumes no goods; the only military draw is the per-head standing-force "
      "upkeep below, which is not a battle (owner BL-646)" },
    { "Endemic trade",  ch_state::present,
      "inject_endemic_demand (market_clearing.cpp) <- economy.endemic_demand basket: wealth-scaled "
      "(nation treasury + positive domiciled balances), nation-flavoured by a pure seeded "
      "preference hash, price-elastic like its sibling baskets" },
};
constexpr std::size_t k_channel_count = sizeof(k_channels) / sizeof(k_channels[0]);

/// Passes that really inject demand but are NOT one of the eight. Reported
/// beside the register so the per-resource columns below are all accounted for
/// and none of them is quietly read as one of the eight.
const channel_row k_off_register[] = {
    { "Processing inputs", ch_state::present,
      "run_processing -> economy_report::wants -- the production chain's own input bid. The largest "
      "live sink in either band, and not one of the eight (the eight are FINAL demand)" },
    { "Background-industrial", ch_state::stopgap,
      "inject_background_demand -- a world-scale basket standing in for Industry. MARKETS.md property 1 "
      "says a channel whose size is a constant is a stopgap and should be labelled one" },
    { "Inter-body pull", ch_state::present,
      "inject_interbody_demand -- REDISTRIBUTES the home body's unmet demand onto outposts. Injects no "
      "new want: with no home demand for a good it moves nothing" },
    { "Standing-force upkeep", ch_state::present,
      "run_unit_upkeep (economy.military.unit_upkeep.goods_per_head). BL-654: pool first, then the "
      "shortfall is BID on the unit's local market up to price_band.reservation_mult x base -- the "
      "same single goods-draw path the Industry channel takes, so it prices what it wants" },
};
constexpr std::size_t k_off_register_count =
    sizeof(k_off_register) / sizeof(k_off_register[0]);

const char* state_word(ch_state s)
{
    switch (s)
    {
        case ch_state::present: return "PRESENT";
        case ch_state::absent:  return "ABSENT ";
        case ch_state::stopgap: return "STOPGAP";
    }
    return "?????";
}

// ---------------------------------------------------------------------------
// THE HAND-BUILT FINDING (R4)
// ---------------------------------------------------------------------------
// The set a person found by hand on 2026-08-26 at the 0 CE band: goods an
// ancient recipe PRODUCES for which no pass in that band names them as a want.
// `ordnance` is in the set because its only draw is the per-head standing-force
// upkeep rate, which is a POOL draw and never a market bid — so on the market
// side it has no buyer at all.
//
// If the census disagrees with this list the disagreement is PRINTED, both ways,
// and R4 goes red. It is not reconciled by adjusting either side: one of them is
// wrong and which one is worth knowing.
//
// RE-BLESSED 2026-08-26 (BL-640, era-banded household basket) - three goods
// removed, NOTHING relaxed. The assertion below is unchanged in strength: the
// census set and this set must match EXACTLY, both directions, or R4 goes red.
// What moved is the world, not the bar. `ceramics`, `dressed_stone` and
// `leather` left the set because the ancient household basket now names them
// (scripts/economy.lua, economy.population_demand.baskets, era = "ancient"),
// so they have a real market bid at 0 CE for the first time.
//
// WHAT DELIBERATELY STAYED, so the reduction is a finding and not a sweep:
//   ordnance          - still a POOL draw only (unit upkeep); no market bid.
//   tools             - a construction-material draw (BL-590) owns its buyer.
//   rigging           - ship's tackle, not a household good; POPULATION.md's
//                       ancient household is ceramics, cloth, leather and
//                       dressed stone, and rigging is none of them.
//   trade_goods_misc  - endemic luxury demand (BL-647) owns its buyer.
//
// RE-BLESSED 2026-08-31 (BL-654, a short pool buys up to a reservation ceiling)
// - ONE good removed, and again nothing relaxed: the match is still exact, both
// directions. `ordnance` leaves because the sentence four lines above stopped
// being true. The per-head standing-force draw is no longer pool-only: a short
// pool bids its shortfall onto the unit's local market up to
// price_band.reservation_mult x base_price and pays for what it gets, so
// ordnance has a real market buyer at 0 CE for the first time. That was the
// item's whole purpose - a channel that consumes without pricing cannot
// bootstrap its own supply (MARKETS.md § Demand channels, property 3) - so this
// removal is the change working, not the bar moving.
//
// `tools` and `planks` DID NOT leave with it, and that is the honest reading
// rather than an oversight: their draw is BL-641 building upkeep, whose rates
// still ship at zero (scripts/economy.lua). BL-654 gave that draw a bid path;
// turning the rates on is a separate data change with its own measurement, and
// until it happens no building names tools as a want.
const char* const k_r4_expected[] = {
    "rigging", "tools", "trade_goods_misc",
};
constexpr std::size_t k_r4_expected_count =
    sizeof(k_r4_expected) / sizeof(k_r4_expected[0]);

/// BL-573: run_nation_step's template registry. Empty is correct — nothing in
/// this census opens a mercenary contract, so the walk is vacuous.

using res_row = std::array<double, resource_count>;

// sorted_market_ids() — PROMOTED to world/market_saturation.hpp (BL-775), so
// generation and this census share one implementation rather than two that can drift.

/// Summed over markets in ASCENDING ID ORDER — `w.markets` is unordered, and a
/// float accumulation over its layout is the exact seam BL-422 found a latent
/// nondeterminism in. R3 depends on this.
res_row sum_demand(const world& w, const std::vector<entity_id>& mids)
{
    res_row out{};
    for (const entity_id mid : mids)
    {
        const market_component& mc = w.markets.at(mid);
        for (std::size_t r = 0; r < resource_count; ++r)
            out[r] += static_cast<double>(mc.demand[r]);
    }
    return out;
}

void zero_demand(world& w)
{
    for (auto& [mid, mc] : w.markets)
    {
        (void)mid;
        mc.demand.fill(0.0f);
    }
}

res_row sub(const res_row& a, const res_row& b)
{
    res_row out{};
    for (std::size_t r = 0; r < resource_count; ++r)
        out[r] = a[r] - b[r];
    return out;
}

double total_of(const res_row& a)
{
    double s = 0.0;
    for (std::size_t r = 0; r < resource_count; ++r)
        s += a[r];
    return s;
}

int touched_by(const res_row& a)
{
    int n = 0;
    for (std::size_t r = 0; r < resource_count; ++r)
        if (a[r] > 0.0)
            ++n;
    return n;
}

/// One economy tick in app::step_economy's order, WITHOUT clear_markets — the
/// caller either finishes it with clear_markets (a warm-start tick) or performs
/// clear_markets' demand phase itself (the census tick).
economy_report tick_to_clearing(world& w, const recipe_registry& reg, int t, lp_pool_map& lp)
{
    w.current_econ_tick = t;
    w.current_day_tick  = t;
    lp.clear();
    advance_convoys(w);
    credit_arrived_convoys(w, t); // app order: arrivals before the economy
    economy_report rep = run_economy_step(w, reg, /*spectating=*/false, &lp);
    dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land), // BL-995: before the clear
                     reg.logistics_cost(convoy_mode::space), &lp);
    return rep;
}

/// The rest of the tick, for the warm start only.
void finish_tick(world& w, const recipe_registry& reg, int t, economy_report& rep)
{
    auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention, &rep.budgets, &rep.buildings,
                 &rep.building_labour);
    run_nation_step(w, reg, rep, t);
    advance_tech_gates(w);
}

// ---------------------------------------------------------------------------
// Structural resource_classification
// ---------------------------------------------------------------------------

// struct resource_classification — PROMOTED to world/market_saturation.hpp (BL-775), so
// generation and this census share one implementation rather than two that can drift.

std::string sink_word(const resource_classification& c)
{
    std::string s;
    auto add = [&s](const char* t) { if (!s.empty()) s += "+"; s += t; };
    if (c.sink_household)   add("HH");
    if (c.sink_background)  add("BG");
    if (c.sink_process)     add("PROC");
    if (c.sink_construct)   add("CONS");
    if (c.sink_unit_upkeep) add("UPK");   // BL-654: a market bid now, hence upper case
    if (c.sink_industry)    add("IND");   // BL-641 + BL-654, likewise
    if (c.sink_endemic)     add("END");   // BL-647: a market bid, wealth-scaled
    if (c.sink_state)       add("st");    // BL-644: pays, consumes, never bids — lower case
    if (c.sink_infra)       add("in");    // BL-643: likewise, pro-rata
    if (s.empty())
        s = "NONE";
    return s;
}

const char* prod_word(const resource_classification& c)
{
    if (c.produced_by_recipe && c.has_deposit) return "R+D";
    if (c.produced_by_recipe)                  return "REC";
    if (c.has_deposit)                         return "DEP";
    return "-- ";
}

// classify_resources() — PROMOTED to world/market_saturation.hpp (BL-775), so
// generation and this census share one implementation rather than two that can drift.

// ---------------------------------------------------------------------------
// R5 — CHAIN COMPLETENESS, AND ITS SPREAD (BL-706)
// ---------------------------------------------------------------------------
// WHY IT LIVES HERE and not in a harness of its own. The census already walks
// every market and every resource per band, and already knows which sinks are
// TERMINAL (MARKETS.md § Three properties, 4: a household basket, an upkeep
// draw, a construction cost — never a processor, which is a pass-through and a
// chain that ends in one ends nowhere). Chain completeness is that same
// resource_classification asked per MARKET rather than per world, so it costs one tile
// walk and inherits everything above it, before/after discipline included.
//
// WHAT IT MEASURES. GENERATION_STRATEGY.md § Asymmetry is the deliverable: "for
// a market or region, the fraction of the chains terminating there that can be
// sourced within reach". Generation is answerable for the DISTRIBUTION of that
// number across the world — wide, with real tails at both ends — and answerable
// for nothing at all about any individual market's value.
//
// THE GRAIN IS THE MARKET, because a market is where a chain's demand actually
// lands. `market_for_tile` already partitions every tile into exactly one market
// catchment (a body with several markets routes each tile to the nearest centre,
// components.hpp § market_component), so a catchment is a real, disjoint,
// GENERATED region rather than one invented for this measurement. Body grain
// would fold a body's several markets into one figure and lose the intra-body
// spread, which is the half a player actually stands in; province grain would
// measure a partition no chain clears against.
//
// "WITHIN REACH" IS THE GAME'S OWN RULE, NOT A SECOND METRIC. A tile counts for
// a market when (a) it clears against that market and (b) a corporation could
// legally site a building on it — `place_building_allowed`'s reach clause
// (placement_rules.cpp), mirrored exactly: a supply anchor always qualifies,
// otherwise `tile_reach_cost` must be within the AUTHORED budget
// `economy.construction.max_logistics_reach` (24.0 as shipped; < 0 disables the
// rule, and then only an unreachable tile is excluded). An infinite cost — an
// island carrying no city, a landmass cut off from the anchor network — fails
// it, which is the point: ground nobody can supply is not supply.
//
// THE NUMERATOR IS STRUCTURAL, NOT OBSERVED. "Can be sourced" asks what the
// GROUND plus the band's recipe roster permit, never what happens to stand on
// this seed. A good is sourceable in a market when a deposit for it sits on a
// qualifying tile, or when an era-allowed recipe makes it from goods that are —
// computed as a monotone fixpoint over the era-masked recipe list, so a cyclic
// roster terminates and the answer cannot depend on recipe order.
//
// THE DENOMINATOR IS THE BAND'S WHOLE TERMINAL SET, IDENTICAL FOR EVERY MARKET.
// That is MARKETS.md property 5 taken at its word: terminal demand follows
// population, population is everywhere, so every chain terminates in every
// market. Scoring against each market's OBSERVED terminal demand instead would
// let an empty market read 1.00 for wanting nothing, which measures settlement
// rather than endowment. The `heads` column carries the settlement fact
// separately, beside the score, so the two are never confused.
//
// The BACKGROUND-INDUSTRIAL basket is deliberately NOT terminal here. Property 4
// names three terminal sinks and it is not one of them, and the channel register
// above labels it a stopgap; counting a world-scale constant basket as a chain
// endpoint would put identical goods in every market's denominator for a reason
// that is not a fact about the world.
//
// IT REPORTS (R2, unchanged). No row below fails on a market being poor or rich.

/// One market's reading. Sorted by market id; every field is an integer count or
/// a ratio of two, so nothing here depends on container layout.
// struct market_completeness — PROMOTED to world/market_saturation.hpp (BL-775), so
// generation and this census share one implementation rather than two that can drift.

/// The spread — the deliverable. Percentiles by nearest rank over the sorted
/// sample, so no interpolation constant has to be defended.
struct spread_stats
{
    int    n = 0;
    double min = 0.0, p25 = 0.0, median = 0.0, p75 = 0.0, max = 0.0;
    double mean = 0.0, sd = 0.0, range = 0.0;
    int    distinct = 0;            ///< distinct scores, rounded to 1e-6
    std::array<int, 10> hist{};     ///< deciles of [0, 1]
};

// terminal_resources() and its doc comment — PROMOTED to world/market_saturation.hpp (BL-775), so
// generation and this census share one implementation rather than two that can drift.

/// `place_building_allowed`'s reach clause, restated for a read-only question.
/// Requires `body_reach_field` to have been built for the tile's body — the
/// caller does that once per body before the walk.
// tile_in_reach() — PROMOTED to world/market_saturation.hpp (BL-775), so
// generation and this census share one implementation rather than two that can drift.

// measure_market_completeness() — PROMOTED to world/market_saturation.hpp (BL-775), so
// generation and this census share one implementation rather than two that can drift.

spread_stats summarise_spread(std::vector<double> v)
{
    spread_stats st;
    st.n = static_cast<int>(v.size());
    if (st.n == 0)
        return st;

    std::sort(v.begin(), v.end());

    auto rank = [&v](double q) {
        std::size_t i = static_cast<std::size_t>(q * static_cast<double>(v.size() - 1) + 0.5);
        if (i >= v.size())
            i = v.size() - 1;
        return v[i];
    };

    st.min    = v.front();
    st.max    = v.back();
    st.range  = st.max - st.min;
    st.p25    = rank(0.25);
    st.median = rank(0.50);
    st.p75    = rank(0.75);

    double sum = 0.0;
    for (const double x : v)
        sum += x;
    st.mean = sum / static_cast<double>(v.size());
    double ss = 0.0;
    for (const double x : v)
        ss += (x - st.mean) * (x - st.mean);
    st.sd = std::sqrt(ss / static_cast<double>(v.size()));

    st.distinct = 1;
    for (std::size_t i = 1; i < v.size(); ++i)
        if (std::fabs(v[i] - v[i - 1]) > 1e-6)
            ++st.distinct;

    for (const double x : v)
    {
        int b = static_cast<int>(x * 10.0);
        if (b < 0) b = 0;
        if (b > 9) b = 9;
        ++st.hist[static_cast<std::size_t>(b)];
    }
    return st;
}

// ---------------------------------------------------------------------------
// One band
// ---------------------------------------------------------------------------

struct band_result
{
    std::string band;       ///< The band this census PINNED the registry to.
    /// BL-1101: the world's own verdict, derived at the 1960 fold. Printed
    /// beside the pinned band so a reading taken off the roster the world did
    /// not earn says so on its face — this census pins both bands over ONE
    /// generated world by design, and only one of them is the app's.
    era_band    world_band = era_band::any;

    // world shape
    int   tiles = 0, markets = 0, centres = 0, buildings = 0, under_construction = 0;
    // BL-641: how many buildings the Industry draw actually reaches. Printed
    // because the world count alone makes the `indust/pl` figure unreadable —
    // "576 buildings" and a 0.63 tools draw only reconcile once you know how
    // many of the 576 are eligible AND carry an authored basket.
    int   industry_eligible = 0, industry_drawing = 0;
    int   corps = 0, units = 0, heads = 0;
    double centre_scale = 0.0;
    int   recipes_allowed = 0, recipes_authored = 0, max_depth = 0;

    // the census tick's attribution
    res_row household{}, background{}, endemic{}, interbody{}, construction{}, processing{};
    res_row state_pl{};   ///< BL-644: the state's would-be purchases this tick (a pool draw, not market demand)
    res_row infra_pl{};   ///< BL-643: the network's would-be material bill this tick (likewise)
    /// BL-654: the upkeep channels' share of the want register — the shortfall a
    /// short pool actually BID on the market, read off `economy_report::
    /// upkeep_wants` rather than re-derived. Subtracted out of `processing`,
    /// which is a remainder and would otherwise swallow it whole.
    res_row upkeep_bid{};
    res_row wants_raw{}, wants_folded{}, upkeep_pool{}, industry_pool{};
    double  wants_dropped_no_market = 0.0;

    // observed production over the whole run
    res_row produced{};

    std::array<resource_classification, resource_count> cls{};

    bool attribution_ok = true;   ///< processing residual non-negative
    std::vector<std::string> no_sink_produced;   ///< R4's set, sorted by name
    std::vector<std::string> no_sink_raws;       ///< extractable, unwanted
    std::vector<std::string> state_only;         ///< BL-644: bought and consumed by the state alone
    std::vector<std::string> basket_unmakeable;  ///< a basket names what the band cannot produce
    std::vector<std::string> basket_unpriced;    ///< a basket names it, no market prices it -> skipped

    // BL-655 R3 — THE PRICE CONSEQUENCE. Density and prices have to be read
    // together or the count means nothing: firm density can be bought either by
    // consuming more (demand rises, coverage and price hold) or by deliberately
    // overproducing (coverage rises, price falls to the band floor). Those two
    // reach the SAME building count and mean opposite things, and the demand
    // table above cannot tell them apart. This block is the discriminator.
    //
    // Read at the census tick, after that tick's own resolve_price has run, so
    // these are the prices clear_markets last settled on. `price_ratio` is
    // price / base_price: the BL-442 band runs [floor_mult, ceil_mult] =
    // [0.25, 10.0] around 1.0, so a resource sitting at 0.25 in every market
    // that prices it is a glut, and one at 10.0 is an unservable shortage.
    res_row price_mean{};       ///< mean settled price, over markets that price it
    res_row price_ratio{};      ///< mean price / base_price
    std::array<int, resource_count> markets_pricing{};   ///< markets carrying a base price for it
    std::array<int, resource_count> markets_at_floor{};  ///< of those, how many sit AT the floor
    std::array<int, resource_count> markets_at_ceil{};   ///< of those, how many sit AT the ceiling
    float price_floor_mult = 0.0f, price_ceil_mult = 0.0f;  ///< echoed from registry, for the header

    // BL-706 R5 — chain completeness per market, and the spread of it. See the
    // block above `market_completeness` for the grain and the reach definition.
    std::vector<market_completeness> completeness;
    std::vector<std::string>         terminal_goods;
    spread_stats                     spread;
    float                            reach_budget = -1.0f;  ///< echoed from the registry

    // BL-979 — the OTHER half of saturation: per market, the fraction of priced
    // resources whose static supply:demand ratio sits inside the pin band. The
    // number landscape_score's term 2 scores on, read off the same
    // implementation (market_saturation::fraction_in_band). REPORTED, not
    // gated — the band is Ben's to set.
    std::vector<market_balance> balance;
    spread_stats                balance_spread;
    double                      pin_ratio = 0.0;   ///< the band the fraction was read at
};

band_result run_band(const char* band_name, era_band band, uint32_t seed,
                     int warm_ticks, bool prehistory, recipe_registry& reg,
                     const world_gen_config& gen_cfg)
{
    band_result out;
    out.band = band_name;

    world_params p;
    p.seed = seed;
    if (!prehistory)
        p = no_prehistory(p);   // preserves the seed

    // app::setup_world -> load_economy -> search_landscape -> apply the WINNER
    // -> assign_default_recipes, in that order (app.cpp § BL-770 PHASE 6). The
    // background economy is the search winner, not the seed candidate (BL-979;
    // apply_shipped_landscape in harness_params.hpp mirrors the app's sequence).
    world w = make_hard_coded_world(p, nullptr, gen_cfg);
    // Generation reads no epoch (BL-1047) and the band is the world's own
    // (BL-1101), so both bands census the SAME generated world. This census
    // PINS the band by name — it is an instrument over both rosters, like
    // era_roster.cpp — and records the world's own verdict beside it rather
    // than reading `band_registry_from_world`, which would collapse the two
    // runs onto one roster.
    out.world_band = w.campaign_band;
    reg.set_era(band);
    print_shipped_landscape(apply_shipped_landscape(w, reg, seed));

    // 2026-09-01, found by BL-711: the app calls init_survey_states at campaign
    // start (app.cpp) and this census never did, so EVERY body - home included -
    // stayed `hidden`. rank_extraction_sites gates on survey visibility, so it
    // returned an empty list on every tick and THE CORP AI BUILT ZERO EXTRACTION
    // SITES for the whole warm start. Not fewer: zero. Every extraction count this
    // census has ever printed was the seeder's placement, frozen.
    //
    // That made the instrument blind to exactly the behaviour sprint 27 is
    // steering by. BL-711 changed the extraction candidate list from a global
    // top-M to a per-resource top-K, which the probe measures as coal going from
    // 0 mines in any world to 25 - and this census reported the two builds
    // BYTE-IDENTICAL, because in its world neither list is ever consulted.
    //
    // Same class, same day, same one-line answer as ai_skill_harness.cpp's own
    // 2026-08-31 note: a pass the app runs that the benchmark did not, so the
    // benchmark measured a world the game never produces. It is
    // DEVELOPMENT_PRACTICES.md § A harness must build the world the application
    // builds, and it moves every reading this file produces - deliberately, once,
    // dated here rather than dribbled.
    init_survey_states(w);

    out.recipes_authored = static_cast<int>(reg.recipe_count());
    out.recipes_allowed  = reg.recipe_count(building_type::processing_facility);
    out.max_depth        = reg.max_depth();

    lp_pool_map lp;

    // --- the warm start ----------------------------------------------------
    // Observed production is accumulated over EVERY tick, not just the census
    // tick: a chain that ran once in eighty ticks is a different fact from one
    // that never ran, and a single-tick reading cannot tell them apart.
    auto accumulate_production = [&out, &reg](const economy_report& rep) {
        for (const building_report& br : rep.buildings) // a vector, fixed order
        {
            if (br.output_quantity <= 0.0f)
                continue;
            if (br.type == building_type::extraction_site)
            {
                out.produced[static_cast<std::size_t>(br.target_resource)] += br.output_quantity;
                continue;
            }
            const recipe* rc = reg.get_recipe(br.recipe);
            if (rc == nullptr)
                continue;
            double share = 0.0;
            for (std::size_t r = 0; r < resource_count; ++r)
                share += rc->outputs[r];
            if (share <= 0.0)
                continue;
            for (std::size_t r = 0; r < resource_count; ++r)
                if (rc->outputs[r] > 0.0f)
                    out.produced[r] += br.output_quantity * (rc->outputs[r] / share);
        }
    };

    for (int t = 0; t < warm_ticks; ++t)
    {
        economy_report rep = tick_to_clearing(w, reg, t, lp);
        accumulate_production(rep);
        finish_tick(w, reg, t, rep);
    }

    // --- the census tick ---------------------------------------------------
    // The construction want is taken from world state BEFORE the tick runs,
    // using run_construction's own per-tick expression and the registry's own
    // basket accessor. It is the half of `economy_report::wants` that is not a
    // processor's input bid.
    {
        std::vector<entity_id> bids;
        bids.reserve(w.buildings.size());
        for (const auto& [bid, b] : w.buildings)
            if (b.ticks_remaining > 0)
                bids.push_back(bid);
        std::sort(bids.begin(), bids.end());
        out.under_construction = static_cast<int>(bids.size());

        for (const entity_id bid : bids)
        {
            const building_component&  b    = w.buildings.at(bid);
            const building_economics&  econ = reg.economics(b.type);
            if (econ.build_duration_ticks <= 0.0f)
                continue;
            const auto& row = reg.resource_build_cost_for(b.type, b.target_resource, b.recipe);
            for (std::size_t r = 0; r < resource_count; ++r)
                if (row[r] > 0.0f)
                    out.construction[r] += static_cast<double>(row[r] / econ.build_duration_ticks);
            // BL-709: the sector's own draw is part of THIS channel, and it has
            // to be counted here or the reconciliation below misattributes it.
            // `processing` is derived by subtraction (wants_folded less this
            // half less the upkeep half), so a construction want the census does
            // not recognise does not vanish — it silently reappears as a
            // processor's input bid, which is a worse lie than a missing row.
            // Mirrors `run_construction`'s own expression exactly: flat per
            // tick, NOT divided by build_duration_ticks.
            out.construction[static_cast<std::size_t>(resource_type::construction_capacity)] +=
                static_cast<double>(reg.construction().capacity_per_build_tick);
        }
    }

    economy_report rep = tick_to_clearing(w, reg, warm_ticks, lp);
    accumulate_production(rep);

    // clear_markets' own demand phase, in clear_markets' own order, reading the
    // prices clear_markets would read on this tick.
    const market_supply_snapshot prior_supply = snapshot_market_supply(w);
    const std::vector<entity_id> mids = sorted_market_ids(w);

    // --- BL-655 R3: the price consequence ----------------------------------
    // Taken here, BEFORE the demand register is re-injected below: the prices
    // standing now are the ones the census tick's clear_markets resolved, and
    // nothing between here and the print touches market_component::price.
    // The band comes off the registry (price_band()), never a local constant —
    // the BL-442 rule.
    {
        const float floor_mult = reg.price_band().floor_mult;
        const float ceil_mult  = reg.price_band().ceil_mult;
        out.price_floor_mult   = floor_mult;
        out.price_ceil_mult    = ceil_mult;
        for (const entity_id mid : mids)          // sorted — no map-order dependence
        {
            const market_component& mc = w.markets.at(mid);
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                const float base = mc.base_price[r];
                if (base <= 0.0f)
                    continue;                     // unpriced: the `px` column already says so
                out.markets_pricing[r] += 1;
                out.price_mean[r]      += static_cast<double>(mc.price[r]);
                out.price_ratio[r]     += static_cast<double>(mc.price[r] / base);
                if (mc.price[r] <= base * floor_mult * 1.001f)
                    out.markets_at_floor[r] += 1;
                if (mc.price[r] >= base * ceil_mult * 0.999f)
                    out.markets_at_ceil[r] += 1;
            }
        }
        for (std::size_t r = 0; r < resource_count; ++r)
            if (out.markets_pricing[r] > 0)
            {
                out.price_mean[r]  /= out.markets_pricing[r];
                out.price_ratio[r] /= out.markets_pricing[r];
            }
    }

    zero_demand(w);
    inject_population_demand(w, reg);
    out.household = sum_demand(w, mids);

    inject_background_demand(w, reg);
    out.background = sub(sum_demand(w, mids), out.household);

    // BL-647: the endemic pull, in clear_markets' own order — after the two
    // sibling baskets, before the inter-body redistribution.
    inject_endemic_demand(w, reg);
    {
        res_row after = sum_demand(w, mids);
        for (std::size_t r = 0; r < resource_count; ++r)
            out.endemic[r] = after[r] - out.household[r] - out.background[r];
    }

    inject_interbody_demand(w, reg, prior_supply);
    {
        res_row after = sum_demand(w, mids);
        for (std::size_t r = 0; r < resource_count; ++r)
            out.interbody[r] = after[r] - out.household[r] - out.background[r]
                             - out.endemic[r];
    }

    // BL-644: the State channel — re-derive the space programme's claims on
    // the census tick's world state, exactly as the census re-runs the demand
    // injections above. A pure read: the derivation mutates nothing, and the
    // scratch claims are discarded. What it measures is the goods demand the
    // state WOULD place this tick — a pool purchase, never market demand, so
    // it is reported beside the pool columns and folded into no total.
    {
        std::vector<budget_claim> scratch;
        const std::vector<space_purchase> intents = derive_space_programme_claims(
            w, w.nation_budgets, reg.space_programme(), scratch);
        for (const space_purchase& sp : intents)
            out.state_pl[static_cast<std::size_t>(sp.resource)] +=
                static_cast<double>(sp.quantity);
    }

    // BL-643: the Infrastructure channel, same re-derivation discipline — the
    // bill the network would present this tick (quantity is stock-capped by
    // the derivation, so this is the fundable want, not the unbounded need).
    {
        std::vector<budget_claim> scratch;
        const std::vector<network_purchase> intents = derive_network_upkeep_claims(
            w, w.nation_budgets, reg.network_upkeep(), scratch);
        for (const network_purchase& np : intents)
            out.infra_pl[static_cast<std::size_t>(np.resource)] +=
                static_cast<double>(np.quantity);
    }

    // The want register. `clear_markets` drops a want whose (corp, key) is not a
    // market (BL-1003: a body-level key); the dropped mass is REPORTED, not folded.
    for (const auto& [key, wanted] : rep.wants)   // std::map — sorted keys
    {
        const bool folds = (w.markets.find(key.second) != w.markets.end());
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (wanted[r] <= 0.0f)
                continue;
            out.wants_raw[r] += wanted[r];
            if (folds)
                out.wants_folded[r] += wanted[r];
            else
                out.wants_dropped_no_market += wanted[r];
        }
    }

    // BL-654: the UPKEEP half of the want register, taken off the report's own
    // attribution mirror rather than re-derived from the baskets — the mirror is
    // written by the same statement that writes the bid, so it cannot disagree
    // with it. Folded on the same rule the wants above are: a want on a body
    // with no market never reaches a demand register.
    for (const auto& [key, wanted] : rep.upkeep_wants)  // std::map — sorted keys
    {
        if (w.markets.find(key.second) == w.markets.end())
            continue;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (wanted[r] > 0.0f)
                out.upkeep_bid[r] += wanted[r];
    }

    // Processing = the want register less its construction and upkeep halves. A
    // negative residual means the attribution is wrong — R2's kind of failure.
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        out.processing[r] = out.wants_folded[r] - out.construction[r] - out.upkeep_bid[r];
        if (out.processing[r] < -1e-3)
            out.attribution_ok = false;
        if (out.processing[r] < 0.0)
            out.processing[r] = 0.0;
    }
    // The construction column is capped at what actually folded, for the same
    // reason: a site on a body with no market registers a want the market never
    // hears, and printing it in a MARKET demand column would overstate the total.
    // BL-654: less the upkeep bid, which is a measured share of that same folded
    // total rather than a modelled one.
    for (std::size_t r = 0; r < resource_count; ++r)
        out.construction[r] = std::min(out.construction[r],
                                       std::max(0.0, out.wants_folded[r] - out.upkeep_bid[r]));

    // The standing-force pool draw — a real sink that never reaches a market.
    {
        std::vector<entity_id> uids;
        uids.reserve(w.units.size());
        for (const auto& kv : w.units)
            uids.push_back(kv.first);
        std::sort(uids.begin(), uids.end());
        out.units = static_cast<int>(uids.size());
        const unit_upkeep_params& up = reg.military().upkeep;
        for (const entity_id uid : uids)
        {
            const unit_component& u = w.units.at(uid);
            out.heads += static_cast<int>(u.count);
            const unit_upkeep_draw d = resolve_unit_upkeep(u, up);
            for (std::size_t r = 0; r < resource_count; ++r)
                out.upkeep_pool[r] += d.goods[r];
        }
    }

    // BL-641 — the INDUSTRY pool draw, measured the same way: what one tick of
    // run_building_upkeep would draw across the whole world. Sized off the pass's
    // OWN eligibility rule (complete, not decommissioned) and its own band-composing
    // accessor, so what is printed is what is drawn. Ascending building id, matching
    // the pass, though a sum commutes — the order is kept because a census that
    // walked an unordered map would be a bad example to copy.
    {
        std::vector<entity_id> bids;
        bids.reserve(w.buildings.size());
        for (const auto& kv : w.buildings)
            bids.push_back(kv.first);
        std::sort(bids.begin(), bids.end());
        for (const entity_id bid : bids)
        {
            const building_component& bc = w.buildings.at(bid);
            if (bc.ticks_remaining > 0 || bc.decommissioned)
                continue;
            ++out.industry_eligible;
            const auto basket = building_upkeep_goods(reg.building_upkeep(), bc.type, reg.era());
            bool draws = false;
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                out.industry_pool[r] += basket[r];
                if (basket[r] > 0.0f)
                    draws = true;
            }
            if (draws)
                ++out.industry_drawing;
        }
    }

    // --- world shape and resource_classification ------------------------------------
    out.tiles     = static_cast<int>(w.tiles.size());
    out.markets   = static_cast<int>(w.markets.size());
    out.buildings = static_cast<int>(w.buildings.size());
    out.corps     = static_cast<int>(w.corporations.size());
    out.centres   = static_cast<int>(w.population_centres.size());
    {
        // Integer accumulation, so the unordered walk cannot vary the sum.
        long long scale_sum = 0;
        for (const auto& [cid, pcc] : w.population_centres)
        {
            (void)cid;
            if (!pcc.razed)
                scale_sum += static_cast<long long>(pcc.scale);
        }
        out.centre_scale = static_cast<double>(scale_sum);
    }

    out.cls = classify_resources(w, reg);

    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const resource_classification& c = out.cls[r];
        // BL-644/BL-643: a nation-purchased good has a real paying consumer,
        // so it is not sinkless — but its want never reaches a price, which
        // the separate nation-only list below keeps visible.
        const bool nation_bought = c.sink_state || c.sink_infra;
        if (c.produced_by_recipe && !c.any_market_sink() && !nation_bought)
            out.no_sink_produced.emplace_back(rname(r));
        else if (!c.produced_by_recipe && c.has_deposit && !c.any_market_sink() && !nation_bought)
            out.no_sink_raws.emplace_back(rname(r));
        if (nation_bought && !c.any_market_sink())
            out.state_only.emplace_back(rname(r));
        if ((c.sink_household || c.sink_background) && !c.produced_by_recipe && !c.has_deposit)
            out.basket_unmakeable.emplace_back(rname(r));
        if ((c.sink_household || c.sink_background) && !c.priced)
            out.basket_unpriced.emplace_back(rname(r));
    }
    std::sort(out.no_sink_produced.begin(), out.no_sink_produced.end());
    std::sort(out.no_sink_raws.begin(), out.no_sink_raws.end());
    std::sort(out.state_only.begin(), out.state_only.end());
    std::sort(out.basket_unmakeable.begin(), out.basket_unmakeable.end());
    std::sort(out.basket_unpriced.begin(), out.basket_unpriced.end());

    // BL-706 R5. Last, and deliberately so: it builds the body reach fields (a
    // cache on `world`) and reads the resource_classification above. Nothing after it
    // touches the world, so the Dijkstra it triggers perturbs no measurement.
    out.reach_budget = reg.construction().max_logistics_reach;
    out.completeness = measure_market_completeness(w, reg, out.cls);
    // The promoted measure returns the numbers; naming them is presentation and
    // stays here, which is why it no longer takes an out-param for the names.
    for (const std::size_t tr : terminal_resources(out.cls))
        out.terminal_goods.emplace_back(rname(tr));
    {
        std::vector<double> v;
        v.reserve(out.completeness.size());
        for (const market_completeness& m : out.completeness)
            v.push_back(m.completeness);
        out.spread = summarise_spread(std::move(v));
    }

    // BL-979: the balance half of saturation — what the search scores on as its
    // term 2 — off the same implementation, printed beside R5. Reported, never
    // gated. Reads the reach fields R5 just built (memoised), so it perturbs no
    // measurement either.
    out.pin_ratio = k_balance_pin_ratio;
    out.balance   = fraction_in_band(w, reg, out.pin_ratio);
    {
        std::vector<double> v;
        v.reserve(out.balance.size());
        for (const market_balance& m : out.balance)
            v.push_back(m.fraction);
        out.balance_spread = summarise_spread(std::move(v));
    }

    return out;
}

// ---------------------------------------------------------------------------
// Printing — fixed widths, fixed order, no wall-clock (R3)
// ---------------------------------------------------------------------------

std::string join(const std::vector<std::string>& v)
{
    if (v.empty())
        return "(none)";
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        if (i != 0)
            s += ", ";
        s += v[i];
    }
    return s;
}

void print_band(const band_result& b)
{
    std::printf("\n======================================================================"
                "=========================================\n");
    std::printf("=== BAND %s (pinned; the world's own band is %s%s) ===\n", b.band.c_str(),
                era_band_name(b.world_band),
                b.band == era_band_name(b.world_band) ? " — this is the app's roster"
                                                      : " — NOT the roster the app opens on");
    std::printf("  world     : %d tiles, %d markets, %d centres (scale sum %.0f), "
                "%d buildings (%d building), %d corps, %d units / %d heads\n",
                b.tiles, b.markets, b.centres, b.centre_scale, b.buildings,
                b.under_construction, b.corps, b.units, b.heads);
    std::printf("  registry  : %d of %d authored recipes allowed in band; max chain depth %d\n",
                b.recipes_allowed, b.recipes_authored, b.max_depth);
    std::printf("  industry  : %d of %d buildings eligible to draw upkeep (complete, not "
                "decommissioned); %d of those carry an authored basket in this band\n",
                b.industry_eligible, b.buildings, b.industry_drawing);

    std::printf("\n  --- per-resource demand census, census tick, summed over every market ---\n");
    std::printf("  BL-654: `upkeep/bd` is the shortfall a short pool BID on the market, so it is\n"
                "  MARKET demand; `upkeep/pl` and `indust/pl` are the GROSS per-tick need with the\n"
                "  pool-covered part included, and are not. bd <= pl + ind, always.\n");
    std::printf("  %-3s %-22s %-4s %-4s | %10s %10s %10s %10s %10s %10s %10s | %11s | %10s %10s %10s %10s | %10s | %s\n",
                "id", "resource", "prod", "d px",
                "household", "backgrnd", "endemic", "interbody", "construct", "process", "upkeep/bd",
                "MKT TOTAL", "upkeep/pl", "indust/pl", "state/pl", "infra/pl", "produced", "structural sinks");
    std::printf("  %-3s %-22s %-4s %-4s | %10s %10s %10s %10s %10s %10s %10s | %11s | %10s %10s %10s %10s | %10s | %s\n",
                "---", "----------------------", "----", "----",
                "----------", "----------", "----------", "----------", "----------", "----------", "----------",
                "-----------", "----------", "----------", "----------", "----------", "----------", "----------------");

    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const resource_classification& c = b.cls[r];
        const double total = b.household[r] + b.background[r] + b.endemic[r] + b.interbody[r]
                           + b.construction[r] + b.processing[r] + b.upkeep_bid[r];
        std::printf("  %-3zu %-22s %-4s %2d %-1s | %10.3f %10.3f %10.3f %10.3f %10.3f %10.3f %10.3f | "
                    "%11.3f | %10.3f %10.3f %10.3f %10.3f | %10.1f | %s\n",
                    r, rname(r), prod_word(c), c.depth, c.priced ? "$" : "-",
                    b.household[r], b.background[r], b.endemic[r], b.interbody[r],
                    b.construction[r], b.processing[r], b.upkeep_bid[r], total,
                    b.upkeep_pool[r], b.industry_pool[r], b.state_pl[r], b.infra_pl[r],
                    b.produced[r], sink_word(c).c_str());
    }

    std::printf("  %-3s %-22s %-4s %-4s | %10.3f %10.3f %10.3f %10.3f %10.3f %10.3f %10.3f | %11.3f | "
                "%10.3f %10.3f %10.3f %10.3f | %10.1f |\n",
                "", "TOTAL", "", "",
                total_of(b.household), total_of(b.background), total_of(b.endemic),
                total_of(b.interbody),
                total_of(b.construction), total_of(b.processing), total_of(b.upkeep_bid),
                total_of(b.household) + total_of(b.background) + total_of(b.endemic)
                    + total_of(b.interbody)
                    + total_of(b.construction) + total_of(b.processing) + total_of(b.upkeep_bid),
                total_of(b.upkeep_pool), total_of(b.industry_pool), total_of(b.state_pl),
                total_of(b.infra_pl), total_of(b.produced));
    std::printf("  %-3s %-22s %-4s %-4s | %10d %10d %10d %10d %10d %10d %10d | %11s | %10d %10d %10d %10d | "
                "%10s |  (resources touched)\n",
                "", "BREADTH", "", "",
                touched_by(b.household), touched_by(b.background), touched_by(b.endemic),
                touched_by(b.interbody),
                touched_by(b.construction), touched_by(b.processing), touched_by(b.upkeep_bid), "",
                touched_by(b.upkeep_pool), touched_by(b.industry_pool), touched_by(b.state_pl),
                touched_by(b.infra_pl), "");

    if (b.wants_dropped_no_market > 0.0)
        std::printf("  note: %.3f units of want were registered on bodies carrying no market and "
                    "never reached a demand register.\n", b.wants_dropped_no_market);

    // --- BL-655 R3: the price consequence, beside the density ---------------
    // Only rows a market actually prices appear: an unpriced resource has no
    // price consequence to report and the `px` column above already names it.
    std::printf("\n  --- R3  the price consequence (census tick; band = [%.2f, %.2f] x base) ---\n",
                b.price_floor_mult, b.price_ceil_mult);
    std::printf("  A resource FLOORED in every market that prices it is a GLUT. Density bought by\n"
                "  flooring prices is a failure of the demand route, not a success (BL-655 R3).\n");
    std::printf("  %-3s %-22s | %10s %10s %10s | %7s %7s %7s | %s\n",
                "id", "resource", "base", "price", "px/base",
                "mkts", "floored", "ceiled", "verdict");
    std::printf("  %-3s %-22s | %10s %10s %10s | %7s %7s %7s | %s\n",
                "---", "----------------------", "----------", "----------", "----------",
                "-------", "-------", "-------", "-------");
    std::vector<std::string> floored_everywhere, ceiled_everywhere;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (b.markets_pricing[r] == 0)
            continue;
        const resource_classification& c = b.cls[r];
        if (!c.any_market_sink() && b.produced[r] <= 0.0)
            continue;   // neither bought nor made here — no consequence to read
        const double base  = (b.price_ratio[r] > 0.0) ? (b.price_mean[r] / b.price_ratio[r]) : 0.0;
        const bool   floor = (b.markets_at_floor[r] == b.markets_pricing[r]);
        const bool   ceil  = (b.markets_at_ceil[r] == b.markets_pricing[r]);
        if (floor) floored_everywhere.emplace_back(rname(r));
        if (ceil)  ceiled_everywhere.emplace_back(rname(r));
        std::printf("  %-3zu %-22s | %10.3f %10.3f %10.3f | %7d %7d %7d | %s\n",
                    r, rname(r), base, b.price_mean[r], b.price_ratio[r],
                    b.markets_pricing[r], b.markets_at_floor[r], b.markets_at_ceil[r],
                    floor ? "GLUT (floored everywhere)"
                          : (ceil ? "SHORTAGE (ceiled everywhere)" : "in band"));
    }
    std::printf("  floored in EVERY market that prices it : %s\n", join(floored_everywhere).c_str());
    std::printf("  ceiled  in EVERY market that prices it : %s\n", join(ceiled_everywhere).c_str());

    std::printf("\n  --- what this band cannot buy ---\n");
    std::printf("  produced in-band, NO market sink : %s\n", join(b.no_sink_produced).c_str());
    std::printf("  extractable, NO market sink      : %s\n", join(b.no_sink_raws).c_str());
    std::printf("  nation-purchased ONLY (paid pool draws, never a market bid - BL-644/BL-643): %s\n",
                join(b.state_only).c_str());
    std::printf("  a basket names it, band cannot make it or dig it: %s\n",
                join(b.basket_unmakeable).c_str());
    std::printf("  a basket names it, NO market prices it (BL-652, asserted by R6): %s\n",
                join(b.basket_unpriced).c_str());

    int live = 0;
    if (total_of(b.household)    > 0.0) ++live;
    if (total_of(b.background)   > 0.0) ++live;
    if (total_of(b.endemic)      > 0.0) ++live;   // BL-647
    if (total_of(b.interbody)    > 0.0) ++live;
    if (total_of(b.construction) > 0.0) ++live;
    if (total_of(b.processing)   > 0.0) ++live;
    std::printf("  live injecting passes this band  : %d of 6 measured\n", live);

    // --- BL-706 R5: chain completeness, and the spread ----------------------
    std::printf("\n  --- R5  CHAIN COMPLETENESS PER MARKET, and its SPREAD (BL-706) ---\n");
    std::printf("  Fraction of the band's TERMINAL chains a market could source WITHIN REACH.\n"
                "  Grain: the market catchment (market_for_tile). Reach: place_building_allowed's\n"
                "  own clause against economy.construction.max_logistics_reach = %.1f.\n",
                static_cast<double>(b.reach_budget));
    std::printf("  Structural, not observed: what the ground and the band's recipes PERMIT.\n");
    std::printf("  The SPREAD is the deliverable. No row here fails on a market being poor.\n");
    std::printf("  terminal set (%zu goods, the denominator for EVERY market): %s\n",
                b.terminal_goods.size(), join(b.terminal_goods).c_str());

    // BL-979: the in-band columns sit beside completeness, joined by market id
    // (both measures walk sorted_market_ids, but a join is cheaper than an
    // assumption).
    std::map<entity_id, const market_balance*> by_market;
    for (const market_balance& m : b.balance)
        by_market[m.market] = &m;

    std::printf("\n  %-10s %-8s | %9s %9s %9s | %6s | %6s %6s | %-12s | %6s %6s | %s\n",
                "market", "body", "catchment", "in reach", "heads",
                "raws", "closed", "of", "completeness", "inband", "rated", "in band");
    std::printf("  %-10s %-8s | %9s %9s %9s | %6s | %6s %6s | %-12s | %6s %6s | %s\n",
                "----------", "--------", "---------", "---------", "---------",
                "------", "------", "------", "------------", "------", "------", "-------");
    for (const market_completeness& m : b.completeness)
    {
        const auto bit = by_market.find(m.market);
        const market_balance* mb = (bit == by_market.end()) ? nullptr : bit->second;
        std::printf("  %-10llu %-8llu | %9d %9d %9lld | %6d | %6d %6d | %-12.4f | %6d %6d | %.4f\n",
                    static_cast<unsigned long long>(m.market),
                    static_cast<unsigned long long>(m.body),
                    m.catchment_tiles, m.in_reach_tiles, m.heads,
                    m.raws_in_reach, m.terminals_closed, m.terminals_total,
                    m.completeness,
                    mb ? mb->balanced : 0, mb ? mb->rated : 0,
                    mb ? mb->fraction : 0.0);
    }

    // --- BL-979: FRACTION IN BAND, per market and as a spread. REPORTED, not gated.
    {
        const spread_stats& bs = b.balance_spread;
        int any_in_band = 0, rated_sum = 0, balanced_sum = 0;
        for (const market_balance& m : b.balance)
        {
            if (m.balanced > 0)
                ++any_in_band;
            rated_sum    += m.rated;
            balanced_sum += m.balanced;
        }
        std::printf("\n  FRACTION IN BAND (BL-979): of a market's PRICED resources with any signal, the share\n"
                    "  whose static supply:demand ratio sits inside [1/%.1f, %.1f] — landscape_score's\n"
                    "  term 2, read off market_saturation::fraction_in_band. A REPORT: no row gates on it;\n"
                    "  where the band belongs is Ben's call.\n",
                    b.pin_ratio, b.pin_ratio);
        std::printf("  SPREAD over %d markets: min %.4f  median %.4f  max %.4f   (p25 %.4f  p75 %.4f  mean %.4f  sd %.4f)\n",
                    bs.n, bs.min, bs.median, bs.max, bs.p25, bs.p75, bs.mean, bs.sd);
        std::printf("  markets with ANY priced good in band: %d of %d;  pooled: %d of %d rated (%.4f)\n",
                    any_in_band, bs.n, balanced_sum, rated_sum,
                    rated_sum > 0 ? static_cast<double>(balanced_sum) / rated_sum : 0.0);
    }

    const spread_stats& s = b.spread;
    std::printf("\n  COMPLETENESS SPREAD over %d markets: min %.4f  p25 %.4f  median %.4f  p75 %.4f  max %.4f\n",
                s.n, s.min, s.p25, s.median, s.p75, s.max);
    std::printf("           range %.4f   mean %.4f   sd %.4f   distinct scores %d of %d markets\n",
                s.range, s.mean, s.sd, s.distinct, s.n);
    std::printf("  histogram (decile of completeness -> markets):\n");
    for (std::size_t i = 0; i < s.hist.size(); ++i)
    {
        std::printf("    [%.1f,%.1f)%s %4d  ", static_cast<double>(i) / 10.0,
                    static_cast<double>(i + 1) / 10.0, (i == 9) ? "]" : " ", s.hist[i]);
        for (int k = 0; k < s.hist[i] && k < 60; ++k)
            std::printf("#");
        std::printf("\n");
    }
    if (s.n >= 2 && s.distinct == 1)
        std::printf("  FLAT: every market scores identically. Generation produced no supply\n"
                    "        asymmetry at all in this band — see GENERATION_STRATEGY.md\n"
                    "        § Asymmetry is the deliverable. Reported, not asserted.\n");
    if (total_of(b.upkeep_bid)   > 0.0) ++live;   // BL-654: the upkeep bid
    if (total_of(b.state_pl)     > 0.0) ++live;   // BL-644: the state's pool purchase
    if (total_of(b.infra_pl)     > 0.0) ++live;   // BL-643: the network's material bill
    std::printf("  live injecting passes this band  : %d of 9 measured\n", live);
}

} // namespace

int main(int argc, char** argv)
{
    uint32_t seed       = 0;
    int      warm_ticks = 80;
    bool     prehistory = true;
    std::string bands   = "both";
    float    reach_override = -1.0f;   ///< NR-763 probe; < 0 = use the authored value.

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--fast") == 0)
            prehistory = false;
        else if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
            seed = static_cast<uint32_t>(std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc)
            warm_ticks = std::max(0, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--band") == 0 && i + 1 < argc)
            bands = argv[++i];
        // NR-763 PROBE KNOB. `economy.construction.max_logistics_reach` is the
        // clause R5 measures chain completeness against, and NR-763 asks whether
        // the shipped 24.0 is why self-sufficiency is the NORM rather than the
        // exception: market 48706 has a 493-tile catchment and ALL 493 are in
        // reach. Ben's own recommended first probe is "vary this one constant and
        // re-read the spread", so it is a FLAG rather than an edit-and-revert of
        // a shipped value - the probe is repeatable and the authored constant is
        // never touched. Absent, the registry's authored value stands and every
        // reading is unchanged.
        else if (std::strcmp(argv[i], "--reach") == 0 && i + 1 < argc)
            reach_override = static_cast<float>(std::atof(argv[++i]));
    }

    // The real data layer, loaded as app::load_economy loads it. A restated
    // registry would make the census an answer about a world nobody plays.
    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    // world_gen.lua carries `kepler_market.base_price`, and BOTH basket injectors
    // skip a resource whose market base_price is 0. Omit it and the census reports
    // a dead channel that is only unpriced -- the exact false negative it exists to
    // prevent, and one this harness produced on its first run.
    lua.load("scripts/world_gen.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    world_gen_config gen_cfg;
    gen_cfg.load_from_lua(lua);

    // NR-763: applied AFTER load_from_lua so the override is the last word, and
    // echoed in the header below so a saved run always says which budget produced
    // it. R5 already prints `reach_budget` off the registry, so the per-band
    // tables carry it too - there is no way to read a probe run and mistake it
    // for a shipped one.
    if (reach_override >= 0.0f)
    {
        construction_params cp = reg.construction();
        cp.max_logistics_reach = reach_override;
        reg.set_construction(cp);
    }

    // Vacuity guard (the standing lesson from interbody_pull_harness): an empty
    // registry would print every resource as equally unwanted and diagnose nothing.
    if (reg.recipe_count() == 0 ||
        reg.economics(building_type::extraction_site).maintenance <= 0.0f)
    {
        std::printf("FATAL: the registry authored no recipes / no extraction maintenance — "
                    "scripts/*.lua did not load.\n");
        return 2;
    }
    {
        // BL-640: the SHARED tranche plus every banded row (this registry is
        // still band-agnostic here, so era_permits admits all of them) - an
        // authored basket living entirely in `baskets` must not read as empty.
        const std::array<float, resource_count>& pd_basket = reg.population_demand_basket();
        double basket = 0.0;
        for (std::size_t r = 0; r < resource_count; ++r)
            basket += pd_basket[r];
        if (basket <= 0.0)
        {
            std::printf("FATAL: economy.population_demand authored no basket weight at all — "
                        "the census would report every channel as dead.\n");
            return 2;
        }
    }

    std::printf("demand_census — BL-649, requirement group `demand-census` R1-R6\n");
    std::printf("  seed %u | warm ticks %d | prehistory %s | bands %s | reach %.1f%s\n",
                seed, warm_ticks,
                prehistory ? "ON (the shipped spawn)" : "OFF (--fast, NOT the spawn)",
                bands.c_str(), reg.construction().max_logistics_reach,
                reach_override >= 0.0f ? " (--reach OVERRIDE, NR-763 probe)" : " (authored)");
    std::printf("  IT REPORTS. No row below fails on a magnitude; see the file header.\n");

    // -----------------------------------------------------------------------
    // R1a — the channel register
    // -----------------------------------------------------------------------
    std::printf("\n=== R1a  THE CHANNEL REGISTER — the eight of MARKETS.md § Demand channels ===\n");
    std::printf("  An ABSENT row is a finding, not an omission: it is a channel the design names\n"
                "  and the code does not have. Every one of the eight is listed.\n\n");
    std::printf("  %-16s %-8s %s\n", "CHANNEL", "STATE", "INJECTOR, OR WHAT IS MISSING");
    for (std::size_t i = 0; i < k_channel_count; ++i)
        std::printf("  %-16s %-8s %s\n", k_channels[i].channel,
                    state_word(k_channels[i].state), k_channels[i].injector);

    std::printf("\n  OFF-REGISTER PASSES — real injectors that are NOT one of the eight:\n\n");
    std::printf("  %-16s %-8s %s\n", "PASS", "STATE", "WHAT IT IS");
    for (std::size_t i = 0; i < k_off_register_count; ++i)
        std::printf("  %-16s %-8s %s\n", k_off_register[i].channel,
                    state_word(k_off_register[i].state), k_off_register[i].injector);

    // -----------------------------------------------------------------------
    // R1b — the per-band census
    // -----------------------------------------------------------------------
    std::vector<band_result> results;
    const bool want_ancient    = (bands == "both" || bands == "ancient");
    const bool want_industrial = (bands == "both" || bands == "industrial");

    if (want_ancient)
        results.push_back(run_band("ancient", era_band::ancient, seed, warm_ticks, prehistory, reg, gen_cfg));
    if (want_industrial)
        results.push_back(run_band("industrial", era_band::industrial, seed, warm_ticks, prehistory, reg, gen_cfg));

    for (const band_result& b : results)
        print_band(b);

    // -----------------------------------------------------------------------
    // The verification rows
    // -----------------------------------------------------------------------
    std::printf("\n=== VERIFICATION ===\n");

    check(verify_name_table(), "R2",
          "the print name table matches resource_names entry-for-entry and covers the enum");

    {
        bool states_ok = (k_channel_count == 8);
        for (std::size_t i = 0; i < k_channel_count; ++i)
            if (k_channels[i].state != ch_state::present &&
                k_channels[i].state != ch_state::absent &&
                k_channels[i].state != ch_state::stopgap)
                states_ok = false;
        check(states_ok, "R1",
              "all eight MARKETS.md demand channels enumerated, each PRESENT or ABSENT");
    }

    check(!results.empty(), "R1", "at least one era band was censused");

    for (const band_result& b : results)
    {
        char msg[240];

        std::snprintf(msg, sizeof msg,
                      "%s: the want register reconciles — construction half never exceeds "
                      "the folded want", b.band.c_str());
        check(b.attribution_ok, "R2", msg);

        std::snprintf(msg, sizeof msg,
                      "%s: every resource classified (produced/deposit/depth/sinks all resolved)",
                      b.band.c_str());
        bool classified = true;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (std::strcmp(rname(r), "<UNNAMED>") == 0)
                classified = false;
        check(classified, "R2", msg);

        // Anti-vacuity: a census that measured NOTHING is not a passing census,
        // it is a broken instrument. This is a >0 check on the instrument, not a
        // target on the economy.
        const double any_demand = total_of(b.household) + total_of(b.background)
                                + total_of(b.interbody) + total_of(b.construction)
                                + total_of(b.processing) + total_of(b.upkeep_bid);
        std::snprintf(msg, sizeof msg,
                      "%s: the instrument is non-vacuous — some pass injected some demand",
                      b.band.c_str());
        check(any_demand > 0.0, "R2", msg);

        // --- BL-706 R5 ------------------------------------------------------
        // Anti-vacuity on the NEW instrument, in exactly the shape of the row
        // above it: a completeness reading over zero markets, or against an
        // empty terminal set, is a broken instrument reporting 0.00 everywhere
        // and diagnosing nothing. This is a check on the census, NOT a target
        // on the world — the world's own value is reported and never asserted.
        std::snprintf(msg, sizeof msg,
                      "%s: the completeness instrument is non-vacuous — %zu markets read "
                      "against a terminal set of %zu goods",
                      b.band.c_str(), b.completeness.size(), b.terminal_goods.size());
        check(!b.completeness.empty() && !b.terminal_goods.empty(), "R5", msg);

        // Internal consistency, R2's kind: a market cannot close more chains
        // than the band has, and the ratio has to be the two counts it prints.
        bool ratio_ok = true;
        for (const market_completeness& m : b.completeness)
        {
            if (m.terminals_closed < 0 || m.terminals_closed > m.terminals_total ||
                m.in_reach_tiles > m.catchment_tiles)
                ratio_ok = false;
            const double expect = (m.terminals_total > 0)
                                ? static_cast<double>(m.terminals_closed)
                                  / static_cast<double>(m.terminals_total) : 0.0;
            if (std::fabs(expect - m.completeness) > 1e-9)
                ratio_ok = false;
        }
        std::snprintf(msg, sizeof msg,
                      "%s: every completeness row reconciles (closed <= total, in-reach <= "
                      "catchment, ratio = the printed counts)", b.band.c_str());
        check(ratio_ok, "R5", msg);

        // THE ONE SPREAD ASSERTION, and it is deliberately the loosest one that
        // still means something. It does NOT say a market must be rich or poor,
        // and it sets no floor on any market's value — GENERATION_STRATEGY.md
        // § Asymmetry is the deliverable is explicit that generation owes the
        // spread and nothing about an individual region. What it says is that a
        // world in which EVERY market scores the identical number carries no
        // supply asymmetry at all, and that is a generation defect however fine
        // each market looks alone. Two distinct values clear it; the shipped
        // world is far above that, so this catches a flattening regression
        // rather than grading the current tuning.
        std::snprintf(msg, sizeof msg,
                      "%s: the world is not FLAT — markets do not all score identically "
                      "(%d distinct of %d; range %.4f). A floor on the SPREAD only",
                      b.band.c_str(), b.spread.distinct, b.spread.n, b.spread.range);
        check(b.spread.n < 2 || b.spread.distinct >= 2, "R5", msg);
        // --- R6 (BL-652) — THE ONE MAGNITUDE-FREE ROW THAT FAILS -------------
        // Both basket injectors SKIP a resource whose market base_price is 0,
        // and they do it in silence: from every total downstream, an authored
        // but unpriced basket entry is indistinguishable from a channel nobody
        // wrote. Two separate bugs hid behind that on one day in August 2026 —
        // this census read as having no background demand at all, and
        // spawn_solvency measured a whole spawn diagnosis in a world where
        // background demand did not exist. NEITHER FAILED. Both quietly
        // answered a question about a different world.
        //
        // It is not a magnitude assertion and does not breach R2's discipline:
        // the combination is ALWAYS either a missing script (world_gen.lua,
        // which carries kepler_market.base_price) or an authoring error, and it
        // is never intended. `src/world/market_clearing.cpp`'s
        // `unpriced_basket_entries` is the same finding at runtime, where the
        // app warns rather than fails.
        std::snprintf(msg, sizeof msg,
                      "%s: no demand basket names a good NO market prices — %s",
                      b.band.c_str(),
                      b.basket_unpriced.empty() ? "none do"
                                                : join(b.basket_unpriced).c_str());
        check(b.basket_unpriced.empty(), "R6", msg);
    }

    // R4 — the hand-built finding, at the ancient band only.
    const band_result* ancient = nullptr;
    for (const band_result& b : results)
        if (b.band == "ancient")
            ancient = &b;

    std::printf("\n--- R4  THE HAND-BUILT FINDING (0 CE band) ---\n");
    if (ancient == nullptr)
    {
        std::printf("  ancient band not censused this run (--band %s); R4 not evaluated.\n",
                    bands.c_str());
    }
    else
    {
        std::vector<std::string> expected(k_r4_expected, k_r4_expected + k_r4_expected_count);
        std::sort(expected.begin(), expected.end());
        const std::vector<std::string>& got = ancient->no_sink_produced;

        std::vector<std::string> only_census, only_hand;
        std::set_difference(got.begin(), got.end(), expected.begin(), expected.end(),
                            std::back_inserter(only_census));
        std::set_difference(expected.begin(), expected.end(), got.begin(), got.end(),
                            std::back_inserter(only_hand));

        std::printf("  hand-built (NR-671) : %s\n", join(expected).c_str());
        std::printf("  census              : %s\n", join(got).c_str());
        std::printf("  in census only      : %s\n", join(only_census).c_str());
        std::printf("  in hand list only   : %s\n", join(only_hand).c_str());
        check(only_census.empty() && only_hand.empty(), "R4",
              "the census reproduces the hand-built produced-with-no-sink set at 0 CE");
    }

    std::printf("\n%s — %d failure(s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
