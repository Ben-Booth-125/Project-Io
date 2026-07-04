# Project Io — Backlog (legacy markdown bodies)

**Design prose now lives in `backlog.json` → `items[n].design`.** This file is a drain: it
holds markdown bodies only for legacy items not yet migrated (those with `design: "@BACKLOG.md"`
in the JSON). When an item is edited or promoted, its body moves to the JSON `design` field and
is replaced here with a tombstone line. This file is deleted once empty.

**Prose authority:** `backlog.json` `design` field for migrated items; this file for
`@BACKLOG.md`-sentinel items only. **Metadata authority:** `backlog.json` (status, priority,
difficulty, sequencing, files). **Method authority:** [`DELIVERY.md`](DELIVERY.md).
**Active worklist:** [`REFINED.md`](REFINED.md).

**Migrate-on-edit rule (2026-06-17):** when you settle an item's design or promote it,
move its prose to the `design` field in `backlog.json` and replace this body with a tombstone.

## Canvas

*(BL-001 promoted to REFINED.md v0.0.6 batch — body removed.)*

*(BL-002, BL-003, BL-004, BL-005, BL-006, BL-007 promoted to REFINED.md v0.0.6 batch — bodies removed.)*

- **[BL-008 ✓] Clarify the time control view.** Landed (design settled 2026-06-15); authority
  `docs/ui/TIME_CONTROLS.md` § Production clock view / `docs/ui/LAYOUT.md`. *(Tombstoned
  2026-07-04.)*

### Further map lenses (lens-ideas Q&A, 2026-06-16)

A brainstorm pass over *what else is informative as a map lens* (Q&A 2026-06-16). Five lens
ideas were accepted, plus one meta item. They extend the lens family in `docs/ui/LENSES.md`
(`overlay_mode` in `src/ui/ui_state.hpp`; render passes in `src/ui/body_surface_canvas.cpp`;
strip controls in `src/ui/overlay.cpp`). Glyph additions propagate to `docs/ui/ICONS.md`
§ Map-lens glyphs.

**Built 2026-06-16:** the **Population** (per-tile habitability tint) and **Scarcity**
(single-resource translucent heatmap) lenses landed as Planetary render passes (see DEVLOG and
`docs/ui/LENSES.md` § Population/Scarcity lens; the population *density* half is deferred with the
population layer). The remaining Briefs below are `~` (design owed) — each needs a settle pass
(rung applicability, legend/key, glyph, interaction) before promotion.

*(BL-009 prose promoted to backlog.json 2026-06-17.)*

*(BL-010 prose promoted to backlog.json 2026-06-17.)*

- **[F4 ~] Reach / Logistics-range lens.** *(Written 2026-06-16, lens-ideas Q&A.)* A static
  *reach* surface — how far deployment or supply can extend from ports/launchpads — complementing
  the Supply lens (which shows flow) with a distance/falloff field. **Flagged as needing more
  scoping** (deferred, `F`): the notion of "reach" is underdefined until the Supply/Layer-5 route
  model exists, and partly overlaps it. **Design owed:** what reach means (logistical cost radius?
  deployment range? both), its relationship to the [S5] Supply layer, rung (likely multi-rung like
  Supply), whether it is a distinct lens at all or a Supply sub-mode. Couples to § Supply [S5].
  Authority `docs/ui/LENSES.md` § Supply.

*(BL-012 landed 2026-06-17 — the per-lens rung sweep settled into `docs/ui/LENSES.md` § Rung
applicability, which is the authority; item removed from backlog.json. Tombstoned 2026-07-04.)*

### v0.0.6 brief intake — lens & legibility (2026-06-16)

A list-pass over noticed lens/UI issues (v0.0.6 brief-intake session, 2026-06-16). Expanded from
the user's notes; design is deliberately **owed** (`~`) — settle each before promoting. They extend
the lens family (`overlay_mode` in `src/ui/ui_state.hpp`; passes in `src/ui/body_surface_canvas.cpp`;
strip in `src/ui/overlay.cpp`); glyph/colour work propagates to `docs/ui/ICONS.md`,
`src/ui/presentation.hpp`. *(Companion non-lens items from the same intake are filed under their own
categories: § Ledger (multiple-markets visibility), § Trade (warm-start pass), § Workforce
(habitability→workforce rule), § Infrastructure (building rules, construction pricing), § Environment
(country rename + generation).)*

*(BL-013 prose promoted to backlog.json 2026-06-17.)*

- **[F3 ~] Supply-routes lens — market-connection graph.** *(Written 2026-06-16, v0.0.6 brief
  intake.)* A lens drawing the **graph of trade connections between markets**: one edge per active
  route, **thickness/colour from the net worth of all trades on it** (thick green = net positive
  value flow, red = negative). User's render note: it *reads* like an infinite loop but **the graph
  is rendered once per frame** — structure the build so edge aggregation is computed once, not
  recomputed per-edge. **Build-blocked on Layer 5 / the order book** (no inter-market trades to graph
  yet); decide whether this is the [S5] Supply lens itself or a distinct "trade-value" mode.
  **Design owed:** edge/value aggregation, the once-per-frame structure, rung applicability.
  Authority `docs/ui/LENSES.md` § Supply.

*(BL-015 prose promoted to backlog.json 2026-06-17.)*

*(BL-016 prose promoted to backlog.json 2026-06-24.)*

*(BL-017 prose promoted to backlog.json 2026-06-17.)*

*(BL-018 prose promoted to backlog.json 2026-06-17.)*

*(BL-019 prose promoted to backlog.json 2026-06-17.)*

*(BL-020 prose promoted to backlog.json 2026-06-24.)*

## Menu

*(BL-021 promoted to REFINED.md v0.0.6 batch — body removed.)*

*(BL-022 promoted to REFINED.md v0.0.6 batch — body removed.)*

*(BL-023 removed from backlog — stale, already settled into docs/ui/MENU.md.)*

## Ledger

*(BL-024 promoted to REFINED.md v0.0.6 batch — body removed.)*

*(BL-025 prose promoted to backlog.json 2026-06-17.)*

*(Layer-4 read-surface-family conventions preamble (decomposed 2026-06-15) collapsed 2026-07-04 —
every child Brief landed; the shared conventions live with the landed ledger family,
`docs/ui/LAYOUT.md` / the ledger windows.)*

*(BL-026, BL-027, BL-028, BL-029 promoted to REFINED.md v0.0.6 batch — bodies removed.)*

### Selection info element

Follow-up intent for the Selection info element (design in `docs/ui/SELECTION.md`;
shared per-entity content builders in `entity_summary.{hpp,cpp}`):

*(BL-030 promoted to REFINED.md v0.0.6 batch — body removed.)*

- **[BL-031 ✓] Canvas hit-testing for buildings / units / markets.** Landed (with the
  selectable-marker work, BL-059); authority `docs/ui/SELECTION.md`. *(Tombstoned 2026-07-04.)*

- **[BL-032 ✓] Lens-driven selection resolution.** Landed (design settled 2026-06-15); design
  propagated to `docs/ui/SELECTION.md` / `docs/ui/LENSES.md`. *(Tombstoned 2026-07-04.)*

## Documentation

*(BL-033 promoted to REFINED.md v0.0.6 batch — body removed.)*

*(BL-033, BL-034 promoted to REFINED.md v0.0.6 batch — bodies removed.)*

## Trade

The market layer. Per the 2026-06-14 Q&A, **market resolution collapses into Layer 3** and
price resolution has now landed; **inter-body trade stays open**. Markets are a per-body
exchange, **distinct from corp stockpile pools**. Design authority `docs/SYSTEMS.md` § Trade,
`docs/economy/RESOURCES.md`.

*(BL-035 prose promoted to backlog.json 2026-06-17.)*

*(BL-036 prose promoted to backlog.json 2026-06-17.)*

- **[BL-037 ✓] Preferential purchasing (choosing counterparties).** Shipped (design settled
  2026-06-15); design propagated to `docs/SYSTEMS.md` § Trade. *(Tombstoned 2026-07-04.)*

- **[BL-038 ✓] Inter-body / international markets.** Landed (design settled 2026-06-15); design
  propagated to `docs/SYSTEMS.md` § Trade / § Supply. *(Tombstoned 2026-07-04.)*

## Supply

The logistics layer (Layer 5) — the physical movement of goods between bodies. **Newly authored
2026-06-15** to close the v0.1.0 done-definition gap (*goods move between bodies via supply
convoys*); the layer previously had no Brief, only the gated Supply-lens spec and the deferred
inter-body-market note. Design authority `docs/SYSTEMS.md` § Supply, `docs/ui/LENSES.md` § Supply.

- **[BL-039 ✓] Supply routing — convoys (Layer 5).** Landed (design settled 2026-06-15;
  v0.0.7's theme); design propagated to `docs/SYSTEMS.md` § Supply / `docs/economy/SUPPLY.md`.
  *(Tombstoned 2026-07-04.)*

## Resources

The resource economy's data and quality work — the substrate Layer 4 sits on. Design
authority: `docs/economy/RESOURCES.md`, `docs/economy/PRODUCTION.md`.

*(BL-040 prose folded into the backlog.json `design` field — 2026-07-04 reconciliation.)*

## Workforce

The labour scalar (`docs/SYSTEMS.md` § Workforce, `docs/economy/POPULATION.md`).

*(BL-041 prose promoted to backlog.json 2026-06-17.)*

*(BL-042 promoted to REFINED.md v0.0.6 batch — body removed.)*

## Infrastructure

*(BL-043 prose promoted to backlog.json 2026-06-17.)*

*(BL-044 prose promoted to backlog.json 2026-06-17.)*

- **[BL-045 ✓] Logistics network & infrastructure model.** Landed (design settled 2026-06-15 at
  feasibility-probe depth); design propagated to `docs/SYSTEMS.md` § Infrastructure / § Supply.
  *(Tombstoned 2026-07-04.)*

*(BL-046, BL-047, BL-048, BL-049 promoted to REFINED.md v0.0.6 batch — bodies removed.)*

## Environment

The world-generation layer — terrain, nations, corporations. Design authority:
`docs/generation/{TILE,NATION,CORPORATION}_GENERATION.md`.

### Cross-cutting

*(BL-050 prose promoted to backlog.json 2026-06-17.)*

### Tile generation (terrain)

- **[F4 ~] Tile generation refinements (deferred — deep models owed).** The larger production
  passes noted in `TILE_GENERATION.md` § Deferred. **Triaged 2026-06-15** — these are *not* part
  of v0.1.0 design completion (they do not advance the prototype's economy loop) and each deserves
  its own focused settle pass:
  — **Full deposit authoring (the data half) is now covered** by the settled [B3 ✓] Resource
    generation Brief (§ Resources) — band table + affinity gates. This Brief keeps only the
    *generation-mechanics* passes below.
  — **Smooth (noise-blended) band transitions** and **coastline refinement** (enclosed seas,
    archipelagos, lakes) — buildable now (no new model needed), but cosmetic; deferred behind the
    economy work.
  — **Solar-parameter derivation from orbital mechanics** and **tectonic plate-driven landforms**
    — **genuinely speculative; design still owed** (the orbital-derivation formula, the plate
    model). These belong to a procedural-campaign milestone *beyond* the prototype, not v0.1.0.
  Kept as one deferred holder; promote individual passes only when a campaign needs them. Touches
  `src/world/tile_generation.cpp`; authority `docs/generation/TILE_GENERATION.md` § Deferred.

### Nation generation

Design authority: `docs/generation/NATION_GENERATION.md`.

*(BL-052 prose promoted to backlog.json 2026-06-17.)*

- **[B4 ~] Country generation — more countries, generated "in history".** *(Written 2026-06-16,
  v0.0.6 brief intake.)* The world needs **more countries** than today's nation generation produces,
  generated with a sense of **history** rather than a flat Voronoi partition. Couples to
  NATION_GENERATION (Voronoi BFS today) and the saturated-substrate Brief (§ Environment
  cross-cutting). **Design direction (2026-06-16, partial):** target **~45 countries** on Kepler
  (Earth-like density — Earth has hundreds; 45 is a manageable analogue given the lens readability
  constraint). Size distribution: **open** — keep the design flexible and tune based on the generated
  landmass and land shapes; a visual review pass after generation is the arbiter (not a fixed
  formula). "Generated in history" model: **still owed** — what fragmentation, historical conflict,
  or clustering logic produces this distribution at prototype depth. Count target and distribution
  are **open for visual revision** post-generation. Authority `docs/generation/NATION_GENERATION.md`.

- **[F5 ~] Deferred — nation behaviour & production passes.** Per NATION_GENERATION.md
  § Open items: the nation *system* (tax, licences, war, infrastructure), the
  sentiment graph, historical fragmentation (exclaves/disputed zones), and
  non-Kepler jurisdiction. Out of prototype scope; design owed when in scope.

### Corporation generation

Design authority: `docs/generation/CORPORATION_GENERATION.md`.

Open follow-ons recorded in CORPORATION_GENERATION.md § Open items (out of prototype scope):
building tiers/levels, allied-corp/franchise origin, post-WW2 asset-mix grounding, the analytical
corp-selection/re-roll flow, franchising, nation-seeded privatisation, automated tax, Era-based
sovereignty, and diplomatic posture. *(The lean focus-shaped starting-holdings revision [B4]
landed 2026-06-15 — see DEVLOG and the CORPORATION_GENERATION § Pass 3 doc change.)*

## Known Bug

Known defects have moved out of OPENS into [`KNOWN_BUGS.md`](KNOWN_BUGS.md) — a known bug is a
*reported defect with a settled or owed fix*, not a unit of design intent, so it does not belong in
the Brief backlog. The **frame stutter / performance** entry (with the settled frame-time-HUD
measurement instrument) and the **body-label stepping** entry (accept-and-document with the
step-together mitigation) now live there.
