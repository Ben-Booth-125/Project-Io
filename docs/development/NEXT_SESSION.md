# Sprint 48 running handoff — the world moves forward (OPEN)

Updated 2026-09-25 late evening. Read this, then `REFINED.md` § Sprint 48 (wave 3 lists the
chains in order) and the sprint 48 row in `sprints.json`. The DEVLOG's 2026-09-25 (evening) entry
is the session record so far.

## Landed on main (all gated there)

- **BL-1084 (world built once and moved):** the cursor (K1-K2), the wizard (K4) and its review
  fixes. Byte-identical proof: world_determinism, both digest-check arcs 16/16, the 16-seed
  cursor equivalence with negative controls, and `stop_path_digests` (base vs HEAD, identical).
  Next waits; reroll works mid-tail; round 6 keeps its pace; slots freed at Begin. Live clicks
  R4-R6 still owed at a walk.
- **BL-1117 (settle tick one):** one nearest-anchor field per body; settle 116 s -> ~18 s on seed 0.
- **BL-1126 (path cost reads the cache): CLOSED.** A path is directed; a save/load continues
  identically.
- **BL-1119 (roads tree and detour), rounds 1-4:** Kruskal + detour test, the 40,000-head spur
  floor, border links on the network, layable links only (NR-945 kept), flood reuse (step 15
  21.7 -> 2.0 s). R5 (35 s) FAILED on seed 0 (~48 s quiet; seed 28 ~31 s) — the search and the
  corps' strategy step remain; BL-1136 cuts the search.
- **BL-1114 (epoch 0 retired): CLOSED.** **BL-1118/BL-1124** legends and the lane forms (Ben picked
  the bowed arc). Market census and centre census harnesses committed.

## In flight (worktree lanes)

- **Centres:** BL-1130 round 2 — explain why the history moved (battles 7 -> 98 on a test run),
  water regions carry no centre. Explain first, then merge (Ben).
- **UI:** U3 (the arc only), BL-1134 (roads carried across rounds — a defect Ben saw), BL-1135
  (the Culture round worded as the record).
- **Search:** BL-1136's round-count curve; stops for Ben's pick; then BL-1086.
- **Sea:** BL-1120 (ocean currents, the NR-949 reading); stops with a weight ladder; then BL-1140
  (lanes from trade), then BL-1098 (the stamp).

## Then, in order (the density chain is the headline)

BL-1137 (industrial urbanisation + abandonment, aiming at ~500 centres a world) with BL-1132
(settle spacing) and BL-1133 (anchors join a neighbour) -> BL-1125 (markets fold: twins, gravity
aimed at 20-40, conquest) -> BL-1138 (roads pull to markets) -> NR-944 re-read (nations off the
road network) -> BL-1107 -> BL-1139 (abandonment in play). Then ONE re-bless with every cause
named, split if it grows too wide.

## Open for Ben

NR-948 (abandoned in play = the razed tier), NR-949 (currents from wind bands and coasts), NR-950
(the market trunk: nearest neighbours, Road tier) — all decisions taken on his behalf.

## Hazards this sprint taught

- Five lanes on one PC: every agent timing is indicative; take headline numbers quiet.
- A background `X=... && (...) &` scopes X to the list; a keep-awake holder inside a script's
  `wait` deadlocks on its own lock — release the lock before waiting on it.
- `--verify-all` carries state between scripts; run a failing wizard script alone before
  believing it.
- Main-session build: `scratchpad/main_session_build_rel.bat` (writes BUILD_REL_EXIT). Close the
  live app before rebuilding.
