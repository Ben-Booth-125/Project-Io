-- BL-603 — under a lens, hover and selection pivot to the whole STRUCTURE.
--
-- Ben, 2026-08-24: "When a lens reveals any large structure, the selection should
-- pivot to the entire structure, and no longer provinces. So for the market lens,
-- the entire market gets highlighted on mouse over, and clicking opens up our
-- market ledger for that market."
--
-- The principle is not new — LENSES.md has said since 2026-06-15 that "the active
-- lens does not only re-skin the canvas: it DEFINES what the pointer resolves to",
-- and it already routed a Market-lens click to the Market Ledger. What is new is
-- that the rule now works at the grain the lens DRAWS, and says so before the
-- click: the catchment lights whole on hover, and the press takes the market
-- rather than the ground under the pointer.
--
-- THREE CLAIMS, and the third is the one that keeps the other two honest:
--   P1  under the Market lens, a press on ordinary ground selects the MARKET and
--       opens the Market Ledger on it.
--   P2  under the Corporation lens, the same press selects the CORPORATION and
--       opens the Budget ledger.
--   P3  under a lens whose subject IS the tile (Population), the same press still
--       selects the TILE. A pivot that fired under every lens would not be a
--       lens-keyed rule, it would be a canvas that had stopped selecting ground.
--
-- Run: ProjectIo --verify scripts/verify/lens_structure_pivot.lua

verify.window(1280, 720)

local s = stage_ui_fixture()
verify.center_tile(s.unit.col, s.unit.row, 4.0)
verify.frames(4)

-- The press point must be ORDINARY GROUND, and finding it is half the check's
-- design. The first draft pressed the player's own unit's tile and every assertion
-- failed with "got unit" / "got building": a marker outranks a structure, exactly
-- as the canvas intends — "a marker is a specific thing the player aimed at". So
-- the pivot can only be observed where nothing is standing.
--
-- Resolved from the world rather than measured by eye, which is the lesson
-- border_band.lua learned expensively when a hard-coded coordinate passed in one
-- worktree and failed on the merge.
local occupied = {}
local function mark(col, row) occupied[tostring(col) .. "," .. tostring(row)] = true end
for _, b in ipairs(verify.buildings()) do mark(b.x, b.y) end
for _, u in ipairs(verify.units())     do mark(u.col, u.row) end

local tp = nil
for radius = 1, 6 do
    for dc = -radius, radius do
        for dr = -radius, radius do
            local col, row = s.unit.col + dc, s.unit.row + dr
            if not occupied[tostring(col) .. "," .. tostring(row)] then
                local p = verify.tile_screen(col, row)
                -- On screen AND clear of the bottom band / right column, so the
                -- press lands on canvas rather than on chrome.
                if p.ok and p.y < 440 and p.x > 70 and p.x < 920 then
                    tp = { col = col, row = row }
                    break
                end
            end
        end
        if tp then break end
    end
    if tp then break end
end

verify.expect(tp ~= nil, "found a marker-free tile on screen to press")
if tp == nil then return end

--- Set a lens, hover the anchor (so the highlight resolves), capture, then press.
--- Returns the pointer_target reading after the press.
-- Takes GRID coordinates, not screen ones, and resolves the screen point fresh on
-- every call. That is not fussiness: a press under the Market lens OPENS the
-- Market Ledger, the fold-out column then narrows the canvas, and a screen point
-- cached before that lands on a different tile afterwards. The second draft of
-- this check failed its own control that way, reporting "building" for a tile it
-- had already proved was empty — the same class of staleness as a hard-coded
-- coordinate, one level subtler.
local function pivot_at(col, row, lens, name)
    verify.show_panel("market", false)
    verify.show_panel("balance", false)
    verify.clear_selection()
    verify.set_overlay(lens)
    verify.frames(2)
    local point = verify.tile_screen(col, row)
    verify.expect(point.ok, name .. ": the press target is on screen")
    -- Two frames of hover before the shot: the structure highlight is written
    -- AFTER the tile loop, so it lands on the following frame — the same one-frame
    -- lag the province outline has always carried.
    verify.hover(point.x, point.y, 3)
    shot("pivot_" .. name .. "_hover")
    verify.click(point.x, point.y)
    verify.frames(3)
    shot("pivot_" .. name .. "_clicked")
    return verify.pointer_target()
end

--- The common case: press the marker-free ground tile found above.
local function pivot(lens, name) return pivot_at(tp.col, tp.row, lens, name) end

-- P1 — the Market lens
local m = pivot("market", "market")
verify.expect(m.selection_kind == "market",
              "market lens: a press on ground selects the MARKET (got " ..
              tostring(m.selection_kind) .. ")")
verify.expect(m.open_panel == "market",
              "market lens: the press opens the Market Ledger (got " ..
              tostring(m.open_panel) .. ")")
verify.expect(m.selected_province == 0,
              "market lens: the province mirror clears, as for any entity selection")

-- P2 — the Corporation lens, pressed on a BUILDING rather than on ground.
-- Its structure is exactly the tiles carrying that corp's buildings, so there is
-- no empty ground inside it to pivot from; it resolves THROUGH the marker, which
-- is what LENSES.md's routing table has claimed since 2026-06-15.
local bt = nil
for _, b in ipairs(verify.buildings()) do
    local p = verify.tile_screen(b.x, b.y)
    if p.ok and p.y < 440 and p.x > 70 and p.x < 920 then bt = { col = b.x, row = b.y }; break end
end
verify.expect(bt ~= nil, "found a building on screen to press")
local c = bt and pivot_at(bt.col, bt.row, "corporation", "corporation") or {}
verify.expect(c.selection_kind == "corporation",
              "corporation lens: a press selects the CORPORATION (got " ..
              tostring(c.selection_kind) .. ")")
verify.expect(c.open_panel == "balance",
              "corporation lens: the press opens the Budget ledger (got " ..
              tostring(c.open_panel) .. ")")

-- P3 — the control: a tile-grain lens still selects the tile
local t = pivot("population", "population")
verify.expect(t.selection_kind == "tile",
              "population lens: the ground is still the ground (got " ..
              tostring(t.selection_kind) .. ")")
verify.expect(t.selected_province ~= 0,
              "population lens: a tile selection still mirrors its province")

-- And with no lens at all, which is the same claim from the other side.
local n = pivot("none", "nolens")
verify.expect(n.selection_kind == "tile",
              "no lens: the ground is still the ground (got " ..
              tostring(n.selection_kind) .. ")")

verify.set_overlay("none")
verify.clear_selection()
verify.frames(1)
