# Sprint 54 digest movement, measured for BL-1277

Measured 2026-10-13 for BL-1277 (tech wave re-bless). This is measurement only: no source changed.

## What was compared

| label | commit | what it contains |
|---|---|---|
| BEFORE | `9f97bcfc` | the sprint 54 base |
| CAP | `92d0caf5` | BEFORE plus BL-1318 (nation name cap), and nothing else in `src/world` |
| CITY | `14e54de5` | CAP plus unique city names per body (`city_names.*`, `population_generation.cpp`) |
| AFTER | `aeae0430` | head of `claude/sprint-54-ui-improvements-f2d1ec`: CITY plus BL-1320 (market trade value record) and UI |

These four commits are the only ones between BEFORE and AFTER that touch `src/world`. No `scripts/*.lua` data
file read by generation changed; only verify scripts did.

Both harnesses were built at each commit with `bash tools/verify/build_lua_harness.sh` (Release /O2) and
copied to their own names. Runs:

- `player_seed_sweep --digest --seeds <s>`: one process per seed, all sixteen library seeds, six at once.
  This is the shipped arc with the default charter budget (`none`, the stockpile path). Every run exited 0
  with `players 1`.
- `world_determinism` (no args): ALL PASS at all four commits.

## Summary

- **D_search: same on all 16 seeds.** The landscape walk does not read names.
- **D_land, D_settle and D_seat: moved on all 16 seeds.** The snapshot bytes carry names, and all three
  digests hash the snapshot.
- **Unique city names move all 16 seeds** (CAP -> CITY: every seed moved in all three snapshot digests).
- **BL-1318 (name cap) alone moves 3 seeds: 31, 43, 10** (BEFORE -> CAP). Those seeds have a realm or
  nation name longer than 20 code points. The other 13 are byte-identical at CAP. On 31, 43 and 10 the city
  change then moves them again.
- **BL-1320 (trade value record) moves nothing.** CITY -> AFTER is identical on all 16 seeds and all four
  digests, and in `world_determinism`'s deep digests.
- **world_determinism:** the deep digests move at CITY only. The name cap does not move them, because seed
  ABCDEF01 has no over-long name. City names move all three worlds, including prehistory OFF.

| world_determinism digest | BEFORE | CAP | CITY | AFTER |
|---|---|---|---|---|
| seedA/on (twice) | `89517429EECB6E6A` | same | `2B86568726967BC7` | same as CITY |
| seedB/on | `3157EF621E373EF4` | same | `C6A7C58B0AED3139` | same as CITY |
| seedA/off | `BC1194E4BFA8B438` | same | `9FB10FA91E62712F` | same as CITY |
| seedA/on/epoch1900 | `89517429EECB6E6A` | same | `2B86568726967BC7` | same as CITY |

## The shape: names only

No reading the harnesses print changed at any of the four commits; only digests moved.

- **player_seed_sweep**, every seed: all printed lines except the digest row are identical BEFORE vs AFTER
  (`[landscape_search]` wall-clock ms masked). That includes:
  - the snapshot sizes at land, settle and seat, to 0.01 MB;
  - specialists, firms and firms by body;
  - `players 1`;
  - the stockpile points, regions, centres and derived firm price;
  - the account closure and the points ledger, with every unspent reason;
  - the holdings re-measure;
  - the per-body goods list and per-good firm counts;
  - the trade retrofit line (markets with record, Marketplaces bought and placed, unspent);
  - the search path: every round's corps, placement, tier and composite. The search is also hash-identical.

  The same holds for BEFORE vs CAP, CAP vs CITY and CITY vs AFTER.
- **world_determinism**: every line except the wall-clock timing lines and the four digest lines is
  identical across all four commits. That includes:
  - R1: tile, nation and corp counts, and the deposit totals;
  - R2: the abundance totals;
  - R3 coverage: sentiment rows, road and lane tiles by tier, regions, treasury, port stock, standing army,
    navy, nation treasury, corridors by tier, stockpile points, budgeted centres and the carve index;
  - the era report: years, battles, conquests and foundings;
  - the stockpile prices.

So nothing moved in counts, regions, markets, roads, military or the economy. What moved is name bytes in
the snapshot (and so `state_hash`'s inputs downstream of them). That matches both commit messages: the city
commit says "names only"; the cap truncates strings.

One aside for the re-bless: BEFORE's digests are not the rows in `k_shipped_digest_pins`. The pins already
differ at `9f97bcfc`, from the sprint 53 wave, which BL-1277 exists to re-bless. A re-bless taken on AFTER
folds in sprint 54's name movement above. Nothing else from sprint 54 moves the pins.

## Per-seed tables

"moved by" is read off the CAP and CITY intermediate builds. Counters are the row's tail: snapshot MB
land/settle/seat, the roster and the player count.

### Seed 46

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `E857EF697B32414C` | `E857EF697B32414C` | same | — |
| D_land | `8603D0DEB0A18FBA` | `BF5349E1D80CEF78` | moved | unique city names |
| D_settle | `1E49676FCD80CED6` | `38DB5DEA1551D786` | moved | unique city names |
| D_seat | `C3555CC9F23026EF` | `DB4002D69C2F4D3F` | moved | unique city names |

Counters (identical BEFORE/AFTER: yes): `23.07/23.27/23.27  land: specialists 6 firms 103 (by body: 15124:103)  players 1`

### Seed 28

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `E12DBAECD33C9288` | `E12DBAECD33C9288` | same | — |
| D_land | `65D0AAFC70E6413B` | `A5D9B8F2548A766D` | moved | unique city names |
| D_settle | `6FB40864CDC50AF4` | `799C92ACD1806F0E` | moved | unique city names |
| D_seat | `C3823F75182442B8` | `3416B630FC5E7FF6` | moved | unique city names |

Counters (identical BEFORE/AFTER: yes): `22.03/22.31/22.31  land: specialists 4 firms 128 (by body: 15124:128)  players 1`

### Seed 11

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `26C93057012F3530` | `26C93057012F3530` | same | — |
| D_land | `DF2C8DA593F05655` | `16FE9C6E2D414DAF` | moved | unique city names |
| D_settle | `A070D4B7EDEA8EB9` | `EF668CB08518DD4B` | moved | unique city names |
| D_seat | `EF3250CE3570A5E7` | `1245F075B6E11383` | moved | unique city names |

Counters (identical BEFORE/AFTER: yes): `23.68/23.88/23.88  land: specialists 8 firms 100 (by body: 15124:100)  players 1`

### Seed 31

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `3F9C00C195B5D34B` | `3F9C00C195B5D34B` | same | — |
| D_land | `3BF4A8DC2208081B` | `4FAC7E616BD68286` | moved | name cap + unique city names |
| D_settle | `0D925C74BA2FCEC0` | `BD940C52AE82839B` | moved | name cap + unique city names |
| D_seat | `3037C55445A03F5B` | `C1528FE2001C5F98` | moved | name cap + unique city names |

Counters (identical BEFORE/AFTER: yes): `23.78/24.04/24.04  land: specialists 11 firms 109 (by body: 15124:109)  players 1`

### Seed 40

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `A713EAD245D9BF56` | `A713EAD245D9BF56` | same | — |
| D_land | `8AA6D3AB5DAD4C56` | `A43FF8B61D7E6B8F` | moved | unique city names |
| D_settle | `17B1E88FBCE1941B` | `FF6C9727D7466C7A` | moved | unique city names |
| D_seat | `0DAA257672C81603` | `9E536BA3081CE27A` | moved | unique city names |

Counters (identical BEFORE/AFTER: yes): `22.44/22.69/22.69  land: specialists 28 firms 114 (by body: 15124:114)  players 1`

### Seed 12

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `AC41EACA9CF7D585` | `AC41EACA9CF7D585` | same | — |
| D_land | `D5340F76B4C723F0` | `586F1DE9B7B37459` | moved | unique city names |
| D_settle | `9D51848C86126891` | `3FD9B12109CBC794` | moved | unique city names |
| D_seat | `8212EF2FCCB4529F` | `11A9E9328A11280E` | moved | unique city names |

Counters (identical BEFORE/AFTER: yes): `22.03/22.29/22.29  land: specialists 22 firms 113 (by body: 15124:113)  players 1`

### Seed 37

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `E8A486CC82858034` | `E8A486CC82858034` | same | — |
| D_land | `1C5AB544A09601FB` | `67B82471D2B78987` | moved | unique city names |
| D_settle | `3F57BA9A9A01AA9E` | `1AF36A33921BCA7A` | moved | unique city names |
| D_seat | `3FFF45A81AF1A261` | `DA20ED980E08BE25` | moved | unique city names |

Counters (identical BEFORE/AFTER: yes): `23.14/23.35/23.35  land: specialists 4 firms 99 (by body: 15124:99)  players 1`

### Seed 13

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `5365FB19C0F2051C` | `5365FB19C0F2051C` | same | — |
| D_land | `86E43B09DA30EE5B` | `6E42C7E5FADF9C15` | moved | unique city names |
| D_settle | `116580D082456638` | `FF608174278203FE` | moved | unique city names |
| D_seat | `0880A346A2BD4DAE` | `E4147C059E9DD26C` | moved | unique city names |

Counters (identical BEFORE/AFTER: yes): `23.16/23.42/23.42  land: specialists 14 firms 117 (by body: 15124:117)  players 1`

### Seed 41

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `2A3B071C5C08D005` | `2A3B071C5C08D005` | same | — |
| D_land | `E67F9826AADD3E85` | `FC14BAF1D927B9E0` | moved | unique city names |
| D_settle | `173F6369BA584B3A` | `121A14E819AF8799` | moved | unique city names |
| D_seat | `A4A59679783C0187` | `77E63DA27999E7AC` | moved | unique city names |

Counters (identical BEFORE/AFTER: yes): `22.36/22.61/22.61  land: specialists 14 firms 113 (by body: 15124:113)  players 1`

### Seed 43

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `EC09BA30C3E775AB` | `EC09BA30C3E775AB` | same | — |
| D_land | `1DA82EB8A652A554` | `F91A54EDAD95B1C7` | moved | name cap + unique city names |
| D_settle | `5EAFA85515F604E3` | `BAED9605D586E974` | moved | name cap + unique city names |
| D_seat | `0DA27B2C26D67CC4` | `0CA255D712FBD337` | moved | name cap + unique city names |

Counters (identical BEFORE/AFTER: yes): `22.31/22.53/22.53  land: specialists 10 firms 124 (by body: 15124:124)  players 1`

### Seed 32

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `DAAF44506F4FCC39` | `DAAF44506F4FCC39` | same | — |
| D_land | `3A76C704C0EDF73F` | `46D0A70FDA6D8907` | moved | unique city names |
| D_settle | `FE77461823BDE7DE` | `3E73785E2C078D3E` | moved | unique city names |
| D_seat | `958A24C455C49A5B` | `8D0BB71A2F53FD13` | moved | unique city names |

Counters (identical BEFORE/AFTER: yes): `22.69/22.98/22.98  land: specialists 28 firms 118 (by body: 15124:118)  players 1`

### Seed 10

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `6AFC73F266139568` | `6AFC73F266139568` | same | — |
| D_land | `BCEAE517C7BD2582` | `E8225B53E92D1250` | moved | name cap + unique city names |
| D_settle | `3CF79CEBE6A00EF4` | `6BC992C91B7383DA` | moved | name cap + unique city names |
| D_seat | `FE1DE7C5F0699B08` | `974967682A95D206` | moved | name cap + unique city names |

Counters (identical BEFORE/AFTER: yes): `22.95/23.15/23.15  land: specialists 6 firms 103 (by body: 15124:103)  players 1`

### Seed 25

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `D1F59B38B9084262` | `D1F59B38B9084262` | same | — |
| D_land | `182ABFE3E44DE80D` | `764AE3B9E202C67F` | moved | unique city names |
| D_settle | `6D5C5679F33926E7` | `E9E05ADD330CB6E9` | moved | unique city names |
| D_seat | `51C737A3E5BF0B33` | `3644D1E11465A205` | moved | unique city names |

Counters (identical BEFORE/AFTER: yes): `23.40/23.64/23.64  land: specialists 6 firms 121 (by body: 15124:121)  players 1`

### Seed 38

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `4D68D855F5B02CDC` | `4D68D855F5B02CDC` | same | — |
| D_land | `7F2C45D861048609` | `8A56652901ADE2C7` | moved | unique city names |
| D_settle | `0F3631AAB34A6E91` | `300A1882097D56BB` | moved | unique city names |
| D_seat | `13F976F857728F20` | `01129328DFD605C2` | moved | unique city names |

Counters (identical BEFORE/AFTER: yes): `22.26/22.55/22.55  land: specialists 33 firms 133 (by body: 15124:133)  players 1`

### Seed 9

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `0C031EBD181246C9` | `0C031EBD181246C9` | same | — |
| D_land | `D4FD3E378CB2F602` | `6BEBD11007A3F864` | moved | unique city names |
| D_settle | `E149C76A753F850D` | `1621E032ABEAA923` | moved | unique city names |
| D_seat | `76D4535D6B028E6C` | `D06E3F9542C59FB4` | moved | unique city names |

Counters (identical BEFORE/AFTER: yes): `22.18/22.39/22.39  land: specialists 8 firms 120 (by body: 15124:120)  players 1`

### Seed 0

| digest | BEFORE (9f97bcfc) | AFTER (aeae0430) | verdict | moved by |
|---|---|---|---|---|
| D_search | `11013ABA30FEF609` | `11013ABA30FEF609` | same | — |
| D_land | `2C53454FB2C87942` | `7EEFE66C7778A204` | moved | unique city names |
| D_settle | `8488CFCA89B656D6` | `513E9FB09AB69602` | moved | unique city names |
| D_seat | `8FD4EE1C010C51A0` | `FF0E5FB1BF974E96` | moved | unique city names |

Counters (identical BEFORE/AFTER: yes): `22.76/22.99/22.99  land: specialists 8 firms 120 (by body: 15124:120)  players 1`
