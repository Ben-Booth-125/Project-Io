# Next session — sprint 57, nations own everything

Handoff written **2026-10-24** by the session that designed and built sprint 57. Main is **local and
not pushed** (the `v0.2.0` tag too — pushing is Ben's: `git push origin main --follow-tags`).

---

## 1. Where it stands

- **The design** (Ben, 2026-10-23): corporations and companies are retired; every building is owned by
  the nation whose ground it stands on; a nation builds only on its own ground; the player is its
  nation and builds its own; every AI nation, major and minor, builds through the scorer. Written in
  GLOSSARY, CONCEPT, NATIONS, AI_OPPONENT § 11 and ~60 swept docs (e7879fc3). Ruling register
  `tools/session/rulings/sprint-57.json`: 8 rulings, 0 failing.
- **Landed:** BL-1399 (nation builds own ground), BL-1401 (nation AI actor), BL-1402 (fold to ground,
  with ports E/T/M), BL-1403 (nation money flows), BL-1404 (majors a saved fact), BL-1405 (LP one class),
  BL-1406 (UI and protocol as nation — code merged), BL-1407 (delete corporation_component, f0c62ee4).
  `corporation_component` no longer exists; save version 56 (a v54 stream folds on read).
- **Gates on main:** market_viability FULL G1 93.0, G1t50 85.2, G1b 4.6, G2 94.1, G3 100.0 (all pass;
  G2/G3 now measure nations and buildings, NR-1068); world_determinism, save_roundtrip (97/0),
  fold_census, nation_actor_seam, agent_seam (22/0), tax_discontent (44/0), money_conservation pass;
  majors identical on all 16 library seeds. The app builds.

## 2. Waiting on Ben

- **BL-1406 live click** — eight checks in its requirement row (`requirements.json`, group
  `ui-as-nation`): header treasury, Nation overview, inert slots 5/9, Construct on own vs foreign
  ground, unowned-ground road text, survey/hire/trade/research presses, seat screen, profile.
- **Calls:** NR-1064 (sweep readings), NR-1065 (demote, not fold), NR-1066 (six questions: emblem,
  embargo, rival disclosure, works lean, focus, Industry lens), NR-1067 (wage-tax rates, duty cap),
  NR-1068 (G2/G3 re-pointed, firm exit inert, seat clean slate), NR-1069 (treasuries fall; seed 46
  majors stop building), NR-1070 (verify.fold_world, deleted verify scripts, one golden),
  NR-1071 (unit upkeep is the garrison claim, no AI hire, AI trade parked).
- **BL-1408 (the one re-bless)** on Ben's go — digests and floors move with every item here, plus
  the icon_silhouettes_overview golden and ai_skill bands.

## 3. Owed

- **BL-1409** — measure first: on seed 46 a whole-economy nation finds no viable build and treasuries
  fall (27k -> 8.5k over 40 seated ticks on nation 47763). AI trade/Marketplace/survey for nations is
  parked as `docs/development/drafts/bl-1402-t8-nation-ai-trade.patch`: re-opened it fails G1t50 (84.0;
  84.6 with pins off). Same question; Ben decides after the measurement.
- **Known reds, unchanged by sprint 57:** nation_budget R9a-m, lp_pool L7, railway R6,
  history_sim R3a2/R3a3, spectator_determinism R1, a few visual scripts (lens_structure_pivot,
  lens_one_tier, convoys_ledger, shell_pass). `.claude/skills/verifier-visual/SKILL.md` and
  `verifier-headless/SKILL.md` still name retired scripts/harnesses (skill edits are Ben's).

## 4. Traps this sprint paid for

- **Corp-keyed metrics go silently to zero** once ownership moves: market_viability's G2 read 0.0
  until re-pointed. Any harness that walks owners must read nations.
- **Never edit src while a gate script compiles from the tree** — later builds pick up the edit.
- **harness_params.hpp mirrors finish_campaign_world call for call**; a new step goes in both.
- **A pooled owner exposes double counting** that per-firm worlds hid (the scorer's floor read the
  already-committed treasury).
- Older traps still stand: shell heredocs eat `\n`; `sed -i` rewrites CRLF as LF; never `taskkill`
  cl.exe; NR ids mint across hot and cold; background build exit codes are the echo's.
