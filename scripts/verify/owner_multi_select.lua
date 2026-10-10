-- Visual verification for the owner multi-select (BL-1240; LENSES.md § Corporation
-- lens, Owner multi-select, and § Company lens). Confirms requirements.json §
-- owner-multi-select:
--   R1 both lenses at their default: Corporation = the player picked (player in
--      colour, every rival owned-grey); Company = nothing picked (every firm
--      owned-grey). Unowned ground keeps its plain terrain.
--   R2 the key is a checklist (count header, search box, swatch rows); two rivals
--      picked via the verify API tint in colour; a SHIFT-click on an owned tile
--      toggles its owner through the real input path and touches no selection.
--   R3 the sets survive a body switch. (A picked rival's HQ star no longer
--      draws: the Planetary canvas's HQ glyphs retired, Ben 2026-10-10.)
-- The live half (Ben shift-clicks and uses the checklist) is not this script.
-- Run with: ProjectIo --verify scripts/verify/owner_multi_select.lua

-- Seat the player (as trade_flow_lens.lua does) so the Corporation set has a
-- player to default to.
verify.show_seat_screen(true)
verify.frames(3)
local firm = verify.seat_candidate(0)
verify.show_seat_screen(false)
verify.frames(1)
print("[owner_multi_select] seat " .. tostring(firm) .. " -> " .. tostring(verify.take_seat(firm)))

verify.goto_surface("home")
verify.frames(2)

-- The home body, and who holds ground on it, from the building roster.
local home_body = nil
for _, b in ipairs(verify.buildings()) do
    if b.player then home_body = b.body break end
end
local rivals, firms, seen, tile_of = {}, {}, {}, {}
for _, b in ipairs(verify.buildings()) do
    if b.body == home_body and not b.player and not seen[b.corp] then
        seen[b.corp] = true
        tile_of[b.corp] = { x = b.x, y = b.y }
        if b.background then firms[#firms + 1] = b.corp else rivals[#rivals + 1] = b.corp end
    end
end
print(string.format("[owner_multi_select] home body %s: %d rivals, %d firms hold ground",
                    tostring(home_body), #rivals, #firms))

-- A close framing on one owner's tile, so the tile fill reads at glyph scale.
-- The owner's tile, the home market area, sits among the player's and rivals'.
-- The lens washes the baked ground (BL-1250), so wait for the view's chunks: the
-- structures under the wash are what the capture is for.
local function settle()
    for i = 1, 600 do
        verify.frames(1)
        local s = verify.ground_stats()
        if s.pending_uploads == 0 and s.chunks > 0 then return end
    end
end
local function close(name, corp)
    local t = tile_of[corp]
    if t == nil then return end
    verify.center_tile(t.x, t.y, 6.0)
    verify.frames(3)
    settle()
    verify.capture(name)
    verify.goto_surface("home")   -- back to the whole-body framing
    verify.frames(2)
end

local function picks(kind)
    local t = verify.lens_owner_picks(kind)
    local n, s = 0, ""
    for _, id in ipairs(t) do
        n = n + 1
        s = (n == 1) and tostring(id) or (s .. "," .. tostring(id))
    end
    return n, s
end

-- R1: defaults.
verify.set_overlay("corporation")
verify.frames(2)
local n, list = picks("corporation")
print("[owner_multi_select] corporation default picks: " .. list)
verify.expect(n == 1, "the Corporation lens defaults to the player alone")
local player_list = list -- the player's id, as the default set prints it
settle()
verify.capture("owner_corps_default")
close("owner_corps_default_close", rivals[1])

verify.set_overlay("company")
verify.frames(2)
n, list = picks("company")
verify.expect(n == 0, "the Company lens defaults to no firm")
settle()
verify.capture("owner_companies_default")
close("owner_companies_default_close", firms[1])

-- R2/R3: two rivals picked via the API (the checklist's own toggle).
verify.expect(#rivals >= 2, "at least two rivals hold ground on the home body")
if #rivals >= 2 then
    verify.toggle_lens_owner(rivals[1])
    verify.toggle_lens_owner(rivals[2])
end
verify.set_overlay("corporation")
verify.frames(2)
n, list = picks("corporation")
print("[owner_multi_select] corporation picks after two toggles: " .. list)
verify.expect(n == 3, "player + two rivals picked")
settle()
verify.capture("owner_corps_two_rivals")
close("owner_corps_two_rivals_close", rivals[1])

-- Two firms picked, so the Company lens shows the same shape.
if #firms >= 2 then
    verify.toggle_lens_owner(firms[1])
    verify.toggle_lens_owner(firms[2])
end
verify.set_overlay("company")
verify.frames(2)
n, list = picks("company")
print("[owner_multi_select] company picks after two toggles: " .. list)
settle()
verify.capture("owner_companies_two_firms")
close("owner_companies_two_firms_close", firms[1])

-- R2: shift-click a third rival's tile through the real input path. The tile's
-- owner as the lens resolves it is whoever tile_to_corp names, so assert the set
-- moved by one, not which id.
verify.set_overlay("corporation")
verify.frames(1)
verify.expect(#rivals >= 3, "at least three rivals hold ground on the home body (shift-click needs an unpicked one)")
if #rivals >= 3 then
    -- No selection going in, so "touches no selection" is a plain comparison.
    verify.clear_selection()
    verify.frames(1)
    local before = picks("corporation")
    local t = tile_of[rivals[3]]
    local ok = verify.shift_click_tile(t.x, t.y)
    verify.frames(2)
    local after, after_list = picks("corporation")
    local pt = verify.pointer_target()
    print(string.format("[owner_multi_select] shift-click (%d,%d) ok=%s picks %d -> %d (%s) selection=%s",
                        t.x, t.y, tostring(ok), before, after, after_list, tostring(pt.selection_kind)))
    verify.expect(ok and after == before + 1, "a shift-click on an unpicked rival's tile picks it")
    verify.expect(not pt.has_selection and pt.selected_province == 0,
                  "a shift-click touches no selection")
    verify.capture("owner_corps_shift_click")
    -- Toggle rule: the same press again unpicks it.
    verify.shift_click_tile(t.x, t.y)
    verify.frames(2)
    local again = picks("corporation")
    verify.expect(again == before, "a second shift-click unpicks it")
    verify.expect(not verify.pointer_target().has_selection,
                  "the second shift-click touches no selection either")
    -- A PLAIN click on the same ground still selects (the lens's own resolution)
    -- and leaves the picked set alone.
    verify.click_tile(t.x, t.y)
    verify.frames(2)
    local plain = picks("corporation")
    local pp = verify.pointer_target()
    print("[owner_multi_select] plain click selection=" .. tostring(pp.selection_kind))
    verify.expect(pp.has_selection, "a plain click on owned ground still selects")
    verify.expect(plain == before, "a plain click does not toggle a pick")
    verify.clear_selection()
    verify.frames(1)
end

-- R3: the sets survive a body switch.
local before_corps, before_list = picks("corporation")
verify.goto_surface("moon")
verify.frames(2)
verify.goto_surface("home")
verify.frames(2)
local after_corps, after_list = picks("corporation")
verify.expect(after_list == before_list, "the Corporation set survives a body switch")

-- Reset: picks are per session and keyed by corp id, and ids restart in a new
-- world — so a load must drop them back to the default (the player alone, no firm).
local SAVE = "build_gen/verify/owner_multi_select.iosave"
verify.expect(verify.save(SAVE), "save wrote " .. SAVE)
verify.expect(#rivals >= 1 and #firms >= 1, "a rival and a firm to pick before the load")
if #rivals >= 1 then verify.toggle_lens_owner(rivals[1]) end
if #firms >= 1 then verify.toggle_lens_owner(firms[1]) end
verify.expect(verify.load(SAVE), "load read " .. SAVE)
verify.frames(3)
local rn, rlist = picks("corporation")
local cn = picks("company")
print(string.format("[owner_multi_select] after load: corporation picks %s, company picks %d", rlist, cn))
verify.expect(rn == 1 and rlist == player_list, "a load resets the Corporation set to the player")
verify.expect(cn == 0, "a load resets the Company set to empty")

verify.set_overlay("none")
