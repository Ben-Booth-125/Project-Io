-- Visual verification for the tile construction ledger (BL-162).
-- The tile-contextual surface that actually lets the player build: a list of every
-- building type placeable on the selected tile, each with a placeholder image, its
-- full cost, a reason-coded validity read, and a Build action.
-- Run with: ProjectIo --verify scripts/verify/tile_build_ledger.lua
--
-- NOTE: a new tile selection closes any open column panel (app close_all_panels), so
-- the build flag must be set on a LATER frame than the selection. Each capture is one
-- frame, so we capture once to settle the selection, then open the ledger + capture.

verify.econ_step(4)                    -- populate market state (build-cost context)
verify.goto_surface("home")            -- Kepler surface, surveyed

-- A land tile with deposits: extraction options per deposited resource, plus the
-- fixed processing / port / launchpad types with their validity reasons.
verify.select_tile(57, 34)
verify.capture("tile_build_ledger_land_select")  -- settle the selection
verify.show_panel("build", true)
verify.capture("tile_build_ledger_land")

-- A water tile: extraction differs, and terrain-gated types (no building on water)
-- show their reason-coded rejection.
verify.select_tile(66, 6)
verify.capture("tile_build_ledger_water_select") -- settle the selection
verify.show_panel("build", true)
verify.capture("tile_build_ledger_water")
