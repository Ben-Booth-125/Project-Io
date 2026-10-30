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
// UNITS. Recon and stealth are POINTS, per unit (land_combat_stats, the class table
// plus kit). The attacking side holds a recon STOCK, grown daily (MILITARY.md § Recon);
// its hits on a defending unit scale by min(1, stock / stealth). A FIRST CUT.
// ---------------------------------------------------------------------------

/// The rates the battle pass reads to build each side's recon points.
struct land_recon_params
{
    // RETIRED (Ben, 2026-10-29): the 20-day time term and the defender's 1.5x — recon
    // is the attacker's alone, a stock that builds within days; the defender always
    // hits in full.
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
