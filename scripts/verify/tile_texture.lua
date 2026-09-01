-- BL-520 (basic tile texturing) — the substrate grain and the cover pattern.
--
-- The item's resolution: TEXTURE the substrate (which BL-511's province blend
-- already averages across a province, so its texture must not assert a tile
-- boundary) and PATTERN the cover (which is per tile and must read as per tile —
-- "a forest edge is information; a rock-to-rock seam is not"). Procedural marks
-- drawn from ImDrawList primitives, not an atlas: the project ships no art assets
-- and this is the hand-drawn vector idiom ICONS.md already establishes.
--
-- WHAT EACH CAPTURE IS FOR is written above it. The three claims the whole check
-- exists to test:
--   R1  the two passes are distinguishable — cover reads per tile, grain does not
--   R2  the pattern scales with cover_density (thin wood looks thin)
--   R3  texture survives every lens, attenuated, as depth rather than as dirt
--   R4  the LOD bound fires: no texture at all below 14 px of drawn circumradius
--
-- NOTE ON ZOOM: the pass keys on `draw_r = hex_size * zoom - 1`, and `hex_size` is
-- fitted to the window height, so the zoom at which texture appears depends on the
-- capture window. The ladder below is deliberately wide enough to bracket the
-- threshold on any reasonable window; read the ladder as a ramp, not as a single
-- pass/fail frame. `window()` is pinned first so the ladder is reproducible.
verify.window(1600, 1000)

-- ---------------------------------------------------------------------------
-- R1 / R2 — the plain default, home body, zoomed in far enough to read a mark.
-- ---------------------------------------------------------------------------
-- Kepler is the wet body: sedimentary ground under grass / scrub / forest / marsh,
-- which is where the density scale actually varies. Expect canopy ticks over the
-- forest, three-blade tufts over grass, the same canopy silhouette at ~60% over
-- scrub (so a forest line GRADES rather than snapping), and stacked level dashes
-- over marsh. The substrate's own bedding strokes should be visible but quiet.
verify.goto_surface("home")
verify.set_overlay("none")
verify.set_zoom(4)
verify.capture("texture_home_zoom")

-- The density read, framed tight. At full strength the pattern is
--   marks = clamp(1 + round(f*4), 1, 5),  alpha = 0.35 + 0.45*f,  f = density/255
-- so a sparse cover carries 2 marks at alpha 123/255 and a closed canopy 5 marks
-- at 204/255. Two neighbouring tiles of the same cover at different densities must
-- be distinguishable from each other — that is the whole argument for scaling the
-- pattern with the scalar the economy already reads.
verify.set_zoom(6)
-- FIXTURE (re-aimed 2026-08-22, NR-502): (89,10) landed on ocean and ice after
-- BL-515/BL-516 repartitioned the world, so this frame tested nothing while the
-- check still reported green. (40,30) is the tile province_render.lua frames and
-- is confirmed biotic land. Hardcoded fixtures WILL break on the next repartition.
verify.center_tile(40, 30, 6)
verify.capture("texture_density_close")

-- R1's negative half: the grain must NOT draw a seam. A run of same-substrate
-- tiles inside one province should still read as one shape (BL-511's blend), with
-- the grain adding material and not an outline. If tile edges reappear here, the
-- grain alpha is too high or its marks are too large.
-- FIXTURE (re-aimed 2026-08-22, NR-502): (76,12) was pure open ocean, which draws
-- NO texture by design — so the R1-negative claim (grain must not draw a seam) was
-- being asserted over water that could never show one. Re-aimed onto the same
-- confirmed land mass, offset so the frame is a run of ground rather than a coast.
verify.center_tile(37, 28, 8)
verify.capture("texture_grain_no_seam")

-- Ocean draws NOTHING, on purpose — a flat sea is a correct reading of open water,
-- and any mark on it would be read as the animated water this item excludes.
verify.center_tile(20, 30, 8)
verify.capture("texture_ocean_flat")

-- ---------------------------------------------------------------------------
-- R4 — the LOD ramp. texture_lod_scale: 0 at r <= 14, 0.5 at r = 18, 1 at r >= 22.
-- ---------------------------------------------------------------------------
-- A sampled pattern on a small hex is worse than invisible, it is moire: adjacent
-- tiles' marks beat against the pixel grid and the map crawls. So the pass has its
-- own bound, stricter than BL-269's 7 px coarse-fill threshold. Down the ladder the
-- texture should fade smoothly and then be gone — never pop, never shimmer.
verify.set_overlay("none")
verify.goto_surface("home")
verify.set_zoom(3)
verify.capture("texture_lod_z3")
verify.set_zoom(2)
verify.capture("texture_lod_z2")
verify.set_zoom(1)
verify.capture("texture_lod_z1")
-- The whole-grid view, which is also where BL-269's coarse rect fill takes over.
-- Expect ZERO texture here: this frame must look exactly like the pre-BL-520 one.
verify.set_zoom(0.9)
verify.capture("texture_lod_wholegrid")

-- ---------------------------------------------------------------------------
-- R3 — texture under the lenses. THE OPEN QUESTION THIS ITEM CARRIED.
-- ---------------------------------------------------------------------------
-- Decision taken: texture SURVIVES every lens at 0.45 strength, and each mark takes
-- its ink from the tile's own drawn fill rather than from a fixed palette entry —
-- so under the Country lens a mark is that nation's colour darkened, reading as
-- shading ON the block instead of dirt over it. Continent (0.80 tint) and Country
-- (direct replacement) are the two hardest cases and are both captured.
--
-- What would falsify the decision: if the marks read as speckle/noise unrelated to
-- the lens colour, or if they blur the categorical boundary the lens is about.
verify.set_zoom(4)

verify.set_overlay("continent")
verify.capture("texture_lens_continent")

verify.set_overlay("resource")
verify.capture("texture_lens_resource")

-- ---------------------------------------------------------------------------
-- The dry / airless covers, which Kepler never generates.
-- ---------------------------------------------------------------------------
-- Cinder exercises the non-biotic half of the cover vocabulary — dunes (a windward
-- crest), ash and snow (a stipple: both are FALL, not a growth form), salt (a
-- crossed crust tick) — and the rocky / regolith / volcanic / metallic grains.
-- Cinder is not the home body, so it opens unsurveyed; without this reveal every
-- capture below is a dark grid.
verify.set_overlay("none")
verify.set_survey("inner", 99999)
verify.goto_surface("inner")
verify.set_zoom(4)
verify.capture("texture_cinder_zoom")

verify.set_zoom(6)
verify.capture("texture_cinder_close")

-- The survey mask itself: texture is gated on `revealed`, because a cover pattern
-- IS terrain information and drawing it through the mask would leak the shape of
-- ground the player has not paid to survey. Re-lock and confirm the locked tiles
-- carry no marks at all.
verify.set_survey("inner", 0)
verify.goto_surface("inner")
verify.set_zoom(4)
verify.capture("texture_unsurveyed_blank")
