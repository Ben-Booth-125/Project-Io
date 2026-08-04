-- Verify the F9 mock tech-tree viewer's era tabs (BL-087 viewer; tabs added with
-- the BL-307 session). nav_button strip, one era per view. Two captures, one per
-- concern: the default view (Era 0, the campaign's era) proves the strip renders
-- with each era owning a tab; view 0 proves an unauthored era holds a placeholder
-- (Era -1 Antiquity, pointing at the ladder data store).
verify.show_panel("economy", false) -- econ_step force-opens it; keep the shot clean
verify.show_panel("tech_tree", true)
verify.capture("tech_tree_tabs")
verify.panel_view("tech_tree", 0)
verify.capture("tech_tree_antiquity")
