-- BL-601 — the national border band is a thing you can click, and clicking it
-- selects the NATION.
--
-- Ben, 2026-08-24: "National borders should not diffuse together, instead they
-- should borders extending their colour inwards. With this, we can drop the nation
-- lens" — and, on what replaces the lens's route to a nation's ledger: "click the
-- border itself." So the band is not decoration: it is the only remaining way to
-- reach a nation, and this check is what proves the route exists.
--
-- WHY IT DOES NOT HARD-CODE A PRESS POINT. It did, once, at (563, 301) — measured
-- by eye in one worktree — and it passed there and FAILED the moment the slice was
-- merged with its three siblings, because a constant measured against one build is
-- a constant measured against that build's world. That is the same rot that had
-- `built_tile_render.lua` framing open ocean and `pop_markers.lua` framing empty
-- terrain. So the corridor is SEARCHED for, along a scan line, and the search is
-- itself the first assertion: if the band produces no clickable corridor anywhere
-- on a framed frontier, there is no route to a nation and the item has not landed.
--
-- The search cannot use "the province mirror cleared" as its test, because a
-- MARKER press clears it too — it is the mutual exclusion every entity selection
-- takes. It keys on `selection_kind` instead, which is the word the Selection
-- element itself renders.
--
-- Run: ProjectIo --verify scripts/verify/border_band.lua

verify.window(1280, 720)

local s = stage_ui_fixture()

-- Frame a frontier at a zoom where the corridor is wide enough to aim at. The
-- corridor is capped at `draw_r * 0.18`, so it scales with the hex — at coarse
-- zoom it is deliberately a couple of pixels and this check would be testing the
-- LOD collapse rather than the route.
verify.center_tile(s.unit.col, s.unit.row, 5.0)
verify.frames(4)

verify.clear_selection()
verify.frames(2)
verify.expect(not verify.pointer_target().has_selection, "cleared selection before the press")

-- ---------------------------------------------------------------------------
-- Find the corridor, and assert that one exists.
-- ---------------------------------------------------------------------------
-- A horizontal sweep across the canvas at three heights, so a frame whose
-- frontier happens to run flat through one scan line is not the only chance.
local CANVAS_L, CANVAS_R = 70, 920
local hit_x, hit_y, hit_nation = nil, nil, nil

for _, y in ipairs({ 220, 300, 380 }) do
    for x = CANVAS_L, CANVAS_R, 6 do
        verify.click(x, y)
        verify.frames(2)
        local t = verify.pointer_target()
        if t.selection_kind == "nation" then
            hit_x, hit_y, hit_nation = x, y, t
            break
        end
        verify.clear_selection()
    end
    if hit_x then break end
end

verify.expect(hit_x ~= nil,
              "the border band produces a clickable corridor on a framed frontier")

if hit_x then
    -- The route's own claim: what the press selected is a NATION, and the
    -- province mirror cleared with it (the mutual exclusion every entity
    -- selection takes — BL-598 made `selected_province` a derived mirror, so a
    -- structure selection must leave it at 0 rather than at the ground's value).
    verify.expect(hit_nation.selection_kind == "nation",
                  "a press in the corridor selects a nation, not the ground under it")
    verify.expect(hit_nation.selected_province == 0,
                  "a structure selection clears the province mirror")

    verify.click(hit_x, hit_y)
    verify.frames(3)
    verify.capture("border_02_nation_selected")

    -- The hover read, which is the half of the ruling that makes the press
    -- predictable: the band must NAME the nation before the click commits, and
    -- at a dwell far shorter than the hover card's own appear delay.
    verify.clear_selection()
    verify.hover(hit_x, hit_y, 4)
    verify.capture("border_01_hover_label")
end

-- ---------------------------------------------------------------------------
-- The control: the centre of a hex is still the hex.
-- ---------------------------------------------------------------------------
-- The corridor rings most of a frontier hex's rim, so the real risk is that it
-- swallows the tile. This is the assertion that catches that.
verify.clear_selection()
verify.frames(2)
local tp = verify.tile_screen(s.unit.col, s.unit.row)
verify.expect(tp.ok, "the fixture's anchor tile is on screen")
if tp.ok then
    verify.click(tp.x, tp.y)
    verify.frames(3)
    local t = verify.pointer_target()
    verify.expect(t.selection_kind ~= "nation",
                  "a press at a hex centre is not swallowed by the border corridor")
    verify.capture("border_03_control_tile_centre")
end

-- Coarse zoom: the band collapses to a plain political outline with untinted
-- ground inside it, which is the read at the whole-body view.
verify.center_tile(s.unit.col, s.unit.row, 1.5)
verify.frames(4)
verify.capture("border_04_coarse_outline")
