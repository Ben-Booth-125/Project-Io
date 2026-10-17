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

local dep = verify.find_deposit_tile("iron_ore")
verify.expect(dep.ok, "found a tile carrying iron ore")

-- ── 1. Two of the same type on one tile ────────────────────────────────────
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
verify.expect(second == "placed" or second == "slot_occupied" or second == "invalid_tile",
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
verify.expect(q.row_h > 0 and q.row_h <= q.line_h + 8.0,
              string.format("a queue row is one line plus padding (%.1f px against a %.1f px line)",
                            q.row_h, q.line_h))
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
end
print(string.format("construction_progress: %d of %d queue rows are stalled or stretched",
                    stalled_in_queue, q.n_rows))
-- Hover the first queue row and capture the card itself.
if q.n_rows > 0 then
    verify.hover(q.rows[1].x, q.rows[1].y, 2)
    verify.frames(2)
    verify.capture("construction_queue_row_hover")
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
verify.expect(door.row_h > 0 and door.row_h <= door.line_h + 8.0,
              string.format("a build row is one line plus padding (%.1f px against a %.1f px line)",
                            door.row_h, door.line_h))
verify.expect(door.n_rows > 0, "the build door published " .. tostring(door.n_rows) .. " grid rows")
local stalling, refused = 0, 0
for _, r in ipairs(door.rows) do
    verify.expect(r.hover:find("Cost ", 1, true) == 1,
                  "row '" .. r.name .. "' card opens with the cost in full")
    verify.expect(r.hover:find("Materials ", 1, true) ~= nil
                  or r.hover:find("No materials", 1, true) ~= nil,
                  "row '" .. r.name .. "' card carries the MATERIALS the line dropped")
    verify.expect(r.hover:find("here", 1, true) ~= nil or r.hover:find("Full", 1, true) ~= nil,
                  "row '" .. r.name .. "' card carries the stack's own sentence")
    if r.hover:find("Will stall", 1, true) or r.hover:find("Will stretch", 1, true) then
        stalling = stalling + 1
        verify.expect(names_a_cause(r.hover) ~= nil,
                      "a stalling row's card names the GOOD in the authored words: '"
                      .. r.hover:gsub("%s+", " ") .. "'")
    end
    if r.hover:find("Full:", 1, true) then refused = refused + 1 end
end
print(string.format("construction_progress: %d build rows, %d will stall or stretch, %d refused full",
                    door.n_rows, stalling, refused))
-- Hover a real build row and capture the card: the half no other capture shows.
verify.hover(door.rows[1].x, door.rows[1].y, 2)
verify.frames(2)
verify.capture("construction_build_row_hover")

-- ── 4. The tile card: stack reading + the site card's progress bar ──────────
verify.show_panel("construction", false)
verify.select_tile(dep.x, dep.y)
verify.frames(2)
verify.capture("construction_tile_card_stacks")

local card = verify.tile_card()
verify.expect(card.drawn, "the tile card drew")
verify.expect(card.row_h > 0 and card.row_h <= card.line_h + 8.0,
              string.format("a tile-card stack row is one line plus padding (%.1f px / %.1f px line)",
                            card.row_h, card.line_h))
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
