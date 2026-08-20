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

// ---------------------------------------------------------------------------
// The import tariff (Sprint D4)
// ---------------------------------------------------------------------------
// Deliberately NOT folded into `evaluate_laws`. That function resolves the laws
// in force on one CORP; a tariff is a property of a JURISDICTION and a good, and
// the clearing tick needs it keyed by (nation, resource) at the moment a trade
// matches. Making evaluate_laws carry it would mean resolving every nation's
// rate table for every corp every tick to answer a question about neither.

bool any_import_tariff_enacted(const world& w)
{
    for (const law& l : w.laws)
        if (l.enacted && l.effect == law_effect_kind::import_tariff)
            return true;
    return false;
}

float nation_tariff_rate(const world& w, entity_id nation, resource_type resource)
{
    if (nation == null_entity)
        return 0.0f;

    // Authored law order — a fixed sequence, so the float accumulation below is
    // the same number every run (the determinism invariant; `w.laws` is a
    // vector, never reordered).
    float rate = 0.0f;
    for (const law& l : w.laws)
    {
        if (!l.enacted || l.effect != law_effect_kind::import_tariff)
            continue;
        if (l.author_nation != nation)
            continue;
        if (l.scope_resource != law::all_resources &&
            l.scope_resource != static_cast<int>(resource))
            continue;
        rate += l.rate;
    }
    // A stack of laws cannot charge more than the goods are worth, and a
    // negative authored rate is not a subsidy this seam knows how to pay.
    return (rate < 0.0f) ? 0.0f : ((rate > 1.0f) ? 1.0f : rate);
}
