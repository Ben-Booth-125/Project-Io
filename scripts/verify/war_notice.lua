-- The war notice (LAYOUT.md § War notice, Ben 2026-11-09): when a war the player's nation
-- is party to opens, a dismissible notice on the canvas names both parties.
--
-- Stages a rival's declaration on the player through the same verb the rival's scorer
-- issues, and captures the notice. Asserts only what the existing reads can see; the
-- notice's own text and Dismiss are read from the capture and owed to a live click.
--
-- Run:     ProjectIo --verify scripts/verify/war_notice.lua
-- Writes:  screenshots/war_notice_*.png

local function fail(m) print("war_notice: FAIL " .. m) end

verify.window(1280, 720)
verify.goto_surface("home")
verify.frames(3)
verify.capture("war_notice_00_before")

-- A rival's unit, and the player's, to name the two parties and a goal province.
local rival, mine = nil, nil
for _, u in ipairs(verify.units()) do
    if u.col ~= nil then
        if not u.player and rival == nil then rival = u end
        if u.player and mine == nil then mine = u end
    end
end
assert(rival ~= nil and mine ~= nil, "no rival or player unit to stage a war with")

verify.center_tile(mine.col, mine.row, 8.0)
verify.frames(2)
local p = verify.tile_screen(mine.col, mine.row)
verify.hover(p.x, p.y, 2)
local pid = verify.pointer_target().hovered_province
verify.mouse(-1, -1)
assert(pid ~= nil and pid ~= 0, "no province under the player's unit")

-- corp_verb::declare_war is 47 (append-only enum; corp_command.hpp).
local res = verify.corp_command({verb = 47, corp = rival.owner, counterparty = mine.owner, provinces = {pid}})
print("war_notice: staged " .. rival.owner .. " on " .. mine.owner .. " goal " .. pid .. ": " .. tostring(res))
verify.frames(3)
verify.capture("war_notice_01_declared_on_player")
print("war_notice: done")
