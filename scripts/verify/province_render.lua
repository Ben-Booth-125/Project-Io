-- Visual verification for BL-511 — the province as the rendered and selected unit.
--
-- The item's ruling: rendering and selection move to province grain; the ~4-tile
-- province from BL-466 is unchanged in size; the TILE is not retired (deposits,
-- terrain and buildings stay tile-keyed); and terrain reads as a BLENDED GRADIENT
-- across the province's real tile mixture — not a dominant composition, not a
-- texture pattern.
--
-- What each capture is evidence for:
--   P1 the plain Planetary canvas at play zoom — the seams INSIDE a province are
--      gone and the cells read as soft shapes with a faint edge, rather than as a
--      uniform hex lattice
--   P2 the same view zoomed in — the gradient across a single province's real
--      mixture is visible, and no province is one flat colour
--   P3 a SELECTED province — the selection outline traces the cell's outer
--      boundary (never its interior seams), and the Selection band shows the
--      province card: mixture bar, member tiles, deposits, buildings
--   P4 far zoom — the fill LOD still fires; province edges drop out rather than
--      turning into 1 px noise on a 7 px hex
--   P5..P9 the per-lens reduction table (PLANETARY.md § Province grain) actually
--      rendering. EVERY lens must still draw. Resource blends; Country and
--      Continent deliberately REFUSE to blend (categorical fields); Industry
--      reduces to a per-province SUM filled uniformly; Production refuses.
--
-- Run with: ProjectIo --verify scripts/verify/province_render.lua

verify.goto_surface("home")

-- Warm-up frame: the bottom-pivoted lens strip and the Selection band are only
-- positioned once ImGui has measured them, i.e. from the second frame.
verify.capture("province_warmup")

-- P1 — play zoom, no lens. The blend's home case.
verify.center_tile(40, 30, 2.0)
verify.capture("province_blend_play_zoom")

-- P2 — close in. One province should be a gradient, not a flat block.
verify.center_tile(40, 30, 6.0)
verify.capture("province_blend_close")

-- P3 — select a province. Outline traces the cell; the band shows the card.
local pid = verify.select_province(40, 30)
print("selected province id = " .. tostring(pid))
verify.capture("province_selected_card")

-- P3b — HOVER through the real hit-test. verify.select_province sets the state a
-- click produces; this instead moves the cursor and lets the canvas's own
-- distance-to-hex-centre hit-test resolve it, which is the load-bearing half of
-- "a press is reachable" (the click handler consumes exactly the hovered_tile the
-- hover pass resolved). Two frames: hovered_province is written after the tile
-- loop, so the outline it feeds lands on the following frame.
verify.clear_selection()
verify.mouse(700, 300)
verify.capture("province_hover_settle")
verify.capture("province_hover_outline")

-- P4 — far zoom: the coarse fill LOD, with province edges suppressed.
verify.center_tile(40, 30, 1.0)
verify.capture("province_far_zoom_lod")

-- P5..P9 — every lens still renders, with its stated reduction.
verify.center_tile(40, 30, 3.0)

verify.set_overlay("resource")     -- BLENDS (continuous spatial field)
verify.capture("province_lens_resource")

verify.set_overlay("country")      -- REFUSES (categorical: nation)
verify.capture("province_lens_country")

verify.set_overlay("continent")    -- REFUSES (categorical: plate)
verify.capture("province_lens_continent")

verify.set_overlay("industry")     -- PROVINCE SUM, filled uniformly
verify.capture("province_lens_industry")

verify.set_overlay("production")   -- REFUSES (sparse: value belongs to the building)
verify.capture("province_lens_production")

verify.set_overlay("market")       -- no reduction needed (catchment is coarser)
verify.capture("province_lens_market")

verify.set_overlay("corporation")  -- REFUSES (sparse: built tiles are outside the blend)
verify.capture("province_lens_corporation")

verify.set_overlay("population")   -- N/A: a per-tile dot mark, not a fill
verify.capture("province_lens_population")

verify.set_overlay("opportunity")  -- N/A: a per-catchment dot mark, not a fill
verify.capture("province_lens_opportunity")

verify.set_overlay("scarcity")     -- no reduction needed (catchment)
verify.capture("province_lens_scarcity")

verify.set_overlay("reach")        -- N/A: body-level, paints no tile fill
verify.capture("province_lens_reach")

verify.set_overlay("supply_routes")-- N/A: body-level edges
verify.capture("province_lens_supply_routes")

verify.set_overlay("supply")       -- N/A: per-tile glyph, not a fill
verify.capture("province_lens_supply")

verify.set_overlay("none")
verify.capture("province_lens_cleared")
