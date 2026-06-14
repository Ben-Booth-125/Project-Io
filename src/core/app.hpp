#pragma once

#include <SDL3/SDL.h>
#include "sim_loop.hpp"
#include "scripting/lua_state.hpp"
#include "ui/ui_state.hpp"
#include "world/world.hpp"

#include <string>

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
    /// @return 0 on a clean run; non-zero if the script failed to load/execute.
    int run_verify(const std::string& script_path);

private:
    void process_events(bool& running);

    /// Map a key-down event onto the shared canvas command vocabulary (see
    /// docs/ui/CANVASES.md § Keyboard) and apply it, or trigger a capture (F12).
    /// Navigation keys are ignored while ImGui is capturing the keyboard.
    ///
    /// @param key The SDL key-down event.
    void handle_key_down(const SDL_KeyboardEvent& key);

    void render();

    /// Build the prototype world and frame the opening view. Shared by run() and
    /// run_verify() so both start from the same deterministic state.
    void setup_world();

    /// Render exactly one frame and write it to screenshots/<name>.png. Used by
    /// the verify Lua API's capture() to grab a deterministic frame on demand.
    ///
    /// @param name Base file name (no extension) for the capture.
    void capture_frame(const std::string& name);

    /// Read the current backbuffer and write it to screenshots/ as a PNG.
    /// Triggered by F12 (timestamped name) or by capture_frame (named); called at
    /// the end of render(), after the frame is composited but before present.
    void save_screenshot();

    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    sim_loop  m_sim_loop;
    lua_state m_lua;
    world     m_world;
    ui_state  m_ui;

    bool        m_capture_requested = false; ///< Set by F12 / capture_frame, consumed in render().
    std::string m_capture_name;              ///< Base name for the next capture; empty = timestamped (F12).
    int  m_prev_speed = 1; ///< Speed remembered across a pause, so unpausing restores it.

    double m_last_orbit_days = 0.0; ///< elapsed_days at the previous orbit advance; gives the per-frame delta.
};
