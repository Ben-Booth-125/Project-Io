#include "tech_gate.hpp"

#include "recipe_registry.hpp" // BL-588: recipe_unlocked's id -> name resolution
#include "world.hpp"

#include <vector>

namespace {

/// Build the authored table once. A function-local static rather than a global,
/// so initialisation order is defined and nothing runs before main().
std::vector<tech_gate> build_gates()
{
    std::vector<tech_gate> gates;

    // E0-ML-01 "Standing Garrison Doctrine" — the prototype's ONE live gate.
    //
    // The predicate has to be satisfiable through ordinary play, or the gate is
    // decorative. Two conditions, deliberately of different kinds, so the
    // AND-list is doing real work rather than wrapping a single number:
    //
    //   * two extraction sites — an industrial base exists at all. Every corp
    //     generates with assets, so this is normally already true; it is the
    //     condition that stops a corp with nothing standing up an army.
    //   * a Cr 2,000 balance — the doctrine costs money to keep, and money is
    //     the quantity the player already watches every quarter.
    //
    // A military SUBJECT is deliberately NOT used here: gating the first
    // military building on already having a military would be circular. What
    // BL-094's test asks of this item is that the UNLOCK reaches military
    // outcomes, and it does.
    tech_gate garrison;
    garrison.id = "E0-ML-01";
    // Authored through the union (BL-479): add_effect keeps the
    // `unlocks_structure` mirror true, so every pre-union reader is unchanged.
    garrison.add_effect(tech_effect::unlock(building_type::military_base));
    {
        condition c;
        c.subject    = condition_subject::structure;
        c.structure  = building_type::extraction_site;
        c.comparator = condition_comparator::at_least;
        c.operand    = 2.0f;
        garrison.condition.all.push_back(c);
    }
    {
        condition c;
        c.subject    = condition_subject::surplus;
        c.comparator = condition_comparator::at_least;
        c.operand    = 2000.0f;
        garrison.condition.all.push_back(c);
    }
    gates.push_back(garrison);

    // BL-588 — the effect union's first two `unlock_recipe` gates. Not a
    // reshaping of the tech tree (Ben's constraint on this item): both ids
    // below are authored fresh rather than transcribed from tech_tree.lua's
    // ~150 sketch/derived nodes, since none of them resolve to a real
    // predicate yet (NR-579 records this as the open question the next
    // authoring pass owns). "First cut" per the item's own design — proving
    // the arm resolves end to end, not gating the whole roster in one item.
    //
    // Neither predicate uses `stockpile` as a proxy for "produced" — BL-428's
    // OWN mechanism (`corp_reached_depth`, produced_ever) already answers
    // "has this corp ever made X", and re-deriving that from a stockpile
    // snapshot would drift the moment a corp sold or consumed the good. A
    // tech gate is a DIFFERENT, ADDITIONAL lock layered on top of the depth
    // one (both must pass), so its predicate asks a genuinely different
    // question — held infrastructure and cash — rather than shadowing depth.

    // E0-EC-01 "Tool-and-Die Practice" — unlocks the Toolmaker (BL-586), the
    // ancient roster's chain that needs both a smelted good and a milled one
    // at once. Predicate: a Bloomery already stands (the smelting half is
    // real, not hypothetical) and a Cr 500 surplus (a smaller doctrine cost
    // than the garrison's 2,000 — this is a production method, not a standing
    // military commitment).
    tech_gate tool_and_die;
    tool_and_die.id = "E0-EC-01";
    tool_and_die.add_effect(tech_effect::unlock(std::string("toolmaker")));
    {
        condition c;
        c.subject    = condition_subject::structure;
        c.structure  = building_type::processing_facility;
        c.comparator = condition_comparator::at_least;
        c.operand    = 1.0f;
        tool_and_die.condition.all.push_back(c);
    }
    {
        condition c;
        c.subject    = condition_subject::surplus;
        c.comparator = condition_comparator::at_least;
        c.operand    = 500.0f;
        tool_and_die.condition.all.push_back(c);
    }
    gates.push_back(tool_and_die);

    // E1-EC-01 "Converter Practice" — unlocks the Bessemer Converter (BL-587),
    // industrial steel's alternate method. Predicate: the corp already HOLDS
    // machinery — the Converter's own reagent — rather than a cash figure. A
    // surplus-only predicate was authored first and rejected on measurement:
    // `tech_gate_harness`'s T3 fixture (2 extraction sites, Cr 5,000, zero
    // stockpile) satisfied ANY plausible cash bar by accident, which is the
    // same "satisfied by an unrelated corp" failure a gate exists to avoid —
    // not just a test collision, a real design flaw the test caught. Holding
    // machinery means the machine-tool chain (Fabricator: steel + refined
    // copper) is ALREADY running, which is the genuinely industrial-arc fact
    // "converter practice" should require. `structure` is deliberately NOT
    // used: every processing_facility can run the Smelter already, so
    // requiring one would be circular the same way a military subject on the
    // garrison gate would have been.
    tech_gate converter_practice;
    converter_practice.id = "E1-EC-01";
    converter_practice.add_effect(tech_effect::unlock(std::string("steel_bessemer")));
    {
        condition c;
        c.subject    = condition_subject::stockpile;
        c.resource   = resource_type::machinery;
        c.comparator = condition_comparator::at_least;
        c.operand    = 1.0f;
        converter_practice.condition.all.push_back(c);
    }
    gates.push_back(converter_practice);

    // E0-EC-03 "Copper Smelting Practice" — unlocks refined_copper (BL-589, the
    // start-gate audit). Measured: refined_copper is `era = "any"`, required
    // depth 0 (copper_ore is a raw), so it was the roster's widest anachronism —
    // an ancient campaign could smelt refined copper for free on tick one, with
    // no ancient identity to it at all. Ben's ruling (2026-08-24, the start-gate
    // form): the any-band depth exemption itself stays as-is (Metal Foundry's
    // other any-band members are not touched), but THIS one recipe earns a tech
    // gate specifically, the same shape as E0-EC-01/E1-EC-01. Predicate: a
    // processing facility already stands (any of the five open ancient/any
    // recipes gets a corp there — non-circular) and a Cr 400 surplus, between
    // Tool-and-Die's 500 and Converter Practice's stockpile-only bar.
    tech_gate copper_smelting;
    copper_smelting.id = "E0-EC-03";
    copper_smelting.add_effect(tech_effect::unlock(std::string("refined_copper")));
    {
        condition c;
        c.subject    = condition_subject::structure;
        c.structure  = building_type::processing_facility;
        c.comparator = condition_comparator::at_least;
        c.operand    = 1.0f;
        copper_smelting.condition.all.push_back(c);
    }
    {
        condition c;
        c.subject    = condition_subject::surplus;
        c.comparator = condition_comparator::at_least;
        c.operand    = 400.0f;
        copper_smelting.condition.all.push_back(c);
    }
    gates.push_back(copper_smelting);

    return gates;
}

} // namespace

const std::vector<tech_gate>& prototype_tech_gates()
{
    static const std::vector<tech_gate> gates = build_gates();
    return gates;
}

const tech_gate* find_tech_gate(const std::string& tech_id)
{
    for (const tech_gate& g : prototype_tech_gates())
        if (g.id == tech_id)
            return &g;
    return nullptr;
}

int advance_tech_gates(world& w, const std::vector<tech_gate>& gates)
{
    int earned = 0;
    // w.corporations is an unordered_map, but the OUTCOME here does not depend
    // on visit order: each corp's gates are evaluated against a world this pass
    // never mutates in a way another corp's predicate can read (the writes are
    // `earned_techs` and `corp_modifiers`, both keyed to the corp being
    // visited; a `research` condition reads only the SUBJECT corp's own earned
    // set). The gate table order is fixed, so each corp's modifier append order
    // is fixed too. Should a gate ever depend on ANOTHER corp's earned set,
    // this loop must be re-keyed to ascending corp id first.
    for (const auto& [corp, cc] : w.corporations)
    {
        for (const tech_gate& g : gates)
        {
            if (w.has_tech(corp, g.id))
                continue; // monotonic: never re-earned, never un-earned
            if (!evaluate(g.condition, w, corp))
                continue;
            w.earned_techs[corp].insert(g.id);
            // BL-479: the earn moment is when modify_scalar effects land — for
            // the EARNING corp only (research is not a world fact), in stored
            // effect order, exactly once (guarded by the monotonic earn above).
            for (const tech_effect& e : g.effects)
                if (e.kind == tech_effect_kind::modify_scalar)
                    w.corp_modifiers[corp].push_back(e.modifier);
            ++earned;
        }
    }
    return earned;
}

int advance_tech_gates(world& w)
{
    return advance_tech_gates(w, prototype_tech_gates());
}

bool structure_unlocked(const world& w, entity_id corp, building_type type)
{
    // A caller that does not know which corp is asking gets the pre-BL-344
    // behaviour rather than a guess. Placement checks that only describe a TILE
    // (can_place) legitimately have no corp; the ones that gate an ACTION do.
    if (corp == null_entity)
        return true;

    for (const tech_gate& g : prototype_tech_gates())
        if (g.unlocks_structure == type)
            return w.has_tech(corp, g.id);
    return true; // ungated
}

std::string gating_tech_for(building_type type)
{
    for (const tech_gate& g : prototype_tech_gates())
        if (g.unlocks_structure == type)
            return g.id;
    return {};
}

bool recipe_unlocked(const world& w, const recipe_registry& reg,
                     entity_id corp, uint16_t recipe_id)
{
    // Same null_entity/ungated contract as structure_unlocked.
    if (corp == null_entity)
        return true;

    // Gates store the recipe NAME (tech_effect::recipe's comment: an id is
    // positional and would silently repoint). A stale/out-of-range id has no
    // name to resolve, which reads as ungated rather than throwing — the same
    // "never crash on missing data" contract try_switch_recipe already keeps.
    const recipe* rc = reg.get_recipe(recipe_id);
    if (rc == nullptr)
        return true;

    for (const tech_gate& g : prototype_tech_gates())
        if (!g.unlocks_recipe.empty() && g.unlocks_recipe == rc->name)
            return w.has_tech(corp, g.id);
    return true; // ungated
}

std::string gating_tech_for_recipe(const recipe_registry& reg, uint16_t recipe_id)
{
    const recipe* rc = reg.get_recipe(recipe_id);
    if (rc == nullptr)
        return {};

    for (const tech_gate& g : prototype_tech_gates())
        if (!g.unlocks_recipe.empty() && g.unlocks_recipe == rc->name)
            return g.id;
    return {};
}
