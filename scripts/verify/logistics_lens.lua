-- Visual verification for the Logistics lens (BL-598; named Logistics, Ben 2026-10-24 -
-- it was the Throughput lens). LENSES.md § Logistics lens owns what it shows.
--
-- The claims this check photographs and asserts:
--   T1  NO GROUND TINT, AND THE ROADS CARRY THE COLOUR (Ben, 2026-10-24). The whole
--       grid: the ground is the plain canvas's, and every road of the player's nation
--       is coloured green -> yellow by supplied over requested along the routed chains
--       (2x or more green, 0.5x or less yellow, log2 between; nothing routed reads
--       green). Other nations' roads keep their ordinary colour.
--   T2  every Logistic Point SOURCE carries a ring. BL-1352: a source is a running
--       logistics hub, railway station or port on some nation's ground. A CITY IS
--       NOT ONE and must carry no ring (LOGISTICS.md: "Cities make none").
--   T3  the key is READABLE over an open Selection band (foreground list, BL-376).
--   T4  the tile card's Logistics section (BL-1354).
--   T5  the key's absolute arithmetic: generated, consumed, needed.
--   T6  the same with a connected source staged.
--   T7  THE ROAD CENSUS (Ben, 2026-10-24): the colour the canvas draws, as counts -
--       the player nation has roads here, requests are routed through some of them,
--       every road lands in one colour class, and what the routed requests were
--       granted never exceeds what they asked. Close captures show the two stacked
--       numbers (supplied over requested) and the stacked hover card.
--
-- Run with: ProjectIo --verify scripts/verify/logistics_lens.lua

verify.goto_surface("home")
verify.set_overlay("logistics")
verify.expect(verify.overlay_name() == "logistics", "the lens arms by its name, logistics")

-- T1 - the whole grid: the network in its supplied-over-requested colours, no tint.
shot("logistics_lens_full")

-- Find the home body, then its largest city. Never hard-coded coordinates — the
-- world moves under a generation change and a fixed pair silently frames empty
-- ground (the trap industry_lens.lua and settlement_labels.lua both record).
local home_body = nil
for _, b in ipairs(verify.buildings()) do
  if b.player then home_body = b.body break end
end
if not home_body then
  error("logistics_lens: no player building, so the home body cannot be identified")
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
  error("logistics_lens: the home body carries no Logistic Point source (hub, "
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

print("[logistics_lens] framing a " .. stype .. " at " .. sx .. "," .. sy)

-- T2 — in close on a source, where its ring read at size against the
-- served ground around it.
frame_tile(sx, sy, 9)
shot("logistics_lens_anchor")

-- T2b — the same zoom on the largest CITY. Cities make no points, so this frame must
-- show NO ring. It is the half of T2 a city-aimed capture could
-- never state (BL-1352).
if cx then
  print("[logistics_lens] framing the largest city at " .. cx .. "," .. cy
        .. " (scale " .. cscale .. ") - it must carry NO ring")
  frame_tile(cx, cy, 9)
  shot("logistics_lens_city_no_source")
end

-- T3 — the key over an OPEN Selection band. This is the capture that fails if the
-- key ever moves back to the background draw list.
verify.select_tile(sx, sy)
shot("logistics_lens_key_over_selection")

-- T4 — BL-1354: the tile card's Logistics section on that same source tile. The
-- source stands here, so "Makes" must carry its points.
local card = verify.tile_card()
verify.expect(card.drawn and card.lp_drawn,
              "BL-1354: the tile card draws its Logistics section")
verify.expect(card.lp_makes > 0,
              "BL-1354: a source tile's Logistics section says what it makes ("
              .. tostring(card.lp_makes) .. ")")
verify.expect(card.lp_uses <= card.lp_need + 0.001,
              "BL-1354: what the tile uses never exceeds what it needs")
print("[logistics_lens] tile card logistics: makes " .. tostring(card.lp_makes)
      .. ", uses " .. tostring(card.lp_uses) .. " of " .. tostring(card.lp_need)
      .. ", rivals hidden " .. tostring(card.lp_rivals_hidden))

-- T5 - ABSOLUTE POINTS, GENERATED AND CONSUMED (Ben, 2026-10-24: "the logistics lens
-- should show absolute numbers rather than a percentage. It should also show both the
-- amount generated, and the amount consumed"). The key's figures are the player
-- nation's own, read off the tick's resolve; what it consumed can never exceed what
-- it generated, and production is never granted more than it asked. Stepped first so
-- the report carries a real resolve (shipments draw only in a stepped tick).
verify.clear_selection()
verify.econ_step(4)
verify.frames(2)
local k = verify.throughput_key()
print(string.format("[logistics_lens] key: generated %.1f by %d source(s); consumed %.1f "
      .. "(production %.1f of %.1f asked, %.1f needed; marches %.1f, shipments %.1f)",
      k.generated, k.sources, k.consumed, k.production, k.asked, k.needed, k.marches, k.shipments))
verify.expect(k.known, "T5: the key has the player's nation to show")
verify.expect(k.generated > 0, "T5: the player nation generates Logistic Points ("
              .. tostring(k.generated) .. ")")
verify.expect(k.consumed <= k.generated + 0.5,
              "T5: consumed (" .. tostring(k.consumed) .. ") never exceeds generated ("
              .. tostring(k.generated) .. ")")
verify.expect(k.production <= k.asked + 0.5,
              "T5: production is granted no more than it asked")
verify.expect(k.asked <= k.needed + 0.5,
              "T5: what production asks never exceeds what it needs (" .. tostring(k.needed) .. ")")
frame_tile(sx, sy, 9)
local pt = verify.tile_screen(sx, sy)
if pt.ok then
  verify.mouse(pt.x, pt.y)
  verify.frames(40)
end
shot("logistics_lens_absolute_card")

-- T6 - THE SAME FIGURES WITH A CONNECTED SOURCE STAGED, so the numbers have something
-- to show whatever the generated world gives the seat. A logistics hub beside the
-- source framed above joins it to the chain; the key must then generate, every
-- consumed figure stays inside what was generated, and the hover card over the hub's
-- ground reads absolute points both ways (captured: logistics_lens_staged_card).
local hub = verify.stage_building(sx, sy - 1, "logistics_hub")
verify.expect(hub ~= 0, "T6: a logistics hub stages beside the source")
-- One tick, so the report's resolve (what the lens reads) includes the staged hub.
verify.econ_step(1)
verify.frames(3)
local k6 = verify.throughput_key()
print(string.format("[logistics_lens] staged: generated %.1f by %d source(s); consumed %.1f "
      .. "(production %.1f of %.1f asked)", k6.generated, k6.sources, k6.consumed,
      k6.production, k6.asked))
verify.expect(k6.generated > 0, "T6: with a connected hub the player nation generates points ("
              .. tostring(k6.generated) .. ")")
verify.expect(k6.consumed <= k6.generated + 0.5, "T6: consumed never exceeds generated")
verify.expect(k6.production <= k6.asked + 0.5, "T6: production is granted no more than it asked")
frame_tile(sx, sy, 9)
local pt6 = verify.tile_screen(sx, sy - 1)
if pt6.ok then
  verify.mouse(pt6.x, pt6.y)
  verify.frames(40)
end
verify.capture("logistics_lens_staged_card")

-- T7 - THE ROAD CENSUS (Ben, 2026-10-24). Counts and totals only; no tile figure
-- crosses the seam. Supplied is what the resolver GRANTED the requests routed through
-- a road, so it can never exceed what they requested.
verify.clear_selection()
verify.mouse(-100, -100)
verify.frames(3)
local r = verify.logistics_roads()
print(string.format("[logistics_lens] roads: %d of the player's (%d routed, %d routed green); green %d, "
      .. "between %d, yellow %d; other ground tiles %d; supplied %.1f of requested %.1f",
      r.roads, r.routed, r.routed_green, r.green, r.between, r.yellow, r.ground, r.supplied, r.requested))
verify.expect(r.roads > 0, "T7: the player nation has roads on the home body (" .. r.roads .. ")")
verify.expect(r.routed > 0, "T7: some request routes through a road (" .. r.routed .. ")")
verify.expect(r.green + r.between + r.yellow == r.roads,
              "T7: every road lands in exactly one colour class")
-- SUPPLIED is what the chain COULD deliver through a road, so a road whose hub has
-- slack carries more than is asked of it and reads green.
verify.expect(r.routed_green > 0,
              "T7: a routed road with slack reads green (" .. r.routed_green .. ")")
-- Close on the staged hub's road ground, where the two stacked numbers draw.
frame_tile(sx, sy, 14)
shot("logistics_lens_stacked_numbers")
local pt7 = verify.tile_screen(sx, sy - 1)
if pt7.ok then
  verify.mouse(pt7.x, pt7.y)
  verify.frames(40)
end
verify.capture("logistics_lens_stacked_card")
