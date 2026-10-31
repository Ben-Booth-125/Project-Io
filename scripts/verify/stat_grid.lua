-- Verify the stat grid (BL-1454; docs/ui/ledgers/military.md § The stat grid).
--
-- One grid, drawn by one function (src/ui/stat_grid.cpp), on five surfaces. Each is
-- captured and asserted through verify.stat_grids(), which returns what the grid
-- DREW on the last frame: per cell its band, stat name, value as drawn, a group's
-- range, a designer's delta, and whether it carries a bar.
--
-- 1. The unit card: four bands - Condition (HP, readiness, morale), Supply (food,
--    ammunition, equipment), Combat (soft attack, hard attack, defence,
--    breakthrough, hardness), Field (recon, stealth, speed) - in that order; the six
--    stored stats barred, none of the rest; no dash on the player's own unit. A
--    hover on a cell shows its tooltip (captured).
-- 2. A rival's unit: its internals read a dash; unseen (in no battle against the
--    player), every cell does.
-- 3. The land designer with a delta: Combat and Field only; hovering a live kit
--    pick previews it with a signed delta; the pick drafts and the delta stays;
--    speed never carries one. The kit-lock waive is staging and is turned off.
-- 4. An air wing's card: Condition (HP, morale, supply), Combat (air attack, soft
--    attack, hard attack, strategic), Field (recon, range). And the air designer.
-- 5. A stack (unit group) card: each own cell carries a lowest-to-highest range.
--
-- Staging: an airbase and a wing are STAGED (stage_building, stage_wing); two hires
-- at the player's base go through the real hire verb, as unit_counter.lua does.
-- Every press aims at a point the surface published.

verify.goto_surface("home")
verify.clear_selection()
verify.set_overlay("none")
verify.mouse(-1, -1)
verify.frames(3)

local fails = 0
local function fail(msg) fails = fails + 1; verify.expect(false, "stat_grid: " .. msg) end
local function ok(msg)   verify.expect(true, "stat_grid: " .. msg) end
local function settle(n) verify.mouse(-1, -1); verify.frames(n or 3) end

local DASH = "\xe2\x80\x94"

local function grid(id)
    return verify.stat_grids()[id]
end

local function dump(tag, cells)
    if cells == nil then print("stat_grid[" .. tag .. "]: not drawn"); return end
    for _, c in ipairs(cells) do
        print(string.format("  %-9s %-13s %-8s range '%s' delta '%s'%s", c.band, c.stat, c.value,
            c.range, c.delta, c.bar and " bar" or ""))
    end
end

-- The bands in order, as "Band:stat,stat | Band:stat,...". (The verify sandbox
-- carries no `table` library, so the string is built by hand.)
local function layout(cells)
    local s, band = "", nil
    for _, c in ipairs(cells) do
        if c.band ~= band then
            if band ~= nil then s = s .. " | " end
            band = c.band
            s = s .. band .. ":" .. c.stat
        else
            s = s .. "," .. c.stat
        end
    end
    return s
end

local LAND = "Condition:HP,Readiness,Morale | Supply:Food,Ammunition,Equipment"
    .. " | Combat:Soft attack,Hard attack,Defence,Breakthrough,Hardness | Field:Recon,Stealth,Speed"
local LAND_DESIGN = "Combat:Soft attack,Hard attack,Defence,Breakthrough,Hardness | Field:Recon,Stealth,Speed"
local AIR = "Condition:HP,Morale,Supply | Combat:Air attack,Soft attack,Hard attack,Strategic | Field:Recon,Range"
local AIR_DESIGN = "Condition:HP | Combat:Air attack,Soft attack,Hard attack,Strategic | Field:Recon,Range"
local STORED = { HP = true, Readiness = true, Morale = true, Food = true, Ammunition = true, Equipment = true }

local function find(cells, stat)
    for _, c in ipairs(cells or {}) do if c.stat == stat then return c end end
    return nil
end

-- 1. The unit card.
local mine, theirs = nil, nil
for _, u in ipairs(verify.units()) do
    if u.col ~= nil and u.count > 0 then
        if u.player then mine = mine or u else theirs = theirs or u end
    end
end
assert(mine, "the player has no unit (every nation seeds a garrison)")
verify.center_tile(mine.col, mine.row, 10.0)
assert(verify.select_unit(mine.id, false), "select_unit refused the player's unit")
settle()
local g = grid("unit_card")
dump("unit_card", g)
if g == nil then
    fail("the unit card drew no stat grid")
else
    local l = layout(g)
    if l == LAND then ok("the unit card draws the four land bands in order") else fail("unit card layout: " .. l) end
    local dashes, barred, wrong_bar = 0, 0, 0
    for _, c in ipairs(g) do
        if c.value == DASH then dashes = dashes + 1 end
        if c.bar then barred = barred + 1 end
        if c.bar ~= (STORED[c.stat] == true and c.band ~= "Combat") then wrong_bar = wrong_bar + 1 end
        if c.range ~= "" then fail("a single unit's cell carries a range: " .. c.stat) end
    end
    if dashes == 0 then ok("the player's own unit shows every value") else fail(dashes .. " dash(es) on the player's own unit") end
    if barred == 6 and wrong_bar == 0 then ok("the six stored stats are barred, nothing else") else
        fail("bars: " .. barred .. " barred, " .. wrong_bar .. " misplaced") end
    verify.capture("stat_grid_unit_card")
    local soft = find(g, "Soft attack")
    verify.hover(soft.x, soft.y, 3)
    verify.capture("stat_grid_unit_card_tooltip")
    settle()
end

-- 2. A rival's unit.
if theirs ~= nil then
    verify.center_tile(theirs.col, theirs.row, 10.0)
    verify.select_unit(theirs.id, false)
    settle()
    g = grid("unit_card")
    dump("rival", g)
    if g ~= nil then
        local internals_hidden, all_hidden = true, true
        for _, c in ipairs(g) do
            if c.value ~= DASH then
                all_hidden = false
                if c.band == "Condition" or c.band == "Supply" then internals_hidden = false end
            end
        end
        if internals_hidden then ok("a rival's internals read a dash") else fail("a rival's internals are shown") end
        if all_hidden then ok("an unseen rival reads a dash in every cell") else
            print("stat_grid: the rival shows design stats (it stands in a battle against the player)") end
        verify.capture("stat_grid_rival_unit")
    else
        fail("the rival's unit card drew no stat grid")
    end
else
    print("stat_grid: no rival unit on the surface; the rival check is not taken")
end
verify.clear_selection()
settle()

-- 3. The land designer with a delta (armour: three slots).
local ARMOUR = 3
local function section3()
    verify.waive_kit_research(true)
    verify.fold("unit_designer", ARMOUR)
    settle()
    local d = verify.unit_designer()
    assert(d.open and d.cls == ARMOUR, "the designer did not open on armour")
    local gd = grid("unit_designer")
    dump("designer", gd)
    if gd == nil then fail("the designer drew no stat grid"); return end
    local l = layout(gd)
    if l == LAND_DESIGN then ok("the designer shows design stats only: Combat and Field") else fail("designer layout: " .. l) end
    local any = false
    for _, c in ipairs(gd) do if c.delta ~= "" then any = true end end
    if not any then ok("the live design carries no delta") else fail("a delta shows with no draft") end
    verify.capture("stat_grid_designer")

    -- Open slot 0's list and hover a live pick: the grid previews it.
    local box = nil
    for _, b in ipairs(d.boxes) do if b.slot == 0 then box = b end end
    verify.click(box.x, box.y)
    settle()
    d = verify.unit_designer()
    local pick = nil
    for _, i in ipairs(d.items) do if i.enabled and not i.fitted then pick = i; break end end
    if pick == nil then fail("no live pick in the first slot with the kit lock waived"); return end
    verify.hover(pick.x, pick.y, 3)
    gd = grid("unit_designer")
    dump("designer preview " .. pick.name, gd)
    local deltas = 0
    for _, c in ipairs(gd or {}) do if c.delta ~= "" then deltas = deltas + 1 end end
    if deltas > 0 then ok("hovering '" .. pick.name .. "' previews " .. deltas .. " signed delta(s)") else
        fail("hovering a kit pick showed no delta") end
    if find(gd, "Speed") and find(gd, "Speed").delta == "" then ok("speed carries no delta") else
        fail("speed carries a delta") end
    verify.capture("stat_grid_designer_preview")

    -- The pick drafts: the delta stays against the live design.
    verify.click(pick.x, pick.y)
    settle()
    gd = grid("unit_designer")
    local kept = 0
    for _, c in ipairs(gd or {}) do if c.delta ~= "" then kept = kept + 1 end end
    if kept == deltas then ok("the draft keeps its " .. kept .. " delta(s) against the live design") else
        fail("draft deltas " .. kept .. " vs previewed " .. deltas) end
    verify.capture("stat_grid_designer_draft")
    d = verify.unit_designer()
    verify.click(d.revert_x, d.revert_y)
    settle()
    gd = grid("unit_designer")
    local after = 0
    for _, c in ipairs(gd or {}) do if c.delta ~= "" then after = after + 1 end end
    if after == 0 then ok("Revert clears the deltas") else fail(after .. " delta(s) after Revert") end
end
local okd, err = pcall(section3)
verify.waive_kit_research(false)
if not okd then fail("designer section: " .. tostring(err)) end
verify.fold()
settle()

-- 4. An air wing's card, and the air designer.
local home = nil
for _, b in ipairs(verify.buildings()) do
    if b.player then home = b; break end
end
assert(home, "no player building to stand an airbase beside")
local airbase = verify.stage_building(home.x, home.y, "airbase")
assert(airbase ~= 0, "could not stage an airbase")
local wing = verify.stage_wing(airbase, 0)
assert(wing ~= 0, "could not stage a wing")
verify.place_mode("none")
assert(verify.select_wing(wing), "select_wing refused")
settle()
g = grid("wing_card")
dump("wing_card", g)
if g == nil then
    fail("the wing card drew no stat grid")
else
    local l = layout(g)
    if l == AIR then ok("the wing card draws Condition, Combat and Field") else fail("wing card layout: " .. l) end
    local hp = find(g, "HP")
    if hp and hp.bar and hp.value ~= DASH then ok("the wing's HP is barred: " .. hp.value) else fail("the wing's HP cell") end
    verify.capture("stat_grid_wing_card")
end
verify.clear_selection()
verify.fold("air_designer", 0)
settle()
g = grid("air_designer")
dump("air_designer", g)
if g ~= nil and layout(g) == AIR_DESIGN then ok("the air designer shows HP at full, Combat and Field") else
    fail("air designer layout: " .. (g and layout(g) or "not drawn")) end
verify.capture("stat_grid_air_designer")
verify.fold()
settle()

-- 5. A stack: two hires at the player's base, then the group's card.
local VERB_HIRE_UNIT = 8 -- corp_verb::hire_unit
local function base_tile()
    for _, b in ipairs(verify.buildings()) do
        if b.player and b.type == "Military Base" and b.complete then return b.tile end
    end
    return nil
end
local function hire_at(tile)
    local LOAN = 5000.0
    verify.set_balance(verify.player_balance() + LOAN)
    verify.grant_stock("iron_ore", 50)
    verify.grant_stock("agricultural_produce", 50)
    verify.grant_stock("coal", 50)
    local last = "none"
    for row = 0, 15 do
        last = verify.corp_command{ verb = VERB_HIRE_UNIT, tile = tile, unit_type = row }
        if last == "applied" then break end
    end
    verify.set_balance(verify.player_balance() - LOAN)
    return last
end
local tile = base_tile()
if tile == nil then
    local corp = nil
    for _, b in ipairs(verify.buildings()) do
        if b.player then corp = b.corp; break end
    end
    raise_player_force(corp)
    verify.place_mode("none")
    tile = base_tile()
end
assert(tile, "no completed player military base to hire at")
print("stat_grid: hires " .. tostring(hire_at(tile)) .. ", " .. tostring(hire_at(tile)))
verify.econ_step(2)

local stack, best = nil, 0
do
    local by = {}
    for _, u in ipairs(verify.units()) do
        if u.col ~= nil and u.player and u.count > 0 then
            local k = u.col .. "," .. u.row
            by[k] = by[k] or { n = 0, u = u }
            by[k].n = by[k].n + 1
            if by[k].u.id > u.id then by[k].u = u end
            if by[k].n > best then best = by[k].n; stack = by[k].u end
        end
    end
end
assert(stack and best >= 2, "no tile holds two player units (best " .. best .. ")")
verify.center_tile(stack.col, stack.row, 10.0)
assert(verify.select_unit(stack.id, true), "select_unit refused the group")
settle()
g = grid("unit_group")
dump("unit_group", g)
if g == nil then
    fail("the unit group drew no stat grid")
else
    if layout(g) == LAND then ok("the group card draws the four land bands") else fail("group layout: " .. layout(g)) end
    local ranged = 0
    for _, c in ipairs(g) do if c.range ~= "" then ranged = ranged + 1 end end
    if ranged == #g then ok("every cell of a " .. best .. "-unit stack carries its range") else
        fail(ranged .. " of " .. #g .. " cells carry a range") end
    verify.capture("stat_grid_stack")
    local hp = find(g, "HP")
    verify.hover(hp.x, hp.y, 3)
    verify.capture("stat_grid_stack_tooltip")
    settle()
end

print(string.format("stat_grid: %d failure(s)", fails))
verify.clear_selection()
