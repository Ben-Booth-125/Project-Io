# Project Io — Claude Reference

Project Io is a near-future space-based 4X grand strategy game. The player controls a corporate entity competing through resource extraction, trade, and military conflict across an Earth-like solar system. The project is in prototype phase, solo-developed in C++ with Lua scripting.

Read the documents below before responding to any request. They are the authoritative source for all design and technical decisions.

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
The milestone map from the current state to v0.1.0 (the finished prototype): the version sequence, the theme of each minor, and the v0.1.0 done-definition. Forward-facing and lean — it sits above TODO/TASKS, naming which theme each minor carries, not the individual Briefs. Read this for questions about sequence, what comes when, or whether a feature belongs in the prototype's remaining arc.

**`docs/development/DEVELOPMENT_PRACTICES.md`**
Testing framework (Catch2), naming conventions, documentation standards, the per-milestone ImGui panel rule, the standing development constraints, the tone/approach guidance, and how to cut a release. Read this alongside TECH_FOUNDATIONS when working on implementation.

**`docs/development/DEVLOG.md`**
Running session log — chronological record of what was built each session and
in-session decisions. Consult when asked about prior work, open items, or why
a specific implementation choice was made.

**`docs/development/TODO.md`** and **`docs/development/TASKS.md`**
The two-file backlog. TODO.md holds *described intent* — parked additions and
deferred ideas with file pointers. TASKS.md holds the *active, prioritised,
actionable worklist*: a Brief is **promoted** into file-scoped, dependency-
and parallelisation-marked tasks (the A–F style) when we decide to act on it.
Read TODO.md § TODO vs. TASKS for the workflow before promoting or executing
backlog work.

**`docs/ui/CANVASES.md`**
Overview of the three primary canvases — Solar, Circumplanetary, and Planetary — arranged as a **zoom ladder** (descend by clicking a body in the primary, ascend by clicking the minimap). Covers the shared layout, selection/view state, and implementation approach. Per-canvas detail lives in `SOLAR.md`, `CIRCUMPLANETARY.md`, and `PLANETARY.md`; the minimap chrome and ladder navigation are in `MINIMAP.md`. Authoritative reference for Layer 2 and for later work that adds overlays to these canvases.

**`docs/ui/SELECTION.md`**
The Selection info element — a pinned, polymorphic panel showing detail of the current selection. Defines the three interaction states (Active / Focus / Selection) and the single-click-selects / double-click-navigates click-model change shared across all canvases. Read before any work on selection state, canvas click handling, or the shared per-entity content builders (which the Tile Ledger and the future hover card also use).

**`docs/ui/LAYOUT.md`**
Surface-level description of the application shell — how the screen regions (navigation pane, canvas area, time column, ledger windows) are arranged around the canvases. Read for questions about overall UI layout; CANVASES.md covers the canvas internals.

**`docs/ui/ICONS.md`**
The icon vocabulary — every hand-drawn vector glyph in the `ui::icons` namespace (`src/ui/icons.{hpp,cpp}`, the source of truth): building markers, resource pips, unit markers, nav-rail affordances, and the map-lens glyphs. Catalogues each glyph's shape, meaning, usage, and colour source, the shared `(dl, centre, r, colour)` contract, and the recipe for adding one. Read before adding or changing any on-canvas/strip glyph; identity *colours* live in `presentation.hpp`, not here.

**`docs/ui/LENSES.md`**
The map-lens system — the overlay modes (`overlay_mode` in `src/ui/ui_state.hpp`) selectable from the canvas control strip. The **Corporation** lens is fully settled (tile ownership tint, player vs. rival colours, Planetary-only); the **Supply / Market / Faction** sections currently record existing behaviour and the proposed **Resource** lens is a stub — completing them is a Brief under TODO § Canvas. Read before any work on overlay modes, lens rendering, or the lens icon vocabulary (which propagates to ICONS.md).

**`docs/economy/RESOURCES.md`**
The canonical resource list: all 23 resources organised into three tiers (raw → refined → product), their terrain affinity and body availability, the Era 0 / Era 1 split, and the seven-resource prototype subset. Read before any work involving resource types, tile deposits, or market goods.

**`docs/economy/PRODUCTION.md`**
All extraction and processing buildings: placement rules, valid terrain, output resources, and full recipe tables. Also covers the workforce scalar model, stockpile flow, and the Layer 3 prototype scope. Read before any work involving buildings, recipes, or production logic.

**`docs/economy/ERAS.md`**
The Era system — the formal phase structure of the game's industrial arc. Era 0 (Terrestrial) starts at the campaign epoch; Era 1 (Early Space) is unlocked by the Rocketry research + Launchpad + propellant gate. Read for questions about what is accessible when, and how the transition to space is structured.

**`docs/economy/TILES.md`**
Tile classification: the two-axis terrain model (composition × landform), resource deposit profiles per terrain type, ambient resource guarantee, and amenity tile concept. Read before any work involving terrain types, tile generation, or the `terrain_composition`/`terrain_landform` enums. Includes an implementation note recording that the two-axis split is now implemented.

**`docs/economy/POPULATION.md`**
Population centres, scale/agglomeration mechanics, land-use trade-offs, and habitability feedback. Deferred from the prototype but designed here so the data model positions correctly. Read for questions about workforce, habitability, or population demand.

**`docs/generation/TILE_GENERATION.md`**
The procedural tile generation strategy and six-pass pipeline used in `hard_coded_world.cpp`. Covers solar parameters (temperature class, atmosphere class, hydrological state, geological activity), prototype body profiles, the hybrid terrain / noise-banded ocean / cluster landform approach, and deposit generation rules. Read before any work on tile generation or the terrain enum expansion.

**`docs/generation/NATION_GENERATION.md`**
The procedural nation generation strategy: Voronoi BFS territory placement over the tile map, resource profile derivation, political character assignment, and naming. Nation system behaviour is an open item; this document covers generation only. Read before any work on the world political layer or campaign setup.

**`docs/generation/CORPORATION_GENERATION.md`**
The procedural corporation generation strategy: nation assignment, industrial focus, starting asset placement, and financial profile. Covers the player corporation marker, the deferred corporation selection screen, and open design items (franchising, nation-seeded privatisation, tax, Era-based sovereignty). Read before any work on faction setup or campaign initialisation.

**`docs/generation/GENERATION_LEDGER.md`**
The Generation Ledger (design only) — a tuning/analysis surface that explains *why* a tile generated as it did, reading the per-pass intermediates exposed by `generate_body_tiles`'s optional `generation_record`. Covers the per-tile derivation breadcrumb (height/ocean → band/moisture → composition → landform → deposits), per-body histograms, the regenerate-on-demand (don't-persist) data lifetime, and surfacing as a Ledger window plus a Planetary field-overlay lens. Read before building the generation ledger or any heightmap/moisture/band overlay.

---

## Publication pipeline

The five-step **Publish** lifecycle for acting on a Brief. Full detail lives in
`docs/development/TODO.md` § Publish and `docs/development/TASKS.md`. This is the
condensed reference.

### The five steps

0. **Brief-spanning requirement (gate — if the Brief changes `src/`).** Before decomposing a
   `src/`-changing Brief into tasks, write a **brief-spanning requirement** in
   `req/REQUIREMENTS.md` — one requirement covering the whole Brief, **usually a
   visual-verification (`visual`) requirement**. It is the end-to-end acceptance gate and is
   written first so the decomposition is shaped by it. Doc-only Briefs are exempt.
1. **Create tasks** — promote the Brief into TASKS.md: decompose into the smallest
   independently-buildable steps (foundation first), scope each step to its exact files,
   and mark dependencies and parallelisation.
2. **Create requirements** — write or link requirements in `req/REQUIREMENTS.md` per the
   requirements policy there.
3. **Check parallel-safety** — build the collision map (which files each task touches) and
   resolve any scope collisions before execution. Tasks with **disjoint file scopes are
   parallel-safe**: fan them out to concurrent sub-agents. Only same-file (colliding) tasks
   stay sequential.
4. **Complete tasks** — implement, review, and verify each task against its requirements.
   Tasks that prove blocked or out of scope are *cancelled* (intent returned to TODO, stubs
   removed from TASKS.md) — not left in flight.
5. **Commit** — one commit per Brief, format:
   ```
   <Brief title>

   Tasks: <N completed>, <N cancelled>
   Requirements: <N completed>, <N pending>, <N failed>
   ```

**When publishing multiple Briefs together** (a **Batch Publish**; see GLOSSARY), run the five
steps as **barriers across the whole set** (breadth-first, not depth-first): every Brief clears
step *N* before any starts *N+1*. No Brief is committed while another still has a task in flight.
Step 4 closes on *terminal* states (complete **or** cancelled) — a blocked task is cancelled
rather than held open.

A Batch Publish also runs a **documentation-coverage discipline** around the five steps (full
detail in `docs/development/TODO.md` § Publish): up front, **determine per Brief whether the
docs already record the implementation** (or it is a direct consequence of documented
behaviour) — Briefs that fail are doc-changing and get a **per-Brief doc collision map** with
**sub-agent fan-out** across disjoint docs; every changed doc carries a **minor transient
"what was changed" note** (a visible `> ⟳` blockquote, removed once reviewed); and the batch
**always adds an `S`-tier review Brief per changed doc** and, when it made non-trivial design
calls, **closes with a proportional design-direction Q&A** (recorded in DEVLOG; see
`docs/development/DEVELOPMENT_PRACTICES.md` § Design-direction Q&A).

### Proportionality and session boundaries

Two standing guidelines temper the lifecycle above. They are reasserted operationally in
`docs/development/TASKS.md`.

- **Proportionality — the procedure is a guideline, not a fixed ceremony.** Treat the
  documented procedure as proportional to the work. Before promoting, ask: substantial Brief,
  or quick low-risk high-value change? For the latter — a one-file fix, an obvious cleanup, a
  cheap optimisation — skip the TASKS/REQUIREMENTS ceremony, make and verify the change
  directly, commit. The five-step lifecycle exists for work whose coordination cost it pays
  back; applying it to trivial changes is the over-engineering this guards against.

- **Session boundaries — pausing is a legitimate outcome.** Driving a group to *complete* in
  one block is the default, not a mandate. When ending a session early serves the work — the
  batch is large, context is drifting, or a natural checkpoint is reached — **pause** the group
  rather than force completion or cancel it: leave it clean and resumable (TASKS.md true to
  state, build green or breakage noted, a short "resume here" handoff line). A paused group is a
  deliberate scoping choice, distinct from a *cancelled* one, which reverts intent to TODO.

### Parallelisation (the load-bearing rule)

**Two tasks may run as concurrent sub-agents only if their file write-sets are disjoint.**
Two agents writing the same file corrupt each other's edits.

Practical consequences:

- **Build the collision map first** — list every file each task will write. Hotspot files
  (shared headers, `hard_coded_world.cpp`, the integration seam) are where parallelism dies;
  see them before planning waves.
- **Passes inside one generator stay sequential** — all passes of a generator share one
  `.cpp`; give the whole group to one agent. Concurrency lives *across* groups, not within a
  generator.
- **Design for disjointness where it is also good design** — e.g. storing tile→nation
  ownership in a `world` map rather than a `tile_component` field kept the nation and
  tile-tuning groups off each other's files so they could run concurrently in v0.0.3.
- **Keep hotspot files and integration in the main session** — the file every group
  eventually touches is never given to a sub-agent. The main session wires hooks, runs the
  build, and verifies after each wave. Sub-agents write code on a disjoint scope and report
  their public signature; they do not build or commit.
- **Group concurrent tasks into waves** — run a wave of disjoint-scope agents in parallel;
  verify before starting the next wave. Assume nothing about an agent's self-reported
  success — verify retroactively.

**Fan-out is a discretionary call, made *after* writing the tasks and the collision map —
not a default, and not something to ask permission for each time.** By then the shape is
clear, so decide on the evidence: **fan out** when there is a substantial wave of
disjoint-scope work a cold agent can execute from its task text alone (e.g. several
independent generators or audits); **stay in the main session** when the parallel win is
marginal — a short serial dependency chain, interfaces still co-evolving, or work that keeps
returning to shared/integration files — because each spawn starts cold and re-derives
context, which can cost more than it saves. This paragraph is standing authorisation to use
sub-agents at that discretion: state the call and its reason ("fanning out A/B/C — disjoint;
keeping the D→E→F chain serial — co-evolving interfaces") rather than asking first.

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
- **`scoped-commit`** — stages exactly the files belonging to the current task and commits
  with the correct format, without bundling unrelated working-tree changes. Use whenever
  committing, especially on the default branch or when the tree has pre-existing edits.

#### Tool creation is skill creation

When a check or automation does not yet have a tool, **author the tool, then push it to a
skill** — a skill is a permanent, reusable, discoverable asset; a loose tool or bespoke
procedure is forgotten. **Creating or modifying a skill requires user permission**, so the
workflow is:

1. **Attempt to author the tool** (a `tools/verify/*.cpp` harness, a `scripts/verify/*.lua`
   check, a script, or a documented procedure).
2. **Push it to a skill** — wrap the capability in a `.claude/skills/<name>/SKILL.md` (the
   `verifier-*` skills are the model), asking the user to authorise it.
3. **If skill creation is denied, request running the tool as a one-off** — execute it this
   session without promoting it, and note in the requirement's Notes that the method was run
   ad hoc rather than saved.

For a larger or speculative skill that needs design rather than a quick wrap, **propose it as
a Brief** (category: Documentation or the relevant system category) instead of improvising.

#### Promoting a skill's permissions into settings.json

When a skill is created or modified, its Bash commands may require entries in
`.claude/settings.json` (`permissions.allow`) to avoid per-invocation prompts.
**Adding a rule to settings.json is a permanent, project-wide change** — it silently
allows any matching command in all future sessions, not just the one being tested.
The bar for adding a rule is therefore higher than for adding the skill itself.

**Required before any rule is added:**

1. **Read the SKILL.md in full.** Do not infer what commands a skill runs from its
   description alone — open `.claude/skills/<name>/SKILL.md` and read the entire
   Procedure section. Identify every `Bash` call the skill makes.
2. **List the exact commands and the rule that would cover them.** Write out
   each proposed rule alongside the command it permits, so the reviewer can
   match them one-to-one.
3. **Request user approval.** Present the list and ask explicitly: *"May I add
   these rules to `.claude/settings.json`?"* Do not add rules speculatively or
   bundle them silently into a broader change.
4. **Use the narrowest rule that works.** Prefer a prefix pattern
   (`Bash(git *)`) over a bare tool allow (`Bash`). Prefer matching a
   specific executable path over a wildcard if the skill always calls one binary.
5. **Document the mapping in a comment block above the settings file** — since
   JSON forbids inline comments, record the skill-to-rule mapping in this CLAUDE.md
   section (below) so the rationale survives.

**Do not approve a rule you have not manually traced to a specific command in
a specific SKILL.md.** If a proposed rule covers more than the skill warrants,
reject it and ask for a tighter alternative.

#### Current settings.json rules — approved mapping

`.claude/settings.json` → `permissions.allow`:

| Rule | Skill(s) | Commands covered |
|------|----------|-----------------|
| `Bash(git status*)` | `commit`, `scoped-commit` | `git status --porcelain`, `git status --short` |
| `Bash(git add*)` | `commit`, `scoped-commit` | `git add <paths>` |
| `Bash(git diff*)` | `commit`, `scoped-commit` | `git diff --stat`, `git diff --cached --name-status` |
| `Bash(git commit*)` | `commit`, `scoped-commit` | `git commit -m ...`, `git commit -F -` |
| `Bash(git log*)` | `commit`, `scoped-commit` | `git log -1 --pretty=format:...` |
| `Bash(git show*)` | `commit`, `scoped-commit` | `git show --stat HEAD` |
| `Bash(git checkout*)` | `scoped-commit` | `git checkout -b <branch>` |
| `Bash(git restore*)` | `scoped-commit` | `git restore --staged <paths>` |
| `Bash(cmake *)` | `verifier-visual` | `cmake --build build --config Debug --target ProjectIo` |
| `Bash(& "build*)` | `verifier-visual` | `& "build/Debug/ProjectIo.exe" --verify <script>` |
| `Bash(.\build*)` | `verifier-visual` | `.\build\Debug\ProjectIo.exe --verify <script>` |
| `Bash(cl *)` | `verifier-headless` | `cl /nologo /std:c++20 ...` MSVC compile (requires vcvars64 already active) |
| `Bash(& ".\econ_harness*)` | `verifier-headless` | `& ".\econ_harness.exe"` |
| `Bash(& ".\econ_stability*)` | `verifier-headless` | `& ".\econ_stability.exe"` |
| `Bash(& ".\world_audit*)` | `verifier-headless` | `& ".\world_audit.exe"` |
| `Bash(& ".\construction_harness*)` | `verifier-headless` | `& ".\construction_harness.exe"` |

**Intentionally omitted — will still prompt:**

- `Bash(git push*)`, `Bash(git reset*)`, `Bash(git clean*)`, `Bash(git branch*)` —
  destructive or outbound; no current skill requires them without user oversight.
- `Bash(cmd *)` — removed as a shell escape hatch; the `verifier-headless` vcvars64
  compound invocation (`cmd /c "vcvars64.bat && cl ..."`) will prompt until that
  step is wrapped in a dedicated script with a narrower allow rule.
- New harness executables — when a new harness is added to `verifier-headless`, its
  exe name must be explicitly added here before it runs without a prompt.