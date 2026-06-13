#include "app.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>

#include "ui/body_surface_canvas.hpp"
#include "ui/explorer_panel.hpp"
#include "ui/header_panel.hpp"
#include "ui/nav_pane.hpp"
#include "ui/profile_panel.hpp"
#include "ui/solar_system_canvas.hpp"
#include "ui/tile_inspector.hpp"
#include "world/hard_coded_world.hpp"
#include "world/orbital_system.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

static constexpr int window_w = 1280;
static constexpr int window_h = 720;

app::app()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());

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

    // Apply Lua config now that the script has run. Constructing a fresh
    // sim_loop here resets its internal timer to the current wall clock, which
    // is the right start point — not app construction time.
    {
        sol::table cfg = m_lua.state()["config"];
        m_sim_loop = sim_loop();
        m_sim_loop.set_speed(cfg.get_or("default_speed", 1));
    }

    m_world = make_hard_coded_world();

    // Default the surface canvas to a body that has authored tiles, so it shows
    // a populated surface before the player clicks anything. Many bodies are
    // backdrop-only (no tiles); prefer the lowest-id tiled body, falling back to
    // the lowest-id body of any kind.
    if (!m_world.bodies.empty())
    {
        entity_id fallback = m_world.bodies.begin()->first;
        entity_id tiled    = null_entity;
        for (const auto& [id, _] : m_world.bodies)
        {
            fallback = std::min(fallback, id);

            const bool has_tiles = std::any_of(
                m_world.tiles.begin(), m_world.tiles.end(),
                [id](const auto& kv) { return kv.second.body == id; });
            if (has_tiles && (tiled == null_entity || id < tiled))
                tiled = id;
        }
        m_ui.active_body = (tiled != null_entity) ? tiled : fallback;
    }

    bool running = true;
    while (running)
    {
        process_events(running);
        m_sim_loop.tick();

        // Advance orbital motion by the in-game days elapsed this frame. Freezes
        // automatically while paused, since elapsed_days() stops advancing.
        const double now_days = m_sim_loop.elapsed_days();
        advance_orbits(m_world, now_days - m_last_orbit_days);
        m_last_orbit_days = now_days;

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
        else if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_F12)
            m_capture_requested = true;
    }
}

void app::render()
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Shared window geometry. The minimap and the top-right time panels are all
    // sized off the minimap width so the right-hand column stays aligned.
    ImGuiIO&     io     = ImGui::GetIO();
    const ImVec2 disp   = io.DisplaySize;
    constexpr float margin = 8.0f;
    const float  mm_w   = std::max(240.0f, 0.20f * std::min(disp.x, disp.y));
    const float  mm_h   = mm_w * 0.75f; // keep the 4:3 ratio of the 240x180 default
    const ImVec2 mm_origin = {disp.x - margin - mm_w, disp.y - margin - mm_h};
    const ImVec2 mm_size   = {mm_w, mm_h};

    // --- Primary canvases (Layer 2) ---
    // The active canvas fills the window; the inactive one is a fixed inset in
    // the bottom-right corner. Both draw to the ImGui background draw list, so
    // the debug panels below render on top of them.
    {
        // Route input to whichever region the mouse is over; an ImGui panel
        // under the cursor (WantCaptureMouse) takes precedence over both.
        const ImVec2 mp = io.MousePos;
        const bool mouse_in_mm =
            mp.x >= mm_origin.x && mp.x <= mm_origin.x + mm_w &&
            mp.y >= mm_origin.y && mp.y <= mm_origin.y + mm_h;
        const bool panel_blocking = io.WantCaptureMouse;
        const bool primary_input = !mouse_in_mm && !panel_blocking;
        const bool minimap_input = mouse_in_mm && !panel_blocking;

        // Draw the primary canvas first, then the minimap on top of it.
        if (!m_ui.surface_is_primary)
        {
            ui::draw_solar_system_canvas(m_world, m_ui, {0.0f, 0.0f}, disp, primary_input);
            ui::draw_body_surface_canvas(m_world, m_ui, mm_origin, mm_size, minimap_input);
        }
        else
        {
            ui::draw_body_surface_canvas(m_world, m_ui, {0.0f, 0.0f}, disp, primary_input);
            ui::draw_solar_system_canvas(m_world, m_ui, mm_origin, mm_size, minimap_input);
        }

        // A thin border marks the minimap region.
        ImGui::GetBackgroundDrawList()->AddRect(
            mm_origin, {mm_origin.x + mm_w, mm_origin.y + mm_h}, IM_COL32(90, 95, 110, 255));
    }

    // System tick — a permanent, fixed readout pinned to the top-right corner.
    // Same width as the minimap, about a third of its height. Non-interactive
    // so clicks pass through to the canvas behind it.
    const float tick_w = mm_w;
    const float tick_h = mm_h / 3.0f;
    {
        ImGui::SetNextWindowPos({disp.x - margin - tick_w, margin});
        ImGui::SetNextWindowSize({tick_w, tick_h});
        constexpr ImGuiWindowFlags tick_flags =
            ImGuiWindowFlags_NoTitleBar          |
            ImGuiWindowFlags_NoResize            |
            ImGuiWindowFlags_NoMove              |
            ImGuiWindowFlags_NoCollapse          |
            ImGuiWindowFlags_NoScrollbar         |
            ImGuiWindowFlags_NoNav               |
            ImGuiWindowFlags_NoInputs            |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoSavedSettings;
        ImGui::Begin("##system_tick", nullptr, tick_flags);
        ImGui::Text("Day   %llu", m_sim_loop.day_tick());
        ImGui::Text("Econ  %llu", m_sim_loop.econ_tick());
        ImGui::End();
    }

    // Time speed controls — directly below the tick readout, same width.
    {
        const float ctrl_h = mm_h / 3.0f;
        ImGui::SetNextWindowPos({disp.x - margin - tick_w, margin + tick_h + 4.0f});
        ImGui::SetNextWindowSize({tick_w, ctrl_h});
        constexpr ImGuiWindowFlags ctrl_flags =
            ImGuiWindowFlags_NoTitleBar          |
            ImGuiWindowFlags_NoResize            |
            ImGuiWindowFlags_NoMove              |
            ImGuiWindowFlags_NoCollapse          |
            ImGuiWindowFlags_NoScrollbar         |
            ImGuiWindowFlags_NoNav               |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoSavedSettings;
        ImGui::Begin("##time_controls", nullptr, ctrl_flags);

        ImGui::Text("Sim %llu", m_sim_loop.sim_tick());
        ImGui::SameLine();
        if (m_sim_loop.paused())
            ImGui::TextDisabled("(paused)");
        else
            ImGui::TextDisabled("(%dx)", m_sim_loop.speed());

        // Pause plus 1x..5x. The active speed is highlighted.
        const char* labels[] = {"II", "1", "2", "3", "4", "5"};
        const int   speeds[] = { 0,    1,   2,   3,   4,   5 };
        const int   n        = 6;
        const float spacing  = ImGui::GetStyle().ItemSpacing.x;
        const float bw       = (ImGui::GetContentRegionAvail().x - spacing * (n - 1)) / n;

        for (int i = 0; i < n; ++i)
        {
            const bool active = (m_sim_loop.speed() == speeds[i]);
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(labels[i], {bw, 0.0f}))
                m_sim_loop.set_speed(speeds[i]);
            if (active)
                ImGui::PopStyleColor();
            if (i + 1 < n)
                ImGui::SameLine();
        }

        ImGui::End();
    }

    // Corporation profile — top-left corner, above the navigation pane.
    ui::draw_profile_panel();

    // Budget + resource header — spans the top between the profile and the
    // time column, clear of both.
    {
        const float header_left  = ui::nav_pane_width;
        const float header_right = (disp.x - margin - tick_w) - margin;
        ui::draw_header_panel(header_left, header_right);
    }

    // Explorer — right edge, between the time column and the minimap.
    {
        const float ctrl_h        = mm_h / 3.0f;
        const float column_bottom = margin + tick_h + 4.0f + ctrl_h;
        const float exp_x         = mm_origin.x;
        const float exp_y         = column_bottom + margin;
        const float exp_w         = mm_w;
        const float exp_h         = (mm_origin.y - margin) - exp_y;
        ui::draw_explorer_panel(exp_x, exp_y, exp_w, exp_h);
    }

    // Left navigation pane and the menus it opens. Starts below the profile.
    ui::draw_nav_pane(m_ui, ui::profile_panel_height);
    ui::draw_tile_inspector(m_world, &m_ui.show_tile_ledger);

    ImGui::Render();
    SDL_SetRenderDrawColor(m_renderer, 15, 15, 20, 255);
    SDL_RenderClear(m_renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_renderer);

    // Capture before present so the screenshot is the exact composited frame.
    if (m_capture_requested)
    {
        save_screenshot();
        m_capture_requested = false;
    }

    SDL_RenderPresent(m_renderer);
}

void app::save_screenshot()
{
    SDL_Surface* surface = SDL_RenderReadPixels(m_renderer, nullptr);
    if (!surface)
    {
        SDL_Log("Screenshot failed: %s", SDL_GetError());
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories("screenshots", ec);

    char path[128];
    std::snprintf(path, sizeof(path), "screenshots/io_%llu.bmp",
        static_cast<unsigned long long>(SDL_GetTicks()));

    if (SDL_SaveBMP(surface, path))
        SDL_Log("Screenshot saved: %s", path);
    else
        SDL_Log("Screenshot save failed: %s", SDL_GetError());

    SDL_DestroySurface(surface);
}
