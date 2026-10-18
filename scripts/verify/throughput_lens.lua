-- Visual verification for the Throughput lens (BL-598).
--
-- LOGISTICS.md § Logistic Points, "Refusal, surface and determinism":
--   "Throughput is a lens, extending Reach. The Reach lens shows a binary field;
--    throughput is that field with a magnitude, so it is a small step from an
--    existing surface rather than a new one."
--
-- The three claims this check photographs:
--   T1  the BINARY half — ground an anchor reaches is tinted the logistics cyan,
--       ground no anchor reaches goes dark. That is the reach field, drawn
--       through the CONST `tile_reach_cost` read; the canvas never runs the
--       Dijkstra itself.
--   T2  the MAGNITUDE half — every Logistic Point SOURCE carries a disc sized and
--       coloured by the points it makes this tick. BL-1352: a source is a running
--       logistics hub, railway station or port on some nation's ground. A CITY IS
--       NOT ONE and must carry no disc (LOGISTICS.md: "Cities make none"), which is
--       why this check frames a real source and no longer the largest city: aimed
--       at a city it would photograph empty ground and pass either way.
--   T3  the key is READABLE. Six of the seven gradient keys are drawn on the
--       background list and buried by the Selection band; this one takes the
--       Continent key's foreground list and opaque fill (BL-376), so the last
--       capture deliberately opens the Selection band UNDER it.
--
-- Run with: ProjectIo --verify scripts/verify/throughput_lens.lua

verify.goto_surface("home")
verify.set_overlay("throughput")

-- T1 — the whole grid. The envelope is the subject at this zoom: the reach field
-- has shape, and its shape is what "Reach with a magnitude" extends.
shot("throughput_lens_full")

-- Find the home body, then its largest city. Never hard-coded coordinates — the
-- world moves under a generation change and a fixed pair silently frames empty
-- ground (the trap industry_lens.lua and settlement_labels.lua both record).
local home_body = nil
for _, b in ipairs(verify.buildings()) do
  if b.player then home_body = b.body break end
end
if not home_body then
  error("throughput_lens: no player building, so the home body cannot be identified")
end

-- A REAL SOURCE, not the largest city (BL-1352). The disc is drawn where points are
-- MADE, so the frame has to land on a hub, station or port; a city draws nothing now,
-- and a capture aimed at one would show bare ground whether the lens worked or not.
local sx, sy, stype = nil, nil, nil
for _, b in ipairs(verify.buildings()) do
  if b.body == home_body and b.complete
     and (b.type == "Inland Logistics Hub" or b.type == "Port"
          or b.type == "Railway Station") then
    sx, sy, stype = b.x, b.y, b.type
    break
  end
end
if not sx then
  -- A home body with no source at all is a FINDING, not a scene to photograph: the
  -- lens would render an all-dark envelope and an honest "no source makes Logistic
  -- Points here" key, and that would bless clean for ever.
  error("throughput_lens: the home body carries no Logistic Point source (hub, "
        .. "station or port), so there is no magnitude to draw")
end

-- A city, framed only to prove the NEGATIVE: it must carry no disc. Picked the same
-- way the old aim was, so the two frames are directly comparable.
local cx, cy, cscale = nil, nil, -1
for _, pc in ipairs(verify.population_centres()) do
  if pc.body == home_body and pc.scale > cscale then
    cscale, cx, cy = pc.scale, pc.x, pc.y
  end
end

print("[throughput_lens] framing a " .. stype .. " at " .. sx .. "," .. sy)

-- T2 — in close on a source, where the disc and its ring read at size against the
-- served ground around it.
frame_tile(sx, sy, 9)
shot("throughput_lens_anchor")

-- T2b — the same zoom on the largest CITY. Cities make no points, so this frame must
-- show served ground and NO disc. It is the half of T2 a city-aimed capture could
-- never state (BL-1352).
if cx then
  print("[throughput_lens] framing the largest city at " .. cx .. "," .. cy
        .. " (scale " .. cscale .. ") - it must carry NO disc")
  frame_tile(cx, cy, 9)
  shot("throughput_lens_city_no_source")
end

-- T3 — the key over an OPEN Selection band. This is the capture that fails if the
-- key ever moves back to the background draw list.
verify.select_tile(sx, sy)
shot("throughput_lens_key_over_selection")
