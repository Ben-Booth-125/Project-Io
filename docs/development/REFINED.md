# Project Io — REFINED (active worklist)

## The province-grain batch (promoted 2026-08-21)

Requirements: requirements.json § `province-render-and-selection`,
§ `unit-position-province-grain`, § `province-building-limit`, § `supply-interdiction`.

Follows Lane 0 (`10993ca`), which closed the determinism question: no leak, the
golden re-blessed with provenance, and the partition now compared field-for-field
across two generations. Ben's grain ruling is BL-511's design; NR-405 records what
it overturns.

**Main session owns** the board files and `question_log.json` — no agent touches them.

### Lane R — BL-511, the province as the rendered and selected unit

- **[5] R1 — blended province rendering + province selection.** Sub-agent, worktree.
  Files: `src/ui/body_surface_canvas.cpp`, `src/ui/ui_state.hpp`,
  `src/ui/selection_panel.cpp`, `docs/ui/PLANETARY.md`, `docs/ui/SELECTION.md`.
  Province becomes the click target and the drawn unit, with borders softened by a
  blend across the province's real tile mixture. The tile does **not** retire — it
  stays the data grain and appears in the Selection panel as the detail of what was
  clicked. **The blend has no precedent in this codebase**; expect a visual pass
  with Ben rather than a spec that closes it.

### Lane U — unit position at province grain (BL-511's seam half)

- **[4] U1 — `march_unit` retargets from tile to province.** Sub-agent, worktree.
  Files: `src/world/corp_command.{hpp,cpp}`, `src/world/components.hpp`,
  the unit movement step, `docs/ai/ACTIONS.json`, harness.
  The verb enum is serialised and append-only, so the VERB stays and the field it
  reads changes. Recommendation in BL-511: unit stores a tile and derives a
  province — cheaper, and every existing reader keeps working. **Untrusted input
  boundary**: a province id arriving over `--serve` is validated as the value that
  lands, whole-command rejection, rejection mutates nothing.

### Lane B — BL-513, the province building limit

- **[3] B1 — total-buildings ceiling per province.** Sub-agent, worktree.
  Files: `src/world/placement_rules.{hpp,cpp}`, `src/world/province.{hpp,cpp}`,
  `tools/verify/province_capacity_probe.cpp`, `docs/economy/PRODUCTION.md`.
  Type-agnostic by ruling, from area + infrastructure + habitability + population,
  alongside (never replacing) the per-tile deposit cap. Pin any coefficient by
  measurement against the probe's spread — never pick one. **It will refuse nothing
  today** (0.13% of capacity used); that is expected and is not a reason to inflate it.

### Lane M — military, grain-independent

- **[4] M1 — BL-458, supply lines cannot be cut.** Sub-agent, worktree.
  Files: `src/world/logistics.{hpp,cpp}`, `src/world/supply_system.cpp`,
  `src/world/components.hpp`, `src/ui/body_surface_canvas.cpp` (consume-only),
  `src/ui/market_ledger.cpp`.
  Lift `convoy_tile_at` out of the renderer into `logistics.hpp` — the canvas
  already derives the full convoy path and the shared function must own the
  orientation rule, or a convoy's head lands at the wrong end half the time and
  looks fine either way. Then interdiction: the act that finally earns BL-315's
  third reading, **pirate**, which has had no mechanic since 2026-08-07.
  Chosen for this batch because it is entirely tile-pathed and so untouched by
  the grain ruling.

### Held, with reasons

- **BL-471 (unit marker + command surface), BL-469 (battle card), BL-449 (stance
  surface).** All draw on the canvas Lane R rewrites, or collide with it on
  `selection_panel.cpp`. They follow Lane R rather than run beside it.
- **BL-467 (battle state).** Its engagement rule gets simpler once Lane U lands —
  a unit's position becomes a province, so no tile-to-province reduction is needed.
  Sequence it after U1.
- **BL-512 (firm cap tunables).** Pin together with BL-513's coefficient against one
  sweep, or the two get tuned against each other by accident.
