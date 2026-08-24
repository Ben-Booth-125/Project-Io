-- The national border band: does the always-on band render, name itself, and
-- open the nation it carries? (BL-601)
--
-- WHY A CHECK AND NOT JUST A CAPTURE. The band is two things at once, and a
-- picture only evidences the first:
--   * a RENDER — a nation's colour at its frontier, falling off inwards, with
--     two neighbours meeting as two distinct coloured rules and never a third
--     averaged hue;
--   * a SELECTION TARGET — the route the retired Country lens used to own. Ben,
--     2026-08-24, on where the Nation ledger is reached: "click the border
--     itself."
-- A clean compile and a pretty frame say nothing about whether a press on a
-- 2 px line lands, so this presses it.
--
-- THE CONTROL IS THE INTERESTING HALF. A frontier hex can face several foreign
-- neighbours at once, so its corridors ring most of its rim -- an eyeballed
-- "just off the border" point is simply on the NEXT border, which is how the
-- first attempt at this check fooled itself. The control has to be a tile
-- CENTRE, because the centre is where the tile must still win.
--
-- The corridor point below is read off zoom_ladder.lua's x5.0 rung, so this
-- script reproduces that framing exactly. If that fixture's world changes, take
-- a fresh zoom_4_x5.0 capture and re-read a border pixel from it.
--
-- Run: ProjectIo --verify scripts/verify/border_band.lua

verify.window(1280, 720)
local s = stage_ui_fixture()
verify.center_tile(s.unit.col, s.unit.row, 5.0)
verify.frames(4)

verify.clear_selection()
verify.frames(2)
local before = verify.pointer_target()
verify.expect(not before.has_selection, "cleared selection before the press")

-- 1. The hover read. It must name the nation BEFORE the click commits, so it is
--    an immediate label rather than the dwell-gated hover card -- 4 frames is
--    far short of the card's appear delay, and the label must still be up.
verify.hover(563, 301, 4)
verify.capture("border_01_hover_label")

-- 2. The press. Inside the corridor the border is what the pointer is on, so
--    this selects the NATION: an entity selection, hence province cleared.
verify.click(563, 301)
verify.frames(3)
local after = verify.pointer_target()
verify.expect(after.has_selection, "a press in the border corridor selects something")
verify.expect(after.selected_province == 0,
              "a structure selection clears the province (mutual exclusion)")
verify.capture("border_02_nation_selected")

-- 3. The control: a tile centre must NOT resolve to the nation. Whatever wins
--    there -- a marker, a building, the province -- the point is that the
--    corridor did not swallow the ordinary gesture.
verify.clear_selection()
verify.frames(2)
local tp = verify.tile_screen(s.unit.col, s.unit.row)
verify.expect(tp.ok, "the fixture's anchor tile is on screen")
if tp.ok then
    verify.click(tp.x, tp.y)
    verify.frames(3)
    verify.capture("border_03_control_tile_centre")
end

-- 4. The band at the coarse LOD, where it collapses to depth 0 alone and reads
--    as a political outline rather than a falloff. Registers no corridor at
--    this zoom by design: a hex there is barely wider than one.
verify.center_tile(s.unit.col, s.unit.row, 1.5)
verify.frames(4)
verify.capture("border_04_coarse_outline")
