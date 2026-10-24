-- BL-1406 (Ben, 2026-10-23: the player IS its nation) — the UI read as the NATION.
--
-- The --verify world is laid unsearched and is NOT folded, so every other script
-- sees the seat corporation. This one folds it to ground first (verify.fold_world,
-- the world's own fold_to_ground, as every campaign start runs it), then reads the
-- surfaces BL-1406 re-subjected:
--
--   N1  after the fold the player acts as its NATION (player_actor == player_nation)
--   N2  the header reads the nation's TREASURY (capture), and the Nation overview's
--       Balance card is the nation's: its earnings less its expenses IS its net
--   N3  the player's buildings are listed as the nation's
--   N4  OWN GROUND: on a tile another nation holds, the Construct button is not
--       offered — the press asks placement with the player's nation named
--
-- Captures are looked at; the asserts are the verdict.

local function shot(name)
    verify.frames(2)
    verify.capture(name)
end

verify.goto_surface("home")

-- ── N1: fold, and who the player acts as ──
local f = verify.fold_world()
verify.expect(f.folded, "N1 the verify world folds to ground")
verify.expect(f.nation ~= 0 and f.actor == f.nation,
    string.format("N1 the player acts as its nation once folded (actor %d, nation %d)",
                  f.actor, f.nation))
print(string.format("[nation_seat_ui] fold: %d buildings folded, %d units re-owned, treasury %.0f",
                    f.buildings_folded, f.units_reowned, f.treasury))

verify.econ_step(2)

-- ── N2: the header and the Nation overview ──
shot("nation_header_treasury")

local card = verify.corp_balance_card()
verify.expect(card.nation, "N2 the Nation overview's subject is the nation")
verify.expect(card.measured, "N2 the nation's quarter was measured (a state-ledger row)")
local earned, spent = 0.0, 0.0
for _, s in ipairs(card.earnings) do earned = earned + s.value end
for _, s in ipairs(card.expenses) do spent = spent + s.value end
verify.expect(math.abs((earned - spent) - card.net) < 0.05,
    string.format("N2 earnings less expenses IS the printed net: %.3f vs %.3f",
                  earned - spent, card.net))
for _, s in ipairs(card.expenses) do
    verify.expect(s.label ~= "Interest" and s.label ~= "Levies",
                  "N2 the nation's '" .. s.label .. "' segment is not Interest or Levies (a nation never borrows)")
end
verify.show_panel("corporation", true)
verify.fold()
shot("nation_overview")
verify.show_panel("corporation", false)

-- ── N3: the player's buildings are the nation's ──
local mine, foreign = nil, nil
local n_mine = 0
for _, b in ipairs(verify.buildings()) do
    if b.player then
        n_mine = n_mine + 1
        if mine == nil and b.complete then mine = b end
    elseif foreign == nil and not b.background then
        foreign = b
    end
end
verify.expect(n_mine > 0, string.format("N3 the player's nation owns buildings: %d", n_mine))

if mine ~= nil then
    verify.center_tile(mine.x, mine.y, 8)
    verify.select_tile(mine.x, mine.y)
    shot("nation_own_tile")
end

-- ── N4: own ground — no Construct press on another nation's tile ──
verify.expect(foreign ~= nil, "N4 another nation's building to stand on")
if foreign ~= nil then
    verify.center_tile(foreign.x, foreign.y, 8)
    verify.select_tile(foreign.x, foreign.y)
    verify.frames(3)
    local c = verify.construction_controls()
    verify.expect(not c.construct_ok,
                  "N4 the Construct button is not offered on another nation's ground")
    shot("nation_foreign_tile")
end

verify.clear_selection()
shot("nation_cleared")
