# Next session — after the v0.2.0 cut

Handoff written **2026-10-23** by the session that finished sprint 55 and cut **v0.2.0** (the economy
functions, logistics included; ROADMAP.md). Main and the `v0.2.0` tag are **local and not pushed** —
pushing is Ben's (`git push origin main --follow-tags`). The next sprint is not cut yet; Ben opens it.

---

## 1. Where it stands

- **v0.2.0 tagged** on the cut commit. Sprints 55 and 56 are closed and archived cold with their
  retros (`docs/development/archive/sprints-2026-Q4.json`); the sprint 55 ruling register is
  `tools/session/rulings/sprint-55.json` (14 rulings, 0 failing).
- **The digests are re-blessed** (BL-1338 (digest pins stale), Ben 2026-10-23): both
  `player_seed_sweep` arcs pinned at `bd33e496`; the shape and every mover are in
  `docs/development/drafts/v0.2.0-rebless.md`. The next world mover reports its digest movement and
  waits for its wave's re-bless (DELIVERY.md § The digest re-bless is one act per WAVE).
- **Gates at the tip:** market_viability FULL G1 95.7, G1t50 89.1, G1b 2.6, G2 157.5, G3 96.4 — all
  pass; haulage --far-trade 834 / 971 convoys; `lp_coverage_census` every rule passes on 16 seeds
  (21 capitals dark at the fold, 37 at the handoff — nations that cannot fund their nodes);
  world_determinism and save_roundtrip pass.
- **The backlog was cleared for the next sprint** (Ben, 2026-10-23): everything not sprint 55 was
  closed — 4 complete, 32 cancelled — and archived cold. The hot file holds **20 rows**: sprint 55's
  carry-over (the space waves BL-1326..BL-1333, the market-centre items BL-1349/1350/1358/1359,
  BL-1336, BL-1355, BL-1369, BL-1383, BL-1384, BL-1388, BL-1392..1394). `--status cancelled` finds
  the closed-unbuilt ones; three of them were partial and say what remains (BL-1119, BL-1222,
  BL-1231).

## 2. Waiting on Ben

- **His new idea, not filed** (Ben, 2026-10-23): when workforce is short, let the player choose the
  order buildings are staffed in, with a **"most profitable first"** option — beside the ruled
  "network nodes are funded first" for the state's money (NATIONS.md § State industry). Rule 0a: offer
  A) backlog / B) build now. Suggested A — it wants a design pass (PRODUCTION.md § Workforce).
- **Open review calls** (NEEDS_REVIEW.md): NR-1060 (rival hub chains on the player's ground),
  NR-1056 (rail's fade past 20), NR-1055 (tile card Uses counts own buildings), NR-1049, NR-1050,
  NR-1052 and the older NR-1003..NR-1035.

## 3. Owed

- **Live clicks:** BL-1335 (the Taxes control); the BL-1356 card wording ("Not tied to the capital",
  "Topped up from N sources") has not been seen live.
- **Red harnesses** (unchanged by the last items, each failing identically before them): supply_advance
  R5, pools_per_market (a).1/(a).4, unposted_bid U2/U7a, railway_harness, state_industry_harness,
  lp_pool_harness (L7, BL-1383), hauler_room, shelf_share, nation_budget_harness R9;
  tax_discontent_harness does not compile (`net_tail`); the CMake `all` target does not link
  (`battle_engagement_harness`). Sprint 56 recorded state_industry and lp_pool passing at its tip, so
  their red arrived with sprint 55's work — worth a look first.

## 4. Traps this session paid for

- **Shell heredocs eat `\n`** inside Python string literals written through them, even quoted: a C++
  `"...\n"` arrived as a real newline four times. Write edit scripts to a file with the Write tool,
  or use the Edit tool for anything holding an escape.
- **`sed -i` rewrites CRLF files as LF.** Restore with `sed -i 's/\r*$/\r/' <file>` and check
  `git ls-files --eol`.
- **`harness_params.hpp` mirrors the fold's tail call for call.** A new fold step must be added
  there too, or every harness measures the world without it (BL-1398's first reading did).
- **Fixtures without a nation**: a shipment now draws Logistic Points, so a hand-built trade fixture
  needs a nation and a source at its market centre — `tools/verify/lp_fixture.hpp`.
- **A digest walk without switching branches**: `git archive <sha> | tar -x` into the scratchpad, set
  `IO_DEPS_CACHE` to the main checkout's `_deps_cache`, build there.
- Older traps (never `taskkill` cl.exe; hold keep-awake for full sweeps; NR ids mint across hot and
  cold) still stand.
