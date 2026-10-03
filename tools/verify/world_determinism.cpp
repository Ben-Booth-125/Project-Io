// ---------------------------------------------------------------------------
// Headless world-determinism harness (BL-114; no SDL / ImGui; Lua for the works table)
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
//       R3 builds the DEFAULT world (epoch_year 1960 since the epoch flip,
//       BL-1047; prehistory_years 400) and asserts:
//         3.1 same seed + prehistory on, built twice -> identical world;
//         3.2 a different seed + prehistory on -> a different world;
//         3.3 prehistory on vs off at the SAME seed -> a different world, so the
//             pass is demonstrably not a silent no-op;
//         3.4 the pass reports having run, and having done something;
//         3.5 the sim's own reported outcome (battles/conquests/foundings) is
//             identical across the two same-seed runs;
//         3.8 THE EPOCH IS A CALENDAR ALONE (BL-1047): the same seed at epoch
//             1900 builds the byte-identical world the default 1960 epoch does
//             (epoch 0 until it was retired, BL-1114).
//
//       R3 deliberately uses the REAL default of 400 years, not a shortened
//       run: a determinism guarantee measured over a span nobody ships is not
//       the guarantee. It is why this harness is no longer cheap — see the
//       timing lines it prints.
//
// The process exits non-zero if any assertion FAILs. Needs a live Lua state for the works table
// (bash tools/verify/build_lua_harness.sh world_determinism; run from the repo root). Earlier
// rows still build bare worlds; the prehistory-ON rows (R3) build with scripts/works.lua.

#include "world/components.hpp"
#include "world/era_minus_one.hpp" // BL-754: the per-pass clock rides the fixture
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/works_roster.hpp"
#include "world/history_sim.hpp"   // BL-1009: polity::navy_stock, exploration_output
#include "world/settlement.hpp"    // BL-1009: region stocks on world::gen_settlement
#include "world/stockpile_budget.hpp" // BL-1042: the stockpile folded into the region digest
#include "world/world.hpp"
#include "world_deep_digest.hpp" // world_metrics, measure, deep_digest (BL-1084: shared)

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace {

// `world_metrics`, `measure` and `deep_digest` live in world_deep_digest.hpp
// (BL-1084), shared with world_cursor_equivalence; their comments moved with them.

/// A human-readable line of what the BL-1009 folds saw, so a moved digest can be
/// read as a moved FIELD. Reported, never asserted.
void print_coverage(const char* what, const world& w, const era_minus_one_fixture& fx)
{
    std::size_t road_tiles = 0;
    std::map<int, int> road_tier;
    for (const auto& [tid, tc] : w.tiles)
        if (tc.road_level != 0) { ++road_tiles; ++road_tier[tc.road_level]; }
    std::size_t lane_tiles = 0; // BL-1098: the sea lanes the deep digest folds
    for (const auto& [tid, tc] : w.tiles)
        if (tc.lane_level != 0) ++lane_tiles;

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

    std::printf("     coverage %-14s sentiment rows=%zu | road tiles=%zu (T1 %d, T2 %d, T3 %d) | lane tiles=%zu\n",
                what, w.sentiment.pairs.size(), road_tiles,
                road_tier[1], road_tier[2], road_tier[3], lane_tiles);
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

/// BL-1042: the Industrialisation span is OFF in every world this harness builds, so
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
/// THE REAL WORKS TABLE (BL-1149 review, 2026-10-01): the prehistory-ON worlds
/// are built with scripts/works.lua, so the Industrialisation span's scale
/// credit, stream and stockpile run as they ship (R3.7 reads that stockpile).
/// Loaded once in main; the prehistory-OFF and legacy rows above stay bare.
works_registry g_works;

world timed_world(const world_params& p, generation_report* rep, const char* what,
                  era_minus_one_fixture& fx)
{
    const auto t0 = std::chrono::steady_clock::now();
    world w = make_hard_coded_world(p, rep, {}, nullptr, &g_works, &fx);
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
        std::printf("         span:   %lld -> %lld  (tick bands %d)\n",
                    static_cast<long long>(fx.params.start_year),
                    static_cast<long long>(fx.params.stop_year),
                    fx.params.tick_band_count);
    }
    std::fflush(stdout);
    return w;
}

} // namespace

int main()
{
    lua_state lua;
    lua.load("scripts/works.lua");
    g_works.load_from_lua(lua);
    std::printf("works table: %zu rows (scripts/works.lua)\n", g_works.size());
    check(g_works.size() > 0, "the works table is loaded (the prehistory-ON worlds run the stream as it ships)");

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
    // case cannot drift with them: epoch_year 1960 (the epoch flip, BL-1047;
    // it was 0 CE under NR-177) and prehistory_years 400 (the scope knob: on).
    // Nothing here is shortened to fit a timeout.
    std::printf("\n--- R3: determinism with the Era -1 pre-history pass ON ---\n");
    std::fflush(stdout);

    const world_params pre_a{ .seed = seed_a, .abundance = abundance_level::standard,
                              .epoch_year = 1960, .prehistory_years = 400 };
    const world_params pre_b{ .seed = seed_b, .abundance = abundance_level::standard,
                              .epoch_year = 1960, .prehistory_years = 400 };
    const world_params off_a{ .seed = seed_a, .abundance = abundance_level::standard,
                              .epoch_year = 1960, .prehistory_years = 0 };

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

    // 3.7 — BL-1042, RE-POINTED BY BL-1044: the Industrialisation span runs by
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

    // 3.8 — BL-1047, THE EPOCH FLIP: the epoch is the campaign's calendar and
    //       nothing else generation reads, so another epoch must build the SAME
    //       world dated differently — every span runs on its own fixed years. A
    //       mechanism that still keyed on the epoch (the settlement stop, the
    //       Empires start, an arc predicate) would show here as a digest that
    //       differs. RE-SCOPED TO 1900 (BL-1114): epoch 0 is retired (Ben,
    //       2026-09-25, NR-920) and the flag refuses it, so the row compares the
    //       default against a year a player can still ask for.
    {
        world_params pre_ae = pre_a;
        pre_ae.epoch_year   = 1900;
        generation_report     rep_ae{};
        era_minus_one_fixture fx_ae;
        const world    w_ae = timed_world(pre_ae, &rep_ae, "seed A, prehistory ON, epoch 1900", fx_ae);
        const uint64_t d_ae = deep_digest(w_ae, fx_ae);
        std::printf("     digest seedA/on/epoch1900 = %016llX\n", static_cast<unsigned long long>(d_ae));
        check(d_ae == d_a1 && measure(w_ae) == m_a1
                  && rep_ae.prehistory_years == rep_a1.prehistory_years
                  && rep_ae.prehistory_battles == rep_a1.prehistory_battles,
              "R3.8 epoch 1900 and epoch 1960 build the byte-identical world (the epoch moves the calendar alone)");
    }

    // -----------------------------------------------------------------------
    // R4 — RETIRED WITH THE TWO-SPAN ARC (BL-1075)
    // -----------------------------------------------------------------------
    // R4 built the superseded 1160 -> 1560 -> 1960 two-span arc an explicit
    // `epoch_year = 1960` used to select, and asserted it ran both spans and
    // was deterministic (digest '1960/two-span'). The arc is deleted outright
    // (Ben, 2026-09-18, NR-898 (2)). The claim that replaces it is R3.8: the
    // world a 1960 epoch builds is the shipped span-on world, byte for byte the
    // one epoch 0 builds, and R3.1-R3.7 hold that world to the full guarantee.
    // The generation budget (BL-754) is still printed per build by
    // `timed_world`, which is all R4 ever did with it.

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILURES", failures,
                failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
