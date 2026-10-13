#include "city_names.hpp"

#include "settlement.hpp"

#include <algorithm>

namespace {

/// A city root: one or two syllables of the tongue. Shorter than a nation
/// name on purpose — a settlement wears a name people say every day.
std::string city_root(mt_picker& pick, const tongue& t)
{
    return tongue_word(pick, t, 1 + pick.pick(2));
}

} // namespace

std::string generate_city_name(std::mt19937& rng, const tongue& t)
{
    if (!t.usable())
        return {};

    mt_picker pick(rng);
    const tongue_lexicon lex = coin_lexicon(t);

    // Root + the culture's own settlement morpheme, suffixed. The morpheme is
    // coined from the same inventory, so it reads as part of the word rather
    // than as a borrowed "-ton".
    std::string name = city_root(pick, t);
    if (!lex.settlement.empty())
        name += lex.settlement[static_cast<std::size_t>(
            pick.pick(static_cast<int>(lex.settlement.size())))];

    // ~1 in 6: a standing epithet in front, the tongue's own equivalent of a
    // "North"/"New" prefix.
    if (!lex.qualifier.empty() && pick.pick(6) == 0)
        name = lex.qualifier[static_cast<std::size_t>(
                   pick.pick(static_cast<int>(lex.qualifier.size())))] + " " + name;

    return name;
}

std::string generate_unique_city_name(std::mt19937& rng, const tongue& t,
                                      std::unordered_set<std::string>& used)
{
    constexpr int k_redraws = 32;
    std::string name;
    for (int i = 0; i < k_redraws; ++i)
    {
        name = generate_city_name(rng, t);
        if (name.empty())
            return name;
        if (used.insert(name).second)
            return name;
    }
    // The tongue's stock is spent: lengthen the last draw with further roots of the
    // same tongue. Each pass adds a syllable or two, so this ends.
    mt_picker pick(rng);
    while (!used.insert(name).second)
        name += tongue_lower(city_root(pick, t));
    return name;
}

void name_population_centres(world& w, entity_id body_id, int gw,
                             const settlement_state& ss, const creed_state& cs,
                             uint32_t seed)
{
    if (ss.regions.empty() || cs.cultures.empty() || gw <= 0)
        return;

    // Sorted-id order over this body's centres, from an independent stream —
    // the same discipline generate_population_centres names under, so renaming
    // cannot perturb any other pass.
    std::vector<entity_id> ids;
    for (const auto& [cid, tid] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tid);
        if (tit != w.tiles.end() && tit->second.body == body_id)
            ids.push_back(cid);
    }
    std::sort(ids.begin(), ids.end());

    // WHOSE GROUND. The nearest region owns the settlement's speech, the same rule
    // that gave the region its own culture from the nearest cradle — so a city, its
    // region and its gods share one tongue. BL-826 — PLURALITY: a town is named in
    // the tongue of the largest people on the ground under it. -1 = no culture to
    // name it from, and the centre keeps the placeholder name it already carries.
    const auto culture_of = [&](entity_id cid) -> int {
        const auto tit = w.tiles.find(w.population_centre_tile[cid]);
        if (tit == w.tiles.end()) return -1;
        const int pi = nearest_region(ss, tit->second.grid_x, tit->second.grid_y, gw);
        if (pi < 0) return -1;
        const int ci = ss.regions[static_cast<std::size_t>(pi)].culture.plurality();
        return (ci < 0 || ci >= static_cast<int>(cs.cultures.size())) ? -1 : ci;
    };

    // Names unique on the body (Ben, 2026-10-13). A centre this pass will NOT rename
    // keeps its placeholder, so those names are taken before any new one is coined.
    std::unordered_set<std::string> used;
    for (const entity_id cid : ids)
        if (culture_of(cid) < 0)
        {
            const auto nit = w.population_centre_name.find(cid);
            if (nit != w.population_centre_name.end() && !nit->second.empty())
                used.insert(nit->second);
        }

    std::mt19937 name_rng(seed ^ 0xC17910E6u);
    for (const entity_id cid : ids)
    {
        const int ci = culture_of(cid);
        if (ci < 0) continue;

        const tongue& t = cs.cultures[static_cast<std::size_t>(ci)].speech;
        std::string name = generate_unique_city_name(name_rng, t, used);
        if (!name.empty())
            w.population_centre_name[cid] = std::move(name);
        else if (const auto nit = w.population_centre_name.find(cid);
                 nit != w.population_centre_name.end() && !nit->second.empty())
            used.insert(nit->second); // the tongue cannot coin: the placeholder stays, and is taken
    }
}
