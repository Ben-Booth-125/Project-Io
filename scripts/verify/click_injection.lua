-- Verification for BL-521 — click injection in the verify API.
--
-- What this check is FOR. Until now the verify API could only WRITE ui_state: a
-- script could stage a selection but never PRESS anything, so every requirement
-- phrased "click X and see Y" arrived with its live half owed to a human by
-- construction (NR-416 and NR-424, both raised on BL-511 alone). This script is
-- the standing proof that the press now happens for real — through ImGui's own
-- event queue, the canvas's own hit-test, and the canvas's own click handler.
--
-- It is an ACCEPTANCE script (DEVELOPMENT_PRACTICES.md § Acceptance flows): the
-- verdict is verify.expect, and the captures are evidence, not goldens.
--
-- Determinism: every step below is measured in FRAMES. Nothing here sleeps, and
-- the one clock ImGui itself would consult — the double-click window — is forced
-- per press by app::inject_pointer, so C4 cannot flake on a slow Debug frame.
--
-- Run with: ProjectIo --verify scripts/verify/click_injection.lua

verify.goto_surface("home")
verify.frames(2)
verify.capture("click_injection_00_warmup")

-- The subject tile. (40,30) is the tile BL-511's own check frames, so a province
-- is expected here; if world generation ever moves it, C1 fails loudly rather
-- than the script silently testing nothing.
local COL, ROW = 40, 30

verify.set_zoom(6.0)
verify.clear_selection()

-- C1 — a synthesised click SELECTS. click_tile centres the tile (asking the
-- canvas where it is, rather than re-deriving the transform here) and presses
-- there. A plain-ground click lands on the tile's PROVINCE, per BL-511.
local landed = verify.click_tile(COL, ROW)
verify.expect(landed, "C1a click_tile resolved a screen point and pressed")

local t = verify.pointer_target()
print(string.format("C1 after click: province=%d has_entity=%s at (%.0f,%.0f)",
                    t.selected_province, tostring(t.has_selection), t.x, t.y))
verify.expect(t.selected_province ~= 0,
              "C1b the injected click selected a province through the real handler")
verify.capture("click_injection_01_clicked_province")

local pid_click = t.selected_province

-- C2 — the click AGREES with the shortcut it replaces. verify.select_province
-- writes the state a click is supposed to produce; if the two disagree, one of
-- them is lying about the gesture, and until now nothing could tell which.
verify.clear_selection()
local pid_direct = verify.select_province(COL, ROW)
verify.expect(pid_click == pid_direct,
              string.format("C2 injected click and select_province agree (%d vs %d)",
                            pid_click, pid_direct))

-- C3 — a SECOND press on the same tile cycles the rung (BL-511's UNIT >
-- PROVINCE > BUILDING > TILE walk). This is the real evidence that press number
-- two is a distinct press and not a repaint: a fresh click would re-select the
-- province, a repeat click steps past it to the bare tile.
local p = verify.tile_screen(COL, ROW)
verify.expect(p.ok, "C3a tile_screen resolved the tile's screen centre")
verify.click(p.x, p.y)
t = verify.pointer_target()
print(string.format("C3 after repeat click: province=%d has_entity=%s",
                    t.selected_province, tostring(t.has_selection)))
verify.expect(t.selected_province == 0 and t.has_selection,
              "C3b the repeat click advanced the selection cycle off the province")
verify.capture("click_injection_02_cycle_advanced")

-- C4 — double_click delivers TWO presses, not one. Only province and tile are
-- live rungs on plain ground, so two advances return to the rung we are already
-- on: after this the selection must still be the bare tile. One press would have
-- landed on the province, so this distinguishes the two.
verify.double_click(p.x, p.y)
t = verify.pointer_target()
print(string.format("C4 after double click: province=%d has_entity=%s",
                    t.selected_province, tostring(t.has_selection)))
verify.expect(t.selected_province == 0 and t.has_selection,
              "C4 double_click pressed twice (two rung advances, not one)")
verify.capture("click_injection_03_double_click")

-- C5 — HOVER through the same seam. The pointer is placed and then DWELLS by
-- frame count: the glance card appears at 30 frames and sticks at 150 (the fixed
-- 1/60 s verify clock, hover_freeze.lua § phases). Move well clear first so the
-- dwell clock starts from zero rather than from whatever C4 left it at.
verify.hover(p.x - 300.0, p.y + 200.0, 3)
verify.hover(p.x, p.y, 31)
t = verify.pointer_target()
print(string.format("C5 glance: hovered_province=%d card=%s stuck=%s",
                    t.hovered_province, tostring(t.hover_card), tostring(t.hover_card_stuck)))
verify.expect(t.hovered_province ~= 0,
              "C5a the canvas hit-test resolved a province under the synthetic cursor")
verify.expect(t.hover_card, "C5b the glance card appeared after 31 dwell frames")
verify.capture("click_injection_04_hover_glance")

verify.hover(p.x, p.y, 125)
t = verify.pointer_target()
verify.expect(t.hover_card_stuck, "C5c the card stuck past the 150-frame dwell")
verify.capture("click_injection_05_hover_stuck")
