-- BL-1225 (every lens on strip): every built lens sits in at least one rung row
-- of the minimap strip and carries its OWN glyph (Ben, 2026-10-07; LENSES.md
-- § The strip rotates with the rung).
--
-- Captures the strip at each of the three rungs, with each of the four new
-- glyphs (Company, Trade flow at Planetary; Reach, Supply-routes at Solar) shown
-- once ACTIVE, so the highlighted cell names which glyph is which. The "zoomed"
-- reads are crops of these frames taken by eye: capture() is full-frame only.
--
-- The rung is changed by the ascend command — goto_surface resolves a body name
-- and always lands on Planetary (see lens_strip_and_fields.lua § 4).
--
-- Run with: ProjectIo --verify scripts/verify/lens_strip_glyphs.lua

verify.goto_surface("home")
verify.clear_selection()

-- Planetary: ten glyphs. Plain first, then each new glyph active.
verify.set_overlay("none")
verify.frames(2)
verify.capture("lens_glyphs_planetary")

verify.set_overlay("company")
verify.frames(2)
verify.expect(verify.overlay_name() == "company", "Company lens armed")
verify.capture("lens_glyphs_planetary_company")

verify.set_overlay("trade_flow")
verify.frames(2)
verify.expect(verify.overlay_name() == "trade_flow", "Trade flow lens armed")
verify.capture("lens_glyphs_planetary_trade_flow")

-- Circumplanetary: Market, Scarcity, Supply (unchanged; captured for the set).
verify.set_overlay("none")
verify.command("ascend")
verify.frames(3)
verify.capture("lens_glyphs_circumplanetary")

-- Solar: Supply, Reach, Supply-routes.
verify.command("ascend")
verify.frames(3)
verify.capture("lens_glyphs_solar")

verify.set_overlay("reach")
verify.frames(2)
verify.capture("lens_glyphs_solar_reach")

verify.set_overlay("supply_routes")
verify.frames(2)
verify.capture("lens_glyphs_solar_supply_routes")
