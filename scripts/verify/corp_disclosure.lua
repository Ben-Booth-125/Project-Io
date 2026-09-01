-- The Corporations panel after the standing bands were retired
-- (BL-633 / retire-standing-bands R2).
--
--   R2  Capital prints exactly only where the firm files (ownership_class ==
--       public) and shows a dash otherwise, and a row click routes the corp to
--       the Selection element.
--
-- REACH AND SHARE ARE NO LONGER ON THIS SURFACE. R2 originally covered them too —
-- "every row prints an EXACT reach and an EXACT market share" — but the ledger was
-- regrouped by stance and both columns came off it: at the fold-out column's width
-- they cost the firm's NAME, and neither is a stance fact. They are still computed
-- on `corp_standing`, and the per-firm financial comparison they were reaching for
-- belongs to BL-627 (profitability ledger). Capital is the axis that stayed,
-- because the DISCLOSURE GATE is the part with a rule behind it, and that gate is
-- what this script exists to check. The grouped shape itself is covered by
-- scripts/verify/diplomacy_groups.lua.
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
--   * Every firm's NAME is legible in full. A name cut to one or two letters is
--     the defect this surface's width budget exists to prevent.
--   * The Capital figure. A DEFAULT world cannot exercise both halves of the
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

verify.show_panel("corporations_table", true)
verify.frames(2)

verify.capture("corp_disclosure_table")

-- R2, the press half. The row's name Selectable routes the corp to the Selection
-- element; a click on it must therefore land a CORPORATION selection.
--
-- IT AIMS AT THE ROW THE PANEL REPORTS, not at a literal. This used to be
-- `verify.click(150, 130)` — the first data row of the old five-column table at
-- 1280x720 — and that coordinate silently stopped being a row when the ledger was
-- regrouped by stance: it landed on the Friends group's empty-state line, selected
-- nothing, and the failure said "a click selects something" rather than "the row
-- moved". A coordinate literal cannot survive a layout change, so it does not get
-- to be the thing under test here.
verify.clear_selection()
verify.frames(1)

local first_row = verify.corp_panel_rows()[1]
verify.expect(first_row ~= nil, "R2: the ledger drew a row to click")
if first_row ~= nil then
    verify.click(first_row.x, first_row.y)
end
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
