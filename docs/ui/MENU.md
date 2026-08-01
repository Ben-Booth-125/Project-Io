# Project Io — Menus (Navigation Pane)

The **navigation pane** is a fixed, full-height **icon rail** pinned to the left edge of the shell (below the profile, `nav_pane_width` currently 56 px). It is the home for the game's menus and ledgers. See `LAYOUT.md` for placement.

This document is a placeholder to be expanded; the notes below record current understanding.

---

## Structure

- A vertical strip of **nine square icon slots** (`nav_pane.cpp`; BL-174 dropped the glyph-less tenth). **Five are live** — Corporation overview, Budget, Market Ledger, Construction, History; **four are reserved** — Workforce, Research, Corp. Strategy, Diplomacy — disabled, but each carrying its own dimmed glyph so the rail teaches the shape of the game rather than showing a row of identical blanks (BL-174, nav-rail legibility).
- Each slot shows a **vector glyph** (`src/ui/icons.hpp`) instead of a worded label; the slot's name plus a one-line blurb is shown in a wrapping hover tooltip (BL-174). The rail is deliberately narrow — the profile above keeps its own (wider) `profile_panel_width` rather than matching the rail.
- Each slot toggles a panel open/closed; the open slot lights its glyph in the selection accent — the same idiom as the minimap lens bar, so the two icon strips read as one vocabulary.
- Opened menus **fold out into the shell column** to the rail's right (`foldout_begin`, `src/ui/foldout_column.hpp` — see `LAYOUT.md` § Ledger windows). **Nothing floats and there is no ✕**: closing is the toggle — re-click the slot, re-click the active sub-view tab (BL-126 toggle rule), or open another slot (accordion, `close_all_panels`).

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
no slot. This keeps the nine slots scaling with the *systems* the game has, not with the number
of things a player can do to a single entity — e.g. the per-tile "build here" flow lives in the
tile Selection element, while the broad **buildings overview** is what earns the construction
slot (see `docs/development/BACKLOG.md` § Ledger / § Selection info element).

## Superseded: Layer 2 state

> **Superseded by BL-174 (nav-rail legibility, landed).** The Layer 2 rail wired only the
> Tile Ledger at slot 8, with nine disabled hollow-square placeholders and glyphs framed
> as temporary. The rail is now the nine-slot, five-live, per-slot-glyph strip described
> under § Structure; the Tile Ledger's content lives under the **History** slot (slot 9,
> BL-211 — Story / Chain / Tiles) and docks in the fold-out column like everything else.

## Menu set and ordering (2026-06-15)

The slots are derived from the game systems (`docs/SYSTEMS.md`), filtered through the
**menus-are-broad-ledgers** rule above: each slot is a broad overview surface, never a targeted
action. The **settled order is the curated player-facing sequence below** (2026-06-15, [C1] — a
deliberate gameplay order, not strict SYSTEMS.md tier order). It is a **nine-slot rail**:
Exploration drops off the rail. (It originally routed to the Explorer surface — **superseded
2026-07-26 by BL-205 (corp chat log)**; `EXPLORER.md` is retired and the comms surface is
`CHAT.md`.) The `tier-idx` column records each slot's position in the SYSTEMS.md tier
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
- **Slot 1 — Corporation overview dashboard (built 2026-08-01, BL-248).** Four roll-up cards —
  **Production**, **Trade**, **Workforce**, **Finance** — folded out into the shell column
  (`src/ui/corporation_dashboard.{hpp,cpp}`). Each card **rests as one verdict line** and
  expands, through the shared drill-through chevron (BL-214, `LAYOUT.md` § Drill-through), to a
  **full-screen view** carrying its chart, that chart's **question log** (BL-247), and a
  **per-item drill** with a breadcrumb back to the roll-up.

  The four drills are deliberately four *different shapes*, not one generic detail panel: a
  building's operating economics (through the shared `draw_building_profit` builder), a lane's
  completed convoys and recency, a building's assigned-against-effective labour, and — for
  Finance, whose subject is not per-item — the budget's five flows. Every figure is derived from
  the live world and the last `economy_report`; nothing is carried over from the exemplar's
  invented scenario data.

  > **Supersedes the MVP set (Ben, 2026-07-31).** The original design named **balance + last-tick
  > delta**, a **holdings roll-up**, **workforce contention** and **alerts**, with a fuller
  > dashboard pass flagged for v0.1.1. This *is* that pass, and Ben chose the exemplar's four
  > roll-ups over the MVP four when asked directly. The MVP's **click-through into the relevant
  > per-system ledger** is not lost — it survives as the **host axis** (`[>]` / `focus_on_entity`),
  > available *alongside* the drill rather than instead of it, the two being orthogonal per
  > BL-214's three-axis model. The **alert** concept the MVP introduced is not built: an idle
  > building and a negative net read as red verdicts on their own cards, which is the same signal
  > without a second mechanism to maintain.

  The slot previously drew an **all-corporations balance table**, which was neither this nor the
  MVP — a cross-corp comparison surface, and a duplicate of the **Economy panel's Corps view**
  that still carries it. It was retired with this item rather than left as a second home.

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
    **Built 2026-07-29 (BL-211):** the slot now splits into **Story / Chain / Tiles** — the
    body's dated biography, the generation stage charts (one collapsing accordion per chain
    stage, under the wizard's three round tabs), and the tile/building/market tables. The
    charts come from `ui::generation_charts`, shared verbatim with the New World wizard, so
    the plots a player decided a world on are the plots they can reopen mid-campaign.
    **Still owed:** exploration-gating (the slot is ungated today) and the post-generation
    *advisory* read.
  — **Construction / Buildings** (slot 6): **construction-in-progress only** — a build-queue /
    progress surface (what is being built, its cost and progress). Own-building *inventory* stays
    in the dashboard holdings roll-up; this slot is the *active construction* view (the
    Construction Ledger of the [A4] family).

## Application / system menu — session control (BL-070)

Separate from the nav-rail ledgers above (per-system overviews) and from the display-options
surface below (BL-076). This is the **session shell** — start/pause/exit — the application-level
control the player reaches without knowing a hotkey. **Landed in v0.0.9** (`src/core/app.cpp`,
`src/ui/ui_state.hpp`).

- **Surface — corner gear (hamburger) button.** A small three-line glyph pinned to the **top-right**,
  just left of the time column, clear of the header content and the time panel. Lowest layout
  disruption, no permanent chrome, reuses the icon-rail visual language. (Option B of the design;
  the top menu-bar and the centred pause-modal alternatives were rejected for the prototype
  minimum — the centred modal is the right long-term home once a title screen lands.)
- **Contents (prototype minimum).** **Pause / Resume** and **Exit Game**. Pause mirrors the
  Space-hotkey pause — popup and keyboard drive **one** shared session-control flag (routed through
  the same `pause_toggle` path / `sim_loop` speed-0 + remembered speed), not two. Exit is
  **destructive** (there is no save), so it arms an **inline "Really quit?" confirm** in the same
  popup rather than quitting on the first click.
- **Keyboard parity.** **Esc** toggles the same popup (handled ahead of the ImGui keyboard guard so
  it works while the popup or another panel holds focus); an armed exit-confirm backs out first, so
  Esc cancels the confirm before it closes the menu.
- **Reserved home.** New/Restart and **Save/Load** (once serialisation is player-exposed) are the
  named follow-ons for this popup — it is the reserved session-control home. The **title screen
  has since landed** as the app's main menu (the `app_screen` entry flow — main menu → New World
  wizard → in-game; see `STARTUP.md`, 2026-07-31); routing this popup's Exit/Restart back to it
  is the remaining join.

## Application / system menu — display options (BL-076)

Separate from the nav-rail ledgers above (per-system overviews) and from the in-app session menu
(BL-070, above — exit / pause). This is the **display-settings surface**.

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

- ~~Whether all menus open floating windows, or some become docked/persistent panels.~~
  **Answered:** all dock — every ledger folds out into the shell column (BL-122; the Tile
  Ledger followed). No floating ledgers remain.
- ~~Relationship to the explorer (`EXPLORER.md`).~~ **Superseded 2026-07-26 (BL-205, corp
  chat log):** the Explorer is retired; the comms surface (`CHAT.md`) took its place, and
  exploration stays off the rail.

## Related

- `STARTUP.md` — the app's entry screens (main menu → New World wizard → in-game).
- `LAYOUT.md` — placement in the shell.
- `CANVASES.md` — the History ledger's Tiles view (slot 9) reads from the selected body/tile.
- `CHAT.md` — the comms surface that superseded the Explorer (BL-205).
