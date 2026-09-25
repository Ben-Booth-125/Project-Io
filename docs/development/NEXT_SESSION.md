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

Calls raised by the lanes, for Ben: NR-931 (the Culture reroll re-coins peoples, never the walk —
accept, dice, or no reroll), NR-932 (settle tick 1 runs 44-62 s), NR-933 (purchase rate 2000‰),
NR-934 (every library seed derives industrial), NR-935 (tariff bands vs the new posture), NR-936
(Next not gated on the running round). Owed to the main session: the stale enactment comment at
`hard_coded_world.cpp:~1936` (W3's finding), the 16-seed `player_seed_sweep --digest-check` for
BL-1085's R1 (run it once, before the re-bless, with keep-awake held).

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
