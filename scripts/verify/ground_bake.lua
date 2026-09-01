-- BL-732 (ground bake renderer) + wave 2 — the baked painterly ground,
-- docs/ui/RENDERING.md. Requirement groups `ground-bake-renderer` and
-- `ground-wave-2`.
--
-- Under --verify the ground cache bakes synchronously on the main thread
-- (app::m_ground_bake_all), so no capture can race the worker. The request →
-- bake → publish loop is one frame behind the canvas, so every zoom change
-- settles with verify.frames(2) before its capture.
--
-- Zooms below sit on (or near) the stepped x2 ladder's rungs — kMinZoom * 2^k,
-- k = 0..4 — so each capture exercises one bake tier: far page (6 px/r), then
-- the 12 / 24 / 48 / 96 px chunked tiers.

verify.goto_surface("home")
verify.set_overlay("none")

-- Rung 0, whole grid: the far page. The wrap seam falls inside this frame;
-- nothing may mark it. Border band now the muted single-tile ring.
verify.set_zoom(1.26)
verify.frames(2)
verify.capture("ground_bake_wide")

-- Rung 1 (~13 px hexes): the 12 px tier — without this frame that tier has no
-- exercise at all (review fleet, 2026-09-01).
verify.set_zoom(2.5)
verify.frames(2)
verify.capture("ground_bake_mid")

-- Rung 2 (~27 px hexes): the 24 px tier — the working play view.
verify.set_zoom(5)
verify.frames(2)
verify.capture("ground_bake_play")

-- Rung 3 (~51 px hexes): the 48 px tier, close-grain octave active. The bake's
-- own grain carries the ground; the vector texture pass must NOT be drawing.
verify.set_zoom(10)
verify.frames(2)
verify.capture("ground_bake_close")

-- Rung 4 (~102 px hexes): the 96 px tier — the top of the ladder.
verify.set_zoom(20)
verify.frames(2)
verify.capture("ground_bake_closest")

-- Wrap: back to the play tier, panned half a period so the cylinder seam
-- crosses mid-frame.
verify.set_zoom(5)
verify.add_pan(-2600, 0)
verify.frames(2)
verify.capture("ground_bake_seam")

-- Bare-ground judgement pair — national border band off (verify-only toggle)
-- so the ground is judged against it3 C-F without the chrome.
verify.add_pan(2600, 0)
verify.set_border_band(false)
verify.set_zoom(1.26)
verify.frames(2)
verify.capture("ground_bake_bare_wide")
verify.set_zoom(5)
verify.frames(2)
verify.capture("ground_bake_bare_play")
verify.set_border_band(true)

-- Fallback: a lens rung renders through the CLASSIC per-tile path, unchanged —
-- the fallback is alive and lens rendering is BL-734's, not ours.
verify.set_overlay("resource")
verify.frames(1)
verify.capture("ground_bake_lens_fallback")
verify.set_overlay("none")
