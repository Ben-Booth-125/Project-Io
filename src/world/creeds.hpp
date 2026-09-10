#pragma once

// ---------------------------------------------------------------------------
// Creeds — one pantheon per cradle-culture, in that culture's own tongue
// (BL-235; docs/lore/CREEDS.md).
//
// Runs between the pre-national history ladder (BL-221) and nation generation.
// Each agrarian cradle becomes a CULTURE: it rolls its own phonology (a small
// consonant/vowel inventory), names itself and its gods in that tongue, and
// derives its pantheon's shape from the land it sits on — a coastal cradle
// raises a sea god, an ore region a forge god, the charter cradle an oath
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

    // --- Descent (BL-865) --------------------------------------------------
    //
    // WHAT THIS IS FOR, and it is the empire phase rather than this one.
    // `CIVILISATION.md` § Culture relations makes KINSHIP the substrate for how
    // alike two peoples are: they are similar because they share an ancestor and
    // parted recently, which is a fact about the world's history rather than a
    // number assigned to it. Without descent, the empire phase would have to
    // invent similarity from nothing — exactly the rolled-rather-than-earned
    // quantity this layer keeps refusing.
    //
    // THE MIGRATION ALREADY BUILT THIS TREE AND THREW IT AWAY. BL-856 coins each
    // daughter from a named parent and `colonisation_field::spawns` records every
    // edge, but the field died at the end of `run_settlement` and nothing here
    // held the link. These two members are the whole repair.

    /// The culture this one descends from, or -1 for a cradle culture. Indices
    /// are into the same `creed_state::cultures` vector, and a daughter's parent
    /// is ALWAYS at a lower index than the daughter — ids are handed out in
    /// arrival order — so a walk toward the root strictly decreases and cannot
    /// loop.
    int parent = -1;

    /// The `farm_class` this people was coined on, as a plain integer so
    /// `creeds.hpp` need not take a dependency on `colonisation.hpp` for one
    /// field (this header has hundreds of includers). -1 where unknown.
    ///
    /// Its use is the OPPOSITION half of culture relations, which kinship alone
    /// cannot supply: a people of the floodplain and a people of the highlands
    /// want different ground and live differently, which is a material
    /// disagreement rather than a stated one (BL-864 made a daughter a people OF
    /// the country it settled).
    int8_t origin_farm_class = -1;

    // --- Coining year (BL-870) ---------------------------------------------
    //
    // THE OTHER HALF OF THE KINSHIP MEASURE. `parent` says WHO a culture
    // descends from; this says WHEN it came to be its own people — a cradle
    // culture at the migration's start year, a daughter at the calendar year
    // its stream diverged (BL-856's flood already dates every tile it claims;
    // this is that date, kept). NR-816 settled kinship as YEARS SINCE THE
    // COMMON ANCESTOR rather than hop count, and a hop count is all `parent`
    // alone can give — two cultures nine hops apart could be four thousand
    // years or four hundred apart depending how long each hop took, and the
    // difference is exactly what CIVILISATION.md means by "parted recently".
    //
    // -1 where unknown, matching `parent` and `origin_farm_class`'s sentinel.
    int64_t coined_year = -1;
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

// ---------------------------------------------------------------------------
// Culture relations (BL-870; CIVILISATION.md § Culture relations)
// ---------------------------------------------------------------------------

/// Years between @p a and @p b's most recent common ancestor and the YOUNGER
/// of the two — NR-816's measure of kinship, chosen over a hop count because
/// two cultures nine `parent` hops apart may have parted four centuries ago or
/// four thousand years ago, and only the calendar distinguishes them.
///
/// Walks both `parent` chains toward the root. BOUNDED AND CANNOT LOOP: ids
/// are handed out in arrival order, so a parent is always lower-indexed than
/// its child (BL-865) — the same fact `colonisation_harness::case_family_tree`
/// already exercises. Returns -1 if either index is out of range, or if the
/// ancestor or either culture's `coined_year` is unknown (-1) — an
/// unmeasurable pair, not a zero-year one.
int64_t culture_kinship_years(const std::vector<culture>& cultures, int a, int b);

/// Opposition between cultures @p a and @p b, 0-1000. SYMMETRIC (NR-815) — a
/// value over an unordered pair, not a directed relation like `grudge` (which
/// answers a different question: who wronged whom, at a place and date).
///
/// Combines the two axes CIVILISATION.md names for opposition — the war god's
/// own TEMPERAMENT (`culture_god::zeal`/`dominion` on `pantheon[1]`, the god
/// every creed raises) and the COUNTRY each was coined on
/// (`culture::origin_farm_class`) — then discounts the result by how recently
/// the two share an ancestor (`culture_kinship_years`): a people that split
/// off a few centuries ago reads as the same people even where it has since
/// settled different ground, and the discount fades toward the full weight of
/// the two axes as the shared ancestor recedes into the past or is unknown.
///
/// PERMITS conquest, does not score it (Ben, 2026-09-09): this is a queryable
/// value for a caller's OWN weight to read — `run_history_sim`'s campaign
/// scorer feeds it into `history_sim_params::w_cult` per attacker/target pair
/// — never a second scorer term of its own. Returns 0 for the same culture or
/// an out-of-range index.
int culture_opposition_q(const std::vector<culture>& cultures, int a, int b);
