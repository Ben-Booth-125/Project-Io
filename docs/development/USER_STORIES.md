# Project Io — User Stories

**What the player is trying to do**, decomposed by *player intent* — the axis that neither the
design docs (which decompose by system/surface) nor the backlog (which decomposes by feature)
gives you.

This is the **readable mirror** of [`user_stories.json`](user_stories.json), which is
**canonical**. On any disagreement the JSON wins — exactly as `backlog.json` wins over
`BACKLOG.md`. Author story data in the JSON; this file renders it for skimming.

---

## Why this exists — the second route from docs to code

The project already has two spines from design to code. This is a third, on an orthogonal axis:

| Spine | Decomposes by | Answers |
|---|---|---|
| Design docs (`CONCEPT`, `SYSTEMS`, `ui/*`) | system / surface | "what *is* this thing?" |
| Backlog (`backlog.json`) | feature | "what do we *build* next?" |
| **User stories (this)** | **player intent** | **"does the player's *goal* have a home?"** |

A story is only a *route* if it carries **traces**. Each story links **upward** to the systems and
pillars that give it meaning, and **downward** to the UI surface, backlog item, and requirement
that realise and verify it. Those links turn intent into a **coverage check**:

- a story **no surface serves** is a **gap** to fill;
- a shipped feature **no story needs** is **scope** to justify.

### A pillar of testing

Each story's **requirement** trace is a list of `brief` slugs in
[`req/requirements.json`](req/requirements.json), whose rows carry executable verification
(`visual`/golden, `headless`, `code`). That makes every story a **walkable acceptance path**: the
story is the player-goal framing, the linked briefs are how you *prove the goal still holds* after a
change. Walk it by hand ([BL-098](backlog.json)) or re-run its briefs (the `verifier-*` skills). A
`served` story whose briefs go red is a **regression against a player goal** — caught on the intent
axis, not just the feature axis.

### Relationship to BL-098

**BL-098** ("UX review from the perspective of user stories") is the **review activity** — walk
each story through the live UI and produce a prioritised friction list. **This catalogue is the
artifact that review consumes.** Build the stories here; run BL-098 *against* them. The `friction`
field on each story is where BL-098's findings accrete.

---

## The taxonomy — player-goal clusters

Every story belongs to one cluster. The taxonomy maps the **whole game's** intent so the axis is
complete; the *stories* stay within prototype scope. Post-prototype clusters are present but
deliberately unpopulated (io-standing-rules § Scope & sequence).

| Cluster | Intent | Scope |
|---|---|---|
| **Get my bearings** (`orient`) | Know who/where I am and what's next; move around the world and control its clock. | prototype |
| **Find opportunity** (`find`) | See where to grow — good ground, unmet demand — at a glance, without arming anything. | prototype |
| **Build** (`build`) | Pick a tile, understand what I can build and why-not, place it, and steer what it produces. | prototype |
| **Run my economy** (`operate`) | Know whether my assets make money, my runway, and why workforce is constrained. | prototype |
| **Expand & explore** (`expand`) | Survey a new body, judge whether it's worth it, and reach it. | prototype |
| **Move goods & read demand** (`trade`) | Govern how my stock is released, ship it between bodies, find demand and price. | prototype |
| **Read the competition** (`read-rivals`) | Infer rivals from public signals — buildings, market prints, activity — not their books. | prototype |
| **Contest territory** (`contest`) | Claim, defend, invade ground; make supply lines a target. | *post-prototype* |

## Coverage vocabulary

A story's **coverage** is its *served-state* — whether the goal has a working home today. This is a
**different axis** from the backlog's design-state (`✓ designed` / `~ design-owed`); a story can be
`gap` even where every relevant backlog item is `designed`, because the story asks whether the
*player's goal* is served, not whether the *work* is specced.

| Glyph | Coverage | Meaning |
|---|---|---|
| ● | `served` | The goal has a working home in the UI + code today. |
| ◐ | `partial` | Partly served; a friction point or a missing step remains. |
| ○ | `gap` | No home yet — a real hole in the software. |
| ◇ | `planned` | Deliberately outside current scope, or queued. |

---

## Coverage map

*Legacy-feature pass, 2026-07-05 — 12 stories across all seven prototype clusters, every story
requirement-linked.* This baselines the shipped player-facing features. Secondary stories and the
coverage-audit tool are the next passes.

| Cluster | Stories |
|---|---|
| `orient` | ◐ US-001 who/where · ● US-004 zoom ladder · ● US-005 time control |
| `find` | ● US-006 where to grow |
| `build` | ◐ US-002 place a building · ● US-007 recipe & workforce |
| `operate` | ● US-003 profitability & runway · ● US-012 workforce constraint |
| `expand` | ● US-011 survey a body |
| `trade` | ● US-008 sell orders & floor · ● US-009 move goods between bodies |
| `read-rivals` | ● US-010 read a rival |
| `contest` | *(post-prototype — unpopulated)* |

---

## Testing modes — walk it, or run it

Each story carries a `testing.mode`. The split follows importance and feasibility: the strategic /
UX-judgment goals are **walked by hand**; the mechanically-verifiable ones are **automated** against
their linked requirement briefs. `tools/session/story_check.js` enforces that any `auto`/`mixed`
story actually has something to run (a brief with golden/visual/headless verification).

| Mode | Stories | How you verify it |
|---|---|---|
| **manual** | US-001 orient · US-004 navigation feel · US-010 read-a-rival | Walk the flow live (the BL-098 activity). The value is a judgment — "what's my move?", the click-model *feel*, inferring a rival — that no golden captures. |
| **mixed** | US-002 build · US-009 convoys · US-011 survey | Run the automatable core (placement legality, convoy logistics, survey determinism), then a short **manual read** on the legibility on top. |
| **auto** | US-003 · US-005 · US-006 · US-007 · US-008 · US-012 | Re-run the story's requirement briefs — mostly golden/visual renders + headless econ invariants. A red brief = a regression against that player goal. |

**Run the automated set:** `node tools/session/story_check.js --commands` prints the concrete
`ProjectIo --verify scripts/verify/*.lua` invocations per `auto`/`mixed` story (dispatch each via the
`verifier-visual` / `verifier-headless` skills). On this Windows box, heed the golden-mismatch caveat
(pre-v0.0.8 goldens are Linux-blessed — verify by eye, don't blanket re-bless).

**Relationship to the golden harness — not a second copy.** The goldens (`scripts/verify/*.lua`) and
headless harnesses (`tools/verify/*.cpp`) already form a per-*surface* / per-*feature* regression net.
Stories don't replace that net — they **index** it by player intent: a golden regresses one surface,
but a player goal spans several goldens + harnesses, plus a manual judgment ("does the why-not read
as one thing?") that no golden captures. For a pure `auto` story that maps to a single golden, the
story is just a friendly label — the distinctive value is in the `manual`/`mixed` stories and in the
coverage/triage `story_check.js` gives (which goldens back which goal, which are stale, which goals
have no check). If you never run story-scoped subsets or the coverage gate, the catalogue is overhead
over the raw golden sweep.

---

## Stories

Each entry: the story sentence, **Up** (pillar · systems), **Down** (surfaces · backlog · *reqs*),
the walkthrough, and known friction. Requirement slugs are `req/requirements.json` briefs.

### `orient` — Get my bearings

#### ◐ US-001 — Know who and where I am on load, and what I could do next
> *As the player, when the campaign loads I want to see who I am, where my home is, and what my
> first move could be, so that I get my bearings before touching anything.*
- **Up:** Trade · Budget, Infrastructure, Exploration
- **Down:** Profile + header ([LAYOUT](../ui/LAYOUT.md)); opening focus + home/HQ marker ([SOLAR](../ui/SOLAR.md)); Selection ([SELECTION](../ui/SELECTION.md)) · **BL-085/090/092** · *`player-presence`, `start-framing`, `corp-emblem-system`*
- **Walk:** load → view opens on home → Profile shows corp + emblem → "you are here" + HQ pip → select home → Selection frames state + actions.
- **Friction:** the *"what next"* half is weak — identity/where landed, but the opening view doesn't suggest a first move. Candidate BL-098 finding.

#### ● US-004 — Navigate the zoom ladder, from the system down to a tile and back
> *As the player, I want to drill from the solar view down to a single tile and climb back out, so
> that I can move fluidly between the strategic and the local without losing my place.*
- **Up:** Trade · Exploration, Infrastructure
- **Down:** Zoom ladder ([CANVASES](../ui/CANVASES.md)); ascend via minimap ([MINIMAP](../ui/MINIMAP.md)); "go to" ([SELECTION](../ui/SELECTION.md)) · **BL-031/059/032** · *`canvas-hit-testing`, `selectable-markers`, `selection-go-to-planetary`, `non-spatial-goto`, `lens-driven-selection`*
- **Walk:** click a body on Solar → descend → single-click selects, double-click navigates → Selection "go to" descends to a tile → click minimap to ascend.
- **Friction:** the single-selects / double-navigates click-model must stay consistent across all three canvases — the most-repeated player motion.

#### ● US-005 — Control the flow of time: pause, resume, change speed
> *As the player, I want to pause, resume, change the simulation speed, and read the clock, so that
> I can slow down for a decision and speed through the quiet stretches.*
- **Up:** Trade · Budget
- **Down:** Calendar + speed buttons + econ-tick progress ([TIME_CONTROLS](../ui/TIME_CONTROLS.md)); menu Pause/Resume ([MENU](../ui/MENU.md)) · **BL-008/064/070** · *`time-speed-curve`, `in-app-system-menu`*
- **Walk:** read calendar → press speed (I/II/III) or pause → progress bar shows next resolution → gear/Esc opens Pause/Resume without hotkeys.
- **Friction:** pause landed twice (hotkey + gear); confirm they agree and current speed is legible at a glance.

### `find` — Find opportunity

#### ● US-006 — See where to grow at a glance, without arming build mode
> *As the player, I want to read where opportunity is — good ground, dense industry, unmet demand —
> from the map alone, so that I can decide where to expand before committing to build mode.*
- **Up:** Trade · Environment, Resources, Budget
- **Down:** Opportunity / Industry / Scarcity / Production lenses ([LENSES](../ui/LENSES.md)); per-tile tint ([PLANETARY](../ui/PLANETARY.md)) · **BL-086/084/016** · *`opportunity-ambient`, `industry-lens`, `population-opportunity-lens`, `scarcity-lens-render`, `production-output-lens`*
- **Walk:** Opportunity lens → net-margin tint → Industry lens → substrate throughput field → Scarcity lens → where a good is absent → all without arming build.
- **Friction:** several lenses answer "where to grow" differently; the player may not know which answers which. Colour-scheme pass (BL-016) is the open refinement.

### `build` — Build

#### ◐ US-002 — Pick a tile, see what I can build, its cost and why-not, and build it
> *As the player, I want to select a tile and see what I can build there, what it costs in budget
> and materials, and why I can't build the things I can't, so that I can commit in a few clicks.*
- **Up:** Trade · Infrastructure, Budget, Resources, Environment
- **Down:** build flow + "Insufficient funds." ([SELECTION](../ui/SELECTION.md)); "build here" ([MENU](../ui/MENU.md)); suitability tint ([PLANETARY](../ui/PLANETARY.md)) · **BL-071/044/095/082/043/010** · *`build-front-door`, `construction-pricing`, `placement-suitability`, `building-placement-rules`*
- **Walk:** select tile → read without arming → buildable list with budget + material cost → unaffordable/unplaceable show a reason → arm → affinity tiles tint → confirm → placed + debited.
- **Friction:** BL-082 occlusion fixed in v0.0.9 (re-walk). "Why not here" (BL-071) and the material/market-stock gate (BL-095) should read as *one* explanation, not two refusals.

#### ● US-007 — Set a building's recipe and workforce target
> *As the player, I want to choose what an existing building produces and how much workforce it
> draws, so that I can steer production to what the market needs rather than accept a fixed output.*
- **Up:** Trade · Resources, Workforce, Infrastructure
- **Down:** recipe + workforce controls ([SELECTION](../ui/SELECTION.md)); recipe tables + workforce scalar ([PRODUCTION](../economy/PRODUCTION.md)) · **BL-046/041** · *`building-management`, `habitability-workforce`, `layer4-ui-groundwork`*
- **Walk:** select building → current recipe + target read → change recipe → honoured next tick → raise target → output scales, capped by habitability workforce → shortfall shown as the reason.
- **Friction:** habitability cap and the target interact; player needs "you asked for X, the body allows Y" in one place. Ties to US-012.

### `operate` — Run my economy

#### ● US-003 — Know whether a building makes money and what my runway is
> *As the player, I want to know whether a building is profitable and whether I can afford my next
> moves, without hunting across ledgers, so that I can act before bankruptcy rather than discover it.*
- **Up:** Trade · Budget, Resources, Workforce
- **Down:** Economy panel — itemised income/expenditure + runway + trends ([LAYOUT](../ui/LAYOUT.md)); per-building profitability ([SELECTION](../ui/SELECTION.md)) · **BL-072/073/074/081/066** · *`budget-breakdown`, `debt-interest`, `per-building-profitability`, `economy-ledger-legibility`, `economy-panel`*
- **Walk:** open Economy panel → income/expenditure itemised, debt interest distinct, runway projected → select building → profitability reads → trend plots show trajectory → judge "carrying its weight?" and "how long until underwater?".
- **Friction:** watch that the profitability figure and the runway agree in sign/framing so they don't read as contradictory.

#### ● US-012 — Understand why my workforce is constrained
> *As the player, I want to see why workforce is limited on a body and where population concentrates,
> so that I can make siting decisions by reason rather than by guesswork.*
- **Up:** Trade · Workforce, Environment
- **Down:** habitability → workforce ([POPULATION](../economy/POPULATION.md)); Population lens ([LENSES](../ui/LENSES.md)); reasoning on selection ([SELECTION](../ui/SELECTION.md)) · **BL-069/083/041** · *`population-legibility`, `pop-centre-markers`, `population-lens-render`, `habitability-workforce`*
- **Walk:** Population lens → habitability tint → population centres draw as tiered settlements → select building/tile → habitability→workforce chain explains the cap → site where labour can reach.
- **Friction:** same cap as US-007's target; the two surfaces must tell one consistent story about the ceiling.

### `expand` — Expand & explore

#### ● US-011 — Survey a new body and judge whether it's worth it
> *As the player, I want to preview a survey's cost and time, dispatch it, and watch a body reveal
> region by region, so that I can decide whether an unknown body is worth investing in before I commit.*
- **Up:** Trade · Exploration, Budget
- **Down:** Survey system ([DISCOVERY](../ui/DISCOVERY.md)); survey badge ([SOLAR](../ui/SOLAR.md)); cost/ETA + progress ([SELECTION](../ui/SELECTION.md)); masked regions ([PLANETARY](../ui/PLANETARY.md)) · **BL-067/099** · *`survey-system`*
- **Walk:** select unsurveyed body → see only type/orbit/grid size → Survey section previews cost + ETA (scale with size + distance) → dispatch → credits debited upfront → transit reveals nothing, then scan reveals region by region → richness bands resolve.
- **Friction:** cost is charged upfront before any reveal; the preview must make the gamble legible ("pay N now, first tiles in T ticks").

### `trade` — Move goods & read demand

#### ● US-008 — Govern how my stock is released: standing sell orders with a floor
> *As the player, I want to place standing sell orders with a floor price and bias which
> counterparties I match, so that I control how my goods reach market instead of dumping at the
> cleared price.*
- **Up:** Trade · Budget, Resources
- **Down:** Trade pillar mechanic ([SYSTEMS](../SYSTEMS.md)); Market ledger order book + auto-sell health ([LAYOUT](../ui/LAYOUT.md)) · **BL-037/035/025** · *`player-sell-orders`, `order-book`, `warm-start-surface`, `multi-market-dashboard`*
- **Walk:** set a standing sell order (qty + floor) for a (body, resource) → honoured at clearing ahead of anonymous auto-sell → preferential purchasing biases the buyer's match → Market ledger shows whether stock is moving.
- **Friction:** three release mechanisms (orders / auto-sell / order book); the ledger must show which one moved a unit, or US-003's profitability reads are hard to trust.

#### ● US-009 — Move goods between bodies and see where they flow
> *As the player, I want to ship goods from where they're cheap to where they're dear and see the
> lanes that carry them, so that I can profit from spatial price differences net of logistics.*
- **Up:** Trade · Supply, Infrastructure, Budget
- **Down:** Convoy layer ([SUPPLY](../economy/SUPPLY.md)); Market/Scarcity lens ([LENSES](../ui/LENSES.md)); persistent routes ([DISCOVERY](../ui/DISCOVERY.md)) · **BL-039/038/088/036** · *`supply-layer`, `trade-routes`, `market-centre-seeding`, `market-boundary-lens`*
- **Walk:** terrestrial shortfall auto-dispatches a convoy from the cheapest source (cargo leaves at dispatch) → advances per tick, credits destination on arrival → prices converge net of cost → space launches always player-directed → routes record the lanes.
- **Friction:** logistics cost makes distant arbitrage marginal; the player needs per-unit landed cost visible at decision time.

### `read-rivals` — Read the competition

#### ● US-010 — Read what a rival is doing from public signals
> *As the player, I want to infer a competitor's activity from what's public — buildings, market
> prints, commercial activity — without seeing their books, so that I can respond to rivals I can't
> directly observe.*
- **Up:** Trade · Exploration, Diplomacy
- **Down:** visibility rule + activity fog ([DISCOVERY](../ui/DISCOVERY.md)); public-facts-only selection ([SELECTION](../ui/SELECTION.md)); hover activity line ([ICONS](../ui/ICONS.md)) · **BL-068/089/100/060/099** · *`visibility-model`, `commercial-fog`, `activity-hover-line`, `hover-card`*
- **Walk:** see rival buildings on-canvas → select one → public facts only, no production/stockpile → body carries Unknown/Known/Stale/Visible tier lit by own routes → hover known+ body → market pulse → reason from public market signals.
- **Friction:** activity fog and geographic survey fog are independent axes (Known-but-unsurveyed is valid); the UI must not conflate them. Proximity-glimpse (BL-099) is the open follow-on.

---

## Adding a story

1. Append an object to `stories` in [`user_stories.json`](user_stories.json) with a `US-XXX` id,
   its `cluster`, the `As the player…` sentence, a `coverage` + matching `glyph`, and `traces`.
   Fill at least one surface, one backlog id, **and one requirement brief** — a story with no
   downward trace isn't a route, and one with no requirement isn't testable.
2. Set `written` to today's date (absolute), and a `testing` mode (`manual` / `auto` / `mixed`).
3. Render it here under its cluster, and run `story_check` (below) — it will reject a dead trace or
   an `auto`/`mixed` story with nothing to run.

## Coverage audit — `story_check.js`

`node tools/session/story_check.js` machine-checks the catalogue against `backlog.json` and
`requirements.json` (companion to `backlog_lint.js`; zero-dependency, exit 1 on FAIL). It flags:
- **dead traces** — a `traces.backlog` id or `requirements` brief slug that doesn't exist;
- **unroutable / untestable** — a non-`planned` story missing surfaces, backlog, or requirement links;
- **dishonest mode** — an `auto`/`mixed` story whose linked briefs carry no runnable verification;
- **reverse coverage (info)** — shipped player-facing backlog items in **no** story (candidates to cover — currently BL-015/020/055/056/062/065/080/091/093).

`--commands` prints the concrete `ProjectIo --verify …` invocations for the `auto`/`mixed` set. Wire
it into the Delivery close step next to `backlog_lint`.
