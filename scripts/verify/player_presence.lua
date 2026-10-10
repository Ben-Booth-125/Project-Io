-- Visual verification for Player presence (BL-085), after the Planetary canvas
-- lost its HQ star and market-centre glyphs (Ben, 2026-10-10: "We should remove
-- the HQ glyph and market center glyphs").
--
-- What the plain Planetary canvas now carries for the player is the ownership
-- wash + outline on the player's own tiles — no HQ star, no market-centre
-- circle-and-cross. The Solar canvas keeps its home_body halo.
--
-- ASSERTED (not only captured): with no lens, a press on a settlement tile — the
-- ground a market centre stands on — resolves as an ordinary tile or building
-- press and never selects a MARKET. A market is selected from the map through the
-- Market lens (lens_structure_pivot.lua P1) or from the Market ledger.
--
-- Run with:
--   ProjectIo --verify scripts/verify/player_presence.lua

-- Planetary, plain lens. No goto_surface — keep the HQ framing setup_world
-- queued, so the holdings cluster fills the view (mirrors start_framing.lua).
verify.set_overlay("none")
verify.capture("player_presence_home_surface")

-- Plain presses on settlement tiles never select a market.
local home_body = nil
for _, b in ipairs(verify.buildings()) do
    if b.player then home_body = b.body break end
end
verify.goto_surface("home")
verify.frames(2)
local pressed, markets = 0, 0
for _, pc in ipairs(verify.population_centres()) do
    if pc.body == home_body and pc.scale >= 3 then
        verify.clear_selection()
        verify.frames(1)
        if verify.click_tile(pc.x, pc.y) then
            verify.frames(2)
            local t = verify.pointer_target()
            pressed = pressed + 1
            if t.selection_kind == "market" then
                markets = markets + 1
                print(string.format("[player_presence] (%d,%d) selected a MARKET", pc.x, pc.y))
            end
        end
    end
    if pressed >= 12 then break end
end
print(string.format("[player_presence] plain presses on settlement tiles: %d, market selections: %d",
                    pressed, markets))
verify.expect(pressed > 0, "pressed at least one settlement tile on the plain canvas")
verify.expect(markets == 0, "no plain press on a settlement tile selects a market")
verify.clear_selection()
verify.frames(1)

-- Ascend the zoom ladder to the Solar canvas for the home_body halo.
verify.command("ascend")
verify.command("ascend")
verify.capture("player_presence_solar_halo")
