-- Reusable verify-script helpers — Phase 2 of the visual-verification harness.
--
-- The harness (app::run_verify) auto-loads this file from its own directory before
-- running a verify script, so these helpers are available as globals with no
-- `require` (the Lua `package` library is not opened). They layer high-level,
-- parameterised operations over the low-level `verify` API so a new check is
-- "call a helper with the body/lens you care about" rather than a from-scratch
-- script with hand-computed pans. See docs/development/DEVELOPMENT_PRACTICES.md
-- § Visual verification.

-- Settle, then capture. Use this instead of a bare `verify.capture` after anything
-- that OPENS or RESIZES a window, switches a ledger view, or changes a layout.
--
-- Why (Ben, 2026-08-01, from a real miss): `verify.capture` composites exactly ONE
-- frame, while ImGui settles auto-layout over the following frame or two — a child's
-- content region, a table's column widths and a fresh window's scroll state are all
-- provisional on the frame they first appear. Capturing immediately therefore records
-- a half-laid-out frame, and because that frame is *deterministic* it blesses and
-- re-passes at 0.0000% forever: a stable golden of the wrong picture. Four of the ten
-- BL-214 fold captures moved once settled, which is how this was found.
--
--   name   : capture name, as verify.capture.
--   frames : optional settle frames before the shot (default 4).
function shot(name, frames)
    verify.frames(frames or 4)
    verify.capture(name)
end

-- Frame a Planetary tile: set the zoom and centre the tile using the canvas's own
-- transform (verify.center_tile). Replaces the hand-computed
-- pan = (grid_centre - tile_local) * zoom that early scripts carried.
--   col, row : grid coordinates of the tile to centre.
--   zoom     : optional zoom factor (defaults to the current zoom).
function frame_tile(col, row, zoom)
    verify.center_tile(col, row, zoom)
end

-- Capture the active surface under each overlay lens, for side-by-side comparison.
--   prefix : base name; each capture is "<prefix>_<NN>_<lens>".
--   lenses : optional list of lens names; defaults to the meaningful set
--            (plain terrain, faction territory, corporate ownership).
function sweep_overlays(prefix, lenses)
    lenses = lenses or {"none", "faction", "corporation"}
    for i, lens in ipairs(lenses) do
        verify.set_overlay(lens)
        verify.capture(string.format("%s_%02d_%s", prefix, i - 1, lens))
    end
end

-- Centre and capture ONE REPRESENTATIVE building per (type, ownership) pair,
-- using verify.buildings() so the caller never hand-lists coordinates. Captures
-- are named "building_<player|rival>_<type>".
--
-- This used to capture EVERY building on the map: 146 goldens and 514 MB, 40% of
-- the whole golden set, from this one helper. That is not 146 checks — it is one
-- check run 146 times against different data, because what it verifies is the
-- SILHOUETTE (extraction = faceted chunk, processing = square, port = triangle),
-- and a silhouette does not vary per instance. The cost was real: every UI detail
-- change re-blessed all 146, and 514 MB of PNG churned through git each time.
--
-- Two properties matter in the naming, and neither held before:
--   * STABLE ACROSS GENERATION. The old names carried grid coordinates and corp
--     ids, both of which move when world generation changes — so a generation
--     change orphaned all 146 goldens at once rather than diffing them. Type and
--     ownership survive.
--   * DETERMINISTIC PICK. verify.buildings() walks an unordered_map, so "the
--     first of each type" is only stable if we sort first. Sorted by (x, y).
--
--   zoom : optional zoom for each capture (defaults to a close framing of 20).
function tour_buildings(zoom)
    zoom = zoom or 20

    -- NOTE: the verify Lua sandbox exposes no `table` library, so this sorts by
    -- hand. Both passes below exist for determinism, not tidiness:
    -- verify.buildings() walks an unordered_map, so "the first of each type" is
    -- only reproducible if the winner is chosen by a total order on the data.
    local best, keys, nkeys = {}, {}, 0
    for _, b in ipairs(verify.buildings()) do
        -- Type names arrive human-readable ("Extraction Site"); fold to
        -- lower_snake so a golden filename never carries a space.
        local ty = string.lower(string.gsub(b.type or "unknown", " ", "_"))
        local key = (b.player and "player" or "rival") .. "_" .. ty
        local cur = best[key]
        if cur == nil then
            nkeys = nkeys + 1
            keys[nkeys] = key
            best[key] = b
        elseif b.x < cur.x or (b.x == cur.x and b.y < cur.y) then
            best[key] = b -- lowest (x, y) wins, whatever order the map handed them over in
        end
    end

    -- Insertion sort the keys so the capture ORDER is stable too — a reordered
    -- run would still produce identical goldens, but a stable order keeps the
    -- log readable and any future diff-by-sequence honest.
    for i = 2, nkeys do
        local k, j = keys[i], i - 1
        while j >= 1 and keys[j] > k do
            keys[j + 1] = keys[j]
            j = j - 1
        end
        keys[j + 1] = k
    end

    for i = 1, nkeys do
        local b = best[keys[i]]
        frame_tile(b.x, b.y, zoom)
        verify.capture("building_" .. keys[i])
    end
end

-- Dwell on the exact centre of a Planetary tile (BL-521 click injection).
--
-- Two things a script would otherwise hand-roll: asking the canvas where the tile
-- actually is (verify.tile_screen, which centres it and reports the screen point,
-- so the transform is never re-derived in Lua), and then holding the synthetic
-- cursor there long enough for dwell-gated UI to fire. Dwell is counted in FRAMES
-- on the fixed 1/60 s verify clock — the hover card appears at 30 and sticks at
-- 150 (hover_freeze.lua § phases) — so nothing here depends on wall-clock time.
--
--   col, row : grid coordinates of the tile to hover.
--   frames   : dwell frames (default 31, i.e. just past the appear delay).
-- Returns the screen point { ok, x, y }, so a caller can go on to click it.
function dwell_on_tile(col, row, frames)
    local p = verify.tile_screen(col, row)
    if p.ok then
        verify.hover(p.x, p.y, frames or 31)
    end
    return p
end

-- Stage the shared UI-review world: a campaign with enough happening that every
-- ubiquitous surface has something to show. Used by `ui_shell_fixture.lua` (which
-- saves it) and `shell_pass.lua` (which walks it).
--
-- WHY IT IS A HELPER AND NOT A SAVED SNAPSHOT THE PASS LOADS. That was the first
-- shape, and it does not hold: a world loaded from a snapshot does not RENDER as
-- the world it was saved from. Two reasons, both measured (NR-608, NR-609) —
-- `world::current_day_tick` is re-seeded from the sim loop's own day tick, which
-- `verify.econ_step` never advanced, so the activity fog reads every glimpse as
-- stale and the canvas comes back dimmed; and the comms log is not in the save
-- envelope at all, so the dock comes back empty. Both are visible side by side in
-- `ui_shell_fixture_state.png` against `shell_01_at_rest.png`. Until those are
-- ruled on, a review pass stages its own world rather than reviewing a degraded
-- picture of one.
--
-- Returns { corp = <player corp id>, unit = <a player unit row> }.
-- The player's first unit, or nil. Split out because the fixture now has to ask
-- twice: once before raising a force and once after.
function player_unit(corp)
    for _, u in ipairs(verify.units()) do
        if u.owner == corp then return u end
    end
    return nil
end

-- Raise one unit for `corp` THE WAY A PLAYER WOULD, and return it (nil on failure).
--
-- WHY THIS EXISTS (NR-693/NR-696, 2026-08-28). Until 2026-08-27 every corp was
-- seeded with a military base and a regiment, and this fixture simply asserted
-- the unit was there. Commit dab470dc ("No standing army at spawn") made that
-- seeding opt-in and default OFF, for a good reason: under BL-454 upkeep the
-- seeded regiment cost 7.5 cr/qtr, ~16.5% of the seated corp's outgoings, for a
-- force a new charter never asked for. The fixture's assert then killed all six
-- scripts that use it — 27 of the suite's 66 verify.expect assertions — and
-- said nothing, because the harness had no visible log until NR-694 was fixed.
--
-- Ben chose option C (2026-08-28): reach the state through the REAL VERBS rather
-- than re-enable the seeder behind the game's back. Two properties this buys —
-- the captures depict a world a player can actually reach, and the muster gate
-- (BL-325 S2: hire onto a COMPLETED military_base of your own) gets exercised on
-- every fixture run instead of only in its own harness.
function raise_player_force(corp)
    local VERB_HIRE_UNIT = 8 -- corp_verb::hire_unit

    -- 1. Place the muster base through the same construct_building path a click
    --    uses. build_first_valid picks the first placeable tile on the active
    --    body, so no grid coordinate is hard-coded into the fixture.
    verify.place_mode("military_base")
    local placed = verify.build_first_valid()
    if placed ~= "placed" then
        error("fixture could not place a military base: " .. tostring(placed))
    end

    -- 2. Wait for it. Construction is durative and pay-as-you-build, so the tick
    --    count is a property of the recipe and the corp's cash, not something a
    --    script may assume. Step until the base reports complete, with a bound so
    --    a stalled build fails loudly instead of hanging the suite (the failure
    --    mode NR-695 cost this project two hours).
    local base = nil
    for _ = 1, 40 do
        for _, b in ipairs(verify.buildings()) do
            if b.player and b.type == "Military Base" and b.complete then base = b; break end
        end
        if base then break end
        verify.econ_step(1)
    end
    if not base then
        error("fixture's military base never completed within 40 ticks")
    end

    -- 3. Muster onto it. Which roster rows are available depends on the corp's
    --    stockpile and market access (never on cash — the availability half of
    --    the BL-324 grant), so the row that works is not knowable ahead of time:
    --    try each and take the first the authoritative gate accepts.
    -- 3a. Lend the corp the shortfall, and take the loan back after.
    --
    -- Measured 2026-08-28: with the base paid for, every AVAILABLE roster row
    -- comes back `rejected_funds` (3 available, 13 out of band). Nothing is wrong
    -- with the gate — a hire costs hire_base_cost + hire_cost_per_power x power,
    -- and an opening corp genuinely cannot afford one after building a base. That
    -- is Sprint 21's own finding arriving here: a corp running at a loss never
    -- saves up for anything.
    --
    -- So the fixture lends rather than gifts. The corp pays the REAL cost through
    -- the real verb, and the top-up is subtracted again afterwards, leaving the
    -- balance exactly `opening - actual_spend`. Every other surface — the Header
    -- figure, the Budget ledger's split — therefore still depicts the true opening
    -- economy rather than a staged fortune, which is the whole reason option C was
    -- chosen over re-enabling the seeder.
    local opening = verify.player_balance()
    -- Lent against a FLOOR, not against `opening`: the base is pay-as-you-build,
    -- so by the time it completes the balance can be near zero or negative, and a
    -- relative loan then lands short of hire_base_cost + hire_cost_per_power x
    -- power. Measured 2026-08-28: +500 still gave rejected_funds on all six
    -- available rows. The repayment below subtracts the same LOAN, so the corp
    -- still ends `opening - actual_spend` however large the float is.
    local LOAN = 5000.0
    verify.set_balance(opening + LOAN)

    -- AND THE GOODS, which is the gate that actually bit. hire_unit has TWO
    -- funds checks and they return the same string: `payer.balance < hire_cost`
    -- (credits) and `debit_hire_cost` (goods). The first loan addressed credits
    -- and changed nothing, because an opening corp fails the second — every
    -- gated axis wants hire_axis_cost of one of its candidate resources held in
    -- the corp's own pool, and the measured fixture corp's entire stockpile was
    -- worth Cr 34. One candidate per axis is enough (ore / farm / energy,
    -- unit_roster.hpp's hire_axis_table), and 50 is comfortably over the 5.0 a
    -- single axis costs.
    verify.grant_stock("iron_ore", 50)
    verify.grant_stock("agricultural_produce", 50)
    verify.grant_stock("coal", 50)

    local seen, order = {}, {}
    for row = 0, 15 do
        local r = verify.corp_command{ verb = VERB_HIRE_UNIT, tile = base.tile, unit_type = row }
        if r == "applied" then
            verify.set_balance(verify.player_balance() - LOAN) -- repay
            return player_unit(corp)
        end
        -- Carry the REASONS into the failure. A bare "nothing worked" sent this
        -- fixture round a second build+run cycle for want of one string, which is
        -- the same economy NR-694 was about.
        if not seen[r] then seen[r] = 0; order[#order + 1] = r end
        seen[r] = seen[r] + 1
    end
    -- Joined by hand: the verify sandbox opens no `table` library (see
    -- tour_buildings, which sorts by hand for the same reason).
    local why = ""
    for i, r in ipairs(order) do
        why = why .. (i > 1 and ", " or "") .. r .. " x" .. seen[r]
    end
    verify.set_balance(opening) -- no hire happened; leave the economy untouched
    error("fixture placed a muster base at tile " .. tostring(base.tile)
          .. " but no roster row could be hired onto it: " .. why)
end

function stage_ui_fixture()
    local VERB_ACCEPT_OFFER = 25 -- corp_verb::accept_offer

    verify.goto_surface("home")

    -- Twelve ticks: enough for the header's net figure and sparkline, the Budget
    -- ledger's income/spend split and the market series to have a shape, without
    -- the opening economy having been overtaken by it.
    verify.econ_step(12)

    local corp = nil
    for _, b in ipairs(verify.buildings()) do
        if b.player then corp = b.corp; break end
    end
    assert(corp, "no player-owned building found")

    local unit = player_unit(corp)
    if not unit then unit = raise_player_force(corp) end
    assert(unit, "the fixture could not raise a unit for the player (see raise_player_force)")

    local province = verify.select_province(unit.col, unit.row)
    assert(province ~= 0, "the player's own unit stands on an unpartitioned tile")
    verify.clear_selection()

    local client_of = function(offer_id)
        for _, o in ipairs(verify.offers()) do
            if o.id == offer_id then return o.client end
        end
        return nil
    end
    local accept = function(offer_id)
        return verify.corp_command{
            verb = VERB_ACCEPT_OFFER, corp = corp, order = offer_id,
            counterparty = client_of(offer_id), units = { unit.id },
        }
    end

    -- Content in ALL THREE Contracts views. A capture of two populated tabs
    -- beside an empty third says nothing about whether the third lays out.
    -- (a) History: a one-tick deadline, accepted for real, then stepped past it
    --     so the real tick pass settles it into a terminal state.
    local terminal = verify.inject_offer{ province = province, fee = 400, deadline_in = 1 }
    assert(terminal ~= 0, "inject_offer could not resolve a client nation")
    accept(terminal)
    verify.econ_step(2)
    -- (b) Active: a long deadline, accepted and left running.
    accept(verify.inject_offer{ province = province, fee = 900, deadline_in = 400 })
    -- (c) Offers: two left open, with different fees and deadlines so the table's
    --     own columns are distinguishable rather than three copies of one row.
    verify.inject_offer{ province = province, fee = 650,  deadline_in = 240 }
    verify.inject_offer{ province = province, fee = 1250, deadline_in = 90 }

    verify.clear_selection()
    verify.set_overlay("none")
    verify.frames(3)

    return { corp = corp, unit = unit }
end
