-- Every lens's path to further information: what HOVER lights, and what the
-- SELECTION that follows opens.
--
-- Ben, 2026-08-28: "show me each lens, with it's new hover feature, and then the
-- result of selection on that hover feature ... the lens is a visual prelude to
-- whatever we design on ledgers using various charts and tables."
--
-- The pairing is the point, and it is why each lens gets TWO captures rather than
-- one. A hover highlight that lights the right region and then opens the wrong
-- ledger is a worse failure than either half alone, and no single frame can show
-- it. Captured as <lens>_hover then <lens>_selected, always in that order.
--
-- Structures resolve at two grains here (LENSES.md § Structure-grain selection):
--   market / scarcity  -> the catchment      -> Market ledger, aimed at it
--   corporation        -> the corp's holdings-> Balance ledger  (marker-resolved)
--   resource           -> the DEPOSIT        -> Market ledger for that resource   (BL-659)
--   continent          -> the PLATE          -> History ledger                    (BL-660)
-- The last two are non-entity structures and travel in their own ui_state fields
-- (Ben's option A, 2026-08-28) rather than in selected_entity.

verify.goto_surface("home")
verify.econ_step(6)

-- A tile with ground under it on every body we frame. Picked by walking the
-- buildings table rather than hard-coded, so a generation change cannot quietly
-- move the world out from under this check (the pop_markers lesson, BL-363).
-- IT MUST BE MARKER-FREE GROUND, which the first draft of this check got wrong.
-- Aiming at the player's own building made every press resolve to the BUILDING:
-- a marker outranks a structure by design ("a marker is a specific thing the
-- player aimed at", lens_structure_of_tile), so the deposit and plate pivots
-- could never fire and the captures showed an Extraction Site instead. Anchor on
-- a player building, then step OFF it onto clear ground near by.
local anchor_col, anchor_row = nil, nil
for _, b in ipairs(verify.buildings()) do
    if b.player then anchor_col, anchor_row = b.x, b.y; break end
end
verify.expect(anchor_col ~= nil, "found a player building to anchor near")

local occupied = {}
for _, b in ipairs(verify.buildings()) do
    occupied[tostring(b.x) .. "," .. tostring(b.y)] = true
end

verify.center_tile(anchor_col, anchor_row, 8)
local probe_col, probe_row = nil, nil
for radius = 2, 8 do
    for dc = -radius, radius do
        for dr = -radius, radius do
            local col, row = anchor_col + dc, anchor_row + dr
            if not occupied[tostring(col) .. "," .. tostring(row)] then
                local pt = verify.tile_screen(col, row)
                -- On canvas, clear of the bottom band and the right chrome column.
                if pt.ok and pt.y < 440 and pt.x > 70 and pt.x < 920 then
                    probe_col, probe_row = col, row
                    break
                end
            end
        end
        if probe_col then break end
    end
    if probe_col then break end
end
verify.expect(probe_col ~= nil, "found marker-free ground to press")

-- One lens, two frames: the hover highlight, then whatever the click opened.
local function hover_then_select(lens, name)
    verify.set_overlay(lens)
    verify.center_tile(probe_col, probe_row, 8)
    verify.hover_tile(probe_col, probe_row)
    shot(name .. "_hover")
    verify.click_tile(probe_col, probe_row)
    shot(name .. "_selected")
end

hover_then_select("resource",    "path_resource")
hover_then_select("market",      "path_market")
hover_then_select("scarcity",    "path_scarcity")
hover_then_select("corporation", "path_corporation")
hover_then_select("company",     "path_company")
hover_then_select("continent",   "path_continent")

-- Population draws at tile grain and has no structure, which is a real answer
-- rather than a gap (lens_structure_of_tile returns none for it). Captured so
-- the contrast is on file: hovering lights one tile, not a region.
hover_then_select("population",  "path_population")

verify.set_overlay("none")
verify.clear_selection()
shot("path_cleared")
