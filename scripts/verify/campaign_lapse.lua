-- campaign_lapse — BL-723's visual half: the spectated campaign as a film strip.
--
-- Spectate + god view, then one Corporation-lens capture every 4 econ ticks
-- (one game-year per frame) for 30 frames — 30 years of corp-border spread on
-- the home body — closing with a Market and a Scarcity still so the price and
-- shortage picture at the end state is on film too.
--
-- CAPTURE-ONLY, NO GOLDENS, deliberately (the decision_feed.lua rationale):
-- every frame reads live AI economy state, and the on-screen normalisations
-- are frame-relative by design — the CSV (tools/verify/campaign_lapse.cpp) is
-- the data; these frames are only the film. Frames land in screenshots/ and
-- are stitched to a time-lapse outside the engine.
--
-- Run: ProjectIo --verify scripts/verify/campaign_lapse.lua

-- Spectate BEFORE stepping (econ_step reads the flag per call); god view so the
-- whole field is visible — presentation-only, the sim never sees it.
verify.spectate(true)
verify.god_view(true)

verify.goto_surface("home")
verify.clear_selection()
verify.set_overlay("corporation")
-- 0.8 frames the whole main continent (probed 2026-09-01; higher zoom = closer).
verify.set_zoom(0.8)
verify.frames(2)
verify.capture("lapse_corp_y00")

for year = 1, 30 do
    verify.econ_step(4)             -- one game-year of quarters
    verify.frames(2)
    verify.capture(string.format("lapse_corp_y%02d", year))
end

-- The end state through the other two measurement lenses: prices vs base, and
-- where demand outran supply — one still each.
verify.set_overlay("market")
verify.set_lens_resource("food_rations")
verify.frames(2)
verify.capture("lapse_market_food_end")

verify.set_overlay("scarcity")
verify.frames(2)
verify.capture("lapse_scarcity_food_end")

verify.expect(true, "campaign lapse frames captured")
