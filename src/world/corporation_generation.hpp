#pragma once

#include "world.hpp"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Procedural corporation generation
//
// Deterministic, five-pass corporation generation for the campaign start state.
// Runs after nation generation; reads nation_component and tile_component data
// from `w` and writes corporation_component entries plus building/stockpile
// components back into `w`. Also sets w.player_entity.
// See docs/generation/CORPORATION_GENERATION.md for the design authority and
// per-pass rules.
// ---------------------------------------------------------------------------

/// Tunable parameters for a corporation generation run.
struct corporation_params
{
    /// Total number of corporations to generate (including the player's).
    /// The prototype targets 6–10 on Kepler.
    int corporation_count = 8;

    /// Baseline starting capital before wealth variance is applied.
    float base_capital = 100000.0f;

    /// Fractional spread around base_capital. A value of 0.4 means each
    /// corporation's capital is drawn from [base × (1 − 0.4), base × (1 + 0.4)].
    float wealth_variance = 0.4f;
};

/// Generate corporations over the nation map already in @p w and register all
/// results in @p w. Also sets w.player_entity to the entity id of the
/// corporation flagged as the player's.
///
/// Runs the five-pass corporation generation pipeline (nation assignment,
/// industrial focus, starting asset placement, financial profile, naming)
/// described in docs/generation/CORPORATION_GENERATION.md. Requires that
/// w.nations is non-empty (i.e. generate_nations has already been called);
/// returns {} immediately if it is empty.
///
/// Deterministic in @p seed: the same seed, nation map, and params always
/// produce the same set of corporations. Does not use std::random_device or
/// global state.
///
/// @param w      World holding nation and tile components; receives new
///               corporation entities, building entities, and stockpile entries.
///               w.player_entity is set before this function returns.
/// @param params Tunable generation parameters.
/// @param seed   Per-run RNG seed for independent, reproducible results.
/// @return       Corporation entity IDs in generation order (one per corporation
///               created). The entry whose corporation_component::is_player is
///               true equals w.player_entity.
std::vector<entity_id> generate_corporations(
    world& w,
    const corporation_params& params,
    uint32_t seed);
