#pragma once

// ---------------------------------------------------------------------------
// A land class's combat numbers (BL-1421; MILITARY.md § Land — the classes)
// ---------------------------------------------------------------------------
// Its own header because two layers read it and must not include each other's:
// the roster authors the table (unit_roster.{hpp,cpp}) and the resolver fights
// with it (campaign_battle.hpp, which world.hpp includes). Pulling the roster
// header into world.hpp would leak its forward declarations into every TU.

/// One class's numbers, PER MAN PER ROUND, in milli-hits (attacks, defence,
/// breakthrough) and per-mille (hardness). Authored data; the scale is set by the
/// round's own params (campaign_battle_params), which are tuned against it.
struct land_combat_stats
{
    int soft         = 0; ///< Hits on a target's soft share.
    int hard         = 0; ///< Hits on its hard share.
    int hardness     = 0; ///< Share of the unit that is armoured, 0..1000.
    int defence      = 0; ///< Hits absorbed when defending.
    int breakthrough = 0; ///< Hits absorbed when attacking.
    /// BL-1453 (land recon; MILITARY.md § Recon). PER UNIT, not per man, and not
    /// scaled by the roster row's quality: recon points this unit gives its side
    /// over a battle, and the recon points an enemy needs to see all of it. The
    /// share of a unit its enemy sees scales every hit landed on it
    /// (`visible_share_permille`, campaign_battle.hpp).
    int recon        = 0;
    int stealth      = 0;
};
