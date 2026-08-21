#include "corp_ai.hpp"

#include "budget_system.hpp"  // compute_building_opex, body_mean_habitability
#include "building_profit.hpp"
#include "economy_system.hpp"  // economy_report, agency_event, solve_workforce_target
#include "logistics.hpp"       // body_reach_field (BL-379: warm before the reach-checked muster scan)
#include "market_clearing.hpp" // market_for_tile
#include "placement_rules.hpp"
#include "recipe_registry.hpp"
#include "survey_system.hpp"
#include "unit_roster.hpp"
#include "world.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <ostream>
#include <string>
#include <utility>

namespace {

/// splitmix64 — a small, well-mixed integer hash (public domain constants).
uint64_t mix64(uint64_t x)
{
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

entity_id tile_body(const world& w, entity_id tile)
{
    const auto it = w.tiles.find(tile);
    return (it != w.tiles.end()) ? it->second.body : null_entity;
}

// ---------------------------------------------------------------------------
// World history log (BL-208) — decision + agency narration for the strategic
// tier. Deliberately self-contained (no history_log.hpp include): both new
// world_history_entry fields it needs (history_topic, world::history_log) are
// already visible via world.hpp, which this file already includes, so wiring
// the log here adds no new translation-unit dependency for the existing
// hand-written tools/verify/README.md recipes that link corp_ai.cpp.
// ---------------------------------------------------------------------------

/// Body tag for a corp_command's log entry, resolved BEFORE apply_corp_command
/// runs (a demolish erases its subject from w.buildings, so resolving after
/// would silently lose the tag).
entity_id resolve_command_body(const world& w, const corp_command& cmd)
{
    switch (cmd.verb)
    {
        case corp_verb::build:
        case corp_verb::place_road:
        case corp_verb::hire_unit: // keys off cmd.tile, same as build/place_road.
            return tile_body(w, cmd.tile);
        case corp_verb::survey:
        case corp_verb::place_sell_order:
            return cmd.subject; // subject IS the body for these verbs
        case corp_verb::remove_sell_order:
        {
            // The order names no body directly; resolve it from the book while
            // the order still exists (this runs before the command applies).
            for (const sell_order& o : w.sell_orders)
                if (o.id == cmd.order)
                    return o.body;
            return null_entity;
        }
        default:
        {
            const auto bit = w.buildings.find(cmd.subject);
            return (bit != w.buildings.end()) ? tile_body(w, bit->second.tile) : null_entity;
        }
    }
}

/// One-line narration of a strategic decision for the log's `event` column.
std::string narrate_corp_decision(const corp_decision& d)
{
    char buf[96];
    std::snprintf(buf, sizeof buf, " (%s, score %.2f vs %.2f)",
                  corp_decision_reason_label(d.reason), d.winning_score, d.runner_up);
    std::string s = "Corp ";
    s += std::to_string(d.corp);
    s += " ordered a ";
    s += corp_verb_label(d.command.verb);
    s += buf;
    return s;
}

const char* agency_kind_label(agency_event::kind k)
{
    switch (k)
    {
        case agency_event::kind::recipe_switch:     return "switched recipe";
        case agency_event::kind::idled:              return "idled a loss-making building";
        case agency_event::kind::built:               return "built a new facility";
        case agency_event::kind::demolished:          return "demolished a facility";
        case agency_event::kind::workforce_set:       return "adjusted workforce";
        case agency_event::kind::resumed:             return "resumed an idled facility";
        case agency_event::kind::road_placed:         return "placed a road";
        case agency_event::kind::survey_dispatched:   return "dispatched a survey";
        case agency_event::kind::hired:               return "raised a unit";
        case agency_event::kind::order_placed:        return "listed stock for sale";
        case agency_event::kind::order_removed:       return "withdrew a sell order";
    }
    return "took an agency action";
}

/// One-line narration of an agency_event for the log's `event` column.
std::string narrate_agency_event(const agency_event& ev)
{
    std::string s = "Corp ";
    s += std::to_string(ev.corp);
    s += " ";
    s += agency_kind_label(ev.what);
    return s;
}

/// Price the market at `tile` would pay for resource `r` right now — last
/// tick's cleared price, base before the first clear, 0 with no market.
float local_price(const world& w, entity_id tile, std::size_t r)
{
    const entity_id mid = market_for_tile(w, tile);
    if (mid == null_entity)
        return 0.0f;
    const auto mit = w.markets.find(mid);
    if (mit == w.markets.end())
        return 0.0f;
    const market_component& m = mit->second;
    return (m.price[r] > 0.0f) ? m.price[r] : m.base_price[r];
}

/// Per-batch margin of `recipe_id` at the prices of the market serving `tile`.
float recipe_margin(const world& w, const recipe_registry& reg,
                    entity_id tile, uint16_t recipe_id)
{
    const recipe* rc = reg.get_recipe(recipe_id);
    if (!rc)
        return 0.0f;
    float m = 0.0f;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (rc->outputs[r] > 0.0f) m += rc->outputs[r] * local_price(w, tile, r);
        if (rc->inputs[r]  > 0.0f) m -= rc->inputs[r]  * local_price(w, tile, r);
    }
    return m;
}

/// The resource a recipe mostly makes — its largest output. Names what a
/// processor is FOR, which is the argument the glut forecast and the placement
/// rules both want (BL-439). Ties resolve to the lowest resource index, so the
/// answer is a function of the recipe rather than of iteration order.
resource_type primary_output(const recipe& rc)
{
    std::size_t best = 0;
    float       q    = 0.0f;
    for (std::size_t r = 0; r < resource_count; ++r)
        if (rc.outputs[r] > q) { q = rc.outputs[r]; best = r; }
    return static_cast<resource_type>(best);
}

/// Whether `tile` is revealed under its body's survey state (geographic fog).
bool tile_surveyed(const world& w, const tile_component& tc)
{
    const auto bit = w.bodies.find(tc.body);
    if (bit == w.bodies.end())
        return false;
    const body_component& b = bit->second;
    return survey_tile_visible(b.survey, b.grid_width, b.grid_height, tc.grid_x, tc.grid_y);
}

/// One scored candidate action awaiting selection.
struct candidate
{
    corp_command          cmd;
    float                 score  = 0.0f;
    float                 spend  = 0.0f; ///< Committed capex (build cost / survey cost).
    corp_decision_reason  reason = corp_decision_reason::best_build;
    corp_priority_bucket  bucket = corp_priority_bucket::nice_to_have;
};

/// Deterministic ordering: bucket asc FIRST (a lower bucket may never starve
/// a higher one — AI_OPPONENT.md §2B), then score desc, then verb, subject,
/// tile as the existing BL-202 tie-break.
bool candidate_before(const candidate& a, const candidate& b)
{
    if (a.bucket != b.bucket) return a.bucket < b.bucket;
    if (a.score != b.score) return a.score > b.score;
    if (a.cmd.verb != b.cmd.verb) return a.cmd.verb < b.cmd.verb;
    if (a.cmd.subject != b.cmd.subject) return a.cmd.subject < b.cmd.subject;
    return a.cmd.tile < b.cmd.tile;
}

/// Strategy weight for a verb family under the corp's industrial focus —
/// the specialist premise (CORPORATION_GENERATION.md) as a legible bias.
float focus_weight(industrial_focus f, corp_verb v)
{
    switch (v)
    {
        case corp_verb::build:
            return (f == industrial_focus::extraction) ? 1.25f
                 : (f == industrial_focus::processing) ? 1.00f : 0.90f;
        case corp_verb::survey:
            return (f == industrial_focus::extraction) ? 1.20f : 1.00f;
        default:
            return 1.0f; // dials are focus-neutral corrections
    }
}

/// What a building costs per tick while it sits idle — the only outflow that
/// survives decommissioning (BL-049's fixed material share of maintenance; the
/// labour share and all wages stop).
///
/// This is the correct counterfactual for BOTH the idle and the resume gain, and
/// it is deliberately obtained by asking `compute_building_opex` rather than by
/// re-deriving it: the split lives in one place (budget_system.cpp), the budget
/// charges exactly this figure, and a constant copied here would be a second
/// definition to keep in step. Probes a copy so nothing is mutated.
float idle_maintenance(const world& w, const recipe_registry& reg, const building_component& b)
{
    building_component probe = b;
    probe.decommissioned     = true;
    const entity_id body     = tile_body(w, b.tile);
    const float     hab      = (body != null_entity) ? body_mean_habitability(w, body) : 1.0f;
    return compute_building_opex(probe, reg.economics(b.type), 1.0f, hab).maintenance;
}

} // namespace

const char* corp_verb_label(corp_verb v)
{
    switch (v)
    {
        case corp_verb::build:         return "build";
        case corp_verb::demolish:      return "demolish";
        case corp_verb::set_recipe:    return "recipe change";
        case corp_verb::set_workforce: return "workforce change";
        case corp_verb::idle:          return "idle";
        case corp_verb::resume:        return "resume";
        case corp_verb::place_road:    return "road placement";
        case corp_verb::survey:        return "survey";
        case corp_verb::hire_unit:     return "hire unit";
        case corp_verb::place_sell_order:   return "sell order";
        case corp_verb::remove_sell_order:  return "sell-order withdrawal";
        case corp_verb::set_workforce_auto: return "workforce auto";
        // BL-350's procurement trio. Absent, these fell through to the generic
        // "action" below, so a contract decision would narrate itself into the
        // world history log as "Corp 34590 ordered a action". Exhaustive now —
        // though -Wswitch had been warning about it all along, under -Wall
        // without -Werror, so the omission was visible on every build.
        case corp_verb::request_quote:      return "quote request";
        case corp_verb::accept_quote:       return "quote acceptance";
        case corp_verb::cancel_contract:    return "contract cancellation";
        // BL-452's convoy pair. The scorer does not emit either verb (the
        // auto-dispatcher still runs the AI's logistics), but an agent on the
        // MCP seam can, and this label is what the world history log narrates
        // whatever the seam applied.
        case corp_verb::dispatch_convoy:    return "convoy dispatch";
        case corp_verb::hold_convoy:        return "convoy hold";
    }
    return "action";
}

const char* corp_decision_reason_label(corp_decision_reason r)
{
    switch (r)
    {
        case corp_decision_reason::best_build:     return "best available build site";
        case corp_decision_reason::dial_workforce: return "workforce margin";
        case corp_decision_reason::dial_recipe:    return "recipe margin";
        case corp_decision_reason::dial_idle:      return "sustained losses";
        case corp_decision_reason::dial_resume:    return "now profitable";
        case corp_decision_reason::survey_expand:  return "discovery within budget";
        case corp_decision_reason::hire_available: return "roster row available";
        case corp_decision_reason::trade_surplus:  return "stock piled up past the hold threshold";
    }
    return "unspecified";
}

float corp_personality_jitter(entity_id corp, uint64_t personality_seed)
{
    const uint64_t h = mix64(personality_seed ^ (static_cast<uint64_t>(corp) * 0x9e3779b97f4a7c15ull));
    // Map to [0.9, 1.1]: constant per campaign, deterministic by construction.
    return 0.9f + 0.2f * static_cast<float>(h % 10000ull) / 10000.0f;
}

float corp_reserve_floor(const world& w, const recipe_registry& reg,
                         entity_id corp, const corp_ai_params& p)
{
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return p.floor_constant;
    float wage_bill = 0.0f;
    for (const entity_id bid : cit->second.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const building_component& b = bit->second;
        if (b.decommissioned || b.ticks_remaining > 0)
            continue;
        wage_bill += reg.economics(b.type).base_wage * b.workforce_assigned;
    }
    // One tick is one quarter, so wage_bill IS the quarterly wage bill.
    return std::max(p.floor_constant, p.floor_wage_mult * wage_bill);
}

corp_priority_bucket bucket_for_reason(corp_decision_reason reason)
{
    switch (reason)
    {
        case corp_decision_reason::dial_idle:
            return corp_priority_bucket::must_have; // stops wage/maintenance bleed now
        case corp_decision_reason::dial_recipe:
        case corp_decision_reason::dial_workforce:
        case corp_decision_reason::dial_resume:
            return corp_priority_bucket::should_have; // feeds/tunes a running asset
        case corp_decision_reason::trade_surplus:
            // Should-Have, not Nice-to-Have: listing accumulated stock carries no
            // capex — it BRINGS cash in — so it can never starve a higher bucket,
            // which is the whole test the buckets apply. It is not Must-Have
            // because nothing breaks if the stock sits another quarter.
            return corp_priority_bucket::should_have;
        case corp_decision_reason::best_build:
        case corp_decision_reason::survey_expand:
        case corp_decision_reason::hire_available:
        default:
            return corp_priority_bucket::nice_to_have; // expansion
    }
}

float corp_should_have_buffer(const world& w, const recipe_registry& reg,
                              const economy_report& report, entity_id corp)
{
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return 0.0f;
    float buffer = 0.0f;
    for (const entity_id bid : cit->second.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const building_component& b = bit->second;
        if (b.type != building_type::processing_facility)
            continue;
        if (b.decommissioned || b.ticks_remaining > 0)
            continue;
        const building_profit bp = estimate_building_profit(w, reg, report, bid);
        if (bp.has_data)
            buffer += std::max(0.0f, bp.input_cost);
    }
    return buffer;
}

float forecast_glut_multiplier(const world& w, entity_id tile, resource_type target,
                               float added_rate_per_tick, int horizon_ticks,
                               const corp_ai_params& p)
{
    if (added_rate_per_tick <= 0.0f)
        return 1.0f;
    const entity_id mid = market_for_tile(w, tile);
    if (mid == null_entity)
        return 1.0f; // no market to glut — nothing to forecast against
    const auto mit = w.markets.find(mid);
    if (mit == w.markets.end())
        return 1.0f;
    const market_component& m = mit->second;
    const std::size_t r = static_cast<std::size_t>(target);
    const float demand = m.demand[r]; // PUBLIC aggregate only (BL-068) — same fact a rival sees.
    if (demand <= 0.0f)
        return 1.0f; // no public demand signal to forecast against; do not guess.
    const float horizon = static_cast<float>(std::max(1, horizon_ticks));
    const float projected_supply = m.supply[r] + added_rate_per_tick * horizon;
    const float ratio = projected_supply / demand;
    if (ratio <= p.glut_taper_ratio)
        return 1.0f;
    if (ratio >= p.glut_veto_ratio)
        return 0.0f;
    const float span = std::max(1.0e-6f, p.glut_veto_ratio - p.glut_taper_ratio);
    return 1.0f - (ratio - p.glut_taper_ratio) / span;
}

namespace {
std::vector<entity_id> sorted_corp_ids(const world& w)
{
    std::vector<entity_id> ids;
    ids.reserve(w.corporations.size());
    for (const auto& kv : w.corporations)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

/// A surveyed, deposit-bearing tile ranked as a build-candidate site.
/// Corp-independent: survey state and deposit richness are world facts, not
/// per-corp knowledge (BL-068's activity fog gates market visibility, not
/// geographic survey). Populated once per tick and read by every due corp,
/// rather than rescanned per corp — see BL-253.
struct extraction_site
{
    entity_id     tile;
    float         suitability;
    resource_type target;
};

/// The O(tiles) half of the build-candidate scan, hoisted out of the per-corp
/// loop (BL-253): `run_corp_strategic_step` used to walk every tile for every
/// due corp this tick, an O(corps x tiles) term that dominates the tick at
/// scale (measured: time ~ size^1.28 in BL-250's scaling sweep). Everything
/// this function reads — composition, survey, deposit richness, landform —
/// is a world fact, not per-corp state, so the ranked top-M list is identical
/// for every corp evaluating this tick; only the later can_place_in_world
/// filter (tech gate, logistics reach) and scoring are genuinely per-corp,
/// and both run over this already-truncated top-M list, not the full tile set.
/// Per-resource siting pull from unmet recipe demand (BL-440).
///
/// `wanted` counts the loaded recipes naming the resource as an input — a static
/// property of the recipe graph, so it is identical for every corp and every
/// tick. `sites` counts the extraction sites in the world already NAMED for it,
/// which is what makes the pull self-limiting: the first mine for a starved good
/// halves the bonus, the third quarters it, and a well-supplied resource gets
/// essentially nothing.
///
/// Deliberately keyed on sites-named-for-it rather than on units produced. Since
/// BL-437 a site works every deposit on its tile, so a starved good trickles in
/// as a BY-PRODUCT of mines placed for something richer — which is exactly how
/// coal reached 6.1 units/tick against demand that starved half the economy while
/// looking, to a production-keyed measure, like it was being supplied.
std::array<float, resource_count> input_demand_weights(const world& w,
                                                       const recipe_registry& reg,
                                                       const corp_ai_params& p)
{
    std::array<float, resource_count> weight;
    weight.fill(1.0f);
    if (p.input_demand_pull <= 0.0f)
        return weight; // conversion disabled: pre-BL-440 behaviour

    std::array<int, resource_count> wanted{};
    const int n = reg.recipe_count(building_type::processing_facility);
    for (int i = 0; i < n; ++i)
    {
        const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
        for (std::size_t r = 0; r < resource_count; ++r)
            if (rc.inputs[r] > 0.0f)
                ++wanted[r];
    }

    std::array<int, resource_count> sites{};
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::extraction_site)
            ++sites[static_cast<std::size_t>(b.target_resource)];

    for (std::size_t r = 0; r < resource_count; ++r)
        if (wanted[r] > 0)
            weight[r] = 1.0f + p.input_demand_pull * static_cast<float>(wanted[r])
                                                   / static_cast<float>(1 + sites[r]);
    return weight;
}

std::vector<extraction_site> rank_extraction_sites(const world& w, int top_m,
                                                   const std::array<float, resource_count>& demand_weight)
{
    std::vector<extraction_site> sites;
    sites.reserve(w.tiles.size() / 2); // most land tiles carry some deposit

    // BL-253 follow-on (2026-08-12, the 3x map): tile_surveyed re-looked-up the
    // body in w.bodies for EVERY tile — 45,240 hash lookups per tick on the new
    // grid, to fetch the same handful of bodies over and over. Tiles arrive
    // grouped by body in practice, so a one-entry memo removes almost all of
    // them. Behaviour-identical: the same body yields the same answer.
    entity_id             memo_body = null_entity;
    const body_component* memo_bc   = nullptr;

    for (const auto& [tid, tc] : w.tiles)
    {
        if (placement_rules::is_water_tile(tc.substrate))
            continue;
        if (tc.body != memo_body)
        {
            const auto bit = w.bodies.find(tc.body);
            memo_body = tc.body;
            memo_bc   = (bit == w.bodies.end()) ? nullptr : &bit->second;
        }
        if (memo_bc == nullptr ||
            !survey_tile_visible(memo_bc->survey, memo_bc->grid_width, memo_bc->grid_height,
                                 tc.grid_x, tc.grid_y))
            continue; // geographic fog: unsurveyed tiles are not candidates
        // Terrain affinity: mountains/canyons expose minerals (TILES.md), read
        // here as a mild suitability bonus. Tile-wide, so it is hoisted out of
        // the per-target loop below.
        float affinity = 1.0f;
        if (tc.landform == terrain_landform::mountain ||
            tc.landform == terrain_landform::canyon)
            affinity = 1.15f;

        // EVERY extractable deposit on this tile is a candidate, not just the
        // richest (BL-440). This used to call richest_extractable and emit one
        // site per tile, which meant a resource that is common but rarely
        // dominant was never offered to the scorer AT ALL — it lost every
        // richness comparison it was entered into, so no amount of demand could
        // reach it. Seven wanted recipe inputs sat on 200+ tiles apiece with
        // zero sites named for them; one of them on 16,361 tiles.
        //
        // Pre-selecting the richest was a TILE-LOCAL heuristic answering a
        // WORLD-level question. Enumerating is what lets the demand weight (and
        // the existing net/glut/solvency scoring downstream) decide instead.
        for (const resource_type rt : placement_rules::k_extractable)
        {
            const std::size_t ri   = static_cast<std::size_t>(rt);
            const float       rich = tc.resource_deposit[ri];
            if (rich <= 0.0f)
                continue;
            sites.push_back({tid, rich * affinity * demand_weight[ri], rt});
        }
    }
    // PARTIAL SORT, not a full one (2026-08-12). Only the top M survive the
    // resize below, so sorting all ~18,000 qualifying sites to discard all but
    // ~64 of them was work thrown away. `std::partial_sort` orders exactly the
    // first M under the same comparator and leaves the tail unspecified — which
    // is precisely what `resize` then discards, so the returned list is
    // BIT-IDENTICAL to sort-then-resize. No golden moves.
    const auto cmp = [](const extraction_site& a, const extraction_site& b) {
        if (a.suitability != b.suitability) return a.suitability > b.suitability;
        return a.tile < b.tile;
    };
    const std::size_t keep = std::min(sites.size(), static_cast<std::size_t>(std::max(0, top_m)));
    std::partial_sort(sites.begin(), sites.begin() + static_cast<std::ptrdiff_t>(keep),
                      sites.end(), cmp);
    sites.resize(keep);
    return sites;
}
} // namespace

bool corp_strategic_eval_due(const world& w, entity_id corp, int tick, const corp_ai_params& p)
{
    // The player-corp skip, and the ONE condition that removes it (BL-409).
    //
    // In a played session the prohibition is absolute: the player's corp is
    // never strategically evaluated, so this returns false for it on every
    // tick regardless of cadence (io-standing-rules.md § Determinism & data
    // model). Do not soften that — the standing exceptions are narrow and
    // local (BL-079 reflexes, BL-181's single workforce dial), and none of
    // them reaches this tier.
    //
    // Under `p.spectating` the rule is not excepted, it is *unsubscribed*:
    // the prohibition protects a corp because a human owns it, and a
    // spectated session has no human seat, so there is no owner to protect
    // (Ben, 2026-08-14 — "'who plays your corp' collapses as a question").
    // `world::player_entity` survives only as a camera/ledger anchor.
    // Not a corporation at all: an existence check, not an ownership one, so
    // it binds in both modes.
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return false;
    // The prohibition itself — same shape as the guard in
    // run_corp_strategic_step below, so the two read as one rule.
    if (!p.spectating && (cit->second.is_player || corp == w.player_entity))
        return false;

    const std::vector<entity_id> corp_ids = sorted_corp_ids(w);
    const auto it = std::lower_bound(corp_ids.begin(), corp_ids.end(), corp);
    if (it == corp_ids.end() || *it != corp)
        return false;
    const int k     = std::max(1, p.cadence_k);
    const int index = static_cast<int>(it - corp_ids.begin());
    return (tick % k) == (index % k);
}

void run_corp_strategic_step(world& w, const recipe_registry& reg,
                             economy_report& report, int tick,
                             const corp_ai_params& p)
{
    // Sorted corp ids: the deterministic visit order AND the stable per-corp
    // index the cadence stagger keys on.
    const std::vector<entity_id> corp_ids = sorted_corp_ids(w);

    const int k = std::max(1, p.cadence_k);

    // BL-253: the ranked build-candidate site list is a world fact, not a
    // per-corp one — compute it once for the whole tick rather than once per
    // due corp. Cheap to build unconditionally even on ticks with no due
    // corp (rare, and still O(tiles) once rather than O(tiles) per corp).
    // BL-440: the demand weights are a world fact too (recipe graph + the sites
    // standing in the world), so they are computed once per tick alongside the
    // site ranking they feed, not once per due corp — the same BL-253 hoist.
    const std::array<float, resource_count> demand_weight = input_demand_weights(w, reg, p);
    const std::vector<extraction_site> ranked_sites =
        rank_extraction_sites(w, p.top_m_sites, demand_weight);

    for (std::size_t index = 0; index < corp_ids.size(); ++index)
    {
        const entity_id              corp = corp_ids[index];
        const corporation_component& cc   = w.corporations.at(corp);

        // NEVER act on the player corp — no strategic command, no cooldown
        // bookkeeping, nothing (io-standing-rules.md). This is the guard that
        // makes the prohibition real; the one in corp_strategic_eval_due only
        // reports the schedule.
        //
        // `p.spectating` (BL-409) lifts it, and ONLY it: nothing else in this
        // loop distinguishes the player's corp, so once past here it is scored,
        // built, dialled, surveyed and traded exactly as a rival is. That is
        // the point — a spectated race with one runner standing still is not a
        // demonstration of the AI, and it leaves one specialist corp's worth of
        // demand out of the economy. The prohibition still holds in full for a
        // PLAYED session: `spectating` defaults false and no gameplay path
        // sets it (app.cpp passes the session's own spectator flag).
        if (!p.spectating && (cc.is_player || corp == w.player_entity))
            continue;

        // Staggered cadence (Victoria-3 tick-task idea): due this tick?
        if ((tick % k) != (static_cast<int>(index) % k))
            continue;

        // This corp is evaluating: tick down its buildings' dial cooldowns.
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit != w.buildings.end() && bit->second.ai_cooldown > 0)
                --bit->second.ai_cooldown;
        }

        const float jitter  = corp_personality_jitter(corp, p.personality_seed);
        const float floor_  = corp_reserve_floor(w, reg, corp, p);
        // Should-Have buffer (BL-203): cash reserved for feeding this corp's
        // own running processors, ahead of any Nice-to-Have (build/survey)
        // spend — the concrete "a lower bucket may never starve a higher
        // one" enforcement for the only bucket that carries capex.
        const float should_have_buffer = corp_should_have_buffer(w, reg, report, corp);
        const float nice_to_have_floor = floor_ + should_have_buffer;

        std::vector<candidate> cands;

        // ---- Build candidates: surveyed, deposit-bearing tiles, top-M ------
        // Site ranking is precomputed once per tick above (BL-253); only the
        // per-corp filter (tech gate, logistics reach) and scoring run here.
        {
            // Known accepted divergence (BL-162): estimate_prospective_profit
            // (world/building_profit.hpp) is the same model done properly — it takes
            // opex through compute_building_opex, so it carries habitability-scaled
            // wages and the depletion taper, which the inline scorer below omits.
            // Deliberately NOT switched: it would move AI build scoring, hence world
            // evolution, hence every blessed golden, for no player-visible gain.
            const building_economics& ex = reg.economics(building_type::extraction_site);
            for (const extraction_site& s : ranked_sites)
            {
                // Deliberately checked WITHOUT the seam's logistics-reach budget
                // (BL-379 gave the muster-base candidate parity; this site keeps
                // the default-disabled reach on purpose): enforcing it here would
                // reshuffle which economic builds every rival attempts, hence
                // world evolution, hence every blessed golden — the same
                // no-player-visible-gain call as the BL-162 note above. A pick
                // the seam refuses as out_of_range costs one wasted seam call
                // and no build slot (APPLY THEN COUNT, below).
                if (!placement_rules::can_place_in_world(w, s.tile, building_type::extraction_site, s.target))
                    continue;
                const tile_component& tc  = w.tiles.at(s.tile);
                const std::size_t     ri  = static_cast<std::size_t>(s.target);
                const float           wf  = 0.5f; // construct_building staffs at 0.5
                // BL-436: same conversion as the tick. A raw richness made the
                // scorer expect ~50x the revenue a site actually delivers.
                const float rich          = richness_rate_scalar(ex, tc.resource_deposit[ri]);
                const float price         = local_price(w, s.tile, ri);
                const float revenue       = ex.base_rate * rich * wf * (1.0f - tc.hazard_level) * price;
                const float net           = revenue - ex.maintenance - ex.base_wage * wf;
                if (net <= 0.0f)
                    continue; // never build into an expected loss
                const float capex = std::max(1.0f, ex.build_cost);

                // Predictive spending (BL-203): forecast this build's added
                // supply against the local market's PUBLIC demand over its
                // construction horizon + one clearing pass, so the corp does
                // not build into its own glut. Visibility-honest: reads only
                // market_component.supply/demand, the same public aggregates
                // export_corp_blackboard shows a rival (BL-068).
                const float added_rate = ex.base_rate * rich * wf * (1.0f - tc.hazard_level);
                const int   horizon    = static_cast<int>(ex.build_duration_ticks) + p.forecast_clearing_ticks;
                const float glut       = forecast_glut_multiplier(w, s.tile, s.target, added_rate, horizon, p);
                if (glut <= 0.0f)
                    continue; // forecast hard glut — veto, do not even enumerate

                candidate c;
                c.cmd.tick   = tick;
                c.cmd.corp   = corp;
                c.cmd.verb   = corp_verb::build;
                c.cmd.tile   = s.tile;
                c.cmd.type   = building_type::extraction_site;
                c.cmd.target = s.target;
                // BL-417 step 1: this used to read `net / payback` with
                // `payback = capex / net` — which is `net^2 / capex`. It looked
                // like capital efficiency and behaved like a margin bias:
                // doubling the margin quadruples the score, doubling the cost
                // only halves it. Written out here so the code stops lying to
                // the next reader.
                //
                // The bias is RETAINED, deliberately. focus_weight, jitter and
                // the glut multiplier were all tuned against this curve, and
                // every blessed golden records a world evolved under it —
                // replacing it with an explicit linear metric is a re-tune plus
                // a golden reshuffle, which is BL-417 step 2 and Ben's call.
                //
                // NOT algebraically free in float: `net / (capex / net)` and
                // `net * net / capex` round differently. Measured against the
                // MSVC build (ai_skill_harness 5 seeds, spectator_determinism
                // state_hash) before landing; byte-identical both sides. See
                // BL-417 `step_1_landed` and requirements group
                // quadratic-build-score-honest.
                c.score  = (net * net / capex) * focus_weight(cc.focus, corp_verb::build) * jitter * glut;
                c.spend  = capex;
                c.reason = corp_decision_reason::best_build;
                c.bucket = bucket_for_reason(c.reason);
                cands.push_back(c);
            }
        }

        // ---- Build candidates: a processing facility on the corp's own ground -
        //
        // BL-439. Until 2026-08-17 this scorer emitted `corp_verb::build` from
        // exactly two places — the extraction loop above and the muster base
        // below — so a rival owned only the processors it was GENERATED with,
        // for the whole campaign. Three shipped claims rested on that not being
        // true (NR-265/266/267): "the AI prefers mines" read as a scoring-curve
        // artefact when it was structural; BL-436's calibration narrative
        // explained a collapse by corps building processors, which they cannot
        // do; and BL-428's chain-depth ladder had exactly one climber.
        //
        // The seam was never the problem: corp_command/construct_building build
        // a processor, recipe and all, when a command asks for one (BL-388). The
        // scorer simply never asked.
        {
            const building_economics& pe = reg.economics(building_type::processing_facility);

            // SITING. A processor is not sited by a deposit, so `ranked_sites`
            // offers it nothing — it needs its own rule, and the corp's OWN
            // asset tiles are the legible one: same body, so the same stockpile
            // pool its extraction feeds; same tile, so the same market; and
            // O(assets) rather than another O(tiles) scan. Nothing stops two
            // buildings sharing a tile (only the launchpad is capped), so this
            // is a real placement, not a workaround.
            //
            // One processor per tile, deliberately: without it a corp would
            // stack processors on its best tile every evaluation. Collected in
            // the same single walk that finds the tiles.
            std::vector<entity_id> own_tiles;
            std::vector<entity_id> tiles_with_processor;
            own_tiles.reserve(cc.assets.size());
            for (const entity_id bid : cc.assets)
            {
                const auto bit = w.buildings.find(bid);
                if (bit == w.buildings.end())
                    continue;
                own_tiles.push_back(bit->second.tile);
                if (bit->second.type == building_type::processing_facility)
                    tiles_with_processor.push_back(bit->second.tile);
            }
            // Sorted + deduped so the candidate order is a function of the world,
            // not of insertion history — the same determinism contract
            // rank_extraction_sites keeps for the extraction half.
            std::sort(own_tiles.begin(), own_tiles.end());
            own_tiles.erase(std::unique(own_tiles.begin(), own_tiles.end()), own_tiles.end());
            std::sort(tiles_with_processor.begin(), tiles_with_processor.end());

            const int   n_recipes = reg.recipe_count(building_type::processing_facility);
            const int   depth     = corp_reached_depth(cc, reg);
            const float wf        = 0.5f; // construct_building staffs at 0.5, as above
            const float batches   = pe.base_rate * wf;

            for (const entity_id tile : own_tiles)
            {
                if (std::binary_search(tiles_with_processor.begin(),
                                       tiles_with_processor.end(), tile))
                    continue;
                const auto tit = w.tiles.find(tile);
                if (tit == w.tiles.end())
                    continue;
                const entity_id body = tit->second.body;

                // The corp's own stock on this body, plus what the local market
                // actually holds — the SAME two sources the production tick
                // draws inputs from (economy_system.cpp § run_processing, the
                // BL-130 pool + inventory coverage). Read const: a candidate
                // that is only being scored must not author a pool.
                const auto pit = w.corp_body_pools.find(std::make_pair(corp, body));
                const stockpile_component* pool =
                    (pit != w.corp_body_pools.end()) ? &pit->second : nullptr;
                const entity_id mid = market_for_tile(w, tile);
                const auto      mit = (mid != null_entity) ? w.markets.find(mid) : w.markets.end();
                const market_component* mkt = (mit != w.markets.end()) ? &mit->second : nullptr;

                // RECIPE CHOICE. Walk the BROWSE space (this era's roster) and
                // cross to the ABSOLUTE id through recipe_id(name) — the two id
                // spaces recipe_registry.hpp keeps apart, and the exact crossing
                // NR-254 caught the build door getting wrong.
                //
                // Each surviving recipe is priced by `estimate_prospective_profit`
                // (BL-162) rather than by the inline revenue-minus-wages sum the
                // extraction candidate above uses. That inline model is kept
                // there ONLY because switching it would move every blessed
                // golden for no player-visible gain (the BL-162 note). This
                // candidate is new, so it has no golden to protect and no reason
                // to inherit the weaker model: the proper one takes opex through
                // compute_building_opex, so it carries habitability-scaled wages,
                // real input cost at local prices, and BL-193 stack decay.
                //
                // What it still does NOT model is input STARVATION — it prices a
                // full run. Measured at 30.9% of processing building-ticks
                // (NR-266), so this remains an over-estimate; the `reachable`
                // gate below is the coarse guard against the worst of it.
                uint16_t best_recipe = no_recipe;
                float    best_net    = 0.0f;
                for (int i = 0; i < n_recipes; ++i)
                {
                    const recipe&  rc  = reg.recipe_at(building_type::processing_facility, i);
                    const uint16_t rid = reg.recipe_id(rc.name);
                    const recipe*  abs = reg.get_recipe(rid);
                    if (abs == nullptr)
                        continue; // the name did not round-trip; never emit it

                    // BL-428's growth gate, asked here rather than discovered at
                    // the seam: construct_building rejects a recipe deeper than
                    // the corp has reached, and a candidate that can only ever be
                    // refused costs a build slot to learn nothing.
                    const int required = reg.recipe_required_depth(rid);
                    if (required < 0 || required > depth)
                        continue;

                    // INPUT ACCESS. A processor with no reachable input is an
                    // immediate loss-maker, so this asks the production tick's
                    // own question: does pool + market inventory cover a full
                    // run at the idle threshold? Below it the building would not
                    // even bootstrap on the tick it completed.
                    bool  reachable = true;
                    for (std::size_t r = 0; r < resource_count && reachable; ++r)
                    {
                        const float in = abs->inputs[r];
                        if (in <= 0.0f)
                            continue;
                        const float need  = in * batches;
                        const float avail = (pool ? pool->quantities[r] : 0.0f)
                                          + (mkt ? std::max(0.0f, mkt->inventory[r]) : 0.0f);
                        if (need > 0.0f && avail / need < reg.t_idle())
                            reachable = false;
                    }
                    if (!reachable)
                        continue;

                    const resource_type    rt = primary_output(*abs);
                    const building_profit  bp = estimate_prospective_profit(
                        w, reg, tile, building_type::processing_facility, rt, rid);
                    if (!bp.has_data)
                        continue;
                    const float n = bp.net();
                    if (n > best_net || (n == best_net && best_recipe != no_recipe && rid < best_recipe))
                    {
                        best_net    = n;
                        best_recipe = rid;
                    }
                }
                if (best_recipe == no_recipe)
                    continue; // nothing this corp can reach, run and profit from

                const recipe* rc = reg.get_recipe(best_recipe);
                // The target names the primary output. It is what the glut
                // forecast is run against, and it is the argument placement_rules
                // reads for the Hydroponics Bay rule (BL-166).
                const resource_type target    = primary_output(*rc);
                const float         primary_q = rc->outputs[static_cast<std::size_t>(target)];

                if (!placement_rules::can_place_in_world(w, tile, building_type::processing_facility, target))
                    continue;

                const float net = best_net;
                if (net <= 0.0f)
                    continue; // never build into an expected loss, same as above

                const float capex      = std::max(1.0f, pe.build_cost);
                const float added_rate = primary_q * batches;
                const int   horizon    = static_cast<int>(pe.build_duration_ticks) + p.forecast_clearing_ticks;
                const float glut       = forecast_glut_multiplier(w, tile, target, added_rate, horizon, p);
                if (glut <= 0.0f)
                    continue; // forecast hard glut — veto, as the extraction half does

                candidate c;
                c.cmd.tick   = tick;
                c.cmd.corp   = corp;
                c.cmd.verb   = corp_verb::build;
                c.cmd.tile   = tile;
                c.cmd.type   = building_type::processing_facility;
                c.cmd.target = target;
                // The recipe MUST travel with the command. construct_building
                // substitutes steel for `no_recipe` — right for a caller with no
                // opinion, wrong for one that chose — and BL-388 closed exactly
                // that trap by making corp_command assert the built processor
                // kept the recipe it was given.
                c.cmd.recipe = best_recipe;
                // Same curve as the extraction candidate, deliberately: BL-417
                // step 2 is the decision about whether net^2/capex is the right
                // shape, and it should be taken ONCE for both, not forked here.
                c.score  = (net * net / capex) * focus_weight(cc.focus, corp_verb::build) * jitter * glut;
                c.spend  = capex;
                c.reason = corp_decision_reason::best_build;
                c.bucket = bucket_for_reason(c.reason);
                cands.push_back(c);
            }
        }

        // ---- Dial candidates: per owned, operating, off-cooldown building --
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            const building_component& b = bit->second;
            if (b.ticks_remaining > 0 || b.ai_cooldown > 0)
                continue;

            const building_profit bp = estimate_building_profit(w, reg, report, bid);
            const float incumbent    = bp.has_data ? std::fabs(bp.net()) : 1.0f;
            const float margin_gate  = p.theta * std::max(1.0f, incumbent);

            if (b.decommissioned)
            {
                // Resume: would this asset make money if it ran again?
                //
                // This MUST use the same estimator the idle decision uses, or the
                // two fight. It used to hand-roll revenue − wages: no maintenance,
                // no input cost, no stack decay, no depletion taper, no supply
                // response. That reads high on exactly the buildings the reflex
                // tier (economy_system.cpp's BL-079 block) has just idled on
                // `estimate_building_profit`'s fuller number, so the scorer
                // resumed what the reflex had idled and the pair oscillated —
                // measured at 134–255 resumes against 12–17 idles over 30 ticks
                // in ai_skill_harness, more than every other verb put together
                // and around 10x the next single verb.
                //
                // `estimate_prospective_profit` (BL-162) is the existing answer to
                // "what would this earn if it ran": a pure read producing the same
                // building_profit fields `estimate_building_profit` reports, so the
                // two sides of the idle/resume pair finally compare like with like.
                // No new oracle — the standing § 5 rule.
                //
                // The build candidate above deliberately does NOT use it (see its
                // own note): there the divergence is cosmetic and switching would
                // move every blessed golden for no behavioural gain. Here the
                // divergence IS the defect, so the same argument points the other
                // way. Processors are now resumable too — the old branch was
                // extraction-only, so an idled processing facility stayed idled
                // for the rest of the campaign.
                //
                // Dropping the type filter is safe by arithmetic, not by luck.
                // estimate_prospective_profit models revenue for extraction_site
                // and processing_facility only; every other type gets revenue 0,
                // so the gain below is −wages − (running maintenance − idle
                // maintenance), i.e. −wages − 0.7 × maintenance: strictly
                // negative for every type, and never clearing `margin_gate`. No
                // candidate is ever enumerated for a building whose output this
                // estimator cannot price. Infrastructure therefore still cannot be resumed on
                // profit grounds, exactly as before this change; that it also
                // cannot be resumed on REACH grounds is a real gap, but an older
                // and separate one.
                const building_profit pp = estimate_prospective_profit(
                    w, reg, b.tile, b.type, b.target_resource, b.recipe, &b);
                if (pp.has_data)
                {
                    // Exact mirror of the idle gain below. An idled building is
                    // worth −`idle_maintenance` per tick; a running one is worth
                    // pp.net(). So resuming gains pp.net() − (−idle_maintenance).
                    //
                    // It is NOT `+ pp.maintenance`. "Maintenance is paid either
                    // way" is the intuitive claim and it is false: BL-049 splits
                    // maintenance into a fixed material share that survives
                    // decommissioning and a labour share that does not, so idling
                    // saves 70% of it. Crediting the full running figure
                    // overstates every resume by 0.7 × maintenance — a
                    // systematic bias toward running, in the one estimate whose
                    // whole purpose is to stop the AI resuming things it should
                    // leave idle.
                    const float gain = pp.net() + idle_maintenance(w, reg, b);

                    // NOT glut-forecast. Tried and reverted 2026-08-13: adding
                    // forecast_glut_multiplier here (mirroring the build path)
                    // moved not one number on any of the five benchmark seeds,
                    // because the forecast is bimodal on this world rather than
                    // graded — measured at tick 30, every (market, resource) slot
                    // carrying a demand signal sits at supply/demand 78–339
                    // against a veto ratio of 2.0, and the rest carry no demand
                    // signal at all, so the taper band (1.0–2.0) has zero
                    // occupancy. It would therefore hard-veto resumes wherever it
                    // bound at all, which is a behaviour change with no evidence
                    // behind it. See the review queue for the underlying
                    // supply/demand question, which is the thing to settle first.
                    if (gain > margin_gate)
                    {
                        candidate c;
                        c.cmd.tick = tick; c.cmd.corp = corp;
                        c.cmd.verb = corp_verb::resume; c.cmd.subject = bid;
                        c.score = gain * jitter;
                        c.reason = corp_decision_reason::dial_resume;
                        c.bucket = bucket_for_reason(c.reason);
                        cands.push_back(c);
                    }
                }
                continue; // an idled building offers no other dial
            }

            // Idle: sustained loss where stopping (wages saved, the fixed share
            // of maintenance still owed) beats running. Rides the BL-079
            // loss_streak; the 4-tick gate keeps "no idling without a sustained
            // streak" true while letting the strategic tier act ahead of the
            // 8-tick reflex.
            if (bp.has_data && bp.net() < 0.0f && b.loss_streak >= 4)
            {
                // net idle − net running. Same correction as the resume side
                // above, in the opposite direction: subtracting the RUNNING
                // maintenance understated the gain from idling by 0.7 × it,
                // because BL-049's material share is owed whether the building
                // runs or not. The two errors were mirror images and so never
                // produced a single-tick flip — they just both leaned toward
                // keeping loss-makers running.
                const float gain = -bp.net() - idle_maintenance(w, reg, b);
                if (gain > margin_gate)
                {
                    candidate c;
                    c.cmd.tick = tick; c.cmd.corp = corp;
                    c.cmd.verb = corp_verb::idle; c.cmd.subject = bid;
                    c.score = gain * jitter;
                    c.reason = corp_decision_reason::dial_idle;
                    c.bucket = bucket_for_reason(c.reason);
                    cands.push_back(c);
                }
            }

            // Workforce dial: reuse the BL-181 solver as the estimator.
            if (b.type == building_type::extraction_site ||
                b.type == building_type::processing_facility)
            {
                // The solver reports its own gain (BL-181 model, both endpoints).
                // The previous estimate took its sign from `revenue − inputs − wages`
                // rather than from the model, so it could only ever score RAISING a
                // profitable building's target and CUTTING a loss-maker's — the
                // interior optimum the solver exists to find scored negative in both
                // directions and was discarded.
                float     gain     = 0.0f;
                const int proposed = solve_workforce_target(w, reg, b, 1.0f, /*stack_rank=*/1, &gain);
                if (proposed != b.workforce_target && bp.has_data)
                {
                    if (gain > margin_gate)
                    {
                        candidate c;
                        c.cmd.tick = tick; c.cmd.corp = corp;
                        c.cmd.verb = corp_verb::set_workforce; c.cmd.subject = bid;
                        c.cmd.workforce = proposed;
                        c.score = gain * jitter;
                        c.reason = corp_decision_reason::dial_workforce;
                        c.bucket = bucket_for_reason(c.reason);
                        cands.push_back(c);
                    }
                }
            }

            // Recipe margin-chase (generalises the tier-0 floored-output rescue).
            // BL-430 DECISION: `gain` below is not offset by economy.recipe_switch's
            // one-off cost, and this candidate is not gated on the cooldown before it
            // is scored — the seam (corp_command's set_recipe verb, via
            // try_switch_recipe) still enforces both at APPLY time, so a bad chase
            // just gets rejected rather than mutating anything; it is not a new
            // planner, just an unpriced one. Stated explicitly (not a silent gap) —
            // see NEEDS_REVIEW.json.
            if (b.type == building_type::processing_facility && b.recipe != no_recipe)
            {
                const float cur_margin = recipe_margin(w, reg, b.tile, b.recipe);
                const int   n          = reg.recipe_count(building_type::processing_facility);
                uint16_t best_id = b.recipe;
                float    best_m  = cur_margin;
                for (int i = 0; i < n; ++i)
                {
                    const uint16_t rid = static_cast<uint16_t>(i);
                    const float    m   = recipe_margin(w, reg, b.tile, rid);
                    if (m > best_m) { best_m = m; best_id = rid; }
                }
                if (best_id != b.recipe)
                {
                    const float batches = reg.economics(b.type).base_rate * b.workforce_assigned;
                    const float gain    = (best_m - cur_margin) * batches;
                    if (gain > margin_gate)
                    {
                        candidate c;
                        c.cmd.tick = tick; c.cmd.corp = corp;
                        c.cmd.verb = corp_verb::set_recipe; c.cmd.subject = bid;
                        c.cmd.recipe = best_id;
                        c.score = gain * jitter;
                        c.reason = corp_decision_reason::dial_recipe;
                        c.bucket = bucket_for_reason(c.reason);
                        cands.push_back(c);
                    }
                }
            }
        }

        // ---- Survey candidates: paid discovery, bodies sorted ---------------
        {
            std::vector<entity_id> body_ids;
            for (const auto& [id, b] : w.bodies)
                if (b.survey.phase == survey_phase::hidden && b.type != body_type::star)
                    body_ids.push_back(id);
            std::sort(body_ids.begin(), body_ids.end());
            for (const entity_id body : body_ids)
            {
                const body_component& b = w.bodies.at(body);
                const float cost = survey_cost(w, body);
                if (cost <= 0.0f)
                    continue;
                const float area  = static_cast<float>(b.grid_width * b.grid_height);
                const float score = (area / cost) * 0.05f *
                                    focus_weight(cc.focus, corp_verb::survey) * jitter;
                if (score <= 0.0f)
                    continue;
                candidate c;
                c.cmd.tick = tick; c.cmd.corp = corp;
                c.cmd.verb = corp_verb::survey; c.cmd.subject = body;
                c.score  = score;
                c.spend  = cost;
                c.reason = corp_decision_reason::survey_expand;
                c.bucket = bucket_for_reason(c.reason);
                cands.push_back(c);
            }
        }

        // ---- Muster-base build candidate: BL-325 S2 made hire depend on a ---
        // completed military_base, but the ordinary build-candidate loop above
        // only ever proposes extraction_site — it never
        // proposed military_base, so no rival corp could ever satisfy the new
        // hire precondition (caught by ai_skill_harness: hire_unit dropped
        // from 21/seed to 0 the moment S2 landed without this). One flat,
        // deliberately simple candidate — no market/glut forecasting, since
        // the base produces nothing and has no market footprint — that fires
        // only while the corp holds no completed base of its own, so it never
        // competes with real economic builds once satisfied.
        {
            // Gate on ANY base — complete or still under construction — not
            // just a completed one. The first cut of this check required
            // ticks_remaining<=0 and re-fired a fresh candidate on a NEW tile
            // every eval for the whole build_duration_ticks window, since a
            // corp's one in-flight base never counted as "having" one; caught
            // by this same golden collapsing to hundreds of builds/seed.
            bool has_or_building_base = false;
            for (const entity_id bid : cc.assets)
            {
                const auto bit = w.buildings.find(bid);
                if (bit != w.buildings.end() && bit->second.type == building_type::military_base
                    && !bit->second.decommissioned)
                {
                    has_or_building_base = true;
                    break;
                }
            }
            if (!has_or_building_base)
            {
                entity_id hq_tile = null_entity;
                const auto hqb = w.buildings.find(cc.hq_building);
                if (hqb != w.buildings.end())
                    hq_tile = hqb->second.tile;

                const auto nation_it = w.nations.find(cc.home_nation);
                entity_id  best_tile = null_entity;
                long long  best_dist2 = std::numeric_limits<long long>::max();
                if (nation_it != w.nations.end())
                {
                    long long anchor_x = 0, anchor_y = 0;
                    bool have_anchor = false;
                    if (hq_tile != null_entity)
                    {
                        const auto hit = w.tiles.find(hq_tile);
                        if (hit != w.tiles.end())
                        {
                            anchor_x = hit->second.grid_x;
                            anchor_y = hit->second.grid_y;
                            have_anchor = true;
                        }
                    }
                    // BL-379: check with the SAME logistics-reach budget
                    // construct_building enforces at apply time, so the scorer
                    // never picks a best_tile the seam then refuses as
                    // out_of_range (benign — see APPLY THEN COUNT below — but
                    // one wasted seam call per eval for as long as the corp
                    // held no base). The reach rule reads a lazily-built field
                    // and treats not-built as pass, so it must be warmed here
                    // exactly as construct_building warms it; the field is a
                    // pure, cached per-body Dijkstra, so warming it at
                    // enumeration time instead of apply time changes no value.
                    const float reach_budget = reg.construction().max_logistics_reach;
                    for (const entity_id tid : nation_it->second.tiles)
                    {
                        body_reach_field(w, tile_body(w, tid));
                        // Pass `corp` (unlike the un-owned tile-only checks
                        // elsewhere in this file) so BL-344's tech gate is
                        // live here: a corp that has not earned E0-ML-01 gets
                        // NO candidate rather than one that is re-proposed and
                        // rejected every single eval for as long as the gate
                        // stays unmet (the ai_skill_harness debug trace showed
                        // most corps in the golden seeds never earning the
                        // gate's 2-extraction-sites/Cr2000 condition at all).
                        if (!placement_rules::can_place_in_world(w, tid, building_type::military_base,
                                                                  resource_type::iron_ore, reach_budget, corp))
                            continue;
                        if (!have_anchor)
                        {
                            best_tile = tid;
                            break;
                        }
                        const auto tit = w.tiles.find(tid);
                        if (tit == w.tiles.end())
                            continue;
                        const long long dx = tit->second.grid_x - anchor_x;
                        const long long dy = tit->second.grid_y - anchor_y;
                        const long long d2 = dx * dx + dy * dy;
                        if (d2 < best_dist2 || (d2 == best_dist2 && tid < best_tile))
                        {
                            best_dist2 = d2;
                            best_tile  = tid;
                        }
                    }
                }
                if (best_tile != null_entity)
                {
                    const building_economics& mex = reg.economics(building_type::military_base);
                    candidate c;
                    c.cmd.tick   = tick;
                    c.cmd.corp   = corp;
                    c.cmd.verb   = corp_verb::build;
                    c.cmd.tile   = best_tile;
                    c.cmd.type   = building_type::military_base;
                    // Flat, modest score: real enough to win an otherwise-quiet
                    // eval, never enough to out-bid a genuine extraction
                    // net-positive candidate (those score net^2/capex, typically
                    // well above 1.0 for a viable site).
                    //
                    // Said "extraction/processing" until BL-439: there is no
                    // processing_facility build candidate anywhere in this
                    // scorer, so the comparison could only ever be against an
                    // extraction site. See BL-439 (AI never builds processors).
                    c.score  = 0.35f * jitter;
                    c.spend  = std::max(1.0f, mex.build_cost);
                    c.reason = corp_decision_reason::best_build;
                    c.bucket = bucket_for_reason(c.reason);
                    cands.push_back(c);
                }
            }
        }

        // ---- Hire candidates: campaign roster, AVAILABILITY gated on the ----
        // corp's OWN stockpile/market access (unit_roster.hpp), never on cash.
        //
        // The credit cost BL-394 gave the seam (economy.lua § military, on top
        // of the resource debit) is carried in each candidate's `spend`, so the
        // solvency gate and the within-tick `committed` total both see it —
        // Ben's ruling on NR-218, 2026-08-13: "never on cash" governs which
        // rows are OFFERED, not whether a corp may spend past its own reserve
        // floor. Availability is cash-free; spending is gated like every other
        // spend. See io-standing-rules.md § the BL-324 paragraph.
        //
        // Widening the AI-agency exception list to include hiring is a
        // deliberate call (2026-08-08, recorded in io-standing-rules.md) —
        // rival corps raise units through the exact seam the player uses.
        {
            // BL-325 S2: hire moves onto the base. A corp with no COMPLETED
            // military_base of its own has no muster tile at all — it must
            // build one first, which the existing build-candidate machinery
            // above already prices (military_base is an ordinary buildable
            // type, no special-casing needed here beyond finding it).
            const entity_id muster_tile = [&]() -> entity_id
            {
                for (const entity_id bid : cc.assets)
                {
                    const auto bit = w.buildings.find(bid);
                    if (bit != w.buildings.end() && bit->second.type == building_type::military_base
                        && bit->second.ticks_remaining <= 0 && !bit->second.decommissioned)
                        return bit->second.tile;
                }
                return null_entity;
            }();

            // Soft cap: the campaign gate is presence-based (any stockpile at all
            // makes every eval eligible forever), so without a ceiling a corp with
            // steady extraction hires literally every eval — an unconditional extra
            // action outside build/dial's competition for the same budget, measured
            // to drag net worth out of the ai_skill_harness golden band. A corp's
            // own existing unit count is the simplest brake: a modest garrison, not
            // full mobilisation, which matches this being a first cut of the seam.
            constexpr int max_units_per_corp = 3;
            int owned_units = 0;
            for (const auto& [uid, u] : w.units)
                if (u.owner == corp) ++owned_units;

            if (muster_tile != null_entity && owned_units < max_units_per_corp)
            {
                const auto& table = unit_roster_table();
                const auto  avail = available_rows(w, corp, campaign_roster_band_for(reg.era()));
                for (const roster_row* row : avail)
                {
                    const auto idx = static_cast<std::size_t>(row - table.data());
                    candidate c;
                    c.cmd.tick      = tick; c.cmd.corp = corp;
                    c.cmd.verb      = corp_verb::hire_unit;
                    c.cmd.tile      = muster_tile;
                    c.cmd.unit_type = static_cast<uint16_t>(idx);
                    c.score  = static_cast<float>(row->weight) * 0.01f * jitter;
                    // The credit half of the cost BL-394 gave the seam, mirrored
                    // from apply_corp_command's formula so the solvency gate and
                    // the within-tick `committed` running total both see it. It
                    // was 0.0f when hiring was free; leaving it at 0.0f once the
                    // seam charged for it let a rival hire itself BELOW its own
                    // reserve floor (the gate short-circuits on `spend > 0`) and
                    // let a later build in the same evaluation be scored against
                    // a balance the hire had already spent.
                    //
                    // This does not gate AVAILABILITY on cash — which rows are
                    // offered still comes from stockpile/market access alone
                    // (available_rows above), exactly as the standing-rules
                    // grant requires. It gates the SPEND, like every other spend.
                    const military_capability_params& mil = reg.military();
                    c.spend  = mil.hire_base_cost
                             + mil.hire_cost_per_power * static_cast<float>(row->power_mod);
                    c.reason = corp_decision_reason::hire_available;
                    c.bucket = bucket_for_reason(c.reason);
                    cands.push_back(c);
                }
            }
        }

        // ---- Trade candidates: list accumulated stock, floored (BL-293) ------
        //
        // Ben, 2026-08-07: "the AI must be able to trade as a player does." This
        // is the scored-utility layer reaching the order book through the same
        // corp_command seam it reaches build/demolish/survey through — no
        // separate trading brain, no privileged path.
        //
        // The rule is deliberately the narrowest thing that is still trading:
        // stock past the hold threshold, half of the excess listed, floored at
        // the market's rarity floor. It exists so the AI does not sit on a
        // mountain of unsold goods while the auto-surplus path clears them at
        // whatever the ratio says; it is NOT a market strategy, and the three
        // numbers behind it are corp_ai_params fields precisely so tuning it
        // never needs this code changed. See AI_OPPONENT.md § 6.
        {
            // Lowest-id market on a body, or null. Lowest id (not map order) so
            // the choice is stable across container internals.
            auto market_on_body = [&w](entity_id body) -> entity_id {
                entity_id best = null_entity;
                for (const auto& [mid, mc] : w.markets)
                    if (mc.body == body && (best == null_entity || mid < best))
                        best = mid;
                return best;
            };

            // corp_body_pools is a std::map, so this walk is already ordered by
            // (corp, body) — the deterministic iteration the whole scorer rests on.
            for (const auto& [key, pool] : w.corp_body_pools)
            {
                if (key.first != corp)
                    continue;
                const entity_id body = key.second;
                const entity_id mid  = market_on_body(body);
                if (mid == null_entity)
                    continue;
                const market_component& mc = w.markets.at(mid);

                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    if (mc.base_price[r] <= 0.0f)
                        continue; // this market does not price the good

                    const float excess = pool.quantities[r] - p.trade_hold_threshold;
                    if (excess <= 0.0f)
                        continue;

                    // Never duplicate a standing order on the same triple: a
                    // second order would not sell more (the pool is the cap), it
                    // would just grow the book every evaluation forever.
                    bool already = false;
                    for (const sell_order& o : w.sell_orders)
                        if (o.corp == corp && o.body == body &&
                            static_cast<std::size_t>(o.resource) == r)
                        { already = true; break; }
                    if (already)
                        continue;

                    const float qty   = excess * p.trade_release_fraction;
                    const float floor = mc.base_price[r] * p.trade_floor_multiple;
                    if (qty <= 0.0f)
                        continue;

                    // Score in the same currency every other candidate uses —
                    // expected cash — so it competes honestly against a dial or a
                    // build rather than needing a hand-tuned weight. Valued at the
                    // floor, not at the current price: the floor is what the corp
                    // is actually willing to accept, so this is the conservative
                    // estimate, and it keeps a listing on a crashed market from
                    // outscoring a genuinely profitable dial.
                    candidate c;
                    c.cmd.tick        = tick;
                    c.cmd.corp        = corp;
                    c.cmd.verb        = corp_verb::place_sell_order;
                    c.cmd.subject     = body;
                    c.cmd.target      = static_cast<resource_type>(r);
                    c.cmd.quantity    = qty;
                    c.cmd.floor_price = floor;
                    c.score  = qty * floor * jitter;
                    c.reason = corp_decision_reason::trade_surplus;
                    c.bucket = bucket_for_reason(c.reason);
                    cands.push_back(c);
                }
            }
        }

        if (cands.empty())
            continue;
        std::sort(cands.begin(), cands.end(), candidate_before);

        // ---- Greedy selection under the action budget + solvency gate -------
        int   builds = 0, dials = 0, surveys = 0, hires = 0, trades = 0;
        float committed = 0.0f;
        std::vector<entity_id> touched_this_eval;

        // The decision log's runner-up (NR-232, 2026-08-14). It used to be
        // `cands[i + 1].score` — the next candidate in SORT order — which was
        // wrong in a way that mattered: this loop applies up to seven commands
        // per evaluation (max_builds + max_dials + survey + hire + max_trades),
        // so cands[i + 1] was frequently a command that ALSO RAN. The feed built
        // on that field then reported a near-tie as a coin-flip the tuning could
        // have flipped, when in truth nothing had been foregone at all.
        //
        // It now records **the best option this corp did NOT take** — the
        // highest-scoring candidate rejected anywhere in the walk, by an action
        // budget, the one-touch rule, the solvency gate, or the seam itself.
        // That is what the field's name always claimed, and it is the
        // counterfactual a reader actually wants: "what did it pass up?"
        //
        // Consequences, both deliberate:
        //  - It is knowable only once the WHOLE list has been walked (a
        //    candidate rejected at i+5 is as foregone as one rejected at i+1),
        //    so decisions are collected here and pushed after the loop, in
        //    application order. The ring and the history log are still written
        //    one-for-one in that same order, so the feed's positional pairing
        //    between them is unchanged.
        //  - Every decision from one evaluation therefore carries the SAME
        //    runner-up. That is honest: the foregone option belongs to the
        //    evaluation, not to the individual command.
        //
        // Zero still means "nothing was passed up" — every enumerated candidate
        // was acted on — which the feed renders as "uncontested".
        struct pending_decision { corp_decision d; entity_id log_body; };
        std::vector<pending_decision> pending;
        float best_rejected = 0.0f;
        const auto forgo = [&best_rejected](const candidate& cand) {
            best_rejected = std::max(best_rejected, cand.score);
        };
        for (std::size_t i = 0; i < cands.size(); ++i)
        {
            const candidate& c = cands[i];
            const bool is_build  = (c.cmd.verb == corp_verb::build);
            const bool is_survey = (c.cmd.verb == corp_verb::survey);
            const bool is_hire   = (c.cmd.verb == corp_verb::hire_unit);
            // Trade gets its own budget rather than sharing the dial budget: its
            // subject is a BODY, not a building, so neither the dial cap nor the
            // one-touch-per-building rule below means anything for it, and folding
            // it in would let a listing consume a dial slot a loss-maker needed.
            // Hire is excluded from the dial budget for the same reason (its
            // subject is a tile), and capped at one per evaluation besides.
            const bool is_trade  = (c.cmd.verb == corp_verb::place_sell_order ||
                                    c.cmd.verb == corp_verb::remove_sell_order);
            const bool is_dial   = !is_build && !is_survey && !is_hire && !is_trade;
            // Every `continue` below is a FOREGONE candidate, and each one calls
            // forgo() so it can compete to be the decision log's runner-up.
            // A budget-capped candidate is the purest case of the thing the
            // field is for: the corp wanted it and had no slot left.
            if (is_build  && builds  >= p.max_builds) { forgo(c); continue; }
            if (is_dial   && dials   >= p.max_dials)  { forgo(c); continue; }
            if (is_survey && surveys >= 1)            { forgo(c); continue; }
            // One hire per evaluation, same cadence as survey — a rival corp
            // hiring every tick would out-hire the player by pure frequency.
            if (is_hire   && hires   >= 1)            { forgo(c); continue; }
            if (is_trade  && trades  >= p.max_trades) { forgo(c); continue; }
            // One touch per building per evaluation: a second dial on a
            // building already commanded this eval is contradictory.
            if (is_dial && std::find(touched_this_eval.begin(), touched_this_eval.end(),
                                     c.cmd.subject) != touched_this_eval.end())
            {
                // Counted as foregone too, though it is the weakest case — the
                // corp did not pass this up so much as already act on that
                // building. Included because excluding it would need the reader
                // to know this rule to interpret the number, and a runner-up
                // that silently omits some rejections is the same class of
                // half-truth this whole change is undoing.
                forgo(c);
                continue;
            }

            // Solvency gate (BL-203 bucket-aware): Must-Have/Should-Have carry
            // no capex so they are never floor-gated (they cannot starve
            // themselves); only Nice-to-Have (build/survey) spend is checked,
            // and against the STRICTER should-have-aware floor — the "a lower
            // bucket may never starve a higher one" rule, concretely enforced.
            const float required_floor =
                (c.bucket == corp_priority_bucket::nice_to_have) ? nice_to_have_floor : floor_;
            if (c.spend > 0.0f &&
                w.corporations.at(corp).balance - committed - c.spend <= required_floor)
            {
                forgo(c); // could not afford it — foregone in the strongest sense
                continue;
            }

            // Resolve the log's body tag BEFORE the command mutates the world —
            // a demolish erases its subject from w.buildings, so resolving
            // after apply_corp_command would silently lose the tag.
            const entity_id log_body = resolve_command_body(w, c.cmd);

            entity_id built = null_entity;
            if (apply_corp_command(w, reg, c.cmd, &built) != corp_command_result::applied)
            {
                forgo(c);
                continue; // a seam rejection mutates nothing; just skip it
            }

            // APPLY THEN COUNT, and the order is load-bearing rather than
            // incidental. Every budget counter below is incremented only after
            // the seam has actually applied the command, so a rejected candidate
            // consumes no action slot and the next-best candidate is still tried
            // in the same evaluation.
            //
            // This is what makes every enumeration/seam disagreement in this file
            // BENIGN instead of a stall. Enumeration re-checks placement itself,
            // but not always with the seam's exact budget — the extraction-site
            // candidate above deliberately skips the logistics-reach rule that
            // construct_building enforces (the muster-base candidate agreed with
            // the seam's budget in BL-379) — so a candidate the seam refuses is a
            // thing that can happen by construction. Counting first would let one
            // such candidate spend the corp's single build slot every evaluation
            // and starve genuine economic builds indefinitely.
            //
            // Nothing asserts this ordering. Reordering these lines, or moving the
            // increments above the apply, reintroduces that starvation silently.
            committed += c.spend;
            if (is_build)  ++builds;
            if (is_dial)   ++dials;
            if (is_survey) ++surveys;
            if (is_hire)   ++hires;
            if (is_trade)  ++trades;

            // Cooldown on the touched building (anti-thrash). A trade command's
            // subject is a body, so it records no touch — the anti-duplicate rule
            // in the trade block above is what stops it thrashing.
            if (!is_trade)
            {
                const entity_id touched = is_build ? built : c.cmd.subject;
                if (const auto tb = w.buildings.find(touched); tb != w.buildings.end())
                    tb->second.ai_cooldown = p.cooldown_evals;
                touched_this_eval.push_back(touched);
            }

            // Decision log: command + winning score. The runner-up is filled in
            // after the loop, once every foregone candidate has been seen
            // (NR-232 — see the `pending` declaration above). Held here in
            // application order; the flush below preserves it.
            pending_decision pd;
            pd.d.tick          = tick;
            pd.d.corp          = corp;
            pd.d.command       = c.cmd;
            pd.d.winning_score = c.score;
            pd.d.reason        = c.reason;
            pd.log_body        = log_body;
            pending.push_back(std::move(pd));

            // Agency event so the chat feed can render the command (BL-205).
            agency_event ev{};
            ev.corp     = corp;
            ev.building = (is_build || is_hire) ? built : c.cmd.subject;
            switch (c.cmd.verb)
            {
                case corp_verb::build:
                    ev.what = agency_event::kind::built; ev.tile = c.cmd.tile; break;
                case corp_verb::demolish:
                    ev.what = agency_event::kind::demolished; break;
                case corp_verb::set_recipe:
                    ev.what = agency_event::kind::recipe_switch; ev.new_recipe = c.cmd.recipe; break;
                case corp_verb::set_workforce:
                    ev.what = agency_event::kind::workforce_set; ev.value = c.cmd.workforce; break;
                case corp_verb::idle:
                    ev.what = agency_event::kind::idled; break;
                case corp_verb::resume:
                    ev.what = agency_event::kind::resumed; break;
                case corp_verb::place_road:
                    ev.what = agency_event::kind::road_placed; ev.tile = c.cmd.tile;
                    ev.value = c.cmd.road_tier; break;
                case corp_verb::survey:
                    ev.what = agency_event::kind::survey_dispatched; ev.tile = c.cmd.subject; break;
                case corp_verb::hire_unit:
                    ev.what = agency_event::kind::hired; ev.tile = c.cmd.tile;
                    ev.value = c.cmd.unit_type; break;
                case corp_verb::place_sell_order:
                    ev.what = agency_event::kind::order_placed; ev.tile = c.cmd.subject;
                    ev.value = static_cast<int>(c.cmd.target); break;
                case corp_verb::remove_sell_order:
                    ev.what = agency_event::kind::order_removed; ev.tile = log_body; break;
                case corp_verb::set_workforce_auto:
                    // No agency kind of its own: handing one dial back to the
                    // solver is not an event the chat feed should narrate, and the
                    // scorer never emits it (it is a player/agent press). Reported
                    // as a workforce move, which is what it is.
                    ev.what = agency_event::kind::workforce_set;
                    ev.value = w.buildings.count(c.cmd.subject)
                                   ? w.buildings.at(c.cmd.subject).workforce_target : 0;
                    break;
                // BL-350's procurement trio. There is no agency_event::kind for a
                // contract yet, and inventing one here would put a kind in the
                // chat feed that nothing renders. What matters is that these no
                // longer fall out of the switch UNHANDLED: `agency_event ev{}`
                // zero-initialises, and kind 0 is `recipe_switch`, so an
                // unhandled verb would have narrated a contract as "switched
                // recipe" — a wrong statement rather than a missing one. The
                // scorer does not emit these today; an agent on the MCP seam
                // can, and this path runs for whatever the seam applies.
                //
                // Reported as a recipe_switch would be a lie, so instead the
                // event is not pushed at all: `continue` past the feed for verbs
                // the feed has no vocabulary for, leaving the world history log
                // below (which narrates from the verb label, and now knows all
                // fifteen) as the record.
                case corp_verb::request_quote:
                case corp_verb::accept_quote:
                case corp_verb::cancel_contract:
                // BL-452's convoy pair joins them for the same reason: the chat
                // feed has no vocabulary for a convoy, and kind 0 is
                // `recipe_switch`, so falling out of the switch would narrate a
                // dispatch as a recipe change — a wrong statement rather than a
                // missing one.
                case corp_verb::dispatch_convoy:
                case corp_verb::hold_convoy:
                    ev.corp = null_entity; // sentinel: "no feed event for this verb"
                    break;
            }
            if (ev.corp != null_entity)
                report.agency_events.push_back(ev);

            // World history log (BL-208): additive; economy_report's own
            // agency_events flow (chat feed, BL-205) is untouched.
            {
                world_history_entry log_entry;
                log_entry.timestamp = tick;
                log_entry.topic     = history_topic::agency;
                log_entry.body      = log_body;
                log_entry.corp      = corp;
                log_entry.event     = narrate_agency_event(ev);
                w.history_log.push_back(std::move(log_entry));
            }
        }

        // ---- Flush this evaluation's decisions (NR-232) ---------------------
        // Deferred to here because `best_rejected` is only complete once the
        // whole candidate list has been walked. Order is application order, and
        // the ring push and the `decision`-topic log append stay one-for-one in
        // that order — the positional pairing the decision feed relies on to
        // recover pre-ring rows (BL-407) is unchanged.
        //
        // What DID change: `decision` and `agency` entries no longer alternate
        // within one evaluation — the agency entries are appended above, per
        // command, and the decision entries all land here. Nothing reads the
        // two topics as an interleaved sequence (the feed filters by topic and
        // ignores `agency` entirely), but a future reader that wants to pair a
        // decision with its agency event must match on fields, not on adjacency.
        for (pending_decision& pd : pending)
        {
            pd.d.runner_up = best_rejected;
            w.ai_decisions.push(pd.d);

            world_history_entry log_entry;
            log_entry.timestamp = tick;
            log_entry.topic     = history_topic::decision;
            log_entry.body      = pd.log_body;
            log_entry.corp      = corp;
            log_entry.event     = narrate_corp_decision(pd.d);
            w.history_log.push_back(std::move(log_entry));
        }
    }
}

// ---------------------------------------------------------------------------
// State export — the visibility-honest per-corp blackboard
// ---------------------------------------------------------------------------

namespace {

void add_fact(corp_blackboard& bb, int tick, entity_id subject, std::string predicate,
              double value, float confidence, fact_provenance prov)
{
    corp_fact f;
    f.tick       = tick;
    f.subject    = subject;
    f.predicate  = std::move(predicate);
    f.value      = value;
    f.confidence = confidence;
    f.provenance = prov;
    bb.facts.push_back(std::move(f));
}

std::string res_pred(const char* stem, std::size_t r)
{
    char buf[48];
    std::snprintf(buf, sizeof buf, "%s:%zu", stem, r);
    return buf;
}

} // namespace

corp_blackboard export_corp_blackboard(const world& w, entity_id corp, int tick)
{
    corp_blackboard bb;
    bb.corp = corp;
    bb.tick = tick;

    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return bb;
    const corporation_component& cc = cit->second;

    // Section 0 — own corp: cash, buildings in full, pools. own_asset.
    add_fact(bb, tick, corp, "cash", cc.balance, 1.0f, fact_provenance::own_asset);
    {
        std::vector<entity_id> own = cc.assets;
        std::sort(own.begin(), own.end());
        for (const entity_id bid : own)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            const building_component& b = bit->second;
            add_fact(bb, tick, bid, "building_type", static_cast<double>(b.type), 1.0f, fact_provenance::own_asset);
            add_fact(bb, tick, bid, "building_tile", static_cast<double>(b.tile), 1.0f, fact_provenance::own_asset);
            add_fact(bb, tick, bid, "building_recipe", static_cast<double>(b.recipe), 1.0f, fact_provenance::own_asset);
            add_fact(bb, tick, bid, "building_workforce_target", b.workforce_target, 1.0f, fact_provenance::own_asset);
            add_fact(bb, tick, bid, "building_decommissioned", b.decommissioned ? 1.0 : 0.0, 1.0f, fact_provenance::own_asset);
        }
        for (const auto& [key, pool] : w.corp_body_pools) // std::map: sorted
        {
            if (key.first != corp)
                continue;
            for (std::size_t r = 0; r < resource_count; ++r)
                if (pool.quantities[r] > 0.0f)
                    add_fact(bb, tick, key.second, res_pred("pool", r),
                             pool.quantities[r], 1.0f, fact_provenance::own_asset);
        }

        // BL-470 Rider 1 — the blackboard export takes over BL-393's open
        // half (units were write-only and inert, so no scorer could see them
        // to command them). Own force only: reading the corp's OWN units
        // engages no BL-068 visibility rule, exactly like the cash/buildings/
        // pools above. Rivals stay gated as everywhere else in this function
        // — this section has no rival counterpart.
        {
            std::vector<entity_id> own_units;
            for (const auto& [uid, u] : w.units)
                if (u.owner == corp)
                    own_units.push_back(uid);
            std::sort(own_units.begin(), own_units.end());
            for (const entity_id uid : own_units)
            {
                const unit_component& u = w.units.at(uid);
                add_fact(bb, tick, uid, "unit_tile", static_cast<double>(u.position), 1.0f, fact_provenance::own_asset);
                // BL-511: the unit STORES a tile and DERIVES its province, and
                // the province is what march_unit now takes — so an agent that
                // could only read `unit_tile` could read its own position but
                // not name a destination in the same vocabulary. 0 = the tile
                // is unpartitioned (ocean or a world with no partition built).
                add_fact(bb, tick, uid, "unit_province",
                         static_cast<double>(w.provinces.province_of(u.position)), 1.0f,
                         fact_provenance::own_asset);
                add_fact(bb, tick, uid, "unit_type", static_cast<double>(u.type), 1.0f, fact_provenance::own_asset);
                add_fact(bb, tick, uid, "unit_count", static_cast<double>(u.count), 1.0f, fact_provenance::own_asset);
                add_fact(bb, tick, uid, "unit_strength", static_cast<double>(unit_strength(w, u)), 1.0f, fact_provenance::own_asset);
                // order state: 0 = holding (no order), 1 = marching to unit_order_dest.
                const bool has_order = u.order.dest != null_entity;
                add_fact(bb, tick, uid, "unit_order_state", has_order ? 1.0 : 0.0, 1.0f, fact_provenance::own_asset);
                if (has_order)
                {
                    add_fact(bb, tick, uid, "unit_order_dest", static_cast<double>(u.order.dest), 1.0f, fact_provenance::own_asset);
                    // BL-511: the province the order was actually GIVEN in.
                    // `unit_order_dest` is the member tile the path was solved
                    // to, which is a route detail; this is the command.
                    add_fact(bb, tick, uid, "unit_order_dest_province",
                             static_cast<double>(u.order.dest_province), 1.0f,
                             fact_provenance::own_asset);
                }
            }
        }
    }

    // Section 1 — markets: prices + supply/demand aggregates. Public (BL-068).
    {
        std::vector<entity_id> mids;
        for (const auto& kv : w.markets)
            mids.push_back(kv.first);
        std::sort(mids.begin(), mids.end());
        for (const entity_id mid : mids)
        {
            const market_component& m = w.markets.at(mid);
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                if (m.base_price[r] <= 0.0f)
                    continue; // untraded resource — no public quote
                add_fact(bb, tick, mid, res_pred("price", r),
                         (m.price[r] > 0.0f) ? m.price[r] : m.base_price[r],
                         1.0f, fact_provenance::public_market);
                add_fact(bb, tick, mid, res_pred("supply", r), m.supply[r], 1.0f, fact_provenance::public_market);
                add_fact(bb, tick, mid, res_pred("demand", r), m.demand[r], 1.0f, fact_provenance::public_market);
            }
        }
    }

    // Section 2 — rival buildings: existence, type, tile ONLY (BL-068 —
    // internals private: no cash, no pools, no recipe, no workforce).
    {
        std::vector<entity_id> rivals;
        for (const auto& kv : w.corporations)
            if (kv.first != corp)
                rivals.push_back(kv.first);
        std::sort(rivals.begin(), rivals.end());
        for (const entity_id rc : rivals)
        {
            std::vector<entity_id> theirs = w.corporations.at(rc).assets;
            std::sort(theirs.begin(), theirs.end());
            for (const entity_id bid : theirs)
            {
                const auto bit = w.buildings.find(bid);
                if (bit == w.buildings.end())
                    continue;
                add_fact(bb, tick, bid, "rival_building_type",
                         static_cast<double>(bit->second.type), 1.0f, fact_provenance::rival_visible);
                add_fact(bb, tick, bid, "rival_building_tile",
                         static_cast<double>(bit->second.tile), 1.0f, fact_provenance::rival_visible);
                add_fact(bb, tick, bid, "rival_building_owner",
                         static_cast<double>(rc), 1.0f, fact_provenance::rival_visible);
            }
        }
    }

    // Section 3 — tile deposits for SURVEYED regions only (geographic fog).
    // Fog design call (header): the world survey state applies uniformly per
    // corp; provenance `survey` tags the facts a per-corp fog would filter.
    {
        std::vector<entity_id> tids;
        for (const auto& [tid, tc] : w.tiles)
        {
            if (placement_rules::is_water_tile(tc.substrate))
                continue;
            if (!tile_surveyed(w, tc))
                continue;
            if (placement_rules::extractable_deposit(tc) <= 0.0f)
                continue;
            tids.push_back(tid);
        }
        std::sort(tids.begin(), tids.end());
        for (const entity_id tid : tids)
        {
            const tile_component& tc = w.tiles.at(tid);
            for (const resource_type rt : placement_rules::k_extractable)
            {
                const std::size_t r = static_cast<std::size_t>(rt);
                if (tc.resource_deposit[r] > 0.0f)
                    add_fact(bb, tick, tid, res_pred("tile_deposit", r),
                             tc.resource_deposit[r], 1.0f, fact_provenance::survey);
            }
        }
    }

    // Section 4 — activity fog: body tiers for KNOWN bodies only. The tier
    // store is player-derived (BL-089); applied uniformly per corp (see header).
    {
        std::vector<entity_id> body_ids;
        for (const auto& kv : w.bodies)
            body_ids.push_back(kv.first);
        std::sort(body_ids.begin(), body_ids.end());
        for (const entity_id body : body_ids)
        {
            const activity_vis v = body_activity_visibility(w, body, tick);
            if (v == activity_vis::unknown)
                continue;
            const float conf = (v == activity_vis::visible) ? 1.0f
                             : (v == activity_vis::known)   ? 0.7f : 0.4f;
            add_fact(bb, tick, body, "body_activity",
                     static_cast<double>(static_cast<uint8_t>(v)), conf, fact_provenance::route);
        }
    }

    return bb;
}

// ---------------------------------------------------------------------------
// JSONL serialisation (BL-206) — one writer, deterministic bytes.
// ---------------------------------------------------------------------------

const char* fact_provenance_name(fact_provenance p)
{
    switch (p)
    {
    case fact_provenance::own_asset:     return "own-asset";
    case fact_provenance::public_market: return "public-market";
    case fact_provenance::rival_visible: return "rival-visible";
    case fact_provenance::survey:        return "survey";
    case fact_provenance::route:         return "route";
    }
    return "unknown";
}

void to_jsonl(const corp_blackboard& bb, std::ostream& out)
{
    char buf[64];
    for (const corp_fact& f : bb.facts)
    {
        out << "{\"_v\":" << bb._v
            << ",\"t\":" << f.tick
            << ",\"subject\":" << f.subject
            << ",\"predicate\":\"" << f.predicate << '"';
        std::snprintf(buf, sizeof buf, "%.9g", f.value);
        out << ",\"value\":" << buf;
        std::snprintf(buf, sizeof buf, "%.4g", static_cast<double>(f.confidence));
        out << ",\"confidence\":" << buf
            << ",\"provenance\":\"" << fact_provenance_name(f.provenance) << "\"}\n";
    }
}
