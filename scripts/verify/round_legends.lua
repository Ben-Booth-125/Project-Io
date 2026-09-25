-- A legend on every lapse round (BL-1118 R1; Ben, 2026-09-25: "we also
-- probably need a legend for each round, just in case it isn't clear what is
-- shown on the map"; STARTUP.md § Round 4).
--
--   ProjectIo --verify scripts/verify/round_legends.lua
--
-- WHAT THIS CHECKS. One capture per lapse round -- Culture, Empires,
-- Exploration, Industrialisation -- at the round's close, with the key in the
-- band under the map. The key lists exactly what `lapse_layers_drawn` says
-- the round draws (history_lapse.hpp), and the map's passes gate on the same
-- set, so the question a capture answers is the one a player asks: does each
-- row name something this map shows, in the swatch the map draws it in?
--
-- THE EMPIRES CUT IS THE SHARPEST TEST (Ben, 2026-09-25): the Empires key must
-- carry no caravan, no seat-captured ring, no capital slide ("slides if
-- moved"), no navy, harbour, treaty, colonial tie, sail crossing or landing,
-- while the Exploration key after it may carry them all. The run log's
-- "history_lapse legend (<span>): ..." line prints each round's rows in
-- order, once per record, so the lists can be read without the PNGs.
--
-- No binding reads the legend, so the verdict is the captures and the log
-- line, read by eye. Pinned seed 32 (the world sea_lanes.lua looks at): this
-- checks one world's keys, not a property of every world.

verify.window(1920, 1080)
verify.new_world(32)
verify.frames(4)

local rounds = {
    {i = 0, name = "culture"},
    {i = 1, name = "empires"},
    {i = 2, name = "exploration"},
    {i = 3, name = "industrialisation"},
}

for _, r in ipairs(rounds) do
    verify.history_run(r.i)
    verify.frames(4)
    local first, last = verify.history_span()
    verify.expect(last > first, r.name .. " carries a record with a span ("
                  .. first .. " -> " .. last .. ")")
    verify.history_year(last)
    verify.frames(2)
    verify.capture("round_legends_" .. (r.i + 3) .. "_" .. r.name)
    verify.expect(verify.history_powers() > 0,
                  r.name .. ": holders hold ground at the close ("
                  .. verify.history_powers() .. ")")
end
