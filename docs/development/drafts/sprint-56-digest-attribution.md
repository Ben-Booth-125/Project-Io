# Sprint 56 — BL-1338 (digest pins stale): mover attribution

**Attribution only (Ben, 2026-10-16).** No pin in `tools/verify/player_seed_sweep.cpp` is
changed by this draft. The re-pin waits for integration, after sprint 55 lands. The table in
§ 4 is the candidate re-pin at the sprint 56 cut, not a ruling.

Taken 2026-10-16/17 on the worktree branch, walking to `aa3c6304` (the sprint 56 cut, which
includes main `42eb9d99`).

## 1. Method

The precedent is the 2026-10-04 re-pin: the same check on a build of every intermediate main.

- **Base.** `484ccb29` (2026-10-10, the sprint 50 re-bless) reproduces every pinned digest for
  seeds 0, 28, 46 on both arcs. So does `01ae28b2` (the sprint 50 tip) and `a560c5d7` (sprint
  50 merged onto main). The pins are good up to `a560c5d7`.
- **Walk.** Every first-parent commit of `aa3c6304` after `a560c5d7` that touches `src/world`,
  `src/core`, `tools/verify/harness_params.hpp`, `tools/verify/world_digest.hpp` or
  `tools/verify/player_seed_sweep.cpp` (35 commits) was built and run. No splitting was
  needed: each probe took 2-5 minutes, so the walk is exhaustive, not bisected.
- **Second parents.** Two first-parent merges named no single item, so their second-parent
  chains were walked too. `933257ff` (sprint 52) → `6311a090`, `14e8cace`. `15d3c103` (main
  into sprint 54, carrying sprint 53) → its merge base `54d462a0` plus the 18 world-touching
  first-parent commits of `589f8c87`. `54d462a0` reads digest-identical to `933257ff`.
- **Before the shelf merge.** `0526a1f2` (main before `a560c5d7`) reads identical to the merge
  base `d0c74dea`: sprint 51's side moved nothing.
- **Seeds.** 0, 28, 46 per probe, both arcs. The 16-seed table (§ 4) is taken at `aa3c6304`.

### Commands

```bash
git checkout --detach <sha>
bash tools/verify/build_lua_harness.sh player_seed_sweep
build_gen/verify/player_seed_sweep.exe --digest --seeds <s> --arc shipped   # one process per seed
build_gen/verify/player_seed_sweep.exe --digest --seeds <s> --arc legacy
```

The 16-seed table ran the same command per library seed
(`46,28,11,31,40,12,37,13,41,43,32,10,25,38,9,0`), one process each, per arc.

### Toolchain

As the pins' provenance: MSVC cl 14.44.35207 (VS 2022 BuildTools), Windows SDK 10.0.26100.0,
Release `/O2 /MD /DNDEBUG` via `build_lua_harness.sh`. The build log names cl 14.44.35207.

## 2. Movers

Key: **S** D_search, **L** D_land, **T** D_settle, **Z** D_seat. "All" means seeds 0, 28 and
46. Every row below moved all three probed seeds. "Intended" means the commit's subject or
body states a world or save change.

### First-parent movers

| Commit | Item | Shipped | Legacy | Intended? |
|---|---|---|---|---|
| `933257ff` | Sprint 52 (Digitisation tech tree). Inside it: `14e8cace`, BL-1273 (nation tech state). | LTZ | LTZ | Yes — new nation tree state, a research pool and a queue, all serialised. |
| `14e54de5` | Unique market colours and names (Ben, 2026-10-13) | LTZ | LTZ | Yes — its body says city names move "the deep digest". |
| `a86e499f` | BL-1322 (workforce stepped auto); its subject says BL-1310, the id before the sprint 54 renumber | TZ | TZ | Yes — its body says D_settle/D_seat move (the rival dial grid is 0/25/…/100 now). |
| `8dd79087` | BL-1322 (workforce stepped auto): the 0-200 internal scale | TZ | TZ | Yes — a follow-up to the same item. |
| `15d3c103` | Main (sprint 53) merged into sprint 54. Split in § 2.2. | SLTZ | SLTZ | Yes. |
| `0bd77b66` | History integration lane: BL-1303 (stale reading all spans), BL-1304 (revolution a third), BL-1288 (late culture target) | SLTZ | SLTZ | Yes — history tuning. |
| `529806f9` | BL-1255 (rail rung): generated rail lines | LTZ | LTZ | Yes. |
| `dedbd495` | Sprint 53 economy pass: BL-1301 (name moves economy), BL-1305 (research pacing recal), BL-1307 (profit staffing copy), the rate ladder 60→50, state foundings nearest their feed, the rail re-tune | SLTZ | LTZ | Yes — tuning. Not split further. |
| `06c88f17` | State ports: a major's coastal seat holds a state port | LTZ | LTZ | Yes. |
| `64730588` | BL-1309 (logistics one pool) | LTZ | LTZ | Yes. |
| `fdd25faa` | BL-1309 (logistics one pool): the AI builds logistics sources (granted) | TZ | TZ | Yes — AI behaviour in the settle. |
| `7b7f0573` | Economy pass: food processors placed on real surplus | SLTZ | SLTZ | Yes. |
| `2ddffb36` | Economy pass: re-ladder to rate 1 (`industry_points_per_1000_treasury_units` 50→1) | SLTZ | — | Yes. Shipped arc only, as expected: the legacy arc runs no Industrialisation span. |
| `29efedd2` | BL-1327 (space asset model): save v51 | LTZ | LTZ | Yes, as a format change — the snapshot gains a space-asset section. |
| `fbfdf604` | BL-1335 (high tax productivity penalty): taxes and Discontent, save v52 | LTZ | LTZ | Yes. |
| `e135d3f8` | BL-1335 (high tax productivity penalty): review fixes | LTZ | LTZ | Yes — no tax in the settle, plus per-corp return taxes in the v52 section. |

### Inside `15d3c103` (sprint 53, on main's first-parent line)

| Commit | Item | Shipped | Legacy | Intended? |
|---|---|---|---|---|
| `bdaf1947` | BL-1272 (generation without research): no trees in the history; sea legs need a fleet | SLTZ | SLTZ | Yes. |
| `99783dda` | BL-1278 (nation count targets) lane B: the size floor and the nation total as closing rounds | SLTZ | — | Yes. Shipped arc only (Industrialisation's closing rounds). |
| `13407cb2` | BL-1283 (one champion per major) / BL-1278 lane C | SLTZ | — | Yes. Shipped arc only. |
| `757cbad4` | BL-1290 (civil war lean) and BL-1286 (history pattern detectors) | SLTZ | SLTZ | Yes. |
| `10cc388f` | BL-1298 (history banks more) + BL-1283 champion placement | SLTZ | SLTZ | Yes. |
| `7753f750` | BL-1287 (rise and fall target) | SLTZ | SLTZ | Yes. |
| `0be55418` | BL-1289 (world war target), 1900-1930 | SLTZ | — | Yes. Shipped arc only (the war is inside the Industrialisation span). |
| `2fdf38d3` | BL-1296 (state industry) | LTZ | LTZ | Yes. |
| `8435a20c` | BL-1288 (late culture target) | SLTZ | SLTZ | Yes. |

### Probed and read identical

These touch world paths but moved no digest on seeds 0, 28, 46, either arc:

- `92d0caf5` BL-1318 (nation name cap; BL-1306 in its subject, before the renumber). These worlds have no name over 20 code points.
- `db12b765` BL-1321 (settle ticks feed readings).
- `e7b19824` BL-1288 (late culture target): sea contact.
- `cc148277`, `6dbe3d21`, `3957b008`: rail construction UI and build rows.
- `396ef1ca`: live-click wizard fixes.
- `36dc3f3c` BL-1334 (spectate from seat screen).
- `131dbefa` BL-1324 (rising tech prices).
- `9e5b4c0f` BL-1275 (nation research AI).
- `47f37203`: main merged into the BL-1335 lane.
- Sprint 54's own UI merges.
- Sprint 53: `e3f049d7`, `221b3bd6`, `b1fca210`, `10801b97`, `06b115f8`, `dff80cda`,
  `747b3d9b`, `76883caf`.
- `a61986b4` → `f7c47d62` BL-1339 (life reroll moves the world). See § 3.

## 3. Unexplained, and one note

**No unexplained mover.** Every moving commit's subject or body names a world, economy,
history or save-format change. Each change matches the arc it reached: four shipped-only
movers sit in the Industrialisation span, which the legacy arc does not run.

**Note — BL-1339 (life reroll moves the world) moved no digest here.** It is filed as a known
world mover. `aa3c6304` reads identical to `e135d3f8` on seeds 0, 28, 46, both arcs, and
`a61986b4` is the only world-touching commit between them. The harness builds an unrerolled
world, so this is consistent with the reroll folding in only when roll[1] is non-zero.
Two things are therefore NOT checked here:

- that the no-reroll world is byte-identical by design;
- the reroll path itself. BL-1339's own harness, `tools/verify/life_reroll.cpp`, covers that.

Whoever re-pins should confirm the first reading against BL-1339's design.

**Note — three rows are format-only in kind.** BL-1273 (nation tech state), BL-1327 (space
asset model) and BL-1335's v52 section add serialised state. Their D_search holds and
L/T/Z move. That is the signature of new snapshot bytes. It is not proof that no behaviour
changed.

## 4. Candidate re-pin table at `aa3c6304`

Every row seated exactly one player (32/32 rows `players 1`). **NOT applied to the code.**

Seed 0 shipped D_search `5CC46DB210F04D68` reproduces the 2026-10-16 finding.

### Shipped arc (`k_shipped_digest_pins` candidate)

| Seed | D_search | D_land | D_settle | D_seat |
|---|---|---|---|---|
| 46 | `EC6ED1439E6EA409` | `2B2200CBAAD297F4` | `6F1097B68FCC1D80` | `7AEF9942302F1BEB` |
| 28 | `22B21DC634E293C7` | `74777C83CCE81B5F` | `1A67C0CEE3BA13FE` | `3B1B7A79147ED86F` |
| 11 | `AC4D8F3F982D25C4` | `1A9347FFBC01B1AA` | `9233C2BF1942CE05` | `CF14D5D750DC7AAD` |
| 31 | `50C28C135BAB46BE` | `A9D2470CE7B3B054` | `4B207AEF41FDFE95` | `441E88874F6FC905` |
| 40 | `421E7C22E59798AF` | `290E1D89DE6F55CA` | `D8F9161E866A6EEB` | `D955F6B4D010A69E` |
| 12 | `5C417FF3F2783B0E` | `985AE9FA19AFC86C` | `18532473C63ACB92` | `55FFF70A092A9791` |
| 37 | `02EB43B1842127B2` | `6FE8E44641F84BCA` | `61B5894D7B9831E3` | `86412B0687CED47F` |
| 13 | `1F5FBA252EA0636A` | `CB3D911C0B7EDE50` | `73441B423272E394` | `846EA97CC556332D` |
| 41 | `C416624D2593DA51` | `4ECF6FB20DF4C972` | `8FF3EB946A462C1E` | `61A17EFE129A4EA1` |
| 43 | `C3508908CC60804D` | `6CF1BAEB927676F8` | `16D599909AC5D8D0` | `4E6A4D6016408686` |
| 32 | `C3882F670405ED65` | `91A71155C0F822B1` | `8074E37A6A6A12E7` | `FCF44800D158590A` |
| 10 | `A4DBBEDD638E33EE` | `C5ED42374BAC4A85` | `AC90098149784241` | `D70927DFEC051240` |
| 25 | `0460FAE0C91EEB57` | `600D9AAC4FCA5D4D` | `BF7D009535B0E349` | `88C6F5FD6E422B43` |
| 38 | `4F8EEC30A41E5822` | `D97F33BE0AFD5C47` | `D296E3F291030424` | `A8FCE0D96069C3F7` |
| 9 | `02AFE0532EB7E676` | `1F4B0A1A4DAE6898` | `74B4621118C9969C` | `EC89C126F60BE1EC` |
| 0 | `5CC46DB210F04D68` | `6287ED64F4073391` | `C127E302F96B2325` | `1F36D7A1C0DBED20` |

### Legacy arc (`k_world_digest_pins` candidate)

| Seed | D_search | D_land | D_settle | D_seat |
|---|---|---|---|---|
| 46 | `F827102A62BDFAF3` | `DA19CA15906DB80D` | `55DBC3D9BCA1B696` | `7F5F42316C351136` |
| 28 | `69D26AFEEE5ADCCC` | `BBBAB18FD4B4FAA2` | `0C14E183AEB33419` | `620D05539EB97D0E` |
| 11 | `848A563CCEC48C14` | `2C31F01E6774F972` | `3818DE3603641D8F` | `A83B9A6550AC1160` |
| 31 | `87E379A77168B8A3` | `C8460E96A0F13D1F` | `C2D650B0FCCD1BB6` | `5A9C4C17126D97A8` |
| 40 | `33FE4ED64003706F` | `885315370A5ED8C5` | `E1D8C7B3997D5147` | `572A4CE0E8827C83` |
| 12 | `2AE2AF76113ADB96` | `CB93A21BF8AAB08B` | `FD003D18A7B9DC36` | `BB61F8D2AB442669` |
| 37 | `EFFD07FA2CC9D7C3` | `9CA89C741BB24144` | `C0ADBCF16FEE379E` | `F8B8DE989F126CAC` |
| 13 | `2064741DCC9190C5` | `C9DE0FDC8881A9C4` | `D3AB08616DF61C65` | `166D11764D7EE53E` |
| 41 | `A9DF54B324995461` | `D50E16BD10698C9D` | `7D92ADE64E6AAAB3` | `96D66BE390D3DD7F` |
| 43 | `14883823861AE9DF` | `B89EEE9D3F1407CC` | `B1D6620814405CEF` | `AF53906F3B18BE73` |
| 32 | `BCCBA5931DD48338` | `FAC49CF6877F6ADC` | `0FB5F58F363E4F0E` | `B83DB14A74D37E41` |
| 10 | `3EBDAE0989E31C7B` | `C537C6887FC677CE` | `4B8F46B6C1BF225B` | `6EF9DC1E9BE2A22C` |
| 25 | `8DAAE30BDAAD3AB2` | `748B42CE63577E72` | `A27F2357434BB585` | `45E5B000CB7F16D4` |
| 38 | `F55A05A47BBBEFEF` | `39C5E3254160FCDD` | `7DF317BE92AF6026` | `A6C46A899E2A9D48` |
| 9 | `541B3ACEF140117D` | `8D3E43CA8E02DF75` | `BB2E7393BF744596` | `FE231B70787DC65F` |
| 0 | `6F7C5B6DAA30EBFB` | `B3F156D51FA98824` | `6593FDE221698F19` | `A6E8A6CBBC891554` |

**At integration** these values go stale again once sprint 55 lands. Re-run the 16-seed
command on the integrated tree. Then attribute any further change against this table, not
against the 2026-10-10 pins.
