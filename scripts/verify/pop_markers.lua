-- Visual verification for Population-centre markers (BL-083).
-- Confirms requirements.json § pop-centre-markers R1 (item-spanning):
--   the generated population centres render as always-on, tiered settlement
--   markers on the Planetary surface (clustered into conurbations, City+ labelled),
--   civic-neutral by default and host-nation-tinted only under the Country lens.
--
-- Kepler (home) carries the prototype population. Run with:
--   ProjectIo --verify scripts/verify/pop_markers.lua

verify.econ_step(2)                     -- settle
verify.show_panel("economy", false)     -- clear the auto-opened panel off the capture
verify.goto_surface("home")

-- Always-on: markers show at the plain default lens (no lens armed).
verify.set_overlay("none")
verify.capture("pop_markers_default")

-- Zoom onto a populated region so tiers + City+ labels read at scale.
frame_tile(66, 6, 9)
verify.capture("pop_markers_zoom")

-- Country lens: the same markers pick up the host-nation tint (R4).
frame_tile(66, 6, 9)
verify.set_overlay("country")
verify.capture("pop_markers_country_tint")
