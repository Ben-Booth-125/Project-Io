# Project Io — UI Layout

Surface-level description of the application **shell** — the persistent chrome arranged around the two canvases. This document covers *where things sit and what they are for*, not their internals. For the canvases themselves see **`CANVASES.md`**; for the tick model see `src/core/sim_loop.hpp`.

The prototype UI is built with Dear ImGui (see TECH_FOUNDATIONS). Everything here is a debugging-grade layout that doubles as the functional specification for the eventual production shell — it is expected to be revised.

---

## Screen regions

```
┌──────────┬────────────────────────────────────┬──────────────┐
│          │                                     │  System tick │
│   Nav    │                                     ├──────────────┤
│   pane   │         Primary canvas              │ Speed controls│
│          │      (Solar System or Surface)      │              │
│  1.      │                                     │              │
│  ...     │     [ floating ledger windows ]     │              │
│  8. Tile │                                     │              │
│   Ledger │                                     │              │
│  ...     │                                     ┌──────────────┐│
│  10.     │                                     │   Minimap    ││
│          │                                     │ (inactive    ││
│          │                                     │   canvas)    ││
└──────────┴─────────────────────────────────────┴─────────────┘
```

Two layers compose the screen:

- **Background** — the canvases, drawn edge-to-edge via the ImGui background draw list.
- **Foreground** — the ImGui panels below (nav pane, time column, ledger windows), drawn on top of the canvases.

---

## Navigation pane — left

A fixed, full-height column pinned to the left edge (`nav_pane_width`, currently 200 px). Cannot move, resize, or collapse.

- Holds a vertical strip of **ten numbered tab slots** — the home for the game's menus and ledgers.
- Each tab toggles a panel open/closed; the active tab is highlighted.
- **Layer 2 wires only one tab: `8. Tile Ledger`**, parked at slot 8. The other nine are reserved, disabled placeholders. Slot numbering and placement are temporary while canvas work takes priority over menu design.

---

## Canvas area — centre

The two canvases share the window. One is **primary** (fills the window) and the other is the **minimap** (a fixed inset, bottom-right). Clicking the minimap, or clicking a body in the Solar System Canvas, swaps which is primary. Full detail — visual language, coordinate mapping, interaction, and the primary/minimap swap rules — lives in **`CANVASES.md`**.

The canvases render behind the foreground panels, so the nav pane and the time column currently occlude the leftmost sliver and the top-right corner of the primary canvas. Insetting the primary region clear of the chrome is a known follow-up.

---

## Time column — top-right

A two-panel stack in the top-right corner, both the same width as the minimap so the right edge stays aligned.

**System tick** (top) — a permanent, non-interactive readout. Shows the player-facing clock:

- **Day** — in-game days elapsed.
- **Econ** — economy ticks (quarters) elapsed.

**Speed controls** (below) — the time controls and a raw `Sim` counter with the current multiplier:

- **`II`** pauses; **`1`–`5`** set the speed multiplier. The active speed is highlighted.

The clock has three layers — sim tick → day → economy tick — paced so that 1× ≈ 6 s/day and 3× ≈ 2 s/day. The economy resolves quarterly. The authoritative calendar/pacing constants live in `src/core/sim_loop.hpp` and are deliberately tentative.

---

## Ledger windows — floating

Menus opened from the nav pane appear as floating, movable, closable ImGui windows over the canvas area (rather than docking into the pane). The only one present in Layer 2 is the **Tile Ledger** (opened by tab 8): body selector, per-tile table, building list, and market readout. Its **✕** fully closes it; reopen from the tab.

---

## Developer affordances

- **F12** captures the exact composited frame to `build/Debug/screenshots/`. The `tools/capture.ps1` wrapper builds, launches, captures, and converts to PNG in one step.

---

## Prototype / temporary notes

- Nav slot layout (count, ordering, the slot-8 Tile Ledger) is placeholder.
- Canvases are not yet inset clear of the chrome.
- No date formatting yet — the tick readout shows raw Day/Econ counts.
- ImGui is the prototype UI only; the production shell is a later, Lua-driven retained layer (TECH_FOUNDATIONS).
