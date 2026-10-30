# Diplomacy — design Q&A

> **Working design doc** for the ledger-mockup pass. Settled where marked; strawman elsewhere.
> Menu slot: **rail slot 9**, under the Diplomacy glyph (`../MENU.md`) · Source: none yet ·
> Mock table(s): none yet.
> Host: shell fold-out column, ~380px @1720 (derived — `shell_column_width(disp.x)`, 380–460 by resolution).

Asked for by Ben (2026-10-30): *"the basic diplomacy ledger and mechanics for declaring war. We want
to score nations sentiments to each other, and display them for diplomacy."* The model it serves is
`../../politics/RELATIONS.md` — the sentiment substrate, stance (§ 1), embargo (§ 3), standing (§ 4),
Alarm (§ 6) and war (§ 7).

## 1. Top question — the one thing this answers at first glance

**"Who could I fight, who might fight me, and what would it cost?"**

It earns a rail slot under `../MENU.md` § Menus are broad ledgers: every nation is a row, and every
war in the world is listed. Slot 9 held the corporation stance surface until the corporations retired
(Ben, 2026-10-23); this is its subject at the nation grain.

**What it is NOT.** It does not answer "where are my units": the canvas and the unit cards do. It
does not show a rival's treasury, which is private (`../../economy/FINANCE.md` § Disclosure). Nor
does it hold any law but the embargo press, which is a door onto the law verb (§ 4).

## 2. Sub-levels — views & default

Two views behind a `nav_button` strip, inside `foldout_begin("Diplomacy")`.

| View | Answers (one question) | Content |
|---|---|---|
| **Nations** *(default)* | "How does each nation stand with me, and what would a war with it cost?" | One row per nation, folded (`../DRILL_THROUGH.md`). Expanding a row in place shows its detail and presses. |
| **Wars** | "Who is fighting whom, and how is each war going?" | Every war in the world, the player's first, then truces with their time left. |

**Why Nations is the default.** The ledger exists to inform a choice about a nation; Wars is the
world's state.

### Nations — the folded row

- **Name and stance chips**: *you → them* (hostile, friend, offer pending) and *them → you*.
- **Their sentiment toward you**: Access, Trust and Alarm, each a signed bar with a trend arrow
  over the last four ticks. Hovering a bar names its largest contributing conduct; a seeded row names
  the war that seeded it, with its region and year (RELATIONS § Where a grudge lands).

Rows sort hostile first, then by Alarm toward you, highest first.

### Nations — the expanded row (extras ruled by Ben, 2026-10-30)

- **Your sentiment toward them** — the same three bars, the other direction.
- **Power comparison** — their reach and market share (public), their GDP (public), beside yours.
  Then **front strength**: your summed unit strength on the fronts you share against theirs, or "no
  border". Their numbers are what the canvas already shows; nothing private is read.
- **Trade exposure** — the share of your imports that come from them, and of your exports that go
  to them. This is what a war would cut, since hostile cargo is intercepted (RELATIONS § 1).
- **Friendship** — Offer, Withdraw, or Accept an offer they made, read from the offer table.
- **Embargo** — Embargo or Lift. The press enacts or repeals the embargo law against this nation
  (RELATIONS § 3: an embargo's author is a law). It is a door onto the law verb, not a second path.
- **Declare war** — opens the declaration (below). Disabled with its reason during a truce, or when
  already at war. While at war, the row shows the war's score instead and offers **Offer peace**.

### The declaration (RELATIONS § 7)

Pressing Declare war turns the expanded row into the declaration, in place:

- **Goals.** The canvas marks the target's provinces, every one a possible goal, its capital
  included (Ben, 2026-10-30). Clicking one toggles it, to at most three.
- **Justifications** — every opening justification, each marked as holding or not.
- **The price** — the Trust fall and Alarm rise with every other nation, summed and per neighbour,
  updated as goals change.
- **Declare** commits through `declare_war`; **Cancel** leaves everything as it was.

The right-click declare-and-attack opens this same declaration, with the attacked province chosen.

### Wars — the rows

Each war shows declarer and defender, each side's goals and who holds each, the war score as a
centred bar, each side's exhaustion, and days since it opened. Every war is public (Ben,
2026-10-30). On a war the player is party to:

- **Offer peace** — white peace, or cession of named goals the receiving side holds. **Impose**
  replaces Offer while the score is 100 in the player's favour.
- **Answer** — on a war declared against the player that it has not yet answered: name goals among
  the declarer's provinces. Answering is free.
- A pending offer from the other side shows its terms with **Accept** and **Refuse**.

## 3. Lens on open

**None by default.** During the declaration only, the canvas marks the provinces that may be goals,
and the ones chosen. That is a mode of the declaration flow, as the placement lens is of the build
flow, not an `overlay_mode`.

## 4. Data sources

| Read | Source |
|---|---|
| Stance chips | `is_hostile` (each direction), `are_friends`, the offer table (`src/world/stance.hpp`) |
| Sentiment bars | `world::sentiment`, both directions; the largest contributor per row |
| Alarm | the Alarm dimension (RELATIONS § 6) |
| Power | `standing` reach and market share; GDP (`../../economy/FINANCE.md` § Disclosure) |
| Front strength | the shared fronts (`../../military/MILITARY.md` § Fronts) and `unit_strength` of the units standing on them |
| Trade exposure | cargo landed between the pair over the last four ticks, as shares of the player's imports and exports |
| Embargo | `corp_embargo_conditions`; the press is the law verb |
| Wars, offers, truces | the war record (RELATIONS § 7) |
| Presses | `declare_war`, the answer, the peace verbs, `offer_friendship` / `accept_friendship`, the embargo law — each an `ACTIONS.json` entry |

## 5. Toggle / close semantics

Standard: the rail icon toggles the ledger; re-clicking the active tab closes it; switching tabs is
a view change. A row's expander is a toggle. **Declare, Offer peace, Accept and Refuse are presses**:
they have no active state. **Embargo / Lift is a toggle** — its state is visible, and pressing it
while active lifts it. Cancel or Escape closes the declaration without effect.

## Settled (Ben, 2026-10-30)

- **War rules (NR-1101)**: any province may be a goal, capitals included; the score is half
  occupation and half battles; a side at 100 imposes; a truce lasts sixteen ticks; the defender's
  answer is free and names goals; only peace ends a war; a justification halves the price.
- **Rivals make peace too (NR-1100)**: they answer, offer, accept and impose, and so may cede ground.
- **The war is whole**: declaration with a price and justification, goals, score, exhaustion, white
  peace and cession (RELATIONS § 7).
- **Alarm is shown**, as the third sentiment bar.
- **Every declaration and war is public**, between rivals too.
- **Extras in the ledger**: power comparison with front strength, trade exposure, friendship offers in
  and out, the embargo press, and the active wars list. A per-factor "why this sentiment" panel was
  not chosen. Only the hover line remains, since an inherited grudge must name its cause.

## Open questions for Ben

1. The trend window (four ticks) and the trade-exposure window (four ticks) are strawman.
2. Should a pending peace offer *to* the player also raise a dispatch in the chat, as a declaration
   does?
