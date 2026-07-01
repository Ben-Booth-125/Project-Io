#pragma once

#include <SDL3/SDL.h>
#include "sim_loop.hpp"
#include "scripting/lua_state.hpp"
#include "ui/ui_state.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include "ui/canvas_command.hpp"
#include "ui/plot_history.hpp"

#include <cstdint>
#include <string>
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
    int run();

    /// Run a non-interactive visual-verification session: set up a deterministic
    /// world (seeded, sim paused), expose the `verify` Lua API (which drives view
    /// and overlay state directly and captures named PNG frames), execute the
    /// script, then exit. The driver of the project's `visual` requirement class.
    /// See docs/development/DEVELOPMENT_PRACTICES.md and TODO § Canvas.
    ///
    /// @param script_path Path to the .lua verification script.
    /// @param bless       When true, each capture is written into the golden
    ///                    directory (regenerating references) instead of being
    ///                    compared against an existing golden.
    /// @return 0 on a clean run with no golden-diff failures; non-zero if the
    ///         script failed to load/execute or any capture failed its golden diff.
    int run_verify(const std::string& script_path, bool bless = false);

private:
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

    /// Draw the main menu — the deliberate entry point shown at launch (no world
    /// loaded yet). Wires the "New Game" button to start_new_game() and "Quit" to
    /// m_quit_requested. Called from render() when m_screen == menu; folded into
    /// render()'s single capture path so the menu is golden-verifiable.
    void draw_main_menu();

    /// Start a fresh campaign from the menu: reset the sim clock, build the world,
    /// load the economy, run the pre-game warm start, and switch to the in-game
    /// screen. Everything run() used to do inline before its loop.
    void start_new_game();

    /// Build the prototype world and frame the opening view. Shared by run() and
    /// run_verify() so both start from the same deterministic state.
    void setup_world();

    /// Load the economy Lua data layer (scripts/recipes.lua + scripts/economy.lua)
    /// into m_registry, then author processing recipes onto the generated assets
    /// (the recipe id is a registry index, so it is assigned here, not at
    /// generation). Shared by run() and run_verify().
    void load_economy();

    /// Run one economy tick: production → market clearing → budget, storing the
    /// per-building report for the economy panel. Driven by the econ-tick boundary
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
        int  window_w   = 1280;
        int  window_h   = 720;
        bool fullscreen = false;  ///< Borderless-desktop fullscreen.
        bool vsync      = true;
    };

    /// Read options.cfg into m_settings; a missing/partial file leaves defaults.
    void load_settings();
    /// Write m_settings to options.cfg.
    void save_settings() const;
    /// Apply m_settings to the live SDL window + renderer (size, fullscreen, vsync).
    void apply_display_settings();

    /// Which top-level screen is active. run() opens on the menu; run_verify()
    /// jumps straight to in_game (the harness renders the live world, not the menu,
    /// unless a script asks for it via verify.show_menu).
    enum class app_screen { menu, in_game };
    app_screen m_screen = app_screen::menu;
    bool       m_quit_requested = false;  ///< Set by the menu's Quit button; breaks the run() loop.

    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    sim_loop        m_sim_loop;
    lua_state       m_lua;
    world           m_world;
    ui_state        m_ui;
    recipe_registry m_registry;          ///< Recipes + economy constants, loaded from Lua at startup.
    economy_report  m_last_econ_report;  ///< Most recent economy-step report; read by the economy panel.
    uint64_t        m_last_econ_tick = 0; ///< econ_tick() at the previous step; drives the boundary detection in run().
    std::vector<float> m_balance_history;      ///< Recent player balances (one per econ tick, capped); feeds the header net + sparkline.
    std::vector<float> m_income_history;      ///< Recent player income per econ tick (market sales); feeds economy panel graph.
    std::vector<float> m_expenditure_history; ///< Recent player expenditure per econ tick (auto-buys + wages + maintenance); feeds economy panel graph.
    ui::market_plot_history m_market_history; ///< Price / supply / demand history per market, per resource; feeds market ledger graphs.

    bool        m_show_help        = false;   ///< Toggle for the F1 key-binding cheat-sheet overlay.
    bool        m_show_options     = false;   ///< Toggle for the F10 display/options window.
    display_settings m_settings;              ///< Persisted display settings (options.cfg).
    bool        m_capture_requested = false;  ///< Set by F12 / capture_frame, consumed in render().
    std::string m_capture_name;              ///< Base name for the next capture; empty = timestamped (F12).

    // Golden-image diffing (run_verify only; empty m_golden_dir = interactive F12,
    // no compare). See OPENS § Canvas [F3] and the verifier-visual skill.
    std::string m_golden_dir;            ///< Directory holding golden reference PNGs (script dir / "golden").
    bool        m_verify_bless = false;  ///< When true, captures overwrite the golden instead of comparing.
    int         m_verify_failures = 0;   ///< Count of captures that failed their golden diff; sets the exit code.
    int  m_prev_speed = 1; ///< Speed remembered across a pause, so unpausing restores it.

    double m_last_orbit_days = 0.0; ///< elapsed_days at the previous orbit advance; gives the per-frame delta.
    int    m_last_survey_day = 0;   ///< Whole in-game day at the previous survey advance; drives the per-day survey crossing (BL-067).
};
