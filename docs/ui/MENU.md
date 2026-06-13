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

## Layer 2 state

- Only one slot is wired: the **Tile Ledger** (a ruled-table glyph), parked at slot 8. It opens the Tile Ledger window — body selector, per-tile table, building list, and market readout.
- The other nine slots are reserved, **disabled placeholders** (a neutral hollow-square glyph).
- Slot glyphs and placement are **temporary** — the real per-menu icons follow once the menu set is defined; menu design is deprioritised while canvas work takes priority.

## Open questions

- The final set of menus and what each contains (the ten slots are not yet assigned meaning beyond slot 8).
- Ordering and grouping of menus once the set is known.
- Whether all menus open floating windows, or some become docked/persistent panels.
- Relationship to the explorer (`EXPLORER.md`) — the pane is fixed navigation; the explorer is curated navigation.

## Related

- `LAYOUT.md` — placement in the shell.
- `CANVASES.md` — the Tile Ledger (slot 8) reads from the selected body/tile.
- `EXPLORER.md` — the other primary navigation surface.
