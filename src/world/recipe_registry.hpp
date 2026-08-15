#pragma once

#include "components.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Forward-declared so this header carries no sol2/Lua dependency — the economy
// systems (world/*, SDL- and Lua-free) include it and stay headlessly buildable.
// Only recipe_registry.cpp pulls in the Lua state to populate the tables.
class lua_state;

/// BL-433: which product's roster an authored entry belongs to.
///
/// Two bands plus a wildcard, and deliberately NOT ERAS.md's Era 0 / Era 1
/// numbering — that axis is about space access *within* the industrial arc and
/// is gated on launchpad presence, a different question from "which product is
/// this". One field, one meaning.
///
/// `any` is the default and it is load-bearing: a registry whose band is never
/// set permits everything, so every headless harness — none of which knows about
/// eras — loads exactly the roster it loaded before this existed.
enum class era_band : uint8_t
{
    any        = 0, ///< Shared by both arcs. The default for an untagged entry.
    ancient    = 1, ///< The 0 CE product (world_params::epoch_year < 1700).
    industrial = 2, ///< The 1960 arc, including everything space-facing.
};

/// The band a campaign's epoch year belongs to. Uses the SAME 1700 threshold the
/// antiquity branch already documents on world_params::epoch_year, so the split
/// between the two arcs is one number in the codebase rather than two.
inline era_band era_band_for_epoch(int64_t epoch_year)
{
    return (epoch_year < 1700) ? era_band::ancient : era_band::industrial;
}

/// Does an entry authored for band @p entry appear in a campaign running @p campaign?
/// An `any` entry appears in every band; an `any` campaign (the unset default)
/// admits every entry.
inline bool era_permits(era_band campaign, era_band entry)
{
    return campaign == era_band::any || entry == era_band::any || entry == campaign;
}

/// A processing recipe: per-batch input and output quantities, indexed by
/// resource_type. Reagents are simply inputs with no matching output. Authored
/// in scripts/recipes.lua; the recipe's id is its index in recipe_registry::recipes.
struct recipe
{
    std::string                       name;
    std::array<float, resource_count> inputs  = {};
    std::array<float, resource_count> outputs = {};

    /// BL-433: the product this recipe belongs to. Authored as `era = "..."` in
    /// recipes.lua; absent means `any`.
    era_band era = era_band::any;

    /// BL-429: the building's player-facing NAME — "Bloomery", "Smithy" — as
    /// opposed to `name`, which stays the raw lookup key `recipe_id` indexes by
    /// and never changes once a save references it. Authored as
    /// `display_name = "..."` in recipes.lua; if absent, `load_from_lua` fills it
    /// with a title-cased `name` (the old Build-door behaviour), so every recipe
    /// always carries a legible label without requiring one to be authored.
    std::string display_name;
};

/// The primary output resource of a recipe — the argmax of its outputs. Used
/// to key a processing method's resource pip (construction_panel.cpp) and,
/// BL-429 slice 2, its Build-door / on-canvas glyph (`ui::icons::building`):
/// two recipes that reach the same good (Charcoal Burner and the Peat Kiln,
/// both -> charcoal) share one glyph by design, since the glyph identifies
/// WHAT a building makes, not which specific recipe. Shared here (not just in
/// construction_panel.cpp, its original home) so every UI file that draws a
/// processing building's icon reads the same identity.
inline resource_type primary_output_resource(const recipe& r)
{
    std::size_t best   = 0;
    float       best_v = -1.0f;
    for (std::size_t i = 0; i < resource_count; ++i)
        if (r.outputs[i] > best_v) { best_v = r.outputs[i]; best = i; }
    return static_cast<resource_type>(best);
}

/// Per-building-type economic constants, authored in scripts/economy.lua.
struct building_economics
{
    float base_rate   = 0.0f; ///< Extraction units/tick at richness 1, workforce 1; or processing batches/tick at workforce 1.
    float maintenance = 0.0f; ///< Flat per-tick upkeep charged to the owning corp.
    float base_wage   = 0.0f; ///< Wage per unit workforce per tick.
    float build_cost  = 0.0f; ///< One-off construction cost (Layer 4 build UI).
    /// Per-resource material cost of construction (BL-044). Indexed by
    /// static_cast<std::size_t>(resource_type). Bought from the tile's local
    /// market at its prevailing price, folded into the credit cost
    /// (construction.cpp). Zero = no requirement for that good.
    std::array<float, resource_count> resource_build_cost = {};

    /// Economy ticks the building spends under construction before it starts
    /// producing (playtest patch, 2026-07-06). 0 = instant (pre-existing
    /// behaviour), authored in scripts/economy.lua as `build_duration_ticks`.
    float build_duration_ticks = 0.0f;

    /// BL-433: the product this building type belongs to. Authored as
    /// `era = "..."` in economy.lua; absent means `any`. Unlike recipes, a
    /// building type is addressed by its enum value rather than by position, so
    /// gating it needs no mask — `recipe_registry::building_available` reads
    /// this directly.
    era_band era = era_band::any;
};

/// BL-365 population-growth-gate tunables, authored in scripts/economy.lua under
/// `economy.population_growth`. Read only by the population-growth step in
/// run_economy_step (economy_system.cpp) to test whether a centre's basket is
/// broadly met before it levels up. This is the surviving remnant of the old
/// BL-078 elastic nation-substrate model — that model's demand/supply INJECTION
/// (capacity, clearing_fraction, elasticity) was deleted by BL-365, which
/// replaced the abstract substrate with real background corporations
/// (corporation_generation.cpp's generate_background_firms); the growth gate
/// still needs *some* basket + threshold to test consumption against, so those
/// two fields alone survive under a new name.
struct growth_params
{
    /// Per-capita basket weight per resource, used ONLY to weight the met-supply
    /// ratio the growth gate reads. Indexed by static_cast<std::size_t>(resource_type).
    std::array<float, resource_count> demand_basket = {};
    float growth_met_threshold = 0.50f; ///< basket met-supply ratio a centre needs to grow.
};

/// BL-263 spontaneous-market-emergence tunables, authored in scripts/economy.lua
/// under `economy.market_emergence`. Two independent uses, both keyed off distance
/// from the home body (world::home_body), the only body with a market at world
/// generation: seeding a newly-spawned outpost market's opening prices
/// (maybe_spawn_market), and pulling a discounted slice of the home body's own
/// unmet demand onto every outpost market each tick so an outpost with supply and
/// no local population does not collapse to the price floor
/// (inject_interbody_demand). See docs/economy/MARKETS.md § Spontaneous market
/// emergence.
struct market_emergence_params
{
    /// Opening-price markup per AU of distance from the home body: a spawned
    /// market's base_price[r] = home_market.base_price[r] * (1 + price_distance_gain
    /// * distance_au) — haul cost makes goods dearer the further they must travel,
    /// mirroring BL-191's endemic-good distance pricing but keyed to a real
    /// per-resource base price rather than a flat source price.
    float price_distance_gain = 0.08f;
    /// Fraction of the home body's own unmet demand (demand - supply, when
    /// positive) pulled onto each outpost market per tick, before the distance
    /// falloff below.
    float pull_fraction = 0.50f;
    /// Falloff denominator per AU: pulled_demand = home_shortfall * pull_fraction
    /// / (1 + distance_falloff * distance_au). Further outposts feel less of the
    /// home body's pull — plausible if a haul is expensive, and it keeps a distant
    /// outpost's price from being yanked to Kepler's exactly.
    float distance_falloff = 0.15f;
};

/// BL-368 population-demand model tunables, authored in scripts/economy.lua under
/// `economy.population_demand`. Applied at tick time by inject_population_demand:
/// each population centre pulls DEMAND = pcc.scale × basket[r] × elasticity(price)
/// from its catchment market for every resource in the basket — no supply term;
/// population is a pure consumer here. Generalises the BL-190
/// agricultural_produce-only stub into a real per-centre basket across the
/// tradeable set. See docs/economy/MARKETS.md and docs/economy/POPULATION.md
/// for the full model.
struct population_demand_params
{
    /// Per-scale-point demand weight per resource. Indexed by
    /// static_cast<std::size_t>(resource_type). Unlisted resources get 0.
    std::array<float, resource_count> demand_basket = {};
    float demand_elasticity = 0.80f; ///< exponent on (base_price / price).
    float elasticity_min    = 0.30f; ///< clamp lo on the elasticity factor.
    float elasticity_max    = 2.50f; ///< clamp hi on the elasticity factor.
    float demand_scale      = 1.00f; ///< global population → demand scale.
};

/// BL-340/BL-365 background-industrial-demand tunables, authored in
/// scripts/economy.lua under `economy.background_demand`. A world-scale pull
/// (not per-centre, unlike population_demand_params above) representing the
/// aggregate offstage economy's appetite for mid-chain processing goods —
/// silicon, refined_copper, ree_alloy, machinery, alloys, electronics.
/// Deliberately excludes spacecraft_components: BL-340's design keeps the
/// militia's procurement contracts as that good's only buyer. Applied at tick
/// time by inject_background_demand (market_clearing.cpp), called from
/// clear_markets alongside inject_population_demand. Same price-elastic shape;
/// scaled per market by that market's body's total population scale.
struct background_demand_params
{
    /// Per-population-scale-point demand weight per resource. Indexed by
    /// static_cast<std::size_t>(resource_type). Unlisted resources get 0.
    std::array<float, resource_count> demand_basket = {};
    float demand_elasticity = 0.80f; ///< exponent on (base_price / price).
    float elasticity_min    = 0.30f; ///< clamp lo on the elasticity factor.
    float elasticity_max    = 2.50f; ///< clamp hi on the elasticity factor.
    float demand_scale      = 1.00f; ///< global scale → demand scale.
};

/// BL-095 construction-gate tunables, authored in scripts/economy.lua under
/// `economy.construction`. Read by run_economy_step's construction step, which
/// paces each build against the local market's recent supply of its materials.
struct construction_params
{
    float max_stretch = 10.0f; ///< longest a material-starved build stretches to (×base duration); below 1/max_stretch it pauses.

    /// BL-323 S2: furthest a new building may sit from its nearest supply anchor,
    /// in `logistics.hpp`'s weighted cost units. Negative disables the rule.
    ///
    /// DEFAULTS OFF, unlike its siblings above, and deliberately: reach is only
    /// meaningful against a GENERATED world, where cities exist to anchor supply.
    /// A hand-built harness world has no population centres, so every tile on it
    /// is infinitely far from an anchor and the rule would refuse every placement
    /// — turning a construction test into a reach test by accident. scripts/
    /// economy.lua authors the real value (24.0), so the shipped game enforces it;
    /// a harness that wants the rule passes a budget explicitly, as
    /// tools/verify/logistics_reach_harness.cpp does.
    float max_logistics_reach = -1.0f;

    // BL-323 S3 — build time depends on the site, not just the type. Three
    // multipliers on the base build_duration_ticks, applied once at placement
    // (construct_building): landform reuses logistics.hpp's own cost function
    // (no second terrain table); reach and stack are new knobs below.

    /// Extra time at the FURTHEST placeable reach (max_logistics_reach), as a
    /// fraction of base duration — linear from 0 at reach 0 to this at reach ==
    /// max_logistics_reach. E.g. 1.0 means a site right at the budget's edge
    /// takes 2x base; a site at an anchor takes 1x. Ignored (treated as 0) when
    /// max_logistics_reach is disabled (< 0), since there is then no scale to
    /// normalise against.
    float site_time_reach_scale = 1.0f;

    /// Per-existing-building discount on a tile that already carries a site of
    /// the SAME building type — an established stack builds faster, one less
    /// multiplier step per prior building. E.g. 0.15 means the second building
    /// on a tile takes 0.85x, the third 0.70x, floored at site_time_stack_min.
    float site_time_stack_discount = 0.15f;

    /// Floor on the stack discount, so an old, heavily-stacked site never
    /// reaches implausibly-instant (or zero/negative) build time.
    float site_time_stack_min = 0.5f;
};

/// BL-430 player-facing recipe-switch tunables, authored in scripts/economy.lua under
/// `economy.recipe_switch`. Read by the seam that changes a processing_facility's
/// active recipe on the PLAYER path (corp_command's set_recipe verb, and the
/// construction_panel management UI, which shares its gate). Deliberately NOT read
/// by the BL-079 reflex switch (economy_system.cpp's floored-recipe auto-agency) —
/// that stays free/instant, since it is sanctioned auto-agency reacting to a loss,
/// not a player (or the AI's dial_recipe margin-chase, which goes through the same
/// seam and so pays the same cost) committing to a new method. See PRODUCTION.md.
struct recipe_switch_params
{
    /// One-off credit cost debited from the corp on a successful switch. 0 = free
    /// (pre-BL-430 behaviour), the default so a hand-built harness registry that
    /// never authors this table switches recipes for free, same as before.
    float switch_cost = 0.0f;

    /// Economy ticks after a switch before the SAME building may switch again
    /// through this seam. 0 = no cooldown (pre-BL-430 behaviour).
    int cooldown_ticks = 0;
};

/// BL-148/149 logistics-node discount tunables, authored in scripts/economy.lua under
/// the top-level `logistics.node_discount`. A convoy's intra-body haul cost is discounted
/// for each population-centre (BL-148) and inland_logistics_hub (BL-149) tile its A* path
/// crosses, so the world's cities and the player's hubs form a cheap logistics network the
/// player plugs into. See docs/economy/SUPPLY.md (on landing).
struct logistics_node_params
{
    float city_discount_per_scale = 0.04f; ///< discount fraction per population-centre `scale` point (1–5) on the path.
    float hub_discount            = 0.12f; ///< flat discount fraction per inland_logistics_hub tile on the path.
    float discount_cap            = 0.50f; ///< ceiling on the summed node discount (fraction of the haul cost).
};

/// BL-332 capability-point accumulation rates, authored in scripts/economy.lua under the
/// top-level `military` table. A flat per-tick credit to the owning corp's
/// corporation_component.military_points / .science for every COMPLETED military_base /
/// research_institute it owns — see economy_system.cpp's capability-points pass.
struct military_capability_params
{
    float military_points_per_base_tick       = 1.0f;
    float science_per_research_institute_tick = 1.0f;

    /// BL-394: hire_unit's credit cost, debited from the corp's balance at
    /// apply time — cost = hire_base_cost + hire_cost_per_power × the roster
    /// row's power_mod. The base is a FLOOR: row 0's gate is all-zero, so the
    /// resource debit alone made the cheapest rows free and hiring unbounded
    /// for any seam caller. Defaults match economy.lua so a hand-built harness
    /// registry charges hires sensibly without Lua.
    float hire_base_cost      = 40.0f;
    float hire_cost_per_power = 0.5f;
};

/// BL-350 procurement/contract tunables, authored in scripts/economy.lua under the top-level
/// `procurement` table.
struct procurement_params
{
    /// Fraction of the contract's total price (quantity x unit_price) debited at
    /// accept_quote; the remainder is drawn evenly across lead_time_ticks. First
    /// cut 0.25 — see BL-350's Q1 (both alternatives, on-delivery-only and
    /// on-order-only, fail concretely; the design's own reasoning is in the item).
    float deposit_fraction = 0.25f;

    /// Base lead time, ticks — multiplied by ceil(quantity / supplier_throughput)
    /// to get the quote's lead_time_ticks.
    float base_lead_ticks = 2.0f;

    /// (buyer, supplier) reputation floor below which request_quote declines
    /// outright (BL-350 Q2's fourth condition). Reputation starts at 0 for an
    /// unseen pair, so a floor at or below 0 never gates a first-ever quote.
    float reputation_floor = -5.0f;

    /// Reputation delta on a contract's completion (+) / cancellation (-).
    float reputation_on_complete = 1.0f;
    float reputation_on_cancel   = -2.0f;
};

/// Player road-placement cost for a single tier (BL-147 core, BL-172 ladder), authored in
/// scripts/economy.lua under `economy.roads.{track,road,highway}`. A placed road tile costs a
/// flat credit sum plus per-resource materials bought from the local market — the same cost
/// shape as building construction, but paid up front (roads are instant, per-tile, not
/// durative). The registry holds one of these per tier; see `recipe_registry::road_econ(tier)`.
/// See docs/economy/SUPPLY.md / docs/ui/PLANETARY.md.
struct road_economics
{
    float build_cost = 40.0f; ///< flat credit cost of a road tile of this tier.
    /// Per-resource material cost, indexed by static_cast<std::size_t>(resource_type); bought
    /// from the tile's local market at its prevailing price. Zero = no requirement.
    std::array<float, resource_count> resource_build_cost = {};
};

/// Startup-loaded registry of processing recipes and economy constants. Pure
/// data once built; constructed either from Lua (load_from_lua) in the real
/// build or by hand in a headless test harness.
class recipe_registry
{
public:
    /// Populate the registry from scripts/recipes.lua and scripts/economy.lua via
    /// the embedded Lua state (protected calls only).
    ///
    /// @param lua A loaded lua_state.
    /// @throws std::runtime_error on a Lua error or malformed data.
    void load_from_lua(lua_state& lua);

    /// Look up a recipe by its id (index). Returns nullptr for `no_recipe` or any
    /// out-of-range id.
    const recipe* get_recipe(uint16_t id) const
    {
        if (id == no_recipe || id >= m_recipes.size())
            return nullptr;
        return &m_recipes[id];
    }

    /// Recipe id for a recipe name, or `no_recipe` if none matches. Used at
    /// placement to author building_component.recipe from a stable name. Inline
    /// (pure data) so player-construction logic stays headless-buildable without
    /// linking the Lua-bound translation unit.
    uint16_t recipe_id(const std::string& name) const
    {
        for (std::size_t i = 0; i < m_recipes.size(); ++i)
            if (m_recipes[i].name == name)
                return static_cast<uint16_t>(i);
        return no_recipe;
    }

    /// BL-429: the recipe an unconfigured processing facility should default to —
    /// the first recipe the CURRENT ERA allows, as an absolute id.
    ///
    /// This exists because the obvious spelling, `recipe_id("steel")`, became a
    /// trap the moment BL-429 tagged the coal-fired steel route `industrial`: it
    /// still resolves (the storage path is band-independent, by design), so an
    /// ancient campaign would silently seed its processors with a recipe that is
    /// not part of its economy — and nothing would refuse it, since the era gates
    /// browsing and placement, not execution. Callers that want "a sensible
    /// default" must ask for one, not name a recipe and hope.
    ///
    /// Returns `no_recipe` for an empty or fully-masked roster.
    uint16_t default_recipe_id() const
    {
        if (m_allowed.empty())
            return no_recipe;
        return static_cast<uint16_t>(m_allowed.front());
    }

    /// Economy constants for a building type.
    const building_economics& economics(building_type type) const
    {
        return m_building_econ[static_cast<std::size_t>(type)];
    }

    float t_full() const { return m_t_full; }
    float t_idle() const { return m_t_idle; }

    /// BL-365 population-growth-gate tunables (economy.population_growth in Lua).
    const growth_params& growth() const { return m_growth; }

    /// BL-368 population-demand model tunables (economy.population_demand in Lua).
    const population_demand_params& population_demand() const { return m_population_demand; }

    /// BL-340/BL-365 background-industrial-demand tunables (economy.background_demand in Lua).
    const background_demand_params& background_demand() const { return m_background_demand; }

    /// BL-263 spontaneous-market-emergence tunables (economy.market_emergence in Lua).
    const market_emergence_params& market_emergence() const { return m_market_emergence; }

    /// BL-095 construction-gate tunables (economy.construction in Lua).
    const construction_params& construction() const { return m_construction; }

    /// BL-332 capability-point accumulation rates (economy.military in Lua).
    const military_capability_params& military() const { return m_military; }

    /// BL-350 procurement/contract tunables (economy.procurement in Lua).
    const procurement_params& procurement() const { return m_procurement; }

    /// BL-430 player-facing recipe-switch cost/cooldown (economy.recipe_switch in Lua).
    const recipe_switch_params& recipe_switch() const { return m_recipe_switch; }

    /// Base logistics cost per unit distance per unit cargo for the given convoy mode.
    float logistics_cost(convoy_mode m) const
    {
        return m_logistics_costs[static_cast<std::size_t>(m)];
    }

    /// BL-148/149 logistics-node discount tunables (logistics.node_discount in Lua).
    const logistics_node_params& logistics_nodes() const { return m_logistics_nodes; }

    /// Player road-placement cost for a tier (BL-172): 1=Track, 2=Road, 3=Highway; clamped to
    /// [1,3]. Authored in economy.roads.{track,road,highway}. Default arg keeps BL-147 callers
    /// (Track) unchanged.
    const road_economics& road_econ(std::uint8_t tier = 1) const
    {
        const std::size_t i = (tier < 1u ? 1u : (tier > 3u ? 3u : tier)) - 1u;
        return m_road_econ[i];
    }

    /// Every AUTHORED recipe, era-filtered or not. This is the storage-side count
    /// (ids index into it); `recipe_count(building_type)` below is the browse-side
    /// count and respects the era.
    std::size_t recipe_count() const { return m_recipes.size(); }

    // --- BL-433 era band ------------------------------------------------------
    //
    // THE SPLIT THAT MAKES THIS SAFE. A recipe's id is its index in m_recipes and
    // that id is STORED in building_component.recipe, so the era filter masks
    // rather than removes — dropping a disallowed recipe from the vector would
    // silently repoint every building whose recipe sat after it.
    //
    //   * STORAGE path — get_recipe(id) / recipe_id(name). Absolute, band-independent.
    //     A building's stored id means the same recipe in every band.
    //   * BROWSE path  — recipe_count(bt) / recipe_at(bt, i). Maps through m_allowed,
    //     so a caller walking [0, recipe_count) walks THIS era's recipes.
    //
    // Every existing caller of the browse path already meant "the recipes available
    // to me" by it, which is why none of them needed rewriting when this landed.

    /// The campaign's band. `any` (the default) permits every authored entry.
    era_band era() const { return m_era; }

    // --- BL-428 chain depth ---------------------------------------------------
    //
    // THE GROWTH SPINE (Ben, 2026-08-15, chosen over building tiers / a tech
    // ladder / settlement scale). How far down the production graph a corp can
    // reach is what gates its next building — so progress is a consequence of the
    // economy it has built, not a parallel unlock system laid over it. The
    // decisive argument was that every alternative is a SECOND system that must be
    // kept in agreement with the economy; depth is read off the recipe graph that
    // has to exist anyway.
    //
    // DEFINITION, and the two different composition rules it needs:
    //
    //   depth(raw)  = 0            — a good no recipe produces (mined, grown, felled)
    //   depth(good) = min over the recipes that produce it of
    //                   ( 1 + max over that recipe's inputs of depth(input) )
    //
    // MAX within a recipe: you cannot run it until your deepest input exists, so
    // the recipe's difficulty is its hardest input. MIN across recipes: if two
    // routes make the same good, you have reached it as soon as the EASIER route
    // is open. That asymmetry starts mattering the moment BL-430 lands alternate
    // production methods, so it is settled here rather than discovered there.
    //
    // Depth is computed over the ERA-ALLOWED recipes only. A route filtered out by
    // BL-433 does not exist for that campaign, so it must not shorten anything.
    //
    // -1 means UNREACHABLE: no sequence of allowed recipes bottoms out in raws.
    // That covers a cycle (A needs B needs A never bottoms out) and an input
    // orphan (needs a good nothing produces or extracts) with one code, because
    // from the player's side they are the same fact — you cannot get there.

    /// Chain depth of a resource under the current era band, or -1 if unreachable.
    int depth_of(resource_type r) const
    {
        return m_depth[static_cast<std::size_t>(r)];
    }

    /// A good no allowed recipe produces — the graph's floor. Note this is a
    /// statement about the RECIPE graph only: whether a raw is actually extractable
    /// anywhere is a deposit question, and belongs to BL-432's roster audit.
    bool is_raw(resource_type r) const
    {
        return m_depth[static_cast<std::size_t>(r)] == 0;
    }

    /// The deepest reachable good under the current band — the ceiling a corp can
    /// climb to. Useful to a UI readout and as an anti-vacuity bound in tests.
    int max_depth() const
    {
        int best = 0;
        for (const int d : m_depth)
            best = (d > best) ? d : best;
        return best;
    }

    /// Set the campaign's band and rebuild the browse mask. Idempotent; safe to
    /// call again after load_from_lua (which resets the band to `any`).
    void set_era(era_band e)
    {
        m_era = e;
        rebuild_allowed();
    }

    /// Is this building type part of the current era's roster? Types are addressed
    /// by enum value, not by position, so this needs no mask — placement and the
    /// Build door filter on it.
    bool building_available(building_type bt) const
    {
        return era_permits(m_era, economics(bt).era);
    }

    /// Returns the number of recipes available IN THE CURRENT ERA for the given
    /// building type. Only processing_facility has recipes; all other types return
    /// 0. Inline (pure data, no Lua) so the SDL/Lua-free world superset — and the
    /// headless harnesses that exclude recipe_registry.cpp — link without it
    /// (BL-079 uses this).
    int recipe_count(building_type bt) const
    {
        if (bt != building_type::processing_facility)
            return 0;
        return static_cast<int>(m_allowed.size());
    }

    /// Returns the @p i-th recipe available in the current era for building type
    /// @p bt. The index is clamped to [0, recipe_count(bt) - 1]; returns a dummy
    /// empty recipe if the type has no recipes. Inline for the same headless-link
    /// reason as above.
    ///
    /// NOTE the index is a position in the ERA'S list, not a recipe id. To store
    /// the result, go through `recipe_id(r.name)` — as every existing caller does.
    const recipe& recipe_at(building_type bt, int i) const
    {
        static const recipe empty{};
        const int n = recipe_count(bt);
        if (n == 0)
            return empty;
        const int clamped = (i < 0) ? 0 : (i >= n ? n - 1 : i);
        return m_recipes[m_allowed[static_cast<std::size_t>(clamped)]];
    }

    // --- direct construction for tests (headless harness builds these by hand) ---
    void set_thresholds(float t_full, float t_idle) { m_t_full = t_full; m_t_idle = t_idle; }
    void set_growth(const growth_params& s) { m_growth = s; }
    void set_population_demand(const population_demand_params& p) { m_population_demand = p; }
    void set_background_demand(const background_demand_params& b) { m_background_demand = b; }
    void set_market_emergence(const market_emergence_params& m) { m_market_emergence = m; }
    void set_construction(const construction_params& c) { m_construction = c; }
    void set_military(const military_capability_params& m) { m_military = m; }
    void set_procurement(const procurement_params& p) { m_procurement = p; }
    void set_recipe_switch(const recipe_switch_params& p) { m_recipe_switch = p; }
    void set_economics(building_type type, const building_economics& e)
    {
        m_building_econ[static_cast<std::size_t>(type)] = e;
    }
    void set_logistics_cost(convoy_mode m, float v)
    {
        m_logistics_costs[static_cast<std::size_t>(m)] = v;
    }
    void set_logistics_nodes(const logistics_node_params& p) { m_logistics_nodes = p; }
    void set_road_econ(std::uint8_t tier, const road_economics& r)
    {
        const std::size_t i = (tier < 1u ? 1u : (tier > 3u ? 3u : tier)) - 1u;
        m_road_econ[i] = r;
    }
    uint16_t add_recipe(const recipe& r)
    {
        m_recipes.push_back(r);
        rebuild_allowed();
        return static_cast<uint16_t>(m_recipes.size() - 1);
    }

private:
    /// Recompute the browse mask from the authored recipes and the current band.
    /// Walks m_recipes in authored order, so the mask — and therefore every
    /// browse-order-dependent decision downstream of it — is a pure function of
    /// authored data, not of any container's iteration order.
    void rebuild_allowed()
    {
        m_allowed.clear();
        for (std::size_t i = 0; i < m_recipes.size(); ++i)
            if (era_permits(m_era, m_recipes[i].era))
                m_allowed.push_back(i);
        rebuild_depth();
    }

    /// BL-428: recompute chain depth over the era-allowed recipes.
    ///
    /// A FIXED POINT rather than a graph walk, and deliberately. It terminates
    /// obviously (a bounded loop), it is independent of any traversal or container
    /// order — the BL-406 lesson, where an unordered container decided a number the
    /// economy read — and it makes "cyclic" and "input-orphaned" the same
    /// observable outcome (a good that never settles), which is what they are from
    /// the player's side. No recursion, so no stack depth to reason about.
    ///
    /// Bound: `resource_count` passes. The longest simple chain cannot exceed the
    /// number of distinct goods, so anything still unsettled after that many
    /// relaxations never will be.
    void rebuild_depth()
    {
        constexpr int unreachable = -1;

        // Seed: a good produced by no allowed recipe is a raw at depth 0;
        // everything else starts unreachable and must earn a depth.
        for (std::size_t r = 0; r < resource_count; ++r)
            m_depth[r] = 0;
        for (const std::size_t i : m_allowed)
            for (std::size_t r = 0; r < resource_count; ++r)
                if (m_recipes[i].outputs[r] > 0.0f)
                    m_depth[r] = unreachable;

        for (std::size_t pass = 0; pass < resource_count; ++pass)
        {
            bool changed = false;
            for (const std::size_t i : m_allowed) // authored order — see above
            {
                const recipe& rc = m_recipes[i];

                // The recipe is runnable only once every input has settled; its
                // cost is then its DEEPEST input (max within a recipe).
                int deepest_input = 0;
                bool inputs_ready = true;
                for (std::size_t r = 0; r < resource_count && inputs_ready; ++r)
                {
                    if (rc.inputs[r] <= 0.0f)
                        continue;
                    const int d = m_depth[r];
                    if (d == unreachable)
                        inputs_ready = false;
                    else if (d > deepest_input)
                        deepest_input = d;
                }
                if (!inputs_ready)
                    continue;

                const int cand = deepest_input + 1;
                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    if (rc.outputs[r] <= 0.0f)
                        continue;
                    // MIN across recipes: the easiest route is the one that decides
                    // when you have reached this good.
                    if (m_depth[r] == unreachable || cand < m_depth[r])
                    {
                        m_depth[r] = cand;
                        changed    = true;
                    }
                }
            }
            if (!changed)
                break; // settled
        }
    }

    std::vector<recipe> m_recipes;

    /// BL-433 browse mask: positions in m_recipes permitted by m_era, in authored
    /// order. Rebuilt by set_era / add_recipe / load_from_lua.
    std::vector<std::size_t> m_allowed;

    /// The campaign's band. `any` = unset = permit everything, which is what keeps
    /// every pre-BL-433 harness and the default world byte-identical.
    era_band m_era = era_band::any;

    /// BL-428 chain depth per resource under the current band; -1 = unreachable.
    /// Rebuilt with the mask, since a route the era filters out must not shorten
    /// anything. Every entry is 0 until the first rebuild — a registry with no
    /// recipes is all raws, which is the truthful answer for an empty graph.
    std::array<int, resource_count> m_depth = {};

    /// Indexed by building_type (none / extraction_site / processing_facility / port /
    /// launchpad / inland_logistics_hub / military_base / research_institute — BL-332
    /// bumped the count 7 → 8).
    std::array<building_economics, 8> m_building_econ = {};

    float m_t_full = 1.0f;
    float m_t_idle = 0.2f;

    /// BL-078 elastic-substrate model tunables (economy.substrate). Defaults match
    /// economy.lua so a hand-built harness registry behaves sensibly without Lua.
    growth_params m_growth = {};
    population_demand_params m_population_demand = {};
    background_demand_params m_background_demand = {};
    market_emergence_params m_market_emergence = {};

    /// BL-095 construction-gate tunables (economy.construction). Defaults match
    /// economy.lua so a hand-built harness registry paces builds sensibly.
    construction_params m_construction = {};

    /// BL-332 capability-point rates. Defaults match economy.lua so a
    /// hand-built harness registry behaves sensibly without Lua.
    military_capability_params m_military = {};

    /// BL-350 procurement tunables. Defaults match economy.lua.
    procurement_params m_procurement = {};

    /// BL-430 recipe-switch cost/cooldown. Defaults to free/instant so a
    /// hand-built harness registry that never sets this behaves as it always did.
    recipe_switch_params m_recipe_switch = {};

    /// Logistics base cost per unit distance per unit cargo, indexed by convoy_mode
    /// (land=0, sea=1, air=2, space=3). Defaults match economy.lua values.
    std::array<float, 4> m_logistics_costs = { 0.02f, 0.05f, 0.15f, 1.00f };

    /// BL-148/149 node-discount tunables (logistics.node_discount). Defaults match economy.lua
    /// so a hand-built harness registry discounts city/hub routes sensibly without Lua.
    logistics_node_params m_logistics_nodes = {};

    /// Road-placement cost per tier (BL-172): index 0..2 = Track/Road/Highway (road_level 1/2/3).
    /// Credit defaults are used by the Lua-free harnesses; the material line is seeded from Lua
    /// (a harness that skips Lua pays credits only).
    std::array<road_economics, 3> m_road_econ = { {
        road_economics{ 25.0f, {} }, // Track   (road_level 1)
        road_economics{ 45.0f, {} }, // Road    (road_level 2)
        road_economics{ 90.0f, {} }, // Highway (road_level 3)
    } };
};
