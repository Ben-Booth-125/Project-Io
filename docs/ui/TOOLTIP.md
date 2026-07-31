# Project Io — Hover Card

Authoritative spec for the **shared hover-card primitive** — the transient popup
that shows a compact readout of whatever the player is hovering. It is the
**Focus**-state surface in the three-state pointer model (see
[SELECTION.md](SELECTION.md)): where the Selection info element answers "what is
this thing I clicked?", the hover card answers "what is this thing under my
cursor right now?".

The card is **built** (landed 2026-07-30, BL-228/BL-230): the frame is
`src/ui/hover_card.{hpp,cpp}`, the content dispatch is
`src/ui/hover_content.cpp`, and the host is the Planetary surface
(`body_surface_canvas.cpp`). The reveal model that shipped — **glance, then
stick** — is documented below; the original reveal-timing design it replaced,
and the retired dwell-to-open bar, are preserved under § Superseded.

See also: [LAYOUT.md](LAYOUT.md) (where the card sits in the shell),
[SELECTION.md](SELECTION.md) (the three pointer states and the click model),
[CANVASES.md](CANVASES.md) (the canvases), `src/ui/presentation.hpp`,
`src/ui/format.hpp`, `src/ui/icons.hpp` (the metadata/formatter/glyph helpers).

---

## Current state (what exists)

- **Planetary surface** — the shared hover card, glance-then-stick, serving
  tiles, buildings, and market centres through the lens-keyed
  `draw_hover_content` dispatch (below). This replaced the old open-coded
  `BeginTooltip` block on the hovered tile.
- **Solar / Circumplanetary** (`solar_system_canvas.cpp`,
  `circumplanetary_canvas.cpp`) — still the lightweight ad-hoc `SetTooltip` on
  the hovered body: name, type, orbital radius. Migrating them onto the card is
  owed, not landed.
- **Overlay strip** (`overlay.cpp`) — a one-line `SetTooltip` naming the lens;
  the nav rail's slot tooltips (`nav_pane.cpp`, BL-174) are the same class of
  lightweight chrome tooltip. These are fine as they are — instant identity
  labels on chrome, not entity readouts.

---

## The landed model — glance, then stick (BL-228/BL-230)

Hovering **never opens the Selection band** — opening is the click's job alone,
one gesture, one meaning. This retires BL-200's dwell-to-open and its progress
bar (see § Superseded). What hover does instead has **two phases**, driven by
three constants in `hover_card.hpp` and the hover state in `ui_state.hpp`
(`hover_card_entity` / `hover_card_stuck` / `hover_card_anchor` /
`hover_card_min` / `hover_card_max`, fed by `hovered_entity` + `hover_ticks`):

- **Glance** — after `kHoverAppearDelay` (30 frames ≈ 0.5 s at 60 Hz) of stable
  hover, the card appears and **tracks the live cursor** like an ordinary
  tooltip. Leaving the entity dismisses it.
- **Stick** — after `kHoverStickDelay` (150 frames ≈ 2.5 s total, i.e. 2 s
  after appearing), the card **freezes** at its current position and stops
  following the pointer. It is dismissed only once the cursor leaves the card's
  reported rect inflated by `kHoverCardExitPadPx` (26 px — spans the gap
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
200 px) drawn just above `anchor`, invoking the caller-supplied `content`
closure for its body and reporting the rect it occupied so the caller can
hit-test dismissal next frame. The caller passes the live cursor as `anchor`
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
| **Tile** | Population | header, habitability, workforce cap, the 0.6-cliff why-line (BL-069) |
| **Tile** | *(default)* | header, habitability, and the landform's movement-cost multiplier (`landform_logistics_cost`) when not plains (BL-232) |
| **Building** (player) | any | type, target/recipe line, workforce, an operational why-line (idle / understaffed / active) |
| **Building** (rival) | any | type + owner emblem only — production/stockpile stay private (BL-068) |
| **Market centre** | Market | headline price vs base, supply/demand, a price-signal why-line |
| **Market centre** | *(default)* | market identity + "Switch to Market lens for prices" |

Every tile variant shares the **terrain header** — `composition · landform`,
with plains left unnamed as the baseline — so the on-canvas landform glyph
vocabulary is learnable at the point of looking (BL-231/BL-232, CANVASES.md
§ Terrain channels).

The shipped bands match the designed shape: a title line, one or two stat
lines, then a dimmed **"why" annotation** interpreting the figure — the card's
distinctive value over the click-to-inspect path. The fourth designed band
(sectioned detail) has not been needed by any variant yet.

---

## The Selection band (the click surface)

The click-opened counterpart landed as the **Selection band** (BL-195/BL-213):
`ui::draw_selection_band` (`src/ui/selection_card.{hpp,cpp}`) frames a **fixed
bottom rect** and calls `draw_selection_content` (`selection_panel.cpp`) — the
full per-kind Selection layout, **not** the hover content. The interim
BL-194 form (a click-anchored card at the ledger spawn anchor, showing
`draw_hover_content` as placeholder) is fully superseded; SELECTION.md
§ Layout & chrome is the authority for the band.

Same content family, opposite lifetimes: the band **persists** and carries
actions ('go to', close, drill-down); the hover card is **transient** and
carries none.

---

## Owner's calls — settled in play

The § Superseded design flagged three owner's calls; the landed model settles
them. **Reveal delay:** 0.5 s to appear, 2.5 s to stick (the constants above).
**Rich-by-default:** resolved by the lens-keyed dispatch — depth follows the
active lens, not the entity kind. **"Why" verbosity:** one dimmed line per
variant.

---

## Superseded

### Dwell-to-open bar (BL-200 — RETIRED 2026-07-30 by BL-228/BL-230)

> **Retired, not just superseded.** BL-200's dwell-to-open — holding the
> pointer still filled a thin progress bar at the card's foot, then opened the
> Selection surface on the hovered entity — is removed (commits b04ecfc,
> aea9ab4). Hovering never opens the Selection band; clicking is the only
> opener. The original design, for the record:

**Dwell-to-open bar (BL-200).** The one exception to "no action here" is a thin
progress bar at the foot of the card that fills while the pointer holds **still**,
then opens the sticky detail card on the hovered entity (SELECTION.md § Click
model). It is a *pre-open indicator*, deliberately hosted by this transient
tooltip rather than the opened card's header (Ben, 2026-07-23), and shows only
mid-dwell (strictly between empty and full).

### Reveal-timing design (superseded by glance→stick, 2026-07-30)

> The pre-implementation design proposed **instant reveal for lightweight
> cards, a ~450 ms delay for rich cards**, with depth (lightweight vs rich) a
> per-kind hint on one primitive. What shipped instead is the two-phase
> glance→stick model above: one delay to appear, a second to stick, and depth
> keyed on the **lens** rather than a caller hint. The instant-vs-delayed
> split survives only as chrome tooltips (instant) vs the entity card
> (delayed).

### `selection_kind` dispatch through the shared builders (superseded)

> The design called for `draw_hover_card(const world&, entity_id hovered)` to
> classify by `selection_kind_of` and dispatch to the `entity_summary.hpp`
> builders (`draw_body_summary`, `draw_tile_summary`, …) — the same content
> source as the then-stat-block Selection element, serving a seven-kind table
> (body, tile, building, market, resource pip, unit/convoy, route/lane) across
> every canvas and ledger. Two things moved under it: the Selection element
> **stopped being a stat block** (BL-093 — SELECTION.md § Removed), and the
> card that shipped keys content on the **lens**, which the builders do not
> see. The landed dispatch is `draw_hover_content` (above); the
> `entity_summary` builders remain the Tile Ledger's content, and an uncalled
> tooltip wrapper of the same name survives in `entity_summary.{hpp,cpp}`.
> Body hover (Solar/Circumplanetary) and the later unit/route kinds remain the
> designed-for extension when those canvases migrate onto the card.

---

## Prototype / deferred notes

- Solar / Circumplanetary body hover still uses ad-hoc `SetTooltip`; migrating
  it onto the card (and the unit/convoy/route kinds, Layer 5) is owed.
- The card has no spec-level styling beyond `palette` and the shell's ImGui
  theme; the production shell's retained-mode equivalent is a later concern.
- Text-wrap behaviour inside the 200 px card is within scope of the open
  BL-215 (text-wrap render audit) — see LAYOUT.md § Container vocabulary.
