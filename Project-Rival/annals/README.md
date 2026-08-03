# The Annals — format and rules (2026-08-03)

One file per campaign year: `annals/<war-slug>/year-<NN>.md`. The annal is the campaign's
durable output — the match is raw material, the replay is evidence, the annal is the history.

## The year record

```markdown
---
war: <war-slug>            # the named war this year belongs to
year: <N>                  # campaign year within the war
date: <YYYY-MM-DD>         # real date of play
seed: <match seed>
map: <TYPEDIR/MAPNAME>
replay: <replays-folder timestamp, once the year's match segment exists>
dispatches: <count issued>
verdict: <held | advanced | repulsed | lost>   # the year in one word
conformance:               # rite checks R1–R8 (docs/CAMPAIGN.md § Rite conformance)
  fails: []                # empty = all eight passed; else the failed ids, e.g. [R3, R7]
---

## Chronicle

(The year in voice — classical Chinese register per Pantheon
data/voices/classical-chinese.md, translation primary, native lines only
where fidelity practice allows. Gods send soldiers; no intrigue.)

## Accounting

(Station 5, plain modern prose: each dispatch — held / taken / lost /
spent, with numbers where the screen gave them. Departures from orders,
and why. Calls taken on Ben's behalf, logged at the moment of taking.)

## Phrase-bank candidates

(Lines worth proposing to Pantheon data/voices/, each flagged
`attested-adapted` or `invented`, with the moment that earned it.)

## Records

(Codebook v3 JSONL, one line per verdict/finding, per Pantheon
data/codebook.json — same grammar as doctrine-001.)

## Doctrine

(Only if the year earned an amendment: the rule, its condition, and the
annal evidence. Absence of this section means precedent stands.)
```

## Rules

**The chronicle obeys the framing law** (`docs/CAMPAIGN.md`): soldiers move because gods send
them. The accounting obeys the opposite law: plain numbers, no gods, no voice.

**Fidelity before flourish.** Native-language composition follows the voices corpus's honesty
practice — attested formulas preferred, least-certain constructions named, nothing faked in a
tongue we cannot verify. When in doubt, English carries the chronicle and the phrase bank waits.

**Nothing is rewritten.** A wrong annal gets a correcting entry in the next year's accounting,
never a silent edit — the error is part of the history (the record can be burned, not redrafted).

**A lost year is a full entry.** Defeat is the credential in this house; the annal of a rout
teaches more than the annal of a parade.

## The campaign aggregate

`annals/campaigns.json` holds one record per war — verdict tallies, doctrine amendments,
phrase-bank yield, conformance fails, calibration-note counts. It is the analytics substrate
over the annals (the `cycles.json` analogue from AgenticProcess research practice), updated at
station 6 as each year closes. It re-derives from the annals: if the two ever disagree, the
annals win and the aggregate is regenerated, never the other way round.
