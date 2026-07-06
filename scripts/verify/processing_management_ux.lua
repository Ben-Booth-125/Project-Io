-- UX walkthrough (playtest patch, 2026-07-06): US-007 ("choose what an existing
-- building produces and how much workforce it draws") for a PROCESSING FACILITY,
-- to compare against extraction-site management (which Ben says "seems right").
-- Not golden-diffed — a narrated capture sequence for eyeballing the panel.
-- Run: ProjectIo --verify scripts/verify/processing_management_ux.lua

verify.goto_surface("home")

-- A real player never manages at literal balance=0 (see fresh_start_build.lua /
-- build_walkthrough.lua for the new-charter + cold-verify interaction).
verify.set_balance(2000.0)

verify.show_construction()

-- Place a processing facility (defaults to the "steel" recipe at construction,
-- construction.cpp) and select it — build_first_valid sets selected_entity.
verify.place_mode("processing")
local result = verify.build_first_valid()
verify.expect(result == "placed", "processing facility placed; got: " .. result)

-- The management section right after placement: default recipe display,
-- workforce slider, and (since 3 recipes exist) the closed recipe combo.
-- No synthetic mouse in the headless harness, so the combo can't be opened or
-- an option picked from a script — that gap is itself part of what's being
-- checked (can the panel communicate a recipe's inputs/outputs without a click?).
verify.capture("proc_mgmt_01_just_built")
