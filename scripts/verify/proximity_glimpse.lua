-- Visual verification for the BL-099 proximity-glimpse peek (commercial-fog deferral 2 of 2).
-- A player convoy completing a lane faintly lights a FRONTIER body it merely passes near --
-- not an endpoint, and with no commerce of its own. Kepler->Pallas is a long cross-system
-- lane; Cinder sits ~on that segment, so completing the lane glimpses Cinder into the fog at
-- the faint known_stale tier while it holds no route of its own.
--
-- The tier logic + geometry + determinism are covered exhaustively by commercial_fog_harness
-- (R2, authored positions). This is the on-canvas confirmation (R1), inspected by-eye as a
-- before/after pair zoomed onto the inner system so the faint glimpse badge on Cinder reads.
-- Run with: ProjectIo --verify scripts/verify/proximity_glimpse.lua

-- Ascend to the Solar canvas, close the ledger, and zoom onto the inner system (Helios/Cinder).
verify.command("ascend")
verify.command("ascend")
verify.show_panel("economy", false)
verify.command("zoom_in")
verify.command("zoom_in")
verify.command("zoom_in")
verify.command("zoom_in")
verify.command("zoom_in")
verify.command("zoom_in")

-- Baseline: before any player commerce, Cinder is outside the network -> a plain astronomy
-- dot with no activity badge.
verify.capture("proximity_glimpse_before")

-- Complete a long cross-system player lane Kepler->Pallas (a near-complete convoy; one econ
-- step arrives + credits it). Cinder lies close to that segment, so it is glimpsed to the
-- faint known_stale tier -- lit without a route of its own.
verify.seed_convoy("Kepler", "Pallas", "iron_ore", 20, 0.98)
verify.econ_step(1)
verify.show_panel("economy", false)
verify.capture("proximity_glimpse_after")
