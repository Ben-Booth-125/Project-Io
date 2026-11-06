-- The Build door's folds (Ben, 2026-11-05; docs/ui/SELECTION.md § The tile construction
-- ledger): Power Generation sits in Infrastructure, and a Road fold holds Track, Road,
-- Highway and Railroad at the shared row height. The Railway Station stays in
-- Infrastructure. The finder files every building as the door does.
--
-- What this check reads: the rows the door drew this frame, each naming the fold it drew
-- in and the height it drew at, and the options the finder offers.
--
-- Run with: ProjectIo --verify scripts/verify/construction_folds.lua

verify.window(1720, 1080)
verify.goto_surface("home")
verify.set_balance(5000000.0)
verify.show_panel("build", true)
verify.frames(3)

-- THE FINDER'S HALF: one mapping, so a Power Generation recipe is never in Processing.
local power_options = 0
for _, o in ipairs(verify.site_options()) do
    if o.group == "Power Generation" then
        power_options = power_options + 1
        verify.expect(o.category == "Infrastructure",
                      "the finder files '" .. o.name .. "' under Infrastructure, not " .. o.category)
    end
end
print(string.format("construction_folds: %d Power Generation options in the finder", power_options))

-- Aim the door at the player's own ground, by the surface that knows it.
local aimed = false
for _, o in ipairs(verify.site_options()) do
    if o.category == "Extraction" then
        verify.site_pick(o.name)
        verify.frames(2)
        local s = verify.site_finder()
        if s.n_rows > 0 then
            verify.select_tile(s.rows[1].x, s.rows[1].y)
            aimed = true
            break
        end
    end
end
verify.expect(aimed, "the site finder found the player's own ground")
verify.site_clear()
verify.frames(3)
verify.capture("construction_folds_top")

local door = verify.build_door()
verify.expect(door.n_rows > 0, "the door drew its rows")

local folds, seen = {}, {}
for _, r in ipairs(door.rows) do
    if not seen[r.fold] then seen[r.fold] = true; folds[#folds + 1] = r.fold end
    verify.expect(r.fold ~= "Road", "no building row sits in the Road fold: " .. r.name)
    if r.name == "Power Generation" then
        verify.expect(r.fold == "Infrastructure", "the door files Power Generation under Infrastructure, not " .. r.fold)
    end
    if r.fold == "Processing" then
        verify.expect(r.name ~= "Power Generation", "Processing no longer holds Power Generation")
    end
end
local fold_list = ""
for _, f in ipairs(folds) do fold_list = fold_list .. (fold_list == "" and "" or ", ") .. f end
print("construction_folds: building folds drawn: " .. fold_list)

-- THE ROAD FOLD: three tiers, in the shared grid's row height, with their presses.
verify.scroll_panel("build_list", 1.0)
verify.frames(3)
verify.capture("construction_folds_road")
door = verify.build_door()
local roads = verify.road_state()
verify.expect(roads.n_rows == 3, "the Road fold drew three road tiers: " .. roads.n_rows)
local want = { "Track", "Road", "Highway" }
for i, r in ipairs(roads.rows) do
    verify.expect(r.name == want[i], "road row " .. i .. " reads " .. want[i] .. ": " .. r.name)
    verify.expect(r.fold == "Road", "'" .. r.name .. "' sits in the Road fold")
    verify.expect(math.abs(r.h - door.row_h) < 0.5,
                  string.format("'%s' draws at the shared row height (%.1f vs %.1f)", r.name, r.h, door.row_h))
    verify.expect(r.x > 0, "'" .. r.name .. "' publishes its press")
    verify.expect(r.explainer ~= "", "'" .. r.name .. "' press explains itself")
end

-- Rail stands only in the eras that have it, and the rows are absent otherwise.
print(string.format("construction_folds: %d rail rows", door.n_rail_rows))
for _, r in ipairs(door.rail_rows) do
    verify.expect(r.name ~= "Track", "the rail row no longer reads Track")
    if r.name == "Railroad" then
        verify.expect(r.fold == "Road", "Railroad sits in the Road fold, not " .. r.fold)
    elseif r.name == "Railway Station" then
        verify.expect(r.fold == "Infrastructure", "the station stays in Infrastructure, not " .. r.fold)
    end
    verify.expect(math.abs(r.h - door.row_h) < 0.5, "'" .. r.name .. "' draws at the shared row height")
end

-- THE TOGGLE ON A GRID PRESS: a road press arms, and pressing it again disarms.
local live = nil
for _, r in ipairs(roads.rows) do if r.enabled then live = r break end end
if live ~= nil then
    verify.click(live.x, live.y)
    verify.frames(3)
    local armed = verify.road_state()
    verify.expect(armed.armed and armed.arm_tier == live.tier, "the grid press arms " .. live.name)
    verify.capture("construction_folds_armed")
    local again = armed.rows[live.tier]
    verify.click(again.x, again.y)
    verify.frames(3)
    verify.expect(not verify.road_state().armed, "pressing the armed row again disarms it")
else
    print("construction_folds: no live road press on this tile; the arm toggle went unexercised")
end

print("construction_folds: done")
