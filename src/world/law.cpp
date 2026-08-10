#include "law.hpp"

#include "world.hpp"

law_effects evaluate_laws(const world& w, entity_id subject_corp)
{
    law_effects fx;

    // Authored order, once per law. The predicate is BL-342's and reads nothing
    // but `w`, so this is pure — two identical worlds resolve identically.
    for (const law& l : w.laws)
    {
        if (!l.enacted)
            continue;
        if (!evaluate(l.conditions, w, subject_corp))
            continue;

        switch (l.effect)
        {
            case law_effect_kind::extraction_levy:
            {
                if (l.scope_resource == law::all_resources)
                {
                    for (std::size_t ri = 0; ri < resource_count; ++ri)
                        fx.extraction_levy[ri] += l.rate;
                }
                else if (l.scope_resource >= 0 &&
                         static_cast<std::size_t>(l.scope_resource) < resource_count)
                {
                    fx.extraction_levy[static_cast<std::size_t>(l.scope_resource)] += l.rate;
                }
                fx.any = true;
                break;
            }
        }
    }
    return fx;
}

void seed_prototype_laws(world& w, float rate)
{
    law levy;
    levy.id      = "LAW-EXTRACTION-LEVY";
    levy.name    = "Extraction Levy";
    levy.effect  = law_effect_kind::extraction_levy;
    levy.rate    = rate;
    levy.scope_resource = law::all_resources;
    // Empty condition_set: unconditional once enacted (BL-155's common case,
    // and the path that exercises BL-342's always-true degenerate branch).
    levy.enacted = false; // shipped OFF — see the header for why.
    w.laws.push_back(levy);
}
