-- Visual verification for the built-tile render (Ben's 2026-07-22 call: "ensure that
-- tiles do not get rendered behind buildings, they are fully swapped out").
--
-- A built tile fills with an owner-coloured plate instead of its terrain colour and
-- carries a large silhouette; the terrain no longer shows through around a small glyph.
--
-- WHY THIS SCRIPT EXISTS. Every pre-existing canvas golden PASSED that change at
-- 0.02-0.09% differing, because they all frame the whole body at ~4px per tile — far
-- too small for the fill of a handful of built tiles to move the diff past the 0.5%
-- threshold. The suite was blind to how a built tile actually renders. This check
-- zooms in far enough that the plate, the silhouette and the owner emblem each occupy
-- real pixels, so a regression in any of them fails the diff.
--
-- Frames a rival cluster (extraction sites at (156,37)/(156,36)/(155,37) and a
-- processing facility at (155,36)) — four adjacent built tiles of two different types,
-- which also proves the per-type silhouettes stay distinguishable.
--
-- Run with: ProjectIo --verify scripts/verify/built_tile_render.lua

verify.econ_step(4)
verify.goto_surface("home")
verify.show_panel("economy", false)

-- Plain canvas: the plate and silhouette with no lens competing for the fill.
verify.center_tile(156, 37, 44)
verify.capture("built_tile_plain")

-- A blend lens still tints a built tile — a lens the player chose must not go blind
-- exactly where their assets are.
verify.set_overlay("resource")
verify.capture("built_tile_resource")

-- Population deliberately REPLACES the silhouette with a per-tile value mark
-- (BL-135). This frame is the regression guard on that suppression: the plate
-- stays, the silhouette must not come back. Opportunity shared the idiom and had
-- its own frame here until BL-604 retired it.
verify.set_overlay("population")
verify.capture("built_tile_population")
