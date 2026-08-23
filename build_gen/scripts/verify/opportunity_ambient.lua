-- Visual verification for the Ambient opportunity read (BL-086).
-- Confirms requirements.json § opportunity-ambient R1 (item-spanning):
--   the Opportunity lens's diverging profit->loss margin surface reads at rest,
--   with NO building type armed (construction mode off), and carries its on-canvas
--   profit->loss key. It is NOT auto-activated at campaign start (the canvas opens
--   unskinned per the BL-013 reversal); the player picks it like any lens.
--
-- Finding (2026-07-04): the shipped Opportunity lens (BL-017) already computes the
-- best-building margin per tile independent of construction state and already draws
-- draw_opportunity_key, so this item's scoped delta needed no new code — this script
-- pins that the ambient read + key hold. Run with:
--   ProjectIo --verify scripts/verify/opportunity_ambient.lua

verify.econ_step(4)                     -- settle prices so margins are live
verify.show_panel("economy", false)
verify.goto_surface("home")

-- Opportunity lens active with NO build armed: the margin surface + key must show.
verify.set_overlay("opportunity")
verify.capture("opportunity_ambient_full")

frame_tile(66, 6, 9)
verify.capture("opportunity_ambient_zoom")
