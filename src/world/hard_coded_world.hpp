#pragma once

#include "continents.hpp"
#include "planetology.hpp"
#include "settlement.hpp"
#include "world.hpp"
#include "world_gen_config.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

/// Resource-abundance tier for a generated world. Earth-like is the ceiling: no
/// tier exceeds the baseline (GENERATION_STRATEGY.md § The resource ceiling), so
/// `standard` (1.0×) is the richest and the leaner tiers step *down* from it.
/// Maps to the `deposit_scalar` threaded into generate_body_tiles.
enum class abundance_level : uint8_t { sparse, lean, standard };

/// The reproducible world descriptor: a master seed plus the high-level generation
/// knobs. Same seed + same params → an identical world on a given binary. Threaded
/// through make_hard_coded_world and edited on the main-menu New World setup (BL-114);
/// it lives in the app, not the `world` struct, so it stays off the serialisation seam.
struct world_params
{
    uint32_t        seed       = 0;                         ///< Master seed, XOR-folded into each per-body seed. 0 reproduces the legacy world.
    abundance_level abundance  = abundance_level::standard; ///< Deposit-density tier (standard = earth-like ceiling).

    /// Calendar year the generated world BEGINS at (BL-271, the Era -1 sandbox).
    /// 1960 (default) is the campaign epoch: the full pre-computed history runs
    /// (industrialisation, ruptures, globalisation). Any year below 1700 stops
    /// the settlement pass at that year instead: provinces founded later do not
    /// exist yet, no furnace ever lights, no rupture is pre-resolved — that
    /// history is the year-tick sim's to produce — and province demography is
    /// seeded at founding and grown to the start year. Sandbox-only: the
    /// 1960-era economy scaffolding (corps, markets, roads) still generates
    /// underneath and is out of frame; gating it is BL-271's build.
    /// **0 CE since 2026-08-12 (Ben, NR-177).** The ancient refocus makes the
    /// antiquity branch above the DEFAULT rather than a sandbox opt-in: the
    /// campaign now opens at 0 CE, so the settlement pass stops there and the
    /// year-tick sim produces the run-up. The 1960 arc is not deleted — pass
    /// `epoch_year = 1960` and it still runs, which is what keeps the parked
    /// space work (era/space) buildable.
    int64_t         epoch_year = 0;

    /// Years of year-tick pre-history the antiquity branch simulates before the
    /// epoch (Ben's figure: 400 at 4 years a tick). SCOPE KNOB, not a tuning
    /// dial — set it to 0 and the pass is skipped entirely.
    ///
    /// It exists because wiring the sim into generation added ~23 s to EVERY
    /// world any caller builds, and the headless harnesses build several each.
    /// Most of them (tile generation, placement audits, economy arithmetic) do
    /// not test the era at all and were paying its whole cost, several past
    /// their ctest timeouts. A harness that does not test the era sets 0; one
    /// that does (era_world_harness, stepped_clock_harness) leaves it alone.
    /// Determinism is untouched — the value is part of the params, so the same
    /// params still give the same world.
    int             prehistory_years = 400;
    int             body_count = 0;                         ///< Reserved — the body-count knob is PHASED to a follow-on (bodies are still hard-coded profiles).
    // Note: there is no nation-count knob. The number of nations on the home body is a
    // *consequence* of its habitable land area and the minimum-viable-territory floor
    // (nation_params in world/nation_generation.hpp), not a value the player pre-sets.

    /// What the player expressed on the New World wizard (BL-167). Preferences,
    /// not parameters: make_hard_coded_world resolves these against the seed —
    /// rejecting and rerolling until the homeworld clears the strict Earth-like
    /// floor — and the resolved values land in the generation_report.
    world_preferences preferences{};
};

/// Coarse progress sink for a caller drawing a loading screen (Ben, 2026-08-12).
///
/// WHY THIS EXISTS. Generation takes ~25 s on the 312x145 grid with the
/// year-tick sim wired in, and `start_new_game` is called from inside an ImGui
/// frame — so the UI never pumps and Windows marks the process "not responding".
/// It looks exactly like a crash, and that is how it was first reported.
///
/// Deliberately atomic and deliberately dumb: the caller runs generation on a
/// worker thread and reads these from the render thread every frame. No mutex,
/// no callback, no re-entrant rendering. `stage` is monotonic and never exceeds
/// `stage_count`, so a reader can always form a fraction.
struct generation_progress
{
    std::atomic<int> stage{0};       ///< Passes completed so far.
    std::atomic<int> stage_count{1}; ///< Total passes this run will report.
    /// Index into `generation_stage_labels`, for the line under the bar.
    std::atomic<int> label{0};

    /// Progress WITHIN the current pass, for the loading screen's second bar.
    /// The passes are wildly uneven — the ancient era is ~23 s of a ~25 s
    /// generation — so the outer bar alone freezes on it for the whole wait.
    /// A pass that can report honestly (the year loop) sets `sub_total` to its
    /// span before starting and back to 0 when done; the renderer draws the
    /// inner bar only while `sub_total > 0`. Same contract as above: worker
    /// writes, renderer reads, relaxed atomics, no mutex.
    std::atomic<int> sub_progress{0};
    std::atomic<int> sub_total{0};
};

/// Human labels for `generation_progress::label`, in pass order.
inline const char* const generation_stage_labels[] = {
    "Preparing",            // 0
    "Forming the system",   // 1
    "Settling chemistry",   // 2
    "Drifting continents",  // 3
    "Raising terrain",      // 4
    "Carving rivers",       // 5
    "Seeding peoples",      // 6
    "Founding provinces",   // 7
    "Running the ancient era", // 8 — the long one
    "Drawing borders",      // 9
    "Laying roads",         // 10
    "Placing companies",    // 11
    "Finishing",            // 12
};
inline constexpr int generation_stage_label_count =
    static_cast<int>(sizeof(generation_stage_labels) / sizeof(generation_stage_labels[0]));

/// What the generation pass recorded about each body, for the staged generation
/// screen and the planet report.
///
/// This is a PRESENTATION artefact, not simulation state: it is filled during
/// make_hard_coded_world and handed to the app, which reveals it stage by stage.
/// It never enters the `world` struct, so it stays off the serialisation seam —
/// the same reasoning that keeps world_params in the app (BL-114).
struct generation_report
{
    struct body_entry
    {
        std::string       name;

        /// The world entity this entry describes — the IDENTITY key (BL-257).
        /// Every consumer that used to match a report entry to a world body by
        /// comparing `name` matches on this instead: names are generated and
        /// display-only, so a name test is a display string standing in for an
        /// identity. `null_entity` only on a report built without a world
        /// (none today; the wizard's preview uses `generate_body_previews`).
        entity_id         id = null_entity;

        /// True for the homeworld — the same flag `body_inputs::is_homeworld`
        /// carries, copied through so a consumer holding only a report (a
        /// harness comparing two generations, with no `world` in hand) can ask
        /// "which entry is the home body" without a name test.
        bool              is_homeworld = false;

        planetology_state state;

        /// The same body with the industrial drawdown dialled to zero — what the
        /// chain FORMED, before a prior era took the accessible half of it. Kept
        /// alongside the real state so the History ledger can redraw the wizard's
        /// formed-against-left chart, which otherwise has no "before" to point at
        /// (the wizard computes it from a live second preview; a loaded campaign
        /// has no preview to consult). Drawdown consumes no randomness, so this is
        /// the same world minus its industrial history — not a second roll.
        planetology_state undrawn;

        /// What the Continents/Drift pass computed for this body (BL-226). Its
        /// `history` is empty here — those lines were moved into `state.history`
        /// at generation, where the biography reads them; what is kept is the
        /// plate set and the per-tile `plate_id`, which nothing else records.
        /// The Continent lens is the consumer. Presentation data, like the rest
        /// of this struct: it never enters `world`, so it stays off the
        /// serialisation seam.
        continent_state continents;

        /// What the settlement/industrialisation pass computed for this body
        /// (BL-218/BL-219). Its `history` is empty here for the same reason
        /// `continents.history` is — those lines were merged into
        /// `state.history` at generation. What is kept is the province set (who
        /// settled where, whose gods they keep, which ancient deposits they sit
        /// on, when their furnaces lit), the rupture `checkpoints`, and the
        /// `lacunae` count — the holes the wars left in the record. Nothing
        /// else records any of it. Presentation data, like the rest of this
        /// struct: it never enters `world`, so it stays off the serialisation
        /// seam.
        settlement_state settlement;
    };

    world_preferences       preferences{}; ///< What the player asked for.
    planetology_params      params{};      ///< What the seed actually rolled within it.
    float                   home_orbit_au = 1.0f; ///< Derived from the star, not authored.
    uint32_t                attempts      = 1;    ///< Viability draws consumed (reroll cost).
    std::vector<body_entry> bodies;

    /// Per-stage one-line summary of what the chain did across the whole system,
    /// indexed by chain_stage. This is what the staged generation screen reveals.
    std::vector<std::string> stage_lines;

    // --- The pre-epoch era (BL-271 wired into generation, 2026-08-12) --------
    //
    // What the year-tick sim actually produced in the run-up to the campaign.
    // Reported rather than merely run, because the acceptance test for that pass
    // is behavioural — Ben asked for "turmoil at the beginning of the campaign
    // ... some losing / winning bodies" — and a pass that silently generated a
    // PEACEFUL world would look identical to one that never ran at all. Which is
    // exactly how this sim sat unwired: it existed, it was tested, and nothing
    // in the campaign path called it.
    //
    // Zero on a 1960-era world, where the antiquity branch does not run.
    int64_t prehistory_years     = 0; ///< Years simulated before the epoch.
    int64_t prehistory_battles   = 0; ///< Battles fought in that span.
    int64_t prehistory_conquests = 0; ///< Provinces that changed hands.
    int64_t prehistory_foundings = 0; ///< Provinces founded by the sim.
};

/// Construct and return a world populated with the prototype's authored bodies.
///
/// Bodies (all orbiting the star Helios):
///   Cinder — hot inner planet, 180×84 procedural tile grid.
///   Kepler — temperate rocky planet, 180×84 procedural tile grid; the
///            corporation's home body, with two surface installations and a
///            market.
///   Selene — Kepler's moon, 90×42 procedural tile grid.
///
/// Grids follow a ~9:5 width:height ratio (see PLANETARY.md). All values are
/// hand-authored for prototype testing. Replace this function with a
/// data-driven loader when scripted body definitions are added.
///
/// @param params The world descriptor — seed + generation knobs. Defaulted so the
///               legacy call `make_hard_coded_world()` reproduces the original world.
/// @param report Optional out-param; when non-null, receives the per-body Planetology
///               results and the per-stage summaries the generation screen reveals.
///               The common path passes nullptr and pays nothing.
/// @param gen_cfg Balance values authored in scripts/world_gen.lua (BL-236). Defaulted
///               so a headless caller that never touches Lua reproduces the same world.
/// @return A fully populated world ready to drive the simulation.
/// @param progress Optional coarse progress sink for a caller drawing a loading
///               screen from another thread. Null for every headless caller.
world make_hard_coded_world(world_params params = {}, generation_report* report = nullptr,
                            const world_gen_config& gen_cfg = {},
                            generation_progress* progress = nullptr);

/// The homeworld's tile grid dimensions — one authority the build and the
/// wizard preview both read.
///
/// **312×145 = 45,240 tiles since 2026-08-12 (Ben), three times the area of the
/// old 180×84 = 15,120.** The aspect ratio is held at ~2.15:1 so the cylinder
/// still maps a plausible globe: columns wrap as the equator, rows do not.
///
/// The scale-up is only half a change on its own. Travel time used to be
/// `1 / distance_in_AU`, which is zero for two markets on the same body — so
/// every intra-body convoy arrived in exactly one econ tick regardless of
/// distance, and tripling the map would have made trade RELATIVELY faster
/// rather than slower. The distance→time model in logistics.hpp
/// (`body_km_per_tile`, `convoy_travel_ticks`) landed with this constant for
/// that reason; changing one without the other is a regression.
inline constexpr int home_grid_width  = 312;
inline constexpr int home_grid_height = 145;

/// Generate ONLY the homeworld tile surface into @p w (a scratch world), exactly
/// as make_hard_coded_world builds Kepler's: same resolved preferences, same
/// planetology chain, same Continents pass, same BL-276 acceptance gate, same
/// seed formulas — the gate is literally the same function. The New World
/// wizard's preview pane calls this so the map a player rerolls IS the map
/// "Begin" hands them. Rivers and the political layer (sibling passes the
/// preview does not show) are skipped. Returns raster-order tile ids.
std::vector<entity_id> generate_home_surface_preview(world& w, entity_id body,
                                                     const world_params& params,
                                                     const world_gen_config& gen_cfg = {});
