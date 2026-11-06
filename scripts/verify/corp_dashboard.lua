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
--   R5  Standing ranks the nation on GDP, market share, reach and total force
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

-- R5a (BL-1509): Total force at the opening, before any step. Six econ_steps leave no unit
-- on the --verify world, so the later R5 reads a real zero; this one must read the garrisons.
verify.show_panel("corporation", true)
verify.frames(2)
do
    local own = 0
    for _, u in ipairs(verify.units()) do
        if u.player and u.count > 0 then own = own + 1 end
    end
    for _, r in ipairs(verify.nation_standing()) do
        if r.label == "Total force" then
            print(string.format("corp_dashboard: opening force %.0f of leader %.0f, rank %d of %d",
                                r.value, r.leader, r.rank, r.of))
            verify.expect(own > 0 and r.value == own, "R5a Total force counts the player's units: " .. own)
            verify.expect(r.leader >= r.value, "R5a the leader's force is at least the player's")
        end
    end
    shot("corp_dashboard_opening_force")
end
verify.show_panel("corporation", false)

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

-- ── R5 (BL-1509): Standing ranks the nation on four public readings, each a real rank
--    against a real leader, and the card shows them at rest. ──
local rows = verify.nation_standing()
verify.expect(#rows == 4, string.format("R5 Standing carries four rows: %d", #rows))
local wanted = { GDP = true, ["Market share"] = true, Reach = true, ["Total force"] = true }
for _, r in ipairs(rows) do
    print(string.format("corp_dashboard: %s value %.2f leader %.2f rank %d of %d",
                        r.label, r.value, r.leader, r.rank, r.of))
    verify.expect(wanted[r.label] == true, "R5 a named reading: " .. r.label)
    verify.expect(r.of >= 2, "R5 ranked among more than one nation: " .. r.label)
    verify.expect(r.rank >= 1 and r.rank <= r.of, "R5 the rank lies in the field: " .. r.label)
    verify.expect(r.value <= r.leader + 1e-3, "R5 no figure exceeds its leader's: " .. r.label)
end
shot("corp_dashboard_standing")

verify.show_panel("corporation", false)
