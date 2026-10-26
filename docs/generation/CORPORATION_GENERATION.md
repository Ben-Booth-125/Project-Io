# Project Io — Industry Generation

> **Settles:** industry generation — what a nation's ground opens with · what a charter is and
> how a centre's charter budget is spent as buildings · how a charter's focus is read · where
> buildings are placed, and the rules every placement passes (own ground, chain-feasible, sized
> to its inputs, bounded by want) · what opening stock reaches the shelves · how the seat is
> offered over the majors · what generation deliberately declines to seed · which firm-grain
> passes retired (Ben, 2026-10-23).
> **Not here:** how a nation *decides* anything (../ai/AI_OPPONENT) · how the nation was made and
> which nations are majors (NATION_GENERATION) · how the budget is built and priced
> (INDUSTRIALISATION § 1) · what its money does thereafter (../economy/FINANCE, ../politics/NATIONS).
> **Confused with:** ../ai/AI_OPPONENT.md, NATION_GENERATION.md, INDUSTRIALISATION.md,
> ../economy/FINANCE.md.

**This doc is industry generation: what a nation's ground opens with.** The filename is kept
because the router and many docs cite it; its old subject, the corporation, is retired.

**There are no corporations and no companies (Ben, 2026-10-23).** Ben: *"We can remove the
corp/company layer entirely. Just have nations building their own buildings."* Every building
generation places is owned outright by the nation whose ground it stands on, in every sector
(`../politics/NATIONS.md` § What a nation owns). The champion, the specialist, the background firm,
the HQ and the ownership class retire with the firm. Where this doc kept a firm-grain pass, it now
says so in one line.

Behaviour is not generation's. The scored-utility build layer that runs every non-player nation's
economy is `docs/ai/AI_OPPONENT.md`'s (§ 11). Generation's contract with it is § Generation seeds
no behaviour below.

---

## Design principles

**One path: the charter budget (Ben, 2026-10-23).** The legacy path that drew a fixed roster of
eight corporations by weighted nation draw is retired. Every world's industry is the landscape
search spending each population centre's **charter budget** (`INDUSTRIALISATION.md` § 1; § This
phase sets budgets; the search spends them). What a centre's budget bought as firms it now buys as
**buildings owned by the centre's nation**.

**A charter is the unit the budget buys.** A charter is one priced set of buildings for one good,
placed together around a centre — the holding set a firm used to stand on. It owns nothing and
outlives its placement only as a record (its founding year and region, § Pass 6). The buildings are
the nation's.

**Own ground binds every placement (Ben, 2026-10-23).** A charter is placed only on its centre's
nation's tiles — never another nation's, never ground no nation owns (`../politics/NATIONS.md`
§ Own ground). That binds production, the logistics sources (hubs, stations, ports) and rail alike.

**Starting assets reflect tile reality.** A building lands where its ground supports it: an
extractor on a deposit, a processor where its inputs are within reach, a depot near traffic.
Placement uses the same tile data as the economy.

**Density follows the city.** Where industry accumulated, the ground opens thick with plant; where
it was spent or never lit, thin (`INDUSTRIALISATION.md` § 1).

---

## Generation pipeline

### Pass 1 — Nation assignment

**Retired** (Ben, 2026-10-23). It drew each corporation's home nation; a building's nation is
simply its ground's, and a charter's is its centre's.

### Pass 2 — A charter's focus

A charter carries an `industrial_focus` — extraction, processing or trade — which shapes its
building mix (§ Pass 3). The per-firm reading of focus retires with the firm; the derivation
survives at the charter's grain.

| Focus | Building mix |
|---|---|
| `extraction` | Mine, well or quarry on a resource-rich tile, with at most one attached processor |
| `processing` | Processors near their feed, the authored pair |
| `trade` | A depot near a high-traffic zone |

**Focus is DERIVED from the charter's region** (BL-219, corporation focus from settlement): the
region of the centre whose budget pays for it. There is no authored weighting table.

- **The ancient endowment sets the base tier.** An ore or farm region emits extraction, an energy
  region processing, a harbour region trade.
- **Movement up the value chain sets the shift.** Early industrialisers moved up the chain and late
  ones stayed extraction-heavy: a region that lit its furnaces before the world median moves one
  tier up; one that industrialised late, or never, stays raw. This reads the same
  industrialisation-timing scalar the settlement pass produces for the ideology axis
  (`NATION_GENERATION.md` § Pass 4).

**Per-region, not per-nation.** Two charters in one nation differ because their regions differ,
while each stays internally coherent.

The legacy path's world-level focus-diversity reroll and its no-settlement fallback draw retire
with that path (Ben, 2026-10-23).

### Pass 2b — Ownership class

**Retired** (Ben, 2026-10-23). It derived whether a firm was public, private or closed — whether
its books were open and whether it could be bought. No firm remains to have books or be bought.

### How the charter reaches ground

**Retired with the ownership class** (Ben, 2026-10-23). This was the diffusion of the Charter Act's
enforceable promise, by the sealed-oath god's culture and by trade, which set the class. The lore of
the Charter Act is `../lore/HISTORY.md`'s; nothing in industry generation reads it now.

### Pass 3 — Starting asset placement

Each charter places a **lean, focus-coherent set of buildings** on its nation's ground. The set is
small and thematically tight — a foothold in one slice of one chain.

- **Count — lean and focus-shaped.** The ranges (inclusive, `holdings_range` in
  `corporation_generation.cpp`) are **extraction 3–4**, **processing 2–3**, **trade 1–2**. Ground
  too cramped or deposit-poor yields fewer — the count is a ceiling, not a guarantee.
- **Industry gathers where logistics is cheap (Ben, 2026-10-26).** A processing or trade charter
  anchors at its nation's **capital** first, where the Logistic Point chain is rooted and points
  are cheapest; then at the nation's **market centres** in order of chain distance to the capital,
  nearest first, where stacked points pay. Ties go to the lower tile index. The anchor tile is
  searched in a window around that anchor, then its region; "inside a region" is
  `nearest_region`: regions are anchor points, not stored tile sets. The window and region search
  are BL-283 (region-anchored holdings); what changed is which place a charter anchors at.
  **Chain distance** is the road-network cost from the capital — each step priced by the road tier
  on the edge it crosses, as the Logistic Point road field prices it — because the seeding lays the
  chain out from the capital along exactly that network (`../economy/LOGISTICS.md` § Where
  generation puts the sources). A market centre the capital's roads do not reach comes after every
  reached one. Only the capital's body counts. A nation with neither a capital nor a market centre
  on its ground anchors at its own centre, the one place it has. An extraction charter keeps its own
  centre: it stands on its deposits.
- **An extractor takes the deposit that is rich and served (Ben, 2026-10-26).** Extraction must
  stand on its deposit, so it cannot move to the capital. Among a charter's candidate deposits it
  ranks each by **richness × Logistic Point cover** at that tile, so that of two equally rich
  deposits the one the chain reaches wins. Where neither is reached the richer wins; then the lower
  tile index. The anchor is ranked, not drawn. A Well or Wharf site has no deposit to rank and keeps
  its draw (§ Pass 6, the dig ladder).
- **What cover means at placement (decided 2026-10-26; the reading delegated to the implementer).**
  No Logistic Point source stands yet when a charter is placed: the fold seeds them after every
  charter, and the site roads join each building to its network after that. So the cover read at
  placement is the cover the seeding is **bound** to give before it reaches for any consumer — a
  source on the nation's **capital**, and every one of its **market centres** covered at full
  strength by the chain laid out from it (`../economy/LOGISTICS.md` § Where generation puts the
  sources, the order it seeds in). A tile's cover is the road reach at its distance from the nearest
  of those points: full strength to 6, nothing by 12. The distance is walked as the road field walks
  it, each step priced by the road tier on the edge it crosses. Off the road, a step over the
  nation's own dry ground costs a Track's, the crossing the site-road join will lay to the building
  (`../economy/LOGISTICS.md` § 4, *A site's road joins its own nation's network*). Roads do not
  move during the walk, so a nation's field is read once.
  It is honest and conservative. It never credits a point the seeding may not place. It does not
  foresee the hubs a chain lays between the capital and a far centre, nor those laid outward to a
  consumer. Generation covers every consumer in the end, so the finished world's cover cannot rank
  deposits: measured, every extractor opens at full cover.
- **Cluster around the anchor.** The remaining slots fill the nearest valid tiles of the same nation
  that still have room, by grid distance, the anchor's own tile first — so the cluster fills a tile
  before it spreads to the next (below).
- **On own ground, every rung.** No anchor rung and no cluster slot leaves the nation's tiles (§ Design
  principles). A charter that finds no ground there is counted unspent, never scattered.
- **Mix follows focus.** An extraction charter places extractors on its best-served rich deposits (above), a
  processing charter pairs processors with feed, a trade charter a depot. The patterns themselves
  are an open item to ground in post-war industrial history (§ Open items).
- **Validity-gated.** Every placement is gated by `placement_rules::can_place` (the shared
  placement seam), so no building lands on invalid terrain or a zero-deposit tile.
- **Chain-feasible — a processor needs a reachable producer of every input (Ben, 2026-10-04;
  a HARD RULE, not a search score).** No processor is placed, by any pass, unless every input of
  its recipe has a producer **within reach** of its market: an extractor on a deposit of that good,
  or a processor making it. "Within reach" is the reach goods actually travel between markets
  (`../economy/SUPPLY.md`), not grid distance. A processor with no feasible site is **not
  placed**; a chain with no feasible site is absent from that world rather than present as idle
  plant. An idle building the player inherits reads as a broken economy, where an absent chain
  reads as a world that lacks it. BL-1185 (chain-feasible placement) owns the work.
- **A food processor reads real household demand (Ben, 2026-10-14).** A processor that takes
  agricultural produce is placed only where the produce left after households' real bids — not the
  base basket — covers its draw; households clear first, so a plant sited on food they will eat
  starves.
- **Sized to its inputs — a processor needs SPARE reachable supply (Ben, 2026-10-07; BL-1233,
  processors to inputs).** A producer within reach is necessary, not sufficient: every placement
  puts a processor only where the **spare** reachable output of each of its inputs covers its draw
  at `t_idle` — a fifth of a full run. Spare is the producers' output in reach less the draw of
  every processor already standing there, **and less what households and the background pull draw
  there** (Ben, 2026-10-09; `../ai/AI_OPPONENT.md` § 11, processors sized against every buyer; in
  generation, before any clear, those draws are the household and background baskets whole at base
  — accepted by Ben, 2026-10-09, at the measured cost of a smaller economy: about a third fewer
  processors placed, 15% fewer charters, 29% less play income, and 96% of processors running at the
  handoff with 2% starved), judged at the labour a new plant is judged at, the same test the
  play-time scorer uses (`recipe_inputs_obtainable` with its supply clause). A good that fails is
  passed over at that centre and retried once a charter producing its input lands, by the same retry
  the chain-feasible rule uses. Measured before the rule: one rare-earth site yields about 6 units a
  tick against a REE alloy plant's 32, and the walk placed about one site per plant, so bodies made
  19–62% of the raw ore their processors wanted.
  - **Within reach, exactly — the dispatcher's own gate, with the destination short** (refined
    2026-10-05). The producer's market is the processor's market, or both of these hold for the
    pair: `price_market_export_leg(producer market, processor market)` is viable, and
    `R × base_dest − haul > (1 + dispatch_margin) × base_src`. The dispatcher ships because the
    destination is short and bids up, so generation reads the gate with the destination at its
    shortage price, its reservation ceiling: `R` is `reservation_mult`, or `ceil_mult` where the
    reservation is off (≤ 0) — reach never collapses to one market. Each base is that market's
    **own** base price for the input, and `haul` is the leg's per-unit cost. **This is one
    definition, not a copy:** reach and the producer test are `../ai/AI_OPPONENT.md` § Build only
    what runs (the play-time scorer's), asked here in their generation form (no tick's production
    report: nominal output, base prices).
  - **When reach is read.** A pass reads the logistics nodes once, at its start, and prices each
    market pair the first time a placement asks, memoising it for the rest of the pass. A port
    placed later in the walk widens a pair only if that pair had not been asked yet: placement may
    miss a lane the finished world has (conservative), and never assumes one it lacks.
  - **Which producers count.** The buildings standing when the processor's recipe is decided: the
    base installations, every charter placed before this one in the walk's order, and the charter's
    own buildings. A charter's whole set is placed before any of its processors is checked, so its
    own feed counts. Nothing placed afterwards is foreseen.
  - **The recipe bends before the building goes.** A processor takes the feasible recipe nearest its
    feed: its **own** charter's buildings first, then its own market, then reach, ties to the band's
    recipe order. A processor with no feasible recipe is unplaced. A charter whose **anchor** is
    unplaced is refused whole, so the building behind it is never promoted in its place. A charter
    left with nothing — or a processing charter left with no processor — tries its next anchor rung,
    and with none left it is not placed; its price books under its own unspent reason,
    `chain_infeasible` (the windows held ground, none of it within reach of a chain), distinct from
    `window_exhausted` (no ground at all).

**Buildings stack on every tile (Ben, 2026-10-26).** A charter may place buildings on any tile up
to that tile's stack cap; the cluster fills a tile before it spreads to the next. The cap is the
placement rules' own, on two axes that never share a slot: extraction sites of one target up to the
deposit's richness-bound stack capacity, and every other building together up to the ground's
non-extraction cap (`../economy/PRODUCTION.md` § Building stacks). A stacked site yields its rank's
share, as it does in play, and every generation reading of nominal output counts it at that share,
so the spare-input rule never credits supply the stack will not make. The one-site-per-tile limit on construction in play is a separate
rule (`../economy/PRODUCTION.md`); generation's opening buildings stand complete.

### Pass 3b — HQ designation & border range

**Retired** (Ben, 2026-10-23). It designated a corporation's seat building and an influence range,
the foundation of the corporate border. A nation's seat is its capital and its border is its
territory.

### Pass 4 — Financial profile

**Retired** (Ben, 2026-10-23). It drew each corporation's starting capital. A building's money is its
nation's treasury, which nation generation opens (`NATION_GENERATION.md`; `../politics/NATIONS.md`
§ 2).

**Pre-game operating history survives, at the nation's grain.** The economy is run forward a fixed
number of **pre-game ticks** before turn one — phase 6's single validation run of the winning
landscape (`k_campaign_settle_ticks`, `src/world/campaign_settle.hpp`; the length and its
measurement are `../economy/ERAS.md` § The opening position; BL-978, warm start retired, owns the
work). So every nation enters turn one with a **multi-tick operating history**: shelves already moved
by production, wages and trade, and a treasury moved with them. The headless `--verify` path stays
deterministically cold (no pre-game ticks) so generation audits remain reproducible.

### Pass 4b — Starting stockpile

An **opening resource stockpile** is generated for each charter (`generate_starting_stockpile`,
`corporation_generation.cpp`; BL-116, generated starting stockpile), so build, production and trade
have materials from turn one:

- **Per-focus weights** over the seven-resource prototype subset: extraction hoards the raws it
  mines (e.g. iron ore ×2.0 against trade's ×0.5); processing pairs feedstock with refined output;
  trade carries finished goods and thinner raws.
- **Magnitude**: base 100 units × weight. The capital scalar that sized it per firm retires with the
  financial profile (Ben, 2026-10-23).
- **A seeded per-resource jitter ×[0.85, 1.15]** varies each campaign without disturbing the focus
  ordering. Deterministic: one draw per prototype resource in fixed order.

**The opening stock goes on the shelves (Ben, 2026-10-10; `../economy/MARKETS.md` § The shelf
economy).** No nation holds a stockpile of goods (`../politics/NATIONS.md` § What a nation is). The
stock this pass generates is placed on the shelves of the markets the charter's buildings sit in. It
is the market's from then on; nothing holds it back from sale.

### Pass 5 — Naming

**Retired** (Ben, 2026-10-23). It named each corporation from a template bank (`make_corp_name`):
a phoneme identifier with an English structural type word. With no firm there is nothing to name;
buildings carry their nation.

### Pass 6 — The charter walk (formerly background firm generation)

The walk fills the home economy: centre by centre, it spends each charter budget on charters, each
placed under Pass 3's rules on its nation's ground. Every building it places is the nation's.

**The legacy per-resource cap retires with the legacy path (Ben, 2026-10-23).** It capped a body at 8
firms per demanded good, which made density a function of the basket's breadth (Ben, 2026-09-06;
no 0.90 production-to-demand ratio ever bound). On the one path that remains, the per-good cap
**scales with the body's charter capital** by a square root, under a density ceiling per good
served (Ben, 2026-09-18, NR-889; `INDUSTRIALISATION.md` § 1). The base of 8 survives as the
square root's base.

**Density follows the city (Ben, 2026-09-15).** Each population centre arrives with a **charter
budget** — its unspent industry points — and the landscape search spends it on charters around that
centre (`INDUSTRIALISATION.md` § This phase sets budgets; the search spends them). Breadth still
decides which goods a charter can serve; the budget decides how many stand where.

**The walk is a one-shot.** There is no economic settle to recur through. `GENERATION_STRATEGY.md`
§ The eight phases replaces the warm start and the settle with **phase 6's directed static
search**: a settle asks *what survives whatever generation happened to place*, a search asks *which
placement is worth handing over*. This pass produces the candidate rosters the search ranks.

**Every building is a full participant.** Once placed, a building is its nation's like any other,
and the nation's build scorer runs it — build, demolish, recipe, trade — on the same layer for
majors and minors (Ben, 2026-10-23; `../ai/AI_OPPONENT.md` § 11). The player's nation is the one
exception: nothing builds on its ground for it (`../CONCEPT.md` § Player identity).

**Chain-feasible, as Pass 3** (BL-1185, chain-feasible placement). A charter for a good is placed
only where some recipe for that good has every input produced within reach (§ Pass 3,
"Chain-feasible"). Its processors take the gap selection's recipe where that is feasible, and
otherwise the good's next recipe by output that is; a processor with no feasible recipe is unplaced,
and a charter none of whose processors can make its good is not placed for it. The walk is
**ordered**, so a chain can be absent because its upstream had not landed yet. A good refused this
way is passed over at that centre until the centre places a charter that produces one of the good's
inputs; an unrelated charter changes nothing. Where nothing feasible is left, the points book as
`chain_infeasible`.

**A water-gap or produce-gap charter digs its good** (Ben, 2026-10-05, NR-973; BL-1197, gap firm
digs the gap; BL-1208, generation sites wharves). The two goods with a **depositless tier**
(`../economy/PRODUCTION.md` § Extraction buildings) each have a dig ladder, and an extraction
charter for either gap stands only on that ladder's ground:

- **Water** — a **Well** site (fresh-water adjacent, no ice deposit) first, so the water tends to
  land where people live; else an **ice deposit**.
- **Agricultural produce** — a **produce deposit** (a Farm) first, the good's own ground; else a
  **Fishing Wharf** site (coastal, no produce deposit).

A depositless site is drawn at random with each site weighted by its habitability plus 0.05; a
deposit is ranked by richness × cover (§ Pass 3). Each tier is tried across every window the charter may anchor in before the
next. Where its windows hold no ground for the good at all it places **nothing** — no fallback to
another deposit, which would spend the slot on a good nobody chose. The good is passed over for the
rest of that centre and the next good is taken. Every other gap good keeps the richest-deposit rule.
The ladder reaches exactly the ground `body_producible` counts (below).

**A processor's inputs are wanted too (Ben, 2026-10-05; derived demand).** What the walk fills is
not only final demand but the **input demand of every processor standing or placed**: a steel
works' iron ore and coal are wants of the body like any other, entered into the same gap and filled
in the same turn. So a chain is absent only because its ground is absent. The input demand is read
in the generation form (nominal rates) and recomputed as charters land. The goods with demand (G),
fixed before the walk, hold the processors' inputs too: the input demand of those standing, and the
**anchor-route closure** of G (main session, 2026-10-05). Each member's anchor route is its cheapest
in-band recipe by marginal cost per unit of primary output (`../economy/PRODUCTION.md` § The recipe
margin anchor); its inputs join G, to a fixed point. A raw's anchor route is extraction, so the
closure stops there. A closure good no processor yet wants is not short, so the turn passes over it.

**G holds only goods the body can produce** (main session, 2026-10-05): a raw with a deposit on the
body's land (water also wherever a Well site stands, produce also on a coast), or a good some recipe
makes from producible inputs, to a fixed point. If a good outside it is wanted and short when a
centre can place nothing else, the rest of that centre's points book as `unproducible`, distinct
from `late_shortfall` (a producible good outside G that the walk itself made short). |G| is
`goods_in_g`.

**Derived demand counts the prospective draw of a processor refused for want of spare input (Ben,
2026-10-07; BL-1233, processors to inputs).** A processor the sized rule turns away enters its
input's demand, so the walk places the extraction that would feed it, and the processor is placed on
the retry. Without it the two rules deadlock. A processor refused for any other reason is never
counted.

**No processor is placed beyond its output's want (Ben, 2026-10-09; BL-1217, G1 plants
running).** Every placement of a processor is bounded by the want for what it makes: final demand
plus the derived demand of what stands or is placed. A processor laid with no recipe is not placed
when its default output is unwanted (Ben, 2026-10-09). **Two exceptions (Ben, 2026-10-09,
measured):** the world's **pre-authored installation** keeps its default — the one steel maker the
chain-feasibility test can see at the start — and a **processing charter keeps its second works**
once the first covers the gap (measured: 120 more plants running at the handoff and 8% more play
income).

**A charter carries its founding year and its origin region** (Ben, 2026-09-24). The origin is the
region of the centre whose budget paid for it, and its year is read off the Industrialisation span:
a region's k-th charter takes the year of its k-th crossing of the running price
(`INDUSTRIALISATION.md` owns the crossing). On the buildings it placed, the fields record when and
where the nation's industry there began; what reads them is the reading surface's business.

**What Pass 6 does not do.** It seeds no behavioural state (§ Generation seeds no behaviour).
Market-clearing consequences of its output are `docs/economy/MARKETS.md`'s.

---

## The player's nation

**The player is a major nation and builds its own buildings (Ben, 2026-10-23; `docs/CONCEPT.md`
§ Player identity).** Generation makes the player's industry exactly as it makes a rival's: the walk
places charters on the player's ground from its centres' budgets, and nothing is placed, sized or
favoured for the player. Which nations are majors is `NATION_GENERATION.md` § The nation count is a
target. The seat is shaped for up to six humans, one major each, and the prototype seats one
(`docs/ui/STARTUP.md` § The seat).

### The spawn shortlist, and the seat

**The player picks the seat over the majors (Ben, 2026-09-09; re-subjected 2026-10-23).** The seat
canvas at Begin offers **every major nation**, over the landscape the search selected
(`docs/ui/STARTUP.md` § The seat), ranked by the static score below; the ones below the viability
floor are **marked, not removed** (Ben, 2026-09-24). A weighted draw survives only for a path with
no player to ask (a headless or autostarted run), which says so. The champion each major was read
through retires (Ben, 2026-10-23); a major is read through its own buildings.

1. **Generate** — the walk, exactly as above. No nation is the player's yet.
2. **Search and settle in spectate** — phase 6 scores and applies the landscape, then its single
   validation run ticks with **no seated nation**, under spectate. **This is generation's last act,
   and it runs inside the Industrialisation round's worker** (Ben, 2026-09-24), so an adopted world
   and a cold build open on one state.
3. **Shortlist** — every major, ranked by the static landscape score of its ground.
4. **Seat** — the player picks one (the `take_seat` command; a path with no player to ask draws one
   against the world seed), and the player flag and `world::player_entity` are re-pointed onto it.
   The same world and the same pick seat the same nation in the same state.
5. **The seat opens with a clean slate of construction (Ben, 2026-10-07; BL-1206, seat day-1
   profit).** During the settle the seated nation's scorer started builds the player never chose.
   At the seat, every building of the seated nation still under construction is cancelled and
   everything paid into it — materials, flat build cost and the construction capacity it drew — is
   refunded to its treasury, as the recorded sum of those charges. Finished buildings stay.

**Spectate is the machine this needs, and it already exists.** BL-409 settled that under spectate
the no-auto-act prohibition has *no subject*: every actor evaluates on the same staggered cadence,
and admitting one more shifts no rival's cadence slot. The reorder made every seed's opening
position change once, a cost Ben took knowingly on 2026-08-26.

**The viability floor reads the ground, not a trading record** (Ben, 2026-09-16, NR-881; BL-1020,
the seat floor reads the static score). No trailing net exists to judge in a settle that is still
ramping. What *is* knowable is what phase 6 already scored (`GENERATION_STRATEGY.md` § The eight
phases, phase 6): for each of the nation's buildings, the **viability** of the market it clears
against — `actual × balance` — and the seat's score is the mean over its buildings.

- **The gate:** a major qualifies if its score is above zero — at least one of its buildings stands
  in a market where the roster closes a chain *and* some resource sits inside the pin band.
- **The order:** highest score first, entity id breaking a tie.
- **The trailing figures stay, as information.** The settle's closing treasury and the trailing net
  over the last 8 filed quarterly returns are read for every candidate and shown on the seat card.
  They never gate and never rank.

**The headless draw is WEIGHTED, not uniform.** Ben, 2026-08-26: *"mostly random for now, targeted
towards population centres and processing, rather than extraction."* The weighting is a **bias and
never a second gate**:

- **Processing over extraction.** A major whose buildings include a processor weights up — the
  **depth** lever, for variety of play, not wealth.
- **Population centres.** A major whose buildings sit on or near populated ground weights up.

*Depth, not wealth.* A processor is the **deeper** start, not the richer one: it earns **less** per
tick than the extraction site it replaces (BL-436, processing under-earns extraction). Nothing in the
shortlist or its surfaces may call it the richer opening while that holds.

**An unmet floor stands.** If no major clears, the first-ranked is seated and the world records that
the floor went unmet — a viability signal to be read, not hidden.

**What this retired.** The starting-corp selection stage (`app_screen::choosing_corp`) and its
*Surprise me* press; `docs/ui/STARTUP.md` owns that screen.

**The seat's buildings can run on day one** (BL-1188, seat kit runs). They are placed under Pass 3's
chain-feasible rule, so no processor the player inherits runs a recipe nothing within reach
supplies. Whether the inputs arrive is the market's and the dispatcher's business
(`../economy/SUPPLY.md`).

## Corporate seeding is watched — split by kind

Industry generation has a **surface** (BL-305, loading-screen carve): it is shown live on the
loading screen (`app::draw_building_carve` in `src/core/app.cpp`, `app_screen::building`),
immediately after the nation carve it sits on, published through a `generation_progress*`
(`src/world/hard_coded_world.hpp`).

| Pass | Kind | Surface |
|---|---|---|
| **Pass 3 — starting asset placement** | *Positional.* A building is somewhere. | The **map**: each building is marked at its grid position as it is staked, over the carve, in its nation's colour. |
| **Per charter** | *Non-positional.* A focus and a count are derivations. | A **ledger column**: focus and buildings placed, one row per charter, swatch-matched to its markers. |

The firm-grain columns — opening capital and a firm's name — retire with Passes 4 and 5 (Ben,
2026-10-23). Like the carve, this is a **pure tap**: write-only, no randomness consumed, no branch
changed, null for every headless caller (`NATION_GENERATION.md` § The carve is watched).

---

## Generation seeds no behaviour

Generation's contract is: place the buildings, open the treasury, and let behaviour read them. No
sentiment, no diplomatic posture, no strategic plans or goals are generated. The build scorer
(`docs/ai/AI_OPPONENT.md` § 11; code in `src/world/corp_ai.cpp`) derives its decisions each tick from
world state, and the narrow per-building agency (idle a loss-maker, switch a floored recipe,
throttle a depleting extractor) reads the same state.

---

## Open items

**Building tiers / levels.** A level/tier axis for buildings — **distinct from production methods
(recipes)**. A charter's footprint may be characterised as much by the *tier* of its buildings as by
their count. Unsettled; interacts with `docs/economy/PRODUCTION.md`. Also in
`GENERATION_STRATEGY.md`.

**Post-war industrial grounding for the building mix.** The **focus** half is grounded in a named
pattern (§ Pass 2). The per-focus building patterns of Pass 3 are authored, and grounding them in
the post-war pathway toward off-world reach is open. Also in `GENERATION_STRATEGY.md`.

**What a world whose spend is refused opens on.** The fallback to the no-budget world retired with
the legacy path (Ben, 2026-10-23); `INDUSTRIALISATION.md` § Open questions carries it.

**Retired with the firm (Ben, 2026-10-23):** corporate borders and the HQ tall/wide axis (BL-182);
allied nations sharing corporations; franchise generation; nation-seeded privatisation (a nation
builds its whole economy itself); era-based corporate sovereignty; a firm's diplomatic posture.
Sentiment between nations stays `docs/politics/RELATIONS.md`'s. How a nation's treasury is filled
and spent is `docs/politics/NATIONS.md`'s (§ Taxes and Discontent, § 3).
