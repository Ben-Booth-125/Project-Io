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
// float that could disagree with the pool: the head's progress is
// `min(pool, cost(head))`, and the remainder after a completion is simply what
// is left in the pool, which flows on to the next entry in the same pass.
//
// Deterministic: nations walked ascending, queue walked in stored order, every
// float written in a fixed order. Nothing here reads the clock or a RNG.
// ---------------------------------------------------------------------------

/// The research dials (RESEARCH.md § What is open: the line's conversion rate,
/// the University's RP and the RP unit are dials for the sweep). A FIRST CUT,
/// measured against the pacing rule on the 16 curated seeds (2026-10-10,
/// nation_tech_harness's RATE SOLVE row): the median nation holds its first
/// ring-2 major (DG-PR-EX-A, 18 cost units with its path) by live quarter ~10
/// at 11.5 RP per credit, rounded to 12. The measurement also shows the cost of
/// a linear conversion over treasuries that span orders of magnitude: a quarter
/// of nations need 10^5x that rate and the richest would buy the whole tree in
/// ~12 quarters. No University or institute stands on any curated seed at the
/// seat, so their two rates are UNMEASURED placeholders (one and half a cost
/// unit a quarter).
struct research_params
{
    /// RP per credit the academic-research budget line spends.
    float rp_per_research_credit = 12.0f;
    /// RP per quarter per completed University standing in the nation's territory.
    float rp_per_university = 100.0f;
    /// RP per quarter per completed research institute whose owner corp's home
    /// nation is this nation (NR-1023: the institute's corp `science` folds in).
    float rp_per_institute = 50.0f;
    /// The RP unit: a node costs `cost_units x rp_per_cost_unit`.
    float rp_per_cost_unit = 100.0f;
};

/// One nation's tech state.
struct nation_tech_state
{
    std::bitset<io::dg::node_count> held;  ///< Nodes held, by `io::dg` index.
    float                           pool = 0.0f; ///< RP in hand; the head's progress.
    std::vector<std::uint16_t>      queue;  ///< Node indices, head first.
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
    entity_id nation        = null_entity;
    float     line_spend    = 0.0f; ///< Credits the academic-research line spent.
    int       universities  = 0;
    int       institutes    = 0;
    float     multiplier    = 1.0f; ///< `research_multiplier` (identity until the effect pass).
    float     rp            = 0.0f; ///< RP added to the pool.
    int       completed     = 0;    ///< Nodes completed this quarter.
};

/// The RP cost of node @p node under @p p.
float node_cost_rp(int node, const research_params& p = {});

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
/// for every nation (ascending id) accrue RP — the academic-research line's
/// share of the treasury (debited, a direct expenditure on the garrison-upkeep
/// precedent) x `rp_per_research_credit`, plus Universities and research
/// institutes — scaled by `research_multiplier`, then flow the pool down the
/// queue: the head completes when its cost is met and the remainder flows on;
/// a quest with an unmet deed (or an entry whose requirements are not yet
/// held) is passed over and stays queued. An empty queue lets the pool wait.
///
/// @param out Optional per-nation rows, ascending nation id.
void advance_research(world& w, const research_params& p = {},
                      std::vector<research_accrual>* out = nullptr);

/// Only the queue half of `advance_research` for one nation — no accrual.
/// Exposed so a harness can drive the queue semantics with a hand-set pool.
/// @return Nodes completed.
int flow_research_queue(world& w, entity_id nation, const research_params& p = {});
