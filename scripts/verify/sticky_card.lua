-- Visual verification for the sticky detail card (BL-194).
-- Confirms requirements.json § sticky-detail-card R1:
--   a single click opens a click-opened detail card (binary open/closed) that
--   persists until dismissed, confined within the canvas, coexisting with the
--   unchanged transient hover card.
-- Driver: direct state manipulation (select_tile / dismiss_selection).
-- Run with: ProjectIo --verify scripts/verify/sticky_card.lua

verify.goto_surface("home")

-- Select a tile: the card should appear (open).
verify.select_tile(0, 0)
verify.capture("sticky_card_00_open")

-- Dismiss (equivalent to the card's x / Esc): the card should disappear.
verify.dismiss_selection()
verify.capture("sticky_card_01_dismissed")

-- Re-selecting shows it again (a new selection always re-shows, even if the
-- same entity was previously hidden for a different reason).
verify.select_tile(0, 1)
verify.capture("sticky_card_02_reopened")
