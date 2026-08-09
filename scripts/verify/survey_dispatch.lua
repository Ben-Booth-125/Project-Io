-- Survey dispatch acceptance (BL-113, US-011).
-- Proves a player can EXPAND their knowledge of the world through the real commit
-- path: dispatch a survey of an unsurveyed body via the SAME dispatch_survey path
-- app::render runs when it consumes ui.pending_survey_dispatch (the Selection panel's
-- Survey button only enqueues; the real debit + schedule live in dispatch_survey),
-- then assert BOTH halves of the gamble the player is promised — credits are debited
-- upfront AND the reveal advances after the survey clock ticks. This is the coverage
-- the discovery flow lacked: no check dispatched a survey through the real path and
-- asserted the debit + region reveal together. Realises USER_STORIES.md US-011
-- ("survey determinism + cost/ETA scaling are headless").
-- Run: ProjectIo --verify scripts/verify/survey_dispatch.lua

-- Selene is Kepler's moon: unsurveyed at campaign start (init_survey_states opens
-- only home + the star), close and small, so its survey is affordable from the
-- starting balance.
local target = "moon"

local cost = verify.survey_cost_of(target)
verify.expect(cost > 0.0,
    "unsurveyed body " .. target .. " has a positive survey cost (got " .. cost .. ")")

-- Stage sufficient funds so the flow exercises the SUCCESS path (debit + reveal),
-- not the insufficient-funds guard: the starting balance does not cover an off-home
-- survey (a separate BL-112 starting-economy concern, deliberately not entangled
-- here). set_balance only moves the number; dispatch still debits the real cost.
verify.set_balance(cost + 5000.0)

local bal_before = verify.player_balance()

-- Dispatch through the real path: debits the cost upfront and arms the schedule.
local result = verify.dispatch_survey_of(target)
verify.expect(result == "success",
    "survey dispatched via the real dispatch_survey path (got: " .. result .. ")")

-- Credits debited upfront (US-011: "credits are debited upfront").
local bal_after = verify.player_balance()
verify.expect(math.abs((bal_before - bal_after) - cost) < 0.5,
    "survey cost debited upfront (delta=" .. (bal_before - bal_after) ..
    " expected " .. cost .. ")")

-- Advance the survey clock well past the transit + scan window via the real
-- advance_surveys path; the reveal must have progressed (regions_done climbs from 0).
verify.advance_survey_days(400)
local done = verify.survey_regions_done(target)
verify.expect(done > 0,
    "survey reveal advanced after ticking (regions_done=" .. done .. ")")

verify.goto_surface(target)
verify.capture("survey_dispatch")
