// Headless harness for BL-350 (procurement/contract seam): one contract
// quoted, accepted, paced and delivered end to end with the treasury debited
// in the split shape (a deposit at accept, the remainder across lead time),
// plus one decline observed for each of the four refusal conditions.
// Kept outside src/ so the CMake glob does not pull it into the real build.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/condition_set.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char* what)
{
    if (cond) std::printf("  PASS  %s\n", what);
    else      { std::printf("  FAIL  %s\n", what); ++g_failures; }
}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-2f; }
static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

int main()
{
    recipe_registry reg;
    { building_economics ex; ex.base_rate = 10.0f;
      reg.set_economics(building_type::extraction_site, ex); }
    { building_economics pr; pr.base_rate = 10.0f;
      reg.set_economics(building_type::processing_facility, pr); }

    world w;
    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};
    const entity_id market = w.create_entity();
    market_component mc; mc.body = body;
    mc.base_price[ri(resource_type::iron_ore)] = 2.5f;
    mc.base_price[ri(resource_type::silica)]   = 0.0f; // deliberately unpriced (R2's fixture)
    mc.price = mc.base_price;
    w.markets[market] = mc;

    // --- Test 1: the full lifecycle, on a supplier with real extraction capacity ---
    const entity_id supplier = w.create_entity();
    {
        corporation_component cc; cc.name = "Supplier Co"; cc.balance = 10000.0f; cc.is_player = false;
        w.corporations[supplier] = cc;
    }
    {
        const entity_id tile = w.create_entity();
        tile_component tc{}; tc.body = body; tc.resource_deposit[ri(resource_type::iron_ore)] = 2.0f;
        w.tiles[tile] = tc;
        const entity_id bld = w.create_entity();
        building_component b{}; b.tile = tile; b.type = building_type::extraction_site;
        b.target_resource = resource_type::iron_ore; b.ticks_remaining = 0;
        w.buildings[bld] = b;
        w.corporations[supplier].assets.push_back(bld);
    }
    const entity_id buyer = w.create_entity();
    { corporation_component cc; cc.name = "Buyer Co"; cc.balance = 10000.0f; cc.is_player = true;
      w.corporations[buyer] = cc; }
    const float starting_balance = w.corporations[buyer].balance;

    corp_command quote_cmd;
    quote_cmd.corp = buyer; quote_cmd.verb = corp_verb::request_quote;
    quote_cmd.counterparty = supplier; quote_cmd.target = resource_type::iron_ore; quote_cmd.quantity = 20.0f;
    const corp_command_result quote_res = apply_corp_command(w, reg, quote_cmd, nullptr);
    check(quote_res == corp_command_result::applied, "R0.1 request_quote succeeds against a capable, unembargoed, in-reputation supplier");
    check(w.procurement_quotes.size() == 1, "R0.1 a live quote is recorded");

    const uint32_t quote_id = w.procurement_quotes.empty() ? 0 : w.procurement_quotes.front().id;
    const float quoted_price = w.procurement_quotes.empty() ? 0.0f : w.procurement_quotes.front().unit_price;
    const int   lead_time    = w.procurement_quotes.empty() ? 0 : w.procurement_quotes.front().lead_time_ticks;

    corp_command accept_cmd;
    accept_cmd.corp = buyer; accept_cmd.verb = corp_verb::accept_quote; accept_cmd.order = quote_id;
    const corp_command_result accept_res = apply_corp_command(w, reg, accept_cmd, nullptr);
    check(accept_res == corp_command_result::applied, "R0.2 accept_quote converts the live quote into a contract");
    check(w.procurement_quotes.empty(), "R0.2 the quote is consumed (cannot be accepted twice)");
    check(w.procurement_contracts.size() == 1, "R0.2 exactly one contract now stands");

    const float total   = 20.0f * quoted_price;
    const float deposit = total * reg.procurement().deposit_fraction;
    check(near(w.corporations[buyer].balance, starting_balance - deposit),
          "Q1 SPLIT: the deposit (not the full price) is debited at accept_quote");

    // The buyer holds no building that reserves iron_ore, so the delivered
    // quantity reads as pure surplus to clear_markets' auto-sell pass
    // (market_clearing.cpp) THE SAME TICK it lands — a genuine, unrelated
    // economic interaction, not a defect in this seam. Both checks below are
    // therefore taken right after the delivering run_economy_step call, on
    // the completion tick, BEFORE that tick's clear_markets has a chance to
    // auto-sell the delivery or credit its income.
    for (int t = 0; t < lead_time; ++t)
    {
        const bool final_tick = (t == lead_time - 1);
        economy_report rep = run_economy_step(w, reg);
        if (final_tick)
        {
            check(w.procurement_contracts.empty(), "R0.3 the contract completes and is removed within its own lead time");
            check(near(w.pool_for(buyer, body).quantities[ri(resource_type::iron_ore)], 20.0f),
                  "R0.3 the full quantity lands in the BUYER's pool on completion, before any auto-sell");
            check(near(w.corporations[buyer].balance, starting_balance - total),
                  "Q1 SPLIT: by completion the treasury has paid exactly the deposit plus the paced remainder, before any auto-sell income");
        }
        auto flows = clear_markets(w, reg, rep);
        apply_budget(w, reg, flows, rep.workforce_contention);
    }
    const auto rep_it = w.corp_reputation.find({buyer, supplier});
    check(rep_it != w.corp_reputation.end() && rep_it->second > 0.0f,
          "Q3 a completed contract moves (buyer, supplier) reputation up");

    // --- Test 2: no capacity — a supplier with no buildings at all ---
    const entity_id supplier_empty = w.create_entity();
    { corporation_component cc; cc.balance = 1000.0f; w.corporations[supplier_empty] = cc; }
    {
        corp_command c; c.corp = buyer; c.verb = corp_verb::request_quote;
        c.counterparty = supplier_empty; c.target = resource_type::iron_ore; c.quantity = 5.0f;
        check(apply_corp_command(w, reg, c, nullptr) == corp_command_result::rejected_no_capacity,
              "Q2.1 NO CAPACITY: a supplier with no producing building declines");
    }

    // --- Test 3: no input access — a processor whose recipe input is unpriced ---
    const entity_id supplier_starved = w.create_entity();
    { corporation_component cc; cc.balance = 1000.0f; w.corporations[supplier_starved] = cc; }
    {
        recipe silicon_recipe; silicon_recipe.name = "silicon";
        silicon_recipe.inputs[ri(resource_type::silica)] = 2.0f;
        silicon_recipe.outputs[ri(resource_type::silicon)] = 1.0f;
        const uint16_t rid = reg.add_recipe(silicon_recipe);
        const entity_id tile = w.create_entity();
        tile_component tc{}; tc.body = body; w.tiles[tile] = tc;
        const entity_id bld = w.create_entity();
        building_component b{}; b.tile = tile; b.type = building_type::processing_facility;
        b.recipe = rid; b.ticks_remaining = 0;
        w.buildings[bld] = b;
        w.corporations[supplier_starved].assets.push_back(bld);

        corp_command c; c.corp = buyer; c.verb = corp_verb::request_quote;
        c.counterparty = supplier_starved; c.target = resource_type::silicon; c.quantity = 5.0f;
        check(apply_corp_command(w, reg, c, nullptr) == corp_command_result::rejected_no_input_access,
              "Q2.2 NO INPUT ACCESS: a processor whose recipe input (silica) is unpriced here declines");
    }

    // --- Test 4: embargo — a hand-authored condition_set that can never hold ---
    {
        condition_set cs;
        condition c; c.subject = condition_subject::surplus; c.comparator = condition_comparator::at_least;
        c.operand = 1.0e9f; // no corp will ever hold this much cash
        cs.all.push_back(c);
        w.corp_embargo_conditions[supplier] = cs;

        corp_command cmd; cmd.corp = buyer; cmd.verb = corp_verb::request_quote;
        cmd.counterparty = supplier; cmd.target = resource_type::iron_ore; cmd.quantity = 5.0f;
        check(apply_corp_command(w, reg, cmd, nullptr) == corp_command_result::rejected_embargo,
              "Q2.3 EMBARGO: a supplier's condition_set evaluating false against the buyer declines");
        w.corp_embargo_conditions.erase(supplier); // leave clean for test 5
    }

    // --- Test 5: reputation floor ---
    {
        w.corp_reputation[{buyer, supplier}] = -100.0f;
        corp_command cmd; cmd.corp = buyer; cmd.verb = corp_verb::request_quote;
        cmd.counterparty = supplier; cmd.target = resource_type::iron_ore; cmd.quantity = 5.0f;
        check(apply_corp_command(w, reg, cmd, nullptr) == corp_command_result::rejected_reputation,
              "Q2.4 REPUTATION: a pair below the standing floor declines");
    }

    // --- Cancellation: forfeits the deposit, moves reputation down ---
    {
        w.corp_reputation[{buyer, supplier}] = 0.0f; // clear test 5's fixture
        corp_command qc; qc.corp = buyer; qc.verb = corp_verb::request_quote;
        qc.counterparty = supplier; qc.target = resource_type::iron_ore; qc.quantity = 5.0f;
        apply_corp_command(w, reg, qc, nullptr);
        const uint32_t qid = w.procurement_quotes.back().id;
        corp_command ac; ac.corp = buyer; ac.verb = corp_verb::accept_quote; ac.order = qid;
        apply_corp_command(w, reg, ac, nullptr);
        const uint32_t cid = w.procurement_contracts.back().id;
        const float balance_before_cancel = w.corporations[buyer].balance;

        corp_command cancel; cancel.corp = buyer; cancel.verb = corp_verb::cancel_contract; cancel.order = cid;
        check(apply_corp_command(w, reg, cancel, nullptr) == corp_command_result::applied,
              "R1 cancel_contract terminates an in-flight contract");
        check(w.procurement_contracts.empty(), "R1 the cancelled contract is removed");
        check(near(w.corporations[buyer].balance, balance_before_cancel),
              "Q1 FORFEIT: cancellation does not refund the already-debited deposit");
        check(w.corp_reputation[{buyer, supplier}] < 0.0f, "Q3 a cancelled contract moves reputation DOWN");
    }

    if (g_failures == 0) std::printf("\nALL PASS  (0 failures)\n");
    else                 std::printf("\nFAILURES (%d failures)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
