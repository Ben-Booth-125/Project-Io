// ---------------------------------------------------------------------------
// Planetology preference sweep (BL-167 calibration; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// A MEASUREMENT tool over resolve_preferences(), not a pass/fail check of the
// chain itself.
//
// Ben's calls (2026-07-22): the homeworld floor stays STRICT — every homeworld
// should be recognisably Earth — and a miss is handled by REJECTING AND
// REROLLING rather than clamping, so no value is ever silently overridden. The
// wizard expresses PREFERENCES, not settings, so the player narrows a sampling
// range and the seed picks within it.
//
// That design only works if three numbers hold up, and this harness measures all
// three:
//
//   R1  ACCEPTANCE. Reroll is only free if the viable region is a decent
//       fraction of the space. Reported as attempts-to-first-hit.
//
//   R2  NO DEAD LEAN. A preference the player can express that almost never
//       yields a viable world is a lie in the UI — it reads as a choice but
//       behaves as a slow reroll. Every lean on every axis is measured.
//
//   R3  VARIETY SURVIVES THE FLOOR. A strict floor is only acceptable if it
//       still produces visibly different Earths. Reported as the p05->p95 spread
//       of each endowment across accepted worlds.
//
//   C1  WHAT THE FLOOR ACTUALLY REJECTS. R1 reports the acceptance RATE; this
//       reports its CAUSE — a histogram of the clause that rejected each failed
//       draw, plus what those failed worlds became. Added 2026-08-03 as stage 1
//       of the earth-like test battery: the clauses that fire are the knobs a
//       corridor sweep should map first, and a clause that never fires is dead
//       weight in the floor. MEASUREMENT ONLY — the distribution is reported,
//       never asserted (BL-275's rule: assertions come after Ben has seen the
//       raw spread, not before).
//
// Run: planetology_sweep [draws]     (default 20000)

#include "world/components.hpp"
#include "world/planetology.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

float dep(const planetology_state& s, resource_type r)
{
    return s.endowment[static_cast<std::size_t>(r)];
}

/// Resolve one campaign and return the homeworld it produced.
planetology_state roll_home(const world_preferences& pref, uint32_t seed, resolved_world& rw_out)
{
    rw_out = resolve_preferences(pref, seed);
    body_inputs home = prototype_body(1);
    home.orbit_au = rw_out.home_orbit_au;
    return run_planetology(home, rw_out.params, seed ^ prototype_body_seed(1));
}

struct stats
{
    std::vector<float> v;
    void add(float x) { v.push_back(x); }
    float pct(float q)
    {
        if (v.empty()) return 0.0f;
        std::vector<float> c = v;
        const std::size_t k = static_cast<std::size_t>(q * static_cast<float>(c.size() - 1));
        std::nth_element(c.begin(), c.begin() + static_cast<std::ptrdiff_t>(k), c.end());
        return c[k];
    }
};

void report_spread(const char* name, stats& s)
{
    if (s.v.empty()) { std::printf("    %-20s (none)\n", name); return; }
    const float lo = s.pct(0.05f), md = s.pct(0.50f), hi = s.pct(0.95f);
    std::printf("    %-20s p05 %7.2f   median %7.2f   p95 %7.2f   spread x%.2f\n",
                name, static_cast<double>(lo), static_cast<double>(md),
                static_cast<double>(hi), static_cast<double>(lo > 0.001f ? hi / lo : 0.0f));
}

const char* lean_name(lean l)
{
    switch (l) { case lean::any: return "any"; case lean::low: return "low";
                 case lean::mid: return "mid"; case lean::high: return "high"; }
    return "?";
}

// Every axis the wizard exposes, so R2 can walk them generically.
struct axis { const char* name; lean world_preferences::*field; };
const axis k_axes[] = {
    { "star",         &world_preferences::star         },
    { "world_size",   &world_preferences::world_size   },
    { "interior",     &world_preferences::interior     },
    { "metal",        &world_preferences::metal        },
    { "ocean",        &world_preferences::ocean        },
    { "oxygen_story", &world_preferences::oxygen_story },
    { "coal_basins",  &world_preferences::coal_basins  },
    { "drawdown",     &world_preferences::drawdown     },
};

// --- C1: replaying a draw the generator threw away -------------------------
// resolve_preferences() only ever returns the draw that PASSED, so the
// rejections — the whole subject of this census — are invisible from outside
// it. They are recoverable because a draw is a pure function of (preferences,
// seed, attempt): replaying attempt k reproduces exactly what the generator saw
// and discarded. checkpoint_rng is the same splitmix64 mechanism as
// planetology.cpp's file-local `rng`, so the replay is bit-identical.
//
// This mirrors resolve_preferences' band table, and a mirrored table drifts.
// It cannot drift SILENTLY: every censused draw is cross-checked against the
// live function (a viable replay must mean attempts == 1 with identical
// parameters; a rejected replay must mean attempts >= 2), so a band edited in
// planetology.cpp fails this harness rather than quietly re-pointing the
// histogram at a space the generator no longer samples.
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

struct drawn { planetology_params params; float orbit_au = 1.0f; };

/// Attempt @p k of resolve_preferences(@p pref, @p seed), mirrored. Draw ORDER
/// is load-bearing, not just draw count — each value consumes one unit() from
/// its round's stream, so a reordering here silently decorrelates the replay.
drawn replay_draw(const world_preferences& pref, uint32_t seed, uint32_t k)
{
    checkpoint_rng ra(seed ^ (pref.roll[0] * 0x1000193u), 0xA000u ^ k);
    checkpoint_rng rb(seed ^ (pref.roll[1] * 0x1000193u), 0xB000u ^ k);
    checkpoint_rng rc(seed ^ (pref.roll[2] * 0x1000193u), 0xC000u ^ k);

    auto draw = [](checkpoint_rng& r, band b) { return b.lo + (b.hi - b.lo) * r.unit(); };

    drawn d{};
    planetology_params& p = d.params;

    // --- Round A: the System ---
    p.star_mass = draw(ra, pick(pref.star,
        {0.60f, 1.50f}, {0.60f, 0.90f}, {0.90f, 1.20f}, {1.20f, 1.50f}));
    p.home_mass = draw(ra, pick(pref.world_size,
        {0.66f, 1.48f}, {0.66f, 0.93f}, {0.93f, 1.21f}, {1.21f, 1.48f}));
    p.metallicity = draw(ra, pick(pref.metal,
        {0.30f, 2.20f}, {0.30f, 0.93f}, {0.93f, 1.57f}, {1.57f, 2.20f}));
    p.system_age_gyr = draw(ra, pick(pref.interior,
        {2.12f, 9.02f}, {6.72f, 9.02f}, {4.42f, 6.72f}, {2.12f, 4.42f}));
    p.radiogenic = draw(ra, pick(pref.interior,
        {0.57f, 1.73f}, {0.57f, 0.96f}, {0.96f, 1.34f}, {1.34f, 1.73f}));

    const float l = p.star_mass * p.star_mass * p.star_mass * std::sqrt(p.star_mass);
    d.orbit_au = std::sqrt(l) * (0.985f + (1.400f - 0.985f) * ra.unit());

    // --- Round B: Life ---
    p.home_ocean = draw(rb, pick(pref.ocean,
        {0.40f, 0.75f}, {0.40f, 0.52f}, {0.52f, 0.63f}, {0.63f, 0.75f}));
    p.oxygenation = draw(rb, pick(pref.oxygen_story,
        {0.30f, 0.91f}, {0.30f, 0.50f}, {0.50f, 0.71f}, {0.71f, 0.91f}));
    p.coal_climate = draw(rb, pick(pref.coal_basins,
        {0.00f, 1.00f}, {0.00f, 0.34f}, {0.33f, 0.67f}, {0.66f, 1.00f}));

    p.abiogenesis_ease = 1.00f; // not a preference — life on the homeworld is a given

    // --- Round C: Inheritance ---
    p.drawdown = draw(rc, pick(pref.drawdown,
        {0.00f, 0.95f}, {0.00f, 0.32f}, {0.32f, 0.63f}, {0.63f, 0.95f}));

    return d;
}

bool same_params(const planetology_params& a, const planetology_params& b)
{
    // Exact float equality is the point: this is a determinism check, and a
    // replay that is merely CLOSE has already lost the property being tested.
    return a.star_mass == b.star_mass && a.system_age_gyr == b.system_age_gyr
        && a.metallicity == b.metallicity && a.home_ocean == b.home_ocean
        && a.oxygenation == b.oxygenation && a.drawdown == b.drawdown
        && a.home_mass == b.home_mass && a.radiogenic == b.radiogenic
        && a.abiogenesis_ease == b.abiogenesis_ease && a.coal_climate == b.coal_climate;
}

/// Every clause homeworld_viability can reject on, in its own source order.
/// Ten criteria, fourteen clauses — each two-sided range rejects from either
/// end, and which END fires is the whole point of the census.
///
/// Kept in sync by the census's own unknown-reason counter: a clause added to
/// planetology.cpp and not to this table shows up as UNKNOWN and fails, rather
/// than vanishing from the histogram.
const char* const k_clauses[] = {
    "not a Cradle",
    "no civilisation",
    "O2 below breathable/combustion floor",
    "O2 too high (fire-suppressed)",
    "no liquid surface water",
    "too dry",
    "too wet",
    "too cold",
    "too hot",
    "not enough arable land",
    "gravity too low",
    "gravity too high",
    "no primary fuel",
    "iron too poor",
};
constexpr int k_clause_count = static_cast<int>(sizeof(k_clauses) / sizeof(k_clauses[0]));

int clause_index(const char* reason)
{
    for (int i = 0; i < k_clause_count; ++i)
        if (std::strcmp(reason, k_clauses[i]) == 0) return i;
    return -1;
}

} // namespace

int main(int argc, char** argv)
{
    const int draws = (argc > 1) ? std::atoi(argv[1]) : 20000;

    std::printf("=== Planetology preference sweep: %d campaigns ===\n\n", draws);

    // --- R1: acceptance with no preferences expressed --------------------------
    {
        long long total_attempts = 0;
        int gave_up = 0, worst = 0;
        stats iron, coal, oil, copper, o2, arable, ocean, temp, grav;

        for (int i = 0; i < draws; ++i)
        {
            world_preferences pref;              // every axis `any`
            resolved_world rw;
            const planetology_state s = roll_home(pref, 0xBEEF0000u + static_cast<uint32_t>(i), rw);

            total_attempts += rw.attempts;
            worst = std::max(worst, static_cast<int>(rw.attempts));
            if (rw.gave_up) ++gave_up;

            iron.add(dep(s, resource_type::iron_ore));
            coal.add(dep(s, resource_type::coal));
            oil.add(dep(s, resource_type::petroleum));
            copper.add(dep(s, resource_type::copper_ore));
            o2.add(s.o2_fraction * 100.0f);
            arable.add(s.arable_share * 100.0f);
            ocean.add(s.profile.water_fraction * 100.0f);
            temp.add(s.surface_temp_k);
            grav.add(s.v_esc_kms);
        }

        const double mean_att = static_cast<double>(total_attempts) / draws;
        std::printf("R1 ACCEPTANCE (no preferences expressed)\n");
        std::printf("    mean attempts to a viable homeworld : %.2f  (=> %.1f%% acceptance)\n",
                    mean_att, 100.0 / mean_att);
        std::printf("    worst case                          : %d attempts\n", worst);
        std::printf("    gave up (hit the cap)               : %d\n\n", gave_up);

        std::printf("R3 VARIETY across accepted homeworlds\n");
        report_spread("iron ore",   iron);
        report_spread("coal",       coal);
        report_spread("petroleum",  oil);
        report_spread("copper ore", copper);
        report_spread("oxygen %",   o2);
        report_spread("arable %",   arable);
        report_spread("ocean %",    ocean);
        report_spread("surface K",  temp);
        report_spread("escape km/s", grav);
        std::printf("\n");

        check(gave_up == 0, "R1 no campaign exhausted the reroll cap");
        check(mean_att < 4.0, "R1 a viable homeworld is found in under 4 draws on average");
        check(coal.pct(0.95f) / std::max(coal.pct(0.05f), 0.01f) > 2.5f,
              "R3 coal still varies several-fold under the strict floor");
        check(iron.pct(0.95f) / std::max(iron.pct(0.05f), 0.01f) > 1.4f,
              "R3 iron still varies under the strict floor");
    }

    // --- C1: what the floor actually rejects ----------------------------------
    // One unshaped draw per seed — the reroll loop is just this draw repeated
    // until it lands, so a single attempt per seed is the clean unbiased sample
    // of the space the generator searches.
    {
        // chain_stage carries a `count` sentinel, so its slot count maintains
        // itself. body_archetype deliberately does NOT (its ids are a
        // save-format value and the enum is append-only), so this number is
        // hand-held — and BL-209 has already appended three. A fourth would
        // silently fall out of the histogram and leave the percentages quietly
        // failing to sum, so an out-of-range archetype is counted and FAILS,
        // the same discipline the clause table uses.
        constexpr int k_archetype_count = 16; // 13 original + 3 appended by BL-209
        constexpr int k_stage_slots     = static_cast<int>(chain_stage::count) + 1;

        long long clause_hits[k_clause_count] = {};
        long long arch_hits[k_archetype_count] = {};
        long long stage_hits[k_stage_slots] = {};
        long long accepted = 0, unknown_reason = 0, mismatch = 0;
        long long unknown_archetype = 0, unknown_stage = 0;

        for (int i = 0; i < draws; ++i)
        {
            const uint32_t seed = 0xCE750000u + static_cast<uint32_t>(i);
            const world_preferences pref;          // every axis `any`

            const drawn d = replay_draw(pref, seed, 0);
            body_inputs home = prototype_body(1);
            home.orbit_au = d.orbit_au;
            const planetology_state s =
                run_planetology(home, d.params, seed ^ prototype_body_seed(1));
            const viability v = homeworld_viability(s);

            if (v.ok) ++accepted;
            else
            {
                const int c = clause_index(v.reason);
                if (c < 0) ++unknown_reason; else ++clause_hits[c];

                const int a = static_cast<int>(s.archetype);
                if (a >= 0 && a < k_archetype_count) ++arch_hits[a]; else ++unknown_archetype;
                const int st = static_cast<int>(s.died_at);
                if (st >= 0 && st < k_stage_slots) ++stage_hits[st]; else ++unknown_stage;
            }

            // The mirror's guard rail: our replay of attempt 0 must agree with
            // the live search about whether attempt 0 was the winner, and about
            // exactly which numbers it drew.
            const resolved_world rw = resolve_preferences(pref, seed);
            const bool live_took_first = (rw.attempts == 1 && !rw.gave_up);
            if (v.ok != live_took_first) ++mismatch;
            else if (v.ok && !same_params(rw.params, d.params)) ++mismatch;
        }

        const long long rejected = draws - accepted;
        std::printf("C1 REJECTION CENSUS (%d unshaped draws, one attempt per seed)\n", draws);
        std::printf("    accepted on the first draw          : %lld  (%.1f%%)\n",
                    accepted, 100.0 * static_cast<double>(accepted) / draws);
        std::printf("    rejected                            : %lld  (%.1f%%)\n\n",
                    rejected, 100.0 * static_cast<double>(rejected) / draws);

        // Sort clauses by how much work they actually do.
        int order[k_clause_count];
        for (int i = 0; i < k_clause_count; ++i) order[i] = i;
        std::sort(order, order + k_clause_count,
                  [&](int a, int b) { return clause_hits[a] > clause_hits[b]; });

        std::printf("    CLAUSE THAT REJECTED                       count    %% of rejects\n");
        int live_clauses = 0;
        for (int i = 0; i < k_clause_count; ++i)
        {
            const int c = order[i];
            if (clause_hits[c] == 0) continue;
            ++live_clauses;
            std::printf("    %-40s %8lld   %5.1f%%\n", k_clauses[c], clause_hits[c],
                        rejected ? 100.0 * static_cast<double>(clause_hits[c]) / static_cast<double>(rejected) : 0.0);
        }
        if (unknown_reason)
            std::printf("    %-40s %8lld\n", "*** UNKNOWN (clause not in table) ***", unknown_reason);

        std::printf("\n    never fired (%d of %d clauses):\n", k_clause_count - live_clauses, k_clause_count);
        for (int i = 0; i < k_clause_count; ++i)
            if (clause_hits[i] == 0) std::printf("        %s\n", k_clauses[i]);

        std::printf("\n    WHAT THE REJECTS BECAME\n");
        for (int i = 0; i < k_archetype_count; ++i)
            if (arch_hits[i])
                std::printf("    %-40s %8lld   %5.1f%%\n",
                            archetype_name(static_cast<body_archetype>(i)), arch_hits[i],
                            rejected ? 100.0 * static_cast<double>(arch_hits[i]) / static_cast<double>(rejected) : 0.0);
        if (unknown_archetype)
            std::printf("    %-40s %8lld\n", "*** ARCHETYPE PAST THE END OF THE TABLE ***", unknown_archetype);

        std::printf("\n    WHERE THEY DIED (chain gate)\n");
        for (int i = 0; i < k_stage_slots; ++i)
        {
            if (!stage_hits[i]) continue;
            const bool survived = (i == static_cast<int>(chain_stage::count));
            std::printf("    %-40s %8lld   %5.1f%%\n",
                        survived ? "(survived every gate — failed a number)"
                                 : chain_stage_name(static_cast<chain_stage>(i)),
                        stage_hits[i],
                        rejected ? 100.0 * static_cast<double>(stage_hits[i]) / static_cast<double>(rejected) : 0.0);
        }
        std::printf("\n");

        // Only the mirror's integrity is asserted. WHICH clause dominates is
        // measurement for Ben to read, not a bar for the generator to clear.
        check(mismatch == 0, "C1 the replayed draw matches the live search exactly");
        check(unknown_reason == 0, "C1 every rejection names a clause this census knows");
        check(unknown_archetype == 0 && unknown_stage == 0,
              "C1 every reject lands inside the archetype and stage tables");
    }

    // --- R2: no lean may be a dead end ----------------------------------------
    // A preference that reads as a choice but almost never yields a viable world
    // is worse than no preference at all: the player sets it and the generator
    // quietly ignores them.
    std::printf("R2 PER-LEAN COST (mean attempts; a dead lean shows up as a large number)\n");
    const int per = std::max(200, draws / 40);
    float worst_mean = 0.0f;
    const char* worst_axis = "";
    const char* worst_lean = "";

    for (const axis& a : k_axes)
    {
        std::printf("    %-14s", a.name);
        for (lean l : { lean::low, lean::mid, lean::high })
        {
            long long att = 0;
            int gave_up = 0;
            for (int i = 0; i < per; ++i)
            {
                world_preferences pref;
                pref.*(a.field) = l;
                resolved_world rw;
                roll_home(pref, 0xC0FFEE00u + static_cast<uint32_t>(i), rw);
                att += rw.attempts;
                if (rw.gave_up) ++gave_up;
            }
            const float mean = static_cast<float>(att) / per;
            if (mean > worst_mean) { worst_mean = mean; worst_axis = a.name; worst_lean = lean_name(l); }
            std::printf("  %s %5.2f%s", lean_name(l), static_cast<double>(mean),
                        gave_up ? "!" : " ");
        }
        std::printf("\n");
    }
    std::printf("\n    worst lean: %s=%s at %.2f attempts\n\n",
                worst_axis, worst_lean, static_cast<double>(worst_mean));

    check(worst_mean < 12.0f, "R2 no expressible preference costs more than 12 draws on average");

    std::printf("\n=== %s ===\n", g_fail == 0 ? "SWEEP OK" : "SWEEP PROBLEM");
    return g_fail == 0 ? 0 : 1;
}
