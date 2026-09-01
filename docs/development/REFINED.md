# Project Io — REFINED (active worklist)

**Success-lever session — 2026-09-01.** Goal: with 0 research points spent, a skilled player can
build a functioning economy and start gaining influence around their spawn. This block lands the
two missing buyers first, then the measurement battery designs. Sweeps run in later sessions on
the BL-723 instrument.

**Base:** `ca8d6476`. The review queue holds one entry (NR-774, the four definitional calls).

---

## In flight — worktree agents

| Task | Branch | Verification required before merge |
|---|---|---|
| **BL-644** (state channel) — tenth budget line `space_programme`, buying and CONSUMING spacecraft_components + propellant; append-only enum; conservation-exact transfer | `bl644-space-line` | `nation_budget_harness` rows extended, each proven able to fail; `money_conservation` green; clean full build; save-version bump via the registry if the weight vector moves the seam |
| **BL-647** (endemic luxury) — wealth-scaled, nation-flavoured household demand for the luxury goods; adds them to `k_extractable` (the BL-586 slice-2 gap) | `bl647-endemic-luxury` | New/extended harness rows (deposit-where-wealth, differing-nation baskets, determinism, placement gap closed), each proven able to fail; `demand_census` before/after in-tree — luxuries leave the no-sink list |

Agents build and commit on their branches; **this session merges, builds, verifies** — nothing
self-reported is assumed. Integration owns the `demand_census` channel enumeration update (both
new channels must read PRESENT), re-running the census + `chain_conversion_probe`, and one commit
per item.

## Filed this block

- [x] **BL-644 / BL-647 restored** from the cold store on Ben's instruction; cold copies cleared;
      lint green.
- [x] **BL-723** (campaign lapse instrument) — the spectated-campaign CSV + film-strip harness
      every sweep runs on; T0 validity rows built in.
- [x] **BL-724…BL-729** (sweep battery) — spawn distribution, price levers, debt dynamics,
      scarcity geography, demand composition, interactions. Each carries its success shape and
      its honest bounds in the item.
- [x] **NR-774** — the four definitional calls (GDP, influence, band, proxy) taken to make the
      sweeps measurable; awaiting ratification.

## Next (after merge)

1. Merge both branches; resolve the one expected collision (`scripts/economy.lua`).
2. Update `demand_census` channel enumeration; full build; census + probe re-run; the luxuries and
   space goods leave "NO market sink".
3. One commit per item, `scoped-commit` path.
4. Build **BL-723** and produce the first time-lapse + baseline CSV this session if time allows.
