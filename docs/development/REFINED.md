# Project Io — REFINED (active worklist)

## Sprint 16 — the mercenary vertical slice (v0.1.15), Batch Delivery

Ten items, promoted 2026-08-23. Goal: a polity hires the company, the company fights, the
company is paid — playable end-to-end. Design is settled for all ten (`node
tools/session/backlog_query.js --status designed --sprint 16 --table`); each has an
item-spanning requirement in `req/requirements.json` (`--batch
2026-08-23-sprint-16-mercenary-slice`).

**Dependency waves** (an item's `requires` in backlog.json; a wave's items build in parallel,
waves run in order — this is the collision map's splitting heuristic, not a file-disjointness
gate, since worktree isolation absorbs any overlap):

```
Wave 1  BL-569 province holder        BL-575 unit marker + march UI
           |                                    |
Wave 2  BL-570 condition subject   BL-571 nation garrisons
           |         \                 /   |
Wave 3      \          BL-572 contract offers
             \                   |
Wave 4        BL-573 contract record & verbs
                 /        |          \
Wave 5  BL-574 harness  BL-577 messages+income  BL-576 ledger (+ needs BL-575)
                 \        |          /
Wave 6         BL-578 slice playthrough (needs all nine)
```

Wave 1 has no shared files (battle/province vs. UI canvas) — genuinely disjoint, no worktree
needed for the pair, though each still gets its own worktree per the standing sub-agent model.
Every later wave is a strict sequential dependency on the wave above it; there is no same-wave
symbol sharing beyond wave 1 and the three-way wave 5 fan-out (574/576/577 all read BL-573's
`mercenary_contract` type but do not write each other's files).

---

### Wave 1

#### BL-569 — PROVINCE_HOLDER (province has a holder, a won battle moves it)

Requirements: `req/requirements.json` § province-holder. Files: `src/world/province.hpp`,
`world.hpp`, `hard_coded_world.cpp`, `battle_system.cpp`, `world_save.cpp`,
`tools/verify/battle_engagement_harness.cpp`.

1. `world::province_holder` — seed from `tile_to_nation` plurality at generation.
   **Provides:** `world::province_holder`, `province_holder_for(world, province_id)`.
2. `run_battles` sets `province_holder[p]` on a decisive close; logs the change to the
   dispatch stream.
3. `world_save.cpp` trailing section + version bump; `state_hash` folds the vector.
4. `battle_engagement_harness`: decisive-battle-moves-holder case, stalemate-leaves-it case.

#### BL-575 — UNIT_MARKER_AND_MARCH_UI (unit visible, can be ordered to march)

Requirements: `req/requirements.json` § unit-marker-and-march-ui. Files:
`src/ui/body_surface_canvas.cpp`, `icons.cpp`, `ui_state.hpp`, `src/core/app.cpp`,
`selection_panel.cpp`.

1. `ui::icons::unit_marker` at the province anchor tile, owner-coloured, count badge; a ring
   for contract-committed units (reads a field BL-573 adds later — leave a `bool
   committed = false` stub on the unit-summary struct now so BL-576/577 need no further UI
   plumbing change). **Consumes:** nothing from this batch yet.
2. Unit card gains March/Halt/Disband presses; March enters `ui_state::pending_march_unit`,
   next province click dispatches `corp_verb::march_unit` via the existing seam.
3. `question_log.json` + `docs/ai/ACTIONS.json` entries.
4. Live click pass before flipping the visual requirement complete (standing rule).

---

### Wave 2 (after BL-569)

#### BL-570 — CONDITION_PROVINCE_SUBJECT

Requirements: § condition-province-subject. Files: `condition_set.{hpp,cpp}`,
`scripts/contracts.lua`, `tools/verify/condition_set_harness.cpp`.

1. `condition_subject::province_held` + `condition::province` field.
   **Consumes:** `world::province_holder` (BL-569). **Provides:**
   `condition_subject::province_held`, the `contracts.lua` template table shape
   `{id, name, predicate, continuous, fee_mult, deadline_ticks}`.
2. `condition_text` rendering; `scripts/contracts.lua` two rows (take, hold).
3. `condition_set_harness` cases: holder / non-holder / sea; template load + round-trip.

#### BL-571 — NATION_GARRISONS

Requirements: § nation-garrisons. Files: `components.hpp`, `corporation_generation.cpp`,
`nation_generation.cpp`, `battle_system.cpp`, `stance.hpp`, `unit_upkeep.cpp`.

1. `unit_component::owner` accepts a nation entity; `nation_generation.cpp` seeds garrisons
   (capital + grudge-border provinces, treasury-scaled). **Consumes:**
   `world::province_holder` (BL-569, for border-province identification). **Provides:**
   nation-owned units in `world::units`, garrison-strength-per-province query.
2. `run_battles` second trigger (contract-is-hostility, stance keyed on contract id) — the
   contract id it keys on does not exist until BL-573; stub the trigger against a
   placeholder `active_mercenary_contract_for(corp, province)` returning none until then, so
   this task is not blocked waiting on wave 4.
3. Garrison upkeep as a `military_research` budget claim.
4. `battle_engagement_harness` corp-vs-nation case.

---

### Wave 3 (after BL-570, BL-571)

#### BL-572 — CONTRACT_OFFERS

Requirements: § contract-offers. Files: `nation_ai.{hpp,cpp}`, `nation_step.cpp`,
`nation_budget.{hpp,cpp}`, `world.hpp`, `world_save.cpp`,
`tools/verify/nation_scorer_harness.cpp`.

1. `mercenary_offer{id, client, target_province, template, fee, deadline, issued_tick}`;
   `world::mercenary_offers` (vector — concurrent offers). **Consumes:** garrison-strength
   query (BL-571), `contracts.lua` template shape (BL-570). **Provides:** `mercenary_offer`,
   `world::mercenary_offers`, `offer_escrow` per-offer accumulator.
2. `derive_contract_offers`, called from `run_nation_step`; targeting rule; escrow
   accumulation; oldest-issued-first tick-share split; TTL expiry.
3. `line_takes_subject(contracted_force) = true`.
4. Save section + version bump; `nation_scorer_harness` R5.

---

### Wave 4 (after BL-572)

#### BL-573 — CONTRACT_RECORD_AND_VERBS

Requirements: § contract-record-and-verbs. Files: `components.hpp`, `corp_command.{hpp,cpp}`,
`economy_system.cpp`, `sentiment.hpp`, `world.hpp`, `world_save.cpp`, `docs/ai/ACTIONS.json`.

1. `mercenary_contract{id, client, contractor, template, province, fee, deposit_paid,
   deadline, accepted_tick, units[8], state}`; `world::mercenary_contracts`. **Consumes:**
   `world::mercenary_offers` (BL-572), the condition-template shape (BL-570). **Provides:**
   `mercenary_contract`, `corp_verb::accept_offer`, `corp_verb::abandon_contract`,
   `sentiment_factor_kind::contract_failed`. This is what BL-571's placeholder
   `active_mercenary_contract_for` should resolve against — wire it for real here.
2. `accept_offer` / `abandon_contract` on the corp-command seam, untrusted-boundary rule.
3. Tick evaluation after `run_battles`: completed/failed/abandoned, deposit/remainder split.
4. Committed-unit lock (no disband/double-commit); save round-trip; ACTIONS.json entries.

---

### Wave 5 (after BL-573; BL-576 also needs BL-575)

#### BL-574 — CONTRACT_HARNESS
Requirements: § contract-harness. Files: `tools/verify/mercenary_contract_harness.cpp`.
**Consumes:** `mercenary_contract` + verbs (BL-573). One task: M1–M7 cases per the item design.

#### BL-576 — CONTRACTS_LEDGER
Requirements: § contracts-ledger. Files: `src/ui/contracts_ledger.{hpp,cpp}`, `nav_pane.cpp`,
`ui_state.hpp`, `src/core/app.cpp`. **Consumes:** `mercenary_contract`/offers (BL-573,
BL-572), the unit-summary `committed` field stub (BL-575). Tasks: rail slot + toggle rule;
Offers/Active/History views; Accept force-picker flow; Abandon press; question_log +
ACTIONS.json; live click pass.

#### BL-577 — CONTRACT_MESSAGES_AND_INCOME
Requirements: § contract-messages-and-income. Files: `session_history.cpp`,
`battle_dispatch_text.cpp`, `selection_panel.cpp`, `balance_ledger.cpp`, `header_panel.cpp`.
**Consumes:** `mercenary_contract` events (BL-573). Tasks: five dispatch-text phrasings +
Public-channel post; contract card; Balance "Contract income" line from `subsidies`.

---

### Wave 6 (after all nine)

#### BL-578 — MERCENARY_SLICE_PLAYTHROUGH
Requirements: § mercenary-slice-playthrough (R1–R4). Files: `scripts/verify/mercenary_slice.lua`,
`ROADMAP.md`, `sprints.json`, `MANUAL.md`. One task: the six-capture `--verify` script, a live
playthrough, the flag-off determinism check, then the v0.1.15 cut.

---

Promote/build order: DELIVERY.md § The Delivery lifecycle. Step 4a (`verifier-review`) runs
once per wave that merges more than one item, not once for the whole batch, since waves 2–5
land on top of code the previous wave actually wrote.

---

## Sprint 17 — the ancient roster becomes a ladder (v0.1.17), one item ahead of the batch

Ben's sequencing ruling holds Sprint 17 to start only after Sprint 16 merges to `main`. One item
is promoted ahead of that gate because it touches no file Sprint 16 owns (checked against Sprint
16's full file set, `backlog_query.js --sprint 16`): `scripts/recipes.lua`,
`tools/verify/chain_depth.cpp` and `docs/economy/PRODUCTION.md` appear nowhere in it. The other
nine items wait for the merge — several depend on this one's chains, and the rest touch
`world_save.cpp`, `selection_panel.cpp` or `icons.cpp`, all live under Sprint 16.

#### BL-587 — INTERCHANGEABLE_METHODS_EXIST — COMPLETE 2026-08-23

Requirements: `req/requirements.json` § interchangeable-methods-exist. Files: `scripts/recipes.lua`,
`tools/verify/chain_depth.cpp`, `docs/economy/PRODUCTION.md`.

1. Two genuine interchangeable methods authored on the **existing** resource roster (no new
   `resource_type` values — that is BL-585's item, gated on Sprint 16's save-format bump):
   `charcoal_from_kiln` "Coking Kiln" (ancient, shares `timber` with the Charcoal Burner, reagent
   `iron_blooms` at required depth 2) and `steel_bessemer` "Bessemer Converter" (industrial, shares
   `iron_ore`+`coal` with the Smelter, reagent `machinery` at required depth 2). Both landed as
   recipe ids 29/30 — appended, never inserted.
2. `chain_depth.cpp`'s R2 row re-run against the shipped roster: **13 sibling pairs, 10 supply
   routes, 1 precondition, 2 interchangeable methods, 0 dominated** — the bucket that was empty
   before this item now holds real content and the guard still passes.
3. `recipe_switch_harness` and `price_band_harness` re-run clean — both link the live
   `recipes.lua`/`economy.lua` and neither is Sprint-16-owned, so both are cheap confirmation that
   the shared file was not broken for anyone else building against it.
4. `PRODUCTION.md` § Alternate production methods corrected: the four pairs once reported
   "dominated" (NR-243) are supply routes/a precondition, not methods — closed rather than
   retuned (NR-577) — and the new pairs are recorded in the Metal Foundry / Fuel Production group
   tables and the ancient-chain table.
