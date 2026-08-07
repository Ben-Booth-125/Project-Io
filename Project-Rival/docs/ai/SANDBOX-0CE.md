# The 0 CE Sandbox Battery — phrasing tests for buildings (and, later, units)

**Date:** 2026-08-06. **Status:** Battery A run 1 executed 2026-08-06 over the live seam —
10/10 on the action axis, two stance-label divergences; see
[`annals/battery-0ce-A-run1.md`](../../annals/battery-0ce-A-run1.md). Battery B still
blocked on BL-271 (Era −1 sim).

**Purpose:** test whether the phrasing resolutions in [`PHRASINGS.json`](PHRASINGS.json)
survive a **register shift** — the same seven stances, voiced in the year-zero idiom the
campaign uses (divine counsel, gods sending builders). Register drift is the eighth axis,
kept out of the core sub-dictionary deliberately: the battery owns it.

## Why 0 CE

The campaign rite already narrates in this register (`docs/CAMPAIGN.md`, divine-framing
rule). If a compressed local model can resolve *"Let a quarry be raised where the hill
bleeds ore"* to `gameplay.build {type: extraction_site, target: stone/ore}`, the stance
taxonomy is doing its work under the hardest phrasing the project will actually use.
The mechanism transfers; per the standing naming rule, no Earth proper nouns enter Io.

## Scope honesty — units

Io's action dictionary has **no unit actions**: the gameplay family is buildings, roads,
sell orders and survey. Units arrive with BL-271 (Era −1 sim) and BL-274 (era-keyed
rosters). The unit section below is **design-owed** — battery slots reserved, unrunnable
until those land. Do not fake unit tests against the current seam.

## Method

Each test row: an era-voiced sentence → the expected `(action id, binds, via)` → run
through the resolver (today: a prompted model holding `ACTIONS_INDEX.json` + the
PHRASINGS stance taxonomy; later: the compressed local model). **Pass** = resolves to
the expected action with correct binds; **instructive fail** = wrong action but a
defensible reading (record it — it may be a missing reading, not a model fault);
**fail** = illegal or unbound resolution.

Score each row on the stance it exercises, so failures localise to an axis rather
than to "phrasing" generally.

## Battery A — buildings (runnable now)

| # | Era-voiced instruction | Expected resolution | Stance |
|---|---|---|---|
| A1 | "Let a quarry be raised where the hill bleeds ore." | `build {extraction_site, target: the tile's deposited ore}` | imperative + deictic (site by deposit) |
| A2 | "The granaries must fill before the fleet is laid down." | `place_sell_order {food, metered}` or withheld surplus toward a future `build` — rationing stance | future-anchored |
| A3 | "The forge devours silver and returns nothing; make it serve or make it sleep." | `set_recipe` first, fall through to `idle` | corrective (candidate set) |
| A4 | "Send the diviners to the far shore; we build nothing on ground we have not read." | `survey {the unsurveyed body/region}` | future-anchored (enabling press) |
| A5 | "A road of stone from the mines to the river market, that the carts cease to ruin us." | `place_road {route tiles, tier by saving}` | corrective |
| A6 | "Raise a second forge beside the first, its twin in every part." | `build` via the 'Build another here' press | deictic |
| A7 | "The old shrine yields nothing and never shall; return its ground to us." | `demolish` (past idle — 'never shall' removes option value) | corrective, disambiguated |
| A8 | "Let the smiths rest until the price of iron returns." | `idle`, resume trigger pre-registered | imperative + future trigger |
| A9 | "The gods favour iron again; wake what already stands." | `resume {idled iron buildings}` | outcome |
| A10 | "Half the harvest is to be held against the temple's raising; the rest may go to market." | `place_sell_order {quantity: half surplus, floor 0}` | future-anchored (rationing) |

## Battery B — units (design-owed, blocked on BL-271 / BL-274)

Reserved slots, same method: levy/raise (unit analogue of build), march (route analogue
of place_road binds), garrison/disband (idle/demolish analogues), scout (survey
analogue). The building↔unit analogy is itself a finding to test: if the stance
taxonomy transfers across the analogy unchanged, the sub-dictionary is era-portable —
exactly what BL-274 (era-keyed rosters) needs.

## Recording

Results go in an annal-adjacent record under `annals/` (a battery run is a campaign
act); instructive fails feed back into PHRASINGS.json as new readings. Findings that
touch Io (missing verbs, resolver gaps) go to Io's review queue as usual.
