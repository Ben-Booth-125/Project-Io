# Next session — sprint 33, the growth half

Sprint 31 closed 2026-09-02 a success on solvency: on the standard industrial lapse the field ends
thirty years with a majority of corps operating-positive (46 of 61, 31 of 55) where it began with
four and none, debtors a tenth of the field, median balances climbing. Sprint 33 owns what is left:
**valued production still falls across the run** (×0.2 on seed 0, ×0.6 on seed 1, from a level ten
to twenty times the old baseline). `docs/development/SPRINTS.md` § Sprint 33 is the plan; this note
is the handoff.

## Order of work

1. **BL-746 stage 2 (the generation bootstrap, NR-782 (c) held).** The field's mean supply factor
   sits at ~0.57 — most buildings run at the new floor because power does not arrive (generation
   18 → 3 units against a demand of 64; a generator short of power throttles itself; only
   network-reached tiles can receive it). Design it so a fix that only silences the draw reads as
   one: measure the supply-factor trend AND the power price together.
2. **BL-745 (processor input bid cap).** 42 of 57 remaining debt entries are processors producing
   less than they buy — construction materials at 8–10× base through the boom, and inputs above the
   recipe's output value. The anchor's M1 identity carried to the live tick.
3. BL-738 re-measure, then BL-726 (seed 1's interest is still 70% of net loss), then BL-725.

## The instrument

```
cmd //c tools\verify\build_lua_harness.bat campaign_lapse
./build_gen/verify/campaign_lapse.exe --epoch 1960 --seed 0 --warm 0 --ticks 60 --tag <tag>   # where debt begins
./build_gen/verify/campaign_lapse.exe --epoch 1960 --seed 0 --tag <tag>                        # the done-when form
```

`corps.csv` carries every corp's balance delta attributed by tick phase (residual asserted zero),
produced value, building state counts, labour and supply factor; `debt.csv` one row per debt entry
with the dominant drain. The 2026-09-02 traces to compare against: `final-ind-s0/s1` (standard
form, the sprint-33 baseline), `fix-ind-s0/s1` (unwarmed), `exp-noupkeep` (the zero-draw control),
`debt-ind-s0/s1` (the cliff, before the fix). The aggregator pattern is in the devlog entry
"every balance tracked".

## Traps (still true)

- Lua-linked harnesses build via `cmd //c tools\verify\build_lua_harness.bat <name>`; run as
  `./build_gen/verify/<name>.exe` from the repo root. `nmake all` stops at
  `battle_engagement_harness` (BL-731's sibling rot) and everything after reads as ctest "Not
  Run"; build the target you need. `ctest -j6` is unusable — Debug world-building harnesses hit
  the 60 s timeout under contention.
- A function called from `io_world_obj` must not live in a Lua-linked TU (world_gen_config.cpp,
  recipe_registry.cpp, tech_tree.cpp), or every Lua-free harness fails to link.
- chain_depth's one red row is the DELIBERATE named-list guard. Do not quiet it.
- Every remembered seed-0 number from before 2026-09-02 is stale twice over (the anchor, then the
  floor). Re-baseline from `final-ind-s*`.
