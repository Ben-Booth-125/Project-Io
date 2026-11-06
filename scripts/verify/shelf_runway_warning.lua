-- The shelf runway warning (Ben, 2026-11-05; docs/ui/ledgers/construction.md § The site
-- ledger): the Build door row's hover and the site ledger's row warn when a recipe input
-- has only a few quarters left on that market's shelf, naming the good and the quarters.
--
-- What this check reads: each finder row's own reading, the Net hover as the row drew it,
-- and the door's bar column for the same recipe at the same tile. The world's shelves at
-- the opening are deep, so a warning has to be staged: `verify.set_input_shelf` writes the
-- shelf of an option's inputs on one market.
--   W0  the census: how many rows warn on the world as generated, printed for Ben.
--   W1  the reading's arithmetic: quarters = shelf / (use + drain), under the threshold.
--   W2  no double warning: a starved row carries no runway warning.
--   W3  the door and the finder say the same sentence, on one shared helper.
--   W4  a shelf past the threshold does not warn.
--   W5  the drain comes from the shelf's own trail: a shelf that fell says so.
--   W6  a load has no trail: the reading is the plant's use alone, and it says so.
--   W7  one quarter is singular.
--
-- Run with: ProjectIo --verify scripts/verify/shelf_runway_warning.lua

verify.window(1720, 1080)
verify.goto_surface("home")
verify.set_balance(50000000.0)
verify.show_panel("build", true)
verify.frames(3)

local k_threshold = 4.0 -- k_shelf_runway_warn_quarters, the default Ben reads live

local function has(text, part) return text:find(part, 1, true) ~= nil end

-- What the sentence says. The reading is checked against the drawn figures, so nothing
-- about the shelf reaches this script that the player cannot read.
local function read_sentence(t)
    local use = tonumber(t:match("it would use ([%d%.]+) a quarter")) or tonumber(t:match("at the ([%d%.]+) a quarter"))
    local drain = tonumber((t:match("already falling ([%d]+%.?%d?)"))) or 0.0
    local good = t:match("quarters? of (.-) left on")
    local whole = tonumber(t:match("about (%d+) quarter"))
    return { use = use or 0.0, drain = drain, good = good or "", whole = whole, under = has(t, "Under a quarter") }
end
-- The quarters the sentence states agree with shelf / (use + drain), to its rounding.
local function agrees(s, shelf)
    local q = shelf / math.max(0.001, s.use + s.drain)
    if s.under then return q < 1.1 end
    return s.whole ~= nil and q >= s.whole - 0.2 and q < s.whole + 1.2
end

verify.econ_step(1)
verify.frames(2)

-- W0 and W2: every processing option's rows, and the covered rows to stage on.
local function census(label)
    local candidates = {}
    local warned, starved, clean, no_cover, double = 0, 0, 0, 0, 0
    for _, o in ipairs(verify.site_options()) do
        if o.group ~= "" then
            verify.site_pick(o.name)
            verify.frames(2)
            local cand = nil
            for _, r in ipairs(verify.site_finder().rows) do
                if r.starved then
                    starved = starved + 1
                    if r.runway_warn then double = double + 1 end
                elseif r.runway_warn then
                    warned = warned + 1
                else
                    clean = clean + 1
                    if not (r.cover > 0.0) then no_cover = no_cover + 1 end
                end
                -- A starved row may only be starved of a shelf the staging is about to
                -- write, so it is a candidate as much as a clean one.
                if cand == nil and r.cover > 0.0 then cand = r end
            end
            if cand ~= nil then candidates[#candidates + 1] = { option = o, row = cand } end
        end
    end
    print(string.format("shelf_runway_warning: census, %s: warned=%d starved=%d clean=%d (of those, no cover=%d); %d options on covered ground",
                        label, warned, starved, clean, no_cover, #candidates))
    verify.expect(double == 0, "W2: no starved row carries a runway warning, " .. label .. " (" .. double .. ")")
    return candidates
end

local candidates = census("the world as generated")

-- The verify world can hold no running source of Logistic Points at all, and a row with no
-- cover would not run, so it has no runway to warn of. Stage one hub beside the capital, as
-- construction_sites does, and a tick so the resolve sees it.
if #candidates == 0 then
    local cs = verify.capital_stars()
    if cs.x then
        print("shelf_runway_warning: staged a logistics hub beside the capital: "
              .. tostring(verify.stage_building(cs.x, cs.y - 1, "logistics_hub")))
        verify.econ_step(1)
        verify.set_balance(50000000.0)
        verify.frames(2)
        candidates = census("with a hub staged")
    end
end

local function row_at(x, y)
    for _, r in ipairs(verify.site_finder().rows) do
        if r.x == x and r.y == y then return r end
    end
    return nil
end

-- W1: stage a low shelf until a row warns rather than starves. A very low shelf can also
-- take the plant's own bid over the reservation, which is the starved case and rightly
-- wins, so the staging climbs a ladder of shelves and takes the first that warns.
local picked, reading = nil, nil
local k_stage = 0.0
local ladder = { 1.0, 4.0, 16.0, 64.0, 256.0, 1024.0 }
for _, c in ipairs(candidates) do
    local market = verify.market_at(c.row.x, c.row.y)
    verify.site_pick(c.option.name)
    for _, shelf in ipairs(ladder) do
        local wrote = verify.set_input_shelf(c.option.name, market, shelf)
        verify.frames(2)
        local r = row_at(c.row.x, c.row.y)
        if r ~= nil and wrote > 0 then
            print(string.format("shelf_runway_warning: staged %s at %.0f on %s: warn=%s starved=%s", c.option.name, shelf,
                                r.market, tostring(r.runway_warn), tostring(r.starved)))
            verify.expect(not (r.starved and r.runway_warn), "W2: staged, a starved row still carries one warning")
            if r.runway_warn then
                picked, reading, k_stage = { option = c.option, x = r.x, y = r.y, market = market }, r, shelf
                break
            end
        end
    end
    if picked ~= nil then break end
end
verify.expect(picked ~= nil, "a staged low shelf warned on some option")
if picked == nil then return end

local r = reading
local said = read_sentence(r.runway_text)
print(string.format("shelf_runway_warning: %s: %s", picked.option.name, r.runway_text))
verify.expect(said.good ~= "", "W1: the sentence names the good: " .. r.runway_text)
verify.expect(said.under or (said.whole ~= nil and said.whole < k_threshold),
              "W1: the warning is under the threshold: " .. r.runway_text)
verify.expect(said.use > 0.0, "W1: the sentence states the plant's use: " .. r.runway_text)
verify.expect(agrees(said, k_stage), "W1: quarters = shelf / (use + drain): " .. r.runway_text)
verify.expect(has(r.runway_text, "shelf"), "W1: the sentence says shelf: " .. r.runway_text)

-- The row as drawn: aim the sort at it by reading what the ledger drew.
local s = verify.site_finder()
local d_hit = nil
for _, d in ipairs(s.drawn) do
    if d.tile == r.tile then d_hit = d end
end
if d_hit ~= nil then
    verify.expect(has(d_hit.net_text, r.runway_text), "W1: the drawn Net hover opens with the warning")
else
    print("shelf_runway_warning: the warned row is not among the rows on screen; its drawn hover went unread")
end
for i, d in ipairs(s.drawn) do
    local row = s.rows[i]
    if row ~= nil and d.tile == row.tile and row.starved then
        verify.expect(not has(d.net_text, "left on this market's shelf"), "W2: a starved row's hover has no runway sentence")
    end
end
-- The Net cell sits one Place column to the right of the row's published aim.
if d_hit ~= nil then
    verify.hover(d_hit.x + 57, d_hit.y, 40)
end
verify.capture("shelf_runway_warning_finder")
verify.hover(10, 10, 1)

-- W3: the door says the finder's sentence. The door folds a group to its best recipe, and
-- which one is best moves with the shelves, so stage every recipe of a group low on one
-- market and match the door's sentence against the finder's rows for that group on that
-- tile. A door that said anything of its own would match none of them.
local function bar_of(group)
    for _, dr in ipairs(verify.build_door().rows) do
        if dr.name == group then
            for _, c in ipairs(dr.cols) do if c.key == "bar" then return c end end
        end
    end
    return nil
end

local all_options = verify.site_options()
local matched, idle_seen = false, 0
for _, c in ipairs(candidates) do
    local market = verify.market_at(c.row.x, c.row.y)
    for _, shelf in ipairs(ladder) do
        local sentences = {}
        for _, o in ipairs(all_options) do
            if o.group == c.option.group then verify.set_input_shelf(o.name, market, shelf) end
        end
        for _, o in ipairs(all_options) do
            if o.group == c.option.group then
                verify.site_pick(o.name)
                verify.frames(2)
                local fr = row_at(c.row.x, c.row.y)
                if fr ~= nil and fr.runway_warn then sentences[#sentences + 1] = { text = fr.runway_text, name = o.name } end
            end
        end
        verify.select_tile(c.row.x, c.row.y)
        verify.frames(3)
        local bar = bar_of(c.option.group)
        if bar ~= nil then
            if has(bar.text, "would likely stand idle") then
                idle_seen = idle_seen + 1
                verify.expect(not has(bar.text, "left on this market's shelf"), "W3: the door does not double-warn a starved row")
            elseif has(bar.text, "left on this market's shelf") then
                for _, t in ipairs(sentences) do
                    if has(bar.text, t.text) then
                        matched = true
                        -- A door row quotes one recipe of its group, and says which.
                        if t.name ~= c.option.group then
                            verify.expect(has(bar.text, "Quoted for " .. t.name),
                                          "W3: the door names the recipe it quotes: " .. bar.text)
                        end
                    end
                end
                verify.expect(matched, "W3: the door's sentence is one of the finder's for this group: " .. bar.text)
                print("shelf_runway_warning: door bar: " .. bar.text)
                -- The list scrolls: bring the row into its window so the hover is the card's.
                for step = 0, 20 do
                    verify.scroll_panel("build_list", step / 20)
                    verify.frames(2)
                    bar = bar_of(c.option.group)
                    if bar.y > 640 and bar.y < 780 then break end
                end
                verify.hover(bar.x, bar.y, 40)
                verify.capture("shelf_runway_warning_door")
                verify.hover(10, 10, 1)
                break
            end
        end
    end
    if matched then break end
end
print(string.format("shelf_runway_warning: W3 door: matched=%s, %d starved door rows on the way", tostring(matched), idle_seen))
verify.expect(matched, "W3: a door row warned, with the finder's sentence")

verify.site_pick(picked.option.name)
verify.frames(2)

-- W4: a shelf just past the threshold does not warn on this good.
local past = (said.use + said.drain) * (k_threshold + 0.5)
verify.set_input_shelf(picked.option.name, picked.market, past)
verify.frames(2)
local after = row_at(picked.x, picked.y)
verify.expect(after ~= nil, "the row is still listed")
if after ~= nil then
    verify.expect(not (after.runway_warn and read_sentence(after.runway_text).good == said.good),
                  string.format("W4: %.1f units, %.1f quarters, no warning on %s: %s", past, k_threshold + 0.5,
                                said.good, after.runway_text))
end

-- W7: one quarter is singular.
verify.set_input_shelf(picked.option.name, picked.market, said.use * 1.5)
verify.frames(2)
local single = row_at(picked.x, picked.y)
verify.expect(single ~= nil and single.runway_warn, "W7: a quarter and a half warns")
if single ~= nil and single.runway_warn then
    print("shelf_runway_warning: single: " .. single.runway_text)
    verify.expect(not has(single.runway_text, "1 quarters"), "W7: no '1 quarters': " .. single.runway_text)
    verify.expect(has(single.runway_text, "about 1 quarter of"), "W7: 'about 1 quarter': " .. single.runway_text)
end

-- W5: a falling shelf, staged down a step a tick so the whole trail reads the fall.
for _, shelf in ipairs({ 5000.0, 4000.0, 3000.0, 2000.0, 1000.0 }) do
    verify.set_input_shelf(picked.option.name, picked.market, shelf)
    verify.econ_step(1)
end
verify.set_input_shelf(picked.option.name, picked.market, k_stage)
verify.frames(2)
local falling = row_at(picked.x, picked.y)
verify.expect(falling ~= nil, "the row is still listed after the ticks")
if falling ~= nil and falling.runway_warn then
    print("shelf_runway_warning: falling: " .. falling.runway_text)
    verify.expect(has(falling.runway_text, "already falling"), "W5: the sentence says the shelf is falling")
    verify.expect(not has(falling.runway_text, "alone"), "W5: a read trail does not say it knows the use alone")
    verify.expect(agrees(read_sentence(falling.runway_text), k_stage), "W5: the drain counts in the quarters")
else
    print("shelf_runway_warning: W5's row does not warn after the ticks: starved=" .. tostring(falling and falling.starved))
    verify.expect(falling ~= nil and falling.starved, "W5: the only reason not to warn is that it starved")
end

-- W6: a load has no trail, and the sentence says it knows only the plant's own use.
local path = "shelf_runway_warning.save"
local saved = verify.save(path)
verify.expect(saved, "W6: the save is written")
if saved then
    verify.expect(verify.load(path), "the save loads")
    verify.show_panel("build", true)
    verify.site_pick(picked.option.name)
    verify.frames(2)
    verify.set_input_shelf(picked.option.name, picked.market, k_stage)
    verify.frames(2)
    local loaded = row_at(picked.x, picked.y)
    verify.expect(loaded ~= nil and loaded.runway_warn, "W6: the staged shelf warns after the load")
    if loaded ~= nil and loaded.runway_warn then
        print("shelf_runway_warning: after load: " .. loaded.runway_text)
        verify.expect(not has(loaded.runway_text, "already falling"), "W6: the sentence claims no fall")
        verify.expect(has(loaded.runway_text, "its own use alone"), "W6: and says it knows only the plant's use")
        verify.expect(agrees(read_sentence(loaded.runway_text), k_stage), "W6: the reading is the plant's use alone")
    end
end

verify.site_clear()
print("shelf_runway_warning: done")
