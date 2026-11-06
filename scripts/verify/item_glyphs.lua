-- The item glyphs (BL-1518; ICONS.md § 2b): one shape per good, the full set.
--
-- The census is the assertion: no two goods draw the same geometry, and every
-- slot draws something. It cannot judge whether a shape reads, so the contact sheet
-- is captured for a person: every good at r = 4, 5, 6, 8 and 16 px.

verify.goto_surface("home")
verify.show_panel("market", true)
verify.panel_view("market", 0)
verify.frames(2)
verify.capture("item_glyphs_goods_rows")

verify.item_glyph_sheet(true)
verify.frames(2)
local census = verify.item_glyph_census()
verify.capture("item_glyphs_contact_sheet")
verify.item_glyph_sheet(false)
verify.frames(1)

print("MEASURED item glyphs: " .. #census .. " goods")
verify.expect(#census > 0, "the contact sheet drew a census")

local seen, empty, shared = {}, 0, 0
for _, g in ipairs(census) do
    if g.vertices == 0 then
        empty = empty + 1
        print("MEASURED no geometry: " .. g.name)
    end
    if seen[g.shape] then
        shared = shared + 1
        print("MEASURED shared shape: " .. g.name .. " and " .. seen[g.shape])
    else
        seen[g.shape] = g.name
    end
end
verify.expect(empty == 0, "every good draws a glyph (" .. empty .. " draw nothing)")
verify.expect(shared == 0, "no two goods share a glyph (" .. shared .. " do)")
