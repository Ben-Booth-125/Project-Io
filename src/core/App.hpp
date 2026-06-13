#pragma once

#include <SDL3/SDL.h>
#include "sim_loop.hpp"
#include "scripting/lua_state.hpp"
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

    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    sim_loop  m_sim_loop;
    lua_state m_lua;
    world     m_world;
};
