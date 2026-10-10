-- BL-1218 (wharf placed on shore): the construction ledger offers a Fishing
-- Wharf on LAND beside the sea, and never on the water itself.
--
-- Ben's live click (2026-10-07): the Wharf was offered only on coast-substrate
-- WATER (a water tile bordering open ocean read as "coastal"), and pressing it
-- there failed "Cannot build on water". is_wharf_site now refuses water ground,
-- the Well's own rule, so the offer and the placement gate agree.
--
-- The tile coordinates below are the default verify world's (home body) and
-- were found with a one-off probe; if world generation moves, re-aim them.
--
--   (186, 90) coastal land, no produce deposit  -> "Fishing Wharf" row, valid
--   (107, 85) coast-substrate water             -> no Fishing Wharf row
--   ( 52, 86) coastal land WITH produce         -> a Farm, no Wharf (deposit wins)
--   (231, 83) fresh-water land                  -> "Well" row (unchanged rule)
--
-- Run:     ProjectIo --verify scripts/verify/wharf_on_shore.lua
-- Inspect: screenshots/wharf_on_shore_*.png

verify.window(1920, 1080)
verify.econ_step(4)
verify.goto_surface("home")

local function ledger_at(col, row, name)
    verify.select_tile(col, row)
    verify.capture(name .. "_select") -- a selection closes open panels; settle it
    verify.show_panel("build", true)
    verify.capture(name)
end

ledger_at(186, 90, "wharf_on_shore_land")
ledger_at(107, 85, "wharf_on_shore_water")
ledger_at(52, 86, "wharf_on_shore_farm_coast")
ledger_at(231, 83, "wharf_on_shore_well")

-- The press: queue the Wharf on the shore tile through the ledger seam and
-- capture the outcome line.
verify.select_tile(186, 90)
verify.capture("wharf_on_shore_press_select")
verify.show_panel("build", true)
verify.ledger_build("extraction", "agricultural_produce", "")
verify.capture("wharf_on_shore_press")
verify.capture("wharf_on_shore_pressed")
