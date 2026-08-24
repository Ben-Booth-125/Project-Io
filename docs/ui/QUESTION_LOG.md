# UI question log — every surface states what it answers

> **Generated file — do not hand-edit.** Source of truth is
> [`question_log.json`](question_log.json); regenerate with
> `node tools/session/render_question_log.js`.

Every information surface declares the **question it answers** and **why it earns its
space**, with the backlog item that demanded it. The pair is required. Enforcement is
authorship, not machinery — there is deliberately no audit check against this file
(BL-260, Ben 2026-08-01: *"the docs are the audit"*).

**43 surfaces** — 4 settled, 39 awaiting Ben's wording.

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

### Stacked-tile ring (Planetary canvas building marker)

**Answers:** This hex holds more than one building - which KINDS are standing here?

**Because:** A tile carries as many buildings as its richness allows (BL-193, building stack capacity), and the canvas drew exactly one silhouette however many stood there. The '+N' count badge told the player a stack existed but never what was in it, and Ben rejected primary-plus-count for exactly that reason: it is always legible and never says WHICH. The ring is the only one of the three shapes considered that scales with the richness-derived cap AND names its contents - a glyph cluster becomes soup past three. It earns space it does not take from anything else: it occupies the empty annulus between the silhouette (0.48 r) and the rim, adds no chrome, no legend and no control, and it composes with the two marks already there rather than replacing them - the ring says which kinds, the centre glyph says which of them leads, the badge says how many in total. It draws nothing on a single-kind tile, so the world's ordinary built tiles are unchanged. Its LOD bound (draw_r > 10 px) is derived from the arc length one segment needs to read as a segment, and below it the tile degrades to the dominant kind's glyph alone - never to an empty hex.

*Demanded by BL-596, BL-193 · `src/ui/icons.cpp`, `src/ui/icons.hpp`, `src/ui/body_surface_canvas.cpp`, `src/ui/presentation.cpp` · id `building_stack_ring`*

### Comms dock

**Answers:** What has happened that I did not watch happen?

**Because:** The simulation runs while the player is looking elsewhere. Without a log, events are only discoverable by noticing a changed number, which is the failure mode the alerts work (BL-261) also targets.

*Demanded by BL-212, BL-216 · `src/ui/chat_panel.cpp` · id `chat_panel`*

### Construction panel

**Answers:** Can I build here, what will it cost me, and why was I refused?

**Because:** Placement carries terrain, deposit, slot and now logistics-reach rules (BL-323). A refusal the player cannot read is indistinguishable from a broken build, so the panel must state the reason, not merely deny.

*Demanded by BL-029, BL-082, BL-095, BL-367 · `src/ui/construction_panel.cpp` · id `construction_panel`*

### Contract card (Selection element, contract kind)

**Answers:** What did I actually agree to, and what have I been paid for it so far?

**Because:** SELECTION.md's battle element is this card's own precedent: a mercenary_contract has no entity id, so — like a battle — it resolves before selection_kind_of and owns its whole layout rather than the shared action|facts split. It earns its space by being the one place the four facts a contract's OWN record carries sit together: the predicate (via condition_text, the same reader the Balance ledger's laws listing already uses, so a contract's terms and a law's conditions never read in two different vocabularies), the committed force (CONTRACTS.md Q1 — the player names the force, never the contract, so the card is where that choice is read back), the deadline the tick-evaluation pass judges it against, and the fee SPLIT — deposit already paid versus the remainder still owed on completion — because 'the fee is 400cr' hides the one number that actually matters mid-contract: how much is still at stake.

*Demanded by BL-577 · `src/ui/selection.hpp`, `src/ui/selection_panel.cpp`, `src/ui/ui_state.hpp` · id `contract_card`*

### Public channel (comms dock) — mercenary-contract dispatches

**Answers:** What is happening to the mercenary work I have taken or am chasing, without me having to hold the ledger open?

**Because:** CONTRACTS.md's EVENTS.md-derived rule is that an event lands with its message in the SAME change: offer issued, accepted, completed, failed and abandoned are the five moments a contract's money and reputation actually move, and none of them had a surface before this — a completed contract paid out silently. The Public channel is the only correct home rather than a new channel of its own (unlike the battle dispatch stream): CHAT.md already settles Public as nation-voiced, and a contract's counterparty on the client side IS a nation, so the wording is the SAME first-person register post_nation_agency_comms already established, not a new voice to learn. Phrase selection folds the record's own stable id with the event kind (the BL-290 tongue-bank idiom battle_dispatch_line already uses), so replays read identically and no RNG draw is spent narrating a fact the simulation already decided.

*Demanded by BL-577 · `src/core/battle_dispatch_text.cpp`, `src/core/session_history.cpp`, `src/world/nation_step.cpp`, `src/world/nation_step.hpp` · id `contract_events_public_channel`*

### Contracts ledger - Active view

**Answers:** What am I on the hook for right now, and what does walking away cost?

**Because:** An accepted mercenary_contract commits real units and a real deposit; the player needs to see the predicate it is paying to make true (condition_text), the deadline, and the force committed to it. The Abandon press exists because CONTRACTS.md Q2 makes an early exit cheaper than a rout but never free — the reputation cost has to be shown BEFORE the press commits, not discovered afterward, or the ledger would be teaching the wrong lesson about the one number that makes abandoning a real decision.

*Demanded by BL-576 · `src/ui/contracts_ledger.cpp` · id `contracts_ledger_active`*

### Contracts ledger - History view

**Answers:** How has this line of work actually gone for me?

**Because:** CONTRACTS.md Q2 names three terminal states (completed / failed / abandoned) with three different money and reputation outcomes, deliberately against procurement's two — failure is the mechanism's teeth ('you are not paid for trying'). A player cannot read their own standing spiral (CONTRACTS.md: reputation falls, fees fall with it) without a record of which contracts ended which way and what they actually paid; without this view every terminal contract vanishes into the sentiment substrate with nothing on screen to show for it.

*Demanded by BL-576 · `src/ui/contracts_ledger.cpp` · id `contracts_ledger_history`*

### Contracts ledger - Offers view

**Answers:** Who wants to hire me, for what, and can I afford to say yes?

**Because:** CONTRACTS.md settles the mercenary contract as the sell side of the income loop: a client nation offers a fee to make a fact about the world true by a deadline. Without a surface listing open offers the player cannot act on the mechanism at all — offers exist in world::mercenary_offers whether or not anyone can see them, which is exactly the BL-089 activity-fog framing this view honours (an offer is hidden unless its target body is at least Known). The view is also where the force-picker lives: CONTRACTS.md Q1 rules the player chooses the force, never the contract, so the Accept press has to open onto something.

*Demanded by BL-576 · `src/ui/contracts_ledger.cpp` · id `contracts_ledger_offers`*

### AI decision feed

**Answers:** What did the rival corporations just decide, and how close was the call?

**Because:** The scorer has recorded its own rationale since BL-202 - the winning score, the runner-up score and a reason code, into a 256-entry ring and permanently into the history log - and nothing has ever read either store. The reasoning accumulated every tick of every session and was shown to nobody. The margin is what earns the space: a command taken at 0.81 against a runner-up of 0.79 is a coin-flip the tuning could have gone either way on, one taken at 0.90 against 0.10 is a conviction, and nothing else in the game distinguishes them. That difference is most of what 'which strategy is it running' means. It also pays as diagnostics rather than spectacle (AI_OPPONENT.md 10h): the idle/resume oscillation that was the AI's dominant behaviour for an unknown number of sprints was invisible for exactly this reason.

*Demanded by BL-407 · `src/ui/decision_feed.cpp` · id `decision_feed`*

### Economy panel (Corps / Holdings / Markets)

**Answers:** How do the corporations, their holdings and the markets compare against each other?

**Because:** The cross-corp comparison view. Ben ruled 2026-08-09 that it earns a nav-rail door rather than retirement (BL-292), so it must now justify the slot it occupies.

*Demanded by BL-063, BL-117, BL-292 · `src/ui/economy_panel.cpp` · id `economy_panel`*

### Entity summary

**Answers:** What is this entity, in one line?

**Because:** The shared per-entity content builder feeding the Selection element, the Tile Ledger and the hover card. It exists once so those three cannot drift into describing the same entity differently.

*Demanded by BL-031, BL-145 · `src/ui/entity_summary.cpp` · id `entity_summary`*

### Field channel (comms dock)

**Answers:** What happened in my fights while I was looking somewhere else?

**Because:** A battle resolves several rounds per tick and a concluded one is ERASED at the end of the tick it ends — so the aftermath, which is the single line a player most needs (who held the ground, what it cost), is unreachable from any state-reading surface by the time they could look. The dispatch stream is the only place it can live. It is a SEPARATE channel rather than more traffic in Public because its volume is driven by simulation intensity, not by scripted events: a war is several lines a tick, and mixing that into Public would bury everything else. The phase vocabulary is shared verbatim with the battle card (battle_phase_word), so the two surfaces cannot drift into describing the same fight differently.

*Demanded by BL-468 · `src/core/battle_dispatch_text.cpp`, `src/core/session_history.cpp`, `src/ui/chat_panel.hpp` · id `field_channel`*

### Generation Ledger - Body view (histograms, thresholds, profile echo)

**Answers:** What shape did this body's generation actually come out, and which input made it that shape?

**Because:** A biome-balance question ('forest and wetland stay sparse on the homeworld') was previously answered by eyeballing the map, which cannot distinguish a bad tuning constant from an unlucky seed. Putting the composition/landform histograms, the ocean threshold against the profile's target, and the profile that drove them on ONE surface is what makes the answer traceable to an input rather than to an impression. It earns its space as a tuning instrument, not shipped chrome - it is the last rail slot for that reason.

*Demanded by BL-303 · `src/ui/generation_ledger.cpp` · id `generation_ledger_body`*

### Generation Ledger - Tile derivation breadcrumb

**Answers:** Why is THIS tile what it is?

**Because:** The six-pass pipeline discards its intermediates, so a surprising tile was previously unanswerable without a debugger: the heightmap, the sea score it was tested against, the moisture and the band all vanish before the tile exists. The breadcrumb names the input value and the rule that fired at each pass, which turns 'that looks wrong' into a specific pass to go and read. It is also the shared content builder the hover card and the Selection element are intended to wrap, so the space it earns is paid for more than once.

*Demanded by BL-303 · `src/ui/generation_ledger.cpp` · id `generation_ledger_tile`*

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

### Market Ledger

**Answers:** What is this good worth here, and who is willing to trade it?

**Because:** Markets are the public intelligence channel under the BL-068 visibility rule -- a rival's production and stockpiles are private, so price and the order book are the only honest read the player has on a competitor. Without this surface the discovery model has no channel to reason through.

*Demanded by BL-122, BL-159 · `src/ui/market_ledger.cpp` · id `market_ledger`*

### Market Ledger - Convoys tab

**Answers:** What is on its way to me, and when does it land?

**Because:** Convoys are drawn on three canvases -- a moving beam on the Planetary canvas, lines on the Solar canvas, a lens glyph, an aggregated route graph -- and were LISTED nowhere, so the one number the player needs from them had no home. Travel time became load-bearing on 2026-08-12: a long haul now takes several quarters where it used to take one, and stock committed to a convoy is out of the pool for the whole of it. Without ticks-to-arrival on a surface, a player cannot tell a delivery that is late from one that was always going to be slow, and cannot plan a build against stock already in transit. The tab also gives BL-452's Hold press a per-convoy row to sit on.

*Demanded by BL-452, BL-453 · `src/ui/market_ledger.cpp` · id `market_ledger_convoys`*

### National border band (Planetary canvas, always-on chrome)

**Answers:** Whose ground is this, and where does it stop?

**Because:** A nation used to be a LENS - a territory-wide tint you had to switch to, which meant the political map was invisible unless you asked for it, and the tint occupied the same channel as terrain, texture and every other lens. The band answers the same question as always-on chrome instead, the way roads already do: colour at the boundary falling off inwards, so a nation reads as a bordered region and the middle of a territory stays free for whatever else is being shown. Two neighbours meeting therefore show two parallel rules and never average into a third nation's colour - which the old tint did, because it was composited INSIDE the blended fill. It also carries the route the lens used to own: clicking the band selects the nation, which is the only way to reach one (Ben, 2026-08-24: 'click the border itself'). The corridor is capped at 0.18 of the drawn hex radius so it narrows with the hex rather than swallowing a frontier tile, and hovering it names the nation immediately - well short of the hover card's dwell - so the target is readable before the click commits.

*Demanded by BL-601 · `src/ui/body_surface_canvas.cpp`, `src/ui/ui_state.hpp`, `docs/ui/PLANETARY.md` · id `national_border_band`*

### Nav rail

**Answers:** What can I open from here?

**Because:** Every ledger and panel needs exactly one discoverable door. BL-292 is the standing proof of the cost when a surface lacks one: the Economy panel was drawn every frame and reachable by nobody.

*Demanded by BL-022, BL-027, BL-028 · `src/ui/nav_pane.cpp` · id `nav_pane`*

### Profile panel

**Answers:** Who am I in this world?

**Because:** Identity is carried by emblem and colour across every canvas and ledger (BL-090). One place has to establish that vocabulary, or the marker colours are arbitrary everywhere else.

*Demanded by BL-090, BL-091 · `src/ui/profile_panel.cpp` · id `profile_panel`*

### The province sections of the tile Selection element (Buildings / Deposits / Population)

**Answers:** The canvas just blended several tiles into one shape - what is actually IN this locality, how much room is left in it, and who lives there?

**Because:** BL-511 removed the tile as a click target, so the mixture, the deposits and the buildings of a locality became unreachable by the gesture that used to reach them, and a CARD OF ITS OWN was the answer. BL-598 (Ben, 2026-08-24) reversed the premise rather than the answer: the tile is a click target again, and the province readings are SECTIONS of the tile element's one accordion. The question is unchanged and still earns its space - a player deciding whether a locality is worth a mine asks about the locality, not one hex - but it no longer earns a second element to ask it in. Two surfaces asking about one piece of ground made the player choose a grain before knowing what they wanted to know; one accordion, ordered from what can be acted on to what the ground merely is, does not. What was dropped in the fold is the province's own Buildings ROLL-UP (the same question the Buildings section's Built column answers, at a grain the player does not build at) and its member-tile list (whose job was to give back a tile that is no longer taken away). What was gained is Population, which had no home on either surface.

*Demanded by BL-511, BL-598 · `src/ui/selection_panel.cpp`, `src/ui/body_surface_canvas.cpp` · id `province_card`*

### Selection band - Building card (3-column band)

**Answers:** What's the state of this building, how much workforce does it have, can I close or dismantle it, and what can I do about it?

**Because:** The building card previously split action|facts like every other selection kind, with three MORE independently-toggled sections stacked below the facts column (Method/Chain/Depth, BL-431) - a tall, scrolling stack that read nothing like the tile card sitting one selection away. Moving the building onto the SAME 3-column band shape (zoomed tile render / paged accordion / action grid) makes 'select a thing, get its picture + a pager + its actions' one shape across the two entity kinds that actually get selected in play, rather than two competing layouts a player has to relearn. The former toggles become PAGES for the same reason the tile card's resource graphs are pages, not accordions: a page IS the opened state, so there is nothing left inside it to fold. 2026-08-15: Workforce (graph/Auto/tier grid) and Lifecycle (Close-Reopen/Dismantle) joined the accordion as two more pages, absorbed from the construction panel's deleted Buildings tab - this card is now the single home for a building's full detail, not a duplicate of it.

*Demanded by BL-431, BL-430, BL-428, BL-074 · `src/ui/selection_panel.cpp`, `src/ui/selection_panel.hpp`, `src/ui/selection_card.cpp`, `src/ui/detail_level.hpp`, `src/ui/ui_state.hpp` · id `selection_building_card`*

### Selection element

**Answers:** What have I selected, and what can I do with it?

**Because:** The pinned, polymorphic detail surface for the current selection. It is the answer to the click model's promise: single-click selects, and something must visibly happen when it does.

*Demanded by BL-067, BL-068, BL-071, BL-367 · `src/ui/selection_panel.cpp` · id `selection_panel`*

### Selection band - the tile element's section top nav

**Answers:** Everything this ground has to say, in the order I can act on it: what can I still build here, what does the locality hold, what does this hex yield, who works here, what is the terrain?

**Because:** The centre column was a PAGER, and the province was a second element with a pager of its own; both hid the list of questions the surface can answer behind a press. An accordion was built to show that list and was ruled out on sight, on a measurement rather than a taste: five stacked headers spent 169 of the band's 258 px on chrome to leave the open section 89. The nav keeps what the accordion was FOR - a visible sense of how many readings exist - by putting an i/N count beside the title, which costs one row instead of five, and returns the rest of the band to the reading you are actually doing. The chevrons straddle the span so the two presses are as far apart as the element allows; the title centres on the run between them; the full-canvas control is excepted and keeps the rightmost slot, which is where every other surface in the shell puts it. The ORDER is the other half of the argument (Ben, 2026-08-24): Buildings, Deposits, Resources, Population, Terrain runs from what the player can act on to what the ground merely is.

*Demanded by BL-598 · `src/ui/selection_panel.cpp`, `src/ui/ui_state.hpp` · id `selection_tile_section_nav`*

### Selection band - Unit (Soldier) card (3-column band)

**Answers:** What is this unit/unit-stack, how strong is it, what type is it, who owns it, and can I do anything with it?

**Because:** selection_kind::unit existed but fell through to the generic action/facts split with a bare Go to button - the only selection kind still on that path once the tile (BL-123) and building (BL-431 rework) cards moved to the 3-column band shape. BL-393 (UNITS_ARE_WRITE_ONLY_AND_INERT) already flags that units are largely inert in the live economy; Ben's direction was to build the CARD shape now anyway rather than wait on combat, so a unit selected today reads real unit_component fields (strength, count, roster type, owner) in the same picture/pager/actions shape as everything else, instead of standing out as the one kind that still looks unfinished. Paired with a repeat-click tile-cycle (Soldier -> Building -> Tile) in body_surface_canvas.cpp so a tile carrying a unit is actually reachable by clicking. BL-575 (unit marker + march UI, 2026-08-23) answers the "can I do anything with it" half for real: the action grid gained March (arms province-picking on the Planetary canvas, then dispatches corp_verb::march_unit on the qualifying province click), Halt (clears the standing order) and Disband (permanent, confirm popup, no refund — MILITARY.md § Marching) alongside the existing Go to, replacing three of the five reserved slots. All three route through the SAME corp-command seam corp_ai scores for rival units, so the player takes no shortcut around it.

*Demanded by BL-393, BL-575 · `src/ui/selection_panel.cpp`, `src/ui/selection_panel.hpp`, `src/ui/ui_state.hpp`, `src/ui/body_surface_canvas.cpp`, `src/core/app.cpp` · id `selection_unit_card`*

### Starting-corp selection stage (app_screen::choosing_corp, app::draw_corp_choice_screen)

**Answers:** Which of this world's corporations am I going to run - and what does each one actually open with?

**Because:** Which corp the player ran was an invisible lottery: the generator drew one and flagged it. Measured over 24 seeds (tools/verify/player_seed_sweep), that handed the player a pure-extraction corp on 13 of them, so the chain-depth ladder (BL-428) and the Method page (BL-430/BL-431) had no rung to stand on - while better openings sat unchosen in the same world. The screen earns its space by converting a hidden draw into a stated choice, which is also the honest alternative to rejection-sampling seeds (that would have hidden the distribution instead of exposing it). It shows name, industrial focus, home nation and holdings as N proc / N extr / N other. BALANCE IS DELIBERATELY ABSENT: opening balances are seeded BY the pre-game warm start, which has not run at this point, so every corp would read 0.0. And the screen deliberately does NOT rank the openings - BL-436 measured a processing facility as currently earning LESS per tick than the extraction site it replaces, so a processor-bearing corp is the DEEPER opening, not the richer one.

*Demanded by BL-435 · `src/core/app.cpp`, `src/core/app.hpp`, `src/core/verify_api.cpp`, `scripts/verify/corp_choice.lua` · id `starting_corp_choice`*

### Strategy readout (nav slot 12)

**Answers:** What strategy is emerging from each corporation's run — what mix of verbs is it taking, which spending priorities dominate, and which reasons keep firing?

**Because:** The decision feed (BL-407) shows the moves; nothing showed the SHAPE across a run — the thing Ben asked for (2026-08-14: watch AI play "and discover what strategies emerge"). The scorer has no strategy object, so an emergent strategy IS a distribution over decisions: the verb mix separates an expander from a consolidator; the must/should/nice bucket split is the most legible health signal in the stream; the reason tally is where a pathology like § 10h's idle/resume oscillation shows as two reciprocal codes. Score/margin figures deliberately absent (NR-226); no strategy is ever named (STRATEGIES.md's discovered-not-authored position).

*Demanded by BL-411 · `src/ui/strategy_readout.cpp` · id `strategy_readout`*

### Tech tree viewer (F9)

**Answers:** What can I unlock, and what stands between me and it?

**Because:** A constellation of gates is only a decision if the player can see which are reachable. BL-344 made that second half real: each node now reports EARNED, LOCKED with its unmet conditions itemised, or -- honestly -- "no gate authored", instead of showing an unevaluable string condition that could never resolve.

*Demanded by BL-087, BL-126, BL-344 · `src/ui/tech_tree_panel.cpp` · id `tech_tree_panel`*

### Tile inspector

**Answers:** What is this ground, and what could it support?

**Because:** Terrain is two axes plus a deposit profile, none of which is fully legible from the canvas colour alone. Siting is the player's central recurring decision and this is where its inputs are read.

*Demanded by BL-122, BL-144 · `src/ui/tile_inspector.cpp` · id `tile_inspector`*

### Terrain texture (Planetary canvas ground pass)

**Answers:** What is this ground made of, and how heavily is it covered?

**Because:** Colour alone could not carry both halves once BL-519 split the terrain axes. The substrate and the cover blend into ONE fill, so a rocky slope with a thin wood and a sedimentary plain with a thin wood arrive at neighbouring greens and the player cannot tell which they are siting on. Texture separates the two readings onto separate channels: a faint substrate grain that says what the ground is, and a per-tile cover pattern whose mark count and weight scale with cover_density, so a sparse wood LOOKS thin exactly where the economy already CUTS it thin. It earns its space by being free of any: it adds no chrome, no legend and no control, and it is the only way a cover boundary reads as a boundary now that BL-511's province blend deliberately smooths the fills.

*Demanded by BL-520, BL-519 · `src/ui/hex_render.cpp`, `src/ui/body_surface_canvas.cpp` · id `tile_texture`*

### Unit marker (Planetary canvas, province anchor tile)

**Answers:** Where are my (and my rivals') forces standing, and whose are they?

**Because:** Units had no on-canvas glyph at all before this (ICONS.md previously documented Unit as "(no glyph)"), reachable only by clicking the exact tile a unit stood on or cycling into it — a large province full of units was otherwise invisible on the map. BL-511 made a unit's command grain the PROVINCE (march_unit targets a province, not a tile), so the marker follows the same province-anchor convention the battle marker already established: drawn once per (province, owner) GROUP at the province's lowest-member-tile anchor, with a "+N" count badge for more than one unit in the group, rather than once per unit or per tile. The humanoid silhouette echoes the unit card's own placeholder glyph (glyph_soldier) so the canvas and the card read as one vocabulary. Carries a stub ring for contract-committed units (always false today; BL-573, a later wave of the same Sprint 16 batch, adds the real per-unit flag) so that later item needs no further UI plumbing change.

*Demanded by BL-575, BL-511 · `src/ui/body_surface_canvas.cpp`, `src/ui/icons.cpp`, `src/ui/icons.hpp`, `src/ui/ui_state.hpp` · id `unit_marker`*

---

## Settled

### Corporation dashboard

**Answers:** How is my corporation doing overall?

**Because:** Roll-up cards over holdings, balance and production, so the player has a whole-corp read without assembling it from four ledgers. Pairs existed on these cards before BL-247's log was removed. BL-343 added the sixth Finance bar, Levies: a law the player cannot see working is indistinguishable from an unimplemented one, so the levy is its own number rather than folded into maintenance.

*Demanded by BL-081, BL-214, BL-343 · `src/ui/corporation_dashboard.cpp` · id `corporation_dashboard`*

### Corporation panel — stance column

**Answers:** What is my stance toward this rival, and can I change it?

**Because:** BL-448 landed a corp stance data model (friend/neutral/hostile) with zero UI — the exact BL-350 lesson (a complete seam with no press, unnoticed for weeks) this item exists to avoid. The column shows the current stance label and the legal transition presses (Declare Hostile, Offer/Accept Friendship, Return to Neutral), gated on ordinary BL-068 competitor-visibility per NR-350 (a hostile declaration stays silent, discovered on contact rather than announced).

*Demanded by BL-449, BL-448 · `src/ui/corporation_panel.cpp` · id `corporation_panel`*

### Generation charts

**Answers:** Why did this world come out the way it did?

**Because:** Each chain stage settles one question about the body's history; the chart row is that stage's evidence. This surface carried the original BL-247 question pairs, which is why its per-round `question` field survives in generation_charts.hpp.

*Demanded by BL-191, BL-247 · `src/ui/generation_charts.cpp` · id `generation_charts`*

### Sticky detail card

**Answers:** Can I keep this detail on screen while I look at something else?

**Because:** Comparison is impossible when every selection replaces the last. The card frame is what makes drill-through (BL-214) a shared idiom rather than a per-panel behaviour. Pairs existed here before BL-247's log was removed.

*Demanded by BL-196, BL-213, BL-214 · `src/ui/selection_card.cpp` · id `selection_card`*

