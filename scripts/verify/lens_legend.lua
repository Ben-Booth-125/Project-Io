-- BL-533 (legend re-homed beside the minimap) — the collapsed dropdown and the
-- scrolling body.
--
-- WHY THIS EXISTS. The Country legend used to paint free-hand on the canvas,
-- anchored flush-left of the minimap and bounded to the CANVAS vertical span.
-- That span runs behind the bottom panels, so "clamped to the canvas" never
-- promised it would not overlap anything — and with 40 nations it overprinted
-- the tile ledger outright (NR-503). Nothing caught it: text_overflow_floor
-- looks for CLIPPED text, and this text was not clipped, it was overdrawn.
--
-- Ben's ruling (2026-08-22): every legend moves to the right chrome column
-- beside the minimap, takes about a quarter of that space, scrolls rather than
-- grows, wraps its labels, and is COLLAPSED BY DEFAULT.
--
-- The three claims:
--   L1  a lens with a legend opens COLLAPSED — one header bar, canvas clear
--   L2  the header is a real toggle, reachable by a real press
--   L3  expanded, the body is bounded by the minimap top and SCROLLS
--
-- This check is also the first consumer of BL-521's click injection outside
-- click_injection.lua itself, which is the point of having built it.
verify.window(1280, 720)

verify.goto_surface("home")
verify.frames(2)

-- ---------------------------------------------------------------------------
-- L1 — collapsed by default. The whole legend is one bar above the minimap.
-- ---------------------------------------------------------------------------
-- Country is the worst case (40 nations on seed 0) and the one that overflowed.
-- Expect: a single "> Countries (40)" header in the right chrome column, its
-- right edge on the screen edge, and NOTHING drawn over the canvas or the
-- bottom panels. The count belongs in the header on purpose — a collapsed
-- legend that does not say how much it hides gives no reason to open it.
verify.set_overlay("corporation")
verify.frames(2)
verify.capture("legend_corporation_collapsed")

-- ---------------------------------------------------------------------------
-- L2 / L3 — the press lands, and the body scrolls rather than overrunning.
-- ---------------------------------------------------------------------------
-- The header bar sits directly above the minimap. Derive its position from the
-- minimap rather than hardcoding pixels, so this survives a window change:
-- minimap_width = max(336, 0.28*min(w,h)) and height = 0.75*width, with the
-- right edge flush to the screen edge (BL-312).
local W, H = 1280, 720
local mm_w = math.max(336, 0.28 * math.min(W, H))
local mm_h = mm_w * 0.75
local chrome_margin = 8
-- The box is PINNED AT ITS TOP, just below the time panel, and grows downward.
-- That is what makes the toggle repeatable: the header does not move when the
-- list opens. The ceiling is shell_margin + time_panel_h + shell_margin; the
-- panel measures ~62px at this font, so the header lands around y=88.
local shell_margin = 8
local time_panel_h = 84                  -- measured off the capture, not assumed
local ceiling_y = shell_margin + time_panel_h + shell_margin
local header_x = W - mm_w * 0.5          -- horizontal centre of the column
local header_y = ceiling_y + 16          -- inside the header band (now the full chrome)

verify.hover(header_x, header_y, 2)
verify.click(header_x, header_y)
verify.frames(2)
verify.capture("legend_corporation_expanded")

-- Re-press closes it again. This is the standing toggle rule: a control whose
-- active state is visible undoes itself on a second press.
verify.hover(header_x, header_y, 2)
verify.click(header_x, header_y)
verify.frames(2)
verify.capture("legend_corporation_recollapsed")

-- ---------------------------------------------------------------------------
-- The other legend-bearing lenses use the SAME box, so a regression in one is
-- a regression in all. Market additionally hosts the resource combo above its
-- header, which is the case most likely to mis-stack.
-- ---------------------------------------------------------------------------
verify.set_overlay("market")
verify.frames(2)
verify.capture("legend_market_collapsed")

-- Market stacks the resource combo ABOVE its header (kLensComboH + 4 = 26px),
-- because the good being shown is worth picking even while the list is shut.
-- So its header band starts one combo lower than every other lens's.
local market_header_y = header_y + 26

verify.hover(header_x, market_header_y, 2)
verify.click(header_x, market_header_y)
verify.frames(2)
verify.capture("legend_market_expanded")

verify.set_overlay("reach")
verify.frames(2)
verify.capture("legend_reach")

verify.set_overlay("none")
verify.frames(2)
verify.capture("legend_cleared")
