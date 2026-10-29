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
    /// BL-1453 (land recon; MILITARY.md § Recon, Ben 2026-10-29). PER UNIT, not per
    /// man, and not scaled by the roster row's quality:
    ///   recon   — what the unit adds to its ATTACKING side's recon stock each day of
    ///             the battle, and to the opening when it faced the tile a whole day.
    ///             Every land class has a base above zero.
    ///   stealth — the recon an attacker needs to land its full hits on the unit.
    /// The attacker's hits on a defending unit are scaled by min(1, recon / stealth)
    /// (`visible_share_permille`, campaign_battle.hpp); the defender's are always full.
    int recon   = 0;
    int stealth = 0;
};
