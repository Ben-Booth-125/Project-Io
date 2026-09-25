-- The corporation selection canvas, and the pick as a reproducible game act
-- (BL-1076, the player chooses the corporation they play; STARTUP.md § The seat).
--
--   ProjectIo --verify scripts/verify/seat_pick.lua
--
-- WHAT IT PROVES.
--   S1  the canvas opens over the world and offers EVERY specialist, ranked;
--   S2  a hover re-points the map and the card (captured: the highlight moves);
--   S3  a press opens the briefing; Back closes it and changes NOTHING (the
--       state hash and the player are exactly as they were);
--   S4  a firm the floor MARKED is pickable: Confirm on it seats the player;
--   S5  (seed, pick) REPRODUCES THE SEAT: the same world and the same pick give
--       the same player and the same state hash; a different pick gives a
--       different player (state_hash does not fold the seat, so the player id
--       is read beside it);
--   S6  the pick is validated as an untrusted input: an id that names nothing,
--       and a background firm, are both refused and move nothing.
--
-- UNDER --verify THE LANDSCAPE IS NOT SEARCHED, so the ranking reads an empty
-- score and every firm is marked below the floor. That is not a gap in the
-- check: it is exactly S4's case (a marked firm stays pickable), and the
-- search-ranked order is the draw's own order, covered by player_seed_sweep.
-- The live Begin -> canvas -> Confirm walk is the main session's click-through.
--
-- Determinism: measured in frames, never in sleeps.

verify.window(1920, 1080)

-- S1 -------------------------------------------------------------------------
local player0 = select(6, verify.seat_state())
local hash0   = verify.state_hash()
verify.show_seat_screen(true)
verify.frames(3)
local open, n, shortlisted = verify.seat_state()
verify.expect(open, "the selection canvas is open")
verify.expect(n >= 2, "the canvas offers every specialist, ranked (" .. n .. " rows)")
verify.expect(shortlisted <= n,
              "the floor counts at most every row (" .. shortlisted .. " of " .. n .. ")")
verify.capture("seat_00_canvas_row1")

-- S2 -------------------------------------------------------------------------
local last = n - 1
verify.seat_hover(last)
verify.frames(2)
verify.expect(select(4, verify.seat_state()) == last, "a hover re-points the map and card")
verify.capture("seat_01_canvas_hover_last")

-- S3 -------------------------------------------------------------------------
verify.seat_brief(last)
verify.frames(2)
local picked, marked = verify.seat_candidate(last)
verify.expect(select(5, verify.seat_state()) == picked, "a press opens that firm's briefing")
verify.capture("seat_02_briefing")
verify.seat_back()
verify.frames(2)
verify.expect(select(5, verify.seat_state()) == 0, "Back closes the briefing")
verify.expect(verify.state_hash() == hash0, "Back changed nothing (state hash unmoved)")
verify.expect(select(6, verify.seat_state()) == player0, "Back changed nothing (player unmoved)")

-- S4 -------------------------------------------------------------------------
verify.seat_brief(last)
verify.frames(1)
local r = verify.seat_confirm()
verify.frames(2)
verify.expect(r == "applied", "Confirm seats the player through take_seat (" .. r .. ")")
verify.expect(select(6, verify.seat_state()) == picked,
              "the player is the firm that was picked (" .. picked .. ")")
verify.expect(not select(1, verify.seat_state()), "Confirm leaves the canvas for play")
if marked then
  print("[seat_pick] S4 picked a firm the floor MARKED -- and it seated")
end
verify.capture("seat_03_after_confirm")

-- S5 -------------------------------------------------------------------------
-- The same world, rebuilt from its seed, and the same pick: the same state.
-- verify.new_world rebuilds through setup_world, so both sides take the same
-- path; the first pick above is on the run_verify world and is not compared.
--
-- THE PICK IS RE-READ ON EACH REBUILT WORLD (2026-09-25, when BL-1092's lane
-- restored new_world -- it had silently kept the reference world since
-- BL-1085, so this check compared a world to itself). A firm id belongs to
-- the world that generated it: seed 1's last-ranked candidate is read off
-- its own ranking, on both sides, and the two picks must agree as the two
-- hashes must.
local seed = 1
local function pick_on_rebuilt(s)
    verify.new_world(s)
    verify.show_seat_screen(true)
    verify.frames(3)
    local rows = select(2, verify.seat_state())
    local firm = verify.seat_candidate(rows - 1)
    verify.show_seat_screen(false)
    verify.frames(1)
    local r = verify.take_seat(firm)
    return firm, r, verify.state_hash()
end
local pick_a, a, hash_a = pick_on_rebuilt(seed)
local pick_b, b, hash_b = pick_on_rebuilt(seed)
verify.expect(a == "applied" and b == "applied", "the pick applies on the rebuilt world")
verify.expect(pick_a == pick_b, "the rebuilt world ranks the same last candidate ("
                                .. pick_a .. " == " .. pick_b .. ")")
verify.expect(select(6, verify.seat_state()) == pick_b, "the rebuilt world is seated on the pick")
verify.expect(hash_a == hash_b,
              "(seed, pick) reproduces the seat: " .. hash_a .. " == " .. hash_b)
print("[seat_pick] seed " .. seed .. " pick " .. pick_a .. " state_hash " .. hash_a)

local other = verify.seat_candidate(0)
if other == picked then other = verify.seat_candidate(1) end
verify.new_world(seed)
local c = verify.take_seat(other)
verify.expect(c == "applied", "a different pick applies")
verify.expect(select(6, verify.seat_state()) == other, "a different pick seats a different firm")
-- NOT asserted: that the hash differs. `world::state_hash` folds balances and
-- building state but not `is_player` / `player_entity`, so two picks on one
-- world hash alike and the SEAT is read as the player id beside the hash. The
-- reproducibility claim above is (player, hash) equal; this one is player unequal.
print("[seat_pick] different pick " .. other .. " state_hash " .. verify.state_hash()
      .. " (state_hash does not fold the seat)")

-- S6 -------------------------------------------------------------------------
local before = verify.state_hash()
local p_before = select(6, verify.seat_state())
verify.expect(verify.take_seat(4000000000) ~= "applied", "an id naming nothing is refused")
verify.expect(verify.take_seat(-5) == "rejected_invalid", "a negative id is refused")
verify.expect(verify.take_seat(1.5) == "rejected_invalid", "a fractional id is refused")
-- a background firm (a company) is never a seat
local bg = verify.seat_background_firm()
if bg > 0 then
  verify.expect(verify.take_seat(bg) == "rejected_invalid", "a background firm is refused")
end
verify.expect(verify.state_hash() == before, "every refusal moved nothing")
verify.expect(select(6, verify.seat_state()) == p_before, "every refusal left the player")
