---
name: ruling-check
description: Run the sprint-close doc check (Ben, 2026-10-08; made a skill 2026-10-10) - every dated ruling "(Ben, YYYY-MM-DD" in the authority docs is listed and checked for state-dependent words and bare BL ids, and a sprint's ruling register is checked so each ruling is written in its owning doc and no sibling doc still states the overturned claim. Use it at every sprint close, and after any batch of rulings, before the re-bless.
---

# ruling-check

The doc check Ben asked for at the sprint 50 close ("make sure all the various decisions are
documented ... add a check for docs"). `tools/session/ruling_check.js` is read-only: no writes,
no network.

## Run (repo root)

```bash
node tools/session/ruling_check.js --since 2026-10-07
node tools/session/ruling_check.js --register tools/session/rulings/sprint-50.json
```

- `--since DATE` LISTS every dated ruling on or after DATE in `docs/` (minus `docs/development`
  and `docs/research`), grouped by doc, and FLAGS a ruling line carrying a state-dependent word
  (landed, shipped, "until now", "not yet built", ...) or a `BL-` id with no short handle.
- `--register FILE` CHECKS a sprint's register. Each entry is
  `{ "ruling", "doc", "anchor", "stale": [regex...], "owner" }`: the anchor must match in its
  owning doc, and no `stale` regex (the OLD wording) may match anywhere in `docs/` outside
  `docs/development` - the same-day-ruling trap.
- `--json` for machine output. Exit 1 on any flag or failing entry.

## At a sprint close

1. Write the sprint's register, `tools/session/rulings/sprint-<N>.json`: one entry per ruling
   taken that sprint (git log "Ruling"/"Ben" commits and the handoff's BEN RULED lines), with
   the old wording each one overturned as `stale`.
2. Run both modes. Fix doc-only gaps in place; a ruling missing from its doc is written in.
3. **The code half is a reading, not a script.** For each ruling, read the cited code and say
   whether it matches. A mismatch is Ben's call (narrow the doc or widen the code) or a backlog
   item - never a silent doc edit.
4. Owners: a ruling's unbuilt part needs a backlog item (`backlog_query.js`).

## What it cannot see

Whether the code does what the doc says. Whether a ruling was taken but never written anywhere
(the register only checks rulings someone listed). Read the handoff and the commit log for those.
