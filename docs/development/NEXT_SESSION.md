# Sprint 50 handoff — logistics and trade flow (written 2026-10-06)

Read this, then `SPRINTS.md` (sprint 49's record), then `backlog_query.js --grep` for each item
you pick up. The sprint 50 cut is Ben's form; this is the proposal.

## Focus (Ben, 2026-10-06)

Sprint 50 is the logistics system and the core trading loops, with visibility. **Proposed split:**
sprint 50 builds the logistics and trade-flow simulation with one diagnostic lens; sprint 51 takes
the player's trading loop and the visibility suite. Lenses built over mechanics still moving waste
the lens work; debugging logistics still needs to see flows, hence one lens early.

## Why logistics next

Sprint 49 (market viability) left goods that exist but do not move. On the 5-seed checkpoint,
thousands of units of water sit on surplus shelves while ~13-16 markets per seed are dry, and
stalled construction sites wait on steel another market holds. The water diagnosis
(`tools/verify/water_pair_probe`) classified each dry market's best route: no room 62% (fixed in
sprint 49 by the hauler-room rule, SUPPLY.md § Dispatch trigger), price gate 22%, no route 13%,
room 4%. After the room fix, the one-destination-per-pass rule binds (~113 units unfilled per
probe tick). The `market-viability` skill (BL-1184) is the gate. Sprint 49 closed (2026-10-07) on
16 seeds at G1 54.7% (target 70, unmet) / G2 54.4% / G3 88.9%: a quarter of built processors are
idled for want of inputs before the handoff.

## Sprint 50 — logistics and trade flow

- **The trade-flow lens, first:** who ships what, where, at what landed price, and what was
  refused and why (the refusal classes the water probe already computes). The instrument for
  the rest of the sprint.
- **BL-1217 (inputs reach processors):** G1 carried from sprint 49 (Ben, the close-out form) —
  which inputs fail to arrive, where, and why; fix the cause in network or dispatch. Also
  re-measures construction stalls (abandonment is still unruled).
- **BL-1203 follow-ups:** the one-destination-per-pass rule (`supply_system.cpp` market export);
  the price gate at k = 0; no route — ports per body and port placement.
- **BL-1195 (convoy lane follows legs):** position, vision and interdiction read the direct
  centre-to-centre path, not the land-port-sea-port-land route the cargo takes.
- **BL-1165 (untraced re-bless movements),** including `haulage_measure`'s stale copy of the
  old pricing (re-point it at `price_market_export_leg`).
- **BL-1192 (catchment ignores water)** and **BL-1190 (markets meet firms).**
- **BL-1119 (roads tree and detour):** the road network's shape, settled 2026-09-25.
- **The gate:** extend BL-1184 with a logistics row (units moved surplus -> shortage; dry
  markets; refusals by class).

## Sprint 51 — the player's trading loop and visibility

- The player's trade verbs end to end — standing orders (price floors, MARKETS.md step 4),
  directed hauls, contracts (`CONTRACTS.md`) — and what the player reads to choose them.
- The lenses Ben held "for before sprint 50" over sprint 48's surfaces; ledger legibility for
  prices, shortages and routes. Live-click heavy: plan for Ben's walks.
- **BL-1180 (construction rate panel drift):** the panels predict the rate the tick takes,
  including the pro-rata share of a short shelf.

## Displaced to sprint 52 (currently tagged 50)

BL-1170 (heartlands reach overseas) + BL-1148 (migration carries culture), BL-1174 (hiring
answers a threat — needs its AI grant), BL-1181 (bind-and-free churn). From sprint 49's close:
BL-1175 (firm entry, design first), BL-1189 (opening stock sized), BL-1139 (centres abandoned in
play).

## Hazards to carry

- Cold review caught a defect in nearly every sprint 49 lane; budget a fix round per world-mover.
- A ratio gate can hide a real drop: read absolute numbers (G2 is play / settle income).
- Measure main yourself before merging; an agent's "before" goes stale as main moves.
- Lanes cannot run `cmd`: build the app and CMake-only harnesses in the main session.
- Rebuild `build_rel` (the play build) before Ben opens the game.
