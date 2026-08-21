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

local VERB_DECLARE_HOSTILE = 17
local VERB_MARCH_UNIT      = 21

verify.goto_surface()

-- Who is the player? Read it from the buildings table's own `player` flag rather
-- than assuming a corp id — corp ids are generation-dependent.
local player_corp = nil
for _, b in ipairs(verify.buildings()) do
    if b.player then player_corp = b.corp; break end
end
assert(player_corp, "no player-owned building found; cannot identify the player corp")

-- Pick the player's unit and the nearest rival unit.
local mine, theirs = nil, nil
for _, u in ipairs(verify.units()) do
    if u.col ~= nil then
        if u.owner == player_corp then
            if mine == nil then mine = u end
        elseif theirs == nil then
            theirs = u
        end
    end
end
assert(mine,   "the player has no unit on the surface (BL-331 seeds one)")
assert(theirs, "no rival unit on the surface (BL-476: named rivals are armed)")

local target_province = verify.select_province(theirs.col, theirs.row)
assert(target_province ~= 0, "the rival's unit stands on an unpartitioned tile")
verify.clear_selection()

-- Hostility BOTH WAYS. It is a directed state, and making the intent explicit
-- beats relying on discovery's "at least one direction" rule holding forever.
verify.corp_command{ verb = VERB_DECLARE_HOSTILE, corp = player_corp,  counterparty = theirs.owner }
verify.corp_command{ verb = VERB_DECLARE_HOSTILE, corp = theirs.owner, counterparty = player_corp }

verify.corp_command{ verb = VERB_MARCH_UNIT, corp = player_corp,
                     subject = mine.id, province = target_province }

-- Step until contact. A generous budget: the march is path-marched across ticks
-- on the shared traversal-cost metric, so how long it takes is a function of the
-- generated ground, not a constant this script may assume.
local in_battle = false
for _ = 1, 120 do
    verify.econ_step(1)
    if verify.select_battle(theirs.col, theirs.row) then in_battle = true; break end
end
assert(in_battle, "no battle opened within 120 ticks of marching into contact")

-- The card: phase word, both sides' per-unit strength bars, and the withdrawal
-- price with its three terms separated.
verify.capture("battle_card")

-- One more tick batch, so the card shows a fight that has MOVED — the phase
-- readout is a reading of the last round batch, and a first-round capture cannot
-- show it changing.
verify.econ_step(1)
verify.select_battle(theirs.col, theirs.row)
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
