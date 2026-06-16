# Project Io — Roadmap

The milestone map from the current state to **v0.1.0**, the finished prototype. This
document is **forward-facing and lean**: it names the version sequence, the *theme* of each
minor, and the done-definition for v0.1.0. It deliberately does **not** enumerate individual
Briefs — that lives in [`OPENS.md`](OPENS.md) (described intent) and [`TASKS.md`](TASKS.md)
(the active worklist). The roadmap sits *above* both: it says which theme each minor carries;
OPENS/TASKS say what work realises it.

The prototype validates the **economy loop only**. The full scope and its exclusions
(Conflict, Research, Policy, Diplomacy beyond a data-model stub) are owned by
[`../tech/TECH_FOUNDATIONS.md`](../tech/TECH_FOUNDATIONS.md); the development constraints and
tone that govern *how* the work is done live in
[`DEVELOPMENT_PRACTICES.md`](DEVELOPMENT_PRACTICES.md). This document owns only the milestones.

---

## Versioning grain

One **minor version** (`0.0.x`) carries **one coherent theme** — a layer, or a cluster of
enablers, taken end-to-end and cut as a release (see `DEVELOPMENT_PRACTICES.md` § Cutting a
release). This is the grain established by v0.0.3 (the whole world-generation spine in one
bump). A minor is not a fixed quantum of work; it is a theme that reads as one thing.

A minor may prove too large and **split in practice** at TASKS promotion — that is expected,
not a roadmap failure. The plan below is the *intended* shape; the split point is called out
where one is likely.

---

## Where we are — v0.0.5

The engine, data model, primary canvases, world generation, the single-body economy loop, and
the Layer 4 foundations are complete:

- **Layer 0–2** — SDL3 + fixed-timestep simulation + economy tick + sol2; the core data
  model; the Solar / Body-Surface canvases with the minimap zoom ladder.
- **World generation (v0.0.3)** — procedural tiles (two-axis terrain), nations (Voronoi BFS),
  and corporations (asset placement, financial profile).
- **Layer 3 economy (v0.0.4)** — extraction → processing → per-`(corp, body)` stockpile →
  per-body market with supply/demand **price resolution** → budget; finite **deposit
  depletion**; a warm-start so the world opens live; the player's balance in the header.
- **Layer 4 foundations (v0.0.5)** — the reusable placement-rules seam, the multi-tick
  economy-stability harness, the workforce-pool/model groundwork, uniform ledger-window chrome,
  and the Layer 4 UI scaffold; plus the visual-verification **golden-image** harness (with
  pass/fail diffing live), the Resource / Market / Population / Scarcity **map lenses**, the
  multiple-market-centres seam (one market per body today, tile-centred catchment ready), and
  the world-gen fixes (orphan-island claim, lean focus-shaped corp holdings).

The economy *runs* but is not yet *played*: the player observes authored assets rather than
building and managing them. Closing that gap is the spine of the road to v0.1.0.

---

## The road to v0.1.0

Four minor versions, then the cut. Each is a single theme; v0.0.7 (the merged interactive build)
is the certain split point.

### v0.0.6 — Improved core-loop

*Theme: deepen and make the economy that already runs legible — no new player verbs yet.* The
loop (extraction → processing → market → budget) runs but is shallow and merely observed; this
minor sharpens it *before* v0.0.7 makes it interactive. Three strands:

- **Market depth.** Replace the flat anonymous auto-clear with the matched price-time **order
  book** (intra-body; the cross-body reachability bias waits on supply), and run the **inter-body
  market feasibility probe** — the convoy-free minimal coupling that tests whether the model
  carries its weight without triggering per-body data-creep.
- **A saturated world.** Generate the **nation-owned background substrate** so the map reads as a
  saturated earth-like economy to trade against, with real market liquidity, rather than the
  sparse world the lean corporation holdings leave today.
- **Legibility.** Refit the economy panel onto the shared ledger conventions (the convention
  reference the v0.0.7 ledger family lifts from), and the time-control clarity pass — non-linear
  speed curve and the days-until-next-quarter countdown.

Lower-risk consolidation that pays back when v0.0.7 piles player interaction onto the loop.

### v0.0.7 — Building management, population & supply (Layers 4–5)

*Theme: the economy becomes interactive and gains space.* The two large interactive builds,
**merged into one theme** — they are adjacent and share the construction / market / ledger
surfaces:

- **Building management + population (Layer 4).** Player **construction** (placement + build-cost
  spend + terrain/deposit validation), **recipe / workforce / sell-order** control, and the
  **Market / Balance / Construction** ledger family + corporation dashboard — coupled with
  **population centres** (scale, agglomeration, land-use trade-offs, habitability feedback) and
  the **workforce pool** that grounds labour supply.
- **Supply routing (Layer 5).** Supply convoys with source/destination/cargo and fractional
  progress; distance-based logistical cost; delivered cargo adjusting the destination market.
  Inter-body market linkage completes here — **price diverges spatially**, the first real
  spatial-strategic dimension, and the cross-body half of the order book unlocks.

**By far the largest minor; certain to split** at promotion — building management ahead of
population, construction ahead of the ledger family, and supply as its own slice. Planned as one
theme; expected to land across several releases.

### v0.0.8 — Budget + hardening (Layer 6 + polish)

*Theme: legible and stable.* The full budget made legible (income vs. expenditure broken down,
running balance under competing pressure); the shared hover-card system; completion of the
lens system; the menu vocabulary; and the standing known-bug and icon-consistency work. No new
systems — this minor consolidates and hardens what Layers 4–5 built.

### v0.0.9 — Code quality, performance & data-creep audit

*Theme: prove it holds up before the cut.* No new systems — a dedicated pass to **measure and
harden** what Layers 4–6 built, so v0.1.0 is cut on evidence rather than hope. Three things to
assess, each with a rough target a non-specialist can read straight off an instrument:

- **Frame budget (lag).** 60 FPS gives a **~16.7 ms** per-frame budget. The frame-time HUD (the
  [B4] known-bug instrument in OPENS) reads last / avg / max ms plus the **1% lows** — the worst
  1% of frames, which is what a player actually feels as a stutter. Rough targets: **avg < 8 ms,
  max < 16.7 ms** while panning the dense Kepler grid (180×84 = 15,120 tiles). If max spikes, the
  HUD's job is to say *why* — GPU present (vsync/compositor), draw-call volume (the immediate-mode
  tile loop), or per-frame allocation churn.
- **Econ-tick cost (scaling).** One economy tick should be invisible — **target well under 1 ms**
  for the prototype world. Extend the existing `econ_stability` harness to print the tick time and
  watch how it grows as bodies / corps / markets multiply. The thing to catch: a tick that grows
  **faster than linearly** in (bodies × corps) — that is an algorithm quietly going quadratic.
- **Data creep (the inter-body worry).** The concern flagged during the supply Q&A: structures
  multiplying *per body* — the `(corp, body)` stockpile map, per-body markets, live convoys. Add
  simple **counters** (entity count, pool entries, market entries, live convoys) and a **memory
  readout** (RSS — the resident memory the process holds), then run the harness long (100 → 1000+
  ticks): counts and memory should **plateau**, not climb without bound. A steady climb over a long
  *idle* run is the signature of a leak or unbounded growth, and points straight at the structure
  that is growing.

Plus the cheap hygiene that needs no instrument: the build stays **warning-clean**, a **one-off
static-analysis run** (cppcheck, or MSVC `/analyze`) read for genuine findings, and the headless
harnesses kept green. None of this requires benchmarking expertise — it is reading three numbers
off an instrument and checking they sit under a threshold. The detailed Briefs (what each
instrument prints, the exact harness extensions) are settled in OPENS when this minor is designed.

### v0.1.0 — Cut the prototype

*Theme: validate and release.* No new systems. A final verification pass against the
done-definition below, then the Cut.

---

## Done-definition — v0.1.0 (full economy loop)

v0.1.0 is the **economy loop, validated and playable end-to-end** — not the full game. It is
cut when all of the following hold:

- The player can **construct and manage** buildings — placement with cost and validation,
  recipe and workforce control, and sell orders — not merely observe authored assets.
- **Population centres** ground workforce supply, wages, and demand.
- Goods **move between bodies** via supply convoys, so **price diverges spatially** and
  logistics affects margin.
- The **full budget** reflects pressure from competing demands and can go negative.
- The **read surfaces** — the ledger family and the hover-card system — make stockpiles,
  markets, balances, and construction legible at a glance.
- The build is **green** and the loop is **verified** (headless economy/generation harnesses
  and visual capture checks).
- **Performance and data growth hold** (the v0.0.9 audit): frame and econ-tick budgets met, and
  counts/memory plateau over a long run rather than creeping.
- Excluded throughout, by scope: Conflict, Research, Policy, and Diplomacy beyond the
  data-model stub.

When these hold, the prototype has validated what it set out to validate, and v0.1.0 is the
milestone that says so.

---

## Near-term publish plan — sequencing the unblocked backlog

The milestone map above is **theme-level** and deliberately Brief-free. This section is the
**operational layer beneath it**: how the *currently-unblocked* Briefs in [`OPENS.md`](OPENS.md)
are sliced into work sessions and published. It names Briefs (which the map above does not) but
**does not duplicate their design** — each Brief's detail stays in OPENS; this is only the
*sequencing*. Treat it as the standing reference a session opens with, and update the slice list
as sessions complete. *(If it churns enough to feel out of place inside the lean roadmap, graduate
it to its own `PUBLISH_PLAN.md` and cross-link — for now it lives here so there is one place to
look.)*

### Batch Publish is a strategy, not a code-sprint

A **Batch Publish** ([`../GLOSSARY.md`](../GLOSSARY.md)) is the whole *process* of moving many
Briefs from intent to committed code — **collision mapping, checkpoints, and session boundaries
included** — not just "write a lot of code." Its load-bearing parts:

- **Barrier semantics** — the set advances breadth-first; every Brief clears a Publish step
  before any starts the next (OPENS § Publishing multiple Briefs together).
- **Collision mapping** — the file write-sets that decide what can fan out and what stays serial
  (built *per session*, not frozen here — see below).
- **Session boundaries as checkpoints** — a large set is **paused** at a clean, resumable
  boundary rather than forced to complete (TASKS.md § Pausing a task group). The slices below
  *are* those boundaries.

### Cross-cutting rules (read once, apply every session)

- **Golden-image diffing is live (landed Session 1).** The F3 golden-image Brief shipped first,
  as planned, so every `visual` check is now automated pass/fail against a blessed golden rather
  than eyeball-only — the real lever against silent bugs. Author a `scripts/verify/*.lua` and
  bless a golden for each new visual requirement (via the `verifier-visual` skill); re-bless when
  a Brief deliberately changes a captured surface.
- **Build per commit, not per wave.** One commit per Brief already means a green tree at each
  commit. The only *extra* build worth inserting is right after editing a shared/integration file
  (`app.cpp`, `ui_state.hpp`, `overlay.cpp`). A green build proves it *links*, not that it is
  *correct* — correctness rides on the requirement verification, not the compiler.
- **Fan-out is rare here.** Most unblocked work is UI and **collides on shared files**, so it
  serialises in the main session. The DEVLOG bears this out: even the disjoint 8-Brief Layer 3 set
  ran sequentially. Reach for sub-agents only where file scopes are genuinely disjoint (the
  world-gen fixes; the self-contained harness).
- **Hotspot files** (perennial collision points — plan around them, keep single-writer in the
  main session): `body_surface_canvas.cpp` (border + Resource lens + Market lens + hover-card),
  `icons.{hpp,cpp}` (every icon Brief), `app.cpp`, `ui_state.hpp`, `overlay.cpp`.

### The sessions

*Interrupted 2026-06-16.* The earlier ordered session slices (verification + world-gen foundation
→ the lens batch → UI polish → ledger foundation) were **largely worked through** by the v0.0.5
close — the golden-image harness, the lens batch, tile-centred markets, and the world-gen fixes all
landed. The plan was then **paused for a v0.0.6 brief-intake session** (the 14 Briefs filed across
[`OPENS.md`](OPENS.md) on 2026-06-16). The next session slices will be **rebuilt from the refreshed
OPENS backlog** rather than the stale per-session list that stood here.

### Out of scope for this plan (gated — do not pull in)

- **v0.0.7 (Layer 4 half)**: corporation dashboard, Market/Balance/Construction ledgers,
  population (S4 + dynamic), building management, workforce step 2.
- **Selection trio**: non-spatial routing, canvas hit-testing, lens-driven resolution — blocked
  on markers / ledgers existing.
- **v0.0.7 (Layer 5 half)**: supply convoys (S5), logistics/infrastructure, and the cross-body
  half of preferential purchasing — all blocked on Layer 5. *(The convoy-free inter-body
  feasibility probe and the intra-body order book move up to v0.0.6 — improved core-loop.)*
- **Design-owed (`~`)**: the v0.0.6 brief-intake (2026-06-16) parked a large design-owed backlog
  that each needs a *design* pass before any publish — a **lens-redesign cluster** (strip
  single-select/rename, the colour-scheme pass, per-rung representation, and the Production /
  Placement-suitability / Reach / market-boundary / resource-density reworks plus tooltip
  simplification); **core-loop design** (the nation-owned substrate, market-centre seeding,
  construction-cost-in-resources, stricter placement rules, habitability→workforce); and
  **world / country** work (Faction→Country rename, more countries generated "in history", nation
  behaviour, and the tile-gen deep models). None are promotable until settled.
- **v0.2**: resource-generation scarcity, full deposit authoring.

*(Two entries — C1 nav-rail ordering, A3 design-pass propagation — read as largely doc-only and
self-describe as already settled into their authority docs; check whether they are stale entries
to **remove** rather than work to publish, before slotting either into a session.)*

### Collision maps: built per session, not frozen here

A deliberate choice. This plan records the **durable** facts — the slices, their order, and the
**hotspot files** — but **not** the fine-grained per-task collision table. That table is a derived
artifact that goes stale the moment a file is restructured or a Brief re-scoped, so it is **built
fresh at promotion** in TASKS.md (§ Dividing work across agents, step 1), where it is actually
consumed. A session reads the hotspot list here, then maps its own slice. Documenting the full map
here would only rot; documenting the hotspots is what carries between sessions.
