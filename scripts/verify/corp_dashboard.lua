-- BL-248 (Corporation dashboard roll-ups) — requirements.json § corp-dashboard-rollups.
-- Supersedes this script's BL-022 content: slot 1 no longer draws an
-- all-corporations balance table (that lives on the Diplomacy slot).
--
--   R1  four roll-ups — Production / Trade / Workforce / Finance — each resting as
--       ONE verdict line, each expanding through BL-214's shared chevron
--   R2  every figure is read from the live world + economy_report
--   R3  four card-specific drills, four genuinely different shapes
--   R4  each roll-up chart carries a BL-247 question log
--
-- Ticks first: the roll-ups report a measured tick, and the Finance card says so
-- explicitly rather than drawing five zeroes when no tick has been recorded.
verify.econ_step(6)
verify.show_panel("corporation", true)

-- ── R1: the resting dashboard. Four lines, four verdicts, no charts. ──
verify.fold()
shot("corp_dashboard")

-- ── R1 / R2: each card, full screen — chart and drillable rows. (The question
--    log this used to capture was removed 2026-08-02, NEEDS_REVIEW NR-018.) ──
local cards = {"production", "trade", "workforce", "finance"}
for i, name in ipairs(cards) do
    verify.fold("corp_rollup", i - 1)
    verify.rollup_drill(-1)
    shot("corp_rollup_" .. name)
end

-- ── R3: the drills. Four different shapes, not one generic detail panel —
--    a building's operating economics, a lane's traffic, a building's labour.
--    Finance has no per-item drill (its subject is its own flows), which is why
--    it is absent here rather than faked. ──
for i, name in ipairs({"production", "trade", "workforce"}) do
    verify.fold("corp_rollup", i - 1)
    verify.rollup_drill(0)
    shot("corp_drill_" .. name)
end

verify.rollup_drill(-1)
verify.fold()
verify.show_panel("corporation", false)
