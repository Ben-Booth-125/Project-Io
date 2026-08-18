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

    std::mt19937 name_rng(seed ^ 0xC17910E6u);
    for (const entity_id cid : ids)
    {
        const auto tit = w.tiles.find(w.population_centre_tile[cid]);
        if (tit == w.tiles.end()) continue;

        // WHOSE GROUND. The nearest region owns the settlement's speech, the
        // same rule that gave the region its own culture from the nearest
        // cradle — so a city, its region and its gods share one tongue.
        const int pi = nearest_region(ss, tit->second.grid_x, tit->second.grid_y, gw);
        if (pi < 0) continue;
        const int ci = ss.regions[static_cast<std::size_t>(pi)].culture;
        if (ci < 0 || ci >= static_cast<int>(cs.cultures.size())) continue;

        const tongue& t = cs.cultures[static_cast<std::size_t>(ci)].speech;
        std::string name = generate_city_name(name_rng, t);
        if (!name.empty())
            w.population_centre_name[cid] = std::move(name);
    }
}
