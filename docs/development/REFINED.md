# Project Io — REFINED (active worklist)

*Empty between work blocks.*

**Sprint 23 (UI visibility, batch 2: selection & hover) CLOSED 2026-08-28 — goal
met.** Twelve items across three waves; every planned item landed and both
carried review-queue entries (NR-697, NR-698) were answered and pruned. The
retro is in `archive/sprints-*.json`.

Two things are **owed** out of it, neither of them the sprint's own work:

- `NR-706` — the Company ledger destination has never been live-clicked. No
  generated world tested carried a background firm holding a tile on the home
  body, so there was nothing to press. Needs a world or a save that has one.
- `NR-701` — under a lens the hover card still describes the TILE while the
  click selects the structure. `draw_hover_content` has no branch for a
  corporation, company, catchment or plate; writing those four is content work
  of a different kind from the resolution rule.

**Next up:** sprint 24 (UI visibility, batch 3: ledgers). It already has two
defects filed against its own surfaces from this sprint's live pass —
`NR-705` (the corporations table clips every firm's name to one character) and
`NR-704` (the two corporation ui_state flags are named the wrong way round,
which is how a destination shipped green while opening the player's own books).

**Open work with no promoted tasks:** `node tools/session/backlog_query.js --table`.
