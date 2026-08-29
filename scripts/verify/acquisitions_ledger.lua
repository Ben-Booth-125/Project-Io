-- Verify the Acquisitions ledger and its profitability fold-out.
--
-- WHAT THIS SCRIPT DOES NOT RELY ON, and why it is written this way:
--
-- `expect_no_clipping` is VACUOUS on this class of surface (NR-663). It reported
-- ZERO records over frames carrying six visibly clipped strings, because table
-- cells and `SmallButton` labels are not instrumented. Its green proves nothing
-- here, so it is called for the record but nothing is concluded from it — the
-- LAYOUT is checked by looking at the captures, and the CONTENT is checked by
-- asserting on the set the ledger computes (`verify.acquisitions_field`).
--
-- THE FIELD SIZE IS MEASURED, NOT ASSUMED — AND IT HAS MOVED BY FIFTY TIMES.
-- `tools/verify/acquisition_viability.cpp` § C, twelve seeds of the shipped
-- spawn: 88 corporations per seed, of which a mean of 81.6 are buyable. It was
-- 1.6 when this script was written, before closure was retired for background
-- firms.
--
-- THAT IS EXACTLY WHY THIS SCRIPT ASSERTS NO FIELD SIZE. It asserts the RULES
-- the field obeys, and it manufactures the two-group case it needs with
-- `verify.set_balance` rather than hoping a seed supplies it — so a fifty-fold
-- change in the world it runs against cost it no edits and produced no false
-- red. A check pinned to the measured number would have failed here and been
-- read as a regression.
--
-- Assertions, in order:
--   A1  No unfiled and no non-public firm is listed. The listed set is exactly
--       the public-and-filed set.
--   A2  Every Purchasable row's price is within the player's balance, and every
--       Possible row's is not. Checked twice at two different balances, so the
--       grouping is shown to FOLLOW the balance rather than to have been right
--       once by luck.
--   A3  Both groups are non-empty at some point in the run, so neither branch of
--       the layout is captured blind.
--   A4  The Buy press TRANSFERS A FIRM: clicked through `verify.click` on the
--       real control, after which the target is gone from the corporation set
--       and gone from the buyable field.
--   A5  The profitability fold-out opens and lists EVERY corporation, not only
--       the buyable ones.
--   A6  The Company lens's click lands HERE, and the Corporation lens's does not.
--       The tile is read off verify.buildings(), never guessed.
--   A7  BINARY FOG (BL-679). No undisclosed firm appears in the profitability
--       table at all, and every row that IS listed carries every operational
--       field — no fog dash survives anywhere in a listed row.
--   A8  THE THREE FILTERS (BL-680) actually narrow. For each of end resource,
--       input resource and body: pick a value PRESENT IN THE DATA, apply it,
--       and assert every surviving row matches it. Swept over every option, not
--       just the first.
--   A9  The filter combos are REACHABLE — driven with verify.click on the real
--       published control (open the combo, then press a real option), not only
--       captured. The two presses are the two a player makes.

verify.goto_surface("home")
verify.window(1720, 980)

-- ---------------------------------------------------------------------------
-- Warm the world so firms have filed. A price reads the target's own returns,
-- so a field measured before anyone has filed is empty by construction and
-- would make every assertion below vacuously true.
-- ---------------------------------------------------------------------------
verify.econ_step(12)

-- show_panel writes ui_state directly rather than through close_all_panels, so
-- the column can otherwise hold two open windows at once (the same fix
-- decision_feed.lua and
-- contracts_ledger.lua both apply to themselves).
verify.show_panel("acquisitions", true)
verify.frames(2)

-- ---------------------------------------------------------------------------
-- A1 — the exclusion. A firm that does not file cannot be priced, so it is not
-- listed at all: not greyed, not shown at zero, absent.
-- ---------------------------------------------------------------------------
local f = verify.acquisitions_field()
assert(f.corps > 0, "no corporations in the world at all — the reading is vacuous")
verify.expect(f.listed == f.public_filed,
    string.format("A1 listed == public-and-filed: %d vs %d", f.listed, f.public_filed))
verify.expect(f.public_filed <= f.public_held,
    string.format("A1 filed <= public: %d of %d", f.public_filed, f.public_held))
for _, row in ipairs(f.rows) do
    verify.expect(row.filed > 0,
        "A1 every listed firm has filed: " .. row.name)
end
print(string.format("field: %d corps, %d public, %d public+filed, %d listed, balance %.0f",
    f.corps, f.public_held, f.public_filed, f.listed, f.balance))

if f.listed == 0 then
    -- A REAL OUTCOME, REPORTED AS ONE. Every seed measured carries buyable
    -- firms — a mean of 81.6 since closure was retired for background firms —
    -- but nothing GUARANTEES it, and this guard was written when the mean was
    -- 1.6 and an empty seed was a live possibility. Keep it: fail loudly rather
    -- than passing on an empty set, because a check that cannot see its subject
    -- has not checked it.
    verify.expect(false, "FIELD EMPTY on this seed - nothing below can be checked")
    return
end

-- Find the cheapest and the dearest listed firm — the two ends the grouping
-- has to separate.
local cheapest, dearest = f.rows[1], f.rows[1]
for _, row in ipairs(f.rows) do
    if row.price < cheapest.price then cheapest = row end
    if row.price > dearest.price  then dearest  = row end
end

-- ---------------------------------------------------------------------------
-- A2 / A3 — the two groups, at a balance chosen to populate BOTH.
--
-- Set the balance between the cheapest and the dearest price where the field is
-- wide enough to have two distinct prices; otherwise park just above the single
-- price so Purchasable is non-empty, and check the empty-Possible layout too.
-- `set_balance` is the only manufactured thing here: the prices, the grouping
-- and the press are all the real seam.
-- ---------------------------------------------------------------------------
local function check_grouping(tag)
    local g = verify.acquisitions_field()
    local n_purch, n_poss = 0, 0
    for _, row in ipairs(g.rows) do
        if row.group == "purchasable" then
            n_purch = n_purch + 1
            verify.expect(row.price <= g.balance, string.format(
                "A2 (%s) Purchasable within balance: '%s' at %.0f, balance %.0f",
                tag, row.name, row.price, g.balance))
        else
            n_poss = n_poss + 1
            verify.expect(row.price > g.balance, string.format(
                "A2 (%s) Possible beyond balance: '%s' at %.0f, balance %.0f",
                tag, row.name, row.price, g.balance))
        end
    end
    print(string.format("  %s: balance %.0f -> %d purchasable, %d possible",
        tag, g.balance, n_purch, n_poss))
    return n_purch, n_poss
end

local seen_purchasable, seen_possible = false, false

if dearest.price > cheapest.price then
    -- A balance between the two ends: at least one of each group by
    -- construction, which is the case the second group exists to show.
    verify.set_balance(cheapest.price + (dearest.price - cheapest.price) * 0.5)
    verify.frames(2)
    local p, q = check_grouping("split")
    seen_purchasable = seen_purchasable or p > 0
    seen_possible    = seen_possible    or q > 0
    verify.capture("acquisitions_ledger_both_groups")
end

-- Everything affordable: the Possible group empties, and the layout has to say
-- "None." rather than collapsing to nothing.
verify.set_balance(dearest.price + 1000.0)
verify.frames(2)
local p1, q1 = check_grouping("all affordable")
seen_purchasable = seen_purchasable or p1 > 0
seen_possible    = seen_possible    or q1 > 0
verify.capture("acquisitions_ledger_all_purchasable")

-- Nothing affordable: the Purchasable group empties and NO Buy press exists.
-- `cheapest.price` can be exactly 0 (the free-firm case FINANCE.md documents),
-- and no balance is below 0 under this rule, so this branch is skipped there
-- rather than asserted falsely.
if cheapest.price > 0.0 then
    verify.set_balance(cheapest.price - 1.0)
    verify.frames(2)
    local p2, q2 = check_grouping("none affordable")
    seen_purchasable = seen_purchasable or p2 > 0
    seen_possible    = seen_possible    or q2 > 0
    verify.expect(p2 == 0, "A2 no row stays Purchasable below the cheapest price")
    local btn = verify.acquisitions_buy_button()
    verify.expect(not btn.ok,
        "A2 no Buy press is drawn with nothing purchasable")
    verify.capture("acquisitions_ledger_none_purchasable")
end

verify.expect(seen_purchasable, "A3 the Purchasable group was non-empty at some point")
verify.expect(seen_possible or dearest.price == cheapest.price,
    "A3 the Possible group was non-empty at some point (or the field has one price)")

-- ---------------------------------------------------------------------------
-- A5 / A7 — the profitability fold-out, under BINARY FOG (BL-679).
--
-- THE RULE CHANGED HERE, and the old assertion would now be wrong. This table
-- used to list every corporation and dash out the cells it could not disclose.
-- Ben's 2026-08-29 ruling made fog binary: a firm the player knows nothing
-- about is not listed AT ALL, and a firm that discloses discloses everything.
-- So what is asserted is the new rule, not the old one — listed == disclosed,
-- and no fog dash anywhere inside a listed row.
--
-- THE TABLE IS NO LONGER SPARSE, AND THE SCRIPT DID NOT HAVE TO CHANGE. When
-- this was written a mean of 1.6 of 88 corporations disclosed, so ~3 rows was
-- the correct outcome of the rule and the note here said so. Retiring closure
-- for background firms took that to a mean of 81.6. Both are correct outcomes
-- of the SAME rule on different data, which is why the script asserts the rule
-- and prints the count rather than asserting a row count.
-- ---------------------------------------------------------------------------
verify.set_balance(dearest.price + 1000.0)
verify.fold("acquisitions_profit", 0)
verify.frames(2)
verify.capture("acquisitions_ledger_profitability")
verify.expect_no_clipping("acquisitions_profitability") -- VACUOUS here (NR-663); recorded, not relied on

local pt = verify.acquisitions_profit()
print(string.format("profit table: %d corps, %d listed, %d undisclosed and excluded",
    pt.corps, pt.listed, pt.undisclosed))

verify.expect(pt.listed + pt.undisclosed == pt.corps, string.format(
    "A7 every corporation is either listed or excluded: %d + %d vs %d",
    pt.listed, pt.undisclosed, pt.corps))
verify.expect(pt.listed > 0,
    "A7 at least one firm discloses - otherwise nothing below can be checked")
verify.expect(pt.undisclosed > 0,
    "A7 at least one firm is EXCLUDED - otherwise the exclusion is vacuous")

-- Every listed row is disclosed by the surface's own predicate: the player's own
-- corp, or a publicly held one. A private or closed firm reaching this table
-- would be the exact defect BL-679 removes.
local n_full, n_genuine = 0, 0
for _, row in ipairs(pt.rows) do
    verify.expect(row.is_player or row.class == "public", string.format(
        "A7 no undisclosed firm is listed: '%s' is %s", row.name, row.class))
    -- A listed row carries its operational fields. `has_input` false is a
    -- GENUINE absence (an extractor consumes nothing), never fog, so it is
    -- counted and reported rather than asserted away.
    verify.expect(row.has_end, string.format(
        "A7 a listed row names its end resource: '%s'", row.name))
    verify.expect(row.has_profit or row.is_player, string.format(
        "A7 a listed non-player row carries its filed profit: '%s'", row.name))
    if row.has_end and row.has_input and row.has_profit and row.has_price then
        n_full = n_full + 1
    else
        n_genuine = n_genuine + 1
    end
end
print(string.format("  %d rows fully populated, %d carry a genuine absence "
    .. "(extractor with no input, or the player's own unpriceable row)",
    n_full, n_genuine))

-- ---------------------------------------------------------------------------
-- A8 — the three filters narrow, swept over EVERY option present in the data.
--
-- The option values are taken from the rows themselves, never from the resource
-- registry: a filter is only meaningful on a value some row actually holds, and
-- asserting against a registry value no row carries would assert on an empty
-- set and pass vacuously.
-- ---------------------------------------------------------------------------
-- The rows keyed by corp id, so a `shown` id can be resolved back to the row it
-- drew and checked against the filter that was set.
local by_corp = {}
for _, row in ipairs(pt.rows) do by_corp[row.corp] = row end

local function sweep(slot, label, values, matches)
    local n, narrowed = 0, 0
    for _, v in ipairs(values) do
        verify.set_acquisitions_filter(slot, v)
        verify.frames(2)
        local g = verify.acquisitions_profit()

        -- ASSERTED AGAINST WHAT THE SURFACE DREW (`shown`), never against a
        -- filter re-applied here. A script that re-derived the rule would grade
        -- its own arithmetic and would keep passing if the table stopped
        -- filtering entirely.
        verify.expect(g.shown_count > 0, string.format(
            "A8 %s = %d is present in the data, so the table keeps a row", label, v))
        verify.expect(g.shown_count <= pt.listed, string.format(
            "A8 %s filter never widens the set: %d shown of %d listed",
            label, g.shown_count, pt.listed))
        for _, corp in ipairs(g.shown) do
            local row = by_corp[corp]
            verify.expect(row ~= nil, string.format(
                "A8 %s: a shown row is one of the disclosed set (corp %d)", label, corp))
            if row then
                verify.expect(matches(row, v), string.format(
                    "A8 %s = %d: every surviving row matches - '%s' does not",
                    label, v, row.name))
            end
        end
        if g.shown_count < pt.listed then narrowed = narrowed + 1 end
        n = n + 1
    end
    verify.set_acquisitions_filter(slot, -1)
    verify.frames(2)
    local g = verify.acquisitions_profit()
    verify.expect(g.shown_count == pt.listed, string.format(
        "A8 %s cleared to 'every' restores the full set: %d vs %d",
        label, g.shown_count, pt.listed))
    print(string.format("  %s: swept %d option(s); %d of them strictly narrowed the table",
        label, n, narrowed))
    return n, narrowed
end

-- Collect the option sets exactly as the surface does — from the listed rows.
local function distinct(fn)
    local seen, out = {}, {}
    for _, row in ipairs(pt.rows) do
        for _, v in ipairs(fn(row)) do
            if not seen[v] then seen[v] = true; out[#out + 1] = v end
        end
    end
    -- Sorted BY HAND. The verify Lua sandbox opens no `table` library, so
    -- `table.sort` is a nil index rather than a sort — lib.lua's tour_buildings
    -- carries the same insertion sort for the same reason and says so. The sort
    -- is for determinism, not tidiness: `distinct` walks the drawn rows, and the
    -- sweep below reports per-option, so an unstable option order would make the
    -- log differ run to run on identical data.
    for i = 2, #out do
        local k, j = out[i], i - 1
        while j >= 1 and out[j] > k do
            out[j + 1] = out[j]
            j = j - 1
        end
        out[j + 1] = k
    end
    return out
end

local end_opts   = distinct(function(r) return r.has_end   and { r.end_res }   or {} end)
local input_opts = distinct(function(r) return r.has_input and { r.input_res } or {} end)
local body_opts  = distinct(function(r) return r.bodies end)

verify.expect(#end_opts > 0, "A8 the end-resource filter has at least one option")
verify.expect(#body_opts > 0, "A8 the body filter has at least one option")

sweep(1, "end resource", end_opts,
      function(row, v) return row.has_end and row.end_res == v end)
sweep(2, "input resource", input_opts,
      function(row, v) return row.has_input and row.input_res == v end)
sweep(3, "body", body_opts, function(row, v)
    for _, b in ipairs(row.bodies) do if b == v then return true end end
    return false
end)

-- The AND combination, and the surface's own words when it yields nothing. Two
-- filters that no single row satisfies together must produce the stated message
-- rather than an empty table with headers.
if #end_opts > 1 then
    verify.set_acquisitions_filter(1, end_opts[1])
    verify.set_acquisitions_filter(2, input_opts[1] or -1)
    verify.frames(2)
    verify.capture("acquisitions_ledger_filtered_and")
    verify.set_acquisitions_filter(1, -1)
    verify.set_acquisitions_filter(2, -1)
    verify.frames(2)
end

-- ---------------------------------------------------------------------------
-- A9 — the filter controls are REACHABLE, driven through verify.click on the
-- real published positions. Two presses, the two a player makes: the first
-- opens the combo (a popup has no position until it is on screen), the second
-- lands on a real option. Asserted by reading the filter state back.
-- ---------------------------------------------------------------------------
for slot, label in ipairs({ "end resource", "input resource", "body" }) do
    verify.set_acquisitions_filter(slot, -1)
    verify.frames(2)

    local c = verify.acquisitions_filter_control(slot)
    verify.expect(c.ok, string.format(
        "A9 the %s combo is drawn and published", label))
    if c.ok then
        verify.click(c.x, c.y)          -- press 1: open the popup
        verify.frames(2)
        verify.capture("acquisitions_ledger_filter_open_" .. slot)

        local o = verify.acquisitions_filter_control(slot)
        verify.expect(o.opt_ok, string.format(
            "A9 the %s combo's first real option is published once open", label))
        if o.opt_ok then
            verify.click(o.opt_x, o.opt_y)   -- press 2: choose it
            verify.frames(2)
            local g = verify.acquisitions_profit()
            local live = (slot == 1) and g.filter_end
                      or (slot == 2) and g.filter_input
                      or g.filter_body
            local unset = (slot == 3) and 0 or -1
            verify.expect(live ~= unset, string.format(
                "A9 clicking the %s option actually SET the filter: now %s",
                label, tostring(live)))
            print(string.format("  %s: click set the filter to %s", label, tostring(live)))
        end
    end
    verify.set_acquisitions_filter(slot, -1)
    verify.frames(2)
end

verify.capture("acquisitions_ledger_filters")
verify.fold()
verify.frames(2)

-- ---------------------------------------------------------------------------
-- A4 — the press actually transfers a firm.
--
-- Clicked through `verify.click` on the button's REAL published position, so
-- what is exercised is the control a player presses and not the seam behind it.
-- The command is enqueued onto pending_order_commands and drained on the next
-- frame, so a frame is pumped before the world is re-read.
-- ---------------------------------------------------------------------------
local before = verify.acquisitions_field()
verify.expect(before.listed > 0, "A4 there is something to press")

local btn = verify.acquisitions_buy_button()
verify.expect(btn.ok, "A4 a Buy button is published and reachable")
if btn.ok then
    local target = btn.corp
    verify.capture("acquisitions_ledger_before_buy")
    verify.click(btn.x, btn.y)
    verify.frames(3)   -- press frame, drain frame, redraw frame

    local after = verify.acquisitions_field()
    verify.expect(after.corps == before.corps - 1, string.format(
        "A4 the press dissolved exactly one firm: %d -> %d corporations",
        before.corps, after.corps))
    for _, row in ipairs(after.rows) do
        verify.expect(row.corp ~= target,
            "A4 the bought firm is gone from the buyable field")
    end
    print(string.format("  buy: %d corps -> %d corps, target %d gone",
        before.corps, after.corps, target))
    verify.capture("acquisitions_ledger_after_buy")
end

-- ---------------------------------------------------------------------------
-- A6 — the ledger's SECOND door: the Company lens's click destination.
--
-- Asserted rather than merely captured. The tile is not guessed: it is read off
-- `verify.buildings()` (which reports `background` for exactly this purpose), so
-- the click lands on a real background firm's holding on the active body. Under
-- the CORPORATION lens the same click must still reach the Balance ledger — the
-- two lenses ask different questions and must land in different places, and a
-- wiring that sent both here would pass a one-sided check.
-- ---------------------------------------------------------------------------
verify.show_panel("acquisitions", false)
verify.goto_surface("home")

local home_body, bg = nil, nil
for _, b in ipairs(verify.buildings()) do
    if b.player then home_body = b.body break end
end
for _, b in ipairs(verify.buildings()) do
    if b.background and b.body == home_body then bg = b break end
end

if bg then
    verify.set_overlay("company")
    verify.frames(2)
    verify.click_tile(bg.x, bg.y)
    verify.frames(2)
    -- `open_panel` is published by pointer_target(), not by a state() reader
    -- (there is none) — it is the same string the BL-603 structure-click checks
    -- assert on, so this door is checked the way every other lens door is.
    local st = verify.pointer_target()
    verify.expect(st.open_panel == "acquisitions", string.format(
        "A6 the Company lens lands on Acquisitions: got '%s'", tostring(st.open_panel)))
    verify.capture("acquisitions_ledger_company_lens")

    -- The other half of the split: the Corporation lens must NOT land here.
    verify.show_panel("acquisitions", false)
    verify.set_overlay("corporation")
    verify.frames(2)
    verify.click_tile(bg.x, bg.y)
    verify.frames(2)
    local st2 = verify.pointer_target()
    verify.expect(st2.open_panel ~= "acquisitions", string.format(
        "A6 the Corporation lens does NOT land on Acquisitions: got '%s'",
        tostring(st2.open_panel)))
else
    -- Reported, not silently skipped: a check that could not find its subject
    -- has not checked anything, and the reader needs to know which it was.
    print("A6 SKIPPED: no background firm holds a building on the home body")
    verify.set_overlay("company")
    verify.show_panel("acquisitions", true)
    verify.frames(2)
    verify.capture("acquisitions_ledger_company_lens")
end
