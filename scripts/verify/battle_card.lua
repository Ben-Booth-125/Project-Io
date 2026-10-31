-- Verify the battle card and the Field dispatch channel (BL-469 R1 / BL-468 R1).
--
-- THE HARD PART IS GETTING A BATTLE TO EXIST AT ALL, and it is why this script
-- could not have been written before BL-469's rider (NR-345). Battle discovery is
-- gated on `corp_hostile_pairs`, which only `declare_hostile` writes, which before
-- the generic `verify.corp_command` binding no script could reach — so every
-- visual requirement on this surface was green-but-blind by construction.
--
-- The staging is therefore the substance of the check: declare hostility both
-- ways, march the player's unit into a rival's province, step until contact.
-- Nothing here hard-codes a grid coordinate, so a generation change moves the
-- fight rather than silently invalidating the check.
--
-- FIXED (BL-1453, 2026-10-29): the loop below steps DAYS (verify.day_step), so
-- the fragility recorded here no longer applies. The record, kept:
-- KNOWN FRAGILITY (NR-587, 2026-08-24): the step-until-contact loop below only
-- ever observes a battle via `verify.select_battle`, which can only see world
-- state BETWEEN whole ticks. If the specific matchup this run's generation
-- produces settles within one `econ_step` call -- and since BL-1445 (move by
-- days) battles step a round a DAY and one `econ_step` is a whole quarter, ninety
-- days, so a SHORT fight opens and concludes inside one call (battles have no
-- maximum duration since 2026-10-29, so a long one is still live after it) --
-- the battle is never observably "in progress", and this script
-- fails with "no battle opened" even though a real fight did happen. Noted rather than fixed (Ben, 2026-08-24:
-- low priority) -- if this script starts failing, check for that before
-- assuming discovery itself broke.

local VERB_DECLARE_HOSTILE = 17
local VERB_MARCH_UNIT      = 21

verify.goto_surface("home")

-- Who is the player? Read it from the units table's own `player` flag rather
-- than assuming an id — ids are generation-dependent (corporations are retired;
-- every unit is a nation's).
local player_corp = nil
for _, u in ipairs(verify.units()) do
    if u.player then player_corp = u.owner; break end
end
assert(player_corp, "no player-owned unit found; cannot identify the player nation")

-- Pick the NEAREST (player unit, rival unit) pair. BL-1444 (battle card march stall):
-- this used to take the player's first unit and the FIRST rival unit in the units
-- table, wherever it stood. On the verify world that rival stood ~150 tiles away
-- ((250,76) -> (102,18)): the march was accepted and walked every day, but 720 days
-- covered only ~80 tiles of it, so no contact ever came. (The older stall - a march
-- that never left its tile - went when movement stopped drawing Logistic Points,
-- MILITARY.md § Marching.) Hex steps on the odd-r grid, no cylinder wrap: a
-- deterministic "near", not an exact one. Ties go to the earlier pair in table order.
-- The next pair is tried only after a refusal, so on the verify world the first one wins.
local function hex_steps(a, b)
    local function cube(col, row)
        local x = col - (row - (row % 2)) / 2
        return x, row, -x - row
    end
    local ax, ay, az = cube(a.col, a.row)
    local bx, by, bz = cube(b.col, b.row)
    return math.max(math.abs(ax - bx), math.abs(ay - by), math.abs(az - bz))
end
local own, rivals = {}, {}
for _, u in ipairs(verify.units()) do
    if u.col ~= nil then
        if u.owner == player_corp then own[#own + 1] = u else rivals[#rivals + 1] = u end
    end
end
assert(#own > 0,    "the player has no unit on the surface (BL-331 seeds one)")
assert(#rivals > 0, "no rival unit on the surface (BL-476: named rivals are armed)")
local candidates = {} -- every (player unit, rival unit) pair, in (own, rival) table order
for _, m in ipairs(own) do
    for _, t in ipairs(rivals) do
        candidates[#candidates + 1] = { d = hex_steps(m, t), mine = m, theirs = t }
    end
end

-- The nearest untried pair, by a min-scan (the verify state opens no `table`
-- library); ties to the earlier pair. Scanned again only after a refusal.
local function nearest_untried()
    local best = nil
    for _, p in ipairs(candidates) do
        if not p.tried and (best == nil or p.d < best.d) then best = p end
    end
    return best
end

-- The march onto the rival's tile first (BL-1443: contact is the attack), then
-- hostility BOTH WAYS toward that rival, and only that one: a refused pair (another
-- landmass, a class wall) declares nothing and gives way to the next nearest. The
-- battle opens on contact, days later, by which time both stances stand. Hostility
-- is a directed state, and making the intent explicit beats relying on discovery's
-- "at least one direction" rule holding forever.
local mine, theirs, march_result = nil, nil, nil
local p = nearest_untried()
while p ~= nil do
    p.tried = true
    march_result = verify.corp_command{ verb = VERB_MARCH_UNIT, corp = player_corp,
                                        subject = p.mine.id, col = p.theirs.col, row = p.theirs.row }
    print(string.format("battle_card: march %s from (%d,%d) onto (%d,%d), %d hex steps",
                        tostring(march_result), p.mine.col, p.mine.row, p.theirs.col, p.theirs.row, p.d))
    if march_result == "applied" then mine, theirs = p.mine, p.theirs; break end
    p = nearest_untried()
end
assert(march_result == "applied", "every march onto a rival stack was refused: " .. tostring(march_result))
verify.corp_command{ verb = VERB_DECLARE_HOSTILE, corp = player_corp,  counterparty = theirs.owner }
verify.corp_command{ verb = VERB_DECLARE_HOSTILE, corp = theirs.owner, counterparty = player_corp }

local target_province = verify.select_province(theirs.col, theirs.row)
assert(target_province ~= 0, "the rival's unit stands on an unpartitioned tile")
verify.clear_selection()

-- Step DAYS until contact (BL-1453, NR-587's fix): verify.day_step walks the day
-- loop's own path, so the battle is seen live on the day it opens rather than
-- opening and closing inside a ninety-day econ_step. A generous budget: how long
-- the march takes is a function of the generated ground, not a constant.
local in_battle = false
for _ = 1, 720 do
    verify.day_step(1)
    if verify.select_battle(theirs.col, theirs.row) then in_battle = true; break end
    for _, u in ipairs(verify.units()) do
        if u.id == mine.id and (_G.__bc_n or 0) % 10 == 0 then
            print(string.format("battle_card: tick at (%s,%s) marching=%s", tostring(u.col), tostring(u.row), tostring(u.marching)))
        end
    end
    _G.__bc_n = (_G.__bc_n or 0) + 1
end
assert(in_battle, "no battle opened within 720 days of marching into contact")

-- The card: phase word, both sides' per-unit strength bars, and the withdrawal
-- price with its three terms separated.
verify.capture("battle_card")

-- A few more days, so the card shows a fight that has MOVED — the phase readout
-- is a reading of the last round batch, and a first-round capture cannot show it
-- changing. A battle is weeks long, so it is still live.
verify.day_step(3)
assert(verify.select_battle(theirs.col, theirs.row), "the battle closed within three days of opening")
verify.capture("battle_card_stepped")

-- The Field channel (BL-468). Channel 1 is Field — appended after Public, never
-- inserted, because the counsel channel caches an index into that list.
local field_lines = 0
for _, line in ipairs(verify.chat_lines()) do
    if line:match("^1:") then field_lines = field_lines + 1 end
end
assert(field_lines > 0, "the fight produced no Field-channel dispatches")

-- The comms dock is chrome, not a panel: it is always on screen, so there is
-- nothing to open before capturing it.
verify.capture("battle_dispatches")
