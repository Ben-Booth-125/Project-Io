-- BL-210 (Continents/Drift, first slice) — a look at Kepler's terrain now that
-- Pass 1's heightmap is biased by simulated plate boundaries instead of pure
-- noise. Plain terrain shows the landmass shape itself; the Country lens shows
-- how nation borders (Voronoi over that terrain) read against it.
verify.goto_surface("Kepler")
verify.set_zoom(1)

verify.set_overlay("none")
verify.capture("continents_terrain_plain_wide")

verify.set_overlay("country")
verify.capture("continents_terrain_country_wide")
