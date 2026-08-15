#pragma once

#include "entity.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Shared enumerations
// ---------------------------------------------------------------------------

/// Resource types produced, traded, and consumed in the economy. Ordered by the
/// production tiers in docs/economy/RESOURCES.md. All values are defined from the
/// start so array sizes are correct and no data-model retrofit is required as the
/// economy is authored; resources outside the prototype subset simply carry zero
/// tile deposits and no market entries until a later pass authors them.
enum class resource_type : uint8_t
{
    // --- Tier 1: raw materials (Earth-sourced) ---
    iron_ore              = 0,  ///< Backbone structural mineral.
    coal                  = 1,  ///< Carbon energy source and smelting reagent.
    petroleum             = 2,  ///< Liquid hydrocarbon; fuel precursor.
    silica                = 3,  ///< Silicon dioxide; semiconductor and bulk input.
    copper_ore            = 4,  ///< Primary conductive metal ore.
    rare_earth_ore        = 5,  ///< Critical minerals for electronics and magnets.
    agricultural_produce  = 6,  ///< Food crop output; needs water and habitability.
    // --- Tier 1: raw materials (space-sourced) ---
    water                 = 7,  ///< Extracted from surface/subsurface ice.
    iron_nickel_ore       = 8,  ///< Metallic-asteroid feedstock for steel.
    platinum_group_metals = 9,  ///< Ultra-rare catalytic metals; belt's high-value good.
    regolith              = 10, ///< Loose surface material on airless bodies; in-situ build mass.
    // --- Tier 1: ambient (low-value, near-universal) ---
    stone                 = 11, ///< Universal construction aggregate.
    timber                = 12, ///< Construction material and fuel.
    sand                  = 13, ///< Glass precursor; construction aggregate.
    clay                  = 14, ///< Ceramics and construction.
    peat                  = 15, ///< Pre-industrial fuel.
    // --- Tier 1: endemic trade goods (the MERCANTILE value track, BL-191) ---
    //
    // These are Tier 1 raws like any other, but their value comes from a
    // different place. An industrial good is worth something because it is
    // USEFUL — coal burns, iron bears load. An endemic good is worth something
    // because it only grows THERE: the biosphere evolved it in one region and
    // nowhere else, so its price is a function of distance from where it grows.
    //
    // A body only carries the ones its own biosphere produced, and only in the
    // band and sector it evolved in. See docs/economy/RESOURCES.md § Mercantile.
    tobacco               = 16, ///< Temperate/subtropical grassland leaf crop.
    spices                = 17, ///< Tropical wetland and forest aromatics.
    coffee                = 18, ///< Subtropical highland forest bean.
    furs                  = 19, ///< Subpolar and polar tundra pelts.
    // --- Tier 2: refined goods (prototype subset) ---
    steel                 = 20, ///< Smelted from iron ore (+ coal).
    refined_fuel          = 21, ///< Refined from petroleum.
    food_rations          = 22, ///< Processed from agricultural produce.
    // --- Logistics goods (BL-286, 2026-08-04) ---
    //
    // Enum + serialization + base-price wiring only (BL-286). The follow-on
    // behaviours — consumption, transport-capacity range cap, salt shelf-life
    // gate, bullion purchase — are NOT yet filed; ids get allocated via
    // `node tools/session/next_id.js` when they are. Do not cite placeholder
    // ids here: a wrong id routes the reader to unrelated work, which is worse
    // than no id. See docs/economy/RESOURCES.md § Logistics goods.
    grain                 = 23, ///< Human ration staple; per-tick army/unit draw (behaviour unfiled).
    fodder                = 24, ///< Draft-animal/cavalry feed; per-tick draw alongside grain (behaviour unfiled).
    salt                  = 25, ///< Preservative; gates ration shelf-life (behaviour unfiled).
    transport_capacity    = 26, ///< Abstract logistics-train throughput good; caps supply range (behaviour unfiled).
    charcoal              = 27, ///< Refined fuel-wood; pre-coal smelting/heating input.
    iron_blooms           = 28, ///< Bloomery-refined iron intermediate — distinct from raw iron/iron-nickel ore.
    bullion               = 29, ///< Minted precious-metal specie; local purchase medium (behaviour unfiled).
    trade_goods_misc      = 30, ///< Placeholder endemic-luxury-class good; a specific luxury name is a later design step.
    // --- Tier 2: propellant (BL-308, 2026-08-09) ---
    //
    // The good a Launchpad burns to put a convoy into space. Two authored
    // routes (scripts/recipes.lua, PRODUCTION.md § Chemical Plant): the Era 0
    // atmosphere route (refined fuel + oxygen separated from the local
    // atmosphere, so no stockpiled oxidiser input) and the Era 1 airless route
    // (water electrolysis supplying the oxidiser). Liquid oxygen is folded into
    // both recipes rather than given its own enum value — nothing outside the
    // Chemical Plant would ever hold it.
    //
    // SAVE-FORMAT NOTE (BL-308 / BL-107): resource_type is a serialised width
    // and every per-resource array is sized off `count`, so APPENDING a value
    // renumbers nothing but DOES change every array's length. There is no
    // serialisation layer today and BL-107 (save magic + version header) has
    // not landed, so this append is free right now. It stops being free the
    // moment saves exist: from then on, a new resource_type value is a
    // save-format break and needs a version bump + migration.
    propellant            = 31, ///< Launch propellant; consumed per space-mode convoy dispatch.
    // --- Tier 2/3: processing chain roster (BL-340, 2026-08-11) ---
    //
    // Closes the minable-but-unsellable asymmetry on silica/copper_ore/
    // rare_earth_ore (base_price 0 before this) and gives the militia's
    // procurement contracts (BL-350) a terminal good to buy. Admission rule
    // applied (PRODUCTION.md / this item's design): every value here is
    // consumed by an authored recipe, or is a terminal object a named actor
    // contracts for. No orphans — see BL-286 for the cautionary precedent.
    silicon                = 32, ///< Refined from silica. Refinery.
    refined_copper         = 33, ///< Refined from copper ore. Smelter.
    ree_alloy              = 34, ///< Refined from rare earth ore. Refinery.
    machinery              = 35, ///< Steel + refined copper. Fabricator. Alloys' alternative path.
    alloys                 = 36, ///< Steel + REE alloy. Fabricator.
    electronics            = 37, ///< Silicon + refined copper + REE alloy. Electronics Lab.
    spacecraft_components  = 38, ///< Alloys + electronics. Assembly Plant. Terminal — the militia's
                                 ///< procurement contract object (BL-350); no background demand.
    // --- Habitability tranche (BL-368, 2026-08-11) ---
    //
    // The three habitability goods population centres actually consume as
    // tradeable goods (RESOURCES.md § Habitability goods). Building Materials
    // and Utilities are deliberately excluded — Building Materials feeds
    // construction cost (PRODUCTION.md's concern, a different consumption
    // path) and Utilities is an abstracted budget cost with no resource
    // identity of its own. Admission rule satisfied on both ends: population
    // centres are the consumer (inject_population_demand, market_clearing.cpp),
    // each has a producing recipe (recipes.lua).
    clean_water            = 39, ///< Water Treatment Plant, from water. Reduces habitability if undersupplied.
    consumer_goods         = 40, ///< Consumer Goods Factory, from food_rations + steel. Reduces workforce efficiency if undersupplied.
    medical_supplies       = 41, ///< Pharmaceutical Lab, from water + agricultural_produce. Reduces habitability if undersupplied.
    count                  = 42
};

static constexpr std::size_t resource_count = static_cast<std::size_t>(resource_type::count);

/// Material character of a tile — what it is made of. Determines which resource
/// deposits can appear, its terrain colour, and its base habitability ceiling.
/// One of the two axes of the tile model; see docs/economy/TILES.md.
enum class terrain_composition : uint8_t
{
    barren    = 0,  ///< Dry, dusty, minimal organics; iron ore, coal, petroleum.
    rocky     = 1,  ///< Hard rock outcrops; iron, copper, rare earth ore.
    volcanic  = 2,  ///< Geologically active; rare earth and iron ore. High hazard.
    icy       = 3,  ///< Ice-dominated surface; water ice. Low habitability.
    tundra    = 4,  ///< Cold, sparse vegetation; surface iron, peat.
    grassland = 5,  ///< Open fertile land; agricultural produce. Habitable.
    forest    = 6,  ///< Dense tree cover; timber. Habitable.
    wetland   = 7,  ///< Marsh, bog, floodplain; agricultural produce, clay. Habitable.
    ocean     = 8,  ///< Open deep water; carries no land deposits and no buildings.
    regolith  = 9,  ///< Loose surface material on airless bodies.
    metallic  = 10, ///< High metal content; iron-nickel ore, platinum group metals.
    urban     = 11, ///< BL-366: one-way transform, fired when a tile's non-extraction building
                    ///< stack fills its starting composition's cap. No new extraction or
                    ///< ambient-resource placement; existing extraction sites are grandfathered.
                    ///< High habitability ceiling; never reverts.
};

/// Physical shape of a tile — its elevation, slope, and form. Modifies the base
/// properties set by composition without changing them. The second axis of the
/// tile model; the build-cost multiplier notes follow docs/economy/TILES.md.
enum class terrain_landform : uint8_t
{
    plains   = 0, ///< Flat, easy access. ×1.0 build cost. Default for most tiles.
    highland = 1, ///< Elevated plateau, moderate slope. ×1.25.
    mountain = 2, ///< Steep peaks; difficult terrain. ×2.0. Boosts mineral richness.
    canyon   = 3, ///< Deep gorge, access from above. ×1.5. Erosion-exposed deposits.
    valley   = 4, ///< Low ground between higher terrain. ×1.1. Fertile.
    crater   = 5, ///< Impact basin; common on airless bodies. ×1.3.
    rift     = 6, ///< Geological fault zone. ×1.6. Strong volcanic association.
};

/// Celestial body classification.
enum class body_type : uint8_t
{
    planet   = 0,
    moon     = 1,
    asteroid = 2,
    station  = 3,
    star     = 4, ///< The system's central star. Sits at the centre, stationary; has no surface.
};

/// Type of surface installation.
enum class building_type : uint8_t
{
    none                = 0,
    extraction_site     = 1,
    processing_facility = 2,
    port                = 3,
    launchpad           = 4, ///< Space-launch facility; gates space-mode convoy dispatch.
    inland_logistics_hub = 5, ///< BL-149: land-mode logistics node; its tile discounts intra-body haul cost (like a city).
    military_base       = 6, ///< BL-325 S1: unit muster building. Produces nothing, staffs at zero,
                             ///< and is deliberately NOT a supply anchor — military reach IS the
                             ///< economic reach field (BL-325 ruling 3). Hire moves onto it in S2.
    research_institute  = 7, ///< BL-332: the dedicated "how does tech get done" building. Passive
                             ///< like military_base (produces nothing tradeable, staffs at zero) —
                             ///< its output is a flat per-tick credit to its owning corp's
                             ///< `corporation_component::science`, a market-invisible accumulator,
                             ///< not a resource_type. See economy_system.cpp's capability-points pass.
};

/// One past the last building type — the wire parser's range gate (BL-396:
/// run_serve refuses any `type=` outside [0, building_type_count) rather than
/// letting a narrowing cast truncate it into a type the caller never named,
/// or index the per-type economics table out of bounds). Derived from the
/// enum's tail, the same way resource_count derives from resource_type::count:
/// appending a type means moving this with it.
static constexpr uint8_t building_type_count =
    static_cast<uint8_t>(building_type::research_institute) + 1;

/// Sentinel `building_component.recipe` value meaning "no processing recipe is
/// assigned" — used by extraction sites and unconfigured processors. The recipe
/// id is otherwise an index into the recipe registry (see recipe_registry.hpp).
static constexpr uint16_t no_recipe = 0xFFFFu;

// ---------------------------------------------------------------------------
// Component structs
// ---------------------------------------------------------------------------

/// Fixed physical description of a tile. Properties are authored at world
/// creation and never mutated during play.
///
/// resource_deposit is indexed by static_cast<std::size_t>(resource_type).
/// A zero value means the resource is absent on this tile.
struct tile_component
{
    entity_id  body;      ///< Body this tile belongs to.
    int        grid_x;    ///< Column index within the body's tile grid.
    int        grid_y;    ///< Row index within the body's tile grid.
    terrain_composition composition; ///< Material character (geology/ecology).
    terrain_landform    landform;    ///< Physical shape (elevation/slope).
    /// Fixed deposit **richness** per resource type (the extraction rate multiplier).
    /// Value-initialised for the same reason as market_component's arrays below: a
    /// resource no generation rule authors must read as zero, not as stack garbage.
    std::array<float, resource_count> resource_deposit = {};

    /// Finite depletion reserve per resource type. **Live** — extraction draws this
    /// down each tick (run_extraction, economy_system.cpp): richness sets the rate,
    /// this reserve is consumed and tapers the output as it nears empty, at which
    /// point the building reports the deposit exhausted. Seeded at generation to
    /// richness × a reserve factor. Indexed by resource_type, as above.
    std::array<float, resource_count> resource_remaining = {};

    float      hazard_level;     ///< 0.0 (safe) – 1.0 (extreme hazard).
    float      habitability;     ///< 0.0 (uninhabitable) – 1.0 (hospitable).
    float      substrate_density = 0.0f; ///< Background nation industrial occupation [0, 1].

    /// Road tier on this tile (BL-077 field; BL-172 ladder). 0 = no road; 1 = Track, 2 = Road,
    /// 3 = Highway. Higher tiers lower the intra-body A* traversal cost of the tile
    /// (road_traversal_multiplier: x0.67 / x0.50 / x0.40). Populated by BL-146/BL-172 road
    /// generation and BL-147/BL-172 player placement; 0 everywhere in the economic core.
    std::uint8_t road_level = 0;

    /// River edge bitmask (BL-170). Bit i (0-5) set means this tile's hex side i carries a
    /// river crossing. Side ordering: 0=E, 1=NE, 2=NW, 3=W, 4=SW, 5=SE (odd-r offset — see
    /// river_generation.hpp). A river is an EDGE, never a tile — no lake/water tile feature
    /// is implied by this field. Written by generate_rivers (src/world/river_generation.cpp),
    /// a sibling pass run after generate_body_tiles; 0 on every body until that pass runs.
    std::uint8_t river_edges = 0;

    /// Per-side flow direction for the bits set in river_edges (BL-170): bit i set means side i
    /// is the DOWNSTREAM (outflow) direction from this tile; bit i clear (but river_edges bit i
    /// set) means side i is the upstream (inflow) direction. Meaningless for bits not set in
    /// river_edges. See river_edge_discount (src/world/river_generation.hpp) for how this feeds
    /// the intra-body logistics cost.
    std::uint8_t river_downstream = 0;
};

/// Survey lifecycle of a body (BL-067, docs/ui/SOLAR.md § Survey badge).
/// A body starts `hidden` — the player knows only its type, orbital position and
/// grid size. Dispatching a survey moves it through `in_transit` (probe en route,
/// nothing revealed) and `scanning` (regions revealed one at a time) to `surveyed`.
enum class survey_phase : uint8_t { hidden, in_transit, scanning, surveyed };

/// Per-body survey progress. Carried inline on `body_component` (bodies are few).
/// The reveal is deterministic: `regions_total` partitions the grid by a fixed
/// super-cell size, regions reveal in raster (row-major) order, and a tile is
/// visible iff its region's reveal-index < `regions_done`. No RNG anywhere — the
/// order is a pure function of grid dimensions (see survey_system.hpp).
struct survey_state
{
    survey_phase phase           = survey_phase::hidden;
    int          regions_total   = 0;  ///< Computed at dispatch from the grid + super-cell size.
    int          regions_done    = 0;  ///< Regions revealed so far, in raster order.
    int          ticks_remaining = 0;  ///< Days until the next phase / region boundary.
};

/// A celestial body — the primary unit of territorial control and the location
/// where extraction, market activity, and conflict occur.
///
/// Orbital model: a body orbits the star unless `parent` is set, in which case
/// it orbits that parent body (a moon around its planet). `orbital_radius_au`
/// and `orbital_angle_rad` are interpreted relative to whichever centre applies.
/// `orbital_angle_rad` advances each frame by `orbital_angular_velocity_rad_per_day`
/// (see advance_orbits in orbital_system.hpp); the authored value is the phase
/// at world construction, frozen into `orbital_epoch_angle_rad` so the econ tick
/// can reconstruct positions purely from the day tick (orbital_angle_at_tick).
struct body_component
{
    std::string name;
    body_type   type;
    entity_id   parent = null_entity; ///< Body this one orbits; null_entity = orbits the star directly.
    float       orbital_radius_au;    ///< Orbital distance in AU — from the star, or from `parent` if set.
    float       orbital_angle_rad;    ///< Current angular position, radians. 0 = right, increases counter-clockwise.
    float       orbital_angular_velocity_rad_per_day = 0.0f; ///< Angular speed; advances orbital_angle_rad over time. 0 = stationary.
    float       orbital_epoch_angle_rad = 0.0f; ///< Phase at world construction (tick 0); never advanced. Basis for orbital_angle_at_tick.
    int         grid_width;           ///< Number of tile columns.
    int         grid_height;          ///< Number of tile rows.

    /// Body mass in Earth masses, carried down from planetology's `home_mass`
    /// (Ben, 2026-08-12). Added so the PHYSICAL tile scale can be derived from
    /// the generation chain rather than authored beside it: a rocky planet's
    /// radius follows its mass, the radius gives a circumference, and the
    /// circumference over `grid_width` gives kilometres per tile
    /// (`body_km_per_tile`, logistics.hpp).
    ///
    /// Defaults to Earth mass so a body built without one still has a sane
    /// scale rather than a zero that would read as "instant travel".
    float       mass_earths = 1.0f;

    /// Survey progress (BL-067). `home_body` is seeded `surveyed`; all others open
    /// `hidden` until the player dispatches a survey. Advanced by advance_surveys
    /// (survey_system.hpp); read by the Solar/Planetary canvases and the Selection panel.
    survey_state survey;
};

/// Surface installation stub. Combat and production logic are added in later
/// layers; this struct exists so the data model does not need retrofitting.
struct building_component
{
    entity_id     tile;               ///< Tile this building occupies.
    building_type type;
    float         workforce_assigned; ///< 0.0–1.0 fraction of allocated workforce.

    /// Extraction target — the resource this building harvests from its tile's
    /// deposit. Authored at placement; meaningful only for extraction_site.
    /// What a building *does* is this field, not the generic building_type.
    resource_type target_resource = resource_type::iron_ore;

    /// Processing recipe id — an index into the recipe registry (recipe_registry.hpp).
    /// Authored at placement and fixed at construction; meaningful only for
    /// processing_facility. `no_recipe` means none assigned.
    uint16_t      recipe = no_recipe;

    /// Player-authored workforce target as a percentage of nominal capacity.
    /// Range [0, 200]; 100 = full nominal staffing. Scales actual output and
    /// the labour portion of maintenance. Applied in economy_system and budget_system.
    /// When `workforce_auto` is set, the economy tick overwrites this each tick with
    /// the profit-maximising value (BL-181); a manual override clears the auto flag.
    int  workforce_target = 100;

    /// BL-181: when true (default), the economy tick auto-solves `workforce_target`
    /// to maximise this building's estimated net profit (player corp only). A manual
    /// workforce-target choice in the management UI clears this, pinning the target;
    /// the "Auto" control re-enables it. Reconciles with the "never auto-act the
    /// player's corp" rule as an opt-out convenience (see io-standing-rules.md).
    bool workforce_auto = true;

    /// When true the building produces nothing and is charged only material
    /// (fixed overhead) maintenance — no labour cost. Pending removal by the
    /// player. Set by the building-management UI (construction_panel).
    bool decommissioned = false;

    /// Index of the active recipe in the building type's recipe list. 0 = default.
    /// Used by the management UI; the economy system continues to use the `recipe`
    /// field (the registry-wide index), which is updated when the player selects a
    /// different recipe via `active_recipe_index`.
    int  active_recipe_index = 0;

    /// Economy ticks left before this building completes construction and starts
    /// producing (playtest patch, 2026-07-06 — a build-spree headless check found
    /// the player could place a dozen-plus buildings in a single instant burst
    /// with zero ticks elapsed, since construction had no time cost at all; this
    /// is the pacing gate). Set from `building_economics::build_duration_ticks`
    /// at placement; decremented (BL-095: by whole units as sub-tick progress
    /// accumulates) each economy tick until it reaches 0, at which point the
    /// building is built and operational.
    int  ticks_remaining = 0;

    /// BL-095: sub-tick construction progress accumulator in [0, 1). Each tick the
    /// build advances by a rate in [0, 1] set by how much of its per-tick material
    /// need the local market can supply (full/stretched/paused); when the
    /// accumulator crosses 1.0 one whole `ticks_remaining` unit is consumed. This
    /// keeps `ticks_remaining` an integer gate while allowing a material-starved
    /// build to stretch over many ticks. Not meaningful once ticks_remaining == 0.
    float construction_progress = 0.0f;

    /// BL-079: consecutive economy ticks this building has been estimated
    /// loss-making. Read only by the scoped background-corp agency (a non-player corp
    /// idles a building whose streak passes a threshold); reset to 0 on any
    /// profitable tick. Never advanced for the player's own buildings.
    int  loss_streak = 0;

    /// BL-202: strategic-scorer cooldown, in owning-corp evaluations. Set when
    /// the scored utility layer touches this building (dial change / build);
    /// while > 0 the building is not a dial candidate (the anti-thrash rule,
    /// AI_OPPONENT.md § 5 Hysteresis). Decremented each evaluation of the
    /// owning corp. Never set on player buildings.
    int  ai_cooldown = 0;

    /// BL-430: economy ticks remaining before this building's recipe may be
    /// switched again through the PLAYER-grade seam (corp_command's set_recipe
    /// verb, and the construction_panel UI, which shares its gate). Set from
    /// recipe_switch_params::cooldown_ticks on a successful switch through that
    /// seam; decremented once per economy tick (run_economy_step). NOT touched by
    /// the BL-079 reflex switch, which stays free/instant — see recipe_switch_params.
    int  recipe_switch_cooldown = 0;
};

/// Pooled resource quantities held by an entity.
/// Used for building output buffers, convoy cargo, and similar stores.
///
/// Indexed by static_cast<std::size_t>(resource_type).
struct stockpile_component
{
    std::array<float, resource_count> quantities = {};
};

/// Local exchange for a single body. Supply, demand, and prices resolve at
/// each economy tick boundary.
///
/// All arrays are indexed by static_cast<std::size_t>(resource_type).
struct market_component
{
    entity_id body;
    /// Tile this market is centred on (usually a body's principal/capital tile).
    /// A body may carry **several** markets, each with a distinct centre; a tile
    /// clears against the market whose centre is nearest (see `market_for_tile`).
    /// `null_entity` = unanchored: a body with a single market routes to it
    /// regardless of centre (the degenerate, behaviour-preserving case).
    entity_id centre_tile = null_entity;
    // VALUE-INITIALISED DELIBERATELY (2026-08-04). These four carried no
    // initialiser and relied on every slot being written at world creation.
    // That held only while the authored resource set covered the whole enum:
    // BL-286 added eight goods nothing authors yet, so their slots kept
    // whatever was on the stack — which surfaced as a NaN price propagating
    // into prospective_profit's revenue estimate, in Release only (Debug
    // fills fresh allocations with a pattern and hid it). An unauthored
    // resource must read as zero, not as garbage.
    std::array<float, resource_count> supply = {};
    std::array<float, resource_count> demand = {};
    std::array<float, resource_count> price = {};      ///< Current resolved price; set to base_price until first tick.
    std::array<float, resource_count> base_price = {}; ///< Rarity-derived floor; authored at world creation.

    /// BL-130: REAL persistent per-resource stock — not reset each tick (unlike
    /// supply/demand above, which stay per-tick FLOW figures for price
    /// resolution/reporting). Fills from this tick's sell-side clearing
    /// (`clear_markets`, after all auto-surplus/standing-sell listing);
    /// drains during the PRIOR phase of the same economy tick — production
    /// (`run_processing`) and construction (`run_construction`), both of which
    /// run before `clear_markets` — against whatever stock is left from
    /// PRIOR ticks' sales. A processor or a build can therefore only draw what
    /// the market genuinely has on hand; it is no longer an unconditional,
    /// infinite auto-buy. See docs/economy/MARKETS.md § Real market inventory.
    std::array<float, resource_count> inventory = {};
};

/// A standing sell order — the manual side of the market. Each economy tick the
/// order lists up to `quantity` of `resource` from the (corp, body) pool for sale
/// at no less than `floor_price` (the order clears at `max(resolved_price,
/// floor_price)`; an unmet floor simply means less or nothing sells that tick).
/// Defined here (rather than in market_clearing.hpp) so the clearing system, the
/// command seam and the UI can all name it without an include cycle.
///
/// HELD IN `world::sell_orders` (BL-293, 2026-08-07). It used to live in
/// `ui_state` and be handed to `clear_markets` by the caller, which made it
/// unreachable by `corp_command` (a verb mutates `world&`, and there was nothing
/// in the world to mutate) and invisible to the save seam. It is world state now:
/// the player and a rival corp place orders through the same verb, and the
/// clearing tick reads the book itself.
///
/// SAVE-FORMAT RECORD — see order_book.hpp. `id` leads the struct because it is
/// the order's identity, and identity is what `remove_sell_order` names; the
/// remaining fields keep their original order and meaning.
struct sell_order
{
    /// Stable handle, allocated by `world::allocate_order_id()`. Nonzero on any
    /// order that has been placed; 0 marks a default-constructed order that never
    /// entered the book. Stable across a tick, an erase of a *different* order,
    /// and a save/load round-trip — which an index into the vector is not.
    uint32_t      id          = 0;
    entity_id     corp        = null_entity;
    entity_id     body        = null_entity;
    resource_type resource    = resource_type::iron_ore;
    float         quantity    = 0.0f;
    float         floor_price = 0.0f; ///< Minimum acceptable unit price; 0 = sell at the market price.
};

/// A buy order for one resource — the demand side of the order book. Each
/// economy tick the clearing system matches buy orders against sell orders
/// by price-time priority (cheapest seller first; highest bidder first).
/// Held in `world::buy_orders`, alongside `sell_orders`; see that struct for why
/// the book is world state and what `id` is for. No press authors one yet — the
/// buy side exists in the clearing algorithm and in the save format, waiting for
/// its verb.
struct buy_order
{
    uint32_t      id               = 0;           ///< Stable handle; see sell_order::id.
    entity_id     corp             = null_entity;
    entity_id     body             = null_entity;
    resource_type resource         = resource_type::iron_ore;
    float         quantity         = 0.0f;
    float         max_price        = 0.0f; ///< Maximum acceptable unit price; 999 = pay anything.
    entity_id     preferred_seller = null_entity; ///< Optional counterparty preference.
};

// Save-format guards, following `molecular_event`'s precedent in
// chemistry_tables.hpp: the order book is written field-by-field rather than as a
// raw blob, but a silent layout change is still the failure mode a stale save
// exhibits, and it is far easier to diagnose at compile time than at load time.
// Tripping one of these means the record changed — bump `order_book_version`
// (order_book.hpp) in the same edit, then update the size here.
static_assert(sizeof(sell_order) == 24, "sell_order is a save-format record — see order_book.hpp");
static_assert(sizeof(buy_order)  == 28, "buy_order is a save-format record — see order_book.hpp");

/// A live price quote (BL-350) — the answer to `request_quote`, before it is
/// accepted into a `procurement_contract`. Parallel to the order book rather
/// than an entry in it (that item's Q4): the book is price-time priority over
/// ANONYMOUS asks, with no representation for a named counterparty or a lead
/// time. Held in `world::procurement_quotes`; `accept_quote` consumes one by
/// id, converting it into a contract.
struct procurement_quote
{
    uint32_t      id           = 0;              ///< Stable handle; see sell_order::id.
    entity_id     buyer        = null_entity;
    entity_id     supplier     = null_entity;
    entity_id     body         = null_entity;    ///< Where the supplier fulfils from.
    resource_type resource     = resource_type::iron_ore;
    float         quantity     = 0.0f;
    float         unit_price   = 0.0f;           ///< Quoted at request time; locked in on accept.
    int32_t       lead_time_ticks = 0;            ///< `base_ticks x ceil(quantity / throughput)`.
};

/// An accepted procurement contract (BL-350) — "a build order placed with
/// someone else", BL-095's pay-as-you-build shape with the materials drawn
/// against the SUPPLIER's market and the output delivered to the BUYER's pool.
/// Held in `world::procurement_contracts`.
struct procurement_contract
{
    uint32_t      id              = 0;             ///< Stable handle; see sell_order::id.
    entity_id     buyer           = null_entity;
    entity_id     supplier        = null_entity;
    entity_id     body            = null_entity;
    resource_type resource        = resource_type::iron_ore;
    float         quantity        = 0.0f;
    float         unit_price      = 0.0f;
    int32_t       lead_time_ticks = 0;
    int32_t       ticks_elapsed   = 0;
    float         deposit_paid    = 0.0f; ///< Already debited at accept_quote (economy.procurement.deposit_fraction).
};

static_assert(sizeof(procurement_quote)    == 32, "procurement_quote is a save-format record — see procurement.hpp");
static_assert(sizeof(procurement_contract) == 40, "procurement_contract is a save-format record — see procurement.hpp");

/// Land-use classification of a tile or zone. Drives the trade-off between
/// residential, industrial, agricultural, and undeveloped land.
/// See docs/economy/POPULATION.md § Land-use trade-offs.
struct land_use_component
{
    enum class type { residential, industrial, agricultural, wilderness, infrastructure };
    type use = type::wilderness;
};

/// A population centre occupying a tile. Scale governs agglomeration bonuses
/// and workforce supply; population is the absolute headcount in thousands;
/// habitability is a 0–1 scalar read from the tile at generation and cached here.
/// No AI behaviour in the prototype — data-model only.
/// See docs/economy/POPULATION.md.
struct population_centre_component
{
    int   scale              = 1;    ///< 1–5 (village → metropolis).
    int   population         = 0;    ///< Absolute headcount in thousands.
    float habitability       = 1.0f; ///< 0–1 scalar inherited from the tile.
    int   growth_accumulator = 0;    ///< Ticks of qualifying growth; resets on level-up.
};

/// Deployable unit stub. Faction AI and transport are deferred; the combat
/// engine (BL-272, see world/combat.hpp) reads `type` when a stack is
/// resolved into an army_stack_entry for resolve_battle. `type` is an opaque
/// index into whichever roster table the current era supplies (BL-274 owns
/// rosters, not this file) — unit_component itself does not interpret it.
///
/// BL-157 ruled tile position canonical over body/province (2026-08-07); this
/// struct shipped with a `body` field ahead of that ruling — `position`
/// (BL-324, 2026-08-08) is the fix, landed with the hire-unit item that first
/// needed tile grain rather than deferring it back to BL-157.
struct unit_component
{
    entity_id position;      ///< Tile the unit currently occupies (BL-157: tile-canonical).
    entity_id owner;         ///< Corporation or faction entity that controls this unit.
    int       count;         ///< Number of units in the group.
    uint16_t  type = 0;      ///< Opaque roster-type index; see combat.hpp's army_stack_entry.
    int32_t   strength = 0;  ///< Fixed-point combat strength scalar (BL-157). Not a stat block —
                              ///< a single number, same spirit as building_component's own scalars.
};

// ---------------------------------------------------------------------------
// Convoy component
// ---------------------------------------------------------------------------

/// A goods convoy in transit between two markets. Created at dispatch, retired on
/// arrival. Progress advances per Tick via advance_convoys (supply_system.hpp).
/// Cargo is committed from the source pool at dispatch; destination is credited on
/// arrival. See docs/development/BACKLOG.md § BL-039 and supply_system.hpp.
enum class convoy_mode : uint8_t { land = 0, sea = 1, air = 2, space = 3 };

struct convoy_component
{
    entity_id   source_market  = null_entity; ///< Market the cargo was dispatched from.
    entity_id   dest_market    = null_entity; ///< Market the cargo is bound for.
    convoy_mode mode           = convoy_mode::land;
    resource_type cargo_resource = resource_type::iron_ore;
    float       cargo_qty      = 0.0f;
    float       progress       = 0.0f; ///< 0.0 (just dispatched) → 1.0 (arrived).
    float       speed          = 0.0f; ///< Progress increment per Tick (1/distance in ticks).
    entity_id   corp           = null_entity; ///< Dispatching corporation.
    bool        arrived        = false; ///< Set true when progress >= 1.0; retirement pending.
};

// ---------------------------------------------------------------------------
// Trade route (BL-088)
// ---------------------------------------------------------------------------

/// A persistent body-pair trade lane that a corporation's commerce has actually
/// run. Unlike convoy_component (transient — created at dispatch, erased on
/// arrival), a trade_route is **never erased**: it accumulates and carries a
/// last-traffic stamp so a reader can age it to 'stale' without any deletion
/// logic. Keyed on the *unordered* (body_a, body_b) pair plus the dispatching
/// corp. Upserted in credit_arrived_convoys when a convoy completes a lane
/// between two distinct bodies. Consumed by the commercial-sphere fog (BL-089),
/// which reads the player's routes to light the Solar canvas.
/// See docs/economy/SUPPLY.md and supply_system.hpp.
struct trade_route
{
    entity_id body_a       = null_entity; ///< One endpoint body (unordered relative to body_b).
    entity_id body_b       = null_entity; ///< The other endpoint body.
    entity_id corp         = null_entity; ///< Dispatching corporation (the fog reads the player's).
    int       last_tick    = 0;           ///< Day (sim day tick) a convoy last COMPLETED this lane; drives stale-aging.
    int       convoy_count = 0;           ///< Cumulative completions on this lane (telemetry / future volume model).
};

// ---------------------------------------------------------------------------
// Corporation enumerations
// ---------------------------------------------------------------------------

/// Primary industrial focus of a corporation. Semantically distinct from the
/// nation-level economic_focus even though the values mirror it — they model
/// different concepts (corporate strategy vs. national economic policy) and
/// are intentionally kept as separate types so they can diverge independently.
enum class industrial_focus : uint8_t
{
    extraction  = 0, ///< Raw-material producers; prefer resource-rich land tiles.
    processing  = 1, ///< Refiners and converters; prefer proximity to extraction clusters.
    trade       = 2, ///< Logistics and market operators; prefer port-adjacent tiles.
};

// ---------------------------------------------------------------------------
// Corporation component
// ---------------------------------------------------------------------------

/// All persistent data describing a single corporation at campaign start.
/// Corporations take no autonomous actions in the prototype; this struct is
/// generation output only. See docs/generation/CORPORATION_GENERATION.md and
/// docs/development/backlog.json for the deferred behaviour design.
struct corporation_component
{
    /// Generated name produced by Pass 5 of the corporation generation pipeline.
    std::string name;

    /// Nation entity this corporation is legally registered in.
    /// Set by Pass 1 (nation assignment); never null after generation.
    entity_id home_nation = null_entity;

    /// Primary industrial focus; drawn by Pass 2 (focus assignment).
    industrial_focus focus = ::industrial_focus::extraction;

    /// Starting capital in the economy's base currency unit; set by Pass 4.
    float starting_capital = 0.0f;

    /// Running cash balance, moved by the economy money loop (budget_system.hpp).
    /// Opens at `starting_capital`; may go negative (no insolvency consequence in
    /// the prototype — flagged red in the economy panel).
    float balance = 0.0f;

    /// True for exactly one corporation per campaign — the human player's
    /// corporation. Set after all corps are generated.
    bool is_player = false;

    /// True for a BL-365 background firm — a real corporation generated purely
    /// to fill the body's production/demand gap (the old abstract nation-substrate
    /// injection's replacement). A corp is exactly one of player / rival /
    /// background — is_player and is_background are never both true. Background
    /// firms are otherwise ordinary corporations: real buildings, real recipes,
    /// real stockpiles, picked up by corp_ai.cpp's uniform non-player iteration
    /// like any rival.
    bool is_background = false;

    /// Building entity IDs owned by this corporation at campaign start.
    /// Populated by Pass 3 (starting asset placement); each entry has a
    /// building_component and a stockpile_component in the world.
    std::vector<entity_id> assets;

    /// Corporate border foundation (BL-182). The **HQ** is the corp's seat — the
    /// holding nearest its holdings centroid on the home body, designated at
    /// generation (Pass 3b) — and **influence_range** is the radius of its
    /// HQ-projected border in *unit-hex* distance (hex_size = 1), so a renderer
    /// obtains pixels by `influence_range * hex_size * zoom`, matching
    /// `ui::hex_local_centre`'s geometry exactly. Together they let every corp draw
    /// a border off real data rather than a render-time recompute. A corp with no
    /// holdings on its home body keeps `{null_entity, 0}` — no border. **Render-only
    /// today**: the border gates nothing until the deferred BL-182 operate-gate
    /// lands (post-v0.1.0). See docs/generation/CORPORATION_GENERATION.md, LENSES.md.
    ///
    /// **The border RING itself was retired 2026-08-08 (BL-329)** — Ben's live
    /// critique: a fixed-radius circle that never grew as the player built
    /// outward showed nothing informative once the BL-323 reach fog existed to
    /// show supply reach properly. `influence_range` is still computed and
    /// stored (a future operate-gate may want it); only `body_surface_canvas.cpp`'s
    /// `draw_corp_hq` (formerly `draw_corp_border`) stopped drawing it — the HQ
    /// marker glyph itself is unaffected.
    entity_id hq_building     = null_entity;
    float     influence_range = 0.0f;

    /// Capability-point accumulators (BL-332). Stockpiled, market-invisible,
    /// never decaying — closer to "research points" than a resource_type
    /// good (no market slot, no price, not held in a (corp, body) pool).
    /// military_points is produced passively by every completed military_base
    /// the corp owns (Ben, 2026-08-08: "produced by bases"); science by every
    /// completed research_institute. Symmetric across every corp — player,
    /// rival, background — matching the same-clock extension BL-324 gave
    /// hire_unit. No spend mechanism reads either yet: BL-087's constellation
    /// (band progress) is the intended sink, not built by this item.
    float military_points = 0.0f;
    float science          = 0.0f;
};

// ---------------------------------------------------------------------------
// Nation enumerations
// ---------------------------------------------------------------------------

/// Broad political orientation of a nation. Seeds the starting sentiment graph
/// and the tone of diplomatic interactions between nations.
enum class ideology : uint8_t
{
    authoritarian = 0, ///< Centralised state authority; hostile to mercantile/technocratic neighbours.
    technocratic  = 1, ///< Governance by technical expertise; cooperative with innovation-focused peers.
    mercantile    = 2, ///< Trade-first policy; favours open markets and corporation access.
    isolationist  = 3, ///< Minimises external interaction; low base sentiment toward all neighbours.
};

/// Military and territorial posture of a nation at campaign start.
enum class expansionism : uint8_t
{
    passive    = 0, ///< Defensive; does not initiate border pressure.
    moderate   = 1, ///< Opportunistic; will contest resources near existing borders.
    aggressive = 2, ///< Proactively pressures neighbours and contests unclaimed zones.
};

/// Dominant economic activity of a nation; biases infrastructure investment
/// and the resource demands it places on corporations operating within it.
enum class economic_focus : uint8_t
{
    extraction  = 0, ///< Raw-material economy; values ore and agricultural output.
    processing  = 1, ///< Industrial economy; demands refined goods and energy inputs.
    trade       = 2, ///< Commercial economy; prioritises port capacity and market access.
};

// ---------------------------------------------------------------------------
// Nation component
// ---------------------------------------------------------------------------

/// All persistent data describing a single nation at campaign start. Nations
/// take no autonomous actions in the prototype; this struct is generation output
/// only. See docs/generation/NATION_GENERATION.md and docs/development/backlog.json
/// for the deferred behaviour design.
struct nation_component
{
    /// Generated name produced by Pass 5 of the nation generation pipeline.
    std::string name;

    /// Ordered list of tile entity IDs controlled by this nation.
    /// Populated by Pass 2 (territory expansion) and stable thereafter.
    std::vector<entity_id> tiles;

    /// Summed resource deposit profile across all owned tiles, indexed by
    /// static_cast<std::size_t>(resource_type). Computed in Pass 3.
    /// A value of 0.0 means the resource is absent from the nation's territory.
    std::array<float, resource_count> resource_abundance{};

    /// Political orientation; drawn from seeded RNG in Pass 4.
    /// Named `politics` (not `ideology`) so the field does not shadow its enum
    /// type — a field whose name matches its type is ill-formed under GCC
    /// (-Wchanges-meaning) and breaks the Linux build. Mirrors the
    /// tile_component convention (terrain_composition composition).
    ideology       politics = ::ideology::mercantile;

    /// Military / territorial posture; drawn from seeded RNG in Pass 4.
    expansionism   posture  = ::expansionism::passive;

    /// Dominant economic activity; drawn from seeded RNG in Pass 4.
    economic_focus focus    = ::economic_focus::extraction;
};
