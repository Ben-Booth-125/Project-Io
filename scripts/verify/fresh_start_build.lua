-- Fresh-start buildability — the acceptance test the build flow always lacked.
-- Proves a player can PLACE their first building from a cold campaign start,
-- through the real construct_building path against the real economy registry.
-- This is the exact coverage that would have caught the BL-044 material-cost
-- deadlock (construction_harness uses a hand-built registry without the steel
-- costs; build_walkthrough only armed placement and never committed). It fails
-- (non-zero exit) if a fresh player cannot place a basic extraction site — e.g.
-- if construction is re-gated on materials the empty starting pool can't meet
-- before BL-095 lands. Realises docs/development/USER_STORIES.md US-002.
-- Run: ProjectIo --verify scripts/verify/fresh_start_build.lua

verify.goto_surface("home")

-- Corps now open as new charters (base_capital=0, 2026-07-06 playtest patch) and
-- earn their turn-one balance through app::start_new_game's 12-tick pre-game warm
-- start (~3 in-game years of simulated production/wages/trade against the
-- generation-time asset placement). run_verify stays deterministically cold by
-- design (no warm-up, for golden reproducibility), so a literal balance=0 here
-- would test a state that never occurs in real play. Approximate the earned
-- turn-one balance instead (headless pregame_balance_harness measured ~400-2000cr
-- across the warm-start window) so this still checks what "fresh player, turn
-- one" can actually afford, per US-002.
verify.set_balance(500.0)

-- Arm the most basic building a fresh player would place first.
verify.place_mode("extraction", "iron_ore")

-- Commit it on the first valid tile, via the same construct_building path an
-- interactive click uses. "placed" iff a fresh player can actually build.
local result = verify.build_first_valid()
verify.expect(result == "placed",
    "fresh player can place their first building (extraction/iron_ore); got: " .. result)

verify.capture("fresh_start_built")
