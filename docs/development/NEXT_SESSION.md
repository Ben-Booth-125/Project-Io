# Sprint 49 handoff — sprint 48 closed 2026-10-04

Read this, then `SPRINTS.md` (sprint 48's retro) and `backlog_query.js --grep` for each item you
pick up. Sprint 49 is **not cut yet**: the cut is Ben's form.

## First, before anything else

1. **The second re-bless is PREPARED, not authorised.** A lane is writing
   `docs/development/drafts/sprint-48-rebless-2.md` (shape table over the 16 curated seeds, every
   cause named, every pin old -> new) and the re-pin commit on its own branch. Ben authorises
   against the SHAPE (DELIVERY.md § the digest re-bless), then the main session merges it. Causes:
   the lane merge 314e972b, the bridge cap 5c18bf8c, BL-1168, BL-1171 (with the meeting gate and
   trunks that follow the current), BL-1172/1173, BL-1169 K = 1000.
2. **`exploration_sweep.json` is modified and uncommitted** on main. The re-bless lane regenerates
   it on the after-tree; take its copy, do not commit the stray one.
3. **The version cut** (v0.1.27) follows the re-bless, as sprint 47's did.
4. **Push.** Main is ahead of origin by the whole close (merges, rulings, archives). Ben pushes,
   or asks.

## Where main stands

Every sprint 48 branch is merged, cold-reviewed and gated; closed items and groups are in the cold
store. Known reds on main: history_sim R3a2/R3a3; logistics T7-T10 (BL-1131); rival_military_seeding
R0b; resource_chain_harness 4 R1 rows; industrialisation_sim_harness's stale self-check (BL-1166).
The play build (`build_rel`) was rebuilt from 9171c2ef and walked by Ben.

Ben (2026-10-04): more lenses are wanted for visibility on several sprint 48 surfaces. He holds that
for before sprint 50; do not file it unasked.

## Sprint 49 candidates

Tagged 49:

| Item | Note |
|---|---|
| BL-1170 (heartlands reach overseas) | DESIGNED 2026-10-04, seven build slices in the item; requires BL-1148. Judged property: a far rival holds ≥ 25% of the leader's industry (14/16 today) |
| BL-1148 (migration carries culture) | Untagged but BL-1170 requires it — tag it in with BL-1170 |
| BL-1163 (play villages decline) | Design pass first; BL-1139 (abandonment) waits behind it |
| BL-1174 (hiring answers a threat) | Widens the AI grant: raise it, read AI_OPPONENT.md § 11 |
| BL-1175 (firm entry) | Design first |
| BL-1179 (shelf spoilage) | Then re-read shelf supply k (it is 0 until this lands; MARKETS.md) |
| BL-1165 (untraced re-bless movements) | Can absorb BL-1119's R6 haulage reading |
| BL-1119 (roads tree and detour) | Carried: R4 quiet road-cost reading, R6 haulage, R7 review of round 4 |
| BL-1166, BL-1176..1178, BL-1180, BL-1181 | Small: harness self-check, charter review leftovers, panel drift, bind-and-free churn |

Untagged but named for sprint 49 at the sprint 48 cut: BL-1157 (fleet upkeep, industrial fleets),
BL-1158 (slopes), BL-1160 (wants by age), BL-1161 (province 0 sentinels), BL-1162 (amenity goods),
BL-1167 (icon goldens).

## Open for Ben

- NR-966: make the re-bless shape probe a skill (novel work).
- The lapse draws roads along land (novel work, from sprint 48).

## Hazards this sprint taught

- On the economy seam, brief every rule with a MULTI-TICK row: a one-tick row cannot see a price
  its own buyer moves (BL-1172 took six review rounds).
- Build a walk list from pending rows only after checking each owning item is live.
- `requirements_query.js --class visual` crashes on a row whose `verification` is not an array.
- Lanes misreport branch names: `git branch --contains <commit>`. Worktrees start stale: every
  lane merges main first. A harness gate edited mid-run builds the new source into its later
  harnesses.
