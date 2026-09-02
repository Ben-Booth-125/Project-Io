#include "law.hpp"

#include "world.hpp"

#include <algorithm> // BL-741: the sorted nation walk
#include <string>    // BL-741: per-nation law ids
#include <vector>

law_effects evaluate_laws(const world& w, entity_id subject_corp)
{
    law_effects fx;

    // Authored order, once per law. The predicate is BL-342's and reads nothing
    // but `w`, so this is pure — two identical worlds resolve identically.
    for (const law& l : w.laws)
    {
        if (!l.enacted)
            continue;
        // BL-480: a law with no author cannot exist; an ill-formed authorless
        // record is defensively inert (an authorless charge would destroy money,
        // which is the exact defect this item removed).
        if (l.enacting_nation == null_entity)
            continue;
        if (!evaluate(l.conditions, w, subject_corp))
            continue;

        switch (l.effect)
        {
            case law_effect_kind::extraction_levy:
            {
                // Find-or-append the author's schedule; first-appearance order
                // over the authored list keeps the bundle deterministic.
                law_effects::levy_schedule* sched = nullptr;
                for (law_effects::levy_schedule& s : fx.levies)
                    if (s.enacting_nation == l.enacting_nation)
                    {
                        sched = &s;
                        break;
                    }
                if (!sched)
                {
                    fx.levies.emplace_back();
                    sched = &fx.levies.back();
                    sched->enacting_nation = l.enacting_nation;
                }

                if (l.scope_resource == law::all_resources)
                {
                    for (std::size_t ri = 0; ri < resource_count; ++ri)
                        sched->extraction_levy[ri] += l.rate;
                }
                else if (l.scope_resource >= 0 &&
                         static_cast<std::size_t>(l.scope_resource) < resource_count)
                {
                    sched->extraction_levy[static_cast<std::size_t>(l.scope_resource)] += l.rate;
                }
                fx.any = true;
                break;
            }

            case law_effect_kind::import_tariff:
                // Jurisdiction-grain, not corp-grain: a tariff is charged in the
                // clearing tick against the market's nation (nation_tariff_rate),
                // so it contributes nothing to a per-corp effects bundle. Handled
                // explicitly rather than by a default, so adding a law family
                // forces a decision here instead of silently falling through.
                break;
        }
    }
    return fx;
}

entity_id choose_levy_author(const world& w)
{
    // 1. The player corp's home nation — the seeded law should bind the player,
    //    so the laws surface reads "whose law, what it costs me" non-vacuously.
    const auto pit = w.corporations.find(w.player_entity);
    if (pit != w.corporations.end() &&
        pit->second.home_nation != null_entity &&
        w.nations.find(pit->second.home_nation) != w.nations.end())
        return pit->second.home_nation;

    // 2. Largest territory, ties to the lowest entity id. The scan is over an
    //    unordered_map but the (tiles, id) ordering criterion is total, so the
    //    winner is independent of iteration order — deterministic.
    entity_id   best       = null_entity;
    std::size_t best_tiles = 0;
    for (const auto& [nid, nc] : w.nations)
    {
        if (best == null_entity ||
            nc.tiles.size() > best_tiles ||
            (nc.tiles.size() == best_tiles && nid < best))
        {
            best       = nid;
            best_tiles = nc.tiles.size();
        }
    }
    return best; // 3. null_entity when no nations exist.
}

void seed_prototype_laws(world& w, float rate)
{
    // BL-480: a law with no author cannot exist. No nations, no law.
    //
    // BL-741 (2026-09-01): EVERY nation authors its own levy, not just one.
    // Measured motivation: with a single seeded author, 1 of 43 nations held
    // any treasury (sum 108.3 cr over an 80-tick warm start), so every
    // state-side demand channel — network upkeep (BL-643), the space programme
    // (BL-644), the garrison line — was throttled by an empty wallet, funding
    // 138 of 3,357 derived purchase intents. The charge machinery was built
    // for this from the start: each schedule is jurisdiction-bounded and
    // transfers into its own author's treasury, so N laws is the same
    // arithmetic N times, and a corp extracting in nation A pays A, never B.
    // Sorted walk: the law list is serialised state and its order is semantic.
    std::vector<entity_id> nation_ids;
    nation_ids.reserve(w.nations.size());
    for (const auto& [nid, nc] : w.nations)
    {
        (void)nc;
        nation_ids.push_back(nid);
    }
    std::sort(nation_ids.begin(), nation_ids.end());

    for (const entity_id author : nation_ids)
    {
        law levy;
        levy.id      = "LAW-EXTRACTION-LEVY-" + std::to_string(author);
        levy.name    = "Extraction Levy";
        levy.effect  = law_effect_kind::extraction_levy;
        levy.rate    = rate;
        levy.scope_resource  = law::all_resources;
        levy.enacting_nation = author;
        // Empty condition_set: unconditional once enacted (BL-155's common case,
        // and the path that exercises BL-342's always-true degenerate branch).
        //
        // ENACTED at generation by its author nation (BL-480) — enactment is a
        // governing-body act, not a player checkbox. The charge is a transfer
        // into the author's treasury, bounded by its jurisdiction, so what ships
        // is every nation taxing extraction in its own territory rather than a
        // free-floating money sink a corporation could switch on.
        levy.enacted = true;
        w.laws.push_back(levy);
    }
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
        if (l.enacting_nation != nation)
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
