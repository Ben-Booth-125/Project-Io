-- The Corporations ledger as a DIPLOMACY surface: the named field only, grouped
-- by stance, with the four stance presses moved into each row's action strip.
--
--   A1  POPULATION. The ledger lists corporations, not background firms. Every
--       corp with `is_background` is absent; every corp without it is present.
--   A2  GROUPING. Each drawn row's group agrees with the stance tables read
--       independently — hostile in EITHER direction, then friends, then neutral.
--   A3  DIRECTION. A hostile row records which way the hostility runs, and the
--       two directions are read separately (`is_hostile` is directional).
--   A4  THE PRESS MOVES THE ROW. A real press on Declare Hostile, through its
--       confirm, moves a row out of Neutral and into Hostile.
--
-- WHY THE PRESSES ARE INJECTED RATHER THAN CAPTURED, and why this script does not
-- lean on `expect_no_clipping`: that check is VACUOUS on this class of surface.
-- It reported "PASS: 0 failure(s), 0 records" on 2026-08-29 over frames holding
-- six visibly clipped strings, because table cells and button labels are not
-- instrumented into the overflow ledger (NR-663). So it is not the gate here.
-- Instead every press is aimed at the rect the PANEL ITSELF reported for that
-- control (`verify.corp_panel_rows`) and is required to change the world. ImGui's
-- hit-test rejects a press outside the window's clip rect, so a control laid out
-- past the fold-out column's edge FAILS this script rather than passing a
-- screenshot. That is the exact failure mode the old five-column table shipped
-- with ("Declare Hostile" clipped to "De...").
--
-- WHAT THIS STILL DOES NOT PROVE. It is a synthesised press, not a human one. It
-- says the control is where the panel says it is and that pressing there works;
-- only a person at the app can say the surface READS right. LOOK AT THE CAPTURES:
--   * Every firm's NAME is legible in full. A name cut to one letter is the
--     original defect and is a fail no assertion here will catch.
--   * Three group headers are present — Friends / Hostile / Neutral — each with a
--     count, and an EMPTY one says so in words rather than vanishing.
--   * No Reach and no Share column. No band word anywhere (Negligible / Minor /
--     Notable / Major / Dominant); those are retired.
--   * The action strip's buttons span the column and are not clipped at the right.
--
-- Run:     ProjectIo --verify scripts/verify/diplomacy_groups.lua
-- Inspect: screenshots/diplomacy_groups*.png

verify.window(1280, 720)

-- Tick first, so the world is a running one rather than the generation instant.
verify.econ_step(6)

verify.show_panel("corporations_table", true)
verify.frames(2)

verify.capture("diplomacy_groups_open")

-- ---------------------------------------------------------------- A1 population

local corps = verify.corps()
local rows  = verify.corp_panel_rows()

local total, background, expected_listed = 0, 0, 0
for _, c in ipairs(corps) do
    total = total + 1
    if c.is_background and not c.is_player then
        background = background + 1
    else
        expected_listed = expected_listed + 1
    end
end

print(string.format(
    "[diplomacy_groups] world holds %d corporations: %d background, %d named; ledger drew %d rows",
    total, background, expected_listed, #rows))

verify.expect(#rows > 0, "A1: the ledger drew at least one row")

-- Guard against a vacuous pass. If the fixture happened to contain no background
-- firm, "no background firm is listed" would be true of an unfiltered panel too.
verify.expect(background > 0,
    "A1: the fixture actually CONTAINS background firms (" .. background ..
    ") — otherwise the exclusion below proves nothing")

local leaked = 0
for _, r in ipairs(rows) do
    if r.is_background then
        leaked = leaked + 1
        print("[diplomacy_groups] background firm listed: " .. r.name)
    end
end
verify.expect(leaked == 0, "A1: no background firm appears in the list (" .. leaked .. " did)")

verify.expect(#rows == expected_listed,
    "A1: every named corporation IS listed — drew " .. #rows ..
    ", expected " .. expected_listed)

-- --------------------------------------------------------------- A2/A3 grouping

local function expected_group(r)
    if r.is_player then return "neutral" end
    -- Hostility first, and in BOTH directions: `is_hostile` is directional, and a
    -- declaration made ON the player groups the row exactly as one made BY them.
    if r.hostile_out or r.hostile_in then return "hostile" end
    if r.friends then return "friends" end
    return "neutral"
end

local group_count = { friends = 0, hostile = 0, neutral = 0 }
local mismatches = 0
for _, r in ipairs(rows) do
    local want = expected_group(r)
    group_count[r.group] = (group_count[r.group] or 0) + 1
    if r.group ~= want then
        mismatches = mismatches + 1
        print(string.format("[diplomacy_groups] %s grouped '%s', stance tables say '%s'",
            r.name, tostring(r.group), want))
    end
end
verify.expect(mismatches == 0,
    "A2: every listed row's group matches its stance tables (" .. mismatches .. " disagreed)")

print(string.format("[diplomacy_groups] groups: friends=%d hostile=%d neutral=%d",
    group_count.friends, group_count.hostile, group_count.neutral))

-- ------------------------------------------------------------ A4 the press moves

-- Pick a neutral, non-player row to declare against.
local target = nil
for _, r in ipairs(rows) do
    if not r.is_player and r.group == "neutral" then target = r break end
end
verify.expect(target ~= nil, "A4: there is a neutral row to declare hostility against")

if target ~= nil then
    print("[diplomacy_groups] declaring hostility toward " .. target.name)

    -- 1. Open the row's action strip. The caret is a disclosure toggle, so the
    --    press must both land and be observable as `expanded`.
    verify.click(target.caret_x, target.caret_y)
    verify.frames(2)

    local function row_for(corp)
        for _, r in ipairs(verify.corp_panel_rows()) do
            if r.corp == corp then return r end
        end
        return nil
    end

    local t = row_for(target.corp)
    verify.expect(t ~= nil and t.expanded,
        "A4: pressing the row's caret opened its action strip")
    verify.expect(t ~= nil and t.declare_x > 0.0,
        "A4: the open strip drew a Declare Hostile press")

    verify.capture("diplomacy_groups_row_actions")

    -- 2. Declare Hostile. It must NOT apply on its own — it raises the confirm.
    if t ~= nil and t.declare_x > 0.0 then
        verify.click(t.declare_x, t.declare_y)
        verify.frames(2)

        t = row_for(target.corp)
        verify.expect(t ~= nil and t.confirm_x > 0.0,
            "A4: Declare Hostile raises its confirm rather than applying")
        verify.expect(t ~= nil and t.group == "neutral",
            "A4: the row has NOT moved yet — the confirm is real friction, not decoration")

        verify.capture("diplomacy_groups_confirm")

        -- 3. Confirm. Now the world changes and the row must move groups.
        if t ~= nil and t.confirm_x > 0.0 then
            verify.click(t.confirm_x, t.confirm_y)
            verify.frames(3) -- the command drains in render(); allow it a frame to apply

            t = row_for(target.corp)
            verify.expect(t ~= nil, "A4: the row is still listed after the declaration")
            verify.expect(t ~= nil and t.hostile_out,
                "A4: the stance table now records the player's hostility")
            verify.expect(t ~= nil and t.group == "hostile",
                "A4: the row MOVED from Neutral into Hostile (now '" ..
                tostring(t and t.group) .. "')")

            verify.capture("diplomacy_groups_after_declare")
        end
    end
end

-- Run for completeness and for the record, NOT as the gate. See the header: this
-- check cannot see a clipped table cell or button label (NR-663), so a green row
-- from it is evidence of nothing on this surface. The presses above are the gate.
verify.expect_no_clipping("diplomacy_groups")
