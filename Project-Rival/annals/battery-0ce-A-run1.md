# Battery A, run 1 — 0 CE building instructions over the live MCP seam

**Date:** 2026-08-06. **Method:** blind resolver (cloud model, corpus-generation role per
AI_OPPONENT.md § 10 — cloud generates, never plays) holding `ACTIONS_INDEX.json` + the
stance taxonomy only; live seam via `ProjectIo --serve` behind `tools/mcp/server.js`
(lookup_action + get_blackboard, player corp 30318 "Genom Systems"). The resolver never
saw PHRASINGS.json or the expected column.

## Score: 10/10 on action resolution

Every row resolved to the expected action id (A3's candidate set in the expected order:
set_recipe → throttle → idle). Register drift — the divine idiom — cost nothing on the
action axis. Binds were grounded against the real blackboard: the resolver named entity
30310 (the sole processor) for A3/A6/A8/A9 and market 30288 prices for targets.

| Row | Expected | Resolved | Verdict |
|---|---|---|---|
| A1 | build (extraction) | build (extraction, ore by scarcity) | pass |
| A2 | metered sell order | metered sell order, "fleet has no press" flagged | pass |
| A3 | set_recipe → idle set | set_recipe → set_workforce → idle | pass (richer set) |
| A4 | survey (enabling) | survey, future-anchored | pass |
| A5 | place_road | place_road, tier 2, per-tile series | pass |
| A6 | build via 'another here' | build via 'another here', tile 18500 | pass |
| A7 | demolish past idle | demolish, "never shall" kills option value | pass |
| A8 | idle | idle (30310) | pass |
| A9 | resume | resume, noted rejected_state absent A8 | pass |
| A10 | metered sell order | metered sell order + gap finding | pass |

## Instructive divergences (stance axis, not action axis)

- **A5** labelled *composite* (per-tile press series) where the battery expected
  *corrective* (haul cost as the named problem). Both are true — a sentence can carry
  two stances, and the taxonomy should say so rather than force one label.
- **A7** labelled *outcome* where the battery expected *corrective*. Same lesson: the
  problem-statement and the named-result readings coexist. Stance is a set, not a scalar.

## Findings for Io

1. **The order-book gap bites in practice (BL-293, order book unreachable by command).**
   A2 and A10 — two of ten era-basic instructions — resolve cleanly to
   `place_sell_order`, an entry with no `corp_verb`: the resolver can *name* the press
   but no agent can *issue* it over the seam. The battery turns BL-293 from a noted
   asymmetry into a measured 20% of a basic instruction set.
2. **Corp discovery gap has closed upstream:** `--serve` now answers a `CORPS` opcode
   (one JSON line per corp, `is_player` flagged) — the IO-EARTHLIKE-TESTS gap list
   (item 3) is stale on this point.
3. **A resolver honesty pattern worth keeping:** the model flagged unsatisfiable binds
   (A7: no building in the world matches "never shall profit"; A9: nothing is idled
   absent A8) instead of forcing them. The battery should keep rewarding this — it is
   exactly the refusal behaviour the compressed local model must learn.

## Feedback into PHRASINGS.json

Stance becomes multi-valued (divergence above). No new readings owed — no action-axis
misses this run. Next hardening: adversarial rows where near-synonym sentences resolve
to *different* actions (idle vs set_workforce 0; demolish vs idle), where run 1 suggests
the model leans correctly but was never forced to choose under ambiguity.
