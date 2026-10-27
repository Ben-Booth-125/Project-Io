-- Verify the land unit counter, the unit card and the unit group (BL-1425 R1-R3,
-- and Ben's iteration of 2026-10-27).
--
-- The counter: one rectangle per (tile, owner) on the Planetary canvas — owner
-- band, the number of units on the tile, and two VERTICAL bars filling bottom-up,
-- HP (green) then readiness (orange), for the player's own units only; a
-- hover-colour ring when the pointer is on it. The card: HP, readiness and morale
-- bars, and the land class the unit fights as. The group: a press on a counter of
-- several units opens one row per unit; a row opens that unit's card, whose
-- header carries "< Group (N)" back.
--
-- Staging: no verify call wounds a unit, so the counters are captured at full HP
-- and readiness; a fight's bars are a live check. Two units on one tile come from
-- hiring at the player's own military base through the real hire verb (lib.lua's
-- raise_player_force stands one up when the player has none). Nothing here
-- hard-codes a coordinate: units are read from verify.units(), and the counter is
-- found by moving the pointer down from its tile's centre until the hover pass
-- names a unit.

verify.goto_surface("home")

local VERB_HIRE_UNIT = 8 -- corp_verb::hire_unit

local function player_units()
    local out = {}
    for _, u in ipairs(verify.units()) do
        if u.col ~= nil and u.player then out[#out + 1] = u end
    end
    return out
end

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

-- Aim the pointer at the counter on (col, row): the counter stands below the hex
-- centre, so step down from the tile's screen point until hover names a unit.
-- Returns the screen point, or nil.
local function find_counter(col, row)
    local t = verify.tile_screen(col, row)
    if not t.ok then return nil end
    for dy = 0, 60, 2 do
        verify.hover(t.x, t.y + dy, 2)
        if verify.pointer_target().hovered_kind == "unit" then
            return { x = t.x, y = t.y + dy + 3 } -- a little into the plate
        end
    end
    return nil
end

-- 1. The player's counter, vertical bars, not hovered.
verify.center_tile(mine.col, mine.row, 10.0)
verify.clear_selection()
verify.mouse(-1, -1)
verify.frames(2)
verify.capture("unit_counter_own")

-- 2. The same counter hovered: the hover ring.
local at = find_counter(mine.col, mine.row)
if at then
    verify.hover(at.x, at.y, 3)
    verify.capture("unit_counter_hover")
else
    print("unit_counter: FAIL could not aim the pointer at the player's counter")
end

-- 3. The card, Strength page: HP, readiness and morale without scrolling.
assert(verify.select_unit(mine.id), "select_unit refused the player's own unit")
verify.mouse(-1, -1)
verify.frames(2)
verify.capture("unit_counter_card")

-- 4. Two units on one tile: hire at the player's base.
local function base_tile()
    for _, b in ipairs(verify.buildings()) do
        if b.player and b.type == "Military Base" and b.complete then return b.tile, b.corp end
    end
    return nil
end

local function hire_at(tile)
    local LOAN = 5000.0
    verify.set_balance(verify.player_balance() + LOAN)
    verify.grant_stock("iron_ore", 50)
    verify.grant_stock("agricultural_produce", 50)
    verify.grant_stock("coal", 50)
    local last = "none"
    for row = 0, 15 do
        last = verify.corp_command{ verb = VERB_HIRE_UNIT, tile = tile, unit_type = row }
        if last == "applied" then break end
    end
    verify.set_balance(verify.player_balance() - LOAN)
    return last
end

local tile = base_tile()
if tile == nil then
    local corp = nil
    for _, b in ipairs(verify.buildings()) do
        if b.player then corp = b.corp; break end
    end
    assert(corp, "no player-owned building found")
    raise_player_force(corp) -- places a base and hires one unit onto it
    verify.place_mode("none") -- out of placement mode: the presses below select
    tile = base_tile()
end
assert(tile, "no completed player military base to hire at")
local r1 = hire_at(tile)
local r2 = hire_at(tile)
print("unit_counter: hires at the base: " .. tostring(r1) .. ", " .. tostring(r2))

-- The tile holding the most player units.
local stack, best = nil, 0
do
    local by = {}
    for _, u in ipairs(player_units()) do
        local k = u.col .. "," .. u.row
        by[k] = by[k] or { n = 0, u = u }
        by[k].n = by[k].n + 1
        if by[k].u.id > u.id then by[k].u = u end
        if by[k].n > best then best = by[k].n; stack = by[k].u end
    end
end
assert(stack and best >= 2, "no tile holds two player units after hiring (best " .. best .. ")")
print("unit_counter: " .. best .. " player units on tile " .. stack.col .. "," .. stack.row)

-- 5. The stacked counter, then a REAL press on it: the group panel. A press on
--    a neighbouring tile first, so the press below is a FIRST press here and not
--    a repeat-click cycle step (the base's placement anchored the cycle on it).
local reset = verify.click_tile(stack.col + 6, stack.row)
print("unit_counter: anchor reset press landed=" .. tostring(reset)
      .. " selection_kind=" .. verify.pointer_target().selection_kind)
verify.center_tile(stack.col, stack.row, 10.0)
verify.clear_selection()
verify.mouse(-1, -1)
verify.frames(2)
verify.capture("unit_counter_stack")

local sat = find_counter(stack.col, stack.row)
assert(sat, "could not aim the pointer at the stacked counter")
print("unit_counter: pressing at " .. sat.x .. "," .. sat.y .. " hovered_kind="
      .. verify.pointer_target().hovered_kind)
verify.click(sat.x, sat.y)
verify.mouse(-1, -1)
verify.frames(2)
local pt = verify.pointer_target()
print("unit_counter: after the press selection_kind=" .. pt.selection_kind
      .. " unit_group=" .. tostring(pt.unit_group))
assert(pt.selection_kind == "unit" and pt.unit_group, "a press on a stacked counter did not open its group")
verify.capture("unit_counter_group")

-- 6. A row opened, then the card with its way back.
assert(verify.select_unit(stack.id, false), "select_unit refused")
verify.frames(2)
verify.capture("unit_counter_group_member")

-- 7. A rival's counter: owner and count, no bars (R3).
if theirs ~= nil then
    verify.center_tile(theirs.col, theirs.row, 10.0)
    verify.clear_selection()
    verify.frames(2)
    verify.capture("unit_counter_rival")
else
    print("unit_counter: no rival unit on the surface; R3's capture not taken")
end
