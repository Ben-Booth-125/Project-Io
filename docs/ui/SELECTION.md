# Project Io — Selection Info Element

The **Selection info element** is a pinned, polymorphic panel that shows detail
about the **current selection** — whatever entity the player last single-clicked.
It is the persistent "what is this?" surface that complements the transient
hover card and the deep per-domain ledgers.

It is a *kind* of ledger (it presents per-entity detail and shares content
builders with the ledgers and the future hover card), but unlike the floating
ledgers it is **pinned chrome** and is **not** reachable from the navigation
rail — the only way to open it is to select something.

See also: [LAYOUT.md](LAYOUT.md) (where it sits in the shell), [CANVASES.md](CANVASES.md)
(the click model it revises), the deferred hover-card item in
`docs/development/BACKLOG.md`, and `src/ui/view_nav.hpp` (`focus_on_entity`, the
'go to' target).

---

## The three interaction states

This element forces us to name three distinct, previously-conflated pointer
states. They are independent: an entity can be any combination of active,
focused, and selected at once.

| State | Meaning | Lifetime | Drives | Backing |
|---|---|---|---|---|
| **Active** | The navigation **anchor** — which body/tile the canvas rungs are framed around. | Persists until you navigate. | Which Circumplanetary/Planetary rung renders, and around what. | `ui_state.active_body`, `ui_state.active_tile` (existing). |
| **Focus** | The entity **under the pointer** right now. | Transient, per-frame. | The hover tooltip / hover card (see [`TOOLTIP.md`](TOOLTIP.md)). | Per-frame `hovered_*` locals today (not stored); a future `focus` field when the hover card lands. |
| **Selection** | The entity the player **single-clicked** to inspect. | Persists until you select something else (or clear). | The Selection info element's contents and its 'go to' target. | **New:** `ui_state.selected_entity`. |

Key consequence: **Selection is distinct from Active.** Selecting a body in the
Solar view fills the panel but does **not** move or re-anchor the canvas. Only
**navigation** (double-click, or the panel's 'go to' button) changes Active.

---

## Click model (revises CANVASES.md)

The canvases today use **single-click-descends**. This element splits that into
two gestures, applied uniformly across all three canvases:

- **Single-click an entity → select it.** Sets `selected_entity`, opens (or
  re-points) the Selection info element. **No view change** — same rung, same
  pan, same zoom.
- **Double-click an entity → navigate to it.** The old descend/focus behaviour:
  routes through `ui::focus_on_entity`, which resolves the entity to its most
  informative view (descend a rung, focus a surface/tile, or — for non-spatial
  entities — open the relevant ledger).
- **Single-click empty space → clear the selection** (panel shows its empty
  state; see below).

The 'go to' button on the panel is exactly equivalent to a double-click on the
current selection.

This is a deliberate behavioural change: a single click no longer descends the
zoom ladder. CANVASES.md and the minimap ascend gesture are updated to match.
(Minimap ascend stays a single click — the minimap has no selection semantics.)

---

## Polymorphism — content by selection type

The panel is **polymorphic by selection kind**. The entity's kind is resolved by
probing the `world` maps (the same discrimination `focus_on_entity` already
does: `w.tiles`, `w.buildings`, `w.units`, `w.markets`, `w.bodies`, and later
nations / corporations / logistics vessels).

Each kind renders its own content and routes its 'go to' to the right place:

| Selection kind | Content (stat block) | 'Go to' target |
|---|---|---|
| **Body** (planet/moon/asteroid/station/star) | Name, type, orbit, parent; surface summary. Plus a **Survey section** (survey system, BL-067) and a **Commercial activity section** (activity fog, BL-089) — see below. | Canvas: `focus_on_surface` — descend to the body's **Planetary tile surface**, the most informative rung. (Not `focus_on_body` / the orbital framing: landing on a sparse circumplanetary view reads as "nothing happened", which was the *only-works-for-Kepler* symptom.) |
| **Tile** | Composition × landform, hazard, habitability, deposits. | **No-op for now.** A tile is selected from the surface it lives on, so 'go to' has nothing to descend to; pan-to-tile is out of scope. |
| **Building** | Type, recipe, throughput, host tile. Player-owned buildings carry a **profitability readout** (BL-074) — see below. | Canvas: `focus_on_tile` (host tile). |
| **Market** | Body, headline prices / balances. | Canvas: `focus_on_surface`; or Market ledger. |
| **Unit / logistics vessel** | Type, owner, location, status. | Canvas: `focus_on_surface` / vessel's position. |
| **Nation** | Name, character, territory summary. | A ledger (no canvas of its own). |
| **Corporation** | Name, parent nation, headline standing. | A ledger (no canvas of its own). |

So 'go to' is itself polymorphic: spatial entities navigate a canvas;
non-spatial entities (nation, corporation) open the relevant ledger. For the
prototype the spatial kinds (body, tile, building) are wired first; the rest are
designed here and stubbed.

### The tile element is the build front door

Beyond its stat block, the **Tile** selection carries a **"Build here" affordance** — the
player's primary construction entry point. It lists the building types placeable on the
selected tile (gated by `placement_rules::can_place`) with their registry build cost,
affordability-gated against the player corporation's balance; choosing one enqueues a
construction request that the mutable-world pass executes (`construct_building`). This is the
deliberate design choice that **building on one tile is a targeted action reached through the
tile Selection element**, not a reserved menu — the nav-rail construction surface stays a
broad overview (see `docs/ui/MENU.md`, BACKLOG § Ledger). The equivalent placement-mode canvas
click enqueues the same request.

### The tile element reads back its affordances (BL-071)

Above the "Build here" front door, a selected tile carries an **always-on affordance readout**
— the *inverse* of the placement-suitability surface (`LENSES.md`, BL-010). That surface answers
"given an armed building, which tiles?"; this answers "given this tile, which buildings?",
**without arming anything**, so the player can read a tile before committing to a build. It shows
the tile's **territory owner** and a **Thrives / Valid / Invalid** grouping over the
prototype-buildable types (extraction per deposited resource, processing, port), reading the same
`placement_rules` seam the front door and the armed canvas ghost use.

Rejection is **reason-coded, not silent**. `placement_rules::can_place[_in_world]` return a
`placement_result` — a `placement_reason` enum plus human string, implicitly convertible to
`bool` so existing boolean call sites are untouched — so an invalid type shows *why*
(`Cannot build on water`, `No extractable deposit here`, `A port must sit on the coast`, …). The
same reason string enriches the build front door (replacing the former bare "Cannot build on
water") and follows the cursor as a **"why not here"** label under the armed placement ghost on
the Planetary canvas. One vocabulary, three surfaces.

### The building element carries a profitability readout (BL-074)

A selected **Building** carries a **"Profitability (est. / tick)"** section: the estimated net
per-tick contribution of that single building, broken into four component lines plus Net —
**Revenue**, **Inputs** (input cost), **Wages**, **Maintenance**, laid out as two paired columns,
then **Net** (coloured positive/negative/neutral by sign). Figures come from
`estimate_building_profit` (`src/world/building_profit.hpp`) — realised last-tick revenue/cost
where attributable, estimated where the pooled market resists exact per-building attribution.
Before an economy tick has run, the section shows "Run an economy tick to estimate." instead.
Rendered by `draw_building_profit` in `src/ui/selection_panel.cpp`.

### The body element is the survey front door

Beyond its stat block, a selected **Body** carries a **Survey section** (survey system, BL-067)
keyed on the body's survey phase, mirroring the tile build front door. See
[DISCOVERY.md](DISCOVERY.md) for the model authority (the geographic fog this section reads).

- **`hidden`** — a **Dispatch Survey** button with a `cost cr · ETA days` preview (cost and ETA
  derived from size + distance). Affordability-gated against the player corporation's balance;
  disabled with an "Insufficient funds." reason when the player cannot pay.
- **`in_transit`** — `En route — ETA <days> d`.
- **`scanning`** — `Surveying <k>/<N> — ETA <days> d`.
- **`surveyed`** — `Surveyed.`

The button only **enqueues** `ui_state::pending_survey_dispatch`; the mutable-world pass in
`app::render` performs the upfront debit and arms the schedule (`dispatch_survey`), exactly as
construction requests are executed — the UI surfaces hold a `const world&`. The star carries no
survey section.

### Commercial activity

Below the Survey section, a selected **Body** (not the star) carries a **Commercial activity**
section keyed on `body_activity_visibility` (the activity fog, BL-089) — independent of survey
phase, so it can be populated on an unsurveyed body and empty on a surveyed one. See
[DISCOVERY.md](DISCOVERY.md) for the model authority (`activity_vis`, the tier derivation).

- **`unknown`** — "Outside your trade network - no market data." No further content.
- **`known` / `visible`** — a coarse **market pulse** (`busy` / `steady` / `quiet`, derived from
  the body's aggregate market throughput); `visible` additionally notes "Live lane / your
  presence." No per-building production or stockpiles — competitor internals stay private
  (BL-068).
- **`known_stale`** — greyed: "Route gone cold - last market read is stale."

Rendered by `draw_activity_section` in `src/ui/selection_panel.cpp`.

---

## Shared content builders (reuse)

The per-kind stat blocks are the same content the Tile Ledger renders today and
the future **hover card** will render. To avoid three copies, factor each kind's
summary into a reusable builder, e.g.:

```
void draw_body_summary   (const world&, entity_id);
void draw_tile_summary   (const world&, entity_id);
void draw_building_summary(const world&, entity_id);
// …market, unit
```

- The **Selection info element** calls the builder for the selected kind.
- The **Tile Ledger** (`tile_inspector.cpp`) is refactored to call the same
  builders for its per-tile / per-building / per-market rows.
- The **hover card** (deferred) calls them inside its tooltip frame.

These builders lean on the existing presentation layer — `presentation.hpp`
(names, identity colours, semantic palette), `format.hpp` (number/sign
formatting), `icons.hpp` (glyphs). The builder is the appropriate abstraction
seam; the *frame* around it (pinned panel vs. ledger row vs. tooltip) differs per
caller, the *content* does not.

---

## Layout & chrome

- **Pinned**, not floating. It docks in the bottom-left, **above the overlay
  lens / zoom control strip**, anchored to the shell like the nav rail (not a
  draggable ImGui window).
- **Header row:** title line (name + type + icon), a **'go to'** button, and a
  **close** button.
- **Close hides, it does not destroy.** Closing sets the panel hidden; it
  reappears on the next selection. There is no nav-rail entry to reopen it —
  selection is the only opener.
- **Empty / no-selection state:** when nothing is selected (fresh session, or
  after clicking empty space) the panel is hidden. It is shown only while a
  valid selection exists and has not been closed.

---

## Lens-driven hover & selection resolution (settled 2026-06-15, [F4])

A single pointer position overlaps a **stack** of entities — a building sits *on* a tile sits *on*
a body. Both Focus (hover) and Selection (click) resolve that stack to exactly one entity. The
rule has two parts: a fixed stack order, and a lens-evaluated validity filter over it.

**The kind stack (most-specific → least).** At a pointer position the candidate entities are
ordered:

```
building → market → unit  →  tile  →  body
```

(the marker kinds first, then the tile they occupy, then the body that hosts it). "Lowest" means
**most-specific** — earliest in that order.

**Lowest *valid* entity.** Resolution walks the stack from most-specific and returns the **first
entity the active lens deems valid**. Validity is not fixed: it is **the active lens's question**
(§ Per-lens validity in LENSES.md). With **no lens** every drawn kind is valid, so resolution
returns the literal lowest entity (a building over a tile resolves to the building; bare terrain
resolves to the tile). Under a lens, kinds the lens does not care about are *skipped*, so the same
pointer position can resolve to a **different entity per lens**.

**The lens names the ledger the selection drives.** Resolving the entity and choosing its 'go to'
ledger are the *same* decision — the lens that validates the entity also routes it:

| Active lens | Resolves to | 'Go to' / routes to |
|---|---|---|
| **none** (terrain) | the tile (or the lowest drawn marker on it) | Tile Ledger |
| **Corporation** | the owning **corporation** of the hovered tile/building | Balance Ledger |
| **Resource** | the hovered tile's **deposit** | Tile Ledger (deposit detail) |
| **Market** | the body's **market** / listing | Market Ledger |
| **Faction** | the owning **nation** | Nation ledger |
| **Supply** *(Layer 5)* | the **route / stockpile** under the pointer | (Supply surface, when it exists) |

This couples the Selection element's Focus state to the lens system (LENSES.md) and the ledger
family (BACKLOG § Ledger): the 'go to' dispatch seam (`focus_on_entity`) is unchanged — the lens
only changes *which entity id and which target* are handed to it. The same dispatch carries the
non-spatial 'go to' routing (nation/corporation → ledger) already specified above.

**Wiring order.** This is the design rule; it lands once its prerequisites exist — canvas
hit-testing for the marker kinds (§ Open questions below) and the per-lens ledgers (the [A4]
family). Until then the rule is dormant: with no lens and no marker hit-testing, resolution
degrades to today's tile/body behaviour, which is the `none`-lens row above.

## Open questions / deferred

- **Multi-select.** Out of scope; the model is single-selection. A future
  drag-box or shift-click would extend `selected_entity` to a set.
- **Selection persistence across navigation.** A selection survives canvas
  navigation (it is independent of Active). Whether a stale selection (entity
  destroyed) auto-clears is an implementation detail — treat a missing id as
  "no selection".
- **Hit-testing non-body entities on the canvas.** Buildings, units, and markets
  are not yet independently click-selectable on the surface canvas (they appear
  as Tile Ledger rows). Canvas hit-testing for them is its own task; until then
  those kinds are selected only from within ledgers.
