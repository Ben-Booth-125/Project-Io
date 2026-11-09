-- The Build door and the site finder offer Schooling and University (Ben, 2026-11-07;
-- docs/ui/SELECTION.md § The tile construction ledger). The stratum gate stays what decides
-- where (docs/economy/POPULATION.md § Strata gate buildings): Schooling any centre, a University
-- City (4) or above.
--
-- What this check reads: the finder's options, its rows, and the rows the door drew. A finder
-- row is a tile the gate passed, so a University row that is not a Schooling row would mean
-- the two gates disagree. Three presses, each on its OWN centre, because a tile with a site
-- under way refuses another for a reason that is not the one under test:
--   SCHOOL  Schooling goes up on a centre.
--   TOWN    a University is refused on a centre below City.
--   CITY    a University goes up on a City; STAGED when the player holds none.
-- Fewer than three centres, or no centre below City, is staged rather than skipped.
--
-- Run with: ProjectIo --verify scripts/verify/schooling_door.lua

verify.window(1720, 1080)
verify.goto_surface("home")
verify.set_balance(5000000.0)
verify.show_panel("build", true)
verify.clear_selection() -- a selected tile scopes the finder to its province (Ben, 2026-11-08)
verify.frames(3)

-- THE FINDER LISTS BOTH, in one fold.
local seen = {}
for _, o in ipairs(verify.site_options()) do
    if o.name == "Schooling" or o.name == "University" then
        seen[o.name] = true
        verify.expect(o.category == "Infrastructure",
                      "the finder files " .. o.name .. " under Infrastructure, not " .. o.category)
    end
end
verify.expect(seen["Schooling"], "the finder offers Schooling")
verify.expect(seen["University"], "the finder offers University")

local function rows_of(name)
    verify.construction_view(2)
    verify.expect(verify.site_pick(name), "the finder arms " .. name)
    verify.frames(3)
    local s = verify.site_finder()
    local out = {}
    for i = 1, s.n_rows do
        out[i] = { x = s.rows[i].x, y = s.rows[i].y }
    end
    return out, s
end

local key = function(r) return r.x .. "," .. r.y end

local school_rows, school = rows_of("Schooling")
print(string.format("schooling_door: Schooling %d sites of %d own tiles", #school_rows, school.own_tiles))
verify.expect(#school_rows >= 3, "the player has three centres, one for each press")
verify.expect(#school_rows < school.own_tiles, "Schooling is gated: not every own tile is a centre")
verify.capture("schooling_door_finder")

local at_centre = {}
for _, r in ipairs(school_rows) do at_centre[key(r)] = true end

local uni_rows = rows_of("University")
print(string.format("schooling_door: University %d sites", #uni_rows))
verify.expect(#uni_rows <= #school_rows, "a University's ground is no wider than Schooling's")
local at_city = {}
for _, r in ipairs(uni_rows) do
    at_city[key(r)] = true
    verify.expect(at_centre[key(r)], "a University site is also a Schooling site: " .. key(r))
end

-- WHAT THE GROUND HOLDS (Rule 0b, the numbers, unasserted).
local hist = { 0, 0, 0, 0, 0 }
for _, c in ipairs(verify.population_centres()) do
    if hist[c.scale] then hist[c.scale] = hist[c.scale] + 1 end
end
print(string.format("schooling_door: centres by scale 1..5: %d %d %d %d %d; the player's City+: %d of %d",
                    hist[1], hist[2], hist[3], hist[4], hist[5], #uni_rows, #school_rows))

-- Choose the three tiles. The town comes first from row 1; if every centre the player holds is
-- a City, one is STAGED down, so the refusal is always exercised.
local town = nil
for _, r in ipairs(school_rows) do
    if not at_city[key(r)] then town = r; break end
end
if town == nil and #school_rows > 0 then
    town = school_rows[1]
    verify.expect(verify.stage_centre(town.x, town.y, 3), "a City staged down to a town")
    at_city[key(town)] = nil
end
verify.expect(town ~= nil, "a centre below City to refuse")

local function other_centre(not_a, not_b)
    for _, r in ipairs(school_rows) do
        if (not not_a or key(r) ~= key(not_a)) and (not not_b or key(r) ~= key(not_b)) then return r end
    end
    return nil
end

local city = nil
for _, r in ipairs(uni_rows) do
    if key(r) ~= key(town) then city = r; break end
end
if city == nil then
    city = other_centre(town, nil)
    verify.expect(city ~= nil, "a second centre to stage as a City")
    verify.expect(verify.stage_centre(city.x, city.y, 4), "a centre staged to City")
    print(string.format("schooling_door: the player holds no City: (%d,%d) STAGED to scale 4", city.x, city.y))
end
local centre = other_centre(town, city)
verify.expect(centre ~= nil, "a third centre for the Schooling press")

verify.site_clear()
verify.construction_view(0)
verify.frames(2)

local function door_row(name)
    local d = verify.build_door()
    for _, r in ipairs(d.rows) do
        if r.name == name then return r, d end
    end
    return nil, d
end

local function look_at(r)
    verify.select_tile(r.x, r.y)
    verify.frames(4)
    verify.scroll_panel("build_list", 1.0)
    verify.frames(3)
end

local function count_type(name)
    local n = 0
    for _, b in ipairs(verify.buildings()) do
        if b.player and b.type == name then n = n + 1 end
    end
    return n
end

-- SCHOOL.
look_at(centre)
verify.capture("schooling_door_row")
local sr, door = door_row("Schooling")
verify.expect(door.n_rows > 0, "the door drew its rows")
verify.expect(sr ~= nil, "the door lists Schooling on a centre")
if sr then
    verify.expect(sr.fold == "Infrastructure", "the door files Schooling under Infrastructure, not " .. sr.fold)
    print("schooling_door: Schooling hover: " .. sr.hover)
    verify.expect(sr.hover:find("nation's qualification", 1, true) ~= nil, "Schooling's row says what it buys")
end
local schools_before = count_type("Schooling")
verify.ledger_build("schooling", "", "")
verify.frames(3)
verify.expect(count_type("Schooling") == schools_before + 1, "the press stands a Schooling on a centre")

-- TOWN: the row names the gate and the press is refused by it.
look_at(town)
local u = door_row("University")
verify.expect(u ~= nil, "the door lists University on a small centre, refused")
if u then
    print(string.format("schooling_door: University at a small centre (%d,%d): %s", town.x, town.y, u.hover))
    verify.expect(u.fold == "Infrastructure", "the door files University under Infrastructure, not " .. u.fold)
    verify.expect(u.hover:find("too small", 1, true) ~= nil,
                  "a small centre's University row says the settlement is too small")
end
verify.capture("schooling_door_town")
local unis_before = count_type("University")
verify.ledger_build("university", "", "")
verify.frames(3)
verify.expect(count_type("University") == unis_before, "a town refuses a University: you can't build a university in a town")

-- CITY: the finder and the door agree, and the press stands it. A moved balance is what makes
-- the finder re-read the ground it cached.
verify.set_balance(4000000.0)
verify.construction_view(2)
verify.expect(verify.site_pick("University"), "the finder arms University")
verify.frames(3)
local sites = verify.site_finder()
local found = false
for i = 1, sites.n_rows do
    local r = sites.rows[i]
    if r.x == city.x and r.y == city.y then found = true end
end
print(string.format("schooling_door: University sites with the City: %d", sites.n_rows))
verify.expect(found, "the finder lists a University site at the City")
verify.site_clear()
verify.construction_view(0)
verify.frames(2)
look_at(city)
local cu = door_row("University")
verify.expect(cu ~= nil, "the door lists University at a City")
if cu then
    verify.expect(cu.hover:find("too small", 1, true) == nil, "the door does not refuse a University at a City")
    verify.expect(cu.hover:find("research points", 1, true) ~= nil, "University's row says what it buys")
end
verify.capture("schooling_door_city")
verify.ledger_build("university", "", "")
verify.frames(3)
verify.expect(count_type("University") == unis_before + 1, "the press stands a University at a City")
