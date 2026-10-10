-- Verify the Corporation lens with every rival picked, after the HQ star retired
-- from the Planetary canvas (Ben, 2026-10-10: "We should remove the HQ glyph and
-- market center glyphs"). The reach ring went first (BL-329, 2026-08-08); the
-- star — the player's always-on one and every picked rival's under this lens —
-- is now gone too. A corporation reads on this lens by its tile tint alone.
--
-- ASSERTED: picking every rival through the lens's own toggle lands them all in
-- the picked set, and a press on a rival's held tile resolves to that rival's
-- CORPORATION — the owner is reached through its ground, not through a seat glyph.
-- The captures show the tint without a star; the absence itself is read by eye.
verify.goto_surface("home")
verify.set_overlay("corporation")

local home_body = nil
for _, b in ipairs(verify.buildings()) do
    if b.player then home_body = b.body break end
end
local rivals, seen, tile_of = {}, {}, {}
for _, b in ipairs(verify.buildings()) do
    if b.body == home_body and not b.player and not b.background and not seen[b.corp] then
        seen[b.corp] = true
        tile_of[b.corp] = { x = b.x, y = b.y }
    end
end
for _, c in ipairs(verify.corps()) do
    if not c.is_player and not c.is_background then
        verify.toggle_lens_owner(c.id)
        rivals[#rivals + 1] = c.id
    end
end
verify.frames(2)
local picked = #verify.lens_owner_picks("corporation")
print(string.format("[corporate_reach] %d rivals, %d picked", #rivals, picked))
verify.expect(#rivals > 0, "the world has rival corporations")
verify.expect(picked >= #rivals, "every rival is picked")

-- Wide view: every picked rival's holdings tinted, no HQ stars.
verify.set_zoom(3)
verify.capture("corporate_reach_wide")

-- Framed on the player's holdings: the player's own tint, no always-on star.
for _, b in ipairs(verify.buildings()) do
    if b.player then
        frame_tile(b.x, b.y, 12)
        verify.capture("corporate_reach_player_hq")
        break
    end
end

-- A press on a rival's held tile selects the rival corporation.
local target = nil
for _, id in ipairs(rivals) do
    if tile_of[id] then target = id break end
end
verify.expect(target ~= nil, "a picked rival holds a building tile")
if target then
    local t = tile_of[target]
    verify.clear_selection()
    verify.click_tile(t.x, t.y)
    verify.frames(3)
    local pt = verify.pointer_target()
    print("[corporate_reach] press on a rival tile selected " .. tostring(pt.selection_kind))
    verify.expect(pt.selection_kind == "corporation",
                  "a Corporation-lens press on a rival's tile selects the corporation (got " ..
                  tostring(pt.selection_kind) .. ")")
    verify.capture("corporate_reach_rival_pressed")
    verify.clear_selection()
end
