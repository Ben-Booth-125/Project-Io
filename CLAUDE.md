# Project Io — Claude Reference

Project Io is a near-future space-based grand strategy game: a solar system of earth-like
bodies, generated with a simulated pre-campaign history, in which corporations extract, trade
and fight. Solo-developed in C++ with Lua scripting. `docs/CONCEPT.md` owns who the player is
and what the game should feel like; `docs/SYSTEMS.md` owns the system map; `docs/GLOSSARY.md`
owns the words. Read those three before anything else on a first visit.

**The docs are the authority, and they are state-independent.** An authority doc says what is
true of the game's design — never whether a piece is built, when it landed, or which backlog
item did it. Whether a thing is built is a backlog fact: `node tools/session/backlog_query.js
--touches <doc>`. If a doc and the code disagree, one of them is wrong and the fix is work,
not a footnote. Full rule: `.claude/rules/io-standing-rules.md` § Terms & docs.

This file is a **router**. It tells you which mode the session is in, which doc owns your
question, and which tool answers it. It does not restate what the docs say.

---

## 1. Decide the session mode first

Say the mode in your first line. Everything else — what to read, whether to file, whether to
build — follows from it.

| Mode | Signal | Deliverable | Read |
|---|---|---|---|
| **Design** | Ben is thinking aloud, asks *should / why / how would*, or raises an idea with no "do it". | An assessment, or a settled design **written into the owning authority doc** plus a backlog item pointing at it. No code. Rule 0a applies: offer *A) backlog* / *B) build now* before acting on an idea. | The owning doc, `backlog_query.js --grep`, the relevant `NEEDS_REVIEW.json` entries. |
| **Delivery — Light** | A one-line fix, an obvious cleanup, a doc tweak. | Make it, check it, say what you did. No tasks, no requirements. | The file and its owning doc. |
| **Delivery — Full** | Touches the economy / save-format / integration seam, spans >~2 logic files, or carries determinism risk. | The DELIVERY.md lifecycle: requirement → tasks in `REFINED.md` → build → verify → one commit per item. | `docs/development/DELIVERY.md`, then the owning docs only. |
| **Corpus** | Sweeps, reconciliations, index regeneration, doc-vs-code truth checks. | Edited docs/stores plus a `NEEDS_REVIEW.json` entry per call taken. Fan out by file group. | `DELIVERY.md` § Batch Delivery; the stores' own `_note` blocks. |
| **Review / verify** | "Does this work", "review this", a failing harness, a visual check. | A verdict with evidence — harness output, a capture, a diff. Fix only when asked. | The `verifier-*` skills; `docs/development/REVIEW_AUTOMATION.md`. |
| **Rival** | Anything under `Project-Rival/`. | Per `Project-Rival/CLAUDE.md`; never writes Io source. | That file only. |

Default to **Design** when Ben is describing or asking, **Light** when he is instructing.
Full is *earned*, not assumed. If the mode changes mid-session, say so.

**Session start.** If the last commit on `main` is ≥ 3 days old, `git fetch origin` and integrate
`origin/main` before any work. Before minting a backlog id, run `node tools/session/next_id.js`.
`tools/status.ps1` (the `status` skill) is the "what's left / what shipped" dashboard. **Check
`git status` before committing: this checkout is shared with other sessions**, and an
uncommitted edit here can be wiped by another session's commit path.

## 2. Response style

Terse: **max 2 sentences per paragraph, max 15 words per clause**; more short paragraphs over
long ones. **Never cite a backlog item by bare id** — always `BL-229 (building selection)`;
every item carries a `short_name` for this. Commit messages and code comments are exempt.
Stay the advisor: state uncertainty and options with trade-offs; Ben makes the calls.

## 3. Where each question lives

Read for **traversal**: find the doc that owns the question and read that one. The corpus is
~650K tokens (`node tools/doc_weight.js`); reading it all is not an instruction anyone can
follow. Where a store has a query tool, use the tool, never load the file.

### Game — what it is
| Doc | Owns |
|---|---|
| `docs/CONCEPT.md` | Player identity, core loop, campaign shape, how it should feel. |
| `docs/SYSTEMS.md` | Every system, how they relate, which are load-bearing. |
| `docs/GLOSSARY.md` | Canonical terms. If a word is defined here, use it. |
| `docs/META_LAYER.md` | The predicate/effect substrate every rule is built from: `condition_set`, `modifier_set`. Read before adding any rule family. |
| `docs/PEOPLE.md` | Named individuals holding roles — a person exists only where a role gates something. |
| `docs/EVENTS.md` | Things that happen *to* the player; seeded uncertainty on the meta layer. |
| `docs/CLIMATE.md` | The living commons: strain, per-body state, hazard/habitability, the Era boundary's cause. |
| `docs/MANUAL.md` | The player-facing manual. |

### Economy
| Doc | Owns |
|---|---|
| `docs/economy/RESOURCES.md` | The resource list, tiers, terrain affinity, era split. |
| `docs/economy/PRODUCTION.md` | Buildings, recipes, placement, workforce, stockpile flow. |
| `docs/economy/MARKETS.md` | Market centres, clearing, price resolution, the order book. |
| `docs/economy/FINANCE.md` | The money loop: income, expenditure, maintenance, wages, interest, debt. |
| `docs/economy/CONTRACTS.md` | Promises between named parties: procurement and the mercenary contract — the income loop. |
| `docs/economy/LOGISTICS.md` | The network: traversal cost, reach, roads, scale/travel time, interdiction, Logistic Points. *Logistics is the road.* |
| `docs/economy/SUPPLY.md` | The flow: convoys — cargo, dispatch, cost, arrival. *Supply is the traffic.* |
| `docs/economy/TILES.md` | Two-axis terrain, deposit profiles, amenity tiles. |
| `docs/economy/POPULATION.md` | Population centres, agglomeration, habitability. |
| `docs/economy/ERAS.md` | The era ladder and the gate into space. |
| `docs/economy/RESEARCH.md` | Research points and technology unlocks (stub). |
| `docs/economy/SPACE_ASSETS.md` | Off-body assets (stub). |

### Military, politics, relations
| Doc | Owns |
|---|---|
| `docs/military/MILITARY.md` | How force works: two resolvers, units, muster, terrain, march, battles, upkeep, one reach field. |
| `docs/politics/NATIONS.md` | The nation as actor: territory, treasury, law, lobbying, the national budget. |
| `docs/politics/RELATIONS.md` | Sentiment (derived), stance (declared), reputation, embargo, standing — which quantity answers which question. |

### Generation & lore
| Doc | Owns |
|---|---|
| `docs/generation/GENERATION_STRATEGY.md` | Map of the generation layer and the economic premise. Start here for cross-doc questions. |
| `docs/generation/PLANETOLOGY.md` | Body-level atmosphere, chemistry, biosphere history. |
| `docs/generation/CONTINENTS.md` | Plates, drift, `height_bias`, the Continent lens. |
| `docs/generation/TILE_GENERATION.md` | The six-pass tile pipeline. |
| `docs/generation/PROVINCES.md` | The spatial unit of consequence: partition rules, three domains, walk order. |
| `docs/generation/NATION_GENERATION.md` | Territory placement, resource profile, character, naming. |
| `docs/generation/CORPORATION_GENERATION.md` | Nation assignment, focus, starting assets, finances. |
| `docs/generation/GENERATION_LEDGER.md` | The why-did-this-tile-generate surface. |
| `docs/lore/HISTORY.md` | The institutional ladder that drives the Era −1 sim. |
| `docs/lore/COLLAPSE.md` | Polity strategies and culminating events for Era −1. |
| `docs/lore/CREEDS.md` | Pantheons per cradle-culture, generated tongues. |

### AI & tech
| Doc | Owns |
|---|---|
| `docs/ai/AI_OPPONENT.md` | The whole AI direction: scored-utility rivals, the word interface, the local-model goal, the no-cloud invariant, the MCP server. |
| `docs/ai/ACTIONS.json` → `ACTIONS.md` | The action dictionary — every control as press/args/preconditions. Query with `tools/session/actions_query.js`; never hand-edit the mirror. **Any control change updates its entry.** |
| `docs/ai/STRATEGIES.md` | The meta, authored ahead of the game — research, not authority. |
| `docs/ai/LANGUAGE_POLICY_FEASIBILITY.md` | Research note: does a language-driven opponent compress and run locally. |
| `docs/tech/TECH_FOUNDATIONS.md` | Settled technical decisions and the prototype scope/exclusions. Read before any code or architecture suggestion. |
| `docs/multiplayer/MULTIPLAYER_PRINCIPLES.md` | Which settled decisions keep multiplayer cheap later. Non-binding. |
| `docs/research/*.md` | Research scaffolding (tech effects, ancient ladder, Era 1 landscape). Not authority. |

### UI
| Doc | Owns |
|---|---|
| `docs/ui/LAYOUT.md` | The application shell — how regions are arranged around the canvases. |
| `docs/ui/CANVASES.md` → `SOLAR.md`, `CIRCUMPLANETARY.md`, `PLANETARY.md`, `MINIMAP.md` | The zoom-ladder canvases and the minimap chrome. |
| `docs/ui/RENDERING.md` | Canvas ground rendering: the baked-chunk mechanism, C-F direction, grid rule, installations-as-geometry, animation, LOD. |
| `docs/ui/design/GLOBAL_STYLE_SHEET.md` | The visual-language exploration (owner: Joe) — style verdicts, palette, render iterations in `design/renders/`. Settled values promote into the authority docs. |
| `docs/ui/SELECTION.md` | The Selection element; the Active/Focus/Selection click model. |
| `docs/ui/TOOLTIP.md` | The shared hover-card primitive. |
| `docs/ui/LENSES.md` | The map-lens system and roster. |
| `docs/ui/ICONS.md` | The glyph vocabulary (`src/ui/icons.*` is the source of truth). |
| `docs/ui/DISCOVERY.md` | The two fogs: survey (geographic) and activity (commercial), competitor visibility, trade routes. |
| `docs/ui/STARTUP.md` | Menu → wizard → generation → in-game. |
| `docs/ui/MENU.md`, `HEADER.md`, `PROFILE.md`, `TIME_CONTROLS.md`, `CHAT.md`, `EXPLORER.md` | The shell's fixed panels, one each. |
| `docs/ui/ledgers/*.md` | One design Q&A per fold-out ledger (`README.md` gives the five axes). |
| `docs/ui/question_log.json` → `QUESTION_LOG.md` | Every surface's question, justification and demanding item. **Required on every surface**; regenerate with `render_question_log.js`; never build a check against it (Ben, 2026-08-01). |
| `docs/ui/ui_elements.json` | The surface inventory — every canvas, ledger, panel and chrome element that exists, with its `checks`. The spine a UI pass scopes off. **Query, don't load.** |
| `docs/ui/UI_COVERAGE.md` | Derived: what a green visual run actually proves, per element. `ui_coverage.js` (`--class --element --check --orphans --json`); orphan checks are the catalogue's staleness detector. |

### Development — method and state
| Store | Owns | Tool |
|---|---|---|
| `docs/development/DELIVERY.md` | The method: lifecycle, design state, depth verbs, batch, worktrees. Read before Full mode. | — |
| `docs/development/DEVELOPMENT_PRACTICES.md` | Harness testing (no unit framework), naming, doc standards, release cutting. | — |
| `docs/development/ROADMAP.md` | Milestone sequence through v1.0.0. The only place that says *when*. | — |
| `docs/development/backlog.json` | **Open work only** — closed items, delivered or cancelled, are not here at all. **Query, never load.** | `backlog_query.js` (`--status --priority --version --touches --grep --full`), `backlog_view.js`, `next_id.js`, `backlog_lint.js` |
| `docs/development/archive/backlog-design-*.json` | Closed items **whole** — row and prose. Amend anything closed **here**. The query tools union it automatically, so `--touches` still answers "is this built?", and `--status cancelled` finds work that was closed unbuilt. | `archive_landed.js` (rows, `--restore`), `archive_designs.js` (prose) |
| `docs/development/REFINED.md` | The active worklist — promoted tasks. Empty between work blocks. | — |
| `docs/development/req/requirements.json` | Requirements and their verification record. | `requirements_query.js`, `archive_requirements.js` |
| `docs/development/NEEDS_REVIEW.json` → `.md` | Ben's review queue: questions, decisions taken on his behalf, observations, novelty flags. Write **as things arise**, not at close. Resolved entries go cold in `archive/needs-review-<quarter>.json`. | `render_needs_review.js`, `archive_reviews.js` |
| `docs/development/review.json` | Blocked on a visual artifact only Ben can produce. | — |
| `docs/development/sprints.json` → `SPRINTS.md` | Sprint goal + retro per themed span; hot file holds open/gated only, completed sprints cold in `archive/sprints-*.json`. Pacing feedback, not authority. | `render_sprints.js`, `archive_sprints.js` |
| `docs/development/user_stories.json` → `USER_STORIES.md` | Player-intent coverage — the second route from docs to code. | `story_check.js` |
| `docs/development/DEVLOG_INDEX.md` → `DEVLOG.md` / `archive/DEVLOG-*.md` | What was built each session and why. **Start at the index.** | `devlog_index.js` (`--rollover`) |
| `docs/development/PHANTOMS.md` | Scan of designed-but-undocumented features; pointers to design sessions. | — |
| `docs/development/NEXT_SESSION.md` | A handoff note when one exists. Read at session start if present. | — |
| `docs/development/REVIEW_AUTOMATION.md` | How the review/merge loop is automated without losing the human. | `code-reviewer` agent |
| `KNOWN_BUGS.md`, `REVIEW_LOG.md`, `BACKLOG.md` | Retired. Do not add to them. | — |
| `.claude/rules/io-standing-rules.md` | The always-on invariants: determinism, scope, the AI-behaviour grants, terms. | — |
| `Project-Rival/CLAUDE.md` | The AI-player research discipline. | — |

## 4. Working method (condensed — DELIVERY.md is the authority)

- **Rule 0a — ad-hoc ideas.** Offer *A) backlog* / *B) build now (smoke-test, ask before
  commit)* before acting; no clarifying questions first.
- **Rule 0b — ambiguous measurements.** Report the exact current numbers with units, then ask.
  Open the live app whenever asking Ben to weigh in on visuals.
- **Rule 0c — log as you go, and only what needs a JUDGEMENT.** Anything wanting Ben's judgement goes
  into `NEEDS_REVIEW.json` *when it arises*. The closing message points at the log; it does not
  reproduce it. **The queue is not a backlog and not a notebook (Ben, 2026-09-01).** It reached 117
  open entries and was drained in one pass; most of them were work, and work belongs in
  `backlog.json`. Before filing, ask which of three things it is: a CALL only Ben can make → the
  queue; WORK somebody must do → a backlog item; a fact worth remembering → the comment or doc next
  to the code. An observation with no reader is not a record, it is a queue nobody finishes.
- **Novelty flag.** If no doc owns the task or it would quietly grow scope, file a
  `kind: "novel-work"` entry and continue.
- **Timestamp every new item**; newest-dated wins on conflict.

### The Full lifecycle
0. Item-spanning requirement (if `src/` changes) → 1. tasks in `REFINED.md` → 2. requirement
group → 3. collision map (a *splitting heuristic*, not a gate) → 4. build, review, verify — every
task terminal before any commit; a UI requirement needs a **live click**, not just a capture →
5. one commit per item:
```
<item title>

Tasks: <N completed>, <N cancelled>
Requirements: <N completed>, <N pending>, <N failed>
```
Pausing a group clean is a legitimate outcome.

### Sub-agents
Concurrent agents run in **separate worktrees** (`isolation: "worktree"`) when they write code;
disjoint doc edits may run in place. Brief each on a **tight block** with the one or two docs it
needs. Agents build and commit on their branch; **the main session merges, builds, verifies** —
assume nothing about self-reported success. An agent blocks on its own long waits and stops once
it has a decision; say so in the brief. Integration wiring stays in the main session.

Saved agents: `economy-dev`, `generation-dev`, `ui-dev` (focused implementers, one layer each)
and `code-reviewer` (cold adversarial review, author ≠ reviewer, for Full-mode/seam work).

### Skills
`verifier-visual` (headless `--verify` captures against `scripts/verify/*.lua`) ·
`verifier-headless` (`tools/verify/*.cpp` harnesses over `world/*`) · `verifier-review` (static
pre-compile gate after slice merges) · `scoped-commit` (stage only this task's files — the usual
path here) · `commit` (clean tree only) · `status` (the dashboard).

**Tool creation is skill creation.** A check without a tool: author it (`tools/verify/*.cpp`,
`scripts/verify/*.lua`, a script), then wrap it in a `.claude/skills/<name>/SKILL.md` — which
needs Ben's permission. Denied → run it once, note "ad hoc" in the requirement.

### Build & permissions
`build_app.bat` is the build; `build/` is real, `build_live/` a copy. Harnesses build to
`build_gen\verify\<name>.exe` under one path-scoped allow rule; `%TEMP%` is banned as a harness
target (unsigned exes there are indistinguishable from a dropper). `.claude/settings.json` is
broad prefix allows plus a `deny` net (`rm -rf`, `git push`, `git reset --hard`, `git clean`);
tighten rather than broaden. Git writes from native only, never the Cowork shell.
