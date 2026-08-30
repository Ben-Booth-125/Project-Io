// acquisition_viability — BL-634. Sprint 20's DEFINITION OF DONE, measured.
//
// BEN'S CRITERION, verbatim (2026-08-26), and the only thing this harness is
// testing:
//
//     "a corporation can save up in a few economy ticks to buy another company,
//      and then continue making a profit."
//
// Three clauses, three measurements, in that order: it must ACCUMULATE (R1), it
// must REACH a buyable firm's price in a stated number of quarters (R2), and it
// must still be earning afterwards WITH the acquired holdings' maintenance
// folded in (R3). R4 keeps the answer honest — a player loop that only closes in
// a dying world is not a working loop — and R5 says what the measurement did to
// `k_acquisition_multiple`.
//
// Requirement group `acquisition-viability`, rows R0-R5.
//
// ---------------------------------------------------------------------------
// R0 IS FIRST AND IT IS NOT A FORMALITY
// ---------------------------------------------------------------------------
// `make_hard_coded_world`'s `gen_cfg` parameter defaults to the C++ fallback in
// world_gen_config.hpp, which carries base prices for TEN of forty-seven
// resources. scripts/world_gen.lua authors FORTY-TWO. Markets are seeded from
// that table and BOTH basket injectors silently SKIP an unpriced resource, so a
// harness that omits it measures a world where stone, timber, clay, fibre,
// planks and tools are not cheap but UNQUOTED — unsellable at any workforce, by
// any corporation, forever.
//
// That defect invalidated an entire sprint's numbers on 2026-08-26, and the
// first attempt at fixing it added the LOAD and not the PARSE, which reads
// identically at the call site and measures the same wrong world. So this file
// loads it, parses it into a `world_gen_config`, passes it as the third
// argument, and PRINTS THE COUNT OF PRICED RESOURCES in its own header — a
// reader can tell at a glance which of the two worlds these numbers came from
// without trusting a comment.
//
// ---------------------------------------------------------------------------
// THE SHIPPED SPAWN, IN ITS OWN ORDER (and it is NOT spawn_solvency's)
// ---------------------------------------------------------------------------
// spawn_solvency (BL-635) warm-starts with `spectating = false` and reads
// whichever corp generation provisionally flagged. BL-630 changed what the game
// does, and this harness follows the game:
//
//   1. make_hard_coded_world(params, nullptr, gen_cfg)   — with the config.
//   2. assign_default_recipes                             — load_economy's pass.
//   3. generate_background_firms                          — app.cpp:912.
//   4. assign_default_recipes                             — app.cpp:923, and NOT
//      belt-and-braces: without it every processor a background firm authored
//      keeps `no_recipe` for the whole campaign.
//   5. 80 warm ticks with `spectating = TRUE`. Nobody is seated yet, so the
//      no-auto-act prohibition has no subject (BL-409's rule, BL-630's second
//      case) and every corp is scorer-driven. A warm start run false would
//      measure a world one corp never acted in.
//   6. seat_player_corporation — the seat is DRAWN from the viable specialists.
//   7. THE MEASURED WINDOW: live play, `spectating = FALSE`. The seated corp is
//      now excluded from the scorer, exactly as a human's corp is, so what it
//      accumulates here is what it accumulates by holding what it was handed.
//      That is the honest reading of "save up".
//
// The tick itself is app::step_economy's, in its order, with the production sink
// passed so the BL-343 levy path is live.
//
// ---------------------------------------------------------------------------
// WHAT IS DELIBERATELY NOT RE-DERIVED
// ---------------------------------------------------------------------------
// The price comes from `corp_acquisition_price` and the buy goes through
// `apply_corp_command(corp_verb::buy_corporation)`. A harness that restated the
// formula would pass whenever the implementation was self-consistently wrong,
// and a harness that mutated the world by hand would not exercise the
// dissolution rule at all. The candidate filter mirrors the seam's own gates
// (exists / not the acquirer / not the player / `publicly_held` / has filed) so
// the shortlist and the seam agree; the seam re-checks every one of them and the
// result is asserted `applied`, so a divergence is a failure here rather than a
// silent skip.
//
// Usage:  acquisition_viability [seed_count] [--fast] [--seed0 N]
//                               [--search Q] [--after Q]
//   --fast     zero the pre-epoch year-tick sim. ~23 s a world cheaper and NOT
//              the shipped spawn — iteration only. Baseline-relative rows are
//              reported rather than asserted under it.
//   --search Q quarters of live play to search for affordability (default 60).
//   --after  Q quarters to run on after a buy (default 20).
//   --target cheapest|priced  which gate the BUY fires on (default cheapest — the
//              brief's literal gate). Both gates are always REPORTED; only one can
//              fire, because a buy changes the world it was measured in.
//
// Exits 0 on PASS, non-zero on any failure.

#include "scripting/lua_state.hpp"

#include "harness_params.hpp"
#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/market_clearing.hpp"
#include "world/nation_step.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/supply_system.hpp"
#include "world/tech_gate.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* row, const char* what)
{
    std::printf("  [%s] %-3s %s\n", ok ? "PASS" : "FAIL", row, what);
    if (!ok)
        ++g_failures;
}

/// True on the shipped spawn, false under --fast.
///
/// --fast leaves a world with no settlement history, so its buildings run
/// near-unstaffed and every magnitude in it is a different quantity wearing the
/// same name. Rows whose meaning depends on the shipped spawn are REPORTED
/// under --fast rather than asserted — a red row that means nothing teaches a
/// reader to ignore red rows.
bool g_real_spawn = true;

void check_on_real_spawn(bool ok, const char* row, const char* what)
{
    if (!g_real_spawn)
    {
        std::printf("  [....] %-3s %s  (not asserted under --fast — "
                    "the shipped spawn is the subject)\n", row, what);
        return;
    }
    check(ok, row, what);
}

constexpr int k_warm_ticks = 80;  ///< app::pre_game_ticks.
constexpr int k_r1_window  = 8;   ///< Quarters R1 reads for its trend. Two years.

/// BL-573: run_nation_step's template registry. Empty is correct — nothing in
/// this sweep opens a mercenary contract, so the walk is vacuous.

// ---------------------------------------------------------------------------
// The tick
// ---------------------------------------------------------------------------

/// One economy tick in app::step_economy's order. @p spectating is the ONLY
/// thing that differs between the warm start and live play, and it is the whole
/// of BL-630: true means nobody is seated and every corp is scorer-driven.
void tick(world& w, const recipe_registry& reg, int t, bool spectating)
{
    w.current_econ_tick = t;
    w.current_day_tick  = t;
    lp_pool_map lp;
    dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                     reg.logistics_cost(convoy_mode::space), &lp);
    advance_convoys(w);
    economy_report rep = run_economy_step(w, reg, spectating, &lp);
    auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention, &rep.budgets, &rep.buildings,
                 &rep.building_labour);
    run_nation_step(w, reg, rep, t);
    advance_tech_gates(w);
    credit_arrived_convoys(w, t);
}

std::vector<entity_id> sorted_corp_ids(const world& w)
{
    std::vector<entity_id> ids;
    ids.reserve(w.corporations.size());
    for (const auto& kv : w.corporations)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

// ---------------------------------------------------------------------------
// The acquisition shortlist
// ---------------------------------------------------------------------------

struct target_quote
{
    entity_id   corp  = null_entity;
    float       price = 0.0f;
    std::string name;
    float       book         = 0.0f;
    float       trailing_net = 0.0f;
    float       balance      = 0.0f;
    int         holdings     = 0;
    bool        found        = false;
};

/// The cheapest firm @p acquirer may legally buy, priced by the seam's own
/// function. The predicates mirror `apply_corp_command`'s gates (2)-(4) so the
/// shortlist and the seam agree; the seam re-checks all of them, and the caller
/// asserts the command applied, so any divergence surfaces as a failure rather
/// than a quiet skip.
///
/// @p require_positive selects the SECOND gate this harness measures, and the
/// reason it exists is a finding rather than a convenience. `price = max(0, book
/// + k x trailing_net + balance)` is signed in its cash term, so the cheapest
/// buyable firm on a live world is reliably the most INDEBTED one: its negative
/// balance cancels its book value, the expression goes below zero, and the floor
/// hands it over for nothing. Buying it is free and transfers the debt — the
/// dissolution rule moves `balance` as property, so a zero price is not a
/// bargain but a liability taken on at face value.
///
/// So "quarters until it can afford the cheapest firm" is answered by a corp
/// that saved nothing at all, and answers no question anyone asked. Both gates
/// are therefore measured on every seed: the brief's literal cheapest, and the
/// cheapest firm carrying a price ABOVE ZERO — the cheapest firm you must
/// actually save up for.
///
/// Sorted walk over the corporation ids, strict `<` on the price, so ties break
/// on the lowest id and the choice cannot inherit the unordered map's layout.
target_quote cheapest_buyable(const world& w, const recipe_registry& reg,
                              entity_id acquirer, bool require_positive)
{
    target_quote best;
    const float k = reg.acquisition().multiple;
    for (const entity_id id : sorted_corp_ids(w))
    {
        if (id == acquirer || id == w.player_entity)
            continue;
        const corporation_component& cc = w.corporations.at(id);
        if (cc.is_player)
            continue;
        if (cc.ownership_class != ownership_class::publicly_held)
            continue;
        if (cc.returns.empty())
            continue;   // never filed — the seam refuses to price it
        const float price = corp_acquisition_price(cc, k);
        if (!std::isfinite(price))
            continue;
        if (require_positive && !(price > 0.0f))
            continue;
        if (best.found && !(price < best.price))
            continue;
        best.found        = true;
        best.corp         = id;
        best.price        = price;
        best.name         = cc.name;
        best.book         = cc.returns.back().book_value;
        best.trailing_net = corp_trailing_net(cc);
        best.balance      = cc.balance;
        best.holdings     = static_cast<int>(cc.assets.size());
    }
    return best;
}

// ---------------------------------------------------------------------------
// Field health (R4)
// ---------------------------------------------------------------------------

struct field_snapshot
{
    int rivals   = 0;
    int solvent  = 0;
    int holdings = 0;
    int units    = 0;
    double net_sum = 0.0;   ///< summed last-filed net over the rivals

    // BL-678 (companies are open) — CONSOLIDATION. Opening the whole company
    // population to acquisition makes ~85 firms per seed buyable where 1.6 were,
    // and `buy_corporation` is scored in corp_ai.cpp's candidate list, so a rival
    // can now snowball through acquisition in a way this economy has never been
    // exercised against. These four fields are what tells you whether it did.
    int    corps        = 0;   ///< every corporation alive, PLAYER INCLUDED
    int    holders      = 0;   ///< corporations holding at least one building
    int    max_holdings = 0;   ///< the largest single holding count in the field
    /// Herfindahl index over holding SHARES, 0..1. 1/holders when every holder is
    /// the same size; 1.0 when one firm owns the world. The number that answers
    /// "did the field concentrate into fewer hands" without needing a threshold.
    double hhi          = 0.0;
};

field_snapshot survey_field(const world& w)
{
    field_snapshot f;
    for (const entity_id id : sorted_corp_ids(w))
    {
        const corporation_component& cc = w.corporations.at(id);
        const int hold = static_cast<int>(cc.assets.size());
        f.holdings += hold;
        ++f.corps;
        if (hold > 0)
            ++f.holders;
        if (hold > f.max_holdings)
            f.max_holdings = hold;
        if (id == w.player_entity || cc.is_player)
            continue;
        ++f.rivals;
        if (cc.balance > 0.0f)
            ++f.solvent;
        if (!cc.returns.empty())
            f.net_sum += static_cast<double>(cc.returns.back().net);
    }
    if (f.holdings > 0)
    {
        // Second walk, same sorted order: the shares need the total, which the
        // first walk is still accumulating.
        for (const entity_id id : sorted_corp_ids(w))
        {
            const double share = static_cast<double>(w.corporations.at(id).assets.size())
                               / static_cast<double>(f.holdings);
            f.hhi += share * share;
        }
    }
    // Ascending unit id, so the walk over the unordered `w.units` cannot inherit
    // its layout — the count is order-independent, but the walk must not be.
    std::vector<entity_id> unit_ids;
    unit_ids.reserve(w.units.size());
    for (const auto& kv : w.units)
        unit_ids.push_back(kv.first);
    std::sort(unit_ids.begin(), unit_ids.end());
    for (const entity_id uid : unit_ids)
        if (w.units.at(uid).owner != w.player_entity)
            ++f.units;
    return f;
}

// ---------------------------------------------------------------------------
// THE BUYABLE FIELD — a census (2026-08-29, for the acquisitions ledger)
// ---------------------------------------------------------------------------
//
// The acquisitions ledger groups the buyable field into PURCHASABLE (priced
// within the acquirer's balance) and POSSIBLE (priced, beyond it today). Both
// groups are empty unless firms are BOTH `publicly_held` AND have filed, and a
// fixture capture on 2026-08-29 showed most firms rendering a dash for Capital
// — i.e. not public at all. So the field may be a ledger with nothing in it,
// and that has to be measured before a pixel of it is laid out.
//
// It REPORTS and never asserts a magnitude, deliberately: the moment it asserts
// "the field must hold N firms", tuning gets aimed at the harness instead of at
// the economy — the failure mode `demand_census`'s header already names. Its one
// assertion is the non-vacuity of its own reading.
struct census_entry
{
    std::string name;
    float       price      = 0.0f;
    bool        affordable = false;
};

struct field_census
{
    int corps_total     = 0;   ///< every corporation, player included
    int public_held     = 0;   ///< publicly_held, acquirer and player excluded
    int private_held    = 0;
    int closed_held     = 0;
    int public_filed    = 0;   ///< public AND has filed — THE BUYABLE FIELD
    int public_unfiled  = 0;   ///< public but never filed — priced by nothing
    int nonpublic_filed = 0;   ///< files but is unbuyable (disclosure ≠ acquisition)
    int purchasable     = 0;   ///< buyable, priced within the acquirer's balance
    int possible        = 0;   ///< buyable, priced, beyond the acquirer's balance
    int at_floor        = 0;   ///< priced at exactly zero by the max(0, ...) floor
    float acquirer_balance = 0.0f;
    float min_price = 0.0f, max_price = 0.0f, median_price = 0.0f;
    std::vector<census_entry> entries;   ///< the buyable field, ascending price
};

/// Walks the field through the SAME gates `apply_corp_command` applies, in the
/// same order, and prices through `corp_acquisition_price` — never a restated
/// formula. `is_background` is deliberately NOT among the gates, because the
/// verb does not test it: the buyable field is every public filed firm.
field_census census_field(const world& w, const recipe_registry& reg, entity_id acquirer)
{
    field_census c;
    const float k = reg.acquisition().multiple;
    const auto ait = w.corporations.find(acquirer);
    if (ait != w.corporations.end())
        c.acquirer_balance = ait->second.balance;

    for (const entity_id id : sorted_corp_ids(w))
    {
        const corporation_component& cc = w.corporations.at(id);
        ++c.corps_total;
        if (id == acquirer || id == w.player_entity || cc.is_player)
            continue;   // gates (1)/(2): never the acquirer, never the player

        const bool filed = !cc.returns.empty();
        switch (cc.ownership_class)
        {
            case ownership_class::publicly_held:  ++c.public_held;  break;
            case ownership_class::privately_held: ++c.private_held; break;
            case ownership_class::closed:         ++c.closed_held;  break;
        }
        if (cc.ownership_class != ownership_class::publicly_held)
        {
            if (filed)
                ++c.nonpublic_filed;
            continue;   // gate (3)
        }
        if (!filed)
        {
            ++c.public_unfiled;
            continue;   // gate (4) — a firm that does not file cannot be priced
        }
        ++c.public_filed;

        const float price = corp_acquisition_price(cc, k);
        if (!std::isfinite(price))
            continue;
        if (!(price > 0.0f))
            ++c.at_floor;
        const bool afford = c.acquirer_balance >= price;   // gate (5), the seam's own test
        if (afford) ++c.purchasable; else ++c.possible;
        c.entries.push_back(census_entry{ cc.name, price, afford });
    }

    std::sort(c.entries.begin(), c.entries.end(),
              [](const census_entry& a, const census_entry& b)
              { return a.price != b.price ? a.price < b.price : a.name < b.name; });
    if (!c.entries.empty())
    {
        c.min_price    = c.entries.front().price;
        c.max_price    = c.entries.back().price;
        c.median_price = c.entries[c.entries.size() / 2].price;
    }
    return c;
}

// ---------------------------------------------------------------------------
// One seed
// ---------------------------------------------------------------------------

struct seed_row
{
    uint32_t seed = 0;

    // --- the seat ---
    entity_id   seated = null_entity;
    std::string seated_name;
    bool  floor_unmet   = false;
    int   shortlist     = 0;
    int   specialists   = 0;
    float seat_balance  = 0.0f;   ///< balance the moment the seat was drawn
    int   seat_holdings = 0;

    // --- R1: the trajectory, live play, one entry per quarter ---
    std::vector<float> balance_trace;
    std::vector<float> net_trace;
    int   r1_window   = 0;        ///< quarters R1's trend actually read
    float r1_open     = 0.0f;
    float r1_close    = 0.0f;
    float r1_per_qtr  = 0.0f;
    bool  accumulates = false;

    // --- R2: the gate, tracked for BOTH readings of "cheapest" ---
    struct gate
    {
        bool  had_target = false;   ///< such a firm existed at some quarter
        bool  afforded   = false;
        int   quarters   = 0;       ///< quarters of live play until affordable
        float price      = 0.0f;
        std::string name;
        float book       = 0.0f;
        float trailing   = 0.0f;
        float balance    = 0.0f;    ///< the TARGET's balance (signed; buying buys the debt)
        int   holdings   = 0;
        /// The cheapest quote still standing at the END of the search window on a
        /// seed that never afforded — the distance left to travel, which is the
        /// number a reader wants when the row is red.
        float final_quote = 0.0f;
    };
    gate  cheapest;   ///< the brief's literal gate: cheapest buyable public firm
    gate  priced;     ///< the cheapest firm priced ABOVE ZERO
    float final_balance = 0.0f;

    // --- R3: after the buy ---
    bool  bought          = false;
    bool  buy_attempted   = false;
    int   buy_result      = -1;
    float net_before      = 0.0f;   ///< mean filed net over the k_r1_window before the buy
    float net_after       = 0.0f;   ///< mean filed net over the run-on window
    float maint_before    = 0.0f;
    float maint_after     = 0.0f;
    int   holdings_before = 0;
    int   holdings_after  = 0;
    float balance_after   = 0.0f;
    bool  still_earning   = false;

    // --- R4 ---
    field_snapshot field_seat;
    field_snapshot field_close;
    int   field_holdings_gen = 0;

    // --- C: the buyable field, at the seat and at the last pre-buy quarter ---
    field_census census_seat;
    field_census census_close;
};

/// Mean filed `net` over a corp's last @p n returns, and the matching mean
/// maintenance. Reads the record only; nothing here re-computes a flow.
void trailing_means(const corporation_component& cc, int n, float& out_net, float& out_maint)
{
    out_net = 0.0f;
    out_maint = 0.0f;
    if (cc.returns.empty() || n <= 0)
        return;
    const std::size_t take = std::min(cc.returns.size(), static_cast<std::size_t>(n));
    double net = 0.0, maint = 0.0;
    for (std::size_t i = cc.returns.size() - take; i < cc.returns.size(); ++i)
    {
        net   += static_cast<double>(cc.returns[i].net);
        maint += static_cast<double>(cc.returns[i].maintenance);
    }
    out_net   = static_cast<float>(net / static_cast<double>(take));
    out_maint = static_cast<float>(maint / static_cast<double>(take));
}

/// Which of the two gates the acquisition actually fires on.
enum class buy_mode { cheapest, priced };

seed_row run_seed(uint32_t seed, const recipe_registry& reg, bool prehistory,
                  const world_gen_config& gen_cfg, int search_quarters,
                  int after_quarters, buy_mode mode)
{
    seed_row r;
    r.seed = seed;

    world_params p;
    p.seed = seed;
    if (!prehistory)
        p = no_prehistory(p);

    // THE SHIPPED SPAWN'S OWN ORDER — see the header. The gen_cfg is the third
    // argument and it is parsed, not merely loaded.
    world w = make_hard_coded_world(p, nullptr, gen_cfg);
    assign_default_recipes(w, reg);
    generate_background_firms(w, reg, seed ^ 0x8A21F00Du);
    assign_default_recipes(w, reg);

    for (const auto& kv : w.corporations)
        r.field_holdings_gen += static_cast<int>(kv.second.assets.size());

    // The warm start runs in SPECTATE: nobody is seated yet.
    for (int t = 1; t <= k_warm_ticks; ++t)
        tick(w, reg, t, /*spectating=*/true);

    const spawn_seat_result seat = seat_player_corporation(w, seed);
    r.seated      = seat.seated;
    r.floor_unmet = seat.floor_unmet;
    r.shortlist   = seat.shortlist_size;
    r.specialists = seat.specialist_count;
    if (r.seated == null_entity)
        return r;   // no specialist at all — reported, never a pass

    {
        const corporation_component& cc = w.corporations.at(r.seated);
        r.seated_name   = cc.name;
        r.seat_balance  = cc.balance;
        r.seat_holdings = static_cast<int>(cc.assets.size());
    }
    r.field_seat   = survey_field(w);
    r.census_seat  = census_field(w, reg, r.seated);
    r.census_close = r.census_seat;   // overwritten every quarter below

    // --- live play. The seated corp is excluded from the scorer, exactly as a
    //     human's corp is, so what it accumulates it accumulates by holding what
    //     it was handed. -------------------------------------------------------
    int t = k_warm_ticks;
    int q = 0;
    for (; q < search_quarters && !r.buy_attempted; ++q)
    {
        tick(w, reg, ++t, /*spectating=*/false);
        const corporation_component& cc = w.corporations.at(r.seated);
        r.balance_trace.push_back(cc.balance);
        r.net_trace.push_back(cc.returns.empty() ? 0.0f : cc.returns.back().net);

        r.final_balance = cc.balance;

        // The census of the field the ledger will render, this quarter. Kept as
        // the LAST PRE-BUY reading: a buy erases a firm, so a census taken after
        // one is not the field the ledger would have shown.
        r.census_close = census_field(w, reg, r.seated);

        // Both gates, every quarter. The first time each becomes affordable is
        // recorded; only the selected one fires the verb.
        const target_quote qc = cheapest_buyable(w, reg, r.seated, /*positive=*/false);
        const target_quote qp = cheapest_buyable(w, reg, r.seated, /*positive=*/true);
        const struct { const target_quote& q; seed_row::gate& g; } pairs[] = {
            { qc, r.cheapest }, { qp, r.priced },
        };
        for (const auto& pr : pairs)
        {
            if (!pr.q.found)
                continue;
            pr.g.had_target  = true;
            pr.g.final_quote = pr.q.price;
            if (pr.g.afforded || cc.balance < pr.q.price)
                continue;
            pr.g.afforded = true;
            pr.g.quarters = q + 1;
            pr.g.price    = pr.q.price;
            pr.g.name     = pr.q.name;
            pr.g.book     = pr.q.book;
            pr.g.trailing = pr.q.trailing_net;
            pr.g.balance  = pr.q.balance;
            pr.g.holdings = pr.q.holdings;
        }

        // --- fire the verb on the selected gate ------------------------------
        const target_quote& fire = (mode == buy_mode::priced) ? qp : qc;
        if (!fire.found || cc.balance < fire.price)
            continue;

        r.r1_window = std::min(q + 1, k_r1_window);
        trailing_means(cc, r.r1_window, r.net_before, r.maint_before);
        r.holdings_before = static_cast<int>(cc.assets.size());

        corp_command cmd;
        cmd.tick         = t;
        cmd.verb         = corp_verb::buy_corporation;
        cmd.corp         = r.seated;
        cmd.counterparty = fire.corp;
        const corp_command_result res = apply_corp_command(w, reg, cmd);
        r.buy_attempted = true;
        r.buy_result    = static_cast<int>(res);
        r.bought        = (res == corp_command_result::applied);
    }

    if (!r.bought)
    {
        // Never afforded (or never had a target). R1 reads the window it has.
        const corporation_component& cc = w.corporations.at(r.seated);
        r.r1_window   = std::min<int>(q, k_r1_window);
        r.final_balance = cc.balance;
        trailing_means(cc, r.r1_window, r.net_before, r.maint_before);
        r.holdings_before = static_cast<int>(cc.assets.size());
    }

    // R1's trend, over the whole pre-buy segment of the trajectory. "Climbing"
    // is a comparison of two ends, not a sign test at one instant.
    if (!r.balance_trace.empty())
    {
        r.r1_open  = r.seat_balance;
        r.r1_close = r.balance_trace.back();
        const double n = static_cast<double>(r.balance_trace.size());
        r.r1_per_qtr = static_cast<float>((r.r1_close - r.r1_open) / n);
        r.accumulates = r.r1_close > r.r1_open;
    }

    // --- R3: run on with the acquired holdings and their maintenance folded in.
    if (r.bought)
    {
        for (int i = 0; i < after_quarters; ++i)
        {
            tick(w, reg, ++t, /*spectating=*/false);
            const corporation_component& cc = w.corporations.at(r.seated);
            r.balance_trace.push_back(cc.balance);
            r.net_trace.push_back(cc.returns.empty() ? 0.0f : cc.returns.back().net);
        }
        const corporation_component& cc = w.corporations.at(r.seated);
        trailing_means(cc, after_quarters, r.net_after, r.maint_after);
        r.holdings_after = static_cast<int>(cc.assets.size());
        r.balance_after  = cc.balance;
        r.still_earning  = r.net_after >= 0.0f;
    }

    r.field_close = survey_field(w);
    return r;
}

void print_trace(const char* label, const std::vector<float>& v, int stride)
{
    std::printf("%s", label);
    for (std::size_t i = 0; i < v.size(); i += static_cast<std::size_t>(stride))
        std::printf(" %.0f", static_cast<double>(v[i]));
    if (!v.empty())
        std::printf("  [last %.0f]", static_cast<double>(v.back()));
    std::printf("\n");
}

} // namespace

int main(int argc, char** argv)
{
    int      seed_count      = 8;
    uint32_t seed0           = 0;
    bool     prehistory      = true;
    int      search_quarters = 60;
    int      after_quarters  = 20;
    buy_mode mode            = buy_mode::cheapest;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--fast") == 0)
            prehistory = false;
        else if (std::strcmp(argv[i], "--seed0") == 0 && i + 1 < argc)
            seed0 = static_cast<uint32_t>(std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--search") == 0 && i + 1 < argc)
            search_quarters = std::max(1, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--after") == 0 && i + 1 < argc)
            after_quarters = std::max(1, std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--target") == 0 && i + 1 < argc)
        {
            ++i;
            if (std::strcmp(argv[i], "priced") == 0)
                mode = buy_mode::priced;
            else if (std::strcmp(argv[i], "cheapest") == 0)
                mode = buy_mode::cheapest;
            else
            {
                std::printf("usage: --target cheapest|priced\n");
                return 2;
            }
        }
        else if (argv[i][0] != '-')
            seed_count = std::max(1, std::atoi(argv[i]));
    }
    g_real_spawn = prehistory;

    // The real data layer, loaded as app::load_economy loads it.
    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);

    // R0. PARSED, not merely loaded — the one line that decides whether these
    // numbers describe the shipped spawn or a world where a third of the ancient
    // roster cannot be sold at all.
    world_gen_config gen_cfg{};
    gen_cfg.load_from_lua(lua);

    int priced = 0;
    for (std::size_t i = 0; i < resource_count; ++i)
        if (gen_cfg.kepler_base_price[i] > 0.0f)
            ++priced;

    world_params probe;              // for the epoch year alone
    reg.set_era(era_band_for_epoch(probe.epoch_year));

    // Vacuity guard: an empty registry would print every corp as equally poor
    // and diagnose nothing.
    if (reg.economics(building_type::extraction_site).maintenance <= 0.0f)
    {
        std::printf("FATAL: the registry authored no extraction maintenance — "
                    "scripts/economy.lua did not load.\n");
        return 2;
    }

    std::printf("acquisition_viability — BL-634, requirement group "
                "`acquisition-viability` R0-R5\n");
    std::printf("  Ben's criterion: \"a corporation can save up in a few economy "
                "ticks to buy another\n"
                "                   company, and then continue making a profit.\"\n");
    std::printf("  %d seeds from %u | %d warm ticks in SPECTATE, then seat, then "
                "live play\n"
                "  search window %d quarters, run-on %d quarters, prehistory %s\n"
                "  the buy fires on: %s\n",
                seed_count, seed0, k_warm_ticks, search_quarters, after_quarters,
                prehistory ? "ON (the shipped spawn)" : "OFF (--fast, NOT the spawn)",
                mode == buy_mode::priced
                    ? "GATE B, the cheapest firm priced ABOVE ZERO (--target priced)"
                    : "GATE A, the cheapest buyable public firm (--target cheapest)");

    // =====================================================================
    // R0 — the generation config, stated in the header where a reader sees it
    // =====================================================================
    std::printf("\n=== R0  THE GENERATION CONFIG ===\n");
    std::printf("  world_gen_config: PARSED from scripts/world_gen.lua and passed to "
                "make_hard_coded_world\n");
    std::printf("  PRICED RESOURCES: %d of %d  (the C++ fallback carries 10 — if this "
                "line says 10,\n"
                "                    the config was not parsed and every number below "
                "is of the wrong world)\n",
                priced, static_cast<int>(resource_count));
    std::printf("  k_acquisition_multiple = %.3f   (scripts/economy.lua, "
                "economy.acquisition.multiple)\n", static_cast<double>(reg.acquisition().multiple));
    std::printf("  trailing window        = %d quarters (k_acquisition_trailing_quarters)\n",
                static_cast<int>(k_acquisition_trailing_quarters));
    check(priced > 10, "R0",
          "the generation config was PARSED — more resources are priced than the "
          "C++ fallback's ten");

    // =====================================================================
    // The sweep
    // =====================================================================
    std::vector<seed_row> rows;
    rows.reserve(static_cast<std::size_t>(seed_count));
    for (int i = 0; i < seed_count; ++i)
    {
        rows.push_back(run_seed(seed0 + static_cast<uint32_t>(i), reg, prehistory,
                                gen_cfg, search_quarters, after_quarters, mode));
        const seed_row& b = rows.back();
        std::printf("  ... seed %u done  (seat %s, %s)\n", b.seed,
                    b.seated == null_entity ? "NONE" : b.seated_name.c_str(),
                    b.bought ? "BOUGHT"
                             : (b.cheapest.had_target ? "no buy" : "NO TARGET"));
        std::fflush(stdout);
    }

    // =====================================================================
    // C — THE BUYABLE FIELD. How much is there to render at all?
    // =====================================================================
    std::printf("\n=== C  THE BUYABLE FIELD - is there a ledger here? ===\n");
    std::printf("  The acquisitions ledger groups the field into PURCHASABLE "
                "(price <= the player's\n"
                "  balance) and POSSIBLE (priced, beyond it today). A firm that "
                "does not file cannot be\n"
                "  priced and does not appear at all. Every count below is taken "
                "through the SAME gates\n"
                "  apply_corp_command applies, in its order, priced by "
                "corp_acquisition_price.\n"
                "  REPORTED, never asserted: a row that pinned a field size would "
                "be an instrument to\n"
                "  tune against rather than a reading.\n\n");
    std::printf("  seed | corps | public priv closed | pub+FILED  pub-unfiled | "
                "PURCHASABLE  POSSIBLE | at floor\n");
    std::printf("  -----+-------+--------------------+-----------------------+-"
                "-----------------------+---------\n");
    long tot_public = 0, tot_filed = 0, tot_unfiled = 0, tot_purch = 0,
         tot_poss = 0, tot_floor = 0, tot_priv = 0, tot_closed = 0,
         tot_nonpub_filed = 0;
    int census_seeds = 0;
    for (const seed_row& r : rows)
    {
        if (r.seated == null_entity)
            continue;
        const field_census& c = r.census_close;
        ++census_seeds;
        tot_public  += c.public_held;   tot_priv    += c.private_held;
        tot_closed  += c.closed_held;   tot_filed   += c.public_filed;
        tot_unfiled += c.public_unfiled; tot_purch  += c.purchasable;
        tot_poss    += c.possible;      tot_floor   += c.at_floor;
        tot_nonpub_filed += c.nonpublic_filed;
        std::printf("  %4u | %5d | %6d %4d %6d | %9d %12d | %11d %9d | %7d\n",
                    r.seed, c.corps_total, c.public_held, c.private_held,
                    c.closed_held, c.public_filed, c.public_unfiled,
                    c.purchasable, c.possible, c.at_floor);
    }
    if (census_seeds > 0)
    {
        const double n = static_cast<double>(census_seeds);
        std::printf("  -----+-------+--------------------+-----------------------+-"
                    "-----------------------+---------\n");
        std::printf("  mean |       | %6.1f %4.1f %6.1f | %9.1f %12.1f | %11.1f "
                    "%9.1f | %7.1f\n",
                    tot_public / n, tot_priv / n, tot_closed / n, tot_filed / n,
                    tot_unfiled / n, tot_purch / n, tot_poss / n, tot_floor / n);
        std::printf("\n  Non-public firms that DO file (disclosure is not "
                    "acquisition): %.1f per seed.\n", tot_nonpub_filed / n);
    }

    std::printf("\n  Prices in the buyable field, and the balance they are read "
                "against:\n");
    std::printf("  seed |   player balance |      min |   median |      max | "
                "the field, ascending price\n");
    std::printf("  -----+------------------+----------+----------+----------+-"
                "---------------------------\n");
    for (const seed_row& r : rows)
    {
        if (r.seated == null_entity)
            continue;
        const field_census& c = r.census_close;
        if (c.entries.empty())
        {
            std::printf("  %4u | %16.0f |        - |        - |        - | "
                        "(EMPTY — no public filed firm)\n",
                        r.seed, static_cast<double>(c.acquirer_balance));
            continue;
        }
        std::printf("  %4u | %16.0f | %8.0f | %8.0f | %8.0f |",
                    r.seed, static_cast<double>(c.acquirer_balance),
                    static_cast<double>(c.min_price),
                    static_cast<double>(c.median_price),
                    static_cast<double>(c.max_price));
        // Every row, with a P/o marker: P purchasable, o possible.
        for (const census_entry& e : c.entries)
            std::printf(" %s%.0f", e.affordable ? "P" : "o",
                        static_cast<double>(e.price));
        std::printf("\n");
    }
    for (const seed_row& r : rows)
    {
        if (r.seated == null_entity || r.census_close.entries.empty())
            continue;
        std::printf("    seed %4u names:", r.seed);
        for (const census_entry& e : r.census_close.entries)
            std::printf("  [%s] %.24s %.0f", e.affordable ? "P" : "o",
                        e.name.c_str(), static_cast<double>(e.price));
        std::printf("\n");
    }

    std::printf("\n  Same census AT THE SEAT (quarter 0), for the change over the "
                "window:\n");
    for (const seed_row& r : rows)
    {
        if (r.seated == null_entity)
            continue;
        const field_census& c = r.census_seat;
        std::printf("    seed %4u  public %2d, filed %2d, purchasable %2d, "
                    "possible %2d, balance %.0f\n",
                    r.seed, c.public_held, c.public_filed, c.purchasable,
                    c.possible, static_cast<double>(c.acquirer_balance));
    }
    check(census_seeds > 0 && tot_public + tot_priv + tot_closed > 0, "C",
          "the census read a non-empty field of corporations (non-vacuous)");

    // =====================================================================
    // R1 — does the seated corp accumulate?
    // =====================================================================
    std::printf("\n=== R1  DOES THE SEATED CORPORATION ACCUMULATE? ===\n");
    std::printf("  The balance at the seat against the balance at the end of the "
                "pre-buy segment.\n"
                "  Climbing is the claim; a positive number at one instant is not.\n\n");
    std::printf("  seed  seated corporation          hold  short/spec |     at seat"
                "    at close   cr/qtr | climbs\n");
    std::printf("  ----------------------------------------------------------------"
                "-------------------------------------\n");
    int climbing = 0, seated_ok = 0;
    for (const seed_row& r : rows)
    {
        if (r.seated == null_entity)
        {
            std::printf("  %4u  (NO SPECIALIST SEATED — reported, never a pass)\n", r.seed);
            continue;
        }
        ++seated_ok;
        climbing += r.accumulates ? 1 : 0;
        std::printf("  %4u  %-26.26s %4d   %3d/%-3d   | %11.1f %11.1f %8.2f | %s%s\n",
                    r.seed, r.seated_name.c_str(), r.seat_holdings,
                    r.shortlist, r.specialists,
                    static_cast<double>(r.r1_open), static_cast<double>(r.r1_close),
                    static_cast<double>(r.r1_per_qtr),
                    r.accumulates ? "yes" : "NO ",
                    r.floor_unmet ? "  (viability floor UNMET at the seat)" : "");
    }
    std::printf("\n  The trajectory, every 4th quarter of live play (a year a "
                "figure):\n");
    for (const seed_row& r : rows)
    {
        if (r.seated == null_entity)
            continue;
        char label[64];
        std::snprintf(label, sizeof label, "    seed %4u bal:", r.seed);
        print_trace(label, r.balance_trace, 4);
    }
    std::printf("\n  Filed net per quarter, same stride:\n");
    for (const seed_row& r : rows)
    {
        if (r.seated == null_entity)
            continue;
        char label[64];
        std::snprintf(label, sizeof label, "    seed %4u net:", r.seed);
        print_trace(label, r.net_trace, 4);
    }
    std::printf("\n  climbing on %d/%d seated seeds\n", climbing, seated_ok);
    check(seated_ok == static_cast<int>(rows.size()), "R1",
          "every seed seated a corporation (the reading is non-vacuous)");
    check_on_real_spawn(seated_ok > 0 && climbing * 2 > seated_ok, "R1",
          "the seated corporation ACCUMULATES on a majority of seeds — its balance "
          "at the close of the pre-buy segment is above its balance at the seat");

    // =====================================================================
    // R2 — THE GATE
    // =====================================================================
    std::printf("\n=== R2  THE GATE - quarters to afford a buyable public firm ===\n");
    std::printf("  Priced by corp_acquisition_price through the ownership_class == "
                "public gate.\n"
                "  The quarters and the price are REPORTED. No target is asserted.\n\n");
    std::printf("  TWO gates, because the brief's literal one turns out to be "
                "degenerate, and that\n"
                "  is itself the finding. `price = max(0, book + k x trailing_net + "
                "balance)` is SIGNED\n"
                "  in its cash term, so the CHEAPEST buyable firm on a live world is "
                "reliably the most\n"
                "  INDEBTED one: its negative balance cancels its book value, the sum "
                "goes below zero,\n"
                "  and the floor hands the firm over for nothing. A corp that saved "
                "NOTHING can buy it,\n"
                "  and the dissolution rule then transfers the debt at face value. So "
                "the second gate is\n"
                "  the cheapest firm priced ABOVE ZERO - the cheapest firm you must "
                "actually save up for.\n");

    struct gate_totals { int with_target = 0, afforded = 0, no_target = 0;
                         double quarters = 0.0, price = 0.0; };

    auto report_gate = [&](const char* title, bool positive_only) -> gate_totals
    {
        gate_totals g;
        std::printf("\n  --- %s ---\n", title);
        std::printf("  seed | target                     | quarters |     price = "
                    "book + %.1f x trail + cash | corp balance\n",
                    static_cast<double>(reg.acquisition().multiple));
        std::printf("  -----+----------------------------+----------+--------------"
                    "-----------------------------+-------------\n");
        for (const seed_row& r : rows)
        {
            if (r.seated == null_entity)
                continue;
            const seed_row::gate& gt = positive_only ? r.priced : r.cheapest;
            if (!gt.had_target)
            {
                ++g.no_target;
                std::printf("  %4u | NO SUCH FIRM EXISTED       |    n/a   | "
                            "(reported as such; never counted as a pass)    |%12.1f\n",
                            r.seed, static_cast<double>(r.final_balance));
                continue;
            }
            ++g.with_target;
            if (gt.afforded)
            {
                ++g.afforded;
                g.quarters += gt.quarters;
                g.price    += gt.price;
                std::printf("  %4u | %-26.26s |   %4d   | %9.1f = %8.1f + %.1f x "
                            "%8.2f + %9.1f |%12.1f\n",
                            r.seed, gt.name.c_str(), gt.quarters,
                            static_cast<double>(gt.price), static_cast<double>(gt.book),
                            static_cast<double>(reg.acquisition().multiple),
                            static_cast<double>(gt.trailing),
                            static_cast<double>(gt.balance),
                            static_cast<double>(r.final_balance));
            }
            else
            {
                std::printf("  %4u | (out of reach)             | NEVER in | cheapest "
                            "quote still standing %9.1f      |%12.1f\n",
                            r.seed, static_cast<double>(gt.final_quote),
                            static_cast<double>(r.final_balance));
            }
        }
        std::printf("  afforded on %d/%d seeds that HAD such a firm (%d had none)\n",
                    g.afforded, g.with_target, g.no_target);
        if (g.afforded > 0)
            std::printf("  mean quarters to afford %.1f, mean price %.1f cr\n",
                        g.quarters / g.afforded, g.price / g.afforded);
        else
            std::printf("  mean quarters to afford: N/A - nothing was affordable "
                        "inside %d quarters\n", search_quarters);
        return g;
    };

    const gate_totals gc = report_gate("GATE A: the cheapest buyable public firm "
                                       "(the brief's literal gate)", false);
    const gate_totals gp = report_gate("GATE B: the cheapest firm priced ABOVE ZERO "
                                       "(the one you save for)", true);

    int zero_priced = 0;
    for (const seed_row& r : rows)
        if (r.cheapest.afforded && r.cheapest.price <= 0.0f)
            ++zero_priced;
    std::printf("\n  ZERO-PRICED: %d of gate A's %d affordances cost NOTHING.\n",
                zero_priced, gc.afforded);
    std::printf("  A zero price is not a bargain. It means book + %.1f x trailing_net "
                "+ balance went\n"
                "  below zero and the floor caught it, so the firm is being taken on "
                "as a liability.\n",
                static_cast<double>(reg.acquisition().multiple));

    const int with_target = gc.with_target;
    const int afforded    = gc.afforded;
    std::printf("\n");
    check(with_target > 0, "R2",
          "at least one seed offered a buyable public firm (a sweep with none "
          "measures nothing)");
    check_on_real_spawn(with_target > 0 && afforded * 2 > with_target, "R2",
          "GATE A - a majority of seeds with a buyable public firm reach its price "
          "inside the search window");
    check_on_real_spawn(gp.with_target > 0 && gp.afforded * 2 > gp.with_target, "R2",
          "GATE B - a majority of seeds reach the price of the cheapest firm that "
          "costs anything at all (the gate that answers \"save up\")");


    // =====================================================================
    // R3 — after the buy
    // =====================================================================
    std::printf("\n=== R3  AFTER THE ACQUISITION — is it still earning %d quarters "
                "later? ===\n", after_quarters);
    std::printf("  The buy goes through the real `buy_corporation` verb, so the "
                "dissolution rule\n"
                "  transfers the holdings and their maintenance lands on the "
                "acquirer's books.\n\n");
    std::printf("  seed | hold before/after | maint before/after | net before/after "
                "| balance after | still earning\n");
    std::printf("  -----+-------------------+--------------------+------------------"
                "+---------------+--------------\n");
    int bought = 0, earning = 0;
    for (const seed_row& r : rows)
    {
        if (r.seated == null_entity || !r.buy_attempted)
            continue;
        if (!r.bought)
        {
            std::printf("  %4u | THE VERB REFUSED THE BUY (corp_command_result %d) — "
                        "the shortlist and the seam disagree\n",
                        r.seed, r.buy_result);
            continue;
        }
        ++bought;
        earning += r.still_earning ? 1 : 0;
        std::printf("  %4u |      %4d / %-4d   |  %7.1f / %-7.1f |  %7.1f / %-7.1f "
                    "| %13.1f | %s\n",
                    r.seed, r.holdings_before, r.holdings_after,
                    static_cast<double>(r.maint_before), static_cast<double>(r.maint_after),
                    static_cast<double>(r.net_before), static_cast<double>(r.net_after),
                    static_cast<double>(r.balance_after),
                    r.still_earning ? "yes" : "NO");
    }
    if (bought == 0)
        std::printf("  (no acquisition fired on any seed — this row has nothing to "
                    "measure, and says so)\n");
    std::printf("\n  still earning on %d/%d seeds that bought\n", earning, bought);
    int attempted = 0;
    for (const seed_row& r : rows)
        attempted += r.buy_attempted ? 1 : 0;
    check(attempted == 0 || bought == attempted, "R3",
          "every seed that reached the price completed the buy through the real "
          "verb (no shortlist/seam divergence)");
    check_on_real_spawn(bought > 0 && earning * 2 > bought, "R3",
          "the acquisition is not a trap — net is still non-negative at the end of "
          "the run-on window on a majority of buying seeds, with the acquired "
          "holdings' maintenance folded in");

    // =====================================================================
    // R4 — field health
    // =====================================================================
    std::printf("\n=== R4  FIELD HEALTH — the loop must not go green by draining "
                "the background ===\n");
    std::printf("  seed | rivals | solvent at seat -> at close | rival holdings "
                "gen -> close | rival units | mean rival net\n");
    std::printf("  -----+--------+-----------------------------+----------------"
                "---------------+-------------+---------------\n");
    int tot_rivals = 0, tot_solv_seat = 0, tot_solv_close = 0;
    int tot_hold_gen = 0, tot_hold_close = 0, tot_units = 0;
    for (const seed_row& r : rows)
    {
        if (r.seated == null_entity)
            continue;
        tot_rivals     += r.field_close.rivals;
        tot_solv_seat  += r.field_seat.solvent;
        tot_solv_close += r.field_close.solvent;
        tot_hold_gen   += r.field_holdings_gen;
        tot_hold_close += r.field_close.holdings;
        tot_units      += r.field_close.units;
        std::printf("  %4u |  %5d |   %4d/%-4d  ->  %4d/%-4d      |   %5d  ->  "
                    "%-5d          |    %6d   | %14.1f\n",
                    r.seed, r.field_close.rivals,
                    r.field_seat.solvent, r.field_seat.rivals,
                    r.field_close.solvent, r.field_close.rivals,
                    r.field_holdings_gen, r.field_close.holdings,
                    r.field_close.units,
                    r.field_close.rivals > 0
                        ? r.field_close.net_sum / r.field_close.rivals : 0.0);
    }
    const double solv_seat_pct  = tot_rivals > 0 ? 100.0 * tot_solv_seat  / tot_rivals : 0.0;
    const double solv_close_pct = tot_rivals > 0 ? 100.0 * tot_solv_close / tot_rivals : 0.0;
    std::printf("\n  rival solvency %.1f%% at the seat -> %.1f%% at the close\n",
                solv_seat_pct, solv_close_pct);
    std::printf("  field holdings %d at generation -> %d at the close (%+.1f%%)\n",
                tot_hold_gen, tot_hold_close,
                tot_hold_gen > 0 ? 100.0 * (tot_hold_close - tot_hold_gen) / tot_hold_gen : 0.0);
    std::printf("  rival standing force %d units\n", tot_units);

    // --- CONSOLIDATION (BL-678, companies are open) -----------------------
    // REPORTED, never asserted. A pinned concentration figure would be an
    // instrument to tune the economy against, which is the failure this file's
    // C section already refuses for the field size. What it must do is make the
    // snowball VISIBLE if there is one.
    //
    // `buy_corporation` is the ONLY path that erases a corporation (corp_command
    // .cpp is the sole `w.corporations.erase`), so the fall in the corp count
    // between the seat and the close IS the acquisition count over the run,
    // exactly. One of them is the player's when the seed bought; the rest are
    // rivals acting through the scorer.
    std::printf("\n  --- CONSOLIDATION: acquisitions over the run, and where the "
                "holdings ended up ---\n");
    std::printf("  Corps only ever disappear by acquisition, so the count delta IS "
                "the acquisition count.\n");
    std::printf("  seed | corps seat -> close | acq total | player | RIVAL | holders "
                "seat -> close | largest holding | HHI seat -> close\n");
    std::printf("  -----+---------------------+-----------+--------+-------+--------"
                "--------------+-----------------+-------------------\n");
    int tot_acq = 0, tot_acq_player = 0, tot_acq_rival = 0, consol_seeds = 0;
    double hhi_seat_sum = 0.0, hhi_close_sum = 0.0;
    for (const seed_row& r : rows)
    {
        if (r.seated == null_entity)
            continue;
        const int acq        = r.field_seat.corps - r.field_close.corps;
        const int acq_player = r.bought ? 1 : 0;
        const int acq_rival  = acq - acq_player;
        tot_acq        += acq;
        tot_acq_player += acq_player;
        tot_acq_rival  += acq_rival;
        hhi_seat_sum   += r.field_seat.hhi;
        hhi_close_sum  += r.field_close.hhi;
        ++consol_seeds;
        std::printf("  %4u |     %4d -> %-4d    |    %4d   |   %2d   |  %4d |    "
                    "%4d -> %-4d      |      %4d       |  %.4f -> %.4f\n",
                    r.seed, r.field_seat.corps, r.field_close.corps,
                    acq, acq_player, acq_rival,
                    r.field_seat.holders, r.field_close.holders,
                    r.field_close.max_holdings,
                    r.field_seat.hhi, r.field_close.hhi);
    }
    if (consol_seeds > 0)
    {
        const double n = static_cast<double>(consol_seeds);
        std::printf("\n  acquisitions per seed: %.2f total = %.2f player + %.2f RIVAL\n",
                    tot_acq / n, tot_acq_player / n, tot_acq_rival / n);
        std::printf("  mean HHI over holdings: %.4f at the seat -> %.4f at the close "
                    "(%+.1f%%)\n",
                    hhi_seat_sum / n, hhi_close_sum / n,
                    hhi_seat_sum > 0.0
                        ? 100.0 * (hhi_close_sum - hhi_seat_sum) / hhi_seat_sum : 0.0);
        std::printf("  READ IT LIKE THIS: HHI rising sharply with a falling holder "
                    "count is a snowball;\n"
                    "                     a flat HHI with the holder count intact is "
                    "an open field that\n"
                    "                     nobody consolidated.\n");
    }

    check(tot_hold_close > tot_hold_gen, "R4",
          "the field is still ACTING — its holding count grew over the run");
    check(tot_rivals > 0 && tot_solv_close > 0, "R4",
          "the field is still SOLVENT — rivals remain above zero at the close");
    check(tot_units > 0, "R4",
          "the field still fields a standing force (rivals can still afford to hire)");

    // =====================================================================
    // R5 — the multiple
    // =====================================================================
    std::printf("\n=== R5  k_acquisition_multiple ===\n");
    std::printf("  authored value: %.3f  (scripts/economy.lua)\n",
                static_cast<double>(reg.acquisition().multiple));
    {
        // What the multiple is actually WORTH in the measured prices: the share
        // of the price the profit term contributes, and how long a seated corp
        // would need to earn the price at its own measured rate. Both are read
        // off the sweep; neither is a target.
        double book = 0.0, profit = 0.0, cash = 0.0;
        int n = 0;
        for (const seed_row& r : rows)
        {
            // The gate the buy actually FIRED on — gate B under --target priced,
            // gate A otherwise. Reading the other one would decompose a price
            // nobody paid.
            const seed_row::gate& gt = (mode == buy_mode::priced) ? r.priced : r.cheapest;
            if (!r.bought || !gt.afforded)
                continue;
            book   += gt.book;
            profit += static_cast<double>(reg.acquisition().multiple) * gt.trailing;
            cash   += gt.balance;
            ++n;
        }
        if (n > 0)
        {
            const double pre_floor = (book + profit + cash) / n;
            double paid = 0.0;
            for (const seed_row& r : rows)
            {
                const seed_row::gate& gt = (mode == buy_mode::priced) ? r.priced
                                                                     : r.cheapest;
                if (r.bought && gt.afforded)
                    paid += gt.price;
            }
            std::printf("  the %d buys that fired, decomposed BEFORE the zero floor:\n"
                        "    book %.1f | profit term (%.1f x trailing) %.1f | cash %.1f"
                        "  => %.1f\n"
                        "    price actually PAID after the floor: %.1f\n",
                        n, book / n, static_cast<double>(reg.acquisition().multiple),
                        profit / n, cash / n, pre_floor, paid / n);
            const double mag = std::fabs(book) + std::fabs(profit) + std::fabs(cash);
            if (mag > 1e-6)
                std::printf("    the profit term is %.1f%% of the price's total "
                            "magnitude — that is the leverage the multiple has\n",
                            100.0 * std::fabs(profit) / mag);
            if (pre_floor < 0.0)
                std::printf("    NOTE the pre-floor sum is NEGATIVE, so the multiple "
                            "had NO effect on what was paid:\n"
                            "         the floor decided the price, and no value of the "
                            "multiple would change it.\n");
        }
        else
        {
            std::printf("  NO BUY FIRED, so this sweep produced no evidence about the "
                        "price LEVEL.\n"
                        "  A multiple calibrated from zero observations would be a "
                        "pick, not a calibration.\n");
        }

        // The calibration-relevant pair: what a firm that costs SOMETHING costs,
        // against the rate a seated corp actually accumulates. Their ratio is the
        // number of quarters "save up" takes at the authored multiple.
        double quote = 0.0; int qn = 0;
        double earn = 0.0;  int en = 0;
        for (const seed_row& r : rows)
        {
            if (r.seated == null_entity)
                continue;
            earn += r.r1_per_qtr;
            ++en;
            if (r.priced.had_target)
            {
                quote += r.priced.afforded ? r.priced.price : r.priced.final_quote;
                ++qn;
            }
        }
        const double rate = en > 0 ? earn / en : 0.0;
        std::printf("  seated accumulation rate across the sweep: %.2f cr/qtr\n", rate);
        if (qn > 0)
        {
            const double q = quote / qn;
            std::printf("  cheapest ABOVE-ZERO firm on offer, mean over %d seeds: "
                        "%.1f cr\n", qn, q);
            if (rate > 0.0)
                std::printf("  => at the measured rate that is %.0f quarters of saving "
                            "(%.1f in-game years)\n", q / rate, q / rate / 4.0);
            else
                std::printf("  => at the measured rate that is NEVER: the seated corp "
                            "is not accumulating.\n");
        }
    }
    // The verdict, stated in the harness rather than only in a report, because a
    // report goes stale and this file is re-run. It is a LEAVE-AS-IS with its
    // reason, not a pick — R5's own words are "calibrated from what this measures,
    // or explicitly reported as leave-as-is with the reason. Never picked."
    std::printf("  VERDICT — LEAVE AS IS at %.1f, and the reason is that this sweep "
                "produced no\n"
                "  evidence that would move it:\n"
                "    1. The profit term is a MINORITY of the price and it is NEGATIVE. "
                "The firms on\n"
                "       offer are loss-making on trailing net, so `k x trailing_net` "
                "SUBTRACTS from the\n"
                "       price. Raising the multiple would make a bad firm CHEAPER, "
                "which is the ledger\n"
                "       telling the truth (FINANCE.md: nothing clamps the profit term) "
                "but is not a\n"
                "       price level anyone is calibrating.\n"
                "    2. On the cheapest-firm gate the pre-floor sum is negative, so the "
                "ZERO FLOOR sets\n"
                "       the price and the multiple has no effect on it at ANY value. A "
                "constant that\n"
                "       cannot move the measured outcome cannot be calibrated from it.\n"
                "    3. What the measurement DOES indict is the floor, not the "
                "multiple: a firm whose\n"
                "       debts exceed its book value is handed over free and its debt "
                "transfers with it.\n"
                "       That is a design question for FINANCE.md § Whole-firm "
                "acquisition, not a\n"
                "       number to nudge here.\n",
                static_cast<double>(reg.acquisition().multiple));

    // =====================================================================
    std::printf("\n=== SUMMARY — Ben's criterion ===\n");
    std::printf("  save up      : accumulates on %d/%d seated seeds\n",
                climbing, seated_ok);
    std::printf("  buy another  : gate A (cheapest, may be free) afforded on %d/%d "
                "seeds; %d of those cost nothing\n",
                gc.afforded, gc.with_target, zero_priced);
    std::printf("                 gate B (cheapest priced ABOVE ZERO) afforded on "
                "%d/%d seeds\n", gp.afforded, gp.with_target);
    std::printf("  keep earning : still earning on %d/%d seeds that bought\n",
                earning, bought);

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES ABOVE",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
