# Project Io — User Manual

> **Read the status marks.** This manual is written during a refocus (NR-177, 2026-08-12), so it
> deliberately describes two things at once: the game as it is **designed** and the game as it is
> **built**. Every section carries a mark saying which. Nothing here should read as more finished
> than it is.
>
> | Mark | Meaning |
> |---|---|
> | **`[BUILT]`** | Shipped and playable today. You can do this in the current build. |
> | **`[BUILT · REFRAMING]`** | The mechanism ships and works. Its *ancient-era framing* is settled but not yet applied, so names and flavour will change; behaviour will not. |
> | **`[DESIGNED]`** | Design is settled and filed. No code yet. |
> | **`[OWED]`** | Named as necessary, not yet settled. Here so its absence is visible. |

---

## Preface

Project Io is a single-player grand strategy game about running a **mercenary company** in a
procedurally generated ancient world. You are not a nation and you do not rule anyone. You raise
force, you sell its use, and you spend the proceeds on being able to sell more of it next time.

This manual is for two readers. If you are **playing**, sections 1 through 4 are yours — what the
game is, how to start, and every control. If you are **building** it, section 5 maps each system to
the document that owns it, so you can find the authority rather than guessing from the code.

The project is solo-developed in C++ with Lua scripting, and is in prototype. It began as a
near-future space game; that arc is parked on the `era/space` branch and tag `space-arc-parked`,
intended to return as downloadable content. The reasoning behind the refocus is recorded in
`docs/development/NEEDS_REVIEW.json` § NR-177 and is not repeated here.

---

## 1. What the game is about

### 1.1 The premise — `[DESIGNED]`

The world is generated from first principles: a star, a planet, its chemistry and weather, its
continents, its rivers, its peoples, and the centuries of settlement and war that leave it looking
the way it looks when you arrive. Nothing is hand-authored. Every nation, city and person has a name
coined from its own culture's sound system, and none of them is drawn from Earth.

You arrive at the campaign epoch as a company of armed professionals with a base, a unit, and a
balance. The world's polities want things from each other that they cannot take alone. That gap is
your business.

### 1.2 The player — `[DESIGNED]`

You are a **mercenary company**. Three things follow from that, and they are the design's spine:

- **You do not legislate.** Law, tax and policy are conditions you operate inside, not levers you
  pull. When a law makes your work harder, your options are to route around it, price it in, or
  take work somewhere else.
- **You do not manufacture.** Equipment is bought from private companies who are counterparties,
  not subsidiaries. They quote a price and a lead time, and they can refuse.
- **You are paid for outcomes.** Not for effort, and not for time served.

### 1.3 The loop — `[DESIGNED]`

> **Be contracted → field force → be paid → reinvest.**

A client offers a contract: a fact about the world it will pay to have become true, by a deadline.
Take the river province before the season ends. Hold the pass. Break the siege.

**The contract names the outcome and the fee. It never names the force.** Deciding whether the fee
covers what the objective actually costs is the whole skill of the game, and it is why the loop is a
strategy rather than a mission list. Underbid and you will send too little, lose the fight, and lose
the fee and your standing with it.

### 1.4 The three pillars — `[BUILT · REFRAMING]`

Everything in the game is meant to justify itself by feeding one of three things.

**Trade.** A live economy of extraction, refining, transport and price. Markets clear every tick
against real supply and demand; prices diverge by geography and converge along trade routes. You
buy equipment and provisions here, and you sell what your holdings produce.

**Conflict.** Force is expensive, slow to move, and decisive when it arrives. Battles resolve
against unit class, formation doctrine, terrain, supply state and season. Supply lines are the real
constraint: reach far enough and your army arrives too thin to win.

**The world that connects them.** Distance, terrain and politics are not scenery. A mountain costs
roughly twice a plain to cross, a cold border makes a convoy dearer, and a frontier ground down
over decades eventually gives.

### 1.5 Winning, and losing — `[BUILT · REFRAMING]`

**There is no win screen and no lose screen.** The game does not end; your position gets better or
worse.

Losing is progressive and it is a spiral rather than an event. Lose a contract and your standing
falls. With lower standing the fees on offer fall too, and fewer clients will deal with you at all.
With smaller fees you can afford less force, which loses you the next contract. A company can die
of this without ever losing a battle it could not have won.

The escape is the same as the trap, run backwards: stop taking work you cannot deliver, hold what
you have, and rebuild. That is a real strategic choice with a real cost in time.

### 1.6 Time — `[BUILT]`

The simulation advances in **ticks**. Between ticks the game runs in real time, so you can look
around, read ledgers and give orders without the world moving under you.

You control the clock directly: pause with `Space`, and set the speed tier with `1` through `5`.
Pausing is a legitimate way to play; nothing is hidden behind reaction speed.

---

## 2. Getting started

### 2.1 Starting a campaign — `[BUILT]`

From the main menu, **New Game** opens the New World wizard. You set *preferences*, not parameters:
how abundant the world's resources are, and the lean of the generation. The world is then generated
in visible stages — planetology, continents, tiles, rivers, population, history, nations, roads,
corporations — each stage a deterministic consequence of the one above it.

The **seed** is an 8-digit hex value. You can roll it, type one in, or copy the current one. The
same seed and the same preferences always produce the same world; this is guaranteed, not
incidental.

Generation then runs **4000 years of history**, from 4000 BCE to the campaign epoch of **0 CE**,
before handing you a world with settled peoples, borders drawn by centuries of war, and cities
where the ground actually rewarded building one. The homeworld is a **312 × 145 hex grid — 45,240
tiles**, of which roughly 18,000 are land.

### 2.2 The screen — `[BUILT · REFRAMING]`

| Region | What it is |
|---|---|
| **Canvas** | The main view: the planet's hex tile surface. Where you look, select and build. |
| **Icon rail** (left) | One glyph per ledger. Clicking a glyph opens its ledger; clicking it again closes it. |
| **Inset** (bottom right) | A fixed close-up of your base. *(`[DESIGNED]` — today this still shows the parked circumplanetary view; see BL-378.)* |
| **Lens bar** (under the inset) | Map overlays. One click applies a lens; clicking the active one clears it. |
| **Selection band** (bottom) | Detail on whatever is currently selected, and the actions available on it. |
| **Time column** (top right) | The clock, the pause control and the speed tiers. |
| **Comms dock** (bottom) | Messages and channels. |

### 2.3 Your first ten minutes — `[DESIGNED]`

1. **Find your base.** You begin with one military base and one unit on your home province.
2. **Look at the ground.** Cycle lenses with `L`. The Resource and Population lenses tell you what
   is near you and who lives there.
3. **Read the market.** Open the Market Ledger from the icon rail. Prices are local; what is cheap
   at home may be worth carrying.
4. **Take stock of the money.** The Budget Ledger itemises income, expenditure, maintenance, wages
   and interest. Wages are the line that will kill you if you ignore it.
5. **Take a contract you can afford to deliver.** Not the largest fee on offer. The largest fee you
   can service with the force you already have.

---

## 3. Instructions — the controls

All bindings below are transcribed from the action dictionary (`docs/ai/ACTIONS.json`), which is the
authoritative source and is kept in step with the code. Where this manual and the dictionary
disagree, the dictionary is right.

### 3.1 Camera and view — `[BUILT]`

| Action | Control |
|---|---|
| Pan | Arrow keys, or hold the **middle mouse button** and drag |
| Zoom | Mouse wheel, or `=` / `+` to zoom in and `-` to zoom out |
| Zoom (precise) | Drag the zoom slider |
| Pause / resume | `Space`, or click the pause button |
| Speed tier | `1` – `5`, or click a speed-tier button |

### 3.2 Selection — `[BUILT]`

| Action | Control |
|---|---|
| Select | Single left-click a tile or marker |
| Inspect without selecting | Hover the pointer over it |
| Deselect | Single left-click empty space |
| Close the Selection band | Click the `x` in its header row |
| Go to the selected thing | Click the `>` in its header row |
| Drill into a metric | Click a chart; `<` goes back, `x` closes |
| Page between metrics | The left/right arrows beside the metric title |

**The click model is consistent everywhere: single-click selects, double-click navigates.**

### 3.3 Lenses — `[BUILT]`

Lenses tint the map to show one quantity. Eight are on the bar — **Corporation, Country, Resource,
Market, Population, Opportunity, Production, Continent** — and four more are reachable only by
cycling: **Scarcity, Industry, Reach, Supply-routes**.

| Action | Control |
|---|---|
| Apply a lens | Click its glyph in the lens bar |
| Cycle forward / back | `L` / `Shift+L` |
| Clear the lens | `0`, or click the currently-active glyph |
| Choose the good (Resource / Market) | The combo in the lens legend |

### 3.4 Buildings and production — `[BUILT]`

| Action | Control |
|---|---|
| Build | Select a tile → **Construct Buildings** → pick a candidate |
| Build a road | Same panel, below the building candidates: Track / Road / Highway |
| Manage a building | Select it, then the gear icon on the Selection band |
| Change recipe | The production-method combo on the band |
| Set workforce | The workforce slider (0–200% of nominal) |
| Auto-solve workforce | The **Auto** checkbox — the slider is disabled while it is on |
| Idle / resume | **Idle** on a running building; **Resume** on an idled one |
| Demolish | **Demolish**, then confirm |

### 3.5 Markets — `[BUILT]`

| Action | Control |
|---|---|
| Open the Market Ledger | The market glyph on the icon rail |
| Switch view | The **Prices** / **Sell Orders** tabs |
| Place a sell order | Sell Orders tab → pick a resource → set price and quantity |
| Remove a sell order | **Remove** beside the listed order |

### 3.6 Ledgers — `[BUILT]`

Each ledger has one glyph on the left icon rail. Clicking an open ledger's glyph closes it.

| Ledger | Glyph | What it answers |
|---|---|---|
| Budget | Ledger | Where the money went, itemised |
| Market | Market | What things cost, and where |
| Construction | Industry | What is being built, and what you own |
| Corporation | Corporation | Your own position |
| Corporations table | Diplomacy | Everyone else's, as far as you can see it |
| Economy | Workforce | Population and labour |
| History | History | What has happened in the world |

Tax and wage tiers are set in the Budget Ledger with `-` / `+`, or by clicking a Roman numeral
`I`–`V`.

### 3.7 System — `[BUILT]`

| Key | Opens |
|---|---|
| `F1` | Controls cheat-sheet overlay |
| `F9` | Tech tree |
| `F10` | Options — resolution, UI scale, fullscreen, VSync |
| `F11` | Frame-budget HUD |
| `F12` | Screen capture |
| `Esc` | System menu; backs out of an armed confirmation first |

---

## 4. Systems overview

Each entry says what the system does, its status, and the document that owns it. The owning document
is the authority; this manual is a summary and defers to it.

### 4.1 World generation — `[BUILT]`

A deterministic chain, each stage consuming the one above. **Planetology** settles atmosphere,
chemistry and the biosphere's history. **Continents** derives tectonic plates and hands a height
bias downward. **Tiles** generates the hex surface on two axes — composition and landform — and
seeds deposits. **Rivers**, **population centres**, the **institutional history ladder**,
**nations**, **roads** and **corporations** follow in order.

*Authority: `docs/generation/GENERATION_STRATEGY.md`, and the per-stage documents it maps.*

### 4.2 Tiles and terrain — `[BUILT]`

Every tile carries a **composition** (what it is made of) and a **landform** (what shape it is).
Composition drives colour and deposits; landform drives movement cost, hazard, habitability and
mineral richness. Landform is the reason a mountain costs about twice a plain to cross — in money
*and* in days.

The homeworld is **312 × 145 = 45,240 tiles**, about 18,200 of them land, and a tile is roughly
128 km across.

*Authority: `docs/economy/TILES.md`.*

### 4.3 Resources — `[BUILT · REFRAMING]`

Three tiers — raw, refined, product — across four value tracks: industrial, ambient, habitability,
and **mercantile** (endemic goods whose value comes from geography rather than utility, so their
price is a function of distance from where they grow).

Roughly two thirds of the current list is already pre-industrial: grain, fodder, salt, charcoal,
iron blooms, bullion, stone, timber, clay, peat, furs, spices. The space-sourced entries are parked
with the space arc.

*Authority: `docs/economy/RESOURCES.md`.*

### 4.4 Production — `[BUILT · REFRAMING]`

Buildings occupy tiles, consume inputs, and emit outputs on a recipe. Placement is validated against
terrain, deposits, slot rules and **logistical reach** — you cannot build at unbounded distance from
a supply anchor, and build time depends on where you build, not only what.

*Authority: `docs/economy/PRODUCTION.md`.*

### 4.5 Markets and prices — `[BUILT]`

Each market is independent. Base prices come from global rarity; local prices respond to local
supply and demand, smoothed rather than jumping. A matched price-time **order book** sits on top,
so the player and the AI place standing orders through the same mechanism.

*Authority: `docs/economy/MARKETS.md`.*

### 4.6 Money — `[BUILT]`

Five flows resolve every tick: income, expenditure, maintenance, wages and interest. Debt accrues
interest. Balances may go negative. Wages and maintenance are separate lines because they fail
differently.

*Authority: `docs/economy/FINANCE.md`.*

### 4.7 Logistics — `[BUILT]`

Roads come in three tiers and are generated as a per-nation lattice; cities are free logistics hubs.
Convoys route goods between markets at a cost weighted by terrain, and distance costs both money
and **time**.

A tile is about **128 km across** — derived from the planet's generated mass, not authored. A laden
caravan makes ~25 km/day overland and ~130 km/day by sea, and terrain multiplies that: a mountain
crossing costs about twice a plain. So a regional haul clears in a single quarter and a
cross-continental one takes several, which is what makes siting and sea access strategic rather
than cosmetic.

*Authority: `docs/economy/SUPPLY.md`.*

### 4.8 Discovery — `[BUILT]`

Two independent fogs. The **geographic fog** hides the tile map and deposits until you pay for a
survey. The **activity fog** is lit by your own trade routes and presence, tiering each body from
Unknown through Known, Stale and Visible. A place can be Known but unsurveyed.

Rival buildings are visible; their production and stockpiles are private. Markets are the public
intelligence channel.

*Authority: `docs/ui/DISCOVERY.md`.*

### 4.9 Units and combat — `[BUILT · REFRAMING]`

A battle resolver ships and is already calibrated for the ancient era: unit class matchup by
formation doctrine, modified by terrain, supply state and season, with era-keyed rosters running
from classical through industrial. Winter costs the attacker attrition and the defender readiness.

**What is missing is the campaign layer that commands it.** The resolver is exercised today by the
history simulation, not by play.

*Authority: `docs/ai/AI_OPPONENT.md` for the actor; the conflict spine is filed as BL-315.*

### 4.10 Contracts — `[DESIGNED]`

Your income. A contract is a predicate the client will pay to have become true by a deadline; you
choose the force. Three terminal states: **completed** (paid, standing up), **failed** (deposit
forfeit, standing down hard), and **cancelled** (deposit forfeit, standing down less).

Offers are derived from the history simulation's existing objective scorer — a polity that wants a
province and cannot take it alone offers it as work — so offers are deterministic and their fees
track a value the simulation actually computed.

*Filed as BL-377.*

### 4.11 Procurement — `[BUILT]`

How you buy. Request a quote from a named supplier, accept it to open a contract, and pay a deposit
plus a paced remainder. A supplier can **decline**, with a stated reason: no capacity, no input
access, an embargo, or your standing being too low. Refusal is not payable-through — you route
around it. Asking for a quote also tells you something real about the supplier's capacity.

*Authority: `docs/economy/MARKETS.md`. Landed as BL-350.*

### 4.12 Law and technology — `[BUILT]`

A law is a predicate plus an effect, evaluated against the world. One extraction levy is enacted,
enforced and visible in the budget; enacting nothing changes nothing, bit for bit. Technologies are
gated by the same predicate machinery and earned per corporation, not per world.

Under the refocus these are conditions you operate inside, not levers you pull.

*Authority: `docs/development/backlog.json` § BL-155, BL-156 until the surfaces land.*

### 4.13 The opponent — `[BUILT · REFRAMING]`

Rival companies run a deterministic scored-utility layer over the same command seam you use: they
build, demolish, survey, road, hire and place sell orders. There are no hidden AI exceptions — they
play by your rules or a defined subset of them.

Above that sits the direction: a small local model driving the game through its word interface. The
engine ships no HTTP client, no API key and no cloud dependency, and it never will.

*Authority: `docs/ai/AI_OPPONENT.md`.*

### 4.14 The pre-history — `[BUILT]`

Before you arrive, the world runs — **4000 years of it, from 4000 BCE to the 0 CE campaign epoch.**
Polities settle, campaign, invest and consolidate; cohesion falls when ground is lost, so defeat
compounds; supply decays with distance and with the breadth of what you hold, so empires stall on
arithmetic rather than on a designer's cap.

**The clock steps.** Decisions come every 100 years in deep prehistory, then 50, 20, 10, 5, and
finally every year approaching the epoch — so the recent centuries that shaped your starting world
are simulated in detail while the distant ones are painted in broad strokes. That is 136 decision
rounds rather than 4000, which is both cheaper and finer where it matters. Population, meanwhile,
grows every real year regardless; only the *decisions* are stepped.

A deep prehistory dominated by settling new ground rather than by war is the intended shape, not an
accident.

This is a generator, not a play layer. It produces the world you start in, and its architecture —
not its constants — is what graduates into the campaign.

*Authority: `docs/lore/HISTORY.md`; the simulation is `src/world/history_sim.{hpp,cpp}`.*

---

## 5. What is owed

Listed so their absence is visible rather than discovered.

- **`[OWED]` The campaign conflict layer.** Combat resolves, but nothing in play commands it. Until
  it does, the mercenary loop cannot be played end to end. *(BL-315.)*
- **`[OWED]` A product name.** Io is a moon of Jupiter, which the ancient game is not about.
- **`[OWED]` The ancient building roster at tile grain.** The pre-history's works table exists at
  province grain and does not transfer directly.
- **`[OWED]` Save and load.** No save format ships yet.
- **`[OWED]` Sentiment-based diplomacy.** Designed, with no sentiment layer in code.

---

*This manual is generated by hand and is not authoritative over the documents it cites. When it
disagrees with `docs/ai/ACTIONS.json` about a control, or with a system's owning document about
behaviour, those win.*
