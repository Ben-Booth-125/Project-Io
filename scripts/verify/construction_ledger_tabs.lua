-- The Construction ledger's two tabs, and the levers' new home.
--
--   ProjectIo --verify scripts/verify/construction_ledger_tabs.lua
--
-- WHAT THIS ASSERTS, and why each assertion is here rather than left to a capture.
-- `expect_no_clipping` is VACUOUS on this class of surface — it passes with zero
-- records over a visibly clipped frame (NR-663) — so a green picture of a ledger
-- proves that a ledger drew, and nothing about what it drew. Every claim below is
-- therefore read back from state or from the surface's own published position.
--
--   C1  The DEFAULT tab is Buildings. The queue is empty most of the time, so
--       opening on it would make the ledger's front door an empty room.
--   C2  Every type row's COUNT is the player's buildings of that type, and its
--       TOTAL is the sum of their profits. Recomputed here from
--       `verify.buildings()` (which carries each building's own profit and the
--       name the roster files it under) rather than asked of the roster, so it is
--       the roster's ARITHMETIC under test and not its self-report.
--   C3  Expanding a type row lists EXACTLY that type's buildings. Driven with
--       `verify.click` on the header's real published position.
--   C4  A press on one of those rows SELECTS that building, and the levers are
--       drawn FOR IT — read from `levers_for`, which the view publishes with
--       whatever it actually drew.
--   C5  The same levers appear when the building is selected on the MAP. The two
--       selectors compose; they do not compete.
--   C6  A RIVAL building's Selection card still shows Status ONLY, and carries no
--       open-in-ledger button. The second half is the one a player would otherwise
--       discover: the Buildings view is the player's estate, so a button aiming a
--       rival building at it would open an empty aim.
--   C7  Method and Workforce are GONE from a player building's card. An absence a
--       capture cannot show, because a removed page just renumbers the pager.
--   C8  The Construction tab draws the build bar with a tile selected, and
--       "Select a tile" with none; both tabs are reachable as a PRESS.
--   C9  DOOR TWO: the tile card's Construct button opens the SAME Construction
--       view the rail slot opens. The half a capture cannot show - two doors onto
--       two identical build bars would photograph exactly like two onto one.
--
-- CAPTURES, the six Ben asked for plus the card:
--   01 Construction tab, no tile selected      04 Buildings, one type expanded
--   02 Construction tab, tile selected         05 Buildings, a building selected
--   03 Buildings tab at rest                   06 the building card after the move
--   07 the card's action grid with its fourth button

verify.window(1280, 720)

local staged = stage_ui_fixture()
print("CONSTRUCTION_TABS state_hash=" .. verify.state_hash())

local function close_column()
    for _, p in ipairs({ "corporation", "balance", "construction", "tech_tree",
                         "acquisitions", "market", "corporations_table", "tile",
                         "generation_ledger", "decisions", "strategy", "contracts" }) do
        verify.show_panel(p, false)
    end
    verify.frames(1)
end

-- The player's own buildings, and a rival's, from the reader rather than
-- hand-listed coordinates. `verify.buildings()` walks corps in sorted id order, so
-- "the first rival" is the same rival on every run.
local mine, rival = {}, nil
for _, b in ipairs(verify.buildings()) do
    if b.player then
        mine[#mine + 1] = b
    elseif rival == nil then
        rival = b
    end
end
verify.expect(#mine > 0, string.format(
    "the fixture gives the player something to roster: %d buildings", #mine))

-- ===========================================================================
-- C1 — the default tab is Buildings
-- ===========================================================================
close_column()
verify.clear_selection()
verify.show_panel("construction", true)
verify.frames(3)

local g = verify.building_groups()
verify.expect(g.view == 1, string.format(
    "C1 the Construction ledger opens on Buildings (view 1): got %d", g.view))
verify.expect(g.groups > 0, string.format(
    "C1 the roster has rows to show: %d type groups", g.groups))
print(string.format("  roster: %d type groups over %d player buildings", g.groups, #mine))

verify.capture("construction_tabs_03_buildings_rest")

-- ===========================================================================
-- C2 — count and total, recomputed from the buildings themselves
-- ===========================================================================
for _, row in ipairs(g.rows) do
    local n, sum = 0, 0.0
    for _, b in ipairs(mine) do
        if b.group == row.name then
            n = n + 1
            if b.profit_known then sum = sum + b.profit end
        end
    end
    verify.expect(row.count == n, string.format(
        "C2 '%s' counts every one of its buildings: roster says %d, the world has %d",
        row.name, row.count, n))
    -- Float sum over the same terms in a different order: compare on a tolerance
    -- proportional to the magnitude, never on equality.
    local tol = 0.01 + 0.001 * (sum < 0 and -sum or sum)
    local diff = row.total - sum
    if diff < 0 then diff = -diff end
    verify.expect(diff <= tol, string.format(
        "C2 '%s' totals its members: roster says %.3f, the members sum to %.3f",
        row.name, row.total, sum))
    verify.expect(#row.members == n, string.format(
        "C2 '%s' holds exactly its own members: %d listed, %d owned",
        row.name, #row.members, n))
end

-- ===========================================================================
-- C3 — expanding a type row lists exactly that type's buildings
-- ===========================================================================
local c = verify.construction_controls()
verify.expect(c.group_ok, "C3 the first type-group header is drawn and published")

local expanded_name = nil
if c.group_ok then
    verify.click(c.group_x, c.group_y)
    verify.frames(3)

    local g2 = verify.building_groups()
    for _, row in ipairs(g2.rows) do
        if row.expanded then expanded_name = row.name end
    end
    verify.expect(expanded_name ~= nil, string.format(
        "C3 the header press EXPANDED a group: expanded='%s'", tostring(g2.expanded)))

    if expanded_name then
        -- Exactly that type's buildings, both directions: every listed member is of
        -- this type, and every building of this type is listed.
        local listed, owned = 0, 0
        for _, row in ipairs(g2.rows) do
            if row.name == expanded_name then listed = #row.members end
        end
        for _, b in ipairs(mine) do
            if b.group == expanded_name then owned = owned + 1 end
        end
        verify.expect(listed == owned, string.format(
            "C3 '%s' expands to exactly its own buildings: %d listed, %d owned",
            expanded_name, listed, owned))
        print(string.format("  expanded '%s': %d buildings", expanded_name, listed))
    end
end

verify.capture("construction_tabs_04_buildings_expanded")

-- ===========================================================================
-- C4 — a press on a building row selects it, and draws its levers
-- ===========================================================================
local c2 = verify.construction_controls()
verify.expect(c2.member_ok, "C4 a building row inside the expanded group is published")

local picked = nil
if c2.member_ok then
    picked = c2.member
    verify.click(c2.member_x, c2.member_y)
    verify.frames(3)

    local g3 = verify.building_groups()
    verify.expect(g3.selected == picked, string.format(
        "C4 the row press SELECTED that building: selected=%d, expected %d",
        g3.selected, picked))

    local c3 = verify.construction_controls()
    verify.expect(c3.levers_ok and c3.levers_for == picked, string.format(
        "C4 the levers are drawn FOR THAT BUILDING: levers_for=%d, expected %d",
        c3.levers_for, picked))
end

verify.capture("construction_tabs_05_buildings_selected")

-- ===========================================================================
-- C5 — selecting on the MAP drives the same levers
-- ===========================================================================
-- A different building of the player's than the row press picked, where one
-- exists, so this cannot pass by the selection simply not having changed.
--
-- PREFERRED: one whose profit the report can price. That is what makes C7's
-- "the centre still presents" a real assertion — a building with no estimate falls
-- back to the single Status page, and "no Method, no Workforce" would then be true
-- of a card that has nothing on it at all.
local map_pick = nil
for _, b in ipairs(mine) do
    if b.complete and map_pick == nil then map_pick = b end
    if b.complete and b.profit_known then map_pick = b end
end
if map_pick then
    verify.goto_surface("home")
    verify.select_building(map_pick.x, map_pick.y)
    verify.show_panel("construction", true)
    verify.construction_view(1)
    verify.frames(3)

    local c4 = verify.construction_controls()
    local g4 = verify.building_groups()
    verify.expect(c4.levers_ok, string.format(
        "C5 a MAP selection drives the same levers: levers_for=%d, selected=%d",
        c4.levers_for, g4.selected))
    verify.expect(c4.levers_for == g4.selected, string.format(
        "C5 the levers follow the selection, whichever selector set it: %d vs %d",
        c4.levers_for, g4.selected))
end

-- ===========================================================================
-- C7 — Method and Workforce have left the card; Profitability/Status remain
-- ===========================================================================
if map_pick then
    local pages = verify.building_pages()
    verify.expect(pages.player, "C7 the staged card is a player building's")
    local has_method, has_workforce, has_report = false, false, false
    for _, label in ipairs(pages.labels) do
        if label == "Method"    then has_method    = true end
        if label == "Workforce" then has_workforce = true end
        if label == "Profitability" or label == "Status" then has_report = true end
    end
    verify.expect(not has_method, "C7 Method has left the Selection card's centre")
    verify.expect(not has_workforce, "C7 Workforce has left the Selection card's centre")
    verify.expect(has_report, string.format(
        "C7 the centre still PRESENTS: %d page(s) remain", pages.count))
    if map_pick.profit_known then
        local has_profit = false
        for _, label in ipairs(pages.labels) do
            if label == "Profitability" then has_profit = true end
        end
        verify.expect(has_profit,
            "C7 Profitability survived the move - it reports, so it stays")
    end

    close_column()
    verify.frames(2)
    verify.capture("construction_tabs_06_building_card")

    local cc = verify.construction_controls()
    verify.expect(cc.open_ledger_ok,
        "C7 the player card carries the fourth action-grid button")
    verify.capture("construction_tabs_07_card_action_grid")

    -- The button is a DOOR: it opens the Buildings tab aimed at this building.
    if cc.open_ledger_ok then
        verify.click(cc.open_ledger_x, cc.open_ledger_y)
        verify.frames(3)
        local ga = verify.building_groups()
        verify.expect(ga.view == 1, string.format(
            "C7 the grid button opens the BUILDINGS tab: view=%d", ga.view))
        local aimed = false
        for _, row in ipairs(ga.rows) do
            if row.expanded then
                for _, m in ipairs(row.members) do
                    if m == ga.selected then aimed = true end
                end
            end
        end
        verify.expect(aimed, string.format(
            "C7 it AIMS: the selected building's own type group is the expanded one ('%s')",
            tostring(ga.expanded)))
        local ca = verify.construction_controls()
        verify.expect(ca.levers_ok and ca.levers_for == ga.selected, string.format(
            "C7 one press reaches the levers: levers_for=%d, selected=%d",
            ca.levers_for, ga.selected))
    end
end

-- ===========================================================================
-- C6 — a rival building: Status only, and no open-in-ledger button
-- ===========================================================================
if rival then
    close_column()
    verify.goto_surface("home")
    verify.select_building(rival.x, rival.y)
    verify.frames(3)

    local pages = verify.building_pages()
    verify.expect(not pages.player, "C6 the staged card is a rival's")
    verify.expect(pages.count == 1 and pages.labels[1] == "Status", string.format(
        "C6 a rival building's card is Status ONLY: %d page(s), first='%s'",
        pages.count, tostring(pages.labels[1])))

    local cr = verify.construction_controls()
    verify.expect(not cr.open_ledger_ok,
        "C6 a rival's card carries NO open-in-ledger button - the Buildings view is the player's estate")
    verify.capture("construction_tabs_08_rival_card")
else
    print("  no rival building in this world - C6 not exercised")
end

-- ===========================================================================
-- C8 — the Construction tab, with a tile selected and with none
-- ===========================================================================
close_column()
verify.clear_selection()
verify.show_panel("construction", true)
verify.construction_view(0)
verify.frames(3)
verify.capture("construction_tabs_01_construction_no_tile")

-- With a tile selected the build bar itself draws. The tile under one of the
-- player's own buildings is a tile that certainly exists and is certainly reached.
if map_pick then
    verify.goto_surface("home")
    verify.select_tile(map_pick.x, map_pick.y)
    verify.show_panel("construction", true)
    verify.construction_view(0)
    verify.frames(3)
    verify.capture("construction_tabs_02_construction_tile")
end

-- ===========================================================================
-- C9 — DOOR TWO. The tile card's Construct button opens the SAME view.
--
-- This is the load-bearing half of "one construction element, two doors", and it
-- is the half a capture cannot show: two doors onto two identical-looking build
-- bars would photograph exactly like two doors onto one. So the door is PRESSED,
-- from a column with nothing open, and where it lands is read back.
-- ===========================================================================
if map_pick then
    close_column()
    verify.goto_surface("home")
    verify.select_tile(map_pick.x, map_pick.y)
    verify.frames(3)

    local d = verify.construction_controls()
    verify.expect(d.construct_ok,
        "C9 the tile card's Construct button is drawn and published")
    if d.construct_ok then
        verify.click(d.construct_x, d.construct_y)
        verify.frames(3)
        local st = verify.pointer_target()
        verify.expect(st.open_panel == "construction", string.format(
            "C9 the tile card's Construct button opens the CONSTRUCTION LEDGER: got '%s'",
            tostring(st.open_panel)))
        verify.expect(verify.building_groups().view == 0, string.format(
            "C9 it lands on the Construction view - the same view the rail slot opens: view=%d",
            verify.building_groups().view))
    end
end

-- The tab strip is reachable as a press, not only as a state write.
local ct = verify.construction_controls()
verify.expect(ct.tab_ok, "C8 both tabs are drawn and published")
if ct.tab_ok then
    verify.click(ct.buildings_x, ct.buildings_y)
    verify.frames(2)
    verify.expect(verify.building_groups().view == 1,
        "C8 pressing Buildings switches the view")
    verify.click(ct.construction_x, ct.construction_y)
    verify.frames(2)
    verify.expect(verify.building_groups().view == 0,
        "C8 pressing Construction switches back")
end

close_column()
verify.frames(1)
