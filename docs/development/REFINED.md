# Project Io — REFINED (active worklist)

**Empty between work blocks.** Sprint P1 closed its build tasks on 2026-08-21; what
remains from it is judgement, not work, and lives in `NEEDS_REVIEW.json`.

## What the last block left owed

Not promoted tasks — these are the honest remainders, each with its review entry.

- **The live click.** BL-511's requirement R1 has a LIVE half nobody has taken: two
  agents in a row could not (no click injection in the verify API, no granted
  applications in a non-interactive session). The hit-test resolution path is
  proven; the press is not. NR-416, NR-424 — and NR-424 proposes the permanent fix,
  a click-injection hook, which would close the whole class.
- **Six items landed-awaiting a live check.** BL-412, BL-408, BL-411, BL-480,
  BL-429, BL-453. Code in the tree, every held row a live check. NR-388.
- **BL-408 cannot be entered at all.** `ui_state.spectating` has exactly one writer
  in the whole tree — the Lua binding `verify.spectate`. No menu item, no CLI flag.
  NR-389 — it needs a ruling, not a task.
- **BL-458 shipped silent.** The mechanic is proved; the comms message, the
  Convoys-tab cause and the canvas mark are absent, because they need a field on
  `world` that other lanes were holding. NR-407.

## Rulings owed before the next build block

- **NR-438 / NR-439 — the 12-tile ceiling no longer holds.** 4.9% of provinces
  exceed it, max 16. The prefer-room alternative is costed (241 over, max 14,
  80.63% in band) and compiled out behind a flag. BL-467's battle envelope reads
  province size, so this wants settling BEFORE that lands.
- **NR-406** — should the building ceiling move during play, since infrastructure
  is one of its inputs?
- **NR-444** — is the `composition()` accessor permanent or deleted last?
- **NR-421** — the per-province firm cap is inert by orders of magnitude. Drop,
  keep, or move the constraint to a grain where firms actually compete.
- **NR-415** — where does the per-lens reduction table live, and does every future
  lens inherit the obligation?

## Held deliberately

- **BL-514** (blend all tiles) — held until Ben sees the organic borders. The A/B
  capture exists.
- **BL-516** (water kinds + sea provinces), **BL-518** (war redraws borders) — both
  want BL-515 settled first, which it now is.
- **BL-519** (tile axis split) / **BL-520** (texturing) — designed today, unstarted.
  BL-520 is sequenced behind BL-519: 84 hand-authored combinations versus ~16 that
  compose.
