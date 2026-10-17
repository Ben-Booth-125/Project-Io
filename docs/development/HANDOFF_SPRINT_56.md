# Sprint 56 handoff — trustworthy ground (2026-10-17)

Sprint 56 ran beside sprint 55, on branch `claude/sprint-56-456d2b` in its own worktree, while Ben
playtested main. It is **gated, not merged and not pushed**. The retro is in `sprints.json`.
This note is for the session that brings the branch onto main after sprint 55 lands.

## What the branch carries

| Item | State |
|---|---|
| BL-1337 (return net crosses zero) | Delivered. Save **v54**. |
| BL-1214 (eighth field doc drift) | Delivered (FINANCE.md). |
| BL-1338 (digest pins stale) | Movers attributed; **re-pin owed at integration** (Ben). |
| BL-1319 (fixture base stall) | Delivered; consumers' own failures filed as BL-1376. |
| BL-1310 (nations contiguous) | Delivered; world mover. |
| BL-1247 (ground benchmark) | Delivered: `ProjectIo --bench-ground`. |
| BL-1311 (ground paint perf) | Delivered; the 6 GB rest is BL-1377 (ground under six GB). |
| Wizard footer anchored | Delivered (STARTUP.md, Ben 2026-10-16). Owes Ben's live click. |

## Integration steps, in order

1. **Merge main into the branch, not the other way first**, and resolve there.
2. **Save versions.** `save_version_reservations.jsonl` holds v53 for BL-1340 (roads cross tile
   edges, sprint 55) and v54 for BL-1337. If BL-1340 has landed on main, BL-1337's exact-net
   section goes after its section and `k_world_save_version_pre_net` moves from 52 to 53
   (`world_save.hpp`). The five harnesses that cut streams back (`save_roundtrip`, `lp_pool_harness`,
   `railway_harness`, `space_assets_harness`, `tax_discontent_harness`) subtract
   `world_save_net_section_bytes`; recheck them after the stack.
3. **Ids.** This branch minted BL-1370..BL-1377 and NR-1049..NR-1051 from the main tip of
   2026-10-17 (main then held BL-1369, NR-1048). Sprint 55 may have minted the same numbers since:
   compare and renumber the later ones with their cross-references.
4. **Re-run the gates on the integrated tree** (both sprints move the world): `market_viability`
   full, `history_patterns` full, `history_sim_harness`, `world_audit --seeds` (library),
   `nation_targets_harness`, `world_determinism`, `save_roundtrip`, `quarterly_return`.
5. **Re-pin the digests (BL-1338)** on the integrated tree — authorised in principle (Ben, 2026-10-17) once both sprints are merged and the gates pass:
   `player_seed_sweep --digest-check --arc shipped|legacy` over the 16 library seeds. The mover
   table and the method are in `drafts/sprint-56-digest-attribution.md`; add sprint 55's movers
   and BL-1310 (which moves both arcs since every launch obeys the strait rule).
6. **Archive the delivered rows** (BL-1337, BL-1214, BL-1319, BL-1310, BL-1247, BL-1311, and
   BL-1338 once re-pinned) with `archive_landed.js` — it evicts on any call, so run it once, on
   purpose.
7. **Debug build trees rebuild once**: BL-1311 switches on CMake policy CMP0184 and builds the
   ground bake at /O2 in Debug (kept, on by default: Ben, 2026-10-17, NR-1051).

## Open for Ben

- BL-1377 (ground under six GB): decided later (Ben, 2026-10-17); options C (stream the master) or D (96 px).
- The wizard footer's live click, if not yet taken.

Resolved 2026-10-17: NR-1049 (strait 3 tiles kept; exact net in the acquisition price kept), NR-1051
(Debug-optimised bake kept, on by default), the BL-1338 re-pin authorised in principle.
