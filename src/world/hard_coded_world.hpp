#pragma once

#include "continents.hpp"
#include "planetology.hpp"
#include "world.hpp"
#include "world_gen_config.hpp"

#include <cstdint>
#include <string>
#include <vector>

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

    /// What the player expressed on the New World wizard (BL-167). Preferences,
    /// not parameters: make_hard_coded_world resolves these against the seed —
    /// rejecting and rerolling until the homeworld clears the strict Earth-like
    /// floor — and the resolved values land in the generation_report.
    world_preferences preferences{};
};

/// What the generation pass recorded about each body, for the staged generation
/// screen and the planet report.
///
/// This is a PRESENTATION artefact, not simulation state: it is filled during
/// make_hard_coded_world and handed to the app, which reveals it stage by stage.
/// It never enters the `world` struct, so it stays off the serialisation seam —
/// the same reasoning that keeps world_params in the app (BL-114).
struct generation_report
{
    struct body_entry
    {
        std::string       name;
        planetology_state state;

        /// The same body with the industrial drawdown dialled to zero — what the
        /// chain FORMED, before a prior era took the accessible half of it. Kept
        /// alongside the real state so the History ledger can redraw the wizard's
        /// formed-against-left chart, which otherwise has no "before" to point at
        /// (the wizard computes it from a live second preview; a loaded campaign
        /// has no preview to consult). Drawdown consumes no randomness, so this is
        /// the same world minus its industrial history — not a second roll.
        planetology_state undrawn;

        /// What the Continents/Drift pass computed for this body (BL-226). Its
        /// `history` is empty here — those lines were moved into `state.history`
        /// at generation, where the biography reads them; what is kept is the
        /// plate set and the per-tile `plate_id`, which nothing else records.
        /// The Continent lens is the consumer. Presentation data, like the rest
        /// of this struct: it never enters `world`, so it stays off the
        /// serialisation seam.
        continent_state continents;
    };

    world_preferences       preferences{}; ///< What the player asked for.
    planetology_params      params{};      ///< What the seed actually rolled within it.
    float                   home_orbit_au = 1.0f; ///< Derived from the star, not authored.
    uint32_t                attempts      = 1;    ///< Viability draws consumed (reroll cost).
    std::vector<body_entry> bodies;

    /// Per-stage one-line summary of what the chain did across the whole system,
    /// indexed by chain_stage. This is what the staged generation screen reveals.
    std::vector<std::string> stage_lines;
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
/// @param report Optional out-param; when non-null, receives the per-body Planetology
///               results and the per-stage summaries the generation screen reveals.
///               The common path passes nullptr and pays nothing.
/// @param gen_cfg Balance values authored in scripts/world_gen.lua (BL-236). Defaulted
///               so a headless caller that never touches Lua reproduces the same world.
/// @return A fully populated world ready to drive the simulation.
world make_hard_coded_world(world_params params = {}, generation_report* report = nullptr,
                            const world_gen_config& gen_cfg = {});
