# Project Io — Hover Card

Authoritative spec for the **shared hover-card primitive** — the transient popup
that shows a compact readout of whatever the player is hovering. It is the
**Focus**-state surface in the three-state pointer model (see
[SELECTION.md](SELECTION.md)): where the Selection info element answers "what is
this thing I clicked?", the hover card answers "what is this thing under my
cursor right now?".

The frame is `src/ui/hover_card.{hpp,cpp}`, the content dispatch is
`src/ui/hover_content.cpp`, and the host is the Planetary surface
(`body_surface_canvas.cpp`). The reveal model is **glance, then stick**
(BL-228/BL-230, hover card). The design it replaced — instant-vs-delayed reveal
and the dwell-to-open bar — is preserved under § Superseded, with the reasons.

See also: [LAYOUT.md](LAYOUT.md) (where the card sits in the shell),
[SELECTION.md](SELECTION.md) (the three pointer states and the click model),
[CANVASES.md](CANVASES.md) (the canvases), `src/ui/presentation.hpp`,
`src/ui/format.hpp`, `src/ui/icons.hpp` (the metadata/formatter/glyph helpers).

---

## Where hover readouts live

- **Planetary surface** — the shared hover card, glance-then-stick, serving
  tiles, buildings, and market centres through the lens-keyed
  `draw_hover_content` dispatch (below).
- **Solar / Circumplanetary** (`solar_system_canvas.cpp`,
  `circumplanetary_canvas.cpp`) — a lightweight `SetTooltip` on the hovered
  body: name, type, orbital radius. Body hover migrates onto the card when those
  canvases carry entity readouts worth a why-line; the unit/convoy/route kinds
  join it then.
- **Overlay strip** (`overlay.cpp`) — a one-line `SetTooltip` naming the lens;
  the nav rail's slot tooltips (`nav_pane.cpp`) are the same class of
  lightweight chrome tooltip. Instant identity labels on chrome, not entity
  readouts — the instant-vs-delayed split survives exactly here.

---

## The model — glance, then stick

Hovering **never opens the Selection band** — opening is the click's job alone,
one gesture, one meaning. What hover does instead has **two phases**, driven by
three constants in `hover_card.hpp` and the hover state in `ui_state.hpp`
(`hover_card_entity` / `hover_card_stuck` / `hover_card_anchor` /
`hover_card_min` / `hover_card_max`, fed by `hovered_entity` + `hover_ticks`):

- **Glance** — after `kHoverAppearDelaySec` (0.5 s) of stable hover, the card
  appears and **tracks the live cursor** like an ordinary tooltip. Leaving the
  entity dismisses it.
- **Stick** — after `kHoverStickDelaySec` (2.5 s total from the same start,
  i.e. 2 s after appearing), the card **freezes** at its current position and
  stops following the pointer. It is dismissed only once the cursor leaves the
  card's reported rect inflated by `kHoverCardExitPadPx` (26 px — spans the gap
  between the anchor and the card drawn above it), so a long line can be read
  to its end without the card sliding away.

**Z-order is constant across both phases.** The window carries
`ImGuiWindowFlags_NoInputs` plus the `Tooltip` flag: it draws above every other
window but never captures the pointer, so canvas hover/click always resolves to
the tile or marker beneath it. A new card is never summoned while one is up, or
while construction placement mode is active (the ghost owns that moment).

---

## The frame — `draw_hover_card`

`draw_hover_card(ImVec2 anchor, content, ImVec2* out_min, ImVec2* out_max)`
(`hover_card.hpp`) is an **anchor + content frame, nothing more**: a
semi-opaque, chrome-free ImGui window (no title bar, 4 px rounding, max width
200 px — `kMaxWidth`) drawn just above `anchor`, invoking the caller-supplied
`content` closure for its body and reporting the rect it occupied so the caller
can hit-test dismissal next frame. The caller passes the live cursor as `anchor`
while glancing and the frozen position once stuck.

The card has **no chrome** — no title bar, no close button, no 'go to'. Any
action belongs to the click that follows, not the hover.

## The content — `draw_hover_content` (lens-keyed)

Dispatch lives in `src/ui/hover_content.cpp`, not in the frame. It resolves the
hovered entity by probing the world maps (tile / building / market) and then
keys the **variant on the active lens**, so the card answers the question the
lens is asking:

| Entity | Lens | Content |
|---|---|---|
| **Tile** | Resource | composition · landform header, the selected resource's deposit richness, a richness-band why-line |
| **Tile** | Population | header, habitability, workforce cap, the 0.6-cliff why-line |
| **Tile** | *(default)* | header, habitability, and the landform's movement-cost multiplier (`landform_logistics_cost`) when not plains |
| **Building** (player) | any | type, target/recipe line, workforce, an operational why-line (idle / understaffed / active) |
| **Building** (rival) | any | type + owner emblem only — production/stockpile stay private (the competitor-visibility rule, DISCOVERY.md) |
| **Market centre** | Market | headline price vs base, supply/demand, a price-signal why-line |
| **Market centre** | *(default)* | market identity + "Switch to Market lens for prices" |

Every tile variant shares the **terrain header** — `composition · landform`,
with plains left unnamed as the baseline — so the on-canvas landform glyph
vocabulary is learnable at the point of looking (CANVASES.md § Terrain
channels).

Each variant is a title line, one or two stat lines, then a dimmed **"why"
annotation** interpreting the figure — the card's distinctive value over the
click-to-inspect path. A fourth band (sectioned detail) is available in the
shape but no variant needs it.

---

## The Selection band (the click surface)

The click-opened counterpart is the **Selection band** (BL-195/BL-213, selection
band): `ui::draw_selection_band` (`src/ui/selection_card.{hpp,cpp}`) frames a
**fixed bottom rect** and calls `draw_selection_content` (`selection_panel.cpp`)
— the full per-kind Selection layout, **not** the hover content. SELECTION.md
§ Layout & chrome is the authority for the band.

Same content family, opposite lifetimes: the band **persists** and carries
actions ('go to', close, drill-down); the hover card is **transient** and
carries none.

---

## Owner's calls

**Reveal delay:** 0.5 s to appear, 2.5 s to stick (the constants above).
**Rich-by-default:** resolved by the lens-keyed dispatch — depth follows the
active lens, not the entity kind. **"Why" verbosity:** one dimmed line per
variant.

---

## Superseded

### Dwell-to-open bar

BL-200's dwell-to-open — holding the pointer still filled a thin progress bar at
the card's foot, then opened the Selection surface on the hovered entity — is
**retired**, not merely superseded. Hovering never opens the Selection band;
clicking is the only opener. The original design, for the record: a
*pre-open indicator* hosted by this transient tooltip rather than the opened
card's header (Ben, 2026-07-23), shown only mid-dwell (strictly between empty
and full).

### Reveal-timing design

The pre-implementation design proposed **instant reveal for lightweight cards,
a ~450 ms delay for rich cards**, with depth (lightweight vs rich) a per-kind
hint on one primitive. The two-phase glance→stick model above replaces it: one
delay to appear, a second to stick, and depth keyed on the **lens** rather than
a caller hint. The instant-vs-delayed split survives only as chrome tooltips
(instant) vs the entity card (delayed).

### `selection_kind` dispatch through the shared builders

The original design called for `draw_hover_card(const world&, entity_id hovered)`
to classify by `selection_kind_of` and dispatch to the `entity_summary.hpp`
builders (`draw_body_summary`, `draw_tile_summary`, …) — the same content source
as the then-stat-block Selection element, serving a seven-kind table (body,
tile, building, market, resource pip, unit/convoy, route/lane) across every
canvas and ledger. Two things moved under it: the Selection element **stopped
being a stat block** (SELECTION.md § Removed), and the card keys content on the
**lens**, which the builders do not see. The dispatch is `draw_hover_content`
(above); the `entity_summary` builders remain the Tile Ledger's content.

---

## Notes

- The card has no spec-level styling beyond `palette` and the shell's ImGui
  theme; the production shell's retained-mode equivalent is a later concern.
- Text-wrap behaviour inside the 200 px card is within scope of BL-215
  (text-wrap render audit) — see LAYOUT.md § Container vocabulary.
