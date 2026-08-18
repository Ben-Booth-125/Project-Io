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
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
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
    std::map<int, int> comp_hist;    ///< tiles per terrain_composition
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
        ++m.comp_hist[static_cast<int>(tc.composition)];
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
// Used ONLY by R3. R1/R2 keep the metrics they were written against, so nothing
// about the existing cases changes.

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

uint64_t deep_digest(const world& w)
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

    return h;
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
world timed_world(const world_params& p, generation_report* rep, const char* what)
{
    const auto t0 = std::chrono::steady_clock::now();
    world w = make_hard_coded_world(p, rep);
    const double secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("     [%-24s] %7.2f s  (seed %08X, prehistory %d y, epoch %lld)\n",
                what, secs, p.seed, p.prehistory_years,
                static_cast<long long>(p.epoch_year));
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

    const world w_a1  = timed_world(pre_a, &rep_a1, "seed A, prehistory ON #1");
    const world w_a2  = timed_world(pre_a, &rep_a2, "seed A, prehistory ON #2");
    const world w_b   = timed_world(pre_b, &rep_b,  "seed B, prehistory ON");
    const world w_off = timed_world(off_a, nullptr, "seed A, prehistory OFF");

    const world_metrics m_a1 = measure(w_a1);
    const world_metrics m_a2 = measure(w_a2);

    const uint64_t d_a1  = deep_digest(w_a1);
    const uint64_t d_a2  = deep_digest(w_a2);
    const uint64_t d_b   = deep_digest(w_b);
    const uint64_t d_off = deep_digest(w_off);

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
    // 75 s at seed B, same Debug build, same 400-year span), and the counters
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
    check(rep_a1.prehistory_years == 400,
          "R3.4 the era pass ran the full 400-year span (report agrees with the params)");
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
    if (!report_same)
        std::printf("     run #2:          years=%lld battles=%lld conquests=%lld foundings=%lld\n",
                    static_cast<long long>(rep_a2.prehistory_years),
                    static_cast<long long>(rep_a2.prehistory_battles),
                    static_cast<long long>(rep_a2.prehistory_conquests),
                    static_cast<long long>(rep_a2.prehistory_foundings));

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILURES", failures,
                failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
