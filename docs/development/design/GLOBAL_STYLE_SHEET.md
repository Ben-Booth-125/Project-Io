# Global Style Sheet

Status: **draft — iteration 0, nothing settled yet.**
Owner: Joe. Sprint: ST1 (`docs/development/sprints.json`).

Working doc for narrowing Project Io's visual language to one specific style + UX
direction. Rules here are proposals until a render has actually been judged against
them (ST1's `done_when`) — then mark them **SETTLED** with the render that decided it.

Composite example renders (one 'screenshot' of the finished game — UI, map, icons,
tiles etc. together) live in `docs/development/design/renders/`, one per iteration,
named `iter-01.png`, `iter-02.png`, etc.

---

## Iterations

| # | Render | What changed | Verdict |
|---|--------|--------------|---------|
| 0 | — | starting point, no render yet | — |

## Colour guide

*Unsettled.* Fill in once a palette survives a render.

| Role | Colour | Hex | Notes |
|---|---|---|---|
| Background | | | |
| Panel / chrome | | | |
| Primary text | | | |
| Secondary / muted text | | | |
| Accent | | | |
| Positive / warning / danger | | | |

## Global rules

*Unsettled.* Candidates to test against a render, not commitments:

- Corner style (flat vs rounded).
- Button treatment (filled / outline / minimal-transparent).
- Type scale and base text size.
- Iconography weight/line-style, and how it should read at map scale vs panel scale.
- Tile/terrain rendering style (flat colour, textured, shaded).

## Open questions

- [ ] What should the map itself look like at a glance — abstract/schematic or textured/representational?
- [ ] Icon style: does it match the UI chrome style, or is it allowed to differ (e.g. more detailed on-canvas glyphs vs flat panel icons)?
- [ ] Does the style vary by canvas (Solar / Circumplanetary / Planetary) or stay uniform?

## Promotion

Once a direction is settled, propagate it into the real UI authority docs
(`docs/ui/*`) and/or a backlog item — this file stays the design-in-progress
record, not a permanent authority.
