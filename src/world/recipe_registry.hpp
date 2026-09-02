#pragma once

#include "components.hpp"
#include "nation_ai.hpp"   // BL-542: nation_ai_params — the nation scorer's tunables (light: entity.hpp + nation_budget.hpp only)
#include "sentiment.hpp"   // BL-545/BL-546: sentiment_params — the authored factor table
#include "space_programme.hpp" // BL-644: space_programme_params — the tenth budget line's purchase lumps
#include "unit_roster.hpp" // BL-454: unit_upkeep_params — per-type unit data lives with the roster

#include <array>
#include <cstdint>
#include <map>
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

/// One past the last band — the size of any per-band table. Derived from the
/// enum's tail, the same way `resource_count` and `building_type_count` derive
/// from theirs: appending a band means moving this with it.
inline constexpr std::size_t era_band_count =
    static_cast<std::size_t>(era_band::industrial) + 1;

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

/// BL-640: one era-banded tranche of a demand basket.
///
/// Authored as a row in `economy.population_demand.baskets` /
/// `economy.background_demand.baskets`, each row carrying the SAME `era = "..."`
/// field a recipe carries (BL-433) and read through the SAME `read_era` reader —
/// so an unknown band string is a load-time error here exactly as it is there,
/// never a silent fallback. This is deliberately not a parallel mechanism: the
/// mask below is `era_permits`, the same predicate the recipe browse mask uses.
///
/// Why the baskets needed one at all (MARKETS.md § Demand channels, property 2):
/// recipes have been banded since BL-433 but the demand baskets never were, so
/// a basket authored in industrial goods left an ancient campaign wanting things
/// nothing in that band can make. Era decides WHICH value chain a household
/// consumes; the stratum ladder (POPULATION.md § Population demand) decides how
/// far up it — the two compose, and this field is the first half.
struct era_basket
{
    era_band                          era           = era_band::any;
    std::array<float, resource_count> demand_basket = {};
};

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

    /// BL-434: which sub-facility KIND this recipe belongs to — "Metal Foundry",
    /// "Food Processing", etc. — so the generic processing_facility building type
    /// can be presented (Build door) and gated (recipe-switch cost) as if it were
    /// several distinct facility kinds, without adding new building_type enum
    /// values. Authored as `group = "..."` in recipes.lua, same optional-field
    /// pattern as `era`/`display_name`; absent means the literal "General" — a
    /// real, if catch-all, group name (not empty string) so every recipe always
    /// has SOME group to compare against in the cross-group-switch cost check
    /// (economy_system.cpp's try_switch_recipe) without a special empty-string case.
    std::string group = "General";

    /// BL-615: per-recipe stratum-gate radius — this NAMED processing building
    /// must stand within this grid distance of a population centre (the heavy
    /// processor class: steel-mill-like recipes). 0 (the default) = ungated.
    /// Per-recipe rather than per-type because a recipe IS the named-building
    /// identity for processing (the BL-590 override precedent): a steel mill
    /// needs a workforce pool nearby, a miller does not, and both are the one
    /// processing_facility type. Overrides the type-level gate's radius when
    /// non-zero — see `placement_gate_for`.
    int centre_proximity_radius = 0;
    /// BL-613 (qualification fraction; docs/economy/POPULATION.md
    /// § Qualification): the fraction of this recipe's labour that must be
    /// QUALIFIED, in [0, 1]. 0 (the default, and the value for every
    /// hand-built harness recipe) means the recipe runs on ordinary labour
    /// alone and the qualified pool never gates it. Above 0, a building
    /// running this recipe throttles against its host nation's qualified pool
    /// (nation_component::qualification × the nation's share of the body's
    /// labour supply) exactly like the ordinary contention scalar — a factor,
    /// not a new shape (economy_system.cpp's workforce pass). Authored as
    /// `qualified_workforce = ...` in recipes.lua on the deepest / latest
    /// methods only; validated at load as a finite rate in [0, 1] and
    /// REJECTED otherwise, never clamped.
    float qualified_workforce = 0.0f;
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

    /// BL-436: how a deposit's richness converts into an extraction RATE
    /// multiplier. Extraction only; ignored for every other building type.
    ///
    /// `base_rate` above has always documented itself as "units/tick AT RICHNESS
    /// 1", but `tile_component::resource_deposit` is generated as a quantity —
    /// measured mean 53.3, max 72,321 — and was fed in raw. A mine therefore ran
    /// at ~1067 units/tick against a processor's flat 8: a 133:1 structural gap
    /// that no price or wage could offset, and the reason refining could not pay.
    ///
    /// The fix restores the documented contract rather than inventing a new one.
    /// Richness is divided by `richness_reference` and clamped to
    /// [`richness_min`, `richness_max`], so a typical deposit lands near 1.0 and
    /// a mine runs at roughly its authored base_rate.
    ///
    /// The DESIGN statement behind the clamp: richness should decide how LONG a
    /// deposit lasts, not how fast you can pull it out. Reserve still scales with
    /// raw richness (`resource_remaining`, seeded at generation), so a rich tile
    /// is still worth far more over its life — it simply cannot also run a
    /// thousand times faster. A reference of 0 disables the conversion entirely
    /// and restores the raw pre-BL-436 behaviour.
    float richness_reference = 0.0f;
    float richness_min       = 0.25f;
    float richness_max       = 2.0f;

    /// BL-433: the product this building type belongs to. Authored as
    /// `era = "..."` in economy.lua; absent means `any`. Unlike recipes, a
    /// building type is addressed by its enum value rather than by position, so
    /// gating it needs no mask — `recipe_registry::building_available` reads
    /// this directly.
    era_band era = era_band::any;

    /// BL-615: this type's stratum placement gate (components.hpp holds the
    /// struct and its contract; POPULATION.md § Strata gate buildings the
    /// design). Authored per building in scripts/economy.lua
    /// (`requires_centre` / `min_centre_scale` / `centre_proximity_radius`);
    /// the default gates nothing, so an unauthored type — and every
    /// hand-built harness registry — places exactly as before.
    placement_gate gate = {};
};

/// Convert a raw deposit richness into the extraction RATE multiplier, per the
/// contract on `building_economics::richness_reference`. A free function rather
/// than a member so every preview path — building_profit's expected-output
/// estimate, the AI scorer's candidate valuation — applies exactly the same
/// conversion as the live tick. A preview that disagreed with the tick would
/// have the AI and the Build door pricing a site the economy will not deliver.
inline float richness_rate_scalar(const building_economics& e, float richness)
{
    if (e.richness_reference <= 0.0f)
        return richness; // conversion disabled: raw, pre-BL-436 behaviour
    const float s = richness / e.richness_reference;
    return (s < e.richness_min) ? e.richness_min
         : (s > e.richness_max) ? e.richness_max
                                : s;
}

// ---------------------------------------------------------------------------
// BL-641 — building upkeep in goods
// ---------------------------------------------------------------------------
//
// A unit pays upkeep in credits AND a goods vector (`unit_upkeep_params`); a
// building paid credits only. That asymmetry was an omission, not a design, and
// it is the largest single reason the goods roster has more producers than
// consumers — nothing in the world consumed on a scale that GROWS with the
// world. Closing it makes every firm a consumer, so demand scales with how much
// industry exists rather than with a weight somebody has to maintain
// (docs/economy/MARKETS.md § Demand channels, the Industry channel; the design
// is FINANCE.md § Upkeep is credits AND goods — for buildings too).
//
// The shape is NOT new. It is § Standing-force upkeep's, applied to the other
// kind of asset: a per-type goods vector drawn from the owner's pool on the
// asset's own body, in a fixed order, with an unmet draw WEAKENING the asset by
// the same subtraction rather than destroying it.
//
// WHY THE RATES LIVE HERE AND NOT IN `building_economics`. The credit half
// (maintenance, base_wage) is per-type and nothing else; the goods half is per
// type AND per era band, because an ancient workshop runs on tools and planks
// while an industrial one runs on machinery and electronics. One table keyed by
// both, plus the one decay curve that governs every type, reads as one feature
// rather than as a field scattered across nine building entries.

/// Per-tick building upkeep rates, authored in scripts/economy.lua under
/// `economy.building_upkeep`. Reached through `recipe_registry::building_upkeep()`.
///
/// EVERY RATE DEFAULTS TO ZERO, and that is the BL-454 precedent restated: the
/// shape lands INERT. With the goods table at zero no pool is touched (not even
/// created), no draw can go unmet, and no building's supply factor ever moves —
/// so the economy is bit-identical to the pre-BL-641 build and turning the sink
/// on is a DATA change rather than a code change. A zero entry is skipped
/// exactly as an absent one is.
struct building_upkeep_params
{
    /// Goods drawn per COMPLETED, non-decommissioned building per tick, keyed
    /// [building_type][era_band] and resource-indexed. Authored as ordinary
    /// `{resource_name = qty}` tables so naming a good is a one-line economy.lua
    /// change and never a code change.
    ///
    /// THE BAND RULE, and it is `era_permits` restated for a vector: a basket
    /// authored under `any` applies in every campaign, a basket authored under a
    /// band applies only in that band, and a building's draw is the SUM of the
    /// baskets that apply. That lets a line common to both arcs be authored once
    /// while a band-specific line stays band-specific. See `building_upkeep_goods`.
    std::array<std::array<std::array<float, resource_count>, era_band_count>,
               building_type_count> goods = {};

    /// The ONE decay subtraction, per-mille of supply factor per tick, applied
    /// when a building's draw goes unmet. Deliberately the same quantity and the
    /// same rule as `unit_upkeep_params::supply_decay_permille` — a building has
    /// only the ONE trigger (a unit's out-of-reach trigger has no building
    /// counterpart; a building cannot march out of supply).
    int supply_decay_permille = 0;

    /// Regained per tick while the draw is met (or while there is nothing to
    /// draw). Ceilinged at 1000 (fully supplied).
    int supply_recovery_permille = 0;
};

/// Resolve @p bt's upkeep basket for a campaign running in band @p campaign,
/// per the band rule on `building_upkeep_params::goods`. A building type outside
/// the table resolves to an all-zero basket rather than failing — an unknown
/// type is a data problem, not a reason to make a building free.
///
/// Free function, not a member, so every reader — the live pass, a harness, the
/// census — composes the bands identically. `any` as the CAMPAIGN band (the
/// unset default every hand-built harness registry carries) draws the `any`
/// basket alone, never the union of both arcs: a registry that was never told
/// which era it is in must not be handed an ancient workshop's tools AND an
/// industrial one's electronics.
inline std::array<float, resource_count>
building_upkeep_goods(const building_upkeep_params& p, building_type bt, era_band campaign)
{
    std::array<float, resource_count> out{};
    const std::size_t bi = static_cast<std::size_t>(bt);
    if (bi >= building_type_count)
        return out;
    const auto& per_band = p.goods[bi];
    const auto& base     = per_band[static_cast<std::size_t>(era_band::any)];
    for (std::size_t r = 0; r < resource_count; ++r)
        out[r] = base[r];
    if (campaign != era_band::any)
    {
        const auto& banded = per_band[static_cast<std::size_t>(campaign)];
        for (std::size_t r = 0; r < resource_count; ++r)
            out[r] += banded[r];
    }
    return out;
}

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

/// BL-617 (population migration) tunables, authored in scripts/economy.lua
/// under `economy.migration` (docs/economy/POPULATION.md § Migration). Read
/// only by run_population_migration (economy_system.cpp). First-cut-then-tune
/// (the NR-600 idiom): the SHAPE — a per-tick permille flow from low- toward
/// high-attractiveness centres, stance-gated between nations, carrying
/// qualification — is the design; every number here is a tuning dial.
///
/// The defaults reproduce the shipped Lua values, so a harness that hand-builds
/// a registry gets live-shaped flows without Lua.
struct migration_params
{
    /// Share (thousandths) of a below-mean centre's heads that seeks to move
    /// each tick. Integer permille: the flow arithmetic is integer-exact so
    /// heads are conserved head-for-head. [0, 1000]; loader rejects outside.
    int rate_permille = 10;

    /// Throttle (thousandths) on a flow between two NEUTRAL nations — neither
    /// friends nor hostile in the stance store. Friendly pairs pass at 1000,
    /// hostile pairs at 0, same-nation moves ungated. [0, 1000].
    int neutral_gate_permille = 250;

    /// Credits→attractiveness conversion on the clearing-wage signal:
    /// attractiveness = habitability + wage_weight × clearing_wage(nation, body).
    /// Finite, >= 0; loader rejects otherwise.
    float wage_weight = 0.02f;

    /// Brain-drain selectivity: migrants carry min(1, origin qualification ×
    /// this) as their own qualified share — movers skew qualified, so
    /// emigration debits the origin nation's FRACTION, not just its headcount.
    /// >= 1 (loader-enforced): below 1 would make emigration RAISE the origin's
    /// fraction, inverting the design.
    float qualified_selectivity = 1.5f;
};

/// BL-442 price-band tunables, authored in scripts/economy.lua under
/// `economy.price_band`. THE single authority for how far a market price may
/// travel from its rarity-derived `base_price`: the clamp `resolve_price`
/// (market_clearing.cpp) applies to every market price each tick, and the
/// identical clamp the BL-181 workforce auto-solver applies to its forward price
/// estimate (`wf_target_price`, economy_system.cpp) so the solver prices a
/// candidate against the same band the market will actually clear it in.
///
/// Both sites used to hold their own `constexpr` copy, hand-synchronised — the
/// second literally commented "mirror market_clearing.cpp price band". That is
/// what this struct removes: the band is authored once, in data, and both sites
/// read it off the registry. `tools/verify/price_band_harness.cpp` is the guard
/// that they cannot drift apart again.
///
/// The defaults reproduce the pre-BL-442 hard-coded band exactly, so a harness
/// that hand-builds a registry and never authors this table is unchanged. See
/// docs/economy/MARKETS.md § The price band.
struct price_band_params
{
    float floor_mult = 0.25f; ///< Lowest a price may fall: 0.25x base_price.
    float ceil_mult  = 4.0f;  ///< Highest a price may rise: 4x base_price.

    /// BL-654 — the BUYER'S RESERVATION CEILING, x base_price. "Go without
    /// rather than buy above this."
    ///
    /// It lives in THIS family and not in upkeep (Ben, 2026-08-26) because it
    /// is a statement about what a good is worth PAYING, not about who is
    /// buying: one number per world, read by every goods draw, exactly as
    /// `floor_mult`/`ceil_mult` are read by every price resolution.
    ///
    /// It is the exact mirror of `sell_order::floor_price`, the seller's
    /// reservation (BL-386): both sides may decline a trade, neither may
    /// dictate one. A short pool bids its shortfall onto the market while the
    /// good prices at or below `reservation_mult x base_price`, and declines to
    /// bid at all above it — the shortfall then stands and the existing
    /// shortfall rule weakens the building or unit, which is an outcome the
    /// design already knows how to express.
    ///
    /// ZERO MEANS THE DRAW NEVER BUYS, which is the pre-BL-654 pool-only
    /// behaviour exactly — and it is the DEFAULT, so a harness that hand-builds
    /// a registry and never authors this table is byte-identical. A resource
    /// with `base_price <= 0` is unbuyable for the same arithmetic reason (its
    /// ceiling is 0 and no price clears it), which is `run_construction`'s own
    /// "unpriced == unbuyable" reading rather than a second rule.
    ///
    /// Bounded above by `ceil_mult` in practice, and meaningfully BELOW it: a
    /// resource pegged at `ceil_mult` is a generation-calibration signal
    /// (MARKETS.md § Price resolution), not a legitimate purchase, so a
    /// reservation equal to the ceiling would decline nothing and the rule
    /// would be inert. See docs/economy/MARKETS.md § Settled: a short pool BUYS.
    float reservation_mult = 0.0f;
};

/// BL-708 — what makes a good a GRID GOOD, as authored data.
///
/// `power` is the roster's first good whose MOVEMENT and MARKET are separate
/// questions (docs/economy/PRODUCTION.md § Power; docs/economy/LOGISTICS.md
/// § 3a). It has a price and it clears like any other good — that half needs no
/// new machinery at all, which is the design's whole claim. What is new is two
/// properties, and they are carried HERE, per resource, rather than as a branch
/// on `resource_type::power` in the four places that read them. A branch would
/// make "is this power?" a question logic asks; a table makes it a fact the data
/// states, which is the difference between a rule and a special case.
///
/// EVERY FIELD DEFAULTS TO THE INERT VALUE — `is_grid` all false, every ceiling
/// zero — so a harness that hand-builds a registry and never authors
/// `economy.grid_goods` behaves exactly as it did before this struct existed,
/// the same tolerance `price_band_params::reservation_mult = 0` carries.
struct grid_goods_params
{
    /// True for a good transmitted on the ROAD NETWORK rather than carried by a
    /// convoy. Two consequences, and they are the two the design names:
    ///
    ///   * NEVER CARGO. `price_convoy_leg` refuses a leg for a grid good, so
    ///     neither auto-dispatch nor the player's/rival's `dispatch_convoy` verb
    ///     can ship it — one refusal at the one shared seam, not three.
    ///   * CONNECTION-GATED. It only moves to or from a tile the network
    ///     reaches, and connectivity is `tile_reach_cost` read as a BOOLEAN:
    ///     finite means connected, infinity means not. That is the multi-source
    ///     Dijkstra § 3 already computes — no second graph and no second field,
    ///     which is LOGISTICS.md § 3a's own reduction of the problem.
    ///
    /// Latency is deliberately absent from this struct: it is a flat one tick
    /// regardless of distance, which is the tick the draw already runs on, so
    /// there is nothing to author.
    std::array<bool, resource_count> is_grid = {};

    /// Per-resource stockpile CEILING — the store may not hold more than this.
    /// ZERO MEANS UNCAPPED, which is every other good in the roster and the
    /// reason this lands inert.
    ///
    /// Power's one genuinely novel property against the rest of the roster
    /// (PRODUCTION.md § Power): "a generator running into a full store is
    /// producing nothing anyone will ever buy — a real decision rather than an
    /// accounting detail." Applied where output ACCRUES, so the overflow is
    /// never produced rather than produced and then deleted; a corp that has
    /// filled its store has to find a buyer, sell down, or throttle.
    ///
    /// Deliberately per (corp, body) — the grain the pool itself is held at —
    /// rather than per building, so it is a statement about a corp's storage on
    /// a body, not about any one generator's tank.
    std::array<float, resource_count> stockpile_ceiling = {};

    /// Is @p r transmitted on the network rather than carried? Bounds-checked so
    /// every reader can pass a raw index without repeating the guard.
    bool grid(std::size_t r) const
    {
        return (r < resource_count) && is_grid[r];
    }

    /// @p r's stockpile ceiling, or 0 for "uncapped". Bounds-checked, as above.
    float ceiling(std::size_t r) const
    {
        return (r < resource_count) ? stockpile_ceiling[r] : 0.0f;
    }

    /// Does ANY resource carry a grid rule? The zero-cost early out every
    /// per-tick reader takes first, so a world that authors no grid good pays
    /// one bool for the whole feature rather than a per-resource test per draw.
    bool any() const
    {
        for (std::size_t r = 0; r < resource_count; ++r)
            if (is_grid[r] || stockpile_ceiling[r] > 0.0f)
                return true;
        return false;
    }
};

/// BL-263 spontaneous-market-emergence tunables, authored in scripts/economy.lua
/// under `economy.market_emergence`. Two independent uses, both keyed off distance
/// from the home body (world::home_body), the only body with a market at world
/// generation: seeding a newly-spawned outpost market's opening prices
/// (maybe_spawn_market), and pulling a discounted slice of a home-body
/// COUNTERPARTY's unmet demand onto every outpost market each tick so an outpost
/// with supply and no local population does not collapse to the price floor
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
    /// Fraction of the counterpart home-body market's unmet demand (its demand
    /// less its PREVIOUS tick's supply, when positive) pulled onto each outpost
    /// market per tick, before the distance falloff below. The counterpart is
    /// chosen per resource — the home-body market wanting that resource most
    /// (BL-406); it is not "the home market" and not a body-level aggregate.
    ///
    /// NOT re-tuned when BL-406/BL-404 landed (2026-08-17), though the ruling
    /// cleared it to be: on seed 0 the corrected pull is 2.8x its old size where
    /// it fires at all. Folded into BL-440's repricing pass — NR-277.
    float pull_fraction = 0.50f;
    /// Falloff denominator per AU: pulled_demand = shortfall * pull_fraction
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
    /// The SHARED (`any`) tranche: per-scale-point demand weight per resource,
    /// indexed by static_cast<std::size_t>(resource_type). Unlisted resources
    /// get 0. BL-640: this is no longer the whole basket — see `baskets` below,
    /// and read the era-resolved sum through
    /// recipe_registry::population_demand_basket(), which is what the injector
    /// multiplies by.
    std::array<float, resource_count> demand_basket = {};
    /// BL-640 era-banded tranches, in authored order. The registry folds every
    /// row `era_permits` admits onto the shared tranche above; a row the current
    /// band excludes contributes nothing. Authored order, so the fold is a pure
    /// function of the script and not of any container layout.
    std::vector<era_basket> baskets;
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
    /// The SHARED (`any`) tranche: per-population-scale-point demand weight per
    /// resource, indexed by static_cast<std::size_t>(resource_type). Unlisted
    /// resources get 0. BL-640: read the era-resolved sum through
    /// recipe_registry::background_demand_basket().
    std::array<float, resource_count> demand_basket = {};
    /// BL-640 era-banded tranches, in authored order — same fold, same mask as
    /// population_demand_params::baskets. All six of this stopgap's goods are
    /// industrial, so today every row here is banded `industrial` and an ancient
    /// campaign draws nothing from this pass. BANDED, NOT DELETED: the pass is
    /// still the stand-in for the Industry channel until BL-641 lands.
    std::vector<era_basket> baskets;
    float demand_elasticity = 0.80f; ///< exponent on (base_price / price).
    float elasticity_min    = 0.30f; ///< clamp lo on the elasticity factor.
    float elasticity_max    = 2.50f; ///< clamp hi on the elasticity factor.
    float demand_scale      = 1.00f; ///< global scale → demand scale.
};

/// BL-647 endemic-luxury-demand tunables, authored in scripts/economy.lua under
/// `economy.endemic_demand`. The Endemic trade channel (MARKETS.md § Demand
/// channels): a household pull for the endemic luxury goods (tobacco, spices,
/// coffee, furs) that scales with a nation's WEALTH rather than its headcount,
/// flavoured per (nation, good) by a seeded, campaign-fixed preference weight —
/// so different nations crave different luxuries and the trade route is
/// directional by construction. Applied at tick time by inject_endemic_demand
/// (market_clearing.cpp), called from clear_markets alongside the population and
/// background injectors. Same price-elastic shape as both siblings — deliberately
/// not a second elasticity model.
struct endemic_demand_params
{
    /// The SHARED (`any`) tranche: per-credit-of-national-wealth demand weight
    /// per resource (see wealth_scale for the units), indexed by
    /// static_cast<std::size_t>(resource_type). Unlisted resources get 0.
    /// Luxuries are DEPOSITS — band-independent by construction, like
    /// agricultural_produce and water in the population basket's shared tranche
    /// — so the authored basket lives here; the era rows below exist for
    /// symmetry with the two sibling params and for any future banded luxury.
    /// Read the era-resolved sum through
    /// recipe_registry::endemic_demand_basket().
    std::array<float, resource_count> demand_basket = {};
    /// Era-banded tranches, same fold, same mask as
    /// population_demand_params::baskets (BL-640's mechanism, reused).
    std::vector<era_basket> baskets;
    float demand_elasticity = 0.80f; ///< exponent on (base_price / price).
    float elasticity_min    = 0.30f; ///< clamp lo on the elasticity factor.
    float elasticity_max    = 2.50f; ///< clamp hi on the elasticity factor.
    /// Credits of national wealth → demand units, multiplied by the basket
    /// weight: demand[r] = wealth_share × wealth_scale × basket[r] × pref ×
    /// elastic. DEFAULTS OFF (0.0) so a hand-built harness registry — and every
    /// pre-BL-647 golden — injects nothing; scripts/economy.lua authors the
    /// real value (the building_upkeep zero-default precedent).
    float wealth_scale = 0.0f;
    /// Half-width of the per-(nation, good) preference band around 1.0:
    /// pref ∈ [1 − spread, 1 + spread), mean 1 regardless of spread, so tuning
    /// asymmetry never moves the channel's total. 0 = every nation craves
    /// identically; 1 = a nation may ignore a luxury entirely or want double.
    float preference_spread = 0.0f;
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

    /// BL-709 — units of `construction_capacity` a site under construction draws
    /// per FULL-RATE tick, on top of its material basket
    /// (docs/economy/PRODUCTION.md § Construction as a rate: "Building projects
    /// consume that capacity").
    ///
    /// It is added to the SAME per-tick need row `run_construction` already
    /// builds from `resource_build_cost_for`, so it joins the same `rate`
    /// minimum, the same stretch/pause thresholds and the same want register as
    /// every material — one rule rather than a second one. A build stalled for
    /// want of capacity therefore registers that want, which prices capacity and
    /// induces the yard that answers it (MARKETS.md property 3).
    ///
    /// DEFAULTS TO ZERO = the pre-BL-709 behaviour exactly, which is what a
    /// hand-built harness registry gets and why every construction harness that
    /// authors no value is arithmetically untouched. scripts/economy.lua authors
    /// the shipped value.
    float capacity_per_build_tick = 0.0f;

    /// BL-709 — how much `construction_capacity` demand the PRE-GAME SEEDER
    /// provisions per standing building on a body, so that construction yards
    /// exist at tick 0 (docs/economy/PRODUCTION.md § Construction as a rate:
    /// "because generation can SEED CONSTRUCTION CAPACITY, the demand for its
    /// inputs is non-zero from tick 0, in every market that has any").
    ///
    /// IT IS A GENERATION-TIME ESTIMATE, NOT A LIVE DRAW, and the distinction is
    /// the whole reason it is a separate field from `capacity_per_build_tick`
    /// above. Nothing in the tick reads it. The live consumer is the build
    /// project, which is episodic by nature; the seeder cannot size against an
    /// episodic figure that happens to read zero at generation time, so it sizes
    /// against the STANDING BUILDING STOCK instead — the steady-state rate at
    /// which a world of that many buildings replaces and extends itself. That is
    /// MARKETS.md property 1's own "more buildings" scaling, read as a
    /// provisioning target.
    ///
    /// Generation provisions the sector; the market decides everything
    /// downstream of it — the same division BL-708 drew for power. ZERO means
    /// the seeder provisions no capacity, which is the pre-BL-709 behaviour and
    /// what a hand-built harness registry gets.
    float seed_capacity_per_building = 0.0f;
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

    // BL-434 added a `cross_group_multiplier` here (a steep price tier on a
    // cross-group retool) and it was removed again minutes later in the same
    // session: Ben reconsidered and cross-group switching is now REFUSED
    // outright (recipe_switch_result::cross_group in economy_system.hpp), not
    // priced — see try_switch_recipe's doc comment for the full call. Noted
    // here so a future reader isn't left wondering why a tunable was added and
    // pulled back-to-back rather than assuming it was reverted by accident.
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

/// BL-332 research accumulation rate, authored in scripts/economy.lua under the
/// top-level `military` table. A flat per-tick credit to the owning corp's
/// corporation_component.science for every COMPLETED research_institute it owns —
/// see economy_system.cpp's research-points pass.
///
/// `military_points_per_base_tick` was removed 2026-08-17 (BL-455) with the field
/// it fed. The struct keeps its name because BL-394's hire costs live in it too.
struct military_capability_params
{
    float science_per_research_institute_tick = 1.0f;

    /// BL-394: hire_unit's credit cost, debited from the corp's balance at
    /// apply time — cost = hire_base_cost + hire_cost_per_power × the roster
    /// row's power_mod. The base is a FLOOR: row 0's gate is all-zero, so the
    /// resource debit alone made the cheapest rows free and hiring unbounded
    /// for any seam caller. Defaults match economy.lua so a hand-built harness
    /// registry charges hires sensibly without Lua.
    float hire_base_cost      = 40.0f;
    float hire_cost_per_power = 0.5f;

    /// BL-454: standing-force UPKEEP — what it costs to KEEP a unit, as opposed
    /// to `hire_*` above, which is what it costs to raise one. Authored under
    /// `economy.military.unit_upkeep`; see unit_upkeep_params (unit_roster.hpp)
    /// for the field-by-field contract and for why every rate defaults to zero.
    /// BL-394's own comment flagged upkeep as "deliberately absent" and pointed
    /// at the contract loop; this is where it actually landed.
    unit_upkeep_params upkeep = {};

    /// BL-470: march points spent per tick against per-tile traversal cost,
    /// keyed by `unit_class` (combat.hpp — infantry/cavalry/ranged/siege/naval).
    /// PER-CLASS FROM THE START (overturned the item's own one-speed first
    /// cut, same elicitation) — a wing outpaces a line, authored as data
    /// rather than a code branch. Authored under
    /// `economy.military.march_points_per_class`; a class with a non-positive
    /// entry cannot march at all (naval defaults to 0 — no naval movement
    /// model exists yet, combat.hpp's own "strategic-only presence" note).
    /// A composite unit (BL-472, not built here) reads its SLOWEST member's
    /// class entry — the hook, not the composite logic.
    std::array<float, unit_class_count> march_points_per_class = {};

    /// BL-596 (LP_ACTIVE_MARCH, LOGISTICS.md § Logistic Points): active LP
    /// generated per supply anchor per tick, in the SAME units as a march
    /// point (`march_points_per_class` above) — one active LP admits exactly
    /// one march-point's worth of a unit's movement this tick, so the cap and
    /// the thing it caps share a scale rather than needing a conversion
    /// factor. Zero (the default) means no active LP exists anywhere, which
    /// refuses every march — economy.lua sets the real first-cut rate.
    /// Authored under `economy.military.active_lp_per_anchor_tick`.
    float active_lp_per_anchor_tick = 0.0f;

    /// BL-596: credits charged per (active-LP-unit x head) an active march
    /// draws — "it should cost actual money to resolve LP usage... moving
    /// units" (Ben, 2026-08-22). LP stays the CAP (rule 1: "LP is a cap, not
    /// a price"); this is the separate price. First-cut derivation against
    /// `logistics.base_cost_per_unit_distance.land` in scripts/economy.lua —
    /// see that file's comment. Authored under
    /// `economy.military.active_lp_credit_per_unit_distance`.
    float active_lp_credit_per_unit_distance = 0.0f;
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

    // --- BL-392: a commitment has to BUY something --------------------------
    // Before this, unit_price was spot and the goods liquidated at spot, so a
    // measured 20-unit iron round trip settled at -0.14 credits: break-even
    // minus friction, by construction, and a rational agent never contracts.
    // The discount is what the buyer gets for committing volume ahead of time;
    // the freight is what the distance costs. Both are data.

    /// Ceiling on the commitment discount off spot, as a fraction. The realised
    /// discount is `max * q / (q + half_quantity)` — asymptotic, so no order can
    /// drive the price to zero however large it is.
    float volume_discount_max = 0.15f;

    /// Order quantity at which HALF the ceiling discount is reached. Larger =
    /// the discount is reserved for bigger commitments.
    float volume_discount_half_quantity = 100.0f;

    /// Carriage when the goods land on a body other than the one the supplier
    /// fulfils from, as a fraction of the order's pre-discount goods value.
    /// Paid TO THE SUPPLIER, who arranges the shipping — a transfer, not a burn.
    /// Below `volume_discount_max`, so a volume order still beats spot after
    /// carriage; a same-body delivery pays nothing.
    float offbody_freight_fraction = 0.05f;
};

/// BL-628 whole-firm acquisition tunables, authored in scripts/economy.lua under
/// the top-level `acquisition` table. One number today, and a table rather than a
/// loose float because the acquisition market is the kind of thing that grows a
/// second term (a control premium, a minimum-filings gate) the moment it is
/// played with.
struct acquisition_params
{
    /// `k_acquisition_multiple` — how many quarters of trailing earnings the
    /// profit term is worth (FINANCE.md § Whole-firm acquisition). One economy
    /// tick is one quarter, so 8 is TWO YEARS of purchase against the eight
    /// quarters of observation `k_acquisition_trailing_quarters` averages: the
    /// buyer pays two years of what the last two years actually earned. A round,
    /// legible first cut, to be moved by playtest and nothing else — it is
    /// authored precisely so that moving it is a data change.
    ///
    /// The term is SIGNED and unclamped by design: a loss-making firm prices
    /// below its book value, which is the ledger telling the truth about it.
    float multiple = 8.0f;
};

/// BL-546: THE TWO PROCUREMENT RATES ARE TWO SENTIMENT FACTOR WEIGHTS.
///
/// Reputation stopped being its own store (`world::corp_reputation`) and became
/// the TRUST dimension of sentiment at (buyer, supplier) grain. Nothing about
/// the two rates' MEANING changed with it — a completed contract still raises
/// the axis by `reputation_on_complete`, a cancelled one still lowers it by
/// `reputation_on_cancel`. This function is where that is guaranteed once,
/// rather than restated at each writer: whatever `procurement` authors, the two
/// Trust weights follow it.
///
/// It is also what makes the migration work WITHOUT a Lua edit. `sentiment_params`
/// defaults every weight to zero — BL-545's inertness rule, which must stay true
/// of the substrate itself — so unseeded, a build that had not yet authored
/// `economy.sentiment` would have silently stopped moving reputation at all.
/// `scripts/economy.lua`'s own `sentiment.factors` rows are applied AFTER this
/// (recipe_registry.cpp) and win where they name a row.
///
/// ACCESS IS DELIBERATELY UNTOUCHED. Procurement conduct says nothing about
/// whether the observer would LET the subject operate; that dimension belongs to
/// BL-540, and seeding it here would invent a meaning the rates never had.
///
/// @param sp Sentiment parameters to seed, mutated in place.
/// @param pp The authored procurement rates.
inline void seed_procurement_sentiment(sentiment_params& sp, const procurement_params& pp)
{
    sp.factors[static_cast<std::size_t>(sentiment_factor_kind::contract_completed)].trust =
        pp.reputation_on_complete;
    sp.factors[static_cast<std::size_t>(sentiment_factor_kind::contract_cancelled)].trust =
        pp.reputation_on_cancel;
}

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

    /// BL-434: same contract as default_recipe_id() above, narrowed to one GROUP —
    /// the first era-allowed recipe whose `group` matches. Used by the Build door
    /// to seed a freshly-placed processing_facility with a group's own default
    /// recipe when the caller wants "the Metal Foundry" rather than "a recipe".
    /// Returns `no_recipe` if the group has no era-allowed recipe (should not
    /// happen for an authored group, but a caller passing a stale/typo'd group
    /// name gets a clean no-op rather than an out-of-range id).
    uint16_t default_recipe_id(const std::string& group) const
    {
        for (const std::size_t i : m_allowed)
            if (m_recipes[i].group == group)
                return static_cast<uint16_t>(i);
        return no_recipe;
    }

    /// Economy constants for a building type.
    const building_economics& economics(building_type type) const
    {
        return m_building_econ[static_cast<std::size_t>(type)];
    }

    /// BL-590: the material cost for a SPECIFIC named building, falling back
    /// to `economics(type).resource_build_cost` when no override is authored.
    /// `extraction_site` is keyed by its `target_resource`, `processing_facility`
    /// by its recipe id — the same two identities every other per-building
    /// lookup in this codebase already uses (a building's TYPE says what kind
    /// of thing it is; its target/recipe says which named building it is).
    /// No building_type parameter is needed in the override keys themselves:
    /// only `extraction_site` ever carries a target and only
    /// `processing_facility` ever carries a recipe, so the type dispatch below
    /// is unambiguous without one.
    ///
    /// The single lookup every material-cost call site goes through, so a
    /// preview (the Build door's capex, the management view's rate) can never
    /// disagree with the real per-tick draw — the same argument BL-436 makes
    /// about richness, applied here to materials.
    const std::array<float, resource_count>&
    resource_build_cost_for(building_type type, resource_type target, std::uint16_t recipe) const
    {
        if (type == building_type::extraction_site)
        {
            const auto it = m_extraction_material_overrides.find(target);
            if (it != m_extraction_material_overrides.end())
                return it->second;
        }
        else if (type == building_type::processing_facility)
        {
            const auto it = m_processing_material_overrides.find(recipe);
            if (it != m_processing_material_overrides.end())
                return it->second;
        }
        return economics(type).resource_build_cost;
    }

    /// BL-615: the stratum placement gate for a SPECIFIC named building — the
    /// type's authored gate, with a processing recipe's own
    /// `centre_proximity_radius` overriding the type's when non-zero. The same
    /// per-named-building fallback shape as `resource_build_cost_for` above,
    /// and for the same reason: the caller that gates placement
    /// (construct_building) and any preview must resolve the SAME gate.
    /// Pass `no_recipe` for non-processing types (it is ignored for them).
    placement_gate placement_gate_for(building_type type, std::uint16_t id) const
    {
        placement_gate g = economics(type).gate;
        if (type == building_type::processing_facility)
            if (const recipe* r = get_recipe(id); r != nullptr && r->centre_proximity_radius > 0)
                g.centre_proximity_radius = r->centre_proximity_radius;
        return g;
    }

    float t_full() const { return m_t_full; }
    float t_idle() const { return m_t_idle; }

    /// BL-365 population-growth-gate tunables (economy.population_growth in Lua).
    const growth_params& growth() const { return m_growth; }

    /// BL-617 population-migration tunables (economy.migration in Lua).
    const migration_params& migration() const { return m_migration; }

    /// BL-368 population-demand model tunables (economy.population_demand in Lua).
    const population_demand_params& population_demand() const { return m_population_demand; }

    /// BL-640: the population basket AS MASKED BY THE CURRENT ERA — the shared
    /// (`any`) tranche plus every banded row `era_permits` admits, summed in
    /// authored order. THIS, not `population_demand().demand_basket` (the shared
    /// tranche alone), is the vector inject_population_demand multiplies by, and
    /// it is what any check asking "does this pass want this good" must read.
    /// An unset band (`era_band::any`, the default) admits every row, exactly as
    /// it admits every recipe.
    const std::array<float, resource_count>& population_demand_basket() const
    {
        return m_population_basket;
    }

    /// BL-340/BL-365 background-industrial-demand tunables (economy.background_demand in Lua).
    const background_demand_params& background_demand() const { return m_background_demand; }

    /// BL-640: the background-industrial basket as masked by the current era.
    /// Same fold, same caveat as population_demand_basket() above.
    const std::array<float, resource_count>& background_demand_basket() const
    {
        return m_background_basket;
    }

    /// BL-647 endemic-luxury-demand tunables (economy.endemic_demand in Lua).
    const endemic_demand_params& endemic_demand() const { return m_endemic_demand; }

    /// BL-647: the endemic luxury basket as masked by the current era.
    /// Same fold, same caveat as population_demand_basket() above.
    const std::array<float, resource_count>& endemic_demand_basket() const
    {
        return m_endemic_basket;
    }

    /// BL-442 price band (economy.price_band in Lua) — the ONE authority for the
    /// [floor x, ceil x] clamp around base_price, read by both resolve_price
    /// (market_clearing.cpp) and wf_target_price (economy_system.cpp).
    const price_band_params& price_band() const { return m_price_band; }

    /// BL-708 grid-good rules (economy.grid_goods in Lua): which goods ride the
    /// road network instead of a convoy, and what their stockpile ceiling is.
    /// Defaults to all-inert, so a hand-built registry authors nothing and
    /// behaves exactly as it did before the table existed.
    const grid_goods_params& grid_goods() const { return m_grid_goods; }

    /// BL-263 spontaneous-market-emergence tunables (economy.market_emergence in Lua).
    const market_emergence_params& market_emergence() const { return m_market_emergence; }

    /// BL-095 construction-gate tunables (economy.construction in Lua).
    const construction_params& construction() const { return m_construction; }

    /// BL-332 capability-point accumulation rates (economy.military in Lua).
    const military_capability_params& military() const { return m_military; }

    /// BL-641 building-upkeep rates (economy.building_upkeep in Lua): the
    /// per-type, era-banded goods vector plus the one decay curve. Every rate
    /// defaults to zero, so a hand-built harness registry draws nothing.
    const building_upkeep_params& building_upkeep() const { return m_building_upkeep; }

    /// BL-350 procurement/contract tunables (economy.procurement in Lua).
    const procurement_params& procurement() const { return m_procurement; }

    /// BL-545/BL-546 relational substrate tunables (economy.sentiment in Lua):
    /// the authored factor table plus the per-dimension decay rates. The two
    /// `contract_*` Trust weights are seeded from `procurement()` and overridden
    /// by Lua — see `seed_procurement_sentiment`.
    const sentiment_params& sentiment() const { return m_sentiment; }

    /// BL-542 nation-scorer tunables (economy.nation_ai in Lua). Defaults are
    /// the accepted-design values in `nation_ai_params` itself; the Lua table,
    /// where it exists, overrides field by field, and every value it names is
    /// range-checked at load (finite; `cadence_k` >= 1; every float >= 0) and
    /// REJECTED by key rather than clamped -- a scorer tuned by a NaN is a
    /// scorer nobody can replay. Sprint N3 T1: no caller yet; the registry is
    /// the home so that wiring the scorer (T6) reads one authored object.
    const nation_ai_params& nation_ai() const { return m_nation_ai; }

    /// BL-644 space-programme purchase lumps (economy.space_programme in Lua).
    /// Both default ZERO — an unauthored registry derives no state purchase —
    /// and every authored value is range-checked at load and rejected by key
    /// rather than clamped, the nation_ai discipline.
    const space_programme_params& space_programme() const { return m_space_programme; }

    /// BL-628 whole-firm acquisition tunables (economy.acquisition in Lua).
    /// One number, `multiple`, range-checked at load and rejected rather than
    /// clamped. The trailing WINDOW is not here: it is
    /// `k_acquisition_trailing_quarters` (components.hpp), a constant on purpose.
    const acquisition_params& acquisition() const { return m_acquisition; }

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

    /// BL-428 THE GATE: how far down the graph a corp must already have reached
    /// before it may run this recipe — the depth of the recipe's DEEPEST input,
    /// which is exactly the `deepest_input` term `rebuild_depth` already computes.
    ///
    /// DERIVED, never authored, and that is the ruling's own argument: a
    /// hand-authored per-building minimum would be a second system to keep in
    /// agreement with the economy, which is the cost chain depth was chosen to
    /// avoid. Build a Charcoal Burner (charcoal, depth 1) and the Bloomery becomes
    /// placeable because charcoal exists — not because a flag was set.
    ///
    /// Addressed by ABSOLUTE recipe id (the `get_recipe`/`recipe_id` space), not by
    /// browse position, because a gate is asked about a *stored*
    /// `building_component.recipe`.
    ///
    /// 0 for an input-free recipe (extraction — nothing must precede it), and -1
    /// when an input is unreachable under this band, matching `depth_of`'s code so
    /// the two never disagree about what "you cannot get there" means.
    int recipe_required_depth(uint16_t id) const
    {
        if (id == no_recipe || id >= m_required_depth.size())
            return -1;
        return m_required_depth[id];
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
        rebuild_baskets(); // BL-640: the demand baskets ride the same band change.
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
    /// BL-739: the idle-maintenance floor (economy.thresholds.idle_maintenance_floor).
    float idle_maintenance_floor() const { return m_idle_maintenance_floor; }
    void  set_idle_maintenance_floor(float f) { m_idle_maintenance_floor = f; }
    void set_growth(const growth_params& s) { m_growth = s; }
    void set_migration(const migration_params& m) { m_migration = m; }
    void set_population_demand(const population_demand_params& p)
    {
        m_population_demand = p;
        rebuild_baskets(); // BL-640: keep the era-resolved basket in step.
    }
    void set_background_demand(const background_demand_params& b)
    {
        m_background_demand = b;
        rebuild_baskets();
    }
    void set_endemic_demand(const endemic_demand_params& e)
    {
        m_endemic_demand = e;
        rebuild_baskets(); // BL-647: same era-resolved fold as the two siblings.
    }
    void set_price_band(const price_band_params& p) { m_price_band = p; }
    void set_grid_goods(const grid_goods_params& g) { m_grid_goods = g; }
    void set_market_emergence(const market_emergence_params& m) { m_market_emergence = m; }
    void set_construction(const construction_params& c) { m_construction = c; }
    void set_military(const military_capability_params& m) { m_military = m; }
    void set_building_upkeep(const building_upkeep_params& b) { m_building_upkeep = b; }
    /// BL-546: seeds the two `contract_*` Trust weights from @p p in the same
    /// call, so a harness that sets a procurement rate never gets a registry
    /// whose sentiment table disagrees with it.
    void set_procurement(const procurement_params& p)
    {
        m_procurement = p;
        seed_procurement_sentiment(m_sentiment, p);
    }

    /// Full control of the substrate's tunables (decay, epsilon, limit, every
    /// factor row). Call AFTER `set_procurement` — that one re-seeds the two
    /// `contract_*` Trust weights and would otherwise overwrite these.
    void set_sentiment(const sentiment_params& p) { m_sentiment = p; }
    void set_nation_ai(const nation_ai_params& p) { m_nation_ai = p; }
    void set_space_programme(const space_programme_params& p) { m_space_programme = p; }
    void set_recipe_switch(const recipe_switch_params& p) { m_recipe_switch = p; }
    void set_acquisition(const acquisition_params& p) { m_acquisition = p; }
    void set_economics(building_type type, const building_economics& e)
    {
        m_building_econ[static_cast<std::size_t>(type)] = e;
    }
    /// BL-590: test-construction setters for the per-named-building material
    /// overrides, mirroring `set_economics` above — a harness builds these by
    /// hand rather than going through `load_from_lua`.
    void set_extraction_material_override(resource_type target,
                                          const std::array<float, resource_count>& cost)
    {
        m_extraction_material_overrides[target] = cost;
    }
    void set_processing_material_override(std::uint16_t recipe,
                                          const std::array<float, resource_count>& cost)
    {
        m_processing_material_overrides[recipe] = cost;
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

    /// BL-640: recompute the era-resolved demand baskets — the SAME mask, the
    /// same predicate and the same authored-order walk `rebuild_allowed` uses
    /// for recipes, applied to the two basket injectors' tranches. Effective
    /// basket = the shared (`any`) tranche + every banded row era_permits admits.
    ///
    /// Deterministic by construction: `baskets` is a vector in authored order,
    /// so nothing here depends on a container's layout.
    void rebuild_baskets()
    {
        auto fold = [this](const std::array<float, resource_count>& shared,
                           const std::vector<era_basket>&           rows,
                           std::array<float, resource_count>&       dst)
        {
            dst = shared;
            for (const era_basket& b : rows)
            {
                if (!era_permits(m_era, b.era))
                    continue;
                for (std::size_t r = 0; r < resource_count; ++r)
                    dst[r] += b.demand_basket[r];
            }
        };
        fold(m_population_demand.demand_basket, m_population_demand.baskets, m_population_basket);
        fold(m_background_demand.demand_basket, m_background_demand.baskets, m_background_basket);
        fold(m_endemic_demand.demand_basket,    m_endemic_demand.baskets,    m_endemic_basket); // BL-647
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

        // Required depth per recipe, from the settled table. Walked over ALL
        // recipes in authored order (not m_allowed), so a stored recipe id always
        // indexes a valid entry even when the era mask hides that recipe from
        // browsing — the same absolute/browse split get_recipe keeps.
        m_required_depth.assign(m_recipes.size(), -1);
        for (std::size_t i = 0; i < m_recipes.size(); ++i)
        {
            const recipe& rc = m_recipes[i];
            int  deepest     = 0;
            bool ready       = true;
            for (std::size_t r = 0; r < resource_count && ready; ++r)
            {
                if (rc.inputs[r] <= 0.0f)
                    continue;
                const int d = m_depth[r];
                if (d == unreachable)
                    ready = false;
                else if (d > deepest)
                    deepest = d;
            }
            m_required_depth[i] = ready ? deepest : unreachable;
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

    /// BL-428 gate: required reached-depth per recipe, indexed by ABSOLUTE recipe
    /// id. Rebuilt with m_depth from the same settled table, so the gate and the
    /// readout can never disagree. Sized to m_recipes (not m_allowed) — see
    /// rebuild_depth's tail.
    std::vector<int> m_required_depth;

    /// Indexed by building_type. Sized off `building_type_count` since BL-615
    /// (which appended schooling / university) rather than a hand-kept literal
    /// — appending a type means moving the enum's tail constant, and this
    /// array follows it.
    std::array<building_economics, building_type_count> m_building_econ = {};

    /// BL-590: per-named-building material overrides, populated by `load_from_lua`
    /// from `buildings.extraction_site.material_overrides` /
    /// `buildings.processing_facility.material_overrides`. See
    /// `resource_build_cost_for`'s comment for why no building_type is needed
    /// in either key.
    std::map<resource_type, std::array<float, resource_count>> m_extraction_material_overrides;
    std::map<std::uint16_t, std::array<float, resource_count>> m_processing_material_overrides;

    float m_t_full = 1.0f;
    float m_t_idle = 0.2f;

    /// BL-739: fraction of `maintenance` charged even at workforce 0 or
    /// decommissioned. The C++ default is the pre-BL-739 constant so an
    /// unloaded registry (hand-built harness mirrors) runs the old arithmetic;
    /// the shipped Lua authors 0.15 (Ben, 2026-09-01).
    float m_idle_maintenance_floor = 0.3f;

    /// BL-078 elastic-substrate model tunables (economy.substrate). Defaults match
    /// economy.lua so a hand-built harness registry behaves sensibly without Lua.
    growth_params m_growth = {};
    migration_params m_migration = {};
    population_demand_params m_population_demand = {};
    background_demand_params m_background_demand = {};
    endemic_demand_params    m_endemic_demand    = {}; // BL-647
    /// BL-640: the era-resolved folds of the three above, rebuilt by
    /// rebuild_baskets() on every band change and every setter. Cached rather
    /// than recomputed per read: inject_population_demand walks it once per
    /// centre per tick.
    std::array<float, resource_count> m_population_basket = {};
    std::array<float, resource_count> m_background_basket = {};
    std::array<float, resource_count> m_endemic_basket    = {}; // BL-647
    price_band_params m_price_band = {};
    grid_goods_params m_grid_goods = {};
    market_emergence_params m_market_emergence = {};

    /// BL-095 construction-gate tunables (economy.construction). Defaults match
    /// economy.lua so a hand-built harness registry paces builds sensibly.
    construction_params m_construction = {};

    /// BL-332 capability-point rates. Defaults match economy.lua so a
    /// hand-built harness registry behaves sensibly without Lua.
    military_capability_params m_military = {};

    /// BL-641 building-upkeep rates. Every rate defaults to ZERO — the goods
    /// sink is inert until `economy.building_upkeep` authors one, which is what
    /// keeps every pre-BL-641 harness (all of which hand-build their registry)
    /// bit-identical.
    building_upkeep_params m_building_upkeep = {};

    /// BL-350 procurement tunables. Defaults match economy.lua.
    procurement_params m_procurement = {};

    /// BL-545/BL-546 relational substrate tunables. Decay defaults to ZERO —
    /// the substrate is inert until `economy.sentiment` authors a rate, so a
    /// build that never authors it replays bit-identically — but the two
    /// `contract_*` Trust weights are seeded from the procurement defaults, or
    /// the migration would have silently stopped moving reputation.
    sentiment_params m_sentiment = [] {
        sentiment_params sp;
        seed_procurement_sentiment(sp, procurement_params{});
        return sp;
    }();

    /// BL-542 nation-scorer tunables (economy.nation_ai). Default-constructed,
    /// so a hand-built harness registry scores exactly as the scorer's own
    /// defaults do -- the registry adds a home, not a second set of numbers.
    nation_ai_params m_nation_ai = {};

    /// BL-644 space-programme purchase lumps (economy.space_programme).
    /// Default-constructed — both lumps zero, so a hand-built harness registry
    /// derives no state purchase unless it sets them.
    space_programme_params m_space_programme = {};

    /// BL-430 recipe-switch cost/cooldown. Defaults to free/instant so a
    /// hand-built harness registry that never sets this behaves as it always did.
    recipe_switch_params m_recipe_switch = {};

    /// BL-628 acquisition tunables. Defaults match economy.lua, so a hand-built
    /// harness registry prices a buyout exactly as the shipped game does.
    acquisition_params m_acquisition = {};

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
