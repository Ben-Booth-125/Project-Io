#include "corp_ai.hpp"

#include "building_profit.hpp"
#include "economy_system.hpp"  // economy_report, agency_event, solve_workforce_target
#include "market_clearing.hpp" // market_for_tile
#include "placement_rules.hpp"
#include "recipe_registry.hpp"
#include "survey_system.hpp"
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ostream>

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
    corp_command         cmd;
    float                score  = 0.0f;
    float                spend  = 0.0f; ///< Committed capex (build cost / survey cost).
    corp_decision_reason reason = corp_decision_reason::best_build;
};

/// Deterministic ordering: score desc, then verb, then subject, then tile.
bool candidate_before(const candidate& a, const candidate& b)
{
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

void run_corp_strategic_step(world& w, const recipe_registry& reg,
                             economy_report& report, int tick,
                             const corp_ai_params& p)
{
    // Sorted corp ids: the deterministic visit order AND the stable per-corp
    // index the cadence stagger keys on.
    std::vector<entity_id> corp_ids;
    corp_ids.reserve(w.corporations.size());
    for (const auto& kv : w.corporations)
        corp_ids.push_back(kv.first);
    std::sort(corp_ids.begin(), corp_ids.end());

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
                candidate c;
                c.cmd.tick   = tick;
                c.cmd.corp   = corp;
                c.cmd.verb   = corp_verb::build;
                c.cmd.tile   = s.tile;
                c.cmd.type   = building_type::extraction_site;
                c.cmd.target = s.target;
                c.score  = (net / payback) * focus_weight(cc.focus, corp_verb::build) * jitter;
                c.spend  = capex;
                c.reason = corp_decision_reason::best_build;
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
                cands.push_back(c);
            }
        }

        if (cands.empty())
            continue;
        std::sort(cands.begin(), cands.end(), candidate_before);

        // ---- Greedy selection under the action budget + solvency gate -------
        int   builds = 0, dials = 0, surveys = 0;
        float committed = 0.0f;
        std::vector<entity_id> touched_this_eval;
        for (std::size_t i = 0; i < cands.size(); ++i)
        {
            const candidate& c = cands[i];
            const bool is_build  = (c.cmd.verb == corp_verb::build);
            const bool is_survey = (c.cmd.verb == corp_verb::survey);
            const bool is_dial   = !is_build && !is_survey;
            if (is_build  && builds  >= p.max_builds) continue;
            if (is_dial   && dials   >= p.max_dials)  continue;
            if (is_survey && surveys >= 1)            continue;
            // One touch per building per evaluation: a second dial on a
            // building already commanded this eval is contradictory.
            if (is_dial && std::find(touched_this_eval.begin(), touched_this_eval.end(),
                                     c.cmd.subject) != touched_this_eval.end())
                continue;

            // Solvency gate: cash − committed spend must stay above the floor.
            if (c.spend > 0.0f &&
                w.corporations.at(corp).balance - committed - c.spend <= floor_)
                continue;

            entity_id built = null_entity;
            if (apply_corp_command(w, reg, c.cmd, &built) != corp_command_result::applied)
                continue; // a seam rejection mutates nothing; just skip it

            committed += c.spend;
            if (is_build)  ++builds;
            if (is_dial)   ++dials;
            if (is_survey) ++surveys;

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

            // Agency event so the chat feed can render the command (BL-205).
            agency_event ev{};
            ev.corp     = corp;
            ev.building = is_build ? built : c.cmd.subject;
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
            }
            report.agency_events.push_back(ev);
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
