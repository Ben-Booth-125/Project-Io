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

The play model is the one Ben already chose for Io (NR-040, the computer-use steer): Claude
plays visually, with mouse and keyboard, no engine hooks. Rival is that steer given a home
and a discipline.

## Why Rome as the target

At year zero the world holds two great settled powers (Parthia a distant third). Han China is
the larger by the only hard number the era offers — the AD 2 census registers ~58 million
people, against model-based Roman estimates of 45–60 million — and by contiguous territory.
Rome is the second — and the only one **dedicated to expansion**: *imperium sine fine*, empire
without end, promised by Jupiter and prosecuted on three frontiers at once.

The Han posture is the opposite — self-preservation as cosmology: walls, garrison-farms,
tributary ritual, the Mandate held by harmony rather than conquest. A self-preserving rival
does not press; an expansionist one generates campaigns. We play the preserver; Rome supplies
the war.

The ideological war is the real engine, and it is documented, not invented: Rome's gods send
soldiers *outward* and are proven right only by expansion; Heaven *withholds* soldiers except
to restore harmony and is proven right only by stability. Each side's win condition is the
other's evidence of impiety. Full dossier: `docs/RIVAL-ROME.md`.

One honesty note, stated once and carried always: Rome and Han never fought — at year zero they
knew each other only as rumours along the silk routes. The campaign is a counterfactual
collision of mirror-antagonists, and the annals say so.

## What the campaign produces

**1. The yearly-prompt discipline.** A tested liturgy — six stations per campaign year, from
omen-reading to annal — that turns god-bench counsel into military strategy and back into
record. Spec: `docs/CAMPAIGN.md`. This is the prompt-series design the whole project exists to
refine.

**2. The refined oral history.** Annals in the campaign tradition's voice (classical Chinese,
per the voices corpus), phrase-bank candidates with fidelity flags, and doctrine records in
codebook v3 — grounded successors to doctrine-001 (align-with-the-weaker). The unseen-battle
idioms gain referents: battles that actually happened, in play.

**3. Calibration for Io.** BL-271 (Era −1 sim) names its own needs: campaign-season structure,
army supply radii, plausible hegemony-formation speed. A played antiquity campaign observes all
three — and since BL-272 (mil-sim) landed, observations can also test its constants and feed
doctrine rows toward BL-274 (era-keyed unit rosters). Io's rule that Rome is *calibration
reference, not content* stands untouched — Rival plays actual Rome only in the stand-in arena,
and hands Io numbers, not legions.

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
