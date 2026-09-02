#pragma once

#include "components.hpp"
#include "recipe_registry.hpp"
#include "battle_system.hpp" // battle_dispatch (BL-467/BL-468)
#include "nation_ai.hpp"     // nation_scorer_report (BL-542; Sprint N3 T4)
#include "nation_budget.hpp" // budget_claim, national_budget_tick (BL-537; Sprint N3 T4)
#include "nation_step.hpp"   // earmark_result (Sprint N3 T6)
#include "space_programme.hpp" // space_purchase (BL-644)
#include "network_upkeep.hpp"  // network_purchase (BL-643)
#include "corp_command.hpp"    // firm_exit_record (BL-743)
#include "logistics.hpp"     // lp_pool_map (BL-596/BL-597, shared active+passive LP pool)
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

    /// BL-537 (Sprint N3): credits received from a nation this tick — the
    /// national budget's direct transfers to this corp, summed from
    /// `economy_report::national_budget.transfers` — BEFORE the earmarked
    /// spend that leaves the same tick. A `public_exploration` credit is paid
    /// in full and then spent in full by the survey dispatch it was for (NR-568:
    /// earmarked, not a top-up), so on that line the inflow here and the
    /// outflow of the dispatch net to zero across the tick; the ledger shows
    /// both rather than neither. The one INFLOW that is not market income.
    /// A COMPLETED space-programme purchase (BL-644) folds here too: the state
    /// paid for goods it consumed, the credit stays on the balance, and this
    /// is the line that explains it.
    float subsidies   = 0.0f;

    /// The per-tick balance delta: income less every outflow.
    float net() const
    {
        return income + subsidies - expenditure - maintenance - wages - interest - levies - upkeep;
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

    /// BL-654 ATTRIBUTION MIRROR: the subset of `wants` contributed by the two
    /// upkeep passes (`run_unit_upkeep`, `run_building_upkeep`). Every figure
    /// here is ALSO in `wants`, which stays the one register clearing reads —
    /// this map is never summed into `mc.demand` and is never paid against.
    ///
    /// It exists because `wants` deliberately merges its consumers (the market
    /// does not care who bid), and demand_census recovers the construction half
    /// from world state and calls the remainder "processing". Before BL-654 that
    /// remainder was exactly processing; now the upkeep bid is in there too, and
    /// with no tag the census would report the Industry channel as processing
    /// demand — a report about a different world, which is the failure that
    /// census exists to prevent.
    ///
    /// Same key type and same std::map as `wants`, for the same
    /// sorted-accumulation reason.
    std::map<std::pair<entity_id, entity_id>, std::array<float, resource_count>> upkeep_wants;

    /// Per (corporation, body): the pool-level workforce scarcity figure this
    /// tick — `min(1, supply/demand)`, rescaled by habitability efficiency after
    /// production. Since BL-614 (wage competition) this is a REPORTING aggregate
    /// (the economy panel's number, and the wage fallback for a building
    /// `building_labour` below never saw); the scalar actually applied to each
    /// building is per-building, in `building_labour`. Below 1.0 means the pool
    /// is contended and allocation ran by offered wage
    /// (docs/economy/POPULATION.md § Contention).
    std::map<std::pair<entity_id, entity_id>, float> workforce_contention;

    /// BL-614 (wage competition): per BUILDING, the labour-allocation factor
    /// applied this tick — the fraction of `workforce_assigned` the pool
    /// granted, ordinary and qualified (BL-613) constraints folded together,
    /// rescaled by habitability efficiency after production (the same rescale
    /// `workforce_contention` gets). One entry per completed producing building
    /// (extraction / processing); 1.0 when its pools are uncontended. When a
    /// pool is contended, scarce labour goes to the buildings offering the
    /// higher wage — offered wage = base_wage × (1 + wage_bid), descending,
    /// then building id ascending — so entries differ ACROSS one (corp, body)
    /// pool, which is what the uniform proportional scalar could never do. Read
    /// by the budget step (wages at the offered rate on this allocation) and by
    /// estimate_building_profit; absent keys read as the (corp, body) aggregate.
    std::map<entity_id, float> building_labour;

    /// BL-613 (qualification fraction): per (NATION, body), the qualified-labour
    /// contention scalar applied this tick — `min(1, qualified supply / qualified
    /// demand)`, where qualified supply is `nation_component::qualification` × the
    /// labour the nation's population centres contribute on that body, and
    /// qualified demand sums `workforce_assigned × recipe.qualified_workforce`
    /// over the buildings running qualified methods on the nation's tiles there.
    /// A building running such a method is throttled by this factor BESIDE the
    /// ordinary (corp, body) scalar above — a second factor, same shape
    /// (docs/economy/POPULATION.md § Qualification). Keyed by nation, not corp:
    /// the qualified pool is national, so every corp drawing deep labour inside
    /// one nation contends for the same heads. Empty when no authored recipe
    /// carries a `qualified_workforce` requirement (every hand-built harness).
    std::map<std::pair<entity_id, entity_id>, float> qualified_contention;

    /// Per-body mean habitability aggregate (BL-048): weighted average of all
    /// population-centre tiles on the body. 0.0 = uninhabitable, 1.0 = full.
    std::map<entity_id, float> body_habitability;

    /// Per-corporation budget breakdown for this tick (BL-072): the four+1 flows
    /// netting to the balance delta. Populated by apply_budget when given a
    /// breakdown sink (app's live loop); empty under the headless harnesses, which
    /// do not request it. Read by the Balance Ledger / Economy panel.
    std::map<entity_id, corp_budget> budgets;

    // ---- The nation spines (BL-537 / BL-542; Sprint N3 slice 1) -----------
    // Three records the national pass leaves on the tick's report, so a surface
    // can see a nation act the way it already sees a corp act. Produced in tick
    // order: the strategic tier emits `budget_claims` while it scores; the
    // nation step (`run_nation_step`, after `apply_budget`) scores the nations
    // into `nation_scores`, runs the budget over the claims, and leaves the
    // pass's own report in `national_budget`. Each is empty on a tick that
    // produced nothing; none is persisted.

    /// Claims on a nation's budget raised THIS tick, in emission order. Two
    /// producers: `run_corp_strategic_step`'s cash gate — a rival whose
    /// top-scoring survey it could not afford asks its home nation to fund that
    /// survey in full (`public_exploration`, subject = the body), at most one
    /// per corp per evaluation — and `derive_space_programme_claims` (BL-644,
    /// inside `run_nation_step`), the state's own purchase claims. Emission
    /// order is each producer's sorted walk, and the budget pass re-sorts into
    /// its own order anyway.
    std::vector<budget_claim> budget_claims;

    /// What the national budget pass did this tick: per-nation / per-line
    /// detail, the total moved, and every transfer (who paid whom, for what).
    /// "Who is paying me" (BL-555) reads `transfers`; the earmarked dispatch
    /// walks the `public_exploration` entries.
    national_budget_tick national_budget;

    /// What the nation scorer did this tick: the term breakdown for every
    /// nation DUE on the cadence, plus the coverage counts that keep a sweep
    /// from passing vacuously. BL-558 renders it.
    nation_scorer_report nation_scores;

    /// What each paid EARMARK did this tick (Sprint N3 T6): the survey a nation
    /// bought for a corp, dispatched or clawed back. An earmark is not a subsidy
    /// — it leaves the corp's balance where it found it — so it is reported here
    /// rather than folded into `budgets[corp].subsidies`. BL-555's line.
    std::vector<earmark_result> earmarks;

    /// What the space programme did this tick (BL-644): every state purchase
    /// intent, from "the share could buy a lump" through funded (the budget
    /// paid it) to completed (the goods left the supplier's pool and ceased to
    /// exist — the satellite launched). The State demand channel's census
    /// surface: a completed row is realised state demand for its `resource`
    /// and `quantity`; an unfunded row is demand the treasury could not yet
    /// cover. In emission order (ascending nation, then the fixed good order).
    std::vector<space_purchase> space_purchases;

    /// What network upkeep did this tick (BL-643): every Infrastructure-channel
    /// purchase intent — the material bill each nation's road/hub network asked
    /// for, what the `logistics_maintenance` share actually funded (pro rata,
    /// unlike the space programme's lumps), and what left the supplier's pool
    /// and ceased to exist. The census surface: `quantity` is the bill,
    /// `drawn` the realised consumption, and the gap is demand the budget
    /// could not pay. In emission order (ascending nation, stone then timber).
    std::vector<network_purchase> network_purchases;

    /// Which firms the insolvency wind-up erased this tick (BL-743), with the
    /// written-off balance and what was liquidated. Report-only — the lapse
    /// and census read it; nothing feeds it back into the sim.
    std::vector<firm_exit_record> firm_exits;

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
/// @param shared_lp_pools BL-597: forwarded to `run_unit_march`'s own
///        parameter of the same name — see that function's doc comment. Null
///        (the default) keeps every existing caller's behaviour byte-for-byte
///        unchanged (a private, always-fresh pool per call, as BL-596 built
///        it); the real per-tick driver (app.cpp/main.cpp) passes the SAME
///        `lp_pool_map` it handed `dispatch_convoys` moments earlier this
///        tick, so active and passive draws genuinely contend.
/// @return    The step report (building states + auto-bought shortfalls).
economy_report run_economy_step(world& w, const recipe_registry& reg,
                                bool spectating = false,
                                lp_pool_map* shared_lp_pools = nullptr);

// ---------------------------------------------------------------------------
// BL-617 — the population migration pass
// ---------------------------------------------------------------------------

/// What one run of the migration pass did. Pure observability, the
/// unit_upkeep_tick shape — nothing reads it to make a decision.
struct migration_tick
{
    int   moved           = 0;    ///< Heads (thousands) that changed centre this tick.
    int   flows           = 0;    ///< Donor→receiver pair flows that moved at least one head.
    int   gated_closed    = 0;    ///< Pair flows zeroed by a hostile stance gate.
    float qualified_moved = 0.0f; ///< Qualified heads (thousands) that crossed a nation border.
};

/// BL-617's per-tick migration resolution (docs/economy/POPULATION.md
/// § Migration). Runs inside run_economy_step after the growth/decline pass;
/// exported so a harness can drive it directly with a hand-built wage map.
///
/// PER BODY: centres are snapshotted in ascending id, each with an
/// attractiveness `habitability + wage_weight × clearing_wage(nation, body)`
/// (the wage term is the allocation-weighted mean OFFERED wage the BL-614 wage
/// pass actually granted labour at, per (nation, body); 0 where no labour
/// cleared). Every centre below the body's mean attractiveness emits
/// `pop × rate_permille / 1000` heads (integer, capped at pop − 1, so a centre
/// never empties), split across the above-mean centres in proportion to their
/// excess attractiveness — floor division; the remainder stays home.
///
/// BETWEEN NATIONS each pair flow is STANCE-GATED against the existing stance
/// store (stance.hpp, read with NATION entity ids — nation stance rides the
/// same tables; nothing declares nation rows yet, so live flows clear at the
/// neutral throttle): hostile either direction closes the flow (×0), a
/// friendship row opens it fully (×1000), otherwise the neutral permille
/// applies. Same-nation moves are ungated. A flow with a stateless side is
/// neutral-gated (no treaty is possible with nobody).
///
/// BRAIN DRAIN: migrants carry `min(1, origin_q × qualified_selectivity)`
/// qualification each. A cross-border flow debits the origin nation's ledger
/// (fraction × centre-headcount) and credits the destination's, conserving
/// qualified heads exactly (the debit is clamped to what the origin actually
/// holds); each touched nation's `qualification` fraction is re-derived from
/// its post-flow ledger. Same-nation moves touch no ledger.
///
/// Deterministic: sorted bodies, sorted centres, integer head arithmetic, all
/// float accumulation in fixed order, no RNG anywhere.
///
/// @param clearing_wage Per (nation, body): the allocation-weighted mean paid
///        wage from this tick's wage pass. run_economy_step computes and
///        passes its own; a harness may hand-build one.
migration_tick run_population_migration(world& w, const recipe_registry& reg,
    const std::map<std::pair<entity_id, entity_id>, float>& clearing_wage);

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
///     negative. **BL-654: what the pool cannot cover is then BID onto the
///     unit's local market and paid for** — the same single path
///     `run_building_upkeep` takes, never a second mechanism — unless the good
///     prices above the buyer's reservation ceiling
///     (`price_band_params::reservation_mult`), in which case the unit declines
///     to buy. Only what is short after BOTH marks the unit unmet.
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
///
/// @param report BL-654: the shortfall bid lands in `wants` (the price signal),
///               the fill in `purchases` (what the corp is billed for) and a
///               copy of the bid in `upkeep_wants` (attribution only). With
///               `reservation_mult` at its 0 default nothing is ever written.
unit_upkeep_tick run_unit_upkeep(world& w, const recipe_registry& reg, economy_report& report);

// ---------------------------------------------------------------------------
// BL-641 — the building pass
// ---------------------------------------------------------------------------

/// What one run of the building-upkeep pass did. Pure observability, the
/// `unit_upkeep_tick` shape — nothing reads it to make a decision, so recording
/// it cannot perturb determinism.
struct building_upkeep_tick
{
    int buildings = 0; ///< Completed, non-decommissioned buildings the pass drew for.
    int drawing   = 0; ///< Of those, the ones whose type/band authored a non-zero basket.
    int unmet     = 0; ///< Buildings whose goods draw could not be met from the pool.
    int weakened  = 0; ///< Buildings whose supply factor fell this tick.
    int recovered = 0; ///< Buildings whose supply factor rose this tick.
};

/// BL-641's building pass — the GOODS half of a building's upkeep, and the same
/// decay rule the unit pass applies to the other kind of asset. Runs inside
/// run_economy_step beside `run_unit_upkeep`; exported so a harness can drive it
/// directly. Design: FINANCE.md § Upkeep is credits AND goods — for buildings
/// too; the demand channel it fills is MARKETS.md § Demand channels (Industry).
///
/// A unit pays credits AND a goods vector; a building paid credits only. That
/// asymmetry was an omission, and it is the largest single reason the goods
/// roster has more producers than consumers — nothing consumed on a scale that
/// GROWS with the world. This makes every firm a consumer, so the sink scales
/// with how much industry exists rather than with an authored weight.
///
/// Per building, in ASCENDING BUILDING ID:
///
///  1. WHO DRAWS. A building draws only once it is COMPLETE
///     (`ticks_remaining == 0`) and not decommissioned. Under construction it is
///     already drawing materials through the construction channel, and charging
///     it operating goods too would double-count the same building; torn down it
///     is not operating at all. Its owner is the corp whose `assets` name it.
///
///  2. THE GOODS DRAW. `building_upkeep_goods` resolves the per-type basket for
///     the registry's CAMPAIGN BAND — an ancient workshop runs on tools and
///     planks, an industrial one on machinery and electronics — and each good is
///     drawn from the owner's pool ON THE BUILDING'S OWN BODY. The pool can be
///     empty, so the draw takes what is there and NEVER goes negative; a short
///     draw marks the building unmet.
///
///     THE ORDER IS LOAD-BEARING, exactly as it is for units: two buildings of
///     one corp on one body draw the same stock, so the visit order decides
///     which one goes short. `w.buildings` is unordered, so ids are sorted.
///
///  3. THE SHORTFALL RULE IS THE SAME RULE. An unmet draw subtracts
///     `supply_decay_permille` from the building's `supply_factor_permille`; a
///     met draw adds `supply_recovery_permille` back, ceilinged at 1000. It
///     NEVER destroys, idles or decommissions the building — a factory short of
///     its tools runs badly, it does not vanish. What "runs badly" means is
///     `building_supply_scalar` folded into nominal output (components.hpp).
///     Deterministic scalar arithmetic, no RNG.
///
/// The CREDIT half is deliberately not here — it stays in `maintenance` and
/// `wages`, which is where the money loop already carries it (budget_system).
///
/// Inert at the shipped-zero rates: every `building_upkeep_params` rate defaults
/// to zero, which means no basket resolves non-zero, no pool is touched (not
/// even created), no draw can go unmet, and no supply factor ever moves.
///
/// BL-654: what the pool cannot cover is BID onto the building's local market
/// and paid for — the same single path `run_unit_upkeep` takes — unless the good
/// prices above the buyer's reservation ceiling, in which case the building goes
/// without and step 3's shortfall rule applies unchanged. This is what makes the
/// Industry channel a PRICE SIGNAL rather than a silent sink: the draw's want
/// reaches `market_component::demand`, so a rival scoring the building that
/// supplies it finally has a reason to (MARKETS.md § Demand channels).
///
/// @param report Same three registers `run_unit_upkeep` writes.
building_upkeep_tick run_building_upkeep(world& w, const recipe_registry& reg,
                                         economy_report& report);

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

    /// BL-596: orders refused outright this tick for want of active Logistic
    /// Points at the unit's nearest anchor (no reachable anchor at all, or
    /// that anchor's pool already exhausted by higher-priority draws this
    /// tick). A refusal MUTATES NOTHING — order, position, next_index,
    /// progress and the owning corp's balance are all left exactly as they
    /// were — so this is the only signal a caller has that the tick happened
    /// and did nothing; surfacing it here follows the marching/arrived/
    /// recomputed counter style rather than inventing a narration pathway
    /// (nothing narrates unit movement today; see the doc comment below).
    int refused_no_lp = 0;
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
/// corp's units follow, in ascending unit id within each group. This order is
/// what BL-596 (active Logistic Points) resolves contention with: it now IS
/// the "deterministic priority over a sorted set" LOGISTICS.md's Refusal
/// section calls for, reused rather than re-invented.
///
/// BL-596 (LOGISTICS.md § Logistic Points): before spending march points, a
/// unit draws against its NEAREST supply anchor's active-LP pool for this
/// tick (`active_lp_anchor_pools`, logistics.hpp — a fresh, uncached map per
/// body, decremented as this pass's units draw from it; never persisted,
/// because LP is a per-tick RATE, never a stock). "Nearest" is NOT specified
/// by LOGISTICS.md for a mid-route unit — this is a reasoned interpretation:
/// the anchor with the lowest `intra_body_path` cost from the unit's CURRENT
/// position, among the body's anchor set, reusing the exact anchor
/// enumeration `body_reach_field` seeds its Dijkstra from (never a second
/// distance model — BL-325 ruling 3). An anchor with insufficient pool (or a
/// body with no reachable anchor at all) REFUSES the unit's move outright
/// this tick — no partial draw, no order/position/balance mutation,
/// `unit_march_tick::refused_no_lp` counts it. A granted draw both consumes
/// the anchor's pool AND debits the owning corp's credit balance
/// (`active_lp_credit_per_unit_distance`) — LP is the cap, credits are the
/// separate price (LOGISTICS.md rule 1). A nation-owned garrison (no
/// `w.corporations` entry, BL-571) still draws against the LP cap but pays
/// no credits — its upkeep is a nation-budget line, not a corp balance.
///
/// At peace this now DOES change something observable where it did not
/// before: BL-464's shared pool exists (as of BL-596, for the active half
/// only — BL-597 lands the passive convoy half in the same sprint), so two
/// corps' units converging on one anchor genuinely contend for it, resolved
/// by the visit order above.
///
/// Called from run_economy_step in the slot BL-467's (not-yet-built)
/// battle-discovery phase will occupy — see that call site's comment for why
/// there is nothing to run after yet.
///
/// @param shared_lp_pools BL-597 (LOGISTICS.md § Logistic Points, passive
///        convoy admissibility): when non-null, this pass draws against —
///        and leaves its draws visible in — the CALLER'S `lp_pool_map`
///        rather than a private one, so a passive draw (`commit_convoy`,
///        supply_system.cpp) earlier THIS tick against the same instance is
///        already reflected in the pool a march contends for, and vice
///        versa. Null (the default) reproduces BL-596's original behaviour
///        exactly: a fresh, private map built and discarded within this one
///        call, so every existing caller (every harness, and any call site
///        not yet updated to share a pool) is unaffected byte-for-byte.
unit_march_tick run_unit_march(world& w, const recipe_registry& reg,
                               lp_pool_map* shared_lp_pools = nullptr);

/// BL-430: outcome of a PLAYER-grade recipe-switch attempt (try_switch_recipe).
enum class recipe_switch_result : uint8_t
{
    applied,              ///< b.recipe changed; cost debited, cooldown started.
    invalid,              ///< Unknown recipe id, or already the active recipe.
    on_cooldown,          ///< b.recipe_switch_cooldown has not reached 0 yet.
    insufficient_funds,   ///< The owning corp cannot cover recipe_switch().switch_cost.
    cross_group,          ///< BL-434 retraction: new recipe is a DIFFERENT `group` — refused
                          ///< outright, not priced. See try_switch_recipe's comment.
    // BL-692 removed `depth_locked` (BL-428): chain depth no longer gates a
    // retool, so the value became unreachable.
    tech_locked,          ///< BL-588: the corp has not earned the tech that unlocks the new recipe.
                          ///< The only method lock left at this door — see try_switch_recipe.
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
