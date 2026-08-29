-- Verify the Contracts ledger (BL-576): Offers / Active / History.
--
-- THE HARD PART, as with battle_card.lua's own fight, is getting a real
-- instance to exist at all. There is no player-reachable verb that CREATES a
-- mercenary_offer — offers come from `derive_contract_offers`, a nation-
-- budget-side pass with no corp_verb of its own — so this script seeds one
-- through the TEST-ONLY `verify.inject_offer` fixture hook (verify_api.cpp)
-- rather than waiting on the nation AI to happen to fund one. Everything
-- AFTER that point runs through the real seam: `verify.corp_command` drives
-- the actual `accept_offer` verb, and the real tick-evaluation pass
-- (`run_mercenary_contract_tick`) is what settles the contract for History —
-- nothing about the CONSEQUENCES of accepting is faked, only the offer's
-- existence.

local VERB_ACCEPT_OFFER = 25

verify.goto_surface("home")

-- Who is the player, and where does their own unit stand? (BL-331 seeds one
-- on the home body.) Same buildings()/units() reader idiom battle_card.lua
-- uses to avoid assuming a generation-dependent id.
local player_corp = nil
for _, b in ipairs(verify.buildings()) do
    if b.player then player_corp = b.corp; break end
end
assert(player_corp, "no player-owned building found")

local mine = nil
for _, u in ipairs(verify.units()) do
    if u.owner == player_corp then mine = u; break end
end
assert(mine, "the player has no unit on the surface (BL-331 seeds one)")

local target_province = verify.select_province(mine.col, mine.row)
assert(target_province ~= 0, "the player's own unit stands on an unpartitioned tile")
verify.clear_selection()

-- Seed an offer against that province — a short deadline (1 tick) so a
-- single econ_step below both settles it AND reaches a terminal state for
-- the History view, rather than needing to fast-forward for tens of ticks.
local offer_id = verify.inject_offer{ province = target_province, fee = 400, deadline_in = 1 }
assert(offer_id ~= 0, "inject_offer could not resolve a client nation for that province")

-- show_panel writes ui_state DIRECTLY rather than routing through
-- close_all_panels (decision_feed.lua's own R5 note explains why), so the
-- column can otherwise hold two open windows at once. Close it explicitly —
-- the same fix decision_feed.lua applies to itself.
verify.show_panel("contracts", true)
verify.panel_view("contracts", 0) -- Offers

-- R6 (the fog rule): the injected offer's target is the player's OWN unit's
-- tile, which is visible/known by construction (the home body is always
-- `visible` under the activity fog), so the Offers view is a non-vacuous
-- capture rather than an accidentally-empty one.
local offers = verify.offers()
assert(#offers >= 1, "the injected offer did not appear in verify.offers()")
local client = nil
for _, o in ipairs(offers) do
    if o.id == offer_id then client = o.client; break end
end
assert(client, "could not find the injected offer's client")

verify.capture("contracts_ledger_offers")

-- Accept it for real, through the actual corp_command seam — the SAME path
-- the Offers view's force-picker Confirm press uses.
local result = verify.corp_command{
    verb = VERB_ACCEPT_OFFER,
    corp = player_corp,
    order = offer_id,
    counterparty = client,
    units = { mine.id },
}
assert(result == "applied", "accept_offer did not apply: " .. tostring(result))

verify.panel_view("contracts", 1) -- Active
verify.capture("contracts_ledger_active")

-- The 1-tick deadline is reached within a couple of econ_step calls —
-- deliberately not asserting an exact step count against the internal
-- warm-start/econ-counter bookkeeping (BL-568's cadence key), just that the
-- REAL tick-evaluation pass (run_mercenary_contract_tick) settles it. The
-- target province is the PLAYER's own unit's tile, so `province_held`
-- reads true and the "take" contract COMPLETES — a real terminal state,
-- not a chosen one; a script wanting a "failed" capture would target a
-- province the player does not hold instead.
local settled = nil
for _ = 1, 5 do
    verify.econ_step(1)
    for _, c in ipairs(verify.contracts()) do
        if c.state ~= "active" then settled = c; break end
    end
    if settled then break end
end
assert(settled, "the contract did not reach a terminal state within 5 ticks of its deadline")

verify.panel_view("contracts", 2) -- History
verify.capture("contracts_ledger_history")

-- NO GOLDEN — same reasoning as decision_feed.lua: this is a capture-only
-- check, on demand, when someone touches the ledger. The fixture depends on
-- which real nation ends up owning the player's home-body province and on
-- corp/unit ids that are generation-dependent, so a byte golden would go red
-- on unrelated generation or nation-assignment changes.
