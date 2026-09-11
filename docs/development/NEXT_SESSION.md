# Next session — sprint 38, the Empire phase

Written 2026-09-11 at the close of a long batch-delivery session. **Sprint 38 is the PHASE, not a
fixed item list** (Ben, 2026-09-11) — it closes when the phase produces what `CIVILISATION.md` says
it must, not when a count is exhausted.

## The headline: the arc lands

`BL-894` (Settle is re-settlement) was the keystone. Measured `--epoch 0`, 8 seeds, single
variable:

| | rule ON | rule OFF |
|---|---|---|
| battles / world | **810** | 7 |
| conquests / world | **396** | 3 |
| largest polity share | **10%**, peaking at 22% | 3% |
| polities eliminated | **8 / 8 worlds** | 0 / 8 |
| **rise → peak → fall** | **8 / 8 worlds** | 1 / 8 |

Every seed now shows the arc `GENERATION_STRATEGY.md` § The asymmetry is POLITICAL asks for.
Closing the frontier was the fix — and it was admissible where a weight change was not, because it
changed what Settle *means*, not what the scorer *prefers*.

**Caveat that must travel with those numbers:** the phase runs **400 of its designed 1,600 years**.
The wizard's epoch is 0 CE and round 5 / pass 2 are not built, so this is the arc over a quarter of
the span.

## Delivered this session

| Item | |
|---|---|
| `BL-894` | Settle is re-settlement. The keystone. |
| `BL-890` | Wizard opens on a rolled seed. Live-checked; the check caught a real regression. |
| `BL-892` | `W7b` re-aimed onto supply; `reach_mod` was never inert. |
| `BL-900` | The sweep's default is now the run the game performs. |
| `BL-889` | Closed as delivered by `BL-894`. |
| `BL-845` | Closed: the 1:1 ratio is absent on the real span (0 of 8). |
| `BL-868` | Delivered after seven failed attempts. Conquests +26%. |
| `BL-893` | **partial** — opens 45% of water refusals, changes no outcome. |
| `BL-891` | **partial** — arc readout wired; live click owed. |

## Read this before measuring anything

Three wrong diagnoses were published this session and each was caught only by one number
contradicting another on the same page. **The instrument was right every time; the reasoning ahead
of it was not.**

- **Check the `TUNED:` banner** before believing any `--set` comparison.
- **Check a counter is not trace-gated** before reading a zero as a finding (`campaign_contacts`,
  `scored`, `cleared`, `chosen` are all gated on `trace_battles`).
- **`--epoch` takes a YEAR.** `8 --epoch` used to run the struct defaults silently; both that and
  `--epoch --set …` now fail loudly (`18c2299d`), and generation's span is the default (`657b0d8e`).

## The 9 open items

**Designed and ready to build (Ben ruled 2026-09-11, `CIVILISATION.md` carries both):**

- **`BL-895`** — materials have **no sink**: campaigns are the only spender, hundreds against
  241,000,000 produced. Two sinks chosen: a standing army eats materials *every year*, and roads
  and works *cost* to build. Stock stays unbounded. Build the sinks, then re-measure whether trade
  is a material share before tuning its magnitude.
- **`BL-896`** — the network is **reach**, not the road graph (which is ~50 edges/world). Ground the
  realm cannot reach **secedes** rather than falling to a neighbour, so the dark age hands forward
  successors. Determinism is a real constraint: a seceding polity's id/seat/culture must be
  allocated from sim state only.

**Design-owed — do not build:**

- **`BL-897`** (universalising creed) — four unsettled questions in the item.
- **`BL-899`** (seafaring creed crosses fed) — four unsettled questions. Pairs with `BL-897`.

**Premise now live for the first time:**

- **`BL-823`**, **`BL-838`** — brakes on a riser. There is a riser now (22%). Read them against the
  new baseline before building; 22% is well short of hegemony.

**Other:** `BL-839` (needs `BL-868` + `BL-838`), `BL-887` (reach as centre chains), `BL-898`
(grudges seed nation sentiment — **corrected before coding**: making the Era −1 scorer read grudges
is forbidden by `BL-827`; the real consumer is `RELATIONS.md`'s seeded sentiment).

## Two things that are not items yet

- **`history_sim_harness` R3a2/R3a3 still fail** on main. The reach gate refuses far targets in that
  synthetic fixture. It denies **nothing** in a generated world, so this is a fixture-only red.
- **Subagents were unusable this session** — two `generation-dev` agents returned with `Bash`,
  `Read`, `Edit`, `Write` all disabled before reading a file. A session-level config fault, not a
  brief problem. All work here was done serially.
