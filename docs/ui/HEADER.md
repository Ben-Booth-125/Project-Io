# Project Io — Header

The **header** is a full-width strip across the top of the canvas area, sitting between the profile (top-left) and the time column (top-right). It top-aligns at `y=0` and stands at the profile card's full height (`profile_panel_height`), so it reads level with the identity tile as a single top band; its content row is vertically centred within that band. It is the player's persistent financial and material dashboard — always visible, glanceable, never opened or closed. See `LAYOUT.md` for where it sits in the shell.

It reads the live economy (`src/ui/header_panel.{hpp,cpp}`) and is a **summary surface**: three money figures plus a trend line, with all detail left to the ledgers, and two survival signals — runway and the debt flag — on the same row.

---

## Purpose

Answer, at a glance and without opening a ledger: **how much money do I have, what is my material worth, and which way is it trending this quarter?**

## Contents

The header reads left to right as three figures:

- **Balance** — the player corporation's running treasury balance (`corporation_component.balance` for `world::player_entity`). Negatives are flagged red (`palette::negative`), matching the economy-panel convention.
- **Stockpile valuation** — an estimated liquid value of everything the player holds: each player `(corporation, body)` pool's quantities priced at that body's **current market price** and summed. A single currency figure, deliberately *not* a per-resource inventory — the per-body, per-resource breakdown stays in the Tile and Market ledgers. Resources on a body with no market contribute nothing.
- **Net + trend** — the **last economy tick's net change** as a coloured signed per-quarter figure (`fmt::rate(net, "qtr")`, coloured by `palette::value_colour`), followed by a small **sparkline** of recent balances (`ImGui::PlotLines` over a capped balance history maintained in `app`, pushed each `step_economy()`).
- **Runway** (BL-177, budget runway legibility) — how long the balance lasts at the current burn: `RUNWAY  12q` (balance ÷ quarterly loss), or `in debt` when already underwater. **Shown only while it means something** — burning cash or in debt; a growing balance shows nothing rather than a lying "infinite quarters" figure. **Urgency-coloured**: red under 8 quarters of cushion (about two years — the point it needs acting on) and when in debt, plain text otherwise. Tooltip explains the arithmetic and its steady-burn assumption. Load-bearing, not decorative — it degrades through the fit ladder below but the fact is never simply lost.
- **`[in debt]` flag** (BL-073, debt interest) — a negative balance is self-accelerating (interest accrues each quarter), so it is flagged in words beside the red number: `[in debt - interest accruing]` in full, compacting to `[in debt]` with the full text as tooltip when width is tight.

## Fit and degradation

The header is a **guaranteed-fit strip** (LAYOUT.md container vocabulary): it never wraps to a
second line and never silently clips a load-bearing number. `draw_header_panel`
(`src/ui/header_panel.cpp`) measures everything against the available width and degrades in a
fixed order:

1. **Runway ladder** — resolved first, in three honest steps:
   - full — `   |   RUNWAY  12q` (label + value);
   - value-only — `   |   12q` (label dropped, tooltip explains);
   - none — **folded into the NET figure's tooltip**, so the fact survives even when the strip
     cannot draw it.
2. **Debt flag** — the worded `[in debt - interest accruing]` drops next (the red number already
   carries the signal), compacting to `[in debt]` + tooltip.
3. **Sparkline** — the last decorative element to drop.

The three money figures (Balance / Stockpile / Net) are bounded-length (`fmt::credits` /
`fmt::rate` abbreviate) and always drawn in full.

This ladder is the **reference case for BL-215** (text-wrap render audit): the audit's job is to
make every other worded surface degrade this honestly rather than wrap or clip.

## Settled design questions

The header design pass (2026-06-15) settled:

- **Resource overview** — *not* a scarce per-resource icon strip; the single **stockpile valuation** figure instead. A per-resource header strip was judged too lossy at a glance and redundant with the ledgers.
- **Per-tick delta** — yes, the header shows the **last-tick net** (not balance alone), plus a sparkline for direction of travel.
- **Change animation** — none; the sparkline carries the trend. Flash/tick animation is a possible later polish, not specified here.

## Related

- `LAYOUT.md` — placement in the shell, and the **uniform ledger-window chrome** principle (the header is exempt as persistent chrome).
- `PROFILE.md` — the adjacent top-left identity panel.
- Economy/market systems (`SYSTEMS.md`) — source of the numbers shown here; price resolution feeds the valuation figure.
