// ---------------------------------------------------------------------------
// world_copy_determinism — does a COPIED world tick byte for byte as its original?
// (BL-1034; needs Lua: build with tools/verify/build_lua_harness.{sh,bat})
// ---------------------------------------------------------------------------
// FOUND 2026-09-17 on library seed 28 (the BL-1033 lane): a `world` copied from
// the app-order base matched the BL-1031 pins at D_search and D_land, then settled
// to D_settle 18F78EB9B2B20F29 against the pinned 265C48A23E313B1A. The copy was
// faithful as DATA (the snapshot bytes agreed) and unfaithful as a PROGRAM. Any
// feature that copies a world and ticks it — a what-if preview, a save branch, a
// matrix instrument — would silently diverge from the world it copied.
//
// WHAT THIS DOES, per seed:
//   1. builds the seed in app order through harness_params.hpp
//      (`build_app_base_world`, then `apply_app_start_landscape`);
//   2. copies the world — at the base (both then take the landscape) or once the
//      landscape has landed (`--copy-at base|land`, default land) — by the copy
//      constructor, copy assignment, or a flat-binary save round trip
//      (`--copy-by construct|assign|snapshot`, default construct);
//   3. AUDITS every unordered store on `world` for iteration order, original vs
//      copy: same size, same bucket count, same key sequence. A store whose
//      sequence differs is a store a tick can read in a different order;
//   4. runs the app's validation settle on BOTH in lockstep
//      (`run_app_validation_tick`), digesting each after every tick — or after
//      every lap with `--laps` — with the BL-1031 D_settle recipe
//      (world_digest.hpp: snapshot bytes + state_hash), and names the FIRST
//      tick and lap at which the two part.
// A seed PASSES when the order audit is clean and every digest agrees. The ONLY
// thing copied is the world: both sides settle against the one registry.
//
// WHAT IT FOUND (2026-09-18, seed 28, before the fix). 13 of world's 18
// unordered stores iterated differently in a copy — MSVC's copy constructor
// re-inserts into the source's bucket count and puts each new key at the FRONT
// of its bucket, so every shared bucket comes out reversed. The first reading
// that differed was `body_mean_habitability` (budget_system.cpp), a float sum in
// `population_centres` order: one body's mean moved two ULP, and tick 0's lap 3
// (apply_budget, through the wages built on it) was the first lap whose digest
// parted. The fix is the store type, src/world/faithful_unordered_map.hpp.
//
// `--copy-by snapshot` IS A DIAGNOSTIC, NOT A CONTRACT. A save round trip drops
// what the save deliberately does not carry (derived caches, generation-time
// indexes), so a divergence there is a finding about loading, reported but not
// counted against the copy.
//
// USAGE (repo root):
//   ./build_gen/verify/world_copy_determinism.exe --seed 28
//   ./build_gen/verify/world_copy_determinism.exe --seeds all          (the 16 library seeds)
//   ./build_gen/verify/world_copy_determinism.exe --seeds 46,28 --copy-at base --laps
//   ... [--copy-by construct|assign|snapshot] [--ticks N]
// Exit 0 when every seed passes, 1 when any fails, 2 on a usage error.

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world_digest.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

/// The sixteen seed-library worlds, in library order — exactly
/// `node tools/session/seed_library.js --seed-list`, and the order of
/// player_seed_sweep's BL-1031 pin table. If the library changes, this changes.
const std::vector<std::uint32_t> k_library_seeds = {
    46, 28, 11, 31, 40, 12, 37, 13, 41, 43, 32, 10, 25, 38, 9, 0 };

enum class copy_at { base, land };
enum class copy_by { construct, assign, snapshot };

const char* name_of(copy_at a) { return a == copy_at::base ? "base" : "land"; }
const char* name_of(copy_by b)
{
    switch (b)
    {
    case copy_by::construct: return "construct";
    case copy_by::assign:    return "assign";
    case copy_by::snapshot:  return "snapshot";
    }
    return "?";
}

using clk = std::chrono::steady_clock;
double secs(clk::time_point a, clk::time_point b)
{
    return std::chrono::duration<double>(b - a).count();
}

/// Copy @p src by the chosen route. The snapshot route restores what a load
/// does not (the econ/day counters and the generation-time settlement record,
/// which only the landscape search reads) so the comparison is about order, not
/// about fields a save deliberately omits.
std::unique_ptr<world> copy_world(const world& src, copy_by by)
{
    switch (by)
    {
    case copy_by::construct:
        return std::make_unique<world>(src);   // the copy constructor, nothing after it
    case copy_by::assign:
    {
        auto dst = std::make_unique<world>();
        *dst = src;
        return dst;
    }
    case copy_by::snapshot:
    {
        auto dst = std::make_unique<world>();
        std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
        write_world_snapshot(src, ss);
        ss.seekg(0);
        if (!read_world_snapshot(*dst, ss))
        {
            std::printf("FATAL: read_world_snapshot refused a snapshot it had just written\n");
            std::exit(1);
        }
        dst->current_econ_tick = src.current_econ_tick;
        dst->current_day_tick  = src.current_day_tick;
        dst->gen_settlement    = src.gen_settlement;
        return dst;
    }
    }
    return nullptr;
}

// --- the order audit ---------------------------------------------------------

struct order_row
{
    const char* name = "";
    std::size_t size = 0;
    std::size_t buckets_orig = 0, buckets_copy = 0;
    std::size_t shared_buckets = 0;   ///< buckets in the original holding 2+ keys
    bool        same_size  = true;
    bool        same_order = true;
    std::size_t first_mismatch = 0;   ///< position of the first key that differs
};

template <class M>
order_row audit_order(const char* name, const M& a, const M& b)
{
    order_row r;
    r.name         = name;
    r.size         = a.size();
    r.buckets_orig = a.bucket_count();
    r.buckets_copy = b.bucket_count();
    for (std::size_t i = 0; i < a.bucket_count(); ++i)
        if (a.bucket_size(i) > 1)
            ++r.shared_buckets;
    r.same_size = a.size() == b.size();
    if (!r.same_size)
    {
        r.same_order = false;
        return r;
    }
    std::size_t pos = 0;
    auto ib = b.begin();
    for (auto ia = a.begin(); ia != a.end(); ++ia, ++ib, ++pos)
        if (!(ia->first == ib->first))
        {
            r.same_order     = false;
            r.first_mismatch = pos;
            break;
        }
    return r;
}

/// Every unordered store a `world` holds, directly or in a member.
std::vector<order_row> audit_world_orders(const world& a, const world& b)
{
    std::vector<order_row> rows;
    rows.push_back(audit_order("bodies", a.bodies, b.bodies));
    rows.push_back(audit_order("tiles", a.tiles, b.tiles));
    rows.push_back(audit_order("buildings", a.buildings, b.buildings));
    rows.push_back(audit_order("stockpiles", a.stockpiles, b.stockpiles));
    rows.push_back(audit_order("markets", a.markets, b.markets));
    rows.push_back(audit_order("units", a.units, b.units));
    rows.push_back(audit_order("population_centres", a.population_centres, b.population_centres));
    rows.push_back(audit_order("population_centre_tile", a.population_centre_tile, b.population_centre_tile));
    rows.push_back(audit_order("population_centre_name", a.population_centre_name, b.population_centre_name));
    rows.push_back(audit_order("land_use", a.land_use, b.land_use));
    rows.push_back(audit_order("nations", a.nations, b.nations));
    rows.push_back(audit_order("tile_to_nation", a.tile_to_nation, b.tile_to_nation));
    rows.push_back(audit_order("corporations", a.corporations, b.corporations));
    rows.push_back(audit_order("body_tile_index", a.body_tile_index, b.body_tile_index));
    rows.push_back(audit_order("body_reach_cost", a.body_reach_cost, b.body_reach_cost));
    rows.push_back(audit_order("body_market_index", a.body_market_index, b.body_market_index));
    rows.push_back(audit_order("corp_embargo_conditions", a.corp_embargo_conditions, b.corp_embargo_conditions));
    rows.push_back(audit_order("provinces.tile_province", a.provinces.tile_province, b.provinces.tile_province));
    return rows;
}

// --- the lockstep settle -----------------------------------------------------

struct lap_digests
{
    bool          all_laps = false;
    std::uint64_t d[k_app_tick_phase_count] = {};
};

void digest_after_lap(const world& w, int lap, void* ctx)
{
    auto& out = *static_cast<lap_digests*>(ctx);
    if (out.all_laps || lap == k_app_tick_phase_count - 1)
        out.d[lap] = world_state_digest(w);
}

struct seed_result
{
    std::uint32_t seed = 0;
    bool          orders_clean = true;
    int           orders_differing = 0;
    bool          land_match = true;
    bool          search_match = true;   ///< copy-at base only: the copy's walk
    int           ticks_compared = 0;
    int           first_tick = -1;       ///< first divergent tick, -1 = none
    int           first_lap  = -1;       ///< its lap (with --laps), else the tick's end
    std::uint64_t settle_orig = 0, settle_copy = 0;
    double        seconds = 0.0;
    bool pass() const { return orders_clean && land_match && search_match && first_tick < 0; }
};

std::uint64_t search_digest(const landscape_search_result& r)
{
    // The winner and its composite are what the copy's landscape is built from;
    // a disagreement here means the two sides laid different landscapes, and
    // everything after it is a different world rather than a divergent tick.
    fnv1a64 f;
    f.scalar(r.winner.corporation_count);
    f.scalar(r.winner.placement_seed);
    f.scalar(r.winner.road_tier);
    f.scalar(r.winner_score.composite);
    f.scalar(r.evaluations);
    f.scalar(r.accepted);
    return f.h;
}

seed_result run_seed(lua_state& lua, std::uint32_t seed, copy_at at, copy_by by, int ticks,
                     bool laps)
{
    seed_result res;
    res.seed = seed;
    const clk::time_point t_start = clk::now();

    world_params p{};          // the shipped start: full prehistory, as the pins
    p.seed = seed;

    app_start_world orig;
    build_app_base_world(lua, p, orig);
    const clk::time_point t_base = clk::now();

    std::unique_ptr<world> copy_owner;
    double copy_s = 0.0;
    if (at == copy_at::base)
    {
        const clk::time_point c0 = clk::now();
        copy_owner = copy_world(orig.w, by);
        copy_s = secs(c0, clk::now());
        apply_app_start_landscape(orig);
        // The copy takes the SAME landscape call against the ORIGINAL's registry,
        // so the world is the only thing that was copied.
        const shipped_landscape copy_land = apply_shipped_landscape(
            *copy_owner, orig.reg, p.seed, /*search=*/true, orig.cfg.corporation_count);
        res.search_match = search_digest(orig.land.search) == search_digest(copy_land.search);
    }
    else
    {
        apply_app_start_landscape(orig);
        const clk::time_point c0 = clk::now();
        copy_owner = copy_world(orig.w, by);
        copy_s = secs(c0, clk::now());
    }
    world& copy_w = *copy_owner;
    const clk::time_point t_land = clk::now();

    std::printf("\nseed %u  copy-at=%s copy-by=%s  (build %.1f s, landscape %.1f s, copy %.3f s)\n",
                seed, name_of(at), name_of(by), secs(t_start, t_base), secs(t_base, t_land) - copy_s,
                copy_s);

    // --- the order audit ---
    const std::vector<order_row> rows = audit_world_orders(orig.w, copy_w);
    for (const order_row& r : rows)
        if (!r.same_order)
        {
            ++res.orders_differing;
            std::printf("  ORDER  %-24s n=%-7zu buckets %zu/%zu, %zu shared; %s\n", r.name, r.size,
                        r.buckets_orig, r.buckets_copy, r.shared_buckets,
                        r.same_size ? "" : "SIZE DIFFERS");
            if (r.same_size)
                std::printf("         first key out of place at position %zu\n", r.first_mismatch);
        }
    res.orders_clean = res.orders_differing == 0;
    std::printf("  order audit: %zu unordered stores, %d iterate differently in the copy\n",
                rows.size(), res.orders_differing);
    if (at == copy_at::base)
        std::printf("  landscape search on the copy: %s the original's\n",
                    res.search_match ? "MATCHES" : "DIFFERS FROM");

    // --- D_land ---
    const std::uint64_t land_o = world_state_digest(orig.w);
    const std::uint64_t land_c = world_state_digest(copy_w);
    res.land_match = land_o == land_c;
    std::printf("  landed:   orig %016" PRIX64 "  copy %016" PRIX64 "  %s\n", land_o, land_c,
                res.land_match ? "match" : "DIFFER");

    // --- the settle, in lockstep ---
    for (int t = 0; t < ticks; ++t)
    {
        lap_digests lo, lc;
        lo.all_laps = lc.all_laps = laps;
        run_app_validation_tick(orig.w, orig.reg, t, nullptr, &digest_after_lap, &lo);
        run_app_validation_tick(copy_w, orig.reg, t, nullptr, &digest_after_lap, &lc);
        ++res.ticks_compared;
        const int first_lap = laps ? 0 : k_app_tick_phase_count - 1;
        for (int lap = first_lap; lap < k_app_tick_phase_count; ++lap)
        {
            const bool same = lo.d[lap] == lc.d[lap];
            if (laps || !same || t == ticks - 1)
                std::printf("  tick %2d lap %d %-28s orig %016" PRIX64 "  copy %016" PRIX64 "  %s\n",
                            t, lap, k_app_tick_phase_names[lap], lo.d[lap], lc.d[lap],
                            same ? "match" : "DIFFER");
            if (!same && res.first_tick < 0)
            {
                res.first_tick = t;
                res.first_lap  = lap;
            }
        }
    }
    res.settle_orig = world_state_digest(orig.w);
    res.settle_copy = world_state_digest(copy_w);
    res.seconds     = secs(t_start, clk::now());
    std::printf("  D_settle: orig %016" PRIX64 "  copy %016" PRIX64 "  -> %s  (%.1f s)\n",
                res.settle_orig, res.settle_copy, res.pass() ? "PASS" : "FAIL", res.seconds);
    if (res.first_tick >= 0)
        std::printf("  FIRST DIVERGENCE: tick %d, %s %d (%s)\n", res.first_tick,
                    laps ? "lap" : "by the end of lap", res.first_lap,
                    k_app_tick_phase_names[res.first_lap]);
    std::fflush(stdout);
    return res;
}

std::vector<std::uint32_t> parse_seeds(const std::string& csv)
{
    if (csv == "all")
        return k_library_seeds;
    std::vector<std::uint32_t> seeds;
    std::size_t at = 0;
    while (at <= csv.size())
    {
        std::size_t comma = csv.find(',', at);
        if (comma == std::string::npos)
            comma = csv.size();
        if (comma > at)
        {
            const std::string tok = csv.substr(at, comma - at);
            if (tok.find_first_not_of("0123456789") != std::string::npos)
            {
                std::printf("--seeds: '%s' is not a seed number\n", tok.c_str());
                std::exit(2);
            }
            seeds.push_back(static_cast<std::uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
        }
        at = comma + 1;
    }
    return seeds;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    copy_at at    = copy_at::land;
    copy_by by    = copy_by::construct;
    int     ticks = k_app_validation_ticks;
    bool    laps  = false;
    for (int a = 1; a < argc; ++a)
    {
        const std::string arg  = argv[a];
        const bool        more = a + 1 < argc;
        if ((arg == "--seed" || arg == "--seeds") && more)
            seeds = parse_seeds(argv[++a]);
        else if (arg == "--copy-at" && more)
        {
            const std::string v = argv[++a];
            if (v == "base")      at = copy_at::base;
            else if (v == "land") at = copy_at::land;
            else { std::printf("--copy-at base|land\n"); return 2; }
        }
        else if (arg == "--copy-by" && more)
        {
            const std::string v = argv[++a];
            if (v == "construct")     by = copy_by::construct;
            else if (v == "assign")   by = copy_by::assign;
            else if (v == "snapshot") by = copy_by::snapshot;
            else { std::printf("--copy-by construct|assign|snapshot\n"); return 2; }
        }
        else if (arg == "--ticks" && more)
            ticks = std::atoi(argv[++a]);
        else if (arg == "--laps")
            laps = true;
        else
        {
            std::printf("usage: %s --seed N | --seeds a,b,c|all [--copy-at base|land] "
                        "[--copy-by construct|assign|snapshot] [--ticks N] [--laps]\n", argv[0]);
            return 2;
        }
    }
    if (seeds.empty() || ticks < 1)
    {
        std::printf("no seeds (pass --seed N or --seeds all) or --ticks < 1\n");
        return 2;
    }

    std::printf("world_copy_determinism (BL-1034): %zu seed(s), copy-at=%s, copy-by=%s, %d settle "
                "tick(s), digests per %s\n",
                seeds.size(), name_of(at), name_of(by), ticks, laps ? "lap" : "tick");

    const clk::time_point t0 = clk::now();
    lua_state lua;   // app::m_lua: one long-lived state the scripts are (re)loaded into
    std::vector<seed_result> results;
    for (const std::uint32_t s : seeds)
        results.push_back(run_seed(lua, s, at, by, ticks, laps));

    std::printf("\n seed  verdict  ticks  orders-differing  first-divergence       D_settle orig     "
                "D_settle copy     seconds\n");
    int failed = 0;
    for (const seed_result& r : results)
    {
        char first[64] = "-";
        if (r.first_tick >= 0)
            std::snprintf(first, sizeof first, "tick %d lap %d", r.first_tick, r.first_lap);
        std::printf(" %4u  %-7s  %5d  %16d  %-21s  %016" PRIX64 "  %016" PRIX64 "  %7.1f\n", r.seed,
                    r.pass() ? "PASS" : "FAIL", r.ticks_compared, r.orders_differing, first,
                    r.settle_orig, r.settle_copy, r.seconds);
        if (!r.pass())
            ++failed;
    }
    std::printf("\n%zu seed(s): %zu pass, %d fail. Total %.1f s.\n", results.size(),
                results.size() - static_cast<std::size_t>(failed), failed, secs(t0, clk::now()));
    return failed == 0 ? 0 : 1;
}
