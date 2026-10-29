#pragma once

// ---------------------------------------------------------------------------
// Terrain and the classes (BL-1455; MILITARY.md § Terrain and the classes,
// Ben 2026-10-29).
//
// Ground treats the land classes differently: tanks cannot cross mountains
// without a road, and fight worse in them. On top of the shared terrain table
// (terrain_combat.hpp), which every unit reads alike, each land class carries a
// PER-TERRAIN ROW of effects - move, blocked unless roaded, fight, wear, stealth -
// authored under `economy.military.terrain_class` (scripts/economy.lua) and loaded
// into `military_capability_params::terrain` (recipe_registry.hpp).
//
// Pure, authored data and pure functions: no stored field, nothing on the
// serialisation seam, deterministic by construction.
// ---------------------------------------------------------------------------

#include "components.hpp"
#include "unit_roster.hpp" // land_class, land_class_count

#include <array>
#include <cstdint>

struct military_capability_params; // recipe_registry.hpp

/// The terrain a class row keys on. Mountain, canyon and rift are LANDFORMS;
/// marsh, desert (the dunes cover), forest, urban and snow are COVERS. A tile
/// can match one landform row and one cover row at once (a forested mountain).
enum class class_terrain : std::uint8_t
{
    mountain = 0,
    canyon   = 1,
    rift     = 2,
    marsh    = 3,
    desert   = 4, ///< the `dunes` cover
    forest   = 5,
    urban    = 6,
    snow     = 7,
};

inline constexpr int class_terrain_count = 8;

/// One class's row for one terrain. Every field's default is NEUTRAL: an
/// unauthored cell changes nothing.
struct terrain_class_row
{
    float move                  = 1.0f;  ///< x the march days of a hop onto the tile; >= 1.
    bool  blocked_unless_roaded = false; ///< the class cannot ENTER the tile without a road on it.
    bool  dismount              = false; ///< march at foot infantry's days on the tile (carriers on high ground).
    float fight                 = 1.0f;  ///< x soft, hard, defence and breakthrough when it is the battle ground; (0, 1].
    float wear                  = 1.0f;  ///< x the daily drain of carried equipment standing there; >= 1.
    float stealth               = 1.0f;  ///< x the unit's stealth standing there; > 0.
    bool  has_stealth           = false; ///< the row authors stealth (only authored rows vote on it).
};

/// The whole authored table: a row per (land class, terrain), plus the one
/// open-ground rule every class reads.
struct terrain_class_params
{
    std::array<std::array<terrain_class_row, class_terrain_count>, land_class_count> rows{};

    /// OPEN GROUND EXPOSES EVERYONE (Ben, 2026-10-29): on plains or highland with no
    /// concealing cover (none, grass, salt, snow or dunes) every unit's stealth is
    /// multiplied by this. Authored as `economy.military.terrain_open_ground_stealth`;
    /// 1.0 (no effect) when absent.
    float open_ground_stealth = 1.0f;
};

/// What the ground does to ONE unit's class on ONE tile: all five effects, the
/// worst matching row per effect (never a product of rows).
struct terrain_class_effect
{
    float move    = 1.0f;  ///< x the days of a hop onto this tile (a dismount folds in as foot / own days).
    bool  blocked_unless_roaded = false; ///< a matching row blocks entry without a road.
    bool  walled  = false; ///< blocked_unless_roaded AND the tile carries no road: the class cannot enter.
    bool  dismounted = false; ///< a matching row dismounts the class here.
    float fight   = 1.0f;  ///< the row's raw fight scale (deepest debuff).
    float battle_fight = 1.0f; ///< `fight` with the road rule: a road on the tile halves the debuff.
    float wear    = 1.0f;  ///< x the daily drain of carried equipment (fastest wear).
    float stealth = 1.0f;  ///< x the unit's stealth (the lowest authored value; open ground included).
};

/// The class terrain keys a tile matches, written into @p out (up to two: its
/// landform's row and its cover's row). @return how many. Water matches nothing.
int class_terrains_of(const tile_component& t, class_terrain out[2]);

/// True iff @p t is OPEN GROUND: plains or highland, land, with no concealing cover
/// (none, grass, salt, snow or dunes).
bool is_open_ground(const tile_component& t);

/// THE ONE LOOKUP. All five effects of the ground on a unit of land class @p c
/// standing on (or stepping onto) @p t. When the tile matches two rows - a forested
/// mountain - each effect takes the WORST of them for the unit: blocked if either
/// blocks, the larger move multiplier, the deeper fight debuff, the faster wear, the
/// lower stealth. Rows never multiply. A dismount reads `mil.march_days_per_land_class`
/// (foot infantry's days over the class's own). `land_class::none` and the
/// pre-modern marker `k_land_class_from_cls` get no effects.
terrain_class_effect terrain_class_effects(const military_capability_params& mil, land_class c,
                                           const tile_component& t);

/// The same for a roster row: a row that DECLARES a land class reads that class; a
/// pre-modern row (no land class) and an unknown index get no effects.
terrain_class_effect terrain_class_effects_for_type(const military_capability_params& mil,
                                                    std::uint16_t type, const tile_component& t);

/// The battle fight scale as a per-mille integer (`battle_fight`, rounded), the
/// grain the round arithmetic reads (campaign_battle.cpp `side_hits` / `side_absorb`).
int terrain_fight_permille(const terrain_class_effect& e);
