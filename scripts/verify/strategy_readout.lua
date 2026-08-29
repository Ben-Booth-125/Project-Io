-- Verify the Strategy readout (BL-411 / emergent-strategy-readout R1).
--
-- The readout aggregates the same corp_decision stream the decision feed
-- lists, one sample per econ tick, so it is EMPTY until the strategic tier has
-- run. Every capture steps the economy first; a shot taken before the ticks
-- would show the honest "no decisions yet" text and pass vacuously.
--
-- WHAT A READER SHOULD SEE (and what R1 names): verb-mix bars, the
-- must/should/nice bucket split, and reason tallies — and NO score or margin
-- figures anywhere (the NR-226 fence: runner_up can legitimately exceed
-- winning_score because candidates sort by priority bucket before score, so
-- any margin aggregate would launder a bucket artefact into a statistic).

-- Spectate BEFORE stepping (decision_feed.lua's reasoning verbatim): the flag
-- is read per econ step, and under spectate every corp is scored — the
-- player's included — which is what makes the field full enough to read.
verify.spectate(true)

-- 24 quarters: enough for every corp to have evaluated several times under the
-- staggered cadence and for a spread of reasons beyond the opening build rush.
verify.econ_step(24)

-- The fold-out column holds ONE occupant.
-- As with the feed's script, the mutual exclusion is ARRANGED here, not
-- demonstrated — show_panel writes the flag directly and never routes through
-- close_all_panels, so the rail's one-occupant rule stays an eyeball item.
verify.show_panel("strategy", true)

-- R1, all-corporations view: one bucket-split share bar per corp with window
-- counts — the at-a-glance "who is defending solvency, who is expanding" read.
verify.capture("strategy_readout_all")

-- R1, single-corp profile: the bucket-split-by-quarter band, the verb-mix
-- bars, and the reason tally. -1 = the player corp, which always exists and
-- (under spectate) always has decisions; a script cannot name a generated
-- rival's id.
verify.strategy_filter(-1)
verify.capture("strategy_readout_player")

-- Back to the comparison view (the selector's clear leg).
verify.strategy_filter(0)
verify.capture("strategy_readout_cleared")

-- NO GOLDEN, for the decision feed's reason verbatim: a golden over an
-- aggregation of AI decisions goes red on every corp_ai tuning change, every
-- scorer candidate-set change and every world-gen change — none of which are
-- this surface — and a golden that is always red teaches blessing without
-- looking. Capture-only, run on demand when someone touches the readout.
