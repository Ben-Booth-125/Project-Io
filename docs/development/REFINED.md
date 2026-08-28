# Project Io — REFINED (active worklist)

**Sprint 23 (UI visibility, batch 2: selection & hover) — Batch Delivery, wave 1.**
Three items, two lanes. The rule is already settled in `docs/ui/SELECTION.md`
§ A lens collapses selection to ONE TIER; these tasks build it.

Batch key: `sprint-23-wave-1`.

---

## Collision map

**File layer.** Lane A writes `src/ui/body_surface_canvas.cpp` and
`src/core/verify_api.cpp`. Lane B writes new `src/ui/company_ledger.{hpp,cpp}`
plus `src/ui/ui_state.hpp`, `src/ui/nav_pane.cpp`, `src/core/app.cpp`. Disjoint —
Lane B runs in a worktree, Lane A in the main session.

The one crossing is A2, which names Lane B's new state fields from
`verify_api.cpp`. It is a *read* of three field names, resolved at integration.

**Symbol layer.** See `provides:` / `consumes:` per task.

---

## Lane A — the click handler (main session, sequential)

### A1 · BL-664 (one tier under a lens) — the gate
*files:* `src/ui/body_surface_canvas.cpp`
*provides:* `lens_is_interactive(overlay_mode)`, the one-tier click branch, the
lens-gated hover resolution
*consumes:* —

Under an active lens the click resolves to `lens_structure_of_tile` alone.
Markers do not pre-empt it; a `none` answer selects nothing, clears the band to
resting, and suppresses the hover card. The four-rung repeat-click cycle and the
marker precedence run only under `overlay_mode::none`.

### A2 · BL-664 — the assertion surface
*files:* `src/core/verify_api.cpp`
*provides:* `pointer_target.hovered_structure_kind`, `.selected_deposit_resource`,
`.selected_plate`, `open_panel` recognising the company ledger
*consumes:* `ui_state::show_company_ledger`, `selected_company` (B1)

`pointer_target` reports `selection_kind` but nothing about the two non-entity
structure channels, so a check on the deposit or plate pivot can assert the side
effect and never the subject. This exposes **UI state**, not tile data — it does
not let a script ask which tiles carry a resource, so it does not touch the
standing rule NR-698 is about.

### A3 · BL-665 (corp and company tile groups) — the area resolver
*files:* `src/ui/body_surface_canvas.cpp`
*provides:* `lens_structure_of_tile` cases for `corporation` and `company`
*consumes:* A1 (the marker precedence must be gone first, or this can never fire)

A corporation's holdings on this body are one structure, keyed by owner. Hover
lights the whole group; unowned ground answers `none` and is inert. The
hand-wired `lens_through` marker pivot folds into this and is deleted. Company
takes the same resolver, split on `corporation_component::is_background`.

### A4 · BL-666 (owner ledger destinations) — the routing
*files:* `src/ui/body_surface_canvas.cpp`
*provides:* —
*consumes:* `ui_state::show_company_ledger`, `selected_company`,
`selected_corporation_dossier` (B1); A3

Corporation → the corporations table, aimed at that corporation's row. Company →
the placeholder company ledger, naming that firm. Same seam BL-603 and BL-659
use: set the target, `close_all_panels`, open the surface.

### A5 · the check
*files:* `scripts/verify/lens_one_tier.lua`
*provides:* the `lens_one_tier` check
*consumes:* A1, A2, A3, A4, B1

Asserted, not capture-only. Per lens: what a press on a BUILT tile resolves to,
what an inert lens does to the hover card and the band, and that a repeat press
under a lens does not advance a cycle.

---

## Lane B — the company ledger (worktree, parallel with A1/A3)

### B1 · BL-666 — the placeholder surface and its state
*files:* `src/ui/company_ledger.hpp`, `src/ui/company_ledger.cpp`,
`src/ui/ui_state.hpp`, `src/ui/nav_pane.cpp`, `src/core/app.cpp`
*provides:* `ui_state::show_company_ledger`, `ui_state::selected_company`,
`ui_state::selected_corporation_dossier`, `ui::draw_company_ledger`
*consumes:* —

A background firm has no surface of any kind. This is the stand-in Ben's
parenthesis authorises — a real target field and an honest empty state, not
content. It must yield to `close_all_panels` and answer `any_panel_open` like
every other column occupant.

---

## Closed out

All five Lane A tasks and Lane B's B1 are **complete**. `lens_one_tier` runs 38/38 green.

Two symbol notes against the map above, both harmless and both worth recording
rather than quietly correcting: A1's `provides:` named a helper
`lens_is_interactive(overlay_mode)` that was never written — the code uses an
inline `lensed` bool instead — and A2 landed one field beyond its list,
`pointer_target.hovered_structure`, which was then **removed** at the review
barrier because under the Continent lens it carried a per-tile plate id across
the Lua seam.

## Not in this batch

`BL-659`, `BL-660`, `BL-661`, `BL-662`, `BL-663` carry in sprint 23 with Ben's
decisions recorded but are not promoted here. `NR-697` (a plate has no
Selection-band content) and `NR-698` (the deposit pivot needs a tile query or a
live-click-only requirement) stay open — A2 narrows NR-698 but does not answer
the question it asks.

**Open work with no promoted tasks:** `node tools/session/backlog_query.js --table`.
