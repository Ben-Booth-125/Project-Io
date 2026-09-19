// charter_refusal_probe — the charter spend's CONTRACT, case by case (BL-1039, BL-1060).
//
// Things in src/world/charter_budget.hpp and the budget walk are cheap to state
// and expensive to reach through a generated world (a full player_seed_sweep row
// is minutes and ~3 GB), so this probe asks them directly — no generation, no
// Lua, milliseconds:
//
//  1. `charter_spend_refusal`, the one gate a charter budget passes before any
//     mutation (a refused spend is today's world): every no-default number
//     refused where a rule reads it, and refused where set and unread — a
//     per-good cap under the lifted rule, a ceiling under a legacy rule — which
//     no sweep row ever reaches. Every refusal case names the CLAUSE it expects,
//     so a case refused by some other clause still fails. BL-1060 adds a
//     negative firm price, a non-positive specialist price under a positive firm
//     price, and the int64 range bound on c, the firm price and the budget.
//  2. The square-root rule's ANCHOR (Ben, 2026-09-18: B and B_ref in the same
//     units): at B = B_ref = c x |G| x firm price the cap is c EXACTLY, over a
//     grid of c, |G| and firm prices; the rule steps up only past (k/c)^2 B_ref;
//     B's per-centre term (`charter_centre_firm_points`) nets out the specialist
//     price in whole firm charters; and the integer root beneath it, which the
//     max(c, ...) floor hides from every cap-level check.
//  3. THE TURN (BL-1060), on a HAND-BUILT world — one body, one nation, one
//     centre, two recipes — through `charter_web_from_budget` itself: a good the
//     centre's window has no deposit for is SKIPPED and the processing goods
//     after it still charter (NR-903); the legacy rule on the same world still
//     stops at its first failed placement; and a good only the walk's own firms
//     made short is booked `late_shortfall`, never `no_gap`.
//
// Run:   bash tools/verify/build_lua_harness.sh charter_refusal_probe
//        (or node tools/verify/build_harness.js charter_refusal_probe — it needs no Lua)
//        ./build_gen/verify/charter_refusal_probe.exe
// Exit:  0 every case as expected, 1 otherwise.

#include "world/charter_budget.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <memory>

namespace {

int g_fail = 0;

/// @p clause: a substring the refusal text must carry (null: any refusal).
void expect(const char* name, const charter_budget& b, const charter_spend_params& s, bool refused,
            const char* clause = nullptr)
{
    const char* why = charter_spend_refusal(b, s);
    const bool got = why != nullptr;
    bool ok = got == refused;
    if (ok && refused && clause != nullptr && std::strstr(why, clause) == nullptr)
        ok = false;   // refused, but by another clause
    std::printf("  [%s] %-62s %s%s\n", ok ? "PASS" : "FAIL", name,
                got ? "refused: " : "accepted", got ? why : "");
    if (!ok)
        ++g_fail;
}

void expect_eq(const char* name, long long got, long long want)
{
    const bool ok = got == want;
    std::printf("  [%s] %-62s got %lld, want %lld\n", ok ? "PASS" : "FAIL", name, got, want);
    if (!ok)
        ++g_fail;
}

void expect_true(const char* name, bool ok, const char* detail = "")
{
    std::printf("  [%s] %-62s %s\n", ok ? "PASS" : "FAIL", name, detail);
    if (!ok)
        ++g_fail;
}

/// A spend every rule accepts: the synthetic numbers (firm 1, specialist 4,
/// c 8, guard 200) under the fixed rule.
charter_spend_params base()
{
    charter_spend_params s;
    s.firm_price_points        = 1;
    s.specialist_firm_charters = 4;
    s.window_radius            = 4;
    s.resource_cap_rule        = charter_cap_rule::fixed;
    s.per_resource_firm_cap    = 8;
    s.max_firms_per_body       = 200;
    return s;
}

charter_spend_params sqrt_spend()
{
    auto s = base();
    s.resource_cap_rule = charter_cap_rule::sqrt_capital;
    s.density_ceiling   = 120;
    return s;
}

// ---------------------------------------------------------------------------
// Part 3's world: one body, one nation owning every tile, one centre.
// ---------------------------------------------------------------------------
namespace turn {

constexpr int k_w = 24, k_h = 12;          // the body's grid
constexpr int k_cx = 6, k_cy = 6;          // the centre's tile
constexpr int k_radius = 4;                // the spend's window radius
constexpr std::size_t k_raw   = static_cast<std::size_t>(resource_type::iron_ore); // index 0: FIRST in the turn
constexpr std::size_t k_mill1 = static_cast<std::size_t>(resource_type::steel);
constexpr std::size_t k_mill2 = static_cast<std::size_t>(resource_type::planks);
constexpr std::size_t k_late  = static_cast<std::size_t>(resource_type::power);    // never in G

enum class ground
{
    everywhere,      ///< iron-ore deposits on every tile
    outside_window,  ///< only on tiles farther than radius 6 from the centre: the
                     ///< window (radius 4) holds no deposit tile at all
};

struct fixture
{
    world           w;
    recipe_registry reg;
    entity_id       centre = null_entity;
};

/// @p upkeep: every processing facility draws power as upkeep — a good no
/// base installation draws, so it is not in G, and only the walk's own works
/// can make it short.
void build(fixture& f, ground g, bool upkeep)
{
    world& w = f.w;
    const entity_id body = w.create_entity();
    {
        body_component bc{};
        bc.name        = "FixtureBody";
        bc.grid_width  = k_w;
        bc.grid_height = k_h;
        w.bodies[body] = bc;
    }
    const entity_id nation = w.create_entity();
    nation_component nc{};
    nc.name = "Veyl";
    entity_id centre_tile = null_entity;
    for (int y = 0; y < k_h; ++y)
        for (int x = 0; x < k_w; ++x)
        {
            const entity_id tid = w.create_entity();
            tile_component tc{};
            tc.body   = body;
            tc.grid_x = x;
            tc.grid_y = y;
            tc.substrate = terrain_substrate::barren;
            int dx = std::abs(x - k_cx);
            if (dx > k_w / 2)
                dx = k_w - dx;
            const int dy = y - k_cy;
            const bool deposit = (g == ground::everywhere) || (dx * dx + dy * dy > 36);
            if (deposit)
                tc.resource_deposit[k_raw] = 1.0f;
            w.tiles[tid] = tc;
            nc.tiles.push_back(tid);   // raster order: the nation's stored order
            w.tile_to_nation[tid] = nation;
            if (x == k_cx && y == k_cy)
                centre_tile = tid;
        }
    w.nations[nation] = nc;

    f.centre = w.create_entity();
    population_centre_component pc{};
    pc.scale = 5;
    w.population_centres[f.centre]     = pc;
    w.population_centre_tile[f.centre] = centre_tile;

    // G = { iron ore, steel, planks }: the households want all three, far more
    // than any firm here makes (every base rate is 0), so each stays short until
    // it reaches its cap. Iron ore has no recipe, so its firm is an extraction
    // firm; steel and planks each have one, so theirs are processing firms.
    population_demand_params pd;
    pd.demand_basket[k_raw]   = 100.0f;
    pd.demand_basket[k_mill1] = 100.0f;
    pd.demand_basket[k_mill2] = 100.0f;
    f.reg.set_population_demand(pd);
    recipe steelworks;
    steelworks.name = "fixture_steelworks";
    steelworks.inputs[k_raw]    = 1.0f;
    steelworks.outputs[k_mill1] = 1.0f;
    f.reg.add_recipe(steelworks);
    recipe sawmill;
    sawmill.name = "fixture_sawmill";
    sawmill.inputs[static_cast<std::size_t>(resource_type::timber)] = 1.0f;
    sawmill.outputs[k_mill2] = 1.0f;
    f.reg.add_recipe(sawmill);
    if (upkeep)
    {
        building_upkeep_params up;
        up.goods[static_cast<std::size_t>(building_type::processing_facility)]
                [static_cast<std::size_t>(era_band::any)][k_late] = 1.0f;
        f.reg.set_building_upkeep(up);
    }
}

/// The turn's spend: firm 1 point, a specialist no centre here can afford (100),
/// c 2, so |G| 3 gives B_ref 6 and an 8-point budget keeps cap 2 (floor(2 x
/// sqrt(8/6)) = 2): six firms fill G, and two points are left to book.
charter_spend_params spend(charter_cap_rule rule)
{
    charter_spend_params s;
    s.firm_price_points        = 1;
    s.specialist_firm_charters = 100;
    s.window_radius            = k_radius;
    s.province_cap             = true;
    s.resource_cap_rule        = rule;
    s.per_resource_firm_cap    = 2;
    s.max_firms_per_body       = 200;
    s.density_ceiling          = (rule == charter_cap_rule::sqrt_capital) ? 120 : 0;
    return s;
}

struct reading
{
    charter_spend_report rep;
    std::array<int, resource_count> firms{};          ///< per good, from the records
    std::array<int, resource_count> processing{};     ///< per good, firms with a processing focus
    std::array<long long, charter_unspent_reason_count> unspent{};
    bool balanced = false;
};

reading run(ground g, bool upkeep, charter_cap_rule rule)
{
    auto f = std::make_unique<fixture>();
    build(*f, g, upkeep);
    const charter_budget budget(std::map<entity_id, std::int32_t>{ { f->centre, 8 } });
    reading out;
    charter_web_from_budget(f->w, f->reg, budget, spend(rule), /*seed=*/1060u, /*settle=*/nullptr,
                            &out.rep);
    for (const charter_record& r : out.rep.charters)
    {
        if (r.specialist || r.good >= resource_count)
            continue;
        ++out.firms[r.good];
        if (f->w.corporations.at(r.corp).focus == industrial_focus::processing)
            ++out.processing[r.good];
    }
    long long unspent = 0;
    for (const charter_unspent& u : out.rep.unspent)
    {
        out.unspent[static_cast<std::size_t>(u.reason)] += u.points;
        unspent += u.points;
    }
    out.balanced = out.rep.points_budgeted == 8 && out.rep.points_spent + unspent == 8
                && out.rep.points_unspent == unspent;
    return out;
}

void print(const char* label, const reading& r)
{
    std::printf("  %s: firms iron_ore %d (processing %d), steel %d (processing %d), planks %d "
                "(processing %d); unspent", label, r.firms[k_raw], r.processing[k_raw],
                r.firms[k_mill1], r.processing[k_mill1], r.firms[k_mill2], r.processing[k_mill2]);
    for (int i = 0; i < charter_unspent_reason_count; ++i)
        if (r.unspent[static_cast<std::size_t>(i)] != 0)
            std::printf(" %s %lld", charter_unspent_reason_name(static_cast<charter_unspent_reason>(i)),
                        r.unspent[static_cast<std::size_t>(i)]);
    std::printf("%s\n", r.balanced ? "" : " [UNBALANCED]");
}

} // namespace turn

} // namespace

int main()
{
    const charter_budget empty;
    const charter_budget small(std::map<entity_id, std::int32_t>{ { 1, 40 }, { 2, 7 } });

    std::printf("charter_refusal_probe — charter_spend_refusal, case by case\n");

    // --- the empty budget is never refused, whatever the params ---
    expect("empty budget, default (unpriced) params", empty, charter_spend_params{}, false);

    // --- no shipped default ---
    expect("default params on a non-empty budget", small, charter_spend_params{}, true);
    expect("base spend (fixed)", small, base(), false);
    { auto s = base(); s.max_firms_per_body = 0;
      expect("guard 0", small, s, true, "max_firms_per_body"); }
    { auto s = base(); s.per_resource_firm_cap = 0;
      expect("fixed rule, c 0", small, s, true, "per_resource_firm_cap must be > 0"); }

    // --- BL-1060: the prices, one clause each ---
    { auto s = base(); s.firm_price_points = 0;
      expect("firm price 0", small, s, true, "firm_price_points must be > 0"); }
    { auto s = base(); s.firm_price_points = -1;
      expect("firm price -1 (negative), specialist 4", small, s, true, "firm_price_points must be > 0"); }
    { auto s = base(); s.specialist_firm_charters = 0;
      expect("specialist 0 charters, firm price 1", small, s, true, "specialist_firm_charters"); }
    { auto s = base(); s.specialist_firm_charters = -1;
      expect("specialist -1 charters (price -1), firm price 1", small, s, true,
             "specialist_firm_charters"); }

    // --- lifted: c is unread, so a set c is refused ---
    { auto s = base(); s.resource_cap_rule = charter_cap_rule::lifted;
      expect("lifted rule, c 8 set (unread)", small, s, true, "not read under the lifted"); }
    { auto s = base(); s.resource_cap_rule = charter_cap_rule::lifted; s.per_resource_firm_cap = 0;
      expect("lifted rule, c 0", small, s, false); }

    // --- the density ceiling: sqrt only, 0 < ceiling < guard ---
    { auto s = base(); s.density_ceiling = 120;
      expect("fixed rule with a ceiling set", small, s, true, "read only by the sqrt_capital"); }
    { auto s = base(); s.resource_cap_rule = charter_cap_rule::sqrt_capital;
      expect("sqrt rule, no ceiling", small, s, true, "density_ceiling must be > 0"); }
    expect("sqrt rule, ceiling 120", small, sqrt_spend(), false);
    { auto s = sqrt_spend(); s.density_ceiling = 200;
      expect("sqrt rule, ceiling at the guard", small, s, true, "below max_firms_per_body"); }
    { auto s = sqrt_spend(); s.per_resource_firm_cap = 0;
      expect("sqrt rule, c 0", small, s, true, "per_resource_firm_cap must be > 0"); }

    // --- BL-1060: the int64 range, each bound TIGHT at |G| = resource_count ---
    // Accepted AT the bound, where the arithmetic is exact; refused ONE PAST it,
    // where it would not be. lim = INT64_MAX / resource_count.
    std::printf("\nthe arithmetic's range (BL-1060) — lim = INT64_MAX / %zu\n", resource_count);
    constexpr std::int64_t k_max = std::numeric_limits<std::int64_t>::max();
    const std::int64_t lim = k_max / static_cast<std::int64_t>(resource_count);
    const int g_all = static_cast<int>(resource_count);
    {
        // B_ref = c x |G| x fp, under the fixed rule (which reports it).
        const std::int32_t c = 1 << 30;
        const std::int64_t fp_max = lim / c;
        auto s = base();
        s.per_resource_firm_cap = c;
        s.firm_price_points = static_cast<std::int32_t>(fp_max);
        expect("fixed, c 2^30, fp at the B_ref bound", small, s, false);
        s.firm_price_points = static_cast<std::int32_t>(fp_max + 1);
        expect("fixed, c 2^30, fp one past the B_ref bound", small, s, true, "(B_ref)");
    }
    {
        // cap(B_ref) == c needs c x B_ref = c^2 x |G| x fp in int64.
        const std::int32_t c = 65536;
        const std::int64_t fp_max = lim / c / c;
        auto s = sqrt_spend();
        s.per_resource_firm_cap = c;
        s.firm_price_points = static_cast<std::int32_t>(fp_max);
        expect("sqrt, c 65536, fp at the square-root bound", small, s, false);
        const std::int64_t bref_in = static_cast<std::int64_t>(c) * g_all * fp_max;
        expect_eq("  ... and there cap(B_ref) == c exactly (|G| = roster)",
                  charter_sqrt_per_good_cap(c, bref_in, g_all, static_cast<std::int32_t>(fp_max)), c);
        s.firm_price_points = static_cast<std::int32_t>(fp_max + 1);
        expect("sqrt, c 65536, fp one past the square-root bound", small, s, true, "(the square root at");
        const std::int64_t bref_out = static_cast<std::int64_t>(c) * g_all * (fp_max + 1);
        const std::int32_t cap_out =
            charter_sqrt_per_good_cap(c, bref_out, g_all, static_cast<std::int32_t>(fp_max + 1));
        char detail[96];
        std::snprintf(detail, sizeof detail, "cap(B_ref) %d, not c %d: why it is refused",
                      static_cast<int>(cap_out), static_cast<int>(c));
        expect_true("  ... one past, cap(B_ref) saturates", cap_out != c, detail);
    }
    {
        // c x B on a body, B <= the budget's total. c 4e8 passes the square-root
        // bound (c x fp <= lim / c); ten int32-max centres fit, eleven do not.
        const std::int32_t c = 400000000;
        auto s = sqrt_spend();
        s.per_resource_firm_cap    = c;
        s.specialist_firm_charters = 1;
        std::map<entity_id, std::int32_t> pts;
        for (entity_id id = 1; id <= 10; ++id)
            pts[id] = std::numeric_limits<std::int32_t>::max();
        const charter_budget ten(pts);
        pts[11] = std::numeric_limits<std::int32_t>::max();
        const charter_budget eleven(pts);
        expect("sqrt, c 4e8, 10 x int32-max points (c x total fits)", ten, s, false);
        expect("sqrt, c 4e8, 11 x int32-max points (c x total does not)", eleven, s, true,
               "the budget's total");

        // What the refused budget would have done: B for one body holding all
        // eleven, and the EXACT cap by the split c x B / D = c (B / D) + c (B % D) / D,
        // against what the function returns once c x B passes int64.
        std::int64_t b_all = 0;
        for (const auto& kv : eleven.points())
            b_all += charter_centre_firm_points(kv.second, s);
        const std::int64_t d = static_cast<std::int64_t>(g_all) * s.firm_price_points;
        const std::int64_t y = static_cast<std::int64_t>(c) * (b_all / d)
                             + (static_cast<std::int64_t>(c) * (b_all % d)) / d;
        const std::int64_t exact = std::max<std::int64_t>(c, charter_isqrt(y));
        const std::int32_t got = charter_sqrt_per_good_cap(c, b_all, g_all, s.firm_price_points);
        char detail[128];
        std::snprintf(detail, sizeof detail, "B %lld: the function %d, the exact cap %lld",
                      static_cast<long long>(b_all), static_cast<int>(got), static_cast<long long>(exact));
        // The exact cap fits int32, so the saturated one is not an honest clamp.
        expect_true("  ... eleven: c x B saturates to a WRONG cap",
                    got != exact && exact < std::numeric_limits<std::int32_t>::max(), detail);
        std::int64_t b_ten = 0;
        for (const auto& kv : ten.points())
            b_ten += charter_centre_firm_points(kv.second, s);
        const std::int64_t y10 = static_cast<std::int64_t>(c) * (b_ten / d)
                               + (static_cast<std::int64_t>(c) * (b_ten % d)) / d;
        expect_eq("  ... ten: the function gives the exact cap",
                  charter_sqrt_per_good_cap(c, b_ten, g_all, s.firm_price_points),
                  std::max<std::int64_t>(c, charter_isqrt(y10)));
    }

    // --- the anchor: cap(B_ref) == c exactly, over a grid ---
    std::printf("\nthe square-root rule's anchor — cap(B_ref = c x |G| x fp) == c\n");
    {
        int cases = 0, wrong = 0;
        for (std::int32_t c = 1; c <= 32; ++c)
            for (int g = 1; g <= 38; ++g)
                for (const std::int32_t fp : { 1, 2, 3, 7, 100, 12345 })
                {
                    const std::int64_t bref = static_cast<std::int64_t>(c) * g * fp;
                    ++cases;
                    if (charter_sqrt_per_good_cap(c, bref, g, fp) != c)
                        ++wrong;
                    // One point short of the reference still keeps c (the floor).
                    if (charter_sqrt_per_good_cap(c, bref - 1, g, fp) != c)
                        ++wrong;
                }
        char name[96];
        std::snprintf(name, sizeof name, "cap(B_ref) == c and cap(B_ref - 1) == c over %d cases", cases);
        expect_eq(name, wrong, 0);
    }
    // WHAT THE cap(B_ref - 1) HALF CAN AND CANNOT CATCH. cap = max(c, root), and
    // below B_ref the true root is below c, so the max(c, ...) floor answers c
    // whatever the root says. The half therefore catches only an OVERSHOOT just
    // under the anchor — a reference understated (|G| or the price off low), or
    // a root stepped up a point early — never an undershoot: a rule that
    // returned c for every B, or a root off LOW anywhere below B_ref, passes it
    // and the anchor half both. The step rows below catch "always c"; the root
    // itself is checked directly under them, beneath any floor.
    std::printf("  note: cap(B_ref - 1) == c can only catch an overshoot below the anchor; the\n"
                "        max(c, ...) floor hides any undershoot there, so the root is tested bare below\n");
    // The step: cap reaches k exactly at B = (k/c)^2 B_ref (k^2 |G| fp / c when
    // it divides), and not one point before.
    {
        const std::int32_t c = 8, fp = 1;
        const int g = 11;                                    // the library seeds' |G|
        const std::int64_t bref = static_cast<std::int64_t>(c) * g * fp;   // 88
        expect_eq("c 8, |G| 11: B_ref 88", bref, 88);
        // k = 16 at B = 4 B_ref (352), 15 one point before.
        expect_eq("cap(4 x B_ref = 352)", charter_sqrt_per_good_cap(c, 4 * bref, g, fp), 16);
        expect_eq("cap(351)", charter_sqrt_per_good_cap(c, 4 * bref - 1, g, fp), 15);
        // The smoke row's B: 4x on seed 0 is 348 points over 11 specialists at 4.
        expect_eq("cap(348 - 11 x 4 = 304)", charter_sqrt_per_good_cap(c, 304, g, fp), 14);
        // Below the reference: the floor holds.
        expect_eq("cap(1)", charter_sqrt_per_good_cap(c, 1, g, fp), 8);
        expect_eq("cap with |G| 0 keeps c", charter_sqrt_per_good_cap(c, 1000, 0, fp), 8);
    }
    // The root BENEATH the floor: floor(sqrt(x)) exact at and around every
    // perfect square, where a floating-point root or an off-by-one would slip.
    {
        int wrong = 0, cases = 0;
        for (std::int64_t k = 1; k <= 200000; ++k)
        {
            const std::int64_t sq = k * k;
            cases += 3;
            if (charter_isqrt(sq) != k)          ++wrong;
            if (charter_isqrt(sq - 1) != k - 1)  ++wrong;
            if (charter_isqrt(sq + 2 * k) != k)  ++wrong;   // (k+1)^2 - 1
        }
        for (const std::int64_t k : { 3037000498LL, 3037000499LL })   // floor(sqrt(INT64_MAX))
        {
            cases += 2;
            if (charter_isqrt(k * k) != k)       ++wrong;
            if (charter_isqrt(k * k - 1) != k - 1) ++wrong;
        }
        char name[96];
        std::snprintf(name, sizeof name, "isqrt at and around k^2 over %d cases", cases);
        expect_eq(name, wrong, 0);
        expect_eq("isqrt(INT64_MAX)", charter_isqrt(k_max), 3037000499LL);
        expect_eq("isqrt(0), isqrt(-5)", charter_isqrt(0) + charter_isqrt(-5), 0);
    }

    // --- B's per-centre term: points net of the specialist price, whole firms ---
    std::printf("\nB's per-centre term — charter_centre_firm_points\n");
    {
        auto s = base();                                     // firm 1, specialist 4
        expect_eq("40 points, affords a specialist: 36", charter_centre_firm_points(40, s), 36);
        expect_eq("4 points, exactly a specialist: 0", charter_centre_firm_points(4, s), 0);
        expect_eq("3 points, no specialist: 3", charter_centre_firm_points(3, s), 3);
        s.firm_price_points = 3;                             // specialist 12
        expect_eq("fp 3, 40 points: (40 - 12) / 3 = 9 firms = 27", charter_centre_firm_points(40, s), 27);
        expect_eq("fp 3, 11 points, no specialist: 3 firms = 9", charter_centre_firm_points(11, s), 9);
        expect_eq("0 points", charter_centre_firm_points(0, s), 0);
    }

    // --- THE TURN, on a hand-built world (BL-1060) ---
    std::printf("\nthe turn — one centre, 8 points, G = {iron_ore, steel, planks}, c 2 (cap 2)\n");
    using turn::ground;
    {
        // CONTROL: ground everywhere. Iron ore is FIRST in the turn and does get
        // an extraction firm, so the case below is not vacuous: the good is short
        // and placeable when the window has a deposit.
        const turn::reading r = turn::run(ground::everywhere, /*upkeep=*/false,
                                          charter_cap_rule::sqrt_capital);
        turn::print("control, deposits everywhere", r);
        expect_true("control: iron ore holds 2 extraction firms",
                    r.firms[turn::k_raw] == 2 && r.processing[turn::k_raw] == 0);
        expect_true("control: steel and planks hold 2 processing firms each",
                    r.processing[turn::k_mill1] == 2 && r.processing[turn::k_mill2] == 2);
        expect_true("control: the 2 points left after G fills are no_gap",
                    r.unspent[static_cast<std::size_t>(charter_unspent_reason::no_gap)] == 2
                    && r.balanced);
    }
    {
        // NR-903: the window holds NO deposit tile, so iron ore — the first good
        // in the turn — cannot place. It is skipped for this centre, and the
        // processing goods after it still charter. With a failed placement fatal,
        // as before NR-903, this centre chartered nothing at all.
        const turn::reading r = turn::run(ground::outside_window, /*upkeep=*/false,
                                          charter_cap_rule::sqrt_capital);
        turn::print("no deposit in the window", r);
        expect_true("NR-903: iron ore (first in the turn) gets no firm", r.firms[turn::k_raw] == 0);
        expect_true("NR-903: steel and planks still charter, 2 processing firms each",
                    r.processing[turn::k_mill1] == 2 && r.processing[turn::k_mill2] == 2);
        expect_true("NR-903: the centre stops only when no good can place; the rest "
                    "(4) is window_exhausted",
                    r.unspent[static_cast<std::size_t>(charter_unspent_reason::window_exhausted)] == 4
                    && r.unspent[static_cast<std::size_t>(charter_unspent_reason::no_gap)] == 0
                    && r.balanced);
    }
    {
        // THE LEGACY RULE ON THE SAME WORLD, unchanged: biggest gap first picks
        // iron ore (the gaps tie, the first wins), the placement fails, and the
        // centre stops there — every point window_exhausted.
        const turn::reading r = turn::run(ground::outside_window, /*upkeep=*/false,
                                          charter_cap_rule::fixed);
        turn::print("legacy fixed rule, same world", r);
        expect_true("legacy: the first failed placement still ends the centre",
                    r.firms[turn::k_raw] + r.firms[turn::k_mill1] + r.firms[turn::k_mill2] == 0
                    && r.unspent[static_cast<std::size_t>(charter_unspent_reason::window_exhausted)] == 8
                    && r.balanced);
    }
    {
        // BL-1060 (4): every works the walk charters draws power, which no base
        // installation did, so power is short but NOT in G, and the turn never
        // serves it. Once G fills, the rest is that shortfall — late_shortfall.
        const turn::reading r = turn::run(ground::everywhere, /*upkeep=*/true,
                                          charter_cap_rule::sqrt_capital);
        turn::print("works draw power (not in G)", r);
        const bool power_outside_g =
            !r.rep.bodies.empty()
            && std::find(r.rep.bodies.front().goods.begin(), r.rep.bodies.front().goods.end(),
                         static_cast<std::uint16_t>(turn::k_late)) == r.rep.bodies.front().goods.end();
        expect_true("late: power is not in G", power_outside_g);
        expect_true("late: the 2 points left are late_shortfall, not no_gap",
                    r.unspent[static_cast<std::size_t>(charter_unspent_reason::late_shortfall)] == 2
                    && r.unspent[static_cast<std::size_t>(charter_unspent_reason::no_gap)] == 0
                    && r.balanced);
    }

    std::printf("\n%s (%d failing)\n", g_fail == 0 ? "ALL PASS" : "FAILED", g_fail);
    return g_fail == 0 ? 0 : 1;
}
