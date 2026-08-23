-- Visual verification for the Selection bar layout fix (uncommitted, 2026-06-30).
-- Confirms two coupled changes to draw_selection_panel:
--   1. Bar HEIGHT matches the bottom-right minimap (caller passes mm_h), so the
--      selection bar reads as the minimap's left-hand twin rather than a thin strip.
--   2. The header-row [>] (go to) and [x] (close) buttons RENDER inside the bar.
--      They were previously clipped off the right edge unseen because SameLine's
--      offset double-counted content_x; the fix measures the offset from the
--      content column origin (content_w) directly.
--
-- Driver: verify.select_tile on the active body (Kepler/home) so the bar is shown
-- with a tile selection. Both the bar and the minimap must be visible in the same
-- frame to compare their heights, so we do NOT open any ledger panel over them.
-- Run with: ProjectIo --verify scripts/verify/selection_bar.lua

-- A populated land tile on the home body — the bar shows the tile header (name +
-- kind), the [>] go-to and [x] close buttons on the right, and tile content rows.
verify.select_tile(66, 6)
verify.capture("selection_bar_00_tile")

-- A second tile in a different region, to confirm the layout is stable regardless
-- of selection content length (longer/shorter titles must not reflow the buttons
-- off-edge again).
verify.select_tile(40, 20)
verify.capture("selection_bar_01_tile_alt")
