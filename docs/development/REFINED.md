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

### Wave 1 — DONE (2026-08-23)

**BL-569 (province holder)** and **BL-575 (unit marker + march UI)** landed and merged to
`main` (worktree-agent-a85dd40a41493c103, worktree-agent-a0220dee51ed2793d); both `complete`
in backlog.json, design prose archived. Full-suite re-verification after the merge fixed one
save-version tripwire and re-blessed one golden — see commit `e9c2c5ac`. BL-575's live-click
pass confirmed March is reachable and dispatches; it did not confirm the unit visibly moving
(NR-577, not a blocker — see that entry).

---

### Wave 2 — DONE (2026-08-23)

**BL-570 (condition province subject)** and **BL-571 (nation garrisons)** landed and merged to
`main` (worktree-agent-aae464e367f3fcb1d, worktree-agent-a477567f226eaa377); both `complete` in
backlog.json, design prose archived. Both branches had merged an older `main` into themselves
before this session's origin/main reconciliation landed, so integrating them meant resolving
stale-base doc conflicts by hand each time (see commits `0a8f6de2`, `b409fc6f`) — and a real
collision: both items independently bumped `world_save_version` 4→5 for different new fields;
combined into one coherent bump to 6. `spectator_determinism`'s golden re-blessed again
(garrisons are new units, folded into `state_hash` unconditionally).

Two things flagged for Ben, not blockers: `NR-579` (BL-570's `fee_mult`/`deadline_ticks` are
legible placeholders) and `NR-580` (every nation's treasury is 0 at generation, so BL-571's
garrisons all land on the sizing floor — the same gap BL-572, next, is about to hit from the
funding side).

---

### Wave 3 — DONE (2026-08-23)

**BL-572 (contract offers)** landed and merged to `main` (worktree-agent-a1a24b0f421a2fbf6, a
clean fast-forward — no sibling wave-3 agent, no doc-conflict archaeology this time). `complete`
in backlog.json, design prose archived. `nation_scorer_harness` gained R8 (34 checks, all
pass); `save_roundtrip` clean (`world_save_version` 6→7). `spectator_determinism`'s golden did
NOT move this time (a default world's `mercenary_offers` stays empty while treasury is 0, so no
new state entered the hash).

One thing carried forward into Wave 4, not a blocker: `NR-581` — offer fee/deadline are a
placeholder `contract_offer_params`, not a live `contract_template_registry` lookup (that
registry needs sol2/Lua, unreachable from `world/*`'s Lua-free superset `derive_contract_offers`
lives in). BL-573 below is where a real lookup belongs, since accepting an offer needs the
template's actual predicate anyway — thread `contract_template_registry` through the same way
`recipe_registry` already reaches `world/*` (a plain parameter, loaded once at the app-layer
boundary, never loaded by `world/*` itself).

---

### Wave 4 (after BL-572)

#### BL-573 — CONTRACT_RECORD_AND_VERBS

Requirements: § contract-record-and-verbs. Files: `components.hpp`, `corp_command.{hpp,cpp}`,
`economy_system.cpp`, `sentiment.hpp`, `world.hpp`, `world_save.cpp`, `docs/ai/ACTIONS.json`.

1. `mercenary_contract{id, client, contractor, template, province, fee, deposit_paid,
   deadline, accepted_tick, units[8], state}`; `world::mercenary_contracts`. **Consumes:**
   `world::mercenary_offers` (BL-572), the condition-template shape (BL-570) — this is also
   where `contract_template_registry` needs to become reachable for real (NR-581): thread it
   through the same way `recipe_registry` already is, a plain parameter from the app-layer
   boundary, not a `world/*`-side Lua load. **Provides:**
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
