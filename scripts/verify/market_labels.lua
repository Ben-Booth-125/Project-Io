-- BL-1316 (MARKET_LABELS_ON_CANVAS): under the Market lens, every market's name is drawn
-- on its market city (off the lens: plain labels, no glyph; Ben, 2026-10-13). The market
-- city (PLANETARY.md § Market labels). The city that names a market
-- (market_city_name's rule, BL-1315) draws its settlement label with the market
-- glyph (three ascending bars) before the name; a city that names no market keeps
-- its plain label. Capture-only: inspect that marked labels appear, that the
-- names match the Market ledger's combo, and that plain labels are unchanged.
--
--   ProjectIo --verify scripts/verify/market_labels.lua

verify.window(1920, 1080)
verify.goto_surface("home")
verify.econ_step(2)
verify.frames(2)
verify.capture("market_labels_lens_off")   -- plain labels, no market glyph
verify.set_overlay("market")
verify.frames(2)
verify.capture("market_labels_home")        -- glyphs, distinct catchment colours

for _, z in ipairs({ 2.0, 3.0 }) do
    verify.set_zoom(z)
    verify.frames(2)
    verify.capture(string.format("market_labels_zoom_%d", math.floor(z)))
end

-- The ledger beside the map, so a label can be read against the combo's name.
verify.show_panel("market", true)
verify.frames(2)
verify.capture("market_labels_with_ledger")
