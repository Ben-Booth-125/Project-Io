-- BL-785: a water tile is selectable, and its Selection band answers who owns it.
--
-- The check is a SWEEP, not a single aimed press, because nothing in the verify
-- API can name a water tile: `find_deposit_tile` answers where a GOOD is, and
-- the terrain axis is deliberately not exposed to Lua (the standing rule). So
-- this walks a row across a coastline and captures the band at each step; the
-- shoreline is where the centre column changes shape, from the five-section
-- accordion to the owner/domain pair.
--
-- What a reader is looking for in the captures:
--   * an "Owner" row that says a NATION'S NAME on coastal water;
--   * an "Owner" row that says "Unowned" IN WORDS on open ocean - never blank;
--   * a "Domain" row naming which of the three domains this water is.
verify.goto_surface("home")

local row = 40
for i = 0, 11 do
    local col = 40 + i * 6
    verify.select_tile(col, row)
    verify.capture(string.format("water_selection_c%03d_r%03d", col, row))
end

-- THE PRESS, not the shortcut. `select_tile` writes the selection directly, so on
-- its own it proves the band draws and nothing about whether water is REACHABLE
-- by clicking. `click_tile` injects the real pointer event and goes through the
-- canvas hit-test, the marker precedence and the fallback — the same path a land
-- tile takes. Water resolving there is the item's first claim.
--
-- Both kinds of water are pressed: coastal (owned, on this seed) and open ocean.
for _, col in ipairs({ 40, 76 }) do
    verify.clear_selection()
    verify.click_tile(col, row)
    local s = verify.pointer_target()
    verify.expect(s.selection_kind == "tile",
                  string.format("clicking water at [%d, %d] selects a tile (got %s)",
                                col, row, tostring(s.selection_kind)))
    verify.capture(string.format("water_selection_clicked_c%03d_r%03d", col, row))
end
