// War (BL-1459 war record and declaration, BL-1428 war goals and peace) —
// docs/politics/RELATIONS.md § 7 is the authority; § 1 (Stance) owns the
// hostility rows a war writes and clears.
//
// A WAR IS A RECORD THAT A DECLARATION OPENS. Hostility (`corp_hostile_pairs`,
// stance.hpp) stays the gate every consumer reads — interdiction, the march
// queue, the battle trigger. The war gives that hostility a purpose (goals), a
// measure (the war score and each side's exhaustion) and an end (a peace).
//
// A war is keyed by its two LEADERS, (declarer, defender). Other nations JOIN a
// side (BL-1706, wars draw in allies; RELATIONS.md § Joining a war): every member
// of a side is at war with every member of the other, and a pair stands across in
// at most one war. A nation may be party to several wars at once.
//
// THE TYPES HERE ARE WORLD-FREE (the sentiment.hpp precedent): `world` holds the
// tables and this header names their meaning, so world.hpp can include it
// without a cycle. The verbs take a `world&` through a forward declaration.
//
// DETERMINISM. Every table is a `std::map` (or a vector kept in append order),
// so every walk is a sorted walk over ids. Goals are stored ascending. No
// randomness is drawn anywhere in this file.
//
// THE INVARIANT HOLDS (RELATIONS.md § The invariant). Sentiment PRICES a
// declaration here — the justifications read Trust, and the price is charged as
// conduct — but nothing in this file ever declares on sentiment's behalf. Every
// mutation is one of the named verbs, called by a player, a wire agent, or a
// rival's scorer under its grant (AI_OPPONENT.md § 11).

#pragma once

#include "condition_set.hpp" // condition_comparator — the justification rows' grammar
#include "entity.hpp"
#include "sentiment.hpp"     // sentiment_params, sentiment_factor_kind

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

struct world;
struct battle_tick;

// ---------------------------------------------------------------------------
// Authored constants (calibration — Lane W's first cut, sprint 61)
// ---------------------------------------------------------------------------

/// Most goals a declaration or an answer may name (RELATIONS.md § 7: one to three).
inline constexpr std::size_t k_war_goals_max = 3;

/// A truce bars either party declaring on the other for this many economy ticks
/// (Ben, 2026-10-30: sixteen ticks, four years).
inline constexpr int k_war_truce_ticks = 48; ///< Months: four years, the 16 quarters (BL-1642).

/// `war_declared`'s magnitude multiplier for a nation bordering either party
/// (RELATIONS.md § 7: "watches twice as hard").
inline constexpr float k_war_border_multiplier = 2.0f;

/// `war_declared`'s magnitude rises by this fraction of the base per goal past the
/// first ("greed is watched"): 1 goal x1.0, 2 goals x1.5, 3 goals x2.0.
inline constexpr float k_war_per_extra_goal = 0.5f;

/// Any justification that holds multiplies the third-party price by this
/// (Ben, 2026-10-30: halves it).
inline constexpr float k_war_justified_multiplier = 0.5f;

/// Exhaustion a party gains for losing its WHOLE standing force in battle: the
/// increment is this x men lost / (men standing + men lost). Scale-free, so a
/// small nation and a large one tire at the same rate for the same proportion.
inline constexpr float k_war_exhaustion_loss_full = 60.0f;

/// Exhaustion a party gains per day with its WHOLE force in the field (on the
/// enemy's ground or in a battle with it); scaled by the fraction in the field.
/// A whole army abroad for a quarter (90 days) gains 9.
inline constexpr float k_war_exhaustion_per_field_day = 0.1f;

/// Exhaustion a party gains per tick (a month) for each of its own provinces the enemy
/// holds: a third of the quarter's 2.0 (BL-1642).
inline constexpr float k_war_exhaustion_per_held_province = 2.0f / 3.0f;

/// Fraction of its remaining exhaustion a nation at peace (in no open war) sheds
/// each tick (a month): 1 - 0.75^(1/3), the quarter's 0.25 restated per month, so the
/// ~2.4-quarter (~7.2-month) half-life holds (BL-1642).
inline constexpr float k_war_exhaustion_decay = 0.0914397f;

/// A residual exhaustion below this snaps to zero and its row is erased.
inline constexpr float k_war_exhaustion_epsilon = 0.5f;

/// A refused peace offer may not be repeated by its offerer, in that war, for this
/// many economy ticks (Ben, 2026-11-03, NR-1120: four ticks, one year). The refusal
/// tick is saved (`world::peace_refusals`, world_save_version 75).
inline constexpr int k_war_offer_cooldown_ticks = 12; ///< Months: a year, the 4 quarters (BL-1642).

/// Morale's ceiling falls by this many per-mille per point of exhaustion: at 100
/// exhaustion no unit of the nation may stand above 500 morale.
inline constexpr int k_war_morale_per_exhaustion = 5;

// ---------------------------------------------------------------------------
// The record
// ---------------------------------------------------------------------------

/// A war's key in `world::wars`: (declarer, defender) — its two LEADERS.
using war_key_t = std::pair<entity_id, entity_id>;

/// BL-1706 (wars draw in allies; RELATIONS.md § Joining a war): a nation that joined
/// a side. Leaders stay in `war_record`'s own fields.
struct war_member
{
    entity_id             nation = null_entity;
    uint8_t               side   = 0;   ///< 0 the declarer's side, 1 the defender's.
    std::vector<uint32_t> goals;        ///< Ascending; provinces of members across (0..3 once pruned).
    int64_t               men_lost   = 0;
    float                 exhaustion = 0.0f;
    float                 field_days = 0.0f;
    int32_t               joined_tick = 0;
};

/// One open war. Keyed (declarer, defender) in `world::wars`.
struct war_record
{
    entity_id declarer = null_entity;
    entity_id defender = null_entity;

    /// The declarer's goals: 1..3 of the defender's provinces, ascending id.
    std::vector<uint32_t> declarer_goals;
    /// The defender's goals: empty until it answers, then 1..3 of the declarer's
    /// provinces, ascending id.
    std::vector<uint32_t> defender_goals;
    /// True once the defender has answered (named its goals). Its hostility row
    /// stands from the declaration either way (Ben, 2026-11-09).
    bool answered = false;

    int32_t opened_tick = 0; ///< `world::current_econ_tick` at the declaration.
    int32_t opened_day  = 0; ///< `world::construction_quarter_day` at the declaration.

    /// WAR SCORE INPUTS — the battles half: men each side lost in battles between
    /// the pair over the war. The occupation half reads `province_holder` live.
    int64_t declarer_men_lost = 0;
    int64_t defender_men_lost = 0;

    /// Each party's exhaustion from this war, [0, 100].
    float declarer_exhaustion = 0.0f;
    float defender_exhaustion = 0.0f;

    /// Field days this quarter, per party: the share of its standing units in the
    /// field (on the enemy's ground or fighting it), summed over the days walked
    /// (`accumulate_war_field_days`). Charged into exhaustion and reset by
    /// `run_war_quarter`.
    float declarer_field_days = 0.0f;
    float defender_field_days = 0.0f;

    /// BL-1706: the joiners, in join order (ascending `joined_tick`, then append).
    std::vector<war_member> joiners;
    /// BL-1706: men lost by members who have since left, per side — kept so the
    /// battles half still counts the whole war.
    std::array<int64_t, 2> departed_men_lost{};
};

/// BL-1706: a pending request to join a war (`world::join_requests`, keyed (war key,
/// asker)). One per nation per war.
struct join_request
{
    uint8_t               side = 0;
    std::vector<uint32_t> goals; ///< Ascending.
    int32_t               tick = 0;
};
using war_join_key_t = std::pair<war_key_t, entity_id>; ///< (war key, asker)

/// What a peace hands over. White: nothing. Cession: `provinces` pass from the
/// loser to `gainer`.
enum class peace_kind : uint8_t
{
    white    = 0,
    cession  = 1,
};
inline constexpr uint8_t peace_kind_max = 1;

struct peace_terms
{
    peace_kind            kind      = peace_kind::white;
    entity_id             gainer    = null_entity; ///< Cession only: who receives the provinces.
    std::vector<uint32_t> provinces;               ///< Cession only: ascending, 1..3, the gainer's goals it holds.
};

/// The one pending offer a war may carry (`world::peace_offers`, keyed by the war's key).
struct peace_offer
{
    entity_id   offerer = null_entity;
    peace_terms terms;
    int32_t     offered_tick = 0;
};

/// How a war ended, for the closed-war history.
enum class war_end : uint8_t
{
    white_peace   = 0,
    cession       = 1,
    imposed_white = 2,
    imposed_cession = 3,
    capitulation    = 4, ///< BL-1706: a leader capitulated (RELATIONS.md § 7, Capitulation).
};
inline constexpr uint8_t war_end_max = 4;

/// A closed war (`world::war_history`, append order = close order).
struct war_history_entry
{
    entity_id             declarer = null_entity;
    entity_id             defender = null_entity;
    int32_t               opened_tick = 0;
    int32_t               closed_tick = 0;
    war_end               end = war_end::white_peace;
    entity_id             gainer = null_entity;   ///< Cession only.
    std::vector<uint32_t> ceded;                  ///< Cession only, ascending.
    int32_t               final_score = 0;        ///< `war_score` at the close.
};

/// One province ceded at a peace (`world::province_cessions`, append order).
/// The LOST GROUND justification reads it: a goal the declarer once ceded to the
/// defender.
struct province_cession
{
    entity_id from     = null_entity;
    entity_id to       = null_entity;
    uint32_t  province = 0;
    int32_t   tick     = 0;
};

// ---------------------------------------------------------------------------
// Results
// ---------------------------------------------------------------------------

/// Only `applied` mutates the world; every rejection mutates NOTHING.
enum class war_result : uint8_t
{
    applied = 0,
    rejected_invalid, ///< Unknown or equal nations, a bad goal list, bad terms.
    rejected_state,   ///< Valid arguments the current state refuses (a war already
                      ///< stands, a truce, no such war, an offer already pending,
                      ///< a score short of 100, a goal not held...).
};

// ---------------------------------------------------------------------------
// Justifications — condition rows (META_LAYER.md's grammar at the pair grain)
// ---------------------------------------------------------------------------
// Each is a ROW: a measure, a comparator and an operand. A new justification is a
// row in `war_justification_rows()`, never a code branch in the evaluator. The
// measures are PAIR measures — (declarer, defender, goals) — which the
// single-subject `condition_subject` vocabulary cannot express (a condition there
// is measured against one corp); see the report's design question on promoting
// them into `condition_subject` with a counterparty qualifier.

enum class war_measure : uint8_t
{
    declarer_trust_toward_defender = 0, ///< sentiment Trust, declarer -> defender.
    defender_embargoes_declarer,        ///< 1 if the defender embargoes the declarer (BL-1418, `embargoes`), else 0.
    goals_ceded_by_declarer,            ///< Goals the declarer ceded to the defender at an earlier peace.
    defender_wars_on_declarer_friends,  ///< Wars the defender DECLARED on a friend of the declarer.
};

/// Where a row's operand comes from: authored on the row. (Grievance once read the
/// adverse band's edge, -band_edge_far x limit = -50; Ben, 2026-11-03, NR-1104, moved
/// it to the hostile-leaning band, `k_war_grievance_trust`, which is no band edge.)
enum class war_operand : uint8_t
{
    fixed = 0,
};

/// The Trust at or below which GRIEVANCE holds - the declarer's Trust toward the
/// defender in the hostile-leaning band (RELATIONS.md § 7; Ben, 2026-11-03, NR-1104).
/// -15, not -25 (Ben, 2026-11-03, NR-1129): with the doubled Trust weights and the
/// 18-tick Trust decay a sustained offence reaches -15; nothing reached -25.
inline constexpr float k_war_grievance_trust = -15.0f;

struct war_justification_row
{
    const char*          name;
    war_measure          measure;
    condition_comparator comparator;
    war_operand          operand_source;
    float                operand;
};

/// The opening set (RELATIONS.md § 7), in display order: grievance, embargo, lost
/// ground, a friend attacked.
inline constexpr std::size_t k_war_justification_count = 4;
const std::array<war_justification_row, k_war_justification_count>& war_justification_rows();

// ---------------------------------------------------------------------------
// The declaration price — a pure preview, and exactly what declare_war charges
// ---------------------------------------------------------------------------

/// One conduct event the declaration would emit, with the delta it moves.
struct war_price_row
{
    entity_id             observer  = null_entity;
    sentiment_factor_kind kind      = sentiment_factor_kind::war_declared;
    float                 magnitude = 0.0f;
    float                 access    = 0.0f; ///< weight.access x magnitude
    float                 trust     = 0.0f; ///< weight.trust x magnitude
    float                 alarm     = 0.0f; ///< weight.alarm x magnitude x the declarer's alarm_toward scale
    bool                  borders   = false; ///< war_declared: the observer borders a party.
};

struct war_price
{
    bool valid = false; ///< False when the declaration would be refused (rows then empty).
    std::array<bool, k_war_justification_count> justifications{}; ///< Which rows hold.
    bool  justified = false; ///< Any of them.
    std::vector<war_price_row> rows; ///< Ascending observer; hostility_declared first.
    float sum_access = 0.0f;
    float sum_trust  = 0.0f;
    float sum_alarm  = 0.0f;
    /// False under the settle (`sentiment_conduct_live`): the conduct would be
    /// dropped, so every delta above reads 0 — the preview is the charge.
    bool  charged    = true;
};

// ---------------------------------------------------------------------------
// Reads
// ---------------------------------------------------------------------------

/// The political owner of a province: the plurality of `tile_to_nation` over its
/// tiles, ties to the lower nation id. `null_entity` for an unknown province or
/// one with no claimed tile. (Not `province_holder`, the military holder.)
entity_id province_owner(const world& w, uint32_t province_id);

/// The province a war goal for an attack on @p tile names (RELATIONS.md § 7, the
/// right-click declare-and-attack): the tile's province when @p defender owns it;
/// otherwise the defender's capital province (the attacked stack stands on ground
/// that is not the defender's). 0 when neither names one of the defender's provinces.
uint32_t attack_goal_province(const world& w, entity_id defender, entity_id tile);

/// The open war in which @p a and @p b stand on opposite sides (leaders or joiners;
/// BL-1706), or nullptr. A pair stands across in at most one war.
const war_record* war_between(const world& w, entity_id a, entity_id b);
bool at_war(const world& w, entity_id a, entity_id b);

// --- BL-1706 (wars draw in allies; RELATIONS.md § Joining a war): sides, pure ---

/// @p nation's side in @p war: 0 the declarer's, 1 the defender's, -1 not a member.
int war_side(const war_record& war, entity_id nation);
/// The leader of @p side (0 the declarer, 1 the defender).
entity_id war_leader(const war_record& war, int side);
/// Every member of @p side: its leader, then its joiners in join order.
std::vector<entity_id> war_side_members(const war_record& war, int side);
/// The goals @p side named: its leader's, then each joiner's, ascending and distinct.
std::vector<uint32_t> war_side_goals(const war_record& war, int side);
/// The member of @p side that named @p province (the leader first, then join order),
/// or null_entity.
entity_id war_goal_namer(const war_record& war, int side, uint32_t province);
/// The goals @p member named (its leader list or its joiner entry), or nullptr.
const std::vector<uint32_t>* war_member_goals(const war_record& war, entity_id member);
/// Men lost by @p side over the war: its members' and its departed members'.
int64_t war_side_men_lost(const war_record& war, int side);
/// The war (and its key) in which @p nation is a member opposite @p across, or nullptr.
const war_record* war_of_member(const world& w, entity_id nation, entity_id across,
                                war_key_t* key = nullptr);

/// Pure: every refusal `request_join_war` makes, in its order. @p allow_pending
/// skips "a request already pending" (accept re-validates the request itself).
war_result check_join(const world& w, const war_key_t& key, entity_id asker, uint8_t side,
                      const std::vector<uint32_t>& goals, bool allow_pending = false);
/// True iff @p asker's last request to join the war at @p key was refused fewer
/// than `k_war_offer_cooldown_ticks` ago.
bool join_cooling(const world& w, const war_key_t& key, entity_id asker);
/// The price of joining (pure): exactly what `accept_join` charges — the declaration's
/// price for declaring on the opposing leader, with hostility_declared on EVERY member
/// across. `valid` false when the join would be refused.
war_price join_price(const world& w, const sentiment_params& p, const war_key_t& key,
                     entity_id asker, uint8_t side, const std::vector<uint32_t>& goals);

/// True iff a truce stands between the pair at `w.current_econ_tick`.
bool truce_between(const world& w, entity_id a, entity_id b);

/// Which justification rows hold for this declaration (pure).
std::array<bool, k_war_justification_count>
war_justifications(const world& w, const sentiment_params& p, entity_id declarer,
                   entity_id defender, const std::vector<uint32_t>& goals);

/// The price preview (pure). `valid` false, rows empty, when `declare_war` would refuse.
war_price declaration_price(const world& w, const sentiment_params& p, entity_id declarer,
                            entity_id defender, const std::vector<uint32_t>& goals);

/// The declarer's war score in [-100, 100] (RELATIONS.md § 7): 50 x occupation
/// (the tile-weighted share of its own goals the declarer holds, less the share of
/// its own answered goals the defender holds; NR-1103)
/// + 50 x battles (men lost by the defender less by the declarer, over both),
/// rounded to the nearest integer. 0 for an unknown war.
int war_score(const world& w, const war_record& war);

/// A nation's war exhaustion: the larger of its residual and its exhaustion in
/// every open war. [0, 100].
float nation_war_exhaustion(const world& w, entity_id nation);

/// The morale ceiling exhaustion imposes on @p nation's units, per-mille.
int war_morale_ceiling(const world& w, entity_id nation);

// ---------------------------------------------------------------------------
// Verbs — each validates everything first; a rejection mutates nothing
// ---------------------------------------------------------------------------

/// Opens a war: writes BOTH hostility rows, declarer -> defender and defender ->
/// declarer (Ben, 2026-11-09, RELATIONS.md § 7: a declaration of war makes both sides
/// hostile; stance's internals — friendship and offers dissolve) and the record, and charges exactly
/// `declaration_price`. The charge is GATHERED (`queue_conduct`, BL-1458) and lands
/// at the tick's one sentiment fold (`fold_pending_sentiment`), with every other
/// conduct of the tick. Refused on: declarer == defender or an unknown nation,
/// 0 or > 3 goals, a repeated goal, a goal not the defender's province
/// (rejected_invalid); a war already standing between the pair either way, or a
/// standing truce (rejected_state). `goals` may arrive in any order.
war_result declare_war(world& w, const sentiment_params& p, entity_id declarer,
                       entity_id defender, const std::vector<uint32_t>& goals);

/// The defender's answer — free: names 1..3 goals among the declarer's provinces. Its
/// hostility row already stands (the declaration wrote both, Ben 2026-11-09). Refused on
/// a bad goal list (rejected_invalid) or when no unanswered war (declarer, defender)
/// stands (rejected_state).
war_result answer_war(world& w, entity_id defender, entity_id declarer,
                      const std::vector<uint32_t>& goals);

/// Pure: whether @p terms are a legal peace for the war between @p a and @p b.
war_result check_peace_terms(const world& w, const war_record& war, const peace_terms& terms);

/// True iff @p offerer's last offer to @p other was refused fewer than
/// `k_war_offer_cooldown_ticks` ticks ago, so `offer_peace` would refuse it (pure).
bool peace_offer_cooling(const world& w, entity_id offerer, entity_id other);

/// Files @p offerer's terms on the war with @p other. One pending offer per war, and
/// none while the offerer's last refused offer in it is cooling (rejected_state).
war_result offer_peace(world& w, entity_id offerer, entity_id other, const peace_terms& terms);

/// @p receiver accepts the pending offer @p offerer made: the peace is concluded
/// on its terms (re-validated now; refused, mutating nothing, if they no longer hold).
/// At ANY peace, every goal of the war that one party holds against the other and
/// that was not ceded returns to its owner, and the occupier's units on that ground
/// withdraw by the retreat rule; a third nation's hold is never touched.
war_result accept_peace(world& w, const sentiment_params& p, entity_id receiver, entity_id offerer);

/// @p receiver refuses the pending offer @p offerer made; the offer is erased and the
/// refusal tick is recorded (`world::peace_refusals`, keyed (offerer, receiver)), so the
/// offerer may not offer again in this war for `k_war_offer_cooldown_ticks`.
war_result refuse_peace(world& w, entity_id receiver, entity_id offerer);

/// @p imposer concludes the peace on its own terms, without acceptance — only
/// when the war score stands at 100 in its favour.
war_result impose_peace(world& w, const sentiment_params& p, entity_id imposer,
                        entity_id other, const peace_terms& terms);

// --- BL-1706 (wars draw in allies; RELATIONS.md § Joining a war) ---------------
// The peace verbs above also take a SEPARATE PEACE between a joiner and the OTHER
// side's leader (offers in `world::separate_peace_offers`, keyed (joiner, leader);
// terms white, or a cession of goals the gainer named on the other's ground that its
// side holds): it takes the joiner alone out of the war. Imposing is the leaders' only.

/// Files @p asker's request to join the war at @p key on @p side, naming 1..3 goals
/// among the provinces of the other side's members. Refused (mutating nothing):
/// rejected_invalid — unknown nation, side > 1, a bad goal list; rejected_state — no
/// such war, already a member, at war with any member (either side), a truce with a
/// member across, a goal its side already named, a request pending, a refusal cooling.
war_result request_join_war(world& w, entity_id asker, const war_key_t& key, uint8_t side,
                            const std::vector<uint32_t>& goals);

/// @p leader accepts @p asker's pending request on its side (the first in war-key
/// order, if several): hostility both ways with every member across, the join's price
/// charged as conduct (`join_price`), the joiner appended. Re-validated now; refused,
/// mutating nothing, if it no longer holds.
war_result accept_join(world& w, const sentiment_params& p, entity_id leader, entity_id asker);

/// @p leader refuses @p asker's pending request; the refusal tick is recorded
/// (`world::join_refusals`) and @p asker may not ask again in that war for
/// `k_war_offer_cooldown_ticks`.
war_result refuse_join(world& w, entity_id leader, entity_id asker);

/// RELATIONS.md § 7, Capitulation (Ben, 2026-11-19): unilateral. Every goal the other
/// side named on @p nation's ground, held or not, passes to the member that named it
/// (`cede_provinces`, `province_ceded` conduct). A leader's capitulation closes the war
/// (`war_end::capitulation`), its side's joiners leaving with a white peace; a joiner's
/// takes it alone out. Truces as any peace; a last province ceded ends the nation.
war_result capitulate(world& w, const sentiment_params& p, entity_id nation, const war_key_t& key);

/// A CESSION MOVES THE TERRITORY (BL-1460; RELATIONS.md § 7). For each province
/// (ascending): `province_holder` becomes @p to (already so for a legal cession)
/// and the cession is recorded in `world::province_cessions` (the LOST GROUND
/// record). Then every tile of those provinces that @p from owns passes to @p to
/// in `tile_to_nation` (through `set_tile_nation`, so the catchment and front stamps
/// move) and in both nations' tile lists and deposit profiles; every building of
/// @p from's on that ground joins @p to's `state_assets`, in its stored order; its
/// track follows; a muster @p from was raising at a base that passed is dropped, and
/// a wing of @p from's based there rebases to @p from's nearest own airbase (hex
/// distance, ties to the lower id) or, with none left, is disbanded. Fronts are
/// re-anchored FIRST, against the map before it moves, so a cession right after a
/// load remaps front keys exactly as an unbroken run does. A
/// ceded capital moves to @p from's remaining province with the largest population
/// centre (ties to the lower province id) and its free logistics hub moves with it
/// (NR-1102). The logistics caches drop; @p from's units on the ceded ground
/// withdraw by the retreat rule (`withdraw_units`); fronts re-anchor. Population
/// centres and the jurisdiction of law read `tile_to_nation` live, so they follow
/// with it. Called by a concluded cession (which may take the loser's last province,
/// Ben 2026-11-09) and by `eliminate_nation` for the ended nation's residual ground.
void cede_provinces(world& w, entity_id from, entity_id to,
                    const std::vector<uint32_t>& provinces);

/// True iff @p nation is a known nation that has ENDED (NATIONS.md § Elimination).
bool nation_ended(const world& w, entity_id nation);

/// ELIMINATION (BL-1562, NATIONS.md § Elimination; Ben 2026-11-09). Run once, by a
/// concluded cession that left @p ended owning no land province, after the ground moved;
/// @p gainer took that last province (@p ceded: the provinces that peace ceded). In
/// order: every province @p ended still OWNS (its coastal water) passes to @p gainer by
/// `cede_provinces`; a tile it owns inside a province another nation owns passes alone
/// (owner, tile lists, deposits, track), with no holder write and no cession record;
/// its buildings and its space assets pass to @p gainer; its other wars close as white
/// peace with NO truce; its peace and friendship offers drop; hostility, friendship,
/// embargo rows, truces, refusals and sentiment rows to and from it are removed (and its
/// queued conduct), and the laws it enacted (its tariffs) go; its units, wings, musters,
/// front orders and battles end, its convoys and trades are cancelled; a land unit of a
/// nation not at war with @p gainer left on the provinces @p gainer just gained
/// (@p ceded and the residual ones) withdraws by the retreat rule; a positive treasury
/// passes to @p gainer, BOOKED on its next return as a refund-style receipt (so a
/// return's net still equals its balance change), and a debt is written off; it is
/// flagged `ended`, its live capital cleared (the generation capital stays its
/// identity); the history log writes one entry. A no-op for an unknown or already-ended
/// nation.
void eliminate_nation(world& w, const sentiment_params& p, entity_id ended, entity_id gainer,
                      const std::vector<uint32_t>& ceded = {});

// ---------------------------------------------------------------------------
// Passes
// ---------------------------------------------------------------------------

/// The daily hook (run_military_day, after the battles): each battle dispatch
/// between two war parties adds its men lost to the war's battle inputs and to
/// each side's exhaustion; then exhaustion's morale ceiling is applied to the
/// units of every nation at war. A no-op with no wars.
void note_war_battles(world& w, const battle_tick& battles);

/// One exchange of losses between two nations, outside a battle dispatch (BL-1643: a
/// battery's shells). `a` lost `a_lost` men and `b` lost `b_lost`.
struct war_losses
{
    entity_id a = null_entity, b = null_entity;
    int a_lost = 0, b_lost = 0;
};

/// The battle inputs' path for losses outside a battle (BL-1643, MILITARY.md § Artillery
/// fires shells: a strike's losses count as a battle's toward the war): each exchange
/// between two war parties adds its men lost to the war's battle inputs and to each side's
/// exhaustion, exactly as `note_war_battles` adds a dispatch's; then the morale ceiling. A
/// no-op with no wars.
void note_war_losses(world& w, const std::vector<war_losses>& losses);

/// The daily accumulation (advance_quarter_to): adds @p days x each party's share
/// of its standing units in the field to the war's field-day accumulator. Called
/// once per military day walked, and once with the REST of the quarter's days when
/// the walk stops early because nothing military moves (the positions then hold
/// for the remainder, so the share is the same every one of those days).
void accumulate_war_field_days(world& w, int days);

/// The quarterly pass (run_economy_step): field days and held provinces raise
/// each open war's exhaustion; a nation in no open war sheds its residual;
/// expired truces are erased; the morale ceiling is applied. A no-op with no
/// wars, no residual and no truces.
void run_war_quarter(world& w);

/// Clamp each unit of a nation with exhaustion to the morale ceiling (and its
/// readiness to its morale). Shared by the passes above and out-of-contact recovery.
void apply_war_morale_ceiling(world& w);
