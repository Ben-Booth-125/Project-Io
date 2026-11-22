# Project Io — UI Layout

> **Settles:** which regions the application shell is divided into and where each
> sits · which region owns the rect algebra and how the top and bottom bands share
> their edges · what hosts a fold-out ledger and how wide that column gets · which
> container vocabulary a new surface should reach for.
> **Not here:** what any one panel says (HEADER, PROFILE, TIME_CONTROLS) ·
> which ledgers the rail holds (MENU) · what the canvases draw (CANVASES) · what a
> click selects (SELECTION) · the lens mode bar and the region above the minimap
> (MINIMAP) · how much of a surface is shown at once (DRILL_THROUGH).
> **Confused with:** MENU.md, CANVASES.md, DRILL_THROUGH.md.

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
│ ▢ … │ (runs to   │                             │ [lens legend]│
│ ▦   │ the screen │                             ├──────────────┤
│ ▢ … │ bottom,    │                             │   Minimap    │
│ ▢   │ over the   ├─────────────────────────────┤  [lens bar]  │
│     │ LOCATOR)   │ SELECTION BAND (fixed;      │              │
│     │ LOCATOR    │ action/facts or tile)       │              │
└─────┴────────────┴─────────────────────────────┴──────────────┘
```

**The top band is one row of three tiles sharing a bottom edge** — profile, balance bar, time
controls — exactly as the bottom band is one row of three sharing a top edge. No internal
gutter in either: the balance bar runs to the right chrome column's edge, and the time panel's
height is floored at `profile_panel_height` rather than ending wherever its content happens to
stop. Ben, 2026-08-24: widen the time controls *"to the same x/y as the corner of our main
balance bar"*. The gap that used to sit between the balance bar and the time panel was the
shell's only internal one, which is the argument `selection_band_rect` already made for the
bottom band — *"introducing the shell's only gap here would read as a mistake"*.

The nav rail and the fold-out column both run the **full screen height**. The screen's
**bottom strip** is one solid band of three tiles sharing a top edge that lands exactly on
the minimap's: the **locator** bottom-left, under the fold-out column and its width; the
**Selection band** from the shell column's right edge (`shell_column_width`) to the right
chrome column; and the **minimap** in the corner. A ledger runs from the identity tile to the
screen's bottom edge, over the locator — see *Selection band* and *The locator* below.

The **right chrome column** splits two ways and elastically: the content-derived
**time panel** on top, the ratio-locked **minimap** at the foot, and the whole
residual middle to **pinned items** — the space a count-driven lens legend also
drops into when opened (MINIMAP.md § Lens chrome). The minimap does not move.

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
`foldout_column_rect`) rather than duplicating them — the split
is primitives there, composed rects here. **Every region edge lands on a whole pixel**: the
shell column's width, the right chrome column's left edge (`right_chrome_left`, rounded, with
the minimap and time panel running from it to the screen edge) and the bottom strip's height
(`selection_band_height`, rounded). ImGui truncates a window's position and size, so a
fractional edge left a one-pixel seam of map between neighbours (Ben, 2026-11-22: the
Selection band "should expand so it covers the edges of the canvas, and the edge of the mini
map"). The right column is flush to the screen edge, and every occupant of it shares one left
edge.

Two layers compose the screen:

- **Background** — the canvases, drawn edge-to-edge via the ImGui background draw list.
- **Foreground** — the ImGui panels below (profile, header, nav pane, time column, minimap, ledger windows), drawn on top of the canvases.

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
  identity tile) when a nav slot is active — see *Ledger windows*. The column **runs to the
  screen's bottom edge** (`foldout_column_rect`), covering the locator, so a ledger loses no
  height to the bottom strip (Ben, 2026-11-19; 2026-11-20).
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
taking its full width `W`. Shows the player's nation at a glance:

- **No emblem** — a nation carries none until one is designed (Ben, 2026-10-26; `PROFILE.md`).
- **Nation name**, read live from the nation table.

This is a static identity readout in the prototype — no interaction beyond, eventually, opening a fuller nation screen. Implemented in `src/ui/profile_panel.cpp`; see `PROFILE.md`.

---

## Header — top
**Spec: `HEADER.md`**

A strip across the top of the canvas area, between the identity tile and the time column. Its left edge is the shell column's right edge (`x = W`). It stands the **full identity-tile height** (`profile_panel_height`, ~92 px) and top-aligns at `y = 0`, so the header and the identity tile read as **one level top band**; its content row is vertically centred within that strip. It is the player's persistent financial dashboard, wired to the live economy:

- **Treasury** — the player nation's treasury (a nation never runs into debt, so it has no negative state).
- **Net, in brackets beside the treasury** — the last economy tick's operating net as a coloured per-month figure, `TREASURY 1.2M (+34/mo)`. Hovering a money figure shows the graph of recent treasury values (Ben, 2026-11-05).
- **Cargo valuation** — an estimated value of the goods the player owns: its trades' cargo in transit, at each destination market's current price (`HEADER.md`). A single money figure, not a per-resource inventory.

The header answers "can I afford this, and which way is it trending?" without opening a ledger. Detailed, per-body breakdowns stay in their respective ledgers.

---

## Navigation pane — left
**Spec: `MENU.md`**

A fixed, full-height **icon rail** pinned to the left edge (`nav_pane_width`, 56 px), below the profile. Cannot move, resize, or collapse. The rail is narrow; the profile keeps its own (wider) `profile_panel_width` above it rather than matching the rail.

- Holds a vertical strip of **twelve square icon slots** — the home for the game's menus and ledgers. **Every slot carries its own vector glyph** (`src/ui/icons.hpp`) — live slots in the bright stroke, reserved slots dimmed — plus a wrapping name-and-blurb tooltip.
- Each slot toggles a panel open/closed; the open slot lights its glyph in the selection accent. An active slot **folds its ledger out into the shell column** to the rail's right (`[nav_pane_width, W]`) rather than spawning a floating window; opening one collapses whichever was open (accordion, via `close_all_panels`).
- **All twelve slots are live** (`nav_pane.cpp`): the curated slots carry Nation overview, Budget, Construction, Market Ledger, Convoys, Military, Diplomacy and History, and Research hosts the tech-tree viewer as a **provisional occupant**; the tail carries Generation Ledger, AI decisions and Strategy readout. **Acquisitions retired** with the buyout (Ben, 2026-10-23) and its slot left the rail, every later slot moving up one (Ben, 2026-10-24); **Diplomacy's corporations table retired** with the corporation (Ben, 2026-10-23) and the slot carries the nation-grain Diplomacy ledger (Ben, 2026-10-30); **Corp. Strategy retired** with the corporations and its slot went to Military (Ben, 2026-10-27). Full slot semantics in `MENU.md`.

---

## Alert strip — centre, below the header (Ben, 2026-11-05)

A strip at the top centre of the canvas area, just below the header. It says what wants the player's
attention, and **each alert clicks through** to the surface that answers it. It informs and never acts
(`../CONCEPT.md` § Player identity). The alerts:

- **Research not set**
- **Idle buildings**
- **Wars**
- **Low runway**
- **Construction stalled**
- **Deposit exhausted**
- **Trades exceed trade points**

It says only what is true now: an alert leaves when its cause does and cannot be dismissed, and the
strip draws nothing with nothing to say. One press stands for each kind, its count in its words, so
a dozen idle mines are one alert. It wraps rather than clips. Low runway is the header's own red bar,
under 24 months (`HEADER.md` § Contents).

## War notice — on the canvas, below the alert strip (Ben, 2026-11-09)

When a war the player's nation is party to opens, by either side's declaration, a notice appears at
the top centre of the canvas area. It names both parties and stays until it is dismissed, one notice
per war. It is an announcement, not a dialog: it pauses nothing and changes nothing. Where the player
was declared on and has not answered, it offers **Answer** (`ledgers/diplomacy.md`); otherwise a door
to the Wars tab. A war already standing on a new or loaded world raises no notice.

A nation's end raises the same notice when the player's nation took part in its last war (Ben,
2026-11-09). When the ended nation is the player's own, the notice says the campaign is lost and
offers to spectate or return to the menu.

---

## UI scale (Ben, 2026-11-05; Ben, 2026-11-10; Ben, 2026-11-11)

The UI is smaller than the size its chrome was authored at, so **each element is smaller** and text
stops clipping (Ben, 2026-11-05).

**The UI fits the window; it is not a setting** (Ben, 2026-11-11). The font is 16 px at a window
1080 lines tall and moves with the window's height, rounded to a whole pixel and held between 12 and
24 px: about 12 px at 720 lines, 21 px at 1440, 24 px at 1620 and above. A resize refits it. There is
no UI Scale control, and `options.cfg` stores none. The smallest fit, 12 px, is 0.75x of the 16 px
the chrome was authored against (Ben, 2026-11-10), and sets the window floor: 960x540.

**The menu column and the Selection band keep their share of the window down to 720 lines** (Ben,
2026-11-11). They are sized by the window height over 1080, not by the font, so below about 810
lines, where the font has stopped at 12 px, they still shrink with the window: the column is about
253 px and the band about 187 px at 720 lines. Below 720 lines they hold.

**Everything in the chrome scales with the text, by one factor** (Ben, 2026-11-10): the font px over
16. Padding, spacing and item sizes scale with it (ImGui's style, rebuilt from a fresh base and
scaled on every change), and so does every hard-coded pixel in the chrome: panel and column widths,
the icon rail, heights, offsets, gaps and glyph sizes. One factor is the source of truth
(`ui::ui_scale()`, `src/ui/ui_scale.hpp`); a chrome length is written at its authored 16 px value and
read through `ui::px` or a `ui::px_const`. A layout rule written against the display (a column that is
a share of the window, between two px bounds) reads the display in authored pixels and scales its
answer, so at 0.75x the shell is the 1.0x shell drawn smaller.

**Canvas ground does not scale** (the main session's reading of Ben's 2026-11-10 ruling, open as NR-1216). The map bake, tile sizes and zoom are the
canvases' own (`RENDERING.md`); a smaller UI gives the canvas more of the window, not smaller tiles.

The window's minimum size is 960 x 540 (BL-215), the layout at its smallest font. The headless
harness runs at a fixed 12 px with the panels at the same factor, whatever its window's height.

---

## Canvas area — centre
**Spec: `CANVASES.md`**

Three canvases — Solar, Circumplanetary, Planetary — form a **zoom ladder** and share the window. One is **primary** (fills the window) and the rung one step *out* from it is shown in the **minimap** (a fixed inset, bottom-right). The player **descends** (zooms in) by double-clicking a body in the primary canvas and **ascends** (zooms out) by clicking the minimap. Full detail — visual language, coordinate mapping, interaction, and the ladder navigation rules — lives in **`CANVASES.md`**.

The canvases render behind the foreground panels, so the chrome occludes the edges of the primary canvas.

**The region has a name: `ui::canvas_rect()`** (`src/ui/foldout_column.{hpp,cpp}`) — bounded by the shell column on the left, the header strip on top and the bottom strip below, with the minimap floating inside it rather than reserving a column. Anything that occupies the main stage without being a canvas — a takeover, the tech-tree viewer — is sized to this rect. `CANVASES.md` owns the zoom-ladder canvases themselves.

---

## Minimap — bottom-right inset
**Spec: `MINIMAP.md`**

A fixed inset in the bottom-right corner showing the **zoom-out neighbour** of the primary canvas at reduced scale, framed by its own chrome — a title bar (the viewed body, or the star name; the game name at the top rung) above the inset. It is sized off the window's shorter dimension against a pixel floor, at a 4:3 aspect — `MINIMAP.md` § Sizing and placement owns that algebra; the time panel scales with it, sharing the right-column width. Clicking it **ascends** one rung. It shares the primary canvases' drawing code, parameterised by region size. The minimap also carries the **lens mode bar** along its bottom edge, with the **lens chrome region** anchored above its top edge; `MINIMAP.md` is the authoritative spec for the minimap chrome, the lens bar, that region, and the ladder navigation, and `CANVASES.md` covers the shared drawing path.

---

## Selection band — fixed bottom band
**Spec: `SELECTION.md`**

The Selection info element lives in a **fixed band at the bottom of the screen** — the
left tile of the bottom strip, from the **shell column's right edge** (`shell_column_width`,
so an open ledger sits beside it, never under it) to the **minimap** on its right. Its height is
`selection_band_height` (`src/ui/foldout_column.{hpp,cpp}`), **derived from the minimap
height** plus `chrome_margin` so the whole strip's top edge lands on the minimap's. The
band always occupies the same rect — it is never click-anchored. **It meets every neighbour
with no gap and square corners** — the shell column on its left, the minimap on its right, the
screen's foot below (Ben, 2026-11-22): a rounded corner or a fractional edge let the map show
through the seam. Ben's call: *"doing/
building" menus need a fixed place at the bottom of the screen, not a widget that floats
with the cursor.* It shows detail about the **current selection**, whatever entity the
player last single-clicked, and **does not close an open ledger**: the two do not
compete for the same space.

It is an **action surface**, not a stat block: a header row (`[kind icon] Name · type` on
the left, a **go-to** `>` button on the right; no close button — the band is always open)
over the kind's content — except a **tile**, which draws no header and gives the band's whole
height to four columns (preview, province card, tile card, action grid; a building is that
card with one stack expanded in place). A **unit** and a **battle** use the purpose-built
three-column band layout (left image, centre accordion, right 2×3 action grid — § SELECTION.md); the remaining kinds
(body, market, nation) use an action|facts split — a **body** offers *Dispatch
Survey* or *Go to surface* beside its commercial-activity pulse.

The **go-to** button is equivalent to a double-click on the selection (routes through
`ui::focus_on_entity` — navigates a canvas for spatial entities, opens a ledger for
non-spatial ones). With nothing selected the band rests on the player's own nation.
Unlike the fold-out ledgers it has **no nav-rail slot** — being selection-driven,
**selecting an entity is the only way to point it.** It carries the click-model shared
across all canvases: **single-click selects** (fills this band, no view change),
**double-click navigates**. See `SELECTION.md` and `CANVASES.md`.

## Time panel — top-right
**Spec: `TIME_CONTROLS.md`**

A single panel in the top-right corner, the same width as the minimap so the right edge stays aligned. It is split into **two columns** (25% / 75%): a compact calendar block on the left and the speed controls on the right. It publishes its measured height (`ui_state::time_panel_h`) so the rest of the right chrome column knows where its own ceiling is.

**Calendar block** (left column, 25%) — the player-facing clock in three rows:

- **Year + month** — `1960 Q1` (see `TIME_CONTROLS.md`).
- **Month + day** — `Jan 01`, abbreviated month + zero-padded day.
- **Month progress** — a progress bar through the current in-year month, labelled with its percentage.

**Speed controls** (right column, 75%) — a raw `Sim` counter with the current multiplier, then the controls:

- A **pause button** — a drawn filled square while running (deliberately not "||", which read as the Roman numeral II beside the tiers), flipping to a play "**>**" while paused — then speed tiers **I–V** in Roman numerals. The active speed is highlighted; each tier names its real rate on hover.

The clock has three layers — sim tick → day → economy tick — paced so that tier III (5×) runs a day in 1.2 s (`TIME_CONTROLS.md` § Speed curve). The economy resolves monthly. The authoritative calendar/pacing constants live in `src/core/sim_loop.hpp` and are deliberately tentative.

---

## The locator — bottom-left (Ben, 2026-11-20)

The bottom-left tile, under the fold-out column and exactly its width (`[nav_pane_width,
shell_column_width]`), at the bottom strip's height. It is the **zoomed map of the ground under
the selection** (`draw_selection_locator`): the selected tile, a building's or a unit's tile, a
market's capital, a nation's capital — the player's own capital at rest. It is the tile card's
former first column, moved out so the Selection band has the width and the corner the comms dock
left is not an empty tile. A selection with no one tile (a convoy, a deposit, a plate, a front)
leaves it dark; a battle's card shows the battle's ground there. It is a plan view: the canvas's
3D buildings appear in it seen from above (`SELECTION.md` § the tile element, Preview).

**A ledger covers it.** With a ledger open the column runs to the screen's bottom over the
locator, which is not drawn; it returns when the ledger closes. It takes no input.

## The comms dock is retired (Ben, 2026-11-19)

The bottom-left comms dock — the channel chat log with its Public, Field and Counsel channels
and player-made groups — is **retired outright**, with every feed that posted to it. Its tile
is the **locator**'s (above), which an open ledger covers, and the Selection band starts at the
column's right edge rather than the dock's.

Nothing it carried moves to another surface. What the player must know already has a home
that does not scroll away: a war opened on them is the **war notice** and the **alert strip**
(above); a war's record is the History ledger's; a battle is its marker on the canvas and its
own card. Ambient lines — a rival nation's heaviest event of the month, a persona's counsel —
and the quick save's "Saved to" line go with the dock.

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
  sits on the Selection band header, at its top-right corner, and shows whenever the band
  shows an entity; it is a toggle, so pressing it on a pinned subject unpins it.
- **What is kept.** The player's pins travel in the save's ui slice, in pin order; a pin
  whose entity is gone drops off. Front pins are derived from the war each frame and are
  never saved.
- **The lens key yields nothing.** A count-driven lens key grows up from the minimap into
  the same residual (`MINIMAP.md` § Lens chrome); the pins stop one margin above it.
- **No nav-rail slot.** Fixed shell chrome, not a ledger — absent from
  `close_all_panels` / `any_panel_open`, exactly like the Selection band.

The panel, its pin toggle and the pin glyph are BL-216's pinned-items slice.

**Front pins (Ben, 2026-11-10).** While the player's nation is at war, the panel carries one **front
pin** for every body where the war has a front (`../military/MILITARY.md` § Space, "A war spans
every body"). A front pin is placed and removed by the war itself, not by the player: it appears when
a front opens and goes when it closes. It shows the body, the enemy and the front's state; a
double-click takes the canvas to that body's front, so a war on a moon is one click from a war at
home. Front pins sit above the player's own pins. A single click selects the player's front toward
that enemy on that body (its front card), or the enemy nation where there is no land front; a
double click lands on the middle of that front, else a contested province, else the enemy's
province nearest the player's ground. The front's state is the provinces each side holds there,
the provinces occupied either way, and the war score from the player's side.

---

## Ledger windows — all fold-out

No ledger floats. Every ledger — Construction, Market, Balance, the
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
front door is the tile Selection element's and trades are the Market Ledger's); the
**Market Ledger** — **Goods / Trades**; the **History** ledger — Story / Chain /
Ages. The **Balance** ledger and the **Nation overview** are single-question — no split. The principle
is *one question per view, a menu to move between views* — not a mandate to split every panel.

**Toggle rule on the strip.** Consistent with the universal toggle rule
(`.claude/rules/io-standing-rules.md`): re-clicking the **currently-active** sub-view tab **closes
the hosting ledger** (clears its `show_*` flag) — matching the nav-rail icon exactly — rather than
being a no-op. Switching to a *different* tab is an ordinary view change. `ui::nav_button` takes the
ledger's open-flag as an optional `close` target; a strip with no flag passed stays a plain,
non-closing selector.

### Narrow-column table legibility

Every ledger table is tuned for the narrow shell column: in ~244px a `SizingStretchProp`
multi-column table collapses every column to a leading glyph — a balances column reading
`9 8 1 1…` — so the **identity column stretches and the numeric columns take tight fixed
widths**, and low-value columns are dropped rather than clipped.

### Ledger family conventions

All ledger windows share four standing conventions, stated once here so each new ledger
need not rediscover them:

- **Uniform chrome = the fold-out column.** Every ledger draws through
  `foldout_begin`/`foldout_end`, pinned to `foldout_column_rect` — one rect, one border
  policy, no per-ledger sizing.
- **Player nation defaulted.** Every ledger that shows per-nation data defaults to the
  player's nation in its selector and offers a selector to view any other nation's figures
  (re-subjected from the retired corporation, Ben, 2026-10-23). Cross-nation side-by-side
  comparison is not in scope for the prototype.
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
| 1 | **Fixed shell column / dock** — two instances: the **fold-out ledger column** (`foldout_begin`, `foldout_column.cpp`) and the **pinned items panel** | Fixed-width rect. The ledger column is `[nav_pane_width, W]`, from below the identity tile to the screen's bottom edge; the pins panel takes its rect from `shell_metrics.hpp` | Wrap to inner width | Vertical scroll |
| 2 | **Lens legend box** (draw-list, `body_surface_canvas.cpp`) | Fit-to-content — gradient keys sized from their own entries; count-driven keys take the right-column width and wrap labels | Guaranteed-fit (gradient keys) / wrap (count-driven rows) | Gradient keys grow to fit; count-driven keys scroll inside the box |
| 3 | **Selection band** (`draw_selection_band`, `selection_card.cpp` framing `draw_selection_content`, `selection_panel.cpp`) | Fixed rect — the shell column's right edge to the right chrome column, `selection_band_height` tall (minimap-derived) | Wrap | Vertical scroll |
| 4 | **Header / balance strip** (`header_panel.cpp`) **and the identity tile** (`profile_panel.cpp`) | Stretch-to-width (strip) / fixed card (tile), fixed height | Guaranteed-fit — segments measured, never wraps | Elide-with-tooltip, only as a last resort; never silent truncation |
| 5 | **Time panel** (`app.cpp`) | Fixed width (shares the minimap's), content-derived height | Guaranteed-fit — authored to fit | None |
| 6 | **Hover card** (`draw_hover_card`, `hover_card.cpp`) | Fit-to-content, capped at 200 px max width, auto height | Wrap at the max width | Grows vertically |
| 7 | **Minimap lens bar** | Fixed strip | Icon-only / guaranteed-fit labels | None |
| 8 | **Nav rail** | Fixed (56 px) | Icon-only; tooltips wrap (`nav_pane.cpp`, `PushTextWrapPos`) | None (tooltip wrap absorbs it) |
| 9 | **ImGui table** (ledgers) | Stretch columns with per-column min widths | Per-cell guaranteed-fit (clip + tooltip) for numeric/identity columns, wrap for description columns | Horizontal scroll on the table; never silent truncation of a load-bearing value |
| 10 | **Hand-drawn chart plot** (draw-list, `charts.cpp`) | Host-supplied screen-space box, measured before it is reserved | Guaranteed-fit, **reflowing** — labels measured, the plot's own geometry yields to them | Legend reflows below the plot, then elide-with-record; never silent truncation of a plotted value |

The **pinned items panel** is a fixed-rect docked panel obeying container 1's policy (wrap,
vertical scroll) — which is why row 1 is named for the *policy* rather than for the fold-out
column alone.

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
- How much of a surface is shown at once — the `⌄` in-place and `›` full-canvas idiom —
  is [DRILL_THROUGH.md](DRILL_THROUGH.md).

---

## UI popup elements

Beyond the persistent chrome, the UI uses **transient popup elements** — context menus, confirmation dialogs, hover cards, and action prompts that appear in response to a click or hover and dismiss on action or click-away. Examples: right-clicking a unit for an order menu, a "confirm purchase" dialog.

The **hover card** is the glance-then-stick card on the Planetary surface, framed by `draw_hover_card` (`src/ui/hover_card.cpp`) with lens-keyed content from `hover_content.cpp` — [`TOOLTIP.md`](TOOLTIP.md) is its spec. The confirm popups on Dismantle and Withdraw (SELECTION.md), are the popup dialogs. Context menus float above every region without belonging to any one of them, which the layout accounts for.

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
| Drill-through / disclosure | `DRILL_THROUGH.md` |
| Discovery / fog surfaces | `DISCOVERY.md` |
| Time column | `TIME_CONTROLS.md` |
