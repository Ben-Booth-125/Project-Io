-- Visual verification for the Budget ledger's top-buildings-by-profit rank table
-- (BL-171). The default cold-verify player owns only a Port (no L3 production), so
-- the table is empty from a bare start; this script BUILDS a few extraction sites,
-- runs enough econ ticks for construction to finish and the economy to report, then
-- captures the Budget ledger with the rank table populated.
-- Run with: ProjectIo --verify scripts/verify/budget_ledger_ranked.lua

verify.goto_surface("home")
verify.set_balance(20000.0)   -- afford several buildings (cold verify has no warm-start)

-- Place extraction sites for a few different resources; each build_first_valid commits
-- on the first valid deposit tile via the real construct_building path.
verify.place_mode("extraction", "iron_ore")
verify.build_first_valid()
verify.place_mode("extraction", "water")
verify.build_first_valid()
verify.place_mode("extraction", "agricultural_produce")
verify.build_first_valid()

-- Run the economy long enough for construction to complete and buildings to report
-- (well past the rank-change window of 4 econ ticks).
verify.econ_step(24)
verify.show_panel("economy", false)

-- The last build left a building selected; capture once to settle that selection
-- (a new selection closes any column panel), then open the Budget ledger — otherwise
-- the pending selection would close it on the capture frame.
verify.capture("budget_ledger_ranked_built")
verify.show_panel("balance", true)
verify.capture("budget_ledger_ranked")
