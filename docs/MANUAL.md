# Project Io — User Manual

## Preface

Project Io is a single-player grand strategy game about running a **mercenary company** in a
procedurally generated ancient world. You are not a nation and you do not rule anyone. You raise
force, you sell its use, and you spend the proceeds on being able to sell more of it next time.

This manual is for two readers. If you are **playing**, sections 1 through 4 are yours — what the
game is, how to start, and every control. If you are **building** it, section 4 maps each system to
the document that owns it, so you can find the authority rather than guessing from the code.

The project is solo-developed in C++ with Lua scripting. It began as a near-future space game; that
arc is a separate product arc, on the `era/space` branch and tag `space-arc-parked`, intended to
return as downloadable content (Ben, 2026-08-12, NR-177; `docs/development/ROADMAP.md` § The two
arcs).

---

## 1. What the game is about

### 1.1 The premise

The world is generated from first principles: a star, a planet, its chemistry and weather, its
continents, its rivers, its peoples, and the centuries of settlement and war that leave it looking
the way it looks when you arrive. Nothing is hand-authored. Every nation, city and person has a name
coined from its own culture's sound system, and none of them is drawn from Earth.

You arrive at the campaign epoch as a company of armed professionals with a base, a unit, and a
balance. The world's polities want things from each other that they cannot take alone. That gap is
your business.

### 1.2 The player

You are a **mercenary company**. Three things follow from that, and they are the design's spine:

- **You do not legislate.** Law, tax and policy are conditions you operate inside, not levers you
  pull. When a law makes your work harder, your options are to route around it, price it in, or
  take work somewhere else.
- **You do not manufacture.** Equipment is bought from private companies who are counterparties,
  not subsidiaries. They quote a price and a lead time, and they can refuse.
- **You are paid for outcomes.** Not for effort, and not for time served.

### 1.3 The loop

> **Be contracted → field force → be paid → reinvest.**

A client offers a contract: a fact about the world it will pay to have become true, by a deadline.
Take the river province before the season ends. Hold the pass. Break the siege.

**The contract names the outcome and the fee. It never names the force.** Deciding whether the fee
covers what the objective actually costs is the whole skill of the game, and it is why the loop is a
strategy rather than a mission list. Underbid and you will send too little, lose the fight, and lose
the fee and your standing with it.

### 1.4 The three pillars

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

### 1.5 Winning, and losing

**There is no win screen and no lose screen.** The game does not end; your position gets better or
worse.

Losing is progressive and it is a spiral rather than an event. Lose a contract and your standing
falls. With lower standing the fees on offer fall too, and fewer clients will deal with you at all.
With smaller fees you can afford less force, which loses you the next contract. A company can die
of this without ever losing a battle it could not have won.

The escape is the same as the trap, run backwards: stop taking work you cannot deliver, hold what
you have, and rebuild. That is a real strategic choice with a real cost in time.

### 1.6 Time

The simulation advances in **quarters** — a tick is three months of campaign time, four to a year.
Between ticks the game runs in real time, so you can look around, read ledgers and give orders
without the world moving under you.

You control the clock directly: pause with `Space`, and set the speed tier with `1` through `5`.
Pausing is a legitimate way to play; nothing is hidden behind reaction speed.

---

## 2. Getting started

### 2.1 Starting a campaign

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

### 2.2 The screen

| Region | What it is |
|---|---|
| **Canvas** | The main view: the planet's hex tile surface. Where you look, select and build. |
| **Icon rail** (left) | One glyph per ledger. Clicking a glyph opens its ledger; clicking it again closes it. |
| **Minimap** (bottom right) | The rung one step out from the main view, and the way back up the zoom ladder. |
| **Lens bar** (under the minimap) | Map overlays. One click applies a lens; clicking the active one clears it. |
| **Selection band** (bottom) | Detail on whatever is currently selected, and the actions available on it. A live battle of yours selects ahead of everything else, as a battle card. |
| **Time column** (top right) | The clock, the pause control and the speed tiers. |
| **Comms dock** (bottom) | Messages and channels. The **Field** channel carries one dispatch per battle of yours per tick. |

### 2.3 Your first ten minutes

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

### 3.1 Camera and view

| Action | Control |
|---|---|
| Pan | Arrow keys, or hold the **middle mouse button** and drag |
| Zoom | Mouse wheel, or `=` / `+` to zoom in and `-` to zoom out |
| Zoom (precise) | Drag the zoom slider |
| Pause / resume | `Space`, or click the pause button |
| Speed tier | `1` – `5`, or click a speed-tier button |

### 3.2 Selection

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

### 3.3 Lenses

Lenses tint the map to show one quantity. Eight are on the bar — **Corporation, Country, Resource,
Market, Population, Opportunity, Production, Continent** — and four more are reachable only by
cycling: **Scarcity, Industry, Reach, Supply-routes**.

| Action | Control |
|---|---|
| Apply a lens | Click its glyph in the lens bar |
| Cycle forward / back | `L` / `Shift+L` |
| Clear the lens | `0`, or click the currently-active glyph |
| Choose the good (Resource / Market) | The combo in the lens legend |

### 3.4 Buildings and production

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

### 3.5 Units

| Action | Control |
|---|---|
| Hire a unit | Select a tile carrying your completed military base → **Hire** on a roster row |
| Follow a battle | Select the battle; the Selection band shows the battle card |

### 3.6 Markets

| Action | Control |
|---|---|
| Open the Market Ledger | The market glyph on the icon rail |
| Switch view | The **Prices** / **Sell Orders** tabs |
| Place a sell order | Sell Orders tab → pick a resource → set price and quantity |
| Remove a sell order | **Remove** beside the listed order |

### 3.7 Ledgers

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
| Contracts | Checkmark | Your mercenary offers, active contracts, and history |

Tax and wage tiers are set in the Budget Ledger with `-` / `+`, or by clicking a Roman numeral
`I`–`V`.

### 3.8 System

| Key | Opens |
|---|---|
| `F1` | Controls cheat-sheet overlay |
| `F5` | Quick save — the whole campaign to `quicksave.iosave` beside the executable |
| `F6` | Quick load |
| `F9` | Tech tree |
| `F10` | Options — resolution, UI scale, fullscreen, VSync |
| `F11` | Frame-budget HUD |
| `F12` | Screen capture |
| `Esc` | System menu; backs out of an armed confirmation first |

---

## 4. Systems overview

Each entry says what the system does and the document that owns it. The owning document is the
authority; this manual is a summary and defers to it.

### 4.1 World generation

A deterministic chain, each stage consuming the one above. **Planetology** settles atmosphere,
chemistry and the biosphere's history. **Continents** derives tectonic plates and hands a height
bias downward. **Tiles** generates the hex surface on three axes — substrate, cover and landform —
and seeds deposits. **Rivers**, **population centres**, the **institutional history ladder**,
**nations**, **roads** and **corporations** follow in order.

*Authority: `docs/generation/GENERATION_STRATEGY.md`, and the per-stage documents it maps.*

### 4.2 Tiles and terrain

Every tile carries a **substrate** (what the ground is made of), a **cover** (what sits on it, at a
density) and a **landform** (what shape it is). Substrate drives mineral deposits, defence and build cost;
cover drives biotic deposits, defensive cover and forage, and how the tile is drawn; landform drives movement cost, hazard, habitability and mineral
richness. Landform is the reason a mountain costs about twice a plain to cross — in money *and* in
days.

The homeworld is **312 × 145 = 45,240 tiles**, about 18,200 of them land, and a tile is roughly
128 km across.

*Authority: `docs/economy/TILES.md`.*

### 4.3 Resources

Three tiers — raw, refined, product — across four value tracks: industrial, ambient, habitability,
and **mercantile** (endemic goods whose value comes from geography rather than utility, so their
price is a function of distance from where they grow).

Roughly two thirds of the list is pre-industrial: grain, fodder, salt, charcoal, iron blooms,
bullion, stone, timber, clay, peat, furs, spices. The space-sourced entries belong to the space
arc.

*Authority: `docs/economy/RESOURCES.md`.*

### 4.4 Production

Buildings occupy tiles, consume inputs, and emit outputs on a recipe. Placement is validated against
terrain, deposits, slot rules and **logistical reach** — you cannot build at unbounded distance from
a supply anchor, and build time depends on where you build, not only what.

A campaign does not open onto the whole roster. Most building types and most recipes are not
buildable on day one — a fresh corporation sees a narrow opening set, and the rest is **earned**.
Three separate locks govern this, and the Build door tells them apart: an **era lock** (a recipe
belongs to a later technological era and never appears here); a **depth lock** (you have not yet
reached the chain depth a recipe requires — shown greyed with the reason, so you can see what you
are missing); and a **tech lock** (a specific research condition, e.g. holding a stockpile or
running a processing facility, that unlocks one recipe by name once met). Some buildings offer
**alternate production methods** for the same output — a real trade-off, not a straight upgrade,
so the better choice depends on which market the output is going to.

The corporation dashboard's Production card carries a **growth track**: the chain depth you have
reached and the good that set it, the buildings within reach at the next rung, and the specific
inputs still missing to place one of them. This is the intended way to read "what do I build
next" — not a wiki, the readout on your own dashboard.

*Authority: `docs/economy/PRODUCTION.md`.*

### 4.5 Markets and prices

Each market is independent. Base prices come from global rarity; local prices respond to local
supply and demand, smoothed rather than jumping. A matched price-time **order book** sits on top,
so the player and the AI place standing orders through the same mechanism.

*Authority: `docs/economy/MARKETS.md`.*

### 4.6 Money

Five flows resolve every tick: income, expenditure, maintenance, wages and interest. Debt accrues
interest. Balances may go negative. Wages and maintenance are separate lines because they fail
differently.

*Authority: `docs/economy/FINANCE.md`.*

### 4.7 Logistics

Roads come in three tiers and are generated as a per-nation lattice; cities are free logistics hubs.
Convoys route goods between markets at a cost weighted by terrain, and distance costs both money
and **time**.

A tile is about **128 km across** — derived from the planet's generated mass, not authored. A laden
caravan makes ~25 km/day overland and ~130 km/day by sea, and terrain multiplies that: a mountain
crossing costs about twice a plain. So a regional haul clears in a single quarter and a
cross-continental one takes several, which is what makes siting and sea access strategic rather
than cosmetic.

*Authority: `docs/economy/LOGISTICS.md` for the network, `docs/economy/SUPPLY.md` for the traffic.*

### 4.8 Discovery

Two independent fogs. The **geographic fog** hides the tile map and deposits until you pay for a
survey. The **activity fog** is lit by your own trade routes and presence, tiering each body from
Unknown through Known, Stale and Visible. A place can be Known but unsurveyed.

Rival buildings are visible; their production and stockpiles are private. Markets are the public
intelligence channel.

*Authority: `docs/ui/DISCOVERY.md`.*

### 4.9 Units and combat

Units are hired at a military base from an era-keyed roster. A battle opens whenever hostile units
share a province — declared hostility between corporations, or an active mercenary contract's own
force standing in its target province against that nation's garrison, no declaration needed — and
plays out over ticks in seeded rounds, with withdrawal priced rather than free.
Each round resolves unit class matchup by formation doctrine, modified by terrain, supply state and
season. Winter costs the attacker attrition and the defender readiness. Your own battles show as a
battle card and post a dispatch to the Field channel every tick.

*Authority: `docs/military/MILITARY.md`; the rival's decisions are `docs/ai/AI_OPPONENT.md`.*

### 4.10 Contracts

Your income as a mercenary company. A contract is a predicate a client nation will pay to have
become true by a deadline; you commit the force. Open the **Contracts ledger** (nav rail) for
three views — **Offers**, **Active**, **History** — and accept an offer through its own
force picker. Terminal states: **completed** (paid, standing up), **failed** (deposit forfeit,
standing down hard), **cancelled** (deposit forfeit, standing down less — you walked away), and
**abandoned** (same cost as cancelled, your own choice mid-contract).

Offers come from a threatened nation's own budget: a nation that cannot afford to hold a
contested border province against its highest-grudge neighbour posts the gap as work, deposit
funded in full before the offer is even acceptable. The company you hire fights that neighbour's
garrison directly — accepting a "take" offer and marching your committed force into the target
province is what starts the fight; win it and hold the ground to the deadline and the contract
pays. Every terminal event, and the deadline itself, posts to the Public channel; the Balance
ledger's own "Contract income" line and the header runway both read the same payout the tick it
lands.

*Authority: `docs/economy/CONTRACTS.md`; `docs/military/MILITARY.md` § Nation garrisons for what
you fight.*

### 4.11 Procurement

How you buy. Request a quote from a named supplier, accept it to open a contract, and pay a deposit
plus a paced remainder. A supplier can **decline**, with a stated reason: no capacity, no input
access, an embargo, or your standing being too low. Refusal is not payable-through — you route
around it. Asking for a quote also tells you something real about the supplier's capacity.

*Authority: `docs/economy/CONTRACTS.md`.*

### 4.12 Law and technology

A law is a predicate plus an effect, evaluated against the world, enacted by a nation and scoped to
its territory. The extraction levy is enforced and visible as its own line in the budget; enacting
nothing changes nothing, bit for bit. Technologies are gated by the same predicate machinery and
earned per corporation, not per world.

These are conditions you operate inside, not levers you pull. A nation can be **lobbied**; it cannot
be legislated for.

*Authority: `docs/politics/NATIONS.md` and `docs/META_LAYER.md`.*

### 4.13 The opponent

Rival companies run a deterministic scored-utility layer over the same command seam you use: they
build, demolish, survey, road, hire, trade and place sell orders. Nations act in the same shape,
allocating a budget over priority lines. There are no hidden AI exceptions — they play by your
rules or a defined subset of them.

Above that sits the direction: a small local model driving the game through its word interface. The
engine ships no HTTP client, no API key and no cloud dependency, and it never will.

*Authority: `docs/ai/AI_OPPONENT.md`.*

### 4.14 The pre-history

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

### 4.15 Relations

Every actor — corporation or nation — holds a **sentiment** toward every other it has met, derived
from conduct and decaying with time. On top of it sits **stance**: hostility you declare, friendship
both sides choose. Sentiment informs a declaration and never makes one.

*Authority: `docs/politics/RELATIONS.md`.*

---

*This manual is written by hand and is not authoritative over the documents it cites. When it
disagrees with `docs/ai/ACTIONS.json` about a control, or with a system's owning document about
behaviour, those win.*
