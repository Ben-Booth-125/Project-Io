-- BL-534 (tile selection becomes an accordion) — the three views and the
-- available-buildings table.
--
-- WHY. The Selection band's centre column used to page ONE-PER-DEPOSITED-
-- RESOURCE: "Iron Ore (1/7)" with prev/next arrows, so reading the seventh
-- deposit cost six presses, and anything that was not a resource graph had
-- nowhere to live. Ben's ruling (2026-08-22): page VIEWS instead, bundle the
-- resource choice into a dropdown, and add an available-buildings view.
--
-- The three views are Terrain, Resources, Available buildings. Ben named that
-- set and did NOT pick Province or Ownership, which were offered.
--
-- The claims:
--   S1  the accordion pages three VIEWS, not N resources
--   S2  the Resources view offers a dropdown, not a carousel
--   S3  Available buildings reads PER PROVINCE — built vs max per resource,
--       over the province's own BL-513 total ceiling
verify.window(1280, 720)

verify.goto_surface("home")
verify.set_overlay("none")
verify.frames(2)

-- A tile with deposits, so the Resources view has more than one entry to offer.
-- (40,30) is the tile province_render.lua frames and is confirmed workable land.
verify.center_tile(40, 30, 6)
verify.select_tile(40, 30)
verify.frames(2)

-- ---------------------------------------------------------------------------
-- S1 / default — view 0 is Terrain. The header reads "Terrain  (1/3)".
-- ---------------------------------------------------------------------------
verify.capture("accordion_view_terrain")

-- ---------------------------------------------------------------------------
-- S2 — view 1 is Resources, with the dropdown where the carousel used to be.
-- ---------------------------------------------------------------------------
-- The pager's next arrow sits at the right edge of the centre column. The band
-- is `minimap_height + chrome_margin` tall and flush to the screen bottom; the
-- centre column is the middle half of the span between the comms dock and the
-- right chrome. Derived rather than hardcoded so a window change does not
-- silently re-aim this at nothing (NR-508 — panels publish no rect to ask).
-- MEASURED off accordion_view_terrain.png, not derived. Deriving it needs the
-- comms-dock width, the band split and the pager row offset, and getting any of
-- the three wrong aims the press at nothing and silently does something else.
-- Ben accepted that cost on NR-508 rather than build a target registry, so this
-- is the accepted shape: pixels, named, from a capture that is in the repo.
local prev_x, next_x, pager_y = 491, 711, 515

verify.hover(next_x, pager_y, 2)
verify.click(next_x, pager_y)
verify.frames(2)
verify.capture("accordion_view_resources")

-- ---------------------------------------------------------------------------
-- S3 — view 2 is Available buildings: the province table.
-- ---------------------------------------------------------------------------
-- Expect a "Province capacity  N / M (K free)" line over a three-column table
-- of Resource / Built / Max. Every number is a PROVINCE number, per Ben's
-- ruling on the grain — the selected tile only names which province is meant.
verify.hover(next_x, pager_y, 2)
verify.click(next_x, pager_y)
verify.frames(2)
verify.capture("accordion_view_buildings")

-- Paging back returns to Terrain, so the pager is symmetric.
verify.hover(prev_x, pager_y, 2)
verify.click(prev_x, pager_y)
verify.frames(2)
verify.capture("accordion_back_to_resources")
