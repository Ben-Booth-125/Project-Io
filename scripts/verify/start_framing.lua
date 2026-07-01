-- Campaign-start default after the framing + persistent-accent work.
-- No goto_surface: keep the HQ framing that setup_world queued.
verify.capture("start_warmup")           -- settle strip
verify.capture("start_default")          -- HQ-framed, plain lens, player wash + ring
verify.set_overlay("production")         -- a value lens: ring must persist over it
verify.capture("start_under_production_lens")
verify.set_overlay("corporation")
verify.capture("start_under_corp_lens")
