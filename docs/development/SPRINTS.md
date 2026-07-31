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

**Retro** — *pending, fill in once BL-166/168/170 land (build in progress) and Sprint 1's goal is
otherwise settled. Note: session Runtime tracking hit a real gap this sprint — the wall-clock
timer conflated an idle/interruption gap with work time; worth fixing before relying on it for
the retro's pacing read.*

---

## Sprint 2 — BL-210 decomposition: the Nations/Corporations history rewrite (opened 2026-07-31)

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

**Retro** — *pending, fill in once BL-217's design conversation happens or Sprint 2 is otherwise
closed out.*

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

### Sprint 3 — Stage B + the skill harness (BL-203, BL-204) — **PROMOTED, ready to build**

**Goal.** Make the corp AI actually skilled (predictive spending replacing the crude solvency
floor) and give it a regression harness that *is* simulated play: seed-set bot-vs-bot goldens
(solvency, net-worth curves) plus the tick-boundary state hash (doubles as the multiplayer desync
primitive, AI_OPPONENT.md §8). Both require only BL-202 (complete) — no blocking dependency.

**Decomposed 2026-07-31** into REFINED.md § "Sprint 3 — Corp AI Stage B + skill harness" (10 tasks,
B1-B4/A1-A4/C1/D1) and `requirements.json` § corp-ai-predictive-spending / ai-skill-harness.
Source-verified before decomposing: `corp_ai.hpp` already has the Stage-A scaffolding (
`corp_ai_params`, `corp_reserve_floor`, `corp_personality_jitter`, `run_corp_strategic_step`) this
item extends rather than a blank slate. Harness (B-chain) lands first for a pre-change baseline
golden; strategy layer (A-chain) lands second; re-bless shows the delta as the acceptance evidence.
A4 also closes BL-205's own deferred "wire BL-202 command-stream messages into chat" tail — small
enough to ride along rather than reopen a sprint for it (see Sprint 4 below).

**Planned.** BL-203 (diff 4), BL-204 (diff 3).

### Sprint 4 — retired: BL-205's core surface was already built at filing time

**Finding (2026-07-31, source-checked before drafting a sprint for it).** `chat_panel.{hpp,cpp}`
already implements the whole first slice BL-205 itemises — `chat_channel`/`chat_state`, Public +
player-created groups, `chat_post`/`draw_chat_panel` — landed the same session BL-205 was filed
(2026-07-26), not left as a pending build. What BL-205's own summary names as still open ("LATER:
BL-202 command-stream messages; C-route in-character LLM chat; AI-AI private groups + intercept
mechanics") isn't a sprint-sized chat surface anymore — it's three small, disjoint follow-ons,
already covered elsewhere: the command-stream wiring is Sprint 3's task A4; the LLM chat is BL-220
below; the private groups + intercepts are BL-221 below. No standalone sprint needed — this is the
"trivial step, fold it in or spin it off" case Ben asked to watch for.

### Sprint 5 — persona bench expansion (BL-207 continuation)

**Finding.** No stale-status discrepancy after all: BL-207's own `progress_note` (added at a
2026-07-29 audit) already records slice 1 landed (pack loader, the mountain bench — sun-tzu/
amaterasu/krishna, the Counsel channel, `persona_counsel_harness`) while correctly keeping status
`designed`, because the item's full scope isn't done. Checked `persona_pack.hpp`: the bench loader
(`load_bench(dir)`) is generic — seating the hearth and banner benches, and the remaining
Faculty-analyst personas beyond the seated three, is authoring more `scripts/personas/*.lua` files
against an already-built runtime, not new plumbing. Mechanical, but real content work (each persona
needs its extractor + phrase bank + documented failure condition per persona-runtime.md) — still
worth its own sprint rather than folding into Sprint 3.

**Planned.** BL-207 continuation: hearth + banner bench authoring (remaining scope, no new backlog
item needed — same id, same design field).

### Sprint 6 — naming the natural-language tier proper (BL-220, BL-221) — **filed, not designed**

**Goal.** The two genuinely new threads this decomposition pass surfaced, filed 2026-07-31 as
`design-owed` (scoping only, per the Design depth verb):

- **BL-220** — Stage C, the out-of-process LLM planner (AI_OPPONENT.md §2 Area C / §7 Stage C).
  This is the actual "skilled natural-language agent" tier Ben named — everything through Sprint 5
  is the deterministic, legible scaffolding (scored actions, blackboard, chat medium, persona
  voices) it stands on. Difficulty 5; its own design pass names narration-only vs real-planning as
  the first fork.
- **BL-221** — AI-AI private message groups + the intercept mechanic, extending DISCOVERY.md's fog
  model to comms a third time (after BL-212's public-comms anonymisation). Smaller (difficulty 3),
  reuses BL-205's existing `chat_channel` machinery, and is a candidate to land before or
  independently of BL-220.

**Planned.** Design passes for BL-220 and BL-221 — not code. Sequencing between them (or against
BL-217's resumed nations-rewrite thread) is a call for whenever Sprint 6 actually opens.
