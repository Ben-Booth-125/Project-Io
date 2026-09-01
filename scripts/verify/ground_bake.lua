-- BL-732 (ground bake renderer) — the baked painterly ground, docs/ui/RENDERING.md.
-- Requirement group `ground-bake-renderer` rows R1 + R3.
--
-- Under --verify the ground cache bakes EVERY chunk synchronously
-- (app::m_ground_bake_all), so no capture here can race the per-frame budget.
--
-- R1: with no lens active the Planetary ground draws from the baked chunks —
-- continuous painterly terrain, hillshaded relief, the C-F near-future grade,
-- NO hex grid lines — judged against docs/ui/design/renders/map/it3 panel C-F
-- for direction (colour family, relief legibility, grade coolness), never for
-- pixel identity.

verify.goto_surface("home")
verify.set_overlay("none")

-- Whole-grid view: the far page + full-res chunks at the widest read. The wrap
-- seam falls inside this frame; nothing may mark it.
verify.set_zoom(1)
verify.capture("ground_bake_wide")

-- Play zoom: the working view. Relief and biome variation must read; no grid.
verify.set_zoom(3)
verify.capture("ground_bake_play")

-- Close zoom: texture magnification — the bake's own grain carries the ground,
-- the vector texture pass must NOT be drawing (it is gated off on_bake).
verify.set_zoom(6)
verify.capture("ground_bake_close")

-- R1 wrap: pan half a period east so the cylinder seam crosses mid-frame.
verify.set_zoom(3)
verify.add_pan(-2600, 0)
verify.capture("ground_bake_seam")

-- R1 judgement pair: the bake BARE — national border band off (verify-only
-- toggle) — so the ground can be judged against it3 C-F without the chrome
-- BL-734 has yet to re-weigh. Wide + play zoom.
verify.add_pan(2600, 0)
verify.set_border_band(false)
verify.set_zoom(1)
verify.capture("ground_bake_bare_wide")
verify.set_zoom(3)
verify.capture("ground_bake_bare_play")
verify.set_border_band(true)

-- R3: a lens rung renders through the CLASSIC per-tile path, unchanged by this
-- item — the fallback is alive and lens rendering is BL-734's, not ours.
verify.set_overlay("resource")
verify.capture("ground_bake_lens_fallback")
verify.set_overlay("none")
