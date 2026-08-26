# Project Io — REFINED (active worklist)

**Two sprints open.** Sprint 20's waves 1 and 2 have landed; Sprint 21's wave 0 is in flight.

---

## Sprint 21 — demand. Wave 0 (batch `sprint-21-wave-0`)

The guard and the instrument, deliberately **before** any channel is built: without them a
viability pass is a guess with a number attached.

### T8 — BL-648 (admission rule names an injector) · agent: economy-dev · worktree
- `provides:` a registry of real demand injectors; `chain_depth` R1 resolving exemptions against it
- `consumes:` — nothing
- Files: `tools/verify/chain_depth.cpp`
- Requirements: `admission-rule-injector` R1–R4
- **Expected RED on ten goods when it lands.** That is the item working. Do not weaken it while the
  channels are being built.

### T9 — BL-649 (demand census) · agent: economy-dev · worktree
- `provides:` `tools/verify/demand_census.cpp` — per resource, per band, who injects demand
- `consumes:` — nothing
- Files: `tools/verify/`, its skill entry (owed, needs Ben)
- Requirements: `demand-census` R1–R4
- Reports; never asserts a magnitude. Run before and after every pass, deltas kept.

### Wave 1 (not promoted)
BL-640 (era-banded household basket) · BL-641 (building upkeep in goods) · BL-642 (construction
actually draws). **Wave 2:** BL-644 (space programme line) · BL-647 (endemic luxury) · BL-643
(network upkeep) · BL-646 (battles burn ordnance). **Gated:** BL-645 (research consumes goods),
behind BL-619's design session.

---

## Sprint 20 — landed, and what it still owes

**Wave 1** (2026-08-26): BL-626 (quarterly return), BL-631 (ownership class), BL-637 (save-version
reservation). **Wave 2** (2026-08-26): BL-638 (charter ownership), BL-628 (whole-firm buyout),
BL-633 (retire standing bands), BL-635 (spawn solvency). Integrating build BUILD_OK both times;
save format at **17**; `spectator_determinism` re-blessed once to `E350DF2A50BF4BAA` with provenance.

### Two rows are deliberately not green, and both are results

- **`spawn-solvency` R2 — FAILED, left red.** Net ≥ 0 moved 0/8 → 1/8 seeds. The money loop is
  fixed (operating net −646 → −28 cr/qtr); the residual is 60 % maintenance on buildings whose
  output has no buyer, which is Sprint 21's territory and not a constant anyone can tune (NR-671).
- **`retire-standing-bands` R2 — PENDING.** No live click was reachable. Joins BL-636.

### Wave 3 (not promoted — deliberately behind Sprint 21)

- **BL-630 (spawn shortlist)** — its viability floor now has a real mechanism to gate on, but only
  once demand exists.
- **BL-634 (acquisition viability)** — the definition of done for *both* sprints. Cannot honestly
  pass until Sprint 21 lands: a corp at −28/qtr never saves up for anything.
- **BL-627 (profitability ledger)** — design-owed on its `question_log.json` pair, Ben's wording.
- **BL-629 (rival acquisition)** — ready; BL-628 landed.
- **BL-650 / BL-651** — the dissolution walk's last two loose ends, and `charter_reach` being
  silently zeroed on load.

---

## Carried, not promoted

- **BL-619 (research system)** — gated: a design session with Ben. BL-645 waits on it.
- **BL-636 (live-click debt)** — dispatch form, Throughput lens, and now BL-633's panel; blocked on
  NR-622's environment problem, not on design.
- **BL-632 (warm-start progress)**, **BL-639 (corp panel column budget)**, **BL-622**, **BL-595**.
- **NR-655 / NR-668** — whether a return gains an eighth field so subsidies and contract payouts are
  visible to it. Measured at **640 credits** of undervaluation per acquisition. Ben's call.
- The **v0.1.18 / v0.1.19 / v0.1.20** tags — a release is Ben's to call.
