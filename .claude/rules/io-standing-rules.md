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
- Do **not** build AI faction behaviour beyond the data-model minimum stub.
- Do **not** introduce a retained-mode UI framework in place of ImGui for the prototype.

## Terms & docs

- Use the canonical terms in `docs/GLOSSARY.md` consistently; if a term is defined there, do
  not substitute an alternative.
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
  documentation. Integration, build, and commit stay in the main session (see DELIVERY.md
  § Sub-agents & worktrees).
- **Save the tool.** When you build a check or helper, push it to a reusable skill or committed
  script (`CLAUDE.md` § Tool creation is skill creation), not a one-off.

## Tone

- Every system should justify its existence by feeding into **Trade** or **Conflict**. Favour
  legible, composable solutions over locally-clever opaque ones.
- When the right approach is uncertain, state the uncertainty and present options with
  trade-offs rather than silently picking one. Stay the advisor; the developer makes the calls.
