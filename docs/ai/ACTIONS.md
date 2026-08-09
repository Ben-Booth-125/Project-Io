# Project Io — the action dictionary

Every control in the game: what pressing it does, and why you would. Readable
mirror of [`ACTIONS.json`](ACTIONS.json), which is canonical — the JSON is the
machine-consumable half an AI player reads (BL-270). Pair it with the corp
blackboard export (BL-206, the read channel) and the corp-command seam
(`src/world/corp_command.hpp`, the write channel) and an LLM has the full
word interface to play through.

> **Generated file.** Produced by `node tools/session/render_actions.js`.
> Edit the JSON, then re-run; hand edits here are overwritten.

*116 entries — 11 gameplay · 24 canvas · 15 lens · 37 ledger · 29 chrome.*

---

## Gameplay — presses that mutate the world

### `gameplay.build` — Tile construction ledger (fold-out column), opened from the Selection band of a selected tile; a shortcut lives on a selected owned building ('Build another here').

**Press.** Single-click a tile on the Planetary canvas (the Selection band appears), click 'Construct Buildings' in the band's action grid, then click 'Build' on a candidate row in the ledger. Rows are one per extractable resource deposited on the tile (plus a coastal Fishing Wharf row even with zero deposit), one per processing recipe, then Port, Launchpad, Inland Logistics Hub. Alternate press: with an owned building selected, 'Build another here' repeats its type/target on the same tile, gated by the tile's stack capacity.

| Arg | Type | Meaning |
|---|---|---|
| `tile` | `entity_id` | The selected tile the building goes on. |
| `type` | `building_type` | Which building — extraction_site, processing_facility, port, launchpad, inland_logistics_hub. Set by which row is pressed. |
| `target` | `resource_type` | Extraction rows only: the deposited resource the site extracts. Ignored for other types. |
| `recipe` | `uint16 recipe id` | Processing rows only: the recipe the facility is seeded with (the row it was priced on). no_recipe elsewhere; a recipe-less processor seeds the default steel recipe. |

**Valid when:**
- The acting corporation exists (rejected_no_corp otherwise).
- The tile entity exists (rejected_invalid otherwise).
- placement_rules::can_place_in_world accepts (type, target) on this tile — ocean, missing deposit, a port off the coast, or a full per-body slot cap (Launchpad: max 1 per body) all refuse (rejected_placement).
- The corporation's balance covers the full capex: registry build_cost plus the material costs priced at the tile's local market (rejected_funds). The UI shows this one credit total and disables Build with 'Can't afford' when short.

**Expected output.** The press enqueues a construction request; the app's mutable pass executes construct_building the same frame. On success a building entity exists immediately — staffed at 50% workforce (0 for a port), a processing facility seeded with the pressed row's recipe — and the capex is debited up front. Construction is then DURATIVE and material-gated: each economy tick (one quarter) it advances at a rate in [0,1] set by how much of its per-tick material need the local market can supply; scarce materials stretch the ETA and total shortage shows 'Paused - market can't supply materials'. Management controls unlock only when construction completes. A rejected attempt mutates nothing; the reason string appears at the top of the ledger (construction.last_message), and invalid rows already show reason-coded text in place of the Build button ('Cannot build on water', 'A port must sit on the coast', ...).

**Reason to select.** The only way to add productive capacity. Extraction turns a tile deposit into pool stock to sell; processing turns inputs into higher-margin outputs; ports/hubs move goods cheaper and a launchpad gates space access. The ledger ranks candidates by expected net per quarter and prints payback, so build is the press when a candidate's expected profit beats holding the cash.

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

### `gameplay.place_sell_order` — The 'Sell Orders' tab of the Market Ledger (opened from the nav rail; the tab is scoped to the ledger's selected market body).

**Press.** Open the Market Ledger, switch to the Sell Orders tab, pick a resource in the combo (only resources the market prices are listed), type a Quantity/qtr and a Floor price, click 'Add sell order'. The button is disabled with no resource chosen or quantity <= 0.

| Arg | Type | Meaning |
|---|---|---|
| `body` | `entity_id` | The body whose market the order lists on — the ledger's currently selected body. |
| `resource` | `resource_type` | What to sell. Must have a positive base price on this market. |
| `quantity` | `float > 0` | Maximum units offered per quarter, capped each tick by what the (corp, body) pool actually holds. |
| `floor_price` | `float >= 0` | Minimum acceptable unit price. 0 means sell at the market price. |

**Valid when:**
- The selected body has a market ('This body has no market.' otherwise, and no form renders).
- The resource is priced on that market (base_price > 0).
- Quantity is positive.

**Expected output.** A STANDING order — it persists until removed and is evaluated every economy tick, not once. Each tick it lists up to `quantity` from the corp's pool on that body (an empty pool lists nothing, silently); it clears at max(resolved market price, floor) — with no matching buyer it still auto-clears at that price, so offered supply always clears in the prototype, but a floor above what the resolution supports means less or nothing sells that tick. CRITICAL side effect: the (corp, body, resource) triple leaves the automatic surplus-selling path — the auto path yields to the manual order — so a too-high floor does not just fail to sell, it stops that resource selling AT ALL on that body and the stock piles up. There is no rejection enum; a bad order simply sits there selling nothing.

**Reason to select.** Price and quantity control the auto-sell path lacks: floor-protect against dumping stock into a crashed price, or meter quantity to ration a stockpile toward a construction project or a better market. The manual side of the trade loop — the press when 'sell everything at whatever it fetches' is the wrong answer.

### `gameplay.remove_sell_order` — The 'Remove' button beside each listed order on the Market Ledger's Sell Orders tab.

**Press.** Open the Market Ledger, switch to the Sell Orders tab; each of the player's standing orders on the selected body renders as a row ('<resource> x<qty> >= <floor>') with a small 'Remove' button. Click Remove on the target row. Immediate — no confirmation.

| Arg | Type | Meaning |
|---|---|---|
| `order` | `row reference` | Which standing order to remove, identified by the row pressed — i.e. the (body, resource, quantity, floor_price) tuple displayed. Internally the order's index in the sell-order list. Only the player's own orders on the ledger's selected body are listed. |

**Valid when:**
- The order exists — the button only renders on rows that do, so there is no rejection mode; a body with no orders shows 'No sell orders on this body.' and nothing to press.

**Expected output.** The order is erased from the standing sell-order list immediately, at no cost. From the next economy tick that quantity is no longer listed for sale. Routing consequence: if this was the LAST remaining order for that (corp, body, resource) triple, the triple returns to the automatic surplus-selling path — each tick the pool's surplus above processor reservation auto-sells at the market's reference price, with no floor protection. If another order for the same triple still stands, manual control persists. Nothing else changes; already-cleared past sales are untouched.

**Reason to select.** Ends manual control over a resource's sales: hand it back to the auto-surplus path when floor-pricing or rationing is no longer wanted (e.g. the price crash passed, or a too-high floor was silently stockpiling the resource instead of selling it). Also the only way to correct a mistyped order — there is no edit; remove and re-add.

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

### `gameplay.set_recipe` — The 'Production method' combo on the Selection band's building layout; the same combo also appears as 'Production Methods' in the Building panel's Buildings-tab inline detail.

**Press.** Select the owned building, open the production-method combo, click a recipe row (each row carries a resource pip for its primary output).

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

**Expected output.** An immediate component write — no queue, no cost. From the next economy tick the building consumes the new recipe's inputs and produces its outputs; its profit readout, its input demand on the local market, and what it contributes to the pool all change with it. Re-selecting the recipe already active is a no-op (rejected_state at the command seam).

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

**Expected output.** workforce_target is set AND workforce_auto is cleared — a manual move pins the dial, so the per-tick profit-max auto-solver stops adjusting this building until Auto is re-enabled. This IS the workforce-auto opt-out. Actual staffing is the target mediated by body-level labour contention: when the corp's demand on the body exceeds the habitability-sized pool, every building is throttled by the same fraction and the band prints 'Body allows N%'. Wages scale with assigned workforce; output scales with effective workforce. Setting the value already held is a no-op (rejected_state).

**Reason to select.** Trades wage bill against output when the solver's profit-max answer is not what is wanted — throttle to cut losses without idling, staff to 0 to park a building cheaply, or overdrive past 100% to feed a downstream chain even at a per-building loss. Also the deliberate press for taking manual control away from the auto-solver.

### `gameplay.set_workforce_auto` — The 'Auto' checkbox beside the band's workforce slider; the 'Auto (N%)' button above the panel's tier grid.

**Press.** Select the owned building, click the Auto checkbox (band) or the Auto button (panel). Clicking while already on is a plain re-assert on the checkbox; the button simply sets it on.

| Arg | Type | Meaning |
|---|---|---|
| `building` | `entity_id` | The building handed back to the auto-solver. |

**Valid when:**
- The acting corporation exists.
- The corporation owns the building.
- Construction is complete (controls hidden until then).

**Expected output.** workforce_auto := true. Each economy tick the solver sets this building's workforce_target to the profit-maximising value; the panel's Auto button displays the currently solved percentage. This is the only sanctioned auto-action on the player's corporation — it moves one dial and never places, relocates, retargets, or decommissions anything. Any later manual target (slider drag or tier button) clears it again.

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

**Press.** Single left-click on an entity: a body on the Solar/Circumplanetary rungs, a tile or marker on the Planetary surface.

| Arg | Type | Meaning |
|---|---|---|
| `target` | `entity` | The entity under the cursor. Overlapping candidates resolve to one entity: the stack building > market > unit > tile > body is walked most-specific first, the active lens filters validity, and nearest-to-cursor (entity id breaking ties) picks a single stable winner. |

**Valid when:**
- The app is in-game (not the main menu or New World wizard).
- The pointer is over the primary canvas, not over the minimap inset or any ImGui panel (panels capture the mouse).
- An entity is under the cursor (empty space is a different press — see canvas.deselect).

**Expected output.** selected_entity becomes the target and the Selection band (fixed strip at the bottom of the screen) appears or re-points, showing that kind's action and facts; a new selection resets any drill-down stack in the band. Nothing else changes: same rung, same pan, same zoom, active_body untouched, no lens change, the canvas is not re-skinned, and any open fold-out ledger stays open. A single click never navigates.

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

**Expected output.** The current rung's zoom factor steps in or out, clamped to the same per-rung bounds the wheel and slider share. Unlike the wheel there is no cursor anchor to aim — the view scales in place. No selection, rung, lens, or speed change.

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

**Expected output.** The current rung's zoom factor changes, anchored at the cursor: the point under the pointer stays put while the view scales around it. Per-rung state; the zoom slider (where present) moves to reflect the new factor. No selection, rung, pan-recentre, lens, or speed change.

**Reason to select.** Move closer to or further from a specific spot — cursor anchoring means you aim the zoom at the thing you are interested in.

---

## Lenses — re-skinning the map to answer a question

### `lens.clear` — Minimap lens bar — the glyph of whichever lens is currently active (shown highlighted)

**Press.** Single left-click on the currently-active (highlighted) lens glyph. This is the family's one toggle behaviour, stated here once: the bar is single-select with a null state, so re-selecting the active lens clears to no-lens (overlay_mode::none). Each bar-lens entry references this. Off-bar lenses have no glyph to re-click; they are cleared via the keyboard cycle or the clear hotkey (controls family).

**Valid when:**
- A bar lens must currently be active — with no lens active there is no highlighted glyph and this press does not exist.
- Selecting a different lens is an ordinary switch, not a clear; only re-clicking the active one clears.

**Expected output.** The canvas returns to plain terrain (overlay_mode::none) — the state the campaign opens in. All lens tints, marks, and keys disappear. Always-on chrome survives: the player-identity tile wash and outline, the player's home ring and HQ star, selection outlines, and building/unit markers are not lens-dependent. Pointer clicks revert to resolving the lowest drawn entity (marker, else tile), routing to the Tile Ledger.

**Reason to select.** Return to the unskinned terrain read — when the current lens's tint is obscuring terrain, markers, or colours you need, or when a plain click should select the thing under the pointer rather than the lens's unit of meaning.

### `lens.continent` — Minimap lens bar, slot 8 (the two-interlocking-plates-split-by-a-diagonal-seam glyph)

**Press.** Single left-click on the Continent glyph in the lens bar.

**Valid when:**
- Always pressable; Planetary-only.
- Needs the active body's generation-report plate record (matched by body name); a body with no record gets an honest 'no plate record' key instead of a tint.
- Re-clicking while active clears the lens (see lens.clear).

**Expected output.** Each tectonic plate tints a categorical colour from a dedicated ten-colour earthy table at 0.80 opacity (deliberately not the nation wheel — plates are substrate, not identity). Boundary tiles — any neighbour on another plate — get a separate white lift at 0.45, so plate seams read pale. The key explains that pale tiles are boundaries and reports the plate count; a stagnant-lid body says 'one immobile plate' rather than drawing a meaningless single tint.

**Reason to select.** Why is the land shaped like that? Shows the plates that drifted the terrain into place, and above all where they meet — the seams the mountain ranges, rifts, and boundary-formed deposits came from. Honestly informational/orientational: it explains the map rather than driving an economic decision.

### `lens.corporation` — Minimap lens bar, slot 1 (the seal-square glyph: filled square with centred inner dot)

**Press.** Single left-click on the Corporation glyph in the lens bar under the minimap inset.

**Valid when:**
- The lens bar is always present on the minimap chrome, so the press is always available.
- The lens only re-skins the Planetary canvas; selecting it while on Solar or Circumplanetary changes nothing visible until the player descends to a body surface.
- If Corporation is already the active lens, this same press clears it instead (see lens.clear).

**Expected output.** On the Planetary canvas, every tile holding a corporate building tints to its owning corporation's identity colour (the literal building tile only — no influence radius). The player's tiles additionally get a thin white border. Each rival corporation's HQ-projected reach ring and HQ star draw on that corp's home body in its identity colour. Tiles with no corporate building keep their plain terrain colour — there is no nation underlay. Pointer clicks are NOT lens-dependent: selection resolves the same way under every lens — marker hit-test (building outranks market centre), else the tile under the pointer, with a built tile resolving to its building. The lens changes what is drawn, never what a click selects. No on-canvas colour key yet (glyph highlight + tooltip only).

**Reason to select.** Who owns what: where do rival corporations operate, how does my footprint sit against theirs, and where do their HQ reach rings suggest they will grow? Use it to find uncontested ground to expand into or to size up a rival's holdings before competing.

### `lens.country` — Minimap lens bar, slot 2 (the downward-pointing shield glyph)

**Press.** Single left-click on the Country glyph in the lens bar.

**Valid when:**
- Always pressable from the lens bar; Planetary-only surface — no Solar or Circumplanetary representation.
- Re-clicking while active clears the lens (see lens.clear).

**Expected output.** Claimed tiles tint to their owning nation's identity colour; a dark border stroke draws on every hex edge between different owners (including claimed/unclaimed boundaries), so territories read as filled regions with hard outlines. Unclaimed tiles keep their plain terrain hue. An on-canvas per-nation key (one colour swatch + name per nation on the active body) folds out flush-left of the minimap. Pointer clicks are NOT lens-dependent: selection resolves the same way under every lens — marker hit-test (building outranks market centre), else the tile under the pointer, with a built tile resolving to its building. The lens changes what is drawn, never what a click selects.

**Reason to select.** Which nation holds which tile, and where the borders fall. Political context for siting: whose territory would I be building in, which nations border my operations, and how the body's political map is carved up.

### `lens.good_selector` — The on-canvas lens legend — the key box that folds out flush-left of the minimap while the Resource, Market, or Scarcity lens is active. One shared combo (bound to a single shared lens_resource value), not three separate controls.

**Press.** Open the combo in the lens legend and pick a good from the list.

| Arg | Type | Meaning |
|---|---|---|
| `good` | `resource name` | The good the active lens interrogates: whose deposits fill (Resource), which price line highlights in the Circumplanetary strip (Market), whose shortfall tints the catchments (Scarcity). |

**Valid when:**
- One of the Resource, Market, or Scarcity lenses must be active — the combo only exists inside those lenses' legends.
- This is a cross-cutting selector, exempt from the toggle rule: it switches a target rather than expressing an active state, so re-picking the current good is a no-op, not a clear.

**Expected output.** The active lens's surface re-skins immediately for the newly selected good — new deposit fill (Resource), new highlighted price row (Market), or new shortfall blocks (Scarcity) — and the legend swatch/name update. The lens itself stays active. Because the value is shared, switching lenses afterwards carries the same good across all three.

**Reason to select.** Change the question's subject without changing the question: compare goods on the same surface — where is copper versus iron, which good is this market pricing high, what is each market short of — by flipping the good while the lens holds.

### `lens.industry` — Off the lens bar (trimmed in BL-093 the day it shipped); the factory-silhouette glyph exists but is not on the strip

**Press.** No bar press — reachable only via the keyboard lens-cycle (controls family owns the hotkeys).

**Valid when:**
- Only reachable by keyboard cycle.
- Planetary-only. The economy should have ticked a few times so the nation-substrate injection has settled.
- Cleared by cycling off it or the clear hotkey, not by a bar re-click.

**Expected output.** A sequential dark-to-amber throughput tint: each tile's nation-owned substrate density weighted by its terrain deposit richness, normalised to the body's maximum — brightest where dense background occupation sits on rich terrain. Tiles with no substrate keep plain terrain. Low-to-high amber gradient key. Pure rendering — it changes nothing in the market arithmetic. Pointer clicks fall through to the tile (Tile Ledger); there is no dedicated ledger route.

**Reason to select.** Where does the existing, nation-owned background economy already operate — distinct from where people live (Population) and from player/corp production? Context for competition: the substrate is what injects background supply and demand into each market. Largely informational; it frames the world you are entering rather than pointing at a specific action.

### `lens.market` — Minimap lens bar, slot 4 (the three-ascending-vertical-bars glyph)

**Press.** Single left-click on the Market glyph in the lens bar.

| Arg | Type | Meaning |
|---|---|---|
| `good` | `resource name (optional)` | The good highlighted in the Circumplanetary price strip; set via the shared good selector (lens.good_selector). The Planetary catchment tint itself is per-market, not per-good. |

**Valid when:**
- Always pressable. Surfaces exist on Planetary and Circumplanetary rungs; Solar shows nothing (prices are per-body-market).
- Prices must have resolved (the economy has ticked) for the strip to show meaningful numbers.
- Re-clicking while active clears the lens (see lens.clear).

**Expected output.** Planetary: a catchment tint — one colour per market, so market boundaries read as colour boundaries — with a city-name swatch key and the shared good selector in the legend. Circumplanetary: a compact per-body price strip (good → price list, the selected good highlighted). Terrain shows through the tint. Pointer clicks are NOT lens-dependent: selection resolves the same way under every lens — marker hit-test (building outranks market centre), else the tile under the pointer, with a built tile resolving to its building. The lens changes what is drawn, never what a click selects.

**Reason to select.** Which market does a tile clear against, and where do market boundaries fall? Decide which catchment to build in (your output sells to the nearest centre) and read per-body prices on the Circumplanetary rung to pick where a good is dear enough to sell.

### `lens.opportunity` — Minimap lens bar, slot 6 (the open circle with inner plus glyph)

**Press.** Single left-click on the Opportunity glyph in the lens bar.

**Valid when:**
- Always pressable; Planetary-only.
- The economy must have ticked so market prices have diverged from base — before that the surface reads flat/neutral.
- Re-clicking while active clears the lens (see lens.clear).

**Expected output.** Per-tile red-to-green value marks (same mark idiom as Population), keyed to each tile's catchment market's demand-gap rank — a body-relative ranking on gap × volume — unmet demand quantity weighted by traded volume, not a price-above-base measure. All tiles of one catchment read uniformly (the market is the unit), so the surface shows blocks per catchment. Tiles with no catchment market keep plain terrain. Key is the red-to-green rank bar.

**Reason to select.** Where is demand going unmet, so the market will pay a premium to whoever supplies it? The forward-looking siting lens: green catchments are markets bidding above base — build or route supply there. Reads potential; the Production lens reads what is realised.

### `lens.population` — Minimap lens bar, slot 5 (the small figure glyph: round head over tapered torso)

**Press.** Single left-click on the Population glyph in the lens bar.

**Valid when:**
- Always pressable; Planetary-only. No selector — a whole-body surface with no per-good pick.
- Re-clicking while active clears the lens (see lens.clear).

**Expected output.** Every buildable tile gets a small per-tile red-to-green value mark keyed by workforce efficiency — the same habitability-to-labour curve the economy applies (full 1.0x efficiency at habitability >= 0.6, ramping down to 0.5x at habitability 0). Tiles keep their terrain hue; the marks carry the signal. A gradient key labelled 'Workforce efficiency' labels the bar's ends 'low' and 'high' (the underlying curve spans 0.5x to 1.0x, but the key does not print the numbers). Note: this is the labour consequence, not raw habitability, and not population density (that lives on the population-centre markers).

**Reason to select.** Where does labour run at full efficiency? Site buildings where the marks read green (habitability >= 0.6 = full workforce), because the same wages buy less output on the red end. The siting complement to Resource's material read.

### `lens.production` — Minimap lens bar, slot 7 (the filled upward triangle over a baseline glyph)

**Press.** Single left-click on the Production glyph in the lens bar.

**Valid when:**
- Always pressable; Planetary today (a Circumplanetary per-body output badge is specified but owed).
- The economy must have ticked so buildings have produced and the economy report is populated — otherwise everything reads cold.
- Re-clicking while active clears the lens (see lens.clear).

**Expected output.** Each producing tile tints on the production ramp (production_colour: red → yellow → green) by its output value this tick (sum of output quantity x resolved price) relative to the body's producing-tile geometric mean, composited at 0.6 over terrain. Above-mean producers read green, below-mean red; idle, exhausted, or unbuilt tiles produce nothing and stay untinted. Honest caveat: a body of similar producers reads near-neutral — little spread to show. Low-to-high key. Pointer clicks are NOT lens-dependent: selection resolves the same way under every lens — marker hit-test (building outranks market centre), else the tile under the pointer, with a built tile resolving to its building. The lens changes what is drawn, never what a click selects.

**Reason to select.** Where is value actually being made right now? The realised-output counterpart to Opportunity's potential: spot the hot producers worth expanding and the cold ones worth investigating or idling.

### `lens.reach` — Off the lens bar; currently reuses the convoy glyph (a dedicated glyph is an open TODO)

**Press.** No bar press — reachable only via the keyboard lens-cycle (controls family owns the hotkeys).

**Valid when:**
- Only reachable by keyboard cycle.
- Planetary key today; the specified Solar connected-body glow is owed.
- Shows the player's own trade routes only (competitor-visibility rule — rival lanes stay private).
- Cleared by cycling off it or the clear hotkey.

**Expected output.** No tile re-skin. A connection-list key headed 'Reach (your trade network)' folds out flush-left of the minimap: one row per body the active body is routed to, name plus a recency dot — fresh routes green, gone-cold routes grey (the activity-fog colour convention). An unrouted body honestly reads 'no routes from this body'.

**Reason to select.** Which bodies does my commercial network actually touch from here, and which links have gone cold? The health check on your persistent trade network — a greying link is a market going stale in your activity fog.

### `lens.resource` — Minimap lens bar, slot 3 (the three-stacked-strata glyph)

**Press.** Single left-click on the Resource glyph in the lens bar.

| Arg | Type | Meaning |
|---|---|---|
| `resource` | `resource name (optional)` | The good whose deposits the lens fills. Set via the shared good selector in the on-canvas legend (lens.good_selector), not on this press; the lens opens showing the currently-set shared lens_resource. |

**Valid when:**
- Always pressable; Planetary-only. No simulation dependency — deposit data exists from tile generation, so it works from turn one.
- Re-clicking while active clears the lens (see lens.clear).

**Expected output.** Every tile carrying any deposit of the selected good (deposit > 0) fills flat and uniform with that resource's identity colour at fixed 0.8 opacity — the shape of the contiguous deposit, not a magnitude gradient. Tiles without the good keep their terrain hue. The on-canvas key (flush-left of the minimap) shows the selected resource's swatch + name, the note 'filled = deposit present', and hosts the shared good selector. Deposit magnitude lives in tile detail, not this surface. Pointer clicks are NOT lens-dependent: selection resolves the same way under every lens — marker hit-test (building outranks market centre), else the tile under the pointer, with a built tile resolving to its building. The lens changes what is drawn, never what a click selects.

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

**Expected output.** A market-level field, not per-tile: every tile in a market's catchment reads as one solid block, composited toward a hot red hue at opacity proportional to that market's supply shortfall of the selected good (max(0, demand - supply), normalised to the body's worst market). A met market keeps plain terrain; a short one reads hot. With one market per body the whole body is a single block — honest to the market structure. Abundant-to-scarce key plus the selected resource's swatch and the shared selector. Pointer clicks are NOT lens-dependent: selection resolves the same way under every lens — marker hit-test (building outranks market centre), else the tile under the pointer, with a built tile resolving to its building. The lens changes what is drawn, never what a click selects.

**Reason to select.** Where did demand outrun supply for a chosen good last tick? The inverse of the Resource lens — gaps, not concentrations. Pick a good you can produce and find the hot markets: that is where to sell into or build supply for.

### `lens.supply` — Off the lens bar; the two-parallel-horizontal-lines convoy glyph exists but is not on the strip

**Press.** No bar press — reachable only via the keyboard lens-cycle (controls family owns the hotkeys).

**Valid when:**
- Only reachable by keyboard cycle.
- The one genuinely multi-rung lens: surfaces on all three canvases.
- Shows player convoys only; nothing renders if no player convoy is in transit.
- Cleared by cycling off it or the clear hotkey.

**Expected output.** Solar: a route line per player convoy currently in transit between bodies. Circumplanetary: a convoy-count badge beside each body's label. Planetary: a convoy glyph on the active body's tiles while a player convoy touches them. Lines and badges use a single neutral logistics hue — flow, not ownership. Tiles are not re-tinted; supply annotates, it does not re-skin terrain. The throughput scale-key is still owed.

**Reason to select.** Where are my goods moving right now? Verify dispatched convoys are actually in flight and see the live shape of your logistics — the in-motion read; the standing lanes they carve belong to the Supply-routes lens.

### `lens.supply_routes` — Off the lens bar; reuses the supply glyph

**Press.** Cycle lenses with L (forward) / Shift+L (backward) until Supply-routes is active — it is the last mode in the cycle. No lens-bar slot.

**Valid when:**
- In-game on a canvas (the lens-cycle keys are live).
- Off the bar: reachable only via the keyboard lens cycle. (A 2026-07-31 doc note claimed the cycle could not reach this lens; that was stale — canvas_command.cpp anchors overlay_mode_count to supply_routes+1 with a static_assert, so the cycle covers all 14 modes.)
- Planetary key only (the specified Solar aggregated-graph render is owed); player routes only.

**Expected output.** No tile re-skin. A lane-list key: one row per standing trade lane touching the active body (one entry per body pair), with a log-scaled thickness bar from that lane's cumulative convoy count (a single completion reads as a thin sliver; heavy repeat traffic saturates rather than growing linearly) and the same recency-tier colouring as Reach.

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

---

## Chrome — startup, the system menu, settings, F-keys

### `chrome.esc` — Keyboard, in-game only

**Press.** Press Esc.

**Valid when:**
- App is in game. On the main menu and the wizard, Esc does nothing (the key handler returns before Esc handling on those screens).
- Works even while an ImGui panel holds keyboard focus — Esc is handled before the ImGui keyboard guard.

**Expected output.** Exactly one rung of this precedence ladder fires, highest first: (1) an armed exit-confirm backs out (Really quit? disarms); (2) an open system menu closes; (3) if the sticky selection card is open and has a drill stack, one drill level unwinds; (4) if the corporation roll-up is drilled into a constituent, it returns to the roll-up; (5) if a fold overlay is expanded full-screen, it folds up; (6) an open sticky card hides (hidden for this selection, not destroyed); (7) otherwise the system menu opens. One press never does two of these — e.g. Esc reaches the menu only once the card is fully closed.

**Reason to select.** The universal step-out key: back out of a confirmation, close the menu, unwind a drill, fold an overlay, dismiss the card — or, with nothing open, reach the session menu.

### `chrome.f1_help` — Keyboard; opens a centred Key Bindings window

**Press.** Press F1 (press again, or click the window's X, to close).

**Valid when:**
- App is in game (the binding table is gated off on the menu and wizard).
- Not while an ImGui widget owns the keyboard (typing in a text field suppresses it).

**Expected output.** Toggles a two-column Action/Key cheat-sheet, generated from the same binding table the key handler loops over, so it can never drift from the real bindings. F11 and F12 sit outside that table and are appended to the sheet by hand.

**Reason to select.** To look up every keyboard shortcut without leaving the game.

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

### `chrome.f9_tech_tree` — Keyboard; opens the mock tech-tree viewer

**Press.** Press F9 (press again to close).

**Valid when:**
- App is in game.
- Not while an ImGui widget owns the keyboard.

**Expected output.** Toggles a read-only viewer over scripts/tech_tree.lua, tabbed by era: Era -1 Antiquity (placeholder pointing at the BL-307 ladder store), Era 0, Era 1, and Standing lines; eras with no authored quests show a placeholder. Honest status: this is a mock — a design aid with no simulation coupling; nothing can be researched and nothing in the world reads it.

**Reason to select.** To browse the drafted tech-tree content; of no strategic use to an AI player yet.

### `chrome.tech_tree_era_tab` — Tech-tree viewer (F9), era tab strip

**Press.** Click the 'Era -1 Antiquity', 'Era 0 — Terrestrial', 'Era 1 — Early Space' or 'Standing lines' tab button

| Arg | Type | Meaning |
|---|---|---|
| `view` | `enum` | 'Era -1 Antiquity' (placeholder pointing at the BL-307 ladder store) / 'Era 0 — Terrestrial' (the default) / 'Era 1 — Early Space' / 'Standing lines' (span eras, never gate one) |

**Valid when:**
- Tech-tree viewer is open (F9)

**Expected output.** Switches which era's tree is shown — each era carries its own tree; eras with no authored quests show a placeholder. Re-clicking the currently-active tab closes the viewer (toggle rule); switching tabs is an ordinary view change.

**Reason to select.** To browse a specific era's drafted content; of no strategic use to an AI player yet — the viewer is a mock with no simulation coupling.

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

