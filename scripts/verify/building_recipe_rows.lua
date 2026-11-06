-- A selected own building shows its recipe, as rows (Ben, 2026-11-05;
-- docs/ui/SELECTION.md § A selected building): inputs and outputs per quarter, each input
-- flagged when short, owner only.
--
-- What this check reads: the rows the card drew this frame, each with its figure, shelf,
-- mark and per-column hover text, and the states they must keep apart:
--   R1  NO QUARTER READ (the state a loaded game opens in): the heading says so, and an
--       output reads a dash, never a zero.
--   R2  a quarter read: the output has a figure, and the input's hover calls its figure
--       an estimate, because the sim reports one input only.
--   R3  a short input carries a mark whose hover says which kind of knowledge it is.
--       Stocking the shelf clears an estimated mark.
--   R4  a save and a load: the report is gone, so the card reads R1 again.
--   R5  a rival's building draws no rows.
--
-- Run with: ProjectIo --verify scripts/verify/building_recipe_rows.lua

verify.window(1720, 1080)
verify.goto_surface("home")
verify.set_balance(5000000.0)

local function close_ledgers()
    for _, p in ipairs({ "construction", "tile", "market", "balance", "corporation", "build" }) do
        verify.show_panel(p, false)
    end
end

-- An own plant, standing and unstepped, so R1 reads a world with no quarter behind it.
-- The fixture is the real construct_building, minus the construction wait. Refined
-- Copper takes an input, so the rows have both kinds.
local works = verify.seed_processing("refined_copper")
verify.expect(works.result == "placed", "the fixture stood a plant: " .. tostring(works.result))
if works.result ~= "placed" then return end
local plant = { x = works.x, y = works.y }
local stepped = false
print(string.format("building_recipe_rows: plant at (%d,%d)", plant.x, plant.y))

close_ledgers()
verify.select_building(plant.x, plant.y)
verify.frames(3)

local function card() return verify.tile_card() end

local function col(row, key)
    for _, c in ipairs(row.cols) do if c.key == key then return c end end
    return nil
end

local function expect_no_quarter(c, when)
    verify.expect(not c.recipe_reported, when .. ": the rows read no report")
    verify.expect(c.card_scroll_max <= 0.5,
                  string.format("%s: the card is laid out to fit, no scroll (%.1f px over)", when, c.card_scroll_max))
    for _, r in ipairs(c.recipe_rows) do
        verify.expect(r.good ~= r.id, when .. ": the row reads a display name, not the id: " .. r.good)
    end
    verify.expect(c.recipe_heading:find("no quarter yet", 1, true) ~= nil,
                  when .. ": the heading says no quarter is read: '" .. c.recipe_heading .. "'")
    for _, r in ipairs(c.recipe_rows) do
        if not r.input then
            verify.expect(r.figure ~= "+0.0" and not r.figure:find("%d"),
                          when .. ": the output '" .. r.good .. "' reads no figure, not a zero: '" .. r.figure .. "'")
            verify.expect(col(r, "figure").text:find("not a zero", 1, true) ~= nil,
                          when .. ": the output's hover says it is not a zero")
        end
    end
end

-- R1, where the world is still unstepped.
local c = card()
verify.expect(c.is_expanded and c.player, "the card is the player's own building")
verify.expect(c.recipe_drawn and c.n_recipe_rows >= 1, "the card draws recipe rows")
if not stepped then
    expect_no_quarter(c, "R1")
    verify.capture("building_recipe_rows_no_quarter")
end

-- R2: a quarter read.
verify.econ_step(1)
verify.frames(3)
c = card()
verify.expect(c.recipe_reported, "R2: the rows read the quarter's report")
verify.expect(c.recipe_heading == "Recipe", "R2: the heading is plain")
verify.expect(c.card_scroll_max <= 0.5,
              string.format("the card is laid out to fit, no scroll (%.1f px over)", c.card_scroll_max))
local inputs, outputs = 0, 0
for _, r in ipairs(c.recipe_rows) do
    print(string.format("building_recipe_rows: %s %-16s fig=%s shelf=%s mark=%s", r.input and "in " or "out",
                        r.good, r.figure, r.shelf, r.mark))
    if r.input then
        inputs = inputs + 1
        verify.expect(r.figure:sub(1, 1) == "-", "an input's figure reads as a draw: " .. r.figure)
        verify.expect(col(r, "figure").text:find("An estimate", 1, true) ~= nil,
                      "R2: the input's per-good figure is labelled an estimate")
    else
        outputs = outputs + 1
        verify.expect(r.figure:sub(1, 1) == "+", "an output reads a figure: " .. r.figure)
        verify.expect(col(r, "figure").text:find("reported", 1, true) ~= nil, "R2: the output cites the report")
    end
end
verify.expect(outputs >= 1, "the rows carry an output")
print(string.format("building_recipe_rows: %d inputs, %d outputs", inputs, outputs))
verify.capture("building_recipe_rows_reported")

-- R3: every mark says which kind of knowledge it is, and an estimated one answers the shelf.
local market = verify.market_at(plant.x, plant.y)
for _, r in ipairs(c.recipe_rows) do
    if r.mark == "binding" then
        verify.expect(col(r, "mark").text:find("report", 1, true) ~= nil, "a binding mark cites the report")
    elseif r.mark == "short" then
        verify.expect(col(r, "mark").text:find("by estimate", 1, true) ~= nil, "a short mark says it is an estimate")
        verify.stock_market(market, r.id, 100000.0)
        verify.frames(2)
        local after = card()
        for _, a in ipairs(after.recipe_rows) do
            if a.good == r.good and a.input then
                verify.expect(a.mark ~= "short" or a.shelf ~= r.shelf,
                              "stocking the shelf moves '" .. r.good .. "': mark '" .. a.mark .. "'")
                print(string.format("building_recipe_rows: stocked %s: mark '%s' -> '%s'", r.good, r.mark, a.mark))
            end
        end
    end
end

-- R4: a save and a load empty the report.
local path = "building_recipe_rows.save"
if verify.save(path) then
    verify.expect(verify.load(path), "the save loads")
    close_ledgers()
    verify.select_building(plant.x, plant.y)
    verify.frames(3)
    expect_no_quarter(card(), "R4")
    verify.capture("building_recipe_rows_after_load")
else
    print("building_recipe_rows: verify.save refused; R4 went unexercised")
end

-- R5: a rival's building draws no rows.
local rival = nil
for _, b in ipairs(verify.buildings()) do
    if not b.player and b.complete then rival = b break end
end
if rival ~= nil then
    verify.goto_surface("home")
    verify.select_building(rival.x, rival.y)
    verify.frames(3)
    local rc = card()
    if rc.is_expanded and not rc.player then
        verify.expect(not rc.recipe_drawn and rc.n_recipe_rows == 0, "R5: a rival's card draws no recipe rows")
    else
        print("building_recipe_rows: the rival is not on this body; R5 went unexercised")
    end
end

print("building_recipe_rows: done")
