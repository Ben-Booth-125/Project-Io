#pragma once

#include "components.hpp"
#include "corp_command.hpp"
#include "entity.hpp"

#include <array>
#include <cstddef>
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

// ---------------------------------------------------------------------------
// Standing — what a corporation's position is measured in (BL-700)
// ---------------------------------------------------------------------------
// AI_OPPONENT.md § "Standing — what the margin is measured in". Ben, 2026-08-31:
// a corporation's standing is "an aggregate of net worth, research, military
// strength… maybe others too".
//
// Balance alone is the wrong measure and would misread the game constantly — a
// corp that has just spent its treasury on a smelter is not behind, and a corp
// hoarding cash while its rivals arm is not ahead. Everything in the codebase
// measures balance today; this is the one quantity that does not.
//
// NOT `standing.hpp`'s `corp_standing`, and the two are worth telling apart
// because the word is overloaded. That one is a DISCLOSURE-GATED PROFILE for
// the Corporations panel — reach, capital, market share, each shown or withheld
// by the observed firm's own filing status. This one is a single scalar the
// scorer compares corps by, computed from full world state with no visibility
// filter at all, and it is deliberately NOT player-facing: a coalition scores
// against it, and what the player may READ about a rival is that other type's
// question. Hence `standing_index` rather than a second `corp_standing`.

/// The components of the composite standing index.
///
/// APPEND-ONLY, and that is the point of the enum rather than three named
/// fields: Ben's wording left the list deliberately open, so a FOURTH COMPONENT
/// MUST BE AN ADDITION, NOT A REWRITE. Adding one is four edits and no
/// restructuring — an enumerator here (appended, never inserted), one more
/// initialiser in `corp_ai_params::standing_weights`, one `case` in
/// `corp_ai.cpp`'s measurement switch, and a label in
/// `standing_component_name`. Nothing sums the components by hand: the weighted
/// total is a loop over this enum, so no arithmetic anywhere has to change.
enum class standing_component : uint8_t
{
    economic = 0, ///< Net worth: cash + the assessed value of buildings and held stock.
    research,     ///< The corp's accumulated `science` (reached, never spent).
    military,     ///< Summed `unit_strength` over the corp's fielded units.
};

/// One past the highest component — the count every array here is sized by.
/// Bound to the append-only rule above: appending a component moves this with it.
inline constexpr std::size_t standing_component_count =
    static_cast<std::size_t>(standing_component::military) + 1;

/// Canonical prose label for a component ("economic", "research", "military").
const char* standing_component_name(standing_component c);

/// Tunables for the stage-A scorer. Defaults are the accepted-design values;
/// a harness may tighten them to force behaviour into few ticks.
struct corp_ai_params
{
    int   cadence_k       = 4;     ///< Corp c evaluates when tick % k == index(c) % k.
    /// BL-711: build-site pre-filter width, PER RESOURCE - not a global cap.
    ///
    /// It was `top_m_sites = 8`, a global top-M over deposit x affinity x
    /// demand_weight. Deposit magnitudes span three orders, so all 8 rows came
    /// back iron_ore and clay, peat, sand, hides and fibre were never candidates
    /// anywhere, for any corp, in any world (AI_OPPONENT.md - Selection must be
    /// scale-free). Per-resource, every extractable reaches the scorer and the
    /// scorer decides, which is what the scorer is for.
    ///
    /// 2 rather than 1 deliberately: one site per resource is a single point of
    /// failure against the placement and glut vetoes downstream, so a resource
    /// whose best tile is unbuildable would silently lose its whole category
    /// again - the same second-chance argument as BL-712's per-group build
    /// candidates. Upper bound on the returned list is K x extractables, ~36
    /// today against the old flat 8. 0 disables enumeration entirely.
    int   top_k_sites_per_resource = 2;
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

    // --- Standing (BL-700). WEIGHTS ARE DATA, SO TUNING IS A DATA CHANGE ------
    //
    // Indexed by `standing_component`, and the array (rather than three named
    // floats) is what keeps a fourth component an addition: one more
    // initialiser here and the weighted sum in corp_ai.cpp picks it up
    // untouched.
    //
    // EVERY WEIGHT IS "CREDITS PER UNIT OF THIS COMPONENT", so the composite is
    // denominated in credits and stays legible: a corp's standing reads as a
    // sum of money it holds, money it has spent reaching a research level, and
    // money it has spent putting an army in the field. Three components in
    // three unrelated units cannot be added without SOME conversion, and a
    // conversion nobody can state is a magic number; this one anyone can check.
    //
    // The defaults below are DERIVED FIRST CUTS, not tuned values — each is an
    // anchor read off the shipped `scripts/economy.lua`, and each is stated so
    // it can be argued with. None has been calibrated against play; the
    // coalition layer that scores against this composite is the thing that will
    // want them revisited.
    std::array<float, standing_component_count> standing_weights = {
        // ECONOMIC — 1.0. Net worth is already in credits; it is the unit the
        // other two convert INTO, so this weight is 1 by definition and moving
        // it is really a rescaling of the other two.
        1.0f,
        // RESEARCH — 25 credits per science point. A research_institute
        // produces exactly 1.0 science per econ tick
        // (`economy.military.science_per_research_institute_tick`) and costs
        // 15 maintenance + 10 wages per tick to run, so 25 credits is what a
        // science point costs to make. Marginal production cost, not a market
        // price: `science` has no market slot at all, by design — it is
        // stockpiled, market-invisible and never decays.
        25.0f,
        // MILITARY — 0.02 credits per point of `unit_strength`. The anchor is
        // the replacement cost of a fielded regiment: `hire_unit` raises
        // `hire_batch_manpower` (50) heads for `hire_base_cost` +
        // `hire_cost_per_power` x the row's power_mod, and `unit_strength`
        // scores a fully-supplied unit at count x quality x 100. A Levy Spear
        // costs 40 credits and stands at 5,000 (0.008 cr/point); a Rifle
        // Regiment costs 230 and stands at 6,900 (0.033). 0.02 sits between
        // them.
        //
        // A DELIBERATE UNDERSTATEMENT worth flagging rather than hiding: this
        // prices an army at what it cost to raise, which is not what it is
        // worth to the corp holding it. On the defaults, three regiments add
        // roughly 300 credits to a standing whose economic term runs to five
        // figures — so military barely registers today. That is a tuning
        // question for whoever scores coalitions against this, and the reason
        // the number lives here rather than in the code.
        0.02f,
    };

    /// Projected supply/demand ratio at which a build's score starts to taper
    /// (>1.0 = the forecast expects the market to be adequately served) and
    /// the ratio at which it is vetoed outright (a hard glut). Linear taper
    /// between the two; ratio <= glut_taper_ratio is unpenalised.
    float glut_taper_ratio = 1.0f;
    float glut_veto_ratio  = 2.0f;
};

/// One corporation's standing, component by component plus the weighted total.
///
/// The components are kept ALONGSIDE the total rather than collapsed into it,
/// and that is not convenience: a coalition that forms against a leader has to
/// be able to say WHY it formed, and "ahead on military" is a different
/// statement from "ahead on net worth" even when the totals match. It is also
/// what makes the weights auditable — a reader can recompute the total.
struct standing_index
{
    /// Raw, UNWEIGHTED component values, indexed by `standing_component`.
    /// Economic is in credits, research in science points, military in
    /// `unit_strength` (the x100 fixed point).
    std::array<float, standing_component_count> component{};

    /// Sum of `component[i] * p.standing_weights[i]`, in credits.
    float total = 0.0f;
};

/// The composite standing of `corp` (BL-700) — AI_OPPONENT.md § "Standing".
/// An unknown corp stands at zero on every component; this never throws.
///
/// READ POINT — BINDING, and the reason this is a free function over a const
/// world rather than something the scorer computes inline.
///
/// Standing must be read at the ECON-TICK BOUNDARY: after `apply_budget` has
/// written the closing balances and `clear_markets` has resolved the prices
/// that value held stock, and BEFORE any corp's strategic evaluation mutates
/// the world. `run_corp_strategic_step` walks corps in SORTED ID ORDER and
/// applies each one's commands as it goes, so a standing read from inside that
/// walk would answer differently for the first corp than for the last — the
/// evaluation cadence would silently become the tiebreak of every comparison
/// built on it, which is exactly the class of thing that makes a deterministic
/// simulation replay differently for a reason nobody can see. A consumer that
/// needs the whole field's standings must therefore SNAPSHOT them once at the
/// boundary and score against the snapshot; it must never call this per corp
/// from inside the walk.
///
/// DETERMINISTIC BY CONSTRUCTION, in three separate places:
///   * cash and building value walk `corporation_component::assets`, a vector
///     in authored order;
///   * held stock walks `world::corp_body_pools`, a std::map, so key-ordered;
///   * the military term accumulates `unit_strength` as an INTEGER over
///     `world::units`, which is an unordered_map — a float accumulator there
///     would make the sum depend on hash layout, since float addition is not
///     associative. This is the same guard `condition_set.cpp`'s
///     `military_strength` subject applies, for the same reason.
standing_index corp_standing_index(const world& w, const recipe_registry& reg,
                                  entity_id corp, const corp_ai_params& p = {});

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
