// ---------------------------------------------------------------------------
// stockpile_budget_check — BL-1042 (stockpile to budget), the checks that can
// fail.
//
//   bash tools/verify/build_lua_harness.sh stockpile_budget_check
//   ./build_gen/verify/stockpile_budget_check.exe [--r8] [--seed N]
//        [--seat-curve [--seeds a,b,c] [--pairs d:m,...]]   (part 3, ~45 s a seed)
//        [--firm-census [--seeds a,b,c]]                     (part 4, the whole start a seed)
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
// PART 4 — THE FIRM CENSUS (BL-1146, `--firm-census`; a reading, run from the
// repo root). Each seed's campaign start is built as the app builds it
// (harness_params.hpp `build_app_start_world`: generation, the stockpile budget,
// the landscape search and its winner applied with that budget), and the world
// is read at the seam before the settle: budget firms and Pass 6 firms apart,
// every charter-unspent reason as points and as bookings (one row per centre and
// reason), the land provinces on the charter bodies and the largest, the
// provinces holding exactly 2 firms (the village's cap; since BL-1146 a province's
// cap is 2 per rung of its centre — table E reads firms by rung), and where the budget concentrates
// — the richest centre's share, its firms, its `province_cap`, and how many
// provinces its window (the spend's own radius, the centre nation's tiles, the
// walk's column-wrapped metric) spans. Asserts only that each world's budget
// account closes; the numbers are the product.
//
// Exit 0 only when every check passes.
// ---------------------------------------------------------------------------

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/charter_budget.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"   // province_centre_rungs (BL-1146)
#include "world/province.hpp"
#include "world/settlement.hpp"
#include "world/stockpile_budget.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
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
/// BL-1168: the reach the seat curve prices by (`--reach`); the shipped one by default.
charter_price_reach g_curve_reach = k_stockpile_charter_reach;

/// Centres affording @p m firm charters at their OWN price (BL-1168: the
/// centre's trade reach's, `firm_price_at`; the world's under `world`).
int seats_by_reach(const stockpile_budget& sb, std::int64_t m)
{
    int n = 0;
    for (const auto& [centre, p] : sb.budget.points())
        if (p >= m * static_cast<std::int64_t>(sb.firm_price_at(centre)))
            ++n;
    return n;
}

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
    std::printf("\n--- PART 3: the seat curve — centres affording a specialist, from the budget alone "
                "(priced by %s reach) ---\n", charter_price_reach_name(g_curve_reach));
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
        const stockpile_budget base = build_stockpile_budget(out->w, k_stockpile_price_divisor,
                                                             g_curve_reach);
        check(!base.budget.empty() && !base.rejected && base.balanced(),
              "3.1 seed " + std::to_string(seed) + ": the span-on budget is non-empty and closes");
        std::printf("     %-5u %12lld %7zu", seed, static_cast<long long>(base.points_total),
                    base.budget.points().size());
        for (std::size_t i = 0; i < pairs.size(); ++i)
        {
            const stockpile_budget sb = build_stockpile_budget(out->w, pairs[i].first, g_curve_reach);
            const int n = seats_by_reach(sb, pairs[i].second);
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

// ---------------------------------------------------------------------------
// PART 4 — THE FIRM CENSUS (BL-1146)
// ---------------------------------------------------------------------------
// BL-1133 made a province its centre's whole ground, so a large centre's spend
// window, which spanned several small provinces, may now sit inside one — and
// the budget path's per-province cap would then refuse the rest of that
// centre's budget. The cap was a flat 2 (NR-910); since BL-1146 (NR-960) it is 2
// per rung of the province's centre, a village 2 .. a megacity 10. This reads
// what the cap does on the shipped start. READ-ONLY on the world: nothing here
// writes it after the build.

/// Distinct provinces (non-zero ids) among @p centre's charter window: the
/// centre nation's tiles on the centre's body within @p radius of the centre
/// tile, on the walk's column-wrapped squared metric (corporation_generation.cpp
/// `charter_centre_window`, restated read-only). 0 for a centre with no tile or
/// no nation. @p tiles_out receives the window's tile count.
int census_window_provinces(const world& w, entity_id centre, int radius, int& tiles_out)
{
    tiles_out = 0;
    const auto ct = w.population_centre_tile.find(centre);
    if (ct == w.population_centre_tile.end()) return 0;
    const auto t = w.tiles.find(ct->second);
    if (t == w.tiles.end()) return 0;
    const auto own = w.tile_to_nation.find(ct->second);
    if (own == w.tile_to_nation.end()) return 0;
    const auto nit = w.nations.find(own->second);
    if (nit == w.nations.end()) return 0;
    const entity_id body = t->second.body;
    const auto b = w.bodies.find(body);
    const int gw = (b != w.bodies.end()) ? b->second.grid_width : 0;
    const long long r2 = static_cast<long long>(radius) * radius;
    std::set<std::uint32_t> provs;
    for (const entity_id tid : nit->second.tiles)
    {
        const auto tt = w.tiles.find(tid);
        if (tt == w.tiles.end() || tt->second.body != body) continue;
        long long dx = std::abs(tt->second.grid_x - t->second.grid_x);
        if (gw > 0 && dx > gw / 2) dx = gw - dx;
        const long long dy = tt->second.grid_y - t->second.grid_y;
        if (dx * dx + dy * dy > r2) continue;
        ++tiles_out;
        const std::uint32_t pv = w.provinces.province_of(tid);
        if (pv != 0) provs.insert(pv);
    }
    return static_cast<int>(provs.size());
}

struct census_row
{
    std::uint32_t seed = 0;
    char          kind = 'B';   ///< B budget world, F fell back (NR-910), R refused, E empty budget
    // the stockpile, before the charter spend
    std::int64_t stock_total = 0, stock_to_centres = 0;
    std::array<std::int64_t, stockpile_unspent_reason_count> stock_unspent{};
    int          centres = 0;       ///< budgeted centres
    std::int32_t firm_price = 0;
    std::int64_t specialist_price = 0;
    // the charter spend
    std::int64_t budgeted = 0, spent = 0;
    std::array<std::int64_t, charter_unspent_reason_count> u_pts{};
    std::array<int, charter_unspent_reason_count>          u_rows{};
    int specialists = 0, budget_firms = 0, pass6_firms = 0, all_specialists = 0;
    int body_firms_max = 0, density_ceiling = 0;
    // provinces
    int land_provs = 0, largest_prov = 0, median_prov = 0;       ///< on the charter bodies
    int land_provs_world = 0, largest_prov_world = 0;
    int prov_with_budget = 0, prov_at2_budget = 0, prov_over2_budget = 0, budget_no_prov = 0;
    int prov_with_p6 = 0, prov_at2_p6 = 0, prov_over2_p6 = 0;
    // concentration
    std::int32_t top_pts = 0;
    int top_firms = 0, top_window_provs = 0, top_window_tiles = 0;
    std::int64_t top_pcap = 0;
    std::int64_t top3_pts = 0, top3_pcap = 0;
    double wmean_window_provs = 0.0;   ///< budget-weighted
    int one_prov_centres = 0;          ///< budgeted centres whose window is ONE province
    std::int64_t one_prov_pts = 0;
    int pcap_centres_one_prov = 0;     ///< centres booking province_cap whose window is one province
    int window_radius = 0;
    bool balanced = false;
    // BL-1146: the province's CENTRE, and what stands in it. The rung (1-5) is
    // the province's ANCHOR's scale (`census_province_rungs`); index 0 = a
    // province with no centre, which the cap reads as a village's.
    std::array<int, 6> budget_firms_by_rung{};   ///< budget firms, by the rung of the province they stand in
    std::array<int, 6> p6_firms_by_rung{};       ///< Pass 6 firms, the same
    std::array<int, 6> provs_by_rung{};          ///< land provinces on the charter bodies, by rung
    std::array<int, 6> max_budget_by_rung{};     ///< the most budget firms any one province of that rung holds
    int max_prov_budget = 0, max_prov_p6 = 0;    ///< the largest per-province firm count
    int shared_anchor_tiles = 0;                 ///< anchors whose tile holds more than one centre
    int markets = 0;                             ///< home-body markets at the seam
    double        winner_composite = 0.0;
    std::uint32_t winner_placement = 0;
    int           winner_tier = 0;
};

/// BL-1146 — each province's CENTRE RUNG, the one rule the charter cap reads
/// (`province_centre_rungs`, corporation_generation.hpp, over province.cpp's
/// `province_anchors` — the anchor `seed_province_holders` reads). CALLED, not
/// restated (the review's fix round), so the census cannot read a different
/// province from the cap. @p shared_out counts anchors whose tile holds more than
/// one centre, where the summed scale and the largest single centre differ — a
/// diagnostic of the clamp, read off the same anchors.
std::map<std::uint32_t, int> census_province_rungs(const world& w, int& shared_out)
{
    shared_out = 0;
    std::map<entity_id, int> centres_on_tile;
    for (const auto& [cid, tid] : w.population_centre_tile)
        if (w.population_centres.count(cid) != 0)
            ++centres_on_tile[tid];
    for (const auto& [pv, anchor] : province_anchors(w))
        if (centres_on_tile[anchor.tile] > 1) ++shared_out;
    return province_centre_rungs(w);
}

census_row census_one(std::uint32_t seed)
{
    census_row row;
    row.seed = seed;

    world_params p{};   // the shipped descriptor: the 1960 epoch, the span on (BL-1044)
    p.seed = seed;
    lua_state lua;
    auto out = std::make_unique<app_start_world>();
    charter_spend_report rep;
    harness_charter_input in;   // null budget: the world's own stockpile, as the app
    in.report = &rep;
    build_app_start_world(lua, p, *out, in);
    const world& w = out->w;
    const stockpile_budget& sb = out->land.stockpile;
    const charter_spend_params& spend = out->land.stockpile_spend;

    row.stock_total      = sb.points_total;
    row.stock_to_centres = sb.points_to_centres;
    row.stock_unspent    = sb.unspent;
    row.centres          = static_cast<int>(sb.budget.points().size());
    row.firm_price       = spend.firm_price_points;
    row.specialist_price = spend.specialist_price_points();
    row.window_radius    = spend.window_radius;
    row.kind = sb.budget.empty() ? 'E' : rep.refused ? 'R' : rep.fell_back ? 'F' : 'B';

    row.budgeted = rep.points_budgeted;
    row.spent    = rep.points_spent;
    for (const charter_unspent& u : rep.unspent)
    {
        row.u_pts[static_cast<std::size_t>(u.reason)] += u.points;
        ++row.u_rows[static_cast<std::size_t>(u.reason)];
    }
    std::int64_t u_sum = 0;
    for (const std::int64_t v : row.u_pts) u_sum += v;
    row.balanced = sb.budget.empty()
        || (rep.points_budgeted == sb.budget.total() && rep.points_spent + u_sum == rep.points_budgeted
            && rep.points_unspent == u_sum);

    row.specialists     = static_cast<int>(rep.specialists.size());
    row.budget_firms    = static_cast<int>(rep.firms.size());
    row.all_specialists = static_cast<int>(out->land.specialists.size());
    for (const charter_body_record& br : rep.bodies)
    {
        row.body_firms_max  = std::max(row.body_firms_max, static_cast<int>(br.firms));
        row.density_ceiling = std::max(row.density_ceiling, static_cast<int>(br.density_ceiling));
    }

    // Pass 6's firms: the background firms the apply laid that the spend did not
    // charter (a fallen-back or refused world lays the legacy passes).
    const std::set<entity_id> budget_ids(rep.firms.begin(), rep.firms.end());
    std::map<std::uint32_t, int> by_prov_budget, by_prov_p6;
    for (const charter_record& r : rep.charters)
    {
        if (r.specialist) continue;
        const std::uint32_t pv = w.provinces.province_of(r.anchor_tile);
        if (pv == 0) ++row.budget_no_prov;
        else         ++by_prov_budget[pv];
    }
    for (const entity_id f : out->land.firms)
    {
        if (budget_ids.count(f) != 0) continue;
        ++row.pass6_firms;
        const corporation_component& corp = w.corporations.at(f);
        if (corp.assets.empty()) continue;
        const auto bit = w.buildings.find(corp.assets.front());   // the anchor: Pass 6 reads the cap here
        if (bit == w.buildings.end()) continue;
        const std::uint32_t pv = w.provinces.province_of(bit->second.tile);
        if (pv != 0) ++by_prov_p6[pv];
    }
    for (const auto& [pv, n] : by_prov_budget)
    {
        ++row.prov_with_budget;
        if (n == 2) ++row.prov_at2_budget;
        if (n > 2)  ++row.prov_over2_budget;
    }
    for (const auto& [pv, n] : by_prov_p6)
    {
        ++row.prov_with_p6;
        if (n == 2) ++row.prov_at2_p6;
        if (n > 2)  ++row.prov_over2_p6;
    }

    // BL-1146: the province's centre rung, and the firms by it.
    const std::map<std::uint32_t, int> rungs = census_province_rungs(w, row.shared_anchor_tiles);
    const auto rung_of = [&](std::uint32_t pv) {
        const auto it = rungs.find(pv);
        return it == rungs.end() ? 0 : it->second;
    };
    for (const auto& [pv, n] : by_prov_budget)
    {
        const int r = rung_of(pv);
        row.budget_firms_by_rung[static_cast<std::size_t>(r)] += n;
        row.max_budget_by_rung[static_cast<std::size_t>(r)] =
            std::max(row.max_budget_by_rung[static_cast<std::size_t>(r)], n);
        row.max_prov_budget = std::max(row.max_prov_budget, n);
    }
    for (const auto& [pv, n] : by_prov_p6)
    {
        row.p6_firms_by_rung[static_cast<std::size_t>(rung_of(pv))] += n;
        row.max_prov_p6 = std::max(row.max_prov_p6, n);
    }
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == w.home_body) ++row.markets;
    row.winner_composite = out->land.search.winner_score.composite;
    row.winner_placement = out->land.search.winner.placement_seed;
    row.winner_tier      = out->land.search.winner.road_tier;

    // Land provinces: on the bodies holding a budgeted centre, and world-wide.
    std::set<entity_id> charter_bodies;
    for (const auto& [c, pts] : sb.budget.points())
    {
        const auto ct = w.population_centre_tile.find(c);
        if (ct == w.population_centre_tile.end()) continue;
        const auto t = w.tiles.find(ct->second);
        if (t != w.tiles.end()) charter_bodies.insert(t->second.body);
    }
    std::vector<int> sizes;
    for (const province& pr : w.provinces.provinces)
    {
        if (province_kind_of(w, pr) != province_kind::land) continue;
        const int n = static_cast<int>(pr.tiles.size());
        ++row.land_provs_world;
        row.largest_prov_world = std::max(row.largest_prov_world, n);
        if (charter_bodies.count(pr.body) == 0) continue;
        ++row.land_provs;
        row.largest_prov = std::max(row.largest_prov, n);
        sizes.push_back(n);
        row.provs_by_rung[static_cast<std::size_t>(rung_of(pr.id))] += 1;
    }
    if (!sizes.empty())
    {
        std::sort(sizes.begin(), sizes.end());
        row.median_prov = sizes[sizes.size() / 2];
    }

    // Where the budget concentrates: centres in SPEND ORDER (points descending,
    // ties to the lower id — INDUSTRIALISATION.md § 1).
    std::vector<std::pair<entity_id, std::int32_t>> order(sb.budget.points().begin(),
                                                          sb.budget.points().end());
    std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    std::map<entity_id, std::int64_t> pcap_at;
    for (const charter_unspent& u : rep.unspent)
        if (u.reason == charter_unspent_reason::province_cap)
            pcap_at[u.centre] += u.points;
    std::map<entity_id, int> firms_at;
    for (const charter_record& r : rep.charters)
        if (!r.specialist) ++firms_at[r.centre];
    double wsum = 0.0, wtot = 0.0;
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        const entity_id c = order[i].first;
        const std::int32_t pts = order[i].second;
        int tiles = 0;
        const int provs = census_window_provinces(w, c, spend.window_radius, tiles);
        wsum += static_cast<double>(pts) * provs;
        wtot += static_cast<double>(pts);
        const std::int64_t pc = pcap_at.count(c) ? pcap_at.at(c) : 0;
        if (provs == 1)
        {
            ++row.one_prov_centres;
            row.one_prov_pts += pts;
            if (pc > 0) ++row.pcap_centres_one_prov;
        }
        if (i == 0)
        {
            row.top_pts          = pts;
            row.top_firms        = firms_at.count(c) ? firms_at.at(c) : 0;
            row.top_pcap         = pc;
            row.top_window_provs = provs;
            row.top_window_tiles = tiles;
        }
        if (i < 3)
        {
            row.top3_pts  += pts;
            row.top3_pcap += pc;
        }
    }
    row.wmean_window_provs = wtot > 0.0 ? wsum / wtot : 0.0;
    return row;
}

double census_pct(std::int64_t part, std::int64_t whole)
{
    return whole > 0 ? 100.0 * static_cast<double>(part) / static_cast<double>(whole) : 0.0;
}

void part_four_firm_census(const std::vector<std::uint32_t>& seeds)
{
    std::printf("\n--- PART 4 (BL-1146): the firm census — the shipped start, the per-province cap ---\n");
    std::printf("     kind: B budget world, F fell back (no specialist affordable, NR-910; Pass 6 laid),\n"
                "     R refused, E empty budget. Firms per province by the ANCHOR's province, as each cap reads it.\n");
    std::fflush(stdout);

    std::vector<census_row> rows;
    for (const std::uint32_t seed : seeds)
    {
        census_row r = census_one(seed);
        check(r.balanced, "4.1 seed " + std::to_string(seed) + ": the charter account closes");
        // One compact line per seed as it lands, so a long run shows progress.
        std::printf("     [seed %u] kind %c: %d specialists, %d budget firms, %d Pass 6 firms; "
                    "province_cap %lld points in %d bookings; %d land provinces, largest %d\n",
                    r.seed, r.kind, r.specialists, r.budget_firms, r.pass6_firms,
                    static_cast<long long>(r.u_pts[static_cast<std::size_t>(charter_unspent_reason::province_cap)]),
                    r.u_rows[static_cast<std::size_t>(charter_unspent_reason::province_cap)],
                    r.land_provs, r.largest_prov);
        std::fflush(stdout);
        rows.push_back(r);
    }

    const auto P = [](charter_unspent_reason r) { return static_cast<std::size_t>(r); };

    // TABLE A — firms and provinces.
    std::printf("\n  TABLE A — firms and provinces (land provinces on the charter bodies; world-wide in [])\n");
    std::printf("  %-5s %1s %5s %5s %6s %6s | %6s %7s %6s %11s | %8s %6s %5s %6s | %8s %6s %5s\n",
                "seed", "k", "spec", "all_s", "bfirms", "p6firm", "landpv", "largest", "median",
                "[world pv/max]", "pv_bfirm", "at2_b", ">2_b", "b_nopv", "pv_p6", "at2_p6", ">2_p6");
    census_row tot;
    for (const census_row& r : rows)
    {
        char world_col[32];
        std::snprintf(world_col, sizeof world_col, "[%d/%d]", r.land_provs_world, r.largest_prov_world);
        std::printf("  %-5u %c %5d %5d %6d %6d | %6d %7d %6d %11s | %8d %6d %5d %6d | %8d %6d %5d\n",
                    r.seed, r.kind, r.specialists, r.all_specialists, r.budget_firms, r.pass6_firms,
                    r.land_provs, r.largest_prov, r.median_prov, world_col, r.prov_with_budget,
                    r.prov_at2_budget, r.prov_over2_budget, r.budget_no_prov, r.prov_with_p6,
                    r.prov_at2_p6, r.prov_over2_p6);
        tot.specialists += r.specialists;         tot.all_specialists += r.all_specialists;
        tot.budget_firms += r.budget_firms;       tot.pass6_firms += r.pass6_firms;
        tot.land_provs += r.land_provs;           tot.largest_prov = std::max(tot.largest_prov, r.largest_prov);
        tot.prov_with_budget += r.prov_with_budget; tot.prov_at2_budget += r.prov_at2_budget;
        tot.prov_over2_budget += r.prov_over2_budget; tot.budget_no_prov += r.budget_no_prov;
        tot.prov_with_p6 += r.prov_with_p6;       tot.prov_at2_p6 += r.prov_at2_p6;
        tot.prov_over2_p6 += r.prov_over2_p6;
    }
    std::printf("  %-5s %1s %5d %5d %6d %6d | %6d %7d %6s %11s | %8d %6d %5d %6d | %8d %6d %5d\n",
                "POOL", "", tot.specialists, tot.all_specialists, tot.budget_firms, tot.pass6_firms,
                tot.land_provs, tot.largest_prov, "", "", tot.prov_with_budget, tot.prov_at2_budget,
                tot.prov_over2_budget, tot.budget_no_prov, tot.prov_with_p6, tot.prov_at2_p6,
                tot.prov_over2_p6);

    // TABLE B — the budget and every unspent reason (points; bookings in ()).
    std::printf("\n  TABLE B — the charter budget's account, points (bookings = one row per centre and reason)\n");
    std::printf("  %-5s %1s %6s %6s %10s %10s | %14s %6s %5s | %12s %12s %12s %12s %12s %12s %12s %12s\n",
                "seed", "k", "price", "spec$", "budgeted", "spent", "province_cap", "%bud", "firms",
                "window_exh", "no_gap", "remainder", "density_ceil", "share_unpl", "late_short",
                "no_special", "other");
    std::int64_t tb = 0, ts = 0;
    std::array<std::int64_t, charter_unspent_reason_count> tu{};
    std::array<int, charter_unspent_reason_count> tr{};
    int worlds_capped = 0;
    double max_share = 0.0;
    std::uint32_t max_share_seed = 0;
    std::int64_t pcap_firms_tot = 0;
    for (const census_row& r : rows)
    {
        const auto cell = [&](charter_unspent_reason why) {
            static char buf[16][32];
            static int k = 0;
            char* s = buf[k++ & 15];
            std::snprintf(s, 32, "%lld(%d)", static_cast<long long>(r.u_pts[P(why)]), r.u_rows[P(why)]);
            return s;
        };
        const std::int64_t other = r.u_pts[P(charter_unspent_reason::no_nation)]
                                 + r.u_pts[P(charter_unspent_reason::body_cap)]
                                 + r.u_pts[P(charter_unspent_reason::refused)];
        const std::int64_t pc = r.u_pts[P(charter_unspent_reason::province_cap)];
        const std::int64_t pc_firms = r.firm_price > 0 ? pc / r.firm_price : 0;
        const double share = census_pct(pc, r.budgeted);
        if (pc > 0) ++worlds_capped;
        if (share > max_share) { max_share = share; max_share_seed = r.seed; }
        pcap_firms_tot += pc_firms;
        std::printf("  %-5u %c %6d %6lld %10lld %10lld | %14s %5.1f%% %5lld | %12s %12s %12s %12s %12s %12s %12s %12lld\n",
                    r.seed, r.kind, r.firm_price, static_cast<long long>(r.specialist_price),
                    static_cast<long long>(r.budgeted), static_cast<long long>(r.spent),
                    cell(charter_unspent_reason::province_cap), share, static_cast<long long>(pc_firms),
                    cell(charter_unspent_reason::window_exhausted), cell(charter_unspent_reason::no_gap),
                    cell(charter_unspent_reason::remainder), cell(charter_unspent_reason::density_ceiling),
                    cell(charter_unspent_reason::share_unplaced), cell(charter_unspent_reason::late_shortfall),
                    cell(charter_unspent_reason::no_specialist), static_cast<long long>(other));
        tb += r.budgeted;
        ts += r.spent;
        for (int i = 0; i < charter_unspent_reason_count; ++i)
        {
            tu[static_cast<std::size_t>(i)] += r.u_pts[static_cast<std::size_t>(i)];
            tr[static_cast<std::size_t>(i)] += r.u_rows[static_cast<std::size_t>(i)];
        }
    }
    const auto tcell = [&](charter_unspent_reason why) {
        static char buf[16][32];
        static int k = 0;
        char* s = buf[k++ & 15];
        std::snprintf(s, 32, "%lld(%d)", static_cast<long long>(tu[P(why)]), tr[P(why)]);
        return s;
    };
    std::printf("  %-5s %1s %6s %6s %10lld %10lld | %14s %5.1f%% %5lld | %12s %12s %12s %12s %12s %12s %12s %12lld\n",
                "POOL", "", "", "", static_cast<long long>(tb), static_cast<long long>(ts),
                tcell(charter_unspent_reason::province_cap),
                census_pct(tu[P(charter_unspent_reason::province_cap)], tb),
                static_cast<long long>(pcap_firms_tot),
                tcell(charter_unspent_reason::window_exhausted), tcell(charter_unspent_reason::no_gap),
                tcell(charter_unspent_reason::remainder), tcell(charter_unspent_reason::density_ceiling),
                tcell(charter_unspent_reason::share_unplaced), tcell(charter_unspent_reason::late_shortfall),
                tcell(charter_unspent_reason::no_specialist),
                static_cast<long long>(tu[P(charter_unspent_reason::no_nation)]
                                       + tu[P(charter_unspent_reason::body_cap)]
                                       + tu[P(charter_unspent_reason::refused)]));
    std::printf("  province_cap binds (> 0 points) on %d of %zu worlds; the largest share is %.1f%% of "
                "a budget (seed %u)\n",
                worlds_capped, rows.size(), max_share, max_share_seed);

    // TABLE C — the stockpile before the spend.
    std::printf("\n  TABLE C — the stockpile before the spend (points; unspent here never reaches a centre)\n");
    std::printf("  %-5s %12s %12s %7s | %12s %12s %12s %12s %12s\n", "seed", "stock", "to_centres",
                "centres", "carve_drop", "carve_notile", "razed", "no_carved", "rejected");
    for (const census_row& r : rows)
        std::printf("  %-5u %12lld %12lld %7d | %12lld %12lld %12lld %12lld %12lld\n", r.seed,
                    static_cast<long long>(r.stock_total), static_cast<long long>(r.stock_to_centres),
                    r.centres,
                    static_cast<long long>(r.stock_unspent[0]), static_cast<long long>(r.stock_unspent[1]),
                    static_cast<long long>(r.stock_unspent[2]), static_cast<long long>(r.stock_unspent[3]),
                    static_cast<long long>(r.stock_unspent[4]));

    // TABLE D — where the budget concentrates.
    std::printf("\n  TABLE D — where the budget concentrates (centres in spend order; window = radius %d)\n",
                rows.empty() ? 0 : rows.front().window_radius);
    std::printf("  %-5s %7s | %10s %6s %6s %10s %8s %8s | %7s %9s | %8s %7s %8s %8s | %9s %9s\n",
                "seed", "centres", "top_pts", "%bud", "firms", "top_pcap", "win_pv", "win_tile",
                "top3%", "top3pcap%", "wmean_pv", "1pv_ctr", "1pv_%bud", "pcap1pv", "bodyfirms", "ceiling");
    for (const census_row& r : rows)
    {
        const std::int64_t pc = r.u_pts[P(charter_unspent_reason::province_cap)];
        std::printf("  %-5u %7d | %10d %5.1f%% %6d %10lld %8d %8d | %6.1f%% %8.1f%% | %8.2f %7d %7.1f%% %8d | %9d %9d\n",
                    r.seed, r.centres, r.top_pts, census_pct(r.top_pts, r.budgeted), r.top_firms,
                    static_cast<long long>(r.top_pcap), r.top_window_provs, r.top_window_tiles,
                    census_pct(r.top3_pts, r.budgeted), census_pct(r.top3_pcap, pc),
                    r.wmean_window_provs, r.one_prov_centres, census_pct(r.one_prov_pts, r.budgeted),
                    r.pcap_centres_one_prov, r.body_firms_max, r.density_ceiling);
    }

    // TABLE E (BL-1146) — firms by the RUNG of the province they stand in (the
    // province's anchor centre: 0 none, 1 village .. 5 megacity), and the most
    // any one province of each rung holds.
    static const char* const rung_name[6] = { "none", "village", "town", "city", "metro", "mega" };
    std::printf("\n  TABLE E — budget firms by the rung of the province's centre (the most in one province of that rung in [])\n");
    std::printf("  %-5s |", "seed");
    for (int k = 0; k < 6; ++k) std::printf(" %11s", rung_name[k]);
    std::printf(" | %6s %6s %6s | %6s\n", "maxpv", "p6", "maxp6", "shared");
    std::array<int, 6> tot_firms{}, tot_max{}, tot_provs{}, tot_p6{};
    int tot_maxpv = 0, tot_maxp6 = 0, tot_shared = 0;
    for (const census_row& r : rows)
    {
        std::printf("  %-5u |", r.seed);
        int p6 = 0;
        for (int k = 0; k < 6; ++k)
        {
            char cell[24];
            std::snprintf(cell, sizeof cell, "%d[%d]", r.budget_firms_by_rung[static_cast<std::size_t>(k)],
                          r.max_budget_by_rung[static_cast<std::size_t>(k)]);
            std::printf(" %11s", cell);
            tot_firms[static_cast<std::size_t>(k)] += r.budget_firms_by_rung[static_cast<std::size_t>(k)];
            tot_max[static_cast<std::size_t>(k)] = std::max(tot_max[static_cast<std::size_t>(k)],
                                                            r.max_budget_by_rung[static_cast<std::size_t>(k)]);
            tot_provs[static_cast<std::size_t>(k)] += r.provs_by_rung[static_cast<std::size_t>(k)];
            tot_p6[static_cast<std::size_t>(k)] += r.p6_firms_by_rung[static_cast<std::size_t>(k)];
            p6 += r.p6_firms_by_rung[static_cast<std::size_t>(k)];
        }
        std::printf(" | %6d %6d %6d | %6d\n", r.max_prov_budget, p6, r.max_prov_p6, r.shared_anchor_tiles);
        tot_maxpv = std::max(tot_maxpv, r.max_prov_budget);
        tot_maxp6 = std::max(tot_maxp6, r.max_prov_p6);
        tot_shared += r.shared_anchor_tiles;
    }
    std::printf("  %-5s |", "POOL");
    for (int k = 0; k < 6; ++k)
    {
        char cell[24];
        std::snprintf(cell, sizeof cell, "%d[%d]", tot_firms[static_cast<std::size_t>(k)],
                      tot_max[static_cast<std::size_t>(k)]);
        std::printf(" %11s", cell);
    }
    std::printf(" | %6d %6s %6d | %6d\n", tot_maxpv, "", tot_maxp6, tot_shared);
    std::printf("  land provinces on the charter bodies by rung, pooled:");
    for (int k = 0; k < 6; ++k)
        std::printf(" %s %d", rung_name[k], tot_provs[static_cast<std::size_t>(k)]);
    std::printf("\n  Pass 6 firms by rung, pooled:");
    for (int k = 0; k < 6; ++k)
        std::printf(" %s %d", rung_name[k], tot_p6[static_cast<std::size_t>(k)]);
    std::printf("\n");

    // TABLE F (BL-1146) — markets and the search's winner, per seed.
    std::printf("\n  TABLE F — home-body markets and the landscape search's winner\n");
    std::printf("  %-5s %7s | %10s %4s %12s\n", "seed", "markets", "placement", "tier", "composite");
    int tot_markets = 0;
    for (const census_row& r : rows)
    {
        std::printf("  %-5u %7d | %08X %4d %12.9f\n", r.seed, r.markets, r.winner_placement,
                    r.winner_tier, r.winner_composite);
        tot_markets += r.markets;
    }
    std::printf("  %-5s %7d\n", "POOL", tot_markets);

    // One machine-readable line per seed, for a before/after diff.
    for (const census_row& r : rows)
    {
        std::printf("CENSUS seed=%u kind=%c spec=%d bfirms=%d p6firms=%d budgeted=%lld pcap=%lld "
                    "density=%lld bodyfirms=%d maxpv=%d markets=%d placement=%08X tier=%d "
                    "composite=%.12g rung_firms",
                    r.seed, r.kind, r.specialists, r.budget_firms, r.pass6_firms,
                    static_cast<long long>(r.budgeted),
                    static_cast<long long>(r.u_pts[P(charter_unspent_reason::province_cap)]),
                    static_cast<long long>(r.u_pts[P(charter_unspent_reason::density_ceiling)]),
                    r.body_firms_max, r.max_prov_budget, r.markets, r.winner_placement, r.winner_tier,
                    r.winner_composite);
        for (int k = 0; k < 6; ++k) std::printf(" %d", r.budget_firms_by_rung[static_cast<std::size_t>(k)]);
        std::printf(" rung_max");
        for (int k = 0; k < 6; ++k) std::printf(" %d", r.max_budget_by_rung[static_cast<std::size_t>(k)]);
        std::printf("\n");
    }
    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv)
{
    bool          r8   = false;
    std::uint32_t seed = 28;
    bool          curve = false;
    bool          census = false;
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
        if (arg == "--reach" && a + 1 < argc)
        {
            const std::string v = argv[++a];
            if (v == "world") g_curve_reach = charter_price_reach::world;
            else if (v == "landmass") g_curve_reach = charter_price_reach::landmass;
            else if (v == "market") g_curve_reach = charter_price_reach::market;
            else
            {
                std::printf("--reach: '%s' is not world, landmass or market\n", v.c_str());
                return 2;
            }
            continue;
        }
        if (arg == "--firm-census")
        {
            census = true;
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
                    "[--seat-curve [--seeds a,b] [--pairs d:m,...]] [--firm-census [--seeds a,b]]\n");
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
    if (census)
        part_four_firm_census(curve_seeds);   // --seeds names them; the library otherwise

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILURES", failures,
                failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
