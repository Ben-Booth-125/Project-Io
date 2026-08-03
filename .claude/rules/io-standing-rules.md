# Project Io — Standing Rules

Load-bearing invariants that apply to **every session without exception**. This file is the
always-on rulebook extracted from `CLAUDE.md` and the design docs so the core constraints are
short and impossible to miss; the *map* of which doc owns what stays in `CLAUDE.md`. Where a
rule has a fuller authority, it is cited — this file does not redefine it.

## Scope & sequence

- Do **not** suggest or implement anything outside the prototype scope in
  `docs/tech/TECH_FOUNDATIONS.md`.
- Do **not** design or implement a milestone that depends on an earlier one not yet complete —
  flag it instead (`docs/development/ROADMAP.md` owns the sequence).
- Read the relevant design doc *before* responding; the docs are the authoritative source for
  all design and technical decisions (`CLAUDE.md` § Documents is the map).

## Determinism & data model

- The simulation is **deterministic**: seeded world generation (`make_hard_coded_world`), a
  fixed Tick model. Do not introduce non-determinism into the `world/*` logic.
- Do **not** expose individual tile data to Lua.
- Do **not** use unprotected sol2 calls where errors can occur.
- Do **not** add SQLite — flat binary serialisation is correct for the prototype.
- Do **not** build AI faction behaviour beyond the data-model minimum stub. **Scoped
  exception (BL-079, landed 2026-07-07):** background (non-player) corporations may take
  *narrow, local, deterministic* per-building actions from mechanical triggers — idle a
  persistently loss-making building, switch a floored recipe, throttle extraction as a
  deposit depletes. This is **not** a licence for strategic planning, relocation, or
  global optimisation; the player's own corp is never auto-acted on **strategically**.
  Anything broader stays deferred (backlog.json § BL-054). See `src/world/economy_system.cpp`
  (run_economy_step § agency).
  **Player-corp exception (BL-181, landed 2026-07-15):** the *workforce target* of a
  player building may be auto-solved each tick to maximise that building's profit — a
  **narrow, local, deterministic, opt-out** convenience for a single micromanagement dial,
  not strategic agency. It is opt-out per building (`building_component.workforce_auto`; a
  manual target in the management UI pins it), and it never places, relocates, retargets,
  or decommissions. This is the *only* sanctioned auto-action on the player's corp; anything
  beyond this one dial stays prohibited. See `solve_workforce_target` in economy_system.cpp.
- Do **not** introduce a retained-mode UI framework in place of ImGui for the prototype.

## Terms & docs

- Use the canonical terms in `docs/GLOSSARY.md` consistently; if a term is defined there, do
  not substitute an alternative.
- **Real history is a mechanism reference, never a name source (Ben, 2026-08-03).** The design
  leans on real history constantly and should keep doing so — the institutional ladder
  (`docs/lore/HISTORY.md`), the Era −1 sim's "use Rome as a sandbox" (BL-271), the mil-sim's
  calibration constants. What transfers is the **mechanism**: how a charter enforces a promise,
  how a frontier stalls, how a hegemony forms or fails to. What must **never** transfer is a
  proper noun. Every generated name in Io — nation, province, city, corporation, body, person —
  is **sci-fi / fantasy**, produced by the seeded template banks and phoneme tables, never drawn
  from an Earth list and never Earth-flavoured. If a doc says "Rome", it is naming an analogy
  for the reader, not content for the game. **Project-Rival is the one exception and only
  outside Io**: it plays an actual RTS with actual civilisations, and hands Io *numbers and
  doctrine*, never names.
- A settled design for an **open backlog item** lives in the item (BACKLOG.md / backlog.json)
  until the work lands; it propagates into the subject's authority doc *as part of landing the
  work*. Authority time-slices — do not edit the authority doc ahead of the work (see
  `docs/development/DELIVERY.md` § Design state).

## Working method (see DELIVERY.md for the full lifecycle)

- **Match the effort (Rule 0).** State the mode. **Light** by default — a one-line fix, an
  obvious cleanup, a doc tweak: make it, check it, say what you did, no ceremony. **Full** is
  *earned* by work whose coordination cost it repays (touches the economy/save-format/integration
  seam, spans more than ~2 logic files, or carries determinism/reconciliation risk).
- **Ad-hoc ideas (Rule 0a).** When the user raises an unscoped idea with no explicit "do it
  now", offer two options before acting — **A) save to the backlog** or **B) implement now**
  (smoke-test, then ask before committing) — without first asking clarifying questions.
- **Sub-agent isolation.** Concurrent sub-agents run in **separate git worktrees** (the primary
  safety mechanism); the file collision map is a *splitting heuristic* for carving focused
  agents, no longer the hard gate. Keep each agent on a **tight block of code**, reading minimal
  documentation — a narrow, well-scoped agent is the unit that pays back. Integration, build, and
  commit stay in the main session (see DELIVERY.md
  § Sub-agents & worktrees).
- **Save the tool.** When you build a check or helper, push it to a reusable skill or committed
  script (`CLAUDE.md` § Tool creation is skill creation), not a one-off — the saved check keeps
  paying; the loose one is forgotten.
- **Toggle rule (UI).** Any control whose active state is visible is a **toggle**: clicking it while
  active undoes it. A nav-rail menu icon toggles its ledger open/closed; re-clicking the
  *currently-active* sub-view tab **closes** the ledger (it does not collapse to an overview);
  switching between tabs is an ordinary view change. **Exempt:** cross-cutting selectors
  (body/market/resource combos), which switch a target rather than express an active state; and the
  Selection element, which is selection-driven with no rail slot.
- **Git writes from native only.** `git add`, `commit`, `merge`, `push` must run from native
  Claude Code or a native terminal — never the Cowork shell. The Cowork bridge mounts the Windows
  repo into a Linux VM; git writes cause CRLF diff churn and `.git` lock-file failures. File
  edits via the file tools (Read/Write/Edit) are safe from either context. (BL-058; dissolves
  once development moves fully to native Linux — BL-057.)

## Tone

- Every system should justify its existence by feeding into **Trade** or **Conflict**. Favour
  legible, composable solutions over locally-clever opaque ones — the legible solution reads as
  obvious in hindsight; the clever one is a debt.
- When the right approach is uncertain, state the uncertainty and present options with
  trade-offs rather than silently picking one. Stay the advisor; the developer makes the calls.
