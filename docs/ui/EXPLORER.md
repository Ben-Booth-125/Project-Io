# Project Io — Explorer

The **explorer** is a panel on the right edge of the shell, in the middle band — below the time column and above the minimap. It is the player's pinning and quick-navigation surface. See `LAYOUT.md` for placement.

This document is a placeholder to be expanded; the notes below record current understanding.

---

## Purpose

Keep frequently-revisited things one click away in an otherwise large UI. The explorer is a working set of bookmarks across the whole game state.

## Contents and behaviour

- **Pinned items** — the player pins UI elements of interest: a unit, a body, a building, a market, or any other navigable entity.
- **Quick navigation** — clicking a pinned entry jumps straight to that element (e.g. selects the body and brings its surface canvas forward, or focuses a unit).
- The list is the player's own curation — what they choose to keep at hand, not an automatic feed.

## Open questions

- What categories of element can be pinned in the first pass (units? bodies? buildings? markets?).
- How pinning is triggered — context menu, drag, a pin affordance on the element itself.
- Ordering and grouping of pins (manual, by type, by recency).
- Persistence — are pins saved with the campaign?

## Related

- `LAYOUT.md` — placement in the shell.
- `CANVASES.md` — navigation targets jump into the canvases (body selection, surface focus).
- `MENU.md` — the nav pane is the other primary navigation surface.
