# Project Io — REFINED (active worklist)

**Empty between work blocks.** Sprints P1, C3, B2 and B3 closed 2026-08-21.
**Sprint 28 (Lane A) is now active, re-scoped on measurement.**

---

## Sprint 28 — "growth stops extinguishing war" (Lane A)

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
  would be committing to a mechanism before the measurement that chooses it. One
  constraint holds whatever it turns out to be — **the peaceful worlds must not be tuned
  away wholesale**. `BL-224`'s non-hegemony invariant wants some worlds to stay
  multipolar, so the target is a *rate inside a stated band across seeds*, reported and
  never clamped (Sprint 30's done-when says exactly this). Files: `src/world/history_sim.cpp`,
  `scripts/` tuning if the answer is a constant. **Depends on T2.**

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
