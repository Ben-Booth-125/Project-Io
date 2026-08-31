# Project Io — REFINED (active worklist)

**Sprint 26 (watch the AI play) — BATCH DELIVERY, opened 2026-08-31.**

Goal, Ben's words: *"ensure spectator mode works, and visibly watch AI play in order to further
develop the meta."* Nine items, three waves. Barrier semantics per `DELIVERY.md` § Batch Delivery —
every item clears step *N* before any starts *N+1*, with one `verifier-review` pass over the whole
integrated set at step 4a.

**Base:** `ef19dab7`. Note the batch opened by fixing the build — `main` had not configured since
`cc88997c` left three CMakeLists references to files the mercenary-contract tear-out deleted.

---

## Collision map

**File layer** (a splitting heuristic; worktrees absorb overlap):

| Slice | Items | Writes | Overlaps |
|---|---|---|---|
| **A — Spectate** | BL-695, BL-702 | `src/main.cpp`, `src/core/app.cpp`, `src/core/verify_api.cpp`, `src/ui/ui_state.hpp`, `src/ui/time_panel.cpp`, `docs/ai/ACTIONS.json` | B on `main.cpp`; C/E/F on `verify_api.cpp` |
| **B — Epoch** | BL-705 | `src/world/hard_coded_world.hpp`, `src/world/planetology.hpp`, `src/ui/format.{hpp,cpp}`, `src/ui/tile_inspector.cpp`, `src/main.cpp` | A on `main.cpp` |
| **C — corp_ai reads** | BL-700, BL-696 | `src/world/corp_ai.{hpp,cpp}`, `src/ui/decision_feed.cpp` | E, F |
| **D — Harness** | BL-697 | `tools/verify/ai_skill_harness.cpp` | none |
| **E — Coalitions** | BL-699 | `src/world/corp_ai.{hpp,cpp}` | C, F |
| **F — Trace** | BL-704 | `src/world/corp_ai.{hpp,cpp}`, `src/core/verify_api.cpp` | C, E, A |
| **G — Watch** | BL-703 | `docs/ai/STRATEGIES.md` | none |

**Symbol layer** — the review-barrier checklist:

| Task | provides | consumes |
|---|---|---|
| C1 (BL-700) | `corp_standing_index(w, reg, corp, params)` -> `standing_index`; `corp_ai_params::standing_weights` | — |
| C2 (BL-696) | decision-feed reason/score stream reaching `decision_feed.cpp` | — |
| D1 (BL-697) | spread band in `ai_skill_harness` | `corp_standing_index` |
| E1 (BL-699) | stance scoring over the four BL-448 verbs | `corp_standing_index`, decision-feed reason stream |
| F1 (BL-704) | per-corp decision trace | decision-feed reason stream |
| A1 (BL-695) | a route setting `ui_state::spectating` outside `--verify` | — |
| A2 (BL-702) | a control setting `ui_state::god_view` | A1's spectate route |
| B1 (BL-705) | selectable `world_params::epoch_year`; epoch read from params | — |
| G1 (BL-703) | the written finding | everything above |

No unmatched `consumes`.

---

## Wave 1 — the instruments — LANDED 2026-08-31

- [x] **A1 · BL-695 (live spectate route)** — a route sets `ui_state::spectating` outside
      `--verify`. Today `verify.spectate(on)` at `verify_api.cpp:1096` is the only assignment site
      in the tree. Prefer entry-at-start; mid-session toggling changes who the scorer may act on
      halfway through a run and needs its own argument. `spectator_determinism` must pass unchanged.
- [x] **A2 · BL-702 (spectate god view control)** — a control sets `ui_state::god_view`, offered
      only while spectating, honouring the toggle rule. Every read site already tests the pair, so
      do not touch the gate.
- [x] **B1 · BL-705 (selectable 1960s start)** — `epoch_year` selectable without editing a default,
      and the UI epoch reads `world_params` rather than the hard-coded 1960 in `format.hpp:82` /
      `planetology.hpp:275`. Both starts stay supported. Expect to find stale things on the 1960
      path; file them, do not absorb them.
- [x] **C1 · BL-700 (composite standing index)** — one function: net worth + `science` + summed
      `unit_strength`. Weights in `corp_ai_params`. Deterministic read point, named and asserted.
- [x] **C2 · BL-696 (decision feed reasons)** — fix the feed at its cause. NR-626: every row reads
      `overridden` at 0.00.

## Wave 2 — measurement and the brake (parallel, after wave 1 merges)

- [ ] **D1 · BL-697 (skill harness margin metric)** — band the spread in composite standing;
      absolute bands demote to a solvency floor; re-derive from a fresh bless with dated provenance.
- [ ] **E1 · BL-699 (rival coalitions)** — score the four stance verbs against standing. Legibility
      is a requirement, not a follow-up. Never reads `player_entity`. Never vetoes construction.

## Wave 3 — the output

- [ ] **F1 · BL-704 (rival trace export)** — optional. Check the existing world history log first.
- [ ] **G1 · BL-703 (watch session finding)** — main session. A run watched end to end on the 1960s
      start, written into `STRATEGIES.md`. **This is the item that stops the sprint ending with
      working plumbing and nothing learned.**

---

**Open work with no promoted tasks:** `node tools/session/backlog_query.js --table`.
