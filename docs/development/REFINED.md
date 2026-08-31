# Project Io — REFINED (active worklist)

**Sprint 27 (demand, resumed) — BATCH DELIVERY, opened 2026-08-31.**

Give the goods a buyer. On the industrial band `iron_ore` is produced 42991.6 against total demand
**0.000**; five of the eight designed channels do not exist; every AI corp is insolvent 29–30 ticks
of 30 on every seed. Barrier semantics per `DELIVERY.md` § Batch Delivery.

**Success is not "the channels are built"** — it is `ai_skill_harness` showing a field that is not
monotonically insolvent. Channels built and corps still broke is exactly what BL-641 produced alone.

**Base:** `28443671`.

---

## Collision map

| Slice | Items | Writes | Overlaps |
|---|---|---|---|
| **A — the bid seam** | BL-654, BL-652 | `src/world/market_clearing.cpp`, `src/world/economy_system.cpp`, `scripts/economy.lua`, `docs/economy/MARKETS.md` | B on `economy.lua` |
| **B — ancient chain** | BL-707 | diagnostic; reads `corp_ai.cpp`, `placement_rules.cpp`, `scripts/recipes.lua` | A on `economy.lua` |
| **C — completeness read** | BL-706 | `tools/verify/demand_census.cpp` | none |
| **D — power** | BL-708 | `components.hpp`, `economy_system.cpp`, `logistics.cpp`, `scripts/*.lua` | A, E |
| **E — construction** | BL-709 | `economy_system.cpp`, `corp_ai.cpp`, `placement_rules.cpp`, build door | A, D |

**Symbol layer** — the review-barrier checklist:

| Task | provides | consumes |
|---|---|---|
| A1 (BL-654) | the short-pool **bid path**; the reservation ceiling in the price-band family | — |
| A2 (BL-652) | unpriced-basket-entry diagnostic + a failing harness row | — |
| B1 (BL-707) | a written, evidenced diagnosis | — |
| C1 (BL-706) | per-market completeness column + spread summary in `demand_census` | — |
| D1 (BL-708) | generation building; power as a bought upkeep draw; network transmission | A1's bid path |
| E1 (BL-709) | construction sector, five banded methods; scorer prices contended capacity | D1 |

No unmatched `consumes`.

---

## Wave 1 — the gate and the instruments (parallel, worktree agents)

- [ ] **A1 · BL-654 (a channel must bid)** — **gates the whole sprint.** A short draw bids the
      shortfall and pays; above a reservation ceiling it does not bid and the shortfall rule weakens
      the building instead. One rule for every goods draw, unit upkeep included.
- [ ] **A2 · BL-652 (injectors must not skip silently)** — an injector that cannot price a basket
      entry says so. Two bugs hid behind this silence on one day.
- [ ] **B1 · BL-707 (the ancient chain does not convert)** — **diagnose before fixing.** Do not move
      the price ceiling to hide the reading.
- [ ] **C1 · BL-706 (chain completeness read)** — reports, never gates. The enemy is uniformity.

## Wave 2 — power (after A1 merges)

- [ ] **D1 · BL-708 (power as a grid)** — bought, sold between corps, traded across borders,
      transmitted on the road network at flat one-tick latency. Shortfall **scales output down**,
      never idles.

## Wave 3 — construction (after D1, with a census run between)

- [ ] **E1 · BL-709 (construction as a rate)** — five banded methods; seeded capacity gives the
      channel a non-zero reading at tick 0. NR-592 fixed here.

## Wave 4 — feed the findings back up (main session)

- [ ] **Reconcile into the authority docs** (Ben, 2026-08-31: *"feed our findings back up to
      authority docs"*). Every measurement this batch produces goes into the doc that owns its
      subject — `MARKETS.md`, `PRODUCTION.md`, `LOGISTICS.md`, `RESOURCES.md`,
      `GENERATION_STRATEGY.md` — not left in a review-queue entry or a commit message.

---

**Open work with no promoted tasks:** `node tools/session/backlog_query.js --table`.
