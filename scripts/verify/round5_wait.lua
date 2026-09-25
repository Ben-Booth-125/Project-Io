-- BL-1084 (world built once and moved) -- ROUND 5'S WAIT, PHOTOGRAPHED.
--
--   ProjectIo --verify scripts/verify/round5_wait.lua
--
-- WHY IT EXISTS. Every other wizard script parks a round with
-- verify.history_run, which ADOPTS the harness's own world at once -- so no
-- capture in the project had ever shown a round's wait. STARTUP.md § The wait,
-- then the lapse says the wait names the one step under way, and BL-1084 says
-- round 5's wait names ONLY the Exploration span, because the round now runs
-- only its own stage on the world round 4 closed (§ The world cache) rather
-- than re-running the ancient era first.
--
-- verify.history_wait(2) stages that for real: round 4's closing world is
-- built and held, round 5's own stage starts on a worker from a copy of it
-- (the wizard's own launch), and the call returns once the wait is on screen.
-- The bars and the inner year counter are wherever the worker has got to, so
-- the capture is for inspection, never a golden; the caption is asserted.
-- The elapsed count reads 0 s: every clock is frozen under --verify.

verify.window(1920, 1080)

local started = verify.history_wait(2)
verify.expect(started, "round 5's own stage is running on a worker, from a held copy of round 4's world")
verify.frames(2)
local round = select(1, verify.wizard_round())
verify.expect(round == 4, "parked on round 5, Exploration (0-based " .. round .. ")")
local caption = verify.history_wait_caption()
verify.expect(caption == "Running the exploration age",
              "round 5's wait names the Exploration span and nothing earlier (caption: '"
              .. caption .. "')")
verify.capture("round5_wait")

-- The run lands; its record plays on the same round.
verify.expect(verify.history_wait_land(), "round 5's run lands and its record is whole")
verify.frames(4)
verify.expect(verify.history_powers() > 0,
              "the landed record holds ground (" .. verify.history_powers() .. " powers)")
verify.capture("round5_landed")
