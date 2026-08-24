-- Visual verification for BL-593 (the Build door under a doubled roster).
-- Confirms the tech-lock filter added this item: `refined_copper` (Metal
-- Foundry, tech-locked by E0-EC-03 since BL-589) does not appear in the
-- ledger at all, the same way an era-locked or depth-locked candidate never
-- did — "the door not showing what the gate would refuse", extended to the
-- third lock kind.
--
-- Run: ProjectIo --verify scripts/verify/build_door_wide_roster.lua

verify.econ_step(4)
verify.show_panel("economy", false)
verify.goto_surface("home")

verify.select_tile(57, 34)
verify.capture("build_door_select") -- settle the selection first (BL-162's own lesson)
verify.show_panel("build", true)
verify.frames(2)
verify.capture("build_door_wide_roster")
