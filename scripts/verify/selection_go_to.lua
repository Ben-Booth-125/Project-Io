-- Visual verification for the Selection panel 'go to' landing rung.
-- Confirms REQUIREMENTS.md § selection-go-to-planetary R3/R5:
--   R3  'go to' lands on the Planetary tile surface for ANY body, not only the
--       home planet (Kepler) — the "only works for Kepler" symptom was an
--       unhelpful landing rung (circumplanetary), not an id/lookup failure.
--   R5  verify.go_to drives focus_on_entity (the real 'go to' path), so each
--       capture is evidence of where 'go to' actually lands.
--
-- Each capture should show a Planetary tile grid (the most informative rung), not
-- the orbital / circumplanetary local view that the old focus_on_body produced.
-- Driver: direct state manipulation. Captures land in screenshots/<name>.png.
-- Run with: ProjectIo --verify scripts/verify/selection_go_to.lua

-- Home planet — the body that already worked before the fix (control case).
verify.go_to("home")
verify.capture("go_to_00_kepler_home")

-- A different planet: must now land on its Planetary surface, not Cinder's
-- circumplanetary view. This is the case the old dispatch got wrong.
verify.go_to("inner")
verify.capture("go_to_01_cinder_planet")

-- A moon: focus_on_entity resolves a moon through focus_on_surface too, so 'go to'
-- lands on Selene's own tile surface.
verify.go_to("moon")
verify.capture("go_to_02_selene_moon")
