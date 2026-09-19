// ---------------------------------------------------------------------------
// stockpile_budget_check — BL-1042 (stockpile to budget), the checks that can
// fail.
//
//   bash tools/verify/build_lua_harness.sh stockpile_budget_check
//   ./build_gen/verify/stockpile_budget_check.exe [--r8] [--seed N]
//
// The DEFAULT run is part 1 alone (instant, script-free), so the ctest glob that
// registers every tools/verify/*.cpp can run it under its 60 s default from the
// build directory. `--r8` adds part 2, which generates two span-on worlds (a few
// minutes, from the repo root, where scripts/ resolves).
//
// PART 1 — THE SPLIT, ON HAND-BUILT SLOTS (instant, no generation). A world is
// built by hand: a settlement record whose regions carry industry points and
// `centres_razed`, a carve index of founded and dropped slots, and centres the
// index does NOT name (a province anchor and a coverage founding). Every
// expected share is written out below as a literal, worked by hand, so the
// check fails if the builder sent every point to rank 1, split evenly, broke a
// tie to the wrong slot, paid an anchor, spread a dropped slot's share over its
// siblings, or let a razed region's points reach anyone. The rejections are
// checked too: an int32 overflow, and a stock with points but no carve index
// (a lost index must never pass as "no carved centre").
//
// PART 2 — R8, A NON-EMPTY BUDGET BUILT TWICE (one seed, the Digitisation span
// ON). world_determinism's stockpile fold only runs when a region holds a
// point, and none of its worlds runs the span, so it never sees a non-empty
// budget. This builds the seed's campaign world twice in app order as far as
// the search (harness_params.hpp `build_app_base_world`: config, works,
// generation, setup, load_economy — no search, which needs none of this), builds
// the stockpile budget off each, and compares every field: the region stock,
// the carve index, every centre's budget, every unspent reason. It FAILS if the
// budget is empty (the check would be vacuous) or any field differs.
//
// Exit 0 only when every check passes.
// ---------------------------------------------------------------------------

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/settlement.hpp"
#include "world/stockpile_budget.hpp"
#include "world/world.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& label)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", label.c_str());
    if (!ok)
        ++failures;
}

std::int64_t unspent(const stockpile_budget& sb, stockpile_unspent_reason r)
{
    return sb.unspent[static_cast<std::size_t>(r)];
}

std::int32_t pts(const stockpile_budget& sb, entity_id centre)
{
    const auto it = sb.budget.points().find(centre);
    return it == sb.budget.points().end() ? 0 : it->second;
}

/// A hand-built world: @p regions as the settlement record, the carve index
/// given, and every indexed centre plus two unindexed ones (an anchor and a
/// coverage founding) standing as population centres.
std::unique_ptr<world> hand_world(const std::vector<region>& regions,
                                  const std::map<entity_id, carve_slot>& founded,
                                  const std::vector<carve_dropped_slot>& dropped)
{
    auto w = std::make_unique<world>();
    auto ss = std::make_shared<settlement_state>();
    ss->regions         = regions;
    ss->urban_map_drawn = true;
    w->gen_settlement    = ss;
    w->gen_carve_centres = founded;
    w->gen_carve_dropped = dropped;
    for (const auto& [cid, slot] : founded)
        w->population_centres[cid] = population_centre_component{};
    population_centre_component anchor;
    anchor.province_anchor = true;
    w->population_centres[900] = anchor;                        // a province anchor
    w->population_centres[901] = population_centre_component{};  // a coverage founding
    return w;
}

region points_region(std::int64_t points, int centres_razed = 0)
{
    region r;
    r.industry_points = points;
    r.centres_razed   = centres_razed;
    return r;
}

void part_one()
{
    std::printf("--- PART 1: the split, on hand-built carve slots ---\n");

    // Region 0: 1001 points over four slots keyed 300/150/100/75 (sum 625).
    //   exact: 480.48 / 240.24 / 160.16 / 120.12 -> floors 480/240/160/120 = 1000,
    //   the 1 left goes to the largest remainder (.48, rank 1): 481/240/160/120.
    //   Rank 3 was DROPPED (the body built out): its 160 is unspent, never
    //   spread over ranks 1, 2 and 4.
    // Region 1: 5 points over two EQUAL keys (10, 10): 2.5 each -> floors 2/2,
    //   the 1 left is a TIE, which goes to the LOWER RANK (rank 1, centre 31),
    //   not the lower centre id (rank 2 is centre 30): 31 -> 3, 30 -> 2.
    // Region 2: 700 points, no carved slot, towns razed twice -> `razed` 700.
    // Region 3: 50 points, no carved slot, nothing razed -> the residual 50.
    // Region 4: 7 points over two ZERO keys: split evenly, the odd point to rank
    //   1: centre 50 -> 4, centre 51 -> 3.
    // Region 5: 9 points, one slot whose tile never resolved -> carve_no_tile 9.
    // Region 6: no points, one founded slot (centre 60) -> nothing, no row.
    std::vector<region> regions = {
        points_region(1001), points_region(5), points_region(700, 2), points_region(50),
        points_region(7),    points_region(9), points_region(0),
    };
    std::map<entity_id, carve_slot> founded = {
        { 10, { 0, 1, 300 } }, { 11, { 0, 2, 150 } }, { 13, { 0, 4, 75 } },
        { 31, { 1, 1, 10 } },  { 30, { 1, 2, 10 } },
        { 50, { 4, 1, 0 } },   { 51, { 4, 2, 0 } },
        { 60, { 6, 1, 500 } },
    };
    std::vector<carve_dropped_slot> dropped = {
        { 0, 3, 100, carve_drop_reason::body_built_out },
        { 5, 1, 40,  carve_drop_reason::no_tile },
    };
    const auto w  = hand_world(regions, founded, dropped);
    const stockpile_budget sb = build_stockpile_budget(*w);

    std::printf("     budget: 10=%d 11=%d 13=%d 31=%d 30=%d 50=%d 51=%d 60=%d anchor900=%d cover901=%d\n",
                pts(sb, 10), pts(sb, 11), pts(sb, 13), pts(sb, 31), pts(sb, 30), pts(sb, 50),
                pts(sb, 51), pts(sb, 60), pts(sb, 900), pts(sb, 901));
    std::printf("     unspent: carve_dropped %lld, carve_no_tile %lld, razed %lld, no_carved_centre %lld, "
                "rejected %lld; total %lld, to centres %lld\n",
                (long long)unspent(sb, stockpile_unspent_reason::carve_dropped),
                (long long)unspent(sb, stockpile_unspent_reason::carve_no_tile),
                (long long)unspent(sb, stockpile_unspent_reason::razed),
                (long long)unspent(sb, stockpile_unspent_reason::no_carved_centre),
                (long long)unspent(sb, stockpile_unspent_reason::rejected),
                (long long)sb.points_total, (long long)sb.points_to_centres);

    check(!sb.rejected, "1.0 the hand-built stock is not rejected");
    check(pts(sb, 10) == 481 && pts(sb, 11) == 240 && pts(sb, 13) == 120,
          "1.1 largest remainder by slot key: region 0 -> 481 / 240 / (dropped) / 120, not all to "
          "rank 1 (1001) and not even (250/250/250/251)");
    check(unspent(sb, stockpile_unspent_reason::carve_dropped) == 160,
          "1.2 the dropped slot's share (160) is unspent as carve_dropped, not spread over its siblings");
    check(pts(sb, 31) == 3 && pts(sb, 30) == 2,
          "1.3 an exact tie breaks to the LOWER RANK (rank 1 = centre 31 takes 3), not the lower id");
    check(unspent(sb, stockpile_unspent_reason::razed) == 700,
          "1.4 a region whose towns were razed loses its points with them: razed 700 (NR-901)");
    check(unspent(sb, stockpile_unspent_reason::no_carved_centre) == 50,
          "1.5 points on a region that carved nothing and razed nothing are the residual (50)");
    check(pts(sb, 50) == 4 && pts(sb, 51) == 3,
          "1.6 all-zero keys split evenly, the odd point to rank 1 (4 / 3)");
    check(unspent(sb, stockpile_unspent_reason::carve_no_tile) == 9,
          "1.7 a slot that resolved to no tile is unspent under its own reason (9)");
    check(pts(sb, 900) == 0 && pts(sb, 901) == 0 && pts(sb, 60) == 0,
          "1.8 an anchor and a coverage founding get nothing; a centre on a pointless region gets nothing");
    check(sb.budget.points().size() == 7,
          "1.9 exactly the seven founded slots on point-holding regions hold a budget");
    check(sb.points_total == 1772 && sb.points_to_centres == 853 && sb.balanced(),
          "1.10 every point accounted for: 1772 = 853 to centres + 919 unspent, and the account closes");

    // Determinism of the builder itself on one input.
    const stockpile_budget sb2 = build_stockpile_budget(*w);
    check(sb2.budget.points() == sb.budget.points() && sb2.unspent == sb.unspent,
          "1.11 the builder is a pure function: the same input gives the same budget");

    // The narrowing: one centre's share past int32 rejects the WHOLE budget.
    {
        std::vector<region> big = { points_region(3000000000LL), points_region(10) };
        std::map<entity_id, carve_slot> f = { { 70, { 0, 1, 100 } }, { 71, { 1, 1, 100 } } };
        const auto wb = hand_world(big, f, {});
        const stockpile_budget r = build_stockpile_budget(*wb);
        std::printf("     int32: rejected=%d (%s) budget entries %zu, rejected points %lld\n",
                    r.rejected ? 1 : 0, r.rejection.c_str(), r.budget.points().size(),
                    (long long)unspent(r, stockpile_unspent_reason::rejected));
        check(r.rejected && r.budget.empty()
                  && unspent(r, stockpile_unspent_reason::rejected) == 3000000010LL && r.balanced(),
              "1.12 a centre share past int32 REJECTS the whole budget: empty, every point rejected");
    }

    // A lost carve index: points, but both lists empty -> rejected, never razed.
    {
        std::vector<region> lost = { points_region(400, 1), points_region(600) };
        const auto wl = hand_world(lost, {}, {});
        const stockpile_budget r = build_stockpile_budget(*wl);
        std::printf("     lost index: rejected=%d (%s), razed %lld, residual %lld\n",
                    r.rejected ? 1 : 0, r.rejection.c_str(),
                    (long long)unspent(r, stockpile_unspent_reason::razed),
                    (long long)unspent(r, stockpile_unspent_reason::no_carved_centre));
        check(r.rejected && r.budget.empty() && unspent(r, stockpile_unspent_reason::razed) == 0
                  && unspent(r, stockpile_unspent_reason::rejected) == 1000 && r.balanced(),
              "1.13 a stock with points and NO carve index is rejected as inconsistent (not razed)");
    }

    // No settlement record, and a span-off stock: both empty, nothing rejected.
    {
        world none;
        const stockpile_budget a = build_stockpile_budget(none);
        std::vector<region> zero = { points_region(0), points_region(0) };
        const auto wz = hand_world(zero, {}, {});
        const stockpile_budget b = build_stockpile_budget(*wz);
        check(!a.rejected && a.budget.empty() && a.points_total == 0
                  && !b.rejected && b.budget.empty() && b.points_total == 0,
              "1.14 no settlement record, or a stock of zero (the span off): an empty budget, no rejection");
    }
}

/// Every field of a stockpile budget and the carve index behind it, compared.
std::string diff_budgets(const stockpile_budget& a, const stockpile_budget& b)
{
    std::string d;
    if (a.rejected != b.rejected) d += " rejected;";
    if (a.points_total != b.points_total) d += " points_total;";
    if (a.points_to_centres != b.points_to_centres) d += " points_to_centres;";
    if (a.unspent != b.unspent) d += " unspent;";
    if (a.budget.points() != b.budget.points()) d += " the per-centre budget;";
    if (a.regions.size() != b.regions.size()) d += " region rows;";
    else
        for (std::size_t i = 0; i < a.regions.size(); ++i)
        {
            const stockpile_region_row& x = a.regions[i];
            const stockpile_region_row& y = b.regions[i];
            if (x.region != y.region || x.points != y.points || x.to_centres != y.to_centres
                || x.founded != y.founded || x.dropped != y.dropped || x.unspent != y.unspent)
            {
                d += " region row " + std::to_string(x.region) + ";";
                break;
            }
        }
    return d;
}

std::uint64_t fnv(const stockpile_budget& sb)
{
    std::uint64_t h = 1469598103934665603ull;
    const auto mix = [&](std::uint64_t v) {
        for (int i = 0; i < 8; ++i) { h ^= (v >> (8 * i)) & 0xFF; h *= 1099511628211ull; }
    };
    mix(static_cast<std::uint64_t>(sb.points_total));
    for (const std::int64_t u : sb.unspent) mix(static_cast<std::uint64_t>(u));
    for (const auto& [c, p] : sb.budget.points()) { mix(c); mix(static_cast<std::uint64_t>(p)); }
    return h;
}

void part_two(std::uint32_t seed)
{
    std::printf("\n--- PART 2 (R8): a NON-EMPTY stockpile budget built twice, seed %u, span ON ---\n", seed);
    std::fflush(stdout);

    world_params p{};
    p.seed = seed;
    p.digitisation_span_enabled = true;

    stockpile_budget first;
    std::map<entity_id, carve_slot> carve_first;
    std::size_t dropped_first = 0;
    {
        lua_state lua;
        auto out = std::make_unique<app_start_world>();
        build_app_base_world(lua, p, *out);
        first         = build_stockpile_budget(out->w);
        carve_first   = out->w.gen_carve_centres;
        dropped_first = out->w.gen_carve_dropped.size();
    }
    stockpile_budget second;
    std::map<entity_id, carve_slot> carve_second;
    std::size_t dropped_second = 0;
    {
        lua_state lua;
        auto out = std::make_unique<app_start_world>();
        build_app_base_world(lua, p, *out);
        second         = build_stockpile_budget(out->w);
        carve_second   = out->w.gen_carve_centres;
        dropped_second = out->w.gen_carve_dropped.size();
    }

    const std::uint64_t h1 = fnv(first), h2 = fnv(second);
    std::printf("     build 1: %lld points -> %lld to %zu centres, razed %lld, residual %lld, rejected %d; "
                "carve %zu founded / %zu dropped; digest %016llX\n",
                (long long)first.points_total, (long long)first.points_to_centres,
                first.budget.points().size(),
                (long long)unspent(first, stockpile_unspent_reason::razed),
                (long long)unspent(first, stockpile_unspent_reason::no_carved_centre),
                first.rejected ? 1 : 0, carve_first.size(), dropped_first,
                static_cast<unsigned long long>(h1));
    std::printf("     build 2: %lld points -> %lld to %zu centres; carve %zu founded / %zu dropped; "
                "digest %016llX\n",
                (long long)second.points_total, (long long)second.points_to_centres,
                second.budget.points().size(), carve_second.size(), dropped_second,
                static_cast<unsigned long long>(h2));

    check(!first.budget.empty() && !first.rejected,
          "2.1 the span-on budget is NON-EMPTY and not rejected (else this check is vacuous)");
    const std::string d = diff_budgets(first, second);
    if (!d.empty())
        std::printf("     differs:%s\n", d.c_str());
    check(d.empty() && h1 == h2, "2.2 R8: the same seed builds the same stockpile budget twice");
    check(carve_first == carve_second && dropped_first == dropped_second,
          "2.3 the carve index is identical across the two builds");
    check(first.balanced() && second.balanced(), "2.4 both accounts close");
}

} // namespace

int main(int argc, char** argv)
{
    bool          r8   = false;
    std::uint32_t seed = 28;
    for (int a = 1; a < argc; ++a)
    {
        const std::string arg = argv[a];
        if (arg == "--r8")
        {
            r8 = true;
            continue;
        }
        if (arg == "--seed" && a + 1 < argc)
        {
            const char* val = argv[++a];
            char* end = nullptr;
            const unsigned long long v = std::strtoull(val, &end, 10);
            if (end != val && *end == '\0' && v <= 0xFFFFFFFFull)
            {
                seed = static_cast<std::uint32_t>(v);
                continue;
            }
            std::printf("--seed: '%s' is not a seed number\n", val);
            return 2;
        }
        std::printf("usage: stockpile_budget_check [--r8] [--seed N]\n");
        return 2;
    }

    part_one();
    if (r8)
        part_two(seed);
    else
        std::printf("\n(part 2, R8's non-empty budget built twice, not run: pass --r8)\n");

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILURES", failures,
                failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
