-- Visual verification for the intra-body activity fog + convoy vision beam (BL-151/152).
-- Descends to the home planet (Kepler, fully surveyed, player buildings present):
--   BL-151: the surface reads mostly UNKNOWN, lit only in a tight pocket around the
--           player's presence (own building tiles); everything else takes a dark wash.
--   BL-152: a live player convoy lights a radius-2 beam of vision along its route,
--           which then dims over one econ tick. (Static capture shows the lit pocket;
--           the fade is temporal and not visible in a still.)

verify.goto_surface("home")
verify.capture("intrabody_fog_default")

-- Zoom out to see the whole body: only the HQ pocket is lit, the rest reads fogged.
verify.set_zoom(0.35)
verify.capture("intrabody_fog_wide")

-- Seed a live player convoy on the home body and run one econ step so the vision-beam
-- update (app::step_economy -> ui::update_convoy_vision) lights the beam pocket.
verify.seed_convoy("Kepler", "Kepler", "iron_ore", 20, 0.4)
verify.econ_step(1)
verify.capture("intrabody_fog_convoy_beam")
