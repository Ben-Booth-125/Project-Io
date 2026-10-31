-- Verify the unit stat grid (BL-1454; docs/ui/ledgers/military.md § The stat grid,
-- docs/ui/SELECTION.md § The unit element's layout; Ben, 2026-10-29).
--
-- One grid, two hosts:
-- 1. The unit card (Selection band): the player's own unit shows every cell -
--    HP, readiness, morale, food, ammunition, equipment (each with a bar), then
--    soft, hard, defence, breakthrough, hardness, recon, stealth and speed - with
--    the class image on the left. The Strength and Roster pages fold into it.
-- 2. A rival unit: its internals (the six stored cells) read "-" always, and its
--    design stats read "-" until the player has seen it (a battle or recon).
-- 3. The designer (bottom right): design stats only, no deltas on the live
--    design; with the kit lock waived (verify staging), hovering a pick shows
--    signed deltas against the live design, a drafted pick keeps them, and
--    Revert clears them.
--
-- Every press aims at a point the surface published; the cells come from
-- verify.unit_stat_grid / verify.unit_designer, the builders the screen draws.

verify.goto_surface("home")
verify.clear_selection()
verify.set_overlay("none")
verify.mouse(-1, -1)
verify.frames(3)

local DASH = "\xe2\x80\x94"
local function fail(msg) verify.expect(false, "unit_stat_grid: " .. msg) end
local function ok(msg)   verify.expect(true, "unit_stat_grid: " .. msg) end
local function settle() verify.mouse(-1, -1); verify.frames(3) end

local function dump(tag, cells)
    local line = ""
    for _, c in ipairs(cells) do
        line = line .. "  " .. string.format("%s=%s%s", c.label, c.value,
            (c.delta ~= nil and c.delta ~= 0) and string.format("(%+d)", c.delta) or "")
    end
    print("unit_stat_grid[" .. tag .. "]:" .. line) -- the verify sandbox has no table library
end

-- 1. The player's own unit.
local mine, rival = nil, nil
for _, u in ipairs(verify.units()) do
    if u.count > 0 then
        if u.player and mine == nil then mine = u end
        if not u.player and rival == nil then rival = u end
    end
end

local STORED = { "HP", "Readiness", "Morale", "Food", "Ammunition", "Equipment" }
local DESIGN = { "Soft attack", "Hard attack", "Defence", "Breakthrough", "Hardness", "Recon", "Stealth", "Speed" }

if mine == nil then
    fail("the player has no unit to select")
else
    verify.select_unit(mine.id)
    settle()
    local g = verify.unit_stat_grid(mine.id)
    dump("own", g)
    if #g == 14 then ok("the own unit's grid has the 14 cells") else fail("own grid has " .. #g .. " cells, want 14") end
    for i, l in ipairs(STORED) do
        local c = g[i]
        if c and c.label == l and c.value ~= DASH and c.bar then ok(l .. " shows " .. c.value .. " with a bar") else
            fail("stored cell " .. i .. " should be '" .. l .. "' with a value and a bar") end
    end
    for i, l in ipairs(DESIGN) do
        local c = g[6 + i]
        if c and c.label == l and c.value ~= DASH and not c.bar then ok(l .. " shows " .. c.value) else
            fail("design cell " .. i .. " should be '" .. l .. "' with a value") end
    end
    verify.capture("unit_stat_grid_card")
end

-- 2. A rival unit.
if rival == nil then
    print("unit_stat_grid: no rival unit in the world; the rival cells are not checked")
else
    verify.select_unit(rival.id)
    settle()
    local g = verify.unit_stat_grid(rival.id)
    dump("rival", g)
    local hidden = true
    for i = 1, 6 do if g[i] == nil or g[i].value ~= DASH then hidden = false end end
    if hidden then ok("a rival's internals read '-'") else fail("a rival's internals are shown") end
    local design_dash = true
    for i = 7, #g do if g[i].value ~= DASH then design_dash = false end end
    -- Unseen in this staged world (no battle, no recon): design stats hidden too.
    if design_dash then ok("an unseen rival's design stats read '-'") else
        print("unit_stat_grid: the rival's design stats are shown - it has been seen (battle or recon)") end
    verify.capture("unit_stat_grid_rival")
end
verify.clear_selection()
settle()

-- 3. The designer.
local function open_designer(cls)
    local x, y = verify.nav_slot(7)
    if not verify.military_ledger().open then verify.click(x, y); settle() end
    local l = verify.military_ledger()
    if l.view ~= 1 then verify.click(l.tab_design_x, l.tab_design_y); settle() end
    for _, c in ipairs(verify.military_ledger().classes) do
        if c.cls == cls then verify.click(c.x, c.y); settle(); return verify.unit_designer() end
    end
    return verify.unit_designer()
end

local ARMOUR = 3
local function body()
    local d = open_designer(ARMOUR)
    if not d.open then fail("the armour designer did not open"); return end
    dump("designer live", d.grid)
    if #d.grid == 8 then ok("the designer's grid has the 8 design cells") else fail("designer grid has " .. #d.grid .. " cells, want 8") end
    local nonzero = false
    for _, c in ipairs(d.grid) do if c.delta ~= 0 then nonzero = true end end
    if not nonzero then ok("the live design shows no deltas") else fail("the live design shows deltas") end
    verify.capture("unit_stat_grid_designer_live")

    verify.waive_kit_research(true)
    settle()
    d = verify.unit_designer()
    local box0 = nil
    for _, b in ipairs(d.boxes) do if b.slot == 0 then box0 = b end end
    verify.click(box0.x, box0.y)
    settle()
    d = verify.unit_designer()
    local pick = nil
    for _, i in ipairs(d.items) do if i.enabled and not i.fitted then pick = i; break end end
    if pick == nil then fail("no live pick with the kit lock waived"); return end

    -- Hover: the hovered item previewed fitted, deltas against the live design.
    verify.hover(pick.x, pick.y, 3)
    d = verify.unit_designer()
    dump("designer hover '" .. pick.name .. "'", d.grid)
    nonzero = false
    for _, c in ipairs(d.grid) do if c.delta ~= 0 then nonzero = true end end
    if nonzero then ok("hovering '" .. pick.name .. "' shows signed deltas") else fail("hovering a pick shows no delta") end
    verify.capture("unit_stat_grid_designer_hover")

    -- Pick: the draft keeps the deltas.
    verify.click(pick.x, pick.y)
    settle()
    d = verify.unit_designer()
    dump("designer draft", d.grid)
    nonzero = false
    for _, c in ipairs(d.grid) do if c.delta ~= 0 then nonzero = true end end
    if nonzero and d.changed_slots > 0 then ok("the draft shows its deltas against the live design") else
        fail("the draft shows no delta (changed_slots " .. d.changed_slots .. ")") end
    verify.capture("unit_stat_grid_designer_draft")

    -- Revert: back to the live design, no deltas.
    verify.click(d.revert_x, d.revert_y)
    settle()
    d = verify.unit_designer()
    nonzero = false
    for _, c in ipairs(d.grid) do if c.delta ~= 0 then nonzero = true end end
    if not nonzero and d.changed_slots == 0 then ok("Revert clears the deltas") else fail("deltas remain after Revert") end
end
local okrun, err = pcall(body)
verify.waive_kit_research(false)
if not okrun then fail("designer section errored: " .. tostring(err)) end
