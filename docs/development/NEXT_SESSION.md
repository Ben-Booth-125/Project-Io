# Sprint 48 running handoff — the world moves forward (OPEN)

Updated 2026-10-03, session closed with every lane reported. Read this, then `REFINED.md`
§ Sprint 48 and the sprint 48 row in `sprints.json`.

## Main

Tip 5c18bf8c. Merged this session:
- The sprint 48 re-bless (authorised; shape in `drafts/sprint-48-rebless.md`).
- BL-1138 roads pull to markets.
- Sea lanes merge into shared trunks (reuse cost 500).
- Roads drawn as thinner curves, and sea lanes along their sea path (Ben liked both).
- BL-1169's crowded-heartland brake, merged OFF with K = 0.
- The bridge cap: at most 2 water tiles on every road writer and in the lapse.
- Instruments: industry_concentration and firm_attrition_trace.

Known reds on main:
- history_sim R3a2/R3a3.
- logistics T7-T10 (BL-1131).
- centre_census C13 C2b slope (BL-1158 slopes).
- rival_military_seeding R0b.
- The icon_silhouettes goldens are ~2.2% off from the curved roads. Re-bless after Ben's look
  (BL-1167 icon goldens).
- player_seed_sweep pins have likely moved since the re-bless (the lane merge 314e972b and the
  bridge cap 5c18bf8c). Re-check, and fold this into the second re-bless at sprint close.

The play build (build_rel) is from 969d2abe and lacks the bridge cap. Rebuild it before Ben's
next look (`main_session_build_rel.bat`; check BUILD_REL_EXIT and that the app is closed).

## Merged 2026-10-03/04 (session 2) — every branch cold-reviewed, then gated on main

All four merged on main, each green except main's known reds:
- **BL-1168 charter by reach** (option a, 44 charters). Islet commit reverted: a centre is priced
  by its region's ANCHOR. Works notes price by reach. Live tick reads x1.12 legacy (noisy).
- **BL-1171 far trade** + meeting by sea gated by fleet out-projection; either side's fleet binds;
  a bridged strait still parts two landmasses; conquest inherits overseas contacts ungated.
  Sea-lane trunks are shared only WITH the current (align >= 0 keeps the discount) — row B passes
  16/16. Lane field moved; drawn trunks look different (eyeball on the live walk).
- **BL-1172/1173 fair-price army + working capital.** A draw pays the POSTED price; the 2x
  ceiling governs every draw (upkeep, processors, construction, nations' pool and shelf); a draw
  over it does not bid. Shelf-as-supply is built but k = 0 until shelf spoilage (BL-1179): every
  k > 0 cost survivors. Known cost at k = 0, stated in FINANCE.md and bounded by U13/M5: a unit or
  processor beside a full, unlisted shelf is fed one tick in four.
- **BL-1169 K = 1000** in Industrialisation only: far rival 12 -> 14 of 16, industry -29%.

Filed: BL-1176..1178 (charter review leftovers), BL-1179 shelf spoilage, BL-1180 construction
rate panel drift. BL-1166 already covers the industrialisation_sim_harness self-check red.
Also red on main, not on the old list: resource_chain_harness 4 R1 rows (identical before these
merges).

exploration_sweep.json is modified, uncommitted: regenerate it at the second re-bless, after the
last mover.

## Rulings this session (in their docs)

Every ruling is dated 2026-10-03.

| Ruling | Doc |
|---|---|
| Abandonment held (C); culture rung 250/4 | — |
| A market road's tier follows the land it crosses | LOGISTICS.md |
| The Exploration age grounds international trade; NR-888 is overturned for that age | EXPLORATION.md |
| Far realms bind where the seller's fleet out-projects the partner's at its port | EXPLORATION.md |
| Goods between landmasses sail in every span | EXPLORATION.md |
| A charter is priced by the capital in its trade reach | INDUSTRIALISATION.md |
| A crowded heartland yields less, measured as capital per worker | INDUSTRIALISATION.md |
| Judged property: a rival far away holds at least 25% of the leader's industry | INDUSTRIALISATION.md |
| America direction (PROPOSED) | INDUSTRIALISATION.md |
| An army eats at a fair price | FINANCE.md |
| Every firm opens with working capital | FINANCE.md |
| Hiring answers a threat (sprint 49) | FINANCE.md |
| Bridges span at most 2 water tiles | LOGISTICS.md |

## Open for Ben

- NR-966: make the shape probe a skill.
- The lapse draws roads along land (novel work).
- The three branch calls above.

## Sprint 49 (filed)

| Item | Note |
|---|---|
| BL-1163 growth basket | Per market; design pass first |
| BL-1139 abandonment | Behind BL-1163 |
| BL-1174 hiring answers a threat | Widens the AI grant |
| BL-1175 firm entry | Design first |
| BL-1165 | Untraced movements |
| BL-1166 | Industrialisation self-check |
| BL-1160 | Wants by age |
| BL-1162 | Amenity goods |
| BL-1157 | Fleet upkeep and industrial fleets |
| BL-1158 | Slopes |
| BL-1161 | Province 0 sentinels |
| BL-1167 | Icon goldens |
| BL-1148 | Migration carries culture |

## Hazards this sprint taught

- Lanes misreport their branch names. Always `git branch --contains <commit>`.
- SendMessage refuses a stopped agent's worktree. Spawn a fresh agent to merge its branch.
- Lanes leave wait-loop bash processes behind. Kill only their own PIDs, never Ben's
  ProjectIo.exe.
- Brief a ruling at its stated scope. A re-scoped red test is a finding.
- Requirement group names can collide with archived groups.
- A `git merge` can fail on a transient lock with a clean tree. Retry once.
- Worktrees start stale; every lane merges main first. Use a new gate folder per run.
