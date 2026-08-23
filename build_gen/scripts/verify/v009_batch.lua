-- Visual verification for the v0.0.9 polish batch.
-- Covers the surfaces the harness can drive headlessly:
--   * BL-070 gear button placement (top-right, clear of the time column) — the
--     popup interior is interaction-gated (Esc / click), so only the resting
--     gear affordance is captured here; the popup contents are code-verified.
--   * BL-090 corp emblems — the profile-card emblem, the Selection-header emblem,
--     and the on-canvas owner-identity tags beside building markers.
--   * BL-081 economy-panel refit — widened balances table, per-building table gone.
--   * BL-082 construction panel anchored clear of the bottom-left Selection element.
--
-- Run: ProjectIo --verify scripts/verify/v009_batch.lua [--bless]

verify.econ_step(4)                    -- populate balances / markets / building states
verify.show_panel("economy", false)    -- close the auto-opened economy panel

-- 1) Plain surface frame: gear button (top-right) + profile-card emblem (top-left).
verify.goto_surface("home")
verify.capture("v009_gear_and_profile")

-- 2) A player building selected: the Selection header carries the corp emblem, and
--    the on-canvas building markers carry small owner-identity emblem tags.
local player
for _, b in ipairs(verify.buildings()) do
    if b.player then player = b; break end
end
assert(player, "v009_batch.lua: no player building on the home body")
verify.select_building(player.x, player.y)
verify.capture("v009_emblem_selection_and_markers")

-- 3) Construction panel open with a tile selected: the panel must sit clear of the
--    bottom-left Selection element (BL-082) so the tile affordance read stays visible.
verify.select_tile(player.x, player.y)
verify.show_construction()
verify.place_mode("extraction", "iron_ore")
verify.capture("v009_construction_no_overlap")
verify.show_panel("construction", false)  -- close it before the next capture

-- 4) Economy-panel refit: widened corp balances, no per-building table.
verify.show_panel("economy", true)
verify.capture("v009_economy_refit")
