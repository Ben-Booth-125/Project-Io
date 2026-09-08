# REFINED — active worklist

**Empty.** The whole board was cleared on 2026-09-08 on Ben's call: too many items hung off sprint
plans that should have been better planned, so the live backlog (31 items), the review queue (6
entries) and the sprint list (32c, 33, 34) were all archived. Nothing carries forward as a
commitment, and the next planning pass starts from a clean sheet.

Nothing to promote from, and nothing lost. The cancelled rows are whole and cold in
`docs/development/archive/backlog-design-2026-Q3.json` — read them with
`node tools/session/backlog_query.js --status cancelled --full`, or bring them all back with
`node tools/session/archive_landed.js --restore`. The sprints are in
`archive/sprints-2026-Q3.json`; sprint 33 closed **retro-recorded**, because it ran — 16 items
delivered — and its retro is the record of that. The next new sprint is **35**.
