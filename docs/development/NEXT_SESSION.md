# Next session — Sprint 27, block 3: the channels, and two instruments that were lying

Sprint 27 is still **open**. Block 2 (2026-09-01) closed the four items NEXT_SESSION ordered ahead
of the channels and fixed the sprint's own primary instrument. What is left is the channels — and
one design fork that blocks the first of them.

## What changed under you, and it matters before you read anything else

**Two of this sprint's instruments were blind, and both were found by accident.**

1. **`demand_census` surveyed nothing.** It never called `init_survey_states`, which `app.cpp` runs
   at campaign start. Every body — home included — stayed `hidden`, and `rank_extraction_sites`
   gates on survey visibility, so **the corp AI built zero extraction sites in every census ever
   run.** Not fewer. Zero. Every extraction figure the census has printed was the seeder's
   placement, frozen. Fixed (`8fe177dc`, NR-772); it moves every reading the file produces, so any
   census number you remember from before 2026-09-01 is not comparable.
2. **`ai_skill_harness` cannot see a per-category change.** Its hand-built registry holds three
   recipes and sets `group` on none, so all three fall to the default "General". BL-712 returned
   **byte-identical** numbers before and after — that is the proof, not a null result (NR-771).
   **The sprint's stated success criterion is this harness.** Until NR-771 is answered, read the
   criterion as `chain_conversion_probe` + `demand_census` instead.

Both are NR-762's family — ~30 harnesses skipping the app's world-building tail — and that question
is still unowned. Two expensive instances in two sessions is now the argument for taking it.

## Order of work

**1. BL-642 (construction draws) — BLOCKED ON NR-773, so read that first.**
Its half (1) premise moved under it. `ERAS.md` settles that "the first 20 years" ARE the 80-tick
warm start, not a separate generation pass, and the warm start now constructs: ancient-band
construction-channel demand went 93.25 → 202.58 → 210.08 across the census fix and BL-711. What
remains of half (1) is narrower and bigger — the seeder's ~330 buildings are authored complete and
draw nothing, and fixing that literally opens the campaign with 330 half-built buildings. Half (2)
(centres draw as they grow) has a clean hook in `economy_system.cpp`'s BL-616 growth block and needs
exactly one call: does the draw **gate** growth, or only register a want. NR-773 has A/B/C and a
recommendation.

**2. BL-644 (state channel), then BL-647 (endemic luxury).**
BL-647 carries a prerequisite the item does not name: tobacco, spices, coffee and furs are **not in
`placement_rules::k_extractable`**, so no extraction site can target them at all. BL-586 slice 2
recorded that gap and left it. A luxury basket naming four goods nothing can mine would be a channel
that cannot clear.

**3. BL-643 / BL-646 / BL-645** at priority B.

**4. BL-709 R1**, once NR-770 is diagnosed.

**Run `demand_census` before and after every item — and note it now takes ~36 s and reports a world
where the AI actually builds.**

## Traps, including two new ones

- **Lua-linked harnesses are a much bigger class than any list says** (NR-767). `harness_params.hpp`
  pulls `scripting/lua_state.hpp`, so **any** harness including it needs
  `cmd //c tools\verify\build_lua_harness.bat <name>`. `build_harness.bat` fails them with
  `fatal error C1083: 'sol/sol.hpp'`, which reads as broken code rather than the wrong builder. Five
  of eight harnesses reached for in one block hit this. Confirmed Lua-linked so far:
  `save_roundtrip`, `world_determinism`, `determinism_harness`, `recipe_switch_harness`,
  `build_spree_harness`, `decision_trace_harness`, `ai_skill_harness`, `spectator_determinism`,
  `demand_census`, `chain_depth`, `spawn_solvency`, `chain_conversion_probe`, `upkeep_harness`.
- **Run harness exes as `./build_gen/verify/<name>.exe`.** `cmd //c "build_gen\verify\$h.exe"` eats
  the backslashes when `$h` is a shell variable and reports "not recognized as an internal or
  external command", which looks like a missing build.
- Build: `cmd //c "<repo>\build_app.bat"` — **absolute path**.
- **Worktrees are cut from a diverged `origin/main`.** `git switch -C <branch> main`. Never
  hard-reset to `origin/main`.
- Source files are **CRLF in the working tree, LF in the blob**. A Python patch script that reads
  text and rejoins on `\n` rewrites the whole file and buries a two-line change in a 1000-line diff.
  Read/write binary and keep `\r\n`.

## Known-red, reported and NOT re-blessed

- `spectator_determinism` R2 byte-identity: `golden=E350DF2A50BF4BAA observed=90BFB27CB57CC308`.
  Stale by 150+ commits before this block (NR-752); BL-711 moved it further. **Ben's.**
  Determinism itself is intact — R2's own reproducibility row and all of R3 pass.
- `ai_skill_harness` — 25 band failures, unchanged in count since 2026-08-16 (NR-305).
- `chain_depth` — 8 pre-existing `injector::none` rows.

## Open questions carried forward

- **NR-769 is the big one.** BL-712's fix works — `Power Generation` and `Construction` went from
  **zero** build candidates to 168 and 430 — and they still never win, because `net²/capex` is
  itself an absolute contest (peaks: Advanced Fabrication 1884, Power Generation 31.9). The rule in
  `AI_OPPONENT.md` § Selection must be scale-free condemns that curve in as many words; § Scoring
  says its retention is **BL-417, Ben's call**. Four options with trade-offs are in the entry.
  **BL-711 left the same fingerprint independently**: peat reaches the scorer, both its slots are
  placeable, and it still gains no site.
- **NR-770** — construction yards are removed on the industrial band and produce 0.0 on the ancient
  one. Holds BL-709 R1 open; the ancient half is the cheaper diagnosis.
- **NR-771, NR-772, NR-762** — the blind instruments, above.
- **NR-763, NR-765, NR-766** — carried in from block 1. NR-765 is now answered: BL-711 and BL-712
  are its two fixes and both have landed.

## What sprint 28 is waiting for

Unchanged: BL-697, BL-699 and BL-698 are **proposed**, not open — the systemic brake needs a leader
worth forming a coalition against. Read `sprints.json` § 28 before touching them.
