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
- Do **not** expose individual tile data to Lua. **Scoped exception (Ben, 2026-08-28, ruling on
  NR-698):** the verify API may answer *where* a good is deposited —
  `verify.find_deposit_tile(good)` returns the grid position of ONE tile on the active body and
  nothing else. Not the magnitude, not terrain, not ownership; one tile in raster order, so it
  cannot be walked into a map of the resource field. The rule is otherwise unchanged. Reason for
  the grant: the Resource lens's deposit pivot is reachable only by pressing ground that carries
  the selected good, and a script had no way to find such ground — so that half of every lens check
  swept blindly or asserted nothing, and a check that cannot aim is not a check. Verify-only by
  construction (`src/core/verify_api.cpp`, bound behind `--verify`).
- Do **not** use unprotected sol2 calls where errors can occur.
- Do **not** add SQLite — flat binary serialisation is correct for the prototype.
- Do **not** build AI faction behaviour beyond the data-model minimum stub. A **register of
  dated, scoped exceptions** widens this — background corporations and nations run a
  deterministic scored-utility layer; the player's own corp is never auto-acted on
  strategically, save one opt-out dial. Every grant is bound by the same constraints, and
  they are the whole basis on which each was given: **pure, seeded, deterministic and
  replayable, issuing only legal verbs, and never a planner.** No cloud model is ever in the
  loop. **The register lives in `docs/ai/AI_OPPONENT.md` § 11 — read it before touching
  `src/world/corp_ai.cpp`, and read it before assuming a grant covers your case.**
- **A NEW widening is RAISED, never assumed.** This is the gate, and it is not a formality:
  reading an existing grant as already covering a new subject is exactly the quiet precedent
  the prohibition exists to prevent (NR-517, and the register records where that was caught).
  If the subject is new — a different actor, a different grain, a relation rather than an
  action — it is a new grant and it is Ben's to give.
- Do **not** introduce a retained-mode UI framework in place of ImGui for the prototype.
- **An AI-facing seam is an untrusted input boundary (recorded 2026-08-14 with BL-387/BL-396/
  BL-397, after four instances of the pattern in one session; promotion to a standing rule is a
  delegated call — NR-234).** Validation written for a trusted
  in-process caller does not transfer to an external surface: the moment a seam takes wire input
  (`--serve`, the MCP server, any future agent transport), every field must be validated **as the
  value that lands in the destination** — range-checked against the real domain before any narrowing
  cast (a finite double can be an infinite float), with the whole command rejected on violation,
  never truncated, wrapped, or clamped silently. A rejection must mutate **nothing**. Actor
  authority is part of the same boundary: the protocol layer decides which corp a session may act
  as and read as; `apply_corp_command` stays permissive for the in-process scorer.

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
- **Authority docs are state-independent (Ben, 2026-08-23).** A doc says what is true of the
  game's design — never whether a piece is built, when it landed, or which backlog item did it.
  A settled design is written into the subject's authority doc **the moment it is settled**;
  the backlog item points at it and carries only the work. Whether a thing is built is a
  backlog fact (`backlog_query.js --touches <doc>`), not a doc fact. Cite a `BL-` id in a doc
  only as the owner of a design, with its short handle; cite a dated ruling as provenance.
  Never write "landed", "not yet built", "shipped" or a build-status section into one (see
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
- **An agent blocks on its own long waits and stops once it has a decision (2026-08-19).** A
  sub-agent running a long build or harness checks the result itself rather than yielding control
  mid-wait; once it reaches a stated conclusion it stops, rather than continuing to re-investigate
  a question it already answered. Brief this explicitly — it does not happen by default (see
  DELIVERY.md § Sub-agents & worktrees).
- **A UI requirement needs a live check (2026-08-19).** A scripted `verifier-visual` capture
  proves a surface renders; it does not prove a press on it is reachable. Before marking any
  `visual` requirement on an interactive surface `complete`, open the built app, click the thing,
  and look — a clean compile and a green harness are not sufficient on their own (see DELIVERY.md
  § The Delivery lifecycle, step 4).
- **Raise the novelty flag (Ben, 2026-08-20).** If the task in hand feels **novel** — no
  authority doc owns it, no saved role or established pattern fits, or it would quietly grow
  the project's scope — file a `kind: "novel-work"` entry in
  `docs/development/NEEDS_REVIEW.json` *at the moment the feeling arises*, then continue
  (pause first if the scope growth is large). Sub-agents flag it in their report; the main
  session files it. Novelty should be chosen, not accreted.
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
