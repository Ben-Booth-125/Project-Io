#include "survey_system.hpp"

#include <algorithm>
#include <cmath>

namespace {

// --- Calibration constants (placeholders; tuned via tools/verify/survey_harness) ---
// One sim tick = one day. Target feel: a near small body is cheap and fast; a far
// large body is very expensive and slow. These are deliberately legible round
// numbers — recalibrate in the harness, not by feel.
constexpr float base_dispatch       = 500.0f;  ///< Flat dispatch cost (credits).
constexpr float dist_cost_per_au    = 2000.0f; ///< Credits per AU of distance.
constexpr float scan_cost_per_tile  = 0.5f;    ///< Credits per tile scanned.

constexpr int   base_transit        = 5;       ///< Minimum transit days.
constexpr float transit_per_au      = 30.0f;   ///< Transit days per AU.
constexpr int   base_scan           = 10;      ///< Minimum scan days.
constexpr float scan_per_tile       = 0.02f;   ///< Scan days per tile.

/// Day-offset into the scan phase at which region `k` (1-based) completes. Region 0
/// completes at offset 0. Regions are spread evenly across `scan` days.
int region_complete_day(int k, int regions_total, int scan)
{
    if (regions_total <= 0) return 0;
    return static_cast<int>(std::lround(static_cast<double>(k) / regions_total * scan));
}

} // namespace

float survey_heliocentric_radius(const world& w, entity_id body)
{
    const auto it = w.bodies.find(body);
    if (it == w.bodies.end()) return 0.0f;
    const body_component& b = it->second;
    if (b.parent != null_entity)
    {
        const auto pit = w.bodies.find(b.parent);
        if (pit != w.bodies.end()) return pit->second.orbital_radius_au;
    }
    return b.orbital_radius_au;
}

float survey_distance_au(const world& w, entity_id body)
{
    const float r_body = survey_heliocentric_radius(w, body);
    const float r_home = survey_heliocentric_radius(w, w.home_body);
    return std::fabs(r_body - r_home);
}

float survey_cost(const world& w, entity_id body)
{
    const auto it = w.bodies.find(body);
    if (it == w.bodies.end()) return 0.0f;
    const body_component& b = it->second;
    const float tiles = static_cast<float>(b.grid_width) * static_cast<float>(b.grid_height);
    return base_dispatch + dist_cost_per_au * survey_distance_au(w, body) + scan_cost_per_tile * tiles;
}

survey_schedule survey_compute_schedule(const world& w, entity_id body)
{
    survey_schedule s;
    const auto it = w.bodies.find(body);
    if (it == w.bodies.end()) return s;
    const body_component& b = it->second;
    const float tiles = static_cast<float>(b.grid_width) * static_cast<float>(b.grid_height);

    s.transit_ticks = base_transit + static_cast<int>(std::lround(transit_per_au * survey_distance_au(w, body)));
    s.scan_ticks    = base_scan    + static_cast<int>(std::lround(scan_per_tile  * tiles));
    s.total_ticks   = s.transit_ticks + s.scan_ticks;
    s.regions_total = survey_region_count(b.grid_width, b.grid_height);
    return s;
}

int survey_eta_days(const world& w, entity_id body)
{
    const auto it = w.bodies.find(body);
    if (it == w.bodies.end()) return 0;
    const survey_state& s = it->second.survey;
    if (s.phase == survey_phase::surveyed) return 0;

    const survey_schedule sched = survey_compute_schedule(w, body);
    if (s.phase == survey_phase::hidden)     return sched.total_ticks;
    if (s.phase == survey_phase::in_transit) return s.ticks_remaining + sched.scan_ticks;
    // scanning: `ticks_remaining` is the days left to the *next* region boundary
    // (within-region progress); add the completion-day span from that next region to
    // the last. Deriving "elapsed" from regions_done alone would freeze the ETA between
    // boundaries — wrong on every day a region does not complete.
    const int after_next = sched.scan_ticks
                         - region_complete_day(s.regions_done + 1, sched.regions_total, sched.scan_ticks);
    return std::max(0, s.ticks_remaining + after_next);
}

void init_survey_states(world& w)
{
    for (auto& [id, b] : w.bodies)
    {
        const bool always_known = (id == w.home_body) || (id == w.star_body);
        if (always_known)
        {
            b.survey.phase         = survey_phase::surveyed;
            b.survey.regions_total = survey_region_count(b.grid_width, b.grid_height);
            b.survey.regions_done  = b.survey.regions_total;
            b.survey.ticks_remaining = 0;
        }
        else
        {
            b.survey = survey_state{}; // hidden, nothing revealed
        }
    }
}

survey_dispatch_result dispatch_survey(world& w, entity_id body, entity_id payer)
{
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end()) return survey_dispatch_result::invalid;
    survey_state& s = bit->second.survey;

    if (s.phase == survey_phase::surveyed)   return survey_dispatch_result::already_surveyed;
    if (s.phase != survey_phase::hidden)      return survey_dispatch_result::in_progress;

    if (payer == null_entity)
        payer = w.player_entity; // default: the player corp pays (pre-BL-202 behaviour)
    // BL-1402 (NATIONS.md § What a nation owns): the payer is a corporation
    // (its balance) or a nation (its treasury, through `purse_of`). A nation
    // commits only its uncommitted treasury and never runs below zero
    // (`purse_room_of`); for a corporation the room IS its balance, as before.
    float* purse = purse_of(w, payer);
    if (purse == nullptr) return survey_dispatch_result::invalid;

    const float cost = survey_cost(w, body);
    if (purse_room_of(w, payer) < cost) return survey_dispatch_result::insufficient_funds;

    const survey_schedule sched = survey_compute_schedule(w, body);

    *purse -= cost;                          // debited upfront
    s.phase           = survey_phase::in_transit;
    s.regions_total   = sched.regions_total;
    s.regions_done    = 0;
    s.ticks_remaining = sched.transit_ticks;
    return survey_dispatch_result::success;
}

void advance_surveys(world& w, int days)
{
    if (days <= 0) return;

    for (auto& [id, b] : w.bodies)
    {
        survey_state& s = b.survey;
        if (s.phase == survey_phase::hidden || s.phase == survey_phase::surveyed) continue;
        // BL-1600: a body mapped from orbit for the player runs no probe schedule.
        if (s.phase == survey_phase::scanning && s.ticks_remaining == k_survey_ticks_orbit_mapped) continue;

        const survey_schedule sched = survey_compute_schedule(w, id);
        int budget = days;

        // Event-stepping: consume `ticks_remaining` days to reach the next boundary,
        // fire it, then schedule the following boundary. Zero-day gaps (a small body
        // with more regions than scan-days) fire in the same call.
        while (true)
        {
            if (s.ticks_remaining > budget)
            {
                s.ticks_remaining -= budget;
                break;
            }
            budget -= s.ticks_remaining;
            s.ticks_remaining = 0;

            if (s.phase == survey_phase::in_transit)
            {
                s.phase = survey_phase::scanning;
                if (sched.regions_total <= 0) { s.phase = survey_phase::surveyed; break; }
                s.ticks_remaining = region_complete_day(1, sched.regions_total, sched.scan_ticks);
            }
            else // scanning
            {
                ++s.regions_done;
                if (s.regions_done >= sched.regions_total)
                {
                    s.regions_done = sched.regions_total;
                    s.phase = survey_phase::surveyed;
                    s.ticks_remaining = 0;
                    break;
                }
                s.ticks_remaining = region_complete_day(s.regions_done + 1, sched.regions_total, sched.scan_ticks)
                                  - region_complete_day(s.regions_done,     sched.regions_total, sched.scan_ticks);
            }

            if (budget <= 0 && s.ticks_remaining > 0) break;
        }
    }
}

// ---------------------------------------------------------------------------
// Mapping, then prospecting (BL-1600) — see survey_system.hpp
// ---------------------------------------------------------------------------

namespace {

int region_count_of(const world& w, entity_id body)
{
    const auto it = w.bodies.find(body);
    return it == w.bodies.end() ? 0 : survey_region_count(it->second.grid_width, it->second.grid_height);
}

/// The player's mirror into the one per-body store (see the header). Writes only
/// a body no dispatched probe owns: `hidden`, or already mapped from orbit.
void mirror_player_view(world& w, entity_id nation, entity_id body, const body_survey_knowledge& k)
{
    if (nation == null_entity || nation != player_nation(w))
        return;
    survey_state& s = w.bodies.at(body).survey;
    const bool orbit_owned = s.phase == survey_phase::hidden
        || (s.phase == survey_phase::scanning && s.ticks_remaining == k_survey_ticks_orbit_mapped);
    if (!orbit_owned)
        return;
    bool all = k.regions_mapped >= k.regions_total;
    for (std::size_t i = 0; all && i < k.prospected.size(); ++i)
        all = k.prospected[i] != 0;
    s.regions_total = k.regions_total;
    s.regions_done  = k.regions_mapped;
    if (all)
    {
        s.phase           = survey_phase::surveyed;
        s.ticks_remaining = 0;
    }
    else
    {
        s.phase           = survey_phase::scanning;
        s.ticks_remaining = k_survey_ticks_orbit_mapped;
    }
}

} // namespace

bool survey_knowledge_applies(const world& w, entity_id body)
{
    if (body == null_entity || body == w.home_body || body == w.star_body)
        return false;
    const auto it = w.bodies.find(body);
    return it != w.bodies.end() && it->second.type != body_type::star
        && survey_region_count(it->second.grid_width, it->second.grid_height) > 0
        && it->second.grid_width > 0 && it->second.grid_height > 0;
}

const body_survey_knowledge* survey_knowledge_of(const world& w, entity_id nation, entity_id body)
{
    const auto it = w.survey_knowledge.find({nation, body});
    return it == w.survey_knowledge.end() ? nullptr : &it->second;
}

bool survey_region_mapped(const world& w, entity_id nation, entity_id body, int region)
{
    if (body != null_entity && body == w.home_body)
        return true;
    const body_survey_knowledge* k = survey_knowledge_of(w, nation, body);
    return k != nullptr && region >= 0 && region < k->regions_mapped;
}

bool survey_region_prospected(const world& w, entity_id nation, entity_id body, int region)
{
    if (body != null_entity && body == w.home_body)
        return true;
    const body_survey_knowledge* k = survey_knowledge_of(w, nation, body);
    return k != nullptr && region >= 0 && region < k->regions_mapped
        && static_cast<std::size_t>(region) < k->prospected.size() && k->prospected[region] != 0;
}

namespace {
int tile_region(const world& w, entity_id body, int col, int row)
{
    const auto it = w.bodies.find(body);
    if (it == w.bodies.end())
        return -1;
    const int gw = it->second.grid_width, gh = it->second.grid_height;
    if (col < 0 || row < 0 || col >= gw || row >= gh)
        return -1;
    return survey_region_of(gw, gh, col, row);
}
} // namespace

bool survey_tile_mapped(const world& w, entity_id nation, entity_id body, int col, int row)
{
    const int r = tile_region(w, body, col, row);
    return r >= 0 && survey_region_mapped(w, nation, body, r);
}

bool survey_tile_prospected(const world& w, entity_id nation, entity_id body, int col, int row)
{
    const int r = tile_region(w, body, col, row);
    return r >= 0 && survey_region_prospected(w, nation, body, r);
}

bool survey_body_surveyed_for(const world& w, entity_id nation, entity_id body)
{
    if (body != null_entity && body == w.home_body)
        return true;
    const body_survey_knowledge* k = survey_knowledge_of(w, nation, body);
    if (k == nullptr || k->regions_total <= 0 || k->regions_mapped < k->regions_total)
        return false;
    for (const uint8_t b : k->prospected)
        if (b == 0)
            return false;
    return true;
}

int survey_map_regions(world& w, entity_id nation, entity_id body, int k)
{
    if (k <= 0 || nation == null_entity || w.nations.count(nation) == 0 || !survey_knowledge_applies(w, body))
        return 0;
    const int total = region_count_of(w, body);
    auto it = w.survey_knowledge.find({nation, body});
    if (it == w.survey_knowledge.end())
    {
        body_survey_knowledge fresh;
        fresh.regions_total = total;
        fresh.prospected.assign(static_cast<std::size_t>(total), 0);
        it = w.survey_knowledge.emplace(std::make_pair(nation, body), std::move(fresh)).first;
    }
    body_survey_knowledge& kn = it->second;
    const int before = kn.regions_mapped;
    kn.regions_mapped = std::min(kn.regions_total, kn.regions_mapped + k);
    mirror_player_view(w, nation, body, kn);
    return kn.regions_mapped - before;
}

survey_prospect_result survey_prospect_region(world& w, entity_id nation, entity_id body, int* region_out)
{
    if (region_out)
        *region_out = -1;
    if (nation == null_entity || w.nations.count(nation) == 0 || !survey_knowledge_applies(w, body))
        return survey_prospect_result::invalid;
    const auto it = w.survey_knowledge.find({nation, body});
    if (it == w.survey_knowledge.end())
        return survey_prospect_result::not_mapped;
    body_survey_knowledge& kn = it->second;
    if (survey_body_surveyed_for(w, nation, body))
        return survey_prospect_result::complete;

    // The landing stands at region 0 (no landing tile is modelled): the nearest
    // unprospected mapped region by squared region-cell distance, ties low index.
    const body_component& b = w.bodies.at(body);
    const int rc = survey_region_cols(b.grid_width);
    int best = -1;
    long long best_d = 0;
    for (int r = 0; r < kn.regions_mapped; ++r)
    {
        if (kn.prospected[static_cast<std::size_t>(r)] != 0)
            continue;
        const long long cx = r % rc, cy = r / rc;
        const long long d = cx * cx + cy * cy;
        if (best < 0 || d < best_d)
        {
            best = r;
            best_d = d;
        }
    }
    if (best < 0)
        return survey_prospect_result::not_mapped;
    kn.prospected[static_cast<std::size_t>(best)] = 1;
    if (region_out)
        *region_out = best;
    mirror_player_view(w, nation, body, kn);
    return survey_prospect_result::prospected;
}
