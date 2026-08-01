-- BL-247 (chart question log) — requirements.json § chart-question-log.
--
--   R1  a chart's "Why this chart" toggle is CLOSED by default and, when opened,
--       reveals exactly two lines: Answers: / Because:
--   R2  the pair is optional — a chart with no authored pair draws no toggle
--   R3  one note is open at a time, and it resets closed when a view opens
--
-- The logs live in the expanded views (a chart row in the 380 px ledger column has
-- no line to spare), so each capture expands a surface first.

verify.econ_step(4)

-- ── R1 / R3: closed by default. The wizard's first stage, expanded, with every
--    chart showing only its "? Why this chart +" toggle. Nothing is revealed
--    unprompted — this is the whole point of the affordance. ──
verify.show_generation(true)
verify.generation_stage(0)
verify.why_note(false)
verify.fold("generation_stage", 0)
shot("why_note_closed")

-- ── R1: opened. Exactly two lines, and the row RESERVED their measured height —
--    a clipped note would be this item creating the fitting defect BL-215 audits. ──
verify.why_note(true)
shot("why_note_open")

-- ── R3: one at a time. Folding closes the note, so re-expanding shows the
--    resting state rather than a note left open from a previous read. ──
verify.fold()
verify.fold("generation_stage", 1)
shot("why_note_resets_on_expand")
verify.fold()
verify.show_generation(false)

-- ── R1: the same affordance on a different surface, proving it is the shared
--    helper and not a per-chart imitation. The Selection band's expanded metric
--    carries its own authored pair. ──
verify.goto_surface("Kepler")
verify.select_tile(110, 79)
verify.fold("selection_metric", 0)
verify.why_note(true)
shot("why_note_selection_metric")
verify.fold()
