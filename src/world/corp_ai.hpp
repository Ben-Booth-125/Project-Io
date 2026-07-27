#pragma once

#include "components.hpp"
#include "corp_command.hpp"
#include "entity.hpp"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

struct world;
class recipe_registry;
struct economy_report;

// ---------------------------------------------------------------------------
// Corp AI stage A — the scored utility layer (BL-202, AI_OPPONENT.md § 5)
// ---------------------------------------------------------------------------
// Per-corp strategic evaluation over a bounded candidate set, scored with
// existing estimators only, applied through the corp-command seam. Runs inside
// run_economy_step after the BL-079 reflex tier (tier 0). Deterministic by
// construction: sorted iteration, lowest-id tie-breaks, and the only
// "randomness" is a per-corp hash used as a fixed personality jitter.

/// Tunables for the stage-A scorer. Defaults are the accepted-design values;
/// a harness may tighten them to force behaviour into few ticks.
struct corp_ai_params
{
    int   cadence_k       = 4;     ///< Corp c evaluates when tick % k == index(c) % k.
    int   top_m_sites     = 8;     ///< Build-site pre-filter width (bounded enumeration).
    float theta           = 0.15f; ///< Hysteresis: beat do-nothing/incumbent by this relative margin.
    int   cooldown_evals  = 4;     ///< Evals a touched building holds before re-dialling.
    int   max_builds      = 1;     ///< Constructions per corp per evaluation.
    int   max_dials       = 3;     ///< Dial changes (recipe/workforce/idle/resume) per evaluation.

    /// Solvency reserve floor = max(floor_constant, floor_wage_mult × the corp's
    /// per-tick wage bill). A tick is one quarter, so the default is two
    /// quarters of wages — the accepted design's "≈ 2× quarterly wage bill".
    float floor_wage_mult = 2.0f;
    float floor_constant  = 50.0f;

    /// Seed folded into the per-corp personality hash. The world does not store
    /// its generation seed, so this is supplied by the caller (0 by default —
    /// the corp id alone still yields a stable, campaign-constant jitter).
    uint64_t personality_seed = 0;
};

/// The corp's solvency reserve floor under `p` (exposed for the harness).
float corp_reserve_floor(const world& w, const recipe_registry& reg,
                         entity_id corp, const corp_ai_params& p = {});

/// Fixed personality jitter for a corp in [0.9, 1.1] — a pure hash of
/// (personality_seed, corp id); constant for the whole campaign.
float corp_personality_jitter(entity_id corp, uint64_t personality_seed);

/// Run the strategic evaluation for every due NON-player corp at `tick`,
/// emit corp_commands, apply them through apply_corp_command, and record both
/// the world's decision ring and the report's agency_events. The player's corp
/// is never evaluated and never commanded. Called from run_economy_step after
/// the BL-079 reflex tier; deterministic.
void run_corp_strategic_step(world& w, const recipe_registry& reg,
                             economy_report& report, int tick,
                             const corp_ai_params& p = {});

// ---------------------------------------------------------------------------
// State export — the per-corp blackboard (AI_OPPONENT.md § 6)
// ---------------------------------------------------------------------------
// A compact, tick-tagged, visibility-honest view of the world as one corp may
// see it: own buildings/pools/cash in full; market prices/aggregates public;
// rival buildings existence/type/tile ONLY (never cash, pools, recipes, or
// workforce — BL-068); tile facts only for surveyed regions; activity-fog
// facts only for known bodies.
//
// FOG DESIGN CALL (stated per the brief): the survey store (body_component
// .survey) and the activity fog (trade_routes + body_activity_visibility) are
// single, player-centric world stores — there is no per-corp fog state yet.
// The export applies the world's survey/activity state uniformly to every
// corp and tags each such fact's provenance (`survey` / `route`) so a future
// per-corp fog can tighten the filter without changing the schema.

/// Where a fact's knowledge comes from — the visibility rule that admitted it.
enum class fact_provenance : uint8_t
{
    own_asset = 0, ///< The corp's own building / pool / cash — full detail.
    public_market, ///< Market prices and aggregates — public to everyone.
    rival_visible, ///< A rival building's existence/type/tile — internals private.
    survey,        ///< Geographic fog: tile facts from surveyed regions.
    route,         ///< Activity fog: body-level commercial visibility.
};

/// One (tick, subject, predicate, value) fact with confidence + provenance.
struct corp_fact
{
    int             tick       = 0;
    entity_id       subject    = null_entity;
    std::string     predicate;            ///< e.g. "cash", "building_type", "price:steel".
    double          value      = 0.0;
    float           confidence = 1.0f;    ///< 1.0 = direct read; lower for stale/coarse tiers.
    fact_provenance provenance = fact_provenance::own_asset;
};

/// The per-corp state export. `_v` versions the schema from day one.
struct corp_blackboard
{
    int                    _v   = 1;          ///< Schema version.
    entity_id              corp = null_entity;
    int                    tick = 0;
    std::vector<corp_fact> facts;             ///< Deterministically ordered (see .cpp).
};

/// Build the visibility-honest blackboard for `corp` at `tick`. Pure read;
/// deterministic ordering (subject-kind section, then entity id, then predicate).
corp_blackboard export_corp_blackboard(const world& w, entity_id corp, int tick);

/// Canonical string for a provenance tag ("own-asset", "public-market", ...).
const char* fact_provenance_name(fact_provenance p);

/// Serialise the blackboard as JSONL (BL-206): one fact per line
/// `{"_v":1,"t":..,"subject":..,"predicate":"..","value":..,"confidence":..,
/// "provenance":".."}` in the blackboard's deterministic order. Numeric
/// formatting is fixed (%.9g) so same-seed runs are byte-identical.
void to_jsonl(const corp_blackboard& bb, std::ostream& out);
