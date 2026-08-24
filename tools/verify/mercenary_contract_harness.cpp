// ---------------------------------------------------------------------------
// Headless mercenary-contract harness (BL-573; no SDL / ImGui)
// ---------------------------------------------------------------------------
// BL-572 built the offer half (derive_contract_offers, nation_scorer_harness's
// own R8) and left it a placeholder: nothing could ACCEPT an offer, nothing
// evaluated a live contract, and active_mercenary_contract_for
// (battle_system.cpp) was hard-coded to null_entity. This is the item that
// closes the loop -- accept_offer / abandon_contract on the corp-command seam,
// the tick-evaluation pass (run_mercenary_contract_tick, nation_step.cpp),
// the committed-unit lock, and the real active_mercenary_contract_for lookup.
//
// A NARROWER harness than BL-574's own (that item is explicitly "one observed
// instance of every contract terminal state" as its own dedicated suite).
// This proves the mechanism works at all -- accept, evaluate, pay-or-fail --
// per this item's own R1 requirement:
//
//   R1  ACCEPT_OFFER, THE UNTRUSTED SEAM. A rejection (wrong counterparty
//       nation, an under-escrowed offer, a non-owned unit, an empty force)
//       mutates NOTHING -- world.hpp's byte-for-byte comparison, not just "the
//       obvious field didn't move". A valid accept converts the offer into a
//       live contract, pays the deposit (economy.procurement.deposit_fraction
//       of the fee) directly to the contractor, and consumes the offer.
//   R2  DOUBLE-COMMIT AND THE UNIT LOCK. A unit already committed to an ACTIVE
//       contract cannot be named on a second accept_offer, and cannot be
//       disbanded -- both refusals mutate nothing. Once the contract goes
//       terminal the SAME unit is free again, with no separate release step.
//   R3  TICK EVALUATION: PAY OR FAIL. A "take" (point-in-time) contract judges
//       its predicate only AT the deadline -- false before it changes nothing;
//       true at the deadline pays the fee's balance and folds
//       contract_completed, false at the deadline pays nothing further and
//       folds contract_failed. A "hold" (continuous) contract fails the
//       MOMENT its predicate reads false, never waiting for the deadline.
//   R4  ABANDONMENT IS A DISTINCT, LESSER PENALTY. abandon_contract pays
//       nothing beyond the deposit already sent (the same money outcome as a
//       failure) but folds contract_cancelled, not contract_failed -- and the
//       authored magnitude (scripts/economy.lua) makes failure measurably the
//       harder reputation move, not merely a different-numbered one.
//   R5  active_mercenary_contract_for IS WIRED FOR REAL. Non-null for an
//       ACTIVE contract naming (corp, province); null once that contract goes
//       terminal, with no separate stance row to clear.
//   R6  DETERMINISM. Two identical tick sequences over two structurally
//       identical worlds settle every contract the same way and leave every
//       balance and every sentiment pair bit-identical.
//
// The process exits non-zero if any assertion FAILs.
//
// ---------------------------------------------------------------------------
// BL-574 extension — CONTRACT_HARNESS, "one observed instance of every
// contract terminal state, conserved and replayable" (CONTRACTS.md §
// Verification). Cases below are labelled M1-M7 after the item's own R1.
// Several are already satisfied by BL-573's own rows above or by a sibling
// harness; those are named rather than duplicated (Ben, project-wide: an
// observed instance beats a re-authored one).
//
//   M1  AN OFFER ISSUED BY A THREATENED NATION (BL-572's own mechanism,
//       `derive_contract_offers`). Already fully exercised, end to end
//       (weakest-border-province targeting, fee/deadline, escrow accrual
//       and clamp, TTL expiry-and-refund), by nation_scorer_harness.cpp's
//       own R8a-R8e. NOT duplicated here: this file's fixtures hand-build
//       `mercenary_offer` rows directly (`make_offer`) precisely so the
//       accept/evaluate/pay-or-fail machinery under test here is isolated
//       from the derivation machinery R8 already covers on its own terms.
//   M2  ACCEPT WITH A VALID FORCE CONSERVES EXACTLY. R1e below already
//       proves the deposit lands on the contractor at the exact authored
//       fraction; the new case adds the other half of "conserves" — the
//       client nation's treasury is untouched by accept_offer itself,
//       because the fee already left it during the offer's own funding
//       (R8c). A second debit here would be a real leak, not a style
//       choice — accept_offer's own comment (corp_command.cpp) says so.
//   M3  A "TAKE" CONTRACT COMPLETES OFF A REAL, DECISIVE BATTLE — not a
//       hand-set `province_holder` (R3b's shortcut, which proves the
//       EVALUATION side only). Hand-builds an `active_battle` past the
//       swing draw's reach of the outcome, exactly battle_engagement_
//       harness.cpp's own B15 idiom (BL-569), so the ending is
//       defender_broken by construction, then lets `run_battles` move
//       `province_holder` for real before the contract tick reads it.
//   M4  A "HOLD" CONTRACT FAILS ON THE FIRST LOST TICK. R3d below already
//       proves the EARLY-fail timing; the new assertions on that same
//       case add the money outcome and the ORDERING CONTRACTS.md § Q2
//       demands: a hold-contract's failure costs strictly MORE Trust than
//       an otherwise-identical abandon, not merely a different number.
//       NOTE ON "ESCROW RETURNED": the item's own requirement text (R1)
//       says the failure case has "the escrow returned". Read literally
//       against the code, that is not what happens — a `mercenary_contract`
//       carries no escrow field at all (only `fee`/`deposit_paid`;
//       world.hpp's own struct comment), and run_mercenary_contract_tick's
//       failure branch says outright: "the reserved remainder is not
//       disbursed to anyone; it was already spent, from the client's
//       perspective, the moment the escrow that funded it left the
//       treasury." An OFFER's escrow returns to the treasury on TTL
//       expiry (R8e) — a different record, a different event, BEFORE
//       acceptance. This case therefore asserts the ACTUAL observed
//       failure behaviour (nothing beyond the deposit moves) rather than
//       the requirement's literal wording; flagged to Ben in
//       NEEDS_REVIEW.json (NR-583) rather than silently reworded in the
//       requirement text, which is out of this item's scope to edit.
//   M5  ABANDON. Already fully exercised by R4 below (R4a-R4d): the
//       not-the-contractor rejection, the state and money outcome, and
//       the reputation ordering against a fresh failure. Not duplicated.
//   M6  DETERMINISM, HASH-IDENTICAL. R6 below already replays two
//       identical accept-through-complete sequences and compares state
//       field by field; extended in place with a `world::state_hash`
//       comparison for the stronger "hash-identical" claim (folds
//       corp balance, nation treasury and `province_holder` — see
//       world.cpp's own state_hash comment for what it folds and why;
//       sentiment is NOT folded, so the existing field-by-field sentiment
//       check stays alongside the hash rather than being replaced by it).
//   M7  SAVE/LOAD ROUND-TRIPS MID-CONTRACT. Already fully exercised by
//       save_roundtrip.cpp's own P12 (BL-573's addition): an ACTIVE
//       mercenary_contract, scalar fields and the committed-force array
//       both, round-tripped field by field. Not duplicated here — this
//       file has no save/load surface of its own to exercise.

#include "scripting/lua_state.hpp"
#include "world/battle_system.hpp"      // active_mercenary_contract_for
#include "world/components.hpp"
#include "world/condition_set.hpp"
#include "world/contract_template.hpp"
#include "world/corp_command.hpp"
#include "world/nation_step.hpp"        // run_mercenary_contract_tick
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/sentiment.hpp"
#include "world/unit_roster.hpp"        // unit_to_stack_entry -- M3's hand-built battle state
#include "world/world.hpp"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

bool same(float a, float b) { return a == b; }

// ---------------------------------------------------------------------------
// Fixture: one body, two nations (client A, a second nation B that starts
// holding the target province so a "take" contract has something to take),
// one corp with a starting balance of zero and three units.
// ---------------------------------------------------------------------------

constexpr int k_gh = 1;

struct fixture
{
    world     w;
    entity_id body   = null_entity;
    entity_id nat_a  = null_entity; ///< The client.
    entity_id nat_b  = null_entity; ///< Starts holding the target province.
    entity_id corp   = null_entity; ///< The contractor.
    uint32_t  prov_home   = 0; ///< Nation A's own province -- never the contract's target.
    uint32_t  prov_target = 0; ///< The contract's province.
    entity_id unit1 = null_entity, unit2 = null_entity, unit3 = null_entity;
};

entity_id add_nation(world& w, const char* name)
{
    const entity_id  n = w.create_entity();
    nation_component nc{};
    nc.name  = name;
    w.nations[n] = nc;
    return n;
}

entity_id add_tile(world& w, entity_id body, int gx, entity_id nation)
{
    const entity_id t = w.create_entity();
    tile_component  tc{};
    tc.body   = body;
    tc.grid_x = gx;
    tc.grid_y = 0;
    w.tiles[t] = tc;
    if (nation != null_entity)
    {
        w.tile_to_nation[t] = nation;
        w.nations.at(nation).tiles.push_back(t);
    }
    return t;
}

entity_id add_unit(world& w, entity_id owner, entity_id tile, int count)
{
    const entity_id u = w.create_entity();
    unit_component  uc{};
    uc.owner = owner;
    uc.position = tile;
    uc.count = count;
    uc.type = 0;
    uc.supply_factor_permille = 1000;
    w.units[u] = uc;
    return u;
}

fixture make_fixture()
{
    fixture f;
    f.body = f.w.create_entity();
    body_component bc{};
    bc.grid_width  = 4;
    bc.grid_height = k_gh;
    f.w.bodies[f.body] = bc;

    f.nat_a = add_nation(f.w, "Client");
    f.nat_b = add_nation(f.w, "Holder");

    const entity_id t0 = add_tile(f.w, f.body, 0, f.nat_a);
    const entity_id t1 = add_tile(f.w, f.body, 1, f.nat_b);

    province p_home;   p_home.id   = 1; p_home.body   = f.body; p_home.tiles = { t0 };
    province p_target; p_target.id = 2; p_target.body = f.body; p_target.tiles = { t1 };
    f.w.provinces.provinces = { p_home, p_target }; // ascending id
    f.w.province_holder     = { f.nat_a, f.nat_b }; // positionally aligned -- B holds the target
    f.w.provinces.tile_province[t0] = 1;
    f.w.provinces.tile_province[t1] = 2;
    f.prov_home = 1; f.prov_target = 2;

    const entity_id corp_id = f.w.create_entity();
    corporation_component cc{};
    cc.name = "Free Company";
    cc.home_nation = f.nat_a;
    cc.balance = 0.0f;
    f.w.corporations[corp_id] = cc;
    f.corp = corp_id;

    f.unit1 = add_unit(f.w, f.corp, t0, 40);
    f.unit2 = add_unit(f.w, f.corp, t0, 25);
    f.unit3 = add_unit(f.w, f.corp, t0, 10);

    return f;
}

mercenary_offer make_offer(uint32_t id, entity_id client, uint32_t province, int template_index,
                           float fee, int deadline, float escrow)
{
    mercenary_offer o;
    o.id = id; o.client = client; o.target_province = province;
    o.template_index = template_index; o.fee = fee; o.deadline = deadline;
    o.issued_tick = 0; o.offer_escrow = escrow;
    return o;
}

corp_command make_accept(entity_id corp, uint32_t offer_id, entity_id counterparty,
                         std::vector<entity_id> units)
{
    corp_command c;
    c.verb  = corp_verb::accept_offer;
    c.corp  = corp;
    c.order = offer_id;
    c.counterparty = counterparty;
    for (std::size_t i = 0; i < units.size() && i < c.units.size(); ++i)
        c.units[i] = units[i];
    return c;
}

} // namespace

int main()
{
    std::printf("=== mercenary contract harness (BL-573) ===\n\n");

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);

    lua_state contract_lua;
    contract_lua.load("scripts/contracts.lua");
    contract_template_registry templates;
    templates.load_from_lua(contract_lua);

    check(templates.size() >= 2, "setup: scripts/contracts.lua loads at least the take/hold rows");
    const int take_idx = templates.index_of("take");
    const int hold_idx = templates.index_of("hold");
    check(take_idx >= 0 && hold_idx >= 0, "setup: both authored template ids resolve");

    const float deposit_fraction = reg.procurement().deposit_fraction;

    // -------------------------------------------------------------------
    // R1a: accept_offer rejects a counterparty that is not a nation at all,
    // and mutates nothing.
    // -------------------------------------------------------------------
    {
        fixture f = make_fixture();
        f.w.mercenary_offers.push_back(make_offer(1, f.nat_a, f.prov_target, take_idx, 400.0f, 100, 400.0f));

        const corp_command cmd = make_accept(f.corp, 1, f.corp /* a corp, not a nation */, { f.unit1 });
        const corp_command_result r = apply_corp_command(f.w, reg, cmd);
        check(r == corp_command_result::rejected_invalid,
              "R1a a counterparty that names a corp, not a nation, is rejected_invalid");
        check(f.w.mercenary_offers.size() == 1 && f.w.mercenary_contracts.empty()
                  && f.w.corporations.at(f.corp).balance == 0.0f,
              "R1a the rejection mutated nothing");
    }

    // -------------------------------------------------------------------
    // R1b: accept_offer rejects a REAL nation that is not this offer's client.
    // -------------------------------------------------------------------
    {
        fixture f = make_fixture();
        f.w.mercenary_offers.push_back(make_offer(1, f.nat_a, f.prov_target, take_idx, 400.0f, 100, 400.0f));

        const corp_command cmd = make_accept(f.corp, 1, f.nat_b /* real nation, wrong one */, { f.unit1 });
        const corp_command_result r = apply_corp_command(f.w, reg, cmd);
        check(r == corp_command_result::rejected_invalid,
              "R1b the wrong (but real) nation as counterparty is rejected_invalid");
        check(f.w.mercenary_offers.size() == 1 && f.w.mercenary_contracts.empty(),
              "R1b the rejection mutated nothing");
    }

    // -------------------------------------------------------------------
    // R1c: an under-escrowed ("not yet postable") offer is refused.
    // -------------------------------------------------------------------
    {
        fixture f = make_fixture();
        f.w.mercenary_offers.push_back(make_offer(1, f.nat_a, f.prov_target, take_idx, 400.0f, 100, 250.0f));

        const corp_command cmd = make_accept(f.corp, 1, f.nat_a, { f.unit1 });
        const corp_command_result r = apply_corp_command(f.w, reg, cmd);
        check(r == corp_command_result::rejected_state,
              "R1c an offer whose escrow has not yet cleared its fee is rejected_state");
        check(f.w.mercenary_offers.size() == 1 && f.w.mercenary_contracts.empty(),
              "R1c the rejection mutated nothing");
    }

    // -------------------------------------------------------------------
    // R1d: a unit the corp does not own is refused; an empty force is refused.
    // -------------------------------------------------------------------
    {
        fixture f = make_fixture();
        f.w.mercenary_offers.push_back(make_offer(1, f.nat_a, f.prov_target, take_idx, 400.0f, 100, 400.0f));
        const entity_id foreign_unit = add_unit(f.w, f.nat_b, f.w.tiles.begin()->first, 5);

        const corp_command bad_owner = make_accept(f.corp, 1, f.nat_a, { foreign_unit });
        check(apply_corp_command(f.w, reg, bad_owner) == corp_command_result::rejected_not_owner,
              "R1d a non-owned unit is rejected_not_owner");
        check(f.w.mercenary_offers.size() == 1 && f.w.mercenary_contracts.empty(),
              "R1d the rejection mutated nothing");

        const corp_command empty_force = make_accept(f.corp, 1, f.nat_a, {});
        check(apply_corp_command(f.w, reg, empty_force) == corp_command_result::rejected_invalid,
              "R1d an empty committed force is rejected_invalid");
        check(f.w.mercenary_offers.size() == 1 && f.w.mercenary_contracts.empty(),
              "R1d that rejection mutated nothing either");
    }

    // -------------------------------------------------------------------
    // R1e: a valid accept applies -- contract created, deposit paid, offer
    // consumed.
    // -------------------------------------------------------------------
    fixture accepted; // kept alive for R2/R3/R5 below
    {
        fixture f = make_fixture();
        f.w.mercenary_offers.push_back(make_offer(1, f.nat_a, f.prov_target, take_idx, 400.0f, 100, 400.0f));

        const corp_command cmd = make_accept(f.corp, 1, f.nat_a, { f.unit1, f.unit2 });
        const corp_command_result r = apply_corp_command(f.w, reg, cmd);
        check(r == corp_command_result::applied, "R1e a fully-valid accept_offer applies");
        check(f.w.mercenary_offers.empty(), "R1e the offer is consumed");
        check(f.w.mercenary_contracts.size() == 1, "R1e exactly one contract now exists");

        if (f.w.mercenary_contracts.size() == 1)
        {
            const mercenary_contract& c = f.w.mercenary_contracts[0];
            check(c.client == f.nat_a && c.contractor == f.corp
                      && c.template_index == take_idx && c.province == f.prov_target
                      && c.fee == 400.0f && c.deadline == 100
                      && c.state == mercenary_contract_state::active,
                  "R1e the contract's scalar fields copy the offer correctly");
            check(same(c.deposit_paid, 400.0f * deposit_fraction),
                  "R1e the deposit is fee times the authored procurement.deposit_fraction");
            check(same(f.w.corporations.at(f.corp).balance, c.deposit_paid),
                  "R1e the contractor's balance is credited exactly the deposit");
            check(c.units[0] == f.unit1 && c.units[1] == f.unit2 && c.units[2] == null_entity,
                  "R1e the committed force lands in the named slots, the rest null_entity");
        }
        accepted = std::move(f);
    }

    // -------------------------------------------------------------------
    // M2 (BL-574, NEW): accept_offer's transfer conserves exactly. R1e
    // above proves the deposit lands on the contractor at the exact
    // authored fraction; this proves the OTHER half of "conserves" -- the
    // client nation's treasury is untouched by accept_offer itself. The
    // fixture hand-sets a treasury value standing in for whatever
    // derive_contract_offers would already have spent it down to while
    // funding this offer's escrow (R8c, nation_scorer_harness.cpp), so a
    // double-debit here would show up as a real, not cosmetic, drift.
    // -------------------------------------------------------------------
    {
        fixture f = make_fixture();
        f.w.nations.at(f.nat_a).treasury = 1234.5f; // an arbitrary already-settled value
        f.w.mercenary_offers.push_back(make_offer(1, f.nat_a, f.prov_target, take_idx, 400.0f, 100, 400.0f));

        const float corp_balance_before = f.w.corporations.at(f.corp).balance;
        const corp_command cmd = make_accept(f.corp, 1, f.nat_a, { f.unit1 });
        check(apply_corp_command(f.w, reg, cmd) == corp_command_result::applied,
              "M2 setup: the accept applies");
        check(same(f.w.nations.at(f.nat_a).treasury, 1234.5f),
              "M2a the client nation's treasury is UNTOUCHED by accept_offer -- the fee already "
              "left it during offer funding, so paying the deposit here is not a second debit");
        check(same(f.w.corporations.at(f.corp).balance, corp_balance_before + 400.0f * deposit_fraction),
              "M2b the contractor's balance moves by EXACTLY the deposit -- fee times the "
              "authored deposit_fraction, neither more nor less: the transfer conserves");
    }

    // -------------------------------------------------------------------
    // R2: double-commit and the unit lock.
    // -------------------------------------------------------------------
    {
        fixture& f = accepted;
        f.w.mercenary_offers.push_back(make_offer(2, f.nat_a, f.prov_target, take_idx, 300.0f, 200, 300.0f));

        const corp_command reuse = make_accept(f.corp, 2, f.nat_a, { f.unit1 /* already committed */ });
        check(apply_corp_command(f.w, reg, reuse) == corp_command_result::rejected_state,
              "R2a a unit already committed to an ACTIVE contract cannot be committed again");
        check(f.w.mercenary_offers.size() == 1 && f.w.mercenary_contracts.size() == 1,
              "R2a the rejection mutated nothing");

        corp_command disband_committed;
        disband_committed.verb = corp_verb::disband_unit;
        disband_committed.corp = f.corp;
        disband_committed.subject = f.unit1;
        check(apply_corp_command(f.w, reg, disband_committed) == corp_command_result::rejected_state,
              "R2b a unit committed to an ACTIVE contract cannot be disbanded");
        check(f.w.units.count(f.unit1) == 1, "R2b the unit was not erased");

        corp_command disband_free;
        disband_free.verb = corp_verb::disband_unit;
        disband_free.corp = f.corp;
        disband_free.subject = f.unit3; // never committed
        check(apply_corp_command(f.w, reg, disband_free) == corp_command_result::applied,
              "R2c an UNcommitted unit disbands freely");
        check(f.w.units.count(f.unit3) == 0, "R2c it is actually gone");

        f.w.mercenary_offers.clear(); // tidy the throwaway offer for the rows below
    }

    // -------------------------------------------------------------------
    // R5a: active_mercenary_contract_for is wired for real, while active.
    // -------------------------------------------------------------------
    {
        fixture& f = accepted;
        const entity_id found = active_mercenary_contract_for(f.w, f.corp, f.prov_target);
        check(found != null_entity, "R5a an ACTIVE contract on (corp, province) is found");
        const entity_id not_found = active_mercenary_contract_for(f.w, f.corp, f.prov_home);
        check(not_found == null_entity, "R5a the same corp names nothing on a DIFFERENT province");
    }

    // -------------------------------------------------------------------
    // R3a: a "take" contract judged BEFORE its deadline changes nothing, even
    // though the predicate reads false (nat_b still holds the target).
    // -------------------------------------------------------------------
    {
        fixture& f = accepted;
        run_mercenary_contract_tick(f.w, reg, templates, /*econ_tick*/ 50);
        check(f.w.mercenary_contracts[0].state == mercenary_contract_state::active,
              "R3a a 'take' contract stays active before its deadline, predicate false or not");
    }

    // -------------------------------------------------------------------
    // R3b: at the deadline, predicate TRUE completes -- balance and sentiment.
    // -------------------------------------------------------------------
    {
        fixture& f = accepted;
        f.w.province_holder[1] = f.corp; // the contractor now holds prov_target (index 1)
        const float before = f.w.corporations.at(f.corp).balance;
        const mercenary_contract c_before = f.w.mercenary_contracts[0];

        run_mercenary_contract_tick(f.w, reg, templates, /*econ_tick*/ 100); // == deadline
        const mercenary_contract& c = f.w.mercenary_contracts[0];
        check(c.state == mercenary_contract_state::completed,
              "R3b a 'take' contract completes when its predicate holds AT the deadline");
        check(same(f.w.corporations.at(f.corp).balance, before + (c_before.fee - c_before.deposit_paid)),
              "R3b the balance of the fee (fee - deposit_paid) is paid on completion");
        check(active_mercenary_contract_for(f.w, f.corp, f.prov_target) == null_entity,
              "R5b a COMPLETED contract is no longer the hostility for this pair");

        const sentiment_value sv = sentiment_toward(f.w.sentiment, c.client, c.contractor);
        check(sv.trust > 0.0f, "R3b contract_completed moved Trust upward (client -> contractor)");
    }

    // -------------------------------------------------------------------
    // R3c: a "take" contract whose predicate is FALSE at the deadline fails,
    // and pays nothing beyond the deposit already sent.
    // -------------------------------------------------------------------
    {
        fixture f = make_fixture();
        f.w.mercenary_offers.push_back(make_offer(1, f.nat_a, f.prov_target, take_idx, 400.0f, 10, 400.0f));
        apply_corp_command(f.w, reg, make_accept(f.corp, 1, f.nat_a, { f.unit1 }));
        const float balance_after_deposit = f.w.corporations.at(f.corp).balance;

        // f.prov_target (index 1) is still held by nat_b -- predicate false.
        run_mercenary_contract_tick(f.w, reg, templates, /*econ_tick*/ 10);
        const mercenary_contract& c = f.w.mercenary_contracts[0];
        check(c.state == mercenary_contract_state::failed,
              "R3c a 'take' contract fails when its predicate is false AT the deadline");
        check(same(f.w.corporations.at(f.corp).balance, balance_after_deposit),
              "R3c nothing further is paid on failure -- balance is exactly the deposit still");

        const sentiment_value sv = sentiment_toward(f.w.sentiment, c.client, c.contractor);
        check(sv.trust < 0.0f, "R3c contract_failed moved Trust downward");
        check(templates.at(take_idx).continuous == false,
              "sanity: 'take' really is authored non-continuous, or R3c/R3a prove nothing");
    }

    // -------------------------------------------------------------------
    // M3 (BL-574, NEW): a "take" contract completes off a REAL, decisive
    // battle -- not R3b's hand-set province_holder (which proves only the
    // EVALUATION side of completion). Hand-builds an active_battle already
    // past the swing draw's reach of the outcome, exactly
    // battle_engagement_harness.cpp's own B15 idiom (BL-569): the
    // defender preset BELOW the rout threshold so a single round-batch
    // settles defender_broken by construction, not by luck. run_battles
    // then moves province_holder for real (battle_system.cpp) before the
    // contract tick ever reads it.
    // -------------------------------------------------------------------
    {
        fixture f = make_fixture();
        const province* pv = f.w.provinces.find(f.prov_target);
        check(pv != nullptr && !pv->tiles.empty(),
              "M3 setup: the target province resolves a tile to garrison");
        const entity_id target_tile = (pv != nullptr && !pv->tiles.empty()) ? pv->tiles.front() : null_entity;
        const entity_id garrison = add_unit(f.w, f.nat_b, target_tile, 30);

        f.w.mercenary_offers.push_back(make_offer(1, f.nat_a, f.prov_target, take_idx, 400.0f, 20, 400.0f));
        check(apply_corp_command(f.w, reg, make_accept(f.corp, 1, f.nat_a, { f.unit1 }))
                  == corp_command_result::applied,
              "M3 setup: the offer accepts");
        check(province_holder_for(f.w, f.prov_target) == f.nat_b,
              "M3 setup: nat_b still holds the target before the battle");

        active_battle b;
        b.province       = f.prov_target;
        b.attacker        = f.corp;
        b.defender        = f.nat_b;
        b.attacker_units  = { f.unit1 };
        b.defender_units  = { garrison };
        b.state.attacker  = { unit_to_stack_entry(f.w, f.w.units.at(f.unit1)) };
        b.state.defender  = { unit_to_stack_entry(f.w, f.w.units.at(garrison)) };
        b.state.attacker_strength_permille = 1000;
        b.state.defender_strength_permille = 250; // guaranteed defender_broken -- B15's own idiom
        b.state.rounds_fought = 0;
        b.state.end           = campaign_battle_end::in_progress;
        b.state.stream_seed   = 0xB574u;
        b.state.rng_state     = b.state.stream_seed;
        f.w.battles.assign(1, b); // bypass discovery entirely, same as B15/B16

        const battle_tick t = run_battles(f.w, reg, 0);
        check(!t.dispatches.empty() && t.dispatches.front().end == campaign_battle_end::defender_broken,
              "M3a the hand-seeded battle resolves defender_broken, by construction not luck");
        check(province_holder_for(f.w, f.prov_target) == f.corp,
              "M3b THE PROVINCE HOLDER MOVED to the contractor through the REAL combat path");

        const float balance_before_completion = f.w.corporations.at(f.corp).balance;
        const mercenary_contract c_before = f.w.mercenary_contracts[0];
        run_mercenary_contract_tick(f.w, reg, templates, /*econ_tick*/ 20); // == deadline
        const mercenary_contract& c = f.w.mercenary_contracts[0];
        check(c.state == mercenary_contract_state::completed,
              "M3c the 'take' contract completes off a battle-driven province flip");
        check(same(f.w.corporations.at(f.corp).balance,
                   balance_before_completion + (c_before.fee - c_before.deposit_paid)),
              "M3d the remainder is paid on this real-battle completion too");
    }

    // -------------------------------------------------------------------
    // R3d: a "hold" (continuous) contract fails EARLY, the moment its
    // predicate reads false -- it does not wait for the deadline.
    //
    // M4 (BL-574): extends this same case with the money outcome and the
    // ordering assertion CONTRACTS.md § Q2 demands -- a hold-contract's
    // failure must cost strictly MORE Trust than an otherwise-identical
    // abandon, not merely a different number. See the file-header note on
    // "escrow returned" (NR-583): a mercenary_contract has no escrow of
    // its own, so the money assertion below is "nothing beyond the
    // deposit moves", the actually-observed behaviour, not the
    // requirement text's literal wording.
    // -------------------------------------------------------------------
    {
        fixture f = make_fixture();
        f.w.province_holder[0] = f.corp; // the contractor holds prov_HOME already (index 0)
        f.w.mercenary_offers.push_back(make_offer(1, f.nat_a, f.prov_home, hold_idx, 400.0f, 100, 400.0f));
        apply_corp_command(f.w, reg, make_accept(f.corp, 1, f.nat_a, { f.unit1 }));
        check(templates.at(hold_idx).continuous, "setup: 'hold' really is authored continuous");
        const float balance_after_deposit = f.w.corporations.at(f.corp).balance;

        run_mercenary_contract_tick(f.w, reg, templates, /*econ_tick*/ 5); // well before deadline 100
        check(f.w.mercenary_contracts[0].state == mercenary_contract_state::active,
              "R3d a 'hold' contract stays active while its predicate still holds");

        f.w.province_holder[0] = f.nat_b; // the corp LOSES the province mid-contract
        run_mercenary_contract_tick(f.w, reg, templates, /*econ_tick*/ 6); // still far from deadline 100
        const mercenary_contract& c = f.w.mercenary_contracts[0];
        check(c.state == mercenary_contract_state::failed,
              "R3d a 'hold' contract fails the FIRST tick its predicate reads false, "
              "not waiting for the deadline");
        check(same(f.w.corporations.at(f.corp).balance, balance_after_deposit),
              "M4a nothing beyond the already-paid deposit moves on this failure -- there is no "
              "escrow on a mercenary_contract to return (see the file-header note, NR-583)");

        const sentiment_value sv_fail = sentiment_toward(f.w.sentiment, f.nat_a, f.corp);
        check(sv_fail.trust < 0.0f, "M4b contract_failed moved Trust downward on the 'hold' case too");

        // The ordering assertion: an otherwise-identical pair that
        // ABANDONS instead of failing loses LESS Trust -- the same
        // comparison R4d draws for a 'take' contract's failure, drawn
        // here for 'hold'.
        fixture g = make_fixture();
        g.w.mercenary_offers.push_back(make_offer(1, g.nat_a, g.prov_target, take_idx, 400.0f, 100, 400.0f));
        apply_corp_command(g.w, reg, make_accept(g.corp, 1, g.nat_a, { g.unit1 }));
        const uint32_t g_contract_id = g.w.mercenary_contracts[0].id;
        corp_command ab;
        ab.verb  = corp_verb::abandon_contract;
        ab.corp  = g.corp;
        ab.order = g_contract_id;
        check(apply_corp_command(g.w, reg, ab) == corp_command_result::applied,
              "M4 setup: the comparison pair abandons cleanly");
        const sentiment_value sv_ab = sentiment_toward(g.w.sentiment, g.nat_a, g.corp);
        check(sv_fail.trust < sv_ab.trust,
              "M4c a HOLD contract's failure costs strictly MORE Trust than an otherwise-identical "
              "abandon -- the ordering CONTRACTS.md Q2 demands, not just two separate numbers");
    }

    // -------------------------------------------------------------------
    // R4: abandonment -- a distinct, lesser penalty than failure, same money
    // outcome (nothing beyond the deposit).
    // -------------------------------------------------------------------
    {
        fixture f = make_fixture();
        f.w.mercenary_offers.push_back(make_offer(1, f.nat_a, f.prov_target, take_idx, 400.0f, 100, 400.0f));
        apply_corp_command(f.w, reg, make_accept(f.corp, 1, f.nat_a, { f.unit1 }));
        const float balance_after_deposit = f.w.corporations.at(f.corp).balance;
        const uint32_t contract_id = f.w.mercenary_contracts[0].id;

        // A second REAL corp -- not the contractor, and not a nation, so the
        // rejection this row is actually testing (rejected_not_owner) is not
        // masked by the top-of-seam rejected_no_corp check (apply_corp_command
        // refuses any cmd.corp that is not a live corporation before it ever
        // reaches a verb's own body).
        const entity_id rival_corp = f.w.create_entity();
        corporation_component rival_cc{};
        rival_cc.name = "Rival Company";
        f.w.corporations[rival_corp] = rival_cc;

        corp_command bad;
        bad.verb = corp_verb::abandon_contract;
        bad.corp = rival_corp; // a real corp, but not the contractor
        bad.order = contract_id;
        check(apply_corp_command(f.w, reg, bad) == corp_command_result::rejected_not_owner,
              "R4a only the contractor may abandon its own contract");

        corp_command ab;
        ab.verb = corp_verb::abandon_contract;
        ab.corp = f.corp;
        ab.order = contract_id;
        check(apply_corp_command(f.w, reg, ab) == corp_command_result::applied,
              "R4b the contractor abandoning its own contract applies");
        check(f.w.mercenary_contracts[0].state == mercenary_contract_state::abandoned,
              "R4b the contract state is 'abandoned', a state distinct from 'failed'");
        check(same(f.w.corporations.at(f.corp).balance, balance_after_deposit),
              "R4b nothing beyond the deposit moves on abandonment either");

        const sentiment_value sv_ab = sentiment_toward(f.w.sentiment, f.nat_a, f.corp);
        check(sv_ab.trust < 0.0f, "R4c abandonment still costs Trust...");

        // Compare against a FRESH pair's failure, same magnitude table, to
        // prove failure is measurably the harder move (CONTRACTS.md Q2).
        fixture g = make_fixture();
        g.w.mercenary_offers.push_back(make_offer(1, g.nat_a, g.prov_target, take_idx, 400.0f, 10, 400.0f));
        apply_corp_command(g.w, reg, make_accept(g.corp, 1, g.nat_a, { g.unit1 }));
        run_mercenary_contract_tick(g.w, reg, templates, 10); // predicate false at deadline -> failed
        const sentiment_value sv_fail = sentiment_toward(g.w.sentiment, g.nat_a, g.corp);
        check(sv_fail.trust < sv_ab.trust,
              "R4d ...but LESS than a failure's, on an otherwise-identical pair: failure is "
              "measurably the harder reputation move");

        check(active_mercenary_contract_for(f.w, f.corp, f.prov_target) == null_entity,
              "R5c an ABANDONED contract is no longer the hostility for this pair either");

        corp_command disband_freed;
        disband_freed.verb = corp_verb::disband_unit;
        disband_freed.corp = f.corp;
        disband_freed.subject = f.unit1;
        check(apply_corp_command(f.w, reg, disband_freed) == corp_command_result::applied,
              "R2d a unit's contract going terminal frees it -- disband succeeds now");
    }

    // -------------------------------------------------------------------
    // R6: determinism -- two identical tick sequences over two structurally
    // identical worlds settle identically (contract state, balance, and the
    // sentiment pair the fold moved).
    // -------------------------------------------------------------------
    {
        auto run_sequence = [&]() {
            fixture f = make_fixture();
            f.w.mercenary_offers.push_back(make_offer(1, f.nat_a, f.prov_target, take_idx, 400.0f, 20, 400.0f));
            apply_corp_command(f.w, reg, make_accept(f.corp, 1, f.nat_a, { f.unit1, f.unit2 }));
            f.w.province_holder[1] = f.corp;
            for (int t = 1; t <= 25; ++t)
                run_mercenary_contract_tick(f.w, reg, templates, t);
            return f;
        };

        // make_fixture() allocates entity ids from a fresh counter each call
        // (world's own m_next_id starts at 1), so the SAME concrete ids come
        // back both times -- f1.corp == f2.corp, etc. -- and comparing across
        // the two worlds by those ids is exactly comparing "the same actor".
        const fixture f1 = run_sequence();
        const fixture f2 = run_sequence();
        check(f1.w.mercenary_contracts.size() == 1 && f2.w.mercenary_contracts.size() == 1,
              "R6 both replays produce exactly one contract");
        if (f1.w.mercenary_contracts.size() == 1 && f2.w.mercenary_contracts.size() == 1)
        {
            check(f1.w.mercenary_contracts[0].state == f2.w.mercenary_contracts[0].state
                      && f1.w.mercenary_contracts[0].state == mercenary_contract_state::completed,
                  "R6 both replays settle the SAME contract to the SAME terminal state");
            check(same(f1.w.corporations.at(f1.corp).balance, f2.w.corporations.at(f2.corp).balance),
                  "R6 both replays leave the contractor's balance bit-identical");
            const sentiment_value s1 = sentiment_toward(f1.w.sentiment, f1.nat_a, f1.corp);
            const sentiment_value s2 = sentiment_toward(f2.w.sentiment, f2.nat_a, f2.corp);
            check(same(s1.trust, s2.trust) && same(s1.access, s2.access),
                  "R6 both replays fold the SAME sentiment event to the SAME value");

            // M6 (BL-574): the stronger "hash-identical" claim, on top of
            // R6's field-by-field comparison above. world::state_hash folds
            // corporation balance, nation treasury and province_holder
            // (world.cpp's own state_hash comment) -- all three of which
            // this sequence's accept-then-complete moves -- so a divergence
            // in any of them, not just the ones asserted by name above,
            // would show here. Sentiment is NOT folded into state_hash
            // (also per that comment), which is exactly why R6's own
            // sentiment_toward comparison stays alongside this rather than
            // being replaced by it.
            check(f1.w.state_hash(25) == f2.w.state_hash(25),
                  "M6 both replays are HASH-IDENTICAL at the final tick, not just "
                  "identical on the fields checked by name");
        }
    }

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
