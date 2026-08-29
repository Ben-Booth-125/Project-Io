-- Visual verification for Player presence (BL-085).
-- Confirms requirements.json § player-presence R1 (item-spanning):
--   an always-on home marker — a ring around the player's holdings cluster + an HQ
--   star on the origin building (Planetary), and a halo on home_body (Solar) —
--   distinct from the survey badge and selection/hover highlights.
--
-- The initial camera focus + ownership accent shipped earlier (start-framing) and
-- are not re-checked here. Run with:
--   ProjectIo --verify scripts/verify/player_presence.lua


-- Planetary: home-cluster ring + HQ star, plain lens (always-on chrome). No
-- goto_surface — keep the HQ framing setup_world queued, so the holdings cluster
-- fills the view and the ring/star read at scale (mirrors start_framing.lua).
verify.set_overlay("none")
verify.capture("player_presence_home_surface")

-- Ascend the zoom ladder to the Solar canvas for the home_body halo.
verify.command("ascend")
verify.command("ascend")
verify.capture("player_presence_solar_halo")
