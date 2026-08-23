# Project Io — UI Layout

Surface-level description of the application **shell** — the persistent chrome arranged around the canvases. This document covers *where things sit and what they are for*, not their internals. Each region below links to its own detailed specification where one exists. For the canvases themselves see **`CANVASES.md`**; for the tick model see `src/core/sim_loop.hpp`.

The prototype UI is built with Dear ImGui (see TECH_FOUNDATIONS). Everything here is a debugging-grade layout that doubles as the functional specification for the eventual production shell.

---

## Screen regions

```
┌──────────────────┬─────────────────────────────┬──────────────┐
│ Profile          │           Header            │  Time panel  │
│ (identity tile)  │   (budget + resource strip) │ (date + bar, │
├─────┬────────────┤ — one top band, level tiles │  speed ctl)  │
│ Nav │ Fold-out   ├─────────────────────────────┼──────────────┤
│ rail│ column:    │                             │ PINNED ITEMS │
│ ▢   │ ledger     │       Primary canvas        │ (watch list) │
│ ▢   │            │ (Solar / Circumpl./Surface) │              │
│ ▢ … │ (stops at  │                             │ [lens legend]│
│ ▦   │ the comms  │                             ├──────────────┤
│ ▢ … │ dock's top │                             │   Minimap    │
│ ▢   │ edge)      │                             │  [lens bar]  │
│     ├────────────┴──────┬──────────────────────┤              │
│     │ COMMS DOCK        │ SELECTION BAND (fixed│              │
│     │ (channel chat)    │ action/facts or tile)│              │
└─────┴───────────────────┴──────────────────────┴──────────────┘
```

The nav rail runs the **full screen height**. The screen's **bottom strip** is one
solid band, all three tiles sharing a top edge that lands exactly on the minimap's:
the **comms dock** bottom-left (`comms_dock_rect`, three quarters of the fold-out
column's width), the **Selection band** from the dock's right edge to the right
chrome column, and the **minimap** in the corner. The fold-out column stops at the
dock's top edge, so every ledger is shorter by the strip's height — see *Selection
band* and *Comms dock* below.

The **right chrome column** splits two ways and elastically: the content-derived
**time panel** on top, the ratio-locked **minimap** at the foot, and the whole
residual middle to **pinned items** — the space a count-driven lens legend also
drops into when opened (LENSES.md § Legend placement). The minimap does not move.

### One owner for the rect algebra

The regions above are not each other's business, but their **edges** are. The right
chrome column's left edge is needed by the minimap, the time panel, the system gear,
the header's right bound and the Selection band's right edge; derived by hand in
five places it drifts.

**`src/ui/shell_metrics.{hpp,cpp}`** owns that algebra (BL-216, shell re-plan), and
every call site asks it for a rect: `right_chrome_width` / `right_chrome_left`,
`minimap_rect`, `time_panel_rect`, `bottom_band_budget`, `selection_band_rect`,
`pinned_panel_rect`. It **consumes** `foldout_column.hpp`'s primitives
(`shell_column_width`, `minimap_width`/`_height`, `selection_band_height`,
`foldout_column_rect`, `comms_dock_rect`) rather than duplicating them — the split
is primitives there, composed rects here. Widths round to whole pixels, so every
region edge lands on a pixel boundary.

> One inconsistency is **recorded, not silently fixed**: the minimap is flush to the
> screen edge (`disp.x - mm_w`) while the column's other occupants stop a
> `shell_margin` short of it (`right_chrome_left`). `minimap_rect` and
> `right_chrome_left` therefore disagree by 8 px, deliberately and in one visible
> place, instead of five invisible ones.

Two layers compose the screen:

- **Background** — the canvases, drawn edge-to-edge via the ImGui background draw list.
- **Foreground** — the ImGui panels below (profile, header, nav pane, comms dock, time column, minimap, ledger windows), drawn on top of the canvases.

---

## The shell column

The left edge is a single **permanent shell column** of width
`W = clamp(round(0.20 · display_width), 380, 460)` px (`ui::shell_column_width`,
`src/ui/foldout_column.hpp`) — runtime-computed from the display so it stays legible
across resolutions rather than a magic constant. It resolves to ~380 px at 1720 wide,
384 px at 1920, ~410 px at 2048 — the 380 floor binds across almost the whole common
range. The column is reserved down the whole left edge:

- The **identity tile** (profile) caps it at top, taking the full width `W`.
- The narrow **icon nav rail** (56 px) runs down its left sub-edge, full height.
- A **fold-out ledger** fills the rest of the column (`[nav_pane_width, W]`, below the
  identity tile) when a nav slot is active — see *Ledger windows*. The column **stops at
  the comms dock's top edge** (`foldout_column_rect`), so every ledger is shorter by the
  bottom strip's height.
- The **balance bar** (header) starts at `x = W` permanently, whether or not any fold-out is
  open, so it always clears the column.

The Selection element does not share this column: it lives in the fixed bottom band
(*Selection band* below), and selection leaves an open ledger untouched. The width the
column would otherwise need to host a Selection sidebar belongs to the band instead.

The 380 px floor keeps the one-question-per-view panel splits load-bearing.

---

## Profile — top-left (the identity tile)
**Spec: `PROFILE.md`**

The **identity tile**: a panel pinned to the top-left corner that caps the shell column,
taking its full width `W`. Shows the player corporation at a glance:

- **Corporation emblem** — a geometric emblem (deterministic shape + identity colour) on a portrait plate.
- **Corporation name**, plus `Parent: <home nation>` and `Focus: <industrial focus>`, read live from `corporation_component`.

This is a static identity readout in the prototype — no interaction beyond, eventually, opening a fuller corporation screen. Implemented in `src/ui/profile_panel.cpp`; see `PROFILE.md`.

---

## Header — top
**Spec: `HEADER.md`**

A strip across the top of the canvas area, between the identity tile and the time column. Its left edge is the shell column's right edge (`x = W`). It stands the **full identity-tile height** (`profile_panel_height`, ~92 px) and top-aligns at `y = 0`, so the header and the identity tile read as **one level top band**; its content row is vertically centred within that strip. It is the player's persistent financial dashboard, wired to the live economy:

- **Balance** — the player corporation's running treasury balance (negatives flagged red).
- **Stockpile valuation** — an estimated liquid value of everything the player holds: its `(corporation, body)` pools summed at each body's current market price. A single money figure, not a per-resource inventory.
- **Net + trend** — the last economy tick's net change as a coloured per-quarter figure, alongside a small sparkline of recent balances.

The header answers "can I afford this, and which way is it trending?" without opening a ledger. Detailed, per-body breakdowns stay in their respective ledgers.

---

## Navigation pane — left
**Spec: `MENU.md`**

A fixed, full-height **icon rail** pinned to the left edge (`nav_pane_width`, 56 px), below the profile. Cannot move, resize, or collapse. The rail is narrow; the profile keeps its own (wider) `profile_panel_width` above it rather than matching the rail.

- Holds a vertical strip of **nine square icon slots** — the home for the game's menus and ledgers. **Every slot carries its own vector glyph** (`src/ui/icons.hpp`) — live slots in the bright stroke, reserved slots dimmed — plus a wrapping name-and-blurb tooltip.
- Each slot toggles a panel open/closed; the open slot lights its glyph in the selection accent. An active slot **folds its ledger out into the shell column** to the rail's right (`[nav_pane_width, W]`) rather than spawning a floating window; opening one collapses whichever was open (accordion, via `close_all_panels`).
- **Eight slots are live** (`nav_pane.cpp`): five carry their own subject — Corporation overview, Budget, Market Ledger, Construction, History — and three carry a **provisional occupant**, a subject whose slot hosts a surface that would otherwise have no door: Workforce hosts the Economy panel, Research the tech-tree viewer, Diplomacy the corporations table (NR-012). **One is reserved**, disabled with a dimmed glyph: Corp. Strategy. Full slot semantics in `MENU.md`.

---

## Canvas area — centre
**Spec: `CANVASES.md`**

Three canvases — Solar, Circumplanetary, Planetary — form a **zoom ladder** and share the window. One is **primary** (fills the window) and the rung one step *out* from it is shown in the **minimap** (a fixed inset, bottom-right). The player **descends** (zooms in) by double-clicking a body in the primary canvas and **ascends** (zooms out) by clicking the minimap. Full detail — visual language, coordinate mapping, interaction, and the ladder navigation rules — lives in **`CANVASES.md`**.

The canvases render behind the foreground panels, so the chrome occludes the edges of the primary canvas.

**The region has a name: `ui::canvas_rect()`** (`src/ui/foldout_column.{hpp,cpp}`) — bounded by the shell column on the left, the header strip on top and the bottom strip below, with the minimap floating inside it rather than reserving a column. Anything that occupies the main stage without being a canvas — a takeover, the tech-tree viewer — is sized to this rect. `CANVASES.md` owns the zoom-ladder canvases themselves.

---

## Minimap — bottom-right inset
**Spec: `MINIMAP.md`**

A fixed inset in the bottom-right corner showing the **zoom-out neighbour** of the primary canvas at reduced scale, framed by its own chrome — a title bar (the viewed body, or the star name; the game name at the top rung) above the inset. Its size is `max(336, 0.28 · min(w, h))` px at a 4:3 ratio; the time panel scales with it, sharing the right-column width. Clicking it **ascends** one rung. It shares the primary canvases' drawing code, parameterised by region size. The minimap also carries the **lens mode bar** along its bottom edge (see below); `MINIMAP.md` is the authoritative spec for the minimap chrome, the lens bar, and the ladder navigation, and `CANVASES.md` covers the shared drawing path.

---

## Lens mode bar — on the minimap
**Spec: `MINIMAP.md` / `LENSES.md`**

The overlay-lens controls sit on the **minimap itself**, as an **eight-glyph** mode bar
running along its bottom edge (`ui::draw_overlay_controls`, called from the minimap
block in `src/core/app.cpp`). The on-screen eight are Corp, Country, Resource, Market,
Population, Opportunity, Production and Continent; Scarcity, Industry, Reach,
Supply-routes and Supply are **keyboard-cycle only** lenses (no bar glyph — the bar does
not have room for all thirteen). Clicking a glyph toggles that lens; the active one is
highlighted, and clicking it again clears the overlay. The lens-local resource/good
picker (Resource, Market, Scarcity) lives in the **lens legend** (BL-134, lens selector
in legend), not on the bar.

The read-only **lens legend** is a separate element from this control bar, and has two
homes (LENSES.md § Legend placement): the **gradient-bar keys** (Resource, Production,
Opportunity, Population, Scarcity, Industry, Continent) sit **flush-left of the minimap**
— right edge at the minimap's left edge, vertically centred on it, reading as a drawer
folding out from the minimap's left side (a `lens_key_anchor` from `app.cpp`); the
**count-driven keys** (Country, Market, Reach, Supply-routes) live in the **right chrome
column above the minimap** as a dropdown collapsed by default, so a long list never
spills over the map or the bottom panels (Ben, 2026-08-22). `LENSES.md` is the
authoritative spec.

## Selection band — fixed bottom band
**Spec: `SELECTION.md`**

The Selection info element lives in a **fixed band at the bottom of the screen** — the
middle tile of the bottom strip, between the **comms dock** on its left (the band starts
at the dock's right edge) and the **minimap** on its right. Its height is
`selection_band_height` (`src/ui/foldout_column.{hpp,cpp}`), **derived from the minimap
height** plus `chrome_margin` so the whole strip's top edge lands on the minimap's. The
band always occupies the same rect — it is never click-anchored. Ben's call: *"doing/
building" menus need a fixed place at the bottom of the screen, not a widget that floats
with the cursor.* It shows detail about the **current selection**, whatever entity the
player last single-clicked, and **does not close an open ledger**: the two do not
compete for the same space.

It is an **action surface**, not a stat block: a header row (`[kind icon] Name · type` on
the left, a **go-to** `>` button on the right; no close button — the band is always open)
over the kind's content. Per selection kind: a **tile**, a **building**, a **unit**, a
**province** and a **battle** each use the purpose-built three-column band layout (left
image, centre accordion, right 2×3 action grid — § SELECTION.md); the remaining kinds
(body, market, nation/corporation) use an action|facts split — a **body** offers *Dispatch
Survey* or *Go to surface* beside its commercial-activity pulse.

The **go-to** button is equivalent to a double-click on the selection (routes through
`ui::focus_on_entity` — navigates a canvas for spatial entities, opens a ledger for
non-spatial ones). With nothing selected the band rests on the player's own corporation.
Unlike the fold-out ledgers it has **no nav-rail slot** — being selection-driven,
**selecting an entity is the only way to point it.** It carries the click-model shared
across all canvases: **single-click selects** (fills this band, no view change),
**double-click navigates**. See `SELECTION.md` and `CANVASES.md`.

## Time panel — top-right
**Spec: `TIME_CONTROLS.md`**

A single panel in the top-right corner, the same width as the minimap so the right edge stays aligned. It is split into **two columns** (25% / 75%): a compact calendar block on the left and the speed controls on the right. It publishes its measured height (`ui_state::time_panel_h`) so the rest of the right chrome column knows where its own ceiling is.

**Calendar block** (left column, 25%) — the player-facing clock in three rows:

- **Year + quarter** — `1960 Q1` (see `TIME_CONTROLS.md`).
- **Month + day** — `Jan 01`, abbreviated month + zero-padded day.
- **Quarter progress** — a progress bar through the current in-year quarter, labelled with its percentage.

**Speed controls** (right column, 75%) — a raw `Sim` counter with the current multiplier, then the controls:

- A **pause button** — a drawn filled square while running (deliberately not "||", which read as the Roman numeral II beside the tiers), flipping to a play "**>**" while paused — then speed tiers **I–V** in Roman numerals. The active speed is highlighted; each tier names its real rate on hover.

The clock has three layers — sim tick → day → economy tick — paced so that 1× ≈ 6 s/day and 3× ≈ 2 s/day. The economy resolves quarterly. The authoritative calendar/pacing constants live in `src/core/sim_loop.hpp` and are deliberately tentative.

---

## Comms dock — bottom-left
**Spec: `CHAT.md`**

The channel-based **comms chat log**: the Public channel plus arbitrary player-created
corp groups, fed by deterministic sim events (the agency reflexes) and player messages
(`src/ui/chat_panel.cpp`).

It docks in the **bottom-left tile of the bottom strip** (`comms_dock_rect`,
`src/ui/foldout_column.{hpp,cpp}`): the fold-out column's x-range, three quarters of the
column's width, sharing the Selection band's top edge and height so the two read as one
bar. Comms is ambient chatter, not a decision surface, so it does not hold the prime
right-edge slot under the time panel; the quarter-width it gives back goes to the
Selection band. The icon rail keeps its full height past the dock — at the 1280×720 floor
a shortened rail would clip two slots.

**The Field channel** (BL-468, dispatch stream) is the dock's second standing channel,
appended after Public — *appended*, never inserted, because `m_counsel_channel` caches an
index into the channel list. It carries battle traffic for **the player's own fights**: one
dispatch line per battle per tick, plus the aftermath line of any that concluded.
Rival-vs-rival fights are skipped, on the precedent that corporations stay out of comms so
a rival's internals do not leak through it — they are seen as a canvas marker instead, which
says *where* without saying how it is going (NR-470). It is separate from Public because its
volume is driven by **simulation intensity** rather than by scripted events — a running war
is several lines a tick, which mixed into Public would bury everything else. It is also the
only place the aftermath can live at all: a concluded battle is erased at the end of the tick
it ends, so no state-reading surface can still see who held the ground by the time the
player looks. There is no per-channel mute (NR-471).

The diplomacy-as-communication principle behind the surface: `docs/ai/AI_OPPONENT.md` § 7.

Inside the dock, **rows are the scarce axis, not width** — it is `selection_band_height`
tall (260 px at 1280–1920). The log's message form, channel selector and group popup are
laid out for that; see `CHAT.md` § Layout in the dock.

---

## Pinned items — right column, middle

The residual middle of the right chrome column — between the time panel and the minimap
— is the **pinned-items watch list**: a short, player-curated set of entities kept
visible while the player works elsewhere.

- **Rect:** `ui::pinned_panel_rect(disp, time_h)` (`shell_metrics.hpp`): `right_chrome_left`
  across, from one margin below the time panel down to one margin above the minimap.
  336 × ~698 px at 1720×1080, 336 × ~338 px at 1280×720.
- **The minimap does not move.** The column's split stays two-way elastic — the time
  panel is content-derived, the minimap is ratio-locked, and pins take the whole
  residual.
- **Not a second action surface.** A pin is a watch card — kind glyph, name, and the
  shared `entity_summary` stat block — over which single-click selects (the Selection
  band then shows the full action content) and double-click navigates, per the click
  model in `SELECTION.md`. Acting on a pin is one click away, by design. The pin toggle
  sits on the Selection band header.
- **No nav-rail slot.** Fixed shell chrome, not a ledger — absent from
  `close_all_panels` / `any_panel_open`, exactly like the comms dock and the Selection
  band.

The panel, its pin toggle and the pin glyph are BL-216's pinned-items slice.

---

## Ledger windows — all fold-out

No ledger floats. Every ledger — Construction, Economy, Market, Balance, Corporations, the
tile construction ledger, **and the Tile Ledger** (History, slot 9 — `tile_inspector.cpp`
opens through the same path) — draws as a **pinned, borderless panel filling the shell
column** (`ui::foldout_begin`/`foldout_end`, `src/ui/foldout_column.hpp`) when its nav slot
is active, and folds back to just the icon rail when toggled off or when another opens
(accordion). There is no title-bar close and no **✕** — closing is the nav-rail toggle.
The set of menus is described in **`MENU.md`**.

**All ledgers start closed** on a fresh session — none are shown until the player opens them from the pane (see the policy in `MENU.md`).

Only **broad** ledgers (overviews across many entities) earn a nav-rail slot; targeted, per-entity actions are reached contextually through the Selection info element or a popup, not the rail (see `MENU.md` § Menus are broad ledgers).

### One-question-per-view splits

Fold-out ledgers with more than one question split their content across a **button-strip nav**
(`ui::nav_button`, `foldout_column.hpp` — a manual `Selectable`/`Button` strip, since
`ImGui::BeginTabBar` does not render in this build), each view drawing exclusively. The splits:
the **Construction** panel — **Construction / Buildings** (defaults to Buildings; the build
front door is the tile Selection element's and sell orders are the Market Ledger's); the
**Market Ledger** — **Prices / Sell Orders**; the **Economy** panel — Corps / Holdings / Markets
(`ui_state::economy_view`, reachable from nav slot 3); the **History** ledger — Story / Chain /
Ages. The **Balance** and **Corporation** ledgers are single-question — no split. The principle
is *one question per view, a menu to move between views* — not a mandate to split every panel.

**Toggle rule on the strip.** Consistent with the universal toggle rule
(`.claude/rules/io-standing-rules.md`): re-clicking the **currently-active** sub-view tab **closes
the hosting ledger** (clears its `show_*` flag) — matching the nav-rail icon exactly — rather than
being a no-op. Switching to a *different* tab is an ordinary view change. `ui::nav_button` takes the
ledger's open-flag as an optional `close` target; a strip with no flag passed stays a plain,
non-closing selector.

### Economy-panel table legibility

The Corporations dashboard (`corporation_panel.cpp`) and the economy tables are tuned for the
narrow shell column: in ~244px a `SizingStretchProp` multi-column table collapses every column to a
leading glyph, so the **identity column stretches and the numeric columns take tight fixed widths**,
and low-value columns are dropped rather than clipped (the corp dashboard shows Corporation / Focus /
Balance; Home Nation and Status live on the Selection panel / row tint).

The Economy panel's tables use a **stretch name/resource column + fixed-width numeric columns**
(not `SizingStretchProp`, which collapses cells to a leading glyph — a balances column reading
`9 8 1 1…`). The **Corporation balances**, **Workforce**, **Stockpile pools**, and **Markets**
tables all follow this pattern, so corp/body/resource identity and the full numeric values read at
a glance. There is no per-building table on this panel — per-building profitability is the Corp
Dashboard's job, and duplicating it here is the redundancy the panel avoids.

### Ledger family conventions

All ledger windows share four standing conventions, stated once here so each new ledger
need not rediscover them:

- **Uniform chrome = the fold-out column.** Every ledger draws through
  `foldout_begin`/`foldout_end`, pinned to `foldout_column_rect` — one rect, one border
  policy, no per-ledger sizing.
- **Player corporation defaulted.** Every ledger that shows per-corporation data defaults
  to `w.player_entity` in its corp selector and offers a selector to view any other
  corporation's figures. Cross-corporation side-by-side comparison is not in scope for
  the prototype.
- **Shared content builders.** Per-entity stat blocks are rendered through the shared
  `entity_summary` helpers (`src/ui/entity_summary.{hpp,cpp}`) — the Tile Ledger and the
  pinned-item cards. The Selection element does not call them (SELECTION.md § Not a stat
  block) and the hover card carries its own lens-keyed content (`hover_content.cpp`,
  TOOLTIP.md); the rule stands for any new ledger stat block — do not duplicate the logic,
  call the builders.
- **Start closed.** All ledgers open with their initial `show_*` flag `false`
  (`src/ui/ui_state.hpp` defaults; toggled in `src/ui/nav_pane.cpp`). None are shown
  until the player explicitly opens them from the nav pane.

---

## Container vocabulary

A closed set of **ten** container kinds recur across the shell and canvases (BL-141,
container vocabulary). Each combines a sizing rule with exactly one text policy — **wrap**
(`PushTextWrapPos` + `TextWrapped`, reflowing to the box) or **guaranteed-fit**
(`CalcTextSize` measured first, the box sized to the text — the pattern the market-legend
key established) — plus an overflow rule for when content still exceeds the box. New UI
work lands in one of these ten rather than inventing an eleventh.

| # | Container | Sizing | Text policy | Overflow |
|---|---|---|---|---|
| 1 | **Fixed shell column / dock** — three instances: the **fold-out ledger column** (`foldout_begin`, `foldout_column.cpp`), the **comms dock** (`chat_panel.cpp`) and the **pinned items panel** | Fixed-width rect. The ledger column is `[nav_pane_width, W]`, from below the identity tile to the **comms dock's top edge**; the dock and the pins panel take their rects from `shell_metrics.hpp` | Wrap to inner width | Vertical scroll |
| 2 | **Lens legend box** (draw-list, `body_surface_canvas.cpp`) | Fit-to-content — gradient keys sized from their own entries; count-driven keys take the right-column width and wrap labels | Guaranteed-fit (gradient keys) / wrap (count-driven rows) | Gradient keys grow to fit; count-driven keys scroll inside the box |
| 3 | **Selection band** (`draw_selection_band`, `selection_card.cpp` framing `draw_selection_content`, `selection_panel.cpp`) | Fixed rect — comms dock's right edge to the right chrome column, `selection_band_height` tall (minimap-derived) | Wrap | Vertical scroll |
| 4 | **Header / balance strip** (`header_panel.cpp`) **and the identity tile** (`profile_panel.cpp`) | Stretch-to-width (strip) / fixed card (tile), fixed height | Guaranteed-fit — segments measured, never wraps | Elide-with-tooltip, only as a last resort; never silent truncation |
| 5 | **Time panel** (`app.cpp`) | Fixed width (shares the minimap's), content-derived height | Guaranteed-fit — authored to fit | None |
| 6 | **Hover card** (`draw_hover_card`, `hover_card.cpp`) | Fit-to-content, capped at 200 px max width, auto height | Wrap at the max width | Grows vertically |
| 7 | **Minimap lens bar** | Fixed strip | Icon-only / guaranteed-fit labels | None |
| 8 | **Nav rail** | Fixed (56 px) | Icon-only; tooltips wrap (`nav_pane.cpp`, `PushTextWrapPos`) | None (tooltip wrap absorbs it) |
| 9 | **ImGui table** (ledgers) | Stretch columns with per-column min widths | Per-cell guaranteed-fit (clip + tooltip) for numeric/identity columns, wrap for description columns | Horizontal scroll on the table; never silent truncation of a load-bearing value |
| 10 | **Hand-drawn chart plot** (draw-list, `charts.cpp`) | Host-supplied screen-space box, measured before it is reserved | Guaranteed-fit, **reflowing** — labels measured, the plot's own geometry yields to them | Legend reflows below the plot, then elide-with-record; never silent truncation of a plotted value |

The **comms dock** and the **pinned items panel** are both fixed-rect docked panels obeying
container 1's policy (wrap, vertical scroll) — which is why row 1 is named for the *policy*
rather than for the fold-out column alone.

This table is the baseline the text-wrap render audit consumes (BL-215, text-wrap render
audit) — an audit of every container's *rendered* wrap behaviour against the policy declared
here — so the rows above describe the real containers.

A few cross-cutting notes:

- **Wrap vs guaranteed-fit is a binary choice per container, not per instance.** A
  container that measures text to size itself (4, 5, 7, 9-numeric, the gradient keys of 2)
  never also wraps that same text; a container that wraps (1, 3, 6, 8-tooltip,
  9-description, the count-driven keys of 2) never pre-measures to a fixed box.
- **Silent truncation is never acceptable** for a load-bearing figure (balance, price,
  quantity). The only sanctioned degradations are elide-with-tooltip (4) and clip-with-tooltip
  (9) — both keep the value one hover away.
- These are prototype-tuned concrete behaviours (`foldout_column`, the market legend's
  measure-then-size pattern), not a general layout engine — see
  `docs/tech/TECH_FOUNDATIONS.md` for why a retained-mode framework is out of scope.
- Amending this set is a deliberate act, recorded here. Every text draw names its container
  through `ui::text_fit`; a draw that names none is caught by the coverage grep in
  `verifier-visual`.

---

## Drill-through

**One disclosure idiom, obeyed by every dense surface** (BL-214, drill-through; BL-265, the
in-place rung). Without it each surface invents its own way of showing more — a collapsing
header in one ledger, a pager in another, a permanent horizontal scroll in a table, nothing
at all in the wizard. Drill-through is the single idiom, and it has **three** states reached
by **two** controls:

| State | What it is | Control |
|---|---|---|
| **Folded** | A verdict line — a figure, a label, a glyph. No sentences. | — (the resting state) |
| **Expanded in place** | The row grows where it sits: one graph, or an accordion of graphs for a more complex menu. The rest of the screen keeps working. | `⌄` (becomes `⌃`, and collapses on click) |
| **Full canvas** | A takeover of the **canvas region** showing everything the surface has at once — every item of an accordion, open, top to bottom, scrolled. | `›` (leave with `‹`, or Esc) |

A two-state model — folded, or a full-screen overlay, nothing between — is too big a step
for *"I want to see this one chart properly"* (Ben, 2026-08-02, after using it); a
three-level Glance / Read / Study stepper with one cycling control is ruled out too (Ben,
2026-07-31). These are two distinct affordances with two distinct meanings, which is what
makes them legible where one cycling control is not.

### Three axes, never conflated

The competing idioms were never several answers to one question — they were three
different questions wearing one costume. Naming the three is most of the fix:

| Axis | Question | Gesture | State |
|---|---|---|---|
| **Depth** | *How much of this subject do I want?* | `⌄` in place, `›` full canvas | `ui_state::expanded` (the takeover target **and** the in-place set) |
| **Subject** (BL-196, resource drill-down) | *What am I looking at?* | click the element; breadcrumb + back | `ui_state::card_stack`, `corp_rollup_drill` |
| **Host** (SELECTION.md) | *Where does this properly live?* | `[>]` go-to | `ui::focus_on_entity` |

Subject is therefore the **sibling axis**, not a competitor and not folded in.

### The geometry: a takeover takes the CANVAS, not the window

`fold_overlay_begin` is sized to **`ui::canvas_rect()`**, not to the display (Ben: *"full
screen should actually just take the place of a canvas, rather than the entire view. I would
prefer to see consistency in the UI really."*). The nav rail, identity tile, header, clock,
comms dock, Selection band and minimap all survive a takeover, and the player never loses
their selection to open a chart.

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

1. **One TAKEOVER at a time — and only the takeover is single-target.** Two rules, not one:
   - The **takeover** is a single `(surface, key)` target (`fold_state::surface`/`key`): it
     owns one rectangle, so a second has nowhere to go. Opening a second replaces the first.
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
   single-block view that already shows its content in the column (History Story).
   Folded-by-default governs **scrolling** containers, where a fold buys real room back.
6. **Drill-through adds no eleventh container.** The ten kinds above say *how text fits*; this
   says *how much of it there is*. The takeover is container **1**'s policy (wrap, vertical
   scroll) at canvas size.

### The controls, and where they sit

**The rule, in one line: controls that operate on an item in a list are right-gutter-aligned;
the control that leaves a view is top-left.** Any disclosure surface is checked against that
sentence.

A **foldable row**:

```
Expandable item          verdict                       ⌄  ›
```

Both controls sit in a **fixed right gutter** (`ui::disclosure_controls`,
`ui::disclosure_gutter_width`) — not immediately after the label, because a gutter is what
makes them line up in one vertical column across every row. **A title's length must never
move its controls** (Ben: *"it is disorienting when a button is not in the same column, but
does the same job."*). `generation_charts.cpp`, `corporation_dashboard.cpp`,
`tile_inspector.cpp` and `selection_panel.cpp` all use the same column. A row that offers
only `›` leaves the gutter's **left slot empty** rather than letting `›` slide sideways, so
the full-canvas column never moves. A row's verdict is drawn through `ui::gutter_text`, which
clips it short of the gutter so a long verdict cannot run underneath the controls.

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
those codepoints are in the font atlas, and a string literal renders `?`. `draw_caret` and
`draw_open_arrow` in `src/ui/detail_level.cpp` build them through `ImDrawList`. Do not
introduce them as string literals.

**Esc** closes the takeover, one rung **below** the subject drills: exit-confirm → system
menu → pop `card_stack` → pop `corp_rollup_drill` → **close the takeover** → open menu. A
single press never both unwinds a drill and closes the view hosting it. Esc **does not**
collapse in-place expansions: that is a reading preference, the kind of state Esc has never
collapsed, and putting it on the ladder would make Esc unpredictable (sometimes leaving a
view, sometimes re-folding four graphs you deliberately opened). A takeover with no keyboard
exit is a defect, not a principle.

The takeover's **entry/exit transition** is deliberately unsettled and instant — a feel
question that wants the live app, not a paragraph.

### No chart question log

There is no closed-by-default "Why this chart" toggle (`ui::why_note`) revealing an
*Answers:* / *Because:* pair on a chart, and none is to be built. Ben removed it under
**NR-018**; `src/ui/detail_level.cpp` carries the standing note — *"Do not reinstate a draw
path here without reopening NR-018."* There are no `why_note` symbols in `src/`.

The **derivation caption** — which answers how a number was computed — is a different thing
and stands.

### The surfaces, and the extension recipe

On the ladder, with what each one's takeover shows:

| Surface | `⌄` in place | `›` full canvas |
|---|---|---|
| Selection band's metric card | — (rests expanded, invariant 5) | that metric's chart, on the canvas |
| History ledger — Story | — (already shown in the column) | the whole biography, on the canvas |
| History ledger — Chain (per stage) | that stage's title, explainer and charts | the **whole round** — every stage open, scrolled |
| New World wizard — chain stages | that stage's title, explainer and charts | the **whole round**, scrolled (window-wide; no shell yet) |
| Corporation dashboard — 4 roll-ups | that card's chart or rows | **all four** roll-ups, headed and scrolled |

The three accordions — History Chain, wizard stages, corp roll-ups — show everything in a
takeover: a surface that has taken the largest rectangle available has no excuse for showing
a subset. The History ledger's **Ages** tab takes no disclosure control at all — its map sizes
itself to whatever column it is given.

Adding surface #N is **two edits**: an enumerator in `detail_surface`, and a
`disclosure_controls` + `fold_overlay_begin` pair at the call site. Nothing else — no new
control, no new state, no new container kind. A surface with many foldable blocks needs
**one** enumerator, not one per block: the `key` disambiguates instances (a chain stage, a
roll-up card). Removing a surface is the same two edits in reverse.

---

## UI popup elements

Beyond the persistent chrome, the UI uses **transient popup elements** — context menus, confirmation dialogs, hover cards, and action prompts that appear in response to a click or hover and dismiss on action or click-away. Examples: right-clicking a unit for an order menu, a "confirm purchase" dialog.

The **hover card** is the glance-then-stick card on the Planetary surface, framed by `draw_hover_card` (`src/ui/hover_card.cpp`) with lens-keyed content from `hover_content.cpp` — [`TOOLTIP.md`](TOOLTIP.md) is its spec. The system menu and its inline exit-confirm, and the confirm popups on Dismantle and Withdraw (SELECTION.md), are the popup dialogs. Context menus float above every region without belonging to any one of them, which the layout accounts for.

---

## Developer affordances

- **F12** captures the exact composited frame to `build/Debug/screenshots/`. The `tools/capture.ps1` wrapper builds, launches, captures, and converts to PNG in one step.

---

## Prototype notes

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
