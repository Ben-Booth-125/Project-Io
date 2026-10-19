#include "world_save.hpp"
#include "logistic_points.hpp" // BL-1309: lp_share_rule_valid

#include "binary_io.hpp"
#include "logistics.hpp"        // BL-1340: road_edges_mirrored — the mirror is checked on load
#include "river_generation.hpp"  // BL-1340: hex_side_for_offset — the pre-v53 migration
#include "history_log.hpp" // write_history_log / read_history_log -- the embedded section

#include <algorithm>
#include <cmath> // std::isfinite -- the sentiment record's own rejection (BL-546)
#include <istream>
#include <limits>
#include <ostream>
#include <map>
#include <tuple>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace bio;

// ---------------------------------------------------------------------------
// Enum ceilings
// ---------------------------------------------------------------------------
// Every enum in the stream is read through `r_enum`, which needs the largest
// valid enumerator so an out-of-range byte is REJECTED rather than cast into a
// value no switch handles. Naming them here rather than inline keeps the ceiling
// next to nothing but other ceilings, which is where a reader looks after adding
// an enumerator and wondering what else to touch. (Answer: this list, and
// `world_save_version`.)
constexpr auto max_resource   = static_cast<resource_type>(resource_count - 1);
constexpr auto max_substrate  = terrain_substrate::coast;
constexpr auto max_cover      = terrain_cover::urban;
constexpr auto max_landform   = terrain_landform::rift;
constexpr auto max_body_type  = body_type::star;
constexpr auto max_atmosphere = atmosphere_class::thick;
constexpr auto max_building   = building_type::railway_station; // BL-1255 (v49); BL-1266 (v46); BL-615 before it.
                                                           // A WIDENED range gate, not a format
                                                           // change: no serialised array is sized
                                                           // by building_type (unlike resource_
                                                           // count), so every older stream still
                                                           // reads — its values 0..7 sit inside
                                                           // the new ceiling. No version bump.
constexpr auto max_survey     = survey_phase::surveyed;
constexpr auto max_convoy     = convoy_mode::space;
constexpr auto max_focus      = industrial_focus::trade;
constexpr auto max_ownership  = ownership_class::closed; // BL-631: Pass 2b's derived class.
constexpr auto max_ideology   = ideology::isolationist;
constexpr auto max_posture    = expansionism::aggressive;
constexpr auto max_econ_focus = economic_focus::trade;
constexpr auto max_law_effect = law_effect_kind::import_tariff;
constexpr auto max_cond_subj  = condition_subject::produced; // NR-1015 appended after province_held (BL-570); science removed by BL-1273 (v47)
constexpr auto max_cond_cmp   = condition_comparator::less_than;
constexpr auto max_unit_class = unit_class::naval;
constexpr auto max_stance     = siege_stance::invest;
constexpr auto max_season     = season::winter;
constexpr auto max_battle_res = battle_result::defender_victory;
constexpr auto max_battle_end = campaign_battle_end::stalemate;
constexpr auto max_withdraw   = withdrawing_side::defender;

// ---------------------------------------------------------------------------
// Component writers / readers
// ---------------------------------------------------------------------------
// One pair per struct, fields in declaration order. Declaration order is not
// required by anything -- the stream only needs the two sides to agree -- but
// keeping it makes a field addition a two-line diff in one obvious place
// instead of a hunt.

/// BL-1340: the tile record at @p version. v53 writes the SIX-SIDE road mask (u16); every
/// older version wrote one road tier a tile (u8), and `write_world_snapshot_pre_road_edges`
/// still produces that — the one producer of an old stream, so the migration path below has
/// something to be verified against.
void w_tile_v(std::ostream& o, const tile_component& t, uint32_t version)
{
    w_id(o, t.body);
    w_int(o, t.grid_x);
    w_int(o, t.grid_y);
    w_enum(o, t.substrate);
    w_enum(o, t.cover);
    w_u8(o, t.cover_density);
    w_enum(o, t.landform);
    w_f32_array(o, t.resource_deposit);
    w_f32_array(o, t.resource_remaining);
    w_f32(o, t.hazard_level);
    w_f32(o, t.habitability);
    w_f32(o, t.substrate_density);
    if (version >= world_save_version)
        w_u16(o, t.road_edges); // BL-1340 (v53): a tier per hex side, two bits each
    else
        w_u8(o, max_road_tier(t)); // the pre-v53 shape: one tier a tile
    w_u8(o, t.river_edges);
    w_u8(o, t.river_downstream);
    w_f32(o, t.height);
    w_u8(o, t.lane_level); // BL-1098 (v28): the sea-lane tier, at the record's tail
}

/// BL-1340: the tile record at @p version. On a pre-v53 stream the one road byte is read
/// into @p legacy_road (keyed by the tile's own body and grid cell, which the record
/// carries) rather than into the component — a tier a tile is not expressible any more,
/// and `migrate_legacy_roads` turns the whole field into edges once every tile is in.
bool r_tile_v(std::istream& i, tile_component& t, uint32_t version,
              std::map<std::tuple<entity_id, int, int>, std::uint8_t>* legacy_road)
{
    if (!(r_id(i, t.body) && r_int(i, t.grid_x) && r_int(i, t.grid_y)
          && r_enum(i, t.substrate, max_substrate) && r_enum(i, t.cover, max_cover)
          && r_u8(i, t.cover_density) && r_enum(i, t.landform, max_landform)
          && r_f32_array(i, t.resource_deposit) && r_f32_array(i, t.resource_remaining)
          && r_f32(i, t.hazard_level) && r_f32(i, t.habitability)
          && r_f32(i, t.substrate_density)))
        return false;
    if (version >= world_save_version)
    {
        if (!r_u16(i, t.road_edges)) // BL-1340 (v53)
            return false;
        // Two bits a side can only hold 0-3, so no tier is out of range by construction;
        // what IS checkable is the mirror, and read_world_snapshot checks it on the whole
        // field once every tile is present.
    }
    else
    {
        std::uint8_t legacy = 0;
        if (!r_u8(i, legacy))
            return false;
        if (legacy > 3)
            return false; // a tier outside the ladder: the stream is corrupt, not old
        t.road_edges = 0;
        if (legacy_road != nullptr)
            legacy_road->emplace(std::make_tuple(t.body, t.grid_y, t.grid_x), legacy);
    }
    return r_u8(i, t.river_edges) && r_u8(i, t.river_downstream) && r_f32(i, t.height)
        && r_u8(i, t.lane_level); // BL-1098 (v28)
}

/// BL-1340 — THE PRE-v53 MIGRATION (LOGISTICS.md § 4, "Older saves"). A save written
/// before the field existed carried one tier a tile. Between each pair of 4-cardinal
/// neighbours that BOTH carried a road, lay an edge at the LOWER of their two tiers — a
/// road crosses only where both ends were roaded. Deterministic (a std::map walk over the
/// legacy field, both halves of every edge written with the same value, so no visit order
/// can reach the answer) and it reconstructs the network the generated lattice meant
/// rather than inventing crossings at its frontier.
void migrate_legacy_roads(world& s,
                          const std::map<std::tuple<entity_id, int, int>, std::uint8_t>& legacy)
{
    if (legacy.empty())
        return;
    // (body, row, col) -> tile, over the same key the legacy field is held on.
    std::map<std::tuple<entity_id, int, int>, entity_id> cell;
    for (const auto& [tid, tc] : s.tiles)
        cell.emplace(std::make_tuple(tc.body, tc.grid_y, tc.grid_x), tid);

    constexpr int dc[4] = {  0,  0, -1,  1 }; // N, S, W, E — the 4-cardinal walk A* makes
    constexpr int dr[4] = { -1,  1,  0,  0 };
    for (const auto& [key, lvl] : legacy)
    {
        if (lvl == 0)
            continue;
        const auto [body, row, col] = key;
        const auto bit = s.bodies.find(body);
        const int gw = bit != s.bodies.end() ? bit->second.grid_width  : 0;
        const int gh = bit != s.bodies.end() ? bit->second.grid_height : 0;
        const auto tit = cell.find(key);
        if (tit == cell.end())
            continue;
        const auto self = s.tiles.find(tit->second);
        if (self == s.tiles.end())
            continue;
        // Water is no bar either end (BL-1340, Ben 2026-10-17): a pre-v53 field could
        // carry a road on a water tile — a bridge — and its crossings migrate like any
        // other pair. (A stream written before the bridge ruling simply has none.)
        for (int k = 0; k < 4; ++k)
        {
            int nx = col + dc[k];
            const int ny = row + dr[k];
            if (ny < 0 || (gh > 0 && ny >= gh))
                continue; // the grid does not wrap north-south
            if (gw > 0)
                nx = ((nx % gw) + gw) % gw; // the east-west cylinder wrap
            const auto nlvl = legacy.find(std::make_tuple(body, ny, nx));
            if (nlvl == legacy.end() || nlvl->second == 0)
                continue; // the neighbour carried no road: no crossing between them
            const auto ncell = cell.find(std::make_tuple(body, ny, nx));
            if (ncell == cell.end())
                continue;
            const auto other = s.tiles.find(ncell->second);
            if (other == s.tiles.end())
                continue;
            // The side is read from the UNWRAPPED step (dc/dr), never the wrapped column
            // difference, which at the seam would be +-(gw-1) and name no neighbour.
            const int s_side = hex_side_for_offset(dc[k], dr[k], (row & 1) != 0);
            if (s_side < 0)
                continue;
            const std::uint8_t tier = std::min(lvl, nlvl->second); // THE LOWER OF THE TWO
            if (road_tier_on_side(self->second, s_side) >= tier)
                continue;
            set_road_tier_on_side(self->second, s_side, tier);
            set_road_tier_on_side(other->second, opposite_hex_side(s_side), tier);
        }
    }
}

void w_body(std::ostream& o, const body_component& b)
{
    w_str(o, b.name);
    w_enum(o, b.type);
    w_id(o, b.parent);
    w_f32(o, b.orbital_radius_au);
    w_f32(o, b.orbital_angle_rad);
    w_f32(o, b.orbital_angular_velocity_rad_per_day);
    w_f32(o, b.orbital_epoch_angle_rad);
    w_int(o, b.grid_width);
    w_int(o, b.grid_height);
    w_f32(o, b.mass_earths);
    w_enum(o, b.atmosphere); // world_save_version 41 (propellant routes follow the air)
    w_enum(o, b.survey.phase);
    w_int(o, b.survey.regions_total);
    w_int(o, b.survey.regions_done);
    w_int(o, b.survey.ticks_remaining);
}

bool r_body(std::istream& i, body_component& b)
{
    return r_str(i, b.name) && r_enum(i, b.type, max_body_type) && r_id(i, b.parent)
        && r_f32(i, b.orbital_radius_au) && r_f32(i, b.orbital_angle_rad)
        && r_f32(i, b.orbital_angular_velocity_rad_per_day)
        && r_f32(i, b.orbital_epoch_angle_rad) && r_int(i, b.grid_width)
        && r_int(i, b.grid_height) && r_f32(i, b.mass_earths)
        && r_enum(i, b.atmosphere, max_atmosphere) // world_save_version 41
        && r_enum(i, b.survey.phase, max_survey) && r_int(i, b.survey.regions_total)
        && r_int(i, b.survey.regions_done) && r_int(i, b.survey.ticks_remaining);
}

void w_building(std::ostream& o, const building_component& b)
{
    w_id(o, b.tile);
    w_enum(o, b.type);
    w_f32(o, b.workforce_assigned);
    w_enum(o, b.target_resource);
    w_u16(o, b.recipe);
    w_int(o, b.workforce_target);
    w_bool(o, b.workforce_auto);
    w_bool(o, b.decommissioned);
    w_int(o, b.active_recipe_index);
    w_int(o, b.ticks_remaining);
    w_f32(o, b.construction_progress);
    w_int(o, b.loss_streak);
    w_int(o, b.ai_cooldown);
    w_f32(o, b.wage_bid); // BL-614: world_save_version 14 (v12 pre-renumber)
    w_int(o, b.supply_factor_permille); // BL-641: world_save_version 18
    w_int(o, b.recipe_switch_cooldown);
    w_f32(o, b.construction_paid); // BL-1206: world_save_version 37
}

bool r_building(std::istream& i, building_component& b)
{
    if (!(r_id(i, b.tile) && r_enum(i, b.type, max_building)
        && r_f32(i, b.workforce_assigned) && r_enum(i, b.target_resource, max_resource)
        && r_u16(i, b.recipe) && r_int(i, b.workforce_target)
        && r_bool(i, b.workforce_auto) && r_bool(i, b.decommissioned)
        && r_int(i, b.active_recipe_index) && r_int(i, b.ticks_remaining)
        && r_f32(i, b.construction_progress) && r_int(i, b.loss_streak)
        && r_int(i, b.ai_cooldown) && r_f32(i, b.wage_bid)
        && r_int(i, b.supply_factor_permille)
        && r_int(i, b.recipe_switch_cooldown)
        && r_f32(i, b.construction_paid)))
        return false;
    // BL-1206: what a site has been charged is a non-negative running sum.
    if (!(std::isfinite(b.construction_paid) && b.construction_paid >= 0.0f))
        return false;
    // BL-614: `wage_bid` is non-negative by contract (a bid raises, never
    // undercuts, in this cut). The writer cannot have produced a NaN or a
    // negative, so a stream carrying one is corrupt — refuse whole, never
    // clamp (the r_nation qualification precedent one record up).
    //
    // BL-641: `supply_factor_permille` is a per-mille of a 0..1000 factor, and
    // the pass that writes it clamps into exactly that range at both ends — so a
    // stream carrying anything outside it is corrupt on the same reasoning.
    // Refused whole, never clamped: a clamp here would silently repair a
    // divergence the save exists to reveal.
    return std::isfinite(b.wage_bid) && b.wage_bid >= 0.0f
        && b.supply_factor_permille >= 0 && b.supply_factor_permille <= 1000;
}

void w_stockpile(std::ostream& o, const stockpile_component& s) { w_f32_array(o, s.quantities); }
bool r_stockpile(std::istream& i, stockpile_component& s) { return r_f32_array(i, s.quantities); }

void w_market(std::ostream& o, const market_component& m)
{
    w_id(o, m.body);
    w_id(o, m.centre_tile);
    w_f32_array(o, m.supply);
    w_f32_array(o, m.demand);
    w_f32_array(o, m.price);
    w_f32_array(o, m.base_price);
    w_f32_array(o, m.inventory);
    w_f32_array(o, m.household_bid);  // BL-1196: world_save_version 35 (first claimed as 32)
    w_f32_array(o, m.household_fill); // BL-1196: world_save_version 35 (first claimed as 32)
    w_f32_array(o, m.household_weight); // BL-1203: world_save_version 36
    w_f32_array(o, m.hauler_want);      // BL-1203: world_save_version 36
    w_f32_array(o, m.unposted_bid);     // BL-1227: world_save_version 38
    for (const int32_t t : m.unposted_bid_tick) w_i32(o, t); // BL-1227: world_save_version 38
    // BL-1265 (v46): the dial's pool-draw register (v40) retired with the pool.
    w_f32_array(o, m.background_fill); // BL-1217 G1b R2: world_save_version 43
    w_f32(o, m.trade_lp_spare);        // world_save_version 46
}

bool r_i32_array(std::istream& i, std::array<int32_t, resource_count>& a)
{
    for (int32_t& v : a)
        if (!r_i32(i, v))
            return false;
    return true;
}

bool r_market(std::istream& i, market_component& m)
{
    return r_id(i, m.body) && r_id(i, m.centre_tile) && r_f32_array(i, m.supply)
        && r_f32_array(i, m.demand) && r_f32_array(i, m.price)
        && r_f32_array(i, m.base_price) && r_f32_array(i, m.inventory)
        && r_f32_array(i, m.household_bid) && r_f32_array(i, m.household_fill)
        && r_f32_array(i, m.household_weight) && r_f32_array(i, m.hauler_want)
        && r_f32_array(i, m.unposted_bid) && r_i32_array(i, m.unposted_bid_tick)
        && r_f32_array(i, m.background_fill) // BL-1217 G1b R2: v45
        && r_f32(i, m.trade_lp_spare)         // v46: finite, never negative
        && std::isfinite(m.trade_lp_spare) && m.trade_lp_spare >= 0.0f;
}

void w_unit(std::ostream& o, const unit_component& u)
{
    w_id(o, u.position);
    w_id(o, u.owner);
    w_int(o, u.count);
    w_u16(o, u.type);
    w_id(o, u.order.dest);
    w_u32(o, u.order.dest_province);
    w_ids(o, u.order.path);
    w_count(o, u.order.next_index);
    w_f32(o, u.order.progress);
    w_i32(o, u.supply_factor_permille);
    w_id(o, u.muster_base);
}

bool r_unit(std::istream& i, unit_component& u)
{
    uint32_t next_index = 0;
    if (!(r_id(i, u.position) && r_id(i, u.owner) && r_int(i, u.count) && r_u16(i, u.type)
          && r_id(i, u.order.dest) && r_u32(i, u.order.dest_province) && r_ids(i, u.order.path)
          && r_count(i, next_index) && r_f32(i, u.order.progress)
          && r_i32(i, u.supply_factor_permille) && r_id(i, u.muster_base)))
        return false;
    u.order.next_index = static_cast<std::size_t>(next_index);
    return true;
}

void w_popcentre(std::ostream& o, const population_centre_component& p)
{
    w_int(o, p.scale);
    w_int(o, p.population);
    w_f32(o, p.habitability);
    w_int(o, p.growth_accumulator);
    // BL-611 (format v12): the anchor-founding marker — a rebuild of the
    // partition must not seed from a centre the partition itself caused.
    w_int(o, p.province_anchor ? 1 : 0);
    // BL-624 (format v15): the razed tier — a demoted centre must reload as
    // the ruin it is, not as a live village.
    w_int(o, p.razed ? 1 : 0);
}

bool r_popcentre(std::istream& i, population_centre_component& p)
{
    int anchor = 0;
    int razed  = 0;
    if (!(r_int(i, p.scale) && r_int(i, p.population) && r_f32(i, p.habitability)
          && r_int(i, p.growth_accumulator) && r_int(i, anchor) && r_int(i, razed)))
        return false;
    if (anchor != 0 && anchor != 1)
        return false; // not a value the writer above can produce — corrupt stream
    if (razed != 0 && razed != 1)
        return false; // same contract for the BL-624 flag
    p.province_anchor = (anchor == 1);
    p.razed           = (razed == 1);
    return true;
}

void w_nation(std::ostream& o, const nation_component& n)
{
    w_str(o, n.name);
    w_ids(o, n.tiles);
    w_f32_array(o, n.resource_abundance);
    w_enum(o, n.politics);
    w_enum(o, n.posture);
    w_enum(o, n.focus);
    w_f32(o, n.treasury);
    w_id(o, n.capital_tile);     // BL-571: world_save_version 5
    w_f32(o, n.qualification);   // BL-613: world_save_version 13 (v11 pre-renumber)
}

bool r_nation(std::istream& i, nation_component& n)
{
    if (!(r_str(i, n.name) && r_ids(i, n.tiles) && r_f32_array(i, n.resource_abundance)
        && r_enum(i, n.politics, max_ideology) && r_enum(i, n.posture, max_posture)
        && r_enum(i, n.focus, max_econ_focus) && r_f32(i, n.treasury)
        && r_id(i, n.capital_tile) && r_f32(i, n.qualification)))
        return false;
    // BL-613: `qualification` is a fraction by contract. The writer cannot have
    // produced a NaN or a value outside [0, 1], so a stream carrying one is
    // corrupt rather than odd — refuse whole, never clamp (the same grounds as
    // r_nation_budget's own rejection).
    return std::isfinite(n.qualification)
        && n.qualification >= 0.0f && n.qualification <= 1.0f;
}

/// Sprint N3 T2: one nation's budget -- the ten line weights in authored enum
/// order, then the reserve fraction. Fixed-width (`w_f32_array` writes no
/// count), so `priority_count` is part of the format: appending a line to
/// `budget_priority` widens every record and is a version bump (v23 was
/// exactly that: BL-644's `space_programme` took the count 9 -> 10).
void w_nation_budget(std::ostream& o, const nation_budget& b)
{
    w_f32_array(o, b.weights);
    w_f32(o, b.reserve_fraction);
}

/// Rejects a non-finite weight or reserve on the same grounds the sentiment
/// record does: the writer above cannot have produced one, so the stream is
/// corrupt rather than odd -- and a NaN weight would otherwise normalise the
/// whole vector into a spend nobody could replay.
bool r_nation_budget(std::istream& i, nation_budget& b)
{
    if (!(r_f32_array(i, b.weights) && r_f32(i, b.reserve_fraction)))
        return false;
    for (const float w : b.weights)
        if (!std::isfinite(w))
            return false;
    return std::isfinite(b.reserve_fraction);
}

// BL-626: one filed quarterly return. Eleven fixed-width fields, declaration
// order, no strings and no nested containers — so the whole history is a plain
// count-prefixed run of identical records. Retention is bounded at
// `k_quarterly_return_retention` by the writer, which is why the reader can
// refuse a longer run outright rather than truncating it.
void w_return(std::ostream& o, const quarterly_return& q)
{
    w_f32(o, q.income);
    w_f32(o, q.expenditure);
    w_f32(o, q.maintenance);
    w_f32(o, q.wages);
    w_f32(o, q.interest);
    w_f32(o, q.levies);
    w_f32(o, q.upkeep);
    // BL-1337: `net` is a double; its f32 slot keeps v52 a strict prefix, and
    // the exact figure follows in the v54 trailing exact-net section.
    w_f32(o, static_cast<float>(q.net));
    w_f32(o, q.balance);
    w_u32(o, q.holdings);
    w_f32(o, q.book_value);
    w_f32(o, q.refunds); // BL-1206: world_save_version 37
}

bool r_return(std::istream& i, quarterly_return& q)
{
    float net32 = 0.0f; // BL-1337: the f32 slot; a v54 stream refines it below
    if (!(r_f32(i, q.income) && r_f32(i, q.expenditure) && r_f32(i, q.maintenance)
          && r_f32(i, q.wages) && r_f32(i, q.interest) && r_f32(i, q.levies)
          && r_f32(i, q.upkeep) && r_f32(i, net32) && r_f32(i, q.balance)
          && r_u32(i, q.holdings) && r_f32(i, q.book_value) && r_f32(i, q.refunds)))
        return false;
    q.net = static_cast<double>(net32);
    // BL-1206: a refund is a credit, never negative; the writer cannot produce
    // one, so a negative or non-finite figure is a corrupt stream.
    if (!(std::isfinite(q.refunds) && q.refunds >= 0.0f))
        return false;
    // The writer cannot have produced a non-finite figure — the money loop that
    // fills the record is finite by construction — so one here means the stream
    // is corrupt rather than merely odd. Same rejection shape as the sentiment
    // record's (BL-546).
    return std::isfinite(q.income) && std::isfinite(q.expenditure)
        && std::isfinite(q.maintenance) && std::isfinite(q.wages)
        && std::isfinite(q.interest) && std::isfinite(q.levies)
        && std::isfinite(q.upkeep) && std::isfinite(net32) && std::isfinite(q.balance)
        && std::isfinite(q.book_value);
}

void w_corp(std::ostream& o, const corporation_component& c)
{
    w_str(o, c.name);
    w_id(o, c.home_nation);
    w_enum(o, c.focus);
    w_enum(o, c.ownership_class); // BL-631: world_save_version bumped for this.
    w_f32(o, c.starting_capital);
    w_f32(o, c.balance);
    w_bool(o, c.is_player);
    w_bool(o, c.is_background);
    w_ids(o, c.assets);
    w_id(o, c.hq_building);
    w_f32(o, c.influence_range);
    w_bool_array(o, c.produced_ever);
    w_vec(o, c.returns, w_return); // BL-626: world_save_version 16
    w_i32(o, c.founded_year);      // BL-1099: world_save_version 27
    w_i32(o, c.origin_region);     // BL-1099: world_save_version 27
    w_f32(o, c.refund_unbooked);   // BL-1206: world_save_version 37
    w_f32(o, c.refund_opening);    // BL-1206 review: world_save_version 37
    w_f32(o, c.trade_reserve);     // BL-1266: world_save_version 46
    w_f32(o, c.trade_points);      // BL-1266: world_save_version 46
}

bool r_corp(std::istream& i, corporation_component& c)
{
    // Field order is the declaration order, and BOTH of Sprint 20 wave 1's
    // additions sit in it: `ownership_class` (BL-631) immediately after `focus`,
    // `returns` (BL-626) after `produced_ever`; BL-1099's origin pair
    // (`founded_year`, `origin_region`) last, after `returns`. The write side
    // above must match this exactly.
    if (!(r_str(i, c.name) && r_id(i, c.home_nation) && r_enum(i, c.focus, max_focus)
          && r_enum(i, c.ownership_class, max_ownership)
          && r_f32(i, c.starting_capital) && r_f32(i, c.balance) && r_bool(i, c.is_player)
          && r_bool(i, c.is_background) && r_ids(i, c.assets) && r_id(i, c.hq_building)
          && r_f32(i, c.influence_range)
          && r_bool_array(i, c.produced_ever) && r_vec(i, c.returns, r_return)
          && r_i32(i, c.founded_year) && r_i32(i, c.origin_region)
          && r_f32(i, c.refund_unbooked) && r_f32(i, c.refund_opening)
          && r_f32(i, c.trade_reserve) && r_f32(i, c.trade_points))) // BL-1266: v46
        return false;
    // BL-1266: a reserve and a points figure are finite and never negative —
    // the only values the seam and the trade pass write. Refused, never clamped.
    if (!(std::isfinite(c.trade_reserve) && c.trade_reserve >= 0.0f
          && std::isfinite(c.trade_points) && c.trade_points >= 0.0f))
        return false;
    // BL-1206: an unbooked refund is a credit awaiting its return — finite and
    // never negative; the balance it was credited onto is finite. Refused,
    // never clamped.
    if (!(std::isfinite(c.refund_unbooked) && c.refund_unbooked >= 0.0f
          && std::isfinite(c.refund_opening)))
        return false;
    // BL-626: retention is bounded by the writer, so a longer run is a corrupt
    // stream, not a longer history. Refused rather than trimmed — trimming would
    // silently pick which quarters to believe.
    // BL-1099: an origin is a region index or -1 (no origin); anything below
    // -1 is not a value the writer produces. Refused, never clamped.
    return c.returns.size() <= k_quarterly_return_retention && c.origin_region >= -1;
}

void w_convoy(std::ostream& o, const convoy_component& c)
{
    w_id(o, c.source_market);
    w_id(o, c.dest_market);
    w_enum(o, c.mode);
    w_enum(o, c.cargo_resource);
    w_f32(o, c.cargo_qty);
    w_f32(o, c.progress);
    w_f32(o, c.speed);
    w_id(o, c.corp);
    w_bool(o, c.arrived);
    w_u32(o, c.id);
    w_bool(o, c.held);
    w_f32(o, c.cost_paid);
    w_id(o, c.origin_tile); // BL-1195: world_save_version 42 (the lane follows the legs)
    w_id(o, c.port_a);      // BL-1195: world_save_version 42
    w_id(o, c.port_b);      // BL-1195: world_save_version 42
}

bool r_convoy(std::istream& i, convoy_component& c)
{
    return r_id(i, c.source_market) && r_id(i, c.dest_market) && r_enum(i, c.mode, max_convoy)
        && r_enum(i, c.cargo_resource, max_resource) && r_f32(i, c.cargo_qty)
        && r_f32(i, c.progress) && r_f32(i, c.speed) && r_id(i, c.corp) && r_bool(i, c.arrived)
        && r_u32(i, c.id) && r_bool(i, c.held) && r_f32(i, c.cost_paid)
        && r_id(i, c.origin_tile) && r_id(i, c.port_a) && r_id(i, c.port_b); // BL-1195: v42
}

void w_route(std::ostream& o, const trade_route& t)
{
    w_id(o, t.body_a);
    w_id(o, t.body_b);
    w_id(o, t.corp);
    w_int(o, t.last_tick);
    w_int(o, t.convoy_count);
}

bool r_route(std::istream& i, trade_route& t)
{
    return r_id(i, t.body_a) && r_id(i, t.body_b) && r_id(i, t.corp) && r_int(i, t.last_tick)
        && r_int(i, t.convoy_count);
}

// --- manual trades (BL-1266, world_save_version 46) -------------------------
// One record in `standing_trade`'s declaration order. The reader refuses what
// the writer cannot produce: a zero id, a non-finite or non-positive points
// figure. Cross-references (owner, markets) are checked once the whole world
// has been read, in `read_world_snapshot`.
void w_trade(std::ostream& o, const standing_trade& t)
{
    w_u32(o, t.id);
    w_id(o, t.owner);
    w_enum(o, t.resource);
    w_id(o, t.from_market);
    w_id(o, t.to_market);
    w_f32(o, t.points);
}

bool r_trade(std::istream& i, standing_trade& t)
{
    return r_u32(i, t.id) && r_id(i, t.owner) && r_enum(i, t.resource, max_resource)
        && r_id(i, t.from_market) && r_id(i, t.to_market) && r_f32(i, t.points)
        && t.id != 0 && std::isfinite(t.points) && t.points > 0.0f
        && t.points <= 1.0e6f; // corp_command.hpp's max_trade_points: the seam's own bound
}

// --- the exchange record (BL-685) ------------------------------------------
// One row, in the declaration order of `exchange_record` (components.hpp). The
// two counterparty ids are written as plain ids because `null_entity` is a
// LEGAL value on either side here and means the market itself, not an absent
// field — the reader must not treat it as a defect.
void w_exchange(std::ostream& o, const exchange_record& e)
{
    w_int(o, e.tick);
    w_id(o, e.market);
    w_enum(o, e.resource);
    w_f32(o, e.quantity);
    w_f32(o, e.unit_price);
    w_id(o, e.seller);
    w_id(o, e.buyer);
    w_u8(o, e.side);     // NR-1021: world_save_version 46
    w_u16(o, e.parties); // NR-1021: world_save_version 46
}

bool r_exchange(std::istream& i, exchange_record& e)
{
    return r_int(i, e.tick) && r_id(i, e.market) && r_enum(i, e.resource, max_resource)
        && r_f32(i, e.quantity) && r_f32(i, e.unit_price) && r_id(i, e.seller)
        && r_id(i, e.buyer) && r_u8(i, e.side) && r_u16(i, e.parties)
        && e.side <= 1 && e.parties >= 1;
}

void w_quote(std::ostream& o, const procurement_quote& q)
{
    w_u32(o, q.id);
    w_id(o, q.buyer);
    w_id(o, q.supplier);
    w_id(o, q.body);
    w_id(o, q.delivery_body);
    w_enum(o, q.resource);
    w_f32(o, q.quantity);
    w_f32(o, q.unit_price);
    w_i32(o, q.lead_time_ticks);
    w_f32(o, q.freight_cost);
}

bool r_quote(std::istream& i, procurement_quote& q)
{
    return r_u32(i, q.id) && r_id(i, q.buyer) && r_id(i, q.supplier) && r_id(i, q.body)
        && r_id(i, q.delivery_body) && r_enum(i, q.resource, max_resource)
        && r_f32(i, q.quantity) && r_f32(i, q.unit_price) && r_i32(i, q.lead_time_ticks)
        && r_f32(i, q.freight_cost);
}

void w_contract(std::ostream& o, const procurement_contract& c)
{
    w_u32(o, c.id);
    w_id(o, c.buyer);
    w_id(o, c.supplier);
    w_id(o, c.body);
    w_enum(o, c.resource);
    w_id(o, c.delivery_body);
    w_f32(o, c.quantity);
    w_f32(o, c.unit_price);
    w_i32(o, c.lead_time_ticks);
    w_i32(o, c.ticks_elapsed);
    w_f32(o, c.deposit_paid);
    w_f32(o, c.freight_cost);
}

bool r_contract(std::istream& i, procurement_contract& c)
{
    return r_u32(i, c.id) && r_id(i, c.buyer) && r_id(i, c.supplier) && r_id(i, c.body)
        && r_enum(i, c.resource, max_resource) && r_id(i, c.delivery_body)
        && r_f32(i, c.quantity) && r_f32(i, c.unit_price) && r_i32(i, c.lead_time_ticks)
        && r_i32(i, c.ticks_elapsed) && r_f32(i, c.deposit_paid) && r_f32(i, c.freight_cost);
}

void w_condition(std::ostream& o, const condition& c)
{
    w_enum(o, c.subject);
    w_enum(o, c.comparator);
    w_f32(o, c.operand);
    w_enum(o, c.resource);
    w_enum(o, c.structure);
    w_str(o, c.key);
    w_u32(o, c.province); // BL-570 (format v5): province_held's qualifier
}

bool r_condition(std::istream& i, condition& c)
{
    return r_enum(i, c.subject, max_cond_subj) && r_enum(i, c.comparator, max_cond_cmp)
        && r_f32(i, c.operand) && r_enum(i, c.resource, max_resource)
        && r_enum(i, c.structure, max_building) && r_str(i, c.key)
        && r_u32(i, c.province); // BL-570 (format v5): not range-gated here, exactly
                                 // like c.resource/c.structure are gated but a raw
                                 // province id is not — province_holder_for (province.cpp)
                                 // is already the authoritative existence check, and it is
                                 // safe over every uint32 including no_province and a stale
                                 // id from a partition that has since been regenerated.
}

void w_condition_set(std::ostream& o, const condition_set& cs) { w_vec(o, cs.all, w_condition); }
bool r_condition_set(std::istream& i, condition_set& cs) { return r_vec(i, cs.all, r_condition); }

void w_law(std::ostream& o, const law& l)
{
    w_str(o, l.id);
    w_str(o, l.name);
    w_condition_set(o, l.conditions);
    w_enum(o, l.effect);
    w_bool(o, l.enacted);
    w_id(o, l.enacting_nation);
    w_f32(o, l.rate);
    w_int(o, l.scope_resource);
}

bool r_law(std::istream& i, law& l)
{
    return r_str(i, l.id) && r_str(i, l.name) && r_condition_set(i, l.conditions)
        && r_enum(i, l.effect, max_law_effect) && r_bool(i, l.enacted)
        && r_id(i, l.enacting_nation) && r_f32(i, l.rate) && r_int(i, l.scope_resource);
}

// --- Nation tech state (BL-1273, format v47) ---------------------------------
//
// One record per nation: the pool (milli-RP, int64), the held set and the
// queue. The held set is written as the node count it was written under, then
// that many bits packed into 32-bit words, so a store that APPENDS nodes (the
// store is append-only: gen_tech_tree_table.js) still reads an older bitset —
// the new tail reads unheld. Refused whole, never repaired: a count past this
// build's tree, a set bit past the count, a negative pool, a queue entry
// outside the tree, a duplicate queue entry, a queued node already held, and
// both sides of a pick-one pair held or queued together — every one of them a
// state `enqueue_node` and `flow_research_queue` can never produce.
void w_nation_tech(std::ostream& o, const nation_tech_state& t)
{
    w_i64(o, t.pool_milli);
    const uint32_t count = static_cast<uint32_t>(io::dg::node_count);
    w_u32(o, count);
    for (uint32_t i = 0; i < count; i += 32)
    {
        uint32_t word = 0;
        for (uint32_t b = 0; b < 32 && i + b < count; ++b)
            if (t.held.test(i + b))
                word |= (1u << b);
        w_u32(o, word);
    }
    w_vec(o, t.queue, [](std::ostream& s, const uint16_t& n) { w_u16(s, n); });
}

bool r_nation_tech(std::istream& i, nation_tech_state& t)
{
    uint32_t count = 0;
    if (!(r_i64(i, t.pool_milli) && r_u32(i, count)))
        return false;
    if (t.pool_milli < 0)
        return false;
    if (count > static_cast<uint32_t>(io::dg::node_count))
        return false;
    t.held.reset();
    for (uint32_t base = 0; base < count; base += 32)
    {
        uint32_t word = 0;
        if (!r_u32(i, word))
            return false;
        for (uint32_t b = 0; b < 32; ++b)
        {
            if ((word & (1u << b)) == 0)
                continue;
            if (base + b >= count)
                return false; // a bit past the written count: corrupt
            t.held.set(base + b);
        }
    }
    if (!r_vec(i, t.queue, [](std::istream& s, uint16_t& n) { return r_u16(s, n); }))
        return false;
    std::bitset<io::dg::node_count> queued;
    for (const uint16_t n : t.queue)
    {
        if (n >= static_cast<uint16_t>(io::dg::node_count))
            return false; // outside the tree
        if (queued.test(n) || t.held.test(n))
            return false; // queued twice, or queued while already held
        queued.set(n);
    }
    for (int n = 0; n < io::dg::node_count; ++n)
    {
        const int ex = io::dg::nodes[static_cast<std::size_t>(n)].excludes;
        if (ex < 0)
            continue;
        const bool a = t.held.test(static_cast<std::size_t>(n)) || queued.test(static_cast<std::size_t>(n));
        const bool b = t.held.test(static_cast<std::size_t>(ex)) || queued.test(static_cast<std::size_t>(ex));
        if (a && b)
            return false; // both sides of a pick-one pair
    }
    return true;
}

// --- Battles (BL-536 reverses world.hpp's "NOT SERIALISED, deliberately") ----
//
// `rng_state` and `stream_seed` are the fields that make this worth doing at
// all: restored, a battle CONTINUES on the stream it was fighting on; dropped,
// a load would silently re-roll the fight. Ben's 2026-08-22 call.

void w_stack_entry(std::ostream& o, const army_stack_entry& e)
{
    w_u16(o, e.type_id);
    w_enum(o, e.cls);
    w_int(o, e.count);
    w_int(o, e.type_power_mod);
}

bool r_stack_entry(std::istream& i, army_stack_entry& e)
{
    return r_u16(i, e.type_id) && r_enum(i, e.cls, max_unit_class) && r_int(i, e.count)
        && r_int(i, e.type_power_mod);
}

void w_doctrine(std::ostream& o, const doctrine_row& d)
{
    w_int(o, d.frontal_bonus);
    w_int(o, d.flank_fragility);
    w_int(o, d.mountain_penalty);
    w_enum(o, d.stance);
}

bool r_doctrine(std::istream& i, doctrine_row& d)
{
    return r_int(i, d.frontal_bonus) && r_int(i, d.flank_fragility)
        && r_int(i, d.mountain_penalty) && r_enum(i, d.stance, max_stance);
}

void w_battle_round(std::ostream& o, const campaign_battle_round& r)
{
    w_int(o, r.round);
    w_int(o, r.attacker_power);
    w_int(o, r.defender_power);
    w_enum(o, r.round_winner);
    w_int(o, r.attacker_strength_permille);
    w_int(o, r.defender_strength_permille);
}

bool r_battle_round(std::istream& i, campaign_battle_round& r)
{
    return r_int(i, r.round) && r_int(i, r.attacker_power) && r_int(i, r.defender_power)
        && r_enum(i, r.round_winner, max_battle_res)
        && r_int(i, r.attacker_strength_permille) && r_int(i, r.defender_strength_permille);
}

void w_battle_state(std::ostream& o, const campaign_battle_state& s)
{
    w_vec(o, s.attacker, w_stack_entry);
    w_vec(o, s.defender, w_stack_entry);
    w_doctrine(o, s.attacker_doctrine);
    w_doctrine(o, s.defender_doctrine);
    w_enum(o, s.terrain_sub);
    w_enum(o, s.terrain_cov);
    w_u8(o, s.terrain_density);
    w_enum(o, s.terrain_lf);
    w_enum(o, s.battle_season);
    w_int(o, s.attacker_supply);
    w_int(o, s.defender_supply);
    w_int(o, s.attacker_strength_permille);
    w_int(o, s.defender_strength_permille);
    w_u64(o, s.rng_state);
    w_u64(o, s.stream_seed);
    w_int(o, s.rounds_fought);
    w_enum(o, s.end);
    w_vec(o, s.rounds, w_battle_round);
}

bool r_battle_state(std::istream& i, campaign_battle_state& s)
{
    return r_vec(i, s.attacker, r_stack_entry) && r_vec(i, s.defender, r_stack_entry)
        && r_doctrine(i, s.attacker_doctrine) && r_doctrine(i, s.defender_doctrine)
        && r_enum(i, s.terrain_sub, max_substrate) && r_enum(i, s.terrain_cov, max_cover)
        && r_u8(i, s.terrain_density) && r_enum(i, s.terrain_lf, max_landform)
        && r_enum(i, s.battle_season, max_season) && r_int(i, s.attacker_supply)
        && r_int(i, s.defender_supply) && r_int(i, s.attacker_strength_permille)
        && r_int(i, s.defender_strength_permille) && r_u64(i, s.rng_state)
        && r_u64(i, s.stream_seed) && r_int(i, s.rounds_fought)
        && r_enum(i, s.end, max_battle_end) && r_vec(i, s.rounds, r_battle_round);
}

void w_battle(std::ostream& o, const active_battle& b)
{
    w_u32(o, b.province);
    w_id(o, b.attacker);
    w_id(o, b.defender);
    w_ids(o, b.attacker_units);
    w_ids(o, b.defender_units);
    w_battle_state(o, b.state);
    w_enum(o, b.withdraw_requested);
    w_id(o, b.ground_tile); // BL-1145 review: world_save_version 30
}

bool r_battle(std::istream& i, active_battle& b)
{
    return r_u32(i, b.province) && r_id(i, b.attacker) && r_id(i, b.defender)
        && r_ids(i, b.attacker_units) && r_ids(i, b.defender_units)
        && r_battle_state(i, b.state) && r_enum(i, b.withdraw_requested, max_withdraw)
        && r_id(i, b.ground_tile);
}

// ---------------------------------------------------------------------------
// Entity-keyed component stores
// ---------------------------------------------------------------------------
// Written as a count then (id, value) pairs, IN ASCENDING ID ORDER. The stores
// are `unordered_map`s whose iteration order is not itself trusted anywhere in
// the project (see world::state_hash, which sorts for the same reason), so the
// sort is what makes two snapshots of structurally-identical worlds byte-equal.
// It costs one vector of ids per store and buys a diffable file.

template <class V, class F>
void w_store(std::ostream& o, const std::unordered_map<entity_id, V>& m, F write_val)
{
    std::vector<entity_id> ids;
    ids.reserve(m.size());
    for (const auto& kv : m)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());

    w_count(o, ids.size());
    for (const entity_id id : ids)
    {
        w_id(o, id);
        write_val(o, m.at(id));
    }
}

template <class V, class F>
bool r_store(std::istream& i, std::unordered_map<entity_id, V>& m, F read_val)
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
        V v{};
        if (!read_val(i, v))
            return false;
        m.emplace(id, std::move(v));
    }
    return true;
}

/// An `unordered_map<entity_id, entity_id>` -- `tile_to_nation`,
/// `population_centre_tile`. Same ascending-id rule as `w_store`.
void w_id_store(std::ostream& o, const std::unordered_map<entity_id, entity_id>& m)
{
    w_store(o, m, [](std::ostream& s, const entity_id& v) { w_id(s, v); });
}

bool r_id_store(std::istream& i, std::unordered_map<entity_id, entity_id>& m)
{
    return r_store(i, m, [](std::istream& s, entity_id& v) { return r_id(s, v); });
}

// --- Pair-keyed maps --------------------------------------------------------
// `std::map`, so already in sorted order: `w_map` writes them as stored.

void w_id_pair(std::ostream& o, const std::pair<entity_id, entity_id>& k)
{
    w_id(o, k.first);
    w_id(o, k.second);
}

bool r_id_pair(std::istream& i, std::pair<entity_id, entity_id>& k)
{
    return r_id(i, k.first) && r_id(i, k.second);
}

} // namespace

// ---------------------------------------------------------------------------

void clear_derived_state(world& w)
{
    // Pure caches. Each is a function of what the snapshot DOES carry, so the
    // stream stays free of anything two sides could disagree about.
    w.body_tile_index.clear();
    w.astar_cost_cache.clear();
    w.logistics_flood_fields.clear();
    w.leg_flood_fields.clear(); // BL-1186: the leg-confined floods, same footing
    w.leg_path_cache.clear();
    w.body_port_tiles.clear();
    w.rail_networks.clear(); // BL-1255: the rail network, same footing
    w.body_reach_cost.clear();
    w.lp_anchor_fields.clear(); // BL-1117: the nearest-anchor field, same footing
    w.power_grid_of_province.clear(); // BL-1230: the power grid, same footing
    w.power_grid_built = false;
    bump_logistics_cache_generation(w); // BL-1195: view-only stamp, never serialised

    // The market index carries its own staleness stamps; zeroing them is what
    // makes the next `market_for_tile` rebuild rather than trust an empty index.
    w.body_market_index.clear();
    w.body_folded_index.clear(); // BL-1125: rebuilt with the market index
    w.body_market_sig.clear();   // BL-1125: likewise
    w.body_route_index.clear();  // BL-1125: the catchment raster, rebuilt on first read
    w.body_market_index_count  = 0;
    w.body_market_index_cursor = 0; // not an id: the allocator cursor at build (BL-1079)

    // BL-1050's per-body centre index, on the same terms: a pure function of
    // `population_centres` + `population_centre_tile`, both of which the
    // snapshot DOES carry, so it is rebuilt rather than written.
    w.body_centre_index.clear();
    w.body_centre_index_count  = 0;
    w.body_centre_index_cursor = 0; // not an id: the allocator cursor at build

    // Observability, not state: the chat feed and the harness read this ring.
    // A loaded campaign starts with an empty decision log rather than a
    // resurrected one, which is honest -- those decisions were taken in a
    // session that has ended.
    w.ai_decisions.entries.clear();
    w.ai_decisions.next  = 0;
    w.ai_decisions.total = 0;

    // BL-1296: this tick's state ledger is transient (world.hpp): a loaded
    // campaign opens with none, and the next tick's funding gate writes it.
    w.state_ledger.clear();

    // Mirrored from the sim loop each frame by app; the envelope carries the
    // authoritative tick.
    w.current_day_tick  = 0;
    w.current_econ_tick = 0;
}

/// The one writer body. @p version is the stream's version word AND the tile record's
/// shape (BL-1340): `world_save_version` for a real save,
/// `k_world_save_version_pre_road_edges` for the verification-only producer of a pre-edge
/// stream. Nothing else differs — v52 is today's stream with yesterday's tile record.
void write_world_snapshot_at(const world& w, std::ostream& out, uint32_t version)
{
    w_u32(out, world_save_magic);
    w_u32(out, version);

    // --- well-known entities, the belt, and the allocator cursor -------------
    w_id(out, w.player_entity);
    w_id(out, w.star_body);
    w_id(out, w.home_body);
    w_f32(out, w.belt.inner_radius_au);
    w_f32(out, w.belt.outer_radius_au);
    w_u32(out, w.next_entity_id());
    w_u32(out, w.next_convoy_id);
    w_u32(out, w.next_trade_id); // BL-1266 (v46): the retired order book's id slot
    w_u32(out, w.next_procurement_id);
    // BL-1101 (format v26): the world's own recipe band, one byte. Written
    // here with the other top-level scalars because it is one — a verdict the
    // Industrialisation fold reached once, not live tick state — and read back
    // by `app::load_game_from` so a save opens on the band its history earned.
    w_enum(out, w.campaign_band);

    // --- component stores ---------------------------------------------------
    w_store(out, w.bodies, w_body);
    w_store(out, w.tiles, [version](std::ostream& o, const tile_component& t) {
        w_tile_v(o, t, version);
    });
    w_store(out, w.buildings, w_building);
    w_store(out, w.stockpiles, w_stockpile);
    w_store(out, w.markets, w_market);
    w_store(out, w.units, w_unit);
    w_store(out, w.population_centres, w_popcentre);
    w_id_store(out, w.population_centre_tile);
    w_store(out, w.population_centre_name,
            [](std::ostream& s, const std::string& v) { w_str(s, v); });
    // BL-612 (format v11): sparse per-tile land use — the urban footprints
    // generation stamps under population centres, plus whatever play mutates.
    w_store(out, w.land_use, [](std::ostream& s, const land_use_component& v) {
        w_int(s, static_cast<int>(v.use));
    });
    w_store(out, w.nations, w_nation);
    // Sprint N3 T2 (format v3): the persistent weight map, directly after the
    // nations it keys on. A `std::map`, so `w_map` writes it ascending as held.
    w_map(out, w.nation_budgets, [](std::ostream& s, const entity_id& k) { w_id(s, k); },
          w_nation_budget);
    w_id_store(out, w.tile_to_nation);
    w_store(out, w.corporations, w_corp);

    // --- pair-keyed economy tables ------------------------------------------
    // BL-1265 (v46): the corporation pools (BL-1003) retired — every good is
    // on a market's shelf, written with the markets above.
    w_map(out, w.workforce_supply_overrides, w_id_pair,
          [](std::ostream& s, const float& v) { w_f32(s, v); });
    // BL-546: `corp_reputation` (one float per pair) became `sentiment` (two,
    // Access and Trust). The map is written in the same place and the same
    // shape — a pair key, then its value — so the only layout change is the
    // second float, and world_save_version 2 is what refuses to read a v1
    // snapshot's single-float records as this.
    w_map(out, w.sentiment.pairs, w_id_pair,
          [](std::ostream& s, const sentiment_value& v) { w_f32(s, v.access); w_f32(s, v.trust); });

    // --- logistics ----------------------------------------------------------
    w_vec(out, w.convoys, w_convoy);
    w_vec(out, w.trade_routes, w_route);
    w_map(out, w.body_last_glimpse_tick, [](std::ostream& s, const entity_id& k) { w_id(s, k); },
          [](std::ostream& s, const int& v) { w_int(s, v); });

    // --- manual trades and procurement --------------------------------------
    // BL-1265 (v46): the order book retired; its slot carries the manual trades.
    w_vec(out, w.trades, w_trade);
    w_vec(out, w.procurement_quotes, w_quote);
    w_vec(out, w.procurement_contracts, w_contract);
    w_store(out, w.corp_embargo_conditions, w_condition_set);

    // --- stance (BL-448) ----------------------------------------------------
    // Written in the sets' own sorted order, which is why they need no re-sort
    // on the way back in.
    w_id_pair_set(out, w.corp_hostile_pairs);
    w_id_pair_set(out, w.corp_friend_pairs);
    w_id_pair_set(out, w.corp_friend_offers);

    // --- research and law ---------------------------------------------------
    // BL-1273 (v47): each nation's tech state, ascending as held (std::map). It
    // replaces the per-corp `earned_techs` set and the order-bearing
    // `corp_modifiers` list (NR-1022), both retired with the four tech gates.
    w_map(out, w.nation_tech, [](std::ostream& s, const entity_id& k) { w_id(s, k); },
          w_nation_tech);
    w_vec(out, w.laws, w_law);

    // --- battles ------------------------------------------------------------
    // In `world::battles`' own order, which is kept sorted by
    // (province, attacker, defender). Preserving it is the point: that ordering
    // is where order-independence is recovered from the unordered containers
    // battles are discovered from.
    w_vec(out, w.battles, w_battle);

    // --- the history log + province partition, as one embedded stream --------
    // `write_history_log` already writes both (BL-466 appended provinces as its
    // trailing section). Calling it nests a complete IOHL stream inside this one,
    // magic and all, rather than defining those bytes a second time (NR-512).
    write_history_log(w, out);

    // --- the province holder (BL-569), APPENDED after the embedded stream ----
    // NOT part of the province partition's own section (province.hpp's
    // write_province_section / read_province_section): the partition is
    // generation output and stays fixed, while the holder is live state, so
    // it gets its own trailing section, gated by world_save_version alone,
    // rather than riding inside the partition's own magic-guarded format.
    // Placed AFTER write_history_log so read_province_section (called once,
    // at a fixed point inside read_history_log) still reads exactly its own
    // magic-prefixed section and returns before ever reaching these bytes —
    // it only reads a "clean end of stream" as the pre-BL-466 case, and there
    // is a well-formed section here, so that path is not the one taken.
    w_ids(out, w.province_holder);

    // --- the exchange record (BL-685), the v19 trailing section --------------
    // Same shape as the two sections above: live tick state, its own section.
    //
    // The RING'S CURSORS travel with the rows and are not derivable from them.
    // `entries` is the raw ring, so once it has wrapped its vector order is no
    // longer chronological — `next` is the only thing that says where the oldest
    // row sits, and a load that reset it to 0 would hand every reader the
    // history rotated. `total` is the lifetime count, which the retained rows
    // cannot reconstruct once the ring has dropped its first row.
    //
    // Written in the ring's STORED sequence, not re-ordered oldest-first, for
    // the reason the order book is written as stored: the stored sequence is
    // itself the state, and canonicalising it here would make write(read(x))
    // differ from x for a wrapped ring.
    w_vec(out, w.exchanges.entries, w_exchange);
    w_u32(out, static_cast<uint32_t>(w.exchanges.next));
    w_u64(out, static_cast<uint64_t>(w.exchanges.total));

    // BL-1125 (world_save_version 29): the fold map -- every market folded at
    // generation, by old id, with its body, centre and absorber. ROUTING STATE:
    // `market_for_tile` reads it every tick, and it cannot be derived from the
    // standing markets. A `std::map`, written ascending as held.
    w_map(out, w.folded_markets, [](std::ostream& s, const entity_id& k) { w_id(s, k); },
          [](std::ostream& s, const folded_market& f) {
              w_id(s, f.body);
              w_id(s, f.centre_tile);
              w_id(s, f.into);
          });

    // BL-1265 (v46): the held opening stock (BL-1217 D5, v39) retired — the
    // opening stock is on the shelves before the first tick.

    // BL-1296 (world_save_version 48): STATE INDUSTRY, its own trailing section.
    // Each nation holding state buildings, ascending id, with its
    // `state_assets` in STORED order (the order the funding gate spends in, so
    // the sequence is state); then every building the gate idled this tick,
    // ascending id. A trailing section rather than a field in the nation and
    // building records, so a v47 stream — which ends before it — still reads,
    // as a world with no state industry (read_world_snapshot).
    {
        std::vector<entity_id> nids;
        for (const auto& [nid, nc] : w.nations)
            if (!nc.state_assets.empty())
                nids.push_back(nid);
        std::sort(nids.begin(), nids.end());
        w_u32(out, static_cast<uint32_t>(nids.size()));
        for (const entity_id nid : nids)
        {
            w_id(out, nid);
            w_ids(out, w.nations.at(nid).state_assets);
        }
        std::vector<entity_id> idle;
        for (const auto& [bid, b] : w.buildings)
            if (b.state_idle)
                idle.push_back(bid);
        std::sort(idle.begin(), idle.end());
        w_ids(out, idle);
    }

    // BL-1255 (world_save_version 49): RAILWAYS, their own trailing section after
    // state industry. The track field — (tile, owner) ascending by tile, as the
    // std::map holds it — then every convoy riding a rail route, in `w.convoys`
    // order, as (id, boarding station, alighting station); then the owners whose
    // track the funding gate idled this tick (`rail_idle`), ascending. A trailing section, so
    // a v48 (and v47) stream — which ends before it — still reads, as a world with
    // no rail. The stations themselves are ordinary buildings in the building store.
    {
        w_u32(out, static_cast<uint32_t>(w.rail_track.size()));
        for (const auto& [tile, owner] : w.rail_track)
        {
            w_id(out, tile);
            w_id(out, owner);
        }
        uint32_t riding = 0;
        for (const convoy_component& cv : w.convoys)
            if (cv.station_a != null_entity || cv.station_b != null_entity)
                ++riding;
        w_u32(out, riding);
        for (const convoy_component& cv : w.convoys)
        {
            if (cv.station_a == null_entity && cv.station_b == null_entity)
                continue;
            w_u32(out, cv.id);
            w_id(out, cv.station_a);
            w_id(out, cv.station_b);
        }
        // The fix round: the owners whose track is idle (unfunded upkeep), ascending.
        w_ids(out, std::vector<entity_id>(w.rail_idle.begin(), w.rail_idle.end()));
    }

    // BL-1309 (world_save_version 50): THE LOGISTIC POINT SHARE RULES, a trailing
    // section after rail -- every nation that has SET one, ascending id, as (id,
    // state, champion, other). A nation with no rule shares by the mechanical
    // default and writes nothing. A v49 (and older) stream ends before it.
    {
        std::vector<entity_id> ruled;
        for (const auto& [nid, nc] : w.nations)
            if (nc.lp_shares.set)
                ruled.push_back(nid);
        std::sort(ruled.begin(), ruled.end());
        w_u32(out, static_cast<uint32_t>(ruled.size()));
        for (const entity_id nid : ruled)
        {
            w_id(out, nid);
            for (const float f : w.nations.at(nid).lp_shares.share)
                w_f32(out, f);
        }
    }

    // BL-1327 (world_save_version 51): SPACE ASSETS, a trailing section after
    // the share rules -- every asset, ascending id as the std::map holds it,
    // field by field. A world that never launched writes one empty count. A v50
    // (and older) stream ends before it.
    {
        w_u32(out, static_cast<uint32_t>(w.space_assets.size()));
        for (const auto& [aid, a] : w.space_assets)
        {
            w_id(out, aid);
            w_id(out, a.owner);
            w_enum(out, a.cls);
            w_id(out, a.dest.body);
            w_enum(out, a.dest.slot);
            w_enum(out, a.state);
            w_i32(out, a.launch_tick);
            w_i32(out, a.arrival_tick);
            w_f32(out, a.upkeep);
            w_i32(out, a.unpaid_quarters);
            w_f32(out, a.launch_cost);
        }
    }

    // BL-1335 (world_save_version 52): TAXES AND DISCONTENT, a trailing section
    // after the space assets -- every nation whose (tier, Discontent) differs
    // from the opening default, ascending id, as (id, u8 tier, f32 discontent).
    // A world where no nation has moved writes one empty count. A v51 (and
    // older) stream ends before it.
    {
        std::vector<entity_id> moved;
        for (const auto& [nid, nc] : w.nations)
            if (nc.tax_tier != k_default_tax_tier || nc.discontent != 0.0f)
                moved.push_back(nid);
        std::sort(moved.begin(), moved.end());
        w_u32(out, static_cast<uint32_t>(moved.size()));
        for (const entity_id nid : moved)
        {
            const nation_component& nc = w.nations.at(nid);
            w_id(out, nid);
            w_u8(out, static_cast<uint8_t>(nc.tax_tier));
            w_f32(out, nc.discontent);
        }
    }

    // BL-1335 review (v52, same layout — unreleased): the TAX each filed
    // quarterly return booked, so a reloaded firm's returns still telescope.
    // Kept out of the return record so v51 stays a strict prefix: every
    // corporation with any taxed return, ascending id, as (id, u32 n = its
    // return count, n x f32 in filed order). Nobody taxed: one empty count.
    {
        std::vector<entity_id> taxed;
        for (const auto& [cid, cc] : w.corporations)
            for (const quarterly_return& q : cc.returns)
                if (q.taxes != 0.0f) { taxed.push_back(cid); break; }
        std::sort(taxed.begin(), taxed.end());
        w_u32(out, static_cast<uint32_t>(taxed.size()));
        for (const entity_id cid : taxed)
        {
            const std::vector<quarterly_return>& rs = w.corporations.at(cid).returns;
            w_id(out, cid);
            w_u32(out, static_cast<uint32_t>(rs.size()));
            for (const quarterly_return& q : rs)
                w_f32(out, q.taxes);
        }
    }

    // BL-1337 (world_save_version 54): the EXACT NETS. `quarterly_return::net`
    // is a double; the record's f32 slot carries it rounded, which is exact on
    // almost every row and short by up to an ULP of the larger balance where a
    // balance crossed zero. Every corporation with any such row, ascending id,
    // as (id, u32 n = its return count, n x f64 in filed order). None: one
    // empty count. A v52 (and older) stream ends before it.
    //
    // GATED ON THE VERSION BEING WRITTEN (integration of sprints 55 and 56): the
    // reader consumes this section only at `world_save_version`, so a stream written
    // AT AN OLDER VERSION - which `write_world_snapshot_pre_road_edges` does, to give
    // BL-1340's migration a real v52 stream to read - must end before it, exactly as
    // the comment above says. Unconditional, it appended bytes no reader would eat.
    if (version == world_save_version)
    {
        std::vector<entity_id> inexact;
        for (const auto& [cid, cc] : w.corporations)
            for (const quarterly_return& q : cc.returns)
                if (static_cast<double>(static_cast<float>(q.net)) != q.net)
                {
                    inexact.push_back(cid);
                    break;
                }
        std::sort(inexact.begin(), inexact.end());
        w_u32(out, static_cast<uint32_t>(inexact.size()));
        for (const entity_id cid : inexact)
        {
            const std::vector<quarterly_return>& rs = w.corporations.at(cid).returns;
            w_id(out, cid);
            w_u32(out, static_cast<uint32_t>(rs.size()));
            for (const quarterly_return& q : rs)
                w_f64(out, q.net);
        }
    }
}

std::size_t world_save_net_section_bytes(const world& w)
{
    std::size_t n = 4;
    for (const auto& [cid, cc] : w.corporations)
        for (const quarterly_return& q : cc.returns)
            if (static_cast<double>(static_cast<float>(q.net)) != q.net)
            {
                n += 4 + 4 + 8 * cc.returns.size();
                break;
            }
    return n;
}

std::size_t world_save_tax_section_bytes(const world& w)
{
    std::size_t n = 4;
    for (const auto& [nid, nc] : w.nations)
        if (nc.tax_tier != k_default_tax_tier || nc.discontent != 0.0f)
            n += 4 + 1 + 4;
    n += 4;
    for (const auto& [cid, cc] : w.corporations)
        for (const quarterly_return& q : cc.returns)
            if (q.taxes != 0.0f) { n += 4 + 4 + 4 * cc.returns.size(); break; }
    return n;
}

void write_world_snapshot(const world& w, std::ostream& out)
{
    write_world_snapshot_at(w, out, world_save_version);
}

void write_world_snapshot_pre_road_edges(const world& w, std::ostream& out)
{
    // BL-1340, VERIFICATION ONLY (road_edges harness): the v52 stream shape — one road
    // tier a tile, the max over its sides — so the migration path has a producer. The
    // game never writes an old version; `write_world_snapshot` is the only save path.
    write_world_snapshot_at(w, out, k_world_save_version_pre_road_edges);
}

bool read_world_snapshot(world& w, std::istream& in)
{
    uint32_t magic = 0;
    if (!r_u32(in, magic) || magic != world_save_magic)
        return false; // not a world snapshot at all

    uint32_t version = 0;
    // BL-1296: v47 is the one older stream still read — it predates state
    // industry, which lives wholly in v48's trailing section, so a v47 world
    // loads byte-for-byte as before with no state buildings (no nation owns a
    // building outright; every major's strategic plants stay with the firms
    // that held them when it was saved). Every other version is refused whole.
    // BL-1255: v48 (pre-rail) is read too -- the rail section is v49's trailing
    // addition and nothing else moved, so v48 is a strict prefix of v49 as v47 is
    // of v48.
    if (!r_u32(in, version)
        || (version != world_save_version && version != k_world_save_version_pre_lp
            && version != k_world_save_version_pre_rail
            && version != k_world_save_version_pre_state
            && version != k_world_save_version_pre_space
            && version != k_world_save_version_pre_tax
            && version != k_world_save_version_pre_road_edges // BL-1340: v52, migrated
            && version != k_world_save_version_pre_net))      // BL-1337: v53, nets as f32
        return false; // mismatched version -- refuse rather than misread

    // Everything below builds into `s`, and `w` is not touched until the last
    // read has succeeded. That is what makes a failed load leave the caller's
    // world exactly as it was, rather than half-replaced.
    world s;

    entity_id player = null_entity, star = null_entity, home = null_entity;
    uint32_t  next_entity = 1, next_convoy = 1, next_trade = 1, next_proc = 1;

    if (!(r_id(in, player) && r_id(in, star) && r_id(in, home)))
        return false;
    if (!(r_f32(in, s.belt.inner_radius_au) && r_f32(in, s.belt.outer_radius_au)))
        return false;
    if (!(r_u32(in, next_entity) && r_u32(in, next_convoy) && r_u32(in, next_trade)
          && r_u32(in, next_proc)))
        return false;

    s.player_entity = player;
    s.star_body     = star;
    s.home_body     = home;
    s.set_next_entity_id(next_entity);
    s.next_convoy_id      = next_convoy;
    s.next_trade_id       = next_trade; // BL-1266 (v46)
    s.next_procurement_id = next_proc;

    // BL-1101 (format v26). A byte past `industrial` cannot have been written
    // by the writer above, so the stream is corrupt rather than odd and is
    // refused whole (the standing rejection contract; `s` is scratch, so the
    // caller's world is untouched).
    if (!r_enum(in, s.campaign_band, era_band::industrial))
        return false;

    if (!r_store(in, s.bodies, r_body))
        return false;
    // BL-1340: a pre-v53 stream's one road byte a tile lands here, keyed by the cell it
    // was read on, and becomes edges below once every tile is in.
    std::map<std::tuple<entity_id, int, int>, std::uint8_t> legacy_road;
    if (!r_store(in, s.tiles, [&](std::istream& i, tile_component& t) {
            return r_tile_v(i, t, version, &legacy_road);
        }))
        return false;
    if (version < world_save_version)
        migrate_legacy_roads(s, legacy_road);
    // THE MIRROR INVARIANT, ON LOAD (LOGISTICS.md § 4). Two bits a side can only hold
    // 0-3, so a tier is in range by construction; what a stream can still carry is two
    // halves of one crossing that disagree. A world that fails it is not a world, so the
    // whole stream is refused and nothing is mutated — `s` is scratch until the tail.
    if (!road_edges_mirrored(s))
        return false;
    if (!r_store(in, s.buildings, r_building))
        return false;
    if (!r_store(in, s.stockpiles, r_stockpile))
        return false;
    if (!r_store(in, s.markets, r_market))
        return false;
    if (!r_store(in, s.units, r_unit))
        return false;
    if (!r_store(in, s.population_centres, r_popcentre))
        return false;
    if (!r_id_store(in, s.population_centre_tile))
        return false;
    if (!r_store(in, s.population_centre_name,
                 [](std::istream& st, std::string& v) { return r_str(st, v); }))
        return false;
    // BL-612 (format v11). A value outside the five authored states cannot
    // have been written by the writer above, so the stream is corrupt rather
    // than odd and is refused whole (the standing rejection contract).
    if (!r_store(in, s.land_use, [](std::istream& st, land_use_component& v) {
            int raw = 0;
            if (!r_int(st, raw) || raw < 0
                || raw > static_cast<int>(land_use_component::type::infrastructure))
                return false;
            v.use = static_cast<land_use_component::type>(raw);
            return true;
        }))
        return false;
    if (!r_store(in, s.nations, r_nation))
        return false;
    // Sprint N3 T2 (format v3). A null key cannot have been written -- a nation
    // entity is never `null_entity` -- so one is a corruption and rejects the
    // stream whole, like every other rejection here.
    if (!r_map(in, s.nation_budgets,
               [](std::istream& st, entity_id& k) { return r_id(st, k) && k != null_entity; },
               r_nation_budget))
        return false;
    if (!r_id_store(in, s.tile_to_nation))
        return false;
    if (!r_store(in, s.corporations, r_corp))
        return false;

    if (!r_map(in, s.workforce_supply_overrides, r_id_pair,
               [](std::istream& st, float& v) { return r_f32(st, v); }))
        return false;
    // BL-546: the relational substrate, in `corp_reputation`'s old slot. A row
    // is rejected on the same grounds `read_sentiment` rejects one — a null id,
    // a self pair, a non-finite value or a stored NEUTRAL row cannot have been
    // written by the writer above, so the stream is corrupt rather than odd.
    // (`r_map` parses into a local and commits on success, so a rejection here
    // leaves the destination world untouched.)
    if (!r_map(in, s.sentiment.pairs,
               [](std::istream& st, std::pair<entity_id, entity_id>& k) {
                   return r_id_pair(st, k) && k.first != null_entity &&
                          k.second != null_entity && k.first != k.second;
               },
               [](std::istream& st, sentiment_value& v) {
                   return r_f32(st, v.access) && r_f32(st, v.trust) &&
                          std::isfinite(v.access) && std::isfinite(v.trust) && !v.neutral();
               }))
        return false;

    if (!r_vec(in, s.convoys, r_convoy))
        return false;
    if (!r_vec(in, s.trade_routes, r_route))
        return false;
    if (!r_map(in, s.body_last_glimpse_tick,
               [](std::istream& st, entity_id& k) { return r_id(st, k); },
               [](std::istream& st, int& v) { return r_int(st, v); }))
        return false;

    if (!r_vec(in, s.trades, r_trade)) // BL-1266 (v46)
        return false;
    if (!r_vec(in, s.procurement_quotes, r_quote))
        return false;
    if (!r_vec(in, s.procurement_contracts, r_contract))
        return false;
    if (!r_store(in, s.corp_embargo_conditions, r_condition_set))
        return false;

    if (!r_id_pair_set(in, s.corp_hostile_pairs))
        return false;
    if (!r_id_pair_set(in, s.corp_friend_pairs))
        return false;
    if (!r_id_pair_set(in, s.corp_friend_offers))
        return false;

    // BL-1273 (v47): the nation tech map, where the per-corp earned set and
    // corp_modifiers used to sit. An entry whose nation is not in the stream's
    // nation store is refused: the map is keyed by nation, never by anything else.
    if (!r_map(in, s.nation_tech, [](std::istream& st, entity_id& k) { return r_id(st, k); },
               r_nation_tech))
        return false;
    for (const auto& [nid, st] : s.nation_tech)
        if (s.nations.find(nid) == s.nations.end())
            return false;
    if (!r_vec(in, s.laws, r_law))
        return false;
    if (!r_vec(in, s.battles, r_battle))
        return false;

    // The embedded IOHL stream -- fills s.history_log AND s.provinces.
    if (!read_history_log(s, in))
        return false;

    // BL-569: the province holder, the trailing section written after the
    // embedded stream above. Sized independently of s.provinces here -- a
    // stream that disagrees with the partition it was written against
    // (a hand-corrupted file, never a real write path) is not this reader's
    // job to reconcile; province_holder_for's own bounds check is what stays
    // safe against that.
    if (!r_ids(in, s.province_holder))
        return false;

    // BL-685: the exchange record, format v19's trailing section. The two range
    // checks are not defensive noise — `exchange_record_ring::oldest_first`
    // indexes with `next` and would run off the end of a shorter `entries` on a
    // corrupt or hand-edited stream, and this stream reaches an AI-facing seam
    // (a save handed to `--serve`), so it is an untrusted input boundary. The
    // writer cannot produce either violation: `push` never grows past `capacity`
    // and keeps `next` in [0, capacity). Refuse whole, destination untouched.
    {
        uint32_t next_slot = 0;
        uint64_t total     = 0;
        if (!r_vec(in, s.exchanges.entries, r_exchange))
            return false;
        if (s.exchanges.entries.size() > exchange_record_ring::capacity)
            return false;
        if (!(r_u32(in, next_slot) && r_u64(in, total)))
            return false;
        if (next_slot >= exchange_record_ring::capacity)
            return false;
        s.exchanges.next  = static_cast<std::size_t>(next_slot);
        s.exchanges.total = static_cast<std::size_t>(total);
    }

    // BL-1125 (v29): the fold map. Untrusted on the same footing as the ring
    // above: a record must name a body, a tile on that body, and an absorber
    // that is a STANDING market on that body, and its own id must not be a
    // standing market -- the writer can produce none of those violations, and
    // a record that broke one would route tiles to a market that is not there.
    if (!r_map(in, s.folded_markets,
               [](std::istream& st, entity_id& k) { return r_id(st, k) && k != null_entity; },
               [](std::istream& st, folded_market& f) {
                   return r_id(st, f.body) && r_id(st, f.centre_tile) && r_id(st, f.into);
               }))
        return false;
    for (const auto& [fid, fm] : s.folded_markets)
    {
        if (s.markets.count(fid) != 0) return false;
        const auto into = s.markets.find(fm.into);
        if (into == s.markets.end() || into->second.body != fm.body) return false;
        const auto tile = s.tiles.find(fm.centre_tile);
        if (tile == s.tiles.end() || tile->second.body != fm.body) return false;
    }

    // BL-1296 (v48): the state-industry section. Untrusted on the ring's
    // footing: every entry must name a nation in the stream and a building in
    // the stream that no corporation and no other nation owns, at most once —
    // the only state the handover and the state build door can write — and an
    // idled building must be a state building. Refused whole otherwise. A v47
    // stream ends before this section: no state industry.
    if (version >= k_world_save_version_pre_rail) // v48 and v49 carry it
    {
        uint32_t count = 0;
        if (!r_u32(in, count) || count > s.nations.size())
            return false;
        std::set<entity_id> owned;
        for (const auto& [cid, cc] : s.corporations)
            for (const entity_id bid : cc.assets)
                owned.insert(bid);
        std::set<entity_id> state_owned;
        entity_id prev = null_entity;
        for (uint32_t k = 0; k < count; ++k)
        {
            entity_id nid = null_entity;
            if (!r_id(in, nid) || nid <= prev)
                return false; // ascending, distinct, never null
            prev = nid;
            const auto nit = s.nations.find(nid);
            if (nit == s.nations.end())
                return false;
            std::vector<entity_id> assets;
            if (!r_ids(in, assets) || assets.empty())
                return false;
            for (const entity_id bid : assets)
            {
                if (s.buildings.count(bid) == 0 || owned.count(bid) != 0
                    || !state_owned.insert(bid).second)
                    return false;
            }
            nit->second.state_assets = std::move(assets);
        }
        std::vector<entity_id> idle;
        if (!r_ids(in, idle))
            return false;
        for (const entity_id bid : idle)
        {
            if (state_owned.count(bid) == 0)
                return false;
            s.buildings.at(bid).state_idle = true;
        }
    }

    // BL-1255 (v49): the rail section. Untrusted: every track tile exists in the
    // stream, strictly ascending, and its owner is null (laid, not yet handed over)
    // or a nation or corporation in the stream; every riding convoy names a convoy
    // in the stream once, and two distinct tiles in the stream as its stations.
    // Refused whole otherwise. A v48 / v47 stream ends before it: no rail.
    if (version >= k_world_save_version_pre_lp) // v49 and v50 carry it
    {
        uint32_t count = 0;
        if (!r_u32(in, count) || count > s.tiles.size())
            return false;
        entity_id prev = null_entity;
        for (uint32_t k = 0; k < count; ++k)
        {
            entity_id tile = null_entity, owner = null_entity;
            if (!r_id(in, tile) || !r_id(in, owner))
                return false;
            if (tile == null_entity || (k > 0 && tile <= prev) || s.tiles.count(tile) == 0)
                return false;
            prev = tile;
            if (owner != null_entity && s.nations.count(owner) == 0
                && s.corporations.count(owner) == 0)
                return false;
            s.rail_track.emplace_hint(s.rail_track.end(), tile, owner);
        }
        uint32_t riding = 0;
        if (!r_u32(in, riding) || riding > s.convoys.size())
            return false;
        std::set<uint32_t> seen;
        for (uint32_t k = 0; k < riding; ++k)
        {
            uint32_t id = 0;
            entity_id a = null_entity, b = null_entity;
            if (!r_u32(in, id) || !r_id(in, a) || !r_id(in, b))
                return false;
            if (!seen.insert(id).second || a == null_entity || b == null_entity || a == b
                || s.tiles.count(a) == 0 || s.tiles.count(b) == 0)
                return false;
            convoy_component* hit = nullptr;
            for (convoy_component& cv : s.convoys)
                if (cv.id == id) { hit = &cv; break; }
            if (hit == nullptr)
                return false;
            hit->station_a = a;
            hit->station_b = b;
        }
        // Idle track owners: ascending, distinct, each an owner of track in the stream.
        std::vector<entity_id> idle;
        if (!r_ids(in, idle))
            return false;
        entity_id last = null_entity;
        for (std::size_t k = 0; k < idle.size(); ++k)
        {
            const entity_id o = idle[k];
            if (o == null_entity || (k > 0 && o <= last))
                return false;
            last = o;
            bool owns = false;
            for (const auto& [t, own] : s.rail_track)
                if (own == o) { owns = true; break; }
            if (!owns)
                return false;
            s.rail_idle.insert(o);
        }
    }

    // BL-1309 (v50): the share rules. Untrusted: each names a nation in the
    // stream, strictly ascending, with three finite shares in [0,1] summing to 1
    // (the seam's own rule, `lp_share_rule_valid`). Refused whole otherwise.
    if (version >= k_world_save_version_pre_space) // v50 and v51 carry it
    {
        uint32_t count = 0;
        if (!r_u32(in, count) || count > s.nations.size())
            return false;
        entity_id prev = null_entity;
        for (uint32_t k = 0; k < count; ++k)
        {
            entity_id nid = null_entity;
            std::array<float, 3> sh{};
            if (!r_id(in, nid) || !r_f32(in, sh[0]) || !r_f32(in, sh[1]) || !r_f32(in, sh[2]))
                return false;
            if (nid == null_entity || (k > 0 && nid <= prev) || s.nations.count(nid) == 0
                || !lp_share_rule_valid(sh))
                return false;
            prev = nid;
            s.nations.at(nid).lp_shares.set   = true;
            s.nations.at(nid).lp_shares.share = sh;
        }
    }

    // BL-1327 (v51): the space assets. Untrusted: ids strictly ascending, never
    // null, below the allocator cursor and naming no nation or body; the owner a
    // nation in the stream; the destination a body in the stream; every enum in
    // range; arrival at or after launch; upkeep finite and non-negative; the
    // unpaid count non-negative. Refused whole otherwise. A v50 (and older)
    // stream ends before it: a world with no space assets.
    if (version >= k_world_save_version_pre_tax) // v51 and v52 carry it
    {
        uint32_t count = 0;
        if (!r_u32(in, count) || count > max_count)
            return false;
        entity_id prev = null_entity;
        for (uint32_t k = 0; k < count; ++k)
        {
            space_asset a;
            entity_id   aid = null_entity;
            if (!r_id(in, aid) || !r_id(in, a.owner)
                || !r_enum(in, a.cls, space_asset_class::station_module)
                || !r_id(in, a.dest.body)
                || !r_enum(in, a.dest.slot, space_slot::surface)
                || !r_enum(in, a.state, space_asset_state::lost)
                || !r_i32(in, a.launch_tick) || !r_i32(in, a.arrival_tick)
                || !r_f32(in, a.upkeep) || !r_i32(in, a.unpaid_quarters)
                || !r_f32(in, a.launch_cost))
                return false;
            // The id: ascending, live in the allocator, and owned by NO other
            // entity kind (every store whose key is an entity's own id).
            if (aid == null_entity || (k > 0 && aid <= prev) || aid >= next_entity
                || s.nations.count(aid) != 0 || s.bodies.count(aid) != 0
                || s.tiles.count(aid) != 0 || s.buildings.count(aid) != 0
                || s.markets.count(aid) != 0 || s.units.count(aid) != 0
                || s.population_centres.count(aid) != 0 || s.corporations.count(aid) != 0)
                return false;
            prev = aid;
            // Owner and destination: a nation; a body with a slot (never the star),
            // in a slot the class can EVER occupy (the home-independent half of
            // space_launch_legal — the home half needs the owner's capital, which
            // a legal launch already checked and nothing later moves).
            const auto dbit = s.bodies.find(a.dest.body);
            if (s.nations.count(a.owner) == 0 || dbit == s.bodies.end()
                || dbit->second.type == body_type::star || !space_slot_legal(a.cls, a.dest.slot))
                return false;
            if (a.arrival_tick < a.launch_tick || !std::isfinite(a.upkeep) || a.upkeep < 0.0f
                || !std::isfinite(a.launch_cost) || a.launch_cost < 0.0f)
                return false;
            // State and unpaid count agree: the step writes 0 for in-transit and
            // active, 1..loss-1 for decaying, and the loss count (<= the ceiling)
            // for lost. The reader cannot see the registry's dial, so it holds the
            // count to the dial's hard ceiling.
            const int32_t u = a.unpaid_quarters;
            const int32_t cap = k_space_quarters_to_loss_ceiling;
            bool state_ok = false;
            switch (a.state)
            {
                case space_asset_state::in_transit: state_ok = (u == 0); break;
                case space_asset_state::active:     state_ok = (u == 0); break;
                case space_asset_state::decaying:   state_ok = (u >= 1 && u <= cap - 1); break;
                case space_asset_state::lost:       state_ok = (u >= 1 && u <= cap); break;
            }
            if (!state_ok)
                return false;
            a.id = aid;
            s.space_assets.emplace_hint(s.space_assets.end(), aid, a);
        }
    }

    // BL-1335 (v52): taxes and Discontent. Untrusted: each names a nation in
    // the stream, strictly ascending, with a tier in 1..5 and a Discontent
    // finite in [0, 1]. Refused whole otherwise. A v51 (and older) stream ends
    // before it: every nation at tier III with no Discontent (the defaults).
    if (version >= k_world_save_version_pre_net) // v52 and v54 carry it
    {
        uint32_t count = 0;
        if (!r_u32(in, count) || count > s.nations.size())
            return false;
        entity_id prev = null_entity;
        for (uint32_t k = 0; k < count; ++k)
        {
            entity_id nid  = null_entity;
            uint8_t   tier = 0;
            float     d    = 0.0f;
            if (!r_id(in, nid) || !r_u8(in, tier) || !r_f32(in, d))
                return false;
            if (nid == null_entity || (k > 0 && nid <= prev) || s.nations.count(nid) == 0
                || !tax_tier_valid(static_cast<int>(tier)) || !discontent_valid(d))
                return false;
            prev = nid;
            s.nations.at(nid).tax_tier   = static_cast<int>(tier);
            s.nations.at(nid).discontent = d;
        }

        // The returns' taxes. Untrusted: each names a corporation in the stream,
        // strictly ascending, with exactly its return count of finite,
        // non-negative figures. Refused whole otherwise.
        uint32_t ccount = 0;
        if (!r_u32(in, ccount) || ccount > s.corporations.size())
            return false;
        entity_id cprev = null_entity;
        for (uint32_t k = 0; k < ccount; ++k)
        {
            entity_id cid = null_entity;
            uint32_t  n   = 0;
            if (!r_id(in, cid) || !r_u32(in, n))
                return false;
            const auto cit = s.corporations.find(cid);
            if (cid == null_entity || (k > 0 && cid <= cprev) || cit == s.corporations.end()
                || n != cit->second.returns.size())
                return false;
            cprev = cid;
            for (quarterly_return& q : cit->second.returns)
            {
                float t = 0.0f;
                if (!r_f32(in, t) || !std::isfinite(t) || t < 0.0f)
                    return false;
                q.taxes = t;
            }
        }
    }

    // BL-1337 (v54): the exact nets. Untrusted: each names a corporation in the
    // stream, strictly ascending, with exactly its return count of finite
    // figures, each of which rounds to the f32 slot its record already carried
    // (the writer wrote that slot from this very double). Refused whole
    // otherwise. A v52 (and older) stream ends before it: every net is its slot.
    if (version == world_save_version)
    {
        uint32_t ccount = 0;
        if (!r_u32(in, ccount) || ccount > s.corporations.size())
            return false;
        entity_id cprev = null_entity;
        for (uint32_t k = 0; k < ccount; ++k)
        {
            entity_id cid = null_entity;
            uint32_t  n   = 0;
            if (!r_id(in, cid) || !r_u32(in, n))
                return false;
            const auto cit = s.corporations.find(cid);
            if (cid == null_entity || (k > 0 && cid <= cprev) || cit == s.corporations.end()
                || n != cit->second.returns.size())
                return false;
            cprev = cid;
            for (quarterly_return& q : cit->second.returns)
            {
                double v = 0.0;
                // Range-checked before the narrowing cast: a finite double can
                // be an infinite float (io-standing-rules, the seam rule).
                if (!r_f64(in, v) || !std::isfinite(v)
                    || std::fabs(v) > static_cast<double>(std::numeric_limits<float>::max())
                    || static_cast<double>(static_cast<float>(v)) != q.net)
                    return false;
                q.net = v;
            }
        }
    }

    // BL-1266 (v46): every manual trade names a live owner and two distinct
    // standing markets, and holds an id below the next one to be issued, all
    // ids distinct — the only trades the seam can write. Refused whole.

    {
        std::set<uint32_t> ids;
        std::map<entity_id, std::size_t> per_owner;
        for (const standing_trade& t : s.trades)
        {
            if (++per_owner[t.owner] > 64) return false; // max_trades_per_corp
            if (s.corporations.count(t.owner) == 0) return false;
            if (t.from_market == t.to_market) return false;
            if (s.markets.count(t.from_market) == 0 || s.markets.count(t.to_market) == 0) return false;
            if (t.id >= s.next_trade_id || !ids.insert(t.id).second) return false;
        }
    }

    clear_derived_state(s);
    w = std::move(s); // replace only on full success
    return true;
}
