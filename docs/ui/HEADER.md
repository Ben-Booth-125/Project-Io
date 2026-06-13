# Project Io — Header

The **header** is a full-width strip across the top of the canvas area, sitting between the profile (top-left) and the time column (top-right). It is the player's persistent financial and material dashboard — always visible, glanceable, never opened or closed. See `LAYOUT.md` for where it sits in the shell.

This document is a placeholder to be expanded; the notes below record current understanding.

---

## Purpose

Answer two questions without opening a ledger:

- **Can I afford this?** — the budget readout.
- **What do I have?** — a scarce, summed resource overview.

## Contents

**Budget**
- Current treasury balance (the player's liquid funds).
- Eventually: net income/expenditure per economy tick, so the player sees the direction of travel each quarter.

**Resource overview**
- A compact strip of the player's headline stockpiles, summed across all holdings.
- **Deliberately scarce in the prototype** — a small handful of resources shown as icon + quantity. Not a full inventory; the per-body detail stays in the Tile Ledger and later resource ledgers.

## Open questions

- Which resources earn a slot in the prototype strip, and how is that set chosen (fixed list vs. most-held)?
- Does the budget show a per-tick delta in the prototype, or just the balance?
- Behaviour when a value changes — flash/tick animation, or static?

## Related

- `LAYOUT.md` — placement in the shell.
- `PROFILE.md` — the adjacent top-left identity panel.
- Economy/market systems (`SYSTEMS.md`) — source of the numbers shown here.
