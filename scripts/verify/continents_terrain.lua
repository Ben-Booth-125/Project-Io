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

-- BL-226 (Continent lens): the plates themselves, read back from the retained
-- generation record. This is the check that the lens shows the plate field the
-- terrain was DERIVED from — the boundaries here should line up with the ridges
-- and coastlines visible in the plain capture above.
verify.set_overlay("continent")
verify.capture("continents_lens_kepler_wide")

-- The lens key hangs flush-left of the minimap, where the Selection element sits
-- whenever something is selected. Dismiss the selection so the key is actually in
-- frame: plate swatches, the pale boundary swatch, and the plate count.
verify.dismiss_selection()
verify.capture("continents_lens_kepler_key")

-- Zoomed in, where a boundary is a band of individual hexes rather than a line —
-- this is the capture that shows the pale boundary lift landing tile-by-tile.
verify.set_zoom(4)
verify.capture("continents_lens_kepler_zoom")
