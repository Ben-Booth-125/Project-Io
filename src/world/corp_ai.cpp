#include "corp_ai.hpp"

#include "building_profit.hpp"
#include "economy_system.hpp"  // economy_report, agency_event, solve_workforce_target
#include "market_clearing.hpp" // market_for_tile
#include "placement_rules.hpp"
#include "recipe_registry.hpp"
#include "survey_system.hpp"
#include "unit_roster.hpp"
#include "world.hpp"

#include <algorithm>
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
            return cmd.subject; // subject IS the body for this verb
        default:
        {
            const auto bit = w.buildings.find(cmd.subject);
            return (bit != w.buildings.end()) ? tile_body(w, bit->second.tile) : null_entity;
        }
    }
}

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
    }
    return "unspecified";
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
        case agency_event::kind::hired:                return "raised a unit";
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

} // namespace

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
} // namespace

bool corp_strategic_eval_due(const world& w, entity_id corp, int tick, const corp_ai_params& p)
{
    if (corp == w.player_entity)
        return false;
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end() || cit->second.is_player)
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

    for (std::size_t index = 0; index < corp_ids.size(); ++index)
    {
        const entity_id              corp = corp_ids[index];
        const corporation_component& cc   = w.corporations.at(corp);

        // NEVER act on the player corp — no strategic command, no cooldown
        // bookkeeping, nothing (io-standing-rules.md).
        if (cc.is_player || corp == w.player_entity)
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
        {
            struct site { entity_id tile; float suitability; resource_type target; };
            std::vector<site> sites;
            for (const auto& [tid, tc] : w.tiles)
            {
                if (placement_rules::is_ocean_tile(tc.composition))
                    continue;
                if (!tile_surveyed(w, tc))
                    continue; // geographic fog: unsurveyed tiles are not candidates
                bool any = false;
                const resource_type best = placement_rules::richest_extractable(tc, any);
                if (!any)
                    continue;
                const float rich = tc.resource_deposit[static_cast<std::size_t>(best)];
                if (rich <= 0.0f)
                    continue;
                // Terrain affinity × deposit richness: mountains/canyons expose
                // minerals (TILES.md), read here as a mild suitability bonus.
                float affinity = 1.0f;
                if (tc.landform == terrain_landform::mountain ||
                    tc.landform == terrain_landform::canyon)
                    affinity = 1.15f;
                sites.push_back({tid, rich * affinity, best});
            }
            std::sort(sites.begin(), sites.end(), [](const site& a, const site& b) {
                if (a.suitability != b.suitability) return a.suitability > b.suitability;
                return a.tile < b.tile;
            });
            if (static_cast<int>(sites.size()) > p.top_m_sites)
                sites.resize(static_cast<std::size_t>(p.top_m_sites));

            // Known accepted divergence (BL-162): estimate_prospective_profit
            // (world/building_profit.hpp) is the same model done properly — it takes
            // opex through compute_building_opex, so it carries habitability-scaled
            // wages and the depletion taper, which the inline scorer below omits.
            // Deliberately NOT switched: it would move AI build scoring, hence world
            // evolution, hence every blessed golden, for no player-visible gain.
            const building_economics& ex = reg.economics(building_type::extraction_site);
            for (const site& s : sites)
            {
                if (!placement_rules::can_place_in_world(w, s.tile, building_type::extraction_site, s.target))
                    continue;
                const tile_component& tc  = w.tiles.at(s.tile);
                const std::size_t     ri  = static_cast<std::size_t>(s.target);
                const float           wf  = 0.5f; // construct_building staffs at 0.5
                const float rich          = tc.resource_deposit[ri];
                const float price         = local_price(w, s.tile, ri);
                const float revenue       = ex.base_rate * rich * wf * (1.0f - tc.hazard_level) * price;
                const float net           = revenue - ex.maintenance - ex.base_wage * wf;
                if (net <= 0.0f)
                    continue; // never build into an expected loss
                const float capex = std::max(1.0f, ex.build_cost);
                const float payback = capex / net;

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
                c.score  = (net / payback) * focus_weight(cc.focus, corp_verb::build) * jitter * glut;
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
                if (b.type == building_type::extraction_site)
                {
                    const auto tit = w.tiles.find(b.tile);
                    if (tit != w.tiles.end())
                    {
                        const std::size_t ri = static_cast<std::size_t>(b.target_resource);
                        const building_economics& e = reg.economics(b.type);
                        const float rev = e.base_rate * tit->second.resource_deposit[ri] *
                                          b.workforce_assigned * (1.0f - tit->second.hazard_level) *
                                          local_price(w, b.tile, ri);
                        const float gain = rev - e.base_wage * b.workforce_assigned; // maint paid either way
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
                }
                continue; // an idled building offers no other dial
            }

            // Idle: sustained loss where stopping (wages saved, maintenance
            // kept) beats running. Rides the BL-079 loss_streak; the 4-tick
            // gate keeps "no idling without a sustained streak" true while
            // letting the strategic tier act ahead of the 8-tick reflex.
            if (bp.has_data && bp.net() < 0.0f && b.loss_streak >= 4)
            {
                const float gain = -bp.net() - bp.maintenance; // net idle − net running
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
                const int proposed = solve_workforce_target(w, reg, b, 1.0f);
                if (proposed != b.workforce_target && bp.has_data)
                {
                    // The variable part of net (revenue − inputs − wages) scales
                    // ~linearly with the target; maintenance's fixed part does not.
                    const float variable = bp.revenue - bp.input_cost - bp.wages;
                    const float cur      = std::max(10.0f, static_cast<float>(b.workforce_target));
                    const float gain     = variable * (static_cast<float>(proposed) - static_cast<float>(b.workforce_target)) / cur;
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
        // only ever proposes extraction_site/processing_facility — it never
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
                    for (const entity_id tid : nation_it->second.tiles)
                    {
                        // Pass `corp` (unlike the un-owned tile-only checks
                        // elsewhere in this file) so BL-344's tech gate is
                        // live here: a corp that has not earned E0-ML-01 gets
                        // NO candidate rather than one that is re-proposed and
                        // rejected every single eval for as long as the gate
                        // stays unmet (the ai_skill_harness debug trace showed
                        // most corps in the golden seeds never earning the
                        // gate's 2-extraction-sites/Cr2000 condition at all).
                        if (!placement_rules::can_place_in_world(w, tid, building_type::military_base,
                                                                  resource_type::iron_ore, -1.0f, corp))
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
                    // eval, never enough to out-bid a genuine extraction/processing
                    // net-positive candidate (those score net/payback, typically
                    // well above 1.0 for a viable site).
                    c.score  = 0.35f * jitter;
                    c.spend  = std::max(1.0f, mex.build_cost);
                    c.reason = corp_decision_reason::best_build;
                    c.bucket = bucket_for_reason(c.reason);
                    cands.push_back(c);
                }
            }
        }

        // ---- Hire candidates: campaign roster, gated on the corp's OWN ------
        // stockpile/market access (unit_roster.hpp), never on cash — the
        // solvency gate below still applies (spend stays 0.0f: the cost is a
        // resource debit inside apply_corp_command, not capex). Widening the
        // AI-agency exception list to include hiring is a deliberate call
        // (2026-08-08, recorded in io-standing-rules.md) — rival corps raise
        // units through the exact seam the player uses, no special case.
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
                const auto  avail = available_rows(w, corp, campaign_roster_band);
                for (const roster_row* row : avail)
                {
                    const auto idx = static_cast<std::size_t>(row - table.data());
                    candidate c;
                    c.cmd.tick      = tick; c.cmd.corp = corp;
                    c.cmd.verb      = corp_verb::hire_unit;
                    c.cmd.tile      = muster_tile;
                    c.cmd.unit_type = static_cast<uint16_t>(idx);
                    c.score  = static_cast<float>(row->weight) * 0.01f * jitter;
                    c.spend  = 0.0f; // resource-debited inside apply_corp_command
                    c.reason = corp_decision_reason::hire_available;
                    c.bucket = bucket_for_reason(c.reason);
                    cands.push_back(c);
                }
            }
        }

        if (cands.empty())
            continue;
        std::sort(cands.begin(), cands.end(), candidate_before);

        // ---- Greedy selection under the action budget + solvency gate -------
        int   builds = 0, dials = 0, surveys = 0, hires = 0;
        float committed = 0.0f;
        std::vector<entity_id> touched_this_eval;
        for (std::size_t i = 0; i < cands.size(); ++i)
        {
            const candidate& c = cands[i];
            const bool is_build  = (c.cmd.verb == corp_verb::build);
            const bool is_survey = (c.cmd.verb == corp_verb::survey);
            const bool is_hire   = (c.cmd.verb == corp_verb::hire_unit);
            const bool is_dial   = !is_build && !is_survey && !is_hire;
            if (is_build  && builds  >= p.max_builds) continue;
            if (is_dial   && dials   >= p.max_dials)  continue;
            if (is_survey && surveys >= 1)            continue;
            // One hire per evaluation, same cadence as survey — a rival corp
            // hiring every tick would out-hire the player by pure frequency.
            if (is_hire   && hires   >= 1)             continue;
            // One touch per building per evaluation: a second dial on a
            // building already commanded this eval is contradictory.
            if (is_dial && std::find(touched_this_eval.begin(), touched_this_eval.end(),
                                     c.cmd.subject) != touched_this_eval.end())
                continue;

            // Solvency gate (BL-203 bucket-aware): Must-Have/Should-Have carry
            // no capex so they are never floor-gated (they cannot starve
            // themselves); only Nice-to-Have (build/survey) spend is checked,
            // and against the STRICTER should-have-aware floor — the "a lower
            // bucket may never starve a higher one" rule, concretely enforced.
            const float required_floor =
                (c.bucket == corp_priority_bucket::nice_to_have) ? nice_to_have_floor : floor_;
            if (c.spend > 0.0f &&
                w.corporations.at(corp).balance - committed - c.spend <= required_floor)
                continue;

            // Resolve the log's body tag BEFORE the command mutates the world —
            // a demolish erases its subject from w.buildings, so resolving
            // after apply_corp_command would silently lose the tag.
            const entity_id log_body = resolve_command_body(w, c.cmd);

            entity_id built = null_entity;
            if (apply_corp_command(w, reg, c.cmd, &built) != corp_command_result::applied)
                continue; // a seam rejection mutates nothing; just skip it

            committed += c.spend;
            if (is_build)  ++builds;
            if (is_dial)   ++dials;
            if (is_survey) ++surveys;
            if (is_hire)   ++hires;

            // Cooldown on the touched building (anti-thrash).
            const entity_id touched = is_build ? built : c.cmd.subject;
            if (const auto tb = w.buildings.find(touched); tb != w.buildings.end())
                tb->second.ai_cooldown = p.cooldown_evals;
            touched_this_eval.push_back(touched);

            // Decision log: command + winning score vs the next-best candidate.
            corp_decision d;
            d.tick          = tick;
            d.corp          = corp;
            d.command       = c.cmd;
            d.winning_score = c.score;
            d.runner_up     = (i + 1 < cands.size()) ? cands[i + 1].score : 0.0f;
            d.reason        = c.reason;
            w.ai_decisions.push(d);

            // World history log (BL-208): an additional, non-evicting copy of
            // the same decision — the ring above is unchanged and keeps its
            // existing readers.
            {
                world_history_entry log_entry;
                log_entry.timestamp = tick;
                log_entry.topic     = history_topic::decision;
                log_entry.body      = log_body;
                log_entry.corp      = corp;
                log_entry.event     = narrate_corp_decision(d);
                w.history_log.push_back(std::move(log_entry));
            }

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
            }
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
            if (placement_rules::is_ocean_tile(tc.composition))
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
