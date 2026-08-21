# UI question log — every surface states what it answers

> **Generated file — do not hand-edit.** Source of truth is
> [`question_log.json`](question_log.json); regenerate with
> `node tools/session/render_question_log.js`.

Every information surface declares the **question it answers** and **why it earns its
space**, with the backlog item that demanded it. The pair is required. Enforcement is
authorship, not machinery — there is deliberately no audit check against this file
(BL-260, Ben 2026-08-01: *"the docs are the audit"*).

**28 surfaces** — 4 settled, 24 awaiting Ben's wording.

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

### Comms dock

**Answers:** What has happened that I did not watch happen?

**Because:** The simulation runs while the player is looking elsewhere. Without a log, events are only discoverable by noticing a changed number, which is the failure mode the alerts work (BL-261) also targets.

*Demanded by BL-212, BL-216 · `src/ui/chat_panel.cpp` · id `chat_panel`*

### Construction panel

**Answers:** Can I build here, what will it cost me, and why was I refused?

**Because:** Placement carries terrain, deposit, slot and now logistics-reach rules (BL-323). A refusal the player cannot read is indistinguishable from a broken build, so the panel must state the reason, not merely deny.

*Demanded by BL-029, BL-082, BL-095, BL-367 · `src/ui/construction_panel.cpp` · id `construction_panel`*

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

### Market Ledger

**Answers:** What is this good worth here, and who is willing to trade it?

**Because:** Markets are the public intelligence channel under the BL-068 visibility rule -- a rival's production and stockpiles are private, so price and the order book are the only honest read the player has on a competitor. Without this surface the discovery model has no channel to reason through.

*Demanded by BL-122, BL-159 · `src/ui/market_ledger.cpp` · id `market_ledger`*

### Market Ledger - Convoys tab

**Answers:** What is on its way to me, and when does it land?

**Because:** Convoys are drawn on three canvases -- a moving beam on the Planetary canvas, lines on the Solar canvas, a lens glyph, an aggregated route graph -- and were LISTED nowhere, so the one number the player needs from them had no home. Travel time became load-bearing on 2026-08-12: a long haul now takes several quarters where it used to take one, and stock committed to a convoy is out of the pool for the whole of it. Without ticks-to-arrival on a surface, a player cannot tell a delivery that is late from one that was always going to be slow, and cannot plan a build against stock already in transit. The tab also gives BL-452's Hold press a per-convoy row to sit on.

*Demanded by BL-452, BL-453 · `src/ui/market_ledger.cpp` · id `market_ledger_convoys`*

### Nav rail

**Answers:** What can I open from here?

**Because:** Every ledger and panel needs exactly one discoverable door. BL-292 is the standing proof of the cost when a surface lacks one: the Economy panel was drawn every frame and reachable by nobody.

*Demanded by BL-022, BL-027, BL-028 · `src/ui/nav_pane.cpp` · id `nav_pane`*

### Profile panel

**Answers:** Who am I in this world?

**Because:** Identity is carried by emblem and colour across every canvas and ledger (BL-090). One place has to establish that vocabulary, or the marker colours are arbitrary everywhere else.

*Demanded by BL-090, BL-091 · `src/ui/profile_panel.cpp` · id `profile_panel`*

### Province card (Selection element, Planetary rung)

**Answers:** The canvas just blended four tiles into one shape — what is actually IN it, and what can I do there?

**Because:** BL-511 removed the tile as a click target, so the mixture, the deposits and the buildings of a locality became unreachable by the gesture that used to reach them. The card is where they go: the mixture bar is specifically the BLEND LEGEND (it says what the gradient is made of, which the gradient itself deliberately smooths away), the member-tile list keeps the tile visible as the data grain Ben ruled it remains, and the summed deposits answer the question a player actually asks of a locality rather than of one hex. Without it the render change would have been a net loss of information.

*Demanded by BL-511 · `src/ui/selection_panel.cpp`, `src/ui/body_surface_canvas.cpp` · id `province_card`*

### Selection band - Building card (3-column band)

**Answers:** What's the state of this building, how much workforce does it have, can I close or dismantle it, and what can I do about it?

**Because:** The building card previously split action|facts like every other selection kind, with three MORE independently-toggled sections stacked below the facts column (Method/Chain/Depth, BL-431) - a tall, scrolling stack that read nothing like the tile card sitting one selection away. Moving the building onto the SAME 3-column band shape (zoomed tile render / paged accordion / action grid) makes 'select a thing, get its picture + a pager + its actions' one shape across the two entity kinds that actually get selected in play, rather than two competing layouts a player has to relearn. The former toggles become PAGES for the same reason the tile card's resource graphs are pages, not accordions: a page IS the opened state, so there is nothing left inside it to fold. 2026-08-15: Workforce (graph/Auto/tier grid) and Lifecycle (Close-Reopen/Dismantle) joined the accordion as two more pages, absorbed from the construction panel's deleted Buildings tab - this card is now the single home for a building's full detail, not a duplicate of it.

*Demanded by BL-431, BL-430, BL-428, BL-074 · `src/ui/selection_panel.cpp`, `src/ui/selection_panel.hpp`, `src/ui/selection_card.cpp`, `src/ui/detail_level.hpp`, `src/ui/ui_state.hpp` · id `selection_building_card`*

### Selection element

**Answers:** What have I selected, and what can I do with it?

**Because:** The pinned, polymorphic detail surface for the current selection. It is the answer to the click model's promise: single-click selects, and something must visibly happen when it does.

*Demanded by BL-067, BL-068, BL-071, BL-367 · `src/ui/selection_panel.cpp` · id `selection_panel`*

### Selection band - Unit (Soldier) card (3-column band)

**Answers:** What is this unit/unit-stack, how strong is it, what type is it, who owns it, and can I do anything with it?

**Because:** selection_kind::unit existed but fell through to the generic action/facts split with a bare Go to button - the only selection kind still on that path once the tile (BL-123) and building (BL-431 rework) cards moved to the 3-column band shape. BL-393 (UNITS_ARE_WRITE_ONLY_AND_INERT) already flags that units are largely inert in the live economy; Ben's direction was to build the CARD shape now anyway rather than wait on combat, so a unit selected today reads real unit_component fields (strength, count, roster type, owner) in the same picture/pager/actions shape as everything else, instead of standing out as the one kind that still looks unfinished. Paired with a repeat-click tile-cycle (Soldier -> Building -> Tile) in body_surface_canvas.cpp so a tile carrying a unit is actually reachable by clicking.

*Demanded by BL-393 · `src/ui/selection_panel.cpp`, `src/ui/selection_panel.hpp`, `src/ui/ui_state.hpp`, `src/ui/body_surface_canvas.cpp` · id `selection_unit_card`*

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

