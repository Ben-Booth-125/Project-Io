#include "planetology.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Planetology — the ten-stage chain (BL-167). See docs/generation/PLANETOLOGY.md.
//
// DETERMINISM NOTES (the doc's § Determinism & cost names both hazards):
//
//  * No std:: distributions anywhere. std::uniform_real_distribution and friends
//    are implementation-defined and produce different sequences on libstdc++ vs
//    MSVC from identical seeds. Everything here draws from a hand-rolled
//    splitmix64 with an explicit float mapping.
//
//  * No exp/log/pow in any gate path. Those are not bit-identical across
//    compilers, libm versions, or fast-math settings, and a gate that flips on
//    someone else's machine is a miserable class of bug. The radiogenic decay
//    curve is a baked lookup table; the mass-radius relation uses sqrt and a
//    Newton cube root. std::sqrt IS safe — IEEE 754 requires it to be correctly
//    rounded, so it is bit-identical by specification.
//
//  * Per-stage RNG sub-streams, not one sequential stream. Retuning a roll in
//    S2 must not silently reshuffle every deposit in S8.
// ---------------------------------------------------------------------------

namespace {

// --- Deterministic RNG -----------------------------------------------------

uint64_t splitmix64(uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

/// A per-stage sub-stream. Seeded from (body seed, stage tag) so stages are
/// independent — adding a draw in one stage cannot perturb another.
struct rng
{
    uint64_t s;
    explicit rng(uint32_t seed, uint32_t stage_tag)
        : s(splitmix64((static_cast<uint64_t>(seed) << 32) ^ (stage_tag * 0x9E3779B1u))) {}

    /// Uniform in [0,1). Mantissa-mapped rather than modulo, so the distribution
    /// is flat and the mapping is explicit (no library dependence).
    float unit()
    {
        s = splitmix64(s);
        return static_cast<float>((s >> 40) & 0xFFFFFFull) * (1.0f / 16777216.0f);
    }

    float range(float lo, float hi) { return lo + (hi - lo) * unit(); }
};

} // namespace

// checkpoint_rng is PUBLIC (declared in planetology.hpp) so a checkpoint
// resolver outside this file can draw from the same deterministic mechanism
// rather than inventing a second one. Implemented in terms of the same
// splitmix64 core as the file-local `rng` above — one RNG mechanism, two
// thin wrappers (BL-217).
checkpoint_rng::checkpoint_rng(uint32_t seed, uint32_t stage_tag)
    : s(splitmix64((static_cast<uint64_t>(seed) << 32) ^ (stage_tag * 0x9E3779B1u)))
{
}

float checkpoint_rng::unit()
{
    s = splitmix64(s);
    return static_cast<float>((s >> 40) & 0xFFFFFFull) * (1.0f / 16777216.0f);
}

int checkpoint_rng::index(int count)
{
    if (count <= 1) return 0;
    const float u = unit();
    int i = static_cast<int>(u * static_cast<float>(count));
    return i >= count ? count - 1 : i;
}

namespace {

// --- Deterministic maths helpers -------------------------------------------

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/// Cube root by Newton iteration. Pure +,-,*,/ so it is bit-identical
/// everywhere, unlike std::cbrt.
float det_cbrt(float a)
{
    if (a <= 0.0f) return 0.0f;
    float x = a > 1.0f ? a : 1.0f;
    for (int i = 0; i < 24; ++i)
        x = (2.0f * x + a / (x * x)) / 3.0f;
    return x;
}

/// Linear interpolation into a uniformly-spaced table.
float table_lerp(const float* tbl, int n, float step, float x)
{
    if (x <= 0.0f) return tbl[0];
    const float f = x / step;
    const int   i = static_cast<int>(f);
    if (i >= n - 1) return tbl[n - 1];
    const float t = f - static_cast<float>(i);
    return tbl[i] + (tbl[i + 1] - tbl[i]) * t;
}

/// Radiogenic heat production against system age, normalised so a 4.55 Gyr
/// system reads 1.0. A baked evaluation of the explicit four-isotope sum
///   0.400·2^(dt/4.468) + 0.015·2^(dt/0.704) + 0.420·2^(dt/14.05) + 0.165·2^(dt/1.248)
/// (238-U, 235-U, 232-Th, 40-K half-lives in Gyr), sampled every 0.5 Gyr.
///
/// A single lumped half-life is a bad fit to a sum of exponentials spanning
/// 0.70-14 Gyr, which is why the four terms are kept. Production runs ~4.7x
/// higher at formation than today — the first-order reason small bodies froze
/// early and Earth did not.
constexpr float k_radiogenic[21] = {
    4.7236f, 3.6363f, 2.8740f, 2.3304f, 1.9353f, 1.6424f, 1.4214f, 1.2509f,
    1.1169f, 1.0096f, 0.9220f, 0.8492f, 0.7877f, 0.7352f, 0.6894f, 0.6489f,
    0.6133f, 0.5811f, 0.5519f, 0.5253f, 0.5008f,
};

/// Ozone column (Dobson units) against O2 as a fraction of the present level.
/// Deliberately a lookup, not a power law: the relation is strongly sublinear
/// and a power-law fit overestimates the column by ~2.3x at 10% PAL. Anchored on
/// 3D chemistry-climate results (279 DU at 1.0 PAL, ~66 at 0.1, 18 at 0.001),
/// which revised earlier 1D columns DOWN by up to 4.68x — so full UV shielding
/// needs roughly 5-10% PAL, not the ~1% older work implied.
float ozone_du(float o2_pal)
{
    static constexpr float x[6] = { 0.0f, 0.001f, 0.01f, 0.1f, 0.5f, 1.0f };
    static constexpr float y[6] = { 0.0f, 18.0f, 40.0f, 66.0f, 190.0f, 279.0f };
    for (int i = 1; i < 6; ++i)
    {
        if (o2_pal <= x[i])
        {
            const float t = (o2_pal - x[i - 1]) / (x[i] - x[i - 1]);
            return y[i - 1] + (y[i] - y[i - 1]) * t;
        }
    }
    return 279.0f;
}

// --- Small formatting helper -----------------------------------------------

std::string fmt(const char* f, float a)
{
    char buf[160];
    std::snprintf(buf, sizeof buf, f, static_cast<double>(a));
    return std::string(buf);
}

std::string fmt2(const char* f, float a, float b)
{
    char buf[192];
    std::snprintf(buf, sizeof buf, f, static_cast<double>(a), static_cast<double>(b));
    return std::string(buf);
}

// --- Stage tags for the RNG sub-streams ------------------------------------

enum : uint32_t
{
    tag_accretion = 0x51A1u,
    tag_breath    = 0xB6EAu,
    tag_green     = 0x67EEu,
};

} // namespace

// ---------------------------------------------------------------------------
// The prototype body set. Authored once, here, so make_hard_coded_world and the
// New World wizard's preview read the SAME physical facts — otherwise the charts
// a player decides against could drift from the world they get.
// ---------------------------------------------------------------------------

namespace {

struct prototype_entry
{
    body_inputs in;
    uint32_t    seed;
};

// Mercury / Earth / Luna / a stripped metallic asteroid. Order matters: it is the
// order the report and the generation screen list bodies in.
const prototype_entry k_prototypes[] = {
    { [] { body_inputs b{}; b.name = "Cinder"; b.mass_earths = 0.055f;   b.orbit_au = 0.39f; return b; }(), 0xC1D0001u },
    { [] { body_inputs b{}; b.name = "Kepler"; b.mass_earths = 1.000f;   b.orbit_au = 1.00f;
           b.is_homeworld = true; return b; }(),                                                             0xE471001u },
    { [] { body_inputs b{}; b.name = "Selene"; b.mass_earths = 0.0123f;  b.orbit_au = 1.00f;
           b.parent_mass_earths = 1.0f; b.parent_orbit_au = 0.00257f; b.eccentricity = 0.055f; return b; }(), 0x5E1E001u },
    { [] { body_inputs b{}; b.name = "Pallas"; b.mass_earths = 0.000034f; b.orbit_au = 3.05f;
           b.core_fragment = true; return b; }(),                                                            0x9A11A5u  },
};

constexpr int k_prototype_count = static_cast<int>(sizeof(k_prototypes) / sizeof(k_prototypes[0]));

} // namespace

int prototype_body_count() { return k_prototype_count; }

const body_inputs& prototype_body(int index)
{
    const int i = (index < 0) ? 0 : (index >= k_prototype_count ? k_prototype_count - 1 : index);
    return k_prototypes[i].in;
}

uint32_t prototype_body_seed(int index)
{
    const int i = (index < 0) ? 0 : (index >= k_prototype_count ? k_prototype_count - 1 : index);
    return k_prototypes[i].seed;
}

// ---------------------------------------------------------------------------
// Preferences -> parameters
// ---------------------------------------------------------------------------

namespace {

/// The sampling range for one axis at each lean. `any` spans the whole viable
/// band; the three leans partition it. Ranges are calibrated against
/// tools/verify/planetology_sweep.cpp rather than guessed — the sweep reports the
/// acceptance rate per lean, so a lean that mostly rejects is visible as a number
/// rather than as a mysteriously slow reroll.
struct band { float lo, hi; };

band pick(lean l, band any_, band low_, band mid_, band high_)
{
    switch (l)
    {
        case lean::low:  return low_;
        case lean::mid:  return mid_;
        case lean::high: return high_;
        case lean::any:  break;
    }
    return any_;
}

} // namespace

viability homeworld_viability(const planetology_state& s)
{
    const auto d = [&](resource_type r) { return s.endowment[static_cast<std::size_t>(r)]; };

    if (s.archetype != body_archetype::cradle)  return { false, "not a Cradle" };
    if (s.stage != life_stage::civilised)       return { false, "no civilisation" };
    // Humans have to breathe it and light fires in it.
    if (s.o2_fraction < 0.16f)                  return { false, "O2 below breathable/combustion floor" };
    if (s.o2_fraction > 0.30f)                  return { false, "O2 too high (fire-suppressed)" };
    // Liquid surface water, in Earth-ish proportion.
    if (s.profile.hydrology != hydrological_state::liquid) return { false, "no liquid surface water" };
    if (s.profile.water_fraction < 0.40f)       return { false, "too dry" };
    if (s.profile.water_fraction > 0.75f)       return { false, "too wet" };
    // A climate people can farm in.
    if (s.surface_temp_k < 275.0f)              return { false, "too cold" };
    if (s.surface_temp_k > 305.0f)              return { false, "too hot" };
    if (s.arable_share < 0.08f)                 return { false, "not enough arable land" };
    // Gravity a human body tolerates and a chemical rocket can leave.
    if (s.v_esc_kms < 9.5f)                     return { false, "gravity too low" };
    if (s.v_esc_kms > 13.0f)                    return { false, "gravity too high" };
    // An industrial base: at least one primary fuel, and workable iron.
    if (d(resource_type::coal) + d(resource_type::petroleum) < 0.35f)
                                                return { false, "no primary fuel" };
    if (d(resource_type::iron_ore) < 0.80f)     return { false, "iron too poor" };
    return { true, "" };
}

resolved_world resolve_preferences(const world_preferences& pref, uint32_t seed)
{
    const body_inputs proto = prototype_body(1); // the homeworld slot

    resolved_world best;
    bool have_best = false;

    // Reject and reroll. The attempt index is folded into the draw, so the search
    // is a pure function of (preferences, seed) — same inputs, same attempt count,
    // same world. The cap exists only so a pathological lean combination cannot
    // spin forever; the sweep reports real acceptance near 60-85%, so hitting it
    // means the preferences genuinely have almost no viable region.
    constexpr uint32_t max_attempts = 512;

    for (uint32_t k = 0; k < max_attempts; ++k)
    {
        // One stream per round, so bumping a later round's reroll counter cannot
        // disturb an earlier round's already-committed draw.
        rng ra(seed ^ (pref.roll[0] * 0x1000193u), 0xA000u ^ k);
        rng rb(seed ^ (pref.roll[1] * 0x1000193u), 0xB000u ^ k);
        rng rc(seed ^ (pref.roll[2] * 0x1000193u), 0xC000u ^ k);

        auto draw = [](rng& r, band b) { return r.range(b.lo, b.hi); };

        resolved_world w;
        planetology_params& p = w.params;

        // BANDS ARE THE MEASURED ALWAYS-VIABLE SPANS (2026-08-04). Each `any`
        // band below is the range over which tools/verify/earthlike_corridor.cpp
        // measured a 100% viability rate at 65 steps x 128 seeds, with every
        // other parameter at its Sol default. The three leans partition that
        // span into thirds.
        //
        // This replaces bands that were hand-calibrated against acceptance
        // alone, and it cuts both ways: the three axes that could reject
        // (oxygenation, radiogenic, home_ocean) NARROW, and the six that never
        // could (star, mass, metallicity, age, coal, drawdown) WIDEN to the
        // room they were already entitled to. More variety and fewer rerolls
        // from the same change.
        //
        // The spans are one-at-a-time slices, so they do NOT promise 100%
        // acceptance in combination — the C1 census measures what the joint
        // distribution actually does.

        // --- Round A: the System ---
        p.star_mass = draw(ra, pick(pref.star,
            {0.60f, 1.50f}, {0.60f, 0.90f}, {0.90f, 1.20f}, {1.20f, 1.50f}));
        p.home_mass = draw(ra, pick(pref.world_size,
            {0.66f, 1.48f}, {0.66f, 0.93f}, {0.93f, 1.21f}, {1.21f, 1.48f}));
        p.metallicity = draw(ra, pick(pref.metal,
            {0.30f, 2.20f}, {0.30f, 0.93f}, {0.93f, 1.57f}, {1.57f, 2.20f}));

        // Interior folds age and radiogenic endowment together: "cold and old"
        // against "young and vigorous" is one idea to a player, and splitting it
        // into two sliders would be two settings rather than one preference.
        // Age runs INVERTED against the lean — low means OLD.
        p.system_age_gyr = draw(ra, pick(pref.interior,
            {2.12f, 9.02f}, {6.72f, 9.02f}, {4.42f, 6.72f}, {2.12f, 4.42f}));
        p.radiogenic = draw(ra, pick(pref.interior,
            {0.57f, 1.73f}, {0.57f, 0.96f}, {0.96f, 1.34f}, {1.34f, 1.73f}));

        // The orbit is DERIVED, not preferred. It is placed inside the star's own
        // CONTINUOUSLY habitable band — narrower than the instantaneous one, since
        // the star brightens ~30% across its main sequence (0.976-1.428 AU for a
        // Sun, scaled by sqrt(L)).
        const float l  = p.star_mass * p.star_mass * p.star_mass * std::sqrt(p.star_mass);
        w.home_orbit_au = std::sqrt(l) * ra.range(0.985f, 1.400f);

        // --- Round B: Life ---
        p.home_ocean = draw(rb, pick(pref.ocean,
            {0.40f, 0.68f}, {0.40f, 0.49f}, {0.49f, 0.59f}, {0.59f, 0.68f}));
        p.oxygenation = draw(rb, pick(pref.oxygen_story,
            {0.30f, 0.91f}, {0.30f, 0.50f}, {0.50f, 0.71f}, {0.71f, 0.91f}));
        // coal_climate is viable across its whole range, so its band is already
        // the always-viable span and does not move.
        p.coal_climate = draw(rb, pick(pref.coal_basins,
            {0.00f, 1.00f}, {0.00f, 0.34f}, {0.33f, 0.67f}, {0.66f, 1.00f}));

        // Not a preference: rolling "life is rare" and then rejecting every
        // lifeless result is just wasted work. Life on the homeworld is a given.
        p.abiogenesis_ease = 1.00f;

        // --- Round C: Inheritance ---
        p.drawdown = draw(rc, pick(pref.drawdown,
            {0.00f, 0.95f}, {0.00f, 0.32f}, {0.32f, 0.63f}, {0.63f, 0.95f}));

        w.attempts = k + 1;

        body_inputs home = proto;
        home.orbit_au = w.home_orbit_au;
        const planetology_state s = run_planetology(home, p, seed ^ prototype_body_seed(1));

        if (homeworld_viability(s).ok)
            return w;

        if (!have_best) { best = w; have_best = true; }
    }

    best.gave_up = true;
    return best;
}

void preview_system(const planetology_params& p, float home_orbit_au, uint32_t seed,
                    std::vector<planetology_state>& out)
{
    out.clear();
    out.reserve(static_cast<std::size_t>(k_prototype_count));
    for (int i = 0; i < k_prototype_count; ++i)
    {
        body_inputs in = k_prototypes[i].in;
        // The homeworld's orbit is derived from the star, so the preview has to
        // use it too — otherwise the charts describe a world at 1 AU that the
        // generator never builds.
        if (in.is_homeworld && home_orbit_au > 0.0f)
            in.orbit_au = home_orbit_au;
        out.push_back(run_planetology(in, p, seed ^ k_prototypes[i].seed));
    }
}

// ---------------------------------------------------------------------------

const char* archetype_name(body_archetype a)
{
    switch (a)
    {
        case body_archetype::cradle:         return "Cradle";
        case body_archetype::mat_world:      return "Mat World";
        case body_archetype::boring_billion: return "Boring Billion";
        case body_archetype::graveyard:      return "Graveyard";
        case body_archetype::waterworld:     return "Waterworld";
        case body_archetype::oven:           return "Oven";
        case body_archetype::snowball_lock:  return "Snowball Lock";
        case body_archetype::relict_wet:     return "Relict Wet World";
        case body_archetype::ocean_vault:    return "Ocean Vault";
        case body_archetype::cryo_organic:   return "Cryo-Organic";
        case body_archetype::bake_out:       return "Bake-Out";
        case body_archetype::dead_rock:      return "Dead Rock";
        case body_archetype::core_fragment:  return "Core Fragment";
        case body_archetype::silent_eden:      return "Silent Eden";
        case body_archetype::rna_lock:         return "RNA Lock";
        case body_archetype::ferrotroph_world: return "Ferrotroph World";
    }
    return "Unknown";
}

const char* archetype_blurb(body_archetype a)
{
    switch (a)
    {
        case body_archetype::cradle:
            return "Has everything, and the cheap century is already spent.";
        case body_archetype::mat_world:
            return "Failed at oxygenation - and is therefore the iron-richest world here. No fire.";
        case body_archetype::boring_billion:
            return "Oxygen present and useless. Mineable, not settleable.";
        case body_archetype::graveyard:
            return "Geology outlives biology: the fuels survive, the forests did not.";
        case body_archetype::waterworld:
            return "Passes every naive habitability test and hosts no industry.";
        case body_archetype::oven:
            return "Rich in exactly one axis, lethal on every other.";
        case body_archetype::snowball_lock:
            return "Everything it had, under kilometres of ice. Access is the problem.";
        case body_archetype::relict_wet:
            return "Nearly made it. A full sedimentary record, and no fuel.";
        case body_archetype::ocean_vault:
            return "Habitable and physically unreachable in the same breath.";
        case body_archetype::cryo_organic:
            return "Industrial hydrocarbons, no oxidiser. Import the air to burn them.";
        case body_archetype::bake_out:
            return "Spectacularly alive, economically almost worthless.";
        case body_archetype::dead_rock:
            return "Unweathered ore at the surface, and nothing that ever lived.";
        case body_archetype::core_fragment:
            return "Not a world - a single deposit. Differentiation already did the work.";
        case body_archetype::silent_eden:
            return "Oceans, continents, mild weather - and land you cannot farm.";
        case body_archetype::rna_lock:
            return "Alive, and permanently simple. The genome never got a ceiling lift.";
        case body_archetype::ferrotroph_world:
            return "It never learned to split water. Iron without end, oxygen never.";
    }
    return "";
}

const char* life_stage_name(life_stage s)
{
    switch (s)
    {
        case life_stage::sterile:    return "Sterile";
        case life_stage::prebiotic:  return "Prebiotic";
        case life_stage::microbial:  return "Microbial";
        case life_stage::oxygenated: return "Oxygenated";
        case life_stage::complex:    return "Complex";
        case life_stage::land:       return "Land biosphere";
        case life_stage::civilised:  return "Civilised";
    }
    return "?";
}

const char* chain_stage_name(chain_stage s)
{
    switch (s)
    {
        case chain_stage::system:    return "System";
        case chain_stage::accretion: return "Accretion";
        case chain_stage::air:       return "Air";
        case chain_stage::engine:    return "Engine";
        case chain_stage::water:     return "Water";
        case chain_stage::spark:     return "Spark";
        case chain_stage::breath:    return "Breath";
        case chain_stage::green:     return "Green";
        case chain_stage::legacy:    return "Legacy";
        case chain_stage::spend:     return "Spend";
        case chain_stage::count:     return "";
    }
    return "";
}

const char* chain_stage_title(chain_stage s)
{
    switch (s)
    {
        case chain_stage::system:    return "What did this nebula have to work with?";
        case chain_stage::accretion: return "What is each body made of?";
        case chain_stage::air:       return "Which of them kept an atmosphere?";
        case chain_stage::engine:    return "Whose interior still runs?";
        case chain_stage::water:     return "Where does liquid water survive?";
        case chain_stage::spark:     return "Did life start?";
        case chain_stage::breath:    return "Did oxygen accumulate?";
        case chain_stage::green:     return "Did land get colonised - and can it burn?";
        case chain_stage::legacy:    return "What did life leave in the rocks?";
        case chain_stage::spend:     return "How much has already been dug up?";
        case chain_stage::count:     return "";
    }
    return "";
}

std::string format_history_date(int64_t years_before_epoch)
{
    const int64_t y = years_before_epoch;
    if (y == 0)
        return "now";

    char buf[64];

    // Everything below works on the MAGNITUDE and picks its suffix from the
    // sign, so a future date can never render as a bare negative. Nothing
    // generates one today, but the water gate emits `age - 1.2f` against an age
    // clamped only at 1.0 Gyr, so a caller setting system_age_gyr low already
    // reaches this branch — and "-200 Mya" is a worse answer than "200 Myr
    // hence". INT64_MIN is special-cased because negating it is UB.
    if (y == INT64_MIN)
        return "beyond reckoning";
    const bool past = y > 0;
    const int64_t mag = past ? y : -y;

    // Deep time, in the units the chemistry is argued in. Integer division
    // keeps the split points exact; only the final display divide is float.
    //
    // Round to Myr BEFORE choosing the unit, not after. Rounding second means
    // 999,999,999 sits just under the Gyr threshold, takes the Myr branch, and
    // then rounds UP to "1000 Mya" — a unit the table never promises, for a
    // value that should read "1.00 Gya".
    const int64_t myr = (mag + 500000LL) / 1000000LL;
    if (myr >= 1000LL)
    {
        std::snprintf(buf, sizeof buf, "%.2f %s", static_cast<double>(mag) / 1.0e9,
                      past ? "Gya" : "Gyr hence");
        return buf;
    }
    if (mag >= 1000000LL)
    {
        // Nearest Myr rather than truncated, so 251.9 Myr does not print as the
        // wrong century of the Permian.
        std::snprintf(buf, sizeof buf, "%lld %s", static_cast<long long>(myr),
                      past ? "Mya" : "Myr hence");
        return buf;
    }

    // Human time. See the table on format_history_date's declaration for why
    // the boundary sits at 10 kyr.
    if (mag >= 10000LL)
    {
        // Thousands separators: "11,650 years ago" is read at a glance, and
        // std::locale is not something a deterministic build should touch.
        char digits[32];
        std::snprintf(digits, sizeof digits, "%lld", static_cast<long long>(mag));
        std::string grouped;
        const std::size_t n = std::strlen(digits);
        for (std::size_t i = 0; i < n; ++i)
        {
            if (i > 0 && (n - i) % 3 == 0) grouped.push_back(',');
            grouped.push_back(digits[i]);
        }
        return grouped + (past ? " years ago" : " years hence");
    }

    // Recorded history: a calendar year, astronomical numbering (negative for
    // BCE), which is what the historical ladder's own lines want to read as.
    const int64_t year = campaign_epoch_year - y;
    std::snprintf(buf, sizeof buf, "%lld", static_cast<long long>(year));
    return buf;
}

// ---------------------------------------------------------------------------

planetology_state run_planetology(const body_inputs& in,
                                  const planetology_params& p,
                                  uint32_t seed)
{
    planetology_state st;
    st.endowment.fill(1.0f);

    // Deep-time stages date in Gya because that is how their chemistry is
    // argued; the conversion to the stored integer year narrows here, once, so
    // no gate path ever sees the float timestamp (BL-220).
    auto say = [&](float gya, chain_stage stage, std::string ev, std::string cons = {}) {
        st.history.push_back(history_event{ years_from_gya(gya), stage, std::move(ev), std::move(cons) });
    };
    auto die = [&](chain_stage where, body_archetype a) {
        st.died_at   = where;
        st.archetype = a;
    };

    // Records the S5-S8 mass-extinction checkpoint retrofit (BL-217): makes a
    // branch decision the biology stages already make LEGIBLE, without
    // changing the biology itself. `seed_used` is the body's own per-body
    // seed, which combined with `stage_id` and this checkpoint class's own
    // (undocumented-elsewhere, fixed) stage-tag table is enough to replay the
    // decision. `ok` is "did the chain survive this checkpoint" — the
    // per-checkpoint-class viability floor for biological mass-extinction.
    auto checkpoint = [&](chain_stage where, std::string branch, bool ok) {
        st.checkpoints.push_back(checkpoint_record{ where, std::move(branch), seed, ok });
    };

    const float age = clampf(p.system_age_gyr, 1.0f, 10.0f);

    // =======================================================================
    // S0 — SYSTEM. One nebula, one metallicity. Stellar mass fixes luminosity
    // (L ~ M^3.5, evaluated as M*M*M*sqrt(M) so it stays bit-exact) and the ice
    // line at ~2.71*sqrt(L) AU, which decides wet-vs-dry accretion.
    // =======================================================================
    const float m_star   = clampf(p.star_mass, 0.6f, 1.5f);
    const float l_star   = m_star * m_star * m_star * std::sqrt(m_star);
    const float snow_line = 2.71f * std::sqrt(l_star);

    // =======================================================================
    // S1 — ACCRETION. Formation radius against the ice line sets the volatile
    // inventory before any delivery; water is an imported veneer either way
    // (Earth's ocean is only 0.023% of its mass). Late accretion of ~0.3-1% of
    // planetary mass after core closure restores the highly siderophile elements
    // that core formation otherwise strips >99% into the core — so platinum-group
    // metals are a stochastic roll, decoupled from mass and tectonics.
    // =======================================================================
    rng r1(seed, tag_accretion);

    // The homeworld's mass is decided by the player at the Accretion stage; every
    // other body keeps its authored value. Taken here so v_esc, the heat budget
    // and the mobile-lid floor all read the same number.
    const float mass = in.is_homeworld ? clampf(p.home_mass, 0.5f, 2.0f) : in.mass_earths;

    // Computed here rather than in S2 so a body that exits early (a core
    // fragment) still reports a real instellation in its history.
    st.instellation = l_star / (in.orbit_au * in.orbit_au);

    // Mass-radius. Above ~0.05 M_E use the rocky relation (M^0.25 via two exact
    // sqrts — close enough to the published 0.27 exponent for a generator, and
    // bit-identical). Below it, self-gravity no longer sets shape: switch to
    // constant density, or the error enters the shoreline gate at the FOURTH
    // power (M^0.27 gives Mercury 0.451 R_E against a true 0.383 — 18% out).
    // Above the threshold, the rocky relation. Below it, constant density: R
    // scales as the cube root of mass against the Moon's anchor (0.0123 M_E,
    // 0.2725 R_E).
    const float radius = mass >= 0.05f
        ? std::sqrt(std::sqrt(mass))
        : 0.2725f * det_cbrt(mass / 0.0123f);

    st.v_esc_kms = 11.186f * std::sqrt(mass / radius);

    const float late_veneer = r1.range(0.4f, 1.6f);
    const bool  wet_accretion = in.orbit_au > snow_line * 0.62f;

    say(age, chain_stage::accretion,
        std::string(in.name) + (wet_accretion ? " accreted damp, outside the delivery line"
                                              : " accreted dry, inside the ice line"),
        fmt("escape velocity %.2f km/s", st.v_esc_kms));

    if (in.core_fragment)
    {
        // Differentiated on 26-Al heat, then a hit-and-run impact stripped the
        // silicate mantle. Iron meteorites are the warrant: >95 wt% Fe-Ni-Co,
        // Ni 5-25 wt%. The concentrating step already happened, so there is no
        // deposit to prospect for — the whole object is the deposit.
        st.core_exposed = true;
        die(chain_stage::accretion, body_archetype::core_fragment);
        say(age - 0.05f, chain_stage::accretion,
            "Struck and stripped - the silicate mantle is gone",
            "iron-nickel and platinum at grades no crust reaches");

        st.profile = body_profile{
            temperature_class::cold, atmosphere_class::none,
            hydrological_state::none, geological_activity::none,
            0.0f, composition_bias::metallic,
        };
        st.endowment.fill(0.0f);
        st.endowment[static_cast<std::size_t>(resource_type::iron_nickel_ore)]       = 2.6f * p.metallicity;
        st.endowment[static_cast<std::size_t>(resource_type::platinum_group_metals)] = 2.2f * late_veneer;
        st.endowment[static_cast<std::size_t>(resource_type::iron_ore)]              = 1.4f * p.metallicity;
        st.endowment[static_cast<std::size_t>(resource_type::regolith)]              = 1.0f;
        st.endowment[static_cast<std::size_t>(resource_type::stone)]                 = 0.6f;
        return st;
    }

    // =======================================================================
    // S2 — AIR. The cosmic shoreline: I proportional to v_esc^4, normalised so
    // Mars reads 1.0. This is an empirical FIT, not a derivation — energy-limited
    // theory predicts a shallower exponent and nobody has explained the observed
    // 4 — but it separates every inner-system body with air from every body
    // without, which no cheaper test does.
    //
    // Note what it does NOT need: a magnetic field. Venus has no dynamo and
    // 92 bar, and measured ion escape at Venus, Earth and Mars agrees within
    // 1-2 orders of magnitude. The dynamo is narration here, not a gate.
    // =======================================================================
    const float vr = st.v_esc_kms / 11.186f;
    const float v4 = vr * vr * vr * vr;
    st.shore = (v4 / st.instellation) * 10.534f; // normaliser puts Mars at 1.0

    // Equilibrium temperature. T_eq = 278.6 * S^0.25 * (1-A)^0.25, with both
    // quarter-powers taken as exact double sqrts.
    // Bare rock is much darker than a cloudy, wet planet: Mercury and the Moon
    // run Bond albedos near 0.07-0.12 against Earth's 0.30. Using the wet value
    // on an airless body costs ~45 K and mis-bands it a whole temperature class.
    const float albedo = (st.shore < 0.7f) ? 0.10f : 0.30f;
    const float t_eq   = 278.6f * std::sqrt(std::sqrt(st.instellation))
                                * std::sqrt(std::sqrt(1.0f - albedo));

    bool airless = false;
    if (st.shore < 0.05f)
    {
        airless = true;
        st.profile.atmosphere = atmosphere_class::none;
    }
    else if (st.shore < 0.7f)
    {
        st.profile.atmosphere = atmosphere_class::thin;
        airless = true; // thin still gates out organic compositions in Pass 4
    }
    else if (st.shore < 1.5f)
    {
        st.profile.atmosphere = atmosphere_class::thin;
        airless = true;
    }
    else
    {
        st.profile.atmosphere = atmosphere_class::moderate;
    }

    // =======================================================================
    // S3 — ENGINE. Radiogenic production scales with mantle mass, loss with
    // surface area, so shutdown time scales as density*radius — the first-order
    // reason Mars died and Earth did not. Tidal dissipation is the only way a
    // small body stays hot, and its a^-7.5 dependence IS the mechanism that
    // separates Io from Europa from Callisto.
    // =======================================================================
    // Scaled by the radiogenic endowment, NOT by metallicity: U and Th come from
    // rare r-process events, so a metal-rich system is not automatically a
    // geologically active one. Keeping them separate is both truer and a second
    // independent axis for the player to set.
    const float radio  = table_lerp(k_radiogenic, 21, 0.5f, age) * clampf(p.radiogenic, 0.3f, 2.2f);
    const float size_t_= mass / radius; // proportional to density*radius
    st.theta = radio * size_t_ * 1.0f;

    // Tidal heating for moons: strongly non-linear in orbital distance.
    if (in.parent_mass_earths > 0.0f && in.parent_orbit_au > 0.0f && in.eccentricity > 0.0f)
    {
        // Distance is normalised against the Moon (0.00257 AU) BEFORE the steep
        // exponent. Feeding raw AU into a^-7.5 underflows to ~1e-21 and blows the
        // thermal budget up by eleven orders of magnitude.
        //
        // The primary-mass term is what actually separates the cases: Jupiter is
        // 318 Earth masses, so M_p^2 alone is a ~1e5 multiplier, which is why Io
        // runs at ~2 W/m2 and the Moon is cold at a nearly identical distance.
        const float a_rel = in.parent_orbit_au / 0.00257f;
        const float a2 = a_rel * a_rel;
        const float a4 = a2 * a2;
        const float a8 = a4 * a4;
        const float tidal = 9.0f * in.parent_mass_earths * in.parent_mass_earths
                            * radius * radius * in.eccentricity * in.eccentricity
                            / (a8 > 1.0e-6f ? a8 : 1.0e-6f);
        st.theta += tidal;
    }

    // Mobile lid needs heat AND water AND mass. Convective stress rises as a
    // planet cools, so the gate is a band, not a monotone threshold — which is
    // why Venus falls out as stagnant-lid despite ample heat: it lost its water.
    const bool has_water_now = !airless && st.instellation > 0.30f && st.instellation < 1.05f;

    // THETA AS IT STOOD AT A GIVEN AGE, not as it stands now. The radiogenic
    // term decays by a factor of ~4.7 across a body's life; the tidal term does
    // not, so it carries across unscaled.
    //
    // This exists because S6's two gates ask about events billions of years in
    // the past, and asking them about PRESENT-DAY heat is a category error: an
    // 8 Gyr world is radiogenically cold today but was not cold when its
    // oxygenation actually fired. Measured by tools/verify/planetology_sweep.cpp
    // (the C1 census): it was the single largest source of homeworld rejection,
    // and it made "cold and old" cost twice what any other preference did — for
    // heat the world only lost AFTER the gate it was being judged on.
    const float tidal_theta = st.theta - radio * size_t_;
    auto theta_at = [&](float at_age) {
        const float r = table_lerp(k_radiogenic, 21, 0.5f, clampf(at_age, 0.0f, 10.0f))
                        * clampf(p.radiogenic, 0.3f, 2.2f);
        return r * size_t_ + tidal_theta;
    };
    auto mobile_lid_at = [&](float at_age) {
        const float th = theta_at(at_age);
        return (th >= 0.55f && th <= 2.2f) && mass >= 0.45f && has_water_now;
    };

    st.mobile_lid = mobile_lid_at(age); // present day — bit-identical to before

    if (st.theta < 0.10f)      st.profile.geology = geological_activity::none;
    else if (st.theta < 0.45f) st.profile.geology = geological_activity::low;
    else if (st.theta < 1.60f) st.profile.geology = geological_activity::moderate;
    else                       st.profile.geology = geological_activity::high;

    // =======================================================================
    // S4 — WATER. Do NOT model the greenhouse as a fixed offset: the outer
    // habitable edge is DEFINED by a maximum ~8 bar CO2 greenhouse needing
    // 78-93 K of warming, so a fixed increment generates frozen homeworlds
    // across the outer half of the corridor. Instead PIN surface temperature
    // where liquid water and a tectonic return path coexist, and let it run away
    // outside the limits.
    // =======================================================================
    float t_surf = t_eq;
    const bool runaway  = st.instellation > 1.0512f; // Kopparapu 2013 (Sun)
    const bool freezeout= st.instellation < 0.3438f; // maximum-greenhouse edge

    if (airless)
    {
        // No atmosphere, no heat redistribution, no greenhouse — an airless
        // surface runs colder in the mean than its equilibrium value.
        t_surf = t_eq - 20.0f;
    }
    else if (!runaway && !freezeout)
    {
        // The carbonate-silicate thermostat. Weathering rises exponentially with
        // temperature and stabilises on ~1e6 yr, fast enough to treat as
        // instantaneous at generation resolution.
        t_surf = 288.0f + (st.instellation - 1.0f) * 22.0f;
    }
    else if (runaway)
    {
        t_surf = 480.0f + (st.instellation - 1.05f) * 260.0f;
    }
    else
    {
        t_surf = t_eq - 12.0f;
    }
    st.surface_temp_k = t_surf;

    if (t_surf >= 400.0f)      st.profile.temperature = temperature_class::scorching;
    else if (t_surf >= 320.0f) st.profile.temperature = temperature_class::hot;
    else if (t_surf >= 260.0f) st.profile.temperature = temperature_class::temperate;
    else if (t_surf >= 120.0f) st.profile.temperature = temperature_class::cold;
    else                       st.profile.temperature = temperature_class::frozen;

    // Cold traps: polar shadowed regions below ~110 K hold water ice even on an
    // airless body. LCROSS measured 5.6 +/- 2.9 wt% in Cabeus regolith. This is
    // the single mapping fix that stops airless bodies reading as economically
    // empty — for a space 4X, polar ice is propellant.
    if (airless && t_surf < 300.0f && !in.core_fragment)
        st.cold_traps = clampf((300.0f - t_surf) / 180.0f, 0.0f, 1.0f);

    const bool subsurface_ocean = airless && in.ice_shell_hint > 0.0f && st.theta > 0.25f;

    if (airless)
    {
        st.profile.hydrology = st.cold_traps > 0.25f ? hydrological_state::polar_frozen
                                                     : hydrological_state::none;
        st.profile.water_fraction = 0.0f;
        say(age - 0.05f, chain_stage::air,
            "No atmosphere retained",
            fmt2("%.2f km/s against %.2f suns", st.v_esc_kms, st.instellation));
        die(chain_stage::air, subsurface_ocean ? body_archetype::ocean_vault
                                               : body_archetype::dead_rock);
    }
    else
    {
        say(age - 0.05f, chain_stage::air, "Held its air",
            st.shore > 4.0f ? "comfortably" : "marginally - it thinned for a billion years");

        if (runaway)
        {
            st.profile.hydrology = hydrological_state::none;
            st.profile.atmosphere = atmosphere_class::thick;
            say(age - 0.6f, chain_stage::water,
                "The oceans boiled; hydrogen escaped",
                "carbonate sink dead - CO2 accumulates without limit");
            die(chain_stage::water, body_archetype::oven);
        }
        else if (freezeout)
        {
            st.profile.hydrology = hydrological_state::polar_frozen;
            say(age - 0.8f, chain_stage::water,
                "Ice-albedo runaway, with no tectonic way back",
                "everything it had, under kilometres of ice");
            die(chain_stage::water, body_archetype::snowball_lock);
        }
        else if (st.theta < 0.12f)
        {
            // Held air on the shoreline, but the interior stopped refilling it.
            st.profile.hydrology = hydrological_state::polar_frozen;
            say(age - 1.2f, chain_stage::water,
                "Interior cooled; outgassing stopped and the air bled away",
                "a full early sedimentary record, and no fuel");
            die(chain_stage::water, body_archetype::relict_wet);
        }
        else
        {
            st.profile.hydrology = hydrological_state::liquid;
            st.profile.water_fraction = in.is_homeworld ? clampf(p.home_ocean, 0.20f, 0.85f)
                                                        : 0.55f;
            st.profile.atmosphere = st.shore > 3.0f ? atmosphere_class::thick
                                                    : atmosphere_class::moderate;
            say(age - 0.7f, chain_stage::water,
                "Oceans condensed; the carbonate thermostat took hold",
                fmt("surface holds near %.0f K", t_surf));
        }
    }

    st.surface_pressure_bar = airless ? 0.0f : (st.profile.atmosphere == atmosphere_class::thick ? 1.4f : 0.7f);

    // =======================================================================
    // S5 — SPARK. Serpentinisation delivers the reductant for free (up to
    // 200 mM H2 at pH 9-11 into a CO2-rich ocean — a proton gradient of the same
    // polarity every living cell uses, across Fe(Ni)S walls whose unit cells
    // match ferredoxin's active site).
    //
    // The hard part is CONCENTRATION: HCN oligomerises to purines only above
    // ~0.1 M and loses below ~0.01 M, so a dilute ocean does not work. The gate
    // is really about GEOMETRY — subaerial relief plus water, or a hydrothermal
    // pore network. This is why a waterworld with no land is a WORSE abiogenesis
    // candidate than a world with continents.
    //
    // Gating on preconditions with near-certain firing is a deliberate design
    // position, and it is a game dial wearing scientific clothing: n = 1, with
    // anthropic selection guaranteeing we observe the world where it happened.
    // =======================================================================
    const bool liquid_surface = st.profile.hydrology == hydrological_state::liquid;
    const bool cycling_venue  = liquid_surface && st.profile.water_fraction < 0.92f;
    const bool time_budget    = age >= 1.5f;

    if (st.died_at != chain_stage::count)
    {
        // Already terminated. A subsurface ocean is the one branch that can still
        // carry life past an airless verdict.
        if (subsurface_ocean && st.theta > 0.25f && time_budget)
        {
            st.stage = st.peak = life_stage::microbial;
            say(age - 1.0f, chain_stage::spark,
                "Life at the vents, under the ice",
                "chemical energy in abundance, and no way to reach it");
            checkpoint(chain_stage::spark, "subsurface life at the vents", true);
        }
        else
        {
            checkpoint(chain_stage::spark, "no subsurface life", false);
        }
    }
    else if (!(cycling_venue && st.profile.geology >= geological_activity::low && time_budget
               && rng(seed, 0x5A4Bu).unit() < clampf(p.abiogenesis_ease, 0.0f, 1.0f)))
    {
        st.stage = st.peak = life_stage::prebiotic;
        say(age - 1.0f, chain_stage::spark,
            st.profile.water_fraction >= 0.92f
                ? "Water everywhere and nowhere to concentrate it"
                : "Chemistry, but no cycling venue - life never started",
            "no fossil carbon, ever");
        die(chain_stage::spark,
            st.profile.water_fraction >= 0.92f ? body_archetype::waterworld
                                               : body_archetype::relict_wet);
        checkpoint(chain_stage::spark,
            st.profile.water_fraction >= 0.92f ? "no cycling venue - waterworld"
                                               : "no cycling venue - relict wet world",
            false);
    }
    else
    {
        st.stage = st.peak = life_stage::microbial;
        say(age - 1.0f, chain_stage::spark,
            "Serpentinising vents; abiogenesis",
            "an Fe-oxidising biosphere - banded iron begins");
        checkpoint(chain_stage::spark, "abiogenesis fired", true);
    }

    // =======================================================================
    // S6 — BREATH. Photosynthesis and respiration are exact inverses, so a
    // biosphere that respires everything it fixes produces ZERO net O2. Oxygen
    // accumulates only when reduced carbon is BURIED — 1.00 mol O2 stranded per
    // mol C_org, ~1.875 per mol pyrite S. Oxygenation is a sedimentology problem
    // and a ratio test: burial flux against a geological reductant flux that
    // decays as radiogenic heat runs down.
    //
    // It happens TWICE, a billion years apart. Collapsing them into one gate is
    // the error that makes the ladder unsatisfiable — Earth itself would fail a
    // 10%-O2 complex-life gate immediately after the GOE.
    // =======================================================================
    float ferruginous_start = 0.0f;
    float noe_epoch         = 0.0f;

    if (st.peak >= life_stage::microbial && st.died_at == chain_stage::count)
    {
        rng r6(seed, tag_breath);

        // The player's oxygenation dial. 0 = early (short banded-iron window,
        // iron-lean, but a long land-plant era and rich coal); 1 = late (an
        // unbounded window, iron-rich, but a short coal age). ONE number sets
        // both, in opposite directions, with every gate still passed.
        const float dial = clampf(p.oxygenation, 0.0f, 1.0f);

        // The biological clock runs as a FRACTION of the time available, not in
        // absolute Gyr. A younger system still climbs the whole ladder, but every
        // window is shorter — so it reaches industry with a genuinely thinner
        // endowment rather than failing outright. That keeps the homeworld
        // guarantee on the inputs (PLANETOLOGY.md § The homeworld rule) instead
        // of making the age slider able to break the campaign.
        ferruginous_start = age - 1.0f;
        const float budget    = ferruginous_start;
        const float goe_delay = budget * (0.12f + dial * 0.46f) + r6.range(-0.05f, 0.05f);
        const float goe_at    = ferruginous_start - goe_delay; // Gya

        // The reductant flux must actually fall below the burial flux. A young,
        // hot, vigorously outgassing world can never clear it.
        //
        // DELIBERATELY present-day theta, unlike the NOE gate below. This is an
        // UPPER bound on heat, and heat only falls, so today's value is a
        // conservative proxy for the epoch's. More to the point, the 2.4 constant
        // was calibrated against present-day theta: re-siting the test to the GOE
        // epoch (where theta runs ~2x higher) without re-deriving the constant
        // sends acceptance from 78% to 60% and turns 69% of rejects into Mat
        // Worlds. Measured, not assumed — planetology_sweep C1, 2026-08-04. The
        // asymmetry with the NOE gate is real and known; closing it needs a
        // defensible epoch-relative threshold, which nothing here yet supplies.
        const bool goe_fires = (goe_at > 0.15f) && (st.theta < 2.4f) && st.profile.water_fraction > 0.15f;

        if (!goe_fires)
        {
            // Mat World — the model's best inversion. It FAILED at oxygenation,
            // and that is precisely why the ferruginous window never closed.
            st.o2_fraction    = 0.001f;
            st.ferruginous_gyr = clampf(ferruginous_start, 0.0f, 4.0f);
            st.marine_anoxia_gyr = st.ferruginous_gyr * 0.8f;
            say(1.9f, chain_stage::breath,
                "Oxygen never accumulated - the reductant flux won",
                fmt("%.0f Myr of ferruginous ocean, and counting", st.ferruginous_gyr * 1000.0f));
            die(chain_stage::breath, body_archetype::mat_world);
            checkpoint(chain_stage::breath, "GOE fails - Mat World", false);
        }
        else
        {
            st.stage = st.peak = life_stage::oxygenated;
            say(goe_at, chain_stage::breath,
                "Oxygen begins to accumulate",
                fmt("methane greenhouse destroyed - a %.1f Gyr glaciation", 0.3f));
            checkpoint(chain_stage::breath, "GOE fires", true);

            // The Boring Billion lock: ferruginous oceans scavenge phosphate onto
            // iron oxyhydroxides, capping productivity, capping burial, keeping
            // the ocean ferruginous. Escape needs a tectonic nutrient shock.
            const float noe_delay = budget * (0.30f + (1.0f - dial) * 0.12f) + r6.range(-0.04f, 0.06f);
            const float noe_at    = goe_at - noe_delay;
            noe_epoch = noe_at;
            // The nutrient shock has to be available WHEN the lock breaks, so the
            // lid is tested at the NOE's own epoch rather than at present day.
            const bool  noe_fires = (noe_at > 0.10f) && mobile_lid_at(age - noe_at);

            // Banded iron stops when the deep ocean finally oxidises. The window
            // duration IS the iron endowment — not the endpoint oxygen level.
            const float bif_close = noe_fires ? noe_at : 0.0f;
            st.ferruginous_gyr    = clampf(ferruginous_start - bif_close, 0.0f, 4.0f);
            st.marine_anoxia_gyr  = clampf(st.ferruginous_gyr * 0.75f + (noe_fires ? 0.4f : 0.0f), 0.0f, 4.0f);

            if (!noe_fires)
            {
                st.o2_fraction = 0.004f;
                say(1.4f, chain_stage::breath,
                    "Phosphate locked onto iron oxyhydroxides; productivity capped",
                    "oxygen present and useless - below the ozone threshold");
                die(chain_stage::breath, body_archetype::boring_billion);
                checkpoint(chain_stage::breath, "NOE fails - Boring Billion", false);
            }
            else
            {
                st.stage = st.peak = life_stage::complex;
                st.o2_fraction = clampf(0.18f + (1.0f - dial) * 0.10f + r6.range(-0.03f, 0.05f), 0.10f, 0.34f);
                say(noe_at, chain_stage::breath,
                    "Deep ocean oxidises; banded iron ceases",
                    fmt2("%.0f Myr window -> iron x%.2f", st.ferruginous_gyr * 1000.0f,
                         1.0f + st.ferruginous_gyr * 0.85f));
                checkpoint(chain_stage::breath, "NOE fires", true);
            }
        }
    }

    // =======================================================================
    // S7 — GREEN. Land is UV-sterile until an ozone column forms. Then lignin is
    // the payload: not hydrolysable, and with a terrestrial C:P far above marine,
    // so burying plant carbon strands O2 WITHOUT starving the phosphorus cycle
    // that produced it. Self-reinforcing.
    //
    // Combustion sits at a much higher bar than land colonisation, and they are
    // SEPARABLE — which yields a genuinely novel outcome: a green world that
    // cannot burn. No charcoal, no kiln, no smelting, at any ore grade.
    // =======================================================================
    float coal_window = 0.0f;

    if (st.peak >= life_stage::complex && st.died_at == chain_stage::count)
    {
        rng r7(seed, tag_green);

        const float o2_pal    = st.o2_fraction / 0.21f;
        const float column    = ozone_du(o2_pal);
        const float land_frac = 1.0f - st.profile.water_fraction;

        if (column < 60.0f || land_frac < 0.12f)
        {
            say(0.7f, chain_stage::green,
                land_frac < 0.12f ? "No land to colonise"
                                  : "The ozone column never closed - land stayed sterilised",
                "no coal, no soil, no timber");
            die(chain_stage::green,
                land_frac < 0.12f ? body_archetype::waterworld : body_archetype::boring_billion);
            checkpoint(chain_stage::green,
                land_frac < 0.12f ? "no land to colonise" : "ozone column never closed",
                false);
        }
        else
        {
            st.stage = st.peak = life_stage::land;
            checkpoint(chain_stage::green, "land colonised", true);

            // Land colonisation lands earlier on a world that oxygenated early —
            // the other arm of the iron/coal trade.
            // Land follows the second oxygenation. Oxygenate early and the land
            // era is long (rich coal); oxygenate late and it is short — the other
            // arm of the iron/coal trade, and it falls out of the same number.
            const float greened_at = clampf(noe_epoch * 0.55f, 0.06f, 1.30f);
            say(greened_at, chain_stage::green,
                "Ozone column closes; the land greened",
                fmt("%.0f%% of the surface is land", land_frac * 100.0f));

            // Coal is NOT a general consequence of land plants. ~90% of Earth's
            // beds come from a single ~107 Myr window, and the cause is climate x
            // tectonics — everwet equatorial mires over subsiding basins. The
            // lignin-versus-fungi lag story is refuted on mass balance.
            const float subsidence = st.mobile_lid ? 1.0f : 0.25f;
            // The everwet/seasonal call is the player's; the jitter stays seeded so two
        // campaigns at the same setting still differ.
        const float climate = 0.45f + clampf(p.coal_climate, 0.0f, 1.0f) * 1.05f;
        coal_window = clampf(greened_at * 0.55f * subsidence * climate * r7.range(0.85f, 1.15f), 0.0f, 0.6f);
            st.land_burial_gyr = coal_window;

            if (coal_window > 0.10f)
                say(greened_at * 0.62f, chain_stage::green,
                    "Everwet basins over a subsiding craton",
                    fmt("%.0f Myr of peat accumulation -> coal", coal_window * 1000.0f));
            else
                say(greened_at * 0.62f, chain_stage::green,
                    "No subsiding basins - the mires never stacked",
                    "timber and soil, but almost no coal");

            // Fire. Sustained spread in natural fuels needs roughly 15-18.5% O2
            // (contested, and fuel-moisture and pressure dependent). Below it
            // there is no metallurgy at any ore grade.
            const float eff_po2 = st.o2_fraction * clampf(st.surface_pressure_bar, 0.2f, 2.0f);
            if (eff_po2 < 0.155f)
            {
                say(0.10f, chain_stage::green,
                    "And nothing on it would ever burn",
                    fmt("%.0f%% oxygen - below the combustion floor", st.o2_fraction * 100.0f));
                die(chain_stage::green, body_archetype::boring_billion);
                checkpoint(chain_stage::green, "green world cannot burn", false);
            }
            else
            {
                checkpoint(chain_stage::green, "combustion threshold cleared", true);
            }

            // Above ~21% fire suppresses forest cover — a high-oxygen world is
            // grassland-biased and timber-poor.
            st.arable_share = clampf(land_frac * (0.28f - (st.o2_fraction - 0.21f) * 0.45f)
                                     * (st.mobile_lid ? 1.0f : 0.72f), 0.02f, 0.40f);
        }
    }

    // =======================================================================
    // C -> D — ENDEMISM. What this biosphere evolved that grows nowhere else.
    //
    // The B -> C joint above answers "what can this world BUILD with". This one
    // answers "what is this world worth CARRYING FROM". They are different
    // economics: an industrial good is valuable because it is useful, an endemic
    // good is valuable because it is somewhere else.
    //
    // A biosphere does not spread a species evenly. It evolves it in one band, in
    // one region, and nowhere else — which is why nutmeg was worth an ocean
    // crossing and sand never was. So each good gets a latitude band (which
    // climate it wants) AND a longitude sector (where it actually arose). Two
    // worlds can both have temperate grassland and only one of them grow tobacco.
    // =======================================================================
    if (st.stage >= life_stage::land)
    {
        rng rd(seed, 0xC2D0u);

        // Band assignment is fixed per good — a tundra pelt animal is not going to
        // turn up in the tropics — but WHETHER a world evolved it at all, and
        // where, is rolled. That roll is the scarcity the whole trade rests on.
        struct candidate { resource_type good; float lo, hi, chance; };
        static constexpr candidate k_candidates[] = {
            { resource_type::tobacco, 0.20f, 0.55f, 0.70f }, // temperate/subtropical grassland
            { resource_type::spices,  0.00f, 0.25f, 0.60f }, // tropical wetland and forest
            { resource_type::coffee,  0.15f, 0.45f, 0.60f }, // subtropical highland forest
            { resource_type::furs,    0.60f, 0.95f, 0.65f }, // subpolar and polar tundra
        };

        for (const candidate& c : k_candidates)
        {
            if (rd.unit() > c.chance)
                continue; // this biosphere simply never produced it

            endemic_good e;
            e.good          = c.good;
            e.lat_lo        = c.lo;
            e.lat_hi        = c.hi;
            e.sector_centre = rd.unit();                 // anywhere around the globe
            e.sector_width  = rd.range(0.10f, 0.30f);    // one region, not a belt
            e.richness      = rd.range(0.70f, 1.60f);
            st.endemics.push_back(e);
        }

        if (!st.endemics.empty())
        {
            std::string names;
            for (const endemic_good& e : st.endemics)
            {
                if (!names.empty()) names += ", ";
                switch (e.good)
                {
                    case resource_type::tobacco: names += "tobacco"; break;
                    case resource_type::spices:  names += "spices";  break;
                    case resource_type::coffee:  names += "coffee";  break;
                    case resource_type::furs:    names += "furs";    break;
                    default: break;
                }
            }
            say(0.02f, chain_stage::legacy,
                "Species that spread nowhere else: " + names,
                "worth more the further you carry them");
        }
    }

    // =======================================================================
    // S8 — LEGACY. The B -> C joint. A biosphere physically manufactures the
    // industrial base; each product has its own chemical gate. This is where the
    // whole chain gets spent.
    // =======================================================================
    {
        auto& e = st.endowment;
        auto at = [&](resource_type r) -> float& { return e[static_cast<std::size_t>(r)]; };

        // Clay is a phyllosilicate produced by AQUEOUS alteration, so it needs
        // liquid water at some epoch. Polar cold-trapped ice does not count —
        // that is why Mars has abundant clay and the Moon has essentially none.
        const bool has_water_history = st.profile.hydrology == hydrological_state::liquid
                                       || st.archetype == body_archetype::relict_wet
                                       || st.archetype == body_archetype::snowball_lock
                                       || st.peak >= life_stage::microbial;
        const float metal = clampf(p.metallicity, 0.3f, 2.2f);

        // --- Iron. Never zero: a magmatic and impact-delivered floor on any
        // differentiated body. Then the banded-iron term, gated on an
        // Fe-OXIDISING BIOSPHERE rather than on free oxygen (photoferrotrophy
        // and abiotic UV photo-oxidation both deposit BIF with no O2 at all).
        // Then a SECOND, independent oxic-weathering upgrade that takes ~30% Fe
        // taconite to 60-65% direct-shipping ore.
        // The window term is normalised against a nominal 2 Gyr rather than taken
        // as raw Gyr, and the ceiling sits above the reachable maximum. Both
        // matter: with a tight clamp the whole dial saturates and two very
        // different histories hand back the same number, which is exactly the
        // failure the harness caught.
        float iron = 0.25f * metal;
        if (st.peak >= life_stage::microbial) iron += (st.ferruginous_gyr / 2.0f) * 1.60f;
        if (st.o2_fraction > 0.05f)           iron *= 1.0f + clampf(st.o2_fraction * 2.2f, 0.0f, 0.55f);
        at(resource_type::iron_ore) = clampf(iron, 0.15f, 4.0f);

        // --- Petroleum. Marine algal and planktonic — Type I/II kerogen under
        // bottom water below 0.2 mL/L O2, then the 60-120 C oil window. Needs
        // only a MICROBIAL-grade biosphere, which is the sharpest divergence
        // from coal in the whole mapping.
        at(resource_type::petroleum) = st.peak >= life_stage::oxygenated
            ? clampf(st.marine_anoxia_gyr * 0.8f * (0.4f + st.profile.water_fraction), 0.0f, 2.6f)
            : 0.0f;

        // --- Coal and peat. Hard zero below a land biosphere.
        at(resource_type::coal) = st.peak >= life_stage::land
            ? clampf(coal_window * 4.2f, 0.0f, 2.8f) : 0.0f;
        at(resource_type::peat) = st.stage >= life_stage::land ? 1.0f : 0.0f;

        // --- LIVING resources: these track the CURRENT biosphere, so a dead
        // world loses them while keeping its coal. That split is the graveyard.
        at(resource_type::timber) = st.stage >= life_stage::land
            ? clampf(1.4f - (st.o2_fraction - 0.21f) * 3.0f, 0.2f, 1.8f) : 0.0f;
        at(resource_type::agricultural_produce) = st.stage >= life_stage::land
            ? clampf(st.arable_share * 4.0f, 0.0f, 1.8f) : 0.0f;

        // --- Copper. Porphyry needs sustained subduction (arc magmas carrying
        // 2-6 wt% H2O); no mobile lid, no porphyry, and porphyry is most of the
        // world's copper. Then a supergene multiplier that needs free oxygen.
        // Crustal Cu is ~28 ppm against a 0.15-1% cutoff — a 150-350x
        // concentration factor, against iron's mere 5-12x. That asymmetry is why
        // iron is near-universal and copper is the chokepoint.
        float cu = st.mobile_lid ? 1.25f * metal : 0.18f * metal;
        if (st.o2_fraction > 0.10f) cu *= 1.9f;
        at(resource_type::copper_ore) = clampf(cu, 0.05f, 2.8f);

        // --- Rare earths. Lithophile, so never zero: a floor from one-shot
        // magma-ocean fractionation (the KREEP route, no tectonics required),
        // rising with crustal reworking.
        at(resource_type::rare_earth_ore) = clampf(0.35f + (st.mobile_lid ? 0.9f : 0.15f)
                                                  + (st.theta > 0.5f ? 0.25f : 0.0f), 0.2f, 2.2f)
                                            * metal;

        // --- The late-veneer channel: stochastic, decoupled from mass and
        // tectonics, so a small dead world can be platinum-rich.
        at(resource_type::platinum_group_metals) = clampf(0.5f * late_veneer * metal, 0.1f, 2.4f);
        at(resource_type::iron_nickel_ore)       = st.core_exposed ? 2.6f : 0.25f * metal;

        // --- Sedimentary. Clay REQUIRES liquid water at some epoch — clays are
        // phyllosilicates from aqueous alteration. Mars has abundant clay; the
        // Moon has essentially none. Sand needs an atmosphere or water to sort
        // and transport it; an airless body gets impact regolith instead, which
        // is a different material entirely.
        at(resource_type::clay) = has_water_history ? 1.0f : 0.0f;
        at(resource_type::sand) = (!airless || st.profile.hydrology != hydrological_state::none) ? 1.0f : 0.0f;
        at(resource_type::regolith) = airless ? 1.35f : 0.4f;
        at(resource_type::stone)  = 1.0f;
        at(resource_type::silica) = 1.0f * metal;

        // --- Water. Surface hydrology on wet bodies; polar cold traps on airless
        // ones. This is what stops airless bodies reading as economically empty.
        at(resource_type::water) = st.profile.hydrology == hydrological_state::liquid
            ? 1.0f
            : clampf(st.cold_traps * 1.6f, 0.0f, 1.6f);

        // Refined goods are made, not mined.
        at(resource_type::steel)        = 0.0f;
        at(resource_type::refined_fuel) = 0.0f;
        at(resource_type::food_rations) = 0.0f;
    }

    // =======================================================================
    // S9 — SPEND. Homeworld only. The inversion is the point: a richer endowment
    // industrialises EARLIER and is therefore MORE drawn down at campaign start.
    // Depletion multiplies the ACCESSIBLE fraction, never the formed fraction —
    // the ore is still there, the cheap ore is not.
    //
    // This is the generated, in-fiction reason a corporation goes to space.
    // =======================================================================
    if (in.is_homeworld && st.died_at == chain_stage::count && st.peak >= life_stage::land)
    {
        st.stage = st.peak = life_stage::civilised;
        st.archetype = body_archetype::cradle;
        st.drawdown  = clampf(p.drawdown, 0.0f, 0.95f);

        const float keep = 1.0f - st.drawdown * 0.75f;
        st.endowment[static_cast<std::size_t>(resource_type::coal)]      *= keep;
        st.endowment[static_cast<std::size_t>(resource_type::petroleum)] *= keep;
        st.endowment[static_cast<std::size_t>(resource_type::iron_ore)]  *= (1.0f - st.drawdown * 0.45f);
        st.endowment[static_cast<std::size_t>(resource_type::copper_ore)]*= (1.0f - st.drawdown * 0.55f);

        say(0.0f, chain_stage::spend,
            "An industrial era, already spent",
            fmt("%.0f%% of the accessible surface endowment is gone", st.drawdown * 100.0f));
    }
    else if (st.died_at == chain_stage::count)
    {
        // Survived every gate but never reached industry — a living world with
        // nobody on it.
        st.archetype = st.peak >= life_stage::land ? body_archetype::cradle
                                                   : body_archetype::mat_world;
    }

    return st;
}
