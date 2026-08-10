#pragma once

#include "components.hpp"
#include "entity.hpp"

#include <cstdint>
#include <vector>

struct world;
class recipe_registry;

// ---------------------------------------------------------------------------
// Corp-command seam (BL-202, docs/ai/AI_OPPONENT.md § 6)
// ---------------------------------------------------------------------------
// The strategic AI never mutates the world directly: it emits corp_command
// records that are applied at the tick boundary through apply_corp_command,
// which dispatches to the *player-grade* validation seams (construct_building,
// demolish_building, place_road, dispatch_survey, and the BL-079 component
// idioms for recipe/workforce/idle). No bypass, no cheats by construction.
// The same record shape is the lockstep intent a networked player would send
// (MULTIPLAYER_PRINCIPLES § Preserve now #2) and the contract an out-of-process
// policy (stage C/D) would return.

/// The AI's verbs — exactly the player's action surface.
enum class corp_verb : uint8_t
{
    build = 0,     ///< construct_building(type, tile, target) — durative, pay-as-you-build.
    demolish,      ///< demolish_building(building).
    set_recipe,    ///< building_component.recipe := recipe (BL-079 idiom, validated).
    set_workforce, ///< building_component.workforce_target := value (0–200).
    idle,          ///< building_component.decommissioned := true.
    resume,        ///< building_component.decommissioned := false.
    place_road,    ///< place_road(tile, tier).
    survey,        ///< dispatch_survey(body) on behalf of the corp.
    hire_unit,     ///< Raise a unit at a tile from the campaign roster (BL-324).
};

/// One serialisable intent: {tick, corp, verb, fixed-size args}. Which args are
/// meaningful depends on the verb; unused args stay at their defaults so two
/// commands with the same intent compare equal byte-for-byte.
struct corp_command
{
    int        tick = 0;               ///< Sim tick the command was issued for.
    entity_id  corp = null_entity;     ///< Acting corporation.
    corp_verb  verb = corp_verb::build;

    /// Primary subject: the building (demolish / set_recipe / set_workforce /
    /// idle / resume) or the body (survey). null for build / place_road, whose
    /// subject is `tile`.
    entity_id  subject = null_entity;

    entity_id     tile      = null_entity;              ///< build / place_road target tile.
    building_type type      = building_type::none;      ///< build only.
    resource_type target    = resource_type::iron_ore;  ///< build (extraction) only.
    uint16_t      recipe    = no_recipe;                ///< set_recipe (and build seed).
    int           workforce = 100;                      ///< set_workforce value [0, 200].
    uint8_t       road_tier = 1;                        ///< place_road tier (1–3).
    uint16_t      unit_type = 0;                        ///< hire_unit: index into unit_roster_table().
};

/// Outcome of applying a command. Only `applied` mutates the world; every
/// rejection leaves it untouched (the underlying seams guarantee this).
enum class corp_command_result : uint8_t
{
    applied = 0,
    rejected_no_corp,       ///< Acting corporation does not exist.
    rejected_not_owner,     ///< Subject building is not owned by the corp.
    rejected_invalid,       ///< Bad subject / tile / recipe / verb arguments.
    rejected_placement,     ///< construct_building / place_road refused the tile.
    rejected_funds,         ///< Solvency check inside the seam refused the spend.
    rejected_state,         ///< No-op in current state (already idle, survey in progress, ...).
    rejected_tech_locked,   ///< BL-344: the corp has not earned the tech that unlocks this type.
};

/// Apply one command through the player-grade seams. Deterministic; a rejected
/// command mutates nothing. `out_building` (optional) receives the entity id a
/// successful `build` or `hire_unit` created.
corp_command_result apply_corp_command(world& w, const recipe_registry& reg,
                                       const corp_command& cmd,
                                       entity_id* out_building = nullptr);

// ---------------------------------------------------------------------------
// Decision log (the AI's legibility surface / replay artifact)
// ---------------------------------------------------------------------------

/// Why the scorer picked (or was allowed to pick) this command.
enum class corp_decision_reason : uint8_t
{
    best_build = 0,   ///< Won the bounded build-site enumeration.
    dial_workforce,   ///< Workforce dial beat the incumbent by the hysteresis margin.
    dial_recipe,      ///< Recipe margin-chase beat the incumbent.
    dial_idle,        ///< Sustained loss; idling saves more than running.
    dial_resume,      ///< Idled asset now reads profitable.
    survey_expand,    ///< Discovery spend within the solvency floor.
    hire_available,   ///< Roster row available under the corp's own campaign gate (BL-324).
};

/// One ring-buffer entry: the command plus its score rationale.
struct corp_decision
{
    int                  tick          = 0;
    entity_id            corp          = null_entity;
    corp_command         command;
    float                winning_score = 0.0f;
    float                runner_up     = 0.0f; ///< Next-best candidate's score (0 if none).
    corp_decision_reason reason        = corp_decision_reason::best_build;
};

/// Fixed-capacity ring of the most recent strategic decisions, world-wide.
/// Derived observability, not save-format state — bounded so the log can run
/// forever. Insertion order is the deterministic application order.
struct corp_decision_ring
{
    static constexpr std::size_t capacity = 256;

    std::vector<corp_decision> entries; ///< Grows to `capacity`, then wraps.
    std::size_t                next = 0; ///< Slot the next entry overwrites once full.
    std::size_t                total = 0; ///< Lifetime entries pushed (telemetry).

    void push(const corp_decision& d)
    {
        if (entries.size() < capacity)
        {
            entries.push_back(d);
        }
        else
        {
            entries[next] = d;
            next = (next + 1) % capacity;
        }
        ++total;
    }
};
