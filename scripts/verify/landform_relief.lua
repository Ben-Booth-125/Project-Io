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

-- BL-232: spanning markers, and the tooltip that makes the glyph vocabulary
-- learnable. A run of contiguous mountains draws ONE chain of peaks rather than a
-- repeated icon per tile; a lone tile keeps its centred glyph, the same role the
-- road's centre cap plays.
--
-- Coordinates come from world_audit § S4's exemplar readout, not from eyeballing a
-- screenshot — if a generation change moves these runs, the harness prints the new
-- ones instead of these captures quietly becoming pictures of empty ground.
verify.goto_surface("Kepler")
verify.set_overlay("none")

-- Mountain run at [89,10] vs the lone mountain at [76,12]: the span-vs-cap contrast
-- is the whole point of the item, so both are framed tight enough to compare.
verify.center_tile(89, 10, 6)
verify.capture("landform_span_mountain_run")

verify.center_tile(76, 12, 6)
verify.capture("landform_span_mountain_lone")

-- Rift at [157,42] — the strongest contiguity of the three (81% connected) and the
-- one whose real-world referent most obviously IS a line.
verify.center_tile(157, 42, 6)
verify.capture("landform_span_rift_run")

-- Canyon at [143,38] — the marginal case: 50% isolated, 24 tiles system-wide, so
-- half its glyphs stay caps and the spanning read is thin. Captured so that
-- thinness is on the record rather than assumed away.
verify.center_tile(143, 38, 6)
verify.capture("landform_span_canyon_run")

-- THE TOOLTIP (Ben, on delivery: "there is no easy way for a player to know what is
-- what"). Every tile hover named the composition and never the landform, so the
-- glyphs had no label anywhere short of clicking through to the Selection panel.
-- Hovering the mountain run should now name it AND give its real consequence.
-- capture() composits a single frame, and the hover card is gated on
-- kHoverAppearDelay = 30 frames (BL-230's glance-then-stick), so the cursor must sit
-- still for a while before the card exists. verify.frames (BL-228) is what makes that
-- reachable from a script; without it the capture shows only the hover ring.
verify.hover_tile(89, 10)
verify.frames(40)
verify.capture("landform_tooltip_mountain")

verify.hover_tile(157, 42)
verify.frames(40)
verify.capture("landform_tooltip_rift")
