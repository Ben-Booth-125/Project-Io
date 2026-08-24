-- Verify BL-578 — the mercenary vertical slice plays end-to-end (Sprint 16).
--
-- Six captures from a NEW world: the ledger with an offer, the force picker,
-- a unit marching, the battle card, a completed contract, and Balance with
-- contract income. Every step after the offer's own existence runs through
-- the real seam (verify.corp_command / a real march / real combat), the same
-- discipline contracts_ledger.lua and battle_card.lua established: only the
-- OFFER is faked (verify.inject_offer — there is still no player-facing verb
-- that creates one, offers come from derive_contract_offers on the
-- nation-budget side), never the consequences of accepting it.
--
-- TWO DELIBERATE SUBSTITUTIONS, both investigated live in this session and
-- both real, documented gaps — not corners cut:
--
-- 1) CAPTURE 4 SHOWS THE MOMENT OF CONTACT, not the battle card's own
--    in-progress modal. Investigated at length: a scripted check can only
--    observe world state BETWEEN whole econ ticks, but this build's combat
--    (campaign_battle_params::rounds_per_tick == 3) resolved EVERY
--    encounter tried in this session — the contract's own garrison fight
--    AND a from-scratch, evenly-matched corp-vs-corp fight built with
--    battle_card.lua's own proven declare_hostile+march+step technique —
--    fully within the SAME econ_step call it opened in, every single tick,
--    for as long as the standoff continued. `verify.select_battle` never
--    once returned true across several hundred ticks of trying either
--    encounter. Field-channel dispatch text ("holds the field after 2
--    rounds") and the casualty count are the only observable evidence a
--    fight happened. Whether this is this session's own bad luck on two
--    matchups or a real pacing/discovery-window issue worth a fix is
--    exactly the kind of question BL-578's live playthrough exists to
--    surface — flagged for Ben as its own NEEDS_REVIEW entry, not resolved
--    here. The capture below shows the field dispatch line and the
--    resulting casualties/garrison count instead of the modal card.
--
-- 2) "A COMPLETED CONTRACT" IS CAPTURED VIA THE LEDGER'S HISTORY VIEW, not
--    the contract card. BL-577's card (selection_kind::contract) has no live
--    selection trigger anywhere in the UI (Wave 5's own finding, NR-585's
--    sibling) — wiring one is out of this item's scope, so History is the
--    reachable substitute for "a completed contract" on screen.
--
-- Run: ProjectIo --verify scripts/verify/mercenary_slice.lua [--bless]

local VERB_MARCH_UNIT   = 21
local VERB_ACCEPT_OFFER = 25

verify.goto_surface("home")

local player_corp = nil
for _, b in ipairs(verify.buildings()) do
    if b.player then player_corp = b.corp; break end
end
assert(player_corp, "mercenary_slice: no player-owned building found")

local contract_unit = nil
for _, u in ipairs(verify.units()) do
    if u.owner == player_corp then contract_unit = u; break end
end
assert(contract_unit, "mercenary_slice: the player has no unit on the surface (BL-331 seeds one)")

-- The contract's own target: the nearest nation garrison (BL-571) — "the
-- thing the hired company actually fights is the OPPOSING nation's own
-- force" (CONTRACTS.md § Where offers come from). count == 20 is BL-571's
-- seeded garrison size, distinct from a corp's own starting count.
local garrison, garrison_d = nil, math.huge
for _, u in ipairs(verify.units()) do
    if u.owner ~= player_corp and u.count == 20 then
        local d = math.abs(u.col - contract_unit.col) + math.abs(u.row - contract_unit.row)
        if d < garrison_d then garrison_d = d; garrison = u end
    end
end
assert(garrison, "mercenary_slice: no nation garrison found on the surface")

local target_province = verify.select_province(garrison.col, garrison.row)
assert(target_province ~= 0, "mercenary_slice: the garrison stands on an unpartitioned tile")
verify.clear_selection()

-- 1) The ledger with an offer. deadline_in is short — the "take" predicate
-- (nation_step.cpp's run_mercenary_contract_tick) is evaluated only AT the
-- deadline, not continuously, so this must clear the march+fight window
-- (a handful of ticks, given the garrison stands right at the border) with
-- margin, not the long fuse a real nation-funded offer would carry.
local offer_id = verify.inject_offer{ province = target_province, fee = 600, deadline_in = 25 }
assert(offer_id ~= 0, "mercenary_slice: inject_offer could not resolve a client nation")

verify.show_panel("economy", false)
verify.show_panel("contracts", true)
verify.panel_view("contracts", 0) -- Offers
verify.capture("mercenary_slice_01_offer")

-- 2) The force picker — the SAME popup the Offers row's "Accept" button
-- opens, pressed for real (verify.click), not faked by writing ui_state
-- directly. Single-offer, single-row layout: the button's screen position is
-- stable for this capture the way COL,ROW is stable in click_injection.lua.
verify.click(91, 229)
verify.frames(2)
verify.capture("mercenary_slice_02_force_picker")

-- Confirm through the REAL seam (what the picker's own Confirm press calls).
local client = 0
for _, o in ipairs(verify.offers()) do
    if o.id == offer_id then client = o.client; break end
end
assert(client ~= 0, "mercenary_slice: could not find the injected offer's client")
local accept_result = verify.corp_command{
    verb = VERB_ACCEPT_OFFER, corp = player_corp, order = offer_id,
    counterparty = client, units = { contract_unit.id },
}
assert(accept_result == "applied", "mercenary_slice: accept_offer did not apply: " .. tostring(accept_result))
verify.show_panel("contracts", false)

-- 3) A unit marching. Issue the real march order toward the contract's own
-- target province, then capture mid-transit -- zoomed and centred on the
-- unit's own tile (BL-511's unit rung), and selected, so the Selection
-- element's own march/order readout is on screen as evidence of motion, not
-- just a wide shot the unit marker is too small to read in.
verify.corp_command{ verb = VERB_MARCH_UNIT, corp = player_corp,
                     subject = contract_unit.id, province = target_province }
verify.econ_step(1)
verify.show_panel("economy", false) -- econ_step's own side effect (contracts_ledger.lua's own note)
local at_col, at_row = contract_unit.col, contract_unit.row
for _, u in ipairs(verify.units()) do
    if u.id == contract_unit.id then at_col, at_row = u.col, u.row end
end
verify.center_tile(at_col, at_row, 8.0)
verify.frames(2)
verify.click_tile(at_col, at_row) -- real click: rung 0 is the unit (BL-511)
verify.frames(2)
verify.capture("mercenary_slice_03_marching")
verify.clear_selection()

-- 4) The moment of contact — see the SUBSTITUTION note above. Step until the
-- garrison takes its first casualties (the fight is, by construction, the
-- contract's own — the company engaging the client's opposing nation), then
-- capture the frame plus the Field-channel dispatch line as the evidence a
-- modal battle card cannot supply in this build.
local contact_tick = nil
for i = 1, 30 do
    verify.econ_step(1)
    for _, u in ipairs(verify.units()) do
        if u.id == garrison.id and u.count < 20 then contact_tick = i; break end
    end
    if contact_tick then break end
end
assert(contact_tick, "mercenary_slice: the garrison took no casualties within 30 ticks of contact")
verify.show_panel("economy", false)
verify.center_tile(garrison.col, garrison.row, 8.0)
verify.frames(2)
verify.capture("mercenary_slice_04_battle_card")

local field_line = nil
for _, l in ipairs(verify.chat_lines()) do
    if l:match("^1:") then field_line = l end -- last Field-channel line wins: freshest
end
print("mercenary_slice: field dispatch -- " .. tostring(field_line))

-- 5) A completed contract (History view — see the KNOWN GAP note above) and
-- 6) Balance with contract income. Step to and past the offer's own
-- deadline_in=25 tick fuse; the garrison fight (unlike the rival-corp fight
-- above) resolves same-tick against a floor-sized garrison, so the province
-- is held with margin well before the deadline settles the contract.
local settled = nil
for _ = 1, 60 do
    verify.econ_step(1)
    for _, c in ipairs(verify.contracts()) do
        if c.province == target_province and c.state ~= "active" then settled = c; break end
    end
    if settled then break end
end
assert(settled, "mercenary_slice: the contract did not reach a terminal state within budget")
assert(settled.state == "completed",
       "mercenary_slice: the contract's terminal state was '" .. settled.state ..
       "', not 'completed' -- see the capture for evidence")

verify.show_panel("economy", false)
verify.show_panel("contracts", true)
verify.panel_view("contracts", 2) -- History
verify.capture("mercenary_slice_05_completed_contract")

-- The Balance ledger's own "Contract income" line (balance_ledger.cpp,
-- BL-577) reads THIS TICK's budget_result::subsidies -- captured with no
-- further econ_step between settlement and this shot, or the line reads
-- zero again next tick.
verify.show_panel("contracts", false)
verify.show_panel("balance", true)
verify.scroll_panel("balance", 1.0) -- "Contract income" sits under Assets, below the fold
verify.frames(2)
verify.capture("mercenary_slice_06_balance_income")

print(string.format("mercenary_slice: OK -- offer %d, province %d, contract state=%s",
                    offer_id, target_province, settled.state))
