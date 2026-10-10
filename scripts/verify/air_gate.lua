-- The air gate at the two doors (sprint 50; PRODUCTION.md § Chemical Plant,
-- "And it runs only there"; Ben, 2026-10-09). Scripted in place of a live click
-- on Ben's request, 2026-10-10.
--
-- A propellant route runs only under the air it needs: `propellant_electrolysis`
-- only on an airless body (atmosphere class none | thin), `propellant_atmospheric`
-- only under air. Both doors hide the route the body cannot run, and the retool
-- seam refuses it if pressed anyway.
--
--   A1  On Kepler (air) the Build door lists NO candidate row using
--       propellant_electrolysis — read from the door's OWN list
--       (`verify.build_door`, the recipe ids behind the folded rows) — and does
--       list propellant_atmospheric, so the absence is the air filter and not
--       the era or tech mask hiding the whole Chemical Works group.
--   A2  A FINISHED player Chemical Works on Kepler running propellant_atmospheric:
--       its method grid (`verify.method_grid`, the grid's own offer) does not
--       offer propellant_electrolysis.
--   A3  Switching it onto propellant_electrolysis is refused: through the
--       set_recipe verb (`rejected_wrong_air`) and through the grid's own Switch
--       press (`wrong_air`), and the status line under the grid says so.
--   A5  The first airless body: a seeded Chemical Works running
--       propellant_electrolysis does not offer the atmospheric route, and the
--       set_recipe verb refuses it (or the seed's refusal is printed, SKIPPED).
--   A4  Every airless body in the verify world (surveyed first, through the
--       verify set_survey fixture, so the door has ground to stand on): the Build
--       door does not list propellant_atmospheric, and does list
--       propellant_electrolysis.
--
-- "Kepler" is the design's name for the home planet; the verify world's home
-- carries a generated name, so the script finds it by the `home` flag.
--
-- Fixtures: `verify.seed_processing` builds through the real construct_building
-- and only skips the construction wait. Nothing here changes the game.

local VERB_SET_RECIPE = 2 -- corp_verb::set_recipe
local ELEC = "propellant_electrolysis"
local ATMO = "propellant_atmospheric"
local REFUSED_AIR = "Switch refused: that method cannot run on this body's air."

local function join(list, sep) -- the verify sandbox opens no `table` library
    local s = ""
    for i, v in ipairs(list) do s = s .. (i > 1 and sep or "") .. tostring(v) end
    return s
end

local function has(list, name)
    for _, n in ipairs(list) do
        if n == name then return true end
    end
    return false
end

local function door_for(col, row, label)
    verify.select_tile(col, row)
    verify.show_panel("construction", true)
    verify.construction_view(0)
    verify.frames(2)
    local d = verify.build_door()
    local names = join(d.recipes, ", ")
    print(string.format("air_gate: door on %s tile (%d,%d) lists %d processing rows: %s",
        label, col, row, d.count, names))
    return d
end

-- --- The body roster -------------------------------------------------------
local airs = verify.body_airs()
local kepler
for _, b in ipairs(airs) do
    print(string.format("air_gate: body %s atmosphere=%s airless=%s surveyed=%s",
        b.name, b.atmosphere, tostring(b.airless), tostring(b.surveyed)))
    if b.home then kepler = b end
end
verify.expect(kepler ~= nil, "A0 the verify world has a home planet")
if not kepler then return end
print("air_gate: the home planet (Kepler) is " .. kepler.name)
verify.expect(not kepler.airless, "A0 Kepler holds air: atmosphere=" .. kepler.atmosphere)

-- --- A1: the Build door on Kepler ------------------------------------------
verify.goto_surface(kepler.name)
local anchor
for _, b in ipairs(verify.buildings()) do
    if b.player and b.body == kepler.body then anchor = b; break end
end
verify.expect(anchor ~= nil, "A1 the player has a building on Kepler to frame the door from")
if not anchor then return end
verify.center_tile(anchor.x, anchor.y)

local d = door_for(anchor.x, anchor.y, "Kepler")
verify.expect(d.drawn, "A1 the Build door drew on the selected Kepler tile")
verify.expect(not has(d.recipes, ELEC),
    "A1 the Kepler Build door lists no propellant_electrolysis row")
verify.expect(has(d.recipes, ATMO),
    "A1 control: the Kepler Build door does list propellant_atmospheric "
    .. "(so the absence above is the air filter, not the group masked out)")
verify.capture("air_gate_door_kepler")

-- --- A2: the method grid of a finished Chemical Works on Kepler ------------
verify.set_balance(100000.0)
local works = verify.seed_processing(ATMO)
verify.expect(works.result == "placed",
    "A2 a player Chemical Works running propellant_atmospheric is seeded on Kepler; got "
    .. tostring(works.result))
if works.result ~= "placed" then return end
verify.expect(works.recipe == ATMO, "A2 the seeded works runs " .. tostring(works.recipe))

verify.center_tile(works.x, works.y)
verify.select_building(works.x, works.y)
verify.show_panel("construction", true)
verify.construction_view(1)
verify.frames(2)
local c = verify.construction_controls()
verify.expect(c.levers_ok and c.levers_for == works.id,
    string.format("A2 the Buildings view drew the levers for the works: levers_for=%d id=%d",
        c.levers_for, works.id))
local g = verify.method_grid(works.id)
print("air_gate: Kepler works grid offers: " .. join(g.recipes, ", "))
verify.expect(g.active == ATMO, "A2 the works' active method is " .. tostring(g.active))
verify.expect(not has(g.recipes, ELEC), "A2 the method grid does not offer propellant_electrolysis")
verify.expect(has(g.recipes, ATMO), "A2 the method grid lists the active route")
verify.capture("air_gate_method_grid_kepler")

-- --- A3: the wrong-air switch is refused, and said -------------------------
local r = verify.corp_command{ verb = VERB_SET_RECIPE, subject = works.id, recipe_name = ELEC }
verify.expect(r == "rejected_wrong_air", "A3 set_recipe onto propellant_electrolysis: " .. r)

local pr = verify.method_grid_press(works.id, ELEC)
verify.expect(pr == "wrong_air", "A3 the grid's Switch press onto propellant_electrolysis: " .. pr)
verify.frames(2)
local g2 = verify.method_grid(works.id)
verify.expect(g2.active == ATMO, "A3 the refusal changed nothing: active=" .. tostring(g2.active))
verify.expect(g2.status == REFUSED_AIR, "A3 the grid recorded the refusal: " .. g2.status)
-- The line must be DRAWN, not merely recorded. The grid returns early on "Only one
-- method available." before its status line, and an air-gated Chemical Works
-- always has exactly one runnable method, so this is where that shows.
verify.expect(g2.status_drawn,
    "A3 the status line under the grid SHOWS the refusal (status_drawn=" .. tostring(g2.status_drawn) .. ")")
verify.capture("air_gate_method_grid_refused")

-- --- A4: the airless bodies ------------------------------------------------
local airless = 0
for _, b in ipairs(airs) do
    if b.airless then
        airless = airless + 1
        verify.set_survey(b.name, 1000000) -- clamps to the body's region total: surveyed
        verify.goto_surface(b.name)
        verify.center_tile(0, 0)
        local da = door_for(0, 0, b.name)
        verify.expect(da.drawn, "A4 the Build door drew on " .. b.name)
        verify.expect(not has(da.recipes, ATMO),
            "A4 the " .. b.name .. " Build door lists no propellant_atmospheric row")
        verify.expect(has(da.recipes, ELEC),
            "A4 control: the " .. b.name .. " Build door lists propellant_electrolysis")
        verify.capture("air_gate_door_" .. string.lower(string.gsub(b.name, " ", "_")))
    end
end
print(string.format("air_gate: %d airless bod%s checked", airless,
    airless == 1 and "y" or "ies"))
verify.expect(airless > 0, "A4 the verify world carries at least one airless body")

-- --- A5: the method grid on an airless body --------------------------------
-- The same seed through the real construct_building. The player has no reach on
-- an airless body in the verify world, so the seed falls back to inserting the
-- works (`direct`) — placement is not the subject here; the air is, and the
-- fallback still refuses a recipe the body cannot run. Any other refusal is
-- printed and the grid half reported SKIPPED, never counted as a pass.
for _, b in ipairs(airs) do
    if b.airless then
        verify.goto_surface(b.name)
        local w2 = verify.seed_processing(ELEC, true)
        print("air_gate: A5 seed on " .. b.name .. ": " .. w2.result)
        if w2.result ~= "placed" and w2.result ~= "inserted" then
            print("air_gate: A5 SKIPPED on " .. b.name .. " - the seed was refused: " .. w2.result)
        else
            verify.center_tile(w2.x, w2.y)
            verify.select_building(w2.x, w2.y)
            verify.show_panel("construction", true)
            verify.construction_view(1)
            verify.frames(2)
            local ga = verify.method_grid(w2.id)
            print("air_gate: " .. b.name .. " works grid offers: " .. join(ga.recipes, ", "))
            verify.expect(ga.active == ELEC, "A5 the airless works runs " .. tostring(ga.active))
            verify.expect(not has(ga.recipes, ATMO),
                "A5 the " .. b.name .. " method grid does not offer propellant_atmospheric")
            local ra = verify.corp_command{ verb = VERB_SET_RECIPE, subject = w2.id, recipe_name = ATMO }
            verify.expect(ra == "rejected_wrong_air", "A5 set_recipe onto propellant_atmospheric: " .. ra)
            verify.capture("air_gate_method_grid_airless")
        end
        break
    end
end
