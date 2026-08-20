# src/ui — scoped instructions

Loaded automatically when working in this directory. The presentation layer: **ImGui,
immediate-mode** — do not introduce a retained-mode framework. `CLAUDE.md` at the repo root
holds the full doc map; this file holds only what every `ui/*` session needs.

## Invariants and standing duties

- **Toggle rule.** Any control whose active state is visible is a toggle: clicking it while
  active undoes it; re-clicking the active sub-view tab closes the ledger. Exempt:
  cross-cutting selectors (body/market/resource combos) and the Selection element.
- **New or materially changed surface ⇒ `docs/ui/question_log.json`** gains/updates its entry
  (the question answered, why it earns space, the demanding item). Regenerate the mirror with
  `node tools/session/render_question_log.js`; never hand-edit `QUESTION_LOG.md`. Enforcement
  is authorship — do not build a verify check against this file.
- **Any changed control, binding, lens, ledger or panel ⇒ `docs/ai/ACTIONS.json`** is updated
  in the same change (`node tools/session/render_actions.js` regenerates the mirror and
  index). A stale entry misleads the AI player like a stale golden misleads a visual check.
- **Glyphs** live in `src/ui/icons.{hpp,cpp}` under the `(dl, centre, r, colour)` contract,
  catalogued in `docs/ui/ICONS.md`; identity colours live in `presentation.hpp`, not there.
- On-screen wording uses `docs/GLOSSARY.md` terms exactly.
- UI reads world state; it never mutates the simulation except through command verbs.

## Doc ownership (read the one your change touches)

| Subject | Authority |
|---|---|
| Canvas internals, zoom ladder | `docs/ui/CANVASES.md` → `SOLAR/CIRCUMPLANETARY/PLANETARY/MINIMAP.md` |
| Selection states, click model | `docs/ui/SELECTION.md` |
| Shell regions, ledger layout | `docs/ui/LAYOUT.md` |
| Entry screens, wizard | `docs/ui/STARTUP.md` |
| Overlay modes / lens bar | `docs/ui/LENSES.md` |
| Glyph vocabulary | `docs/ui/ICONS.md` |
| Fog / survey / intel rendering | `docs/ui/DISCOVERY.md` |

## Verification

Visual work is verified by the **headless capture harness** — `ProjectIo --verify
scripts/verify/<name>.lua`, PNG inspection via the `verifier-visual` skill. Golden captures
are contracts: report diffs and their cause; re-bless only with authorisation. When asking
Ben to weigh in on visuals, report exact current measurements first (Rule 0b) and open the
live app.
