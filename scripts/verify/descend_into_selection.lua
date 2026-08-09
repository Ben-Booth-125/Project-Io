-- BL-165: selection-aware descend. On the Solar view, selecting a body and then
-- issuing 'descend' (the Enter hotkey) must dive into THAT body's Planetary
-- surface via focus_on_entity — re-anchoring away from the previously-active
-- body — rather than a blind one-rung drop on the old anchor.
--
-- Anchor Kepler, climb to Solar, then select a DIFFERENT body (Cinder) and
-- descend: the final frame must be Cinder's Planetary surface, not Kepler's and
-- not the Circumplanetary rung.

verify.goto_surface("home")   -- active_body = Kepler, on Planetary
verify.command("ascend")        -- -> Circumplanetary
verify.command("ascend")        -- -> Solar (active_body still Kepler)
verify.capture("descend_sel_solar")

verify.select_body("inner")    -- single-click-select a different body
verify.command("descend")       -- selection-aware dive into Cinder's surface
verify.capture("descend_sel_planetary")
