#pragma once

#include "components.hpp" // building_type
#include "digitisation_tree_data.hpp"
#include "entity.hpp"
#include "modifier_set.hpp"

#include <bitset>
#include <cstdint>
#include <vector>

struct world;
class recipe_registry;

// ---------------------------------------------------------------------------
// Nation tech state (BL-1273) — docs/economy/RESEARCH.md is the authority
// ---------------------------------------------------------------------------
// The NATION holds technology (Ben, 2026-10-10): a held set over the current
// age's tree (the Digitisation tree, `io::dg::nodes`), a research-point pool
// and a queue. A corporation reaches technology through its nation
// (`corp_nation_holds`). Every nation opens holding nothing.
//
// WHERE IT LIVES. A world-level `std::map<entity_id, nation_tech_state>`
// (`world::nation_tech`), keyed by nation, beside `world::nation_budgets` and
// for the same reasons: a std::map walks ascending with no sort (the save leg
// and the quarterly pass), and an ABSENT entry is a nation holding nothing with
// an empty pool — so a fresh world and a fixture need no seeding.
//
// THE POOL IS THE IN-PROGRESS REMAINDER. RP flow to the head of the queue; what
// the head has not yet consumed stays in `pool`. There is no second "progress"
// counter that could disagree with the pool: the head's progress is
// `min(pool_milli, cost(head))`, and the remainder after a completion is simply what
// is left in the pool, which flows on to the next entry in the same pass.
//
// Deterministic: nations walked ascending, queue walked in stored order, every
// RP sum in integer milli-RP. Nothing here reads the clock or a RNG.
// ---------------------------------------------------------------------------

/// The research dials (RESEARCH.md § What is open: the line's conversion rate,
/// the University's RP and the RP unit are dials for the sweep).
///
/// THE CONVERSION. A nation's RP for a quarter is
///
///     base_rp_per_nation
///   + rp_per_research_credit      x credits_spent
///   + rp_per_research_credit_sqrt x sqrt(credits_spent)
///   + rp_per_university x Universities + rp_per_institute x institutes
///
/// all times `research_multiplier`. Its SHAPE is a dial change, not a code
/// change. The defaults are Ben's ruling (2026-10-10): base + sqrt — 120 RP a
/// quarter for every nation plus 20 x sqrt(credits the line spends), linear 0,
/// 200 RP per cost unit (an age is about 80 years; Ben's correction of the
/// 100-RP measurement, which ran about twice too fast). Why a base and a
/// concave term: treasuries span orders of magnitude, and about a third of
/// nations have almost none, so no line-only conversion reaches them; a
/// linear one lets the richest buy the tree in quarters. nation_tech_harness
/// R0 measures it on the 16 curated seeds.
///
/// THE LINE PAYS WHAT THE QUEUE OWES. The academic-research line spends
/// min(its share, the credits that would buy the RP the queue still owes) —
/// the garrison-upkeep precedent of min(share, bill). An empty queue spends
/// nothing; the treasury keeps the share.
///
/// No University or institute stands on any curated seed at the seat, so their
/// two rates are UNMEASURED placeholders (one and half a cost unit a quarter).
struct research_params
{
    /// RP every nation gets each quarter, at no treasury cost.
    float base_rp_per_nation = 120.0f;
    /// RP per credit the academic-research budget line spends (the linear term).
    float rp_per_research_credit = 0.0f;
    /// RP per sqrt(credit) the line spends (the concave term).
    float rp_per_research_credit_sqrt = 20.0f;
    /// RP per quarter per completed University standing in the nation's territory.
    float rp_per_university = 100.0f;
    /// RP per quarter per completed research institute whose owner corp's home
    /// nation is this nation (NR-1023: the institute's corp `science` folds in).
    float rp_per_institute = 50.0f;
    /// The RP unit: a node costs `cost_units x rp_per_cost_unit`.
    float rp_per_cost_unit = 200.0f;
};

/// RP are held in MILLI-RP, an integer: a pool accrues small increments for
/// years, and a float pool stops registering them once it is large.
inline constexpr std::int64_t k_milli_per_rp = 1000;

/// One nation's tech state.
struct nation_tech_state
{
    std::bitset<io::dg::node_count> held;           ///< Nodes held, by `io::dg` index.
    std::int64_t                    pool_milli = 0; ///< RP in hand (milli-RP); the head's progress.
    std::vector<std::uint16_t>      queue;          ///< Node indices, head first.

    bool empty() const { return held.none() && pool_milli == 0 && queue.empty(); }
};

/// What `enqueue_node` did.
enum class enqueue_result : std::uint8_t
{
    queued = 0,      ///< The node (and any unheld requirements ahead of it) was queued.
    already_held,    ///< Nothing to do.
    already_queued,  ///< Nothing to do.
    refused_pair,    ///< The node, or every route to it, crosses a pick-one pair whose
                     ///< other side is held or queued. Nothing was queued.
    invalid,         ///< Unknown nation or out-of-range node. Nothing was queued.
};

/// One nation's accrual this quarter — reported, not stored.
struct research_accrual
{
    entity_id    nation       = null_entity;
    float        line_share   = 0.0f; ///< The research line's share of the treasury this quarter.
    float        line_spend   = 0.0f; ///< Credits it actually spent: min(share, what the queue owes).
    std::int64_t owed_milli   = 0;    ///< RP the queue still owed before this quarter's accrual.
    int          universities = 0;
    int          institutes   = 0;
    float        multiplier   = 1.0f; ///< `research_multiplier` (identity until the effect pass).
    std::int64_t rp_milli     = 0;    ///< Milli-RP added to the pool.
    int          completed    = 0;    ///< Nodes completed this quarter.
};

/// The cost of node @p node under @p p, in milli-RP (0 for an unknown node).
std::int64_t node_cost_milli(int node, const research_params& p = {});

/// RP (as milli-RP) the line converts @p credits into, before the multiplier:
/// the linear and sqrt terms only (no base, no buildings).
std::int64_t line_rp_milli(float credits, const research_params& p = {});

/// The credits the line must spend to convert into @p milli RP — the inverse
/// of `line_rp_milli` (+infinity when the line converts nothing).
double line_credits_for(std::int64_t milli, const research_params& p = {});

/// Whether @p nation holds node @p node. Unknown nation or index: false.
bool nation_holds(const world& w, entity_id nation, int node);

/// Whether @p corp's home nation holds node @p node — the read a corporation's
/// technology goes through (RESEARCH.md § Who holds technology).
bool corp_nation_holds(const world& w, entity_id corp, int node);

/// Whether every requirement group of @p node has a held member for @p nation.
bool node_requirements_met(const world& w, entity_id nation, int node);

/// Whether a quest's deed is met. EVERY deed reads false until the effect pass
/// wires them (BL-1274), so quests wait in the queue and never complete yet —
/// expected. Non-quest nodes: true.
bool deed_met(const world& w, entity_id nation, int node);

/// Queue @p node for @p nation with its unheld requirements ahead of it —
/// every group, the cheapest member where a group offers a choice (cheapest =
/// fewest RP to reach, its own unheld path included; ties to the lower store
/// index), recursively (RESEARCH.md § The research queue). Refuses — mutating
/// nothing — when the node or its only routes cross a pick-one pair whose
/// other side is held or queued.
enqueue_result enqueue_node(world& w, entity_id nation, int node);

/// The research multiplier a nation's held nodes give (*+N% national
/// research*). The effect pass (BL-1274) fills this; identity for now.
float research_multiplier(const world& w, entity_id nation);

/// A nation's held-node multiplier on @p subject, applied to @p base. The hook
/// the economy calls where a node moves a scalar (the old per-corp
/// `world::modified_scalar` read site). The effect pass (BL-1274) fills it;
/// returns @p base untouched for now.
float nation_scalar(const world& w, entity_id nation, modifier_subject subject, float base);

/// Whether @p corp may build @p type: true unless a node that UNLOCKS the type
/// is not held by the corp's home nation. The single read point the placement
/// check and the construction door share (they used to share BL-344's
/// per-corp gate). No node carries a machine unlock effect yet — the effect
/// pass (BL-1274) gives them one — so every type is open to every nation, which
/// is NR-1022's ruling for what the retired gates held. `null_entity` = open.
bool structure_unlocked(const world& w, entity_id corp, building_type type);

/// The recipe counterpart of `structure_unlocked`, same contract: the build
/// door, the retool door, the corp scorer and the selection panel all ask it.
/// Open for every recipe until a node carries an `unlock_recipe` effect.
bool recipe_unlocked(const world& w, const recipe_registry& reg, entity_id corp,
                     std::uint16_t recipe_id);

/// The quarterly research pass, once per economy tick from the nation step:
/// for every nation (ascending id) accrue RP — the academic-research line pays
/// min(its share of the treasury, the credits that buy the RP its queue still
/// owes) as a direct expenditure (the garrison-upkeep precedent), converted by
/// the `research_params` conversion; Universities, research institutes and the
/// per-nation base add RP at no treasury cost — scaled by
/// `research_multiplier`, then flow the pool down the queue: the head completes
/// when its cost is met and the remainder flows on; a quest with an unmet deed
/// (or an entry whose requirements are not yet held) is passed over and stays
/// queued. An empty queue lets the pool wait and spends no treasury.
///
/// A nation's entry in `world::nation_tech` is created only when it has
/// something to hold (RP, a queue, a held node), so a world where nothing
/// researches carries an empty map.
///
/// @param out Optional per-nation rows, ascending nation id (every nation).
void advance_research(world& w, const research_params& p = {},
                      std::vector<research_accrual>* out = nullptr);

/// Only the queue half of `advance_research` for one nation — no accrual.
/// Exposed so a harness can drive the queue semantics with a hand-set pool.
/// @return Nodes completed.
int flow_research_queue(world& w, entity_id nation, const research_params& p = {});

// ---------------------------------------------------------------------------
// The research panel's reads and the queue edits (BL-1276)
// ---------------------------------------------------------------------------

/// What `enqueue_node` WOULD do for @p nation and @p node, mutating nothing.
/// On `queued`, @p plan (optional) receives the entries it would append, in
/// queue order — the node's unheld requirements, then the node. The same
/// planner `enqueue_node` runs, so the preview cannot disagree with the press.
enqueue_result preview_enqueue(const world& w, entity_id nation, int node,
                               std::vector<std::uint16_t>* plan = nullptr);

/// The milli-RP @p nation accrues in a quarter while its queue owes more than
/// that quarter brings: the base, its buildings and the research line at its
/// whole share of the current treasury, times the multiplier. Read-only — the
/// same arithmetic `advance_research` applies, not a second formula.
std::int64_t estimate_rp_per_quarter_milli(const world& w, entity_id nation,
                                           const research_params& p = {});

/// For each entry of @p nation's queue (in queue order), the quarter it is
/// projected to complete in — 1 = the next quarterly pass — or -1 when it does
/// not complete within @p horizon_quarters (a quest waits on its deed, and an
/// entry needing it waits with it). Runs the quarterly pass's own accrual and
/// queue flow on a COPY of the state, holding the treasury where it stands.
std::vector<int> project_queue_arrivals(const world& w, entity_id nation,
                                        const research_params& p = {},
                                        int horizon_quarters = 480);

/// Drop the LAST entry of @p nation's queue. False (nothing touched) when the
/// queue is empty. The pool is untouched: it is the head's progress.
bool remove_last_queued(world& w, entity_id nation);

/// Empty @p nation's queue. False (nothing touched) when it is already empty.
/// The pool stays, so nothing researched is stranded (RESEARCH.md § The research
/// queue: the pool IS the head's progress).
bool clear_research_queue(world& w, entity_id nation);

/// THE HANDOVER (RESEARCH.md § Who holds technology: no nation opens ahead in
/// technology). Empties every nation's tech state — pool, queue and held set —
/// so play opens with `world::nation_tech` empty. Called at the end of the
/// campaign settle (`run_settle`, and the harness mirror of it): the settle's
/// ticks run the nation step like any other, so Universities, institutes and
/// the base accrue through it; none of that may carry into play.
void clear_research_at_handover(world& w);
