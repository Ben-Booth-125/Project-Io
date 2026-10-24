// quarterly_return — the BL-626 filed balance sheet (requirement group
// `quarterly-return`, rows R1-R6). No SDL / Lua / ImGui.
//
// Build and run (from the repo root):
//   node tools/verify/build_harness.js quarterly_return --run
//
// WHAT IS BEING CHECKED, and why each row is shaped the way it is.
//
//   R1  apply_budget files ONE return per corporation per econ tick, carrying
//       corp_budget's seven flows, the delta, the closing balance,
//       assets.size() and book_value. Checked field by field against the
//       optional breakdown sink for the same tick — and then checked AGAIN with
//       the sink absent, because the sink is optional and every headless caller
//       omits it. A record that only existed when a UI asked for one would be
//       the second computation this deliberately is not.
//
//   R2  THE RETAIN PROPERTY. The sum of a corp's filed `net` equals the balance
//       delta exactly — not to a tolerance, exactly.
//
//       "Balance delta" is measured ACROSS THE apply_budget CALL, and that
//       precision is load-bearing rather than a softening. A corporation's
//       balance also moves in construction.cpp, corp_command.cpp (hire, order
//       deposits), supply_system.cpp (convoy legs), survey_system.cpp and
//       nation_step.cpp — none of which a quarterly return claims to record,
//       and all of which run outside apply_budget. The return is a retain of
//       THE MONEY LOOP; the property that keeps it honest is that it cannot
//       disagree with the loop that wrote it. P2a nonetheless asserts the
//       stronger whole-run form on the isolated fixture, where apply_budget is
//       the only thing that touches a balance at all, so the weaker measurement
//       is never doing the work on its own.
//
//       Exactness is possible because `quarterly_return::net` is the DIFFERENCE
//       of two consecutive balances rather than a re-grouped sum of the flows,
//       taken in DOUBLE (BL-1337: a float difference rounds when a balance
//       crosses zero, so a float `net` could not hold the delta at all).
//       Each term is therefore exact, and a double-precision sum of exact
//       differences telescopes: every partial sum b_k - b_0 is itself
//       representable, so IEEE addition returns it unrounded.
//
//   R3  Retention is exactly 40 quarters (`k_quarterly_return_retention`),
//       oldest dropped first. Checked by recording every tick's closing balance
//       and asserting the retained window is the LAST 40 of that sequence, in
//       order — which a wrong-end trim or a ring-buffer rotation would fail.
//
//   R4  book_value is the recipe registry's flat `build_cost` summed over the
//       holdings — historical cost, and deliberately NOT the build press's
//       charge (which adds a market-priced materials term). Checked against an
//       independent sum, with a non-vacuity guard (non-zero, and different for
//       corps holding different building sets).
//
//   R5  The returns round-trip the save. Every assertion names
//       `world_save_version` SYMBOLICALLY: BL-631 is claiming the next version
//       on another branch this same wave, so a literal here would break at the
//       stack. A stream one version older is refused whole with the destination
//       untouched.
//
//   R6  THE ITEM-SPANNING GATE. End to end over the settle — phase 6's
//       validation run, 12 economy ticks (`app::validation_ticks`) on a
//       really generated world: every corporation has a filed history, each
//       corp's last return's balance is bit-identical to its live
//       corporation_component.balance, and two identical settles produce
//       byte-identical records. The same world then runs on past the
//       40-quarter retention — the harness's own choice, not the game's; see
//       the note at k_past_retention_ticks — so the cap and the trimmed-window
//       telescoping are exercised on a real world and not only the fixture.
//
// REGISTRY. This harness is Lua-free by construction (build_harness.js excludes
// the sol2 TUs), so the economics are restated here as constants taken from
// scripts/economy.lua — the same treatment sea_leg_census gives the logistics
// rates, and for the same reason. If economy.lua's build_cost / maintenance /
// base_wage figures move, these move with them; nothing here asserts a shipped
// number, only that the record retains whatever the loop charged.
//
// Exits 0 on PASS, non-zero on any failure.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/market_clearing.hpp"
#include "world/nation_step.hpp" // BL-1335: run_nation_step (R7)
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/trade.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include "harness_params.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <sstream>
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

std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

// --- the shipped economy.lua figures this harness needs -------------------
// Only the three per-type scalars the money loop reads, plus build_cost (the
// number R4 is about). Restated, not loaded: see the header note.
void author_economics(recipe_registry& reg)
{
    building_economics ex;
    ex.base_rate = 20.0f; ex.maintenance = 5.0f; ex.base_wage = 8.0f; ex.build_cost = 100.0f;
    reg.set_economics(building_type::extraction_site, ex);

    building_economics pr;
    pr.base_rate = 8.0f;  pr.maintenance = 10.0f; pr.base_wage = 12.0f; pr.build_cost = 200.0f;
    reg.set_economics(building_type::processing_facility, pr);

    building_economics po;
    po.maintenance = 4.0f; po.base_wage = 6.0f; po.build_cost = 500.0f;
    reg.set_economics(building_type::port, po);
}

// ---------------------------------------------------------------------------
// Fixture A — the isolated world
// ---------------------------------------------------------------------------
// Two corporations on one body, both flagged is_player so corp_ai never
// commands them (the supported "not AI-driven" exclusion econ_harness uses).
// Nothing here builds, hires, dispatches or surveys, so apply_budget is the ONLY
// thing in the tick that moves a balance — which is what lets P2a assert the
// strong whole-run form of the retain property.
struct fixture
{
    world     w;
    entity_id corp_a = null_entity; // 2 extraction sites  -> book 200
    entity_id corp_b = null_entity; // 1 processing + 1 port -> book 700
};

fixture build_fixture(const recipe_registry& reg)
{
    fixture f;
    world&  w = f.w;

    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};
    w.bodies[body].name = "Fixture";

    const entity_id market = w.create_entity();
    {
        market_component mc;
        mc.body = body;
        mc.base_price[ri(resource_type::iron_ore)] = 2.5f;
        mc.base_price[ri(resource_type::steel)]    = 8.0f;
        mc.price = mc.base_price;
        mc.inventory[ri(resource_type::iron_ore)] = 1000.0f;
        w.markets[market] = mc;
    }

    const auto make_tile = [&](float deposit) {
        const entity_id t = w.create_entity();
        tile_component tc{};
        tc.body      = body;
        tc.substrate = terrain_substrate::rocky;
        tc.resource_deposit[ri(resource_type::iron_ore)]   = deposit;
        tc.resource_remaining[ri(resource_type::iron_ore)] = 1.0e6f;
        w.tiles[t] = tc;
        return t;
    };

    const auto make_building = [&](building_type type, float workforce) {
        const entity_id b = w.create_entity();
        building_component bc{};
        bc.tile               = make_tile(2.0f);
        bc.type               = type;
        bc.workforce_assigned = workforce;
        bc.workforce_target   = 100.0f;
        bc.target_resource    = resource_type::iron_ore;
        w.buildings[b] = bc;
        return b;
    };

    const entity_id a1 = make_building(building_type::extraction_site, 0.5f);
    const entity_id a2 = make_building(building_type::extraction_site, 0.4f);
    const entity_id b1 = make_building(building_type::processing_facility, 0.6f);
    const entity_id b2 = make_building(building_type::port, 0.2f);
    w.buildings[b1].recipe = reg.recipe_id("steel");

    const auto make_corp = [&](const char* name, float capital,
                               std::vector<entity_id> assets) {
        const entity_id c = w.create_entity();
        corporation_component cc;
        cc.name             = name;
        cc.starting_capital = capital;
        cc.balance          = capital;
        cc.assets           = std::move(assets);
        cc.is_player        = true; // not AI-driven; see the fixture comment
        w.corporations[c] = cc;
        return c;
    };

    f.corp_a = make_corp("Ashfall Extraction", 5000.0f, {a1, a2});
    f.corp_b = make_corp("Berevin Works",      3000.0f, {b1, b2});
    return f;
}

/// One economy tick. `breakdown` is the OPTIONAL sink — passed for the R1 field
/// comparison, omitted everywhere else so the return is proved not to depend on
/// it. Matches the composition player_seed_sweep and pregame_balance_harness run.
void tick(world& w, const recipe_registry& reg, int t,
          std::map<entity_id, corp_budget>* breakdown = nullptr)
{
    w.current_econ_tick = t;
    advance_convoys(w);
    credit_arrived_convoys(w, t); // app order: arrivals before the economy
    economy_report report = run_economy_step(w, reg);
    run_trades(w, reg, report); // BL-1266: the trade pass (was dispatch_convoys), before the clear
    const auto flows = clear_markets(w, reg, report);
    apply_budget(w, reg, flows, report.workforce_contention, breakdown);
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

float book_of(const world& w, const recipe_registry& reg, entity_id corp)
{
    float sum = 0.0f;
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return sum;
    for (const entity_id bid : cit->second.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        sum += reg.economics(bit->second.type).build_cost;
    }
    return sum;
}

/// Every corp's whole filed history, flattened in ascending entity_id, as raw
/// bytes. Two runs that agree here agree record for record, field for field.
std::string returns_image(const world& w)
{
    std::string bytes;
    for (const entity_id id : sorted_corp_ids(w))
    {
        const auto& hist = w.corporations.at(id).returns;
        const char* p = reinterpret_cast<const char*>(hist.data());
        bytes.append(p, hist.size() * sizeof(quarterly_return));
    }
    return bytes;
}

std::string to_bytes(const world& w)
{
    std::ostringstream out(std::ios::binary);
    write_world_snapshot(w, out);
    return out.str();
}

// ---------------------------------------------------------------------------
// R1 / R2a / R4 — the isolated fixture
// ---------------------------------------------------------------------------
void run_fixture_rows(const recipe_registry& reg)
{
    std::printf("\nFixture A — isolated world, apply_budget is the only balance mover\n");

    // --- R1: one tick with the breakdown sink, compared field for field ----
    {
        fixture f = build_fixture(reg);
        std::map<entity_id, float> opening;
        for (const entity_id id : sorted_corp_ids(f.w))
            opening[id] = f.w.corporations.at(id).balance;

        std::map<entity_id, corp_budget> sink;
        tick(f.w, reg, 0, &sink);

        bool one_each = true, flows_match = true, net_exact = true;
        bool bal_match = true, holdings_match = true, book_match = true;
        for (const entity_id id : sorted_corp_ids(f.w))
        {
            const corporation_component& cc = f.w.corporations.at(id);
            one_each = one_each && cc.returns.size() == 1;
            if (cc.returns.empty())
                continue;
            const quarterly_return& q = cc.returns.back();
            const corp_budget&      b = sink.at(id);

            flows_match = flows_match
                && q.income == b.income && q.expenditure == b.expenditure
                && q.maintenance == b.maintenance && q.wages == b.wages
                && q.interest == b.interest && q.levies == b.levies
                && q.upkeep == b.upkeep;
            net_exact      = net_exact
                          && q.net == static_cast<double>(cc.balance)
                                    - static_cast<double>(opening.at(id));
            bal_match      = bal_match && q.balance == cc.balance;
            holdings_match = holdings_match
                          && q.holdings == static_cast<uint32_t>(cc.assets.size());
            book_match     = book_match && q.book_value == book_of(f.w, reg, id);
        }
        check(one_each,       "R1", "one return filed per corporation per econ tick");
        check(flows_match,    "R1", "the seven flows are bit-identical to corp_budget's");
        check(net_exact,      "R1", "net is the exact balance delta this tick");
        check(bal_match,      "R1", "balance is the closing corporation_component.balance");
        check(holdings_match, "R1", "holdings is assets.size()");
        check(book_match,     "R1", "book_value is the registry build_cost over the holdings");
    }

    // --- R1: the sink is OPTIONAL; the record is not ------------------------
    {
        fixture with_sink = build_fixture(reg);
        fixture no_sink   = build_fixture(reg);
        std::map<entity_id, corp_budget> sink;
        for (int t = 0; t < 5; ++t)
        {
            sink.clear();
            tick(with_sink.w, reg, t, &sink);
            tick(no_sink.w,   reg, t, nullptr);
        }
        const bool same = returns_image(with_sink.w) == returns_image(no_sink.w)
                       && !returns_image(no_sink.w).empty();
        check(same, "R1", "the filed record is byte-identical with the breakdown sink absent");
    }

    // --- R2a: the strong retain property, whole run ------------------------
    // 40 ticks, so retention drops nothing and the retained window IS the run.
    {
        fixture f = build_fixture(reg);
        std::map<entity_id, double> initial;
        for (const entity_id id : sorted_corp_ids(f.w))
            initial[id] = static_cast<double>(f.w.corporations.at(id).balance);

        for (int t = 0; t < 40; ++t)
            tick(f.w, reg, t);

        bool exact = true, non_trivial = false;
        for (const entity_id id : sorted_corp_ids(f.w))
        {
            const corporation_component& cc = f.w.corporations.at(id);
            double sum = 0.0;
            for (const quarterly_return& q : cc.returns)
                sum += static_cast<double>(q.net);
            const double delta = static_cast<double>(cc.balance) - initial.at(id);
            exact = exact && cc.returns.size() == 40 && sum == delta;
            non_trivial = non_trivial || delta != 0.0;
        }
        check(exact,       "R2", "sum of 40 filed nets == the whole-run balance delta, EXACTLY");
        check(non_trivial, "R2", "the run actually moved a balance (not vacuously zero)");
    }

    // --- R3: rolling 40, oldest dropped first ------------------------------
    {
        fixture f = build_fixture(reg);
        constexpr int k_ticks = 55; // 15 more than the retention
        std::map<entity_id, std::vector<float>> closing; // per tick, in order
        for (int t = 0; t < k_ticks; ++t)
        {
            tick(f.w, reg, t);
            for (const entity_id id : sorted_corp_ids(f.w))
                closing[id].push_back(f.w.corporations.at(id).balance);
        }

        bool capped = true, window_ok = true;
        for (const entity_id id : sorted_corp_ids(f.w))
        {
            const corporation_component& cc = f.w.corporations.at(id);
            capped = capped && cc.returns.size() == k_quarterly_return_retention;
            if (cc.returns.size() != k_quarterly_return_retention)
                continue;
            const std::vector<float>& seq = closing.at(id);
            const std::size_t first = seq.size() - k_quarterly_return_retention;
            for (std::size_t i = 0; i < k_quarterly_return_retention; ++i)
                window_ok = window_ok && cc.returns[i].balance == seq[first + i];
        }
        check(capped,    "R3", "a corp run past the retention holds exactly 40 returns");
        check(window_ok, "R3", "the retained 40 are the LAST 40, in order (oldest dropped first)");
    }

    // --- R4: book_value is flat build_cost (historical), and is not a constant --
    {
        fixture f = build_fixture(reg);
        tick(f.w, reg, 0);
        const float a = f.w.corporations.at(f.corp_a).returns.back().book_value;
        const float b = f.w.corporations.at(f.corp_b).returns.back().book_value;
        const float want_a = reg.economics(building_type::extraction_site).build_cost * 2.0f;
        const float want_b = reg.economics(building_type::processing_facility).build_cost
                           + reg.economics(building_type::port).build_cost;
        check(a == want_a && b == want_b, "R4",
              "book_value == sum of economics(type).build_cost over the holdings");
        check(a > 0.0f && b > 0.0f && a != b, "R4",
              "book_value is non-zero and differs with the holding set (not vacuous)");
        std::printf("       corp A book %.1f (want %.1f), corp B book %.1f (want %.1f)\n",
                    static_cast<double>(a), static_cast<double>(want_a),
                    static_cast<double>(b), static_cast<double>(want_b));
    }
}

// ---------------------------------------------------------------------------
// R7 — BL-1335: the tax is in the SAME quarter's filed return
// ---------------------------------------------------------------------------
// The fixture's two corporations are given a home nation at tier V and run 20
// live quarters (past the settle, so the tax is on) through apply_budget AND
// run_nation_step. The tax is taken inside apply_budget before the return is
// filed, so the sum of the filed nets equals the real balance change across the
// whole run EXACTLY — the nation step moves no balance here (no budget, no
// claims) — every return's flows, taxes included, rebuild its net, and the
// treasury rose by exactly the taxes the returns booked.
void run_tax_rows(const recipe_registry& reg)
{
    std::printf("\nR7 — BL-1335: the tax lands in the filed return, run_nation_step included\n");
    fixture f = build_fixture(reg);
    const entity_id nation = f.w.create_entity();
    f.w.nations[nation] = nation_component{};
    f.w.nations[nation].name     = "Fixture Nation";
    f.w.nations[nation].tax_tier = 5;
    for (auto& [cid, cc] : f.w.corporations)
        cc.home_nation = nation;

    std::map<entity_id, double> initial;
    for (const entity_id id : sorted_corp_ids(f.w))
        initial[id] = static_cast<double>(f.w.corporations.at(id).balance);
    const double treasury0 = static_cast<double>(f.w.nations.at(nation).treasury);

    for (int t = 0; t < 20; ++t)
    {
        const int econ = k_campaign_settle_ticks + t;
        f.w.current_econ_tick = econ;
        advance_convoys(f.w);
        credit_arrived_convoys(f.w, econ);
        economy_report report = run_economy_step(f.w, reg);
        run_trades(f.w, reg, report);
        auto flows = clear_markets(f.w, reg, report);
        // The fixture has no shelf its firms sell onto, so the market income is
        // supplied: 100 a quarter to each firm, the term the tax is levied on.
        for (const entity_id id : sorted_corp_ids(f.w))
            flows[id].income += 100.0f;
        apply_budget(f.w, reg, flows, report.workforce_contention, &report.budgets,
                     &report.buildings, &report.building_labour);
        run_nation_step(f.w, reg, report, econ);
    }

    bool exact = true, rebuild = true;
    double taxes = 0.0;
    for (const entity_id id : sorted_corp_ids(f.w))
    {
        const corporation_component& cc = f.w.corporations.at(id);
        double sum = 0.0;
        for (const quarterly_return& q : cc.returns)
        {
            sum   += static_cast<double>(q.net);
            taxes += static_cast<double>(q.taxes);
            const double flows_net = static_cast<double>(q.income) - q.expenditure - q.maintenance
                                   - q.wages - q.interest - q.levies - q.upkeep - q.taxes + q.refunds;
            rebuild = rebuild && std::fabs(flows_net - q.net) <= 1e-3 * std::max(1.0, std::fabs(static_cast<double>(q.net)));
        }
        exact = exact && cc.returns.size() == 20
             && sum == static_cast<double>(cc.balance) - initial.at(id);
    }
    for (const entity_id id : sorted_corp_ids(f.w))
    {
        double inc = 0.0, out = 0.0;
        for (const quarterly_return& q : f.w.corporations.at(id).returns)
        {
            inc += q.income;
            out += static_cast<double>(q.expenditure) + q.maintenance + q.wages + q.interest + q.levies + q.upkeep;
        }
        std::printf("       corp %u: income %.2f, outflows %.2f over the run\n", static_cast<unsigned>(id), inc, out);
    }
    const double raised = static_cast<double>(f.w.nations.at(nation).treasury) - treasury0;
    char buf[200];
    std::snprintf(buf, sizeof buf, "the run taxed something (%.4f credits over 20 quarters at tier V)", taxes);
    check(taxes > 0.0, "R7", buf);
    check(exact, "R7", "sum of 20 filed nets == the real balance change with the tax and run_nation_step on, EXACTLY");
    check(rebuild, "R7", "every filed return's flows, taxes included, rebuild its net");
    std::snprintf(buf, sizeof buf, "the treasury rose by the returns' taxes (%.4f vs %.4f)", raised, taxes);
    check(std::fabs(raised - taxes) <= 1e-3 * std::max(1.0, taxes), "R7", buf);
}

// ---------------------------------------------------------------------------
// R5 — the serialisation seam
// ---------------------------------------------------------------------------
void run_save_rows(const recipe_registry& reg)
{
    std::printf("\nR5 — the save seam (version referenced symbolically: %u)\n",
                static_cast<unsigned>(world_save_version));

    fixture f = build_fixture(reg);
    for (int t = 0; t < 45; ++t) // past the retention, so a trimmed history round-trips
        tick(f.w, reg, t);

    const std::string bytes = to_bytes(f.w);

    // P1 — the returns survive the round trip, record for record.
    {
        world back;
        std::istringstream in(bytes, std::ios::binary);
        const bool ok = read_world_snapshot(back, in);
        bool same = ok && !returns_image(f.w).empty();
        same = same && returns_image(back) == returns_image(f.w);
        check(same, "R5", "every filed return round-trips the snapshot byte-identically");

        // And the re-serialisation agrees, which no hand-written walker can fake.
        check(ok && to_bytes(back) == bytes, "R5",
              "write -> read -> write is byte-equal (read and write sides agree)");
    }

    // P1b — BL-1337: an exact net NO FLOAT CAN HOLD survives the round trip
    // through the exact-net section; a stream cut to the predecessor's prefix
    // reads it back as its f32 slot; and an exact net that disagrees with its
    // own slot is refused whole. Planted on purpose: the fixture's nets may all
    // be float-exact, which would leave the section empty and these rows vacuous.
    {
        world x = f.w;
        const entity_id cid = sorted_corp_ids(x).front();
        quarterly_return& q = x.corporations.at(cid).returns.back();
        const float b0 = 17.7972374f, b1 = -1.70437169f; // the t=24 crossing row
        q.net = static_cast<double>(b1) - static_cast<double>(b0);
        const double exact = q.net;
        const double slot  = static_cast<double>(static_cast<float>(exact));
        check(slot != exact, "R5", "the planted net is one a float cannot hold (not vacuous)");

        const std::string xb = to_bytes(x);
        {
            world back;
            std::istringstream in(xb, std::ios::binary);
            const bool ok = read_world_snapshot(back, in);
            check(ok && back.corporations.at(cid).returns.back().net == exact
                     && to_bytes(back) == xb,
                  "R5", "an exact net round-trips bit for bit (the exact-net section)");
        }

        const std::size_t sec = world_save_net_section_bytes(x);
        const std::size_t n   = x.corporations.at(cid).returns.size();
        check(sec == 4 + 4 + 4 + 8 * n, "R5",
              "the exact-net section carries exactly the one inexact history");
        constexpr std::size_t k_version_offset = sizeof(uint32_t);
        {
            // BL-1404: less the v55 majors' section that follows the nets.
            std::string pre = xb.substr(0, xb.size() - world_save_major_section_bytes(x) - sec);
            const uint32_t v = k_world_save_version_pre_net;
            std::memcpy(pre.data() + k_version_offset, &v, sizeof v);
            world back;
            std::istringstream in(pre, std::ios::binary);
            const bool ok = read_world_snapshot(back, in);
            check(ok && back.corporations.at(cid).returns.back().net == slot, "R5",
                  "the predecessor's prefix still reads, each net as its f32 slot");
        }
        {
            std::string bad = xb; // the last 8 bytes before the v55 majors' section are this net
            const double wrong = exact + 1.0;
            std::memcpy(bad.data() + bad.size() - world_save_major_section_bytes(x) - sizeof wrong,
                        &wrong, sizeof wrong);
            world sentinel = build_fixture(reg).w;
            const std::string before = returns_image(sentinel);
            std::istringstream in(bad, std::ios::binary);
            check(!read_world_snapshot(sentinel, in) && returns_image(sentinel) == before,
                  "R5", "an exact net its own f32 slot contradicts is REFUSED, destination untouched");
        }
    }

    // P2 — a stream at the PRIOR version is refused whole, destination untouched.
    {
        std::string older = bytes;
        // BL-1296: v47 is a strict prefix of v48 and still read, so the prior
        // version refused is the one before the accepted predecessor.
        const uint32_t prior = k_world_save_version_pre_state - 1u; // symbolic; never a literal
        // The header is magic then version, so the version u32 sits at offset 4.
        // The offset is asserted rather than searched for: a scan would happily
        // rewrite some unrelated field that happened to hold the same number and
        // then "pass" on a refusal that had nothing to do with the version.
        constexpr std::size_t k_version_offset = sizeof(uint32_t);
        uint32_t stamped = 0;
        bool patched = older.size() > k_version_offset + sizeof(uint32_t);
        if (patched)
        {
            std::memcpy(&stamped, older.data() + k_version_offset, sizeof stamped);
            patched = stamped == world_save_version;
            std::memcpy(older.data() + k_version_offset, &prior, sizeof prior);
        }

        world sentinel = build_fixture(reg).w;
        const std::string before = returns_image(sentinel);
        std::istringstream in(older, std::ios::binary);
        const bool read_ok = read_world_snapshot(sentinel, in);
        check(patched && !read_ok, "R5",
              "a stream at the newest unread prior version is REFUSED whole");
        check(returns_image(sentinel) == before, "R5",
              "the refused read left the destination world untouched");
    }
}

// ---------------------------------------------------------------------------
// R6 — the item-spanning gate: the settle, then the same world past retention
// ---------------------------------------------------------------------------
// TWO LENGTHS, AND ONLY ONE OF THEM IS THE GAME'S (BL-1008 re-read, 2026-09-16).
//
// k_settle_ticks mirrors `app::validation_ticks` — phase 6's single validation
// run, whose balances and filed returns ARE the opening position play receives
// (ERAS.md § The opening position). The handoff rows and the byte-identity row
// are read there, because that is the record the seat is drawn from. Restated,
// not included: app.hpp brings SDL, and this harness is SDL-free by
// construction. If app::validation_ticks moves, this moves with it.
//
// k_past_retention_ticks is THIS HARNESS'S OWN CHOICE, not the game's. At the
// settle every history is twelve returns long, under the 40-quarter retention,
// so on a generated world the retention row would pass on nothing and the
// retained-window telescoping below would never trim — its offset into the
// measured deltas would be zero. Running the same world on to retention plus
// one settle makes both bite on a world where construction, hiring, convoys and
// surveys all move balances, with the settle's own quarters rolled out of every
// history by the close. R3 proves the trim on the fixture; this proves it holds
// under the real tick.
constexpr int k_settle_ticks         = 12;
constexpr int k_past_retention_ticks =
    static_cast<int>(k_quarterly_return_retention) + k_settle_ticks;

world settled_world(const recipe_registry& reg, uint32_t seed)
{
    world_params p = no_prehistory();
    p.seed = seed;
    world w = make_hard_coded_world(p);
    assign_default_recipes(w, reg);
    generate_background_firms(w, reg, seed ^ 0x8A21F00Du);
    for (int t = 0; t < k_settle_ticks; ++t)
        tick(w, reg, t);
    return w;
}

/// Every corp's history length, and whether its last return is its live balance.
struct history_read
{
    bool        every_files  = true;
    bool        capped       = true;
    bool        last_is_live = true;
    std::size_t shortest     = static_cast<std::size_t>(-1);
    std::size_t longest      = 0;
};

history_read read_histories(const world& w, const std::vector<entity_id>& ids)
{
    history_read h;
    for (const entity_id id : ids)
    {
        const corporation_component& cc = w.corporations.at(id);
        h.every_files = h.every_files && !cc.returns.empty();
        h.capped      = h.capped && cc.returns.size() <= k_quarterly_return_retention;
        h.shortest    = std::min(h.shortest, cc.returns.size());
        h.longest     = std::max(h.longest, cc.returns.size());
        if (cc.returns.empty())
            continue;
        h.last_is_live = h.last_is_live && cc.returns.back().balance == cc.balance;
    }
    return h;
}

void run_settle_rows(const recipe_registry& reg)
{
    std::printf("\nR6 — end to end over the settle (%d econ ticks, app::validation_ticks), "
                "then the same world on to %d ticks (past the %zu-quarter retention)\n",
                k_settle_ticks, k_past_retention_ticks, k_quarterly_return_retention);

    // The retain property has to be measured across the apply_budget CALL here:
    // the real world builds, hires, dispatches and surveys, and every one of
    // those moves a balance outside the money loop. See the header note on R2.
    world_params p = no_prehistory();
    p.seed = 7;
    world w = make_hard_coded_world(p);
    assign_default_recipes(w, reg);
    generate_background_firms(w, reg, 7u ^ 0x8A21F00Du);

    // Per corp, the apply_budget movement measured tick by tick — the loop's own
    // delta, taken immediately either side of the call.
    std::map<entity_id, std::vector<double>> measured;
    bool per_tick_exact = true; // every filed net == that tick's measured delta
    int  per_tick_rows  = 0;

    for (int t = 0; t < k_past_retention_ticks; ++t)
    {
        w.current_econ_tick = t;
        advance_convoys(w);
        credit_arrived_convoys(w, t); // app order: arrivals before the economy
        economy_report report = run_economy_step(w, reg);
        run_trades(w, reg, report); // BL-1266: the trade pass (was dispatch_convoys), before the clear
        const auto flows = clear_markets(w, reg, report);

        std::map<entity_id, float> before;
        for (const auto& kv : w.corporations)
            before[kv.first] = kv.second.balance;
        apply_budget(w, reg, flows, report.workforce_contention);
        for (const auto& [id, b0] : before)
        {
            const auto cit = w.corporations.find(id);
            if (cit == w.corporations.end() || cit->second.returns.empty())
            {
                per_tick_exact = false; // a corp that did not file is a failure
                continue;
            }
            const double d = static_cast<double>(cit->second.balance)
                           - static_cast<double>(b0);
            measured[id].push_back(d);

            // ASSERTED BY REPLAYING THE ADDITION, not by differencing it, and the
            // difference is not cosmetic (NR-720, measured 2026-08-30).
            //
            // This line used to read `(double)net == d`, and it FAILED on main with
            // no owner, on a check whose subject is the figure the profitability
            // ledger prints and the acquisition price is read off. Measured: 7 of 640
            // rows, max absolute error 9.2e-05 Cr, max RELATIVE error 3.89e-08 - which
            // is float epsilon (2^-24 = 5.96e-08). `net` and `balance` are both floats;
            // `d` is an exact DOUBLE difference of two floats. Demanding bit equality
            // between them tests whether the sum happened to be representable, not
            // whether the books agree.
            //
            // The invariant actually worth asserting is that the filed net is exactly
            // what apply_budget added, so replay the float addition and require the
            // stored balance back, bit for bit. NO TOLERANCE - this is stricter than
            // an epsilon, not looser, and it passed on all 640 rows the moment it was
            // tried. The explicit cast pins the result to float against a
            // wider-evaluation FLT_EVAL_METHOD.
            //
            // BL-1337 (2026-10-16): the replay then FAILED too — 3 of 416 rows,
            // each a balance CROSSING ZERO with interest charged (t=24: b0 17.797,
            // close -1.70437169, float net -19.5016098 replays to -1.70437241). A
            // float `net` cannot hold a delta that needs more significand than a
            // float has, so neither form could pass while the record was float.
            // `net` is now the DOUBLE difference, and BOTH forms are asserted: the
            // replay, and the original `net == d` bit equality NR-720 had to drop.
            const double net_d = cit->second.returns.back().net;
            per_tick_exact = per_tick_exact && net_d == d
                          && static_cast<float>(static_cast<double>(b0) + net_d)
                                 == cit->second.balance;
            ++per_tick_rows;
        }

        // --- the handoff: the record play receives -------------------------
        if (t + 1 == k_settle_ticks)
        {
            const std::vector<entity_id> ids = sorted_corp_ids(w);
            check(!ids.empty(), "R6", "the generated world actually holds corporations");
            const history_read h = read_histories(w, ids);
            std::printf("       at the settle: %zu corporations, history length %zu..%zu\n",
                        ids.size(), h.shortest, h.longest);
            check(h.every_files, "R6", "every corporation has a filed history at the settle");
            check(h.last_is_live, "R6",
                  "at the settle, each corp's last return's balance == its live "
                  "corporation_component.balance");
        }
    }

    const std::vector<entity_id> ids = sorted_corp_ids(w);
    const history_read h = read_histories(w, ids);
    std::printf("       at tick %d: %zu corporations, history length %zu..%zu (retention %zu)\n",
                k_past_retention_ticks, ids.size(), h.shortest, h.longest,
                k_quarterly_return_retention);
    check(h.every_files, "R6", "every corporation still has a filed history past retention");
    check(h.capped,      "R6", "no history exceeds the retention");
    check(h.longest == k_quarterly_return_retention, "R6",
          "the run reached the retention (the cap row above is not vacuous)");
    check(h.last_is_live, "R6",
          "past retention, each corp's last return's balance == its live balance");

    // R2 on the real world: EVERY filed net is EXACTLY what apply_budget added to
    // the balance that tick. This is the record checked against the loop rather
    // than against itself, on a world where construction, hiring, convoys,
    // surveys and the market all run. See the replay note at the comparison for
    // why it is stated as an addition rather than as a difference.
    check(per_tick_exact && per_tick_rows > 0, "R2",
          "every filed net is EXACTLY what apply_budget added to the balance");
    std::printf("       %d filed rows replayed against the loop's own addition\n", per_tick_rows);

    // ...and the retained window's sum telescopes onto the same measured
    // deltas over that window, exactly. Past retention the window is TRIMMED, so
    // the offset into `measured` is non-zero — the case the settle alone never
    // reaches, and the reason this run is longer than the game's.
    {
        bool window_exact = true, non_trivial = false, trimmed = false;
        for (const entity_id id : ids)
        {
            const corporation_component& cc = w.corporations.at(id);
            const std::vector<double>&   m  = measured[id];
            if (cc.returns.size() > m.size())
            {
                window_exact = false;
                continue;
            }
            trimmed = trimmed || m.size() > cc.returns.size();
            double filed = 0.0, loop = 0.0;
            for (const quarterly_return& q : cc.returns)
                filed += static_cast<double>(q.net);
            for (std::size_t i = m.size() - cc.returns.size(); i < m.size(); ++i)
                loop += m[i];
            window_exact = window_exact && filed == loop;
            non_trivial  = non_trivial || filed != 0.0;
        }
        check(window_exact, "R2",
              "sum of the retained nets == the loop's movement over the same window, EXACTLY");
        check(trimmed, "R2",
              "the window was TRIMMED (retention rolled over, so the offset above is non-zero)");
        check(non_trivial, "R2", "the run actually moved balances (not vacuously zero)");
    }

    // --- determinism: two identical settles, byte-identical records ----------
    {
        const world a = settled_world(reg, 11u);
        const world b = settled_world(reg, 11u);
        const std::string ia = returns_image(a);
        const std::string ib = returns_image(b);
        check(!ia.empty() && ia == ib, "R6",
              "two identical settles produce byte-identical filed records");
        std::printf("       record image %zu bytes across %zu corporations\n",
                    ia.size(), a.corporations.size());
    }
}

} // namespace

int main()
{
    recipe_registry reg;
    author_economics(reg);
    reg.set_thresholds(/*t_full=*/1.0f, /*t_idle=*/0.2f);
    {
        recipe steel;
        steel.name = "steel";
        steel.inputs[ri(resource_type::iron_ore)] = 2.0f;
        steel.outputs[ri(resource_type::steel)]   = 1.0f;
        reg.add_recipe(steel);
    }
    // Vacuity guard (the standing lesson from interbody_pull_harness): a registry
    // with a zero build_cost would make R4 pass on nothing at all.
    if (reg.economics(building_type::extraction_site).build_cost <= 0.0f)
    {
        std::printf("FATAL: registry authored no build_cost — R4 would be vacuous.\n");
        return 2;
    }

    std::printf("quarterly_return — BL-626, requirement group `quarterly-return` R1-R6\n");

    run_fixture_rows(reg);
    run_save_rows(reg);
    run_settle_rows(reg);
    run_tax_rows(reg);

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES ABOVE",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
