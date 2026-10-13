-- Visual verification for the redesigned TILE Selection element (BL-123, Ben's mockup).
-- Confirms the tile selection now takes the vertical stack rather than the old
-- action|facts split:
--   * a placeholder image box + [x, y] coordinate caption at the top;
--   * the tile's deposits drawn as vertical bar charts (world-max-relative), each
--     with a dotted-gridline axis, in a scrollable list;
--   * since BL-1300 (tile card split): no header row; preview | province card |
--     tile card (buildings, three across) | 2x3 action grid (Construct, History,
--     Supply) — History/Supply are stubs, Manage Buildings is gone.
-- Run with: ProjectIo --verify scripts/verify/selection_tile_layout.lua

verify.econ_step(4)                    -- populate live building / market state
verify.goto_surface("home")            -- Kepler: fully surveyed, deposits visible

-- A tile with deposits: image + coords at top, deposit bars in the middle,
-- the 2x2 button grid at the bottom. Header shows Tile + [>] [x].
verify.select_tile(66, 6)
verify.capture("selection_tile_layout")

-- A land tile that hosts a player building: the tile card's building buttons.
local player
for _, b in ipairs(verify.buildings()) do
    if b.player then player = b; break end
end
assert(player, "selection_tile_layout.lua: no player building on the home body")
verify.select_tile(player.x, player.y)
verify.capture("selection_tile_layout_built")
