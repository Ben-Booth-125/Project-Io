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

## Landed since (2026-09-26)

- **BL-1130 (centres consolidate)** merged with its review fixes: centres 164,982 -> ~66,000 pooled, land under centres 70% -> 28%, spills 0; the prize and relay read heads; a settled place is worth at least a village; a razing is counted in people.
- **BL-1133 (a province is its centre's ground)** merged: provinces 45,199 -> 30,894, anchors 20,505 -> 6,856 (NR-954 open).
- **BL-1136** the search on two threads (six rounds kept); **BL-1120** ocean currents, **BL-1140** lanes from trade, **BL-1098** lanes stamped bending with the current (world save v28; NR-955 open).
- UI: the bowed-arc lane, roads carried across rounds (BL-1134), the Culture round worded as the record (BL-1135, a draft for the walk), realm_identity's script fixed.

## In flight (worktree lanes, 2026-09-26)

- **Centres:** BL-1141 (a region deepens into ONE centre, its scale from its heads; stranded points to the realm's nearest centre) then BL-1137 (urbanisation) rebuilt on it and read against the ~500 aim.
- **Spacing:** BL-1132 at three tiles, Settle scored only where there is room.
- **Search:** BL-1086 option A (the carve counts the charter budget's planned firms; no discarded roster).
- **Sea:** BL-1142 (far pairs bind across water; a leg against its current delivers less).

## Then

BL-1125 (markets fold: twins, gravity aimed at 20-40, conquest) on the thinned world -> BL-1138 (roads pull to markets) -> NR-944 / NR-954 re-read -> BL-1107 -> BL-1139 (abandonment in play: delete and merge the province) -> the one re-bless (every cause named; the pins R3b/R3d.1 and every digest are red by design until then) -> the live walk (Next waits, reroll, round 6 pace, the Culture wording, the arc, carried roads).

## Open for Ben

NR-952 (the ruin fallback), NR-953 (scale weights reach), NR-954 (hemmed villages, anchors by the settled line, giant provinces), NR-955 (inland ports, lane vs road cost).

## Hazards this sprint taught

- Five lanes on one PC: every agent timing is indicative; take headline numbers quiet.
- A background `X=... && (...) &` scopes X to the list; a keep-awake holder inside a script's
  `wait` deadlocks on its own lock — release the lock before waiting on it.
- `--verify-all` carries state between scripts; run a failing wizard script alone before
  believing it.
- Main-session build: `scratchpad/main_session_build_rel.bat` (writes BUILD_REL_EXIT). Close the
  live app before rebuilding.
