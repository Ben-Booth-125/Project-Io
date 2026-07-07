#include "city_names.hpp"

#include <iterator> // std::size

namespace {

// Capitalised consonant onsets (single + clusters) — the name's first sound.
const char* const k_onset[] = {
    "B","Br","C","Cl","D","Dr","F","Fl","G","Gr","H","K","Kr","L","M","N","P","Pr",
    "R","S","Sh","Sk","St","T","Th","Tr","V","W","Wr","Bl","Gl"
};
// Vowel nuclei (some diphthongs) — the root's vowel.
const char* const k_vowel[] = { "a","e","i","o","u","ae","ea","ei","ou","ai","au" };
// Optional medial consonant(s) closing the root before the suffix.
const char* const k_medial[] = { "","","l","r","n","m","d","th","st","nd","rk","ll","sh","rn" };
// English-ish place suffixes — the "settlement" morpheme.
const char* const k_suffix[] = {
    "ton","ford","haven","burg","moor","gate","wick","dale","bury","field",
    "port","stead","holm","march","mere","by","ham","cross","reach","bridge"
};
// Occasional directional / quality prefix (with a trailing space).
const char* const k_prefix[] = { "North ","South ","East ","West ","New ","Old ","High ","Little " };

template <std::size_t N>
const char* pick(const char* const (&arr)[N], std::mt19937& rng)
{
    std::uniform_int_distribution<std::size_t> d(0, N - 1);
    return arr[d(rng)];
}

} // namespace

std::string generate_city_name(std::mt19937& rng)
{
    // Root = onset + vowel, occasionally a second onset+vowel for length, then a single
    // medial consonant before the suffix. The medial goes last (not between the two
    // syllables) so a second root does not create a mid-word consonant pile-up.
    std::string root = pick(k_onset, rng);
    root += pick(k_vowel, rng);
    if (std::uniform_int_distribution<int>(0, 2)(rng) == 0) // ~1 in 3: longer root
    {
        root += pick(k_onset, rng);
        root += pick(k_vowel, rng);
    }
    root += pick(k_medial, rng);

    std::string name = root + pick(k_suffix, rng);

    if (std::uniform_int_distribution<int>(0, 5)(rng) == 0) // ~1 in 6: directional prefix
        name = std::string(pick(k_prefix, rng)) + name;

    return name;
}
