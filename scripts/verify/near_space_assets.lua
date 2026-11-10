-- BL-1590 part 2 (circumplanetary space draw): the Circumplanetary rung draws
-- what sits at its orbital nodes - satellites (owner colour, role mark),
-- stations (a larger square-cored mark) and filled slot pips in the holder's
-- colour (docs/ui/CIRCUMPLANETARY.md § Orbital nodes and what sits at them).
--
-- verify.near_space_fixture() stages, at the home body's nodes: two player
-- satellites in low orbit, a player station in high orbit, a rival satellite in
-- high orbit, and a player survey satellite in transit to the first moon's L4.
-- Frames: the rung at rest, a hover on a satellite (its card), a hover on the
-- station, and the station selected after a press.

verify.goto_surface("home")
verify.command("ascend")
verify.frames(2)

local staged = verify.near_space_fixture()
print(string.format("near_space_assets: staged %d satellites, %d stations",
    staged.satellites, staged.stations))
verify.expect(staged.satellites >= 2, "the fixture staged satellites")
verify.expect(staged.stations == 1, "the fixture staged a station")

verify.mouse(-1, -1)
verify.frames(2)
verify.capture("near_space_assets_rest")

local assets = verify.circum_assets()
print(string.format("near_space_assets: %d assets drawn", #assets))
local sats, stations, transit, owners = {}, {}, 0, {}
for _, a in ipairs(assets) do
    print(string.format("  %-9s id=%-6d owner=%-6d x=%7.1f y=%7.1f r=%4.1f %s%s",
        a.kind, a.id, a.owner, a.x, a.y, a.r, a.node, a.in_transit and " (in transit)" or ""))
    if a.kind == "station" then stations[#stations + 1] = a else sats[#sats + 1] = a end
    if a.in_transit then transit = transit + 1 end
    owners[a.owner] = true
end
verify.expect(#sats == staged.satellites, "every staged satellite is drawn")
verify.expect(#stations == staged.stations, "the staged station is drawn")
local n_owners = 0
for _ in pairs(owners) do n_owners = n_owners + 1 end
verify.expect(n_owners >= 2, "two owners' assets are drawn")
if staged.satellites == 4 then verify.expect(transit == 1, "the in-transit satellite is drawn as such") end

-- A station draws larger than a satellite.
if #stations > 0 and #sats > 0 then
    verify.expect(stations[1].r > sats[1].r, "a station draws larger than a satellite")
end

-- Occupancy: the home low orbit (the first node listed) shows two held slots.
local nodes = verify.circum_nodes()
print(string.format("near_space_assets: %s slots %d/%d free", nodes[1].name, nodes[1].free, nodes[1].capacity))
verify.expect(nodes[1].capacity - nodes[1].free >= 2, "the home low orbit shows its held slots")
for _, n in ipairs(nodes) do
    if n.clamped then print("near_space_assets: clamped above the bottom strip: " .. n.name) end
end

-- Hover a satellite: its card answers.
local s = sats[1]
verify.hover(s.x, s.y, 3)
local hovered = verify.circum_asset_hovered()
verify.expect(hovered == s.id, "the satellite answers the hover")
verify.capture("near_space_assets_hover_satellite")

-- Hover, then press, the station.
local st = stations[1]
verify.hover(st.x, st.y, 3)
verify.expect(verify.circum_asset_hovered() == st.id, "the station answers the hover")
verify.capture("near_space_assets_hover_station")
verify.click(st.x, st.y)
verify.frames(2)
verify.expect(verify.circum_selected() == st.id, "a press on the station selects it")
verify.mouse(-1, -1)
verify.frames(2)
verify.capture("near_space_assets_station_selected")

-- A press on a satellite selects it.
verify.click(s.x, s.y)
verify.frames(1)
verify.expect(verify.circum_selected() == s.id, "a press on a satellite selects it")

-- BL-1614 (space transit segment): the in-transit satellite draws a dashed
-- segment from its recorded origin (the home low orbit ring) to its ghost at
-- the destination, with the marker at its share of the way.
verify.mouse(-1, -1)
verify.frames(2)
local segs = verify.circum_transits()
print(string.format("near_space_assets: %d transit segments drawn", #segs))
for _, sg in ipairs(segs) do
    print(string.format("  segment id=%-6d (%.1f,%.1f) -> (%.1f,%.1f) share=%.2f",
        sg.id, sg.x0, sg.y0, sg.x1, sg.y1, sg.share))
end
if transit == 1 then
    verify.expect(#segs == 1, "the in-transit satellite draws one segment")
    if #segs == 1 then
        local sg = segs[1]
        local ghost = nil
        for _, a in ipairs(assets) do if a.in_transit then ghost = a end end
        verify.expect(ghost ~= nil and sg.id == ghost.id, "the segment belongs to the in-transit satellite")
        if ghost ~= nil then
            verify.expect(math.abs(sg.x1 - ghost.x) < 0.5 and math.abs(sg.y1 - ghost.y) < 0.5,
                "the segment ends at the ghost mark")
        end
        local len = math.sqrt((sg.x1 - sg.x0) ^ 2 + (sg.y1 - sg.y0) ^ 2)
        verify.expect(len > 4, "the segment has a visible length")
        verify.expect(sg.share >= 0.45 and sg.share <= 1.0, "the marker sits at least half-way (one quarter of two gone)")
    end
end
verify.capture("near_space_assets_transit")
