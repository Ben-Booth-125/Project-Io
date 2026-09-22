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
//  3. THE TURN (BL-1060), on a HAND-BUILT world — one body, one nation, one or
//     two centres, a few recipes — through `charter_web_from_budget` itself.
//     A placement never sees the good, only the firm's FOCUS: an extraction
//     anchor takes the richest deposit on its tile, so a window with no free
//     deposit tile OF ANY KIND places no extraction firm, and one with no free
//     land places no works. On that ground:
//      * the extraction good first in the turn is SKIPPED and the processing
//        goods after it still charter (NR-903); the legacy rule on the same
//        world still stops at its first failed placement;
//      * the construction yard, a works, is skipped the same way where the
//        window is all farmland, and the mines still charter;
//      * a stop where the province cap took one focus's ground and the window
//        the other's books `province_cap`, not `window_exhausted`;
//      * a second centre serves the good the first centre skipped;
//      * where the ceiling binds each good keeps an EVEN SHARE (NR-905): a
//        first centre with no quarry leaves the ore's share for a second that
//        has one, and where no centre has one the gap is `share_unplaced`;
//      * the share is a RESERVATION, not a cap (NR-906), on a ten-good body: a
//        good that stops being short releases its share and the ceiling still
//        fills; a good placeable nowhere holds its reservation and costs the
//        body no more than it; a yard's place comes off the ceiling first; and
//        a ceiling too small for the turn and its yards is refused;
//      * a good only the walk's own firms made short is `late_shortfall`.
//  4. THE NO-SPECIALIST WORLD (BL-1044; Ben, 2026-09-21, NR-910), on the same
//     hand-built world: `charter_budget_affords_specialist` answers the walk's own
//     test (a centre on a nation's tile whose points cover the specialist), and
//     `apply_landscape_candidate`'s budget overload, given a budget no centre can
//     buy a specialist with, FALLS BACK before anything is chartered — the world
//     it leaves is the no-budget world's, byte for byte (the snapshot digest),
//     and the report says so: not refused, nothing spent, every point unspent as
//     `no_specialist`.
//
// Run:   bash tools/verify/build_lua_harness.sh charter_refusal_probe
//        (or node tools/verify/build_harness.js charter_refusal_probe — it needs no Lua)
//        ./build_gen/verify/charter_refusal_probe.exe
// Exit:  0 every case as expected, 1 otherwise.

#include "world_digest.hpp"   // world_state_digest: the fallback world IS the no-budget one
#include "world/charter_budget.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/landscape_search.hpp"  // apply_landscape_candidate's budget overload (NR-910)
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
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
#include <string>
#include <utility>
#include <vector>

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
// Part 3's world: one body, one nation owning every tile, one or two centres.
// ---------------------------------------------------------------------------
namespace turn {

constexpr int k_cx = 6, k_cy = 6;          // the ground's reference tile (and the default centre)
constexpr std::size_t k_raw   = static_cast<std::size_t>(resource_type::iron_ore); // index 0: FIRST in the turn
constexpr std::size_t k_mill1 = static_cast<std::size_t>(resource_type::steel);
constexpr std::size_t k_mill2 = static_cast<std::size_t>(resource_type::planks);
constexpr std::size_t k_late  = static_cast<std::size_t>(resource_type::power);    // never in G
constexpr std::size_t k_yard  = static_cast<std::size_t>(resource_type::construction_capacity);

/// Ten processed goods for the ten-good body (none is iron ore, power or capacity).
constexpr std::size_t k_ten[] = {
    static_cast<std::size_t>(resource_type::steel),          static_cast<std::size_t>(resource_type::refined_fuel),
    static_cast<std::size_t>(resource_type::food_rations),   static_cast<std::size_t>(resource_type::charcoal),
    static_cast<std::size_t>(resource_type::iron_blooms),    static_cast<std::size_t>(resource_type::silicon),
    static_cast<std::size_t>(resource_type::refined_copper), static_cast<std::size_t>(resource_type::ceramics),
    static_cast<std::size_t>(resource_type::planks),         static_cast<std::size_t>(resource_type::tools),
};

enum class ground
{
    everywhere,      ///< an iron-ore deposit on every tile
    outside_window,  ///< iron ore only on tiles farther than radius 6 from (6, 6): a
                     ///< window of radius 4 around (6, 6) or (8, 6) holds no deposit
                     ///< tile OF ANY KIND, so no extraction firm can anchor in it
    farmland,        ///< a farm deposit on every tile: extraction anchors anywhere,
                     ///< and no works can (a processing anchor refuses a farm tile)
    barren,          ///< no deposit anywhere: no extraction firm can anchor at all
};

struct centre_spec
{
    int          x = k_cx, y = k_cy;
    std::int32_t points = 8;
};

struct basket_row
{
    std::size_t good;
    float       demand;        ///< per scale point
    bool        works;         ///< a recipe makes it (a processing firm); else extraction
};

struct config
{
    ground g = ground::everywhere;
    int body_w = 24, body_h = 12, radius = 4;
    int scale = 5;                         ///< each centre's scale
    std::vector<centre_spec> centres{ centre_spec{} };
    /// G: iron ore (extraction), steel and planks (works), 100 per scale point.
    std::vector<basket_row> basket{ { k_raw, 100.0f, false }, { k_mill1, 100.0f, true },
                                    { k_mill2, 100.0f, true } };
    charter_cap_rule rule = charter_cap_rule::sqrt_capital;
    std::int32_t c = 2;                    ///< per_resource_firm_cap
    std::int32_t ceiling = 120;            ///< density_ceiling (sqrt only)
    float works_rate = 0.0f;               ///< processing base rate: 0 = works make nothing
    bool upkeep = false;                   ///< every works draws power (not in G)
    bool yard = false;                     ///< a construction recipe, and capacity wanted per building
    float yard_seed = 1.0f;                ///< capacity wanted per standing building
    float yard_output = 1.0f;              ///< the yard recipe's capacity per batch
    bool one_province = false;             ///< every tile in province 1 (cap 2 firms)
};

struct reading
{
    charter_spend_report rep;
    std::vector<entity_id> centres;                   ///< as the config lists them
    std::array<int, resource_count> firms{};          ///< per good, from the records
    std::array<int, resource_count> processing{};     ///< per good, firms with a processing focus
    std::array<long long, charter_unspent_reason_count> unspent{};
    bool balanced = false;
    std::size_t corporations = 0;                     ///< in the world after the spend
    const char* world_refusal = nullptr;              ///< charter_spend_world_refusal, asked first

    /// Firms for @p good chartered by the config's centre @p i.
    int firms_at(std::size_t i, std::size_t good) const
    {
        int n = 0;
        for (const charter_record& r : rep.charters)
            if (!r.specialist && r.good == good && r.centre == centres[i])
                ++n;
        return n;
    }
    long long u(charter_unspent_reason why) const { return unspent[static_cast<std::size_t>(why)]; }
    int body_firms() const { return rep.bodies.empty() ? 0 : static_cast<int>(rep.bodies.front().firms); }
};

/// The hand-built world @p cfg describes, its registry filled into @p reg; the
/// budget's centres land in @p points and @p centres (as the config lists them).
/// Deterministic: two builds of one config are the same world, id for id.
std::unique_ptr<world> build(const config& cfg, recipe_registry& reg,
                             std::map<entity_id, std::int32_t>& points,
                             std::vector<entity_id>& centres)
{
    auto w = std::make_unique<world>();

    const entity_id body = w->create_entity();
    {
        body_component bc{};
        bc.name        = "FixtureBody";
        bc.grid_width  = cfg.body_w;
        bc.grid_height = cfg.body_h;
        w->bodies[body] = bc;
    }
    const entity_id nation = w->create_entity();
    nation_component nc{};
    nc.name = "Veyl";
    std::map<std::pair<int, int>, entity_id> at;
    for (int y = 0; y < cfg.body_h; ++y)
        for (int x = 0; x < cfg.body_w; ++x)
        {
            const entity_id tid = w->create_entity();
            tile_component tc{};
            tc.body   = body;
            tc.grid_x = x;
            tc.grid_y = y;
            tc.substrate = terrain_substrate::barren;
            int dx = std::abs(x - k_cx);
            if (dx > cfg.body_w / 2)
                dx = cfg.body_w - dx;
            const int dy = y - k_cy;
            switch (cfg.g)
            {
            case ground::everywhere:     tc.resource_deposit[k_raw] = 1.0f; break;
            case ground::outside_window: if (dx * dx + dy * dy > 36) tc.resource_deposit[k_raw] = 1.0f; break;
            case ground::farmland:
                tc.resource_deposit[static_cast<std::size_t>(resource_type::agricultural_produce)] = 1.0f;
                break;
            case ground::barren: break;
            }
            w->tiles[tid] = tc;
            nc.tiles.push_back(tid);   // raster order: the nation's stored order
            w->tile_to_nation[tid] = nation;
            if (cfg.one_province)
                w->provinces.tile_province[tid] = 1u;
            at[{ x, y }] = tid;
        }
    w->nations[nation] = nc;

    for (const centre_spec& cs : cfg.centres)
    {
        const entity_id id = w->create_entity();
        population_centre_component pc{};
        pc.scale = cfg.scale;
        w->population_centres[id]     = pc;
        w->population_centre_tile[id] = at.at({ cs.x, cs.y });
        points[id] = cs.points;
        centres.push_back(id);
    }

    // G is the basket: the households want every good in it. A good with a
    // recipe is chartered as a works (processing firm); one without, as a mine.
    population_demand_params pd;
    for (const basket_row& b : cfg.basket)
        pd.demand_basket[b.good] = b.demand;
    reg.set_population_demand(pd);
    for (const basket_row& b : cfg.basket)
        if (b.works)
        {
            recipe rc;
            rc.name = std::string("fixture_works_") + std::to_string(b.good);
            rc.inputs[static_cast<std::size_t>(resource_type::timber)] = 1.0f;
            rc.outputs[b.good] = 1.0f;
            reg.add_recipe(rc);
        }
    if (cfg.works_rate > 0.0f)
    {
        // Every works firm opens with two processing facilities (processing_mix),
        // each at 0.5 workforce: 2 x 0.5 x rate x 1 output a firm.
        building_economics e;
        e.base_rate = cfg.works_rate;
        reg.set_economics(building_type::processing_facility, e);
    }
    if (cfg.yard)
    {
        // A yard makes construction capacity, and every standing building wants
        // some: none stands before the walk, so capacity is not in G, and the
        // yard step wants a yard from the walk's first firm on.
        recipe yard;
        yard.name = "fixture_yard";
        yard.inputs[static_cast<std::size_t>(resource_type::stone)] = 1.0f;
        yard.outputs[k_yard] = cfg.yard_output;
        reg.add_recipe(yard);
        construction_params cp;
        cp.seed_capacity_per_building = cfg.yard_seed;
        reg.set_construction(cp);
    }
    if (cfg.upkeep)
    {
        building_upkeep_params up;
        up.goods[static_cast<std::size_t>(building_type::processing_facility)]
                [static_cast<std::size_t>(era_band::any)][k_late] = 1.0f;
        reg.set_building_upkeep(up);
    }

    return w;
}

reading run(const config& cfg)
{
    recipe_registry reg;
    reading out;
    std::map<entity_id, std::int32_t> points;
    auto w = build(cfg, reg, points, out.centres);

    // Firm 1 point; a specialist no centre here can afford (1000 charters).
    charter_spend_params s;
    s.firm_price_points        = 1;
    s.specialist_firm_charters = 1000;
    s.window_radius            = cfg.radius;
    s.province_cap             = true;
    s.resource_cap_rule        = cfg.rule;
    s.per_resource_firm_cap    = cfg.c;
    s.max_firms_per_body       = 200;
    s.density_ceiling          = (cfg.rule == charter_cap_rule::sqrt_capital) ? cfg.ceiling : 0;

    const charter_budget budget(points);
    out.world_refusal = charter_spend_world_refusal(*w, reg, budget, s);
    charter_web_from_budget(*w, reg, budget, s, /*seed=*/1060u, /*settle=*/nullptr, &out.rep);
    out.corporations = w->corporations.size();
    for (const charter_record& r : out.rep.charters)
    {
        if (r.specialist || r.good >= resource_count)
            continue;
        ++out.firms[r.good];
        if (w->corporations.at(r.corp).focus == industrial_focus::processing)
            ++out.processing[r.good];
    }
    long long unspent = 0;
    for (const charter_unspent& u : out.rep.unspent)
    {
        out.unspent[static_cast<std::size_t>(u.reason)] += u.points;
        unspent += u.points;
    }
    out.balanced = out.rep.points_budgeted == budget.total()
                && out.rep.points_spent + unspent == budget.total()
                && out.rep.points_unspent == unspent;
    return out;
}

void print(const char* label, const reading& r)
{
    std::printf("  %s: firms iron_ore %d (processing %d), steel %d (processing %d), planks %d "
                "(processing %d), yard %d; body %d; unspent", label, r.firms[k_raw], r.processing[k_raw],
                r.firms[k_mill1], r.processing[k_mill1], r.firms[k_mill2], r.processing[k_mill2],
                r.firms[k_yard], r.body_firms());
    for (int i = 0; i < charter_unspent_reason_count; ++i)
        if (r.unspent[static_cast<std::size_t>(i)] != 0)
            std::printf(" %s %lld", charter_unspent_reason_name(static_cast<charter_unspent_reason>(i)),
                        r.unspent[static_cast<std::size_t>(i)]);
    if (!r.rep.bodies.empty() && r.rep.bodies.front().even_share > 0)
        std::printf("; even share %d (+1 on %d), yard places %d",
                    static_cast<int>(r.rep.bodies.front().even_share),
                    static_cast<int>(r.rep.bodies.front().even_share_extra),
                    static_cast<int>(r.rep.bodies.front().yard_places));
    std::printf("%s%s\n", r.balanced ? "" : " [UNBALANCED]", r.rep.refused ? " [REFUSED]" : "");
}

void print_ten(const reading& r)
{
    std::printf("      by good:");
    if (r.firms[k_raw] > 0 || r.rep.charters.empty())
        std::printf(" iron_ore %d", r.firms[k_raw]);
    for (const std::size_t g : k_ten)
        std::printf(" %s %d", resource_names::name_of(static_cast<resource_type>(g)).c_str(), r.firms[g]);
    std::printf(" | yard %d\n", r.firms[k_yard]);
}

/// The ten-good body (NR-906's check): 40 x 24 tiles of barren land, one
/// centre at (20, 12) of scale 1 with a window of radius 12 that holds every
/// firm it charters, works that make 1 of their good a firm, and a yard whose
/// one firm covers every building the walk can stand up (so exactly one yard
/// is wanted, and one is reserved).
config ten_goods(std::int32_t points, std::int32_t c)
{
    config cfg;
    cfg.g       = ground::barren;
    cfg.body_w  = 40;
    cfg.body_h  = 24;
    cfg.radius  = 12;
    cfg.scale   = 1;
    cfg.centres = { { 20, 12, points } };
    cfg.basket.clear();
    for (const std::size_t g : k_ten)
        cfg.basket.push_back({ g, 1000.0f, true });
    cfg.c           = c;
    cfg.works_rate  = 1.0f;
    cfg.yard        = true;
    cfg.yard_seed   = 0.01f;
    cfg.yard_output = 100.0f;
    return cfg;
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
    std::printf("\nthe turn — G = {iron_ore, steel, planks}; one centre, 8 points, c 2 (cap 2) unless named\n");
    using turn::ground;
    using why_t = charter_unspent_reason;
    {
        // CONTROL: deposits everywhere. Iron ore is FIRST in the turn and does
        // get an extraction firm, so the cases below are not vacuous: the good is
        // short, and placeable where the window holds a deposit.
        turn::config cfg;
        const turn::reading r = turn::run(cfg);
        turn::print("control, deposits everywhere", r);
        expect_true("control: iron ore holds 2 extraction firms",
                    r.firms[turn::k_raw] == 2 && r.processing[turn::k_raw] == 0);
        expect_true("control: steel and planks hold 2 processing firms each",
                    r.processing[turn::k_mill1] == 2 && r.processing[turn::k_mill2] == 2);
        expect_true("control: the 2 points left after G fills are no_gap",
                    r.u(why_t::no_gap) == 2 && r.balanced);
    }
    {
        // NR-903: the window holds no free deposit tile OF ANY KIND, so no
        // extraction firm can anchor in it — iron ore, first in the turn, is
        // skipped for this centre, and the processing goods after it still
        // charter. With a failed placement fatal, as before NR-903, this centre
        // chartered nothing at all.
        turn::config cfg;
        cfg.g = ground::outside_window;
        const turn::reading r = turn::run(cfg);
        turn::print("no deposit tile in the window", r);
        expect_true("NR-903: iron ore (first in the turn) gets no firm", r.firms[turn::k_raw] == 0);
        expect_true("NR-903: steel and planks still charter, 2 processing firms each",
                    r.processing[turn::k_mill1] == 2 && r.processing[turn::k_mill2] == 2);
        expect_true("NR-903: the centre stops only when no good can place; the rest "
                    "(4) is window_exhausted",
                    r.u(why_t::window_exhausted) == 4 && r.u(why_t::no_gap) == 0 && r.balanced);
    }
    {
        // THE LEGACY RULE ON THE SAME WORLD, unchanged: biggest gap first picks
        // iron ore (the gaps tie, the first wins), the placement fails, and the
        // centre stops there — every point window_exhausted.
        turn::config cfg;
        cfg.g    = ground::outside_window;
        cfg.rule = charter_cap_rule::fixed;
        const turn::reading r = turn::run(cfg);
        turn::print("legacy fixed rule, same world", r);
        expect_true("legacy: the first failed placement still ends the centre",
                    r.firms[turn::k_raw] + r.firms[turn::k_mill1] + r.firms[turn::k_mill2] == 0
                    && r.u(why_t::window_exhausted) == 8 && r.balanced);
    }
    {
        // THE YARD IS SKIPPED TOO. Farmland everywhere: an extraction anchor
        // takes a farm tile, a works cannot stand on one. From the first firm on
        // the body wants a yard; it cannot place, so it is skipped for this centre
        // and the turn goes on to the ore. With the yard's failure fatal the
        // centre would stop at its second firm with one mine.
        turn::config cfg;
        cfg.g    = ground::farmland;
        cfg.yard = true;
        const turn::reading r = turn::run(cfg);
        turn::print("farmland, a yard wanted", r);
        expect_true("yard: no yard and no works can stand on farmland",
                    r.firms[turn::k_yard] == 0 && r.firms[turn::k_mill1] == 0 && r.firms[turn::k_mill2] == 0);
        expect_true("yard: skipped, so iron ore still takes both its firms", r.firms[turn::k_raw] == 2);
        expect_true("yard: the rest (6) is window_exhausted", r.u(why_t::window_exhausted) == 6 && r.balanced);
    }
    {
        // THE STOP'S REASON: province_cap WINS. Every tile is one province (2
        // firms at most) and the window has no deposit tile: the ore fails on
        // the WINDOW, then two works fill the province and the next works fails
        // on the CAP. The centre stops with both reasons among its goods; the cap
        // took ground from one of them, so the rest is province_cap.
        turn::config cfg;
        cfg.g            = ground::outside_window;
        cfg.one_province = true;
        const turn::reading r = turn::run(cfg);
        turn::print("one province, no deposit tile", r);
        expect_true("precedence: two works, then the province is full",
                    r.firms[turn::k_mill1] == 1 && r.firms[turn::k_mill2] == 1 && r.firms[turn::k_raw] == 0);
        expect_true("precedence: the rest (6) is province_cap, not window_exhausted",
                    r.u(why_t::province_cap) == 6 && r.u(why_t::window_exhausted) == 0 && r.balanced);
    }
    {
        // TWO CENTRES: the first (6 points, at (6,6)) has no deposit tile and
        // skips the ore; the second (4 points, at (18,6)) stands on ore ground
        // and serves it. A skip is for THAT centre only.
        turn::config cfg;
        cfg.g       = ground::outside_window;
        cfg.centres = { { 6, 6, 6 }, { 18, 6, 4 } };
        const turn::reading r = turn::run(cfg);
        turn::print("two centres, the second on ore", r);
        expect_true("two centres: the first charters the works, not the ore",
                    r.firms_at(0, turn::k_raw) == 0 && r.firms_at(0, turn::k_mill1) == 2
                    && r.firms_at(0, turn::k_mill2) == 2);
        expect_true("two centres: the second serves the ore the first skipped",
                    r.firms_at(1, turn::k_raw) == 2);
        expect_true("two centres: window_exhausted 2 (first), no_gap 2 (second)",
                    r.u(why_t::window_exhausted) == 2 && r.u(why_t::no_gap) == 2 && r.balanced);
    }
    {
        // NR-905: THE CEILING BINDS. c 4, ceiling 6; 15 firm charters over 3
        // goods whose caps sum to 12, so each good holds at most 2. The first
        // centre (10 points) has no quarry: it charters its works up to their
        // share and waits. The second (5 points) is on ore ground and takes the
        // ore's share. Without the share the first centre filled the ceiling with
        // works and the ore got nothing.
        turn::config cfg;
        cfg.g       = ground::outside_window;
        cfg.c       = 4;
        cfg.ceiling = 6;
        cfg.centres = { { 6, 6, 10 }, { 18, 6, 5 } };
        const turn::reading r = turn::run(cfg);
        turn::print("ceiling 6 binds, the second centre on ore", r);
        expect_true("NR-905: the even share is 2",
                    !r.rep.bodies.empty() && r.rep.bodies.front().even_share == 2
                    && r.rep.bodies.front().even_share_extra == 0);
        expect_true("NR-905: the works stop at their share (2 each) at the first centre",
                    r.firms_at(0, turn::k_mill1) == 2 && r.firms_at(0, turn::k_mill2) == 2);
        expect_true("NR-905: the ore takes its share at the second centre", r.firms_at(1, turn::k_raw) == 2);
        expect_true("NR-905: first centre's 6 window_exhausted, second's 3 density_ceiling; no gap",
                    r.u(why_t::window_exhausted) == 6 && r.u(why_t::density_ceiling) == 3
                    && r.u(why_t::share_unplaced) == 0 && r.balanced);
    }
    {
        // NR-905, NO QUARRY ANYWHERE: the second centre (at (8,6)) has no deposit
        // tile either. The ore's share (2) is never placed and the body ends 2
        // below its ceiling; those 2 firms' points are the gap, share_unplaced,
        // drawn from the first centre (spend order). The rest stays as booked.
        turn::config cfg;
        cfg.g       = ground::outside_window;
        cfg.c       = 4;
        cfg.ceiling = 6;
        cfg.centres = { { 6, 6, 10 }, { 8, 6, 5 } };
        const turn::reading r = turn::run(cfg);
        turn::print("ceiling 6 binds, no quarry anywhere", r);
        expect_true("NR-905 gap: no ore, the works at their share",
                    r.firms[turn::k_raw] == 0 && r.firms[turn::k_mill1] == 2 && r.firms[turn::k_mill2] == 2);
        expect_true("NR-905 gap: share_unplaced 2 (the ore's share), window_exhausted 9",
                    r.u(why_t::share_unplaced) == 2 && r.u(why_t::window_exhausted) == 9
                    && r.u(why_t::no_gap) == 0 && r.u(why_t::density_ceiling) == 0 && r.balanced);
    }
    // --- NR-906: THE SHARE IS A RESERVATION, on the ten-good body ---
    std::printf("\nthe even share as a reservation (NR-906) — ten works goods, ceiling 120, one yard\n");
    {
        // THE RELEASE AND THE YARD. c 15 at B = B_ref = 150: cap 15; 150 charters
        // over caps summing to 150 outrun the ceiling, so it binds. One yard is
        // wanted, and its place comes off the ceiling first: (120 - 1) / 10 = 11,
        // the first nine goods 12. Steel's households want only 6, which six
        // works make: steel stops being short at 6 and RELEASES the rest of its
        // share, so the ceiling still fills — 120, not the 114 a capping share
        // leaves — and nothing is booked no_gap while goods are short and under
        // their cap.
        turn::config cfg = turn::ten_goods(150, 15);
        cfg.basket.front().demand = 6.0f;   // steel: covered by its sixth works
        const turn::reading r = turn::run(cfg);
        turn::print("release: steel wants 6", r);
        turn::print_ten(r);
        const charter_body_record* b = r.rep.bodies.empty() ? nullptr : &r.rep.bodies.front();
        expect_true("yard: one yard reserved, one provisioned",
                    b != nullptr && b->yard_places == 1 && r.firms[turn::k_yard] == 1);
        expect_true("yard: its place comes off the ceiling before the cut, 119 / 10 = 11 (+1 on 9)",
                    b != nullptr && b->even_share == 11 && b->even_share_extra == 9);
        expect_true("release: steel stops at 6", r.firms[turn::k_ten[0]] == 6);
        expect_true("release: the ceiling still fills to 120", r.body_firms() == 120);
        expect_true("release: no no_gap while goods are short and under cap; the rest (30) is "
                    "density_ceiling",
                    r.u(why_t::no_gap) == 0 && r.u(why_t::density_ceiling) == 30 && r.balanced);
    }
    {
        // THE THRESHOLD. Iron ore joins the nine works as the first good, and the
        // body has no deposit anywhere: iron ore is placeable nowhere. c 13 at
        // B = B_ref = 130: cap 13. Without shares (a ceiling of 150, which 130
        // charters cannot reach) the body reaches 9 x 13 + 1 yard = 118. With the
        // ceiling at 120 the ore's reservation (12) holds to the end, so the body
        // reaches 120 - 12 = 108 — no less than 118 less that reservation — and
        // the 12 unfilled firms are share_unplaced.
        turn::config cfg = turn::ten_goods(130, 13);
        cfg.basket.back() = { turn::k_raw, 1000.0f, false };   // tools out, iron ore (a mine) in
        turn::config open = cfg;
        open.ceiling = 150;
        const turn::reading r0 = turn::run(open);
        turn::print("threshold, ceiling 150 (no shares)", r0);
        const turn::reading r = turn::run(cfg);
        turn::print("threshold, ceiling 120", r);
        turn::print_ten(r);
        const charter_body_record* b = r.rep.bodies.empty() ? nullptr : &r.rep.bodies.front();
        const int reserve = (b != nullptr) ? static_cast<int>(b->even_share) + (b->even_share_extra > 0 ? 1 : 0) : -1;
        expect_true("threshold: without shares the body reaches 118, and the ore gets nothing",
                    r0.body_firms() == 118 && r0.firms[turn::k_raw] == 0
                    && (r0.rep.bodies.empty() || r0.rep.bodies.front().even_share == 0));
        expect_true("threshold: the ore's reservation is 12 (first in the turn)", reserve == 12);
        expect_true("threshold: with shares the body reaches exactly 120 - 12 = 108",
                    r.body_firms() == 120 - reserve);
        expect_true("threshold: never below 118 less the ore's reservation",
                    r.body_firms() >= r0.body_firms() - reserve);
        expect_true("threshold: the ore's 12 unfilled firms are share_unplaced",
                    r.u(why_t::share_unplaced) == 12 && r.u(why_t::window_exhausted) == 10 && r.balanced);
    }
    {
        // THE REFUSAL. Ten goods and one yard place need a ceiling of at least 11;
        // at 10 the spend is refused before anything is chartered — the world
        // gains no corporation and every point is booked `refused` — and at 11 it
        // goes ahead with a share of one each.
        turn::config cfg = turn::ten_goods(150, 15);
        cfg.ceiling = 10;
        const turn::reading r = turn::run(cfg);
        turn::print("ceiling 10 < 10 goods + 1 yard", r);
        expect_true("refusal: charter_spend_world_refusal names the ceiling",
                    r.world_refusal != nullptr && std::strstr(r.world_refusal, "turn goods plus") != nullptr);
        expect_true("refusal: the spend is refused and mutates nothing",
                    r.rep.refused && r.corporations == 0 && r.rep.charters.empty()
                    && r.u(why_t::refused) == 150 && r.balanced);
        cfg.ceiling = 11;
        const turn::reading r11 = turn::run(cfg);
        turn::print("ceiling 11 = 10 goods + 1 yard", r11);
        expect_true("refusal: at 11 the spend goes ahead, a share of 1 each",
                    r11.world_refusal == nullptr && !r11.rep.refused && !r11.rep.bodies.empty()
                    && r11.rep.bodies.front().even_share == 1 && r11.body_firms() == 11);
    }

    {
        // BL-1060 round 4 (the cold review's findings 1 and 4). THE THREE YARD
        // TESTS DISAGREE HERE, so only the right one can pass: at a seed rate
        // of 1.0 per building and a yard making 1.0 capacity a batch, the WANT
        // at the walk's most-built extreme is ~480 yards, the per-good CAP is
        // 15, and the COVER is larger still. The places the shares are cut from
        // are the smallest of the three, 15 — and the refusal must read THAT,
        // not the want: refusing on the want turned this ordinary world down
        // (120 < 10 + 480) and the search fell back to the legacy world in
        // silence. A yard bound that read any one of the other two tests, or a
        // refusal that read the want, fails here.
        turn::config cfg = turn::ten_goods(150, 15);
        cfg.yard_seed   = 1.0f;
        cfg.yard_output = 1.0f;
        const turn::reading r = turn::run(cfg);
        turn::print("yard tests disagree: want ~480, cap 15, cover larger", r);
        const charter_body_record* b = r.rep.bodies.empty() ? nullptr : &r.rep.bodies.front();
        expect_true("yard bound: the world is NOT refused (the want is not the refusal's reading)",
                    r.world_refusal == nullptr && !r.rep.refused);
        expect_true("yard bound: the places are the per-good cap, the smallest of the three tests",
                    b != nullptr && b->yard_places == 15);
        expect_true("yard bound: the shares are cut from 120 - 15 = 105, so 10 each (+1 on 5)",
                    b != nullptr && b->even_share == 10 && b->even_share_extra == 5);
        expect_true("yard bound: the walk never provisions more yards than the places reserved",
                    b != nullptr && r.firms[turn::k_yard] <= b->yard_places && r.balanced);
    }

    {
        // BL-1060 (4): every works the walk charters draws power, which no base
        // installation did, so power is short but NOT in G, and the turn never
        // serves it. Once G fills, the rest is that shortfall — late_shortfall.
        turn::config cfg;
        cfg.upkeep = true;
        const turn::reading r = turn::run(cfg);
        turn::print("works draw power (not in G)", r);
        const bool power_outside_g =
            !r.rep.bodies.empty()
            && std::find(r.rep.bodies.front().goods.begin(), r.rep.bodies.front().goods.end(),
                         static_cast<std::uint16_t>(turn::k_late)) == r.rep.bodies.front().goods.end();
        expect_true("late: power is not in G", power_outside_g);
        expect_true("late: the 2 points left are late_shortfall, not no_gap",
                    r.u(why_t::late_shortfall) == 2 && r.u(why_t::no_gap) == 0 && r.balanced);
    }

    // --- 4. THE NO-SPECIALIST WORLD (BL-1044, NR-910) --------------------------
    std::printf("\ncharter_refusal_probe — the no-specialist world falls back (NR-910)\n");
    {
        // Two centres, 8 and 5 points; firm price 1 point, so a specialist of m
        // charters costs m points. The default ground and basket (the turn's).
        turn::config cfg;
        cfg.centres = { turn::centre_spec{ turn::k_cx, turn::k_cy, 8 },
                        turn::centre_spec{ turn::k_cx + 3, turn::k_cy, 5 } };
        const auto spend_at = [&](std::int32_t m) {
            charter_spend_params s;
            s.firm_price_points        = 1;
            s.specialist_firm_charters = m;
            s.window_radius            = cfg.radius;
            s.province_cap             = true;
            s.resource_cap_rule        = charter_cap_rule::sqrt_capital;
            s.per_resource_firm_cap    = cfg.c;
            s.max_firms_per_body       = 200;
            s.density_ceiling          = cfg.ceiling;
            return s;
        };

        // (a) the predicate: the walk's own test, on the walk's own resolution.
        {
            recipe_registry reg;
            std::map<entity_id, std::int32_t> points;
            std::vector<entity_id> centres;
            auto w = turn::build(cfg, reg, points, centres);
            const charter_budget budget(points);
            expect_true("affords: m 9 — neither centre (8, 5) affords a specialist",
                        !charter_budget_affords_specialist(*w, budget, spend_at(9)));
            expect_true("affords: m 8 — the 8-point centre affords one (points == price)",
                        charter_budget_affords_specialist(*w, budget, spend_at(8)));
            expect_true("affords: an empty budget affords nothing",
                        !charter_budget_affords_specialist(*w, charter_budget{}, spend_at(1)));
            // The walk charters nothing where no nation owns the centre's tile,
            // so the predicate must not count such a centre.
            w->tile_to_nation.erase(w->population_centre_tile.at(centres[0]));
            expect_true("affords: m 8 — the only affording centre on a tile no nation owns does not count",
                        !charter_budget_affords_specialist(*w, budget, spend_at(8)));
            expect_true("affords: m 5 — the other centre, on a nation's tile, still does",
                        charter_budget_affords_specialist(*w, budget, spend_at(5)));
        }

        // (b) the apply: a budget no centre can buy a specialist with lays the
        // no-budget world, byte for byte, and reports the fallback.
        {
            recipe_registry reg_a, reg_b;
            std::map<entity_id, std::int32_t> points_a, points_b;
            std::vector<entity_id> centres_a, centres_b;
            auto wa = turn::build(cfg, reg_a, points_a, centres_a);
            auto wb = turn::build(cfg, reg_b, points_b, centres_b);
            const charter_budget budget(points_a);
            landscape_candidate cand;
            cand.placement_seed = 1060u;
            // The budget overload with no specialist affordable (m 9) ...
            charter_spend_report rep;
            rep.points_spent = -1;   // proves the report was written
            apply_landscape_candidate(*wa, reg_a, cand, /*regenerate_specialists=*/false,
                                      &budget, spend_at(9), &rep);
            // ... against the legacy overload, the world with no budget at all.
            apply_landscape_candidate(*wb, reg_b, cand, /*regenerate_specialists=*/false);
            const std::uint64_t da = world_state_digest(*wa);
            const std::uint64_t db = world_state_digest(*wb);
            std::printf("    fallback world %016llx, no-budget world %016llx; corporations %zu / %zu\n",
                        static_cast<unsigned long long>(da), static_cast<unsigned long long>(db),
                        wa->corporations.size(), wb->corporations.size());
            expect_true("fallback: the world is the no-budget world, byte for byte (snapshot digest)",
                        da == db && wa->corporations.size() == wb->corporations.size());
            long long by_reason = 0, other_reasons = 0;
            for (const charter_unspent& u : rep.unspent)
                (u.reason == charter_unspent_reason::no_specialist ? by_reason : other_reasons) += u.points;
            expect_true("fallback: the report says it FELL BACK, is not a refusal, and seats nobody",
                        rep.fell_back && !rep.refused && rep.no_specialists
                        && rep.player == null_entity && rep.charters.empty());
            expect_true("fallback: nothing spent; every point unspent as no_specialist (13 = 8 + 5)",
                        rep.points_spent == 0 && rep.points_budgeted == 13 && rep.points_unspent == 13
                        && by_reason == 13 && other_reasons == 0);

            // And a budget that DOES afford one (m 8) is a budget world: the
            // walk runs, and the 8-point centre charters its specialist.
            recipe_registry reg_c;
            std::map<entity_id, std::int32_t> points_c;
            std::vector<entity_id> centres_c;
            auto wc = turn::build(cfg, reg_c, points_c, centres_c);
            charter_spend_report rep_c;
            apply_landscape_candidate(*wc, reg_c, cand, /*regenerate_specialists=*/false,
                                      &budget, spend_at(8), &rep_c);
            expect_true("no fallback at m 8: the walk ran and chartered the affordable specialist",
                        !rep_c.fell_back && !rep_c.refused && rep_c.specialists.size() == 1
                        && rep_c.player == rep_c.specialists.front());
        }
    }

    std::printf("\n%s (%d failing)\n", g_fail == 0 ? "ALL PASS" : "FAILED", g_fail);
    return g_fail == 0 ? 0 : 1;
}
