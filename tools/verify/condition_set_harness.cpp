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
//
// The process exits non-zero if any assertion FAILs.

#include "world/condition_set.hpp"
#include "world/world.hpp"

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
    uc.strength = 250;
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

        condition s = make(condition_subject::military_strength, condition_comparator::at_least, 250.0f);
        check(measure_condition(s, w, f.corp) == 250.0f, "C6d military_strength sums unit strength");

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

    // --- rendering ----------------------------------------------------------
    std::printf("\n-- text rendering --\n");
    {
        condition c = make(condition_subject::stockpile, condition_comparator::at_least, 100.0f);
        c.resource  = resource_type::iron_ore;
        const std::string t = condition_text(c);
        check(t.find(">=") != std::string::npos && t.find("100") != std::string::npos,
              "T1 condition_text renders comparator and a whole operand without a decimal tail");
    }

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
