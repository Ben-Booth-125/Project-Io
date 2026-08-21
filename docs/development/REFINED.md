# Project Io — REFINED (active worklist)

**Empty between work blocks.** Sprint P1 is open; the entries below are its live arc.

## Landed this session (2026-08-21)

All four assigned items plus the two unqueued ones, on branch
`claude/bl-519-520-nr-438-439-n29ljl`.

- **NR-438 / NR-439 — the raised ceiling.** 12 is a preference, 20 the asserted
  hard cap, over-12 share reported never asserted, `IO_ABSORB_PREFER_ROOM` gone.
- **BL-519 — the tile axis split.** `terrain_composition` → substrate × cover ×
  density. 522 references across ~110 files. New `tile_axes_harness` (13 checks).
  The 120-seed earthlike census is bit-identical to the pre-split baseline.
- **BL-521 — click injection in the verify API.** Drives ImGui's real event queue;
  double-click promotion forced per press so frame time cannot decide it.
- **BL-520 — basic tile texturing.** Grain on the substrate, pattern on the cover;
  13 composable marks instead of 84 enumerated. LOD off below 14px.
- **BL-516 — water kinds and sea provinces.** Structural lake/coast/ocean with no
  threshold; sea provinces pinned by measurement at d=7, mean 41.07.

**The one thing all of it shares: nothing has been rendered.** This container
cannot build the GUI, so every UI half is compile-clean and arithmetically checked
and visually unseen. No golden was blessed — blessing an unseen frame pins
whatever got built.

## Owed, and each has a review entry

- **The visual pass**, on Ben's machine: BL-519's colour blend (NR-451), BL-520's
  marks and lens attenuation (NR-457), BL-521's click script (never executed),
  BL-516's water in/out of the blend.
- **Rulings waiting**: the 80-tile sea clamp exceeded at 82 (NR-460, the same
  shape as NR-438); `is_coastal` narrowed to sea, costing ~22% of coastal tiles
  their port eligibility (NR-461); lakes' province home (NR-462); does the
  province blend survive texturing, which also decides BL-514.
- **Stale goldens, none re-blessed**: `ai_skill_harness` (28), `history_sim_harness`
  (8), and the pre-BL-409 `state_hash` quoted in the standing rules (NR-452).
- **Panels still not clickable by name** — needs a target registry (NR-454).

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
