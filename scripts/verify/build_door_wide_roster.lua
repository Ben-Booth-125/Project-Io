-- Visual verification for BL-593 (the Build door under a doubled roster).
-- The door filters what the tech gate would refuse ("the door not showing what
-- the gate would refuse"). SINCE BL-1273 NOTHING IS TECH-LOCKED: E0-EC-03's
-- lock on `refined_copper` retired with the per-corp gates (NR-1022), so the
-- recipe appears in the ledger like any other; the filter stays, reading the
-- nation's tree (recipe_unlocked), for the nodes the effect pass will lock.
--
-- Run: ProjectIo --verify scripts/verify/build_door_wide_roster.lua

verify.econ_step(4)
verify.goto_surface("home")

verify.select_tile(57, 34)
verify.capture("build_door_select") -- settle the selection first (BL-162's own lesson)
verify.show_panel("build", true)
verify.frames(2)
verify.capture("build_door_wide_roster")
