-- Visual verification for the BL-122 Paradox-style fold-out shell (skeleton).
--
-- Confirms the outer shell reshape:
--   * the identity tile (top-left) takes the shell-column width W,
--   * the balance bar and the Selection element start at x = W (permanently narrowed),
--   * each named ledger folds OUT into the [nav_rail, W] column as a borderless, pinned
--     panel rather than a floating window, one at a time.
--
-- Run:     ProjectIo --verify scripts/verify/foldout_shell.lua
-- Inspect: screenshots/foldout_shell_*.png
--   - foldout_shell_bare         : widened identity tile + icon rail; header + Selection
--                                  begin at the column edge; no fold-out open (canvas
--                                  shows through the [rail, W] body).
--   - foldout_shell_construction : Construction folded out into the column.
--   - foldout_shell_economy      : Economy folded out, Construction closed (accordion).
--
-- Captured at the harness window size (1280x720 -> W = clamp(round(0.17*1280),300,360) =
-- 300). The 1920x1080 case shares the shell_column_width() code path (W ~ 326) and is
-- not separately captured (the harness window is fixed at 1280x720).

verify.goto_surface("Kepler")

-- A few economy ticks so the header shows a live balance + sparkline and the ledgers
-- have content to render inside the narrow column.
verify.econ_step(4)

-- Select a tile so the bottom-left Selection element is visible at its new left edge (x = W).
verify.select_tile(70, 38)
verify.center_tile(70, 38, 8)

-- 1) Bare shell: no fold-out open. econ_step force-opens Economy; close it for a clean read.
verify.show_panel("economy", false)
verify.capture("foldout_shell_bare")

-- 2) Construction folded out into the shell column.
verify.show_panel("construction", true)
verify.capture("foldout_shell_construction")

-- 3) Economy folded out; Construction closed (accordion — one panel at a time).
verify.show_panel("construction", false)
verify.show_panel("economy", true)
verify.capture("foldout_shell_economy")
