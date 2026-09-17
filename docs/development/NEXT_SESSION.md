# Next session — finish sprint 44, then rule NR-889

Written 2026-09-17, with sprint 44's cost sweep still running. **Read this, then
`node tools/session/backlog_query.js --status designed --full`, then the three open review entries
in `docs/development/NEEDS_REVIEW.md`.**

## Where things stand

- **Sprint 43 (the 1960 baseline) is closed.** Four items, no shipped world moved. Its gate NR-888 is
  ruled: Digitisation owns both the displacement force and the binding of far pairs, on Exploration's
  4-year band. `DIGITISATION.md` § 3, § Far pairs meet and bind, and § 1 carry it.
- **Sprint 44 (the corporate web's plumbing) is three items of four done.** BL-1030 (parity),
  BL-1031 (the world pin) and BL-1032 (the charter budget seam) are closed; BL-1033 (the cost sweep)
  is built and its sweep was running when this was written.
- **The seam is off by default and pinned.** `player_seed_sweep --digest-check` holds all 16 pinned
  seeds with no budget, an empty budget, an all-zero budget and a refused one.

## First thing: pick up the sweep

```
./build_gen/verify/player_seed_sweep.exe --charter-cost --seeds 0,28,46 --out charter_cost_sweep.json --note "<conditions>"
```
It writes `charter_cost_sweep.json` after **each seed**, so a partial file is usable: check its
`aborted` field (null when complete) and which seeds it carries. If it finished, the close-out is:

1. Read the table per seed, never pooled: charters and unspent points by reason, search ms per
   evaluation, ms per economy tick over the validation and live windows (a stated **lower bound** —
   comms, history recorders and the strategy readout are not timed), the seat shortlist and its
   trailing-net spread, at the per-resource cap **kept and lifted** and at specialist prices 4 and 8.
2. File one review entry with those readings, so Ben can rule **NR-889**: his 2026-09-06 cap ruling
   (a body holds 8 firms per demanded good) against his 2026-09-17 one (the budget decides how many
   firms stand where). He chose to measure both first.
3. Close BL-1033 against `requirements.json § charter-budget-cost` (R1-R7), check the table in, then
   close sprint 44 with its retro.

## Open review entries

- **NR-886** — items 1, 2, 4, 5, 6 of wave A's calls. Item 3's figure was re-read on the app's world
  (74.3% of the shortlist has negative trailing net, not 93.8%); item 7 is answered.
- **NR-889** — the density cap, above. Ben: measure first.
- **NR-890** — scheduling **BL-1034** (a copied world does not tick byte for byte as its original:
  seed 28 settles to D_settle 18F78EB9B2B20F29 against the pinned 265C48A23E313B1A). Latent today;
  the recommendation is early in sprint 45, before Beat 1 or the wizard's time-lapse round wants a
  ticking copy.

## Then sprint 45 (proposed): industrialisation makes the web real

Its goal row is in `sprints.json`. Owed when its items are cut: whether consolidation and the
near-home cutoff re-anchor at 1660; a resume path for dated objects and trade flows; whether Beat 1's
first cut carries the three sinks or only stockpiles; and the no-specialist world Ben deferred.

## Tools this sprint left

- `exploration_sweep --seeds a,b --out path --through Y --cost` — per-half weakness counters with a
  prefix check when Y > 1660.
- `digitisation_sim_harness --through Y --out path` — the thirteen readings beside a
  fingerprint-checked 1660 control.
- `player_seed_sweep --digest | --digest-check [--charter-budget none|empty|zero|synthetic|refused]`
  — the four world digests per seed against the BL-1031 pins. **Never re-pin these**; a failing row
  names its digest, and the tamper tests show each digest fails at its own phase.
- `player_seed_sweep --charter-cost [--budget-scales --resource-cap --province-cap
  --specialist-prices --ladder-scales --live-ticks --out --note]` — the cost matrix.
- `seed_library.js --seed-list | --from path` — the library reads `seed_library_sweep.json`.

## Standing hazards

- **Timings need a quiet machine**, serial, in Release, with the build tree quoted. A 16-seed digest
  check is ~55 min; a cost row is 200-450 s; the readings harness is ~24 min for the library.
- **Never baseline on `epoch_year = 1960`** — that is the superseded two-span arc with Exploration
  off. "Through 1960" is `world_params::exploration_stop_year`.
- **Do not copy a world and then tick it** until BL-1034 is fixed.
- Regenerate sweep artefacts on the **final integrated tree**; write measuring runs to `--out`.
- Mint `NR-` ids against the **hot store and the archive together**.
- `history_sim_harness` carries a tracked 2-failure baseline (R3a2/R3a3); `story_check` fails 2 on
  US-016 (no surfaces, no requirement briefs) — both pre-existing, neither this sprint's.
