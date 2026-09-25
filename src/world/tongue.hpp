#pragma once

// ---------------------------------------------------------------------------
// Tongue — the per-culture phonology, and the naming machinery that consumes it
// (BL-290).
//
// The creeds pass (BL-235) already rolls one small consonant/vowel inventory
// per cradle-culture and coins that culture's own name and its gods out of it.
// This header lifts that inventory out of creeds.cpp so it can be CONSUMED
// downstream: a nation grown from a region settled by a culture is named in
// THAT culture's tongue, and so is every city on its ground. The shared sound
// system is therefore a consequence of the generation chain, not a coincidence
// of two banks that happened to be written in the same accent.
//
// THE STANDING RULE (.claude/rules/io-standing-rules.md, Terms & docs): every
// generated name is produced by the seeded phoneme tables here, never drawn
// from an Earth list and never Earth-flavoured. That extends to the STRUCTURAL
// words — a culture coins its own morphemes for "realm" and "town"
// (`tongue_lexicon`), so no name carries an English or Latin morpheme.
// ---------------------------------------------------------------------------

#include <cctype>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

/// One culture's phonology: the sounds it builds every proper noun from.
struct tongue
{
    std::vector<std::string> onsets;
    std::vector<std::string> vowels;
    std::vector<std::string> codas;

    /// A tongue with no onsets or no vowels cannot coin a word.
    bool usable() const { return !onsets.empty() && !vowels.empty(); }
};

/// The structural vocabulary a tongue coins for itself — the words that would
/// otherwise be borrowed from English or Latin ("Republic", "-ton", "Free").
/// Every culture has its own, so nations sharing a tongue read as kin.
struct tongue_lexicon
{
    std::vector<std::string> polity;     ///< "realm" words, following a nation name.
    std::vector<std::string> settlement; ///< "town" morphemes, suffixed to a city root.
    std::vector<std::string> qualifier;  ///< Standing epithets, preceding a name.

    /// Region words a region name takes ("<People> <Region>"), BL-348. NINE,
    /// because `settlement.cpp`'s positional mapping selects from a 5-band
    /// (north→south) axis and a 4-sector (dawnward→outer) axis: indices 0-4 are
    /// the bands, 5-8 the sectors. Sized to that mapping deliberately, so the
    /// name keeps carrying a FACT ABOUT THE GROUND rather than becoming
    /// decorative — a coastal region and an interior one still read
    /// differently, they just read differently in their own language.
    std::vector<std::string> quarter;
};

/// Roll one tongue. Draws a deterministic subset of each pool with an
/// independent keep-roll per entry, so no sample-with-retry loop is needed.
///
/// Templated on the RNG so the creeds pass (its own splitmix stream) and the
/// generation passes (`std::mt19937`, via `mt_picker`) share ONE inventory and
/// one word-builder. @p r must expose `int pick(int n)` returning [0, n).
template <class Rng>
tongue roll_tongue(Rng& r)
{
    static const char* const onset_pool[] = { "k", "t", "m", "n", "s", "r", "l", "v",
                                              "th", "sh", "g", "d", "b", "h", "z", "kh" };
    static const char* const vowel_pool[] = { "a", "e", "i", "o", "u", "ai", "ua", "ei" };
    static const char* const coda_pool[]  = { "n", "r", "l", "s", "k", "th", "m" };

    tongue p;
    for (const char* c : onset_pool) if (r.pick(16) < 6) p.onsets.push_back(c);
    for (const char* v : vowel_pool) if (r.pick(8)  < 3) p.vowels.push_back(v);
    for (const char* c : coda_pool)  if (r.pick(7)  < 2) p.codas.push_back(c);

    // An inventory can come up short; backfill deterministically so the word
    // builder always has material. The backfill order is fixed, so this stays
    // pure.
    if (p.onsets.size() < 4) { p.onsets = { "k", "t", "m", "r" }; }
    if (p.vowels.size() < 2) { p.vowels = { "a", "i" }; }
    return p;
}

/// Build one word of @p syllables in @p t, capitalised. A coda closes roughly
/// a third of syllables, when the tongue has any.
template <class Rng>
std::string tongue_word(Rng& r, const tongue& t, int syllables)
{
    std::string s;
    if (!t.usable()) return s;
    for (int i = 0; i < syllables; ++i)
    {
        s += t.onsets[static_cast<std::size_t>(r.pick(static_cast<int>(t.onsets.size())))];
        s += t.vowels[static_cast<std::size_t>(r.pick(static_cast<int>(t.vowels.size())))];
        if (!t.codas.empty() && r.pick(3) == 0)
            s += t.codas[static_cast<std::size_t>(r.pick(static_cast<int>(t.codas.size())))];
    }
    if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}

/// `std::mt19937` wearing the `pick(int)` contract the templates above want, so
/// the generation passes (which own mt19937 streams) can coin words without
/// duplicating the phoneme tables.
struct mt_picker
{
    std::mt19937& g;

    explicit mt_picker(std::mt19937& gen) : g(gen) {}

    int pick(int n)
    {
        if (n <= 1) return 0;
        return std::uniform_int_distribution<int>(0, n - 1)(g);
    }
};

/// Coin @p t's structural vocabulary. A PURE FUNCTION of the tongue — the
/// stream is seeded by hashing the inventory itself, not by any caller's RNG —
/// so one culture coins the same words wherever it is consumed (nation names
/// in one pass, city names in another) without those passes having to agree on
/// a stream or carry the words between them.
tongue_lexicon coin_lexicon(const tongue& t);

/// Coin a REALM's name in @p t (BL-1088; CIVILISATION.md sec A realm's name;
/// the register in NATION_GENERATION.md sec Pass 5). The same three structural
/// forms `make_nation_name` builds — bare name, coined epithet + name, name +
/// coined realm word — over `coin_lexicon(t)`'s own morphemes, so a realm and
/// the nation it becomes read as one sound system.
///
/// A PURE FUNCTION of the tongue and @p salt, on the same footing as
/// `coin_lexicon`: the stream is seeded from the tongue's own signature
/// crossed with the salt, never from the sim's RNG, so coining a name inside
/// `run_history_sim` draws nothing from the decision loop and moves no digest.
/// @p salt tells two realms of one culture apart — the caller passes the realm's
/// own id and founding seat; the same realm coins the same name on replay.
/// Empty when the tongue cannot coin (the caller falls back to its seat's name).
std::string coin_realm_name(const tongue& t, uint64_t salt);

/// Lower-case @p w, for use as a suffix morpheme inside a longer name.
std::string tongue_lower(const std::string& w);
