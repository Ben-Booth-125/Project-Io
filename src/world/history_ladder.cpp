#include "history_ladder.hpp"

#include "nation_generation.hpp"
#include "terrain_combat.hpp"

#include <algorithm>

namespace {

// ---------------------------------------------------------------------------
// Deterministic RNG — same splitmix64 shape as planetology.cpp / continents.cpp,
// duplicated rather than shared, per the existing convention that each
// generation file owns its stream.
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

    float unit()
    {
        s = splitmix64(s);
        return static_cast<float>((s >> 40) & 0xFFFFFFull) * (1.0f / 16777216.0f);
    }
};

// FRESH stage tags. PLANETOLOGY.md § Determinism warns that 0x4A71012u is
// already folded twice in hard_coded_world.cpp; none of these collides with
// tag_continents (0xC017), tag_accretion (0x51A1), tag_breath (0xB6EA) or
// tag_green (0x67EE).
constexpr uint32_t tag_surplus   = 0x5A11u; // Stage 0 — agrarian surplus.
constexpr uint32_t tag_charter   = 0xC4A7u; // Stage 1 — the enforceable promise.
constexpr uint32_t tag_frontiers = 0xF2A6u; // Stage 2 — fragmentation.

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

/// How well one tile feeds a settled population, 0-100, integer.
///
/// INTEGER BY DESIGN: this score picks cradle sites by argmax, and a float
/// argmax over terrain is exactly the portability/tie-break hazard
/// PLANETOLOGY.md bans from a gate path.
///
/// Two terms BL-221 asks for are missing because their items are unbuilt, and
/// they are named here rather than silently approximated:
///   * RIVER CONNECTIVITY (BL-170, designed, no code) would add a term here —
///     floodplain access is the single strongest real-world cradle predictor,
///     and its absence is why `wetland` carries as much weight as it does.
///   * DOMESTICABLE CLADES (BL-217, designed, no code) would scale the whole
///     score by what there actually was to farm. The body-level `endemics`
///     count stands in for it, applied by the caller rather than per tile.
int agrarian_score(const tile_component& t)
{
    int score = 0;
    switch (t.composition)
    {
        case terrain_composition::grassland: score = 70; break; // The staple cradle.
        case terrain_composition::wetland:   score = 80; break; // Floodplain — see the BL-170 note.
        case terrain_composition::forest:    score = 45; break; // Clearable, but costly.
        case terrain_composition::tundra:    score = 10; break;
        case terrain_composition::rocky:     score =  5; break;
        default:                             return 0;          // Ocean, ice, barren, regolith, metallic, volcanic.
    }

    switch (t.landform)
    {
        case terrain_landform::valley:   score += 20; break; // Sheltered and watered.
        case terrain_landform::plains:   score += 12; break;
        case terrain_landform::highland: score -=  8; break;
        case terrain_landform::canyon:   score -= 10; break;
        case terrain_landform::mountain: return 0;           // Nobody farms a peak.
        default: break;
    }

    // Habitability is already a generated 0-1 scalar; fold it in as integer
    // percent so the whole score stays exact.
    score += clampi(static_cast<int>(t.habitability * 30.0f), 0, 30);
    return clampi(score, 0, 100);
}

/// How heavily open water counts toward the barrier field, per 1000 of ocean
/// share (BL-233; Ben's call, 2026-07-31).
///
/// Water is a movement MODE, not a magnitude, so `terrain_resistance` returns 0
/// for it and it cannot ride in on the graded land term. But it plainly does
/// resist an army, so it enters as its own term at its own weight — tunable
/// independently of terrain, which is the whole point of separating them.
///
/// The predecessor `is_barrier` gave water weight 1000: one ocean tile counted
/// exactly as much as one mountain. That is what the measurement pass found to
/// be wrong — on Kepler, 588 of 730 "barrier" tiles were simply ocean, so 81%
/// of a field meant to say "can an army get there?" was really saying "how wet
/// is this planet?".
///
/// CHOSEN BY SWEEP, not by taste. Kepler's nation count across the knob, against
/// a committed `is_barrier` baseline of 30 nations / 37 realms:
///
///     weight     0    250    500    750   1000
///     nations   17     17     17     30     36
///     realms    23     23     26     33     50
///
/// 750 is the value that holds the political map Ben set while the land term
/// still does its job — the grading defect (mountain and barren plain scoring
/// alike, forest scoring nothing) is fixed without moving the world shape that
/// downstream generation and every existing golden already assume. Lower values
/// are defensible on principle and were offered; this is the deliberate choice
/// of continuity, recorded so a later re-tune knows it was a choice.
constexpr int k_ocean_barrier_weight = 750;

/// A deterministic descriptive name for a grid position. Cradles predate
/// nations, so there is no political name to borrow — and inventing a proper
/// noun here would be worldbuilding that changes no decision (PLANETOLOGY.md's
/// diagnostic test), so this stays purely descriptive.
std::string region_name(int col, int row, int gw, int gh)
{
    const char* band =
        (row < gh / 3)         ? "northern" :
        (row < (2 * gh) / 3)   ? "equatorial" : "southern";
    const char* side =
        (col < gw / 3)         ? "western" :
        (col < (2 * gw) / 3)   ? "central" : "eastern";
    // No trailing noun: the caller supplies the landform word, so this must
    // read as an adjective phrase ("the northern western floodplain"), not as
    // one itself.
    return std::string("the ") + band + " " + side;
}

const tile_component* tile_at(const world& w, const std::vector<entity_id>& ids, std::size_t idx)
{
    if (idx >= ids.size()) return nullptr;
    const auto it = w.tiles.find(ids[idx]);
    return it == w.tiles.end() ? nullptr : &it->second;
}

} // namespace

// ---------------------------------------------------------------------------
// Stage 0 — Agrarian surplus, and Stage 2's terrain half.
// ---------------------------------------------------------------------------

history_ladder_state run_history_ladder(const planetology_state& pl,
                                        const world& w,
                                        const std::vector<entity_id>& tile_ids,
                                        int gw, int gh, uint32_t seed)
{
    history_ladder_state out;

    if (gw <= 0 || gh <= 0 || tile_ids.empty())
        return out;

    // A world that never reached a land biosphere grows nothing and settles
    // nobody. The brevity of that record IS its characterisation — do not pad
    // it (PLANETOLOGY.md § Presentation).
    if (pl.peak < life_stage::land)
        return out;

    const std::size_t n = static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh);

    // --- Pass 1: score every tile, and measure the barrier field ------------
    // The barrier field is measured in TWO independent quantities (BL-233),
    // because the ground and the water resist an army for different reasons and
    // at different strengths:
    //   - resistance_sum — graded terrain_resistance summed over LAND tiles, so
    //     a mountain and a barren plain no longer score identically and a forest
    //     no longer scores nothing.
    //   - ocean_tiles — counted separately, because water is a movement mode
    //     rather than a magnitude (terrain_combat.hpp § WHY TWO FUNCTIONS).
    std::vector<int> score(n, 0);
    int land_tiles = 0, ocean_tiles = 0, fertile_tiles = 0;
    long long resistance_sum = 0;

    for (std::size_t i = 0; i < n; ++i)
    {
        const tile_component* t = tile_at(w, tile_ids, i);
        if (!t) continue;

        if (t->composition == terrain_composition::ocean)
        {
            ++ocean_tiles;
        }
        else
        {
            ++land_tiles;
            resistance_sum += terrain_resistance(t->composition, t->landform);
        }

        score[i] = agrarian_score(*t);
        if (score[i] >= 60) ++fertile_tiles;
    }

    if (land_tiles == 0)
        return out;

    // --- Pass 2: cradles, by greedy argmax with a separation floor ----------
    // Walked in raster order and broken by tile index, so the result cannot
    // depend on iteration order or float comparison.
    //
    // The separation floor is what makes a cradle INDEPENDENT rather than a
    // neighbourhood of one: two granary belts a few tiles apart are one people.
    const int separation = std::max(6, gw / 12);

    // How many cradles the land could support at all. The biosphere's endemic
    // count stands in for BL-217's domesticable clades (see agrarian_score):
    // a richer biosphere offers more to domesticate, so more independent
    // origins are plausible.
    const int endemic_bonus = clampi(static_cast<int>(pl.endemics.size()), 0, 4);
    const int cradle_cap = clampi(fertile_tiles / 220 + endemic_bonus, 0, 12);

    std::vector<char> taken(n, 0);
    for (int picked = 0; picked < cradle_cap; ++picked)
    {
        int best = -1, best_score = 0;
        for (std::size_t i = 0; i < n; ++i)
        {
            if (taken[i] || score[i] <= best_score) continue; // `<=` keeps the LOWEST index on a tie.
            best = static_cast<int>(i);
            best_score = score[i];
        }
        if (best < 0 || best_score < 60) break; // Nothing left worth calling a cradle.

        const int bc = best % gw, br = best / gw;

        // Basin extent + coastal access, over the neighbourhood the cradle feeds.
        int basin = 0, coastal = 0;
        for (int dr = -separation; dr <= separation; ++dr)
        {
            const int r = br + dr;
            if (r < 0 || r >= gh) continue;
            for (int dc = -separation; dc <= separation; ++dc)
            {
                const int c = ((bc + dc) % gw + gw) % gw; // Columns wrap.
                const std::size_t j = static_cast<std::size_t>(c + r * gw);
                taken[j] = 1;                              // Claim the neighbourhood.
                if (score[j] >= 60) ++basin;
                const tile_component* t = tile_at(w, tile_ids, j);
                if (t && t->composition == terrain_composition::ocean) coastal = 1;
            }
        }

        out.cradles.push_back(agrarian_cradle{
            bc, br, basin, coastal, region_name(bc, br, gw, gh) });
    }

    // --- Pass 3: fragmentation, conquest cost, exit cost --------------------
    // Two independent terms, deliberately: barrier terrain (can an army get
    // there?) and cradle count (are there separate peoples to weld?). A smooth
    // world with many cradles and a broken world with one should NOT score the
    // same, and one term alone cannot tell them apart.
    // barrier_q has two terms (BL-233): the MEAN graded resistance of the land,
    // plus the ocean share at its own weight. Water is deliberately not folded
    // into the first — see k_ocean_barrier_weight for why it earns its own.
    const int land_resistance_q = land_tiles > 0
        ? clampi(static_cast<int>(resistance_sum / land_tiles), 0, 1000)
        : 0;
    const int ocean_share_q = clampi((ocean_tiles * 1000) / std::max(1, static_cast<int>(n)), 0, 1000);

    const int barrier_q = clampi(
        land_resistance_q + (ocean_share_q * k_ocean_barrier_weight) / 1000, 0, 1000);
    const int cradle_q  = clampi(static_cast<int>(out.cradles.size()) * 110, 0, 1000);

    out.fragmentation_q = clampi((barrier_q * 6 + cradle_q * 4) / 10, 0, 1000);

    // Conquest is priced by the ground an army must cross; exit is priced by
    // how easy it is to leave, which COASTS make cheap. High conquest against
    // low exit is HISTORY.md's non-hegemony condition.
    int coastal_cradles = 0;
    for (const agrarian_cradle& c : out.cradles) coastal_cradles += c.coastal;

    out.conquest_cost_q = clampi((barrier_q * 7) / 10 + static_cast<int>(out.cradles.size()) * 25, 0, 1000);
    out.exit_cost_q     = clampi(700 - coastal_cradles * 120 - barrier_q / 4, 0, 1000);

    // The DRIVING output: how many polities this land should grow. Seeded from
    // the cradles, widened by fragmentation.
    out.polity_seeds = clampi(
        static_cast<int>(out.cradles.size()) + (out.fragmentation_q * 12) / 1000, 1, 24);

    // --- Pass 4: the charter's home ----------------------------------------
    // The best-placed TRADING seat, not the biggest farm: Stage 1 is about the
    // enforceable promise between strangers, which needs strangers arriving.
    // Coastal access dominates; basin size breaks the rest. Tie-break by
    // cradle index, which is already raster-ordered.
    {
        int best = -1, best_rank = -1;
        for (std::size_t i = 0; i < out.cradles.size(); ++i)
        {
            const int rank = out.cradles[i].coastal * 10000 + out.cradles[i].fertile_tiles;
            if (rank > best_rank) { best_rank = rank; best = static_cast<int>(i); }
        }
        out.charter_cradle = best;
    }

    // --- Stage 0's line -----------------------------------------------------
    if (!out.cradles.empty())
    {
        // Richer land feeds a surplus sooner. Integer throughout: the date
        // participates in sorting, so it is a gate path (BL-220).
        const int arable_q = clampi(static_cast<int>(pl.arable_share * 1000.0f), 0, 1000);
        rng r(seed, tag_surplus);
        const int jitter = static_cast<int>(r.unit() * 400.0f); // +-0..400 yr of texture, not of outcome.
        const int64_t year = -(3000 + static_cast<int64_t>(arable_q) * 6 + jitter);

        const agrarian_cradle& first = out.cradles.front();
        std::string ev = "First granary cities on " + first.region + " floodplain.";
        std::string cons;
        if (out.cradles.size() == 1)
            cons = "-> a single cradle; one people, and nothing to fragment";
        else
            cons = "-> " + std::to_string(out.cradles.size())
                 + " independent cradles -> fragmentation "
                 + std::to_string(out.fragmentation_q / 10) + "%";

        out.history.push_back(history_event{
            years_from_calendar_year(year), chain_stage::legacy,
            std::move(ev), std::move(cons) });
    }

    return out;
}

// ---------------------------------------------------------------------------
// The driving hook — the ladder shapes the political map.
// ---------------------------------------------------------------------------

nation_params nation_params_from_ladder(const history_ladder_state& hl,
                                        const nation_params& base)
{
    nation_params p = base;

    // No cradles means no ladder signal at all (a world below a land
    // biosphere). Leave the caller's defaults untouched rather than driving
    // the political map from zeros.
    if (hl.cradles.empty())
        return p;

    // A fragmented world carries more, smaller polities. Seed density rises
    // (fewer tiles per seed) and the minimum viable territory falls, so the
    // merge pass absorbs less of what Pass 1 placed.
    //
    // Bounded on both sides: this MODULATES the body-specific defaults rather
    // than replacing them, so a tuning mistake here cannot produce a one-nation
    // or thousand-nation world. Constrain the inputs, never the gates
    // (PLANETOLOGY.md § The homeworld rule).
    const int f = hl.fragmentation_q;                    // 0-1000
    const int scale = 1000 + (500 - f);                  // 500 (fragmented) .. 1500 (unifiable)

    p.land_tiles_per_seed = clampi((base.land_tiles_per_seed * scale) / 1000,
                                   base.land_tiles_per_seed / 2,
                                   (base.land_tiles_per_seed * 3) / 2);
    p.min_nation_tiles    = clampi((base.min_nation_tiles * scale) / 1000,
                                   base.min_nation_tiles / 2,
                                   (base.min_nation_tiles * 3) / 2);

    // Expensive conquest keeps neighbours further apart — a wide, defended
    // frontier is exactly what a high conquest cost describes.
    p.min_seed_separation = clampi(base.min_seed_separation + hl.conquest_cost_q / 400,
                                   2, base.min_seed_separation + 3);
    return p;
}

// ---------------------------------------------------------------------------
// Stages 1-2 — written only once the political map exists.
// ---------------------------------------------------------------------------

void record_institutional_history(history_ladder_state& hl,
                                  const world& w,
                                  entity_id body_id,
                                  const std::vector<entity_id>& tile_ids,
                                  int gw)
{
    if (hl.cradles.empty())
        return;

    // Count the realms that actually survived the merge pass on THIS body.
    // Walked over the nation store's sorted key order rather than raw
    // unordered_map iteration, so the count and the naming are reproducible.
    std::vector<entity_id> nation_ids;
    nation_ids.reserve(w.nations.size());
    for (const auto& kv : w.nations) nation_ids.push_back(kv.first);
    std::sort(nation_ids.begin(), nation_ids.end());

    int realms = 0;
    for (const entity_id nid : nation_ids)
    {
        const auto nit = w.nations.find(nid);
        if (nit == w.nations.end() || nit->second.tiles.empty()) continue;
        const auto tit = w.tiles.find(nit->second.tiles.front());
        if (tit != w.tiles.end() && tit->second.body == body_id) ++realms;
    }

    // --- Stage 1: the Charter Act ------------------------------------------
    // Competition drives institutions, so a MORE fragmented world writes its
    // charter EARLIER — HISTORY.md's "it should come early relative to Earth"
    // is therefore a generated consequence rather than an authored date.
    const int64_t charter_year = 1700 - (static_cast<int64_t>(hl.fragmentation_q) * 2) / 5;

    std::string holder;
    if (hl.charter_cradle >= 0 && static_cast<std::size_t>(hl.charter_cradle) < hl.cradles.size())
    {
        const agrarian_cradle& c = hl.cradles[static_cast<std::size_t>(hl.charter_cradle)];
        const std::size_t idx = static_cast<std::size_t>(c.col + c.row * gw);
        if (idx < tile_ids.size())
        {
            const auto own = w.tile_to_nation.find(tile_ids[idx]);
            if (own != w.tile_to_nation.end())
            {
                const auto nit = w.nations.find(own->second);
                if (nit != w.nations.end()) holder = nit->second.name;
            }
        }
    }

    if (!holder.empty())
    {
        hl.history.push_back(history_event{
            years_from_calendar_year(charter_year), chain_stage::legacy,
            "The " + holder + " Charter Act - first perpetual company registered.",
            "-> a company outlives its founders; it can own, sue, and be sued" });
    }

    // --- Stage 2: the border accord ----------------------------------------
    // Written last because it counts an outcome. A world that unified into one
    // or two realms gets the OPPOSITE line: the premise says no hegemon, and a
    // seed that produced one is a data point to report, not a failure to hide
    // (Ben: "it would actually be valuable to see many failure cases").
    const int64_t accord_year =
        std::min<int64_t>(1900, charter_year + 100 + static_cast<int64_t>(hl.cradles.size()) * 10);

    if (realms >= 3)
    {
        hl.history.push_back(history_event{
            years_from_calendar_year(accord_year), chain_stage::legacy,
            "The Great Accord - " + std::to_string(realms) + " realms confirm mutual borders.",
            "-> no hegemon; a capability banned in one polity re-emerges next door" });
    }
    else
    {
        hl.history.push_back(history_event{
            years_from_calendar_year(accord_year), chain_stage::legacy,
            "One power absorbed the rest; the frontier wars ended early.",
            "-> a hegemon can choose stagnation, and this one could" });
    }
}
