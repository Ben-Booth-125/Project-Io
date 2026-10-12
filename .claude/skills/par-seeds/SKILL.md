---
name: par-seeds
description: Run a Project Io seed-sweeping harness (market_viability, player_seed_sweep, industrialisation_sim_harness, nation_targets_harness, history_patterns, treasury_census, ocean_currents_harness, history_sweep, exploration_sweep) across its seeds IN PARALLEL and merge the verdict. Use for every multi-seed gate instead of the serial run; pick the quick tier while iterating and the full tier at merge.
---

# par-seeds

`tools/verify/par_seeds.sh` splits a harness's seeds over one process per core
(16 threads on this machine) and merges the shards with
`tools/verify/par_seeds_merge.js`. Each shard is the unchanged harness handed
`--seeds <shard>`, so its output is exactly the serial run's for those seeds;
pooled figures (market_viability's gates) are recomputed from the shards'
absolute numerators, never averaged. Measured 2026-10-12: market_viability over
the 16 library seeds 20 min serial -> 162 s, identical pooled gates.

## The two gate tiers (DELIVERY.md § The Delivery lifecycle, step 4)

| Tier | Seeds | When |
|---|---|---|
| `--tier quick` | 0, 28, 46, 12 (reference, small, rich, inward) | iterating, calibrating, mid-fix-round |
| `--tier full` (default) | the 16 of `docs/generation/seed_library.json` | the merge gate |

`--range N` sweeps 0..N-1 (player_seed_sweep's guard is defined on `--range 12`);
`--seeds a,b,c` names worlds outright.

## Use

```bash
bash tools/verify/par_seeds.sh market_viability --build
bash tools/verify/par_seeds.sh player_seed_sweep --range 12 -- --guard
bash tools/verify/par_seeds.sh industrialisation_sim_harness -- --through 1960
bash tools/verify/par_seeds.sh nation_targets_harness --tier quick -- --pairs 6:1,40:6
```

Harness args go after `--` and stay first on its command line (a mode flag that
must be argv[1] keeps its place). `--build` compiles the harness first through
`build_lua_harness.sh`; `-j N` caps the shards; shard files land in
`build_gen/verify/out/par/<harness>/`. Exit 0 only if every shard exited 0 —
read the MERGED block for the gates themselves (market_viability exits 0 on a
failed gate, as its serial run does).

## Rules

- **One heavy parallel run per machine at a time — enforced:** the runner queues on a lock in the main checkout. A shard set already uses
  every core; two at once only slow each other. Queue them.
- Not for the fixed-fixture harnesses (`world_determinism`,
  `history_sim_harness`, `exploration_sim_harness`, `realm_names_check`) — they
  take no seed list; run them directly.
- A sub-agent brief names the tier: quick while it iterates, full once before
  it reports a merge candidate.
