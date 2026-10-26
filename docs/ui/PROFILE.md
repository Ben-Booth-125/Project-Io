# Project Io — Profile

> **Settles:** which question the identity tile answers · what identifies the
> player's nation on it and where each field is read from · why the card carries
> no emblem · what becomes of a name too long for the card.
> **Not here:** the financial dashboard (HEADER) · the rail beneath it (MENU) ·
> placement (LAYOUT) · what a nation is (politics/NATIONS).
> **Confused with:** HEADER.md, LAYOUT.md, MENU.md.

The **profile** is a compact identity card pinned to the top-left corner of the shell, above the navigation pane and aligned to its width. It identifies the player's nation. See `LAYOUT.md` for placement; implementation in `src/ui/profile_panel.cpp`.

---

## Purpose

Give the player a persistent, at-a-glance sense of *who they are* in the world — the nation they are governing (re-subjected from the retired corporation, Ben, 2026-10-23; `CONCEPT.md` § Player identity).

## Contents

- **No emblem.** A nation carries no emblem until one is designed (Ben, 2026-10-26). The card draws none, and nothing keys a shape off the nation's entity id. The nation's identity colour is the only mark it has (`ICONS.md` § 2b: no nation flags exist).
- **Nation name** — read live from the nation table; a placeholder name appears only as a lookup-failure fallback, e.g. a spectated world with nobody seated.
- The former **`Parent: <home nation>`** and **`Focus: <industrial focus>`** lines retire with the corporation (Ben, 2026-10-23): the player is the nation, and the corporation's industrial focus has no subject. A nation's economic focus is a scorer input, not an identity line (`../GLOSSARY.md` § Economic focus; Ben, 2026-10-26).

Long lines **ellipsize** to the width remaining beside the portrait, so generated names never spill past the fixed card edge; the full text is available as a hover tooltip.

The panel is a static identity readout. It may later become the entry point to a fuller nation screen.

## Emblem

A nation emblem is a generated identity system, a feature of its own. Until it is designed, no surface stands one in — the retired corporation's geometric emblem is not borrowed for the nation (Ben, 2026-10-26).

## Related

- `LAYOUT.md` — placement in the shell.
- `HEADER.md` — the adjacent budget/resource strip.
