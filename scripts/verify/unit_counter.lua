-- Verify the land unit counter and the unit card (BL-1425 R1-R3).
--
-- The counter: one rectangle per (tile, owner) on the Planetary canvas — owner
-- band, the number of units on the tile, an HP bar over a readiness bar (the
-- player's own units only). The card: HP, readiness and morale bars, and the
-- land class the unit fights as.
--
-- Staging: no verify call wounds a unit, so the player's counter is captured at
-- full HP and readiness; a fight's bars are a live check. Then a rival's counter,
-- if one is on the surface. Nothing here hard-codes a coordinate: the units are
-- read from verify.units().

verify.goto_surface("home")

local mine, theirs = nil, nil
for _, u in ipairs(verify.units()) do
    if u.col ~= nil then
        if u.player then
            if mine == nil then mine = u end
        elseif theirs == nil then
            theirs = u
        end
    end
end
assert(mine, "the player has no unit on the surface (every nation seeds a garrison)")

-- Close on the player's unit: the counter needs a hex large enough to print its count.
verify.center_tile(mine.col, mine.row, 10.0)
verify.clear_selection()
verify.capture("unit_counter_own")

-- The card, Strength page: HP, readiness and morale bars.
assert(verify.select_unit(mine.id), "select_unit refused the player's own unit")
verify.capture("unit_counter_card")

-- A rival's counter: owner and count, no bars (R3).
if theirs ~= nil then
    verify.center_tile(theirs.col, theirs.row, 10.0)
    verify.clear_selection()
    verify.capture("unit_counter_rival")
else
    print("unit_counter: no rival unit on the surface; R3's capture not taken")
end
