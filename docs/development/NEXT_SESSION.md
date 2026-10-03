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

## Three branches NOT merged — each waits on Ben, then a cold review

Verified with `git branch --contains` at close.

**1. BL-1168 charter priced by trade reach.** Branch `worktree-agent-aca5141d2e1b745e1`:
- efecc62c prices by reach.
- bc4b12a7 re-anchors the specialist count, 2 → 44 charters. HOLD.

Ben's call, one of:
- (a) Accept 44. Seats move off the heartland: 6 of 190 on the top landmasses, and seed 11
  keeps one seat.
- (b) Anchor at 8 rather than 12.
- (c) Price specialists at the world's stock and firms by reach.

The shipped digest pins move. Still owed: the Industrialisation works-chartered notes price
world-wide. BL-1169's K is set only after this lands, read together.

**2. BL-1171 far trade.** Branch `worktree-agent-a55addb838a362e72`, ab60a34a and c1c05c87. The
switches are ON. Readings:
- 1660 lanes 79 → 130.
- Lanes 31+ tiles long 21 → 50.
- Far-bound realms 0 → 81.
- Cross-landmass trade −56% / −53%.
- Industrialisation subjections 70 → 16.

sea_lane_stamp_harness row B fails on seed 28: mean current 70 against 124. That is a call for
Ben; do not weaken the row. Open question for him: should meeting by sea be gated too? Needs a
cold review.

**3. BL-1172 army eats at a fair price, BL-1173 working capital.** Branch
`worktree-agent-a4454709fec7f62e0`, c570b1cf and 9ae9e0c1. The lane reported a wrong branch
name. What changed:
- reservation_mult 9.0 → 2.0 in `scripts/economy.lua`.
- A billing cap in clear_markets via `economy_report::upkeep_purchases`.
- The supply share floors unit strength.
- Working capital at 0.25 × opening stock value.

With both fixes, out of 462 firms, survivors read 56 / 34 / 27, against a baseline of 25 / 22 / 5.

Ben's calls:
- The billing cap goes beyond the ruling's wording.
- Building upkeep is affected too.
- BL-1173 alone is harmful, so ship both fixes or neither.

R0b fails on main too. The pss pins move. Needs a cold review.

**Held for sprint 49:** BL-1139 part 2 (abandonment), 2f719556 on
`worktree-agent-ae9978d9141f0dc73`. It sits behind BL-1163 (growth basket).

## Then

1. Ben's calls on the three branches.
2. Cold reviews.
3. Merge each branch, then gate it on main.
4. Set BL-1169's K.
5. BL-1170 (heartlands reach overseas, "something like America"): a design form first.
6. The second re-bless, with every cause named.
7. Ben's live walk (checklist given; BL-1145 RD owed; confirm lane merge 500 by eye).
8. The sprint close: retro and version cut.

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
