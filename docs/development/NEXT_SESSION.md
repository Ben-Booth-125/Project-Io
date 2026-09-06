# Next session — sprint 32b, after the market batch

The market batch landed 2026-09-06: **BL-774, BL-759, BL-760, BL-770 slice 1**. All four
requirement groups complete. `docs/development/SPRINTS.md` § Sprint 32b has the full record;
this note is the handoff.

## Start here — the batch produced one decision and it is Ben's

**BL-770 slice 1 returned the negative result.** The phase 6 objective **does not discriminate
between candidate rosters**. Five candidates on one fixed world — `corporation_count` 4/8/16 and
two placement seeds, exactly the axes Ben's point 3 names — score *identically*, every term at
relative range `0.000e+00`. The positive control moves `1.000e+00` across three different worlds,
so the scorer sees plenty; it just cannot see a roster.

The cause is structural, not tuning: every term reads tiles, markets and population, and
`src/world/market_saturation.cpp` contains neither "corporation" nor "building". The objective
measures the **world's saturation potential** — whether a chain *could* close within reach of a
market — and is silent on whether any firm closes it.

**The owed fix, and it is BL-770 slice 2:** a roster-aware term. The natural shape is **actual
against potential completeness** — of the terminals a market *could* close, how many are closed by
a building that actually exists in its catchment. Small, because the scorer already walks the
catchment. Until it exists, the parallel search is not worth a line of code.

## Three things the batch changed that other items believed

1. **BL-726's premise is void.** Seed-1 interest share of net loss went **70% → 7%**. The
   asymmetry the item exists to chase is gone, and sprint 33's interest done-when is *already met*.
   Re-scope it against the re-based numbers or close it — Ben's call, recorded on the item.
2. **Sprint 33's baseline is a third off.** Valued production falls **×0.57 / ×0.68**, not
   ×0.21 / ×0.61. The growth gate is still unmet on both seeds, but the gap is much smaller than
   the sprint was written against. Op-positive is better too: 48 of 55 and 50 of 63.
3. **`build_harness.js` had never worked**, for anyone, from any shell — a `spawnSync` double-wrap
   split the vcvars path at its first space and `>nul` swallowed the error. The whole non-Lua
   verifier-headless tier was unbuildable. Fixed.

## Building a harness — no more archaeology

```bash
node tools/verify/build_harness.js <name>        # SDL/Lua-free world superset
bash tools/verify/build_lua_harness.sh <name>    # needs a live Lua state
```

`build_harness.js` now **derives** which builder a harness needs (21 of 138, against a hand list
that named four) and refuses with the reason and the exact command. A harness failing on
`sol/sol.hpp` or `LNK2019` is the **wrong builder, not broken code**.

## The chain past BL-770 is unchanged, and still a sequence

| Item | Blocked on |
|---|---|
| BL-772 (retire warm start) — the 72 s budget win | BL-770 |
| BL-768 (roads and markets from history) | BL-766 (population map early), d5, not started |
| BL-750 (tariff posture) — design settled 2026-09-06 | BL-748 (industrial pass ladder) |
| BL-752 (colonial ties) | BL-749, held on NR-785's five calls |

## The water wave is still deferred, not withdrawn

BL-776 → BL-780 keep their shape and their before-figures. BL-780 exists so the water model moves
every world **once**; do not let 776–779 each re-bless.

```
world_determinism   039EE9880739CDF6 / B0EBBA249B3DDABB / DE55600457797638
1960 two-span       1160 -> 1560 -> 1960, digest DB86651B9A596F7B
sim_water_census    1105 of 3819 regions on water (982 sea, 613 OPEN OCEAN)
                    43% of adjacency edges cross sea (54% / 22% / 57% by seed)
warm start          72-73 s on BOTH arcs, ~12x the ~6 s app.cpp budgets
```

All four determinism digests were **re-confirmed unmoved** after this batch touched
`history_sim.cpp`.

## Calls still waiting on Ben

- **BL-726** — re-scope or close, now its premise is void (new, from this batch).
- **NR-783** — span boundary: authored at epoch − 400, or derived from the first furnace?
- **NR-784** — cap the ancient arc at medieval? BL-760's new per-band counters can now *answer*
  this: over 16 seeds nothing above medieval is ever fielded, so the cap looks free. Worth
  deciding with that number in hand.
- **NR-785** — BL-749's five design calls, which hold the sea leg and therefore BL-752.
- **BL-758** — era-seeded demography at 1960. `era_world_harness` R2 is deliberately **red**.
- **Water's 0 forage**; **can a coastal province hold a port?**
- **Three ad-hoc harnesses** still want naming in `.claude/skills/verifier-headless/SKILL.md`:
  `continent_drift`, `sim_water_census`, the promoted saturation measure — and now a fourth,
  `landscape_score_harness`.

## One economic signal nobody asked for

Mean supply:demand **balance is 0.008** — under 1% of priced resources sit within a 4× band of
their own demand. Almost everything is glutted or starved. That is a static shadow of the illness
sprint 33 is chasing dynamically, and it is visible without running a single tick.
