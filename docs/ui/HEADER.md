# Project Io — Header

> **Settles:** which question the top strip answers without opening anything ·
> which money and material figures it carries and how a trend is shown · which
> survival signals share that row and when each is worth showing · what the strip
> gives up first as width runs out.
> **Not here:** the detail behind any figure (ledgers/*) · the identity tile
> (PROFILE) · the clock (TIME_CONTROLS) · placement (LAYOUT).
> **Confused with:** PROFILE.md, TIME_CONTROLS.md, LAYOUT.md.

The **header** is a full-width strip across the top of the canvas area, sitting between the profile (top-left) and the time column (top-right). It top-aligns at `y=0` and stands at the profile card's full height (`profile_panel_height`), so it reads level with the identity tile as a single top band; its content row is vertically centred within that band. It is the player's persistent financial and material dashboard — always visible, glanceable, never opened or closed. See `LAYOUT.md` for where it sits in the shell.

It reads the live economy (`src/ui/header_panel.{hpp,cpp}`) and is a **summary surface**: three money figures, with the trend on hover and all detail left to the ledgers, and one survival signal — runway — on the same row.

---

## Purpose

Answer, at a glance and without opening a ledger: **how much money do I have, what is my material worth, and which way is it trending this quarter?**

## Contents

The header reads left to right: the treasury with its net in brackets, then cargo (Ben, 2026-11-05).

- **Treasury** — the player nation's treasury (`nation_component::treasury` for the seat's nation; re-subjected from the retired corporation balance, Ben, 2026-10-23). A nation never runs into debt (`../politics/NATIONS.md` § What a nation owns), so the figure has no negative state to flag. **Hovering the treasury, its net or the cargo figure shows the trend graph** in a tooltip (Ben, 2026-11-05).
- **Cargo valuation** (labelled **CARGO**) — an estimated value of the goods the player **owns**: its trades' cargo in transit, each in-flight shipment priced at its **destination** market's current price — the price it lands into. A nation holds no stockpiles (`../economy/MARKETS.md` § The shelf economy): what its buildings make is sold where it lands, so its trades' cargo is the only stock it owns between purchase and sale (`../economy/TRADE.md` § A trade). A single currency figure, deliberately *not* a per-resource inventory; it reads zero with nothing in flight.
- **Net, in brackets beside the treasury (Ben, 2026-11-05)** — the **last economy tick's operating net**: the Balance card's own "Net this quarter" (income, less expenditure, maintenance and wages, plus a nation's tax income), so the strip and the card always agree. **Refunds and flat construction are excluded** — cancelled construction credited back is not earnings, and construction is not an operating flow (Ben, 2026-10-26). The nation's Balance card has no refunds line either: its net is the same operating figure. Shown as a coloured signed per-quarter figure in brackets, `TREASURY 1.2M (+34/qtr)` (`fmt::rate(net, "qtr")`, coloured by `palette::value_colour`). The graph of recent treasury values (`ImGui::PlotLines` over a capped history maintained in `app`, pushed each `step_economy()`) is the tooltip above, not part of the strip.
- **Runway** (BL-177, budget runway legibility) — how long the treasury lasts at the current burn: `RUNWAY  12q` (treasury ÷ the last tick's fall in the treasury). **Its source is not NET's**: the burn is the treasury's actual movement over the last economy tick (the state ledger's close less open — construction and refunds included), because a nation at a positive operating net can still be spending its cash on construction. NET answers "is my economy earning?"; RUNWAY answers "how long does my cash last?". **Shown only while it means something** — burning cash; a growing treasury shows nothing rather than a lying "infinite quarters" figure. **Urgency-coloured**: red under 8 quarters of cushion (about two years — the point it needs acting on), plain text otherwise. Tooltip explains the arithmetic and its steady-burn assumption. Load-bearing, not decorative — it degrades through the fit ladder below but the fact is never simply lost. When the treasury runs out, a losing building idles rather than the nation borrowing (`../politics/NATIONS.md` § What a nation owns).
- **Recruits** (Ben, 2026-11-02) — at the **top right**, one readout per theatre: **land**, **air** and **sea**, in that order. Each is the theatre's glyph, then the nation's recruits held against the pool's cap (`120 / 750`), with a thin **bar** of held over cap beneath. Its tooltip names the pool, its quarterly output (the running bases, airbases or ports and their rate) and that a full pool takes none (`../military/MILITARY.md` § Recruits). A theatre with no source and nothing held reads `0 / 0` with an empty bar, so the three always stand. This widens the header's question to *and how many men can I raise?*
- **The `[in debt]` flag retires** (Ben, 2026-10-23): it flagged a negative, interest-accruing balance, and a nation never runs into debt.

## Fit and degradation

The header is a **guaranteed-fit strip** (LAYOUT.md container vocabulary): it never wraps to a
second line and never silently clips a load-bearing number. `draw_header_panel`
(`src/ui/header_panel.cpp`) measures everything against the available width and degrades in a
fixed order:

1. **Runway ladder** — resolved first, in three honest steps:
   - full — `   |   RUNWAY  12q` (label + value);
   - value-only — `   |   12q` (label dropped, tooltip explains);
   - none — **folded into the treasury's tooltip**, so the fact survives even when the strip
     cannot draw it.
2. **Recruit readouts** — next, in two steps: the numbers drop to the held figure alone (`120`),
   then the three readouts fold to glyph and bar only, their numbers moving into each glyph's
   tooltip. The bars never drop. The readouts are drawn compact (Ben, 2026-11-02): their glyph-and-bar floor is
   52 px, so at the 1280 × 720 floor the runway keeps its value (`   |   12q`).
The strip carries no decorative element to drop; the graph lives in the tooltip (Ben, 2026-11-05).
The three money figures (Treasury / Cargo / Net) are bounded-length (`fmt::credits` /
`fmt::rate` abbreviate) and always drawn in full.

This ladder is the **reference case for BL-215** (text-wrap render audit): the audit's job is to
make every other worded surface degrade this honestly rather than wrap or clip.

## Settled design questions

The header design pass (2026-06-15) settled:

- **Resource overview** — *not* a scarce per-resource icon strip; the single **cargo valuation** figure instead. A per-resource header strip was judged too lossy at a glance and redundant with the ledgers.
- **Per-tick delta** — yes, the header shows the **last-tick net** (not the treasury alone), with direction of travel in the hover graph (Ben, 2026-11-05).
- **Change animation** — none; the hover graph carries the trend. Flash/tick animation is a possible later polish, not specified here.

## Related

- `LAYOUT.md` — placement in the shell, and the **uniform ledger-window chrome** principle (the header is exempt as persistent chrome).
- `PROFILE.md` — the adjacent top-left identity panel.
- Economy/market systems (`SYSTEMS.md`) — source of the numbers shown here; price resolution feeds the valuation figure.
