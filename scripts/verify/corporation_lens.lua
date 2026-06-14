-- Visual verification for the Corporation lens.
-- Confirms REQUIREMENTS.md § corporation-lens R2-R6:
--   R2/R3 the lens glyph + button in the bottom-left strip (in every capture)
--   R4    corporate tiles tint to their corp colour
--   R5    player-corp tiles get a faction_colour(0) border
--   R6    tiles with no corporate building stay terrain-coloured
--
-- Driver: direct state manipulation (the `verify` API writes ui_state). Captures
-- land in screenshots/<name>.png. Uses the shared lib.lua helpers (auto-loaded by
-- the harness), so there is no hand-computed pan math here.
-- Run with: ProjectIo --verify scripts/verify/corporation_lens.lua

verify.goto_surface("home")

-- Full-planet framing under each overlay (terrain / faction / corporation), for
-- comparison. Produces corp_lens_full_00_none / _01_faction / _02_corporation.
sweep_overlays("corp_lens_full")

-- Zoomed onto the player corp building at grid (22,82): expect a player tint
-- (faction_colour(0)), surrounded by terrain-coloured (untinted) tiles.
verify.set_overlay("corporation")
frame_tile(22, 82, 8)
verify.capture("corp_lens_zoom_player_22_82")

-- Same player tile, zoomed further in so the tint reads clearly.
frame_tile(22, 82, 20)
verify.capture("corp_lens_zoom_player_close")

-- Zoomed onto a rival corp building at grid (42,63): expect a DIFFERENT corp tint
-- from the player's (a hashed slot, never slot 0).
frame_tile(42, 63, 20)
verify.capture("corp_lens_zoom_rival_close")
