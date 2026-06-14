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
`docs/development/TODO.md`, and `src/ui/view_nav.hpp` (`focus_on_entity`, the
'go to' target).

---

## The three interaction states

This element forces us to name three distinct, previously-conflated pointer
states. They are independent: an entity can be any combination of active,
focused, and selected at once.

| State | Meaning | Lifetime | Drives | Backing |
|---|---|---|---|---|
| **Active** | The navigation **anchor** — which body/tile the canvas rungs are framed around. | Persists until you navigate. | Which Circumplanetary/Planetary rung renders, and around what. | `ui_state.active_body`, `ui_state.active_tile` (existing). |
| **Focus** | The entity **under the pointer** right now. | Transient, per-frame. | The hover tooltip / hover card. | Per-frame `hovered_*` locals today (not stored); a future `focus` field when the hover card lands. |
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
| **Body** (planet/moon/asteroid/station/star) | Name, type, orbit, parent; surface summary. | Canvas: `focus_on_body`. |
| **Tile** | Composition × landform, hazard, habitability, deposits. | Canvas: `focus_on_tile`. |
| **Building** | Type, recipe, throughput, host tile. | Canvas: `focus_on_tile` (host tile). |
| **Market** | Body, headline prices / balances. | Canvas: `focus_on_surface`; or Market ledger. |
| **Unit / logistics vessel** | Type, owner, location, status. | Canvas: `focus_on_surface` / vessel's position. |
| **Nation** | Name, character, territory summary. | A ledger (no canvas of its own). |
| **Corporation** | Name, parent nation, headline standing. | A ledger (no canvas of its own). |

So 'go to' is itself polymorphic: spatial entities navigate a canvas;
non-spatial entities (nation, corporation) open the relevant ledger. For the
prototype the spatial kinds (body, tile, building) are wired first; the rest are
designed here and stubbed.

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
