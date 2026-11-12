#pragma once

// ---------------------------------------------------------------------------
// BL-1643 — artillery's authored numbers (MILITARY.md § Artillery fires shells)
// ---------------------------------------------------------------------------
// A FIRST CUT (calibrated 2026-11-12). Held on `military_capability_params::artillery`.
// C++ defaults, as missile_params.hpp's are; a `economy.military.artillery` Lua table is
// owed with the missiles'. Its own header so recipe_registry.hpp can hold it without
// pulling artillery.hpp (and world.hpp) into every TU.
//
// UNITS. Range is hex steps (tiles). Shares are per-mille. A bombardment's hits are on the
// land round's scale (campaign_battle_params: milli-hits per man per round), so a battery's
// day of fire is measured against the day it would fight in a battle.
// ---------------------------------------------------------------------------

struct artillery_params
{
    /// The class's range before kit: a towed howitzer reaches 2 tiles. Each gun item adds
    /// its `kit_item::art_range` (long barrel +1, rocket launcher +1, guided rockets +2,
    /// radar-laid +1), so the farthest battery reaches 4 tiles - inside the 4-tile sight a
    /// unit gives its nation (missile_params::sight_tiles), so a battery never fires blind.
    int base_range = 2;
    /// A day's bombardment is this share of the fire the battery would put into a battle that
    /// day: standing off, unspotted at the target's front line, it lands less than in contact.
    int strike_share_permille = 500;
    /// Of what it fires, the share that lands: shells (base) and precision shells.
    int landed_permille           = 600;
    int precision_landed_permille = 900;
};
