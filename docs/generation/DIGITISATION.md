# Project Io — Digitisation

> **Settles:** what the fourth simulated span is for and where its boundary with Exploration
> falls · which subjects belong to it and which were deliberately kept out.
> **Not here:** the phase before it and everything it hands forward (EXPLORATION) · the nodes of
> its technology tree (trees/INDUSTRY_TREE) · how a market clears and how a price resolves
> (../economy/MARKETS) · how a corporation is placed, focused and financed at the epoch
> (CORPORATION_GENERATION) · the pass map and the calendar (GENERATION_STRATEGY § Pass 2).
> **Confused with:** EXPLORATION.md, CORPORATION_GENERATION.md, ../economy/MARKETS.md.

**THIS IS A PLACEHOLDER (Ben, 2026-09-11).** *"Add a placeholder for Digitisation, and we will
work on the current Exploration round."* What this document settles is the **boundary** — which
subjects are Digitisation's rather than Exploration's — so that the phase before it can be
designed without leaking into it. The mechanism is not designed and no section below claims to
settle one.

**Digitisation is the fourth and last simulated span: 1660 → 1960 CE, 300 years.** It is the
second half of what used to be one economy pass, split on 2026-09-11
(`GENERATION_STRATEGY.md` § Pass 2). It ends at the epoch, so the world it produces is the world
the campaign opens on.

---

## The boundary

**Ben, 2026-09-11, drawing it in one line:** *"Digitisation is exactly the round which generates
companies as we know them in live play. Exploration gives us cultural preference to goods,
markets that trade named goods in a simplified form."*

| Belongs to Exploration | Belongs to Digitisation |
|---|---|
| Capital as a per-polity treasury | Capital as firm balance sheets |
| Named goods, traded in simplified form | The price field, the order book, the market carve |
| Trade flows between polities, unpriced, opened by treaty | Trade relationships a firm inherits and prices |
| Cultural preference for a good | Demand resolved from population and preference |
| Ports, navies, standing armies and their upkeep | Industrialisation and the furnace crossing |
| Treaties, subjects, trade provinces | **Corporations as live-play actors** |
| The Exploration tree | The **Industry** tree, gated behind Exploration's rim |
| — | Tariff posture by 1960; national political character hardened |

**The company is the reason the split exists.** A corporation is the object the campaign is played
with (`../CONCEPT.md`), and generating one needs a chartering institution, a treasury behind it,
and a market it can price against. Exploration produces all three and charters none of them;
this phase is where the actor appears.

**This phase inherits `EXPLORATION.md` § What this phase hands digitisation and nothing else.**
That list is the contract, in the same sense `CIVILISATION.md` § The closure of the Empire era is
the contract before it: a struct, not a promise.

**The tariff posture is derived here, from inputs that already cross.** Scarcity
(`region::scarcity_q`), trade flows (`trade_flows`) and cultural preference (`culture_preference`)
arrive in `exploration_output`; the derivation of `polity::protection_q` from them is owed to this
phase (BL-976, tariff derivation hands to Digitisation), and `derive_national_protection` →
`seed_national_tariffs` is the enactment seam that reads whatever this phase writes. The Era −1
sim derives the scalar only on the two-span arc, from industrialisation timing; a single-span world
carries no tariff, which is a legitimate outcome rather than a gap.

---

## Deliberately deferred

- **A world war late in the span.** Ben, 2026-09-11: *"I am on the fence about simulating a world
  war towards the latter end of Digitisation — but that is for a future session either way."* It
  is neither adopted nor declined, and nothing elsewhere should assume either.
- **Everything else.** The verbs, the firm-spawn rule, the tariff derivation, the industrial
  crossing's relationship to the Industry tree, and what the phase is judged on. Owed when the
  phase is designed.
- **The default epoch.** The campaign epoch is 1960 (Ben, 2026-09-08). The default world
  descriptor selects that arc **when this phase lands, as its done-when** (Ben, 2026-09-15,
  NR-869) — never before, because a 1960 world with no Digitisation opens on a 300-year gap the sim
  did not simulate.
