-- Export live economy data to CSV for external ledger mock-ups (e.g. Power BI).
--
-- Runs the deterministic seeded world forward a few dozen economy ticks so pools
-- fill, markets clear, balances diverge, and the player balance-trend series has
-- points, then writes a set of CSV tables to docs/ui/mockdata/.
--
-- Run: ProjectIo --verify scripts/verify/export_mockdata.lua
-- Output (docs/ui/mockdata/):
--   corporations.csv      corp_id,name,focus,home_nation,balance,starting_capital,since_start,buildings
--   stockpiles.csv        corp_id,corp_name,body_id,body_name,resource,quantity
--   markets.csv           body_id,body_name,resource,supply,demand,price,base_price
--   cashflow.csv          corp_id,corp_name,income,expenditure,maintenance,wages,interest,net
--   workforce.csv         corp_id,corp_name,body_id,body_name,staffing_pct
--   buildings.csv         building_id,corp_id,corp_name,body_name,type,output,active,exhausted
--   player_timeseries.csv tick,balance,income,expenditure

verify.goto_surface("Kepler")

-- ~10 game-years of quarterly economy ticks: enough for prices to move off their
-- cold-start values and the trend series to be chartable.
verify.econ_step(16)

verify.export_data("docs/ui/mockdata")

-- Textual echo of the same figures for a sanity check against the CSVs.
verify.dump_economy()
