# Project Io — UI Layout

Surface-level description of the application **shell** — the persistent chrome arranged around the canvases. This document covers *where things sit and what they are for*, not their internals. Each region below links to its own detailed specification where one exists. For the canvases themselves see **`CANVASES.md`**; for the tick model see `src/core/sim_loop.hpp`.

The prototype UI is built with Dear ImGui (see TECH_FOUNDATIONS). Everything here is a debugging-grade layout that doubles as the functional specification for the eventual production shell — it is expected to be revised.

---

## Screen regions

```
┌──────────┬────────────────────────────────────┬──────────────┐
│ Profile  │              Header                 │  Time panel  │
├──────────┤      (budget + resource strip)      │ (date + bar, │
│          ├────────────────────────────────────┤  speed ctl)  │
│ Nav rail │                                     ├──────────────┤
│ (icons)  │         Primary canvas              │              │
│ ▢        │  (Solar / Circumplanetary / Surface)│   Explorer   │
│ ▢        │                                     │  (pinned     │
│ ▢  …     │     [ floating ledger windows ]     │   shortcuts) │
│ ▦ (8)    │                                     │              │
│ ▢  …     │                                     │              │
│ ▢        │  [ Selection info — action surface ]┌──────────────┐│
│ ▢        │  (owns bottom-left corner)          │   Minimap    ││
│          │                                     │ [lens bar]   ││
│          │                                     │   (inactive  ││
│          │                                     │    canvas)   ││
└──────────┴─────────────────────────────────────┴─────────────┘
```

Two layers compose the screen:

- **Background** — the canvases, drawn edge-to-edge via the ImGui background draw list.
- **Foreground** — the ImGui panels below (profile, header, nav pane, explorer, time column, minimap, ledger windows), drawn on top of the canvases.

The explorer is **not yet implemented** — it is specified here ahead of the work so the shell has a settled shape. Layer 2 ships the nav pane, canvases, time column, minimap, and the Tile Ledger window; the header (Layer 3 finalisation) and profile (`src/ui/profile_panel.cpp`) have since landed.

---

## The shell column (BL-122)

Since **BL-122** the left edge is a single **permanent shell column** of width
`W = clamp(round(0.17 · display_width), 300, 360)` px (`ui::shell_column_width`,
`src/ui/foldout_column.hpp`) — runtime-computed from the display so it stays legible
across resolutions rather than a magic constant. The column is reserved down the whole
left edge:

- The **identity tile** (profile) caps it at top, taking the full width `W`.
- The narrow **icon nav rail** (56 px) runs down its left sub-edge.
- A **fold-out ledger** fills the rest of the column (`[nav_pane_width, W]`, below the
  identity tile to the bottom margin) when a nav slot is active — see *Ledger windows*.
- The **balance bar** (header) and the **Selection element** both start at `x = W`
  permanently, whether or not any fold-out is open, so they always clear the column.

The 300 px floor is deliberate: it is the constraint that forces the one-question-per-view
panel splits (BL-117..121). The per-panel splits and the Tile Ledger's migration into the
column are **not** part of BL-122 — the skeleton hosts each panel's current content.

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

A strip across the top of the canvas area, between the identity tile and the time column. Its left edge is the shell column's right edge (`x = W`, BL-122). It is the player's persistent financial dashboard, wired to the live economy as of the Layer 3 finalisation:

- **Balance** — the player corporation's running treasury balance (negatives flagged red).
- **Stockpile valuation** — an estimated liquid value of everything the player holds: its `(corporation, body)` pools summed at each body's current market price. A single money figure, not a per-resource inventory.
- **Net + trend** — the last economy tick's net change as a coloured per-quarter figure, alongside a small sparkline of recent balances.

The header answers "can I afford this, and which way is it trending?" without opening a ledger. Detailed, per-body breakdowns stay in their respective ledgers.

---

## Navigation pane — left
**Spec: `MENU.md`**

A fixed, full-height **icon rail** pinned to the left edge (`nav_pane_width`, currently 56 px), below the profile. Cannot move, resize, or collapse. The rail is narrow; the profile keeps its own (wider) `profile_panel_width` above it rather than matching the rail.

- Holds a vertical strip of **ten square icon slots** — the home for the game's menus and ledgers. Each slot shows a **vector glyph** (`src/ui/icons.hpp`) instead of a worded label, with the menu name in a hover tooltip.
- Each slot toggles a panel open/closed; the active slot is highlighted. Since BL-122 an active slot **folds its ledger out into the shell column** to the rail's right (`[nav_pane_width, W]`) rather than spawning a floating window; opening one collapses whichever was open (accordion, via `close_all_panels`).
- **Layer 2 wires only one slot: the Tile Ledger** (a ruled-table glyph), parked at slot 8. The other nine are reserved, disabled placeholders (a neutral hollow-square glyph). Slot placement is temporary while canvas work takes priority over menu design.

---

## Canvas area — centre
**Spec: `CANVASES.md`**

Three canvases — Solar, Circumplanetary, Planetary — form a **zoom ladder** and share the window. One is **primary** (fills the window) and the rung one step *out* from it is shown in the **minimap** (a fixed inset, bottom-right). The player **descends** (zooms in) by clicking a body in the primary canvas and **ascends** (zooms out) by clicking the minimap. Full detail — visual language, coordinate mapping, interaction, and the ladder navigation rules — lives in **`CANVASES.md`**.

The canvases render behind the foreground panels, so the chrome currently occludes the edges of the primary canvas. Insetting the primary region clear of the chrome is a known follow-up.

---

## Minimap — bottom-right inset
**Spec: `MINIMAP.md`**

A fixed inset in the bottom-right corner showing the **zoom-out neighbour** of the primary canvas at reduced scale, framed by its own chrome — a title bar (the viewed body, or the star name; the game name at the top rung) above the inset. Clicking it **ascends** one rung. It shares the primary canvases' drawing code, parameterised by region size. Since BL-093 the minimap also carries the **lens mode bar** along its bottom edge (see below); `MINIMAP.md` is the authoritative spec for the minimap chrome, the lens bar, and the ladder navigation, and `CANVASES.md` covers the shared drawing path.

---

## Lens mode bar — on the minimap
**Spec: `MINIMAP.md` / `LENSES.md`**

The overlay-lens controls no longer occupy their own bottom-left strip — BL-093
relocated them onto the **minimap itself**, as a 7-glyph mode bar running along
its bottom edge (`ui::draw_overlay_controls`, called from the minimap block in
`src/core/app.cpp`). The on-screen seven are Corp, Country, Resource, Market,
Population, Opportunity, and Production; Scarcity and Industry join Supply as
**keyboard-cycle only** lenses (no bar glyph — the bar does not have room for
all nine). Clicking a glyph toggles that lens; the active one is highlighted,
and clicking it again clears the overlay. The lens-local resource/good picker
(Resource, Market, Scarcity) is now a **popup button** on the bar rather than an
inline combo — the fixed-width combo did not fit the bar's reduced footprint.
Freeing the bottom-left strip is what lets the Selection info element (below)
take over the whole corner.

## Selection info element — bottom-left corner
**Spec: `SELECTION.md`**

The Selection info element sits in the bottom-left, anchored to the bottom margin.
Since BL-093 it sizes its height from content; since BL-122 its **left edge is the
shell column's right edge** (`x = W`) — so it clears the fold-out column permanently —
and its right edge is the minimap's left edge. It shows detail about the **current
selection**, whatever entity the player last single-clicked.

It is now an **action surface**, not a stat block: a header row
(`[kind icon] Name · type` on the left, **go-to** `>` and **close** `x` buttons
on the right) over two columns — a dominant **ACTION** column (~58% width, the
kind's primary affordance) beside a narrower, muted **FACTS** column. Per
selection kind: a **tile** offers *Build here* as the hero action beside its
Thrives/Valid placement facts; a **body** offers *Dispatch Survey* or *Go to
surface* beside its commercial-activity pulse; a **player-owned building**
offers a *Manage building* button that routes into the construction panel
beside its profitability readout; a **rival building** is intel-only — owner
and location facts, production/stockpile shown as explicit private rows. The
old undifferentiated stat-block polymorphism and the separate lens-supplement
section are gone — the action/facts split *is* the per-kind content now.

The panel **sizes its height to its content** in text-line units (so it stays
correct across resolutions) rather than matching a fixed neighbour's height,
and is anchored bottom-left.

The **go-to** button is equivalent to a double-click on the selection (routes
through `ui::focus_on_entity` — navigates a canvas for spatial entities, opens a
ledger for non-spatial ones); **close** hides the panel until the next
selection. Unlike the floating ledgers it is **not** reachable from the
navigation rail — **selecting an entity is the only way to open it.** It carries
the click-model shared across all canvases: **single-click selects** (fills this
panel, no view change), **double-click navigates**. See `SELECTION.md` and
`CANVASES.md`.

## Time panel — top-right
**Spec: `TIME_CONTROLS.md`**

A single panel in the top-right corner, the same width as the minimap so the right edge stays aligned. It is split into **two columns** (25% / 75%): a compact calendar block on the left and the speed controls on the right.

**Calendar block** (left column, 25%) — the player-facing clock in three rows:

- **Year + quarter** — `1960 Q1` (see `TIME_CONTROLS.md`).
- **Month + day** — `Jan 01`, abbreviated month + zero-padded day.
- **Quarter progress** — a progress bar through the current in-year quarter, labelled with its percentage.

**Speed controls** (right column, 75%) — a raw `Sim` counter with the current multiplier, then the controls:

- **`II`** pauses; **`1`–`5`** set the speed multiplier. The active speed is highlighted.

The clock has three layers — sim tick → day → economy tick — paced so that 1× ≈ 6 s/day and 3× ≈ 2 s/day. The economy resolves quarterly. The authoritative calendar/pacing constants live in `src/core/sim_loop.hpp` and are deliberately tentative.

---

## Explorer — right, middle
**Spec: `EXPLORER.md`**

A panel on the right edge, below the time column and above the minimap. The explorer is the player's **pinning and quick-navigation** surface:

- The player can **pin** UI elements of interest — a unit, a body, a building, a market — and jump straight to them from a single list.
- Acts as a working set of bookmarks across an otherwise large UI, so frequently-revisited things stay one click away.

Not implemented in the prototype; specified here to reserve the region and the interaction.

---

## Ledger windows — fold-out (BL-122) and floating

Since **BL-122** the five named ledgers — **Construction, Economy, Market, Balance, and
Corporations** — no longer float. Each draws as a **pinned, borderless panel filling the
shell column** (`ui::foldout_begin`/`foldout_end`, `src/ui/foldout_column.hpp`) when its
nav slot is active, and folds back to just the icon rail when toggled off or when another
opens (accordion). There is no title-bar close — closing is the nav-rail toggle. The old
uniform floating chrome (`ledger_window_spawn`/`size`, `ImGuiCond_Once` movable windows)
and the Construction panel's BL-082 height-cap no longer apply to these five.

The **Tile Ledger** (History, slot 9) is the exception — it **still floats** as a movable
window (its migration into the column is deferred). The set of menus is described in
**`MENU.md`**.

**All ledgers start closed** on a fresh session — none are shown until the player opens them from the pane (see the policy in `MENU.md`).

Only **broad** ledgers (overviews across many entities) earn a nav-rail slot; targeted, per-entity actions are reached contextually through the Selection info element or a popup, not the rail (see `MENU.md` § Menus are broad ledgers).

### Uniform ledger-window chrome (settled principle)

Every ledger window obeys a **single chrome rule**: they all share **one size and one
spawn anchor**. A ledger opens at the same on-screen position and the same default
extent as every other, so the family reads as one consistent surface rather than a
scatter of differently-sized windows. Concretely, the prototype should drive the
floating ledgers from **one shared size constant and one shared spawn-position
constant** (anchored clear of the profile/header chrome, `ImGuiCond_Once` so the player
may then move/resize freely), rather than each ledger hard-coding its own.

This is now **implemented**: the shared constants `ledger_window_size` and
`ledger_window_spawn` live in `src/ui/ledger_chrome.hpp` (the spawn anchor derived from
the profile-panel dimensions so it clears the top-left chrome), and both the **Tile
Ledger** (`tile_inspector.cpp`) and the **Economy panel** (`economy_panel.cpp`) drive
their `SetNextWindowSize`/`SetNextWindowPos` from them with `ImGuiCond_Once`. This
resolved the prior inconsistency (Tile Ledger 820×560, Economy panel 760×620, at
different offsets). The **Market / Balance / Construction ledger family** (deferred to
Layer 4 — OPENS § Ledger) inherits the same two constants when it is built. The header is
exempt — it is persistent chrome, not a ledger.

> **Superseded by BL-122.** The uniform floating chrome above now applies **only to the
> still-floating Tile Ledger**. The five named ledgers (Construction, Economy, Market,
> Balance, Corporations) fold out into the shell column instead (see *Ledger windows*) and
> no longer read `ledger_window_spawn`/`size`. The **BL-082 Construction-panel height-cap
> is dissolved** — the fold-out column sits entirely left of the Selection element, so the
> panel can no longer occlude the build front door and needs no caller-supplied spawn.

### One-question-per-view splits (BL-117 sweep)

Fold-out ledgers with more than one question split their content across a **button-strip nav**
(`ui::nav_button`, `foldout_column.hpp` — a manual `Selectable`/`Button` strip, since
`ImGui::BeginTabBar` does not render in this build), each view drawing exclusively. The
**Construction** panel (Build / Manage / Sell Orders) and the **Economy** panel (Corps / Holdings /
Markets, `ui_state::economy_view`) use this. The **Balance**, **Market**, **Corporation**, and (still-
floating) **Tile** ledgers were audited and found to be single-question already — no split. The
principle is *one question per view, a menu to move between views* — not a mandate to split every
panel.

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

- **Uniform chrome.** Every ledger drives `SetNextWindowSize` / `SetNextWindowPos` from
  `ledger_window_size` and `ledger_window_spawn` in `src/ui/ledger_chrome.hpp`, with
  `ImGuiCond_Once` so the player may freely move or resize after first open.
- **Player corporation defaulted.** Every ledger that shows per-corporation data defaults
  to `w.player_entity` in its corp selector and offers a selector to view any other
  corporation's figures. Cross-corporation side-by-side comparison is not in scope for
  the prototype.
- **Shared content builders.** Per-entity stat blocks are rendered through the shared
  `entity_summary` helpers (`src/ui/entity_summary.{hpp,cpp}`), which are also used by
  the Selection info element and the hover card. Do not duplicate this logic inside a
  ledger — call the shared builders.
- **Start closed.** All ledger windows open with their initial `open` flag set to `false`
  (policy established in `src/ui/ledger_chrome.hpp` / `src/ui/nav_pane.cpp`). None are
  shown until the player explicitly opens them from the nav pane.

---

## UI popup elements

Beyond the persistent chrome and the floating ledgers, the production UI will use **transient popup elements** — context menus, confirmation dialogs, hover cards, and action prompts that appear in response to a click or hover and dismiss on action or click-away. Examples: right-clicking a unit for an order menu, a "confirm purchase" dialog, a richer hover card than the canvas tooltip.

These are **not implemented next** and have no dedicated spec yet. They are noted here so the layout accounts for content that floats above every region without belonging to any one of them. The canvas hover tooltips (see `CANVASES.md`) are the only popup-like elements in the prototype. Hover cards use the shared `draw_hover_card` dispatcher — see [`TOOLTIP.md`](TOOLTIP.md).

---

## Developer affordances

- **F12** captures the exact composited frame to `build/Debug/screenshots/`. The `tools/capture.ps1` wrapper builds, launches, captures, and converts to PNG in one step.

---

## Prototype / temporary notes

- The explorer is specified but not yet implemented; profile and header are live.
- Nav slot layout (count, ordering, the slot-8 Tile Ledger) is placeholder.
- Canvases are not yet inset clear of the chrome.
- Popup elements (context menus, dialogs) are deferred.
- ImGui is the prototype UI only; the production shell is a later, Lua-driven retained layer (TECH_FOUNDATIONS).

---

## Companion specifications

| Region | Document |
|---|---|
| Profile | `PROFILE.md` |
| Header | `HEADER.md` |
| Navigation pane / menus | `MENU.md` |
| Canvases (overview / ladder) | `CANVASES.md` |
| Solar canvas | `SOLAR.md` |
| Circumplanetary canvas | `CIRCUMPLANETARY.md` |
| Planetary canvas | `PLANETARY.md` |
| Minimap | `MINIMAP.md` |
| Time column | `TIME_CONTROLS.md` |
| Explorer | `EXPLORER.md` |
