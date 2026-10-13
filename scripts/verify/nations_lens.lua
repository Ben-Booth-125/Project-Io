-- BL-1311 (Nations lens): the lens paints each nation's whole territory, a press
-- under it resolves to the nation, and no two bordering nations share a similar
-- colour (LENSES.md § The Nations lens; the identity colour rule).
--
-- The colour claim is asserted against the LIVE table every surface reads
-- (verify.nation_colour_clashes walks the bordering pairs the pin saw and
-- measures them through palette::nation_colour), so a surface that kept its own
-- copy could not pass this by accident.

local c = verify.nation_colour_clashes()
print(string.format("[nations_lens] bordering pairs %d; min neighbour distance %.2f (threshold %.1f); clashes %d",
                    c.pairs, c.min_distance, c.threshold, c.clashes))
verify.expect(c.pairs > 0, "the verify world has bordering nations to check")
verify.expect(c.clashes == 0, "no two bordering nations sit under the minimum colour distance")

-- The plain map first: the national border band, unchanged.
verify.goto_surface("home")
verify.set_overlay("none")
verify.set_zoom(4)
verify.capture("nations_lens_plain_band")

-- The lens: whole territories filled, band suppressed.
verify.set_overlay("nations")
verify.capture("nations_lens_fill")

-- A press on owned ground under the lens resolves to the nation. A player
-- building stands on a nation's ground, so its tile is the probe.
local col, row = nil, nil
for _, b in ipairs(verify.buildings()) do
    if b.player then col, row = b.x, b.y; break end
end
verify.expect(col ~= nil, "found a player building to aim at")
if col then
    verify.center_tile(col, row, 8)
    verify.capture("nations_lens_fill_zoom")
    verify.click_tile(col, row)
    local t = verify.pointer_target()
    print("[nations_lens] press under the lens -> " .. tostring(t.selection_kind))
    verify.expect(t.selection_kind == "nation", "Nations lens: a press on owned ground selects the nation")
    verify.capture("nations_lens_press")
end
verify.set_overlay("none")
