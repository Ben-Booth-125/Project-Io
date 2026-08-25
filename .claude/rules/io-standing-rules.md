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
  deposit depletes. The player's own corp is never auto-acted on **strategically**.
  See `src/world/economy_system.cpp` (run_economy_step § agency).
  **Rival-corp strategic exception (BL-202/BL-203, landed 2026-08-01/02; widened by
  BL-293, 2026-08-08):** background corporations run a **deterministic scored-utility**
  layer over the corp-command seam — build, dial, survey, hire and sell decisions scored
  each tick, plus predictive spending, **plus standing sell orders on the open market**
  (`src/world/corp_ai.cpp`). Determinism is the binding constraint, not simplicity: the
  scorer is pure, seeded and replayable, and it issues only legal `corp_command` verbs.
  The trading grant is Ben's, 2026-08-07: *"Order book needs to be a background process,
  the AI must be able to trade as a player does."* It is deliberately a grant of **reach,
  not of skill** — a rival lists surplus stock above a hold threshold at a floor over the
  market's rarity price, and that first-cut rule lives in `corp_ai_params` so tuning it is
  a data change. What stays deferred is **nation** behaviour (backlog.json § BL-054) and
  any planner that is not deterministic. Authority: `docs/ai/AI_OPPONENT.md`.
  **Player-corp exception (BL-181, landed 2026-07-15):** the *workforce target* of a
  player building may be auto-solved each tick to maximise that building's profit — a
  **narrow, local, deterministic, opt-out** convenience for a single micromanagement dial,
  not strategic agency. It is opt-out per building (`building_component.workforce_auto`; a
  manual target pins it — from the management UI *or* from the `set_workforce` command verb,
  which since BL-293 clears the flag exactly as the press always has), and the
  `set_workforce_auto` verb hands the dial back. It never places, relocates, retargets,
  or decommissions. This is the *only* sanctioned auto-action on the player's corp; anything
  beyond this one dial stays prohibited. See `solve_workforce_target` in economy_system.cpp.
  **Rival-corp hiring exception (BL-324, landed 2026-08-08):** background corporations may
  raise units through the same `hire_unit` corp_verb the player uses — scored alongside
  build/dial/survey/sell in `corp_ai.cpp`'s candidate list, capped at one hire per
  evaluation, gated on the corp's own stockpile/market access (never on cash). A deliberate
  widening of the BL-202/BL-203 exception, not a new category: hiring is one more legal verb
  on the same deterministic scored-utility layer, not a planner of its own.
  **"Never on cash" governs AVAILABILITY, not spend (Ben, 2026-08-13, ruling on NR-218).**
  Which roster rows are offered is decided by stockpile and market access alone — a
  cash-poor corp still sees every row it has the goods for. But since BL-394 gave
  `hire_unit` a real credit cost, that cost is subject to the **solvency gate like every
  other spend**: the scorer carries it in the candidate's `spend`, so a rival cannot hire
  itself below its own reserve floor, and a hire reserves its cash against later candidates
  in the same evaluation. Availability is cash-free; spending is not.
  **Spectator mode has NO SUBJECT for this rule (BL-409, landed 2026-08-14).** Every
  exception above widens *what may be done to a corp a human owns*. Spectator mode is
  not another such widening — it removes the owner. Ben, 2026-08-14: *"In spectator
  mode, there is no need to mark a corp as played by a human. 'Who plays your corp'
  collapses as a question."* The prohibition protects a corp **because** a human owns
  it, so under `corp_ai_params::spectating` its precondition is absent and every corp
  evaluates on the same staggered cadence, `world::player_entity` included — that field
  degrading to a camera/ledger anchor with no ownership meaning. Two properties keep
  this honest, both asserted by `tools/verify/spectator_determinism.cpp`: the flag
  **defaults false**, so an ordinary played session is byte-identical (verified against
  the genuine pre-BL-409 build, `state_hash 3CBAD1D44EE71EDE` — that was the value AT
  THE TIME; the golden has since been re-blessed as the world legitimately changed, and
  `spectator_determinism` carries the dated provenance log, so read the harness for the
  current constant and this line as the historical claim it is), and admitting one more
  corp **shifts no rival's cadence slot**, since the index is over the sorted corp set.
  Outside spectate the prohibition is unchanged and absolute. **These two properties are
  what the harness guarantees — not RNG-stream-identical behaviour across a content
  change (Ben, 2026-08-24, ruling on NR-596).** `spectator_determinism.cpp` also carried
  a third, stricter check (a seated+spectated corp reaches every verb family it reached
  as a rival) that a resource-roster widening (BL-586 slice 2) broke through simple
  RNG-stream drift, unrelated to the new content. Bit-identical RNG-stream determinism
  across a content change was ruled out of scope for this harness — saves carry the
  actual world state, not a replay-from-seed, and occasional randomness is a deliberate
  strategy lever, not a defect — so that check is retired, not the two properties above.
  **Nation and polity behaviour is GRANTED (Ben, 2026-08-18, ruling 4 of NR-331), in the
  BL-202/BL-203 shape.** This is the exception that BL-054 (nation behaviour) had deferred
  indefinitely, and it is granted for **both** grains Ben named: Era −1 polities inside the
  generation sim, and campaign-era nations. The binding constraints are unchanged and are the
  whole basis of the grant — the behaviour must be **pure, seeded, deterministic and replayable**,
  a scored-utility layer issuing only legal verbs, and **never a planner**. What it admits: a
  polity choosing among its sim verbs; a nation holding a treasury, setting a tariff or tax rate,
  and enacting a law; a polity carrying pair-state toward another. What it does **not** admit:
  anything whose timing, latency or ordering can vary the generated world (`docs/lore/HISTORY.md`
  is the authority for what the ladder produces, not a licence to randomise it), and any cloud
  model in the loop — the no-cloud invariant in `docs/ai/AI_OPPONENT.md` § 10 is untouched.
  Reason for the grant: three of the four systems Ben's 2026-08-18 brief names — international
  trade, logistics and diplomacy — are nation-grain, and `GENERATION_STRATEGY.md`'s economic
  premise already assumes nations that act. See `docs/development/SPRINTS.md` § Sprints 26–33.
  **A rival acting POLITICALLY against the player's corp is granted (Ben, 2026-08-22,
  answering the design register), on the same terms as BL-450.** This is the newest widening
  and the one whose subject is furthest from the original prohibition: a rival may **lobby**
  a nation to shift its budget weights or its law (BL-539), and a nation's derived stance may
  **gate the player's corp** out of a territory (BL-540) — both being consequences imposed on
  a corp a human owns by an actor the player does not control. Same constraints, unchanged:
  deterministic, seeded, scored-utility, legal verbs only, never a planner. It was raised
  rather than assumed (NR-517) precisely because reading the 2026-08-18 nation grant as
  already covering it would have set the quiet precedent this section exists to prevent.
  What it does **not** admit: a rival *enacting* law (only a nation can), or influence
  acquired outside the `lobby` verb.

  **A rival scoring STANCE toward the player's corp is a separate, corp-grain widening
  (BL-450, rivals score stance) and is GRANTED on the same terms and date.** It is called out
  separately because every other widening above is dated and scoped, and because its subject is
  the one actor this prohibition exists to protect: it is the first time a rival takes a
  *relational* action against a corp a human owns. Same constraints — deterministic, seeded,
  scored-utility, legal verbs only. Hostility remains a **declared state a corp opts into**
  (Ben, 2026-08-17), so a rival may score and declare it, never acquire it ambiently.

  **Rivals may EXTEND THE NETWORK, and direct convoys on it (Ben, 2026-08-24, the Sprint 18
  design form).** Two verbs join the scorer's candidate list on the same terms as every grant
  above — deterministic, seeded, scored-utility, legal verbs only, never a planner. (1)
  `place_road`, plus port / inland-hub build candidates scored like any building: the
  generator half of Logistic Points' constraint 5 (`docs/economy/LOGISTICS.md` § Logistic
  Points — a rival must be able to build the generator), honouring Ben's 2026-08-22 "before
  LP lands" ordering. (2) `dispatch_convoy` in its directed form, through the same
  `price_convoy_leg`/`commit_convoy` seam auto-dispatch and the player use — a grant of reach
  to the player's own verb, no fourth code path, its spend under the solvency gate like every
  spend. Raised on the form rather than assumed (the NR-517 precedent); fresh authoring — the
  purged BL-447 prose is reference only. Owners: BL-599 (rival roads and hubs), BL-600
  (rival directed dispatch).
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
