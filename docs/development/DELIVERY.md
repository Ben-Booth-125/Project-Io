# Project Io — Delivery Method

How work flows from intent to a committed, verified change. This is the long-form authority for
the backlog model and the **Delivery** lifecycle; `CLAUDE.md` carries the condensed reference and
`.claude/rules/io-standing-rules.md` the always-on summary.

The three artefacts:

| Artefact | Role |
|---|---|
| [`backlog.json`](backlog.json) | **Canonical backlog — metadata index.** Every item's status, priority, difficulty, sequencing, file scope. Queryable; the source of truth for metadata. |
| [`BACKLOG.md`](BACKLOG.md) | **Design bodies + legend.** The rich per-item design prose, keyed by id, plus the status/priority legend. Metadata mirrors `backlog.json` (JSON wins on conflict). |
| [`REFINED.md`](REFINED.md) | **Active worklist (transient).** A `designed` item is *promoted* here into file-scoped, dependency-marked tasks when we act on it. Cleared as tasks complete. |

*(History: `backlog.json` + `BACKLOG.md` replace the former `OPENS.md`; `REFINED.md` replaces
`TASKS.md`. The lifecycle verb "Publish" was renamed "Deliver" — "Cut" stays reserved for cutting a
release, see `DEVELOPMENT_PRACTICES.md` § Cutting a release.)*

### Where design prose lives — the markdown/JSON policy (2026-06-16)

Item **metadata** always lives in `backlog.json`. Item **design prose** follows one rule:

- **New items are JSON-native.** Author the design / detail / rationale in the item's `design`
  field (a markdown-formatted string). A new item gets **no `BACKLOG.md` body**.
- **Legacy items keep their markdown body.** The 54 items migrated from OPENS keep their prose in
  `BACKLOG.md`, keyed by title; their `design` field is the sentinel `"@BACKLOG.md"`.
- **On promotion, the markdown body is deleted.** When an item is promoted into `REFINED.md`, its
  `BACKLOG.md` body is removed — a refined item must not have a markdown body. So `BACKLOG.md`
  only ever **drains** (on promotion/landing), never grows, and is **deleted once empty**.
- **Rich tables** (the rare item whose design needs them) set `design` to a pointer to the
  authority doc rather than fight JSON-string formatting.

The effect is a self-converging single-source model: metadata in JSON, prose either in the JSON
`design` field (new) or a draining set of `BACKLOG.md` bodies (legacy).

## The one idea

Separate thinking from doing, and **size the effort to the job** (Rule 0). For a tiny change this
is thirty seconds in your head; for a big one, write it down. Do not apply the heavy version to
small jobs.

## Two modes (Rule 0)

Every non-trivial task states its mode:

- **Light** — a one-line fix, an obvious cleanup, a doc tweak. Make it, check it, say what you
  did. No tasks, no requirements, no ceremony. **This is the default.**
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

## Design state — the two open states

Every backlog item is **not yet implemented** (implemented work leaves the backlog). What varies
is whether its **design is settled**. The `status` field (in `backlog.json`) is authoritative; the
glyph in `BACKLOG.md` mirrors it 1:1 for human skimming:

- **`designed` (glyph `✓`)** — design settled; **promote-ready**. What remains is execution. (May
  still be *blocked* on a dependency existing — a sequencing fact, not a design gap; see
  `blocked_on`.)
- **`design-owed` (glyph `~`)** — design still owed. The item names the problem and open
  questions but **must be designed before it is promoted**. Doing the design *is* the next action.

Status is orthogonal to **priority** (importance) and **difficulty** (size).

**Design happens in the item, not mid-flight.** Pausing to settle a `design-owed` item beats
redesigning during a Delivery (redesign in place is costly). **The backlog is the design authority
while a design is open** — the settled design lives in the item's `BACKLOG.md` body, which is by
definition more current than any authority doc on that subject. Authority **time-slices**: the
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

## Priority & difficulty

- **Priority** (importance, ascending): `F · C · B · A · S · SSS`. `F` = deferred/parked
  (`parked:true`); `SSS` = do immediately. Re-rate against the **current goal**.
- **Difficulty** (1–5, non-linear ~3–4× per step): 1 ≈ 5 min, 2 ≈ 20 min, 3 ≈ 1 h, 4 ≈ 3 h,
  5 ≈ 12 h+ (**break a 5 down** rather than take it whole).

## Depth verbs — how far to take an item

Name *how far* an instruction carries an item, so effort is never ambiguous:

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
3. **Plan parallelisation** — build the **file collision map** (which files each task writes).
   This now *informs how to split work into focused sub-agents* (see below) rather than gating
   it.
4. **Complete tasks** — implement, review, and verify each against its requirements. Tasks that
   prove blocked or out of scope are **cancelled** (intent returned to the backlog), not left in
   flight.
5. **Commit** — once all tasks are terminal (complete or cancelled), one commit per item:
   ```
   <item title>

   Tasks: <N completed>, <N cancelled>
   Requirements: <N completed>, <N pending>, <N failed>
   ```

### Batch Delivery (barrier semantics)

Delivering more than one item in a work block runs the steps as **barriers across the whole set**
(breadth-first): every item clears step *N* before any starts *N+1*. Step 4 is the load-bearing
barrier — **all** tasks across **all** items reach a terminal state before any item is committed;
a blocked task is *cancelled*, not held. Commits are still one-per-item, made back-to-back once
the step-4 barrier closes.

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
  self-reported success — verify retroactively after merge.
- **Hotspot/integration wiring stays in the main session.** The seam every slice eventually
  touches is integrated centrally, after the agent slices land.
- **Fan-out is a discretionary call made *after* the tasks and collision map.** Fan out when
  there is a substantial wave of slice-able work a cold agent can execute from its brief alone;
  stay in the main session when the win is marginal (a short serial chain, co-evolving
  interfaces). State the call and its reason rather than asking permission each time.

## Proportionality & session boundaries

- **Proportionality.** The lifecycle is a guideline proportional to the work, not a fixed
  ceremony. A quick low-risk high-value change skips the tasks/requirements steps (Rule 0, Light).
- **Pausing is legitimate.** Driving a group to complete in one block is the default, not a
  mandate. When ending early serves the work, **pause** the group (REFINED.md true to state, build
  green or breakage noted, a short "resume here" line) rather than force completion or cancel it.
  A paused group is a deliberate scoping choice, distinct from a *cancelled* one.
