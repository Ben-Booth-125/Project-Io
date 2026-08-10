#pragma once

#include "components.hpp"
#include "recipe_registry.hpp"
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
    /// levy (a per-unit charge on raw output). Zero unless a law is enacted that
    /// reaches this corp, which is the shipped default. This is the line that
    /// makes a law OBSERVABLE: a law the player cannot see working is
    /// indistinguishable from an unimplemented one.
    float levies      = 0.0f;

    /// The per-tick balance delta: income less every outflow.
    float net() const { return income - expenditure - maintenance - wages - interest - levies; }
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
    };

    entity_id corp;
    entity_id building;
    kind      what;
    uint16_t  new_recipe = 0;           ///< recipe_switch only: the recipe switched to.
    entity_id tile       = null_entity; ///< built / road_placed: target tile; survey_dispatched: body.
    int       value      = 0;           ///< workforce_set: new target; road_placed: tier.
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

    /// Per (corporation, body): the input quantities a processor could not cover
    /// from its own pool and auto-bought from the market this tick. Resource-indexed.
    std::map<std::pair<entity_id, entity_id>, std::array<float, resource_count>> purchases;

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

/// Inject the elastic nation-substrate demand/supply into body markets (BL-078).
/// For each (nation, body) substrate, demand is a price-elastic per-capita basket
/// (population_weight × economy.substrate.demand_basket × elasticity(price)) and
/// supply is an abstract nation capacity that tracks demand and clears it only
/// partially, capped by deposit-derived capacity — leaving a live margin (the
/// saturation cushion where capacity is ample, a fillable gap where it is short).
/// Called from clear_markets after the per-tick supply/demand reset, before the
/// order-book pass, so substrate is additive to the market quantities. Reads last
/// tick's cleared price, so the model is a stabilising negative feedback.
/// Deterministic — no RNG; a curve.
///
/// @param w   World; market supply/demand arrays are mutated in place.
/// @param reg Loaded registry; supplies the economy.substrate tunables.
void inject_substrate_demand(world& w, const recipe_registry& reg);

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
/// @return    The step report (building states + auto-bought shortfalls).
economy_report run_economy_step(world& w, const recipe_registry& reg);

/// The rate one extraction site would yield at full reserve, BEFORE its stack
/// decay: base_rate × richness × effective workforce × workforce target ×
/// (1 − hazard). The single definition of "one site's nominal draw" — the stack
/// pre-pass sizes the shared taper against a sum of these, `run_extraction` draws
/// one of them, and `estimate_prospective_profit` prices one of them (BL-346), so
/// the figure the taper is sized against is always the figure actually drawn.
///
/// Exported for the estimator; the taper and the stack decay are NOT in it.
///
/// @param w          Read-only world state.
/// @param reg        Loaded recipe/economy registry.
/// @param b          The extraction site (its tile supplies richness and hazard).
/// @param contention The (corp, body) labour-contention scalar to apply.
/// @return           Units per tick, before stack decay and before depletion taper.
float extraction_nominal(const world& w, const recipe_registry& reg,
                         const building_component& b, float contention);

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
int solve_workforce_target(const world& w, const recipe_registry& reg,
                           const building_component& b, float contention,
                           int stack_rank = 1);
