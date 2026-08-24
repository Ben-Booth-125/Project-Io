# Project Io — Roadmap

This document owns the **milestone sequence**: which theme comes next, and what *finished* means
for it. It does **not** list items — the backlog does — and it does **not** record what was built
or when — [`DEVLOG.md`](DEVLOG.md) and [`SPRINTS.md`](SPRINTS.md) do.

*Trimmed from 1,336 lines on 2026-08-24, on Ben's call that it had become a devlog with a version
number attached. The old text — including the done-definition written at every past cut and the
per-minor findings — is kept verbatim at
[`archive/ROADMAP-2026-08-24.md`](archive/ROADMAP-2026-08-24.md).*

> **Open question, raised by Ben 2026-08-24 (NR-589): do we still want versioned releases?**
> Item-level versioning is already gone — the 2026-08-23 backlog purge left the hot file holding
> one sprint's items and no `version` field at all. The tags are real and the themes below are
> live; it is the *numbering* that is in doubt. Until it is ruled on, read the band as a set of
> named themes and let SPRINTS.md say what is being cut.

---

## The two arcs (2026-08-12, NR-177)

**Ancient — live.** A standalone commercial product set at 0 CE, with the player as a **mercenary
company**, built on the campaign engine as it stands. Four rulings shape it: the space work is
**stashed, not re-anchored**; the mercenary company is the militia one era earlier (procure force,
field it, be paid), so the 2026-08-10 identity is being *tested*, not replaced; the grain is **tile
and fine tick**, with `history_sim` staying the *generator*; and the release bar is **commercial** —
Steam or itch, paid, polish and content depth required.

Why: *"our project is too grand to be able to really feel out the gameplay"* — and, on the method,
*"when making your ideal game, you should first focus on a smaller project with similar design."*
The ancient product is deliberate practice for the space game, not an abandonment of it.

**Space — parked for DLC.** **v0.2.0** (the AI opponent), **v0.3.0** (the national private
militia), **v0.4.0** (politics + the Era → Filter rename) and **v1.0.0** (the playable space game)
are parked whole, their designs intact, so the DLC resume is a re-triage rather than an archaeology
dig. Do not build against them; do not delete them. Their combined bar, in one line: a player
contesting **both Trade and Conflict** against AI rivals doing the same, where **procurement of
force from private suppliers** is the mechanism coupling the two pillars. The full parked design is
in the archived roadmap and in `archive/backlog-purged-2026-08-23.json`.

The generation layer is shared by both arcs and is never parked.

---

## The grain

One minor carries **one coherent theme**, taken end to end and cut as a release (see
[`DEVELOPMENT_PRACTICES.md`](DEVELOPMENT_PRACTICES.md) § Cutting a release). A minor is not a fixed
quantum of work; it is a theme that reads as one thing. It may prove too large and **split at
promotion** — that is expected, not a roadmap failure.

**The numbers are naming order, not build order.** Minors were numbered as they were named and cut
in whatever order was ready; pre-1.0 numbering is advisory, and each tag documents its own theme.

**Every cut carries a done-definition, written at the cut** (NR-103). A theme with no test for
*finished* absorbs items indefinitely — that is exactly what happened to v0.1.1, and it is the one
piece of ceremony this document keeps. It is deliberately **not** written ahead of the work:
authoring a test for finished before knowing what finishing looks like is invention, not planning.

---

## The live band — themes not yet cut

Ordered by subject, not by number or by date. One line each; the design behind each lives in the
authority doc for its subject, and the work in whichever sprint opens it.

| Theme | What it means |
|---|---|
| **The watch** | An agent plays a rendered session and a human watches — the live agent control seam, the spectator god view, the emergent-strategy readout, the attention director. |
| **Economy breadth** | The growth spine is **chain depth**: how far down the production graph a corp reaches gates its next building. Past 20 building types, with alternate production methods that trade off rather than upgrade. |
| **The economy tells the truth** | The measured pathologies, fixed — processing underearns extraction, the economy mines one resource, co-extraction is invisible, intra-catchment distance is free. |
| **Ancient conflict & seams** | The Era −1 machinery graduates from generator to gameplay: era-keyed rosters, the strategic layer, the diplomacy seam and its battery, myth and theology. |
| **Stance & force** | Who may fight whom, and the verbs to do it — the unit verb family, company answerability, the stance surface, Logistic Points, formations. |
| **The credible rival** | The scorer behaves like a competitor and every verb is reachable: build scoring, procurement, demolition and roading, the processor and mine-siting blind spots. |
| **Harness truth** | The instruments measure the sim that ships — lazy-sim cost, the stale-exe gate, the world-snapshot cache, seed-invariant settlement counts. |
| **Policy, tech meta, the nation as actor** | The player is *subject to* a law and someone else enacts it. The tech effect union and shared modifier vocabulary; a law author over a real nation treasury, so a levy is a conserved transfer. |
| **Who owns whom** | Ownership separates from identity: a **syndicate** tier holds equity and owns no buildings, control comes with a majority and **costs attention**. Designed, and deliberately not started until the mercenary slice has proved the current identity. |
| **Who answers to whom** | The two-way channel between corporations and nations — a weighted national budget, lobbying as the player's first lever on law, sentiment as one derived relational quantity under declared stance. |
| **The channel becomes visible** | Those subsystems ship with no surface at all, and sentiment cannot work without one: its invariant is that it *informs a declaration and may never make one*, so the player is the only thing it can act through. |
| **Generation visibility + UI alignment** | Every generation step earns a surface it can be watched through, and the UI is walked against everything the band added. Shared by both arcs. |
| **Logistics modes** | Distance costs something in more than one way — haul-weighted payout, rail as its own mode, coastal ports and sea trade — and the Supply lens reads as *flow* rather than a uniform arrow field. |
| **Markets & materials** | The market stops being fixed at world-gen, the goods it trades deepen, and the save format learns to say no (magic + version header). |
| **Politics stub** | The one axis that never dissolved into another item: inter-nation relationships, built **with a consumer** — a stub nothing reads is indistinguishable from no stub. |
| **Military tail** | Out-of-supply unit decay beyond the logistics reach field — designed, and explicitly not required by the military cut. |

---

## Cut so far

Tags `v0.0.4` → `v0.1.15`. The done-definition written at each of those cuts, and the findings each
one recorded, are in [`archive/ROADMAP-2026-08-24.md`](archive/ROADMAP-2026-08-24.md); the per-item
record is [`DEVLOG.md`](DEVLOG.md) via [`DEVLOG_INDEX.md`](DEVLOG_INDEX.md), and the
sprint-by-sprint account with retros is [`SPRINTS.md`](SPRINTS.md).

`v0.1.0` remains the **prototype cut** — the economy loop, validated and playable end to end. Its
scope and exclusions (Conflict, Research, Policy, and Diplomacy beyond a data-model stub) are owned
by [`../tech/TECH_FOUNDATIONS.md`](../tech/TECH_FOUNDATIONS.md), not by this document.

---

## Owed

- **The ancient product's own done-definition** — the commercial cut's bar, distinct from
  v1.0.0's, which is the *space* game's and stays parked with it (NR-177).
- **The product's name.** Io is a moon of Jupiter.
- **The mercenary sell side** beyond the contract seam: win and lose consequences, and reputation.

---

## Sequencing

Live sequencing does not belong here. **Which sprint cuts which theme** is
[`SPRINTS.md`](SPRINTS.md); the active worklist is [`REFINED.md`](REFINED.md); the method — the
delivery lifecycle, batch delivery, worktrees — is [`DELIVERY.md`](DELIVERY.md); the constraints
and tone that govern *how* the work is done are
[`DEVELOPMENT_PRACTICES.md`](DEVELOPMENT_PRACTICES.md).

The one rule this document exists to hold: a theme named here and a sprint opened there must not
drift apart silently. That drift is NR-102, and this file has recorded it twice.
