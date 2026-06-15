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

## Where we are — v0.0.4

The engine, data model, primary canvases, world generation, and the single-body economy loop
are complete:

- **Layer 0–2** — SDL3 + fixed-timestep simulation + economy tick + sol2; the core data
  model; the Solar / Body-Surface canvases with the minimap zoom ladder.
- **World generation (v0.0.3)** — procedural tiles (two-axis terrain), nations (Voronoi BFS),
  and corporations (asset placement, financial profile).
- **Layer 3 economy (v0.0.4)** — extraction → processing → per-`(corp, body)` stockpile →
  per-body market with supply/demand **price resolution** → budget; finite **deposit
  depletion**; a warm-start so the world opens live; the player's balance in the header.

The economy *runs* but is not yet *played*: the player observes authored assets rather than
building and managing them. Closing that gap is the spine of the road to v0.1.0.

---

## The road to v0.1.0

Five minor versions, then the cut. Each is a single theme; v0.0.6 is the likely split point.

### v0.0.5 — Layer 4 foundations

*Theme: make the economy buildable-on.* The enablers that must precede any building-management
UI — the reusable placement-rules seam (so player construction and generation share one
validity check), a multi-tick economy-stability harness (insurance before UI piles on the
loop), the workforce-model design (settled before it is exposed), uniform ledger-window
chrome, and the Layer 4 UI groundwork scaffold. Low-risk, largely disjoint, fan-out-friendly.

### v0.0.6 — Building management + population

*Theme: the economy becomes interactive and gains depth.* The heart of "Layer 4 working":
player **construction** (placement + build-cost spend + terrain/deposit validation),
**recipe / workforce / sell-order** control, and the **Market / Balance / Construction**
ledger family — coupled with **population centres** (scale, agglomeration, land-use
trade-offs, habitability feedback) and the **workforce pool** that grounds labour supply.
The largest minor; **likely to split** at promotion (building management ahead of population,
or construction ahead of the ledger family). Planned as one theme; expected to land in two.

### v0.0.7 — Supply routing (Layer 5)

*Theme: the economy gains space.* Supply convoys with source/destination/cargo and fractional
progress; distance-based logistical cost; delivered cargo adjusting the destination market.
Inter-body market linkage unlocks here — **price can now diverge between bodies**, the first
real spatial-strategic dimension.

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
