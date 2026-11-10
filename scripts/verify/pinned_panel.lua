-- The pinned-items panel and its front pins (BL-1603; LAYOUT.md § Pinned items — right
-- column, middle). Drives REAL PRESSES through ImGui's hit-test (verify.click /
-- verify.double_click), so a control drawn off its panel FAILS here.
--
--   1. Select a unit, press the pin toggle on the Selection band: the panel draws a
--      watch card for it. Pin the home body too.
--   2. The toggle rule: pressing the toggle on a pinned subject unpins it.
--   3. Stage a rival's declaration on the player (the war_notice.lua idiom: the verb the
--      rival's scorer issues). A front pin appears above the player's pins.
--   4. Frame another body; double-click the front pin: the canvas goes to the home
--      body's Planetary view, centred on the front.
--   5. Frame another body; double-click the unit pin: the canvas goes back to the unit.
--   6. Single-click the front pin: the player's front is selected.
--
-- Run:     ProjectIo --verify scripts/verify/pinned_panel.lua
-- Writes:  screenshots/pinned_panel_*.png

local function say(m) print("pinned_panel: " .. m) end

verify.window(1280, 720)
verify.goto_surface("home")
verify.frames(3)

local p = verify.pinned_panel()
verify.expect(not p.panel, "no pins and no war: the panel draws nothing")
verify.capture("pinned_panel_00_empty")

-- 1. Pin a unit of the player's, then the home body.
local rival, mine = nil, nil
for _, u in ipairs(verify.units()) do
    if u.col ~= nil then
        if not u.player and rival == nil then rival = u end
        if u.player and mine == nil then mine = u end
    end
end
assert(rival ~= nil and mine ~= nil, "no rival or player unit to stage with")

verify.select_unit(mine.id)
verify.frames(2)
p = verify.pinned_panel()
verify.expect(p.toggle_x > 0, "the pin toggle shows for a selected unit")
say(string.format("toggle at (%.0f, %.0f) subject %d", p.toggle_x, p.toggle_y, p.toggle_subject))
verify.click(p.toggle_x, p.toggle_y)
verify.frames(2)
p = verify.pinned_panel()
verify.expect(p.pins == 1 and p.toggle_on, "pressing the toggle pins the unit")
verify.expect(p.panel and #p.cards == 1, "the panel draws one watch card")

verify.select_body("home")
verify.frames(2)
p = verify.pinned_panel()
verify.expect(p.toggle_x > 0 and not p.toggle_on, "the toggle shows unpinned for the home body")
verify.click(p.toggle_x, p.toggle_y)
verify.frames(2)
p = verify.pinned_panel()
verify.expect(p.pins == 2, "the home body pinned too")
verify.mouse(-1, -1)
verify.frames(2)
verify.capture("pinned_panel_01_two_pins")

-- The toggle on a TILE selection, whose layout draws no header row: where it lands.
verify.select_tile(mine.col, mine.row)
verify.frames(2)
local tp = verify.pinned_panel()
verify.expect(tp.toggle_x > 0 and not tp.toggle_on, "the toggle shows unpinned for a tile")
verify.capture("pinned_panel_01b_tile_toggle")
verify.select_body("home")
verify.frames(2)
p = verify.pinned_panel()

-- 2. The toggle rule: a press on a pinned subject unpins it; a second press re-pins.
verify.click(p.toggle_x, p.toggle_y)
verify.frames(2)
p = verify.pinned_panel()
verify.expect(p.pins == 1 and not p.toggle_on, "re-pressing the toggle unpins the body")
verify.click(p.toggle_x, p.toggle_y)
verify.frames(2)
p = verify.pinned_panel()
verify.expect(p.pins == 2 and p.toggle_on, "a third press pins it again")

-- 3. A war: a rival declares on the player.
verify.center_tile(mine.col, mine.row, 8.0)
verify.frames(2)
local sp = verify.tile_screen(mine.col, mine.row)
verify.hover(sp.x, sp.y, 2)
local pid = verify.pointer_target().hovered_province
verify.mouse(-1, -1)
assert(pid ~= nil and pid ~= 0, "no province under the player's unit")
-- corp_verb::declare_war is 47 (append-only enum; corp_command.hpp).
local res = verify.corp_command({verb = 47, corp = rival.owner, counterparty = mine.owner, provinces = {pid}})
say("staged " .. rival.owner .. " on " .. mine.owner .. ": " .. tostring(res))
verify.frames(3)
p = verify.pinned_panel()
say("cards " .. #p.cards .. " fronts " .. p.fronts)
verify.expect(p.fronts >= 1, "a war with a nation on the home body opens a front pin")
verify.expect(p.cards[1] ~= nil and p.cards[1].front, "the front pin sits above the player's pins")
verify.capture("pinned_panel_02_front_pin")

-- 4. Away to another body, then double-click the front pin.
local home_body = p.cards[1].id
verify.goto_surface("moon")
verify.frames(3)
local before = verify.pinned_panel()
say(string.format("before: level %d body %d", before.level, before.active_body))
verify.expect(before.active_body ~= home_body, "framed away from the front's body")
local fc = before.cards[1]
verify.double_click(fc.x, fc.y)
verify.frames(3)
local after = verify.pinned_panel()
say(string.format("after: level %d body %d pan (%.1f, %.1f)", after.level, after.active_body, after.pan_x, after.pan_y))
verify.expect(after.active_body == home_body, "double-clicking the front pin goes to the front's body")
verify.expect(after.pan_x ~= 0.0 or after.pan_y ~= 0.0 or after.center_pending,
              "and centres on the front rather than the grid's centre")
verify.mouse(-1, -1)
verify.frames(2)
verify.capture("pinned_panel_03_front_double_click")

-- 5. Away again, then double-click the unit's pin.
verify.goto_surface("moon")
verify.frames(3)
local q = verify.pinned_panel()
local uc = nil
for _, c in ipairs(q.cards) do
    if not c.front and c.id == mine.id then uc = c end
end
assert(uc ~= nil, "the unit's card is drawn")
verify.double_click(uc.x, uc.y)
verify.frames(3)
q = verify.pinned_panel()
verify.expect(q.active_body == home_body, "double-clicking the unit pin goes back to its body")
verify.expect(q.selected == mine.id, "and selects the unit")
verify.mouse(-1, -1)
verify.frames(2)
verify.capture("pinned_panel_04_unit_double_click")

-- 6. A single click on the front pin selects the front.
q = verify.pinned_panel()
verify.click(q.cards[1].x, q.cards[1].y)
verify.frames(2)
q = verify.pinned_panel()
say("front selected: " .. tostring(q.front_selected) .. " selected " .. q.selected)
verify.expect(q.front_selected or q.selected == q.cards[1].enemy,
              "a single click on the front pin selects the front (or the enemy)")
verify.mouse(-1, -1)
verify.frames(2)
verify.capture("pinned_panel_05_front_selected")
say("done")
