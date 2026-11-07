-- The Build door and the site finder offer Schooling and University (Ben, 2026-11-07;
-- docs/ui/SELECTION.md § The tile construction ledger). The stratum gate stays what decides
-- where (docs/economy/POPULATION.md § Strata gate buildings): Schooling any centre, a University
-- City (4) or above.
--
-- What this check reads: the finder's options, its rows, and the rows the door drew. A finder
-- row is a tile the gate passed, so a University row that is not a Schooling row would mean
-- the two gates disagree; and the door on a tile with no centre must refuse both.
--
-- Run with: ProjectIo --verify scripts/verify/schooling_door.lua

verify.window(1720, 1080)
verify.goto_surface("home")
verify.set_balance(5000000.0)
verify.show_panel("build", true)
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
    verify.expect(verify.site_pick(name), "the finder arms " .. name)
    verify.frames(3)
    local s = verify.site_finder()
    local out = {}
    for i = 1, s.n_rows do
        out[i] = { x = s.rows[i].x, y = s.rows[i].y }
    end
    return out, s
end

local school_rows, school = rows_of("Schooling")
print(string.format("schooling_door: Schooling %d sites of %d own tiles", #school_rows, school.own_tiles))
verify.expect(#school_rows > 0, "the player has a centre to school")
verify.expect(#school_rows < school.own_tiles, "Schooling is gated: not every own tile is a centre")
verify.capture("schooling_door_finder")

local key = function(r) return r.x .. "," .. r.y end
local at_centre = {}
for _, r in ipairs(school_rows) do at_centre[key(r)] = true end

local uni_rows = rows_of("University")
print(string.format("schooling_door: University %d sites", #uni_rows))
verify.expect(#uni_rows <= #school_rows, "a University's ground is no wider than Schooling's")
for _, r in ipairs(uni_rows) do
    verify.expect(at_centre[key(r)], "a University site is also a Schooling site: " .. key(r))
end

-- A centre too small for a University is a Schooling site that holds no University site.
local at_city = {}
for _, r in ipairs(uni_rows) do at_city[key(r)] = true end
-- (A tile with a site under way refuses another, so each press below takes its own tile.)
local town = nil
for i = 2, #school_rows do
    if not at_city[key(school_rows[i])] then town = school_rows[i]; break end
end
verify.site_clear()
verify.frames(2)

-- Find a door's row, by name, in the Infrastructure fold.
local function door_row(name)
    local d = verify.build_door()
    for _, r in ipairs(d.rows) do
        if r.name == name then return r, d end
    end
    return nil, d
end

-- The door names both rows on any own tile, and its hover names the reason on a refusal.
local function look_at(x, y)
    verify.select_tile(x, y)
    verify.frames(4)
    verify.scroll_panel("build_list", 1.0)
    verify.frames(3)
end

local centre = school_rows[1]
look_at(centre.x, centre.y)
verify.capture("schooling_door_row")
local sr, door = door_row("Schooling")
verify.expect(door.n_rows > 0, "the door drew its rows")
verify.expect(sr ~= nil, "the door lists Schooling on a centre")
if sr then
    verify.expect(sr.fold == "Infrastructure", "the door files Schooling under Infrastructure, not " .. sr.fold)
    print("schooling_door: Schooling hover: " .. sr.hover)
    verify.expect(sr.hover:find("qualification", 1, true) ~= nil, "Schooling's row says what it buys")
end
local ur = door_row("University")
verify.expect(ur ~= nil, "the door lists University on a centre")
if ur then
    print("schooling_door: University hover: " .. ur.hover)
    verify.expect(ur.fold == "Infrastructure", "the door files University under Infrastructure, not " .. ur.fold)
end

if town then
    look_at(town.x, town.y)
    local u = door_row("University")
    verify.expect(u ~= nil, "the door lists University on a small centre, refused")
    if u then
        print(string.format("schooling_door: University at a small centre (%d,%d): %s", town.x, town.y, u.hover))
        verify.expect(u.hover:find("too small", 1, true) ~= nil,
                      "a small centre's University row says the settlement is too small")
    end
    verify.capture("schooling_door_town")
else
    print("schooling_door: every centre the player holds is a City: no small centre to refuse")
end

-- WHAT THE GROUND HOLDS (Rule 0b, the numbers, unasserted): how many centres stand at each
-- rung, across the body, and how many of the player's are City or above.
local hist = { 0, 0, 0, 0, 0 }
for _, c in ipairs(verify.population_centres()) do
    if hist[c.scale] then hist[c.scale] = hist[c.scale] + 1 end
end
print(string.format("schooling_door: centres by scale 1..5: %d %d %d %d %d; the player's City+: %d of %d",
                    hist[1], hist[2], hist[3], hist[4], hist[5], #uni_rows, #school_rows))

local function count_type(name)
    local n = 0
    for _, b in ipairs(verify.buildings()) do
        if b.player and b.type == name then n = n + 1 end
    end
    return n
end

-- THE BUILD PRESS'S SEAM, on a centre: Schooling goes up.
local before = count_type("Schooling")
look_at(centre.x, centre.y)
verify.ledger_build("schooling", "", "")
verify.frames(3)
verify.expect(count_type("Schooling") == before + 1, "the press stands a Schooling on a centre")

-- A town refuses a University (the press's own gate, not only the row's words).
local uni_before = count_type("University")
if town then
    look_at(town.x, town.y)
    verify.ledger_build("university", "", "")
    verify.frames(3)
    verify.expect(count_type("University") == uni_before, "a town refuses a University: you can't build a university in a town")
end

-- A City takes one. STAGED when the player holds none: the gate is what is under test.
local city = uni_rows[1]
if city == nil then
    local stage = nil
    for i = 2, #school_rows do
        local r = school_rows[i]
        if not town or key(r) ~= key(town) then stage = r; break end
    end
    verify.expect(stage ~= nil, "a second small centre to stage")
    verify.expect(verify.stage_centre(stage.x, stage.y, 4), "a centre staged to City")
    verify.set_balance(4000000.0) -- the finder re-reads the ground when its key moves
    verify.frames(2)
    city = stage
end
verify.expect(verify.site_pick("University"), "the finder arms University")
verify.frames(3)
local staged_sites = verify.site_finder()
local found = false
for i = 1, staged_sites.n_rows do
    local r = staged_sites.rows[i]
    if r.x == city.x and r.y == city.y then found = true end
end
print(string.format("schooling_door: University sites with a City: %d", staged_sites.n_rows))
verify.expect(found, "the finder lists a University site at the City")
verify.site_clear()
verify.frames(2)
look_at(city.x, city.y)
local cu = door_row("University")
verify.expect(cu ~= nil and cu.hover:find("too small", 1, true) == nil, "the door does not refuse a University at a City")
verify.capture("schooling_door_city")
verify.ledger_build("university", "", "")
verify.frames(3)
verify.expect(count_type("University") == uni_before + 1, "the press stands a University at a City")
