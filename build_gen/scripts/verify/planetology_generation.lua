-- Visual verification for Planetology (BL-167) — the New World wizard.
--
-- Ben's asks, in order: "the user sees each stage and chooses at each stage ...
-- present this with graphs and charts", then "the wizard should be pseudo random,
-- so if you have preferences you can find them, but really you don't get full
-- customization ... we don't need so many rounds, it's just too slow - try to
-- batch them together thematically."
--
-- So the wizard is three rounds on the chain's own A -> B -> C shape, each
-- showing every chart of its stages, and each taking PREFERENCES (named leans,
-- no numbers) rather than settings.
--
-- Confirms:
--   * the main menu is back to seed / resources / nations, with the world's
--     character chosen during generation rather than up front;
--   * each round draws its charts in the tile-selection idiom (bordered
--     container, tick gutter, dotted gridlines, clustered columns, legend);
--   * the gate stages draw threshold lines on the same axis as the values they
--     judge — the retention shoreline, the mobile-lid band, the liquid-water window;
--   * no raw parameter value appears anywhere in the decision area.
--
-- Every round is a stable capture: the wizard is driven by the resolved
-- preferences, not by wall-clock, so there is no animation to race.
--
-- Run with: ProjectIo --verify scripts/verify/planetology_generation.lua

-- The New World setup. First capture after a screen switch is a warmup — an
-- AlwaysAutoResize window measures zero on its first frame (as main_menu.lua does).
verify.show_menu(true)
verify.capture("planetology_menu_warmup")
verify.capture("planetology_menu")
verify.show_menu(false)

-- The three rounds. Named for the chain phase each covers, so a diff points at
-- the round that moved.
local rounds = {
    [0] = "system",      -- A: System, Accretion, Air, Engine
    [1] = "life",        -- B: Water, Spark, Breath, Green
    [2] = "inheritance", -- C: Legacy, Spend
}

verify.generation_stage(0)
verify.capture("planetology_wizard_warmup")

for i = 0, 2 do
    verify.generation_stage(i)
    verify.capture("planetology_wizard_" .. i .. "_" .. rounds[i])
end

verify.show_generation(false)

-- The surface the chain actually produced. Kepler is the one living world, so it
-- keeps grassland/forest/wetland; the biotic mask strips those from any body that
-- never reached a land biosphere.
verify.goto_surface("home")
verify.capture("planetology_home_surface")
