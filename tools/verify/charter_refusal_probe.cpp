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
#include "world/input_reach.hpp"     // BL-1233: reachable_supply, the sized rule's rows
#include "world/market_clearing.hpp" // market_for_tile
#include "world/logistics.hpp"       // invalidate_logistics_caches
#include "world/landscape_search.hpp"  // apply_landscape_candidate's budget overload (NR-910)
#include "world/placement_rules.hpp"   // is_wharf_site, the produce ladder's rows (BL-1208)
#include "world/province.hpp"          // province_anchors, the fixture's province (BL-1146)
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
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
    bool one_province = false;             ///< every tile in province 1 (its cap: 2 x the centre's rung, BL-1146)
    /// BL-1185 (chain-feasible placement): the works and the yard need NO input
    /// by default. The fixture has no market and no producer of anything, so a
    /// works that needs timber (the fixture's recipe until 2026-10-05) is
    /// chain-infeasible and is never chartered: every turn/share/cap case below
    /// would read the chain rule instead of the rule it was written for. Set it
    /// to read the chain rule on purpose (`chain_infeasible`).
    bool works_input = false;
    /// BL-1197 review: households also want TOOLS, whose one recipe draws
    /// platinum group metals — a raw with no deposit on this body, so tools
    /// cannot be produced here and leave G (`unproducible`).
    bool offworld_good = false;
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
            // BL-1197 round 5 (G holds only goods the body can produce): the
            // ore, and the timber a works_input recipe needs, each stand on ONE
            // tile at the antipode column of (6, 6), row 0 — outside every
            // window these cases open, so the goods stay in G (the body CAN make
            // them) while no centre has ground for them, which is what these
            // cases measure.
            if (x == (k_cx + cfg.body_w / 2) % cfg.body_w && y == 0)
            {
                tc.resource_deposit[k_raw] = 1.0f;
                if (cfg.works_input)
                    tc.resource_deposit[static_cast<std::size_t>(resource_type::timber)] = 1.0f;
            }
            w->tiles[tid] = tc;
            nc.tiles.push_back(tid);   // raster order: the nation's stored order
            w->tile_to_nation[tid] = nation;
            if (cfg.one_province)
                w->provinces.tile_province[tid] = 1u;
            at[{ x, y }] = tid;
        }
    w->nations[nation] = nc;
    // The province ITSELF, not only the tile index (BL-1146 review): the cap
    // reads its centre's rung from `province_anchors`, which walks the province
    // list's tiles, as `seed_province_holders` does. Tiles ascend by id (the
    // partition's contract) because they were created in this order.
    if (cfg.one_province)
    {
        province pr;
        pr.id    = 1u;
        pr.body  = body;
        pr.tiles = nc.tiles;
        w->provinces.provinces.push_back(std::move(pr));
    }

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
            if (cfg.works_input)
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
        if (cfg.works_input)
            yard.inputs[static_cast<std::size_t>(resource_type::stone)] = 1.0f;
        yard.outputs[k_yard] = cfg.yard_output;
        reg.add_recipe(yard);
        construction_params cp;
        cp.seed_capacity_per_building = cfg.yard_seed;
        reg.set_construction(cp);
    }
    if (cfg.offworld_good)
    {
        pd.demand_basket[static_cast<std::size_t>(resource_type::tools)] = 100.0f;
        reg.set_population_demand(pd);
        recipe rc;
        rc.name = "fixture_tools_from_pgm";
        rc.inputs[static_cast<std::size_t>(resource_type::platinum_group_metals)] = 1.0f;
        rc.outputs[static_cast<std::size_t>(resource_type::tools)] = 1.0f;
        reg.add_recipe(rc);
    }
    if (cfg.upkeep)
    {
        // BL-1197 review: power must be PRODUCIBLE here for its shortfall to be
        // the walk's own (`late_shortfall`) rather than `unproducible` — a
        // recipe with no inputs makes it. Nothing wants power before the walk,
        // so it is still not in G.
        recipe pw;
        pw.name = "fixture_power";
        pw.outputs[k_late] = 1.0f;
        reg.add_recipe(pw);
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

// ---------------------------------------------------------------------------
// 6. THE CHAIN RULE (BL-1185, chain-feasible placement; the cold review's rows)
// ---------------------------------------------------------------------------
// A hand-built body split into TWO markets: A (centre (4, 6)) holds every tile
// with x <= 9, B (centre (14, 6)) every tile with x >= 10. A works that makes
// steel needs timber, and the one timber producer — an extraction site standing
// on (14, 6) — is in B unless @p timber_in_a puts it at (2, 6). The centre sits
// at (6, 6) with a window of radius 2, and every tile of market A except (6, 6)
// is already built on, so the works firm's ANCHOR can only be (6, 6) (market A)
// and its second processor lands on the nearest free tile, (10, 6) — market B.
// Timber is priced so B's timber cannot reach A under the dispatcher's
// shortage gate (R x base_A - haul > 1.05 x base_B fails), so the anchor's
// market has no reachable producer.
namespace chainfx {

struct result
{
    charter_spend_report rep;
    std::size_t firms = 0;
    bool holds_in_b   = false; ///< some chartered firm holds a building with x >= 10
    bool anchor_at_x  = false; ///< the chartered firm's anchor is (6, 6)
    long long chain_infeasible = 0;
    bool balanced = false;
    bool input_mine   = false; ///< BL-1197: a firm's anchor is a timber site at (5, 6)
    std::size_t steel_firms = 0; ///< firms whose holdings include a steel-making processor
};

/// BL-1233 review round 3: @p spare_richness is that deposit's richness — the
/// derived-demand row makes it rich enough to feed the standing works AND the
/// steel firm it charters for, which the sized rule requires (one ordinary site
/// is wholly drawn by the works).
/// BL-1197 round 4 (derived demand): @p spare_deposit puts an UNBUILT timber
/// deposit at (5, 6), inside the centre's window in market A; @p points firm
/// charters to spend; @p standing_works stands a steel-from-timber works at
/// (7, 6) in market A before the walk (no corporation holds it); @p no_producer
/// removes the timber producer, so nothing on the body makes timber.
result run(bool timber_in_a, bool spare_deposit = false, std::int32_t points = 1,
           bool standing_works = false, bool no_producer = false, float spare_richness = 1.0f)
{
    auto w = std::make_unique<world>();
    recipe_registry reg;
    const int bw = 24, bh = 12;
    const entity_id body = w->create_entity();
    {
        body_component bc{};
        bc.name = "ChainBody";
        bc.grid_width = bw;
        bc.grid_height = bh;
        w->bodies[body] = bc;
    }
    const entity_id nation = w->create_entity();
    nation_component nc{};
    nc.name = "Veyl";
    std::map<std::pair<int, int>, entity_id> at;
    const std::size_t timber = static_cast<std::size_t>(resource_type::timber);
    const int tx = timber_in_a ? 2 : 14;
    for (int y = 0; y < bh; ++y)
        for (int x = 0; x < bw; ++x)
        {
            const entity_id tid = w->create_entity();
            tile_component tc{};
            tc.body = body;
            tc.grid_x = x;
            tc.grid_y = y;
            tc.substrate = terrain_substrate::barren;
            if ((x == tx && y == 6) || (spare_deposit && x == 5 && y == 6))
            {
                tc.resource_deposit[timber]   = (spare_deposit && x == 5 && y == 6) ? spare_richness : 1.0f;
                tc.resource_remaining[timber] = 1000.0f;   // an unspent reserve: a producer
            }
            w->tiles[tid] = tc;
            nc.tiles.push_back(tid);
            w->tile_to_nation[tid] = nation;
            at[{ x, y }] = tid;
        }
    w->nations[nation] = nc;

    // The two markets. Timber is CHEAP in A (base 0.2) and 1.0 in B, so B's
    // timber never reaches A: the shortage gate R x 0.2 - haul > 1.05 x 1.0
    // fails at any R <= 5 (the default registry's R is ceil_mult, 4).
    for (const int cx : { 4, 14 })
    {
        const entity_id mid = w->create_entity();
        market_component m{};
        m.body = body;
        m.centre_tile = at.at({ cx, 6 });
        m.base_price.fill(1.0f);
        if (cx == 4)
            m.base_price[timber] = 0.2f;
        m.price = m.base_price;
        w->markets[mid] = m;
    }

    // The timber producer, and filler on every market-A tile but (6, 6).
    if (!no_producer)
    {
        const entity_id bid = w->create_entity();
        building_component b{};
        b.tile = at.at({ tx, 6 });
        b.type = building_type::extraction_site;
        b.target_resource = resource_type::timber;
        b.workforce_assigned = 0.5f;
        w->buildings[bid] = b;
        w->stockpiles[bid] = stockpile_component{};
    }
    for (int y = 0; y < bh; ++y)
        for (int x = 0; x <= 9; ++x)
        {
            if ((x == 6 && y == 6) || (x == tx && y == 6) || (spare_deposit && x == 5 && y == 6)
                || (standing_works && x == 7 && y == 6))
                continue;
            const entity_id bid = w->create_entity();
            building_component b{};
            b.tile = at.at({ x, y });
            b.type = building_type::military_base;
            w->buildings[bid] = b;
        }

    const entity_id centre = w->create_entity();
    population_centre_component pc{};
    pc.scale = 5;
    w->population_centres[centre] = pc;
    w->population_centre_tile[centre] = at.at({ 6, 6 });

    population_demand_params pd;
    pd.demand_basket[static_cast<std::size_t>(resource_type::steel)] = 100.0f;
    reg.set_population_demand(pd);
    recipe rc;
    rc.name = "fixture_steel_from_timber";
    rc.inputs[timber] = 1.0f;
    rc.outputs[static_cast<std::size_t>(resource_type::steel)] = 1.0f;
    reg.add_recipe(rc);
    if (standing_works)
    {
        // A works runs at a nominal rate, so it has an input draw to want.
        building_economics proc;
        proc.base_rate = 1.0f;
        reg.set_economics(building_type::processing_facility, proc);
        const entity_id bid = w->create_entity();
        building_component b{};
        b.tile = at.at({ 7, 6 });
        b.type = building_type::processing_facility;
        b.recipe = reg.recipe_id(rc.name);
        b.workforce_assigned = 0.5f;
        w->buildings[bid] = b;
        w->stockpiles[bid] = stockpile_component{};
    }
    building_economics ext;
    ext.base_rate = 1.0f;
    reg.set_economics(building_type::extraction_site, ext);

    charter_spend_params s;
    s.firm_price_points        = 1;
    s.specialist_firm_charters = 1000;
    s.window_radius            = 2;
    s.province_cap             = false;
    s.resource_cap_rule        = charter_cap_rule::sqrt_capital;
    s.per_resource_firm_cap    = 2;
    s.max_firms_per_body       = 200;
    s.density_ceiling          = 120;
    const charter_budget budget(std::map<entity_id, std::int32_t>{ { centre, points } });

    result out;
    charter_web_from_budget(*w, reg, budget, s, /*seed=*/1185u, /*settle=*/nullptr, &out.rep);
    out.firms = out.rep.firms.size();
    for (const entity_id cid : out.rep.firms)
    {
        const corporation_component& corp = w->corporations.at(cid);
        for (const entity_id bid : corp.assets)
            if (w->tiles.at(w->buildings.at(bid).tile).grid_x >= 10)
                out.holds_in_b = true;
        if (!corp.assets.empty() && w->buildings.at(corp.assets.front()).tile == at.at({ 6, 6 }))
            out.anchor_at_x = true;
        if (!corp.assets.empty())
        {
            const building_component& a = w->buildings.at(corp.assets.front());
            if (a.tile == at.at({ 5, 6 }) && a.type == building_type::extraction_site
                && a.target_resource == resource_type::timber)
                out.input_mine = true;
        }
        for (const entity_id bid : corp.assets)
        {
            const building_component& b = w->buildings.at(bid);
            if (b.type == building_type::processing_facility && b.recipe != no_recipe
                && reg.get_recipe(b.recipe) != nullptr
                && reg.get_recipe(b.recipe)->outputs[static_cast<std::size_t>(resource_type::steel)] > 0.0f)
            {
                ++out.steel_firms;
                break;
            }
        }
    }
    long long unspent = 0;
    for (const charter_unspent& u : out.rep.unspent)
    {
        unspent += u.points;
        if (u.reason == charter_unspent_reason::chain_infeasible)
            out.chain_infeasible += u.points;
    }
    out.balanced = out.rep.points_spent + unspent == budget.total();
    return out;
}

/// THE KEPT ROSTER (`enforce_chain_feasible_roster`, the second cold review):
/// the same two-market body, timber only in B, steel cheap in A so B's steel
/// cannot reach A either. Specialist S (processing focus, seated) holds
///   P1 at (6, 6) in A: steel from timber — infeasible (no timber in A);
///   P3 at (7, 6) in A: tools from steel — feasible ONLY against P1's stale
///      default recipe, so an order-dependent pass would keep it;
///   P2 at (12, 6) in B: steel from timber — feasible (timber beside it).
/// Specialist T (extraction focus) holds the timber mine. With @p s_loses_all,
/// S holds only P1 and P3, so it is left with no processor and the seat moves.
struct roster_result
{
    chain_roster_enforcement first, second;
    std::uint64_t digest_first = 0, digest_second = 0;
    chain_feasibility_audit audit;
    bool p1_gone = false, p3_gone = false, p2_kept = false;
    entity_id s = null_entity, t = null_entity;
};

roster_result run_roster(bool s_loses_all)
{
    auto w = std::make_unique<world>();
    recipe_registry reg;
    const int bw = 24, bh = 12;
    const entity_id body = w->create_entity();
    {
        body_component bc{};
        bc.name = "RosterBody";
        bc.grid_width = bw;
        bc.grid_height = bh;
        w->bodies[body] = bc;
    }
    const entity_id nation = w->create_entity();
    nation_component nc{};
    nc.name = "Veyl";
    std::map<std::pair<int, int>, entity_id> at;
    const std::size_t timber = static_cast<std::size_t>(resource_type::timber);
    const std::size_t steel  = static_cast<std::size_t>(resource_type::steel);
    const std::size_t tools  = static_cast<std::size_t>(resource_type::tools);
    for (int y = 0; y < bh; ++y)
        for (int x = 0; x < bw; ++x)
        {
            const entity_id tid = w->create_entity();
            tile_component tc{};
            tc.body = body;
            tc.grid_x = x;
            tc.grid_y = y;
            tc.substrate = terrain_substrate::barren;
            if (x == 14 && y == 6)
            {
                tc.resource_deposit[timber]   = 1.0f;
                tc.resource_remaining[timber] = 1000.0f;
            }
            w->tiles[tid] = tc;
            nc.tiles.push_back(tid);
            w->tile_to_nation[tid] = nation;
            at[{ x, y }] = tid;
        }
    w->nations[nation] = nc;
    for (const int cx : { 4, 14 })
    {
        const entity_id mid = w->create_entity();
        market_component m{};
        m.body = body;
        m.centre_tile = at.at({ cx, 6 });
        m.base_price.fill(1.0f);
        if (cx == 4)
        {
            m.base_price[timber] = 0.2f;   // B's timber cannot reach A
            m.base_price[steel]  = 0.2f;   // nor B's steel
        }
        m.price = m.base_price;
        w->markets[mid] = m;
    }
    recipe st;
    st.name = "fixture_steel_from_timber";
    st.inputs[timber] = 1.0f;
    st.outputs[steel] = 1.0f;
    reg.add_recipe(st);
    recipe tl;
    tl.name = "fixture_tools_from_steel";
    tl.inputs[steel] = 1.0f;
    tl.outputs[tools] = 1.0f;
    reg.add_recipe(tl);
    building_economics e;
    e.base_rate = 1.0f;
    reg.set_economics(building_type::extraction_site, e);
    reg.set_economics(building_type::processing_facility, e);

    const auto add = [&](int x, int y, building_type t, const char* rname) {
        const entity_id bid = w->create_entity();
        building_component b{};
        b.tile = at.at({ x, y });
        b.type = t;
        b.workforce_assigned = 0.5f;
        if (t == building_type::extraction_site)
            b.target_resource = resource_type::timber;
        if (rname != nullptr)
            b.recipe = reg.recipe_id(rname);
        w->buildings[bid] = b;
        w->stockpiles[bid] = stockpile_component{};
        return bid;
    };
    const entity_id p1 = add(6, 6, building_type::processing_facility, "fixture_steel_from_timber");
    const entity_id p3 = add(7, 6, building_type::processing_facility, "fixture_tools_from_steel");
    const entity_id p2 = s_loses_all ? null_entity
                       : add(12, 6, building_type::processing_facility, "fixture_steel_from_timber");
    const entity_id mine = add(14, 6, building_type::extraction_site, nullptr);

    roster_result out;
    out.s = w->create_entity();
    {
        corporation_component s;
        s.name = "Sereth Works";
        s.focus = industrial_focus::processing;
        // P3 BEFORE P1: a walk in asset order would judge P3 while P1 still
        // carries its stale recipe — the order dependence the review found.
        s.assets = { p3, p1 };
        if (p2 != null_entity)
            s.assets.push_back(p2);
        s.hq_building = p1;
        s.is_player = true;
        w->corporations[out.s] = s;
        w->player_entity = out.s;
    }
    out.t = w->create_entity();
    {
        corporation_component t;
        t.name = "Tolvan Extraction";
        t.focus = industrial_focus::extraction;
        t.assets = { mine };
        t.hq_building = mine;
        w->corporations[out.t] = t;
    }

    out.first = enforce_chain_feasible_roster(*w, reg, /*seed=*/1185u);
    out.digest_first = world_state_digest(*w);
    out.second = enforce_chain_feasible_roster(*w, reg, /*seed=*/1185u);
    out.digest_second = world_state_digest(*w);
    out.audit = audit_chain_feasibility(*w, reg);
    out.p1_gone = w->buildings.count(p1) == 0;
    out.p3_gone = w->buildings.count(p3) == 0;
    out.p2_kept = p2 != null_entity && w->buildings.count(p2) != 0
               && w->buildings.at(p2).recipe == reg.recipe_id("fixture_steel_from_timber");
    return out;
}

} // namespace chainfx

// ---------------------------------------------------------------------------
// 7. THE WATER DIG LADDER (BL-1197, gap firm digs the gap; the review's rows)
// ---------------------------------------------------------------------------
// One nation on a 24 x 12 barren body, a centre at (6, 6) with a window of
// radius 4, and households that want WATER and STONE (the turn takes water,
// index 7, before stone, 11). Stone stands at (9, 6) and (9, 7). Water ground,
// by mode:
//   well_and_ice  a Well site (a river along (7, 6)) AND an ice deposit at (5, 6)
//   ice_only      the ice deposit at (5, 6) alone
//   far_ice_only  an ice deposit at (18, 0) only — outside the window, so water
//                 is producible on the body (in G) but this centre has no ground
//   no_water      no Well site and no ice anywhere (Pass 6's mask case)
namespace waterfx {

enum class mode { well_and_ice, ice_only, far_ice_only, no_water };

struct result
{
    std::size_t firms        = 0;
    int  water_firms         = 0;  ///< charters booked to water
    int  stone_firms         = 0;  ///< charters booked to stone
    bool anchor_at_well      = false;
    bool anchor_at_ice       = false;
    bool any_water_site      = false; ///< some placed building targets water
    bool balanced            = false;
    long long unspent        = 0;
};

std::unique_ptr<world> build(mode m, recipe_registry& reg, entity_id& centre,
                             std::map<std::pair<int, int>, entity_id>& at)
{
    auto w = std::make_unique<world>();
    const int bw = 24, bh = 12;
    const entity_id body = w->create_entity();
    {
        body_component bc{};
        bc.name = "WaterBody";
        bc.grid_width = bw;
        bc.grid_height = bh;
        w->bodies[body] = bc;
    }
    const entity_id nation = w->create_entity();
    nation_component nc{};
    nc.name = "Veyl";
    const std::size_t water = static_cast<std::size_t>(resource_type::water);
    const std::size_t stone = static_cast<std::size_t>(resource_type::stone);
    for (int y = 0; y < bh; ++y)
        for (int x = 0; x < bw; ++x)
        {
            const entity_id tid = w->create_entity();
            tile_component tc{};
            tc.body = body;
            tc.grid_x = x;
            tc.grid_y = y;
            tc.substrate = terrain_substrate::barren;
            tc.habitability = 0.5f;
            if (x == 9 && (y == 6 || y == 7))
            {
                tc.resource_deposit[stone]   = 1.0f;
                tc.resource_remaining[stone] = 1000.0f;
            }
            const bool ice_here =
                ((m == mode::well_and_ice || m == mode::ice_only) && x == 5 && y == 6)
                || (m == mode::far_ice_only && x == 18 && y == 0);
            if (ice_here)
            {
                tc.resource_deposit[water]   = 1.0f;
                tc.resource_remaining[water] = 1000.0f;
            }
            if (m == mode::well_and_ice && x == 7 && y == 6)
                tc.river_edges = 1;   // a river along the tile: a Well site
            w->tiles[tid] = tc;
            nc.tiles.push_back(tid);
            w->tile_to_nation[tid] = nation;
            at[{ x, y }] = tid;
        }
    w->nations[nation] = nc;

    centre = w->create_entity();
    population_centre_component pc{};
    pc.scale = 5;
    w->population_centres[centre] = pc;
    w->population_centre_tile[centre] = at.at({ 6, 6 });

    population_demand_params pd;
    pd.demand_basket[water] = 100.0f;
    pd.demand_basket[stone] = 100.0f;
    reg.set_population_demand(pd);
    building_economics ext;
    ext.base_rate = 1.0f;
    reg.set_economics(building_type::extraction_site, ext);
    return w;
}

/// The charter walk (sqrt_capital, the turn) with @p points firm charters.
result run_walk(mode m, std::int32_t points)
{
    recipe_registry reg;
    entity_id centre = null_entity;
    std::map<std::pair<int, int>, entity_id> at;
    auto w = build(m, reg, centre, at);

    charter_spend_params s;
    s.firm_price_points        = 1;
    s.specialist_firm_charters = 1000;
    s.window_radius            = 4;
    s.province_cap             = false;
    s.resource_cap_rule        = charter_cap_rule::sqrt_capital;
    s.per_resource_firm_cap    = 2;
    s.max_firms_per_body       = 200;
    s.density_ceiling          = 120;
    const charter_budget budget(std::map<entity_id, std::int32_t>{ { centre, points } });

    charter_spend_report rep;
    charter_web_from_budget(*w, reg, budget, s, /*seed=*/1197u, /*settle=*/nullptr, &rep);
    result out;
    out.firms = rep.firms.size();
    for (const charter_record& r : rep.charters)
    {
        if (r.specialist)
            continue;
        if (r.good == static_cast<std::uint16_t>(resource_type::water)) ++out.water_firms;
        if (r.good == static_cast<std::uint16_t>(resource_type::stone)) ++out.stone_firms;
        const corporation_component& corp = w->corporations.at(r.corp);
        if (corp.assets.empty())
            continue;
        const building_component& a = w->buildings.at(corp.assets.front());
        if (a.target_resource == resource_type::water && a.tile == at.at({ 7, 6 }))
            out.anchor_at_well = true;
        if (a.target_resource == resource_type::water && a.tile == at.at({ 5, 6 }))
            out.anchor_at_ice = true;
    }
    for (const auto& [bid, b] : w->buildings)
        if (b.type == building_type::extraction_site && b.target_resource == resource_type::water)
            out.any_water_site = true;
    for (const charter_unspent& u : rep.unspent)
        out.unspent += u.points;
    out.balanced = rep.points_spent + out.unspent == budget.total();
    return out;
}

/// Budget-less Pass 6 (`generate_background_firms`) on the same ground.
result run_pass6(mode m)
{
    recipe_registry reg;
    entity_id centre = null_entity;
    std::map<std::pair<int, int>, entity_id> at;
    auto w = build(m, reg, centre, at);
    const std::vector<entity_id> firms = generate_background_firms(*w, reg, /*seed=*/1197u);
    result out;
    out.firms = firms.size();
    for (const entity_id cid : firms)
    {
        const corporation_component& corp = w->corporations.at(cid);
        if (corp.assets.empty())
            continue;
        const building_component& a = w->buildings.at(corp.assets.front());
        if (a.type == building_type::extraction_site && a.target_resource == resource_type::stone)
            ++out.stone_firms;
        if (a.type == building_type::extraction_site && a.target_resource == resource_type::water)
        {
            ++out.water_firms;
            if (a.tile == at.at({ 7, 6 }))
                out.anchor_at_well = true;
        }
    }
    for (const auto& [bid, b] : w->buildings)
        if (b.type == building_type::extraction_site && b.target_resource == resource_type::water)
            out.any_water_site = true;
    out.balanced = true;
    return out;
}

} // namespace waterfx

// ---------------------------------------------------------------------------
// 8. THE PRODUCE DIG LADDER (BL-1208, generation sites wharves)
// ---------------------------------------------------------------------------
// The water fixture's shape for AGRICULTURAL PRODUCE: one nation on a 24 x 12
// barren body, a centre at (6, 6) with a window of radius 4, households that
// want PRODUCE and STONE (the turn takes produce, index 6, before stone, 11).
// Stone stands at (9, 6) and (9, 7). Produce ground, by mode:
//   deposit_and_coast  a produce deposit at (5, 6) AND a sea tile at (7, 8),
//                      whose land neighbours are Wharf sites
//   coast_only         the sea tile at (7, 8) alone — a coastal body with no
//                      produce deposit anywhere (the BL-1208 gap)
//   far_coast_only     a sea tile at (18, 0) only — outside the window, so
//                      produce is producible on the body but not at this centre
//   no_produce         no sea and no produce deposit anywhere (Pass 6's mask)
namespace producefx {

enum class mode { deposit_and_coast, coast_only, far_coast_only, no_produce };

struct result
{
    std::size_t firms        = 0;
    int  produce_firms       = 0;  ///< charters / firms whose anchor digs produce
    int  stone_firms         = 0;
    bool anchor_at_deposit   = false;
    bool anchor_at_wharf     = false; ///< a produce anchor on a Wharf site
    bool any_produce_site    = false; ///< some placed building targets produce
    bool balanced            = false;
};

std::unique_ptr<world> build(mode m, recipe_registry& reg, entity_id& centre,
                             std::map<std::pair<int, int>, entity_id>& at)
{
    auto w = std::make_unique<world>();
    const int bw = 24, bh = 12;
    const entity_id body = w->create_entity();
    {
        body_component bc{};
        bc.name = "FoodBody";
        bc.grid_width = bw;
        bc.grid_height = bh;
        w->bodies[body] = bc;
    }
    const entity_id nation = w->create_entity();
    nation_component nc{};
    nc.name = "Oskar";
    const std::size_t food  = static_cast<std::size_t>(resource_type::agricultural_produce);
    const std::size_t stone = static_cast<std::size_t>(resource_type::stone);
    for (int y = 0; y < bh; ++y)
        for (int x = 0; x < bw; ++x)
        {
            const entity_id tid = w->create_entity();
            tile_component tc{};
            tc.body = body;
            tc.grid_x = x;
            tc.grid_y = y;
            tc.substrate = terrain_substrate::barren;
            tc.habitability = 0.5f;
            if (x == 9 && (y == 6 || y == 7))
            {
                tc.resource_deposit[stone]   = 1.0f;
                tc.resource_remaining[stone] = 1000.0f;
            }
            if (m == mode::deposit_and_coast && x == 5 && y == 6)
            {
                tc.resource_deposit[food]   = 1.0f;
                tc.resource_remaining[food] = 1000.0f;
            }
            const bool sea_here =
                ((m == mode::deposit_and_coast || m == mode::coast_only) && x == 7 && y == 8)
                || (m == mode::far_coast_only && x == 18 && y == 0);
            if (sea_here)
            {
                tc.substrate    = terrain_substrate::ocean;
                tc.habitability = 0.0f;
            }
            w->tiles[tid] = tc;
            nc.tiles.push_back(tid);
            w->tile_to_nation[tid] = nation;
            at[{ x, y }] = tid;
        }
    w->nations[nation] = nc;

    centre = w->create_entity();
    population_centre_component pc{};
    pc.scale = 5;
    w->population_centres[centre] = pc;
    w->population_centre_tile[centre] = at.at({ 6, 6 });

    population_demand_params pd;
    pd.demand_basket[food]  = 100.0f;
    pd.demand_basket[stone] = 100.0f;
    reg.set_population_demand(pd);
    building_economics ext;
    ext.base_rate = 1.0f;
    reg.set_economics(building_type::extraction_site, ext);
    return w;
}

void read_anchor(const world& w, const building_component& a,
                 const std::map<std::pair<int, int>, entity_id>& at, result& out)
{
    if (a.type != building_type::extraction_site)
        return;
    if (a.target_resource == resource_type::stone)
        ++out.stone_firms;
    if (a.target_resource != resource_type::agricultural_produce)
        return;
    ++out.produce_firms;
    if (a.tile == at.at({ 5, 6 }))
        out.anchor_at_deposit = true;
    if (placement_rules::is_wharf_site(w, a.tile, resource_type::agricultural_produce))
        out.anchor_at_wharf = true;
}

/// The charter walk (sqrt_capital, the turn) with @p points firm charters.
result run_walk(mode m, std::int32_t points)
{
    recipe_registry reg;
    entity_id centre = null_entity;
    std::map<std::pair<int, int>, entity_id> at;
    auto w = build(m, reg, centre, at);

    charter_spend_params s;
    s.firm_price_points        = 1;
    s.specialist_firm_charters = 1000;
    s.window_radius            = 4;
    s.province_cap             = false;
    s.resource_cap_rule        = charter_cap_rule::sqrt_capital;
    s.per_resource_firm_cap    = 2;
    s.max_firms_per_body       = 200;
    s.density_ceiling          = 120;
    const charter_budget budget(std::map<entity_id, std::int32_t>{ { centre, points } });

    charter_spend_report rep;
    charter_web_from_budget(*w, reg, budget, s, /*seed=*/1208u, /*settle=*/nullptr, &rep);
    result out;
    out.firms = rep.firms.size();
    for (const charter_record& r : rep.charters)
    {
        if (r.specialist)
            continue;
        const corporation_component& corp = w->corporations.at(r.corp);
        if (corp.assets.empty())
            continue;
        read_anchor(*w, w->buildings.at(corp.assets.front()), at, out);
    }
    for (const auto& [bid, b] : w->buildings)
        if (b.type == building_type::extraction_site
            && b.target_resource == resource_type::agricultural_produce)
            out.any_produce_site = true;
    long long unspent = 0;
    for (const charter_unspent& u : rep.unspent)
        unspent += u.points;
    out.balanced = rep.points_spent + unspent == budget.total();
    return out;
}

/// Budget-less Pass 6 (`generate_background_firms`) on the same ground.
result run_pass6(mode m)
{
    recipe_registry reg;
    entity_id centre = null_entity;
    std::map<std::pair<int, int>, entity_id> at;
    auto w = build(m, reg, centre, at);
    const std::vector<entity_id> firms = generate_background_firms(*w, reg, /*seed=*/1208u);
    result out;
    out.firms = firms.size();
    for (const entity_id cid : firms)
    {
        const corporation_component& corp = w->corporations.at(cid);
        if (corp.assets.empty())
            continue;
        read_anchor(*w, w->buildings.at(corp.assets.front()), at, out);
    }
    for (const auto& [bid, b] : w->buildings)
        if (b.type == building_type::extraction_site
            && b.target_resource == resource_type::agricultural_produce)
            out.any_produce_site = true;
    out.balanced = true;
    return out;
}

} // namespace producefx

// ---------------------------------------------------------------------------
// BL-1233 (processors to inputs) — the SIZED rule: a processor needs SPARE
// reachable supply covering its draw at t_idle (CORPORATION_GENERATION.md § Pass
// 3, "Sized to its inputs"). One body, timber only; every market prices timber
// alike, so every market pair with a cheap haul is within reach.
namespace sizedfx {

constexpr std::size_t k_timber = static_cast<std::size_t>(resource_type::timber);
constexpr std::size_t k_steel  = static_cast<std::size_t>(resource_type::steel);
constexpr std::size_t k_tools  = static_cast<std::size_t>(resource_type::tools);

struct fx
{
    std::unique_ptr<world> w = std::make_unique<world>();
    recipe_registry reg;
    std::map<std::pair<int, int>, entity_id> at;
    std::vector<entity_id> markets;

    /// A 24x12 body, markets centred at @p centres (x, row 6), timber deposits
    /// at @p deposits (x, row 6).
    fx(const std::vector<int>& centres, const std::vector<int>& deposits)
    {
        const entity_id body = w->create_entity();
        body_component bc{};
        bc.name = "SizedBody";
        bc.grid_width = 24;
        bc.grid_height = 12;
        w->bodies[body] = bc;
        const entity_id nation = w->create_entity();
        nation_component nc{};
        nc.name = "Veyl";
        for (int y = 0; y < 12; ++y)
            for (int x = 0; x < 24; ++x)
            {
                const entity_id tid = w->create_entity();
                tile_component tc{};
                tc.body = body;
                tc.grid_x = x;
                tc.grid_y = y;
                tc.substrate = terrain_substrate::barren;
                if (y == 6 && std::find(deposits.begin(), deposits.end(), x) != deposits.end())
                {
                    tc.resource_deposit[k_timber]   = 1.0f;
                    tc.resource_remaining[k_timber] = 1000.0f;
                }
                w->tiles[tid] = tc;
                nc.tiles.push_back(tid);
                w->tile_to_nation[tid] = nation;
                at[{ x, y }] = tid;
            }
        w->nations[nation] = nc;
        for (const int cx : centres)
        {
            const entity_id mid = w->create_entity();
            market_component m{};
            m.body = body;
            m.centre_tile = at.at({ cx, 6 });
            m.base_price.fill(1.0f);
            m.price = m.base_price;
            w->markets[mid] = m;
            markets.push_back(mid);
        }
        building_economics e;
        e.base_rate = 1.0f;
        reg.set_economics(building_type::extraction_site, e);
        reg.set_economics(building_type::processing_facility, e);
    }
    entity_id add(int x, building_type t, const char* rname = nullptr)
    {
        const entity_id bid = w->create_entity();
        building_component b{};
        b.tile = at.at({ x, 6 });
        b.type = t;
        b.workforce_assigned = 0.5f;
        if (t == building_type::extraction_site)
            b.target_resource = resource_type::timber;
        if (rname != nullptr)
            b.recipe = reg.recipe_id(rname);
        w->buildings[bid] = b;
        w->stockpiles[bid] = stockpile_component{};
        return bid;
    }
    float output(entity_id bid, std::size_t r) const
    {
        return building_output(*w, reg, bid, w->buildings.at(bid), r, nullptr);
    }
    entity_id corp(const char* name, industrial_focus f, std::vector<entity_id> assets, bool seat)
    {
        const entity_id cid = w->create_entity();
        corporation_component c;
        c.name = name;
        c.focus = f;
        c.assets = std::move(assets);
        c.hq_building = c.assets.front();
        c.is_player = seat;
        w->corporations[cid] = c;
        if (seat)
            w->player_entity = cid;
        return cid;
    }
};

/// One processor whose recipe needs @p need_over_output x the mine's output at
/// t_idle, beside the mine, in one market. Kept after the roster enforcement?
struct one_result { float out = 0, need = 0; bool kept = false; };
one_result run_one(float need_over_output)
{
    fx f({ 6 }, { 5 });
    const entity_id mine = f.add(5, building_type::extraction_site);
    one_result r;
    r.out = f.output(mine, k_timber);
    // need at t_idle = inputs x judged batches (rate 1 x labour 0.5) x t_idle.
    const float batches = 0.5f;
    recipe rc;
    rc.name = "fixture_steel_from_timber";
    rc.inputs[k_timber] = need_over_output * r.out / (batches * f.reg.t_idle());
    rc.outputs[k_steel] = 1.0f;
    f.reg.add_recipe(rc);
    r.need = rc.inputs[k_timber] * batches * f.reg.t_idle();
    const entity_id p = f.add(6, building_type::processing_facility, rc.name.c_str());
    f.corp("Sereth Works", industrial_focus::processing, { p }, true);
    f.corp("Tolvan Extraction", industrial_focus::extraction, { mine }, false);
    enforce_chain_feasible_roster(*f.w, f.reg, /*seed=*/1233u);
    r.kept = f.w->buildings.count(p) != 0 && f.w->buildings.at(p).recipe != no_recipe;
    return r;
}

/// Two producer markets (x 2 and x 18) each reaching a consumer market (x 10)
/// whose standing works draws 0.75 x their summed output: each producer market
/// alone is overdrawn, the set is not.
struct twin_result { float o1 = 0, o2 = 0, drawn = 0, spare = 0; bool reach1 = false, reach2 = false; };
twin_result run_twin()
{
    fx f({ 2, 10, 18 }, { 1, 19 });
    const entity_id m1 = f.add(1, building_type::extraction_site);
    const entity_id m2 = f.add(19, building_type::extraction_site);
    twin_result r;
    r.o1 = f.output(m1, k_timber);
    r.o2 = f.output(m2, k_timber);
    recipe rc;
    rc.name = "fixture_steel_from_timber";
    rc.inputs[k_timber] = 0.75f * (r.o1 + r.o2) / 0.5f; // draw = inputs x rate 1 x labour 0.5
    rc.outputs[k_steel] = 1.0f;
    f.reg.add_recipe(rc);
    const entity_id q = f.add(10, building_type::processing_facility, rc.name.c_str());
    r.drawn = building_draw(f.reg, f.w->buildings.at(q), k_timber);
    input_reach ir = make_input_reach(*f.w, f.reg);
    const entity_id cm = market_for_tile(*f.w, f.at.at({ 10, 6 }));
    r.reach1 = market_within_reach(*f.w, f.reg, ir, market_for_tile(*f.w, f.at.at({ 1, 6 })), cm, k_timber);
    r.reach2 = market_within_reach(*f.w, f.reg, ir, market_for_tile(*f.w, f.at.at({ 19, 6 })), cm, k_timber);
    r.spare = reachable_supply(*f.w, f.reg, ir, cm, k_timber, null_entity).spare;
    invalidate_logistics_caches(*f.w);
    return r;
}

/// CONTENDED SUPPLY AND A CASCADE: one mine; two steel works P1 (its market)
/// and P2 (the next market, within the mine's reach), each needing 0.4 x its output at t_idle and drawing 2 x it at full
/// run, so either fits alone and both together do not; a tools works T in a
/// third market no timber reaches, fed only by their steel, listed FIRST.
/// Enforcement suspends P1 and P2 together,
/// then T for want of steel; re-admission must take P1 back and then T.
struct contend_result
{
    chain_roster_enforcement first, second;
    std::uint64_t d1 = 0, d2 = 0;
    int works_kept = 0;
    bool tools_kept = false;
    std::string held; ///< what T, P1, P2 hold after the first call
};
contend_result run_contend()
{
    // P2 stands in a second market (x 14) whose steel is cheap, so no steel
    // reaches it: it can only be a steel works, never switch to tools.
    // T stands in a third market (x 22) whose timber is cheap, so no timber
    // reaches it: it can only be a tools works, fed by steel from A.
    fx f({ 6, 14, 22 }, { 5 });
    {
        market_component& b = f.w->markets.at(f.markets[1]);
        b.base_price[k_steel] = 0.2f;
        b.price = b.base_price;
        market_component& c = f.w->markets.at(f.markets[2]);
        c.base_price[k_timber] = 0.2f;
        c.price = c.base_price;
    }
    const entity_id mine = f.add(5, building_type::extraction_site);
    const float out = f.output(mine, k_timber);
    recipe st;
    st.name = "fixture_steel_from_timber";
    st.inputs[k_timber] = 0.4f * out / (0.5f * f.reg.t_idle());
    st.outputs[k_steel] = 1.0f;
    f.reg.add_recipe(st);
    recipe tl;
    tl.name = "fixture_tools_from_steel";
    tl.inputs[k_steel] = 0.5f;
    tl.outputs[k_tools] = 1.0f;
    f.reg.add_recipe(tl);
    const entity_id t  = f.add(21, building_type::processing_facility, tl.name.c_str());
    const entity_id p1 = f.add(6, building_type::processing_facility, st.name.c_str());
    const entity_id p2 = f.add(13, building_type::processing_facility, st.name.c_str());
    f.corp("Sereth Works", industrial_focus::processing, { t, p1, p2 }, true);
    f.corp("Tolvan Extraction", industrial_focus::extraction, { mine }, false);
    contend_result r;
    r.first  = enforce_chain_feasible_roster(*f.w, f.reg, /*seed=*/1233u);
    r.d1     = world_state_digest(*f.w);
    for (const entity_id b : { t, p1, p2 })
    {
        const auto it = f.w->buildings.find(b);
        const recipe* rc = it == f.w->buildings.end() ? nullptr : f.reg.get_recipe(it->second.recipe);
        r.held += it == f.w->buildings.end() ? "gone" : (rc ? rc->name : "none");
        r.held += ' ';
    }
    r.second = enforce_chain_feasible_roster(*f.w, f.reg, /*seed=*/1233u);
    r.d2     = world_state_digest(*f.w);
    const uint16_t sid = f.reg.recipe_id(st.name);
    for (const entity_id p : { p1, p2 })
        if (f.w->buildings.count(p) && f.w->buildings.at(p).recipe == sid)
            ++r.works_kept;
    r.tools_kept = f.w->buildings.count(t) != 0
                && f.w->buildings.at(t).recipe == f.reg.recipe_id(tl.name);
    return r;
}

/// Set a market's base (and current) price of one good.
void price(fx& f, std::size_t mi, std::size_t r, float base)
{
    market_component& m = f.w->markets.at(f.markets[mi]);
    m.base_price[r] = base;
    m.price[r]      = base;
}

/// REVIEW ROUND 2 — SPARE BOUNDED BY WHAT REACHES THE CONSUMER. C (x 6) holds
/// mine A and is the asker's market; B (x 14) holds mine B; Q (x 22) holds a
/// works drawing 10 x a mine's output. C's timber is dear (3), Q's cheap (0.8),
/// so C's timber cannot reach Q while B's reaches both C and Q. Q may take only
/// B's output from the set, so A's output stays spare.
struct bound_result { float oa = 0, ob = 0, dq = 0, spare = 0; bool c_q = true, b_q = false, b_c = false; };
bound_result run_bound()
{
    fx f({ 6, 14, 22 }, { 5, 13 });
    price(f, 0, k_timber, 3.0f);
    price(f, 2, k_timber, 0.8f);
    const entity_id ma = f.add(5, building_type::extraction_site);
    const entity_id mb = f.add(13, building_type::extraction_site);
    bound_result r;
    r.oa = f.output(ma, k_timber);
    r.ob = f.output(mb, k_timber);
    recipe rc;
    rc.name = "fixture_steel_from_timber";
    rc.inputs[k_timber] = 10.0f * r.oa / 0.5f;
    rc.outputs[k_steel] = 1.0f;
    f.reg.add_recipe(rc);
    const entity_id q = f.add(21, building_type::processing_facility, rc.name.c_str());
    r.dq = building_draw(f.reg, f.w->buildings.at(q), k_timber);
    input_reach ir = make_input_reach(*f.w, f.reg);
    r.c_q = market_within_reach(*f.w, f.reg, ir, f.markets[0], f.markets[2], k_timber);
    r.b_q = market_within_reach(*f.w, f.reg, ir, f.markets[1], f.markets[2], k_timber);
    r.b_c = market_within_reach(*f.w, f.reg, ir, f.markets[1], f.markets[0], k_timber);
    r.spare = reachable_supply(*f.w, f.reg, ir, f.markets[0], k_timber, null_entity).spare;
    invalidate_logistics_caches(*f.w);
    return r;
}

/// REVIEW ROUND 2 — THE QUOTED COST IS A PRODUCER'S WITH SPARE. The asker's
/// market C (x 6, timber 1). P1 (x 14) sells timber at 0.5 but a works there
/// draws its whole output, and P1 cannot import (so the works is P1's alone);
/// P2 (x 22) sells at 2 with spare. The quote is P2's landed cost, not P1's.
struct quote_result { float landed = -1, spare = 0; bool p1_c = false, p2_c = false, p2_p1 = true; };
quote_result run_quote()
{
    fx f({ 6, 14, 22 }, { 13, 21 });
    price(f, 1, k_timber, 0.5f);
    price(f, 2, k_timber, 2.0f);
    const entity_id m1 = f.add(13, building_type::extraction_site);
    f.add(21, building_type::extraction_site);
    recipe rc;
    rc.name = "fixture_steel_from_timber";
    rc.inputs[k_timber] = f.output(m1, k_timber) / 0.5f; // draws exactly P1's output
    rc.outputs[k_steel] = 1.0f;
    f.reg.add_recipe(rc);
    f.add(15, building_type::processing_facility, rc.name.c_str());
    input_reach ir = make_input_reach(*f.w, f.reg);
    quote_result r;
    r.p1_c  = market_within_reach(*f.w, f.reg, ir, f.markets[1], f.markets[0], k_timber);
    r.p2_c  = market_within_reach(*f.w, f.reg, ir, f.markets[2], f.markets[0], k_timber);
    r.p2_p1 = market_within_reach(*f.w, f.reg, ir, f.markets[2], f.markets[1], k_timber);
    const reachable_spare rs = reachable_supply(*f.w, f.reg, ir, f.markets[0], k_timber, null_entity);
    r.landed = rs.landed;
    r.spare  = rs.spare;
    invalidate_logistics_caches(*f.w);
    return r;
}

/// RULING A — THE BOOK. Market A (x 6) is the refused steel works' market; B
/// (x 14) cannot send timber to A (A's timber is cheap). The entry wants 2.0
/// timber a tick. Each step reads what `add_prospective_draws` adds to timber's
/// demand, and whether the entry survives.
struct book_result
{
    float set_once = 0, twice = 0;           ///< draw after one and two refusals
    float bare = 0;                          ///< added with no producer anywhere
    float out_of_reach = 0;                  ///< after a mine lands in B
    float far_centre = 0;                    ///< at a centre in B (B cannot reach A)
    float in_reach = 0, mine_out = 0;        ///< after a mine lands in A
    bool  kept_capped = true, kept_not_short = true, kept_short = false;
};
book_result run_book()
{
    fx f({ 6, 14 }, { 5, 13 });
    price(f, 0, k_timber, 0.2f);
    recipe rc;
    rc.name = "fixture_steel_from_timber";
    rc.inputs[k_timber] = 1.0f;
    rc.outputs[k_steel] = 1.0f;
    f.reg.add_recipe(rc);
    refused_draw ref;
    ref.set    = true;
    ref.market = f.markets[0];
    ref.draw[k_timber] = 2.0f;
    prospective_draws book;
    record_refused_draw(book, k_steel, ref);
    book_result r;
    r.set_once = book.at(k_steel).draw[k_timber];
    record_refused_draw(book, k_steel, ref); // the retry refused again
    r.twice = book.at(k_steel).draw[k_timber];
    input_reach ir = make_input_reach(*f.w, f.reg);
    std::array<bool, resource_count> none{};
    // The body is GLUTTED with timber elsewhere (review round 3): its production
    // dwarfs every demand, so only a want read as a shortage of its own shows.
    std::array<float, resource_count> production{};
    production[k_timber] = 1000.0f;
    // What it reads: timber's shortage — demand over the body's production.
    const auto added = [&](entity_id centre, std::array<bool, resource_count> capped,
                           std::array<float, resource_count> prod) {
        std::array<float, resource_count> demand{};
        demand[k_steel] = 100.0f; // the good the refused works serves is short
        add_prospective_draws(*f.w, f.reg, ir, book, centre, demand, prod, capped);
        return std::max(0.0f, demand[k_timber] - prod[k_timber]);
    };
    r.bare = added(f.markets[0], none, production);
    f.add(13, building_type::extraction_site);            // a timber firm lands in B
    r.out_of_reach = added(f.markets[0], none, production);
    r.far_centre   = added(f.markets[1], none, production);
    const entity_id ma = f.add(5, building_type::extraction_site); // and one in A
    r.mine_out = f.output(ma, k_timber);
    r.in_reach = added(f.markets[0], none, production);
    {
        prospective_draws keep = book;
        std::array<bool, resource_count> capped{};
        capped[k_steel] = true;
        added(f.markets[0], capped, production);
        r.kept_capped = book.count(k_steel) != 0;
        book = keep;
    }
    {
        prospective_draws keep = book;
        std::array<float, resource_count> prod = production;
        prod[k_steel] = 1000.0f; // steel no longer short
        added(f.markets[0], none, prod);
        r.kept_not_short = book.count(k_steel) != 0;
        book = keep;
    }
    added(f.markets[0], none, production);
    r.kept_short = book.count(k_steel) != 0;
    invalidate_logistics_caches(*f.w);
    return r;
}

/// RULING A — THE WALK. One market. A standing works draws the whole output of
/// the one standing timber site (x 10-11, outside the centre's window), so the
/// market has no spare timber. Free timber lies at the window's four edges
/// (distance 2 from (6, 6) along each axis). A steel works here needs 0.75 timber at t_idle — more
/// than the one feed site a processing firm brings (0.5) — so the steel firm is
/// refused. Its prospective draw must charter a timber firm (two or three sites)
/// in the window, after which the steel firm places on the retry. A free recipe
/// (no inputs) heads the registry so an extraction firm's incidental processor
/// takes it and leaves the timber alone.
struct walk_result
{
    charter_spend_report rep;
    int  steel_at = -1, timber_after = -1; ///< charter indices; -1 none
    long long chain_infeasible = 0;
    bool balanced = false;
};
walk_result run_walk(std::int32_t points)
{
    fx f({ 4 }, {});
    const auto deposit = [&](int x, int y, std::size_t r, float rich) {
        tile_component& tc = f.w->tiles.at(f.at.at({ x, y }));
        tc.resource_deposit[r]   = rich;
        tc.resource_remaining[r] = 1000.0f;
    };
    for (const auto& xy : { std::make_pair(4, 6), std::make_pair(8, 6), std::make_pair(6, 4),
                            std::make_pair(6, 8) })
        deposit(xy.first, xy.second, k_timber, 1.0f);
    deposit(10, 6, k_timber, 1.0f);
    {
        recipe free;
        free.name = "fixture_free_tools";
        free.outputs[k_tools] = 1.0f;
        f.reg.add_recipe(free);
    }
    recipe rc;
    rc.name = "fixture_steel_from_timber";
    rc.inputs[k_timber] = 0.75f / (0.5f * f.reg.t_idle()); // need 0.75 at t_idle
    rc.outputs[k_steel] = 1.0f;
    f.reg.add_recipe(rc);
    const entity_id site = f.add(10, building_type::extraction_site);
    // The works draws exactly the site's output (draw = inputs x rate 1 x labour 0.5).
    {
        // It makes tools, not steel: a steel firm cannot take this recipe.
        recipe heavy;
        heavy.name = "fixture_heavy_tools_from_timber";
        heavy.outputs[k_tools] = 1.0f;
        heavy.inputs[k_timber] = f.output(site, k_timber) / 0.5f;
        f.reg.add_recipe(heavy);
    }
    {
        const entity_id bid = f.w->create_entity();
        building_component b{};
        b.tile = f.at.at({ 11, 6 });
        b.type = building_type::processing_facility;
        b.recipe = f.reg.recipe_id("fixture_heavy_tools_from_timber");
        b.workforce_assigned = 0.5f;
        f.w->buildings[bid] = b;
        f.w->stockpiles[bid] = stockpile_component{};
    }
    const entity_id centre = f.w->create_entity();
    population_centre_component pc{};
    pc.scale = 5;
    f.w->population_centres[centre] = pc;
    f.w->population_centre_tile[centre] = f.at.at({ 6, 6 });
    population_demand_params pd;
    pd.demand_basket[k_steel] = 100.0f;
    f.reg.set_population_demand(pd);

    charter_spend_params s;
    s.firm_price_points        = 1;
    s.specialist_firm_charters = 1000;
    s.window_radius            = 2;
    s.province_cap             = false;
    s.resource_cap_rule        = charter_cap_rule::sqrt_capital;
    s.per_resource_firm_cap    = 2;
    s.max_firms_per_body       = 200;
    s.density_ceiling          = 120;
    const charter_budget budget(std::map<entity_id, std::int32_t>{ { centre, points } });
    walk_result out;
    charter_web_from_budget(*f.w, f.reg, budget, s, /*seed=*/1233u, /*settle=*/nullptr, &out.rep);
    for (std::size_t i = 0; i < out.rep.charters.size(); ++i)
        if (out.rep.charters[i].good == k_steel && out.steel_at < 0)
            out.steel_at = static_cast<int>(i);
    for (std::size_t i = 0; i < out.rep.charters.size(); ++i)
        if (out.rep.charters[i].good == k_timber && out.timber_after < 0)
            out.timber_after = static_cast<int>(i);
    long long unspent = 0;
    for (const charter_unspent& u : out.rep.unspent)
    {
        unspent += u.points;
        if (u.reason == charter_unspent_reason::chain_infeasible)
            out.chain_infeasible += u.points;
    }
    out.balanced = out.rep.points_spent + unspent == budget.total();
    return out;
}

/// THE REFRESH IS A FULL BUILD. One world mutated step by step — a processor
/// added, a mine removed, a recipe switched, labour changed, a plant
/// decommissioned, an unstaffed plant staffed — the refreshed index compared,
/// entry for entry, with a fresh build after every step.
struct refresh_result { int steps = 0, equal = 0; };
bool same_index(const input_reach& a, const input_reach& b)
{
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (a.producers[r].size() != b.producers[r].size() || a.draws[r] != b.draws[r])
            return false;
        for (std::size_t i = 0; i < a.producers[r].size(); ++i)
            if (a.producers[r][i].market != b.producers[r][i].market
                || a.producers[r][i].building != b.producers[r][i].building
                || a.producers[r][i].out != b.producers[r][i].out)
                return false;
    }
    return true;
}
refresh_result run_refresh()
{
    fx f({ 6, 14 }, { 5, 13, 7 });
    recipe st;
    st.name = "fixture_steel_from_timber";
    st.inputs[k_timber] = 1.0f;
    st.outputs[k_steel] = 1.0f;
    f.reg.add_recipe(st);
    recipe tl;
    tl.name = "fixture_tools_from_steel";
    tl.inputs[k_steel] = 0.5f;
    tl.outputs[k_tools] = 1.0f;
    f.reg.add_recipe(tl);
    const entity_id m1 = f.add(5, building_type::extraction_site);
    const entity_id m2 = f.add(13, building_type::extraction_site);
    const entity_id p1 = f.add(6, building_type::processing_facility, st.name.c_str());
    const entity_id p2 = f.add(14, building_type::processing_facility, tl.name.c_str());
    input_reach ir = make_input_reach(*f.w, f.reg);
    refresh_result out;
    const auto check = [&]() {
        input_reach_refresh(*f.w, f.reg, ir);
        input_reach full = make_input_reach(*f.w, f.reg);
        reachable_supply(*f.w, f.reg, full, f.markets[0], k_timber, null_entity); // builds it
        ++out.steps;
        if (same_index(ir, full))
            ++out.equal;
    };
    check();
    const entity_id p3 = f.add(15, building_type::processing_facility, st.name.c_str()); // add
    check();
    f.w->buildings.erase(m2);                                                     // remove
    f.w->stockpiles.erase(m2);
    check();
    f.w->buildings.at(p1).recipe = f.reg.recipe_id(tl.name);                      // switch
    check();
    f.w->buildings.at(p2).workforce_assigned = 0.8f;                              // labour
    check();
    f.w->buildings.at(p3).decommissioned = true;                                  // decommission
    check();
    const entity_id p4 = f.add(8, building_type::processing_facility, st.name.c_str());
    f.w->buildings.at(p4).workforce_assigned = 0.0f;                              // unstaffed
    check();
    f.w->buildings.at(p4).workforce_assigned = 0.5f;                              // staffed
    check();
    (void)m1;
    invalidate_logistics_caches(*f.w);
    return out;
}

} // namespace sizedfx

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

    // --- BL-1168: a reach price is a positive price no dearer than the world's ---
    { auto s = base(); s.firm_price_points = 4; s.centre_firm_price = { { 1, 2 }, { 2, 4 } };
      expect("reach prices 2 and 4 under a world price of 4", small, s, false); }
    { auto s = base(); s.firm_price_points = 4; s.centre_firm_price = { { 1, 0 } };
      expect("reach price 0", small, s, true, "reach firm price must be > 0"); }
    { auto s = base(); s.firm_price_points = 4; s.centre_firm_price = { { 1, 5 } };
      expect("reach price 5 above a world price of 4", small, s, true, "must not exceed the world's"); }
    { auto s = base(); s.firm_price_points = 4; s.centre_firm_price = { { 1, 2 } };
      expect_true("a listed centre pays its reach price, an unlisted one the world's",
                  s.firm_price_of(1) == 2 && s.firm_price_of(2) == 4
                && s.specialist_price_of(1) == 2LL * s.specialist_firm_charters
                && charter_centre_firm_points(40, s, 1) == ((40 - 2LL * s.specialist_firm_charters) / 2) * 2); }

    // --- lifted: c is unread, so a set c is refused ---
    { auto s = base(); s.resource_cap_rule = charter_cap_rule::lifted;
      expect("lifted rule, c 8 set (unread)", small, s, true, "not read under the lifted"); }
    { auto s = base(); s.resource_cap_rule = charter_cap_rule::lifted; s.per_resource_firm_cap = 0;
      expect("lifted rule, c 0", small, s, false); }

    // --- the density ceiling: sqrt only, 0 < ceiling < guard ---
    { auto s = base(); s.density_ceiling = 120;
      expect("fixed rule with a ceiling set", small, s, true, "read only by the sqrt_capital"); }
    { auto s = base(); s.resource_cap_rule = charter_cap_rule::sqrt_capital;
      expect("sqrt rule, no ceiling", small, s, true, "exactly one of density_ceiling"); }
    expect("sqrt rule, ceiling 120", small, sqrt_spend(), false);
    { auto s = sqrt_spend(); s.density_ceiling = 200;
      expect("sqrt rule, ceiling at the guard", small, s, true, "below max_firms_per_body"); }

    // --- BL-1204: the ceiling per good served — sqrt only, exclusive of a fixed one ---
    { auto s = sqrt_spend(); s.density_ceiling = 0; s.density_per_good_tenths = 75;
      expect("sqrt rule, 7.5 per good", small, s, false); }
    { auto s = sqrt_spend(); s.density_per_good_tenths = 75;
      expect("sqrt rule, a fixed ceiling AND a rate", small, s, true, "exactly one of density_ceiling"); }
    { auto s = base(); s.density_per_good_tenths = 75;
      expect("fixed rule with a rate set", small, s, true, "read only by the sqrt_capital"); }
    { auto s = sqrt_spend(); s.density_ceiling = 0; s.density_per_good_tenths = -75;
      expect("sqrt rule, a negative rate", small, s, true, "must not be negative"); }
    { auto s = sqrt_spend(); s.density_ceiling = 0; s.density_per_good_tenths = 75;
      s.max_firms_per_body = 1;
      expect("sqrt rule, a rate under a guard of 1", small, s, true, "max_firms_per_body >= 2"); }
    {
        // floor(7.5 x |G|) under the guard less one; a fixed ceiling is read as set.
        auto s = sqrt_spend(); s.density_ceiling = 0; s.density_per_good_tenths = 75;
        const auto f = sqrt_spend();
        expect_true("per good: |G| 0/1/16/23/26/27/49 -> 0/7/120/172/195/199/199; fixed 120 at any |G|",
                    charter_density_ceiling(s, 0) == 0 && charter_density_ceiling(s, 1) == 7
                 && charter_density_ceiling(s, 16) == 120 && charter_density_ceiling(s, 23) == 172
                 && charter_density_ceiling(s, 26) == 195 && charter_density_ceiling(s, 27) == 199
                 && charter_density_ceiling(s, static_cast<int>(resource_count)) == 199
                 && charter_density_ceiling(f, 0) == 120 && charter_density_ceiling(f, 49) == 120
                 && charter_density_ceiling(base(), 23) == 0);
    }
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
        // THE STOP'S REASON: province_cap WINS. Every tile is one province and
        // its centre is a VILLAGE, so its cap is 2 firms (BL-1146: 2 x the
        // centre's rung; the demand is held where the scale-5 fixture had it by
        // a basket x5, since demand is per scale point). The window has no
        // deposit tile: the ore fails on the WINDOW, then two works fill the
        // province and the next works fails on the CAP. The centre stops with
        // both reasons among its goods; the cap took ground from one of them, so
        // the rest is province_cap.
        turn::config cfg;
        cfg.g            = ground::outside_window;
        cfg.one_province = true;
        cfg.scale        = 1;
        cfg.basket       = { { turn::k_raw, 500.0f, false }, { turn::k_mill1, 500.0f, true },
                             { turn::k_mill2, 500.0f, true } };
        const turn::reading r = turn::run(cfg);
        turn::print("one province (a village's), no deposit tile", r);
        expect_true("precedence: two works, then the village's province is full",
                    r.firms[turn::k_mill1] == 1 && r.firms[turn::k_mill2] == 1 && r.firms[turn::k_raw] == 0);
        expect_true("precedence: the rest (6) is province_cap, not window_exhausted",
                    r.u(why_t::province_cap) == 6 && r.u(why_t::window_exhausted) == 0 && r.balanced);
    }
    {
        // BL-1146: THE CAP SCALES WITH THE PROVINCE'S CENTRE. The same one-
        // province world with a TOWN at its centre (rung 2, a cap of 4; the
        // same demand, a basket x2.5), and the per-good cap raised to 3 so the
        // works WANT MORE than the town's cap (3 + 3 = 6 > 4): the cap must be
        // what stops them. (At c = 2 the works saturate at 4 on their own, so a
        // cap of 4 or anything larger passes — the review found that row could
        // not fail.) Four works place, two per good; the next is refused by the
        // cap; the ore still fails on the window; the rest (4) is province_cap.
        turn::config cfg;
        cfg.g            = ground::outside_window;
        cfg.one_province = true;
        cfg.scale        = 2;
        cfg.c            = 3;
        cfg.basket       = { { turn::k_raw, 250.0f, false }, { turn::k_mill1, 250.0f, true },
                             { turn::k_mill2, 250.0f, true } };
        const turn::reading r = turn::run(cfg);
        turn::print("one province (a town's), no deposit tile, per-good cap 3", r);
        expect_true("BL-1146: a town's province holds exactly 4 works (its cap), where the works wanted 6",
                    r.firms[turn::k_mill1] == 2 && r.firms[turn::k_mill2] == 2 && r.firms[turn::k_raw] == 0);
        expect_true("BL-1146: the town's cap refuses the rest (4) as province_cap, not window_exhausted",
                    r.u(why_t::province_cap) == 4 && r.u(why_t::window_exhausted) == 0 && r.balanced);
        expect_true("BL-1146: the rule reads 2 per rung, village 2 .. megacity 10",
                    province_firm_cap(1) == 2 && province_firm_cap(2) == 4 && province_firm_cap(3) == 6
                    && province_firm_cap(4) == 8 && province_firm_cap(5) == 10
                    && province_firm_cap(0) == 2 && province_firm_cap(9) == 10);
    }
    {
        // BL-1146 review: THE RUNG HELPER, DIRECTLY — `province_anchors` (the
        // anchor seed_province_holders reads) and `province_centre_rungs` over
        // it. Three provinces on a 6-tile strip:
        //   P1 tiles t0 t1: NO centre              -> no anchor, a village's cap 2;
        //   P2 tiles t2 t3: a city (3) on t2 and a town (2) on t3
        //                   -> the anchor is t2, rung 3, cap 6;
        //   P3 tiles t4 t5: two megacities on t5 (a sum of 10) and a town on t4
        //                   -> the anchor is t5, the sum CLAMPED to rung 5, cap 10.
        // And a tie: a fourth province P4 with two villages, one each on t6 and
        // t7, anchors on the LOWER tile id.
        world w;
        const entity_id body = w.create_entity();
        w.bodies[body] = body_component{};
        std::vector<entity_id> t;
        for (int i = 0; i < 8; ++i)
        {
            const entity_id id = w.create_entity();
            tile_component tc{};
            tc.body = body;
            tc.grid_x = i;
            w.tiles[id] = tc;
            t.push_back(id);
        }
        const auto add_province = [&](std::uint32_t id, std::initializer_list<int> tiles) {
            province pr;
            pr.id   = id;
            pr.body = body;
            for (const int i : tiles)
            {
                pr.tiles.push_back(t[static_cast<std::size_t>(i)]);
                w.provinces.tile_province[t[static_cast<std::size_t>(i)]] = id;
            }
            w.provinces.provinces.push_back(std::move(pr));
        };
        add_province(11u, { 0, 1 });
        add_province(12u, { 2, 3 });
        add_province(13u, { 4, 5 });
        add_province(14u, { 6, 7 });
        const auto add_centre = [&](int tile, int scale) {
            const entity_id id = w.create_entity();
            population_centre_component pc{};
            pc.scale = scale;
            w.population_centres[id] = pc;
            w.population_centre_tile[id] = t[static_cast<std::size_t>(tile)];
        };
        add_centre(2, 3);
        add_centre(3, 2);
        add_centre(5, 5);
        add_centre(5, 5);
        add_centre(4, 2);
        add_centre(7, 1);
        add_centre(6, 1);
        const std::map<std::uint32_t, province_anchor> anchors = province_anchors(w);
        const std::map<std::uint32_t, int> rungs = province_centre_rungs(w);
        expect_true("BL-1146: a province with NO centre has no anchor and a village's cap (2)",
                    anchors.count(11u) == 0 && rungs.count(11u) == 0
                    && province_firm_cap_of(rungs, 11u) == 2 && province_firm_cap_of(rungs, 99u) == 2);
        expect_true("BL-1146: the anchor is the highest summed scale: a city (3) over a town, cap 6",
                    anchors.count(12u) == 1 && anchors.at(12u).tile == t[2] && anchors.at(12u).scale == 3
                    && rungs.at(12u) == 3 && province_firm_cap_of(rungs, 12u) == 6);
        expect_true("BL-1146: two megacities on one tile sum to 10, CLAMPED to rung 5, cap 10",
                    anchors.count(13u) == 1 && anchors.at(13u).tile == t[5] && anchors.at(13u).scale == 10
                    && rungs.at(13u) == 5 && province_firm_cap_of(rungs, 13u) == 10);
        expect_true("BL-1146: a tie in summed scale anchors on the LOWER tile id",
                    anchors.count(14u) == 1 && anchors.at(14u).tile == t[6] && rungs.at(14u) == 1);
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
    {
        // BL-1197 review — UNPRODUCIBLE: households want tools, made only from
        // platinum group metals, which this body holds nowhere. Tools leave G,
        // and so do the metals; G's count and the per-good cap are read AFTER
        // that filter; once G fills, the rest books `unproducible`.
        turn::config cfg;
        cfg.offworld_good = true;
        const turn::reading r = turn::run(cfg);
        turn::print("tools need platinum (none on the body)", r);
        const charter_body_record* b = r.rep.bodies.empty() ? nullptr : &r.rep.bodies.front();
        const auto in_g = [&](resource_type g) {
            return b != nullptr
                && std::find(b->goods.begin(), b->goods.end(), static_cast<std::uint16_t>(g))
                       != b->goods.end();
        };
        std::printf("  unproducible: G %d [", b ? b->goods_in_g : -1);
        if (b)
            for (const std::uint16_t g : b->goods)
                std::printf(" %s", resource_names::name_of(static_cast<resource_type>(g)).c_str());
        std::printf(" ], per-good cap %d, tools firms %d\n", b ? static_cast<int>(b->per_good_cap) : -1,
                    r.firms[static_cast<std::size_t>(resource_type::tools)]);
        expect_true("unproducible: tools and platinum are out of G; ore, steel and planks are in",
                    b != nullptr && !in_g(resource_type::tools)
                    && !in_g(resource_type::platinum_group_metals) && in_g(resource_type::iron_ore)
                    && in_g(resource_type::steel) && in_g(resource_type::planks));
        expect_true("unproducible: |G| and the per-good cap are read after the filter (3 goods)",
                    b != nullptr && b->goods_in_g == 3
                    && b->goods_in_g == static_cast<int>(b->goods.size())
                    && b->per_good_cap == charter_sqrt_per_good_cap(2, b->firm_charters, 3, 1));
        expect_true("unproducible: no firm for tools; the rest books unproducible, not late_shortfall",
                    r.firms[static_cast<std::size_t>(resource_type::tools)] == 0
                    && r.u(why_t::unproducible) > 0 && r.u(why_t::late_shortfall) == 0
                    && r.u(why_t::no_gap) == 0 && r.balanced);
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

    // --- 5. THE POOLED REMAINDER (NR-913, a measurement; off by default) ------
    std::printf("\ncharter_refusal_probe — pooled remainders (NR-913)\n");
    {
        // Two centres, 8 and 5 points; a firm costs 3 and no centre affords a
        // specialist (1000 charters). Unpooled: 2 + 1 firms, remainders 2 + 2.
        // Pooled by nation (the fixture has one): the 5-point centre sends its 2
        // to the 8-point centre, which then spends 8 - 2 + 4 = 10 — three firms
        // and a remainder of 1 — so 4 firms stand and 1 point is unspent.
        turn::config cfg;
        cfg.centres = { turn::centre_spec{ turn::k_cx, turn::k_cy, 8 },
                        turn::centre_spec{ turn::k_cx + 3, turn::k_cy, 5 } };
        const auto run_pool = [&](charter_pool pool, charter_spend_report& rep, std::vector<entity_id>& centres) {
            recipe_registry reg;
            std::map<entity_id, std::int32_t> points;
            auto w = turn::build(cfg, reg, points, centres);
            charter_spend_params s;
            s.firm_price_points        = 3;
            s.specialist_firm_charters = 1000;
            s.window_radius            = cfg.radius;
            s.province_cap             = true;
            s.resource_cap_rule        = charter_cap_rule::sqrt_capital;
            s.per_resource_firm_cap    = cfg.c;
            s.max_firms_per_body       = 200;
            s.density_ceiling          = cfg.ceiling;
            s.pool                     = pool;
            const charter_budget budget(points);
            charter_web_from_budget(*w, reg, budget, s, /*seed=*/913u, /*settle=*/nullptr, &rep);
            return world_state_digest(*w);
        };
        charter_spend_report plain, pooled, by_region;
        std::vector<entity_id> c_plain, c_pooled, c_region;
        run_pool(charter_pool::none, plain, c_plain);
        run_pool(charter_pool::nation, pooled, c_pooled);
        run_pool(charter_pool::region, by_region, c_region);
        const auto unspent_at = [](const charter_spend_report& r, entity_id c) {
            long long n = 0;
            for (const charter_unspent& u : r.unspent)
                if (u.centre == c) n += u.points;
            return n;
        };
        std::printf("    unpooled: %zu firms, %lld spent | nation pool: %zu firms, %lld spent, %zu transfer(s)\n",
                    plain.firms.size(), static_cast<long long>(plain.points_spent),
                    pooled.firms.size(), static_cast<long long>(pooled.points_spent),
                    pooled.pool_transfers.size());
        expect_true("pool none: no transfer, 3 firms, remainders 2 + 2 stranded",
                    plain.pool_transfers.empty() && plain.firms.size() == 3 && plain.points_spent == 9
                    && unspent_at(plain, c_plain[0]) == 2 && unspent_at(plain, c_plain[1]) == 2);
        expect_true("pool nation: one transfer, the 5-point centre's 2 to the 8-point centre",
                    pooled.pool_transfers.size() == 1 && pooled.pool_transfers[0].from == c_pooled[1]
                    && pooled.pool_transfers[0].to == c_pooled[0] && pooled.pool_transfers[0].points == 2);
        expect_true("pool nation: 4 firms, 12 spent, 1 unspent at the richest centre, 0 at the sender",
                    pooled.firms.size() == 4 && pooled.points_spent == 12 && pooled.points_unspent == 1
                    && unspent_at(pooled, c_pooled[0]) == 1 && unspent_at(pooled, c_pooled[1]) == 0);
        expect_true("pool region with no carve index: nothing resolves a group, so it is the unpooled walk",
                    by_region.pool_transfers.empty() && by_region.firms.size() == plain.firms.size()
                    && by_region.points_spent == plain.points_spent);
    }

    // --- 6. THE CHAIN RULE (BL-1185) -------------------------------------------
    std::printf("\ncharter_refusal_probe — the chain rule (BL-1185)\n");
    {
        // A works good whose recipe needs an input nothing on the body makes is
        // never chartered, and its points book as chain_infeasible; the mine
        // beside it in the turn still charters.
        turn::config cfg;
        cfg.works_input = true;
        const turn::reading r = turn::run(cfg);
        turn::print("works need timber, none made", r);
        expect_true("chain: steel and planks (timber, unmade) charter no firm; iron ore still does",
                    r.firms[turn::k_mill1] == 0 && r.firms[turn::k_mill2] == 0 && r.firms[turn::k_raw] > 0);
        expect_true("chain: the refused points book as chain_infeasible, and the account closes",
                    r.u(charter_unspent_reason::chain_infeasible) > 0 && r.balanced);
    }
    {
        // THE ANCHOR STAYS OR THE CHARTER GOES (the cold review's HIGH): the
        // anchor (6, 6) is in market A, where nothing makes timber; the firm's
        // second processor lands at (10, 6) in market B, beside the timber.
        // Promoting it to anchor would charter a firm on ground no gate tested.
        const chainfx::result bad = chainfx::run(/*timber_in_a=*/false);
        std::printf("  timber in B: firms %zu, holds in B %d, chain_infeasible %lld%s\n", bad.firms,
                    bad.holds_in_b ? 1 : 0, bad.chain_infeasible, bad.balanced ? "" : " [UNBALANCED]");
        expect_true("anchor lost: the firm is NOT chartered, nothing of it stands in market B",
                    bad.firms == 0 && !bad.holds_in_b);
        expect_true("anchor lost: its point books chain_infeasible, and the account closes",
                    bad.chain_infeasible == 1 && bad.balanced);
        // Control: timber in market A, so the anchor is feasible and stays.
        const chainfx::result good = chainfx::run(/*timber_in_a=*/true);
        std::printf("  timber in A: firms %zu, anchor at (6, 6) %d, chain_infeasible %lld\n", good.firms,
                    good.anchor_at_x ? 1 : 0, good.chain_infeasible);
        expect_true("control: timber in A — the firm charters, anchored at (6, 6)",
                    good.firms == 1 && good.anchor_at_x && good.chain_infeasible == 0 && good.balanced);
        // DERIVED DEMAND (BL-1197 round 4; Ben, 2026-10-05): a steel works
        // standing in market A wants timber, nothing on the body makes it, and
        // an unbuilt timber deposit lies at (5, 6) in the window. No household
        // wants timber, yet the works' input demand puts it in the gap: the walk
        // charters timber's extractor there, and the steel firm then places.
        // (The extractor's own processor slot may take the steel recipe too, fed
        // by its own timber, so at least one firm makes steel.)
        const chainfx::result dd = chainfx::run(/*timber_in_a=*/false, /*spare_deposit=*/true,
                                                /*points=*/2, /*standing_works=*/true,
                                                /*no_producer=*/true, /*spare_richness=*/4.0f);
        std::printf("  derived demand: firms %zu, timber site at (5, 6) %d, steel firms %zu, "
                    "works anchored at (6, 6) %d, chain_infeasible %lld%s\n", dd.firms,
                    dd.input_mine ? 1 : 0, dd.steel_firms, dd.anchor_at_x ? 1 : 0,
                    dd.chain_infeasible, dd.balanced ? "" : " [UNBALANCED]");
        expect_true("derived demand: a standing works' raw is chartered, then the steel firm places",
                    dd.firms == 2 && dd.input_mine && dd.steel_firms >= 1 && dd.anchor_at_x
                    && dd.chain_infeasible == 0 && dd.balanced);
        // Control — the cold start: the same ground with NO works standing.
        // Nothing wants timber, so none is chartered, and steel (whose works
        // would need it) books chain_infeasible: derived demand counts what
        // stands or is chartered, never what a refused firm would have wanted.
        const chainfx::result cold = chainfx::run(false, true, 2, /*standing_works=*/false,
                                                  /*no_producer=*/true);
        std::printf("  cold start: firms %zu, timber site %d, chain_infeasible %lld\n", cold.firms,
                    cold.input_mine ? 1 : 0, cold.chain_infeasible);
        expect_true("cold start: no works stands, so timber is not wanted and steel is chain_infeasible",
                    cold.firms == 0 && !cold.input_mine && cold.chain_infeasible > 0 && cold.balanced);
    }
    {
        // THE KEPT ROSTER: enforce_chain_feasible_roster is order-free,
        // idempotent, and leaves nothing infeasible.
        const chainfx::roster_result r = chainfx::run_roster(/*s_loses_all=*/false);
        std::printf("  kept roster: call 1 re-decided %d unplaced %d holdless %d%s; call 2 re-decided "
                    "%d unplaced %d; audit infeasible held %d of %d\n",
                    r.first.processors_redecided, r.first.processors_unplaced, r.first.holdless,
                    r.first.seat_redrawn ? " (seat redrawn)" : "", r.second.processors_redecided,
                    r.second.processors_unplaced, r.audit.infeasible_held, r.audit.processors_held);
        expect_true("roster: P1 (no timber) and P3 (fed only by P1's stale recipe) unplaced; P2 kept",
                    r.p1_gone && r.p3_gone && r.p2_kept);
        expect_true("roster: a SECOND call changes nothing (re-decides 0, unplaces 0, same digest)",
                    r.second.processors_redecided == 0 && r.second.processors_unplaced == 0
                    && r.digest_first == r.digest_second);
        expect_true("roster: no held processor is left infeasible (audit)",
                    r.audit.infeasible_held == 0);
        expect_true("roster: the seat stays — S still holds a processor",
                    !r.first.seat_redrawn && r.first.seat == r.s);
        const chainfx::roster_result lost = chainfx::run_roster(/*s_loses_all=*/true);
        std::printf("  seat loses all: unplaced %d holdless %d, seat %u (S %u, T %u)%s\n",
                    lost.first.processors_unplaced, lost.first.holdless,
                    static_cast<unsigned>(lost.first.seat), static_cast<unsigned>(lost.s),
                    static_cast<unsigned>(lost.t), lost.first.seat_redrawn ? " redrawn" : "");
        expect_true("roster: a seated processing specialist left with no processor is re-seated "
                    "on the one that still qualifies",
                    lost.first.seat_redrawn && lost.first.seat == lost.t && lost.first.holdless == 1
                    && lost.audit.infeasible_held == 0);
    }
    {
        // BL-1233 — THE SIZED RULE.
        const sizedfx::one_result lean = sizedfx::run_one(1.5f);
        const sizedfx::one_result fits = sizedfx::run_one(0.5f);
        std::printf("  sized: mine output %.3f; need at t_idle %.3f -> kept %d; need %.3f -> kept %d\n",
                    lean.out, lean.need, lean.kept ? 1 : 0, fits.need, fits.kept ? 1 : 0);
        expect_true("sized: a processor whose t_idle need exceeds the spare reachable supply is unplaced",
                    lean.out > 0.0f && !lean.kept);
        expect_true("sized: the same processor is kept where the spare covers its t_idle need",
                    fits.out > 0.0f && fits.kept);
        const sizedfx::twin_result tw = sizedfx::run_twin();
        std::printf("  twin producers: out %.3f + %.3f, drawn %.3f once, spare %.3f (reach %d %d)\n",
                    tw.o1, tw.o2, tw.drawn, tw.spare, tw.reach1 ? 1 : 0, tw.reach2 ? 1 : 0);
        expect_true("sized: spare is read over the reach SET, each consumer's draw counted once "
                    "(two producer markets, each overdrawn alone)",
                    tw.reach1 && tw.reach2 && tw.drawn > tw.o1 && tw.drawn > tw.o2
                    && std::fabs(tw.spare - (tw.o1 + tw.o2 - tw.drawn)) < 1e-4f && tw.spare > 0.0f);
        const sizedfx::contend_result c = sizedfx::run_contend();
        std::printf("  contended roster: call 1 re-decided %d unplaced %d; call 2 re-decided %d "
                    "unplaced %d; steel works kept %d, tools kept %d (T P1 P2: %s)\n",
                    c.first.processors_redecided, c.first.processors_unplaced,
                    c.second.processors_redecided, c.second.processors_unplaced, c.works_kept,
                    c.tools_kept ? 1 : 0, c.held.c_str());
        expect_true("sized roster: of two works contending for one mine exactly one is kept, and the "
                    "tools works it feeds is re-admitted after it (no cascade loss)",
                    c.works_kept == 1 && c.tools_kept && c.first.processors_unplaced == 1);
        expect_true("sized roster: a SECOND call changes nothing (re-decides 0, unplaces 0, same digest)",
                    c.second.processors_redecided == 0 && c.second.processors_unplaced == 0
                    && c.d1 == c.d2);

        // REVIEW ROUND 2.
        const sizedfx::bound_result bd = sizedfx::run_bound();
        std::printf("  bounded charge: A %.3f (reaches C only) + B %.3f (reaches C and Q), Q draws %.3f; "
                    "spare at C %.3f (reach C->Q %d, B->Q %d, B->C %d)\n", bd.oa, bd.ob, bd.dq, bd.spare,
                    bd.c_q ? 1 : 0, bd.b_q ? 1 : 0, bd.b_c ? 1 : 0);
        expect_true("spare: a consumer is charged only what the set markets reaching it make "
                    "(A's output stays spare beside a hungry Q)",
                    !bd.c_q && bd.b_q && bd.b_c && bd.dq > bd.oa + bd.ob
                    && std::fabs(bd.spare - bd.oa) < 1e-4f);
        const sizedfx::quote_result qt = sizedfx::run_quote();
        std::printf("  quoted cost: landed %.3f, spare %.3f (reach P1->C %d, P2->C %d, P2->P1 %d)\n",
                    qt.landed, qt.spare, qt.p1_c ? 1 : 0, qt.p2_c ? 1 : 0, qt.p2_p1 ? 1 : 0);
        expect_true("spare: the quoted cost is the cheapest landed among producers WITH spare "
                    "(P1, fully drawn at 0.5, is passed over for P2 at 2.0)",
                    qt.p1_c && qt.p2_c && !qt.p2_p1 && qt.spare > 0.0f && qt.landed >= 2.0f);

        // RULING A (Ben, 2026-10-07).
        const sizedfx::book_result bk = sizedfx::run_book();
        std::printf("  ruling A book: set %.2f, after a second refusal %.2f; adds %.3f bare, %.3f with a "
                    "mine out of reach, %.3f at a centre out of reach, %.3f with a mine of %.3f in reach; "
                    "kept capped %d, not short %d, short %d\n",
                    bk.set_once, bk.twice, bk.bare, bk.out_of_reach, bk.far_centre, bk.in_reach,
                    bk.mine_out, bk.kept_capped ? 1 : 0, bk.kept_not_short ? 1 : 0, bk.kept_short ? 1 : 0);
        expect_true("ruling A: a refusal SETS its good's entry; a retry refused again counts it once",
                    bk.set_once == 2.0f && bk.twice == 2.0f && std::fabs(bk.bare - 2.0f) < 1e-5f);
        expect_true("ruling A: on a body glutted with timber, a timber firm out of reach of the "
                    "refused plant neither satisfies nor clears its want (timber stays short by it)",
                    std::fabs(bk.out_of_reach - 2.0f) < 1e-5f && bk.kept_short);
        expect_true("ruling A: a centre out of reach of the refused plant is not offered its want",
                    bk.far_centre == 0.0f);
        expect_true("ruling A: a firm in reach answers it by its output",
                    bk.mine_out > 0.0f && std::fabs(bk.in_reach - (2.0f - bk.mine_out)) < 1e-5f);
        expect_true("ruling A: withdrawn when the good is capped, or no longer short",
                    !bk.kept_capped && !bk.kept_not_short);
        const sizedfx::walk_result wk = sizedfx::run_walk(2);
        std::printf("  ruling A walk goods:");
        for (const charter_record& c : wk.rep.charters)
            std::printf(" %u", static_cast<unsigned>(c.good));
        for (const charter_unspent& u : wk.rep.unspent)
            std::printf(" | unspent %d x%lld", static_cast<int>(u.reason), static_cast<long long>(u.points));
        std::printf("\n");
        std::printf("  ruling A walk: firms %zu, timber charter #%d, steel charter #%d, chain_infeasible %lld%s\n",
                    wk.rep.firms.size(), wk.timber_after, wk.steel_at, wk.chain_infeasible,
                    wk.balanced ? "" : " [UNBALANCED]");
        expect_true("ruling A: the refused steel works' draw charters timber in reach, and the steel "
                    "firm places on the retry (erased on landing: no chain_infeasible left)",
                    wk.timber_after >= 0 && wk.steel_at > wk.timber_after && wk.chain_infeasible == 0
                    && wk.balanced);

        const sizedfx::refresh_result rf = sizedfx::run_refresh();
        std::printf("  refresh vs full build: %d of %d steps equal\n", rf.equal, rf.steps);
        expect_true("input_reach_refresh leaves the index a full build makes (add, remove, switch, "
                    "labour, decommission, staffing)",
                    rf.steps == 8 && rf.equal == rf.steps);
    }


    // --- 7. THE WATER DIG LADDER (BL-1197) --------------------------------------
    std::printf("\ncharter_refusal_probe — the water dig ladder (BL-1197)\n");
    {
        const waterfx::result wi = waterfx::run_walk(waterfx::mode::well_and_ice, 1);
        std::printf("  well and ice in the window: water firms %d, anchor at Well %d, at ice %d\n",
                    wi.water_firms, wi.anchor_at_well ? 1 : 0, wi.anchor_at_ice ? 1 : 0);
        expect_true("dig ladder: with a Well site and an ice deposit, the water firm stands on the Well",
                    wi.water_firms == 1 && wi.anchor_at_well && !wi.anchor_at_ice && wi.balanced);
        const waterfx::result io = waterfx::run_walk(waterfx::mode::ice_only, 1);
        std::printf("  ice only: water firms %d, anchor at ice %d\n", io.water_firms,
                    io.anchor_at_ice ? 1 : 0);
        expect_true("dig ladder: with no Well site, the water firm stands on the ice deposit",
                    io.water_firms == 1 && io.anchor_at_ice && io.balanced);
        const waterfx::result fi = waterfx::run_walk(waterfx::mode::far_ice_only, 1);
        std::printf("  no water ground in the window: firms %zu, water %d, stone %d, water site %d\n",
                    fi.firms, fi.water_firms, fi.stone_firms, fi.any_water_site ? 1 : 0);
        expect_true("dig ladder: no water ground in the window places nothing for water (no fallback)",
                    fi.water_firms == 0 && !fi.any_water_site && fi.balanced);
        expect_true("dig ladder: the same firm takes the next good (stone)",
                    fi.firms == 1 && fi.stone_firms == 1);
    }
    {
        const waterfx::result nw = waterfx::run_pass6(waterfx::mode::no_water);
        std::printf("  Pass 6, no water ground on the body: firms %zu, water %d, stone %d, water site %d\n",
                    nw.firms, nw.water_firms, nw.stone_firms, nw.any_water_site ? 1 : 0);
        expect_true("Pass 6: water misses once per nation, is masked, and stone still charters",
                    nw.water_firms == 0 && !nw.any_water_site && nw.stone_firms >= 1);
        const waterfx::result pw = waterfx::run_pass6(waterfx::mode::well_and_ice);
        std::printf("  Pass 6, a Well site and ice on the body: water firms %d, anchor at Well %d\n",
                    pw.water_firms, pw.anchor_at_well ? 1 : 0);
        expect_true("Pass 6: a water-gap firm digs water, the Well tier first",
                    pw.water_firms >= 1 && pw.anchor_at_well);
    }

    // --- 8. THE PRODUCE DIG LADDER (BL-1208) ------------------------------------
    std::printf("\ncharter_refusal_probe — the produce dig ladder (BL-1208)\n");
    {
        const producefx::result dc = producefx::run_walk(producefx::mode::deposit_and_coast, 1);
        std::printf("  deposit and coast in the window: produce firms %d, at deposit %d, at Wharf %d\n",
                    dc.produce_firms, dc.anchor_at_deposit ? 1 : 0, dc.anchor_at_wharf ? 1 : 0);
        expect_true("produce ladder: with a produce deposit and a coast, the firm stands on the deposit",
                    dc.produce_firms == 1 && dc.anchor_at_deposit && !dc.anchor_at_wharf && dc.balanced);
        const producefx::result co = producefx::run_walk(producefx::mode::coast_only, 1);
        std::printf("  coast, no produce deposit: produce firms %d, at Wharf %d\n",
                    co.produce_firms, co.anchor_at_wharf ? 1 : 0);
        expect_true("produce ladder: a coastal body with no produce deposit gets a Wharf for the gap",
                    co.produce_firms == 1 && co.anchor_at_wharf && co.balanced);
        const producefx::result fc = producefx::run_walk(producefx::mode::far_coast_only, 1);
        std::printf("  no produce ground in the window: firms %zu, produce %d, stone %d, produce site %d\n",
                    fc.firms, fc.produce_firms, fc.stone_firms, fc.any_produce_site ? 1 : 0);
        expect_true("produce ladder: no produce ground in the window places nothing for produce (no fallback)",
                    fc.produce_firms == 0 && !fc.any_produce_site && fc.balanced);
        expect_true("produce ladder: the same firm takes the next good (stone)",
                    fc.firms == 1 && fc.stone_firms == 1);
    }
    {
        const producefx::result np = producefx::run_pass6(producefx::mode::no_produce);
        std::printf("  Pass 6, no produce ground on the body: firms %zu, produce %d, stone %d, produce site %d\n",
                    np.firms, np.produce_firms, np.stone_firms, np.any_produce_site ? 1 : 0);
        expect_true("Pass 6: produce misses once per nation, is masked, and stone still charters",
                    np.produce_firms == 0 && !np.any_produce_site && np.stone_firms >= 1);
        const producefx::result pc = producefx::run_pass6(producefx::mode::coast_only);
        std::printf("  Pass 6, a coast and no produce deposit: produce firms %d, at Wharf %d\n",
                    pc.produce_firms, pc.anchor_at_wharf ? 1 : 0);
        expect_true("Pass 6: a produce-gap firm on a depositless coast stands on a Wharf",
                    pc.produce_firms >= 1 && pc.anchor_at_wharf);
    }

    std::printf("\n%s (%d failing)\n", g_fail == 0 ? "ALL PASS" : "FAILED", g_fail);
    return g_fail == 0 ? 0 : 1;
}
