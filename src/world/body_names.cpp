#include "body_names.hpp"

#include "planetology.hpp" // the prototype body set — what each body IS
#include "tongue.hpp"      // the shared phoneme inventory + word builder (BL-290)

#include <cstdio>

namespace {

// ---------------------------------------------------------------------------
// Deterministic RNG — the same splitmix64 shape as tongue.cpp / creeds.cpp /
// history_ladder.cpp, per the convention that each generation file owns its
// stream. Seeded from the world seed alone, so the catalogue is a pure function
// of it.
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
    explicit rng(uint64_t seed) : s(splitmix64(seed ^ 0xB0D1E5u)) {}

    int pick(int n)
    {
        s = splitmix64(s);
        return n <= 0 ? 0 : static_cast<int>((s >> 33) % static_cast<uint64_t>(n));
    }
};

/// True once @p candidate has not already been handed to another body. Coining
/// draws from a small inventory, so collisions are ordinary rather than rare —
/// the caller re-coins with an extra syllable rather than accepting a duplicate.
bool taken(const std::vector<std::string>& used, const std::string& candidate)
{
    for (const std::string& u : used)
        if (u == candidate)
            return true;
    return false;
}

/// Coin a word of @p syllables that no other body in this system holds. Adds a
/// syllable every few attempts, so the loop terminates on any tongue: a longer
/// word has strictly more distinct forms available.
std::string coin_unique(rng& r, const tongue& t, int syllables,
                        const std::vector<std::string>& used)
{
    for (int attempt = 0; attempt < 32; ++attempt)
    {
        const std::string w = tongue_word(r, t, syllables + attempt / 8);
        if (!w.empty() && !taken(used, w))
            return w;
    }
    return tongue_word(r, t, syllables + 4); // pathological tongue; accept what we get.
}

} // namespace

body_naming generate_body_names(uint32_t seed)
{
    body_naming out;

    rng r(seed);
    const tongue          sky = roll_tongue(r);
    const tongue_lexicon  lex = coin_lexicon(sky);

    const int n = prototype_body_count();
    out.bodies.assign(static_cast<std::size_t>(n), std::string{});

    std::vector<std::string> used;
    used.reserve(static_cast<std::size_t>(n) + 1);

    // The star first: the sky's chief light, and the mythological register's
    // clearest case.
    out.star = coin_unique(r, sky, 2 + r.pick(2), used);
    used.push_back(out.star);

    // Pass 1 — everything that is not a moon. A moon's name is built from its
    // parent's, so the parents must be named first.
    int home_index = -1;
    int catalogue_index = 0;
    for (int i = 0; i < n; ++i)
    {
        const body_inputs& in = prototype_body(i);
        if (in.parent_mass_earths > 0.0f)
            continue; // a moon — pass 2.

        std::string name;
        if (in.is_homeworld)
        {
            // DESCRIPTIVE-OF-THE-GROUND: a root plus the tongue's own "place"
            // morpheme. The homeworld is the one body named by people standing
            // on it, and its shape says so.
            home_index = i;
            const std::string root = coin_unique(r, sky, 2, used);
            name = root;
            if (!lex.settlement.empty())
                name += lex.settlement[static_cast<std::size_t>(r.pick(static_cast<int>(lex.settlement.size())))];
        }
        else if (in.core_fragment)
        {
            // CATALOGUE: a coined word plus its index in the catalogue. Neither
            // a light in the sky nor ground underfoot — a line in a list.
            const std::string root = coin_unique(r, sky, 2, used);
            char buf[64];
            std::snprintf(buf, sizeof buf, "%s %d", root.c_str(), ++catalogue_index);
            name = buf;
        }
        else
        {
            // MYTHOLOGICAL: one bare coined word, no affix.
            name = coin_unique(r, sky, 2 + r.pick(2), used);
        }

        out.bodies[static_cast<std::size_t>(i)] = name;
        used.push_back(name);
    }

    // Pass 2 — moons. The prototype set encodes a moon's primary only as
    // "it has one" (`parent_mass_earths`), and the homeworld is the sole body
    // in the set carrying moons, so that is the parent used here. A prototype
    // set with a second moon-bearing planet would need the parent recorded
    // explicitly rather than inferred.
    for (int i = 0; i < n; ++i)
    {
        const body_inputs& in = prototype_body(i);
        if (in.parent_mass_earths <= 0.0f)
            continue;

        const std::string parent =
            (home_index >= 0) ? out.bodies[static_cast<std::size_t>(home_index)] : out.star;

        // Qualifier FIRST, as a separate word: "Zair Huhaidar" rather than
        // "Huhaidarzair". Suffixing onto a three-syllable planet name produces
        // an unreadable run, and the two-word form makes the relationship to
        // the parent legible at a glance, which is the whole point of the
        // descriptive register.
        std::string name;
        for (int attempt = 0; attempt < 16; ++attempt)
        {
            const std::string q =
                lex.qualifier.empty()
                    ? tongue_word(r, sky, 1)
                    : lex.qualifier[static_cast<std::size_t>(r.pick(static_cast<int>(lex.qualifier.size())))];
            name = q + " " + parent;
            if (!taken(used, name))
                break;
        }

        out.bodies[static_cast<std::size_t>(i)] = name;
        used.push_back(name);
    }

    return out;
}
