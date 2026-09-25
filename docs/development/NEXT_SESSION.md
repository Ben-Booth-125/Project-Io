# Sprint 48 handoff — the world moves forward (proposed, not cut)

Written 2026-09-25 at the close of sprint 47 (v0.1.26). Read this, then the sprint 48 row in
`sprints.json` (`node tools/session/render_sprints.js` renders `SPRINTS.md`), then the owning doc of
whatever you pick up. The history of how sprint 47 got here is the DEVLOG (2026-09-24 evening and
2026-09-25 afternoon entries) and `drafts/sprint-47-rulings.md` §§ A–G.

## Where things stand

- **Sprint 47 is CLOSED** (Ben: "I'll accept this as sprint 47 complete"). 27 items delivered and
  archived, 20 requirement groups resolved and archived, the one re-bless taken (`2f877e2c`), two
  live walks passed. **v0.1.26 is tagged and pushed.**
- **The review queue is EMPTY** — every NR is resolved. Ben overruled "no sim beats" (NR-919): sim
  beats are in scope now, one re-bless each.
- **The hot backlog holds 33 open items**, seven already marked sprint 48.
- **REFINED.md has no open tasks.** Sprint 48's tasks are written at its cut (DELIVERY.md steps 0–2).

## Sprint 48: proposed shape (the cut is the next session's, with Ben)

Theme (proposed): **the world moves forward** — the seam sprint 47 carried, plus what Ben's two live
walks asked for.

**Already marked sprint 48:**
| Item | What | Weight |
|---|---|---|
| BL-1084 (world built once and moved) | Split the 2,800-line generation monolith into a cursor the rounds move forward; byte-identical to the monolith at every round boundary on the 16 seeds before any UI builds on it. NR-918 ruled the reroll path (copy + replay); NR-936 (Next doesn't wait) rides with it. | S, the risk |
| BL-1119 (roads tree and detour) | Kruskal tree + a detour test (> 2× the direct route) replaces the relative-neighbour lattice; only villages above a measured size spur; round 6's tail inside 35 s. World-mover, one re-bless. Consider BL-1077 (village roads cost) beside it. | S |
| BL-1118 (round legends) | A key per lapse round naming exactly its drawn layers, AFTER the Empires cut (built). | A |
| BL-1124 (sea lanes seen) | Lanes draw (seed 32: 16) but sit under colonial ties; a soft band was still not found. Pair with the legend. | A |
| BL-1086 (search inside generation) | Fold the landscape search into generation (NR-924: after BL-1084). | A |
| BL-1098 (sea-lane tier stamped) | The water stamp, through the existing seam (NR-928). | A |
| BL-1107 (culture ground profile) | WIDENED by NR-926: the profile gets its reader (cultural preference), an amenity classifier first. A sim beat. | B |

**Sim beats Ben ruled today, not yet scheduled** (each needs a read on the 16 seeds first and one
re-bless; take them one or two per sprint, never all at once):
- BL-1123 (band per nation) — ERAS.md owns the rule; an actor-aware registry. Delegated reading
  inside: the band reads the realm the nation grew from.
- BL-1122 (in-span chartering) — firms founded in the span with points debited; a design pass first
  (how the 1960 charter budget and the search treat firms that already exist).
- BL-1115 (sea legs in the later spans) — should land before BL-1120 (ocean currents) and before
  choosing BL-1124's final form, since wet campaigns then write lanes off the ties.

**Filed, smaller:** BL-1114 (epoch 0 retired), BL-1116 (handoff validators, Light), BL-1117 (settle
tick one cost — tick 1 is all `run_economy_step`, 38.5 s on seed 0), BL-1121 (colonial tribute:
measure, then show), BL-1110..1113.

**Suggested cut:** BL-1084 first and alone in its lane (the risk), BL-1119 beside it (the complaint
Ben felt most: round 6's minute), BL-1118 + BL-1124 together as one UI lane. Hold the sim beats for
sprint 49 unless Ben asks — BL-1084 and BL-1119 both move digests, and one re-bless per sprint is
the practice.

## Hazards this sprint taught

- **Check a measurement's build time against the last code commit before applying pins.** The
  overnight re-bless measured before its riders landed; its re-pin sat uncommitted across a session
  boundary and would have been wrong.
- **A running app locks `build_rel/ProjectIo.exe`.** The build fails with LNK1104 and any script
  then runs the OLD exe and passes on nothing. Read `BUILD_REL_EXIT`, not the script's result.
- **A capture passes what a live click does not.** The lane drew correctly and still read as nothing;
  the globe dissolve passed its capture while covering the map. Budget a live walk per UI item.
- **The walk-through form works:** one check per row, in walk order, Pass/Fail/Not seen, notes per
  round. Two passes took minutes of Ben's time.
- Long harnesses: hold keep-awake from `tools/session/keepawake.ps1 -WhileFile <lock>`;
  `player_seed_sweep --digest-check` is ~45 min per arc and both arcs run in parallel fine.

## Tools that exist now

- `scripts/verify/sea_lanes.lua` — seed 32, round 5 mid-span and close, for BL-1124.
- `verify.wizard_span_seed(k)` — reads a span's pending seed, so a script can prove a reroll moved
  only its own slot.
- `finish_campaign_world` prints the slowest settle tick by lap — the first read BL-1117 needs.
