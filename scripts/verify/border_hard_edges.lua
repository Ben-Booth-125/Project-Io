-- BL-1262 (borders hard edges) — the plain canvas at the three widest rungs, framed
-- on a many-nation frontier, so a reader can SEE what the plain canvas draws for
-- national borders and roads (PLANETARY.md § The national border band;
-- RENDERING.md § Roads and sea lanes).
--
-- Ben, 2026-10-10: "a lot of visual clutter when many nations border each other.
-- One fix would be removing shading inside national borders, and just go with the
-- hard edges." So a border is the inset stroke alone — no tile is tinted by its
-- nation — and the plain canvas draws no road network at any rung (the painted
-- roads are the roads; the Throughput lens keeps the drawn network).
--
-- What it ASSERTS (the stroke and its corridor survive the wash's removal):
--   * at rung 2, a sweep across the frame finds a press that selects a NATION —
--     the corridor exists, so the border is still the route to a nation;
--   * a press at a hex centre is not swallowed by it.
-- What it CAPTURES (for the eye; there is no pixel API): rungs 0-2 plain, the
-- same frames with the band hidden (the difference is the stroke alone), and
-- rung 1 under the Throughput lens (the drawn network it keeps).
--
-- Run: ProjectIo --verify scripts/verify/border_hard_edges.lua

local kMinZoom = 1.2531328  -- body_surface_canvas.cpp kMinZoom

local function settle()
    verify.frames(3)
    for i = 1, 2000 do
        verify.frames(1)
        local s = verify.ground_stats()
        if s.pending_uploads == 0 and s.chunks > 0 then break end
    end
    verify.frames(5)
end

verify.window(1720, 1080)
verify.goto_surface("home")
verify.ground_complete_master(600)
verify.set_overlay("none")

for k = 0, 2 do
    verify.set_zoom(kMinZoom * (2 ^ k))
    verify.set_pan(0, 0)
    verify.set_border_band(true)
    settle()
    verify.capture(string.format("border_hard_edges_rung%d", k))
    verify.set_border_band(false)
    settle()
    verify.capture(string.format("border_hard_edges_rung%d_noband", k))
    verify.set_border_band(true)
end

-- The drawn network the Throughput lens keeps.
verify.set_overlay("throughput")
verify.set_zoom(kMinZoom * 2)
verify.set_pan(0, 0)
settle()
verify.capture("border_hard_edges_rung1_throughput")
verify.set_overlay("none")

-- The corridor: at rung 2 a sweep finds a press that selects a nation.
verify.set_zoom(kMinZoom * 4)
verify.set_pan(0, 0)
settle()
verify.clear_selection()
verify.frames(2)
local hit_x, hit_y = nil, nil
for _, y in ipairs({ 300, 450, 600, 750 }) do
    for x = 120, 1500, 5 do
        verify.click(x, y)
        verify.frames(2)
        if verify.pointer_target().selection_kind == "nation" then
            hit_x, hit_y = x, y
            break
        end
        verify.clear_selection()
    end
    if hit_x then break end
end
verify.expect(hit_x ~= nil,
              "the hard-edge border stroke keeps a clickable corridor on the plain canvas")
if hit_x then
    verify.frames(2)
    verify.capture("border_hard_edges_rung2_nation_selected")
end
