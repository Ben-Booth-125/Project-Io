// War (BL-1459 war record and declaration, BL-1428 war goals and peace) —
// docs/politics/RELATIONS.md § 7 is the authority; § 1 (Stance) owns the
// hostility rows a war writes and clears.
//
// A WAR IS A RECORD THAT A DECLARATION OPENS. Hostility (`corp_hostile_pairs`,
// stance.hpp) stays the gate every consumer reads — interdiction, the march
// queue, the battle trigger. The war gives that hostility a purpose (goals), a
// measure (the war score and each side's exhaustion) and an end (a peace).
//
// There is ONE WAR PER PAIR, keyed (declarer, defender). A nation may be party
// to several wars at once; there are no alliances.
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
inline constexpr int k_war_truce_ticks = 16;

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

/// Exhaustion a party gains per quarter for each of its own provinces the enemy holds.
inline constexpr float k_war_exhaustion_per_held_province = 2.0f;

/// Fraction of its remaining exhaustion a nation at peace (in no open war) sheds
/// each quarter. ~ a 2.4-quarter half-life.
inline constexpr float k_war_exhaustion_decay = 0.25f;

/// A residual exhaustion below this snaps to zero and its row is erased.
inline constexpr float k_war_exhaustion_epsilon = 0.5f;

/// Morale's ceiling falls by this many per-mille per point of exhaustion: at 100
/// exhaustion no unit of the nation may stand above 500 morale.
inline constexpr int k_war_morale_per_exhaustion = 5;

// ---------------------------------------------------------------------------
// The record
// ---------------------------------------------------------------------------

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
    /// True once the defender has answered (its hostility row is written).
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
};

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
};
inline constexpr uint8_t war_end_max = 3;

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
                      ///< a score short of 100, the loser's last province...).
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
inline constexpr float k_war_grievance_trust = -25.0f;

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

/// The open war between @p a and @p b in either direction, or nullptr.
const war_record* war_between(const world& w, entity_id a, entity_id b);
bool at_war(const world& w, entity_id a, entity_id b);

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

/// Opens a war: writes the declarer's hostility row (stance's internals —
/// friendship and offers dissolve) and the record, and charges exactly
/// `declaration_price`. The charge is GATHERED (`queue_conduct`, BL-1458) and lands
/// at the tick's one sentiment fold (`fold_pending_sentiment`), with every other
/// conduct of the tick. Refused on: declarer == defender or an unknown nation,
/// 0 or > 3 goals, a repeated goal, a goal not the defender's province
/// (rejected_invalid); a war already standing between the pair either way, or a
/// standing truce (rejected_state). `goals` may arrive in any order.
war_result declare_war(world& w, const sentiment_params& p, entity_id declarer,
                       entity_id defender, const std::vector<uint32_t>& goals);

/// The defender's answer — free: writes its hostility row and names 1..3 goals
/// among the declarer's provinces. Refused on a bad goal list (rejected_invalid)
/// or when no unanswered war (declarer, defender) stands (rejected_state).
war_result answer_war(world& w, entity_id defender, entity_id declarer,
                      const std::vector<uint32_t>& goals);

/// Pure: whether @p terms are a legal peace for the war between @p a and @p b.
war_result check_peace_terms(const world& w, const war_record& war, const peace_terms& terms);

/// Files @p offerer's terms on the war with @p other. One pending offer per war.
war_result offer_peace(world& w, entity_id offerer, entity_id other, const peace_terms& terms);

/// @p receiver accepts the pending offer @p offerer made: the peace is concluded
/// on its terms (re-validated now; refused, mutating nothing, if they no longer hold).
/// At ANY peace, every goal of the war that one party holds against the other and
/// that was not ceded returns to its owner, and the occupier's units on that ground
/// withdraw by the retreat rule; a third nation's hold is never touched.
war_result accept_peace(world& w, const sentiment_params& p, entity_id receiver, entity_id offerer);

/// @p receiver refuses the pending offer @p offerer made; the offer is erased.
war_result refuse_peace(world& w, entity_id receiver, entity_id offerer);

/// @p imposer concludes the peace on its own terms, without acceptance — only
/// when the war score stands at 100 in its favour.
war_result impose_peace(world& w, const sentiment_params& p, entity_id imposer,
                        entity_id other, const peace_terms& terms);

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
/// with it. Called only by a concluded cession, whose terms already refused the
/// loser's last province.
void cede_provinces(world& w, entity_id from, entity_id to,
                    const std::vector<uint32_t>& provinces);

// ---------------------------------------------------------------------------
// Passes
// ---------------------------------------------------------------------------

/// The daily hook (run_military_day, after the battles): each battle dispatch
/// between two war parties adds its men lost to the war's battle inputs and to
/// each side's exhaustion; then exhaustion's morale ceiling is applied to the
/// units of every nation at war. A no-op with no wars.
void note_war_battles(world& w, const battle_tick& battles);

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
