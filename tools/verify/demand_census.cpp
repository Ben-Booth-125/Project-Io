// demand_census — BL-649. Per RESOURCE and per ERA BAND: how much demand the
// world models, and WHICH PASS injects it. Requirement group `demand-census`,
// rows R1-R4.
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
// hand-built finding), not on the economy.
//
// WHY IT LOADS LUA. The demand baskets ARE Lua data (economy.population_demand,
// economy.background_demand, economy.military.unit_upkeep), the recipe roster and
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
//     economy_report::wants         -> CONSTRUCTION + PROCESSING INPUTS
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
// STRUCTURAL SINK vs OBSERVED DEMAND. The per-resource table carries both, and
// the distinction matters. OBSERVED demand can be zero merely because nobody has
// built the consumer yet on this seed. A STRUCTURAL sink is a statement about the
// content: does any pass in this band NAME this resource at all — a household
// basket entry, a background basket entry, an input to an era-allowed recipe, a
// line in an era-available building's construction basket. A good with no
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
#include "world/contract_template.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/market_clearing.hpp"
#include "world/nation_step.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/supply_system.hpp"
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
    { "Industry",       ch_state::absent,
      "no goods vector on building upkeep -- apply_budget charges maintenance+wages in CREDITS only; "
      "cf. run_unit_upkeep, which does carry one (owner BL-641)" },
    { "Construction",   ch_state::present,
      "run_construction -> economy_report::wants (economy_system.cpp) <- economy.buildings.resource_costs "
      "/ material_overrides. Fires only where something is BUILDING (owner BL-642)" },
    { "Infrastructure", ch_state::absent,
      "no material draw for roads / ports / hubs anywhere in src/world; the logistics_maintenance "
      "budget line spends credits (owner BL-643)" },
    { "State",          ch_state::absent,
      "nation budget lines (incl. strategic_reserve) carry weights and spend credits; no goods "
      "purchase exists (owner BL-644)" },
    { "Research",       ch_state::absent,
      "research_institute credits corporation_component::science per tick; nothing draws goods "
      "(owner BL-645)" },
    { "Conflict",       ch_state::absent,
      "battle resolution consumes no goods; the only military draw is the per-head standing-force "
      "upkeep below, which is not a battle (owner BL-646)" },
    { "Endemic trade",  ch_state::absent,
      "no wealth-scaled or character-flavoured luxury basket; tobacco/spices/coffee/furs are named "
      "by no injector (owner BL-647)" },
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
      "run_unit_upkeep -- a POOL draw (economy.military.unit_upkeep.goods_per_head). It never reaches "
      "market_component::demand, so it consumes goods WITHOUT pricing them" },
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
const char* const k_r4_expected[] = {
    "ordnance", "rigging", "tools", "trade_goods_misc",
};
constexpr std::size_t k_r4_expected_count =
    sizeof(k_r4_expected) / sizeof(k_r4_expected[0]);

/// BL-573: run_nation_step's template registry. Empty is correct — nothing in
/// this census opens a mercenary contract, so the walk is vacuous.
const contract_template_registry g_no_contract_templates;

using res_row = std::array<double, resource_count>;

std::vector<entity_id> sorted_keys_markets(const world& w)
{
    std::vector<entity_id> ids;
    ids.reserve(w.markets.size());
    for (const auto& kv : w.markets)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

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
    dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                     reg.logistics_cost(convoy_mode::space), &lp);
    advance_convoys(w);
    return run_economy_step(w, reg, /*spectating=*/false, &lp);
}

/// The rest of the tick, for the warm start only.
void finish_tick(world& w, const recipe_registry& reg, int t, economy_report& rep)
{
    auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention, &rep.budgets, &rep.buildings,
                 &rep.building_labour);
    run_nation_step(w, reg, rep, t, g_no_contract_templates);
    advance_tech_gates(w);
    credit_arrived_convoys(w, t);
}

// ---------------------------------------------------------------------------
// Structural classification
// ---------------------------------------------------------------------------

struct classification
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
    bool sink_unit_upkeep = false;    ///< pool draw, NOT a market bid

    /// Does ANY market carry a base price for it? Both basket injectors skip a
    /// resource whose `base_price` is 0 ("untradeable -- no base price to anchor
    /// the elasticity curve"), so an unpriced basket entry is a want the engine
    /// silently discards. That is a channel going quiet without saying so, which
    /// is the one thing this census exists to make impossible.
    bool priced = false;

    bool any_market_sink() const
    {
        return sink_household || sink_background || sink_process || sink_construct;
    }
};

std::string sink_word(const classification& c)
{
    std::string s;
    auto add = [&s](const char* t) { if (!s.empty()) s += "+"; s += t; };
    if (c.sink_household)   add("HH");
    if (c.sink_background)  add("BG");
    if (c.sink_process)     add("PROC");
    if (c.sink_construct)   add("CONS");
    if (c.sink_unit_upkeep) add("upk");   // lower case: not a market bid
    if (s.empty())
        s = "NONE";
    return s;
}

const char* prod_word(const classification& c)
{
    if (c.produced_by_recipe && c.has_deposit) return "R+D";
    if (c.produced_by_recipe)                  return "REC";
    if (c.has_deposit)                         return "DEP";
    return "-- ";
}

std::array<classification, resource_count>
classify(const world& w, const recipe_registry& reg)
{
    std::array<classification, resource_count> c{};

    for (std::size_t r = 0; r < resource_count; ++r)
        c[r].depth = reg.depth_of(static_cast<resource_type>(r));

    // --- what an era-allowed recipe makes and eats -------------------------
    // The BROWSE path (recipe_count/recipe_at), which is the era-masked one.
    const int n_allowed = reg.recipe_count(building_type::processing_facility);
    const bool processing_available = reg.building_available(building_type::processing_facility);
    for (int i = 0; i < n_allowed && processing_available; ++i)
    {
        const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (rc.outputs[r] > 0.0f)
                c[r].produced_by_recipe = true;
            if (rc.inputs[r] > 0.0f)
                c[r].sink_process = true;
        }
    }

    // --- what the ground yields --------------------------------------------
    // A BOOLEAN only. `w.tiles` is unordered and a float sum over its layout
    // would be run-dependent; an OR is not.
    for (const auto& [tid, t] : w.tiles)
    {
        (void)tid;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (t.resource_deposit[r] > 0.0f)
                c[r].has_deposit = true;
    }

    // --- the two authored baskets, AS MASKED BY THIS BAND -------------------
    // BL-640: read population_demand_basket() / background_demand_basket(), the
    // registry's era-resolved folds - the exact vectors inject_population_demand
    // and inject_background_demand multiply by. `.demand_basket` on the params is
    // now the SHARED (`any`) tranche alone, and reading it here would have this
    // census attribute a structural sink to a band whose basket never names the
    // good - precisely the defect the banding closes. `reg` already carries this
    // band (set_era, above), so no extra state is threaded in.
    const std::array<float, resource_count>& pd_basket = reg.population_demand_basket();
    const std::array<float, resource_count>& bd_basket = reg.background_demand_basket();
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (pd_basket[r] > 0.0f)
            c[r].sink_household = true;
        if (bd_basket[r] > 0.0f)
            c[r].sink_background = true;
    }

    // --- construction baskets, over every era-available building -----------
    // extraction_site is keyed by target_resource and processing_facility by
    // recipe id (recipe_registry::resource_build_cost_for); every other type
    // resolves to its type-level basket. Enumerated through that ONE accessor so
    // the census cannot disagree with the draw run_construction actually makes.
    for (std::size_t bt_i = 0; bt_i < building_type_count; ++bt_i)
    {
        const building_type bt = static_cast<building_type>(bt_i);
        if (bt == building_type::none || !reg.building_available(bt))
            continue;

        auto note = [&c](const std::array<float, resource_count>& row) {
            for (std::size_t r = 0; r < resource_count; ++r)
                if (row[r] > 0.0f)
                    c[r].sink_construct = true;
        };

        if (bt == building_type::extraction_site)
        {
            for (std::size_t r = 0; r < resource_count; ++r)
                note(reg.resource_build_cost_for(bt, static_cast<resource_type>(r), no_recipe));
        }
        else if (bt == building_type::processing_facility)
        {
            for (int i = 0; i < n_allowed; ++i)
            {
                const recipe& rc = reg.recipe_at(bt, i);
                note(reg.resource_build_cost_for(bt, resource_type::iron_ore,
                                                 reg.recipe_id(rc.name)));
            }
        }
        else
        {
            note(reg.resource_build_cost_for(bt, resource_type::iron_ore, no_recipe));
        }
    }

    // --- is it priced on any market? ---------------------------------------
    for (const auto& [mid, mc] : w.markets)
    {
        (void)mid;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (mc.base_price[r] > 0.0f)
                c[r].priced = true;
    }

    // --- the standing-force pool draw --------------------------------------
    const unit_upkeep_params& up = reg.military().upkeep;
    for (std::size_t r = 0; r < resource_count; ++r)
        if (up.goods_per_head[r] > 0.0f)
            c[r].sink_unit_upkeep = true;

    return c;
}

// ---------------------------------------------------------------------------
// One band
// ---------------------------------------------------------------------------

struct band_result
{
    std::string band;
    int64_t     epoch = 0;

    // world shape
    int   tiles = 0, markets = 0, centres = 0, buildings = 0, under_construction = 0;
    int   corps = 0, units = 0, heads = 0;
    double centre_scale = 0.0;
    int   recipes_allowed = 0, recipes_authored = 0, max_depth = 0;

    // the census tick's attribution
    res_row household{}, background{}, interbody{}, construction{}, processing{};
    res_row wants_raw{}, wants_folded{}, upkeep_pool{};
    double  wants_dropped_no_market = 0.0;

    // observed production over the whole run
    res_row produced{};

    std::array<classification, resource_count> cls{};

    bool attribution_ok = true;   ///< processing residual non-negative
    std::vector<std::string> no_sink_produced;   ///< R4's set, sorted by name
    std::vector<std::string> no_sink_raws;       ///< extractable, unwanted
    std::vector<std::string> basket_unmakeable;  ///< a basket names what the band cannot produce
    std::vector<std::string> basket_unpriced;    ///< a basket names it, no market prices it -> skipped
};

band_result run_band(const char* band_name, int64_t epoch, uint32_t seed,
                     int warm_ticks, bool prehistory, recipe_registry& reg,
                     const world_gen_config& gen_cfg)
{
    band_result out;
    out.band  = band_name;
    out.epoch = epoch;

    reg.set_era(era_band_for_epoch(epoch));

    world_params p;
    p.seed       = seed;
    p.epoch_year = epoch;
    if (!prehistory)
        p = no_prehistory(p);   // preserves seed and epoch_year

    // app::setup_world -> load_economy -> generate_background_firms ->
    // assign_default_recipes, in that order (app.cpp § start_new_game_prelude).
    world w = make_hard_coded_world(p, nullptr, gen_cfg);
    assign_default_recipes(w, reg);
    generate_background_firms(w, reg, seed ^ 0x8A21F00Du);
    assign_default_recipes(w, reg);

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
        }
    }

    economy_report rep = tick_to_clearing(w, reg, warm_ticks, lp);
    accumulate_production(rep);

    // clear_markets' own demand phase, in clear_markets' own order, reading the
    // prices clear_markets would read on this tick.
    const market_supply_snapshot prior_supply = snapshot_market_supply(w);
    const std::vector<entity_id> mids = sorted_keys_markets(w);

    zero_demand(w);
    inject_population_demand(w, reg);
    out.household = sum_demand(w, mids);

    inject_background_demand(w, reg);
    out.background = sub(sum_demand(w, mids), out.household);

    inject_interbody_demand(w, reg, prior_supply);
    {
        res_row after = sum_demand(w, mids);
        for (std::size_t r = 0; r < resource_count; ++r)
            out.interbody[r] = after[r] - out.household[r] - out.background[r];
    }

    // The want register. `clear_markets` drops a want whose (corp, body) has no
    // market on that body; the dropped mass is REPORTED rather than folded away.
    std::set<entity_id> bodies_with_market;
    for (const entity_id mid : mids)
        bodies_with_market.insert(w.markets.at(mid).body);

    for (const auto& [key, wanted] : rep.wants)   // std::map — sorted keys
    {
        const bool folds = (bodies_with_market.count(key.second) != 0);
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

    // Processing = the want register less its construction half. A negative
    // residual means the attribution is wrong — R2's kind of failure.
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        out.processing[r] = out.wants_folded[r] - out.construction[r];
        if (out.processing[r] < -1e-3)
            out.attribution_ok = false;
        if (out.processing[r] < 0.0)
            out.processing[r] = 0.0;
    }
    // The construction column is capped at what actually folded, for the same
    // reason: a site on a body with no market registers a want the market never
    // hears, and printing it in a MARKET demand column would overstate the total.
    for (std::size_t r = 0; r < resource_count; ++r)
        out.construction[r] = std::min(out.construction[r], out.wants_folded[r]);

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

    // --- world shape and classification ------------------------------------
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

    out.cls = classify(w, reg);

    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const classification& c = out.cls[r];
        if (c.produced_by_recipe && !c.any_market_sink())
            out.no_sink_produced.emplace_back(rname(r));
        else if (!c.produced_by_recipe && c.has_deposit && !c.any_market_sink())
            out.no_sink_raws.emplace_back(rname(r));
        if ((c.sink_household || c.sink_background) && !c.produced_by_recipe && !c.has_deposit)
            out.basket_unmakeable.emplace_back(rname(r));
        if ((c.sink_household || c.sink_background) && !c.priced)
            out.basket_unpriced.emplace_back(rname(r));
    }
    std::sort(out.no_sink_produced.begin(), out.no_sink_produced.end());
    std::sort(out.no_sink_raws.begin(), out.no_sink_raws.end());
    std::sort(out.basket_unmakeable.begin(), out.basket_unmakeable.end());
    std::sort(out.basket_unpriced.begin(), out.basket_unpriced.end());

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
    std::printf("=== BAND %s (epoch %lld) ===\n", b.band.c_str(),
                static_cast<long long>(b.epoch));
    std::printf("  world     : %d tiles, %d markets, %d centres (scale sum %.0f), "
                "%d buildings (%d building), %d corps, %d units / %d heads\n",
                b.tiles, b.markets, b.centres, b.centre_scale, b.buildings,
                b.under_construction, b.corps, b.units, b.heads);
    std::printf("  registry  : %d of %d authored recipes allowed in band; max chain depth %d\n",
                b.recipes_allowed, b.recipes_authored, b.max_depth);

    std::printf("\n  --- per-resource demand census, census tick, summed over every market ---\n");
    std::printf("  %-3s %-22s %-4s %-4s | %10s %10s %10s %10s %10s | %11s | %10s | %10s | %s\n",
                "id", "resource", "prod", "d px",
                "household", "backgrnd", "interbody", "construct", "process",
                "MKT TOTAL", "upkeep/pl", "produced", "structural sinks");
    std::printf("  %-3s %-22s %-4s %-4s | %10s %10s %10s %10s %10s | %11s | %10s | %10s | %s\n",
                "---", "----------------------", "----", "----",
                "----------", "----------", "----------", "----------", "----------",
                "-----------", "----------", "----------", "----------------");

    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const classification& c = b.cls[r];
        const double total = b.household[r] + b.background[r] + b.interbody[r]
                           + b.construction[r] + b.processing[r];
        std::printf("  %-3zu %-22s %-4s %2d %-1s | %10.3f %10.3f %10.3f %10.3f %10.3f | %11.3f | "
                    "%10.3f | %10.1f | %s\n",
                    r, rname(r), prod_word(c), c.depth, c.priced ? "$" : "-",
                    b.household[r], b.background[r], b.interbody[r],
                    b.construction[r], b.processing[r], total,
                    b.upkeep_pool[r], b.produced[r], sink_word(c).c_str());
    }

    std::printf("  %-3s %-22s %-4s %-4s | %10.3f %10.3f %10.3f %10.3f %10.3f | %11.3f | "
                "%10.3f | %10.1f |\n",
                "", "TOTAL", "", "",
                total_of(b.household), total_of(b.background), total_of(b.interbody),
                total_of(b.construction), total_of(b.processing),
                total_of(b.household) + total_of(b.background) + total_of(b.interbody)
                    + total_of(b.construction) + total_of(b.processing),
                total_of(b.upkeep_pool), total_of(b.produced));
    std::printf("  %-3s %-22s %-4s %-4s | %10d %10d %10d %10d %10d | %11s | %10d | %10s |  "
                "(resources touched)\n",
                "", "BREADTH", "", "",
                touched_by(b.household), touched_by(b.background), touched_by(b.interbody),
                touched_by(b.construction), touched_by(b.processing), "",
                touched_by(b.upkeep_pool), "");

    if (b.wants_dropped_no_market > 0.0)
        std::printf("  note: %.3f units of want were registered on bodies carrying no market and "
                    "never reached a demand register.\n", b.wants_dropped_no_market);

    std::printf("\n  --- what this band cannot buy ---\n");
    std::printf("  produced in-band, NO market sink : %s\n", join(b.no_sink_produced).c_str());
    std::printf("  extractable, NO market sink      : %s\n", join(b.no_sink_raws).c_str());
    std::printf("  a basket names it, band cannot make it or dig it: %s\n",
                join(b.basket_unmakeable).c_str());
    std::printf("  a basket names it, NO market prices it (both injectors SKIP it silently): %s\n",
                join(b.basket_unpriced).c_str());

    int live = 0;
    if (total_of(b.household)    > 0.0) ++live;
    if (total_of(b.background)   > 0.0) ++live;
    if (total_of(b.interbody)    > 0.0) ++live;
    if (total_of(b.construction) > 0.0) ++live;
    if (total_of(b.processing)   > 0.0) ++live;
    std::printf("  live injecting passes this band  : %d of 5 measured\n", live);
}

} // namespace

int main(int argc, char** argv)
{
    uint32_t seed       = 0;
    int      warm_ticks = 80;
    bool     prehistory = true;
    std::string bands   = "both";

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

    std::printf("demand_census — BL-649, requirement group `demand-census` R1-R4\n");
    std::printf("  seed %u | warm ticks %d | prehistory %s | bands %s\n",
                seed, warm_ticks,
                prehistory ? "ON (the shipped spawn)" : "OFF (--fast, NOT the spawn)",
                bands.c_str());
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
        results.push_back(run_band("ancient", 0, seed, warm_ticks, prehistory, reg, gen_cfg));
    if (want_industrial)
        results.push_back(run_band("industrial", 1960, seed, warm_ticks, prehistory, reg, gen_cfg));

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
                                + total_of(b.processing);
        std::snprintf(msg, sizeof msg,
                      "%s: the instrument is non-vacuous — some pass injected some demand",
                      b.band.c_str());
        check(any_demand > 0.0, "R2", msg);
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
