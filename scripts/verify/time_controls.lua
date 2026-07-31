-- Verify the time-speed curve (BL-007 / time-speed-curve R1) and the BL-178
-- time-control legibility work.
--
-- BL-178 supersedes the original R3 ("ProgressBar empty overlay"): the bar now
-- carries a centred "<N> d to Q<n>" overlay, because the distance to the next
-- economic resolution was the fact the bare bar failed to convey. The speed
-- tiers also gained per-button tooltips and an always-visible active-rate line.
--
-- SCOPE LIMIT (deliberate): only the PAUSED state is captured. Verify mode pins
-- the sim at speed 0 on purpose - "sim left paused so orbits and ticks never
-- advance between captures" (app.cpp run_verify) - and a nonzero speed would
-- advance the day counter on real frame timing, making captures
-- non-deterministic. So the running rate line ("1x  .  ~3m per quarter") is
-- verified by code inspection, exactly as R2/R3 already are, not by capture.
-- Both labels derive from sim_loop::speed_multiplier / seconds_per_day_1x /
-- econ_tick_days, so they cannot drift from the curve they describe.

verify.capture("time_controls_default")
