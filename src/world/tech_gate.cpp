#include "tech_gate.hpp"

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
    garrison.id                = "E0-ML-01";
    garrison.unlocks_structure = building_type::military_base;
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

int advance_tech_gates(world& w)
{
    int earned = 0;
    // w.corporations is an unordered_map, but the OUTCOME here does not depend
    // on visit order: each corp's gates are evaluated against a world this pass
    // never mutates in a way another corp's predicate can read (`earned_techs`
    // is the only write, and no gate has a `research` condition today). The
    // gate table order is fixed. Should a gate ever depend on another corp's
    // earned set, this loop must be re-keyed to ascending corp id first.
    for (const auto& [corp, cc] : w.corporations)
    {
        for (const tech_gate& g : prototype_tech_gates())
        {
            if (w.has_tech(corp, g.id))
                continue; // monotonic: never re-earned, never un-earned
            if (!evaluate(g.condition, w, corp))
                continue;
            w.earned_techs[corp].insert(g.id);
            ++earned;
        }
    }
    return earned;
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
