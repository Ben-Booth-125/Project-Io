# The Campaign — the yearly rite (2026-08-03)

One prompt series per campaign year. Six stations, in order, every year, no skipping —
the regularity is what makes the record an *annal* rather than a war diary.

## The framing law

**Soldiers move because gods send them. Political intrigue is banned as a cause.**

This is the project's one aesthetic absolute, and it is honest: in the RTS the true causes of
events are terrain, supply, unit odds, and bot policy; in Io's creeds system war gods literally
drive battle odds and a won war plants the victor's pantheon. Divine dispatch is not a gloss
laid over the mechanism — it *is* the mechanism, narrated natively.

Both sides get their own theology (dossier: `docs/RIVAL-ROME.md`):

- **Our voice (Han):** Heaven withholds soldiers except to restore harmony. A dispatch is
  Heaven's favour redeployed; an omen is strategic intelligence; the civilising mission is
  *laihua* — the distant transformed by virtue, arriving as tribute, not as subjects taken.
- **The rival's voice (Rome):** their augurs license campaigns, their Mars avenges, their rite
  of *evocatio* tries to call our gods over to their side. Roman moves are narrated as their
  priests would claim them — which at year zero is period-accurate propaganda, and flagged as such.

The bench of record is Pantheon's, cross-tradition by design: the mountain bench counsels
(Sun Tzu deterrence, Amaterasu withdrawal-pricing, Krishna coalition arithmetic), the banner
and hearth benches check them (Ares morale, Zhu Bajie desertion, Hestia stakes, Baldr grievance),
and the annal is voiced in the campaign tradition — classical Chinese, per the voices corpus,
with its fidelity rules.

## The six stations

**1. Omens.** Read the state as portents: minimap, economy panel, army positions, the rival's
visible works. No inference beyond what is seen — fog is fog, and the memorial to the throne
records gaps as gaps (Muninn's law).

**2. Council.** Put the year's question to the bench. Counsel is sought in character but
scored on content; genuine disagreement between benches is the desired texture, resolved into
a conditional decision rule, never a slogan. Standing precedent applies until an annal overturns it — doctrine-001 (align with the weaker),
noting its alignment verdict needs three or more forces and so lies dormant in a 1v1 arena;
its Hestia survival-test clause (survival before position) binds every year regardless.

**3. Dispatch.** Written orders, each in the form: *which god sends which soldiers to which
place, to what end.* Concrete, checkable, few — three dispatches is a busy year. These are the
prompts that affect military strategy; everything else is commentary.

**4. The season.** Play, via computer-use, until the next year mark. Execute the dispatches;
improvise only within their intent; note every departure for the accounting.

**5. Accounting.** Outcomes against dispatches, plainly: held, taken, lost, spent. Zhu Bajie
prices the desertions, Hestia names what must still be standing, the crow-murmuration grades
what was claimed against what befell. (Candidate hires — Hine-nui-te-pō's endings ledger, say —
may be consulted in prose, but only seated personas write codebook records.) This station's
output **is** the annal's Accounting section; decisions taken on Ben's behalf are logged here,
at the moment of taking.

**6. The annal.** The year's chronicle entry, in voice, to `annals/` (format:
`annals/README.md`). Then the data: phrase-bank candidates, codebook v3 records, doctrine
amendments. The annal is the deliverable; the match is its raw material.

## The prompt templates

The year-opening prompt (stations 1–3, issued at each year mark):

```
Year <N> of the campaign of <war-name>. The season opens.

OMENS — read the field and report as portents: our holdings, our hosts,
the rival's visible works, and what the fog still withholds. Gaps are gaps.

COUNCIL — put to the bench: <the year's question, one sentence>.
Mountain speaks first, banner and hearth answer. Cite precedent by name
(doctrine-001 and any annal-born rule). Resolve to a conditional rule.

DISPATCH — issue at most three orders, each in the form:
  <God> sends <soldiers/works> to <place>, that <purpose>.
No order may cite a court, a faction, or an intrigue as its cause.
```

The season-closing prompt (stations 5–6, issued before the next year mark):

```
Year <N> closes.

ACCOUNTING — each dispatch: held / taken / lost / spent, in numbers where
the screen gives them. Name every departure from an order and its reason.
Log any call taken on Ben's behalf, now.

ANNAL — write the year's entry per annals/README.md: chronicle in voice,
then the accounting (station 5's output, plain prose), then phrase-bank
candidates (flagged attested-adapted/invented), then codebook records,
then any doctrine amendment the year has earned.
```

These templates are the seed of the series, not its ceiling — each campaign may refine them,
and the refinement is itself recorded in the annal that made it.

## Rite conformance — the yearly self-check

The rite is itself under test, so each year measures whether it ran clean. Eight checks,
scored pass/fail at station 6 by the playing session — the rubric is mechanical on purpose:

| # | Check | Pass = |
|---|---|---|
| R1 | **Stations** | All six stations ran, in order, none skipped. |
| R2 | **Framing** | Every chronicle cause is a divine dispatch; no intrigue, no godless cause in voice. |
| R3 | **Dispatch form** | At most three orders, each naming god, soldiers/works, place, and purpose. |
| R4 | **Godless accounting** | The accounting is plain numbers — no gods, no voice. |
| R5 | **Fidelity flags** | Every phrase-bank candidate flagged `attested-adapted` or `invented`; nothing faked in an unverifiable tongue. |
| R6 | **Muninn's law** | Omens reported only what the screen showed; fog recorded as gaps. |
| R7 | **Live logging** | Calls taken on Ben's behalf logged in the accounting at the moment of taking, not at close. |
| R8 | **Evidence** | Seed, map, and replay reference recorded; the annal cites its replay. |

**Grading model (decided 2026-08-03): self-score with spot-check.** The playing session scores
R1–R8 itself; the match outcome remains the external verifier of doctrine; Ben spot-reviews
annals at his own cadence. A failed check is *recorded, never repaired* — the no-rewrite rule
applies to the score as to the chronicle.

Fails go in the annal's frontmatter (`conformance.fails`) and roll up to
`annals/campaigns.json`, the campaign-level aggregate. The ten-year test below reads that
file, which is what makes it decidable rather than a feeling.

## The refinement contract

What flows back, and to where:

- **Phrase-bank lines** (native idiom for battles, dispatches, omens, tribute) → proposed to
  Pantheon `data/voices/`, fidelity-flagged, never edited in directly.
- **Doctrine records** (codebook v3 JSONL, one line per verdict) → proposed to Pantheon
  `data/thoughts/`, successors to doctrine-001.
- **Persona performance** (which counsel proved out, which bench was blind) → notes for the
  persona packs (BL-207) — a persona's campaign record is authoring-time data.
- **Calibration observations** (season structure, supply radii, hegemony speed) → proposed
  backlog notes for BL-271 (Era −1 sim); constants checks against the landed BL-272 (mil-sim)
  engine; doctrine rows toward BL-274 (era-keyed unit rosters).

The test of the whole rite: after ten years of annals, the oral history should be *richer where
it was vague* — idioms with referents, doctrines with scars, personas with records. If it is
merely longer, the rite has failed and the accounting must say so.

That test is read off `annals/campaigns.json`, not off an impression: idioms with referents =
phrase-bank candidates each carrying the moment that earned them; doctrines with scars =
amendments earned by recorded losses; personas with records = counsel verdicts accumulated in
the codebook lines. A war whose aggregate shows only rising year counts has failed the test.
