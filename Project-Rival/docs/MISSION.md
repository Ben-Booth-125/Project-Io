# Mission — a test environment at year zero (2026-08-03)

## The aim in one line

Refine the oral history by play: take the Pantheon corpus to a real battlefield, let the gods
direct a campaign year by year, and keep what survives contact.

## Why 0 A.D., and why now

Project Io cannot host this yet. A 0 AD start is blocked for the game proper — its tech, laws,
and materials only work 1900s-onward — and the sanctioned antiquity sandbox, BL-271 (Era −1
sim), is designed but unbuilt, queued behind the v0.1.0 prototype.

The method cannot wait for the arena. 0 A.D. (the RTS) supplies antiquity today: Romans and
Han Chinese both playable, deterministic seeded matches, headless autostart, and plain-text
replay records — a test environment we can *get*, at the exact year the name promises.
(Its sibling BL-272, the mil-sim, already landed 2026-08-02 — `resolve_battle` exists; what
remains unbuilt is the antiquity *world* to run it in.)

The play model is computer-use: Claude plays visually, with mouse and keyboard, no engine hooks.

**Amended 2026-08-03.** This was originally framed as inheriting Io's own play model (NR-040's
computer-use steer). Io has since settled on an **MCP server** as its agent interface
(`docs/ai/AI_OPPONENT.md` § 10), so the two now differ by arena rather than by principle:
0 A.D. offers no agent interface, so Rival drives a screen; Io offers a protocol seam, so its
own AI drives that. What transfers is the *discipline* — legal presses only, state handed to
the agent rather than remembered, decisions logged as they are taken — not the input device.

## Why Han as the target

*(Flipped 2026-08-03 by Ben, NR-042. The campaign was seeded the other way round — playing Han
against Rome. The reasoning below is the same material read from the other end.)*

At year zero the world holds two great settled powers (Parthia a distant third). Han China is
the larger by the only hard number the era offers — the AD 2 census registers ~58 million
people, against model-based Roman estimates of 45–60 million — and by contiguous territory.
Rome is the second — and the only one **dedicated to expansion**: *imperium sine fine*, empire
without end, promised by Jupiter and prosecuted on three frontiers at once.

**We play the expansionist.** Rome's theology is the one that generates campaigns rather than
absorbing them: no legitimate war without auspices, every frontier a proof of divine favour,
and *evocatio* — the doctrine that a siege is won by inviting the enemy's god to defect. That
last is the single best fit for a project about gods sending soldiers, and it is *ours* now
rather than the rival's.

The Han posture is the opposite — self-preservation as cosmology: walls, garrison-farms,
tributary ritual, the Mandate held by harmony rather than conquest. As a **target** that is
harder and more interesting than it sounds: a preserving power does not overextend, does not
take bait, and wins by simply still being there. Rome has to prove its gods right; Han only has
to not be disproved.

The ideological war is the real engine, and it is documented, not invented: Rome's gods send
soldiers *outward* and are proven right only by expansion; Heaven *withholds* soldiers except
to restore harmony and is proven right only by stability. Each side's win condition is the
other's evidence of impiety. Full dossier: `docs/RIVAL-HAN.md`.

One honesty note, stated once and carried always: Rome and Han never fought — at year zero they
knew each other only as rumours along the silk routes. The campaign is a counterfactual
collision of mirror-antagonists, and the annals say so.

## What the campaign produces

**1. The yearly-prompt discipline.** A tested liturgy — six stations per campaign year, from
omen-reading to annal — that turns god-bench counsel into military strategy and back into
record. Spec: `docs/CAMPAIGN.md`. This is the prompt-series design the whole project exists to
refine.

**2. The refined oral history.** Annals in the campaign tradition's voice — **Latin** since the
2026-08-03 flip, where it was classical Chinese — phrase-bank candidates with fidelity flags,
and doctrine records in codebook v3, grounded successors to doctrine-001
(align-with-the-weaker). The unseen-battle idioms gain referents: battles that actually
happened, in play.

> **Owed before Year 1:** confirm Pantheon's voices corpus has a Latin register to work from
> (`data/voices/`), as it did for classical Chinese. If it does not, that is a Pantheon
> refinement to propose rather than a licence to fake one — the honesty ethic below applies to
> our own voice as much as to the rival's.

**3. Calibration for Io.** BL-271 (Era −1 sim) names its own needs: campaign-season structure,
army supply radii, plausible hegemony-formation speed. A played antiquity campaign observes all
three — and since BL-272 (mil-sim) landed, observations can also test its constants and feed
doctrine rows toward BL-274 (era-keyed unit rosters). Io's rule that Rome is *calibration
reference, not content* stands untouched — and matters more now that Rome is the played civ
rather than the rival: Rival plays actual Rome only in the stand-in arena, and hands Io numbers,
not legions.

## The bridge to Io

| Rival artifact | Io destination |
|---|---|
| Yearly-prompt liturgy | Persona-counsel texture (BL-207, persona packs) and the nation-advisor model |
| Annals + phrase banks | BL-210 (oral-history pivot) generation material, via Pantheon |
| Doctrine records | Candidate priors for persona-pack policy weights |
| Campaign observations | BL-271 (Era −1 sim) calibration; constants checks on the landed BL-272 (mil-sim); doctrine rows toward BL-274 (era-keyed unit rosters) |
| The play harness itself | Io's word-interface loop, with computer-use standing in for the corp-command seam as the write leg (the NR-040 play mode) — rehearsed against a foreign game first |

When BL-271 (Era −1 sim) lands, the arena moves home: same liturgy, same annal format, Kepler's
own generated antiquity instead of Earth's. The RTS is scaffolding, and scaffolding comes down.
