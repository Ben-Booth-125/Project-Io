-- Visual verification for the Industry lens (BL-084; re-subjected BL-1419).
--
-- The lens's subject is the plants of nations other than the player's — "where
-- is the industry I did not build?" (LENSES.md § Industry lens). It reads only
-- what the screen may show: a rival plant's presence and whether it is running,
-- never its output (DISCOVERY.md § Competitor visibility).
--
--   I1  after the fold, the home body carries rival nations' buildings, so the
--       lens has a subject (an empty map would still capture and bless clean)
--   I2  the lens is up, and it is a strip lens at the Planetary rung (capture:
--       the factory glyph highlighted on the minimap strip)
--   I3  framed on the densest rival tile, the amber fill sits on rival ground
--       (capture, looked at)
--
-- Run with: ProjectIo --verify scripts/verify/industry_lens.lua

verify.goto_surface("home")

-- The --verify world is laid unfolded: every building is still the seat
-- corporation's until fold_to_ground runs, as every campaign start runs it.
local f = verify.fold_world()
verify.expect(f.folded, "I1 the verify world folds to ground")

-- LOAD-BEARING: running state is read off the economy report, so the lens
-- needs ticks for a working plant to read as working.
verify.econ_step(4)

-- The home body is where the player's own buildings stand.
local home_body = nil
for _, b in ipairs(verify.buildings()) do
  if b.player then home_body = b.body break end
end
verify.expect(home_body ~= nil, "I1 the player holds a building on the home body")

-- Rival plant on the home body, counted per tile; frame the densest tile.
local busiest_x, busiest_y, busiest_n = nil, nil, 0
local per_tile, rivals, mine = {}, 0, 0
for _, b in ipairs(verify.buildings()) do
  if b.body == home_body then
    if b.player then
      mine = mine + 1
    else
      rivals = rivals + 1
      local key = b.x .. "," .. b.y
      per_tile[key] = (per_tile[key] or 0) + 1
      if per_tile[key] > busiest_n then
        busiest_n, busiest_x, busiest_y = per_tile[key], b.x, b.y
      end
    end
  end
end
print(string.format("[industry_lens] home body: %d rival buildings, %d the player's",
                    rivals, mine))
verify.expect(rivals > 0,
  "I1 the home body holds rival nations' buildings (the lens has a subject)")

verify.set_overlay("industry")
verify.frames(2)
verify.expect(verify.overlay_name() == "industry" or verify.overlay_name() == "Industry",
  "I2 the Industry lens is up (got '" .. tostring(verify.overlay_name()) .. "')")
verify.capture("industry_lens_full")

if busiest_x then
  print("[industry_lens] framing " .. busiest_x .. "," .. busiest_y
        .. " (" .. busiest_n .. " rival buildings)")
  frame_tile(busiest_x, busiest_y, 9)
  verify.frames(2)
  verify.capture("industry_lens_zoom")
end
