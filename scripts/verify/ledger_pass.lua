-- The ledger pass: every nav-rail ledger SUB-VIEW, and the foot of every ledger
-- that scrolls, captured from one staged world.
--
-- WHY THIS EXISTS BESIDE shell_pass.lua. `shell_pass` opens each of the thirteen
-- slots once and captures its DEFAULT view. That is the right instrument for "does
-- every slot open"; it is the wrong one for a design review of the ledger class,
-- because most of what a ledger holds is not on its default view:
--   * the Balance ledger's Assets block and its top-buildings ranking sit BELOW THE
--     FOLD at 720p and were captured by nothing (`budget_ledger_ranked` is named for
--     the ranking and its two frames do not contain it);
--   * Market and History each carry three or four views, of which shell_pass
--     sees one;
--   * the Research mock carries one era per tab and the campaign's era is not the
--     first.
-- A review that only ever looks at the resting frame reports on a fraction of the
-- surface and reads as if it covered all of it.
--
--   ProjectIo --verify scripts/verify/ledger_pass.lua
--
-- ORDERING, same trap as shell_pass: `show_panel` writes ui_state directly rather
-- than routing through `close_all_panels` the way a rail press does, so two ledgers
-- can be open at once and the column stacks them. `ledger()` closes everything first.
--
-- SCROLL IS STICKY and lands on the FOLLOWING frame (verify_api.cpp § scroll_panel),
-- so `foot()` renders before capturing and resets to 0 afterwards — otherwise the
-- next ledger inherits the request and a capture named for a head shows a foot.

-- 1920x1080, AND THAT IS THE POINT OF THE LINE (Ben, 2026-08-29): "it seems your
-- captures are not at 1920x1080p, so what we see here is zoomed compared to my
-- viewing. In other words, the UI doesn't shrink at smaller resolutions."
--
-- He is right, and the arithmetic says why the difference is all vertical.
-- `shell_column_width` is 0.20 * disp_x clamped to [380, 460] — 380 px at 1280,
-- 384 px at 1920, four pixels across the whole common range. But the chrome above
-- and below the column is FIXED: the profile tile, and a bottom band whose height
-- derives from `minimap_width` = max(336, 0.28 * min(disp_x, disp_y)), which is
-- 336 at BOTH. So every one of the extra 360 rows at 1080p lands in the ledger's
-- own content height.
--
-- Measured on this surface: the Market price list shows 3.5 goods at 720p and 9
-- at 1080p. A density judgement taken at 720p is taken against half the content
-- height the reviewer has, and every "only N rows fit" number this sprint produced
-- before 2026-08-29 was measured on the wrong screen.
--
-- `shell_pass.lua` deliberately STAYS at 1280x720: it carries the clipping
-- assertion, and the smallest supported display (BL-215) is where a string is
-- most likely to overrun. Worst-case fit there, design review here.
verify.window(1920, 1080)

local staged = stage_ui_fixture()
print("LEDGER state_hash=" .. verify.state_hash())

local PANELS = {
    "corporation", "balance", "construction", "tech_tree", "acquisitions",
    "market", "corporations_table", "tile", "generation_ledger", "decisions",
    "strategy", "build",
}

local function close_all()
    for _, p in ipairs(PANELS) do verify.show_panel(p, false) end
    verify.frames(1)
end

--- Open one ledger alone, optionally on a sub-view, and capture it.
local function ledger(panel, name, view_key, view)
    close_all()
    verify.show_panel(panel, true)
    if view ~= nil then verify.panel_view(view_key, view) end
    shot(name)
end

--- Capture the FOOT of a ledger that scrolls. `scroll_key` is scroll_panel's
--- vocabulary, which is not always show_panel's (history/tile share a window).
local function foot(panel, scroll_key, name, view_key, view)
    close_all()
    verify.show_panel(panel, true)
    if view ~= nil then verify.panel_view(view_key, view) end
    verify.frames(2)
    verify.scroll_panel(scroll_key, 1.0)
    verify.frames(3)
    verify.capture(name)
    verify.scroll_panel(scroll_key, 0.0)
    verify.frames(2)
end

-- ===========================================================================
-- Slot 1 — Corporation ledger (UI-100)
-- ===========================================================================
-- ONE card since BL-691, Balance, which shows its chart AT REST — so the resting
-- frame is no longer a verdict line with nothing under it. The takeover is still
-- worth its own frame: it is the same card given the canvas. The old loop over
-- four card indices is gone with the three cards it opened.
ledger("corporation", "ledger_01_corp_dashboard_rest")
close_all()
verify.show_panel("corporation", true)
verify.fold("corp_rollup", 0)
shot("ledger_01_corp_dashboard_expand_0")
verify.fold()

-- ===========================================================================
-- Slot 2 — Balance / Budget (UI-070..073)
-- ===========================================================================
-- One flat panel of stacked sections, so the sub-levels are SCROLL POSITIONS
-- rather than tabs. Three shots: head (chart + levers), foot (Assets + the
-- top-buildings ranking), and the foot again in debt, since the interest row is
-- hidden at zero and the section's row count changes across the solvency
-- boundary (balance.md § Data sources).
ledger("balance", "ledger_02_budget_head")
foot("balance", "balance", "ledger_02_budget_foot")

-- ===========================================================================
-- Slot 3 — Construction (UI-074..077)
-- ===========================================================================
ledger("construction", "ledger_03_construction")

-- ===========================================================================
-- Slot 4 — Research / tech-tree mock (UI-108)
-- ===========================================================================
-- One era per tab, and the default is Era 0 rather than the first tab, so a
-- single capture shows neither the placeholder era above it nor the unauthored
-- one below.
for v = 0, 3 do
    ledger("tech_tree", string.format("ledger_04_research_era_%d", v), "tech_tree", v)
end

-- ===========================================================================
-- Slot 5 — Acquisitions ledger
-- ===========================================================================
ledger("acquisitions", "ledger_05_acquisitions")

-- ===========================================================================
-- Slot 6 — Market ledger (UI-080..083)
-- ===========================================================================
for i, v in ipairs({ "prices", "sell_orders", "convoys" }) do
    ledger("market", string.format("ledger_06_market_%d_%s", i - 1, v), "market", i - 1)
end
foot("market", "market", "ledger_06_market_0_prices_foot", "market", 0)

-- ===========================================================================
-- Slot 8 — the all-corporations table (UI-078/079)
-- ===========================================================================
-- Head and foot: the footer row (`Corporations: N`) is below the fold at 720p
-- with forty firms in the world, and the footer is the only thing on the surface
-- that says how long the list is.
ledger("corporations_table", "ledger_08_corps_table_head")
foot("corporations_table", "corporation", "ledger_08_corps_table_foot")

-- ===========================================================================
-- Slot 9 — History (UI-084..087)
-- ===========================================================================
-- FOUR views, not three: Story / Chain / Ages / Tectonics (tile_inspector.hpp
-- § history_view_id). The Tectonics view landed with BL-660 and neither
-- ui_elements.json nor ui_state.hpp's own comment has caught up — the comment
-- still reads "2=Tiles, 3=Ages", which is two errors in one line.
ledger("tile", "ledger_09_history_0_story", "history", 0)
foot("tile", "history", "ledger_09_history_0_story_foot", "history", 0)
ledger("tile", "ledger_09_history_1_chain", "history", 1)
-- Chain groups its stages into three rounds behind a second button strip; one
-- round is one third of the view.
for r = 0, 2 do
    close_all()
    verify.show_panel("tile", true)
    verify.panel_view("history", 1)
    verify.panel_view("history_round", r)
    shot(string.format("ledger_09_history_1_chain_round_%d", r))
end
-- Ages is DELIBERATELY NOT CAPTURED HERE — see § Ages at the foot of this file.
ledger("tile", "ledger_09_history_3_tectonics", "history", 3)

-- ===========================================================================
-- Slot 10 — Generation ledger (UI-105)
-- ===========================================================================
-- Two tabs (Body / Tile) with no panel_view hook, so only the default is
-- reachable from a script. That gap is itself a finding, recorded here rather
-- than worked around.
ledger("generation_ledger", "ledger_10_generation_body")
foot("generation_ledger", "generation_ledger", "ledger_10_generation_body_foot")

-- ===========================================================================
-- Slots 11 / 12 — the observability tail (UI-106/107)
-- ===========================================================================
ledger("decisions", "ledger_11_decisions_all")
ledger("strategy",  "ledger_12_strategy_all")
-- The Strategy readout's resting state is the all-corporations comparison; the
-- single-corp profile is a different layout and is reachable only through the
-- selector.
close_all()
verify.show_panel("strategy", true)
verify.strategy_filter(-1)
shot("ledger_12_strategy_player")
verify.strategy_filter(0)

-- Contracts held a fourteenth slot and three sub-views here (offers / active /
-- history). The mercenary contract is retired and the ledger is deleted, so the
-- sweep ends at slot 13, the last of the developer tail (BL-693).

close_all()
verify.frames(1)

-- The one assertion this pass can make for free. It is CURRENTLY VACUOUS on
-- these frames (0 records — NR-663: SmallButton and table-cell labels are not
-- instrumented into the overflow ledger), and it is left in deliberately: the
-- captures above contain at least four visibly clipped strings, so the day the
-- ledger grows to see them this check turns red without anyone re-authoring it.
verify.expect_no_clipping("ledger_pass")

-- ===========================================================================
-- § Ages — why this pass does not capture it
-- ===========================================================================
-- The History ledger's Ages view (history_view = 2) is NOT swept above, and the
-- omission is the finding rather than a gap.
--
-- MEASURED 2026-08-29, this build, this fixture: the FIRST DRAW of Ages had not
-- produced a single frame after NINETEEN MINUTES of solid CPU, and the run was
-- killed rather than left to finish. tile_inspector.cpp § Ages builds its cache
-- inline on the drawing thread — `run_history_sim(cached_ss, ..., start_year 0,
-- stop_year campaign_epoch_year)` over the body's real terrain — so the first
-- frame that shows the tab pays for replaying the whole Era -1 political history
-- before it returns. In the built app that is a tab click that stops the
-- application, with no progress and no way back.
--
-- WHAT IS AND IS NOT ESTABLISHED. That no frame arrived in nineteen minutes is
-- measured. Whether the sim is merely very slow or does not terminate at all on
-- this fixture is NOT established — CPU climbed steadily throughout, which is
-- consistent with both. Do not repeat either reading as fact.
--
-- WHY NOBODY HIT IT BEFORE: no verify script has ever selected this view.
-- `history_ledger_and_comms.lua` covers Story and Chain and returns to Story;
-- `verify.ages_year` was added with BL-277 and, until this file, called by
-- nothing. The view has been shipped, documented in tile_ledger.md, and never
-- once rendered by a check.
--
-- AND THE HOOK IS DEFEATED ANYWAY. tile_inspector.cpp sets `s.ages_year = 0` on
-- the frame it (re)builds the cache, which is the same frame a script's park
-- lands on — so a captured Ages frame would show year 0 whatever the script asked
-- for. Both halves want fixing together: the sim off the draw thread, and the
-- park applied after the cache build rather than before it.
