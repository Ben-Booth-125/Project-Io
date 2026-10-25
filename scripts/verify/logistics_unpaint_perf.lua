-- What arming the Logistics lens costs the FRAME, on the play path (Ben, 2026-10-25:
-- "keep the 60 fps budget"; RENDERING.md § Roads and sea lanes, "The Logistics lens
-- takes the routes off the ground").
--
-- NOT a golden check and it asserts nothing: it prints. Run it on the POOL path, where
-- the re-bake runs behind the frame as it does in play:
--   IO_GROUND_BENCH=1 SDL_RENDER_DRIVER=software ProjectIo --verify scripts/verify/logistics_unpaint_perf.lua
-- (On the synchronous --verify path every moved chunk is baked inside one frame, which
-- is what logistics_chain.lua's ROAD_HIDE lines time instead.)
--
-- Per rung (close, mid): the whole master is baked first; then the lens is armed and
-- 240 frames run - the UNPAINT_PERF line gives the worst and mean frame, the frames
-- and wall ms until the view is final again, and the re-bake count. Then disarmed,
-- the same. CSVs: unpaint_<rung>_<n>_<lens>.csv (frame_csv columns); a Market toggle first is the control.

local function poll_final(limit)
  local t0 = verify.ground_stats().clock_ms
  for i = 1, limit do
    verify.frames(1)
    local s = verify.ground_stats()
    if s.view_final then return i, s.clock_ms - t0 end
  end
  return -1, verify.ground_stats().clock_ms - t0
end

verify.window(1720, 1080)
verify.goto_surface("home")
verify.set_overlay("none")
local r = verify.ground_complete_master(900)
print(string.format("UNPAINT_PERF master %d/%d complete in %.0f ms (pool path %s)",
      r.ready, r.total, r.ms, tostring(verify.ground_stats().pool_path)))

local seq = 0
local cs = verify.capital_stars()
local cx, cy = cs.x or 0, cs.y or 0
for _, rung in ipairs({ { "close", 14 }, { "mid", 5 } }) do
  frame_tile(cx, cy, rung[2])
  poll_final(4000)
  verify.frames(10)
  -- The Market toggle is the CONTROL: a lens that re-bakes nothing, so what its frames
  -- cost is what any lens toggle costs (the stroke cache re-keys on the overlay).
  for _, lens in ipairs({ "market", "none", "logistics", "none" }) do
    local c0 = verify.ground_bake_counters()
    verify.frame_reset()
    local t0 = verify.ground_stats().clock_ms
    verify.set_overlay(lens)
    -- Settled = the view is final and no re-bake has landed for 60 frames; the time of
    -- the LAST landing is when the ground under the view finished changing.
    local last_n, last_t, quiet, frames = c0.chunk_rebakes, t0, 0, 0
    while frames < 6000 do
      verify.frames(1)
      frames = frames + 1
      local c = verify.ground_bake_counters()
      local s = verify.ground_stats()
      if c.chunk_rebakes ~= last_n then
        last_n, last_t, quiet = c.chunk_rebakes, s.clock_ms, 0
      else
        quiet = quiet + 1
      end
      if quiet >= 60 and s.view_final then break end
    end
    seq = seq + 1
    local csv = string.format("unpaint_%s_%d_%s.csv", rung[1], seq, lens)
    verify.frame_csv(csv)
    local c1 = verify.ground_bake_counters()
    local g = verify.ground_stats()
    print(string.format("UNPAINT_PERF %s -> %s: draw_r %.1f px; last re-bake landed %.0f ms after the toggle "
          .. "(%d frames run); re-bakes %d, worker ms %.0f (csv %s)", rung[1], lens, g.draw_r,
          last_t - t0, frames, c1.chunk_rebakes - c0.chunk_rebakes, c1.rebake_ms - c0.rebake_ms, csv))
  end
end
