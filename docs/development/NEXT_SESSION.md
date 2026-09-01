# Next session — Sprint 27, block 3: the channels

Sprint 27 is still **open**. Block 2 (2026-09-01) closed the four items ordered ahead of the
channels, took BL-417, fixed the sprint's own primary instrument, and drained the review queue.
What is left is the channels.

## The review queue is EMPTY, and it is meant to stay that way

117 open entries → **0**, on Ben's instruction: *"we are relying on the fact there is a review queue
too much. Use judgment on things that seem obvious in hindsight, and close all the remaining
items."* Most of them named **work**, not a judgement, so ten backlog items now carry it —
**BL-713**…**BL-722**. Every entry is in `archive/needs-review-2026-Q3.json` with a resolution
saying where it went.

`CLAUDE.md` Rule 0c gained the discipline that keeps it drained. Before filing, ask which of three
things you have: **a call only Ben can make** → the queue; **work somebody must do** → a backlog
item; **a fact worth remembering** → the comment or doc next to the code. An observation with no
reader is not a record.

## What changed under you — read before trusting any older number

1. **`demand_census` surveyed nothing.** It never called `init_survey_states`, which `app.cpp` runs
   at campaign start, so every body — home included — stayed `hidden`. `rank_extraction_sites` gates
   on survey visibility, so **the corp AI built zero extraction sites in every census ever run.**
   Not fewer. Zero. Fixed (`8fe177dc`): **any census number from before 2026-09-01 is not
   comparable.**
2. **`ai_skill_harness` is blind to per-category work.** Its hand-built registry holds three recipes
   with `group` set on none, so all three fall to "General". BL-712 returned byte-identical numbers
   before and after — that identity is the proof, not a null result. **The sprint's stated success
   criterion is this harness**, so until BL-713 lands, read the criterion as
   `chain_conversion_probe` + `demand_census` instead.
3. **The build score is linear.** BL-417 replaced `net² / capex` with `net / capex`. Every score
   magnitude in the AI moved by two orders; anything comparing against a remembered score is stale.

## Order of work

**1. BL-642 (construction draws) — the fork is RULED, build it.**
Ben ruled option B on NR-773: a population centre's growth **draws construction materials AND
stretches on them**, exactly as `run_construction` stretches a build. One rule for both consumers.
The hook is `economy_system.cpp`'s BL-616 growth block, where `pcc.population` steps and
`++pcc.scale` promotes.

**Design for the BL-641 cliff, do not discover it.** A brand-new universal draw unmet on tick 1
collapsed operating firms 198/328 → 33/317. Two things make gating survivable here and both must
hold: growth is **episodic** (a step every 10 qualifying ticks, not a per-tick draw), and the
existing `max_stretch` machinery already models *slower, not dead*. Floor the behaviour at stretch
rather than at stop, and measure against the census before authoring a non-zero rate.

Half (1) is separate and still open — its premise moved. `ERAS.md` settles that "the first 20 years"
ARE the 80-tick warm start, and the warm start now constructs: ancient construction-channel demand
went 93.25 → 202.58 → 210.08 across the census fix and BL-711. What remains is the seeder's ~330
buildings being authored complete, and fixing that literally opens the campaign with 330 half-built
buildings — a gameplay change, not plumbing.

BL-642 also owns **NR-770** (routed there): industrial construction yards are *removed* inside 80
ticks and ancient ones stand idle producing 0.0 with both inputs on the shelf. The ancient half is
the cheaper diagnosis. That is also what holds **BL-709 R1** open, so the two close together.

**2. BL-644 (state channel), then BL-647 (endemic luxury).**
BL-647 carries a prerequisite the item does not name: tobacco, spices, coffee and furs are **not in
`placement_rules::k_extractable`**, so no extraction site can target them at all (BL-586 slice 2
recorded that gap and left it). A luxury basket naming four goods nothing can mine is a channel that
cannot clear.

**3. BL-643 / BL-646 / BL-645** at priority B.

**4. BL-713 (harnesses build the app's world) — AFTER the sprint closes**, per Ben's ruling on
NR-762. One golden wave, after the work that would otherwise move them twice.

**Run `demand_census` before and after every item** — it takes ~36 s and now reports a world where
the AI actually builds. `--reach` sweeps the logistics budget (NR-763's probe).

## Traps

- **Lua-linked harnesses are a much bigger class than any list says.** `harness_params.hpp` pulls
  `scripting/lua_state.hpp`, so **any** harness including it needs
  `cmd //c tools\verify\build_lua_harness.bat <name>`. `build_harness.bat` fails them with
  `fatal error C1083: 'sol/sol.hpp'`, which reads as broken code rather than the wrong builder.
  Confirmed in the class: `save_roundtrip`, `world_determinism`, `determinism_harness`,
  `recipe_switch_harness`, `build_spree_harness`, `decision_trace_harness`, `ai_skill_harness`,
  `spectator_determinism`, `demand_census`, `chain_depth`, `spawn_solvency`,
  `chain_conversion_probe`, `upkeep_harness`. BL-713 owns deriving this instead of listing it.
- **Run harness exes as `./build_gen/verify/<name>.exe`.** `cmd //c "build_gen\verify\$h.exe"` with a
  shell variable eats the backslashes and reports "not recognized as an internal or external
  command", which looks like a missing build.
- Build: `cmd //c "<repo>\build_app.bat"` — absolute path; backgrounding it is fine.
- **Worktrees are cut from a diverged `origin/main`.** `git switch -C <branch> main`. Never
  hard-reset to `origin/main`.
- Source files are **CRLF in the working tree, LF in the blob**. A Python patcher that reads text and
  rejoins on `\n` rewrites every line ending and buries a two-line change in a 1000-line diff. Read
  and write binary, keep `\r\n`, and check `git diff --numstat` before believing a change is small.

## Known-red

- `ai_skill_harness` — 25 band failures, stale since 2026-08-16. BL-417 improved three of five seeds
  and the aggregate; the bands themselves are still unblessed and that is deliberate.
- `chain_depth` — 8 pre-existing `injector::none` rows.
- `spectator_determinism` — **green.** Its byte-identity row was retired on Ben's ruling (it asserted
  world-content stability, not a spectator-mode property, and had been re-blessed ten times for
  changes that were never spectator facts). Both real invariants still assert.

## What sprint 28 is waiting for

Unchanged: BL-697, BL-699 and BL-698 are **proposed**, not open — the systemic brake needs a leader
worth forming a coalition against. Read `sprints.json` § 28 before touching them.
