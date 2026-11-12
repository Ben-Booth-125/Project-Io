-- BL-1609 (fleet UI): space fleets drawn on the Circumplanetary and Solar rungs,
-- selected and ordered from the Selection band (move with its delta-v preview,
-- dock, refuel, bombard), debris drawn per node, and the Military ledger's Design
-- tab gaining space hulls (docs/ui/CIRCUMPLANETARY.md, SOLAR.md, SELECTION.md
-- § The space-unit element, docs/military/SPACE_FLEETS.md).
--
-- verify.space_fleet_fixture() stages, straight into the stores (no raise verb:
-- the pad, the shelf and the keys are not what the canvas check is about): a
-- player Frigate design; two player Frigates idle in the home low orbit, tanks
-- full; a player Frigate in transit from the home low orbit to the first moon's
-- low orbit, a third of the way; a rival Frigate idle in the home high orbit; and
-- debris at the home high orbit. Returns what it staged and nothing else.
--
-- Frames: the rung at rest, a fleet hovered, the fleet selected with Move armed
-- and a destination picked (the preview), after the move is confirmed, the Solar
-- rung, and the Design tab's Space theatre with a hull open.

verify.goto_surface("home")
verify.command("ascend")
verify.frames(2)

local staged = verify.space_fleet_fixture()
print(string.format("space_fleets_ui: staged %d units (%d in transit), debris %s",
    staged.units or 0, staged.in_transit or 0, tostring(staged.debris)))
verify.expect((staged.units or 0) >= 3, "the fixture staged space units")

verify.mouse(-1, -1)
verify.frames(2)
verify.capture("space_fleets_rest")

-- ── The fleets drawn ──
local fleets = verify.circum_fleets()
print(string.format("space_fleets_ui: %d fleet marks", #fleets))
local mine, rival, moving = nil, nil, nil
local me = staged.player or 0
for _, f in ipairs(fleets) do
    print(string.format("  lead=%-6d owner=%-6d %-8s x%d at %-24s (%.1f,%.1f) r=%.1f%s share=%.2f",
        f.lead, f.owner, f.hull, f.count, f.node, f.x, f.y, f.r, f.in_transit and " transit" or "", f.share))
    if f.in_transit then moving = f
    elseif f.owner == me then mine = f
    else rival = f end
end
verify.expect(mine ~= nil and mine.count == 2, "the player's two idle Frigates draw as one fleet of two")
verify.expect(rival ~= nil, "the rival's Frigate draws as its own fleet")
verify.expect(moving ~= nil, "the travelling Frigate draws on its segment")
if moving ~= nil then
    verify.expect(moving.share > 0.05 and moving.share < 0.95, "the travelling mark sits part-way along its segment")
end

-- ── Debris ──
local deb = verify.circum_debris()
print(string.format("space_fleets_ui: %d debris marks", #deb))
for _, d in ipairs(deb) do print(string.format("  debris at %s level %.1f (%.1f,%.1f)", d.node, d.level, d.x, d.y)) end
if staged.debris then verify.expect(#deb >= 1, "the staged debris draws at its node") end

-- ── Storm marks: derived from the seed's quarter, so only reported ──
for _, a in ipairs(verify.circum_assets()) do
    if a.storm then print("space_fleets_ui: storm-struck " .. a.kind .. " " .. a.id) end
end

if mine == nil then return end

-- ── Hover, then select, the player's fleet ──
verify.hover(mine.x, mine.y, 3)
verify.expect(verify.circum_fleet_hovered() == mine.lead, "the fleet answers the hover")
verify.capture("space_fleets_hover")
verify.click(mine.x, mine.y)
verify.frames(2)
verify.expect(verify.circum_selected() == mine.lead, "a press on the fleet selects its lead unit")
verify.mouse(-1, -1)
verify.frames(2)
verify.capture("space_fleets_selected")

local card = verify.fleet_card()
local press = {}
for _, p in ipairs(card.presses) do
    press[p.name] = p
    print(string.format("  press %-12s (%.0f,%.0f) %s %s", p.name, p.x, p.y, p.enabled and "on" or "off", p.reason))
end
verify.expect(press.move ~= nil and press.dock ~= nil and press.refuel ~= nil and press.bombard ~= nil,
    "the card carries Move, Dock, Refuel and Bombard")
if press.dock ~= nil then verify.expect(not press.dock.enabled, "Dock is off with no station of the player's here") end

-- ── Move: armed, a destination picked, the preview, confirmed ──
verify.click(press.move.x, press.move.y)
verify.frames(2)
card = verify.fleet_card()
print(string.format("space_fleets_ui: Move armed, %d destinations", #card.dest_rows))
verify.expect(card.move_armed and #card.dest_rows > 0, "Move lists destinations")
local pick = nil
for _, r in ipairs(card.dest_rows) do
    if r.affordable and (pick == nil or r.dv < pick.dv) then pick = r end
end
verify.expect(pick ~= nil, "a destination the budget can pay is listed")
if pick == nil then return end
verify.click(pick.x, pick.y)
verify.frames(2)
card = verify.fleet_card()
print(string.format("space_fleets_ui: picked %s, preview dv %.1f of %.1f", pick.node, card.preview_dv, card.preview_budget))
verify.expect(card.preview_dv > 0 and card.preview_budget >= card.preview_dv,
    "the preview shows the move's delta-v against the budget before confirming")
verify.capture("space_fleets_move_preview")

-- Toggle: re-pressing Move disarms it.
for _, p in ipairs(card.presses) do press[p.name] = p end
verify.click(press.move.x, press.move.y)
verify.frames(2)
card = verify.fleet_card()
verify.expect(not card.move_armed and #card.dest_rows == 0, "re-pressing the lit Move disarms it (the toggle rule)")

-- Arm again, pick, confirm.
for _, p in ipairs(card.presses) do press[p.name] = p end
verify.click(press.move.x, press.move.y)
verify.frames(2)
card = verify.fleet_card()
for _, r in ipairs(card.dest_rows) do if r.node == pick.node then pick = r end end
verify.click(pick.x, pick.y)
verify.frames(2)
card = verify.fleet_card()
for _, p in ipairs(card.presses) do press[p.name] = p end
verify.expect(press.confirm_move ~= nil and press.confirm_move.enabled, "Confirm move is offered")
if press.confirm_move ~= nil then
    verify.click(press.confirm_move.x, press.confirm_move.y)
    verify.frames(3)
end
card = verify.fleet_card()
print("space_fleets_ui: after confirm: " .. (card.message or ""))
local after = verify.circum_fleets()
local transit_marks = 0
for _, f in ipairs(after) do if f.in_transit then transit_marks = transit_marks + 1 end end
verify.expect(transit_marks >= 2, "the confirmed move puts the unit in transit (a second travelling mark)")
verify.mouse(-1, -1)
verify.frames(2)
verify.capture("space_fleets_moved")

-- ── Solar rung ──
verify.command("ascend")
verify.frames(3)
local solar = verify.solar_fleets()
print(string.format("space_fleets_ui: %d solar fleet marks", #solar))
for _, f in ipairs(solar) do
    print(string.format("  solar lead=%-6d owner=%-6d %s x%d at %s%s", f.lead, f.owner, f.hull, f.count, f.node,
        f.in_transit and " transit" or ""))
end
verify.expect(#solar >= 1, "the Solar rung draws the fleets at their body")
verify.capture("space_fleets_solar")

-- ── The Military ledger's Design tab: the Space theatre ──
local sx, sy = verify.nav_slot(7) -- the Military ledger's rail slot (military_ledger.lua)
if sx then
    verify.click(sx, sy)
    verify.frames(3)
end
local led = verify.military_ledger()
if led.open and led.tab_design_x > 0 then
    verify.click(led.tab_design_x, led.tab_design_y)
    verify.frames(2)
    led = verify.military_ledger()
    local space_tab = nil
    for _, t in ipairs(led.theatre_tabs) do if t.tab == 1 and t.theatre == "space" then space_tab = t end end
    verify.expect(space_tab ~= nil, "Design offers a Space theatre")
    if space_tab ~= nil then
        verify.click(space_tab.x, space_tab.y)
        verify.frames(2)
        local d = verify.space_designs()
        print(string.format("space_fleets_ui: %d hull rows", #d.hull_rows))
        verify.expect(#d.hull_rows == 5, "the Space theatre lists the five hulls")
        local frig = nil
        for _, r in ipairs(d.hull_rows) do if r.name == "Frigate" then frig = r end end
        if frig ~= nil then
            verify.expect(frig.enabled, "the staged Frigate design reads as live")
            verify.click(frig.x, frig.y)
            verify.frames(2)
            d = verify.space_designs()
            verify.expect(d.open_hull == 3 and #d.design_presses >= 1, "a hull row expands its slots in place with a Save press")
            verify.capture("space_fleets_designer")
            verify.click(frig.x, frig.y)
            verify.frames(2)
            d = verify.space_designs()
            verify.expect(d.open_hull == -1, "re-pressing the open hull row folds it (the toggle rule)")
        end
    end
else
    print("space_fleets_ui: the Military ledger did not open; the designer leg is skipped")
end
