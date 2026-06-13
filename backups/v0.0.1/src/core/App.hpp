#pragma once

#include <SDL3/SDL.h>
#include "sim_loop.hpp"
#include "scripting/lua_state.hpp"
#include "ui/ui_state.hpp"
#include "world/world.hpp"

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

private:
    void process_events(bool& running);
    void render();

    /// Read the current backbuffer and write it to screenshots/ as a BMP.
    /// Triggered by F12; called at the end of render(), after the frame is
    /// composited but before present.
    void save_screenshot();

    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    sim_loop  m_sim_loop;
    lua_state m_lua;
    world     m_world;
    ui_state  m_ui;

    bool m_capture_requested = false; ///< Set by F12, consumed in render().

    double m_last_orbit_days = 0.0; ///< elapsed_days at the previous orbit advance; gives the per-frame delta.
};
