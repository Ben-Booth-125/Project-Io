# Project Io — REFINED (active worklist)

**Sprint 20 — the books open, and the start earns its way. Wave 2 (batch `sprint-20-wave-2`).**

Wave 1 landed 2026-08-26: BL-626 (quarterly return), BL-631 (ownership class), BL-637
(save-version reservation). Integrating build BUILD_OK, all harnesses green, save format at **17**.

**Save versions are now claimed through the tool, not by hand** — that is what wave 1 built it
for. If a wave-2 slice needs a bump: `node tools/session/next_save_version.js --claim "<BL-id
short handle>"`, commit the ledger line, and assert the constant **symbolically**. None of the
four tasks below is expected to need one.

---

## T4 — BL-638 (charter ownership class) · agent: generation-dev · worktree

Re-point the derivation from Stage 4 to Stage 1. **This is the wave's gating task** — until it
lands, the default world classes every corporation `closed` and nothing downstream has anything
to read or buy.

- `provides:` the re-pointed derivation reading `history_ladder::charter_cradle`
- `consumes:` `corporation_component::ownership_class` (landed, BL-631)
- Files: `src/world/corporation_generation.cpp`, `tools/verify/ownership_class.cpp`
- Requirements: `charter-ownership-class` R1–R4
- Authority: `docs/generation/CORPORATION_GENERATION.md` § Pass 2b

## T5 — BL-628 (whole-firm buyout) · agent: economy-dev · worktree

The verb, the price, and the dissolution walk.

- `provides:` `buy_corporation` corp_verb, `k_acquisition_multiple`, the dissolution path
- `consumes:` `corporation_component::returns` + `book_value` (landed, BL-626);
  `ownership_class` (landed, BL-631)
- Files: `src/world/corp_command.{hpp,cpp}`, `src/world/components.hpp`, `scripts/economy.lua`,
  `docs/ai/ACTIONS.json`, `tools/verify/whole_firm_buyout.cpp`
- Requirements: `whole-firm-buyout` R1–R6
- Authority: `docs/economy/FINANCE.md` § Whole-firm acquisition

## T6 — BL-635 (spawn solvency) · agent: economy-dev · worktree

Diagnose the measured Cr −1 / net −627 opening, then fix it at its cause.

- `provides:` the per-flow diagnosis, and whatever constant or generation term the diagnosis
  indicts
- `consumes:` `corporation_component::returns` (landed, BL-626) — the diagnosis reads filed returns
- Files: `scripts/economy.lua`, `src/world/economy_system.cpp`, `src/world/budget_system.cpp`,
  and **possibly** `src/world/corporation_generation.cpp` — see the collision note
- Requirements: `spawn-solvency` R1–R4
- Authority: `docs/economy/FINANCE.md`

## T7 — BL-633 (retire standing bands) · agent: ui-dev · worktree

Exact where a firm files, a dash where it does not. The doc work is already done.

- `provides:` band-free `corp_standing`, the disclosure-gated Corporations panel
- `consumes:` `ownership_class` (landed, BL-631)
- Files: `src/world/standing.{hpp,cpp}`, `src/ui/corporation_panel.{hpp,cpp}`,
  `tools/verify/standing_harness.cpp`
- Requirements: `retire-standing-bands` R1–R4
- Authority: `docs/politics/RELATIONS.md` § Standing · `docs/ui/DISCOVERY.md`
- **R2 needs a LIVE CLICK**, not just a capture. It is a panel with presses on it, and BL-449
  shipped clean on a compile and a 36/36 harness while being unusable.

---

### Collision map (file layer)

Mostly disjoint, with one real overlap: **T6 may need `corporation_generation.cpp`, which T4
owns.** If BL-635's diagnosis indicts opening capital or the holding count, its fix lands in T4's
file. T6 is briefed to make the minimal edit and say so; the main session resolves at merge. Every
other pair is disjoint.

### Symbol contract

Every `consumes` entry names a symbol that **already landed in wave 1** — no wave-2 task depends
on another wave-2 task's output. The wave is a four-way fan-out with a merge barrier at the end,
and T4's gating role is about *world content* being non-degenerate, not about symbols.

---

## Wave 3 (not promoted yet)

- **BL-630 (spawn shortlist)** — deliberately held back. Its viability floor reads what BL-635
  decides "solvent" means, and a shortlist over a field of insolvent corps is meaningless. It also
  carries the sprint's golden re-bless, which should happen once, late, in one wave.
- **BL-634 (acquisition viability)** — the sprint's definition of done. Needs T5 and T6 landed.
- **BL-627 (profitability ledger)** — design-owed on its `question_log.json` pair, which is Ben's
  wording.
- **BL-629 (rival acquisition)** — needs T5.

## Carried, not promoted

- **BL-619 (research system)** — gated: a design session with Ben.
- **BL-636 (live-click debt)** — blocked on NR-622's environment problem, not on design.
- **BL-632 (warm-start progress)** — design-owed.
- **NR-655's fork** — whether a return gains an eighth field so subsidies and contract payouts are
  visible to it. Ben's call, and BL-628's price depends on the answer.
- The **v0.1.18 / v0.1.19 tags** — a release is Ben's to call.
