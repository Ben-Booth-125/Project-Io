#include "app.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>

#include "ui/tile_inspector.hpp"
#include "world/hard_coded_world.hpp"

static constexpr int window_w = 1280;
static constexpr int window_h = 720;

app::app()
{
    SDL_Init(SDL_INIT_VIDEO);

    m_window   = SDL_CreateWindow("Project Io", window_w, window_h, SDL_WINDOW_RESIZABLE);
    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    SDL_SetRenderVSync(m_renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(m_window, m_renderer);
    ImGui_ImplSDLRenderer3_Init(m_renderer);
}

app::~app()
{
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

int app::run()
{
    m_lua.load("scripts/init.lua");
    m_world = make_hard_coded_world();

    bool running = true;
    while (running)
    {
        process_events(running);
        m_sim_loop.tick();
        render();
    }
    return 0;
}

void app::process_events(bool& running)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT)
            running = false;
    }
}

void app::render()
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Engine status — top-left ticker confirming both loops and Lua are alive.
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_Once);
    ImGui::Begin("Engine Status");
    ImGui::Text("Sim tick:   %llu", m_sim_loop.sim_tick());
    ImGui::Text("Econ tick:  %llu", m_sim_loop.econ_tick());
    ImGui::Text("Lua:        alive");
    ImGui::End();

    ui::draw_tile_inspector(m_world);

    ImGui::Render();
    SDL_SetRenderDrawColor(m_renderer, 15, 15, 20, 255);
    SDL_RenderClear(m_renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_renderer);
    SDL_RenderPresent(m_renderer);
}
