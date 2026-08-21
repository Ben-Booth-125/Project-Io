# Project Io — REFINED (active worklist)

**Empty between work blocks.** Sprint P1 closed its build tasks on 2026-08-21.

## Next session takes these three (Ben, 2026-08-21)

- **BL-519 — the tile axis split.** `terrain_composition` split into substrate ×
  cover, with `landform` unchanged and `none` a first-class cover value. 330 call
  sites across 49 files, no save migration (there is no save format). Open
  questions live in the item: graded vs binary cover, whether `tundra` survives as
  a cover, whether `urban` is a state or a cover, and whether the derived
  `composition()` accessor is permanent or deleted last (NR-444).
- **BL-520 — basic texturing.** Sequenced behind BL-519: 84 hand-authored
  combinations against ~16 that compose. Decide **BL-514** (blend all tiles)
  alongside it — texture and the blend are in direct tension, and each changes what
  the other is for (NR-443).
- **NR-438 / NR-439 — the raised ceiling**, now ruled. 12 tiles is a PREFERENCE,
  20 a hard cap, larger-than-12 permitted in rare cases. **Check before coding: the
  shipped behaviour already satisfies this** — max is 16 across 6 seeds. The work is
  likely saying it (`province.hpp` still calls 12 a clamp), asserting it (a hard-cap
  row at 20), deleting the now-unneeded `IO_ABSORB_PREFER_ROOM` variant, and
  REPORTING the over-12 share (currently 4.9%) rather than asserting a rareness
  threshold nobody has chosen.

## Also ready, unqueued

- **BL-521 — click injection in the verify API.** Designed this session at Ben's
  instruction, deliberately unbuilt. This is the item that closes the live-check
  class permanently: a non-interactive agent currently CANNOT satisfy a live check,
  so every interactive surface it builds arrives with its live half owed by
  construction. Two agents in a row hit this on BL-511 alone.
- **BL-516** (lake/coast/ocean tile kinds, sea provinces capped at 80 tiles) and
  **BL-518** (the Era −1 sim redrawing borders as its wars resolve) — both unblocked
  now that BL-515 is settled.

## Still owed, each with its review entry

- **The live click** on a province (BL-511 R1) — NR-416, NR-424. BL-521 is the fix.
- **Six items landed-awaiting a live check**: BL-412, BL-408, BL-411, BL-480,
  BL-429, BL-453 — NR-388.
- **BL-408 has no entry point at all.** `ui_state.spectating` has one writer in the
  tree, the Lua binding. Needs a ruling, not a task — NR-389.
- **BL-458 shipped silent.** Comms message, Convoys-tab cause and canvas mark all
  absent; they need a field on `world` that other lanes were holding — NR-407.

## Rulings still open

- **NR-406** — should the building ceiling move during play, since infrastructure is
  an input?
- **NR-421** — the per-province firm cap is inert by orders of magnitude (296 firms
  across 295 distinct provinces). Drop it, keep it for later density, or move the
  constraint to a grain where firms actually compete.
- **NR-415** — where does the per-lens reduction table live, and does every future
  lens inherit the obligation?
