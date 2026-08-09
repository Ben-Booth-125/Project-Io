-- Visual verification for the Survey system (overlay-independent; BL-067).
-- Confirms requirements.json § survey-system R1:
--   an unsurveyed body is masked on the Planetary canvas; a partial reveal shows
--   some regions visible and some masked; a full reveal shows the whole surface;
--   the Solar canvas badge reads hidden (?) / in-progress (magnifier + k/N) /
--   surveyed (no badge).
--
-- Driver: verify.set_survey(body, regions_done) sets a body's survey state
-- deterministically (the verify clock is paused). Cinder is a non-home planet
-- (180x84 = 253 survey regions); Kepler (home) opens surveyed.
-- Run with: ProjectIo --verify scripts/verify/survey.lua

-- Planetary mask progression on Cinder.
verify.goto_surface("inner")

verify.set_survey("inner", 0)        -- hidden: the whole surface is masked
verify.capture("survey_planetary_masked")

verify.set_survey("inner", 100)      -- partial: ~100/253 regions revealed (raster)
verify.capture("survey_planetary_partial")

verify.set_survey("inner", 99999)    -- clamped to full: surveyed, whole surface shown
verify.capture("survey_planetary_full")

-- Solar badges: Cinder mid-survey (magnifier + k/N), Selene + asteroids hidden (?),
-- Kepler (home) surveyed (no badge).
verify.set_survey("inner", 100)
verify.goto_surface("home")
verify.command("ascend")   -- Planetary -> Circumplanetary
verify.command("ascend")   -- Circumplanetary -> Solar
verify.capture("survey_solar_badges")
