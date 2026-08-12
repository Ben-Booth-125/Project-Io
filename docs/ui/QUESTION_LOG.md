# UI question log — every surface states what it answers

> **Generated file — do not hand-edit.** Source of truth is
> [`question_log.json`](question_log.json); regenerate with
> `node tools/session/render_question_log.js`.

Every information surface declares the **question it answers** and **why it earns its
space**, with the backlog item that demanded it. The pair is required. Enforcement is
authorship, not machinery — there is deliberately no audit check against this file
(BL-260, Ben 2026-08-01: *"the docs are the audit"*).

**16 surfaces** — 3 settled, 13 awaiting Ben's wording.

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

### Economy panel (Corps / Holdings / Markets)

**Answers:** How do the corporations, their holdings and the markets compare against each other?

**Because:** The cross-corp comparison view. Ben ruled 2026-08-09 that it earns a nav-rail door rather than retirement (BL-292), so it must now justify the slot it occupies.

*Demanded by BL-063, BL-117, BL-292 · `src/ui/economy_panel.cpp` · id `economy_panel`*

### Entity summary

**Answers:** What is this entity, in one line?

**Because:** The shared per-entity content builder feeding the Selection element, the Tile Ledger and the hover card. It exists once so those three cannot drift into describing the same entity differently.

*Demanded by BL-031, BL-145 · `src/ui/entity_summary.cpp` · id `entity_summary`*

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

### Nav rail

**Answers:** What can I open from here?

**Because:** Every ledger and panel needs exactly one discoverable door. BL-292 is the standing proof of the cost when a surface lacks one: the Economy panel was drawn every frame and reachable by nobody.

*Demanded by BL-022, BL-027, BL-028 · `src/ui/nav_pane.cpp` · id `nav_pane`*

### Profile panel

**Answers:** Who am I in this world?

**Because:** Identity is carried by emblem and colour across every canvas and ledger (BL-090). One place has to establish that vocabulary, or the marker colours are arbitrary everywhere else.

*Demanded by BL-090, BL-091 · `src/ui/profile_panel.cpp` · id `profile_panel`*

### Selection element

**Answers:** What have I selected, and what can I do with it?

**Because:** The pinned, polymorphic detail surface for the current selection. It is the answer to the click model's promise: single-click selects, and something must visibly happen when it does.

*Demanded by BL-067, BL-068, BL-071, BL-367 · `src/ui/selection_panel.cpp` · id `selection_panel`*

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

### Generation charts

**Answers:** Why did this world come out the way it did?

**Because:** Each chain stage settles one question about the body's history; the chart row is that stage's evidence. This surface carried the original BL-247 question pairs, which is why its per-round `question` field survives in generation_charts.hpp.

*Demanded by BL-191, BL-247 · `src/ui/generation_charts.cpp` · id `generation_charts`*

### Sticky detail card

**Answers:** Can I keep this detail on screen while I look at something else?

**Because:** Comparison is impossible when every selection replaces the last. The card frame is what makes drill-through (BL-214) a shared idiom rather than a per-panel behaviour. Pairs existed here before BL-247's log was removed.

*Demanded by BL-196, BL-213, BL-214 · `src/ui/selection_card.cpp` · id `selection_card`*

