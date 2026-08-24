-- The zoom ladder: what the Planetary canvas looks like at each LOD band.
--
-- WHY. "It looks blurry" is not actionable until you know WHICH zoom, because the
-- canvas has four bands and they fail differently:
--   * below  7 px circumradius — coarse fill; no hexes, no texture
--   * below 14 px — hexes, no texture at all (the pattern would be moire)
--   *  14–22 px — texture ramping in
--   * above 22 px — texture at full strength
-- and every lens attenuates texture to 0.45 on top of that, so a lens frame is a
-- second, compounding softening. This walks the bands at one spot so the band can
-- be named before anything is tuned (BL-597, Rule 0b).
--
-- WHAT IT FOUND, 2026-08-24: the softness is not the texture pass at all. Land
-- tiles lost their hex edges entirely from ~3.5x up and read as a continuous smear,
-- while OCEAN tiles stayed crisp — the signature of the province blend (ocean is
-- province 0 and does not blend), not of a faint texture. So the texture constants
-- were never the dial.
--
-- WHAT IT NOW GUARDS (BL-597). The blend is dialled by
-- `k_province_blend_strength` (body_surface_canvas.cpp), shipped at 0.35. The
-- constant's contract is that **1.0 reproduces the pre-BL-597 render byte for
-- byte** — set it to 1.0, re-run this ladder, and the captures must match the
-- pre-change ones exactly. If they do not, the lerp is wrong, not the value.
--
-- The lens rungs use the Corporation lens. They used the Country lens until
-- BL-601 retired it: national borders are always-on chrome now, so a "lens frame"
-- had to be a lens that still exists. Any lens attenuates texture to 0.45, which
-- is all these two rungs are testing.
--
-- Run: ProjectIo --verify scripts/verify/zoom_ladder.lua

verify.window(1280, 720)
local s = stage_ui_fixture()

for i, z in ipairs({ 1.0, 2.0, 3.5, 5.0, 7.0, 10.0 }) do
    verify.center_tile(s.unit.col, s.unit.row, z)
    verify.frames(4)
    verify.capture(string.format("zoom_%d_x%0.1f", i, z))
end

-- The same ladder under a lens, since the 0.45 attenuation compounds with whatever
-- the base pass is already doing.
verify.set_overlay("corporation")
for i, z in ipairs({ 3.5, 7.0 }) do
    verify.center_tile(s.unit.col, s.unit.row, z)
    verify.frames(4)
    verify.capture(string.format("zoomlens_%d_x%0.1f", i, z))
end
verify.set_overlay("none")
