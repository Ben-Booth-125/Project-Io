-- Visual verification for the per-building profitability readout (BL-074).
-- Runs economy ticks so the report + market prices populate, then selects a PLAYER
-- building and captures the Selection bar, whose Section B now shows the estimated
-- net per-tick contribution and its component lines (revenue / inputs / wages /
-- maintenance). Realised last-tick figures; revenue is an estimate (pooled market).
-- Driver: verify.select_building (same player building visibility.lua uses).
-- Run with: ProjectIo --verify scripts/verify/building_profit.lua

verify.econ_step(6)            -- populate building run states and market prices
verify.goto_surface("home")    -- Kepler: fully surveyed, player buildings visible

-- A player building: the Selection bar shows the BL-074 profitability breakdown.
verify.select_building(70, 38)
verify.capture("building_profit_player")
