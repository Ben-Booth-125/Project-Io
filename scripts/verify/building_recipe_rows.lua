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
--   R6  pointing at a row opens one card, and it holds that row's text alone.
--   R7  where the sim runs no batch, an input reads a dash and no shortage.
--   R8  an extraction site's rows are its basket, each share labelled an estimate.
--   R9  buildings the sim ran: a plant's inputs read as estimated draws, and a worked site's
--       shares sum to the reported total.
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

-- R6: each row's hover is its own. Rows are one line tall; a two-line rect would let one
-- point open this row's card and the row above's.
local function hover_is_its_own(c, when)
    for _, r in ipairs(c.recipe_rows) do
        for _, col in ipairs(r.cols) do
            verify.hover(col.x, col.y, 3)
            local h = card()
            verify.expect(h.recipe_hover_cards == 1,
                          string.format("%s: pointing at %s/%s opens one card (%d)", when, r.good, col.key, h.recipe_hover_cards))
            verify.expect(h.recipe_hover_who == r.good and h.recipe_hover_text == col.text,
                          string.format("%s: the card is %s/%s's alone, not '%s'", when, r.good, col.key, h.recipe_hover_who))
        end
    end
    verify.hover(10, 10, 1)
end
hover_is_its_own(card(), "R6 unread")

-- R3: every mark says which kind of knowledge it is, and an estimated one answers the shelf.
-- Read on the unstepped plant, which is still judged at its staffing.
local market = verify.market_at(plant.x, plant.y)
local marks = 0
for _, r in ipairs(card().recipe_rows) do
    verify.expect(r.mark ~= "binding", "R3: an unread plant has no reported binding input")
    if r.mark == "short" then
        marks = marks + 1
        verify.expect(col(r, "mark").text:find("by estimate", 1, true) ~= nil, "a short mark says it is an estimate")
        verify.stock_market(market, r.id, 100000.0)
        verify.frames(2)
        for _, a in ipairs(card().recipe_rows) do
            if a.good == r.good and a.input then
                verify.expect(a.mark ~= "short", "stocking the shelf clears '" .. r.good .. "': mark '" .. a.mark .. "'")
            end
        end
    end
end
verify.expect(marks >= 1, "R3: the plant's bare shelf gave a short mark to read")
verify.frames(2)

-- R2: a quarter read. Scarce labour goes to the oldest buildings first, so this new plant
-- can go on getting none: that is R7, read here. R9 reads an older plant that does run.
local no_draw_seen = false
local function draws_none(c)
    for _, r in ipairs(c.recipe_rows) do
        if r.input and not r.figure:find("%d") then return true end
    end
    return false
end
for _ = 1, 4 do
    verify.econ_step(1)
    verify.frames(3)
    c = card()
    verify.expect(c.recipe_reported, "R2: the rows read the quarter's report")
    verify.expect(c.recipe_heading == "Recipe", "R2: the heading is plain")
    verify.expect(c.card_scroll_max <= 0.5,
                  string.format("the card is laid out to fit, no scroll (%.1f px over)", c.card_scroll_max))
    if not draws_none(c) then break end
    if not no_draw_seen then
        no_draw_seen = true
        print("building_recipe_rows: no labour yet: the inputs read no draw")
        verify.capture("building_recipe_rows_no_draw")
    end
    -- R7: where the sim runs no batch there is no appetite and no shortage.
    for _, r in ipairs(c.recipe_rows) do
        if r.input then
            verify.expect(r.mark == "", "R7: a plant that draws nothing is short of nothing: " .. r.good)
            verify.expect(col(r, "figure").text:find("dr[ae]w") ~= nil,
                          "R7: the hover says it draws nothing: " .. col(r, "figure").text)
        end
    end
end
local inputs, outputs = 0, 0
local ran = not draws_none(c)
print("building_recipe_rows: a report with a draw reached: " .. tostring(ran))
for _, r in ipairs(c.recipe_rows) do
    print(string.format("building_recipe_rows: %s %-16s fig=%s shelf=%s mark=%s", r.input and "in " or "out",
                        r.good, r.figure, r.shelf, r.mark))
    if r.input then
        inputs = inputs + 1
        if ran then
            verify.expect(r.figure:sub(1, 1) == "-", "an input's figure reads as a draw: " .. r.figure)
            verify.expect(col(r, "figure").text:find("An estimate", 1, true) ~= nil,
                          "R2: the input's per-good figure is labelled an estimate")
        end
    else
        outputs = outputs + 1
        verify.expect(r.figure:sub(1, 1) == "+", "an output reads a figure: " .. r.figure)
        verify.expect(col(r, "figure").text:find("reported", 1, true) ~= nil, "R2: the output cites the report")
    end
end
verify.expect(outputs >= 1, "the rows carry an output")
print(string.format("building_recipe_rows: %d inputs, %d outputs", inputs, outputs))
hover_is_its_own(c, "R6 reported")
verify.capture("building_recipe_rows_reported")

-- R7, staged: a plant with no staff target draws nothing, whatever a scorer would judge it at.
verify.set_building_workforce(works.tile, 0)
verify.frames(3)
c = card()
verify.expect(draws_none(c), "R7: a zero staff target reads no draw")
for _, r in ipairs(c.recipe_rows) do
    if r.input then verify.expect(r.mark == "", "R7: and no shortage: " .. r.good .. " '" .. r.mark .. "'") end
end
verify.set_building_workforce(works.tile, 100)
verify.frames(2)

-- R8: an extraction site's rows are its basket. Find a tile working more than one deposit.
local goods = { "iron_ore", "copper_ore", "coal", "limestone", "clay", "sand" }
local basket = nil
for _, g in ipairs(goods) do
    local d = verify.find_deposit_tile(g)
    if d ~= nil and d.x ~= nil then
        if verify.stage_building(d.x, d.y, "extraction", g) ~= 0 then
            close_ledgers()
            verify.select_building(d.x, d.y)
            verify.frames(3)
            local ec = card()
            local n = 0
            for _, r in ipairs(ec.recipe_rows) do
                if not r.input then n = n + 1 end
            end
            print(string.format("building_recipe_rows: a %s site at (%d,%d) yields %d good(s)", g, d.x, d.y, n))
            verify.expect(n >= 1, "R8: a site shows at least one output")
            if n >= 2 then basket = { x = d.x, y = d.y } break end
        end
    end
end
verify.expect(basket ~= nil, "R8: some staged site works more than one deposit")
if basket ~= nil then
    verify.goto_surface("home")
    verify.select_building(basket.x, basket.y)
    verify.frames(3)
    c = card()
    for _, r in ipairs(c.recipe_rows) do
        verify.expect(not r.figure:find("%d"), "R8 unread: '" .. r.good .. "' reads no figure")
    end
    verify.expect(c.card_scroll_max <= 0.5,
                  string.format("R8: the basket fits the card (%.1f px over)", c.card_scroll_max))
    verify.econ_step(2)
    verify.frames(3)
    c = card()
    verify.expect(c.recipe_reported, "R8: the site has a report")
    for _, r in ipairs(c.recipe_rows) do
        local t = col(r, "figure").text
        verify.expect(t:find("an estimate", 1, true) ~= nil, "R8: a basket share is labelled an estimate: " .. t)
        verify.expect(t:find("reported total is", 1, true) ~= nil, "R8: and states the reported total: " .. t)
    end
    hover_is_its_own(c, "R6 basket")
    verify.capture("building_recipe_rows_basket")
end

-- R4: a save and a load empty the report.
local path = "building_recipe_rows.save"
local saved = verify.save(path)
verify.expect(saved, "R4: the save is written")
if saved then
    verify.expect(verify.load(path), "the save loads")
    close_ledgers()
    verify.select_building(plant.x, plant.y)
    verify.frames(3)
    expect_no_quarter(card(), "R4")
    verify.capture("building_recipe_rows_after_load")
end

-- R5: a rival's building draws no rows. Staged beside the plant when no rival stands here.
verify.goto_surface("home")
local rival = nil
for dy = 1, 6 do
    if verify.stage_building(plant.x, plant.y + dy, "extraction", "iron_ore", { rival = true }) ~= 0 then
        rival = { x = plant.x, y = plant.y + dy }
        break
    end
end
verify.expect(rival ~= nil, "R5: a rival's building stands to read")
if rival ~= nil then
    verify.select_building(rival.x, rival.y)
    verify.frames(3)
    local rc = card()
    verify.expect(rc.is_expanded and not rc.player, "R5: the card is a rival's")
    verify.expect(not rc.recipe_drawn and rc.n_recipe_rows == 0, "R5: a rival's card draws no recipe rows")
end

-- R9: buildings the sim really ran. The verify world's own builder gives its nation no labour
-- pool until it is folded to ground (verify.fold_world), so fold, step, and read. A worked
-- site's shares sum to its reported total, and a running plant's inputs are figures labelled
-- estimates, each hover its own.
local folded = verify.fold_world()
verify.expect(folded.folded, "R9: the verify world folds to ground")
verify.econ_step(2)
verify.frames(3)
local ran_plant, ran_site, ran_basket = false, false, false
local to_read = verify.buildings()
local seen_player, seen_card, seen_reported = 0, 0, 0
print("building_recipe_rows: R9 reads " .. #to_read .. " buildings")
for _, b in ipairs(to_read) do
    if b.player and b.complete and not (ran_plant and ran_basket) then
        seen_player = seen_player + 1
        verify.goto_surface("home")
        verify.select_building(b.x, b.y)
        verify.frames(2)
        local bc = card()
        if bc.is_expanded and bc.player then seen_card = seen_card + 1 end
        if bc.is_expanded and bc.player and bc.recipe_reported then
            seen_reported = seen_reported + 1
            if seen_reported <= 6 then
                for _, r in ipairs(bc.recipe_rows) do
                    print(string.format("building_recipe_rows: read (%d,%d) %s %s %s", b.x, b.y, r.input and "in" or "out", r.good, r.figure))
                end
            end
            local n_in, n_out, sum, total, drew = 0, 0, 0.0, nil, false
            for _, r in ipairs(bc.recipe_rows) do
                if r.input then
                    n_in = n_in + 1
                    if r.figure:find("%d") then drew = true end
                else
                    n_out = n_out + 1
                    sum = sum + (tonumber(r.figure) or 0.0)
                    total = tonumber(col(r, "figure").text:match("reported total is ([%d%.]+)")) or total
                end
            end
            if n_in > 0 and drew and not ran_plant then
                ran_plant = true
                print(string.format("building_recipe_rows: a running plant at (%d,%d)", b.x, b.y))
                for _, r in ipairs(bc.recipe_rows) do
                    print(string.format("building_recipe_rows:   %s %-16s fig=%s shelf=%s mark=%s", r.input and "in " or "out", r.good, r.figure, r.shelf, r.mark))
                    if r.input then
                        verify.expect(r.figure:sub(1, 1) == "-", "R9: a running plant's input reads a draw: " .. r.figure)
                        verify.expect(col(r, "figure").text:find("An estimate", 1, true) ~= nil, "R9: labelled an estimate")
                    else
                        verify.expect(r.figure:sub(1, 1) == "+", "R9: its output reads a figure: " .. r.figure)
                    end
                end
                hover_is_its_own(bc, "R9 plant")
                verify.capture("building_recipe_rows_running")
            elseif n_in == 0 and n_out >= 1 and sum > 0.0 then
                ran_site = true
                if n_out >= 2 and not ran_basket then
                    ran_basket = true
                    print(string.format("building_recipe_rows: a worked basket at (%d,%d), total %s", b.x, b.y, tostring(total)))
                    for _, r in ipairs(bc.recipe_rows) do
                        print(string.format("building_recipe_rows:   out %-16s fig=%s", r.good, r.figure))
                        verify.expect(col(r, "figure").text:find("an estimate", 1, true) ~= nil, "R9: a basket share is labelled an estimate")
                    end
                    verify.expect(total ~= nil and math.abs(sum - total) <= 0.1 * n_out,
                                  string.format("R9: the shares sum to the reported total (%.1f of %s)", sum, tostring(total)))
                    hover_is_its_own(bc, "R9 basket")
                    verify.capture("building_recipe_rows_basket_worked")
                end
            end
        end
    end
end
print(string.format("building_recipe_rows: R9 found: plant=%s site=%s basket=%s; own %d, carded %d, reported %d", tostring(ran_plant), tostring(ran_site), tostring(ran_basket), seen_player, seen_card, seen_reported))
verify.expect(ran_plant, "R9: a running plant was read")
verify.expect(ran_basket, "R9: a worked basket was read")

print("building_recipe_rows: done")
