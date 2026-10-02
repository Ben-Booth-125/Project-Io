// ---------------------------------------------------------------------------
// world_cursor_equivalence — BL-1084 (world built once and moved)
// ---------------------------------------------------------------------------
// `make_hard_coded_world` is a composition of six stage functions over one
// `generation_cursor` (world/generation_cursor.hpp), so a wizard round can take
// the world the round before it closed and run only its own stage on it. That
// is only sound if a stage run on a COPY of its predecessor's cursor builds
// exactly what the single call builds. This harness is the proof, on the
// sixteen curated seeds (docs/generation/seed_library.json), with the app's
// generation inputs (the parsed world_gen.lua and the works table, as
// harness_params.hpp's app-order builder passes them):
//
//   (a) THE COMPOSITION IN PLACE: one cursor, run stage by stage, digested at
//       every round boundary (life_gate, culture, empires, exploration,
//       industrialisation, tail). No copy anywhere.
//   (b) THE STAGED BUILD: at every boundary the cursor, its report and its
//       fixture are copied, the copy is rebound to its own report and fixture,
//       the ORIGINAL IS FREED, and the next stage runs on the copy -- the
//       wizard's slot hand-off. A member that did not copy as a value, or a
//       pointer into the freed predecessor, shows here as a digest that moves.
//   (c) THE ONE CALL: `make_hard_coded_world` itself, compared with (a)'s tail.
//
// At every boundary (a) and (b) must agree on:
//   owner_changes  every ownership record on the report -- the migration's,
//                  the Empires span's, Exploration's and Industrialisation's
//                  `era_timelapse` (changes, culture changes, events, samples);
//   polities       the polity tables the spans closed on (the cursor's 1200 and
//                  1660 handoffs, and the fixture's three captures);
//   world          world_determinism's deep digest (world_deep_digest.hpp,
//                  shared, not rewritten) and its tile metrics;
//   cursor         the cursor's own members, which no world digest can see;
//   report         the report's counters, lines and body entries.
//
// NON-VACUITY: on the shipped params every span runs, so each boundary's
// owner-change digest must differ from the one before it, and each stage must
// move the cursor's STATE (the digest without the stage counter, which would
// differ at every boundary by construction) -- a digest that could not see a
// stage would pass (a) == (b) while comparing nothing.
//
// NEGATIVE CONTROLS (the K1/K2 cold review): at the culture and exploration
// boundaries one member of a copied cursor is perturbed -- a settlement history
// line, an Exploration sea leg -- and the SAME comparison the proof uses must
// report the difference. Every record the tail or a later span reads is folded
// by content, not by size, so a copy that kept a table's count and lost its
// rows cannot pass for a faithful one.
//
// --measure (K3): on the given seeds (default 0 and 28), no tail, the cost of
// copying a closed stage's cursor at each boundary against the cost of
// REPLAYING up to that boundary -- from a held Life-gate cursor and from the
// params alone -- and asserts each replay lands on the walk's own digests.
// Timings are wall clock on whatever machine runs it: reported, never asserted.
//
// Usage:
//   world_cursor_equivalence [--seeds a,b,c] [--no-one-call]
//   world_cursor_equivalence --measure [--seeds 0,28] [--reps N]
//
// Build (needs a live Lua state):  bash tools/verify/build_lua_harness.sh world_cursor_equivalence
// Run from the repo root (it reads scripts/*.lua and the seed library).

#include "harness_params.hpp"      // load_app_generation_inputs, the app's inputs
#include "world_deep_digest.hpp"   // world_metrics, measure, deep_digest (shared)

#include "scripting/lua_state.hpp"
#include "world/era_minus_one.hpp"
#include "world/generation_cursor.hpp"
#include "world/hard_coded_world.hpp"
#include "world/works_roster.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& label)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", label.c_str());
    std::fflush(stdout);
    if (!ok) ++failures;
}

constexpr generation_stage k_boundaries[] = {
    generation_stage::life_gate, generation_stage::culture, generation_stage::empires,
    generation_stage::exploration, generation_stage::industrialisation, generation_stage::tail,
};
constexpr int k_boundary_count = static_cast<int>(sizeof(k_boundaries) / sizeof(k_boundaries[0]));

const char* stage_name(generation_stage s)
{
    switch (s)
    {
        case generation_stage::none:              return "none";
        case generation_stage::life_gate:         return "life_gate";
        case generation_stage::culture:           return "culture";
        case generation_stage::empires:           return "empires";
        case generation_stage::exploration:       return "exploration";
        case generation_stage::industrialisation: return "industrialisation";
        case generation_stage::tail:              return "tail";
    }
    return "?";
}

// --- folds -------------------------------------------------------------------
// FNV-1a through world_deep_digest.hpp's own fold_* helpers. Structs are folded
// FIELD BY FIELD, never as bytes: padding is not state.

void fold_u8(uint64_t& h, uint8_t v) { fold_bytes(h, &v, sizeof v); }
void fold_bool(uint64_t& h, bool v) { fold_u8(h, v ? 1 : 0); }
void fold_u64(uint64_t& h, uint64_t v) { fold_bytes(h, &v, sizeof v); }

template <typename T>
void fold_ints(uint64_t& h, const std::vector<T>& v)
{
    fold_u32(h, static_cast<uint32_t>(v.size()));
    for (const T x : v) fold_i64(h, static_cast<int64_t>(x));
}

void fold_strs(uint64_t& h, const std::vector<std::string>& v)
{
    fold_u32(h, static_cast<uint32_t>(v.size()));
    for (const std::string& s : v) fold_str(h, s);
}

void fold_timelapse(uint64_t& h, const era_timelapse& t)
{
    fold_u32(h, static_cast<uint32_t>(t.changes.size()));
    for (const owner_change& c : t.changes)
    {
        fold_i32(h, c.year);
        fold_u32(h, c.region);
        fold_u32(h, c.owner);
    }
    fold_i32(h, t.region_stride);
    fold_i32(h, t.start_year);
    fold_i32(h, t.years);
    fold_u32(h, static_cast<uint32_t>(t.culture_changes.size()));
    for (const culture_change& c : t.culture_changes)
    {
        fold_i32(h, c.year);
        fold_u32(h, c.region);
        fold_i32(h, c.other_q);
    }
    fold_u32(h, static_cast<uint32_t>(t.events.size()));
    for (const lapse_event& e : t.events)
    {
        fold_i32(h, e.year);
        fold_u8(h, e.kind);
        fold_u32(h, e.region);
        fold_u32(h, e.polity);
        fold_u32(h, e.other);
    }
    fold_u32(h, static_cast<uint32_t>(t.steps.size()));
    for (const timelapse_step& s : t.steps)
    {
        fold_i32(h, s.year);
        fold_i32(h, s.first_sample);
        fold_i32(h, s.sample_count);
    }
    fold_u32(h, static_cast<uint32_t>(t.samples.size()));
    for (const polity_sample& s : t.samples)
    {
        fold_i64(h, s.population);
        fold_u32(h, s.polity);
        fold_u32(h, s.regions);
        fold_u8(h, s.cap_military);
        fold_i64(h, s.industry_points);
        fold_i64(h, s.navy_stock);
        fold_i32(h, s.port_stock_q);
    }
    fold_strs(h, t.civilisation_name);
    fold_strs(h, t.creed_name);
    fold_ints(h, t.polity_creed);
    fold_strs(h, t.polity_name);
}

void fold_polity(uint64_t& h, const polity& p)
{
    fold_i32(h, p.id);
    fold_i32(h, p.culture);
    fold_i32(h, p.capital);
    fold_str(h, p.name);
    fold_i32(h, p.aggression_q);
    fold_i32(h, p.cohesion_q);
    fold_i64(h, p.industrial_year);
    fold_i32(h, p.protection_q);
    fold_bool(h, p.major);
    fold_i32(h, p.parent);
    fold_i32(h, p.universal_creed);
    fold_i64(h, p.creed_adopted_year);
    fold_bool(h, p.alive);
    fold_u64(h, p.empire_mask);
    fold_i32(h, p.empire_progress_q);
    fold_u64(h, p.exploration_mask);
    fold_i32(h, p.exploration_progress_q);
    fold_u64(h, p.industry_mask);
    fold_i32(h, p.industry_progress_q);
    fold_i32(h, p.overlord);
    fold_i32(h, p.subject_kind);
    fold_i64(h, p.navy_stock);
    fold_i64(h, p.naval_coastal_years); // BL-1147: the naval ledger crosses every handoff
    fold_i64(h, p.naval_crossings);
    fold_i64(h, p.naval_sea_techs);
    fold_i32(h, p.treaties_broken);
    fold_u32(h, p.tree_keys);
}

void fold_polities(uint64_t& h, const std::vector<polity>& ps)
{
    fold_u32(h, static_cast<uint32_t>(ps.size()));
    for (const polity& p : ps) fold_polity(h, p);
}

void fold_region(uint64_t& h, const region& r)
{
    fold_i32(h, r.anchor);
    fold_i32(h, r.col);
    fold_i32(h, r.row);
    fold_i32(h, static_cast<int32_t>(r.domain));
    for (int k = 0; k < culture_share_slots; ++k)
    {
        fold_i32(h, r.culture.id[k]);
        fold_i32(h, r.culture.weight_q[k]);
    }
    fold_i32(h, r.founding_culture);
    fold_str(h, r.name);
    fold_i32(h, r.farm_q);
    fold_i32(h, r.ore_q);
    fold_i32(h, r.energy_q);
    fold_i32(h, r.port_q);
    fold_i32(h, r.survey_fuel_q);
    fold_i32(h, r.survey_forest_q);
    fold_i64(h, r.founded_year);
    fold_i64(h, r.industrial_year);
    fold_i32(h, r.nation);
    fold_i32(h, r.contest_q);
    fold_i32(h, r.protection_q);
    fold_bool(h, r.has_market);
    fold_i64(h, r.material_stock);
    fold_i64(h, r.treasury);
    fold_i64(h, r.industry_points);
    fold_i32(h, r.port_stock_q);
    fold_i64(h, r.standing_army);
    fold_i32(h, r.standing_army_owner);
    fold_i32(h, r.civilisation);
    fold_i64(h, r.population);
    fold_i64(h, r.manpower_stock);
    fold_i64(h, r.army_stock);
    fold_i32(h, r.centres);
    fold_i64(h, r.urban_population);
    fold_i32(h, r.universal_creed);
    fold_u32(h, r.works_built);
    fold_i32(h, r.work_reach_mod);
}

void fold_regions(uint64_t& h, const std::vector<region>& rs)
{
    fold_u32(h, static_cast<uint32_t>(rs.size()));
    for (const region& r : rs) fold_region(h, r);
}

void fold_corridors(uint64_t& h, const std::vector<history_corridor>& cs)
{
    fold_u32(h, static_cast<uint32_t>(cs.size()));
    for (const history_corridor& c : cs)
    {
        fold_u32(h, c.a);
        fold_u32(h, c.b);
        fold_u32(h, c.tier);
        fold_u32(h, c.wet); // BL-1147 review: a corridor walked across sea
        fold_i64(h, static_cast<int64_t>(c.uses));
    }
}

void fold_grudges(uint64_t& h, const std::vector<grudge>& gs)
{
    fold_u32(h, static_cast<uint32_t>(gs.size()));
    for (const grudge& g : gs)
    {
        fold_u32(h, g.from);
        fold_u32(h, g.to);
        fold_i32(h, g.score);
        fold_i32(h, g.peak);
        fold_i32(h, g.event_count);
    }
}

// CONTENT, NOT SIZE (the K1/K2 cold review): every record the tail or a later
// span reads is folded field by field, so a copy that kept the count and lost
// the content cannot pass for a faithful one.

void fold_history(uint64_t& h, const std::vector<history_event>& es)
{
    fold_u32(h, static_cast<uint32_t>(es.size()));
    for (const history_event& e : es)
    {
        fold_i64(h, e.years_before_epoch);
        fold_i32(h, static_cast<int32_t>(e.stage));
        fold_str(h, e.event);
        fold_str(h, e.consequence);
        fold_i32(h, static_cast<int32_t>(e.rung));
    }
}

void fold_contacts(uint64_t& h, const std::vector<contact>& cs)
{
    fold_u32(h, static_cast<uint32_t>(cs.size()));
    for (const contact& c : cs)
    {
        fold_u32(h, c.from);
        fold_u32(h, c.to);
        fold_i32(h, c.first.year);
        fold_u32(h, c.first.region);
        fold_i32(h, static_cast<int32_t>(c.first.kind));
        fold_u32(h, c.first.across_water); // BL-1142: the class the meeting recorded
    }
}

void fold_sea_legs(uint64_t& h, const std::vector<sea_leg>& ls)
{
    fold_u32(h, static_cast<uint32_t>(ls.size()));
    for (const sea_leg& l : ls)
    {
        fold_u32(h, l.a);
        fold_u32(h, l.b);
        fold_i32(h, l.uses);
    }
}

/// BL-1152: a span's fleet ledger -- the counters and every stop, by content.
void fold_fleet(uint64_t& h, const fleet_ledger& f)
{
    fold_i64(h, f.read); fold_i64(h, f.stopped); fold_i64(h, f.stopped_by_partner);
    fold_i64(h, f.unlifted); fold_i64(h, f.clipped); fold_i64(h, f.men_ashore);
    fold_i64(h, f.no_leg); fold_i64(h, f.partners_abstained); fold_i64(h, f.seat_coast_fleets);
    fold_i64(h, f.exec_failed);
    fold_u32(h, static_cast<uint32_t>(f.stops.size()));
    for (const crossing_stop& c : f.stops)
    {
        fold_i32(h, c.year); fold_u32(h, c.attacker); fold_u32(h, c.realm); fold_u32(h, c.stopper);
        fold_u32(h, c.region); fold_u32(h, c.hub); fold_i64(h, c.attacker_power); fold_i64(h, c.defender_power);
        fold_i64(h, c.hub_army); fold_i64(h, c.realm_army); fold_i64(h, c.navy);
    }
}

void fold_dated(uint64_t& h, const std::vector<dated_object>& ds)
{
    fold_u32(h, static_cast<uint32_t>(ds.size()));
    for (const dated_object& d : ds)
    {
        fold_i64(h, d.expires_year);
        fold_i32(h, d.kind);
        fold_i32(h, d.a);
        fold_i32(h, d.b);
    }
}

void fold_civilisations(uint64_t& h, const std::vector<civilisation>& cs)
{
    fold_u32(h, static_cast<uint32_t>(cs.size()));
    for (const civilisation& c : cs)
    {
        fold_str(h, c.name);
        fold_ints(h, c.members);
        fold_i32(h, c.ethic.zeal);
        fold_i32(h, c.ethic.dominion);
        fold_i32(h, c.strain_q);
        fold_i64(h, c.formed_year);
    }
}

void fold_creeds(uint64_t& h, const std::vector<universal_creed>& cs)
{
    fold_u32(h, static_cast<uint32_t>(cs.size()));
    for (const universal_creed& c : cs)
    {
        fold_str(h, c.name);
        fold_i64(h, c.founded_year);
        fold_i32(h, c.origin_polity);
        fold_i32(h, c.origin_region);
    }
}

void fold_tongue(uint64_t& h, const tongue& t)
{
    fold_strs(h, t.onsets);
    fold_strs(h, t.vowels);
    fold_strs(h, t.codas);
}

void fold_cultures(uint64_t& h, const std::vector<culture>& cs)
{
    fold_u32(h, static_cast<uint32_t>(cs.size()));
    for (const culture& c : cs)
    {
        fold_i32(h, c.cradle);
        fold_str(h, c.name);
        fold_u32(h, static_cast<uint32_t>(c.pantheon.size()));
        fold_i32(h, c.aggression_q);
        fold_i32(h, c.sea_legs_q);
        fold_i32(h, c.parent);
        fold_i32(h, c.origin_farm_class);
        // BL-1107: the ground profile, by content.
        fold_i32(h, c.profile.farm);
        fold_i32(h, c.profile.ore);
        fold_i32(h, c.profile.energy);
        fold_i32(h, c.profile.water);
        fold_i32(h, c.profile.amenity);
        fold_i32(h, c.profile.amenity_share);
        fold_i64(h, c.coined_year);
        fold_i32(h, c.coined_from);
        fold_i32(h, c.folded_into);
        fold_tongue(h, c.speech);
    }
}

// --- the five digests ----------------------------------------------------------

uint64_t owner_changes_digest(const generation_report& rep)
{
    uint64_t h = 14695981039346656037ull;
    for (const generation_report::body_entry& be : rep.bodies)
    {
        fold_u32(h, be.id);
        fold_timelapse(h, be.migration_timelapse);
        fold_timelapse(h, be.prehistory_timelapse);
        fold_timelapse(h, be.exploration_timelapse);
        fold_timelapse(h, be.industrialisation_timelapse);
    }
    return h;
}

/// The fixture's three polity captures: 1200 (as Exploration opened on it),
/// 1660 and 1960. Comparable against the one call, which has no cursor.
uint64_t fixture_polities_digest(const era_minus_one_fixture& fx)
{
    uint64_t h = 14695981039346656037ull;
    fold_polities(h, fx.pre_exploration_polities);
    fold_polities(h, fx.exploration_handoff.polities);
    fold_polities(h, fx.industrialisation_handoff.polities);
    return h;
}

uint64_t polities_digest(const generation_cursor& c, const era_minus_one_fixture& fx)
{
    uint64_t h = fixture_polities_digest(fx);
    fold_polities(h, c.pass_one.polities);
    fold_polities(h, c.exploration.polities);
    return h;
}

/// The cursor's own STATE -- everything a stage hands the next that no world
/// digest can see (and the tile fields the deep digest does not fold, so the
/// Life gate's boundary is checked on its terrain, not only its deposits).
/// Folded by content wherever the tail or a later span reads the member. The
/// stage counter and the three run flags are NOT in it (`cursor_digest` adds
/// them): a digest led by `reached` differs at every boundary by construction,
/// so "every stage moves the cursor" read off it could never fail (the K1/K2
/// cold review's finding).
uint64_t cursor_state_digest(const generation_cursor& c)
{
    uint64_t h = 14695981039346656037ull;
    fold_f32(h, c.deposit_scalar);
    fold_str(h, c.naming.star);
    fold_strs(h, c.naming.bodies);
    fold_f32(h, c.rw.home_orbit_au);
    fold_u32(h, c.rw.attempts);
    fold_u32(h, c.kepler);
    fold_ints(h, c.kepler_tiles);
    fold_u32(h, static_cast<uint32_t>(c.kepler_pl.endemics.size()));
    for (const endemic_good& e : c.kepler_pl.endemics) fold_i32(h, static_cast<int32_t>(e.good));
    fold_history(h, c.kepler_pl.history);

    // The homeworld's tiles, in raster order: terrain, rivers, settled.
    for (const entity_id tid : c.kepler_tiles)
    {
        const auto it = c.w.tiles.find(tid);
        if (it == c.w.tiles.end()) { fold_u32(h, 0xFFFFFFFFu); continue; }
        const tile_component& t = it->second;
        fold_i32(h, static_cast<int32_t>(t.substrate));
        fold_i32(h, static_cast<int32_t>(t.cover));
        fold_i32(h, static_cast<int32_t>(t.landform));
        fold_u32(h, t.cover_density);
        fold_u32(h, t.river_edges);
        fold_u32(h, t.river_downstream);
        fold_u32(h, t.road_level);
        fold_f32(h, t.height);
        fold_f32(h, t.hazard_level);
        fold_f32(h, t.habitability);
        fold_f32(h, t.substrate_density);
        for (const float d : t.resource_deposit) fold_f32(h, d);
        fold_bool(h, c.w.tile_settled.count(tid) != 0);
    }

    // The ladder, the creeds and the settlement -- their histories by content:
    // the tail merges all three into the homeworld's biography.
    fold_history(h, c.kepler_hist.history);
    fold_cultures(h, c.kepler_creeds.cultures);
    fold_history(h, c.kepler_creeds.history);
    fold_regions(h, c.kepler_settlement.regions);
    fold_i64(h, c.kepler_settlement.migration_end_year);
    fold_cultures(h, c.kepler_settlement.spawned_cultures);
    fold_ints(h, c.kepler_settlement.settled_cells);
    fold_history(h, c.kepler_settlement.history);

    fold_ints(h, c.kepler_np.seed_tiles);
    fold_ints(h, c.kepler_np.seed_polities);
    fold_ints(h, c.kepler_np.polity_treasuries);
    fold_strs(h, c.kepler_np.polity_names);
    fold_u32(h, static_cast<uint32_t>(c.kepler_np.seed_tongues.size()));
    for (const tongue& t : c.kepler_np.seed_tongues) fold_tongue(h, t);
    fold_i32(h, c.kepler_np.min_seed_separation);
    fold_i32(h, c.kepler_np.land_tiles_per_seed);

    fold_i64(h, c.sim_start);
    fold_i64(h, c.culture_round_end_year);
    fold_timelapse(h, c.migration_lapse);

    fold_corridors(h, c.kepler_corridors);
    fold_sea_legs(h, c.kepler_sea_legs); // BL-1098: the lane stamp's record, carried to the tail
    fold_grudges(h, c.kepler_grudges);
    fold_i32(h, c.kepler_grudge_cap);
    fold_ints(h, c.kepler_polity_treasuries);
    fold_strs(h, c.kepler_polity_names);
    fold_ints(h, c.kepler_polity_industrial_years); // BL-1159
    fold_ints(h, c.kepler_region_polity);
    fold_ints(h, c.capital_market_shells);

    // The sim's terrain view, as the three spans read it.
    fold_u32(h, static_cast<uint32_t>(c.terrain.substrate.size()));
    for (std::size_t i = 0; i < c.terrain.substrate.size(); ++i)
    {
        fold_i32(h, static_cast<int32_t>(c.terrain.substrate[i]));
        fold_i32(h, static_cast<int32_t>(c.terrain.cover[i]));
        fold_u32(h, c.terrain.density[i]);
        fold_i32(h, static_cast<int32_t>(c.terrain.landform[i]));
        fold_u32(h, c.terrain.river[i]);
    }

    // The two resume structs, beyond their polities (folded under `polities`),
    // by content: every table the next span resumes from.
    fold_regions(h, c.pass_one.regions);
    fold_grudges(h, c.pass_one.grudges);
    fold_corridors(h, c.pass_one.surviving_corridors);
    fold_contacts(h, c.pass_one.contacts);
    fold_cultures(h, c.pass_one.cultures);
    fold_civilisations(h, c.pass_one.civilisations);
    fold_creeds(h, c.pass_one.universal_creeds);
    fold_timelapse(h, c.pass_one.timelapse);
    fold_regions(h, c.exploration.regions);
    fold_grudges(h, c.exploration.grudges);
    fold_corridors(h, c.exploration.surviving_corridors);
    fold_contacts(h, c.exploration.contacts);
    fold_sea_legs(h, c.exploration.sea_legs);
    fold_dated(h, c.exploration.dated_objects);
    fold_civilisations(h, c.exploration.civilisations);
    fold_creeds(h, c.exploration.universal_creeds);
    fold_fleet(h, c.exploration.fleet); // BL-1152: the 1660 ledger
    fold_fleet(h, c.fleet_1960);        // BL-1152: the 1960 ledger
    return h;
}

/// The whole cursor: its state, then where it stands and what has run.
uint64_t cursor_digest(const generation_cursor& c)
{
    uint64_t h = cursor_state_digest(c);
    fold_u8(h, static_cast<uint8_t>(c.reached));
    fold_bool(h, c.era_ran);
    fold_bool(h, c.exploration_ran);
    fold_bool(h, c.history_closed);
    fold_bool(h, c.migration_folded);
    return h;
}

uint64_t report_digest(const generation_report& rep)
{
    uint64_t h = 14695981039346656037ull;
    fold_u32(h, rep.attempts);
    fold_f32(h, rep.home_orbit_au);
    fold_u32(h, static_cast<uint32_t>(rep.bodies.size()));
    for (const generation_report::body_entry& be : rep.bodies)
    {
        fold_str(h, be.name);
        fold_u32(h, be.id);
        fold_bool(h, be.is_homeworld);
        fold_u32(h, static_cast<uint32_t>(be.state.history.size()));
        fold_bool(h, be.tiles.valid);
        fold_u32(h, be.tiles.seed);
        fold_f32(h, be.tiles.deposit_scalar);
        fold_i32(h, be.tiles.gw);
        fold_i32(h, be.tiles.gh);
        fold_regions(h, be.settlement.regions);
        fold_ints(h, be.nation_ids);
        fold_ints(h, be.nation_polity);
        fold_ints(h, be.nation_absorbed);
    }
    fold_strs(h, rep.stage_lines);
    for (const int64_t v : { rep.prehistory_years, rep.prehistory_battles, rep.prehistory_conquests,
                             rep.prehistory_foundings, rep.exploration_years, rep.exploration_battles,
                             rep.exploration_conquests, rep.exploration_foundings,
                             rep.industrialisation_years, rep.industrialisation_battles,
                             rep.industrialisation_conquests, rep.industrialisation_foundings,
                             rep.prehistory_corridors, rep.prehistory_junctions, rep.markets_from_trade })
        fold_i64(h, v);
    fold_i32(h, rep.grudge_sentiment_rows);
    fold_i32(h, rep.grudge_sentiment_dropped);
    // BL-1149 (the review fix): the span's scale credit, heads moved, inert rounds.
    fold_i64(h, rep.industrialisation_points_from_scale);
    fold_i64(h, rep.industrialisation_stream_moved);
    fold_i64(h, rep.industrialisation_stream_within);
    fold_i64(h, rep.industrialisation_scale_inert_rounds);
    fold_bool(h, rep.handoff_invalid);
    fold_str(h, rep.handoff_violation);
    return h;
}

struct boundary_digest
{
    uint64_t      world         = 0;
    world_metrics metrics;
    uint64_t      cursor        = 0; ///< State plus stage and run flags.
    uint64_t      cursor_state  = 0; ///< State alone (the non-vacuity reading).
    uint64_t      owner_changes = 0;
    uint64_t      polities      = 0;
    uint64_t      report        = 0;
};

boundary_digest digest_at(const generation_cursor& c, const generation_report& rep,
                          const era_minus_one_fixture& fx)
{
    boundary_digest d;
    d.world         = deep_digest(c.w, fx);
    d.metrics       = measure(c.w);
    d.cursor        = cursor_digest(c);
    d.cursor_state  = cursor_state_digest(c);
    d.owner_changes = owner_changes_digest(rep);
    d.polities      = polities_digest(c, fx);
    d.report        = report_digest(rep);
    return d;
}

/// THE ONE COMPARISON the proof and its negative controls both use, so a
/// control that makes this report a difference proves the proof would.
bool boundary_equal(const boundary_digest& a, const boundary_digest& b)
{
    return a.world == b.world && a.metrics == b.metrics && a.cursor == b.cursor
        && a.owner_changes == b.owner_changes && a.polities == b.polities
        && a.report == b.report;
}

/// One slot of the staged build: a cursor with its own report and fixture. The
/// copy is a COPY CONSTRUCTION of all three (aggregate init), then the cursor's
/// two sinks are rebound onto the copy's own -- exactly what a caller resuming
/// a copy must do (generation_cursor.hpp).
struct staged_slot
{
    generation_report     report;
    era_minus_one_fixture fixture;
    generation_cursor     cursor;
};

std::unique_ptr<staged_slot> copy_slot(const staged_slot& src)
{
    std::unique_ptr<staged_slot> s(new staged_slot{ src.report, src.fixture, src.cursor });
    s->cursor.report  = &s->report;
    s->cursor.fixture = &s->fixture;
    return s;
}

using clk = std::chrono::steady_clock;
double ms_since(clk::time_point t0)
{
    return std::chrono::duration<double, std::milli>(clk::now() - t0).count();
}

std::vector<uint32_t> library_seeds()
{
    std::ifstream in("docs/generation/seed_library.json");
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    std::vector<uint32_t> out;
    const std::regex rx("\"seed\"\\s*:\\s*([0-9]+)");
    for (std::sregex_iterator it(text.begin(), text.end(), rx), end; it != end; ++it)
        out.push_back(static_cast<uint32_t>(std::stoul((*it)[1].str())));
    return out;
}

std::vector<uint32_t> parse_seeds(const char* s)
{
    std::vector<uint32_t> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ','))
        if (!tok.empty()) out.push_back(static_cast<uint32_t>(std::stoul(tok)));
    return out;
}

// ---------------------------------------------------------------------------
// K2 — the staged proof, one seed.
// ---------------------------------------------------------------------------
void prove_seed(uint32_t seed, const world_gen_config& cfg, const works_registry& works,
                bool one_call)
{
    world_params params{};
    params.seed = seed;
    std::printf("\n--- seed %u ---\n", seed);
    std::fflush(stdout);

    // (a) the composition in place.
    const clk::time_point ta = clk::now();
    boundary_digest da[k_boundary_count];
    uint64_t        fx_polities_a = 0; ///< (a)'s fixture captures, for the one call
    {
        generation_report     rep;
        era_minus_one_fixture fx;
        generation_cursor c = begin_generation(params, &rep, cfg, nullptr, &works, &fx);
        for (int i = 0; i < k_boundary_count; ++i)
        {
            run_generation_to(c, k_boundaries[i]);
            da[i] = digest_at(c, rep, fx);
        }
        fx_polities_a = fixture_polities_digest(fx);
        std::printf("     (a) composition built, %.1f s\n", ms_since(ta) / 1000.0);
        std::fflush(stdout);
        check(!rep.handoff_invalid, "seed " + std::to_string(seed)
                                        + ": the composition's handoffs pass their validators"
                                        + (rep.handoff_invalid ? " -- " + rep.handoff_violation : ""));
    }
    const double ms_a = ms_since(ta);

    // (b) the staged build: every stage on a copy of its predecessor's slot,
    //     the predecessor freed before the stage runs.
    const clk::time_point tb = clk::now();
    boundary_digest db[k_boundary_count];
    // NEGATIVE CONTROLS (the K1/K2 cold review): one member of a COPIED cursor
    // is perturbed at a boundary, and the same comparison the proof uses must
    // report the difference -- a proof whose digests cannot see a member cannot
    // prove that member copied. Two members the digests once folded by size
    // alone: an Exploration sea leg (a span table the Industrialisation span
    // resumes from) and a settlement history line (the tail merges it into the
    // homeworld's biography). A member empty on this seed falls back to the
    // next table in the same struct, and says which it took.
    std::string control_span_what, control_hist_what;
    bool        control_span = false, control_hist = false;
    {
        std::unique_ptr<staged_slot> slot(new staged_slot{});
        slot->cursor = begin_generation(params, &slot->report, cfg, nullptr, &works, &slot->fixture);
        for (int i = 0; i < k_boundary_count; ++i)
        {
            if (i > 0)
            {
                std::unique_ptr<staged_slot> next = copy_slot(*slot);
                slot.reset();              // the predecessor is consumed
                slot = std::move(next);
            }
            run_generation_to(slot->cursor, k_boundaries[i]);
            db[i] = digest_at(slot->cursor, slot->report, slot->fixture);

            if (k_boundaries[i] == generation_stage::culture)
            {
                std::unique_ptr<staged_slot> bad = copy_slot(*slot);
                generation_cursor& c = bad->cursor;
                if (!c.kepler_settlement.history.empty())
                {
                    c.kepler_settlement.history.front().event += ".";
                    control_hist_what = "a settlement history line's text";
                }
                else if (!c.kepler_creeds.history.empty())
                {
                    c.kepler_creeds.history.front().event += ".";
                    control_hist_what = "a creed history line's text";
                }
                else
                {
                    c.kepler_hist.history.front().event += ".";
                    control_hist_what = "a ladder history line's text";
                }
                control_hist = !boundary_equal(da[i], digest_at(c, bad->report, bad->fixture));
            }
            if (k_boundaries[i] == generation_stage::exploration)
            {
                std::unique_ptr<staged_slot> bad = copy_slot(*slot);
                exploration_output& x = bad->cursor.exploration;
                if (!x.sea_legs.empty())
                {
                    x.sea_legs.front().uses += 1;
                    control_span_what = "an Exploration sea leg's uses";
                }
                else if (!x.contacts.empty())
                {
                    x.contacts.front().first.year += 1;
                    control_span_what = "an Exploration contact's year";
                }
                else if (!x.dated_objects.empty())
                {
                    x.dated_objects.front().expires_year += 1;
                    control_span_what = "an Exploration dated object's expiry";
                }
                control_span = !control_span_what.empty()
                            && !boundary_equal(da[i], digest_at(bad->cursor, bad->report, bad->fixture));
            }
        }
    }
    const double ms_b = ms_since(tb);

    bool all = true;
    for (int i = 0; i < k_boundary_count; ++i)
    {
        const boundary_digest& a = da[i];
        const boundary_digest& b = db[i];
        const bool w_ok = a.world == b.world && a.metrics == b.metrics;
        const bool o_ok = a.owner_changes == b.owner_changes;
        const bool p_ok = a.polities == b.polities;
        const bool c_ok = a.cursor == b.cursor;
        const bool r_ok = a.report == b.report;
        all = all && boundary_equal(a, b);
        std::printf("  %-18s world %016llX %s | owner_changes %016llX %s | polities %016llX %s"
                    " | cursor %016llX %s | report %016llX %s\n",
                    stage_name(k_boundaries[i]),
                    static_cast<unsigned long long>(a.world), w_ok ? "==" : "!=",
                    static_cast<unsigned long long>(a.owner_changes), o_ok ? "==" : "!=",
                    static_cast<unsigned long long>(a.polities), p_ok ? "==" : "!=",
                    static_cast<unsigned long long>(a.cursor), c_ok ? "==" : "!=",
                    static_cast<unsigned long long>(a.report), r_ok ? "==" : "!=");
    }
    check(all, "seed " + std::to_string(seed)
                   + ": every stage on a faithful copy of its predecessor == the composition, at "
                     "every boundary (world, owner_changes, polities, cursor, report)");

    // THE NEGATIVE CONTROLS, reported: each must make the comparison FAIL.
    check(control_hist, "seed " + std::to_string(seed) + ": negative control -- " + control_hist_what
                            + " perturbed in a copied culture cursor makes the comparison report FAIL");
    check(control_span, "seed " + std::to_string(seed) + ": negative control -- "
                            + (control_span_what.empty() ? std::string("(no Exploration table held a row)")
                                                         : control_span_what)
                            + " perturbed in a copied exploration cursor makes the comparison report FAIL");

    // Non-vacuity: every digest can see the stages that write what it folds.
    //   * owner_changes: every span boundary (the migration's record at
    //     culture, then one span record each);
    //   * cursor STATE: every stage -- the state digest, never the one led by
    //     the stage counter, which differs at every boundary by construction;
    //   * report: every stage after culture (culture's one report write is the
    //     migration record, which owner_changes folds);
    //   * world: the tail (the stages before it write tiles, the settled set and
    //     the fixture, which the cursor digest and the fixture folds carry).
    bool moved = true;
    for (int i = 1; i <= 4; ++i) // culture, empires, exploration, industrialisation
        moved = moved && da[i].owner_changes != da[i - 1].owner_changes;
    for (int i = 1; i < k_boundary_count; ++i)
        moved = moved && da[i].cursor_state != da[i - 1].cursor_state;
    for (int i = 2; i < k_boundary_count; ++i)
        moved = moved && da[i].report != da[i - 1].report;
    moved = moved && da[k_boundary_count - 1].world != da[k_boundary_count - 2].world;
    check(moved, "seed " + std::to_string(seed)
                     + ": the digests can see every stage (each span moves the ownership record, "
                       "each stage the cursor, each stage after culture the report, the tail the world)");

    std::printf("     composition %.1f s | staged %.1f s", ms_a / 1000.0, ms_b / 1000.0);

    // (c) the one call.
    if (one_call)
    {
        const clk::time_point tc = clk::now();
        generation_report     rep;
        era_minus_one_fixture fx;
        const world w = make_hard_coded_world(params, &rep, cfg, nullptr, &works, &fx);
        const double ms_c = ms_since(tc);
        std::printf(" | one call %.1f s\n", ms_c / 1000.0);
        const boundary_digest& t = da[k_boundary_count - 1];
        // The one call has no cursor to fold, so its polities are the fixture's
        // three captures, compared against (a)'s own fixture.
        const bool ok = deep_digest(w, fx) == t.world && measure(w) == t.metrics
                     && owner_changes_digest(rep) == t.owner_changes
                     && fixture_polities_digest(fx) == fx_polities_a
                     && report_digest(rep) == t.report;
        check(ok, "seed " + std::to_string(seed)
                      + ": make_hard_coded_world == the composition at the tail "
                        "(world, owner_changes, polities, report)");
    }
    else
    {
        std::printf("\n");
    }
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------
// K3 — copy against replay, one seed.
// ---------------------------------------------------------------------------
struct measure_row
{
    double copy_cursor_ms = 0; ///< generation_cursor copy (world + members), median of reps
    double copy_world_ms  = 0; ///< the world alone, median of reps
    double copy_report_ms = 0; ///< the report alone, median of reps
    double stage_ms       = 0; ///< this stage, on the walk
    double replay_gate_ms = 0; ///< copy the held gate cursor, run to here
    double replay_full_ms = 0; ///< from params, run to here
    std::size_t tiles     = 0;
    bool   replay_equal   = false;
};

double median(std::vector<double> v)
{
    std::sort(v.begin(), v.end());
    return v.empty() ? 0.0 : v[v.size() / 2];
}

void measure_seed(uint32_t seed, const world_gen_config& cfg, const works_registry& works, int reps)
{
    world_params params{};
    params.seed = seed;
    constexpr int n = 5; // life_gate .. industrialisation: the rerollable rounds' predecessors
    measure_row row[n];
    uint64_t    walk_world[n]  = {};
    uint64_t    walk_cursor[n] = {};

    // The walk, with a report (as the wizard's rounds build) and no fixture.
    generation_report rep;
    generation_cursor walk = begin_generation(params, &rep, cfg, nullptr, &works, nullptr);
    std::unique_ptr<generation_cursor> gate;       // the held Life-gate slot
    generation_report                  gate_report; // and the Life round's record
    const era_minus_one_fixture no_fx{};
    for (int i = 0; i < n; ++i)
    {
        const clk::time_point t0 = clk::now();
        run_generation_to(walk, k_boundaries[i]);
        row[i].stage_ms = ms_since(t0);
        row[i].tiles    = walk.w.tiles.size();
        walk_world[i]   = deep_digest(walk.w, no_fx);
        walk_cursor[i]  = cursor_digest(walk);

        std::vector<double> cc, cw, cr;
        for (int r = 0; r < reps; ++r)
        {
            clk::time_point t = clk::now();
            { const generation_cursor copy(walk); (void)copy; }
            cc.push_back(ms_since(t));
            t = clk::now();
            { const world copy(walk.w); (void)copy; }
            cw.push_back(ms_since(t));
            t = clk::now();
            { const generation_report copy(rep); (void)copy; }
            cr.push_back(ms_since(t));
        }
        row[i].copy_cursor_ms = median(cc);
        row[i].copy_world_ms  = median(cw);
        row[i].copy_report_ms = median(cr);
        if (i == 0)
        {
            gate.reset(new generation_cursor(walk));
            gate_report  = rep;
            gate->report = &gate_report;
        }
    }

    // Replays. From the held gate: a copy of slot 0 and its record (both inside
    // the timing, as a reroll would pay them), run to the boundary.
    for (int i = 0; i < n; ++i)
    {
        {
            const clk::time_point t0 = clk::now();
            generation_report r2(gate_report);
            generation_cursor c(*gate);
            c.report = &r2;
            run_generation_to(c, k_boundaries[i]);
            row[i].replay_gate_ms = ms_since(t0);
            row[i].replay_equal = deep_digest(c.w, no_fx) == walk_world[i]
                               && cursor_digest(c) == walk_cursor[i];
        }
        {
            generation_report r3;
            const clk::time_point t0 = clk::now();
            generation_cursor c = begin_generation(params, &r3, cfg, nullptr, &works, nullptr);
            run_generation_to(c, k_boundaries[i]);
            row[i].replay_full_ms = ms_since(t0);
            row[i].replay_equal = row[i].replay_equal && deep_digest(c.w, no_fx) == walk_world[i]
                               && cursor_digest(c) == walk_cursor[i];
        }
    }

    std::printf("\n--- K3 seed %u (median of %d copies; wall clock, loaded machine) ---\n", seed, reps);
    std::printf("  %-18s %9s %9s %9s %9s %12s %12s  %s\n", "boundary", "tiles", "stage ms",
                "copy ms", "world ms", "replay(gate)", "replay(full)", "replay == walk");
    bool eq = true;
    for (int i = 0; i < n; ++i)
    {
        std::printf("  %-18s %9zu %9.1f %9.1f %9.1f %12.1f %12.1f  %s\n",
                    stage_name(k_boundaries[i]), row[i].tiles, row[i].stage_ms,
                    row[i].copy_cursor_ms, row[i].copy_world_ms,
                    i == 0 ? row[i].copy_cursor_ms : row[i].replay_gate_ms, row[i].replay_full_ms,
                    row[i].replay_equal ? "yes" : "NO");
        eq = eq && row[i].replay_equal;
    }
    std::printf("  (report copy ms per boundary:");
    for (int i = 0; i < n; ++i) std::printf(" %.1f", row[i].copy_report_ms);
    std::printf(")\n");
    check(eq, "seed " + std::to_string(seed)
                  + ": a replay from the held gate and from the params lands on the walk's world "
                    "and cursor at every boundary (fixed span seeds)");
}

} // namespace

int main(int argc, char** argv)
{
    bool measure_mode = false;
    bool one_call     = true;
    int  reps         = 5;
    std::vector<uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--measure") == 0) measure_mode = true;
        else if (std::strcmp(argv[i], "--no-one-call") == 0) one_call = false;
        else if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc) seeds = parse_seeds(argv[++i]);
        else if (std::strcmp(argv[i], "--reps") == 0 && i + 1 < argc) reps = std::max(1, std::atoi(argv[++i]));
        else
        {
            std::fprintf(stderr, "usage: world_cursor_equivalence [--seeds a,b] [--no-one-call] "
                                 "| --measure [--seeds a,b] [--reps N]\n");
            return 2;
        }
    }
    if (seeds.empty())
        seeds = measure_mode ? std::vector<uint32_t>{ 0u, 28u } : library_seeds();
    if (seeds.empty())
    {
        std::fprintf(stderr, "world_cursor_equivalence: no seeds (run from the repo root, or pass --seeds)\n");
        return 2;
    }

    lua_state        lua;
    world_gen_config cfg;
    works_registry   works;
    load_app_generation_inputs(lua, cfg, works);
    std::printf("world_cursor_equivalence (BL-1084): %s on %zu seed(s):", measure_mode ? "K3 measure" : "K2 proof",
                seeds.size());
    for (const uint32_t s : seeds) std::printf(" %u", s);
    std::printf("\n  config %s, works %zu\n", cfg.is_fallback ? "FALLBACK" : "parsed world_gen.lua",
                static_cast<std::size_t>(works.size()));
    check(!cfg.is_fallback, "the app's parsed world_gen.lua config, not the C++ fallback");

    for (const uint32_t s : seeds)
    {
        if (measure_mode) measure_seed(s, cfg, works, reps);
        else              prove_seed(s, cfg, works, one_call);
    }

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "FAILURES", failures,
                failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
