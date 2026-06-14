-- Visual verification for the Corporation lens.
-- Confirms REQUIREMENTS.md § corporation-lens R2-R6:
--   R2/R3 the lens glyph + button in the bottom-left strip (in every capture)
--   R4    corporate tiles tint to their corp colour
--   R5    player-corp tiles get a faction_colour(0) border
--   R6    tiles with no corporate building stay terrain-coloured
--
-- Driver: direct state manipulation (the `verify` API writes ui_state). Captures
-- land in screenshots/<name>.png. Run with: ProjectIo --verify scripts/verify/corporation_lens.lua

verify.goto_surface("home")

-- Log where the corporate buildings sit so the pan/zoom below can be aimed.
verify.log_buildings()

-- Full-planet framing under each overlay, for comparison.
verify.set_overlay("none")
verify.capture("corp_lens_full_00_terrain")

verify.set_overlay("faction")
verify.capture("corp_lens_full_01_faction")

verify.set_overlay("corporation")
verify.capture("corp_lens_full_02_corporation")

-- Zoomed onto the player corp building at grid (22,82): expect a player tint
-- (faction_colour(0)), surrounded by terrain-coloured (untinted) tiles. Pan is in
-- screen px (pan = (grid_centre - tile_local) * zoom), confirmed against capture.
verify.set_zoom(8)
verify.set_pan(4926, -2551)
verify.capture("corp_lens_zoom_player_22_82")

-- Same player tile, zoomed further in so the tint reads clearly.
verify.set_zoom(20)
verify.set_pan(12315, -6378)
verify.capture("corp_lens_zoom_player_close")

-- Zoomed onto a rival corp building at grid (42,63): expect a DIFFERENT corp tint
-- from the player's (a hashed slot, never slot 0). Zoom set explicitly so it does
-- not inherit the close-up's zoom.
verify.set_zoom(20)
verify.set_pan(8590, -3384)
verify.capture("corp_lens_zoom_rival_close")
