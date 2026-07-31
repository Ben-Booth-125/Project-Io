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

## Sprint 2 — Close out the v0.1.0 cut set (opened 2026-07-31)

*(Goal set by the assistant during the 2026-07-31 doc sweep — Ben to amend.)*

**Goal.** Land the remaining v0.1.0-goal items so the prototype cut is unblocked: the build
surface finished, terrain's combat consequence in, the tooltip text rendering clean.

**Planned.**
- BL-162 — tile construction panel: the per-candidate expected-profit chart (the one owed half)
- BL-233 — terrain combat modifiers (measurement landed 2026-07-31; adoption decision + wiring)
- BL-234 — font glyph range (defect: `fonts.cpp` atlas misses U+2014/U+2265; 26 strings show "?")
- BL-226 — continent lens: finish the settled option-B follow-on (still open, `designed`)

**Retro** — *pending.*

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
