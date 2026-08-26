#pragma once

#include "recipe_registry.hpp"
#include "world.hpp"

#include <array>
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

    /// Baseline starting capital before wealth variance is applied. Set to 0
    /// 2026-07-06 (playtest patch, Ben's steer) — model every corp as a *new
    /// charter*: it opens with no seeded cash at all. Its actual opening balance
    /// comes entirely from the pre-game warm start (app::start_new_game runs
    /// 12 economy ticks — ~3 in-game years — against the generation-time asset
    /// placement before turn one), so a corp's capital is *earned*, grounded in
    /// its own simulated production/wages/trade rather than an arbitrary lump
    /// sum. (Interim step was 4000, replacing an original 100000 that dwarfed
    /// building costs by 200-1000x.) `compute_capital`/`compute_starting_stockpile`
    /// already guard base_capital <= 0 (cap_scalar defaults to 1.0), so this is a
    /// supported value, not a special case bolted on.
    float base_capital = 0.0f;

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
/// @param settle Optional settlement/industrialisation record for the home body
///               (BL-219). When supplied, Pass 2 stops drawing a focus from the
///               authored weighting table and DERIVES it from the region the
///               corporation is anchored to: a region that industrialised
///               around ore emits extraction/processing corps, a coastal trade
///               region emits trade-focused ones, and an early industrialiser
///               skews one tier further up the value chain. Diversity then comes
///               from a world-level reject-and-reroll against a floor on the
///               SET — never a quota on any member — so Pass 2's
///               diversity-without-quota property survives the rewrite. Null
///               (the default) keeps the pre-BL-219 authored table, so a body
///               with no settlement pass generates exactly as before.
/// @param progress Optional progress sink (BL-305). When non-null, Pass 3
///               publishes each holding's grid position as it is staked (the
///               POSITIONAL half — it lands on the loading screen's carve map)
///               and Pass 5's assembly publishes each corp's focus / holdings /
///               capital (the NON-POSITIONAL half — a financial profile has
///               nowhere on a map to be, so it goes to a ledger column). A pure
///               TAP: write-only, consumes no randomness, changes no branch.
///               Defined in world/hard_coded_world.hpp.
/// @return       Corporation entity IDs in generation order (one per corporation
///               created). The entry whose corporation_component::is_player is
///               true equals w.player_entity.
std::vector<entity_id> generate_corporations(
    world& w,
    const corporation_params& params,
    uint32_t seed,
    const struct settlement_state* settle = nullptr,
    struct generation_progress* progress = nullptr);

// ---------------------------------------------------------------------------
// Pass 2b — ownership class (BL-631)
//
// Exposed rather than file-local for one reason: the ownership_class harness
// has to check the MAPPING, not just its end-to-end consequences, and a check
// that can only observe finished corporations cannot tell a correct derivation
// from a lucky one. Both functions are pure: no rng parameter, no world, no
// hidden state — which is what makes "the derivation consumes no additional
// randomness" a property of the signature rather than a claim about the body.
// ---------------------------------------------------------------------------

/// Pass 2b's derivation: the ownership class a corporation anchored in @p p
/// carries. Reads HISTORY.md's **Stage 1** — the enforceable promise — through
/// the charter's diffusion frame, never an authored table (BL-219's principle).
///
///  * the charter never reached here   -> `closed`  (no filing, no market in the firm)
///  * it reached here only as a COPY   -> `private` (it trades; its books are its own)
///  * this ground LIVES the promise    -> `public`  (it files, and it can be bought)
///  * lives it, but under a STATIST polity -> `private`
///
/// WHY STAGE 1 AND NOT STAGE 4 (BL-638). The obvious reading — early
/// industrialisers get public firms — is the wrong one, and it was measured
/// wrong once: an antiquity world skips Stage 4 entirely, so
/// `median_industrial_year` is 0 there and the never-industrialised rung fires
/// for every region in every default campaign. All 64 corporations across 8
/// seeds classed `closed`; nothing filed and nothing was buyable. Stage 1 runs
/// in every era, so a class derived from it is era-agnostic where a class
/// derived from industry is silently post-industrial.
///
/// @param p        The corporation's home region.
/// @param ch       `settlement_state::charter` — the charter's diffusion frame.
/// @param politics The home NATION's ideology. It supplies only the
///                 holds-the-firm-close DEMOTION; the region stays the primary
///                 discriminator, so two corps in one nation still differ
///                 (CORPORATION_GENERATION.md § Pass 2 - "per-region, not
///                 per-nation, and that is the point").
ownership_class ownership_from_region(const struct region& p,
                                      const struct charter_reach& ch,
                                      ideology politics);

/// The no-home-region fallback: the same read one grain up, taken from the
/// national character alone. Used by the rung-3 case (a nation the settlement
/// pass never reached), by the whole no-settlement path, and by every BL-365
/// background firm — which has a home nation but never a home region. Nothing
/// here branches on `is_background`; the flag is not an input.
///
/// The nation's `politics` IS its industrialisation-timing tercile
/// (settlement.cpp: never -> isolationist, early -> mercantile, mid ->
/// technocratic, late -> authoritarian), so this is the region mapping with the
/// tercile standing in for the region's own furnace year.
ownership_class ownership_from_character(ideology politics);


/// BL-365 — generate REAL background corporations (`is_background = true`) that
/// produce and consume through the normal recipe/workforce/market pipeline,
/// replacing the old abstract nation-substrate demand/supply injection
/// (BL-078's `inject_substrate_demand`, deleted). Reuses this file's own
/// clustered asset-placement machinery (`place_starting_assets`), so a
/// background firm's holdings read exactly like a rival's: a focus-shaped
/// cluster anchored on a scored tile, not a scatter.
///
/// MUST run after `generate_corporations` (so player/rival holdings claim the
/// choicest tiles first — background firms fill what's left) AND after the
/// recipe_registry is loaded from Lua, because the stop condition below reads
/// real recipe outputs. `make_hard_coded_world` runs BEFORE the Lua economy
/// layer loads (see app::setup_world / app::load_economy ordering, and the
/// identical constraint the pre-game warm start documents in app.cpp) — so,
/// unlike `generate_corporations`, this function is NOT called from inside
/// `hard_coded_world.cpp`. Callers invoke it once `reg` is loaded, mirroring
/// the pre-game-warm-start seam: `app::run` calls it right after
/// `load_economy()`, before the warm-start ticks (so the warm start also seeds
/// the new firms' opening balances); the headless `--serve` / blackboard-export
/// paths in `main.cpp` call it right after their own `reg.load_from_lua`.
///
/// MEASURED stop condition (the load-bearing decision, not a fixed firm count):
/// for each body carrying at least one population centre, spawns one firm at a
/// time — extraction or processing, whichever the current biggest per-resource
/// production/demand gap calls for — until aggregate real production (summed
/// from every building's actual recipe/extraction output, generated firms
/// included) reaches ~90% of aggregate demand (population + BL-340 background
/// pull, the same clearing_fraction the deleted substrate model used) over the
/// body's tradeable resource set, or a bound bites (max_firms_per_body /
/// max_iterations) — logged, not crashed, if 90% is unreachable (e.g. a body
/// missing the resource's deposit entirely).
///
/// Deterministic in @p seed (xor-offset from `generate_corporations`'
/// seed_asset stream so the two passes cannot collide).
///
/// @param w   World already carrying generated nations, corporations, tiles,
///            and markets (`generate_corporations` + market seeding must have
///            already run).
/// @param reg Loaded recipe registry — supplies recipe outputs, building
///            economics, population_demand and background_demand tunables.
/// @param seed Per-run RNG seed for deterministic, reproducible placement.
/// @return    Entity ids of the background corporations created (possibly
///            empty if every body is already at/above the clearing fraction,
///            or no body qualifies).
std::vector<entity_id> generate_background_firms(
    world& w,
    const recipe_registry& reg,
    uint32_t seed);

/// Give every processing facility that still carries `no_recipe` the registry's
/// era-aware default. Idempotent, and a no-op on a world where every processor
/// is already configured.
///
/// **Why this is a function and not three lines in the caller.** `author_building`
/// cannot set a recipe: a recipe id is an index into a registry that does not
/// exist yet when `make_hard_coded_world` runs (the Lua economy layer loads
/// after world generation — see app::setup_world / app::load_economy ordering).
/// So the field is left `no_recipe` and backfilled once the registry exists.
///
/// That backfill used to live inline in `app::load_economy`, which made a
/// **world-generation invariant depend on the UI's startup sequence**. Every path
/// that builds a world without going through `app` — every headless harness,
/// `--serve`, `--verify` — got generated processors that could never produce, and
/// nothing said so: they report as idle with `no_recipe`, which reads as an
/// economic outcome rather than a missing initialisation. Measured at **20.3% of
/// all processing building-ticks** in `tier_margin` before this moved (2026-08-17),
/// silently dragging down every mean the BL-436 calibration was being read off.
///
/// Call it after the registry is loaded and after every generation pass that can
/// author a processor (`generate_corporations`, `generate_background_firms`).
void assign_default_recipes(world& w, const recipe_registry& reg);

/// Measurement seam (2026-08-20) — the SHIPPED background-firm stop condition,
/// readable from outside. `generate_background_firms` stops when the basket-
/// weighted production/demand ratio reaches its target (0.90) OR when it hits
/// `max_firms_per_body`; which one fires decides whether a body's markets open
/// stocked or thin, and nothing outside that file could previously ask.
///
/// Exported rather than re-derived on purpose: a harness that re-implements a
/// generation rule drifts from it, which has now cost this project four wrong
/// confident measurements (world_audit.cpp B4 is the latest).
///
/// All three are pure reads over @p w and @p reg. A body with no measurable
/// demand reads as fully met (ratio 1.0) — there is nothing to fill.
std::array<float, resource_count> measure_body_demand(const world& w,
                                                     const recipe_registry& reg,
                                                     entity_id body_id);
std::array<float, resource_count> measure_body_production(const world& w,
                                                          const recipe_registry& reg,
                                                          entity_id body_id);
float measure_production_ratio(const world& w, const recipe_registry& reg,
                               entity_id body_id);
