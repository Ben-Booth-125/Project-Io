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
-- there.
--
-- BL-598 (2026-08-24) CHANGED WHAT A PLAIN-GROUND CLICK LANDS ON. It used to
-- land on the tile's PROVINCE, leaving `selected_entity` null (BL-511); the
-- province has since folded into the tile Selection element as a set of
-- accordion sections, so the click lands on the TILE and `selected_province`
-- carries the province as a derived mirror. Both are set now — which is the
-- assertion below, and the one that would have failed silently under the old
-- wording (`selected_province ~= 0` alone was true before AND after).
local landed = verify.click_tile(COL, ROW)
verify.expect(landed, "C1a click_tile resolved a screen point and pressed")

local t = verify.pointer_target()
print(string.format("C1 after click: province=%d has_entity=%s at (%.0f,%.0f)",
                    t.selected_province, tostring(t.has_selection), t.x, t.y))
verify.expect(t.selected_province ~= 0 and t.has_selection,
              "C1b the injected click selected the TILE and mirrored its province")
verify.capture("click_injection_01_clicked_tile")

local pid_click = t.selected_province

-- C2 — the click AGREES with the shortcut it stands in for. verify.select_province
-- writes the state a click is supposed to produce; if the two disagree, one of
-- them is lying about the gesture, and nothing else could tell which. The hook
-- FOLLOWS the gesture since BL-598: it selects the tile and returns the province
-- id, rather than writing a province-only tuple no player can reach.
verify.clear_selection()
local pid_direct = verify.select_province(COL, ROW)
local d = verify.pointer_target()
verify.expect(pid_click == pid_direct,
              string.format("C2a injected click and select_province agree (%d vs %d)",
                            pid_click, pid_direct))
verify.expect(d.selected_province == pid_direct and d.has_selection,
              "C2b select_province leaves the same tuple the click does")

-- C3 — a SECOND press on the same tile cycles the rung. This is the real
-- evidence that press number two is a distinct press and not a repaint.
--
-- IT HAS TO BE A BUILT TILE. The cycle is BATTLE > UNIT > BUILDING > TILE since
-- BL-598 dissolved the province rung, so on bare ground exactly ONE rung is live
-- and a repeat click honestly re-selects the same tile — no observable advance,
-- and nothing for this check to read. A tile carrying a building has two live
-- rungs, and the two are told apart by `selected_province`: the BUILDING rung
-- clears the mirror (a building card has no province section), the TILE rung
-- carries it. Zero vs non-zero is the whole assertion.
local built = nil
for _, b in ipairs(verify.buildings()) do built = b; break end
verify.expect(built ~= nil, "C3a a building exists on the home body to cycle on")

verify.click_tile(built.x, built.y)          -- first press: the building marker
t = verify.pointer_target()
print(string.format("C3 first press on a built tile: province=%d has_entity=%s",
                    t.selected_province, tostring(t.has_selection)))
verify.expect(t.selected_province == 0 and t.has_selection,
              "C3b the first press resolved the BUILDING marker (no province mirror)")

local bp = verify.tile_screen(built.x, built.y)
verify.expect(bp.ok, "C3c tile_screen resolved the built tile's screen centre")
verify.click(bp.x, bp.y)                     -- second press: advance to the tile
t = verify.pointer_target()
print(string.format("C3 after repeat click: province=%d has_entity=%s",
                    t.selected_province, tostring(t.has_selection)))
verify.expect(t.selected_province ~= 0 and t.has_selection,
              "C3d the repeat click advanced the cycle from building to tile")
verify.capture("click_injection_02_cycle_advanced")

-- C4 — double_click delivers TWO presses, not one. From the tile rung two
-- advances wrap past the dead battle and unit rungs to the BUILDING and then on
-- to the TILE again, so the mirror must be non-zero after it. One press would
-- have left it on the building with the mirror cleared, so this distinguishes
-- the two.
verify.double_click(bp.x, bp.y)
t = verify.pointer_target()
print(string.format("C4 after double click: province=%d has_entity=%s",
                    t.selected_province, tostring(t.has_selection)))
verify.expect(t.selected_province ~= 0 and t.has_selection,
              "C4 double_click pressed twice (two rung advances, not one)")
verify.capture("click_injection_03_double_click")

local p = bp

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
