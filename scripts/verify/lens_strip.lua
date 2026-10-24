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

-- BL-1406: the Corporation and Company lenses retired with the firms; "who owns
-- what" is the Nations lens (LENSES.md § Corporation and Company lenses - retired).
-- The Resource lens active: the first strip glyph is highlighted, the rest dim.
-- Captures the full curated strip order.
verify.set_overlay("resource")
verify.capture("lens_strip_resource")

-- Switching to Population highlights only that glyph (single-select). This was
-- Production until BL-604 retired that lens; the check is about the highlight
-- being exclusive, so any glyph but the first serves.
verify.set_overlay("population")
verify.capture("lens_strip_population")

-- Clearing to none leaves plain terrain with no glyph highlighted.
verify.set_overlay("none")
verify.capture("lens_strip_none")
