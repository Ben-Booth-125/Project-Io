# Project Io — Backlog (legacy markdown bodies)

**Drain completed 2026-07-31.** All design prose lives in `backlog.json` → `items[n].design`;
this file holds no bodies. The last five markdown bodies were verified against their JSON items
and deleted: BL-011 (reach lens) and BL-014 (supply-routes lens) landed complete, BL-053 (country
generation) landed complete, and BL-051 (tile-gen refinements) / BL-054 (nation behaviour) carry
their full settled prose in their JSON `design` fields.

The file stays only because `CLAUDE.md` § Documents references it. Do not add bodies here — new
items get their prose in `backlog.json` directly (see [`DELIVERY.md`](DELIVERY.md) § Where design
prose lives).

Seven closed items still carry a `design: "@BACKLOG.md"` pointer in the JSON; their design landed
in the named authority doc, and these tombstones are what the pointer resolves to:

- *(BL-008 ✓ landed — authority `docs/ui/TIME_CONTROLS.md` § Production clock view / `docs/ui/LAYOUT.md`.)*
- *(BL-031 ✓ landed with BL-059 — authority `docs/ui/SELECTION.md`.)*
- *(BL-032 ✓ landed — authority `docs/ui/SELECTION.md` / `docs/ui/LENSES.md`.)*
- *(BL-037 ✓ landed — authority `docs/SYSTEMS.md` § Trade.)*
- *(BL-038 ✓ landed — authority `docs/SYSTEMS.md` § Trade / § Supply.)*
- *(BL-039 ✓ landed — authority `docs/SYSTEMS.md` § Supply / `docs/economy/SUPPLY.md`.)*
- *(BL-045 ✓ landed — authority `docs/SYSTEMS.md` § Infrastructure / § Supply.)*

**Prose authority:** `backlog.json` `design` field. **Metadata authority:** `backlog.json`.
**Method authority:** [`DELIVERY.md`](DELIVERY.md). **Active worklist:** [`REFINED.md`](REFINED.md).
