-- Visual verification for the BL-249 frame-budget HUD — the first of ROADMAP § v0.1.0's
-- three quality-audit instruments.
--
-- What is being checked, and what deliberately is NOT. The NUMBERS the HUD reports in
-- THIS script's captures are transient one-frame samples — meaningless as budget
-- evidence, so no golden pins them. (An earlier version of this header claimed --verify
-- runs an offscreen/dummy driver with no vsync; that was wrong — nothing in src/ sets
-- any such driver, and a --verify run uses the real renderer with the build's real
-- vsync. The ROADMAP budget targets are measured by scripts/verify/pan_perf.lua, which
-- exists because the real-renderer property holds. Corrected 2026-08-02, BL-268.)
--
-- What this script DOES pin is everything else, which is what regresses silently:
--   * the HUD is OFF by default — capture 1 is a normal frame with no HUD anywhere, so
--     every other golden in the suite stays unaffected by this instrument existing;
--   * it draws, and reads, when parked open;
--   * it survives the two contexts that have different frame shapes — the pre-play
--     screens (which take app::render's early-return branch) and in-game.
--
-- Run with: ProjectIo --verify scripts/verify/frame_budget_hud.lua

-- 1. OFF BY DEFAULT. An audit instrument must not appear in an ordinary frame; if this
--    capture ever shows the HUD, every other golden in the suite is contaminated.
verify.goto_surface("home")
verify.capture("frame_hud_absent_by_default")

-- 2. Parked open over the Planetary canvas. The HUD anchors into the canvas area's
--    top-left, clear of the shell column and header strip.
verify.show_panel("frame_hud", true)
verify.capture("frame_hud_in_game")

-- 3. A few economy steps so the ring buffer has a populated history rather than the
--    first-frame degenerate case (one sample, avg == max == last).
verify.econ_step(4)
verify.capture("frame_hud_populated")

-- 4. Toggle rule (io-standing-rules § Toggle rule): a control whose active state is
--    visible is a toggle — putting it away must leave no residue.
verify.show_panel("frame_hud", false)
verify.capture("frame_hud_dismissed")
