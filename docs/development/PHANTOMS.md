# Project Io — Phantom Features

> **Status: scan output, 2026-08-22. Not authority, and not work.** Written from Ben's ask:
> *"we have a lot of game design that hasn't been given proper space and attention in
> documentation — do a scan for these phantom features."* Every row below is a pointer to a
> **design session**, not a task. Nothing here should be built off this file; the session's
> output lands in the subject's authority doc, and this file is then stale by design.

A **phantom** is game design that is real to the project but has no place a reader would find it.
It comes in five kinds, and the first is not a documentation problem at all.

| Kind | The failure |
|---|---|
| **0 — Filed as delivered, never built** | The backlog says landed. No code exists. |
| **1 — Runs in code, no doc owns it** | The behaviour ships. The reasoning lives in a header comment. |
| **2 — Settled design, never left the backlog** | Design is finished and sits in a 700 KB JSON nobody reads whole. |
| **3 — Design conversation, never promoted** | A doc exists, declares itself non-authority, and is used as one. |
| **4 — Flagged as unowned by the session that made it** | `novel-work` was raised. Nobody ruled. |

---

## The measure

Four numbers frame the rest of this file.

- **1.53 MB** of authority docs (`docs/**/*.md`, excluding `docs/development/`).
- **514 KB** of design prose held **only** in `backlog.json`, across **161 open items**.
- **45** open items carry more than 4 KB of design each — chapters, not notes.
- **198** open `NEEDS_REVIEW.json` entries, **6** of them `novel-work` and unruled.

So roughly **one third as much design prose sits in the backlog as in every authority doc
combined**. None of it is where a reader looks when asking "how does this work".

---

## Class 0 — the one that is not a doc problem

### BL-377 (mercenary contract seam) was marked `complete` and was never built — *resolved*

This is the game's **income loop** — *be contracted, field force, be paid*. Under the ancient arc
it is the product's spine, and MANUAL.md § 4.10 correctly marks it `[DESIGNED]`.

The backlog disagrees, and the backlog is wrong. Evidence:

- Its `files` name `src/world/contracts.hpp`, `src/world/contracts.cpp` and `scripts/contracts.lua`.
  **None of the three has ever existed** — `git log --all --diff-filter=A` finds no creation commit.
- **Zero** references to `BL-377` anywhere in `src/`.
- `corp_verb` carries no contract verb. The design called for three appended verbs.
- `world.hpp` holds `procurement_quotes` and `procurement_contracts` only — BL-350's **buy** side.

It was closed by commit `bb4f612`, *"Close BL-377 and BL-378 — two more landed items still listed
as open."* BL-378 (minimap base render) genuinely had landed; BL-377 was swept up with it.

Meanwhile ROADMAP § The near sequence still names Sprint 16 as *"BL-377 (contract seam) playable
end-to-end"*, and SPRINTS.md § Sprint 16 is **open**. The item is closed inside the sprint that
would deliver it.

**REOPENED 2026-08-22** (Ben's call, same day). Status is `designed`, version goal **v0.1.15**,
priority **A**. Its design was pulled back out of the cold store intact, and the item now carries a
dated note recording the false close. Sprint 16 owes it.

> **Same check, the rest of the roster.** Fifteen `complete` items name source paths that resolve
> to nothing. Thirteen are stale renames and cost nothing — `src/app.cpp` (now `src/core/app.cpp`),
> `src/ui/chart.cpp` (now `charts.cpp`), `render_ux_questions.js` (now `render_question_log.js`).
> The exception is **`src/world/serialisation.cpp`**, named by BL-263 (spontaneous markets),
> BL-448 (corp stance) and BL-517 (retain heightmap). That file has never existed — no save
> path ships, which NR-349 and NR-399 both record. **Save/load is itself a phantom** (BL-107).

---

## Class 1 — a system runs, and no doc owns it

Each of these ships and is exercised. In each, the *design reasoning* lives in a header comment,
which is the one place the CLAUDE.md doc map cannot route a reader to.

| Subject | Runs in | Design currently lives | Proposed owner |
|---|---|---|---|
| ~~**The nation as an actor**~~ | `law.cpp`, `nation_component::treasury`, tariffs in `market_clearing` | — | **`docs/politics/NATIONS.md` — written 2026-08-22** |
| ~~**Law**~~ | `law.cpp`, `condition_set.cpp` | — | same doc, § The enforcement seam |
| ~~**Stance and standing**~~ | `stance.cpp`, `standing.cpp` | — | **`docs/politics/RELATIONS.md` — written 2026-08-22** |
| **Conditions & modifiers** | `condition_set.cpp`, `modifier_set.hpp`, read by laws, techs, gates, units | Header comment; SYSTEMS.md § Conditions (one subsection) | **`docs/SYSTEMS.md`, promoted** |
| **Province as a grain** | `province.cpp` (936 lines), unit position, battles, rendering | Split across TILE_GENERATION, PLANETARY, SELECTION | **new `docs/generation/PROVINCES.md`** |
| **Interdiction** | BL-458 (supply lines cannot be cut) | Nowhere — the word appears in **no** authority doc | **`docs/economy/SUPPLY.md`** |
| **The star map** | `star_map.cpp` — authored, seed-free, cross-campaign | Header comment only | **`docs/ui/MINIMAP.md`** |

**The nation actor led this list, and is now done.** `NATION_GENERATION.md` line 7 said outright:
*"Nation system design is an open item. This document covers only the generation strategy."* Since
then a nation gained a treasury, became a law's author, and was granted deterministic behaviour
(standing rules, 2026-08-18) — while the only doc that could own it went on disclaiming it.

**`docs/politics/NATIONS.md` now owns it** (2026-08-22), written as capture rather than design: what
runs, what is absent, and six open questions left for Ben rather than answered by a session. The
generation doc's disclaimer is struck through and re-pointed, and SYSTEMS.md § Policy, MARKETS.md
§ Tariffs and FINANCE.md § Levies each carry a pointer at the half they do *not* own.

**All six were settled the same day**, along with a system Ben raised alongside them — a two-way
channel between corporations and nations, lobbying forward and the budget in reverse. That is the
argument for capture-first in one line: **the questions were answerable once they were asked
precisely**, and they had gone unasked for as long as no doc owned the subject.

**Why MARKETS.md is overloaded.** It is 657 lines and carries three systems that are not markets —
procurement (53 lines), the contract worth model (47), and tariffs (38). The income loop of the
game is a subsection of the market doc.

---

## Class 2 — settled design that never left the backlog

Heaviest first. Each is finished thinking with no reader-facing home.

| Item | KB | Status | Subject | Where it should land |
|---|---|---|---|---|
| **BL-524 (syndicate tier)** + 6 children | 18.2 | designed | Ownership split from identity | **new `docs/economy/OWNERSHIP.md`** |
| **BL-094 (player-identity pivot)** | 18.7 | parked | The militia | `docs/CONCEPT.md` |
| **BL-262 (scoring system)** | 15.5 | parked | How the player is measured | `docs/CONCEPT.md` |
| **BL-155 (law/policy surface)** | 14.8 | designed | What a law *is* | Class 1's nations doc |
| **BL-156 (tech system)** | 9.0 | designed | Gate = quest = tech, one object | **new `docs/research/TECH.md`** |
| **BL-464 (logistic points)** | 9.0 | design-owed | Movement economy | `docs/economy/SUPPLY.md` |
| **BL-315 (conflict spine)** | 7.8 | designed | Campaign command of force | `docs/military/MILITARY.md` |
| **BL-472 (unit formations)** | 6.8 | designed | Merge, split, garrison, scout | same |

**The syndicate tier is the sharpest case.** Seven items and a settled vocabulary — *corporation*
= operating firm, *syndicate* = ownership tier — and its only home outside `backlog.json` is the
**CLAUDE.md preamble** plus a GLOSSARY entry. A framing that reshapes player identity is being
carried by the file that is supposed to *point at* authority docs, not be one.

---

## Class 3 — design conversations never promoted

**3,076 lines** across six docs, each of which opens by declaring itself *"research scaffolding —
the design conversation's home, not authority."*

| Doc | Lines | Names it as authority |
|---|---|---|
| `docs/research/ERA1_TECH_LANDSCAPE.md` | 882 | 1 open item |
| `docs/research/ANCIENT_TECH_LADDER.md` | 739 | 2 items |
| `docs/ai/STRATEGIES.md` | 475 | — |
| `docs/lore/COLLAPSE.md` | 439 | **19 open items** |
| `docs/ai/LANGUAGE_POLICY_FEASIBILITY.md` | 271 | — |
| `docs/research/TECH_EFFECTS.md` | 270 | — |

**COLLAPSE.md is the contradiction worth fixing.** Nineteen open items name a self-declared
non-authority as their authority doc. Either the doc graduates, or those nineteen point somewhere
else — but the current state means a reader following the pointer lands on a page that tells them
it settles nothing.

The convention is sound and works elsewhere: scaffolding is promoted **when the work lands**. What
is missing is a promotion that ever happened. None of the six has graduated.

---

## Class 4 — flagged unowned by the session that made it

The project already has a phantom detector: `kind: "novel-work"` in `NEEDS_REVIEW.json`. Eight
have been raised, two are resolved, and six are open and unruled. One adjacent `question`
(NR-454) belongs with them and is listed too.

- **NR-483 — population-centre density.** No doc owns *"how many centres should a world have, and
  what is that number answerable to?"* The divisor is at once a generation constant, a road
  constant (`n < 2`) and the body's whole labour supply. Three docs have a claim; none states the
  constraint.
- **NR-458 — the mark vocabulary (BL-520, tile texturing).** Thirteen new mark kinds, deliberately
  *not* icons, plus a third always-on render channel. ICONS.md owns the glyph namespace and says
  nothing about them. Two visual vocabularies now exist where there was one.
- **NR-453 — the verify API (BL-521, click injection).** Built with no design to build against;
  six Lua binding names chosen by the agent. Other checks will be written against that seam.
- **NR-454 (a `question`) — a UI target registry.** `verify.click('nav.market')` needs every clickable surface to
  publish `{name, rect}`. Nobody owns the registry, and it may duplicate ACTIONS.json.
- **NR-398 — the nation treasury.** See Class 1.
- **NR-392 — how a worktree agent builds a harness.** Process, not game design; listed for
  completeness.
- **NR-513 — this file.** A list of unowned things is itself unowned. Raised by this scan, with
  the recommendation that it dissolve into per-doc absent-lists once each subject has an owner.

**NR-495 is resolved and worth reading beside these.** It flagged the syndicate tier as the fourth
player-identity framing in seven weeks. Ben ruled: the design stands, the **build** waits for the
mercenary vertical slice (v0.1.15) to be cut. That is the right shape for most of this file.

---

## Class 5 — the map is stale

Not design gaps, but they are why the gaps persist: a reader cannot reach what the map does not name.

- **41 files under `docs/` are not named in CLAUDE.md** (42 counting this one), including `docs/MANUAL.md`, all three
  `docs/research/` docs, `docs/lore/COLLAPSE.md`, `docs/lore/CREEDS.md`,
  `docs/economy/SUPPLY.md`, `docs/economy/SPACE_ASSETS.md`, six `docs/ui/` surface docs and the
  eight `docs/ui/ledgers/` pages.
- **`docs/CONCEPT.md` is two framings behind.** CLAUDE.md sends every new session there first for
  *"what the game is"*. It carries the 2026-08-10 militia correction, but not the 2026-08-12
  two-arcs split — so it still describes solar scope, orbital logistics and planetary isolation as
  live, when the live arc is ancient and 0 CE.
- **`docs/MANUAL.md` § 5 drifts the other way.** It lists the campaign conflict layer as `[OWED]`;
  BL-467/468/469 landed the engagement trigger, battle card and dispatch stream on 2026-08-21.
- **`tools/doc_weight.js` reports three named paths that do not resolve** — `docs/lore/`,
  `docs/generation/`, `docs/ai/`. The reading order names directories where it means files.

> **The pattern to copy is already in the repo.** MILITARY.md § *What is absent, and known to be*
> names its own holes, strikes them through as they land, and dates each. Every doc that owns a
> partially-built system should carry that section. It is the cheapest phantom prevention there is.

---

## Proposed design sessions

Ordered by what unblocks the most. Each is one session; each ends with prose in a named doc.

| # | Session | Settles | Lands in |
|---|---|---|---|
| **1** | **Reopen the income loop** | BL-377's status; what Sprint 16 actually owes | `backlog.json`, ROADMAP |
| ~~**2**~~ | ~~**The nation as an actor**~~ | **Done 2026-08-22** — doc written, then all six questions settled by Ben the same day; seven items filed (BL-537–BL-543) | `docs/politics/NATIONS.md` ✓ |
| **3** | **Contracts leave the market doc** | The mercenary sell side as its own subject | new `docs/economy/CONTRACTS.md` |
| **4** | **Ownership** | Syndicate tier, control gate, dividends, portfolio | new `docs/economy/OWNERSHIP.md` |
| ~~**5**~~ | ~~**Relations**~~ | **Done 2026-08-22** — doc written; found a fourth quantity (embargo) and NR-520, three designs converging on one shape | `docs/politics/RELATIONS.md` ✓ |
| **6** | **The predicate substrate** | `condition_set` / `modifier_set` as one owned vocabulary | SYSTEMS.md, promoted |
| **7** | **Province as a grain** | The unit of position, rendering, battle and ceiling | new `docs/generation/PROVINCES.md` |
| **8** | **Research as a currency** | BL-478's debit model — NR-387 says this needs design, not an implementer | new `docs/research/TECH.md` |
| **9** | **The novel-work backlog** | NR-483, NR-458, NR-453, NR-454 ruled in one sitting | their named docs |
| **10** | **Re-voice CONCEPT.md** | The live arc, in the doc every session reads first | `docs/CONCEPT.md` |

Sessions 2 and 3 are the two that change what the game *is* soonest. Session 1 is ten minutes and
should not wait for a session at all.

---

## Checked, and not phantoms

Recorded so the next scan does not re-derive them.

- **Doc propagation on landing is working.** Of 337 `complete` items, only **15** show no trace in
  their authority doc, and those are hygiene and build items — not game design.
- **The `authority_doc` pointer is well maintained.** Exactly two dangle:
  `docs/ui/ACCESSIBILITY.md` (BL-063) and `docs/development/AI_OPPONENT.md` (BL-253, a path typo).
- **Supply, discovery, markets, production, lenses, selection and the action dictionary** all have
  a doc that owns them, sized to the system.
- **The code comments are good.** `stance.hpp`, `standing.hpp`, `condition_set.hpp` and
  `star_map.hpp` each carry real design reasoning. The problem is location, not quality.
