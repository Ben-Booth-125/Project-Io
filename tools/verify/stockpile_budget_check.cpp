// ---------------------------------------------------------------------------
// stockpile_budget_check — BL-1042 (stockpile to budget), the checks that can
// fail.
//
//   bash tools/verify/build_lua_harness.sh stockpile_budget_check
//   ./build_gen/verify/stockpile_budget_check.exe [--r8] [--seed N]
//        [--seat-curve [--seeds a,b,c] [--pairs d:m,...]]   (part 3, ~45 s a seed)
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
// PART 1b — BL-1064, THE DERIVED PRICE (instant). The firm price is the whole
// stock over the divisor (Ben, 2026-09-21, NR-907): floored, never below 1, the
// unspent points included; a divisor <= 0 or a price past int32 rejects the
// budget whole; an empty budget carries none. And the property the ruling is
// for: two hand-built worlds whose stockpiles differ 5x open seat menus within
// one seat of each other, where one fixed price opens twice the menu on the
// richer. Every expected number is worked by hand beside its check.
//
// PART 2 — R8, A NON-EMPTY BUDGET BUILT TWICE (one seed, the Industrialisation span
// ON). world_determinism's stockpile fold only runs when a region holds a
// point, and none of its worlds runs the span, so it never sees a non-empty
// budget. This builds the seed's campaign world twice in app order as far as
// the search (harness_params.hpp `build_app_base_world`: config, works,
// generation, setup, load_economy — no search, which needs none of this), builds
// the stockpile budget off each, and compares every field: the region stock,
// the carve index, every centre's budget, every unspent reason, and the derived
// price (BL-1064). It FAILS if the budget is empty (the check would be vacuous)
// or any field differs.
//
// Exit 0 only when every check passes.
// ---------------------------------------------------------------------------

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/settlement.hpp"
#include "world/stockpile_budget.hpp"
#include "world/world.hpp"

#include <algorithm>
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
        check(a.firm_price_points == 0 && b.firm_price_points == 0
                  && stockpile_charter_spend(b).firm_price_points == 0,
              "1.15 an empty budget carries NO price (0), and nothing prices it (BL-1064)");
    }
}

/// Seats a budget opens at @p specialist_price: its centres whose points cover
/// one specialist. The count `charter_web_from_budget` charters — one specialist
/// per centre that can afford one (INDUSTRIALISATION.md § 1) — before any ground is
/// read; a centre whose window holds no free site can only lower it.
int seats_at(const stockpile_budget& sb, std::int64_t specialist_price)
{
    int n = 0;
    for (const auto& [centre, p] : sb.budget.points())
        if (p >= specialist_price)
            ++n;
    return n;
}

void part_one_price()
{
    std::printf("\n--- PART 1b (BL-1064): the firm price is the stock's own ---\n");

    // The hand world of part 1: 1772 points in all (853 to centres, 919
    // unspent). The price is the WHOLE stock over the divisor — the unspent
    // points included (NR-907: "the world's whole industry stockpile").
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
    const auto w = hand_world(regions, founded, dropped);

    // 1772 / 100 = 17.72 -> 17 (floors); 1772 / 10000 = 0 -> the floor of 1.
    const stockpile_budget d100   = build_stockpile_budget(*w, 100);
    const stockpile_budget d10000 = build_stockpile_budget(*w, 10000);
    std::printf("     divisor 100: price %d (divisor %lld); divisor 10000: price %d; shipped "
                "divisor %lld: price %d\n",
                d100.firm_price_points, (long long)d100.price_divisor, d10000.firm_price_points,
                (long long)k_stockpile_price_divisor, build_stockpile_budget(*w).firm_price_points);
    check(!d100.rejected && d100.firm_price_points == 17 && d100.price_divisor == 100,
          "1b.1 the firm price is the WHOLE stock over the divisor, floored: 1772 / 100 -> 17 "
          "(not the 853 to centres: that would be 8)");
    check(!d10000.rejected && d10000.firm_price_points == 1,
          "1b.2 a stock smaller than the divisor prices a charter at 1 point, never 0");
    check(d100.budget.points() == d10000.budget.points() && d100.unspent == d10000.unspent,
          "1b.3 the divisor moves the PRICE only: the split and every reason are the same");
    {
        const charter_spend_params s = stockpile_charter_spend(d100);
        check(s.firm_price_points == 17
                  && s.specialist_price_points()
                         == 17LL * static_cast<long long>(k_stockpile_specialist_firm_charters),
              "1b.4 the spend charges the derived price, and a specialist costs its firm charters "
              "at it (m x 17)");
    }

    // The rejections: a divisor that is not one, and a price past int32.
    {
        const stockpile_budget z = build_stockpile_budget(*w, 0);
        const stockpile_budget n = build_stockpile_budget(*w, -5);
        std::printf("     divisor 0: rejected=%d (%s)\n", z.rejected ? 1 : 0, z.rejection.c_str());
        check(z.rejected && z.budget.empty() && z.firm_price_points == 0
                  && unspent(z, stockpile_unspent_reason::rejected) == 1772 && z.balanced()
                  && n.rejected && n.budget.empty(),
              "1b.5 a divisor <= 0 REJECTS the whole budget (every point rejected), never clamps");
    }
    {
        // 3e9 razed points: no centre's share passes int32 (the centre takes
        // 10), but the whole stock over a divisor of 1 does.
        std::vector<region> big = { points_region(3000000000LL, 1), points_region(10) };
        std::map<entity_id, carve_slot> f = { { 71, { 1, 1, 100 } } };
        const auto wb = hand_world(big, f, {});
        const stockpile_budget r1 = build_stockpile_budget(*wb, 1);
        const stockpile_budget r2 = build_stockpile_budget(*wb, 2);
        std::printf("     int32 price: divisor 1 rejected=%d (%s); divisor 2 price %d\n",
                    r1.rejected ? 1 : 0, r1.rejection.c_str(), r2.firm_price_points);
        check(r1.rejected && r1.budget.empty() && r1.firm_price_points == 0
                  && unspent(r1, stockpile_unspent_reason::rejected) == 3000000010LL && r1.balanced(),
              "1b.6 a derived price past int32 REJECTS the whole budget, never clamps");
        check(!r2.rejected && r2.firm_price_points == 1500000005,
              "1b.7 the same stock over 2 prices at 1500000005, inside int32: accepted");
    }

    // A stock with points that no centre takes — region 0's 700 razed, the one
    // founded slot on a pointless region — builds an EMPTY budget, and an empty
    // budget prices nothing (the cold review's case, 2026-09-21).
    {
        std::vector<region> razed = { points_region(700, 1), points_region(0) };
        std::map<entity_id, carve_slot> f = { { 80, { 1, 1, 100 } } };
        const auto wr = hand_world(razed, f, {});
        const stockpile_budget r = build_stockpile_budget(*wr, 100);
        std::printf("     all razed: rejected=%d, budget entries %zu, price %d, divisor %lld\n",
                    r.rejected ? 1 : 0, r.budget.points().size(), r.firm_price_points,
                    (long long)r.price_divisor);
        check(!r.rejected && r.budget.empty() && r.points_total == 700 && r.firm_price_points == 0
                  && r.price_divisor == 0 && stockpile_charter_spend(r).firm_price_points == 0
                  && r.balanced(),
              "1b.8 a stock whose every point went unspent builds an EMPTY budget with NO price");
    }

    // THE PROPERTY THE RULING IS FOR: two worlds whose stockpiles differ 5x
    // open seat menus of the same size. World A: fourteen centres, one region
    // and one slot each, so a centre's budget IS its region's points (a Zipf-like
    // 12000 ... 300, 36000 in all). World B: every region x5 (180000).
    //   divisor 100, m = 4 (a literal here, so a pinned constant cannot move it):
    //   A: price 360, specialist 1440 -> 12000..1700 afford, 1300 does not: 7.
    //   B: price 1800, specialist 7200 -> 60000..8500 afford, 6500 does not: 7.
    //   At A's FIXED price (specialist 1440) B affords all 14 (its smallest is 1500).
    // TOLERANCE: one seat. Two scaled stocks price within the floor's rounding
    // of each other, so a centre sitting exactly on the line may fall either
    // side; none does here. The fixed price must miss by MORE than the tolerance.
    {
        const std::vector<std::int64_t> a_pts = { 12000, 6000, 4000, 3000, 2400, 2000, 1700,
                                                  1300,  1000, 800,  600,  500,  400,  300 };
        std::vector<region> ra, rb;
        std::map<entity_id, carve_slot> fa;
        for (std::size_t i = 0; i < a_pts.size(); ++i)
        {
            ra.push_back(points_region(a_pts[i]));
            rb.push_back(points_region(a_pts[i] * 5));
            fa[static_cast<entity_id>(100 + i)] = { static_cast<int>(i), 1, 100 };
        }
        const auto wa = hand_world(ra, fa, {});
        const auto wb = hand_world(rb, fa, {});
        const stockpile_budget a = build_stockpile_budget(*wa, 100);
        const stockpile_budget b = build_stockpile_budget(*wb, 100);
        constexpr std::int64_t m = 4;
        const int seats_a       = seats_at(a, m * a.firm_price_points);
        const int seats_b       = seats_at(b, m * b.firm_price_points);
        const int seats_b_fixed = seats_at(b, m * a.firm_price_points);
        std::printf("     5x: A %lld points, price %d, %d seats | B %lld points, price %d, %d seats "
                    "| B at A's fixed price: %d seats\n",
                    (long long)a.points_total, a.firm_price_points, seats_a,
                    (long long)b.points_total, b.firm_price_points, seats_b, seats_b_fixed);
        check(a.points_total == 36000 && b.points_total == 180000 && a.firm_price_points == 360
                  && b.firm_price_points == 1800,
              "1b.9 the two worlds: 36000 and 180000 points, priced 360 and 1800 at divisor 100");
        check(seats_a == 7 && seats_b == 7 && std::abs(seats_a - seats_b) <= 1,
              "1b.10 stockpiles 5x apart open seat menus within ONE seat of each other at the "
              "derived price (7 and 7)");
        check(seats_b_fixed == 14 && seats_b_fixed - seats_a > 1,
              "1b.11 at one FIXED price the richer world opens twice the menu (14 against 7): "
              "the spread the ruling removes");
    }
}

/// Every field of a stockpile budget and the carve index behind it, compared.
std::string diff_budgets(const stockpile_budget& a, const stockpile_budget& b)
{
    std::string d;
    if (a.rejected != b.rejected) d += " rejected;";
    if (a.points_total != b.points_total) d += " points_total;";
    if (a.points_to_centres != b.points_to_centres) d += " points_to_centres;";
    if (a.firm_price_points != b.firm_price_points || a.price_divisor != b.price_divisor)
        d += " the derived price;";
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
    mix(static_cast<std::uint64_t>(sb.firm_price_points));   // BL-1064
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
    p.industrialisation_span_enabled = true;

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
    std::printf("     derived price: %d then %d (the stock over %lld)\n", first.firm_price_points,
                second.firm_price_points, (long long)first.price_divisor);
    check(first.firm_price_points > 0 && first.firm_price_points == second.firm_price_points
              && first.price_divisor == k_stockpile_price_divisor
              && first.firm_price_points
                     == std::max<std::int64_t>(1, first.points_total / k_stockpile_price_divisor),
          "2.5 BL-1064: the derived price is the whole stock over the shipped divisor, and the "
          "same seed derives the same price twice");
}

/// Parse "a,b,c" of whole numbers (or "d:m" pairs when @p pairs). Empty on a bad token.
std::vector<std::pair<std::int64_t, std::int64_t>> parse_list(const std::string& val, bool pairs)
{
    std::vector<std::pair<std::int64_t, std::int64_t>> out;
    std::size_t at = 0;
    while (at <= val.size())
    {
        std::size_t comma = val.find(',', at);
        if (comma == std::string::npos) comma = val.size();
        const std::string tok = val.substr(at, comma - at);
        const std::size_t colon = tok.find(':');
        if (pairs != (colon != std::string::npos)) return {};
        const std::string a = pairs ? tok.substr(0, colon) : tok;
        const std::string b = pairs ? tok.substr(colon + 1) : "1";
        if (a.empty() || b.empty() || a.size() > 10 || b.size() > 6
            || a.find_first_not_of("0123456789") != std::string::npos
            || b.find_first_not_of("0123456789") != std::string::npos)
            return {};
        out.emplace_back(std::strtoll(a.c_str(), nullptr, 10), std::strtoll(b.c_str(), nullptr, 10));
        at = comma + 1;
    }
    return out;
}

/// PART 3 — THE SEAT CURVE (BL-1043 stage 2's owed reading, 2026-09-21). Seats
/// on a budget world are the centres that can AFFORD a specialist (one per
/// centre, richest first; stage 2 found the affording count equal to the seats
/// on 74 of 80 rows, never below them), and affording reads the budget alone —
/// so this builds each seed's span world as far as the budget (no search, no
/// settle) and counts, for each (divisor d, charters m), the centres whose
/// points cover m x the price d derives. An UPPER BOUND on seats, exact wherever
/// every affording centre finds ground. Reports; asserts only that each world's
/// budget is non-empty and its account closes.
void part_three_seat_curve(const std::vector<std::uint32_t>& seeds,
                           const std::vector<std::pair<std::int64_t, std::int64_t>>& pairs)
{
    std::printf("\n--- PART 3: the seat curve — centres affording a specialist, from the budget alone ---\n");
    std::printf("     %-5s %12s %7s", "seed", "stock", "centres");
    for (const auto& [d, m] : pairs)
        std::printf(" %7s", (std::to_string(d) + ":" + std::to_string(m)).c_str());
    std::printf("   (d:m; d/m =");
    for (const auto& [d, m] : pairs)
        std::printf(" %lld", static_cast<long long>(d / m));
    std::printf(")\n");
    std::fflush(stdout);

    std::vector<std::vector<int>> col(pairs.size());
    for (const std::uint32_t seed : seeds)
    {
        world_params p{};
        p.seed = seed;
        p.industrialisation_span_enabled = true;
        lua_state lua;
        auto out = std::make_unique<app_start_world>();
        build_app_base_world(lua, p, *out);
        const stockpile_budget base = build_stockpile_budget(out->w);
        check(!base.budget.empty() && !base.rejected && base.balanced(),
              "3.1 seed " + std::to_string(seed) + ": the span-on budget is non-empty and closes");
        std::printf("     %-5u %12lld %7zu", seed, static_cast<long long>(base.points_total),
                    base.budget.points().size());
        for (std::size_t i = 0; i < pairs.size(); ++i)
        {
            const stockpile_budget sb = build_stockpile_budget(out->w, pairs[i].first);
            const int n = seats_at(sb, pairs[i].second * static_cast<std::int64_t>(sb.firm_price_points));
            col[i].push_back(n);
            std::printf(" %7d", n);
        }
        std::printf("\n");
        std::fflush(stdout);
    }
    const auto med = [](std::vector<int> v) {
        std::sort(v.begin(), v.end());
        const std::size_t n = v.size();
        return n == 0 ? 0.0 : (n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0);
    };
    std::printf("     %-5s %12s %7s", "MED", "", "");
    for (const auto& c : col) std::printf(" %7.1f", med(c));
    std::printf("\n     %-5s %12s %7s", "MIN", "", "");
    for (const auto& c : col) std::printf(" %7d", c.empty() ? 0 : *std::min_element(c.begin(), c.end()));
    std::printf("\n     %-5s %12s %7s", "MAX", "", "");
    for (const auto& c : col) std::printf(" %7d", c.empty() ? 0 : *std::max_element(c.begin(), c.end()));
    std::printf("\n     %-5s %12s %7s", "ZERO", "", "");
    for (const auto& c : col) std::printf(" %7d", static_cast<int>(std::count(c.begin(), c.end(), 0)));
    std::printf("   (worlds with no centre affording a specialist)\n");
}

} // namespace

int main(int argc, char** argv)
{
    bool          r8   = false;
    std::uint32_t seed = 28;
    bool          curve = false;
    std::vector<std::uint32_t> curve_seeds = { 46, 28, 11, 31, 40, 12, 37, 13,
                                               41, 43, 32, 10, 25, 38, 9, 0 };   // the library
    std::vector<std::pair<std::int64_t, std::int64_t>> curve_pairs = {
        { 650, 4 }, { 900, 4 }, { 650, 2 }, { 900, 2 }, { 650, 1 }, { 900, 1 }, { 1300, 1 } };
    for (int a = 1; a < argc; ++a)
    {
        const std::string arg = argv[a];
        if (arg == "--r8")
        {
            r8 = true;
            continue;
        }
        if (arg == "--seat-curve")
        {
            curve = true;
            continue;
        }
        if ((arg == "--seeds" || arg == "--pairs") && a + 1 < argc)
        {
            const auto v = parse_list(argv[++a], arg == "--pairs");
            bool ok = !v.empty();
            for (const auto& [x, y] : v)
                ok = ok && (arg == "--pairs" ? (x > 0 && y > 0) : x <= 0xFFFFFFFFll);
            if (!ok)
            {
                std::printf("%s: '%s' is not a list of %s\n", arg.c_str(), argv[a],
                            arg == "--pairs" ? "d:m pairs, both > 0" : "seeds");
                return 2;
            }
            if (arg == "--pairs")
                curve_pairs = v;
            else
            {
                curve_seeds.clear();
                for (const auto& e : v) curve_seeds.push_back(static_cast<std::uint32_t>(e.first));
            }
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
        std::printf("usage: stockpile_budget_check [--r8] [--seed N] "
                    "[--seat-curve [--seeds a,b] [--pairs d:m,...]]\n");
        return 2;
    }

    part_one();
    part_one_price();
    if (r8)
        part_two(seed);
    else
        std::printf("\n(part 2, R8's non-empty budget built twice, not run: pass --r8)\n");
    if (curve)
        part_three_seat_curve(curve_seeds, curve_pairs);

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILURES", failures,
                failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
