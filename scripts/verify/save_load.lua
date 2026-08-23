-- BL-536 (world snapshot save) — a loaded campaign renders as the one it was
-- taken from, and the app-envelope surfaces come back with it.
--
-- WHY THIS EXISTS. save_roundtrip.cpp proves the WORLD half field for field, in
-- headless C++. It cannot prove the half that motivated the item: that a capture
-- taken from a loaded save is worth anything. A snapshot that restored every
-- number but opened with a blank body biography and an empty Generation Ledger
-- would pass every headless row and still be useless for a design pass, because
-- those are exactly the surfaces such a pass is capturing.
--
-- The claims:
--   S1  the canvas + Selection band render identically across a round trip
--       (this carries the tile BIOGRAPHY, which reads generation_report)
--   S2  the Generation Ledger renders identically too (it reads tile_inputs)
--   S3  the view slice — rung, framing, lens, selection — comes back as saved
--   S4  a refused load leaves the live campaign untouched
--
-- ORDERING MATTERS HERE and got this check wrong once already: the "before"
-- capture must be taken in EXACTLY the state that is then saved. Capture first
-- and mutate afterwards, and the pair being compared is not a pair at all.
-- So: build the view, capture it, save it, and only then touch anything.
--
-- The paired captures are named <name>_before / <name>_after on purpose: the
-- claim is that the two are the SAME image, so they must be inspectable side by
-- side rather than collapsed into one golden.

verify.window(1280, 720)

local SAVE = "build_gen/verify/save_load_check.iosave"

-- ---------------------------------------------------------------------------
-- Build a view worth restoring — every value deliberately off-default.
-- ---------------------------------------------------------------------------
-- A save that restored the DEFAULTS would pass S3 by accident. So: a surface
-- rung rather than solar, a lens that is not `none`, a zoom that is not the
-- opening zoom, a non-zero pan, and a selected tile.
verify.goto_surface("home")
verify.set_overlay("continent")
verify.set_zoom(1.9)
verify.set_pan(40, -25)
verify.click_tile(120, 55)
verify.frames(3)

-- S1's "before". The Selection band in this frame is showing the clicked tile,
-- biography included — which is why this one capture carries both claims.
verify.capture("save_load_view_before")
local hash_before = verify.state_hash()

-- The save is taken HERE, in the state just captured. Nothing between.
verify.expect(verify.save(SAVE), "save wrote " .. SAVE)

-- S2's "before". Panel open/closed state is deliberately NOT in the envelope,
-- so this is opened by the script on both sides rather than restored.
verify.show_panel("generation_ledger", true)
verify.frames(3)
verify.capture("save_load_genledger_before")
verify.show_panel("generation_ledger", false)
verify.frames(1)

-- ---------------------------------------------------------------------------
-- Scramble, so a load that silently did nothing fails loudly.
-- ---------------------------------------------------------------------------
-- Without this, a no-op load would leave the correct view on screen and both
-- captures would match for the wrong reason.
verify.set_overlay("market")
verify.set_zoom(1.0)
verify.set_pan(0, 0)
verify.click_tile(30, 20)
verify.frames(3)
verify.capture("save_load_scrambled")

-- ---------------------------------------------------------------------------
-- S1 / S3 — load, and the view is the saved one again.
-- ---------------------------------------------------------------------------
verify.expect(verify.load(SAVE), "load read " .. SAVE)
verify.frames(3)
verify.capture("save_load_view_after")

-- The world half, corroborated in-app. The headless harness asserts this too;
-- asserting it here as well distinguishes "the picture matches" from "the
-- picture matches AND it is the same world", which a re-render of a half-loaded
-- world could otherwise fake.
verify.expect(verify.state_hash() == hash_before,
              "state_hash unchanged across the round trip: "
              .. hash_before .. " vs " .. verify.state_hash())

-- ---------------------------------------------------------------------------
-- S2 — the Generation Ledger, opened the same way as before.
-- ---------------------------------------------------------------------------
verify.show_panel("generation_ledger", true)
verify.frames(3)
verify.capture("save_load_genledger_after")
verify.show_panel("generation_ledger", false)
verify.frames(1)

-- ---------------------------------------------------------------------------
-- S4 — a refused load costs the player nothing.
-- ---------------------------------------------------------------------------
-- A missing file must be a posted failure, not a crash and not a blank world.
verify.expect(verify.load("build_gen/verify/definitely_not_a_save.iosave") == false,
              "a missing save is refused rather than opened")
verify.frames(3)
verify.capture("save_load_after_failed_load")

verify.expect(verify.state_hash() == hash_before,
              "a REFUSED load left the live campaign untouched")

-- ---------------------------------------------------------------------------
-- S5 — the F5 / F6 quick-slot bindings, through the real dispatch
-- ---------------------------------------------------------------------------
-- `verify.command` goes through app::dispatch_action, which is the same function
-- the key table calls — so this exercises everything a real F5 press does except
-- SDL's delivery of the scancode. Worth having: the bindings are the only part
-- of this item a player actually touches, and a command wired into the table but
-- not into the dispatch would look exactly like a working one from the outside.
verify.set_overlay("resource")
verify.set_zoom(1.35)
verify.frames(2)
local hash_quick = verify.state_hash()
verify.capture("save_load_quickslot_before")

verify.command("quick_save")
verify.frames(1)

-- Move away, then bring it back with the load binding alone.
verify.set_overlay("population")
verify.set_zoom(1.0)
verify.click_tile(60, 40)
verify.frames(2)

verify.command("quick_load")
verify.frames(3)
verify.capture("save_load_quickslot_after")

verify.expect(verify.state_hash() == hash_quick,
              "F6 quick-load restored what F5 quick-saved")

-- ---------------------------------------------------------------------------
-- S6 — the bindings are in the table the KEY HANDLER reads
-- ---------------------------------------------------------------------------
-- The step above proves the dispatch. It does not prove that F5 and F6 reach
-- that dispatch, because it calls the command by name rather than by key. The
-- F1 overlay closes exactly that gap: it is GENERATED from `s_bindings`, the
-- same array `handle_key_down` looks the scancode up in, so a command wired
-- into the dispatch but missing from the table would be absent here.
--
-- (This capture is also the first thing to exercise `help_toggle` through
-- verify.command, which silently did nothing until BL-536 routed the hook
-- through dispatch_action — see NR-513.)
verify.command("help_toggle")
verify.frames(2)
verify.capture("save_load_bindings_overlay")
verify.command("help_toggle")
verify.frames(1)
