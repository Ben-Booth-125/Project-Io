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
    // --- Ancient intermediates (BL-286, 2026-08-04) ---
    //
    // REMOVED 2026-08-16 (Ben's call on NR-257, option B): grain, fodder, salt,
    // transport_capacity and bullion were five of BL-286's eleven values, added
    // with "behaviour unfiled" and still, twelve days later, produced by nothing,
    // yielded by no deposit and consumed by nothing. chain_depth's R1 row found
    // them; they are gone rather than exempted, and the ids below shifted down
    // by five to close the gap.
    //
    // Re-adding one is an APPEND at the end of the enum with its behaviour filed
    // in the same change — never a re-insertion here, which would repoint every
    // id after it. The admission rule (PRODUCTION.md, BL-340) is the standing
    // answer: a value earns its place by being consumed by an authored recipe or
    // contracted for by a named actor, and nothing else gets in.
    charcoal              = 23, ///< Refined fuel-wood; pre-coal smelting/heating input.
    iron_blooms           = 24, ///< Bloomery-refined iron intermediate — distinct from raw iron/iron-nickel ore.
    trade_goods_misc      = 25, ///< Placeholder endemic-luxury-class good; a specific luxury name is a later design step.
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
    // renumbers nothing but DOES change every array's length. When BL-340
    // appended here there was no serialisation layer and the append was free.
    // IT IS NOT FREE ANY MORE: BL-107 landed 2026-08-22 (world_save.cpp, magic
    // + `world_save_version`), so from then on a new resource_type value is a
    // save-format break and needs a version bump + migration.
    propellant            = 26, ///< Launch propellant; consumed per space-mode convoy dispatch.
    // --- Tier 2/3: processing chain roster (BL-340, 2026-08-11) ---
    //
    // Closes the minable-but-unsellable asymmetry on silica/copper_ore/
    // rare_earth_ore (base_price 0 before this) and gives the militia's
    // procurement contracts (BL-350) a terminal good to buy. Admission rule
    // applied (PRODUCTION.md / this item's design): every value here is
    // consumed by an authored recipe, or is a terminal object a named actor
    // contracts for. No orphans — see BL-286 for the cautionary precedent.
    silicon                = 27, ///< Refined from silica. Refinery.
    refined_copper         = 28, ///< Refined from copper ore. Smelter.
    ree_alloy              = 29, ///< Refined from rare earth ore. Refinery.
    machinery              = 30, ///< Steel + refined copper. Fabricator. Alloys' alternative path.
    alloys                 = 31, ///< Steel + REE alloy. Fabricator.
    electronics            = 32, ///< Silicon + refined copper + REE alloy. Electronics Lab.
    spacecraft_components  = 33, ///< Alloys + electronics. Assembly Plant. Terminal — the militia's
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
    clean_water            = 34, ///< Water Treatment Plant, from water. Reduces habitability if undersupplied.
    consumer_goods         = 35, ///< Consumer Goods Factory, from food_rations + steel. Reduces workforce efficiency if undersupplied.
    medical_supplies       = 36, ///< Pharmaceutical Lab, from water + agricultural_produce. Reduces habitability if undersupplied.
    // --- The military terminal good (BL-457, 2026-08-17) ---
    //
    // The 2026-08-10 refocus (NR-120) put the player as a militia whose trade is
    // "coloured directly with military use, and space equipment". The space half
    // landed with BL-340 — the chain terminates in spacecraft_components, which
    // BL-350's procurement contracts buy. The military half had NO OBJECT AT ALL
    // until this value: no ordnance, no munitions, no small arms anywhere in the
    // roster. It went unnoticed because nothing consumed one.
    //
    // Admission rule (PRODUCTION.md / BL-340) satisfied on both ends, which is
    // what makes it admissible now and not before: PRODUCED by an authored
    // Fabricator recipe (steel + machinery, recipes.lua), and CONSUMED by
    // BL-454's per-tick unit upkeep draw — a named actor, carried in
    // chain_depth's R1 exemption table beside propellant and the habitability
    // goods rather than assumed.
    //
    // ONE value, not the three Ben's words named ("supplies, rations, weapons"):
    // food_rations already covers rations and is already produced, and a
    // "supplies" value with no distinct consumer is exactly the
    // produced-by-nothing shape NR-257 deleted five of. A distinct field ration
    // or medical draw later is an APPEND with its behaviour filed in the same
    // change — never a re-insertion here, which would repoint every id after it.
    ordnance               = 37, ///< Fabricator, from steel + machinery. Terminal military good; drawn per-tick by unit upkeep (BL-454).
    // --- Ancient roster, slice 1 (BL-585/BL-586, 2026-08-24) ---
    //
    // The wide ancient roster's first slice: four processing outputs from
    // EXISTING raws (clay, stone, timber, iron_blooms) — no new tile deposit,
    // no new extraction target, no new placement rule. `hides` and its two
    // consumers (`leather`, `cloth`) are deliberately NOT in this slice: they
    // need a new extractable raw with real tile-generation deposits, which is
    // a separate change (BL-586's own design names the deferral).
    //
    // Admission rule (PRODUCTION.md / BL-340): PRODUCED by an authored recipe
    // (recipes.lua, this same change). CONSUMED: `planks` by the Toolmaker
    // recipe below (a real recipe input); `ceramics`, `dressed_stone` and
    // `tools` are TERMINAL — sold on the market, reprocessed by nothing —
    // same shape as `trade_goods_misc`, and named in chain_depth's R1
    // exemption table for exactly that reason rather than left to read as an
    // orphan. `tools` gains a second, real consumer (a construction-material
    // draw) when BL-590 lands; the exemption is not a promise, it is today's
    // honest state.
    //
    // world_save_version bumped 8 -> 9 for this append (world_save.hpp).
    ceramics               = 38, ///< Potter, from clay. Terminal — sold, not reprocessed.
    dressed_stone          = 39, ///< Stonemason, from stone. Terminal — sold, not reprocessed.
    planks                 = 40, ///< Sawmill, from timber. Consumed by the Toolmaker.
    tools                  = 41, ///< Toolmaker, from iron_blooms + planks. Ancient depth 3, tied with the existing ceiling. Terminal for now.
    // --- Ancient roster, slice 2 (BL-586, 2026-08-24) ---
    //
    // The three chains slice 1 deliberately deferred: `hides` is the roster's
    // first ENDEMIC raw (planetology.cpp's C -> D candidate list, alongside
    // tobacco/spices/coffee/furs — lat/sector-restricted, richness-scored,
    // NOT the plain cover-based ambient mechanic timber/clay use), on the
    // pasture/hunting-ground reading of `terrain_cover::grass` (Ben,
    // 2026-08-24). `fibre` is the ordinary case: a grass/marsh crop, grown by
    // the SAME cover-based ambient/biotic mechanic `agricultural_produce`
    // uses, additively — a tile can carry both.
    //
    // Admission rule (PRODUCTION.md / BL-340): PRODUCED — `hides` by the
    // endemic pass, `fibre` by tile_generation.cpp's ambient pass, `leather`/
    // `cloth`/`rigging` by the Tannery/Weaver/Shipwright recipes below
    // (recipes.lua, this same change). CONSUMED — `hides` by the Tannery,
    // `fibre` by the Weaver, `planks` (already consumed by the Toolmaker) and
    // `cloth` both by the Shipwright; `leather` and `rigging` are TERMINAL —
    // sold on the market, reprocessed by nothing — same shape as `ceramics`/
    // `dressed_stone` above, and named in chain_depth's R1 exemption table for
    // exactly that reason. `rigging` is the roster's chosen name for the
    // "terminal trade good" the design table calls Shipwright's output —
    // ropework/cordage/tackle, a genuine ancient-craft good a shipyard
    // plausibly makes, in the same generic-material-noun register as
    // `ceramics`/`dressed_stone`/`tools` (never a proper noun).
    //
    // world_save_version bumped 9 -> 10 for this append (world_save.hpp).
    hides                  = 42, ///< Endemic (BL-191-shaped), pasture/hunting ground. Consumed by the Tannery.
    fibre                  = 43, ///< Ambient grass/marsh crop, alongside agricultural_produce. Consumed by the Weaver.
    leather                = 44, ///< Tannery, from hides. Terminal — sold, not reprocessed.
    cloth                  = 45, ///< Weaver, from fibre. Consumed by the Shipwright.
    rigging                = 46, ///< Shipwright, from planks + cloth. Ancient depth 2. Terminal — sold, not reprocessed.
    count                  = 47
};

static constexpr std::size_t resource_count = static_cast<std::size_t>(resource_type::count);

// ---------------------------------------------------------------------------
// The tile terrain axes (BL-519, 2026-08-21)
// ---------------------------------------------------------------------------
// A tile is described on THREE orthogonal axes: what the ground is MADE OF
// (`terrain_substrate`), what SITS ON IT (`terrain_cover`, which may be
// `none`), and what SHAPE it is (`terrain_landform`, unchanged).
//
// WHY THE SPLIT EXISTS. Until BL-519 there were two axes, and the first of them
// was doing three unrelated jobs at once: substrate (barren/rocky/volcanic/
// metallic/regolith), cover (forest/grassland/tundra/wetland) and state
// (urban/icy/ocean). Ben's brief was "a mountain might have a forest or not" —
// and a mountain WITH a forest was already expressible (composition=forest ×
// landform=mountain). What could not be said was a ROCKY mountain that happens
// to be forested, because the one slot had been spent on the forest.
//
// `urban` was the proof rather than the exception. It is a one-way transform
// (BL-366) that OVERWROTE the composition, so paving a metallic tile destroyed
// the fact that it was metallic. The split fixes that as a side effect: urban is
// a COVER value (Ben's call, 2026-08-21), so a city now sits ON its geology
// instead of erasing it, and `maybe_transform_to_urban` leaves the substrate
// alone.
//
// THIS ADDS NO CONCEPT. It un-mixes one that got overloaded. Authority:
// docs/economy/TILES.md; the deposit consequences are in
// docs/economy/RESOURCES.md.
// ---------------------------------------------------------------------------

/// What the ground is MADE OF — the geology. Fixed at generation and NEVER
/// transformed: nothing in the simulation rewrites a tile's substrate, which is
/// the property that distinguishes this axis from `terrain_cover`.
///
/// Determines which MINERAL deposits can appear (ore follows the substrate;
/// timber and produce follow the cover), the tile's base build cost, and the
/// terrain colour the map lenses tint. See docs/economy/TILES.md.
enum class terrain_substrate : uint8_t
{
    barren      = 0, ///< Dry, dusty, minimal organics; iron ore, coal, petroleum.
    rocky       = 1, ///< Hard rock outcrops; iron, copper, rare earth ore.
    sedimentary = 2, ///< Soil, silt and sandstone — the ground a biotic cover grows on.
                     ///< NEW with BL-519: the old model had no way to say "real
                     ///< soil" except by spending the slot on what grew there.
    volcanic    = 3, ///< Geologically active; rare earth and iron ore. High hazard.
    metallic    = 4, ///< High metal content; iron-nickel ore, platinum group metals.
    regolith    = 5, ///< Loose surface material on airless bodies.
    icy         = 6, ///< Ice-dominated ground; water ice. A SUBSTRATE, not a cover —
                     ///< an ice cap is what the ground is, whereas `cover::snow` is
                     ///< what fell on it and could melt.
    ocean       = 7, ///< OPEN water: the sea, out of sight of land. Carries no land
                     ///< deposits and no buildings. NARROWED by BL-516 — it used to
                     ///< mean every water tile; the shallows and the inland waters
                     ///< are now `coast` and `lake` below.

    // BL-516 (Ben, 2026-08-21): "We can have lakes, coasts, and oceans." Water
    // stops being one value, and it stays on THIS axis because BL-519's interim
    // note was right about why: every existing consumer asks "is this water?" of
    // the ground. APPENDED, NEVER RENUMBERED — `ocean` keeps id 7, so no wire
    // value, golden or fixture shifts meaning.
    //
    // The three kinds are EXCLUSIVE and STRUCTURAL — no threshold picks between
    // them (tile_generation.cpp § classify_water_kinds):
    //   * `lake`  — a water component that does not reach the body's sea.
    //   * `coast` — a sea tile with at least one land neighbour: the shoreline ring.
    //   * `ocean` — a sea tile with none: open water.
    //
    // ASK THE QUESTION YOU MEAN. Almost every existing consumer asks "is this
    // water?", and must keep asking exactly that — use `is_water()`. Only the
    // consumers that genuinely care WHICH water (the road pass's strait rule, the
    // province partition's size band, the lenses) name a kind.
    //
    // A WATER TILE STILL CARRIES `terrain_cover::none`, whichever kind it is. The
    // cover axis describes what grew on ground; water has no ground.
    lake        = 8, ///< Inland water with no path to the sea. Buildings refuse it as ocean does.
    coast       = 9, ///< Shallow sea adjacent to land — the shoreline ring. Not buildable.
};

// ---------------------------------------------------------------------------
// Water predicates (BL-516) — the choke point every water test goes through
// ---------------------------------------------------------------------------
// These live HERE, beside the axis, rather than in placement_rules.hpp, because
// half the callers (logistics, rivers, roads, provinces, terrain combat) ask the
// question about TERRAIN and have no business depending on the placement layer.
// `placement_rules::is_water_tile` delegates to `is_water` so there is one
// implementation and one truth.

/// Is this ground water OF ANY KIND? The question nearly every consumer means:
/// nothing is built here, nothing is farmed here, and a land walker stops here.
constexpr bool is_water(terrain_substrate s)
{
    return s == terrain_substrate::ocean || s == terrain_substrate::coast
           || s == terrain_substrate::lake;
}

/// Open sea, out of sight of land — the water a road never crosses and a large
/// sea province is drawn over.
constexpr bool is_open_ocean(terrain_substrate s) { return s == terrain_substrate::ocean; }

/// The shoreline ring: sea with land on at least one side. A strait is made of
/// these and only these, which is what lets the road pass tell a crossing from
/// an open-sea route out of the DATA rather than out of a run length.
constexpr bool is_coastal_water(terrain_substrate s) { return s == terrain_substrate::coast; }

/// Inland water with no path to the sea.
constexpr bool is_lake(terrain_substrate s) { return s == terrain_substrate::lake; }

/// Sea of either kind — water that is part of the body's connected ocean, as
/// opposed to a lake. What "coastal" means for a port: a lakeshore is not a coast.
constexpr bool is_sea(terrain_substrate s)
{
    return s == terrain_substrate::ocean || s == terrain_substrate::coast;
}

/// What SITS ON the substrate — and it may be absent. `none` is a FIRST-CLASS
/// value, and that absence is the whole point of the axis: "a mountain with no
/// forest" stops being a different kind of ground from "a mountain with one".
///
/// Determines BIOTIC deposits (timber, produce), defensive cover and forage in
/// combat, and the overlay pattern BL-520 draws. Unlike the substrate, a cover
/// CAN change — `urban` is the one transform that does so today (BL-366).
enum class terrain_cover : uint8_t
{
    none   = 0, ///< Bare ground. A real answer, not a missing one.
    grass  = 1, ///< Open fertile cover; agricultural produce.
    scrub  = 2, ///< Sparse woody cover — the half-step that makes a forest line
                ///< gradual instead of a hard edge. Also where `tundra` went:
                ///< tundra was a climate outcome wearing a terrain slot, and it
                ///< is now scrub (or none) on cold ground (Ben, 2026-08-21).
    forest = 3, ///< Dense tree cover; timber. Ben's example case.
    marsh  = 4, ///< Bog, floodplain, fen; produce and clay. Wants low landforms.
    snow   = 5, ///< Lying snow. Falls out of latitude × BL-517's retained height
                ///< with no new generation input.
    dunes  = 6, ///< Wind-blown sand. Explains why some barren ground is workable
                ///< and some is not.
    ash    = 7, ///< Volcanic fall. A hazard reading rather than a resource one.
    salt   = 8, ///< Dry-basin crust; a home for a salt deposit if one is restored.
    urban  = 9, ///< BL-366: one-way transform, fired when a tile's non-extraction
                ///< building stack fills its cap. No new extraction or ambient
                ///< placement; existing extraction sites are grandfathered. High
                ///< habitability ceiling; never reverts.
                ///<
                ///< A COVER, by Ben's call (2026-08-21) — so it still erases what
                ///< grew here, but no longer erases the GEOLOGY underneath, which
                ///< was the shipped bug the axis split was reaching for.
};

/// Cover density, 0-255: sparse scrub at the bottom, closed-canopy forest at the
/// top. Ben's call, 2026-08-21 — cover is GRADED, not binary.
///
/// ONE NUMBER, TWO CONSUMERS, and that is why it earns a field rather than being
/// two: BL-520's texture reads it as how heavily to draw the overlay pattern, and
/// the economy reads it as biotic yield (timber richness, forage in combat). A
/// sparse wood should both LOOK thin and CUT thin, and a single scalar is what
/// keeps those two from drifting apart.
///
/// INVARIANT: density is 0 if and only if cover is `none`. Every producer and
/// consumer may rely on it; `tile_axes_harness` asserts it over generated worlds.
inline constexpr std::uint8_t k_cover_density_max = 255;

/// Cover density as a fraction in [0, 1] — the form every scalar consumer wants.
inline constexpr float cover_fraction(std::uint8_t density)
{
    return static_cast<float>(density) / static_cast<float>(k_cover_density_max);
}

/// True if @p c is a BIOTIC cover — something that grew rather than something
/// that fell, was blown, or was built. The distinction the deposit rules turn on:
/// timber and produce need life, snow/dunes/ash/salt/urban do not supply it.
inline constexpr bool is_biotic_cover(terrain_cover c)
{
    return c == terrain_cover::grass || c == terrain_cover::scrub
        || c == terrain_cover::forest || c == terrain_cover::marsh;
}

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
    schooling           = 8, ///< BL-615: passive education building (same shape as research_institute:
                             ///< produces nothing tradeable, staffs at zero). PASSIVE for now — its
                             ///< qualification-raising effect is a separate seam (POPULATION.md
                             ///< § Qualification). Placement-gated: must stand IN a population
                             ///< centre of any stratum (placement_gate, POPULATION.md § Strata
                             ///< gate buildings).
    university          = 9, ///< BL-615: the schooling building's City-tier sibling. Same passive
                             ///< shape; placement-gated to a centre of stratum City (4) or above —
                             ///< "you can't build a university in a town" (Ben, 2026-08-25).
};

/// One past the last building type — the wire parser's range gate (BL-396:
/// run_serve refuses any `type=` outside [0, building_type_count) rather than
/// letting a narrowing cast truncate it into a type the caller never named,
/// or index the per-type economics table out of bounds). Derived from the
/// enum's tail, the same way resource_count derives from resource_type::count:
/// appending a type means moving this with it.
static constexpr uint8_t building_type_count =
    static_cast<uint8_t>(building_type::university) + 1;

/// BL-615 stratum placement gates (docs/economy/POPULATION.md § Strata gate
/// buildings): which relationship to the population-centre scale ladder a
/// building's SITE must hold. Authored DATA on the building definition —
/// per-type in `building_economics` (scripts/economy.lua), with a per-recipe
/// radius override for processing facilities (scripts/recipes.lua) — and read
/// generically by `placement_rules::can_place_in_world`; never a
/// building-name switch in logic. A default-constructed gate gates nothing,
/// so every pre-BL-615 call site and hand-built harness registry is unchanged.
struct placement_gate
{
    /// The building must stand ON a tile hosting a population centre.
    bool requires_centre = false;
    /// The hosting centre must be at least this stratum (1–5, the
    /// Outpost→Metropolis scale ladder). 0 = no minimum. A non-zero minimum
    /// implies the centre requirement (a tile with no centre has no stratum).
    int min_centre_scale = 0;
    /// The building must stand within this grid distance of SOME population
    /// centre on the same body (wrapped squared-grid-distance metric, the
    /// codebase's standard proximity measure). 0 = ungated.
    int centre_proximity_radius = 0;
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
    /// What the ground is MADE OF. Fixed at generation; nothing rewrites it.
    terrain_substrate   substrate;
    /// What SITS ON the ground, `none` included. Mutable — `urban` is the one
    /// transform today (maybe_transform_to_urban, placement_rules.cpp).
    terrain_cover       cover = terrain_cover::none;
    /// How heavily the cover sits, 0-255. ZERO IF AND ONLY IF `cover` is `none`
    /// — an invariant `tile_axes_harness` asserts, not merely a convention.
    std::uint8_t        cover_density = 0;
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
    /// Background nation industrial occupation [0, 1].
    ///
    /// NOT A TERRAIN FIELD, despite the name, and NOT related to
    /// `terrain_substrate` or `cover_density` — it predates both (BL-365's
    /// background-industry premise) and means how heavily the nation's own
    /// industry already occupies this tile. Left named as it is because
    /// renaming it is a separate change with its own call sites; flagged here
    /// so the collision misleads nobody.
    float      substrate_density = 0.0f;

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

    /// Normalised generation heightmap value for this tile, [0, 1] (BL-517).
    ///
    /// **Captured, never recomputed.** This is the exact float tile generation Pass 1
    /// produced for this tile — the continent-biased, normalised heightmap the whole
    /// pipeline was then derived from (`generate_body_tiles`, src/world/tile_generation.cpp).
    /// It is copied out of the pass's own `height` vector at tile assembly; nothing
    /// re-derives it, and it is the same number `generation_record::height` reports.
    ///
    /// **Why it is world state and not a ledger breadcrumb.** Every other Pass 1/2
    /// intermediate (moisture, band, ocean_score) stays disposable and regenerates on
    /// demand — `docs/generation/GENERATION_LEDGER.md` § Data lifetime. Height graduated
    /// out of that set on 2026-08-21 because BL-515 grows province borders against
    /// elevation DIFFERENCE, making it an input to a live partition rather than a
    /// breadcrumb explaining a past decision. The rule that keeps this from becoming a
    /// loophole is in that doc: an intermediate graduates only when a system outside the
    /// ledger reads it, and it graduates by being named there. Do not delete this field
    /// to restore the symmetry.
    ///
    /// **Not a terrain input.** `landform` and `substrate` are NOT derived from this
    /// field — they were decided inside the pass by rules this capture does not touch.
    /// `cover` is the one partial exception, and it is deliberate: BL-519's snow cover
    /// reads latitude × this height, which is what let snow arrive without a new
    /// generation input of its own.
    /// Reading it is legitimate; re-deriving terrain from it is a different item.
    ///
    /// 0.0 on any tile not produced by `generate_body_tiles` (hand-built harness fixtures).
    float height = 0.0f;
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
    /// BL-392: where the goods LAND — the buyer's own body, not the supplier's.
    /// Delivery used to credit `body`, so the stock arrived on a body the buyer
    /// had no processor reservation on and the auto-surplus path liquidated it
    /// in the same tick. Null only if the buyer owns nothing anywhere, in which
    /// case it degrades to `body` (the pre-BL-392 behaviour, and the only case
    /// where there is nowhere better to put it).
    entity_id     delivery_body = null_entity;
    resource_type resource     = resource_type::iron_ore;
    float         quantity     = 0.0f;
    float         unit_price   = 0.0f;           ///< Quoted at request time; locked in on accept. BL-392: spot LESS the volume discount.
    int32_t       lead_time_ticks = 0;            ///< BL-392: `base_ticks x ceil(quantity / the SUPPLIER's throughput for this good)`.
    /// BL-392: carriage to `delivery_body`, in credits, for the WHOLE order.
    /// Zero when the delivery body is the fulfilment body. Paid to the supplier,
    /// who arranges the shipping — so it is a transfer, never a burn.
    float         freight_cost = 0.0f;
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
    entity_id     delivery_body   = null_entity; ///< BL-392: where the goods land — see procurement_quote.
    float         quantity        = 0.0f;
    float         unit_price      = 0.0f;
    int32_t       lead_time_ticks = 0;
    int32_t       ticks_elapsed   = 0;
    float         deposit_paid    = 0.0f; ///< Already debited at accept_quote (economy.procurement.deposit_fraction).
    float         freight_cost    = 0.0f; ///< BL-392: carriage for the whole order; paid to the supplier.
};

static_assert(sizeof(procurement_quote)    == 40, "procurement_quote is a save-format record — see procurement.hpp");
static_assert(sizeof(procurement_contract) == 48, "procurement_contract is a save-format record — see procurement.hpp");

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
/// BL-157 ruled tile position canonical over body/region (2026-08-07); this
/// struct shipped with a `body` field ahead of that ruling — `position`
/// (BL-324, 2026-08-08) is the fix, landed with the hire-unit item that first
/// needed tile grain rather than deferring it back to BL-157.
/// BL-459 (2026-08-17) REMOVED the stored `strength` field. It was documented as
/// a "fixed-point combat strength scalar" and BOTH writers set it to the same
/// value as `count` — a literal duplicate kept in sync by hand, with nothing
/// enforcing it. Strength is now DERIVED, never stored:
///
///     strength = count x roster_type_quality x supply_factor   (x100 fixed point)
///
/// computed by `unit_strength(world, unit_component)` in world/unit_roster.hpp.
/// A stored writable field would let the duplicate come straight back, so the
/// field is gone rather than merely deprecated. `count` stays the honest
/// headcount; `supply_factor_permille` is the only new state, and it is written
/// by BL-454's upkeep pass, not by the hire path.
/// BL-470: a path-march order in flight. `dest == null_entity` means "no
/// active order" — the unit is halted (or has never been given one), the
/// only state `halt_unit` needs to restore. The path is computed ONCE, at
/// order time, from the same cost function `body_reach_field` and the A*
/// pathfinder use (`intra_body_path`, logistics.hpp) — never re-Dijkstra'd
/// per tick. `next_index` is the index into `path` of the next tile the unit
/// has not yet reached (`path[0]` is the tile the unit stood on when the
/// order was placed, so a fresh order starts at `next_index == 1`).
/// `progress` banks fractional march points toward the next hop's cost
/// across tick boundaries — see run_unit_march (economy_system.cpp).
///
/// BL-511 (2026-08-21): the ORDER IS NOW PROVINCE-GRAIN. `dest_province` is
/// what the player/agent actually commanded; `dest` is the canonical member
/// TILE the path was solved to, kept because the recompute path needs a tile
/// endpoint and because every existing reader of `dest` keeps working. The
/// march ENDS when the unit's tile lies in `dest_province` — it does not walk
/// on to `dest` once it is already inside the commanded province. A legacy /
/// harness-built order with `dest_province == 0` keeps the pure tile
/// behaviour (arrive when the path is exhausted).
struct movement_order
{
    entity_id               dest       = null_entity;
    /// BL-511: the commanded destination province (province::id). 0 means
    /// "no province grain on this order" — see the note above.
    uint32_t                dest_province = 0;
    std::vector<entity_id>  path;
    std::size_t             next_index = 1;
    float                   progress   = 0.0f;
};

struct unit_component
{
    entity_id position;      ///< Tile the unit currently occupies (BL-157: tile-canonical).
    entity_id owner;         ///< Corporation or faction entity that controls this unit.
    int       count;         ///< Number of units in the group.
    uint16_t  type = 0;      ///< Opaque roster-type index; see combat.hpp's army_stack_entry.

    /// BL-470: the unit's standing movement order, if any. `order.dest ==
    /// null_entity` means unordered/halted — a freshly hired unit starts this
    /// way, unpinning nothing until march_unit is issued (BL-393's open half:
    /// units were write-only and inert until this field gave them somewhere
    /// to go).
    movement_order order;

    /// BL-454. How well this unit is supplied, per-mille (1000 = fully supplied).
    /// Lowered by the ONE decay rule with TWO triggers in the upkeep pass
    /// (economy_system.cpp § run_unit_upkeep): the unit is beyond the reach field,
    /// or its upkeep goods draw went unmet. Both are the same subtraction for the
    /// same reason — an army that is not being supplied gets weaker — so they are
    /// deliberately not two rules. Feeds `unit_strength` and the combat adapter,
    /// so an unsupplied army is measurably weaker IN THE RESOLVER, not merely
    /// more expensive.
    int32_t   supply_factor_permille = 1000;

    /// BL-454. The military_base this unit was mustered at, or `null_entity` for
    /// a unit that predates the muster rule (the hard-coded world's stub). The
    /// ORPHAN KEY: `demolish_building` erases the building, the corp asset and the
    /// stockpile but never touches `w.units`, so demolishing a muster base used to
    /// leave every unit raised there orphaned. The upkeep pass is the only place
    /// that can see it; it disbands a unit whose muster base has been erased.
    /// Null means "no muster base recorded" and is never orphaned.
    entity_id muster_base = null_entity;
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

    // --- BL-452: the layer becomes reachable by command (2026-08-17) ---

    /// Stable handle, allocated at dispatch from `world::allocate_convoy_id`.
    /// A convoy is a **transient** subject — it exists for a handful of ticks
    /// and is then erased — but `hold_convoy` has to name one, and naming it by
    /// vector index would let one convoy's arrival re-point a command already
    /// composed against another. Same "stable across erase, never reused"
    /// contract as `sell_order::id` (world.hpp § next_order_id), and carried in
    /// `corp_command::order` for the same reason: it is a handle, not an entity.
    uint32_t    id             = 0;
    /// True while the convoy is **held** — `advance_convoys` skips it, so it
    /// stops making progress and simply waits on its lane. Deliberately NOT a
    /// cancel: the cargo left the source pool at dispatch, so a cancel would
    /// have to invent a return leg or mint the goods back. Toggled by the
    /// `hold_convoy` verb; a held convoy costs nothing further (the haul is paid
    /// once, at dispatch) and resumes exactly where it stopped.
    bool        held           = false;
    /// Credits the dispatch actually charged for this haul — recorded so the
    /// Convoys tab can report what the cargo in flight has already cost, which
    /// is otherwise unrecoverable once the balance has moved on.
    float       cost_paid      = 0.0f;
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

    /// Research accumulator (BL-332). Stockpiled, market-invisible, never
    /// decaying — closer to "research points" than a resource_type good (no
    /// market slot, no price, not held in a (corp, body) pool). Produced
    /// passively by every completed research_institute the corp owns, symmetric
    /// across player, rival and background alike.
    ///
    /// READ BY `condition_subject::science` (BL-455, 2026-08-17), so a tech gate
    /// or a law can require a research level. It is REACHED, not SPENT: a
    /// condition_set is a predicate over corp state and nothing in the gate
    /// system debits anything. See condition_set.hpp for why spending would be a
    /// different mechanism rather than a tuning choice.
    ///
    /// **`military_points` was REMOVED here on 2026-08-17 (BL-455, Ben's call on
    /// NR-307).** It sat beside `science` and was credited per tick by every
    /// completed military_base — and read by nothing at all: no gate, no
    /// surface, no scorer, not the blackboard. Three sites in all of `src/`
    /// (this declaration, the Lua param load, the write) and no fourth.
    ///
    /// It was deleted rather than wired because no consumer could be named for
    /// it. The economy→military interface is already the base tile plus the
    /// reach field (BL-325's ruling 3), and the recurring cost of force is
    /// already credits and ordnance (BL-454's upkeep) — a third abstract
    /// military currency beside those either duplicates them or gates something
    /// nobody has specified. `science` survived the same test because its
    /// consumer already existed and was merely unconnected.
    ///
    /// This is NR-257's orphan-resource rule applied to a component field:
    /// a value earns its place by having a consumer, and the honest response to
    /// "there is no consumer" is deletion, not a placeholder. If the
    /// governing-body pivot (BL-094) later wants a military capacity scalar, it
    /// is an append with its behaviour filed in the same change.
    float science = 0.0f;

    /// BL-428 growth spine: every good this corporation has EVER produced, set the
    /// tick a building of its actually makes some (economy_system.cpp's
    /// run_extraction / run_processing). Read through `recipe_registry` to obtain
    /// the corp's reached chain depth, which gates what it may build next.
    ///
    /// PRODUCED-ONCE-EVER, never cleared, and that is the design's legibility call
    /// (Ben, 2026-08-15): progress must not evaporate because a building idled for
    /// a tick or was demolished for a better site. A corp that has smelted iron
    /// once knows how to smelt iron. Monotonicity is the property the gate rests
    /// on — a placement that was legal must not silently become illegal.
    ///
    /// A bool per resource rather than a bitset: `resource_count` is small, and a
    /// plain array keeps this trivially copyable and order-independent, which
    /// matters because the economy reads a number off it (the BL-406 lesson).
    std::array<bool, resource_count> produced_ever = {};
};

/// BL-428: how far down the production graph @p c has actually reached — the
/// deepest good it has ever produced. 0 for a fresh corporation (it has reached
/// the raws and nothing beyond), which is exactly what an untouched
/// `produced_ever` should mean.
///
/// Unreachable goods (`depth_of` == -1) cannot raise it: a corp cannot have
/// produced a good the current era's graph says is unmakeable, and if a stale bit
/// survived a band change it must not be allowed to unlock anything.
///
/// @param c    The corporation.
/// @param reg  Loaded registry, read for `depth_of` under the campaign's band.
/// @return     The corp's reached depth (>= 0).
template <typename Registry>
int corp_reached_depth(const corporation_component& c, const Registry& reg)
{
    int best = 0;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (!c.produced_ever[r])
            continue;
        const int d = reg.depth_of(static_cast<resource_type>(r));
        if (d > best)
            best = d;
    }
    return best;
}

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
/// take no autonomous actions in the prototype; every field except `treasury`
/// is generation output (the treasury is live state, credited each tick by the
/// levy transfer — BL-480). See docs/generation/NATION_GENERATION.md and
/// docs/development/backlog.json for the deferred behaviour design.
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
    /// tile_component convention (terrain_substrate substrate).
    ideology       politics = ::ideology::mercantile;

    /// Military / territorial posture; drawn from seeded RNG in Pass 4.
    expansionism   posture  = ::expansionism::passive;

    /// Dominant economic activity; drawn from seeded RNG in Pass 4.
    economic_focus focus    = ::economic_focus::extraction;

    /// BL-480: the nation's credit account. A law's levy is a TRANSFER — the
    /// payer's debit lands here in the same tick, same float, so the sum is
    /// conserved (asserted by tools/verify/law_author_harness.cpp; the BL-392
    /// class of silent money destruction is gone from this flow). Written only
    /// by `apply_budget`'s levy pass and by the D4 import tariff, which credits
    /// here exactly what the buying corporation is debited — also a transfer,
    /// never a mint. Zero at generation; nothing spends it yet, and a treasury
    /// that started full would be a balance change smuggled in as a field. The
    /// spend side is future nation-grain work under the 2026-08-18 grant.
    /// SERIALISED since BL-107, 2026-08-22 (world_save.cpp's nation record,
    /// `w_nation` / `r_nation`, carries it). Whether `state_hash`
    /// folds it is world.cpp's call — Sprint N3 (T8) folds nation treasuries
    /// only when non-trivial, on the battles precedent; until that lands a
    /// treasury divergence is only detectable through the debit half on corp
    /// balances (review 2026-08-19 #3).
    float treasury = 0.0f;

    /// BL-571 (nation garrisons): the tile the nation's Pass-1 seed grew from —
    /// the same "region the realm grew out of" reading Pass 5 already uses to
    /// pick a tongue for the nation's name, restated as a tile rather than a
    /// tongue. `null_entity` for a nation with no surviving seed to point at
    /// (should not occur for a generated nation, but a hand-built harness
    /// fixture may skip Pass 1 entirely).
    ///
    /// This is the closest thing to a "capital" the world holds today —
    /// nothing else names one — and it exists so garrison seeding has a fixed
    /// point to anchor the capital garrison on: `province_of(capital_tile)` is
    /// MILITARY.md's "nation's capital province". Computed once in Pass 5
    /// (nation_generation.cpp), before the province partition even exists, so
    /// it is stored as a TILE rather than derived as a province at that point.
    ///
    /// SERIALISED (world_save.cpp's nation record) since this item, which is
    /// why `world_save_version` moves with it — a v4 stream simply predates
    /// the field and has nowhere to source it from.
    entity_id capital_tile = null_entity;
};
