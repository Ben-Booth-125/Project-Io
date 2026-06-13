# Project Io — UI Layout

Surface-level description of the application **shell** — the persistent chrome arranged around the two canvases. This document covers *where things sit and what they are for*, not their internals. Each region below links to its own detailed specification where one exists. For the canvases themselves see **`CANVASES.md`**; for the tick model see `src/core/sim_loop.hpp`.

The prototype UI is built with Dear ImGui (see TECH_FOUNDATIONS). Everything here is a debugging-grade layout that doubles as the functional specification for the eventual production shell — it is expected to be revised.

---

## Screen regions

```
┌──────────┬────────────────────────────────────┬──────────────┐
│ Profile  │              Header                 │  System tick │
├──────────┤      (budget + resource strip)      ├──────────────┤
│          ├────────────────────────────────────┤ Speed controls│
│   Nav    │                                     │              │
│   pane   │         Primary canvas              │              │
│          │      (Solar System or Surface)      ├──────────────┤
│  1.      │                                     │   Explorer   │
│  ...     │     [ floating ledger windows ]     │  (pinned     │
│  8. Tile │                                     │   shortcuts) │
│   Ledger │                                     │              │
│  ...     │                                     ┌──────────────┐│
│  10.     │                                     │   Minimap    ││
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

A full-width strip across the top of the canvas area, between the profile and the time column. It is the player's persistent financial and material dashboard:

- **Budget** — current treasury balance and, eventually, net income/expenditure per economy tick.
- **Resource overview** — a quick strip of the player's headline stockpiles. For the prototype this is **deliberately scarce**: a small handful of resources, summed across all holdings, shown as icon + quantity.

The header answers "can I afford this, and what do I have?" without opening a ledger. Detailed, per-body breakdowns stay in their respective ledgers.

---

## Navigation pane — left
**Spec: `MENU.md`**

A fixed, full-height column pinned to the left edge (`nav_pane_width`, currently 200 px), below the profile. Cannot move, resize, or collapse.

- Holds a vertical strip of **ten numbered tab slots** — the home for the game's menus and ledgers.
- Each tab toggles a panel open/closed; the active tab is highlighted.
- **Layer 2 wires only one tab: `8. Tile Ledger`**, parked at slot 8. The other nine are reserved, disabled placeholders. Slot numbering and placement are temporary while canvas work takes priority over menu design.

---

## Canvas area — centre
**Spec: `CANVASES.md`**

The two canvases share the window. One is **primary** (fills the window) and the other is the **minimap** (a fixed inset, bottom-right). Clicking the minimap, or clicking a body in the Solar System Canvas, swaps which is primary. Full detail — visual language, coordinate mapping, interaction, and the primary/minimap swap rules — lives in **`CANVASES.md`**.

The canvases render behind the foreground panels, so the chrome currently occludes the edges of the primary canvas. Insetting the primary region clear of the chrome is a known follow-up.

---

## Minimap — bottom-right inset
**Spec: `MINIMAP.md`**

A fixed inset in the bottom-right corner showing the **inactive** canvas at reduced scale. Clicking it swaps primary and minimap. It shares the primary canvases' drawing code, parameterised by region size, so it is documented in depth alongside them in **`CANVASES.md`**; `MINIMAP.md` collects the shell-level behaviour (placement, sizing, swap interaction).

---

## Time column — top-right
**Spec: `TIME_CONTROLS.md`**

A two-panel stack in the top-right corner, both the same width as the minimap so the right edge stays aligned.

**System tick** (top) — a permanent, non-interactive readout. Shows the player-facing clock:

- **Day** — in-game days elapsed.
- **Econ** — economy ticks (quarters) elapsed.

**Speed controls** (below) — the time controls and a raw `Sim` counter with the current multiplier:

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

---

## UI popup elements

Beyond the persistent chrome and the floating ledgers, the production UI will use **transient popup elements** — context menus, confirmation dialogs, hover cards, and action prompts that appear in response to a click or hover and dismiss on action or click-away. Examples: right-clicking a unit for an order menu, a "confirm purchase" dialog, a richer hover card than the canvas tooltip.

These are **not implemented next** and have no dedicated spec yet. They are noted here so the layout accounts for content that floats above every region without belonging to any one of them. The canvas hover tooltips (see `CANVASES.md`) are the only popup-like elements in the prototype.

---

## Developer affordances

- **F12** captures the exact composited frame to `build/Debug/screenshots/`. The `tools/capture.ps1` wrapper builds, launches, captures, and converts to PNG in one step.

---

## Prototype / temporary notes

- Profile, header, and explorer are specified but not yet implemented.
- Nav slot layout (count, ordering, the slot-8 Tile Ledger) is placeholder.
- Canvases are not yet inset clear of the chrome.
- No date formatting yet — the tick readout shows raw Day/Econ counts.
- Popup elements (context menus, dialogs) are deferred.
- ImGui is the prototype UI only; the production shell is a later, Lua-driven retained layer (TECH_FOUNDATIONS).

---

## Companion specifications

| Region | Document |
|---|---|
| Profile | `PROFILE.md` |
| Header | `HEADER.md` |
| Navigation pane / menus | `MENU.md` |
| Canvases | `CANVASES.md` |
| Minimap | `MINIMAP.md` |
| Time column | `TIME_CONTROLS.md` |
| Explorer | `EXPLORER.md` |
