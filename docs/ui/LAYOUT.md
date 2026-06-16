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
│ ▢        │     [ overlay lens strip ]          ┌──────────────┐│
│ ▢        │                                     │   Minimap    ││
│          │                                     │ (inactive    ││
│          │                                     │   canvas)    ││
└──────────┴─────────────────────────────────────┴─────────────┘
```

Two layers compose the screen:

- **Background** — the canvases, drawn edge-to-edge via the ImGui background draw list.
- **Foreground** — the ImGui panels below (profile, header, nav pane, explorer, time column, minimap, ledger windows), drawn on top of the canvases.

The header, profile, and explorer are **not yet implemented** — they are specified here ahead of the work so the shell has a settled shape. Layer 2 ships the nav pane, canvases, time column, minimap, and the Tile Ledger window.

---

## Profile — top-left
**Spec: `PROFILE.md`**

A compact panel pinned to the top-left corner, above the navigation pane and aligned to its width. Shows the player corporation at a glance:

- **Corporation portrait** — a small picture/emblem identifying the player faction.
- **Corporation name** and a line or two of basic detail (e.g. parent nation, founding, headline standing).

This is a static identity readout in the prototype — no interaction beyond, eventually, opening a fuller corporation screen. Faction identity is still undecided (see `CONCEPT.md`); the panel reserves the space and the shape of the data.

---

## Header — top
**Spec: `HEADER.md`**

A full-width strip across the top of the canvas area, between the profile and the time column. It is the player's persistent financial dashboard, wired to the live economy as of the Layer 3 finalisation:

- **Balance** — the player corporation's running treasury balance (negatives flagged red).
- **Stockpile valuation** — an estimated liquid value of everything the player holds: its `(corporation, body)` pools summed at each body's current market price. A single money figure, not a per-resource inventory.
- **Net + trend** — the last economy tick's net change as a coloured per-quarter figure, alongside a small sparkline of recent balances.

The header answers "can I afford this, and which way is it trending?" without opening a ledger. Detailed, per-body breakdowns stay in their respective ledgers.

---

## Navigation pane — left
**Spec: `MENU.md`**

A fixed, full-height **icon rail** pinned to the left edge (`nav_pane_width`, currently 56 px), below the profile. Cannot move, resize, or collapse. The rail is narrow; the profile keeps its own (wider) `profile_panel_width` above it rather than matching the rail.

- Holds a vertical strip of **ten square icon slots** — the home for the game's menus and ledgers. Each slot shows a **vector glyph** (`src/ui/icons.hpp`) instead of a worded label, with the menu name in a hover tooltip.
- Each slot toggles a panel open/closed; the active slot is highlighted.
- **Layer 2 wires only one slot: the Tile Ledger** (a ruled-table glyph), parked at slot 8. The other nine are reserved, disabled placeholders (a neutral hollow-square glyph). Slot placement is temporary while canvas work takes priority over menu design.

---

## Canvas area — centre
**Spec: `CANVASES.md`**

Three canvases — Solar, Circumplanetary, Planetary — form a **zoom ladder** and share the window. One is **primary** (fills the window) and the rung one step *out* from it is shown in the **minimap** (a fixed inset, bottom-right). The player **descends** (zooms in) by clicking a body in the primary canvas and **ascends** (zooms out) by clicking the minimap. Full detail — visual language, coordinate mapping, interaction, and the ladder navigation rules — lives in **`CANVASES.md`**.

The canvases render behind the foreground panels, so the chrome currently occludes the edges of the primary canvas. Insetting the primary region clear of the chrome is a known follow-up.

---

## Minimap — bottom-right inset
**Spec: `MINIMAP.md`**

A fixed inset in the bottom-right corner showing the **zoom-out neighbour** of the primary canvas at reduced scale, framed by its own chrome — a title bar (the viewed body, or the star name; the game name at the top rung) above the inset. Clicking it **ascends** one rung. It shares the primary canvases' drawing code, parameterised by region size. The overlay-lens controls that once sat in a mode bar below the inset now live in a bottom-left **overlay control strip** (see below). `MINIMAP.md` is the authoritative spec for the minimap chrome and the ladder navigation; `CANVASES.md` covers the shared drawing path.

---

## Overlay control strip — bottom-left
**Spec: `CANVASES.md` / `MINIMAP.md`**

A small horizontal strip pinned to the bottom-left of the shell, running from the
nav-rail edge inward toward the centre (clear of the centred scale/zoom control on
the Solar and Circumplanetary canvases). It toggles the **canvas overlay lens** —
a labelled button per mode (Supply / Market / Faction); the active lens is
highlighted, and clicking it again clears the overlay. This replaces the former
minimap mode-bar dots. A default lens is active on load (the supply lens) rather
than no overlay. See `overlay.hpp` (`draw_overlay_controls`).

## Selection info element — bottom-left, above the overlay strip
**Spec: `SELECTION.md`**

A **pinned** panel docked in the bottom-left, directly **above the overlay lens /
zoom control strip**. It shows detail about the **current selection** — whatever
entity the player last single-clicked — and is **polymorphic by selection kind**
(body, tile, building, market, unit, and later nation / corporation / logistics
vessel), each rendering its own stat block.

Its header carries a **'go to'** button (equivalent to a double-click on the
selection; routes through `ui::focus_on_entity` — navigates a canvas for spatial
entities, opens a ledger for non-spatial ones) and a **close** button (which
*hides* the panel; it reappears on the next selection).

Unlike the floating ledgers it is **not** reachable from the navigation rail —
**selecting an entity is the only way to open it.** It introduces a click-model
change shared across all canvases: **single-click selects** (fills this panel,
no view change), **double-click navigates**. See `SELECTION.md` and `CANVASES.md`.

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

## Ledger windows — floating

Menus opened from the nav pane appear as floating, movable, closable ImGui windows over the canvas area (rather than docking into the pane). The only one present in Layer 2 is the **Tile Ledger** (opened by tab 8): body selector, per-tile table, building list, and market readout. Its **✕** fully closes it; reopen from the tab. The set of menus that open these windows is described in **`MENU.md`**.

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

- Profile, header, and explorer are specified but not yet implemented.
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
