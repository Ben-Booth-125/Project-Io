# Project Rival — Claude Reference

Project Rival is the AI-player discipline for Project Io, run as **live play, not shipped code**.
A session (this kind of session) plays a strategy game through interactive prompts — via the
game's text seam where one exists, via computer-use where none does.

**Scope of that constraint (re-amended 2026-08-04, NR-057).** The 2026-08-03 premise — that
0 A.D. exposes no agent interface — was wrong: since Alpha 24 the engine ships
`--rl-interface`, an official HTTP seam driven by the in-tree `zero_ad` Python client. Playing
through it patches nothing and hooks nothing, so **text-only play is the preferred mode**: it is
the same read/meaning/write triple Io's own MCP server (`docs/ai/AI_OPPONENT.md` § 10) exposes,
rehearsed against a real opponent. Computer-use stays available for visual play, as fallback
rather than rule. Rival still never patches or hooks the game it is playing.

**The arena is Project Io itself (re-based 2026-08-04, NR-060).** "0 AD" names the *year* —
Io's Era −1 sandbox (BL-271), not Wildfire Games' RTS; the RTS bench was installed and retired
the same day (`docs/ENVIRONMENT.md` banner). Rival plays through Io's word interface — the MCP
seam today, the antiquity sandbox when BL-271 (Era −1 sim) lands, with the diplomacy seam
(BL-297) as the campaign's growing edge.

The match stays set at year zero: a **civilising-mission** power (our voice, *imperium sine
fine* under the auspices) against a **self-preservation** power (the rival's, the era's larger
creed). The theologies, dossier and rite transfer unchanged; only the board is now Io's own.

*(Flipped 2026-08-03 by Ben, NR-042 — the campaign was seeded playing Han against Rome. The
liturgy, annal format and dossier all transfer; what changed is which theology narrates our
dispatches.)*

The campaign's product is not victory. It is a **refined oral history**: yearly prompts that
direct military strategy in the voice of gods sending soldiers, recorded as annals that feed
Pantheon's phrase banks and Io's generation layer (BL-210, the oral-history pivot).

## Purpose

Prove, by play, the method Io's AI opponent will need: divine counsel in, legible strategy out,
every decision narrated in a tradition's own idiom. Refine the Pantheon corpus against a real
opponent instead of a blank page.

## Scope

Rival owns its campaign method, its annals, its dossiers, and its test-environment setup.
Everything under `Project-Rival/` is in scope; outside it, the only sanctioned write is an
append to Io's review queue (`docs/development/NEEDS_REVIEW.json`, mirror re-rendered).

Paths in these docs are relative to `Project-Rival/` unless absolute — write them that way.

Rival **never writes Project Io source**. Findings flow back as proposed backlog items or
review-queue entries, honouring Io's standing rule against building AI faction behaviour
beyond the sanctioned stubs.

Pantheon assets (`C:/Users/benbo/Pantheon`) are consumed read-only. Refinements — new phrase-bank
lines, doctrine records, persona-performance notes — are proposed back to Pantheon, not edited in.

## Documents

**`docs/MISSION.md`**
Why this project exists: the aim, the bridge to Io, and what "refine the oral history by play" delivers. Start here.

**`docs/ENVIRONMENT.md`**
The 0 A.D. test environment: install state, match template (Rome vs Han), the year cadence, headless and visual modes, replays as records.

**`docs/CAMPAIGN.md`**
The yearly rite — the prompt series. Six stations per campaign year, the divine-framing rule, the prompt templates, and the oral-history refinement contract.

**`docs/RIVAL-HAN.md`**
The rival dossier: Han's cosmology of preservation (the creed we are set against), Rome's theology of expansion (ours), the ideological war between them, and the other live creeds of the year-zero world. Contested history is flagged as such. *(Renamed from `RIVAL-ROME.md` 2026-08-03 when the played civ flipped.)*

**`annals/README.md`**
The annal format — one record per campaign year, chronicle plus data. The campaign's durable output lives here.

**`docs/ai/ACTIONS.json`** (pulled 2026-08-06)
Working mirror of Io's `docs/ai/ACTIONS.json` — the action dictionary an AI player reads. Pulled
into Rival because AI-thinking design (the per-action NL-phrasing sub-dictionary, `docs/ai/AI_OPPONENT.md`
§ 10) belongs here. Source of truth stays Io's copy; land changes back explicitly, don't let the
two silently diverge, re-pull if Io's copy moves upstream. `ACTIONS.md`/`ACTIONS_INDEX.json`
alongside it are generated snapshots, not kept in sync automatically.

**`docs/ai/PHRASINGS.json`**
The NL-phrasing sub-dictionary (2026-08-06) — per gameplay action, a rich set of *sentences* with
their paths back to the canonical press: seven stances (imperative / outcome / future-anchored /
corrective / deictic / composite / gated), arg binds, and `via` routing. Each reading doubles as a
training pair for the compressed local opponent (BL-279 corpus leg). Lands into Io explicitly, never
by silent merge.

**`docs/ai/VOICES.json`**
The voice dictionary (2026-08-06) — six voices, each mapped to an ideology (religious or
philosophical): creed, register, causal idiom, per-stance inflections, and an explicit
`decision_bias` weight table the deterministic scorer can apply. Seeding a voice seeds an ideology —
register and decision bias together. Campaign seats: imperial-providential (ours) vs
harmonic-preservationist (the rival's); four bench voices for BL-207 (persona packs). Real
traditions are cited as mechanism sources only, never as Io names.

**`docs/ai/UNITS.json`**
The unit roster dictionary (2026-08-07, military design session) — the noun half of the unit
dictionary: four era bands, 17 unit types, 7 doctrine presets over BL-272's `doctrine_row` shape,
availability derived from endowment + industrialisation (never research). Era-keying lives here
only; verbs (BL-314, design-owed) stay era-invariant. Lands into Io with BL-274 (era rosters),
never by silent merge.

**`docs/ai/SANDBOX-0CE.md`**
The 0 CE sandbox battery — era-voiced instructions (register drift, the eighth axis) tested against
PHRASINGS.json's resolutions. Battery A (buildings) runnable now over the MCP seam; Battery B (units)
design-owed, blocked on BL-271 (Era −1 sim) / BL-274 (era-keyed rosters).

## Response style

Match Project Io's register: terse, max 2 sentences per paragraph, short clauses.
Pair every backlog id with a human handle — `BL-271 (Era −1 sim)`, never bare `BL-271`.

Campaign narration is the exception: inside an annal, the tradition's voice rules
(see `docs/CAMPAIGN.md` station 6, and `annals/README.md`). Out of the annal, plain modern prose.

## Working method

**Proportionality first (Rule 0, shared house culture).** A one-file tweak gets made, checked,
and reported without ceremony. The full rite is for campaign years, not for every edit.

**Log decisions as they happen.** A call taken on Ben's behalf goes into the current annal's
accounting section (or Io's `docs/development/NEEDS_REVIEW.json` if it touches Io) at the moment
it is taken — never saved for a closing summary.

**The divine-framing rule.** In campaign narration, soldiers move because gods send them —
political intrigue is banned as a cause. This is honesty, not decoration: in the RTS and in Io's
creeds system alike, the true causes *are* terrain, supply, odds, and creed weights; the divine
dispatch is the mythically faithful narration of mechanism.

**The honesty ethic (inherited from Pantheon).** Legendary or contested history is flagged where
cited. Composition in historical languages follows the voices corpus's fidelity practice:
attested formulas preferred, inventions caveated, extinct tongues never faked.

**Play is the verifier.** A doctrine that loses campaigns is wrong, whatever the bench said.
Record the loss in the annal and amend the doctrine; the defeat is the credential.
