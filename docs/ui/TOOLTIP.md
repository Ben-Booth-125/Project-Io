# Project Io — Hover Card

Authoritative spec for the **shared hover-card primitive** — the transient,
pointer-following popup that shows a compact readout of whatever the player is
hovering, on any canvas or in any ledger. It is the **Focus**-state surface in
the three-state pointer model (see [SELECTION.md](SELECTION.md)): where the
Selection info element answers "what is this thing I clicked?", the hover card
answers "what is this thing under my cursor right now?", and dismisses the
instant the pointer leaves.

It is one primitive serving every hoverable entity, built on the same
per-entity content builders the Selection info element and the ledgers use. This
document settles its structure, what it serves, and the design decisions the
[B4] Brief raised. It is **design only** — nothing here is implemented yet; the
ad-hoc canvas tooltips described under *Current state* are what exists today.

See also: [LAYOUT.md](LAYOUT.md) (§ UI popup elements — where the card sits in
the shell), [SELECTION.md](SELECTION.md) (the three pointer states and the
shared content builders), [CANVASES.md](CANVASES.md) (the canvases that host it),
`src/ui/entity_summary.hpp` (the shared builders), `src/ui/presentation.hpp`,
`src/ui/format.hpp`, `src/ui/icons.hpp` (the metadata/formatter/glyph helpers).

---

## Current state (what exists)

The prototype already has **ad-hoc, per-call-site tooltips**, each open-coding
its own content:

- **Planetary** (`body_surface_canvas.cpp`) — an `ImGui::BeginTooltip` block on
  the hovered tile: grid coordinates, composition · landform, hazard.
- **Solar / Circumplanetary** (`solar_system_canvas.cpp`,
  `circumplanetary_canvas.cpp`) — an `ImGui::SetTooltip` on the hovered body:
  name, type, orbital radius.
- **Overlay strip** (`overlay.cpp`) — a one-line `SetTooltip` naming the lens.

These are lightweight, instant, and inconsistent: each duplicates formatting the
[SELECTION.md](SELECTION.md) builders already centralise. The hover card unifies
them onto one primitive and one content source, exactly as the Selection element
unified the per-entity stat blocks.

---

## Card structure

A hover card is a stack of up to four bands, top to bottom. Only the title line
is mandatory; the rest appear when the entity and the caller's context warrant
them.

1. **Title line** — `icon name · type`. The identity glyph from
   `ui::icons` (building marker, resource pip, unit chevron, ledger glyph,
   body dot), the entity's display name, and a dimmed type label
   (`selection_kind_name`, or the more specific `body_type_name` /
   `building_type_name`). This is the same title shape the Selection panel header
   draws (`selection_title` + dimmed `selection_kind_name`).

2. **Stat block** — a short, fixed set of the entity's defining figures: a tile's
   composition × landform + hazard; a building's recipe + throughput; a market's
   headline price and balance; a body's orbit + surface summary. This is
   **identical content** to the Selection element's stat block — it *is* the
   shared builder (see *Sharing the content builders*).

3. **Sectioned detail (optional)** — when an entity has more than the headline
   stats, a thin separator then a second labelled section (e.g. a building's
   per-input draw, a market's per-good rows). Kept short; the hover card is a
   glance, not a ledger. The full breakdown stays in the relevant ledger.

4. **"Why" annotations (optional)** — a dimmed trailing line explaining how a
   derived figure was produced: how a market **price** was set, how a building's
   **yield** was scaled (workforce scalar, deposit richness, terrain affinity).
   This is the hover card's distinctive value over the click-to-inspect path —
   it surfaces *derivation at the point of curiosity* without making the player
   open a ledger and reason backward. The annotation reads from the same
   intermediate figures the economy/generation records already expose (cf. the
   Generation Ledger's per-pass breadcrumb in
   `docs/generation/GENERATION_LEDGER.md`); where a figure has no recorded
   derivation, the band is simply omitted.

The card has **no chrome** — no title bar, no close button, no 'go to'. It is
transient and self-dismissing; any action belongs to the click that follows, not
the hover.

---

## What it serves

One primitive, dispatched by `selection_kind` (`src/ui/selection.hpp`), serving
every hoverable entity across every surface:

| Entity | Hovered on | Headline stats | "Why" annotation |
|---|---|---|---|
| **Body** | Solar, Circumplanetary, minimap | Type, orbit, parent, surface summary | — |
| **Tile** | Planetary surface | Composition × landform, hazard, habitability, deposits | Deposit richness → terrain affinity |
| **Building** | Planetary surface, Tile Ledger rows | Type, recipe, throughput, host tile | Yield = base × workforce scalar × deposit |
| **Market** | Tile Ledger / Market ledger rows | Body, headline price, balance | Price = supply/demand pressure |
| **Resource pip** | Resource strips, deposit markers | Name, tier, stockpile | — |
| **Unit / convoy** *(later)* | Canvas markers | Type, owner, location, status | — |
| **Route / lane** *(later)* | Logistics overlay | Endpoints, cargo, throughput | — |

The set is deliberately the **same polymorphic family** as the Selection element
(SELECTION.md § Polymorphism) plus the lighter strip/pip entities. New kinds are
added in one place — the builder dispatch — and inherit the card for free.

---

## Design decisions

### 1. One `draw_hover_card(...)` helper, not per-call-site tooltips

**Recommendation: a single dispatcher, reusing the existing per-kind builders.**

The Selection element already established the pattern: `selection_kind_of`
classifies the entity, and a `switch` dispatches to the matching builder in
`src/ui/entity_summary.hpp` (`draw_body_summary`, `draw_tile_summary`,
`draw_building_summary`, `draw_market_summary`, …). The hover card is the *same
dispatch wrapped in a tooltip frame*:

```
void draw_hover_card(const world& w, entity_id hovered);
// internally: kind = selection_kind_of(w, hovered);
//             ImGui::BeginTooltip(); draw title; draw_summary(w, kind, hovered); EndTooltip();
```

Each canvas/ledger computes its `hovered` entity (it already does, as a
per-frame local — SELECTION.md calls this the **Focus** state) and calls the one
helper. This replaces the three open-coded tooltip blocks with one call site
each. Per-entity *builders* exist; per-call-site *tooltips* should not.

This keeps the abstraction seam exactly where SELECTION.md put it: the **builder**
is shared content; the **frame** (pinned panel vs. ledger row vs. tooltip)
differs per caller. The hover card is just the third frame.

### 2. Instant reveal for lightweight cards, short delay for rich cards

**Recommendation: instant for the title-only / single-stat case; a short
(~400–500 ms) hover delay before a rich, multi-band card.**

The existing canvas tooltips are instant and should stay so — an instant
identity readout while sweeping the pointer over bodies/tiles is good. But a
*rich* card with derivation annotations is visually heavy; showing it the instant
the pointer grazes an entity makes a busy canvas flicker. So: the **lightweight**
card (title + one stat line) appears immediately; the **rich** card's extra bands
(sectioned detail + "why") fade in only after the pointer **dwells**. ImGui
exposes the dwell timer (`IsItemHovered(ImGuiHoveredFlags_DelayNormal)` /
`HoveredFlags_ForTooltip`), so this is a flag on the same call, not a second code
path. The delay constant is a single tunable; **flag for the owner** whether
~450 ms feels right in play (see *Owner's call*).

### 3. Rich card vs. lightweight canvas tooltip — same primitive, two depths

**Recommendation: not two primitives — one card with a depth that scales to the
context.** LAYOUT.md § UI popup elements already distinguishes "a richer hover
card than the canvas tooltip"; this spec resolves that as **depth, not type**:

- **Lightweight** (today's canvas tooltips) = bands 1–2 only (title + stat
  block), instant.
- **Rich** = bands 1–4 (adds sectioned detail + "why"), delayed.

The same `draw_hover_card` produces both; the caller passes a depth hint (or the
card derives it from the entity kind — a market/building warrants the rich form,
a passing body does not). Keeping it one primitive means the title/stat content
never diverges between the two, which was the duplication the builders exist to
prevent.

---

## Overlap with the Selection info element

The hover card and the Selection element (SELECTION.md) present the **same
per-entity detail** in different frames. They must **share, not duplicate**:

- **One content source.** Both call the `src/ui/entity_summary.hpp` builders.
  The Selection panel calls `draw_summary` inside its pinned frame; the hover
  card calls the same `draw_summary` inside `BeginTooltip`/`EndTooltip`. A change
  to a tile's stat block changes both surfaces at once. This is the seam
  SELECTION.md § Shared content builders already names the hover card as a
  consumer of — this doc just makes the card the concrete third caller.
- **Three pointer states, three surfaces.** SELECTION.md's table maps cleanly:
  **Active** drives which rung renders, **Selection** drives the pinned panel,
  **Focus** (the per-frame `hovered_*` local) drives *this card*. The card is the
  visible manifestation of the Focus state, which until now had no UI.
- **Distinct lifetimes.** The Selection panel **persists** until you select
  elsewhere and carries actions ('go to', close). The hover card is **transient**,
  follows the pointer, and carries no actions. Same content, opposite
  persistence — which is exactly why they are separate frames over one builder
  rather than one widget.
- **Builder signature.** The builders take `(const world&, entity_id)` and emit
  ImGui content into the current window — frame-agnostic by construction, so they
  already work unmodified inside a tooltip. No builder change is required to land
  the card; only the new dispatcher and the call-site swaps.

---

## Layer 4 support

Layer 4 brings building and market detail to the fore (the Market / Balance /
Construction ledger family, per ROADMAP / OPENS § Ledger). The hover card is the
**low-friction inspection path** for that data:

- **Building hover** surfaces recipe, throughput, and the **yield derivation**
  ("why" band) — letting a player read a site's economics without opening the
  Construction ledger. The builder is `draw_building_summary`; the card adds the
  derivation annotation on top.
- **Market hover** surfaces headline price and balance plus the **price
  derivation** ("why" band: supply/demand pressure) — the single most-asked
  economy question, answered at the point of pointing. The builder is
  `draw_market_summary`.
- Because the card reuses the ledger's own builders, **the Layer 4 ledgers and
  the hover card stay in lockstep by construction** — a new market figure added
  to the ledger row appears in the card without separate work.

The "why" annotation band is the piece that earns its keep in Layer 4: it turns
the card from a label into the game's primary *explainer* of derived economic
figures, complementing the deeper, persistent ledgers.

---

## Owner's call (flagged)

- **Reveal delay.** ~450 ms for the rich card is a starting guess; the right
  value is a feel decision made in play, not from first principles.
- **Rich-by-default kinds.** Whether markets/buildings should jump straight to
  the rich card on hover, or always start lightweight and deepen on dwell, is a
  UX preference for the owner once the card is in hand.
- **"Why" verbosity.** How much derivation to show (one summary line vs. a small
  factor breakdown) trades clarity against clutter; settle against a real
  populated economy in Layer 4.

---

## Prototype / deferred notes

- Convoy/route hover entities are designed-for here but land with logistics
  (Layer 5); their builders do not yet exist.
- The card has no spec-level styling beyond reusing `palette` and `icons`; it
  inherits the shell's ImGui theme.
- Like all popup elements (LAYOUT.md), this is prototype ImGui; the production
  shell's retained-mode equivalent is a later concern.
