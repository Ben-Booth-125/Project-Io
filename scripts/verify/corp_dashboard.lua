-- The Corporation ledger — BL-691 (corp: how am I doing).
--
-- Supersedes this script's four-card content. The ledger asked four questions
-- and three of them were answered better elsewhere (Production and Workforce by
-- the Construction ledger's Buildings tab, Trade by the Market and Convoys
-- ledgers), so it now holds ONE card, Balance: earnings against stacked
-- expenses, drawn by the same ui::charts drawer the building card uses.
--
--   R1  exactly one card is drawn, and it shows its chart AT REST
--   R2  the chart's expense segments sum to the quarter's real expenditure
--       from corp_budget — the chart and the budget are one arithmetic
--   R3  interest is ABSENT from the stack while the corp is solvent and
--       PRESENT once it is in debt (the variable segment count)
--   R4  the full-canvas takeover still opens
--
-- CAPTURED AT 1920x1080 DELIBERATELY. The void this item exists to fix — four
-- verdict lines over ~700 px of empty column — was only visible at Ben's own
-- resolution; it got worse at 1080p rather than dissolving, which is why a
-- 1280-wide capture would have shown the least interesting half of the problem.
--
-- expect_no_clipping is NOT called here: NR-663 records it as VACUOUS on this
-- class of surface, passing with zero records over visibly clipped frames. The
-- captures are looked at instead.

verify.window(1920, 1080)
verify.econ_step(6)
verify.show_panel("corporation", true)
verify.fold()

-- ── R1: one card, resting, with its chart under it ──
shot("corp_dashboard")

local card = verify.corp_balance_card()
verify.expect(card.cards == 1,
    string.format("R1 the ledger draws exactly one card: %d", card.cards))
verify.expect(card.measured,
    "R1 six economy quarters were measured, so the card charts real flows")

-- ── R2: the chart IS the budget. Every drawn expense segment summed against
--    the quarter's own expenditure — read through the same builder the drawer
--    calls, so this cannot pass while the drawing disagrees. ──
local sum = 0.0
local names = {}
for _, seg in ipairs(card.expenses) do
    sum = sum + seg.value
    names[seg.label] = seg.value
end
verify.expect(math.abs(sum - card.expense_total) < 0.01,
    string.format("R2 the stack sums to the card's own expenditure: %.3f vs %.3f",
                  sum, card.expense_total))
verify.expect(#card.expenses > 0,
    string.format("R2 the expense stack is not vacuously empty: %d segments", #card.expenses))
verify.expect(#card.earnings >= 1,
    string.format("R2 the earnings column carries at least Income: %d segments",
                  #card.earnings))

-- The net line under the chart is the two columns differenced, so a chart that
-- summed correctly but charted the wrong side would still be caught.
local earned = 0.0
for _, seg in ipairs(card.earnings) do earned = earned + seg.value end
verify.expect(math.abs((earned - sum) - card.net) < 0.05,
    string.format("R2 earnings less expenses IS the printed net: %.3f vs %.3f",
                  earned - sum, card.net))

-- ── R3a: solvent — interest is absent from the stack, not a flat zero. ──
verify.expect(names["Interest"] == nil,
    "R3 a solvent corp's stack carries NO Interest segment")
local solvent_segments = #card.expenses

-- ── R3b: in debt — interest joins the stack, one segment longer. ──
verify.set_balance(-5000)
verify.econ_step(1)
local debt = verify.corp_balance_card()
local debt_names = {}
local debt_sum = 0.0
for _, seg in ipairs(debt.expenses) do
    debt_sum = debt_sum + seg.value
    debt_names[seg.label] = seg.value
end
verify.expect(debt_names["Interest"] ~= nil and debt_names["Interest"] > 0.0,
    string.format("R3 an indebted corp's stack carries Interest: %s",
                  tostring(debt_names["Interest"])))
verify.expect(#debt.expenses == solvent_segments + 1,
    string.format("R3 the stack grew by exactly one segment: %d -> %d",
                  solvent_segments, #debt.expenses))
verify.expect(math.abs(debt_sum - debt.expense_total) < 0.01,
    string.format("R3 the indebted stack still sums to its expenditure: %.3f vs %.3f",
                  debt_sum, debt.expense_total))
shot("corp_dashboard_in_debt")

-- ── R4: the full-canvas takeover still opens on this surface. ──
verify.fold("corp_rollup", 0)
shot("corp_rollup_balance")
verify.fold()

verify.show_panel("corporation", false)
