-- BL-1590 part 1 (circumplanetary nodes): the Circumplanetary rung draws the
-- orbital graph's nodes - low and high orbit rings around the anchor and each
-- moon, a diamond at each L1/L2/L4/L5 of every anchor-moon pair, and a row of
-- slot pips beside every node (docs/ui/CIRCUMPLANETARY.md § Orbital nodes and
-- what sits at them).
--
-- Three frames: the resting rung, a hover on an L4 diamond (the shared hover
-- card: name, free slots, thrust cost and days from home), and the same rung
-- zoomed in, to show the diamonds and pips keep their pixel size and the
-- L-points stay on the moon's drawn orbit. Then two presses: the anchor's disc
-- selects it, and a press on the L4 diamond leaves that selection alone.

verify.goto_surface("home")
verify.command("ascend")
verify.frames(2)
verify.capture("near_space_nodes_rest")

local nodes = verify.circum_nodes()
print(string.format("near_space_nodes: %d nodes drawn", #nodes))
for _, n in ipairs(nodes) do
    print(string.format("  %-28s x=%7.1f y=%7.1f ring=%5.1f slots %d/%d free",
        n.name, n.x, n.y, n.ring_r, n.free, n.capacity))
end
verify.expect(#nodes >= 2, "the anchor's low and high orbit are drawn")

-- Nothing is launched at the start: every node shows every slot free.
for _, n in ipairs(nodes) do
    verify.expect(n.free == n.capacity, n.name .. " shows every slot free")
end

local l4 = nil
for _, n in ipairs(nodes) do
    if n.ring_r == 0 and n.name:sub(-3) == " L4" then l4 = n break end
end

if l4 then
    verify.hover(l4.x, l4.y, 3)
    local hovered = verify.circum_node_hovered()
    print("near_space_nodes: hovered '" .. hovered .. "'")
    verify.expect(hovered == l4.name, "the L4 diamond answers the hover")
    verify.capture("near_space_nodes_hover_l4")

    -- The anchor's disc sits at its rings' centre (nodes[1] is its low orbit).
    -- A press there selects it; a press on the L4 diamond changes nothing.
    verify.click(nodes[1].x, nodes[1].y)
    verify.frames(1)
    local sel = verify.circum_selected()
    verify.expect(sel ~= 0, "a press on the anchor's disc selects it")
    verify.click(l4.x, l4.y)
    verify.frames(1)
    verify.expect(verify.circum_selected() == sel, "a press on a node changes nothing")
else
    print("near_space_nodes: the home anchor has no moon, so no L4 diamond to hover")
end

-- Zoomed in: diamonds and pips keep their pixel size.
verify.mouse(-1, -1)
for _ = 1, 8 do verify.command("zoom_in") end
verify.frames(2)
verify.capture("near_space_nodes_zoomed")
