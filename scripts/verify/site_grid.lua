-- The site finder's building grid and its province scope (Ben, 2026-11-08: "We should
-- list building types in a grid, showing estimated returns without having to select a
-- building type. If a province is clicked, it should restrict the search space to just
-- buildings placed on that province."). docs/ui/ledgers/construction.md § The site
-- ledger owns it.
--
-- What this check asserts, without a read of any cell's drawn figure:
--   G1  the grid orders as the rule says, reading the TABLE's own rows for every option:
--       priced options by best Net descending, then sites with no figure, then options
--       with no site. Agreement with the table's rows is what proves the grid's figure
--       is the table's figure.
--   G2  a press on a cell arms the finder, a second press clears it, and a choice
--       does not reorder the grid.
--   G3  a selected tile scopes the table and the lens to its province: fewer or equal
--       rows, every row a whole-body row, every sampled row in that province, the lens
--       counts agreeing, and the grid ordering again by the scoped rows.
--   G4  clearing the selection returns the whole body.
--   G5  a press on a table row selects its tile WITHOUT narrowing the table.
--   G6  a treasury that cannot pay puts every paid option in the no-site tier.
--
-- Run with: ProjectIo --verify scripts/verify/site_grid.lua

verify.window(1720, 1080)
verify.goto_surface("home")
verify.econ_step(2)
verify.set_balance(5000000.0)
verify.show_panel("build", true)
verify.clear_selection() -- the home surface opens with a tile selected, which would scope G1
verify.frames(3)

local opts = verify.site_options()
local by_name = {}
for _, o in ipairs(opts) do by_name[o.name] = o end

-- Read the table for every option, restoring the unarmed state.
local function table_truth()
  local t = {}
  local top = verify.site_finder().picker_rows[1].name
  for _, o in ipairs(opts) do
    verify.site_pick(o.name)
    verify.frames(2)
    local s = verify.site_finder()
    local earns = o.category == "Extraction" or o.category == "Processing" or (o.group or "") ~= ""
    local best = nil
    for _, r in ipairs(s.rows) do
      if r.priced and (best == nil or r.profit > best) then best = r.profit end
    end
    t[o.name] = { n = s.n_rows, best = earns and best or nil }
  end
  -- A pick frames its cell; end on the top cell so the grid is scrolled home and the
  -- first cell is pressable.
  verify.site_pick(top)
  verify.frames(2)
  verify.site_clear()
  verify.frames(2)
  return t
end

local function tier(e) return e.n == 0 and 2 or (e.best ~= nil and 0 or 1) end

local function assert_order(where)
  local truth = table_truth()
  local cells = verify.site_finder().picker_rows
  verify.expect(#cells == #opts, where .. ": one cell per option")
  local counts = { 0, 0, 0 }
  for i, c in ipairs(cells) do
    local e = truth[c.name]
    counts[tier(e) + 1] = counts[tier(e) + 1] + 1
    if i > 1 then
      local p = truth[cells[i - 1].name]
      verify.expect(tier(p) <= tier(e), string.format("%s: '%s' (tier %d) is not after '%s' (tier %d)",
                    where, cells[i - 1].name, tier(p), c.name, tier(e)))
      if tier(p) == 0 and tier(e) == 0 then
        verify.expect(p.best + 0.001 >= e.best, string.format("%s: '%s' %.2f sorts before '%s' %.2f",
                      where, cells[i - 1].name, p.best, c.name, e.best))
      end
    end
  end
  print(string.format("[site_grid] %s: %d priced, %d with no figure, %d with no site",
        where, counts[1], counts[2], counts[3]))
  return truth, counts
end

-- G1
local truth = assert_order("whole body")
verify.hover(1100, 450, 2)
shot("site_grid_whole_body")

-- G2
local first = verify.site_finder().picker_rows[1]
verify.click(first.x, first.y)
verify.frames(3)
local armed = verify.site_finder()
verify.expect(armed.armed and armed.choice == first.name, "G2: a press on a cell chooses it")
verify.expect(armed.live, "G2: the choice arms the lens")
verify.expect(armed.n_rows == truth[first.name].n, "G2: the table lists the cell's count of sites")
verify.expect(armed.picker_rows[1].name == first.name, "G2: a choice does not reorder the grid")
verify.hover(1100, 450, 2)
shot("site_grid_chosen")
verify.click(armed.picker_rows[1].x, armed.picker_rows[1].y)
verify.frames(3)
verify.expect(not verify.site_finder().armed, "G2: pressing the chosen cell clears it")

-- G3: choose the priced option with the most sites, and scope to its best row's province.
local pick, pick_n = nil, 0
for name, e in pairs(truth) do
  if e.best ~= nil and (e.n > pick_n or (e.n == pick_n and pick and name < pick)) then pick, pick_n = name, e.n end
end
verify.expect(pick ~= nil, "G3: some option has priced sites")
if not pick then return end
verify.site_pick(pick)
verify.frames(3)
local whole = verify.site_finder()
local whole_tiles = {}
for _, r in ipairs(whole.rows) do whole_tiles[r.tile] = true end
local target = whole.rows[1]

-- G5: a press on a table row selects without narrowing, while the scope is the whole body.
if whole.n_drawn > 0 then
  local d = whole.drawn[1]
  verify.click(d.x, d.y)
  verify.frames(4)
  local after = verify.site_finder()
  verify.expect(after.n_rows == whole.n_rows, string.format(
    "G5: pressing a table row does not narrow it (%d rows, was %d)", after.n_rows, whole.n_rows))
  verify.clear_selection()
  verify.frames(2)
end

-- G3: a tile selected from the map side.
verify.select_tile(target.x, target.y)
verify.frames(4)
local province = verify.pointer_target().selected_province
verify.expect(province ~= 0, "G3: the tile has a province")
local scoped = verify.site_finder()
print(string.format("[site_grid] %s: %d sites whole body, %d in the province", pick, whole.n_rows, scoped.n_rows))
verify.expect(scoped.n_rows > 0 and scoped.n_rows <= whole.n_rows, "G3: the table narrows to the province")
verify.expect(scoped.lens_valid + scoped.lens_flagged == scoped.n_rows, "G3: the lens marks exactly the scoped rows")
local sample, step = {}, math.max(1, math.floor(scoped.n_rows / 8))
for i = 1, scoped.n_rows, step do sample[#sample + 1] = scoped.rows[i] end
for _, r in ipairs(scoped.rows) do
  verify.expect(whole_tiles[r.tile] == true, "G3: every scoped row was a whole-body row")
end
verify.hover(1100, 450, 2)
shot("site_grid_province_scoped")

-- Order again under the scope (table_truth arms and clears; the selection holds).
for _, r in ipairs(sample) do
  verify.select_tile(r.x, r.y)
  verify.frames(2)
  verify.expect(verify.pointer_target().selected_province == province,
                string.format("G3: row (%d,%d) is in the selected province", r.x, r.y))
end
verify.select_tile(target.x, target.y)
verify.frames(3)
assert_order("province scope")

-- G4
verify.site_pick(pick)
verify.clear_selection()
verify.frames(4)
verify.expect(verify.site_finder().n_rows == whole.n_rows, "G4: clearing the selection returns the whole body")
verify.site_clear()

-- G6: a treasury that cannot pay.
verify.set_balance(0.0)
verify.frames(3)
local poor = assert_order("empty treasury")
local any_blocked = false
for _, e in pairs(poor) do if e.n == 0 then any_blocked = true end end
verify.expect(any_blocked, "G6: an empty treasury leaves options with no affordable site")
verify.set_balance(5000000.0)
verify.frames(3)

-- The narrow end: every cell still legible at 1280.
verify.window(1280, 720)
verify.frames(3)
verify.show_panel("build", true)
verify.frames(3)
local narrow = verify.site_finder()
verify.expect(narrow.n_picker_rows == #opts, "1280: every option draws")
for _, o in ipairs(narrow.picker_rows) do
  verify.expect(o.need <= o.have + 0.5, string.format("1280: '%s' fits (%.1f of %.1f px)", o.name, o.need, o.have))
end
verify.hover(1000, 300, 2)
shot("site_grid_1280")

print("[site_grid] done")
