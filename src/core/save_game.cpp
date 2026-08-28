#include "save_game.hpp"

#include "world/binary_io.hpp"
#include "world/continents.hpp"
#include "world/planetology.hpp"
#include "world/settlement.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <utility>
#include <vector>

namespace {

using namespace bio;

// Enum ceilings for the envelope's own types -- same rule as world_save.cpp:
// an unchecked byte cast into an enum is the failure the version guard exists
// to prevent, one field down.
constexpr auto max_abundance   = abundance_level::standard;
constexpr auto max_lean        = lean::high;
constexpr auto max_archetype   = body_archetype::ferrotroph_world;
constexpr auto max_chain_stage = chain_stage::count;
constexpr auto max_life_stage  = life_stage::civilised;
constexpr auto max_rung        = ladder_rung::borders;
constexpr auto max_region_cls  = region_class::port;
constexpr auto max_temp        = temperature_class::frozen;
constexpr auto max_atmos       = atmosphere_class::thick;
constexpr auto max_hydro       = hydrological_state::liquid;
constexpr auto max_geo         = geological_activity::high;
constexpr auto max_bias        = composition_bias::metallic;
constexpr auto max_resource    = static_cast<resource_type>(resource_count - 1);
constexpr auto max_canvas      = canvas_level::planetary;
// `overlay_mode::count` is a sentinel, not a lens -- the last real value is the
// one before it. Accepting `count` would hand the canvas a lens it cannot draw.
constexpr auto max_overlay     = overlay_mode::supply_routes;

// ---------------------------------------------------------------------------
// world_params / preferences
// ---------------------------------------------------------------------------

void w_prefs(std::ostream& o, const world_preferences& p)
{
    w_enum(o, p.star);
    w_enum(o, p.world_size);
    w_enum(o, p.interior);
    w_enum(o, p.metal);
    w_enum(o, p.ocean);
    w_enum(o, p.oxygen_story);
    w_enum(o, p.coal_basins);
    w_enum(o, p.drawdown);
    for (const uint32_t r : p.roll)
        w_u32(o, r);
}

bool r_prefs(std::istream& i, world_preferences& p)
{
    if (!(r_enum(i, p.star, max_lean) && r_enum(i, p.world_size, max_lean)
          && r_enum(i, p.interior, max_lean) && r_enum(i, p.metal, max_lean)
          && r_enum(i, p.ocean, max_lean) && r_enum(i, p.oxygen_story, max_lean)
          && r_enum(i, p.coal_basins, max_lean) && r_enum(i, p.drawdown, max_lean)))
        return false;
    for (uint32_t& r : p.roll)
        if (!r_u32(i, r))
            return false;
    return true;
}

void w_world_params(std::ostream& o, const world_params& p)
{
    w_u32(o, p.seed);
    w_enum(o, p.abundance);
    w_i64(o, p.epoch_year);
    w_int(o, p.prehistory_years);
    w_int(o, p.body_count);
    w_prefs(o, p.preferences);
}

bool r_world_params(std::istream& i, world_params& p)
{
    return r_u32(i, p.seed) && r_enum(i, p.abundance, max_abundance) && r_i64(i, p.epoch_year)
        && r_int(i, p.prehistory_years) && r_int(i, p.body_count) && r_prefs(i, p.preferences);
}

void w_planetology_params(std::ostream& o, const planetology_params& p)
{
    w_f32(o, p.star_mass);
    w_f32(o, p.system_age_gyr);
    w_f32(o, p.metallicity);
    w_f32(o, p.home_ocean);
    w_f32(o, p.oxygenation);
    w_f32(o, p.drawdown);
    w_f32(o, p.home_mass);
    w_f32(o, p.radiogenic);
    w_f32(o, p.abiogenesis_ease);
    w_f32(o, p.coal_climate);
}

bool r_planetology_params(std::istream& i, planetology_params& p)
{
    return r_f32(i, p.star_mass) && r_f32(i, p.system_age_gyr) && r_f32(i, p.metallicity)
        && r_f32(i, p.home_ocean) && r_f32(i, p.oxygenation) && r_f32(i, p.drawdown)
        && r_f32(i, p.home_mass) && r_f32(i, p.radiogenic) && r_f32(i, p.abiogenesis_ease)
        && r_f32(i, p.coal_climate);
}

// ---------------------------------------------------------------------------
// planetology_state and its parts
// ---------------------------------------------------------------------------

void w_history_event(std::ostream& o, const history_event& h)
{
    w_i64(o, h.years_before_epoch);
    w_enum(o, h.stage);
    w_str(o, h.event);
    w_str(o, h.consequence);
    w_enum(o, h.rung);
}

bool r_history_event(std::istream& i, history_event& h)
{
    return r_i64(i, h.years_before_epoch) && r_enum(i, h.stage, max_chain_stage)
        && r_str(i, h.event) && r_str(i, h.consequence) && r_enum(i, h.rung, max_rung);
}

void w_checkpoint(std::ostream& o, const checkpoint_record& c)
{
    w_enum(o, c.stage_id);
    w_str(o, c.branch_taken);
    w_u32(o, c.seed_used);
    w_bool(o, c.viability_result);
}

bool r_checkpoint(std::istream& i, checkpoint_record& c)
{
    return r_enum(i, c.stage_id, max_chain_stage) && r_str(i, c.branch_taken)
        && r_u32(i, c.seed_used) && r_bool(i, c.viability_result);
}

void w_endemic(std::ostream& o, const endemic_good& e)
{
    w_enum(o, e.good);
    w_f32(o, e.lat_lo);
    w_f32(o, e.lat_hi);
    w_f32(o, e.sector_centre);
    w_f32(o, e.sector_width);
    w_f32(o, e.richness);
}

bool r_endemic(std::istream& i, endemic_good& e)
{
    return r_enum(i, e.good, max_resource) && r_f32(i, e.lat_lo) && r_f32(i, e.lat_hi)
        && r_f32(i, e.sector_centre) && r_f32(i, e.sector_width) && r_f32(i, e.richness);
}

void w_body_profile(std::ostream& o, const body_profile& p)
{
    w_enum(o, p.temperature);
    w_enum(o, p.atmosphere);
    w_enum(o, p.hydrology);
    w_enum(o, p.geology);
    w_f32(o, p.water_fraction);
    w_enum(o, p.bias);
}

bool r_body_profile(std::istream& i, body_profile& p)
{
    return r_enum(i, p.temperature, max_temp) && r_enum(i, p.atmosphere, max_atmos)
        && r_enum(i, p.hydrology, max_hydro) && r_enum(i, p.geology, max_geo)
        && r_f32(i, p.water_fraction) && r_enum(i, p.bias, max_bias);
}

void w_planetology_state(std::ostream& o, const planetology_state& s)
{
    w_enum(o, s.archetype);
    w_enum(o, s.died_at);
    w_enum(o, s.stage);
    w_enum(o, s.peak);
    w_f32(o, s.v_esc_kms);
    w_f32(o, s.instellation);
    w_f32(o, s.shore);
    w_f32(o, s.theta);
    w_f32(o, s.surface_pressure_bar);
    w_f32(o, s.surface_temp_k);
    w_f32(o, s.o2_fraction);
    w_f32(o, s.eclipse_ratio_mean);
    w_f32(o, s.eclipse_ratio_perigee);
    w_f32(o, s.ferruginous_gyr);
    w_f32(o, s.marine_anoxia_gyr);
    w_f32(o, s.land_burial_gyr);
    w_f32(o, s.arable_share);
    w_f32(o, s.drawdown);
    w_f32(o, s.cold_traps);
    w_bool(o, s.mobile_lid);
    w_bool(o, s.core_exposed);
    w_body_profile(o, s.profile);
    w_f32_array(o, s.endowment);
    w_vec(o, s.endemics, w_endemic);
    w_vec(o, s.history, w_history_event);
    w_vec(o, s.checkpoints, w_checkpoint);
}

bool r_planetology_state(std::istream& i, planetology_state& s)
{
    return r_enum(i, s.archetype, max_archetype) && r_enum(i, s.died_at, max_chain_stage)
        && r_enum(i, s.stage, max_life_stage) && r_enum(i, s.peak, max_life_stage)
        && r_f32(i, s.v_esc_kms) && r_f32(i, s.instellation) && r_f32(i, s.shore)
        && r_f32(i, s.theta) && r_f32(i, s.surface_pressure_bar) && r_f32(i, s.surface_temp_k)
        && r_f32(i, s.o2_fraction) && r_f32(i, s.eclipse_ratio_mean)
        && r_f32(i, s.eclipse_ratio_perigee) && r_f32(i, s.ferruginous_gyr)
        && r_f32(i, s.marine_anoxia_gyr) && r_f32(i, s.land_burial_gyr)
        && r_f32(i, s.arable_share) && r_f32(i, s.drawdown) && r_f32(i, s.cold_traps)
        && r_bool(i, s.mobile_lid) && r_bool(i, s.core_exposed) && r_body_profile(i, s.profile)
        && r_f32_array(i, s.endowment) && r_vec(i, s.endemics, r_endemic)
        && r_vec(i, s.history, r_history_event) && r_vec(i, s.checkpoints, r_checkpoint);
}

// ---------------------------------------------------------------------------
// continent_state -- four per-tile rasters, the Continent lens's whole input
// ---------------------------------------------------------------------------

void w_plate(std::ostream& o, const tectonic_plate& p)
{
    w_f32(o, p.seed_col);
    w_f32(o, p.seed_row);
    w_f32(o, p.drift_col);
    w_f32(o, p.drift_row);
    w_bool(o, p.oceanic);
}

bool r_plate(std::istream& i, tectonic_plate& p)
{
    return r_f32(i, p.seed_col) && r_f32(i, p.seed_row) && r_f32(i, p.drift_col)
        && r_f32(i, p.drift_row) && r_bool(i, p.oceanic);
}

void w_continents(std::ostream& o, const continent_state& c)
{
    w_vec(o, c.plates, w_plate);
    w_ints(o, c.plate_id);
    w_floats(o, c.height_bias);
    w_bytes(o, c.convergent);
    w_bytes(o, c.divergent); // save_game_version 2 -- keep r_continents in step.
    // c.history is deliberately empty in a report -- those lines were merged
    // into planetology_state::history at generation, and the struct comment
    // says so. Written anyway, because "empty by convention" and "empty because
    // the format drops it" should not be indistinguishable on the far side.
    w_vec(o, c.history, w_history_event);
}

bool r_continents(std::istream& i, continent_state& c)
{
    return r_vec(i, c.plates, r_plate) && r_ints(i, c.plate_id) && r_floats(i, c.height_bias)
        && r_bytes(i, c.convergent) && r_bytes(i, c.divergent)
        && r_vec(i, c.history, r_history_event);
}

// ---------------------------------------------------------------------------
// settlement_state
// ---------------------------------------------------------------------------

void w_region(std::ostream& o, const region& r)
{
    w_int(o, r.anchor);
    w_int(o, r.col);
    w_int(o, r.row);
    w_int(o, r.culture);
    w_int(o, r.founding_culture);
    w_bool(o, r.creed_conquered);
    w_str(o, r.name);
    w_int(o, r.settle_score_q);
    w_int(o, r.farm_q);
    w_int(o, r.ore_q);
    w_int(o, r.energy_q);
    w_int(o, r.port_q);
    w_enum(o, r.dominant);
    w_i64(o, r.founded_year);
    w_i64(o, r.industrial_year);
    w_bool(o, r.industrialised);
    w_int(o, r.nation);
    w_int(o, r.contest_q);
    w_i64(o, r.population);
    w_i64(o, r.last_demography_year);
    w_i64(o, r.manpower_stock);
    w_u32(o, r.works_built);
    w_int(o, r.work_capacity_mod);
    w_int(o, r.work_manpower_mod);
    w_int(o, r.work_reach_mod);
    w_int(o, r.work_defence_mod);
    w_int(o, r.work_industrial_mod);
}

bool r_region(std::istream& i, region& r)
{
    return r_int(i, r.anchor) && r_int(i, r.col) && r_int(i, r.row) && r_int(i, r.culture)
        && r_int(i, r.founding_culture) && r_bool(i, r.creed_conquered) && r_str(i, r.name)
        && r_int(i, r.settle_score_q) && r_int(i, r.farm_q) && r_int(i, r.ore_q)
        && r_int(i, r.energy_q) && r_int(i, r.port_q) && r_enum(i, r.dominant, max_region_cls)
        && r_i64(i, r.founded_year) && r_i64(i, r.industrial_year) && r_bool(i, r.industrialised)
        && r_int(i, r.nation) && r_int(i, r.contest_q) && r_i64(i, r.population)
        && r_i64(i, r.last_demography_year) && r_i64(i, r.manpower_stock)
        && r_u32(i, r.works_built) && r_int(i, r.work_capacity_mod)
        && r_int(i, r.work_manpower_mod) && r_int(i, r.work_reach_mod)
        && r_int(i, r.work_defence_mod) && r_int(i, r.work_industrial_mod);
}

void w_settlement(std::ostream& o, const settlement_state& s)
{
    w_vec(o, s.regions, w_region);
    w_vec(o, s.history, w_history_event);
    w_vec(o, s.checkpoints, w_checkpoint);
    w_int(o, s.lacunae);
    w_i64(o, s.median_industrial_year);
}

bool r_settlement(std::istream& i, settlement_state& s)
{
    return r_vec(i, s.regions, r_region) && r_vec(i, s.history, r_history_event)
        && r_vec(i, s.checkpoints, r_checkpoint) && r_int(i, s.lacunae)
        && r_i64(i, s.median_industrial_year);
}

// ---------------------------------------------------------------------------
// generation_report
// ---------------------------------------------------------------------------

void w_body_entry(std::ostream& o, const generation_report::body_entry& b)
{
    w_str(o, b.name);
    w_id(o, b.id);
    w_bool(o, b.is_homeworld);
    w_planetology_state(o, b.state);
    w_planetology_state(o, b.undrawn);
    w_continents(o, b.continents);
    w_settlement(o, b.settlement);
    w_bool(o, b.tiles.valid);
    w_u32(o, b.tiles.seed);
    w_f32(o, b.tiles.deposit_scalar);
    w_int(o, b.tiles.gw);
    w_int(o, b.tiles.gh);
    w_bool(o, b.tiles.used_convergent);
}

bool r_body_entry(std::istream& i, generation_report::body_entry& b)
{
    return r_str(i, b.name) && r_id(i, b.id) && r_bool(i, b.is_homeworld)
        && r_planetology_state(i, b.state) && r_planetology_state(i, b.undrawn)
        && r_continents(i, b.continents) && r_settlement(i, b.settlement)
        && r_bool(i, b.tiles.valid) && r_u32(i, b.tiles.seed)
        && r_f32(i, b.tiles.deposit_scalar) && r_int(i, b.tiles.gw) && r_int(i, b.tiles.gh)
        && r_bool(i, b.tiles.used_convergent);
}

void w_report(std::ostream& o, const generation_report& g)
{
    w_prefs(o, g.preferences);
    w_planetology_params(o, g.params);
    w_f32(o, g.home_orbit_au);
    w_u32(o, g.attempts);
    w_vec(o, g.bodies, w_body_entry);
    w_vec(o, g.stage_lines, [](std::ostream& s, const std::string& v) { w_str(s, v); });
    w_i64(o, g.prehistory_years);
    w_i64(o, g.prehistory_battles);
    w_i64(o, g.prehistory_conquests);
    w_i64(o, g.prehistory_foundings);
}

bool r_report(std::istream& i, generation_report& g)
{
    return r_prefs(i, g.preferences) && r_planetology_params(i, g.params)
        && r_f32(i, g.home_orbit_au) && r_u32(i, g.attempts)
        && r_vec(i, g.bodies, r_body_entry)
        && r_vec(i, g.stage_lines, [](std::istream& s, std::string& v) { return r_str(s, v); })
        && r_i64(i, g.prehistory_years) && r_i64(i, g.prehistory_battles)
        && r_i64(i, g.prehistory_conquests) && r_i64(i, g.prehistory_foundings);
}

// ---------------------------------------------------------------------------
// App histories
// ---------------------------------------------------------------------------

void w_market_history(std::ostream& o, const ui::market_plot_history& m)
{
    // Ascending market id, for the reason `w_store` sorts: an unordered_map's
    // iteration order is not trusted anywhere else in this project either.
    std::vector<entity_id> ids;
    ids.reserve(m.size());
    for (const auto& kv : m)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());

    w_count(o, ids.size());
    for (const entity_id id : ids)
    {
        w_id(o, id);
        for (const ui::resource_plot_series& s : m.at(id))
        {
            w_floats(o, s.price);
            w_floats(o, s.supply);
            w_floats(o, s.demand);
        }
    }
}

bool r_market_history(std::istream& i, ui::market_plot_history& m)
{
    uint32_t n = 0;
    if (!r_count(i, n))
        return false;
    m.clear();
    m.reserve(n);
    for (uint32_t k = 0; k < n; ++k)
    {
        entity_id id = null_entity;
        if (!r_id(i, id))
            return false;
        std::array<ui::resource_plot_series, resource_count> series;
        for (ui::resource_plot_series& s : series)
            if (!(r_floats(i, s.price) && r_floats(i, s.supply) && r_floats(i, s.demand)))
                return false;
        m.emplace(id, std::move(series));
    }
    return true;
}

void w_rank_snapshot(std::ostream& o, const std::unordered_map<entity_id, int>& snap)
{
    std::vector<entity_id> ids;
    ids.reserve(snap.size());
    for (const auto& kv : snap)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());

    w_count(o, ids.size());
    for (const entity_id id : ids)
    {
        w_id(o, id);
        w_int(o, snap.at(id));
    }
}

bool r_rank_snapshot(std::istream& i, std::unordered_map<entity_id, int>& snap)
{
    uint32_t n = 0;
    if (!r_count(i, n))
        return false;
    snap.clear();
    snap.reserve(n);
    for (uint32_t k = 0; k < n; ++k)
    {
        entity_id id = null_entity;
        int       v  = 0;
        if (!r_id(i, id) || !r_int(i, v))
            return false;
        snap.emplace(id, v);
    }
    return true;
}

// ---------------------------------------------------------------------------
// The envelope
// ---------------------------------------------------------------------------

void w_envelope(std::ostream& o, const save_envelope& e)
{
    w_u64(o, e.sim_tick);
    w_u64(o, e.day_tick);
    w_u64(o, e.econ_tick);
    w_f64(o, e.elapsed_days);
    w_int(o, e.speed);

    w_world_params(o, e.params);
    w_report(o, e.report);

    w_floats(o, e.balance_history);
    w_floats(o, e.income_history);
    w_floats(o, e.expenditure_history);
    w_market_history(o, e.market_history);
    w_vec(o, e.building_rank_hist, w_rank_snapshot);

    w_enum(o, e.primary_level);
    w_enum(o, e.overlay);
    w_id(o, e.active_body);
    w_id(o, e.selected_entity);
    w_u32(o, e.selected_province);
    w_f32(o, e.solar_zoom);
    w_f32(o, e.solar_pan_x);
    w_f32(o, e.solar_pan_y);
    w_f32(o, e.circum_zoom);
    w_f32(o, e.circum_pan_x);
    w_f32(o, e.circum_pan_y);
    w_f32(o, e.planetary_zoom);
    w_f32(o, e.planetary_pan_x);
    w_f32(o, e.planetary_pan_y);
}

bool r_envelope(std::istream& i, save_envelope& e)
{
    return r_u64(i, e.sim_tick) && r_u64(i, e.day_tick) && r_u64(i, e.econ_tick)
        && r_f64(i, e.elapsed_days) && r_int(i, e.speed) && r_world_params(i, e.params)
        && r_report(i, e.report) && r_floats(i, e.balance_history)
        && r_floats(i, e.income_history) && r_floats(i, e.expenditure_history)
        && r_market_history(i, e.market_history)
        && r_vec(i, e.building_rank_hist, r_rank_snapshot)
        && r_enum(i, e.primary_level, max_canvas) && r_enum(i, e.overlay, max_overlay)
        && r_id(i, e.active_body) && r_id(i, e.selected_entity)
        && r_u32(i, e.selected_province) && r_f32(i, e.solar_zoom) && r_f32(i, e.solar_pan_x)
        && r_f32(i, e.solar_pan_y) && r_f32(i, e.circum_zoom) && r_f32(i, e.circum_pan_x)
        && r_f32(i, e.circum_pan_y) && r_f32(i, e.planetary_zoom)
        && r_f32(i, e.planetary_pan_x) && r_f32(i, e.planetary_pan_y);
}

} // namespace

// ---------------------------------------------------------------------------

bool write_save_game(const std::string& path, const world& w, const save_envelope& env)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;

    w_u32(out, save_game_magic);
    w_u32(out, save_game_version);

    // The world snapshot, embedded whole with its own IOSV header -- the same
    // nesting the world snapshot itself uses for the history log. A file that
    // fails the inner guard fails the whole load, which is the wanted behaviour.
    write_world_snapshot(w, out);
    w_envelope(out, env);

    out.flush();
    return static_cast<bool>(out); // one check for the whole file, not one per field
}

bool read_save_game(const std::string& path, world& w, save_envelope& env)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;

    uint32_t magic = 0;
    if (!r_u32(in, magic) || magic != save_game_magic)
        return false; // not a save game -- possibly a bare world snapshot

    uint32_t version = 0;
    if (!r_u32(in, version) || version != save_game_version)
        return false;

    // Both halves build into scratch. `read_world_snapshot` already guarantees
    // this for the world; doing the same for the envelope is what lets a caller
    // report a failed load without having already lost the live campaign.
    world         scratch_world;
    save_envelope scratch_env;

    if (!read_world_snapshot(scratch_world, in))
        return false;
    if (!r_envelope(in, scratch_env))
        return false;

    w   = std::move(scratch_world);
    env = std::move(scratch_env);
    return true;
}
