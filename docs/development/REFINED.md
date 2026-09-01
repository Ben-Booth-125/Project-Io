# Project Io — REFINED (active worklist)

**Sprint 27 (demand) — BATCH DELIVERY, block 2, 2026-09-01.**

Give the goods a buyer. **Success is not "the channels are built"** — it is a field that is not
monotonically insolvent. Read NR-771's finding first: `ai_skill_harness`, the sprint's stated
criterion, is blind to per-category work until BL-713 lands, so read the criterion as
`chain_conversion_probe` + `demand_census` in the meantime.

**Base:** `ece82abc`.

---

## Done this block

- [x] **BL-710** save_roundtrip compiles again — green at v22, both dead bumps verified. `e90841a0`
- [x] **BL-712** per-`recipe::group` build candidates; the recipe chase confined to its own group. `7983d5d4`
- [x] **BL-709 verified** — R2/R3/R4 land, **R1 pending**; the item stays open. `1c45f41f`
- [x] **demand_census surveys the world** — it built *zero* extraction sites before. `8fe177dc`
- [x] **BL-711** per-resource top-K — coal goes from 0 mines in any world to 25. `1eed92e3`
- [x] **BL-417** the build score is `net / capex`; the quadratic is gone. `spectator_determinism`'s
      byte-identity row retired with it, and that harness is green for the first time in weeks.

## The review queue is drained

**117 open → 0**, on Ben's instruction, and the reason matters more than the number: most entries
named *work*, not a judgement. Ten items now carry it — **BL-713** … **BL-722**. Rule 0c in
`CLAUDE.md` gained the discipline that keeps it drained: a call only Ben can make goes in the queue,
work goes in the backlog, a fact worth remembering goes in the comment next to the code.

## Next

| # | Item | State |
|---|---|---|
| 1 | **BL-642** (construction draws) | Ruled: centre growth is **gated** — it stretches like a build. Design for the BL-641 cliff rather than discovering it; growth is episodic and `max_stretch` already models "slower, not dead". Also owns NR-770's yards. |
| 2 | **BL-644** (state channel) | Open. |
| 3 | **BL-647** (endemic luxury) | Open. **Prerequisite the item does not name:** tobacco, spices, coffee and furs are not in `k_extractable`, so nothing can mine them. |
| 4 | **BL-643 / BL-646 / BL-645** | Priority B. |
| 5 | **BL-709 R1** | Re-open with BL-642 — same fact from the other side. |
| 6 | **BL-713** (harnesses build the app's world) | **After sprint 27 closes**, per Ben. One golden wave. |

Then, unsequenced: BL-714 (instruments that cannot see), BL-715 (the save seam), BL-718/BL-719 (UI),
BL-720 (seeder), BL-721 (paid for outcomes), BL-716 (tech tree), BL-717 (designed but silent),
BL-722 (live-click debt).

## Known-red

- `ai_skill_harness` — 25 band failures, stale since 2026-08-16 (NR-305). BL-417 improved three of
  five seeds and the aggregate; the bands themselves have not been re-blessed.
- `chain_depth` — 8 pre-existing `injector::none` rows.
- `spectator_determinism` — **now green.**

---

**Open work with no promoted tasks:** `node tools/session/backlog_query.js --table`.
