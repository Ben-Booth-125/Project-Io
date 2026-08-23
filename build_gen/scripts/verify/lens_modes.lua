-- Demo capture: each map-lens overlay over the Planetary canvas (Kepler).
verify.goto_surface("Kepler")

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

verify.set_overlay("resource")
verify.set_lens_resource("iron_ore")
verify.capture("lens_resource_iron_ore")
