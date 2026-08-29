-- Visual verification for the Market lens (overlay_mode::market).
-- Confirms REQUIREMENTS.md § market-lens-render R1-R6:
--   R2/R3 Planetary body-wide diverging price wash (warm = dear, cool = cheap)
--         keyed to price/base_price + the on-canvas diverging key
--   R4    the good-selector (shared with the Resource lens) picks the displayed good
--   R5    the Circumplanetary per-body price strip
--   R6    prices are diverged from base by running the economy first
--
-- Driver: run the economy a fixed number of ticks so prices diverge from their
-- floors (deterministic given the seeded world), close the panel econ_step opens,
-- then capture under the Market lens. Captures diff against the blessed goldens.
-- Run with: ProjectIo --verify scripts/verify/market_lens.lua

verify.goto_surface("home")

-- Diverge the per-body market prices from base before the canvas capture.
verify.econ_step(12)

verify.set_overlay("market")

-- A traded raw good: the body washes warm/cool by iron's price vs its floor.
verify.set_lens_resource("iron_ore")
verify.capture("market_lens_planetary_iron")

-- A refined good proves the selector re-keys the wash to a different good.
verify.set_lens_resource("steel")
verify.capture("market_lens_planetary_steel")

-- Ascend to the Circumplanetary rung for the per-body price strip (iron selected,
-- so its row is highlighted in the strip).
verify.set_lens_resource("iron_ore")
verify.command("ascend")
verify.capture("market_lens_circum_strip")
