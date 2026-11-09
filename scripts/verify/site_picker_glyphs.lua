-- The site finder's building grid draws each option's building glyph (Ben,
-- 2026-11-05; the grid itself Ben, 2026-11-08; docs/ui/ledgers/construction.md
-- § The site ledger).
--
-- What this check reads: the REAL grid, resting in the Site Finder view (Ben, 2026-11-09). The grid
-- reports what it drew for each cell: the glyph's half-extent and the name's widest
-- wrapped line against its room. Every option the finder offers must have drawn, with
-- a glyph, unelided. It also presses a cell, so the press still chooses, and presses
-- it again, so the toggle still clears.
--
-- Run with: ProjectIo --verify scripts/verify/site_picker_glyphs.lua

verify.window(1720, 1080)
verify.goto_surface("home")
verify.show_panel("build", true)
verify.construction_view(2)
verify.frames(3)

local resting = verify.site_finder()
verify.expect(resting.picker_x >= 0, "the grid is on screen")

local opts = verify.site_options()
verify.expect(#opts > 0, "the finder offers buildings")
print(string.format("site_picker_glyphs: %d options offered, %d drawn", #opts, resting.n_picker_rows))
verify.expect(resting.n_picker_rows == #opts, "the grid drew every option the finder offers")
verify.capture("site_picker_glyphs_open")

-- The grid orders by estimated net (site_grid.lua asserts the order); this check
-- asserts the door's vocabulary is all there and every cell is legible.
local offered = {}
for _, o in ipairs(opts) do offered[o.name] = true end
for _, o in ipairs(resting.picker_rows) do
    verify.expect(offered[o.name] == true, "cell is one the door offers: " .. o.name)
    verify.expect(o.glyph_r > 0, "'" .. o.name .. "' draws its building glyph")
    verify.expect(o.need <= o.have + 0.5,
                  string.format("'%s' fits its cell (%.1f of %.1f px)", o.name, o.need, o.have))
end

-- Press a real cell. The first cell is always on screen.
local first = resting.picker_rows[1]
verify.click(first.x, first.y)
verify.frames(3)
local chosen = verify.site_finder()
verify.expect(chosen.armed and chosen.choice == first.name,
              "pressing the cell chooses it: " .. tostring(chosen.choice))
verify.expect(chosen.n_picker_rows == #opts, "the grid stays drawn with a building chosen")

-- The toggle: pressing the chosen cell again clears it.
local again = chosen.picker_rows[1]
verify.expect(again.name == first.name, "a choice does not reorder the grid")
verify.click(again.x, again.y)
verify.frames(3)
verify.expect(not verify.site_finder().armed, "pressing the chosen cell clears it")

print("site_picker_glyphs: done")
