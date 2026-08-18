-- Demo capture: each map-lens overlay over the Planetary canvas (Kepler).
verify.goto_surface("home")

verify.set_overlay("corporation")
verify.capture("lens_corporation")

verify.set_overlay("country")
verify.capture("lens_country")

verify.set_overlay("production")
verify.capture("lens_production")

verify.set_overlay("opportunity")
verify.capture("lens_opportunity")

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
