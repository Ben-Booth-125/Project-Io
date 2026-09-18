# Next session — cut sprint 45, starting with BL-1034

Written 2026-09-18, at the close of sprint 44, with the review queue ruled and empty. **Read this,
then `node tools/session/backlog_query.js --status designed --full`, then
`docs/generation/DIGITISATION.md` § 1 and § Beat 1.**

## Where things stand

- **Sprints 43 and 44 are closed.** The 1960 baseline is measured, and the corporate web's plumbing
  is in: a per-centre charter budget reaches the landscape search and charters specialists and
  background firms around each centre, **off by default** — the app passes no budget, and
  `player_seed_sweep --digest-check` holds all 16 pinned seeds with no budget, an empty budget, an
  all-zero budget and a refused one.
- **The hot backlog holds two items:** BL-1034 (world copies diverge, sprint 45, first) and
  BL-1035 (harness windows long enough, unscheduled).
- **Sprint 45 (industrialisation makes the web real) is a proposed goal row** in `sprints.json`.

## The review queue is empty (ruled 2026-09-18)

- **NR-889:** on a budget world the per-resource cap **scales with the body's charter capital**
  (`DIGITISATION.md` § 1); the scaling rule and the specialist's price are sprint 45's to set, on
  real stockpiles, against `charter_cost_sweep.json`.
- **NR-890:** BL-1034 (world copies diverge) comes **first** in sprint 45, ahead of Beat 1.
- **NR-886:** coastal cheapness and the trade re-base accepted as built; seat solvency decided with
  sprint 45's specialist-capital call; the three red harness rows filed as BL-1035 (harness windows
  long enough); the small-grudge fade accepted and documented; the culture fold accepted.

## Then cut sprint 45

Its goal: Digitisation runs 1660 → 1960 from the `exploration_output` struct, the Industry tree is
invested in, large cities accumulate located industry points, and each centre's unspent points
become the charter budget sprint 44's path spends — one re-bless, one cold review. Owed when its
items are cut: whether consolidation and the near-home cutoff re-anchor at 1660; a resume path for
dated objects and trade flows; whether Beat 1's first cut carries the three sinks or only
stockpiles; what a world with no affordable specialist gets (Ben deferred it here); the rule by
which the per-resource cap scales with charter capital, and the charter prices, both on real
stockpiles against `charter_cost_sweep.json`; and whether a charter price becomes starting
capital, which also decides the seat's solvency question.

## Tools the two sprints left

- `exploration_sweep --seeds --out --through Y --cost` — per-half weakness counters.
- `digitisation_sim_harness --through Y --out` — the thirteen readings beside a 1660 control.
- `player_seed_sweep --digest | --digest-check [--charter-budget none|empty|zero|synthetic|refused]`
  — four world digests per seed against the BL-1031 pins. **Never re-pin them.**
- `player_seed_sweep --charter-cost [...]` — the cost matrix (`--budget-scales --resource-cap
  --province-cap --specialist-prices --ladder-scales --live-ticks --out --note`).
- `seed_library.js --seed-list | --from path` — the library reads `seed_library_sweep.json`.

## Standing hazards

- **Keep the machine awake for long sweeps.** The PC slept 22:07 → 09:03 mid-sweep on 2026-09-17.
- **Timings need a quiet machine**, serial, Release, build tree quoted. A 16-seed digest check is
  ~55 min; a cost row 200-1,000 s; the readings harness ~24 min.
- **Never baseline on `epoch_year = 1960`** (the superseded two-span arc, Exploration off).
- **Do not copy a world and then tick it** until BL-1034 is fixed.
- Write measuring runs to `--out`; regenerate tracked artefacts on the final integrated tree.
- Mint `NR-` ids against the hot store and the archive together.
- `history_sim_harness` carries a tracked 2-failure baseline (R3a2/R3a3); `story_check` fails 2 on
  US-016 — both pre-existing.
