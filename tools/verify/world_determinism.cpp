// ---------------------------------------------------------------------------
// Headless world-determinism harness (BL-114; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Proves make_hard_coded_world is a pure function of its world_params:
//
//   R1  Same seed + params -> a bit-identical world across repeated builds in
//       this binary (equal tile count, per-composition histogram, summed deposit
//       totals, nation/corp/entity counts). A different seed changes the world
//       (at least one metric differs) — so the seed genuinely drives generation.
//
//   R2  Resource abundance scales deposits monotonically without perturbing the
//       RNG streams: summed-deposit(sparse) < (lean) < (standard) at a fixed seed,
//       and the default descriptor (seed 0, standard) is the earth-like baseline.
//
//   R3  THE SAME GUARANTEE WITH THE ERA -1 PRE-HISTORY PASS ON. Every one of
//       R1/R2's call sites sets `prehistory_years = 0`, so until 2026-08-18 the
//       whole-world determinism guarantee EXCLUDED the year-tick history sim —
//       the pass that runs 400 years of settlement, war and conquest before the
//       campaign opens, and the pass an eight-sprint arc is about to change.
//       R3 builds the DEFAULT world (epoch_year 0, prehistory_years 400) and
//       asserts:
//         3.1 same seed + prehistory on, built twice -> identical world;
//         3.2 a different seed + prehistory on -> a different world;
//         3.3 prehistory on vs off at the SAME seed -> a different world, so the
//             pass is demonstrably not a silent no-op;
//         3.4 the pass reports having run, and having done something;
//         3.5 the sim's own reported outcome (battles/conquests/foundings) is
//             identical across the two same-seed runs.
//
//       R3 deliberately uses the REAL default of 400 years, not a shortened
//       run: a determinism guarantee measured over a span nobody ships is not
//       the guarantee. It is why this harness is no longer cheap — see the
//       timing lines it prints.
//
// The process exits non-zero if any assertion FAILs. Links only the SDL/Lua-free
// world-generation translation units (see CMakeLists.txt), mirroring the manual
// build in tools/verify/README.md.

#include "world/components.hpp"
#include "world/era_minus_one.hpp" // BL-754: the per-pass clock rides the fixture
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/history_sim.hpp"   // BL-1009: polity::navy_stock, exploration_output
#include "world/settlement.hpp"    // BL-1009: region stocks on world::gen_settlement
#include "world/stockpile_budget.hpp" // BL-1042: the stockpile folded into the region digest
#include "world/world.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace {

struct world_metrics
{
    std::size_t        tiles    = 0;
    std::size_t        nations  = 0;
    std::size_t        corps    = 0;
    std::size_t        entities = 0; ///< sum of the public component containers
    std::map<int, int> comp_hist;    ///< tiles per (substrate, cover) pair (BL-519)
    double             deposit_total = 0.0;

    bool operator==(const world_metrics& o) const
    {
        return tiles == o.tiles && nations == o.nations && corps == o.corps &&
               entities == o.entities && comp_hist == o.comp_hist &&
               deposit_total == o.deposit_total;
    }
};

world_metrics measure(const world& w)
{
    world_metrics m;
    m.tiles    = w.tiles.size();
    m.nations  = w.nations.size();
    m.corps    = w.corporations.size();
    m.entities = w.bodies.size() + w.tiles.size() + w.buildings.size() +
                 w.stockpiles.size() + w.markets.size() + w.units.size() +
                 w.population_centres.size() + w.nations.size() + w.corporations.size();
    for (const auto& [id, tc] : w.tiles)
    {
        // BOTH AXES fold into the digest (BL-519). Hashing only the substrate
        // would let a cover regression pass a determinism check silently, which
        // is exactly the hole a split axis opens if the digest is not widened
        // with it.
        ++m.comp_hist[static_cast<int>(tc.substrate) * 16 + static_cast<int>(tc.cover)];
        m.deposit_total += static_cast<double>(tc.cover_density) * 1e-6;
        for (std::size_t r = 0; r < resource_count; ++r)
            m.deposit_total += tc.resource_deposit[r];
    }
    return m;
}

// ---------------------------------------------------------------------------
// The deep digest (R3 only)
// ---------------------------------------------------------------------------
// `world_metrics` above is a TILE-AND-COUNT digest: terrain histogram, summed
// deposits, container sizes. That is the right instrument for R1/R2, which ask
// whether the seed and the abundance tier drive TILE generation.
//
// It is the wrong instrument for the Era -1 year-tick sim, and reusing it for
// R3 would be the vacuity trap this file exists to catch elsewhere. The sim
// runs AFTER the tile surface is fixed and changes nothing in it — it moves
// regions between owners, and reaches the world only through the nation
// carve, the derived national character, the generated names and the corporate
// charter placement downstream of them. Two worlds with entirely different
// political histories can hold the identical terrain histogram, the identical
// deposit total and the identical container sizes.
//
// So R3 compares a whole-world FNV-1a instead: `world::state_hash` — the
// project's own canonical snapshot primitive (corps, buildings, markets, pools,
// tile reserves, units, order book) — folded together with the political layer
// state_hash omits. state_hash omits it on purpose: it is a TICK-BOUNDARY
// instrument, and nations, borders and city names do not move on a tick. They
// are precisely what the pre-history pass writes, so R3 hashes them itself.
//
// Used ONLY by R3/R4. R1/R2 keep the metrics they were written against, so
// nothing about the existing cases changes.
//
// THE DIGEST PROVES SAME-SEED-SAME-WORLD ONLY FOR THE FIELDS IT FOLDS (BL-1009,
// with BL-957 merged in). Twice a real world-mover slipped through with every
// digest digit-identical: moving world setup from 1200 grudges and corridors to
// 1660 ones re-seeded sentiment by hundreds of rows and re-stamped hundreds of
// road tiles (BL-956), and folding every held seat into the capital moved the
// mean capital treasury by 80% and the Post Road count by 25 (BL-998). Neither
// field was read here. So the digest now folds, beside the political layer:
//
//   - seeded sentiment   `world::sentiment` (every row, both dimensions);
//   - road tiers         `tile_component::road_level`, per tile;
//   - region stocks      `world::gen_settlement->regions`: `treasury`,
//                        `port_stock_q`, `standing_army` and its owner;
//   - the stockpile      (BL-1042) every region's `industry_points`, and the
//                        charter budget `build_stockpile_budget` makes of them
//                        (per centre, and every unspent reason) — FOLDED ONLY
//                        WHEN SOME REGION HOLDS A POINT, so a span-off world
//                        (no point anywhere) hashes exactly as it did before
//                        the fold existed. Complete as a detector on the same
//                        argument as the nation treasury below: two worlds
//                        differing in any region's points have a non-zero on
//                        at least one side, so at least one folds;
//   - the navy           `polity::navy_stock` off the Exploration handoff;
//   - corridor tiers     the corridor set world setup stamped roads from, per
//                        tier and per corridor.
//
// NATION TREASURY NEEDS NO FOLD OF ITS OWN: `world::state_hash`, which seeds this
// digest, already folds `nation_component::treasury` whenever any is non-zero,
// and that conditional is complete as a detector (two worlds differing in any
// treasury have a non-zero on at least one side).
//
// THE LAST TWO ARE NOT WORLD STATE, which is why the digest takes the fixture.
// A navy lives on the Era -1 polity and never lands on `world`; the corridor
// record is a generation local that `stamp_history_roads` and the junction
// markets consume. Both are what the span hands forward, and a regression in
// either is otherwise invisible until something downstream happens to read it.
// They are read off the SAME fixture generation filled, never re-derived.

constexpr uint64_t fnv_prime = 1099511628211ull;

void fold_bytes(uint64_t& h, const void* p, std::size_t n)
{
    const auto* b = static_cast<const unsigned char*>(p);
    for (std::size_t i = 0; i < n; ++i)
    {
        h ^= static_cast<uint64_t>(b[i]);
        h *= fnv_prime;
    }
}

void fold_u32(uint64_t& h, uint32_t v) { fold_bytes(h, &v, sizeof v); }
void fold_i32(uint64_t& h, int32_t v) { fold_bytes(h, &v, sizeof v); }
void fold_i64(uint64_t& h, int64_t v) { fold_bytes(h, &v, sizeof v); }
void fold_f32(uint64_t& h, float v) { fold_bytes(h, &v, sizeof v); }

void fold_str(uint64_t& h, const std::string& s)
{
    fold_u32(h, static_cast<uint32_t>(s.size()));
    fold_bytes(h, s.data(), s.size());
}

/// Keys of an unordered container, sorted — the same canonicalisation
/// world::state_hash performs, for the same reason (hash order is not state).
template <typename Map>
std::vector<entity_id> sorted_ids(const Map& m)
{
    std::vector<entity_id> ids;
    ids.reserve(m.size());
    for (const auto& kv : m) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

uint64_t deep_digest(const world& w, const era_minus_one_fixture& fx)
{
    // Seed the fold with the canonical snapshot rather than repeating it.
    uint64_t h = w.state_hash(0);

    for (const entity_id id : sorted_ids(w.bodies))
    {
        const body_component& b = w.bodies.at(id);
        fold_u32(h, id);
        fold_str(h, b.name);
        fold_i32(h, b.grid_width);
        fold_i32(h, b.grid_height);
        fold_f32(h, b.mass_earths);
        fold_f32(h, b.orbital_radius_au);
        fold_i32(h, static_cast<int32_t>(b.type));
    }

    // The political carve — what the pre-history pass actually rewrites.
    for (const entity_id id : sorted_ids(w.nations))
    {
        const nation_component& n = w.nations.at(id);
        fold_u32(h, id);
        fold_str(h, n.name);
        fold_u32(h, static_cast<uint32_t>(n.tiles.size()));
        for (const entity_id t : n.tiles) fold_u32(h, t); // ordered: Pass 2 output
        for (const float a : n.resource_abundance) fold_f32(h, a);
        fold_i32(h, static_cast<int32_t>(n.politics));
        fold_i32(h, static_cast<int32_t>(n.posture));
        fold_i32(h, static_cast<int32_t>(n.focus));
    }

    for (const entity_id t : sorted_ids(w.tile_to_nation))
    {
        fold_u32(h, t);
        fold_u32(h, w.tile_to_nation.at(t));
    }

    for (const entity_id id : sorted_ids(w.population_centres))
    {
        const population_centre_component& p = w.population_centres.at(id);
        fold_u32(h, id);
        fold_i32(h, p.scale);
        fold_i32(h, p.population);
        fold_f32(h, p.habitability);
        fold_i32(h, p.growth_accumulator);
        const auto tit = w.population_centre_tile.find(id);
        fold_u32(h, tit != w.population_centre_tile.end() ? tit->second : null_entity);
        const auto nit = w.population_centre_name.find(id);
        if (nit != w.population_centre_name.end()) fold_str(h, nit->second);
    }

    for (const entity_id id : sorted_ids(w.corporations))
    {
        const corporation_component& c = w.corporations.at(id);
        fold_u32(h, id);
        fold_str(h, c.name);
        fold_u32(h, c.home_nation);
        fold_i32(h, static_cast<int32_t>(c.focus));
        fold_f32(h, c.starting_capital);
        fold_i32(h, c.is_player ? 1 : 0);
        fold_u32(h, static_cast<uint32_t>(c.assets.size()));
        for (const entity_id a : c.assets) fold_u32(h, a);
    }

    // The world log: generation narrates into it, so a pass that ran
    // differently shows up here as prose even when nothing numeric moved.
    fold_u32(h, static_cast<uint32_t>(w.history_log.size()));
    for (const world_history_entry& e : w.history_log)
    {
        fold_i64(h, e.timestamp);
        fold_i32(h, static_cast<int32_t>(e.topic));
        fold_u32(h, e.body);
        fold_u32(h, e.corp);
        fold_str(h, e.event);
        fold_str(h, e.consequence);
    }

    // --- BL-1009: what generation moves that play reads ---------------------
    // Every section folds its size first, so an empty table and a table of
    // one zero-valued row cannot collide.

    // Seeded sentiment (BL-898's grudge rows). `sentiment_table::pairs` is a
    // std::map keyed (observer, subject), so this is already a sorted walk.
    fold_u32(h, static_cast<uint32_t>(w.sentiment.pairs.size()));
    for (const auto& [pair, v] : w.sentiment.pairs)
    {
        fold_u32(h, pair.first);
        fold_u32(h, pair.second);
        fold_f32(h, v.access);
        fold_f32(h, v.trust);
    }

    // Road tier per tile, SPARSE: (tile, tier) for every tile carrying a road,
    // in ascending tile id. Complete as a detector — two worlds that differ on
    // any tile's tier differ in this list — and it keeps the walk proportional
    // to the network rather than to the grid.
    {
        std::vector<entity_id> roads;
        for (const auto& [tid, tc] : w.tiles)
            if (tc.road_level != 0) roads.push_back(tid);
        std::sort(roads.begin(), roads.end());
        fold_u32(h, static_cast<uint32_t>(roads.size()));
        for (const entity_id tid : roads)
        {
            fold_u32(h, tid);
            fold_u32(h, w.tiles.at(tid).road_level);
        }
    }

    // Region stocks, in region index order (a vector: its order is the
    // settlement's placement order, which is itself generation output). The
    // absent record folds a sentinel so "no settlement" cannot hash as "a
    // settlement of no regions".
    if (const settlement_state* ss = w.gen_settlement.get())
    {
        fold_u32(h, static_cast<uint32_t>(ss->regions.size()));
        for (const region& rg : ss->regions)
        {
            fold_i64(h, rg.treasury);
            fold_i32(h, rg.port_stock_q);
            fold_i64(h, rg.standing_army);
            fold_i32(h, rg.standing_army_owner);
        }
    }
    else
    {
        fold_u32(h, 0xFFFFFFFFu);
    }

    // BL-1042 — THE STOCKPILE, conditional (see the header): nothing is folded
    // while every region holds zero points, which is every span-off world.
    if (const settlement_state* ss = w.gen_settlement.get())
    {
        bool any = false;
        for (const region& rg : ss->regions)
            any = any || rg.industry_points != 0;
        if (any)
        {
            fold_u32(h, 0x10420000u);   // a section tag: "the stockpile follows"
            for (const region& rg : ss->regions)
                fold_i64(h, rg.industry_points);
            const stockpile_budget sb = build_stockpile_budget(w);
            fold_i32(h, sb.rejected ? 1 : 0);
            fold_i64(h, sb.points_total);
            fold_i32(h, sb.firm_price_points);   // BL-1064: the price the stock derives
            for (const std::int64_t u : sb.unspent)
                fold_i64(h, u);
            fold_u32(h, static_cast<uint32_t>(sb.budget.points().size()));
            for (const auto& [centre, pts] : sb.budget.points())   // std::map: ascending id
            {
                fold_u32(h, centre);
                fold_i32(h, pts);
            }
        }
    }

    // The navy, off the Exploration handoff's polity table (index order).
    // Empty — and folded as a zero size — wherever the span did not run.
    fold_i32(h, fx.exploration_ran ? 1 : 0);
    fold_u32(h, static_cast<uint32_t>(fx.exploration_handoff.polities.size()));
    for (const polity& p : fx.exploration_handoff.polities)
        fold_i64(h, p.navy_stock);

    // The corridor set world setup stamped roads and junction markets from:
    // the per-tier counts, then every corridor's (a, b, tier) in (a, b) order.
    // Sorted here rather than trusted, so the fold cannot lean on either
    // span's own ordering. `uses` is deliberately NOT folded — it is traffic,
    // and nothing past generation reads it; the tier is what reaches the map.
    {
        std::vector<history_corridor> cs = fx.setup_corridors;
        std::sort(cs.begin(), cs.end(),
                  [](const history_corridor& x, const history_corridor& y) {
                      return x.a != y.a ? x.a < y.a : x.b < y.b;
                  });
        std::map<int, uint32_t> tier_count;
        for (const history_corridor& c : cs) ++tier_count[c.tier];
        fold_u32(h, static_cast<uint32_t>(cs.size()));
        fold_u32(h, static_cast<uint32_t>(tier_count.size()));
        for (const auto& [tier, n] : tier_count)
        {
            fold_i32(h, tier);
            fold_u32(h, n);
        }
        for (const history_corridor& c : cs)
        {
            fold_u32(h, c.a);
            fold_u32(h, c.b);
            fold_u32(h, c.tier);
        }
    }

    return h;
}

/// A human-readable line of what the BL-1009 folds saw, so a moved digest can be
/// read as a moved FIELD. Reported, never asserted.
void print_coverage(const char* what, const world& w, const era_minus_one_fixture& fx)
{
    std::size_t road_tiles = 0;
    std::map<int, int> road_tier;
    for (const auto& [tid, tc] : w.tiles)
        if (tc.road_level != 0) { ++road_tiles; ++road_tier[tc.road_level]; }

    int64_t region_treasury = 0, standing = 0;
    int64_t port_stock = 0;
    std::size_t regions = 0;
    if (const settlement_state* ss = w.gen_settlement.get())
    {
        regions = ss->regions.size();
        for (const region& rg : ss->regions)
        {
            region_treasury += rg.treasury;
            port_stock += rg.port_stock_q;
            standing += rg.standing_army;
        }
    }
    const stockpile_budget sb = build_stockpile_budget(w);   // BL-1042
    int64_t navy = 0;
    for (const polity& p : fx.exploration_handoff.polities) navy += p.navy_stock;

    int corridor_tier[4] = { 0, 0, 0, 0 };
    int corridor_other   = 0;
    for (const history_corridor& c : fx.setup_corridors)
    {
        if (c.tier < 4) ++corridor_tier[c.tier];
        else ++corridor_other;
    }

    double nation_treasury = 0.0;
    for (const auto& [nid, nc] : w.nations) nation_treasury += nc.treasury;

    std::printf("     coverage %-14s sentiment rows=%zu | road tiles=%zu (T1 %d, T2 %d, T3 %d)\n",
                what, w.sentiment.pairs.size(), road_tiles,
                road_tier[1], road_tier[2], road_tier[3]);
    std::printf("         regions=%zu treasury=%lld port_stock_q=%lld standing_army=%lld |"
                " navy=%lld | nation treasury=%.0f\n",
                regions, static_cast<long long>(region_treasury),
                static_cast<long long>(port_stock), static_cast<long long>(standing),
                static_cast<long long>(navy), nation_treasury);
    std::printf("         corridors=%zu (tier0 %d, tier1 %d, tier2 %d, tier3 %d, other %d)\n",
                fx.setup_corridors.size(), corridor_tier[0], corridor_tier[1],
                corridor_tier[2], corridor_tier[3], corridor_other);
    std::printf("         stockpile (BL-1042): %lld points, %zu budgeted centres, %lld unspent%s;"
                " carve index %zu founded / %zu dropped\n",
                static_cast<long long>(sb.points_total), sb.budget.points().size(),
                static_cast<long long>(sb.points_unspent()), sb.rejected ? " REJECTED" : "",
                w.gen_carve_centres.size(), w.gen_carve_dropped.size());
}

/// BL-1042: the Digitisation span is OFF in every world this harness builds, so
/// the stockpile must be empty — no point on any region, an empty budget, a
/// closed account — and the stockpile fold must not have run.
bool stockpile_empty(const world& w)
{
    const stockpile_budget sb = build_stockpile_budget(w);
    return sb.points_total == 0 && sb.budget.empty() && !sb.rejected && sb.balanced();
}

int failures = 0;

void check(bool ok, const char* label)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok)
        ++failures;
}

/// Build a world and report how long it took, so the era pass's cost is a
/// measured number in the log rather than an estimate in a comment.
///
/// SINCE BL-754 IT ALSO REPORTS THE PER-PASS SPLIT. The total on its own could
/// not answer the sprint's question — what the Era -1 pass costs against
/// everything else, and what a second span adds to it — because a slower total
/// is equally consistent with a slower tile pass. `make_hard_coded_world`
/// measures the split itself and hands it back on the fixture (which, unlike
/// `generation_report`, has no save-seam presence), so this asks for a fixture
/// purely to read the clock off it.
///
/// Timings are REPORTED here and never asserted. They vary with the machine,
/// the build type and the load, so binding a check to one would be pinning a
/// number that is not a property of the world.
///
/// BL-1009: the fixture is the CALLER'S now, because `deep_digest` reads two
/// fields off it that never land on `world` (the navy and the corridor set).
world timed_world(const world_params& p, generation_report* rep, const char* what,
                  era_minus_one_fixture& fx)
{
    const auto t0 = std::chrono::steady_clock::now();
    world w = make_hard_coded_world(p, rep, {}, nullptr, nullptr, &fx);
    const double secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("     [%-24s] %7.2f s  (seed %08X, prehistory %d y, epoch %lld)\n",
                what, secs, p.seed, p.prehistory_years,
                static_cast<long long>(p.epoch_year));
    std::printf("         passes: pre-settlement %5lld ms | settlement %5lld ms |"
                " era-1 %6lld ms | post-era %5lld ms\n",
                static_cast<long long>(fx.ms_before_settlement),
                static_cast<long long>(fx.ms_settlement),
                static_cast<long long>(fx.ms_era),
                static_cast<long long>(fx.ms_after_era));
    if (fx.ran)
    {
        // The span structure the run ACTUALLY used, read off the fixture rather
        // than re-derived — same reason the fixture exists at all (BL-462).
        char boundary[32] = "(none)";
        if (fx.params.boundary_year != INT64_MIN)
            std::snprintf(boundary, sizeof boundary, "%lld",
                          static_cast<long long>(fx.params.boundary_year));
        std::printf("         span:   %lld -> %s -> %lld"
                    "  (tick bands %d, span-1 ceiling band %d)\n",
                    static_cast<long long>(fx.params.start_year), boundary,
                    static_cast<long long>(fx.params.stop_year),
                    fx.params.tick_band_count,
                    static_cast<int>(fx.params.span1_band_ceiling));
    }
    std::fflush(stdout);
    return w;
}

} // namespace

int main()
{
    constexpr uint32_t seed_a = 0xABCDEF01u;
    constexpr uint32_t seed_b = 0x12345678u;

    // --- R1: same seed + params is bit-identical across two builds ---
    const world_metrics a1 = measure(make_hard_coded_world(
        world_params{ .seed = seed_a, .abundance = abundance_level::standard, .prehistory_years = 0 }));
    const world_metrics a2 = measure(make_hard_coded_world(
        world_params{ .seed = seed_a, .abundance = abundance_level::standard, .prehistory_years = 0 }));

    check(a1 == a2, "R1 same seed+params -> bit-identical world (tiles/hist/deposits/nations/corps/entities)");
    std::printf("     seed %08X: %zu tiles, %zu nations, %zu corps, deposits=%.3f\n",
                seed_a, a1.tiles, a1.nations, a1.corps, a1.deposit_total);

    // --- R1: a different seed produces a demonstrably different world ---
    const world_metrics b = measure(make_hard_coded_world(
        world_params{ .seed = seed_b, .abundance = abundance_level::standard, .prehistory_years = 0 }));
    const bool seed_matters = (b.comp_hist != a1.comp_hist) || (b.deposit_total != a1.deposit_total);
    check(seed_matters, "R1 different seed -> different world (composition histogram or deposit total differs)");
    std::printf("     seed %08X: %zu tiles, deposits=%.3f (vs %.3f)\n",
                seed_b, b.tiles, b.deposit_total, a1.deposit_total);

    // --- R2: abundance scales deposits monotonically at a fixed seed ---
    const double d_sparse = measure(make_hard_coded_world(
        world_params{ .seed = seed_a, .abundance = abundance_level::sparse, .prehistory_years = 0 })).deposit_total;
    const double d_lean = measure(make_hard_coded_world(
        world_params{ .seed = seed_a, .abundance = abundance_level::lean, .prehistory_years = 0 })).deposit_total;
    const double d_standard = a1.deposit_total; // standard at seed_a, measured above

    check(d_sparse < d_lean && d_lean < d_standard,
          "R2 abundance ordering: sparse < lean < standard (deposit totals)");
    std::printf("     deposits sparse=%.3f  lean=%.3f  standard=%.3f\n",
                d_sparse, d_lean, d_standard);

    // --- R2: standard is the earth-like ceiling — no tier exceeds it ---
    check(d_standard >= d_lean && d_standard >= d_sparse,
          "R2 standard is the resource ceiling (no tier richer than earth-like)");

    // --- Sanity: the default descriptor equals seed 0 / standard (legacy world) ---
    const world_metrics dflt   = measure(make_hard_coded_world(no_prehistory()));
    const world_metrics zero_s = measure(make_hard_coded_world(
        world_params{ .seed = 0, .abundance = abundance_level::standard, .prehistory_years = 0 }));
    check(dflt == zero_s, "default descriptor == {seed 0, standard} (legacy world)");

    // -----------------------------------------------------------------------
    // R3 — the same guarantee with the Era -1 pre-history pass ON
    // -----------------------------------------------------------------------
    // The values below are the SHIPPING DEFAULTS, restated explicitly so the
    // case cannot drift with them: epoch_year 0 (the ancient refocus, NR-177)
    // and prehistory_years 400 (Ben's figure, 4 years a tick = 100 rounds).
    // Nothing here is shortened to fit a timeout.
    std::printf("\n--- R3: determinism with the Era -1 pre-history pass ON ---\n");
    std::fflush(stdout);

    const world_params pre_a{ .seed = seed_a, .abundance = abundance_level::standard,
                              .epoch_year = 0, .prehistory_years = 400 };
    const world_params pre_b{ .seed = seed_b, .abundance = abundance_level::standard,
                              .epoch_year = 0, .prehistory_years = 400 };
    const world_params off_a{ .seed = seed_a, .abundance = abundance_level::standard,
                              .epoch_year = 0, .prehistory_years = 0 };

    generation_report rep_a1{}, rep_a2{}, rep_b{};

    era_minus_one_fixture fx_a1, fx_a2, fx_b, fx_off;
    const world w_a1  = timed_world(pre_a, &rep_a1, "seed A, prehistory ON #1", fx_a1);
    const world w_a2  = timed_world(pre_a, &rep_a2, "seed A, prehistory ON #2", fx_a2);
    const world w_b   = timed_world(pre_b, &rep_b,  "seed B, prehistory ON", fx_b);
    const world w_off = timed_world(off_a, nullptr, "seed A, prehistory OFF", fx_off);

    const world_metrics m_a1 = measure(w_a1);
    const world_metrics m_a2 = measure(w_a2);

    const uint64_t d_a1  = deep_digest(w_a1, fx_a1);
    const uint64_t d_a2  = deep_digest(w_a2, fx_a2);
    const uint64_t d_b   = deep_digest(w_b, fx_b);
    const uint64_t d_off = deep_digest(w_off, fx_off);

    print_coverage("seedA/on", w_a1, fx_a1);
    print_coverage("seedB/on", w_b, fx_b);
    print_coverage("seedA/off", w_off, fx_off);

    std::printf("     digest seedA/on  = %016llX and %016llX\n",
                static_cast<unsigned long long>(d_a1), static_cast<unsigned long long>(d_a2));
    std::printf("     digest seedB/on  = %016llX\n", static_cast<unsigned long long>(d_b));
    std::printf("     digest seedA/off = %016llX\n", static_cast<unsigned long long>(d_off));

    // 3.1 — the guarantee itself.
    check(m_a1 == m_a2 && d_a1 == d_a2,
          "R3.1 same seed + prehistory ON, built twice -> identical world (metrics AND deep digest)");
    if (!(m_a1 == m_a2))
        std::printf("     metrics differ: tiles %zu/%zu nations %zu/%zu corps %zu/%zu"
                    " entities %zu/%zu deposits %.6f/%.6f\n",
                    m_a1.tiles, m_a2.tiles, m_a1.nations, m_a2.nations,
                    m_a1.corps, m_a2.corps, m_a1.entities, m_a2.entities,
                    m_a1.deposit_total, m_a2.deposit_total);
    else if (d_a1 != d_a2)
        std::printf("     NOTE: tile/count metrics agree but the deep digest does not — the divergence\n"
                    "           is in the political layer (nations / carve / names / charters), which is\n"
                    "           exactly what the era pass writes and what world_metrics cannot see.\n");

    // 3.2 — the seed still drives the world with the pass on.
    check(d_b != d_a1,
          "R3.2 different seed + prehistory ON -> different world (deep digest differs)");

    // 3.3 — the pass is not a no-op. Same seed, same epoch, ONLY the year span
    //       differs; if that produced the same world the era sim would be
    //       decorative and R3.1 would be asserting nothing.
    check(d_off != d_a1,
          "R3.3 prehistory ON vs OFF at the same seed -> different world (the era pass is not a no-op)");

    // 3.4 — the pass says so itself. The generation report is the instrument
    //       hard_coded_world.cpp added precisely so a peaceful world could not
    //       be mistaken for a pass that never ran.
    std::printf("     report seedA/on: years=%lld battles=%lld conquests=%lld foundings=%lld\n",
                static_cast<long long>(rep_a1.prehistory_years),
                static_cast<long long>(rep_a1.prehistory_battles),
                static_cast<long long>(rep_a1.prehistory_conquests),
                static_cast<long long>(rep_a1.prehistory_foundings));
    // Seed B's counters are printed alongside because the era pass's WALL CLOCK
    // is strongly seed-dependent (measured 2026-08-18: 23-34 s at seed A against
    // 75 s at seed B, same Debug build, same span — 400 years at the time of
    // that measurement, 1600 since BL-906), and the counters
    // are what makes that legible rather than mysterious. Note which counter
    // tracks it: seed B is 3x the cost with FEWER battles (193 vs 365) and more
    // FOUNDINGS (994 vs 765), so the era sim's cost scales with the region
    // table it carries, not with how much fighting happens in it. The arc about
    // to change this pass needs that distinction, not just the total.
    std::printf("     report seedB/on: years=%lld battles=%lld conquests=%lld foundings=%lld\n",
                static_cast<long long>(rep_b.prehistory_years),
                static_cast<long long>(rep_b.prehistory_battles),
                static_cast<long long>(rep_b.prehistory_conquests),
                static_cast<long long>(rep_b.prehistory_foundings));
    // BL-906: the Empires round's own span is 400 BCE -> 1200 CE (1,600
    // years), not the epoch-coupled 400 this assertion pinned before the fix
    // (`docs/generation/CIVILISATION.md` § "The closure of the Empire era").
    check(rep_a1.prehistory_years == 1600,
          "R3.4 the era pass ran the full 1600-year span (report agrees with the params)");
    check(rep_a1.prehistory_battles + rep_a1.prehistory_conquests
              + rep_a1.prehistory_foundings > 0,
          "R3.4 the era pass DID something (battles/conquests/foundings not all zero)");

    // 3.5 — the sim's own reported outcome is reproducible, not just the world
    //       it lands in. A divergence here localises the leak to the sim.
    const bool report_same = rep_a1.prehistory_years == rep_a2.prehistory_years
                             && rep_a1.prehistory_battles == rep_a2.prehistory_battles
                             && rep_a1.prehistory_conquests == rep_a2.prehistory_conquests
                             && rep_a1.prehistory_foundings == rep_a2.prehistory_foundings;
    check(report_same,
          "R3.5 the era sim's reported outcome is identical across two same-seed runs");

    // 3.6 — BL-969: both handoff validators now run on the SHIPPED path and
    //       record a violation on the report rather than in a harness only.
    //       Asserted on both seeds, so a world whose folded handoff broke
    //       the contract cannot pass for one that honoured it.
    if (rep_a1.handoff_invalid)
        std::printf("     seed A handoff violation: %s\n", rep_a1.handoff_violation.c_str());
    if (rep_b.handoff_invalid)
        std::printf("     seed B handoff violation: %s\n", rep_b.handoff_violation.c_str());
    check(!rep_a1.handoff_invalid && !rep_a2.handoff_invalid,
          "R3.6 seed A's pass_one/exploration handoffs pass their validators on the shipped path");
    check(!rep_b.handoff_invalid,
          "R3.6 seed B's pass_one/exploration handoffs pass their validators on the shipped path");
    if (!report_same)
        std::printf("     run #2:          years=%lld battles=%lld conquests=%lld foundings=%lld\n",
                    static_cast<long long>(rep_a2.prehistory_years),
                    static_cast<long long>(rep_a2.prehistory_battles),
                    static_cast<long long>(rep_a2.prehistory_conquests),
                    static_cast<long long>(rep_a2.prehistory_foundings));

    // 3.7 — BL-1042, RE-POINTED BY BL-1044: the Digitisation span runs by
    //       default, so the two shipped worlds here carry a stockpile — points,
    //       a non-empty budget, a derived price, every point accounted — and
    //       it is the same stockpile on two same-seed builds. The prehistory-
    //       OFF world runs no span, so its stockpile is empty (the pre-budget
    //       world). A span-OFF world with the prehistory on is the LEGACY arc,
    //       and player_seed_sweep --arc legacy fails any such row whose
    //       stockpile holds a point.
    {
        const stockpile_budget s_a1 = build_stockpile_budget(w_a1);
        const stockpile_budget s_a2 = build_stockpile_budget(w_a2);
        const stockpile_budget s_b  = build_stockpile_budget(w_b);
        const auto live = [](const stockpile_budget& s) {
            return s.points_total > 0 && !s.budget.empty() && !s.rejected && s.balanced()
                && s.firm_price_points > 0;
        };
        std::printf("     stockpile seedA/on %lld points, price %d | seedB/on %lld points, price %d\n",
                    static_cast<long long>(s_a1.points_total), static_cast<int>(s_a1.firm_price_points),
                    static_cast<long long>(s_b.points_total), static_cast<int>(s_b.firm_price_points));
        check(live(s_a1) && live(s_b) && stockpile_empty(w_off),
              "R3.7 the span runs by default: seeds A and B carry a non-empty, balanced, priced "
              "stockpile; prehistory OFF carries none (BL-1042, BL-1044)");
        check(s_a1.points_total == s_a2.points_total && s_a1.budget.points() == s_a2.budget.points()
                  && s_a1.firm_price_points == s_a2.firm_price_points,
              "R3.7 the stockpile budget is identical across two same-seed builds (BL-1044)");
    }
    check(!w_a1.gen_carve_centres.empty() && w_a1.gen_carve_centres == w_a2.gen_carve_centres,
          "R3.7 the carve index is populated and identical across two same-seed builds (BL-1042)");

    // -----------------------------------------------------------------------
    // R4 — the TWO-SPAN arc (BL-747), and the generation budget (BL-754)
    // -----------------------------------------------------------------------
    // Until BL-747 an `epoch_year` of 1960 skipped the Era -1 pass outright:
    // `era_minus_one_enabled` gated on `epoch_year < 1700`, so the industrial
    // arc generated with no year-tick history at all. It now runs the SAME
    // engine across two spans — an ancient one capped at the medieval roster
    // band, then an industrial one with the ladder unrestricted — expressed as
    // params on the single existing invocation rather than a second call to
    // `run_history_sim` (era_minus_one.hpp § a seventh caller).
    //
    // WHAT IS ASSERTED HERE AND WHAT IS NOT. Asserted: the pass runs at 1960
    // at all, it spans both halves, and it is deterministic. NOT asserted: any
    // magnitude — battle counts, founding counts or wall clock. Those are
    // REPORTED, because the sprint's question is what the second span costs and
    // a number nobody has chosen a target for is not a contract.
    std::printf("\n--- R4: the two-span arc at epoch 1960 (BL-747) ---\n");
    std::fflush(stdout);

    const world_params ind_a{ .seed = seed_a, .abundance = abundance_level::standard,
                              .epoch_year = 1960, .prehistory_years = 400,
                              .industrial_years = 400 };

    generation_report rep_i1{}, rep_i2{};
    era_minus_one_fixture fx_i1, fx_i2;
    const world w_i1 = timed_world(ind_a, &rep_i1, "seed A, epoch 1960 #1", fx_i1);
    const world w_i2 = timed_world(ind_a, &rep_i2, "seed A, epoch 1960 #2", fx_i2);

    const uint64_t d_i1 = deep_digest(w_i1, fx_i1);
    const uint64_t d_i2 = deep_digest(w_i2, fx_i2);
    print_coverage("1960/two-span", w_i1, fx_i1);
    std::printf("     digest 1960/two-span = %016llX and %016llX\n",
                static_cast<unsigned long long>(d_i1), static_cast<unsigned long long>(d_i2));
    std::printf("     report 1960: years=%lld battles=%lld conquests=%lld foundings=%lld\n",
                static_cast<long long>(rep_i1.prehistory_years),
                static_cast<long long>(rep_i1.prehistory_battles),
                static_cast<long long>(rep_i1.prehistory_conquests),
                static_cast<long long>(rep_i1.prehistory_foundings));

    check(rep_i1.prehistory_years == 800,
          "R4.1 the 1960 arc ran BOTH spans (400 ancient + 400 industrial = 800 years)");
    check(rep_i1.prehistory_battles + rep_i1.prehistory_conquests
              + rep_i1.prehistory_foundings > 0,
          "R4.2 the era pass DID something at epoch 1960 (it used to be skipped entirely)");
    check(d_i1 == d_i2 && rep_i1.prehistory_battles == rep_i2.prehistory_battles,
          "R4.3 the two-span run is deterministic (deep digest and counters both agree)");
    if (rep_i1.handoff_invalid)
        std::printf("     1960 handoff violation: %s\n", rep_i1.handoff_violation.c_str());
    check(!rep_i1.handoff_invalid,
          "R4.4 the 1960 arc's handoffs pass their validators on the shipped path (BL-969)");

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILURES", failures,
                failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
