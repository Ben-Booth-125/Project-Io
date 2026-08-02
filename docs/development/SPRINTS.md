# Project Io — Weekly Sprints

A lightweight weekly rhythm layered over the backlog/Delivery system: a **goal** stated at the
start of the week, a **retro** at the end comparing what landed against it. This is feedback *for
Ben* — pacing and priority signal, not a new authority (backlog.json/REFINED.md/DEVLOG stay the
source of truth for what's actually true about an item).

Entries are newest-first, one per **sprint** — a sprint is a themed span of work, not a fixed
calendar week; it closes when its goal is settled (landed or deliberately descoped), not on a
clock. A gap with no entry means no sprint goal was set — that's fine, skip it rather than
backfilling.

## Format

```
## Sprint N — theme (opened YYYY-MM-DD)

**Goal.** 1-2 sentences: the outcome this sprint is aiming for, referencing backlog item ids
and/or a version goal (v0.1.1 etc.).

**Planned.** BL-ids targeted, one line each.

**Retro** (filled in at close).
- Landed: ...
- Slipped: ... (+ why — scope grew, blocked on a dependency, deprioritized)
- Runtime: total session time this sprint (sum of DEVLOG Runtime lines) vs. items delivered —
  the pacing signal Ben asked to track (tools/session/timer.js).
- Feedback: anything worth Ben knowing about how the sprint actually went — a pattern, a
  misjudged estimate, a design call that needed more/less discussion than expected.
```

---

## Where things stand (2026-08-02)

| Sprint | Theme | State |
|---|---|---|
| 1 | Procedural generation v1 | **Closed** — goal met (food cluster landed 2026-08-02, see amendment) |
| 2a | Close out the v0.1.0 cut set | **Closed** — all four planned items landed |
| 2b | BL-210 oral-history pivot (nations/corps rewrite) | **Closed** — all four rungs built (BL-217, BL-208, BL-218, BL-219) |
| 3 | Corp AI stage B + skill harness | **Closed** — BL-203, BL-204 both landed |
| 4 | Communication surface (BL-205 chat log) | **Mostly landed** — slice 1 (window, channels, agency feed) complete 2026-07-26/28; only the C-route remainder (§7 Stage C) stays open, unstaffed |
| 5 | Era −1 history sim, 0–2000 CE (BL-271–275) | **In progress** — foundation wave (BL-272, BL-273) landed 2026-08-02; wave 2 (BL-274) ready to promote |

**Next up:** Sprint 2b is closed — BL-218 (nations settlement rewrite) and BL-219 (corporations
history rewrite) both landed 2026-08-02. What remains of BL-210's umbrella is its batch-sweep
extension and TILE_GENERATION.md's share of the propagation. The natural successors are
**BL-223 (averted rupture)**, which now has real settlement history to define the diplomacy origin
against, and the four v0.1.2–v0.1.5 stubs (BL-155/156/157/158) that were sequenced *behind* this
rewrite precisely so they would not be designed against Voronoi blobs — nations now have the
history to hang policy, military and politics off. Unplanned v0.1.1 work (BL-270, the action
dictionary) still wants a sprint of its own rather than riding on 2b.

---

> **Numbering note (2026-08-02).** Two entries below were both opened as "Sprint 2" on
> 2026-07-31 — the v0.1.0 cut set and the BL-210 decomposition. They are kept under their
> original headings (renamed 2a / 2b) rather than renumbered, since commit messages
> ("Sprint 2 promotion: BL-217") already point at 2b.

## Sprint 2a — Close out the v0.1.0 cut set (opened 2026-07-31, closed 2026-08-02)

*(Goal set by the assistant during the 2026-07-31 doc sweep — Ben to amend.)*

**Goal.** Land the remaining v0.1.0-goal items so the prototype cut is unblocked: the build
surface finished, terrain's combat consequence in, the tooltip text rendering clean.

**Planned.**
- BL-162 — tile construction panel: the per-candidate expected-profit chart (the one owed half)
- BL-233 — terrain combat modifiers (measurement landed 2026-07-31; adoption decision + wiring)
- BL-234 — font glyph range (defect: `fonts.cpp` atlas misses U+2014/U+2265; 26 strings show "?")
- BL-226 — continent lens: finish the settled option-B follow-on (still open, `designed`)

**Retro** (closed 2026-08-02).
- **Landed: all four.** BL-162 (tile construction panel), BL-233 (terrain combat modifiers),
  BL-234 (font glyph range) and BL-226 (continent lens) are all `complete` in `backlog.json`.
  The goal — unblock the v0.1.0 cut — was met in full.
- **Landed beyond plan:** the three v0.1.0 audit instruments (frame-budget HUD BL-249,
  econ-tick scaling, data-creep BL-251), the harness build-type/ctest-timeout fix (BL-255),
  the planetary canvas cull + cache (BL-268), and the action dictionary (BL-270, v0.1.1).
- **Slipped:** nothing from the plan.
- **Runtime:** not summed — the timer gap carried from Sprint 1 is still unfixed.
- **Feedback:** a four-item sprint with every item already `designed` closed cleanly and
  absorbed six unplanned items on top. The pattern holds: sprints scoped to promote-ready
  work land; sprints scoped to a theme (Sprint 1) drift.

---

## Sprint 5 — Era −1 history sim (opened 2026-08-02)

**Goal.** Build the 0–2000 CE settlement/mil-sim sandbox (BL-271–275) that proves out the nation
AI and mil-sim architecture, and tunes the campaign's non-hegemony premise against measured
distributions rather than lore. Bot-only, behind a harness flag — never in the shipped campaign
path (BL-271's stated bound).

**Dependency chain.** BL-272 (combat model) and BL-273 (province demography) have no unmet
dependencies (BL-233 and BL-218 both already landed) and touch disjoint files — **foundation
wave, run in parallel**. BL-274 (era-keyed rosters) needs BL-272. BL-271 (the sim loop) needs all
three. BL-275 (the seed sweep) needs BL-271.

**Planned.**
- BL-272 — unit/doctrine combat engine (foundation wave) — **complete 2026-08-02**
- BL-273 — province demography + manpower (foundation wave) — **complete 2026-08-02**
- BL-274 — era-keyed unit rosters (wave 2, needs BL-272 — dependency now satisfied)
- BL-271 — the year-tick sim loop (wave 3, needs BL-272/273/274)
- BL-275 — the seed-spread sweep (wave 4, needs BL-271)

**A detour, recorded not repeated.** This sprint opened mid-session with a design pass toward a
different item, BL-205's C-route (in-character LLM chat) — it went as far as a full in-process
API-call design before Ben clarified that wasn't the intent (human-in-the-loop play via
computer-use, not a shipped API integration). Reverted cleanly; see NEEDS_REVIEW.json NR-039/NR-040.

---

## Sprint 1 — Procedural generation v1 (opened 2026-07-21)

**Goal.** Take procedural generation to a v1 worth building on — not just the immediate
rivers/food cluster, but a foundation the rest of generation (deposits, climate, market/pop
co-gen) can extend cleanly rather than fight later.

**Planned — this sprint's build (settled, in progress).**
- BL-170 — rivers as an edge feature + logistics discount
- BL-166 — Hydroponics Bay (habitability/terrain-gated food source)
- BL-168 — Fishing Wharf (coastal-adjacency food source)
- BL-190 — food demand model (settled: plain market pull, no starvation)

**Filed this sprint, not yet in scope to build.**
- BL-188 / BL-189 — coastal ports + coastal defense (design-owed, spun off BL-168)

**Design-settled this sprint (surveyed list, widened per Ben's "finalize v1" framing).**
- BL-040 — resource generation full-set deposits: **corrected**, already shipped (backlog was
  just stale bookkeeping).
- BL-051 — tile generation refinements: **settled** — the sibling-pass pipeline-shape convention
  now standard for all future generation extensions.
- BL-132 — market/population co-generation: **settled** — blocker cleared, sequencing fixed.
- BL-167 — **reframed** as Planetology (atmosphere/chemistry/evolution history,
  Shadow-Empire-inspired), un-parked, raised to priority B. New authority doc
  `docs/generation/PLANETOLOGY.md` — design-owed on its own detail (chemistry fidelity, evolution
  abstraction, presentation surface), a likely Sprint 2 candidate.

**R&D pass — BL-167 Planetology (2026-07-21, Ben's ask).**
Ben asked for a rough chemical understanding of how generation gets from (A) an input solar system
to (B) life to (C) civilisation, always human and always carbon-based. Researched across 13 domains
with an adversarial fact-check per domain, then synthesised into `PLANETOLOGY.md` § The chain: ten
gated stages, each a threshold test rather than a simulation, each emitting one dated timeline line.
The **B→C joint is now concrete** — a biosphere physically manufactures the industrial base (banded
iron, petroleum, coal, bauxite, supergene copper, soil), so "no life, no coal" is a mechanism, not a
label. The **homeworld rule** is settled in shape: constrain the *inputs*, never the gates or the
outputs. **11 open calls are recorded and are Ben's** — the two load-bearing ones being the
dead-code problem (at four hand-authored bodies the chain produces one interesting world, so most of
the ladder is unreachable until `body_count` is activated) and `deposit_scalar` ownership vs BL-114.

**BL-167 then BUILT the same day, and the dead-code call resolved.** Ben: *"One planet with life is
much better for what I'm imagining"* — the unreachable rungs are the intended shape, not a gap. The
chain shipped as a sibling pass deriving each `body_profile` (23 of 24 fields reproduce the authored
values; Kepler bit-identical), and the one-shot flow became a **ten-stage New World wizard** — one
decision per stage, charted in the tile-selection idiom — on Ben's follow-up: *"I would prefer that
the user sees each stage and chooses at each stage."* So BL-167 is **complete**, not Sprint 2 input.
Sprint 1's goal (procgen to a v1 worth building on) is now materially exceeded on the generation
axis; the rivers/food cluster (BL-166/168/170) is still the outstanding half.
- BL-169 (solar geometry feasibility) — not touched; a different axis (system layout, not planet
  generation), left for its own pass.

**Retro** (closed 2026-07-31 — *written by the assistant during the doc sweep, not by Ben;
factual from git log + backlog.json, amend freely*).
- **Slipped: the entire planned build set.** BL-166 (hydroponics), BL-168 (fishing wharf),
  BL-170 (rivers) and BL-190 (food demand) were all re-goaled to **post-v0.1.0** in the backlog
  without a closing entry here — deprioritised when the v0.1.0 cut set took over, not blocked.
- **Landed instead** (the window's actual output, 2026-07-21 → 2026-07-31): the BL-167 Planetology
  chain + New World wizard (noted above); the v0.1.0 legibility batch (BL-174/176/177/178/179);
  corp AI stage A + blackboard + persona counsel (BL-202/206/207); the Selection band move
  (BL-213); the history ledger (BL-211); comms dock (BL-227); hover glance-then-stick (BL-230,
  retiring BL-228's freeze model); continent lens (BL-226, follow-on still open); dated history
  timestamps (BL-220); pre-national ladder (BL-221); landform render + spanning markers
  (BL-231/232); terrain-combat measurement (BL-233, ladder untouched). Plus three bulk golden
  re-blesses and the BL-220..225 history-ladder filing.
- **Runtime:** not summed — the wall-clock timer gap noted below was never fixed, so the pacing
  signal for this sprint is unreliable; treat it as missing rather than compute a bad number.
- **Feedback:** the sprint goal named one theme (procgen v1) and the work delivered another
  (v0.1.0 legibility + AI + history). The generation half was genuinely exceeded (BL-167), but
  the rivers/food cluster — the stated "outstanding half" — quietly left the version goal without
  this file noticing. That is the exact drift a close-out entry exists to catch; close the sprint
  when the goal changes, not just when it lands.
- *(Carried note: session Runtime tracking hit a real gap this sprint — the wall-clock timer
  conflated an idle/interruption gap with work time; worth fixing before relying on it.)*

**Amendment (2026-08-02) — the slipped set landed after all.** The retro above was written on
2026-07-31 while the cluster sat at post-v0.1.0. It was re-promoted the next working day and
built: BL-170 (rivers & freshwater, incl. the directed-river render + chevrons), BL-166
(Hydroponics Bay), BL-168 (Fishing Wharf, plus a construction-ledger visibility fix) and
BL-190 (food demand) are all `complete`; the full grid goldens were re-blessed after river
generation shifted the world. Sprint 1's goal is therefore **met**, not slipped — the
deprioritisation was a ten-day deferral, not a drop. The feedback point still stands: this
file did not notice the version-goal change in either direction.

---

## Sprint 2b — BL-210 decomposition → the Nations/Corporations history rewrite (opened 2026-07-31)

**Status 2026-08-02: IN PROGRESS** — decomposition done, and the chain's first two rungs built.

**Goal.** Take BL-210's umbrella pivot (design-owed, difficulty 5, "the biggest remaining piece"
per DEVLOG 2026-07-28) from one unactionable mega-item to a sequenced set of promote-ready design
passes — this is also the first sprint to deliberately practice the Design depth verb
(DELIVERY.md § Depth verbs) rather than jumping straight to code.

**Why this over the v0.1.2–v0.1.5 stubs.** Ben's ask was to decompose the diplomacy/war/tech/legal
placeholders (BL-155/156/157/158). Investigation first: those four are all `design-owed` against
the *current* nation model (Voronoi BFS + a random politics draw), and BL-210 is already
mid-rewrite of exactly that model into a simulated settlement/industrialisation history. Designing
Laws/Politics against nations that are about to stop being Voronoi blobs risks rework. Ben chose
(2026-07-31) to sequence BL-210 first; the four stubs resume once nations have real history to hang
policy/military/politics off.

**Planned — this sprint's decomposition (done 2026-07-31).**
- Split BL-210's three still-open subject-doc passes into their own sequenced items, since a
  difficulty-5 item should break down (DELIVERY.md § Priority, difficulty & version goal) rather
  than stay flat:
  - **BL-217** — checkpoint/branch data model (where branch state lives; how a lean biases which
    branch fires at a checkpoint, extending BL-167's preferences mechanism). No dependency beyond
    the already-shipped BL-167. Difficulty 3.
  - **BL-218** — Nations rewrite: territory/political-character/wealth as *outputs* of a simulated
    settlement → industrialisation → 1900s history, replacing Voronoi BFS + random politics draw.
    Requires BL-217. DEVLOG's named "biggest remaining piece". Difficulty 4.
  - **BL-219** — Corporations rewrite: industrial focus as an emergent consequence of the home
    nation's settlement history, replacing the authored focus table. Requires BL-218. Difficulty 3.
  - BL-210 itself stays open as the umbrella closing condition (all three built, plus the
    batch-sweep extension and authority-doc propagation into TILE_GENERATION/NATION_GENERATION/
    CORPORATION_GENERATION).

**Not yet in scope this sprint.** Actually *settling* BL-217/218/219's open questions (each still
needs a real design conversation — see each item's `design` field) or touching any `src/`. This
sprint's goal was decomposition only (the Design depth verb's first half); running BL-217's design
conversation is the natural Sprint 3 candidate once Ben is ready to work through its open questions.

**Progress (2026-08-02).** The "not yet in scope" line above was overtaken — BL-217's design
questions were settled and the item **built**, not just designed.
- **BL-217 (checkpoint/branch model) — complete.** Append-only `checkpoint_record` + a generic
  eligibility-filter mechanism in `planetology.{hpp,cpp}`; R1–R6 met, CTest 37/37.
- **BL-208 (world history log) — complete**, resequenced ahead of BL-218/219 because both need
  somewhere to write their history. The project's first flat-binary serialiser (BL-107's seam);
  the genesis + checkpoint chapters bridge generation output into `world` for the first time.
- **BL-218 (nations settlement rewrite) — not promoted.** Still `designed`; the strict chain
  means it starts once BL-208 is in `main` (it now is).
- **BL-219 (corporations history rewrite) — not promoted.** Depends on BL-218.
- **BL-210** stays open as the umbrella closing condition.

**Progress (2026-08-02, later the same day) — the chain's last two rungs landed.** Ben's steer:
"complete 2b, and finish with the backend of history implementation… as long as there is a way to
map belief systems onto existing and warring civilisations", and explicitly "don't be afraid to
have parts of the record erased when two nations go to war."

- **BL-218 (nations settlement rewrite) — complete.** `src/world/settlement.{hpp,cpp}`: the
  province becomes the unit that carries belief, ancient endowment and industrial timing at once.
  Nation seeds are now province anchors (seeding changes, expansion does not); the three political
  axes are derived outputs; the historical ruptures are BL-217's second checkpoint class, with
  collapse/war/revolution as real transforms.
- **BL-219 (corporations history rewrite) — complete.** Focus derives from the corp's home
  province plus the movement up the value chain; the authored table is retired and diversity
  becomes a world-level reject-and-reroll.
- **The erasure landed as asked.** A won war plants the victor's pantheon on the provinces taken
  and destroys the lines naming them, leaving a dated lacuna with a count. Four of six seeds lost
  part of their record.
- Verified by a new `settlement_harness` (S1–S8); full CTest **39/39**, which needed two harness
  follow-ons (`history_ladder_harness` H4 narrowed, `ai_skill_harness` MSVC goldens re-blessed —
  every divergence upward) plus one unrelated pre-existing fix (`trade_routes_harness` had not
  linked since BL-170 landed rivers).

**Retro** (closed 2026-08-02).

- **Landed against the goal: the whole chain.** BL-217 → BL-208 → BL-218 → BL-219 are all
  `complete`, and the authority-doc propagation went with them rather than being deferred.
  BL-210's umbrella is down to its batch-sweep extension and TILE_GENERATION.md's share.
- **What the decomposition bought.** The sprint's stated goal was decomposition *only* — "not yet
  in scope: actually settling BL-217/218/219's open questions or touching any `src/`". All of that
  happened anyway, and cheaply, because the decomposition had already named the real dependency
  order. The design passes settled in one session each because they were asking answerable
  questions.
- **Feedback: the "not yet in scope" line was overtaken twice.** Once on 2026-08-02 morning
  (BL-217/BL-208 built) and again the same afternoon (BL-218/BL-219). A sprint whose scope is
  overtaken twice in a day was scoped as a plan when it was really a queue — which is the same
  observation Sprint 3's retro made about numbering being a theme label, not a schedule.
- **Feedback: a whole-world change moves goldens, and the suite says so honestly.** Two harnesses
  moved and both were verified against a stashed pre-change baseline rather than assumed
  pre-existing. Worth keeping as the habit: the cost of checking was one stash and one target
  rebuild, and it turned "probably already broken" into a known, explained divergence.

---

## Sprints 3–5 (committed 2026-07-31) — re-sequenced: skilled NL agents for stress-testing via simulated play

**Ben's steer, same session as Sprint 2's close:** systems get proven by simulating play, so the
next three sprints put agent skill ahead of the BL-217 design conversation Sprint 2 flagged as
"natural next." BL-210/BL-217–219 stay queued, not dropped — they resume after.

**Why this doesn't need new design work (token-sparing).** `AI_OPPONENT.md` already carries a
Ben-accepted architecture (§5, 2026-07-26) and a filed follow-on decomposition (§8): BL-202 (Stage
A scorer, **complete**) → BL-203 (Stage B predictive spending — the "skilled" half) and BL-204 (bot-
vs-bot skill-regression harness — the literal simulated-play stress test) → BL-205 (chat/diplomacy
surface) → BL-207 (persona counsel packs). All four are already `designed` (promote-ready) — this
is execution, not another design pass, which is the cheap path.

### Sprint 3 — Stage B + the skill harness (BL-203, BL-204)

**Goal.** Make the corp AI actually skilled (predictive spending replacing the crude solvency
floor) and give it a regression harness that *is* simulated play: seed-set bot-vs-bot goldens
(solvency, net-worth curves) plus the tick-boundary state hash (doubles as the multiplayer desync
primitive, AI_OPPONENT.md §8). Both require only BL-202 (complete) — no blocking dependency.

**Planned.** BL-203 (diff 4), BL-204 (diff 3). Promote together — BL-204's goldens are the natural
acceptance check for BL-203's behaviour change, so building them in the same batch is cheaper than
sequencing.

**Retro** (closed 2026-07-31 — landed early, ahead of Sprint 2b's chain).
- **Landed: both.** BL-203 (corp AI stage B: strategy layer, priority buckets, predictive
  spending) and BL-204 (AI skill-regression harness: seed-set bot-vs-bot goldens + tick-boundary
  state hash) are both `complete`. BL-235 (creeds) landed alongside, wired into CTest.
- **Feedback:** Sprint 3 overtook Sprint 2b rather than following it — the planned sequence and
  the actual order diverged, which is fine but means "Sprint N" here reads as a *theme label*,
  not a schedule. Treat the numbering that way.

### Sprint 4 — the communication surface (BL-205)

**Goal.** Land the § 7 chat principle: a channel-based message surface (Public + arbitrary groups)
replacing the Explorer placeholder, carrying Stage-A templated decision-log messages now and
becoming the medium personas (BL-207) and, later, a free-text LLM planner (Stage C) speak through.
`designed`, no dependency — could in principle run before Sprint 3, sequenced after only to keep
one theme per sprint.

**Planned.** BL-205 (diff 3).

### Sprint 5 — RE-THEMED (2026-08-02): the Era −1 history sim (BL-271–275)

**Ben's steer (2026-08-02 brainstorm, recorded in the items themselves):** run the "Rome as
sandbox" plan as Sprint 5. Once generation produces a spread of earth-like planets, refine the
worlds' philosophical development by getting tons of examples running 0 CE → 2000 CE — and,
overturning the abstract-war rule, simulated history fights with **real units and real tactics**,
as typed unit types the main era later inherits (BL-272 records the overturn).

**Goal.** The settlement world (BL-218) promoted from a one-shot generation pass to a year-tick
simulation, with a combat engine the 1960 era will share.

**Planned.** BL-271 (Era −1 sim, diff 4) · BL-272 (unit/doctrine combat model, diff 4) ·
BL-273 (province demography, diff 3) · BL-274 (era-keyed unit rosters, diff 3) ·
BL-275 (history sweep distributions, diff 3 — also closes BL-210's last scope item).
Dependency order: 272 → 274, 218 → 273, all → 271, 271 → 275.

**Consumers this unblocks:** BL-054's parked runtime nation-AI half, the BL-155/156
diplomacy/war stubs (sequenced behind the rewrite for exactly this), and BL-223 (averted
rupture), which gets designed against simulated near-ruptures instead of lore.

**The previous Sprint 5 theme (persona audit + Stage C naming) rides along as its original
small scope** — a diff 1–2 reconciliation plus filing one design-owed item — rather than
holding the slot:

**Old goal.** Two threads. First, reconcile a likely stale status: REFINED.md's 2026-07-27 "AI
constituents batch" already logged BL-207 R1–R3 as covered (persona packs C1, loader/runtime C2,
Counsel channel C3, `persona_counsel_harness` C4) and marked the batch **COMPLETE**, but
`backlog.json` still carries BL-207 as `designed`, not `complete` — verify against the harness and
flip the status if the earlier landing checks out, rather than re-building already-shipped work.
Second: file the still-unitemized **Stage C** — the out-of-process LLM planner that speaks
in-character in channels (AI_OPPONENT.md §2 Area C, §7 Stage C) — as a `design-owed` item. This is
the actual "skilled natural language agent" tier Ben is pointing at; A/B (BL-202/203) and the chat
medium (BL-205) are the scaffolding it needs to stand on, which is why it lands last, not first.

**Planned.** BL-207 audit (diff 1–2), file Stage-C item (diff 1, design-owed, no build).

**Not yet in scope.** Actually promoting Sprint 3 into REFINED.md task groups + `requirements.json`
— that collision-mapping step happens at the start of Sprint 3 itself (DELIVERY.md: built fresh at
promotion, not frozen ahead of time), not in this planning pass.
