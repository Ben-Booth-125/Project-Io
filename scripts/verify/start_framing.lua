-- Campaign-start default after the framing + persistent-accent work.
-- No goto_surface: keep the HQ framing that setup_world queued.
verify.capture("start_warmup")           -- settle strip
verify.capture("start_default")          -- HQ-framed, plain lens, player wash + ring
verify.set_overlay("population")         -- a value lens: ring must persist over it
verify.capture("start_under_population_lens")
verify.set_overlay("nations") -- BL-1406: the Corporation lens retired
verify.capture("start_under_corp_lens")
