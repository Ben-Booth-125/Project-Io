# Project Io — UI Layout

Surface-level description of the application **shell** — the persistent chrome arranged around the canvases. This document covers *where things sit and what they are for*, not their internals. Each region below links to its own detailed specification where one exists. For the canvases themselves see **`CANVASES.md`**; for the tick model see `src/core/sim_loop.hpp`.

The prototype UI is built with Dear ImGui (see TECH_FOUNDATIONS). Everything here is a debugging-grade layout that doubles as the functional specification for the eventual production shell — it is expected to be revised.

---

## Screen regions

```
┌──────────────────┬─────────────────────────────┬──────────────┐
│ Profile          │           Header            │  Time panel  │
│ (identity tile)  │   (budget + resource strip) │ (date + bar, │
├─────┬────────────┤ — one top band, level tiles │  speed ctl)  │
│ Nav │ Fold-out   ├─────────────────────────────┴──────────────┤
│ rail│ column:    │                                            │
│ ▢   │ ledger     │            Primary canvas                  │
│ ▢   │            │   (Solar / Circumplanetary / Surface)      │
│ ▢ … │ (stops at  │                                            │
│ ▦   │ the comms  │                             ┌──────────────┤
│ ▢ … │ dock's top │                             │   Minimap    │
│ ▢   │ edge)      │                             │  [lens bar]  │
│     ├────────────┴──────┬──────────────────────┤              │
│     │ COMMS DOCK        │ SELECTION BAND (fixed│              │
│     │ (channel chat)    │ action/facts or tile)│              │
└─────┴───────────────────┴──────────────────────┴──────────────┘
```

The nav rail runs the **full screen height**. The screen's **bottom strip** (BL-213/
BL-227) is one solid band, all three tiles sharing a top edge that lands exactly on
the minimap's: the **comms dock** bottom-left (`comms_dock_rect`, three quarters of
the fold-out column's width), the **Selection band** from the dock's right edge to
the right chrome column, and the **minimap** in the corner. The fold-out column
stops at the dock's top edge, so every ledger is permanently shorter by the strip's
height — see *Selection band* and *Comms dock* below.

Two layers compose the screen:

- **Background** — the canvases, drawn edge-to-edge via the ImGui background draw list.
- **Foreground** — the ImGui panels below (profile, header, nav pane, comms log, time column, minimap, ledger windows), drawn on top of the canvases.

The comms log (BL-205, 2026-07-26) first occupied the right-middle band the Explorer
placeholder reserved; BL-227 (comms dock bottom-left, 2026-07-30) moved it into the
bottom strip. Layer 2 shipped the nav pane, canvases, time column, minimap, and the
Tile Ledger; the header (Layer 3 finalisation) and profile (`src/ui/profile_panel.cpp`)
have since landed.

---

## The shell column (BL-122)

Since **BL-122** the left edge is a single **permanent shell column** of width
`W = clamp(round(0.20 · display_width), 380, 460)` px (`ui::shell_column_width`,
`src/ui/foldout_column.hpp`) — runtime-computed from the display so it stays legible
across resolutions rather than a magic constant. BL-122 originally **widened** the column
~1.6× (to `[480, 576]`) so it could host the Selection element as a sidebar alongside the
ledgers; **BL-213 (2026-07-28) narrowed it back down** to `[380, 460]` once Selection moved
out to its own fixed bottom band (see *Selection band* below) and no longer needed room in
this column at all — the freed width goes to the band instead. It now resolves to ~380 px at
1720 wide, ~410 px at 1920. The column is reserved down the whole left edge:

- The **identity tile** (profile) caps it at top, taking the full width `W`.
- The narrow **icon nav rail** (56 px) runs down its left sub-edge, full height.
- A **fold-out ledger** fills the rest of the column (`[nav_pane_width, W]`, below the
  identity tile) when a nav slot is active — see *Ledger windows*. Since BL-227 (comms
  dock bottom-left) the column **stops at the comms dock's top edge**
  (`foldout_column_rect`), so every ledger is permanently shorter by the bottom strip's
  height.
- The **balance bar** (header) starts at `x = W` permanently, whether or not any fold-out is
  open, so it always clears the column.

> **Superseded (BL-213, 2026-07-28).** The Selection element no longer shares the fold-out
> slot as a sidebar, mutually exclusive with the ledgers — that was the BL-124 shape. It
> lives in the fixed bottom band (*Selection band* below), and selection leaves an open
> ledger untouched.

The 380 px floor keeps the one-question-per-view panel splits (BL-117..121) load-bearing.
The per-panel splits and the Tile Ledger's migration into the column are **not** part of
BL-122 — the skeleton hosts each panel's current content.

---

## Profile — top-left (the identity tile)
**Spec: `PROFILE.md`**

The **identity tile**: a panel pinned to the top-left corner that caps the shell column,
taking its full width `W` (BL-122; formerly a fixed 200 px box above the rail). Shows the
player corporation at a glance:

- **Corporation emblem** — a geometric emblem (deterministic shape + identity colour) on a portrait plate.
- **Corporation name**, plus `Parent: <home nation>` and `Focus: <industrial focus>`, read live from `corporation_component`.

This is a static identity readout in the prototype — no interaction beyond, eventually, opening a fuller corporation screen. Implemented in `src/ui/profile_panel.cpp`; see `PROFILE.md`.

---

## Header — top
**Spec: `HEADER.md`**

A strip across the top of the canvas area, between the identity tile and the time column. Its left edge is the shell column's right edge (`x = W`, BL-122). It now stands the **full identity-tile height** (`profile_panel_height`, ~92 px) and top-aligns at `y = 0`, so the header and the identity tile read as **one level top band**; its content row is vertically centred within that taller strip. It is the player's persistent financial dashboard, wired to the live economy as of the Layer 3 finalisation:

- **Balance** — the player corporation's running treasury balance (negatives flagged red).
- **Stockpile valuation** — an estimated liquid value of everything the player holds: its `(corporation, body)` pools summed at each body's current market price. A single money figure, not a per-resource inventory.
- **Net + trend** — the last economy tick's net change as a coloured per-quarter figure, alongside a small sparkline of recent balances.

The header answers "can I afford this, and which way is it trending?" without opening a ledger. Detailed, per-body breakdowns stay in their respective ledgers.

---

## Navigation pane — left
**Spec: `MENU.md`**

A fixed, full-height **icon rail** pinned to the left edge (`nav_pane_width`, currently 56 px), below the profile. Cannot move, resize, or collapse. The rail is narrow; the profile keeps its own (wider) `profile_panel_width` above it rather than matching the rail.

- Holds a vertical strip of **nine square icon slots** (BL-174 dropped the glyph-less tenth) — the home for the game's menus and ledgers. **Every slot carries its own vector glyph** (`src/ui/icons.hpp`) — live slots in the bright stroke, reserved slots dimmed — plus a wrapping name-and-blurb tooltip (BL-174, nav-rail legibility).
- Each slot toggles a panel open/closed; the open slot lights its glyph in the selection accent. An active slot **folds its ledger out into the shell column** to the rail's right (`[nav_pane_width, W]`, BL-122) rather than spawning a floating window; opening one collapses whichever was open (accordion, via `close_all_panels`).
- **Five slots are live** (`nav_pane.cpp`): Corporation overview, Budget, Market Ledger, Construction, History. **Four are reserved**, disabled with dimmed glyphs: Workforce, Research, Corp. Strategy, Diplomacy. Full slot semantics in `MENU.md`.

---

## Canvas area — centre
**Spec: `CANVASES.md`**

Three canvases — Solar, Circumplanetary, Planetary — form a **zoom ladder** and share the window. One is **primary** (fills the window) and the rung one step *out* from it is shown in the **minimap** (a fixed inset, bottom-right). The player **descends** (zooms in) by clicking a body in the primary canvas and **ascends** (zooms out) by clicking the minimap. Full detail — visual language, coordinate mapping, interaction, and the ladder navigation rules — lives in **`CANVASES.md`**.

The canvases render behind the foreground panels, so the chrome currently occludes the edges of the primary canvas. Insetting the primary region clear of the chrome is a known follow-up.

**The region has a name: `ui::canvas_rect()`** (`src/ui/foldout_column.{hpp,cpp}`) — bounded by the shell column on the left, the header strip on top and the bottom strip below, with the minimap floating inside it rather than reserving a column. It exists because BL-265 needed something for a full-canvas takeover to *take*, and three ad-hoc derivations of the same region already existed. Anything that occupies the main stage without being a canvas — a takeover, the tech-tree viewer — is sized to this rect. Migrating the zoom-ladder canvases themselves onto it is a follow-on; `CANVASES.md` owns them.

---

## Minimap — bottom-right inset
**Spec: `MINIMAP.md`**

A fixed inset in the bottom-right corner showing the **zoom-out neighbour** of the primary canvas at reduced scale, framed by its own chrome — a title bar (the viewed body, or the star name; the game name at the top rung) above the inset. It was **enlarged ~1.4×** — its size is now `max(336, 0.28 · min(w, h))` px (was `max(240, 0.20 · min(w, h))`), keeping the same 4:3 ratio; the time panel scales with it, sharing the right-column width. Clicking it **ascends** one rung. It shares the primary canvases' drawing code, parameterised by region size. Since BL-093 the minimap also carries the **lens mode bar** along its bottom edge (see below); `MINIMAP.md` is the authoritative spec for the minimap chrome, the lens bar, and the ladder navigation, and `CANVASES.md` covers the shared drawing path.

---

## Lens mode bar — on the minimap
**Spec: `MINIMAP.md` / `LENSES.md`**

The overlay-lens controls no longer occupy their own bottom-left strip — BL-093
relocated them onto the **minimap itself**, as an **eight-glyph** mode bar running
along its bottom edge (`ui::draw_overlay_controls`, called from the minimap block
in `src/core/app.cpp`). The on-screen eight are Corp, Country, Resource, Market,
Population, Opportunity, Production, and — since BL-226 (Continent lens) —
Continent; Scarcity, Industry, Reach, and Supply-routes join Supply as
**keyboard-cycle only** lenses (no bar glyph — the bar does not have room for
all thirteen). Clicking a glyph toggles that lens; the active one is highlighted,
and clicking it again clears the overlay. The lens-local resource/good picker
(Resource, Market, Scarcity) moved off the bar into the **on-canvas lens legend**
(BL-134, lens selector in legend).

The read-only **lens legend** (the on-canvas key for the graded lenses — Resource,
Market, Production, Opportunity, Population, Scarcity, Industry) is a separate element
from this control bar. It now sits **flush-left of the minimap** — its right edge at the
minimap's left edge, vertically centred on the minimap, reading as a drawer folding out
from the minimap's left side (passed a `lens_key_anchor` from `app.cpp`). It was moved
here from the canvas left edge, which also clears the now-widened shell column it would
otherwise have overlapped. `LENSES.md` is the authoritative spec.

## Selection band — fixed bottom band
**Spec: `SELECTION.md`**

The Selection info element lives in a **fixed band at the bottom of the screen**
(BL-213, 2026-07-28) — the middle tile of the bottom strip, between the **comms dock**
on its left (BL-227; the band starts at the dock's right edge) and the **minimap** on
its right. Its height is `selection_band_height` (`src/ui/foldout_column.{hpp,cpp}`),
**derived from the minimap height** plus `chrome_margin` so the whole strip's top edge
lands on the minimap's (2026-07-30 — replaced a flat 340 px that overhung the minimap).
This is the third shape the element has had: the original
BL-065 full-width bottom bar, then the BL-124 shell-column sidebar (mutually exclusive
with the ledgers), then this fixed band, which **retires click-anchoring** (the
BL-194/195 "sticky card" that froze at the click position) in favour of always
occupying the same rect. Ben's call: *"doing/building" menus need a fixed place at the
bottom of the screen, not a widget that floats with the cursor.* It shows detail about
the **current selection**, whatever entity the player last single-clicked, and — unlike
every earlier shape — **does not close an open ledger**: the two no longer compete for
the same space at all.

It is an **action surface**, not a stat block: a header row
(`[kind icon] Name · type` on the left, **go-to** `>` and **close** `x` buttons
on the right) over the kind's content. Per selection kind: a **tile** uses the
purpose-built vertical layout (hex neighbourhood render + per-resource production
chart + action icons, § SELECTION.md); other kinds (body, building, market,
nation/corporation) use an action|facts split — a **body** offers *Dispatch Survey* or
*Go to surface* beside its commercial-activity pulse; a **player-owned building**
offers a *Manage building* button that routes into the construction panel beside its
profitability readout; a **rival building** is intel-only — owner and location facts,
production/stockpile shown as explicit private rows.

The **go-to** button is equivalent to a double-click on the selection (routes
through `ui::focus_on_entity` — navigates a canvas for spatial entities, opens a
ledger for non-spatial ones); **close** hides the band until the next
selection. Unlike the fold-out ledgers it has **no nav-rail slot** — being
selection-driven, **selecting an entity is the only way to open it.** It carries
the click-model shared across all canvases: **single-click selects** (fills this
band, no view change), **double-click navigates**. See `SELECTION.md` and
`CANVASES.md`.

## Time panel — top-right
**Spec: `TIME_CONTROLS.md`**

A single panel in the top-right corner, the same width as the minimap so the right edge stays aligned. It is split into **two columns** (25% / 75%): a compact calendar block on the left and the speed controls on the right.

**Calendar block** (left column, 25%) — the player-facing clock in three rows:

- **Year + quarter** — `1960 Q1` (see `TIME_CONTROLS.md`).
- **Month + day** — `Jan 01`, abbreviated month + zero-padded day.
- **Quarter progress** — a progress bar through the current in-year quarter, labelled with its percentage.

**Speed controls** (right column, 75%) — a raw `Sim` counter with the current multiplier, then the controls:

- A **pause button** — a drawn filled square while running (deliberately not "||", which read as the Roman numeral II beside the tiers), flipping to a play "**>**" while paused — then speed tiers **I–V** in Roman numerals. The active speed is highlighted; each tier names its real rate on hover (BL-178).

The clock has three layers — sim tick → day → economy tick — paced so that 1× ≈ 6 s/day and 3× ≈ 2 s/day. The economy resolves quarterly. The authoritative calendar/pacing constants live in `src/core/sim_loop.hpp` and are deliberately tentative.

---

## Comms dock — bottom-left
**Spec: `CHAT.md`**

The channel-based **comms chat log** (BL-205): the Public channel plus arbitrary
player-created corp groups, fed by deterministic sim events (the BL-079 agency reflexes
today) and player messages (`src/ui/chat_panel.cpp`).

Since **BL-227 (comms dock bottom-left, 2026-07-30)** it docks in the **bottom-left tile
of the bottom strip** (`comms_dock_rect`, `src/ui/foldout_column.{hpp,cpp}`): the fold-out
column's x-range, three quarters of the column's width, sharing the Selection band's top
edge and height so the two read as one bar. Comms is ambient chatter (BL-212), not a
decision surface, so it gave up the prime right-edge slot under the time panel; the
quarter-width it gives back goes to the Selection band. The icon rail keeps its full
height past the dock — at the 1280×720 floor a shortened rail would clip two slots.

It replaced the Explorer placeholder (2026-07-26) in its original right-middle home —
pinning was never wired; **BL-216 (chat pinning)** is the open item for its return as a
chat-adjacent affordance, not a reserved band. The diplomacy-as-communication principle
behind the surface: `docs/ai/AI_OPPONENT.md` § 7.

---

## Ledger windows — all fold-out (BL-122)

Since **BL-122** no ledger floats. Every ledger — Construction, Economy, Market, Balance,
Corporations, the tile construction ledger (BL-162), **and the Tile Ledger** (History,
slot 9 — `tile_inspector.cpp` opens through the same path; its migration into the column
is done, not deferred) — draws as a **pinned, borderless panel filling the shell column**
(`ui::foldout_begin`/`foldout_end`, `src/ui/foldout_column.hpp`) when its nav slot is
active, and folds back to just the icon rail when toggled off or when another opens
(accordion). There is no title-bar close and no **✕** — closing is the nav-rail toggle.
The old uniform floating chrome (`ledger_window_spawn`/`size`, `ImGuiCond_Once` movable
windows) and the Construction panel's BL-082 height-cap apply to nothing any more. The
set of menus is described in **`MENU.md`**.

**All ledgers start closed** on a fresh session — none are shown until the player opens them from the pane (see the policy in `MENU.md`).

Only **broad** ledgers (overviews across many entities) earn a nav-rail slot; targeted, per-entity actions are reached contextually through the Selection info element or a popup, not the rail (see `MENU.md` § Menus are broad ledgers).

### Superseded: uniform floating ledger-window chrome

> **Superseded — no floating ledger remains (2026-07-31).** The settled principle below
> governed the floating era: every ledger window shared **one size and one spawn anchor**
> (`ledger_window_size` / `ledger_window_spawn`, `src/ui/ledger_chrome.hpp`,
> `ImGuiCond_Once`) so the family read as one consistent surface. BL-122 folded the five
> named ledgers into the shell column; the Tile Ledger — the last floater — has since
> docked too (`tile_inspector.cpp`, `foldout_begin`). The `ledger_chrome.hpp` constants
> still exist but have **zero callers**; the uniform chrome the family now shares is the
> fold-out column itself. The **BL-082 Construction-panel height-cap is dissolved** — a
> fold-out ledger is confined to the shell column and needs no caller-supplied spawn.

### One-question-per-view splits (BL-117 sweep)

Fold-out ledgers with more than one question split their content across a **button-strip nav**
(`ui::nav_button`, `foldout_column.hpp` — a manual `Selectable`/`Button` strip, since
`ImGui::BeginTabBar` does not render in this build), each view drawing exclusively. The current
splits: the **Construction** panel — **Construction / Buildings** (BL-143, building ledger
redesign; the Build front door moved to the tile Selection element and Sell Orders moved out;
defaults to Buildings, BL-176); the **Market Ledger** — **Prices / Sell Orders** (BL-159,
sell orders on the market surface); the **Economy** panel — Corps / Holdings / Markets
(`ui_state::economy_view`); the **History** ledger — Story / Chain / Tiles (BL-211). The
**Balance** and **Corporation** ledgers remain single-question — no split. The principle is
*one question per view, a menu to move between views* — not a mandate to split every panel.

**Toggle rule on the strip (BL-126).** Consistent with the universal toggle rule
(`.claude/rules/io-standing-rules.md`): re-clicking the **currently-active** sub-view tab **closes
the hosting ledger** (clears its `show_*` flag) — matching the nav-rail icon exactly — rather than
being a no-op. Switching to a *different* tab is an ordinary view change. `ui::nav_button` takes the
ledger's open-flag as an optional `close` target; a strip with no flag passed stays a plain,
non-closing selector.

### Economy-panel table legibility (BL-081, BL-111, BL-117)

The Corporations dashboard (`corporation_panel.cpp`) and the economy tables were retuned for the
narrow shell column: in ~244px a `SizingStretchProp` multi-column table collapses every column to a
leading glyph, so the **identity column stretches and the numeric columns take tight fixed widths**
(the BL-081 pattern), and low-value columns are dropped rather than clipped (the corp dashboard shows
Corporation / Focus / Balance; Home Nation and Status moved to the Selection panel / row tint).
Original BL-081 note follows.



The Economy panel's tables use a **stretch name/resource column + fixed-width numeric columns**
(not `SizingStretchProp`, which collapsed cells to a leading glyph — the balances column read
`9 8 1 1…`). The **Corporation balances**, **Workforce**, **Stockpile pools**, and **Markets**
tables all follow this pattern, so corp/body/resource identity and the full numeric values read at
a glance. The former **per-building table was removed** from this panel — per-building profitability
is the Corp Dashboard's job (BL-074), and duplicating it here was the redundancy that produced the
finding.

### Ledger family conventions

All ledger windows share four standing conventions, stated once here so each new ledger
need not rediscover them:

- **Uniform chrome = the fold-out column.** Every ledger draws through
  `foldout_begin`/`foldout_end`, pinned to `foldout_column_rect` — one rect, one border
  policy, no per-ledger sizing. (The floating-era `ledger_window_size`/`spawn` constants
  are superseded — see above.)
- **Player corporation defaulted.** Every ledger that shows per-corporation data defaults
  to `w.player_entity` in its corp selector and offers a selector to view any other
  corporation's figures. Cross-corporation side-by-side comparison is not in scope for
  the prototype.
- **Shared content builders.** Per-entity stat blocks are rendered through the shared
  `entity_summary` helpers (`src/ui/entity_summary.{hpp,cpp}`) — today that means the
  Tile Ledger. The Selection element stopped calling them (BL-093 — SELECTION.md
  § Removed) and the shipped hover card carries its own lens-keyed content
  (`hover_content.cpp`, TOOLTIP.md); the rule stands for any new ledger stat block —
  do not duplicate the logic, call the builders.
- **Start closed.** All ledgers open with their initial `show_*` flag `false`
  (`src/ui/ui_state.hpp` defaults; toggled in `src/ui/nav_pane.cpp`). None are shown
  until the player explicitly opens them from the nav pane.

---

## Container vocabulary (BL-141)

A closed set of **nine** container kinds recur across the shell and canvases. Each
combines a sizing rule with exactly one text policy — **wrap** (`PushTextWrapPos` +
`TextWrapped`, reflowing to the box) or **guaranteed-fit** (`CalcTextSize` measured first,
the box sized to the text — the pattern the market-legend key established) — plus an
overflow rule for when content still exceeds the box. New UI work should land in one of
these nine rather than inventing a tenth.

| # | Container | Sizing | Text policy | Overflow |
|---|---|---|---|---|
| 1 | **Fold-out ledger column** (`foldout_begin`, `foldout_column.cpp`) | Fixed-width (`[nav_pane_width, W]`), stretches from below the identity tile to the **comms dock's top edge** (BL-227 — no longer the full column height) | Wrap to inner width | Vertical scroll |
| 2 | **On-canvas lens legend box** (draw-list, `body_surface_canvas.cpp`) | Fit-to-content — box sized from its own entries | Guaranteed-fit | None — box grows to fit |
| 3 | **Selection band** (`draw_selection_band`, `selection_card.cpp` framing `draw_selection_content`, `selection_panel.cpp`) | Fixed rect — comms dock's right edge to the right chrome column, `selection_band_height` tall (minimap-derived) | Wrap | Vertical scroll |
| 4 | **Header / balance strip** (`header_panel.cpp`) | Stretch-to-width, fixed height | Guaranteed-fit — segments measured, never wraps | Elide-with-tooltip, only as a last resort; never silent truncation |
| 5 | **Time panel** (`app.cpp`) | Fixed width (shares the minimap's), content-derived height (BL-097) | Guaranteed-fit — authored to fit | None |
| 6 | **Hover card** (`draw_hover_card`, `hover_card.cpp`) | Fit-to-content, capped at 200 px max width, auto height | Wrap at the max width | Grows vertically |
| 7 | **Minimap lens bar** | Fixed strip | Icon-only / guaranteed-fit labels | None |
| 8 | **Nav rail** | Fixed (56 px) | Icon-only; tooltips wrap (`nav_pane.cpp`, BL-174 — `PushTextWrapPos`, implementing this row's stated policy) | None (tooltip wrap absorbs it) |
| 9 | **ImGui table** (ledgers) | Stretch columns with per-column min widths | Per-cell guaranteed-fit (clip + tooltip) for numeric/identity columns, wrap for description columns | Horizontal scroll on the table; never silent truncation of a load-bearing value |

The **comms dock** (BL-227) is not a tenth kind: it is a fixed-rect docked panel obeying
container 1's policy (wrap, vertical scroll) in the bottom strip.

This table is the baseline the open **BL-215 (text-wrap render audit)** consumes — an
audit of every container's *rendered* wrap behaviour against the policy declared here —
so the rows above describe the real containers, re-derived from the code 2026-07-31.
(BL-140, text/image containment, and BL-141, this vocabulary, both landed; BL-215 is the
open follow-through.)

A few cross-cutting notes:

- **Wrap vs guaranteed-fit is a binary choice per container, not per instance.** A
  container that measures text to size itself (2, 4, 5, 7, 9-numeric) never also wraps
  that same text; a container that wraps (1, 3, 6, 8-tooltip, 9-description) never
  pre-measures to a fixed box.
- **Silent truncation is never acceptable** for a load-bearing figure (balance, price,
  quantity). The only sanctioned degradations are elide-with-tooltip (4) and clip-with-tooltip
  (9) — both keep the value one hover away.
- These are prototype-tuned concrete behaviours (BL-122's `foldout_column`, the market
  legend's measure-then-size pattern), not a general layout engine — see
  `docs/tech/TECH_FOUNDATIONS.md` for why a retained-mode framework is out of scope.

---

## Drill-through (BL-214, revised by BL-265)

**One disclosure idiom, obeyed by every dense surface.** Before this, each surface had
invented its own way of showing more — a collapsing header in the History Chain, a pager
in the Selection band, a permanent horizontal scroll in the Tiles table, nothing at all in
the wizard. Drill-through is the single idiom that replaces them, and it has **three**
states reached by **two** controls:

| State | What it is | Control |
|---|---|---|
| **Folded** | A verdict line — a figure, a label, a glyph. No sentences. | — (the resting state) |
| **Expanded in place** | The row grows where it sits: one graph, or an accordion of graphs for a more complex menu. The rest of the screen keeps working. | `⌄` (becomes `⌃`, and collapses on click) |
| **Full canvas** | A takeover of the **canvas region** showing everything the surface has at once — every item of an accordion, open, top to bottom, scrolled. | `›` (leave with `‹`, or Esc) |

BL-214 shipped this as a **binary** model on Ben's 2026-07-31 supersession of the
three-level Glance / Read / Study stepper: folded, or a full-screen overlay, nothing
between. **BL-265 restores the middle rung** (Ben, 2026-08-02, after using it) — a
full-screen takeover turned out to be too big a step for *"I want to see this one chart
properly"*. What is **not** restored is the stepper's single cycling control: these are two
distinct affordances with two distinct meanings, which is what makes them legible where one
cycling control was not.

### Three axes, never conflated

The four competing idioms were never four answers to one question — they were three
different questions wearing one costume. Naming the three is most of the fix:

| Axis | Question | Gesture | State |
|---|---|---|---|
| **Depth** | *How much of this subject do I want?* | `⌄` in place, `›` full canvas | `ui_state::expanded` (the takeover target **and** the in-place set) |
| **Subject** (BL-196) | *What am I looking at?* | click the element; breadcrumb + back | `ui_state::card_stack`, `corp_rollup_drill` |
| **Host** (SELECTION.md) | *Where does this properly live?* | `[>]` go-to | `ui::focus_on_entity` |

BL-196 is therefore the **sibling axis**, not a competitor and not folded in.

### The geometry: a takeover takes the CANVAS, not the window

`fold_overlay_begin` is sized to **`ui::canvas_rect()`**, not to the display (BL-265 change
2, Ben: *"full screen should actually just take the place of a canvas, rather than the
entire view. I would prefer to see consistency in the UI really."*). The nav rail, identity
tile, header, clock, comms dock, Selection band and minimap all survive a takeover, and the
player never loses their selection to open a chart.

The argument is consistency and it is the good one: the canvas region is already the app's
main stage, and the zoom ladder already swaps what occupies it. A ledger going full canvas
is therefore the **same kind of event as descending the ladder**, using the same rectangle,
rather than a second full-window mode with its own rules.

**One exception, and it is real rather than a special case:** the New World wizard's chain
stages (`generation_stage`). The wizard runs on its own screen *before* the shell exists, so
there is no canvas to take and no chrome to preserve; its takeover stays window-wide. That
is what "take the main stage" means on that screen. `takeover_rect` in
`src/ui/detail_level.cpp` is the single place this is decided.

### The invariants

1. **One TAKEOVER at a time — and only the takeover is single-target.** BL-214 derived "one
   thing expanded at a time" from expanded being a window-wide overlay. Both halves of that
   derivation moved, so BL-265 restates it as two rules rather than one:
   - The **takeover** stays a single `(surface, key)` target (`fold_state::surface`/`key`),
     for the original reason: it owns one rectangle, so a second has nowhere to go. Opening
     a second replaces the first.
   - **In-place** expansion is **not** single-target — it is a **set**
     (`fold_state::in_place`). An accordion of graphs is by definition several rows open at
     once; a single-target in-place expander would close the first graph when you opened the
     second, which is the opposite of what an accordion is.
2. **Inside a takeover the in-place set is ignored.** Every item renders open, top to
   bottom, in a scroll region — *"anything full screen deserves its full space"* only holds
   if the takeover does not depend on which rows the player happened to unfold first.
   Nothing writes the set on the takeover's behalf, so returning via `‹` restores the folded
   view **exactly**: the takeover is non-destructive.
3. **A takeover and the fold-out ledger column coexist by design, not by tolerance.**
   `foldout_column_rect()` is entirely left of `canvas_rect()`, so the row that opened a
   takeover **stays visible** in the left column while the canvas changes. That reads
   correctly for Ben's own stated reason — it is the same event as clicking a body on the
   canvas: the source stays put, the stage changes. Do not "fix" it by dimming the ledger.
4. **The level is not remembered.** A takeover is a transient mode and the in-place set is a
   reading preference — both are view state, never serialised.
5. **A fixed-rect container does not fold to one line, and has no `⌄`.** The Selection band's
   rect is a *derived* 260 px (`selection_band_height`) that cannot shrink, so folding its
   metric card would spend ~220 px on emptiness. Fixed-rect surfaces therefore rest
   **expanded in place already** (Ben, 2026-08-01, asked with the measurements) — there is no
   in-place state left to reach, so they take the `›` control alone. The same holds for a
   single-block view that already shows its content in the column (History Story, History
   Tiles). Folded-by-default governs **scrolling** containers, where a fold buys real room
   back.
6. **Drill-through adds no tenth container.** The nine kinds above say *how text fits*; this
   says *how much of it there is*. The takeover is container **1**'s policy (wrap, vertical
   scroll) at canvas size.

### The controls, and where they sit

**The rule, in one line: controls that operate on an item in a list are right-gutter-aligned;
the control that leaves a view is top-left.** Any future disclosure surface is checked
against that sentence.

A **foldable row**:

```
Expandable item          verdict                       ⌄  ›
```

Both controls sit in a **fixed right gutter** (`ui::disclosure_controls`,
`ui::disclosure_gutter_width`) — not immediately after the label, because a gutter is what
makes them line up in one vertical column across every row. **A title's length must never
move its controls**, which is what Ben reported: *"it is disorienting when a button is not
in the same column, but does the same job."* Before BL-265 the placement was split —
`generation_charts.cpp` and `corporation_dashboard.cpp` drew the chevron *left* of the
label, `tile_inspector.cpp` and `selection_panel.cpp` pushed it to the right edge. All four
are now the same column. A row that offers only `›` leaves the gutter's **left slot empty**
rather than letting `›` slide sideways, so the full-canvas column never moves. A row's
verdict is drawn through `ui::gutter_text`, which clips it short of the gutter so a long
verdict cannot run underneath the controls.

`⌄` **is** a toggle — re-clicking while open collapses — so the standing toggle rule applies
with no exemption owed.

The **full-canvas view**:

```
‹ Example title
  <everything, scrolled>
```

One control, **top left**, immediately before the title: `‹`, which returns. Nothing sits in
a right gutter here — there is no list of rows to align with, and a return affordance
belongs where a reader starts, not where they finish. The two placements answer two
different questions: in a list the controls act *on a row*; in a takeover the control acts
on the *whole view* and means "back", and back is top-left everywhere in software, including
this app's own zoom ladder.

**All four glyphs are DRAWN, never typed.** `⌄ ⌃ ‹ ›` are notation for the design; none of
those codepoints are in the font atlas, and BL-234 fixed a defect of exactly this shape (26
strings rendering `?`). `draw_caret` and `draw_open_arrow` in `src/ui/detail_level.cpp` build
them through `ImDrawList`. Do not reintroduce them as string literals.

**Esc** closes the takeover, one rung **below** the subject drills: exit-confirm → system
menu → pop `card_stack` → pop `corp_rollup_drill` → **close the takeover** → open menu. A
single press never both unwinds a drill and closes the view hosting it. Esc **does not**
collapse in-place expansions: that is a reading preference, the kind of state Esc has never
collapsed, and putting it on the ladder would make Esc unpredictable (sometimes leaving a
view, sometimes re-folding four graphs you deliberately opened). *(The takeover rung itself
departs from BL-214's Decision 10, which kept depth off the ladder — that decision reasoned
about an in-place stepper; a takeover with no keyboard exit is a defect, not a principle.)*

The takeover's **entry/exit transition** is deliberately unsettled and currently instant —
a feel question that wants the live app, not a paragraph.

### The chart question log (BL-247) — *removed 2026-08-02*

**This surface does not exist, and is not to be rebuilt.** The design was a closed-by-default
"Why this chart" toggle (`ui::why_note`) revealing an *Answers:* / *Because:* pair on any chart.
Ben removed it under **NR-018**; the draw path is gone and `src/ui/detail_level.cpp:121` carries
the standing note — *"REMOVED 2026-08-02 (Ben, NR-018) … Do not reinstate a draw path here
without reopening NR-018."* There are no `why_note` symbols left in `src/`.

The **derivation caption** — which answers how a number was computed — is a different thing and
was never part of this item. It stands.

### The surfaces, and the extension recipe

On the ladder today, with what each one's takeover shows:

| Surface | `⌄` in place | `›` full canvas |
|---|---|---|
| Selection band's metric card | — (rests expanded, invariant 5) | that metric's chart, on the canvas |
| History ledger — Story | — (already shown in the column) | the whole biography, on the canvas |
| History ledger — Tiles | — (already shown in the column) | buildings + the market table, on the canvas |
| History ledger — Chain (per stage) | that stage's title, explainer and charts | the **whole round** — every stage open, scrolled |
| New World wizard — chain stages | that stage's title, explainer and charts | the **whole round**, scrolled (window-wide; no shell yet) |
| Corporation dashboard — 4 roll-ups | that card's chart or rows | **all four** roll-ups, headed and scrolled |

The three accordions — History Chain, wizard stages, corp roll-ups — are the ones BL-265
change 3 names: a surface that has taken the largest rectangle available has no excuse for
showing a subset.

Adding surface #N is **two edits**: an enumerator in `detail_surface`, and a
`disclosure_controls` + `fold_overlay_begin` pair at the call site. Nothing else — no new
control, no new state, no new container kind. A surface with many foldable blocks needs
**one** enumerator, not one per block: the `key` disambiguates instances (a chain stage, a
roll-up card).

---

## UI popup elements

Beyond the persistent chrome, the production UI will use **transient popup elements** — context menus, confirmation dialogs, hover cards, and action prompts that appear in response to a click or hover and dismiss on action or click-away. Examples: right-clicking a unit for an order menu, a "confirm purchase" dialog.

The **hover card is built** (BL-228/BL-230, landed 2026-07-30): the glance-then-stick card on the Planetary surface, framed by `draw_hover_card` (`src/ui/hover_card.cpp`) with lens-keyed content from `hover_content.cpp` — [`TOOLTIP.md`](TOOLTIP.md) is its spec. The system menu (BL-070) and its inline exit-confirm are the popup dialogs that exist. Context menus and richer confirmation dialogs remain unbuilt; they are noted here so the layout accounts for content that floats above every region without belonging to any one of them.

---

## Developer affordances

- **F12** captures the exact composited frame to `build/Debug/screenshots/`. The `tools/capture.ps1` wrapper builds, launches, captures, and converts to PNG in one step.

---

## Prototype / temporary notes

- The comms log (BL-205) is live in the bottom-left dock (BL-227); profile and header are live.
- Nav slot layout is settled — nine slots, the MENU.md curated order, per-slot glyphs (BL-174).
- Canvases are not yet inset clear of the chrome.
- Context menus and confirmation dialogs (beyond the system menu's exit-confirm) are deferred; the hover card is built (TOOLTIP.md).
- ImGui is the prototype UI only; the production shell is a later, Lua-driven retained layer (TECH_FOUNDATIONS).

---

## Companion specifications

| Region | Document |
|---|---|
| Entry screens (menu / wizard / handoff) | `STARTUP.md` |
| Profile | `PROFILE.md` |
| Header | `HEADER.md` |
| Navigation pane / menus | `MENU.md` |
| Canvases (overview / ladder) | `CANVASES.md` |
| Solar canvas | `SOLAR.md` |
| Circumplanetary canvas | `CIRCUMPLANETARY.md` |
| Planetary canvas | `PLANETARY.md` |
| Minimap | `MINIMAP.md` |
| Lenses (overlay modes) | `LENSES.md` |
| Icon vocabulary | `ICONS.md` |
| Selection band | `SELECTION.md` |
| Hover card | `TOOLTIP.md` |
| Discovery / fog surfaces | `DISCOVERY.md` |
| Time column | `TIME_CONTROLS.md` |
| Comms dock | `CHAT.md` |
