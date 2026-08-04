#include "hard_coded_world.hpp"

#include "continents.hpp"
#include "corporation_generation.hpp"
#include "creeds.hpp"
#include "history_ladder.hpp"
#include "nation_generation.hpp"
#include "orbital_system.hpp"
#include "population_generation.hpp"
#include "road_generation.hpp"
#include "settlement.hpp"
#include "tile_generation.hpp"
#include "river_generation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <iterator>
#include <map>
#include <random>
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
        if (w.tiles.at(tile_ids[i]).composition == terrain_composition::ocean)
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

} // namespace

world make_hard_coded_world(world_params params, generation_report* report,
                            const world_gen_config& gen_cfg)
{
    world w;

    // The resource-abundance multiplier every body's deposit pass is scaled by.
    // At the default `standard` tier this is 1.0f, so a default-params world is
    // bit-identical to the pre-BL-114 generation.
    const float deposit_scalar = deposit_scalar_for(params.abundance, gen_cfg);

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
    auto plan = [&](int proto_index, int gw, int gh, std::vector<float>& bias_out,
                    std::vector<uint8_t>* convergent_out = nullptr) {
        body_inputs in = prototype_body(proto_index);
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
                in.name, st, run_planetology(in, undrawn, body_seed), std::move(cs) });
        }
        return st;
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
        .name                                 = "Helios",
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
        .name                                 = "Cinder",
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
    const planetology_state cinder_pl = plan(0, 180, 84, cinder_bias);
    generate_body_tiles(w, cinder, 180, 84, cinder_pl.profile,
        /*seed=*/params.seed ^ 0xC1D0001u, deposit_scalar, &cinder_pl, nullptr, &cinder_bias);

    // -----------------------------------------------------------------------
    // Kepler — temperate home planet (Earth analogue, ~1.0 AU)
    // 180×84 tile grid. Full climate gradient, 60% ocean, grassland and forest
    // belts. Two installations and a market authored after tile generation.
    // -----------------------------------------------------------------------

    const entity_id kepler = w.create_entity();
    w.bodies[kepler] = body_component{
        .name                                 = "Kepler",
        .type                                 = body_type::planet,
        .parent                               = null_entity,
        // The homeworld orbits where its star can keep it wet, so this is derived
        // rather than authored (a dimmer star pulls the homeworld inward).
        .orbital_radius_au                    = rw.home_orbit_au,
        .orbital_angle_rad                    = 1.05f,
        .orbital_angular_velocity_rad_per_day = kepler_angular_velocity(rw.home_orbit_au),
        .grid_width                           = 180,
        .grid_height                          = 84,
    };

    // Kepler is the corporation's home planet — the game opens on its surface.
    w.home_body = kepler;

    // The homeworld. Its inputs sit inside the corridor where the identical
    // unmodified chain succeeds — the guarantee is on the INPUTS, never on the
    // gates (PLANETOLOGY.md § The homeworld rule). Every gate still runs, still
    // records its margin, and still writes its line.
    std::vector<float> kepler_bias;
    std::vector<uint8_t> kepler_convergent; // Pass 5 seeds mountain ranges along these
    const planetology_state kepler_pl = plan(1, 180, 84, kepler_bias, &kepler_convergent);
    // A non-null generation_record is requested here so the river pass below can read the
    // Pass-1 heightmap it captures; this is a pure capture (TILE_GENERATION.md § Generation
    // history hook) and does not perturb the deterministic tile surface itself.
    generation_record kepler_record;

    // Reject-and-reroll acceptance gate (BL-276), two bars — the hybrid Ben
    // chose (2026-08-03: "about 90% likely ... never impossible to try"):
    //
    //   ARENA (>= 300 tiles): a playable Mediterranean. Three attempts only —
    //     with the rift-basin mechanism (continents.cpp) putting a single seed
    //     at ~58%, three tries land ~90% of worlds. Deliberately NOT retried to
    //     exhaustion: the ~1-in-10 worlds without an arena are the wanted
    //     hard-Rome tail.
    //   FLOOR (>= 30 tiles): some enclosed sea, so trying is never impossible.
    //     All six attempts may serve it; ~98% of seeds pass on attempt 0.
    //
    // Attempt 0 is the unfolded seed, so worlds already qualifying are
    // untouched; on full exhaustion attempt 0 is kept honestly rather than
    // clamping anything (the resolve_preferences idiom). Each probe is one
    // scratch tile generation (~tens of ms).
    uint32_t kepler_tile_seed = params.seed ^ 0xE471001u;
    {
        uint32_t floor_seed = 0;
        bool have_floor = false, have_arena = false;
        for (int attempt = 0; attempt < 6; ++attempt)
        {
            const uint32_t candidate = (params.seed ^ 0xE471001u) ^ (static_cast<uint32_t>(attempt) * 0x9E3779B9u);
            world scratch;
            const entity_id probe = scratch.create_entity();
            const auto probe_tiles = generate_body_tiles(scratch, probe, 180, 84, kepler_pl.profile,
                candidate, deposit_scalar, &kepler_pl, nullptr, &kepler_bias, &kepler_convergent);
            const int sea = largest_enclosed_sea(scratch, probe_tiles, 180, 84);
            if (attempt < 3 && sea >= 300)
            {
                kepler_tile_seed = candidate;
                have_arena = true;
                break;
            }
            if (!have_floor && sea >= 30) { floor_seed = candidate; have_floor = true; }
            // Past the arena window with the floor already met: done searching.
            if (attempt >= 2 && have_floor) break;
        }
        if (!have_arena && have_floor) kepler_tile_seed = floor_seed;
    }

    auto kepler_tiles = generate_body_tiles(w, kepler, 180, 84, kepler_pl.profile,
        kepler_tile_seed, deposit_scalar, &kepler_pl, &kepler_record, &kepler_bias, &kepler_convergent);

    // Rivers (BL-170) — sibling pass (BL-051 convention) over the same heightmap Pass 2
    // already thresholded; needs Kepler's ocean placement done, so it runs after
    // generate_body_tiles returns rather than being spliced into the six-pass core.
    generate_rivers(w, kepler_tiles, 180, 84, kepler_record.height, /*seed=*/params.seed ^ 0x52490001u);

    // Kepler is the only body with a political layer in the prototype; its nation
    // count is derived, not authored. Selene/Cinder/Pallas stay unclaimed.
    // See docs/generation/NATION_GENERATION.md.
    // Population centres must be placed before generate_nations so that Pass 6
    // (substrate density) can reference them during nation territory assignment.
    generate_population_centres(w, kepler, /*seed=*/params.seed ^ 0x70701001u);

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
        run_history_ladder(kepler_pl, w, kepler_tiles, 180, 84,
                           /*seed=*/params.seed ^ 0x5A11EDu);

    // Creeds (BL-235, docs/lore/CREEDS.md): one pantheon per cradle-culture,
    // each in its own generated tongue. Runs BEFORE generate_nations because
    // its tribal-conflict stage DRIVES the map the same way the ladder does:
    // won wars weld cradles together and lower fragmentation_q before
    // nation_params_from_ladder reads it into the seed budget.
    creed_state kepler_creeds =
        run_creeds(kepler_pl, kepler_hist, w, kepler_tiles, 180, 84,
                   /*seed=*/params.seed ^ 0xC4EED5u);
    record_tribal_conflict(kepler_creeds, kepler_hist, /*seed=*/params.seed ^ 0xC4EED5u);

    // Settlement & industrialisation (BL-218, docs/lore/HISTORY.md Stages 3-4).
    // Provinces are placed BEFORE the political map and become its seeds, so
    // nation borders grow out of ground people settled rather than out of a
    // random draw — "seeding changes, expansion does not". Each province
    // inherits its nearest cradle's culture, so the pantheons the creeds pass
    // raised are now mapped onto specific ground and specific ancient deposits.
    settlement_state kepler_settlement;
    nation_params kepler_np =
        nation_params_from_ladder(kepler_hist, nation_params{ .min_seed_separation = 5 });
    {
        // Aim the province budget at the political pass's own seed budget, so
        // the two agree by construction rather than by a tuned constant.
        int kepler_land = 0;
        for (const entity_id tid : kepler_tiles)
        {
            const auto it = w.tiles.find(tid);
            if (it != w.tiles.end() && it->second.composition != terrain_composition::ocean)
                ++kepler_land;
        }
        const int budget = std::max(1, kepler_land / std::max(1, kepler_np.land_tiles_per_seed));

        kepler_settlement = run_settlement(kepler_pl, kepler_hist, kepler_creeds, w,
                                           kepler_tiles, 180, 84, budget,
                                           /*seed=*/params.seed ^ 0x5E77EDu);
        kepler_np.seed_tiles = settlement_seed_tiles(kepler_settlement);
    }

    const std::vector<entity_id> kepler_nations =
        generate_nations(w, kepler, kepler_tiles, 180, 84, kepler_np,
                         /*seed=*/params.seed ^ 0x4A71012u);

    // The political axes are OUTPUTS now (BL-218): expansionism from the
    // border-contest integral, economic_focus from the resource class of the
    // provinces settled during industrialisation, ideology from
    // industrialisation timing against neighbours. This overwrites the random
    // Pass 4 draw, which stays as the fallback for bodies with no settlement.
    derive_national_character(kepler_settlement, kepler_creeds, w,
                              kepler_nations, kepler_tiles, 180, 84);

    // The historical ruptures: collapse, war and revolution as transforms on
    // territory, character and abundance — and, where a war is won, the
    // victor's gods travel with the border and part of the loser's record is
    // destroyed rather than merely contradicted.
    resolve_historical_ruptures(kepler_settlement, kepler_creeds, w,
                                kepler_nations, kepler_tiles, 180, 84,
                                /*seed=*/params.seed ^ 0x80174E5u);

    // Stages 1-2 can only be written now: the Charter Act names a nation and
    // the border accord counts them, and neither existed a moment ago. The
    // lines then merge into Kepler's biography, which the History ledger reads.
    record_institutional_history(kepler_hist, w, kepler, kepler_tiles, 180);

    // Globalisation closes the generated story: the common tongue line is the
    // hinge from the creeds' native record to the campaign epoch — rendered in
    // the player's language (English for now; Ben, 2026-07-31). The creed
    // lines then merge into the same ladder history the report reads.
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
            if (be.name != "Kepler") continue;
            be.state.history.insert(be.state.history.end(),
                                    kepler_hist.history.begin(), kepler_hist.history.end());
            // The province set / checkpoints / lacunae travel with the report
            // (its `history` is already merged above and stays empty here,
            // matching the continents convention).
            be.settlement = kepler_settlement;
            std::stable_sort(be.state.history.begin(), be.state.history.end(),
                [](const history_event& a, const history_event& b)
                { return a.years_before_epoch > b.years_before_epoch; });
            break;
        }
    }

    // Road network (BL-146): stamp each nation's road lattice onto tile.road_level,
    // after nations + population centres exist. Deterministic; no seed of its own —
    // a pure function of the generated tiles/nations/centres.
    generate_roads(w, kepler);

    // Attach installations to the first two land tiles found in raster order.
    {
        auto land = first_land_tiles(kepler_tiles, w, 180, 84, 2);

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
    // uncorrelated with the nation/corp streams). Fuller co-generation is BL-132.
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

        std::map<entity_id, float> concentration; // std::map → ascending id (deterministic)
        for (const auto& [nid, nc] : w.nations)
        {
            float sum = 0.0f;
            for (const resource_type rt : tradeable_raws)
                sum += nc.resource_abundance[static_cast<std::size_t>(rt)];
            const float tiles = static_cast<float>(std::max<std::size_t>(1, nc.tiles.size()));
            concentration[nid] = sum / tiles;
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

        // Seed markets in ascending centre-id order for deterministic market ids.
        std::vector<entity_id> centre_ids;
        centre_ids.reserve(w.population_centres.size());
        for (const auto& [cid, pcc] : w.population_centres)
            centre_ids.push_back(cid);
        std::sort(centre_ids.begin(), centre_ids.end());

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
            if (pcc.scale < gate)
                continue; // folds into a neighbouring market (routed by market_for_tile)

            market_component mc = kepler_market_template;
            mc.centre_tile = tile_it->second;
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
            constexpr int   gw = 180, gh = 84;
            const float half_diag = std::sqrt(static_cast<float>((gw / 2) * (gw / 2) + gh * gh));

            for (const endemic_good& e : kepler_pl.endemics)
            {
                const std::size_t ri = static_cast<std::size_t>(e.good);

                // Where it actually grows. Gathered once per good, in ascending
                // tile id, so the scan order is deterministic.
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

    // Corporations: 6–10 actors registered in the generated nations, including
    // the player's (which sets w.player_entity). Runs after the nations exist and
    // after the pre-authored Kepler installations are in w.buildings, so corporate
    // asset placement collision-avoids those tiles. See CORPORATION_GENERATION.md.
    // BL-219: the settlement record is handed over so each corp's focus is a
    // consequence of the province it anchors to — its ancient endowment plus
    // how early that province industrialised — rather than a table lookup.
    generate_corporations(w, corporation_params{ .corporation_count = gen_cfg.corporation_count },
        /*seed=*/params.seed ^ 0x4A71012u, &kepler_settlement);

    // Player unit stub on Kepler.
    const entity_id kepler_unit = w.create_entity();
    w.units[kepler_unit] = unit_component{
        .body  = kepler,
        .owner = w.player_entity,
        .count = 50,
    };

    // -----------------------------------------------------------------------
    // Selene — Kepler's moon (Luna analogue)
    // 90×42 tile grid (the planet ratio at half scale). Airless regolith surface,
    // icy at the polar rows only, crater-dominated landforms.
    // -----------------------------------------------------------------------

    const entity_id selene = w.create_entity();
    w.bodies[selene] = body_component{
        .name                                 = "Selene",
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
    const planetology_state selene_pl = plan(2, 90, 42, selene_bias);
    generate_body_tiles(w, selene, 90, 42, selene_pl.profile,
        /*seed=*/params.seed ^ 0x5E1E001u, deposit_scalar, &selene_pl, nullptr, &selene_bias);

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
        const char* name;
        float       radius_au;
        float       angle_rad;
        uint32_t    seed;
    };
    constexpr notable_asteroid notables[] = {
        { "Pallas", 3.05f, 4.6f, 0x9A11A5u },
    };

    for (const notable_asteroid& a : notables)
    {
        const entity_id id = w.create_entity();
        w.bodies[id] = body_component{
            .name                                 = a.name,
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
        const planetology_state ast_pl = plan(3, 30, 14, ast_bias);
        generate_body_tiles(w, id, 30, 14, ast_pl.profile,
            params.seed ^ a.seed, deposit_scalar, &ast_pl, nullptr, &ast_bias);
    }

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

    return w;
}
