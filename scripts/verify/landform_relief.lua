-- BL-231 (landform render) — the landform axis, which the renderer discarded until
-- now. `terrain_colour` keyed on composition alone, so mountain, highland, canyon,
-- valley, crater and rift all drew as flat hexes, even though landform drives build
-- cost (x1.0-x2.0), hazard, habitability and mineral richness.
--
-- Two channels, split by the measured mix (world_audit S3): a subtle relief tint for
-- the common ground (plains 77%, valley 18%, highland 3.5% system-wide) and a glyph
-- for the four dramatic landforms (mountain, canyon, crater, rift — each <=1.5%).

-- R2: the dramatic landforms on a WET body. Kepler is 90% plains with 7.7% highland
-- and ~2.6% mountain/canyon/crater/rift, so this is the sparse-glyph case — the
-- glyphs should read as scattered accents, not as a rash over the map.
verify.goto_surface("Kepler")
verify.set_zoom(1)
verify.set_overlay("none")
verify.capture("landform_kepler_wide")

-- Zoomed in far enough to read an individual silhouette: twin peaks (mountain),
-- split rims (canyon), a flattened bowl (crater), a jagged fissure (rift). This is
-- the capture that shows they are distinguishable from EACH OTHER, not merely
-- present.
verify.set_zoom(4)
verify.capture("landform_kepler_zoom")

-- R4: the relief tint alone. Kepler generates ZERO valley tiles (its ocean takes all
-- the low ground), so the wet-body relief read is entirely plains-vs-highland — the
-- subtlest case, and the one where the tint most risks being mistaken for a change
-- of composition rather than a change of elevation.
verify.set_zoom(2)
verify.capture("landform_kepler_relief")

-- R4 again on a DRY body. Cinder is 24.7% valley and 71.5% plains, so the sunken
-- half of the relief scale actually appears here; on Kepler it never fires. Together
-- the two bodies exercise both directions of the lift/sink scale.
--
-- Cinder is NOT the home body, so it opens unsurveyed and the survey mask blanks
-- every tile — without this reveal the captures below show a dark grid and verify
-- nothing. Clamped to full.
verify.set_survey("Cinder", 99999)
verify.goto_surface("Cinder")
verify.set_zoom(1)
verify.capture("landform_cinder_wide")

verify.set_zoom(4)
verify.capture("landform_cinder_zoom")

-- R3: the channel must SURVIVE A LENS. Composition owns hue and lens tints composite
-- over it at 0.6-0.80 alpha, so a landform signal carried in the fill would be buried
-- exactly when a lens is on. Relief is composited after the tint and the glyphs are
-- drawn over it; both should still read here. Continent is the most saturated lens
-- (0.80) and therefore the hardest case.
verify.set_overlay("continent")
verify.capture("landform_cinder_lens_continent")

verify.set_overlay("country")
verify.capture("landform_cinder_lens_country")
