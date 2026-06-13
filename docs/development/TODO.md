# Project Io — TODO

Parked thoughts, recorded but not yet actioned. These are deliberately deferred
while higher-priority work takes precedence. None of them are committed designs —
they are reminders to revisit.

Difficulty scale: **1** trivial · **2** light work · **3** medium · **4** hard ·
**5** very hard · **6** deferred

The pre-Layer-3 **UI building blocks** (the difficulty 1–5 items: presentation
metadata, value/date formatters, the shared highlight convention, the focus-on-
entity helper, render-time interpolation, the canvas overlay layer + mode bar,
and the icon/font-atlas strategy) and the **asteroid belt** ring were implemented
in the 2026-06-13 building-blocks session — see `DEVLOG.md`. The items below are
the follow-up revisions raised against that work, plus the still-deferred
(difficulty 6) work.

---

## Canvas

- **[3] Zoom slider direction + Circumplanetary scale/zoom.** Two parts. (a) The
  Solar zoom slider is **reversed**: dragging right zooms *out* (toward 50 AU) and
  left zooms *in*. Flip it so the **right** end is maximum zoom (zoomed in) and the
  **left** is minimum (zoomed out). (b) The Circumplanetary canvas has **no scale
  bar or zoom slider** — duplicate the Solar canvas's scale/zoom overlay onto it.
  Best done by factoring the scale-bar + slider block out of `solar_system_canvas.cpp`
  into a shared helper both canvases call. See `SOLAR.md` / `CIRCUMPLANETARY.md`.

## Selection & info panel

- **[4] Selection info element (a ledger).** A closable panel (fits the ledger
  category — open/close like the Tile Ledger) docked **above the lens/zoom
  controls**, showing details of the **current selection**. It is **polymorphic
  by selection type**: a tile shows tile data, a body shows body data, a unit
  shows unit data, a building/market each their own — different content per kind
  of selected entity. It carries a **'go to' button next to the close button**
  that focuses the selection (reuse `ui::focus_on_entity`, `src/ui/view_nav.hpp`).
  Pair this with a **navigation change**: **single-click selects** (populates this
  panel) and **double-click navigates** (descends/focuses the rung), where the
  'go to' button has the same effect as a double-click. This revises the canvas
  click handlers (currently a single click descends). Needs its own doc
  (`docs/ui/SELECTION.md`) and an entry in `LAYOUT.md`.

- **[2] Date format & epoch.** The calendar readout shows `Y1 M05 D12`. Switch to
  a **`dd/mmm/yyyy`** form with month abbreviations (e.g. `01/Jan/1960`) and set
  the campaign epoch so **Day 1 falls in year 1960**. The 12 thirty-day months map
  to Jan–Dec. Touches `ui::fmt::date_from_day` / `short_date` and a month-name
  table in `src/ui/format.{hpp,cpp}`, and the readout in `app.cpp`. See
  `TIME_CONTROLS.md`.

- **[2] Highlight resolution on ties.** When more than one entity satisfies the
  same highlight condition at once (e.g. two overlapping hover targets), the
  convention should resolve to a single, **arbitrary-but-stable** choice rather
  than highlighting both or flickering between them. Refines the per-canvas hover
  resolution and `resolve_highlight` (`src/ui/highlight.hpp`).

## Overlays / minimap

- **[3] Relocate the overlay (lens) controls + default lens.** Move the overlay
  mode controls **off the minimap mode bar** to a **bottom-of-screen strip**
  running from the Menu (nav pane) edge inward toward the centre — on the Solar
  view, extending up to the scale bar / zoom control. Also: select a **default
  overlay** on load rather than `overlay_mode::none`. Touches the mode-bar
  handling in `app::render()`, `src/ui/overlay.hpp`, and the `ui_state.overlay`
  default; coordinate with the Solar scale/zoom placement (see the zoom item above)
  and the bottom-left legend chip. See `MINIMAP.md` / `CANVASES.md`.

## Menus

- **[3] Narrower, icon-based Menu.** Restyle the nav pane (Menu) to be **narrower**
  and show **icons instead of worded labels** for each slot, using the vector-glyph
  icon helper (`src/ui/icons.hpp`). Affects `nav_pane.{hpp,cpp}` (`nav_pane_width`)
  and every panel laid out relative to `nav_pane_width`. See `MENU.md` / `LAYOUT.md`.

- **[6] Define the menu items from the systems.** Work out the important menu items
  driven by the game systems (`docs/SYSTEMS.md`), then **get feedback on the
  intended order before final implementation.** See `docs/ui/MENU.md`.

## UI building blocks

- **[6] Informative tooltip / hover-card system.** Deferred. The single most important
  player-communication surface for a grand strategy game. Today there is one
  ad-hoc `ImGui::BeginTooltip` inside the Planetary canvas, plus the lightweight
  `ImGui::SetTooltip` body tooltips on the Solar / Circumplanetary canvases;
  there is no shared rich card. We want a *shared* hover-card primitive with a
  consistent structure — title line (name + type + icon), a short stat block,
  optional sectioned detail, and room for "why" annotations (e.g. how a price or
  yield was derived). It must work for every hoverable thing across all canvases
  and ledgers: bodies, tiles, buildings, markets, and later convoys and routes.
  It can now build on the shared presentation metadata, formatters, and icon
  helpers (`src/ui/presentation.hpp`, `format.hpp`, `icons.hpp`). Decide: a single
  `draw_hover_card(...)` helper vs. per-entity builders; instant vs. delayed
  reveal; how a "rich card" (LAYOUT.md popup elements) differs from the lightweight
  canvas tooltip. Likely earns its own `docs/ui/TOOLTIP.md`. Note the overlap with
  the **Selection info element** above — both present per-entity detail; share the
  per-type content builders where it makes sense. See `docs/ui/LAYOUT.md`.

## Known bugs

- **[4] Body labels move in steps, not smoothly.** Re-logged. The font-oversampling
  pass (`src/ui/fonts.hpp`) improved glyph crispness but did **not** fix the motion
  artefact: body labels visibly advance only every few ticks while the body dot
  glides. The symptom is temporal (stepped position over time), not purely the
  sub-pixel rasterisation originally diagnosed. The label and the dot are drawn
  from the same per-frame `pos`, so the stepping must enter via the text path
  itself (glyph placement quantisation) or the way the label position is read —
  compare the two paths on the Solar / Circumplanetary canvases. See `SOLAR.md`.
