-- Visual verification for the two BL-162 parts a static read cannot settle:
--
--   R6  the construction ledger SURVIVES the build it initiated. app.cpp used to
--       reselect the new building on construction_result::placed, which made the
--       selection non-tile, which forced show_build_ledger false — the ledger
--       vanished, "Construction started." (drawn inside it) was never seen, and
--       placing a second site on a rich tile meant reselecting the tile.
--
--   R7  Processing enumerates ONE ROW PER RECIPE, and the row's recipe survives to
--       construct_building. verify.ledger_build writes exactly the fields the
--       ledger's Build button writes (pending_tile / type / target / recipe), so
--       the request drains through app::render's real construction path — unlike
--       build_first_valid / build_at, which bypass that seam entirely.
--
-- Run with: ProjectIo --verify scripts/verify/tile_build_ledger_survives.lua

verify.econ_step(4)                    -- populate market state (prices the bars + capex)
verify.show_panel("economy", false)
verify.goto_surface("home")
verify.set_balance(500000.0)           -- afford every candidate, so no row is gated

-- The same land tile the sibling check uses: deposits present, so extraction rows
-- and the three processing rows all enumerate.
verify.select_tile(57, 34)
verify.capture("ledger_survives_00_select")   -- settle the selection
verify.show_panel("build", true)
verify.capture("ledger_survives_01_open")     -- the ledger, before any build

-- Build the FOOD RATIONS processing row. Steel is what construct_building seeds by
-- default, so choosing a different recipe is what proves the seam carries it.
verify.ledger_build("processing", "iron_ore", "food_rations")
verify.frames(2)                              -- let app::render drain the request
verify.capture("ledger_survives_02_after_build")

-- Still the same tile, still the ledger: place a SECOND building without reselecting.
verify.ledger_build("extraction", "agricultural_produce", "")
verify.frames(2)
verify.capture("ledger_survives_03_second_build")

verify.log_buildings()
