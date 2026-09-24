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
// THE CREED DRIVES, IT DOES NOT NARRATE (the BL-221 rule, inherited), but not
// through war any more (BL-852, resolving NR-808;
// docs/generation/COLONISATION.md § Fragmentation comes from contact). The
// tribal marches retired: fragmentation is now read off how far two peoples'
// settled shares interpenetrate (`record_cultural_contact`), and a world
// whose cultures never met stays as fragmented as its terrain alone made it.
// `aggression_q`, below, survives unchanged as the temperament reading the
// Era -1 sim consumes as doctrine — it prices how a polity fights once it
// exists, and stops setting the nation count.
// ---------------------------------------------------------------------------

#include "history_ladder.hpp"
#include "planetology.hpp"
#include "tongue.hpp"

#include <cstdint>
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
    /// pantheon's zeal. Survives BL-852 unchanged as the Era -1 sim's
    /// DOCTRINE input (`history_sim.cpp` reads it per attacker/defender); it
    /// no longer sets the nation count — `record_cultural_contact` does.
    int aggression_q = 0;

    /// 0-1000 — how readily this people can FEED a force it has put across
    /// water (BL-899; docs/lore/CREEDS.md § Sea legs). Read at the Era -1 sim's
    /// SUPPLY calculation, never added to a campaign score: it is a property of
    /// the people standing on the staging ground, not a term inside an actor.
    ///
    /// EARNED FROM FACTS, NEVER ROLLED, and derived in exactly the place and
    /// exactly the manner `aggression_q` is — one line off the finished
    /// pantheon, in `build_creeds`, with no RNG of its own. Two of the three
    /// upstream facts CREEDS.md names are reachable here:
    ///
    ///   - A COASTAL CRADLE — `agrarian_cradle::coastal`, the same fact that
    ///     decides whether this pantheon raises a sea god at all. It is the
    ///     larger share, because beginning on the water is what puts boats in a
    ///     people's hands before anybody asks them to fight from one.
    ///   - A SEA OR STORM GOD — read through `culture_god::domain`, the same
    ///     channel `aggression_q` reads the war god through. Zeal weighs twice
    ///     dominion, as it does there: how hard a people throws itself at the
    ///     water matters more than how sure it is of winning.
    ///
    /// THE THIRD FACT — a crossing in the people's own migration (BL-901) —
    /// now survives the handoff: `culture_spawn::crossed_water` is set at the
    /// crude overseas hop's spawn site in `colonisation.cpp`, and
    /// `derive_daughter_culture` reads it to add a THIRD term to the sum
    /// below, on top of the two inherited from the cradle. It is the best
    /// claim of the three to earn sea legs — a coastal cradle says only where
    /// a people started and a sea god says only what they believe, but a
    /// crossing says what they actually did.
    ///
    /// A daughter inherits its parent's pantheon and cradle whole
    /// (`derive_daughter_culture`), so the coastal-cradle and sea-god terms
    /// carry over unchanged; only the crossing term can differ daughter to
    /// daughter, and only a daughter coined at a hop's landing site ever
    /// carries it.
    int sea_legs_q = 0;

    /// Weight of the crossing term in `sea_legs_q` (BL-901) — set once here so
    /// `derive_daughter_culture` and any harness reading the distribution
    /// share the one number. Sized between the coastal term (400) and the
    /// sea/storm god's ceiling (zeal 10 * 40 + dominion 10 * 20 = 600):
    /// large enough to move the distribution, never so large it swamps the
    /// two terms BL-899 already earned.
    static constexpr int sea_legs_crossing_bonus = 300;

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
    ///
    /// THE LIVING TREE after the boundary fold (BL-1017, below): a daughter
    /// whose mother folded is re-parented onto the nearest ancestor that kept
    /// its name, so this never names a folded culture. The as-coined link is
    /// `coined_from`.
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

    /// The calendar year this culture was coined — a cradle culture at the span's
    /// start (`colonisation_start_year`), a daughter at the year its stream
    /// diverged. NR-816 makes kinship a YEARS-SINCE-COMMON-ANCESTOR measure, and
    /// that measure is worthless without the years themselves (BL-873). Read by
    /// `culture_kinship_years` (BL-870) below.
    int64_t coined_year = INT64_MIN;

    // --- The boundary fold (BL-1017) -----------------------------------------
    //
    // Ben, 2026-09-16 (NR-879, option A): A CULTURE HOLDING NO GROUND WHEN THE
    // COLONISATION ROUND HANDS THE MAP TO THE EMPIRES ROUND FOLDS BACK INTO ITS
    // PARENT. The coining rule is untouched, so the split census and the
    // migration's own record still see every split that happened; what shrinks
    // is only the set of names that outlive the round. Measured before the
    // ruling: 5,453 coined over 16 seeds, 790 holding ground at the boundary.
    //
    // TWO LINKS, BECAUSE THEY ANSWER TWO QUESTIONS. `parent` above is THE LIVING
    // TREE — what kinship, opposition and the palette walk — and after the fold
    // it names the nearest ancestor that still carries a name. `coined_from` is
    // THE LINEAGE — who this people actually split from — and is never
    // rewritten, so a re-parented daughter still reads as a descent through the
    // name that folded. They are equal everywhere the fold changed nothing.
    //
    // IDS ARE STABLE. A folded culture keeps its row and its index; nothing that
    // stores a culture id is remapped, because at the boundary nothing names a
    // folded culture — "holds no ground" is exactly "no region's shares, no
    // founding and no scheduled founding name it" (settlement.cpp).

    /// The culture this one was COINED from, never rewritten; -1 at a cradle.
    /// Always lower-indexed than this culture, like `parent`.
    int coined_from = -1;

    /// -1 for a name somebody lives under at the boundary. Otherwise the culture
    /// whose name absorbed this one: its nearest ancestor on the `coined_from`
    /// chain that held ground. A folded culture's `parent` equals this, so no
    /// walk up `parent` from anywhere ever lands on a folded name.
    int folded_into = -1;
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

/// Fragmentation, re-derived from CONTACT rather than war (BL-852, resolving
/// NR-808; docs/generation/COLONISATION.md § Fragmentation comes from
/// contact). Retires the tribal marches: no aggression-vs-defence
/// comparison, no roll, no pairwise war between cradles.
///
/// @p region_mix_q is one entry per SETTLED region — 1000 minus that
/// region's plurality culture share (`region::culture.weight_q[0]`,
/// `settlement.hpp`), i.e. how far a second people has interpenetrated that
/// ground. The caller derives it there rather than this function taking a
/// dependency neither `creeds.hpp` nor `history_ladder.hpp` can afford:
/// `settlement.hpp` already includes both of them. Two cultures whose shares
/// mix heavily across a broad frontier pull @p hl.fragmentation_q down; two
/// that never met leave it exactly where Stage 3 (terrain + cradle count)
/// set it.
///
/// THE NON-HEGEMONY FLOOR (BL-224), RE-DERIVED: contact can never pull
/// fragmentation below half of the STRUCTURAL reading Stage 3 computed
/// before any culture existed to meet another — the same floor the retired
/// welding enforced, over the same base value, because creeds alone still
/// must not be able to manufacture a hegemon by themselves.
///
/// Pure and seedless: a deterministic function of the settled map's own
/// shares, with nothing left to roll.
void record_cultural_contact(history_ladder_state& hl,
                             const std::vector<int>& region_mix_q);

// ---------------------------------------------------------------------------
// Culture relations (BL-870; CIVILISATION.md § Culture relations)
// ---------------------------------------------------------------------------

/// Years between @p a and @p b's most recent common ancestor and the YOUNGER
/// of the two — NR-816's measure of kinship, chosen over a hop count because
/// two cultures nine `parent` hops apart may have parted four centuries ago or
/// four thousand years ago, and only the calendar distinguishes them.
///
/// THE LIVING TREE (BL-1017): it walks `parent`, which after the boundary fold
/// names only cultures that held ground, so two daughters of a mother that
/// folded date their kinship from the nearest ancestor that kept its name —
/// not from the folded mother's coining year. `coined_from` is never read.
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

// ---------------------------------------------------------------------------
// Civilisations (BL-869; CIVILISATION.md § A civilisation is what mixing
// makes, and it is not a creed)
// ---------------------------------------------------------------------------

/// The ethic a civilisation settles on — the field that makes it not-a-creed
/// (CIVILISATION.md: "a creed answers which gods there are, an ethic answers
/// how one ought to live"). Represented on the SAME two axes `culture_god`
/// carries (zeal/dominion, 0-10) rather than a fresh axis invented for the
/// occasion: those are exactly what `culture_opposition_q`'s temperament axis
/// reads, i.e. what CIVILISATION.md says the member cultures "disagreed about
/// and settled". Deliberately NOT a `culture_god` itself — no name, no
/// domain, no epithet — because an ethic has no god behind it; it is the
/// settled answer, not a new belief.
struct civilisation_ethic
{
    int zeal     = 0; ///< 0-10 — the settled relish for the martial life.
    int dominion = 0; ///< 0-10 — the settled expectation of prevailing.
};

/// A NAMED RECORD (Ben, 2026-09-09, elicitation): "with member cultures and
/// an ETHIC. Not a set of axes over the composing cultures, and not a lens on
/// the existing shares." `run_civilisation_formation` (history_sim.cpp) is
/// the only writer — it never runs at a cradle, only where two peoples have
/// shared ground in quantity for a long time (`region::mix_years`).
///
/// OUTLIVES THE POLITIES THAT FORMED IT by construction, not by a special
/// case: `region::civilisation` is a fact about the ground, set once and
/// never cleared, exactly the pattern `region::founding_culture` already
/// uses. A polity that fragments leaves every fragment still pointing at the
/// same record.
struct civilisation
{
    std::string name;             ///< Coined from the tongues that mixed.
    std::vector<int> members;     ///< Culture indices, ascending, no duplicates.
    civilisation_ethic ethic;

    /// 0-1000 — the opposition INHERITED at formation, not resolved away
    /// (NR-817: "inherits what is left as strain"). Never revised after
    /// formation: a civilisation does not settle further with age, it simply
    /// carries what it was founded on.
    int strain_q = 0;

    int64_t formed_year = INT64_MIN;
};

/// Bar above which no civilisation can form at all (NR-817: "a civilisation
/// CANNOT form across an opposition above a bar" — fracture is the formation
/// rule). Same 0-1000 scale as `culture_opposition_q`.
///
/// A MEASUREMENT, NOT A JUDGEMENT (CIVILISATION.md § Open questions: "it
/// should be set from a sweep that produces both alliance-shaped and
/// enmity-shaped worlds, never from a number picked to make one seed look
/// right"). This is a FIRST CUT pending that sweep, chosen as a middle value
/// on the same reasoning `kinship_full_weight_years` (creeds.cpp) documents
/// for its own placeholder: it should let a genuinely near-kin pair (low
/// temperament difference, same farm class) through comfortably while
/// stopping a pair that disagrees on both axes at once.
inline constexpr int civilisation_opposition_bar_q = 600;

/// Per-mille the SECOND-largest culture share must clear for a region to
/// count as carrying "two peoples in quantity" at all (CIVILISATION.md § A
/// civilisation is what mixing makes). Below this, a stray minority is not a
/// mix worth growing anything from.
inline constexpr int civilisation_mix_threshold_q = 200;

/// Years the mix above must hold CONTINUOUSLY before it counts as "for a long
/// time". A FIRST CUT alongside the opposition bar, sized as roughly a fifth
/// of the Empires phase's own 1,600-year span (CIVILISATION.md § The span is
/// 400 BCE to 1200 CE) — long enough that a civilisation cannot form in the
/// first handful of decision rounds after a border moves, short enough that
/// one can still form well before the phase ends.
inline constexpr int64_t civilisation_mix_years_bar = 300;
