# Project Io — Profile

The **profile** is a compact identity card pinned to the top-left corner of the shell, above the navigation pane and aligned to its width. It identifies the player corporation. See `LAYOUT.md` for placement; implementation in `src/ui/profile_panel.cpp`.

---

## Purpose

Give the player a persistent, at-a-glance sense of *who they are* in the world — the corporate identity they are steering.

## Contents (implemented 2026-07-04)

- **Corporation emblem** — a geometric emblem (shape + identity colour) on a dark portrait plate. The shape is chosen deterministically from the corp entity id (stable per campaign, distinct between corps); the colour is the player's identity colour (`palette::corp_colour`).
- **Corporation name** — read live from `corporation_component`; `Unnamed Corp` appears only as a lookup-failure fallback, e.g. a world with no player corp (BL-080, complete 2026-07-04).
- **`Parent: <home nation>`** and **`Focus: <industrial focus>`** — read live from `corporation_component` (`home_nation` resolved through the nation table; focus rendered as a label).

Long lines **ellipsize** to the width remaining beside the portrait, so generated names never spill past the fixed card edge; the full text is available as a hover tooltip (BL-091, landing 2026-07-04).

The panel is a static identity readout in the prototype. Later it may become the entry point to a fuller corporation screen (holdings summary, standing, history).

## Open follow-up

- **Shared-glyph promotion (BL-090)** — the geometric emblem family currently renders only in this card; promoting it to a shared `ui::icons` glyph family reusable as map/selection markers is the named follow-up.

## Related

- `LAYOUT.md` — placement in the shell.
- `HEADER.md` — the adjacent budget/resource strip.
