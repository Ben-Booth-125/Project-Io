# Project Io — Header

The **header** is a full-width strip across the top of the canvas area, sitting between the profile (top-left) and the time column (top-right). It is the player's persistent financial and material dashboard — always visible, glanceable, never opened or closed. See `LAYOUT.md` for where it sits in the shell.

Wired to the live economy as of the Layer 3 finalisation (`src/ui/header_panel.{hpp,cpp}`). The header design pass settled it as a **summary surface**: three money figures plus a trend line, with all detail left to the ledgers.

---

## Purpose

Answer, at a glance and without opening a ledger: **how much money do I have, what is my material worth, and which way is it trending this quarter?**

## Contents

The header reads left to right as three figures:

- **Balance** — the player corporation's running treasury balance (`corporation_component.balance` for `world::player_entity`). Negatives are flagged red (`palette::negative`), matching the economy-panel convention.
- **Stockpile valuation** — an estimated liquid value of everything the player holds: each player `(corporation, body)` pool's quantities priced at that body's **current market price** and summed. A single currency figure, deliberately *not* a per-resource inventory — the per-body, per-resource breakdown stays in the Tile / (future) Market ledgers. Resources on a body with no market contribute nothing.
- **Net + trend** — the **last economy tick's net change** as a coloured signed per-quarter figure (`fmt::rate(net, "qtr")`, coloured by `palette::value_colour`), followed by a small **sparkline** of recent balances (`ImGui::PlotLines` over a capped balance history maintained in `app`, pushed each `step_economy()`).

## Resolved design questions

The header design pass (2026-06-15) settled the questions this doc previously left open:

- **Resource overview** — *not* a scarce per-resource icon strip; replaced by the single **stockpile valuation** figure. A per-resource header strip was judged too lossy at a glance and redundant with the ledgers.
- **Per-tick delta** — yes, the header shows the **last-tick net** (not balance alone), plus a sparkline for direction of travel.
- **Change animation** — none in the prototype; the sparkline carries the trend. Flash/tick animation remains a possible later polish, not specified here.

## Related

- `LAYOUT.md` — placement in the shell, and the **uniform ledger-window chrome** principle (the header is exempt as persistent chrome).
- `PROFILE.md` — the adjacent top-left identity panel.
- Economy/market systems (`SYSTEMS.md`) — source of the numbers shown here; price resolution feeds the valuation figure.
