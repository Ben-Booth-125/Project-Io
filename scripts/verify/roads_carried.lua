-- Roads carried across the rounds (BL-1134; Ben, 2026-09-25, walking round 6:
-- "we lost every road between Exploration and Industrialisation"; STARTUP.md
-- § Round 6, "Every round draws the roads the rounds before it laid").
--
--   ProjectIo --verify scripts/verify/roads_carried.lua
--
-- WHAT THIS CHECKS. A resumed span notes only the promotions it makes, so a
-- round's record read alone opens with none of the roads the earlier rounds
-- laid. The round before hands its whole network over at the seam
-- (`lapse_roads_at_close`, history_lapse.cpp), as the civilisation diamonds
-- are. The assertion is the seam itself, twice: EVERY corridor the map draws
-- at round N's close is drawn on round N+1's first frame, for 4 -> 5 and
-- 5 -> 6. Read through `verify.history_roads()`, the road pass's own
-- predicate, so the count is what the map draws and not what a record holds.
--
-- The rounds run in order (4, 5, 6) because the hand-over is made at launch
-- from the round before; a round launched with nothing behind it carries
-- nothing. One capture per seam's far side, and round 6's first frame is the
-- frame Ben's walk found empty.
--
-- Pinned seed 32 (the world sea_lanes.lua and round_legends.lua look at).

verify.window(1920, 1080)
verify.new_world(32)
verify.frames(4)

local function roads_at(year)
    verify.history_year(year)
    verify.frames(2)
    return verify.history_roads()
end

local function missing(from, to)
    local n, first = 0, nil
    for k, _ in pairs(from.keys) do
        if not to.keys[k] then
            n = n + 1
            first = first or k
        end
    end
    return n, first
end

local prev_close = nil
local prev_name  = nil
for _, r in ipairs({ {i = 1, name = "empires"}, {i = 2, name = "exploration"},
                     {i = 3, name = "industrialisation"} }) do
    verify.history_run(r.i)
    verify.frames(4)
    local first, last = verify.history_span()
    verify.expect(last > first, r.name .. " carries a record with a span ("
                  .. first .. " -> " .. last .. ")")

    local opening = roads_at(first)
    print(string.format("roads_carried: %s opens at %d with %d corridors drawn (%d carried in)",
                        r.name, first, opening.count, opening.carried))
    if prev_close then
        verify.capture("roads_carried_" .. (r.i + 3) .. "_" .. r.name .. "_first_frame")
        local n, k = missing(prev_close, opening)
        verify.expect(n == 0, string.format(
            "every road %s drew at its close (%d) is drawn on %s's first frame (%d missing%s)",
            prev_name, prev_close.count, r.name, n, k and (", e.g. " .. k) or ""))
        verify.expect(opening.carried >= prev_close.count, string.format(
            "%s was handed at least the %d corridors %s drew at its close (%d carried)",
            r.name, prev_close.count, prev_name, opening.carried))
    end

    local closing = roads_at(last)
    print(string.format("roads_carried: %s closes at %d with %d corridors drawn",
                        r.name, last, closing.count))
    if r.i == 1 then
        verify.expect(closing.count > 0,
                      "the Empires round lays roads to carry (" .. closing.count .. " at its close)")
    end
    prev_close, prev_name = closing, r.name
end
