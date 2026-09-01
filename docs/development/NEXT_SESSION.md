# Next session — Sprint 27: demand, and the two scale-blind selections

Sprint 27 is **open**. Ten items. The previous session (2026-08-31) closed sprint 26 complete and
handed this over deliberately rather than starting it tired.

## The sprint in one line

Give the goods a buyer. The economy has a supply chain and almost no demand — and the AI cannot
reach most of what demand does exist, because two selection steps exclude whole categories before
scoring begins.

**Success is NOT "the channels are built."** It is `ai_skill_harness` showing a field that is not
monotonically insolvent. Channels built and corps still broke is exactly what BL-641 produced on its
own, and it is the failure this sprint exists to avoid.

## Read these first, in this order

| Doc | Why |
|---|---|
| `docs/economy/MARKETS.md` §§ Three properties · Settled: a short pool BUYS | Properties 3–6 are the sprint's whole design. Property 3 is why a pool draw starves. |
| `docs/ai/AI_OPPONENT.md` § Selection must be scale-free | New. It is the rule BL-711 and BL-712 implement. |
| `docs/economy/PRODUCTION.md` § A shortfall scales output; it never idles | New. Binds every channel you build. |
| `docs/development/DEVELOPMENT_PRACTICES.md` §§ A harness must build the world the application builds · A broken check is worse than a failing one | New, and both were earned expensively. |

## Order of work, and why

**1. BL-710 (save roundtrip does not compile) — FIRST, and it is priority A for a reason.**
`tools/verify/save_roundtrip.cpp` has not compiled since the mercenary tear-out (`cc88997c`). It
references `world::mercenary_offers`, `mercenary_contracts`, `next_offer_id` and
`mercenary_contract_state`, all deleted by that commit. **Two save-version bumps landed while it was
dead** — v21 and v22, both appending to the serialised format, neither verified by the harness whose
job is exactly that. Flat binary serialisation is the project's chosen persistence, so this harness
is the only thing between an append-only format and silent save corruption. The item lists all six
regions; every one is a **deletion**, not a repair. After fixing, run it against v22 and confirm the
two landed bumps round-trip.

**2. BL-712 (recipe choice is scale-blind) — before BL-711 and before any new channel.**
Smaller, contained, and it unblocks two features that landed unable to grow. A site takes its
highest-margin recipe, so a cheap universal good never wins — which is why no rival can build a
power plant or a construction sector. Both were designed as *economy-scaled* sinks and neither can
scale. Fix this before building more channels onto the same scorer.

**3. BL-709 (construction as a rate) — MERGED BUT UNCLOSED.** The code is on `main` and builds
green; it was never verified with a census run and its bookkeeping is still open. Verify and close
it before starting new work, or the linter's false-open warning is telling the truth.

**4. BL-711 (extraction candidate list is scale-blind).** Bigger: determinism-affecting, moves every
economy golden. `tools/verify/chain_conversion_probe.cpp` is in the tree and is the instrument that
diagnosed it. Expect one deliberate re-bless wave with dated provenance, never a dribble.

**5. The channels**, in the sprint's own order: BL-642 (construction draws), BL-644 (state),
BL-647 (endemic luxury), then BL-643 / BL-646 / BL-645 at priority B.

**Run `demand_census` before and after every item.** The deltas are the sprint, not an epilogue.

## Tooling you will need, and traps that cost the last session real time

- Build: `cmd //c "<repo>\build_app.bat"` — **absolute path**, the bare name does not resolve. It
  cold-configures when `build/` is absent, so use it rather than improvising a generator; a
  different generator changes codegen and has already forced one re-verification.
- Ordinary harnesses: `node tools/verify/build_harness.js <name>`, then `build_gen\verify\<name>.exe`.
- **Lua-linked harnesses** (`demand_census`, `chain_depth`, `spawn_solvency`) need
  `cmd //c tools\verify\build_lua_harness.bat <name>`. `build_harness.js` fails them with unresolved
  externals; CMake needs a configure a worktree does not have.
- **Prebuilt exes under `build_gen/` may be STALE and will PASS while reporting on an older world.**
  Rebuild before trusting any harness result.
- **Worktrees are cut from `origin/main`, which has DIVERGED from local `main`.** A fast-forward is
  impossible. Branch fresh with `git switch -C <branch> main`. **Never hard-reset to `origin/main`** —
  it would discard the local work. Six agents hit this in one session; all six handled it, none was
  warned by anything but the brief.

## Known-red, and NOT yours unless you are fixing them

- `spectator_determinism` R2 byte-identity golden — stale by 150+ commits (NR-752). Re-bless is Ben's.
- `ai_skill_harness` bands — stale since 2026-08-16 (NR-305), plus a real behavioural change on top.
- `chain_depth` — 8 pre-existing `injector::none` rows.
- `save_roundtrip` — does not compile. That is BL-710, above.

**Report, do not re-bless.** A golden re-blessed by whoever trips over it is a golden nobody reviewed.

## Open questions carried in, all Ben's

- **NR-763** — self-sufficiency is currently the norm rather than the exception, and chain closure
  saturates so input asymmetry does not survive into capability asymmetry. Recommended first probe:
  vary `economy.construction.max_logistics_reach` and re-read BL-706's spread. One constant.
- **NR-766** — the seeder sizes against consumer demand only, never processing-input demand. Upstream
  of everything here; unowned.
- **NR-762** — ~30 harnesses skip the app's world-building tail. Unowned.
- **NR-765** — the argmax family; BL-711 and BL-712 are its two fixes.

## What sprint 28 is waiting for

BL-697, BL-699 and BL-698 are **proposed**, not open. They are the systemic brake, and they are
blocked on this sprint: a coalition forms against whoever leads, and that needs a leader worth
forming against. Read `sprints.json` § 28 before touching them — it carries two warnings its future
self needs.
