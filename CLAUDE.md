# Project Io — Claude Reference

Project Io is a near-future space-based 4X grand strategy game. The player controls a corporate entity competing through resource extraction, trade, and military conflict across an Earth-like solar system. The project is in prototype phase, solo-developed in C++ with Lua scripting.

The documents below are the authoritative source for all design and technical decisions. Read the
ones your request touches — **not all of them**: the set is now ~600K tokens (measure it with
`node tools/doc_weight.js`), so "read everything first" stopped being an instruction anyone could
follow. Read for **traversal**: find the doc that owns the question and read that. Where a doc has
an index or a query tool — `DEVLOG_INDEX.md`, `backlog_query.js`, `actions_query.js`,
`ACTIONS_INDEX.json` — use it instead of loading the file.

## Response style

Keep responses terse: **max 2 sentences per paragraph**, **max 15 words per clause**. Break longer
thoughts into more (short) paragraphs rather than longer ones.

**Never refer to a backlog item by bare id.** A bare `BL-229` forces Ben to go and look it up
(his words, 2026-07-30: "it takes me a while to decipher what BL-229 refers to"). Always pair the
id with a short human handle — `BL-229 (building selection)` — in prose, headings, lists and
status summaries alike. Every item in `backlog.json` carries a `short_name` for exactly this;
use it, or a plainer few-word gloss where the `short_name` is itself cryptic. Commit messages and
code comments are the one exception: there the surrounding text already supplies the context.

---

## Documents

**`docs/CONCEPT.md`**
The player identity, core mechanics, and campaign design. Start here for questions about what the game is and how it should feel.

**`docs/SYSTEMS.md`**
Every game system, how they relate to one another, and which are load-bearing. Read this for questions about scope, system design, or how a feature fits into the whole.

**`docs/GLOSSARY.md`**
Canonical definitions for all project terms. Use these terms consistently. If a term is defined here, do not substitute alternatives.

**`docs/tech/TECH_FOUNDATIONS.md`**
All settled technical decisions: language, framework, architecture, tick model, data model, UI approach, and serialisation. Read this before writing any code or making any architectural suggestion. It also defines the prototype scope and what is explicitly excluded.

**`docs/development/ROADMAP.md`**
The milestone map from the current state through v0.1.0 (the finished prototype) and into the post-prototype arc (laws, tech, the v0.2.0 AI opponent): the version sequence, the theme of each minor, and the v0.1.0 done-definition. Forward-facing and lean — it sits above the backlog and worklist, naming which theme each minor carries, not the individual items. Read this for questions about sequence, what comes when, or whether a feature belongs in the prototype's remaining arc.

**`docs/development/DEVELOPMENT_PRACTICES.md`**
Testing framework (Catch2), naming conventions, documentation standards, the per-milestone ImGui panel rule, the standing development constraints, the tone/approach guidance, and how to cut a release. Read this alongside TECH_FOUNDATIONS when working on implementation.

**`docs/development/DEVLOG_INDEX.md`** and **`docs/development/DEVLOG.md`**
Running session log — chronological record of what was built each session and
in-session decisions. Consult when asked about prior work, open items, or why
a specific implementation choice was made.

**Start at the index, not the log.** `DEVLOG_INDEX.md` is one generated line per session
(date, title, the `BL-` ids it touched, which volume holds it) across every volume — find the
session you want there, then open only that entry. `DEVLOG.md` holds the live sessions;
sessions before 2026-07 live in `docs/development/archive/DEVLOG-2026.md`. Regenerate the
index with `node tools/session/devlog_index.js` after adding an entry; roll older entries out
with `--rollover <YYYY-MM>`.

**`docs/development/SPRINTS.md`**
Weekly goal + retro rhythm layered over the backlog — a goal stated at the week's start, a retro
comparing what landed against it, including the DEVLOG Runtime pacing signal. Feedback for Ben,
not a new authority. Read/update when starting or closing out a week's work.

**`docs/development/backlog.json`**, **`docs/development/BACKLOG.md`**, **`docs/development/REFINED.md`**, and **`docs/development/DELIVERY.md`**
The backlog and delivery system. **`backlog.json`** is the canonical **metadata index and design
prose store** — status (`designed` ✓ / `design-owed` ~), priority, difficulty, sequencing, a
**version goal** (every new item names the minor it targets — DELIVERY.md § Priority, difficulty
& version goal), file scope, and the **`design` field** holding each item's prose. **`BACKLOG.md`** is a
**drained legacy file** (drain completed 2026-07-31): every body has migrated to `backlog.json`'s
`design` field; the file keeps only a tombstone note plus seven one-line stubs that old
`@BACKLOG.md` design pointers still resolve to. New items get **no** `BACKLOG.md` body. Authority time-slices: `backlog.json` while the item is open; the
subject's authority doc once the work lands. **`REFINED.md`** is the *active worklist*: a `designed`
item is **promoted** into file-scoped tasks when we act on it. **`DELIVERY.md`** is the method
authority — the Delivery lifecycle, design-state model, depth verbs, and worktree sub-agent model.
Read DELIVERY.md before promoting or executing backlog work.

**Query the backlog; do not load it.** `backlog.json` is ~700 KB. Use
`node tools/session/backlog_query.js` — it defaults to index fields only (id, short_name, status,
priority, version_goal) over open items, and takes `--status`, `--priority`, `--version`,
`--category`, `--touches <path>`, `--grep`, `--fields`, `--table`, `--count`. Add `--full` (or name
an id) to pull one item's prose. That time-slice above is now **physical**: when an item lands, its
design/resolution/completion prose moves to `docs/development/archive/backlog-design-<quarter>.json`
via `tools/session/archive_designs.js`, and the item keeps a pointer. `--full` and `backlog_view.js`
resolve it transparently; amend a landed item's prose **in the cold file**, not in `backlog.json`.

**`docs/development/user_stories.json`** and **`docs/development/USER_STORIES.md`**
The user-story catalogue — the game decomposed by **player intent** (what the player is *trying to
do*), the axis neither the design docs (system/surface) nor the backlog (feature) provides. It is
the **second route from docs to code**: each story traces *upward* to the systems/pillars that give
it meaning and *downward* to the UI surface + backlog item + requirement that realise and verify it,
turning intent into a coverage check (a story no surface serves is a gap; a feature no story needs
is scope to justify). **`user_stories.json`** is canonical (mirrors `backlog.json`'s `_schema`/
`_note`/coverage conventions); **`USER_STORIES.md`** is the readable mirror. Companion to — not a
duplicate of — the backlog; it is the artifact **BL-098**'s UX-review activity consumes. Read
before UX-review work, coverage audits, or when reasoning about whether a player goal has a home.

**`docs/ui/CANVASES.md`**
Overview of the three primary canvases — Solar, Circumplanetary, and Planetary — arranged as a **zoom ladder** (descend by clicking a body in the primary, ascend by clicking the minimap). Covers the shared layout, selection/view state, and implementation approach. Per-canvas detail lives in `SOLAR.md`, `CIRCUMPLANETARY.md`, and `PLANETARY.md`; the minimap chrome and ladder navigation are in `MINIMAP.md`. Authoritative reference for Layer 2 and for later work that adds overlays to these canvases.

**`docs/ui/SELECTION.md`**
The Selection info element — a pinned, polymorphic panel showing detail of the current selection. Defines the three interaction states (Active / Focus / Selection) and the single-click-selects / double-click-navigates click-model change shared across all canvases. Read before any work on selection state, canvas click handling, or the shared per-entity content builders (which the Tile Ledger and the hover card also use).

**`docs/ui/LAYOUT.md`**
Surface-level description of the application shell — how the screen regions (navigation pane, canvas area, time column, comms dock, Selection band, fold-out ledgers) are arranged around the canvases. Read for questions about overall UI layout; CANVASES.md covers the canvas internals.

**`docs/ui/STARTUP.md`**
The app's entry screens — the `app_screen` state machine (menu / generating / in_game), the main menu, the New World wizard (preferences in, `world_params` out), the staged generation screen, and the `start_new_game` handoff. Written 2026-07-31; these are the first screens a player (and a fresh session) sees. Read before any work on the title screen, the wizard, or campaign initialisation.

**`docs/ui/ICONS.md`**
The icon vocabulary — every hand-drawn vector glyph in the `ui::icons` namespace (`src/ui/icons.{hpp,cpp}`, the source of truth): building markers, resource pips, unit markers, nav-rail affordances, and the map-lens glyphs. Catalogues each glyph's shape, meaning, usage, and colour source, the shared `(dl, centre, r, colour)` contract, and the recipe for adding one. Read before adding or changing any on-canvas/strip glyph; identity *colours* live in `presentation.hpp`, not here.

**`docs/ui/LENSES.md`**
The map-lens system — the overlay modes (`overlay_mode` in `src/ui/ui_state.hpp`) selectable from the minimap lens bar. The lens family is built: the eight bar lenses (**Corporation, Country, Resource, Market, Population, Opportunity, Production, Continent**) plus the off-bar **Scarcity, Industry, Reach**, and **Supply-routes** — a current-roster table opens the doc. Only the **Supply** flow lens remains gated. Read before any work on overlay modes, lens rendering, or the lens icon vocabulary (which propagates to ICONS.md).

**`docs/ui/DISCOVERY.md`**
The discovery & intelligence model — how the player *learns about the world*. Owns the two independent "fogs": the **geographic fog** (the Survey system, BL-067 — bodies start unsurveyed; a paid survey reveals tiles + deposit bands region-by-region) and the **activity fog** (the commercial sphere, BL-089 — a body-level Unknown/Known/Stale/Visible tier lit by the player's own trade routes + presence, `body_activity_visibility`), plus the **competitor-visibility rule** (BL-068 — rival buildings visible, internals private, markets public) and **persistent trade routes** (BL-088, the substrate the activity fog reads). The two fogs are independent axes (a body can be Known-but-unsurveyed). Read before any work on survey, competitor intelligence, trade-route recording, or the activity fog; the canvas rendering lives in SOLAR/PLANETARY/SELECTION, the glyphs in ICONS.

**`docs/ai/ACTIONS.json`** and **`docs/ai/ACTIONS.md`**
The action dictionary (BL-270) — every control in the game as `{press, typed args, preconditions, expected_output, reason_to_select}`, in five families (gameplay / canvas / lens / ledger / chrome). **`ACTIONS.json`** is canonical and machine-consumable — the third leg of the word interface an AI player uses (blackboard export BL-206 = read, corp-command seam = write, dictionary = meaning); **`ACTIONS.md`** is the generated mirror (`node tools/session/render_actions.js` — also the store's parse/shape check; never hand-edit the mirror, which also emits **`ACTIONS_INDEX.json`**, the compact `[id, surface]` table of contents a language agent holds in context, fetching full entries via **`tools/session/actions_query.js`** — entries deliberately carry no urgency/importance; the live AI scores those on a 2D urgency × importance map at decision time, AI_OPPONENT.md § 6a). The gameplay family is *transcribed* from `src/world/corp_command.hpp`, not authored — where dictionary and seam disagree, the dictionary is wrong. **Any change to a control, binding, lens, ledger or panel must update its entry** — a stale entry misleads the AI player the way a stale golden misleads a visual check. Sibling axes: `user_stories.json` (intent), UX_QUESTIONS/BL-260 (information).

**`docs/economy/RESOURCES.md`**
The canonical resource list: all 23 resources organised into three tiers (raw → refined → product), their terrain affinity and body availability, the Era 0 / Era 1 split, and the seven-resource prototype subset. Read before any work involving resource types, tile deposits, or market goods.

**`docs/economy/PRODUCTION.md`**
All extraction and processing buildings: placement rules, valid terrain, output resources, and full recipe tables. Also covers the workforce scalar model, stockpile flow, and the Layer 3 prototype scope. Read before any work involving buildings, recipes, or production logic.

**`docs/economy/MARKETS.md`**
The market model — how market centres are seeded (BL-096, resource-carved), the clearing tick, price resolution (`resolve_price`, the price band, EMA smoothing), the sell/buy order book with preferred-seller routing (BL-037), and the honest current limitations. Carved out 2026-07-31; previously the clearing model had no doc home. Read before any work on markets, prices, orders, or the exchange-policy arc (BL-160/BL-161).

**`docs/economy/FINANCE.md`**
The money loop — `apply_budget`'s five flows (income / expenditure / maintenance / wages / interest), the BL-049 wage/maintenance split, debt interest (BL-073), and the surfaces that read it (budget ledger, header runway, per-building profitability). Carved out 2026-07-31. Read before any work on corporate balances, operating costs, or the budget surfaces.

**`docs/economy/ERAS.md`**
The Era system — the formal phase structure of the game's industrial arc. Era 0 (Terrestrial) starts at the campaign epoch; Era 1 (Early Space) is unlocked by the Rocketry research + Launchpad + propellant gate. Designed, **not implemented** — the doc opens with a status banner; in code, space access is gated on launchpad presence only. Read for questions about what is accessible when, and how the transition to space is structured.

**`docs/economy/TILES.md`**
Tile classification: the two-axis terrain model (composition × landform), resource deposit profiles per terrain type, ambient resource guarantee, and amenity tile concept. Read before any work involving terrain types, tile generation, or the `terrain_composition`/`terrain_landform` enums. Includes an implementation note recording that the two-axis split is now implemented.

**`docs/economy/POPULATION.md`**
Population centres, scale/agglomeration mechanics, land-use trade-offs, and habitability feedback. Deferred from the prototype but designed here so the data model positions correctly. Read for questions about workforce, habitability, or population demand.

**`docs/generation/GENERATION_STRATEGY.md`**
The map of the generation layer — how the per-subject generation docs (planetology / tile / nation / corporation / ledger) relate, and the home of the **economic premise**: a saturated, earth-like base whose broad industry the Nation AI owns, with the player and major AI as **specialist** space-interested corporations. Read first for questions spanning more than one generation doc, or about why corporations start lean. Cross-cutting open items (building tiers, allied-corp/franchise origin, post-WW2 grounding) live here.

**`docs/generation/PLANETOLOGY.md`**
Design-owed authority for the body-level Planetology pass (BL-167) — generated atmosphere/chemistry and a simulated abiogenesis/evolution history, ahead of tile generation, explicitly modelled on Shadow Empire's planet generation. Framed as a first-impression pillar, not a nice-to-have. Read before designing or implementing atmosphere generation, body history, or anything that would consume a body's biosphere-richness outcome.

**`docs/generation/TILE_GENERATION.md`**
The procedural tile generation strategy and six-pass pipeline used in `hard_coded_world.cpp`. Covers solar parameters (temperature class, atmosphere class, hydrological state, geological activity), prototype body profiles, the hybrid terrain / noise-banded ocean / cluster landform approach, and deposit generation rules. Read before any work on tile generation or the terrain enum expansion.

**`docs/generation/CONTINENTS.md`**
The Continents/Drift pass (`src/world/continents.{hpp,cpp}`) — plate derivation from the planetology outputs, the `height_bias` contract into tile Pass 1, `plate_id` retention for the Continent lens (BL-226), and the biography lines it appends. Written 2026-07-31; replaces the stale authority pointer in `continents.hpp`. Read before any work on plates, landmass shape, or the Continent lens.

**`docs/generation/NATION_GENERATION.md`**
The procedural nation generation strategy: Voronoi BFS territory placement over the tile map, resource profile derivation, political character assignment, and naming. Nation system behaviour is an open item; this document covers generation only. Read before any work on the world political layer or campaign setup.

**`docs/generation/CORPORATION_GENERATION.md`**
The procedural corporation generation strategy: nation assignment, industrial focus, starting asset placement, and financial profile. Covers the player corporation marker, the deferred corporation selection screen, and open design items (franchising, nation-seeded privatisation, tax, Era-based sovereignty). Read before any work on faction setup or campaign initialisation.

**`docs/generation/GENERATION_LEDGER.md`**
The Generation Ledger (design only) — a tuning/analysis surface that explains *why* a tile generated as it did, reading the per-pass intermediates exposed by `generate_body_tiles`'s optional `generation_record`. Covers the per-tile derivation breadcrumb (height/ocean → band/moisture → composition → landform → deposits), per-body histograms, the regenerate-on-demand (don't-persist) data lifetime, and surfacing as a Ledger window plus a Planetary field-overlay lens. Read before building the generation ledger or any heightmap/moisture/band overlay.

**`docs/lore/HISTORY.md`**
Design-owed authority for the **institutional history ladder** — why the campaign world of 1960 is market-based and non-hegemonic, as a seven-stage causal ladder (agrarian surplus → the enforceable promise / Charter Age → fragmentation-with-connectivity → capital disciplines the sovereign → the energy transition → the averted rupture → saturation). It picks up where `PLANETOLOGY.md` stops (the civilisation gate) and generates what `NATION_GENERATION.md` / `GENERATION_STRATEGY.md` currently assume. The ladder is **driven, not narrated** (Ben, 2026-07-30): it produces fragmentation, nation count, industrialisation timing and drawdown rather than describing an already-generated world. Promoted into **BL-220** (timestamp foundation), **BL-221** / **BL-222** / **BL-223** (the stages) and **BL-224** (the non-hegemony invariant). Read before any work on settlement, industrialisation, or the campaign premise's historical claims. **Stages 5–6 as written are superseded** — see BL-223.

---

## Delivery pipeline

The lifecycle for acting on a backlog **item**. **Full authority lives in
`docs/development/DELIVERY.md`**; this is the condensed reference. The method answers to the
game's own standard: each change feeds something, composes cleanly, reads legibly.

### Session start — check origin if the last local session is stale

If the previous local session was **≥ 3 days ago** (judge by the date of the last commit on `main`),
run `git fetch origin` and compare `origin/main` **before starting work**. Under the push policy
`main` is kept current locally and pushed only at major releases — but origin can still move ahead
independently (work from other machines or cloud sessions). Integrate any upstream commits
(fast-forward, or rebase our branch onto `origin/main`) *before* committing new work, so a stale base
doesn't cause the divergence + backlog-ID-renumber churn it otherwise does. And before authoring a
new backlog item, allocate its id with **`node tools/session/next_id.js`** (scans all branches for the
true max — don't mint off the local file). Full method: DELIVERY.md § Parallel worktree coherence;
see also the push-policy and backlog-ID-collision memories.

### Rule 0 — size the effort to the job

Every non-trivial task states its **mode**:

- **Light (default).** A one-line fix, an obvious cleanup, a doc tweak — make it, check it, say
  what you did. No tasks, no requirements, no ceremony. The small win stays whole.
- **Full (earned).** Work whose coordination cost it repays — touches the economy / save-format /
  integration seam, spans more than ~2 files of real logic, or carries determinism risk. Run the
  Delivery lifecycle below.

**Rule 0c — log reviews as you go; do not save them for a closing summary.** Anything that
wants Ben's judgement — an open question, a call taken on his behalf, an observation he
should see — goes into **`docs/development/NEEDS_REVIEW.json`** *at the moment it arises*,
not into the end-of-session message. A closing summary is read once and then gone; the
session ends, the context is dropped, and an unrecorded delegated decision becomes
indistinguishable from a decision Ben made. Write the entry when the decision is taken,
while the reasoning is still in hand.

The end-of-session message then **points at** the log rather than reproducing it: say what
landed, and that the review notes carry N entries. Keep the prose summary short — the
durable record is the file. (Ben, 2026-08-01.) The three homes, per their own `_note`
blocks: `NEEDS_REVIEW.json` = questions, decisions-taken and observations (non-blocking);
`review.json` = blocked on a visual artifact only Ben can produce; `backlog.json` = actual
work. If an answer creates work, file the backlog item and resolve the entry with its id.

**Rule 0b — ambiguous measurements: report the current numbers first.** When a sizing or layout
request has more than one reading ("a bit smaller", "3/5 the size" — of the width, the height, or
both?), do **not** guess and do **not** ask an open question. State the **exact current
measurements with units** for every element in scope — and what they are derived from — then ask.
Ben can say what he was picturing far faster against real numbers than against prose. Pair this
with the existing rule to **open the live app** whenever asking him to weigh in on visuals.

**Rule 0a — ad-hoc ideas.** When the user raises an unscoped idea with no explicit "do it now",
offer two options **before acting**, without first asking clarifying questions: **A) save to the
backlog**, or **B) implement now** (smoke-test, then ask before committing).

### Source of truth — where each thing lives

| Concern | Authority | Notes |
|---|---|---|
| Backlog **metadata** (status, priority, sequencing, version goal, files) | `docs/development/backlog.json` | Queryable; the JSON wins over any prose/glyph. |
| Backlog **design prose** for an open item | `docs/development/backlog.json` (`design` field) | Design authority *while the item is open*; `BACKLOG.md` is drained (pointer stubs only, 2026-07-31). |
| **Active worklist** (promoted tasks) | `docs/development/REFINED.md` | Transient; empty between work blocks. |
| **Blocker triage** (items awaiting a UI/visual artifact from Ben) | `docs/development/review.json` | Companion to the backlog, not a replacement — see its own `_note`. |
| **Review log** (open questions, decisions taken on Ben's behalf, observations) | `docs/development/NEEDS_REVIEW.json` | Non-blocking. Written **as work happens** (Rule 0c), never deferred to a closing summary. |
| **Ben's review queue** (open questions + decisions taken on his behalf) | `docs/development/NEEDS_REVIEW.json` (view: `NEEDS_REVIEW.md`) | Not work and not blocking. **Append here rather than dropping a judgement call** — especially a `decision-taken`, since an unrecorded delegated decision is indistinguishable from one Ben made. |
| **Player-intent coverage** (user stories) | `docs/development/user_stories.json` (view: `USER_STORIES.md`) | The second route — intent axis; companion to the backlog, consumed by BL-098's review. |
| **Method** (lifecycle, depth verbs, batch, worktrees) | `docs/development/DELIVERY.md` | The long-form of this section. |
| **Requirements** (data + history) | `docs/development/req/requirements.json` (policy `docs/development/req/REQUIREMENTS.md`) | |
| **Standing invariants** | `.claude/rules/io-standing-rules.md` | Always-on; the "do not" rules. |
| Backlog **design prose** for a **landed** item | `docs/development/archive/backlog-design-<quarter>.json` | The cold store. Moved there on landing by `archive_designs.js`; amend it **here**, not in `backlog.json`. |
| What was built / why | `docs/development/DEVLOG_INDEX.md` → `DEVLOG.md` (live) / `archive/DEVLOG-<year>.md` | Find the session in the index; open only that entry. |
| **Weekly goal / retro** (feedback for Ben) | `docs/development/SPRINTS.md` | Not an authority — pacing signal only. |

### The Delivery lifecycle (Full mode)

0. **Item-spanning requirement (gate — if the item changes `src/`).** Write one requirement
   covering the whole item in its `docs/development/req/requirements.json` group (usually a `visual` check), first,
   so decomposition is shaped by it. Doc-only items exempt.
1. **Create tasks** — promote the item into `REFINED.md`: smallest independently-buildable steps
   (foundation first), each scoped to its files, dependencies marked.
2. **Create requirements** — append the item's requirement group to `docs/development/req/requirements.json`.
3. **Plan parallelisation** — build the file **collision map**; it *informs how to split focused
   sub-agents* (see below), no longer gates them.
4. **Complete tasks** — implement, review, verify against requirements. Blocked/out-of-scope tasks
   are **cancelled** (intent returned to the backlog), not left in flight.
5. **Commit** — once all tasks are terminal, one commit per item:
   ```
   <item title>

   Tasks: <N completed>, <N cancelled>
   Requirements: <N completed>, <N pending>, <N failed>
   ```

**Batch Delivery** runs the steps as **barriers across the whole set** (breadth-first); step 4 is
the load-bearing barrier (all tasks terminal before any commit). It also runs the
**documentation-coverage discipline** (per-item doc collision map, transient `> ⟳` change notes, a
standing `S`-tier review item per changed doc, a closing design-direction Q&A when non-trivial
calls were made) and emits a **coarse `%` progress marker** between tool calls. **Timestamp every
new item**; newest-dated wins on conflict (a dated item outranks undated prose; no retroactive
refactor). Full detail in `DELIVERY.md`.

**Proportionality & pausing.** The lifecycle is proportional to the work (Rule 0); a Light change
skips steps 1–2. Pausing a group is a legitimate outcome — leave it clean and resumable rather
than forcing completion or cancelling. Left clean, it resumes without archaeology.

### Sub-agents & worktrees (the parallelism model)

**Worktrees are the primary isolation mechanism.** Concurrent sub-agents each run in their own git
worktree (`isolation: "worktree"`), so two agents touching the same file no longer corrupt each
other. This **replaces** the old hard rule that sub-agents must have disjoint file write-sets.

- **The collision map is now a *splitting heuristic*, not a gate** — use it to carve **focused,
  meaningful agents** (each a coherent vertical slice), not to prove disjointness.
- **Keep each agent on a tight block of code, reading minimal documentation** — give it the task
  text, its files, and the one or two docs it actually needs, not the whole design corpus. A
  narrow, well-scoped agent is the unit that pays back.
- **Agents build and commit on their own worktree branch; the main session merges** them in
  dependency order, runs the integrating build, and verifies. Verify retroactively — assume
  nothing about an agent's self-reported success.
- **Hotspot/integration wiring stays in the main session.**
- **Fan-out is a discretionary call made *after* the tasks and collision map** — state the call
  and its reason rather than asking permission each time.

### Skills

These skills exist and should be used proactively rather than reinventing their steps:

- **`verifier-visual`** — runs the headless `ProjectIo --verify <script>` harness, inspects
  PNG captures, and reports against requirements. Authorising a new visual check means adding
  a `scripts/verify/<feature>.lua` and invoking this skill. Use for any `visual`-class
  requirement.
- **`verifier-headless`** — compiles and runs a `tools/verify/<name>.cpp` harness over the
  SDL/Lua-free `world/*` logic and reports its PASS/FAIL assertions. Use for any `headless`-class
  requirement (economy arithmetic, tile generation, placement audits). Authorising a new check
  means adding a `tools/verify/*.cpp` harness and naming it in the skill.
- **`verifier-review`** — static, no-compile review over an integrated diff; reports a verdict on
  cross-item symbol consistency, compile-by-inspection, standing invariants, the serialisation
  seam, and requirement coverage. The cheap pre-compile gate: run it in a Batch Delivery after
  slice merges and **before** the integrating build (DELIVERY.md step 4a). A filter, not a
  substitute — always still compile.
- **`scoped-commit`** — stages exactly the files belonging to the current task and commits
  with the correct format, without bundling unrelated working-tree changes. Use whenever
  committing, especially on the default branch or when the tree has pre-existing edits.

#### Tool creation is skill creation

When a check or automation does not yet have a tool, **author the tool, then push it to a
skill** — a skill is a permanent, reusable, discoverable asset; a loose tool or bespoke
procedure is forgotten. The check committed today catches next month's regression. **Creating or modifying a skill requires user permission**, so the
workflow is:

1. **Attempt to author the tool** (a `tools/verify/*.cpp` harness, a `scripts/verify/*.lua`
   check, a script, or a documented procedure).
2. **Push it to a skill** — wrap the capability in a `.claude/skills/<name>/SKILL.md` (the
   `verifier-*` skills are the model), asking the user to authorise it.
3. **If skill creation is denied, request running the tool as a one-off** — execute it this
   session without promoting it, and note in the requirement's Notes that the method was run
   ad hoc rather than saved.

For a larger or speculative skill that needs design rather than a quick wrap, **propose it as
a backlog item** (category: Documentation or the relevant system category) instead of improvising.

#### settings.json permissions (slimmed model — confirmed 2026-06-16)

`.claude/settings.json` now uses **broad prefix allows + a `deny` safety net** rather than a
per-command mapping table. The allow-list covers routine read/build/git/worktree commands; the
`deny` list blocks the genuinely dangerous (`rm -rf`, `git push`, `git reset --hard`,
`git clean`) outright. This is the lighter model adopted from Project-Fulcrum's process, and the
allow/deny split is **confirmed**.

When in doubt, tighten an allow rule rather than broaden it. `deny` takes precedence over `allow`,
so a denied command is blocked even if an allow rule would match.

Verify-harness exes are covered by one path-scoped rule, `Bash(& ".\build_gen*)`, rather than a
per-harness entry each. This replaces the old per-binary allows (`econ_harness`, `world_audit`,
etc.), which assumed a repo-root `/Fe:` and are now removed. The rule stays narrow by being
anchored to a **directory**: `verifier-headless` builds every harness to
`build_gen\verify\<full_harness_name>.exe`, so a new harness runs prompt-free without editing
this file. `%TEMP%` is banned as a harness output target — see that skill's Procedure § for why
(unsigned exes in user-writable staging are indistinguishable from a dropper, and whitelisting
`%TEMP%` in a virus scanner to work around it would defeat the scanner).