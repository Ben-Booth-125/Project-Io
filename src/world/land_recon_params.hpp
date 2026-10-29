#pragma once

// ---------------------------------------------------------------------------
// BL-1453 — land recon's authored rates (MILITARY.md § Recon, Ben 2026-10-29)
// ---------------------------------------------------------------------------
// Authored in scripts/economy.lua under `economy.military.recon` and read by
// recipe_registry.cpp into `military_capability_params::recon`. The C++ defaults
// below EQUAL the Lua values, so a hand-built harness registry sees what the game
// sees. Its own header, as air_params.hpp is, so recipe_registry.hpp can hold it
// without pulling the battle headers into every TU.
//
// UNITS. Recon and stealth are POINTS, per unit (land_combat_stats::recon /
// ::stealth, the class table plus kit). A side's recon points over a battle are
// set against each target's stealth: the visible share is min(1, recon / stealth).
// A FIRST CUT, logged in BL-1453's report.
// ---------------------------------------------------------------------------

/// The rates the battle pass reads to build each side's recon points.
struct land_recon_params
{
    /// Days of fighting after which time alone gives full vision: a target's
    /// stealth x min(1, day / this) is added to the side's points against it.
    int time_full_days = 20;
    /// The defender's recon counts this much, per-mille (it knows its ground).
    /// Applied to the defender's whole sum, the time term included.
    int defender_permille = 1500;
    /// Recon points one wing of the RECON class gives its side's battles in a
    /// province it is active over, at full strength and with the province its
    /// only one (a wing over N provinces gives 1/N, as its day is spread).
    int recon_wing_points = 40;
    /// The same for a wing of any other class: the base amount.
    int wing_points = 10;
    /// Recon points the side flying superiority over the province adds at a
    /// share of 1000, in proportion to its share. A side that flies no
    /// superiority there adds none, however uncontested the sky.
    int superiority_points = 20;
};
