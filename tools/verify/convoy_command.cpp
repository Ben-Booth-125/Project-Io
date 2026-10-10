// Headless convoy-command harness (BL-452 → BL-1266; no SDL / Lua / ImGui).
//
// BL-452 put Layer 5 — the logistics layer, and the only coupling between two
// markets' prices — onto the corp-command seam. BL-1266 (TRADE.md) retired the
// directed `dispatch_convoy` verb with the dispatcher and corporation pools:
// goods move between markets only by a TRADE, and the seam's verbs for it are
// `set_trade` / `clear_trade` / `set_trade_reserve`. `hold_convoy` stands. This
// harness is the assertion for each, on the same fixture and with the same
// discipline the dispatch verb had.
//
//   R0  THE VERB WORKS. set_trade (with points reserved for manual trades)
//       standing on a fixture makes the next trade pass ship exactly one convoy
//       of points x capacity, buying exactly that off the SOURCE SHELF, paying
//       the haul and nothing else — no other good, no destination shelf, no
//       market supply/demand moves (the cargo lands at ARRIVAL).
//
//   R1  A REJECTED COMMAND MUTATES NOTHING. Every rejection path (unknown corp,
//       bad market, the same market twice, a good trade does not carry, bad
//       resource, non-positive points, a source market where the actor holds
//       no trade building (NR-1018), another corp's trade, a foreign or
//       nonexistent convoy) is asserted against a FULL world fingerprint taken
//       before the command, not against a spot check. The seam is an untrusted
//       input boundary (io-standing-rules.md, 2026-08-14).
//
//   R2  NON-FINITE AND OUT-OF-RANGE ARE REJECTED, NOT CLAMPED. NaN, +/-inf and
//       points above `max_trade_points` are refused whole — for set_trade and
//       for set_trade_reserve — and the bound itself is accepted.
//
//   R3  HOLD IS A STOP, NOT A CANCEL. A held convoy stops advancing; issuing
//       the verb again releases it and it resumes from where it stopped; and
//       across the whole hold/release/arrive cycle the cargo is conserved —
//       never duplicated, never lost — landing whole at the destination.
//
//   R4  NO SECOND CODE PATH. A player's MANUAL trade and a rival's AUTO trade
//       of the same shape on the same lane carry IDENTICAL cost, speed and
//       mode: both go through the trade pass's one shipment funnel.
//
//   R5  DETERMINISM. Two runs of the same scripted sequence — auto trade and
//       player commands interleaved — produce byte-identical convoy sets.
//
// RETIRED with the verb (BL-1265/1266): "a corp with no stock in the source
// pool cannot haul it" and "a quantity above the pool" (a trade buys off a
// shared shelf and ships what it holds; it carries no quantity to refuse).
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/trade.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

constexpr std::size_t r_iron = static_cast<std::size_t>(resource_type::iron_ore);

/// Iron's trade capacity in this harness: one trade point moves 25 units.
constexpr float k_cap = 25.0f;

// ---------------------------------------------------------------------------
// World fingerprint — the whole point of R1
// ---------------------------------------------------------------------------
// A rejection test that checks "no convoy was created" passes while the corp's
// balance quietly moves. So the rejection assertions compare a string built
// from EVERY field a command could touch: balances and trade reserves, every
// standing trade, every landing, every convoy field, the id counters, and each
// market's supply/demand/price/inventory. Sorted id walks throughout, so the
// fingerprint itself cannot inherit hash layout and give a false PASS.
std::string fingerprint(const world& w)
{
    std::ostringstream o;
    o.precision(9);

    std::vector<entity_id> corp_ids;
    for (const auto& kv : w.corporations) corp_ids.push_back(kv.first);
    std::sort(corp_ids.begin(), corp_ids.end());
    for (const entity_id id : corp_ids)
        o << "C" << id << ':' << w.corporations.at(id).balance << ':'
          << w.corporations.at(id).trade_reserve << ';';

    for (const standing_trade& t : w.trades) // placement order: the spending order
        o << "X" << t.id << ':' << t.owner << ':' << static_cast<int>(t.resource) << ':'
          << t.from_market << '>' << t.to_market << ':' << t.points << ';';

    for (const auto& [key, sp] : w.landed_this_tick) // std::map: (owner, market) order
    {
        o << "L" << key.first << '/' << key.second << ':';
        for (const float q : sp.quantities) o << q << ',';
        o << ';';
    }

    std::vector<entity_id> market_ids;
    for (const auto& kv : w.markets) market_ids.push_back(kv.first);
    std::sort(market_ids.begin(), market_ids.end());
    for (const entity_id id : market_ids)
    {
        const market_component& m = w.markets.at(id);
        o << "M" << id << ':';
        for (std::size_t i = 0; i < resource_count; ++i)
            o << m.supply[i] << '/' << m.demand[i] << '/' << m.price[i] << '/'
              << m.inventory[i] << ',';
        o << ';';
    }

    o << "N" << w.next_convoy_id << ';';
    for (const convoy_component& c : w.convoys)
        o << "V" << c.id << ':' << c.source_market << '>' << c.dest_market << ':'
          << static_cast<int>(c.mode) << ':' << static_cast<int>(c.cargo_resource) << ':'
          << c.cargo_qty << ':' << c.progress << ':' << c.speed << ':' << c.corp << ':'
          << (c.arrived ? 1 : 0) << (c.held ? 1 : 0) << ':' << c.cost_paid << ';';

    o << "T" << w.trade_routes.size() << ';';
    return o.str();
}

/// Just the convoy set — R4/R5's subject, without the balances that a differing
/// tick sequence would legitimately move.
std::string convoy_fingerprint(const world& w)
{
    std::ostringstream o;
    o.precision(9);
    for (const convoy_component& c : w.convoys)
        o << c.id << ':' << c.source_market << '>' << c.dest_market << ':'
          << static_cast<int>(c.mode) << ':' << static_cast<int>(c.cargo_resource) << ':'
          << c.cargo_qty << ':' << c.progress << ':' << c.speed << ':' << c.corp << ':'
          << c.cost_paid << ';';
    return o.str();
}

// ---------------------------------------------------------------------------
// Fixture: one body, a plains column, a corp holding a staffed Planetary
// Marketplace (one trade point a tick) and two markets — a source at row 0 and
// a destination three tiles away. Follows logistics_harness.cpp's
// hand-built-grid idiom; a synthetic world states the preconditions in the
// test rather than hoping the generator produced them.
// ---------------------------------------------------------------------------
struct scenario
{
    world     w;
    entity_id body       = null_entity;
    entity_id corp       = null_entity;
    entity_id src_market = null_entity;
    entity_id dst_market = null_entity;
};

entity_id tile_at(world& w, entity_id body, int c, int r)
{
    const int gw = w.bodies.at(body).grid_width;
    return body_tile_grid(w, body)[static_cast<std::size_t>(r) * static_cast<std::size_t>(gw)
                                   + static_cast<std::size_t>(c)];
}

/// A corporation holding one staffed Planetary Marketplace in the SOURCE
/// market's catchment (one trade point a tick at the registry's rate), far off
/// every haul route. NR-1018 (Ben, 2026-10-10): a trade building makes its
/// points for the market it stands in, and trades leave only from such a
/// market — so the building must route to the source. (20, 0) is 12 columns
/// (wrapped) from both centres, 0 rows from the source and 3 from the
/// destination. It stood at (20, 2) before the ruling, which routes to the
/// DESTINATION: every manual trade from the source was then refused.
entity_id add_trader(scenario& s, float balance)
{
    const entity_id c = s.w.create_entity();
    corporation_component cc;
    cc.balance   = balance;
    cc.is_player = true; // keep the BL-202 strategic tier out of this harness
    const entity_id mp = s.w.create_entity();
    building_component b{};
    b.tile               = tile_at(s.w, s.body, 20, 0);
    b.type               = building_type::planetary_marketplace;
    b.workforce_assigned = 1.0f;
    s.w.buildings[mp] = b;
    cc.assets.push_back(mp);
    s.w.corporations[c] = cc;
    return c;
}

/// `stock` units of iron ore on the SOURCE market's shelf (BL-1265); a
/// four-tile plains column with the source centre at row 0 and the
/// destination market's centre at row 3.
scenario make_scenario(float stock = 100.0f, float balance = 1000.0f)
{
    scenario s;

    s.body = s.w.create_entity();
    body_component bc{};
    bc.name              = "Anvil";
    bc.type              = body_type::planet;
    bc.orbital_radius_au = 1.0f;
    // 32 columns wide so `body_km_per_tile` (circumference / grid_width) gives a
    // tile a sane physical size: on a 1-wide grid one tile would span the whole
    // planet and a three-tile haul would take 54 econ ticks. Four rows tall, and
    // every haul below runs down column 0.
    bc.grid_width        = 32;
    bc.grid_height       = 4;
    s.w.bodies[s.body] = bc;
    for (int r = 0; r < bc.grid_height; ++r)
        for (int c = 0; c < bc.grid_width; ++c)
        {
            const entity_id t = s.w.create_entity();
            tile_component tc{};
            tc.body        = s.body;
            tc.grid_x      = c;
            tc.grid_y      = r;
            tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass; tc.cover_density = 150;
            tc.landform    = terrain_landform::plains;
            s.w.tiles[t] = tc;
        }

    s.corp = add_trader(s, balance);
    s.w.player_entity = s.corp;

    // BL-597: a supply anchor (city) near — but NOT on — the haul route, so
    // every intra-body shipment clears the passive-LP gate (against the
    // generous rate `main()`'s `reg` sets) WITHOUT tripping BL-148/149's node
    // discount, which shares `w.population_centre_tile` — an anchor ON the
    // route would silently discount R0's exact cost assertion.
    s.w.population_centre_tile[s.w.create_entity()] = tile_at(s.w, s.body, 1, 0);

    s.src_market = s.w.create_entity();
    market_component sm{};
    sm.body          = s.body;
    sm.centre_tile   = tile_at(s.w, s.body, 0, 0);
    sm.base_price[r_iron] = 5.0f;
    sm.price         = sm.base_price;
    sm.inventory[r_iron]  = stock;
    s.w.markets[s.src_market] = sm;

    s.dst_market = s.w.create_entity();
    market_component dm{};
    dm.body          = s.body;
    dm.centre_tile   = tile_at(s.w, s.body, 0, 3);
    dm.base_price[r_iron] = 5.0f;
    dm.price         = dm.base_price;
    s.w.markets[s.dst_market] = dm;
    return s;
}

corp_command trade_cmd(const scenario& s, float points,
                       resource_type r = resource_type::iron_ore)
{
    corp_command cmd;
    cmd.corp         = s.corp;
    cmd.verb         = corp_verb::set_trade;
    cmd.subject      = s.src_market;
    cmd.counterparty = s.dst_market;
    cmd.target       = r;
    cmd.quantity     = points;
    return cmd;
}

corp_command reserve_cmd(entity_id corp, float points)
{
    corp_command cmd;
    cmd.corp     = corp;
    cmd.verb     = corp_verb::set_trade_reserve;
    cmd.quantity = points;
    return cmd;
}

corp_command hold_cmd(entity_id corp, uint32_t convoy_id)
{
    corp_command cmd;
    cmd.corp  = corp;
    cmd.verb  = corp_verb::hold_convoy;
    cmd.order = convoy_id;
    return cmd;
}

/// One trade pass, as the tick runs it (its purchases bill at a clear this
/// harness does not run).
trade_tick trade_pass(scenario& s, const recipe_registry& reg)
{
    economy_report rep;
    return run_trades(s.w, reg, rep);
}

/// Iron ore on the SOURCE market's shelf (BL-1265: was the corporation's pool).
float pool_iron(const scenario& s)
{
    return s.w.markets.at(s.src_market).inventory[r_iron];
}

/// Iron ore the corp has LANDED on the destination market — where a delivery
/// lands, to be sold at the next clear.
float dst_landed_iron(const scenario& s)
{
    return s.w.landed(s.corp, s.dst_market, r_iron);
}

/// Iron ore anywhere at all in the fixture — on either shelf, landed, or in
/// flight. The conservation quantity R3 tracks.
float iron_everywhere(const scenario& s)
{
    float total = pool_iron(s) + s.w.markets.at(s.dst_market).inventory[r_iron] + dst_landed_iron(s);
    for (const convoy_component& c : s.w.convoys)
        if (c.cargo_resource == resource_type::iron_ore)
            total += c.cargo_qty;
    return total;
}

/// The one rejection assertion, written once: apply `cmd`, expect `want`, and
/// require the world fingerprint to be byte-identical afterwards.
void check_rejected(scenario& s, const recipe_registry& reg, const corp_command& cmd,
                    corp_command_result want, const char* what)
{
    const std::string before = fingerprint(s.w);
    const corp_command_result got = apply_corp_command(s.w, reg, cmd);
    const std::string after = fingerprint(s.w);
    if (got != want)
        std::printf("       (expected result %d, got %d)\n",
                    static_cast<int>(want), static_cast<int>(got));
    check(got == want && before == after, what);
}

} // namespace

int main()
{
    std::printf("=== convoy_command (BL-452 -> BL-1266: trade joins the seam) ===\n");

    // Default per-mode logistics costs {land .02, sea .05, air .15, space 1.0}.
    recipe_registry reg;
    {
        // BL-597: a deliberately generous per-anchor LP rate — this file's
        // subject is the seam's verb contract, not BL-597's admissibility gate,
        // so the gate is set up to never bind here.
        military_capability_params mp = reg.military();
        mp.active_lp_per_anchor_tick = 1.0e6f;
        reg.set_military(mp);
        // BL-1266: iron moves k_cap units per trade point; a Marketplace makes
        // one point per unit of effective workforce.
        trade_params tp;
        tp.capacity[r_iron]   = k_cap;
        tp.marketplace_points = 1.0f;
        reg.set_trade(tp);
    }

    // -----------------------------------------------------------------------
    // R0 — the verb works, and moves exactly what it should
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario(100.0f);
        const float bal_before = s.w.corporations.at(s.corp).balance;

        const corp_command_result r = apply_corp_command(s.w, reg, trade_cmd(s, 1.0f));
        check(r == corp_command_result::applied, "R0.1 set_trade applies through the seam");
        check(s.w.trades.size() == 1 && s.w.trades.front().id != 0 && s.w.trades.front().owner == s.corp,
              "R0.2 exactly one trade stands, with a nonzero stable id, owned by the actor");
        check(apply_corp_command(s.w, reg, reserve_cmd(s.corp, 1.0f)) == corp_command_result::applied,
              "R0.3 set_trade_reserve applies (the point is reserved for manual trades)");
        check(s.w.convoys.empty(), "R0.4 setting a trade ships nothing until the trade pass");

        const trade_tick t = trade_pass(s, reg);
        check(t.manual_shipments == 1 && s.w.convoys.size() == 1,
              "R0.5 the trade pass ships exactly one convoy for the manual trade");

        if (s.w.convoys.size() == 1)
        {
            const convoy_component& c = s.w.convoys.front();
            check(c.id != 0, "R0.6 the convoy carries a nonzero stable id");
            check(c.corp == s.corp && c.cargo_resource == resource_type::iron_ore &&
                  std::fabs(c.cargo_qty - k_cap) < 1e-4f,
                  "R0.7 it names the trading corp, the cargo and points x capacity (25)");
            check(c.source_market == s.src_market && c.dest_market == s.dst_market,
                  "R0.8 it records the NAMED source and destination markets");
            check(c.mode == convoy_mode::land && !c.held && !c.arrived,
                  "R0.9 an all-plains intra-body lane is land mode, moving, not held");
            check(c.cost_paid > 0.0f &&
                  std::fabs((bal_before - s.w.corporations.at(s.corp).balance) - c.cost_paid) < 1e-4f,
                  "R0.10 the haul cost is recorded on the convoy and debited from the balance "
                  "(the purchase is billed at the clear)");
            // A* down a 4-tall plains column = 3 edges = 3.0; land unit cost 0.02.
            check(std::fabs(c.cost_paid - 0.02f * 3.0f * k_cap) < 1e-3f,
                  "R0.11 cost = land(0.02) x A*(3.0) x qty(25) = 1.5");
        }

        check(std::fabs(pool_iron(s) - 75.0f) < 1e-4f,
              "R0.12 the source SHELF is debited by EXACTLY the shipment (100 -> 75)");

        // Nothing else moved: every other good on the source shelf, the
        // destination shelf, and both markets' supply/demand are untouched.
        bool markets_clean = true;
        for (const entity_id mid : { s.src_market, s.dst_market })
        {
            const market_component& m = s.w.markets.at(mid);
            for (std::size_t i = 0; i < resource_count; ++i)
            {
                if (m.supply[i] != 0.0f || m.demand[i] != 0.0f)
                    markets_clean = false;
                if (m.inventory[i] != 0.0f && !(mid == s.src_market && i == r_iron))
                    markets_clean = false;
            }
        }
        check(markets_clean && s.w.landed_this_tick.empty(),
              "R0.13 no other shelf, no supply/demand and no landing moved (cargo lands at ARRIVAL)");
    }

    // -----------------------------------------------------------------------
    // R1 — every rejection mutates NOTHING (full-fingerprint comparison)
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario(50.0f);

        {   // Unknown acting corporation.
            corp_command c = trade_cmd(s, 1.0f);
            c.corp = s.w.create_entity();
            check_rejected(s, reg, c, corp_command_result::rejected_no_corp,
                           "R1.1 an unknown corp is rejected_no_corp and mutates nothing");
        }
        {   // Source market that is not a market.
            corp_command c = trade_cmd(s, 1.0f);
            c.subject = s.w.create_entity();
            check_rejected(s, reg, c, corp_command_result::rejected_invalid,
                           "R1.2 an unreal SOURCE market id is rejected_invalid and mutates nothing");
        }
        {   // Destination market that is not a market.
            corp_command c = trade_cmd(s, 1.0f);
            c.counterparty = s.w.create_entity();
            check_rejected(s, reg, c, corp_command_result::rejected_invalid,
                           "R1.3 an unreal DESTINATION market id is rejected_invalid and mutates nothing");
        }
        {   // A market to itself.
            corp_command c = trade_cmd(s, 1.0f);
            c.counterparty = s.src_market;
            check_rejected(s, reg, c, corp_command_result::rejected_invalid,
                           "R1.4 a trade from a market to ITSELF is rejected_invalid and mutates nothing");
        }
        {   // A good trade does not carry (no authored capacity).
            check_rejected(s, reg, trade_cmd(s, 1.0f, resource_type::coal),
                           corp_command_result::rejected_invalid,
                           "R1.5 a good with no trade capacity is rejected_invalid and mutates nothing");
        }
        {   // Zero and negative points.
            check_rejected(s, reg, trade_cmd(s, 0.0f), corp_command_result::rejected_invalid,
                           "R1.6 zero points is rejected_invalid and mutates nothing");
            check_rejected(s, reg, trade_cmd(s, -1.0f), corp_command_result::rejected_invalid,
                           "R1.7 negative points is rejected_invalid and mutates nothing");
        }
        {   // A resource index past the enum's tail, as the wire could send it.
            corp_command c = trade_cmd(s, 1.0f);
            c.target = static_cast<resource_type>(resource_count + 5);
            check_rejected(s, reg, c, corp_command_result::rejected_invalid,
                           "R1.8 an out-of-domain resource is rejected_invalid and mutates nothing");
        }
        {   // A convoy id nobody holds.
            check_rejected(s, reg, hold_cmd(s.corp, 4242u), corp_command_result::rejected_invalid,
                           "R1.9 holding a nonexistent convoy is rejected_invalid and mutates nothing");
        }
        {   // Zero is never a valid handle.
            check_rejected(s, reg, hold_cmd(s.corp, 0u), corp_command_result::rejected_invalid,
                           "R1.10 hold_convoy with a zero id is rejected_invalid and mutates nothing");
            corp_command c;
            c.corp = s.corp; c.verb = corp_verb::clear_trade; c.order = 0;
            check_rejected(s, reg, c, corp_command_result::rejected_invalid,
                           "R1.11 clear_trade with a zero id is rejected_invalid and mutates nothing");
        }

        {   // NR-1018: a manual trade leaves only from a market where the owner
            // holds a trade building. The corp's Marketplace stands in the
            // source's catchment; the destination has none of its buildings.
            corp_command c = trade_cmd(s, 1.0f);
            c.subject      = s.dst_market;
            c.counterparty = s.src_market;
            check_rejected(s, reg, c, corp_command_result::rejected_invalid,
                           "R1.18 a trade LEAVING a market where the owner holds no trade "
                           "building is rejected_invalid and mutates nothing (NR-1018)");
        }

        check(s.w.convoys.empty() && s.w.trades.empty() && std::fabs(pool_iron(s) - 50.0f) < 1e-4f,
              "R1.12 after the rejections: no trade, no convoy, and the shelf is untouched");

        // A rival's trade and a rival's convoy are indistinguishable from ones
        // that do not exist (BL-397's oracle rule), and touching them changes
        // nothing.
        {
            const entity_id rival = add_trader(s, 1000.0f);
            corp_command rt = trade_cmd(s, 0.2f); // 5 units
            rt.corp = rival;
            check(apply_corp_command(s.w, reg, rt) == corp_command_result::applied
                      && apply_corp_command(s.w, reg, reserve_cmd(rival, 1.0f))
                             == corp_command_result::applied,
                  "R1.13 fixture: a rival corp sets its own trade through the same verb");
            trade_pass(s, reg);
            check(s.w.convoys.size() == 1 && s.w.convoys.back().corp == rival,
                  "R1.14 fixture: the rival's trade ships its own convoy");
            // Guarded: a refused R1.13 left no trade, and front() on an empty
            // vector is undefined (it was this harness's segfault).
            const uint32_t rival_trade  = s.w.trades.empty() ? 0u : s.w.trades.front().id;
            const uint32_t rival_convoy = s.w.convoys.empty() ? 0u : s.w.convoys.back().id;
            check_rejected(s, reg, hold_cmd(s.corp, rival_convoy),
                           corp_command_result::rejected_invalid,
                           "R1.15 holding ANOTHER corp's convoy is rejected_invalid "
                           "(indistinguishable from R1.9) and mutates nothing");
            corp_command clr;
            clr.corp = s.corp; clr.verb = corp_verb::clear_trade; clr.order = rival_trade;
            check_rejected(s, reg, clr, corp_command_result::rejected_invalid,
                           "R1.16 clearing ANOTHER corp's trade is rejected_invalid and mutates nothing");
            corp_command chg = trade_cmd(s, 2.0f);
            chg.order = rival_trade;
            check_rejected(s, reg, chg, corp_command_result::rejected_invalid,
                           "R1.17 changing ANOTHER corp's trade is rejected_invalid and mutates nothing");
        }
    }

    // -----------------------------------------------------------------------
    // R2 — non-finite and out-of-range are REJECTED, never clamped
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario(100.0f);
        const float qnan = std::numeric_limits<float>::quiet_NaN();
        const float inf  = std::numeric_limits<float>::infinity();

        check_rejected(s, reg, trade_cmd(s, qnan), corp_command_result::rejected_invalid,
                       "R2.1 NaN points are REJECTED, not clamped");
        check_rejected(s, reg, trade_cmd(s, inf), corp_command_result::rejected_invalid,
                       "R2.2 +inf points are REJECTED, not clamped");
        check_rejected(s, reg, trade_cmd(s, -inf), corp_command_result::rejected_invalid,
                       "R2.3 -inf points are REJECTED, not clamped");
        check_rejected(s, reg, trade_cmd(s, 1.0e30f), corp_command_result::rejected_invalid,
                       "R2.4 points above max_trade_points are REJECTED, not clamped to it");
        check_rejected(s, reg, reserve_cmd(s.corp, qnan), corp_command_result::rejected_invalid,
                       "R2.5 a NaN trade reserve is REJECTED");
        check_rejected(s, reg, reserve_cmd(s.corp, -1.0f), corp_command_result::rejected_invalid,
                       "R2.6 a negative trade reserve is REJECTED");
        check_rejected(s, reg, reserve_cmd(s.corp, 1.0e30f), corp_command_result::rejected_invalid,
                       "R2.7 a trade reserve above max_trade_points is REJECTED, not clamped");
        check(s.w.trades.empty(), "R2.8 no rejected value produced a phantom trade");

        // The gate is PRECISE, not a blanket refusal: the bound itself is legal.
        check(apply_corp_command(s.w, reg, trade_cmd(s, max_trade_points)) == corp_command_result::applied
                  && apply_corp_command(s.w, reg, reserve_cmd(s.corp, max_trade_points))
                         == corp_command_result::applied,
              "R2.9 points EXACTLY max_trade_points are accepted, for a trade and a reserve");
    }

    // -----------------------------------------------------------------------
    // R3 — hold is a stop, not a cancel; cargo is conserved
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario(100.0f);
        const float conserved = iron_everywhere(s);
        apply_corp_command(s.w, reg, trade_cmd(s, 1.0f));
        apply_corp_command(s.w, reg, reserve_cmd(s.corp, 1.0f));
        trade_pass(s, reg);
        check(s.w.convoys.size() == 1, "R3.1 fixture: a convoy is in flight");
        if (s.w.convoys.size() == 1)
        {
            const uint32_t id = s.w.convoys.front().id;
            check(std::fabs(iron_everywhere(s) - conserved) < 1e-4f,
                  "R3.2 shipping moves the cargo off the shelf, neither minting nor losing it");

            advance_convoys(s.w);
            const float moving_progress = s.w.convoys.front().progress;
            check(moving_progress > 0.0f, "R3.3 fixture: an unheld convoy advances");

            check(apply_corp_command(s.w, reg, hold_cmd(s.corp, id)) == corp_command_result::applied,
                  "R3.4 hold_convoy applies");
            check(s.w.convoys.front().held, "R3.5 the convoy is marked held");

            for (int i = 0; i < 5; ++i) advance_convoys(s.w);
            check(std::fabs(s.w.convoys.front().progress - moving_progress) < 1e-6f,
                  "R3.6 five ticks later a HELD convoy has not advanced at all");
            check(!s.w.convoys.front().arrived, "R3.7 ...and it has not arrived");
            check(std::fabs(iron_everywhere(s) - conserved) < 1e-4f,
                  "R3.8 holding neither duplicates nor loses the cargo");

            check(apply_corp_command(s.w, reg, hold_cmd(s.corp, id)) == corp_command_result::applied,
                  "R3.9 issuing hold_convoy again applies (it is a toggle)");
            check(!s.w.convoys.front().held, "R3.10 ...and releases the convoy");

            advance_convoys(s.w);
            check(s.w.convoys.front().progress > moving_progress,
                  "R3.11 a released convoy resumes from where it stopped");

            // Run it to arrival and credit it.
            for (int i = 0; i < 20 && !s.w.convoys.empty(); ++i)
            {
                advance_convoys(s.w);
                credit_arrived_convoys(s.w, i);
            }
            check(s.w.convoys.empty(), "R3.12 the convoy arrives and is retired");
            check(std::fabs(pool_iron(s) - 75.0f) < 1e-3f &&
                      std::fabs(dst_landed_iron(s) - k_cap) < 1e-3f,
                  "R3.13 CONSERVATION: the delivered cargo LANDS at the destination in full "
                  "(75 left on the source shelf + 25 landed at the destination = 100)");
        }
    }

    // -----------------------------------------------------------------------
    // R4 — NO SECOND CODE PATH: the player's manual trade IS the rival's auto
    // -----------------------------------------------------------------------
    {
        // Two identical fixtures. In one, AUTO trade fills a shortfall of 25 at
        // the destination market; in the other the player's MANUAL trade ships
        // the same 25 down the same lane. The two convoys must agree on cost,
        // speed and mode — the three numbers a duplicated implementation would
        // get subtly wrong. Auto chases a margin, so the destination prices the
        // good above home (10 vs 5), and its demand is deep enough that the
        // room (`trade_room`, sized to the target price at the landed cost
        // plus the margin) never binds: auto ships the one point's 25, the
        // same 25 the manual trade's one point ships.
        scenario a = make_scenario(100.0f);
        a.w.markets.at(a.dst_market).price[r_iron]  = 10.0f;
        a.w.markets.at(a.dst_market).demand[r_iron] = 1000.0f;
        a.w.markets.at(a.dst_market).supply[r_iron] = 0.0f;
        const trade_tick ta = trade_pass(a, reg); // reserve 0: every point is auto

        scenario p = make_scenario(100.0f);
        p.w.markets.at(p.dst_market).price[r_iron] = 10.0f;
        apply_corp_command(p.w, reg, trade_cmd(p, 1.0f));
        apply_corp_command(p.w, reg, reserve_cmd(p.corp, 1.0f));
        const trade_tick tp = trade_pass(p, reg);

        check(a.w.convoys.size() == 1 && p.w.convoys.size() == 1
                  && ta.auto_shipments == 1 && tp.manual_shipments == 1,
              "R4.1 fixture: one auto convoy and one manual convoy");
        if (a.w.convoys.size() == 1 && p.w.convoys.size() == 1)
        {
            const convoy_component& ac = a.w.convoys.front();
            const convoy_component& pc = p.w.convoys.front();
            std::printf("       auto: cargo %.4f cost %.4f speed %.4f | manual: cargo %.4f cost %.4f speed %.4f\n",
                        ac.cargo_qty, ac.cost_paid, ac.speed, pc.cargo_qty, pc.cost_paid, pc.speed);
            check(std::fabs(ac.cost_paid - pc.cost_paid) < 1e-6f,
                  "R4.2 IDENTICAL COST: the player pays exactly what auto trade pays");
            check(std::fabs(ac.speed - pc.speed) < 1e-6f,
                  "R4.3 IDENTICAL SPEED: the same lane takes the same number of ticks");
            check(ac.mode == pc.mode && ac.cargo_qty == pc.cargo_qty,
                  "R4.4 IDENTICAL MODE and cargo");
            check(std::fabs((1000.0f - a.w.corporations.at(a.corp).balance) -
                            (1000.0f - p.w.corporations.at(p.corp).balance)) < 1e-6f,
                  "R4.5 both corps are debited the same amount");
        }
    }

    // -----------------------------------------------------------------------
    // R5 — determinism: two runs of one script produce identical convoy sets
    // -----------------------------------------------------------------------
    {
        auto run = [&reg](std::vector<std::string>& out) {
            scenario s = make_scenario(400.0f);
            // A second trader on auto, so the trade pass's sorted-corp walk has
            // more than one entry to order — the hash-layout leak this asserts
            // against is invisible with a single corp.
            add_trader(s, 1000.0f);

            for (int t = 0; t < 6; ++t)
            {
                s.w.current_day_tick  = t;
                s.w.current_econ_tick = t;
                // Player traffic: a trade on tick 0, re-pointed on tick 2,
                // cleared on tick 4; the reserve set once; a hold on tick 3.
                if (t == 0)
                {
                    apply_corp_command(s.w, reg, trade_cmd(s, 0.2f));
                    apply_corp_command(s.w, reg, reserve_cmd(s.corp, 1.0f));
                }
                if (t == 2 && !s.w.trades.empty())
                {
                    corp_command chg = trade_cmd(s, 0.4f);
                    chg.order = s.w.trades.front().id;
                    apply_corp_command(s.w, reg, chg);
                }
                if (t == 3 && !s.w.convoys.empty())
                    apply_corp_command(s.w, reg, hold_cmd(s.corp, s.w.convoys.front().id));
                if (t == 4 && !s.w.trades.empty())
                {
                    corp_command clr;
                    clr.corp = s.corp; clr.verb = corp_verb::clear_trade;
                    clr.order = s.w.trades.front().id;
                    apply_corp_command(s.w, reg, clr);
                }
                // Auto traffic: a standing shortfall at the destination market,
                // priced above home.
                s.w.markets.at(s.dst_market).price[r_iron]  = 10.0f;
                s.w.markets.at(s.dst_market).demand[r_iron] = 12.0f;
                s.w.markets.at(s.dst_market).supply[r_iron] = 0.0f;
                trade_pass(s, reg);
                advance_convoys(s.w);
                credit_arrived_convoys(s.w, t);
                out.push_back(convoy_fingerprint(s.w));
            }
        };

        std::vector<std::string> x, y;
        run(x);
        run(y);
        check(!x.empty() && x.size() == y.size(), "R5.1 both runs sampled the same tick count");
        bool same = x.size() == y.size();
        for (std::size_t i = 0; same && i < x.size(); ++i)
            if (x[i] != y[i]) same = false;
        check(same, "R5.2 the convoy set is IDENTICAL at every tick across two runs of one script");
        bool any_convoys = false;
        for (const std::string& f : x) if (!f.empty()) any_convoys = true;
        check(any_convoys, "R5.3 ...and the script actually produced convoys (not vacuously equal)");
    }

    std::printf("\n%s  (%d passed, %d failed)\n",
                g_fail == 0 ? "ALL PASS" : "FAILURES", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
