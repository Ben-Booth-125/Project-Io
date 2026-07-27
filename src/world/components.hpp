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
    count                 = 23
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
};

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
    std::array<float, resource_count> resource_deposit; ///< Fixed deposit **richness** per resource type (the extraction rate multiplier).

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
};

/// Background nation-owned economy baseline for one (nation, body) pair. Carries
/// only *generation* coefficients; the live demand/supply is derived at tick time
/// by inject_substrate_demand from these plus the economy.substrate tunables
/// (BL-078). Injected into the body's markets each economy tick to give them
/// liquidity and a price-discovering demand/supply model.
struct nation_substrate
{
    /// Abstract production capacity per resource on this body, derived from
    /// owned-tile deposits weighted by population proximity (Σ density·deposit).
    /// Scaled by economy.substrate.capacity_scale at tick time to cap supply, so a
    /// resource the nation lacks the deposit for leaves a live, fillable gap.
    std::array<float, resource_count> capacity = {};
    /// Catchment population/economic weight (Σ of the density ripple over owned
    /// tiles) that drives the per-capita basket demand at tick time.
    float population_weight = 0.0f;
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
/// at world construction.
struct body_component
{
    std::string name;
    body_type   type;
    entity_id   parent = null_entity; ///< Body this one orbits; null_entity = orbits the star directly.
    float       orbital_radius_au;    ///< Orbital distance in AU — from the star, or from `parent` if set.
    float       orbital_angle_rad;    ///< Current angular position, radians. 0 = right, increases counter-clockwise.
    float       orbital_angular_velocity_rad_per_day = 0.0f; ///< Angular speed; advances orbital_angle_rad over time. 0 = stationary.
    int         grid_width;           ///< Number of tile columns.
    int         grid_height;          ///< Number of tile rows.

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
    std::array<float, resource_count> supply;
    std::array<float, resource_count> demand;
    std::array<float, resource_count> price;      ///< Current resolved price; set to base_price until first tick.
    std::array<float, resource_count> base_price; ///< Rarity-derived floor; authored at world creation.
};

/// A player-authored standing sell order — the manual side of the market. Each
/// economy tick the order lists up to `quantity` of `resource` from the (corp,
/// body) pool for sale at no less than `floor_price` (the order clears at
/// `max(resolved_price, floor_price)`; an unmet floor simply means less or nothing
/// sells that tick). Held in `ui_state.sell_orders` and passed to `clear_markets`.
/// Defined here (rather than in market_clearing.hpp) so both the UI state and the
/// clearing system can name it without an include cycle.
struct sell_order
{
    entity_id     corp        = null_entity;
    entity_id     body        = null_entity;
    resource_type resource    = resource_type::iron_ore;
    float         quantity    = 0.0f;
    float         floor_price = 0.0f; ///< Minimum acceptable unit price; 0 = sell at the market price.
};

/// A buy order for one resource — the demand side of the order book. Each
/// economy tick the clearing system matches buy orders against sell orders
/// by price-time priority (cheapest seller first; highest bidder first).
/// Defined alongside sell_order so both the UI and clearing system can name it.
struct buy_order
{
    entity_id     corp             = null_entity;
    entity_id     body             = null_entity;
    resource_type resource         = resource_type::iron_ore;
    float         quantity         = 0.0f;
    float         max_price        = 0.0f; ///< Maximum acceptable unit price; 999 = pay anything.
    entity_id     preferred_seller = null_entity; ///< Optional counterparty preference.
};

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

/// Deployable unit stub. Combat rules, faction AI, and transport are deferred;
/// this struct exists so the field-level data model is in place.
struct unit_component
{
    entity_id body;  ///< Body where the unit is currently located.
    entity_id owner; ///< Corporation or faction entity that controls this unit.
    int       count; ///< Number of units in the group.
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
    entity_id hq_building     = null_entity;
    float     influence_range = 0.0f;
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
