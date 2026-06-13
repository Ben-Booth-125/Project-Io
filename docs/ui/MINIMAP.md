# Project Io — Minimap

The **minimap** is a fixed inset in the bottom-right corner of the shell, showing the **inactive** canvas at reduced scale. Clicking it swaps which canvas is primary. See `LAYOUT.md` for placement and `CANVASES.md` for the full drawing and interaction detail.

This document collects the shell-level behaviour. The minimap is not a separate render path — it shares the primary canvases' drawing code, parameterised by region size — so its visual specifics live in `CANVASES.md`.

---

## Behaviour

- **Shows the inactive canvas.** When the Solar System Canvas is primary, the minimap shows the Body Surface Canvas, and vice versa.
- **Swap on click.** Clicking inside the minimap region swaps primary and minimap; selection state is unchanged. Clicking a body in the Solar System (whether primary or minimap) additionally selects it and brings the surface forward. See `CANVASES.md` for the exact swap rules.
- **No separate drawing path.** Scale, cell size, orbit radius, and element sizes are all derived from the region size at draw time.

## Sizing and placement

From `CANVASES.md` (authoritative there):

- `mm_w = max(240, 0.20 × min(window width, height))`; `mm_h = mm_w × 0.75` (4:3).
- Anchored bottom-right with an 8 px margin.
- Labels/title text suppressed below ~320 px on the shorter edge to avoid clutter.

## Open questions

- Whether the minimap eventually gains its own affordances (a maximise button, a lock) rather than being purely a swap target.
- Interaction once overlays (supply routes, units) land on the canvases — does the minimap show them too?

## Related

- `CANVASES.md` — authoritative spec for drawing, sizing, and swap interaction.
- `LAYOUT.md` — placement in the shell.
