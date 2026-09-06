# Next session — sprint 32b, the water model

Sprint 32a closed 2026-09-06: five items delivered, the arc runs on the 1960 arc as two spans, and
four measuring instruments were found describing something other than their subject. 32b carries the
remaining 29 items. `docs/development/SPRINTS.md` § Sprint 32b is the plan; this note is the handoff.

## Start here

**Wave 1 is the water model, BL-776 → BL-780, in that order.** It is self-contained, it moves every
generated world, and BL-780 exists so it moves **once**.

1. **BL-776 (coastal territory)** is one predicate. `nation_generation.cpp:689` builds its
   unclaimable mask from `is_water` — coast, lake *and* ocean — so the carve refuses every water
   tile. Narrow it to `is_open_ocean`. Coastal provinces then become owned for free, because
   province ownership derives from tiles.
2. **BL-777 (region domain)** stops Settle founding on open ocean — ~613 regions, not the ~1105
   BL-756 originally proposed deleting. Save-format bump: `region` has a positional read chain.
3. **BL-778 (unit traversal domains)** adds the field `roster_row` lacks. Land units may cross
   **owned** coastal water.
4. **BL-779 (naval units become real)** — `unit_class::naval` returns base power 0 and `sum_stack`
   skips the class outright, so three authored, port-gated, raisable rows are worth nothing today.
5. **BL-780 (one re-bless)** closes the wave. Do **not** let 776–779 each re-bless.

## Before-figures, already captured — do not re-measure

```
world_determinism   039EE9880739CDF6 / B0EBBA249B3DDABB / DE55600457797638
era report seed A   years=400 battles=270 conquests=207 foundings=833
1960 two-span       1160 -> 1560 -> 1960, digest DB86651B9A596F7B
sim_water_census    1105 of 3819 regions on water (982 sea, 613 OPEN OCEAN)
                    43% of adjacency edges cross sea (54% / 22% / 57% by seed)
warm start          72-73 s on BOTH arcs, ~12x the ~6 s app.cpp budgets
generation          ~8.2 s Release; the era pass itself only 197-323 ms
```

## Four calls waiting on Ben, three of which block work here

- **BL-758** — does era-seeded demography at 1960 belong, or is it scope BL-747 never claimed?
  `era_world_harness` R2 is deliberately **RED** for it. Do not weaken it to pass.
- **Water's 0 forage** — blockade pressure, or an accident of a table written for land?
- **Can a coastal province hold a port?** Buildings currently refuse water outright.
- **NR-783** — is the span boundary authored at epoch − 400, or derived from the first furnace?

## Debts from 32a, stated rather than hidden

- **The batch's cross-slice review barrier never ran.** Sub-agent capacity was unavailable for the
  whole session — 12 launches, every one a 529 with zero tool calls — so each slice was verified
  individually instead. If agents are available, run a cold review over the five delivered items.
- **Three harnesses are ad hoc** pending Ben naming them in `.claude/skills/verifier-headless/`:
  `continent_drift`, `sim_water_census`, and the promoted saturation measure.
- **BL-774** — worktree agents cannot build the Lua-linked harness class, which includes
  `world_determinism`. Every agent inheriting the byte-identity invariant pays that toll first.

## Two traps this session hit, worth not repeating

**A green check is not evidence that it looked.** Four instruments were measuring something other
than their subject, and one of them (`era_world_harness`) let a real regression through a wave I had
called verified.

**A requirement written after the code describes the code, not the intent.** BL-775 said "promote
both"; half was promoted and everything went green on it. Write the group from the item, first.
