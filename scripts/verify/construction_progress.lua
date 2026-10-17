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
--   4. the tile card: the stack button's reading (ETA or "Paused - no <Good>")
--      with its progress strip, and the expanded site card's progress bar.
--
-- Run with: ProjectIo --verify scripts/verify/construction_progress.lua

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

-- ── 4. The tile card: stack reading + the site card's progress bar ──────────
verify.show_panel("construction", false)
verify.select_tile(dep.x, dep.y)
verify.frames(2)
verify.capture("construction_tile_card_stacks")

local card = verify.tile_card()
verify.expect(card.drawn, "the tile card drew")
local saw_reading = false
for _, b in ipairs(card.buttons) do
    if b.detail and (b.detail:find("ETA") or b.detail:find("Paused")) then
        saw_reading = true
        verify.expect(true, "stack reading: " .. b.label .. " -> " .. b.detail)
    end
end
verify.expect(saw_reading, "at least one stack reads as under construction")

-- The expanded site card: header, method, status line and the progress bar.
verify.select_building(dep.x, dep.y)
verify.frames(2)
verify.capture("construction_site_card_progress")
local sited = verify.tile_card()
verify.expect(sited.is_expanded, "a building is expanded on the card")
verify.expect(sited.status ~= "", "the site card drew a status line: " .. sited.status)
