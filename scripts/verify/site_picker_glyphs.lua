-- The site finder's building picker draws each option's building glyph (Ben,
-- 2026-11-05; docs/ui/ledgers/construction.md § The site ledger).
--
-- What this check reads: the REAL combo, opened by a press on it. The combo reports
-- what it drew for each option: the glyph's half-extent and the name's width against
-- its room. Every option the finder offers must have drawn, with a glyph, unelided.
-- It also presses one option, so the row's press still chooses after the swap to a
-- hand-drawn label, and presses it again, so the toggle still clears.
--
-- Run with: ProjectIo --verify scripts/verify/site_picker_glyphs.lua

verify.window(1720, 1080)
verify.goto_surface("home")
verify.show_panel("build", true)
verify.frames(3)

local closed = verify.site_finder()
verify.expect(closed.picker_x >= 0, "the picker is on screen")
verify.expect(closed.n_picker_rows == 0, "a shut combo publishes no option rows")

local opts = verify.site_options()
verify.expect(#opts > 0, "the finder offers buildings")

verify.click(closed.picker_x, closed.picker_y)
verify.frames(3)
local open = verify.site_finder()
print(string.format("site_picker_glyphs: %d options offered, %d drawn", #opts, open.n_picker_rows))
verify.expect(open.n_picker_rows == #opts, "the open combo drew every option the finder offers")
verify.capture("site_picker_glyphs_open")

for i, o in ipairs(open.picker_rows) do
    verify.expect(o.name == opts[i].name, "option " .. i .. " is drawn in the door's order: " .. o.name)
    verify.expect(o.glyph_r > 0, "'" .. o.name .. "' draws its building glyph")
    verify.expect(o.need <= o.have,
                  string.format("'%s' fits its row (%.1f of %.1f px)", o.name, o.need, o.have))
end

-- Press a real row. The combo's first rows are always on screen, so the first one is
-- a press a click can reach; the swap to a hand-drawn label must keep the press.
local first = open.picker_rows[1]
verify.click(first.x, first.y)
verify.frames(3)
local chosen = verify.site_finder()
verify.expect(chosen.armed and chosen.choice == first.name,
              "pressing the row chooses it: " .. tostring(chosen.choice))
verify.expect(chosen.n_picker_rows == 0, "the press closes the combo")

-- The toggle: pressing the chosen row again clears it.
verify.click(chosen.picker_x, chosen.picker_y)
verify.frames(3)
local again = verify.site_finder()
verify.expect(again.n_picker_rows == #opts, "the combo opens again")
verify.click(again.picker_rows[1].x, again.picker_rows[1].y)
verify.frames(3)
verify.expect(not verify.site_finder().armed, "pressing the chosen row clears it")

print("site_picker_glyphs: done")
