-- BL-214 (drill-through: one binary fold idiom) — requirements.json § drill-through-fold.
--
-- Walks every surface on the ladder through BOTH of its states, so the captures
-- prove the idiom is shared rather than re-invented per surface:
--
--   R1  every dense surface folds and expands through the same chevron
--   R2  the overlay composes with BL-196's subject drill (breadcrumb + back)
--   R3  expanding a second surface folds the first
--   R5  the Selection band still RESTS expanded-in-place (Ben, 2026-08-01) —
--       its fixed 260 px rect is never left showing one line over empty space
--   R6  the wizard folds PER CHAIN STAGE
--
-- Read the pairs side by side: `*_folded` then `*_expanded` for each surface.

-- ── R5: the Selection band at rest. A tile selection, nothing expanded. ──
-- The harness opens the Economy panel by default; close it, or it occupies the
-- shell column the History captures below need.
verify.econ_step(4)
verify.show_panel("economy", false)
verify.goto_surface("Kepler")
-- A tile with a real deposit, so the metric pages are drillable resources rather
-- than the habitability/hazard fallback (which has no time series to drill into).
verify.select_tile(110, 79)
verify.fold()
shot("fold_band_resting")

-- ── R1: the same metric, full screen. The chart gets the height it never had,
--    and carries its question log (BL-247) beneath it. ──
verify.fold("selection_metric", 0)
shot("fold_band_expanded")

-- ── R2: Depth composes with Subject. Drill from inside the expanded view; the
--    breadcrumb and back control belong to BL-196 and are untouched by the fold. ──
verify.card_drill(0)
shot("fold_band_expanded_drilled")
verify.card_drill(-1)

-- ── R3: expanding a second surface folds the first. The subject was the History
--    ledger's Tiles view until BL-281 retired it; the Story view inherits the same
--    view-level chevron on the same tab row, so the assertion moves rather than
--    weakening. Story is what the fold now helps most on this ledger: the biography
--    is long wrapped prose, and the 380 px column breaks every line of it. ──
verify.show_panel("tile", true)
verify.panel_view("history", 0)
verify.fold()
shot("fold_history_story_folded")
verify.fold("history_story", 0)
shot("fold_history_story_expanded")

-- ── R1: the History Chain. Its stages used to be CollapsingHeaders — this
--    surface's own private disclosure idiom, the fourth one the item retires.
--    Folded they are four verdict lines; expanded, one stage owns the screen. ──
verify.panel_view("history", 1)
verify.panel_view("history_round", 0)
verify.fold()
shot("fold_history_chain_folded")
verify.fold("history_chain", 0)
shot("fold_history_chain_expanded")
verify.fold()
verify.show_panel("tile", false)

-- ── R1: the Corporation dashboard's four roll-ups, resting as verdict lines. ──
verify.show_panel("corporation", true)
shot("fold_corp_rollups_folded")
verify.show_panel("corporation", false)

-- ── R6: the wizard, per stage. Folded, a round's stages fit one screen as
--    verdicts; expanding opens the stage the roll made interesting. This is where
--    the idiom is taught — it is the first surface a player ever meets. ──
verify.show_generation(true)
verify.generation_stage(0)
verify.fold()
shot("fold_wizard_stages_folded")
verify.fold("generation_stage", 0)
shot("fold_wizard_stage_expanded")
verify.fold()
verify.show_generation(false)
