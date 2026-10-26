-- A lens collapses selection to ONE TIER (BL-664/BL-665/BL-666).
--
-- Ben, 2026-08-28: "when lenses are active, selection is only one tier, as
-- opposed to the default lens, which may cycle through elements with multiple
-- clicks", then "Markers do not outrank lenses" and "for lenses which return
-- none, just don't surface a hover, and clicks will do nothing".
--
-- ASSERTED, not capture-only, and that is the point of the check. The batch-1
-- lens sweep (lens_selection_paths) framed each lens and saved a PNG; a human
-- eye was the whole test. What this rule needs instead is a claim that fails by
-- itself, because every one of its three parts is about what a press RESOLVES
-- TO, and that is invisible in a frame unless you already know what you expected.
--
-- The press target throughout is a BUILT tile — a tile carrying a marker. That
-- is deliberate and it is the sharp edge of rule 2: before BL-664 a marker won
-- every one of these presses, so "selection_kind ~= building" is the single
-- assertion that separates the new rule from the old one. lens_selection_paths
-- had to hunt for marker-FREE ground for exactly this reason; this check aims at
-- the marker on purpose.
--
-- What is NOT asserted here, and why: which deposit or which plate a press
-- resolves to. A script cannot find a tile carrying resource X without a query
-- that does not exist (NR-698, still open), so the resource rows below assert
-- the marker rule and the inert rule, never the deposit's identity.

verify.goto_surface("home")
verify.econ_step(6)

-- A tile the PLAYER has built on: guaranteed to carry a marker, and guaranteed
-- to be owned, so it exercises the corporation group and the marker rule at once.
-- Walked from the buildings table rather than hard-coded, so a generation change
-- cannot quietly move the world out from under the check (the pop_markers lesson).
-- verify.buildings() spans EVERY body and reports per-body grid coordinates, so
-- the body must be carried and matched. Without it a probe can take a building's
-- (x, y) from another body and dwell on an unrelated tile here, which reads as
-- "the feature is broken" and is actually "the probe is aimed off-world".
local built_col, built_row, home_body = nil, nil, nil
for _, b in ipairs(verify.buildings()) do
    if b.player then built_col, built_row, home_body = b.x, b.y, b.body; break end
end
verify.expect(built_col ~= nil, "found a player building to press")

local occupied = {}
for _, b in ipairs(verify.buildings()) do
    if b.body == home_body then
        occupied[tostring(b.x) .. "," .. tostring(b.y)] = true
    end
end

-- Ground nobody has built on, near by: the inert case for the owner lenses, and
-- the "lens has no answer here" case rule 3 is about.
verify.center_tile(built_col, built_row, 8)
local bare_col, bare_row = nil, nil
for radius = 2, 8 do
    for dc = -radius, radius do
        for dr = -radius, radius do
            local col, row = built_col + dc, built_row + dr
            if not occupied[tostring(col) .. "," .. tostring(row)] then
                local pt = verify.tile_screen(col, row)
                if pt.ok and pt.y < 440 and pt.x > 70 and pt.x < 920 then
                    bare_col, bare_row = col, row
                    break
                end
            end
        end
        if bare_col then break end
    end
    if bare_col then break end
end
verify.expect(bare_col ~= nil, "found unbuilt ground to press")

local function press(col, row)
    verify.center_tile(col, row, 8)
    verify.click_tile(col, row)
    return verify.pointer_target()
end

local function dwell(col, row)
    verify.center_tile(col, row, 8)
    local pt = verify.tile_screen(col, row)
    verify.expect(pt.ok, "tile has a screen position to dwell on")
    verify.mouse(pt.x, pt.y)
    -- Past kHoverAppearDelaySec (0.5 s) so the glance card has had its chance to
    -- come up. Asserting "no card" before the delay would pass for the wrong reason.
    verify.frames(40)
    return verify.pointer_target()
end

-- ---------------------------------------------------------------------------
-- BL-664 R1 — a marker does not outrank a lens.
--
-- One row per lens that resolves a structure. The claim is the same each time
-- and it is a NEGATIVE one: pressing a tile with a building on it must not give
-- the building. What it gives instead varies by lens, so the positive half is
-- asserted only where the subject is an entity the API can name.
-- ---------------------------------------------------------------------------

-- BL-1406: the Corporation and Company lenses retired with the firms; the owner
-- grain is the Nations lens's ("whose ground is this").
local structure_lenses = { "nations", "resource", "market", "scarcity", "continent" }

for _, lens in ipairs(structure_lenses) do
    verify.set_overlay(lens)
    local t = press(built_col, built_row)
    verify.expect(t.selection_kind ~= "building",
                  lens .. " lens: a press on a BUILT tile does not resolve to the building")
end

-- The positive half, where the subject has a name. The player's own building sits
-- on the player's own ground, so the Nations lens must answer with the NATION -
-- the building's marker covers the whole hex, and still does not outrank the lens.
verify.set_overlay("nations")
local nation_press = press(built_col, built_row)
verify.expect(nation_press.selection_kind == "nation",
              "Nations lens: a press on an owned BUILT tile resolves to the nation (got " ..
              tostring(nation_press.selection_kind) .. ")")
verify.capture("one_tier_nation_press")

-- ---------------------------------------------------------------------------
-- BL-664 R2 — a lens with no structure grain surfaces nothing at all.
--
-- Population, Industry and Throughput draw a per-tile value field rather than a
-- region, so there is nothing for a selection to be OF. Both halves are checked
-- on BUILT ground, which is the harder case: before the rule, the building's
-- marker answered here under every lens.
-- ---------------------------------------------------------------------------

-- THROUGHPUT LEFT THIS LIST for its hover half (Ben, 2026-10-20, BL-1353 option A):
-- it resolves its own nation's ground to "ground" and raises a card, and its press
-- is still inert - both asserted in the block after this loop.
for _, lens in ipairs({ "population", "industry" }) do
    verify.set_overlay(lens)

    local h = dwell(built_col, built_row)
    verify.expect(h.hover_card == false,
                  lens .. " lens: no hover card is raised, even over a building")
    verify.expect(h.hovered_structure_kind == "none",
                  lens .. " lens: the pointer resolves to no structure")

    -- Seed a real selection under a lens that HAS one, so the press below is
    -- proving the band cleared rather than finding it already empty.
    verify.set_overlay("nations")
    press(built_col, built_row)
    verify.set_overlay(lens)

    local t = press(built_col, built_row)
    verify.expect(t.has_selection == false,
                  lens .. " lens: a press selects nothing and clears the band to resting")
end

-- BL-1353: Throughput is a HOVER READ. The built tile is the player's, on its home
-- nation's ground, so the field covers it and the pointer resolves to the ground.
-- A PRESS SELECTS THE TILE (Ben, 2026-10-26, overturning 2026-10-20's inert press):
-- it used to clear the band to resting, which shows the player's own nation.
verify.set_overlay("logistics")
local tp = dwell(built_col, built_row)
verify.expect(tp.hovered_structure_kind == "ground",
              "throughput lens: the player nation's ground resolves to the ground")
verify.expect(tp.hover_card == true,
              "throughput lens: the ground raises a hover card")
verify.capture("one_tier_throughput_ground_card")
verify.set_overlay("nations")
press(built_col, built_row)
verify.set_overlay("logistics")
local tpp = press(built_col, built_row)
verify.expect(tpp.has_selection == true and tpp.selection_kind == "tile",
              "logistics lens: a press on the ground selects the TILE (" .. tostring(tpp.selection_kind) .. ")")
local tpp2 = press(built_col, built_row)
verify.expect(tpp2.selection_kind == "tile",
              "logistics lens: a repeat press re-selects the tile - no cycle under a lens")

verify.set_overlay("population")
dwell(built_col, built_row)
verify.capture("one_tier_inert_population")

-- ---------------------------------------------------------------------------
-- BL-664 R3 — the repeat-click cycle is a NO-LENS gesture.
--
-- Under a lens the same ground pressed twice gives the same answer, because
-- there is one tier and it does not move. Under no lens the four-rung cycle is
-- untouched and a second press on a built tile advances off the building.
-- ---------------------------------------------------------------------------

verify.set_overlay("nations")
local first  = press(built_col, built_row)
local second = press(built_col, built_row)
verify.expect(first.selection_kind == second.selection_kind,
              "under a lens a repeat press does not advance a cycle")

verify.set_overlay("none")
verify.clear_selection()
local n1 = press(built_col, built_row)
local n2 = press(built_col, built_row)
verify.expect(n1.selection_kind == "building",
              "no lens: the first press on a built tile still gives the building")
verify.expect(n2.selection_kind ~= n1.selection_kind,
              "no lens: the repeat-click cycle still advances off the building")

-- ---------------------------------------------------------------------------
-- BL-664 R4 — the owner's ground, under the Nations lens (BL-1406: the
-- Corporation / Company tile-group rows retired with those lenses, and with
-- them BL-665's corporation/company split and BL-666's two destinations).
-- ---------------------------------------------------------------------------

verify.set_overlay("nations")
local owned_hover = dwell(built_col, built_row)
verify.expect(owned_hover.hovered_structure_kind == "nation",
              "Nations lens: hovering the player's own ground lights its nation (got " ..
              tostring(owned_hover.hovered_structure_kind) .. ")")
verify.capture("one_tier_nation_hover")
-- A retired owner-lens name arms nothing at all.
verify.set_overlay("company")
verify.frames(1)
verify.expect(verify.overlay_name() == "none", "the retired Company lens name arms nothing")


verify.set_overlay("none")
verify.clear_selection()
verify.capture("one_tier_cleared")
