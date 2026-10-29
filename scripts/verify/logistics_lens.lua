-- Visual verification for the Logistics lens (BL-598; named Logistics, Ben 2026-10-24 -
-- it was the Throughput lens). LENSES.md § Logistics lens owns what it shows.
--
-- The claims this check photographs and asserts:
--   T1  NO GROUND TINT, AND THE ROADS CARRY THE COLOUR (Ben, 2026-10-24). The whole
--       grid: the ground is the plain canvas's, and every road of the player's nation
--       is coloured red -> yellow -> green by supplied over requested along the routed chains
--       (2x or more green, 1x yellow, 0.5x or less red, log2 between - Ben 2026-10-26; nothing routed reads
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
--   T8  THE OFF-ROAD LEG (BL-1456, Ben 2026-10-29): a player unit staged off its network
--       is joined to the road its supply is paid at by a dash; one past its 8 tiles by a
--       red dash. Counts only (verify.logistics_roads().dashes*).
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
--
-- THE CHAIN'S ROOT IS STAGED FIRST. Only a source chained to the capital adds to the
-- pool (NR-1057; LOGISTICS.md § The chain is laid outward from the capital), and the
-- campaign's sources are seeded by `finish_campaign_world` (seed_logistics_sources) -
-- a step the --verify world's builder never runs. Without this the world's one source
-- is a port no road ties to the capital, the pool is empty, and T4-T7 read 0 whatever
-- the lens does. A hub ON the capital is what the seeding stands first ("the capital
-- first"), staged as logistics_chain_fixture stages its root; it is the source framed.
local sx, sy, stype = nil, nil, nil
local cs = verify.capital_stars()
if cs.has_capital and cs.body == home_body
   and verify.stage_building(cs.x, cs.y, "logistics_hub") ~= 0 then
  sx, sy, stype = cs.x, cs.y, "Inland Logistics Hub"
  print("[logistics_lens] staged the chain's root: a logistics hub on the capital")
end
for _, b in ipairs(verify.buildings()) do
  if sx then break end
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
      .. "(production %.1f of %.1f asked, %.1f needed; unit supply %.1f, shipments %.1f)",
      k.generated, k.sources, k.consumed, k.production, k.asked, k.needed, k.supply, k.shipments))
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
-- A ROUTED REQUEST IS STAGED FIRST, for the root's reason: the --verify world skips
-- the campaign's site-road call (BL-1347) as it skips the source seeding, so its
-- producers stand off the network and nothing routes through any road. The lens's own
-- chain fixture stands a processing works at the far end of a rail and a sea leg tied
-- to the capital, so a request travels the chain; one tick puts it in the resolve.
local chain = verify.logistics_chain_fixture()
verify.expect(chain.ok, "T7: the chain fixture stages on the player's ground")
verify.econ_step(1)
verify.clear_selection()
verify.mouse(-100, -100)
verify.frames(3)
local r = verify.logistics_roads()
print(string.format("[logistics_lens] roads: %d of the player's (%d routed, %d routed green); green %d, "
      .. "between %d, red %d; other ground tiles %d; supplied %.1f of requested %.1f",
      r.roads, r.routed, r.routed_green, r.green, r.between, r.red, r.ground, r.supplied, r.requested))
verify.expect(r.roads > 0, "T7: the player nation has roads on the home body (" .. r.roads .. ")")
verify.expect(r.routed > 0, "T7: some request routes through a road (" .. r.routed .. ")")
verify.expect(r.green + r.between + r.red == r.roads,
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

-- T8 - THE OFF-ROAD LEG, DASHED (BL-1456; Ben, 2026-10-29; LENSES.md § Logistics lens,
-- "A unit off the road is joined to it by a dashed line"). The fixture moves the
-- player's lowest-id unit to own ground 3-6 steps off its network, and the next unit
-- past its 8 tiles (a red dash, when such ground exists within the 32-step display
-- search). One econ step so the resolve pays both; then the census is COUNTS ONLY, and
-- the capture aims at the near unit through verify.units(), which lists positions.
-- A network to be off: the --verify world's builder ties no source to the capital, so
-- when the key generates nothing T8 stages the chain fixture (a hub on the capital and
-- its links) first. Guarded, so it is not staged twice once an earlier step has.
if verify.throughput_key().generated <= 0 then
  local chain = verify.logistics_chain_fixture()
  print("[logistics_lens] T8 staged the chain fixture: " .. tostring(chain.ok))
  verify.econ_step(1)
  verify.frames(3)
end
local off = verify.offroad_unit_fixture()
verify.expect(off.ok, "T8: a player unit stages 3-6 steps off its network, at a full-cover road")
if not off.far then
  -- Not a skip: the world has no own land past 8 tiles of the network with a road
  -- within the 32-step display search, so the red half of T8 has nothing to stand on.
  print("[logistics_lens] T8 NOTE: no far tile on this world (own land past 8 tiles with a "
        .. "road within 32) - the past-its-roads dash is NOT exercised by this run")
end
verify.econ_step(1)
verify.frames(3)
local r8 = verify.logistics_roads()
print(string.format("[logistics_lens] off-road: near %s, far %s; dashes %d (%d red), drawn %d (%d red)",
      tostring(off.near), tostring(off.far), r8.dashes, r8.dashes_red, r8.dashes_drawn,
      r8.dashes_red_drawn))

-- THE STAGED UNITS THEMSELVES, by id (counts, flags and a share; no positions).
local nd = off.near and verify.logistics_dash(off.near_unit) or { exists = false }
print(string.format("[logistics_lens] near unit %s: dash %s, past %s, forecast %s, share %.3f",
      tostring(off.near_unit), tostring(nd.exists), tostring(nd.past), tostring(nd.forecast),
      nd.share or -1))
verify.expect(nd.exists, "T8: the staged near unit is joined to its road by a dash")
verify.expect(nd.exists and not nd.past, "T8: the near unit's dash is the within-8 rhythm, not past its roads")
verify.expect(nd.exists and not nd.forecast,
              "T8: the near unit's dash colour is its own resolve's share (it has not moved since)")
verify.expect(nd.exists and nd.share >= 0.999,
              "T8: fed - its whole request granted at a full-cover road - so its dash reads GREEN ("
              .. tostring(nd.share) .. ")")
if off.far then
  local fd = verify.logistics_dash(off.far_unit)
  print(string.format("[logistics_lens] far unit %s: dash %s, past %s", tostring(off.far_unit),
        tostring(fd.exists), tostring(fd.past)))
  verify.expect(fd.exists and fd.past,
                "T8: the staged far unit draws the RED past-its-roads dash")
end

local ux, uy, fx, fy = nil, nil, nil, nil
for _, u in ipairs(verify.units()) do
  if off.near and u.id == off.near_unit then ux, uy = u.col, u.row end
  if off.far  and u.id == off.far_unit  then fx, fy = u.col, u.row end
end
if ux then
  frame_tile(ux, uy, 9)
  verify.frames(3)
  local d = verify.logistics_dash(off.near_unit)
  verify.expect(d.exists and d.drawn >= 1,
                "T8: framed on the near unit, the canvas strokes ITS dash (" .. tostring(d.drawn) .. ")")
  shot("logistics_lens_offroad_dash")
  frame_tile(ux, uy, 4)
  shot("logistics_lens_offroad_dash_far_zoom")
end
if fx then
  frame_tile(fx, fy, 6)
  verify.frames(3)
  local d = verify.logistics_dash(off.far_unit)
  verify.expect(d.exists and d.drawn >= 1,
                "T8: framed on the far unit, the canvas strokes ITS red dash (" .. tostring(d.drawn) .. ")")
  shot("logistics_lens_offroad_dash_red")
end
