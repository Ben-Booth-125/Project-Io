#include "creeds.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {

// ---------------------------------------------------------------------------
// Deterministic RNG — same splitmix64 shape as planetology.cpp /
// history_ladder.cpp, duplicated per the convention that each generation file
// owns its stream.
// ---------------------------------------------------------------------------
uint64_t splitmix64(uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

struct rng
{
    uint64_t s;
    rng(uint32_t seed, uint32_t stage_tag)
        : s(splitmix64((static_cast<uint64_t>(seed) << 32) ^ (stage_tag * 0x9E3779B1u))) {}

    float unit()
    {
        s = splitmix64(s);
        return static_cast<float>((s >> 40) & 0xFFFFFFull) * (1.0f / 16777216.0f);
    }

    /// Integer in [0, n). Integer path for anything that participates in
    /// selection, per the gate-path float ban.
    int pick(int n)
    {
        s = splitmix64(s);
        return n <= 0 ? 0 : static_cast<int>((s >> 33) % static_cast<uint64_t>(n));
    }
};

// FRESH stage tags — none collides with the ladder's (0x5A11 / 0xC4A7 /
// 0xF2A6), the continents' (0xC017) or the planetology chain's.
constexpr uint32_t tag_pantheon = 0xD317u; // Pantheon + tongue generation.
constexpr uint32_t tag_conflict = 0x1B47u; // The tribal-conflict stage.

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---------------------------------------------------------------------------
// Tongue — a tiny per-culture phonology. Each culture draws a small consonant
// and vowel inventory from the pools below; every proper noun it ever coins
// (its own name, its gods) is built from that inventory, so two cultures'
// names SOUND different because their inventories differ, not because a style
// flag says so.
// ---------------------------------------------------------------------------
struct phonology
{
    std::vector<const char*> onsets;
    std::vector<const char*> vowels;
    std::vector<const char*> codas;
};

phonology roll_phonology(rng& r)
{
    static const char* onset_pool[] = { "k", "t", "m", "n", "s", "r", "l", "v",
                                        "th", "sh", "g", "d", "b", "h", "z", "kh" };
    static const char* vowel_pool[] = { "a", "e", "i", "o", "u", "ai", "ua", "ei" };
    static const char* coda_pool[]  = { "n", "r", "l", "s", "k", "th", "m" };

    phonology p;
    // Walk each pool once with an independent keep-roll, so the draw is a
    // deterministic subset rather than a sample-with-retry loop.
    for (const char* c : onset_pool) if (r.pick(16) < 6) p.onsets.push_back(c);
    for (const char* v : vowel_pool) if (r.pick(8)  < 3) p.vowels.push_back(v);
    for (const char* c : coda_pool)  if (r.pick(7)  < 2) p.codas.push_back(c);

    // An inventory can come up short; backfill deterministically so word()
    // always has material. The backfill order is fixed, so this stays pure.
    if (p.onsets.size() < 4) { p.onsets = { "k", "t", "m", "r" }; }
    if (p.vowels.size() < 2) { p.vowels = { "a", "i" }; }
    return p;
}

std::string word(rng& r, const phonology& p, int syllables)
{
    std::string s;
    for (int i = 0; i < syllables; ++i)
    {
        s += p.onsets[static_cast<std::size_t>(r.pick(static_cast<int>(p.onsets.size())))];
        s += p.vowels[static_cast<std::size_t>(r.pick(static_cast<int>(p.vowels.size())))];
        // A coda closes roughly a third of syllables, when the tongue has any.
        if (!p.codas.empty() && r.pick(3) == 0)
            s += p.codas[static_cast<std::size_t>(r.pick(static_cast<int>(p.codas.size())))];
    }
    if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}

/// A name unused so far in @p taken. Bounded retry, deterministic: the retry
/// walk consumes the same stream every run.
std::string fresh_name(rng& r, const phonology& p, int syllables,
                       std::vector<std::string>& taken)
{
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        std::string n = word(r, p, syllables);
        if (std::find(taken.begin(), taken.end(), n) == taken.end())
        {
            taken.push_back(n);
            return n;
        }
    }
    // Eight collisions means a tiny inventory; suffix the count, still unique.
    std::string n = word(r, p, syllables) + "-" + std::to_string(taken.size());
    taken.push_back(n);
    return n;
}

const tile_component* tile_at(const world& w, const std::vector<entity_id>& ids, std::size_t idx)
{
    if (idx >= ids.size()) return nullptr;
    const auto it = w.tiles.find(ids[idx]);
    return it == w.tiles.end() ? nullptr : &it->second;
}

/// What the land around a cradle offers its creed. Same neighbourhood window
/// the ladder's Pass 2 claimed for the cradle.
struct cradle_land
{
    int wetland = 0, forest = 0, barrier = 0, land = 0;
    bool ore = false;
};

cradle_land survey_land(const world& w, const std::vector<entity_id>& tile_ids,
                        const agrarian_cradle& c, int gw, int gh)
{
    cradle_land out;
    const int separation = std::max(6, gw / 12);
    for (int dr = -separation; dr <= separation; ++dr)
    {
        const int r = c.row + dr;
        if (r < 0 || r >= gh) continue;
        for (int dc = -separation; dc <= separation; ++dc)
        {
            const int col = ((c.col + dc) % gw + gw) % gw;
            const tile_component* t = tile_at(w, tile_ids, static_cast<std::size_t>(col + r * gw));
            if (!t) continue;
            if (t->composition != terrain_composition::ocean) ++out.land;
            if (t->composition == terrain_composition::wetland) ++out.wetland;
            if (t->composition == terrain_composition::forest)  ++out.forest;
            if (t->landform == terrain_landform::mountain ||
                t->landform == terrain_landform::canyon) ++out.barrier;
            if (t->resource_deposit[static_cast<std::size_t>(resource_type::iron_ore)] > 0.0f)
                out.ore = true;
        }
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// The pantheons — one per cradle, each in its own tongue.
// ---------------------------------------------------------------------------

creed_state run_creeds(const planetology_state& pl,
                       const history_ladder_state& hl,
                       const world& w,
                       const std::vector<entity_id>& tile_ids,
                       int gw, int gh, uint32_t seed)
{
    creed_state out;
    if (hl.cradles.empty() || gw <= 0 || gh <= 0)
        return out;

    const int arable_q = clampi(static_cast<int>(pl.arable_share * 1000.0f), 0, 1000);
    rng r(seed, tag_pantheon);

    for (std::size_t ci = 0; ci < hl.cradles.size(); ++ci)
    {
        const agrarian_cradle& c = hl.cradles[ci];
        const cradle_land land = survey_land(w, tile_ids, c, gw, gh);

        culture cu;
        cu.cradle = static_cast<int>(ci);

        const phonology p = roll_phonology(r);
        std::vector<std::string> taken;
        cu.name = fresh_name(r, p, 2 + r.pick(2), taken);

        // Harsh ground breeds a harsher creed: the barrier share of the
        // cradle's own window raises every god's zeal floor. Integer percent.
        const int harsh = clampi((land.barrier * 100) / std::max(1, land.land), 0, 100);

        auto add_god = [&](const char* domain, const char* epithet,
                           int zeal_lo, int zeal_hi, int dom_lo, int dom_hi) {
            culture_god g;
            g.name     = fresh_name(r, p, 2 + r.pick(2), taken);
            g.domain   = domain;
            g.epithet  = epithet;
            g.zeal     = clampi(zeal_lo + r.pick(zeal_hi - zeal_lo + 1) + harsh / 34, 0, 10);
            g.dominion = clampi(dom_lo + r.pick(dom_hi - dom_lo + 1), 0, 10);
            cu.pantheon.push_back(std::move(g));
        };

        // The chief god is the land's own portrait — the archetype table's
        // terrain column. Coast beats floodplain beats forest beats open sky,
        // because that is the order in which each dominates a people's days.
        if (c.coastal)
            add_god("the sea", "who holds the harbours", 2, 6, 5, 9);
        else if (land.wetland * 2 >= std::max(1, land.land) / 4)
            add_god("the river", "who fattens the floodplain", 1, 4, 4, 8);
        else if (land.forest * 2 >= std::max(1, land.land) / 3)
            add_god("the green dark", "who keeps the old paths", 2, 5, 3, 7);
        else
            add_god("the storm", "who splits the sky", 4, 8, 5, 9);

        // Every creed raises a war god and a door of the dead — no people
        // skipped either question. The war god carries the culture's teeth.
        add_god("war", "who counts the spears", 6, 10, 3, 9);
        add_god("the door of the dead", "who takes no bribe", 0, 3, 8, 10);

        // Conditional seats: an ore province raises a forge; the charter
        // cradle raises an oath god — the seed Stage 1's Charter Act grows
        // from, planted here so the enforceable promise has a creed behind it.
        if (land.ore)
            add_god("the forge", "who buys sweat with iron", 3, 6, 4, 8);
        if (hl.charter_cradle == static_cast<int>(ci))
            add_god("the sealed oath", "who witnesses every bargain", 0, 2, 6, 10);

        const culture_god& chief = cu.pantheon[0];
        const culture_god& war_g = cu.pantheon[1];
        cu.aggression_q = clampi(war_g.zeal * 70 + chief.zeal * 20 + war_g.dominion * 10, 0, 1000);

        // The shrine line. Dated AFTER the granary line by construction: the
        // granary year is -(3000 + 6*arable + jitter), this is
        // -(2500 + 5*arable + jitter<=400), and 500 + arable always clears it.
        const int jitter = static_cast<int>(r.unit() * 400.0f);
        const int64_t year = -(2500 + static_cast<int64_t>(arable_q) * 5 + jitter);

        out.history.push_back(history_event{
            years_from_calendar_year(year), chain_stage::legacy,
            "The " + cu.name + " raise the first shrine of " +
                chief.name + ", " + chief.domain + " - " + chief.epithet + ".",
            "-> one pantheon, one tongue; " + std::to_string(cu.pantheon.size()) +
                " gods, and the war-bands' zeal is " + std::to_string(war_g.zeal) + "/10" });

        out.cultures.push_back(std::move(cu));
    }

    return out;
}

// ---------------------------------------------------------------------------
// The tribal-conflict stage — the creeds reach the political map.
// ---------------------------------------------------------------------------

void record_tribal_conflict(creed_state& cs,
                            history_ladder_state& hl,
                            uint32_t seed)
{
    if (cs.cultures.size() < 2 || hl.cradles.size() < 2)
        return;

    rng r(seed, tag_conflict);
    const int floor_q = hl.fragmentation_q / 2; // Welding can never halve-and-more.
    int welds = 0;

    for (std::size_t i = 0; i < cs.cultures.size(); ++i)
    {
        const culture& a = cs.cultures[i];
        if (a.aggression_q <= 550) continue; // Peaceable creeds farm instead.

        // Nearest other cradle by grid distance, columns wrapping; ties break
        // on the LOWEST index, same rule as every other selection in the layer.
        const agrarian_cradle& ac = hl.cradles[static_cast<std::size_t>(a.cradle)];
        int best = -1, best_d = 1 << 30;
        for (std::size_t j = 0; j < cs.cultures.size(); ++j)
        {
            if (j == i) continue;
            const agrarian_cradle& bc = hl.cradles[static_cast<std::size_t>(cs.cultures[j].cradle)];
            const int dc = std::abs(ac.col - bc.col);
            const int dr = std::abs(ac.row - bc.row);
            const int d  = std::min(dc, 180 - dc) + dr; // gw wrap priced coarsely.
            if (d < best_d) { best_d = d; best = static_cast<int>(j); }
        }
        if (best < 0) continue;
        const culture& b = cs.cultures[static_cast<std::size_t>(best)];

        // Attack must clear the defence AND the ground: the ladder's conquest
        // cost prices the march, exactly as Stage 2 priced it for armies.
        const int attack  = a.pantheon[1].dominion * 60 + a.aggression_q / 2 + r.pick(120);
        const int defence = b.pantheon[1].dominion * 60 + b.aggression_q / 4
                          + hl.conquest_cost_q / 2 + r.pick(120);

        const int jitter = r.pick(300);
        const int64_t year = -(1500 - static_cast<int64_t>(i) * 60 + jitter);

        if (attack > defence)
        {
            ++welds;
            cs.history.push_back(history_event{
                years_from_calendar_year(year), chain_stage::legacy,
                "The " + a.name + " war-bands march under " + a.pantheon[1].name +
                    "; the " + b.name + " cradle falls.",
                "-> two peoples weld into one; fragmentation falls" });
        }
        else
        {
            cs.history.push_back(history_event{
                years_from_calendar_year(year), chain_stage::legacy,
                "The " + a.name + " war-bands break against the " + b.name + " ground.",
                "-> conquest priced too high; the frontier holds" });
        }
    }

    if (welds > 0)
        hl.fragmentation_q = std::max(floor_q, hl.fragmentation_q - welds * 120);
}

// ---------------------------------------------------------------------------
// Globalisation — the common tongue closes generation.
// ---------------------------------------------------------------------------

void record_globalisation(creed_state& cs, const world& w, entity_id body_id)
{
    if (cs.cultures.empty())
        return;

    // The same sorted-key realm count record_institutional_history performs —
    // sorted, so the count cannot depend on unordered_map iteration order.
    std::vector<entity_id> nation_ids;
    nation_ids.reserve(w.nations.size());
    for (const auto& kv : w.nations) nation_ids.push_back(kv.first);
    std::sort(nation_ids.begin(), nation_ids.end());

    int realms = 0;
    for (const entity_id nid : nation_ids)
    {
        const auto nit = w.nations.find(nid);
        if (nit == w.nations.end() || nit->second.tiles.empty()) continue;
        const auto tit = w.tiles.find(nit->second.tiles.front());
        if (tit != w.tiles.end() && tit->second.body == body_id) ++realms;
    }

    // Fixed late date, deliberately: globalisation is the END of the
    // generated story, the hinge to the campaign epoch, not a rolled outcome.
    cs.history.push_back(history_event{
        years_from_calendar_year(1951), chain_stage::legacy,
        "The common tongue spreads through every port and press.",
        "-> " + std::to_string(std::max(realms, 1)) +
            " realms, one trade language; the old tongues survive in the names of gods" });
}
