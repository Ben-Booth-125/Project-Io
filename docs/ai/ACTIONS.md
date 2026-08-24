# Project Io — the action dictionary

Every control in the game: what pressing it does, and why you would. Readable
mirror of [`ACTIONS.json`](ACTIONS.json), which is canonical — the JSON is the
machine-consumable half an AI player reads (BL-270). Pair it with the corp
blackboard export (BL-206, the read channel) and the corp-command seam
(`src/world/corp_command.hpp`, the write channel) and an LLM has the word
interface to play through.

**What that does and does not cover.** Every entry in the *gameplay* family is
issuable as a `corp_verb` — true since BL-293 (2026-08-08) closed the last three
gaps, and worth restating here because this line claimed it for a while before it
was so. The other four families are view controls that were never on the command
seam by design, and the order book's buy side has a save format but no verb yet.

> **Generated file.** Produced by `node tools/session/render_actions.js`.
> Edit the JSON, then re-run; hand edits here are overwritten.

*146 entries — 27 gameplay · 24 canvas · 15 lens · 47 ledger · 33 chrome.*

---

## Gameplay — presses that mutate the world

### `gameplay.accept_quote` — No UI. SEAM-ONLY: this verb has NO player-facing press. A grep of src/ui for it returns nothing — BL-350 landed the procurement seam, its world state, its serialisation and its harness, but no UI. It is reachable ONLY through the corp-command seam: an out-of-process agent via ProjectIo --serve / the MCP server's issue_command, or a harness. The rival scorer does not emit it either (corp_ai.cpp enumerates no procurement candidate). Recorded rather than omitted, because an entry that says 'no human can press this, you can' is exactly the information a language policy needs — and because omitting it is how this dictionary fell four verbs behind the enum it transcribes.

**Press.** None. `COMMAND corp=<you> verb=13 order=<quote id>` on the --serve line protocol, or issue_command with verb 'accept_quote'.

| Arg | Type | Meaning |
|---|---|---|
| `order` | `uint32 (procurement_quote id)` | The live quote to convert into a contract. Reuses the command's `order` field, shared with remove_sell_order and cancel_contract. |

**Valid when:**
- `order` is non-zero and names a quote still in world.procurement_quotes (rejected_invalid otherwise — a quote already accepted has been erased, so accepting twice fails here).
- You are that quote's buyer (rejected_not_owner otherwise).
- Your balance covers the deposit — quantity x unit_price x deposit_fraction (rejected_funds otherwise).

**Expected output.** A procurement_contract appended to world.procurement_contracts carrying a fresh id and the quote's terms, with ticks_elapsed 0 and deposit_paid recorded. Your balance is debited the DEPOSIT only; the remainder is paced across lead_time_ticks as the contract runs. The quote is erased — consumed, not marked — so it cannot be accepted twice.

**Reason to select.** Convert a price you have been offered into a delivery you are owed, accepting a deposit you will forfeit if you cancel. Weigh it against simply buying on the open market: a contract fixes a price and a schedule where the market fixes neither, which is worth most when the good's price is volatile or its local supply is thin — exactly the case where request_quote's no_input_access decline would have warned you off. Note there is no verb to renegotiate: the only exits are letting it run or cancel_contract, which forfeits.

### `gameplay.build` — Tile construction ledger (fold-out column), opened from the Selection band of a selected tile; a shortcut lives on a selected owned building ('Build another here').

**Press.** Single-click a tile on the Planetary canvas (the Selection band appears), click 'Construct Buildings' in the band's action grid, then click 'Build' on a candidate row in the ledger. Rows are one per extractable resource deposited on the tile (plus a coastal Fishing Wharf row even with zero deposit), one per processing recipe, then Port, Launchpad, Inland Logistics Hub, Military Base. Alternate press: with an owned building selected, 'Build another here' repeats its type/target on the same tile, gated by the tile's stack capacity.

| Arg | Type | Meaning |
|---|---|---|
| `tile` | `entity_id` | The selected tile the building goes on. |
| `type` | `building_type` | Which building — extraction_site, processing_facility, port, launchpad, inland_logistics_hub, military_base. Set by which row is pressed. |
| `target` | `resource_type` | Extraction rows only: the deposited resource the site extracts. Ignored for other types. |
| `recipe` | `uint16 recipe id` | Processing rows only: the recipe the facility is seeded with (the row it was priced on). no_recipe elsewhere; a recipe-less processor seeds the default steel recipe. Forwarded through the seam since BL-388 (2026-08-13): the built facility carries exactly the recipe named. A recipe on any non-processing type, or an id the registry does not resolve, is rejected_invalid — stated-but-wrong is refused, never silently coerced. |

**Valid when:**
- The acting corporation exists (rejected_no_corp otherwise).
- The tile entity exists (rejected_invalid otherwise).
- A stated recipe (anything but no_recipe) requires type = processing_facility AND an id the recipe registry resolves (rejected_invalid otherwise, checked before placement — BL-388).
- placement_rules::can_place_in_world accepts (type, target) on this tile — ocean, missing deposit, a port off the coast, or a full per-body slot cap (Launchpad: max 1 per body) all refuse (rejected_placement).
- The building type is UNLOCKED for the acting corporation (BL-344). Today exactly one type is gated: military_base requires the tech E0-ML-01 "Standing Garrison Doctrine", earned automatically once the corp owns at least 2 extraction sites AND holds a balance of at least Cr 2,000. A locked row shows "Locked - the technology that permits this has not been researched" in place of the Build button, and construct_building refuses with tech_locked, which the corp-command seam reports as rejected_tech_locked.
- The corporation's balance covers the full capex: registry build_cost plus the material costs priced at the tile's local market (rejected_funds). The UI shows this one credit total and disables Build with 'Can't afford' when short.

**Expected output.** The press enqueues a construction request; the app's mutable pass executes construct_building the same frame. On success a building entity exists immediately — staffed at 50% workforce (0 for a port), a processing facility seeded with the pressed row's recipe — and the capex is debited up front. Construction is then DURATIVE and material-gated: each economy tick (one quarter) it advances at a rate in [0,1] set by how much of its per-tick material need the local market can supply; scarce materials stretch the ETA and total shortage shows 'Paused - market can't supply materials'. Management controls unlock only when construction completes. A rejected attempt mutates nothing; the reason string appears at the top of the ledger (construction.last_message), and invalid rows already show reason-coded text in place of the Build button ('Cannot build on water', 'A port must sit on the coast', ...).

NOTE (BL-389): through `ProjectIo --serve` the world is generated WITHOUT scripts/world_gen.lua, so only 16 of 37 resources carry a market price and coal is not among them. Since steel's recipe needs coal, a processing facility built through the seam has never produced a unit in any recorded session. Extraction works; processing does not, and the cause is configuration rather than the verb.

**Reason to select.** The only way to add productive capacity. Extraction turns a tile deposit into pool stock to sell; processing turns inputs into higher-margin outputs; ports/hubs move goods cheaper, a launchpad gates space access, and a military base is where units muster (BL-325; hire moves onto it in S2 — until then it is positioning ahead of that change). The ledger ranks candidates by expected net per quarter and prints payback, so build is the press when a candidate's expected profit beats holding the cash.

### `gameplay.cancel_contract` — No UI. SEAM-ONLY: this verb has NO player-facing press. A grep of src/ui for it returns nothing — BL-350 landed the procurement seam, its world state, its serialisation and its harness, but no UI. It is reachable ONLY through the corp-command seam: an out-of-process agent via ProjectIo --serve / the MCP server's issue_command, or a harness. The rival scorer does not emit it either (corp_ai.cpp enumerates no procurement candidate). Recorded rather than omitted, because an entry that says 'no human can press this, you can' is exactly the information a language policy needs — and because omitting it is how this dictionary fell four verbs behind the enum it transcribes.

**Press.** None. `COMMAND corp=<you> verb=14 order=<contract id>` on the --serve line protocol, or issue_command with verb 'cancel_contract'.

| Arg | Type | Meaning |
|---|---|---|
| `order` | `uint32 (procurement_contract id)` | The in-flight contract to terminate. Reuses the command's `order` field. |

**Valid when:**
- `order` is non-zero and names a contract still in world.procurement_contracts (rejected_invalid otherwise).
- You are that contract's buyer (rejected_not_owner otherwise).

**Expected output.** The contract is ERASED, not flagged — a cancelled contract has nothing left to pace. The deposit already paid is forfeit: there is no refund. Your (buyer, supplier) reputation — the Trust dimension of sentiment since BL-546 — moves DOWN by reputation_on_cancel, which is the same figure request_quote's reputation floor is checked against. The move is PERSISTENT but not permanent: it decays toward neutral at the authored rate (economy.sentiment.trust_decay_per_tick, a nine-quarter half-life — NR-568), so one cancellation is half-forgotten nine ticks later and gone entirely in time, while cancellations faster than the decay compound and will make that supplier refuse to quote you at all.

**Reason to select.** Cut a commitment whose remaining payments cost more than the goods are now worth to you — a price collapse, a chain you have abandoned, or cash needed elsewhere this quarter. The cost is deliberately two-sided and the second side is the one to reason about: the forfeited deposit is a one-off you can price, but the reputation move is persistent and compounds with decay rather than being permanent — each cancellation stacks on whatever earlier ones have not yet decayed away, and it is checked at request_quote time. Spacing cancellations further apart than the nine-tick half-life keeps the relationship recoverable; bunching them does not. A policy that cancels freely will find its supplier list shrinking for reasons that surface much later, as rejected_reputation on a supplier it had been relying on.

### `gameplay.demolish` — Selection band, player-owned building layout, bottom action row.

**Press.** Select the owned building, click 'Demolish', then confirm 'Demolish' in the popup (the popup exists because the act is irreversible; 'Cancel' backs out).

| Arg | Type | Meaning |
|---|---|---|
| `building` | `entity_id` | The selected building to remove. Must be owned by the acting corporation. |

**Valid when:**
- The acting corporation exists (rejected_no_corp otherwise).
- The building entity exists (rejected_invalid otherwise).
- The corporation owns the building (rejected_not_owner otherwise).

**Expected output.** Enqueued at confirm; executed against the mutable world after the draw. The building's components are erased, its id is removed from the corporation's assets, and the tile slot is freed for something else. NO refund — the build cost is not returned; demolition destroys the asset outright. A rejected attempt changes nothing and shows nothing beyond the world staying identical.

**Reason to select.** Frees a capacity-capped tile for a better use — the press when the tile is worth more re-purposed than the building is worth running. For a merely loss-making building, idle is the better press: demolition is irreversible and refundless.

### `gameplay.hire_unit` — The 'Hire' block of the Selection info element, shown when the selected tile carries the player's own COMPLETED military base (src/ui/selection_panel.cpp). Also a corp_verb, so an agent issues it against the corp-command seam and a rival corp's scorer issues it too (BL-324, widened by the standing-rules exception).

**Press.** Select a tile carrying your completed military base. The Selection band shows a 'Hire' list, one row per roster row currently available to your corporation under the campaign roster band (industrial). Click 'Hire' on a row. The button queues a corp_command applied at the frame's end through apply_corp_command — the same call an agent makes, with the same re-validation.

| Arg | Type | Meaning |
|---|---|---|
| `tile` | `entity_id` | The muster tile — must carry the acting corporation's own completed, non-decommissioned military_base. Carried in the command's `tile` field, the build/place_road convention. |
| `unit_type` | `uint16 (index into unit_roster_table())` | Which roster row to raise. Validated against the LIVE availability list, not a caller's cached one. |

**Valid when:**
- `unit_type` is a valid index into unit_roster_table() (rejected_invalid otherwise).
- That row is currently available to this corporation under the campaign roster band — re-checked against available_rows() at apply time, so a stale list is refused (rejected_invalid otherwise).
- `tile` names a real tile (rejected_invalid otherwise).
- That tile carries a military_base owned by the acting corporation, COMPLETE (ticks_remaining <= 0) and not decommissioned. A base still under construction does not qualify — BL-325 S2 moved hire onto the base and superseded BL-324's hire-anywhere (rejected_placement otherwise).
- The corporation can pay the row's hire cost, BOTH legs (rejected_funds otherwise): a credit cost of hire_base_cost + hire_cost_per_power × the row's power_mod (economy.lua § military, BL-394 — a floor, so even an ungated row is never free), plus a flat resource draw per gated axis of the row's gate from the corp's (corp, body) pools. A refusal on either leg leaves the corporation wholly uncharged.

**Expected output.** A new unit entity at the muster tile, owned by the acting corporation, with count and strength both set to hire_batch_manpower and `type` set to the roster index. The corporation's balance is debited the row's credit cost, and each gated axis of the row draws its flat resource cost from the corp's pools. The new entity id is returned through apply_corp_command's out-param (the same channel a `build` uses). An `agency_event::kind::hired` is reported for the chat feed and an `agency` entry is appended to the world history log.

**Reason to select.** You want force in the field, and this is currently the only verb that creates any. Two honest caveats an agent should weigh. First, it is gated behind infrastructure: you need a completed military_base on the tile, which is itself a build gated behind the E0-ML-01 tech, so hiring is the END of a chain rather than a quick move. Second — and this is the one that should hold most policies back — the 1960 campaign has NO CONSUMER for units. resolve_battle's only caller is the Era -1 history simulation, and the two condition subjects that read w.units (military_units, military_strength) have no authored producers. A hired unit today is an entity that costs money, sits on a tile, and does nothing. Raise them to satisfy a condition_set that counts them, or because a future conflict layer will want them; do not raise them expecting to fight.

### `gameplay.idle` — The 'Idle' button on the Selection band's building layout, bottom row (paired with Demolish). The Building panel's inline detail carries a one-way 'Decommission' button with the same effect; Resume lives only on the band.

**Press.** Select the owned, running building, click 'Idle'. No confirmation — the act is reversible.

| Arg | Type | Meaning |
|---|---|---|
| `building` | `entity_id` | The building to idle. |

**Valid when:**
- The acting corporation exists (rejected_no_corp otherwise).
- The corporation owns the building (rejected_not_owner otherwise).
- The building is not already idled (rejected_state — a no-op in current state).

**Expected output.** decommissioned := true immediately. From the next economy tick production stops and wages stop; material upkeep CONTINUES, and the building keeps occupying its tile. Status surfaces read 'Decommissioned' / 'Idled - producing nothing'. Fully reversible via resume. Rejected attempts change nothing.

**Reason to select.** A persistently loss-making building burns wages and maintenance every quarter; idling stops the wage bleed while keeping the asset and its tile for when prices recover. The reversible alternative to demolition — pay a small upkeep to keep the option open.

### `gameplay.place_road` — Three road-tier rows (Track / Road / Highway) at the bottom of the tile construction ledger, below the building candidates.

**Press.** Select a tile, click 'Construct Buildings' on the Selection band, scroll to the road rows, click 'Build' on a tier. Each row shows its own credit total and validity.

| Arg | Type | Meaning |
|---|---|---|
| `tile` | `entity_id` | The tile whose road level rises. |
| `road_tier` | `uint8 [1, 3]` | 1 = Track, 2 = Road, 3 = Highway. Higher tiers cost more and cut traversal cost further. |

**Valid when:**
- The acting corporation exists (rejected_no_corp otherwise).
- The tile entity exists (rejected_invalid otherwise).
- can_place_road accepts: a land tile whose current road_level is BELOW the pressed tier — upgrade-in-place is allowed, re-laying the same or a lower tier is refused, ocean is refused (rejected_placement).
- The balance covers the tier's build cost plus materials priced at the tile's local market (rejected_funds; the row disables Build with 'Can't afford').

**Expected output.** INSTANT, unlike building construction: on execution the tile's road_level rises to the tier, the full cost is debited up front, and the world's A* cost cache is invalidated so freight dispatch immediately sees the lower traversal cost through this tile. A road is a tile mutation, not a building — it occupies no build slot. Rejected attempts change nothing; invalid tiers show their reason string in place of the button.

**Reason to select.** Lowers intra-body haul cost along the tile, cheapening every supply run that routes through it — the press that tightens the chain between extraction sites, processors, and the market. Worth it on tiles carrying real freight; a road on an unused tile is pure spend.

### `gameplay.place_sell_order` — The 'Sell Orders' tab of the Market Ledger (opened from the nav rail; the tab is scoped to the ledger's selected market body). Since BL-293 the press is not the only route: place_sell_order is a corp_verb, so an agent issues it directly against the corp-command seam and a rival corp's scorer issues it too.

**Press.** Open the Market Ledger, switch to the Sell Orders tab, pick a resource in the combo (only resources the market prices are listed), type a Quantity/qtr and a Floor price, click 'Add sell order'. The button is disabled with no resource chosen or quantity <= 0. The button does not mutate anything itself — it queues a corp_command that the frame's end applies through apply_corp_command, which is the same call an agent makes.

| Arg | Type | Meaning |
|---|---|---|
| `body` | `entity_id` | The body whose market the order lists on — the ledger's currently selected body. Carried in the command's `subject` field (the survey verb's convention: subject IS the body). |
| `resource` | `resource_type` | What to sell. Carried in the command's `target` field, shared with build's extraction target. Must have a positive base price on this market. |
| `quantity` | `float > 0` | Maximum units offered per quarter, capped each tick by what the (corp, body) pool actually holds. |
| `floor_price` | `float >= 0` | Minimum acceptable unit price — a RESERVATION price (BL-386, fixed 2026-08-14). 0 means sell at the market price. An order whose floor exceeds the resolved market price clears NOTHING that tick: the stock stays in the pool and waits. An order at or below it clears at the RESOLVED price — the floor is never what you are paid, only the line below which you refuse to sell. (History: until 2026-08-14 the engine paid max(resolved, floor) with no buyer debited — a money-printing defect this entry used to carry a warning about. It is fixed; a high floor now holds stock instead of printing.) |

**Valid when:**
- The acting corporation exists (rejected_no_corp otherwise).
- `subject` names a real body (rejected_invalid otherwise).
- That body carries at least one market (rejected_invalid otherwise; the ledger renders 'This body has no market.' and no form at all).
- The resource is priced on that market — base_price > 0 (rejected_invalid otherwise).
- Quantity is strictly positive and floor_price is not negative (rejected_invalid otherwise).
- The corporation holds fewer than `max_sell_orders_per_corp` (64) standing orders (rejected_state otherwise — the book is capped, and a placement past the cap is refused rather than silently dropped).

**Expected output.** A STANDING order in WORLD state — `world::sell_orders`, not a UI list — carrying a stable nonzero `id` that `remove_sell_order` later names. It persists until removed, survives a save (the order book is on the flat-binary serialisation seam, order_book.hpp), and is evaluated every economy tick by clear_markets itself, with no caller handing it over. Each tick it lists up to `quantity` from the corp's pool on that body (an empty pool lists nothing, silently); unmatched quantity clears at the resolved market price only when the floor permits (floor <= resolved); a floor above the market clears nothing that tick and the stock stays in the pool.

(BL-386 landed 2026-08-14: the floor is now a genuine reservation price. The self-contradiction this entry used to carry — max() paid vs "nothing sells" — is resolved in favour of the second half.)

CRITICAL side effect: the (corp, body, resource) triple leaves the automatic surplus-selling path — the auto path yields to the standing order — so a too-high floor stops that resource selling AT ALL on that body and the stock will pile up. Orders append, so position in the book is time priority; a rejected command mutates nothing.

WHAT ACTUALLY HAPPENS TO YOUR THROUGHPUT, learned in play and not obvious from the verb: a standing order makes the auto-surplus path YIELD for that (corp, body, resource), and the order then sells at most its own listed `quantity` per tick where the auto path sold the whole pool. Listing at floor 0 with a large quantity is therefore not equivalent to not listing — one player measured 2,557,231 credits against 9,613,476 for the same campaign with no order at all, with 15 million unsold units piled in the pool. The listed quantity is frozen at listing time and the scorer never revises it (BL-380), and a second order on the same triple sells nothing because time priority gives the first one the whole pool. Also: the response does NOT return the order's id, which `remove_sell_order` requires (BL-390).

**Reason to select.** Price and quantity control the auto-sell path lacks: floor-protect against dumping stock into a crashed price, or meter quantity to ration a stockpile toward a construction project or a better market. The manual side of the trade loop — the press when 'sell everything at whatever it fetches' is the wrong answer. The rival-corp scorer reaches for it on exactly one signal: stock past a hold threshold, listed at a floor over the rarity price.

### `gameplay.remove_sell_order` — The 'Remove' button beside each listed order on the Market Ledger's Sell Orders tab; also a corp_verb, issuable directly.

**Press.** Open the Market Ledger, switch to the Sell Orders tab; each of the player's standing orders on the selected body renders as a row ('<resource> x<qty> >= <floor>') with a small 'Remove' button. Click Remove on the target row. Immediate — no confirmation. The row queues a corp_command naming the order's id.

| Arg | Type | Meaning |
|---|---|---|
| `order` | `uint32 order id` | Which standing order to remove, by its stable `sell_order::id` — NOT its index. The id is allocated once at placement, never reused, and unaffected by the removal of any other order, so a command composed against one order cannot be invalidated by a concurrent removal of another. Only the player's own orders on the ledger's selected body are listed, but the verb can name any order the acting corp owns. |

**Valid when:**
- The acting corporation exists (rejected_no_corp otherwise).
- `order` is nonzero and names an order in the ACTING CORP'S OWN book (rejected_invalid otherwise). A foreign order id and a nonexistent one are deliberately indistinguishable — both answer rejected_invalid (BL-397, 2026-08-14: the old rejected_not_owner for foreign orders was an oracle that let an id sweep map the entire global book without placing an order).

**Expected output.** The order is erased from `world::sell_orders` immediately, at no cost; every surviving order keeps its id. From the next economy tick that quantity is no longer listed. Routing consequence, unchanged: if this was the LAST remaining order for that (corp, body, resource) triple, the triple returns to the automatic surplus-selling path — each tick the pool's surplus above processor reservation auto-sells at the market's reference price, with no floor protection. If another order for the same triple still stands, manual control persists. Nothing else changes; already-cleared past sales are untouched. A rejected removal mutates nothing.

**Reason to select.** Ends manual control over a resource's sales: hand it back to the auto-surplus path when floor-pricing or rationing is no longer wanted (e.g. the price crash passed, or a too-high floor was silently stockpiling the resource instead of selling it). Also the only way to correct a mistyped order — there is no edit; remove and re-add.

### `gameplay.request_quote` — No UI. SEAM-ONLY: this verb has NO player-facing press. A grep of src/ui for it returns nothing — BL-350 landed the procurement seam, its world state, its serialisation and its harness, but no UI. It is reachable ONLY through the corp-command seam: an out-of-process agent via ProjectIo --serve / the MCP server's issue_command, or a harness. The rival scorer does not emit it either (corp_ai.cpp enumerates no procurement candidate). Recorded rather than omitted, because an entry that says 'no human can press this, you can' is exactly the information a language policy needs — and because omitting it is how this dictionary fell four verbs behind the enum it transcribes.

**Press.** None. Issue the corp_command directly: `COMMAND corp=<you> verb=12 subject=<body> target=<resource> quantity=<n> counterparty=<supplier corp>` on the --serve line protocol, or issue_command with verb 'request_quote' through the MCP server.

| Arg | Type | Meaning |
|---|---|---|
| `counterparty` | `entity_id` | The supplier corporation being asked. May not be yourself. |
| `target` | `resource_type` | The good sought. Shares the command's `target` field with build's extraction target. |
| `quantity` | `float > 0` | Units sought. Also drives the derived lead time — a bigger order takes longer. |
| `subject` | `entity_id` | NOT READ. Documented as 'the body the contract would fulfil at', and this entry previously said the seam resolves the body from the supplier's capacity so the field is 'context rather than a constraint'. That understated it: `cmd.subject` appears nowhere in request_quote's implementation. Passing a garbage id is indistinguishable from passing the right one. Send it for readability if you like; it has no effect, and you cannot choose which of a multi-body supplier's bodies fulfils. |

**Valid when:**
- `counterparty` names a real corporation, and is not the acting corporation — no self-contracting (rejected_invalid otherwise).
- Quantity is strictly positive (rejected_invalid otherwise).
- The supplier holds a completed building that can produce the good (rejected_no_capacity otherwise).
- The supplier's local market can supply that recipe's inputs (rejected_no_input_access otherwise).
- The supplier's embargo condition_set, if it has one, evaluates true against you (rejected_embargo otherwise).
- Your (buyer, supplier) reputation is at or above the standing floor (rejected_reputation otherwise).

**Expected output.** A procurement_quote appended to world.procurement_quotes, carrying a fresh id, the resolved body, a unit price read from the supplier's local market (live price, else base price) and a DERIVED lead time — base_lead_ticks x ceil(quantity / processing throughput), clamped to 400 ticks. Nothing is debited: a quote is an offer, not a commitment. AND HERE IS THE PROBLEM: the response tells you NEITHER the id NOR the price. `RESULT result=applied building=0` is the whole of it — `building` is an out-param request_quote never writes. So the one number a quote exists to communicate is withheld, and the id `accept_quote` requires is withheld too. There is no opcode listing outstanding quotes and no blackboard predicate for them. An agent must infer ids by counting its own successful commands against a shared counter that starts at 1, which a player did for forty commands before miscounting once and silently corrupting every id after. See BL-390.

**Reason to select.** The four decline reasons are the point of this verb, not the quote. Each is a different fact about the world that nothing else on the seam will tell you: rejected_no_capacity means this supplier cannot make the good at all, rejected_no_input_access means it could but its market cannot feed it, rejected_embargo means it has a standing policy against you specifically, and rejected_reputation means you have burned this relationship. Sweeping suppliers with a small quantity is therefore a cheap intelligence probe as well as a purchase route — it costs nothing and mutates nothing on a decline, and a player who did exactly that mapped the entire procurable market in one session (iron ore 20 sellers, agricultural produce 8, water 3, petroleum 2, and no processed good from anyone).

USE IT AS A PROBE, NOT AS A QUOTE. You cannot shop: the response carries no price and no lead time, so twenty suppliers who all say yes are indistinguishable, and the only way to learn what you agreed to is to accept and difference your cash. Until BL-390 lands, treat `applied` as 'this supplier is willing' and nothing more.

### `gameplay.resume` — The 'Resume' button on the Selection band's building layout — it renders in place of 'Idle' when the building is idled.

**Press.** Select the owned, idled building, click 'Resume'.

| Arg | Type | Meaning |
|---|---|---|
| `building` | `entity_id` | The idled building to restart. |

**Valid when:**
- The acting corporation exists (rejected_no_corp otherwise).
- The corporation owns the building (rejected_not_owner otherwise).
- The building is currently idled (rejected_state otherwise — resuming a running building is a no-op).

**Expected output.** decommissioned := false immediately; production and wages restart on the next economy tick at the building's existing workforce target and recipe. Rejected attempts change nothing.

**Reason to select.** The idled asset now reads profitable again — prices recovered or inputs cheapened — so resuming captures margin with zero capex. The AI's own dial_resume fires on exactly this signal.

### `gameplay.set_recipe` — The Method comparison section on the Selection panel's building Facts column (BL-431, `draw_production_method_section` — Compare toggle, each alternate's basket/wage/rate plus a Switch button); the same seam also drives the 'Production Methods' combo in the Building panel's Buildings-tab inline detail.

**Press.** Select the owned building, press Compare, then Switch on the desired alternate row (Selection panel); or open the production-method combo and click a recipe row (Buildings-tab detail — each row carries a resource pip for its primary output).

| Arg | Type | Meaning |
|---|---|---|
| `building` | `entity_id` | The building whose recipe changes. |
| `recipe` | `uint16 recipe id` | The recipe to run, from the building type's recipe list. In practice only processing facilities have multiple; extraction is fixed to its target resource. |

**Valid when:**
- The acting corporation exists (rejected_no_corp otherwise).
- The corporation owns the building (rejected_not_owner otherwise).
- The recipe is valid for the building's type (rejected_invalid otherwise).
- Construction is complete — the UI hides all controls while ticks_remaining > 0.
- The building produces output (infrastructure has no recipe section at all).

**Expected output.** On success, an immediate component write gated by economy.recipe_switch (BL-430): a one-off credit cost debited from the corp and a cooldown before the SAME building may switch again through this seam (both configurable, default free/instant). From the next economy tick the building consumes the new recipe's inputs and produces its outputs; its profit readout, its input demand on the local market, and what it contributes to the pool all change with it. Re-selecting the recipe already active, switching on cooldown, or switching without enough credit all reject (rejected_state / on_cooldown / insufficient_funds at the command seam) and change nothing.

**Reason to select.** The first lever on a processor's profitability, free of capex: per-recipe margins genuinely diverge with local prices (steel can lose money on a tile where food rations clear well), so switching chases the better-clearing output with the plant already paid for. Every new processor defaults to the steel recipe, so this is typically the first press after building one.

### `gameplay.set_workforce` — The workforce slider on the Selection band's building layout (0–200% of nominal); the Building panel's inline detail offers a coarser 0/20/40/60/80/100 tier button grid instead.

**Press.** Band: select the owned building, untick 'Auto' (the slider is disabled while Auto holds the dial), drag the slider. Panel: click a tier button directly — this pins even while Auto is active.

| Arg | Type | Meaning |
|---|---|---|
| `building` | `entity_id` | The building whose staffing target changes. |
| `workforce` | `int [0, 200]` | Target staffing as % of nominal. Above 100 pushes past nominal output; the panel grid only reaches 100, the slider and the command seam reach 200. |

**Valid when:**
- The acting corporation exists (rejected_no_corp otherwise).
- The corporation owns the building (rejected_not_owner otherwise).
- The value is inside [0, 200] (rejected_invalid otherwise).
- The building is not ALREADY at that target AND already pinned (rejected_state otherwise). Setting the value the auto-solver happens to be holding is NOT a no-op — it takes control away from the solver, which is a real change.

**Expected output.** workforce_target is set AND workforce_auto is cleared — a manual move pins the dial, so the per-tick profit-max auto-solver stops adjusting this building until Auto is re-enabled. This IS the workforce-auto opt-out. Actual staffing is the target mediated by body-level labour contention: when the corp's demand on the body exceeds the habitability-sized pool, every building is throttled by the same fraction and the band prints 'Body allows N%'. Wages scale with assigned workforce; output scales with effective workforce. Since BL-293 the command seam clears workforce_auto exactly as the press does — before that it set the target and left the flag, so an agent's target was silently re-solved on the next tick. Setting a value already held on an already-pinned dial is a no-op (rejected_state).

**Reason to select.** Trades wage bill against output when the solver's profit-max answer is not what is wanted — throttle to cut losses without idling, staff to 0 to park a building cheaply, or overdrive past 100% to feed a downstream chain even at a per-building loss. Also the deliberate press for taking manual control away from the auto-solver.

### `gameplay.set_workforce_auto` — The 'Auto' checkbox beside the band's workforce slider; the 'Auto (N%)' button above the panel's tier grid. Also a corp_verb.

**Press.** Select the owned building, click the Auto checkbox (band) or the Auto button (panel). Clicking while already on is a plain re-assert on the checkbox; the button simply sets it on. At the command seam, re-asserting is rejected_state rather than a silent success.

| Arg | Type | Meaning |
|---|---|---|
| `building` | `entity_id` | The building handed back to the auto-solver. Carried in the command's `subject`. |

**Valid when:**
- The acting corporation exists (rejected_no_corp otherwise).
- `subject` names a real building (rejected_invalid otherwise).
- The corporation owns the building (rejected_not_owner otherwise).
- The building is not ALREADY on auto (rejected_state otherwise — a genuine no-op).
- Construction is complete (the UI hides the controls until then; the seam does not itself gate on this).

**Expected output.** workforce_auto := true. Each economy tick the solver sets this building's workforce_target to the profit-maximising value; the panel's Auto button displays the currently solved percentage. This is the only sanctioned auto-action on the player's corporation — it moves one dial and never places, relocates, retargets, or decommissions anything. Any later manual target clears it again, from the slider, the tier grid, OR the set_workforce verb.

**Reason to select.** Removes one micromanagement dial per building. The right press whenever 'maximise this building's own profit' is exactly the intent — freshly built buildings start with it on, so this mostly exists to undo an earlier manual pin.

### `gameplay.survey` — The Survey section of the Selection band when an unsurveyed body is selected (the hero action for that selection kind; the star carries no survey).

**Press.** Single-click the body on the Solar or Circumplanetary canvas, then click 'Dispatch Survey' — the button carries a 'cost cr - ETA d' preview derived from the body's size and distance.

| Arg | Type | Meaning |
|---|---|---|
| `body` | `entity_id` | The body to survey. |

**Valid when:**
- The acting corporation exists (rejected_no_corp otherwise).
- The body exists and is not the star (rejected_invalid otherwise).
- The body's survey phase is 'hidden' — a survey already in transit, scanning, or completed is refused (rejected_state).
- The balance covers the survey cost (rejected_funds; the UI disables the button and prints 'Insufficient funds.').

**Expected output.** Enqueued on press; the app debits the FULL cost up front and arms the schedule. DURATIVE over days, not quarters: phase runs in_transit ('En route - ETA N d') then scanning region-by-region ('Surveying k/N - ETA N d') then surveyed. Each completed region reveals that region's tiles and deposit bands — the geographic fog lifts progressively, so partial results arrive before completion. Nothing is produced; the entire payoff is information. Rejected attempts change nothing.

**Reason to select.** An unsurveyed body cannot be evaluated or built on — survey is the discovery spend that opens new deposits and expansion sites. The press when local tiles are saturating or the corp needs its next profitable resource base; the AI's own survey_expand fires on discovery spend within its solvency floor.

### `gameplay.dispatch_convoy` — A corp_verb with no dedicated in-app form yet (BL-452, 2026-08-17). Issued against the corp-command seam, or over --serve, which needed no new wire key: it reuses subject / counterparty / target / quantity. The Market Ledger's Convoys tab (BL-453) shows the RESULT and carries the Hold press, but composes no dispatch.

**Press.** No in-app press yet — issue the command. Context worth carrying: before this verb the logistics layer had NO player verb at all — supply_system.cpp, logistics.cpp, four rendering paths and the only coupling between two markets' prices were entirely automatic, and none of the fifteen prior verbs named a convoy. The verb is the auto-dispatcher's own dispatch body with its shortfall scan removed (supply_system.hpp's price_convoy_leg + commit_convoy), so a convoy you dispatch costs, travels and arrives exactly as one the engine dispatched for a rival. That is asserted, not assumed: tools/verify/convoy_command.cpp R4 compares a player convoy and an auto convoy of the same shape on cost, speed and mode.

| Arg | Type | Meaning |
|---|---|---|
| `source_market` | `entity_id` | The market the cargo leaves, carried in the command's `subject` field. Only its BODY selects and prices the cargo: the pool debited is (corp, source market's body), and an intra-body haul routes from the corp's own production anchor (its lowest-id building on that body) to the destination market's centre tile — not from this market's centre. The named market is recorded on the convoy as its source endpoint, which is what the Convoys tab and the canvases draw. |
| `destination_market` | `entity_id` | Where the cargo is bound, carried in the command's `counterparty` field. This is the ONE verb where `counterparty` names a market rather than a corporation — BL-452 reuses the field as 'the other end of the transaction' rather than growing the command record for a single verb. |
| `resource` | `resource_type` | The cargo, carried in the command's `target` field, shared with build's extraction target and place_sell_order's good. |
| `quantity` | `float > 0` | Units to haul. Validated as the value that lands in the destination: finite FIRST, then strictly positive, then against what the (corp, source body) pool actually holds. A quantity above the pool is REFUSED, never clamped down to it — a caller hauls the amount it named or none at all, so a silent partial haul can never happen. |

**Valid when:**
- The acting corporation exists (rejected_no_corp otherwise).
- `subject` and `counterparty` both name real markets (rejected_invalid otherwise).
- The resource index is inside the enum's domain (rejected_invalid otherwise).
- The quantity is finite and strictly positive (rejected_invalid otherwise). NaN, +inf and -inf are all refused — an infinite double narrows to an infinite float, so the whole command is rejected rather than the value truncated, wrapped or clamped.
- The acting corp's own pool on the source market's body holds at least that quantity (rejected_state otherwise). This is also the ownership check: pools are keyed (corp, body), so there is no way to name another corp's stock.
- The lane can actually be flown (rejected_placement otherwise). Intra-body: the corp holds a building on that body to route from, the destination market has a centre tile, and an A* path between them exists. Inter-body: the corp holds a Launchpad on the source body AND at least one unit of propellant there to burn on the launch — exactly the gate the auto-dispatcher applies.
- The corp can afford the haul cost (rejected_funds otherwise). The solvency gate lives inside commit_convoy, shared with the auto-dispatcher, so the two cannot disagree about affordability.

**Expected output.** One `convoy_component` appended to `world::convoys`, carrying a stable nonzero `id` that `hold_convoy` later names (see that entry: the id is TRANSIENT). Cargo leaves the source pool AT DISPATCH, not on arrival — goods in transit are committed (SUPPLY.md § Convoy entity), which is why there is no cancel verb and never will be one. The haul cost is debited from the corp's balance in the same step and recorded on the convoy as `cost_paid`; a space-mode launch also burns one unit of propellant from the source pool. Mode is DERIVED, never named: land, or sea when the intra-body path crosses water, or space between bodies (air is never dispatched). Speed is 1/travel_ticks, and travel_ticks is the terrain-weighted, physically-scaled figure — since 2026-08-12 a long haul genuinely takes several quarters, so a dispatch commits that stock for that many ticks. NOTHING reaches the destination market at dispatch: the cargo credits the destination (corp, body) pool on arrival and reaches pricing through the ordinary auto-surplus path at the next clear. A rejected command mutates nothing at all — asserted against a full world fingerprint, not a spot check (convoy_command.cpp R1).

**Reason to select.** The only way to move goods on purpose. The auto-dispatcher fills a market's measured SHORTFALL from the cheapest source, which is a reactive rule and a poor one for anything forward-looking: pre-positioning stock before a construction project needs it, moving a good to a market that PRICES it better rather than to the one merely short of it, or supplying a body whose demand has not registered yet. Weigh it against the cost of being wrong, which is real — the stock is committed the moment the convoy leaves, cannot be recalled, and on a long lane will not land for several quarters. All `hold_convoy` can do afterwards is stop it arriving.

### `gameplay.hold_convoy` — The 'Hold' / 'Release' button on each row of the Market Ledger's Convoys tab (BL-453); also a corp_verb, issuable directly against the seam.

**Press.** Open the Market Ledger, switch to the Convoys tab; each in-flight convoy of the player's corp renders as a row — cargo and quantity, source and destination market, mode, a progress bar carrying its TICKS TO ARRIVAL, and the haul cost already paid — with a small 'Hold' button, reading 'Release' when the convoy is already held. Immediate, no confirmation. The row queues a corp_command naming the convoy's id, which the frame's end applies through apply_corp_command — the same call an agent makes.

| Arg | Type | Meaning |
|---|---|---|
| `convoy` | `uint32 convoy id` | Which convoy to stop or release, by its stable `convoy_component::id` — NOT its index in world::convoys, which an arrival re-points by erasing from the middle of the vector. THIS IS A TRANSIENT SUBJECT, the dictionary's first: unlike a sell_order id or a building entity, a convoy id is valid for only a handful of ticks. It is minted at dispatch and gone the moment the cargo lands, and ids are never reused, so a command composed against a convoy that has since arrived is refused rather than silently re-aimed at whichever convoy inherited its slot. An agent must read the live convoy list and act on it in the same tick; a convoy id carried across a long deliberation is worthless. |

**Valid when:**
- The acting corporation exists (rejected_no_corp otherwise).
- `convoy` is nonzero and names a convoy in the ACTING CORP'S OWN fleet (rejected_invalid otherwise). Another corp's convoy and a nonexistent one are deliberately indistinguishable — both answer rejected_invalid, for the same reason remove_sell_order collapses its two cases (BL-397): convoy ids are one global monotonic sequence, so a distinguishable rejection would let an id sweep map every rival's cargo in flight, precisely the intelligence the BL-068 competitor-visibility rule keeps private.
- The convoy has not already arrived (rejected_state otherwise — there is nothing left to hold).

**Expected output.** The convoy's `held` flag flips. A held convoy is SKIPPED by advance_convoys — it stops dead on its lane rather than slowing, makes no progress, does not arrive, and costs nothing further, because the haul was paid once at dispatch. Issuing the verb again releases it and it resumes from exactly the progress it stopped at, at its original speed. This is a TOGGLE, and it is deliberately NOT a cancel: the cargo left the source pool at dispatch, so a cancel would have to invent a return leg or mint the goods back at the source. Neither exists and no verb offers one. A rejected command mutates nothing.

**Reason to select.** Stop a delivery that has stopped being the right one — the destination market's price collapsed, the stock turns out to be wanted at the source, a better destination appeared — without losing the cargo. The honest limit, and the thing to weigh before dispatching rather than after: holding does NOT bring the goods back. The stock is out of the pool either way; hold only stops it arriving somewhere you no longer want it, and buys time to decide. Release when the reason passes.

### `gameplay.declare_hostile` — Corporation panel, Stance column. Confirm popup on press (demolish precedent — not literally irreversible, but not unilaterally reversible by the target either).

**Press.** Open the Corporation panel, find the rival's row (only shown once the corp is otherwise BL-068-discovered), click 'Declare Hostile', confirm 'Declare' in the popup ('Cancel' backs out).

| Arg | Type | Meaning |
|---|---|---|
| `counterparty` | `entity_id` | The corporation being declared hostile. May not be yourself. |

**Valid when:**
- counterparty names a real, distinct corporation (rejected_invalid otherwise).
- Not already hostile toward counterparty (rejected_state otherwise).

**Expected output.** A directed row is inserted into world.corp_hostile_pairs (corp -> counterparty only; the reverse direction is unaffected — hostility is unilateral by design, an ambush the target does not have to agree to). Any existing friendship row between the two is dissolved atomically, and any pending friendship offer in either direction is withdrawn. A rejected declaration mutates nothing.

**Reason to select.** The predicate everything military-adjacent waits on (BL-315). Declaring hostile does not itself trigger any engagement — it only makes one legal later — but it also dissolves friendship on contact, so it is the one press that can undo a standing relationship as a side effect.

### `gameplay.offer_friendship` — Corporation panel, Stance column.

**Press.** Open the Corporation panel, find the rival's row, click 'Offer Friendship'. Shows 'Offer sent' afterward until the target accepts, declines, or either party moves to hostile.

| Arg | Type | Meaning |
|---|---|---|
| `counterparty` | `entity_id` | The corporation being offered friendship. May not be yourself. |

**Valid when:**
- counterparty names a real, distinct corporation (rejected_invalid otherwise).
- Not already friends with counterparty (rejected_state otherwise).
- Not currently hostile toward counterparty — return_to_neutral first (rejected_state otherwise).
- No offer already pending in this direction (rejected_state otherwise; not a re-send).

**Expected output.** A pending offer (corp -> counterparty) is inserted into world.corp_friend_offers. Friendship is NOT yet in effect — offer_friendship alone changes nothing about is_hostile/are_friends until the target calls accept_friendship. A rejected offer mutates nothing.

**Reason to select.** Friendship is deliberately the one stance that cannot be imposed — it requires both a genuine offer and a genuine accept, so a friendly row is always evidence both corps chose it, unlike hostility which is unilateral.

### `gameplay.accept_friendship` — Corporation panel, Stance column. Shown only on a rival row carrying a pending offer FROM that rival TO the player.

**Press.** Open the Corporation panel; a row with an incoming offer shows 'Accept Friendship' instead of 'Offer Friendship'. Click it.

| Arg | Type | Meaning |
|---|---|---|
| `counterparty` | `entity_id` | The corporation whose offer is being accepted (the offerer). The acting corp is cmd.corp, the acceptor. |

**Valid when:**
- counterparty names a real, distinct corporation (rejected_invalid otherwise).
- A pending offer exists in the direction counterparty -> corp (rejected_state otherwise — there is nothing to accept).

**Expected output.** The pending offer (counterparty -> corp) is erased, and a symmetric row is inserted into world.corp_friend_pairs under the canonicalised (min id, max id) key — both parties now read are_friends() true toward each other. A rejected accept mutates nothing.

**Reason to select.** The only way friendship becomes real. An offer alone (offer_friendship) is not a stance change by itself; this verb is the second half of the two-party handshake the model requires.

### `gameplay.return_to_neutral` — Corporation panel, Stance column. Shown whenever the row is currently Hostile or Friend.

**Press.** Open the Corporation panel, find the rival's row, click 'Return to Neutral'.

| Arg | Type | Meaning |
|---|---|---|
| `counterparty` | `entity_id` | The corporation being returned to neutral standing. May not be yourself. |

**Valid when:**
- counterparty names a real, distinct corporation (rejected_invalid otherwise).
- At least one of: the corp's own directed hostility toward counterparty, a shared friendship row, or a pending offer in either direction currently exists (rejected_state otherwise — nothing to clear).

**Expected output.** Unilateral and asymmetric on hostility: clears ONLY the acting corp's own directed hostility toward counterparty (the reverse direction, if counterparty is separately hostile toward the corp, is untouched — that is counterparty's own row to release). Clears the shared friendship row if one exists (either party may dissolve a friendship) and any pending offer between the two in either direction. A rejected call mutates nothing.

**Reason to select.** The universal de-escalation press — the only one of the four verbs usable from every non-neutral state, and the one that lets a corp exit hostility unilaterally even though it could not enter friendship unilaterally.

### `gameplay.march_unit` — The unit card's March press in the Selection element (BL-575, unit marker + march UI, landed 2026-08-23 — BL-471 was the placeholder item name; this batch folded it in). Select a unit (its own on-canvas marker, BL-575, or the repeat-click cycle), press March. Also a corp_verb, so an agent issues it against the corp-command seam (ProjectIo --serve, COMMAND opcode) without going through the card.

**Press.** Select a unit, press 'March' — this ARMS province-picking mode (the button shows the same accent-ring 'primed' state the building card's Auto press uses; pressing March again while armed CANCELS the pick, per the standing toggle rule), then click a province on the Planetary canvas to send the unit there. A click that misses every province (open ocean, off-body) is ignored and the mode stays armed. Over the seam: COMMAND corp=<id> verb=21 subject=<unit id> province=<province id>. The command is applied through apply_corp_command, which recomputes every precondition itself — a stale destination or a unit that has since been disbanded is refused, not applied.

| Arg | Type | Meaning |
|---|---|---|
| `subject` | `entity_id` | The unit to order. Must exist in world.units and be owned by the acting corporation. NOTE the field: units use `subject`, not `tile` — `tile` is the build/place_road/hire_unit convention and this verb does not read it at all. |
| `province` | `uint32 (province::id)` | The DESTINATION PROVINCE (BL-511, 2026-08-21 — this replaced a destination tile; the verb kept its value 21 because the enum is serialised and append-only, only the field it reads changed). A province id is NOT an entity_id: it is derived from (body rank | block raster index | component index), so it lives in its own uint32 domain. The default is `no_province` (0xFFFFFFFF) and an omitted field is rejected — id 0 is a REAL province (the first block of the first body), so it cannot serve as an absent-value sentinel. |

**Valid when:**
- `subject` names a real unit (rejected_invalid otherwise).
- That unit is owned by the acting corporation (rejected_not_owner otherwise).
- `province` is a province in the world's built partition — checked with province_partition::find, which is the authoritative domain test. Any uint32 that is not a built province, including the `no_province` default, is rejected (rejected_invalid otherwise). A wire range gate proves the value FITS; it does not prove it EXISTS, and both checks run.
- That province is on the SAME BODY as the unit's current tile — intra-body path march only, BL-470's ruling 1 (rejected_invalid otherwise).
- The unit is not already in that province — marching to where you already stand is not a move; halt_unit is the stop verb (rejected_state otherwise).
- At least one member tile of the province is reachable from the unit's current tile by intra_body_path (rejected_invalid otherwise).
- Water needs no separate check: the province partition covers LAND ONLY by construction, so no province id can ever name ocean.
- The unit is NOT in a live battle (rejected_state otherwise) — BL-467. Walking away from contact is a priced withdrawal, not a free move; use withdraw_from_battle.

**Expected output.** The unit's movement_order is REPLACED (not queued behind an existing one): dest_province is the commanded province, dest is the province's lowest-id reachable member tile, path is the solved intra-body route with path[0] the tile the unit already occupies, next_index is 1 and progress is 0. The route is computed ONCE, here — never re-Dijkstra'd per tick, only on an actual block. Movement then resolves across ticks in run_unit_march, spending the unit's per-class march_points_per_class against the shared terrain traversal cost and banking the fractional remainder. The order CLEARS ITSELF the tick the unit enters the destination province — it does not walk on to `dest` once it is already inside. A rejection mutates nothing at all.

**Reason to select.** The only verb that moves a unit. Until it is issued, a hired unit is pinned to its muster tile forever. Two things an agent should weigh. First, movement is not free of the economy: a unit beyond the reach field loses supply_factor_permille each tick in the upkeep pass, which lowers its derived strength in the resolver — marching away from your road network makes an army measurably weaker, not merely further away. Second, ARRIVING NOW HAS A CONSEQUENCE (BL-467, 2026-08-21): standing in a province where a corp you are hostile to also has units opens a battle on the next tick, with no verb issued by anyone. Position is no longer free. And the reverse follows — this verb is REFUSED (rejected_state) for a unit already in contact, because leaving a fight is withdraw_from_battle, which is priced, not a march, which is not.

### `gameplay.halt_unit` — The unit card's Halt press in the Selection element (BL-575, unit marker + march UI, landed 2026-08-23). Select a unit, press Halt. Also a corp_verb, so an agent issues it against the corp-command seam (ProjectIo --serve, COMMAND opcode) without going through the card.

**Press.** Select a unit, press 'Halt'. Takes effect immediately — no province pick, no confirm popup (unlike Disband). Over the seam: COMMAND corp=<id> verb=22 subject=<unit id>. No other field is read.

| Arg | Type | Meaning |
|---|---|---|
| `subject` | `entity_id` | The unit whose standing movement order is to be cleared. |

**Valid when:**
- `subject` names a real unit (rejected_invalid otherwise).
- That unit is owned by the acting corporation (rejected_not_owner otherwise).
- That unit currently holds a live order — order.dest is not null (rejected_state otherwise). Halting an already-halted unit is a no-op and is reported as such rather than answering applied.

**Expected output.** The unit's movement_order is reset to its default: no destination, no province, no path, no banked progress. The unit stays exactly where it is, on the tile it had reached. Nothing else about the unit changes.

**Reason to select.** You gave an order you no longer want, and the alternative — waiting for arrival — costs supply every tick out of reach. Note there is no 'resume': halting discards the path, so restarting means a fresh march_unit and a fresh route solve.

### `gameplay.disband_unit` — The unit card's Disband press in the Selection element (BL-575, unit marker + march UI, landed 2026-08-23). Select a unit, press Disband, confirm in the popup. Also a corp_verb, so an agent issues it against the corp-command seam (ProjectIo --serve, COMMAND opcode) without going through the card.

**Press.** Select a unit, press 'Disband' to open a confirm popup ('No refund. This cannot be undone.'), then press 'Disband' again to erase the unit or 'Keep' to back out — the same confirm-popup shape as the building card's Dismantle press. Over the seam: COMMAND corp=<id> verb=23 subject=<unit id>. No other field is read.

| Arg | Type | Meaning |
|---|---|---|
| `subject` | `entity_id` | The unit to erase. Irreversible. |

**Valid when:**
- `subject` names a real unit (rejected_invalid otherwise) — including a unit already disbanded, which is simply gone.
- That unit is owned by the acting corporation (rejected_not_owner otherwise).

**Expected output.** The unit entity is erased from world.units outright. NO REFUND — neither the credit hire cost nor the gated resource draw comes back; manpower walks away (BL-470). Nothing else is touched.

**Reason to select.** The only way to stop paying a unit's upkeep. Since BL-454 a unit draws upkeep goods every tick and weakens when that draw goes unmet, so a force you cannot supply is a running cost with a falling return. Weigh it against the sunk hire cost, which you do not get back — and against the fact that hiring again means a completed military_base and the full gate chain a second time.

### `gameplay.withdraw_from_battle` — The BATTLE CARD in the Selection element (BL-469, landed 2026-08-21): select a live battle and its 'Break off' press sits under the withdrawal price, with a confirm popup. Also a corp_verb, so an agent issues it against the corp-command seam (ProjectIo --serve, COMMAND opcode) without going through the card.

**Press.** Click the province a fight stands in (rung 0 of the repeat-click cycle — a battle selects ahead of unit, building and province), read the phase word and the three-term withdrawal price, press 'Break off', then confirm. 'Cancel' backs out. Over the seam: COMMAND corp=<id> verb=24 province=<province id> counterparty=<opposing corp id>. Applied through apply_corp_command, which recomputes every precondition itself.

| Arg | Type | Meaning |
|---|---|---|
| `province` | `uint32 (province::id)` | The province the battle is being fought in — the ENVELOPE, since BL-467 frames a fight by province rather than by tile. Default is `no_province` (0xFFFFFFFF) and an omitted field is refused: there is no battle in a province that does not exist, so the request finds nothing and mutates nothing. |
| `counterparty` | `entity_id` | WHICH fight to leave. A corp can be in more than one battle in one province, because a third corp arriving opens its OWN battles against each existing participant rather than joining theirs. Pass null_entity to mean 'the first, in sorted order' — deterministic, but it is not a choice you made. |

**Valid when:**
- A battle exists in `province` (rejected_state otherwise).
- The acting corporation is a PARTICIPANT in it — attacker or defender (rejected_state otherwise). A third party cannot order someone else's force off the field.
- That battle is still in progress; a concluded one is erased at the end of the tick it ended (rejected_state otherwise).
- `counterparty`, when not null_entity, names the opposing corp in that battle (rejected_state otherwise).

**Expected output.** The withdrawal is RECORDED, not applied. It is honoured at the START of the next tick's battle pass, before that tick's round batch — which is what makes the window a real window rather than a same-instant escape. When honoured, the resolver's three-term cost (a flat base, plus a term per round already fought, plus a pursuit term scaled by how far behind the withdrawing side is) reduces that side's strength, the loss is distributed across its units proportional to count with the remainder by ascending unit id, and the side that stayed holds the field. A rejection mutates nothing at all.

**Reason to select.** The ONLY decision a commander has once contact is made — a battle opens because two hostile forces share a province, not because anyone asked for it, so there is no 'attack' verb to weigh against this one. Weigh it on the cost curve rather than on the odds alone: the price rises with every round already fought AND with how badly you are losing, and the rounds themselves have already cost their own attrition, so a late withdrawal compounds twice. Breaking off early from a fight you are losing is cheap; breaking off late from one you are losing badly is where armies die. At the shipped pacing (3 rounds a tick, 6 to a stalemate) a full battle spans two ticks, so there is exactly ONE real opportunity to use this. THE CARD QUOTES THE PRICE BEFORE YOU PAY IT — base, per-round and pursuit shown separately, taken from the resolver's own arithmetic — so the cost curve is readable rather than something to infer from the rules.

### `gameplay.accept_offer` — The Contracts ledger's Offers view, Accept press (BL-576). Select an offer whose escrow has cleared its fee, press Accept to open the force picker, check owned uncommitted units, press Confirm. Also a corp_verb, so an agent issues it against the corp-command seam (ProjectIo --serve, COMMAND opcode) without going through the ledger.

**Press.** Press 'Accept' on a fully-escrowed, unexpired offer to open a popup listing the player's own uncommitted units with a checkbox and strength each; check at least one, press 'Confirm' to dispatch, or 'Cancel' to back out with nothing sent. Over the seam: COMMAND corp=<id> verb=25 order=<offer id> counterparty=<client nation id> units=[<unit id>, ...].

| Arg | Type | Meaning |
|---|---|---|
| `order` | `uint32 (mercenary_offer::id)` | The offer to accept. Must name a live entry in world.mercenary_offers — consumed (erased) on success, so accepting the same id twice fails the second time. |
| `counterparty` | `entity_id` | The offer's CLIENT NATION, not a corp — this verb's counterparty check forks on verb (corp_command.cpp). Must equal the named offer's own client field. |
| `units` | `entity_id[8]` | The corp's OWN units to commit — unused slots are 0/null. At least one must be named, each must be owned by the acting corp and not already committed to another ACTIVE mercenary contract. CONTRACTS.md Q1: the player chooses the force, the contract never does. |

**Valid when:**
- `order` names a real, still-open offer (rejected_invalid otherwise).
- `counterparty` names a real nation matching that offer's client (rejected_invalid otherwise).
- `current_econ_tick < offer.deadline` — the offer has not expired (rejected_invalid otherwise).
- `offer_escrow >= fee` — the offer is FULLY ESCROWED (rejected_state otherwise); a partially-funded offer is not yet acceptable.
- Every named unit is real, owned by the acting corp, and not already committed to another active mercenary contract (rejected_invalid / rejected_not_owner / rejected_state otherwise); at least one unit must be named (rejected_invalid on an empty force).
- The acting corp exists (rejected_invalid otherwise).

**Expected output.** A new mercenary_contract is created: client and template copied from the offer, province bound from the offer's target, fee copied, the deposit (fee x deposit_fraction) paid straight from the offer's own already-filled escrow, deadline copied, the named units set as the committed force, state = active. The consumed offer is erased from world.mercenary_offers so it cannot be accepted twice. A rejection mutates nothing.

**Reason to select.** The only way to turn a want into income. Underbidding is the loop's teeth (CONTRACTS.md): a cheap contract with too little force risks losing the fight, the fee AND the standing — reading the target's garrison strength (Offers view) against the force you are about to commit is the whole skill.

### `gameplay.abandon_contract` — The Contracts ledger's Active view, Abandon press (BL-576). Select an active contract, press Abandon to see its reputation cost, confirm in the popup. Also a corp_verb, so an agent issues it against the corp-command seam (ProjectIo --serve, COMMAND opcode) without going through the ledger.

**Press.** Press 'Abandon' on an active contract to open a confirm popup showing the deposit forfeit and the reputation (Trust) cost with the client nation, then press 'Abandon' again to dispatch or 'Keep' to back out. Over the seam: COMMAND corp=<id> verb=26 order=<contract id>. No other field is read.

| Arg | Type | Meaning |
|---|---|---|
| `order` | `uint32 (mercenary_contract::id)` | The contract to walk away from. Must be one of the acting corp's OWN contracts, still in the active state. |

**Valid when:**
- `order` names a real contract (rejected_invalid otherwise).
- The acting corp is that contract's contractor (rejected_not_owner otherwise).
- The contract is still active — an already-terminal contract cannot be abandoned again (rejected_state otherwise).

**Expected output.** The contract's state becomes abandoned. Same money outcome as a failure — the deposit already paid at accept_offer is not clawed back, and the reserved remainder is simply never disbursed — but a DISTINCT, LESSER sentiment magnitude (contract_cancelled, not contract_failed) records that the contractor chose this rather than losing a fight (CONTRACTS.md Q2: 'an honest early exit costs less than a rout, but it still costs').

**Reason to select.** An early, priced way out of a contract that reading the field has shown cannot be won — cheaper than letting it run to a failed deadline, never free. The ledger shows the exact reputation number before the press commits, not after.

---

## Canvas — looking around: navigation, selection, hover, time

### `canvas.ascend_key` — Keyboard.

**Press.** Press Backspace.

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus.
- The primary rung is not already Solar (top of the ladder).

**Expected output.** The primary ascends one rung: Planetary -> Circumplanetary -> Solar. Same effect as clicking the minimap. The minimap updates accordingly. Selection, lens, and sim speed are untouched; at the Solar rung the press does nothing.

**Reason to select.** Step out for context from the keyboard — pair with Enter to ride the ladder without the mouse.

### `canvas.body_next` — Keyboard.

**Press.** Press ] (right bracket).

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus.

**Expected output.** The next body (by entity id) becomes the navigation anchor (active_body) and the current rung re-frames around it — the Circumplanetary view centres on its planet, the Planetary view draws its surface. The rung itself does not change, and neither does the selection (Active and Selection are independent states).

**Reason to select.** Cycle through the system's bodies at your current altitude — tour surfaces or local views one body at a time.

### `canvas.body_prev` — Keyboard.

**Press.** Press [ (left bracket).

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus.

**Expected output.** The previous body (by entity id) becomes active_body and the current rung re-frames around it. Rung, selection, lens, and sim speed unchanged — the mirror of canvas.body_next.

**Reason to select.** Step the body cycle backwards — retrace to the body you just left.

### `canvas.descend_key` — Keyboard.

**Press.** Press Enter.

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus (bindings are suppressed while ImGui captures the keyboard).
- The primary rung is not already Planetary (bottom of the ladder).

**Expected output.** The primary descends one rung, framing the current active_body: Solar -> Circumplanetary -> Planetary. The minimap updates to the new rung's zoom-out neighbour. Selection, lens, and sim speed are untouched. At the Planetary rung the press does nothing.

**Reason to select.** Keyboard equivalent of drilling in — reach the surface of the currently anchored body without picking it out with the mouse.

### `canvas.deselect` — The primary canvas (any rung).

**Press.** Single left-click on empty space (no entity under the cursor).

**Valid when:**
- The app is in-game.
- The pointer is over the primary canvas, not the minimap inset or an ImGui panel.
- No entity resolves under the cursor.

**Expected output.** The selection clears and the Selection band hides (it is hidden whenever nothing is selected). No view change of any kind — rung, pan, zoom, lens, and active_body all stay as they were.

**Reason to select.** Dismiss the Selection band and reclaim the bottom strip when you are done acting on the current selection.

### `canvas.help_overlay_key` — Keyboard.

**Press.** Press F1.

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus.

**Expected output.** Toggles the key-bindings cheat-sheet overlay — a table generated from the same binding table the keys route through, so it can never drift from the real bindings (it also lists the off-table F11/F12 entries). Pressing F1 while the overlay is up closes it (toggle rule: its active state is visible). The sim, view, and selection are untouched.

**Reason to select.** Look up what the keys do without leaving the game — the authoritative in-app copy of this binding table.

### `canvas.hover` — The Planetary surface canvas (tiles, buildings, market centres). The Solar and Circumplanetary rungs still use a lightweight ad-hoc tooltip (name, type, orbital radius) — the full card has not migrated there yet.

**Press.** Hold the pointer over a tile or marker without clicking.

| Arg | Type | Meaning |
|---|---|---|
| `target` | `entity` | The hovered tile, building, or market centre (resolved by the same stack-and-lens rule as clicks). |
| `dwell` | `duration` | How long the hover is held: ~0.5 s of stable hover opens the glance card; ~2.5 s total sticks it. |

**Valid when:**
- The app is in-game with the Planetary surface primary.
- The pointer rests over an entity; moving off it before ~0.5 s means no card appears.
- No hover card is already up, and construction placement mode is not active (the placement ghost owns that moment).

**Expected output.** Glance phase: after ~0.5 s a small chrome-free card appears above the cursor and tracks it like a tooltip; leaving the entity dismisses it. Stick phase: after ~2.5 s total the card freezes in place and is dismissed only when the cursor leaves the card's rect plus a 26 px pad, so long lines can be read to the end. Content is keyed on the active lens (e.g. terrain header + habitability by default; deposit richness under Resource; price signal on a market centre under Market; rival buildings show type + owner only). The card never captures the pointer, never opens the Selection band (clicking is the only opener), and changes no selection or view state.

**Reason to select.** Read a thing before committing a click — check a tile's landform and movement cost, a deposit's richness, or a price signal, and learn the glyph vocabulary at the point of looking.

### `canvas.lens_clear` — Keyboard.

**Press.** Press 0.

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus.

**Expected output.** The overlay lens clears to none: the canvas returns to its unskinned terrain render (the state the app opens in — no lens is active on load). Always-on chrome (landform relief/glyphs, civic markers, player-presence ring) is not lens-gated and stays. The minimap bar shows no active glyph. Selection, framing, and speed unchanged.

**Reason to select.** Get back to plain terrain in one press from any lens, on-bar or keyboard-only, instead of cycling to the null state.

### `canvas.lens_next` — Keyboard (the minimap's 8-glyph lens bar mirrors the result for on-bar lenses).

**Press.** Press L.

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus.

**Expected output.** The overlay lens advances to the next mode in cycle order. The keyboard cycle covers the FULL lens roster, including the off-bar lenses (Scarcity, Industry, Reach, Supply-routes) the minimap bar has no room for — cycling is the only way to reach those. The active lens re-skins the canvas per that lens's semantics (owned elsewhere) and highlights its glyph on the minimap bar when it is an on-bar lens. Selection, view framing, and sim speed are untouched.

**Reason to select.** Walk the lens family in order — including the keyboard-only lenses — to change which question the map is answering.

### `canvas.lens_prev` — Keyboard.

**Press.** Press Shift+L.

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus.

**Expected output.** The overlay lens steps backward through the same full cycle canvas.lens_next walks forward. Everything else as for lens_next: canvas re-skins per the new lens, bar glyph highlight updates for on-bar lenses, no selection/view/speed change.

**Reason to select.** Step back to the lens you just passed instead of cycling all the way around.

### `canvas.minimap_ascend` — The minimap inset canvas, bottom-right corner of the shell (the middle tier of the minimap box, between its title bar and the lens mode bar).

**Press.** Single left-click anywhere on the inset canvas.

**Valid when:**
- The app is in-game.
- The primary rung is Planetary or Circumplanetary. When Solar is primary the minimap is a non-interactive branding placeholder (game name, plain fill) — there is no rung above to ascend to.
- The pointer is over the minimap inset (input goes to the minimap, not the primary, when the mouse is over it).

**Expected output.** The zoom-out neighbour the minimap is showing is promoted to primary: Planetary -> Circumplanetary, Circumplanetary -> Solar. The minimap then shows the next rung out (or the branding placeholder once Solar is primary), and its title updates (planet name -> star name -> game name). This is a single click, not a double — the minimap has no selection semantics. Selection, lens, and sim speed are untouched.

**Reason to select.** Step out one level for context — see where the body you are looking at sits in the bigger picture, or start navigating somewhere else entirely.

### `canvas.navigate` — The primary canvas (Solar or Circumplanetary rung; the Planetary rung is the bottom of the ladder).

**Press.** Double left-click on a body.

| Arg | Type | Meaning |
|---|---|---|
| `target` | `entity` | The body to navigate to (planet or moon). Resolved the same way as a single-click select. |

**Valid when:**
- The app is in-game.
- The pointer is over the primary canvas with a body under the cursor.
- There is a rung below or a body to re-target: on the Planetary surface, tile double-clicks do not descend (bottom rung).

**Expected output.** The canvas sets active_body and primary_level inline (not via focus_on_entity), re-anchoring the ladder. Solar rung: double-click a planet and its Circumplanetary view becomes primary; double-click a moon and the parent planet's Circumplanetary view becomes primary with the moon selected. Circumplanetary rung: double-click the planet or a moon and that body's Planetary surface becomes primary. The minimap updates to show the new rung's zoom-out neighbour. Any existing selection persists (Selection is independent of Active); the sim speed and lens are untouched.

**Reason to select.** Drill down the zoom ladder toward a body's surface — the load-bearing navigation gesture for going somewhere rather than merely inspecting it.

### `canvas.options_key` — Keyboard.

**Press.** Press F10.

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus.

**Expected output.** Toggles the Options window (display/UI settings; changes apply live). Pressing F10 while it is open closes it (toggle rule). Sim, view, and selection untouched — though settings changed inside it (e.g. UI scale) can re-layout the shell.

**Reason to select.** Adjust display or UI settings mid-session without going through a menu.

### `canvas.pan_drag` — The primary canvas (all three rungs).

**Press.** Hold the middle mouse button and drag.

| Arg | Type | Meaning |
|---|---|---|
| `delta` | `vector2` | The drag displacement in screen pixels; the view translates with the cursor. |

**Valid when:**
- The app is in-game.
- The pointer is over the primary canvas, not the minimap inset or an ImGui panel.

**Expected output.** The current rung's pan offset shifts so the world moves with the cursor. Pan state is per-rung — panning the Planetary surface does not move the Solar view. No selection, zoom, rung, lens, or sim-speed change; releasing the button simply stops the pan.

**Reason to select.** Reposition the viewport to look at a different part of the current rung without changing what is selected or how far in you are.

### `canvas.pan_keys` — Keyboard.

**Press.** Press an arrow key (Left / Right / Up / Down).

| Arg | Type | Meaning |
|---|---|---|
| `direction` | `enum: left | right | up | down` | Which arrow was pressed; the view pans one step that way. |

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus.

**Expected output.** The current rung's view pans by one step in the given direction (same per-rung pan state the middle-mouse drag writes). Nothing else changes — no selection, zoom, rung, lens, or speed effect.

**Reason to select.** Nudge the viewport precisely, or pan at all when driving keyboard-only (this is the limited-access surface the verification harness also drives).

### `canvas.pause_button` — The leftmost button of the time-control row in the time panel (right chrome column, above the minimap).

**Press.** Left-click the pause button.

**Valid when:**
- The app is in-game.

**Expected output.** Identical to Space (they route through the same seam): toggles pause, remembering the running tier and restoring it on resume. The button's own face is the state readout — a filled square while running, a > play glyph while paused — so this is a toggle under the toggle rule: clicking it while paused resumes. Its tooltip names the Space binding. No view or selection change.

**Reason to select.** The mouse-side pause: stop or restart time while the cursor is already in the chrome column.

### `canvas.pause_key` — Keyboard (mirrors the on-screen pause button).

**Press.** Press Space.

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus.

**Expected output.** Toggles pause. Running -> paused: the sim clock stops (orbits freeze, no economy ticks; the rate label reads Paused) and the tier you were at is remembered. Paused -> running: resumes at the remembered tier, not a fixed default. The on-screen pause button's glyph flips (filled square while running, > play glyph while paused) — the visible active state that makes this a toggle.

**Reason to select.** Stop time to read the world or line up decisions, then resume at exactly the pace you had.

### `canvas.select` — The primary canvas (Solar, Circumplanetary, or Planetary rung — whichever fills the window).

**Press.** Single left-click on an entity: a body on the Solar/Circumplanetary rungs, a PROVINCE or marker on the Planetary surface (BL-511, 2026-08-21 — a plain click on the Planetary ground now selects the province, not the bare tile). A LIVE BATTLE OUTRANKS ALL OF THEM (BL-469, 2026-08-21): if a fight stands in the clicked province, the click selects the battle.

| Arg | Type | Meaning |
|---|---|---|
| `target` | `entity` | The entity under the cursor. Overlapping candidates resolve to one entity: the stack UNIT > building > market > PROVINCE > body is walked most-specific first (BL-575, 2026-08-23, put the unit marker ahead of the building marker — a unit standing on a built tile must be reachable on the FIRST click, matching the repeat-click cycle's own precedence below, not only after cycling past the building), the active lens filters validity, and nearest-to-cursor (entity id breaking ties) picks a single stable winner. On the Planetary rung the ground itself resolves to the province containing the hovered tile (BL-511): the tile is still the data grain and the Selection card lists the province member tiles, their terrain and their summed deposits, but it is no longer what a plain click addresses. A unit marker (BL-575) is drawn once per (province, owner) GROUP at the province's anchor tile — the group's lowest-id unit is what a click on it resolves to; the Selection card is the same unit card either way. |

**Valid when:**
- The app is in-game (not the main menu or New World wizard).
- The pointer is over the primary canvas, not over the minimap inset or any ImGui panel (panels capture the mouse).
- An entity is under the cursor (empty space is a different press — see canvas.deselect).

**Expected output.** selected_entity becomes the target and the Selection band (fixed strip at the bottom of the screen) appears or re-points, showing that kind's action and facts; a new selection resets any drill-down stack in the band. Nothing else changes: same rung, same pan, same zoom, active_body untouched, no lens change, the canvas is not re-skinned, and any open fold-out ledger stays open. A single click never navigates. PROVINCE GRAIN (BL-511, 2026-08-21): selecting Planetary ground shows the PROVINCE in the same Selection element every other kind uses (refolded 2026-08-21 on Ben's ruling that there should not be a second selection element) — header, then the shared three-column band: the province rendered over its mixture bar (the blend legend) on the left, a Tiles / Deposits / Buildings pager in the centre (member tiles with terrain, deposits summed across the province, the buildings standing in it), a Go to action grid on the right. The canvas outlines the province OUTER boundary. BATTLE GRAIN (BL-469, 2026-08-21): when a live battle stands in the clicked province the Selection element shows the BATTLE instead — the phase word, per-unit strength bars for both sides, and the withdrawal price with its base / per-round / pursuit terms separated, plus the 'Break off' press (see gameplay.withdraw_from_battle). Which battle, when several stand in one province: the acting corp's own first, then sorted (province, attacker, defender) order. REPEAT-CLICK CYCLE (Ben, 2026-08-21; widened to five rungs by BL-469): clicking the same spot again walks BATTLE > UNIT > PROVINCE > BUILDING > TILE, skipping any rung with nothing on it. The hit-test itself is unchanged and still resolves most-specific-first, so the FIRST click on a building selects the building, not its province.

**Reason to select.** Inspect a thing and surface its one primary move — Dispatch Survey on an unsurveyed body, Manage on your building, the construct/manage grid on a tile — without losing your current framing.

### `canvas.speed_buttons` — The five buttons labelled I, II, III, IV, V beside the pause button in the time panel's control row.

**Press.** Left-click a speed-tier button.

| Arg | Type | Meaning |
|---|---|---|
| `tier` | `int 1-5` | The clicked button: I = 0.25x, II = 0.5x, III = 1x, IV = 4x, V = 16x. The panel's rate label shows the multiplier and the real-time cost of one economic quarter at that tier. |

**Valid when:**
- The app is in-game.

**Expected output.** The sim runs at the clicked tier and that tier becomes the remembered resume tier; clicking a tier while paused resumes directly at it. The active tier is visibly highlighted, but the row is a single-select selector, NOT a toggle — re-clicking the active tier leaves the speed unchanged (it does not pause; the pause button/Space is the toggle). No view or selection change.

**Reason to select.** Set the sim pace by eye against the rate label — the mouse equivalent of the 1-5 keys.

### `canvas.speed_keys` — Keyboard (mirrors the on-screen speed buttons I-V).

**Press.** Press a digit 1-5.

| Arg | Type | Meaning |
|---|---|---|
| `tier` | `int 1-5` | Speed tier: 1 = I (0.25x), 2 = II (0.5x), 3 = III (1x), 4 = IV (4x), 5 = V (16x). |

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus.

**Expected output.** The sim runs at the chosen tier's multiplier, and that tier becomes the remembered resume tier. Pressing a digit while paused unpauses directly to that tier. Pressing the digit of the already-active tier changes nothing — speed is a selector, not a toggle; pause (Space) is the toggle. No view or selection change.

**Reason to select.** Set the pace to the decision density: crawl at 0.25x through a delicate moment, run at 16x through quiet quarters.

### `canvas.tech_tree_key` — Keyboard.

**Press.** Press F9.

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus.

**Expected output.** Toggles the mock tech-tree viewer — a read-only design aid rendering display data from scripts/tech_tree.lua, tabbed by era (Era -1 Antiquity placeholder / Era 0 / Era 1 / Standing lines), with NO simulation coupling (nothing can be researched from it; research is not implemented). Pressing F9 again closes it (toggle rule). Sim, view, and selection untouched.

**Reason to select.** Preview the planned research structure. Do not select this expecting to act — it is a mock, not a system.

### `canvas.zoom_keys` — Keyboard.

**Press.** Press = (or +) to zoom in, - to zoom out.

| Arg | Type | Meaning |
|---|---|---|
| `direction` | `enum: in | out` | = / + zooms in one step; - zooms out one step. |

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus.
- The current rung is not at its zoom bound in that direction.

**Expected output.** The current rung's zoom factor steps in or out, clamped to the same per-rung bounds the wheel and slider share. Unlike the wheel there is no cursor anchor to aim — the view scales in place. No selection, rung, lens, or speed change. On the Planetary surface, zoom also gates the terrain-texture pass (BL-520): the substrate grain and cover pattern draw only above 14 px of drawn hex circumradius and reach full strength at 22 px, so zooming out fades the ground texture away before BL-269's coarse-fill threshold (7 px) is reached. Nothing about the tile's identity changes with it — the terrain colour, the relief shading, the survey mask and the fog wash are colour, not geometry, and survive to the whole-grid view.

**Reason to select.** Zoom without the mouse — the keyboard leg of the pan/zoom pair for keyboard-only driving.

### `canvas.zoom_slider` — The scale-bar + zoom-slider overlay pinned to the bottom-centre of the primary canvas — present on the Solar and Circumplanetary rungs only (primary view only, never on the minimap copy; the Planetary surface has no slider).

**Press.** Click and drag the slider handle.

| Arg | Type | Meaning |
|---|---|---|
| `zoom` | `float` | The target zoom factor. The track is logarithmic: left end = most zoomed out (zoom_min), right end = most zoomed in (zoom_max). The slider carries no value text; the scale bar beside it gives the distance reading. |

**Valid when:**
- The app is in-game.
- The primary rung is Solar or Circumplanetary.

**Expected output.** The current rung's zoom factor is set directly to the dragged position, clamped to the same bounds the scroll wheel uses. This is an absolute selector, not a toggle — its position always reflects the current zoom, and wheel zooming moves it. No selection, pan, rung, lens, or speed change.

**Reason to select.** Jump to a known framing in one gesture — e.g. all the way out to see the whole system — instead of scrolling notch by notch.

### `canvas.zoom_wheel` — The primary canvas (all three rungs).

**Press.** Scroll the mouse wheel (up = zoom in, down = zoom out).

| Arg | Type | Meaning |
|---|---|---|
| `direction` | `enum: in | out` | Wheel-up zooms in, wheel-down zooms out, one increment per notch. |

**Valid when:**
- The app is in-game.
- The pointer is over the primary canvas, not the minimap inset or an ImGui panel.
- The current rung is not already at its zoom bound in that direction (zoom is clamped per rung).

**Expected output.** The current rung's zoom factor changes, anchored at the cursor: the point under the pointer stays put while the view scales around it. Per-rung state; the zoom slider (where present) moves to reflect the new factor. No selection, rung, pan-recentre, lens, or speed change. On the Planetary surface, zoom also gates the terrain-texture pass (BL-520): the substrate grain and cover pattern draw only above 14 px of drawn hex circumradius and reach full strength at 22 px, so zooming out fades the ground texture away before BL-269's coarse-fill threshold (7 px) is reached. Nothing about the tile's identity changes with it — the terrain colour, the relief shading, the survey mask and the fog wash are colour, not geometry, and survive to the whole-grid view.

**Reason to select.** Move closer to or further from a specific spot — cursor anchoring means you aim the zoom at the thing you are interested in.

---

## Lenses — re-skinning the map to answer a question

### `lens.clear` — Minimap lens bar — the glyph of whichever lens is currently active (shown highlighted)

**Press.** Single left-click on the currently-active (highlighted) lens glyph. This is the family's one toggle behaviour, stated here once: the bar is single-select with a null state, so re-selecting the active lens clears to no-lens (overlay_mode::none). Each bar-lens entry references this. Off-bar lenses have no glyph to re-click; they are cleared via the keyboard cycle or the clear hotkey (controls family).

**Valid when:**
- A bar lens must currently be active — with no lens active there is no highlighted glyph and this press does not exist.
- Selecting a different lens is an ordinary switch, not a clear; only re-clicking the active one clears.

**Expected output.** The canvas returns to plain terrain (overlay_mode::none) — the state the campaign opens in. All lens tints, marks, and keys disappear. Always-on chrome survives, now at FULL strength rather than the 0.45 a lens attenuates it to (BL-520): the substrate grain and cover pattern that texture the ground, the player-identity tile wash and outline, the player's home ring and HQ star, selection outlines, and building/unit markers are not lens-dependent. Pointer clicks revert to resolving the lowest drawn entity (marker, else tile), routing to the Tile Ledger.

**Reason to select.** Return to the unskinned terrain read — when the current lens's tint is obscuring terrain, markers, or colours you need, or when a plain click should select the thing under the pointer rather than the lens's unit of meaning.

### `lens.continent` — Minimap lens bar, slot 8 (the two-interlocking-plates-split-by-a-diagonal-seam glyph)

**Press.** Single left-click on the Continent glyph in the lens bar.

**Valid when:**
- Always pressable; Planetary-only.
- Needs the active body's generation-report plate record (matched by body name); a body with no record gets an honest 'no plate record' key instead of a tint.
- Re-clicking while active clears the lens (see lens.clear).

**Expected output.** Each tectonic plate tints a categorical colour from a dedicated ten-colour earthy table at 0.80 opacity (deliberately not the nation wheel — plates are substrate, not identity). Boundary tiles — any neighbour on another plate — get a separate white lift at 0.45, so plate seams read pale. The key explains that pale tiles are boundaries and reports the plate count; a stagnant-lid body says 'one immobile plate' rather than drawing a meaningless single tint. Terrain texture (BL-520) survives this lens at 0.45 strength, with each mark's ink derived from the tile's own lens-tinted fill — so it reads as shading on the lens colour, never as a second, competing colour.

**Reason to select.** Why is the land shaped like that? Shows the plates that drifted the terrain into place, and above all where they meet — the seams the mountain ranges, rifts, and boundary-formed deposits came from. Honestly informational/orientational: it explains the map rather than driving an economic decision.

### `lens.corporation` — Minimap lens bar, slot 1 (the seal-square glyph: filled square with centred inner dot)

**Press.** Single left-click on the Corporation glyph in the lens bar under the minimap inset.

**Valid when:**
- The lens bar is always present on the minimap chrome, so the press is always available.
- The lens only re-skins the Planetary canvas; selecting it while on Solar or Circumplanetary changes nothing visible until the player descends to a body surface.
- If Corporation is already the active lens, this same press clears it instead (see lens.clear).

**Expected output.** On the Planetary canvas, every tile holding a corporate building tints to its owning corporation's identity colour (the literal building tile only — no influence radius). The player's tiles additionally get a thin white border. Each rival corporation's HQ-projected reach ring and HQ star draw on that corp's home body in its identity colour. Tiles with no corporate building keep their plain terrain colour — there is no nation underlay. Pointer clicks are NOT lens-dependent: selection resolves the same way under every lens — marker hit-test (building outranks market centre), else the tile under the pointer, with a built tile resolving to its building. The lens changes what is drawn, never what a click selects. No on-canvas colour key yet (glyph highlight + tooltip only). Terrain texture (BL-520) survives this lens at 0.45 strength, with each mark's ink derived from the tile's own lens-tinted fill — so it reads as shading on the lens colour, never as a second, competing colour.

**Reason to select.** Who owns what: where do rival corporations operate, how does my footprint sit against theirs, and where do their HQ reach rings suggest they will grow? Use it to find uncontested ground to expand into or to size up a rival's holdings before competing.

### `lens.country` — Minimap lens bar, slot 2 (the downward-pointing shield glyph)

**Press.** Single left-click on the Country glyph in the lens bar.

**Valid when:**
- Always pressable from the lens bar; Planetary-only surface — no Solar or Circumplanetary representation.
- Re-clicking while active clears the lens (see lens.clear).

**Expected output.** Claimed tiles tint to their owning nation's identity colour; a dark border stroke draws on every hex edge between different owners (including claimed/unclaimed boundaries), so territories read as filled regions with hard outlines. Unclaimed tiles keep their plain terrain hue. An on-canvas per-nation key (one colour swatch + name per nation on the active body) folds out flush-left of the minimap. Pointer clicks are NOT lens-dependent: selection resolves the same way under every lens — marker hit-test (building outranks market centre), else the tile under the pointer, with a built tile resolving to its building. The lens changes what is drawn, never what a click selects. Terrain texture (BL-520) survives this lens at 0.45 strength, with each mark's ink derived from the tile's own lens-tinted fill — so it reads as shading on the lens colour, never as a second, competing colour.

**Reason to select.** Which nation holds which tile, and where the borders fall. Political context for siting: whose territory would I be building in, which nations border my operations, and how the body's political map is carved up.

### `lens.good_selector` — The on-canvas lens legend — the key box that folds out flush-left of the minimap while the Resource, Market, or Scarcity lens is active. One shared combo (bound to a single shared lens_resource value), not three separate controls.

**Press.** Open the combo in the lens legend and pick a good from the list.

| Arg | Type | Meaning |
|---|---|---|
| `good` | `resource name` | The good the active lens interrogates: whose deposits fill (Resource), which price line highlights in the Circumplanetary strip (Market), whose shortfall tints the catchments (Scarcity). |

**Valid when:**
- One of the Resource, Market, or Scarcity lenses must be active — the combo only exists inside those lenses' legends.
- This is a cross-cutting selector, exempt from the toggle rule: it switches a target rather than expressing an active state, so re-picking the current good is a no-op, not a clear.

**Expected output.** The active lens's surface re-skins immediately for the newly selected good — new deposit fill (Resource), new highlighted price row (Market), or new shortfall blocks (Scarcity) — and the legend swatch/name update. The lens itself stays active. Because the value is shared, switching lenses afterwards carries the same good across all three. Terrain texture (BL-520) survives this lens at 0.45 strength, with each mark's ink derived from the tile's own lens-tinted fill — so it reads as shading on the lens colour, never as a second, competing colour.

**Reason to select.** Change the question's subject without changing the question: compare goods on the same surface — where is copper versus iron, which good is this market pricing high, what is each market short of — by flipping the good while the lens holds.

### `lens.industry` — Off the lens bar (trimmed in BL-093 the day it shipped); the factory-silhouette glyph exists but is not on the strip

**Press.** No bar press — reachable only via the keyboard lens-cycle (controls family owns the hotkeys).

**Valid when:**
- Only reachable by keyboard cycle.
- Planetary-only. The field is read from the economy report, so at least one economy tick must have run before the tint has anything to show.
- Cleared by cycling off it or the clear hotkey, not by a bar re-click.

**Expected output.** A sequential dark-to-amber tint over the tiles carrying buildings owned by BACKGROUND corporations (corporation_component.is_background). Per tile the value is the sum over those buildings of (0.5 + 0.5 x output share), where output share is that building's output this tick normalised to the largest background output on the body — so an idle or under-construction background plant still reads, a high-output one reads brightest, and two buildings on one tile stack. Normalised to the body maximum. Tiles with no background building keep plain terrain. Low-to-high amber gradient key titled 'Background industry'. Pure rendering — it changes nothing in the market arithmetic. Pointer clicks fall through to the tile (Tile Ledger); there is no dedicated ledger route. Terrain texture (BL-520) survives this lens at 0.45 strength, with each mark's ink derived from the tile's own lens-tinted fill — so it reads as shading on the lens colour, never as a second, competing colour.

**Reason to select.** Select to answer 'where is the industry I did not build?' — the distribution of rival background firms' plant across this body, before choosing where to place your own or which market to lean on. Re-pointed by BL-373 (2026-08-12) at real background-corporation buildings; it no longer tints the vestigial tile.substrate_density generation ripple, so the tint is now a reading of where background industry IS rather than where it plausibly would be. Distinct from Production, which ranks output intensity INCLUDING your own holdings — use Production to see how hard the body is running, this to see whose plant is already there. It does NOT drive market supply or demand; background firms move the market through their ordinary buildings, which this lens merely locates.

### `lens.market` — Minimap lens bar, slot 4 (the three-ascending-vertical-bars glyph)

**Press.** Single left-click on the Market glyph in the lens bar.

| Arg | Type | Meaning |
|---|---|---|
| `good` | `resource name (optional)` | The good highlighted in the Circumplanetary price strip; set via the shared good selector (lens.good_selector). The Planetary catchment tint itself is per-market, not per-good. |

**Valid when:**
- Always pressable. Surfaces exist on Planetary and Circumplanetary rungs; Solar shows nothing (prices are per-body-market).
- Prices must have resolved (the economy has ticked) for the strip to show meaningful numbers.
- Re-clicking while active clears the lens (see lens.clear).

**Expected output.** Planetary: a catchment tint — one colour per market, so market boundaries read as colour boundaries — with a city-name swatch key and the shared good selector in the legend. Circumplanetary: a compact per-body price strip (good → price list, the selected good highlighted). Terrain shows through the tint. Pointer clicks are NOT lens-dependent: selection resolves the same way under every lens — marker hit-test (building outranks market centre), else the tile under the pointer, with a built tile resolving to its building. The lens changes what is drawn, never what a click selects. Terrain texture (BL-520) survives this lens at 0.45 strength, with each mark's ink derived from the tile's own lens-tinted fill — so it reads as shading on the lens colour, never as a second, competing colour.

**Reason to select.** Which market does a tile clear against, and where do market boundaries fall? Decide which catchment to build in (your output sells to the nearest centre) and read per-body prices on the Circumplanetary rung to pick where a good is dear enough to sell.

### `lens.opportunity` — Minimap lens bar, slot 6 (the open circle with inner plus glyph)

**Press.** Single left-click on the Opportunity glyph in the lens bar.

**Valid when:**
- Always pressable; Planetary-only.
- The economy must have ticked so market prices have diverged from base — before that the surface reads flat/neutral.
- Re-clicking while active clears the lens (see lens.clear).

**Expected output.** Per-tile red-to-green value marks (same mark idiom as Population), keyed to each tile's catchment market's demand-gap rank — a body-relative ranking on gap × volume — unmet demand quantity weighted by traded volume, not a price-above-base measure. All tiles of one catchment read uniformly (the market is the unit), so the surface shows blocks per catchment. Tiles with no catchment market keep plain terrain. Key is the red-to-green rank bar. Terrain texture (BL-520) survives this lens at 0.45 strength, with each mark's ink derived from the tile's own lens-tinted fill — so it reads as shading on the lens colour, never as a second, competing colour.

**Reason to select.** Where is demand going unmet, so the market will pay a premium to whoever supplies it? The forward-looking siting lens: green catchments are markets bidding above base — build or route supply there. Reads potential; the Production lens reads what is realised.

### `lens.population` — Minimap lens bar, slot 5 (the small figure glyph: round head over tapered torso)

**Press.** Single left-click on the Population glyph in the lens bar.

**Valid when:**
- Always pressable; Planetary-only. No selector — a whole-body surface with no per-good pick.
- Re-clicking while active clears the lens (see lens.clear).

**Expected output.** Every buildable tile gets a small per-tile red-to-green value mark keyed by workforce efficiency — the same habitability-to-labour curve the economy applies (full 1.0x efficiency at habitability >= 0.6, ramping down to 0.5x at habitability 0). Tiles keep their terrain hue; the marks carry the signal. A gradient key labelled 'Workforce efficiency' labels the bar's ends 'low' and 'high' (the underlying curve spans 0.5x to 1.0x, but the key does not print the numbers). Note: this is the labour consequence, not raw habitability, and not population density (that lives on the population-centre markers). Terrain texture (BL-520) survives this lens at 0.45 strength, with each mark's ink derived from the tile's own lens-tinted fill — so it reads as shading on the lens colour, never as a second, competing colour.

**Reason to select.** Where does labour run at full efficiency? Site buildings where the marks read green (habitability >= 0.6 = full workforce), because the same wages buy less output on the red end. The siting complement to Resource's material read.

### `lens.production` — Minimap lens bar, slot 7 (the filled upward triangle over a baseline glyph)

**Press.** Single left-click on the Production glyph in the lens bar.

**Valid when:**
- Always pressable; Planetary today (a Circumplanetary per-body output badge is specified but owed).
- The economy must have ticked so buildings have produced and the economy report is populated — otherwise everything reads cold.
- Re-clicking while active clears the lens (see lens.clear).

**Expected output.** Each producing tile tints on the production ramp (production_colour: red → yellow → green) by its output value this tick (sum of output quantity x resolved price) relative to the body's producing-tile geometric mean, composited at 0.6 over terrain. Above-mean producers read green, below-mean red; idle, exhausted, or unbuilt tiles produce nothing and stay untinted. Honest caveat: a body of similar producers reads near-neutral — little spread to show. Low-to-high key. Pointer clicks are NOT lens-dependent: selection resolves the same way under every lens — marker hit-test (building outranks market centre), else the tile under the pointer, with a built tile resolving to its building. The lens changes what is drawn, never what a click selects. Terrain texture (BL-520) survives this lens at 0.45 strength, with each mark's ink derived from the tile's own lens-tinted fill — so it reads as shading on the lens colour, never as a second, competing colour.

**Reason to select.** Where is value actually being made right now? The realised-output counterpart to Opportunity's potential: spot the hot producers worth expanding and the cold ones worth investigating or idling.

### `lens.reach` — Off the lens bar; currently reuses the convoy glyph (a dedicated glyph is an open TODO)

**Press.** No bar press — reachable only via the keyboard lens-cycle (controls family owns the hotkeys).

**Valid when:**
- Only reachable by keyboard cycle.
- Planetary key today; the specified Solar connected-body glow is owed.
- Shows the player's own trade routes only (competitor-visibility rule — rival lanes stay private).
- Cleared by cycling off it or the clear hotkey.

**Expected output.** No tile re-skin. A connection-list key headed 'Reach (your trade network)' folds out flush-left of the minimap: one row per body the active body is routed to, name plus a recency dot — fresh routes green, gone-cold routes grey (the activity-fog colour convention). An unrouted body honestly reads 'no routes from this body'. Terrain texture (BL-520) survives this lens at 0.45 strength, with each mark's ink derived from the tile's own lens-tinted fill — so it reads as shading on the lens colour, never as a second, competing colour.

**Reason to select.** Which bodies does my commercial network actually touch from here, and which links have gone cold? The health check on your persistent trade network — a greying link is a market going stale in your activity fog.

### `lens.resource` — Minimap lens bar, slot 3 (the three-stacked-strata glyph)

**Press.** Single left-click on the Resource glyph in the lens bar.

| Arg | Type | Meaning |
|---|---|---|
| `resource` | `resource name (optional)` | The good whose deposits the lens fills. Set via the shared good selector in the on-canvas legend (lens.good_selector), not on this press; the lens opens showing the currently-set shared lens_resource. |

**Valid when:**
- Always pressable; Planetary-only. No simulation dependency — deposit data exists from tile generation, so it works from turn one.
- Re-clicking while active clears the lens (see lens.clear).

**Expected output.** Every tile carrying any deposit of the selected good (deposit > 0) fills flat and uniform with that resource's identity colour at fixed 0.8 opacity — the shape of the contiguous deposit, not a magnitude gradient. Tiles without the good keep their terrain hue. The on-canvas key (flush-left of the minimap) shows the selected resource's swatch + name, the note 'filled = deposit present', and hosts the shared good selector. Deposit magnitude lives in tile detail, not this surface. Pointer clicks are NOT lens-dependent: selection resolves the same way under every lens — marker hit-test (building outranks market centre), else the tile under the pointer, with a built tile resolving to its building. The lens changes what is drawn, never what a click selects. Terrain texture (BL-520) survives this lens at 0.45 strength, with each mark's ink derived from the tile's own lens-tinted fill — so it reads as shading on the lens colour, never as a second, competing colour.

**Reason to select.** Where can a chosen good be extracted? The pre-economy siting lens: find the deposit blobs of iron, copper, etc. before placing extraction. Answers 'where is the iron' by shape; how rich each tile is comes from clicking it.

### `lens.scarcity` — Off the lens bar (dropped in the BL-093 trim to 7-then-8 glyphs); the hollow downward-triangle glyph exists but is not on the strip

**Press.** No bar press — reachable only via the keyboard lens-cycle (L / Shift+L; those hotkeys are catalogued in the controls family, not here).

| Arg | Type | Meaning |
|---|---|---|
| `resource` | `resource name (optional)` | The good whose shortfall is shown — scarcity of what? Set via the shared good selector (lens.good_selector), which appears in the on-canvas legend once the lens is active. |

**Valid when:**
- Only reachable by keyboard cycle — there is no glyph to click.
- Planetary-only. The economy must have ticked so market supply/demand arrays are populated.
- Cleared by cycling off it or pressing the clear hotkey (controls family), not by a bar re-click.

**Expected output.** A market-level field, not per-tile: every tile in a market's catchment reads as one solid block, composited toward a hot red hue at opacity proportional to that market's supply shortfall of the selected good (max(0, demand - supply), normalised to the body's worst market). A met market keeps plain terrain; a short one reads hot. With one market per body the whole body is a single block — honest to the market structure. Abundant-to-scarce key plus the selected resource's swatch and the shared selector. Pointer clicks are NOT lens-dependent: selection resolves the same way under every lens — marker hit-test (building outranks market centre), else the tile under the pointer, with a built tile resolving to its building. The lens changes what is drawn, never what a click selects. Terrain texture (BL-520) survives this lens at 0.45 strength, with each mark's ink derived from the tile's own lens-tinted fill — so it reads as shading on the lens colour, never as a second, competing colour.

**Reason to select.** Where did demand outrun supply for a chosen good last tick? The inverse of the Resource lens — gaps, not concentrations. Pick a good you can produce and find the hot markets: that is where to sell into or build supply for.

### `lens.supply` — Off the lens bar; the two-parallel-horizontal-lines convoy glyph exists but is not on the strip

**Press.** No bar press — reachable only via the keyboard lens-cycle (controls family owns the hotkeys).

**Valid when:**
- Only reachable by keyboard cycle.
- The one genuinely multi-rung lens: surfaces on all three canvases.
- Shows player convoys only; nothing renders if no player convoy is in transit.
- Cleared by cycling off it or the clear hotkey.

**Expected output.** Solar: a route line per player convoy currently in transit between bodies. Circumplanetary: a convoy-count badge beside each body's label. Planetary: a convoy glyph on the active body's tiles while a player convoy touches them. Lines and badges use a single neutral logistics hue — flow, not ownership. Tiles are not re-tinted; supply annotates, it does not re-skin terrain. The throughput scale-key is still owed. Terrain texture (BL-520) survives this lens at 0.45 strength, with each mark's ink derived from the tile's own lens-tinted fill — so it reads as shading on the lens colour, never as a second, competing colour.

**Reason to select.** Where are my goods moving right now? Verify dispatched convoys are actually in flight and see the live shape of your logistics — the in-motion read; the standing lanes they carve belong to the Supply-routes lens.

### `lens.supply_routes` — Off the lens bar; reuses the supply glyph

**Press.** Cycle lenses with L (forward) / Shift+L (backward) until Supply-routes is active — it is the last mode in the cycle. No lens-bar slot.

**Valid when:**
- In-game on a canvas (the lens-cycle keys are live).
- Off the bar: reachable only via the keyboard lens cycle. (A 2026-07-31 doc note claimed the cycle could not reach this lens; that was stale — canvas_command.cpp anchors overlay_mode_count to supply_routes+1 with a static_assert, so the cycle covers all 14 modes.)
- Planetary key only (the specified Solar aggregated-graph render is owed); player routes only.

**Expected output.** No tile re-skin. A lane-list key: one row per standing trade lane touching the active body (one entry per body pair), with a log-scaled thickness bar from that lane's cumulative convoy count (a single completion reads as a thin sliver; heavy repeat traffic saturates rather than growing linearly) and the same recency-tier colouring as Reach. Terrain texture (BL-520) survives this lens at 0.45 strength, with each mark's ink derived from the tile's own lens-tinted fill — so it reads as shading on the lens colour, never as a second, competing colour.

**Reason to select.** Which standing lanes carry my economy, and how heavily? The aggregate counterpart to Supply's in-flight convoys: the carved trade lanes and their traffic weight, for judging which routes are load-bearing and which are vestigial.

---

## Ledgers & panels — opening and steering the information surfaces

### `ledger.budget_tax_tier` — Balance Ledger (Budget), 'Taxes' tier control

**Press.** Click '-' / '+' to step, or a Roman numeral I-V to set, the tax tier

| Arg | Type | Meaning |
|---|---|---|
| `tier` | `int 1-5` | Target tax tier; arrows step by one, numerals jump directly |

**Valid when:**
- Balance Ledger is open

**Expected output.** The displayed tier changes and the active numeral highlights green. HONESTLY: this is a stub - it has NO economic effect yet (tax mechanics are owed to BL-155, the tooltip says so). Nothing else changes.

**Reason to select.** Will eventually set fiscal policy; today it only answers 'what tier is dialled in' - selecting it changes no outcome.

### `ledger.budget_wage_tier` — Balance Ledger (Budget), 'Wages' tier control

**Press.** Click '-' / '+' to step, or a Roman numeral I-V to set, the wage tier

| Arg | Type | Meaning |
|---|---|---|
| `tier` | `int 1-5` | Target wage tier |

**Valid when:**
- Balance Ledger is open

**Expected output.** The displayed tier changes and highlights. HONESTLY: a stub with NO economic effect yet (BL-155); distinct from the per-building workforce target, which is a real, separate control owned elsewhere.

**Reason to select.** Will eventually set corporate wage policy; today it is display-only.

### `ledger.build_ledger_close` — Tile construction ledger (the 'Construct · [x, y]' fold-out, BL-162), header

**Press.** Click the 'x' button at the right of the header

**Valid when:**
- Tile construction ledger is open (opened from the tile Selection band's Construct button)

**Expected output.** Closes the ledger, returning attention to the tile Selection band. The ledger also closes itself automatically if the selection stops being a tile.

**Reason to select.** Done browsing build candidates (or deciding not to build) - exits without building anything.

### `ledger.chat_channel_tab` — Comms dock (bottom-left), channel button row

**Press.** Click a channel's name button (e.g. 'Public' or a created group)

| Arg | Type | Meaning |
|---|---|---|
| `channel` | `string` | Which chat channel to make active |

**Valid when:**
- In-game (the comms dock is permanent chrome)

**Expected output.** Switches the dock's log and input to that channel; the active channel's button renders pressed. A plain selector - clicking the active channel again does not close the dock (the dock has no open/closed state).

**Reason to select.** Chooses which conversation to read or post into - Public carries nation-voiced public communications (BL-212); groups are player-scoped. It does NOT carry rival corp reflex events — those were removed as a competitor-visibility violation.

### `ledger.chat_message_input` — Comms dock, message input line at the bottom

**Press.** Type a message and press Enter

| Arg | Type | Meaning |
|---|---|---|
| `text` | `string` | The message to post |

**Valid when:**
- In-game
- Message is non-empty

**Expected output.** Posts the message into the active channel's log under the player's name. HONESTLY: no mechanical effect yet - it is the hook the future AI conversation route consumes; today nothing answers.

**Reason to select.** Speaks into the world's comms - currently expressive only, later the way to address rival corporations.

### `ledger.chat_new_group` — Comms dock, '+' button and the NEW GROUP popup it opens

**Press.** Click '+', type a group name, tick member corporations, click 'Create'

| Arg | Type | Meaning |
|---|---|---|
| `name` | `string` | The new group channel's name |
| `members` | `list of corporations` | Which corporations to include (the player is an implicit member) |

**Valid when:**
- In-game
- Create enables only once a name is typed and at least one member is ticked

**Expected output.** Creates a new chat channel with those members and adds its button to the channel row. Clicking away dismisses the popup without creating. Chat-only structure - no economic or diplomatic effect.

**Reason to select.** Sets up a scoped conversation with specific rivals - the substrate the future AI communication route (diplomacy-as-communication) will read.

### `ledger.construction_view_tab` — Construction panel, view tab strip

**Press.** Click the 'Construction' or 'Buildings' tab button

| Arg | Type | Meaning |
|---|---|---|
| `view` | `enum` | 'Construction' (in-progress builds) or 'Buildings' (owned buildings; the default) |

**Valid when:**
- Construction panel is open

**Expected output.** Switches the view. Re-clicking the currently-active tab closes the whole Construction panel (toggle rule); switching tabs is an ordinary view change. Buildings lists owned buildings with their management surface; Construction lists what is currently being built.

**Reason to select.** Buildings answers 'what do I operate and how is each doing?'; Construction answers 'what is on the way and when does it land?'

### `ledger.corp_card_expand` — Corporation dashboard, one of the four roll-up cards (Production / Trade / Workforce / Finance)

**Press.** Click the chevron on a card's title row

| Arg | Type | Meaning |
|---|---|---|
| `card` | `enum` | Which roll-up: Production, Trade, Workforce, or Finance |

**Valid when:**
- Corporation dashboard is open

**Expected output.** Opens that card as a full-screen overlay (the drill-through idiom, BL-214) - the card's full item list and charts at screen size. Only one thing can be expanded at a time; expanding a second card folds the first. Re-clicking the chevron (or pressing Esc) folds the overlay back to the one-line verdict.

**Reason to select.** The folded card is a verdict line; expanding answers 'what is BEHIND this verdict?' - the per-item breakdown that names which building, lane, or account moves the number.

### `ledger.corp_drill_back` — Corporation dashboard, drill breadcrumb ('Corporation > Card > Subject')

**Press.** Click the card-name link in the breadcrumb

**Valid when:**
- A roll-up drill is open (a subject is being viewed)

**Expected output.** Pops the drill: returns the overlay to the card's full item list. (Esc also unwinds one drill level before folding the overlay itself.)

**Reason to select.** Returns to the comparison list to check a different subject against the one just read.

### `ledger.corp_rollup_drill` — Corporation dashboard, expanded roll-up overlay (Production, Trade, or Workforce card)

**Press.** Click one of the labelled magnitude-bar rows in the expanded card

| Arg | Type | Meaning |
|---|---|---|
| `row` | `index` | Which item (a building, trade lane, or workforce site) to drill into |

**Valid when:**
- A roll-up card is expanded
- The card has drillable items (Production, Trade, Workforce do; Finance has no item rows)

**Expected output.** Drills to that subject's detail view inside the overlay, replacing the list; a 'Corporation > Card > Subject' breadcrumb appears at the top. Each row shows a 'Click to drill in' tooltip on hover.

**Reason to select.** Moves from 'production is down' to 'THIS building is the one dragging it' - the subject-level answer a decision needs.

### `ledger.corps_table_row_select` — All-corporations balance table (nav slot 8)

**Press.** Click a corporation's row

| Arg | Type | Meaning |
|---|---|---|
| `corporation` | `entity` | Which corporation to select |

**Valid when:**
- Corporations table is open

**Expected output.** Sets that corporation as the current selection: the Selection band at the bottom fills with its detail (name, home nation; corporations are non-spatial, so its go-to routes to a ledger). The table stays open - selection does not close ledgers.

**Reason to select.** The table gives one number per rival; selecting a row is how to ask 'tell me more about this one'.

### `ledger.nav_generation` — Nav rail, slot 10 (plate/continent glyph)

**Press.** Click the plate glyph at the bottom of the left icon rail

**Valid when:**
- In-game

**Expected output.** Toggles the Generation Ledger open in the fold-out column; re-click closes; opening closes any other ledger. Open, it explains why the surface generated as it did, split into Body (histograms, thresholds, profile echo) and Tile (the per-tile derivation breadcrumb) views. Opening or switching body REGENERATES that body's per-pass record from the recorded tile-pass inputs - a deterministic replay costing one tile pass, cached while the body stays the subject. Nothing it shows is stored on the world or in the save.

**Reason to select.** A DEVELOPER TUNING surface, not a play read: it answers why a tile or a whole body came out as it did. An AI player has no strategic use for it - the deposits and terrain it would act on are already on the tile.

### `ledger.generation_view_tab` — Generation Ledger, view tab strip

**Press.** Click the 'Body' or 'Tile' tab button

| Arg | Type | Meaning |
|---|---|---|
| `view` | `enum` | 'Body' (composition/landform histograms, ocean threshold, latitude bands, profile echo) or 'Tile' (the five-pass derivation breadcrumb for the selected tile) |

**Valid when:**
- Generation Ledger is open

**Expected output.** Switches the view. Re-clicking the currently-active tab closes the whole Generation Ledger (toggle rule); switching tabs is an ordinary view change. The Tile view reads the shared selection: with no tile selected, or a tile on another body, it says so rather than showing a stale breadcrumb.

**Reason to select.** Body answers 'what shape did this generation come out, and which input made it that shape?'; Tile answers 'why is THIS tile what it is?'

### `ledger.generation_body_selector` — Generation Ledger, 'Body' combo

**Press.** Open the Body combo and pick a body

| Arg | Type | Meaning |
|---|---|---|
| `body` | `entity` | Which body to explain |

**Valid when:**
- Generation Ledger is open

**Expected output.** Repoints both views at the chosen body and regenerates its per-pass record (one deterministic tile pass, then cached). Defaults to the canvas's active body. Cross-cutting selector - exempt from the toggle rule.

**Reason to select.** Chooses which body's generation to inspect.

### `ledger.history_body_selector` — History ledger, 'Body' combo (Story view only)

**Press.** Open the Body combo and pick a body

| Arg | Type | Meaning |
|---|---|---|
| `body` | `entity` | Which body's story to show |

**Valid when:**
- History ledger is open
- Active view is Story (Chain has no per-body selector)

**Expected output.** Repoints the Story prose at the chosen body. Cross-cutting selector - exempt from the toggle rule.

**Reason to select.** Chooses which body's generation record to read - e.g. checking a prospective mining body's deposit story before committing.

### `ledger.history_chain_round_tab` — History ledger, Chain view's round strip

**Press.** Click the 'System', 'Life', or 'Legacy' button

| Arg | Type | Meaning |
|---|---|---|
| `round` | `enum` | Which generation round to chart: System (physical), Life (biosphere), or Legacy (aftermath) |

**Valid when:**
- History ledger is open
- Chain view is active

**Expected output.** Switches the Chain charts to that round's question (a caption states it). This strip is a plain selector - it does NOT close the ledger on re-click (no close flag is wired), unlike the top-level view tabs.

**Reason to select.** Each round answers a different comparison - which bodies are physically viable, which grew life, what remains - stepping through them builds the whole system picture.

### `ledger.history_view_expand` — History ledger, chevron on the tab row (Story view only)

**Press.** Click the fold chevron at the right end of the tab row

**Valid when:**
- History ledger is open
- Active view is Story (Chain carries no view-level chevron; its stages carry their own)

**Expected output.** Opens the Story view as a full-screen overlay, giving the biography prose the width the 380 px column breaks every line of. Re-clicking the chevron (or Esc) folds back to the in-column view.

**Reason to select.** A long body biography is unreadable as wrapped fragments in the column; expanding is how to read it as continuous prose.

### `ledger.history_view_tab` — History ledger, view tab strip

**Press.** Click the 'Story' or 'Chain' tab button

| Arg | Type | Meaning |
|---|---|---|
| `view` | `enum` | 'Story' (body biography prose) or 'Chain' (generation chain charts across all bodies) |

**Valid when:**
- History ledger is open

**Expected output.** Switches the view. Re-clicking the currently-active tab closes the whole History ledger (toggle rule); switching tabs is an ordinary view change. Story is about one body (see the Body selector); Chain compares every body side by side and hides the selector. A third tab, Tiles, was retired 2026-08-03 (BL-281) - it was a current-state readout in a ledger about the past; its buildings list is on the canvas and in the Selection element, its market data in the market surfaces.

**Reason to select.** Story answers 'what is this body's biography?'; Chain answers 'how do the bodies compare through the generation stages?' - the two halves of how this world came to be.

### `ledger.market_body_selector` — Market Ledger, 'Body' combo

**Press.** Open the Body combo and pick a body

| Arg | Type | Meaning |
|---|---|---|
| `body` | `entity` | Which body's markets to view; defaults to the home body |

**Valid when:**
- Market Ledger is open
- The body has at least one market

**Expected output.** Repoints the ledger at the chosen body; the Market selector refills with that body's markets and the price/order tables refresh. A cross-cutting target selector - exempt from the toggle rule (re-picking the same body is a no-op, it never closes anything).

**Reason to select.** Prices differ per body; this chooses WHERE the price question is being asked - essential for comparing sale destinations.

### `ledger.market_market_selector` — Market Ledger, 'Market' combo

**Press.** Open the Market combo and pick a market centre on the selected body

| Arg | Type | Meaning |
|---|---|---|
| `market` | `entity` | Which market centre (city) on the selected body |

**Valid when:**
- Market Ledger is open
- Selected body has more than zero markets (auto-selects the first)

**Expected output.** Repoints the price and sell-order tables at that market centre. Cross-cutting selector - exempt from the toggle rule.

**Reason to select.** A body can host several market centres with different books; this picks the exact venue for a trade.

### `ledger.market_view_tab` — Market Ledger, view tab strip at the top

**Press.** Click the 'Prices' or 'Sell Orders' tab button

| Arg | Type | Meaning |
|---|---|---|
| `view` | `enum` | 'Prices' (default) or 'Sell Orders' |

**Valid when:**
- Market Ledger is open

**Expected output.** Switches the ledger to that view. Re-clicking the CURRENTLY-ACTIVE tab closes the whole Market Ledger (toggle rule on tab strips); clicking the other tab is an ordinary view change. Prices shows the order book / price table for the selected market; Sell Orders shows the player's standing sell orders there.

**Reason to select.** Prices answers 'what is the market paying?'; Sell Orders answers 'what am I currently offering, at what floor?' - the two halves of a selling decision.

### `ledger.nav_budget` — Nav rail, slot 2 (Budget icon)

**Press.** Click the ledger glyph on the left icon rail

**Valid when:**
- In-game

**Expected output.** Toggles the Balance Ledger (Budget) open in the fold-out column; re-click closes it; opening closes any other open ledger. Open, it shows the money loop: income, costs, where the money went, a profit-per-tick line chart, a per-building profit rank table, and the stubbed Taxes/Wages tier controls. Single view - no sub-tabs.

**Reason to select.** Answers 'am I making or losing money, and which flow or building is responsible?' - feeds every spend/expand/cut decision.

### `ledger.nav_construction` — Nav rail, slot 6 (Construction icon)

**Press.** Click the industry/factory glyph on the left icon rail

**Valid when:**
- In-game

**Expected output.** Toggles the Construction panel open in the fold-out column; re-click closes; opening closes any other ledger. Open, it shows the Construction / Buildings split (defaults to Buildings): what is being built now, and the buildings the player owns with their management controls.

**Reason to select.** Answers 'what do I own and what is under way?' - the broad building overview; per-building operations (recipe, workforce, demolish) are reached from here but are separate presses.

### `ledger.nav_corporation` — Nav rail, slot 1 (Corporation overview icon)

**Press.** Click the corporation glyph on the left icon rail

**Valid when:**
- In-game (the rail is permanent chrome)

**Expected output.** Toggles the Corporation dashboard open in the fold-out column. Opening it closes whichever other ledger was open (accordion). If it is already open, clicking again closes it (toggle rule). Open, it shows the player corporation's four roll-up cards - Production, Trade, Workforce, Finance - each a one-line verdict with an expand chevron.

**Reason to select.** Answers 'how is my corporation doing overall, and where is the weak card?' - the top-level health check that decides which subsystem to drill into next.

### `ledger.nav_corporations_table` — Nav rail, slot 8 (Diplomacy icon - provisionally hosts the corporations table)

**Press.** Click the diplomacy glyph on the left icon rail

**Valid when:**
- In-game

**Expected output.** Toggles the all-corporations balance table open in the fold-out column; re-click closes; opening closes any other ledger. Open, it lists every corporation with its balance side by side (player row highlighted). Diplomacy itself is not built; this table is the slot's provisional occupant.

**Reason to select.** Answers 'how do rivals' finances compare to mine?' - the only side-by-side rival-balance read in the game.

### `ledger.nav_economy` — Nav rail, slot 3 (Workforce icon - provisionally hosts the Economy panel)

**Press.** Click the workforce (population) glyph on the left icon rail

**Valid when:**
- In-game

**Expected output.** Toggles the Economy panel open in the fold-out column; re-click closes; opening closes any other ledger. Open, it splits across three views - Corps (the player's balance trend, every corporation's balance, and the workforce table), Holdings (stockpile pools by body) and Markets (supply, demand and price per body). Workforce itself is not built; this panel is the slot's provisional occupant.

**Reason to select.** Answers 'what is the whole economy doing?' in one surface - balances, labour, stock on hand and market state side by side, rather than one body or one corporation at a time.

### `ledger.nav_history` — Nav rail, slot 9 (History icon)

**Press.** Click the history glyph on the left icon rail

**Valid when:**
- In-game

**Expected output.** Toggles the History ledger open in the fold-out column; re-click closes; opening closes any other ledger. Open, it shows how the world was generated, split into Story and Chain views.

**Reason to select.** Answers 'why is this world the way it is?' - the body's biography and the generation chain behind it; feeds site-selection and understanding of deposit placement.

### `ledger.nav_market` — Nav rail, slot 5 (Market Ledger icon)

**Press.** Click the market glyph on the left icon rail

**Valid when:**
- In-game

**Expected output.** Toggles the Market Ledger open in the fold-out column; re-click closes; opening closes any other ledger. Open, it shows prices, supply and demand for one market on one body (chosen by the Body and Market selectors), split into Prices and Sell Orders views.

**Reason to select.** Answers 'what does this good trade for, where?' - the price signal behind every sell-order, extraction and routing decision.

### `ledger.selection_body_goto_surface` — Selection band, body kind's action column, 'Go to surface' button

**Press.** Click 'Go to surface'

**Valid when:**
- A body (not the star) is selected
- The body is surveyed (an unsurveyed body shows Dispatch Survey instead - a world-mutating press owned by the survey family)

**Expected output.** Descends the canvas to the body's planetary tile surface (the most informative rung). Equivalent to the header's '>' for a body. No world change.

**Reason to select.** Surveying is done; the move now is to look at the ground - tiles, deposits, and rival footprints.

### `ledger.selection_close` — Selection band header (all kinds, including the tile card), 'x' button

**Press.** Click the 'x' button at the right of the band's header row

**Valid when:**
- Something is selected and the band is visible

**Expected output.** Hides the band for this selection. It is not destroyed: the next selection (clicking any entity) reopens it. There is no other way to reopen it - the band has no nav-rail slot; selection is its only opener.

**Reason to select.** Clears the bottom band to see the canvas; costs nothing since any click re-summons it.

### `ledger.selection_construct_open` — Selection band, tile card's right-hand 2x3 action grid, Construct (hammer) button

**Press.** Click the hammer icon button

**Valid when:**
- A tile is selected
- Something is actually placeable on the tile (otherwise the button disables with 'Nothing can be built on this tile')

**Expected output.** Opens the tile construction ledger (BL-162) in the fold-out column: every building type placeable on THIS tile, with full cost, expected-profit bar, and reason-coded validity. Opening it is non-mutating; the Build presses inside it belong to the construction action family. The button carries an accent ring ('primed') when the player has nothing under construction anywhere.

**Reason to select.** The front door to building on a specific tile - answers 'what could I put here and what would it earn?' before committing.

### `ledger.selection_drill_back` — Selection band, resource drill view, '<' button at the left of its header

**Press.** Click the '<' (Back) button

**Valid when:**
- A resource drill is open (drill stack non-empty)

**Expected output.** Pops one drill frame, returning to the previous view (usually the tile's metric accordion).

**Reason to select.** Done with the history; back to comparing the tile's other metrics.

### `ledger.selection_drill_close` — Selection band, resource drill view, 'x' button at the right of its header

**Press.** Click the 'x' (Close) button

**Valid when:**
- A resource drill is open

**Expected output.** Clears the entire drill stack AND hides the whole Selection band (unlike Back, which unwinds one level). The next selection reopens the band at its root.

**Reason to select.** Abandon the whole inspection in one press rather than backing out level by level.

### `ledger.selection_drill_scroll` — Selection band, resource drill view, horizontal slider under the chart

**Press.** Drag the slider left (older) or right (most recent)

| Arg | Type | Meaning |
|---|---|---|
| `window_start` | `int` | Left edge of the 8-quarter viewing window within the recorded history |

**Valid when:**
- A resource drill is open
- More history is recorded than the 8-quarter window (otherwise no slider draws)

**Expected output.** Scrolls the 8-quarter window through the recorded series. Until dragged, the window tracks the most recent data as time advances; dragging pins it to the chosen position.

**Reason to select.** Reads earlier eras of the tile's output - was it always this productive, or is the deposit running down?

### `ledger.selection_goto` — Selection band header, '>' (go-to) button

**Press.** Click the '>' button at the right of the band's header row

**Valid when:**
- Something is selected and the band is visible
- Selection kind is not a tile (the tile layout carries no go-to; a tile's go-to is a no-op today)

**Expected output.** Navigates to the selection - exactly equivalent to double-clicking it: a spatial entity (body, building, market, unit) moves the canvas (a body descends to its planetary surface); a non-spatial entity (nation, corporation) opens its ledger instead. The band itself stays; an open fold-out ledger is untouched.

**Reason to select.** The bridge from 'I am reading about it' to 'take me to where it lives' - the Host axis of the disclosure model.

### `ledger.selection_locate` — Selection band, market/unit kind's action column, 'Go to' button

**Press.** Click 'Go to'

**Valid when:**
- A market or unit is selected

**Expected output.** Navigates the canvas to the entity's position (its body's surface / location) via the same focus routing as the header '>' button. No world change.

**Reason to select.** Places the abstract row on the map - where IS this market or unit, and what surrounds it?

### `ledger.selection_manage_building` — Selection band, tile card's action grid, Manage (gear) button

**Press.** Click the gear icon button

**Valid when:**
- A tile is selected
- A building occupies the tile (otherwise disabled: 'No building here')

**Expected output.** Switches the selection to the occupying building: next frame the band shows the building's own layout (profitability, its operating controls). Pure re-selection - nothing in the world changes.

**Reason to select.** The route from ground to operator: 'there is a building here - show me its numbers and controls.'

### `ledger.selection_metric_drill` — Selection band, tile card's metric chart (in-band or expanded)

**Press.** Click anywhere on a deposited-resource chart

| Arg | Type | Meaning |
|---|---|---|
| `resource` | `resource` | Implicit - the resource of the page currently shown |

**Valid when:**
- A tile is selected
- The current page is a deposited resource (Habitability/Hazard pages are NOT drillable - no per-tile history is tracked for them; their tooltip says so)

**Expected output.** Drills into that resource's time-series history: the band (or overlay) replaces the comparison chart with the body's total as columns plus this tile's own series as a line, over a shared day axis. Hover tooltip announces 'Click for its history over time'. Pushes a frame onto the drill stack (max depth 20).

**Reason to select.** Moves from 'how good is this tile now?' to 'how has it produced over time?' - depletion and trend, the fact a long-term siting decision needs.

### `ledger.selection_metric_expand` — Selection band, tile card's metric accordion, fold chevron at the right of the pager row

**Press.** Click the chevron

**Valid when:**
- A tile is selected

**Expected output.** Opens the current metric page as a full-screen overlay - the band rests expanded-in-place (its fixed rect cannot shrink), so the chevron means 'give this the whole screen', not 'fold'. Re-clicking (or Esc) closes the overlay back to the band. If a resource drill is open, the overlay shows the drilled time series instead.

**Reason to select.** The band's chart is 260 px tall; the overlay gives it the axis room and legend space to actually compare values.

### `ledger.selection_metric_pager` — Selection band, tile card's centre metric accordion, '<' / '>' arrow buttons

**Press.** Click the left or right arrow beside the metric title ('Name (i/N)')

| Arg | Type | Meaning |
|---|---|---|
| `direction` | `enum` | 'previous' or 'next' page |

**Valid when:**
- A tile is selected
- Not already at the first/last page (the arrow disables at the ends)

**Expected output.** Pages the accordion to the adjacent metric: one page per resource deposited on the tile (this tile's yield vs a top-10% tile), then the tile's Habitability and Hazard scalars (vs the body average). One titled chart shows at a time.

**Reason to select.** Walks every measurable fact about the tile one chart at a time - the read that grades a prospective build site.

### `ledger.decision_feed_open` — Navigation rail slot 11, "AI decisions"

**Press.** Click nav rail slot 11 to open the AI decision feed; click it again to close

**Valid when:**
- In game (not the menu or the generation screen)

**Expected output.** The AI decision feed opens in the shell fold-out column, closing whatever ledger was open (the column holds one occupant). It lists recent strategic decisions newest-first: date, corporation, verb and target, reason, and the winning/runner-up score pair. Clicking the slot while the feed is already open closes it (standing toggle rule).

**Reason to select.** To read WHY the rival corporations did what they did, and how close each call was - the only surface that exposes the scorer's rationale. A near-tied margin means the tuning could have gone the other way; a wide one means conviction.

### `ledger.decision_feed_filter_corp` — AI decision feed, corporation filter

**Press.** Select a corporation from the feed's corp selector, or "All"

| Arg | Type | Meaning |
|---|---|---|
| `corp` | `entity id or "all"` | Which corporation's decisions to show; "all" clears the filter |

**Valid when:**
- AI decision feed is open

**Expected output.** The list narrows to that corporation's decisions. The filter persists across selection changes elsewhere in the game - clicking a tile does not clear it.

**Reason to select.** To follow ONE competitor's run rather than the whole field. Reading a single corp's decisions in order is how a strategy becomes visible as a sequence rather than as noise.

### `ledger.decision_feed_filter_reason` — AI decision feed, reason filter

**Press.** Select a decision reason from the feed's reason selector, or "All"

| Arg | Type | Meaning |
|---|---|---|
| `reason` | `corp_decision_reason or "all"` | best_build / dial_workforce / dial_recipe / dial_idle / dial_resume / survey_expand / hire_available / trade_surplus |

**Valid when:**
- AI decision feed is open

**Expected output.** The list narrows to decisions taken for that reason, across every corporation unless the corp filter is also set.

**Reason to select.** To ask a specific question of the run rather than scroll it. 'Every solvency-defence idle' and 'every build' are different questions, and the reason code is what separates them.

### `ledger.strategy_readout_open` — Navigation rail slot 12, "Strategy readout"

**Press.** Click nav rail slot 12 to open the Strategy readout; click it again to close

**Valid when:**
- In game (not the menu or the generation screen)

**Expected output.** The Strategy readout opens in the shell fold-out column, closing whatever ledger was open (the column holds one occupant). It aggregates each corporation's strategic decisions over a rolling 64-quarter window: with no corp selected, one must-have/should-have/nice-to-have bucket-split bar per corporation; with a corp selected, its bucket split quarter by quarter (a stacked band), its verb mix, and its reason tally. Score and margin figures are deliberately absent (candidates sort by priority bucket before score, so raw margins do not aggregate honestly). Clicking the slot while the readout is already open closes it (standing toggle rule).

**Reason to select.** To see WHAT STRATEGY IS EMERGING rather than the individual moves - the decision feed lists the moves; this shows the shape of a run. A corp living in must-have is defending its solvency; one living in nice-to-have is expanding unopposed.

### `ledger.strategy_readout_select_corp` — Strategy readout, corporation selector

**Press.** Select a corporation from the readout's corp selector, or "All"

| Arg | Type | Meaning |
|---|---|---|
| `corp` | `entity id or "all"` | "all" shows the per-corp bucket-split comparison; a corporation shows its full profile (bucket band by quarter, verb mix, reason tally) |

**Valid when:**
- Strategy readout is open

**Expected output.** The view switches between the all-corporations comparison and one corporation's full profile. The selection persists across selection changes elsewhere in the game - clicking a tile does not clear it.

**Reason to select.** The comparison answers 'who is winning the run and how'; the single-corp profile answers 'what is this corp's strategy made of'. Two different questions over the same window.

### `ledger.nav_contracts` — Nav rail, slot 13 (Contracts icon — a page with a signed check mark)

**Press.** Click the contract glyph on the left icon rail

**Valid when:**
- In-game

**Expected output.** Toggles the Contracts ledger open in the fold-out column; re-click closes; opening closes any other ledger. Open, it shows three views: Offers (open mercenary_offers the activity fog admits), Active (the player's own live mercenary_contracts), History (the player's terminal-state contracts).

**Reason to select.** Answers 'who wants to hire me, what am I on the hook for, and how has it gone?' — the mercenary contract is the sell-side income loop (CONTRACTS.md) and the only door onto it.

### `ledger.contracts_view_tab` — Contracts ledger, view tab strip at the top

**Press.** Click the 'Offers', 'Active' or 'History' tab button

| Arg | Type | Meaning |
|---|---|---|
| `view` | `enum` | 'Offers' (default), 'Active' or 'History' |

**Valid when:**
- Contracts ledger is open

**Expected output.** Switches the ledger to that view. Re-clicking the CURRENTLY-ACTIVE tab closes the whole Contracts ledger (toggle rule on tab strips); clicking a different tab is an ordinary view change.

**Reason to select.** Offers answers 'who wants to hire me'; Active answers 'what am I on the hook for now'; History answers 'how has it gone' — three different questions over the same mercenary-contract record.

---

## Chrome — startup, the system menu, settings, F-keys

### `chrome.esc` — Keyboard, in-game only

**Press.** Press Esc.

**Valid when:**
- App is in game. On the main menu and the wizard, Esc does nothing (the key handler returns before Esc handling on those screens).
- Works even while an ImGui panel holds keyboard focus — Esc is handled before the ImGui keyboard guard.

**Expected output.** Exactly one rung of this precedence ladder fires, highest first: (1) an armed exit-confirm backs out (Really quit? disarms); (2) an open system menu closes; (3) if the sticky selection card is open and has a drill stack, one drill level unwinds; (4) if the corporation roll-up is drilled into a constituent, it returns to the roll-up; (5) if a fold overlay is expanded full-screen, it folds up; (6) an open sticky card hides (hidden for this selection, not destroyed); (7) otherwise the system menu opens. One press never does two of these — e.g. Esc reaches the menu only once the card is fully closed.

**Reason to select.** The universal step-out key: back out of a confirmation, close the menu, unwind a drill, fold an overlay, dismiss the card — or, with nothing open, reach the session menu.

### `chrome.f10_options` — Keyboard; opens the centred Options window

**Press.** Press F10 (press again, click Close, or click the window's X to dismiss).

**Valid when:**
- App is in game.
- Not while an ImGui widget owns the keyboard.

**Expected output.** Toggles the Options window: a Display section (Resolution preset combo, Fullscreen, VSync), an Accessibility section (UI Scale), a live window-size readout, and Close. Every change applies live to the SDL window and persists to options.cfg immediately.

**Reason to select.** To adjust display and UI-scale settings mid-session.

### `chrome.f11_frame_hud` — Keyboard; works on every screen (menu, wizard, and in game)

**Press.** Press F11 (press again, or use the HUD's own close button, to put it away).

**Valid when:**
- None — handled before the screen guard, so the frame budget can be watched on the menu and the generation screen too.

**Expected output.** Toggles the frame-budget HUD, the per-frame timing instrument. In game it first anchors top-left of the canvas area (movable after); on the pre-play screens it insets from the top-left corner. Honest status: this is a dev/audit instrument (the v0.1.0 performance audit tool), not shipped player chrome, and it sits outside the shared canvas command vocabulary.

**Reason to select.** To watch build/submit/present frame costs while reproducing a performance concern.

### `chrome.f12_capture` — Keyboard; works on every screen and regardless of focus

**Press.** Press F12.

**Valid when:**
- None — handled first in the key handler, before the screen guard and the ImGui keyboard guard.

**Expected output.** The exact composited frame (captured before present) is written as a PNG to screenshots/io_<ticks>.png in the working directory, creating the screenshots/ folder if needed. In interactive play there is no golden comparison — named captures and golden diffing belong to the --verify harness, not F12.

**Reason to select.** To record what is on screen — evidence for a bug report, a layout question, or a state worth keeping.

### `chrome.f1_help` — Keyboard; opens a centred Key Bindings window

**Press.** Press F1 (press again, or click the window's X, to close).

**Valid when:**
- App is in game (the binding table is gated off on the menu and wizard).
- Not while an ImGui widget owns the keyboard (typing in a text field suppresses it).

**Expected output.** Toggles a two-column Action/Key cheat-sheet, generated from the same binding table the key handler loops over, so it can never drift from the real bindings. F11 and F12 sit outside that table and are appended to the sheet by hand.

**Reason to select.** To look up every keyboard shortcut without leaving the game.

### `chrome.f9_tech_tree` — Keyboard; opens the mock tech-tree viewer

**Press.** Press F9 (press again to close).

**Valid when:**
- App is in game.
- Not while an ImGui widget owns the keyboard.

**Expected output.** Toggles a read-only viewer over scripts/tech_tree.lua, tabbed by era: Era -1 Antiquity (placeholder pointing at the BL-307 ladder store), Era 0, Era 1, and Standing lines; eras with no authored quests show a placeholder. Honest status: this is a mock — a design aid with no simulation coupling; nothing can be researched and nothing in the world reads it.

**Reason to select.** To browse the drafted tech-tree content; of no strategic use to an AI player yet.

### `chrome.gear_toggle` — In-game chrome, top-right gear button just left of the time column

**Press.** Click the gear (three-bar) button.

**Valid when:**
- App is in game (app_screen::in_game).

**Expected output.** Toggles the small system-menu popup open or closed beneath the gear. Closing it via the gear also disarms any pending exit confirmation.

**Reason to select.** Mouse route to the session controls (Pause/Resume, Exit Game); Esc is the keyboard parity for the same popup.

### `chrome.menu_copy_seed` — Main menu, New World block, Copy seed button

**Press.** Click Copy seed.

**Valid when:**
- App is on the main menu screen.

**Expected output.** The current seed, formatted as 8 hex digits, is placed on the system clipboard. Nothing else changes.

**Reason to select.** To record or share the reproducible key for the world about to be generated.

### `chrome.menu_new_game` — Main menu, primary button column

**Press.** Click New Game.

**Valid when:**
- App is on the main menu screen.

**Expected output.** The screen transitions to the New World wizard (app_screen::generating), opening on round 1 of 3. Nothing is generated or committed by this press; the wizard only previews.

**Reason to select.** To start setting up a new world; this is the only route into play.

### `chrome.menu_quit` — Main menu, primary button column

**Press.** Click Quit.

**Valid when:**
- App is on the main menu screen.

**Expected output.** The application requests quit and exits. No confirmation is asked here (nothing exists to lose on the menu). Settings were already persisted to options.cfg as they changed.

**Reason to select.** To leave the application from the entry screen.

### `chrome.menu_roll_seed` — Main menu, New World block, Roll button beside the seed field

**Press.** Click Roll.

**Valid when:**
- App is on the main menu screen.

**Expected output.** The seed field is replaced with a fresh random draw (a one-shot random_device read; the only place entropy touches the pipeline). No world is generated yet.

**Reason to select.** To get a fresh world without caring which one; the seed shown remains the reproducible key.

### `chrome.menu_set_abundance` — Main menu, New World block, Resources radio row

**Press.** Click one of the three radio buttons: Sparse, Lean, or Standard.

| Arg | Type | Meaning |
|---|---|---|
| `level` | `enum: sparse | lean | standard` | Resource abundance tier. Standard is the Earth-like ceiling; Sparse and Lean step down from it. There is no tier above Standard. |

**Valid when:**
- App is on the main menu screen.

**Expected output.** The pending world parameters carry the chosen abundance level, consumed at Begin. The Bodies slider next to this row is disabled (fixed at 5) and cannot be pressed.

**Reason to select.** To choose how resource-rich the generated world is; leaner tiers make extraction siting and trade tighter.

### `chrome.menu_set_seed` — Main menu, New World block, Seed field

**Press.** Type an 8-digit hex value into the seed input box.

| Arg | Type | Meaning |
|---|---|---|
| `seed` | `hex string (8 digits, u32)` | The world seed. Generation is a pure function of this value plus the knobs: same seed and knobs give an identical world. |

**Valid when:**
- App is on the main menu screen (app_screen::menu).

**Expected output.** The pending world parameters carry the typed seed. Nothing is generated; the seed is consumed only when the wizard's Begin is pressed.

**Reason to select.** To reproduce a specific known world, or to share/compare a world by its seed.

### `chrome.options_close` — Options window, bottom

**Press.** Click Close.

**Valid when:**
- Options window open.

**Expected output.** The Options window closes. All changes were already applied and saved as they were made; Close commits nothing.

**Reason to select.** To put the window away after adjusting settings.

### `chrome.options_fullscreen` — Options window, Display section

**Press.** Click the Fullscreen checkbox.

| Arg | Type | Meaning |
|---|---|---|
| `on` | `bool` | Target state; the press toggles the current one. |

**Valid when:**
- Options window open.

**Expected output.** Fullscreen applies live and persists to options.cfg immediately. While on, the Resolution combo is disabled; the stored windowed size is set first so leaving fullscreen restores it.

**Reason to select.** To switch between fullscreen and windowed play.

### `chrome.options_set_resolution` — Options window, Display section, Resolution combo

**Press.** Open the combo and click a preset.

| Arg | Type | Meaning |
|---|---|---|
| `preset` | `enum: 1280x720 | 1600x900 | 1720x1080 | 1920x1080 | 2560x1440` | Windowed size preset. A free drag-resize of the frame shows as 'Custom' in the combo. |

**Valid when:**
- Options window open; Fullscreen is off (the combo is disabled while fullscreen).

**Expected output.** The SDL window resizes immediately and the choice persists to options.cfg at once.

**Reason to select.** To fit the window to the display or to a capture-friendly size.

### `chrome.options_set_ui_scale` — Options window, Accessibility section, UI Scale combo

**Press.** Open the combo and click a step.

| Arg | Type | Meaning |
|---|---|---|
| `step` | `enum: 1.0x | 1.25x | 1.5x` | Global UI scale step (stored as index 0-2; a corrupt config clamps back into this range). |

**Valid when:**
- Options window open.

**Expected output.** UI scale applies live and persists to options.cfg immediately.

**Reason to select.** To make the interface legible on dense or high-DPI displays.

### `chrome.options_vsync` — Options window, Display section

**Press.** Click the VSync checkbox.

| Arg | Type | Meaning |
|---|---|---|
| `on` | `bool` | Target state; the press toggles the current one. |

**Valid when:**
- Options window open.

**Expected output.** Vertical sync applies live to the renderer and persists to options.cfg immediately.

**Reason to select.** To trade tearing against frame latency, or to uncap frames when measuring with the F11 HUD.

### `chrome.sysmenu_exit` — System menu popup, below the separator

**Press.** Click Exit Game.

**Valid when:**
- App is in game; the system menu popup is open; no exit confirmation is currently armed.

**Expected output.** Arms an inline confirmation: the button is replaced by a 'Really quit?' prompt with Yes, quit and Cancel. Nothing exits yet. The confirm exists because there is no save in the prototype — quitting discards the session.

**Reason to select.** First step of leaving a session deliberately, with a guard against a stray click.

### `chrome.sysmenu_exit_cancel` — System menu popup, armed exit confirmation

**Press.** Click Cancel (or press Esc, which backs the armed confirm out as its highest-precedence rung).

**Valid when:**
- App is in game; the exit confirmation is armed.

**Expected output.** The confirmation disarms and the popup returns to showing Exit Game. Play continues unaffected.

**Reason to select.** To back out of an exit started by mistake.

### `chrome.sysmenu_exit_confirm` — System menu popup, armed exit confirmation

**Press.** Click Yes, quit.

**Valid when:**
- App is in game; the system menu is open and the exit confirmation is armed.

**Expected output.** The application quits. The session is not saved (there is no save in the prototype); display settings already live in options.cfg.

**Reason to select.** To end the session for good.

### `chrome.sysmenu_pause` — System menu popup, top button

**Press.** Click Pause (or Resume — the label reflects the current sim state).

**Valid when:**
- App is in game; the system menu popup is open.

**Expected output.** Toggles simulation pause via the same pause_toggle path as the Space hotkey. Resuming restores the previous speed tier. The button label flips accordingly.

**Reason to select.** To halt or resume the simulation clock from the mouse-driven session menu.

### `chrome.sysmenu_god_view` — System menu popup, spectate-only checkbox

**Press.** Toggle the God view checkbox. Rendered only while spectating (corp_ai_params::spectating); unreachable in a played session by construction.

**Valid when:**
- App is in game; the system menu popup is open; the session is spectating (BL-409).

**Expected output.** While on, competitor-visibility redactions lift for the WATCHER only: rival building internals (production, stockpile, profitability read-only page), corp cash/reserve facts, and the survey tell (unsurveyed tiles render through a lock-colour wash so the corp's own blindness stays visible). Off restores the BL-068 redactions with no residue. Never changes what any AI reads; the rival action grid stays disabled - sight, never hands.

**Reason to select.** To audit a watched corp's decisions against what it actually knew, held and ran - honest watching vs omniscient watching is a deliberate, visible state (BL-408).

### `chrome.tech_tree_era_tab` — Tech-tree viewer (F9), era tab strip

**Press.** Click the 'Era -1 Antiquity', 'Era 0 — Terrestrial', 'Era 1 — Early Space' or 'Standing lines' tab button

| Arg | Type | Meaning |
|---|---|---|
| `view` | `enum` | 'Era -1 Antiquity' (placeholder pointing at the BL-307 ladder store) / 'Era 0 — Terrestrial' (the default) / 'Era 1 — Early Space' / 'Standing lines' (span eras, never gate one) |

**Valid when:**
- Tech-tree viewer is open (F9)

**Expected output.** Switches which era's tree is shown — each era carries its own tree; eras with no authored quests show a placeholder. Re-clicking the currently-active tab closes the viewer (toggle rule); switching tabs is an ordinary view change.

**Reason to select.** To browse a specific era's drafted content; of no strategic use to an AI player yet — the viewer is a mock with no simulation coupling.

### `chrome.wizard_back` — New World wizard, footer button row

**Press.** Click Back.

**Valid when:**
- App is on the wizard screen.

**Expected output.** On rounds 2-3: steps back one round (a plain revision — there is no per-round snapshot, since rounds are causal). On round 1: returns to the main menu. Leaving costs nothing (nothing is generated until Begin), and preferences survive the trip, so re-entering resumes the same leans from round 1.

**Reason to select.** To revise an earlier round's preferences, or to abandon world setup and return to the menu.

### `chrome.wizard_begin` — New World wizard, footer button row (right edge), final round

**Press.** Click Begin.

**Valid when:**
- App is on the wizard screen, on the last round (round 3 of 3).

**Expected output.** The one and only generation call (start_new_game): the sim clock is rebased, the world is built from the pending parameters, the economy loads, a pre-game warm start runs 12 quarterly ticks (~3 in-game years) so play opens onto live markets and non-empty pools, the clock is rebased again, and the screen transitions to in_game. Play opens on the corporation's home planet at the Planetary rung with the home body selected.

**Reason to select.** To commit the chosen seed and preferences and start the campaign.

### `chrome.wizard_continue` — New World wizard, footer button row (right edge)

**Press.** Click Continue.

**Valid when:**
- App is on the wizard screen, on round 1 or 2 (any round except the last).

**Expected output.** Advances to the next round. The button reads Continue on non-final rounds and Begin on the last; this press generates nothing.

**Reason to select.** To accept this round's roll and preferences and move to the next batch of decisions.

### `chrome.wizard_reroll` — New World wizard, footer button row

**Press.** Click Reroll.

**Valid when:**
- App is on the wizard screen.

**Expected output.** The current round is re-drawn from a fresh number under the same preferences, and — because the chain is causal — everything downstream of it re-draws too. Charts refresh; nothing is generated. Rounds already behind the player are not re-rolled by this press.

**Reason to select.** To get a different concrete roll for this round's stages without changing any stated preference.

### `chrome.wizard_set_lean` — New World wizard, the preference block at the bottom of the current round

**Press.** Click one of the four segmented options on a preference row: Any, or the row's low/mid/high named lean.

| Arg | Type | Meaning |
|---|---|---|
| `axis` | `enum by round` | Round 1 rows: Star (Dimmer / Sun-like / Brighter), World (Small / Earth-like / Large), Interior (Old and cold / Moderate / Young and vigorous), Metal (Metal-poor / Normal / Metal-rich). Round 2 rows: Ocean (Continental / Balanced / Oceanic), Oxygen (Oxygenated early / Balanced / Oxygenated late), Coal basins (Seasonal / Mixed / Everwet). Round 3 row: Drawdown (Barely touched / Worked / Stripped). |
| `lean` | `enum: any | low | mid | high` | A named lean, not a parameter: it narrows the range the seed is sampled from and never pins a value. No raw generated number is editable or shown in the decision area. |

**Valid when:**
- App is on the wizard screen, on the round that owns the row.

**Expected output.** The preview chain re-runs as a pure throwaway (resolve_preferences + preview_system); the charts above update to show the new roll. The world itself (m_world) is untouched. Resolution internally rerolls until the homeworld clears the Earth-like floor; if it took more than one draw the screen says 'Found on attempt N', and if no draw cleared the floor it says so and shows the closest world found. The Oxygen row carries the one spelled-out trade: oxygenated early is coal-rich and iron-lean, oxygenated late is iron-rich and coal-lean.

**Reason to select.** To bias the generated world along a named axis — resource character, land/ocean split, remaining untapped deposits — while keeping generation seed-driven.

### `chrome.wizard_stage_fold` — New World wizard, the charts area of the current round

**Press.** Click a stage's fold row (its one-line verdict) to expand it; click again to fold it back to the verdict line.

| Arg | Type | Meaning |
|---|---|---|
| `stage` | `chain stage within the current round` | Which of the round's Planetology stages to open. Round 1 covers the star/world/interior/metal stages, round 2 the ocean/oxygen/coal stages, round 3 the drawdown stage. |

**Valid when:**
- App is on the wizard screen (app_screen::generating).

**Expected output.** The stage expands from its verdict line into its full chart-and-explanation view, or folds back. Purely presentational; no preference or preview state changes. This is the same fold idiom the in-game ledgers use — the wizard is where it is taught.

**Reason to select.** To inspect what the current roll actually produced for a stage before setting or judging a preference; the charts are the only honest feedback (no raw values are printed).

### `chrome.quick_save_key` — Keyboard.

**Press.** Press F5.

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus.

**Expected output.** Writes the whole campaign to the quick-save slot — a single file `quicksave.iosave` beside the executable, overwritten each time without a prompt. The file carries the world snapshot (every component store, the order book, procurement, stance, laws, research, battles in progress, the province partition and the history log) plus the app envelope (sim clock, world_params, the full generation report, the per-tick histories, and the view slice: rung, framing, lens, selection). The outcome is posted to the Field comms channel either way — a save that silently did nothing is worse than one that says it failed. The sim, view and selection are otherwise untouched.

**Reason to select.** Fix a point to come back to before doing something irreversible, or before a long run whose outcome you may want to compare against a re-run from here.

### `chrome.quick_load_key` — Keyboard.

**Press.** Press F6.

**Valid when:**
- The app is in-game and no ImGui text field has keyboard focus.
- A quick-save exists (F5 has been pressed at least once in some session).

**Expected output.** Replaces the live campaign with the quick-save slot's contents, restoring the world, the clock, the generation report, the histories and the view exactly as saved. NO CONFIRMATION PROMPT — the current campaign is discarded. If the file is missing, corrupt, or written by a build with a different save-format version, the load is REFUSED and the running campaign is left completely untouched, with the reason posted to the Field comms channel; a refused load costs nothing. Panel open/closed state is not restored, deliberately.

**Reason to select.** Return to the last fixed point — undo a run of decisions wholesale, or re-open the same starting position to try a different line from it.

### `chrome.load_cli` — Command line.

**Press.** Launch with `ProjectIo --load <path>`.

| Arg | Type | Meaning |
|---|---|---|
| `path` | `string` | undefined |

**Valid when:**
- The named file exists and was written by a build with the same save-format version.

**Expected output.** Opens straight into the saved campaign, skipping the main menu, the New World wizard, and the world generation plus pre-game warm start that follow them (~30-45 s). A failed load falls through to the main menu with the reason printed, rather than exiting — a missing file must not look like a crash.

**Reason to select.** Resume a campaign directly, or open a fixed world for a capture run without paying generation on every launch — the reason the save format exists (BL-536).

