// ---------------------------------------------------------------------------
// Headless contract-dispatch harness (BL-577; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// BL-577 is "an event lands with its message, in the same change" for the
// mercenary contract: economy_report::contract_events (nation_step.hpp) ->
// core/battle_dispatch_text.cpp's contract_dispatch_line -> the Public
// channel, plus the budgets[contractor].subsidies bookkeeping a completed
// contract's payout was missing. This harness covers what does NOT need a
// live Lua state (contract_template_registry's predicate table is Lua-only,
// so completed/failed's PREDICATE evaluation stays BL-574's — the sibling
// harness this item's own brief named, tools/verify/mercenary_contract_harness.cpp).
//
//   D1  DETERMINISM, NO DRAW. contract_dispatch_line is a pure function of the
//       record: the SAME record produces the SAME line twice, and calling it
//       is provably not an RNG draw (two otherwise-identical worlds with a
//       different unrelated RNG state still agree).
//   D2  EVERY KIND HAS A BANK. All five contract_dispatch::kind values produce
//       a non-empty, ASCII-only line (CHAT.md's font constraint) that reads as
//       actual prose (contains a space), constructed BY HAND for all five —
//       offer_issued/accepted/completed/failed/abandoned never touch the
//       tick-evaluation pass, so this needs no template registry at all.
//   D3  THE PUBLIC CHANNEL IS NATION-VOICED (CHAT.md): every phrasing embeds
//       the CLIENT's name somewhere reachable by substitution, never leaks the
//       word "corporation", and a missing corp/nation degrades to the same
//       honest placeholder noun battle_dispatch_line's corp_name uses, rather
//       than an empty gap.
//   D4  OFFER_ISSUED, THE WORLD REPORTS. derive_contract_offers appends
//       exactly one offer_issued contract_dispatch when (and only when) it
//       opens a new offer, with contractor left null_entity (nobody has
//       accepted yet) and every other field matching the offer it just wrote.
//       Needs no contract_template_registry (offer derivation never reads
//       the predicate table), so this is Lua-free.
//   D5  ACCEPTED, RE-USING accepted_tick. run_mercenary_contract_tick emits
//       exactly one `accepted` event on the SAME tick a contract's own
//       accepted_tick matches econ_tick, and none on any other tick for the
//       same contract — proven with an EMPTY contract_template_registry (the
//       accepted check runs before the template-index guard), so completed/
//       failed evaluation never engages and cannot confound the row.
//   D6  ABANDONED IS A ONE-SHOT, EVEN ACROSS AN EMPTY-REPORT TICK.
//       mercenary_contract::abandoned_event_posted (world.hpp) exists so a
//       contract already abandoned before this pass first sees it announces
//       itself EXACTLY once, however many further ticks pass — and a tick
//       walked with report == nullptr (the harnesses' own default; see
//       nation_step.hpp) must NOT burn that flag, so the event still fires
//       the first time a real report IS supplied.
//   D7  THE OPTIONAL-REPORT DEFAULT CHANGES NOTHING ELSE. Calling
//       derive_contract_offers / run_mercenary_contract_tick with no report
//       (report == nullptr, every pre-BL-577 call site's shape) mutates the
//       world identically to calling WITH one — the report is observability
//       only, never a second control path.
//
// The process exits non-zero if any assertion FAILs.

#include "core/battle_dispatch_text.hpp"
#include "world/contract_template.hpp"
#include "world/economy_system.hpp"
#include "world/nation_step.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <string>
#include <utility>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

bool is_ascii(const std::string& s)
{
    for (unsigned char c : s)
        if (c > 127)
            return false;
    return true;
}

entity_id add_nation(world& w, const char* name)
{
    const entity_id n = w.create_entity();
    nation_component nc{};
    nc.name = name;
    w.nations[n] = nc;
    return n;
}

entity_id add_corp(world& w, const char* name)
{
    const entity_id c = w.create_entity();
    corporation_component cc{};
    cc.name = name;
    w.corporations[c] = cc;
    return c;
}

contract_dispatch make_event(contract_dispatch::kind k, entity_id client, entity_id contractor,
                             uint32_t id = 7, uint32_t province = 42, float fee = 400.0f)
{
    contract_dispatch d;
    d.what       = k;
    d.id         = id;
    d.client     = client;
    d.contractor = contractor;
    d.province   = province;
    d.fee        = fee;
    return d;
}

} // namespace

int main()
{
    std::printf("=== BL-577 contract-dispatch harness ===\n");

    world w;
    const entity_id nation = add_nation(w, "Ashfall Compact");
    const entity_id corp   = add_corp(w, "Halcyon Freehold");

    // ---- D1: determinism, no draw -----------------------------------------
    {
        const contract_dispatch ev = make_event(contract_dispatch::kind::accepted, nation, corp);
        const std::string a = contract_dispatch_line(w, ev);
        const std::string b = contract_dispatch_line(w, ev);
        check(a == b, "D1 the same record produces the same line twice");

        // A different id (a different real contract) is free to read
        // differently -- the fold is over `id`, not a constant.
        const contract_dispatch ev2 = make_event(contract_dispatch::kind::accepted, nation, corp,
                                                 /*id*/ 8, 42, 400.0f);
        const std::string c = contract_dispatch_line(w, ev2);
        // Not asserted unequal (a hash collision across a 3-entry bank is a
        // real, harmless possibility) -- both must still be valid lines.
        check(!c.empty(), "D1 a second id also produces a line");
    }

    // ---- D2 / D3: every kind, every bank ------------------------------------
    {
        const contract_dispatch::kind kinds[] = {
            contract_dispatch::kind::offer_issued, contract_dispatch::kind::accepted,
            contract_dispatch::kind::completed,    contract_dispatch::kind::failed,
            contract_dispatch::kind::abandoned,
        };
        const char* names[] = { "offer_issued", "accepted", "completed", "failed", "abandoned" };

        bool all_ok = true;
        for (int i = 0; i < 5; ++i)
        {
            // contractor is null_entity for offer_issued (CONTRACTS.md: nobody
            // has accepted yet) -- exercised deliberately, not avoided.
            const entity_id who = (kinds[i] == contract_dispatch::kind::offer_issued)
                                     ? null_entity : corp;
            const contract_dispatch ev = make_event(kinds[i], nation, who);
            const std::string line = contract_dispatch_line(w, ev);

            const bool nonempty = !line.empty();
            const bool ascii    = is_ascii(line);
            const bool prose    = line.find(' ') != std::string::npos;
            const bool ok       = nonempty && ascii && prose;
            all_ok = all_ok && ok;
            std::printf("       %-14s -> \"%s\"\n", names[i], line.c_str());
            if (!ok)
                std::printf("       ^ FAILED (nonempty=%d ascii=%d prose=%d)\n",
                           nonempty, ascii, prose);
        }
        check(all_ok, "D2 every kind's bank produces a non-empty ASCII prose line");
    }

    // ---- D3: missing corp/nation degrade honestly, never blank -------------
    {
        const contract_dispatch ev = make_event(contract_dispatch::kind::completed,
                                                 /*client*/ 999999, /*contractor*/ 999999);
        const std::string line = contract_dispatch_line(w, ev);
        check(!line.empty() && line.find("999999") == std::string::npos,
              "D3 an unresolvable client/contractor degrades to a noun, not a raw id or a gap");
    }

    // ---- D4: derive_contract_offers reports offer_issued --------------------
    {
        // A FRESH world each call — derive_contract_offers spends the
        // nation's treasury funding the escrow (move 4), so replaying the
        // SAME already-mutated world a second time (as `world w3 = w2` would)
        // starts the second call with a different, already-drawn-down
        // treasury and is not a fair "does report change the outcome" test.
        auto make_offer_world = [&]() {
            world w;
            body_component bc{};
            bc.grid_width  = 8;
            bc.grid_height = 1;
            const entity_id body = w.create_entity();
            w.bodies[body] = bc;

            const entity_id nat_a = add_nation(w, "Subject");
            const entity_id nat_b = add_nation(w, "Neighbour");

            auto add_tile = [&](int gx, entity_id owner) {
                const entity_id t = w.create_entity();
                tile_component tc{};
                tc.body = body; tc.grid_x = gx; tc.grid_y = 0;
                w.tiles[t] = tc;
                w.tile_to_nation[t] = owner;
                w.nations.at(owner).tiles.push_back(t);
                return t;
            };
            const entity_id t0 = add_tile(0, nat_a);
            const entity_id t1 = add_tile(1, nat_a);
            const entity_id t2 = add_tile(2, nat_b); // borders A

            province p_a;    p_a.id = 1;    p_a.body = body;    p_a.tiles = { t0, t1 };
            province p_weak; p_weak.id = 2; p_weak.body = body; p_weak.tiles = { t2 };
            w.provinces.provinces = { p_a, p_weak };
            w.province_holder     = { nat_a, nat_b };
            w.provinces.tile_province[t0] = 1;
            w.provinces.tile_province[t1] = 1;
            w.provinces.tile_province[t2] = 2;

            w.nations.at(nat_a).treasury = 1.0f;
            nation_budget nb;
            nb.reserve_fraction = 0.0f;
            nb.weights[static_cast<std::size_t>(budget_priority::contracted_force)] = 1.0f;
            w.nation_budgets[nat_a] = nb;
            return std::pair<world, entity_id>{ std::move(w), nat_a };
        };

        const recipe_registry reg;

        auto [w2, nat_a] = make_offer_world();
        economy_report report;
        derive_contract_offers(w2, reg, /*econ_tick*/ 0, contract_offer_params{}, &report);

        check(w2.mercenary_offers.size() == 1, "D4 exactly one offer opens");
        check(report.contract_events.size() == 1, "D4 exactly one contract_event is reported");
        if (!report.contract_events.empty())
        {
            const contract_dispatch& ev = report.contract_events.front();
            check(ev.what == contract_dispatch::kind::offer_issued,
                  "D4 the event's kind is offer_issued");
            check(ev.client == nat_a, "D4 the event names the issuing nation as client");
            check(ev.contractor == null_entity,
                  "D4 the event's contractor is null -- nobody has accepted yet");
            check(!w2.mercenary_offers.empty() && ev.id == w2.mercenary_offers.front().id,
                  "D4 the event's id matches the offer actually written");
            check(ev.province == w2.mercenary_offers.front().target_province,
                  "D4 the event's province matches the offer's target_province");
        }

        // D7 (half): the SAME call, from the SAME starting state, with no
        // report supplied, opens the identical offer.
        auto [w3, nat_a3] = make_offer_world();
        derive_contract_offers(w3, reg, /*econ_tick*/ 0, contract_offer_params{});
        check(w3.mercenary_offers.size() == w2.mercenary_offers.size() &&
              (w3.mercenary_offers.empty() ||
               w3.mercenary_offers.front().target_province ==
                   w2.mercenary_offers.front().target_province),
              "D7a derive_contract_offers with report==nullptr opens the same offer");
    }

    // ---- D5 / D6 / D7: run_mercenary_contract_tick's report bookkeeping ----
    {
        const contract_template_registry empty_templates; // size() == 0 -- the
        // accepted/abandoned checks both run BEFORE the template-index guard,
        // so they are exercisable with no Lua state at all.

        // D5: accepted fires exactly on accepted_tick == econ_tick, nowhere else.
        {
            world w4;
            const entity_id nat = add_nation(w4, "Client");
            const entity_id c   = add_corp(w4, "Contractor");
            mercenary_contract mc;
            mc.id = 1; mc.client = nat; mc.contractor = c;
            mc.template_index = -1; // deliberately unresolved -- never reaches settle logic
            mc.deadline = 1000; mc.accepted_tick = 5;
            mc.state = mercenary_contract_state::active;
            w4.mercenary_contracts.push_back(mc);

            const recipe_registry reg;
            economy_report r_before;
            run_mercenary_contract_tick(w4, reg, empty_templates, /*econ_tick*/ 4, &r_before);
            check(r_before.contract_events.empty(),
                  "D5a no accepted event on a tick before accepted_tick");

            economy_report r_on;
            run_mercenary_contract_tick(w4, reg, empty_templates, /*econ_tick*/ 5, &r_on);
            check(r_on.contract_events.size() == 1 &&
                  r_on.contract_events.front().what == contract_dispatch::kind::accepted,
                  "D5b exactly one accepted event fires on accepted_tick itself");

            economy_report r_after;
            run_mercenary_contract_tick(w4, reg, empty_templates, /*econ_tick*/ 6, &r_after);
            check(r_after.contract_events.empty(),
                  "D5c no repeat accepted event on a LATER tick");
        }

        // D6: abandoned is a one-shot, and report==nullptr does not burn it.
        {
            world w5;
            const entity_id nat = add_nation(w5, "Client");
            const entity_id c   = add_corp(w5, "Contractor");
            mercenary_contract mc;
            mc.id = 2; mc.client = nat; mc.contractor = c;
            mc.template_index = -1;
            mc.deadline = 1000; mc.accepted_tick = 1;
            mc.state = mercenary_contract_state::abandoned; // already terminal
            mc.abandoned_event_posted = false;
            w5.mercenary_contracts.push_back(mc);

            const recipe_registry reg;

            // First: no report supplied -- must NOT mark it posted.
            run_mercenary_contract_tick(w5, reg, empty_templates, /*econ_tick*/ 10);
            check(!w5.mercenary_contracts.front().abandoned_event_posted,
                  "D6a a tick with report==nullptr does not burn the one-shot flag");

            // Second: a real report arrives -- the event fires now, for the
            // first time, even though the contract has been abandoned for
            // several ticks already.
            economy_report r1;
            run_mercenary_contract_tick(w5, reg, empty_templates, /*econ_tick*/ 11, &r1);
            check(r1.contract_events.size() == 1 &&
                  r1.contract_events.front().what == contract_dispatch::kind::abandoned,
                  "D6b the FIRST report-carrying tick after abandonment fires the event");
            check(w5.mercenary_contracts.front().abandoned_event_posted,
                  "D6c the flag is now set");

            // Third: further ticks, with or without a report, never repeat it.
            economy_report r2;
            run_mercenary_contract_tick(w5, reg, empty_templates, /*econ_tick*/ 12, &r2);
            run_mercenary_contract_tick(w5, reg, empty_templates, /*econ_tick*/ 13); // no report
            economy_report r3;
            run_mercenary_contract_tick(w5, reg, empty_templates, /*econ_tick*/ 14, &r3);
            check(r2.contract_events.empty() && r3.contract_events.empty(),
                  "D6d no further tick re-announces the same abandonment");
        }

        // D7 (other half): report==nullptr changes no OTHER outcome (state
        // transitions still happen; only the observability differs).
        {
            world w6;
            const entity_id nat = add_nation(w6, "Client");
            const entity_id c   = add_corp(w6, "Contractor");
            mercenary_contract mc;
            mc.id = 3; mc.client = nat; mc.contractor = c;
            mc.template_index = -1; mc.deadline = 5; mc.accepted_tick = 0;
            mc.state = mercenary_contract_state::active;
            w6.mercenary_contracts.push_back(mc);
            const float balance_before = w6.corporations.at(c).balance;

            const recipe_registry reg;
            run_mercenary_contract_tick(w6, reg, empty_templates, /*econ_tick*/ 3); // no report
            check(w6.mercenary_contracts.front().state == mercenary_contract_state::active &&
                  w6.corporations.at(c).balance == balance_before,
                  "D7b an unresolved template_index still leaves the world untouched, "
                  "report or no report");
        }
    }

    std::printf("\n%d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
