-- One-off smoke test for the intra-body commercial-reach fog (BL-151).
-- Descends to the home planet (Kepler, fully surveyed, player buildings present)
-- and captures the surface: tiles whose catchment market the player reaches (owns a
-- building there, or a live convoy touches it) read full-bright; every other market's
-- catchment reads fogged (dark wash). Confirms the fog renders on the Planetary canvas.

verify.goto_surface("home")
verify.capture("intrabody_fog_default")

-- Zoom out to see the whole body: the player reaches only its own HQ catchment, so
-- the other market catchments across the globe should read fogged (dark wash).
verify.set_zoom(0.35)
verify.capture("intrabody_fog_wide")

-- Seed a live intra-body convoy between two markets on Kepler so a second catchment
-- lights up (reach via live convoy, not just building ownership).
verify.seed_convoy("Kepler", "Kepler", "iron_ore", 20, 0.4)
verify.capture("intrabody_fog_convoy")
