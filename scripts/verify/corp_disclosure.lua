-- The Corporations panel after the standing bands were retired
-- (BL-633 / retire-standing-bands R2).
--
--   R2  every row prints an EXACT reach and an EXACT market share, player and
--       rival alike; Capital prints exactly only where the firm files
--       (ownership_class == public) and shows a dash otherwise.
--
-- It is an ACCEPTANCE script, not a golden gate: the verdict is verify.expect
-- and the captures are evidence. The presses are injected through ImGui's own
-- event queue (verify.click, BL-521), so a control that renders past the
-- panel's edge fails the hit-test here rather than passing on a screenshot —
-- the BL-449 failure mode this script exists to refuse.
--
-- IT IS STILL NOT A HUMAN LIVE CLICK, and that distinction is deliberate. This
-- proves a synthesised press lands on the row; only a person at the app can say
-- the surface READS right.
--
-- WHAT TO LOOK FOR IN THE CAPTURE, because a green run proves nothing here:
--   * NO row anywhere says Negligible / Minor / Notable / Major / Dominant.
--     Those five words are the retired bands; one of them on screen is a fail.
--   * Reach reads "N bodies" and Share reads "N%" on EVERY row, not just the
--     player's tinted one.
--   * The Capital column. A DEFAULT world cannot exercise both halves of the
--     gate and it is important to know why before reading the picture: the
--     default campaign is an ANTIQUITY world, settlement lights no furnace, so
--     every region is never-industrialised and corporation_generation waives
--     its public floor as unmeetable (`public_reachable`). Every corp is
--     therefore `closed` and every Capital cell is legitimately a dash. The
--     exact-figure half of the gate is covered headlessly instead, by
--     tools/verify/standing_harness.cpp, which builds public firms directly.
--
-- Run: ProjectIo --verify scripts/verify/corp_disclosure.lua
-- Inspect: screenshots/corp_disclosure*.png

verify.window(1280, 720)

-- Ticks first. Market share is this tick's clearing income over the total, so a
-- capture taken before any trade shows 0% on every row and would pass vacuously
-- while telling you nothing about whether the axis prints at all.
verify.econ_step(6)

-- econ_step opens the economy panel and the fold-out column holds ONE occupant,
-- so close it explicitly before opening the corporations table.
verify.show_panel("economy", false)
verify.show_panel("corporations_table", true)
verify.frames(2)

verify.capture("corp_disclosure_table")

-- R2, the press half. The row Selectable spans all columns and routes the corp
-- to the Selection element; a click on the table body must therefore land a
-- CORPORATION selection. Coordinates are the first data row of the table in the
-- fold-out column at 1280x720.
verify.clear_selection()
verify.frames(1)
verify.click(150, 130)
verify.frames(2)

local t = verify.pointer_target()
verify.expect(t.has_selection,
    "R2: a click on a Corporations row selects something")
verify.expect(t.selection_kind == "corporation",
    "R2: that something is a CORPORATION (kind=" .. tostring(t.selection_kind) .. ")")

verify.capture("corp_disclosure_row_selected")

-- BL-215's text-overflow ledger over everything drawn above. This is the check
-- that would have caught BL-449 shipping a press rendered past the panel's edge,
-- so it is run rather than assumed.
verify.expect_no_clipping("corporations_table")
