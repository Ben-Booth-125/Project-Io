# Project Io — Exploration

> **Settles:** what the exploration age is for and which question it answers · where the treasury
> sits and what it buys · why this phase carries a scarcity signal and never a price · how goods
> move without a cargo ever existing · how a want met by throughput becomes a flow between
> polities, and why that flow is what a market earns · which campaign a want ranks · how the
> treasury allocates its spend · what a treaty is as data and what it binds · what a colony
> is and why it wants things of its own · the two ways ground is claimed across water and what
> each costs · what a colonial tie is on the map and what reads it · how ports, navies and
> standing armies are paid for and
> how they decay · how an arms race buys quiet at home and opportunity abroad · the two ways to
> be strong and how a creed decides which · how a good acquires a cultural preference · what the
> phase hands Industrialisation.
> **Not here:** what crosses into it from the Empire phase (CIVILISATION § The closure of the
> Empire era) · the nodes of its technology tree and the grammar every tree obeys
> (trees/EXPLORATION_TREE, trees/TREES) · the phase after it (INDUSTRIALISATION) · how a market
> clears once there is a price (../economy/MARKETS) · the campaign-era money loop
> (../economy/FINANCE) · the pass map and the calendar (GENERATION_STRATEGY § Pass 2).
> **Confused with:** CIVILISATION.md, COLONISATION.md, INDUSTRIALISATION.md,
> ../economy/MARKETS.md.

> ⟳ **What changed (2026-09-14, the Exploration trade batch — remove once reviewed):** new § Trade
> is a want met by throughput; § A want ranks a campaign; spend allocated in upkeep
> (§ Force persists now, § The engine is shared); unmet-want signal; no flat market income; trade
> value in a binding; an eleventh reading (Trade); trade flows, grudges and the surviving network
> added to the handoff list, with world setup reading them.

**Exploration is the third simulated span and the first with a price on anything.** It runs
**1200 → 1660 CE, 460 years**, opening the instant the Empire phase closes — the 1200 → 1560
coast is retired (`GENERATION_STRATEGY.md` § Pass 2). Where Empires asks *who holds this ground*,
Exploration asks *who wants what someone else holds, and what they will do about it*.

**Ben, 2026-09-11, naming the phase's two jobs:** *"to stimulate international demand for goods,
and strengthen the economic power of colonial actors."* Every mechanism below exists to move one
of those two, and a mechanism that moves neither has no business here.

**The name is Exploration** (Ben, 2026-09-11). *Exploration* describes what the age looks like
and is not a term; the phase, its tree and its store all carry the one name.

---

## The shape of the age, and why it is not Empires again

**Empires is a phase of fear and adjacency; Exploration is a phase of appetite and distance.**
The Empire phase's engine is a neighbour who might come for you, and its verbs resolve against
ground you can walk to. That engine produced the world this one starts on, and running it again
for another 460 years would produce more of the same world.

**So the signature of this phase is that conflict MOVES.** Ben, 2026-09-11: *"we will need to
solidify diplomatic ties, so that conflicts close to home are less likely than skirmish conflicts
over territorial bodies"* — and, clarifying the same day, **the territory in question is on the
home planet**. Other continents, not other worlds; Era 2 owns the sky (`../economy/ERAS.md`).

That gives the phase one structural claim, and it is the claim the sweep must confirm: **the rate
of conflict between long-contacted neighbours FALLS across the span while the rate of conflict on
newly-contacted ground RISES.** A phase in which both fall is a peace nobody asked for; a phase in
which both rise is Empires wearing a hat.

**Three things make that displacement happen, and none of them is a term inside an actor.** The
standing rule against agent handicaps binds here as everywhere: treaties raise the *cost* of a
neighbour war, ports lower the *cost* of a distant one, and a neighbour's visible capability makes
binding cheaper than testing it (§ The arms race). The actor's scorer is untouched; the world
around it changed price. Wants rank which campaigns a polity takes; they do not by themselves move
where it fights (§ A want ranks a campaign).

---

## Capital arrives, and it sits in the capital

**SETTLED (Ben, 2026-09-11): the treasury sits AT THE CAPITAL SEAT, one per polity.** Not a heap
on every region, and not a free-floating per-polity number with no place on the map.

**This keeps the one property the Empire phase's stores were built for.**
`CIVILISATION.md` § Materials are spent when something happens puts stores at the seat precisely
so that *"a conqueror who takes the seat takes the stores standing in it"*. A treasury with no
location would discard that in the phase that needs it most — a fleet that sacks a capital must
take something real, or a distant war is a gesture.

**Consolidating the seats into the capital is the phase's OPENING ACT, and it is visible.**
Ben, 2026-09-11: *"We can just consolidate all stores of material into the polity level as the
initial part of Exploration... it doesn't bother me if we introduce major centralisation as an
initial step."* At 1200 CE every seat's stores flow to the capital, once. That centralisation is
a thing the time-lapse can show, and it is the moment a realm stops being a set of provisioned
cities and starts being a state with a purse.

**Why one number rather than a valued map.** A treasury is the smallest object that can price the
three things this phase buys — a port, a fleet, and a binding worth more than a war. A regional
model would price all three *and* re-open who-may-spend-what, which is a logistics question this
phase does not ask.

**The treasury is a consequence, never an allowance.** It is fed by what the polity already holds
and already reaches — endowment on held ground, the inherited `surviving_corridors` network, the
trade that flows through the markets on its seats, and what its subjects remit. Nothing tops it
up: a polity that inherited little is poor, which is the asymmetry the handoff exists to produce.

**A market earns by what flows through it, not by standing (Ben, 2026-09-14).** A flat income per
market seat is an allowance under another name, so there is none: a market with no trade crossing
it earns nothing, and a market on a busy line earns in proportion to the line
(§ Trade is a want met by throughput).

---

## There is no price here, only a scarcity signal

**SETTLED (Ben, 2026-09-11): this phase carries a SCARCITY SIGNAL, not a price.** One integer per
good per market — how badly this market wants this good — with no clearing, no order book and no
firm. Industrialisation resolves signals into prices; this phase only says what is wanted and how much.

**This is what keeps `CIVILISATION.md`'s boundary from drifting.** That doc's markets at the close
are *a place and a visible condition, not an order book*, and a price in this phase would quietly
build the economy layer two phases early. The signal is the smallest thing that carries the
information forward without becoming a market.

**The signal is the DEMAND half, and it is not sufficient on its own** — § Goods move as
throughput, never as cargo owns the other half.

**The signal reads UNMET want (Ben, 2026-09-14).** A market's raw want is what its ground lacks
and its people need; the signal a reader sees is that want less what trade already brought. A want
that is being met stops pointing anywhere, which is what lets an appetite resolve rather than
persist forever.

---

## Goods move as throughput, never as cargo

**SETTLED (Ben, 2026-09-11): no per-good routing.** Nothing paths a cargo, nothing owns a cargo,
and no convoy is an object. `GENERATION_STRATEGY.md` records that the Empire pass already pays a
full Dijkstra per polity per round against a region count that grows *inside* the run; routing
goods on top of that multiplies the known hot spot across another 460 years.

**What moves instead is ONE NUMBER PER CORRIDOR.** The sim already derives
`region::network_supply_q` — 0–1000, terrain-weighted reach from the capital over held ground,
discounted by a road and relayed by a town. That is throughput in everything but name, and the
flow this phase needs is that quantity read along the `surviving_corridors` graph it already
walks.

**Scarcity says what is wanted; throughput says how much arrives.** Neither alone is enough. A
want with no throughput is an appetite that never resolves, and throughput with no want is traffic
with no reason. The pair is what makes a trade route a consequence rather than a drawing.

**The road ladder gets its third rung here.** The Empire sim promotes a corridor by use — a Track
at four uses, a Road above it, and no tier beyond. A phase about throughput is where the next rung
belongs, bought with capital rather than earned by traffic, which is Ben's *"upgrade of roads"*.

**A resumed corridor reopens at the rung it was recorded at (Ben, 2026-09-18).** A span that opens
from a handoff seeds each corridor at its recorded rung: a bought rung is never demoted to the rung
its walks alone would earn, and a walk the record counts but the road never took is never promoted
past it. Otherwise a post road reopens as a Road and can be bought a second time.

**THE VISUAL IS A FILTER ON THAT NUMBER, NOT A SECOND SIMULATION (Ben, 2026-09-11).** *"We also
want to visualise fleets and caravans moving, this can be heavily abstracted since the pace of our
time-lapse is too fast, so we will just render examples when a threshold supply is reached."* A
corridor whose throughput crosses a threshold draws an exemplar — one caravan, one sail — and the
exemplar stands for the flow rather than depicting it. Because the flow is already a corridor
number, this costs a comparison.

---

## Trade is a want met by throughput

**SETTLED (Ben, 2026-09-14): goods FLOW between polities, as one number per seller, buyer and
good.** Not a cargo, not a route and not a price. A flow is a fact in the same family as a grudge
or a contact — a named, directed pair plus what joined them — and it is recorded, so the time-lapse
and the handoff can read it.

**A flow needs three things at once, and each is already in the world.**

| Needs | Read from | If absent |
|---|---|---|
| A **want** | The buyer market's scarcity signal for the good | Nothing is wanted; no flow |
| A **holder** | Ground the seller holds whose dominant class is the good | Nothing to sell; no flow |
| A **line** | Throughput that can carry it — held corridors on land, a built port and a navy across water | The want never resolves |

The volume is bounded by the smallest of the three. A rich want meeting a thin line moves little;
a fat line with nothing wanted moves nothing.

**ONLY THE TRADE-ACCESS CLAUSE OPENS A FLOW (Ben, 2026-09-14).** Contact makes a stranger's market
*legible*; it does not make it *reachable*. Without the clause, a want pointed at a stranger's good
resolves as a landing or a campaign — which is the doc's own phrase, "a flow instead of a landing",
read literally. So trade and the treaty system are one mechanism, and a polity with no bindings
trades with nobody.

**A met want relieves the buyer's signal**, per § There is no price here. **The flow earns both
ends**: the seller's market for what it sold, and the buyer's market for what passed through it.
That is the whole of market income (§ Capital arrives).

**THE TRADE A BINDING OPENS IS PART OF WHAT A BINDING IS WORTH (Ben, 2026-09-14).** A pair scores
a treaty partly by the flow its trade-access clause would open between them. This is what lets a
DISTANT pair bind: far from home, deterrence earns nothing and the far penalty applies
(§ The arms race), so only trade can make a stranger worth a promise. And it is why the conflict
the phase displaces goes to ground whose holder has **no** binding with the arriving power — the
partner is worth more alive.

**Why this is the global-trade lever.** Nothing in the phase targets a volume. Trade is as global as
wants are uneven, lines are long and bindings cross water — three consequences of upstream
scalars, which is the standing rule for every force here.

---

## Diplomacy becomes real, and this phase is where the exclusion lifts

**`CIVILISATION.md` § Fear of being next states three exclusions, and the third is that there are
no treaties, no negotiation and no alliance objects.** That exclusion is correct *for Empires*:
it is what keeps the coalition a fear response inside a generation sim rather than a diplomacy
layer, and nothing here relaxes it backwards.

**Ben, 2026-09-11, overturning it FORWARD:** *"we should move away from the rapid claim and grudge
system. We should be creating lengthy treaties and pushing slowly towards gaining territory."* So
Exploration has real treaty objects, and Empires still does not. The two phases sharing an
engine (§ The engine is shared) does not make them share a diplomatic model.

### What a treaty is

**A directed or mutual pair, a term of years, and one or more bound clauses.** It is an object in
the same family as `grudge` and `contact` — a named pair plus what joined them — with the two
additions this phase needs: it **expires**, and it **binds** rather than merely records.

Five clause kinds, all five settled (Ben, 2026-09-11):

| Clause | What it binds | Why the phase needs it |
|---|---|---|
| **Non-aggression** | Neither party campaigns against the other while the term runs | The direct cause of the home-coast quiet |
| **Trade access** | One party's market is legible and reachable to the other | Turns a want into a flow instead of a landing — the only thing that opens one (§ Trade is a want met by throughput) |
| **Sphere of claim** | Which unclaimed ground each party may claim | Two powers divide a frontier instead of fighting over it |
| **Tribute** | A remittance from subject to overlord | The colonial actor's economic strengthening, made explicit |
| **Mutual defence** | An attack on one draws the other | Makes a small polity expensive to eat |

**A term of years is what makes it "lengthy" rather than a stance.** `../politics/RELATIONS.md`
separates declared stance from derived sentiment; a treaty is neither. It is a *commitment with
an end date*, and the end date is the whole reason a polity plans around it.

**Breaking one is legal and costly.** A treaty that cannot be broken is a rule, not a promise, and
a phase whose actors never defect produces a flat map. Defection writes a grudge in the existing
table — which is how the Empire phase's machinery is reused rather than duplicated — and the cost
lands on every *other* party's willingness to bind with the defector. Nothing reads the defector's
own ledger to choose a target; that remains revenge and remains declined.

**Nobody negotiates.** A treaty forms when both parties independently score the binding as worth
more than the freedom it costs, evaluated against the same seeded world state. What a binding is
worth includes the trade its access clause would open. This keeps the
determinism rule intact and keeps the phase free of a bargaining layer it cannot afford.

---

## A colony is a subject, and it wants things of its own

**SETTLED (Ben, 2026-09-11): a colony is a LIVE POLITY with an overlord link.** Not a region
annotation and not a new actor class. The polity table already holds everything a colony needs —
a capital, a culture, cohesion, held ground, a tree mask — and the colony's whole distinction is
that one field points at somebody else.

**`polity::parent` is NOT that field.** `parent` records lineage: who this polity broke away
*from*. Subjection is a live relation that can begin, end, and be transferred, and overloading
lineage onto it would make a colony indistinguishable from a secession the moment either changes.
The overlord link is its own field.

**Ben, 2026-09-11: colonies are subjects "with their own desires."** That is the load-bearing
half. A colony keeps its own entry in the want table, and its wants are its own — pointed at what
its ground lacks and its neighbours hold, not at what the metropole needs. Two consequences follow
and both are wanted: a colony can want something its overlord does not care about, and a colony's
wants can point at a *rival's* colony across a frontier the metropoles have treatied over.

**A colony that can pay for itself can stop paying.** Tribute is a treaty clause, and a clause
with a term is a clause that comes up for renewal. The condition under which a subject refuses
renewal is the phase's own secession mechanism, and it should read the same quantities the Empire
phase's secession reads — cohesion, distance, and whether the overlord is reachable at all — so
that a colonial empire outrunning its network is the same failure as a land empire doing it.

### Where subjects come from

**What is discovered is people, not empty land** — `CIVILISATION.md` § Contact is polity to
polity fixes that, and it binds here. The migration filled every habitable landmass, so a far
continent at 1200 CE is settled and unmet. **There is no terra nullius to claim.**

So a subject arises one of two ways, and Ben, 2026-09-11 names both: *"the most technologically
dominant nations are able to lay claim to polities they have no direct control over — beginning to
sprout trade provinces, and steal land from natives."*

- **A trade province** — a foothold on a native polity's coast, taken by a power that arrived with
  capital and a want. It is a *seat with a market and an overlord*, not a conquest: the native
  polity survives beside it, which is what makes the relation extractive rather than terminal.
- **A subjected polity** — the native polity itself, brought under an overlord link, whole.

**The asymmetry that permits either is technological, and it is a node, not a rank.** Nothing
reads size. What decides who can arrive at all is what the arriving polity holds — the sea legs
and the institutions — exactly as the explorer set at 1200 CE is a held rim milestone rather than
a ranking (`CIVILISATION.md` § Some peoples gain the capacity to explore).

---

## Two ways to claim ground across water

*(Folded from `../research/COLONIAL_ERA.md` on 2026-09-16, NR-884. The subject is this phase's; the doc it
came from claimed a span this one and `INDUSTRIALISATION.md` now own between them.)*

A colonial claim is a polity verb on a target across a **sea leg**, staged from harbour works, and
there are **two of them**. Both are the same actor at the same grain choosing among its sim verbs,
and both sit inside the 2026-08-18 nation-and-polity grant on that grant's terms — pure, seeded,
deterministic, scored-utility, never a planner (`../ai/AI_OPPONENT.md` § 11). The second verb was
**raised** by Ben on 2026-09-09, not read into the first.

**Purchase — the diplomatic claim.** *"Something like purchasing a province for the purpose of
trade, and respecting local customs."* A polity pays for a province rather than taking it, out of
the capital its seat already holds (§ Capital arrives, and it sits in the capital — the same
currency, no second one). What it buys is *access*: the province's endowment becomes reachable to
the buyer's network, and the buyer comes to want what it grows (§ A good acquires a cultural
preference). What it does **not** buy is the people. **A purchased province keeps its culture
shares**; nothing is digested, because nothing was conquered. That is what "respecting local
customs" means as data.

**Conquest — the military claim.** *"Military claims, and war for items such as gold."* The
campaign verb `MILITARY_HISTORY.md` already resolves, played across a sea leg. The province changes
hands, its people are digested over centuries, and the grudge ledger records who did it. A
conquered province's endowment is the conqueror's outright, which is why a polity would fight for
it: gold is worth a war in a way a cash crop's *access* is not.

**The scorer chooses between them on the ground's terms**, never on a posture flag. A rich metal
endowment behind a weak defence scores the campaign; a cash-crop province under a polity with a
full treasury scores the purchase; a seller whose stores are empty is cheaper to buy from. The two
verbs are one contest with two costs, which is what makes the map readable afterwards — a player
can point at a colony and say *bought* or *taken*, and the culture shares and the grudge ledger
will agree.

**Both culminate as every major does.** The non-hegemony invariant is not relaxed for the sea; a
metropole that overreaches fragments, and its colonies arrive at the epoch as nations with a
grudge, a taste and a sea lane.

---

## The colonial tie is a sea lane, and the map reads it

*(Folded from `../research/COLONIAL_ERA.md` on 2026-09-16, NR-884.)*

**A tie is a force on the map, never a preference inside an actor.** An earlier design seeded the
order book's preferred-seller relationships from ties; that half is superseded (Ben, 2026-09-09),
because the buy side that would have read them is dormant in play, and a relationship nothing reads
is not a force. What the design needed was the same thing the ancient roads gave the land.

**Sea lanes are stamped FROM the record, exactly as roads are stamped from the empires'.** The span
records every sea leg it walked — a purchase party's crossing, a campaign's sea supply, the
standing traffic between a metropole and what it holds — and a pass immediately after stamps those
legs onto the water as a **sea lane** tier that discounts sea-leg traversal cost, the water
analogue of `../economy/LOGISTICS.md` § 4a. Traffic earns the tier; a crossing made once is no
lane. Purely additive; land tiles are untouched.

**What reads it is everything that reads traversal cost**, because traversal cost is one weight
function (`../economy/LOGISTICS.md` § 1). A convoy between a colony's market and its metropole's
is cheaper than one to a stranger's, so a colony's chains close through its metropole *first*
without anyone naming a preferred seller — the landed price says so. The same discount widens
**placement reach** across the lane, so a firm on one shore can legally hold a site on the other.
That is a wider effect than a market preference and it is the intended one: a tie that only moved
prices would be invisible to the search, and a search that cannot see roads cannot see lanes.

**A lane is what a colony leaves behind when the metropole falls.** Ties outlive the polity that
made them, as roads outlive the empire that paved them; the map carries lanes between nations that
have not been one polity for a century, and that network is what `INDUSTRIALISATION.md` hands to the
landscape search.

---

## Force persists now, and persistence has a bill

**SETTLED (Ben, 2026-09-16, NR-878): the actor has NO PURSE TERM, and poverty is the brake.** With the saturation caps replaced by a real per-head bill, a polity ranked above about 500 on either lean buys a stock step whenever one is affordable and the bill then drains what it bought with: over the span the per-seed MEDIAN polity treasury falls from 1.26M to **211**, the top decile loses about 3%, and roughly **14% of polity-rounds cannot pay the army bill**. Ben ruled that shape ACCEPTED: **a realm that over-builds is poor, and being poor is the consequence.** No solvency rule was added to a step's eligibility, and no cap came back.

The reading is structural rather than a tuning miss, and the figures say so: at a tenth of the rate the median still ends at 181k, because a cap-sized army over 115 rounds costs about one median hoard. **The caps were hiding a 600x treasury spread** — so what looks like a new failure is an old one becoming visible, which is the whole point of putting the cost in the world. The visible consequence to watch is displacement: its median fell to 0.85 on this arm while the pooled figure held at 1.64. If a later reading shows the spent purse suppressing displacement rather than differentiating realms, that is the evidence that reopens this call.

**SETTLED (Ben, 2026-09-11): `army_stock` CARRIES across the handoff; the navy is NEW and starts
at zero, everywhere.** No polity inherits a fleet, which means the first ocean-capable power in a
world built its fleet inside this phase and the sweep can see when.

**Ben, 2026-09-11:** *"Spending capital on ports within the Exploration round enables cheaper
overseas skirmishes. So building a navy and standing army that persists is important — these
degrade over time if insufficient capital is provided."*

Three objects, and each is a stock the treasury maintains:

| Stock | Built from | What it buys | If underfunded |
|---|---|---|---|
| **Port** | Capital, on a region with a port window | Lowers the cost of every crossing staged from it | Silts: its discount decays toward nothing |
| **Navy** | Capital, staged from a port | Crossing capacity, and contest of a crossing | Decays; a fleet is a running cost, not a purchase |
| **Standing army** | Capital, on top of inherited `army_stock` | Force that is *already raised* when a skirmish opens | Falls back toward what muster alone provides |

**Decay is what makes the choice a choice.** A purchase the treasury never revisits is a one-time
score; a stock with upkeep means a polity that over-builds is poorer every round afterwards and a
polity that under-builds arrives late. That is the pressure the phase wants, and it is a cost in
the world rather than a handicap in the scorer.

**Spend is ALLOCATED, not bought whenever affordable (Ben, 2026-09-14).** Once a round's upkeep is
paid, the treasury's investment is one scored choice among the port, the fleet, the standing army
and holding the purse — made in the upkeep step, not as a verb competing with a campaign. Its
inputs are ones the polity already has: its creed's lean toward expansion or consolidation
(§ Two ways to be strong), its unmet wants across water, the Alarm its neighbours raise, and
whether its seat even has a port window. A polity that can afford all three stocks still builds
the one its situation asks for, so a navy says something about who built it.

**`port_q` is endowment, not a port.** The region field the Empire phase carries says the ground
*could* take a harbour. Building one is an act of this phase, paid for, and the two must not be
conflated — otherwise every coastal polity begins with the thing the phase is about acquiring.

**The port is why the skirmish is cheap and the home war is not.** A neighbour war is fought where
treaties bind and no port discount applies. A frontier skirmish is staged from a port the polity
paid for, against ground whose defender has no fleet. That price gap *is* the displacement of
conflict, expressed in the only currency the phase has.

---

## A good acquires a cultural preference

**Ben, 2026-09-11: Exploration "gives us cultural preference to goods."** A good is not equally
wanted everywhere, and which people wants which good is the difference between a trade map and a
distance map.

**Preference attaches to a CULTURE, not to a polity.** Cultures already cross polity borders as
shares on every region (`CIVILISATION.md`), so a preference carried by culture spreads with people
and survives conquest — which is what makes a market's demand a fact about who lives there rather
than about who rules there.

**It is derived, never rolled** (the standing rule: consequences, not dice). A culture's
preference for a good follows from what its ground never held and what its route exposed it to —
the same two inputs the want table already uses, read at culture grain instead of polity grain. A
people that walked past a good and never held it is the people that pays most for it.

**What the preference does in this phase is weight a want, not set a price.** The want table
(`CIVILISATION.md` § The directed want) is directed and unpriced; preference makes some directed
wants *stronger* than others, which is enough to rank where a fleet goes first. Turning a weight
into a price is Industrialisation's job.

**Preference is read LIVE, round by round.** Ground changes hands and routes open across the span,
so a preference derived once at 1200 would describe a world that no longer exists. It is derived
from the round's own state, by the same rule.

### A want ranks a campaign

**SETTLED (Ben, 2026-09-14): a want leans WHICH campaign a polity takes and WHOM it subjects — and
nothing else.** A campaign's prize leans up by the decider's own unmet want for the good the
target ground holds, weighted by its people's preference for that good. Subjection ranks the
natives a power could bind by the same want. Settlement is untouched: a people settles where
ground is empty and reachable, not where a good is.

**It is a lean on the prize, in the scorer's established idiom** — the same shape as the fear of
being next and the creed's appetite: one term reads a richer input rather than a second term being
added beside the score. A want never makes an unwinnable campaign attractive; it ranks winnable
ones.

**A want ranks; it does not displace (Ben, 2026-09-14, NR-865).** It would be tempting to read
the want as what sends conflict to the frontier, since a newly met people holds what one's ground
never did. It does not follow at the grain the phase works in: with a handful of goods, a
long-known neighbour lacks and holds the same goods as often as a stranger does, and trade already
relieves the wants a bound neighbour can meet without pointing the rest outward. So the want
decides between campaigns a polity could already win and reach; **where** it fights is decided by
cost — treaties at home, ports abroad, and deterrence (§ The shape of the age).

---

## The arms race reinforces peace near home and conflict far from it

**Ben, 2026-09-11:** *"military arms races which serve to reinforce peace near to home, and
conflict far from home."*

**This is deterrence, and the project already has its shape.** `../economy/ERAS.md` § The two
scalars carries a per-nation **Ceiling** (restraint) against a per-nation **Alarm** (how
threatened this nation feels by others' *visible* capability), with the explicit discipline that
neither is a new world object. The same pair, at polity grain, is what makes a standing army a
deterrent rather than merely an expense.

**Visible capability is the whole mechanism.** A fleet and a standing army are read by neighbours,
and a neighbour that reads them binds a treaty rather than tests them. The same fleet, pointed at
ground whose holder has none, meets no deterrent at all — so one purchase buys quiet at home and
opportunity abroad, which is exactly the displacement the phase claims.

**AND IT MUST NOT PRODUCE CALM (Ben, 2026-09-11, agreeing the reading is a RATIO).** The Empire
phase runs continuous conflict to 1200, and the obvious failure of this phase is to damp it
everywhere and hand Industrialisation a frozen map. `CIVILISATION.md`'s handoff requires
**fragmentation**, not peace. So the reading is neighbour-war rate *against* frontier-skirmish
rate, and a fall in both is a failure, not a success (§ What the phase is judged on).

**THE DISPLACEMENT MEASURED SO FAR HELD ONLY AGAINST A SATURATED ALARM (measured 2026-09-16, on an
unmerged reading kept for its finding; Ben archived the change; the saturation re-measured on main
2026-09-17, BL-1028: 89.7% of near-home reads at 1000 by 1660, and 88.5% over 1660 -> 1960, while
pooled displacement falls from 3.02 to 1.74).** With `visible_capability_reference`
at 5000, alarm read 1000 on 88.5% of near-home reads, so the deterrence weight acted as a flat
constant on every near pair, and the sprint-41 plateau at weight 525 was an artefact of that. Setting
the reference from the measured distribution instead (the 90th percentile of the other side's army
plus navy across near-home treaty reads, about 31,000 over 16 seeds) spreads alarm (reads at 1000 fall
to 11%) and the displacement the phase claims does not survive: pooled displacement 2.73 -> 0.50,
held seeds 3 -> 15, battle rate 69 -> 206 per century. Re-taken across weights 0 to 1000, no weight
restores pooled displacement above 1 (the best, 1000, reads 0.81). **So a discriminating alarm, read
through the weight alone, does not produce the claim above.** Whatever makes near-home peace and
far-home conflict true has to come from somewhere other than the deterrence weight's magnitude.

**Polities meet; what they meet never binds (measured 2026-09-17, BL-1028, 16 library seeds).**
Contact is not rare in the span: between 3 and 300 new contact pairs a world, 1,250 across the
library, by crossing or by inheriting a conquered polity's contacts. What fails is what follows.
**Not one pair first met after 1200 holds a non-aggression clause at 1660 (0 of 492)**, while 99.1%
of near-home campaign candidates are blocked by one — so treaties seal the old neighbourhood and
never reach the new. The campaign scorer does not read contact, so an unmet target is not "priced on
nothing"; it is simply worse ground to take, with less supply reaching it than reaches known
neighbours on every seed that has a near-home candidate in reach (15 of 16). When an unmet target does clear the threshold, another verb wins 88.2% of
those rounds, Invest most often. Continued past 1660 on Exploration's own forces, contact nearly
stops — 43 new pairs in 300 years across the library, none by crossing on half the seeds.

---

## Two ways to be strong, and the creed decides which

**THE PROBLEM, NAMED BY ITS AUTHOR (Ben, 2026-09-11):** *"so much of this is my euro-centric view
as an author, I want to consider how we can also strengthen larger empires which do not have an
interest in colonization / spreading faith."*

**A phase that only rewards crossing water has built an author's bias into its scoreboard.** The
seven readings would measure colonies and fleets, a continental realm would have no way to succeed
at the phase, and the world would converge on one strategy — which is the flat map
`GENERATION_STRATEGY.md` § Asymmetry is the deliverable exists to prevent.

**So there are TWO strategies, and both must pay.**

| | **Expansion** | **Consolidation** |
|---|---|---|
| What it buys | Ports, hulls, subjects, trade provinces | The road ladder, dense internal throughput, its own ground worked harder |
| What it wants | Goods it lacks, from strangers | Reach and yield it already owns |
| How it grows | Outward, across water, toward people | Inward, absorbing neighbours and their peoples |
| How it fails | Outruns its network, subjects refuse renewal | Stagnates; nothing new enters, and the frontier passes it by |

**NEITHER IS A FLAG, AND NEITHER IS CHOSEN (Ben, 2026-09-11):** *"Ideally we can let candidates
work on either consolidation or expansion depending on their historical creed."* The disposition
is **derived from what already happened to these people**, which is the standing requirement for
any force in this project.

**The material is already recorded, and nothing new needs inventing.** A pantheon's gods carry
`zeal` — how much a god relishes battle — and `dominion` — how surely it expects to prevail
(`../lore/CREEDS.md`). And `sea_legs_q` is earned from three deeds: a coastal cradle, a crossing
in the migration, and a sea or storm god. **High dominion with no sea legs is the continental
consolidator**, already derivable; deep sea legs with a universalising creed is the coloniser.

**The two leans are read on one scale: each as a rank among its world's living polities, never
against each other.** The consolidator lean is a product (dominion held back by sea legs), so it
is at most its smaller factor; the expansion lean is a mean (sea legs with zeal), so it is at
least its smaller input. Compared raw, a middling people reads expansionist with nothing
seafaring about it, and on a real spread no people reads consolidator at all. So a realm is a
consolidator when its consolidator lean ranks in the top third of its world, an expansionist
likewise, and a realm may be both or neither. The spend scorer reads each lean only as that
rank, and the sweep classifies by it (BL-1016, CREED_CLASSIFIER_COMPARES_ONE_SCALE).

**If the mapping turns out not to separate them, the fix is upstream, not here** (Ben, 2026-09-11:
*"if we have to force this outcome, then we should be looking back at the Empire phase to ensure
creeds map well to our phase"*). Forcing a disposition the creeds do not support would be a term
inside an actor, and that stays declined.

---

## The engine is shared

**SETTLED (Ben, 2026-09-11): "let's use the same engine — if needs be we can revise this later."**
Exploration runs on `history_sim`, over the same region table, the same polity table and the
same round loop. It is not a second simulator.

**Two things the shared engine does not already have, and both are honest additions rather than
forks.** A **round-level upkeep step** — the treasury earns, then pays its stocks, then invests —
because every mechanism above depends on a recurring bill. And **objects with a term** — treaties
expire, which nothing in the Empire phase does.

**The verbs are the Empire verbs, and a treasury spends beside them.** Settle, Invest,
Consolidate and Campaign carry over unchanged. What this phase adds is the ability to spend on a
port, a hull or a garrison, and to bind. The first three are one scored allocation inside the
upkeep step (§ Force persists now); a binding forms by the treaty test (§ What a treaty is). The
tree node a polity invests in is still weighed on the scorer's one shape (`trees/TREES.md`
§ The scorer).

**If the shared engine turns out to be the wrong frame, the tell will be a reading it cannot
move** (§ What the phase is judged on). Revising it then is Ben's call and is explicitly left
open; forking it pre-emptively is not.

---

## The tree

**The Exploration tree is the third of four** (`trees/TREES.md`), and its existence is Ben's
ruling of 2026-09-11: *"Exploration and industry use different trees."* The chain is
Colonisation → Empire → **Exploration** → Industry.

**The Empire tree's rim milestone — *The Enforceable Promise* — opens THIS tree**, not the
Industry tree, and the Industry tree's root is gated behind Exploration's own rim. That is the
whole structural consequence of the ruling, and it is why a 1960 spread is now manufactured across
two intervening trees rather than one.

**Sized to 460 years**, which is roughly a quarter of the Empire phase's span, so the tree is
correspondingly shallower — three rings, four branches, three milestones (`trees/TREES.md`
§ Sizes). Its concerns are the phase's: hulls and crossings, the charter and the treasury,
the port and the garrison, and what a people does with a good it did not grow.

Its nodes, its forks and its scorer terms are `trees/EXPLORATION_TREE.md`'s.

---

## What the phase is judged on

Readings taken at **1660 CE**, all over a **seed spread**, never per world — the same
distributional discipline `CIVILISATION.md` § What the closure is judged on applies to the
Empire handoff.

| Reading | What it must show |
|---|---|
| **Displacement — A RATIO** | Neighbour-war rate falls *relative to* frontier-skirmish rate. **A fall in BOTH is a failure**, not a success |
| **Conflict persists** | Total conflict at 1660 is lower than Empires' but not near zero; a frozen map fails the handoff as surely as a hegemon does |
| **Both strategies pay** | Consolidators and expansionists both present among the strongest realms, and each traceable to its creed |
| **Treaty depth** | Treaties standing at 1660 with years still to run, not a churn of one-round bindings |
| **Colonial asymmetry** | Some polities hold subjects, most hold none, and at least one subject sits under a distant overlord |
| **Subject friction** | At least one subject's wants point somewhere its overlord's do not |
| **Fleets** | Navies exist, unevenly, and at least one polity that built one let it decay |
| **Treasury spread** | Wide, and correlated with the inherited corridor network rather than with size alone |
| **Throughput** | Corridors carrying materially different volumes, with the road ladder visible in the difference |
| **Preference** | Goods wanted differently by different cultures, with the difference traceable to route |
| **Trade** | Flow crosses between polities on different landmasses, unevenly — some pairs carry most of it, most carry none — and every flow stands on a trade-access clause (Ben, 2026-09-14) |

**THE FIRST TWO ARE ONE TEST AND MUST BE READ TOGETHER.** Displacement alone can be satisfied by a
world that simply stopped fighting, which is the failure mode Ben named on 2026-09-11 and the
reason the ratio replaced the original one-way reading.

**A reading is a requirement, not a target.** A seed that refuses one is a legitimate world; a
*spread* that refuses one is a phase that did not do its job.

---

## What this phase hands Industrialisation

`INDUSTRIALISATION.md` owns what happens next; this is the list, and nothing else crosses.

**Ben, 2026-09-11, on what that phase needs it for:** *"the aim of [Industrialisation] is to spawn our
saturated web of companies and corporations... the main levers we pass are going to be about how
much of easily accessible resource has been actually spent, and what prices different goods have,
and what preference different cultures develop for these goods."*

- **Treasuries**, one per polity at its capital — the capital a firm will be chartered against.
- **Scarcity signals**, per good per market — what Industrialisation resolves into prices. Not prices.
- **Trade flows**, per seller, buyer and good — the trade relationships a firm inherits.
- **Corridor throughput** — which lines carry volume, and at which rung of the road ladder.
- **Cultural good preference** — the demand shape a price field resolves against.
- **The overlord graph** — who holds whom, and on what tribute terms.
- **Standing treaties and their remaining terms** — the diplomatic position 1660 opens on.
- **Ports, navies and standing armies**, at whatever level upkeep left them.
- **The contact and want tables**, grown: a world that has met itself, mostly.
- **Exploration tree masks** — what each polity brings into the Industry tree, which every living
  polity enters at 1660 (`trees/TREES.md` § Milestones, and how a tree unlocks the next).
- **The grudge table and the surviving network**, grown across the span and filtered over this
  span's dead, exactly as the Empire handoff filters its own.

**The list is a struct, and it has readers before Industrialisation exists.** Every item above is one
field of the handoff value, checked by a validator, on the same footing as the Empire handoff
(`GENERATION_STRATEGY.md` § What crosses each handoff). Wherever the span ran, world setup seeds
sentiment from ITS grudges and stamps roads from ITS surviving corridors. A campaign that opens on
the 1660 political map must not open on 1200's resentments and 1200's roads.

### Spend is estimated at the end, not accumulated throughout

**SETTLED (Ben, 2026-09-11): depletion is RETROFITTED.** *"I would prefer if we retrofit this, and
come to a sensible estimate at the very end of [Industrialisation]. For generation you are right in that
keeping items abstract is best policy."*

**Where it lands is already built.** The campaign's `tile_component::resource_remaining` is a
finite reserve, drawn down by extraction and tapering output as it empties, **seeded at
generation** to richness × a factor. So spend does not need a new accumulator anywhere in the
history sim — the two economy phases set that seed instead of a constant, and ground worked for
centuries hands the campaign a thinner reserve.

**THE PRICE OF RETROFITTING, STATED.** An estimate computed once at 1960 can never be *checked*
against what actually happened, because nothing recorded it. That makes it a **stated formula**
rather than a measurement, and the formula must be written down as one — named inputs, named
arithmetic — or it becomes a fudge factor nobody can audit. Its inputs are the abstract ones the
sim already holds: how long ground was held and worked, its dominant class, and the throughput
that ran off it.

---

## Open questions

- **What displaces conflict, if not the deterrence weight — RULED NOT THIS PHASE'S (Ben,
  2026-09-17, NR-888).** Exploration is not revisited for the saturated alarm or for new contacts
  that never bind (§ The arms race). The displacement this phase measures is carried forward as
  found; `INDUSTRIALISATION.md` owns both the force that sends a patron abroad (§ 3) and the force that
  binds far pairs (§ Far pairs meet and bind, and this phase makes them).
- **What earns the treasury, in terms.** § Capital arrives names the four sources; the arithmetic
  is a measurement, and it must be set from a sweep that produces both rich and poor survivors,
  never from a number picked to make one seed solvent.
- **Where the treaty threshold sits** — how much a binding must be worth before both parties
  independently take it. Too low and the map freezes; too high and the displacement never happens.
  Same discipline as the opposition bar (`CIVILISATION.md` § Open questions).
- **Whether a colony can itself hold a colony.** The overlord link permits a chain; whether the
  phase should is unsettled, and the answer decides whether a subject is an actor or a leaf.
- **How preference interacts with assimilation.** A culture absorbed into another carries its
  preferences somewhere; whether they blend, persist or lapse is unowned.
- **Whether the 460-year span is enough rounds** for a tree, a treasury and a fleet to all turn
  over believably. A measurement owed before any constant is tuned.
- **The depletion formula itself** — which abstract inputs, and what arithmetic. It is a stated
  formula by construction (§ Spend is estimated at the end), so writing it down IS the work; an
  unwritten estimate is the failure mode.
- **Where the deterrence pair's thresholds sit.** The arms race borrows ERAS.md's Ceiling/Alarm
  shape at polity grain; what makes a neighbour bind rather than test is unmeasured.
- **Whether both strategies stand among ONE world's strongest realms.** Read on one scale (§ Two
  ways to be strong), the creed axes do separate consolidators from expansionists across a world's
  polities, and each reaches a world's richest three somewhere on a 16-seed spread (2026-09-16).
  What that spread did not show is a consolidator and a *different* expansionist among one
  world's richest three. Two facts bear on it and neither is yet a cause. A world's richest realms
  often carry identical leans, as kin sharing one creed would, and so one disposition. And every
  living realm's people on the spread carried deep sea legs, so a consolidator is one only
  relative to its world — the landlocked people that section describes does not occur. The
  upstream question is therefore specific: whether
  `sea_legs_q` is earned too easily for "no sea legs" ever to be true. If the creed axes stop
  separating a world's polities at all, the fix is upstream in the Empire phase, never a flag here.
- **What this phase's treaty graph hands the world war.** Ben, 2026-09-15, permits a world war in
  Industrialisation and never forces it; `INDUSTRIALISATION.md` § A world war is permitted, never forced
  reads its spread through the mutual-defence clause this phase creates. How densely great powers
  are bound at 1660 is therefore one of that war's causes, and it is measured, not tuned for.
