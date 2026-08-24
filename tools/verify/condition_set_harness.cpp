// ---------------------------------------------------------------------------
// Headless condition_set harness (BL-342; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Exercises the shared predicate that laws (BL-343) and techs (BL-344) both
// read — src/world/condition_set.{hpp,cpp}. This is the foundation both minors
// stand on, so its properties are asserted here rather than inside either
// consumer's harness, where a regression would present as someone else's bug.
//
//   C1  An EMPTY set is true. BL-155's common case ("most laws are
//       unconditional once enacted") must be the cheap path, not a special
//       case, and it must not depend on the corp being resolvable.
//   C2  Every subject resolves against a hand-built world, and reads the
//       quantity it claims to read (not a neighbouring one).
//   C3  Comparator boundaries are EXACT on integral subjects — `exactly 3`
//       means three, and >= at the threshold holds. No epsilon fuzz.
//   C4  An AND-list is order-independent: the same conditions in any order
//       give the same verdict, and one false condition sinks the set.
//   C5  Evaluation is deterministic — the same world evaluates identically
//       twice, and two structurally-identical worlds agree.
//   C6  A MILITARY subject works. BL-094's design test at the foundation: the
//       type is not shaped so that only economic quantities can be asked about.
//   C7  An unknown corporation measures zero everywhere and never throws.
//   C8  BL-570: `province_held` reads `world::province_holder` correctly for a
//       holder, a non-holder and a sea province, plus the unset/absent-id cases
//       an authored contract template leaves.
//   C9  BL-570: `scripts/contracts.lua`'s `contract_templates` table loads
//       through the protected sol2 path (a real `lua_state`, not a hand-built
//       fixture) and its id -> index reference round-trips.
//
// The process exits non-zero if any assertion FAILs.

#include "world/condition_set.hpp"
#include "world/contract_template.hpp"
#include "world/province.hpp"
#include "world/world.hpp"

#include "scripting/lua_state.hpp"

#include <cstdio>
#include <string>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

/// A minimal world: one body, one corp, three buildings (2 extraction sites and
/// 1 launchpad), a stockpile pool, one market, and one unit group.
struct fixture
{
    world     w;
    entity_id body    = null_entity;
    entity_id corp    = null_entity;
    entity_id market  = null_entity;
};

entity_id add_building(world& w, entity_id corp, entity_id body, building_type type)
{
    const entity_id tile = w.create_entity();
    tile_component  tc{};
    tc.body = body;
    w.tiles[tile] = tc;

    const entity_id b = w.create_entity();
    building_component bc{};
    bc.type = type;
    bc.tile = tile;
    w.buildings[b] = bc;
    w.corporations[corp].assets.push_back(b);
    return b;
}

fixture make_fixture()
{
    fixture f;
    f.body = f.w.create_entity();
    f.w.bodies[f.body] = body_component{};

    f.corp = f.w.create_entity();
    corporation_component cc{};
    cc.name      = "Test Corp";
    cc.balance   = 5000.0f;
    cc.is_player = true;
    f.w.corporations[f.corp] = cc;
    f.w.player_entity        = f.corp;

    add_building(f.w, f.corp, f.body, building_type::extraction_site);
    add_building(f.w, f.corp, f.body, building_type::extraction_site);
    add_building(f.w, f.corp, f.body, building_type::processing_facility);

    f.w.pool_for(f.corp, f.body).quantities[static_cast<std::size_t>(resource_type::iron_ore)] = 120.0f;

    f.market = f.w.create_entity();
    market_component mc{};
    mc.body = f.body;
    mc.price[static_cast<std::size_t>(resource_type::iron_ore)] = 40.0f;
    f.w.markets[f.market] = mc;

    const entity_id u = f.w.create_entity();
    unit_component uc{};
    uc.owner    = f.corp;
    uc.position = null_entity;
    uc.count    = 4;
    // BL-459: `strength` is no longer a stored field — it is derived from count,
    // the roster row's quality and the supply factor. Roster row 15 is Rifle
    // Regiment (power_mod 380 -> quality 1380), chosen so the measured strength
    // (4 * 1380 * 1000 / 10000 = 552) is distinct from BOTH the headcount and
    // from count x 100, which is what C6d needs in order to prove it is reading
    // strength rather than a neighbouring quantity.
    uc.type     = 15;
    uc.supply_factor_permille = 1000;
    f.w.units[u] = uc;

    return f;
}

condition make(condition_subject s, condition_comparator cmp, float operand)
{
    condition c;
    c.subject    = s;
    c.comparator = cmp;
    c.operand    = operand;
    return c;
}

} // namespace

int main()
{
    std::printf("=== condition_set harness (BL-342) ===\n\n");

    fixture f = make_fixture();
    const world& w = f.w;

    // --- C1: the empty set --------------------------------------------------
    std::printf("-- C1  empty set --\n");
    const condition_set empty;
    check(empty.always(), "C1a an empty set reports always()");
    check(evaluate(empty, w, f.corp), "C1b an empty set is TRUE for a real corp");
    check(evaluate(empty, w, 99999), "C1c an empty set is TRUE even for an unknown corp");

    // --- C2: every subject resolves ----------------------------------------
    std::printf("\n-- C2  subjects resolve --\n");
    {
        condition c = make(condition_subject::structure, condition_comparator::at_least, 0.0f);
        c.structure = building_type::extraction_site;
        check(measure_condition(c, w, f.corp) == 2.0f, "C2a structure counts the 2 extraction sites");
        c.structure = building_type::processing_facility;
        check(measure_condition(c, w, f.corp) == 1.0f, "C2b structure counts the 1 processor");
        c.structure = building_type::port;
        check(measure_condition(c, w, f.corp) == 0.0f, "C2c structure counts 0 of an absent type");
    }
    {
        condition c = make(condition_subject::stockpile, condition_comparator::at_least, 0.0f);
        c.resource = resource_type::iron_ore;
        check(measure_condition(c, w, f.corp) == 120.0f, "C2d stockpile reads the pooled iron ore");
        c.resource = resource_type::copper_ore;
        check(measure_condition(c, w, f.corp) == 0.0f, "C2e stockpile reads 0 for an unstocked good");
    }
    {
        condition c = make(condition_subject::market, condition_comparator::at_least, 0.0f);
        c.resource = resource_type::iron_ore;
        check(measure_condition(c, w, f.corp) == 40.0f, "C2f market reads the resolved price");
    }
    check(measure_condition(make(condition_subject::surplus, condition_comparator::at_least, 0.0f),
                            w, f.corp) == 5000.0f,
          "C2g surplus reads the corp balance");
    check(measure_condition(make(condition_subject::era, condition_comparator::at_least, 0.0f),
                            w, f.corp) == 0.0f,
          "C2h era reads 0 with no launchpad");
    {
        condition c = make(condition_subject::research, condition_comparator::at_least, 1.0f);
        c.key = "E1-PL-09";
        check(measure_condition(c, w, f.corp) == 0.0f, "C2i research reads 0 for an unearned tech");
        fixture g = make_fixture();
        g.w.earned_techs[g.corp].insert("E1-PL-09");
        check(measure_condition(c, g.w, g.corp) == 1.0f, "C2j research reads 1 once earned");
        check(measure_condition(c, g.w, 99999) == 0.0f, "C2k research is PER-CORP, not global");
    }
    {
        // Era rises when the corp owns a launchpad — the honest measure, matching
        // the only space gate the code actually has today.
        fixture g = make_fixture();
        add_building(g.w, g.corp, g.body, building_type::launchpad);
        check(measure_condition(make(condition_subject::era, condition_comparator::at_least, 0.0f),
                                g.w, g.corp) == 1.0f,
              "C2l era reads 1 once a launchpad is owned");
    }

    // --- C3: exact comparator boundaries on integral subjects ---------------
    std::printf("\n-- C3  comparator boundaries --\n");
    {
        condition c = make(condition_subject::structure, condition_comparator::at_least, 2.0f);
        c.structure = building_type::extraction_site; // measures exactly 2
        check(evaluate_condition(c, w, f.corp), "C3a >= 2 holds at exactly 2");
        c.comparator = condition_comparator::greater_than;
        check(!evaluate_condition(c, w, f.corp), "C3b > 2 fails at exactly 2");
        c.comparator = condition_comparator::exactly;
        check(evaluate_condition(c, w, f.corp), "C3c == 2 holds at exactly 2");
        c.operand = 3.0f;
        check(!evaluate_condition(c, w, f.corp), "C3d == 3 fails at 2");
        c.comparator = condition_comparator::less_than;
        check(evaluate_condition(c, w, f.corp), "C3e < 3 holds at 2");
        c.comparator = condition_comparator::at_most;
        c.operand    = 2.0f;
        check(evaluate_condition(c, w, f.corp), "C3f <= 2 holds at exactly 2");
        check(condition_subject_is_integral(condition_subject::structure),
              "C3g structure is declared integral (so == is exact, not epsilon-fuzzy)");
        check(!condition_subject_is_integral(condition_subject::stockpile),
              "C3h stockpile is declared continuous (3.5 units is real state)");
    }
    {
        // A continuous subject compares as a float at its exact boundary.
        condition c = make(condition_subject::surplus, condition_comparator::at_least, 5000.0f);
        check(evaluate_condition(c, w, f.corp), "C3i surplus >= 5000 holds at exactly 5000");
        c.operand = 5000.5f;
        check(!evaluate_condition(c, w, f.corp), "C3j surplus >= 5000.5 fails at 5000");
    }

    // --- C4: AND-list semantics --------------------------------------------
    std::printf("\n-- C4  AND-list --\n");
    {
        condition a = make(condition_subject::surplus, condition_comparator::at_least, 1000.0f); // true
        condition b = make(condition_subject::structure, condition_comparator::at_least, 2.0f);  // true
        b.structure = building_type::extraction_site;
        condition bad = make(condition_subject::surplus, condition_comparator::at_least, 1e9f);  // false

        condition_set both;   both.all = {a, b};
        condition_set rev;    rev.all  = {b, a};
        condition_set with_bad;      with_bad.all = {a, b, bad};
        condition_set bad_first;     bad_first.all = {bad, a, b};

        check(evaluate(both, w, f.corp), "C4a all-true AND-list is true");
        check(evaluate(rev, w, f.corp) == evaluate(both, w, f.corp),
              "C4b reversing the list does not change the verdict");
        check(!evaluate(with_bad, w, f.corp), "C4c one false condition sinks the set");
        check(evaluate(bad_first, w, f.corp) == evaluate(with_bad, w, f.corp),
              "C4d the false condition sinks it wherever it sits in the list");
    }

    // --- C5: determinism ----------------------------------------------------
    std::printf("\n-- C5  determinism --\n");
    {
        condition_set cs;
        condition m = make(condition_subject::market, condition_comparator::at_least, 10.0f);
        m.resource  = resource_type::iron_ore;
        condition s = make(condition_subject::stockpile, condition_comparator::at_least, 100.0f);
        s.resource  = resource_type::iron_ore;
        cs.all = {m, s};

        const bool first = evaluate(cs, w, f.corp);
        check(first == evaluate(cs, w, f.corp), "C5a the same world evaluates identically twice");

        // A second market, so the `market` subject's mean is summed over more than
        // one unordered-map entry — the one place a float sum could reorder.
        fixture g = make_fixture();
        const entity_id m2 = g.w.create_entity();
        market_component mc2{};
        mc2.body = g.body;
        mc2.price[static_cast<std::size_t>(resource_type::iron_ore)] = 60.0f;
        g.w.markets[m2] = mc2;

        fixture h = make_fixture();
        const entity_id m3 = h.w.create_entity();
        market_component mc3{};
        mc3.body = h.body;
        mc3.price[static_cast<std::size_t>(resource_type::iron_ore)] = 60.0f;
        h.w.markets[m3] = mc3;

        const float gm = measure_condition(m, g.w, g.corp);
        const float hm = measure_condition(m, h.w, h.corp);
        check(gm == hm, "C5b two structurally-identical worlds measure bit-identically");
        check(gm == 50.0f, "C5c the multi-market mean is the mean (40 and 60 -> 50)");
    }

    // --- C6: a military subject --------------------------------------------
    std::printf("\n-- C6  military subject (BL-094's test at the foundation) --\n");
    {
        condition u = make(condition_subject::military_units, condition_comparator::at_least, 4.0f);
        check(measure_condition(u, w, f.corp) == 4.0f, "C6a military_units counts the unit group");
        check(evaluate_condition(u, w, f.corp), "C6b units >= 4 holds at exactly 4");
        u.operand = 5.0f;
        check(!evaluate_condition(u, w, f.corp), "C6c units >= 5 fails at 4");

        // BL-459: the derived strength of the fixture's one Rifle Regiment —
        // count 4 x quality 1.380 x supply 1.000 x 100 = 552.
        condition s = make(condition_subject::military_strength, condition_comparator::at_least, 552.0f);
        check(measure_condition(s, w, f.corp) == 552.0f, "C6d military_strength sums derived unit strength");
        check(evaluate_condition(s, w, f.corp), "C6d2 strength >= 552 holds at exactly 552");

        // A mixed economic + military AND-list — the shape a governing-body law
        // or a military tech gate actually needs.
        condition_set mixed;
        condition bal = make(condition_subject::surplus, condition_comparator::at_least, 1000.0f);
        mixed.all = {bal, s};
        check(evaluate(mixed, w, f.corp), "C6e an economic AND military predicate resolves");
    }

    // --- C7: unknown corp ---------------------------------------------------
    std::printf("\n-- C7  unknown corporation --\n");
    {
        const entity_id ghost = 987654;
        check(measure_condition(make(condition_subject::surplus, condition_comparator::at_least, 0.0f),
                                w, ghost) == 0.0f,
              "C7a an unknown corp measures 0 rather than throwing");
        condition c = make(condition_subject::structure, condition_comparator::at_least, 1.0f);
        c.structure = building_type::extraction_site;
        check(!evaluate_condition(c, w, ghost), "C7b a non-empty predicate fails for an unknown corp");
    }

    // --- C8: province_held (BL-570) ------------------------------------------
    std::printf("\n-- C8  province_held (BL-570) --\n");
    {
        // A hand-built partition of three provinces, positionally aligned with
        // world::province_holder exactly as province.hpp specifies -- ids need
        // not be contiguous or match position, only ascending, which is
        // province_partition::find's (binary search) actual contract.
        fixture g = make_fixture();
        const entity_id rival = g.w.create_entity();
        g.w.corporations[rival] = corporation_component{};

        province held_by_corp;   held_by_corp.id   = 10; held_by_corp.body = g.body; held_by_corp.tiles = { 100 };
        province held_by_rival;  held_by_rival.id  = 20; held_by_rival.body = g.body; held_by_rival.tiles = { 200 };
        // The sea case: a REAL open-ocean tile, so province_kind_of genuinely
        // reads open_ocean here -- not just an entry that happens to be
        // null_entity for some other reason. seed_province_holders (BL-569,
        // its own harness) is what proves the SEEDING skips this domain; this
        // fixture only needs to prove measure_condition reads whatever
        // province_holder actually stores, sea included.
        province sea; sea.id = 30; sea.body = g.body; sea.tiles = { 300 };
        tile_component sea_tile{}; sea_tile.body = g.body; sea_tile.substrate = terrain_substrate::ocean;
        g.w.tiles[300] = sea_tile;

        g.w.provinces.provinces = { held_by_corp, held_by_rival, sea }; // ascending id, per the partition's contract
        g.w.province_holder     = { g.corp, rival, null_entity };      // positionally aligned with the line above

        check(province_kind_of(g.w, sea) == province_kind::open_ocean,
              "C8a the fixture's sea province really is open_ocean (not a stand-in null)");

        condition c = make(condition_subject::province_held, condition_comparator::at_least, 1.0f);
        c.province = held_by_corp.id;
        check(measure_condition(c, g.w, g.corp) == 1.0f, "C8b province_held reads 1 for the subject corp's own province");
        check(evaluate_condition(c, g.w, g.corp), "C8c >= 1 holds for the corp's own province");

        c.province = held_by_rival.id;
        check(measure_condition(c, g.w, g.corp) == 0.0f, "C8d province_held reads 0 for a province the RIVAL holds");
        check(!evaluate_condition(c, g.w, g.corp), "C8e >= 1 fails for a rival-held province");
        check(measure_condition(c, g.w, rival) == 1.0f, "C8f the SAME province reads 1 for the corp that actually holds it");

        c.province = sea.id;
        check(measure_condition(c, g.w, g.corp) == 0.0f, "C8g province_held reads 0 for a sea province (no holder is ever recorded)");
        check(measure_condition(c, g.w, rival) == 0.0f, "C8h ...for every corp, not just the subject of C8g");

        // The unset default and an id no province in the partition carries --
        // "an unset field measures 0, never guesses" (the same honesty rule
        // C2i/C7a exercise for research/unknown-corp).
        condition unset = make(condition_subject::province_held, condition_comparator::at_least, 1.0f);
        check(unset.province == no_province, "C8i condition::province defaults to no_province, not 0");
        check(measure_condition(unset, g.w, g.corp) == 0.0f, "C8j an unset province measures 0");
        c.province = 999999u; // in range, but no province in this partition carries it
        check(measure_condition(c, g.w, g.corp) == 0.0f, "C8k a province absent from the partition measures 0, not a crash");

        check(condition_subject_is_integral(condition_subject::province_held),
              "C8l province_held is declared integral (held or not -- never a fractional 0.5)");
    }

    // --- C9: contract-template table load + round-trip (BL-570) -------------
    std::printf("\n-- C9  contract_templates (scripts/contracts.lua) --\n");
    {
        lua_state lua;
        lua.load("scripts/contracts.lua"); // throws (protected sol2 path) on any parse/runtime error

        contract_template_registry reg;
        reg.load_from_lua(lua); // throws, naming the row/field, on anything malformed

        check(reg.size() == 2, "C9a contract_templates loads exactly the two authored rows");

        if (reg.size() == 2)
        {
            const contract_template& take = reg.at(0);
            const contract_template& hold = reg.at(1);

            check(take.id == "take" && hold.id == "hold", "C9b the two rows load in authored order (take, hold)");
            check(!take.continuous, "C9c 'take' is point-in-time (continuous = false)");
            check(hold.continuous, "C9d 'hold' is continuous (must hold every tick to the deadline)");
            check(take.predicate.subject == condition_subject::province_held
                      && hold.predicate.subject == condition_subject::province_held,
                  "C9e both rows' predicate.subject parses as province_held");
            check(take.predicate.comparator == condition_comparator::at_least
                      && hold.predicate.comparator == condition_comparator::at_least,
                  "C9f both rows' predicate.comparator parses as at_least");
            check(take.predicate.operand == 1.0f && hold.predicate.operand == 1.0f,
                  "C9g both rows' predicate.operand parses as 1");
            check(take.predicate.province == no_province && hold.predicate.province == no_province,
                  "C9h neither row authors a province -- it is bound per accepted offer, not in the template");
            check(take.deadline_ticks > 0 && hold.deadline_ticks > 0,
                  "C9i both rows carry a positive deadline_ticks");

            // C9j: the round trip a saved contract's template reference makes --
            // id -> index -> the SAME row, and an id that was never authored
            // resolves to "not found" rather than to some other row.
            check(reg.index_of("take") == 0 && reg.index_of("hold") == 1,
                  "C9j index_of round-trips each row's own id to its own position");
            check(&reg.at(static_cast<std::size_t>(reg.index_of("hold"))) == &hold,
                  "C9k the resolved index names the identical row, not a coincidentally-equal one");
            check(reg.index_of("siege") == -1, "C9l an unauthored id resolves to -1, not a guess");
        }
    }

    // --- rendering ----------------------------------------------------------
    std::printf("\n-- text rendering --\n");
    {
        condition c = make(condition_subject::stockpile, condition_comparator::at_least, 100.0f);
        c.resource  = resource_type::iron_ore;
        const std::string t = condition_text(c);
        check(t.find(">=") != std::string::npos && t.find("100") != std::string::npos,
              "T1 condition_text renders comparator and a whole operand without a decimal tail");
    }
    {
        // BL-570: province_held renders its own phrase, not the generic
        // "<subject> <cmp> <operand>" shape -- no comparator noise, and an id
        // since a province carries no name (province.hpp).
        condition c = make(condition_subject::province_held, condition_comparator::at_least, 1.0f);
        c.province = 42;
        const std::string t = condition_text(c);
        check(t.find("holds") != std::string::npos && t.find("42") != std::string::npos,
              "T2 condition_text renders province_held as 'holds province #<id>'");
        check(t.find(">=") == std::string::npos,
              "T2b ...with no '>=' noise (the predicate is always >= 1, so restating it says nothing)");
    }

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
