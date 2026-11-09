-- Verify convoys on the map (BL-1506, convoys on map; SELECTION.md § The convoy element).
--
-- A marker at the lane head of each convoy on the active body, and a real press on one
-- selects it through the canvas: the Selection band then shows its card. A press on the
-- ground retires the selection. A rival's marker draws only under the player's vision.
-- The convoys are staged on the player's own ground: the trade pass offers none on this
-- world in a short run, and an empty canvas proves nothing.

verify.window(1920, 1080)
verify.goto_surface("home")

-- The lowest-id unit of the player's, which is what the staging anchors to.
local anchor = nil
for _, u in ipairs(verify.units()) do
    if u.player and u.col ~= nil and u.count > 0 then anchor = u; break end
end
assert(anchor, "the player has no unit on the surface")

local own_id = verify.seed_lane_convoy("iron_ore", 20, 0.05)
assert(own_id ~= 0, "two market centres near the player's ground are not there")
local rival_id = verify.seed_lane_convoy("iron_ore", 20, 0.06, true)
verify.frames(2)

verify.center_tile(anchor.col, anchor.row, 3.0)
verify.frames(3)

local function count_markers()
    local own, rival, ms = 0, 0, verify.convoy_markers()
    for _, m in ipairs(ms) do
        if m.mine then own = own + 1 else rival = rival + 1 end
    end
    return own, rival, ms
end
local own, rival, ms = count_markers()
print("convoy_map: markers in view: own " .. own .. ", rival " .. rival)
if own == 0 then
    verify.set_zoom(1.0)
    verify.frames(3)
    own, rival, ms = count_markers()
    print("convoy_map: zoomed out: own " .. own .. ", rival " .. rival)
end
verify.capture("convoy_map_overview")
-- The marker and its count box grow with the zoom (Ben, 2026-11-09): read the pair.
verify.set_zoom(8.0)
verify.center_tile(anchor.col + 5, anchor.row + 14, 8.0) -- onto the staged convoy, so the zoomed marker is in frame
verify.frames(3)
print("convoy_map: zoomed in markers: " .. #verify.convoy_markers())
verify.capture("convoy_map_zoom_in")
verify.center_tile(anchor.col, anchor.row, 3.0)
verify.frames(3)
verify.expect(own > 0, "the player's convoy draws a marker")

local target = nil
for _, m in ipairs(ms) do
    if m.mine then target = m; break end
end
if target ~= nil then
    verify.hover(target.x, target.y, 3)
    verify.click(target.x, target.y)
    verify.frames(3)
    print("convoy_map: pressed marker, selected_convoy=" .. verify.selected_convoy() .. " own id " .. own_id)
    verify.expect(verify.selected_convoy() == own_id or verify.selected_convoy() == rival_id,
                  "the press selected a convoy of the marker's tile")
    verify.capture("convoy_map_selected")
    verify.expect_no_clipping("convoy card")

    -- A second press on a marker holding two walks to the next.
    if target.count > 1 then
        local before = verify.selected_convoy()
        verify.click(target.x, target.y)
        verify.frames(3)
        print("convoy_map: repeat press walked " .. before .. " -> " .. verify.selected_convoy())
        verify.expect(verify.selected_convoy() ~= before, "a repeat press walks the tile's convoys")
        verify.capture("convoy_map_walked")
    end

    -- A press on the ground retires it (the tile takes the selection).
    verify.click(target.x - 150, target.y - 60)
    verify.frames(3)
    print("convoy_map: after a ground press selected_convoy=" .. verify.selected_convoy())
    verify.expect(verify.selected_convoy() == 0, "a press elsewhere retired the convoy selection")

    -- Under a lens a marker takes no press.
    verify.set_overlay("nations")
    verify.frames(2)
    verify.click(target.x, target.y)
    verify.frames(2)
    verify.expect(verify.selected_convoy() == 0, "under a lens the marker takes no press")
    verify.set_overlay("none")
    verify.frames(2)
end
