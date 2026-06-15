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
slot (see `docs/development/TODO.md` § Ledger / § Selection info element).

## Layer 2 state

- Only one slot is wired: the **Tile Ledger** (a ruled-table glyph), parked at slot 8. It opens the Tile Ledger window — body selector, per-tile table, building list, and market readout.
- The other nine slots are reserved, **disabled placeholders** (a neutral hollow-square glyph).
- Slot glyphs and placement are **temporary** — the real per-menu icons follow once the menu set is defined; menu design is deprioritised while canvas work takes priority.

## Menu set and ordering (2026-06-15)

> **⟳ Pending review (2026-06-15) — transient.** Defined the ten-slot menu set and its
> gameplay-loop ordering ([B3]). The **ordering principle is gameplay-loop grouping** for now; a
> low-priority Brief tracks settling a *canonical* ordering rule later (TODO § Menu). The
> **Corporation overview dashboard** (slot 1) is a new surface that still needs its own design
> Brief. Remove this note once reviewed.

The ten slots are derived from the game systems (`docs/SYSTEMS.md`), filtered through the
**menus-are-broad-ledgers** rule above: each slot is a broad overview surface, never a targeted
action. They are **grouped by gameplay loop** (not by SYSTEMS.md tier order — see open question
below), with a thin visual separator between clusters:

| # | Slot | System / source | Cluster |
|---|---|---|---|
| 1 | **Corporation overview** (dashboard) | the player corporation at a glance | *anchor* |
| 2 | **Balance Ledger** | Budget ([A4]) | **manage** |
| 3 | **Construction / Buildings overview** | Infrastructure ([A4]/[F4]) | **manage** |
| 4 | **Workforce / Population Ledger** | Workforce ([A4]) / Population ([S4]) | **manage** |
| 5 | **Market Ledger** | Trade ([A4]) | **trade & world** |
| 6 | **Tile Ledger** | Environment (exists; moves here from slot 8) | **trade & world** |
| 7 | **Research** | Research | **strategy** |
| 8 | **Policy** | Policy | **strategy** |
| 9 | **Diplomacy** | Diplomacy | **strategy** |
| 10 | **Exploration** | Exploration (may instead route to the Explorer surface, `EXPLORER.md`) | **strategy** |

Notes on the mapping:

- **Resources, Supply, and Conflict do not get their own slot.** Resource detail lives in the
  Market and Tile ledgers; Supply (Layer 5 logistics) folds into Construction/Market when it
  exists; Conflict has no broad ledger yet. The rail scales with *systems that have a broad
  surface*, per the menus-are-broad-ledgers rule.
- **Slot 1 — Corporation overview dashboard.** A top-level roll-up (balance, holdings, alerts)
  above the per-system ledgers. It is a **new surface needing its own design** before
  implementation — tracked as a Brief under TODO § Menu.
- **Layer-4 ledgers** (slots 2–6) are the near-term build (the [A4] ledger family); slots 7–10
  are reserved placeholders until their systems land, following the *ledgers-start-closed* and
  reserved-placeholder conventions above.

## Open questions

- **Canonical ordering rule (low priority).** The current order is *gameplay-loop grouping*; a
  canonical, self-documenting rule (e.g. strict SYSTEMS.md tier order) is deferred. Tracked as a
  low-priority Brief under TODO § Menu.
- Whether all menus open floating windows, or some become docked/persistent panels.
- Relationship to the explorer (`EXPLORER.md`) — the pane is fixed navigation; the explorer is curated navigation. Slot 10 (Exploration) may route to it rather than open a ledger.

## Related

- `LAYOUT.md` — placement in the shell.
- `CANVASES.md` — the Tile Ledger (slot 8) reads from the selected body/tile.
- `EXPLORER.md` — the other primary navigation surface.
