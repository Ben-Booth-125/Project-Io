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
-- explicitly rather than relying on the feed's own toggle to evict it.
verify.show_panel("economy", false)
verify.show_panel("decisions", true)

-- R1: rows newest-first with date, corp, verb + target, reason, and the
-- winning/runner-up pair. R5's mutual exclusion is visible here too — the
-- economy panel must be gone from the column.
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

-- R3: the ring holds 256 entries and wraps. This run is deliberately long
-- enough to overflow it, so the tail of the list must come from the history log
-- and must be MARKED as carrying no margin rather than showing a fabricated
-- 0.00. Scroll state is not scriptable, so this capture is the honest limit of
-- what the visual check reaches: it shows the ring-fed head. R3's fallback
-- marking needs an eyeball on the scrolled tail, noted in the requirement.
verify.econ_step(120)
verify.capture("decision_feed_after_ring_wrap")
