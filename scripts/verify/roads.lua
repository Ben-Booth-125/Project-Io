-- Visual verification for the road network (BL-146 generation + BL-147/BL-172 rendering &
-- placement) and the Inland Logistics Hub (BL-149) build affordance.
--
--   BL-172 R1: the generated road lattice renders always-on on the Planetary canvas as
--           continuous, symmetric spans (each roaded tile draws its own half of the shared edge,
--           meeting at the edge midpoint — no "from vs to" asymmetry), over a three-tier ladder:
--           Track (road_level 1) thin/dim, Road (2) medium, Highway (3) thick/bright. Visible on
--           the home body (Kepler), which generates with a per-nation road network (BL-146/172).
--   BL-172 R1 / BL-149: the tile build front door lists THREE road affordances — Track, Road,
--           Highway (each with its own cost) — and an "Inland Logistics Hub".
--
--   BL-1370: A ROAD NAMES WHICH NEIGHBOUR IT CROSSES TO (construction.md § "A road is built
--           from a road to a tile, in two presses"; LOGISTICS.md § 4). A road is an edge
--           between two tiles, so the press has to name a SIDE — and until BL-1370 no UI ever
--           did: `k_road_side_auto` let the rules guess, and a tile the player was building
--           OUT of came back "this tile already has an equal or better road", because the
--           question asked was about the TILE. The row now ARMS, the canvas highlights every
--           crossing the rules admit, and the second press lays one of them.
--
--           What this half of the script asserts, and why each assertion is here:
--             1. the highlighted set is EXACTLY what `placement_rules::can_place_road` admits.
--                The expected set is built in THIS script from `verify.road_admits` — a second,
--                independent call site of the same gate — so a bug in the canvas's own loop
--                cannot agree with itself;
--             2. a press on a highlighted tile lays exactly ONE crossing (`count_road_edges`
--                moves by one) and charges it ONCE (the balance moves by the row's own cost);
--             3. a press anywhere else disarms and lays nothing;
--             4. the crossing just laid is gone from the admitted set — the rule that refused
--                the old press ("equal or better road") now refuses one SIDE, which is the
--                whole correction;
--             5. a tile with nowhere left to go arms and highlights NOTHING. The refusal is
--                the absence of a mark, not a sentence after the fact.
-- Run with: ProjectIo --verify scripts/verify/roads.lua

verify.econ_step(4)                     -- populate market state (build-cost context)
verify.goto_surface("home")             -- Kepler surface: the generated road lattice
verify.capture("roads_surface")

verify.set_zoom(0.5)                    -- wider view: the lattice spanning the body
verify.capture("roads_wide")

-- The tile build front door: a land tile now offers a Road (BL-147) and an Inland
-- Logistics Hub (BL-149) alongside the extraction / processing / port / launchpad types.
verify.set_zoom(1.0)
verify.select_tile(57, 34)
verify.capture("roads_build_select")    -- settle the selection (a new select closes the column)
verify.show_panel("build", true)
verify.capture("roads_build_front_door")

-- ===========================================================================
-- BL-1370: the two presses
-- ===========================================================================
-- 1720x1080 for this half, the resolution the shell's widths are tuned against
-- and the one construction_progress.lua captures at. MEASURED: at the 1280x720
-- the captures above use, the road ladder never comes on screen even at the build
-- list's scroll maximum, so every press publishes as absent (-1) and the flow
-- cannot be driven at all. The captures above keep their own resolution.
verify.window(1720, 1080)
verify.frames(2)
-- A road crossing is charged, so fund the corp: this check is about what the
-- presses do, not about affordability (which has its own assertion below).
verify.set_balance(5000000.0)

local body = verify.ground_body_id()

-- The expected set, built HERE from the rules' own answer. Returned sorted by
-- side so two sets compare as strings and an ordering difference is not a
-- failure of its own.
local function admitted(x, y, tier)
    local a = verify.road_admits(x, y, tier)
    local out = {}
    if a.ok then
        for _, e in ipairs(a.sides) do
            out[#out + 1] = string.format("%d@%d,%d", e.side, e.x, e.y)
        end
    end
    -- Sorted and joined BY HAND: the verify Lua sandbox opens no `table`
    -- library, so `table.sort` / `table.concat` are nil indexes rather than
    -- calls (acquisitions_ledger.lua carries the same insertion sort and says
    -- so). The order is for the COMPARISON below, which is a string equality.
    for i = 2, #out do
        local k, j = out[i], i - 1
        while j >= 1 and out[j] > k do out[j + 1] = out[j]; j = j - 1 end
        out[j + 1] = k
    end
    return out, (a.ok and a.n_sides or -1)
end
local function join(t)
    local s = ""
    for i, v in ipairs(t) do s = (i == 1) and v or (s .. " " .. v) end
    return s
end
local function sorted(t)
    for i = 2, #t do
        local k, j = t[i], i - 1
        while j >= 1 and t[j] > k do t[j + 1] = t[j]; j = j - 1 end
        t[j + 1] = k
    end
    return t
end

-- A TILE THAT ALREADY CARRIES A ROAD — Ben's own case. Generation gives every
-- population centre's tile at least a Track (LOGISTICS.md § 4), so a centre is
-- where one is found. The proof that it carries one is indirect and reads no tile
-- data: a side holding Track admits an UPGRADE to Road but no second Track, so a
-- tile whose Road set is larger than its Track set has a Track on it.
--
-- The centre with the MOST crossings still open is the one taken, so the
-- highlight capture shows the affordance at its widest rather than at whatever
-- the first coastal town happens to offer.
local from, from_n = nil, 0
for _, c in ipairs(verify.population_centres()) do
    if c.body == body then
        local _, n1 = admitted(c.x, c.y, 1)
        local _, n2 = admitted(c.x, c.y, 2)
        if n1 ~= nil and n1 > from_n and n2 > n1 then
            from, from_n = c, n1
        end
    end
end
verify.expect(from ~= nil,
              "found a tile that already carries a road and still has somewhere to cross to")

if from ~= nil then
    local want, n_want = admitted(from.x, from.y, 1)
    print(string.format("roads: arming Track from (%d,%d); the rules admit %d crossing(s): %s",
                        from.x, from.y, n_want, join(want)))

    verify.select_tile(from.x, from.y)
    -- The highlights ARE the evidence, so put them on screen and close enough to
    -- read: at the resting zoom a hex is a few pixels and three outlined
    -- neighbours are indistinguishable from map noise.
    verify.center_tile(from.x, from.y)
    verify.set_zoom(3.0)
    verify.frames(1)                 -- a new selection closes the column
    verify.show_panel("build", true)
    verify.frames(2)
    -- THE LADDER IS AT THE FOOT of the build list's own scroller, and a position
    -- a click cannot reach is not published at all (-1). Scroll to it first, as
    -- the rail-row checks do.
    verify.section("construction_queue", false)
    verify.scroll_panel("build_list", 1.0)
    verify.frames(2)

    local rs = verify.road_state()
    verify.expect(rs.n_rows == 3, "the road ladder published its three rows (" .. rs.n_rows .. ")")
    local track = nil
    for _, r in ipairs(rs.rows) do if r.tier == 1 then track = r end end
    verify.expect(track ~= nil, "the ladder carries a Track row")
    verify.expect(track.x > 0 and track.y > 0,
                  "the Track row publishes where its press landed, so the check presses the real one")
    verify.expect(track.enabled, "the Track press is live on a funded corp")
    verify.expect(track.targets == n_want,
                  string.format("the row counts the rules' own crossings (%d vs %d)",
                                track.targets, n_want))
    verify.expect(track.explainer ~= "" and #track.explainer <= 240,
                  "the press keeps a short explainer: '" .. track.explainer .. "'")
    verify.expect(not rs.armed, "nothing is armed before the press")

    -- ── 1. The press ARMS, and the highlight is exactly the rules' answer ────
    -- Guarded rather than assumed: a press the scroll did not reach is published
    -- as absent, and the whole flow below needs it. One failure that names itself
    -- beats a nil index ten lines later.
    verify.expect(track.x > 0, "the Track press is on screen, so the flow below can run")
    if track.x > 0 then
    verify.click(track.x, track.y)
    verify.frames(2)
    local armed = verify.road_state()
    verify.expect(armed.armed, "the Track press ARMED rather than building")
    verify.expect(armed.arm_x == from.x and armed.arm_y == from.y,
                  string.format("it armed the selected tile (%d,%d vs %d,%d)",
                                armed.arm_x, armed.arm_y, from.x, from.y))
    verify.expect(armed.arm_tier == 1, "at the Track tier (" .. armed.arm_tier .. ")")
    local got = {}
    for _, h in ipairs(armed.highlights) do
        got[#got + 1] = string.format("%d@%d,%d", h.side, h.x, h.y)
    end
    sorted(got)
    verify.expect(join(got) == join(want),
                  string.format("the highlight is EXACTLY what can_place_road admits:\n"
                                .. "    drawn:    %s\n    admitted: %s", join(got), join(want)))
    verify.expect(armed.drawn > 0,
                  string.format("the canvas outlined them (%d mark(s))", armed.drawn))
    verify.capture("roads_armed_highlights")

    -- ── 2. A press on a highlighted tile lays ONE crossing, charged ONCE ─────
    local edges_before   = armed.edges
    local balance_before = verify.player_balance()
    local target         = armed.highlights[1]
    verify.click_tile(target.x, target.y)
    verify.frames(2)
    local after = verify.road_state()
    verify.expect(not after.armed, "the second press disarmed")
    verify.expect(after.edges == edges_before + 1,
                  string.format("exactly ONE crossing was laid (%d -> %d edges)",
                                edges_before, after.edges))
    local spent = balance_before - verify.player_balance()
    verify.expect(math.abs(spent - track.cost) <= 1.0,
                  string.format("charged ONCE, at the row's own price (%.1f cr spent, row said %.1f)",
                                spent, track.cost))
    verify.expect(after.message:find("Track", 1, true) ~= nil,
                  "the outcome line names what was built: '" .. after.message .. "'")
    verify.capture("roads_crossing_laid")

    -- ── 3. ...and that side is no longer admitted ───────────────────────────
    local left = admitted(from.x, from.y, 1)
    verify.expect(#left == #want - 1,
                  string.format("the crossing just laid left the admitted set (%d -> %d)",
                                #want, #left))
    local laid = string.format("%d@%d,%d", target.side, target.x, target.y)
    for _, e in ipairs(left) do
        verify.expect(e ~= laid, "the laid crossing is not offered again: " .. laid)
    end

    -- ── 4. A press anywhere else DISARMS and lays nothing ───────────────────
    verify.select_tile(from.x, from.y)
    verify.frames(1)
    verify.show_panel("build", true)
    verify.scroll_panel("build_list", 1.0)
    verify.frames(2)
    local rs2 = verify.road_state()
    local track2 = nil
    for _, r in ipairs(rs2.rows) do if r.tier == 1 then track2 = r end end
    if track2 ~= nil and track2.x > 0 then
        verify.click(track2.x, track2.y)
        verify.frames(2)
        local a2 = verify.road_state()
        verify.expect(a2.armed, "armed again")
        local e2 = a2.edges
        -- The armed tile itself is never one of its own targets (a crossing goes
        -- to a NEIGHBOUR), so pressing it is the "anywhere else" case.
        verify.click_tile(from.x, from.y)
        verify.frames(2)
        local a3 = verify.road_state()
        verify.expect(not a3.armed, "a press off the highlights disarmed")
        verify.expect(a3.edges == e2, "...and laid nothing (" .. a3.edges .. " edges)")
    end
    end -- track.x > 0
end

-- ── 5. A tile with nowhere left to go arms and highlights NOTHING ───────────
-- Found by scanning a window with the rules themselves rather than asserted of a
-- hand-picked coordinate: which tiles are landlocked in water (or fully roaded)
-- is a property of the seed, not of this script.
local dead = nil
if from ~= nil then
    for dy = -12, 12 do
        for dx = -20, 20 do
            local a = verify.road_admits(from.x + dx, from.y + dy, 1)
            if a.ok and a.n_sides == 0 then
                dead = { x = from.x + dx, y = from.y + dy }
                break
            end
        end
        if dead ~= nil then break end
    end
end
if dead == nil then
    print("roads: no tile with zero legal crossings within the scanned window - "
          .. "the empty-highlight case is NOT exercised on this seed")
else
    print(string.format("roads: (%d,%d) has no legal crossing at Track", dead.x, dead.y))
    verify.select_tile(dead.x, dead.y)
    verify.center_tile(dead.x, dead.y)
    verify.set_zoom(3.0)
    verify.frames(1)
    verify.show_panel("build", true)
    verify.scroll_panel("build_list", 1.0)
    verify.frames(2)
    local rd = verify.road_state()
    local trow = nil
    for _, r in ipairs(rd.rows) do if r.tier == 1 then trow = r end end
    verify.expect(trow ~= nil, "the ladder drew for a tile with nowhere to cross to")
    if trow ~= nil then
        verify.expect(trow.targets == 0, "the row counts zero crossings")
        verify.expect(trow.explainer:find("Nowhere left", 1, true) ~= nil,
                      "and SAYS so on the press: '" .. trow.explainer .. "'")
        if trow.x > 0 then
            local edges_before = rd.edges
            verify.click(trow.x, trow.y)
            verify.frames(2)
            local da = verify.road_state()
            verify.expect(da.armed, "it still ARMS - the refusal is the absent highlight")
            verify.expect(da.n_highlights == 0, "and highlights NOTHING ("
                          .. da.n_highlights .. ")")
            verify.expect(da.drawn == 0, "nothing was outlined on the canvas")
            verify.capture("roads_armed_nothing")
            verify.click_tile(dead.x, dead.y)
            verify.frames(2)
            verify.expect(verify.road_state().edges == edges_before,
                          "and no crossing was laid")
        end
    end
end
