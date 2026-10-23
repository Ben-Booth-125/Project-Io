# Project Io — Profile

> **Settles:** which question the identity tile answers · what identifies the
> player's nation on it and where each field is read from · how the emblem is
> chosen and what makes it stable · what becomes of a name too long for the card.
> **Not here:** the financial dashboard (HEADER) · the rail beneath it (MENU) ·
> placement (LAYOUT) · what a nation is (politics/NATIONS).
> **Confused with:** HEADER.md, LAYOUT.md, MENU.md.

The **profile** is a compact identity card pinned to the top-left corner of the shell, above the navigation pane and aligned to its width. It identifies the player's nation. See `LAYOUT.md` for placement; implementation in `src/ui/profile_panel.cpp`.

---

## Purpose

Give the player a persistent, at-a-glance sense of *who they are* in the world — the nation they are governing (re-subjected from the retired corporation, Ben, 2026-10-23; `CONCEPT.md` § Player identity).

## Contents

- **Emblem** — a geometric emblem (shape + identity colour) on a dark portrait plate. The shape is chosen deterministically from the nation's entity id (stable per campaign, distinct between nations); the colour is the nation's identity colour.
- **Nation name** — read live from the nation table; a placeholder name appears only as a lookup-failure fallback, e.g. a spectated world with nobody seated.
- The former **`Parent: <home nation>`** and **`Focus: <industrial focus>`** lines retire with the corporation (Ben, 2026-10-23): the player is the nation, and a nation has no single industrial focus.

Long lines **ellipsize** to the width remaining beside the portrait, so generated names never spill past the fixed card edge; the full text is available as a hover tooltip.

The panel is a static identity readout. It may later become the entry point to a fuller nation screen.

## Emblem family

The geometric emblem renders only in this card. Promoting it to a shared `ui::icons` glyph family reusable as map/selection markers is BL-090 (shared emblem glyphs).

## Related

- `LAYOUT.md` — placement in the shell.
- `HEADER.md` — the adjacent budget/resource strip.
