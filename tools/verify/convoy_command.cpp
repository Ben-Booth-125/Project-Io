// Headless convoy-command harness (BL-452; no SDL / Lua / ImGui).
//
// BL-452 put Layer 5 — the logistics layer, and the only coupling between two
// markets' prices — onto the corp-command seam for the first time. Two verbs
// (`dispatch_convoy`, `hold_convoy`) and one refactor (the auto-dispatcher's
// dispatch half factored out so both callers share it). Each has a way of being
// subtly wrong that a build does not catch, and this harness is the assertion
// for each.
//
//   R0  THE VERB WORKS. dispatch_convoy with valid arguments creates exactly
//       one convoy, debits the source pool by exactly the quantity, and moves
//       nothing else — no other resource, no other corp, no market array.
//
//   R1  A REJECTED COMMAND MUTATES NOTHING. Every rejection path (bad market,
//       bad resource, non-positive quantity, quantity above the pool, unknown
//       corp, foreign/nonexistent convoy) is asserted against a FULL world
//       fingerprint taken before the command, not against a spot check. The
//       seam is an untrusted input boundary (io-standing-rules.md, 2026-08-14):
//       a refusal that half-applies is the failure mode the rule exists for.
//
//   R2  NON-FINITE AND OUT-OF-RANGE ARE REJECTED, NOT CLAMPED. NaN, +/-inf and
//       an absurd finite quantity are refused whole. The distinction matters:
//       a clamp would silently haul a different quantity than the one asked
//       for, which is worse than a refusal because nothing reports it.
//
//   R3  HOLD IS A STOP, NOT A CANCEL. A held convoy stops advancing; issuing
//       the verb again releases it and it resumes from where it stopped; and
//       across the whole hold/release/arrive cycle the cargo is conserved —
//       never duplicated, never lost. Cargo left the source pool at dispatch,
//       so a cancel would have to mint goods; there is deliberately no verb
//       that does.
//
//   R4  NO FOURTH CODE PATH (the assertion the refactor earns). A convoy the
//       player dispatched and one the auto-dispatcher dispatched, of the same
//       shape on the same lane, carry IDENTICAL cost, speed and mode. If the
//       two ever diverge, the player's logistics have stopped being the AI's,
//       and R4 is what says so.
//
//   R5  DETERMINISM. Two runs of the same scripted sequence — auto dispatch and
//       player commands interleaved — produce byte-identical convoy sets. The
//       auto-dispatcher walks unordered_maps (corporations, markets) and the
//       convoy vector's insertion order is the trade-route creation order and
//       the world history log's, so a hash-layout leak here is a replay break.
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/logistics.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
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

// ---------------------------------------------------------------------------
// World fingerprint — the whole point of R1
// ---------------------------------------------------------------------------
// A rejection test that checks "no convoy was created" passes while the corp's
// balance quietly moves. So the rejection assertions compare a string built
// from EVERY field a dispatch could touch: balances, every pool quantity, every
// convoy field, the id counter, and each market's supply/demand/price/inventory.
// Sorted id walks throughout, so the fingerprint itself cannot inherit hash
// layout and give a false PASS.
std::string fingerprint(const world& w)
{
    std::ostringstream o;
    o.precision(9);

    std::vector<entity_id> corp_ids;
    for (const auto& kv : w.corporations) corp_ids.push_back(kv.first);
    std::sort(corp_ids.begin(), corp_ids.end());
    for (const entity_id id : corp_ids)
        o << "C" << id << ':' << w.corporations.at(id).balance << ';';

    // corp_body_pools is a std::map — already in (corp, body) order.
    for (const auto& [key, sc] : w.corp_body_pools)
    {
        o << "P" << key.first << '/' << key.second << ':';
        for (const float q : sc.quantities) o << q << ',';
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
// Fixture: one body, a plains column, a corp anchored at one end and two
// markets — a source at the anchor and a destination three tiles away. Follows
// logistics_harness.cpp's hand-built-grid idiom; a synthetic world states the
// preconditions in the test rather than hoping the generator produced them.
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

/// `stock` units of iron ore in the corp's on-body pool; a four-tile plains
/// column with the corp's anchor at row 0 and the destination market's centre
/// at row 3.
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
            tc.composition = terrain_composition::grassland;
            tc.landform    = terrain_landform::plains;
            s.w.tiles[t] = tc;
        }

    s.corp = s.w.create_entity();
    corporation_component cc;
    cc.balance   = balance;
    cc.is_player = true; // keep the BL-202 strategic tier out of this harness

    const entity_id anchor = tile_at(s.w, s.body, 0, 0);
    const entity_id bld    = s.w.create_entity();
    building_component b{};
    b.tile = anchor;
    b.type = building_type::extraction_site;
    s.w.buildings[bld] = b;
    cc.assets.push_back(bld);
    s.w.corporations[s.corp] = cc;
    s.w.player_entity = s.corp;

    s.src_market = s.w.create_entity();
    market_component sm{};
    sm.body          = s.body;
    sm.centre_tile   = anchor;
    sm.base_price[r_iron] = 5.0f;
    sm.price         = sm.base_price;
    s.w.markets[s.src_market] = sm;

    s.dst_market = s.w.create_entity();
    market_component dm{};
    dm.body          = s.body;
    dm.centre_tile   = tile_at(s.w, s.body, 0, 3);
    dm.base_price[r_iron] = 5.0f;
    dm.price         = dm.base_price;
    s.w.markets[s.dst_market] = dm;

    s.w.pool_for(s.corp, s.body).quantities[r_iron] = stock;
    return s;
}

corp_command dispatch_cmd(const scenario& s, float qty,
                          resource_type r = resource_type::iron_ore)
{
    corp_command cmd;
    cmd.corp         = s.corp;
    cmd.verb         = corp_verb::dispatch_convoy;
    cmd.subject      = s.src_market;
    cmd.counterparty = s.dst_market;
    cmd.target       = r;
    cmd.quantity     = qty;
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

float pool_iron(const scenario& s)
{
    const auto it = s.w.corp_body_pools.find({s.corp, s.body});
    return it != s.w.corp_body_pools.end() ? it->second.quantities[r_iron] : 0.0f;
}

/// Iron ore held by the corp anywhere at all — in the pool or in flight. The
/// conservation quantity R3 tracks.
float iron_everywhere(const scenario& s)
{
    float total = pool_iron(s);
    for (const convoy_component& c : s.w.convoys)
        if (c.corp == s.corp && c.cargo_resource == resource_type::iron_ore)
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
    std::printf("=== convoy_command (BL-452: the logistics layer joins the seam) ===\n");

    // Default per-mode logistics costs {land .02, sea .05, air .15, space 1.0}.
    const recipe_registry reg;

    // -----------------------------------------------------------------------
    // R0 — the verb works, and moves exactly what it should
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario(100.0f);
        const float bal_before = s.w.corporations.at(s.corp).balance;

        const corp_command_result r = apply_corp_command(s.w, reg, dispatch_cmd(s, 25.0f));
        check(r == corp_command_result::applied, "R0.1 dispatch_convoy applies through the seam");
        check(s.w.convoys.size() == 1, "R0.2 exactly one convoy is created");

        if (s.w.convoys.size() == 1)
        {
            const convoy_component& c = s.w.convoys.front();
            check(c.id != 0, "R0.3 the convoy carries a nonzero stable id");
            check(c.corp == s.corp && c.cargo_resource == resource_type::iron_ore &&
                  std::fabs(c.cargo_qty - 25.0f) < 1e-4f,
                  "R0.4 it names the acting corp, the cargo and the quantity asked for");
            check(c.source_market == s.src_market && c.dest_market == s.dst_market,
                  "R0.5 it records the NAMED source and destination markets");
            check(c.mode == convoy_mode::land && !c.held && !c.arrived,
                  "R0.6 an all-plains intra-body lane is land mode, moving, not held");
            check(c.cost_paid > 0.0f &&
                  std::fabs((bal_before - s.w.corporations.at(s.corp).balance) - c.cost_paid) < 1e-4f,
                  "R0.7 the haul cost is recorded on the convoy and debited from the balance");
            // A* down a 4-tall plains column = 3 edges = 3.0; land unit cost 0.02.
            check(std::fabs(c.cost_paid - 0.02f * 3.0f * 25.0f) < 1e-3f,
                  "R0.8 cost = land(0.02) x A*(3.0) x qty(25) = 1.5");
        }

        check(std::fabs(pool_iron(s) - 75.0f) < 1e-4f,
              "R0.9 the source pool is debited by EXACTLY the quantity (100 -> 75)");

        // Nothing else moved: every other resource in the pool, and both
        // markets' arrays, are untouched.
        bool other_resources_clean = true;
        const auto& q = s.w.corp_body_pools.at({s.corp, s.body}).quantities;
        for (std::size_t i = 0; i < resource_count; ++i)
            if (i != r_iron && q[i] != 0.0f) other_resources_clean = false;
        check(other_resources_clean, "R0.10 no other resource in the pool moved");

        bool markets_clean = true;
        for (const entity_id mid : { s.src_market, s.dst_market })
        {
            const market_component& m = s.w.markets.at(mid);
            for (std::size_t i = 0; i < resource_count; ++i)
                if (m.supply[i] != 0.0f || m.demand[i] != 0.0f || m.inventory[i] != 0.0f)
                    markets_clean = false;
        }
        check(markets_clean,
              "R0.11 no market supply/demand/inventory moved (cargo lands at ARRIVAL, not dispatch)");
    }

    // -----------------------------------------------------------------------
    // R1 — every rejection mutates NOTHING (full-fingerprint comparison)
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario(50.0f);

        {   // Unknown acting corporation.
            corp_command c = dispatch_cmd(s, 10.0f);
            c.corp = s.w.create_entity();
            check_rejected(s, reg, c, corp_command_result::rejected_no_corp,
                           "R1.1 an unknown corp is rejected_no_corp and mutates nothing");
        }
        {   // Source market that is not a market.
            corp_command c = dispatch_cmd(s, 10.0f);
            c.subject = s.w.create_entity();
            check_rejected(s, reg, c, corp_command_result::rejected_invalid,
                           "R1.2 an unreal SOURCE market id is rejected_invalid and mutates nothing");
        }
        {   // Destination market that is not a market.
            corp_command c = dispatch_cmd(s, 10.0f);
            c.counterparty = s.w.create_entity();
            check_rejected(s, reg, c, corp_command_result::rejected_invalid,
                           "R1.3 an unreal DESTINATION market id is rejected_invalid and mutates nothing");
        }
        {   // A body the corp has stock on, but a corp that does not own the stock.
            const entity_id other = s.w.create_entity();
            corporation_component oc;
            oc.balance = 1000.0f;
            s.w.corporations[other] = oc;
            corp_command c = dispatch_cmd(s, 10.0f);
            c.corp = other; // real corp, but its (corp, body) pool is empty
            check_rejected(s, reg, c, corp_command_result::rejected_state,
                           "R1.4 WRONG OWNER: a corp with no stock in the source pool cannot haul it");
        }
        {   // Quantity above the pool.
            check_rejected(s, reg, dispatch_cmd(s, 50.001f), corp_command_result::rejected_state,
                           "R1.5 a quantity above the pool is rejected_state and mutates nothing");
        }
        {   // Zero and negative quantities.
            check_rejected(s, reg, dispatch_cmd(s, 0.0f), corp_command_result::rejected_invalid,
                           "R1.6 a zero quantity is rejected_invalid and mutates nothing");
            check_rejected(s, reg, dispatch_cmd(s, -10.0f), corp_command_result::rejected_invalid,
                           "R1.7 a negative quantity is rejected_invalid and mutates nothing");
        }
        {   // A resource index past the enum's tail, as the wire could send it.
            corp_command c = dispatch_cmd(s, 10.0f);
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
        }

        check(s.w.convoys.empty() && std::fabs(pool_iron(s) - 50.0f) < 1e-4f,
              "R1.11 after ten rejections: no convoy exists and the pool is untouched");

        // A rival's convoy is indistinguishable from one that does not exist
        // (BL-397's oracle rule), and holding it changes nothing.
        {
            const entity_id rival = s.w.create_entity();
            corporation_component rc;
            rc.balance = 1000.0f;
            s.w.corporations[rival] = rc;
            s.w.pool_for(rival, s.body).quantities[r_iron] = 40.0f;
            // The rival needs its own anchor to have a route at all.
            const entity_id rb = s.w.create_entity();
            building_component b{};
            b.tile = tile_at(s.w, s.body, 0, 0);
            b.type = building_type::extraction_site;
            s.w.buildings[rb] = b;
            s.w.corporations[rival].assets.push_back(rb);

            corp_command rc_cmd = dispatch_cmd(s, 5.0f);
            rc_cmd.corp = rival;
            check(apply_corp_command(s.w, reg, rc_cmd) == corp_command_result::applied,
                  "R1.12 fixture: a rival corp dispatches its own convoy through the same verb");
            const uint32_t rival_convoy = s.w.convoys.back().id;
            check_rejected(s, reg, hold_cmd(s.corp, rival_convoy),
                           corp_command_result::rejected_invalid,
                           "R1.13 holding ANOTHER corp's convoy is rejected_invalid "
                           "(indistinguishable from R1.9) and mutates nothing");
        }
    }

    // -----------------------------------------------------------------------
    // R2 — non-finite and out-of-range are REJECTED, never clamped
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario(100.0f);
        const float qnan = std::numeric_limits<float>::quiet_NaN();
        const float inf  = std::numeric_limits<float>::infinity();

        check_rejected(s, reg, dispatch_cmd(s, qnan), corp_command_result::rejected_invalid,
                       "R2.1 a NaN quantity is REJECTED, not clamped");
        check_rejected(s, reg, dispatch_cmd(s, inf), corp_command_result::rejected_invalid,
                       "R2.2 a +inf quantity is REJECTED, not clamped");
        check_rejected(s, reg, dispatch_cmd(s, -inf), corp_command_result::rejected_invalid,
                       "R2.3 a -inf quantity is REJECTED, not clamped");
        check_rejected(s, reg, dispatch_cmd(s, 1.0e30f), corp_command_result::rejected_state,
                       "R2.4 an absurd finite quantity is REJECTED against the pool, not clamped to it");
        check(s.w.convoys.empty(), "R2.5 no rejected quantity produced a phantom convoy");

        // The gate is PRECISE, not a blanket refusal: exactly the pool's
        // contents is a legal haul, so the four refusals above are about the
        // values they named and nothing else.
        check(apply_corp_command(s.w, reg, dispatch_cmd(s, 100.0f)) == corp_command_result::applied,
              "R2.6 a quantity EXACTLY equal to the pool is accepted");
    }

    // -----------------------------------------------------------------------
    // R3 — hold is a stop, not a cancel; cargo is conserved
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario(100.0f);
        check(apply_corp_command(s.w, reg, dispatch_cmd(s, 30.0f)) == corp_command_result::applied,
              "R3.1 fixture: a convoy is in flight");
        const uint32_t id = s.w.convoys.front().id;
        const float    conserved = iron_everywhere(s);

        advance_convoys(s.w);
        const float moving_progress = s.w.convoys.front().progress;
        check(moving_progress > 0.0f, "R3.2 fixture: an unheld convoy advances");

        check(apply_corp_command(s.w, reg, hold_cmd(s.corp, id)) == corp_command_result::applied,
              "R3.3 hold_convoy applies");
        check(s.w.convoys.front().held, "R3.4 the convoy is marked held");

        for (int i = 0; i < 5; ++i) advance_convoys(s.w);
        check(std::fabs(s.w.convoys.front().progress - moving_progress) < 1e-6f,
              "R3.5 five ticks later a HELD convoy has not advanced at all");
        check(!s.w.convoys.front().arrived, "R3.6 ...and it has not arrived");
        check(std::fabs(iron_everywhere(s) - conserved) < 1e-4f,
              "R3.7 holding neither duplicates nor loses the cargo");

        check(apply_corp_command(s.w, reg, hold_cmd(s.corp, id)) == corp_command_result::applied,
              "R3.8 issuing hold_convoy again applies (it is a toggle)");
        check(!s.w.convoys.front().held, "R3.9 ...and releases the convoy");

        advance_convoys(s.w);
        check(s.w.convoys.front().progress > moving_progress,
              "R3.10 a released convoy resumes from where it stopped");

        // Run it to arrival and credit it.
        for (int i = 0; i < 20 && !s.w.convoys.empty(); ++i)
        {
            advance_convoys(s.w);
            credit_arrived_convoys(s.w, i);
        }
        check(s.w.convoys.empty(), "R3.11 the convoy arrives and is retired");
        check(std::fabs(pool_iron(s) - 100.0f) < 1e-3f,
              "R3.12 CONSERVATION: the delivered cargo lands in the destination pool in full "
              "(same-body lane: 70 held + 30 delivered = 100)");
    }

    // -----------------------------------------------------------------------
    // R4 — NO FOURTH CODE PATH: the player's convoy IS the auto-dispatcher's
    // -----------------------------------------------------------------------
    {
        // Two identical fixtures. In one, the auto-dispatcher fills a shortfall
        // of 25 at the destination market; in the other the player dispatches
        // the same 25 down the same lane. The two convoys must agree on cost,
        // speed and mode — the three numbers a duplicated implementation would
        // get subtly wrong.
        scenario a = make_scenario(100.0f);
        a.w.markets.at(a.dst_market).demand[r_iron] = 25.0f;
        a.w.markets.at(a.dst_market).supply[r_iron] = 0.0f;
        dispatch_convoys(a.w, reg, reg.logistics_cost(convoy_mode::land),
                         reg.logistics_cost(convoy_mode::space));

        scenario p = make_scenario(100.0f);
        apply_corp_command(p.w, reg, dispatch_cmd(p, 25.0f));

        check(a.w.convoys.size() == 1 && p.w.convoys.size() == 1,
              "R4.1 fixture: one auto convoy and one player convoy");
        if (a.w.convoys.size() == 1 && p.w.convoys.size() == 1)
        {
            const convoy_component& ac = a.w.convoys.front();
            const convoy_component& pc = p.w.convoys.front();
            check(std::fabs(ac.cost_paid - pc.cost_paid) < 1e-6f,
                  "R4.2 IDENTICAL COST: the player pays exactly what the auto-dispatcher pays");
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
            // A second corp, so the auto-dispatcher's sorted-corp walk has more
            // than one entry to order — the hash-layout leak this asserts against
            // is invisible with a single corp.
            const entity_id other = s.w.create_entity();
            corporation_component oc;
            oc.balance = 1000.0f;
            s.w.corporations[other] = oc;
            s.w.pool_for(other, s.body).quantities[r_iron] = 200.0f;
            const entity_id ob = s.w.create_entity();
            building_component b{};
            b.tile = tile_at(s.w, s.body, 0, 0);
            b.type = building_type::extraction_site;
            s.w.buildings[ob] = b;
            s.w.corporations[other].assets.push_back(ob);

            for (int t = 0; t < 6; ++t)
            {
                s.w.current_day_tick = t;
                // Player traffic on even ticks; a hold on tick 3.
                if (t % 2 == 0)
                    apply_corp_command(s.w, reg, dispatch_cmd(s, 5.0f + static_cast<float>(t)));
                if (t == 3 && !s.w.convoys.empty())
                    apply_corp_command(s.w, reg, hold_cmd(s.corp, s.w.convoys.front().id));
                // Auto traffic: a standing shortfall at the destination market.
                s.w.markets.at(s.dst_market).demand[r_iron] = 12.0f;
                s.w.markets.at(s.dst_market).supply[r_iron] = 0.0f;
                dispatch_convoys(s.w, reg, reg.logistics_cost(convoy_mode::land),
                                 reg.logistics_cost(convoy_mode::space));
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
