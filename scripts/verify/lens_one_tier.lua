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
verify.show_panel("economy", false)

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

local structure_lenses = { "corporation", "company", "resource", "market",
                           "scarcity", "continent" }

for _, lens in ipairs(structure_lenses) do
    verify.set_overlay(lens)
    local t = press(built_col, built_row)
    verify.expect(t.selection_kind ~= "building",
                  lens .. " lens: a press on a BUILT tile does not resolve to the building")
end

-- The positive half, where the subject has a name. The player's own building sits
-- on the player's own ground, so the Corporation lens must answer with a
-- corporation — the case that could never fire before BL-664, because the
-- building's marker covered the whole hex.
verify.set_overlay("corporation")
local corp_press = press(built_col, built_row)
verify.expect(corp_press.selection_kind == "corporation",
              "Corporation lens: a press on an owned BUILT tile resolves to the corporation")
verify.capture("one_tier_corp_press")

-- ---------------------------------------------------------------------------
-- BL-664 R2 — a lens with no structure grain surfaces nothing at all.
--
-- Population, Industry and Throughput draw a per-tile value field rather than a
-- region, so there is nothing for a selection to be OF. Both halves are checked
-- on BUILT ground, which is the harder case: before the rule, the building's
-- marker answered here under every lens.
-- ---------------------------------------------------------------------------

for _, lens in ipairs({ "population", "industry", "throughput" }) do
    verify.set_overlay(lens)

    local h = dwell(built_col, built_row)
    verify.expect(h.hover_card == false,
                  lens .. " lens: no hover card is raised, even over a building")
    verify.expect(h.hovered_structure_kind == "none",
                  lens .. " lens: the pointer resolves to no structure")

    -- Seed a real selection under a lens that HAS one, so the press below is
    -- proving the band cleared rather than finding it already empty.
    verify.set_overlay("corporation")
    press(built_col, built_row)
    verify.set_overlay(lens)

    local t = press(built_col, built_row)
    verify.expect(t.has_selection == false,
                  lens .. " lens: a press selects nothing and clears the band to resting")
end

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

verify.set_overlay("corporation")
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
-- BL-664 R4 / BL-665 R1, R2 — the owner's tile group, and unowned ground.
-- ---------------------------------------------------------------------------

verify.set_overlay("corporation")
local owned_hover = dwell(built_col, built_row)
verify.expect(owned_hover.hovered_structure_kind == "corporation",
              "Corporation lens: hovering owned ground lights a corporation group")
verify.capture("one_tier_corp_group_hover")

local bare_hover = dwell(bare_col, bare_row)
verify.expect(bare_hover.hovered_structure_kind == "none",
              "Corporation lens: ground held by nobody lights nothing")

-- R4: and pressing that same nobody's ground clears the band, rather than
-- falling through to the tile the way a no-lens press would.
press(built_col, built_row)
local cleared = press(bare_col, bare_row)
verify.expect(cleared.has_selection == false,
              "Corporation lens: a press on unowned ground clears the band to resting")
verify.capture("one_tier_inert_press_cleared")

-- The Company lens is the exact mirror, and the pair is what proves the split is
-- real: a corporation's tiles are not a company's group. The player's own ground
-- carries a corporation, so under the Company lens it must answer NOTHING.
verify.set_overlay("company")
local company_on_corp = dwell(built_col, built_row)
verify.expect(company_on_corp.hovered_structure_kind ~= "corporation",
              "Company lens: a corporation's ground is not a company group")

-- ---------------------------------------------------------------------------
-- BL-666 — the two destinations, which are different surfaces.
-- ---------------------------------------------------------------------------

verify.set_overlay("corporation")
local corp_dest = press(built_col, built_row)
-- "corporations" (the all-corporations table), NOT "corporation" (the player's
-- own dashboard). The two flags are named the wrong way round in ui_state, this
-- assertion is the thing that catches it, and the first cut of this check could
-- not tell them apart — it asserted "corporation" and passed while the press was
-- opening the player's own books.
verify.expect(corp_dest.open_panel == "corporations",
              "a corporation press opens the corporations TABLE, not the player's own dashboard")
verify.capture("one_tier_corp_destination")

-- A background firm's ground, if this world has one on this body. Companies are
-- generated content rather than a guarantee, so the row is CONDITIONAL — and it
-- says so, because a check that silently skips reads exactly like a check that
-- passed.
verify.set_overlay("company")
local company_col, company_row = nil, nil
for _, b in ipairs(verify.buildings()) do
    if not b.player and b.body == home_body then
        verify.center_tile(b.x, b.y, 8)
        local h = dwell(b.x, b.y)
        if h.hovered_structure_kind == "company" then
            company_col, company_row = b.x, b.y
            break
        end
    end
end

if company_col ~= nil then
    local company_dest = press(company_col, company_row)
    verify.expect(company_dest.open_panel == "company",
                  "a company press opens the company surface, not the corporations table")
    verify.capture("one_tier_company_destination")
else
    verify.log_buildings()
    verify.expect(false,
                  "NO BACKGROUND FIRM FOUND ON THIS BODY - the company destination is unproven, "
                  .. "not passing. Re-aim the probe or say why this world has none.")
end

verify.set_overlay("none")
verify.clear_selection()
verify.capture("one_tier_cleared")
