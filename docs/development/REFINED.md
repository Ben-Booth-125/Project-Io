# Project Io — REFINED (active worklist)

**Sprint 27 (demand) — BATCH DELIVERY, block 2, 2026-09-01.**

Give the goods a buyer. Block 1 (BL-654, BL-652, BL-706, BL-707, BL-708, BL-709) landed on
2026-08-31; this block is the prerequisites NEXT_SESSION.md ordered ahead of the channels, plus
the channels themselves. Barrier semantics per `DELIVERY.md` § Batch Delivery.

**Success is not "the channels are built"** — it is `ai_skill_harness` showing a field that is not
monotonically insolvent. **Read NR-771 before using that criterion**: the harness's hand-built
registry holds three recipes in one default group, so it is blind to any per-category change.

**Base:** `ece82abc`.

---

## Done this block

- [x] **BL-710 (save roundtrip does not compile)** — six regions deleted, not repaired. Green at
      v22, 63 PASS / 0 FAIL; v21 and v22 both round-trip. `e90841a0`
- [x] **BL-712 (recipe choice is scale-blind)** — per-`recipe::group` build candidates, and the
      recipe margin-chase confined to its own group (the seam has refused cross-group since
      2026-08-16). corp_ai_harness R8, falsified before landing. `7983d5d4`
- [x] **BL-709 verified** — R2/R3/R4 complete, **R1 pending**; the item stays open, deliberately.
      `1c45f41f`
- [x] **demand_census surveys the world** — it called no `init_survey_states`, so the corp AI built
      *zero* extraction sites in every census ever run. `8fe177dc`
- [x] **BL-711 (extraction candidate list is scale-blind)** — per-resource top-K. Coal goes from
      0 mines in any world to 25. `1eed92e3`

---

## Next, and what each is waiting on

| # | Item | State |
|---|---|---|
| 1 | **BL-642 (construction draws)** | **Blocked on NR-773.** Half (1)'s premise re-measured and largely overtaken by BL-711 + NR-772; half (2) needs one call — does centre growth *gate* on materials or only register a want. |
| 2 | **BL-644 (state channel)** | Open. Nation budget lines spend credits; no goods purchase exists. |
| 3 | **BL-647 (endemic luxury)** | Open. Note the prerequisite: tobacco/spices/coffee/furs are **not in `k_extractable`**, so nothing can mine them (BL-586 slice 2's own recorded gap). |
| 4 | **BL-643 / BL-646 / BL-645** | Priority B — infrastructure, conflict, research channels. |
| 5 | **BL-709 R1** | Re-open when NR-770 is diagnosed: yards are removed on the industrial band and produce 0.0 on the ancient one. |

## Ben's queue, newest first

`NR-773` (BL-642's fork) · `NR-772` (the census surveyed nothing — decision taken, reversible in one
line) · `NR-771` (ai_skill_harness is blind to per-category change) · `NR-770` (construction yards
do not survive, and where they survive produce nothing) · `NR-769` (**the scale-blind exclusion has
a second seat inside `net²/capex` — BL-417 is yours**) · `NR-768` · `NR-767` (Lua-linked harnesses
are a growing class with only a hand-written list).

## Known-red, reported and NOT re-blessed

- `spectator_determinism` R2 byte-identity — `golden=E350DF2A50BF4BAA observed=90BFB27CB57CC308`.
  Already stale by 150+ commits before this block (NR-752); BL-711 moved it further. **Yours.**
- `ai_skill_harness` bands — 25 failures, unchanged in count since 2026-08-16 (NR-305). BL-711 moved
  the numbers; BL-712 did not move them at all, which is NR-771.
- `chain_depth` — 8 pre-existing `injector::none` rows.

---

**Open work with no promoted tasks:** `node tools/session/backlog_query.js --table`.
