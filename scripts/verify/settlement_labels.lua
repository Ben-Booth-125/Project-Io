-- Visual verification for settlement labels (BL-363).
-- Confirms requirements.json § misc-hygiene-sweep R3: City+ conurbation labels
-- come from the SEEDED naming system (world::population_centre_name, the BL-290
-- tongue/phoneme banks) rather than the hard-coded Earth-flavoured name bank the
-- canvas used to carry — "Oberon" is a real moon, which the standing no-Earth-
-- names rule forbids for any generated name.
--
-- It frames the largest population centre on the home body rather than a fixed
-- tile, so the check survives world-content drift: pop_markers.lua hard-codes
-- (66, 6), which the BL-257 / BL-348 generation changes left pointing at empty
-- terrain. Run with:
--   ProjectIo --verify scripts/verify/settlement_labels.lua

verify.econ_step(2)                 -- settle
verify.show_panel("economy", false) -- clear the auto-opened panel off the capture
verify.goto_surface("home")
verify.set_overlay("none")

-- Find the biggest centre on the home body: labels only draw at City+ scale, so
-- framing the largest is the surest way to put one on the capture.
local best = nil
for _, c in ipairs(verify.population_centres()) do
    if best == nil or c.scale > best.scale then best = c end
end

if best == nil then
    error("settlement_labels: no population centres in the generated world")
end

print(string.format("settlement_labels: framing centre scale=%d at (%d, %d)",
                    best.scale, best.x, best.y))

frame_tile(best.x, best.y, 9)
verify.capture("settlement_labels_city")

-- Wider: several conurbations at once, so a label collision or an unlabelled
-- City+ cluster (the one behaviour change BL-363 introduced — a centre with no
-- persisted name now draws nothing) is visible rather than cropped out.
frame_tile(best.x, best.y, 5)
verify.capture("settlement_labels_region")
