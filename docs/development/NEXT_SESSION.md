# Sprint 47 handoff — one history, told through the rounds (running)

Written 2026-09-24 (evening), after the cut. Read the memory `io-sprint-47-cut`, then this, then
`drafts/sprint-47-rulings.md` (Ben's rulings R1-R25 and the delegated readings). The survey that
produced the questions is `drafts/sprint-47-narrative-survey.md`.

## Where the sprint stands

**Cut and committed** (`df44d903` the cut, `c4eb810c` wave-0 filing). Ben answered the design form;
every ruling is written into its owning doc (STARTUP, CIVILISATION, NATION_GENERATION, COLONISATION,
PLANETOLOGY, EXPLORATION, INDUSTRIALISATION, GENERATION_STRATEGY, ERAS, PRODUCTION, MARKETS,
LOGISTICS, LENSES, CORPORATION_GENERATION and siblings), cold-reviewed and fixed. The sprint row in
`sprints.json` carries the four waves, the risk and the done-when; `REFINED.md` carries the lanes.

**In flight: wave 0**, one workflow (`sprint47-wave0-lanes`, run `wf_ce38c127-8da`), five worktree
lanes each followed by a cold `code-reviewer` pass:

| Lane | Item | Group | Branch |
|---|---|---|---|
| G | BL-1083 (one seed per span) | `span-seeds-per-round` | the agent's worktree branch — read the workflow result |
| C | BL-1085 (Begin retired into round six) + BL-1108 (quit-during-build crash) | `begin-retired-into-round-six` | " |
| W1 | BL-1096 (purchase verb) → BL-1097 (sea legs recorded) | `purchase-verb`, `sea-legs-recorded` | " |
| W2 | BL-1101 (band from history) | `band-from-history` | " |
| W3 | BL-1102 (tariff posture derived) | `tariff-posture-derived` | " |

The lanes cannot build the SDL app in a fresh worktree unless FetchContent is allowed; each was told
to compile-check `src/world` through `build_harness.js` and to say so when the app was not built.
**The main session merges, builds Release, runs `world_determinism` twice, `save_roundtrip`,
`save_envelope_roundtrip` and each lane's harnesses, then Ben's live click** — assume nothing about a
self-reported pass (memory `io-cold-review-catches-what-self-report-cannot`).

## Wave 0 returned and is MERGED (2026-09-25, early hours)

All five lanes are on main: G `7bf2bac0` (+ fix round `8889487a`), C `2c5c2746`, W2 `f954a0fc`,
W1 `747a5bf4`, W3 `cc5e51d1`; bookkeeping `eb6c7563`, `38c5a42a`. Gates after G+C and after W2:
determinism twice bit-identical on the pinned digits, both round trips green, Begin-adopts and
seat-pick all pass, `span_seed_isolation` 25/26 (S0 fails by design — NR-931). The gate run on the
full wave-0 tree (with W1's world content: purchases on 2 library seeds, laws on all 16) plus the
exploration and industrialisation harnesses was running when this was written — read
`scratchpad/gates_wave0.log` in the session's transcript or re-run `main_session_gates.sh`.
Expect the pinned determinism digits to MOVE on that tree (world content moved): that is the
re-bless, taken once, after the last world-mover. Wave 1 is in flight: lane I2 (BL-1090 → BL-1094)
and lanes I1 (BL-1087 → 1088 → 1089) / I3 (BL-1106) in worktrees branched from `cc5e51d1`.

**Gate run on the full wave-0 tree (02:06):** world_determinism twice ALL PASS with the pinned
digits UNCHANGED (the harness world runs the ancient arc to 1200, so purchases and laws never
reach it); save_roundtrip and save_envelope_roundtrip green; Begin-adopts (adopt == cold) and
seat-pick all pass; `span_seed_isolation` 25/26 (S0, NR-931); exploration_sim_harness fails
exactly R3b (its regression pin, +400 tribute — the re-bless); industrialisation fidelity 16/16 on
all three gates with the sea-leg producer wired; BL-1056 self-check pass. Wave-0 requirement rows
are closed accordingly (`fa7e3a7f`). Lane I2 (BL-1090 hard borders, BL-1094 marks) merged
`27154072` with a main-session fix round `70b2759e` (the rest seat read off the record; the
civilisation diamond carried across the seam); its two capture scripts and the 16-seed
`player_seed_sweep --digest-check` were running when this was written (logs in the session
scratchpad: `digest_check_wave0.log`). NR-937 asks Ben whether a hard realm's coast draws heavy
and whether 10% of people (off at 6%) is the pin.

**The re-bless reading (02:56):** `player_seed_sweep --digest-check` on the full wave-0 tree: 0/16 rows
PASS — every library seed moved on all four digests (the laws on all 16 from BL-1102; purchases on 25 and
38 from BL-1096); world_determinism's seed B also reads 4F7BBD76AEEF3431 on main now (was
5ECE9A097D14DACE at v0.1.25); exploration R3b +400 tribute. THE RE-BLESS IS TAKEN ONCE, after lane I1
(realm becomes nation, which renames nations and may move any digest that folds names) is merged:
re-pin the 16 sweep rows, R3b's counters, world_determinism's seed B, the seed library, and any
golden that reads laws or names.

**Attribution of the moved pins (03:55):** the G+C-only tree ALSO reads 0/16 against the sweep's pins — which
were already stale from sprint 46 (never re-pinned). So the pins cannot say who moved what; row-for-row
comparison can. Running: the digest check on the pre-sprint tree 1d401e8c (the session
scratchpad's `digest_check_pre47.log`). If its rows equal the G+C rows (`digest_check_GC.log`),
BL-1083 and BL-1085 moved nothing and BL-1085 R1 closes; the full wave-0 rows
(`digest_check_wave0.log`) are then W1/W3's movement and the re-bless re-pins them. RESULT (04:43): pre-sprint and G+C rows are IDENTICAL on all 16 seeds — BL-1083 and BL-1085 moved nothing; BL-1085 R1 closed. Against the full wave-0 tree: D_land/D_settle/D_seat move on every seed (BL-1102's laws) and D_search on 13, 25, 31, 32, 46 (BL-1096/1097). Those, plus BL-1089's name inheritance, are the re-bless.

**Wave 1 complete on main (04:40):** I1 (BL-1087 colour, BL-1088 name, BL-1089 nation) merged
`66bdbc0f` after a fix round on its branch (pins derived on demand at the hand-over; the Begin/load
derivation rasters each record over its own regions; a load never trusts another world's realm
table; the ROSE start carried by id) and the second review's four low notes taken on main
`f81d0d05`. BL-1089 is the sprint's ONE intended digest mover (nation names fold into the deep
digest): world_determinism on the lane read seedA/on 6DBC0094F0B6B0EF, seedB/on 95EEAD1204FD31AC —
confirm on main in `gates_I1.log`. NR-938 records the clash-rule wording the lane changed. Lane I3
(BL-1106) merged `4a8848a7` + `84baaa65`. Wave 2: F3 (BL-1099/1100/1104) resumed on its worktree
after the session limit; F1 (BL-1091/1092) and F2 (BL-1095) launched from `f81d0d05` with kinds 23
and 24 pre-assigned (20 inherited, 21 works_chartered, 22 rung_crossed are I1's and F3's).

**F3 MERGED (06:20):** `845c3fde` (the lane, clean merge) + `b596747c` (its fix round on main: the
three item-spanning rows and REFINED re-pinned to the merged tree's digits; a report row whose corp
is gone still takes the epoch year). Gates in `gates_F3.log`: world_determinism THREE times,
bit-identical -- seedA/on 6DBC0094F0B6B0EF, seedB/on 95EEAD1204FD31AC, seedA/off 4834366D19271E5F,
main's own post-I1 digits, so F3 moves no digest; save_roundtrip OK; save_envelope_roundtrip PASS;
span_seed_isolation 25/26 (S0, NR-931); begin_adopts_check 12/12 adopted == cold; seat_pick_check
ALL PASS; history_lapse_works.lua (40 marks for 40 charters, 124 notes at f = 2) and seat_pick.lua
green. **NR-941 raised:** the seat briefing's origin sentence now reads 'under Guashe These, the
realm of Guashe These since 1660 CE' -- nations carry their realm's coined name, so the two clauses
agree; collapse or keep is Ben's call before the live click on BL-1099 R5. The I1 identity checks
on main all green (five scripts 0 golden failures, realm_names_check ALL PASS) and the 16-seed
history_sweep re-read written into BL-1087 R5 (75 / 5 clashes, was 101 / 18). Twenty-two stale
worktrees pruned (all merged, none ahead); header_graph now reads 329 dangling / 0 state-dependent.
**F1 and F2 have committed on their branches** (F1 `855bdd12` BL-1091 + `7280a9c7` BL-1092 on
`worktree-wf_dc944402-d08-1`; F2 `a5847574` BL-1095 on `-2`) and their workflow's cold review was
still running when this was written -- merge in that order after reading the reviews, then the
single re-bless (F1/F2 are record-only kinds 23/24 and should move no digest; confirm).

**F1 and F2 MERGED (06:50):** `46e244d8` (F1) and `2ccee936` (F2), Release build green. Both cold
reviews returned fix-round on small findings -- F1: the routes R4 row cites the hashed worst seed
where the script pins library seed 9; the kin-arrow dash uses a two-interior-tile sampler while
the hpp names lapse_corridor_over_water (an NR for the threshold: 0 / 58 / 193 of 249 on seed 13);
life_to_people.lua's ticker assertion assumes the rarest tier; and recipe_workforce.lua was not
re-run after verify.new_world became a real rebuild. F2: LANDINGS LEAK ONTO ROUND 4 (seat_captured
has no Exploration gate) -- the one medium; the trade follow-on stacks lines on a churned pair; the
treaty renewal back-fills a 20-year gap; ties standing at 1660 never draw on round 6 (documented,
a later item); the hull assertion keys on navy_peak. Fix rounds launched as workflow
`wf_9839294a-9b4` (two worktrees off `2ccee936`, each re-reviewed cold); gates and the lanes'
scripts running on the merged tree in `gates_F1F2.log` / `scripts_F1F2.log` (the scratchpad).
Conflicts on the merge were insertion collisions only (kinds 21-24 in order; verify_api.cpp and
history_lapse.hpp resolved three-way, F1's block then F2's).

**F1/F2 FIX ROUNDS MERGED (07:25):** gates on the F1+F2 tree green with the digests unmoved
(`gates_F1F2.log`); ten of eleven scripts green, the eleventh (recipe_workforce.lua) went red
because verify.new_world really rebuilds now and the pinned seed's facility has no inputs -- the
old pass was one tick of leftover settle stock; the script stocks its inputs itself through the
new `verify.stock_building_inputs` and is green (`b7726c90`). F1's fix round `cddf6c08` (re-review
merge; NR-942 for the kin-dash threshold) merged `23bc50b3`; F2's `3eea9af0` merged `88a080f1`
with one medium left, fixed on main `894e736e` (a re-binding cut a standing treaty's trade line
for good: `year_trade_due` carried at the refusal, the cut scoped to the same pair; cold-reviewed,
merge). BL-1112 (ties inherited across the 1660 seam) filed from F2's documented gap. All lane
worktrees pruned. **THE RE-BLESS is measuring** (main session, background, keep-awake held):
`player_seed_sweep --digest` on the 16 seeds (shipped arc), then `--digest-check --arc legacy`,
then the library sweep + `seed_library.js --check`; exploration_sim_harness R3b's counters read
from a rebuilt harness. Logs `rebless_measure.log`, `rebless_legacy.log`, `rebless_library.log`
in the scratchpad. Re-pin the shipped table (and the legacy one only if it moved), R3b's pinned
counters with the cause named, `seed_library.js --bless`; rebuild the harnesses; `--digest-check`
both arcs green; one commit.

**THE RE-BLESS IS HELD FOR ITS RIDERS (08:40):** the first measurement is in -- shipped arc: all 16
rows moved (`rebless_measure.log.digest.txt`); legacy arc: all 16 moved from D_search on, the mover
being BL-1089's name inheritance (runs on every arc; I1's A/B is the attribution); exploration
R3b: tribute +400 and nothing else (BL-1096, one purchase on the fixture) -- re-pinned in the
working tree and the harness ALL PASS; library sweep: seeds 25 and 38 moved (expl_battles 613 ->
609, flows 72 -> 76, treasury median 477 -> 18697; 8602106 -> 8599359), 14 unchanged. NOT
COMMITTED, because BL-1082 (the state hash folds the seat) and BL-1049 (the civilisation and
creed tables passed to the 1200 resume) were ruled to RIDE this re-bless and were still unbuilt:
committing now would have forced a second. Both launched as workflow `wf_5d71bddc-67d` (two
worktrees off main, cold-reviewed). When they land: merge, rebuild, gates, then re-measure all
four (`main_session_rebless_measure.sh`, the legacy `--digest-check`, the library sweep,
exploration_sim_harness), re-pin with `repin_sweep_table.py`, `seed_library.js --bless`,
`--digest-check` both arcs green, ONE commit. The riders' own digest movement is recorded
old -> new in their requirement rows.

**RIDERS MERGED (10:10):** BL-1082 `14cd0004` (world_determinism moves: seedA/on 6DBC0094F0B6B0EF ->
0570E3900D0BD53F, seedB/on 95EEAD1204FD31AC -> D5616442F561DDD3, seedA/off 4834366D19271E5F ->
B89B87C393B06FC9, by design -- the seat is now in the hash) and BL-1049 `67993f80` (world_determinism
unmoved: the harness world never seeds its history log -> BL-1113); their review lows on main
`d6558647` (fidelity gate demands the carried prefixes, 16/16 PASS; R3b re-pinned to 228427744,
exploration harness ALL PASS; history_sim_harness at its 2-failure baseline; NR-943). The FINAL
re-bless measurement runs on this tree (`rebless_final.log`: shipped --digest, legacy --digest, the
library sweep); then `repin_sweep_table.py` both tables, `seed_library.js --bless`, `--digest-check`
both arcs, one commit -- the earlier (pre-rider) readings in `rebless_measure.log` / `rebless_legacy.log`
are superseded. Gates on the riders' tree in `gates_riders.log`.

**THE RE-BLESS IS TAKEN (13:38, a fresh session after the overnight one ran out):** both tables
re-pinned from `rebless_final.log`, 16/16 rows each; `seed_library.js --bless` took seeds 25 and
38; rebuilt, `--digest-check` 16/16 PASS on both arcs. One commit. **What is left of sprint 47:**
Ben's live clicks (the five carried plus every merged lane's `visual` row), the open calls below
plus NR-937..NR-943, BL-1083 R7 (the per-span rule in STARTUP.md and the seed library), and
the decision on BL-1084 (the world built once and moved) -- unstarted, the sprint's named risk
and a done-when clause -- build it now or carry it to sprint 48. Wave 3 (BL-1086, BL-1098,
BL-1107) is unstarted stretch. Local main is not pushed.

Calls raised by the lanes, for Ben: NR-931 (the Culture reroll re-coins peoples, never the walk —
accept, dice, or no reroll), NR-932 (settle tick 1 runs 44-62 s), NR-933 (purchase rate 2000‰),
NR-934 (every library seed derives industrial), NR-935 (tariff bands vs the new posture), NR-936
(Next not gated on the running round). Owed to the main session (both DONE 2026-09-25): the stale enactment comment at
`hard_coded_world.cpp:~1936` (rewritten with the F3 merge prep) and the 16-seed
`player_seed_sweep --digest-check` for BL-1085's R1 (the attribution run above).

## How to proceed on the returns (as planned at the launch)

1. Read each lane's report and review (the workflow result JSON; `journal.jsonl` in the transcript
   dir if the result is truncated). A `fix-round` verdict means a follow-up agent on that branch
   (`git worktree add ../fix-<lane> <branch>`), briefed with the reviewer's findings only.
2. Merge in this order, building and verifying after each: **G (BL-1083)** first — every UI lane
   assumes stable per-span seeds; then **W2 (BL-1101)** and **C (BL-1085)** (both touch `app.cpp`
   and `harness_params.hpp`; C's band function body is what W2 replaces); then **W1** and **W3**
   (both in `history_sim.cpp`, different blocks). Reconcile the save versions to ONE envelope bump
   and ONE world bump across the lanes (`next_save_version.js` shows the claims).
3. The gates per merge: Release build (`cmake --build build_rel --target ProjectIo -j` after
   vcvars; `build_app.bat` is the Debug tree), `world_determinism` twice bit-identical, the two
   round trips, the lane's harnesses, `begin_adopts_check.js` and `seat_pick_check.js` for C.
4. Then open **wave 1** (identity, UI): BL-1087 → BL-1088 → BL-1089 with BL-1090, BL-1094, BL-1106
   beside them; write their tasks and groups first (DELIVERY step 0-2). **BL-1084 (the world built
   once and moved)** opens in lane G after BL-1083 lands — it is the sprint's largest item and its
   cursor must be byte-identical to the monolith at every round boundary before any UI lane builds
   on the moved world.
5. **The re-bless** once, after wave 0's world-movers (BL-1096/1097/1101/1102 and, if it lands,
   BL-1086) are on main: authorised in principle (Ben, 2026-09-24); BL-1082 and BL-1049 ride it.

## Carried calls for Ben

- The five live clicks: BL-1068, BL-1072, BL-1073, BL-1076, BL-1080 (he walked into round 4 on the
  Release build tonight but did not file the answers).
- NR-920 (`--epoch 0` once the band is the history's — calendar only, or a `--band` override for the
  ancient sandbox) and NR-921 (switch sea legs on for the later spans; the port bonus is dead code
  there today).
- NR-918..NR-930 are the delegated readings; each can be overturned before its lane merges.
- BL-1071's next reading (the Logistic Point cap on market exports: "measure more first").

## Hazards this session added

- **header_graph.js is polluted by stale agent worktrees.** Fourteen `.claude/worktrees/agent-*`
  copies are swept into its index, so its totals (1290 dangling) are not comparable to a clean tree
  (82). Run `node tools/session/worktree_prune.js` (check nothing live is in them first) or compare
  per-doc `--doc` MISS lines, which are honest.
- **Big JSON evidence files in `docs/development/drafts/` swamp header_graph** (each `file:line` and
  `§` in them is counted). The two reader-workflow outputs were therefore NOT committed; the rulings
  record cites what they found.
- **Closing the app during any build crashes on exit** (BL-1108, lane C). Until it lands, quit from
  the menu, not mid-wait.
- Keep-awake for the lanes' harnesses is held by `tools/session/keepawake.ps1 -WhileFile
  <scratchpad>/wave0.lock`; delete the lock file when the lanes are done.
