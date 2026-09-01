#pragma once

#include <SDL3/SDL.h>
#include "agent_seam.hpp"
#include "ground_layer.hpp" // BL-732: baked-ground chunk cache
#include "sim_loop.hpp"
#include "scripting/lua_state.hpp"
#include "ui/ui_state.hpp"
#include "world/body_names.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"      // BL-630: the spawn shortlist and the seat
#include "world/tech_tree.hpp"
#include "world/works_roster.hpp"
#include "world/world.hpp"

#include "ui/canvas_command.hpp"
#include "scripting/persona_pack.hpp"
#include "ui/chat_panel.hpp"
#include "ui/plot_history.hpp"
#include "ui/strategy_readout.hpp"
#include "world/market_clearing.hpp"
#include "world/planetology.hpp"
#include "world/standing.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <future>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/// Top-level application object. Owns the SDL window and renderer, orchestrates
/// the simulation and economy loops, and holds the Lua state for the lifetime
/// of the process.
class app
{
public:
    app();
    ~app();

    /// Enter the main loop. Returns when the user closes the window.
    ///
    /// @return 0 on clean exit.
    /// @param windowed_autostart Press "Begin" programmatically on the first frame
    /// (--autostart-windowed): the real window, the real ImGui frame loop, the real
    /// loading-screen -> in_game transition, then exit after a few in-game frames.
    /// Exists to reproduce interactive-only startup crashes under a terminal where
    /// main()'s catch block can actually report them.
    /// How run()'s windowed path boots.
    ///
    /// `smoke` is the --autostart-windowed SMOKE TEST: it walks the wizard, renders
    /// a fixed 120 in-game frames and exits 0. It is not a way to open the game —
    /// pointing a human at it looks exactly like a crash a couple of seconds in,
    /// which is precisely what happened on 2026-08-16.
    /// `play` walks the same wizard and then just keeps running, for when someone
    /// needs to LOOK at the running game without clicking through the wizard first.
    enum class autostart_mode : std::uint8_t { none = 0, smoke, play };

    int run(autostart_mode autostart = autostart_mode::none);

    /// BL-412: ask run() to host the live agent control seam on
    /// 127.0.0.1:@p port (0 = don't host, the default). The listener opens
    /// once a campaign is actually running (in_game); the session actor is
    /// pinned to the player corp — the seat the agent occupies. Call before
    /// run().
    void host_agent(uint16_t port) { m_agent_port = port; }

    /// BL-705: override `world_params::epoch_year` for every world this process
    /// generates (`--epoch <year>`). Absent = the struct's own default.
    ///
    /// WHY A FLAG EXISTS AT ALL. The 1960s industrial start is a live branch —
    /// `era_band_for_epoch` puts the recipe registry on the industrial band, and
    /// `era_minus_one` skips the antiquity prehistory above 1700 — but nothing
    /// in `src/` ever *set* the field, so reaching it meant editing a source
    /// default. Both starts are supported (`docs/economy/ERAS.md` § Where the
    /// ladder starts), so selecting between them belongs at the command line.
    ///
    /// Applies to a NEW world only; a save carries its own epoch and the load
    /// path is not overridden. Call before run() / run_autostart().
    ///
    /// @param year Calendar year the generated world begins at (< 1700 takes
    ///             the antiquity branch).
    void set_epoch_year(std::int64_t year)
    {
        m_epoch_year_override = year;
        m_epoch_year_set      = true;
        m_pending_world_params.epoch_year = year; // the plain interactive path never re-seeds
    }

    /// Open into this save on the next `run()` instead of the main menu.
    /// Set by `--load <path>`; consumed once. Empty = the normal menu entry.
    /// @param path Save file to open.
    void open_save(std::string path) { m_pending_load = std::move(path); }

    /// Enter the session in SPECTATOR MODE — no human seat
    /// (AI_OPPONENT.md § 10i). Set by `--spectate`; call before run().
    ///
    /// ENTRY AT START, and only at start. § 10i's model is that a spectated
    /// session has no human owner, so the standing prohibition's PRECONDITION
    /// is absent rather than waived — which is a property of the whole
    /// session, not a view a watcher steps into and out of. A mid-run flip
    /// would change halfway through which corps the scorer may legally act
    /// on, so no in-game control clears this and the only way in is at
    /// launch. Off by default, so an ordinary played session is untouched.
    ///
    /// The flag lands on `m_ui.spectating` immediately: `m_ui` is a plain
    /// member, live from construction, and nothing outside the verify API
    /// reassigns it — so setting it here carries through the menu, the
    /// wizard, the warm start and every campaign the session opens.
    void spectate_session() { m_ui.spectating = true; }

    /// Run a non-interactive visual-verification session: set up a deterministic
    /// world (seeded, sim paused), expose the `verify` Lua API (which drives view
    /// and overlay state directly and captures named PNG frames), execute the
    /// script, then exit. The driver of the project's `visual` requirement class.
    /// See docs/development/DEVELOPMENT_PRACTICES.md § Visual verification.
    ///
    /// @param script_path Path to the .lua verification script.
    /// @param bless       When true, each capture is written into the golden
    ///                    directory (regenerating references) instead of being
    ///                    compared against an existing golden.
    /// @return 0 on a clean run with no golden-diff failures; non-zero if the
    ///         script failed to load/execute or any capture failed its golden diff.
    int run_verify(const std::string& script_path, bool bless = false);

    /// Batch form of run_verify (BL-423): every *.lua under @p dir (sorted,
    /// lib.lua excluded), against ONE world generation — the ~38 s
    /// make_hard_coded_world cost is paid once, and a pristine world/ui_state
    /// snapshot is restored before each script so every script sees exactly the
    /// state a solo run gives it. A script that throws is counted as a failure
    /// and the batch continues. Same return contract as run_verify.
    int run_verify_all(const std::string& dir, bool bless = false);

    /// Headless: generate a world and run start_new_game's WHOLE tail, then
    /// exit. Covers what run_verify never reaches — it calls setup_world +
    /// load_economy and stops, so generate_background_firms and the pre-game
    /// warm start had no automated coverage at all. Returns 0 on success.
    int run_autostart();

private:
    /// The shared core of run_verify / run_verify_all: one setup + API
    /// registration, then each script against a pristine-state restore.
    int run_verify_scripts(const std::vector<std::string>& scripts, bool bless);

    /// Reset every app-owned per-tick accumulator that lives OUTSIDE world and
    /// ui_state (balance/income/market/resource histories, the last econ
    /// report, orbit/survey clocks). The batch restore (BL-423) reassigns
    /// world + ui, and these are the third bucket it would otherwise miss —
    /// found the honest way: a header sparkline showed six points where a solo
    /// run shows four. Any new per-tick app member belongs in here.
    void reset_verify_transients();

    // --- Save / load (BL-536) -----------------------------------------------

    /// Path of the quick-save slot -- one fixed file beside the executable.
    /// Deliberately the simplest thing that works: named slots, an overwrite
    /// prompt and a load menu are a UI item, not this one.
    /// @return The quick-save file path.
    std::string quick_save_path() const;

    /// Write the live world plus its app-layer envelope to @p path.
    ///
    /// Gathers the four envelope parts off `app` (clock, params, generation
    /// report, histories + the ui_state slice) and hands them to
    /// `write_save_game`. Posts the outcome to the comms log either way -- a
    /// save that silently does nothing is worse than one that says it failed.
    ///
    /// @param path Destination file.
    /// @return     True on success.
    bool save_game_to(const std::string& path);

    /// Replace the live campaign with the save at @p path.
    ///
    /// Leaves the running campaign untouched if the read fails -- `read_save_game`
    /// builds into scratch, so a corrupt file costs the player nothing. On
    /// success it also drops the app-side transients that belong to the world
    /// being replaced (the econ report, the standings, the BL-198 deposit
    /// series), which would otherwise describe a campaign that is no longer
    /// loaded.
    ///
    /// @param path Source file.
    /// @return     True on success.
    bool load_game_from(const std::string& path);

    void process_events(bool& running);

    /// Map a key-down event onto the unified action table and dispatch it, or
    /// trigger a capture (F12). Navigation keys are ignored while ImGui owns the
    /// keyboard. The binding table lives in app.cpp alongside the F1 help overlay
    /// that is generated from it (so table and overlay are always in sync).
    ///
    /// @param key The SDL key-down event.
    void handle_key_down(const SDL_KeyboardEvent& key);

    /// Central dispatch for every app action — canvas navigation, time controls,
    /// and UI toggles. Canvas-only commands route to apply_canvas_command; the
    /// time-control and help-toggle commands are handled here (they need sim_loop /
    /// app members that apply_canvas_command does not see).
    ///
    /// @param cmd Action to dispatch.
    void dispatch_action(ui::canvas_command cmd);

    void render();

    // --- Synthetic pointer input (BL-521) -----------------------------------
    // The verify harness drove UI state DIRECTLY and could not press anything, so
    // every interactive surface arrived with its live half owed by construction.
    // These three drive the REAL input path instead: ImGui's own event queue, the
    // real hit-tests, the real click handlers. Deterministic by construction — the
    // sequence is measured in FRAMES, never in wall-clock time (see
    // inject_pointer's note on MouseDoubleClickTime).

    /// Feed the pending synthetic pointer state into ImGui's event queue for the
    /// frame about to be built. Called from render() between the backend's
    /// NewFrame and ImGui::NewFrame, so the injected events are the LAST in the
    /// queue and therefore win over anything the SDL backend fed from the OS
    /// cursor. A no-op until a script injects (so interactive play, and every
    /// pre-BL-521 verify script, is byte-identical).
    void pump_injected_input();

    /// Move the synthetic cursor to a screen position and render `frames` frames
    /// there. Writes BOTH pointer sources — ui_state.mouse (what the canvases
    /// hit-test with, BL-061) and ImGui's MousePos (what the shell routes input
    /// with) — because a disagreement between them hit-tests one place and routes
    /// to another.
    ///
    /// @param x,y    Screen position in pixels.
    /// @param frames Frames to render at that position (>= 1); dwell-gated UI is
    ///               reached by frame count, the harness's fixed clock.
    void inject_move(float x, float y, int frames);

    /// Press and release a mouse button at a screen position, `clicks` times.
    /// Renders its own frames and returns once the gesture is complete, so a
    /// following capture() shows the result.
    ///
    /// @param x,y    Screen position in pixels.
    /// @param button ImGuiMouseButton_Left / _Right / _Middle.
    /// @param clicks 1 = single click, 2 = double click.
    void inject_pointer(float x, float y, int button, int clicks);

    /// Resolve the screen point that IS Planetary tile (col, row), by asking the
    /// canvas: it centres the tile on the next draw and publishes where that put
    /// it. Renders one frame, and therefore PANS the view — the canvas transform
    /// lives in exactly one place and the harness does not get a second copy.
    ///
    /// @return The tile's screen centre, or {-1,-1} when the Planetary canvas is
    ///         not the primary rung / the request did not resolve.
    ImVec2 resolve_tile_screen(int col, int row);

    /// Draw the main menu — the deliberate entry point shown at launch (no world
    /// loaded yet). Wires the "New Game" button to open_new_world_wizard() and "Quit"
    /// to m_quit_requested. Carries the seed and the world-shape knobs; the
    /// Planetology knobs moved into the wizard, where each is decided against a chart.
    /// Called from render() when m_screen == menu; folded into render()'s single
    /// capture path so the menu is golden-verifiable.
    void draw_main_menu();

    /// Draw the New World wizard (BL-167) — the surface between "New Game" and the
    /// first frame of play. The player walks THREE rounds, each stacking the charts
    /// and explanations of its chain stages and then taking that round's
    /// preferences. The charts come from a live resolve_preferences + preview_system
    /// run, so they show the world the roll actually produced.
    ///
    /// NOTHING is generated here. Every frame runs the chain over the prototype body
    /// set as a pure, throwaway preview; the world itself is built once, from the
    /// finished preferences, when the player presses "Begin" (start_new_game). Called
    /// from render() when m_screen == generating.
    void draw_generation_screen();

    /// Open the wizard at its first round. The menu's "New Game" button — it commits
    /// nothing and builds nothing, it just hands the player the first decision.
    void open_new_world_wizard();

    /// Resolve the pending preferences against the seed, then re-run the Planetology
    /// chain over the prototype body set into m_wiz_preview (and m_wiz_undrawn, the
    /// same world with the drawdown dial at zero — the Spend chart's "before"
    /// reference). Called by the wizard whenever a preference or a reroll moves;
    /// pure, and cheap enough to call freely.
    void refresh_wizard_preview();

    /// Actually start the campaign, from the params the wizard settled: rebase the
    /// sim clock, build the world, load the economy, run the pre-game warm start,
    /// and hand over to play. The wizard's "Begin" button.
    ///
    /// SPLIT 2026-08-12 (the AppHangB1 stall): the tail no longer runs as one
    /// synchronous block inside a frame. `start_new_game_prelude` does the cheap
    /// main-thread setup (~20 ms), then poll_worldgen runs the 80 warm-start
    /// ticks in time-boxed slices — one slice per loading-screen frame — and
    /// `finish_new_game` rebases the clock and enters play. The UI repaints
    /// between slices, so Windows never judges the app hung.
    void start_new_game_prelude();
    void finish_new_game();
    /// Kick generation onto a worker and switch to the loading screen. Loads the
    /// Lua world-gen config first, on this thread — sol2 is not thread-safe.
    void begin_new_game();
    /// Poll the worker; on completion finish setup and enter the campaign.
    void poll_worldgen();
    /// The loading screen: a progress bar over generation_progress.
    void draw_building_screen();
    /// The live nation carve + charter marks drawn inside it (BL-305).
    void draw_building_carve();

    /// Build the prototype world and frame the opening view. Shared by run() and
    /// run_verify() so both start from the same deterministic state. @p params is the
    /// world descriptor (seed + knobs); defaulted so run_verify() stays deterministic-cold.
    void setup_world(world_params params = {});

    /// A default-constructed `world_params` with the `--epoch` override applied
    /// (BL-705). Every "start from scratch" site goes through this rather than
    /// `world_params{}`, so the flag reaches the interactive wizard, both
    /// autostart paths and run_verify alike.
    world_params fresh_world_params() const;

    /// Load the economy Lua data layer (scripts/recipes.lua + scripts/economy.lua)
    /// into m_registry, then author processing recipes onto the generated assets
    /// (the recipe id is a registry index, so it is assigned here, not at
    /// generation). Shared by run() and run_verify().
    void load_economy();

    /// Load scripts/works.lua into `m_works` if it is not loaded already
    /// (BL-321). IDEMPOTENT AND CALLED EARLY, because the works table is now an
    /// INPUT TO WORLD GENERATION — the Era -1 sim builds from it — while
    /// `load_economy` deliberately runs AFTER `setup_world` (the recipe
    /// registry is not needed to generate a world, and the corp pass needs a
    /// generated world to read). Left inside load_economy, the table would have
    /// been empty for the run that needed it and full for nothing, and the only
    /// symptom would have been a pre-history with no works in it.
    void ensure_works_loaded();

    /// Run one economy tick: production → market clearing → budget, storing the
    /// per-building report the ledgers read. Driven by the econ-tick boundary
    /// in run() and by the verify API.
    void step_economy();

    /// Render exactly one frame and write it to screenshots/<name>.png. Used by
    /// the verify Lua API's capture() to grab a deterministic frame on demand.
    ///
    /// @param name Base file name (no extension) for the capture.
    void capture_frame(const std::string& name);

    /// Read the current backbuffer and write it to screenshots/ as a PNG.
    /// Triggered by F12 (timestamped name) or by capture_frame (named); called at
    /// the end of render(), after the frame is composited but before present.
    void save_screenshot();

    /// Compare a just-captured frame against its golden reference (or, in bless
    /// mode, overwrite the golden). Called from save_screenshot during a verify
    /// run; a no-op for interactive captures (m_golden_dir empty). Emits an
    /// advisory PASS/FAIL log + a diff image, and bumps m_verify_failures on fail.
    ///
    /// @param name The capture's base name (matches the golden file stem).
    /// @param rgba The captured RGBA surface (read, not retained).
    void compare_to_golden(const std::string& name, SDL_Surface* rgba);

    /// Persisted display/options settings (options.cfg, key=value). Loaded at
    /// construction so the window opens at the player's last size/mode; re-written
    /// whenever a setting changes in the F10 Options window.
    struct display_settings
    {
        int  window_w   = 1720;
        int  window_h   = 1080;
        bool fullscreen = false;  ///< Borderless-desktop fullscreen.
        bool vsync      = true;
        int  ui_scale_step = 0;   ///< BL-063 discrete UI scale: 0=1.0, 1=1.25, 2=1.5.
    };

    /// Read options.cfg into m_settings; a missing/partial file leaves defaults.
    void load_settings();
    /// Write m_settings to options.cfg.
    void save_settings() const;
    /// Apply m_settings to the live SDL window + renderer (size, fullscreen, vsync).
    void apply_display_settings();
    /// Re-load the UI font atlas at the size for m_settings.ui_scale_step (BL-063).
    void apply_ui_scale();

    /// How many rounds the New World wizard walks (BL-167). Declared here rather than
    /// beside the wizard code because the verify API — registered long before it —
    /// clamps against the same count.
    static constexpr int wizard_round_count = 3;

    /// Which top-level screen is active. run() opens on the menu; "New Game" enters
    /// `generating` (the New World wizard, where the player takes the three rounds of
    /// Planetology preferences) and the wizard's "Begin" hands over to play; run_verify() jumps
    /// straight to in_game (the harness renders the live world, not the menu, unless
    /// a script asks for it via verify.show_menu / verify.show_generation). Only
    /// `in_game` simulates.
    /// `building` (2026-08-12) is the async world-generation screen: generation
    /// moved onto a worker thread because it takes ~25 s on the 312x145 grid and
    /// was called from inside an ImGui frame, so the UI never pumped and Windows
    /// reported the process as "not responding" — indistinguishable from a crash.
    // BL-630 retired the `choosing_corp` stage that used to sit between
    // `building` and play: the player is no longer asked which corporation to
    // be, they are SEATED on one drawn from the spawn shortlist after the warm
    // start (STARTUP.md § The seat). `building` now hosts both phases — the
    // carve, then the warm start — and hands straight to `in_game`.
    enum class app_screen { menu, generating, building, in_game };
    app_screen m_screen = app_screen::menu;
    /// Pending `--load` target, consumed by run() before the frame loop.
    std::string m_pending_load;
    bool       m_quit_requested = false;  ///< Set by the menu's Quit button; breaks the run() loop.

    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    sim_loop        m_sim_loop;
    lua_state       m_lua;
    world           m_world;
    world_params    m_pending_world_params; ///< Edited by the New World menu and then by the wizard; consumed by start_new_game (BL-114/167).
    world_params    m_active_world_params;  ///< The descriptor the live world was built from; shown as the "seed used".

    /// BL-705: the `--epoch <year>` override, and whether one was given.
    /// A separate presence flag rather than a sentinel value, because a
    /// negative epoch year is legitimate (BCE, astronomical numbering) and 0 is
    /// the real default — so no in-band value can mean "absent".
    bool            m_epoch_year_set = false;
    std::int64_t    m_epoch_year_override = 0;

    // Generation (BL-167). The report is a PRESENTATION artefact filled by
    // setup_world; it never enters `world`, so it stays off the serialisation seam.
    // It records the world ACTUALLY BUILT — the wizard decides against its own live
    // preview, not against this, so nothing reads it yet; it is the planet report's input.
    generation_report m_generation_report;   ///< Per-body Planetology results + per-stage summaries for the world that was built.

    // --- New World wizard state (BL-167) ---
    // The wizard walks THREE rounds (each covering several chain stages), recomputing
    // a THROWAWAY preview of the whole system whenever a preference or a reroll
    // changes. None of this touches m_world: the world is built once, on "Begin",
    // from m_pending_world_params.
    int  m_wiz_round = 0;    ///< Round the player is on, 0 .. wizard_round_count-1.
    /// --autostart-windowed wizard driver: frames spent in the wizard so far, or
    /// -1 when inactive (every interactive run). While >= 0 the wizard advances a
    /// round every ~20 frames and presses Begin from inside its own draw — the
    /// same mid-frame call site the real button uses.
    int  m_autostart_wizard = -1;
    bool m_wiz_dirty = true; ///< A control moved (or the wizard just opened) — recompute the preview next frame.
    resolved_world m_wiz_resolved{};              ///< The pending preferences resolved against the seed — the params every preview chart is drawn from, plus the reroll cost (attempts / gave_up).
    body_naming m_wiz_names{};                    ///< The coined body catalogue for the pending seed (BL-257) — what the wizard's charts and orrery label bodies with.
    std::vector<planetology_state> m_wiz_preview; ///< Live chain result per prototype body at the resolved params.
    std::vector<planetology_state> m_wiz_undrawn; ///< The same run with drawdown forced to 0 — the Spend chart's "before" reference.

    // The REAL homeworld surface for the preview globe: raster PACKED terrain
    // axes (substrate << 4 | cover — ui::preview_pack, BL-519)
    // from generate_home_surface_preview (same pipeline, same gate, same
    // seed as "Begin" — bench + parity: tools/verify/home_surface_bench.cpp).
    // Built ASYNC (~0.5-0.9s: the BL-276 gate probes up to 6 scratch surfaces),
    // so a control click never blocks; the pane shows the previous surface until
    // the new one lands. Under --verify it is built synchronously instead, so a
    // golden capture never races the worker. The future is never destroyed while
    // pending (std::async's dtor would block); a params move mid-build sets the
    // stale flag and the poll relaunches on arrival.
    std::future<std::vector<uint8_t>> m_wiz_surface_future;

    // --- Async world generation (2026-08-12) --------------------------------
    /// The worker running make_hard_coded_world. Valid only while `building`.
    std::future<world>  m_worldgen_future;
    /// Read every frame by the loading screen; written by the worker.
    generation_progress m_worldgen_progress;
    /// The config the worker was launched with — Lua is loaded on the MAIN
    /// thread before launch, because sol2 is not thread-safe.
    world_gen_config    m_worldgen_cfg;
    world_params        m_worldgen_params;

    /// The loading screen's own copy of the live carve (BL-305), pulled from
    /// `m_worldgen_progress.owner` only when `m_carve_seen` falls behind the
    /// sink's epoch. Holding a copy is what makes a still frame free: without
    /// it the screen would issue 45,000 atomic loads a frame to redraw a map
    /// that has not moved. Values are nation index + 1; 0 is unclaimed.
    std::vector<int16_t> m_carve_view;
    uint32_t             m_carve_seen = 0;
    std::vector<uint8_t> m_wiz_surface;       ///< Raster compositions; empty = not yet built.
    bool m_wiz_surface_stale = false;         ///< Params moved while a build was in flight.
    void launch_wizard_surface_build();       ///< Start the worker for the CURRENT pending params.
    void poll_wizard_surface();               ///< Per-frame: adopt a finished build, relaunch if stale.

    // --- The seat (BL-630, 2026-08-26) --------------------------------------
    //
    // The starting-corp SELECTION stage is retired. The warm start now runs with
    // NOBODY SEATED, under `corp_ai_params::spectating`, and the player is
    // seated afterwards on a corporation drawn from the viability shortlist.
    // Owned by CORPORATION_GENERATION.md § The spawn shortlist, and the seat.
    //
    // WHY THE REORDER IS THE WHOLE ITEM. The old stage was pinned before the
    // warm start because corp_ai excluded the flagged corp, so whichever corp
    // was the player's sat strategically frozen through all 80 pre-game ticks —
    // which is exactly why no balance could be shown to choose on. Running the
    // warm start under spectate removes the exclusion's SUBJECT (BL-409), so
    // every corp is scorer-driven, every corp files real returns, and a
    // profitability read becomes possible for the first time.

    /// What `seat_player_corporation` decided at this campaign's start, kept for
    /// the log line and for anything that later wants to say the floor went
    /// unmet. Presentation state: never serialised, rebuilt on a new campaign,
    /// empty after a `--load` (a saved campaign carries its seat in `world`).
    spawn_seat_result m_seat_result;
    void seat_player();                ///< Draw the seat and re-point the player-scoped caches.

    /// Pre-game warm start: 20 in-game years of quarterly econ ticks (Ben,
    /// 2026-08-10) — see start_new_game_prelude's warm-start comment.
    static constexpr int pre_game_ticks = 80;
    /// Warm-start ticks completed so far, or -1 when no warm start is in
    /// progress. >= 0 marks the sliced phase between generation finishing and
    /// play starting: poll_worldgen runs a time-boxed batch per call and the
    /// loading screen draws its inner bar from it (2026-08-12, the hang fix).
    int m_warm_ticks_done = -1;
    /// True while the warm-start ticks run. step_economy suppresses the persona
    /// counsel while set: measured at ~1.05 s per tick against ~80 ms for the
    /// whole rest of the tick (2026-08-12), it was 93% of the stall, and its
    /// output is advisory chat for pre-game quarters the player never saw.
    bool m_warm_starting = false;
    std::chrono::steady_clock::time_point m_warm_begin; ///< Warm-start wall-clock start, for the timing report.

    ui_state        m_ui;
    ground_layer    m_ground;            ///< BL-732: baked painterly ground for the Planetary canvas.
    /// True while a --verify script drives frames: the ground cache bakes every
    /// chunk synchronously so a capture never races the per-frame bake budget.
    bool            m_ground_bake_all = false;
    recipe_registry m_registry;          ///< Recipes + economy constants, loaded from Lua at startup.
    works_registry  m_works;             ///< BL-321 Era -1 works table, loaded from scripts/works.lua at startup.
    tech_tree_registry m_tech_tree;      ///< BL-087 mock tech/quest tree, loaded from Lua at startup; F9 viewer only.
    economy_report  m_last_econ_report;  ///< Most recent economy-step report; read by the ledgers.
    std::vector<corp_standing> m_last_corp_standings; ///< Per-corp standing profile (BL-262 first slice), recomputed each econ tick from m_last_econ_report's cash flow; read by the Corporations panel. Transient runtime cache — NOT serialised, same as m_last_econ_report.
    ui::chat_state  m_chat;              ///< Comms log state (BL-205): channels, messages, drafts.
    std::vector<persona::pack> m_persona_bench; ///< Seated mountain bench (BL-207 slice 1); empty if load_bench() failed.
    std::unordered_map<entity_id, int> m_counsel_channel; ///< corp -> its lazily-created Counsel chat_channel index.
    uint64_t        m_last_econ_tick = 0; ///< econ_tick() at the previous step; drives the boundary detection in run().
    /// Count of step_economy() calls this campaign — warm start included — and
    /// the value mirrored onto world::current_econ_tick before each step. The
    /// cadence key (BL-568). On load it resumes at pre_game_ticks + envelope
    /// econ_tick, so a loaded campaign rotates exactly as an unsaved one.
    uint64_t        m_econ_steps = 0;
    std::vector<float> m_balance_history;      ///< Recent player balances (one per econ tick, capped); feeds the header net + sparkline.
    std::vector<float> m_income_history;      ///< Recent player income per econ tick (market sales); feeds the Budget ledger's profit chart.
    std::vector<float> m_expenditure_history; ///< Recent player expenditure per econ tick (auto-buys + wages + maintenance); feeds the Budget ledger's profit chart.
    ui::market_plot_history m_market_history; ///< Price / supply / demand history per market, per resource; feeds market ledger graphs.
    std::deque<std::unordered_map<entity_id, int>> m_building_rank_hist; ///< Player-building profit rankings (entity→rank), one snapshot per econ tick, last 5 kept; the oldest is ~4 ticks (a year) back, feeding the Budget ledger's rank-change column (BL-171).

    // Resource-deposit time series (BL-198) — app-owned, capped, UNSERIALISED (like
    // the balance history: outside the save seam, so no format change and no
    // determinism risk; survives the session only). Feeds the Selection band's
    // resource drill-down chart (BL-196/197). The body aggregate is recorded every
    // econ tick; a tile's own series only from the first drill-down into it (lazy).
    ui::resource_history          m_body_resource_hist;  ///< Σ remaining deposit per body per resource, one sample/tick (always).
    ui::resource_history          m_tile_resource_hist;  ///< Remaining deposit per tile per resource, recorded only for tracked (drilled) tiles.
    std::unordered_set<entity_id> m_tracked_tiles;       ///< Tiles whose per-tile series is being recorded (seeded by card drill-downs).
    std::vector<std::uint64_t>    m_resource_hist_days;  ///< In-game day per resource-history sample; the chart's shared X axis (capped in lockstep).
    ui::strategy_readout_state    m_strategy_readout;    ///< Rolling per-corp decision aggregation (BL-411) — verb mix / spend buckets / reason tally over the last 64 quarters. Same lifetime rules as the histories above: presentation-only, unserialised, advanced once per econ tick.
    std::uint64_t                 m_resource_sample_index = 0; ///< Monotonic count of resource-history samples taken. Each econ tick is a quarter, so sample i is dated i·econ_tick_days — equal to day_tick in live play (econ ticks are quarterly) but also progressing when the harness steps the economy without the sim clock.
    entity_id   m_prev_selection = null_entity; ///< selected_entity last frame; a change to a new selection closes any open ledger so the Selection element takes the shared fold-out column.

    bool        m_show_help        = false;   ///< Toggle for the F1 key-binding cheat-sheet overlay.
    bool        m_show_options     = false;   ///< Toggle for the F10 display/options window.
    display_settings m_settings;              ///< Persisted display settings (options.cfg).
    // --- Synthetic pointer input (BL-521), drained by pump_injected_input() ---
    bool  m_pointer_injected = false; ///< Sticky once a script injects: the harness owns the cursor from then on, and re-posts it every frame so settle frames and captures see the same position the click did.
    float m_pointer_x = 0.0f;         ///< Held synthetic cursor position (px), valid while m_pointer_injected.
    float m_pointer_y = 0.0f;
    int   m_inject_button = -1;       ///< Button whose state change is pending for the next frame; -1 = none. Consumed (reset) by pump_injected_input.
    bool  m_inject_down = false;      ///< Down/up for the pending m_inject_button.

    bool        m_capture_requested = false;  ///< Set by F12 / capture_frame, consumed in render().
    std::string m_capture_name;              ///< Base name for the next capture; empty = timestamped (F12).

    // Golden-image diffing (run_verify only; empty m_golden_dir = interactive F12,
    // no compare). See DEVELOPMENT_PRACTICES.md § Visual verification and the verifier-visual skill.
    std::string m_golden_dir;            ///< Directory holding golden reference PNGs (script dir / "golden").
    bool        m_verify_bless = false;  ///< When true, captures overwrite the golden instead of comparing.
    int         m_verify_failures = 0;   ///< Count of captures that failed their golden diff; sets the exit code.

public:
    /// Per-script wall-clock budget for a verify run, in seconds (BL-714 / NR-695).
    ///
    /// A BACKSTOP AGAINST A RUNAWAY, not a performance assertion. `overflow_tile_v2`
    /// swept a row-per-tile ledger view and starved the nine scripts queued behind
    /// it, so a stall cost a whole pass and reported nothing about what it never
    /// reached. Exceeding this raises a Lua error, which BL-423's existing
    /// keep-going path already turns into one red row.
    ///
    /// Set far above any legitimate script so tripping it means a real defect rather
    /// than a slow machine. MEASURED, not guessed: `text_overflow_floor` — the widest
    /// committed check, ~60 captures over every ledger, lens and wizard round — runs
    /// in 174 s on the Debug build here, so this default is a 3.4x margin.
    /// Override with `--verify-budget <seconds>`; 0 disables the watchdog.
    double      m_verify_script_budget_s = 600.0;

private:
    int  m_prev_speed = 1; ///< Speed remembered across a pause, so unpausing restores it.

    double m_last_orbit_days = 0.0; ///< elapsed_days at the previous orbit advance; gives the per-frame delta.
    int    m_last_survey_day = 0;   ///< Whole in-game day at the previous survey advance; drives the per-day survey crossing (BL-067).

    // --- Live agent control seam (BL-412) -----------------------------------
    /// 0 = not hosting (the default — an interactive session grows a network
    /// listener only when --host-agent asks for one). Set via host_agent().
    uint16_t   m_agent_port = 0;
    agent_seam m_agent_seam; ///< The loopback listener + tick-boundary drain.
};

namespace ui { class frame_stats; }

/// Frame-budget instrument (BL-249), shared by render()'s phase marks (app.cpp)
/// and the verify API's frame_reset/frame_csv tap (verify_api.cpp). Defined in
/// app.cpp; declared here so both translation units reach the same instance.
ui::frame_stats& frame_stats_instance();

/// Process-lifetime per-phase accumulators for app::step_economy (ms):
/// [0] convoys, [1] run_economy_step, [2] clear_markets, [3] apply_budget,
/// [4] tech gates, [5] standings+credit, [6] agency comms, [7] persona counsel,
/// [8] history recorders. Dumped by start_new_game's warm-start timing
/// (the 2026-08-12 stall hunt).
///
/// [9] and [10] SPLIT [7] into its two candidate halves (BL-398): the C++
/// blackboard export and the sol2 pack evaluation. [7] stays the total, so
/// [9] + [10] ~= [7] and the residue is the counsel loop's own bookkeeping.
/// The split exists because "counsel is slow" names a phase, not a cause —
/// bounding the wrong half is the failure this project has already paid for.
std::array<double, 11>& step_economy_phase_ms();
