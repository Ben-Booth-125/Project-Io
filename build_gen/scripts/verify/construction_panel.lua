-- Visual verification for the Layer 4 construction / building-management panel
-- (construction_panel.cpp), the v0.0.5 UI-groundwork scaffold.
--
-- The panel and placement mode are pure UI state, so the harness drives them
-- directly: show_construction() opens the window; place_mode() arms placement
-- (which the panel reports as "Placing: ..."). The on-canvas ghost marker is
-- hover-driven and so cannot be captured headlessly (no synthetic mouse) — its
-- logic is checked by code grep instead (see REQUIREMENTS.md § layer4-ui-groundwork).
--
-- Run: ProjectIo --verify scripts/verify/construction_panel.lua
-- Inspect: screenshots/construction_panel*.png

-- Land on the home planet so the surface (and any corporate buildings) are in scope.
verify.goto_surface("Kepler")

-- Open the panel and arm extraction placement targeting iron ore, so the capture
-- shows the Build section, the target selector, and the armed placement status.
verify.show_construction()
verify.place_mode("extraction", "iron_ore")

verify.capture("construction_panel_build")
