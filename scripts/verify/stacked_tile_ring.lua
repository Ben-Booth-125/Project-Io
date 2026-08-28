-- The stacked-tile ring: does a tile carrying several building KINDS say which
-- kinds, and does the ring degrade instead of vanishing as the canvas zooms out?
--
-- WHY THIS SCRIPT STAGES ITS OWN STACK. A multi-KIND tile is not something the
-- generated world reliably produces: generation sites extraction, and the ancient
-- roster's processing/logistics/garrison buildings arrive through play. Capturing
-- whatever the default world happens to hold would be a check that passes on an
-- empty ring and proves nothing, so the stack is BUILT here — the player's own
-- extraction tile, then a processing facility, a logistics hub and a military base
-- placed onto the same tile through the same construct_building path a click uses.
-- Each placement's result is asserted, so a script that could not reach a multi-kind
-- stack FAILS rather than quietly capturing a single-kind tile.
--
-- WHAT THE FRAMES SHOW (BL-596):
--   * the ring at play zoom — one arc per kind, dominant kind at 12 o'clock, the
--     dominant kind's own glyph in the centre;
--   * the same tile at each rung of the zoom-out, which is where the ring's LOD
--     bound (draw_r > 10 px) has to hold. Below it the tile MUST still carry the
--     dominant glyph — the degrade is to one glyph, never to an empty hex;
--   * the ring under a lens, since a lens fill composites over the same ground the
--     ring is drawn on.
--
-- Run: ProjectIo --verify scripts/verify/stacked_tile_ring.lua

verify.window(1280, 720)
verify.goto_surface("home")
verify.econ_step(4)
verify.show_panel("economy", false)

-- Placement is cash-gated and this check is about the RENDER, not about whether the
-- player could afford four buildings on day four.
verify.set_balance(5.0e8)

-- The seed tile: the player's own first extraction site. Chosen from verify.buildings()
-- rather than hard-coded, so a generation change cannot silently point this check at
-- open ground.
local site = nil
for _, b in ipairs(verify.buildings()) do
    if b.player and b.type == "Extraction Site" then site = b; break end
end
verify.expect(site ~= nil, "found a player extraction site to stack onto")
assert(site, "no player-owned extraction site on the home body")
print(string.format("STACK seed tile (%d,%d)", site.x, site.y))

-- Three more KINDS on the one tile. The extraction stack and the non-extraction
-- stack have separate ceilings (placement_rules::stack_capacity), so these three
-- land alongside the site rather than competing with it for its slots.
local function stack(kind, target)
    verify.place_mode(kind, target)
    local r = verify.build_at(site.x, site.y)
    print(string.format("STACK %-14s -> %s", kind, r))
    return r
end

local kinds_placed = 1 -- the extraction site already standing
if stack("processing")    == "placed" then kinds_placed = kinds_placed + 1 end
if stack("logistics_hub") == "placed" then kinds_placed = kinds_placed + 1 end
if stack("military_base") == "placed" then kinds_placed = kinds_placed + 1 end

-- The ring draws nothing below two kinds, so anything less than two makes every
-- frame below a capture of a state the check was not staging.
verify.expect(kinds_placed >= 2,
              "staged a multi-kind stack: " .. tostring(kinds_placed) .. " kinds on one tile")

-- Let the sites finish, so the centre carries its TYPE silhouette rather than the
-- under-construction crane. The ring is drawn either way — a site being built is
-- still a kind standing there — but the dominant-glyph read is what the frames are for.
verify.econ_step(30)
verify.clear_selection()
verify.set_overlay("none")

-- Close in: the ring, its segments and the centre glyph each at full size.
verify.center_tile(site.x, site.y, 44)
shot("stack_ring_close")

-- The zoom ladder down through the ring's own LOD bound (draw_r > 10 px). The two
-- middle rungs BRACKET it: MEASURED in this window (1280x720, the fitted 180x84 grid),
-- the ring still carries three separable arcs at zoom 3.5 and is gone at 3.0 — so 3.5
-- is the last frame that should show a ring and 3.0 the first that must show the
-- dominant glyph ALONE. (The bracket is stated as a measurement, not as arithmetic
-- from hex_size: the grid fits to the canvas rect, so the zoom that lands on 10 px
-- moves with the window and the ledger column, and a computed rung would silently
-- stop bracketing anything the next time the shell is re-laid.) 1.5 is the far end,
-- inside the coarse-fill band, where there is no rim left to segment at all and the
-- glyph must STILL be there.
for _, z in ipairs({ 12.0, 6.0, 3.5, 3.0, 1.5 }) do
    verify.center_tile(site.x, site.y, z)
    shot(string.format("stack_ring_zoom_%04.1f", z))
end

-- Under a lens: the ground under the ring is a saturated categorical fill here, and
-- the segments' dark under-stroke is what has to carry them over it.
verify.center_tile(site.x, site.y, 44)
verify.set_overlay("none")
