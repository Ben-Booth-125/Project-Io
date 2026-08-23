# Project Io — REFINED (active worklist)

**Empty between work blocks.** Sprints P1, C3, B2 and B3 closed 2026-08-21; **N1 closed 2026-08-23**.
**Sprint N2 is active** — three lanes in parallel worktrees, one of them Sprint 28 (Lane A)'s
measurement half. See § Sprint N2 below.

---

## Sprint N2 — "the spines move" (opened 2026-08-23, three lanes in parallel worktrees)

N1 landed two spines and a check, **all three inert**. An inert subsystem with no caller is the
`military_points` defect with a new name, so N2 gives each one a consumer — and measures the one
thing Lane A cannot fix until it is measured.

| Lane | Item | Files (slice-owned) | Done when |
|---|---|---|---|
| **N2-a** | BL-542 (nation scorer) | `src/world/nation_ai.{hpp,cpp}` (new), `tools/verify/nation_scorer_harness.cpp` (new) | req group `nation-scorer` R1–R6 |
| **N2-b** | BL-546 (reputation → sentiment) + BL-391 (the floor deadlock, landed through it) | `src/world/procurement.cpp`, `src/world/corp_command.cpp`, `src/world/sentiment.cpp`, `tools/verify/procurement_harness.cpp` | req group `reputation-becomes-sentiment` R1–R4 |
| **N2-c** | Sprint 28 Lane A, T1+T2 — verb-competition **measurement** | `src/world/history_sim.{hpp,cpp}`, `tools/verify/history_conquest_gap.cpp` | req group `verb-competition-measurement` R1–R4 |

**Hotspots stay with the main session**, as always: `scripts/economy.lua` (5 open items declare it),
`src/world/components.hpp` (4), `src/world/world.hpp`, `CMakeLists.txt`. N2-b may legitimately need
`world.hpp` to remove `corp_reputation`; that edit is flagged for checking at merge rather than
forbidden.

**Every lane is briefed to MUTATION-TEST its own rows** and report which mutation it ran per row.
That is the N1 lesson applied (NR-547): three agents wrote the code *and* its harness, all three
harnesses passed, two of three subjects were unsound, and not one defect was found by reading.

---

---

## Sprint 28 — "growth stops extinguishing war" (Lane A)

> **T1 and T2 are running now as Sprint N2's lane c.** The section below is the full argument and
> stays here as the reason; T3 (the fix) is deliberately unwritten until T1/T2 report, and T4 is
> untouched.

### Why it is re-scoped before a line is written

Sprint 28's stated goal was *"a province changes hands, and a large polity keeps
campaigning."* **The first half is already true.** `tools/verify/history_conquest_gap.cpp`
measures 1199 conquests over 1226 battles across 8 worlds. Building toward it would have
been building something that works.

BL-384, which the sprint rests on, is **refuted as written**: it reports "267 battles and
ZERO conquests" from a single seed and reads it as a property of the sim. All three
mechanisms it names as candidates are dead:

| Its hypothesis | Measured |
|---|---|
| The scorer is optimistic by roughly the terrain factor | **−0.0pp** over 1215 of 1226 battles |
| The scored staging hub differs from the levying hub | **0 of 1226** |
| Victories never clear the transfer bar | **99.8%** of victories convert |

### The real defect, and it is stranger than the stated one

**Two of eight worlds fight no war in an entire era.** Not few — none. And the funnel says
exactly where they stop:

| Seed | Battles | Contacts | Scored | **Chosen** |
|---|---|---|---|---|
| **0** | **0** | 4,972,710 | 9,945,420 | **0** |
| 1 | 236 | 6,189,696 | 12,379,392 | 241 |
| 2 | 435 | 2,620,190 | 5,240,380 | 435 |
| **4** | **0** | 4,089,264 | 8,178,528 | **0** |
| 7 | 184 | 1,578,177 | 3,156,354 | 185 |

It is **not** adjacency and **not** candidate discovery. Seed 0's scorer looks at a war it
could start **ten million times** and picks something else on every one of them. Note also
that contacts do not predict war at all, and if anything predict it inversely — seed 7 has
the fewest contacts and fights; seed 0 has the most and does not.

So the sprint's subject is **verb competition**, not combat tuning. That is a different
sprint from the one on the board, and a smaller one.

### Tasks

Foundation first; each scoped to its files.

- **T1 — separate "never cleared the threshold" from "always lost to another verb".**
  Two counters on the existing trace: campaign candidates whose score cleared
  `campaign_threshold_q`, and rounds where a cleared campaign lost the `>` comparison to
  Settle / Consolidate / build_work. **This is the whole sprint's fork** and everything
  below is conditional on it. Files: `src/world/history_sim.{hpp,cpp}`,
  `tools/verify/history_conquest_gap.cpp`. **No dependency.**

- **T2 — report the WINNING verb's identity and margin when campaign is beaten.**
  If T1 says campaign clears and loses, this names what beats it and by how much. A margin
  of 3 and a margin of 3000 are different bugs: the first is a weighting nudge, the second
  is BL-318 incommensurability — two scores authored on different scales, which this file
  has been bitten by twice (`w_cult` as a flat 150; `w_dist` flat against a tripled map).
  Files: `src/world/history_sim.{hpp,cpp}`, `tools/verify/history_conquest_gap.cpp`.
  **Depends on T1.**

- **T3 — the fix, whichever T1/T2 names.** Deliberately unspecified here: writing it now
  would be committing to a mechanism before the measurement that chooses it. Two
  constraints hold whatever it turns out to be, and they bound it from opposite ends:

  **A zero-war world is a BUG** — Ben's ruling, 2026-08-21, and his reason is what makes it
  a floor rather than a preference: *"many ancient tech quests would not be unlockable."*
  The failure is content reachability, not atmosphere. But **the peaceful worlds must not
  be tuned away wholesale** either: `BL-224`'s non-hegemony invariant wants some worlds to
  stay multipolar. So the target is a *rate inside a stated band across seeds*, reported
  and never clamped (Sprint 30's done-when says exactly this).

  Worth knowing while building it (NR-490): the war-gated content the floor serves is
  **designed, not live**. `scripts/tech_tree.lua` holds 150 nodes and says in its own
  comment that E0-ML-01 is "THE ONE LIVE GATE"; the other 149 are authored stubs, and even
  that one reads corp military strength rather than battles. This is an argument *for*
  fixing the rate now — the tech design is being drawn on top of a war rate that is zero
  in a quarter of worlds. Files: `src/world/history_sim.cpp`, `scripts/` tuning if the
  answer is a constant. **Depends on T2.**

- **T4 — BL-321's defence works are inert in practice, and it is a separate defect.**
  `work_defence_mod` is non-zero in **zero** of 1226 traced battles. Both sides of the
  plumbing are correct and no battle in eight full runs was ever fought against a region
  carrying one. Two readings, needing different fixes: the works roster rarely fires at
  all (the `works_raised` counter would say), or works are raised on safe interior regions
  while fighting happens on frontiers that have none — which would mean the sim invests in
  defence exactly where defence is not needed. Report `works_raised` beside the traces and
  cross built-works regions against fought-over ones. Files:
  `tools/verify/history_conquest_gap.cpp`, `src/world/history_sim.cpp`. **No dependency —
  parallel with T1.**

### Not in this sprint, and why

- **Sprint 29 (the sim's result reaching the campaign map)** stays next, not now. Carrying
  a political result over is worth less while a quarter of worlds have no political result
  worth carrying.
- **Combat constants.** Measurement says they are calibrated. BL-384's own design is
  explicit that tuning them would be tuning the wrong thing.

### Owed from the closed sprints

- **Two goldens red on `main`**, confirmed independently by two lanes reverting to base:
  `spectator_determinism` R2 and `ai_skill_harness` (28). Neither re-blessed — Ben's
  signature, and B3 caveats that the container's Lua-free build may itself shift the hash
  (NR-487).
- **The population-centre divisor** (NR-482). Not a generation knob: `economy_system`
  derives each body's whole workforce from its centres, so halving it takes headcount
  27,580k → 90,980k. Two structural criteria point at ~150 and ~100–120; both more than
  double the labour pool.
- **BL-522** (whole-route haul mispricing, priority A) and **BL-523** (corp kind axis,
  design-owed) both filed this session and unqueued.

---

## Previously (kept for the session's record)

## Sprint C3 — BL-467 LANDED 2026-08-21

Both resolvers now have a production caller. `src/world/battle_system.{hpp,cpp}` runs
from `run_economy_step` immediately before `run_unit_march`. All ten tasks done;
`battle_engagement_harness` 26/26.

**Its B1 is the row that could not have been written before the item**: it runs the
REAL tick rather than calling a resolver, and asserts men died — 1000 → 648 in one
tick. B1c asserts the negative that makes it mean something: two NEUTRAL corps
sharing a province do not fight.

**Four defects were found AFTER it compiled and passed 26 checks** (NR-463), by a
scout told to refute rather than confirm — a withdrawal that cost no men, a dedup
that silently no-oped with three corps, an unscreened all-naval false victory, and
zero-count units that were never reaped because the item's own aftermath ruling was
false about the code. All four fixed.

**The rider is measured, not taken** (NR-467): `tools/verify/unit_upkeep_rates.cpp`
sweeps five candidate rate sets. Picking one stays Ben's, per `economy.lua`'s own
"tuned against a measured baseline rather than guessed here".

### Owed on C3

- **The rate ruling** (NR-467). The finding that should drive it: a flat per-head
  rate is regressive to the point of being a different rule at different corp sizes
  — "light" costs a large corp 7.8% of income and a small one **155%**.
- **Four named stubs** (NR-465): doctrine is all-zeros, season is hardcoded to
  summer, membership is snapshotted at open, and holding the field has no
  consequence because no territorial-control concept exists. R9 is **partial**.
- **BL-468** (battle dispatches) and **BL-469** (the battle card) are the surfaces
  that make the fight watchable — C3's theme names them and neither is built.

## Landed this session (2026-08-21)

All four assigned items plus the two unqueued ones, on branch
`claude/bl-519-520-nr-438-439-n29ljl`.

- **NR-438 / NR-439 — the raised ceiling.** 12 is a preference, 20 the asserted
  hard cap, over-12 share reported never asserted, `IO_ABSORB_PREFER_ROOM` gone.
- **BL-519 — the tile axis split.** `terrain_composition` → substrate × cover ×
  density. 522 references across ~110 files. New `tile_axes_harness` (13 checks).
  The 120-seed earthlike census is bit-identical to the pre-split baseline.
- **BL-521 — click injection in the verify API.** Drives ImGui's real event queue;
  double-click promotion forced per press so frame time cannot decide it.
- **BL-520 — basic tile texturing.** Grain on the substrate, pattern on the cover;
  13 composable marks instead of 84 enumerated. LOD off below 14px.
- **BL-516 — water kinds and sea provinces.** Structural lake/coast/ocean with no
  threshold; sea provinces pinned by measurement at d=7, mean 41.07.

**The one thing all of it shares: nothing has been rendered.** This container
cannot build the GUI, so every UI half is compile-clean and arithmetically checked
and visually unseen. No golden was blessed — blessing an unseen frame pins
whatever got built.

## Owed, and each has a review entry

- **The visual pass**, on Ben's machine: BL-519's colour blend (NR-451), BL-520's
  marks and lens attenuation (NR-457), BL-521's click script (never executed),
  BL-516's water in/out of the blend.
- **Rulings waiting**: the 80-tile sea clamp exceeded at 82 (NR-460, the same
  shape as NR-438); `is_coastal` narrowed to sea, costing ~22% of coastal tiles
  their port eligibility (NR-461); lakes' province home (NR-462); does the
  province blend survive texturing, which also decides BL-514.
- **Stale goldens, none re-blessed**: `ai_skill_harness` (28), `history_sim_harness`
  (8), and the pre-BL-409 `state_hash` quoted in the standing rules (NR-452).
- **Panels still not clickable by name** — needs a target registry (NR-454).

## Also ready, unqueued

- **BL-521 — click injection in the verify API.** Designed this session at Ben's
  instruction, deliberately unbuilt. This is the item that closes the live-check
  class permanently: a non-interactive agent currently CANNOT satisfy a live check,
  so every interactive surface it builds arrives with its live half owed by
  construction. Two agents in a row hit this on BL-511 alone.
- **BL-516** (lake/coast/ocean tile kinds, sea provinces capped at 80 tiles) and
  **BL-518** (the Era −1 sim redrawing borders as its wars resolve) — both unblocked
  now that BL-515 is settled.

## Still owed, each with its review entry

- **The live click** on a province (BL-511 R1) — NR-416, NR-424. BL-521 is the fix.
- **Six items landed-awaiting a live check**: BL-412, BL-408, BL-411, BL-480,
  BL-429, BL-453 — NR-388.
- **BL-408 has no entry point at all.** `ui_state.spectating` has one writer in the
  tree, the Lua binding. Needs a ruling, not a task — NR-389.
- **BL-458 shipped silent.** Comms message, Convoys-tab cause and canvas mark all
  absent; they need a field on `world` that other lanes were holding — NR-407.

## Rulings still open

- **NR-406** — should the building ceiling move during play, since infrastructure is
  an input?
- **NR-421** — the per-province firm cap is inert by orders of magnitude (296 firms
  across 295 distinct provinces). Drop it, keep it for later density, or move the
  constraint to a grain where firms actually compete.
- **NR-415** — where does the per-lens reduction table live, and does every future
  lens inherit the obligation?


---

# Sprint N1 — "The two spines, landed inert" (Lane N) — opened 2026-08-22

**Why now.** This session designed the whole corp↔nation channel and settled 41 open calls.
**None of it exists in code.** This sprint lands the two spines the rest of v0.1.24 requires, plus
one check.

**The batch's single organising decision: everything lands INERT.** Authored rates at zero, so a
world runs bit-identical to today. That is the pattern BL-343 (law), BL-480 (treasury) and BL-454
(upkeep) all shipped under, and it is the only way to add two subsystems and a check without moving
a single golden — which matters more than usual here, because the economy benchmark is *already*
deliberately red (NR-269/271/272) and a change that moved it would be unattributable.

## The build recipe — this container CAN verify (NR-264, re-proven 2026-08-22)

`cmake` configure **fails**: SDL3 and Lua arrive by FetchContent and the network policy refuses the
download, so no target is generated and every harness *looks* unbuildable. It is not.

```
ls src/world/*.cpp | grep -Ev '(recipe_registry|works_registry|tech_tree|world_gen_config)\.cpp$'
g++ -std=c++20 -O1 -Isrc -c <each>          # 47 TUs, xargs -P8, ~17s
ar rcs libioworld.a obj/*.o
g++ -std=c++20 -O1 -Isrc tools/verify/<h>.cpp libioworld.a -o <h>   # ~5s
```

Re-verified this session: `law_author_harness` green at 14/14. **This is the answer to NR-392** —
a worktree agent now has a sanctioned way to build a harness, and every agent brief carries it.
New harnesses need **no CMakeLists edit**: `file(GLOB IO_VERIFY_HARNESSES tools/verify/*.cpp)`.

## Sprint N1 — CLOSED 2026-08-23. All three landed; two were unsound and were fixed.

Harnesses green at **36 / 41 / 22**, plain and under ASan+UBSan. Retro in `sprints.json` § N1.
Defects fixed in the closing pass, each now guarded by a row that fails against the pre-fix code:
an ASan-confirmed heap-buffer-overflow (`uint8_t`-backed enum indexing a 9-element array), an
overdrawable spend bound (156 overdraws and 26 negative treasuries over 512 generated shapes), a
check that went red on the day its subject was satisfied, a content claim that survived a ×10
reprice, and an order-independence row that survived deleting the sort it guarded. Conservation's
claim was narrowed rather than the code changed — NR-546.

### N1-A — BL-543, the value anchor's CHECK *(no C++ struct change; cleanest slice)*
- **A1** `tools/verify/value_anchor.cpp` — read `scripts/economy.lua` § `unit_upkeep` and
  `scripts/world_gen.lua` § `base_price`; assert the band `Σ(goods × base_price) ≈ 2 × credits`
  per roster class, within a stated tolerance.
- **A2** The mutation row: perturb a rate in-harness and assert the check goes **red**.
- **A3** Report the implied *units funded per year* for the seeded extraction levy, so NR-382 can
  be answered against a number.
- *Scope: `tools/verify/value_anchor.cpp` only. **Authors no rate** — the numbers wait on BL-544.*

### N1-B — BL-545, the sentiment substrate
- **B1** `src/world/sentiment.{hpp,cpp}` — a directed, continuous value per (observer, subject)
  over **two dimensions** (Access, Trust), for corps and nations alike.
- **B2** Storage on `world` + the decay/factor pass, authored in `economy.lua` at **zero**.
- **B3** `tools/verify/sentiment_harness.cpp` — the six requirement rows, the **stance invariant**
  first.
- *Files: `src/world/sentiment.{hpp,cpp}` (new), `src/world/world.hpp` (hotspot — main session),
  `scripts/economy.lua` (hotspot — main session), the harness.*

### N1-C — BL-537, the national budget
- **C1** `src/world/nation_budget.{hpp,cpp}` — weights over priority lines, `reserve_fraction`,
  the per-tick spend pass.
- **C2** The field on `nation_component`, and the hook in `apply_budget`.
- **C3** `tools/verify/nation_budget_harness.cpp` — conservation first.
- *Files: `src/world/nation_budget.{hpp,cpp}` (new), `src/world/components.hpp` (hotspot),
  `src/world/budget_system.cpp` (hotspot), `scripts/economy.lua` (hotspot), the harness.*

## Collision map — a splitting heuristic, not a gate

| File | N1-A | N1-B | N1-C | Handling |
|---|---|---|---|---|
| `tools/verify/*` (3 new) | ✔ | ✔ | ✔ | disjoint — no collision |
| `src/world/sentiment.*` | | ✔ | | new TU, slice-owned |
| `src/world/nation_budget.*` | | | ✔ | new TU, slice-owned |
| `src/world/world.hpp` | | ✔ | | **hotspot — main session** |
| `src/world/components.hpp` | | | ✔ | **hotspot — main session** |
| `src/world/budget_system.cpp` | | | ✔ | **hotspot — main session** |
| `scripts/economy.lua` | reads | ✔ | ✔ | **hotspot — main session** |
| `CMakeLists.txt` | | | | none — harnesses are glob-picked |

**The call:** three worktree slices for the new TUs and harnesses; **every shared-header and
`economy.lua` edit is done by the main session at integration.** That is the DELIVERY rule
(hotspot/integration wiring stays in the main session) and it is what keeps two difficulty-5 items
in one batch tractable.

---

## BL-536 (world snapshot save) — promoted 2026-08-22

Full mode. Requirement group `world-save-snapshot` (R1–R6) written first, per step 0.
Ben's four calls, 2026-08-22: build now · field-wise writes · full four-part envelope ·
battles serialised.

**No sub-agent fan-out this session** (session instruction). Sequential in the main
session; the collision map below is kept as the record of how it *would* have split.

| # | Task | Files | Depends on |
|---|---|---|---|
| T1 | Binary primitives + the magic/version header contract | `src/world/binary_io.hpp` | — |
| T2 | The world snapshot: header, well-known entities, every serialised store | `src/world/world_save.{hpp,cpp}` | T1 |
| T3 | Load-side rebuild — clear derived caches, re-fold `corp_modifiers`, `m_next_id` accessor | `src/world/world_save.cpp`, `src/world/world.hpp` | T2 |
| T4 | The app envelope — `generation_report`, sim tick, `world_params`, histories, `ui_state` slice | `src/core/save_game.{hpp,cpp}` | T1, T2 |
| T5 | Wiring — `--load`, the save trigger, the `--verify` skip-generation hook | `src/main.cpp`, `src/core/app.{hpp,cpp}` | T4 |
| T6 | Round-trip harness — R1–R4, R6 (world half) | `tools/verify/save_roundtrip.cpp` | T2, T3 |
| T7 | Visual check — a loaded world renders as the generated one (R5) | `scripts/verify/save_load.lua` | T5 |
| T8 | Docs — TECH_FOUNDATIONS present tense at last, BL-107 resolved, ACTIONS entry if a press lands | `docs/tech/TECH_FOUNDATIONS.md`, `docs/ai/ACTIONS.json` | T1–T7 |

**All eight tasks completed 2026-08-22; requirement group `world-save-snapshot` complete (R1–R6).
Commits: `3832eb6` (world half), plus the envelope/wiring/docs commit.**

Two things came out of the work that were not in the plan:

- **`corp_modifiers` is not recomputable** (NR-510). The re-fold BL-107 prescribed cannot
  reconstruct cross-tick earn order, and `modified_scalar` folds non-commuting operations.
  Serialised directly; `world.hpp`'s comment and BL-107's note both corrected.
- **`verify.command` never routed through `dispatch_action`** (NR-513), so nine commands —
  every time control and UI toggle — silently did nothing through it. Fixed; behaviour-
  identical for all existing callers, which drive only navigation commands.

**Verification honestly stated.** R1–R4 headless (`save_roundtrip`, 36 assertions). R5/R6
via `save_load.lua`: three byte-identical before/after capture pairs, plus a live `--load`
launch. The F5/F6 keys were exercised through `dispatch_action` (the function the key table
calls) and their presence in `s_bindings` confirmed via the generated F1 overlay — **an actual
SDL keypress was not machine-driven**; computer-use could not resolve a locally-built exe.
The app is left open on the save for Ben to press them.

**Collision map (splitting heuristic).** T2+T3 are one vertical slice over `world/*`;
T4+T5 are one over `core/*`; T6 and T7 are independent verifiers that only need the
signatures. T1 is the shared foundation everything else includes — it lands first, alone.
