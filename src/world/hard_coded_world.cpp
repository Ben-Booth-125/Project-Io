#include "hard_coded_world.hpp"

#include "province.hpp"

#include "body_names.hpp"
#include "city_names.hpp"
#include "colonisation.hpp"      // BL-871: colonisation_start_year, for the migration's own time-lapse
#include "continents.hpp"
#include "corporation_generation.hpp"
#include "creeds.hpp"
#include "era_minus_one.hpp"    // BL-462: the shared Era -1 invocation
#include "grudge_sentiment.hpp"  // BL-898: the Era -1 grudge record's one consumer
#include "history_ladder.hpp"
#include "history_sim.hpp"      // BL-271 wired into generation, 2026-08-12
#include "logistics.hpp"        // BL-910: body_tile_grid, for the close's capital markets
#include "sim_terrain_build.hpp" // build_sim_terrain for the sim's terrain view
#include "law.hpp" // BL-343: seed_prototype_laws
#include "nation_generation.hpp"
#include "orbital_system.hpp"
#include "population_generation.hpp"
#include "road_generation.hpp"
#include "settlement.hpp"
#include "tile_generation.hpp"
#include "river_generation.hpp"

#include <algorithm>
#include <array>
#include <cassert>      // BL-969: the handoff validators assert in debug builds
#include <chrono>       // BL-754: the generation budget, MEASURED not asserted
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <iterator>
#include <map>
#include <random>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

// Map an abundance tier to the deposit multiplier passed to generate_body_tiles.
// Earth-like `standard` is the ceiling (1.0×); leaner tiers step down (never up).
// See GENERATION_STRATEGY.md § The resource ceiling. Values authored in
// scripts/world_gen.lua (world_gen.deposit_scalar), BL-236.
float deposit_scalar_for(abundance_level a, const world_gen_config& cfg)
{
    switch (a)
    {
        case abundance_level::sparse:   return cfg.deposit_scalar[0];
        case abundance_level::lean:     return cfg.deposit_scalar[1];
        case abundance_level::standard: return cfg.deposit_scalar[2];
    }
    return cfg.deposit_scalar[2];
}

} // namespace

// THE MIGRATION'S OWN RECORD, declared in hard_coded_world.hpp: the wizard's
// Culture round lifts it off a finished report under --verify (BL-919), so it
// sits outside the anonymous namespace the rest of this file's helpers keep.
// ---------------------------------------------------------------------------
// The migration's own time-lapse (BL-871).
// ---------------------------------------------------------------------------
//
// `run_settlement` already dates every region it founds (`region::founded_year`)
// and reports when the flood itself finished (`settlement_state::
// migration_end_year`, BL-858 — "all habitable land has some culture"). Round
// 3 (Culture) wants a REPLAY of that walk, in the same `era_timelapse` shape
// round 4 already replays its own history through — one `owner_change` per
// founding, ascending by year, region-indexed exactly as `settlement.regions`
// is.
//
// THE "OWNER" IS A CULTURE, NOT A POLITY, and that is a deliberate departure
// from what `owner_change` names everywhere else it is produced
// (`history_sim.cpp`): there is no polity yet at founding time, only the
// people who reached the ground. `draw_lapse_map` colours by the index alone,
// so a culture id in this slot paints a map of PEOPLES rather than of REALMS —
// which is exactly what "who reached this ground first, and by which routes"
// (STARTUP.md § Round 3) is asking to see. The scoreboard's polity-keyed
// fields (`steps` / `samples` / `culture_changes`) are left empty: the
// migration has no living polities to rank, and an empty playback record is
// the documented "never ran" reading (era_timelapse.hpp), which is honest
// here — round 3 has no scoreboard content, not a broken one.
//
// PURE AND READ-ONLY: folds `settlement_state` as it stood the instant
// `run_settlement` returned, before the empire sim (or anything else) has
// touched it. No randomness, no clock, no write-back.
era_timelapse build_migration_timelapse(const settlement_state& ss, int64_t start_year,
                                        int64_t end_year)
{
    // Rebuild the roster's numbering from the settlement record alone: cradles
    // first (parentless, coined at the migration's start), then the daughters
    // in allocation order -- settlement.hpp says spawned ids run one past the
    // last cradle, so this is the same list `creed_state::cultures` ends up as.
    creed_state cs;
    std::size_t cradle_slots = 0;
    for (const auto& cy : ss.cradle_coined_year)
        if (cy.first >= 0) cradle_slots = std::max(cradle_slots, static_cast<std::size_t>(cy.first) + 1);
    cs.cultures.resize(cradle_slots); // placed BY ID, so a cradle slot is never a daughter slot
    for (const auto& cy : ss.cradle_coined_year)
    {
        if (cy.first < 0) continue;
        culture& cu    = cs.cultures[static_cast<std::size_t>(cy.first)];
        cu.parent      = -1;
        cu.coined_year = cy.second;
    }
    for (const culture& d : ss.spawned_cultures) cs.cultures.push_back(d);
    return build_migration_timelapse(ss, cs, start_year, end_year);
}

era_timelapse build_migration_timelapse(const settlement_state& ss, const creed_state& cs,
                                        int64_t start_year, int64_t end_year)
{
    era_timelapse t;
    t.start_year    = static_cast<int32_t>(start_year);
    t.years         = static_cast<int32_t>(std::max<int64_t>(0, end_year - start_year));
    t.region_stride = static_cast<int32_t>(ss.regions.size());

    // BL-916: the first region each people is the plurality of, by founding
    // year then index — where a daughter culture first shows on the map, and
    // the ground its `culture_split` event is pinned to.
    std::vector<int32_t> first_region(cs.cultures.size(), -1);

    t.changes.reserve(ss.regions.size());
    for (std::size_t i = 0; i < ss.regions.size(); ++i)
    {
        const region& r = ss.regions[i];
        const int plurality = r.culture.plurality();
        if (plurality < 0) continue; // Unpeopled ground never founded a region.
        t.changes.push_back(owner_change{
            static_cast<int32_t>(r.founded_year),
            static_cast<uint16_t>(i),
            static_cast<uint16_t>(plurality)});
        if (static_cast<std::size_t>(plurality) < first_region.size())
        {
            int32_t& fr = first_region[static_cast<std::size_t>(plurality)];
            if (fr < 0 || r.founded_year < ss.regions[static_cast<std::size_t>(fr)].founded_year)
                fr = static_cast<int32_t>(i);
        }
    }
    // ASCENDING BY YEAR, matching every other producer of this format
    // (`era_timelapse.hpp`'s "the replay substrate" contract) — `owner_slice_at`
    // walks it in order and stops at the first change past the query year.
    std::stable_sort(t.changes.begin(), t.changes.end(),
                     [](const owner_change& a, const owner_change& b) {
                         return a.year < b.year;
                     });

    // THE MIGRATION'S ONE EVENT KIND (BL-916): a people splitting from its
    // parent. `culture::parent` and `coined_year` are what `run_settlement`
    // materialised from the walk's spawn list, so this is the same fact the
    // creeds roster already carries, dated and placed. Cradles have no parent
    // and emit nothing. Ascending by coined year, ties by culture id — the
    // allocation order, so the list is stable across machines.
    //
    // THE LINEAGE, NOT THE LIVING TREE (BL-1017). A split is a fact about who
    // a people split FROM, so the event names `coined_from` — the link the
    // boundary fold never rewrites — and a split whose name later folded is
    // still emitted: the record sees every split that happened. A culture
    // with no recorded lineage (a hand-built fixture) falls back to `parent`.
    for (std::size_t c = 0; c < cs.cultures.size(); ++c)
    {
        const culture& cu = cs.cultures[c];
        const int from = cu.coined_from >= 0 ? cu.coined_from : cu.parent;
        if (from < 0 || cu.coined_year == INT64_MIN) continue;
        lapse_event e;
        e.year   = static_cast<int32_t>(cu.coined_year);
        e.kind   = static_cast<uint8_t>(lapse_event_kind::culture_split);
        e.region = first_region[c] >= 0 ? static_cast<uint16_t>(first_region[c])
                                        : lapse_event_none;
        e.polity = static_cast<uint16_t>(c);
        e.other  = static_cast<uint16_t>(from);
        t.events.push_back(e);
    }
    std::stable_sort(t.events.begin(), t.events.end(),
                     [](const lapse_event& a, const lapse_event& b) {
                         return a.year < b.year;
                     });
    return t;
}

namespace {

// ---------------------------------------------------------------------------
// Enclosed-sea measurement for the BL-276 acceptance gate. Returns the tile
// count of the LARGEST landlocked ocean component (any ocean component other
// than the biggest, which is the world ocean) — 0 if none. The gate reads it
// against two bars: the Mediterranean-scale arena (>= 300) and the small-sea
// floor (>= 30) under "a Rome-like start is never impossible" (Ben, 2026-08-03).
// ---------------------------------------------------------------------------
int largest_enclosed_sea(const world& w, const std::vector<entity_id>& tile_ids,
                         int gw, int gh)
{
    std::vector<char> ocean(tile_ids.size(), 0);
    for (std::size_t i = 0; i < tile_ids.size(); ++i)
        if (is_water(w.tiles.at(tile_ids[i]).substrate)) // BL-516: every water kind
            ocean[i] = 1;

    // Flood-fill ocean components on odd-r hex adjacency (columns wrap).
    std::vector<int> comp(ocean.size(), -1);
    std::vector<int> sizes;
    std::vector<int> stack;
    static const int even_d[6][2] = { {1,0},{-1,0},{0,-1},{-1,-1},{0,1},{-1,1} };
    static const int odd_d[6][2]  = { {1,0},{-1,0},{1,-1},{0,-1},{1,1},{0,1} };
    for (int i = 0; i < static_cast<int>(ocean.size()); ++i)
    {
        if (!ocean[static_cast<std::size_t>(i)] || comp[static_cast<std::size_t>(i)] != -1) continue;
        const int id = static_cast<int>(sizes.size());
        sizes.push_back(0);
        comp[static_cast<std::size_t>(i)] = id;
        stack.push_back(i);
        while (!stack.empty())
        {
            const int t = stack.back();
            stack.pop_back();
            ++sizes[static_cast<std::size_t>(id)];
            const int col = t % gw, row = t / gw;
            const auto& d = (row & 1) ? odd_d : even_d;
            for (int k = 0; k < 6; ++k)
            {
                const int nr = row + d[k][1];
                if (nr < 0 || nr >= gh) continue;
                const int nc = ((col + d[k][0]) % gw + gw) % gw;
                const int n = nr * gw + nc;
                if (ocean[static_cast<std::size_t>(n)] && comp[static_cast<std::size_t>(n)] == -1)
                {
                    comp[static_cast<std::size_t>(n)] = id;
                    stack.push_back(n);
                }
            }
        }
    }

    int main_id = -1, main_sz = 0;
    for (int i = 0; i < static_cast<int>(sizes.size()); ++i)
        if (sizes[static_cast<std::size_t>(i)] > main_sz) { main_sz = sizes[static_cast<std::size_t>(i)]; main_id = i; }
    int best = 0;
    for (int i = 0; i < static_cast<int>(sizes.size()); ++i)
        if (i != main_id) best = std::max(best, sizes[static_cast<std::size_t>(i)]);
    return best;
}

// ---------------------------------------------------------------------------
// The BL-276 acceptance gate, as one function. Reject-and-reroll over the
// homeworld tile seed, two bars — the hybrid Ben chose (2026-08-03: "about 90%
// likely ... never impossible to try"):
//
//   ARENA (>= 300 tiles): a playable Mediterranean. Three attempts only —
//     with the rift-basin mechanism (continents.cpp) putting a single seed
//     at ~58%, three tries land ~90% of worlds. Deliberately NOT retried to
//     exhaustion: the ~1-in-10 worlds without an arena are the wanted
//     hard-Rome tail.
//   FLOOR (>= 30 tiles): some enclosed sea, so trying is never impossible.
//     All six attempts may serve it; ~98% of seeds pass on attempt 0.
//
// Attempt 0 is the unfolded seed, so worlds already qualifying are untouched;
// on full exhaustion attempt 0 is kept honestly rather than clamping anything
// (the resolve_preferences idiom). Each probe is one scratch tile generation
// (~tens of ms). Extracted (2026-08-07) so the wizard's real-tile preview and
// make_hard_coded_world choose the SAME seed by calling the same code — a
// forked copy would drift and the preview would silently stop being the world.
uint32_t choose_home_tile_seed(const planetology_state& pl,
                               const std::vector<float>& bias,
                               const std::vector<uint8_t>& convergent,
                               uint32_t campaign_seed, float deposit_scalar)
{
    uint32_t chosen = campaign_seed ^ 0xE471001u;
    uint32_t floor_seed = 0;
    bool have_floor = false;
    for (int attempt = 0; attempt < 6; ++attempt)
    {
        const uint32_t candidate = (campaign_seed ^ 0xE471001u)
                                 ^ (static_cast<uint32_t>(attempt) * 0x9E3779B9u);
        world scratch;
        const entity_id probe = scratch.create_entity();
        const auto probe_tiles = generate_body_tiles(scratch, probe,
            home_grid_width, home_grid_height, pl.profile,
            candidate, deposit_scalar, &pl, nullptr, &bias, &convergent);
        const int sea = largest_enclosed_sea(scratch, probe_tiles,
                                             home_grid_width, home_grid_height);
        if (attempt < 3 && sea >= 300)
            return candidate;
        if (!have_floor && sea >= 30) { floor_seed = candidate; have_floor = true; }
        // Past the arena window with the floor already met: done searching.
        if (attempt >= 2 && have_floor) break;
    }
    if (have_floor) chosen = floor_seed;
    return chosen;
}

} // namespace

std::vector<entity_id> generate_home_surface_preview(world& w, entity_id body,
                                                     const world_params& params,
                                                     const world_gen_config& gen_cfg)
{
    // The same pipeline make_hard_coded_world runs for Kepler, and nothing else:
    // resolve, chain, Continents, the acceptance gate, the final generation.
    // Every seed formula below matches its twin in make_hard_coded_world by
    // construction (the gate IS the same function). Rivers and the political
    // layer are sibling passes the preview does not show, and are skipped.
    const float deposit_scalar = deposit_scalar_for(params.abundance, gen_cfg);
    const resolved_world rw = resolve_preferences(params.preferences, params.seed);

    body_inputs in = prototype_body(1);
    // The preview must describe the world the player will get, name included —
    // the chain writes the body's name into the biography prose it emits.
    const body_naming naming = generate_body_names(params.seed);
    in.name = naming.bodies[1].c_str();
    in.orbit_au = rw.home_orbit_au;
    const uint32_t body_seed = params.seed ^ prototype_body_seed(1);
    const planetology_state st = run_planetology(in, rw.params, body_seed);

    continent_state cs = run_continents(st, home_grid_width, home_grid_height,
                                        body_seed ^ 0xC0117E57u);

    const uint32_t tile_seed = choose_home_tile_seed(st, cs.height_bias, cs.convergent,
                                                     params.seed, deposit_scalar);
    generation_record record;
    std::vector<entity_id> tiles =
        generate_body_tiles(w, body, home_grid_width, home_grid_height,
                            st.profile, tile_seed, deposit_scalar, &st,
                            &record, &cs.height_bias, &cs.convergent, &cs);

    // RIVERS TOO (BL-915). The wizard's Culture and Empires maps draw the river
    // strokes under the political fill, and the only surface those rounds hold
    // is this one — so the river pass runs here with the SAME seed formula
    // make_hard_coded_world uses (`params.seed ^ 0x52490001u`), and the rivers
    // a frontier stalls at in the wizard are the rivers the campaign has. The
    // political layer stays skipped.
    generate_rivers(w, tiles, home_grid_width, home_grid_height,
                    record.height, /*seed=*/params.seed ^ 0x52490001u);
    return tiles;
}

world make_hard_coded_world(world_params params, generation_report* report,
                            const world_gen_config& gen_cfg,
                            generation_progress* progress,
                            const works_registry* works,
                            era_minus_one_fixture* fixture)
{
    world w;

    // --- The generation budget (BL-754) -------------------------------------
    //
    // Per-pass wall clock, REPORTED and never asserted. Ben's question for the
    // sprint is what the two-span era costs against the single-span one, and
    // that is not a number any check can own: it varies with the machine, the
    // build type and what else is running. So it is measured, handed to the
    // fixture (which has no save-seam presence — see era_minus_one.hpp) and
    // printed once; nothing reads it back.
    //
    // THESE CLOCKS TOUCH NO CONTROL FLOW AND NO DIGEST. A `steady_clock` read
    // inside `world/*` is only safe while it is write-only with respect to the
    // world, and every use below is: the values land in the fixture and in one
    // fprintf, and never in a branch, a seed, a hash or a stored field. The
    // standing determinism rule forbids timing that can VARY OUTPUT, not
    // measuring how long the output took.
    using gen_clock = std::chrono::steady_clock;
    const auto ms_between = [](gen_clock::time_point a, gen_clock::time_point b) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
    };
    const gen_clock::time_point t_world_begin = gen_clock::now();
    gen_clock::time_point t_settlement_begin = t_world_begin;
    gen_clock::time_point t_settlement_end   = t_world_begin;
    gen_clock::time_point t_era_end          = t_world_begin;

    // Coarse progress for a caller drawing a loading screen on another thread.
    // `bump` is the only writer and it only ever moves forward, so a reader
    // never sees the fraction go backwards.
    int gen_stage = 0;
    if (progress != nullptr)
        progress->stage_count.store(generation_stage_label_count, std::memory_order_relaxed);
    const auto bump = [&](int label_index) {
        if (progress == nullptr) return;
        progress->label.store(label_index, std::memory_order_relaxed);
        progress->stage.store(++gen_stage, std::memory_order_relaxed);
    };
    bump(0);

    // The resource-abundance multiplier every body's deposit pass is scaled by.
    // At the default `standard` tier this is 1.0f, so a default-params world is
    // bit-identical to the pre-BL-114 generation.
    const float deposit_scalar = deposit_scalar_for(params.abundance, gen_cfg);

    // The system's catalogue (BL-257). Coined from one tongue, before anything
    // else runs — the planetology chain writes body names into the biography
    // prose it emits, so a name decided later would leave that prose stale.
    // See body_names.hpp for the register rule.
    const body_naming naming = generate_body_names(params.seed);

    // ---------------------------------------------------------------------
    // Planetology (BL-167). A body-level sibling pass that runs BEFORE the
    // six-pass tile pipeline and DERIVES each body_profile that used to be a
    // hand-authored literal here. It also produces the per-resource endowment
    // the deposit pass multiplies by, and the readable history the generation
    // screen reveals. Architecture per BL-051: a sibling pass, not a change to
    // the six-pass core. See docs/generation/PLANETOLOGY.md.
    // ---------------------------------------------------------------------
    // Resolve the player's PREFERENCES into the parameters generation runs on.
    // This is where reject-and-reroll happens: the draw repeats, with the attempt
    // index folded in, until the homeworld clears the strict Earth-like floor. It
    // stays a pure function of (preferences, seed).
    const resolved_world rw = resolve_preferences(params.preferences, params.seed);

    if (report)
    {
        report->preferences   = params.preferences;
        report->params        = rw.params;
        report->home_orbit_au = rw.home_orbit_au;
        report->attempts      = rw.attempts;
        report->bodies.clear();
        report->stage_lines.clear();
    }
    // Index into the shared prototype body set (planetology.hpp). The wizard's
    // live preview walks the same list with the same seeds and the same derived
    // orbit, so the charts a player decided against describe exactly this world.
    // Continents/Drift (BL-210 first slice): a sibling pass reading Engine's
    // already-computed mobile_lid/theta, run BEFORE the tile pipeline. Its
    // history merges into the body's biography (chain_stage::engine lines);
    // its height bias feeds Pass 1 in place of pure noise. `plan` therefore
    // needs the body's grid dims and an out-param for the bias.
    // `convergent_out` is optional: only Kepler's Pass 5 currently reads it, and
    // the other bodies have no mountain-bearing terrain worth steering.
    // `body_id` is the world entity this plan describes. It is threaded in so the
    // report entry carries an IDENTITY (BL-257) rather than leaving consumers to
    // match a report entry to a world body by its display name.
    auto plan = [&](int proto_index, entity_id body_id, int gw, int gh,
                    std::vector<float>& bias_out,
                    std::vector<uint8_t>* convergent_out = nullptr,
                    continent_state* cs_out = nullptr) {
        body_inputs in = prototype_body(proto_index);
        // The generated name, not the prototype's placeholder literal — the
        // chain's biography lines quote it (BL-257). `naming` outlives `in`.
        in.name = naming.bodies[static_cast<std::size_t>(proto_index)].c_str();
        if (in.is_homeworld)
            in.orbit_au = rw.home_orbit_au;
        // A moon travels with its planet. The homeworld's orbit is DERIVED from
        // the star, so a moon left at its authored 1.00 AU would sit somewhere
        // else in the system entirely — computing its instellation, and now its
        // eclipse geometry, against a star it is not actually that far from.
        // The prototype set's only moon (Selene) orbits the homeworld.
        else if (in.parent_mass_earths > 0.0f)
            in.orbit_au = rw.home_orbit_au;
        const uint32_t body_seed = params.seed ^ prototype_body_seed(proto_index);
        planetology_state st = run_planetology(in, rw.params, body_seed);

        continent_state cs = run_continents(st, gw, gh, body_seed ^ 0xC0117E57u);
        st.history.insert(st.history.end(),
                          std::make_move_iterator(cs.history.begin()),
                          std::make_move_iterator(cs.history.end()));
        cs.history.clear(); // moved-from; the biography owns these lines now.
        std::stable_sort(st.history.begin(), st.history.end(),
            [](const history_event& a, const history_event& b)
            { return a.years_before_epoch > b.years_before_epoch; });
        bias_out = cs.height_bias;
        if (convergent_out) *convergent_out = cs.convergent;
        // BL-765: Pass 6's Life phase asks the PLATE SET where a tile sat when
        // its fossils formed, so the whole continents result has to reach
        // generate_body_tiles, not just the two per-tile masks derived from it.
        // Copied rather than referenced because `cs` is moved into the report
        // below when one is being written.
        if (cs_out) *cs_out = cs;

        if (report)
        {
            // The pre-drawdown twin, for the History ledger's formed-against-left
            // chart. Drawdown is the chain's last act and consumes no randomness,
            // so re-running with the dial at zero yields the same world minus its
            // industrial history rather than a different roll. Continents are not
            // re-run — the twin is read for its endowment only.
            planetology_params undrawn = rw.params;
            undrawn.drawdown = 0.0f;
            report->bodies.push_back(generation_report::body_entry{
                in.name, body_id, in.is_homeworld, st,
                run_planetology(in, undrawn, body_seed), std::move(cs) });
        }
        return st;
    };

    // Stamp the tile pass's own arguments onto the body's report entry (BL-303).
    // Deliberately recorded at the CALL SITE rather than derived later: the
    // homeworld's seed is chosen by the acceptance gate, and the convergent mask is
    // passed for the homeworld alone, so any attempt to reconstruct these from
    // world_params would be a second implementation waiting to disagree with this
    // one. The Generation Ledger replays the pass from exactly these.
    auto record_tile_inputs = [&](entity_id body_id, uint32_t tile_seed,
                                  int gw, int gh, bool used_convergent) {
        if (!report)
            return;
        for (generation_report::body_entry& be : report->bodies)
            if (be.id == body_id)
            {
                be.tiles = generation_report::body_entry::tile_inputs{
                    true, tile_seed, deposit_scalar, gw, gh, used_convergent };
                break;
            }
    };

    // Nation seeding is no longer parameterised here: generate_nations derives its own
    // seed budget from the body's habitable land area and merges on a minimum-viable
    // territory floor, so the nation count is an outcome rather than a target
    // (nation_generation.hpp; NATION_GENERATION.md § Pass 1).

    w.player_entity = w.create_entity();

    // -----------------------------------------------------------------------
    // System layout — a loose approximation of Sol.
    // -----------------------------------------------------------------------

    // Helios — the central star. A stationary body at the system centre with no
    // surface; its name titles the Solar minimap. Drawn through the same body
    // pass as every other body (with a star style), so it needs no special case.
    const entity_id helios = w.create_entity();
    w.bodies[helios] = body_component{
        .name                                 = naming.star,
        .type                                 = body_type::star,
        .parent                               = null_entity,
        .orbital_radius_au                    = 0.0f,
        .orbital_angle_rad                    = 0.0f,
        .orbital_angular_velocity_rad_per_day = 0.0f,
        .grid_width                           = 0,
        .grid_height                          = 0,
    };
    w.star_body = helios;

    // -----------------------------------------------------------------------
    // Cinder — hot inner planet (Mercury analogue, 0.39 AU)
    // 180×84 tile grid. Airless and scorching: no liquid water; volcanic and
    // barren surface, with rift zones and mountain ranges from high geology.
    // -----------------------------------------------------------------------

    const entity_id cinder = w.create_entity();
    w.bodies[cinder] = body_component{
        .name                                 = naming.bodies[0],
        .type                                 = body_type::planet,
        .parent                               = null_entity,
        .orbital_radius_au                    = 0.39f,
        .orbital_angle_rad                    = 0.40f,
        .orbital_angular_velocity_rad_per_day = kepler_angular_velocity(0.39f),
        .grid_width                           = 180,
        .grid_height                          = 84,
    };

    // Mercury-analogue physical facts; everything else is derived by the chain.
    std::vector<float> cinder_bias;
    continent_state cinder_cs;
    bump(1);
    const planetology_state cinder_pl = plan(0, cinder, 180, 84, cinder_bias, nullptr, &cinder_cs);
    const uint32_t cinder_tile_seed = params.seed ^ 0xC1D0001u;
    generate_body_tiles(w, cinder, 180, 84, cinder_pl.profile,
        cinder_tile_seed, deposit_scalar, &cinder_pl, nullptr, &cinder_bias, nullptr, &cinder_cs);
    record_tile_inputs(cinder, cinder_tile_seed, 180, 84, /*used_convergent=*/false);

    // -----------------------------------------------------------------------
    // Kepler — temperate home planet (Earth analogue, ~1.0 AU)
    // 261×121 tile grid (home_grid_width/height — this comment said 312×145 until 2026-08-21,
    // which BL-424 stopped being true when it took the homeworld to 70% area).
    // Full climate gradient, 60% ocean, grassland and forest
    // belts. Two installations and a market authored after tile generation.
    // -----------------------------------------------------------------------

    const entity_id kepler = w.create_entity();
    w.bodies[kepler] = body_component{
        .name                                 = naming.bodies[1],
        .type                                 = body_type::planet,
        .parent                               = null_entity,
        // The homeworld orbits where its star can keep it wet, so this is derived
        // rather than authored (a dimmer star pulls the homeworld inward).
        .orbital_radius_au                    = rw.home_orbit_au,
        .orbital_angle_rad                    = 1.05f,
        .orbital_angular_velocity_rad_per_day = kepler_angular_velocity(rw.home_orbit_au),
        // These were the literals 180 / 84 until 2026-08-12, which quietly made
        // `home_grid_width`/`home_grid_height` a LIE: the header calls itself
        // "one authority for the grid the build and the wizard preview both
        // assume", and this site — the one that actually builds the homeworld —
        // did not read it. Changing the constants moved the preview and left the
        // world alone. Now genuinely one authority.
        .grid_width                           = home_grid_width,
        .grid_height                          = home_grid_height,
        // Carried down so the physical tile scale is derived from the chain
        // (body_km_per_tile, logistics.hpp) rather than authored beside it.
        .mass_earths                          = rw.params.home_mass,
    };

    // Kepler is the corporation's home planet — the game opens on its surface.
    w.home_body = kepler;

    // The homeworld. Its inputs sit inside the corridor where the identical
    // unmodified chain succeeds — the guarantee is on the INPUTS, never on the
    // gates (PLANETOLOGY.md § The homeworld rule). Every gate still runs, still
    // records its margin, and still writes its line.
    std::vector<float> kepler_bias;
    std::vector<uint8_t> kepler_convergent; // Pass 5 seeds mountain ranges along these
    bump(2);
    continent_state kepler_cs;
    const planetology_state kepler_pl = plan(1, kepler, home_grid_width, home_grid_height,
                                             kepler_bias, &kepler_convergent, &kepler_cs);
    // A non-null generation_record is requested here so the river pass below can read the
    // Pass-1 heightmap it captures; this is a pure capture (TILE_GENERATION.md § Generation
    // history hook) and does not perturb the deterministic tile surface itself.
    generation_record kepler_record;

    // Reject-and-reroll acceptance gate (BL-276) — extracted to
    // choose_home_tile_seed above, SHARED with the wizard's real-tile preview
    // so both choose the same seed by running the same code.
    const uint32_t kepler_tile_seed = choose_home_tile_seed(
        kepler_pl, kepler_bias, kepler_convergent, params.seed, deposit_scalar);

    // TWO MORE HARDCODED 180/84 LIVED HERE UNTIL 2026-08-12 (this call and the
    // river pass below). They were the ones that actually mattered: setting
    // body_component.grid_width to 312x145 while this still generated 15,120
    // tiles produced an INTERNALLY INCONSISTENT world — a body claiming a grid
    // it did not have. home_surface_bench caught it as "preview composition !=
    // make_hard_coded_world Kepler, tile for tile", which is precisely the
    // "the preview silently stops being the world" failure the preview's own
    // header comment warns about.
    bump(4);
    auto kepler_tiles = generate_body_tiles(w, kepler, home_grid_width, home_grid_height,
        kepler_pl.profile,
        kepler_tile_seed, deposit_scalar, &kepler_pl, &kepler_record, &kepler_bias, &kepler_convergent,
        &kepler_cs);
    record_tile_inputs(kepler, kepler_tile_seed, 180, 84, /*used_convergent=*/true);

    // Rivers (BL-170) — sibling pass (BL-051 convention) over the same heightmap Pass 2
    // already thresholded; needs Kepler's ocean placement done, so it runs after
    // generate_body_tiles returns rather than being spliced into the six-pass core.
    bump(5);
    generate_rivers(w, kepler_tiles, home_grid_width, home_grid_height,
                    kepler_record.height, /*seed=*/params.seed ^ 0x52490001u);

    // Kepler is the only body with a political layer in the prototype; its nation
    // count is derived, not authored. Selene/Cinder/Pallas stay unclaimed.
    // See docs/generation/NATION_GENERATION.md.
    //
    // POPULATION CENTRES MOVED (BL-610, centres from demography): the centre
    // pass lived here, before the ladder, until 2026-08-25. It now runs INSIDE
    // the settlement block below — after the Era -1 sim, whose region
    // populations decide the centre count and scales — and still before
    // generate_nations, so Pass 6 (substrate density) reads the centres during
    // nation territory assignment exactly as it always has.

    // The nation count is a CONSEQUENCE of Kepler's geography, not a target: seeds
    // scale with its habitable land area and every nation below the minimum viable
    // territory is absorbed into its largest neighbour (nation_params defaults —
    // world/nation_generation.hpp). Only the separation is body-specific here: a
    // tighter spacing than the default over-seeds the map, so the merge pass has
    // real material to absorb and the layer reads as varied and "grown".
    // The pre-national history ladder (BL-221, docs/lore/HISTORY.md Stages 0-2).
    // Runs BEFORE generate_nations because it DRIVES it: Stage 0 counts the
    // independent agrarian cradles the land supported, Stage 2 prices conquest
    // against exit, and nation_params_from_ladder turns that into the seed
    // budget the Voronoi pass grows from. The ladder does not narrate a map it
    // was handed — it is upstream of the map (Ben, 2026-07-30).
    history_ladder_state kepler_hist =
        run_history_ladder(kepler_pl, w, kepler_tiles, home_grid_width, home_grid_height,
                           /*seed=*/params.seed ^ 0x5A11EDu);

    // Creeds (BL-235, docs/lore/CREEDS.md): one pantheon per cradle-culture,
    // each in its own generated tongue. Runs BEFORE generate_nations because
    // run_settlement (below) needs a pantheon and an aggression reading for
    // every cradle it may found from. Fragmentation no longer moves here
    // (BL-852) — the tribal marches retired; `record_cultural_contact` reads
    // the settled map's own culture shares once settlement has run, further
    // down this function.
    bump(6);
    creed_state kepler_creeds =
        run_creeds(kepler_pl, kepler_hist, w, kepler_tiles, home_grid_width, home_grid_height,
                   /*seed=*/params.seed ^ 0xC4EED5u);

    // Settlement & industrialisation (BL-218, docs/lore/HISTORY.md Stages 3-4).
    // Regions are placed BEFORE the political map and become its seeds, so
    // nation borders grow out of ground people settled rather than out of a
    // random draw — "seeding changes, expansion does not". Each region
    // inherits its nearest cradle's culture, so the pantheons the creeds pass
    // raised are now mapped onto specific ground and specific ancient deposits.
    settlement_state kepler_settlement;

    // BL-768 — THE ANCIENT ROAD RECORD, hoisted to this scope because the two
    // passes that consume it (the road stamp, and the market carve's trade-
    // concentration term) both run long after the block below has closed. The
    // sim's own `history_sim_state` stays local to that block: what crosses out
    // of it is the record, not the run.
    std::vector<history_corridor> kepler_corridors;

    // BL-898 — THE GRUDGE RECORD, hoisted for the same reason and on the same
    // terms as the corridors above: its consumer (`seed_grudge_sentiment`) runs
    // after the political map exists, which is long after the block below has
    // closed. The sim's `history_sim_state` still stays local to that block —
    // what crosses out is the record, not the run.
    //
    // WHY IT NEEDED HOISTING AT ALL. Before this item the record crossed the
    // pass 1 -> pass 2 handoff and died there: `grudge_between` had exactly one
    // caller in the tree (the handoff harness) and this file never read
    // `pass_one_output::grudges`. Carried is not consequential.
    std::vector<grudge> kepler_grudges;
    /// The sim's own `grudge_cap`, carried beside the record so the conversion
    /// is a fraction of the scale that actually produced these scores rather
    /// than of a struct default that might have moved.
    int32_t kepler_grudge_cap = grudge_sentiment_params{}.score_full;

    /// Indexed by region: which POLITY held it at the epoch. Snapshotted before
    /// `derive_national_character` overwrites `region::nation` with the nation
    /// index — the same field, read at the one moment it still names a polity.
    std::vector<int> kepler_region_polity;

    /// BL-975: indexed by POLITY id, the 1660 treasury each polity held —
    /// `region::treasury` summed over the regions flying its flag at the
    /// Exploration span's close. Empty when the span did not run, so a world
    /// without it credits nothing and every nation starts on the floor.
    std::vector<int64_t> kepler_polity_treasuries;

    nation_params kepler_np =
        nation_params_from_ladder(kepler_hist, nation_params{ .min_seed_separation = 5 });
    {
        // Aim the region budget at the political pass's own seed budget, so
        // the two agree by construction rather than by a tuned constant.
        int kepler_land = 0;
        for (const entity_id tid : kepler_tiles)
        {
            const auto it = w.tiles.find(tid);
            if (it != w.tiles.end() && !is_water(it->second.substrate))
                ++kepler_land;
        }
        const int budget = std::max(1, kepler_land / std::max(1, kepler_np.land_tiles_per_seed));

        bump(7);
        t_settlement_begin = gen_clock::now(); // BL-754
        // THE SIM'S OWN START YEAR, so settlement knows which foundings to hand
        // forward rather than place (BL-846). Derived from the same helper the
        // era invocation uses a hundred lines below — there is no second
        // construction here, only an earlier read of the same one.
        //
        // INT64_MAX WHERE THE ERA WILL NOT RUN, and that guard is load-bearing:
        // a schedule with no sim to play it is a set of regions that never get
        // founded at all. `prehistory_years == 0` is exactly how the harnesses
        // that do not test the era avoid paying for it, so this path is taken
        // often and must leave the map complete.
        const int64_t sim_start = era_minus_one_enabled(params)
                                      ? era_minus_one_sim_params(params).start_year
                                      : INT64_MAX;

        kepler_settlement = run_settlement(kepler_pl, kepler_hist, kepler_creeds, w,
                                           kepler_tiles, home_grid_width, home_grid_height, budget,
                                           /*seed=*/params.seed ^ 0x5E77EDu,
                                           /*stop_year=*/params.epoch_year,
                                           /*sim_start_year=*/sim_start);
        t_settlement_end = gen_clock::now(); // BL-754

        // THE SETTLED CELLS BECOME A HARD INPUT TO THE PROVINCE PARTITION
        // (BL-849; docs/generation/PROVINCES.md § The settled cells are a
        // binding input). `kepler_settlement.settled_cells` is raster-order,
        // exactly the order `kepler_tiles` holds this body's tile ids in, so the
        // two zip directly — same convention `col_sub`/`col_cov`/`col_river`
        // already use inside `run_settlement` itself.
        //
        // WRITTEN HERE, LONG BEFORE `build_province_partition` RUNS, because
        // this is the one place both the raster and the entity ids are in
        // scope together; the partition itself only ever reads `w.tile_settled`
        // as a plain per-tile lookup, the same shape `tile_to_nation` already
        // is for the national assignment.
        for (std::size_t ci = 0;
            ci < kepler_settlement.settled_cells.size() && ci < kepler_tiles.size(); ++ci)
            if (kepler_settlement.settled_cells[ci] != 0 && kepler_tiles[ci] != null_entity)
                w.tile_settled.insert(kepler_tiles[ci]);

        // THE CULTURES THE MIGRATION COINED JOIN THE ROSTER (BL-856). Appended
        // rather than kept in a second list, so every downstream consumer -- the
        // sim's per-culture aggression read, the naming passes, the shares in
        // `region::culture` -- sees ONE flat vector and needs no second lookup
        // and no id remapping. The walk allocated their ids as
        // `cs.cultures.size() + n`, which is exactly where they land here.
        //
        // THEY ARRIVE ALREADY FOLDED (BL-1017): `run_settlement` closed the
        // round by folding every daughter that holds no ground into its
        // nearest living ancestor, so from this line on `culture::parent` is
        // the living tree every reader walks, and no id moves.
        kepler_creeds.cultures.insert(kepler_creeds.cultures.end(),
                                      kepler_settlement.spawned_cultures.begin(),
                                      kepler_settlement.spawned_cultures.end());

        // The cradle cultures' own country (BL-865). run_settlement could not
        // write these itself -- it holds the creeds by const reference -- so it
        // reports them and they are copied back here, beside the daughters that
        // already carry theirs.
        for (const auto& [cid, cls] : kepler_settlement.cradle_origin_class)
            if (cid >= 0 && cid < static_cast<int>(kepler_creeds.cultures.size()))
                kepler_creeds.cultures[static_cast<std::size_t>(cid)].origin_farm_class = cls;

        // The cradle cultures' own coining year (BL-870), the same round-trip
        // for the same reason, one line above.
        for (const auto& [cid, year] : kepler_settlement.cradle_coined_year)
            if (cid >= 0 && cid < static_cast<int>(kepler_creeds.cultures.size()))
                kepler_creeds.cultures[static_cast<std::size_t>(cid)].coined_year = year;

        // Fragmentation from contact (BL-852, resolving NR-808;
        // docs/generation/COLONISATION.md § Fragmentation comes from
        // contact). The tribal marches retired: `record_cultural_contact`
        // reads how far two peoples' settlements interpenetrate off the
        // founding map itself — no pantheon comparison, no roll, no war.
        //
        // For each settled region, its NEAREST OTHER region (grid distance,
        // columns wrapping, lowest index breaks a tie — the same rule the
        // retired marches used for their own nearest-cradle search) tells
        // whether a different founding culture sits close enough to count as
        // contact; averaged over the whole settled map this is exactly the
        // breadth of frontier the doc asks for, with `founding_culture`
        // (`region::founding_culture`, never overwritten by a later
        // conquest) standing in for `culture_shares`'s plurality so this
        // reads the map colonisation actually drew rather than anything the
        // Era -1 sim has since fought over.
        {
            std::vector<int> region_mix_q;
            region_mix_q.reserve(kepler_settlement.regions.size());
            for (std::size_t i = 0; i < kepler_settlement.regions.size(); ++i)
            {
                const region& ri = kepler_settlement.regions[i];
                if (ri.anchor < 0 || ri.founding_culture < 0) continue;

                int best = -1, best_d = 1 << 30;
                for (std::size_t j = 0; j < kepler_settlement.regions.size(); ++j)
                {
                    if (j == i) continue;
                    const region& rj = kepler_settlement.regions[j];
                    if (rj.anchor < 0) continue;
                    const int dc = std::abs(ri.col - rj.col);
                    const int dr = std::abs(ri.row - rj.row);
                    const int d  = std::min(dc, home_grid_width - dc) + dr;
                    if (d < best_d) { best_d = d; best = static_cast<int>(j); }
                }
                if (best < 0) continue;
                region_mix_q.push_back(
                    kepler_settlement.regions[static_cast<std::size_t>(best)].founding_culture
                            != ri.founding_culture ? 1000 : 0);
            }
            record_cultural_contact(kepler_hist, region_mix_q);

            // The seed budget nation_params_from_ladder derives depends on
            // fragmentation_q, which just moved. Re-derive it over the
            // CONTACT reading so generate_nations (further down) inherits
            // the measured value; `land_tiles_per_seed` already did its one
            // job sizing the region budget above and is moot from here —
            // `seed_tiles`/`seed_polities` below override it from the
            // concrete settled map regardless.
            kepler_np = nation_params_from_ladder(
                kepler_hist, nation_params{ .min_seed_separation = 5 });
        }

        // THE POPULATION MAP, DRAWN EARLY (BL-766). Before the Era -1 sim, not
        // after it: every region whose ground farms easily is given an opening
        // urban headcount and the centres those heads stand up, so the sim runs
        // over a world that has cities in it and can grow, sack and raze them.
        //
        // This deliberately overturns BL-610's ORDERING while keeping its goal.
        // Centre count and scale are still the history's consequence — they are
        // carved from `region::centres` / `region::urban_population`, which only
        // this sim moves — but now because history grew and sacked the cities
        // rather than because they were placed after it had finished.
        //
        // Pure and seedless: a deterministic consequence of `farm_q`, which is
        // the shape the generation layer asks new stages to take.
        draw_urban_map(kepler_settlement);

        // THE MIGRATION HAS RUN (BL-871). A caller that only wants the Culture
        // round's own record — the wizard's round 3 — stops here, before the
        // Empires round's history sim has run at all. This is what makes round
        // 3's span the migration's OWN span (`colonisation_start_year` to
        // `settlement_state::migration_end_year`, a derived year) rather than
        // the fused span both rounds used to share: nothing below this point
        // has executed, so there is no empire history for round 3 to leak.
        //
        // Same pattern as `stop_after_ancient_era` below: the report is
        // finished before returning, because a caller wants
        // `generation_report` and must discard the half-built `world`.
        if (gen_cfg.stop_after_migration)
        {
            // THE COAST TO 400 BCE (BL-947; docs/generation/CIVILISATION.md §
            // "The span is 400 BCE to 1200 CE" — "the Culture round ends when
            // the filling ends, and the world then coasts to 400 BCE holding
            // what migration left it"). `migration_end_year` is DERIVED per
            // seed (the diffusion frontier's own exhaustion year) and varies
            // seed to seed; round 3's DISPLAYED span always closes at the
            // Empires round's own opening year instead — `sim_start` above,
            // already the exact year `run_settlement` was told to stop
            // founding new regions at (BL-846) and the year
            // `era_minus_one_sim_params` opens the Empires sim on. Reusing
            // that value rather than a bare -400 literal means round 3's
            // coast target and round 4's own opening year can never drift
            // apart from each other.
            //
            // NEVER CLAMPED EARLIER than the true migration end (`std::max`):
            // a migration that overruns the boundary is measured (BL-947
            // sweep: 6/60 seeds, up to 343 years over on the tested set) and
            // is "a defect in the migration, not in this boundary" per the
            // same doc section, so the true (later) year is kept and shown
            // rather than cut short or hidden.
            //
            // Only applied when the era sim is actually configured to run
            // (`sim_start` is a real year, not the INT64_MAX sentinel a
            // `prehistory_years == 0` caller gets) — a caller with no
            // Empires round has no coast target to hold the map still until,
            // and keeps seeing the migration's own raw end exactly as before.
            const int64_t culture_round_end_year =
                sim_start != INT64_MAX
                    ? std::max(kepler_settlement.migration_end_year, sim_start)
                    : kepler_settlement.migration_end_year;

            const era_timelapse migration_lapse =
                build_migration_timelapse(kepler_settlement, kepler_creeds,
                                          colonisation_start_year,
                                          culture_round_end_year);

            // BL-914: round 3 gets the same tap round 4 does. `run_settlement`
            // itself is not instrumented (out of this item's files), so this is
            // ONE publish of the finished migration record rather than a
            // growing one — but it still lands here, on the worker thread,
            // before `make_hard_coded_world` returns and long before the
            // std::async future resolves on the app side. So the renderer's
            // playhead can start moving as soon as this publish happens rather
            // than waiting for the future AND the report-to-lapse conversion
            // both to finish, which is the whole gap this item closes.
            if (progress != nullptr && progress->lapse_tap != nullptr)
            {
                // Geometry alongside ownership, same reasoning as round 4's
                // per-founding publish (history_sim.cpp): the renderer cannot
                // draw a single tile without a region position.
                std::vector<int32_t>     tap_region_col;
                std::vector<int32_t>     tap_region_row;
                std::vector<std::string> tap_region_name;
                tap_region_col.reserve(kepler_settlement.regions.size());
                tap_region_row.reserve(kepler_settlement.regions.size());
                tap_region_name.reserve(kepler_settlement.regions.size());
                for (const region& r : kepler_settlement.regions)
                {
                    tap_region_col.push_back(r.col);
                    tap_region_row.push_back(r.row);
                    tap_region_name.push_back(r.name);
                }
                progress->lapse_tap->publish_regions(tap_region_col, tap_region_row,
                                                     tap_region_name);
                progress->lapse_tap->publish(
                    migration_lapse.changes, migration_lapse.culture_changes,
                    migration_lapse.events,
                    static_cast<int32_t>(culture_round_end_year));
            }

            if (report)
            {
                report->prehistory_years     = culture_round_end_year
                                              - colonisation_start_year;
                report->prehistory_battles   = 0;   // The migration is a diffusion, not a
                report->prehistory_conquests = 0;   // contest — COLONISATION.md owns why
                report->prehistory_foundings = static_cast<int64_t>(kepler_settlement.regions.size());
                for (generation_report::body_entry& be : report->bodies)
                    if (be.id == kepler)
                    {
                        be.settlement           = kepler_settlement;
                        be.prehistory_timelapse = migration_lapse;
                        break;
                    }
            }
            return w;
        }

        // ------------------------------------------------------------------
        // The year-tick sim, wired into generation (Ben, 2026-08-12).
        //
        // run_settlement founds regions and stops. Below the industrial era
        // it deliberately leaves the rest to "the year-tick sim", and that sim
        // HAS EXISTED SINCE BL-271 WITHOUT EVER BEING CALLED HERE — its only
        // caller was the tile inspector. So the campaign opened onto a world
        // that had been settled and then stood perfectly still: no wars, no
        // borders that had ever moved, nothing to inherit.
        //
        // Ben's ask, and the acceptance test for this block: "there is turmoil
        // at the beginning of the campaign... some losing / winning bodies, and
        // the game FEELS alive right from the first tick."
        //
        // 400 years at 4 years a tick — his figure. 100 decision rounds, ~110 ms
        // measured, against a generation budget of one minute that the whole
        // chain currently spends 2.4 s of.
        bump(8);
        // THE GATE AND THE PARAMS ARE DERIVED, NOT AUTHORED HERE (BL-462).
        //
        // Every line of this invocation used to be written out at this one site,
        // where no other caller could read it — so every Era -1 harness took
        // `history_sim_params`'s struct default instead (4000 BCE -> 0 CE on six
        // bands, 136 rounds, against generation's 400 years on one 4-year band,
        // 100 rounds) and no check in the project measured the run that actually
        // generates a world. The derivations now live in era_minus_one.hpp and
        // the harnesses call the same ones. This block's BEHAVIOUR is unchanged:
        // the helpers reproduce it line for line.
        //
        // The settlement clause stays here because it is not a question about
        // params — `era_minus_one_fixture::ran` is the answer that includes it.
        if (era_minus_one_enabled(params) && !kepler_settlement.regions.empty())
        {
            const sim_terrain_arrays terr =
                build_sim_terrain(w, kepler, home_grid_width, home_grid_height);

            const history_sim_params hp    = era_minus_one_sim_params(params);
            const uint32_t           hseed = era_minus_one_sim_seed(params);

            // THE CAPTURE (BL-462). Everything a re-run needs that cannot be
            // re-derived from the report: the settlement BEFORE the sim mutates
            // it in place, the creeds the polity aggressions are read off, the
            // terrain, and the works pointer. Taken here, at the one call site,
            // so a harness constructs nothing of its own and has nothing left to
            // drift on. Costs nothing when no fixture was asked for.
            if (fixture != nullptr)
            {
                fixture->ran        = true;
                fixture->body       = kepler;
                fixture->gw         = home_grid_width;
                fixture->gh         = home_grid_height;
                fixture->settlement = kepler_settlement;
                fixture->creeds     = kepler_creeds;
                fixture->terrain    = terr;
                fixture->params     = hp;
                fixture->seed       = hseed;
                fixture->works      = works;
            }

            // The one pass long enough to earn the loading screen's inner bar
            // (~23 s of the ~25 s total): announce the year span, hand the sim
            // the year counter, and clear the announcement when the pass ends.
            if (progress != nullptr)
            {
                progress->sub_progress.store(0, std::memory_order_relaxed);
                progress->sub_total.store(
                    static_cast<int>(hp.stop_year - hp.start_year),
                    std::memory_order_relaxed);
            }

            const history_sim_state hs =
                run_history_sim(kepler_settlement, &kepler_creeds, terr.view(),
                                home_grid_width, home_grid_height, hp,
                                hseed,
                                progress != nullptr ? &progress->sub_progress
                                                    : nullptr,
                                works, // BL-321: the Era -1 works table, or null.
                                progress != nullptr ? progress->lapse_tap
                                                    : nullptr); // BL-914: null off the wizard's path.

            if (progress != nullptr)
                progress->sub_total.store(0, std::memory_order_relaxed);

            // BL-911: fold into the pass 1 -> pass 2 handoff HERE, while `hs`
            // and the now-final `kepler_settlement` ownership are both still
            // live, so the stamp pass below reads the crossed record rather
            // than reaching back into live sim state. `kepler_settlement` is
            // the ownership read `make_pass_one_output` wants — `run_history_sim`
            // took it by reference and wrote every ownership change into it in
            // place, so by this line it already holds the map at the epoch.
            const pass_one_output kepler_pass_one = make_pass_one_output(
                kepler_settlement, hs, &kepler_creeds);

            // BL-969: THE VALIDATOR RUNS HERE, NOT ONLY IN A HARNESS. Until
            // this line both validators were thorough and called from two
            // harnesses only, so the shipped path folded the struct and
            // trusted it -- GENERATION_STRATEGY.md § What crosses each handoff
            // says "a validator checks them rather than a reader trusting the
            // sentence", and a check that runs off the shipped path is the
            // sentence. Against `kepler_creeds`, the live table the fold read,
            // so the copy in the struct and the table consumers still read
            // are proven to agree at the fold.
            //
            // A violation is RECORDED, never repaired: onto the report so a
            // harness can assert it stays false; one line to stderr so a
            // release build is not silent; an assert so a debug build stops
            // on it. Generation continues on the value as folded -- there is
            // no "fixed" value to substitute, and inventing one here would be
            // exactly the clamp this layer refuses.
            const auto record_handoff_violation = [&](const char* which, const std::string& why) {
                const std::string msg = std::string(which) + ": " + why;
                if (report != nullptr)
                {
                    report->handoff_invalid = true;
                    if (!report->handoff_violation.empty()) report->handoff_violation += "; ";
                    report->handoff_violation += msg;
                }
                std::fprintf(stderr, "make_hard_coded_world: handoff validator failed -- %s\n",
                             msg.c_str());
                assert(false && "a handoff validator failed on the shipped path (BL-969)");
            };
            {
                std::string why;
                if (!pass_one_output_valid(kepler_pass_one, &why, &kepler_creeds))
                    record_handoff_violation("pass_one_output", why);
            }

            // BL-768/BL-911: the corridors the history walked, filtered to the
            // ones a surviving polity still holds an end of (§ The network is
            // the estate, and it crosses) — never the raw `hs.supply_corridors`
            // record, which carries corridors held by realms that fell too.
            kepler_corridors = kepler_pass_one.surviving_corridors;

            // BL-898: the directed grudge table, out of the block with the
            // corridors. READ FROM THE STRUCT (BL-969), not `hs.grudges`: the
            // struct is the whole of what crosses, and a consumer that reached
            // past it into the live sim state was the drift the validator
            // above exists to catch. Sparse, so the copy is a few dozen rows.
            kepler_grudges    = kepler_pass_one.grudges;
            kepler_grudge_cap = static_cast<int32_t>(hp.grudge_cap);

            // The sim narrates through the same history_event shape the other
            // generation passes use, so its wars join the world log without a
            // new case anywhere.
            //
            // A DELIBERATE `hs.` READ PAST THE FOLD (BL-969): the narrative
            // log is not on the doc's list of what crosses the handoff -- it
            // is the biography the ledgers print, joined to the settlement
            // record here for presentation, and nothing downstream computes
            // from it. Carrying it in the struct would widen the contract to
            // a table no consumer reads as data.
            kepler_settlement.history.insert(kepler_settlement.history.end(),
                                             hs.history.begin(), hs.history.end());

            // ------------------------------------------------------------
            // BL-931/BL-946 — THE EXPLORATION SPAN, 1200 -> exploration_stop_year,
            // on the SAME engine, immediately after the Empires round
            // closes above. Gated on `exploration_sim_enabled(params)`
            // (default TRUE since BL-946) so a caller can still opt out, and
            // additionally skipped whenever this call is the wizard's OWN
            // Empires-round launch (`gen_cfg.stop_after_ancient_era`) — that
            // round wants only the Empires history and must not pay for a
            // span it discards a few lines below.
            //
            // BL-956: WHEN THE SPAN RUNS, IT HANDS FORWARD ITS OWN VALUE.
            // The span is folded into `exploration_output` right after it
            // closes, and `kepler_corridors` / `kepler_grudges` above are
            // REPLACED by that value's 1660 grudges and surviving network
            // (filtered over THIS span's dead) — EXPLORATION.md § What this
            // phase hands digitisation: "A campaign that opens on the 1660
            // political map must not open on 1200's resentments and 1200's
            // roads." When the span does not run, the Empires values stand.
            if (exploration_sim_enabled(params) && !kepler_pass_one.polities.empty()
                && !gen_cfg.stop_after_ancient_era)
            {
                history_sim_params ep = exploration_sim_params(params);
                ep.resume_polities  = &kepler_pass_one.polities;
                ep.resume_grudges   = &kepler_pass_one.grudges;
                ep.resume_contacts  = &kepler_pass_one.contacts;
                ep.resume_corridors = &kepler_pass_one.surviving_corridors;

                // BL-937: the PRE-sim capture, taken before this call mutates
                // `kepler_settlement`/`kepler_creeds` in place — see the field
                // comments on `era_minus_one_fixture::pre_exploration_*` for
                // why a harness needs these rather than the post-sim state.
                if (fixture != nullptr)
                {
                    fixture->pre_exploration_settlement = kepler_settlement;
                    fixture->pre_exploration_creeds     = kepler_creeds;
                    fixture->pre_exploration_polities   = kepler_pass_one.polities;
                    fixture->pre_exploration_grudges    = kepler_pass_one.grudges;
                    fixture->pre_exploration_corridors  = kepler_pass_one.surviving_corridors;
                }

                // BL-946: the loading screen's sub-bar AND the live lapse tap,
                // same `progress` pointer the Empires call above already
                // reads — so the wizard's new Exploration round gets the same
                // "wait is the round" live map the Empires round has, rather
                // than a silent hang followed by a populated map on landing.
                //
                // AND IT SAYS WHICH SPAN IT IS RUNNING (Ben, 2026-09-16). The
                // label was last set to "Running the ancient era" at stage 8
                // and never moved, so this pass counted ITS OWN 460 years
                // under the previous pass's name — "year 397 of 460" while the
                // line above said the ancient era, which is 1,600 years long.
                // A counter and a caption that describe different spans are
                // worse than either alone.
                if (progress != nullptr)
                {
                    progress->label.store(13, std::memory_order_relaxed); // the exploration age
                    progress->sub_progress.store(0, std::memory_order_relaxed);
                    progress->sub_total.store(
                        static_cast<int>(ep.stop_year - ep.start_year),
                        std::memory_order_relaxed);
                }

                const history_sim_state kepler_exploration_hs = run_history_sim(
                    kepler_settlement, &kepler_creeds, terr.view(),
                    home_grid_width, home_grid_height, ep,
                    exploration_sim_seed(params),
                    progress != nullptr ? &progress->sub_progress : nullptr,
                    works,
                    progress != nullptr ? progress->lapse_tap : nullptr); // BL-946: wired.

                if (progress != nullptr)
                    progress->sub_total.store(0, std::memory_order_relaxed);

                kepler_settlement.history.insert(kepler_settlement.history.end(),
                                                 kepler_exploration_hs.history.begin(),
                                                 kepler_exploration_hs.history.end());

                // BL-956: fold the 1660 handoff while `kepler_exploration_hs`
                // and the now-final `kepler_settlement` ownership are both
                // live, then let world setup read ITS grudges and corridors.
                const exploration_output kepler_exploration = make_exploration_output(
                    kepler_settlement, kepler_exploration_hs, &kepler_creeds);

                // BL-969: validated on the shipped path, against the live
                // table, same discipline as the Empires fold above. The span
                // wrote `kepler_creeds` in place, so the struct's copy is the
                // 1660 table and this is the check that it is.
                {
                    std::string why;
                    if (!exploration_output_valid(kepler_exploration, &why, &kepler_creeds))
                        record_handoff_violation("exploration_output", why);
                }

                kepler_corridors  = kepler_exploration.surviving_corridors;
                kepler_grudges    = kepler_exploration.grudges;
                kepler_grudge_cap = static_cast<int32_t>(ep.grudge_cap);

                // BL-975: THE TREASURIES CROSS TOO. Read off the handoff
                // struct's own region table, not the live sim state, because
                // this is on EXPLORATION.md's list of what the span hands
                // forward (BL-956 named it first). Summed per polity over the
                // regions it holds at 1660 — the chest is a fact about the
                // ground and the flag over the ground owns it (settlement.hpp,
                // `region::treasury`) — in ascending region order, as
                // integers, so the sum is exact. `generate_nations` Pass 7
                // converts it once (NATION_GENERATION.md § Pass 7).
                for (const region& rg : kepler_exploration.regions)
                {
                    if (rg.nation < 0 || rg.treasury <= 0) continue;
                    const std::size_t pol = static_cast<std::size_t>(rg.nation);
                    if (pol >= kepler_polity_treasuries.size())
                        kepler_polity_treasuries.resize(pol + 1, 0);
                    kepler_polity_treasuries[pol] += rg.treasury;
                }

                if (fixture != nullptr)
                    fixture->exploration_handoff = kepler_exploration;

                // BL-937: hand the sweep harness the span's real input and
                // output, on the same capture-not-re-derive footing as the
                // Empires block above. Costs nothing when no fixture was
                // asked for.
                //
                // DELIBERATE `kepler_exploration_hs` READS PAST THE FOLD
                // (BL-969): the fixture IS a capture of the live sim state --
                // that is what a harness re-running the span against
                // generation's own outcome needs -- so it takes the sim's
                // state by design, beside the handoff value it also holds.
                if (fixture != nullptr)
                {
                    fixture->exploration_ran    = true;
                    fixture->exploration_params = ep;
                    fixture->exploration_seed   = exploration_sim_seed(params);
                    fixture->pre_exploration_contacts = kepler_pass_one.contacts;
                    fixture->exploration_state        = kepler_exploration_hs;
                }

                // BL-946: THE RECORDED RECORD, same discipline as `hs` a few
                // lines below -- recorded once, here, at the one call site
                // that ran it, rather than re-derived by a consumer.
                //
                // DELIBERATE `kepler_exploration_hs` READS PAST THE FOLD
                // (BL-969): the four counters and the time-lapse are the
                // RECORD OF THE RUN for the generation screen and the Ages
                // view, not items on EXPLORATION.md's list of what the span
                // hands forward, and no world-setup consumer computes from
                // them. `exploration_output` carries no time-lapse for that
                // reason; widening it to carry one would put a presentation
                // artefact inside the contract.
                if (report != nullptr)
                {
                    report->exploration_years     = kepler_exploration_hs.years;
                    report->exploration_battles   = kepler_exploration_hs.battles;
                    report->exploration_conquests = kepler_exploration_hs.conquests;
                    report->exploration_foundings = kepler_exploration_hs.foundings;
                    for (generation_report::body_entry& be : report->bodies)
                        if (be.id == kepler)
                            be.exploration_timelapse = as_timelapse(kepler_exploration_hs);
                }

                // --------------------------------------------------------
                // BL-1040 — THE DIGITISATION SPAN, exploration_stop_year ->
                // digitisation_stop_year (1660 -> 1960), as its OWN call to
                // the same engine, resumed from `kepler_exploration` above.
                //
                // THE RUN PREDICATE IS THIS BLOCK'S NESTING. It sits inside
                // the block that ran Exploration, so it runs if and only if
                // Exploration ran (Ben, 2026-09-18) -- never on an epoch test
                // of its own; on the superseded arc (epoch >= 1700) Exploration
                // is off and this never opens. Its own gates are the switch
                // (default off until BL-1044) and `stop_after_exploration`,
                // which must stop BEFORE this span: that knob's own return
                // sits below population centres, past this call, so it is
                // read here -- the Exploration round's launch, exploration_
                // sweep and the seed-library fingerprints never pay for it.
                //
                // AFTER THE EXPLORATION FOLD AND VALIDATOR, BEFORE POPULATION
                // CENTRES: centres materialise from the 1960 demography, and
                // everything downstream that reads `kepler_settlement` (the
                // 1200-close market stand, nations, `gen_settlement`) reads
                // the 1960 map.
                if (params.digitisation_span_enabled && !gen_cfg.stop_after_exploration)
                {
                    // PARAMS FROM EXPLORATION'S DERIVATION, never the Empires
                    // round's (era_minus_one.cpp says why), with every table
                    // of the 1660 struct as the resume -- BL-1036's lossless
                    // set: polities, grudges, contacts, the surviving corridor
                    // record, the standing treaties and tribute, and the
                    // civilisation and creed records so the numbering
                    // continues. Trade flows rebuild from the treaties in the
                    // span's first round (the resume pointer's comment).
                    history_sim_params dp = digitisation_sim_params(params);
                    dp.resume_polities         = &kepler_exploration.polities;
                    dp.resume_grudges          = &kepler_exploration.grudges;
                    dp.resume_contacts         = &kepler_exploration.contacts;
                    dp.resume_corridors        = &kepler_exploration.surviving_corridors;
                    dp.resume_dated_objects    = &kepler_exploration.dated_objects;
                    dp.resume_civilisations    = &kepler_exploration.civilisations;
                    dp.resume_universal_creeds = &kepler_exploration.universal_creeds;
                    const uint32_t dseed = digitisation_sim_seed(params);

                    // THE SPAN OPENS ON THE STRUCT. The region table the
                    // resume reads is the handoff's own (equal to the live one
                    // at this line -- the fold copied it -- so this moves
                    // nothing today, and it keeps the struct the source if a
                    // later fold ever differs from the live settlement). The
                    // creeds stay the LIVE table: the span coins and folds
                    // cultures in place, the naming passes below read it, and
                    // the Exploration validator above has just proved it
                    // equals `kepler_exploration.cultures` row for row.
                    kepler_settlement.regions = kepler_exploration.regions;

                    // BL-1051 -- THE SPAN-OPEN SURVEY (INDUSTRY_TREE.md sec The
                    // scorer, "Forest is surveyed"). Every region the span
                    // opens on is surveyed ONCE, from its own tiles, for fuel
                    // and forest -- here because this is the one place both
                    // the tiles and the 1660 region table are live (the sim has
                    // neither `world&` nor tile ids by design). It writes two
                    // NEW fields and nothing else: `energy_q`, the treasury
                    // endowment and every gate mean stay as Exploration left
                    // them. INSIDE THE SWITCH, so with the span off neither
                    // field is ever written and no digest can see it.
                    //
                    // AFTER the struct copy above, so the 1660 handoff value
                    // itself stays exactly the close Exploration folded (its
                    // validator and the seed library read it); the survey is
                    // what the span opens ON, not part of what Exploration hands.
                    survey_regions_at_span_open(w, kepler_tiles, home_grid_width, home_grid_height,
                                                kepler_settlement.regions);
                    if (fixture != nullptr)
                        fixture->digitisation_open_regions = kepler_settlement.regions;

                    if (progress != nullptr)
                    {
                        progress->label.store(14, std::memory_order_relaxed); // the digitisation span
                        progress->sub_progress.store(0, std::memory_order_relaxed);
                        progress->sub_total.store(
                            static_cast<int>(dp.stop_year - dp.start_year),
                            std::memory_order_relaxed);
                    }

                    const gen_clock::time_point t_span_begin = gen_clock::now(); // reported only
                    const history_sim_state kepler_digitisation_hs = run_history_sim(
                        kepler_settlement, &kepler_creeds, terr.view(),
                        home_grid_width, home_grid_height, dp, dseed,
                        progress != nullptr ? &progress->sub_progress : nullptr,
                        works,
                        progress != nullptr ? progress->lapse_tap : nullptr);
                    const int64_t span_ms = ms_between(t_span_begin, gen_clock::now());

                    if (progress != nullptr)
                        progress->sub_total.store(0, std::memory_order_relaxed);

                    // The span's wars join the world log, as Exploration's do.
                    kepler_settlement.history.insert(kepler_settlement.history.end(),
                                                     kepler_digitisation_hs.history.begin(),
                                                     kepler_digitisation_hs.history.end());

                    // THE 1960 CLOSE, folded while the run and the now-final
                    // settlement are both live, and validated on the shipped
                    // path against the live creeds AND against the 1660 value
                    // it resumed from (BL-969's discipline, one span later).
                    const digitisation_output kepler_digitisation = make_digitisation_output(
                        kepler_settlement, kepler_digitisation_hs, &kepler_creeds);
                    {
                        std::string why;
                        if (!digitisation_output_valid(kepler_digitisation, &why, &kepler_creeds,
                                                       &kepler_exploration))
                            record_handoff_violation("digitisation_output", why);
                    }

                    // WHAT WORLD SETUP READS OF THIS CLOSE, TODAY: the live
                    // settlement it leaves (1960 ownership, population,
                    // treasury, culture) -- population centres, naming, the
                    // market stand, nations and `gen_settlement` all read
                    // `kepler_settlement`. NOT YET the three hoisted records:
                    // `kepler_corridors`, `kepler_grudges` and
                    // `kepler_polity_treasuries` still hold Exploration's 1660
                    // values set a few lines above. BL-1040 scoped that switch
                    // out; it is owed before the span ships on (BL-1044), and
                    // it is three assignments off `kepler_digitisation`, on
                    // the pattern of Exploration's own block.

                    // No report fields: `generation_report` is on the save seam
                    // (a field there is a `save_game_version` bump), and the
                    // wizard's Digitisation round, the one reader a span
                    // time-lapse would have, does not play a record yet.
                    if (fixture != nullptr)
                    {
                        fixture->digitisation_ran     = true;
                        fixture->digitisation_params  = dp;
                        fixture->digitisation_seed    = dseed;
                        fixture->digitisation_handoff = kepler_digitisation;
                        fixture->digitisation_state   = kepler_digitisation_hs;
                        // The profile is a process-wide accumulator the NEXT
                        // run resets; nothing between the call and this line
                        // runs the sim, and it is read only for a harness.
                        fixture->digitisation_rounds  = history_sim_last_profile().decision_rounds;
                        fixture->ms_digitisation      = span_ms;
                    }
                }
            }

            // Report what the era actually produced, into the generation record
            // rather than to stdout. The acceptance test for this block is
            // behavioural ("turmoil... losing / winning bodies"), and a block
            // that silently produced a peaceful world would otherwise look
            // identical to one that was never called — which is exactly how
            // this sim went unwired for so long.
            //
            // DELIBERATE `hs.` READS PAST THE FOLD (BL-969): the four counters
            // are the record of the run, not items on the doc's list of what
            // crosses, and nothing at world setup computes from them. The
            // time-lapse, by contrast, IS on the struct (`pass_one_output::
            // timelapse`, folded from this same `hs`), so it is read there.
            if (report != nullptr)
            {
                report->prehistory_battles   = hs.battles;
                report->prehistory_conquests = hs.conquests;
                report->prehistory_foundings = hs.foundings;
                report->prehistory_years     = hs.years;
                // NR-733: the ownership history itself, so the Ages view can
                // REPLAY the era rather than re-running it. Recorded at the one
                // call site that ran it, which is what makes it generation's own
                // era and not a second construction of one.
                //
                // Onto the CRADLE'S OWN ENTRY, because that is the body the era
                // was fought over — every other entry keeps an empty record, and
                // the view reads empty as "never settled".
                for (generation_report::body_entry& be : report->bodies)
                    if (be.id == kepler)
                        be.prehistory_timelapse = kepler_pass_one.timelapse;
            }

            // The same four counts into the fixture, so a harness holding one
            // can bind its re-run against them without also needing a report.
            // (`hs.` reads by design, as the exploration fixture block above:
            // a fixture captures the live sim state, BL-969.)
            if (fixture != nullptr)
            {
                fixture->battles   = hs.battles;
                fixture->conquests = hs.conquests;
                fixture->foundings = hs.foundings;
                fixture->years     = hs.years;
            }

            // --- BL-910: capitals and markets stand at the 1200 CE close ---
            //
            // `run_history_sim` marked `region::has_market` on every living
            // polity's capital (CIVILISATION.md sec Capitals exist at the
            // close) -- a PLACE AND A VISIBLE CONDITION, not an order book:
            // the market spawned here is default-constructed (no supply, no
            // demand, no price, no inventory) except for its anchor. It is a
            // SEPARATE mechanism from the BL-768 resource/junction carve
            // below, which prices and populates the campaign's own market
            // set once population centres exist; that carve is unchanged and
            // this block does not fold into it, only stands ahead of it.
            {
                const std::vector<entity_id>& grid = body_tile_grid(w, kepler);
                if (static_cast<int>(grid.size()) >= home_grid_width * home_grid_height)
                {
                    for (const region& rg : kepler_settlement.regions)
                    {
                        if (!rg.has_market) continue;
                        if (rg.row < 0 || rg.row >= home_grid_height
                         || rg.col < 0 || rg.col >= home_grid_width)
                            continue;
                        const entity_id anchor_tile =
                            grid[static_cast<std::size_t>(rg.row) * home_grid_width
                                 + static_cast<std::size_t>(rg.col)];
                        if (anchor_tile == null_entity) continue;
                        market_component mc;
                        mc.body        = kepler;
                        mc.centre_tile = anchor_tile;
                        w.markets[w.create_entity()] = mc;
                    }
                }
            }
        }
        // OUTSIDE the gate, deliberately: a skipped era must read as zero
        // elapsed rather than folding the whole remainder of generation into
        // the era's bucket (BL-754).
        t_era_end = gen_clock::now();

        // Population centres (BL-610, centres from demography): placed HERE,
        // after the Era -1 sim has grown, warred and plagued the regions'
        // populations, so the centre count and scale distribution are the
        // history's consequence rather than a land-area divisor and an
        // authored weighted draw. Same seed derivation as the pre-BL-610 call
        // site; only the position in the chain and the settlement argument
        // changed.
        generate_population_centres(w, kepler, /*seed=*/params.seed ^ 0x70701001u,
                                    &kepler_settlement);

        kepler_np.seed_tiles = settlement_seed_tiles(kepler_settlement);

        // BL-769 — THE HISTORY'S POLITICAL MAP CROSSES THE HANDOFF. Read here,
        // BEFORE `derive_national_character` overwrites `region::nation` with
        // the nation index: until that call the field holds the POLITY id the
        // sim wrote as it ran. Phase 5 folds a polity's regions into one nation
        // instead of growing an independent realm out of each anchor.
        kepler_np.seed_polities = settlement_seed_polities(kepler_settlement);

        // BL-975 — THE HISTORY'S CHESTS CROSS WITH ITS MAP. Indexed by the
        // same polity ids `seed_polities` just read, so Pass 2d can land each
        // polity's 1660 treasury on the seed it folds to. Empty when the
        // Exploration span did not run (opted out, or the wizard's Empires-
        // round launch), which credits nothing.
        kepler_np.polity_treasuries = kepler_polity_treasuries;

        // BL-898 — THE SAME READ, KEPT FOR THE SAME WINDOW. `seed_polities`
        // above is filtered to anchored regions because `generate_nations`
        // reads it as a parallel array; the grudge seeding needs the polity of
        // EVERY region, including the unanchored ones, to work out which nation
        // a polity's ground ended up inside. One line, taken at the one moment
        // `region::nation` still holds a polity id.
        kepler_region_polity.reserve(kepler_settlement.regions.size());
        for (const region& p : kepler_settlement.regions)
            kepler_region_polity.push_back(p.nation);

        // Each anchor carries its region's tongue across into Pass 5, so a
        // nation is named in the speech of the people who settled its core
        // rather than out of a bank of its own (BL-290).
        kepler_np.seed_tongues.reserve(kepler_settlement.regions.size());
        for (const region& p : kepler_settlement.regions)
        {
            if (p.anchor < 0) continue;
            // BL-826 — PLURALITY. A nation is named in ONE tongue, so the
            // distribution has to collapse to a single answer here, and the
            // largest people on the core is it. The same rule city_names.cpp
            // applies to a town, applied to the realm grown from that ground.
            const int pc = p.culture.plurality();
            kepler_np.seed_tongues.push_back(
                pc >= 0 && pc < static_cast<int>(kepler_creeds.cultures.size())
                    ? kepler_creeds.cultures[static_cast<std::size_t>(pc)].speech
                    : tongue{});
        }

        // Same act for the cities: the placeholder names generate_population_
        // centres coined before there was any culture are replaced with names
        // in the nearest region's tongue.
        name_population_centres(w, kepler, home_grid_width, kepler_settlement, kepler_creeds,
                                /*seed=*/params.seed ^ 0xC17910E6u);
    }

    // THE ANCIENT ERA HAS RUN. A caller that only wanted the history — the
    // wizard's history round — stops here rather than paying for borders, roads
    // and companies it will discard (about 95% of the wall clock; see
    // world_gen_config::stop_after_ancient_era). The world left behind is
    // deliberately half-built and must not be played.
    //
    // THE REPORT IS FINISHED FIRST, AND THAT ORDERING IS THE WHOLE OF THIS
    // BLOCK. The first cut returned immediately and shipped a report carrying
    // the era's TIME-LAPSE but not its SETTLEMENT — `be.settlement` is assigned
    // a hundred lines below, past the return — so the wizard's round got an
    // ownership record with no region coordinates to draw it against and
    // rendered an empty map for four thousand years while its own header
    // reported 611 foundings.
    //
    // It survived the scripted check because that check cannot see this path:
    // under `--verify` the round ADOPTS the harness's own fully-built world
    // rather than running a stopped one, so five green assertions said nothing
    // about the branch. Caught by driving the built app, which is what the
    // live-click rule is for.
    if (gen_cfg.stop_after_ancient_era)
    {
        if (report)
            for (generation_report::body_entry& be : report->bodies)
                if (be.id == kepler) { be.settlement = kepler_settlement; break; }
        return w;
    }

    // BL-946: the wizard's new Exploration round's own stop point, one round
    // later than the Empires round's above -- same contract, same reason.
    // (It stopped the Digitisation span too, at the span's own gate: the span
    // runs ahead of this line, so this return alone could not.)
    if (gen_cfg.stop_after_exploration)
    {
        if (report)
            for (generation_report::body_entry& be : report->bodies)
                if (be.id == kepler) { be.settlement = kepler_settlement; break; }
        return w;
    }

    // BL-1040: one span later -- the Digitisation span has run (when it was
    // enabled and Exploration ran), and nothing of world setup has. Same
    // contract as the two stops above.
    if (gen_cfg.stop_after_digitisation)
    {
        if (report)
            for (generation_report::body_entry& be : report->bodies)
                if (be.id == kepler) { be.settlement = kepler_settlement; break; }
        return w;
    }

    bump(9);
    const std::vector<entity_id> kepler_nations =
        generate_nations(w, kepler, kepler_tiles, home_grid_width, home_grid_height, kepler_np,
                         /*seed=*/params.seed ^ 0x4A71012u, progress);

    // The political axes are OUTPUTS now (BL-218): expansionism from the
    // border-contest integral, economic_focus from the resource class of the
    // regions settled during industrialisation, ideology from
    // industrialisation timing against neighbours. This overwrites the random
    // Pass 4 draw, which stays as the fallback for bodies with no settlement.
    derive_national_character(kepler_settlement, kepler_creeds, w,
                              kepler_nations, kepler_tiles, home_grid_width, home_grid_height);

    // BL-898 — THE GRUDGES BITE. `derive_national_character` has just put the
    // nation index in `region::nation`, and `kepler_region_polity` holds what
    // that same field said one call earlier — so this is the first and only
    // moment both halves of the polity -> nation correspondence exist at once.
    //
    // WHAT THIS IS NOT. It is not a grudge term inside an actor. The Era -1
    // scorer reads nothing new (BL-827's ruling, written at the field in
    // `history_sim.hpp` § Grudges, stands untouched); the record crosses the
    // handoff and becomes SENTIMENT, which is where `RELATIONS.md` § What each
    // quantity was before said it belonged all along. A campaign-era actor that
    // later reads sentiment is reading a quantity with an authority doc and a
    // visible cause, not a hidden weight.
    //
    // THE MAPPING IS NOT 1:1 AND IS NOT MADE SO. `merge_undersized_nations`
    // absorbs realms below the size floor, so a polity can reach the epoch with
    // no nation of its own; its grudges are DROPPED rather than redirected
    // (`grudge_sentiment.hpp` says why). The representative nation of a polity
    // is whichever nation holds the most of its regions, ties to the lowest
    // nation index — an ascending walk over a vector, so the answer cannot
    // depend on a container's layout.
    //
    // BL-956: the table read here is the 1660 one whenever the Exploration
    // span ran (see the span's block above); the fixture records exactly what
    // this site was handed.
    if (fixture != nullptr) fixture->setup_grudges = kepler_grudges;
    if (!kepler_grudges.empty() && !kepler_nations.empty()
        && kepler_region_polity.size() == kepler_settlement.regions.size())
    {
        int max_polity = -1;
        for (int pol : kepler_region_polity)
            if (pol > max_polity) max_polity = pol;

        if (max_polity >= 0)
        {
            const std::size_t pcount  = static_cast<std::size_t>(max_polity) + 1;
            const std::size_t ncount  = kepler_nations.size();
            // [polity][nation] region counts. Dense and tiny (~40 x ~40).
            std::vector<int> tally(pcount * ncount, 0);
            for (std::size_t ri = 0; ri < kepler_settlement.regions.size(); ++ri)
            {
                const int pol = kepler_region_polity[ri];
                const int nat = kepler_settlement.regions[ri].nation;
                if (pol < 0 || nat < 0 || static_cast<std::size_t>(nat) >= ncount) continue;
                ++tally[static_cast<std::size_t>(pol) * ncount + static_cast<std::size_t>(nat)];
            }

            std::vector<entity_id> polity_nation(pcount, null_entity);
            for (std::size_t pol = 0; pol < pcount; ++pol)
            {
                int best = -1, best_n = -1;
                for (std::size_t n = 0; n < ncount; ++n)
                {
                    const int c = tally[pol * ncount + n];
                    if (c > best) { best = c; best_n = static_cast<int>(n); }
                }
                if (best > 0 && best_n >= 0)
                    polity_nation[pol] = kepler_nations[static_cast<std::size_t>(best_n)];
            }

            grudge_sentiment_params gsp;
            gsp.score_full = kepler_grudge_cap;

            std::vector<grudge_sentiment_seed> seeds;
            const grudge_sentiment_report grep_ =
                seed_grudge_sentiment(w.sentiment, kepler_grudges, polity_nation, gsp, &seeds);

            if (report != nullptr)
            {
                report->grudge_sentiment_rows      = grep_.rows_seeded;
                report->grudge_sentiment_dropped   =
                    grep_.below_floor + grep_.no_successor + grep_.self_pair + grep_.out_of_range;
            }
        }
    }

    // BL-750 — THE TARIFF POSTURE, ENACTED. `derive_national_character` has just
    // put the nation index in `region::nation`, so this is the first moment a
    // nation's inherited protection can be read; the law it bands to is an
    // ordinary `import_tariff` authored by that nation, which is what finally
    // gives NATIONS.md's "vocabulary ahead of its consumer" its instance.
    //
    // A world whose polities never industrialised enacts NOTHING here, and that
    // is a legitimate outcome rather than a gap — see `polity::protection_q`.
    // THIS IS THE ENACTMENT SEAM, NOT THE DERIVATION (BL-976): the Era -1 sim
    // writes `protection_q` only on the two-span arc, and Digitisation owns the
    // derivation on the shipped arc (DIGITISATION.md § The boundary). The call
    // stays so that whatever writes the field is read by one path.
    seed_national_tariffs(w, kepler_nations,
                          derive_national_protection(
                              kepler_settlement, static_cast<int>(kepler_nations.size())));

    // Coverage (BL-463): the population pass above ran before there were borders,
    // so it could only derive its target from LAND AREA. The NATION term lands
    // here — one founding for each nation that ended up holding no centre at all,
    // which was 61% of them across an eight-seed census before this call existed.
    // It must precede generate_roads (a nation below two centres gets no road
    // lattice) and market seeding (a nation with no centre gets no market), which
    // is why one constant explained the settlement, road and hub gaps at once.
    ensure_national_population_centres(w, kepler, /*seed=*/params.seed ^ 0x70702002u);

    // The coverage foundings were coined after the naming pass ran, so they have
    // no name yet. Re-running it with the SAME seed is byte-stable for every
    // centre that already had one — the pass walks centres in sorted-id order off
    // an independent stream, and the new ids sort last, so the existing prefix of
    // draws is reproduced exactly and only the new centres consume fresh ones.
    name_population_centres(w, kepler, home_grid_width, kepler_settlement, kepler_creeds,
                            /*seed=*/params.seed ^ 0xC17910E6u);

    // Urban ground (BL-612, urban ground stamped): every centre — coverage
    // foundings included, which is why this follows ensure_national — paves a
    // footprint sized by its scale, so city ground is scarce and contested
    // from turn one. RNG-free; perturbs no stream. Runs before
    // generate_corporations so starting assets land on a world whose ground
    // already says where the cities are.
    stamp_urban_land_use(w, kepler);

    // Everything from here to globalisation is the 0-1960 story. An antiquity
    // start (BL-271: epoch_year < 1700) generates the world BEFORE it happens —
    // no rupture is pre-resolved, no charter is enacted, no common tongue forms.
    // The year-tick sim produces that history live instead of this pass
    // pre-computing it.
    const bool antiquity_start = params.epoch_year < 1700;

    // The historical ruptures: collapse, war and revolution as transforms on
    // territory, character and abundance — and, where a war is won, the
    // victor's gods travel with the border and part of the loser's record is
    // destroyed rather than merely contradicted.
    if (!antiquity_start)
        resolve_historical_ruptures(kepler_settlement, kepler_creeds, w,
                                    kepler_nations, kepler_tiles, home_grid_width, home_grid_height,
                                    /*seed=*/params.seed ^ 0x80174E5u);

    // Stages 1-2 can only be written now: the Charter Act names a nation and
    // the border accord counts them, and neither existed a moment ago. The
    // lines then merge into Kepler's biography, which the History ledger reads.
    if (!antiquity_start)
        record_institutional_history(kepler_hist, w, kepler, kepler_tiles, home_grid_width);

    // Globalisation closes the generated story: the common tongue line is the
    // hinge from the creeds' native record to the campaign epoch — rendered in
    // the player's language (English for now; Ben, 2026-07-31). The creed
    // lines then merge into the same ladder history the report reads.
    if (!antiquity_start)
        record_globalisation(kepler_creeds, w, kepler);
    kepler_hist.history.insert(kepler_hist.history.end(),
                               std::make_move_iterator(kepler_creeds.history.begin()),
                               std::make_move_iterator(kepler_creeds.history.end()));
    kepler_creeds.history.clear(); // moved-from; the ladder owns the lines now.

    // The settlement/industrialisation record joins the same biography — after
    // the ruptures have had their chance to destroy part of it, so what merges
    // is the record as it SURVIVED, lacunae included.
    kepler_hist.history.insert(kepler_hist.history.end(),
                               std::make_move_iterator(kepler_settlement.history.begin()),
                               std::make_move_iterator(kepler_settlement.history.end()));
    kepler_settlement.history.clear(); // moved-from; the ladder owns the lines now.

    if (report)
    {
        for (generation_report::body_entry& be : report->bodies)
        {
            if (be.id != kepler) continue;
            be.state.history.insert(be.state.history.end(),
                                    kepler_hist.history.begin(), kepler_hist.history.end());
            // The region set / checkpoints / lacunae travel with the report
            // (its `history` is already merged above and stays empty here,
            // matching the continents convention).
            be.settlement = kepler_settlement;
            std::stable_sort(be.state.history.begin(), be.state.history.end(),
                [](const history_event& a, const history_event& b)
                { return a.years_before_epoch > b.years_before_epoch; });
            break;
        }
    }

    // BL-623 (provinces before roads; Ben, 2026-08-25, resolving NR-640 as
    // option c): the homeworld's province partition runs HERE, before
    // generate_roads — the partition reads rivers and urban transforms but NOT
    // roads (the road-bind divisor retired with this reorder), and every
    // settlement-less province receives its anchor founding NOW, so the road
    // pass below stamps the lattice over the COMPLETE settlement set and every
    // anchor joins it like any village. The partition consumes no RNG stream
    // (stateless folds), so inserting it here perturbs nothing around it.
    //
    // Selene and the asteroids do not exist yet — they are authored below,
    // after the homeworld's political pipeline — so this call partitions the
    // bodies that do, and the CANONICAL whole-world partition is rebuilt at
    // the end of this function once every body's tiles exist. The rebuild
    // reproduces this partition byte-identically for the bodies present here:
    // the fill is a pure function of the pre-road world and the seed, none of
    // its inputs (heights, rivers, nation assignment, the non-anchor centre
    // set) changes after this point, and the anchors founded between the two
    // calls are skipped as seeds by their `province_anchor` flag
    // (province_partition_harness P6 is the identity's guard).
    build_province_partition(w, params.seed ^ 0x50524F56u);

    // BL-611 (province centre anchor): every land province on the settled body
    // holds a centre — the leftover pockets the centre-seeded fill could not
    // reach get a scale-1 anchor founded on their best ground. RNG-free. The
    // re-name is the byte-stable re-run name_population_centres documents (new
    // ids sort last, the existing prefix of draws reproduces exactly); the
    // re-stamp is idempotent and paves only the new anchors' footprints.
    ensure_province_anchor_centres(w, kepler);
    name_population_centres(w, kepler, home_grid_width, kepler_settlement, kepler_creeds,
                            /*seed=*/params.seed ^ 0xC17910E6u);
    stamp_urban_land_use(w, kepler);

    // Road network (BL-146): stamp each nation's road lattice onto tile.road_level,
    // after nations + population centres exist — anchor foundings included
    // (BL-623), so they take local streets and spurs like any village and the
    // off-lattice centre class ends here. Deterministic; no seed of its own —
    // a pure function of the generated tiles/nations/centres.
    bump(10);
    generate_roads(w, kepler);

    // ANCIENT ROADS, STAMPED FROM THE HISTORY (BL-768; Ben, the eight-phase
    // reorder point 4 — "we should also be laying simple roads to supply
    // provinces"). The Era -1 sim recorded every corridor it moved an army or a
    // founding party along; this stamps those lines onto the tile field at their
    // own ancient tier — traffic, and the works the corridor's two ends raised,
    // never the 1960 qualification percentile the national lattice reads.
    //
    // AFTER generate_roads, and the ordering is argued in road_generation.hpp:
    // stamping takes the max per tile, so this is purely additive and no
    // national road is downgraded, whereas stamping first would re-route the
    // whole national MST off the ancient corridors' cheapened ground.
    //
    // No-op when the era did not run — `kepler_corridors` is empty, and every
    // harness declaring `no_prehistory()` takes exactly that path. BL-956: the
    // set is the Exploration span's 1660 surviving network whenever it ran.
    if (fixture != nullptr) fixture->setup_corridors = kepler_corridors;
    if (!kepler_corridors.empty())
    {
        std::vector<history_road_node> road_nodes;
        road_nodes.reserve(kepler_settlement.regions.size());
        for (const region& p : kepler_settlement.regions)
            road_nodes.push_back(history_road_node{ p.col, p.row, p.work_reach_mod });
        stamp_history_roads(w, kepler, road_nodes, kepler_corridors);
    }

    // Attach installations to the first two land tiles found in raster order.
    // This lookup used to publish its first tile as `kepler_home_tile` for the
    // player unit stub further down; BL-635 deleted that stub (see below), and
    // with it the only reader outside this block.
    {
        auto land = first_land_tiles(kepler_tiles, w, home_grid_width, home_grid_height, 2);

        const entity_id kepler_extraction = w.create_entity();
        w.buildings[kepler_extraction] = building_component{
            .tile               = land.size() > 0 ? land[0] : null_entity,
            .type               = building_type::extraction_site,
            .workforce_assigned = 0.75f,
        };
        w.stockpiles[kepler_extraction] = stockpile_component{};

        const entity_id kepler_processor = w.create_entity();
        w.buildings[kepler_processor] = building_component{
            .tile               = land.size() > 1 ? land[1] : null_entity,
            .type               = building_type::processing_facility,
            .workforce_assigned = 0.50f,
        };
        w.stockpiles[kepler_processor] = stockpile_component{};
    }

    // BL-132 change (3): corporations generate BEFORE markets, not after --
    // the reorder this item's design calls for, so market carving can read
    // where competing corporations actually cluster, not just raw nation
    // geology. Moved from its old post-market position (below); its own
    // ordering needs are already satisfied here: nations + kepler_settlement
    // exist (Pass 1/2/BL-219 focus), and the pre-authored installations just
    // above are in w.buildings for corporate asset placement to collision-avoid.
    // Corporations: 6-10 actors registered in the generated nations, including
    // the player's (which sets w.player_entity). See CORPORATION_GENERATION.md.
    bump(11);
    generate_corporations(w, corporation_params{ .corporation_count = gen_cfg.corporation_count,
                                                 .seed_starting_force = gen_cfg.seed_starting_force },
        /*seed=*/params.seed ^ 0x4A71012u, &kepler_settlement, progress);

    // Kepler markets — population-anchored but RESOURCE-CARVED (BL-096). Markets
    // still anchor to population-centre tiles (catchment routing via market_for_tile
    // partitions the map), but how finely a nation's territory is split into markets
    // is shaped by its tradeable-resource concentration: a resource-rich nation
    // fractures into more markets (a lower population-scale gate admits more of its
    // centres), a barren one folds into its neighbour (a higher gate — its smaller
    // centres get no market and their tiles route to the nearest neighbour's).
    // Nations are the carving actor, so a resource cluster spanning two nations'
    // territory yields two markets. One-pass at world-gen, deterministic; a small
    // seeded jitter varies the borderline split per campaign (fresh XOR offset,
    // uncorrelated with the nation/corp streams). BL-132 layers two more terms
    // onto this same nation-carved gate: the market SITE itself is a
    // resource<->population trade-flow proxy rather than the bare population
    // tile (see trade_flow_proxy_site below), and the concentration driving the
    // gate also reads how many distinct corporations are already competing in
    // the territory (corps_in_nation below), not raw geology alone.
    // Resources outside the tradeable prototype subset stay at base 0 and are never
    // traded. Supply/demand are seeded by the substrate injection each tick.
    {
        const market_component kepler_market_template{
            .body       = kepler,
            .base_price = gen_cfg.kepler_base_price,
        };

        // Per-nation tradeable-resource concentration = mean raw-deposit richness
        // per owned tile over the tradeable subset. Classified against the
        // cross-nation mean into a population-scale gate (2 = fracture, 3 = normal,
        // 4 = fold). Tunable via rich_factor / barren_factor.
        constexpr resource_type tradeable_raws[] = { resource_type::iron_ore,
                                                      resource_type::petroleum,
                                                      resource_type::water,
                                                      resource_type::agricultural_produce };
        const float rich_factor   = gen_cfg.market_carving.rich_factor;   // concentration >= mean × this → fracture (gate 2)
        const float barren_factor = gen_cfg.market_carving.barren_factor; // concentration <  mean × this → fold    (gate 4)

        // BL-132 change (3): how many DISTINCT corporations hold at least one
        // asset in a nation's territory — the actual competing-actor count a
        // reorder to "corps before markets" exists to make available. A
        // territory several corporations are already carving between them
        // reads as more commercially contested than the same raw geology held
        // by none, so it nudges toward fracture (more, finer markets) on top
        // of the geological concentration term below, rather than replacing it
        // — richness says there's something worth trading, corp presence says
        // multiple actors are already trading it.
        std::map<entity_id, int> corps_in_nation; // std::map → ascending id (deterministic)
        for (const auto& [cid, cc] : w.corporations)
        {
            std::unordered_set<entity_id> nations_touched;
            for (const entity_id bid : cc.assets)
            {
                const auto bit = w.buildings.find(bid);
                if (bit == w.buildings.end())
                    continue;
                const auto nit = w.tile_to_nation.find(bit->second.tile);
                if (nit == w.tile_to_nation.end())
                    continue;
                nations_touched.insert(nit->second);
            }
            for (const entity_id nid : nations_touched)
                ++corps_in_nation[nid];
        }
        const float corp_presence_gain = gen_cfg.market_carving.corp_presence_gain;

        std::map<entity_id, float> concentration; // std::map → ascending id (deterministic)
        for (const auto& [nid, nc] : w.nations)
        {
            float sum = 0.0f;
            for (const resource_type rt : tradeable_raws)
                sum += nc.resource_abundance[static_cast<std::size_t>(rt)];
            const float tiles = static_cast<float>(std::max<std::size_t>(1, nc.tiles.size()));
            const auto cit = corps_in_nation.find(nid);
            const int  n_corps = (cit != corps_in_nation.end()) ? cit->second : 0;
            concentration[nid] = (sum / tiles) * (1.0f + corp_presence_gain * static_cast<float>(n_corps));
        }
        // Seeded jitter in ascending nation-id order (deterministic; fresh offset).
        {
            std::mt19937 jitter_rng(params.seed ^ 0xA5310096u);
            std::uniform_real_distribution<float> jitter(0.85f, 1.15f);
            for (auto& [nid, c] : concentration)
                c *= jitter(jitter_rng);
        }
        float mean_conc = 0.0f;
        if (!concentration.empty())
        {
            for (const auto& [nid, c] : concentration)
                mean_conc += c;
            mean_conc /= static_cast<float>(concentration.size());
        }
        auto gate_for_nation = [&](entity_id nid) -> int {
            const auto it = concentration.find(nid);
            if (it == concentration.end() || mean_conc <= 0.0f)
                return 3;
            if (it->second >= mean_conc * rich_factor)   return 2; // fracture: more markets
            if (it->second <  mean_conc * barren_factor) return 4; // fold: fewer markets
            return 3;
        };

        // ------------------------------------------------------------------
        // BL-768 — MARKETS EMERGE WHERE TRADE CONCENTRATED, not from population
        // alone. Ben, the eight-phase reorder point 4: "markets should begin to
        // emerge towards the end of this phase."
        //
        // The gate above is a nation-grain judgement — this nation's geology and
        // its competing corporations — and it says nothing about WHERE inside
        // that territory exchange actually happened. The history now does: every
        // corridor in `kepler_corridors` is a line the era supplied an army or a
        // founding party along, so a region several of them MEET at is a
        // junction, and a junction is where goods change hands.
        //
        // A JUNCTION IS A GRAPH PROPERTY, NOT A TUNED PERCENTILE. Degree — the
        // number of distinct corridors incident on a region — is a plain integer
        // count over a sorted record, so it cannot drift with a container's
        // layout and it needs no threshold argued from a distribution. It is
        // still MEASURED: over `history_sweep 8 --epoch 1960` (2026-09-06) the
        // 4,657 regions of eight worlds grade 1,297 at degree 0, 3,236 at 1-2,
        // and only 124 at 3 or more — the busiest at 66. So three keeps the
        // junction set at 2.7% of regions, which is the difference between a
        // line and a crossing rather than a nudge to the whole map.
        //
        // IT ONLY EVER LOWERS THE GATE, so this term ADDS markets and removes
        // none. Raising it at a quiet region would delete a market the economy
        // is already built on, and "markets emerge" is an emergence rather than
        // a cull. The floor is the existing FRACTURE gate (2), never below it,
        // so a village still never carries a market however many roads meet on
        // it — the ladder's own bottom rung is not moved.
        //
        // AND IT DROPS TO THAT FLOOR OUTRIGHT, rather than by one rung. One rung
        // was the first cut and it is the weaker claim: it only ever helps a
        // centre sitting exactly one scale under its nation's gate, so on the
        // shipping seed it opened no market at all. Dropping to the fracture gate
        // says the stronger and more historical thing — a crossroads fractures
        // into markets as finely as a rich nation's territory does, BECAUSE
        // trade concentrated there. That is an entrepôt on poor ground, which is
        // the shape a barren nation folded into its neighbour could not
        // otherwise produce.
        constexpr int kMarketJunctionDegree = 3;
        std::vector<int> region_corridor_degree(kepler_settlement.regions.size(), 0);
        for (const history_corridor& c : kepler_corridors)
        {
            if (c.a < region_corridor_degree.size()) ++region_corridor_degree[c.a];
            if (c.b < region_corridor_degree.size()) ++region_corridor_degree[c.b];
        }
        if (report != nullptr)
        {
            report->prehistory_corridors = static_cast<int64_t>(kepler_corridors.size());
            for (const int d : region_corridor_degree)
                if (d >= kMarketJunctionDegree) ++report->prehistory_junctions;
        }
        // `nearest_region` is the canonical read of a region's extent — the same
        // Voronoi over anchors that binds a centre to the region that grew it
        // (BL-783) and names it in that region's tongue. Reusing it is what makes
        // "this centre's region" one answer rather than two.
        auto centre_is_trade_junction = [&](entity_id pop_tile) -> bool {
            if (kepler_corridors.empty()) return false;
            const auto pit = w.tiles.find(pop_tile);
            if (pit == w.tiles.end()) return false;
            const int ri = nearest_region(kepler_settlement, pit->second.grid_x,
                                          pit->second.grid_y, home_grid_width);
            if (ri < 0 || ri >= static_cast<int>(region_corridor_degree.size()))
                return false;
            return region_corridor_degree[static_cast<std::size_t>(ri)]
                   >= kMarketJunctionDegree;
        };

        // Seed markets in ascending centre-id order for deterministic market ids.
        std::vector<entity_id> centre_ids;
        centre_ids.reserve(w.population_centres.size());
        for (const auto& [cid, pcc] : w.population_centres)
            centre_ids.push_back(cid);
        std::sort(centre_ids.begin(), centre_ids.end());

        // BL-132 change (2): markets centre on a static resource<->population
        // trade-flow PROXY rather than sitting exactly on the population tile.
        // Live trade routes don't exist at gen-time, so the proxy is the
        // strongest nearby pull toward a rich deposit: within a bounded radius
        // of the population tile, the site maximising richness/(1+dist^2) wins
        // (a simple gravity model — closer and richer both raise the pull),
        // defaulting back to the population tile itself when nothing nearby
        // beats it (the common case away from a rich cluster, which keeps this
        // a proxy rather than a relocation of every market). Kepler's raster
        // grid, built once here rather than per-market.
        constexpr int    market_gw = home_grid_width, market_gh = home_grid_height;
        constexpr int    proxy_radius = 8; // tiles — a market's plausible catchment reach
        std::vector<entity_id> kepler_tile_ids(
            static_cast<std::size_t>(market_gw * market_gh), null_entity);
        for (const auto& [tid, tc] : w.tiles)
        {
            if (tc.body != kepler)
                continue;
            const int idx = tc.grid_y * market_gw + tc.grid_x;
            if (idx >= 0 && idx < market_gw * market_gh)
                kepler_tile_ids[static_cast<std::size_t>(idx)] = tid;
        }
        auto tile_richness = [&](entity_id tid) -> float {
            const auto it = w.tiles.find(tid);
            if (it == w.tiles.end())
                return 0.0f;
            float sum = 0.0f;
            for (const resource_type rt : tradeable_raws)
                sum += it->second.resource_deposit[static_cast<std::size_t>(rt)];
            return sum;
        };
        auto trade_flow_proxy_site = [&](entity_id pop_tile) -> entity_id {
            const auto pit = w.tiles.find(pop_tile);
            if (pit == w.tiles.end())
                return pop_tile;
            const int pcol = pit->second.grid_x, prow = pit->second.grid_y;

            entity_id best_site  = pop_tile;
            float     best_score = tile_richness(pop_tile); // dist=0 -> /(1+0)
            for (int dr = -proxy_radius; dr <= proxy_radius; ++dr)
            {
                const int row = prow + dr;
                if (row < 0 || row >= market_gh)
                    continue;
                for (int dc = -proxy_radius; dc <= proxy_radius; ++dc)
                {
                    if (dr == 0 && dc == 0)
                        continue;
                    const int col = ((pcol + dc) % market_gw + market_gw) % market_gw;
                    const entity_id cand = kepler_tile_ids[
                        static_cast<std::size_t>(row * market_gw + col)];
                    if (cand == null_entity)
                        continue;
                    const float d2 = static_cast<float>(dr * dr + dc * dc);
                    const float score = tile_richness(cand) / (1.0f + d2);
                    if (score > best_score)
                    {
                        best_score = score;
                        best_site  = cand;
                    }
                }
            }
            return best_site;
        };

        int markets_seeded = 0;
        for (const entity_id cid : centre_ids)
        {
            const population_centre_component& pcc = w.population_centres.at(cid);
            const auto tile_it = w.population_centre_tile.find(cid);
            if (tile_it == w.population_centre_tile.end())
                continue;
            const auto tc_it = w.tiles.find(tile_it->second);
            if (tc_it == w.tiles.end() || tc_it->second.body != kepler)
                continue;

            // Nation-carved population-scale gate for this centre's owning nation.
            int gate = 3;
            const auto nit = w.tile_to_nation.find(tile_it->second);
            if (nit != w.tile_to_nation.end())
                gate = gate_for_nation(nit->second);
            // BL-768: a centre standing where the history's corridors met is
            // gated as a rich nation's centres are. The GATE moves, not the
            // scale — a trade junction lets a smaller centre carry a market, it
            // does not make the centre big.
            const int nation_gate = gate;
            if (centre_is_trade_junction(tile_it->second))
                gate = std::min(gate, 2);
            // Counted HERE, where both gates are in hand, so the report carries
            // the exact number of markets the history's trade opened rather than
            // a difference between two worlds that do not share a settlement.
            if (report != nullptr && gate < nation_gate && pcc.scale >= gate
                && pcc.scale < nation_gate)
                ++report->markets_from_trade;
            if (pcc.scale < gate)
                continue; // folds into a neighbouring market (routed by market_for_tile)

            market_component mc = kepler_market_template;
            mc.centre_tile = trade_flow_proxy_site(tile_it->second);
            mc.price       = mc.base_price; // start at canonical base
            w.markets[w.create_entity()] = mc;
            ++markets_seeded;
        }
        // Fallback: if no centre qualified, seed one unanchored market.
        if (markets_seeded == 0)
        {
            market_component mc = kepler_market_template;
            mc.price = mc.base_price;
            w.markets[w.create_entity()] = mc;
        }

        // ------------------------------------------------------------------
        // C -> D: endemic goods are priced BY DISTANCE FROM WHERE THEY GROW
        // (BL-191). This is the whole mercantile mechanic, and it needs no
        // change to the clearing engine: market_component::base_price is already
        // per-market and authored here, so a good is simply cheap at its origin
        // and dear far from it. Supply and demand then push around that base as
        // they do for every other resource.
        //
        // Distance is physical for now. "Geopolitical" earns its meaning when
        // diplomacy and AI behaviour land (Ben, 2026-07-22) — at which point the
        // multiplier below gains a political term rather than being replaced.
        // ------------------------------------------------------------------
        if (!kepler_pl.endemics.empty() && !w.markets.empty())
        {
            const float source_price   = gen_cfg.endemic.source_price;  ///< Cheap where it grows.
            const float distance_gain  = gen_cfg.endemic.distance_gain; ///< Multiplier across the globe.
            constexpr int   gw = home_grid_width, gh = home_grid_height;
            const float half_diag = std::sqrt(static_cast<float>((gw / 2) * (gw / 2) + gh * gh));

            for (const endemic_good& e : kepler_pl.endemics)
            {
                const std::size_t ri = static_cast<std::size_t>(e.good);

                // Where it actually grows. Gathered once per good from w.tiles'
                // unordered walk — safe, because the only consumer below takes a
                // min over the set, which is order-independent.
                std::vector<std::pair<int, int>> sources;
                for (const auto& [tid, tc] : w.tiles)
                    if (tc.body == kepler && tc.resource_deposit[ri] > 0.0f)
                        sources.emplace_back(tc.grid_x, tc.grid_y);
                if (sources.empty())
                    continue; // this world never evolved it; nothing to price

                for (auto& [mid, mc] : w.markets)
                {
                    if (mc.body != kepler)
                        continue;

                    // An unanchored market has no location, so it prices at the
                    // midpoint rather than pretending to a distance it does not have.
                    float norm = 0.5f;
                    const auto tit = w.tiles.find(mc.centre_tile);
                    if (tit != w.tiles.end())
                    {
                        float best = half_diag;
                        for (const auto& [sx, sy] : sources)
                        {
                            int dc = std::abs(tit->second.grid_x - sx);
                            if (dc > gw / 2) dc = gw - dc; // the surface wraps
                            const int dr = std::abs(tit->second.grid_y - sy);
                            const float d = std::sqrt(static_cast<float>(dc * dc + dr * dr));
                            best = std::min(best, d);
                        }
                        norm = std::min(1.0f, best / half_diag);
                    }

                    mc.base_price[ri] = source_price * (1.0f + distance_gain * norm);
                    mc.price[ri]      = mc.base_price[ri];
                }
            }
        }
    }

    // THE PLAYER UNIT STUB IS GONE (BL-635, 2026-08-26), and its removal is the
    // deletion of a duplicate rather than a change of design.
    //
    // What stood here was a 50-head unit owned by `w.player_entity`, seeded on
    // the Kepler home tile with no muster base — written when nothing else in
    // the world produced a unit at all. Corporation generation has produced one
    // since BL-324: `seed_starting_military` gives every generated corporation a
    // military base and one 50-head unit on it. The player's corp IS a generated
    // corporation, so it collected both, and this stub was never removed.
    //
    // MEASURED (tools/verify/spawn_solvency.cpp, 8 seeds of the shipped spawn):
    // the seated corporation fielded 2 units / 100 heads on every single seed
    // while a rival that had any force at all fielded 1 unit / 50 heads. Under
    // BL-454's standing-force upkeep that asymmetry is not cosmetic — it is a
    // recurring cash cost the player alone pays, twice over, from turn one, and
    // nobody designed it.
    //
    // Removing it is not a subsidy: it takes the player's opening force DOWN to
    // exactly what every rival is handed, which is what a level opening means.
    // Nothing else referenced `kepler_unit`.

    // -----------------------------------------------------------------------
    // Selene — Kepler's moon (Luna analogue)
    // 90×42 tile grid (the planet ratio at half scale). Airless regolith surface,
    // icy at the polar rows only, crater-dominated landforms.
    // -----------------------------------------------------------------------

    const entity_id selene = w.create_entity();
    w.bodies[selene] = body_component{
        .name                                 = naming.bodies[2],
        .type                                 = body_type::moon,
        .parent                               = kepler,
        .orbital_radius_au                    = 0.30f,
        .orbital_angle_rad                    = 0.0f,
        .orbital_angular_velocity_rad_per_day = 0.30f,
        .grid_width                           = 90,
        .grid_height                          = 42,
    };

    // Luna-analogue. orbit_au is its distance from the STAR (it shares Kepler's
    // instellation); parent_orbit_au is its distance from Kepler, which is what
    // drives the tidal term.
    std::vector<float> selene_bias;
    continent_state selene_cs;
    const planetology_state selene_pl = plan(2, selene, 90, 42, selene_bias, nullptr, &selene_cs);
    const uint32_t selene_tile_seed = params.seed ^ 0x5E1E001u;
    generate_body_tiles(w, selene, 90, 42, selene_pl.profile,
        selene_tile_seed, deposit_scalar, &selene_pl, nullptr, &selene_bias, nullptr, &selene_cs);
    record_tile_inputs(selene, selene_tile_seed, 90, 42, /*used_convergent=*/false);

    // -----------------------------------------------------------------------
    // Asteroid belt — a band beyond Kepler. The belt itself is not a body; it
    // is system-level data the Solar canvas renders as a textured ring. One
    // notable asteroid (Pallas) sits within the band as a separate, selectable
    // body entity drawn over it, carrying a small tile grid (no water) so it is
    // explorable like the planets. See backlog.json (asteroid belt) and SOLAR.md.
    // -----------------------------------------------------------------------
    w.belt = asteroid_belt{ /*inner_radius_au=*/2.10f, /*outer_radius_au=*/3.30f };

    struct notable_asteroid
    {
        int         proto_index; ///< Its slot in the prototype set — and in `naming`.
        float       radius_au;
        float       angle_rad;
        uint32_t    seed;
    };
    constexpr notable_asteroid notables[] = {
        { 3, 3.05f, 4.6f, 0x9A11A5u },
    };

    for (const notable_asteroid& a : notables)
    {
        const entity_id id = w.create_entity();
        w.bodies[id] = body_component{
            .name                                 = naming.bodies[static_cast<std::size_t>(a.proto_index)],
            .type                                 = body_type::asteroid,
            .parent                               = null_entity,
            .orbital_radius_au                    = a.radius_au,
            .orbital_angle_rad                    = a.angle_rad,
            .orbital_angular_velocity_rad_per_day = kepler_angular_velocity(a.radius_au),
            .grid_width                           = 30,
            .grid_height                          = 14,
        };

        // Differentiated on 26-Al heat, then stripped: the core_fragment branch
        // exits the chain at accretion and the whole object becomes the deposit.
        std::vector<float> ast_bias;
        continent_state ast_cs;
        const planetology_state ast_pl = plan(a.proto_index, id, 30, 14, ast_bias, nullptr, &ast_cs);
        const uint32_t ast_tile_seed = params.seed ^ a.seed;
        generate_body_tiles(w, id, 30, 14, ast_pl.profile,
            ast_tile_seed, deposit_scalar, &ast_pl, nullptr, &ast_bias, nullptr, &ast_cs);
        record_tile_inputs(id, ast_tile_seed, 30, 14, /*used_convergent=*/false);
    }

    // Freeze each body's authored phase as its epoch angle, so the econ tick can
    // reconstruct positions purely from the day tick (orbital_angle_at_tick) while
    // orbital_angle_rad itself keeps advancing per frame for rendering.
    for (auto& [body_id, body] : w.bodies)
        body.orbital_epoch_angle_rad = body.orbital_angle_rad;

    // ---------------------------------------------------------------------
    // Per-stage system summary for the generation screen. Counts across the
    // bodies the chain just ran, so each revealed stage says what it actually
    // did system-wide rather than restating the design.
    // ---------------------------------------------------------------------
    if (report)
    {
        const auto& bs = report->bodies;
        auto count_if_stage = [&](life_stage min) {
            int n = 0;
            for (const auto& b : bs) if (b.state.peak >= min) ++n;
            return n;
        };
        auto named = [&](life_stage min) {
            std::string out;
            for (const auto& b : bs)
                if (b.state.peak >= min) { if (!out.empty()) out += ", "; out += b.name; }
            return out.empty() ? std::string("none") : out;
        };
        char buf[256];
        const planetology_params& pp = rw.params;

        std::snprintf(buf, sizeof buf,
            "Helios: %.2f solar masses, %.2f Gyr old, metallicity %.2fx solar.",
            static_cast<double>(pp.star_mass), static_cast<double>(pp.system_age_gyr),
            static_cast<double>(pp.metallicity));
        report->stage_lines.emplace_back(buf);

        std::snprintf(buf, sizeof buf, "%d bodies condensed from one disc.",
                      static_cast<int>(bs.size()));
        report->stage_lines.emplace_back(buf);

        int with_air = 0;
        for (const auto& b : bs)
            if (b.state.profile.atmosphere >= atmosphere_class::moderate) ++with_air;
        std::snprintf(buf, sizeof buf, "%d of %d held an atmosphere. The rest are exposed rock.",
                      with_air, static_cast<int>(bs.size()));
        report->stage_lines.emplace_back(buf);

        int mobile = 0;
        for (const auto& b : bs) if (b.state.mobile_lid) ++mobile;
        std::snprintf(buf, sizeof buf,
            "%d running a mobile lid. No subduction elsewhere means no porphyry copper.", mobile);
        report->stage_lines.emplace_back(buf);

        int wet = 0;
        for (const auto& b : bs)
            if (b.state.profile.hydrology == hydrological_state::liquid) ++wet;
        std::snprintf(buf, sizeof buf, "Liquid surface water on %d world%s.",
                      wet, wet == 1 ? "" : "s");
        report->stage_lines.emplace_back(buf);

        std::snprintf(buf, sizeof buf, "Life took hold on: %s.", named(life_stage::microbial).c_str());
        report->stage_lines.emplace_back(buf);

        std::snprintf(buf, sizeof buf, "%d world%s accumulated free oxygen.",
                      count_if_stage(life_stage::oxygenated),
                      count_if_stage(life_stage::oxygenated) == 1 ? "" : "s");
        report->stage_lines.emplace_back(buf);

        std::snprintf(buf, sizeof buf, "Land greened on: %s.", named(life_stage::land).c_str());
        report->stage_lines.emplace_back(buf);

        std::snprintf(buf, sizeof buf,
            "Fossil carbon on %d; unweathered surface ore on the dead ones.",
            count_if_stage(life_stage::oxygenated));
        report->stage_lines.emplace_back(buf);

        std::snprintf(buf, sizeof buf,
            "%.0f%% of the accessible homeworld endowment is already gone.",
            static_cast<double>(pp.drawdown * 100.0f));
        report->stage_lines.emplace_back(buf);
    }

    // BL-343/BL-480/BL-741: the prototype levy, seeded ENACTED — since BL-741
    // by EVERY nation over its own jurisdiction (one nation's levy left 42
    // treasuries empty and every state demand channel unfunded). Enactment is
    // a governing-body act, not a player control; each levy is a transfer into
    // its own author's treasury, bounded by that jurisdiction. Runs after
    // generate_nations/generate_corporations. See law.cpp's seeding comment.
    seed_prototype_laws(w);

    // BL-466/BL-623: the CANONICAL whole-world province partition. The
    // homeworld was partitioned before its roads (BL-623, provinces before
    // roads — see that call above for the ordering rationale); Selene and the
    // asteroids were authored after it, so the partition is rebuilt here, LAST,
    // once every body's tiles exist. For the bodies the early call covered the
    // rebuild is byte-identical (the fill reads no road data and none of its
    // other inputs changed; anchor foundings are seed-skipped by their
    // `province_anchor` flag), so every anchor founded above still anchors the
    // same province. On every other body the partition runs here for the first
    // and only time. Same XOR offset — it IS the same partition; no RNG stream
    // is consumed, so this call perturbs nothing above it. See province.hpp.
    build_province_partition(w, params.seed ^ 0x50524F56u);

    // BL-569: seed the province holder — since BL-611 from each land
    // province's ANCHOR centre's nation (plurality is the no-centre
    // fallback), now that the partition, every tile's nation assignment, and
    // the anchors all exist. `run_battles` moves entries thereafter; the seed
    // here is the only generation-time write.
    seed_province_holders(w);

    // BL-571: nation garrisons — capital plus grudge-border provinces, sized
    // off each nation's treasury. Runs LAST of the three, because it reads
    // both `w.provinces` (just built) and `province_holder_for` (just
    // seeded) for the border-province test. See nation_generation.hpp.
    seed_nation_garrisons(w);

    bump(12);

    // --- The generation budget, reported (BL-754) ---------------------------
    //
    // Three destinations, and they are deliberately different. (1) The
    // progress sink, ALWAYS — that is what lets the app print its own budget
    // on its own generating screen, which is the half of BL-754 that was
    // still owed. (2) The fixture, when a caller asked for one. (3) One line
    // to stderr, gated on the fixture: stderr rather than stdout so a harness
    // parsing its own stdout is unaffected, and gated so the harness tier's
    // output stays exactly as it was when the app started publishing too.
    {
        const gen_clock::time_point t_world_end = gen_clock::now();
        const int64_t ms_total      = ms_between(t_world_begin, t_world_end);
        const int64_t ms_before     = ms_between(t_world_begin, t_settlement_begin);
        const int64_t ms_settlement = ms_between(t_settlement_begin, t_settlement_end);
        const int64_t ms_era        = ms_between(t_settlement_end, t_era_end);
        const int64_t ms_after      = ms_between(t_era_end, t_world_end);

        // Write-only tap. `budget_ready` is released last, so a renderer that
        // acquire-loads it sees all five values or none of them.
        if (progress != nullptr)
        {
            progress->ms_world_total.store(ms_total, std::memory_order_relaxed);
            progress->ms_before_settlement.store(ms_before, std::memory_order_relaxed);
            progress->ms_settlement.store(ms_settlement, std::memory_order_relaxed);
            progress->ms_era.store(ms_era, std::memory_order_relaxed);
            progress->ms_after_era.store(ms_after, std::memory_order_relaxed);
            progress->budget_ready.store(true, std::memory_order_release);
        }

        if (fixture != nullptr)
        {
            fixture->ms_world_total       = ms_total;
            fixture->ms_before_settlement = ms_before;
            fixture->ms_settlement        = ms_settlement;
            fixture->ms_era               = ms_era;
            fixture->ms_after_era         = ms_after;

            std::fprintf(stderr,
                         "[gen budget] total %lld ms  (pre-settlement %lld, settlement %lld, "
                         "era-1 %lld, post-era %lld)  epoch=%lld ancient=%d industrial=%d\n",
                         static_cast<long long>(ms_total),
                         static_cast<long long>(ms_before),
                         static_cast<long long>(ms_settlement),
                         static_cast<long long>(ms_era),
                         static_cast<long long>(ms_after),
                         static_cast<long long>(params.epoch_year),
                         params.prehistory_years,
                         era_minus_one_has_industrial_span(params) ? params.industrial_years : 0);
        }
    }

    // BL-977: the record the landscape search's roster axis regenerates
    // specialists from (world.hpp § gen_settlement). Its `history` was moved
    // into the ladder above; regions, charter and the industrial median — all
    // `generate_corporations` reads — are intact. Set whether or not a report
    // was requested, so the harness tier and the app hand the search one thing.
    w.gen_settlement = std::make_shared<const settlement_state>(kepler_settlement);

    return w;
}
