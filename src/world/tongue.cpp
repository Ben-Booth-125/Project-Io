#include "tongue.hpp"

namespace {

// ---------------------------------------------------------------------------
// Deterministic RNG — the same splitmix64 shape as creeds.cpp /
// history_ladder.cpp, duplicated per the convention that each generation file
// owns its stream. Here the stream is seeded from the TONGUE, not from the
// world seed: coining is a property of the language, not of the pass that
// happens to ask for it.
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
    explicit rng(uint64_t seed) : s(splitmix64(seed ^ 0x10E6D5A17u)) {}

    int pick(int n)
    {
        s = splitmix64(s);
        return n <= 0 ? 0 : static_cast<int>((s >> 33) % static_cast<uint64_t>(n));
    }
};

/// Fold a tongue's whole inventory into one 64-bit signature. Order-sensitive,
/// which is what we want: two tongues holding the same sounds in a different
/// draw order are different tongues.
uint64_t signature(const tongue& t)
{
    uint64_t h = 0xC0FFEE1234567890ull;
    auto fold = [&h](const std::vector<std::string>& pool, uint64_t salt) {
        h ^= splitmix64(salt);
        for (const std::string& s : pool)
            for (const char c : s)
                h = splitmix64(h ^ static_cast<uint64_t>(static_cast<unsigned char>(c)));
    };
    fold(t.onsets, 0x11u);
    fold(t.vowels, 0x22u);
    fold(t.codas,  0x33u);
    return h;
}

} // namespace

tongue_lexicon coin_lexicon(const tongue& t)
{
    tongue_lexicon lex;
    if (!t.usable())
        return lex;

    rng r(signature(t));

    // Three of each: enough that a culture's nations are not all suffixed
    // identically, few enough that the shared morphemes stay recognisable as
    // one language. Polity words run long (two syllables), settlement
    // morphemes short (one), because that is the shape a suffix wears.
    for (int i = 0; i < 3; ++i) lex.polity.push_back(tongue_word(r, t, 2));
    for (int i = 0; i < 3; ++i) lex.settlement.push_back(tongue_lower(tongue_word(r, t, 1)));
    for (int i = 0; i < 3; ++i) lex.qualifier.push_back(tongue_word(r, t, 1 + r.pick(2)));

    // BL-348: nine quarter words, drawn LAST so the three vocabularies above keep
    // their exact draws — every nation and city name generated before this
    // change is byte-identical after it. Two syllables, the length a standalone
    // noun wears (a settlement morpheme is a suffix and runs shorter).
    for (int i = 0; i < 9; ++i) lex.quarter.push_back(tongue_word(r, t, 2));
    return lex;
}

std::string coin_realm_name(const tongue& t, uint64_t salt)
{
    if (!t.usable())
        return std::string{};

    // The tongue's own stream, crossed with the realm: `signature` is what
    // seeds `coin_lexicon`, so the words below and the lexicon's morphemes come
    // out of one sound system, and the salt keeps two realms of one people
    // from coining the same word.
    rng r(signature(t) ^ splitmix64(salt ^ 0x5EA10F0A11u));
    const tongue_lexicon lex = coin_lexicon(t);

    const int   form = r.pick(3);
    std::string name = tongue_word(r, t, 2 + r.pick(2));
    if (name.empty()) return name;

    switch (form)
    {
        case 0:
            return name;
        case 1:
            if (lex.qualifier.empty()) return name;
            return lex.qualifier[static_cast<std::size_t>(r.pick(static_cast<int>(lex.qualifier.size())))]
                 + " " + name;
        default:
            if (lex.polity.empty()) return name;
            return name + " "
                 + lex.polity[static_cast<std::size_t>(r.pick(static_cast<int>(lex.polity.size())))];
    }
}

std::string tongue_lower(const std::string& w)
{
    std::string out = w;
    for (char& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}
