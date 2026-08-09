#pragma once

// ---------------------------------------------------------------------------
// Creeds — one pantheon per cradle-culture, in that culture's own tongue
// (BL-235; docs/lore/CREEDS.md).
//
// Runs between the pre-national history ladder (BL-221) and nation generation.
// Each agrarian cradle becomes a CULTURE: it rolls its own phonology (a small
// consonant/vowel inventory), names itself and its gods in that tongue, and
// derives its pantheon's shape from the land it sits on — a coastal cradle
// raises a sea god, an ore province a forge god, the charter cradle an oath
// god. ONE pantheon per culture (Ben, 2026-07-31): the tongue and the creed
// are the same act of self-description.
//
// THE CREED DRIVES, IT DOES NOT NARRATE (the BL-221 rule, inherited): each
// culture's temperament — zeal (relish for battle) and dominion (expectation
// of prevailing) — prices the tribal-conflict stage, which welds cradles
// together and LOWERS the ladder's fragmentation before nation_params_from_
// ladder reads it. A world of warlike creeds grows fewer, larger polities.
//
// Globalisation closes the pass: at the end of generation a common trade
// tongue spreads, rendered as the player's own language (English for now —
// Ben, 2026-07-31). Proper names stay native; prose goes common.
// ---------------------------------------------------------------------------

#include "history_ladder.hpp"
#include "planetology.hpp"
#include "tongue.hpp"

#include <string>
#include <vector>

/// One god of a culture's pantheon.
struct culture_god
{
    std::string name;    ///< Proper name in the culture's own tongue.
    std::string domain;  ///< Common-tongue domain word ("the storm", "the sea").
    std::string epithet; ///< Common-tongue epithet rendered after the name.

    /// Temperament axes, 0-10 integer — the two axes of the archetype table
    /// (how much the god relishes battle; how surely it expects to prevail).
    /// These are the pantheon's contribution to the culture's war disposition.
    int zeal     = 0;
    int dominion = 0;
};

/// One cradle-culture: a people, their tongue, and their single pantheon.
struct culture
{
    int cradle = -1;                 ///< Index into history_ladder_state::cradles.
    std::string name;                ///< The people's own name for themselves.
    /// The people's tongue — the phoneme inventory their own name and their
    /// gods were coined from, kept so the naming passes downstream (nations,
    /// cities) coin from the SAME sounds rather than a bank of their own
    /// (BL-290, world/tongue.hpp).
    ::tongue speech;
    std::vector<culture_god> pantheon;

    /// 0-1000 — how readily this culture's war-bands march, derived from the
    /// pantheon's zeal. Consumed by record_tribal_conflict.
    int aggression_q = 0;
};

/// What the creeds pass computed for one body.
struct creed_state
{
    std::vector<culture> cultures;   ///< One per cradle, in cradle order.
    std::vector<history_event> history; ///< Dated shrine / war / tongue lines.
};

/// Grow one culture (tongue + pantheon + temperament) per agrarian cradle.
///
/// Pure deterministic function of (@p pl, @p hl, @p w tiles, @p tile_ids,
/// @p gw, @p gh, @p seed). A world with no cradles grows no creeds and the
/// brevity of that record is its characterisation.
creed_state run_creeds(const planetology_state& pl,
                       const history_ladder_state& hl,
                       const world& w,
                       const std::vector<entity_id>& tile_ids,
                       int gw, int gh, uint32_t seed);

/// The tribal-conflict stage — where the creeds start DRIVING.
///
/// Walks cradle pairs in index order; a culture whose aggression clears the
/// conquest cost marches on its nearest neighbour, and a won war WELDS the
/// two cradles: @p hl.fragmentation_q falls (bounded — it can never fall
/// below half its incoming value, so warlike creeds cannot weld a fragmented
/// world into a hegemon by themselves). Must run BEFORE
/// nation_params_from_ladder so the welding reaches the political map.
///
/// Appends its war lines to @p cs.history.
void record_tribal_conflict(creed_state& cs,
                            history_ladder_state& hl,
                            uint32_t seed);

/// The globalisation event that closes generation: a common trade tongue
/// spreads through every realm. From this point the record is rendered in the
/// player's language; the gods keep their native names. Appends one line.
///
/// Counts the surviving realms itself (the same sorted walk
/// record_institutional_history uses), so the caller passes the world rather
/// than re-deriving a number two functions already agree on.
void record_globalisation(creed_state& cs, const world& w, entity_id body_id);
