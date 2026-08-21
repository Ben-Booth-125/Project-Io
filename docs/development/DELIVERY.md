# Project Io — Delivery Method

How work flows from intent to a committed, verified change. This is the long-form authority for
the backlog model and the **Delivery** lifecycle; `CLAUDE.md` carries the condensed reference and
`.claude/rules/io-standing-rules.md` the always-on summary.

The three artefacts:

| Artefact | Role |
|---|---|
| [`backlog.json`](backlog.json) | **Canonical backlog — metadata index.** Every item's status, priority, difficulty, sequencing, file scope. Queryable; the source of truth for metadata. |
| [`BACKLOG.md`](BACKLOG.md) | **Drained 2026-07-31 — no bodies remain.** Keeps a tombstone plus seven pointer stubs that surviving `@BACKLOG.md` design pointers resolve to (`backlog_lint.js` Invariant 4 checks them). **Kept, not deleted** — deleting it would break those pointers. |
| [`archive/backlog-design-<quarter>.json`](archive/) | **Cold store.** A landed item's design/resolution prose moves here via `archive_designs.js`; the hot item keeps an `archived` pointer. `--full` and `backlog_view.js` resolve it transparently. **Amend a landed item's prose here, not in `backlog.json`.** |
| [`REFINED.md`](REFINED.md) | **Active worklist (transient).** A `designed` item is *promoted* here into file-scoped, dependency-marked tasks when we act on it. Cleared as tasks complete. |
| [`NEEDS_REVIEW.json`](NEEDS_REVIEW.json) | **Review log (non-blocking), transient.** Open questions, decisions taken on Ben's behalf, observations — written **at the moment they arise** (Rule 0c), never saved for a closing summary. Resolved entries are **pruned promptly** (Ben, 2026-08-14), not kept as an audit trail — the durable record is wherever the answer landed. Mirror: `NEEDS_REVIEW.md`. |

*(History: `backlog.json` + `BACKLOG.md` replace the former `OPENS.md`; `REFINED.md` replaces
`TASKS.md`. The lifecycle verb "Publish" was renamed "Deliver" — "Cut" stays reserved for cutting a
release, see `DEVELOPMENT_PRACTICES.md` § Cutting a release.)*

### Where design prose lives (updated 2026-08-04 — the migration is finished)

Prose has **three** homes, split by the item's lifecycle:

1. **Open item** → the `design` field in `backlog.json`. This is the design authority while the
   item is open. Rich markdown tables in the string are fine.
2. **Landed item** → `archive/backlog-design-<quarter>.json`, moved there by `archive_designs.js`
   at close-out; the item keeps an `archived` pointer. **Amend a landed item's prose in the cold
   file**, not in `backlog.json` — editing the hot copy silently diverges from what readers see.
3. **The subject's authority doc** → once the work lands and the design propagates. Authority
   time-slices; see § Design state.

`BACKLOG.md` is **finished as a drain** (completed 2026-07-31). It holds no prose — only a
tombstone and seven stubs that surviving `@BACKLOG.md` pointers resolve to. Those pointers name an
authority doc; **do not "migrate" them**, there is nothing to move. New items never get a
`BACKLOG.md` body.

Set `design` to `"@<authority-doc>"` only when the prose genuinely lives in a separate doc.

*(The 2026-06-17 policy this replaces described an in-flight migration with a migrate-on-first-edit
rule and `BACKLOG.md` "converging to empty, deleted once empty". It converged; it is not deleted,
because the pointer stubs are load-bearing.)*

## The one idea

Separate thinking from doing, and **size the effort to the job** (Rule 0). For a tiny change this
is thirty seconds in your head; for a big one, write it down. Do not apply the heavy version to
small jobs.

The method answers to the game's own standard: each change feeds something, composes cleanly,
reads legibly — the same test the systems must pass, applied to the work that builds them. A
small job finished clean in one motion is the method working, not the method skipped. The
ceremony is for the work that earns it — spent there, it buys the speed everywhere else.

## Two modes (Rule 0)

Every non-trivial task states its mode:

- **Light** — a one-line fix, an obvious cleanup, a doc tweak. Make it, check it, say what you
  did. No tasks, no requirements, no ceremony. A clean one-liner is a pleasure; ceremony would
  rob the small win of it. **This is the default.**
- **Full** — work whose coordination cost it repays. Run the Delivery lifecycle below and make
  each step visible. Full is *earned* when the work touches the economy / save-format /
  integration seam, spans more than ~2 files of real logic, or carries determinism/reconciliation
  risk.

### Ad-hoc ideas (Rule 0a)

When the user raises an idea or change that is **not clearly scoped work** — no backlog item, no
explicit "do this now" — offer a two-option choice **before acting**, without first asking
clarifying questions:

- **A) Save for later** — capture it as a backlog item; make reasonable guesses; record
  assumptions in the item's design body / open questions.
- **B) Implement now** — build immediately, smoke-test, and **ask before committing**. No polish
  or refinements until the user confirms.

Taste qualifies: when something merely reads wrong, fixing it is work, not vanity — it gets the
same two options.

## Design state — the two open states

Every backlog item is **not yet implemented** (implemented work leaves the backlog). What varies
is whether its **design is settled**. The `status` field (in `backlog.json`) is authoritative; the
`glyph` field beside it in `backlog.json` mirrors it 1:1 for human skimming:

- **`designed` (glyph `✓`)** — design settled; **promote-ready**. What remains is execution. (May
  still be *blocked* on a dependency existing — a sequencing fact, not a design gap; see
  `requires` and `blocked_on`.)
- **`design-owed` (glyph `~`)** — design still owed. The item names the problem and open
  questions but **must be designed before it is promoted**. Doing the design *is* the next action.

Status is orthogonal to **priority** (importance) and **difficulty** (size).

**Design happens in the item, not mid-flight.** Pausing to settle a `design-owed` item beats
redesigning during a Delivery (redesign in place is costly). **The backlog is the design authority
while a design is open** — the settled design lives in the item's `design` field in
`backlog.json`, which is by definition more
current than any authority doc on that subject. Authority **time-slices**: the
backlog while the item is open; the subject's authority doc once the work lands and the item is
removed. Propagating the design into its authority doc is **part of landing the work**.

### Item timestamping & precedence

The backlog accretes across sessions, so two items — or an item and older prose — can describe the
same subject differently:

- **Timestamp a new item** (the `written` field / a `*(Written YYYY-MM-DD, trigger)*` note).
- **Newest wins on conflict, and a present timestamp is never ignored** — a dated item outranks
  undated prose; between two dated statements the later wins. Do not discount a timestamp because
  the conflicting text is longer or sits in an authority doc.
- **No retroactive refactor** — undated items stand; they simply lose to a dated item that
  contradicts them.
- **Resolve at delivery** — reconcile conflicts among the set being delivered, not continuously.

## Priority, difficulty & version goal

- **Priority** (importance, ascending): `F · C · B · A · S · SSS`. `F` = deferred/parked
  (`parked:true`); `SSS` = do immediately. Re-rate against the **current goal**.
- **Difficulty** (1–5, non-linear ~3–4× per step): 1 ≈ 5 min, 2 ≈ 20 min, 3 ≈ 1 h, 4 ≈ 3 h,
  5 ≈ 12 h+ (**break a 5 down** rather than take it whole).
- **Version goal** (added 2026-07-08) — every new item names a `version_goal`: the minor it's
  aimed at (`"v0.0.9"`), the next one (`"v0.1.0"`), or `"post-v0.1.0"` for anything beyond the
  prototype's remaining arc. Read `ROADMAP.md` for the live version sequence and each minor's
  theme before naming one — an item's `version_goal` should match a theme it actually serves, not
  just the nearest open minor; `backlog.json`'s `version_goal` fields win on any conflict with
  ROADMAP prose. It is a goal, not a promise: re-rate it at re-sequencing the same
  way priority gets re-rated, rather than treating the first guess as fixed. Legacy items authored
  before this policy are not backfilled.

## Depth verbs — how far to take an item

Name *how far* an instruction carries an item, so effort is never ambiguous and a stop is a
finish, not an abandonment:

- **Design** — design depth only. Settle a `design-owed` item's open questions into the item and
  flip it to `designed`, then **stop**. No tasks, no code, no authority-doc edit.
- **Promote** — planning depth only. Break a `designed` item into REFINED.md tasks and write its
  `requirements.json` group, then **stop**.
- **Implement (don't commit)** — code depth. Promote, then write and build the code, hold the
  commit (a *code-complete* group).
- **Deliver** — full depth. The entire lifecycle below, through to a committed, verified change.

When no verb is given, assume **Deliver** for a single named `designed` item (ask if the scope is
large); for a `design-owed` item, **Design** is the implied first step.

## The Delivery lifecycle (Full mode)

0. **Item-spanning requirement (gate — if the item changes `src/`).** Before decomposing, write
   one **item-spanning requirement** in the item's `req/requirements.json` group — usually a
   `visual` check (a `scripts/verify/<feature>.lua`), else the equivalent end-to-end `headless`
   check. It is the acceptance gate, written first so decomposition is shaped by it. Doc-only
   items are exempt.
1. **Create tasks** — promote the item into REFINED.md: smallest independently-buildable steps
   (foundation first), each scoped to its exact files, with dependencies and parallelisation
   marked.
2. **Create requirements** — append the item's requirement group to `req/requirements.json`
   (the data + permanent history) per `req/REQUIREMENTS.md`.
3. **Plan parallelisation** — build the **collision map**. This is two layers: the **file**
   layer (which files each task writes), which *informs how to split work into focused
   sub-agents* (see below) rather than gating it; and the **symbol** layer — each task's
   `provides:` and `consumes:` lines (see § The symbol-level dependency contract). The symbol
   layer is the checklist the review barrier verifies.
4. **Complete tasks** — implement, review, and verify each against its requirements. Tasks that
   prove blocked or out of scope are **cancelled** (intent returned to the backlog), not left in
   flight.

   **A `visual` requirement on an interactive surface is not complete on a scripted capture
   alone (2026-08-19).** `verifier-visual`'s PNG capture proves a surface *renders*; it does not
   prove a button on it is *reachable*. BL-449 (stance surface) shipped clean on both a compile
   and a 36/36 harness, and was still unusable — its Stance column's presses rendered past the
   panel's edge, caught only when someone finally opened the live app. Any requirement whose
   surface has a press, a popup, or user input needs one live open-and-click pass — `request_access`
   the built exe, screenshot it, click the thing — before it flips `complete`. A static capture is
   still the right check for pure rendering (tints, layout, glyphs); it is not a substitute for
   proving an affordance works.
4a. **Review barrier (before any fresh full compile).** Once slices have merged into the
   integrating tree, run the **`verifier-review`** skill over the integrated diff *before* spending
   the integrating build. It is a static, no-compile pass (the cheapest verification tier) whose
   one job is to catch the cross-item integration class — a consumer built against a symbol no
   producer landed — early, when the fix is cheap. Its verdict gates the compile: resolve any
   Critical before building. This is a filter, not a substitute: always still compile. In a
   single-item Light change with no cross-item surface, skip it.

   `verifier-review` is the *author-side, pre-compile* filter. It is **not** the deciding
   review — the author must not sign off on its own work. The independent, cold, adversarial
   review (the `code-reviewer` agent / the PR-open CI action) and the human accept gate are the
   PR-level pipeline; see [`REVIEW_AUTOMATION.md`](REVIEW_AUTOMATION.md).
5. **Commit** — once all tasks are terminal (complete or cancelled), one commit per item:
   ```
   <item title>

   Tasks: <N completed>, <N cancelled>
   Requirements: <N completed>, <N pending>, <N failed>
   ```
   Before committing, **close the loop and lint it.** The close-out is part of *this* commit, not a
   later housekeeping pass that never comes: flip the item's `requirements.json` group to `complete`,
   repoint its `authority_doc` off any process doc onto the subject's authority doc, and drain its
   `REFINED.md` tasks. Then run **`node tools/session/backlog_lint.js`** (0 = clean; non-zero = a
   contradiction between `backlog.json`, `requirements.json`, `BACKLOG.md` and `REFINED.md` — fix it
   before committing). The linter is the machine check that "landed the code, skipped the bookkeeping"
   can no longer pass silently; it exists because a 2026-07-04 currency audit found ~30 such loose
   ends.

   **Then shed the weight (added 2026-08-02).** A landed item's design prose, resolution and
   completion notes are frozen history: read when you look *at* that item, never when you look
   *across* the backlog. Run **`node tools/session/archive_designs.js`** to move them into
   `docs/development/archive/backlog-design-<quarter>.json`, leaving the item's `design` field as an
   `@`-pointer and an `archived` field beside it. This is the physical form of the authority
   time-slice CLAUDE.md already states — `backlog.json` owns the item while it is open, the subject's
   authority doc owns it once the work lands. Nothing is lost: `backlog_query.js --full` and
   `backlog_view.js` resolve the pointer transparently, `--restore` reverses the move, and
   `backlog_lint.js` fails on a pointer with no record behind it. The first run took `backlog.json`
   from 1.22 MB to 710 KB.

   The requirements ledger sheds weight the same way (added 2026-08-14, BL-421): run
   **`node tools/session/archive_requirements.js`** to move resolved groups' `rows` + `resolution`
   into `docs/development/archive/requirements-<quarter>.json`, leaving the index fields plus an
   `archived` pointer. `requirements_query.js` resolves archived groups transparently; amend a
   landed group's rows/resolution **in the cold file**. The first run took `requirements.json`
   from 556 KB to 124 KB.

   Three more close-out checks, all cheap, each catching a class of drift that used to pass:
   **`node tools/session/mirror_check.js`** re-renders every generated Markdown mirror and reports
   any that had drifted from its canonical JSON; **`node tools/session/devlog_index.js`**
   regenerates [`DEVLOG_INDEX.md`](DEVLOG_INDEX.md) so the session you just wrote is findable
   without loading the log; and **`node tools/session/story_check.js`** confirms every user story
   still traces to something runnable (`USER_STORIES.md` asked for this to sit here and it never
   did — wired in 2026-08-04).

   **Two obligations that are not scripts.** Commit with the **`scoped-commit`** skill, not a broad
   `git add` — this tree routinely carries another session's work in progress, and a broad add
   sweeps it into your commit under your message. And if the change moved **generated world
   content**, re-bless the visual suite **in the same commit** (BL-259; `DEVELOPMENT_PRACTICES.md`
   § World-content staleness) — a golden re-blessed a commit later is a golden nobody reviewed
   against the change that moved it.

   Finally, **drain `NEEDS_REVIEW.json`**: entries are written as decisions are taken (Rule 0c),
   so close-out is where you resolve the ones this delivery answered and re-render the mirror. The
   file is **transient** (Ben's ruling, 2026-08-14) — once an entry's answer has landed in code, an
   authority doc, or a backlog item, prune the entry rather than leaving it resolved-but-present. A
   resolved entry surviving in the file is noise the same way an unwritten decision is silence; a
   decision that never got written at all is indistinguishable from one Ben made.

   **Commit-format scope (recorded 2026-07-31).** The `Tasks:`/`Requirements:` trailer applies to
   **Full-mode item deliveries**; Light, measurement, and filing commits carry a plain descriptive
   body — matching actual practice (e.g. the BL-233 terrain-combat measurement and the
   BL-233/234 filing commits).

### The symbol-level dependency contract (`provides` / `consumes`)

`waits_on` / `blocked_on` in `backlog.json` capture *item-level* ordering — coarse, and enough to
sequence items across sessions. They do **not** capture the *symbol-level* contract between
concurrent tasks inside one batch. Close it at the task layer, where the failure lives:

- When promoting items into `REFINED.md`, annotate each task with **`provides:`** (the public
  symbols it adds or changes — struct fields, enum values, function signatures, recipe/Lua keys)
  and **`consumes:`** (the symbols it depends on another task to provide).
- Every `consumes` entry **must** name a `provides` entry on another task in the batch (or an
  already-landed symbol). An unmatched `consumes` is a sequencing bug caught *before* code is
  written, not after a failed compile.
- This is the checklist the **review barrier (step 4a)** verifies against the integrated diff. Keep
  it lightweight — symbol names and owning task, not signatures-in-full; the point is to make the
  cross-slice contract explicit enough to check.

It stays transient in `REFINED.md` (it concerns *this* batch's parallel tasks). Promote a genuinely
item-level prerequisite to `waits_on` as usual; `provides`/`consumes` is the finer, within-batch layer.

### Batch Delivery (barrier semantics)

Delivering more than one item in a work block runs the steps as **barriers across the whole set**
(breadth-first): every item clears step *N* before any starts *N+1*. Step 4 is the load-bearing
barrier — **all** tasks across **all** items reach a terminal state before any item is committed;
a blocked task is *cancelled*, not held. **Step 4a (the review barrier) then runs once over the
whole integrated set** — a single `verifier-review` pass across all merged slices, not one per
item, because the failure it hunts is *cross*-slice. Commits are still one-per-item, made
back-to-back once the review barrier closes clean.

A Batch Delivery also runs a **documentation-coverage discipline**: up front, determine per item
whether the docs already record what it will produce; doc-changing items get a per-item **doc**
collision map (fan out across disjoint docs), a transient `> ⟳` "what changed" note per changed
doc (removed once reviewed), a standing `S`-tier review item per changed doc, and — when the batch
made non-trivial design calls — a closing **design-direction Q&A** recorded in the DEVLOG.

### Progress markers

A Delivery expected to span many steps (any Batch Delivery) emits a **coarse `%` progress line**
in the response text between tool calls — estimated once up front after the collision map, in
multiples of 5, weighting verification/golden steps heavily. A naive pacing guess riding existing
output; never walked backwards. Skip it for a trivial single-item Delivery.

## Sub-agents & worktrees (the parallelism model)

**Worktrees are the primary isolation mechanism.** Concurrent sub-agents each run in their own
git worktree (`isolation: "worktree"`), so two agents editing the same file no longer corrupt each
other — worktree isolation makes overlap safe. This replaces the old hard rule that sub-agents
must have disjoint file write-sets.

Consequences:

- **The collision map is now a *splitting heuristic*, not a gate.** Use it to carve the work into
  **focused, meaningful agents** — each owning a coherent vertical slice — rather than to prove
  disjointness. Where natural slices *are* disjoint, all the better; where they share a file,
  worktrees absorb it and the main session merges.
- **Keep each agent on a tight block of code, reading minimal documentation.** A cold agent costs
  context to spin up; give it a sharp brief (the task text, its files, its public signature
  target) and the *one or two* docs it actually needs — not the whole design corpus. A narrow,
  well-scoped agent is the unit that pays back.
- **Agents build and commit on their own worktree branch**; the **main session merges** them in
  dependency order, runs the integrating build, and verifies. Assume nothing about an agent's
  self-reported success — verify retroactively after merge. The retroactive pass is where the
  quietly-wrong surfaces, while it is still one slice deep.
- **An agent blocks on its own long-running builds and harness runs (2026-08-19).** It does not
  yield control mid-wait and report back "still waiting" — it checks the process or output file
  directly and keeps going. And once an agent reaches a stated decision (a hypothesis refuted, a
  stale threshold dropped), it stops: it does not keep re-investigating a question its own report
  already answered. Both cost supervisory round-trips for no new information; a brief that says
  so up front is cheaper than redirecting it mid-run.
- **Sub-agents must use the Bash tool with a heredoc for all git commits.** The project's
  `settings.json` allow rule is `Bash(git commit*)` — the PowerShell tool is not covered and
  will stall on a permission prompt. Every sub-agent prompt should include: *"Use the Bash tool
  with a heredoc for git commits; PowerShell is blocked by the allow rule."*
- **Hotspot/integration wiring stays in the main session.** The seam every slice eventually
  touches is integrated centrally, after the agent slices land.
- **The recurring roles are saved as agent definitions** (BL-497, 2026-08-20):
  `.claude/agents/economy-dev.md`, `ui-dev.md`, `generation-dev.md` (implementer slices) and
  `code-reviewer.md` (the cold review pass). Each definition carries its role's reading list,
  invariants and commit discipline, so a spawn needs only the task-specific brief — prefer
  spawning a saved role over re-improvising its prompt. Directory-scoped instruction files
  (`src/world/CLAUDE.md`, `src/ui/CLAUDE.md`) load automatically for any session working in
  those directories and carry the always-on invariants; keep role definitions and scoped
  files as **pointers into the authority docs**, never copies of their prose.
- **Fan-out is a discretionary call made *after* the tasks and collision map.** Fan out when
  there is a substantial wave of slice-able work a cold agent can execute from its brief alone;
  stay in the main session when the win is marginal (a short serial chain, co-evolving
  interfaces). State the call and its reason rather than asking permission each time.

### Parallel worktree coherence (keeping N sessions consistent)

Worktrees isolate the *working copy* for free; coherence is about the things they **don't** isolate
— a shared base, shared append-only registries, and a single integration point. Three rules keep N
concurrent sessions consistent (learned the hard way — two ID collisions in one session, 2026-07-06;
memory `backlog-id-collision-hazard`):

1. **Branch off a *current* base.** Every worktree starts from an up-to-date `main`. If the last
   local session was ≥ 3 days ago, `git fetch origin` and integrate first (CLAUDE.md § Session
   start) — origin can move ahead independently (other machines / cloud sessions) even though we
   push only at releases. A stale base is the root cause of most merge pain.
2. **Allocate shared IDs before authoring, don't mint off the local max.** The append-only
   registries — `backlog.json`, `req/requirements.json`, `user_stories.json`, `DEVLOG.md`,
   `REFINED.md` — are the collision hotspots because every session appends to the tail. Before
   filing a backlog item run **`node tools/session/next_id.js [count]`**: it scans *all* branches
   (local + remote) for the true max and returns the next safe id (or a reserved range for a
   session that will file several). Two sessions each grabbing "next = BL-114" off their own stale
   file is the exact failure this prevents. `backlog_lint.js` is the **backstop** — it now FAILs on
   a duplicate id at the merge point.
3. **Integrate in one place, in order, and verify retroactively.** Worktree agents commit on their
   own branch and **never touch `main`**. The **integrator** (the main-tree session) merges branches
   in dependency order, rebasing each onto current `main`; **renumbers any residual collision** —
   keep the earlier/shared item, renumber the later one **and its cross-references, code comments,
   and harness labels**; then runs the **integrating build + headless harnesses on the merged tree**
   (an agent's branch passing in isolation ≠ passing after integration) before fast-forwarding
   `main`. Push only at a release.

## Proportionality & session boundaries

- **Proportionality.** The lifecycle is a guideline proportional to the work, not a fixed
  ceremony. A quick low-risk high-value change skips the tasks/requirements steps (Rule 0, Light).
- **Pausing is legitimate.** Driving a group to complete in one block is the default, not a
  mandate. When ending early serves the work, **pause** the group (REFINED.md true to state, build
  green or breakage noted, a short "resume here" line) rather than force completion or cancel it.
  Left clean, it resumes without archaeology. A paused group is a deliberate scoping choice,
  distinct from a *cancelled* one.
