# Acquisitions — design Q&A

> **Working design doc** for the ledger pass. Strawman answers — Ben revises.
> Menu slot: `rail slot 5 "Acquisitions"` (inserted above Market, Ben 2026-08-29) · Second door: the **Company lens** click destination (`../LENSES.md` § Company lens) · Source: `src/ui/acquisitions_ledger.{hpp,cpp}` · Mock table(s): `corporations.csv`
> Host: shell fold-out column, ~380 px @1720 (derived — `shell_column_width(disp.x)`, 380–460 by resolution), plus a full-canvas fold-out (`ui::canvas_rect`).
> Authority for the rules it renders: `../../economy/FINANCE.md` § Disclosure and § Whole-firm acquisition.

## 0. The measurement this surface was designed around

**The field was measured before a pixel of it was laid out, and the number shaped it.**
`tools/verify/acquisition_viability.cpp` § C, twelve seeds of the shipped spawn:

| | per seed, before BL-678 | per seed, now |
|---|---|---|
| corporations | 88 | 88 |
| `publicly_held` (the player's own excluded) | **1–3, mean 1.6** | **81–83, mean 81.6** |
| public **and** filed — *the buyable field* | **1–3, mean 1.6** | **81–83, mean 81.6** |
| public but never filed | **0 on every seed** | **0 on every seed** |
| `closed` | mean 84.8 | mean 4.8 |
| Purchasable / Possible | mean **1.0** / **0.6** | mean **35.8** / **45.8** |
| seeds where one group is empty | **8 of 12** | **0 of 12** |
| seeds whose cheapest firm is priced at exactly 0 | 5 of 12 | 12 of 12 (mean 10.6 such firms) |
| non-public firms that nevertheless *file* in world state | mean 85.4 | mean 5.4 |

**The right-hand column is BL-678 (companies are open), 2026-08-29**, and it inverts the premise
this section was written on. Ben retired closure for **companies**; corporations keep the derivation
they had. So the two columns are the same twelve seeds of the same spawn, read either side of one
rule change. **The design prose below was authored against the left-hand column and is owed a
re-read** — the surface is no longer explaining a short list, it is ranking a long one.

Three consequences fell out of the original measurement. The first survives, the second is
overturned, the third has changed shape:

1. **Filing is never the binding gate — ownership class is.** *Still true, and now nearly moot.*
   Gate (4) of `buy_corporation` ("must have filed") excludes no firm on any seed, either side of
   the change. What used to exclude 84.8 firms per seed was that they were `closed`; that number is
   now 4.8, and every one of them is a **corporation**. A surface explaining "why can't I buy this"
   is now explaining a rare case rather than the normal one.
2. ~~**The column is a one-to-three-row surface.**~~ **Overturned.** It is a thirty-six-row
   Purchasable group over a forty-six-row Possible one. The count on its face still earns its place,
   but it is now telling the player *how much of the field is in reach today*, not reassuring them
   that a short list is the world.
3. **The substance is in the fold-out, not the groups.** *Changed shape, same conclusion.* The
   85.4-firms-file-but-do-not-disclose inversion is gone — the number is 5.4 — but the fold-out
   still earns its takeover, now because the buyable field is too large to read whole and wants
   ranking rather than because it was too small to fill a column.

## 1. Top question — the one thing this answers at first glance

**"Which firms can I buy outright, and what do they cost?"**

Ownership moves **whole, by buyout** — there is no fractional stake, no share count, no
controlling-holder threshold anywhere in the model (`FINANCE.md` § Whole-firm acquisition,
Ben 2026-08-26). So the answer is a list of firms and a price each, and the only thing that
varies row to row is whether the player can afford it today.

The secondary question, and the reason for the fold-out: **"how is the whole field doing?"**

## 2. Sub-levels — views & default

**No `nav_button` tabs.** The column is one flat surface: a field-size line, then two
collapsing groups, then the fold-out's door.

| Section | Answers (one question) | Content |
|---|---|---|
| **Field line** | How much is there to look at? | The player's balance, and "N of M firms file and can be priced", with the class rule on hover |
| **Purchasable** *(group, open at rest)* | What can I buy today? | Firm name, price, and a **Buy** press. A price of exactly 0 carries a hover warning — the free-firm trap |
| **Possible** *(group, open at rest)* | What is the next rung, and what does it cost? | Firm name and price, and **deliberately no press** |
| **Profitability** *(fold-out door)* | How is the whole field doing? | `›` opens the full-canvas takeover below |

**Both groups are open at rest**, against the usual instinct to collapse: with a mean of 1.6
rows in the entire field, a collapsed group hides the whole answer.

**The Possible group's whole purpose is the contrast.** A row there shows a price with no
button, because the price is what the player is *saving toward*. A greyed-out button would
say "refused" where the truth is "not yet".

**A firm that does not file does not appear at all** — not greyed, not shown at zero, absent.
It cannot be priced, and `apply_corp_command` refuses it.

**The fold-out — the profitability table (BL-627, profitability ledger).** A full-canvas
takeover (`detail_surface::acquisitions_profit`, the BL-214/BL-265 idiom the Corporation
dashboard's roll-up cards use), one row per corporation, **sortable on every column**:

| Column | Disclosure-gated? |
|---|---|
| Firm | no — a name is public |
| Type (the holdings mix: Extraction / Processing / Mixed) | **yes** |
| End resource (the recipe's output) | **yes** |
| Input resource | **yes** |
| Profit/qtr (the filed `net`) | **yes** |
| Price | **yes**, and additionally absent on the player's own row — it is not for sale |
| Ownership | no — it is the thing that explains every dash |

Exact figures where the firm files, a **dash** where it does not, **no bands anywhere**
(`FINANCE.md` § Disclosure; Ben 2026-08-26 retired the banded standing read: *"We don't need
company information to be invisible"*). A dash always means *this firm does not file* and
never *you have not earned this*, and the hover says so in those words.

Sorting treats an undisclosed cell as **absent, not as zero** — a dash is not a small number,
and letting the dashes sort among the real values would make the ordering lie. Sorting by
Ownership is what makes an 85-dash table navigable: it brings the readable firms to the top.

**Default view on open:** the column, groups expanded, fold-out closed (the ledgers-start-closed
policy applies to the takeover too).
**Cross-cutting selectors (NOT views, exempt from the toggle rule):** **none.**

## 3. Lens on open

**None — but the pairing runs the other way, and that is the interesting part.**

Opening this ledger does not arm a lens. The **Company lens arms this ledger**: a click on a
background firm's holdings resolves through to the firm (the same resolve-through rule the
Corporation lens already uses) and lands here, with that firm's row highlighted.

The split between the two lenses is the reason (Ben 2026-08-28): the **Corporation** lens
draws rivals and its click lands on their *books* (the Balance ledger); the **Company** lens
draws background firms, and the question a player has about a background firm is whether they
can *buy* it. Two lenses, two questions, two destinations.

The highlight is **never a filter**. The field is one to three rows; filtering it to one would
answer a question nobody asked. And a clicked firm may well not be in the field at all — most
background firms are `closed` — in which case the highlight matches nothing and the field line
explains why. That is the honest outcome; inventing a row for an unpriceable firm would be worse.

Forcing the **corporation** lens on open was considered and rejected: it paints rivals, which
is the population this ledger is *not* about.

## 4. Data sources

- The buyable field and every price → `corp_acquisition_price(cc, reg.acquisition().multiple)`,
  never a restated formula. A surface that restated it would show a price the seam then refused.
- The gates → mirrored from `apply_corp_command`'s own, in its order: not the acquirer, not the
  player, `ownership_class == publicly_held`, `!returns.empty()`, then the solvency test. The
  seam re-checks every one, so a divergence surfaces as a rejected press rather than a wrong number.
- **`is_background` is deliberately NOT a gate.** The verb does not test it, so neither does the
  ledger: the buyable field is *every* public filed firm, which in practice is mostly background
  companies. Adding such a gate would make the ledger disagree with the verb behind it.
- Profit → `corporation_component::returns.back().net`, the filed quarterly return (BL-626).
- Type / end resource / input resource → derived at draw time by walking `cc.assets` →
  `building_component` → `recipe_registry::get_recipe`. There is no stored "what this firm makes"
  field, and inventing one would be a second source of truth that could disagree with the buildings.
- The press → `corp_verb::buy_corporation` enqueued onto `ui_state::pending_order_commands`, the
  same deferred-dispatch seam every other ledger press uses. Never a direct write.

## 5. Close / toggle semantics

A **standard rail-slot ledger**: the slot-5 icon toggles it open/closed, and opening it closes
whichever other ledger held the column (accordion, `close_all_panels`). There are no sub-view
tabs, so the re-click-the-active-tab rule has nothing to bind to here.

The two **collapsing group headers are toggles** by construction — clicking an open group
closes it — which is the standing rule for any control whose active state is visible.

The **fold-out is the takeover rung**: `›` opens it, the `‹` return control top-left closes it,
and Esc closes it (`ui::fold`). It **coexists** with the column by design (BL-265) — the
Profitability row that opened it stays visible beside it — so the player never loses the
buyable field to read the wider table.

## Open questions for Ben

1. **Gating Type, End resource and Input resource was a call taken here, and it is the one most
   worth overturning.** BL-633's note on the Corporation table is explicit that "no production
   rate, stockpile quantity, **recipe** or workforce dial is readable here", and a firm's end and
   input resources *are* its recipe read out — so widening the operational fog through a financial
   surface was not this surface's to do. But it makes ~85 of 88 rows carry a name, a class and five
   dashes. If the intent is that *disclosure* covers only the money and the operational fog is a
   separate question, these three columns should print for everyone and the table gets much richer.
2. **`Type` reads "Mixed" on all 88 rows** on the seeds measured — every generated firm appears to
   hold both an extraction site and a processor. If that is the generator working as intended, the
   column is dead weight and should probably go; if it is not, this is the surface that found it.
3. **The free-firm trap is visible here and is not fixed here.** Five of twelve seeds price their
   one buyable firm at exactly 0 because its debts cancel its book value, and the solvency gate
   tests the *price* rather than the buyer's position after the transfer (`FINANCE.md` § Whole-firm
   acquisition names this, design BL-658). The ledger warns on hover; whether a zero-priced firm
   should be listed as *Purchasable* at all, or in a third state, is a design question this surface
   raises rather than settles.
4. **Should the fold-out absorb the Corporations table** (rail slot 8, Diplomacy's provisional
   occupant)? Both are one-row-per-corporation comparisons over financial figures, and BL-627's own
   design named this as its open question. They differ today in axis — Reach/Capital/Share and a
   stance press there, operations and price here — but that may be one table.
