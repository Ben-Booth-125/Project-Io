-- Visual verification for the intra-body vision model (BL-151/152/154).
-- Descends to the home planet (Kepler, fully surveyed, player buildings present):
--   BL-151/154: the surface reads mostly UNKNOWN, lit only in permanent pockets around
--           the player's buildings + 3-wide corridors from the corp centre of operation
--           to each market centre it operates in; everything else takes a dark wash.
--   BL-152/154: a live player intra-body convoy lights a radius-2 beam with a bright
--           head that glides along its route and a tail that dims one econ tick behind.
--           (A still shows lit pockets; the head/tail MOTION needs a live inter-market
--           intra-body convoy and is confirmed in the running app, not this capture.)

verify.goto_surface("home")
verify.capture("intrabody_fog_default")

-- Zoom out to see the whole body: only the HQ pocket is lit, the rest reads fogged.
verify.set_zoom(0.35)
verify.capture("intrabody_fog_wide")

-- Seed a live player convoy on the home body and run one econ step so the vision-beam
-- update (app::step_economy -> ui::update_convoy_vision) lights the beam pocket.
verify.seed_convoy("home", "home", "iron_ore", 20, 0.4)
verify.econ_step(1)
verify.capture("intrabody_fog_convoy_beam")
