-- Visual verification for the debt-interest system (BL-073).
-- Stages a negative player balance, then runs one economy tick so apply_budget
-- charges interest = |balance| × k_debt_interest_per_quarter on the still-negative
-- balance. Captures (a) the header in-debt affordance and (b) the Balance Ledger,
-- which now shows the Interest (debt) line in the cashflow breakdown and an
-- "in debt" runway.
-- Run with: ProjectIo --verify scripts/verify/debt_interest.lua

verify.econ_step(4)          -- warm the economy and accumulate some balance history
verify.set_balance(-5000)    -- push the player corp underwater (BL-073 verify hook)
verify.econ_step(1)          -- one tick: operating flows + interest on the debt
verify.show_panel("economy", false)
verify.show_panel("balance", true)
verify.capture("debt_interest_ledger")
