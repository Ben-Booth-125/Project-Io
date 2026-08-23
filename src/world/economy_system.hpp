#pragma once

#include "components.hpp"
#include "recipe_registry.hpp"
#include "battle_system.hpp" // battle_dispatch (BL-467/BL-468)
#include "world.hpp"

#include <array>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

/// Per-building observability record produced by one economy step. Read by the
/// economy panel (idle/active state, output rate, limiting input) — held by the
/// app between econ ticks and not otherwise persisted.
// Each deposit carries a finite reserve (tile_component.resource_remaining, seeded
// at generation to richness × a reserve factor). Extraction draws the reserve down;
// as it nears empty the output tapers, then the building reports "out of resources".
// Hard-coded sensible estimates, iterated by playtest — not derived.
//
// PUBLIC because the prospective-profit estimate behind the tile construction
// ledger (BL-162) has to apply the SAME taper. It previously ignored the reserve
// entirely and priced a spent tile as though it were untouched, which put the
// tallest bar on the one candidate guaranteed to earn nothing. Two copies of a
// depletion curve is exactly how that divergence happens, so there is one.
constexpr float deposit_taper_ticks = 8.0f;  ///< Output tapers over the last ~8 ticks of nominal yield.
constexpr float deposit_min_taper   = 0.05f; ///< Below 5% of nominal the reserve reads as exhausted.

struct building_report
{
    entity_id     building = null_entity;
    entity_id     corp     = null_entity;
    entity_id     body     = null_entity;
    building_type type     = building_type::none;

    resource_type target_resource = resource_type::iron_ore; ///< Extraction only.
    uint16_t      recipe          = no_recipe;                ///< Processing only.

    bool  active    = false; ///< Produced output this tick.
    bool  idle      = false; ///< Produced nothing (no workforce / no deposit / below t_idle / misconfigured).
    bool  exhausted = false; ///< Extraction only: the tile's deposit reserve is spent ("out of resources").
    float output_quantity = 0.0f; ///< Units credited to the pool this tick (sum of outputs for a processor).

    /// Labour actually applied: `workforce_assigned × contention scalar` for the
    /// building's (corp, body). Equals `workforce_assigned` when labour is uncontended.
    float effective_workforce = 0.0f;

    bool          has_limiting   = false;               ///< Processing: a binding input exists.
    resource_type limiting_input = resource_type::iron_ore; ///< Processing: the scarcest input (pool-relative).
};

/// Per-corporation budget breakdown retained from the budget step (BL-072): the
/// flows that net to the balance delta applied this tick. `income`/`expenditure`
/// are the market cash flows (corp_cash_flow); `maintenance`/`wages` are the
/// operating costs apply_budget computes per building; `interest` is the BL-073
/// debt charge (0 until a corp's balance is negative). `net()` is exactly the
/// delta added to the balance this tick. Populated only when apply_budget is given
/// a breakdown sink — the headless harnesses leave it empty.
struct corp_budget
{
    float income      = 0.0f;
    float expenditure = 0.0f;
    float maintenance = 0.0f;
    float wages       = 0.0f;
    float interest    = 0.0f; ///< BL-073: charged only while balance < 0.

    /// BL-343: levies charged by enacted laws this tick — today, the extraction
    /// levy (a per-unit charge on raw output). Since BL-480 the charge is a
    /// TRANSFER into the enacting nation's treasury, bounded by that nation's
    /// jurisdiction — zero for a corp whose extraction stands outside it. This
    /// is the line that makes a law OBSERVABLE: a law the player cannot see
    /// working is indistinguishable from an unimplemented one.
    float levies      = 0.0f;

    /// BL-454: standing-force upkeep — the CREDIT half of what it costs to keep
    /// the corp's units this tick. Its OWN term, deliberately not folded into
    /// `wages`: a hidden term is a term nobody tunes, and the budget ledger has
    /// to be able to show the player what the army costs as against what the
    /// factories cost. The GOODS half is not here at all — that is a pool debit
    /// (economy_system.hpp § run_unit_upkeep), not a money flow.
    float upkeep      = 0.0f;

    /// BL-454 observability, not money: how many units the corp fields, and how
    /// many of those are not fully supplied. The budget ledger's force line
    /// calls the unmet part out — an army quietly weakening because its goods
    /// never arrive is exactly the thing a player must not have to infer.
    int   force_units      = 0;
    int   force_unsupplied = 0;

    /// The per-tick balance delta: income less every outflow.
    float net() const
    {
        return income - expenditure - maintenance - wages - interest - levies - upkeep;
    }
};

/// One background-corp agency action taken this tick (the BL-079 reflexes). Pure
/// derived data recorded alongside the action — recording changes nothing about
/// the action itself, so determinism is untouched. Consumed by the chat feed
/// (BL-205) and, later, the BL-202 decision log.
struct agency_event
{
    enum class kind : uint8_t
    {
        recipe_switch,     ///< A floored processor switched to a healthier recipe.
        idled,             ///< A sustained loss-maker was decommissioned.
        // --- BL-202 strategic-command kinds (the scored-utility layer) ---
        built,             ///< A new building was placed (`tile` set; `building` is the new id).
        demolished,        ///< An owned building was removed.
        workforce_set,     ///< The workforce dial moved (`value` = new target, 0–200).
        resumed,           ///< An idled building was brought back online.
        road_placed,       ///< A road was laid (`tile` set; `value` = tier 1–3).
        survey_dispatched, ///< A survey was dispatched (`building` unused; `tile` = body id).
        hired,             ///< A unit was raised (BL-324; `tile` = muster tile, `value` = roster index).
        // --- BL-293: the order book (`building` unused; `tile` = body id) ---
        order_placed,      ///< A standing sell order was placed (`value` = resource id).
        order_removed,     ///< A standing sell order was withdrawn (`value` = resource id).
    };

    entity_id corp;
    entity_id building;
    kind      what;
    uint16_t  new_recipe = 0;           ///< recipe_switch only: the recipe switched to.
    entity_id tile       = null_entity; ///< built / road_placed: target tile; survey_dispatched / order_*: body.
    int       value      = 0;           ///< workforce_set: new target; road_placed: tier; order_*: resource id.
};

/// Result of one economy step: the per-building reports plus the auto-bought
/// input shortfalls per (corp, body), which become market demand and corporate
/// expenditure downstream (market_clearing.hpp / budget_system.hpp).
struct economy_report
{
    std::vector<building_report> buildings;

    /// Building id → index into `buildings`, filled once by run_economy_step after
    /// the production pass (BL-360). estimate_building_profit resolves its row here
    /// rather than scanning `buildings` — that scan was O(B²) per tick under the
    /// BL-079 reflex loop and the BL-202 strategic step. Empty on a hand-built
    /// report; readers then fall back to the scan.
    std::unordered_map<entity_id, std::size_t> building_row;

    /// Background-corp agency actions taken this tick (BL-079), in the
    /// deterministic order they were applied. See agency_event.
    std::vector<agency_event> agency_events;

    /// The corps the strategic tier EVALUATED this tick, in sorted-id order —
    /// whether or not they then acted. Recorded so a driver's cadence can be
    /// proven through the real tick rather than inferred (BL-568: the live app
    /// ran half the roster for weeks and nothing could see it). Includes the
    /// player corp only under `corp_ai_params::spectating`.
    std::vector<entity_id> corps_evaluated;

    /// Battles that did something this tick (BL-467/BL-468), in the same sorted
    /// (province, attacker, defender) order the battle record is walked in.
    ///
    /// THE WORLD REPORTS; THE PRESENTATION LAYER VOICES. This is the same seam
    /// `agency_events` established and the one NR-407 said BL-458's silent
    /// interdiction was waiting for: a world event needs somewhere to be recorded
    /// that is not `src/ui`, because the world layer must not depend on the UI.
    /// `session_history` turns these into comms lines; the battle card reads the
    /// live record instead, since it wants the trace rather than a summary.
    ///
    /// It includes battles that CONCLUDED this tick, which `world::battles` no
    /// longer holds — the aftermath is the line a player most needs and it
    /// describes something already erased.
    std::vector<battle_dispatch> battle_dispatches;

    /// Per (corporation, body): the input quantities a consumer could not cover
    /// from its own pool and auto-bought from the market this tick — the FILL.
    /// Resource-indexed. This is goods actually RECEIVED, and it is what the
    /// money follows: auto_buys, the VWAP accumulator, and corporate expenditure
    /// all read this and must keep reading it (market_clearing.cpp).
    std::map<std::pair<entity_id, entity_id>, std::array<float, resource_count>> purchases;

    /// BL-441. Per (corporation, body): the input quantities a consumer WANTED
    /// from the market this tick — the WANT, whether or not the draw succeeded.
    /// Resource-indexed, and the ONLY thing `mc.demand` is allowed to read.
    ///
    /// The distinction is the one BL-422 landed on the sell side, applied to the
    /// buy side: SUPPLY is an offer and INVENTORY is a delivery; likewise a WANT
    /// is a bid and a FILL is a receipt. The register used to record fills, so a
    /// processor that needed 16 units of an input and could draw only 2 told the
    /// market it wanted 2 — the resource then read to resolve_price as one almost
    /// nobody wants, its price never rose, and scarcity was invisible. Never pay
    /// against this map: doing so would credit deliveries nobody made.
    ///
    /// Same key type and same std::map as `purchases`, deliberately: this seam is
    /// where BL-422 found a latent unordered_map float-accumulation
    /// nondeterminism, so the accumulation order must stay SORTED.
    std::map<std::pair<entity_id, entity_id>, std::array<float, resource_count>> wants;

    /// Per (corporation, body): the workforce contention scalar applied this tick —
    /// `min(1, supply/demand)`. 1.0 when the corp's labour demand on that body fits
    /// its pool supply; below 1.0 every building on that (corp, body) is throttled
    /// proportionally (docs/economy/POPULATION.md § Workforce model, step 1). Read by
    /// the budget step (wages on effective workforce) and the economy panel.
    std::map<std::pair<entity_id, entity_id>, float> workforce_contention;

    /// Per-body mean habitability aggregate (BL-048): weighted average of all
    /// population-centre tiles on the body. 0.0 = uninhabitable, 1.0 = full.
    std::map<entity_id, float> body_habitability;

    /// Per-corporation budget breakdown for this tick (BL-072): the four+1 flows
    /// netting to the balance delta. Populated by apply_budget when given a
    /// breakdown sink (app's live loop); empty under the headless harnesses, which
    /// do not request it. Read by the Balance Ledger / Economy panel.
    std::map<entity_id, corp_budget> budgets;
};

/// Run one economy step over every corporation's buildings: extraction credits
/// the (corp, body) pool with its target resource and draws the same amount from
/// the tile's finite `resource_remaining` reserve (tapering as it nears empty,
/// then reporting the building exhausted); processing runs its recipe pool-first
/// under the two-threshold partial-run model (registry t_full/t_idle), accruing
/// outputs and recording the auto-bought shortfall. Deterministic: corporations
/// are visited in ascending id order, assets in their stored order.
///
/// @param w   World; the (corp, body) pools are mutated in place.
/// @param reg Loaded recipe/economy registry.
/// @param spectating BL-409: when true the session has no human seat, so the
///        strategic tier evaluates EVERY corp including `w.player_entity`.
///        Defaulted so no existing caller (harnesses included) changes.
/// @return    The step report (building states + auto-bought shortfalls).
economy_report run_economy_step(world& w, const recipe_registry& reg,
                                bool spectating = false);

// ---------------------------------------------------------------------------
// BL-454 — the unit pass
// ---------------------------------------------------------------------------

/// What one run of the unit pass did. Pure observability; nothing reads it to
/// make a decision, so recording it cannot perturb determinism.
struct unit_upkeep_tick
{
    int units        = 0; ///< Units alive after the pass.
    int disbanded    = 0; ///< Orphaned units erased this tick (see run_unit_upkeep).
    int unmet        = 0; ///< Units whose goods draw could not be met from the pool.
    int out_of_reach = 0; ///< Units beyond the reach field (trigger (a)).
};

/// BL-454's unit pass — the GOODS half of standing-force upkeep, plus the one
/// decay rule, plus orphan cleanup. Runs inside run_economy_step; exported so a
/// harness can drive it directly.
///
/// Per unit, in ASCENDING UNIT ID (the map is unordered, so the id list is
/// sorted first — the pass writes a shared pool and its order is therefore
/// load-bearing):
///
///  1. ORPHAN CLEANUP. `demolish_building` erases the building, the corp asset
///     and the building stockpile but never touches `w.units`, so demolishing a
///     muster base left every unit raised there orphaned — a real defect, and
///     this pass is the only place that can see it. A unit is disbanded when its
///     owning corp is gone, when its tile is gone, or when its recorded
///     `muster_base` is no longer in `w.buildings`. A unit with no recorded
///     muster base (the hard-coded world's stub) is never disbanded.
///
///  2. THE GOODS DRAW. `resolve_unit_upkeep` resolves the cost vector from the
///     roster; each good is drawn from the unit owner's pool ON THE UNIT'S OWN
///     BODY. A pool can be empty, so the draw takes what is there and NEVER goes
///     negative; a short draw marks the unit unmet.
///
///  3. THE DECAY RULE — ONE rule, TWO triggers. (a) the unit is beyond the reach
///     field (BL-325 S3's out-of-supply decay), or (b) the draw in step 2 went
///     unmet. Either fires the SAME subtraction on `supply_factor_permille`;
///     neither firing lets it recover. Deterministic scalar arithmetic, no RNG.
///
/// The CREDIT half is deliberately not here — it is a budget flow and lands as
/// its own term in apply_budget's decomposition (budget_system.hpp).
///
/// Inert at the shipped rates: every `unit_upkeep_params` rate defaults to zero,
/// which means no pool is touched (not even created), no draw can go unmet, and
/// the reach field is never built from here.
unit_upkeep_tick run_unit_upkeep(world& w, const recipe_registry& reg);

// ---------------------------------------------------------------------------
// BL-470 — the unit march pass
// ---------------------------------------------------------------------------

/// What one run of the march pass did. Pure observability, same shape/intent
/// as unit_upkeep_tick — nothing reads it to make a decision.
struct unit_march_tick
{
    int marching  = 0; ///< Units that held a live order when the pass started.
    int arrived   = 0; ///< Orders that reached their destination this tick.
    int recomputed = 0; ///< Paths recomputed after a blocked step this tick.
};

/// NR-344's mobilisation test — see run_unit_march's doc comment. Exposed
/// for the harness (tools/verify/unit_march_harness.cpp).
bool corp_is_mobilised(const world& w, entity_id corp);

/// BL-470's per-tick movement resolution. Spends each marching unit's
/// per-CLASS `march_points_per_class` (recipe_registry::military(), keyed off
/// the roster row's `cls`) against the per-tile traversal cost `intra_body_
/// path`/`body_reach_field` already compute — never a second cost model —
/// with fractional carry-over banked in `unit_component::order.progress`.
///
/// VISIT ORDER (NR-344, "war flips the queue", resolved 2026-08-19 alongside
/// this item): a corp party to ANY declared hostility (either direction —
/// being attacked mobilises too, `is_hostile` checked both ways against
/// `w.corp_hostile_pairs`) has its units visited FIRST this tick; every other
/// corp's units follow, in ascending unit id within each group. At peace
/// this changes nothing observable YET — there is no shared logistics-point
/// pool for a march and a convoy to actually contend over until BL-464 lands
/// one — so the rule ships the same way BL-454's upkeep rates did: real,
/// written down, and inert until something exists to contend over.
///
/// Called from run_economy_step in the slot BL-467's (not-yet-built)
/// battle-discovery phase will occupy — see that call site's comment for why
/// there is nothing to run after yet.
unit_march_tick run_unit_march(world& w, const recipe_registry& reg);

/// BL-430: outcome of a PLAYER-grade recipe-switch attempt (try_switch_recipe).
enum class recipe_switch_result : uint8_t
{
    applied,              ///< b.recipe changed; cost debited, cooldown started.
    invalid,              ///< Unknown recipe id, or already the active recipe.
    on_cooldown,          ///< b.recipe_switch_cooldown has not reached 0 yet.
    insufficient_funds,   ///< The owning corp cannot cover recipe_switch().switch_cost.
    cross_group,          ///< BL-434 retraction: new recipe is a DIFFERENT `group` — refused
                          ///< outright, not priced. See try_switch_recipe's comment.
    depth_locked,         ///< BL-428: the corp has not reached the chain depth the new recipe needs.
                          ///< Closes the retool bypass — see try_switch_recipe's comment.
};

/// Attempt a PLAYER-grade recipe switch on `b`, gated by `economy.recipe_switch`
/// (recipe_registry::recipe_switch()) — a one-off credit cost debited from `corp`
/// and a cooldown before the SAME building may switch again through this seam.
/// The single implementation shared by corp_command's set_recipe verb (which the
/// AI's dial_recipe margin-chase also goes through — same seam, same cost) and the
/// construction_panel management UI's method dropdown.
///
/// Deliberately NOT called by the BL-079 reflex switch (economy_system.cpp's
/// floored-recipe auto-agency): that is sanctioned auto-agency reacting to a
/// sustained loss, not a commitment, and stays free/instant. See
/// recipe_switch_params for the full reasoning.
///
/// A rejection mutates nothing.
///
/// BL-434 retraction (2026-08-16, same session BL-434 landed): a switch to a
/// recipe in a DIFFERENT `group` than `b`'s current recipe is REFUSED outright
/// (recipe_switch_result::cross_group), never priced. Ben's call: switching
/// methods can mean changing to a different building type in effect, and the
/// only sanctioned way to change building type is dismantle + rebuild via the
/// tile selector. The cross-group cost tier this superseded (a steep
/// switch_cost multiplier) lived here for a matter of minutes in the same
/// session — see the removed `cross_group_multiplier` field's former doc
/// comment in recipe_registry.hpp if the history matters.
///
/// @param w             World; `corp`'s balance is debited on success.
/// @param reg           Loaded recipe/economy registry (recipe_switch() reads the gate).
/// @param corp          The owning corporation, debited on success.
/// @param b             The building whose recipe changes.
/// @param new_recipe_id The candidate recipe's absolute id (recipe_registry storage id).
recipe_switch_result try_switch_recipe(world& w, const recipe_registry& reg,
                                       entity_id corp, building_component& b,
                                       uint16_t new_recipe_id);

/// The rate one extraction site would yield at full reserve, BEFORE its stack
/// decay: base_rate × richness × effective workforce × workforce target ×
/// (1 − hazard). The single definition of "one site's nominal draw" — the stack
/// pre-pass sizes the shared taper against a sum of these, `run_extraction` draws
/// one of them, and `estimate_prospective_profit` prices one of them (BL-346), so
/// the figure the taper is sized against is always the figure actually drawn.
///
/// Exported for the estimator; the taper and the stack decay are NOT in it.
///
/// BL-479: the owning corp's earned `extraction_rate` modifiers are folded in
/// HERE (`world::modified_scalar`), inside the single definition, so the taper
/// sizing and the draw cannot disagree about a modified corp's rate. `corp` is
/// defaulted to `null_entity` = "apply no corp's modifiers" — the pre-BL-479
/// figure, byte for byte — which is what the estimator call sites (the Build
/// door's stack-member pricing, generation's body-production census) still
/// pass: no shipped tech carries a modifier yet, and pricing a PROSPECT under
/// another owner's buff would be wrong anyway. The two live tick sites
/// (run_extraction, the stack taper pre-pass) pass the real owner.
///
/// @param w          Read-only world state.
/// @param reg        Loaded recipe/economy registry.
/// @param b          The extraction site (its tile supplies richness and hazard).
/// @param contention The (corp, body) labour-contention scalar to apply.
/// @param corp       Owning corp, for its earned extraction_rate modifiers;
///                   `null_entity` applies none (every estimator's default).
/// @return           Units per tick, before stack decay and before depletion taper.
float extraction_nominal(const world& w, const recipe_registry& reg,
                         const building_component& b, float contention,
                         entity_id corp = null_entity);

/// The BL-181 per-building workforce-dial solver: the workforce target (0–200,
/// step 10) that maximises this building's estimated net this tick against the
/// local market's price response. Exported (BL-202) so the strategic scorer can
/// reuse it as the AI's `set_workforce` estimator — one solver, no drift.
/// Deterministic; reads last tick's market state only.
///
/// @param stack_rank This site's 1-based rank in its tile's stack (BL-193). The
///                   solver's revenue model scales by `stack_output_scalar(rank)`
///                   exactly as `run_extraction` does — without it the dial was
///                   maximised against a curve a stacked site never realises
///                   (BL-346). 1 (the default) is a lone site.
/// @param out_gain Optional. Receives the solver's OWN modelled net at the
///                 returned target minus its modelled net at `b.workforce_target`
///                 — i.e. what moving the dial is actually worth, in the same
///                 units and from the same model that chose the target.
///
///                 Reported as >= 0, but NOT >= 0 by construction: the search
///                 walks a step-10 grid while `b.workforce_target` may sit off
///                 it (the player's `set_workforce` verb accepts any 0–200), so
///                 an off-grid incumbent can genuinely out-score every grid
///                 point. That case is a real "the solver's own answer is worse
///                 than where you already are", and it is floored at 0 rather
///                 than reported, because the caller's only use is a gain to
///                 compare against a hysteresis margin and a negative one means
///                 the same thing as zero there: do not move.
///
///                 It exists because the caller cannot reconstruct this. BL-202's
///                 scorer approximated it as `variable * (proposed − target)/target`
///                 with `variable = revenue − inputs − wages`, which takes its SIGN
///                 from `variable` rather than from the model: a profitable building
///                 could then only ever be scored for raising its target and a
///                 loss-maker only for cutting, so every dial the solver found in
///                 the other direction — the interior optimum it exists to find —
///                 scored negative and was silently discarded.
int solve_workforce_target(const world& w, const recipe_registry& reg,
                           const building_component& b, float contention,
                           int stack_rank = 1, float* out_gain = nullptr);
