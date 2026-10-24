# Project Io — Header

> **Settles:** which question the top strip answers without opening anything ·
> which money and material figures it carries and how a trend is shown · which
> survival signals share that row and when each is worth showing · what the strip
> gives up first as width runs out.
> **Not here:** the detail behind any figure (ledgers/*) · the identity tile
> (PROFILE) · the clock (TIME_CONTROLS) · placement (LAYOUT).
> **Confused with:** PROFILE.md, TIME_CONTROLS.md, LAYOUT.md.

The **header** is a full-width strip across the top of the canvas area, sitting between the profile (top-left) and the time column (top-right). It top-aligns at `y=0` and stands at the profile card's full height (`profile_panel_height`), so it reads level with the identity tile as a single top band; its content row is vertically centred within that band. It is the player's persistent financial and material dashboard — always visible, glanceable, never opened or closed. See `LAYOUT.md` for where it sits in the shell.

It reads the live economy (`src/ui/header_panel.{hpp,cpp}`) and is a **summary surface**: three money figures plus a trend line, with all detail left to the ledgers, and one survival signal — runway — on the same row.

---

## Purpose

Answer, at a glance and without opening a ledger: **how much money do I have, what is my material worth, and which way is it trending this quarter?**

## Contents

The header reads left to right as three figures:

- **Treasury** — the player nation's treasury (`nation_component::treasury` for the seat's nation; re-subjected from the retired corporation balance, Ben, 2026-10-23). A nation never runs into debt (`../politics/NATIONS.md` § What a nation owns), so the figure has no negative state to flag.
- **Cargo valuation** (labelled **CARGO**) — an estimated value of the goods the player **owns**: its trades' cargo in transit, each in-flight shipment priced at its **destination** market's current price — the price it lands into. A nation holds no stockpiles (`../economy/MARKETS.md` § The shelf economy): what its buildings make is sold where it lands, so its trades' cargo is the only stock it owns between purchase and sale (`../economy/TRADE.md` § A trade). A single currency figure, deliberately *not* a per-resource inventory; it reads zero with nothing in flight.
- **Net + trend** — the **last economy tick's net change** as a coloured signed per-quarter figure (`fmt::rate(net, "qtr")`, coloured by `palette::value_colour`), followed by a small **sparkline** of recent treasury values (`ImGui::PlotLines` over a capped history maintained in `app`, pushed each `step_economy()`).
- **Runway** (BL-177, budget runway legibility) — how long the treasury lasts at the current burn: `RUNWAY  12q` (treasury ÷ quarterly loss). **Shown only while it means something** — burning cash; a growing treasury shows nothing rather than a lying "infinite quarters" figure. **Urgency-coloured**: red under 8 quarters of cushion (about two years — the point it needs acting on), plain text otherwise. Tooltip explains the arithmetic and its steady-burn assumption. Load-bearing, not decorative — it degrades through the fit ladder below but the fact is never simply lost. When the treasury runs out, a losing building idles rather than the nation borrowing (`../politics/NATIONS.md` § What a nation owns).
- **The `[in debt]` flag retires** (Ben, 2026-10-23): it flagged a negative, interest-accruing balance, and a nation never runs into debt.

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
2. **Sparkline** — the last decorative element to drop.
The three money figures (Treasury / Cargo / Net) are bounded-length (`fmt::credits` /
`fmt::rate` abbreviate) and always drawn in full.

This ladder is the **reference case for BL-215** (text-wrap render audit): the audit's job is to
make every other worded surface degrade this honestly rather than wrap or clip.

## Settled design questions

The header design pass (2026-06-15) settled:

- **Resource overview** — *not* a scarce per-resource icon strip; the single **cargo valuation** figure instead. A per-resource header strip was judged too lossy at a glance and redundant with the ledgers.
- **Per-tick delta** — yes, the header shows the **last-tick net** (not the treasury alone), plus a sparkline for direction of travel.
- **Change animation** — none; the sparkline carries the trend. Flash/tick animation is a possible later polish, not specified here.

## Related

- `LAYOUT.md` — placement in the shell, and the **uniform ledger-window chrome** principle (the header is exempt as persistent chrome).
- `PROFILE.md` — the adjacent top-left identity panel.
- Economy/market systems (`SYSTEMS.md`) — source of the numbers shown here; price resolution feeds the valuation figure.
