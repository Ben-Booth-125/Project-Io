-- Verify the History ledger's Ages view — the Era -1 political time-lapse
-- (BL-277). The sim runs lazily over a COPY of the body's settlement state when
-- the view is first opened, so these captures also prove the view drives a real
-- 0 -> 1960 run without world generation being involved at all.
--
-- Three captures, one per thing that can independently be wrong: that the
-- antiquity start renders as scattered independent powers, that the map has
-- actually changed by the midpoint (borders moved), and that the epoch frame
-- shows a settled political map with the run's own counters under it.
verify.show_panel("economy", false) -- econ_step force-opens it; keep the shot clean
verify.show_panel("tile", true)
verify.panel_view("history", 3)     -- 3 = Ages

verify.panel_view("ages_year", 0)
verify.capture("history_ages_start")

verify.panel_view("ages_year", 900)
verify.capture("history_ages_mid")

verify.panel_view("ages_year", 1960)
verify.capture("history_ages_epoch")
