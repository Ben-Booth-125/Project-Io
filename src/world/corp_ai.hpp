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
    /// BL-440: how hard an UNMET RECIPE INPUT pulls a site's suitability up.
    ///
    /// A tile offers every extractable deposit it carries as a candidate target,
    /// not just its richest, and this weights those candidates by what the economy
    /// cannot currently get. Without it, a resource that is common but rarely
    /// dominant is structurally unmineable: it loses every richness comparison it
    /// is ever entered into, so no scorer ever picks it, however badly a recipe
    /// wants it. Measured 2026-08-17: SEVEN wanted inputs sat on 200+ tiles apiece
    /// with zero sites named for them, one of them on 16,361 tiles.
    ///
    /// Deliberately NOT routed through the price signal, which cannot work here:
    /// a processor must be RUNNING to bid a scarce input's price up, and it cannot
    /// run without that input. The deadlock is the defect, so the pull is taken
    /// from the recipe graph, which is a static, deterministic world fact.
    ///
    /// Self-limiting by construction — the bonus divides by the sites already
    /// targeting the resource, so it decays as the shortage is answered rather
    /// than driving the whole map onto one good. 0 restores pre-BL-440 behaviour.
    float input_demand_pull = 4.0f;
    float theta           = 0.15f; ///< Hysteresis: beat do-nothing/incumbent by this relative margin.
    int   cooldown_evals  = 4;     ///< Evals a touched building holds before re-dialling.
    int   max_builds      = 1;     ///< Constructions per corp per evaluation.
    int   max_dials       = 3;     ///< Dial changes (recipe/workforce/idle/resume) per evaluation.
    int   max_trades      = 1;     ///< Order-book commands per corp per evaluation (BL-293).
    int   max_dispatches  = 1;     ///< Directed convoy dispatches per corp per evaluation (BL-600).

    /// BL-409 — spectator mode: the session has no human seat.
    ///
    /// The standing prohibition on auto-acting strategically covers the
    /// PLAYER'S corp, and it protects that corp *because a human owns it*.
    /// Under spectate nobody does, so the rule's precondition is absent rather
    /// than excepted, and every corp evaluates on the same staggered cadence —
    /// including `world::player_entity`, which degrades to a camera/ledger
    /// anchor with no ownership meaning. Default false: an ordinary played
    /// session is unchanged, byte for byte.
    bool  spectating      = false;

    // --- Trading (BL-293). DELIBERATELY CONSERVATIVE FIRST CUT, AND TUNABLE ---
    // "Can trade" is not "trades well": a scorer that dumps stock at the floor
    // price is worse than one that does not trade at all, because it drags the
    // resolved price down for everyone including itself. So the rule here is the
    // narrowest one that is still genuine trading — list what has visibly piled
    // up, and refuse to sell it cheap — and it is expressed as two numbers rather
    // than as a market strategy, so tuning it is a data change. A real strategy
    // (reading price trend, timing releases, targeting a rival's shortage) is a
    // later item; see AI_OPPONENT.md.

    /// Stock a corp holds on a body before any of it is considered surplus worth
    /// listing. Well above a processor's per-tick input draw, so the trade
    /// candidate cannot compete with feeding the corp's own chain — the pool has
    /// to have genuinely accumulated.
    float trade_hold_threshold = 50.0f;

    /// Fraction of the stock above the threshold that one order lists. Below 1.0
    /// so the corp meters its release rather than emptying the pool into a single
    /// quarter's clearing.
    float trade_release_fraction = 0.5f;

    /// Floor price as a multiple of the market's BASE price. The floor is a
    /// reservation price — an order whose floor exceeds the resolved price holds
    /// its stock rather than selling (market_clearing.cpp § auto-clear) — and the
    /// resolved price pegs at 0.25× base on a glutted market (the price-band
    /// floor). So 0.25 IS the band floor: the lowest price the market can ever
    /// resolve, which means surplus always clears at whatever it does resolve.
    /// Anything above 0.25 makes the corp hold on a deep glut — a strategy call,
    /// not a default. Note the sell-candidate score (corp_ai.cpp) is valued at
    /// the floor, so this multiple scales trade-candidate scores with it; the
    /// conservative estimate stays honest.
    float trade_floor_multiple = 0.25f;

    /// Solvency reserve floor = max(floor_constant, floor_wage_mult × the corp's
    /// per-tick wage bill). A tick is one quarter, so the default is two
    /// quarters of wages — the accepted design's "≈ 2× quarterly wage bill".
    float floor_wage_mult = 2.0f;
    float floor_constant  = 50.0f;

    /// Seed folded into the per-corp personality hash. The world does not store
    /// its generation seed, so this is supplied by the caller (0 by default —
    /// the corp id alone still yields a stable, campaign-constant jitter).
    uint64_t personality_seed = 0;

    /// Predictive-spending horizon padding (AI_OPPONENT.md §2B): a build's
    /// forecast horizon is its own build_duration_ticks (post-completion,
    /// so the projection lands after the building is live) plus this many
    /// ticks for "one clearing pass". 1 by default.
    int forecast_clearing_ticks = 1;

    /// Projected supply/demand ratio at which a build's score starts to taper
    /// (>1.0 = the forecast expects the market to be adequately served) and
    /// the ratio at which it is vetoed outright (a hard glut). Linear taper
    /// between the two; ratio <= glut_taper_ratio is unpenalised.
    float glut_taper_ratio = 1.0f;
    float glut_veto_ratio  = 2.0f;
};

// ---------------------------------------------------------------------------
// Stage B — strategy, priority buckets, predictive spending (BL-203,
// AI_OPPONENT.md §2B). The Victoria-3 import that replaces BL-202's crude
// reserve-floor gate with a solvency answer that scales with the AI's actual
// commitments, so it can stay solvent honestly rather than needing a cheat.
// ---------------------------------------------------------------------------

/// A corp's coarse strategy — currently a direct read of its generated
/// industrial focus (CORPORATION_GENERATION.md's specialist premise); kept as
/// its own named concept (rather than inlining `industrial_focus` everywhere)
/// so the scorer's strategy bias is legible and can diverge from the
/// generation-time focus later without a signature break.
using corp_strategy = industrial_focus;

/// Spending priority — a lower bucket may NEVER starve a higher one
/// (AI_OPPONENT.md §2B). Must-Have candidates carry no capex (idling a
/// loss-maker only saves wages/maintenance) so they are never floor-gated;
/// Should-Have dials (recipe/workforce/resume — feeding a running processor)
/// are likewise free; only Nice-to-Have (build/survey — expansion) spends
/// cash, and it alone is gated against the stricter should-have-aware floor.
enum class corp_priority_bucket : uint8_t
{
    must_have    = 0, ///< Solvency defense: stop a sustained loss (wages/maintenance bleed).
    should_have  = 1, ///< Keep running assets fed/tuned (recipe, workforce, resume).
    nice_to_have = 2, ///< Expansion: new construction, survey.
};

/// The bucket a decision reason belongs to — a pure, deterministic mapping.
corp_priority_bucket bucket_for_reason(corp_decision_reason reason);

/// Canonical prose label for a corp_command's verb — what the world history
/// log and the AI Decision Feed both narrate (BL-420: hoisted out of
/// corp_ai.cpp's anonymous namespace so the feed no longer keeps its own
/// word-for-word copy). Exhaustive over corp_verb; falls back to "action"
/// for any verb the switch has not yet been extended to cover.
const char* corp_verb_label(corp_verb v);

/// Canonical prose label for a corp_decision_reason — companion to
/// `corp_verb_label` (BL-420). Falls back to "unspecified".
const char* corp_decision_reason_label(corp_decision_reason r);

/// The cash a corp must reserve, beyond the BL-202 wage/maintenance floor, to
/// keep feeding its currently-running processing facilities this tick — the
/// Should-Have buffer that a Nice-to-Have (build/survey) spend may never dip
/// into. Sum of `estimate_building_profit(...).input_cost` over the corp's
/// running processing facilities. Exposed for the harness.
float corp_should_have_buffer(const world& w, const recipe_registry& reg,
                              const economy_report& report, entity_id corp);

/// Predictive-spending score multiplier for a candidate build of `type`
/// producing `target` at `tile`, given its expected per-tick output
/// `added_rate_per_tick` once live. Forecasts the added supply over
/// `horizon_ticks` against the LOCAL MARKET'S PUBLIC supply/demand
/// aggregates only (visibility-honest — the same facts export_corp_blackboard
/// would show a rival, per BL-068/DISCOVERY.md) and returns 1.0 (no penalty)
/// when the forecast supply/demand ratio stays at or below `p.glut_taper_ratio`,
/// tapering linearly to 0.0 (veto) at `p.glut_veto_ratio`. Exposed for the
/// harness; also used internally by the build-candidate scorer.
float forecast_glut_multiplier(const world& w, entity_id tile, resource_type target,
                               float added_rate_per_tick, int horizon_ticks,
                               const corp_ai_params& p = {});

/// The corp's solvency reserve floor under `p` (exposed for the harness).
float corp_reserve_floor(const world& w, const recipe_registry& reg,
                         entity_id corp, const corp_ai_params& p = {});

/// Fixed personality jitter for a corp in [0.9, 1.1] — a pure hash of
/// (personality_seed, corp id); constant for the whole campaign.
float corp_personality_jitter(entity_id corp, uint64_t personality_seed);

/// True when `corp` is due to evaluate at `tick` under the staggered cadence
/// (sorted-corp-id index % cadence_k == tick % cadence_k) — the same schedule
/// `run_corp_strategic_step` uses internally. Exposed so callers outside the
/// scorer (e.g. the persona counsel layer, BL-207) can key their own bounded,
/// per-eval work to the identical deterministic boundary without duplicating
/// the corp-sort.
///
/// Always false for the player corp — UNLESS `p.spectating` (BL-409), where
/// the session has no human seat and the player corp is due on its own cadence
/// slot like any other. Do not read the unconditional form into this: for a
/// played session the answer is still always false.
bool corp_strategic_eval_due(const world& w, entity_id corp, int tick,
                             const corp_ai_params& p = {});

/// Run the strategic evaluation for every due corp at `tick`, emit
/// corp_commands, apply them through apply_corp_command, and record both the
/// world's decision ring and the report's agency_events. Called from
/// run_economy_step after the BL-079 reflex tier; deterministic.
///
/// The player's corp is skipped entirely — never evaluated, never commanded,
/// not even its dial cooldowns ticked (io-standing-rules.md). The sole
/// exception is `p.spectating` (BL-409): with no human seat the prohibition
/// has no subject, and every corp including `world::player_entity` evaluates
/// on the same staggered cadence.
///
/// Admitting the player corp does not perturb the others. The cadence keys on
/// a corp's index in the SORTED FULL corp set, which already contains the
/// player — so a spectated run changes who acts, never when the rest do.
///
/// BUDGET CLAIMS (BL-537 / Sprint N3 T5). Besides commands, the step emits
/// `economy_report::budget_claims`: when a due corp's top-scoring survey is
/// foregone at the SOLVENCY GATE and the corp has a `home_nation`, it files one
/// `budget_claim` on `public_exploration` for that survey's FULL cost, subject =
/// the body (NR-568: earmarked, not a top-up). At most one per corp per
/// evaluation; none from a corp that could afford its survey, none from a corp
/// with no home nation, and never from the player corp (never evaluated).
/// Recorded only — the national budget pass that pays it runs later in the
/// tick, and is not this function's.
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
