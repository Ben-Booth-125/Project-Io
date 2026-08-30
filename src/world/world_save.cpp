#include "world_save.hpp"

#include "binary_io.hpp"
#include "history_log.hpp" // write_history_log / read_history_log -- the embedded section

#include <algorithm>
#include <cmath> // std::isfinite -- the sentiment record's own rejection (BL-546)
#include <istream>
#include <ostream>
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
constexpr auto max_building   = building_type::university; // BL-615: appended schooling/university.
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
constexpr auto max_cond_subj  = condition_subject::province_held; // BL-570: appended after science
constexpr auto max_cond_cmp   = condition_comparator::less_than;
constexpr auto max_mod_subj   = modifier_subject::collapse_strain;
constexpr auto max_mod_op     = modifier_op::multiply;
constexpr auto max_unit_class = unit_class::naval;
constexpr auto max_stance     = siege_stance::invest;
constexpr auto max_season     = season::winter;
constexpr auto max_battle_res = battle_result::defender_victory;
constexpr auto max_battle_end = campaign_battle_end::stalemate;
constexpr auto max_withdraw   = withdrawing_side::defender;
constexpr auto max_contract_state = mercenary_contract_state::abandoned; // BL-573

// ---------------------------------------------------------------------------
// Component writers / readers
// ---------------------------------------------------------------------------
// One pair per struct, fields in declaration order. Declaration order is not
// required by anything -- the stream only needs the two sides to agree -- but
// keeping it makes a field addition a two-line diff in one obvious place
// instead of a hunt.

void w_tile(std::ostream& o, const tile_component& t)
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
    w_u8(o, t.road_level);
    w_u8(o, t.river_edges);
    w_u8(o, t.river_downstream);
    w_f32(o, t.height);
}

bool r_tile(std::istream& i, tile_component& t)
{
    return r_id(i, t.body) && r_int(i, t.grid_x) && r_int(i, t.grid_y)
        && r_enum(i, t.substrate, max_substrate) && r_enum(i, t.cover, max_cover)
        && r_u8(i, t.cover_density) && r_enum(i, t.landform, max_landform)
        && r_f32_array(i, t.resource_deposit) && r_f32_array(i, t.resource_remaining)
        && r_f32(i, t.hazard_level) && r_f32(i, t.habitability)
        && r_f32(i, t.substrate_density) && r_u8(i, t.road_level)
        && r_u8(i, t.river_edges) && r_u8(i, t.river_downstream) && r_f32(i, t.height);
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
        && r_int(i, b.recipe_switch_cooldown)))
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
}

bool r_market(std::istream& i, market_component& m)
{
    return r_id(i, m.body) && r_id(i, m.centre_tile) && r_f32_array(i, m.supply)
        && r_f32_array(i, m.demand) && r_f32_array(i, m.price)
        && r_f32_array(i, m.base_price) && r_f32_array(i, m.inventory);
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

/// Sprint N3 T2: one nation's budget -- the nine line weights in authored enum
/// order, then the reserve fraction. Fixed-width (`w_f32_array` writes no
/// count), so `priority_count` is part of the format: appending a line to
/// `budget_priority` widens every record and is a version bump.
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
    w_f32(o, q.net);
    w_f32(o, q.balance);
    w_u32(o, q.holdings);
    w_f32(o, q.book_value);
}

bool r_return(std::istream& i, quarterly_return& q)
{
    if (!(r_f32(i, q.income) && r_f32(i, q.expenditure) && r_f32(i, q.maintenance)
          && r_f32(i, q.wages) && r_f32(i, q.interest) && r_f32(i, q.levies)
          && r_f32(i, q.upkeep) && r_f32(i, q.net) && r_f32(i, q.balance)
          && r_u32(i, q.holdings) && r_f32(i, q.book_value)))
        return false;
    // The writer cannot have produced a non-finite figure — the money loop that
    // fills the record is finite by construction — so one here means the stream
    // is corrupt rather than merely odd. Same rejection shape as the sentiment
    // record's (BL-546).
    return std::isfinite(q.income) && std::isfinite(q.expenditure)
        && std::isfinite(q.maintenance) && std::isfinite(q.wages)
        && std::isfinite(q.interest) && std::isfinite(q.levies)
        && std::isfinite(q.upkeep) && std::isfinite(q.net) && std::isfinite(q.balance)
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
    w_f32(o, c.science);
    w_bool_array(o, c.produced_ever);
    w_vec(o, c.returns, w_return); // BL-626: world_save_version 16
}

bool r_corp(std::istream& i, corporation_component& c)
{
    // Field order is the declaration order, and BOTH of Sprint 20 wave 1's
    // additions sit in it: `ownership_class` (BL-631) immediately after `focus`,
    // `returns` (BL-626) last, after `produced_ever`. The write side below must
    // match this exactly.
    if (!(r_str(i, c.name) && r_id(i, c.home_nation) && r_enum(i, c.focus, max_focus)
          && r_enum(i, c.ownership_class, max_ownership)
          && r_f32(i, c.starting_capital) && r_f32(i, c.balance) && r_bool(i, c.is_player)
          && r_bool(i, c.is_background) && r_ids(i, c.assets) && r_id(i, c.hq_building)
          && r_f32(i, c.influence_range) && r_f32(i, c.science)
          && r_bool_array(i, c.produced_ever) && r_vec(i, c.returns, r_return)))
        return false;
    // BL-626: retention is bounded by the writer, so a longer run is a corrupt
    // stream, not a longer history. Refused rather than trimmed — trimming would
    // silently pick which quarters to believe.
    return c.returns.size() <= k_quarterly_return_retention;
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
}

bool r_convoy(std::istream& i, convoy_component& c)
{
    return r_id(i, c.source_market) && r_id(i, c.dest_market) && r_enum(i, c.mode, max_convoy)
        && r_enum(i, c.cargo_resource, max_resource) && r_f32(i, c.cargo_qty)
        && r_f32(i, c.progress) && r_f32(i, c.speed) && r_id(i, c.corp) && r_bool(i, c.arrived)
        && r_u32(i, c.id) && r_bool(i, c.held) && r_f32(i, c.cost_paid);
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

void w_sell(std::ostream& o, const sell_order& s)
{
    w_u32(o, s.id);
    w_id(o, s.corp);
    w_id(o, s.body);
    w_enum(o, s.resource);
    w_f32(o, s.quantity);
    w_f32(o, s.floor_price);
}

bool r_sell(std::istream& i, sell_order& s)
{
    return r_u32(i, s.id) && r_id(i, s.corp) && r_id(i, s.body)
        && r_enum(i, s.resource, max_resource) && r_f32(i, s.quantity) && r_f32(i, s.floor_price);
}

void w_buy(std::ostream& o, const buy_order& b)
{
    w_u32(o, b.id);
    w_id(o, b.corp);
    w_id(o, b.body);
    w_enum(o, b.resource);
    w_f32(o, b.quantity);
    w_f32(o, b.max_price);
    w_id(o, b.preferred_seller);
}

bool r_buy(std::istream& i, buy_order& b)
{
    return r_u32(i, b.id) && r_id(i, b.corp) && r_id(i, b.body)
        && r_enum(i, b.resource, max_resource) && r_f32(i, b.quantity) && r_f32(i, b.max_price)
        && r_id(i, b.preferred_seller);
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
}

bool r_exchange(std::istream& i, exchange_record& e)
{
    return r_int(i, e.tick) && r_id(i, e.market) && r_enum(i, e.resource, max_resource)
        && r_f32(i, e.quantity) && r_f32(i, e.unit_price) && r_id(i, e.seller)
        && r_id(i, e.buyer);
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

/// BL-572: a mercenary contract offer. Rejects a non-finite `fee`/`offer_escrow`
/// on the same grounds `r_nation_budget` does — the writer below cannot have
/// produced one, so the stream is corrupt rather than odd.
void w_offer(std::ostream& o, const mercenary_offer& m)
{
    w_u32(o, m.id);
    w_id(o, m.client);
    w_u32(o, m.target_province);
    w_int(o, m.template_index);
    w_f32(o, m.fee);
    w_int(o, m.deadline);
    w_int(o, m.issued_tick);
    w_f32(o, m.offer_escrow);
}

bool r_offer(std::istream& i, mercenary_offer& m)
{
    if (!(r_u32(i, m.id) && r_id(i, m.client) && r_u32(i, m.target_province)
          && r_int(i, m.template_index) && r_f32(i, m.fee) && r_int(i, m.deadline)
          && r_int(i, m.issued_tick) && r_f32(i, m.offer_escrow)))
        return false;
    return std::isfinite(m.fee) && std::isfinite(m.offer_escrow);
}

/// BL-573: an accepted mercenary contract. Rejects a non-finite `fee`/
/// `deposit_paid` on the same grounds `r_offer` does above.
void w_mercenary_contract(std::ostream& o, const mercenary_contract& c)
{
    w_u32(o, c.id);
    w_id(o, c.client);
    w_id(o, c.contractor);
    w_int(o, c.template_index);
    w_u32(o, c.province);
    w_f32(o, c.fee);
    w_f32(o, c.deposit_paid);
    w_int(o, c.deadline);
    w_int(o, c.accepted_tick);
    w_id_array(o, c.units);
    w_enum(o, c.state);
}

bool r_mercenary_contract(std::istream& i, mercenary_contract& c)
{
    if (!(r_u32(i, c.id) && r_id(i, c.client) && r_id(i, c.contractor)
          && r_int(i, c.template_index) && r_u32(i, c.province) && r_f32(i, c.fee)
          && r_f32(i, c.deposit_paid) && r_int(i, c.deadline) && r_int(i, c.accepted_tick)
          && r_id_array(i, c.units) && r_enum(i, c.state, max_contract_state)))
        return false;
    return std::isfinite(c.fee) && std::isfinite(c.deposit_paid);
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

void w_modifier(std::ostream& o, const scalar_modifier& m)
{
    w_enum(o, m.subject);
    w_enum(o, m.op);
    w_f32(o, m.magnitude);
}

bool r_modifier(std::istream& i, scalar_modifier& m)
{
    return r_enum(i, m.subject, max_mod_subj) && r_enum(i, m.op, max_mod_op)
        && r_f32(i, m.magnitude);
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
}

bool r_battle(std::istream& i, active_battle& b)
{
    return r_u32(i, b.province) && r_id(i, b.attacker) && r_id(i, b.defender)
        && r_ids(i, b.attacker_units) && r_ids(i, b.defender_units)
        && r_battle_state(i, b.state) && r_enum(i, b.withdraw_requested, max_withdraw);
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
    w.body_reach_cost.clear();

    // The market index carries its own staleness stamps; zeroing them is what
    // makes the next `market_for_tile` rebuild rather than trust an empty index.
    w.body_market_index.clear();
    w.body_market_index_count  = 0;
    w.body_market_index_max_id = null_entity;

    // Observability, not state: the chat feed and the harness read this ring.
    // A loaded campaign starts with an empty decision log rather than a
    // resurrected one, which is honest -- those decisions were taken in a
    // session that has ended.
    w.ai_decisions.entries.clear();
    w.ai_decisions.next  = 0;
    w.ai_decisions.total = 0;

    // Mirrored from the sim loop each frame by app; the envelope carries the
    // authoritative tick.
    w.current_day_tick  = 0;
    w.current_econ_tick = 0;
}

void write_world_snapshot(const world& w, std::ostream& out)
{
    w_u32(out, world_save_magic);
    w_u32(out, world_save_version);

    // --- well-known entities, the belt, and the allocator cursor -------------
    w_id(out, w.player_entity);
    w_id(out, w.star_body);
    w_id(out, w.home_body);
    w_f32(out, w.belt.inner_radius_au);
    w_f32(out, w.belt.outer_radius_au);
    w_u32(out, w.next_entity_id());
    w_u32(out, w.next_convoy_id);
    w_u32(out, w.next_order_id);
    w_u32(out, w.next_procurement_id);

    // --- component stores ---------------------------------------------------
    w_store(out, w.bodies, w_body);
    w_store(out, w.tiles, w_tile);
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
    w_map(out, w.corp_body_pools, w_id_pair, w_stockpile);
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

    // --- the order book and procurement -------------------------------------
    w_vec(out, w.sell_orders, w_sell);
    w_vec(out, w.buy_orders, w_buy);
    w_vec(out, w.procurement_quotes, w_quote);
    w_vec(out, w.procurement_contracts, w_contract);
    w_store(out, w.corp_embargo_conditions, w_condition_set);

    // --- stance (BL-448) ----------------------------------------------------
    // Written in the sets' own sorted order, which is why they need no re-sort
    // on the way back in.
    w_id_pair_set(out, w.corp_hostile_pairs);
    w_id_pair_set(out, w.corp_friend_pairs);
    w_id_pair_set(out, w.corp_friend_offers);

    // --- research, law, and the modifiers that are NOT recomputable ----------
    w_map(out, w.earned_techs, [](std::ostream& s, const entity_id& k) { w_id(s, k); },
          [](std::ostream& s, const std::set<std::string>& v) {
              w_count(s, v.size());
              for (const std::string& id : v)
                  w_str(s, id);
          });
    w_vec(out, w.laws, w_law);
    w_map(out, w.corp_modifiers, [](std::ostream& s, const entity_id& k) { w_id(s, k); },
          [](std::ostream& s, const std::vector<scalar_modifier>& v) { w_vec(s, v, w_modifier); });

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

    // --- open mercenary-contract offers (BL-572), the v7 trailing section ---
    // Same shape as province_holder above: live tick state, not generation
    // output, so it gets its own section rather than riding inside any
    // magic-guarded embedded stream. The allocator cursor travels with it,
    // exactly like `next_convoy_id`/`next_order_id`/`next_procurement_id` do
    // near the top of this function — a load that reset it to 1 would mint an
    // id a live offer already holds.
    w_u32(out, w.next_offer_id);
    w_vec(out, w.mercenary_offers, w_offer);

    // --- accepted mercenary contracts (BL-573), the v8 trailing section -----
    // Same shape as mercenary_offers above: live tick state, its own section,
    // allocator cursor travels with it.
    w_u32(out, w.next_contract_id);
    w_vec(out, w.mercenary_contracts, w_mercenary_contract);

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
}

bool read_world_snapshot(world& w, std::istream& in)
{
    uint32_t magic = 0;
    if (!r_u32(in, magic) || magic != world_save_magic)
        return false; // not a world snapshot at all

    uint32_t version = 0;
    if (!r_u32(in, version) || version != world_save_version)
        return false; // mismatched version -- refuse rather than misread

    // Everything below builds into `s`, and `w` is not touched until the last
    // read has succeeded. That is what makes a failed load leave the caller's
    // world exactly as it was, rather than half-replaced.
    world s;

    entity_id player = null_entity, star = null_entity, home = null_entity;
    uint32_t  next_entity = 1, next_convoy = 1, next_order = 1, next_proc = 1;

    if (!(r_id(in, player) && r_id(in, star) && r_id(in, home)))
        return false;
    if (!(r_f32(in, s.belt.inner_radius_au) && r_f32(in, s.belt.outer_radius_au)))
        return false;
    if (!(r_u32(in, next_entity) && r_u32(in, next_convoy) && r_u32(in, next_order)
          && r_u32(in, next_proc)))
        return false;

    s.player_entity = player;
    s.star_body     = star;
    s.home_body     = home;
    s.set_next_entity_id(next_entity);
    s.next_convoy_id      = next_convoy;
    s.next_order_id       = next_order;
    s.next_procurement_id = next_proc;

    if (!r_store(in, s.bodies, r_body))
        return false;
    if (!r_store(in, s.tiles, r_tile))
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

    if (!r_map(in, s.corp_body_pools, r_id_pair, r_stockpile))
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

    if (!r_vec(in, s.sell_orders, r_sell))
        return false;
    if (!r_vec(in, s.buy_orders, r_buy))
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

    if (!r_map(in, s.earned_techs, [](std::istream& st, entity_id& k) { return r_id(st, k); },
               [](std::istream& st, std::set<std::string>& v) {
                   uint32_t n = 0;
                   if (!r_count(st, n))
                       return false;
                   v.clear();
                   for (uint32_t i = 0; i < n; ++i)
                   {
                       std::string id;
                       if (!r_str(st, id))
                           return false;
                       v.insert(std::move(id));
                   }
                   return true;
               }))
        return false;
    if (!r_vec(in, s.laws, r_law))
        return false;
    if (!r_map(in, s.corp_modifiers, [](std::istream& st, entity_id& k) { return r_id(st, k); },
               [](std::istream& st, std::vector<scalar_modifier>& v) {
                   return r_vec(st, v, r_modifier);
               }))
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

    // BL-572: open mercenary-contract offers, format v7's trailing section.
    uint32_t next_offer = 1;
    if (!(r_u32(in, next_offer) && r_vec(in, s.mercenary_offers, r_offer)))
        return false;
    s.next_offer_id = next_offer;

    // BL-573: accepted mercenary contracts, format v8's trailing section.
    uint32_t next_contract = 1;
    if (!(r_u32(in, next_contract) && r_vec(in, s.mercenary_contracts, r_mercenary_contract)))
        return false;
    s.next_contract_id = next_contract;

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

    clear_derived_state(s);
    w = std::move(s); // replace only on full success
    return true;
}
