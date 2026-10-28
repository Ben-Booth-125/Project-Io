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
    set_workforce, ///< building_component.workforce_target := value (0–200 internal); clears workforce_auto. RIVALS ONLY: refused for the player corp (BL-1322).
    idle,          ///< building_component.decommissioned := true.
    resume,        ///< building_component.decommissioned := false.
    place_road,    ///< place_road(tile, side, tier) — a road is an edge (BL-1340).
    survey,        ///< dispatch_survey(body) on behalf of the corp.
    hire_unit,     ///< Raise a unit at a tile from the campaign roster (BL-324).
    // --- BL-293: the order book joins the seam (2026-08-07) ---
    // Appended AFTER hire_unit: the enum is serialised and append-only, and
    // hire_unit already holds its value in main's dictionary and harnesses.
    place_sell_order,   ///< RETIRED (BL-1265), REJECTED. Pushed a standing sell order; the order book retired with corporation pools. The slot is kept by the append-only rule.
    remove_sell_order,  ///< RETIRED (BL-1265), REJECTED. Erased a standing sell order. The slot is kept by the append-only rule.
    set_workforce_auto, ///< building_component.workforce_auto := true. Refused for the player corp (BL-1322: the game sets the player's workforce).
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
    dispatch_convoy,  ///< RETIRED (BL-1266), REJECTED. Put a corporation's pooled goods on a lane; goods now move only by a trade (`set_trade`). The slot is kept by the append-only rule.
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
    // 2026-10-27 (Ben: "all marches go to a tile") RETURNED IT TO `tile`: the march
    // ends on its tile, and `province` is ignored. Same value, same position.
    march_unit,    ///< Set/replace `subject`'s (a unit) movement order to tile `tile`, path-marched across ticks and ending on it; the path avoids every tile another nation's units stand on, save `tile` itself. rejected_not_owner / rejected_invalid (unknown unit or tile, a tile the type may not enter, off the unit's body, unreachable) / rejected_state (already on `tile`, in a battle, refitting, or spent — readiness 0 — and `tile` held by a nation the actor is hostile to) — see `march_check`. `province` is ignored.
    halt_unit,     ///< Clear `subject`'s movement order. rejected_state if it has none.
    disband_unit,  ///< Erase `subject` (a unit) outright. No refund — manpower walks away.
    // --- BL-467: the battle seam (2026-08-21) ---
    // Appended AFTER disband_unit, same append-only rule. The engagement TRIGGER
    // is deliberately not a verb — a battle opens because a march steps onto a tile
    // a nation it is hostile to holds (2026-10-27), not because someone asked to
    // fight. Breaking off is the only
    // decision a commander actually has once contact is made, so it is the only
    // thing the seam exposes.
    withdraw_from_battle, ///< `corp` breaks off the battle it is fighting over the defended tile `tile` against `counterparty` (null = the first in sorted order). Honoured at the tick boundary, before the next round batch; priced by the resolver's three-term withdrawal cost.
    // --- BL-573: the mercenary-contract seam (2026-08-23) ---
    // Appended AFTER withdraw_from_battle, same append-only rule.
    // CONTRACTS.md § The mercenary contract: "a contract is a condition_set
    // the client will pay to have become true, by a deadline". These two
    // verbs were the whole player/agent surface onto it — accepting an OFFER
    // (BL-572) with a named force, and walking away from a live one early.
    //
    // ── TOMBSTONES: THE MERCENARY CONTRACT IS RETIRED (BL-693) ────────────────
    // Ben, 2026-08-29: "I don't think mercenary contracts is the correct
    // system." The system is gone — its records, its passes, its serialisation,
    // the Contracts ledger and its rail slot — and no scorer candidate issues
    // either verb.
    //
    // THE TWO SLOTS SURVIVE THE SYSTEM, ONLY because the enum is APPEND-ONLY:
    // deleting a value renumbers every verb below it and silently re-points the
    // ACTIONS.json dictionary and every recorded command. That is the whole
    // reason they are still here, and it binds every verb appended after them.
    //
    // THEY REMAIN CALLABLE, AND THEY ARE REJECTED. `apply_corp_command` answers
    // `rejected_invalid` for both and mutates nothing — deliberately, rather
    // than falling through to a default, so a caller is told No instead of being
    // quietly ignored (the AI-facing-seam rule, io-standing-rules.md). They are
    // a dormant record of a retired system, not a live mechanism: the
    // `mercenary_offer` and `mercenary_contract` records they once named exist
    // nowhere in the world. See corp_command.cpp's matching tombstone note.
    //
    // The `request_quote` / `accept_quote` / `cancel_contract` trio — PROCUREMENT,
    // the BUY side — is a different system and is fully live. Do not confuse them.
    accept_offer,      ///< RETIRED (BL-693), REJECTED. Converted the live mercenary offer named by `order` into a contract, committing `units`. The slot is kept by the append-only rule; nothing accepts the verb.
    abandon_contract,  ///< RETIRED (BL-693), REJECTED. Walked away early from the live mercenary contract named by `order`. The slot is kept by the append-only rule; nothing accepts the verb.
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
    // --- BL-1076: the seat is a game act (2026-09-24) ---
    // Appended AFTER buy_corporation, same append-only rule. The player PICKS
    // the corporation they are on the selection canvas at Begin (STARTUP.md §
    // The seat), and Ben ruled the pick a game act: an agent must be able to
    // make it, so it is a verb with a dictionary entry although the wizard is
    // not. THE SEAT IS TAKEN ONCE, BEFORE PLAY: the phase is the HOST's gate,
    // the way actor authority is — the in-play hosts (agent_protocol, so both
    // --serve and the live agent seam) refuse the verb with rejected_state,
    // and only the pre-play hosts (the canvas's Confirm, `--seat`, the verify
    // API) route it here. The seam itself validates the WORLD half: `corp`
    // exists and is a specialist. DELIBERATELY NOT in corp_ai.cpp's candidate
    // list — no rival picks anyone's seat.
    take_seat,         ///< BL-1407: `corp` (== `as_nation`) is a MAJOR NATION and becomes the player: `world::player_entity` re-points onto it. A minor is refused. Pre-play only — the in-play hosts refuse it.
    // --- BL-1266: trade joins the seam (2026-10-10) ---
    // Appended AFTER take_seat, same append-only rule. TRADE.md § A trade and
    // § Auto and reserved trade. The player's, and a rival's under the
    // AI_OPPONENT.md § 11 grant (a rival may set its own trades).
    set_trade,         ///< Set a MANUAL trade: move good `target` from market `subject` to market `counterparty` with `quantity` trade points per tick. `order` 0 adds a trade; a nonzero `order` changes the caller's own trade with that id in place.
    clear_trade,       ///< Erase the caller's own manual trade with id `order`.
    set_trade_reserve, ///< `corp`'s trade points reserved for manual trades := `quantity` (finite, >= 0; clamped to what it makes at use). The rest is auto.
    // --- BL-1276: the research queue joins the seam (2026-10-10) ---
    // Appended AFTER set_trade_reserve (BL-1266), same append-only rule. The NATION holds
    // technology (RESEARCH.md § Who holds technology); the player reaches its
    // nation's queue as the seat — the nation's operating champion — so these
    // three act on the home nation of `corp`, and only when `corp` IS the seat
    // (`world::player_entity`): a rival corporation does not plan its nation's
    // research (that is the nation scorer's, AI_OPPONENT.md § 11), and a wire
    // agent acting as one must not either. `order` names the node: an index
    // into `io::dg::nodes`, range-checked before any use. DELIBERATELY NOT in
    // corp_ai.cpp's candidate list.
    queue_research,    ///< The seat's nation queues node `order` with its unheld requirements ahead of it (`enqueue_node`). rejected_state when already held/queued or a pick-one pair refuses it; rejected_invalid for an unknown node.
    unqueue_research,  ///< The seat's nation takes queued node `order` off its research queue with every later entry that needs it (the toggle, NR-1033); `order == unqueue_last` drops the LAST entry instead. rejected_state when the node is not queued / the queue is empty; rejected_invalid for any other out-of-tree `order`.
    clear_research,    ///< The seat's nation empties its research queue (the pool stays). rejected_state when the queue is empty.
    // --- BL-1255: railways join the seam (2026-10-14) ---
    // Appended AFTER clear_research, same append-only rule. LOGISTICS.md § 4c:
    // the player's NATION lays track and builds stations, paid from the treasury.
    // Both act for `as_nation` (the state-industry door) and only when `corp` IS
    // the seat (`world::player_entity`) and the nation's champion: no grant lets a
    // rival build rail, so corp_ai.cpp issues neither. The station rides `build`
    // with `type = railway_station`; the track is this verb.
    lay_track,         ///< Lay one tile of rail track at `tile` for nation `as_nation`: land, the nation's own ground, untracked, extending its own line or opening one at a city; costs `rail.track_build_cost` + `rail.track_steel_per_tile` steel (priced at the tile's market) from the treasury, upkeep per tile per tick after. rejected_placement / rejected_funds / rejected_era_locked on the door's refusals.
    // --- BL-1309: the nation's Logistic Point shares join the seam (2026-10-14) ---
    // Appended AFTER lay_track, same append-only rule. LOGISTICS.md § Logistic
    // Points, "One pool": the nation sets how its points are shared among its
    // state, its champion and other firms; with no rule the share is mechanical.
    // Acts for `as_nation` and only when `corp` IS the seat and the nation's
    // champion: no grant lets an AI nation set shares, so corp_ai.cpp never issues it.
    set_logistics_shares, ///< RETIRED (BL-1405): a nation's pool serves its own buildings as one class, so there is nothing to share. Refused whole, rejected_invalid, from every actor; mutates nothing. The slot stays (append-only enum).
    // --- BL-1335: the nation's tax tier joins the seam (2026-10-16) ---
    // Appended AFTER set_logistics_shares, same append-only rule. NATIONS.md
    // § Taxes and Discontent: the player sets its nation's tax tier on the
    // Budget ledger's Taxes control. Acts for `as_nation` and only when `corp`
    // IS the seat and the nation's champion; an AI nation's tier is set by its
    // own scorer (run_nation_tax_ai, the 2026-08-18 grant), never by corp_ai.
    set_tax_tier,         ///< Nation `as_nation`'s tax tier := `order` (1..5, I..V). rejected_invalid on any other `order`, or with no `as_nation`; rejected_not_owner unless `corp` is the seat and the nation's champion.
    // --- BL-1437: obsolete unit types join the seam (2026-10-27) ---
    // Appended AFTER set_tax_tier, same append-only rule. MILITARY.md § Land — the
    // classes: a nation marks a roster row obsolete and Recruit hides it. The seat's
    // alone: an AI nation's scorer reads the flags (and their defaults) and never
    // sets them, so corp_ai.cpp issues no such verb.
    set_unit_obsolete,    ///< The seat's nation marks roster row `unit_type` obsolete (`order` = 1) or live (`order` = 0). rejected_invalid for an unknown row or any other `order`; rejected_state when the row already reads that way; rejected_not_owner unless `corp` is the seat.
    // --- Refit to a newer type (Ben, 2026-10-27) ---
    // Appended AFTER set_unit_obsolete, same append-only rule. MILITARY.md § The
    // muster interface, "Refit to a newer type". No grant lets a rival refit, so
    // corp_ai.cpp issues no such verb; any nation may refit its OWN units.
    refit_unit,           ///< Refit unit `subject` to roster row `unit_type`: a newer model of the same land class the nation fields now (`refit_targets`). Paid now (the price difference, floored at 0, plus the target's gated goods); takes the target's muster days. BL-1434: `unit_type` may also be the unit's OWN row when its recorded design differs from its class's live design (a design refit); every refit brings the unit to the live design and buys the kit of each slot that changes. rejected_not_owner / rejected_invalid / rejected_state (in a battle, already refitting) / rejected_placement (off its own ground) / rejected_funds — see `refit_check`.
    // --- BL-1427: fronts (Ben, 2026-10-27) ---
    // Appended AFTER refit_unit, same append-only rule. MILITARY.md § Fronts. Any
    // nation on its OWN units and fronts; rivals issue both by the grant in
    // AI_OPPONENT.md § 11 ("Rival nations use fronts").
    assign_front,         ///< Assign unit `subject` to the actor's front whose span holds `tile` (the front KEYED `tile` first: `front_named`), or unassign it when `tile` is null_entity. The unit stores the front's key. rejected_invalid (no such unit, or no such front of the actor's) / rejected_not_owner (not the actor's unit) / rejected_state (already so — the same front, or already unassigned).
    set_front_order,      ///< Set the actor's front whose span holds `tile` (`front_named`) to `order` (0 hold, 1 push). rejected_invalid (no such front, or another order) / rejected_state (push toward a neighbour the actor is not hostile to, or no change).
    // --- BL-1434: unit designs (Ben, 2026-10-27/28) ---
    // Appended AFTER set_front_order, same append-only rule. MILITARY.md § The kit:
    // one live design per land class per nation; a slot offers only researched kit.
    // The SEAT's alone: no grant lets a rival design (AI_OPPONENT.md § 11), so rivals
    // keep base kit and corp_ai.cpp never issues it.
    set_unit_design,      ///< The seat's nation fits kit item `unit_type` (a kit catalogue id, unit_designs.hpp) into slot `recipe` (0-based) of its live design for land class `order` (a `land_class` value). rejected_not_owner unless `corp` is the seat; rejected_invalid for an unknown class (or `none`), slot or item, or an item that does not fit that (class, slot); rejected_tech_locked when the item is not researched; rejected_state when the slot already holds it. See `unit_design_check`.
};

/// One past the highest verb — the wire parser's range gate (BL-396: run_serve
/// refuses any `verb=` outside [0, corp_verb_count) rather than letting a
/// narrowing cast truncate it into a verb the caller never named). Bound to
/// the append-only rule above: this is always `<last enumerator> + 1`, so
/// appending a verb means moving this with it — and only this, since existing
/// values never renumber.
inline constexpr uint8_t corp_verb_count =
    static_cast<uint8_t>(corp_verb::set_unit_design) + 1;

/// `unqueue_research`'s "the last entry" sentinel for `corp_command::order`
/// (the MCP boundary's -1). UINT32_MAX, never a node index.
inline constexpr uint32_t unqueue_last = 0xFFFFFFFFu;

/// BL-1266: ceiling on one corporation's manual trades. Reachable by command,
/// so by a scorer with a bug in it — the bound that keeps a runaway from
/// growing the save without limit. `set_trade` answers `rejected_state` at it.
inline constexpr std::size_t max_trades_per_corp = 64;

/// BL-1266: the largest trade points a trade or a reserve may name — far above
/// anything a corporation makes, and finite, so a wire value cannot carry an
/// infinity or a float the arithmetic cannot hold into the trade pass.
inline constexpr float max_trade_points = 1.0e6f;

/// Fixed capacity of this command's `units` field below. It once mirrored a
/// `mercenary_contract`'s committed force exactly; that record retired with the
/// system (BL-693) and exists nowhere, so this constant now sizes ONE array and
/// nothing else. It survives for the reason the two verb slots do — the record's
/// byte shape is what a stored command and a wire caller agree on, and shrinking
/// it would re-interpret both. A wire-safe fixed-size array is also what the
/// untrusted-input rule can range-check field by field, the same reason
/// `corp_command` itself carries no `std::vector`.
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

    entity_id     tile      = null_entity;              ///< build / place_road / hire_unit target tile; march_unit's destination; withdraw_from_battle's defended tile; the front assign_front / set_front_order name.
    building_type type      = building_type::none;      ///< build only.
    resource_type target    = resource_type::iron_ore;  ///< build (extraction) AND place_sell_order: what to sell.
    uint16_t      recipe    = no_recipe;                ///< set_recipe (and build seed). set_unit_design: the SLOT index (BL-1434) — the default, no_recipe, names no slot and is refused.
    int           workforce = 100;                      ///< set_workforce value [0, 100] (BL-1322).
    uint8_t       road_tier = 1;                        ///< place_road tier (1–3).
    /// place_road: the hex SIDE of `tile` the road crosses (BL-1340 — a road is an edge
    /// between two tiles). 0=E, 1=NE, 2=NW, 3=W, 4=SW, 5=SE; `k_road_side_auto` (0xFF,
    /// the default) means "no side named", and `place_road` then picks the cheapest land
    /// neighbour deterministically. 0-5 or 0xFF; any other value is rejected at the
    /// untrusted seam (agent_protocol.cpp), whole, mutating nothing.
    uint8_t       road_side = k_road_side_auto;
    uint16_t      unit_type = 0;                        ///< hire_unit: index into unit_roster_table(). set_unit_design: the kit catalogue id (BL-1434).

    // --- BL-293 order-book args ---
    float    quantity    = 0.0f; ///< place_sell_order: max units offered per tick (> 0). Also request_quote.
    float    floor_price = 0.0f; ///< place_sell_order: minimum unit price (>= 0; 0 = market price).
    uint32_t order       = 0;    ///< remove_sell_order: the sell_order::id to erase. Also accept_quote /
                                  ///< cancel_contract: the procurement_quote / procurement_contract id.
                                  ///< The retired pair (accept_offer / abandon_contract, BL-693)
                                  ///< named their mercenary offer and contract here too — the
                                  ///< same "an id names the thing this verb acts on" reading
                                  ///< every reuse carries, never two ids in flight on one
                                  ///< command. Both verbs are rejected, so nothing reads it now.

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

    // --- BL-511: unit movement moved to province grain (2026-08-21); RETIRED for
    // the march and the withdrawal (2026-10-27: both name a tile). NO VERB READS
    // IT NOW; it is kept so the wire's `province=` field and the play log stay
    // parseable. The history below is why its default is `no_province`. ---
    /// Once `march_unit`'s DESTINATION PROVINCE (a `province::id`, world.hpp's
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

    // --- BL-573: the mercenary-contract seam (2026-08-23), RETIRED (BL-693) --
    /// accept_offer's committed force — the corp's OWN units, named by the
    /// caller, unused slots `null_entity`. That verb is rejected now, so NO VERB
    /// READS THIS FIELD. It is kept, and inert, because the record's byte shape
    /// is what a stored command and a wire caller agree on; dropping it would
    /// re-interpret every command already recorded.
    std::array<entity_id, mercenary_contract_max_units> units{};

    // --- BL-1296: state industry joins the seam (2026-10-13) ---
    /// The STATE INDUSTRY this command acts for (NATIONS.md § State industry,
    /// "The player's controls"; AI_OPPONENT.md § 11, the 2026-10-11 grant).
    /// `null_entity` (the default) is every command there has ever been: `corp`
    /// acts on its own book. Non-null, the command acts on that NATION's state
    /// buildings, paid from its treasury, and `corp` must be the nation's
    /// CHAMPION (`champion_of`) — a nation's state industry is operated from its
    /// champion's seat, the player's from the player's, a rival major's from its
    /// champion's scored layer. Only a corporation's building verbs are legal
    /// (build, demolish, set_recipe, set_workforce, set_workforce_auto, idle,
    /// resume); any other verb is `rejected_invalid`, and a building the nation
    /// does not own is `rejected_not_owner`. Appended LAST so every older field
    /// keeps its place in the record.
    /// BL-1399 / BL-1402: `corp == as_nation` is the NATION ACTING AS ITSELF (every
    /// minor must; no champion between). It takes the building verbs above, and
    /// also — through the same owner-generic bodies a corporation's command runs,
    /// paid from the treasury's uncommitted part, never into debt — hire_unit,
    /// march_unit, halt_unit, place_road, the trade verbs (refused until trade.cpp
    /// admits a nation owner) and, for the seat's nation only, the research
    /// verbs. Procurement is refused to a nation (rejected_invalid).
    entity_id as_nation = null_entity;
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
    rejected_wrong_air,       ///< The recipe cannot run on this body's air: propellant_electrolysis only on an airless body, propellant_atmospheric only under air (Ben, 2026-10-09).
};

/// Refit to a newer type (Ben, 2026-10-27): the credits a refit of a unit of row
/// @p from to row @p to costs — the hire price difference, `hire_cost_per_power x
/// (to.power_mod - from.power_mod)`, floored at 0 (the base price cancels). The
/// target's gated goods are charged on top, as a hire charges them. Unknown rows: 0.
float refit_credit_cost(const recipe_registry& reg, uint16_t from, uint16_t to);

/// Every check `refit_unit` makes, READ-ONLY, in its order — so the unit card names
/// the same refusal the seam would give: the unit exists and is @p nation's
/// (rejected_invalid / rejected_not_owner); @p target is a compatible row the nation
/// fields now, or its own row when its design is stale (`refit_targets` of the unit,
/// BL-1434) (rejected_invalid); the unit is not in a battle and not already
/// refitting (rejected_state); it stands on @p nation's own ground
/// (rejected_placement); the nation can pay the credits and the target's goods and the
/// changed slots' kit goods
/// (rejected_funds). `applied` means the verb would apply.
corp_command_result refit_check(const world& w, const recipe_registry& reg, entity_id nation,
                                entity_id unit, uint16_t target);

/// BL-1434 (unit designs): every check `set_unit_design` makes, READ-ONLY, in its
/// order — so the designer names the refusal the seam would give, and can grey a
/// locked item. @p nation is the seat (rejected_not_owner otherwise); @p cls a land
/// class with slots (rejected_invalid); @p slot one of its slots (rejected_invalid);
/// @p item a catalogue id that fits (@p cls, @p slot) (rejected_invalid); researched
/// by @p nation (rejected_tech_locked); not already fitted (rejected_state).
/// `applied` means the verb would apply.
corp_command_result unit_design_check(const world& w, entity_id nation, uint32_t cls,
                                      uint16_t slot, uint16_t item);

/// Every check `march_unit` makes, READ-ONLY save for the logistics path cache the
/// route search warms, in its order — so a caller can ask before it acts (the
/// right-click "declare and attack" runs it for every unit before it declares): the
/// unit exists and is @p actor's (rejected_invalid / rejected_not_owner); it is in no
/// battle and not refitting (rejected_state); @p tile is a tile the unit's type may
/// enter, on its body (rejected_invalid); the unit is not already there, and is not a
/// SPENT unit (readiness 0) marching onto a tile a nation @p actor is hostile to holds
/// — an attack `open_tile_battle` would refuse (rejected_state); a path reaches it
/// (rejected_invalid). @p assume_hostile, when named, is read as hostile already (a
/// declare the caller is about to make). `applied` means the verb would apply; the
/// path it would order is written to @p out_path when given.
corp_command_result march_check(world& w, entity_id actor, entity_id unit, entity_id tile,
                                entity_id assume_hostile = null_entity,
                                std::vector<entity_id>* out_path = nullptr);

/// Apply one command through the player-grade seams. Deterministic; a rejected
/// command mutates nothing. `out_building` (optional) receives the entity id a
/// successful `build` or `hire_unit` created.
corp_command_result apply_corp_command(world& w, const recipe_registry& reg,
                                       const corp_command& cmd,
                                       entity_id* out_building = nullptr);

// ---------------------------------------------------------------------------
// BL-1407 — the acquisition price (BL-628) and firm exit (BL-743) are DELETED
// with the corporation (FINANCE.md § Whole-firm acquisition, § Firm exit:
// retired). The two records below survive only because the registry still
// parses `economy.firm_exit` and the economy report still carries the (always
// empty) list; nothing runs an exit.
// ---------------------------------------------------------------------------

struct firm_exit_params
{
    float balance_floor        = 0.0f; ///< RETIRED (BL-1407): read by nothing.
    int   consecutive_quarters = 0;    ///< RETIRED (BL-1407): read by nothing.
};

/// RETIRED (BL-1407): no exit runs, so no record is ever made.
struct firm_exit_record
{
    entity_id corp     = null_entity;
    float     balance  = 0.0f;
    int       holdings = 0;
    int       units    = 0;
};

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
    // --- BL-1267 (a rival sets its own trades, AI_OPPONENT.md § 11), appended ---
    trade_pin,        ///< A route's margin per point clears the floor; pin a manual trade on it.
    trade_unpin,      ///< A manual trade's route now loses (or cannot run); clear it.
    trade_reserve,    ///< Keep the reserve equal to the manual trades' points.
};

/// One past the highest reason — the count sentinel `decision_feed.cpp` uses to
/// size its reason-filter combo (BL-420), mirroring `corp_verb_count` above.
/// Bound to the same append-only rule: appending a reason means moving this
/// with it.
inline constexpr uint8_t corp_decision_reason_count =
    static_cast<uint8_t>(corp_decision_reason::trade_reserve) + 1;

/// One ring-buffer entry: the command plus its score rationale.
struct corp_decision
{
    int                  tick          = 0;
    entity_id            corp          = null_entity;
    corp_command         command;
    float                winning_score = 0.0f;
    /// Score of the best candidate the corp did NOT take in this evaluation
    /// **from this command's own budget family** (build / dial / survey / hire /
    /// trade / dispatch) — the highest-scoring one rejected by an action budget,
    /// the one-touch rule or the solvency gate. 0 when nothing in that family
    /// was passed up.
    ///
    /// A candidate the SEAM refused does not count (BL-696): `apply_corp_command`
    /// mutates nothing on refusal, so that was never an option the corp had, and
    /// counting it reports a counterfactual that did not exist.
    ///
    /// Was "the next candidate in sort order" until NR-232 (2026-08-14), which
    /// was misleading: one evaluation applies up to seven commands, so the next
    /// candidate was frequently a command that ALSO ran, and a reader comparing
    /// the two scores saw a contest that never happened.
    ///
    /// The FAMILY qualifier is BL-696's, and it is what makes the pair readable
    /// rather than merely honest. A family is scored by one formula in one unit;
    /// across families the scores are not commensurable at all (a foregone sell
    /// order scores hundreds of credits, a good workforce dial scores 3), so a
    /// single evaluation-wide maximum made `runner_up >= winning_score` the
    /// ordinary case and every feed row read "overridden" against a bar pinned
    /// to zero. Comparing a decision only against what it actually displaced
    /// restores the counterfactual the field's name claims.
    ///
    /// Still may EXCEED `winning_score` (NR-226): candidates sort by priority
    /// bucket before score, so a Must-Have idle can displace a higher-scoring
    /// Should-Have dial in the same family. Aggregators must not treat the pair
    /// as a contest the winner was supposed to win.
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
