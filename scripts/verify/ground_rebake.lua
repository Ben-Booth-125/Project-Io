-- BL-1241 (structures baked) — requirement group `structures-baked` row R3:
-- the chunk content hash folds installations and NOTHING tick-rate, so a
-- re-bake follows a construction event, never a tick. Steps the sim one day
-- at a time with a frame per day (under --verify every bake is synchronous)
-- and counts re-bakes against the days the installation digest moved.
--
-- A play year is 365 days; a Debug econ tick on the reference world runs
-- seconds and the background firms roughly double the building count in a
-- month, so the default span is 30 days. Pass a longer one by editing `days`.
verify.goto_surface("home")
verify.set_overlay("none")
local g = verify.stage_gallery()
-- Re-bakes follow construction events, never ticks: step the sim a span with a
-- frame per day and count chunk re-bakes against the days the installation
-- digest moved. A re-bake on a day nothing was built, razed or rescaled would
-- mean something tick-rate leaked into the chunk hash.
verify.center_tile(g.col, g.row, 5)
verify.frames(3)
local days = 30
local s0 = verify.ground_stats()
local prev = s0.installations
local moved_days, rebake_days, leak_days = 0, 0, 0
for d = 1, days do
    local before = verify.ground_stats()
    verify.econ_step(1)
    verify.frames(1)
    local after = verify.ground_stats()
    local moved = after.installations ~= prev
    local rebaked = (after.chunk_rebakes - before.chunk_rebakes) + (after.far_bakes - before.far_bakes) > 0
    if moved then moved_days = moved_days + 1 end
    if rebaked then rebake_days = rebake_days + 1 end
    if rebaked and not moved then leak_days = leak_days + 1 end
    prev = after.installations
end
local s1 = verify.ground_stats()
print(string.format("structures rebake: %d days, installations moved on %d, re-baked on %d (%d without an installation move); chunk re-bakes %d, far bakes %d, buildings %d -> %d",
    days, moved_days, rebake_days, leak_days, s1.chunk_rebakes - s0.chunk_rebakes,
    s1.far_bakes - s0.far_bakes, s0.buildings, s1.buildings))
verify.expect(leak_days == 0, "no chunk re-baked on a day no installation moved (nothing tick-rate in the hash)")
