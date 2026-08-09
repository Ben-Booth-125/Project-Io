-- Visual verification for the Commercial-sphere fog (BL-089).
-- Confirms requirements.json § commercial-fog R1 (item-spanning), Solar-canvas side:
--   the player's trade network lights the map — a completed route makes its far body
--   read 'known' (activity pulse badge, lower-left), a live lane makes its body
--   'visible', bodies with no player commerce stay 'unknown' (plain dot), and player
--   routes draw lit corridors. The badge is distinct from and offset from the survey
--   badge (upper-right). Home (Kepler) carries its BL-085 halo.
--
-- The tier logic itself is covered exhaustively by commercial_fog_harness (R2).
-- Run with: ProjectIo --verify scripts/verify/commercial_fog.lua

verify.show_panel("economy", false)

-- A near-complete player convoy to Cinder: one econ step arrives + credits it, which
-- upserts a persistent route -> Cinder reads 'known'.
verify.seed_convoy("home", "inner", "iron_ore", 20, 0.98)
verify.econ_step(1)
verify.show_panel("economy", false)

-- A live player convoy in transit to Pallas -> Pallas reads 'visible' (active lane).
verify.seed_convoy("home", "asteroid", "iron_ore", 20, 0.4)

-- Ascend the zoom ladder to the Solar canvas and capture the fog.
verify.command("ascend")
verify.command("ascend")
verify.capture("commercial_fog_solar")
