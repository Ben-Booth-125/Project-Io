#pragma once

#include "components.hpp"
#include "entity.hpp"

#include <cstddef>
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
///
/// APPEND-ONLY. The value is what a `corp_command` carries and what a decision
/// log records, so inserting a verb mid-enum would silently re-interpret every
/// stored command. New verbs go on the end.
enum class corp_verb : uint8_t
{
    build = 0,     ///< construct_building(type, tile, target, recipe) — durative, pay-as-you-build.
    demolish,      ///< demolish_building(building).
    set_recipe,    ///< building_component.recipe := recipe (BL-079 idiom, validated).
    set_workforce, ///< building_component.workforce_target := value (0–200); clears workforce_auto.
    idle,          ///< building_component.decommissioned := true.
    resume,        ///< building_component.decommissioned := false.
    place_road,    ///< place_road(tile, tier).
    survey,        ///< dispatch_survey(body) on behalf of the corp.
    hire_unit,     ///< Raise a unit at a tile from the campaign roster (BL-324).
    // --- BL-293: the order book joins the seam (2026-08-07) ---
    // Appended AFTER hire_unit: the enum is serialised and append-only, and
    // hire_unit already holds its value in main's dictionary and harnesses.
    place_sell_order,   ///< Push a standing sell order onto world.sell_orders (subject = body).
    remove_sell_order,  ///< Erase the standing sell order with id `order`.
    set_workforce_auto, ///< building_component.workforce_auto := true (hand the dial back).
    // --- BL-350: the procurement/contract seam joins the seam (2026-08-11) ---
    // Appended AFTER set_workforce_auto, same append-only rule.
    request_quote,   ///< Ask `counterparty` for a price + lead time on `target`/`quantity` at `subject` (body).
    accept_quote,     ///< Convert the live quote `order` into a contract; debits the deposit.
    cancel_contract,  ///< Terminate contract `order` in flight; forfeits the deposit, moves reputation.
};

/// One past the highest verb — the wire parser's range gate (BL-396: run_serve
/// refuses any `verb=` outside [0, corp_verb_count) rather than letting a
/// narrowing cast truncate it into a verb the caller never named). Bound to
/// the append-only rule above: this is always `<last enumerator> + 1`, so
/// appending a verb means moving this with it — and only this, since existing
/// values never renumber.
inline constexpr uint8_t corp_verb_count =
    static_cast<uint8_t>(corp_verb::cancel_contract) + 1;

/// Ceiling on one corporation's outstanding sell orders. The book is now
/// reachable by command, so it is reachable by a scorer with a bug in it — this
/// is the bound that keeps a runaway from growing the save format without limit.
/// Far above any real book: the Market Ledger lists a handful per body, and the
/// AI places at most one per evaluation. `place_sell_order` returns
/// `rejected_state` at the cap rather than silently dropping the order.
inline constexpr std::size_t max_sell_orders_per_corp = 64;

/// One serialisable intent: {tick, corp, verb, fixed-size args}. Which args are
/// meaningful depends on the verb; unused args stay at their defaults so two
/// commands with the same intent compare equal byte-for-byte.
struct corp_command
{
    int        tick = 0;               ///< Sim tick the command was issued for.
    entity_id  corp = null_entity;     ///< Acting corporation.
    corp_verb  verb = corp_verb::build;

    /// Primary subject: the building (demolish / set_recipe / set_workforce /
    /// idle / resume / set_workforce_auto) or the body (survey,
    /// place_sell_order). null for build / place_road, whose subject is `tile`,
    /// and for remove_sell_order, whose subject is `order`.
    entity_id  subject = null_entity;

    entity_id     tile      = null_entity;              ///< build / place_road target tile.
    building_type type      = building_type::none;      ///< build only.
    resource_type target    = resource_type::iron_ore;  ///< build (extraction) AND place_sell_order: what to sell.
    uint16_t      recipe    = no_recipe;                ///< set_recipe (and build seed).
    int           workforce = 100;                      ///< set_workforce value [0, 200].
    uint8_t       road_tier = 1;                        ///< place_road tier (1–3).
    uint16_t      unit_type = 0;                        ///< hire_unit: index into unit_roster_table().

    // --- BL-293 order-book args ---
    float    quantity    = 0.0f; ///< place_sell_order: max units offered per tick (> 0). Also request_quote.
    float    floor_price = 0.0f; ///< place_sell_order: minimum unit price (>= 0; 0 = market price).
    uint32_t order       = 0;    ///< remove_sell_order: the sell_order::id to erase. Also accept_quote /
                                  ///< cancel_contract: the procurement_quote / procurement_contract id.

    // --- BL-350 procurement args ---
    /// request_quote: the supplier corp being asked. `subject` names the body
    /// the contract fulfils at; `target` (resource_type, reused from `build`)
    /// names the resource; `quantity` names the amount.
    entity_id counterparty = null_entity;
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
    rejected_era_locked,    ///< BL-433: the type is not in the campaign's era band (a Launchpad at 0 CE). Distinct from tech_locked: no research reaches it.
    // --- BL-350: request_quote's four distinguishable decline conditions ---
    rejected_no_capacity,     ///< The named supplier holds no completed building that can produce the good.
    rejected_no_input_access, ///< The supplier's local market cannot supply the recipe's inputs.
    rejected_embargo,         ///< The supplier's condition_set evaluates false against the buyer.
    rejected_reputation,      ///< The (buyer, supplier) reputation pair sits below the standing floor.
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
    trade_surplus,    ///< BL-293: stock piled up past the hold threshold; list it with a floor.
};

/// One ring-buffer entry: the command plus its score rationale.
struct corp_decision
{
    int                  tick          = 0;
    entity_id            corp          = null_entity;
    corp_command         command;
    float                winning_score = 0.0f;
    /// Score of the best candidate the corp did NOT take in this evaluation —
    /// the highest-scoring one rejected by an action budget, the one-touch rule,
    /// the solvency gate or the seam. 0 when nothing was passed up.
    ///
    /// Was "the next candidate in sort order" until NR-232 (2026-08-14), which
    /// was misleading: one evaluation applies up to seven commands, so the next
    /// candidate was frequently a command that ALSO ran, and a reader comparing
    /// the two scores saw a contest that never happened. Every decision from one
    /// evaluation now carries the same value — the foregone option belongs to
    /// the evaluation, not to the individual command.
    float                runner_up     = 0.0f;
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
