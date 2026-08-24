-- BL-598 (province folds into the tile accordion) — the ONE accordion, its five
-- sections and their order.
--
-- WHY. Two surfaces answered questions about one piece of ground. The tile
-- Selection element's centre column was a PAGER — "Terrain (1/3)" with prev/next
-- arrows, three views — and the PROVINCE was a second element with a pager of
-- its own (Tiles / Deposits / Buildings). A pager hides the existence of every
-- view but one behind an arrow press, and a second element made the player
-- choose a grain before knowing what they wanted to know.
--
-- Ben's ruling (2026-08-24): "Province selection element must be bundled into
-- the tile selection element. By this I mean, we just use a longer accordion. We
-- can also swap the order. Buildings -> Deposits -> Resources -> Population ->
-- Terrain. Keep the tile available buildings tab, and drop the province
-- buildings tab."
--
-- The claims:
--   S1  ONE element. Selecting bare ground gives the tile card, and the province
--       is three of its sections — there is no province card to reach.
--   S2  FIVE sections, in Ben's order, all five visible at once as headers.
--       The default open section is Buildings, which is the acting end of the
--       order; the pager's default was Terrain, the other end.
--   S3  The section headers are TOGGLES (standing Toggle rule): pressing a
--       closed one opens it and closes the rest; pressing the OPEN one closes it,
--       leaving every section shut.
--   S4  Deposits and Population are PROVINCE readings reached from a tile —
--       the fold, doing the thing it was for.
verify.window(1280, 720)

verify.goto_surface("home")
verify.set_overlay("none")
verify.frames(2)

-- A tile with deposits, so the Resources section has more than one entry to
-- offer and the Deposits section has something to sum. (40,30) is the tile
-- province_render.lua frames and is confirmed workable, partitioned land.
verify.center_tile(40, 30, 6)
verify.select_tile(40, 30)
verify.frames(2)

-- ---------------------------------------------------------------------------
-- S1 / S2 — the resting accordion. Five headers stacked in Ben's order, with
-- Buildings open by default.
-- ---------------------------------------------------------------------------
verify.capture("accordion_00_default_buildings")

-- ---------------------------------------------------------------------------
-- S3 / S4 — walk the sections.
-- ---------------------------------------------------------------------------
-- The header presses are MEASURED PIXELS off probe captures at 1280x720, not
-- derived. Deriving them needs the comms-dock width, the band's three-column
-- split and the collapsing-header pitch, and getting any of the three wrong aims
-- the press at nothing and silently does something else. Ben accepted that cost
-- on NR-508 rather than build a target registry, so this is the accepted shape:
-- pixels, named, measured, with the invariant that makes them safe written down.
--
-- THE INVARIANT. A header's y depends ONLY on how many sections ABOVE it are
-- open, and at most one section is ever open. So with every section CLOSED the
-- five headers sit on a uniform ~23 px pitch (the accordion halves the vertical
-- frame padding on the header rows: five default-padded headers would cost 60%
-- of this column). Section i's header is at that same all-closed y whenever the
-- open section is i itself or one below it.
-- Opening b from a is therefore two presses at fixed coordinates: close a (which
-- restores the uniform grid), then press b.
local HDR_X   = 520      -- inside the centre column, clear of the arrow glyph
local HDR_Y0  = 512      -- the Buildings header when nothing above it is open
local HDR_PITCH = 23.25

local function press(i)
    local y = HDR_Y0 + HDR_PITCH * i
    verify.hover(HDR_X, y, 2)
    verify.click(HDR_X, y)
    verify.frames(2)
end

local open_section = 0   -- Buildings is the default open section

--- Close whatever is open, then open section `i`.
local function open_only(i)
    if open_section >= 0 then press(open_section) end
    press(i)
    open_section = i
end

-- Deposits: the province's stock, summed across its member tiles. A PROVINCE
-- reading, reached from a TILE selection - this is the fold, doing its job.
open_only(1)
verify.capture("accordion_01_deposits")

-- Resources: this tile's per-deposit yield against the top decile, chosen
-- through the dropdown rather than a carousel.
open_only(2)
verify.capture("accordion_02_resources")

-- Population: the province's population centres. The reading that had no home on
-- either of the two surfaces this one replaces.
open_only(3)
verify.capture("accordion_03_population")

-- Terrain: habitability and hazard against the body average. LAST, where the
-- pager had it FIRST - the order runs from what can be acted on to what the
-- ground merely is.
open_only(4)
verify.capture("accordion_04_terrain")

-- The Toggle rule: pressing the section that is already OPEN closes it, leaving
-- five closed headers and no body. The exempt controls are the cross-cutting
-- selectors (the resource dropdown) and the Selection element itself; a section
-- header is neither - it shows its own open state, so it is a toggle.
press(4)
open_section = -1
verify.capture("accordion_05_all_closed")

-- ...and back to the acting end of the order.
open_only(0)
verify.capture("accordion_06_back_to_buildings")

-- ---------------------------------------------------------------------------
-- S4 again, on INHABITED ground. (40,30)'s province holds no population centre,
-- so the Population section there correctly reads "Uninhabited." — which proves
-- the empty case and nothing else. A built tile sits near settlement, so this
-- captures the rows themselves: name, scale word, headcount, habitability.
-- ---------------------------------------------------------------------------
-- Ask the world where a centre IS rather than guessing at a built tile: a
-- building sits near INDUSTRY, which need not be the settlement's province, and
-- the first tile tried read "Uninhabited." too. Pick the largest centre there is,
-- so the rows carry a real scale word and headcount.
local best = nil
for _, c in ipairs(verify.population_centres()) do
    if best == nil or c.population > best.population then best = c end
end
verify.expect(best ~= nil, "S4a the world generated at least one population centre")
if best then
    print(string.format("S4 centre at (%d,%d) scale=%d pop=%dk",
                        best.x, best.y, best.scale, best.population))
    verify.center_tile(best.x, best.y, 6)
    verify.select_tile(best.x, best.y)
    verify.frames(2)
    open_only(3)
    verify.capture("accordion_07_population_inhabited")
end

-- Free, and the one assertion this pass can make without a target registry:
-- every string drawn above either fitted its container or was sanctioned as
-- elided. A five-section accordion is exactly where a label would start to clip.
verify.expect_no_clipping("selection_accordion")
