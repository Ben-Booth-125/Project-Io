# UI question log — every surface states what it answers

> **Generated file — do not hand-edit.** Source of truth is
> [`question_log.json`](question_log.json); regenerate with
> `node tools/session/render_question_log.js`.

Every information surface declares the **question it answers** and **why it earns its
space**, with the backlog item that demanded it. The pair is required. Enforcement is
authorship, not machinery — there is deliberately no audit check against this file
(BL-260, Ben 2026-08-01: *"the docs are the audit"*).

**66 surfaces** — 7 settled, 59 awaiting Ben's wording.

---

## Awaiting Ben's wording

These were **drafted by an implementer for Ben to accept or rewrite**, not authored by
him. BL-260 is explicit that writing the pair *is* the design check — so each of these
is an open question wearing a sentence, and they sit first rather than being buried in
alphabetical order.

### Balance Ledger

**Answers:** Where is my money going, and how long do I have?

**Because:** apply_budget nets six flows into one number; a single balance tells the player they are losing without telling them what to change. Itemising income, expenditure, maintenance, wages, interest and levies is what makes bankruptcy something to act on rather than discover. BL-343 added the Laws section beneath the policy levers: the first law that is not a stub sits directly under the two that are, so the difference between a drawn lever and a working one is visible in one glance.

*Demanded by BL-074, BL-112, BL-122, BL-343 · `src/ui/balance_ledger.cpp` · id `balance_ledger`*

### Balance ledger — Contract income line, and the header runway tooltip

**Answers:** How much of what I just earned came from mercenary work rather than trade?

**Because:** A completed contract's remainder pays out as a direct transfer to the corp balance (accept_offer's own split-payment precedent, corp_command.cpp) — real money that, before this item, moved the number at the top of the Balance ledger with no line anywhere explaining where it came from, exactly the kind of unexplained jump budget_result::subsidies exists to prevent for every OTHER nation-paid credit. It earns its place by closing that one remaining gap rather than opening a new concept: subsidies already means 'a nation paid you' everywhere else it is read (the national-budget transfer case, nation_step.cpp's own step 4), so a mercenary contract's payout landing on the same field is one line, not a second mechanism. The header runway tooltip reads the same field for the reverse reason — a lump-sum contract payment would otherwise read as a steady improvement in the burn rate the 'assumes the burn holds steady' runway estimate explicitly is not built to represent.

*Demanded by BL-577 · `src/ui/balance_ledger.cpp`, `src/ui/header_panel.cpp`, `src/world/nation_step.cpp` · id `balance_ledger_contract_income`*

### Battle card (Selection element, battle kind)

**Answers:** Am I winning this fight, and what does it cost me to walk away right now?

**Because:** A battle is the only thing in the game that spends an asset the player cannot re-buy this tick — men — while they watch. Every other Selection kind answers a standing question about a thing that will still be there next tick; this one answers a decision that expires. It earns its space by carrying the two numbers no other surface can: the phase, which is the world's own reading of whether the fight has turned (read_battle_phase, derived once in the world layer precisely so the card and the dispatch stream cannot disagree), and the WITHDRAWAL PRICE with its three terms separated — base, per-round, pursuit — quoted from the resolver's own arithmetic rather than recomputed here, so what the card says leaving costs is what leaving charges. Per-unit strength bars sit under both sides of a fight that is YOURS - on a rival-vs-rival battle each side reads 'Composition unknown' and the Withdraw press is disabled with a reason rather than hidden, so BL-068's rule reads as a rule instead of as a missing button. The bars are there because because 'I am at 60%' does not tell you whether that is one broken formation or five even ones.

*Demanded by BL-469, BL-467 · `src/ui/selection_panel.cpp`, `src/ui/ui_state.hpp`, `src/world/battle_system.hpp` · id `battle_card`*

### Battle marker (Planetary canvas, province anchor tile)

**Answers:** Where on this body is someone actually fighting?

**Because:** A battle is a province-grain event drawn on a tile-grain canvas, so without a mark it has no position at all — the units are visible but nothing says they are in contact rather than merely adjacent. Drawn on the province anchor tile only, once per battle, because the fight IS the province envelope (BL-467 ruling 1) and scattering a glyph across every participating tile would say the opposite. The glyph is two crossed blades with cross-guards, deliberately not an X: X is already the 'close this' affordance everywhere else in the chrome, and a mark meaning 'a fight is here' must not read as a button meaning 'dismiss this'.

*Demanded by BL-469, BL-467 · `src/ui/body_surface_canvas.cpp`, `src/ui/icons.cpp`, `src/ui/icons.hpp` · id `battle_marker`*

### Comms dock

**Answers:** What has happened that I did not watch happen?

**Because:** The simulation runs while the player is looking elsewhere. Without a log, events are only discoverable by noticing a changed number, which is the failure mode the alerts work (BL-261) also targets.

*Demanded by BL-212, BL-216 · `src/ui/chat_panel.cpp` · id `chat_panel`*

### Company ledger — the fold-out surface a Company-lens click opens (PLACEHOLDER)

**Answers:** I clicked this background firm's holding — what firm is that, and is it one of the corporations I compete with?

**Because:** It earns its space by being the only thing on the other end of a click that would otherwise go nowhere. Ben, 2026-08-28: "clicking will take you to a relevant ledger. (Different types for either, make a placeholder if needed)." The corporation half of that ruling already had a destination — the all-corporations table — while the company half had no surface of any kind, so the Company lens drew a population the player could look at and never open. The surface is DELIBERATELY THIN and says so on its own face: it answers none of the five axes a designed ledger answers (docs/ui/ledgers/README.md — top question, sub-views, lens on open, data sources, close semantics), prints only facts already public about any firm (name, registration, holdings, and capital where the firm files), and invents no figure where the world has none. Its justification is therefore provisional: it earns space today as the click's landing point, and must earn it properly when the ledger batch designs it.

*Demanded by BL-666 · `src/ui/company_ledger.hpp`, `src/ui/company_ledger.cpp`, `src/ui/ui_state.hpp`, `src/ui/nav_pane.cpp`, `src/core/app.cpp` · id `company_ledger`*

### Construction ledger - Buildings view (the estate by type)

**Answers:** What do I own, how much is each kind of it making me, and where do I go to tune one?

**Because:** It is the ledger's DEFAULT view, and it earns that over the queue by the plainest fact about the two: the player always owns buildings and usually has nothing building. Grouping by type rather than listing every building is what makes it an overview - a count and a summed profit per named type answers 'which part of my estate pays' in one glance, at the ~9 rows the shell column affords. Expanding a type row lists its own buildings, and a press selects one and draws its levers, so the chain runs type -> building -> lever without leaving the ledger. That is NAVIGATION, not recommendation: it lists what the player already owns, in a group whose count has already asserted they exist, and it ranks nothing they have not built. It also removes a dead end - 'Select a tile' is an instruction with no affordance attached, and the buildings behind a count are exactly the set the ledger has in hand. It is the destination for Method and Workforce, which left the Selection card's centre when Ben ruled that the centre presents data and never operates; the same function bodies are called from here, never a second set of controls writing the same fields.

*Demanded by BL-682, BL-683 · `src/ui/construction_panel.cpp`, `src/ui/construction_panel.hpp`, `src/ui/ui_state.hpp` · id `construction_buildings_view`*

### Construction ledger - Construction view (queue + build bar)

**Answers:** What is under way, what is it costing me, and can I build on the tile I have selected - and if not, why was I refused?

**Because:** One construction element, reached by two doors. The nav rail's slot 3 and the tile Selection element's Construct button used to open two different surfaces - a queue that was empty most of the time, and a separate column tenant holding the build bar - so a player met one or the other by accident of which control they pressed. They are one view now: the queue COLLAPSED to a single line above the build bar, because the queue is empty most of the time and an empty queue that owns the surface is the front-door-as-empty-room BL-176 diagnosed. Placement carries terrain, deposit, slot and logistics-reach rules, and a refusal the player cannot read is indistinguishable from a broken build, so the bar states the reason rather than merely denying. With NO tile selected it says 'Select a tile' and nothing more - deliberately, not for want of a better idea: a richer empty state would have to list the tiles the player COULD build on, and any ordering of that list is a recommendation, which is the surface Ben declined in the same message (CONCEPT.md).

*Demanded by BL-681, BL-029, BL-095, BL-162, BL-367 · `src/ui/construction_panel.cpp`, `src/ui/selection_panel.cpp`, `src/ui/ui_state.hpp` · id `construction_panel`*

### Contract card (Selection element, contract kind)

**Answers:** What did I actually agree to, and what have I been paid for it so far?

**Because:** SELECTION.md's battle element is this card's own precedent: a mercenary_contract has no entity id, so — like a battle — it resolves before selection_kind_of and owns its whole layout rather than the shared action|facts split. It earns its space by being the one place the four facts a contract's OWN record carries sit together: the predicate (via condition_text, the same reader the Balance ledger's laws listing already uses, so a contract's terms and a law's conditions never read in two different vocabularies), the committed force (CONTRACTS.md Q1 — the player names the force, never the contract, so the card is where that choice is read back), the deadline the tick-evaluation pass judges it against, and the fee SPLIT — deposit already paid versus the remainder still owed on completion — because 'the fee is 400cr' hides the one number that actually matters mid-contract: how much is still at stake.

*Demanded by BL-577 · `src/ui/selection.hpp`, `src/ui/selection_panel.cpp`, `src/ui/ui_state.hpp` · id `contract_card`*

### Public channel (comms dock) — mercenary-contract dispatches

**Answers:** What is happening to the mercenary work I have taken or am chasing, without me having to hold the ledger open?

**Because:** CONTRACTS.md's EVENTS.md-derived rule is that an event lands with its message in the SAME change: offer issued, accepted, completed, failed and abandoned are the five moments a contract's money and reputation actually move, and none of them had a surface before this — a completed contract paid out silently. The Public channel is the only correct home rather than a new channel of its own (unlike the battle dispatch stream): CHAT.md already settles Public as nation-voiced, and a contract's counterparty on the client side IS a nation, so the wording is the SAME first-person register post_nation_agency_comms already established, not a new voice to learn. Phrase selection folds the record's own stable id with the event kind (the BL-290 tongue-bank idiom battle_dispatch_line already uses), so replays read identically and no RNG draw is spent narrating a fact the simulation already decided.

*Demanded by BL-577 · `src/core/battle_dispatch_text.cpp`, `src/core/session_history.cpp`, `src/world/nation_step.cpp`, `src/world/nation_step.hpp` · id `contract_events_public_channel`*

### Corporation ledger — the stance groups and the row action strip

**Answers:** Who am I standing with, who am I standing against, and which way does the hostility run?

**Because:** BL-448 (corp stance) landed a stance data model with zero UI — the BL-350 lesson (a complete seam with no press) this surface exists to avoid — and this ledger is still its only host. The shape is three collapsing groups, Friends / Hostile / Neutral, each carrying its count and each stating its emptiness in words when it has no rows: Friends is normally empty at campaign start, and a section that simply vanished would read as a missing feature rather than as an answered question. A hostile row also says WHICH DIRECTION the hostility runs, because hostility is directed (RELATIONS.md § 1 Stance) and a row that only says 'hostile' has thrown away the half of the fact the player acts on — whether they declared, or are the one being interdicted. `is_hostile` is asked once per direction and never collapsed with `are_friends`, per that section's third invariant. The four transition presses (Declare Hostile with its confirm, Offer / Accept Friendship, Return to Neutral) live in the row's own action strip, opened by a disclosure arrow, and draw at the strip's full width — the arrangement that lets them exist at all in a ~324 px column, where a fixed 220 px in-row column clipped 'Declare Hostile' to 'De...'. There is no discovery gate: RELATIONS.md § 1 Stance overturns NR-350 — 'A declaration against the player is SIGNALLED' (Ben, 2026-08-22) — so a declaration groups and reads immediately rather than waiting on contact.

*Demanded by BL-639, BL-449, BL-448 · `src/ui/corporation_panel.cpp` · id `corporation_panel`*

### Corporation ledger — the firm's name and its Capital figure

**Answers:** Which firm is this, and what is it worth?

**Because:** The row carries two fields, and the first of them is the one this surface had never managed to show. NAME: the population is the named corporate field — corporations, plus the player's own row — and background firms (BL-365, background firms) are excluded, because a background firm exists to fill a body's production gap and is not a party anyone can stand toward. Listing them put eighty-odd rows in front of the handful that are, and left the firm's name so little width it collapsed to a single letter. A diplomacy surface whose rows cannot be told apart answers nothing, so the name gets every pixel the other field does not need. CAPITAL: exact where the firm files, a dash where it does not (FINANCE.md § Disclosure), exact always for the player's own row — a corporation always reads its own books. The dash means 'this firm does not file', never 'you have not earned this', and says so on hover. The bands are retired and are not reintroduced. REACH AND SHARE ARE GONE. Both were public and both printed exactly, but at this column width they cost the name, and neither is a stance fact: reach is a count of bodies and share is a clearing-income ratio, both of them standing figures that belong with the profitability read (BL-627, profitability ledger). What the row deliberately still does NOT carry is any operational quantity — production rates, stockpile levels, recipes and workforce dials stay private. An open book tells you what a firm earned, never how it operates.

*Demanded by BL-639, BL-633, BL-262, BL-631 · `src/ui/corporation_panel.cpp`, `src/world/standing.cpp` · id `corporation_panel_standing`*

### AI decision feed

**Answers:** What did the rival corporations just decide, and how close was the call?

**Because:** The scorer has recorded its own rationale since BL-202 - the winning score, the runner-up score and a reason code, into a 256-entry ring and permanently into the history log - and nothing has ever read either store. The reasoning accumulated every tick of every session and was shown to nobody. The margin is what earns the space: a command taken at 0.81 against a runner-up of 0.79 is a coin-flip the tuning could have gone either way on, one taken at 0.90 against 0.10 is a conviction, and nothing else in the game distinguishes them. That difference is most of what 'which strategy is it running' means. It also pays as diagnostics rather than spectacle (AI_OPPONENT.md 10h): the idle/resume oscillation that was the AI's dominant behaviour for an unknown number of sprints was invisible for exactly this reason.

*Demanded by BL-407 · `src/ui/decision_feed.cpp` · id `decision_feed`*

### Entity summary

**Answers:** What is this entity, in one line?

**Because:** The shared per-entity content builder feeding the Selection element, the Tile Ledger and the hover card. It exists once so those three cannot drift into describing the same entity differently.

*Demanded by BL-031, BL-145 · `src/ui/entity_summary.cpp` · id `entity_summary`*

### Field channel (comms dock)

**Answers:** What happened in my fights while I was looking somewhere else?

**Because:** A battle resolves several rounds per tick and a concluded one is ERASED at the end of the tick it ends — so the aftermath, which is the single line a player most needs (who held the ground, what it cost), is unreachable from any state-reading surface by the time they could look. The dispatch stream is the only place it can live. It is a SEPARATE channel rather than more traffic in Public because its volume is driven by simulation intensity, not by scripted events: a war is several lines a tick, and mixing that into Public would bury everything else. The phase vocabulary is shared verbatim with the battle card (battle_phase_word), so the two surfaces cannot drift into describing the same fight differently.

*Demanded by BL-468 · `src/core/battle_dispatch_text.cpp`, `src/core/session_history.cpp`, `src/ui/chat_panel.hpp` · id `field_channel`*

### Generation Ledger - one flat panel of collapsing sections (profile, thresholds, latitude bands, the three distributions) over a body selector

**Answers:** What shape did this body's generation actually come out, and which input made it that shape?

**Because:** A biome-balance question ('forest and wetland stay sparse on the homeworld') was previously answered by eyeballing the map, which cannot distinguish a bad tuning constant from an unlucky seed. Putting the composition/landform histograms, the ocean threshold against the profile's target, and the profile that drove them on ONE surface is what makes the answer traceable to an input rather than to an impression. It earns its space as a tuning instrument, not shipped chrome - it is the last rail slot for that reason. A sibling question - 'why is THIS tile what it is?', asked by a per-tile derivation breadcrumb in a Tile view - was retired with the ledger's tab strip (Ben, 2026-08-30): the six-pass pipeline discards its intermediates and nothing in the code or the design now rebuilds them per tile, so the body-level shape is the only question this surface asks.

*Demanded by BL-303 · `src/ui/generation_ledger.cpp` · id `generation_ledger_body`*

### The generation wait - every wizard loading round, the building screen, and the Painting-the-ground step after the seat

**Answers:** Is the world still being built, which step is it on, and how long has it taken?

**Because:** A wait that sits still reads as a crash (Ben, 2026-09-24, watching the Industrialisation round load: the bar held on one step for the whole road pass). STARTUP.md § A wait never looks stopped: the outer bar is weighted by what each step COSTS, measured in Release (generation_step_cost_ms), not counted as equal steps; every step over about a second reports progress within itself on the inner bar (the spans by year, the borders by tile row, the roads by village, the old roads by corridor, the validation run by quarter); a caption names the step in generation's own words (generation_stage_labels); and an elapsed-seconds count shows the run is alive. One surface (ui::draw_generation_wait) draws every wizard loading round and the building screen, so there is one wait, not two. Nothing else is added: the loading round stays one line, the bars and these two lines - no map, no board. BL-1246 (Ben, 2026-10-09) adds ONE step after the seat, on the same surface: "Painting the ground", shown only while the home body's ground master (baking since round 6 landed) has not yet landed, its bar the master's chunks landed over its total. It earns its place because the alternative is worse: play opening on ground that is still painting in. The wait never counts as game days (the clock rebases after it).

*Demanded by BL-1072, BL-1246 · `src/ui/generation_wait.cpp`, `src/ui/startup_screens.cpp`, `src/core/app.cpp`, `src/world/hard_coded_world.cpp`, `src/core/ground_layer.cpp` · id `generation_wait`*

### God-view corp/rival readouts (Selection facts column, rival Status rows, rival hover detail) + the survey tell on the Planetary canvas

**Answers:** What does this corp actually know, hold, and run — and where is its own blindness?

**Because:** A spectator otherwise sees LESS than the AI being watched; the lift makes a rival's decisions auditable without touching what the AI reads. Sight, never hands: the action grid stays disabled; the survey tell (lock-wash on unsurveyed tiles) keeps the corp's own blindness visible so the watcher is not misled into judging a decision against information the corp never had.

*Demanded by BL-408 · `src/ui/selection_panel.cpp`, `src/ui/hover_content.cpp`, `src/ui/body_surface_canvas.cpp` · id `god_view_readouts`*

### System menu — God view checkbox (spectate only)

**Answers:** Am I watching honestly or omnisciently?

**Because:** The lift must be a visible, deliberate state, never ambient — a watcher who forgets which mode they are in draws wrong conclusions about what the AI could see. Rendered only under spectate, so a played session cannot reach it by construction.

*Demanded by BL-408 · `src/ui/time_panel.cpp` · id `god_view_toggle`*

### Header

**Answers:** Am I solvent, and what is the date?

**Because:** The two facts that condition every other decision, needed at a glance without opening anything. Runway (BL-073) is here rather than in the ledger precisely because it is a warning, not an analysis.

*Demanded by BL-073, BL-171, BL-177 · `src/ui/header_panel.cpp` · id `header_panel`*

### Hover card

**Answers:** What is this thing I am pointing at?

**Because:** The canvas carries markers, glyphs and lens fills whose meaning is positional. Hover is the cheapest possible disclosure -- it answers without costing a click or displacing the current view, and glance-then-stick (BL-230) keeps it readable.

*Demanded by BL-228, BL-230 · `src/ui/hover_card.cpp` · id `hover_card`*

### Lens chrome region (minimap header, top right) — the lens selector and the active lens's key

**Answers:** What do these colours mean, and which good am I asking about?

**Because:** A lens re-skins the whole canvas and the re-skin is meaningless without its key: a nation tint is a colour until the key names the nation, a red-to-green mark is decoration until the key says which end is good. The region earns its space by being the ONLY place any of that lives — selector and key share one home because a lens draws at most one key, so a roster of any size costs exactly this rect and no more. It also earns it by fixing a measured failure rather than tidying a working one: there were TWO legend chromes, and the gradient-bar one was anchored flush-left of the minimap, inside the rect the always-open Selection band occupies. Six of seven keys rendered as ghosts through the band at roughly a tenth of their contrast (NR-601, measured 2026-08-24) — drawn, and unreadable. Only the Continent key escaped, because it alone had been moved to the foreground draw list (BL-376); one of seven was fixed and the collision was never generalised, and nothing had ever captured the other six to notice. So this region is not a tidy: it is how six lenses get a readable key at all.

*Demanded by BL-602 · `src/ui/shell_metrics.cpp`, `src/ui/shell_metrics.hpp`, `src/ui/body_surface_canvas.cpp` · id `lens_chrome_region`*

### Deposit and plate selection — the Resource and Continent lenses' hover highlight and click-through

**Answers:** This region the lens has drawn — what is it, and where do I go to find out more about it?

**Because:** A lens is a visual prelude to a ledger (Ben, 2026-08-28: "the lens is a visual prelude to whatever we design on ledgers using various charts and tables"), and until now two lenses drew a region that led nowhere. The Market lens had pivoted to its whole catchment since BL-603, but the Resource lens's deposit and the Continent lens's plate were explicitly excluded — lens_structure_of_tile recorded the reason, that a selection must be an entity and neither region is one, and that half-supporting them would "promise a pivot that does not arrive". This surface is that promise finally kept: hovering lights the whole region, and the click opens the ledger that can say something about it — Market (Prices, aimed at the resource) for a deposit, History for a plate. It costs no new screen space at all, which is the strongest form of earning it: the highlight reuses the catchment wash and the destinations are ledgers that already exist.

*Demanded by BL-659 · `src/ui/ui_state.hpp`, `src/ui/body_surface_canvas.cpp`, `scripts/verify/lens_selection_paths.lua` · id `lens_region_selection`*

### Market Ledger - Goods table

**Answers:** What is each good worth here, and which way is it moving?

**Because:** Markets are the public intelligence channel under the BL-068 visibility rule -- a rival's production and stockpiles are private, so price and the order book are the only honest read the player has on a competitor. The stacked-sparkline layout this replaces failed at that in three ways and only one was density: every sparkline was autoscaled to its OWN range, so a good decaying 40% and a good flat to a rounding error drew the same shape and goods could not be compared -- the one thing a price board is for; 'now 0.63' gave no direction; and the scroll hook aimed at the window while the list sat in a child scroller, so no capture, golden or human, had ever seen past the fourth good of 45. One row per good with the 8-quarter graph flattened into it fixes the density; drawing every row against a SHARED price/base axis with a 1.0 baseline fixes the comparison, which was the real defect.

*Demanded by BL-122, BL-159, BL-686 · `src/ui/market_ledger.cpp` · id `market_ledger`*

### Convoys ledger (nav rail slot 7)

**Answers:** What is on its way to me, and when does it land?

**Because:** Convoys are drawn on three canvases -- a moving beam on the Planetary canvas, lines on the Solar canvas, a lens glyph, an aggregated route graph -- and were LISTED nowhere, so the one number the player needs from them had no home. Travel time became load-bearing on 2026-08-12: a long haul now takes several quarters where it used to take one, and stock committed to a convoy is out of the pool for the whole of it. Without ticks-to-arrival on a surface, a player cannot tell a delivery that is late from one that was always going to be slow, and cannot plan a build against stock already in transit. The tab also gives BL-452's Hold press a per-convoy row to sit on. It became a LEDGER OF ITS OWN at rail slot 7 rather than staying a Market tab: it was never a market question -- MARKETS.md owns clearing and the order book, while a convoy is cargo in transit and belongs to SUPPLY.md ('Logistics is the road, Supply is the traffic'). Its own slot also lets it arm supply_routes, the lane overlay that is the literal map twin of the list; a tab strip arms ONE lens for every tab, so opening the Market ledger for a logistics read armed the price wash instead.

*Demanded by BL-452, BL-453, BL-689 · `src/ui/convoys_ledger.cpp` · id `market_ledger_convoys`*

### Market Ledger - nation presence row

**Answers:** Which nations operate in this market?

**Because:** The Body and Market combos name a place but say nothing about who is IN it, and a price is not readable without knowing whose ground feeds it -- a market served by one nation and a market four nations compete over price the same way on screen and mean entirely different things. It earns the space between the selectors and the tabs because it qualifies the market the combos just chose, before the tabs ask a question about it, and it costs one wrapped row. THESE ARE COLOUR CHIPS WITH INITIALS, NOT FLAGS, and that is deliberate: nation_colour is a palette entry and is all that exists -- corporations carry an emblem tag, nations do not -- so real per-nation emblem artwork would be a generated identity system, a feature of its own, and a single stubbed glyph would teach a vocabulary the game does not have. Placeholder even at that (Ben, 2026-08-29: flags and glyphs are a later sprint). Presence is DERIVED, since no store answers it: a building sits on a tile, market_for_tile resolves that tile's catchment market (the same routing clear_markets uses), and tile_to_nation gives the owner -- so a nation appears exactly when its ground really clears here.

*Demanded by BL-688 · `src/ui/market_ledger.cpp` · id `market_ledger_nation_row`*

### Market Ledger - Trades tab

**Answers:** What positions do I hold here, what else is standing, and what could I be doing?

**Because:** IT IS CALLED TRADES BECAUSE THE WORD CARRIES THE WIDENING (Ben, 2026-08-29). A sell order is one direction and one actor; a trade is a position either way round, held by anyone in the market, and -- now that the clearing tick retains a per-exchange record -- one that has already happened. The tab it replaces answered only 'what am I currently offering, at what floor?', which is the narrowest of the four things a player standing at a market wants to know.

It earns its space because markets are the public intelligence channel under the visibility rule: a rival's production and stockpiles are private, so price and the ORDER BOOK are the only honest read the player has on a competitor -- and the book was visible nowhere. THREE READS, KEPT VISIBLY DISTINCT, because they are not equally cheap to know and presenting them as one table would claim they were: my standing trades (a filter on my own orders), the market's standing trades (the same book past a gate), and potential trades (a derivation with no store behind it -- buy price here against sell price there, less the haulage the route would cost, priced through the same price_convoy_leg the auto-dispatcher and the player's own dispatch verb use, so the figure the player acts on is the figure that would bill them).

THE GATE ON THE SECOND READ IS 'the player owns a building on that body' (Ben, 2026-08-29, choosing it over 'an order here', 'either', and 'any discovered market'). Orders are world state and the deliberate public signal, so this is a reading question rather than a disclosure one -- but 'operates in' is a real predicate and is enforced, not assumed: a player reads the books of markets they trade at, not of the whole system.

THE HISTORY COLUMN IS REVENUE, NEVER PROFIT, and the limit is structural. stockpile_component is quantities[] and nothing else, so no cost basis exists anywhere in the model and the margin on a sale cannot be derived from it; quantity * unit_price is honest and a profit column would be a number the clearing loop never computed and a player would act on. An ABSENT counterparty renders as the market and not as 'unknown' -- three of the four clearing paths trade against the market as counterparty of last resort and they carry the volume, so blanking or skipping those rows would empty the section.

RANKING IS PERMITTED HERE and this is the one surface where that has been ruled on explicitly (CONCEPT.md: rank where the top row is one input among several, not where it IS the move). Ben, same day: 'Market prices is a vital pillar of gameplay, but the strategy "just build the most profitable" is a red herring.' A potential trade sorted by margin is still weighed against reach, stock, competition and what the price does next.

EACH LONG SECTION IS BOUNDED AND SCROLLS INSIDE ITSELF -- measured, not preferred: the book runs to 24 rows on the shipped fixture and the exchange read to 120, so laid out end to end the first fills the column and the other three reads are below the fold on open. A tab whose headline question is 'what could I be doing?' cannot open on a list of rival orders with the answer three screens down.

*Demanded by BL-687 · `src/ui/market_ledger.cpp`, `src/ui/market_ledger.hpp` · id `market_ledger_trades`*

### Market Ledger - Trades tab - 'Closed' table under My trades

**Answers:** Which of my standing sell orders closed themselves, and why?

**Because:** AN ORDER THAT CLOSES ITSELF IS A ROW THAT SILENTLY VANISHES. MARKETS.md step 4: an order whose pool stands empty for 4 quarters is removed by the clearing pass and the good returns to auto-surplus. The rule is right — a dead order should not hold a book slot forever — but a player who set a floor ON PURPOSE then finds the good selling at the market price when stock returns, and nothing on screen says the order is gone or why. The clearing pass already logs the close (an agency-topic history line, tagged with corp and body); nothing read it.

IT LIVES UNDER MY TRADES, NOT IN A FEED. The question it answers is about the player's positions on this body, which is exactly what My trades lists; a closed order is the row that used to be there. The decision feed is the rivals' reasoning and the chat feed is economy_report events — routing an order-book fact into either would make the player look for it somewhere other than where the order stood. It is a notice, not a history: the newest 3 closes within the last 4 quarters, Closed (month) · Good · Floor, the log line itself on hover. DRAWN ONLY WHEN THERE IS ONE, so the section's measured height budget is untouched in the ordinary case.

*Demanded by BL-1202 · `src/ui/market_ledger.cpp`, `src/ui/market_ledger.hpp`, `src/world/market_clearing.cpp` · id `market_ledger_trades_closed`*

### National border band (Planetary canvas, plain-canvas chrome)

**Answers:** Whose ground is this, and where does it stop?

**Because:** A nation used to be a LENS - a territory-wide tint you had to switch to, which meant the political map was invisible unless you asked for it, and the tint occupied the same channel as terrain, texture and every other lens. The band answers the same question as always-on chrome instead, the way roads already do: colour at the boundary falling off inwards, so a nation reads as a bordered region and the middle of a territory stays free for whatever else is being shown. Two neighbours meeting therefore show two parallel rules and never average into a third nation's colour - which the old tint did, because it was composited INSIDE the blended fill. It also carries the route the lens used to own: clicking the band selects the nation, which is the only way to reach one (Ben, 2026-08-24: 'click the border itself'). The corridor is capped at 0.18 of the drawn hex radius so it narrows with the hex rather than swallowing a frontier tile, and hovering it names the nation immediately - well short of the hover card's dwell - so the target is readable before the click commits.

*Demanded by BL-601 · `src/ui/body_surface_canvas.cpp`, `src/ui/ui_state.hpp`, `docs/ui/PLANETARY.md` · id `national_border_band`*

### Nav rail

**Answers:** What can I open from here?

**Because:** Every ledger and panel needs exactly one discoverable door, and the rail is where that door is. A surface with no slot is drawn every frame and reachable by nobody; a slot with no surface teaches a system the game does not have.

*Demanded by BL-022, BL-027, BL-028 · `src/ui/nav_pane.cpp` · id `nav_pane`*

### Owner multi-select — the Corporation and Company lenses' picked owner set, owned-grey fill, checklist key and shift-click picker

**Answers:** Where do I, and the one or two rivals (or firms) I am sizing up, hold ground on this body — against everyone else, who simply 'holds something here'?

**Because:** Buildings no longer carry an owner colour on the canvas (RENDERING.md § Installations), so the Corporation and Company lenses became the only place ownership is read at a glance — and with every owner tinted at once they answered 'who owns what' with a map of every colour at once, which answers nothing. A picked set keeps the comparison the player is actually making in colour and folds every other owner into one owned-grey, which still says 'someone holds this' without competing. Ben, 2026-10-08 (the sprint 51 visibility pass). The key copies the Resource lens's search-and-checklist shape so there is no new vocabulary, rows exist only for owners with ground on the body, the Corporation set defaults to the player ('where am I, against everyone'), and shift-click makes the map itself the picker while a plain click keeps its BL-664 meaning. It costs no new screen space: the key fills the one lens chrome region the lens already owned.

*Demanded by BL-1240 · `src/ui/body_surface_canvas.cpp`, `src/ui/ui_state.hpp`, `src/ui/presentation.hpp`, `src/core/verify_api.cpp`, `scripts/verify/owner_multi_select.lua` · id `owner_multi_select`*

### The Planetary canvas ground at every zoom - one baked master per body, viewed at one 22.5-degree angle at every rung and under every lens

**Answers:** What does this ground look like, at any zoom, the instant I look at it?

**Because:** Ben, 2026-10-09, after walking the sprint 51 build: the ground lag was off-putting, because every zoom step had been a fresh bake that arrived late and softer first. One 96 px/hex master per body, pre-baked behind the handoff and held in RAM, makes every rung a downsample of the same picture: a rung change is final on arrival, and detail never changes with zoom, only scale - so the player learns one picture of a world, not five. One camera angle at every rung and under every lens is what makes one master possible (a second angle was a second bake), and it means neither a zoom step nor a lens toggle moves the map under the cursor. It adds no chrome and no control: it replaces the per-rung tiers, the stand-in that hid their fills, and the stepped tilt.

*Demanded by BL-1246 · `src/core/ground_layer.cpp`, `src/core/ground_layer.hpp`, `src/ui/ground_bake.cpp`, `src/ui/body_surface_canvas.cpp` · id `planetary_ground_master`*

### Baked landform relief and carved rivers (Planetary canvas ground) -- mountain as massif and ridge, canyon as a cut between paired rims, crater as a raised-rim bowl, rift as a dark fissure; rivers as curved courses widening downstream

**Answers:** Where is the ground expensive to cross, and where does the water run and which way?

**Because:** The four dramatic landforms are the <=1.5% of land whose movement cost is x1.3 or worse, so an invisible one is an expensive surprise; a river is an edge that discounts travel one way and gates wells and farms. Both used to be vector chrome laid over the painted ground -- a stroke-only glyph on the hex centre, a straight centre-to-centre stroke with chevrons -- and read as annotation on a painting (Ben, 2026-10-08, the sprint 51 form: mountains and rivers blend into the render). Baked, the same facts read as terrain: a contiguous run is one range, one cut, one fissure; a river is one smooth course whose width says which way it flows, so the chevrons have nothing left to say. The hover card still names the landform and its cost.

*Demanded by BL-1242 · `src/ui/ground_bake.cpp`, `src/ui/body_surface_canvas.cpp`, `src/ui/hex_render.cpp` · id `planetary_landforms_rivers`*

### Roads and sea lanes (Planetary canvas, always-on: painted into the ground, drawn as thin strokes at the two widest rungs)

**Answers:** Where can traffic move cheaply on this body - which ground is roaded, at what tier, and which water carries a lane?

**Because:** Roads are terrain, not an overlay: the road ladder (Track / Road / Highway) and the sea lane both discount traversal cost for every convoy and every reach read (LOGISTICS.md sec 1, sec 4b), so a player planning a site or a haul needs them on the plain canvas under every lens. Ben, 2026-10-03, playing the build: "render [roads] as curves rather than lines, and make them thinner" and "sea lanes should always go over ocean, never over ground". So both draw as smooth curves through their tile chains (a quadratic through each tile's two shared-edge midpoints, junctions paired into through-curves), at one named width per tier, half the old straight-segment widths, keeping the 1 : 1.5 : 2 ladder so the tiers still read apart. The sea lane had no stroke on the campaign map at all, though the stamped field shapes every sea leg's cost; it now draws along its own stamped water tiles in the sea blue the wizard's lapse uses, so the curve can never leave the sea. PAINTED INTO THE GROUND (BL-1253; Ben, 2026-10-09, walking the sprint 51 build: "roads go over buildings, rather than being painted as an optional part of the building tile sets. Roads are also so simplified, we want to paint a texture on roads"): the same curves are now a pass in the ground bake with a surface per tier - Track ruts and a grass crown, Road gravel with verges and ditches, Highway asphalt with kerbs and a centre line, a lane a faint broken wake on the water - so the tier reads from the surface itself, not only from a stroke weight. A road meets a built tile through that tile's roaded variant (the works step aside, the road bends round them and ends at a forecourt; a town keeps its blocks off its street), so a road never runs across a roof. The drawn strokes stay only at the two widest rungs (drawn radius under 20 px), where a painted road is under a pixel and the logistics web would otherwise vanish.

*Demanded by BL-1253 · `src/ui/body_surface_canvas.cpp`, `src/ui/route_paint.cpp`, `src/ui/structure_stamps.cpp`, `docs/ui/RENDERING.md` · id `planetary_routes`*

### Baked structures (Planetary canvas ground: buildings, settlements, ruins, construction sites) and the Selection band's zoomed neighbourhood view of them

**Answers:** What stands on this ground, roughly what is it, and how built-up is this region - without hovering anything?

**Because:** A glyph layer over painterly ground made every built tile read as a label stuck on a picture, and one mark carried four answers on half a hex (type, kinds, count, owner). Ben's sprint 51 form (2026-10-08) moved the answer into the art: one procedural structure per depicted subject - a head-frame over a mine, a hall and stacks over a foundry, a quay toward the water at a port - so WHAT is made here reads by silhouette; a stacked tile stands a cluster of up to three, the dominant stack largest and in front; a settlement steps its footprint and height with scale and bakes as a pale paved patch at the far page, where the density dot stood; a razed centre bakes as a ruin and a site under construction as scaffolding. It earns its space by taking none: it IS the ground the canvas already draws, adds no chrome, no legend and no control, and frees the hex the ring, badge, emblem tag and skyline occupied. Finer questions moved to surfaces built for them - count and running state to the Production section, ownership to the hover card, the player footprint outline and the owner lenses. The Selection band's zoomed neighbourhood view draws the same baked page, so the picture of a selected tile matches the canvas it was pressed on.

*Demanded by BL-1241 · `src/ui/structure_stamps.cpp`, `src/ui/structure_stamps.hpp`, `src/ui/ground_bake.cpp`, `src/core/ground_layer.cpp`, `src/ui/body_surface_canvas.cpp`, `src/ui/hex_render.cpp`, `docs/ui/RENDERING.md` · id `planetary_structures`*

### Profile panel

**Answers:** Who am I in this world?

**Because:** Identity is carried by emblem and colour across every canvas and ledger (BL-090). One place has to establish that vocabulary, or the marker colours are arbitrary everywhere else.

*Demanded by BL-090, BL-091 · `src/ui/profile_panel.cpp` · id `profile_panel`*

### The province sections of the tile Selection element (Buildings / Deposits / Population)

**Answers:** The canvas just blended several tiles into one shape - what is actually IN this locality, how much room is left in it, and who lives there?

**Because:** BL-511 removed the tile as a click target, so the mixture, the deposits and the buildings of a locality became unreachable by the gesture that used to reach them, and a CARD OF ITS OWN was the answer. BL-598 (Ben, 2026-08-24) reversed the premise rather than the answer: the tile is a click target again, and the province readings are SECTIONS of the tile element's one accordion. The question is unchanged and still earns its space - a player deciding whether a locality is worth a mine asks about the locality, not one hex - but it no longer earns a second element to ask it in. Two surfaces asking about one piece of ground made the player choose a grain before knowing what they wanted to know; one accordion, ordered from what can be acted on to what the ground merely is, does not. What was dropped in the fold is the province's own Buildings ROLL-UP (the same question the Buildings section's Built column answers, at a grain the player does not build at) and its member-tile list (whose job was to give back a tile that is no longer taken away). What was gained is Population, which had no home on either surface.

*Demanded by BL-511, BL-598 · `src/ui/selection_panel.cpp`, `src/ui/body_surface_canvas.cpp` · id `province_card`*

### Resource lens (Planetary canvas) — the washed-clear map, the toggled deposit set with split tiles, and its search-and-checklist key

**Answers:** Where can the goods I care about be extracted on this body, and where do they share ground?

**Because:** Siting extraction is the first economic decision a player makes, and it needs no simulation — only the generated deposits. Ben, 2026-10-04: the map goes a shade of white so the deposits are the only saturated thing on it, and the lens toggles several resources at once, splitting a tile that carries more than one, with a search bar so the player finds a good quickly. A recipe takes more than one input, so the question is rarely 'where is iron' and usually 'where are iron and coal together' — one good at a time made the player hold the other map in their head. The set is capped at six (one wedge per hex edge) and offers only extractable goods present on the body, because a manufactured good is never on the surface and a box the player cannot use earns no row.

*Demanded by BL-1182 · `src/ui/body_surface_canvas.cpp`, `src/ui/hover_content.cpp`, `src/ui/ui_state.hpp` · id `resource_lens`*

### Corporation selection canvas (Begin, before play)

**Answers:** Which corporation am I?

**Because:** THE SEAT IS AN IDENTITY, NOT AN ADDRESS (Ben, 2026-09-17, NR-885): the player is a corporation, so choosing one is becoming it, and a random draw assigned an identity nobody chose. Ben reversed the 2026-08-26 retirement of the selection screen on 2026-09-09 ("it's time to reverse that ruling") and designed the surface on 2026-09-24. It comes AFTER the firms and the settle, which is what makes a seat worth weighing: the retired pre-settle stage had no moved balances to show. A RANKED LIST BESIDE A MAP: every specialist in static-score order, with the ones below the viability floor visibly marked and still pickable, so a player can knowingly take a hard seat; the map is the home body and highlights the hovered row's HQ, works and home market catchment, so where a firm stands is read by pointing at it rather than by reading coordinates. THE CARD IS FOUR LINES AND NO MORE - industry and goods; HQ and home market with that market's prices for the firm's goods and inputs; cash, debt and assets; nation and its stance - because those are what an opening position is made of; the landscape score is the list's order, not a line, and trailing figures and rivals are left off. PICKING TAKES TWO PRESSES: a briefing in prose (who you are, what you make, where you sell, what you owe, whose law, and for a marked firm why the floor marked it), then Confirm or Back, because choosing who you are is a deliberate act. The pick is a game act (take_seat), reproducible from (seed, pick).

*Demanded by BL-1076, BL-630, BL-1020, BL-1073 · `src/ui/seat_screen.cpp`, `src/core/app.cpp`, `src/world/spawn_seat.cpp` · id `seat_canvas`*

### Selection band - Building card (3-column band)

**Answers:** What is this building, what is it earning me, and what can I do about it?

**Because:** The building card takes the SAME 3-column band shape as the tile card (zoomed tile render / paged accordion / action grid), so 'select a thing, get its picture, a pager and its actions' is one shape across the two entity kinds that actually get selected in play. Its CENTRE presents data and never holds levers (Ben, 2026-08-29): a dial that changes the world belongs to a ledger, so Method and Workforce - two pages whose whole content was a control - moved to the Construction ledger's Buildings view, and what remains reports (Profitability, Status). The boundary runs around the CENTRE, not the element: the right-hand action grid is the designated place to act and keeps Mothball, Dismantle and Auto. The move would have left a building selected on the MAP with no route to its own levers, so the grid gained a fourth button that opens the Buildings view AIMED at this building - its type group expanded, the building selected, the levers on screen in one press. That button is a door rather than a toggle (its active state is not visible here), and it is ABSENT on a rival's card, because the Buildings view is the player's estate and aiming a rival building at it would open an empty aim. A rival card is Status only, by a short-circuit in building_pages that never tests a player page's own guard - a competitor-visibility guarantee rather than a convenience.

*Demanded by BL-683, BL-682, BL-431, BL-430, BL-074 · `src/ui/selection_panel.cpp`, `src/ui/selection_panel.hpp`, `src/ui/selection_card.cpp`, `src/ui/detail_level.hpp`, `src/ui/ui_state.hpp` · id `selection_building_card`*

### Selection band - Market card, Dispatch convoy form

**Answers:** How much of what do I send from the market I'm looking at, to where?

**Because:** SUPPLY.md always specified this front door -- 'a dispatch starts from a source you are looking at, and it is a resource + quantity + destination-market form, not a press' -- but BL-452 only ever wired dispatch_convoy onto the wire/agent dictionary; no UI site issued it, so the player's own corp had no way to direct a convoy at all (only the automatic shortfall scan moved goods). The form reuses the Sell Orders tab's own tradeable-resource test and the Convoys tab's market_city_name identity, so it cannot show a resource or destination the player couldn't already see traded or in flight, and a rejection (no route, insufficient funds) is surfaced inline rather than silently dropped.

*Demanded by BL-607, BL-452 · `src/ui/selection_panel.cpp` · id `selection_market_dispatch`*

### Selection element

**Answers:** What have I selected, and what can I do with it?

**Because:** The pinned, polymorphic detail surface for the current selection. It is the answer to the click model's promise: single-click selects, and something must visibly happen when it does. BL-593 (2026-08-24) extended the tile construction ledger's candidate filter to recipe-level tech locks (tech_locked, BL-588), filtered out the same way era_locked already was -- "the door not showing what the gate would refuse", not a new UI affordance.

*Demanded by BL-067, BL-068, BL-071, BL-367, BL-593 · `src/ui/selection_panel.cpp` · id `selection_panel`*

### Selection band - the tile element's Production section (first in the section nav)

**Answers:** What stands on this tile, is it running (and if not, why not), and what does what it makes - or what lies in the ground here - fetch at the market that prices it?

**Because:** Running state had no home once the canvas art went static (RENDERING.md, installations): a building that sat idle for want of labour or an input looked exactly like one that ran, and the only place that said otherwise was a hover card that has to be found one building at a time. Ben, 2026-10-08 (the sprint 51 visibility pass): the tile opens on what is HAPPENING there - the initial market conditions, read from the ground the player is looking at. Tile grain is the point, not a shortcut: a building stands on a tile and a market's catchment is tile-keyed, so a province sum would blur exactly the two things this section joins. Part one gives one row per stack (the grouping Manage Buildings uses, and where the retired +N badge's count is now read), with output per tick and the running state WITH ITS REASON - labour short, or an input short named with its cause - classified by the one function the hover card and the building card's Status page also read, so the three cannot disagree. A rival's row shows type, count and owner and says 'private' for the rest (DISCOVERY.md). Part two names the market whose catchment holds the tile and prices every good made here, then every good deposited here, with short / balanced / surplus from public aggregates; each row is a drill-through door to the Market ledger aimed at that market and good. An unbuilt tile still earns the section: 'Nothing built here' is stated positively and its deposits are still priced, which on a fresh campaign is the whole read.

*Demanded by BL-1239 · `src/ui/selection_panel.cpp`, `src/ui/building_state.cpp`, `src/ui/building_state.hpp`, `src/ui/hover_content.cpp`, `src/ui/market_ledger.cpp`, `src/ui/ui_state.hpp` · id `selection_tile_production`*

### Selection band - the tile element's section top nav

**Answers:** Everything this ground has to say, opening on what is happening here and then in the order I can act on it: what does this tile make and fetch, what can I still build here, what does the locality hold, what does this hex yield, who works here, what is the terrain?

**Because:** The centre column was a PAGER, and the province was a second element with a pager of its own; both hid the list of questions the surface can answer behind a press. An accordion was built to show that list and was ruled out on sight, on a measurement rather than a taste: five stacked headers spent 169 of the band's 258 px on chrome to leave the open section 89. The nav keeps what the accordion was FOR - a visible sense of how many readings exist - by putting an i/N count beside the title, which costs one row instead of five, and returns the rest of the band to the reading you are actually doing. The chevrons straddle the span so the two presses are as far apart as the element allows; the title centres on the run between them; the full-canvas control is excepted and keeps the rightmost slot, which is where every other surface in the shell puts it. The ORDER is the other half of the argument (Ben, 2026-08-24): Buildings, Deposits, Resources, Population, Terrain runs from what the player can act on to what the ground merely is. Ben, 2026-10-08 (the sprint 51 visibility pass) put Production in front of it - what is HAPPENING here - so the nav is six sections and wraps at six.

*Demanded by BL-598, BL-1239 · `src/ui/selection_panel.cpp`, `src/ui/ui_state.hpp` · id `selection_tile_section_nav`*

### Selection band - Unit (Soldier) card (3-column band)

**Answers:** What is this unit/unit-stack, how strong is it, what type is it, who owns it, and can I do anything with it?

**Because:** selection_kind::unit existed but fell through to the generic action/facts split with a bare Go to button - the only selection kind still on that path once the tile (BL-123) and building (BL-431 rework) cards moved to the 3-column band shape. BL-393 (UNITS_ARE_WRITE_ONLY_AND_INERT) already flags that units are largely inert in the live economy; Ben's direction was to build the CARD shape now anyway rather than wait on combat, so a unit selected today reads real unit_component fields (strength, count, roster type, owner) in the same picture/pager/actions shape as everything else, instead of standing out as the one kind that still looks unfinished. Paired with a repeat-click tile-cycle (Soldier -> Building -> Tile) in body_surface_canvas.cpp so a tile carrying a unit is actually reachable by clicking. BL-575 (unit marker + march UI, 2026-08-23) answers the "can I do anything with it" half for real: the action grid gained March (arms province-picking on the Planetary canvas, then dispatches corp_verb::march_unit on the qualifying province click), Halt (clears the standing order) and Disband (permanent, confirm popup, no refund — MILITARY.md § Marching) alongside the existing Go to, replacing three of the five reserved slots. All three route through the SAME corp-command seam corp_ai scores for rival units, so the player takes no shortcut around it.

*Demanded by BL-393, BL-575 · `src/ui/selection_panel.cpp`, `src/ui/selection_panel.hpp`, `src/ui/ui_state.hpp`, `src/ui/body_surface_canvas.cpp`, `src/core/app.cpp` · id `selection_unit_card`*

### Strategy readout (nav slot 12)

**Answers:** What strategy is emerging from each corporation's run — what mix of verbs is it taking, which spending priorities dominate, and which reasons keep firing?

**Because:** The decision feed (BL-407) shows the moves; nothing showed the SHAPE across a run — the thing Ben asked for (2026-08-14: watch AI play "and discover what strategies emerge"). The scorer has no strategy object, so an emergent strategy IS a distribution over decisions: the verb mix separates an expander from a consolidator; the must/should/nice bucket split is the most legible health signal in the stream; the reason tally is where a pathology like § 10h's idle/resume oscillation shows as two reciprocal codes. Score/margin figures deliberately absent (NR-226); no strategy is ever named (STRATEGIES.md's discovered-not-authored position).

*Demanded by BL-411 · `src/ui/strategy_readout.cpp` · id `strategy_readout`*

### Tech tree viewer (F9)

**Answers:** What can I unlock, and what stands between me and it?

**Because:** A constellation of gates is only a decision if the player can see which are reachable. BL-344 made that second half real: each node now reports EARNED, LOCKED with its unmet conditions itemised, or -- honestly -- "no gate authored", instead of showing an unevaluable string condition that could never resolve.

*Demanded by BL-087, BL-126, BL-344 · `src/ui/tech_tree_panel.cpp` · id `tech_tree_panel`*

### Throughput lens (Planetary canvas)

**Answers:** How much can move through here, and how far is this ground from the capacity that would move it?

**Because:** Active Logistic Points are a CAP: a march over the cap is refused outright, and LOGISTICS.md makes surfacing that non-optional -- 'a refusal nobody sees is silent interdiction again'. Without a surface the player meets the cap only as a move that mysteriously did not happen. It earns its space by costing none: it extends the Reach field the placement rule already computes rather than adding a surface, which is exactly the shape Ben's ruling asked for -- 'throughput is that field with a magnitude, so it is a small step from an existing surface rather than a new one'. It is off the lens bar, so it takes no strip slot from the eight lenses that answer first-sight questions. IT ALSO CARRIES THE NETWORK (BL-1257, Ben 2026-10-10): on the ground a road is a thin pale thread of the land, so the road and lane network as logistics -- every road by tier at full weight, every lane in its blue, at every rung -- is read here, over the reach-cost field that network produces; the plain canvas keeps only a thin drawn network at the two widest rungs.

*Demanded by BL-606, BL-1257 · `src/ui/body_surface_canvas.cpp` · id `throughput_lens`*

### Tile inspector

**Answers:** What is this ground, and what could it support?

**Because:** Terrain is two axes plus a deposit profile, none of which is fully legible from the canvas colour alone. Siting is the player's central recurring decision and this is where its inputs are read.

*Demanded by BL-122, BL-144 · `src/ui/tile_inspector.cpp` · id `tile_inspector`*

### Terrain texture (Planetary canvas ground pass)

**Answers:** What is this ground made of, and how heavily is it covered?

**Because:** Colour alone could not carry both halves once BL-519 split the terrain axes. The substrate and the cover blend into ONE fill, so a rocky slope with a thin wood and a sedimentary plain with a thin wood arrive at neighbouring greens and the player cannot tell which they are siting on. Texture separates the two readings onto separate channels: a faint substrate grain that says what the ground is, and a per-tile cover pattern whose mark count and weight scale with cover_density, so a sparse wood LOOKS thin exactly where the economy already CUTS it thin. It earns its space by being free of any: it adds no chrome, no legend and no control, and it is the only way a cover boundary reads as a boundary now that BL-511's province blend deliberately smooths the fills.

*Demanded by BL-520, BL-519 · `src/ui/hex_render.cpp`, `src/ui/body_surface_canvas.cpp` · id `tile_texture`*

### Trade-flow lens (Planetary canvas) -- the player's market-to-market shipment arrows, a refusal marker per short market, and the class and width key

**Answers:** Where is my surplus going, and why is it not going where it is short?

**Because:** The dispatcher decides where the player's surplus goes every pass, and before this lens that decision surfaced only as its result: Supply shows convoys in flight and Supply-routes the lanes they carved, but neither says what was NOT sent or why. A market short of a good the player holds in surplus reads, without this, as a dispatcher that is broken; with it, as one of eight named reasons (no lane, price gate, no route, costly, no propellant, no room, no funds, room) -- each of which points at a different lever (a port, a road, a price, fuel, cash, a lane). Ben, 2026-10-07 (the lens form): a player lens over the player's own flows only; the whole-world diagnosis stays headless. It is off the strip, keyboard-cycle only, so it takes no slot from the first-sight lenses.

*Demanded by BL-1222 · `src/ui/body_surface_canvas.cpp`, `src/world/supply_system.cpp`, `src/world/world.hpp` · id `trade_flow_lens`*

### Unit marker (Planetary canvas, the group's own tile)

**Answers:** Where are my (and my rivals') forces standing, and whose are they?

**Because:** Units had no on-canvas glyph at all before this (ICONS.md previously documented Unit as "(no glyph)"), reachable only by clicking the exact tile a unit stood on or cycling into it — a large province full of units was otherwise invisible on the map. BL-511 made a unit's command grain the PROVINCE (march_unit targets a province, not a tile), so the marker groups at the province grain the battle marker already established: drawn once per (province, owner) GROUP, on the tile the group's lowest-id unit stands on (BL-1145 — the province's lowest-id tile, the first post, could lie hundreds of tiles from every unit once a province became a centre's whole ground, leaving an army on screen with no marker), with a "+N" count badge for more than one unit in the group, rather than once per unit or per tile. The humanoid silhouette echoes the unit card's own placeholder glyph (glyph_soldier) so the canvas and the card read as one vocabulary. Carries a stub ring for contract-committed units (always false today; BL-573, a later wave of the same Sprint 16 batch, adds the real per-unit flag) so that later item needs no further UI plumbing change.

*Demanded by BL-575, BL-511, BL-1145 · `src/ui/body_surface_canvas.cpp`, `src/ui/icons.cpp`, `src/ui/icons.hpp`, `src/ui/ui_state.hpp` · id `unit_marker`*

### Selection band — the water tile variant (owner / domain centre column)

**Answers:** Who owns this water?

**Because:** Coastal water and lakes carry an owner, derived from the shore that claims them; open ocean structurally does not (PROVINCES.md § Who owns water). That asymmetry is the load-bearing shape of the water model, and it was invisible on every surface the game had — the hover card reported terrain and habitability and said nothing about title, no lens colours ground by owning nation, and clicking water did not move the Selection band at all. A claim nobody can look at can only be trusted, which is not the standard this project holds a generated world to. It earns its space by reusing the tile element rather than adding one: same header, same hex ring (which is where a shoreline reads at all), same action grid; only the centre column forks, and it forks because four of the five ground sections — buildings, deposits, resources, population — ask questions about ground there is none of. The single most important word on it is 'Unowned', stated positively: an empty owner row and an owner row reading 'Unowned' cost the same pixels and mean opposite things, the first looking like the panel failed and the second being the model's central assertion.

*Demanded by BL-785 · `src/ui/selection_panel.cpp` · id `water_tile_selection`*

### Round 6 tail caption (under the playing Industrialisation map)

**Answers:** The map is already playing, but the world I will be handed is still being built - what step is it on?

**Because:** Round 6 starts playing at the span's close while the same worker builds the tail behind it (borders, roads, companies, the landscape search, the settle), so without a word the player cannot tell a playing lapse from a finished round, or why Begin waits. One caption line under the map, naming the ONE step under way ("Drawing borders", "Laying roads", "Searching the landscape", "Proving the field") and nothing else, is the least surface that answers it; it takes no space from the map or the board, and it disappears when the world lands. STARTUP.md § Round 6 specifies it.

*Demanded by BL-1084, BL-1068 · `src/ui/startup_screens.cpp` · id `wizard_round6_tail_caption`*

### New World wizard - round 5, Exploration

**Answers:** Who reaches beyond this ground, and what do they bring back?

**Because:** BL-946 (Ben, 2026-09-13, resolving NR-847/NR-857): the Exploration span (1200 -> 1660 CE, EXPLORATION.md) was running opt-in behind `exploration_sim_enabled` and invisible to the player - a real generation pass with no wizard round to show it. The default flips to TRUE and this round replays it on the same shared engine and the same lapse map as Culture and Empires, stopped at its own close (`world_gen_config::stop_after_exploration`) before borders, roads and companies are built. ITS OWN RECORD, NOT A REUSE OF THE EMPIRES ONE: `generation_report::body_entry::exploration_timelapse` is recorded at the one call site that runs the span (as_timelapse(kepler_exploration_hs)) and wired through the SAME `progress->lapse_tap` publish path Culture and Empires already use, so the round shows a real, growing map rather than a silent hang followed by an empty pane - this is also what gives BL-943's fleet/caravan exemplars and BL-932's capital-consolidation burst real Exploration-span events to draw, closing the exact gap NR-857 named. Its own battle/conquest/founding counters (`exploration_*` on `generation_report`) are shown rather than the Empires round's, because the two spans are not the same age. IT DRAWS THE FLEETS THAT ARE THERE (BL-1095; Ben, 2026-09-24, R13; STARTUP.md § Round 5): a dashed tie overlord -> subject laid at the binding and lifted at the freeing or the overlord's end, fading to a trade line where a treaty between the pair follows; a treaty arc between party capitals for the term, snapped at the break; a hull at each capital scaled by the sampled navy and a harbour mark that silts with the capital's port stock (both series on the polity sample); a sail crossing hub -> target on each wet campaign, and a seat taken across water drawn as a landing. All read from the record; the span is unmoved. The lane line (BL-1097) stays its own layer: a lane runs between two shores and stays, a tie runs between two polities and goes when the bond does. THE LANE IS A BOWED ARC (BL-1124; Ben, 2026-09-25, picked at the live app from three forms): the wide soft band was still not found at the live click, for two measured reasons - the tribute leg writes most lane uses, so a lane nearly always runs the same capital-to-capital line as its colonial tie, and most lanes join neighbouring regions (seed 32 at 1920x1080: 13 of 16 draw under 8 px). So the lane bends south off the straight line the tie keeps (treaty arcs bow north, so the two arcs never meet), and the legend names it in the same arc. THE LANE FOLLOWS ITS SEA PATH (Ben, 2026-10-03, playing the build: "sea lanes should always go over ocean, never over ground"): an arc between the two seats crossed whatever land lay between them, so the lane is now drawn along the water-only walk the campaign stamp lays (LOGISTICS.md sec 4b) - port to port, string-pulled over the sea and corner-cut smooth, never across a land tile. It keeps the FORM Ben picked (a soft, gently curved sea-blue band) and still leaves the tie's straight line, because it starts at the coast and goes round the land. THE ROADS CARRY ACROSS THE ROUNDS (BL-1134; Ben, 2026-09-25, walking round 6: "we lost every road between Exploration and Industrialisation"; STARTUP.md § Round 6). A resumed span notes only the promotions it makes, so each round read alone opened with none of the roads the earlier rounds laid; the round before now hands its whole network over at the seam, as the civilisation diamonds are handed over, and every road drawn at a round's close is drawn on the next round's first frame (roads_carried.lua; seed 32: 480 carried into Exploration, 542 into Industrialisation).

*Demanded by BL-816, BL-829, BL-860, BL-931, BL-937, BL-943, BL-946, BL-1095, BL-1124, BL-1134 · `src/ui/startup_screens.cpp`, `src/ui/history_lapse.cpp`, `src/world/hard_coded_world.cpp`, `src/world/hard_coded_world.hpp`, `src/core/app.hpp` · id `wizard_round_exploration`*

### New World wizard - round 5, The History

**Answers:** Who claimed this ground, and who lost it, in the age before the epoch?

**Because:** Origin belongs to round 4; this round is the rest of the arc - communication, then conquest or diplomatic union, then a stable dark age - over the 4000 years ending at 1200 CE. It earns a page of its own rather than a section of round 4's because it has its own span, its own rules and its own reroll: a history you cannot reject is a history you were assigned, and rejecting it must not re-draw the migration above it (world_params::era_seed exists for exactly that). Its chart surface is the leaderboard, which moves with the map. THE MOMENTS ARE PART OF THE TIME-LAPSE, not outcomes read off at the end (Ben, 2026-09-11): the record carries typed events - foundings, seats falling, realms ending, secessions, re-seatings, roads, civilisations, creeds - and the round shows them as a ticker of the last few at or before the playhead, phrased with the region's generated name, named in the ticker but NOT marked on the map: BL-916 pulsed a ring at every event's region for about a screen-second, the same ring for a realm dying and a trade route opening, and with fifteen region-carrying kinds the map read as a snowstorm over the borders it exists to show (Ben, 2026-09-16, watching it run). The borders are the story; the ticker carries the moments. The arc readout under the board counts destroyed realms off those events (a dead realm is ABSENT from the next sample, so counting zeros read 0 on every world) and takes peak share against the regions live at the peak's own step, not the final stride. Its map is the same surface as round 4's and draws the same ground under its polity fill - rivers, relief, seats, a frontier on both axes, and neighbours never in one hue (BL-915). A FLEET OR CARAVAN EXEMPLAR MARKS A CORRIDOR'S THROUGHPUT CROSSING A THRESHOLD (BL-943, EXPLORATION.md sec Goods move as throughput: "the visual is a filter on that number, not a second simulation"): a `road_promoted` or `trade_link_opened` event already IS that threshold crossing, so one sail or one caravan draws at the corridor's midpoint and fades over the same marker window the event ring does - never a mark per cargo unit, never a continuous animation. Sea vs. land is read off the corridor's own baked terrain sample; the road ladder's three rungs (Track/Road/Post Road, BL-940) grow and ring the mark so they never draw alike. The one-time treasury consolidation (BL-932, "material becomes capital" at 1200 CE) draws as a separate gold burst at every capital, gated on the record actually reaching past that year - which, until the Exploration span's own tap is wired into the recorded era, it does not, so the beat is correctly silent on every world today.

*Demanded by BL-816, BL-829, BL-860, BL-916, BL-915, BL-943 · `src/ui/startup_screens.cpp`, `src/ui/history_lapse.cpp`, `src/core/app.hpp` · id `wizard_round_history`*

### New World wizard - the lapse rounds' legend, under the map

**Answers:** What is each line, colour and mark on this round's map?

**Because:** Every lapse round carries a legend (Ben, 2026-09-25: "we also probably need a legend for each round, just in case it isn't clear what is shown on the map"; STARTUP.md § Round 4), and the reason is on the record: BL-1097's lane line was drawn and a player still could not find it at the live click, because nothing on screen said a lane was a thing to look for. The key is EXACT BY CONSTRUCTION rather than by care: `lapse_layers_drawn` (history_lapse.cpp) is the one place each layer's 'does this round draw it' decision lives, every pass of the map gates on it, and the legend lists what it returns - so the Empires cut (Ben, 2026-09-25: no caravan, seat-captured ring, capital slide, fleets, harbours or treaty arcs on round 4) is made once and leaves the map and its key together, and a layer the record carries nothing for (a world whose span opened no sea lane) is named by neither. Its grain is the ROUND, not the frame: a layer is listed when the round draws it at some frame of its span, because a key that re-flowed as each mark faded would be a second ticker. The swatches are the painter's own glyph functions at the map's own scale, in the round's own largest realms' colours - never re-drawn icons - so a swatch cannot drift from the mark it names. The words are STARTUP.md's and the glossary's (Sea lane, Colonial tie, Post Road, Homeland on the Culture round, as its board says). It sits in the pane's band under the map, grouped into five columns (the ground, who holds it, routes, marks, at sea); where the band is too shallow the map is framed in the height left above it, and the key never covers the map, which is the subject. The sea lane's swatch is a short curved stroke, what a lane laid on its smoothed sea path reads as (BL-1124; 2026-10-03).

*Demanded by BL-1118, BL-1124 · `src/ui/history_lapse.cpp`, `src/ui/history_lapse.hpp` · id `wizard_round_legend`*

### New World wizard - round 3, Culture

**Answers:** Which peoples first set down their names, their gods and their ground?

**Because:** THE ROUNDS ARE NOT CONTINUOUS (Ben, 2026-09-09). One round covering both the peopling of the world and the empires that followed was tried and failed twice: it drew conquest with the migration already finished off-screen, and then - once the migration was moved inside it - migration with no conquest at all and a static final third. The peopling of an empty world and the contest over it have different subjects, different rules and different terminating conditions, so they are two rounds. This one is the diffusion: where people started, the routes they took, and the cultures those routes produced, ending when all land has some culture rather than at a calendar year. It keeps the 2D map, because a globe shows a world and a map shows a FRONTIER (Ben, 2026-09-08, at the live app), and it keeps the inverted model - the pass cannot be previewed per keystroke, so arriving on the round runs it and the wait IS the content. THE MAP'S COLOUR IS THE CULTURE'S, AND KIN SHARE A HUE FAMILY (BL-919): the owner on this round is a culture, and dozens of related cultures under twelve identity colours read as plaid, so the palette is derived once from the culture tree at record time - hue by root cradle with the cradles spread evenly round the wheel, each daughter a fixed step off its parent and bounded to the family's wedge, lightness stepping down by depth and bounded so deep lineages stay readable. A migration route reads as one family shading along its length; another cradle's ground reads as a different family. DRAWS THE GROUND, NOT ONLY THE FILL (Ben, 2026-09-11): rivers and the landform relief are drawn beneath the culture fill and the fill is a translucent tint over them, because a frontier is legible only against the terrain it crosses - a border that stops at a river reads as a river border, and on a flat colour it read as nothing. The frontier is drawn on both axes, each power's seat is a dot, and adjacent powers never share a hue: colours are assigned by a greedy colouring over the polity adjacency graph at record time rather than hashed from the id, over a palette wider than twelve. The terrain base is baked once per record, never re-merged per frame. WORDED AS THE RECORD, NOT THE ROUTE (BL-1135; Ben, 2026-09-25: "change the cultures wording to describe how written records are stored, so the idea is less to trace migration routes (which happened millennia earlier), and more to show how writing and preservation of culture emerges"; STARTUP.md § Round 3). The spread the map plays happened millennia before anyone could have traced it, so the round's words no longer promise the routes: the question asks which peoples set down their names, their gods and their ground; the counters read 'peoples on the record', the census 'peoples named in the record' and 'foundings'; the overrun caption says the record runs on; and a footer like the later rounds' names the span as what the ages after inherit. The map and the record are unchanged - the ticker's event prose is the record's own and keeps its words. The wording is a draft for Ben's read at the walk.

*Demanded by BL-816, BL-829, BL-860, BL-919, BL-915, BL-1135 · `src/ui/startup_screens.cpp`, `src/ui/history_lapse.cpp`, `src/ui/presentation.cpp` · id `wizard_round_migration`*

### New World wizard - round 7, Industrialisation

**Answers:** What does that ground produce, and who trades it?

**Because:** Rounds 4, 5 and 6 settle who reached this ground, who then held it, and who reached beyond it; what that ground PRODUCES is a separate question with its own expensive pass, so it is its own page with its own run and reroll (Ben, 2026-09-08) rather than a coda. It also carries the wizard's one generating press. Named Industrialisation (BL-946, BL-1067) as the honest label for 1660 -> 1960 CE. BL-1068 (round six plays the span) retires the labelled placeholder: the round is a LAPSE ROUND like the three before it, because its subject - cities lighting, rail spreading, people moving, subjects breaking away - happens across a span, and a map that moves shows it where a still page cannot. Its run is the FULL build with no stop (the world Begin builds), and it replays its own recorded span (`generation_report::body_entry::industrialisation_timelapse`, with its own battle/conquest/founding counters) on the same map, board, ticker and transport as rounds 3-5, carrying Exploration's last frame in under its opening tenth. BL-1080 (round six shows industry; Ben, 2026-09-24): watching BL-1068's round showed borders and seats moving while the phase's story - industry - did not show at all, and on the verify seed the map barely changed 1810-1960. So the record carries what the span actually computes about industry - each region's furnace crossing (with the crossings it inherited, at their own years) and each polity's industry points per step - and the round draws it three ways: an ember mark on every region that has crossed, from its crossing year on; an industry column on the board, only on a record that carries points; and each crossing named in the ticker. Rail is not drawn, because the span lays none of its own. MAP LAYER (Ben, 2026-09-24): on an antiquity-epoch world the span draws no furnace crossing, so the ember layer stayed empty while the board's industry column climbed 17.2M -> 31.4M on the verify seed; the map therefore also HEATS industry points - a stipple of sparks over each realm's ground, its coverage the square root of the realm's points per region held over the record's peak density. By polity territory, because the record samples points per polity only; sparks rather than a wash, so the polity colours and frontiers still read beneath it. The map's question on this round is therefore both who holds the ground and where its industry stands. THE ROADS CARRY ACROSS THE ROUNDS (BL-1134; Ben, 2026-09-25, walking round 6: "we lost every road between Exploration and Industrialisation"; STARTUP.md § Round 6). A resumed span notes only the promotions it makes, so each round read alone opened with none of the roads the earlier rounds laid; the round before now hands its whole network over at the seam, as the civilisation diamonds are handed over, and every road drawn at a round's close is drawn on the next round's first frame (roads_carried.lua; seed 32: 480 carried into Exploration, 542 into Industrialisation).

*Demanded by BL-816, BL-819, BL-824, BL-860, BL-946, BL-1068, BL-1080, BL-1134 · `src/ui/startup_screens.cpp`, `src/ui/history_lapse.cpp`, `src/world/history_sim.cpp` · id `wizard_round_substrate`*

---

## Settled

### Acquisitions ledger - the Purchasable / Possible groups

**Answers:** What can I buy now, and what am I saving for?

**Because:** Ben's wording, 2026-08-29, and it settles the surface rather than describing it: the question names BOTH groups, which is what makes the Possible group a goal rather than a tease. FINANCE.md gives ownership a price and a verb, and until this surface existed neither was readable anywhere - buy_corporation could be issued by the scorer and by the wire, and by no press a player could find. It earns its space on the menus-are-broad-ledgers test: an overview across the whole field of firms, never a targeted action on one. The two groups are the load-bearing part and were kept after measurement rather than assumed - Purchasable carries the Buy press, Possible carries a price and deliberately NO press, and that contrast is the only way a player sees what the next rung costs. The surface states the size of the field it draws, which is the clause that had to be re-measured: at the time this was settled the buyable field was one to three firms of eighty-eight, because ownership class was overwhelmingly closed. BL-678 (companies are open) retired closure for companies, and the same twelve-seed sweep now reads 81.6 public firms per seed - 35.8 Purchasable and 45.8 Possible, with 4.8 still closed. So the count on the face stops excusing a short list and starts doing the work the question asks of it: telling the player how much of the world is in reach today against how much is worth saving for.

*Demanded by BL-628, BL-658, BL-678 · `src/ui/acquisitions_ledger.cpp`, `src/ui/nav_pane.cpp`, `src/ui/body_surface_canvas.cpp` · id `acquisitions_ledger_field`*

### Acquisitions ledger - Profitability fold-out (full-canvas)

**Answers:** Who is making money, and what would owning them cost?

**Because:** BL-626 made every corporation file a quarterly return as world state, and BL-631 made ownership class decide who may read it - but neither put a single filed figure on screen where a player could compare firms. This is that surface: one row per corporation, sortable on every column, exact where the firm files and a dash where it does not, no bands (FINANCE.md section Disclosure; Ben 2026-08-26 retired the banded read). It earns a FULL-CANVAS takeover rather than a place in the column because of the measurement that also shrank the groups above it: 85.4 firms per seed file a return they do not disclose, so the table is ~88 rows over seven columns while the buyable field is ~1.6 rows. That inversion is the whole argument for the split - the column holds what you can act on, the takeover holds what you can read.

*Demanded by BL-627 · `src/ui/acquisitions_ledger.cpp`, `src/ui/detail_level.hpp` · id `acquisitions_ledger_profitability`*

### Company lens — background-firm holdings on the Planetary canvas

**Answers:** Who else is operating on this ground, besides the rivals I actually compete with?

**Because:** It earns its space by taking a question OFF another surface rather than adding one. Until 2026-08-28 the Corporation lens drew every non-player firm, so it answered two questions at once and therefore neither: measured on the home body, 373 background-firm buildings across 80 companies against 33 rival buildings across 7 corporations, an 11:1 ratio in which the rivals were invisible inside their own lens. Splitting the population gives each lens one question. The pair is also the cheapest possible addition to learn — drawn identically, differing only in which firms they admit, so a player who can read one can read the other with no new vocabulary. Ben settled the words on the same day: a corporation is the player and its rivals, a company is a background firm (GLOSSARY.md).

*Demanded by BL-663 · `src/ui/ui_state.hpp`, `src/ui/overlay.cpp`, `src/ui/body_surface_canvas.cpp` · id `company_lens`*

### Corporation ledger

**Answers:** How well am I doing?

**Because:** ONE card, Balance: earnings against stacked expenses, and nothing else. The surface shipped with four roll-ups and three of them were answered better elsewhere — Production and Workforce by the Construction ledger's Buildings tab, which holds the estate AND the per-building levers; Trade by the Market and Convoys ledgers. What is left that only this surface can answer is how well the corporation is doing, and the honest answer to that today is money (Ben, 2026-08-29). The card earns its column because it shows its chart AT REST rather than a verdict line over an empty column, and because the chart is the whole of the quarter's arithmetic: earnings on the left, every outflow corp_budget::net() subtracts stacked on the right, each segment naming itself and its figure on hover. Interest is absent while the corp is solvent and present once it is in debt, so the segment count itself reads the solvency boundary. It is the same ui::charts drawer the building card's Revenue/Expenses graph uses, at a different grain — one chart, one arithmetic, two surfaces that cannot disagree. "Balance" is a sub-header inside the Corporation ledger, not a competing surface name, and its overlap with the Budget ledger is explicitly ACCEPTED (Ben, 2026-08-29) rather than an oversight to clean up. Research points, score, ranking and market cap are deferred by name until core gameplay is established; several have no store behind them at all. BL-1215 (refunds flow shown) adds one EARNINGS segment and one line: a refund the quarterly return booked (BL-1206, the seat's cancelled construction) is cash in that is not earnings, so it stacks beside Income as its own named segment and is named again under the net rather than folded into it - otherwise the header's return-based net and this card's operating net disagree with no visible reason.

*Demanded by BL-081, BL-214, BL-343, BL-691 · `src/ui/corporation_dashboard.cpp`, `src/ui/charts.cpp` · id `corporation_dashboard`*

### Generation charts

**Answers:** Why did this world come out the way it did?

**Because:** Each chain stage settles one question about the body's history; the chart row is that stage's evidence. This surface carried the original BL-247 question pairs, which is why its per-round `question` field survives in generation_charts.hpp.

*Demanded by BL-191, BL-247 · `src/ui/generation_charts.cpp` · id `generation_charts`*

### Sticky detail card

**Answers:** Can I keep this detail on screen while I look at something else?

**Because:** Comparison is impossible when every selection replaces the last. The card frame is what makes drill-through (BL-214) a shared idiom rather than a per-panel behaviour. Pairs existed here before BL-247's log was removed.

*Demanded by BL-196, BL-213, BL-214 · `src/ui/selection_card.cpp` · id `selection_card`*

### New World wizard - the lapse rounds' top-sixteen board

**Answers:** Which powers are rising and which are falling as the centuries pass?

**Because:** Ordered and capped is the design, not a display convenience: sixteen rows re-ranking is the surface that shows RISE AND FALL, and an uncapped list of everything would show none of it. It orders by SHARE OF PEOPLE (BL-1000; Ben, 2026-09-15, NR-876): a polity's sampled population over every living polity's at the recorded step at or before the playhead - history_sweep's own arithmetic, so a figure on the board is the figure the sweep prints. Share of LAND was the honest default while the ownership record was all there was (BL-830), but it measures founding as much as conquest - by population the largest realms are two to five points MORE concentrated and most region-count risers were founding empty ground - so land stays as the second, qualifying column and the tie-break, and the region COUNT column went to pay for it. Where a step has no samples (the opening years; the whole Culture round) every row holds no one and the board falls back to land order. Rows are POLITIES, not settlements, because a settlement cannot rise and fall against a rival. Drawn on EVERY lapse round (BL-946 added the third, BL-1068 the fourth) from that round's own record, rather than once on a fused round. Pop (the headcount) and Might (the military capacity band, 1-6) are drawn from BL-817's per-polity sample series at the same step, so the board and the map show one instant. The arc readout under the board prints its peak on the same people column, as the sweep's peak_share_pop_q. A RESEARCH column is refused outright, because research points accrue from population under BL-822 and the column would show a correlation it never measured - on the board Ben is judging the research levers with. BL-1080: on the Industrialisation round the board carries a seventh column, industry points at the step, because that round's rise and fall is industrial; it is a column, never the rank, and the earlier rounds keep six.

*Demanded by BL-830, BL-860, BL-916, BL-946, BL-1000, BL-1080 · `src/ui/history_lapse.cpp` · id `wizard_round_history_board`*

