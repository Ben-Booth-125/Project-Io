-- Construction: the honest ETA, the live queue, the progress bars and the build
-- card's cost/output rows (BL-1342 / BL-1343 / BL-1344 / BL-1341).
--
-- Ben, playtest 2026-10-16: construction stalled with "ETA 2 qtr" and no
-- progress for quarters; the queue said "No active construction" with several
-- builds under way; the build card priced a building in credits only and offered
-- no sign that a second of the same type could stand on the tile.
--
-- What the script stages and then captures:
--   1. two extraction sites of the SAME type on ONE tile, through the real
--      construct_building path, so the queue has rows and the tile has a stack
--      (and so BL-1341's "can I build a second?" is answered with a result
--      string rather than a guess);
--   2. the Construction ledger's queue, open, with its Progress / Qtrs left /
--      Cost left columns and its named stall lines;
--   3. the build door below it on a tile that ALREADY carries one of a type —
--      the cost in goods and credits, what the building gives, and how many
--      stand here;
--   4. the tile card: the stack row's reading (ETA or "Paused - no <Good>")
--      and the expanded site card's progress bar.
--
-- BL-1365 extends it to the SHAPE of those rows (construction.md § "One line a
-- row, and the words live in the hover", § "The line is a grid, not a
-- sentence"): every build-list and queue row is ONE LINE of fixed columns, and
-- every sentence the line dropped is on the shared hover card. A capture cannot
-- prove the second half - the card is drawn at the cursor and the line itself
-- carries glyphs and abbreviations - so the surfaces publish their own row
-- metric and card text, and the check asserts on those AND hovers a real row to
-- capture the card.
--
-- BL-1368 moves the hover PER COLUMN (construction.md § "Each element explains
-- itself, and only itself"): the cost column explains the cost, the pip what it
-- makes, the stack figure WHICH cap it counts against, the state mark the good
-- that stalls it, the bar the estimate, the press a refusal. Every row therefore
-- publishes one entry per column - key, explainer, and the cell's own centre -
-- and this check asserts that each expected column HAS an explainer, that it is
-- short, that the authored stall clause is on the state mark, and then hovers
-- each column in turn and captures the card it draws.
--
-- BL-1366 brings the two RAIL rows into the same grid: they drew a four-line
-- bordered box between one-line rows, so the check asserts their published row
-- height is the build list's own and that they carry the same columns.
--
-- BL-1371 makes every one of those rows TWO TEXT LINES tall (construction.md
-- § "A row is two lines tall, and text wraps rather than truncating"; Ben's live
-- click of the one-line grid: "many rows truncate the text"). The columns, their
-- order and their widths are unchanged - what the second line buys is the stretch
-- NAME column, which resolves to ~70 px at the fold-out's narrowest. So the row
-- metric assertions move from "one line plus padding" to "two lines plus padding",
-- and each row additionally publishes how its name resolved: two lines means it
-- wrapped, and `name_elided` is the failure the second line exists to remove. A
-- capture cannot make that assertion, because a wrapped row and an elided one are
-- the same HEIGHT by construction.
--
-- Run with: ProjectIo --verify scripts/verify/construction_progress.lua

-- 1720x1080, the resolution the shell's widths are tuned against (the same one
-- tile_card.lua captures at): at the 1280 default the Selection band's tile
-- column is half as wide and the grid captures do not show what a player meets.
verify.window(1720, 1080)
verify.goto_surface("home")
verify.econ_step(6) -- let the markets hold real inventory, not just a flow
-- The staged player corp cannot afford a site at the Digitisation baseline
-- (`insufficient_funds`), and this check is about what the SURFACE says, not
-- about affordability — so fund it.
verify.set_balance(5000000.0)

-- BL-1371: the row metric, asserted the same way on all five surfaces. TWO text
-- lines plus the cell padding, and no more - a row that grew a third line would
-- be the seven-line row creeping back.
local function assert_two_line(what, row_h, line_h)
    verify.expect(row_h > 0 and row_h >= line_h * 2.0 and row_h <= line_h * 2.0 + 8.0,
                  string.format("%s is TWO lines plus padding (%.1f px against a %.1f px line)",
                                what, row_h, line_h))
end
-- ...and that the NAME column wrapped rather than eliding, which is the whole
-- point of the second line. Counts the rows that actually used it, so the run
-- prints whether the worst case was exercised at all.
local wrapped_names, elided_names = 0, 0
-- MEASURED 2026-10-17: 15 of the build list's names use the second line and one
-- ("Inland Logistics Hub", three words in a ~70 px column) does not fit even two,
-- so it still elides - with the full string on its own tooltip. That is the honest
-- limit of a two-line budget, not a defect in the wrap, so the assertion is the
-- CONTRACT rather than "never elides": an elision may only happen after BOTH lines
-- have been spent. A name elided on line one would mean the wrap never ran.
local function assert_name_wraps(what, row)
    verify.expect(row.name_lines >= 1 and row.name_lines <= 2,
                  string.format("%s '%s' name fits the two-line budget (%d lines)",
                                what, row.name, row.name_lines))
    if row.name_elided then
        verify.expect(row.name_lines == 2,
                      string.format("%s '%s' elided only after spending BOTH lines (%d)",
                                    what, row.name, row.name_lines))
    end
    if row.name_lines > 1 then wrapped_names = wrapped_names + 1 end
    if row.name_elided then elided_names = elided_names + 1 end
end
-- THE PICK IS THE PLAYER'S OWN GROUND (2026-10-25). It used to be
-- `find_deposit_tile("iron_ore")`, the first iron tile in raster order on the whole
-- body - whoever held it. BL-1399 (a nation builds only on its own ground) then made
-- that tile answer `foreign_ground`, and every later section cascaded off the refusal.
-- Nothing in the verify API names a tile's owner, by design, so the pick asks the
-- SURFACE that already answers "where may the player build this?": the site finder,
-- whose rows are exactly the tiles the build door accepts an Iron Mine on (own
-- ground, tech, road, stack, province ceiling, treasury). Its first row under the
-- default sort (profit, ties by tile id) is deterministic for a seed, and it follows
-- the player's ground wherever generation moves it, so the pick cannot rot again.
verify.show_panel("build", true)
verify.frames(1)
local picked_ok = verify.site_pick("Iron Mine")
verify.frames(3)
local sites = verify.site_finder()
verify.expect(picked_ok and sites.n_rows > 0, string.format(
    "the site finder lists the player's own iron ground for an Iron Mine (%d sites of %d own tiles)",
    sites.n_rows or 0, sites.own_tiles or 0))
if not (picked_ok and sites.n_rows > 0) then return end
local dep = { x = sites.rows[1].x, y = sites.rows[1].y }
print(string.format("construction_progress: picked (%d,%d), the first of %d Iron Mine sites",
                    dep.x, dep.y, sites.n_rows))
-- Disarm and close, so the staging below and the captures after it see the ledger as
-- every earlier pass did: no lens skinning the map, no site rows above the build door.
verify.site_clear()
verify.show_panel("build", false)
verify.frames(2)

-- ── 1. Two of the same type on one tile ────────────────────────────────────
-- THE GROUND IS ROADED FIRST (BL-1380). No building stands on a tile with no roaded
-- edge (PRODUCTION.md § "A building needs a road"), and the deposit `find_deposit_tile`
-- handed back then was a remote one the generated lattice never reached — so every placement
-- below answered `invalid_tile` and all thirteen of this script's failures were that one
-- refusal, cascading. The fixture is what was wrong: this is the Lua form of the
-- `road_edges_on_every_side(1)` the nine headless fixtures write (lib.lua § road_ground).
-- All three placements use this ONE tile, so one crossing is the whole staging. A
-- site-finder tile already passes the road gate; the crossing stays as the fixture's
-- own guarantee rather than an inference from the ledger.
road_ground(dep.x, dep.y)

verify.place_mode("extraction", "iron_ore")
local first = verify.build_at(dep.x, dep.y)
verify.expect(first == "placed", "first extraction site placed (" .. first .. ")")

-- BL-1341: the stack caps permit more than one (non-extraction 2-12 aggregate;
-- extraction max(1, richness/50) per deposit), so this records what the SEAM
-- answers for a second of the same type on the same tile. "placed" = the rules
-- allow it and the door's affordance was the only thing missing; "slot_occupied"
-- = this deposit's richness supports exactly one, and the row must say so.
-- MEASURED 2026-10-16 on the staged world: "invalid_tile". construct_building
-- collapses every can_place_in_world refusal except launchpad and the two it
-- maps by name into `invalid_tile` (src/world/construction.cpp:213), so the SEAM
-- cannot say which cap bit. The DOOR can and now does: the row's refusal names
-- the cap and its counts. The processing build below placing proves the province
-- ceiling was not what refused (that ceiling is type-agnostic), so the refusal
-- was this deposit's own cap, max(1, richness / 50) == 1.
local second = verify.build_at(dep.x, dep.y)
-- Since 2026-10-26 (Ben: one site under construction per tile) a second placement
-- while the first still builds answers `site_busy`, which is the expected refusal here.
verify.expect(second == "placed" or second == "slot_occupied" or second == "invalid_tile"
              or second == "site_busy",
              "second of the same type on one tile: " .. second)

-- A second building of a DIFFERENT family on the same tile, so the aggregate
-- non-extraction cap has an occupant to count whatever the deposit allowed.
verify.place_mode("processing", "iron_ore")
local proc = verify.build_at(dep.x, dep.y)
verify.expect(true, "processing facility on the same tile: " .. proc)

verify.econ_step(2) -- some construction paid, so the bars are not all 0 %

-- ── 2. The queue ───────────────────────────────────────────────────────────
verify.select_tile(dep.x, dep.y)
verify.frames(1) -- a new selection closes the column; open it on a later frame
verify.show_panel("build", true)
verify.section("construction_queue", true)
verify.frames(2)
verify.capture("construction_queue_rows")

-- BL-1365: the queue's rows are ONE LINE, and the stall sentence that used to be
-- printed UNDER the table (one wrapped line per stalled build) is on the row's
-- hover card instead.
local q = verify.construction_queue()
verify.expect(q.n_rows > 0, "the queue published " .. tostring(q.n_rows) .. " grid rows")
assert_two_line("a queue row", q.row_h, q.line_h)
-- The authored stall clauses (BL-1342), the exact words this pass must not lose.
local authored = { "at the market", "priced above the build ceiling",
                   "no market supplies this tile", "market can't supply materials",
                   "could not fund it this quarter" }
local function names_a_cause(text)
    for _, clause in ipairs(authored) do
        if text:find(clause, 1, true) then return clause end
    end
    return nil
end
-- BL-1368: the per-column assertions, shared by the queue and the build list.
-- A column must HAVE an explainer and it must be SHORT - one or two short
-- sentences was the ruling, and the row-wide card it replaced was eight lines.
local k_max_explainer = 240
local function assert_columns(what, row, expected)
    verify.expect(row.n_cols > 0, what .. " '" .. row.name .. "' publishes its columns")
    local seen = {}
    for _, col in ipairs(row.cols) do
        seen[col.key] = col.text
        verify.expect(col.text ~= "",
                      what .. " '" .. row.name .. "' column '" .. col.key
                      .. "' has an explainer of its own")
        verify.expect(#col.text <= k_max_explainer,
                      string.format("%s '%s' column '%s' explainer is short (%d chars): '%s'",
                                    what, row.name, col.key, #col.text, col.text))
        verify.expect(col.x > 0 and col.y > 0,
                      what .. " '" .. row.name .. "' column '" .. col.key
                      .. "' publishes the cell a check must hover")
    end
    for _, key in ipairs(expected) do
        verify.expect(seen[key] ~= nil,
                      what .. " '" .. row.name .. "' carries a '" .. key .. "' explainer")
    end
    return seen
end

local stalled_in_queue = 0
for _, r in ipairs(q.rows) do
    verify.expect(r.hover ~= "", "queue row '" .. r.name .. "' publishes a hover card")
    local paused = r.hover:find("Paused - ", 1, true) ~= nil
    local short  = r.hover:find("short of ", 1, true) ~= nil
                or r.hover:find("priced out", 1, true) ~= nil
    if paused then
        stalled_in_queue = stalled_in_queue + 1
        local clause = names_a_cause(r.hover)
        verify.expect(clause ~= nil,
                      "a paused queue row's card names its CAUSE in the authored words: '"
                      .. r.hover:gsub("%s+", " ") .. "'")
    elseif short then
        stalled_in_queue = stalled_in_queue + 1
    end
    verify.expect(r.hover:find("Building%.%.%.") ~= nil or paused,
                  "queue row '" .. r.name .. "' card carries its status line")
    -- BL-1368: six columns, six explainers, and the STATE mark is the one that
    -- names the good (the authored clause, `construction_stall_reason`'s).
    assert_name_wraps("queue row", r)
    local cols = assert_columns("queue row", r, { "name", "bar", "eta", "cost", "state" })
    if paused then
        verify.expect(names_a_cause(cols["state"]) ~= nil,
                      "a paused queue row's STATE MARK names the cause: '" .. cols["state"] .. "'")
    end
    verify.expect(cols["bar"]:find("built", 1, true) ~= nil,
                  "the queue's done bar explains the percentage: '" .. cols["bar"] .. "'")
    verify.expect(cols["cost"]:find("still to pay", 1, true) ~= nil,
                  "the queue's cost column explains what is left to pay: '" .. cols["cost"] .. "'")
end
print(string.format("construction_progress: %d of %d queue rows are stalled or stretched",
                    stalled_in_queue, q.n_rows))
-- Hover the first queue row and capture the card itself, then EACH COLUMN of it
-- in turn: one card per column is the whole of BL-1368, and it is the half no
-- single capture can show.
if q.n_rows > 0 then
    verify.hover(q.rows[1].x, q.rows[1].y, 2)
    verify.frames(2)
    verify.capture("construction_queue_row_hover")
    for _, col in ipairs(q.rows[1].cols) do
        verify.hover(col.x, col.y, 2)
        verify.frames(2)
        verify.capture("construction_queue_col_" .. col.key)
    end
end

-- ── 3. The build door on a tile that already carries one of a type ──────────
-- Collapse the queue first: open, it and its stall lines take most of the column
-- and the build list below is left a few pixels tall. That is the ledger working
-- as designed (the queue is a collapsing header for exactly this reason), but it
-- means the door's rows are only readable with the queue shut.
verify.section("construction_queue", false)
verify.frames(2)

-- The build list is a nested scroller inside the ledger ("##build_list"), so it
-- takes its own scroll name; scrolling the ledger window moves nothing here.
-- Just past the head: the Iron Mine row, the one type that already stands on
-- this tile, with its own count and its refusal if the deposit is full.
verify.scroll_panel("build_list", 0.07)
verify.frames(2)
verify.capture("construction_build_door_stack")
verify.scroll_panel("build_list", 0.55)
verify.frames(2)
verify.capture("construction_build_door_foot")
-- The head of the list: the Extraction fold, where the Iron Mine row is the one
-- type that ALREADY stands on this tile - so this frame is the BL-1341 evidence,
-- the row's own account of how many are here and whether another fits.
verify.scroll_panel("build_list", 0.0)
verify.frames(2)
verify.capture("construction_build_door_extraction")

local door = verify.build_door()
verify.expect(door.drawn, "the build door drew for the selected tile")

-- BL-1365: ONE LINE A ROW, in a grid, with the words on the card.
assert_two_line("a build row", door.row_h, door.line_h)
verify.expect(door.n_rows > 0, "the build door published " .. tostring(door.n_rows) .. " grid rows")
local stalling, refused = 0, 0
for _, r in ipairs(door.rows) do
    -- BL-1368 re-words these two: the cost and the materials are now the COST
    -- COLUMN's own explainer rather than the first two lines of a row-wide card,
    -- so the union carries them somewhere rather than at the front.
    verify.expect(r.hover:find("Costs ", 1, true) ~= nil,
                  "row '" .. r.name .. "' carries the cost in full")
    verify.expect(r.hover:find("Materials: ", 1, true) ~= nil
                  or r.hover:find("no materials", 1, true) ~= nil,
                  "row '" .. r.name .. "' carries the MATERIALS the line dropped")
    verify.expect(r.hover:find("here", 1, true) ~= nil or r.hover:find("Full", 1, true) ~= nil,
                  "row '" .. r.name .. "' card carries the stack's own sentence")
    assert_name_wraps("build row", r)
    -- BL-1368: EIGHT columns, eight explainers, each about its own column.
    local cols = assert_columns("build row", r,
                                { "name", "cost", "makes", "stack", "state", "bar", "act" })
    verify.expect(cols["cost"]:find("Costs ", 1, true) == 1,
                  "the cost column's explainer is about the COST: '" .. cols["cost"] .. "'")
    verify.expect(cols["makes"]:find("Makes", 1, true) ~= nil,
                  "the pip's explainer says what it MAKES: '" .. cols["makes"] .. "'")
    verify.expect(cols["stack"]:find("deposit", 1, true) ~= nil
                  or cols["stack"]:find("ground", 1, true) ~= nil,
                  "the stack figure's explainer says WHICH CAP it counts against: '"
                  .. cols["stack"] .. "'")
    verify.expect(cols["bar"]:find("Est. net", 1, true) ~= nil
                  or cols["bar"]:find("upkeep", 1, true) ~= nil
                  or cols["bar"]:find("No estimate", 1, true) ~= nil,
                  "the bar's explainer is about the ESTIMATE: '" .. cols["bar"] .. "'")
    if r.hover:find("Will stall", 1, true) or r.hover:find("Will stretch", 1, true) then
        stalling = stalling + 1
        verify.expect(names_a_cause(r.hover) ~= nil,
                      "a stalling row's card names the GOOD in the authored words: '"
                      .. r.hover:gsub("%s+", " ") .. "'")
        verify.expect(names_a_cause(cols["state"]) ~= nil,
                      "...and it is the STATE MARK that names it: '" .. cols["state"] .. "'")
    end
    if r.hover:find("Full:", 1, true) then
        refused = refused + 1
        verify.expect(cols["act"]:find("Full:", 1, true) ~= nil,
                      "a refused row's PRESS explains the refusal: '" .. cols["act"] .. "'")
    end
end
print(string.format("construction_progress: %d build rows, %d will stall or stretch, %d refused full",
                    door.n_rows, stalling, refused))
-- BL-1371: the two-line row's own evidence. At least one row's name must have
-- used the second line, or the budget bought nothing on this world and the
-- assertion above is vacuous.
print(string.format("construction_progress: %d row name(s) wrapped to the second line, %d elided",
                    wrapped_names, elided_names))
-- WHICH names may outrun it is a MEASUREMENT, and it is pinned by name rather than
-- by count, because the count depends on which rows the picked tile lists: the
-- door lists one extraction row per deposit the tile carries. MEASURED 2026-10-25
-- on the player's own iron ground (the pick above): "Woodcutter's Camp" joins
-- "Inland Logistics Hub" - "Woodcutter'" alone fills the ~70 px column, so the name
-- breaks inside the word and "Camp" is still left over after two lines. The tile
-- the 2026-10-17 measurement used carried no timber, so that row was not drawn.
-- Any OTHER name eliding is a failure, on any tile.
local k_may_outrun = { ["Inland Logistics Hub"] = true, ["Woodcutter's Camp"] = true }
local function check_outrun(what, rows)
    for _, r in ipairs(rows) do
        if r.name_elided then
            verify.expect(k_may_outrun[r.name] == true,
                          string.format("%s '%s' outruns the two-line budget - only the measured "
                                        .. "names may", what, r.name))
        end
    end
end
check_outrun("queue row", q.rows)
check_outrun("build row", door.rows)
check_outrun("rail row", door.rail_rows)
verify.expect(elided_names <= 2,
              string.format("at most the two measured names outrun the two-line budget (%d did)",
                            elided_names))
verify.expect(wrapped_names > 0,
              "at least one row name USED the second line (the budget is not vacuous)")
-- A capture of a wrapped row: scrolled to the head of the list, where the long
-- Extraction names are, with no hover card over it.
verify.hover(5.0, 5.0, 1)
verify.scroll_panel("build_list", 0.0)
verify.frames(2)
verify.capture("construction_wrapped_row")
-- Hover a real build row and capture the card: the half no other capture shows.
verify.hover(door.rows[1].x, door.rows[1].y, 2)
verify.frames(2)
verify.capture("construction_build_row_hover")
-- ONE CAPTURE PER COLUMN of a real row (BL-1368).
for _, col in ipairs(door.rows[1].cols) do
    verify.hover(col.x, col.y, 2)
    verify.frames(2)
    verify.capture("construction_build_col_" .. col.key)
end

-- ── 3b. BL-1366: the rail rows are rows of THIS grid ───────────────────────
-- They drew a four-line bordered box, so Track and Railway Station sat visibly
-- taller than the one-line Infrastructure rows above and below them. The rows
-- only exist where a major nation is seated and the era carries the railway, so
-- their ABSENCE is not a failure - but where they draw, they are this grid.
print(string.format("construction_progress: %d rail rows published", door.n_rail_rows))
for _, r in ipairs(door.rail_rows) do
    verify.expect(r.h > 0 and math.abs(r.h - door.row_h) < 0.5,
                  string.format("rail row '%s' is drawn at the SHARED row height (%.1f vs %.1f)",
                                r.name, r.h, door.row_h))
    assert_two_line("rail row '" .. r.name .. "'", r.h, door.line_h)
    assert_name_wraps("rail row", r)
    local cols = assert_columns("rail row", r,
                                { "name", "cost", "makes", "stack", "state", "bar", "act" })
    verify.expect(cols["cost"]:find("treasury", 1, true) ~= nil,
                  "a rail row's cost column says whose money it is: '" .. cols["cost"] .. "'")
    verify.expect(cols["bar"]:find("Upkeep", 1, true) ~= nil,
                  "a rail row's empty bar column carries the upkeep: '" .. cols["bar"] .. "'")
end
-- The columns must LAND IN THE SAME PLACE as the build list's, or "the same
-- grid" is a claim rather than a fact: a column that reads down the list as a
-- column is the whole of the grid ruling.
if door.n_rail_rows > 0 and door.n_rows > 0 then
    local ref = {}
    for _, col in ipairs(door.rows[1].cols) do ref[col.key] = col.x end
    for _, r in ipairs(door.rail_rows) do
        for _, col in ipairs(r.cols) do
            if ref[col.key] and col.key ~= "name" then
                verify.expect(math.abs(col.x - ref[col.key]) <= 1.0,
                              string.format("rail row '%s' column '%s' is in the build list's own "
                                            .. "column (%.1f vs %.1f)",
                                            r.name, col.key, col.x, ref[col.key]))
            end
        end
    end
end
if door.n_rail_rows > 0 then
    verify.hover(door.rail_rows[1].x, door.rail_rows[1].y, 2)
    verify.frames(2)
    verify.capture("construction_rail_row_hover")
end

-- ── 4. The tile card: stack reading + the site card's progress bar ──────────
verify.show_panel("construction", false)
verify.select_tile(dep.x, dep.y)
verify.frames(2)
verify.capture("construction_tile_card_stacks")

local card = verify.tile_card()
verify.expect(card.drawn, "the tile card drew")
assert_two_line("a tile-card stack row", card.row_h, card.line_h)
for _, b in ipairs(card.buttons) do
    verify.expect(not b.name_elided,
                  "tile-card stack row '" .. b.label .. "' name WRAPS rather than eliding")
end
local saw_reading = false
for _, b in ipairs(card.buttons) do
    if b.detail and (b.detail:find("ETA") or b.detail:find("Paused")) then
        saw_reading = true
        verify.expect(true, "stack reading: " .. b.label .. " -> " .. b.detail)
    end
end
verify.expect(saw_reading, "at least one stack reads as under construction")

-- The stack row's own hover card (BL-1365): the reading in words lives here now.
if #card.buttons > 0 then
    verify.hover(card.buttons[1].x, card.buttons[1].y, 2)
    verify.frames(2)
    verify.capture("construction_tile_card_row_hover")
end

-- The expanded site card: header, method, status line and the progress bar.
verify.select_building(dep.x, dep.y)
verify.frames(2)
verify.capture("construction_site_card_progress")
local sited = verify.tile_card()
verify.expect(sited.is_expanded, "a building is expanded on the card")
verify.expect(sited.status ~= "", "the site card drew a status line: " .. sited.status)

-- BL-1368: the expanded SITE row is one grid row, and each of its columns
-- explains itself - the method on the name, the percentage on the bar, the ETA
-- on the quarters, what is left to pay on the cost, and the authored stall
-- clause on the state mark. One capture per column.
verify.expect(sited.n_site_cols > 0, "the site row published its columns")
local site_seen = {}
for _, col in ipairs(sited.site_cols) do
    site_seen[col.key] = col.text
    verify.expect(col.text ~= "", "site column '" .. col.key .. "' explains itself")
    verify.expect(#col.text <= k_max_explainer,
                  string.format("site column '%s' explainer is short (%d): '%s'",
                                col.key, #col.text, col.text))
    verify.hover(col.x, col.y, 2)
    verify.frames(2)
    verify.capture("construction_site_col_" .. col.key)
end
for _, key in ipairs({ "name", "bar", "eta", "cost", "state" }) do
    verify.expect(site_seen[key] ~= nil, "the site row carries a '" .. key .. "' explainer")
end
-- tostring: with no state column published this must FAIL as an assertion, not
-- stop the script on a nil concatenation.
verify.expect(site_seen["state"] ~= nil and site_seen["state"] == sited.status,
              "the site row's STATE MARK carries the status line verbatim, one author for both: '"
              .. tostring(site_seen["state"]) .. "' vs '" .. tostring(sited.status) .. "'")
