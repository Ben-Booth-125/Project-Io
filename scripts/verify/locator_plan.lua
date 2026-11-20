-- The locator's buildings, seen from above (Ben, 2026-11-20; LAYOUT.md § The locator).
--
-- The canvas stands buildings as 3D geometry, and the locator's flat neighbourhood
-- page holds only their ground pads, so the locator draws each standing part's plan
-- over it. This check selects ground the player has built on, lets the page bake,
-- and captures the whole screen: the bottom-left locator should show roofs, tanks and
-- stacks, not bare circles.
--
-- Run with: ProjectIo --verify scripts/verify/locator_plan.lua

verify.econ_step(2)
verify.goto_surface("home")
for _, p in ipairs({ "construction", "tile", "market", "balance", "corporation", "build" }) do
    verify.show_panel(p, false)
end
verify.frames(2)

-- The first complete player building, in the API's own (deterministic) order.
local pick = nil
for _, b in ipairs(verify.buildings()) do
    if b.player and b.complete then pick = b; break end
end
if pick == nil then
    error("locator_plan.lua: the player stands no complete building")
end

verify.center_tile(pick.x, pick.y)
verify.select_tile(pick.x, pick.y)
verify.frames(30) -- the neighbourhood page bakes off the main thread
verify.capture("locator_plan")
