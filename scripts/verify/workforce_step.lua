-- BL-1322 (WORKFORCE_STEPPED_AUTO): the game sets the player's workforce.
--
-- PRODUCTION.md § Workforce target and the auto-solver (Ben, 2026-10-13): each
-- economy tick the target of every player producing building moves ONE 25 %
-- step toward the solver's most profitable level on the internal grid 0/50/100/150/200 (shown as /2, 100% = 200),
-- never further in one quarter; the player has no control over it.
--
-- The check: stage a player extraction site (alone on its tile, so the fixture
-- writes the right building) at 0 % with the verify FIXTURE, then step five
-- quarters and assert every move is at most 25, every target is in 0-100 and on
-- the 25 grid. Then stage it at 100 and step again: the same bounds hold coming
-- down. Prints both series. Also asserts the expanded card's workforce row is
-- read-only text that follows the target.
--
--   ProjectIo --verify scripts/verify/workforce_step.lua

verify.window(1720, 1080)
verify.goto_surface("home")
verify.econ_step(2)

local function by_id(id)
    for _, b in ipairs(verify.buildings()) do
        if b.id == id then return b end
    end
    return nil
end

-- A completed player extraction site that stands alone on its tile.
local count = {}
for _, b in ipairs(verify.buildings()) do count[b.tile] = (count[b.tile] or 0) + 1 end
local site = nil
for _, b in ipairs(verify.buildings()) do
    if b.player and b.complete and b.type == "Extraction Site" and count[b.tile] == 1 and not site then
        site = b
    end
end
assert(site, "workforce_step.lua: no lone completed player extraction site on the home body")
print(string.format("workforce_step: site %d at [%d, %d], target %d%%", site.id, site.x, site.y, site.workforce))

local function walk(from, label)
    local wf = verify.set_building_workforce(site.tile, from)
    verify.expect(wf == from, label .. ": the fixture staged " .. from .. "% (got " .. wf .. ")")
    local prev, series = from, tostring(from)
    for q = 1, 5 do
        verify.econ_step(1)
        local now = by_id(site.id).workforce
        series = series .. " -> " .. tostring(now)
        verify.expect(now >= 0 and now <= 200 and now % 50 == 0,
                      label .. " q" .. q .. ": the target is on the 0-200 50-grid (got " .. now .. ")")
        verify.expect(math.abs(now - prev) <= 50,
                      label .. " q" .. q .. ": at most one 50-internal (25% shown) step a quarter (" .. prev .. " -> " .. now .. ")")
        prev = now
    end
    print("workforce_step: " .. label .. " " .. series)
    return prev
end

walk(0, "from 0%")
local last = walk(200, "from 200 (100% shown)")

-- The card reads the target, read-only.
verify.select_building(site.x, site.y)
verify.frames(3)
local c = verify.tile_card()
verify.expect(c.drawn and c.is_expanded and c.player, "the site's card is expanded and the player's")
verify.expect(c.workforce == tostring(last // 2) .. "%",
              "the card's workforce row reads the target the game set ('" .. c.workforce .. "' vs " .. last .. "%)")
verify.hover(860, 300, 2)
verify.frames(2)
verify.capture("workforce_step_card")
