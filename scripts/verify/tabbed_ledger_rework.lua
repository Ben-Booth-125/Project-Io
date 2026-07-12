-- Tabbed-ledger header rework + tile Manage ledger (this session's steer).
-- Not golden-diffed — a narrated capture sequence for eyeballing:
--   * the full-width, ~2x tab strips on the Building / Economy / Market ledgers,
--   * the Market ledger's Prices/Sell Orders header sitting ABOVE the body/market
--     selectors, and
--   * the tile-scoped Manage ledger reached from a building's "Manage" action.
-- Run: ProjectIo --verify scripts/verify/tabbed_ledger_rework.lua
-- Inspect: screenshots/tabs_*.png, screenshots/*manage*.png

verify.goto_surface("home")
verify.econ_step(4)                 -- give the ledgers live data to render
verify.show_panel("economy", false) -- econ_step force-opens it; close for a clean read

-- 1) Building ledger — "Construction" / "Buildings" tabs span the header at ~2x.
verify.show_construction()
verify.capture("tabs_building")

-- 2) Economy ledger — "Corps" / "Holdings" / "Markets".
verify.show_panel("construction", false)
verify.show_panel("economy", true)
verify.capture("tabs_economy")

-- 3) Market ledger — "Prices" / "Sell Orders", now ABOVE the Body/Market selectors.
verify.show_panel("economy", false)
verify.show_panel("market", true)
verify.capture("tabs_market")

-- 4) Tile-scoped Manage ledger: place a processing facility, select it, open Manage.
verify.show_panel("market", false)
verify.set_balance(2000.0)
verify.show_construction()
verify.place_mode("processing")
local r = verify.build_first_valid()
verify.expect(r == "placed", "processing facility placed; got: " .. r)

-- A capture here settles the new-selection close_all_panels (build_first_valid changed
-- the selection to the new building) so the next frame does not clear show_manage_ledger.
-- It also shows the building Selection panel carrying the "Manage building" front door.
verify.capture("pre_manage_selection")

verify.show_panel("manage", true)
verify.capture("manage_ledger")
