# Ledger mock-up data (Power BI seed)

Real economy data exported from the deterministic seeded world after **16 quarterly economy
ticks** (~4 game-years on the home body, Kepler), for designing the ledger surfaces in Power BI.
Regenerate any time with:

```
ProjectIo --verify scripts/verify/export_mockdata.lua
```

The world is deterministic, so a re-run reproduces the same numbers (bump the `econ_step(N)` in
that script for a longer horizon). The export binding is `verify.export_data(dir)` in
`src/core/app.cpp`.

## Files

| File | Grain | Columns |
|---|---|---|
| `corporations.csv` | one row per corporation | `corp_id, name, focus, home_nation, balance, starting_capital, since_start, buildings` |
| `cashflow.csv` | one row per corp (last tick) | `corp_id, corp_name, income, expenditure, maintenance, wages, interest, net` |
| `markets.csv` | one row per (market, resource) — snapshot | `market_id, market_label, body_id, body_name, resource, supply, demand, price, base_price` |
| `market_prices.csv` | one row per (market, resource, tick) — **time series** | `market_id, market_label, body_name, resource, tick, price, supply, demand` |
| `stockpiles.csv` | one row per (corp, body, resource) held | `corp_id, corp_name, body_id, body_name, resource, quantity` |
| `workforce.csv` | one row per (corp, body) | `corp_id, corp_name, body_id, body_name, staffing_pct` |
| `buildings.csv` | one row per building (last tick) | `building_id, corp_id, corp_name, body_name, type, output, active, exhausted` |
| `player_timeseries.csv` | one row per econ tick | `tick, balance, income, expenditure` |

`corp_id` / `body_id` / `building_id` / `market_id` are stable join keys across the tables (a Power
BI star schema: `corporations` is the corp dimension; `cashflow`, `stockpiles`, `workforce`,
`buildings` are facts keyed by `corp_id`; `markets` / `market_prices` are keyed by `market_id`).
`player_timeseries` (player balance) and `market_prices` (per-market, per-resource price/supply/demand
per tick) are the trend tables for line charts — `market_prices` is what feeds the market-ledger
"price over time" small multiples.

**`market_label` is the generated city name.** Each market resolves to the procedural name of the
population centre anchoring its `centre_tile` (`world::population_centre_name`, assigned by
`generate_population_centres` from an independent seeded stream — see the city-naming feature). The
market ledger's second selector and this column share `ui::market_city_name`. Kepler carries several
markets, one per major population centre, so the body → market cascade has real, named options.

*Deliberately not stating the count or the names here (corrected 2026-08-04): the fixture is
re-blessed whenever generation moves, and the previously-pasted "5 markets … Kynrdton, NuneKrenton,
Thear City" had gone stale on both. Read `markets.csv` — it now carries a `market_id` column, so the
distinct markets are countable directly.*

## Caveats (design inputs, not data bugs)

- **`stockpiles.csv` is sparse (~1–2 rows).** In the current economy, extraction sells to market
  immediately and processors don't accumulate, so `corp_body_pools` drain to near-empty within a
  tick or two — even the seeded starting stockpiles (BL-116) are gone by the visible-game start.
  This is the real behaviour behind **BL-078 PRODUCT_MARKET_INERT** / **BL-079
  EXTRACTION_BOOMBUST_NO_FEEDBACK**. Design the stockpile ledger for a *usually near-empty* pool,
  or add synthetic rows in Power BI if you want to mock a fuller inventory. (To capture the fuller
  *seeded* state, run `export_data` before any `econ_step`.)
- **Prices are near their cold-start band.** 16 ticks is early; product prices haven't hit the
  BL-078 cap yet. Bump `econ_step` in the script for later-game price data.
- **`interest` is 0 while solvent** (BL-073 charges it only in debt); most corps run positive here.

---

## Next-session prompt (paste to seed the ledger-mockup session)

> I'm designing the Project Io ledger surfaces. I've mocked up ledgers in Power BI using the real
> game data in `docs/ui/mockdata/` (corporations, cashflow, markets, stockpiles, workforce,
> buildings, and a player balance time series — see that folder's README for the schema). The
> mockups are attached as images. For each ledger, implement the design against the live world in
> the fold-out shell column (BL-122): the ledgers live in `src/ui/*.cpp`, hosted via
> `ui::foldout_begin`/`foldout_end`, split into one-question views with `ui::nav_button` where a
> panel asks more than one question. Remember: the visual design is mine (the mockups), the coding
> is yours — build to the images, and where a proportion is genuinely my call and I haven't shown
> it, ask or leave a design-owed note (as with BL-123 SELECTION_ELEMENT_RESIZE). Start by reading
> the mockups and telling me, per ledger, what maps cleanly to existing data and what needs new
> data plumbing.

Related open items to fold in as they fit: **BL-123 SELECTION_ELEMENT_RESIZE** (needs your Selection
mock-up), **BL-097 VIEW_BOUNDING_AUDIT** (resolution-robust bounds), and the still-floating **Tile
Ledger** migration (BL-119 deferred).
