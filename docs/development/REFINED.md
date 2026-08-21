# Project Io — REFINED (active worklist)

**Empty between work blocks.** Sprint P1 is open; the entries below are its live arc.

## In flight (2026-08-21 session)

- **BL-519 — the tile axis split. LANDED.** `terrain_composition` is gone;
  `terrain_substrate` × `terrain_cover` × `cover_density`, landform unchanged. 522
  references across ~110 files, migrated by meaning, shim deleted in the same pass
  per Ben's call. New `tools/verify/tile_axes_harness.cpp` (13 checks, all pass).
  The 120-seed earthlike census is bit-identical to the pre-split baseline, which
  is the evidence that no RNG stream drifted. **Owed: the visual check (R7)** —
  this container cannot build the GUI (NR-451).
- **NR-438 / NR-439 — the raised ceiling. LANDED.** 12 is a preference, 20 the
  asserted hard cap, the over-12 share (4.90%) is reported and never asserted,
  and `IO_ABSORB_PREFER_ROOM` is deleted.
- **BL-520 — basic texturing.** Sequenced behind BL-519, now unblocked. Decide
  **BL-514** (blend all tiles) alongside it — texture and the blend are in direct
  tension, and each changes what the other is for (NR-443).
- **BL-521 — click injection in the verify API.** In flight. Note its `design`
  field in `backlog.json` is CORRUPTED (NR-449) — this paragraph and the one below
  are the best statement of intent that survives.

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
