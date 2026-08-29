-- Visual check for the building-management surfaces: the per-building pages that
-- carry production method, profitability and the workforce dial.
--
-- REWRITTEN 2026-08-16 (NR-246). This script used to open the construction
-- panel's Buildings tab via verify.construction_view(1). That tab was deleted by
-- the BL-431 follow-on and the construction panel is queue-only now (view 0), so
-- the call still ran but set an int nobody dispatches on — the capture no longer
-- showed what the script's own comment claimed it verified. The management shell
-- lives on the building Selection card's pager, reached by selecting a building
-- and folding to `building_metric`.
--
-- Run: ProjectIo --verify scripts/verify/building_management_shell.lua
verify.econ_step(6)                       -- populate run states + market prices
verify.goto_surface("home")               -- Kepler: player buildings visible

-- Prefer a PROCESSING building: the Method page is the one surface here that has
-- nothing to show on an extraction site, and it is the page BL-430/BL-431 built.
-- Falls back to any player building so the check still captures on a world whose
-- player corp happens to own no processor.
local player, processor
for _, b in ipairs(verify.buildings()) do
    if b.player then
        player = player or b
        -- b.type is the DISPLAY name (ui::building_type_name), not the enum id.
        if b.type == "Processing Facility" then processor = b; break end
    end
end
local target = processor or player
assert(target, "building_management_shell.lua: no player building on the home body")
verify.select_building(target.x, target.y)

-- One capture per page, so a layout regression on any single page is visible
-- rather than averaged away. Page order per building_pages() as of the
-- 2026-08-15 playtest rework: Profitability / Method / Workforce.
--
-- verify.building_page (not verify.fold) is what moves the pager: fold's second
-- argument is the drill KEY, so folding alone left every capture on page 1.
verify.building_page(0)
verify.capture("building_management_profitability")

verify.building_page(1)
verify.capture("building_management_method")

verify.building_page(2)
verify.capture("building_management_workforce")
