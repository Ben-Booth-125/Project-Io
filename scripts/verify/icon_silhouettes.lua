-- Verify icon silhouette redraws and outline convention (BL-002+003 / icon-silhouettes-outlines R1).
-- Tours all corporation buildings on the Planetary surface to show:
-- - extraction sites: faceted ore-chunk silhouette
-- - processing facilities: distinct shape with dark outline
-- Also captures a Supply-lens view to show convoy (open V chevron) markers.
verify.goto_surface("home")
verify.set_overlay("corporation")

-- Close-framed tour of all buildings to inspect silhouettes and outlines
tour_buildings(20)

-- Wider view to show building markers in context
verify.set_zoom(4)
verify.capture("icon_silhouettes_overview")

-- Supply lens to check convoy glyphs (if any convoys are present)
verify.set_overlay("supply")
verify.capture("icon_silhouettes_supply_lens")
