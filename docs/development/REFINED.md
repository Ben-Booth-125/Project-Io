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

### Wave 4 — DONE (2026-08-23)

**BL-573 (contract record and verbs)** landed and merged to `main`
(worktree-agent-a426dc003723cf20f, a clean fast-forward). `complete` in backlog.json, design
prose archived. New harness `tools/verify/mercenary_contract_harness.cpp` (51 checks) proves
accept/evaluate/pay-or-fail works at all — **BL-574 below extends this existing file with the
M1–M7 terminal-state cases, it does not create a new one.** `save_roundtrip` clean
(`world_save_version` 7→8). `contract_template_registry` now threaded from the app-layer
boundary the way `recipe_registry` is (closes NR-581's deferred half); BL-571's
`active_mercenary_contract_for` stub now resolves for real, so the corp-vs-nation garrison
battle trigger is live. `spectator_determinism`'s golden held.

One deliberate deviation from the item's own original brief, correctly caught in-flight: payout
is a direct transfer from the offer's already-fully-funded escrow, not a second `budget_claim`
draw — the treasury was already debited in full while BL-572's escrow accumulated, so a fresh
claim would double-debit the same contract. One flagged number: `NR-582`, `contract_failed`'s
sentiment magnitude (-4.0 Trust, double `contract_cancelled`'s -2.0) is a ratio, not measured —
same discipline as NR-579/580's placeholders, revisit at playtest.

---

### Wave 5 — DONE (2026-08-24)

**BL-574 (contract harness), BL-576 (Contracts ledger), BL-577 (messages + income)** all landed
and merged to `main` (worktree-agent-afa7ac59bf567cd92, worktree-agent-a5991264ff4655709,
worktree-agent-a868b5c9d8eb5bcb9 — commits `e1a1a44a`, `9e9f581d`, `9bc93591`). All three
`complete` in backlog.json, design prose archived. Two genuine mid-batch collisions, both from a
concurrent session sharing this checkout (a review-queue purge and a Sprint 32 branch merge
landing on `main` between waves): an `NR-583` id collision (renamed to `NR-584`) and a
`question_log.json` conflict between BL-576's and BL-577's own additions (resolved as a union,
both kept). `mercenary_contract_harness` reached 66 checks; new `contract_dispatch_harness`
(20 checks); all independently re-verified on the merged tree, `spectator_determinism`'s golden
held.

**Live-click pass (main session, computer-use, 2026-08-24)** against a real generated world
with real live offers (a funded nation, contrary to NR-580's worst case): the Contracts
ledger's rail slot, toggle rule, all three views, the Accept→force-picker→Confirm flow (balance
visibly credited), and the Abandon confirm-with-reputation-cost popup all confirmed reachable
and correct by mouse click. **BL-577's contract card was NOT reachable** — no UI control
anywhere calls a selection trigger for a `mercenary_contract` entity, confirmed directly by
clicking an Active-view contract row (it selects nothing). The card's rendering code exists and
reads correct, but is unwired; a future item needs to add that trigger before it can be
live-verified. Two design calls flagged, not blockers: `NR-585` (`abandoned_event_posted` is
deliberately unserialized) and `NR-586` (the ledger took a new nav-rail slot 13 — the curated
nine plus the developer tail were both full).

---

### Wave 6 (after all nine)

#### BL-578 — MERCENARY_SLICE_PLAYTHROUGH
Requirements: § mercenary-slice-playthrough (R1–R4). Files: `scripts/verify/mercenary_slice.lua`,
`ROADMAP.md`, `sprints.json`, `MANUAL.md`. One task: the six-capture `--verify` script, a live
playthrough, the flag-off determinism check, then the v0.1.15 cut.

**Known gap to route around, not silently paper over:** the design's "a completed contract"
capture likely meant the contract CARD (BL-577), which has no live trigger yet (Wave 5's own
note, above) — the ledger's History view is the reachable substitute for "a completed contract"
on screen. Either capture History instead, or treat wiring a selection trigger for
`mercenary_contract` as this item's own scope addition if the card specifically is wanted.

---

Promote/build order: DELIVERY.md § The Delivery lifecycle. Step 4a (`verifier-review`) runs
once per wave that merges more than one item, not once for the whole batch, since waves 2–5
land on top of code the previous wave actually wrote.
