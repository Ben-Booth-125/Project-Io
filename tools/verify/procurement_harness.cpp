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
#include "world/procurement.hpp"
#include "world/recipe_registry.hpp"
#include "world/sentiment.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

static int g_failures = 0;

static void check(bool cond, const char* what)
{
    if (cond) std::printf("  PASS  %s\n", what);
    else      { std::printf("  FAIL  %s\n", what); ++g_failures; }
}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-2f; }
static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

// ---------------------------------------------------------------------------
// BL-546 — reputation becomes a VIEW of sentiment (requirement group
// `reputation-becomes-sentiment`, R1–R4), landing BL-391 through it.
// ---------------------------------------------------------------------------
// R1 IS A REAL BEFORE/AFTER. Every constant in `baseline` below was CAPTURED BY
// RUNNING THIS FIXTURE AGAINST THE PRE-MIGRATION TREE (2026-08-23, at 4262d66,
// where `world::corp_reputation` was still a std::map<pair,float>), dumped as
// IEEE-754 bit patterns and pasted here. It is deliberately not a fixture
// written after the change: a post-hoc fixture asserts what the new code does,
// which is not the claim R1 makes.
//
// The fixture ACCUMULATES on purpose — three completions then a cancellation on
// one pair, three cancellations on another — because a single-event fixture
// passes over a substrate that silently drops every event after the first.
// ---------------------------------------------------------------------------

/// Exact bit-pattern compare. The reputation arithmetic is +1 / −2 over values
/// that are all exactly representable, so "bit-identical" is a claim this
/// fixture can genuinely make; `near()` would hide exactly the drift R1 looks
/// for.
static bool bit_eq(float actual, uint32_t expected_bits)
{
    uint32_t a = 0;
    std::memcpy(&a, &actual, sizeof a);
    return a == expected_bits;
}

/// The pre-migration numbers. Names match the capture program's own labels.
namespace baseline {
constexpr uint32_t rep_after_complete_1 = 0x3F800000u; //  1
constexpr uint32_t rep_after_complete_2 = 0x40000000u; //  2
constexpr uint32_t rep_after_complete_3 = 0x40400000u; //  3
constexpr uint32_t rep_after_cancel_1   = 0x3F800000u; //  1  (3 − 2)
constexpr uint32_t buyer_balance        = 0x461BF3B2u; //  9980.92383
constexpr uint32_t supplier_balance     = 0x461DF9A3u; // 10110.4092
constexpr uint32_t buyer_pool_iron      = 0x00000000u; //  0 (auto-sold)
constexpr uint32_t quote_unit_price     = 0x401C0000u; //  2.4375
constexpr uint32_t quote_quantity       = 0x41A00000u; // 20
constexpr uint32_t quote_freight        = 0x00000000u; //  0 (same-body)
constexpr int      quote_lead_ticks     = 4;
constexpr uint32_t floor_value          = 0xC0A00000u; // −5
constexpr uint32_t rep_after_cancel_a   = 0xC0000000u; // −2
constexpr uint32_t rep_after_cancel_b   = 0xC0800000u; // −4
constexpr uint32_t rep_after_cancel_c   = 0xC0C00000u; // −6, below the floor
constexpr uint32_t rep_after_64_ticks   = 0xC0C00000u; // −6 STILL — BL-391's deadlock
} // namespace baseline

/// The BL-546 fixture, rebuilt from scratch per section so no section inherits
/// another's state. Mirrors the world main() builds by hand below.
struct bl546_fixture
{
    recipe_registry reg;
    world           w;
    entity_id       body = null_entity, market = null_entity;
    entity_id       supplier = null_entity, buyer = null_entity;

    bl546_fixture()
    {
        { building_economics ex; ex.base_rate = 10.0f;
          reg.set_economics(building_type::extraction_site, ex); }
        { building_economics pr; pr.base_rate = 10.0f;
          reg.set_economics(building_type::processing_facility, pr); }

        body = w.create_entity();
        w.bodies[body] = body_component{};
        market = w.create_entity();
        market_component mc; mc.body = body;
        mc.base_price[ri(resource_type::iron_ore)] = 2.5f;
        mc.base_price[ri(resource_type::silica)]   = 0.0f;
        mc.price = mc.base_price;
        w.markets[market] = mc;

        supplier = w.create_entity();
        { corporation_component cc; cc.name = "Supplier Co"; cc.balance = 10000.0f;
          cc.is_player = false; w.corporations[supplier] = cc; }
        {
            const entity_id tile = w.create_entity();
            tile_component tc{}; tc.body = body;
            tc.resource_deposit[ri(resource_type::iron_ore)] = 2.0f;
            w.tiles[tile] = tc;
            const entity_id bld = w.create_entity();
            building_component b{}; b.tile = tile; b.type = building_type::extraction_site;
            b.target_resource = resource_type::iron_ore; b.ticks_remaining = 0;
            w.buildings[bld] = b;
            w.corporations[supplier].assets.push_back(bld);
        }
        buyer = w.create_entity();
        { corporation_component cc; cc.name = "Buyer Co"; cc.balance = 10000.0f;
          cc.is_player = true; w.corporations[buyer] = cc; }
    }

    float reputation() const { return w.procurement_reputation(buyer, supplier); }

    void tick()
    {
        economy_report rep = run_economy_step(w, reg);
        auto flows = clear_markets(w, reg, rep);
        apply_budget(w, reg, flows, rep.workforce_contention);
    }

    /// Quote → accept → run to completion. Returns the quote's own result.
    corp_command_result complete_one(float qty)
    {
        const corp_command_result qr = try_quote(qty);
        if (qr != corp_command_result::applied)
            return qr;
        const uint32_t qid  = w.procurement_quotes.back().id;
        const int      lead = w.procurement_quotes.back().lead_time_ticks;
        corp_command a; a.corp = buyer; a.verb = corp_verb::accept_quote; a.order = qid;
        apply_corp_command(w, reg, a, nullptr);
        for (int t = 0; t < lead; ++t)
            tick();
        return qr;
    }

    /// Quote → accept → cancel, with no tick between. Returns the quote's result.
    corp_command_result cancel_one(float qty)
    {
        const corp_command_result qr = try_quote(qty);
        if (qr != corp_command_result::applied)
            return qr;
        const uint32_t qid = w.procurement_quotes.back().id;
        corp_command a; a.corp = buyer; a.verb = corp_verb::accept_quote; a.order = qid;
        apply_corp_command(w, reg, a, nullptr);
        const uint32_t cid = w.procurement_contracts.back().id;
        corp_command c; c.corp = buyer; c.verb = corp_verb::cancel_contract; c.order = cid;
        apply_corp_command(w, reg, c, nullptr);
        return qr;
    }

    /// A bare quote request, for reading a refusal.
    corp_command_result try_quote(float qty)
    {
        corp_command q; q.corp = buyer; q.verb = corp_verb::request_quote;
        q.counterparty = supplier; q.target = resource_type::iron_ore; q.quantity = qty;
        return apply_corp_command(w, reg, q, nullptr);
    }
};

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
    check(w.procurement_reputation(buyer, supplier) > 0.0f,
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
        // BL-546: the fixture writes the SUBSTRATE directly, because there is
        // no second store left to write. -100 is inside the default saturation
        // bound, so it is a value the fold could genuinely have reached.
        w.sentiment.pairs[{buyer, supplier}].trust = -100.0f;
        corp_command cmd; cmd.corp = buyer; cmd.verb = corp_verb::request_quote;
        cmd.counterparty = supplier; cmd.target = resource_type::iron_ore; cmd.quantity = 5.0f;
        check(apply_corp_command(w, reg, cmd, nullptr) == corp_command_result::rejected_reputation,
              "Q2.4 REPUTATION: a pair below the standing floor declines");
    }

    // --- Cancellation: forfeits the deposit, moves reputation down ---
    {
        w.sentiment.pairs.erase({buyer, supplier}); // clear test 5's fixture
                                                    // (absent == neutral, BL-545)
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
        check(w.procurement_reputation(buyer, supplier) < 0.0f, "Q3 a cancelled contract moves reputation DOWN");
    }

    // =======================================================================
    // BL-546 R1 — THE MIGRATION CHANGES NO BEHAVIOUR
    // =======================================================================
    // Against the numbers captured from the pre-migration tree. Decay is
    // unauthored here (the registry default), which is the condition R1 states.
    std::printf("\n-- BL-546 R1: bit-identical to the pre-migration build --\n");
    {
        bl546_fixture f;
        check(f.reg.sentiment().access_decay_per_tick == 0.0f &&
                  f.reg.sentiment().trust_decay_per_tick == 0.0f,
              "R1 decay is UNAUTHORED by default — the substrate is inert until economy.lua says otherwise");

        // The quoted terms, read before anything has moved the axis.
        const corp_command_result q0 = f.try_quote(20.0f);
        check(q0 == corp_command_result::applied, "R1 the first quote is issued exactly as before");
        const procurement_quote& pq = f.w.procurement_quotes.back();
        check(bit_eq(pq.unit_price, baseline::quote_unit_price) &&
                  bit_eq(pq.quantity, baseline::quote_quantity) &&
                  bit_eq(pq.freight_cost, baseline::quote_freight) &&
                  pq.lead_time_ticks == baseline::quote_lead_ticks,
              "R1 the quoted terms are bit-identical (price 2.4375, qty 20, freight 0, lead 4)");
        f.w.procurement_quotes.clear(); // that quote was read, not taken
    }
    {
        // The accumulating run: three completions, then one cancellation.
        bl546_fixture f;
        f.complete_one(20.0f);
        check(bit_eq(f.reputation(), baseline::rep_after_complete_1),
              "R1 reputation after completion 1 is bit-identical (+1)");
        f.complete_one(20.0f);
        check(bit_eq(f.reputation(), baseline::rep_after_complete_2),
              "R1 reputation ACCUMULATES to completion 2, bit-identically (+2)");
        f.complete_one(20.0f);
        check(bit_eq(f.reputation(), baseline::rep_after_complete_3),
              "R1 reputation ACCUMULATES to completion 3, bit-identically (+3)");
        f.cancel_one(5.0f);
        check(bit_eq(f.reputation(), baseline::rep_after_cancel_1),
              "R1 a cancellation subtracts from the accumulated value, bit-identically (3 − 2 = 1)");

        check(bit_eq(f.w.corporations[f.buyer].balance, baseline::buyer_balance),
              "R1 the buyer's balance after the whole run is bit-identical");
        check(bit_eq(f.w.corporations[f.supplier].balance, baseline::supplier_balance),
              "R1 the supplier's balance after the whole run is bit-identical");
        check(bit_eq(f.w.pool_for(f.buyer, f.body).quantities[ri(resource_type::iron_ore)],
                     baseline::buyer_pool_iron),
              "R1 the buyer's iron pool after the whole run is bit-identical");
    }
    {
        // The four refusals, in the order request_quote tests them. Captured as
        // enumerator values, so a reordered enum shows up here too.
        bl546_fixture f;
        const entity_id empty = f.w.create_entity();
        { corporation_component cc; cc.balance = 1000.0f; f.w.corporations[empty] = cc; }
        corp_command c; c.corp = f.buyer; c.verb = corp_verb::request_quote;
        c.counterparty = empty; c.target = resource_type::iron_ore; c.quantity = 5.0f;
        check(apply_corp_command(f.w, f.reg, c, nullptr) == corp_command_result::rejected_no_capacity,
              "R1 NO CAPACITY refusal unchanged");

        condition_set cs;
        condition cd; cd.subject = condition_subject::surplus;
        cd.comparator = condition_comparator::at_least; cd.operand = 1.0e9f;
        cs.all.push_back(cd);
        f.w.corp_embargo_conditions[f.supplier] = cs;
        check(f.try_quote(5.0f) == corp_command_result::rejected_embargo,
              "R1 EMBARGO refusal unchanged");
        f.w.corp_embargo_conditions.erase(f.supplier);

        check(bit_eq(f.reg.procurement().reputation_floor, baseline::floor_value),
              "R1 the reputation floor is bit-identical (−5)");
        f.cancel_one(5.0f);
        check(bit_eq(f.reputation(), baseline::rep_after_cancel_a), "R1 cancel 1 → −2, bit-identical");
        f.cancel_one(5.0f);
        check(bit_eq(f.reputation(), baseline::rep_after_cancel_b), "R1 cancel 2 → −4, bit-identical");
        f.cancel_one(5.0f);
        check(bit_eq(f.reputation(), baseline::rep_after_cancel_c), "R1 cancel 3 → −6, bit-identical");
        check(f.try_quote(5.0f) == corp_command_result::rejected_reputation,
              "R1 REPUTATION refusal unchanged — −6 against a floor of −5 is refused");
    }

    // =======================================================================
    // BL-546 R2 — THE FLOOR IS NO LONGER PERMANENT (BL-391)
    // =======================================================================
    // The genuine before/after. The pre-migration build ran this exact fixture
    // for 64 ticks and the value did not move ONE BIT: 0xC0C00000 in, 0xC0C00000
    // out, and the quote still refused. Decay is what changes that, and it is
    // authored data — no new verb, no reparations mechanic, no special case.
    std::printf("\n-- BL-546 R2 / BL-391: a burned relationship heals --\n");
    {
        bl546_fixture f;

        // A 180-tick half-life, which is the worked example sentiment.hpp gives
        // for converting a legible half-life into the rate that actually crosses
        // the save seam: rate = 1 − 2^(−1/180). THIS IS THE HARNESS'S OWN
        // FIXTURE, not a shipped value — the anchored rate is Ben's call
        // (BL-391's second demand) and belongs in scripts/economy.lua.
        sentiment_params sp = f.reg.sentiment();
        sp.trust_decay_per_tick = 0.00384f;
        f.reg.set_sentiment(sp);

        f.cancel_one(5.0f);
        f.cancel_one(5.0f);
        f.cancel_one(5.0f);
        check(bit_eq(f.reputation(), baseline::rep_after_cancel_c),
              "R2 the fixture reaches BL-391's own measured state: −6 against a floor of −5");
        check(f.try_quote(5.0f) == corp_command_result::rejected_reputation,
              "R2 and is refused there, exactly as the player found it");

        // It must HEAL GRADUALLY, not reset. Ten ticks in, still refused.
        for (int t = 0; t < 10; ++t)
            f.tick();
        const float after_10 = f.reputation();
        check(after_10 > -6.0f && after_10 < f.reg.procurement().reputation_floor,
              "R2 ten ticks in it has MOVED but is still below the floor — decay heals, it does not reset");
        check(f.try_quote(5.0f) == corp_command_result::rejected_reputation,
              "R2 still refused at ten ticks — the consequence is bounded, not removed");

        // Monotone toward neutral, never past it and never reversing.
        bool monotone = true;
        float prev = f.reputation();
        for (int t = 0; t < 54; ++t)
        {
            f.tick();
            const float now = f.reputation();
            if (!(now > prev) || now > 0.0f)
                monotone = false;
            prev = now;
        }
        check(monotone, "R2 every tick moves it strictly toward neutral and never past it");

        // 64 ticks total — the same span that moved the pre-migration value by
        // exactly zero (0xC0C00000 before and after).
        check(!bit_eq(f.reputation(), baseline::rep_after_64_ticks),
              "R2 after 64 ticks the value has MOVED — the pre-migration build sat at 0xC0C00000");
        check(f.reputation() > f.reg.procurement().reputation_floor,
              "R2 after 64 ticks it is back ABOVE the floor");
        check(f.try_quote(5.0f) == corp_command_result::applied,
              "R2 BL-391 DISSOLVED: the corp is quoted again, with no new verb and no reparations mechanic");
    }
    {
        // The other half of the no-floor property: a row that decays all the way
        // home is ERASED, so a fully-forgotten pair is indistinguishable from one
        // that never dealt. That indistinguishability is what leaves the deadlock
        // nowhere to live.
        bl546_fixture f;
        sentiment_params sp = f.reg.sentiment();
        sp.trust_decay_per_tick = 0.25f; // fast, so the row reaches neutral in this run
        f.reg.set_sentiment(sp);
        f.cancel_one(5.0f);
        check(f.w.sentiment.pairs.count({ f.buyer, f.supplier }) == 1,
              "R2 a moved pair holds a row");
        for (int t = 0; t < 128; ++t)
            f.tick();
        check(f.w.sentiment.pairs.count({ f.buyer, f.supplier }) == 0,
              "R2 the row is ERASED at neutral — absent == neutral, so there is no residue to be stuck at");
        check(f.reputation() == 0.0f, "R2 and it reads as exactly neutral");
    }

    // =======================================================================
    // BL-546 R3 — THE SERIALISATION SEAM TRAVELS WITH THE CHANGE
    // =======================================================================
    std::printf("\n-- BL-546 R3: the procurement stream, at version 3 --\n");
    {
        check(procurement_version == 3, "R3 procurement_version is bumped to 3");

        bl546_fixture f;
        f.try_quote(20.0f);
        const uint32_t qid = f.w.procurement_quotes.back().id;
        corp_command a; a.corp = f.buyer; a.verb = corp_verb::accept_quote; a.order = qid;
        apply_corp_command(f.w, f.reg, a, nullptr);
        f.try_quote(7.0f); // leave one live quote as well as one contract
        f.w.sentiment.pairs[{ f.buyer, f.supplier }] = sentiment_value{ 1.5f, -3.25f };

        check(!f.w.procurement_quotes.empty() && !f.w.procurement_contracts.empty(),
              "R3 the fixture holds a live quote AND a live contract to round-trip");

        std::ostringstream out(std::ios::binary);
        write_procurement(f.w, out);
        const std::string bytes = out.str();

        world back;
        std::istringstream in(bytes, std::ios::binary);
        check(read_procurement(back, in), "R3 a v3 stream reads back");
        check(back.next_procurement_id == f.w.next_procurement_id &&
                  back.procurement_quotes.size() == f.w.procurement_quotes.size() &&
                  back.procurement_contracts.size() == f.w.procurement_contracts.size(),
              "R3 the counts and the id cursor survive");
        check(back.procurement_quotes.front().unit_price == f.w.procurement_quotes.front().unit_price &&
                  back.procurement_quotes.front().buyer == f.w.procurement_quotes.front().buyer &&
                  back.procurement_contracts.front().deposit_paid ==
                      f.w.procurement_contracts.front().deposit_paid,
              "R3 the record contents survive value-for-value");

        // NO RELATIONAL VALUE CROSSES THIS STREAM ANY MORE, and the reader must
        // not clear what it does not own.
        check(back.sentiment.pairs.empty(),
              "R3 a v3 stream carries NO relational value — the substrate is not procurement's store");
        world holds_sentiment;
        holds_sentiment.sentiment.pairs[{ 11, 22 }] = sentiment_value{ 0.5f, 0.75f };
        std::istringstream in2(bytes, std::ios::binary);
        check(read_procurement(holds_sentiment, in2) &&
                  holds_sentiment.sentiment.pairs.size() == 1 &&
                  holds_sentiment.sentiment.pairs.at({ 11, 22 }).trust == 0.75f,
              "R3 reading a procurement stream LEAVES `sentiment` untouched rather than clearing it");

        // A stream at the old version is refused outright, not reinterpreted —
        // which is the whole reason the bump is the migration story here. A v2
        // stream's trailing reputation block would otherwise be consumed as
        // quote records.
        std::string stale = bytes;
        const uint32_t v2 = 2;
        std::memcpy(&stale[4], &v2, sizeof v2);
        world victim;
        victim.procurement_quotes.push_back(procurement_quote{});
        std::istringstream in3(stale, std::ios::binary);
        check(!read_procurement(victim, in3), "R3 a v2 stream is REJECTED, not reinterpreted");
        check(victim.procurement_quotes.size() == 1,
              "R3 and the rejection mutates nothing");

        // The record layout itself: exactly one quote and one contract, and no
        // third count block after them. The stream is header (magic, version,
        // next id) + 4-byte count + one quote + 4-byte count + one contract.
        const std::size_t quote_bytes    = 5 * 4 + 1 + 2 * 4 + 4 + 4; // ids, res, qty/price, lead, freight
        const std::size_t contract_bytes = quote_bytes + 4 + 4;       // + ticks_elapsed, deposit_paid
        const std::size_t expected = 3 * 4                                             // magic, version, next id
                                     + 4 + f.w.procurement_quotes.size() * quote_bytes // count + quotes
                                     + 4 + f.w.procurement_contracts.size() * contract_bytes;
        check(bytes.size() == expected,
              "R3 the stream is EXACTLY header + quotes + contracts — no trailing reputation block");
        if (bytes.size() != expected)
            std::printf("        (stream %zu bytes, expected %zu)\n", bytes.size(), expected);
    }

    // =======================================================================
    // BL-546 R4 — THERE IS ONLY ONE STORE
    // =======================================================================
    std::printf("\n-- BL-546 R4: one store, one axis --\n");
    {
        bl546_fixture f;
        f.complete_one(20.0f);

        check(f.reputation() == sentiment_toward(f.w.sentiment, f.buyer, f.supplier).trust,
              "R4 the view and the substrate agree by construction — procurement_reputation IS Trust");
        check(f.w.sentiment.pairs.size() == 1,
              "R4 procurement conduct wrote exactly ONE substrate row and no second table");

        // Procurement conduct is a TRUST fact and says nothing about Access:
        // seeding it there would invent a meaning the two rates never carried.
        check(sentiment_toward(f.w.sentiment, f.buyer, f.supplier).access == 0.0f,
              "R4 conduct moved Trust and left Access at neutral");

        // Direction: reputation is keyed (buyer, supplier), never the reverse.
        check(f.w.procurement_reputation(f.supplier, f.buyer) == 0.0f,
              "R4 the axis is DIRECTED — the supplier's own row is untouched");

        // Moving the substrate moves what request_quote sees, with no third path.
        f.w.sentiment.pairs[{ f.buyer, f.supplier }].trust = -9.0f;
        check(f.reputation() == -9.0f, "R4 moving the substrate moves the view");
        check(f.try_quote(5.0f) == corp_command_result::rejected_reputation,
              "R4 and moves what request_quote decides");

        // THE FLOOR-CONSUMER SWEEP, asserted rather than asserted-about. There
        // is exactly one consumer of the floor (corp_command.cpp's quote path)
        // and it must read "below the floor" as a condition of today. Raising
        // the row back is sufficient to be quoted again — nothing latched.
        f.w.sentiment.pairs[{ f.buyer, f.supplier }].trust = -1.0f;
        check(f.try_quote(5.0f) == corp_command_result::applied,
              "R4 the refusal is NOT TERMINAL: nothing latched when the pair crossed the floor");

        // The authored weights are the procurement rates, which is what makes
        // R1 true rather than lucky.
        const sentiment_params& sp = f.reg.sentiment();
        check(sp.factors[static_cast<std::size_t>(sentiment_factor_kind::contract_completed)].trust ==
                  f.reg.procurement().reputation_on_complete &&
              sp.factors[static_cast<std::size_t>(sentiment_factor_kind::contract_cancelled)].trust ==
                  f.reg.procurement().reputation_on_cancel,
              "R4 the two factor weights ARE the two procurement rates — seeded, not restated");

        // And they follow the procurement table rather than a literal.
        procurement_params pp = f.reg.procurement();
        pp.reputation_on_complete = 4.5f;
        f.reg.set_procurement(pp);
        check(f.reg.sentiment()
                  .factors[static_cast<std::size_t>(sentiment_factor_kind::contract_completed)]
                  .trust == 4.5f,
              "R4 re-authoring a procurement rate re-seeds its factor weight — the two cannot drift");
        bl546_fixture g;
        g.reg.set_procurement(pp);
        g.complete_one(20.0f);
        check(g.reputation() == 4.5f,
              "R4 and a completion then moves the axis by the re-authored amount");
    }

    if (g_failures == 0) std::printf("\nALL PASS  (0 failures)\n");
    else                 std::printf("\nFAILURES (%d failures)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
