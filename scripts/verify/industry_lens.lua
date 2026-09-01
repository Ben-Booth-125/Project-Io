-- Visual verification for the Industry lens (BL-084; re-pointed BL-373).
-- Confirms requirements.json § industry-lens R1 (item-spanning):
--   overlay_mode::industry tints the Planetary surface by the BACKGROUND FIRMS'
--   plant standing on each tile — "where is the industry I did not build?" —
--   distinct from Production, which ranks output including the player's own
--   holdings. Pure rendering; no economy change.
--
-- Run with: ProjectIo --verify scripts/verify/industry_lens.lua

-- LOAD-BEARING, and it stopped being vestigial when BL-373 landed (NR-214): the
-- field is read off the economy report, so with zero ticks the lens renders an
-- EMPTY map — and an empty map still captures a PNG and still blesses clean.
verify.econ_step(4)

verify.goto_surface("home")

verify.set_overlay("industry")
verify.capture("industry_lens_full")

-- Frame the busiest background-industry tile rather than a hard-coded pair.
-- The old (66, 6) framed empty terrain after generation moved the world under it
-- — the same trap settlement_labels.lua documents. Find the subject, then look at it.
local busiest_x, busiest_y, busiest_n = nil, nil, 0
local per_tile = {}
for _, b in ipairs(verify.buildings()) do
  if not b.player then
    local key = b.x .. "," .. b.y
    per_tile[key] = (per_tile[key] or 0) + 1
    if per_tile[key] > busiest_n then
      busiest_n, busiest_x, busiest_y = per_tile[key], b.x, b.y
    end
  end
end

if busiest_x then
  print("[industry_lens] framing " .. busiest_x .. "," .. busiest_y
        .. " (" .. busiest_n .. " background buildings)")
  frame_tile(busiest_x, busiest_y, 9)
else
  -- No background industry at all is a FINDING, not a scene to photograph.
  error("industry_lens: the world holds no background-corp buildings — "
        .. "the lens has nothing to draw and this check would bless an empty map")
end

verify.capture("industry_lens_zoom")
