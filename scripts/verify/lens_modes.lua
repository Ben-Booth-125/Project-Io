-- Demo capture: each map-lens overlay over the Planetary canvas (Kepler).
verify.goto_surface("home")

verify.set_overlay("corporation")
verify.capture("lens_corporation")

-- The Company lens (2026-08-28) is the Corporation lens's mirror: background
-- firms only, drawn identically. Captured next to it deliberately — the pair is
-- only right if the two pictures are DISJOINT, which one capture cannot show.
verify.set_overlay("company")
verify.capture("lens_company")

verify.set_overlay("scarcity")
verify.capture("lens_scarcity")

verify.set_overlay("population")
verify.capture("lens_population")

verify.set_overlay("market")
verify.capture("lens_market")

verify.set_overlay("supply")
verify.capture("lens_supply")

-- Off-strip lenses (BL-011/BL-014) — selectable by name since the lens-cycle
-- count fix; both read the player's trade_routes for the active body.
verify.set_overlay("reach")
verify.capture("lens_reach")

verify.set_overlay("supply_routes")
verify.capture("lens_supply_routes")

verify.set_overlay("resource")
verify.set_lens_resource("iron_ore")
verify.capture("lens_resource_iron_ore")

-- The lens chrome region (BL-602): one home in the minimap header, top right, for
-- the selector and whichever key the active lens draws. The count-driven keys are a
-- DROPDOWN collapsed by default, and their header bar sits on the minimap's top edge
-- so the toggle does not travel when the body opens upward. Press it and capture both
-- states -- the press is the thing a capture alone cannot prove.
verify.window(1280, 720)
verify.set_overlay("corporation")
verify.capture("lens_chrome_corporation_collapsed")

-- The header bar is the full width of the right chrome column, its foot on the
-- minimap's top edge. Aim at the middle of that bar rather than at the caret glyph.
local mini = { w = 336, h = 260 }   -- minimap_rect at 1280x720; see shell_metrics.cpp
verify.click(1280 - mini.w * 0.5, 720 - mini.h - 10)
verify.frames(2)
verify.capture("lens_chrome_corporation_expanded")

-- Toggle rule: a second press on the same spot closes it again.
verify.click(1280 - mini.w * 0.5, 720 - mini.h - 10)
verify.frames(2)
verify.capture("lens_chrome_corporation_reclosed")

-- ---------------------------------------------------------------------------
-- BL-1250: every lens WASHES the baked ground (LENSES.md § A lens washes the
-- rendered ground). Each lens over the densest built region on the home body, at
-- a mid rung (rung 2, ~27 px drawn) and the close rung 3 (~55 px), once the master
-- is baked and every visible chunk uploaded — so the structures, settlements,
-- forms and rivers under the wash are in the picture. Plain first, for the eye to
-- compare against. Captures: lens_wash_<lens>_r<k>.
local kMinZoom = 1.2531328  -- body_surface_canvas.cpp kMinZoom
local function poll(pred, limit)
    for i = 1, limit or 20000 do
        verify.frames(1)
        if pred(verify.ground_stats()) then return true end
    end
    return false
end
verify.window(1720, 1080)
verify.goto_surface("home")
-- The master, until it is whole or stops advancing (2026-10-09: measured stalling
-- at 1960/2975 chunks on the home body; the framing below waits on its own chunks).
do
    local last, still = -1, 0
    for i = 1, 20000 do
        verify.frames(1)
        local s = verify.ground_stats()
        if s.master_total > 0 and s.master_ready >= s.master_total then break end
        if s.master_ready == last then still = still + 1 else still, last = 0, s.master_ready end
        if still >= 120 then
            print(string.format("[lens_modes] master stalled at %d/%d", s.master_ready, s.master_total))
            break
        end
    end
end

-- The densest built spot: the home-body building with the most others within 5 tiles.
local home = nil
local all = verify.buildings()
for _, b in ipairs(all) do if b.player then home = b.body break end end
if home == nil and #all > 0 then home = all[1].body end
local best, best_n = nil, -1
for _, b in ipairs(all) do
    if b.body == home then
        local n = 0
        for _, o in ipairs(all) do
            if o.body == home and math.abs(o.x - b.x) <= 5 and math.abs(o.y - b.y) <= 5 then n = n + 1 end
        end
        if n > best_n then best, best_n = b, n end
    end
end
verify.expect(best ~= nil, "a built region on the home body to frame")
print(string.format("[lens_modes] wash framing (%d,%d), %d buildings within 5 tiles",
                    best and best.x or -1, best and best.y or -1, best_n))

local wash_lenses = { "none", "corporation", "company", "market", "continent", "resource",
                      "population", "scarcity", "industry", "throughput", "supply",
                      "reach", "supply_routes", "trade_flow" }
verify.set_lens_resource("iron_ore")
verify.toggle_lens_resource("coal")     -- a split tile shows its wedges as washes
for _, k in ipairs({ 2, 3 }) do
    for _, lens in ipairs(wash_lenses) do
        verify.set_overlay(lens)
        if best then verify.center_tile(best.x, best.y, kMinZoom * (2 ^ k)) end
        verify.frames(3)
        poll(function(s) return s.pending_uploads == 0 and s.chunks > 0 end, 2000)
        verify.frames(2)
        verify.capture(string.format("lens_wash_%s_r%d", lens, k))
    end
end

verify.set_overlay("none")
