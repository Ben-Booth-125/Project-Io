-- Verify the AI decision feed (BL-407 / ai-decision-feed R1, R2, R3, R5).
--
-- The feed reads world::ai_decisions and the history log's decision/agency
-- topics, so it is EMPTY until the strategic tier has actually run. Every
-- capture here therefore steps the economy first; a shot taken before the
-- ticks would show an empty ledger and pass vacuously.

-- Spectate BEFORE stepping: step_economy reads the flag per call, so setting it
-- afterwards would populate the ring from an ordinary played session while the
-- capture claimed to show a spectated one. Under spectate every corp is scored,
-- including the player's, which is what makes the field full enough to read.
verify.spectate(true)

-- 24 quarters. The strategic cadence is staggered (corp index % k == tick % k),
-- so a handful of ticks only exercises a slice of the field; this is enough for
-- every corp to have evaluated several times and for a spread of reasons to
-- appear rather than only the opening build rush.
verify.econ_step(24)

-- econ_step opens the economy panel; the column holds ONE occupant, so close it
-- explicitly.
--
-- R5 IS NOT CHECKED HERE, and saying so is the point. `show_panel` writes the
-- ui_state flag DIRECTLY — it never routes through `close_all_panels`, and there
-- is no verify hook that simulates a rail click. So the mutual exclusion below
-- is arranged by this script rather than demonstrated by the app: delete both
-- the `show_decision_feed` line from `close_all_panels` AND the
-- `close_all_panels(state)` call from nav_pane's case 11, and these captures
-- come out byte-identical. R5 is verified by INSPECTION only. Claiming
-- otherwise is the vacuous-green failure this project already paid for once
-- (BL-404, and interbody_pull_harness's own first run).
verify.show_panel("economy", false)
verify.show_panel("decisions", true)

-- R1: rows newest-first with date, corp, verb + target, reason, and the score
-- pair.
verify.capture("decision_feed_all")

-- R2: the reason filter narrows the list. 0 = best_build, the reason with the
-- most entries in an expansion-phase run, so the filtered list is non-empty
-- (a filter that captures an empty list demonstrates nothing).
verify.decision_filter(0)
verify.capture("decision_feed_filtered_build")

-- R2 again on a scarcer reason: 3 = dial_idle, the solvency-defence tier. Its
-- absence is as informative as its presence — a run with no idles means nothing
-- was losing money, and the capture should show an honestly empty list rather
-- than a crash or a stale one.
verify.decision_filter(3)
verify.capture("decision_feed_filtered_idle")

-- Clear back to "all" (R2's clear leg).
verify.decision_filter(-1)
verify.capture("decision_feed_cleared")

-- R3 IS DELIBERATELY NOT CHECKED HERE (Ben, 2026-08-14). The obvious way to
-- reach it is to run past the 256-entry ring so the tail falls back to the
-- history log — and that leg was written, run, and then cut. It more than
-- doubled the script's runtime to produce a capture showing the ring-fed HEAD,
-- which the captures above already show: scroll position is not scriptable, so
-- the log-fed tail and its no-margin marking never entered frame. The most
-- expensive part of the check verified the least, which is the wrong trade at
-- any price. R3 stays an eyeball item until a verify hook can park scroll
-- offset; the requirement's notes say so rather than implying coverage.
--
-- NO GOLDEN. This check runs capture-only, on demand, when someone touches the
-- feed. A golden over a list of AI decisions would go red on every corp_ai
-- tuning change, every scorer candidate-set change and every world-gen change —
-- none of which are this surface — and a golden that is always red teaches
-- blessing without looking, which is how a real regression gets waved through.
