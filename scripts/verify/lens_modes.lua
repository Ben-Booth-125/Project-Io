-- Demo capture: each map-lens overlay over the Planetary canvas (Kepler).
verify.goto_surface("home")

verify.set_overlay("corporation")
verify.capture("lens_corporation")

-- The Company lens (2026-08-28) is the Corporation lens's mirror: background
-- firms only, drawn identically. Captured next to it deliberately — the pair is
-- only right if the two pictures are DISJOINT, which one capture cannot show.
verify.set_overlay("company")
verify.capture("lens_company")

verify.set_overlay("scarcity")
verify.capture("lens_scarcity")

verify.set_overlay("population")
verify.capture("lens_population")

verify.set_overlay("market")
verify.capture("lens_market")

verify.set_overlay("supply")
verify.capture("lens_supply")

-- Off-strip lenses (BL-011/BL-014) — selectable by name since the lens-cycle
-- count fix; both read the player's trade_routes for the active body.
verify.set_overlay("reach")
verify.capture("lens_reach")

verify.set_overlay("supply_routes")
verify.capture("lens_supply_routes")

verify.set_overlay("resource")
verify.set_lens_resource("iron_ore")
verify.capture("lens_resource_iron_ore")

-- The lens chrome region (BL-602): one home in the minimap header, top right, for
-- the selector and whichever key the active lens draws. The count-driven keys are a
-- DROPDOWN collapsed by default, and their header bar sits on the minimap's top edge
-- so the toggle does not travel when the body opens upward. Press it and capture both
-- states -- the press is the thing a capture alone cannot prove.
verify.window(1280, 720)
verify.set_overlay("corporation")
verify.capture("lens_chrome_corporation_collapsed")

-- The header bar is the full width of the right chrome column, its foot on the
-- minimap's top edge. Aim at the middle of that bar rather than at the caret glyph.
local mini = { w = 336, h = 260 }   -- minimap_rect at 1280x720; see shell_metrics.cpp
verify.click(1280 - mini.w * 0.5, 720 - mini.h - 10)
verify.frames(2)
verify.capture("lens_chrome_corporation_expanded")

-- Toggle rule: a second press on the same spot closes it again.
verify.click(1280 - mini.w * 0.5, 720 - mini.h - 10)
verify.frames(2)
verify.capture("lens_chrome_corporation_reclosed")

verify.set_overlay("none")
