-- The UI-pass fixture: build a campaign worth LOOKING at, and save it.
--
-- WHY THIS EXISTS. A design pass over the ubiquitous shell surfaces (header,
-- time panel, profile tile, nav rail, comms dock, selection band, minimap, and
-- each of the thirteen rail ledgers) needs a world where every one of those
-- surfaces has something to show. A freshly generated world does not qualify:
-- the balance is 0.0 with no trend behind it, the Contracts ledger is empty in
-- all three views, and the comms dock holds nothing but the epoch line. Captures
-- taken there record what the surfaces look like EMPTY, which is the one state a
-- layout review must not be conducted in.
--
-- So this script stages the state once and writes it to a snapshot
-- (`verify.save`, BL-536). `shell_pass.lua` then opens it with `verify.load`,
-- which is both faster and — more to the point — the SAME world on every run, so
-- two captures taken a day apart are comparable.
--
-- STALENESS, stated plainly. The snapshot is a cache, and a cache of a generated
-- world goes stale when generation changes. Two things guard it, neither
-- perfectly:
--   * a format change bumps `world_save_version`, and the load is REJECTED
--     rather than reinterpreted — so a struct-layout drift rebuilds the fixture.
--   * a generation change that bumps no format leaves the snapshot loadable and
--     WRONG. Every run therefore prints its `state_hash`; if it has moved, the
--     fixture predates a world change and should be deleted.
-- The file lives under `build_gen/` (gitignored) precisely because it is a
-- rebuildable cache, not an asset.
--
-- Run: ProjectIo --verify scripts/verify/ui_shell_fixture.lua

verify.window(1280, 720)

local FIXTURE = "build_gen/verify/ui_shell_fixture.iosave"
local VERB_ACCEPT_OFFER = 25 -- corp_verb::accept_offer (contracts_ledger.lua)

verify.goto_surface("home")

-- ---------------------------------------------------------------------------
-- 1. Let the economy run, so every derived read has a history behind it.
-- ---------------------------------------------------------------------------
-- The header's NET figure and sparkline, the Budget ledger's income/spend split,
-- the Corporation dashboard's balance column and the market price series are all
-- functions of elapsed ticks. Twelve is enough for a trend to have a shape
-- without being so long that the opening economy has been overtaken by it.
verify.econ_step(12)
print("FIXTURE balance_after_12=" .. tostring(verify.player_balance()))

-- ---------------------------------------------------------------------------
-- 2. Give the Contracts ledger content in ALL THREE views.
-- ---------------------------------------------------------------------------
-- Offers / Active / History are three tabs of one ledger, and a capture of two
-- populated tabs beside an empty third says nothing about whether the third
-- LAYS OUT. Offers come from a nation-budget pass with no corp verb of its own,
-- so they are seeded through the test-only `inject_offer` hook exactly as
-- contracts_ledger.lua does; everything downstream of the offer existing runs
-- through the real `accept_offer` seam and the real tick evaluation.
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

local home_province = verify.select_province(mine.col, mine.row)
assert(home_province ~= 0, "the player's own unit stands on an unpartitioned tile")
verify.clear_selection()

-- (a) HISTORY — an offer with a one-tick deadline, accepted for real, then
--     stepped past its deadline so the tick pass settles it into a terminal
--     state. This is the only one of the three that costs a tick.
local terminal_offer = verify.inject_offer{ province = home_province, fee = 400, deadline_in = 1 }
assert(terminal_offer ~= 0, "inject_offer could not resolve a client nation")
local client_of = function(offer_id)
    for _, o in ipairs(verify.offers()) do
        if o.id == offer_id then return o.client end
    end
    return nil
end
local r1 = verify.corp_command{
    verb = VERB_ACCEPT_OFFER, corp = player_corp, order = terminal_offer,
    counterparty = client_of(terminal_offer), units = { mine.id },
}
print("FIXTURE accept_terminal=" .. tostring(r1))
verify.econ_step(2)

-- (b) ACTIVE — a second offer on a long deadline, accepted and left running.
local active_offer = verify.inject_offer{ province = home_province, fee = 900, deadline_in = 400 }
local r2 = verify.corp_command{
    verb = VERB_ACCEPT_OFFER, corp = player_corp, order = active_offer,
    counterparty = client_of(active_offer), units = { mine.id },
}
print("FIXTURE accept_active=" .. tostring(r2))

-- (c) OFFERS — a third, left open for the player to consider. Deliberately a
--     different fee and deadline from the two above so the Offers table's own
--     columns are distinguishable rather than three copies of one row.
local open_a = verify.inject_offer{ province = home_province, fee = 650, deadline_in = 240 }
local open_b = verify.inject_offer{ province = home_province, fee = 1250, deadline_in = 90 }
print("FIXTURE open_offers=" .. tostring(open_a) .. "," .. tostring(open_b))

local contracts = verify.contracts()
print("FIXTURE contract_rows=" .. tostring(#contracts))
for _, c in ipairs(contracts) do
    print("FIXTURE   contract id=" .. tostring(c.id) .. " state=" .. tostring(c.state)
          .. " fee=" .. tostring(c.fee))
end
print("FIXTURE open_offer_rows=" .. tostring(#verify.offers()))

-- ---------------------------------------------------------------------------
-- 3. Park the view somewhere legible, and close whatever the steps opened.
-- ---------------------------------------------------------------------------
-- The view slice IS in the envelope, so what is set here is what a load opens
-- on. `econ_step` opens the Economy panel as a side effect (decision_feed.lua
-- records the same), so close it rather than baking one open ledger into every
-- future capture.
verify.show_panel("economy", false)
verify.show_panel("contracts", false)
verify.clear_selection()
verify.set_overlay("none")
verify.frames(3)

-- Deliberately NOT re-framed. The opening pan/zoom `goto_surface("home")` lands
-- on is the framing a player actually meets, and it shows populated land; an
-- earlier draft centred on the player's own unit at 1.6x and filled the canvas
-- with unlit ocean — a worse picture of the same world, and the wrong one to
-- review a shell against. A capture script that wants a different framing sets
-- its own after loading; the fixture holds the default.

print("FIXTURE state_hash=" .. verify.state_hash())
verify.expect(verify.save(FIXTURE), "fixture written to " .. FIXTURE)

-- One capture, so a run of this script is self-evidencing: if the fixture is
-- staged wrong, it is visible here rather than a puzzle in the pass that loads it.
verify.capture("ui_shell_fixture_state")
