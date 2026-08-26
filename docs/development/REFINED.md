# Project Io — REFINED (active worklist)

**Sprint 20 — the books open, and the start earns its way. Wave 1 (batch `sprint-20-wave-1`).**

Save-format versions are **reserved by hand for this wave** (current constant: 15) because BL-637
does not exist yet: **BL-626 claims 16, BL-631 claims 17**. Both agents assert round-trip against
the `world_save_version` constant **symbolically**, never a literal, so whichever merges second
stacks without breaking the other's assertions.

---

## T1 — BL-637 (save-version reservation) · agent: tooling · worktree

Build the claim tool so wave 2 never reserves by hand again.

- `provides:` `tools/session/next_save_version.js` (or equivalent), a recorded-claim store
- `consumes:` — (nothing; disjoint from all other wave-1 files)
- Files: `tools/session/`, `docs/development/DEVELOPMENT_PRACTICES.md`
- Requirements: `save-version-reservation` R1–R3
- Note: **must not touch `src/`**. It scans it; it does not edit it.

## T2 — BL-631 (ownership class) · agent: generation-dev · worktree

Pass 2b: derive `ownership_class` from the region's industrialisation timing.

- `provides:` `ownership_class` enum, `corporation_component::ownership_class`, its save
  read/write, `world_save_version` 17
- `consumes:` — (the BL-218 industrialisation-timing scalar is already landed)
- Files: `src/world/corporation_generation.cpp`, `src/world/components.hpp`,
  `src/world/world_save.cpp`, `src/world/world_save.hpp`, `tools/verify/`
- Requirements: `ownership-class` R1–R6
- Authority: `docs/generation/CORPORATION_GENERATION.md` § Pass 2b

## T3 — BL-626 (quarterly return) · agent: economy-dev · worktree

The record itself — a retain over `corp_budget` at the end of `apply_budget`.

- `provides:` `quarterly_return` struct, its per-corp store, the append in `apply_budget`, its
  save read/write, `world_save_version` 16
- `consumes:` — (nothing; `corp_budget` and the registry `build_cost` are already landed)
- Files: `src/world/budget_system.hpp`, `src/world/budget_system.cpp`,
  `src/world/components.hpp`, `src/world/world_save.cpp`, `src/world/world_save.hpp`,
  `tools/verify/`
- Requirements: `quarterly-return` R1–R6
- Authority: `docs/economy/FINANCE.md` § The quarterly return

---

### Collision map (file layer — the splitting heuristic)

`components.hpp`, `world_save.cpp` and `world_save.hpp` are written by **both** T2 and T3. That
is why they are in separate worktrees and why the versions are pre-reserved; the main session
merges and stacks. T1 is disjoint from both.

### Symbol contract

No `consumes` entry in this wave names another wave-1 task — every dependency is already landed.
Wave 1 is a genuine three-way fan-out with a merge barrier at the end.

---

## Carried, not promoted

- **BL-619 (research system)** — gated: a design session with Ben over the RESEARCH.md stub.
- **BL-636 (live-click debt)** — the dispatch form and Throughput lens, three sprints running;
  blocked on NR-622's environment problem, not on design.
- **BL-632 (warm-start progress)** — design-owed; what the loading screen's second phase shows
  is Ben's call.
- **BL-622 (density retunes)**, **BL-595 (nation starting treasury)** — outside Sprint 20.
- The **v0.1.18 and v0.1.19 tags** — a release is Ben's to call.
