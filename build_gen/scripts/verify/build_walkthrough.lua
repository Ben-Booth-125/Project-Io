-- New-player walkthrough: from launch to placing a building, captured step by
-- step so we can critique the current onboarding UX (main-menu + default-lens
-- framing work). Not a golden-diffed check — a narrated capture sequence.
--
-- The player corp is 30310 with buildings near (70,37) on the home surface.
-- Run: ProjectIo --verify scripts/verify/build_walkthrough.lua

verify.goto_surface("home")

-- Step 1 — LAUNCH. What the app shows the moment it opens today: the Planetary
-- surface, no lens applied (plain terrain). There is no main menu yet, and no
-- cue as to which tiles are "mine". Warm-up frame settles the bottom lens strip.
verify.capture("walk_00_launch_warmup")
verify.capture("walk_01_launch")

-- Step 2 — "WHO IS WHO". The only current way to see ownership is to manually
-- pick the Corporation lens from the strip. Player vs. rival tint appears.
verify.set_overlay("corporation")
verify.capture("walk_02_corp_lens")

-- Frame the player's holdings for the remaining steps.
verify.set_overlay("none")
frame_tile(70, 40, 14)
verify.capture("walk_03_framed_on_player")

-- Step 3 — SELECT A TILE. Single click selects; the Selection panel answers
-- "what is this / what can I do here". Select the player's building tile.
verify.select_building(70, 37)
verify.capture("walk_04_selection")

-- Step 4 — OPEN CONSTRUCTION. The Construction ledger is where a build starts.
verify.show_panel("construction", true)
verify.capture("walk_05_construction_open")

-- Step 5 — ARM PLACEMENT. Choose a building type (+ extraction target). The
-- panel switches to "Click a tile on the surface to build it." and a ghost
-- marker follows the cursor.
verify.place_mode("extraction", "iron_ore")
verify.hover_tile(69, 40)
verify.capture("walk_06_placement_armed")

-- Frame the ghost close-up.
frame_tile(69, 40, 20)
verify.hover_tile(69, 40)
verify.capture("walk_07_placement_ghost")

-- Step 6 — COMMIT THE BUILD. Actually place the armed building via the real
-- construct_building path (not just disarm + select a pre-existing one) so the
-- final capture shows a freshly placed building — a completed build, not a ghost.
-- A real player never clicks Build at literal balance=0 — corps now open as new
-- charters (base_capital=0, 2026-07-06) and earn their turn-one balance through
-- app::start_new_game's 12-tick pre-game warm start, which run_verify's
-- deterministically-cold path skips by design. Approximate that earned balance
-- here so this walkthrough still reflects what turn one actually looks like.
verify.set_balance(500.0)
verify.build_first_valid()
verify.capture("walk_08_built_building")
