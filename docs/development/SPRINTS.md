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
