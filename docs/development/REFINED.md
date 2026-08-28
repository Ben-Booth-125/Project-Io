# Project Io — REFINED (active worklist)

*Empty between work blocks.* Sprint 22 (UI visibility, batch 1: lenses) closed
2026-08-28 — see `sprints.json` "22" and the DEVLOG. Sprint 21 (demand) is
**paused** with wave 0 landed. Sprints 23–27 are the remaining UI review
batches, proposed and unstarted; Ben opens 23 (selection & hover) as a
dedicated coding session.

**Carried out of batch 1, all with Ben's decisions already recorded** — these
start from a settled brief, not a re-litigation:

- `BL-659` (deposit selects to market ledger) — wired, **unverified**: the probe
  cannot find a tile carrying the lens resource (NR-698). Destination settled:
  Market ledger, Prices view, aimed at that resource.
- `BL-660` (tectonic hover and history route) — plate → History ledger **works**;
  the tectonic section it should aim at is unbuilt, and `continent_state`
  classifies `convergent` boundaries only, so the *divergent* half does not
  exist in the data yet. A plate also has no Selection-band content (NR-697).
- `BL-661` (population regional heatmap) — scaling 2 of 5, gentle end.
- `BL-662` (scarcity takes the opportunity glyph) — keeps the name Scarcity,
  re-cut to tint markets, routed to a new Market-ledger sub-view.
- `BL-663` (lens glyph roster) — Company glyph as a cluster of small forms;
  body-to-body lens glyphs deferred to sprint 26 with the lenses themselves.

**Open work with no promoted tasks:** `node tools/session/backlog_query.js --table`.
