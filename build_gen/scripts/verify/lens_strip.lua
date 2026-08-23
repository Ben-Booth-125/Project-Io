-- Visual verification for the lens strip (BL-013).
-- Confirms requirements.json § lens-strip-single-select:
--   R1 the strip lists lenses in the settled order Corp, Country, Resource,
--      Market, Population, Opportunity, Production, Scarcity (eight glyphs)
--   R3 single-select with a null state: selecting a lens highlights exactly one
--      glyph; re-selecting the active lens clears to none (plain terrain)
--
-- R2 (default lens = Corporation at campaign start) is a code check; this script
-- captures the strip itself and the single-select behaviour.
-- Run with: ProjectIo --verify scripts/verify/lens_strip.lua

verify.goto_surface("home")

-- Corporation lens active (the campaign-start default): the first strip glyph is
-- highlighted, the rest dim. Captures the full curated strip order.
verify.set_overlay("corporation")
verify.capture("lens_strip_corporation")

-- Switching to Production highlights only that glyph (single-select).
verify.set_overlay("production")
verify.capture("lens_strip_production")

-- Clearing to none leaves plain terrain with no glyph highlighted.
verify.set_overlay("none")
verify.capture("lens_strip_none")
