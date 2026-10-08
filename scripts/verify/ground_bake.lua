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

-- Rung 3 (~51 px hexes): the 48 px tier, close-grain octave active, TILTED
-- 22.5 degrees (BL-737). The bake's own grain carries the ground; the vector
-- texture pass must NOT be drawing.
verify.set_zoom(10)
verify.frames(2)
verify.capture("ground_bake_close")

-- Rung 4 (~102 px hexes): the 96 px tier — the top of the ladder, TILTED 45
-- degrees: height-displaced hills, standing trees, squashed chrome.
verify.set_zoom(20)
verify.frames(2)
verify.capture("ground_bake_closest")

-- Between rungs (free-form zoom 8): tilt is a pure function of zoom, so this
-- lands on the 22.5-degree side of the rung-2/3 midpoint deterministically.
verify.set_zoom(8)
verify.frames(2)
verify.capture("ground_bake_midrung_tilt")

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

-- BL-1241 (structures baked) — requirement group `structures-baked`.
-- What stands on a tile is baked into the ground: one procedural form per
-- depicted subject (RENDERING.md § Installations). verify.stage_gallery()
-- stages the whole roster around the player's HQ — every extraction family,
-- every recipe group, every other placeable type, a construction site, a rival
-- plant, a four-stack tile — and steps the six nearest population centres to a
-- settlement ladder (scale 1-5) plus a ruin. C++ picks the ground; the script
-- learns only where it staged. Captured at EVERY rung, far page included, with
-- the border band off so the structures are judged on bare ground.
verify.set_overlay("none")
verify.set_border_band(false)
local g = verify.stage_gallery()
verify.expect(g.staged >= 35, "stage_gallery staged the whole roster (" .. tostring(g.staged) .. ")")
verify.expect(g.centres == 6, "stage_gallery stepped six centres to the ladder (" .. tostring(g.centres) .. ")")

local rungs = { { 1.26, "far" }, { 2.5, "mid" }, { 5, "play" }, { 10, "close" }, { 20, "closest" } }
for _, r in ipairs(rungs) do
    verify.center_tile(g.col, g.row, r[1])
    verify.frames(3)
    verify.capture("structures_" .. r[2])
end

-- The stacked tile up close (four stacks staged; three stand, dominant front).
verify.center_tile(g.stack_col, g.stack_row, 20)
verify.frames(3)
verify.capture("structures_stacked_closest")
verify.center_tile(g.stack_col, g.stack_row, 10)
verify.frames(3)
verify.capture("structures_stacked_close")

-- Hit zones unchanged (PLANETARY.md § Building markers): a press on a
-- single-building tile lands on the building; on a stacked tile, on the tile.
verify.center_tile(g.single_col, g.single_row, 10)
verify.frames(2)
verify.click_tile(g.single_col, g.single_row)
verify.frames(2)
verify.expect(verify.pointer_target().selection_kind == "building",
              "a press on a single-building tile selects the building (hit zone kept)")
verify.center_tile(g.stack_col, g.stack_row, 10)
verify.frames(2)
verify.click_tile(g.stack_col, g.stack_row)
verify.frames(3)
verify.expect(verify.pointer_target().selection_kind == "tile",
              "a press on a stacked tile selects the tile (hit zone kept)")
-- The Selection band's neighbourhood view shows the baked ground.
verify.frames(3)
verify.capture("structures_neighbourhood")
verify.clear_selection()

-- The PRESS AREA (F34; SELECTION.md § Multi-building tiles): a single building
-- owns its whole hex, a stacked tile falls through to the tile — not just at
-- the centre but OFF-centre, on the tilted rung (zoom 10) where the press must
-- resolve in ground space as hover does, and on the flat play rung (zoom 5).
-- Presses at about +-0.6 drawn radius left/right and up/down (the vertical
-- offset rides the camera squash). Single and stacked presses ALTERNATE, so
-- no press repeats the last tile — a repeat press is the selection cycle
-- (Building -> Tile), not a first press.
local function press_at(col, row, dx, dy)
    local p = verify.tile_screen(col, row)   -- centres the tile; returns its screen point
    if not p.ok then return "unresolved" end
    local s = verify.ground_stats()
    local r = s.draw_r
    local sy = (s.sy and s.sy > 0) and s.sy or 1
    verify.clear_selection()
    verify.click(p.x + dx * r, p.y + dy * r * sy)
    verify.frames(2)
    return verify.pointer_target().selection_kind
end
local offsets = { { 0.6, 0 }, { -0.6, 0 }, { 0, 0.6 }, { 0, -0.6 } }
for _, z in ipairs({ 10, 5 }) do
    verify.center_tile(g.single_col, g.single_row, z)
    verify.frames(2)
    for _, o in ipairs(offsets) do
        local ks = press_at(g.single_col, g.single_row, o[1], o[2])
        local kt = press_at(g.stack_col, g.stack_row, o[1], o[2])
        print(string.format("PRESS zoom %d offset (%+.1f, %+.1f) r: single -> %s, stacked -> %s",
                            z, o[1], o[2], ks, kt))
        verify.expect(ks == "building", string.format(
            "zoom %d: an off-centre press (%+.1f, %+.1f) r on a single-building tile selects the building (got %s)",
            z, o[1], o[2], ks))
        verify.expect(kt == "tile", string.format(
            "zoom %d: an off-centre press (%+.1f, %+.1f) r on a stacked tile selects the tile (got %s)",
            z, o[1], o[2], kt))
    end
end
verify.clear_selection()

verify.set_border_band(true)
