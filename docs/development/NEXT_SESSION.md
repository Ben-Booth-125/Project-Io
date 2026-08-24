# Next session — Sprint 17, wave 1

Handoff written 2026-08-23. Self-contained: you should not need this session's
transcript.

## What's true right now

**Sprint 17 (v0.1.17, "the ancient roster becomes a ladder") is authored and
one item deep, not closed.** Ten items, `BL-585`–`BL-594`, filed against the
measured code rather than the archived designs — see `sprints.json`'s `"17"`
entry for the full re-plan and the six rulings behind it (elicitation form,
2026-08-23). `BL-587` (interchangeable methods) is `complete`; the other nine
are unbuilt.

**Branch:** `claude/sprint-17-kickoff-d65ddf`, three commits ahead of where it
forked. Working tree clean. Not pushed, not merged.

```
5a665b2d Regenerate item-commits.json after BL-587
0e1fb3fd BL-587: the roster's first two genuine interchangeable methods
401eae79 Author Sprint 17: the ancient roster becomes a ladder (BL-585..BL-594)
```

**The gate that blocks everything else.** Ben ruled Sprint 17 starts only
after **Sprint 16** (the mercenary vertical slice, v0.1.15) merges to `main`.
As of this session's last check, `origin/main` was at `6030c2b3` and every one
of Sprint 16's nine items still read `status: designed` — **not merged**.
`BL-587` was the one exception: its files (`scripts/recipes.lua`,
`tools/verify/chain_depth.cpp`, `docs/economy/PRODUCTION.md`) appear nowhere
in Sprint 16's file set, checked with:

```
node tools/session/backlog_query.js --sprint 16 --table
```

then diffing each candidate item's `files` against that set. **Re-run that
check before touching anything else** — if Sprint 16 has since merged, the
gate is open and `BL-585` (wave 1) is next.

## What BL-587 actually built, and why it matters to the rest

`chain_depth.cpp`'s R2 row — the live no-dominance guard — reported **zero**
genuine interchangeable production methods before this session, despite the
2026-08-15 "alternates with real trade-offs" ruling. Every same-output sibling
pair in the shipped roster was either a disjoint-raw supply route or a named
precondition pair; the guard's own retraction note (in
`recipe_switch_harness.cpp`) documents why the old grouping mis-reported four
pairs as "dominated" (NR-243) when they were routes, not methods.

BL-587 authored the first two real ones: `charcoal_from_kiln` "Coking Kiln"
(ancient, shares `timber` with the Charcoal Burner) and `steel_bessemer`
"Bessemer Converter" (industrial, shares `iron_ore`+`coal` with the Smelter).
Both trade the chain-depth axis — cheaper by the guard's reference snapshot,
but gated behind a reagent (`iron_blooms` / `machinery`) that requires the
shallow route's own chain to have already run once. `chain_depth`,
`recipe_switch_harness` and `price_band_harness` all rerun clean; see the
`interchangeable-methods-exist` requirement group
(`docs/development/req/requirements.json`) for the exact numbers.

**Why this matters to `BL-589` (the start-gate audit) and `BL-592` (the
breadth guard):** the guard's fixed reference-price snapshot cannot see a
method that's only better "depending on which market it builds to" (Ben's
own framing, 2026-08-23) — it will misreport such a method as dominated. R2
needs a second price vector (fuel-cheap / fuel-dear) before `BL-586`'s wider
roster lands more pairs like these two. That work is scoped into `BL-592`;
don't let it slip to "later" once the roster is wide and the guard starts
crying wolf.

## Corrections made this session, not just additions

- **`docs/economy/PRODUCTION.md` § Alternate production methods** no longer
  reports the four NR-243 pairs as Ben's open call — that text was stale
  (the guard that found them was retracted 2026-08-16). Corrected in place,
  and the doc now carries a table of the two real methods. If you see NR-243
  cited anywhere else as still-open, it's the same staleness — fix it too.
- **`docs/development/ROADMAP.md`'s v0.1.17 bullet** was rewritten: it named
  the *original* six-item carry set (`BL-428`/`429`/`430`/`431`/`432`/`433`),
  three of which had already landed and three of which were absorbed by other
  work. It now names the real ten-item set and the 2026-08-23 re-plan.
- **`NR-589`** (open, `docs/development/NEEDS_REVIEW.json`) records that a
  ruling — "delete the dominated sibling" — was taken on that same stale
  paragraph, and was not acted on: the four pairs it names are not dominated
  under the live guard, and deleting them would have stranded three raws as
  orphans. `BL-587` is the correction; re-rule the deletion only if a specific
  pair is still wanted gone for a reason the guard doesn't model. **This is
  the one item in this handoff that wants Ben's eyes**, not just a resume
  point — everything else here is safe to act on directly.

## Where to pick up

1. Re-run the Sprint 16 merge check above.
2. If merged: promote `BL-585` (ancient goods append + save bump) into
   `REFINED.md` wave 1, following the same item-spanning-requirement pattern
   `BL-587`'s entry there already models. It's the pacing item — everything
   from `BL-586` on is Lua-only once it lands.
3. If not merged: nothing else in Sprint 17 is buildable standalone. Check on
   Sprint 16 instead, or hand this branch to whoever owns that merge.

See also: `sprints.json`'s `"17"` entry (full rulings + notes),
`docs/development/req/requirements.json`'s `interchangeable-methods-exist`
group, and `NR-589`/`NR-590`/`NR-591`.
