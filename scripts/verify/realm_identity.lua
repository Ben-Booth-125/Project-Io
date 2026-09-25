-- A realm keeps its identity across the wizard's seams (BL-1087 colour,
-- BL-1088 name; STARTUP.md § Identity across the rounds; Ben, 2026-09-24).
--
--   ProjectIo --verify scripts/verify/realm_identity.lua
--
-- WHAT IT CHECKS, as verify.expect rather than as a golden: on the default
-- seed, every realm holding ground at 1200 CE on round 4 (Empires, the history
-- to 1200) holds the SAME colour slot and the SAME name at 1200 CE on round 5
-- (Exploration, 1200 -> 1660), and likewise at 1660 across round 5 -> round 6
-- (Industrialisation, 1660 -> 1960). Round 5's ticker at 1200 shows no "rises"
-- for a realm on round 4's final board (the resume notes them `inherited`,
-- ticker-silent). After a `capital_moved` the seat is the moved-to region and
-- the name is unchanged. Captures at each seam so the swatches, the base and a
-- ratchet can be judged by eye (verifier-visual), which the numbers cannot.
--
-- The rounds are run in order because the successor's pins come from the
-- predecessor's landed record: history_run(2) with no round 1 behind it would
-- colour from scratch and prove nothing.

verify.window(1920, 1080)

local function shot(name)
    verify.frames(2)
    verify.capture(name)
end

-- Every holder's (slot, name, capital) at the parked year, keyed by polity id.
local function identity_at(year)
    verify.history_year(year)
    verify.frames(2)
    local out = {}
    for _, p in ipairs(verify.history_holders()) do
        out[p] = { slot = verify.history_polity_slot(p),
                   name = verify.history_polity_name(p),
                   seat = verify.history_polity_capital(p),
                   rung = verify.history_polity_rung(p) }
    end
    return out
end

local function compare(a, b, label)
    local shared, same_slot, same_name = 0, 0, 0
    local first_diff = nil
    for p, ia in pairs(a) do
        local ib = b[p]
        if ib ~= nil then
            shared = shared + 1
            if ia.slot == ib.slot then same_slot = same_slot + 1
            elseif first_diff == nil then first_diff = "polity " .. p .. " slot " .. ia.slot .. " -> " .. ib.slot end
            if ia.name == ib.name then same_name = same_name + 1
            elseif first_diff == nil then first_diff = "polity " .. p .. " name '" .. ia.name .. "' -> '" .. ib.name .. "'" end
        end
    end
    verify.expect(shared > 0, label .. ": realms alive on both sides of the seam (" .. shared .. ")")
    verify.expect(same_slot == shared,
                  label .. ": every shared realm keeps its colour slot (" .. same_slot .. " of " .. shared
                  .. (first_diff and ("; first difference " .. first_diff) or "") .. ")")
    verify.expect(same_name == shared,
                  label .. ": every shared realm keeps its name (" .. same_name .. " of " .. shared .. ")")
    return shared
end

-- ROUND 4: Empires, to 1200 CE.
verify.history_run(1)
verify.frames(4)
local r4_first, r4_last = verify.history_span()
verify.expect(r4_last > r4_first, "round 4 carries a record (" .. r4_first .. " -> " .. r4_last .. ")")
local r4_at_1200 = identity_at(r4_last)
shot("realm_identity_1_round4_close")
local id4 = verify.history_identity()
verify.expect(id4.families > 0, "round 4 has a lineage tree to wedge by (" .. id4.families .. " families)")

-- A named realm: the board prints the coined name, never the seat's.
local named = 0
for _, ia in pairs(r4_at_1200) do if ia.name ~= "" and ia.name ~= "an unnamed realm" then named = named + 1 end end
verify.expect(named > 0, "round 4's realms carry coined names (" .. named .. ")")

-- A ratchet: at least one realm darker than at its founding by the close.
local rungs = 0
for _, ia in pairs(r4_at_1200) do if ia.rung > 0 then rungs = rungs + 1 end end
verify.expect(rungs > 0, "at least one realm has ratcheted a shade rung by 1200 (" .. rungs .. ")")

-- ROUND 5: Exploration, 1200 -> 1660, pinned from round 4.
verify.history_run(2)
verify.frames(4)
local r5_first, r5_last = verify.history_span()
verify.expect(r5_first == r4_last, "round 5 opens where round 4 closed (" .. r5_first .. ")")
local r5_at_1200 = identity_at(r5_first)
shot("realm_identity_2_round5_open")
local id5 = verify.history_identity()
verify.expect(id5.pinned, "round 5 was coloured with round 4's pins")
compare(r4_at_1200, r5_at_1200, "1200 seam")

-- No "rises" at the resume: the events at the open are inherited, not founded.
local founded_at_open = verify.history_event_count(0, r5_first)   -- kind 0 = founded, at or before the open
local inherited_at_open = verify.history_event_count(20, r5_first) -- kind 20 = inherited
verify.expect(founded_at_open == 0,
              "round 5's open re-founds nothing (" .. founded_at_open .. " founded, " .. inherited_at_open .. " inherited)")
verify.expect(inherited_at_open > 0, "round 5's open states every living realm's seat as inherited")

-- After a re-seating the seat moves and the name does not.
local moved_year = verify.history_event_year(4) -- kind 4 = capital_moved, the last in the span
if moved_year >= r5_first then
    local before = identity_at(moved_year - 1)
    local after  = identity_at(moved_year)
    local moved, renamed, seats_changed = 0, 0, 0
    for p, ib in pairs(before) do
        local ia = after[p]
        if ia ~= nil and ia.seat ~= ib.seat then
            seats_changed = seats_changed + 1
            if ia.name ~= ib.name then renamed = renamed + 1 end
        end
    end
    verify.expect(seats_changed > 0, "a capital_moved at " .. moved_year .. " moves a seat (" .. seats_changed .. ")")
    verify.expect(renamed == 0, "no realm is renamed by its re-seating (" .. renamed .. " renamed)")
    shot("realm_identity_3_round5_reseat")
end

local r5_at_1660 = identity_at(r5_last)
shot("realm_identity_4_round5_close")

-- ROUND 6: Industrialisation, 1660 -> 1960, pinned from round 5.
verify.history_run(3)
verify.frames(4)
local r6_first, r6_last = verify.history_span()
verify.expect(r6_first == r5_last, "round 6 opens where round 5 closed (" .. r6_first .. ")")
local r6_at_1660 = identity_at(r6_first)
shot("realm_identity_5_round6_open")
compare(r5_at_1660, r6_at_1660, "1660 seam")
identity_at(r6_last)
shot("realm_identity_6_round6_close")

verify.expect_no_clipping("realm_identity")
