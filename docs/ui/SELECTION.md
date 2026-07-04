# Project Io — Selection Info Element

The **Selection info element** is a pinned **action surface** that answers one
question about the **current selection** — whatever entity the player last
single-clicked: **"what's my move here?"** It is the opposite of a ledger: a
ledger is exhaustive reference reached deliberately from the rail; this is a
decision prompt that appears when you click. One primary **action** per
selection kind, dominant; a slim line of decision-relevant **facts**; a
**'go to'** for the deep ledger where reference detail actually lives.

It is **not** reachable from the navigation rail — the only way to open it is
to select something — and it is **not** the stat-block encyclopedia it once
was (see § Removed: the stat-block polymorphism below); full per-entity detail
lives in the Tile Ledger, Market Ledger, Balance Ledger, etc., one 'go to' away.

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

## Action + Facts — content by selection type (BL-093)

The panel is **polymorphic by selection kind**, but no longer as a stat block.
The entity's kind is resolved by probing the `world` maps (the same
discrimination `focus_on_entity` already does: `w.tiles`, `w.buildings`,
`w.units`, `w.markets`, `w.bodies`, and later nations / corporations /
logistics vessels), and each kind renders through the same two-column split:

- **Action** (left, dominant, ~58% of the content width) — the ONE primary
  move for this kind, via `draw_selection_action` (`src/ui/selection_panel.cpp`).
- **Facts** (right, muted) — only what informs that action, via
  `draw_selection_facts`. Everything encyclopedic (orbit, composition,
  deposits, prices) has moved to the ledgers.

Each kind routes its 'go to' to the right place:

| Selection kind | Action (hero) | Facts (muted) | 'Go to' target |
|---|---|---|---|
| **Tile** | **Build here** — the buildable-types front door + cost, affordability-gated (`draw_build_front_door`). | **Thrives / Valid / Invalid** affordance readout (BL-071) — territory owner + which building types suit this tile and why not. | **No-op for now.** A tile is selected from the surface it lives on, so 'go to' has nothing to descend to; pan-to-tile is out of scope. |
| **Body** (planet/moon/asteroid/station/star) | Unsurveyed → **Dispatch Survey** (BL-067). Surveyed → **Go to surface** (descends via `focus_on_entity`). The star carries neither. | **Commercial activity** pulse (activity fog, BL-089) — see below. | Canvas: `focus_on_surface` — descend to the body's **Planetary tile surface**, the most informative rung. (Not `focus_on_body` / the orbital framing: landing on a sparse circumplanetary view reads as "nothing happened", which was the *only-works-for-Kepler* symptom.) |
| **Building** (player-owned) | **Manage ▸** — routes to the building-management surface (`construction_panel.cpp`, which already owns the workforce slider / recipe / decommission controls). The panel does not duplicate those controls. | **Profitability readout** (BL-074) — Net/tick, the one decision fact — see below. | Canvas: `focus_on_tile` (host tile). |
| **Building** (rival) | None — **"Competitor building - intel only."** | Owner name + explicit `private` rows for production/stockpile (BL-068). | Canvas: `focus_on_tile` (host tile). |
| **Market / Unit** | **Go to** — locate on the canvas. | (none yet; stubbed) | Canvas: `focus_on_surface` / entity's position. |
| **Nation / Corporation** | None — **"Open its ledger via [>]."** | (none yet; stubbed) | A ledger (no canvas of its own). |

So 'go to' is itself polymorphic: spatial entities navigate a canvas;
non-spatial entities (nation, corporation) open the relevant ledger. For the
prototype the spatial kinds (body, tile, building) are wired first; the rest are
designed here and stubbed.

### The tile element's action is the build front door

The **Tile** selection's hero action is **"Build here"** — the player's primary construction
entry point (`draw_build_front_door`). It lists the building types placeable on the selected
tile (gated by `placement_rules::can_place`) with their **full construction cost — budget *and*
materials** (e.g. `100 cr · 20 Steel`, from the registry `build_cost` + `resource_build_cost`,
BL-044). A type is affordability-gated on **both**: the player corporation's balance *and* its
material pool on that body (`corp_body_pools`) — `construct_building` returns
`insufficient_materials` when the resources are absent, so the requirement is surfaced up front
rather than only on a failed click. Choosing an affordable type enqueues a construction request
that the mutable-world pass executes (`construct_building`). This is the deliberate design choice that
**building on one tile is a targeted action reached through the tile Selection element**, not a
reserved menu — the nav-rail construction surface stays a broad overview (see `docs/ui/MENU.md`,
BACKLOG § Ledger). The equivalent placement-mode canvas click enqueues the same request.

### The tile element's facts are its affordance readout (BL-071)

In the Facts column, a selected tile carries an **always-on affordance readout**
(`draw_tile_affordances`) — the *inverse* of the placement-suitability surface (`LENSES.md`,
BL-010). That surface answers "given an armed building, which tiles?"; this answers "given this
tile, which buildings?", **without arming anything**, so the player can read a tile before
committing to a build — the decision fact the Build action needs. It shows the tile's
**territory owner** and a **Thrives / Valid / Invalid** grouping over the prototype-buildable
types (extraction per deposited resource, processing, port), reading the same `placement_rules`
seam the front door and the armed canvas ghost use.

Rejection is **reason-coded, not silent**. `placement_rules::can_place[_in_world]` return a
`placement_result` — a `placement_reason` enum plus human string, implicitly convertible to
`bool` so existing boolean call sites are untouched — so an invalid type shows *why*
(`Cannot build on water`, `No extractable deposit here`, `A port must sit on the coast`, …). The
same reason string enriches the build front door (replacing the former bare "Cannot build on
water") and follows the cursor as a **"why not here"** label under the armed placement ghost on
the Planetary canvas. One vocabulary, three surfaces.

### The building element's fact is a profitability readout (BL-074)

A selected player-owned **Building**'s Facts column carries a **"Profitability (est. / tick)"**
section (`draw_building_profit`) — the one decision fact for the Manage ▸ action: the estimated
net per-tick contribution of that single building, broken into four component lines plus Net —
**Revenue**, **Inputs** (input cost), **Wages**, **Maintenance**, laid out as two paired columns,
then **Net** (coloured positive/negative/neutral by sign). Figures come from
`estimate_building_profit` (`src/world/building_profit.hpp`) — realised last-tick revenue/cost
where attributable, estimated where the pooled market resists exact per-building attribution.
Before an economy tick has run, the section shows "Run an economy tick to estimate." instead.

### The body element's action is the survey front door (then go-to-surface)

An unsurveyed **Body**'s hero action is its **Survey section** (survey system, BL-067),
keyed on the body's survey phase (`draw_survey_section`, called from `draw_selection_action`).
See [DISCOVERY.md](DISCOVERY.md) for the model authority (the geographic fog this section reads).

- **`hidden`** — a **Dispatch Survey** button with a `cost cr · ETA days` preview (cost and ETA
  derived from size + distance). Affordability-gated against the player corporation's balance;
  disabled with an "Insufficient funds." reason when the player cannot pay.
- **`in_transit`** — `En route — ETA <days> d`.
- **`scanning`** — `Surveying <k>/<N> — ETA <days> d`.
- **`surveyed`** — the hero action switches to **"Go to surface"**, which descends via
  `focus_on_entity` — surveying is done, the move now is to look.

The Dispatch Survey button only **enqueues** `ui_state::pending_survey_dispatch`; the
mutable-world pass in `app::render` performs the upfront debit and arms the schedule
(`dispatch_survey`), exactly as construction requests are executed — the UI surfaces hold a
`const world&`. The star carries neither the survey action nor the go-to-surface action.

### The body element's fact is its commercial activity pulse (BL-089)

A selected **Body**'s (not the star's) Facts column carries a **Commercial activity**
section (`draw_activity_section`) keyed on `body_activity_visibility` (the activity fog,
BL-089) — independent of survey phase, so it can be populated on an unsurveyed body and empty
on a surveyed one. See [DISCOVERY.md](DISCOVERY.md) for the model authority (`activity_vis`,
the tier derivation).

- **`unknown`** — "Outside your trade network - no market data." No further content.
- **`known` / `visible`** — a coarse **market pulse** (`busy` / `steady` / `quiet`, derived from
  the body's aggregate market throughput); `visible` additionally notes "Live lane / your
  presence." No per-building production or stockpiles — competitor internals stay private
  (BL-068).
- **`known_stale`** — greyed: "Route gone cold - last market read is stale."

---

## Removed: the stat-block polymorphism (BL-093)

The panel's earlier form (pre-2026-07-04) rendered a **50/50 split** — an encyclopedic per-kind
stat block (`draw_summary`, dispatching to `draw_body_summary` / `draw_tile_summary` /
`draw_building_summary` / … in `entity_summary.hpp`) beside a **lens supplement**
(`draw_lens_supplement`) that re-rendered overlay-keyed market price / production rate /
population-workforce rows inline — literally duplicating the Market / Production / Population
ledgers. Both **are removed**: `draw_summary` and `draw_lens_supplement` no longer exist in
`src/ui/selection_panel.cpp`. Facts and actions carried equal weight and everything read as
uniform flat text, so the panel had no clear reason to exist — it read as "an amalgamation of
all the menu ledgers" (Ben's playtest finding, 2026-07-04). All that reference detail — orbit,
parent, composition, deposits, prices — now lives in the ledgers, one `go to` away; the header
identity line (name · type) is the only trace of it left in the panel itself.

The shared per-entity content builders in `entity_summary.hpp` (`draw_body_summary`,
`draw_tile_summary`, `draw_building_summary`, …) are unaffected by this removal — they remain
the Tile Ledger's content and the future hover card's planned content; only the Selection
element stopped calling them.

---

## Layout & chrome

- **Pinned**, not floating. It docks in the bottom-left and now **owns the whole bottom-left
  corner** (BL-093): anchored from the nav-rail's right edge to the minimap's left edge (less a
  margin), and from the very bottom margin upward. The **lens strip no longer sits beneath
  it** — it relocated onto the minimap (see § Lens strip relocation below), which is what freed
  the vertical room the action/facts split needed.
- **Sized to content, in text-line units (BL-093 fix).** `draw_selection_panel` computes a
  `content_rows` count per selection kind (tile 9, body 5, player building 6, rival building 3,
  default 4) and derives the window height from `ImGui::GetTextLineHeightWithSpacing()` /
  `GetFrameHeight()` rather than pinning to the minimap's pixel height. This is deliberately
  **resolution-robust**: a fixed-pixel height read fine at the resolution it was tuned on but
  ballooned with empty space or clipped content at another — see
  [DEVELOPMENT_PRACTICES.md § Display environment](../development/DEVELOPMENT_PRACTICES.md) for
  the general rule this follows. The window still clamps to `[min_h, screen_h]` so it never
  overruns the display or shrinks below three content lines.
- **Header row:** a small coloured **kind icon** (`draw_selection_icon` — circle for body,
  square for building, outlined square for tile, pentagon otherwise; a first pass ahead of a
  richer per-entity icon), then the title line (name · type), then a right-aligned **`[>]`**
  ('go to') button and **`[x]`** (close) button.
- **Close hides, it does not destroy.** Closing sets the panel hidden; it
  reappears on the next selection. There is no nav-rail entry to reopen it —
  selection is the only opener.
- **Empty / no-selection state:** when nothing is selected (fresh session, or
  after clicking empty space) the panel is hidden. It is shown only while a
  valid selection exists and has not been closed.

### Lens strip relocation (BL-093)

The overlay-lens control strip (`ui::draw_overlay_controls`, `src/ui/overlay.cpp`) no longer
lives beneath the Selection element — it moved onto the **minimap**, reprising its pre-BL-013
location. A lens mode bar now occupies the bottom row of the minimap box (chrome fill drawn in
`app.cpp`'s minimap block; the interactive glyph row is `draw_overlay_controls(ui, x, top_y, w)`
positioned over it), leaving the Selection element the full height it needs. The strip itself
also trimmed from 9 lenses to a single row of **7**: Corporation, Country, Resource, Market,
Population, Opportunity, Production. **Scarcity** and **Industry** dropped off the on-screen row
(joining **Supply**) and are keyboard-cycle only — 9 glyphs did not fit the 240px minimap width.
The Resource/Market/Scarcity resource-selector, formerly a 140px inline combo, is now a compact
popup button opened from the bar (`draw_lens_selector`). Full detail: [LENSES.md](LENSES.md),
[MINIMAP.md](MINIMAP.md).

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
