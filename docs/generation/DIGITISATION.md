# Project Io — Digitisation

> **Settles:** what the fourth simulated span is for · the seven properties of the world the
> campaign opens on · the three beats the wizard shows producing them · what an industry point is,
> where it sits and what it buys · how migration moves people and culture into cities · how
> colonies are lost · why war kills people in this phase and what a world war costs the campaign ·
> what a proxy war is at the epoch · how a region-grain sim becomes a tile-grain opening map ·
> where this phase stops and the landscape search starts · what the phase is judged on · what
> crosses into play · where its boundary with Exploration falls.
> **Not here:** the phase before it and everything it hands forward (EXPLORATION) · the nodes of
> its technology tree (trees/INDUSTRY_TREE) · how a market clears and how a price resolves
> (../economy/MARKETS) · how a haul is costed (../economy/SUPPLY) · how a corporation is placed,
> focused and financed (CORPORATION_GENERATION) · how the culture lens draws (../ui/LENSES) · the
> wizard round's surface (../ui/STARTUP § Rounds) · the Era 1 catastrophe test (../economy/ERAS)
> · how force resolves inside the sim (MILITARY_HISTORY) · the pass map and the calendar
> (GENERATION_STRATEGY § Pass 2).
> **Confused with:** EXPLORATION.md, CORPORATION_GENERATION.md, NATION_GENERATION.md,
> MILITARY_HISTORY.md, ../economy/MARKETS.md.

> ⟳ **What changed (2026-09-15, the Digitisation design session — remove once reviewed):** the
> placeholder is replaced. Ben set the three beats and the seven properties of the opening world,
> then ruled nine calls on an elicitation form: cultural demand weight, the industry-point sinks
> and charter budget, the proxy-war stub, a world war permitted and never forced, war that kills,
> the relative 5% rule, listed value as a market's cap, inequality between nations and cities, the
> search spending this phase's budgets, and a time-lapse round. **SETTLED** blocks are Ben's;
> **PROPOSED** blocks are Claude's readings that the form did not ask about, and stand until
> overturned (NR filed with this change).

> ⟳ **What changed (2026-09-17, cutting the build plan — remove once reviewed):** Ben ruled on an
> elicitation form that the charter budget charters the whole web, specialists included, and that
> the budget has no stand-in source before industry points exist. The epoch flip is recorded as
> selecting this span, never the superseded industrial pass. On the sprint 43 gate (NR-888) Ben
> ruled that Beat 3 carries its own displacement force, that this phase binds far pairs itself,
> and that the span runs on Exploration's 4-year band.

**Digitisation is the fourth and last simulated span: 1660 → 1960 CE, 300 years.** It opens the
instant Exploration closes and ends at the epoch, so the world it produces is the world the
campaign opens on (`GENERATION_STRATEGY.md` § Pass 2).

**The default epoch flips to 1960 as this phase's done-when (Ben, 2026-09-15, NR-869).** The
campaign epoch is 1960 (Ben, 2026-09-08), and the default world descriptor selects that arc when
this phase lands — never before, because a 1960 world with no Digitisation opens on a 300-year gap
the sim did not simulate.

**A 1960 epoch names a calendar, not a history.** The arc the flip selects is Exploration's handoff
continued through this span — never the single 1560 → 1960 industrial pass, which is superseded
and runs no Exploration, and so hands this phase none of its inputs (`GENERATION_STRATEGY.md`
§ Pass 2).

**SETTLED (Ben, 2026-09-17, NR-888): the span runs on Exploration's 4-year band.** One decision
round every four years, 75 rounds from 1660 to 1960. Measured on Exploration's forces alone, that
costs less than the 460 years before it; a 1-year band costs about four times as much and is not
taken unless a mechanism this phase adds is shown to need the grain.

**This phase is designed backwards, from the map it leaves.** Ben, 2026-09-15: *"While the wizard
visibly should show these happening, the real aim of our functions on this round is to seed the
tile view players will have when the game begins."* Pass 1 was designed forwards and grew
mechanisms nobody downstream asked for; this doc starts from the consumer and admits a mechanism
only when a property of the opening map needs it.

**It carries the largest scope of any phase, because so much rests on it** (Ben, 2026-09-15). The
phases before it hand forward abstractions — shares, signals, flows — and this is the phase that
must turn every one of them into something a player can click.

**It is designed to what the world allows, not to what the player does** (Ben, 2026-09-17, ruling
NR-885): *"for the digitisation round, it's less important what the player should be doing, and
more important what the world allows."* The player's verbs are the campaign's business; this phase
owns the ground they are used on.

---

## The boundary

**Ben, 2026-09-11, drawing it in one line:** *"Digitisation is exactly the round which generates
companies as we know them in live play. Exploration gives us cultural preference to goods,
markets that trade named goods in a simplified form."*

| Belongs to Exploration | Belongs to Digitisation |
|---|---|
| Capital as a per-polity treasury | Capital as firm balance sheets, and as industry points at cities |
| Named goods, traded in simplified form | The price field, the order book, the market carve |
| Trade flows between polities, unpriced, opened by treaty | Trade relationships a firm inherits and prices |
| Cultural preference for a good | Demand resolved from population and preference |
| Ports, navies, standing armies and their upkeep | Industrialisation, railways, mechanised force |
| Treaties, subjects, trade provinces | Decolonisation, world war, and **corporations as live-play actors** |
| War that destroys armies and spares civilians | War that kills people |
| The Exploration tree | The **Industry** tree, gated behind Exploration's rim |
| Migration as the Culture phase's diffusion | Migration into cities and across borders |
| — | Tariff posture by 1960; national political character hardened |

**The company is the reason the split exists.** A corporation is the object the campaign is played
with (`../CONCEPT.md`), and generating one needs a chartering institution, a treasury behind it,
and a market it can price against. Exploration produces all three and charters none of them.

**This phase inherits `EXPLORATION.md` § What this phase hands digitisation and nothing else.**
That list is the contract: a struct, not a promise.

**The tariff posture is derived here, from inputs that already cross.** Scarcity
(`region::scarcity_q`), trade flows (`trade_flows`) and cultural preference (`culture_preference`)
arrive in `exploration_output`; the derivation of `polity::protection_q` from them is owed to this
phase (BL-976, tariff derivation hands to Digitisation), and `derive_national_protection` →
`seed_national_tariffs` is the enactment seam that reads whatever this phase writes. The Era −1
sim derives the scalar only on the two-span arc, from industrialisation timing; a single-span world
carries no tariff, which is a legitimate outcome rather than a gap.

---

## The worlds to design against

**Sixteen curated seeds are stored for this phase** (`seed_library.json`; query with
`node tools/session/seed_library.js`). Generation is a pure function of the world descriptor, so a
seed is the save: each entry carries the readings that made that world interesting and a fingerprint
that says whether the world it still generates is the world described. They span the axes this phase
forms companies on — the median chest, the polity count, colonial subjects, post roads and trade
flows — from a floor case that hands this phase almost nothing to a ceiling case that hands it
everything. **The numbers live in the store, not here,** because a world-moving item is supposed to
move them.

---

## Part I — The world the campaign opens on

**SETTLED (Ben, 2026-09-15): seven properties of the opening map.** These are the phase's
deliverable. The three beats in Part II exist to produce them, and a beat that feeds none of them
has no business here.

| # | Property | Lands in | Produced by |
|---|---|---|---|
| 1 | Many companies around population centres; most markets carry goods suited to their cultures | Corporations, companies, market stock, household demand weights | Beat 1's stockpile, spent at the epoch; preference from Exploration |
| 2 | Advanced production (parts, computing) exists; wealth converts into production | Installations on advanced chains; capital | Beat 1, paid into by the treasury |
| 3 | Some conflicts are ongoing, mostly proxy wars in polities left behind or decolonised | A standing war condition on provinces | Beat 3, and the price gap it builds between great powers |
| 4 | A culture lens: primary culture per province, a secondary as a checkered fill when close | Culture shares read onto provinces | Beat 2 moves culture into cities |
| 5 | Wealth inequality, a tangible market cap per market, GDP per nation | Derived readings over the seeded economy | Beats 1 and 2 concentrating output |
| 6 | Some technological progress, political systems and armies — simple stubs | Campaign tech state, a regime field, garrison strength | Tree masks, the works fork, carried force |
| 7 | Working international markets: shipping far, over sea or land, often beats the nearest market | Price gaps with visible causes, and inherited trade relationships | Specialisation, preference, depletion, tariffs, colonial ties |

### 1. A dense corporate web, and markets that stock what their people want

**Firm density follows the city, not the basket.** `CORPORATION_GENERATION.md` § Pass 6 sizes the
background field by how many *different* goods are demanded, which is blind to where people are.
Ben's property is the opposite reading: companies cluster where population does.

**SETTLED (Ben, 2026-09-15, elicitation): a centre's industry-point stockpile at 1960 is its
CHARTER BUDGET.** What a city built and did not spend (§ Beat 1) is the capital a firm is chartered
against, located where the firm will stand. The web is dense where industry accumulated and thin
where it was spent or never lit — a consequence with a place on the map, never a count picked per
world. How the budget is spent is § This phase sets budgets; the search spends them.

**SETTLED (Ben, 2026-09-17, elicitation): the budget charters the WHOLE web, specialists
included.** A city's budget pays for the specialists the seat is chosen from, not only for the
background firms around them. So the seat shortlist is drawn from firms the budgets chartered, and
the corporations a player can hold stand where capital accumulated
(`CORPORATION_GENERATION.md` § The spawn shortlist, and the seat).

**The budget has one source: the stockpile (Ben, 2026-09-17).** No stand-in derived from urban
population fills it before industry points exist. A budget made of headcount would pass *density
follows cities* by construction, and the reading would prove nothing.

**SETTLED (Ben, 2026-09-17, elicitation): one specialist per centre that can afford one, richest
first.** Centres spend in order of budget, largest first, ties to the lower centre id. A centre whose
budget covers a specialist's price — a fixed number of firm charters — charters exactly one
specialist; what remains buys background firms around it; what cannot be spent stays unspent and is
counted. So the number of specialists is the number of centres rich enough to afford one, and it
rises with capital rather than being set by a count. **Pass 1's balancing across nations does not
apply on this path:** a nation whose cities accumulated capital holds the seats that capital
bought. The price of a specialist and of a firm are measured against live-play cost before either
is fixed; a specialist keeps today's starting capital until that measurement is read.

**SETTLED (Ben, 2026-09-18, NR-889): on a budget world the per-resource firm cap SCALES WITH THE
BODY'S CHARTER CAPITAL.** Pass 6 caps a body at 8 firms per demanded good, which froze firm count
however rich a world became; lifting the cap outright handed density to the 200-per-body runaway
guard at four to six times the live tick. So a richer body earns a higher per-good cap — capital
buys density, and breadth still decides which goods. The scaling rule is set in sprint 45 against
the cost table (`charter_cost_sweep.json`), and the specialist's price is chosen there on real
stockpiles, not on the synthetic test budget.

**PROPOSED (listed to Ben 2026-09-17, not overturned): a charter stays near its centre.** A firm is
anchored inside a window around its centre and then its centre's region; a charter that finds no
ground there is counted unspent rather than scattered across the nation, because a scattered firm
is capital that left the city that built it. **No budget and an empty budget are the same world as
today's**: the population-blind placement stands until a budget with something in it arrives.

**SETTLED (Ben, 2026-09-15, elicitation): terminal demand keeps universal PRESENCE and takes a
cultural WEIGHT.** `../economy/MARKETS.md` § Three properties the set has to hold (property 5)
rules terminal demand universal, and that half stands: every market keeps a buyer for every
terminal good its band supports, so no good is an orphan for a reason a player cannot read. What
culture changes is *how much* is bought. A market's household demand for a good is weighted by its
population's preference, which Exploration already derives per culture. Demand asymmetry now joins
supply asymmetry as a cause of trade, and property 7 needs both.

**The weight is read from people, not from rulers.** A market's weight is the population-weighted
preference of the culture shares across its catchment — region shares and centre shares both
(§ 4) — so a city full of migrants wants what its migrants' cultures want.

### 2. Advanced production exists, and wealth converts into it

**Computing is not in the Industry tree, by that tree's own ruling** (`trees/INDUSTRY_TREE.md`:
Early Computing belongs to the campaign tree's root). The campaign roster already carries
electronics, machinery and alloys, and an Electronics Lab that makes the first.

**PROPOSED: 1960's "computers and parts" are the existing tier-3 goods, not a new one.** Electronics
is what a 1960 world calls computing; adding a good is `../economy/RESOURCES.md`'s decision and is
not needed to satisfy the property.

**Wealth converts into production through the treasury, as a force.** A polity's treasury pays
into a centre's industry-point yield (§ Beat 1). A rich polity grows production faster because it
has more to pay with, not because a term favours it — the standing rule against agent handicaps.

**THE HARD PART IS THE BUYER, NOT THE SELLER.** Seeding advanced installations with no buyer
reproduces the failure that killed the industrial field once already — processors paying the
ceiling and idling. **Property 2 therefore depends on the stratum demand ladder**
(`../economy/POPULATION.md` § The stratum ladder, Ben 2026-09-15): appetite by headcount, a
metropolis rung that buys electronics, upper rungs scaled by the nation's qualification — which is
the campaign's own reading of *wealthier nations leverage their wealth*. The intermediates
(silicon, alloys, machinery) still lean on a labelled stopgap until building upkeep buys them.

### 3. Some conflict is live at the epoch, mostly by proxy

**The campaign opens mid-history, not at peace.** `EXPLORATION.md` already refuses a frozen map
at 1660; this phase carries the same refusal to 1960.

**PROPOSED: proxy war is where great-power conflict goes when war at home costs too much.** Great
powers that read each other's visible capability do not fight each other cheaply, so the cheap
option is backing one side of a war somewhere weak. A proxy war is a **patron link** — a great
power paying treasury or force into a client's army — on ground whose polity was left behind (no
furnace lit) or recently decolonised (§ Beat 3). Nothing picks the client; the price gap does.

**SETTLED (Ben, 2026-09-17, NR-888): Beat 3 carries its own displacement force; none is
inherited.** The arms race Exploration hands forward seals near-home war rather than displacing it.
Continued past 1660 on Exploration's own forces, about 89% of near-home alarm reads sit at the
ceiling, over 99% of near-home campaign candidates are blocked by a treaty, and pooled displacement
falls from 3.02 to 1.74 (BL-1028, weakness counters to 1960). So the price gap that sends a patron
abroad is this phase's to build, and **this phase reads Alarm on its own scale**, never through
Exploration's visible-capability reference. Exploration is not revisited for it.

**SETTLED (Ben, 2026-09-15, elicitation): a war standing at the epoch crosses as a STANDING
PROVINCE CONDITION, not as a war anyone plays.** The provinces it covers carry contested ground,
interdicted supply, raised ordnance demand and a named patron, and it resolves by in-world cost
rather than by an actor choosing moves. A player trades around it, sells into it, or avoids it.

**Why the condition and not a nation war state.** The nation grant (`../ai/AI_OPPONENT.md` § 11,
2026-08-18) admits a treasury, a tariff, a law and pair-state; it does not name a nation waging or
sponsoring war. A live war crossing into play would be a new widening, raised rather than assumed
(NR-517). The condition needs no grant, and a later war state can replace it without reshaping the
generation side, which produces the same patron link either way.

### 4. A culture lens at province grain

**SET (Ben, 2026-09-15):** primary culture per province as the fill; a secondary culture as a
checkered fill when the top two are close.

**SETTLED (Ben, 2026-09-15, elicitation): "close" is RELATIVE.** A province is checkered when its
second culture's share is **at least 95% of its first's**. 42% and 40% checker (0.95); 42% and
38% do not (0.90). Only the top two are drawn; a three-way near-tie shows the leading pair. The
relative reading means a checker marks a genuine contest at any level of fragmentation — two
cultures at 20% and 19% in a many-peopled province checker as surely as 50% and 48%.

**It amends three rulings, and each has an honest answer.**

| Ruling | Where | Answer |
|---|---|---|
| A province has *no name, no culture, no economy of its own* | `PROVINCES.md` § What a province is | A province **reads** culture, it does not own it — the same idiom as its owner, derived from its tiles |
| Every fill lens blends across province vertices | `PROVINCES.md` § Every lens blends across provinces | The primary fill blends; the checker is a hard pattern and is the exception, as the border band is |
| Many culture colours on one map read as plaid | `../ui/STARTUP.md` § Rounds (round 3) | The checker marks contested ground only; the lineage palette keeps kin in one hue family |

**Culture shares sit on regions, roughly ten provinces each.** A province read straight from its
region would be uniform across the whole region, and the checker would draw region-sized blocks.

**PROPOSED: population centres carry culture shares too, and migration fills them** (§ Beat 2). A
province's culture is its region's rural share blended with the shares of any centre inside it,
weighted by population. Cities become mixed and the countryside stays plain, so the checker lands
where migrants did — which is the lens telling the player something true.

**The lens's question: who lives here, and so what does this market want?** It is the legible
cause of property 1's cultural weight, which is what earns it a bar slot. `../ui/LENSES.md`
§ Culture lens owns how it draws.

### 5. Wealth inequality, market cap and GDP

**GDP is valued production, per nation** — output on the nation's tiles at local market prices.
The definition was ruled for a verification tool (NR-774); `../politics/NATIONS.md` § GDP now owns
it. At the epoch there is no trailing window of ticks, so the seeded GDP is output capacity at the
seeded price field.

**SETTLED (Ben, 2026-09-15, elicitation): a market's cap is its LISTED VALUE** — the summed
valuation of the firms headquartered in its catchment. It is the stock-exchange reading: how much
capital sits here. The firm valuation is the one the model already carries for a whole-firm buyout
(`../economy/FINANCE.md` § Whole-firm acquisition), so nothing new is valued. The corporation
ledger's *no market cap in the model* is about a firm's own figure and is not overturned; a
market's cap is a sum over firms, not a share price. `../economy/MARKETS.md` § A market's listed
value owns it.

**SETTLED (Ben, 2026-09-15, elicitation): inequality is BETWEEN NATIONS AND BETWEEN CITIES.** Read
as two spreads — GDP per head across nations, and output per centre within a nation — never
stored, and never between households. A household wealth distribution would be a new system and
the opening map does not need one.

### 6. Stubs: technology, political systems, armies

**SET (Ben, 2026-09-15): very simple stubs for now.** Each is one field fed by something the sim
already produces, so the campaign has a place to grow them later.

| Stub | PROPOSED source | Lands in |
|---|---|---|
| **Technology** | The Industry tree mask; *The Renewed Line* opens the campaign tree | The campaign tree's starting unlocks, per nation |
| **Political system** | The Works fork (State Arsenal / Private Works), charter reach, whether the polity was ever a subject, and whether it fought a world war | One regime field per nation, beside the three character enums in `NATION_GENERATION.md` |
| **Armies** | Carried `army_stock` and navy, and beat 1's mechanised force | Nation garrison strength, which otherwise sizes from a treasury seeded at zero |

Regime names are invented, never Earth labels (the standing names rule).

### 7. International markets where far beats near

**This is the property most at risk, because most of it is not generation's to deliver.**
Generation can seed price gaps; only the campaign economy decides whether a seller chases them.
Six rules kept trade local, and Ben ruled five of them changed on 2026-09-15, in the campaign's
own docs:

| Rule that kept trade local | Ruled | Where it now lives |
|---|---|---|
| Goods pools per body, so a same-body haul returned to the pool it left and sold at home | Pools per **market** | `../economy/PRODUCTION.md` § Stockpile and output flow |
| A corp sells through its lowest-id building's market | A corp clears in every market it holds a pool in | `../economy/MARKETS.md` § Market centres and seeding |
| Dispatch chases **shortfall**, never price | The **seller chases net price**, bounded by what the gap absorbs | `../economy/SUPPLY.md` § Dispatch trigger |
| Sea costs **more** per distance than land | Sea **cheapest** per distance, with a handling fee at each port | `SUPPLY.md` § Logistical cost |
| Tariffs apply only to matched order-book trades, so none fire | Duty on **convoy arrival across a border**, paid by the shipper | `MARKETS.md` § Tariffs — the first flow that pays a nation |
| One `base_price` per good in every market | **Kept** — gaps come from forces, not from authored prices | `MARKETS.md` § Price resolution |

**What generation owes is gaps with visible causes that outlive the first year of play.** Five
sources, each a consequence of upstream scalars:

- **Specialisation** — industry points concentrate output in a few cities (§ Beat 1).
- **Preference** — cultural weight makes the same good dearer where it is wanted (property 1).
- **Depletion** — ground worked for centuries hands the campaign a thinner reserve (§ Spend is
  estimated at the end).
- **Tariffs** — a late industrialiser protects what it has (`../politics/NATIONS.md`).
- **Colonial ties** — a former colony's chains still close through its old metropole
  (`GENERATION_STRATEGY.md` § What crosses each handoff).

**What the campaign owes is a seller that can reach for price.** That is `MARKETS.md`'s and
`SUPPLY.md`'s work, not this doc's. It is recorded here because property 7 cannot be judged at
the epoch alone: a gap that play erases in a year was never a working international market.

---

## Part II — The three beats the wizard shows

**SET (Ben, 2026-09-15):** the wizard's Digitisation round visibly shows three things happening —
industrialisation, mass migration into larger centres, and decolonisation.

**The beats are the watched half; Part I is the delivered half.** Each beat names which properties
it feeds, so a beat that drifts from its consumer is visible.

**SETTLED (Ben, 2026-09-15, elicitation): the round is a TIME-LAPSE that ENDS ON THE SEEDED MAP.**
The three beats play over 1660 → 1960 on the same 2D map as rounds 3–5, and the round closes on the
epoch's opening map — firms, markets and the price field drawn in place. `../ui/STARTUP.md`
§ Rounds owns the surface.

### Beat 1 — Industrialisation: cities make industry points

**SET (Ben, 2026-09-15):** *"large city centres build industry points which can be consumed for
appropriate tasks, or stockpiled until the end of the phase."*

**PROPOSED: an industry point is a LOCATED stock, held at a centre, one number per centre.** The
same property Exploration kept for the treasury: *a conqueror who takes the seat takes the stores
standing in it*. A city captured in 1890 hands over its works, and a war over an industrial city
is a war over something real.

**What produces them, as consequences:**

| Input | Read from | Why |
|---|---|---|
| Centre scale | The population map, grown by beat 2 | Large cities build; Ben's own condition |
| Fuel within reach | The Industry tree's `fuel` gate | *The map, not the scorer, decides who industrialises* |
| Capital paid in | The polity treasury | Property 2's wealth leverage, as a purchase |
| Tree capacity | Industry tree capacity nodes held | Technique multiplies what a city can do |

**SETTLED (Ben, 2026-09-15, elicitation): three sinks, and what is not spent charters firms.**

| Sink | What it buys | Feeds |
|---|---|---|
| **Rail** | The road ladder's next rung, on corridors from the city | Property 7 — cheaper long hauls on land |
| **Mechanised force** | Army and navy stock built from industry rather than treasury | Property 3 and beat 3's wars; property 6's garrisons |
| **Works** | Raises the city's own yield next round | Concentration, which is what makes properties 5 and 7 uneven |

**Spending and stockpiling are one choice: the state now, or firms later.** At 1960 each city's
stockpile becomes its charter budget (property 1).

**PROPOSED: the Works fork decides the lean, and it is already in the tree.** A polity holding
*State Arsenal* spends into rail and force; one holding *Private Works* stockpiles and charters.
That is a derived disposition, not a flag, and it is why two equally industrial powers open the
campaign with different corporate webs.

**An industry point is a second currency beside the treasury, and that cost is accepted.** It
earns its place because it is located and the treasury is not spread to cities; the alternative —
a treasury share earmarked per city — is the same object without the name.

### Beat 2 — Mass migration, and innovation where people gather

**SET (Ben, 2026-09-15):** *"mass migration and growth of larger centres (either within or to
another polity), causing technological innovation centred around larger population centres."*

**PROPOSED: migration moves people and culture shares along a line, toward work.** Two streams on
one rule:

| Stream | From → to | Pull | Push | Line |
|---|---|---|---|---|
| **Urbanisation** | A region's countryside → a centre in the same polity | Industry-point output at the centre | Depleted or strained ground | Held corridors |
| **Emigration** | A centre → a centre in another polity | The same pull, larger | War, a lost colony, strain | Contact, a line that can carry people, no war between them |

#### Far pairs meet and bind, and this phase makes them

**SETTLED (Ben, 2026-09-17, NR-888): the force that binds far pairs is Digitisation's.** Polities
arrive having met — 1,250 new contact pairs across the seed library by 1660 — but not one pair
first met after 1200 holds a non-aggression clause, and continued past 1660 on Exploration's own
forces contact nearly stops: 43 new pairs in 300 years (BL-1028, weakness counters to 1960).
Emigration's line needs contact, far trade (property 7) needs relationships between distant
polities, and a world war spreads only through bindings that reach across the map. So this phase
raises contact and binds far pairs itself — through trade, migration and alliance — rather than
inheriting a treaty graph that never reached past the old neighbourhood. Exploration is not
revisited for it.

**Migration carries culture.** A stream moves culture shares into its destination centre, so a
destination becomes mixed while its countryside stays plain. **This is what produces property 4's
checkered provinces**, and it is why the lens and the beat share a cause.

**War is a push, and displacement is how a war moves people** (§ War kills people). Refugees are an
emigration stream with war as its push, so a war visibly empties toward the nearest safe city.

**Innovation reads urban mass, not total population.** A polity's Industry tree investment reads
the population of its largest centres, superlinearly, rather than the population of its ground.
That turns Ben's *"centred around larger population centres"* into one input change on the
scorer's established shape (`trees/TREES.md` § The scorer). It also gives the leaderboard a
research column that no longer restates population — the reason Empires refused one.

**The pull is industry-point output, because the sim holds no wage.** The campaign carries wages;
the sim does not, and industry points at the destination are the nearest thing it holds.

### Beat 3 — Decolonisation, and wars over empire

**SET (Ben, 2026-09-15):** *"Large wars are fought over the notion of empire, and the ability to
keep colonies grows harder regardless of whether there is war."*

**PROPOSED: the loss of a colony reuses Exploration's renewal refusal, with inputs that grow.**
`EXPLORATION.md` § A colony is a subject, and it wants things of its own makes tribute a clause
with a term, and refusal at renewal the subject's secession. That score already reads cohesion,
distance and reach. This phase adds what the subject has become: its own urban mass and its own
industry points. **A colony that industrialised can pay for itself, so it stops paying.**

**"Harder regardless of war" is a cost that rises with the subject, never a timer.** PROPOSED: the
overlord's cost of holding a subject — its garrison upkeep — scales with the subject's population
and industry. As the colony's cities grow, the bill grows, whether or not anyone fires a shot.
Nothing counts down; the subject simply became expensive to hold.

**Wars over empire are contests for subjects between powers.** A great power wanting a rival's
subject — or wanting it freed — fights over it.

**A decolonised polity arrives at 1960 as a nation, and usually a poor one.** Its industry points
were spent or remitted elsewhere for generations. That makes it the natural ground for property
3's proxy wars — the beat and the property share a cause.

#### A world war is permitted, never forced

**SETTLED (Ben, 2026-09-15, elicitation and notes), closing the call deferred on 2026-09-11:**
*"we don't force it, but it is not unlikely. So I am happy with seeds that reach either target."*
A world war late in the span is a legitimate outcome, and so is its absence. The seed spread
should show both; a spread with none, or with one in every world, is a phase whose forces are
mis-set.

**PROPOSED: a world war is a war over empire that the mutual-defence clause spreads.** Exploration
already carries **mutual defence** — *an attack on one draws the other*. A contest for a subject
between two powers draws each power's bound partners, and theirs in turn. Whether a colonial war
stays local or becomes general is decided by how densely the treaty graph binds the great powers
at that moment — a consequence of standing bindings, never a scheduled event and never a roll.

#### War kills people, and it is bad for everyone

**SETTLED (Ben, 2026-09-15, notes):** *"we really need to ensure that people die when wars happen.
It's not good for any regime or polity."*

**This carries `MILITARY_HISTORY.md`'s war-is-non-demographic ruling forward and ends it here, by
the route that ruling left open.** That ruling (Ben, 2026-09-09) keeps battles from killing
farmers inside the Empire sim, because a battle that killed civilians produced empty regions that
outscored every real objective. It also says what an honest exception looks like: *a famine or
displacement mechanism of its own, authored as such and visible as such — never as a coefficient
hidden inside a battle.* Industrial war is that exception.

**PROPOSED: three named ways a war kills or moves people, each visible.**

| Mechanism | Who it takes | Why it cannot empty a region |
|---|---|---|
| **Conscript dead** | Mechanised force is raised from population; its losses are people, drawn back from the regions that raised them in proportion to what each gave | Losses fall across the raising realm, not on the battlefield |
| **Displacement** | War ground pushes an emigration stream toward safe cities (§ Beat 2) | People move; the count is conserved |
| **Blockade famine** | Interdicted supply on a war's ground thins a civilian count while the interdiction stands | Bounded by the interdiction's duration and the ground's own food |

**And it costs the victor too.** Fewer people make smaller cities, and smaller cities make fewer
industry points (§ Beat 1). War weariness lowers cohesion across the fighting realm, which raises
every subject's refusal at renewal — so a power that wins a war over empire can lose its empire
paying for it.

#### A world war leaves the campaign closer to its catastrophe

**SETTLED (Ben, 2026-09-15, notes):** a seed that fought a world war *"will be more likely to fail
our Era 1 catastrophe — nuclear war."*

**It does so through the two scalars `../economy/ERAS.md` § The two scalars already carries, and
through no third one.** A world war leaves every surviving belligerent more **Alarm**ed — standing
mechanised force its neighbours can see, grudges written by the dead, and trade flows the war
severed — and none of that adds **Ceiling**, which is the memory of a rupture *averted*. So a world
war moves the epoch toward the failure branch of the test without deciding it.

**It is not the rupture.** `../lore/HISTORY.md` § Stage 5 places the rupture ahead of the player,
*averted, not past*, and a conventional world war before the epoch does not contradict that. It is
the reason the campaign's near-miss is nearer on some worlds than on others.

---

## Part III — From region to tile: the downscale

**This is the hardest thing the phase does.** The sim works at region and polity grain with a
handful of abstract goods. The campaign opens on tiles, named resources, installations, firms and
prices. Every property in Part I needs a stated rule for crossing that gap.

| Sim quantity | Campaign field | PROPOSED projection |
|---|---|---|
| Industry points at a centre | Firms and installations near it | The landscape search spends the stockpile as its budget (below) |
| Scarcity signal per good per market | Starting price per market | Base price scaled by the signal, inside the band |
| Culture preference × population shares | Household demand weight per market | Population-weighted preference over the catchment |
| Culture shares on regions and centres | Province culture for the lens | Population-weighted blend (property 4) |
| Trade flows per seller, buyer, good | Preferred-seller relationships | One relationship per flow above a threshold |
| Corridor throughput and rail rung | Road tiers on tiles | The existing stamp, one rung higher |
| Held-and-worked duration, class, throughput | `tile_component::resource_remaining` | The depletion formula (§ Spend is estimated at the end) |
| Standing wars and patron links | A war condition on provinces | Property 3 |
| Industry tree mask, works fork, army stock | Tech state, regime, garrisons | Property 6's stubs |
| War grudges, standing force, severed flows | Per-nation Alarm at the epoch | § A world war leaves the campaign closer to its catastrophe |

### This phase sets budgets; the search spends them

**SETTLED (Ben, 2026-09-15, elicitation): Digitisation decides HOW MUCH and WHERE; the landscape
search decides WHICH.** `GENERATION_STRATEGY.md` § The eight phases gives phase 6 — a static search
over rosters, placements and road tiers — the corporate landscape; Ben's 2026-09-11 line gives it
to this phase. Both hold. Digitisation produces a charter budget per centre, a demand weight per
market and a price field; the search picks the roster and placement that spends each budget
viably, on its existing terms.

**So the search is Digitisation's last act, not a phase after it.** Neither is written twice, and
the viability check the search exists for survives: a budget spent on firms that cannot pay is
exactly what its terms score down.

---

## Where it will be hard, ranked

1. **Property 7 is mostly campaign economy work.** Its design is ruled (§ 7), and the build is
   deep: per-market pools change the save format and every pool read, and per-leg routing replaces
   the path's mode bit. A seeded gap erased in a year of play still proves nothing, so the reading
   is taken after play, not only at the epoch.
2. **Property 2 needs a buyer before it has a seller.** The stratum ladder is ruled; until it is
   built, seeding advanced installations reproduces the idle-at-the-ceiling failure.
3. **War that kills reopens the dead-region loop's door.** The mechanisms above are shaped to keep
   it shut, and a sweep must show no region emptied by war before any constant is tuned.
4. **The downscale (Part III) has no precedent.** Every earlier handoff crossed at region grain.
   This one crosses to tiles, and each row above is a design of its own.
5. **Cost.** Three hundred more years on `history_sim`, on a region count that grows inside the
   run, with migration adding per-round work on top of Exploration's.
6. **A world war is permitted, not forced, so its rate must be measured.** The treaty graph's
   density decides it; too sparse and none ever happens, too dense and every world burns.
7. **Firm count at the epoch meets live-play cost.** A dense web is many actors each running the
   corporate scorer every tick. The charter budget must be measured against tick time before it is
   tuned for the map.

---

## What the phase is judged on

Readings taken at **1960 CE** over a **seed spread**, never per world — the discipline
`EXPLORATION.md` § What the phase is judged on applies. PROPOSED, one per property and beat:

| Reading | What it must show |
|---|---|
| **Density follows cities** | Firm count per market rises with the catchment's urban population, not with its good count |
| **Cultural stock** | In most markets, the most-preferred goods have a local or preferred seller |
| **Advanced chains** | Realised somewhere, unevenly, and correlated with treasury rather than size |
| **Live conflict** | A war condition standing at the epoch in most worlds, most of them on left-behind or decolonised ground |
| **Mixed cities** | Checkered provinces present, and concentrated in large centres |
| **Inequality** | GDP per head spread wide across nations; output spread wide across a nation's cities |
| **Far trade** | A material share of seeded trade relationships skip the seller's nearest market |
| **Industrialisation** | Industry points unevenly distributed, with the fuel gate visible in who holds them |
| **Migration** | Urban share rising across the span; at least one cross-border stream in most worlds |
| **Decolonisation** | Fewer subjects at 1960 than at 1660, not zero, and at least one lost without a war |
| **World war** | Present in some worlds and absent in others — never all, never none |
| **War dead** | Every war lowers population somewhere; no region emptied by one |
| **Catastrophe lean** | Worlds that fought a world war open with higher aggregate Alarm against Ceiling than worlds that did not |

**A reading is a requirement, not a target.** A seed that refuses one is a legitimate world; a
spread that refuses one is a phase that did not do its job.

---

## What crosses into play

The epoch's handoff, and nothing else:

- **Nations** from the 1960 polity map, with seeded **GDP**, a **regime** stub and **garrisons**
  from carried force.
- **The corporate web**: firms and installations the search spent from each city's charter budget.
- **The price field** and **household demand weights** per market.
- **Preferred-seller relationships** from surviving flows and colonial ties.
- **Tariff posture** as `import_tariff` laws.
- **Road tiers** with the rail rung, stamped from corridors.
- **Culture shares** per region and per centre, for the lens and for demand.
- **Standing war conditions** on provinces, with their patrons.
- **Campaign tech state** per nation from the Industry tree mask.
- **Per-nation Alarm** seeded from the span's wars, grudges and severed flows.
- **Sea lanes**, a traffic count per sea leg stamped to a tier, discounting sea-leg
  traversal cost for every consumer of it
  (`EXPLORATION.md` § The colonial tie is a sea lane, and the map reads it).
- **Network strength** — corridor traffic, sea-lane traffic and junction degree per region —
  which is what places the landscape search's **seed candidate** (folded from
  `../research/COLONIAL_ERA.md` on 2026-09-16, NR-884). The search refines that seed greedily and its
  perturbation axes are unchanged; what stops being arbitrary is the starting point. A
  junction three corridors and a lane meet at is a target; a rich province nothing reaches
  is not, yet.
- **Depleted reserves** from the retrofit formula.
- **Grudges**, filtered over this span's dead, as sentiment.

### Spend is estimated at the end, not accumulated throughout

**SETTLED (Ben, 2026-09-11): depletion is RETROFITTED.** *"I would prefer if we retrofit this, and
come to a sensible estimate at the very end of digitisation."* The campaign's
`tile_component::resource_remaining` is seeded from a **stated formula** — named inputs, named
arithmetic — computed once at 1960 from how long ground was held and worked, its dominant class,
whether and when it industrialised, and the throughput that ran off it. An unwritten estimate is
the failure mode (`EXPLORATION.md` § Spend is estimated at the end, not accumulated throughout).

---

## Open questions

Measured rather than argued:

- **Whether 300 years stays affordable once mechanism is added.** On Exploration's forces alone, at
  its 4-year band, the span costs less than the 460 years before it: a median 1.9 s a seed in
  Release across the library, at most 14.9 s (BL-1027, span cost to 1960). Migration and war that
  kills add per-round work that measurement does not yet include.
- **Where the migration pull saturates** — a stream that never stops empties the countryside.
- **How dense the treaty graph must be for a world war to be "not unlikely"** without being
  certain.
- **What price gap sends a patron abroad.** The inherited arms race seals near-home war rather than
  displacing it (§ 3), so Beat 3's displacement force is designed here, then measured.
- **What binds a far pair, and how fast** (§ Far pairs meet and bind, and this phase makes them).
- **Whether a colony can hold a colony** (carried from `EXPLORATION.md`).
- **The depletion formula itself** — writing it down is the work.
- **How a charter budget converts to firms** — the price of a firm charter and of a specialist in
  budget points, and the rule by which the per-resource cap scales with a body's charter capital,
  each set against live-play cost before it is fixed (the split and the scaling itself are settled
  in § 1).
- **Whether a specialist's charter price becomes its starting capital**, once that price is measured.
- **What a world gets when no centre can afford a specialist** — decided when Beat 1's real
  stockpiles exist, not on a synthetic budget.

Owed from Ben when they bite, not before:

- **Whether a standing war condition ever becomes a live nation war** — that is a new widening of
  `../ai/AI_OPPONENT.md` § 11, raised when the campaign wants it.
- **The PROPOSED readings above that the form did not ask about** — computing as electronics,
  located industry points, the Works-fork lean, the migration streams, colony loss by rising cost,
  proxy war as a patron link, the three war-death mechanisms, and the stub sources.
