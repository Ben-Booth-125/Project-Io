#include "settlement.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <unordered_map>
#include <utility>

namespace {

// ---------------------------------------------------------------------------
// Deterministic RNG — the same splitmix64 shape as planetology.cpp /
// history_ladder.cpp / creeds.cpp, duplicated per the standing convention that
// each generation file owns its stream.
// ---------------------------------------------------------------------------
uint64_t splitmix64(uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

struct rng
{
    uint64_t s;
    rng(uint32_t seed, uint32_t stage_tag)
        : s(splitmix64((static_cast<uint64_t>(seed) << 32) ^ (stage_tag * 0x9E3779B1u))) {}

    /// Integer in [0, n). Integer path only — every draw here participates in
    /// selection, and PLANETOLOGY.md bans a float argmax from a gate path.
    int pick(int n)
    {
        s = splitmix64(s);
        return n <= 0 ? 0 : static_cast<int>((s >> 33) % static_cast<uint64_t>(n));
    }
};

// FRESH stage tags — none collides with the ladder's (0x5A11 / 0xC4A7 /
// 0xF2A6), the creeds' (0xD317 / 0x1B47), the continents' (0xC017) or the
// planetology chain's.
constexpr uint32_t tag_settle   = 0x5E77u; // Province placement + founding dates.
constexpr uint32_t tag_furnace  = 0xF0F6u; // Industrialisation timing.
constexpr uint32_t tag_rupture  = 0x8017u; // The historical-rupture checkpoints.

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

int64_t clampi64(int64_t v, int64_t lo, int64_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

int wrapc(int col, int gw) { return ((col % gw) + gw) % gw; }

int raster(int col, int row, int gw) { return wrapc(col, gw) + row * gw; }

/// Chebyshev distance with column wrapping; rows are open edges.
int grid_dist(int c0, int r0, int c1, int r1, int gw)
{
    int dc = std::abs(c0 - c1);
    if (dc > gw / 2) dc = gw - dc;
    return std::max(dc, std::abs(r0 - r1));
}

const tile_component* tile_at(const world& w, const std::vector<entity_id>& ids, int idx)
{
    if (idx < 0 || idx >= static_cast<int>(ids.size())) return nullptr;
    const auto it = w.tiles.find(ids[static_cast<std::size_t>(idx)]);
    return it == w.tiles.end() ? nullptr : &it->second;
}

// ---------------------------------------------------------------------------
// Where people settle
// ---------------------------------------------------------------------------

/// How strongly one tile invites a permanent settlement, 0-100, INTEGER.
///
/// This is the ladder's `agrarian_score` grown up: it keeps the composition /
/// landform terms and adds the two the ladder had to name as missing. RIVER
/// CONNECTIVITY is no longer substituted — BL-170 landed, so `river_edges` is
/// read directly, which is why the wetland weight no longer has to carry the
/// floodplain signal on its own.
int settle_score(const tile_component& t, bool coastal)
{
    int score = 0;
    switch (t.composition)
    {
        case terrain_composition::grassland: score = 62; break;
        case terrain_composition::wetland:   score = 58; break; // Floodplain, now that rivers are their own term.
        case terrain_composition::forest:    score = 44; break;
        case terrain_composition::tundra:    score =  9; break;
        case terrain_composition::rocky:     score =  6; break;
        default:                             return 0;          // Ocean, ice, barren, regolith, metallic, volcanic.
    }

    switch (t.landform)
    {
        case terrain_landform::valley:   score += 18; break;
        case terrain_landform::plains:   score += 12; break;
        case terrain_landform::highland: score -=  8; break;
        case terrain_landform::canyon:   score -= 10; break;
        case terrain_landform::mountain: return 0;
        default: break;
    }

    if (t.river_edges != 0) score += 22; // A river is the cheapest road there is.
    if (coastal)            score += 14; // And a harbour is the cheapest of all.

    score += static_cast<int>(t.habitability * 18.0f);
    score -= static_cast<int>(t.hazard_level * 20.0f);
    return clampi(score, 0, 100);
}

bool touches_ocean(const world& w, const std::vector<entity_id>& ids,
                   int col, int row, int gw, int gh)
{
    const int dc[4] = { 0, 0, -1, 1 };
    const int dr[4] = { -1, 1, 0, 0 };
    for (int i = 0; i < 4; ++i)
    {
        const int nr = row + dr[i];
        if (nr < 0 || nr >= gh) continue;
        const tile_component* t = tile_at(w, ids, raster(col + dc[i], nr, gw));
        if (t && t->composition == terrain_composition::ocean) return true;
    }
    return false;
}

/// The ANCIENT endowment under a province — surveyed once, over the window the
/// province's people would have walked. These deposits predate everyone; what
/// changes across a campaign is who ends up standing on them.
///
/// Held as RAW per-tile-mean richness in thousandths, not as a 0-1000 score:
/// the four classes live on completely different absolute scales (a rich coal
/// window and a rich grain window are nowhere near the same number), so an
/// absolute gain either saturates one class or never fires another. The scores
/// are computed later, against the world's own means — see `score_endowments`.
struct endowment
{
    int farm = 0, ore = 0, energy = 0, water = 0; ///< Per-tile mean × 1000.
};

endowment survey_endowment(const world& w, const std::vector<entity_id>& ids,
                           int col, int row, int gw, int gh)
{
    const int win = std::max(3, gw / 45);
    float farm = 0.0f, ore = 0.0f, energy = 0.0f;
    int cells = 0, water = 0;

    for (int dr = -win; dr <= win; ++dr)
    {
        const int r = row + dr;
        if (r < 0 || r >= gh) continue;
        for (int dc = -win; dc <= win; ++dc)
        {
            const tile_component* t = tile_at(w, ids, raster(col + dc, r, gw));
            if (!t) continue;
            ++cells;
            if (t->composition == terrain_composition::ocean) { ++water; continue; }

            const auto& d = t->resource_deposit;
            farm   += d[static_cast<std::size_t>(resource_type::agricultural_produce)];
            ore    += d[static_cast<std::size_t>(resource_type::iron_ore)]
                    + d[static_cast<std::size_t>(resource_type::copper_ore)]
                    + d[static_cast<std::size_t>(resource_type::rare_earth_ore)];
            energy += d[static_cast<std::size_t>(resource_type::coal)]
                    + d[static_cast<std::size_t>(resource_type::petroleum)];
        }
    }

    const int n = std::max(1, cells);
    endowment e;
    // Integer throughout — these feed a class argmax, and PLANETOLOGY.md bans a
    // float argmax from a gate path.
    e.farm   = static_cast<int>(farm   * 1000.0f) / n;
    e.ore    = static_cast<int>(ore    * 1000.0f) / n;
    e.energy = static_cast<int>(energy * 1000.0f) / n;
    e.water  = (water * 1000) / n;
    return e;
}

/// Score one class against the world's own mean for that class: an average
/// province scores 500, twice the average scores 1000.
///
/// RELATIVE, NOT ABSOLUTE, and that is the load-bearing choice. "Ore country"
/// only means anything next to the rest of the map, and a world-relative score
/// also survives the `deposit_scalar` abundance tier (GENERATION_STRATEGY.md §
/// The resource ceiling) without re-tuning — a lean world still has its own
/// ore provinces, they are just poorer in absolute terms.
int score_against(int raw, int mean)
{
    return clampi((raw * 500) / std::max(1, mean), 0, 1000);
}

/// Argmax over the scored endowment, with an explicit fixed order so a tie is
/// resolved by the class list rather than by container order.
province_class classify(int farm_q, int ore_q, int energy_q, int port_q)
{
    int best = farm_q;
    province_class c = province_class::farm;
    if (ore_q    > best) { best = ore_q;    c = province_class::ore; }
    if (energy_q > best) { best = energy_q; c = province_class::energy; }

    // A harbour competes, but at a discount: a coastal province sitting on real
    // ore is an ore province that happens to have a harbour, not the reverse.
    // What lives on trade is the province with a coastline and nothing under it.
    if ((port_q * 4) / 5 > best) { best = (port_q * 4) / 5; c = province_class::port; }

    // Below the world's average on every axis, there is nothing to name.
    return best < 300 ? province_class::none : c;
}

const char* class_word(province_class c)
{
    switch (c)
    {
        case province_class::farm:   return "grain";
        case province_class::ore:    return "ore";
        case province_class::energy: return "coal and oil";
        case province_class::port:   return "harbour";
        default:                     return "little";
    }
}

/// Does this culture's pantheon hold a seat with the given domain? The creed
/// pass raises a FORGE god only where the cradle's window held ore, and a
/// SEALED OATH god only in the charter cradle — so asking this question is
/// asking what the land taught the people, one stage earlier.
bool creed_holds(const creed_state& cs, int culture, const char* domain)
{
    if (culture < 0 || culture >= static_cast<int>(cs.cultures.size())) return false;
    for (const culture_god& g : cs.cultures[static_cast<std::size_t>(culture)].pantheon)
        if (g.domain == domain) return true;
    return false;
}

const char* region_word(int col, int row, int gw, int gh)
{
    const int band = (row * 5) / std::max(1, gh);
    const int sect = (col * 4) / std::max(1, gw);
    static const char* bands[5] = { "northern", "upper", "middle", "lower", "southern" };
    static const char* sects[4] = { "dawnward", "inland", "duskward", "outer" };
    // One word from each axis; the pair is stable for a given anchor.
    return ((band + sect) & 1) ? bands[clampi(band, 0, 4)] : sects[clampi(sect, 0, 3)];
}

} // namespace

// ---------------------------------------------------------------------------
// The settlement pass
// ---------------------------------------------------------------------------

settlement_state run_settlement(const planetology_state& pl,
                                const history_ladder_state& hl,
                                const creed_state& cs,
                                const world& w,
                                const std::vector<entity_id>& tile_ids,
                                int gw, int gh,
                                int target_provinces,
                                uint32_t seed,
                                int64_t stop_year)
{
    settlement_state out;
    if (gw <= 0 || gh <= 0 || target_provinces <= 0 || hl.cradles.empty())
        return out;

    const int total = gw * gh;
    rng r(seed, tag_settle);

    // --- Score every tile once, in raster order --------------------------------
    std::vector<int> score(static_cast<std::size_t>(total), 0);
    for (int idx = 0; idx < total; ++idx)
    {
        const tile_component* t = tile_at(w, tile_ids, idx);
        if (!t || t->composition == terrain_composition::ocean) continue;
        const int col = idx % gw, row = idx / gw;
        score[static_cast<std::size_t>(idx)] =
            settle_score(*t, touches_ocean(w, tile_ids, col, row, gw, gh));
    }

    // --- Rank candidates: score descending, then raster index ascending --------
    // The index tie-break is the whole determinism story here; two tiles with
    // equal ground must not depend on sort stability alone.
    std::vector<int> order;
    order.reserve(static_cast<std::size_t>(total) / 4);
    for (int idx = 0; idx < total; ++idx)
        if (score[static_cast<std::size_t>(idx)] > 0) order.push_back(idx);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const int sa = score[static_cast<std::size_t>(a)];
        const int sb = score[static_cast<std::size_t>(b)];
        return sa != sb ? sa > sb : a < b;
    });

    // --- Place provinces greedily under a separation rule ----------------------
    std::vector<endowment> raw; // Parallel to out.provinces; scored below.
    const int sep = std::max(3, gw / 40);
    for (int idx : order)
    {
        if (static_cast<int>(out.provinces.size()) >= target_provinces) break;
        const int col = idx % gw, row = idx / gw;

        bool too_close = false;
        for (const province& p : out.provinces)
            if (grid_dist(col, row, p.col, p.row, gw) < sep) { too_close = true; break; }
        if (too_close) continue;

        province p;
        p.anchor = idx;
        p.col = col;
        p.row = row;
        p.settle_score_q = clampi(score[static_cast<std::size_t>(idx)] * 10, 0, 1000);

        // WHOSE GODS. A province inherits the nearest surviving cradle's
        // culture, so the pantheon map is a map of who actually walked where —
        // not a per-province re-roll of belief.
        int best_c = -1, best_d = 1 << 30;
        for (std::size_t ci = 0; ci < cs.cultures.size(); ++ci)
        {
            const int cr = cs.cultures[ci].cradle;
            if (cr < 0 || cr >= static_cast<int>(hl.cradles.size())) continue;
            const agrarian_cradle& ac = hl.cradles[static_cast<std::size_t>(cr)];
            const int d = grid_dist(col, row, ac.col, ac.row, gw);
            if (d < best_d) { best_d = d; best_c = static_cast<int>(ci); }
        }
        p.culture = best_c;
        p.founding_culture = best_c;

        raw.push_back(survey_endowment(w, tile_ids, col, row, gw, gh));

        // Better ground is settled earlier. Bounded on both ends so a scoring
        // change cannot push a founding date past the industrial stage.
        p.founded_year = -2000 + static_cast<int64_t>(1000 - p.settle_score_q) * 3
                       + r.pick(120);

        const std::string people = best_c >= 0
            ? cs.cultures[static_cast<std::size_t>(best_c)].name
            : std::string("nameless");
        p.name = people + " " + region_word(col, row, gw, gh);

        out.provinces.push_back(std::move(p));
    }

    if (out.provinces.empty())
        return out;

    // --- Score the endowments against the world's own means --------------------
    {
        int64_t sf = 0, so = 0, se = 0, sw = 0;
        for (const endowment& e : raw)
        {
            sf += e.farm; so += e.ore; se += e.energy; sw += e.water;
        }
        const int n = static_cast<int>(raw.size());
        const int mf = static_cast<int>(sf / n), mo = static_cast<int>(so / n);
        const int me = static_cast<int>(se / n), mw = static_cast<int>(sw / n);

        for (std::size_t i = 0; i < out.provinces.size(); ++i)
        {
            province& p = out.provinces[i];
            const endowment& e = raw[i];
            p.farm_q   = score_against(e.farm,   mf);
            p.ore_q    = score_against(e.ore,    mo);
            p.energy_q = score_against(e.energy, me);
            p.port_q   = score_against(e.water,  mw);
            p.dominant = classify(p.farm_q, p.ore_q, p.energy_q, p.port_q);
        }
    }

    // --- Antiquity stop (BL-271) ----------------------------------------------
    // Below the industrial era the pass generates the world AT `stop_year`:
    // provinces founded later do not exist yet (the year-tick sim founds them),
    // and the population that HAS arrived is seeded at founding and grown
    // logistically to the start year. RNG-safe: the founding loop above already
    // consumed its draws for every candidate, so the streams match the 1960 arc.
    const bool antiquity = stop_year < 1700;
    if (antiquity)
    {
        out.provinces.erase(
            std::remove_if(out.provinces.begin(), out.provinces.end(),
                           [stop_year](const province& p) { return p.founded_year > stop_year; }),
            out.provinces.end());

        for (province& p : out.provinces)
        {
            // Founding band ~2k-26k settlers, richer ground drawing more; the
            // logistic model then does the two millennia (or fewer) of growth.
            p.population = 2000 + static_cast<int64_t>(p.farm_q) * 24;
            p.last_demography_year = p.founded_year;
            advance_province_demography(
                p, static_cast<int>(stop_year - p.founded_year), /*war_pressure_q=*/0);
        }
    }

    // --- Stage 4: who lights the furnaces, and when ----------------------------
    // "Coal-near-cities made Britain — endowment, not virtue" (HISTORY.md Stage
    // 4). The gate is the ground; the creed only moves the date, and only where
    // the creed itself came from the same ground (a forge god is raised over
    // ore, one stage earlier). Never reached by antiquity worlds: no furnace
    // has lit by year 0, and that history belongs to the sim, not the pass.
    rng rf(seed, tag_furnace);
    const int arable_q = clampi(static_cast<int>(pl.arable_share * 1000.0f), 0, 1000);

    for (province& p : out.provinces)
    {
        if (antiquity) break;
        // The gate is ABOVE-AVERAGE fuel, not any fuel: the scores are relative
        // to the world's own means (500 = average), so an average province sits
        // at 750 here and does not industrialise. Only the endowed do.
        const int fuel = p.energy_q + p.ore_q / 2;
        if (fuel < 900) continue; // No fuel, no ceiling to break.

        int64_t year = 1790
                     - p.energy_q / 12
                     - p.ore_q / 22
                     - arable_q / 200          // A fed population industrialises sooner.
                     + rf.pick(45);

        if (creed_holds(cs, p.culture, "the forge"))       year -= 25;
        if (creed_holds(cs, p.culture, "the sealed oath")) year -= 15; // Stage 3: contract law reaches capital.
        if (p.founded_year > 0) year += p.founded_year / 8;            // Late settlement, late furnaces.

        p.industrial_year = clampi(static_cast<int>(year), 1700, 1935);
        p.industrialised = true;
    }

    // Median over the industrialised set — BL-219's early/late pivot.
    {
        std::vector<int64_t> years;
        for (const province& p : out.provinces)
            if (p.industrialised) years.push_back(p.industrial_year);
        if (!years.empty())
        {
            std::sort(years.begin(), years.end());
            out.median_industrial_year = years[years.size() / 2];
        }
    }

    // --- The founding lines ----------------------------------------------------
    // Bounded deliberately: a biography with seventy founding lines is a table,
    // not a history. The six best-placed provinces speak for the settlement
    // stage; the rest exist in the data and drive the map regardless.
    const int named = std::min<int>(6, static_cast<int>(out.provinces.size()));
    for (int i = 0; i < named; ++i)
    {
        const province& p = out.provinces[static_cast<std::size_t>(i)];
        out.history.push_back(history_event{
            years_from_calendar_year(p.founded_year), chain_stage::legacy,
            "The " + p.name + " is settled under the same gods as its cradle.",
            std::string("-> ") + class_word(p.dominant) + " country; the ground was there first" });
    }

    return out;
}

std::vector<int> settlement_seed_tiles(const settlement_state& ss)
{
    std::vector<int> seeds;
    seeds.reserve(ss.provinces.size());
    for (const province& p : ss.provinces)
        if (p.anchor >= 0) seeds.push_back(p.anchor);
    return seeds;
}

int nearest_province(const settlement_state& ss, int col, int row, int gw)
{
    int best = -1, best_d = 1 << 30;
    for (std::size_t i = 0; i < ss.provinces.size(); ++i)
    {
        const int d = grid_dist(col, row, ss.provinces[i].col, ss.provinces[i].row, gw);
        if (d < best_d) { best_d = d; best = static_cast<int>(i); }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Political character — three axes, three derivations, no draws
// ---------------------------------------------------------------------------

namespace {

/// Reverse index from nation entity id to its position in `nation_ids`.
std::unordered_map<entity_id, int> nation_index(const std::vector<entity_id>& nation_ids)
{
    std::unordered_map<entity_id, int> m;
    m.reserve(nation_ids.size() * 2);
    for (std::size_t i = 0; i < nation_ids.size(); ++i)
        m[nation_ids[i]] = static_cast<int>(i);
    return m;
}

/// Per-tile owning nation index, in raster order (-1 = unowned/ocean).
std::vector<int> owner_map_of(const world& w,
                              const std::vector<entity_id>& tile_ids,
                              const std::unordered_map<entity_id, int>& nidx,
                              int gw, int gh)
{
    const int total = gw * gh;
    std::vector<int> owner(static_cast<std::size_t>(total), -1);
    for (int idx = 0; idx < total; ++idx)
    {
        if (idx >= static_cast<int>(tile_ids.size())) break;
        const entity_id tid = tile_ids[static_cast<std::size_t>(idx)];
        if (tid == null_entity) continue;
        const auto it = w.tile_to_nation.find(tid);
        if (it == w.tile_to_nation.end()) continue;
        const auto ni = nidx.find(it->second);
        if (ni != nidx.end()) owner[static_cast<std::size_t>(idx)] = ni->second;
    }
    return owner;
}

economic_focus focus_of_class(province_class c)
{
    switch (c)
    {
        case province_class::energy: return economic_focus::processing;
        case province_class::port:   return economic_focus::trade;
        default:                     return economic_focus::extraction; // farm, ore, none
    }
}

} // namespace

void derive_national_character(settlement_state& ss,
                               const creed_state& cs,
                               world& w,
                               const std::vector<entity_id>& nation_ids,
                               const std::vector<entity_id>& tile_ids,
                               int gw, int gh)
{
    if (ss.provinces.empty() || nation_ids.empty() || gw <= 0 || gh <= 0)
        return;

    const auto nidx = nation_index(nation_ids);
    const std::vector<int> owner = owner_map_of(w, tile_ids, nidx, gw, gh);
    const int nations = static_cast<int>(nation_ids.size());
    const int total = gw * gh;

    // --- Attribute every province to whoever ended up holding it ---------------
    for (province& p : ss.provinces)
        p.nation = (p.anchor >= 0 && p.anchor < total)
                 ? owner[static_cast<std::size_t>(p.anchor)] : -1;

    // --- The border-contest integral -------------------------------------------
    // Two terms, because a frontier is two things: how much of your edge faces
    // somebody (the static share) and how much settled weight is pressed
    // against it (the historical pressure). A nation that expanded into empty
    // land scores near zero on both, which is BL-218's whole point.
    std::vector<int> border(static_cast<std::size_t>(nations), 0);
    std::vector<int> owned(static_cast<std::size_t>(nations), 0);
    for (int idx = 0; idx < total; ++idx)
    {
        const int ni = owner[static_cast<std::size_t>(idx)];
        if (ni < 0) continue;
        ++owned[static_cast<std::size_t>(ni)];

        const int col = idx % gw, row = idx / gw;
        const int dc[4] = { 0, 0, -1, 1 };
        const int dr[4] = { -1, 1, 0, 0 };
        for (int i = 0; i < 4; ++i)
        {
            const int nr = row + dr[i];
            if (nr < 0 || nr >= gh) continue;
            const int no = owner[static_cast<std::size_t>(raster(col + dc[i], nr, gw))];
            if (no >= 0 && no != ni) { ++border[static_cast<std::size_t>(ni)]; break; }
        }
    }

    std::vector<int> rival_pressure(static_cast<std::size_t>(nations), 0);
    std::vector<int> province_count(static_cast<std::size_t>(nations), 0);
    for (std::size_t i = 0; i < ss.provinces.size(); ++i)
    {
        const province& a = ss.provinces[i];
        if (a.nation < 0) continue;
        ++province_count[static_cast<std::size_t>(a.nation)];
        for (std::size_t j = 0; j < ss.provinces.size(); ++j)
        {
            if (i == j) continue;
            const province& b = ss.provinces[j];
            if (b.nation < 0 || b.nation == a.nation) continue;
            if (grid_dist(a.col, a.row, b.col, b.row, gw) <= 12)
                ++rival_pressure[static_cast<std::size_t>(a.nation)];
        }
    }

    std::vector<int> contest(static_cast<std::size_t>(nations), 0);
    for (int ni = 0; ni < nations; ++ni)
    {
        const int share = owned[static_cast<std::size_t>(ni)] > 0
            ? (border[static_cast<std::size_t>(ni)] * 1000) / owned[static_cast<std::size_t>(ni)]
            : 0;
        const int press = province_count[static_cast<std::size_t>(ni)] > 0
            ? (rival_pressure[static_cast<std::size_t>(ni)] * 150)
              / province_count[static_cast<std::size_t>(ni)]
            : 0;
        contest[static_cast<std::size_t>(ni)] = clampi(share * 2 + press, 0, 1000);
    }
    for (province& p : ss.provinces)
        if (p.nation >= 0) p.contest_q = contest[static_cast<std::size_t>(p.nation)];

    // --- Industrialisation timing, per nation ----------------------------------
    constexpr int64_t never = 1 << 30;
    std::vector<int64_t> first_furnace(static_cast<std::size_t>(nations), never);
    for (const province& p : ss.provinces)
    {
        if (p.nation < 0 || !p.industrialised) continue;
        int64_t& f = first_furnace[static_cast<std::size_t>(p.nation)];
        f = std::min(f, p.industrial_year);
    }

    // Terciles over the nations that industrialised at all — "relative to
    // neighbours" is a RANK, so it has to be computed against the field.
    std::vector<int64_t> ranked;
    for (int ni = 0; ni < nations; ++ni)
        if (first_furnace[static_cast<std::size_t>(ni)] != never)
            ranked.push_back(first_furnace[static_cast<std::size_t>(ni)]);
    std::sort(ranked.begin(), ranked.end());
    const int64_t early_cut = ranked.empty() ? 0 : ranked[ranked.size() / 3];
    const int64_t late_cut  = ranked.empty() ? 0 : ranked[(ranked.size() * 2) / 3];

    // --- Write the three axes ---------------------------------------------------
    for (int ni = 0; ni < nations; ++ni)
    {
        const auto it = w.nations.find(nation_ids[static_cast<std::size_t>(ni)]);
        if (it == w.nations.end()) continue;
        nation_component& nc = it->second;

        // expansionism <- the border-contest integral.
        const int cq = contest[static_cast<std::size_t>(ni)];
        nc.posture = cq >= 600 ? expansionism::aggressive
                   : cq >= 280 ? expansionism::moderate
                               : expansionism::passive;

        // economic_focus <- the dominant resource class of the provinces
        // settled DURING industrialisation, not of everything it holds.
        std::array<int, 3> tally = { 0, 0, 0 };
        for (const province& p : ss.provinces)
        {
            if (p.nation != ni) continue;
            if (!p.industrialised) continue;
            ++tally[static_cast<std::size_t>(focus_of_class(p.dominant))];
        }
        if (tally[0] + tally[1] + tally[2] == 0)
            for (const province& p : ss.provinces)
                if (p.nation == ni) ++tally[static_cast<std::size_t>(focus_of_class(p.dominant))];

        int best = 0;
        for (int f = 1; f < 3; ++f)
            if (tally[static_cast<std::size_t>(f)] > tally[static_cast<std::size_t>(best)]) best = f;
        nc.focus = static_cast<economic_focus>(best);

        const int64_t ff = first_furnace[static_cast<std::size_t>(ni)];

        // The movement up the value chain (BL-219 § 3, applied first at the
        // national level): a first mover had time to build refining on top of
        // its extraction base.
        if (ff != never && !ranked.empty() && ff <= early_cut
         && nc.focus == economic_focus::extraction)
            nc.focus = economic_focus::processing;

        // ideology <- industrialisation timing against neighbours.
        if (ff == never)                nc.politics = ideology::isolationist;
        else if (ff <= early_cut)       nc.politics = ideology::mercantile;
        else if (ff <= late_cut)        nc.politics = ideology::technocratic;
        else                            nc.politics = ideology::authoritarian;
    }

    // --- Stage 4's lines, now that they can name a nation ----------------------
    // The earliest three furnaces only: this is the seed's own Britain, and
    // naming it is the whole payload HISTORY.md Stage 4 asks for.
    std::vector<int> by_year;
    for (std::size_t i = 0; i < ss.provinces.size(); ++i)
        if (ss.provinces[i].industrialised && ss.provinces[i].nation >= 0)
            by_year.push_back(static_cast<int>(i));
    std::sort(by_year.begin(), by_year.end(), [&](int a, int b) {
        const province& pa = ss.provinces[static_cast<std::size_t>(a)];
        const province& pb = ss.provinces[static_cast<std::size_t>(b)];
        return pa.industrial_year != pb.industrial_year
             ? pa.industrial_year < pb.industrial_year : a < b;
    });

    const int lines = std::min<int>(3, static_cast<int>(by_year.size()));
    for (int i = 0; i < lines; ++i)
    {
        const province& p = ss.provinces[static_cast<std::size_t>(by_year[static_cast<std::size_t>(i)])];
        const auto it = w.nations.find(nation_ids[static_cast<std::size_t>(p.nation)]);
        const std::string nation = it != w.nations.end() ? it->second.name : std::string("a realm");
        const bool forge = creed_holds(cs, p.culture, "the forge");
        ss.history.push_back(history_event{
            years_from_calendar_year(p.industrial_year), chain_stage::spend,
            nation + " lights the first coke furnaces of the " + p.name + ".",
            forge ? "-> the forge god's country cashes its ore"
                  : "-> endowment, not virtue; the organic ceiling breaks here" });
    }
}

// ---------------------------------------------------------------------------
// The historical ruptures — transforms, and a record that can be destroyed
// ---------------------------------------------------------------------------

namespace {

/// Move one tile between nations, keeping `tile_to_nation` and both nations'
/// tile lists in step. Doing this in one place is the only reason the transforms
/// below can be read as history rather than as bookkeeping.
void transfer_tile(world& w, entity_id tid, entity_id from_nid, entity_id to_nid)
{
    if (tid == null_entity || from_nid == to_nid) return;
    w.tile_to_nation[tid] = to_nid;

    const auto f = w.nations.find(from_nid);
    if (f != w.nations.end())
    {
        auto& v = f->second.tiles;
        v.erase(std::remove(v.begin(), v.end(), tid), v.end());
    }
    const auto t = w.nations.find(to_nid);
    if (t != w.nations.end())
        t->second.tiles.push_back(tid);
}

void scale_abundance(world& w, entity_id nid, float factor)
{
    const auto it = w.nations.find(nid);
    if (it == w.nations.end()) return;
    for (std::size_t r = 0; r < resource_count; ++r)
        it->second.resource_abundance[r] *= factor;
}

ideology flip(ideology i)
{
    switch (i)
    {
        case ideology::mercantile:    return ideology::authoritarian;
        case ideology::authoritarian: return ideology::mercantile;
        case ideology::technocratic:  return ideology::isolationist;
        default:                      return ideology::technocratic;
    }
}

/// ERASE part of the record. Every line naming @p subject is destroyed and one
/// dated lacuna is left in its place — a hole the player can see, which is the
/// difference between a history that was fought over and a history that was
/// merely written.
void redact(settlement_state& ss, const std::string& subject,
            int64_t year, const std::string& by)
{
    const std::size_t before = ss.history.size();
    ss.history.erase(std::remove_if(ss.history.begin(), ss.history.end(),
        [&](const history_event& e) {
            return e.event.find(subject) != std::string::npos
                || e.consequence.find(subject) != std::string::npos;
        }), ss.history.end());

    const int lost = static_cast<int>(before - ss.history.size());
    if (lost <= 0) return;

    ss.lacunae += lost;
    ss.history.push_back(history_event{
        years_from_calendar_year(year), chain_stage::legacy,
        "What the " + subject + " wrote of itself does not survive " + by + ".",
        "-> " + std::to_string(lost) + " lines lost; the victors keep the record" });
}

} // namespace

void resolve_historical_ruptures(settlement_state& ss,
                                 const creed_state& cs,
                                 world& w,
                                 const std::vector<entity_id>& nation_ids,
                                 const std::vector<entity_id>& tile_ids,
                                 int gw, int gh,
                                 uint32_t seed)
{
    if (ss.provinces.empty() || nation_ids.size() < 2 || gw <= 0 || gh <= 0)
        return;

    const auto nidx = nation_index(nation_ids);
    const std::vector<int> owner = owner_map_of(w, tile_ids, nidx, gw, gh);
    const int nations = static_cast<int>(nation_ids.size());
    const int total = gw * gh;

    // Shared-border counts, so "war" can pick a real neighbour rather than a
    // nation on the other side of the world.
    std::vector<std::vector<int>> shared(
        static_cast<std::size_t>(nations), std::vector<int>(static_cast<std::size_t>(nations), 0));
    for (int idx = 0; idx < total; ++idx)
    {
        const int ni = owner[static_cast<std::size_t>(idx)];
        if (ni < 0) continue;
        const int col = idx % gw, row = idx / gw;
        const int dc[4] = { 0, 0, -1, 1 };
        const int dr[4] = { -1, 1, 0, 0 };
        for (int i = 0; i < 4; ++i)
        {
            const int nr = row + dr[i];
            if (nr < 0 || nr >= gh) continue;
            const int no = owner[static_cast<std::size_t>(raster(col + dc[i], nr, gw))];
            if (no >= 0 && no != ni) ++shared[static_cast<std::size_t>(ni)][static_cast<std::size_t>(no)];
        }
    }

    // The nations a rupture is even about: the most-contested handful. Bounded
    // so the count of ruptures is a property of the design, not of the map size.
    std::vector<int> candidates;
    for (int ni = 0; ni < nations; ++ni) candidates.push_back(ni);
    std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
        int ca = 0, cb = 0;
        for (const province& p : ss.provinces)
        {
            if (p.nation == a) ca = std::max(ca, p.contest_q);
            if (p.nation == b) cb = std::max(cb, p.contest_q);
        }
        return ca != cb ? ca > cb : a < b;
    });
    if (candidates.size() > 6) candidates.resize(6);

    rng pick_rng(seed, tag_rupture);

    for (std::size_t k = 0; k < candidates.size(); ++k)
    {
        const int ni = candidates[k];
        const entity_id nid = nation_ids[static_cast<std::size_t>(ni)];
        const auto nit = w.nations.find(nid);
        if (nit == w.nations.end() || nit->second.tiles.empty()) continue;

        // Province inventory for this nation, in placement order.
        std::vector<int> mine;
        for (std::size_t i = 0; i < ss.provinces.size(); ++i)
            if (ss.provinces[i].nation == ni) mine.push_back(static_cast<int>(i));
        if (mine.empty()) continue;

        // Strongest real neighbour, by shared border (tie: lowest index).
        int rival = -1, rival_border = 0;
        for (int nj = 0; nj < nations; ++nj)
        {
            const int b = shared[static_cast<std::size_t>(ni)][static_cast<std::size_t>(nj)];
            if (b > rival_border) { rival_border = b; rival = nj; }
        }

        const int contest = ss.provinces[static_cast<std::size_t>(mine.front())].contest_q;
        const int64_t year = 1901 + static_cast<int64_t>(k) * 6 + pick_rng.pick(9);

        bool applied = false;
        resolve_checkpoint(
            chain_stage::spend, seed, tag_rupture ^ (static_cast<uint32_t>(ni) * 0x9E3779B1u),
            /*max_attempts=*/4,
            // propose — a lean is an ELIGIBILITY FILTER, never a weight.
            [&](checkpoint_rng&, uint32_t) {
                std::vector<checkpoint_branch> b;
                b.push_back({ "collapse", mine.size() >= 3 });
                b.push_back({ "war", rival >= 0 && contest >= 280 });
                b.push_back({ "revolution", true });
                return b;
            },
            // apply — each branch is a transform on state; the line it appends
            // is a RECORD of the transform, never a substitute for it.
            [&](checkpoint_rng&, const checkpoint_branch& chosen) {
                if (applied) return; // A reroll must not stack transforms.
                const std::string label = chosen.label;
                const std::string self = nit->second.name;

                if (label == "revolution")
                {
                    nit->second.politics = flip(nit->second.politics);
                    scale_abundance(w, nid, 0.92f);
                    ss.history.push_back(history_event{
                        years_from_calendar_year(year), chain_stage::spend,
                        "The " + ss.provinces[static_cast<std::size_t>(mine.front())].name +
                            " rises; " + self + " is remade from the inside.",
                        "-> ideology flips; territory untouched" });
                    applied = true;
                    return;
                }

                if (label == "collapse")
                {
                    // Contract toward the core: the two most peripheral
                    // provinces are lost, and their industrial clock resets.
                    std::vector<int> ordered = mine;
                    const province& core = ss.provinces[static_cast<std::size_t>(mine.front())];
                    std::sort(ordered.begin(), ordered.end(), [&](int a, int b) {
                        const province& pa = ss.provinces[static_cast<std::size_t>(a)];
                        const province& pb = ss.provinces[static_cast<std::size_t>(b)];
                        const int da = grid_dist(pa.col, pa.row, core.col, core.row, gw);
                        const int db = grid_dist(pb.col, pb.row, core.col, core.row, gw);
                        return da != db ? da > db : a < b;
                    });

                    int lost = 0;
                    for (int pi : ordered)
                    {
                        if (lost >= 2) break;
                        province& p = ss.provinces[static_cast<std::size_t>(pi)];
                        if (pi == mine.front()) continue;

                        // Hand the province's anchor window to the nearest
                        // OTHER nation that already borders it.
                        int taker = -1;
                        for (int nj = 0; nj < nations && taker < 0; ++nj)
                            if (nj != ni && shared[static_cast<std::size_t>(ni)][static_cast<std::size_t>(nj)] > 0)
                                taker = nj;
                        if (taker < 0) break;

                        const entity_id tnid = nation_ids[static_cast<std::size_t>(taker)];
                        int moved = 0;
                        for (int dr2 = -3; dr2 <= 3 && moved < 24; ++dr2)
                            for (int dc2 = -3; dc2 <= 3 && moved < 24; ++dc2)
                            {
                                const int rr = p.row + dr2;
                                if (rr < 0 || rr >= gh) continue;
                                const int idx = raster(p.col + dc2, rr, gw);
                                if (owner[static_cast<std::size_t>(idx)] != ni) continue;
                                const entity_id tid = tile_ids[static_cast<std::size_t>(idx)];
                                transfer_tile(w, tid, nid, tnid);
                                ++moved;
                            }
                        p.nation = taker;
                        p.industrial_year = p.industrialised
                            ? std::min<int64_t>(1935, p.industrial_year + 40) : 0;
                        ++lost;
                    }

                    scale_abundance(w, nid, 0.80f);
                    ss.history.push_back(history_event{
                        years_from_calendar_year(year), chain_stage::spend,
                        self + " cannot hold its periphery; " + std::to_string(lost) +
                            " provinces answer to someone else by winter.",
                        "-> territory contracts; the industrial clock resets where it went" });
                    applied = true;
                    return;
                }

                // --- war -------------------------------------------------------
                const entity_id rnid = nation_ids[static_cast<std::size_t>(rival)];
                const auto rit = w.nations.find(rnid);
                if (rit == w.nations.end()) { applied = true; return; }

                // The stronger is the one with more to fight with: territory
                // plus how much of it is industrial.
                int my_industry = 0, their_industry = 0;
                for (const province& p : ss.provinces)
                {
                    if (!p.industrialised) continue;
                    if (p.nation == ni) ++my_industry;
                    if (p.nation == rival) ++their_industry;
                }
                const int mine_w  = static_cast<int>(nit->second.tiles.size()) + my_industry * 40;
                const int their_w = static_cast<int>(rit->second.tiles.size()) + their_industry * 40;

                const int win_ni = mine_w >= their_w ? ni : rival;
                const int los_ni = mine_w >= their_w ? rival : ni;
                const entity_id win_nid = nation_ids[static_cast<std::size_t>(win_ni)];
                const entity_id los_nid = nation_ids[static_cast<std::size_t>(los_ni)];
                auto& winner = w.nations[win_nid];
                auto& loser  = w.nations[los_nid];

                const std::string win_name = winner.name;
                const std::string los_name = loser.name;

                // Redraw the contested border toward the stronger — bounded to
                // a quarter of the loser's territory, because BL-224's
                // non-hegemony invariant is respected here, not spent.
                const int budget = std::max(1, static_cast<int>(loser.tiles.size()) / 4);
                int moved = 0;
                for (int idx = 0; idx < total && moved < budget; ++idx)
                {
                    if (owner[static_cast<std::size_t>(idx)] != los_ni) continue;
                    const int col = idx % gw, row = idx / gw;
                    bool front = false;
                    const int dc[4] = { 0, 0, -1, 1 };
                    const int dr[4] = { -1, 1, 0, 0 };
                    for (int i = 0; i < 4 && !front; ++i)
                    {
                        const int nr = row + dr[i];
                        if (nr < 0 || nr >= gh) continue;
                        if (owner[static_cast<std::size_t>(raster(col + dc[i], nr, gw))] == win_ni)
                            front = true;
                    }
                    if (!front) continue;
                    transfer_tile(w, tile_ids[static_cast<std::size_t>(idx)], los_nid, win_nid);
                    ++moved;
                }

                // Grievance is the point of the axis: the loser comes out of it
                // more expansionist, not less.
                loser.posture = expansionism::aggressive;
                if (winner.posture == expansionism::passive)
                    winner.posture = expansionism::moderate;
                scale_abundance(w, los_nid, 0.75f);
                scale_abundance(w, win_nid, 0.90f);

                // THE VICTOR'S GODS TRAVEL WITH THE BORDER. A province taken
                // keeps its founders in `founding_culture` and its conquerors
                // in `culture` — which is exactly the pair a later religion or
                // population layer needs to describe a grievance.
                int converted = 0;
                int victor_culture = -1;
                for (const province& p : ss.provinces)
                    if (p.nation == win_ni) { victor_culture = p.culture; break; }

                for (province& p : ss.provinces)
                {
                    if (p.nation != los_ni) continue;
                    if (p.anchor < 0) continue;
                    bool near_front = false;
                    for (const province& q : ss.provinces)
                        if (q.nation == win_ni
                         && grid_dist(p.col, p.row, q.col, q.row, gw) <= 10) { near_front = true; break; }
                    if (!near_front) continue;

                    p.nation = win_ni;
                    if (victor_culture >= 0 && victor_culture != p.culture)
                    {
                        p.culture = victor_culture;
                        p.creed_conquered = true;
                        ++converted;
                    }
                }

                ss.history.push_back(history_event{
                    years_from_calendar_year(year), chain_stage::spend,
                    win_name + " and " + los_name + " fight over the same frontier; " +
                        win_name + " keeps it.",
                    "-> " + std::to_string(moved) + " tiles change hands; " +
                        std::to_string(converted) + " provinces change gods" });

                // And now the erasure. The loser's own account of itself is
                // what a sack destroys first.
                if (!mine.empty())
                {
                    for (const province& p : ss.provinces)
                    {
                        if (p.nation != win_ni || !p.creed_conquered) continue;
                        redact(ss, p.name, year, "the " + win_name + " occupation");
                        break;
                    }
                }

                // The victor's pantheon is now the frontier's pantheon — name
                // it, since the gods are the only part of the old tongues that
                // survived globalisation (CREEDS.md).
                if (converted > 0 && victor_culture >= 0
                 && victor_culture < static_cast<int>(cs.cultures.size())
                 && !cs.cultures[static_cast<std::size_t>(victor_culture)].pantheon.empty())
                {
                    ss.history.push_back(history_event{
                        years_from_calendar_year(year + 1), chain_stage::spend,
                        "The shrines of the taken provinces are rededicated to " +
                            cs.cultures[static_cast<std::size_t>(victor_culture)].pantheon[0].name + ".",
                        "-> the founders' names survive only in the land registry" });
                }
                applied = true;
            },
            // floor_ok — the viability predicate, evaluated after apply. A
            // rupture that leaves a nation with nothing is not a rupture, it is
            // a deletion, and the transforms above are bounded so this holds.
            [&]() {
                const auto it2 = w.nations.find(nid);
                return it2 != w.nations.end() && !it2->second.tiles.empty();
            },
            ss.checkpoints);
    }

    // Chronological, so the biography reads forward regardless of which
    // transform appended what.
    std::stable_sort(ss.history.begin(), ss.history.end(),
        [](const history_event& a, const history_event& b)
        { return a.years_before_epoch > b.years_before_epoch; });
}

// ---------------------------------------------------------------------------
// BL-219's read — a corporation operates at the tier its province earned
// ---------------------------------------------------------------------------

industrial_focus focus_from_province(const province& p, int64_t median)
{
    // The ancient endowment sets the base tier.
    industrial_focus f = industrial_focus::extraction;
    switch (p.dominant)
    {
        case province_class::energy: f = industrial_focus::processing; break;
        case province_class::port:   f = industrial_focus::trade;      break;
        default:                     f = industrial_focus::extraction; break;
    }

    // MOVEMENT UP THE CHAIN (BL-219 § 3): an early industrialiser had time to
    // build refining on top of its extraction base and trade on top of that. A
    // late one — or one that never lit a furnace — stayed raw.
    if (p.industrialised && median > 0 && p.industrial_year <= median)
    {
        if (f == industrial_focus::extraction) f = industrial_focus::processing;
        else if (f == industrial_focus::processing) f = industrial_focus::trade;
    }
    return f;
}

// ---------------------------------------------------------------------------
// Demography (BL-273) — population growth, war/plague drawdown, manpower.
// ---------------------------------------------------------------------------

namespace {

// Fixed-point rate table, all in thousandths (1000 = 1.0). Named constants
// rather than inline magic numbers so the tuning surface is one place.
constexpr int64_t demog_capacity_floor    = 5000;  // Headcount at farm_q = 0.
constexpr int64_t demog_capacity_per_q    = 500;   // Extra headcount per farm_q point.
constexpr int64_t demog_growth_rate_q     = 12;     // ~1.2%/yr logistic rate, low-density.
constexpr int64_t demog_war_drawdown_q    = 40;     // Up to ~4%/yr loss at war_pressure_q = 1000.
constexpr int64_t demog_manpower_frac_q   = 50;     // Ceiling: 5% of population under arms at once.
constexpr int64_t demog_manpower_recover_q = 250;   // Stock closes 25% of the ceiling gap per step.
constexpr int64_t demog_plague_core_q     = 250;    // Epicentre loses up to 25% of its population.
constexpr int64_t demog_plague_falloff_q  = 60;     // Per grid step of distance, the loss tapers.
constexpr int      demog_plague_radius    = 6;      // Neighbours beyond this feel nothing.

} // namespace

int64_t province_carrying_capacity(int farm_q)
{
    const int q = clampi(farm_q, 0, 1000);
    return demog_capacity_floor + static_cast<int64_t>(q) * demog_capacity_per_q;
}

int64_t manpower_ceiling(int64_t population)
{
    if (population <= 0) return 0;
    return (population * demog_manpower_frac_q) / 1000;
}

void replenish_manpower(province& p)
{
    const int64_t ceiling = manpower_ceiling(p.population);
    if (p.manpower_stock > ceiling) { p.manpower_stock = ceiling; return; } // Losses shrink the ceiling too.
    const int64_t gap = ceiling - p.manpower_stock;
    p.manpower_stock += (gap * demog_manpower_recover_q) / 1000;
    p.manpower_stock = clampi64(p.manpower_stock, 0, ceiling);
}

int64_t raise_manpower(province& p, int64_t want)
{
    if (want <= 0 || p.manpower_stock <= 0) return 0;
    const int64_t raised = std::min(want, p.manpower_stock);
    p.manpower_stock -= raised; // Bounded by construction: never negative, never over-drawn.
    return raised;
}

void advance_province_demography(province& p, int years, int war_pressure_q)
{
    if (years <= 0) return;
    const int wq = clampi(war_pressure_q, 0, 1000);
    const int64_t K = province_carrying_capacity(p.farm_q);

    for (int y = 0; y < years; ++y)
    {
        if (p.population <= 0) { p.population = 0; continue; } // Nothing to grow from.

        const int64_t P = p.population;
        // Logistic growth term: dP = r * P * (K - P) / K, all integer.
        const int64_t delta = (demog_growth_rate_q * P * (K - P)) / (K * 1000);
        int64_t next = P + delta;

        if (wq > 0)
        {
            const int64_t loss = (next * static_cast<int64_t>(wq) * demog_war_drawdown_q) / (1000 * 1000);
            next -= loss;
        }

        p.population = clampi64(next, 0, K);
    }

    p.last_demography_year += years;
    replenish_manpower(p);
}

bool resolve_plague_event(std::vector<province>& provinces,
                          std::vector<checkpoint_record>& checkpoints,
                          uint32_t seed, uint32_t stage_tag, int gw)
{
    if (provinces.empty()) return false;

    bool applied = false;
    return resolve_checkpoint(
        chain_stage::spend, seed, stage_tag,
        /*max_attempts=*/4,
        // propose — one branch per province; eligibility is a FILTER
        // (BL-217's rule), never a weight: a province with nobody left
        // cannot be an epicentre.
        [&](checkpoint_rng&, uint32_t) {
            // Each label is the address of that province's OWN name buffer
            // (a std::string's storage is per-object even when empty via
            // SSO), so identity comparison in apply() below is exact and
            // survives duplicate name text.
            std::vector<checkpoint_branch> b;
            b.reserve(provinces.size());
            for (const province& p : provinces)
                b.push_back({ p.name.c_str(), p.population > 0 });
            return b;
        },
        // apply — the chosen province is the epicentre; nearby provinces
        // (grid-proximity connectivity proxy, see the header comment on why
        // not the full trade graph) lose population too, tapering with
        // distance.
        [&](checkpoint_rng&, const checkpoint_branch& chosen) {
            if (applied) return; // A reroll must not stack losses.

            // Identity, not content, comparison: `chosen.label` is the address
            // of a specific province's own `name.c_str()` (set in propose,
            // same call), so a pointer match is exact even when two
            // provinces share a name string (region-word collisions do
            // happen — see settle-name generation in run_settlement).
            int epicentre = -1;
            for (std::size_t i = 0; i < provinces.size(); ++i)
                if (provinces[i].name.c_str() == chosen.label) { epicentre = static_cast<int>(i); break; }
            if (epicentre < 0) return;

            const province& origin = provinces[static_cast<std::size_t>(epicentre)];
            for (province& p : provinces)
            {
                if (p.population <= 0) continue;
                const int dist = grid_dist(p.col, p.row, origin.col, origin.row, gw);
                if (dist > demog_plague_radius) continue;

                const int64_t falloff = static_cast<int64_t>(dist) * demog_plague_falloff_q;
                const int64_t loss_q = clampi64(demog_plague_core_q - falloff, 0, 1000);
                if (loss_q <= 0) continue;

                const int64_t loss = (p.population * loss_q) / 1000;
                p.population = clampi64(p.population - loss, 0, p.population);
                replenish_manpower(p); // The ceiling just fell with the population.
            }
            applied = true;
        },
        // floor_ok — a plague never has to leave anyone alive to be viable;
        // it is not a nation-level checkpoint, so there is no floor beyond
        // the bounded, non-negative arithmetic above already guaranteeing.
        [&]() { return true; },
        checkpoints);
}
