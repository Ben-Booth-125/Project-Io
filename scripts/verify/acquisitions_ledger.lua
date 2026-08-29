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
-- THE FIELD IS SMALL, AND THAT IS MEASURED, NOT ASSUMED.
-- `tools/verify/acquisition_viability.cpp` § C, twelve seeds of the shipped
-- spawn: 88 corporations per seed, of which one to three are buyable — mean
-- 1.6, seven of twelve at exactly one. So this script does NOT assert a field
-- size; it asserts the RULES the field obeys, and it manufactures the two-group
-- case it needs with `verify.set_balance` rather than hoping a seed supplies it.
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
    -- A REAL OUTCOME, REPORTED AS ONE. Every seed measured so far carried at
    -- least one buyable firm, but the field is one to three firms wide and
    -- nothing guarantees it. Fail loudly rather than passing on an empty set —
    -- a check that cannot see its subject has not checked it.
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
-- A5 — the profitability fold-out. Every corporation, not only the buyable
-- ones: that is the whole reason it earns a full-canvas takeover, and it is the
-- inversion the measurement found (a mean of 1.6 buyable firms against 88 rows
-- of filed returns).
-- ---------------------------------------------------------------------------
verify.set_balance(dearest.price + 1000.0)
verify.fold("acquisitions_profit", 0)
verify.frames(2)
verify.capture("acquisitions_ledger_profitability")
verify.expect_no_clipping("acquisitions_profitability") -- VACUOUS here (NR-663); recorded, not relied on
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
