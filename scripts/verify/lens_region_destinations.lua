-- Sprint 23 wave 3: the two NON-ENTITY selections reach a surface (BL-671, BL-660,
-- BL-659, closing NR-697).
--
-- A deposit and a plate are the two things the lens system can select that are not
-- entities. Both channels were WRITE-ONLY: the canvas set them and nothing read
-- them, so a press left the Selection band showing whatever was selected before it
-- and the ledger it opened was aimed at nothing. That is what NR-697 reported
-- against the plate; the deposit had the same gap and nobody had said so.
--
-- WHAT THIS CAN AND CANNOT ASSERT, stated because the gap is the interesting part.
-- pointer_target exposes the two channels and the open panel, so "the press set
-- the right subject and opened the right surface" is machine-checkable. Whether
-- the BAND then draws a plate card rather than the player's corporation is not:
-- the band's contents are pixels. The captures are that half, and they are a
-- human's job.
--
-- The plate rows need the Continent lens over ground the lens actually classifies,
-- and the deposit rows need a tile carrying the selected resource — which a script
-- cannot search for (NR-698, still open). Both therefore PROBE and report honestly
-- when the world hands them nothing, rather than passing on an empty run.

verify.goto_surface("home")
verify.econ_step(6)
verify.show_panel("economy", false)

local built_col, built_row, home_body = nil, nil, nil
for _, b in ipairs(verify.buildings()) do
    if b.player then built_col, built_row, home_body = b.x, b.y, b.body; break end
end
verify.expect(built_col ~= nil, "found a player building to frame on")

local function press(col, row)
    verify.center_tile(col, row, 8)
    verify.click_tile(col, row)
    return verify.pointer_target()
end

-- ---------------------------------------------------------------------------
-- The PLATE: Continent lens -> History ledger, Tectonics view, that plate named.
--
-- Every land tile belongs to a plate on a mobile-lid body, so the tile under the
-- player's own building is a valid probe and needs no search.
-- ---------------------------------------------------------------------------
verify.set_overlay("continent")
local plate = press(built_col, built_row)

verify.expect(plate.selected_plate >= 0,
              "a plate press selects a PLATE (its own channel, not selected_entity)")
verify.expect(plate.has_selection == false,
              "a plate press leaves selected_entity null - a plate is not an entity")
verify.expect(plate.open_panel == "tile",
              "a plate press opens the History ledger")
verify.capture("region_plate_selected")

-- The band is the half NR-697 was about, and it is a capture rather than an
-- assertion: what has to be true is that the card says PLATE, not that some field
-- changed. Framed alone so the card fills the frame.
verify.capture("region_plate_band")

-- ---------------------------------------------------------------------------
-- The DEPOSIT: Resource lens -> Market ledger, Prices view, that resource.
--
-- The probe has to FIND ground carrying the lens resource, and cannot ask for it
-- (NR-698). So it sweeps the tiles around the anchor under each of a few common
-- goods and takes the first press that actually resolves to a deposit.
-- ---------------------------------------------------------------------------
verify.set_overlay("resource")

local found_resource, found_col, found_row = nil, nil, nil
for _, good in ipairs({ "iron_ore", "stone", "timber", "clay", "coal" }) do
    verify.set_lens_resource(good)
    for radius = 0, 6 do
        for dc = -radius, radius do
            for dr = -radius, radius do
                local col, row = built_col + dc, built_row + dr
                verify.center_tile(col, row, 8)
                local pt = verify.tile_screen(col, row)
                if pt.ok and pt.y < 440 and pt.x > 70 and pt.x < 920 then
                    local t = press(col, row)
                    if t.selected_deposit_resource >= 0 then
                        found_resource, found_col, found_row = good, col, row
                        break
                    end
                end
            end
            if found_resource then break end
        end
        if found_resource then break end
    end
    if found_resource then break end
end

if found_resource ~= nil then
    verify.set_lens_resource(found_resource)
    local dep = press(found_col, found_row)
    verify.expect(dep.selected_deposit_resource >= 0,
                  "a deposit press selects a DEPOSIT (its own channel)")
    verify.expect(dep.has_selection == false,
                  "a deposit press leaves selected_entity null - a deposit is not an entity")
    verify.expect(dep.open_panel == "market",
                  "a deposit press opens the Market ledger")
    verify.capture("region_deposit_selected")
else
    -- Reported, not silently skipped: a check that finds nothing and says PASS is
    -- indistinguishable from one that found everything.
    verify.expect(false,
                  "NO TILE CARRYING ANY PROBED RESOURCE was found near the anchor - the "
                  .. "deposit destination is UNPROVEN, not passing. This is NR-698's gap: a "
                  .. "script cannot ask which tiles carry a good, so it guesses and reports.")
end

verify.set_overlay("none")
verify.clear_selection()
