// ---------------------------------------------------------------------------
// Life reroll moves the world (BL-1339) — no SDL / Lua
// ---------------------------------------------------------------------------
// Ben, playing 2026-10-16: "It looks like reroll in life doesn't reroll the
// seed. I'm still getting similar landmasses either way." He was right. The
// Life round's Reroll bumps `world_preferences::roll[1]`, whose only consumer
// was `resolve_preferences`' stream `rb` — three draws: `home_ocean`,
// `oxygenation`, `coal_climate`. The continents pass is seeded off the body
// seed and reads `theta` / `mobile_lid`, both of which the System round owns,
// so the plates, the drift and the whole `height_bias` field were bit-identical
// across any number of Life rolls. Sea level and the oxygen story moved; the
// land did not.
//
// `home_land_roll_fold` (planetology.hpp) now folds `roll[1]` into the two
// seeds that lay the home body's land down — the continents seed and the
// BL-276 acceptance gate's tile seed — and nothing else. This harness is the
// standing guard on the division of labour that implies:
//
//   R1  DETERMINISM. The same seed and the same roll vector give the same
//       world, twice, byte for byte. A roll is part of the world's identity,
//       not a clock.
//   R2  THE LAND MOVES. Eight successive Life rolls give eight DISTINCT
//       land digests, on the shipped path (generate_home_surface_preview, the
//       function the wizard's map and make_hard_coded_world's Kepler share).
//   R3  AND IT MOVES VISIBLY. Not just a different digest: the land/sea mask
//       differs on a double-digit share of tiles against roll 0. A reroll that
//       nudged one coastline would pass R2 and fail a player, so the bar is on
//       the map, not the hash.
//   R4  THE PLATES ARE REDRAWN. The `height_bias` field and the plate_id
//       assignment both differ per roll — the continents pass, not only the
//       Pass-1 noise, is taking the reroll.
//   R5  THE SYSTEM ROUND IS UNTOUCHED. Every value the player accepted on
//       round 1 — star mass, world mass, metallicity, system age, radiogenic
//       index, the derived orbit, and the chain's `theta` / `mobile_lid` — is
//       bit-identical across the rolls, and so are the two things CONTINENTS.md
//       § Consequence, not dice derives from that budget: the PLATE COUNT and
//       the DRIFT SPEED. What the Life roll moves is where the plates sit,
//       which way they drift and which are oceanic.
//   R6  ZERO IS THE UNREROLLED WORLD. `home_land_roll_fold` returns exactly 0
//       at `roll[1] == 0` — the fold is an XOR of zero, so every fixture,
//       golden and curated seed keeps its world — and a distinct non-zero
//       value for each of the first 64 rolls, so no two rolls collide into one
//       world.
//
// R5 is conditioned honestly: `resolve_preferences` folds the ATTEMPT index
// into all three streams, so a Life roll that changes the attempt count changes
// the System draws too. That is pre-existing (it was already true of the three
// Life draws) and is reported per roll rather than asserted away; the equality
// bar is taken over the rolls that resolved at the same attempt count, and the
// harness fails if fewer than half of them did.
//
// Run: life_reroll
// ---------------------------------------------------------------------------

#include "world/continents.hpp"
#include "world/hard_coded_world.hpp"
#include "world/planetology.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
int g_checks   = 0;

void check(bool ok, const std::string& what)
{
    ++g_checks;
    if (!ok) ++g_failures;
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

// FNV-1a over raw bytes. A digest, not a golden: nothing here pins a literal,
// so the harness cannot drift into a re-bless.
struct digest
{
    uint64_t h = 0xCBF29CE484222325ull;
    void feed(const void* p, std::size_t n)
    {
        const auto* b = static_cast<const unsigned char*>(p);
        for (std::size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 0x100000001B3ull; }
    }
    template <typename T> void feed(const T& v) { feed(&v, sizeof(T)); }
};

constexpr int k_cells = home_grid_width * home_grid_height;

/// One world, as the three rounds divide it.
struct observed
{
    // --- the Life round's subject: the land ---
    uint64_t            land_digest  = 0; ///< substrate + landform + height, every tile
    uint64_t            bias_digest  = 0; ///< continents' height_bias field
    uint64_t            plate_digest = 0; ///< continents' plate_id assignment
    std::vector<uint8_t> is_land;         ///< per-tile land/sea, for the R3 map diff

    // --- the System round's subject: the physics ---
    float    star_mass = 0.0f, home_mass = 0.0f, metallicity = 0.0f;
    float    age_gyr = 0.0f, radiogenic = 0.0f, orbit_au = 0.0f;
    float    theta = 0.0f;
    bool     mobile_lid = false;
    int      plate_count = 0;
    float    drift_speed = 0.0f; ///< DERIVED from theta, per CONTINENTS.md.
    bool     drift_speed_held = false; ///< Every plate drifts at exactly that speed.
    uint32_t attempts = 0;
};

observed run(uint32_t seed, uint32_t life_roll)
{
    observed o;
    world_params params;
    params.seed = seed;
    params.preferences.roll[1] = life_roll;

    // THE SHIPPED PATH, not a mirror of it: generate_home_surface_preview is
    // the same resolve -> chain -> Continents -> acceptance gate -> tiles
    // make_hard_coded_world runs for Kepler (home_surface_bench pins that
    // parity tile for tile), so what moves here is what moves in the game.
    world scratch;
    const entity_id probe = scratch.create_entity();
    const std::vector<entity_id> tiles =
        generate_home_surface_preview(scratch, probe, params);

    digest land;
    o.is_land.reserve(tiles.size());
    for (const entity_id t : tiles)
    {
        const tile_component& tc = scratch.tiles.at(t);
        land.feed(tc.substrate);
        land.feed(tc.landform);
        land.feed(tc.height);
        o.is_land.push_back(tc.substrate == terrain_substrate::ocean ? 0u : 1u);
    }
    o.land_digest = land.h;

    // The chain and the continents pass again, for the per-round readings the
    // preview does not hand back. Same seeds the preview used by construction
    // (hard_coded_world.cpp's plan_body / generate_home_surface_preview).
    const resolved_world rw = resolve_preferences(params.preferences, params.seed);
    body_inputs home = prototype_body(1);
    home.orbit_au = rw.home_orbit_au;
    const uint32_t body_seed = params.seed ^ prototype_body_seed(1);
    const planetology_state st = run_planetology(home, rw.params, body_seed);
    const continent_state cs =
        run_continents(st, home_grid_width, home_grid_height,
                       body_seed ^ 0xC0117E57u ^ home_land_roll_fold(params.preferences));

    digest bias, plate;
    for (int i = 0; i < k_cells; ++i)
    {
        bias.feed(cs.height_bias[static_cast<std::size_t>(i)]);
        plate.feed(cs.plate_id[static_cast<std::size_t>(i)]);
    }
    o.bias_digest  = bias.h;
    o.plate_digest = plate.h;

    o.star_mass   = rw.params.star_mass;
    o.home_mass   = rw.params.home_mass;
    o.metallicity = rw.params.metallicity;
    o.age_gyr     = rw.params.system_age_gyr;
    o.radiogenic  = rw.params.radiogenic;
    o.orbit_au    = rw.home_orbit_au;
    o.attempts    = rw.attempts;
    o.theta       = st.theta;
    o.mobile_lid  = st.mobile_lid;
    o.plate_count = static_cast<int>(cs.plates.size());
    // SPEED, NOT DIRECTION. Direction is exactly what a Life roll should move
    // (a seeded pick from the 8-way compass table); speed is the thermal
    // budget's, and must not move. The compass carries 0.7071 on the diagonals,
    // so the per-plate speed is the vector magnitude, compared against the
    // value CONTINENTS.md derives from theta rather than against another roll
    // -- which pins the DERIVATION, not merely the agreement.
    o.drift_speed = std::clamp(o.theta * 0.6f, 0.15f, 1.2f);
    o.drift_speed_held = !cs.plates.empty();
    for (const tectonic_plate& pl2 : cs.plates)
    {
        const float mag = std::sqrt(pl2.drift_col * pl2.drift_col
                                    + pl2.drift_row * pl2.drift_row);
        if (std::fabs(mag - o.drift_speed) > 1.0e-3f) o.drift_speed_held = false;
    }
    return o;
}

bool system_values_match(const observed& a, const observed& b)
{
    return a.star_mass == b.star_mass && a.home_mass == b.home_mass
        && a.metallicity == b.metallicity && a.age_gyr == b.age_gyr
        && a.radiogenic == b.radiogenic && a.orbit_au == b.orbit_au
        && a.theta == b.theta && a.mobile_lid == b.mobile_lid
        && a.plate_count == b.plate_count && a.drift_speed == b.drift_speed
        && a.drift_speed_held && b.drift_speed_held;
}

double land_diff_share(const observed& a, const observed& b)
{
    if (a.is_land.size() != b.is_land.size() || a.is_land.empty()) return 0.0;
    std::size_t diff = 0;
    for (std::size_t i = 0; i < a.is_land.size(); ++i)
        if (a.is_land[i] != b.is_land[i]) ++diff;
    return static_cast<double>(diff) / static_cast<double>(a.is_land.size());
}

constexpr uint32_t k_rolls = 8;

} // namespace

int main()
{
    std::printf("=== BL-1339  Life reroll moves the world ===\n\n");

    // --- R6: the fold itself -------------------------------------------------
    {
        world_preferences p;
        p.roll[1] = 0;
        check(home_land_roll_fold(p) == 0u,
              "R6 roll 0 folds to 0 — the unrerolled world is untouched, bit for bit");
        std::set<uint32_t> folds;
        bool all_new = true;
        for (uint32_t r = 1; r <= 64; ++r)
        {
            p.roll[1] = r;
            const uint32_t f = home_land_roll_fold(p);
            if (f == 0u || !folds.insert(f).second) all_new = false;
        }
        check(all_new, "R6 the first 64 rolls fold to 64 distinct non-zero values");
        // roll[0] (System) must not reach this fold — it has its own stream.
        p.roll[1] = 0; p.roll[0] = 9; p.roll[2] = 9;
        check(home_land_roll_fold(p) == 0u,
              "R6 the fold reads roll[1] only — System and Spend rolls do not reach the land");
    }

    int seeds_run = 0;
    for (const uint32_t seed : { 0u, 7u, 1234u })
    {
        std::printf("\n--- campaign seed %u ---\n", seed);
        std::vector<observed> obs;
        obs.reserve(k_rolls);
        for (uint32_t r = 0; r < k_rolls; ++r) obs.push_back(run(seed, r));

        std::printf("  roll  land digest        height_bias        plate_id          "
                    " attempts plates theta  land!=roll0\n");
        for (uint32_t r = 0; r < k_rolls; ++r)
        {
            const observed& o = obs[r];
            std::printf("  %4u  %016llx  %016llx  %016llx  %6u %5d  %5.3f  %6.2f%%\n",
                        r,
                        static_cast<unsigned long long>(o.land_digest),
                        static_cast<unsigned long long>(o.bias_digest),
                        static_cast<unsigned long long>(o.plate_digest),
                        o.attempts, o.plate_count, static_cast<double>(o.theta),
                        100.0 * land_diff_share(obs[0], o));
        }

        // --- R1: the same roll reproduces its world ---------------------------
        {
            const observed again = run(seed, 3);
            check(again.land_digest == obs[3].land_digest
                      && again.bias_digest == obs[3].bias_digest
                      && again.plate_digest == obs[3].plate_digest,
                  "R1 same seed + same roll -> the same world, twice");
        }

        // --- R2 / R4: N rolls, N worlds ---------------------------------------
        {
            std::set<uint64_t> land, bias, plate;
            for (const observed& o : obs)
            {
                land.insert(o.land_digest);
                bias.insert(o.bias_digest);
                plate.insert(o.plate_digest);
            }
            check(land.size() == k_rolls,
                  "R2 " + std::to_string(k_rolls) + " Life rolls -> "
                      + std::to_string(land.size()) + " distinct land rasters");
            check(bias.size() == k_rolls,
                  "R4 ... and " + std::to_string(bias.size())
                      + " distinct height_bias fields");
            check(plate.size() == k_rolls,
                  "R4 ... and " + std::to_string(plate.size())
                      + " distinct plate assignments");
        }

        // --- R3: the map a player looks at actually changed --------------------
        {
            double worst = 1.0;
            for (uint32_t r = 1; r < k_rolls; ++r)
                worst = std::min(worst, land_diff_share(obs[0], obs[r]));
            std::printf("       least-changed roll moves %.1f%% of the land/sea mask\n",
                        100.0 * worst);
            check(worst >= 0.10,
                  "R3 every roll moves >= 10% of the land/sea mask against roll 0");
        }

        // --- R5: the System round keeps what the player accepted ---------------
        {
            int comparable = 0, matched = 0;
            for (uint32_t r = 1; r < k_rolls; ++r)
            {
                if (obs[r].attempts != obs[0].attempts) continue; // see the header
                ++comparable;
                if (system_values_match(obs[0], obs[r])) ++matched;
            }
            std::printf("       %d of %u rolls resolved at the same attempt count\n",
                        comparable, k_rolls - 1);
            check(comparable * 2 >= static_cast<int>(k_rolls - 1),
                  "R5 at least half the rolls are comparable (attempt count held)");
            check(comparable > 0 && matched == comparable,
                  "R5 star/mass/metallicity/age/radiogenic/orbit/theta/mobile_lid"
                  " + plate count + drift speed identical across Life rolls");
        }
        ++seeds_run;
    }

    std::printf("\n%d checks over %d campaign seeds, %d failed\n",
                g_checks, seeds_run, g_failures);
    std::printf("%s\n", g_failures == 0 ? "LIFE REROLL OK" : "LIFE REROLL FAILED");
    return g_failures == 0 ? 0 : 1;
}
