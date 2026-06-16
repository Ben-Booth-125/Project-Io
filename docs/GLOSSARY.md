# Project Io — Glossary

**Asset**
Any owned entity with economic or military value: a building, installation, unit, or vehicle. A corporation persists as long as it holds at least one asset.

**Body**
Any discrete celestial object in the simulation — planet, moon, asteroid, or station. Bodies are the primary unit of territorial control and the locations where resources are extracted, colonies are built, and conflict occurs.

**Corporation**
The player's controlling entity. Unlike nation-states, a corporation begins with no territorial claim or military force and must justify every asset through economic or strategic return. The corporation persists as long as it holds any asset.

**Tick**
The fixed period between economy updates. Supply, demand, and market prices resolve at the end of each Tick. Real-time play continues within a Tick; the boundary is a hard economic checkpoint, not a pause.

**Faction**
A named actor with a distinct starting position, ideology, and resource profile. Factions include the player's corporation and all AI-controlled entities. Each faction maintains sentiment values toward every other known faction.

**Market**
A pooled exchange with a physical boundary through which any faction can buy or sell goods with the delivery cost depending on logistical cost. Price is set by rarity on a body, then modulated by local supply and demand. All tiles belong to a market.

**Sentiment**
A numeric value representing one faction's disposition toward another. Sentiment is shaped by trade history, territorial conflict, and ideological alignment, and governs diplomatic options and the likelihood of conflict.

**Tile**
The smallest subdivision of land on a body. Each tile has a fixed, procedurally generated profile covering resource deposits, terrain type, hazard level, and habitability. Tile properties determine local extraction yields, infrastructure construction costs, and combat conditions. Tiles are the granular unit of environment data, with many properties that are fixed upon generation.

**Building**
A surface installation placed on a tile. Buildings are either **extraction** (harvesting raw materials from tile deposits) or **processing** (consuming inputs and producing outputs via a recipe) or **infrastructure** (affecting logistical or economic capacity). Each building holds a `building_component` and a `stockpile_component`.

**Canvas**
A screen that the player navigates to inform decision making and understand what's happening on a body or in space.

**Active (state)**
The navigation **anchor** — the body or tile the canvas zoom ladder is currently framed around. Persists until the player navigates. Distinct from Selection: selecting an entity does not change what is Active. Backed by `ui_state.active_body` / `active_tile`. See `docs/ui/SELECTION.md`.

**Focus (state)**
The entity **under the pointer** in the current frame — the transient hover target that drives the tooltip / hover card. Distinct from both Active and Selection. See `docs/ui/SELECTION.md`.

**Selection (state)**
The entity the player **single-clicked to inspect**. Persists until another entity is selected (or the selection is cleared). Drives the **Selection info element** and its 'go to' target, and does not move the canvas. Backed by `ui_state.selected_entity`. See `docs/ui/SELECTION.md`.

**Item (backlog item)** *(formerly "Brief")*
A unit of **described intent** in the backlog (`docs/development/backlog.json` — the metadata index — with its design prose in `docs/development/BACKLOG.md`): a problem to solve, a feature to build, or a doc to write, captured with enough context and file pointers to plan later — but deliberately carrying *no* implementation breakdown. An item is the design-level view of a single piece of work; **promoting** it into `docs/development/REFINED.md` decomposes it into a **task group** (one item ↔ one group of tasks). Distinct from a **task**, which is one file-scoped, individually-buildable step within that group. Every item carries a **design state** (see below): `designed` (✓) or `design-owed` (~). See `docs/development/DELIVERY.md`.

**Design state (item)**
Whether an **item**'s design is settled. The authoritative value is the `status` field in `backlog.json`; the glyph in `BACKLOG.md` mirrors it 1:1. **`designed` (✓) — not implemented:** the design is settled and the item is **promote-ready** (it may still be *blocked* on a dependency existing — a sequencing fact, not a design gap). **`design-owed` (~) — not implemented:** design is still owed and must be settled *before* the item is promoted. Orthogonal to priority and difficulty. Only `designed` items are promotable. See `docs/development/DELIVERY.md` (§ Design state).

**Open (adjective)**
Of an item: **not yet implemented**. An open item describes intent that has not landed in `src/` — it lives in the backlog. An open item may additionally be **owed** to design (its explanation debt is unpaid; see below), or already settled. See `docs/development/DELIVERY.md`.

**Owed (adjective)**
Of an item: one that **owes the design documentation an explanation** — the design behind it has not yet been written into the subject's authority doc. An owed item carries the unpaid explanation debt itself (the backlog is the most up-to-date design source while the debt stands). Contrast **settled**. See `docs/development/DELIVERY.md` (§ Design state).

**Settled (adjective)**
Of an item: one that has **repaid its explanation debt** — the design has been written into the authority doc, so the documentation now carries the explanation rather than the item. Contrast **owed**. See `docs/development/DELIVERY.md` (§ Design state).

**Deliver / Delivery** *(formerly "Publish")*
The lifecycle for taking a `designed` item through to a committed, verified change — item-spanning requirement, task creation, requirements, parallelisation planning, completion, commit. *("Cut" is reserved for cutting a release; see below.)* See `docs/development/DELIVERY.md`.

**Batch Delivery** *(formerly "Batch Publish")*
Delivering **more than one item in a single work block**, run breadth-first under
**barrier semantics** — every item clears each Delivery step before *any* item begins the
next (see `docs/development/DELIVERY.md` § Batch Delivery).

A Batch Delivery **usually executes already-designed items — it does not normally include
design work.** Design is settled into the items *beforehand*; **pausing to design is preferred
to redesigning in place**, which is costly. The design-direction Q&A (below) catches *incidental*
calls a batch made — it is not a substitute for up-front design. When a broken-up session forces
**rapid redesign** of work already attempted, **refactor the in-flight tasks back into items**
(intent returned to the backlog, as when cancelling a group) and make a clean **second attempt**
from the item, rather than redesigning mid-batch.

Beyond the single-item lifecycle, a Batch Delivery carries a **documentation-coverage
discipline** a lone Delivery does not:

- **Doc-coverage determination (first).** Before execution, determine for each item whether
  the design docs already record the implementation it will produce — or whether that
  implementation is a **direct consequence of already-documented behaviour**. Items that
  pass need no doc work; items that fail are flagged **doc-changing**.
- **Per-item documentation collision map.** For each doc-changing item, build a collision map
  of the **documents** it will change (the doc analogue of the source-file collision map).
  Disjoint-doc items are parallel-safe — **fan out sub-agents to write the doc changes**;
  items touching the same doc stay sequential.
- **Transient change note per doc.** Every changed doc carries a **minor transient
  "what was changed" note** — a dated breadcrumb (a **visible `> ⟳` blockquote**, the standard
  form) recording the edit, removed once the user has reviewed it.
- **Standing review reminders.** A Batch Delivery **always adds an `S`-tier item** (one per
  changed doc) under § Documentation, reminding the user to review the doc changes.
- **Design-direction Q&A (proportional).** When the batch made non-trivial or ambiguous design
  calls, it **closes by raising a Q&A** clarifying the design direction those calls surfaced,
  recorded with the session in the DEVLOG (see `docs/development/DEVELOPMENT_PRACTICES.md`
  § Design-direction Q&A). Skipped for a batch that surfaced nothing worth asking.

A Batch Delivery is a **strategy, not a code-sprint**: collision mapping (which file
write-sets may fan out vs. stay serial) and **session boundaries as checkpoints** (a large set is
*paused* at a clean, resumable boundary rather than forced to complete — REFINED.md § Pausing a task
group) are first-class parts of it, not afterthoughts. When a set spans more than one work block,
the standing slice plan is `docs/development/ROADMAP.md` § Near-term publish plan.

Distinct from a single-item **Delivery**, which carries no batch-level doc-coverage step. See
`docs/development/DELIVERY.md` and `CLAUDE.md` § Delivery pipeline.

**Complete (task state)**
A development task is **complete** only when every requirement it satisfies has been **reviewed**, **implemented**, and **tested** — completeness is measured against the requirements, not against "the code is written". A task that is implemented and builds but whose requirements have not all been reviewed and verification-run is *code-complete*, not complete. See `docs/development/REFINED.md` (§ Definition of "complete"), the requirement records in `docs/development/req/requirements.json`, and the policy in `docs/development/req/REQUIREMENTS.md`.

**Cancelled (task state)**
A task group that could not be driven to *complete* in one working block and is reverted rather than left half-tracked. Cancelling marks its requirements `failed`, rewrites its intent back into the backlog (merging into a related item where possible), and removes the task stubs from `REFINED.md`. It reverts *tracking*, not committed code — landed code stays in the tree; its intent returns to the backlog. Distinct from *paused* (a clean, resumable handoff that keeps the group in `REFINED.md`). See `docs/development/REFINED.md` (§ Cancelling a task group).

**Paused (task state)**
A task group deliberately stopped at a session boundary before completion, as a scoping choice rather than a failure. Unlike *cancelled*, a paused group stays in `REFINED.md` for the next session to resume; it is legitimate only when the stop is **clean and resumable** — `REFINED.md` is true to state (the in-flight task marked as the resume point), the build is green or the breakage is noted, and a one-line handoff records where to resume. A paused group is an explicit, recorded intermission, not a terminal state. See `docs/development/REFINED.md` (§ Pausing a task group) and `CLAUDE.md` (§ Delivery pipeline).

**Cut (release process)**
The ritual of finalising a version: merge the working branch into `main`, snapshot `src/` to a local `backups/vX.Y.Z/`, stamp `CHANGELOG.md` and the README "Latest release" line, commit, apply an annotated git tag (`vX.Y.Z`), and push with `--follow-tags`. The **tag** is the authoritative version-history record; the local backup is a convenience rollback point, not the record. "Cutting v0.0.5." See `docs/development/DEVELOPMENT_PRACTICES.md` (§ Cutting a release).

**Era**
A named phase in the game's industrial arc, defined by the accessible territory, available buildings, and dominant strategic challenge. The game begins in **Era 0** (Terrestrial) and transitions to **Era 1** (Early Space) by meeting an explicit gate condition. See `docs/economy/ERAS.md`.

**ISRU**
In-situ resource utilisation. The practice of producing resources — particularly propellant — from materials extracted at the operating location rather than shipped from the home planet. The primary logistical lever in Era 1.

**Ledger**
A view which provides a report on a sub-system to give detail for decision making.

**Recipe**
The configured input/output specification of a processing building. One building type may support multiple recipes; the active recipe is set per building. Recipe conversion rates are authored in Lua.

**Resource**
Any tradeable good in the economy. Resources occupy one of three tiers: raw materials (extracted from tile deposits), refined goods (produced by processing buildings), or products (manufactured from refined goods). See `docs/economy/RESOURCES.md`.

**Stockpile**
A per-entity store of resource quantities, held in a `stockpile_component`. Extraction and processing outputs accumulate in the building's stockpile each simulation step. At the economy tick boundary, all building stockpiles on a body aggregate into the body's market supply.

---

<!-- BEGIN GLOSSARY AUDIT (review only) — auto-managed block, safe to delete; the scheduled audit replaces everything between these markers -->

## Suggested improvements — audit 2026-06-16 (run 1 of 4, review only)

> ⟳ Auto-generated by the `glossary-term-audit` scheduled task. These are **flags for your review only** — no definitions above were changed and nothing was rippled into other docs. Action, edit, or delete freely; the next scheduled run overwrites this block.

### Terms whose name fights its definition (strongest candidates)

- **Focus (state)** — defined as "the entity under the pointer… the transient hover target." This directly contradicts the near-universal UI meaning of *focus* (the element receiving keyboard input), where hover and focus are explicitly distinct. A reader from any UI background will mis-map this. Consider **Hover (state)** — it matches the definition exactly and leaves "focus" free for its conventional meaning if keyboard focus is ever modelled.
- **Canvas** — definition is vague ("A screen that the player navigates…") and the name collides with a strong industry meaning: in HTML5, ImGui-adjacent UI, and design tooling, a *canvas* is a free-form drawing/layout surface, not a whole navigable screen. Since the project already uses ImGui, this is a live ambiguity. Consider **Screen** or **View** (note the overlap with *Ledger* below if you pick "View").
- **Ledger** — defined generically as "a view which provides a report on a sub-system." But *ledger* strongly connotes a financial transaction record (accounting, and now blockchain), and this is an economic game — readers will expect a money/transaction log specifically. Consider **Report**, **Panel**, or **Readout** for the generic case, reserving "ledger" for any actual transaction history.

### Collisions with industry-standard terms (lower severity)

- **Asset** — defined as an owned in-game entity (building, unit, vehicle). In a C++/Lua *game* codebase, "asset" overwhelmingly means an art/data file (texture, model, sound). The two meanings will collide in code, file names, and conversation. Worth a deliberate decision even if you keep it; an explicit disambiguation note would help.
- **Body** — "celestial body" is the right idiom, but "body" also names a rigid body in physics engines and a request/response body elsewhere. Minor; flag only if physics terminology enters the codebase.
- **Resource** — idiomatic for 4X tradeable goods, but also the generic programming sense (a loaded data resource). Mild overlap with **Asset**'s ambiguity above. Likely fine as-is.
- **Sentiment** — clear and distinctive, but the genre convention is *opinion*, *attitude*, or *relations* (e.g. Civ's "opinion"), and in tech "sentiment" reads as NLP sentiment analysis. Acceptable; noting for consistency only.
- **Brief** / **task group** — these diverge from standard agile vocabulary (*issue*/*story*/*epic*). The mapping is roughly Brief ≈ epic, task ≈ story. The project's system is internally consistent and intentional, so this is informational, not a recommendation to change — just a note that newcomers will translate.

### Conceptual overlaps worth a crisp boundary

- **Faction** vs **Corporation** — Faction is defined to *include* the player's corporation, yet Corporation is its own headline entry as "the player's controlling entity." The relationship (every corporation is a faction; a faction may be a corporation or something else) is implied but never stated. One sentence in each entry cross-referencing the other would remove the ambiguity.
- **Canvas** vs **Ledger** — both are "views/screens." The intended distinction (Canvas = navigable spatial screen; Ledger = sub-system report window) isn't recoverable from the definitions alone. Worth making explicit in both entries.

### Terms referenced in the docs but missing from the glossary

- **Nation** / **nation-state** — used in the *Corporation* entry ("Unlike nation-states…") and central to `docs/generation/NATION_GENERATION.md`, but undefined here. Since CLAUDE.md designates this glossary as canonical, a definition seems owed.
- **Selection info element** — referenced in the *Selection (state)* and *Ledger* contexts as a named UI element but never defined. Candidate entry (or a cross-reference to `docs/ui/SELECTION.md`).

### Minor / non-term notes

- **Market** — the definition reads awkwardly ("…through which any faction can buy or sell goods with the delivery cost depending on logistical cost"). Wording only; the term itself is fine.
- The glossary interleaves **game-world vocabulary** (Asset, Body, Tile, Market…) with **dev-process vocabulary** (Brief, Batch Publish, Cut, task states). No change requested here, but if a future pass ever restructures, splitting those two domains would aid navigation.

<!-- END GLOSSARY AUDIT (review only) -->
