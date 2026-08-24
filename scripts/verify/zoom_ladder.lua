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
-- tiles lose their hex edges entirely from ~3.5x up and read as a continuous smear,
-- while OCEAN tiles stay crisp — which is the signature of BL-511's province blend
-- (ocean is province 0 and does not blend), not of a faint texture. Keep that in
-- mind before reaching for the texture constants.
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
verify.set_overlay("country")
for i, z in ipairs({ 3.5, 7.0 }) do
    verify.center_tile(s.unit.col, s.unit.row, z)
    verify.frames(4)
    verify.capture(string.format("zoomlens_%d_x%0.1f", i, z))
end
verify.set_overlay("none")
