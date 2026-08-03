# Project Rival — Claude Reference

Project Rival is the AI-player discipline for Project Io, run as **live play, not shipped code**.
A session (this kind of session) plays a strategy game through interactive prompts and
computer-use tooling — screen, mouse, keyboard.

**Scope of that constraint (amended 2026-08-03).** Computer-use is how Rival plays **0 A.D.**,
because 0 A.D. exposes no agent interface and patching its engine is out of scope. It is *not* a
house position that protocol interfaces are forbidden: Io's own direction is now an **MCP server**
over its existing read/meaning/write legs (`docs/ai/AI_OPPONENT.md` § 10). So the destination is
a protocol seam even though the near-term arena is a screen. Rival still never patches or hooks
the game it is playing.

The near-term arena is **0 A.D.** (Wildfire Games' RTS, Release 28), a match set at year zero:
we play **Han China** on a civilising mission; the target is **Rome**, the era's second great
power and the only *expansionist* one. The destination is Project Io itself — its word interface
today, its antiquity sandbox (BL-271, the Era −1 sim) when that lands.

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
The 0 A.D. test environment: install state, match template (Han vs Rome), the year cadence, headless and visual modes, replays as records.

**`docs/CAMPAIGN.md`**
The yearly rite — the prompt series. Six stations per campaign year, the divine-framing rule, the prompt templates, and the oral-history refinement contract.

**`docs/RIVAL-ROME.md`**
The rival dossier: Rome's theology of expansion, Han's cosmology of preservation, the ideological war between them, and the other live creeds of the year-zero world. Contested history is flagged as such.

**`annals/README.md`**
The annal format — one record per campaign year, chronicle plus data. The campaign's durable output lives here.

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
