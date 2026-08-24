-- Make the UI-review save game, and prove what a load of it does and does not
-- bring back.
--
-- WHAT IT DOES. Stages the shared review world (`stage_ui_fixture`, lib.lua),
-- captures it LIVE, writes it to a snapshot, loads that snapshot straight back,
-- and captures it again. The pair of captures is the point: they are the same
-- world one save apart, so anything that differs between them is something the
-- round trip lost.
--
-- WHAT THE PAIR SHOWED when this was written (2026-08-24), and why `shell_pass.lua`
-- stages its own world rather than opening this one:
--   * the canvas comes back DIMMED. `world::current_day_tick` is re-seeded on load
--     from the sim loop's day tick, and `verify.econ_step` advances the world's
--     mirror without ever advancing the sim loop — so a snapshot taken after N
--     ticks reloads at day 0 and the activity fog reads every glimpse as stale.
--     It is also why `state_hash` does not round-trip once any tick has run: the
--     hash is keyed on that tick. Filed as NR-608.
--   * the comms dock comes back EMPTY. The chat log is not in the save envelope;
--     `save_game.hpp` excludes "hover and chat state" as transient view state,
--     which is true of hover and is not true of the LOG. Filed as NR-609.
--
-- The snapshot lives under `build_gen/` (gitignored) because it is a rebuildable
-- cache, not an asset — and because a save is rejected outright when the format
-- version moves, which is the guard that keeps a stale one from being opened.
--
-- Run: ProjectIo --verify scripts/verify/ui_shell_fixture.lua

verify.window(1280, 720)

local FIXTURE = "build_gen/verify/ui_shell_fixture.iosave"

stage_ui_fixture()

local live_hash    = verify.state_hash()
local live_balance = verify.player_balance()
print("FIXTURE live_hash=" .. live_hash)
print("FIXTURE live_balance=" .. tostring(live_balance))
shot("ui_fixture_live")

verify.expect(verify.save(FIXTURE), "fixture written to " .. FIXTURE)
verify.expect(verify.load(FIXTURE), "fixture read back from " .. FIXTURE)
verify.frames(3)

print("FIXTURE loaded_hash=" .. verify.state_hash())
print("FIXTURE loaded_balance=" .. tostring(verify.player_balance()))
shot("ui_fixture_loaded")

-- The world half IS intact: the balance comes back to the bit. Asserting that
-- here is what separates "the save is broken" from "the save is whole and two
-- app-layer mirrors are not" — the actual finding, and a much smaller one.
verify.expect(verify.player_balance() == live_balance,
              "balance survives the round trip exactly: "
              .. tostring(live_balance) .. " -> " .. tostring(verify.player_balance()))

-- And the hash does NOT, which is the symptom NR-608 names. Asserted in the
-- direction it actually holds rather than the direction it ought to: a check
-- that claims the round trip is clean would be a check that lies.
verify.expect(verify.state_hash() ~= live_hash,
              "state_hash moves across the round trip (NR-608): "
              .. live_hash .. " -> " .. verify.state_hash())
