#pragma once

#include "continents.hpp"
#include "era_timelapse.hpp" // the recorded Era -1 ownership replay (NR-733)
#include "planetology.hpp"
#include "settlement.hpp"
#include "world.hpp"
#include "world_gen_config.hpp"

#include <array>
#include <atomic>

// Forward-declared rather than included (BL-321): this header is included very
// widely, and the works table is only ever passed through it by pointer.
class works_registry;

// Same reason, same shape (BL-462): the Era -1 capture is only ever passed
// through here by pointer, and its own header pulls in history_sim.hpp,
// creeds.hpp and the terrain arrays — none of which this header's several
// hundred includers should have to pay for. Include world/era_minus_one.hpp to
// use one.
struct era_minus_one_fixture;

#include <cstddef>
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

    /// A SECOND SEED FOR THE ANCIENT ERA ALONE, so the same ground can be
    /// played through twice and come out differently (Ben, 2026-09-09:
    /// "reroll should produce differences regardless").
    ///
    /// THE PROBLEM IT SOLVES. `era_minus_one_sim_seed` folds `params.seed`, so
    /// the era was a pure function of the master seed. The wizard's history
    /// round could therefore re-RUN the pass on Reroll but could not vary it —
    /// pressing Reroll reproduced the same history exactly. Folding the round's
    /// roll into `params.seed` instead was the obvious repair and is the wrong
    /// one: `seed` drives the planetology rounds ABOVE this one, so it would
    /// re-draw the star, the world and its surface, and rounds-are-causal says
    /// a round may only invalidate the rounds BELOW it.
    ///
    /// So the era gets a seed of its own. Rerolling the history round moves
    /// this and nothing else: the same planet, a different four thousand years.
    ///
    /// DEFAULT 0 CHANGES NO WORLD. The fold below is an addition, so at 0 the
    /// derivation is `params.seed ^ 0x415C1E17u` — exactly what it has always
    /// been. Every existing seed, golden and fixture is untouched.
    uint32_t        era_seed   = 0;
    abundance_level abundance  = abundance_level::standard; ///< Deposit-density tier (standard = earth-like ceiling).

    /// Calendar year the generated world BEGINS at (BL-271, the Era -1 sandbox).
    /// 1960 (default) is the campaign epoch: the full pre-computed history runs
    /// (industrialisation, ruptures, globalisation). Any year below 1700 stops
    /// the settlement pass at that year instead: regions founded later do not
    /// exist yet, no furnace ever lights, no rupture is pre-resolved — that
    /// history is the year-tick sim's to produce — and region demography is
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

    /// Years of INDUSTRIAL span the sim plays after the boundary year, on an
    /// epoch that has one. `prehistory_years` is the ANCIENT span before it,
    /// so a 1960 arc at the defaults runs 1160 -> 1560 -> 1960. Zero means no
    /// industrial span and the run is single-span, as an ancient epoch is.
    int             industrial_years = 400;

    /// THE EMPIRES ROUND'S OWN STOP YEAR (BL-906), for a SINGLE-span run only
    /// (`era_minus_one_has_industrial_span(params) == false`) — independent of
    /// `epoch_year`.
    ///
    /// THE BUG THIS CLOSES. `era_minus_one_sim_params` used to set
    /// `hp.stop_year = params.epoch_year` unconditionally, so an ancient-epoch
    /// world (the campaign's `epoch_year == 0`) ran the Empires round 400 BCE
    /// -> 0 CE — 400 years — where `docs/generation/CIVILISATION.md` § "The
    /// closure of the Empire era" specifies 400 BCE -> 1200 CE, 1,600 years.
    /// The stop year was coupled to a field that means something else
    /// entirely: the CAMPAIGN's calendar start, not this round's own close.
    ///
    /// 1200 is Ben's ruling (2026-09-11) and the doc's own "1200 is unmoved" —
    /// it is a fixed year the Empires round closes ON, not a span length, so it
    /// does not move if `prehistory_years` (the START side of the span) is
    /// ever retuned.
    ///
    /// TWO-SPAN EPOCHS (>= 1700) DO NOT READ THIS FIELD. There the industrial
    /// arc's own stop is `epoch_year` (the 1960 arc, untouched) and the ancient
    /// half beneath it stops at `boundary_year`, both unaffected by BL-906.
    ///
    /// Default 1200 matches the doc for every existing single-span caller —
    /// nothing opts in, the shipped world simply runs its documented span.
    int64_t         empires_stop_year = 1200;

    /// BL-931 — RUN THE EXPLORATION SPAN, 1200 -> `exploration_stop_year`,
    /// on the SAME `history_sim` engine, immediately after the single-span
    /// Empires round closes (EXPLORATION.md sec The engine is shared).
    ///
    /// OFF BY DEFAULT, deliberately, unlike the Empires round itself. Wiring
    /// this on unconditionally would move `region::nation` (read by
    /// `derive_national_character` right after the era block) from the 1200
    /// CE political map to whatever the exploration span leaves at
    /// `exploration_stop_year` — every generation golden and every harness
    /// that checks post-generation political state would move with it. That
    /// is real work this item does not do (verifying the downstream
    /// consequences and re-baselining what moves), so the capability lands
    /// real and tested but does not yet change what a caller who asks for
    /// nothing new gets. A caller that wants the span opts in here.
    ///
    /// Only read on a SINGLE-span Empires run (`!era_minus_one_has_industrial
    /// _span(params)`) — a two-span (>= 1700) epoch's own industrial arc
    /// already plays past 1660 on a different calendar mapping, and stacking
    /// this on top of it is a question this item does not answer.
    /// DEFAULT FLIPPED TO TRUE (Ben, 2026-09-13, BL-946, resolving NR-847):
    /// the wizard now shows an Exploration round and it must have something
    /// real to run. This re-baselines `region::nation` to the 1660 CE map for
    /// every generated world -- authorised, not a silent re-bless; see
    /// world_determinism's re-recorded digest in the DEVLOG.
    bool exploration_sim_enabled = true;

    /// The calendar year the Exploration span closes. Default 1660 is
    /// EXPLORATION.md's own span end; exposed as a field on the same footing
    /// as `empires_stop_year` rather than a compile-time constant, for a
    /// harness that wants to bind a shorter span.
    int64_t         exploration_stop_year = 1660;

    /// BL-1040 — RUN THE INDUSTRIALISATION SPAN, `exploration_stop_year` ->
    /// `industrialisation_stop_year`, as its OWN call to the same engine, resumed
    /// from the Exploration span's `exploration_output` (INDUSTRIALISATION.md, the
    /// span paragraphs).
    ///
    /// THE RUN PREDICATE IS "EXPLORATION RAN", NEVER THE EPOCH (Ben,
    /// 2026-09-18): the span runs wherever Exploration runs, whatever
    /// `epoch_year` says, and nowhere else. On the superseded arc (epoch >=
    /// 1700) Exploration is off, so this span is too. The call site nests it
    /// inside the block that ran Exploration, so the predicate is structural
    /// rather than a second reading of the same conditions.
    ///
    /// ON BY DEFAULT (Ben, 2026-09-18, sprint 45 elicitation: "the span runs
    /// on by default at epoch 0"; turned on by BL-1044's re-bless). The span
    /// rewrites `region::nation`, population and treasury to the 1960 map
    /// before population centres are placed, and its industry stockpile is
    /// the charter budget the new-game path spends (`build_stockpile_budget`).
    /// Off, a world is byte-identical to one built before this field existed:
    /// the pre-BL-1044 world, which instruments still build as the LEGACY arc
    /// (with `resume_seeds_corridor_tier` below off too) to keep the BL-1031
    /// pins a live check.
    ///
    /// NOT ON THE SAVE SEAM, on exactly the footing of `exploration_sim_enabled`
    /// and `exploration_stop_year` above: `w_world_params` writes neither of
    /// those, and a scope knob that decides which history generation plays is
    /// not a property a loaded campaign re-reads. A loaded span world needs
    /// nothing from it: generation has run, and what it left (the nations, the
    /// centres, the chartered web) is world state the save already carries.
    bool industrialisation_span_enabled = true;

    /// The calendar year the Industrialisation span closes: the campaign epoch the
    /// span grows the world to (INDUSTRIALISATION.md: "1660 -> 1960 CE, 300
    /// years"). A field on the same footing as `exploration_stop_year`, for a
    /// harness that wants to bind a shorter span.
    int64_t         industrialisation_stop_year = 1960;

    /// BL-1037 — a resumed span's corridors reopen at the RUNG they were
    /// bought to, not the rung their walks earn (`history_sim_params::
    /// resume_seeds_corridor_tier` says why). ON BY DEFAULT with BL-1044's
    /// re-bless. Copied into BOTH resumed spans' params — Exploration's
    /// (`exploration_sim_params`) and so the Industrialisation span's, which is
    /// built on it; the Empires round resumes no corridors. A field here, not
    /// only the struct default, so an instrument can build the LEGACY arc
    /// (span off, this off): the world the BL-1031 pins were taken on.
    /// NOT ON THE SAVE SEAM, on the footing of `industrialisation_span_enabled`.
    bool resume_seeds_corridor_tier = true;

    int             body_count = 0;                        ///< Reserved — the body-count knob is PHASED to a follow-on (bodies are still hard-coded profiles).
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

    // --- The live lapse tap (BL-914) ----------------------------------------
    //
    // A pass round's own record, published while it computes rather than
    // handed over whole once the future lands. The pointer is set by the
    // caller BEFORE `std::async` starts the worker and never reassigned while
    // the worker runs, so no lock is needed on the pointer itself; the object
    // it points at (`era_lapse_tap`) carries its own mutex around the growing
    // vectors, for the reason its own header argues. Null for every generation
    // call that has no wizard watching — `start_new_game`, every harness — so
    // the sim pays nothing beyond one pointer compare when nobody is drawing.
    era_lapse_tap* lapse_tap = nullptr;

    // --- The generation budget, published to the loading screen (BL-754) ----
    //
    // WHY HERE AND NOWHERE ELSE. The same numbers already reach a harness on
    // `era_minus_one_fixture` (see that type for why they may not live on
    // `generation_report` — the report is serialised, and a wall clock is the
    // worst possible thing to put through a save). But the fixture is a heavy
    // capture the app never asks for, and BL-754's remaining half is the APP
    // printing its own budget on its own generating screen. This sink is
    // already the app-to-generation seam, already atomic, already a pure tap
    // with no save presence — so it is the one place the measurement can sit
    // without becoming world state or costing a capture.
    //
    // NOTHING BELOW MAY EVER ENTER A DIGEST, A HASH, OR A BRANCH. These are
    // milliseconds of wall clock: reading one back into generation would make
    // the world non-deterministic by construction. They are WRITE-ONLY from
    // the worker and READ-ONLY from the renderer, exactly like every other
    // field in this struct, and they are reported to a human, never asserted.
    //
    // `budget_ready` is release-stored AFTER the five values, so an
    // acquire-load of it is the renderer's guarantee that all five are filled.
    // Zero until generation finishes; zero for any pass that did not run.
    std::atomic<int64_t> ms_world_total{0};
    std::atomic<int64_t> ms_before_settlement{0};
    std::atomic<int64_t> ms_settlement{0};
    std::atomic<int64_t> ms_era{0};
    std::atomic<int64_t> ms_after_era{0};
    std::atomic<bool>    budget_ready{false};

    // --- The territory carve, live (BL-305) ---------------------------------
    //
    // The two passes that decide the world's POLITICS — the nation carve and
    // corporate asset placement — are the most interesting thing generation
    // does, and until now they happened behind a progress bar. These fields let
    // the loading screen show them: the map fills in the order the BFS actually
    // claims ground, then charter marks land on the finished borders.
    //
    // Same contract as everything above it. A pure TAP: every publish below is
    // write-only, is never read back, consumes no randomness and changes no
    // branch, so a world built with the screen watching is byte-identical to
    // one built headlessly. Every producer takes `generation_progress*`
    // defaulted to null and treats null as "publish nothing".
    //
    // WHY PER-CELL ATOMICS AND NOT A DOUBLE BUFFER. The obvious shape for
    // publishing a growing map is a back buffer plus a flip counter, so the
    // reader always sees one internally-consistent frame. The carve does not
    // need that and would pay ~90 KB of copy per publish for it: each tile is
    // claimed exactly once and never re-claimed, so a cell only ever goes from
    // unclaimed to claimed. A renderer that observes half of one publish sees a
    // map with slightly fewer tiles filled — a valid EARLIER FRAME of the very
    // animation being shown, not a torn read. The one place indices genuinely
    // change under the reader (the merge + compaction in Pass 2c) is restated
    // as a single whole-map pass, so the recolour reads as "the borders
    // settled". The counter-guarded array below (asset marks) DOES need
    // ordering, and uses the release/acquire pair on its count.

    /// The homeworld grid the carve is published over. static_assert'd against
    /// home_grid_width/height below — begin_carve publishes a zero grid rather
    /// than truncating if a bigger body ever arrives.
    static constexpr int carve_capacity = 261 * 121;

    /// Asset markers plotted on the carve. Each corp stakes a small cluster, so
    /// this bounds the ~8 generated charters at a comfortable ceiling. Overflow
    /// is DROPPED, not wrapped: a missing marker is a cosmetic loss, a wrapped
    /// one would make the map lie.
    static constexpr int max_asset_marks = 512;

    /// Corporation ledger rows. `corporation_count` is 8; background firms
    /// (BL-365) generate outside make_hard_coded_world and are not published.
    static constexpr int max_corp_slots = 64;

    std::atomic<int> grid_w{0}; ///< 0 until begin_carve; 0 also means "no carve to draw".
    std::atomic<int> grid_h{0};
    std::atomic<int> nation_count{0}; ///< Seeds placed (pre-merge), then survivors after Pass 2c.

    /// Entity id of nation index 0, published once the nations are actually
    /// created — which is AFTER the carve is drawn, because the carve works in
    /// indices and the entities do not exist until the borders settle. It is
    /// what lets the screen key `nation_colour` off the real id, so the map the
    /// player watches being carved is coloured the same as the map they meet
    /// again under the in-game Country lens. Ids are allocated in one tight
    /// `create_entity` loop, so index i is `nation_id_base + i`. 0 = not yet
    /// known; the screen falls back to the index, which is the same palette in
    /// a different order rather than a wrong-looking map.
    std::atomic<uint32_t> nation_id_base{0};

    /// Bumped on every publish. The renderer keeps its own copy of the map and
    /// re-reads only when this moves, so a still frame costs nothing.
    std::atomic<uint32_t> carve_epoch{0};

    /// Per-tile owner in raster order (`row * grid_w + col`), stored as
    /// **nation index + 1** so the zero-initialised state already means
    /// "unclaimed" and no init loop is needed at construction.
    std::array<std::atomic<int16_t>, carve_capacity> owner{};

    /// Release-stored AFTER the marker it counts, so an acquire-load here is
    /// the reader's guarantee that slots [0, count) are filled.
    std::atomic<int> asset_mark_count{0};

    /// Packed marker: bits 0-8 col, bits 9-17 row, bits 18-23 corp slot. Nine
    /// bits per axis covers any grid this sink can hold (312 x 145 < 512).
    std::array<std::atomic<uint32_t>, max_asset_marks> asset_mark{};

    // Numbers, not prose. A corp's generated NAME is a std::string decided in
    // Pass 5, and pushing a string across an atomics-only seam would need a
    // lock for no gain — the moment the worker returns, the app owns the whole
    // world and can print real names. So the screen shows the derivation live
    // and the names arrive with the finished world.
    std::atomic<int> corp_row_count{0};                            ///< Release-stored, as above.
    std::array<std::atomic<int32_t>, max_corp_slots> corp_focus{};   ///< industrial_focus as int.
    std::array<std::atomic<int32_t>, max_corp_slots> corp_assets{};  ///< Holdings placed in Pass 3.
    std::array<std::atomic<float>,   max_corp_slots> corp_capital{}; ///< Pass 4 capital.
    std::atomic<int32_t> player_slot{-1};                          ///< Which row is the player's; -1 until known.

    // --- Publishing (worker side; every one of these is write-only) ---------

    /// Open the carve for a body of @p gw x @p gh, clearing the map. A grid this
    /// sink cannot hold publishes nothing rather than a truncated lie.
    void begin_carve(int gw, int gh, int seeds)
    {
        if (gw <= 0 || gh <= 0 || gw > 511 || gh > 511 || gw * gh > carve_capacity)
        {
            grid_w.store(0, std::memory_order_relaxed);
            grid_h.store(0, std::memory_order_relaxed);
            return;
        }
        for (int i = 0; i < gw * gh; ++i)
            owner[static_cast<std::size_t>(i)].store(0, std::memory_order_relaxed);
        nation_count.store(seeds, std::memory_order_relaxed);
        grid_w.store(gw, std::memory_order_relaxed);
        grid_h.store(gh, std::memory_order_relaxed);
        carve_epoch.fetch_add(1, std::memory_order_release);
    }

    /// One tile settled. Called from the BFS's settle point, so the map fills in
    /// exactly the order the pass claims ground.
    void claim_tile(int raster_index, int nation_idx)
    {
        if (raster_index < 0 || raster_index >= carve_capacity) return;
        owner[static_cast<std::size_t>(raster_index)]
            .store(static_cast<int16_t>(nation_idx + 1), std::memory_order_relaxed);
    }

    /// Tell the renderer the map moved. Called every N claims, not every claim:
    /// the renderer redraws at most once a frame anyway, and an epoch bump per
    /// tile would be 45,000 needless release fences.
    void publish_carve() { carve_epoch.fetch_add(1, std::memory_order_release); }

    void mark_asset(int col, int row, int corp_slot)
    {
        const int n = asset_mark_count.load(std::memory_order_relaxed);
        if (n >= max_asset_marks) return;
        if (col < 0 || row < 0 || col > 511 || row > 511 || corp_slot < 0 || corp_slot > 63) return;
        asset_mark[static_cast<std::size_t>(n)].store(
            static_cast<uint32_t>(col)
                | (static_cast<uint32_t>(row) << 9)
                | (static_cast<uint32_t>(corp_slot) << 18),
            std::memory_order_relaxed);
        asset_mark_count.store(n + 1, std::memory_order_release); // publishes the store above
    }

    void add_corp_row(int slot, int focus, int assets, float capital)
    {
        if (slot < 0 || slot >= max_corp_slots) return;
        corp_focus  [static_cast<std::size_t>(slot)].store(focus,   std::memory_order_relaxed);
        corp_assets [static_cast<std::size_t>(slot)].store(assets,  std::memory_order_relaxed);
        corp_capital[static_cast<std::size_t>(slot)].store(capital, std::memory_order_relaxed);
        if (slot + 1 > corp_row_count.load(std::memory_order_relaxed))
            corp_row_count.store(slot + 1, std::memory_order_release); // publishes the stores above
    }
};

/// How many tiles the carve settles between epoch bumps. One publish per frame
/// is all a 60 Hz screen can show, so this is tuned to give the eye roughly a
/// hundred frames of growth rather than to keep up with the pass.
inline constexpr int gen_carve_publish_interval = 384;

/// Human labels for `generation_progress::label`, in pass order.
inline const char* const generation_stage_labels[] = {
    "Preparing",            // 0
    "Forming the system",   // 1
    "Settling chemistry",   // 2
    "Drifting continents",  // 3
    "Raising terrain",      // 4
    "Carving rivers",       // 5
    "Seeding peoples",      // 6
    "Founding regions",   // 7
    "Running the ancient era", // 8 — the long one
    "Drawing borders",      // 9
    "Laying roads",         // 10
    "Placing companies",    // 11
    "Finishing",            // 12
    "Running the exploration age", // 13 — 1200 -> 1660, after the ancient era
    "Running the Industrialisation span", // 14 — 1660 -> 1960, after the exploration age (BL-1040)
};
inline constexpr int generation_stage_label_count =
    static_cast<int>(sizeof(generation_stage_labels) / sizeof(generation_stage_labels[0]));

/// BL-1053: how many stages a `make_hard_coded_world` call on @p cfg will
/// REPORT -- the number of times it advances `generation_progress::stage`,
/// which is what `stage_count` must hold for the bar to reach its end. Never
/// the label count above: some labels caption a stage rather than being one
/// (the exploration age and the Industrialisation span re-caption the history's
/// single stage), and one is never published. Set by generation itself at its
/// first line; a caller that publishes `stage_count` before the worker starts
/// uses this so the two agree.
int generation_stage_count(const world_gen_config& cfg);

/// What the generation pass recorded about each body, for the staged generation
/// screen and the planet report.
///
/// This is a PRESENTATION artefact, not simulation state: it is filled during
/// make_hard_coded_world and handed to the app, which reveals it stage by stage.
/// It never enters the `world` struct — the same reasoning that keeps world_params
/// in the app (BL-114). It is NOT off the serialisation seam, though:
/// `core/save_game.cpp` writes the report whole (`w_report`), because a loaded
/// campaign has no generation to consult and the Continent lens, the History
/// ledger and the Generation Ledger's tile replay all read it. Two seams — a
/// field added anywhere in this struct is a `save_game_version` bump.
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
        /// The Continent lens is the consumer.
        ///
        /// IT IS NOT OFF THE SERIALISATION SEAM (corrected 2026-09-03, BL-763).
        /// This comment used to say "it never enters `world`, so it stays off
        /// the serialisation seam". The first clause is true and the second does
        /// not follow: `continent_state` is written and read by
        /// `src/core/save_game.cpp` as part of the SAVE ENVELOPE, so a field
        /// added here is a `save_game_version` bump exactly like a field on
        /// `world_params`. Two seams, and this struct is on the second one.
        continent_state continents;

        /// What the settlement/industrialisation pass computed for this body
        /// (BL-218/BL-219). Its `history` is empty here for the same reason
        /// `continents.history` is — those lines were merged into
        /// `state.history` at generation. What is kept is the region set (who
        /// settled where, whose gods they keep, which ancient deposits they sit
        /// on, when their furnaces lit), the rupture `checkpoints`, and the
        /// `lacunae` count — the holes the wars left in the record. Nothing
        /// else records any of it. Presentation data, like the rest of this
        /// struct: it never enters `world`, but it reaches the save with the
        /// rest of the report (`w_settlement`, `core/save_game.cpp`).
        settlement_state settlement;

        /// THE RECORDED ERA -1 TIME-LAPSE (NR-733, Ben's ruling 2026-08-30) — the
        /// ownership history the History ledger's Ages view replays, beside the
        /// settlement it is the history OF.
        ///
        /// RECORDED RATHER THAN RE-SIMULATED, and that is the whole point. The
        /// view used to re-run the era itself, which made it a seventh caller of
        /// an invocation `era_minus_one.hpp` exists to keep singular. It diverged
        /// from generation on all six of BL-462's axes, and three could not be
        /// closed at the call site because `settlement` above is the state AFTER
        /// the sim mutated it — a re-run started the era from its own ending, so
        /// it reported 0 battles and 0 conquests however carefully it was
        /// parameterised. Replaying what generation produced closes all six by
        /// deleting the second caller, and costs a fold over a change list rather
        /// than minutes on the drawing thread.
        ///
        /// EMPTY ON EVERY BODY BUT THE CRADLE, and on all of them in a 1960-era
        /// world — generation runs the era once, gated on `era_minus_one_enabled`.
        /// The view reads an empty record as "never settled", which is what it is.
        ///
        /// UNLIKE the rest of this struct this one DOES reach the save: the
        /// report is serialised whole by `core/save_game.cpp`, which is why it
        /// moved `save_game_version` to 3.
        era_timelapse prehistory_timelapse;

        /// THE EXPLORATION SPAN'S OWN RECORD (BL-946), same shape and same
        /// discipline as `prehistory_timelapse` above -- recorded, never
        /// re-simulated. Empty wherever `exploration_sim_enabled(params)` did
        /// not run (an opted-out world, or any body but the cradle). This is
        /// what the wizard's new Exploration round replays.
        era_timelapse exploration_timelapse;

        /// THE INDUSTRIALISATION SPAN'S OWN RECORD (BL-1068), 1660 -> 1960,
        /// same shape and discipline as the two above -- recorded at the one
        /// call site that ran the span, never re-simulated. Empty wherever the
        /// span did not run (`industrialisation_span_enabled` off, Exploration
        /// did not run, a `stop_after_exploration` run, or any body but the
        /// cradle). This is what the wizard's Industrialisation round replays.
        /// A RECORD ONLY: nothing at world setup reads it, so it cannot steer
        /// the world it describes (a watched and an unwatched build are the
        /// same build).
        era_timelapse industrialisation_timelapse;

        /// Exactly what `generate_body_tiles` was called with for this body — the
        /// arguments that are NOT recoverable from anything else the report or the
        /// world holds (the seed above all: Kepler's is chosen by the BL-276
        /// reject-and-reroll gate, so it cannot be re-derived from world_params).
        ///
        /// The Generation Ledger (BL-303) regenerates a body's `generation_record`
        /// on demand from these rather than the world storing one per tile — the
        /// derivation is deterministic and cheap, so keeping it is bloat
        /// (GENERATION_LEDGER.md § Data lifetime). What DOES reach the save is
        /// this struct: the six inputs are written with the rest of the report
        /// (`w_body_entry`, `core/save_game.cpp`) precisely so a loaded campaign
        /// can replay the same tiles the ledger explains. The intermediates it
        /// regenerates never do.
        struct tile_inputs
        {
            bool     valid           = false; ///< False on a report built without a tile pass.
            uint32_t seed            = 0;     ///< The per-body tile seed actually used.
            float    deposit_scalar  = 1.0f;  ///< The abundance multiplier the deposits were scaled by.
            int      gw              = 0;     ///< Grid width the pass ran at.
            int      gh              = 0;     ///< Grid height the pass ran at.
            /// Whether the Continents convergent mask was passed in. `continents`
            /// above holds the mask for every body, but only the homeworld's Pass 5
            /// was given it — regenerating with it where generation did not would
            /// silently produce a DIFFERENT surface from the one on screen.
            bool     used_convergent = false;
        } tiles;
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
    int64_t prehistory_conquests = 0; ///< Regions that changed hands.
    int64_t prehistory_foundings = 0; ///< Regions founded by the sim.

    // --- The Exploration span's own counters (BL-946), same discipline as
    // the four above -- zero wherever `exploration_sim_enabled(params)` did
    // not run (an opted-out world, or a report stopped before it, e.g. the
    // wizard's Empires round).
    int64_t exploration_years     = 0; ///< Years simulated in the Exploration span.
    int64_t exploration_battles   = 0; ///< Battles fought in that span.
    int64_t exploration_conquests = 0; ///< Regions that changed hands.
    int64_t exploration_foundings = 0; ///< Regions founded (trade provinces) in that span.

    // --- The Industrialisation span's own counters (BL-1068), same
    // discipline again -- zero wherever that span did not run (see
    // `body_entry::industrialisation_timelapse`).
    int64_t industrialisation_years     = 0; ///< Years simulated in the Industrialisation span.
    int64_t industrialisation_battles   = 0; ///< Battles fought in that span.
    int64_t industrialisation_conquests = 0; ///< Regions that changed hands.
    int64_t industrialisation_foundings = 0; ///< Regions founded in that span.

    // --- What the grudge record seeded (BL-898) -----------------------------
    //
    // Reported for exactly the reason the four counters above are, and this
    // item is the reason that reason is worth restating: the grudge table was
    // CARRIED across the pass 1 -> pass 2 handoff for a whole sprint with no
    // consumer at all, and a carried-but-unread field is indistinguishable from
    // one that was never carried — except that it looks finished. These two
    // make the seeding countable, so a generation that seeded nothing cannot
    // pass for one that was never wired.
    int32_t grudge_sentiment_rows    = 0; ///< Grudges that became a sentiment row.
    int32_t grudge_sentiment_dropped = 0; ///< Grudges refused: below floor, no successor, self pair.

    // --- The ancient road record and what it carved (BL-768) ----------------
    //
    // Reported for the reason the four counters above are: the acceptance test
    // is behavioural — roads whose shape follows the history's trunk routes, and
    // markets where trade concentrated — and a pass that recorded nothing looks
    // exactly like one that was never wired. These three make the difference
    // countable rather than eyeballed.
    //
    // Zero when the era did not run, which is every `no_prehistory()` harness.
    int64_t prehistory_corridors = 0; ///< Distinct region-to-region corridors recorded.
    int64_t prehistory_junctions = 0; ///< Regions where three or more of them met.
    /// Markets that qualified ONLY because their centre stands at a trade
    /// junction — the ones that would not exist on the nation gate alone. THE
    /// EXACT COUNT, taken at the carve by evaluating both gates, rather than a
    /// difference between two worlds: an era-ON and an era-OFF world do not
    /// share a settlement pattern, so subtracting their market counts would
    /// measure the whole era rather than this term.
    int64_t markets_from_trade   = 0;

    // --- The handoff validators' verdict (BL-969) ---------------------------
    //
    // `pass_one_output_valid` and `exploration_output_valid` run on the
    // SHIPPED path, right after each fold, against the live `creed_state`
    // the fold read from. A violation is recorded here rather than swallowed:
    // the flag so a harness can assert it never trips, the message so a
    // human can read which clause of GENERATION_STRATEGY.md § What crosses
    // each handoff the world just broke. Debug builds also assert; every
    // build prints one line to stderr. Both handoffs share the pair -- the
    // message names which validator spoke, and a second failure appends.
    bool        handoff_invalid = false;
    std::string handoff_violation;
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
/// @param works  The Era -1 works table (BL-321), forwarded to the history sim.
///               Null — the default, and every headless caller that does not
///               care — runs the pre-history with works disabled. Defaulted for
///               the same reason `gen_cfg` is: a caller that never touches Lua
///               still builds a world, it just builds one without works.
/// @param fixture Optional out-param (BL-462): when non-null, receives EXACTLY
///               what the Era -1 year-tick sim was invoked with, plus the three
///               counts it produced. It exists so a harness can re-run the era
///               generation actually ships instead of `history_sim_params`'s
///               struct default — see world/era_minus_one.hpp for the six
///               divergences that made every Era -1 check measure a different
///               sim. Costs one copy of the settlement, creeds and terrain, and
///               only when asked for; nullptr pays nothing.
world make_hard_coded_world(world_params params = {}, generation_report* report = nullptr,
                            const world_gen_config& gen_cfg = {},
                            generation_progress* progress = nullptr,
                            const works_registry* works = nullptr,
                            era_minus_one_fixture* fixture = nullptr);

/// The homeworld's tile grid dimensions — one authority the build and the
/// wizard preview both read.
///
/// **261×121 = 31,581 tiles since 2026-08-14 (Ben): 70% of the 2026-08-12
/// 312×145 = 45,240 area, which was itself three times the original
/// 180×84 = 15,120.** The 3× map priced Debug-build world generation at ~38 s
/// and the lazy Era −1 ages run past ten minutes; 70% area claws back cost
/// while keeping roughly double the original playfield. The aspect ratio is
/// held at ~2.15:1 so the cylinder still maps a plausible globe: columns wrap
/// as the equator, rows do not.
///
/// The scale-up is only half a change on its own. Travel time used to be
/// `1 / distance_in_AU`, which is zero for two markets on the same body — so
/// every intra-body convoy arrived in exactly one econ tick regardless of
/// distance, and tripling the map would have made trade RELATIVELY faster
/// rather than slower. The distance→time model in logistics.hpp
/// (`body_km_per_tile`, `convoy_travel_ticks`) landed with this constant for
/// that reason; changing one without the other is a regression.
inline constexpr int home_grid_width  = 261;
inline constexpr int home_grid_height = 121;

// The carve sink is sized for exactly this grid. Declared up beside the struct
// (which precedes these constants) and tied back here, so a future map resize
// fails to compile rather than silently publishing a blank map — begin_carve's
// capacity guard would otherwise turn the regression into a missing screen.
static_assert(generation_progress::carve_capacity == home_grid_width * home_grid_height,
              "generation_progress::carve_capacity must match the homeworld grid");

/// Generate ONLY the homeworld tile surface into @p w (a scratch world), exactly
/// as make_hard_coded_world builds Kepler's: same resolved preferences, same
/// planetology chain, same Continents pass, same BL-276 acceptance gate, same
/// seed formulas — the gate is literally the same function. The New World
/// wizard's preview pane calls this so the map a player rerolls IS the map
/// "Begin" hands them. The river pass runs too, under the same seed formula, so
/// the wizard's lapse maps draw the campaign's rivers (BL-915); the political
/// layer (a sibling pass the preview does not show) is skipped. Returns
/// raster-order tile ids.
std::vector<entity_id> generate_home_surface_preview(world& w, entity_id body,
                                                     const world_params& params,
                                                     const world_gen_config& gen_cfg = {});

/// Fold a settlement into the MIGRATION's ownership record: one change per
/// region at its `founded_year`, owned by its plurality CULTURE (not a polity),
/// ascending by year. Pure and read-only. This is the record the wizard's
/// Culture round replays; `make_hard_coded_world` calls it at the migration's
/// end, and the wizard calls it under `--verify` to lift the same record off
/// a finished report rather than run the pass a second time (BL-919).
era_timelapse build_migration_timelapse(const settlement_state& ss, const creed_state& cs,
                                        int64_t start_year, int64_t end_year);

/// The same fold without a creeds roster: the culture tree is REBUILT from the
/// settlement's own record (`cradle_coined_year` first, then `spawned_cultures`
/// in allocation order -- the exact numbering the roster carries, per
/// settlement.hpp), so the `culture_split` events are the same ones. This is the
/// `--verify` adoption path (BL-919), where the report holds a settlement and
/// no `creed_state`.
era_timelapse build_migration_timelapse(const settlement_state& ss, int64_t start_year,
                                        int64_t end_year);
