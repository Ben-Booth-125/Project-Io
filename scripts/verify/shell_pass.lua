-- The shell pass: every UBIQUITOUS UI surface, captured from one staged world.
--
-- WHAT THIS IS FOR. Not a regression gate — a design review. It walks the chrome
-- that is on screen in every session (profile tile, header strip, time panel, nav
-- rail, fold-out column, comms dock, selection band, minimap at each rung, the
-- lens legend, the hover card, the system menu) plus each of the THIRTEEN
-- nav-rail ledgers, so each one can be looked at and its authority doc reconciled
-- against what actually renders.
--
-- IT STAGES ITS OWN WORLD, and that is a deliberate reversal. The first shape
-- loaded the snapshot `ui_shell_fixture.lua` writes — Ben's own sequence, and the
-- faster one. It does not hold: a loaded world does not RENDER as the world it
-- was saved from. The canvas comes back dimmed because the activity fog reads a
-- day tick the load re-seeded to the sim loop's (NR-608), and the comms dock comes
-- back empty because the chat log is not in the envelope (NR-609). Reviewing a
-- layout against a degraded picture of it is worse than paying the generate cost.
-- `ui_shell_fixture.lua` still makes the save and captures both sides of the round
-- trip, which is where that evidence lives; when the two are ruled on, this script
-- goes back to a one-line load.
--
--   ProjectIo --verify scripts/verify/shell_pass.lua
--
-- ORDERING. `show_panel` writes ui_state directly rather than routing through
-- `close_all_panels` the way a real rail press does, so two ledgers can be open
-- at once and the column will stack them. Every ledger below is therefore closed
-- explicitly before the next is opened — `ledger()` does that for you.

verify.window(1280, 720)

local staged = stage_ui_fixture()
print("SHELL state_hash=" .. verify.state_hash())

-- Every panel this pass touches, so `ledger()` can guarantee a clean column.
local PANELS = {
    "corporation", "balance", "construction", "tech_tree", "acquisitions",
    "market", "corporations_table", "tile", "generation_ledger", "decisions",
    "strategy", "contracts", "build",
}

local function close_all()
    for _, p in ipairs(PANELS) do verify.show_panel(p, false) end
    verify.frames(1)
end

--- Open one ledger alone and capture it.
---   panel : show_panel name
---   name  : capture name
---   view  : optional panel_view index
local function ledger(panel, name, view)
    close_all()
    verify.show_panel(panel, true)
    if view ~= nil then verify.panel_view(panel, view) end
    shot(name)
end

-- ===========================================================================
-- 1. The shell at rest — what a player meets
-- ===========================================================================
close_all()
verify.clear_selection()
shot("shell_01_at_rest")

-- ===========================================================================
-- 2. The always-on chrome, one variation each
-- ===========================================================================

-- 2a. The launch screen. `show_menu` re-enters it (its own binding says so) —
--     it is NOT the in-session system menu, which is 2b. Captured because the
--     wizard is the first thing a player ever sees.
verify.show_menu(true)
shot("shell_02_main_menu")
verify.show_menu(false)
verify.frames(1)

-- 2b. The in-session system menu: the header-corner popup holding save, load,
--     options and quit. The only way out of a session, so it is chrome even
--     though it is normally closed.
verify.show_panel("system_menu", true)
shot("shell_02b_system_menu")
verify.show_panel("system_menu", false)
verify.frames(1)

-- 2b. The hover card over canvas content. The card is dwell-gated (BL-200), so
--     the hover must be HELD for frames rather than sampled once — `hover`'s
--     third argument is how many frames the pointer stays put.
verify.hover(640, 300, 60)
shot("shell_03_hover_card_glance", 2)

-- 2c. The CLICK-opened card, which is a different surface from the dwell card
--     above and reached by a different gesture (sticky_card.lua § header: a
--     single click opens the full per-kind layout; dwell only ever gives the
--     terse two-line glance). Measured while writing this: a 160-frame dwell
--     produces the SAME two lines as a 60-frame one, so the two states the
--     tooltip doc describes are appear-vs-nothing, not glance-vs-detail.
local tp = verify.tile_screen(staged.unit.col, staged.unit.row)
if tp.ok then
    verify.click(tp.x, tp.y)
    shot("shell_03b_click_card", 3)
end

-- 2c. A nav-rail slot's own tooltip: the only place a slot explains itself.
verify.hover(27, 121, 60)
shot("shell_04_navrail_tooltip", 2)
verify.hover(640, 690, 2) -- park the pointer off the rail again

-- 2d. The time panel under a running speed rather than paused. The speed row is
--     the one piece of chrome whose ACTIVE state is the thing being read.
verify.command("speed_3")
shot("shell_05_time_running")
verify.command("pause_toggle")
verify.frames(1)

-- ===========================================================================
-- 3. The selection band — the three kinds a player meets most
-- ===========================================================================
-- It was FOUR until BL-598. The fourth was the PROVINCE, and it is gone as a
-- kind: the province folded into the tile element as three of its five accordion
-- sections, so `verify.select_province` and `verify.select_tile` now stage the
-- same selection and the two captures would have been the same picture. The
-- tile capture below is where the province is now read.
-- The band never hides (BL-266): with nothing selected it rests on the player's
-- own corporation, which is the first capture below rather than an empty frame.
close_all()

verify.clear_selection()
shot("shell_06_selection_none")

-- The staging already resolved both, from the buildings()/units() readers rather
-- than from a hard-coded id, so a generation change moves the subject instead of
-- silently invalidating the capture.
local mine = staged.unit

verify.select_tile(mine.col, mine.row)
shot("shell_07_selection_tile")

-- The player's own building, then a rival's — the pair that carries the
-- visibility rule (BL-068): full detail on one, `private` placeholders on the other.
-- `buildings()` rows carry (x, y) rather than an id, and `select_building`
-- takes the same pair — so the two compose without a lookup table.
local own_building, rival_building = nil, nil
for _, b in ipairs(verify.buildings()) do
    if b.player and own_building == nil then own_building = b end
    if (not b.player) and rival_building == nil then rival_building = b end
end
if own_building then
    verify.select_building(own_building.x, own_building.y)
    shot("shell_09_selection_own_building")
end
if rival_building then
    verify.select_building(rival_building.x, rival_building.y)
    shot("shell_10_selection_rival_building")
end

verify.clear_selection()
verify.frames(1)

-- ===========================================================================
-- 4. The minimap, at each rung of the zoom ladder
-- ===========================================================================
-- The minimap is chrome at every rung but shows a different thing at each, so a
-- single capture of it says nothing about the other two.
verify.goto_surface("home")
shot("shell_11_minimap_planetary")
verify.command("ascend")
shot("shell_12_minimap_circumplanetary")
verify.command("ascend")
shot("shell_13_minimap_solar")
verify.command("descend")
verify.command("descend")
verify.frames(2)

-- ===========================================================================
-- 5. The lens legend — the always-on key for the canvas's colour code
-- ===========================================================================
verify.set_overlay("market")
shot("shell_14_lens_legend_market")
verify.set_overlay("population")
shot("shell_15_lens_legend_population")
verify.set_overlay("none")
verify.frames(1)

-- ===========================================================================
-- 6. The thirteen nav-rail ledgers, one capture each
-- ===========================================================================
-- Slot 7 (Corp. Strategy) is the one reserved slot: it has no panel to open, so
-- it appears in the rail captures above and nowhere here. That absence is the
-- accurate record, not a gap.
ledger("corporation",        "shell_20_slot01_corporation")
ledger("balance",            "shell_21_slot02_budget")
ledger("construction",       "shell_22_slot03_construction")
ledger("tech_tree",          "shell_23_slot04_research")
ledger("acquisitions",       "shell_24_slot05_acquisitions")
ledger("market",             "shell_25_slot06_market")
ledger("corporations_table", "shell_26_slot08_diplomacy_corps")
ledger("tile",               "shell_27_slot09_history")
ledger("generation_ledger",  "shell_28_slot10_generation")
ledger("decisions",          "shell_29_slot11_ai_decisions")
ledger("strategy",           "shell_30_slot12_strategy")
ledger("contracts",          "shell_31_slot13_contracts_offers", 0)

-- The Contracts ledger's other two views. The fixture stages content in all
-- three deliberately (see its header), so an empty tab here is a finding rather
-- than a fixture accident.
verify.panel_view("contracts", 1)
shot("shell_32_slot13_contracts_active")
verify.panel_view("contracts", 2)
shot("shell_33_slot13_contracts_history")

close_all()
verify.frames(1)

-- ===========================================================================
-- 7. The clipping ledger's verdict over everything above
-- ===========================================================================
-- Free, and it is the one assertion this pass can make: every string drawn in
-- the frames above either fitted its container or was sanctioned as elided.
verify.expect_no_clipping("shell_pass")
