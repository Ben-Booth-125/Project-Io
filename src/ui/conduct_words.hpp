#pragma once
// BL-1470: the player's words for a sentiment factor kind — RELATIONS.md § The factors, read
// aloud. Header-only and ImGui-free so a world harness can check every kind has words.
// Indexed by the kind, so a new kind is one more entry; the static_assert below refuses to
// compile a table shorter than the kind roster (a short table left a null "%s", BL-1431 review).

#include "world/sentiment.hpp"

#include <cstddef>
#include <iterator>

namespace ui {

inline constexpr const char* k_conduct_words[] = {
    "contracts delivered",       // contract_completed
    "contracts cancelled",       // contract_cancelled
    "trade",                     // trade_conducted
    "a firm taken",              // equity_taken
    "a declaration of war",      // hostility_declared
    "a friendship accepted",     // friendship_accepted
    "levies charged",            // levy_charged
    "an embargo",                // embargo_imposed
    "lobbying against them",     // lobbied_against
    "force used",                // force_used
    "a contract failed",         // contract_failed
    "an old grudge",             // historical_grudge
    "a war declared on others",  // war_declared
    "a peace concluded",         // peace_concluded
    "provinces ceded",           // province_ceded
    "visible technology",        // technology_visible
    "large facilities",          // facility_visible
    "force on the border",       // border_force
    "trade severed",             // trade_severed
    "unrest",                    // unrest_visible
    "a nuclear weapon used",     // nuclear_used (BL-1431)
};

constexpr bool conduct_words_all_set()
{
    for (const char* s : k_conduct_words)
        if (s == nullptr || s[0] == '\0')
            return false;
    return true;
}

static_assert(std::size(k_conduct_words) == sentiment_factor_count,
              "every sentiment_factor_kind needs its words in k_conduct_words");
static_assert(conduct_words_all_set(), "no sentiment kind may read as an empty word");

inline const char* conduct_words(sentiment_factor_kind k)
{
    const std::size_t i = static_cast<std::size_t>(k);
    return i < std::size(k_conduct_words) ? k_conduct_words[i] : "conduct";
}

} // namespace ui
