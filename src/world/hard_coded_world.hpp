#pragma once

#include "world.hpp"

#include <cstdint>

/// Resource-abundance tier for a generated world. Earth-like is the ceiling: no
/// tier exceeds the baseline (GENERATION_STRATEGY.md § The resource ceiling), so
/// `standard` (1.0×) is the richest and the leaner tiers step *down* from it.
/// Maps to the `deposit_scalar` threaded into generate_body_tiles.
enum class abundance_level : uint8_t { sparse, lean, standard };

/// The reproducible world descriptor: a master seed plus the high-level generation
/// knobs. Same seed + same params → an identical world on a given binary. Threaded
/// through make_hard_coded_world and edited on the main-menu New World setup (BL-114);
/// it lives in the app, not the `world` struct, so it stays off the serialisation seam.
struct world_params
{
    uint32_t        seed       = 0;                         ///< Master seed, XOR-folded into each per-body seed. 0 reproduces the legacy world.
    abundance_level abundance  = abundance_level::standard; ///< Deposit-density tier (standard = earth-like ceiling).
    int             body_count = 0;                         ///< Reserved — the body-count knob is PHASED to a follow-on (bodies are still hard-coded profiles).
    // Note: there is no nation-count knob. The number of nations on the home body is a
    // *consequence* of its habitable land area and the minimum-viable-territory floor
    // (nation_params in world/nation_generation.hpp), not a value the player pre-sets.
};

/// Construct and return a world populated with the prototype's authored bodies.
///
/// Bodies (all orbiting the star Helios):
///   Cinder — hot inner planet, 180×84 procedural tile grid.
///   Kepler — temperate rocky planet, 180×84 procedural tile grid; the
///            corporation's home body, with two surface installations and a
///            market.
///   Selene — Kepler's moon, 90×42 procedural tile grid.
///
/// Grids follow a ~9:5 width:height ratio (see PLANETARY.md). All values are
/// hand-authored for prototype testing. Replace this function with a
/// data-driven loader when scripted body definitions are added.
///
/// @param params The world descriptor — seed + generation knobs. Defaulted so the
///               legacy call `make_hard_coded_world()` reproduces the original world.
/// @return A fully populated world ready to drive the simulation.
world make_hard_coded_world(world_params params = {});
