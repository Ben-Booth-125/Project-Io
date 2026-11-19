-- BL-1703 (terse tooltips): the hover card writes its quantities as symbols
-- (TOOLTIP.md § The symbols). A player building's card leads with its good's item
-- glyph (an extraction site) and its workforce behind the labour glyph, not the words
-- "Extracting:" / "Workforce:". Capture-only evidence; the verdict is the expects.
--
-- Run with: ProjectIo --verify scripts/verify/tooltip_symbols.lua

verify.econ_step(2)
verify.goto_surface("home")
verify.show_panel("construction", false)
verify.show_panel("build", false)

local pick
for _, b in ipairs(verify.buildings()) do
    if b.player and b.complete then
        if b.type == "Extraction Site" then pick = b; break end
        pick = pick or b
    end
end
verify.expect(pick ~= nil, "a completed player building stands to hover")
if not pick then return end
print(string.format("tooltip_symbols: hovering a %s at %d,%d", pick.type, pick.x, pick.y))

verify.center_tile(pick.x, pick.y)
verify.frames(3)
local p = verify.tile_screen(pick.x, pick.y)
verify.hover(p.x - 300.0, p.y + 200.0, 3)
verify.hover(p.x, p.y, 31)
local t = verify.pointer_target()
verify.expect(t.hover_card, "the hover card appears over the building")
verify.capture("tooltip_symbols_building_card")
