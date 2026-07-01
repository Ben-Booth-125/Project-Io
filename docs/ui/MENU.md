# Project Io — Menus (Navigation Pane)

The **navigation pane** is a fixed, full-height **icon rail** pinned to the left edge of the shell (below the profile, `nav_pane_width` currently 56 px). It is the home for the game's menus and ledgers. See `LAYOUT.md` for placement.

This document is a placeholder to be expanded; the notes below record current understanding.

---

## Structure

- A vertical strip of **ten square icon slots**.
- Each slot shows a **vector glyph** (`src/ui/icons.hpp`) instead of a worded label; the menu name is shown in a hover tooltip. The rail is deliberately narrow — the profile above keeps its own (wider) `profile_panel_width` rather than matching the rail.
- Each slot toggles a panel open/closed; the active slot is highlighted.
- Opened menus appear as **floating, movable, closable ImGui ledger windows** over the canvas area (they do not dock into the pane). Closing a window with its **✕** is equivalent to toggling the slot off.

### Policy: ledgers start closed

**Every ledger starts closed on a fresh session.** None are shown until the
player opens them from the navigation pane. This keeps the canvas unobstructed
by default and makes opening a ledger a deliberate act. New ledgers added later
must follow this — default their open-state to closed.

## Menus are broad ledgers

A nav-rail slot is **reserved UI**, and reserved UI is justified only by a **broad** ledger —
a surface that gives an overview *across many entities*: all of the player's buildings, all of
a body's market, the whole budget. **Specific, targeted ledgers do not get a reserved menu
slot.** A targeted action — building on *one* tile, inspecting *one* market listing, managing
*one* building — is reached **contextually**, through the Selection info element
(`SELECTION.md`) or a transient popup (`LAYOUT.md` § UI popup elements), not through the rail.

The test when deciding whether a new surface earns a slot: *is this a broad overview, or a
targeted action?* Broad → a ledger with a slot. Targeted → the Selection element / a popup,
no slot. This keeps the ten slots scaling with the *systems* the game has, not with the number
of things a player can do to a single entity — e.g. the per-tile "build here" flow lives in the
tile Selection element, while the broad **buildings overview** is what earns the construction
slot (see `docs/development/BACKLOG.md` § Ledger / § Selection info element).

## Layer 2 state

- Only one slot is wired: the **Tile Ledger** (a ruled-table glyph), parked at slot 8. It opens the Tile Ledger window — body selector, per-tile table, building list, and market readout.
- The other nine slots are reserved, **disabled placeholders** (a neutral hollow-square glyph).
- Slot glyphs and placement are **temporary** — the real per-menu icons follow once the menu set is defined; menu design is deprioritised while canvas work takes priority.

## Menu set and ordering (2026-06-15)

The slots are derived from the game systems (`docs/SYSTEMS.md`), filtered through the
**menus-are-broad-ledgers** rule above: each slot is a broad overview surface, never a targeted
action. The **settled order is the curated player-facing sequence below** (2026-06-15, [C1] — a
deliberate gameplay order, not strict SYSTEMS.md tier order). It is a **nine-slot rail**:
Exploration drops off the rail (it routes to the Explorer surface, `EXPLORER.md`, rather than
opening a ledger). The `tier-idx` column records each slot's position in the SYSTEMS.md tier
list, so the curation is auditable.

| # | Slot | tier-idx | System / source |
|---|---|---|---|
| 1 | **Corporation overview** (dashboard) | 0 | the player corporation at a glance |
| 2 | **Budget** *(was Balance Ledger — scope Q&A)* | 2 | Budget ([A4]) |
| 3 | **Workforce / Population Ledger** | 4 | Workforce ([A4]) / Population ([S4]) |
| 4 | **Research** | 7 | Research |
| 5 | **Market Ledger** | 1 | Trade ([A4]) |
| 6 | **Construction / Buildings** *(identity Q&A — buildings folded out)* | 3 | Infrastructure |
| 7 | **Corp. Strategy** *(was Policy — scope Q&A)* | 8 | Policy |
| 8 | **Diplomacy** | 9 | Diplomacy |
| 9 | **History** *(was Tile Ledger — scope Q&A)* | 6 | Environment |

Three slots are **renamed from their source-ledger name**, each broadening scope (Budget,
Corp. Strategy, History); those scope changes are settled in a Q&A and recorded per-slot below.

Notes on the mapping:

- **Resources, Supply, and Conflict do not get their own slot.** Resource detail lives in the
  Market and Tile ledgers; Supply (Layer 5 logistics) folds into Construction/Market when it
  exists; Conflict has no broad ledger yet. The rail scales with *systems that have a broad
  surface*, per the menus-are-broad-ledgers rule.
- **Slot 1 — Corporation overview dashboard (design settled 2026-06-15, [B3]).** A top-level
  roll-up opened as a **floating window** (consistent with the ledger family; no permanent chrome
  cost). The MVP surfaces four roll-ups: **balance + last-tick delta**, a **holdings roll-up**
  (building count / bodies present — this is where the player reads *their own* buildings; see
  the buildings note below), **workforce contention**, and **alerts** (idle buildings, unsold
  output, negative cashflow — note this introduces an *alert* concept). A **fuller dashboard
  design pass is flagged for v0.1.1** — the four roll-ups are the MVP set, not the final
  composition. Its lines are intended to **click through** into the relevant per-system ledger.

- **Buildings have no dedicated ledger (settled 2026-06-15, [F4]).** A standalone buildings
  overview proved more "good to know" than goal-driving, so it is **dropped as a reserved slot**.
  Buildings are read in the two places a player cares about them: **own buildings** ("good for
  me") in the **Corporation overview dashboard** (slot 1, the holdings roll-up); **competitors'
  buildings** ("competition") in the **Market Ledger**. This collapses the old slot-3
  "Construction / Buildings overview" — the slot table above predates this call and is reconciled
  in the pending **nav-rail ordering** pass (BACKLOG § Menu); construction *in progress* still needs
  a home, folded into the dashboard/market surfaces rather than its own ledger.
- **Layer-4 ledgers** are the near-term build (the [A4] ledger family — Budget, Workforce,
  Market, and the Corporation dashboard); the strategy slots (Corp. Strategy, Diplomacy) are
  reserved placeholders until their systems land, following the *ledgers-start-closed* and
  reserved-placeholder conventions above.
- **Scope-changing renames (settled by Q&A 2026-06-15).**
  — **Budget** (slot 2, was Balance Ledger): broadens to the full **Budget-system** surface —
    income vs expenditure broken down (sales, input purchases, maintenance, wages) **plus budget
    allocation** across research / military / workforce contracts, not just the running balance.
    Supersedes the [A4] "Balance Ledger" framing: same money-loop data, wider remit.
  — **Corp. Strategy** (slot 7, was Policy): broadens Policy into a **standing-strategy** surface
    — the player's "laws" / pre-set options, with **wage levels** and **military posture** the
    archetypal levers, and **possibly a goal / quest system** later. Scope deliberately left
    **open** beyond the standing-rules core.
  — **History** (slot 9, was Tile Ledger): **generation history**, not a live event log. For
    v0.1.0 it surfaces the **procedural generation as a number-crunch** — the world as generated.
    It also gives **post-generation advisory** detail: reading the state of resources and
    workforce after generation and approaching *advice* from it. **Gated by exploration** (you
    see the history of what you have explored) but a **distinct informational set**. Closely
    relates to the **Generation Ledger** (`../generation/GENERATION_LEDGER.md`) — likely the same
    rail surface. **Open note:** lengthen the pre-game generation phase and design how that
    history is *presented*; per-tile environment inspection moves to the tile Selection element.
  — **Construction / Buildings** (slot 6): **construction-in-progress only** — a build-queue /
    progress surface (what is being built, its cost and progress). Own-building *inventory* stays
    in the dashboard holdings roll-up; this slot is the *active construction* view (the
    Construction Ledger of the [A4] family).

## Application / system menu — display options (BL-076)

Separate from the nav-rail ledgers above (per-system overviews) and from the future in-app system
menu (BL-070, session control: exit / pause). This is the **display-settings surface**.

- **Options window** — toggled by **F10** (`canvas_command::options_toggle`; appears in the F1
  cheat-sheet). A floating ImGui window with a **Display** section: resolution preset combo
  (1280×720 / 1600×900 / 1920×1080 / 2560×1440, "Custom" when the live size matches no preset),
  **Fullscreen** (borderless-desktop) and **VSync** toggles, a live `Window: W×H` readout, and
  Close. Resolution is disabled while fullscreen is on.
- **Persistence** — settings are stored in a flat `options.cfg` (key=value: `window_w`, `window_h`,
  `fullscreen`, `vsync`) in the working directory, loaded at interactive startup only (never in the
  `--verify` path, so golden captures stay at the fixed default). Preset/toggle changes save
  immediately; a free drag-resize of the window frame is remembered and flushed on clean exit.
- **Follow-ons** — this surface is the natural home for later non-display options (UI scale, font
  size, monitor selection) and would sit beside a session menu (BL-070) and Save/Load once
  serialisation is player-exposed.

## Open questions

- Whether all menus open floating windows, or some become docked/persistent panels.
- Relationship to the explorer (`EXPLORER.md`) — the pane is fixed navigation; the explorer is
  curated navigation. **Exploration is off the rail** (slot dropped) and routes to the Explorer
  surface rather than opening a ledger.

## Related

- `LAYOUT.md` — placement in the shell.
- `CANVASES.md` — the Tile Ledger (slot 8) reads from the selected body/tile.
- `EXPLORER.md` — the other primary navigation surface.
