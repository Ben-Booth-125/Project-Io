// haulage_measure — what does it actually COST to serve a neighbouring market?
// (BL-442 step 2)
//
// WHY THIS EXISTS. BL-442 step 2 asks for a widened scarcity ceiling, and Ben's
// requirement (2026-08-17) makes that ceiling a DERIVED quantity rather than a
// round number: scarcity must price high enough to cross the margin for nearby
// markets, so inter-market trading exists from day 1 rather than as a late-game
// unlock. "Cross the margin" is an arithmetic claim about a number nobody had
// measured — the per-unit haulage the supply layer already charges between two
// adjacent markets in the REAL generated world.
//
// A ceiling picked before that number is known is a guess wearing a derivation's
// clothes. This harness measures the number, so the multiplier in
// scripts/economy.lua can be re-derived rather than trusted.
//
// WHAT IT MEASURES. Exactly the cost `dispatch_convoys` charges
// (supply_system.cpp): for an intra-body leg,
//
//     haulage_per_unit = logistics_cost(mode) * path.cost * (1 - node_discount)
//
// where path.cost is the terrain-weighted A* cost (logistics.hpp) between the
// two markets' centre tiles, mode is sea when the path crosses ocean, and the
// node discount is the BL-148/149 city/hub discount along that same path. It
// walks every market and finds its NEAREST market neighbour by that cost — the
// "nearby market" of the requirement — and reports the distribution.
//
// THE DERIVATION IT FEEDS. A seller in market A can serve a scarce market B only
// if the scarcity price clears the base price plus the haul:
//
//     ceil_mult * base_price  >  base_price + haulage_per_unit
//     ceil_mult               >  1 + haulage_per_unit / base_price
//
// The BINDING case is the cheapest good over the dearest haul, so the harness
// reports the required ceil_mult against the minimum authored base_price as well
// as against the median. It also reports the SECOND constraint Ben named: the
// ceiling must be wide enough that a scarce cheap resource outprices an abundant
// dear one, i.e. ceil_mult > max_base_price / min_base_price.
//
// Report-only. This harness asserts nothing about the band's value — the band is
// authored data and the assertions on it live in price_band_harness.cpp. Its one
// assertion is a vacuity guard: that it measured a world with markets in it.
//
// Run: .\build\haulage_measure.exe [seeds] [ticks] [--per-tick]
//
// --per-tick (BL-978) prints the dispatch count of EVERY tick, pooled over the
// seeds, after the totals. It is the instrument that sized the winner's
// validation run: the settle that hands play its opening position is a fixed
// number of econ ticks on the searched landscape, and "how many" is answered
// by reading where this series stops moving, not by picking a round number.
//
// THE DEFAULT SECTIONS COUNT DISPATCHES, AND A DISPATCH IS NOT TRADE. The
// "intra-body, market -> market" line below is the figure MARKETS.md recorded
// as 1,677 of 2,146 — and under (corp, body) pools every one of those cargoes
// returned to the pool it left and sold at the seller's home market. The
// reading that says what actually reached a buyer is the --far-trade mode:
//
// Run: .\build_gen\verify\haulage_measure.exe --far-trade [--seeds N]
//        [--first-seed S] [--years N] [--epoch Y] [--fast] [--tail N]
//
// THE FAR TRADE READING (BL-1006). It lands before per-market pools (BL-1003),
// sea legs (BL-1004) and trade that reaches for price (BL-995) so each of them
// can be judged against a number rather than argued. Taken on the world the app
// builds, over a seed spread, in two windows of four quarterly dispatches — the
// first year of play from the opening position ("at the epoch"), and the year
// after N years of play — it prints:
//
//   (a) volume DELIVERED and SOLD at its destination market, where that market
//       is not the source pool's own clearing market. Not a dispatch count.
//   (b) the share of (a) whose destination is not the seller's nearest market.
//   (c) per delivered good, the price gap (destination - home) at dispatch and
//       at arrival, beside the haul paid per unit, so a gap play erases shows.
//
// DEFINITIONS, stated because each is a choice a reader should be able to see:
//   * HOME is where the source pool clears — since per-market pools (BL-1003)
//     the pool IS the convoy's source market's, so home is `source_market`.
//   * The SELL MARKET of an arrival is where the pool the cargo lands in clears
//     on the next clear: (corp, destination MARKET), so a same-body haul sells
//     at its destination. Under the retired (corp, body) pools it landed back in
//     the pool it left and same-body (a) read zero. `arrival_sell_market`.
//   * SOLD is read from the exchange record of the first clear after arrival
//     (rows whose seller is the corp, at the sell market, in the cargo's good),
//     attributed CARGO-FIRST: min(cargo, what the corp sold there), shared across
//     that clear's arrivals in convoy-id order. Goods are fungible inside a pool,
//     so this is an UPPER bound on the cargo's own sale, and it says so.
//   * The seller's NEAREST market is the market other than home that
//     `price_convoy_leg` prices cheapest per unit from the seller's dispatch
//     origin at the dispatch tick — the dispatcher's own cost, with its node
//     discount and its port and launchpad gates, so "nearest" means nearest
//     REACHABLE, ties to the lower id.
//
// THE WORLD IS THE APP'S (BL-1007 parity): scripts/world_gen.lua parsed into the
// generation config, scripts/works.lua loaded and passed, the registry's era set
// from the epoch, full pre-history unless --fast (which the output then names),
// the landscape-search WINNER applied (apply_shipped_landscape), the app's
// `validation_ticks` run in app::step_economy's order, then play. Nobody is
// seated: every corp is scorer-driven throughout, campaign_lapse's reading.
//
// Report-only, like the rest of this harness: no bar on the numbers it exists to
// discover. Its guards are non-vacuity guards only — the data layer was parsed,
// each window saw a dispatch and a delivery, no clear pushed more exchange rows
// than the ring retains (which would under-read every sale), and every credited
// arrival raised the (corp, destination market) pool by exactly its cargo, so the
// pool model the definitions above lean on is checked against the tree rather
// than assumed. If the pool model moves again, that guard is the one that goes red.

#include "scripting/lua_state.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/corporation_generation.hpp"
#include "world/hard_coded_world.hpp"
#include "world/budget_system.hpp"
#include "world/economy_system.hpp"
#include "world/era_band.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/nation_step.hpp"
#include "world/resource_names.hpp"
#include "world/supply_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/tech_gate.hpp"
#include "world/works_roster.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"
#include "harness_params.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok)
        ++g_failures;
}

double quantile(std::vector<double> v, double q)
{
    if (v.empty())
        return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t i = static_cast<std::size_t>(q * static_cast<double>(v.size() - 1) + 0.5);
    return v[std::min(i, v.size() - 1)];
}

// ===========================================================================
// --far-trade — the far trade reading (BL-1006). The file header states what it
// measures and every definition it uses; this block is the mechanics.
// ===========================================================================

/// app.hpp `app::validation_ticks` — the opening position's settle. Mirrored,
/// not shared (app.hpp drags the whole UI in): change both.
constexpr int k_validation_ticks = 12;
/// sim_loop::econ_tick_days — the day tick one quarter advances in play.
constexpr int k_econ_tick_days = 90;

struct far_options
{
    int           seeds      = 3;
    std::uint32_t first_seed = 0;
    int           years      = 1;    ///< The second window opens after 4 x years quarters of play.
    std::int64_t  epoch      = 1960; ///< The live product's campaign epoch (CONCEPT.md § Eras).
    bool          fast       = false;///< Zero the pre-history: NOT the app's world; the output says so.
    int           tail_cap   = 16;   ///< Extra quarters allowed for window cargo to arrive and sell.
};

enum class cargo_fate : std::uint8_t
{
    in_flight,     ///< Dispatched, not yet arrived.
    awaiting_sale, ///< Credited on arrival; its first clear has not run yet.
    settled,       ///< Its first clear after arrival has been read.
    cut,           ///< Intercepted (BL-458) — never delivered.
    lost,          ///< Arrived at a market that no longer exists — never credited.
};

/// One convoy followed from dispatch to its first clear after arrival.
struct cargo_track
{
    std::uint32_t id            = 0;
    int           window        = -1; ///< 0 at the epoch, 1 after N years, -1 neither.
    entity_id     corp          = null_entity;
    convoy_mode   mode          = convoy_mode::land;
    std::size_t   r             = 0;
    double        qty           = 0.0;
    double        haul_per_unit = 0.0;
    entity_id     src_body      = null_entity;
    entity_id     home          = null_entity; ///< The source pool's clearing market.
    entity_id     dest_market   = null_entity;
    entity_id     dest_body     = null_entity;
    entity_id     nearest       = null_entity; ///< Seller's cheapest-reached market other than home.
    double        p_home_dispatch = 0.0;
    double        p_dest_dispatch = 0.0;
    cargo_fate    fate          = cargo_fate::in_flight;
    double        p_home_arrival = 0.0;
    double        p_dest_arrival = 0.0;
    entity_id     sell_market   = null_entity;
    double        sold          = 0.0;
    /// BL-1071: a MARKET's own shelf export (owner null_entity). It lands on the
    /// destination SHELF, not in a pool, and no seller lists it — so it is read
    /// on its own line and kept out of every corporation figure below.
    bool          market_export = false;
};

/// Where an ARRIVED cargo sells: the market of the pool it was credited to.
/// Since BL-1003 (per-market pools) `credit_arrived_convoys` credits
/// (corp, DESTINATION MARKET), so the answer is the destination itself — a
/// same-body haul sells where it was delivered, not back at home.
entity_id arrival_sell_market(const world& w, const cargo_track& t)
{
    (void)w;
    return t.dest_market;
}

/// The seller's nearest market at dispatch: the market other than `home` that
/// the dispatcher's own pricing reaches cheapest per unit from the source pool
/// `src_key` (BL-1003: a market, or a market-less body).
/// Priced at one unit — the leg cost is linear in quantity, so the order is the
/// order at any cargo size. Ties to the lower id (ascending walk, strict <).
entity_id sellers_nearest_market(world& w, const recipe_registry& reg,
                                 const logistics_nodes& nodes,
                                 const std::vector<entity_id>& market_ids, entity_id corp,
                                 entity_id src_key, entity_id home, std::size_t r)
{
    entity_id best      = null_entity;
    float     best_cost = std::numeric_limits<float>::max();
    for (const entity_id mid : market_ids)
    {
        if (mid == home)
            continue;
        const convoy_leg leg = price_convoy_leg(w, reg, nodes, corp, src_key, mid, r, 1.0f,
                                                reg.logistics_cost(convoy_mode::space));
        if (!leg.viable)
            continue;
        if (leg.cost < best_cost)
        {
            best      = mid;
            best_cost = leg.cost;
        }
    }
    return best;
}

double price_at(const world& w, entity_id market, std::size_t r)
{
    const auto it = w.markets.find(market);
    return it != w.markets.end() ? static_cast<double>(it->second.price[r]) : 0.0;
}

/// One window's reading, summed from its tracks. Pooled across seeds by `add`.
struct far_tally
{
    long   n_disp = 0;          double v_disp = 0.0;
    long   n_disp_sb_other = 0; double v_disp_sb_other = 0.0; ///< same body, destination != home
    long   n_disp_sb_own = 0;   double v_disp_sb_own = 0.0;   ///< same body, destination == home
    long   n_disp_ib = 0;       double v_disp_ib = 0.0;       ///< inter-body (space lane)
    long   n_del_sb = 0;        double v_del_sb = 0.0;
    long   n_del_ib = 0;        double v_del_ib = 0.0;
    long   n_cut = 0;           double v_cut = 0.0;
    long   n_open = 0;          double v_open = 0.0;          ///< in flight / unsold / lost at read end
    double v_a_sb = 0.0;        ///< (a) on a same-body haul
    double v_a_ib = 0.0;        ///< (a) on an inter-body haul
    double v_sold_home = 0.0;   ///< sold, but at the seller's own home market
    double v_sold_third = 0.0;  ///< sold at a market that is neither home nor the destination
    double v_unsold = 0.0;      ///< delivered, not sold on its first clear
    long   n_sb_del = 0;        ///< same-body deliveries...
    long   n_sb_del_home = 0;   ///< ...whose sell market is home (the pool they left)
    double v_a_skipped = 0.0;   ///< (b) numerator: (a) volume whose destination != nearest
    double v_del_skipped = 0.0; ///< context: delivered-away volume whose destination != nearest
    double v_del_away = 0.0;    ///< delivered volume bound for a market other than home
    double v_del_total = 0.0;
    // BL-1071: market exports (a market's own shelf), their own line.
    long   n_mx = 0;            double v_mx = 0.0;            ///< dispatched
    long   n_mx_del = 0;        double v_mx_del = 0.0;        ///< landed on a destination shelf
    double v_mx_open = 0.0;     ///< in flight / cut / lost at read end
    double mx_haul_w = 0.0;     ///< qty x haul/unit, over the dispatched
    double mx_gap_w  = 0.0;     ///< qty x (dest price - source price) at dispatch

    struct good_row
    {
        long   n = 0;
        double delivered = 0.0, a = 0.0;
        double gap_vol = 0.0, gap_dispatch_w = 0.0, gap_arrival_w = 0.0;
        double haul_w = 0.0;
    };
    std::array<good_row, resource_count> goods{};

    void add(const far_tally& o)
    {
        n_disp += o.n_disp; v_disp += o.v_disp;
        n_disp_sb_other += o.n_disp_sb_other; v_disp_sb_other += o.v_disp_sb_other;
        n_disp_sb_own += o.n_disp_sb_own; v_disp_sb_own += o.v_disp_sb_own;
        n_disp_ib += o.n_disp_ib; v_disp_ib += o.v_disp_ib;
        n_del_sb += o.n_del_sb; v_del_sb += o.v_del_sb;
        n_del_ib += o.n_del_ib; v_del_ib += o.v_del_ib;
        n_cut += o.n_cut; v_cut += o.v_cut;
        n_open += o.n_open; v_open += o.v_open;
        v_a_sb += o.v_a_sb; v_a_ib += o.v_a_ib;
        v_sold_home += o.v_sold_home; v_sold_third += o.v_sold_third; v_unsold += o.v_unsold;
        n_sb_del += o.n_sb_del; n_sb_del_home += o.n_sb_del_home;
        v_a_skipped += o.v_a_skipped; v_del_skipped += o.v_del_skipped;
        v_del_away += o.v_del_away; v_del_total += o.v_del_total;
        n_mx += o.n_mx; v_mx += o.v_mx; n_mx_del += o.n_mx_del; v_mx_del += o.v_mx_del;
        v_mx_open += o.v_mx_open; mx_haul_w += o.mx_haul_w; mx_gap_w += o.mx_gap_w;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            good_row& g = goods[r];
            const good_row& h = o.goods[r];
            g.n += h.n; g.delivered += h.delivered; g.a += h.a;
            g.gap_vol += h.gap_vol; g.gap_dispatch_w += h.gap_dispatch_w;
            g.gap_arrival_w += h.gap_arrival_w; g.haul_w += h.haul_w;
        }
    }
};

far_tally tally_window(const std::vector<cargo_track>& tracks, int window)
{
    far_tally t;
    for (const cargo_track& c : tracks)
    {
        if (c.window != window)
            continue;
        if (c.market_export)
        {
            ++t.n_mx;
            t.v_mx      += c.qty;
            t.mx_haul_w += c.qty * c.haul_per_unit;
            t.mx_gap_w  += c.qty * (c.p_dest_dispatch - c.p_home_dispatch);
            if (c.fate == cargo_fate::settled) { ++t.n_mx_del; t.v_mx_del += c.qty; }
            else                               t.v_mx_open += c.qty;
            continue;
        }
        const bool same_body = c.mode != convoy_mode::space;
        ++t.n_disp;
        t.v_disp += c.qty;
        if (!same_body)                  { ++t.n_disp_ib;       t.v_disp_ib += c.qty; }
        else if (c.dest_market == c.home){ ++t.n_disp_sb_own;   t.v_disp_sb_own += c.qty; }
        else                             { ++t.n_disp_sb_other; t.v_disp_sb_other += c.qty; }

        if (c.fate == cargo_fate::cut)
        {
            ++t.n_cut;
            t.v_cut += c.qty;
            continue;
        }
        if (c.fate != cargo_fate::settled)
        {
            ++t.n_open;
            t.v_open += c.qty;
            continue;
        }

        // Delivered, and its first clear has been read.
        if (same_body) { ++t.n_del_sb; t.v_del_sb += c.qty; }
        else           { ++t.n_del_ib; t.v_del_ib += c.qty; }
        t.v_del_total += c.qty;
        if (c.dest_market != c.home)
        {
            // Bound for a market other than home: the population (b) is a share
            // of, before the sale is asked about. A haul to home skips nothing.
            t.v_del_away += c.qty;
            if (c.dest_market != c.nearest)
                t.v_del_skipped += c.qty;
        }
        if (same_body)
        {
            ++t.n_sb_del;
            if (c.sell_market == c.home)
                ++t.n_sb_del_home;
        }

        far_tally::good_row& g = t.goods[c.r];
        ++g.n;
        g.delivered += c.qty;
        g.haul_w    += c.qty * c.haul_per_unit;
        if (c.home != null_entity)
        {
            g.gap_vol        += c.qty;
            g.gap_dispatch_w += c.qty * (c.p_dest_dispatch - c.p_home_dispatch);
            g.gap_arrival_w  += c.qty * (c.p_dest_arrival - c.p_home_arrival);
        }

        const double unsold = c.qty - c.sold;
        t.v_unsold += unsold;
        if (c.sold <= 0.0)
            continue;
        if (c.sell_market == c.home)
            t.v_sold_home += c.sold;
        else if (c.sell_market == c.dest_market)
        {
            // (a): delivered AND sold at its destination, which is not home.
            (same_body ? t.v_a_sb : t.v_a_ib) += c.sold;
            g.a += c.sold;
            if (c.dest_market != c.nearest)
                t.v_a_skipped += c.sold;
        }
        else
            t.v_sold_third += c.sold;
    }
    return t;
}

void print_share(const char* label, double num, double den)
{
    if (den > 0.0)
        std::printf("%s%6.1f%%  (%.1f of %.1f units)\n", label, 100.0 * num / den, num, den);
    else
        std::printf("%s   n/a  (no volume to take a share of)\n", label);
}

void print_window(const far_tally& t, const char* title, bool per_good)
{
    std::printf("\n--- %s ---\n", title);
    std::printf("  dispatched                          %6ld convoys  %12.1f units\n", t.n_disp, t.v_disp);
    std::printf("    same body, to another market      %6ld          %12.1f\n", t.n_disp_sb_other, t.v_disp_sb_other);
    std::printf("    same body, to its own home market %6ld          %12.1f\n", t.n_disp_sb_own, t.v_disp_sb_own);
    std::printf("    inter-body (space lane)           %6ld          %12.1f\n", t.n_disp_ib, t.v_disp_ib);
    std::printf("  delivered (credited on arrival)     %6ld          %12.1f\n", t.n_del_sb + t.n_del_ib, t.v_del_total);
    std::printf("    same body                         %6ld          %12.1f\n", t.n_del_sb, t.v_del_sb);
    std::printf("    inter-body                        %6ld          %12.1f\n", t.n_del_ib, t.v_del_ib);
    std::printf("  cut by interdiction                 %6ld          %12.1f\n", t.n_cut, t.v_cut);
    std::printf("  in flight, unread or lost at end    %6ld          %12.1f\n", t.n_open, t.v_open);
    std::printf("  delivered volume on its first clear (cargo-first attribution, an UPPER bound):\n");
    std::printf("    (a) SOLD AT ITS DESTINATION, not home       %12.1f\n", t.v_a_sb + t.v_a_ib);
    std::printf("        ... same body                           %12.1f\n", t.v_a_sb);
    std::printf("        ... inter-body                          %12.1f\n", t.v_a_ib);
    std::printf("    sold back at the seller's home market       %12.1f\n", t.v_sold_home);
    std::printf("    sold at a third market                      %12.1f\n", t.v_sold_third);
    std::printf("    not sold on that clear                      %12.1f\n", t.v_unsold);
    print_share("  (b) share of (a) whose destination is not the seller's nearest market: ",
                t.v_a_skipped, t.v_a_sb + t.v_a_ib);
    print_share("      context - the same share over DELIVERED volume bound away from home: ",
                t.v_del_skipped, t.v_del_away);
    std::printf("  same-body deliveries whose arrival pool clears at home: %ld of %ld\n",
                t.n_sb_del_home, t.n_sb_del);
    // BL-1071: a market's own shelf, exported — its own line, in none of the above.
    std::printf("  MARKET EXPORTS (a market's own shelf, owner none; BL-1071):\n");
    std::printf("    dispatched                          %6ld convoys  %12.1f units\n", t.n_mx, t.v_mx);
    std::printf("    landed on the destination shelf     %6ld          %12.1f\n", t.n_mx_del, t.v_mx_del);
    std::printf("    in flight / cut / lost at end                     %12.1f\n", t.v_mx_open);
    if (t.v_mx > 0.0)
        std::printf("    volume-weighted: price gap dest - source %.3f, haul/unit %.3f\n",
                    t.mx_gap_w / t.v_mx, t.mx_haul_w / t.v_mx);
    if (!per_good)
        return;
    std::printf("  (c) price gap per delivered good, destination - home, volume-weighted:\n");
    std::printf("      %-24s %7s %11s %11s %13s %12s %10s\n", "good", "convoys", "delivered",
                "(a) sold", "gap@dispatch", "gap@arrival", "haul/unit");
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const far_tally::good_row& g = t.goods[r];
        if (g.n == 0)
            continue;
        const std::string name = resource_names::name_of(static_cast<resource_type>(r));
        if (g.gap_vol > 0.0)
            std::printf("      %-24s %7ld %11.1f %11.1f %13.3f %12.3f %10.3f\n", name.c_str(), g.n,
                        g.delivered, g.a, g.gap_dispatch_w / g.gap_vol,
                        g.gap_arrival_w / g.gap_vol, g.haul_w / g.delivered);
        else
            std::printf("      %-24s %7ld %11.1f %11.1f %13s %12s %10.3f\n", name.c_str(), g.n,
                        g.delivered, g.a, "(no home)", "(no home)", g.haul_w / g.delivered);
    }
}

/// Everything one seed's run observed.
struct far_seed_result
{
    std::array<far_tally, 2> windows{};
    long ring_overflows = 0; ///< clears that pushed more exchange rows than the ring retains
    long arrivals_checked = 0;      ///< credited arrivals whose pool gain was checked
    long pool_model_mismatches = 0; ///< (corp, destination market) pools that did not gain their cargo
    int  priced         = 0;
    int  markets        = 0;
    int  play_quarters  = 0;
};

far_seed_result run_far_seed(const far_options& o, std::uint32_t seed, const recipe_registry& reg,
                             const world_gen_config& gen_cfg, const works_registry& works)
{
    far_seed_result out;
    for (std::size_t i = 0; i < resource_count; ++i)
        if (gen_cfg.kepler_base_price[i] > 0.0f)
            ++out.priced;

    world_params p;
    p.seed       = seed;
    p.epoch_year = o.epoch;
    if (o.fast)
        p = no_prehistory(p);
    world w = make_hard_coded_world(p, nullptr, gen_cfg, nullptr, &works);
    print_shipped_landscape(apply_shipped_landscape(w, reg, seed));
    out.markets = static_cast<int>(w.markets.size());

    std::vector<entity_id> market_ids;
    for (const auto& [mid, mc] : w.markets)
        market_ids.push_back(mid);
    std::sort(market_ids.begin(), market_ids.end());

    std::vector<cargo_track> tracks;             // ascending convoy id, as dispatched
    std::unordered_map<std::uint32_t, std::size_t> by_id; // lookup only — never walked
    std::uint32_t max_seen = 0;
    bool have_seen = false;

    // Adopt every convoy this tick has appended since the last look. Prices are
    // read here, BEFORE clear_markets, so they are the prices dispatch acted on.
    auto adopt_new = [&](int window) {
        logistics_nodes nodes;
        bool nodes_built = false;
        std::uint32_t max_now = max_seen;
        for (const convoy_component& cv : w.convoys)
        {
            if (have_seen && cv.id <= max_seen)
                continue;
            max_now = std::max(max_now, cv.id);
            cargo_track t;
            t.id            = cv.id;
            t.window        = window;
            t.corp          = cv.corp;
            t.mode          = cv.mode;
            t.r             = static_cast<std::size_t>(cv.cargo_resource);
            t.qty           = static_cast<double>(cv.cargo_qty);
            t.haul_per_unit = cv.cargo_qty > 0.0f
                                  ? static_cast<double>(cv.cost_paid) / static_cast<double>(cv.cargo_qty)
                                  : 0.0;
            t.dest_market   = cv.dest_market;
            t.dest_body     = body_of_market(w, cv.dest_market);
            // A land or sea leg never leaves its body; a space leg names its
            // source market (auto-dispatch: the body's lowest-id market; the
            // verb: the market the command named).
            t.src_body = (cv.mode != convoy_mode::space) ? t.dest_body
                                                         : body_of_market(w, cv.source_market);
            // BL-1003: the source pool IS the source market's pool, so home is
            // the convoy's own source market (null for a market-less body).
            t.home            = cv.source_market;
            t.p_home_dispatch = price_at(w, t.home, t.r);
            t.p_dest_dispatch = price_at(w, t.dest_market, t.r);
            t.market_export   = (cv.corp == null_entity); // BL-1071
            if (window >= 0 && t.src_body != null_entity && !t.market_export)
            {
                if (!nodes_built)
                {
                    nodes       = collect_logistics_nodes(w);
                    nodes_built = true;
                }
                t.nearest = sellers_nearest_market(w, reg, nodes, market_ids, t.corp,
                                                   t.home != null_entity ? t.home : t.src_body,
                                                   t.home, t.r);
            }
            by_id[t.id] = tracks.size();
            tracks.push_back(t);
        }
        if (!w.convoys.empty() || have_seen)
        {
            max_seen  = max_now;
            have_seen = have_seen || !w.convoys.empty();
        }
    };

    // One econ tick in app::step_economy's order (BL-1066/BL-995: advance ->
    // credit arrivals -> economy -> dispatch -> clear -> budget -> nation ->
    // tech -> exits), spectating (nobody seated), with the reading's three
    // looks placed at the seams where each fact exists.
    auto step = [&](int econ_tick, int day_tick, int window) {
        w.current_econ_tick = econ_tick;
        w.current_day_tick  = day_tick;
        lp_pool_map lp;
        advance_convoys(w);

        // LOOK 2 — what arrives, what is cut. Prices now are the last clear's:
        // the prices standing when the cargo lands (arrivals are credited at the
        // top of the tick, before the economy runs — app::step_economy's order).
        std::vector<std::uint32_t> arrived;
        // The pool model the reading ASSUMES — an arrival credits (corp,
        // DESTINATION MARKET), BL-1003 — is CHECKED here, not trusted: each such
        // pool is read before and after the credit, and must have gained exactly
        // the cargo that landed in it. If the tree moves again, this goes red
        // instead of the reading quietly describing a world that is gone.
        using pool_key = std::tuple<entity_id, entity_id, std::size_t>;
        std::map<pool_key, std::pair<double, double>> pool_watch; // before, expected gain
        auto pool_qty = [&](const pool_key& k) {
            // BL-1071: a market export (owner null) credits the destination
            // SHELF, and nothing else writes a shelf during the credit, so the
            // same before/after check reads the shelf for it.
            if (std::get<0>(k) == null_entity)
            {
                const auto mit = w.markets.find(std::get<1>(k));
                return mit != w.markets.end()
                           ? static_cast<double>(mit->second.inventory[std::get<2>(k)])
                           : 0.0;
            }
            const auto pit = w.corp_market_pools.find({std::get<0>(k), std::get<1>(k)});
            return pit != w.corp_market_pools.end()
                       ? static_cast<double>(pit->second.quantities[std::get<2>(k)])
                       : 0.0;
        };
        for (const convoy_component& cv : w.convoys)
            if (cv.arrived)
            {
                arrived.push_back(cv.id);
                const pool_key k{cv.corp, cv.dest_market,
                                 static_cast<std::size_t>(cv.cargo_resource)};
                if (pool_watch.find(k) == pool_watch.end())
                    pool_watch[k] = {pool_qty(k), 0.0};
            }
        std::vector<interception_record> cuts;
        credit_arrived_convoys(w, day_tick, &cuts);
        std::unordered_set<std::uint32_t> cut_ids;
        for (const interception_record& ir : cuts)
        {
            cut_ids.insert(ir.convoy_id);
            if (const auto it = by_id.find(ir.convoy_id); it != by_id.end())
                tracks[it->second].fate = cargo_fate::cut;
        }
        for (const std::uint32_t id : arrived)
        {
            const auto it = by_id.find(id);
            if (it == by_id.end() || cut_ids.count(id) != 0)
                continue;
            cargo_track& t = tracks[it->second];
            if (w.markets.find(t.dest_market) == w.markets.end())
            {
                t.fate = cargo_fate::lost;
                continue;
            }
            // A market export lands on a shelf; no seller lists it, so it has
            // no first clear to read: it is settled on landing (BL-1071).
            t.fate           = t.market_export ? cargo_fate::settled : cargo_fate::awaiting_sale;
            t.p_home_arrival = price_at(w, t.home, t.r);
            t.p_dest_arrival = price_at(w, t.dest_market, t.r);
            pool_watch[{t.corp, t.dest_market, t.r}].second += t.qty;
            ++out.arrivals_checked;
        }
        for (const auto& [k, before_expected] : pool_watch)
        {
            const double gained = pool_qty(k) - before_expected.first;
            const double want   = before_expected.second;
            if (std::fabs(gained - want) > 1e-3 * std::max(1.0, std::fabs(want)))
                ++out.pool_model_mismatches;
        }

        economy_report rep = run_economy_step(w, reg, /*spectating=*/true, &lp);
        adopt_new(window); // the scorer's own dispatch_convoy verbs (BL-600)
        dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                         reg.logistics_cost(convoy_mode::space), &lp);
        adopt_new(window);

        const std::size_t rows_before = w.exchanges.total;
        auto flows = clear_markets(w, reg, rep);
        const std::size_t pushed = w.exchanges.total - rows_before;

        // LOOK 1 — this tick's arrivals meet their first clear (BL-995 order:
        // they were credited at the top of this same tick).
        {
            if (pushed > w.exchanges.size())
                ++out.ring_overflows;
            std::map<std::tuple<entity_id, entity_id, std::size_t>, double> sold_by;
            const std::size_t n = std::min(pushed, w.exchanges.size());
            for (std::size_t i = w.exchanges.size() - n; i < w.exchanges.size(); ++i)
            {
                const exchange_record& e = w.exchanges.oldest_first(i);
                if (e.seller == null_entity)
                    continue;
                sold_by[{e.seller, e.market, static_cast<std::size_t>(e.resource)}] +=
                    static_cast<double>(e.quantity);
            }
            for (cargo_track& t : tracks) // ascending id: the attribution order
            {
                if (t.fate != cargo_fate::awaiting_sale)
                    continue;
                t.sell_market = arrival_sell_market(w, t);
                const auto it = sold_by.find({t.corp, t.sell_market, t.r});
                if (it != sold_by.end())
                {
                    t.sold = std::min(t.qty, it->second);
                    it->second -= t.sold;
                }
                t.fate = cargo_fate::settled;
            }
        }

        apply_budget(w, reg, flows, rep.workforce_contention, &rep.budgets, &rep.buildings,
                     &rep.building_labour);
        run_nation_step(w, reg, rep, econ_tick);
        advance_tech_gates(w);

        run_firm_exits(w, reg.firm_exit(), &rep.firm_exits);

        // A convoy that left the world without arriving (a wound-up firm's
        // cargo) will never deliver: close it as lost rather than wait for it.
        std::unordered_set<std::uint32_t> live;
        for (const convoy_component& cv : w.convoys)
            live.insert(cv.id);
        for (cargo_track& t : tracks)
            if (t.fate == cargo_fate::in_flight && live.count(t.id) == 0)
                t.fate = cargo_fate::lost;
    };

    int econ = 0;
    for (int v = 0; v < k_validation_ticks; ++v)
        step(econ++, 0, -1);

    const int second_open  = 4 * o.years;
    const int second_close = second_open + 4;
    auto window_of = [&](int q) {
        if (q < 4) return 0;
        if (q >= second_open && q < second_close) return 1;
        return -1;
    };
    int q = 0;
    for (; q < second_close; ++q)
        step(econ++, (q + 1) * k_econ_tick_days, window_of(q));

    auto pending = [&] {
        for (const cargo_track& t : tracks)
            if (t.window >= 0 &&
                (t.fate == cargo_fate::in_flight || t.fate == cargo_fate::awaiting_sale))
                return true;
        return false;
    };
    for (int tail = 0; tail < o.tail_cap && pending(); ++tail, ++q)
        step(econ++, (q + 1) * k_econ_tick_days, -1);
    out.play_quarters = q;

    out.windows[0] = tally_window(tracks, 0);
    out.windows[1] = tally_window(tracks, 1);
    return out;
}

int run_far_trade(int argc, char** argv)
{
    far_options o;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--far-trade") continue;
        else if (a == "--seeds" && i + 1 < argc)      o.seeds = std::atoi(argv[++i]);
        else if (a == "--first-seed" && i + 1 < argc) o.first_seed = static_cast<std::uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        else if (a == "--years" && i + 1 < argc)      o.years = std::atoi(argv[++i]);
        else if (a == "--epoch" && i + 1 < argc)      o.epoch = std::atoll(argv[++i]);
        else if (a == "--tail" && i + 1 < argc)       o.tail_cap = std::atoi(argv[++i]);
        else if (a == "--fast")                       o.fast = true;
        else
        {
            std::printf("usage: %s --far-trade [--seeds N] [--first-seed S] [--years N] "
                        "[--epoch Y] [--tail N] [--fast]\n", argv[0]);
            return 2;
        }
    }
    if (o.seeds <= 0 || o.years <= 0 || o.tail_cap < 0)
    {
        std::printf("--seeds and --years must be positive, --tail non-negative\n");
        return 2;
    }

    // The data layer, loaded the way the app loads it (BL-1007).
    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    reg.set_era(era_band_for_epoch(o.epoch));
    world_gen_config gen_cfg;
    gen_cfg.load_from_lua(lua);
    lua.load("scripts/works.lua");
    works_registry works;
    works.load_from_lua(lua);
    if (reg.recipe_count(building_type::processing_facility) == 0)
    {
        std::printf("FATAL: no recipes loaded — run from the repo root.\n");
        return 2;
    }

    std::printf("=== haulage_measure --far-trade (BL-1006) — delivered volume sold at its "
                "destination ===\n");
    std::printf("seeds %u..%u; epoch %lld (%s band); pre-history %s; works rows %zu\n",
                o.first_seed, o.first_seed + static_cast<std::uint32_t>(o.seeds) - 1,
                static_cast<long long>(o.epoch),
                era_band_for_epoch(o.epoch) == era_band::ancient ? "ANCIENT" : "INDUSTRIAL",
                o.fast ? "OFF (--fast: NOT the app's world)" : "ON (the app's world)",
                works.size());
    std::printf("opening position: %d validation ticks (app::validation_ticks), spectated\n",
                k_validation_ticks);
    std::printf("window E: dispatches in play quarters 1-4 (the first year from the epoch)\n");
    std::printf("window Y: dispatches in play quarters %d-%d (the year after %d year(s) of play)\n",
                4 * o.years + 1, 4 * o.years + 4, o.years);
    std::printf("pools on this tree are keyed (corp, body): an arrival is credited to the "
                "(corp, destination market) pool\n");

    std::array<far_tally, 2> pooled{};
    long overflows = 0;
    long arrivals_checked = 0, pool_mismatches = 0;
    int  min_priced = std::numeric_limits<int>::max();
    long markets = 0;
    for (int s = 0; s < o.seeds; ++s)
    {
        const std::uint32_t seed = o.first_seed + static_cast<std::uint32_t>(s);
        std::printf("\n##### seed %u #####\n", seed);
        std::fflush(stdout);
        const far_seed_result r = run_far_seed(o, seed, reg, gen_cfg, works);
        min_priced = std::min(min_priced, r.priced);
        markets += r.markets;
        overflows += r.ring_overflows;
        arrivals_checked += r.arrivals_checked;
        pool_mismatches += r.pool_model_mismatches;
        std::printf("seed %u: %d markets, %d play quarters run (windows + tail)\n", seed,
                    r.markets, r.play_quarters);
        const char* names[2] = { "E", "Y" };
        for (int k = 0; k < 2; ++k)
        {
            const far_tally& t = r.windows[static_cast<std::size_t>(k)];
            std::printf("  seed %u window %s: dispatched %ld (%.1f u; same-body->other %ld, "
                        "->home %ld, inter-body %ld) | delivered %.1f u (same-body %.1f, "
                        "inter-body %.1f) | (a) %.1f u (same-body %.1f, inter-body %.1f)",
                        seed, names[k], t.n_disp, t.v_disp, t.n_disp_sb_other, t.n_disp_sb_own,
                        t.n_disp_ib, t.v_del_total, t.v_del_sb, t.v_del_ib, t.v_a_sb + t.v_a_ib,
                        t.v_a_sb, t.v_a_ib);
            const double a = t.v_a_sb + t.v_a_ib;
            if (a > 0.0)
                std::printf(" | (b) %.1f%%", 100.0 * t.v_a_skipped / a);
            else
                std::printf(" | (b) n/a");
            std::printf(" | market exports %ld (%.1f u, %.1f u landed on a shelf)\n", t.n_mx,
                        t.v_mx, t.v_mx_del);
            pooled[static_cast<std::size_t>(k)].add(t);
        }
        std::fflush(stdout);
    }

    std::printf("\n===== POOLED over %d seed(s) =====\n", o.seeds);
    print_window(pooled[0], "window E — dispatched in the first year of play, from the epoch", true);
    char title[128];
    std::snprintf(title, sizeof title,
                  "window Y — dispatched in the year after %d year(s) of play", o.years);
    print_window(pooled[1], title, true);

    // R2: the same-body figure, said plainly and with its reason.
    std::printf("\n--- same-body hauls on this tree ---\n");
    for (int k = 0; k < 2; ++k)
    {
        const far_tally& t = pooled[static_cast<std::size_t>(k)];
        std::printf("  window %s: %ld same-body convoys delivered %.1f units; sold at their "
                    "destination: %.1f units; %ld of %ld landed in a pool that clears at home\n",
                    k == 0 ? "E" : "Y", t.n_del_sb, t.v_del_sb, t.v_a_sb, t.n_sb_del_home,
                    t.n_sb_del);
    }
    std::printf("  WHY: credit_arrived_convoys credits the (corp, destination MARKET) pool "
                "(BL-1003), so a same-body\n"
                "  haul's cargo sells at its destination. Under the retired (corp, body) pools "
                "it returned to the pool\n"
                "  it left and sold at home; a nonzero 'landed in a pool that clears at home' "
                "count now means a haul\n"
                "  whose destination IS its source market, which dispatch refuses.\n");

    std::printf("\n--- non-vacuity guards ---\n");
    check(min_priced > 10,
          "the gen config was PARSED on every seed (priced resources exceed the C++ fallback's ten)");
    check(markets > 0, "the worlds carried markets");
    check(pooled[0].n_disp > 0 && pooled[1].n_disp > 0,
          "each window observed at least one dispatch (pooled over seeds)");
    check(pooled[0].n_del_sb + pooled[0].n_del_ib > 0 && pooled[1].n_del_sb + pooled[1].n_del_ib > 0,
          "each window observed at least one delivery read through its first clear");
    check(overflows == 0,
          "no clear pushed more exchange rows than the ring retains (sales were fully readable)");
    std::printf("  pool model: %ld credited arrivals checked; %ld pool(s) did not gain their cargo\n",
                arrivals_checked, pool_mismatches);
    check(arrivals_checked > 0 && pool_mismatches == 0,
          "every credited arrival raised the (corp, destination market) pool — or, for a market's "
          "own export (BL-1071), the destination shelf — by exactly its cargo "
          "(the pool model this reading assumes is the tree's)");

    std::printf("\n=== haulage_measure --far-trade: %d failure(s) ===\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--far-trade") == 0)
            return run_far_trade(argc, argv);

    const int n_seeds       = (argc > 1) ? std::atoi(argv[1]) : 5;
    const int n_trade_ticks = (argc > 2) ? std::atoi(argv[2]) : 20;
    bool per_tick = false;
    for (int i = 3; i < argc; ++i)
        if (std::string(argv[i]) == "--per-tick")
            per_tick = true;
    if (n_seeds <= 0 || n_trade_ticks <= 0)
    {
        std::printf("usage: %s [seeds] [ticks] [--per-tick]  (both positive)\n", argv[0]);
        return 2;
    }

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    world_gen_config gen_cfg;
    gen_cfg.load_from_lua(lua);
    if (reg.recipe_count(building_type::processing_facility) == 0)
    {
        std::printf("FATAL: no recipes loaded — run from the repo root.\n");
        return 2;
    }

    std::printf("=== haulage_measure (BL-442 step 2) — the cost of serving a neighbour ===\n");
    std::printf("%d seeds, real generated world, real Lua logistics costs\n\n", n_seeds);

    std::printf("authored logistics cost per unit distance per unit cargo:\n");
    std::printf("  land %.4f   sea %.4f   space %.4f\n\n",
                static_cast<double>(reg.logistics_cost(convoy_mode::land)),
                static_cast<double>(reg.logistics_cost(convoy_mode::sea)),
                static_cast<double>(reg.logistics_cost(convoy_mode::space)));
    std::printf("current price band: floor %.3fx  ceil %.3fx\n\n",
                static_cast<double>(reg.price_band().floor_mult),
                static_cast<double>(reg.price_band().ceil_mult));

    // Per-unit haulage to each market's NEAREST market neighbour, pooled over seeds.
    std::vector<double> nearest_haul;
    // Every same-body market pair, for the wider picture.
    std::vector<double> all_pair_haul;
    long n_markets = 0, n_bodies_multi = 0, n_sea_legs = 0;

    // Authored base prices, pooled: the denominator of the derivation.
    std::vector<double> base_prices;
    double base_min = std::numeric_limits<double>::max(), base_max = 0.0;
    int base_min_idx = -1, base_max_idx = -1;

    for (int s = 0; s < n_seeds; ++s)
    {
        world_params p = no_prehistory();
        p.seed = static_cast<uint32_t>(s);
        world w = make_hard_coded_world(p, nullptr, gen_cfg);
        // The landscape-search WINNER, as the app applies it — not the seed
        // candidate (BL-979; apply_shipped_landscape in harness_params.hpp).
        print_shipped_landscape(apply_shipped_landscape(w, reg, static_cast<uint32_t>(s)));

        // Sorted id walk (standing.hpp convention) — markets is an unordered_map.
        std::vector<entity_id> mids;
        mids.reserve(w.markets.size());
        for (const auto& [id, m] : w.markets)
            mids.push_back(id);
        std::sort(mids.begin(), mids.end());

        if (s == 0)
        {
            // Base prices are authored per market from the rarity table; read the
            // first market's, which is the world's authored reference set.
            for (const entity_id mid : mids)
            {
                const market_component& m = w.markets.at(mid);
                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    const double bp = static_cast<double>(m.base_price[r]);
                    if (bp <= 0.0)
                        continue;
                    base_prices.push_back(bp);
                    if (bp < base_min) { base_min = bp; base_min_idx = static_cast<int>(r); }
                    if (bp > base_max) { base_max = bp; base_max_idx = static_cast<int>(r); }
                }
                break; // one market's authored table is the reference
            }
        }

        // Group markets by body.
        std::vector<entity_id> bodies;
        for (const entity_id mid : mids)
        {
            const entity_id b = w.markets.at(mid).body;
            if (std::find(bodies.begin(), bodies.end(), b) == bodies.end())
                bodies.push_back(b);
        }
        std::sort(bodies.begin(), bodies.end());

        for (const entity_id body : bodies)
        {
            std::vector<entity_id> on_body;
            for (const entity_id mid : mids)
                if (w.markets.at(mid).body == body)
                    on_body.push_back(mid);
            if (on_body.size() < 2)
                continue;
            ++n_bodies_multi;

            for (const entity_id a : on_body)
            {
                const entity_id ta = w.markets.at(a).centre_tile;
                if (ta == null_entity)
                    continue;
                ++n_markets;
                double best = std::numeric_limits<double>::max();
                for (const entity_id b : on_body)
                {
                    if (b == a)
                        continue;
                    const entity_id tb = w.markets.at(b).centre_tile;
                    if (tb == null_entity)
                        continue;
                    // Read cost/mode out of the cache reference immediately —
                    // it is only valid until an invalidation.
                    double cost;
                    bool   sea;
                    {
                        const logistics_path& path = intra_body_path(w, body, ta, tb);
                        if (!path.reachable)
                            continue;
                        cost = static_cast<double>(path.cost);
                        sea  = path.crosses_ocean;
                    }
                    const double unit = static_cast<double>(
                        reg.logistics_cost(sea ? convoy_mode::sea : convoy_mode::land));
                    // The node discount needs the path tiles; recompute against the
                    // same cached path. Deliberately NOT applied here: the discount
                    // is a corp-built optimisation (cities/hubs on the route) and the
                    // day-1 case the requirement names has neither. Measuring the
                    // UNDISCOUNTED haul is the conservative side — it is the cost a
                    // day-1 trader actually pays, and the discount only makes the
                    // derived ceiling more than sufficient.
                    const double haul = unit * cost;
                    all_pair_haul.push_back(haul);
                    if (haul < best)
                    {
                        best = haul;
                        if (sea) ++n_sea_legs;
                    }
                }
                if (best < std::numeric_limits<double>::max())
                    nearest_haul.push_back(best);
            }
        }
    }

    check(!nearest_haul.empty(),
          "measured at least one market-to-neighbour haul (vacuity guard)");
    if (nearest_haul.empty())
    {
        std::printf("\n=== haulage_measure: %d failure(s) ===\n", g_failures);
        return 1;
    }

    const double n_p10 = quantile(nearest_haul, 0.10);
    const double n_med = quantile(nearest_haul, 0.50);
    const double n_p90 = quantile(nearest_haul, 0.90);
    const double n_max = quantile(nearest_haul, 1.00);
    const double a_med = quantile(all_pair_haul, 0.50);
    const double a_max = quantile(all_pair_haul, 1.00);

    std::printf("\n--- per-unit haulage, market -> NEAREST market neighbour ---\n");
    std::printf("  samples %zu over %ld multi-market bodies (%ld markets)\n",
                nearest_haul.size(), n_bodies_multi, n_markets);
    std::printf("  p10 %8.4f   median %8.4f   p90 %8.4f   max %8.4f\n",
                n_p10, n_med, n_p90, n_max);
    std::printf("--- per-unit haulage, ALL same-body market pairs ---\n");
    std::printf("  samples %zu   median %8.4f   max %8.4f\n",
                all_pair_haul.size(), a_med, a_max);

    std::printf("\n--- authored base prices (the derivation's denominator) ---\n");
    std::printf("  min %6.3f (resource #%d)   median %6.3f   max %6.3f (resource #%d)   max/min %6.2f\n",
                base_min, base_min_idx, quantile(base_prices, 0.50),
                base_max, base_max_idx,
                base_min > 0.0 ? base_max / base_min : 0.0);

    std::printf("\n--- DERIVATION: ceil_mult > 1 + haulage / base_price ---\n");
    const double bmed = quantile(base_prices, 0.50);
    std::printf("  vs cheapest good (%6.3f), median haul  : ceil > %6.2f\n", base_min, 1.0 + n_med / base_min);
    std::printf("  vs cheapest good (%6.3f), p90 haul     : ceil > %6.2f\n", base_min, 1.0 + n_p90 / base_min);
    std::printf("  vs cheapest good (%6.3f), max haul     : ceil > %6.2f\n", base_min, 1.0 + n_max / base_min);
    std::printf("  vs median good   (%6.3f), median haul  : ceil > %6.2f\n", bmed, 1.0 + n_med / bmed);
    std::printf("  vs median good   (%6.3f), p90 haul     : ceil > %6.2f\n", bmed, 1.0 + n_p90 / bmed);
    std::printf("\n--- DERIVATION: scarcity must outrank abundance ---\n");
    std::printf("  ceil_mult > max_base/min_base = %6.2f\n",
                base_min > 0.0 ? base_max / base_min : 0.0);

    // -----------------------------------------------------------------------
    // Does inter-market trade actually HAPPEN? (BL-442 step 2's real deliverable)
    // -----------------------------------------------------------------------
    // The derivation above says what the ceiling must EXCEED. It cannot say
    // whether widening it makes trade occur — that is a claim about the running
    // economy, and asserting it from arithmetic is exactly the guess this
    // harness exists to refuse. So run the real tick loop and count what moves.
    //
    // Three counters, because they are three different claims:
    //   intra-body inter-market convoys — the "nearby market" of the requirement
    //   inter-body convoys              — the space lane (launchpad-gated)
    //   trade_routes                    — BL-088's persistent record, body-level
    //                                     only, so it can only ever see the second
    std::printf("\n--- does inter-market trade OCCUR? (%d ticks per seed) ---\n", n_trade_ticks);
    long conv_total = 0, conv_inter_market = 0, conv_inter_body = 0, routes = 0;
    // Per-tick dispatches pooled over seeds (all / intra-body market->market);
    // printed only under --per-tick.
    std::vector<long> tick_all(static_cast<std::size_t>(n_trade_ticks), 0L);
    std::vector<long> tick_im(static_cast<std::size_t>(n_trade_ticks), 0L);
    for (int s = 0; s < n_seeds; ++s)
    {
        world_params p = no_prehistory();
        p.seed = static_cast<uint32_t>(s);
        world w = make_hard_coded_world(p, nullptr, gen_cfg);
        // The landscape-search WINNER, as the app applies it — not the seed
        // candidate (BL-979; apply_shipped_landscape in harness_params.hpp).
        print_shipped_landscape(apply_shipped_landscape(w, reg, static_cast<uint32_t>(s)));

        std::size_t seen = 0;
        for (int t = 1; t <= n_trade_ticks; ++t)
        {
            // BL-995: app::step_economy's order — advance -> credit arrivals ->
            // economy -> dispatch -> clear -> budget.
            advance_convoys(w);
            credit_arrived_convoys(w, t);
            const economy_report report = run_economy_step(w, reg);
            seen = w.convoys.size(); // what is appended past here is this dispatch
            dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                             reg.logistics_cost(convoy_mode::space));
            // Count only convoys appended THIS tick — w.convoys is append-only
            // within a tick and compacted by advance/credit, so walking from the
            // previous size is the honest dispatch count.
            for (std::size_t i = seen; i < w.convoys.size(); ++i)
            {
                const convoy_component& c = w.convoys[i];
                ++conv_total;
                ++tick_all[static_cast<std::size_t>(t - 1)];
                if (c.mode == convoy_mode::space)
                    ++conv_inter_body;
                else if (c.source_market != c.dest_market)
                {
                    ++conv_inter_market;
                    ++tick_im[static_cast<std::size_t>(t - 1)];
                }
            }
            const auto flows = clear_markets(w, reg, report);
            apply_budget(w, reg, flows, report.workforce_contention, nullptr);
        }
        routes += static_cast<long>(w.trade_routes.size());
    }
    std::printf("  convoys dispatched (all)          : %ld\n", conv_total);
    std::printf("  ... intra-body, market -> market  : %ld  (DISPATCHES, not delivered trade — "
                "see --far-trade)\n", conv_inter_market);
    std::printf("  ... inter-body (space lane)       : %ld\n", conv_inter_body);
    std::printf("  persistent trade_routes (BL-088)  : %ld\n", routes);
    if (per_tick)
    {
        std::printf("\n--- per-tick dispatches, pooled over %d seeds "
                    "(tick, all, intra-body market->market) ---\n", n_seeds);
        for (int t = 0; t < n_trade_ticks; ++t)
            std::printf("  tick %3d : %6ld %6ld\n", t + 1,
                        tick_all[static_cast<std::size_t>(t)],
                        tick_im[static_cast<std::size_t>(t)]);
    }

    std::printf("\n=== haulage_measure: %d failure(s) ===\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
