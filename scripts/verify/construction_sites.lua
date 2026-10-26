-- The site finder: the building-first placement lens and the site ledger (Ben,
-- 2026-10-25: "When the construction tab is open, we should use a lens to show which
-- tiles are valid, and open a ledger to compare tiles, sortable by profit. This should
-- take into account logistics points also, and flag tiles which do not currently have
-- logistics operating."). docs/ui/ledgers/construction.md § The site ledger and
-- docs/ui/LENSES.md § The placement lens own what it shows.
--
-- What this check photographs and asserts:
--   S1  the Construction view opens, a building is chosen through the picker, and the
--       lens goes live; the ledger lists sites and the canvas classes the same count.
--   S2  the rows are SORTED by estimated net, descending, every tie by tile id.
--   S3  every row the check samples is a tile the BUILD DOOR ACCEPTS: the row press
--       selects the tile, and the door's own row for that building carries an enabled
--       Build press ("Starts the build here") and quotes the same net.
--   S4  a tile with NO Logistic Points operating is flagged: its row reads cover 0 and
--       no_lp, the lens classes it as flagged, and at close zoom the canvas draws the
--       struck-truck mark on it.
--   S5  the toggle: choosing the chosen building again clears it, and closing the
--       ledger disarms the lens.
--   S6  the cost: the evaluation's wall time, that it is cached (a still frame does
--       not rebuild it), and the frame cost with the lens up against the lens down.
-- Ben's live-click rulings of 2026-10-26 add:
--   S7  every header is legible: each publishes the width it needs unelided (its
--       text plus the sort arrow) and the width its column gave it, at 1720 and at
--       1280; there is no x,y column, and every row names its place instead.
--   S8  the selected site keeps its "build here" wash under the selection mark.
--   S9  a site under construction carries the progress circle on the map, and
--       hovering it - or its queue row - opens the site card naming the materials
--       still to draw.
--   It also PRINTS the tie measurement (rows sharing a displayed net) for Ben's
--   province-price decision; construction_sites_ties.lua sweeps it across seeds.
--
-- Run with: ProjectIo --verify scripts/verify/construction_sites.lua

verify.window(1720, 1080)
verify.goto_surface("home")
verify.econ_step(2)
-- Fund the seat: affordability is one of the door's gates and the lens honours it,
-- but this check is about the surface agreeing with the door, so a treasury that
-- cannot afford anything would assert nothing.
verify.set_balance(5000000.0)
verify.show_panel("build", true) -- the Construction ledger, on its Construction view
verify.frames(3)

local function sites() return verify.site_finder() end

-- Choose the extraction building with the most sites, preferring one whose list mixes
-- served and unserved tiles (so S4 has something to find). Water and produce are
-- skipped: their door rows are named by the ground (Well / Fishing Wharf), not by the
-- option, so S3 could not match them by name.
local function pick_best()
  local best, best_n, best_mixed = nil, -1, false
  for _, o in ipairs(verify.site_options()) do
    if o.category == "Extraction" and o.name ~= "Farm" and not o.name:find("Water", 1, true)
       and not o.name:find("Well", 1, true) then
      verify.site_pick(o.name)
      verify.frames(2)
      local s = sites()
      local mixed = s.no_lp_rows > 0 and s.no_lp_rows < s.n_rows
      if (mixed and not best_mixed) or (mixed == best_mixed and s.n_rows > best_n) then
        best, best_n, best_mixed = o.name, s.n_rows, mixed
      end
    end
  end
  return best, best_n, best_mixed
end

local choice, n, mixed = pick_best()
if choice and not mixed then
  -- The verify world can hold no running source at all (BL-1410), so every site would
  -- read zero cover and the ledger could not show served against unserved. Stage one
  -- connected logistics hub beside the capital, a tick so the resolve sees it, and
  -- choose again.
  local cs = verify.capital_stars()
  if cs.x then
    local hub = verify.stage_building(cs.x, cs.y - 1, "logistics_hub")
    print("[construction_sites] staged a logistics hub beside the capital: " .. tostring(hub))
    verify.econ_step(1)
    verify.set_balance(5000000.0)
    verify.frames(2)
    choice, n, mixed = pick_best()
  end
end
verify.expect(choice ~= nil, "S1: some extraction building has sites on the player's ground")
if not choice then return end
verify.site_pick(choice)
verify.frames(4)
local s = sites()
print(string.format("[construction_sites] %s: %d sites of %d own tiles (%d unaffordable), "
      .. "%d with no Logistic Points; lens %d valid + %d flagged; eval %.2f ms",
      choice, s.n_rows, s.own_tiles, s.unaffordable, s.no_lp_rows, s.lens_valid,
      s.lens_flagged, s.eval_ms))
verify.expect(s.armed and s.live, "S1: choosing a building arms the lens (live on the Planetary rung)")
verify.expect(s.n_rows > 0, "S1: the ledger lists sites")
verify.expect(s.lens_valid + s.lens_flagged == s.n_rows,
              "S1: the lens classes exactly the tiles the ledger lists")
verify.expect(s.lens_flagged == s.no_lp_rows, "S1: the lens flags exactly the ledger's no-logistics rows")
verify.expect(s.sort_col == 1 and s.sort_desc, "S2: the default sort is profit, descending")

-- S7 - the headers fit, the x,y column is gone, and every row names a place.
local function assert_headers(s7, where)
  verify.expect(s7.n_headers == 6, string.format("S7 (%s): six headers drawn (%d)", where, s7.n_headers))
  for _, h in ipairs(s7.headers) do
    verify.expect(h.label ~= "Tile", "S7 (" .. where .. "): no tile x,y column")
    verify.expect(h.have + 0.5 >= h.need, string.format(
      "S7 (%s): header '%s' is legible - needs %.1f px with its sort arrow, column gives %.1f",
      where, h.label, h.need, h.have))
  end
end
assert_headers(s, "1720x1080")
local unnamed = 0
for _, r in ipairs(s.rows) do if r.place == "" then unnamed = unnamed + 1 end end
verify.expect(unnamed == 0, string.format("S7: every row names its place (%d of %d unnamed)", unnamed, s.n_rows))
print(string.format("[construction_sites] S7: first row '%s'%s", s.rows[1].place, s.rows[1].near and " (near)" or ""))

-- S2 - sorted by profit, descending, ties by tile id.
local sorted = true
for i = 1, s.n_rows - 1 do
  local a, b = s.rows[i], s.rows[i + 1]
  if a.priced and b.priced then
    if a.profit < b.profit or (a.profit == b.profit and a.tile > b.tile) then
      sorted = false
      print(string.format("[construction_sites] S2 out of order at %d: %.3f (%d) then %.3f (%d)",
            i, a.profit, a.tile, b.profit, b.tile))
      break
    end
  elseif b.priced and not a.priced then
    sorted = false
    break
  end
end
verify.expect(sorted, "S2: rows are sorted by estimated net, descending, ties by tile id")

print(string.format("[construction_sites] TIES %s: %d rows; sharing a displayed net at full cover %d "
      .. "(%.0f%%), with cover %d (%.0f%%), net+cost %d (%.0f%%)", choice, s.n_rows,
      s.ties_full_cover, 100 * s.ties_full_cover / math.max(1, s.n_rows),
      s.ties_profit, 100 * s.ties_profit / math.max(1, s.n_rows),
      s.ties_profit_cost, 100 * s.ties_profit_cost / math.max(1, s.n_rows)))

-- The captures: the whole body with the lens up and the ledger beside it.
-- Framed on the best site at a mid rung, so the capture shows the sites and the
-- ground around them (the default framing can sit a whole wrap away from them).
frame_tile(s.rows[1].x, s.rows[1].y, 2.5)
shot("construction_sites_lens_full")

-- S3 - every sampled row is a tile the Build door accepts, at the same figure. The
-- sample: the top five, the first flagged row, and the last.
local sample, seen = {}, {}
local function add(i) if i >= 1 and i <= s.n_rows and not seen[i] then seen[i] = true; sample[#sample + 1] = i end end
for i = 1, 5 do add(i) end
for i = 1, s.n_rows do if s.rows[i].no_lp then add(i) break end end
add(s.n_rows)

local accepted = 0
for _, i in ipairs(sample) do
  local r = s.rows[i]
  verify.select_tile(r.x, r.y)
  verify.frames(3)
  local door = verify.build_door()
  local ok, quoted = false, nil
  if door.drawn then
    for _, dr in ipairs(door.rows) do
      if dr.name == choice then
        for _, c in ipairs(dr.cols) do
          if c.key == "act" and c.text:find("Starts the build here", 1, true) then ok = true end
          if c.key == "bar" then quoted = tonumber(c.text:match("Est%. net ([%+%-]?%d+)")) end
        end
      end
    end
  end
  if ok then accepted = accepted + 1 end
  verify.expect(ok, string.format("S3: row %d (%d,%d) is a tile whose Build door accepts %s", i, r.x, r.y, choice))
  if quoted and r.priced then
    verify.expect(math.abs(quoted - r.profit) <= 1.0,
                  string.format("S3: the door quotes the row's net at (%d,%d): door %d, row %.1f",
                                r.x, r.y, quoted, r.profit))
  end
end
print(string.format("[construction_sites] S3: %d of %d sampled rows accepted by the Build door", accepted, #sample))

-- The ledger, its top row pressed through the REAL control: it selects and frames.
verify.clear_selection()
verify.frames(3)
local s2 = sites()
if s2.n_drawn > 0 then
  local d = s2.drawn[1]
  verify.click(d.x, d.y)
  verify.frames(4)
  local card = verify.tile_card()
  verify.expect(card.drawn, "S3: pressing the top row selects its tile (the tile card draws)")
  -- S8 - the selected site keeps its wash: the canvas washed the selected tile
  -- with a site class (1 served, 2 no Logistic Points), not left it bare.
  local s8 = sites()
  verify.expect(s8.selected_wash == 1 or s8.selected_wash == 2, string.format(
    "S8: the selected site keeps its build-here wash under the selection mark (class %d)",
    s8.selected_wash))
end
verify.hover(1100, 450, 2) -- off the row just pressed, so its card does not cover the ledger
shot("construction_sites_ledger")
-- S8's picture: the selected site, close, green under the selection mark.
if s2.n_drawn > 0 then
  frame_tile(s2.rows[1].x, s2.rows[1].y, 14)
  verify.hover(1100, 450, 2)
  shot("construction_sites_selected_keeps_wash")
end

-- S4 - a flagged tile, framed close enough for the mark.
local flagged = nil
for i = 1, s.n_rows do if s.rows[i].no_lp then flagged = s.rows[i] break end end
if flagged then
  verify.expect(flagged.cover == 0, "S4: a flagged row reads zero Logistic Point cover")
  verify.clear_selection()
  frame_tile(flagged.x, flagged.y, 14)
  verify.frames(4)
  local s4 = sites()
  print(string.format("[construction_sites] S4: flagged site at %d,%d; %d marks drawn", flagged.x,
        flagged.y, s4.map_marks))
  verify.expect(s4.map_marks > 0, "S4: the canvas draws the no-logistics flag at close zoom")
  verify.hover(1100, 450, 2) -- off the ledger, so no row card covers the caption
  shot("construction_sites_no_logistics")
else
  verify.expect(false, "S4: some listed site has no Logistic Points operating (none found)")
end

-- S6 - the cost. Cached: a still frame does not rebuild the evaluation.
local e0 = sites().evaluations
verify.frames(30)
verify.expect(sites().evaluations == e0, "S6: the evaluation is cached across still frames")
frame_tile(flagged and flagged.x or s.rows[1].x, flagged and flagged.y or s.rows[1].y, 5)
verify.frames(10)
verify.frame_reset()
verify.frames(120)
local up = verify.frame_summary()
verify.site_clear()
verify.frames(10)
verify.frame_reset()
verify.frames(120)
local down = verify.frame_summary()
print(string.format("[construction_sites] FRAME lens up: build %.2f ms mean / %.2f max, frame %.2f mean; "
      .. "lens down: build %.2f mean / %.2f max, frame %.2f mean (%d frames each)",
      up.build_mean, up.build_max, up.total_mean, down.build_mean, down.build_max,
      down.total_mean, up.frames))

-- S5 - the toggle and the disarm.
verify.site_pick(choice)
verify.frames(2)
verify.expect(sites().live, "S5: re-armed")
verify.show_panel("construction", false)
verify.frames(2)
local s5 = sites()
verify.expect(not s5.live and not s5.armed, "S5: closing the ledger disarms the lens and clears the choice")

-- S7 at the narrow end: the headers still fit at 1280x720.
verify.window(1280, 720)
verify.frames(3)
verify.show_panel("build", true)
verify.site_pick(choice)
verify.frames(4)
assert_headers(sites(), "1280x720")
verify.hover(900, 400, 2)
shot("construction_sites_ledger_1280")
verify.site_clear()
verify.show_panel("construction", false)
verify.window(1720, 1080)
verify.frames(3)

-- S9 - the progress circle and the site card. Stage an Iron Mine on the first
-- Iron Mine site through the Build door's own seam (ledger_build writes the fields
-- the press writes), let a tick run, and read the canvas's marks.
verify.show_panel("build", true)
verify.site_pick("Iron Mine")
verify.frames(3)
local si = sites()
verify.expect(si.n_rows > 0, "S9: the site finder lists an Iron Mine site to stage on")
if si.n_rows > 0 then
  local t = si.rows[1]
  verify.site_clear()
  verify.select_tile(t.x, t.y)
  verify.frames(2)
  verify.ledger_build("extraction", "iron_ore", "")
  verify.frames(3)
  verify.econ_step(1)
  verify.set_balance(5000000.0)
  verify.clear_selection()
  verify.show_panel("construction", false)
  frame_tile(t.x, t.y, 14)
  verify.hover(1100, 120, 2)
  verify.frames(4)
  local cc = verify.construction_controls()
  print(string.format("[construction_sites] S9: %d progress circle(s) drawn", cc.n_site_marks))
  verify.expect(cc.n_site_marks > 0, "S9: a site under construction carries the progress circle")
  if cc.n_site_marks > 0 then
    -- The mark nearest the canvas centre, which is the framed site.
    local best, bd = nil, nil
    for _, m in ipairs(cc.site_marks) do
      local d = (m.x - 1000) ^ 2 + (m.y - 540) ^ 2
      if not bd or d < bd then best, bd = m, d end
    end
    verify.expect(best.days_total > 0 and best.days_done >= 0 and best.days_done <= best.days_total,
      string.format("S9: the circle shows days done over total (%d / %d)", best.days_done, best.days_total))
    verify.hover(best.x, best.y, 3)
    local card = verify.construction_controls()
    local text = card.site_card_text or ""
    print("[construction_sites] S9 circle card:\n" .. text)
    verify.expect(card.site_card_for == best.building, "S9: hovering the circle opens that site's card")
    local after = text:match("Still to draw:\n(.+)")
    verify.expect(after ~= nil and after:find("%a") ~= nil,
                  "S9: the card names the materials still to draw")
    verify.expect(after ~= nil and (after:find(" @ ", 1, true) ~= nil or after:find("(unpriced)", 1, true) ~= nil),
                  "S9: each material carries the market's current price (or says it is unpriced)")
    if after and text:find("the market holds less", 1, true) then
      verify.expect(after:sub(1, 2) == "! " or after:find("\n! ", 1, true) ~= nil,
                    "S9: the footnote's '!' is on a short good's line")
    end
    shot("construction_sites_progress_circle", 2)

    -- The same card from the queue row.
    verify.select_tile(t.x, t.y)
    verify.frames(1)
    verify.show_panel("build", true)
    verify.section("construction_queue", true)
    verify.frames(3)
    local q = verify.construction_queue()
    local hovered = false
    for _, r in ipairs(q.rows) do
      for _, c in ipairs(r.cols) do
        if c.key == "name" and not hovered then
          verify.hover(c.x, c.y, 3)
          hovered = true
        end
      end
    end
    local qc = verify.construction_controls()
    verify.expect(hovered and (qc.site_card_text or ""):find("Still to draw:", 1, true) ~= nil,
                  "S9: hovering the queue row opens the site card with its materials")
    shot("construction_sites_queue_card", 2)
  end
end
