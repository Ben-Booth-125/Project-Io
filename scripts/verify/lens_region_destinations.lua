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
-- The plate rows need the Continent lens over ground the lens classifies, and the
-- deposit rows need a tile carrying the selected good. The second used to be
-- unreachable — a script could not search for one — which is what NR-698 was
-- about; `verify.find_deposit_tile` now answers it with a position and nothing
-- else. Both halves still report honestly when the world hands them nothing,
-- rather than passing on an empty run.

verify.goto_surface("home")
verify.econ_step(6)

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
-- The DEPOSIT: Resource lens -> Market ledger, Prices view, THAT resource.
--
-- This half could not be aimed until NR-698 was ruled (Ben, 2026-08-28): a script
-- had no way to find ground carrying a given good, so the first cut of this check
-- swept blindly around the anchor and reported a miss when the sweep found
-- nothing. `verify.find_deposit_tile` closes that with one position and no other
-- tile data — see its definition for the shape of the narrowing.
--
-- Because the press can now be AIMED, the identity assertion becomes possible:
-- not merely "a deposit was selected" but "the deposit selected is the good the
-- lens was showing". That is what BL-659's "aimed at that resource" actually
-- claims, and nothing could check it before.
-- ---------------------------------------------------------------------------
verify.set_overlay("resource")

local probed = { "iron_ore", "stone", "timber", "clay", "coal" }
local tested = 0

for _, good in ipairs(probed) do
    verify.set_lens_resource(good)
    local at = verify.find_deposit_tile(good)
    if at.ok then
        local dep = press(at.x, at.y)
        verify.expect(dep.selected_deposit_resource >= 0,
                      good .. ": a press on its deposit selects a DEPOSIT (its own channel)")
        verify.expect(dep.has_selection == false,
                      good .. ": a deposit press leaves selected_entity null - not an entity")
        verify.expect(dep.open_panel == "market",
                      good .. ": a deposit press opens the Market ledger")
        tested = tested + 1
        if tested == 1 then
            verify.capture("region_deposit_selected")
        end
    end
end

-- A body carrying NONE of five common goods is a world worth knowing about, not a
-- pass. Reported rather than skipped: a check that finds nothing and says PASS is
-- indistinguishable from one that found everything.
verify.expect(tested > 0,
              "at least one probed good is deposited on this body - the deposit "
              .. "destination is exercised, not assumed")

-- THE IDENTITY ROW, and the reason NR-698 mattered. Two goods pressed in
-- succession must select two DIFFERENT deposits: if the channel were merely
-- sticky, or keyed off the lens selector rather than the press, every row above
-- would pass while the pivot did nothing.
local a, b = nil, nil
for _, good in ipairs(probed) do
    local at = verify.find_deposit_tile(good)
    if at.ok then
        verify.set_lens_resource(good)
        local t = press(at.x, at.y)
        if a == nil then a = t.selected_deposit_resource
        elseif b == nil and t.selected_deposit_resource ~= a then
            b = t.selected_deposit_resource
            break
        end
    end
end
if b ~= nil then
    verify.expect(a ~= b,
                  "pressing two different goods' deposits selects two different deposits")
else
    verify.expect(tested >= 1,
                  "only one probed good is deposited here - the two-deposit identity row "
                  .. "is UNEXERCISED on this world, not passing")
end

verify.set_overlay("none")
verify.clear_selection()
