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
    // --- BL-452: the logistics layer joins the seam (2026-08-17) ---
    // Appended AFTER cancel_contract, same append-only rule. Layer 5 — the only
    // coupling between two markets' prices — was entirely automatic until this
    // pair: fifteen verbs named a building, an order, a contract, a body or a
    // tile, and not one named a convoy.
    dispatch_convoy,  ///< Put a convoy on a lane: `subject` = source market, `counterparty` = destination market, `target` = cargo, `quantity` = units.
    hold_convoy,      ///< Toggle convoy `order` between held and moving. NOT a cancel — see the verb's comment in corp_command.cpp.
    // --- BL-448: corp stance joins the seam (2026-08-19) ---
    // Appended AFTER hold_convoy, same append-only rule. Data model + verbs
    // only — gates nothing yet (BL-315 is the future engagement trigger).
    declare_hostile,    ///< `corp` declares hostile toward `counterparty`. Unilateral; dissolves any friendship row atomically.
    offer_friendship,   ///< `corp` offers friendship to `counterparty`. Records a PENDING offer only, not a stance.
    accept_friendship,  ///< `corp` accepts a pending offer FROM `counterparty`.
    return_to_neutral,  ///< `corp` clears its own hostility toward, and/or any friendship with, `counterparty`. Unilateral.
    // --- BL-470: the unit march seam (2026-08-19) ---
    // Appended AFTER return_to_neutral, same append-only rule. One seam, one
    // dictionary family — NOT a parallel unit_command type (BL-470's ruling 2).
    // BL-511 (2026-08-21) RETARGETED march_unit's PAYLOAD from a tile to a
    // PROVINCE. The verb keeps its value and its position — the enum is
    // serialised and append-only, so nothing renumbers; only the field it
    // reads changed, from `tile` to `province`. Ben's grain ruling (unit
    // position and movement are province grain) collapses BL-467's
    // command-at-tile / engagement-at-province split; see NR-405.
    march_unit,    ///< Set/replace `subject`'s (a unit) movement order toward `province`, path-marched across ticks.
    halt_unit,     ///< Clear `subject`'s movement order. rejected_state if it has none.
    disband_unit,  ///< Erase `subject` (a unit) outright. No refund — manpower walks away.
    // --- BL-467: the battle seam (2026-08-21) ---
    // Appended AFTER disband_unit, same append-only rule. The engagement TRIGGER
    // is deliberately not a verb — a battle opens because two hostile forces are
    // in the same province, not because someone asked. Breaking off is the only
    // decision a commander actually has once contact is made, so it is the only
    // thing the seam exposes.
    withdraw_from_battle, ///< `corp` breaks off the battle it is fighting in `province` against `counterparty` (null = the first in sorted order). Honoured at the tick boundary, before the next round batch; priced by the resolver's three-term withdrawal cost.
    // --- BL-573: the mercenary-contract seam (2026-08-23) ---
    // Appended AFTER withdraw_from_battle, same append-only rule.
    // CONTRACTS.md § The mercenary contract: "a contract is a condition_set
    // the client will pay to have become true, by a deadline". These two
    // verbs are the whole player/agent surface onto it — accepting an OFFER
    // (BL-572) with a named force, and walking away from a live one early.
    //
    // ── DORMANT: THE MERCENARY CONTRACT IS RETIRED (BL-693) ──────────────────
    // Ben, 2026-08-29: "I don't think mercenary contracts is the correct
    // system." No surface reaches these two verbs any more — the Contracts
    // ledger and its rail slot are deleted — and no scorer candidate issues
    // them. They remain callable, and they still apply, ONLY because the
    // enum is APPEND-ONLY: removing a verb renumbers every verb below it and
    // breaks the ACTIONS.json dictionary and every recorded command. They are a
    // dormant record, not a live mechanism; see `mercenary_offer` in world.hpp.
    //
    // The `request_quote` / `accept_quote` / `cancel_contract` trio — PROCUREMENT,
    // the BUY side — is a different system and is fully live. Do not confuse them.
    accept_offer,      ///< Convert live offer `order` (a mercenary_offer id) into a mercenary_contract, committing `units` (the corp's own). `counterparty` names the offer's client NATION for this verb, not a corp — the seam's counterparty check forks on verb; see corp_command.cpp.
    abandon_contract,  ///< `corp` (the contractor) walks away from live contract `order` early. Same money outcome as a failure (deposit only); a distinct, lesser sentiment magnitude (CONTRACTS.md § Q2).
    // --- BL-616: razing joins the seam (2026-08-25) ---
    // Appended AFTER abandon_contract, same append-only rule. The deliberate
    // destruction act POPULATION.md § Growth, decline and razing reserves for
    // an agent in occupation — passive decline only ever shrinks a centre.
    // DELIBERATELY NOT in corp_ai.cpp's candidate list: no standing-rules
    // grant covers a rival razing, so the verb exists on the seam (player /
    // agent reach) and the scorer never emits it.
    raze_centre,       ///< `corp` razes population centre `subject`. Occupation-context: requires the corp's own military presence (a unit) on the centre's body. Demotes to the razed tier (BL-624) — population zeroed, entity/name/tile/urban ground kept, the province anchor survives; the growth pass re-settles at a reduced gate.
    // --- BL-628: the whole-firm buyout (2026-08-26) ---
    // Appended AFTER raze_centre, same append-only rule. THIS VERB IS UNLIKE
    // EVERY OTHER ONE ON THE SEAM: the rest move goods, credits, units,
    // sentiment or a building; this one ERASES AN ACTOR. Nothing else in the
    // codebase removes a corporation, which is why the whole-world dissolution
    // walk (corp_command.cpp § dissolve_into) is the substance of the verb and
    // the price is the easy half. FINANCE.md § Whole-firm acquisition.
    buy_corporation,   ///< `corp` buys `counterparty` (a PUBLIC corporation) outright at `book_value + k_acquisition_multiple x trailing_net + balance`, floored at 0. Holdings, pools, balance and filed returns transfer; the target is dissolved. Never a fractional stake — there is no equity relation to hold.
};

/// One past the highest verb — the wire parser's range gate (BL-396: run_serve
/// refuses any `verb=` outside [0, corp_verb_count) rather than letting a
/// narrowing cast truncate it into a verb the caller never named). Bound to
/// the append-only rule above: this is always `<last enumerator> + 1`, so
/// appending a verb means moving this with it — and only this, since existing
/// values never renumber.
inline constexpr uint8_t corp_verb_count =
    static_cast<uint8_t>(corp_verb::buy_corporation) + 1;

/// Ceiling on one corporation's outstanding sell orders. The book is now
/// reachable by command, so it is reachable by a scorer with a bug in it — this
/// is the bound that keeps a runaway from growing the save format without limit.
/// Far above any real book: the Market Ledger lists a handful per body, and the
/// AI places at most one per evaluation. `place_sell_order` returns
/// `rejected_state` at the cap rather than silently dropping the order.
inline constexpr std::size_t max_sell_orders_per_corp = 64;

/// Fixed capacity of a mercenary contract's committed force
/// (`mercenary_contract::units`, world.hpp — BL-573) and of this command's own
/// `units` field below, which mirrors it exactly: a wire-safe fixed-size array
/// is what the untrusted-input rule can range-check field by field, the same
/// reason `corp_command` itself carries no `std::vector`. Eight is far above
/// any single contract's real force at the prototype's roster scale. Defined
/// here (not in world.hpp, where `mercenary_contract` lives) because
/// `world.hpp` includes THIS header, not the reverse — a single definition
/// both sides reach without a cycle.
inline constexpr std::size_t mercenary_contract_max_units = 8;

/// BL-511: "no province named" for `corp_command::province`. Deliberately
/// UINT32_MAX and deliberately NOT 0 — 0 is a REAL province id under
/// province.hpp's layout (body rank 0 | block 0 | component 0), so it cannot
/// serve as an absent-value sentinel. UINT32_MAX is structurally unreachable
/// instead: it requires component index 7, and a 2x2 block splits into at
/// most 4 connected components, so the partition never assigns above 3.
inline constexpr uint32_t no_province = 0xFFFFFFFFu;

/// One serialisable intent: {tick, corp, verb, fixed-size args}. Which args are
/// meaningful depends on the verb; unused args stay at their defaults so two
/// commands with the same intent compare equal byte-for-byte.
struct corp_command
{
    int        tick = 0;               ///< Sim tick the command was issued for.
    entity_id  corp = null_entity;     ///< Acting corporation.
    corp_verb  verb = corp_verb::build;

    /// Primary subject: the building (demolish / set_recipe / set_workforce /
    /// idle / resume / set_workforce_auto), the body (survey,
    /// place_sell_order), or the unit (march_unit / halt_unit / disband_unit,
    /// BL-470). null for build / place_road, whose subject is `tile`,
    /// and for remove_sell_order, whose subject is `order`.
    entity_id  subject = null_entity;

    entity_id     tile      = null_entity;              ///< build / place_road / hire_unit target tile. NOT march_unit's — see `province` (BL-511).
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
                                  ///< BL-573 reuses it again: accept_offer's mercenary_offer id,
                                  ///< abandon_contract's mercenary_contract id — the same "an id
                                  ///< names the thing this verb acts on" reading every prior reuse
                                  ///< carries, never two ids in flight on one command.

    // --- BL-350 procurement args ---
    /// request_quote: the supplier corp being asked. `subject` names the body
    /// the contract fulfils at; `target` (resource_type, reused from `build`)
    /// names the resource; `quantity` names the amount.
    ///
    /// BL-452 reuses this field for `dispatch_convoy`'s DESTINATION MARKET —
    /// the field is "the other end of the transaction" in both readings, and a
    /// fifth entity_id arg for one verb would grow the record every command
    /// pays for. `subject` is the source market, `target` the cargo,
    /// `quantity` the units; `order` names the convoy for `hold_convoy`.
    entity_id counterparty = null_entity;

    // --- BL-511: unit movement moves to province grain (2026-08-21) ---
    /// `march_unit`'s DESTINATION PROVINCE (a `province::id`, world.hpp's
    /// `province_partition`). Not an `entity_id`: a province id is DERIVED
    /// from its body rank / block / component (province.hpp's layout), never
    /// allocated from the entity pool, so it is its own uint32 domain and
    /// must not be conflated with `tile`.
    ///
    /// The default is `no_province`, NOT 0. The ORIGINAL reason (2026-08-21,
    /// BL-511's seam half) was that province id 0 was a REAL id under the
    /// block-derived layout `body_rank | block | component`, where it named
    /// the first block of the first body on every fixture and generated
    /// world — so a 0 default would have made an OMITTED `province=` on the
    /// wire resolve to a real destination and answer `applied`, precisely the
    /// silent order-substitution the untrusted-input rule forbids.
    ///
    /// BL-515 (same day) replaced that layout: an id is now the province's
    /// LOWEST-ID MEMBER TILE, and `province_partition_harness` P8d asserts no
    /// id is 0. So 0 is unreachable today and the original argument no longer
    /// holds — but the DEFAULT IS UNCHANGED AND SHOULD STAY, for a reason that
    /// does not depend on the id scheme: `no_province` is refused by the seam,
    /// so an omitted field is rejected rather than silently interpreted. That
    /// property is the one worth keeping, and a default of 0 would only be safe
    /// for as long as 0 happens to be unreachable.
    ///
    /// `no_province` is refused by the seam because
    /// `province_partition::find` returns nullptr for it, which is the
    /// authoritative domain check — a wire caller's range gate only proves
    /// the value fits, and fitting is not existing.
    uint32_t province = no_province;

    // --- BL-573: the mercenary-contract seam (2026-08-23) -------------------
    /// accept_offer's committed force — the corp's OWN units, named by the
    /// caller. Unused slots are `null_entity`; the seam counts a slot as
    /// "named" iff it is not `null_entity`, so a caller need not also send a
    /// count. "The player chooses the force, the contract never does"
    /// (CONTRACTS.md Q1) — this field is the whole mechanism that ruling
    /// rests on. Ignored by every other verb.
    std::array<entity_id, mercenary_contract_max_units> units{};
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
    // BL-692 removed `rejected_depth_locked` (BL-428): no seam can produce it any
    // more, since chain depth gates neither the build door nor the retool one.
    // The numeric values below shift down by one, which is safe — this enum is
    // never serialised; agent_protocol and verify_api both emit the NAME.
    // --- BL-350: request_quote's four distinguishable decline conditions ---
    rejected_no_capacity,     ///< The named supplier holds no completed building that can produce the good.
    rejected_no_input_access, ///< The supplier's local market cannot supply the recipe's inputs.
    rejected_embargo,         ///< The supplier's condition_set evaluates false against the buyer.
    rejected_reputation,      ///< The (buyer, supplier) reputation pair sits below the standing floor.
    rejected_cooldown,        ///< BL-430: economy.recipe_switch's cooldown has not elapsed on this building.
    rejected_no_lp,           ///< BL-597: no passive Logistic Points at the source anchor for this leg.
};

/// Apply one command through the player-grade seams. Deterministic; a rejected
/// command mutates nothing. `out_building` (optional) receives the entity id a
/// successful `build` or `hire_unit` created.
corp_command_result apply_corp_command(world& w, const recipe_registry& reg,
                                       const corp_command& cmd,
                                       entity_id* out_building = nullptr);

// ---------------------------------------------------------------------------
// BL-628 — the acquisition price (docs/economy/FINANCE.md § Whole-firm acquisition)
// ---------------------------------------------------------------------------
// Exposed rather than kept file-local for two reasons that will both be used:
// the profitability ledger has to SHOW the price beside the row it would buy
// (BL-627), and the rival scorer has to carry it as a candidate's `spend`
// (BL-629). Neither may re-derive the arithmetic; there is one formula.

/// Mean `net` over @p c's last `k_acquisition_trailing_quarters` filed returns,
/// or over however many it has if it is younger. 0 for a firm that has never
/// filed — which the seam refuses to price at all, so this value is never the
/// thing that makes such a firm cheap.
///
/// **WHAT THIS CANNOT SEE, stated because it changes what the price means.** A
/// filed return records the MONEY LOOP only (FINANCE.md § The quarterly return).
/// National transfers and mercenary-contract payouts land after `apply_budget`
/// in the same tick (`app.cpp`: `apply_budget` then `run_nation_step`), so a
/// corporation earning through contracts reads as less profitable on its own
/// returns than it is, and THIS VERB UNDERVALUES IT BY EXACTLY THAT GAP. Closing
/// it is owed work filed as NR-655, deliberately not papered over here: the price
/// is built as FINANCE.md specifies, and `tools/verify/whole_firm_buyout.cpp`
/// row X states the exposure with a measured number rather than choosing a
/// fixture that hides it.
///
/// @param c The target corporation.
/// @return  Mean quarterly net over the trailing window; 0 with no filed returns.
float corp_trailing_net(const corporation_component& c);

/// The whole-firm acquisition price of @p target:
/// `max(0, book_value + multiple * trailing_net + balance)`.
///
/// `book_value` is the LAST FILED return's — historical cost as disclosed, not a
/// live recompute, because the doc's premise is that the price is read off the
/// returns and a buyer must be able to reproduce it from the ledger surface.
/// `balance` is the LIVE component balance, because the buyer receives exactly
/// that cash and the doc's word is "cheaper by exactly what it owes"; at a tick
/// boundary (where every command applies) the two coincide, since `apply_budget`
/// files the closing balance it just wrote.
///
/// A NON-FINITE result is RETURNED AS-IS rather than floored, so a caller can
/// tell "worthless" from "unpriceable": a corrupt filed return must reject the
/// command, and applying the zero floor first would quietly turn a NaN into a
/// free corporation (`NaN > 0` is false). Every caller must test the result's
/// finiteness before spending against it.
///
/// Nothing clamps the profit term: a chronically loss-making firm prices below
/// its book value, and should. The floor is ZERO and deliberately not book value
/// — there is no salvage in the prototype, so book value is not a redemption
/// anyone could take, and the price is a SINK (a public firm's sellers are a
/// diffuse shareholder base, not a modelled actor).
///
/// @param target   The corporation being priced.
/// @param multiple `economy.acquisition.multiple` (recipe_registry::acquisition()).
/// @return         The price in credits, >= 0. 0 for a firm that has never filed.
float corp_acquisition_price(const corporation_component& target, float multiple);

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

/// One past the highest reason — the count sentinel `decision_feed.cpp` uses to
/// size its reason-filter combo (BL-420), mirroring `corp_verb_count` above.
/// Bound to the same append-only rule: appending a reason means moving this
/// with it.
inline constexpr uint8_t corp_decision_reason_count =
    static_cast<uint8_t>(corp_decision_reason::trade_surplus) + 1;

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
