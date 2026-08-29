-- BL-215 render-precision audit: the saved overflow check.
--
-- Walks every ledger/sub-view, the wizard rounds, the lenses, the Selection-band
-- kinds, the header, the comms dock and the build ledger at the 1280x720 display
-- floor, then asserts the text-overflow ledger holds ZERO clipped/unfittable
-- records. The PNGs are incidental output — the ledger is the pass criterion.
--
-- Run:     ProjectIo --verify scripts/verify/text_overflow_floor.lua
-- Report:  screenshots/text_overflow.txt (written by expect_no_clipping)

verify.goto_surface("home")

-- Live data everywhere: balances, markets, sparkline, chat epoch lines.
verify.econ_step(4)

-- Header + bare shell (comms dock included in every frame).
verify.capture("overflow_shell")

-- Every fold-out ledger, walking each button-strip sub-view.
local panels = {
    { name = "construction", views = nil },
    { name = "tile",         views = { "history", 0, 2 } },
    { name = "market",       views = { "market", 0, 2 } },
    { name = "balance",      views = nil },
    { name = "corporation",  views = nil },
    { name = "build",        views = nil },
    { name = "tech_tree",    views = { "tech_tree", 0, 2 } },
}
for _, p in ipairs(panels) do
    verify.show_panel(p.name, true)
    if p.views then
        local key, lo, hi = p.views[1], p.views[2], p.views[3]
        for v = lo, hi do
            verify.panel_view(key, v)
            verify.capture("overflow_" .. p.name .. "_v" .. v)
        end
    else
        verify.capture("overflow_" .. p.name)
    end
    verify.show_panel(p.name, false)
end

-- The History ledger's round strip (the fold-out charts that clipped, BL-215).
verify.show_panel("tile", true)
verify.panel_view("history", 1) -- Chain view
for r = 0, 2 do
    verify.panel_view("history_round", r)
    verify.capture("overflow_history_round" .. r)
end
verify.show_panel("tile", false)

-- Every lens (the on-canvas legend boxes).
local lenses = { "corporation", "country", "resource", "market", "population",
                 "continent", "scarcity", "industry", "reach", "supply_routes" }
for _, l in ipairs(lenses) do
    verify.set_overlay(l)
    verify.capture("overflow_lens_" .. l)
end
verify.set_overlay("none")

-- Selection-band kinds: tile, building, body go-to.
verify.select_tile(70, 38)
verify.capture("overflow_selection_tile")
verify.place_mode("extraction", "iron_ore")
verify.build_first_valid()
verify.capture("overflow_selection_building")
verify.go_to("home")
verify.capture("overflow_selection_body")

-- The New World wizard, all three rounds (the charts' widest host), expanding
-- every chain stage so the chart draw paths actually run (stages rest folded).
local rounds = { [0] = { 0, 3 }, [1] = { 4, 7 }, [2] = { 8, 9 } }
for r = 0, 2 do
    verify.generation_stage(r)
    verify.capture("overflow_wizard_round" .. r)
    for k = rounds[r][1], rounds[r][2] do
        verify.fold("generation_stage", k)
        verify.capture("overflow_wizard_r" .. r .. "_stage" .. k)
    end
    verify.fold()
end
verify.show_generation(false)

-- The History ledger's chain stages expanded in the narrow fold-out column —
-- the below-plot legend reflow's real host.
verify.show_panel("tile", true)
verify.panel_view("history", 1) -- Chain view
for r = 0, 2 do
    verify.panel_view("history_round", r)
    for k = rounds[r][1], rounds[r][2] do
        verify.fold("generation_stage", k)
        verify.capture("overflow_history_r" .. r .. "_stage" .. k)
    end
    verify.fold()
end
verify.show_panel("tile", false)

-- The pass criterion: zero clipped/unfittable records across everything above.
verify.expect_no_clipping("floor")
