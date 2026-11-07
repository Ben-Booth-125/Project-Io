-- Wind Power on the research panel (Ben, 2026-11-07: a major; docs/economy/tech/DIGITISATION_TREE.md
-- § One branch, node by node). It sits on Energy's axis between its two arms, clear of Fuel Economy
-- IV, and needs Fuel Economy III and V. A shift-click plans both arms ahead of it.
--
-- Captures: wind_power_ring (the panel at its default zoom), wind_power_hover (its card),
-- wind_power_planned (its route queued).
--
-- Run with: ProjectIo --verify scripts/verify/wind_power_links.lua

local function node(id)
    local x, y = verify.tech_tree_node(id)
    verify.expect(x ~= nil and x >= 0, "ring drew " .. id)
    return x, y
end
local function has(t, id) for _, v in ipairs(t) do if v == id then return true end end return false end

verify.window(1920, 1080)
verify.frames(2)
verify.show_panel("tech_tree", true)
verify.frames(3)

local wx, wy = node("DG-PR-EN-W")
local t3x, t3y = node("DG-PR-EN-T3")
local t5x, t5y = node("DG-PR-EN-T5")
local t4x, t4y = node("DG-PR-EN-T4")

local function dist(ax, ay, bx, by) return math.sqrt((ax - bx) ^ 2 + (ay - by) ^ 2) end
print(string.format("wind_power_links: W (%.0f,%.0f) T3 (%.0f,%.0f) T5 (%.0f,%.0f) T4 (%.0f,%.0f); W-T4 %.1f px",
                    wx, wy, t3x, t3y, t5x, t5y, t4x, t4y, dist(wx, wy, t4x, t4y)))
verify.expect(dist(wx, wy, t4x, t4y) > 12.0, "Wind Power no longer draws on Fuel Economy IV")
verify.expect(math.abs(dist(wx, wy, t3x, t3y) - dist(wx, wy, t5x, t5y)) < 3.0, "Wind Power sits between its two arms")
verify.capture("wind_power_ring")

verify.hover(wx, wy, 40)
verify.capture("wind_power_hover")

verify.shift_click(wx, wy)
verify.frames(3)
verify.hover(20, 20, 2)
local q = verify.tech_tree_queue()
local ids = {}
for _, e in ipairs(q) do ids[#ids + 1] = type(e) == "table" and e.id or e end
print("wind_power_links: planned " .. #ids .. " entries")
verify.expect(has(ids, "DG-PR-EN-T3"), "the route plans Fuel Economy III")
verify.expect(has(ids, "DG-PR-EN-T5"), "the route plans Fuel Economy V")
verify.expect(ids[#ids] == "DG-PR-EN-W", "Wind Power comes last on its route")
verify.capture("wind_power_planned")
