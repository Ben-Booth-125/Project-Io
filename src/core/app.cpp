#include "app.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>

#include "ui/body_surface_canvas.hpp"
#include "ui/circumplanetary_canvas.hpp"
#include "ui/explorer_panel.hpp"
#include "ui/fonts.hpp"
#include "ui/format.hpp"
#include "ui/header_panel.hpp"
#include "ui/nav_pane.hpp"
#include "ui/overlay.hpp"
#include "ui/profile_panel.hpp"
#include "ui/solar_system_canvas.hpp"
#include "ui/tile_inspector.hpp"
#include "ui/view_nav.hpp"
#include "world/hard_coded_world.hpp"
#include "world/orbital_system.hpp"

#include <algorithm>
#include <cmath>
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

    // Load the UI font with oversampling so on-canvas labels stay crisp at the
    // fractional positions moving bodies produce. The renderer backend uploads
    // the atlas on the first NewFrame.
    ui::load_ui_font();
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

    // Set the initial solar zoom so the default view covers roughly 5 AU.
    // The auto-fit scale at zoom 1 shows max_radius_au; dividing it by 5 zooms
    // in so that 5 AU fills the same screen extent.
    {
        float max_radius_au = 0.0f;
        for (const auto& [id, body] : m_world.bodies)
            max_radius_au = std::max(max_radius_au, body.orbital_radius_au);
        if (max_radius_au > 0.0f)
            m_ui.solar_zoom = max_radius_au / 5.0f;
    }

    // Open on the corporation's home planet (its surface). Fall back to the
    // lowest-id tiled body, then any body, if no home is set.
    entity_id start_body = m_world.home_body;
    if (start_body == null_entity && !m_world.bodies.empty())
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
        start_body = (tiled != null_entity) ? tiled : fallback;
    }

    // The game opens looking at the home planet's surface — the bottom rung.
    // Routed through the shared focus helper rather than poking ui_state.
    ui::focus_on_surface(m_world, m_ui, start_body);

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

    // --- Primary canvases (Layer 2) — the zoom ladder ---
    // The primary rung fills the window; the rung one step *out* renders in the
    // minimap inset (bottom-right), framed by chrome (title bar + mode bar).
    // Both draw to the ImGui background draw list, so the panels below render on
    // top. See CANVASES.md / MINIMAP.md.
    {
        // Minimap chrome reserves a title bar above the inset and a placeholder
        // mode bar below; the neighbouring canvas occupies the space between.
        const float  title_h      = ImGui::GetTextLineHeight() + 6.0f;
        const float  mode_h       = 14.0f; // tall enough for the overlay mode dots to be clickable
        const ImVec2 inset_origin = { mm_origin.x, mm_origin.y + title_h };
        const ImVec2 inset_size   = { mm_w, mm_h - title_h - mode_h };

        // Route input: the whole minimap box blocks the primary behind it; only
        // the inset canvas receives minimap input. An ImGui panel under the
        // cursor (WantCaptureMouse) takes precedence over both.
        const ImVec2 mp = io.MousePos;
        const bool mouse_in_mm =
            mp.x >= mm_origin.x && mp.x <= mm_origin.x + mm_w &&
            mp.y >= mm_origin.y && mp.y <= mm_origin.y + mm_h;
        const bool mouse_in_inset =
            mp.x >= inset_origin.x && mp.x <= inset_origin.x + inset_size.x &&
            mp.y >= inset_origin.y && mp.y <= inset_origin.y + inset_size.y;
        const bool panel_blocking = io.WantCaptureMouse;
        const bool primary_input  = !mouse_in_mm && !panel_blocking;
        const bool minimap_input  = mouse_in_inset && !panel_blocking;

        // Draw the primary rung full-window, then the zoom-out neighbour into the
        // inset. The minimap title names what the inset shows.
        const char* mm_title = "Project Io";
        switch (m_ui.primary_level)
        {
            case canvas_level::solar:
                ui::draw_solar_system_canvas(m_world, m_ui, {0.0f, 0.0f}, disp, primary_input, false);
                // No rung above solar — the minimap is game branding (game name).
                break;

            case canvas_level::circumplanetary:
                ui::draw_circumplanetary_canvas(m_world, m_ui, {0.0f, 0.0f}, disp, primary_input, false);
                ui::draw_solar_system_canvas(m_world, m_ui, inset_origin, inset_size, minimap_input, true);
                if (m_world.star_body != null_entity && m_world.bodies.count(m_world.star_body))
                    mm_title = m_world.bodies.at(m_world.star_body).name.c_str();
                break;

            case canvas_level::planetary:
                ui::draw_body_surface_canvas(m_world, m_ui, {0.0f, 0.0f}, disp, primary_input);
                ui::draw_circumplanetary_canvas(m_world, m_ui, inset_origin, inset_size, minimap_input, true);
                {
                    const entity_id anchor = ui::circumplanetary_anchor(m_world, m_ui.active_body);
                    if (anchor != null_entity && m_world.bodies.count(anchor))
                        mm_title = m_world.bodies.at(anchor).name.c_str();
                }
                break;
        }

        // Overlay pass — drawn over the primary canvas (full window), below the
        // minimap chrome. A no-op unless an overlay lens is active.
        ui::draw_canvas_overlay(m_world, m_ui, m_ui.primary_level,
                                {0.0f, 0.0f}, disp, ImGui::GetBackgroundDrawList());

        // --- Minimap chrome, drawn on top of the inset ---
        ImDrawList* bdl = ImGui::GetBackgroundDrawList();

        // At the top rung the inset has no canvas — fill it as a dark branding
        // placeholder so the full-window solar canvas does not show through.
        if (m_ui.primary_level == canvas_level::solar)
            bdl->AddRectFilled(inset_origin,
                               {inset_origin.x + inset_size.x, inset_origin.y + inset_size.y},
                               IM_COL32(8, 10, 20, 255));

        // Title bar.
        bdl->AddRectFilled(mm_origin, {mm_origin.x + mm_w, mm_origin.y + title_h}, IM_COL32(28, 30, 40, 255));
        bdl->AddText({mm_origin.x + 5.0f, mm_origin.y + 3.0f}, IM_COL32(220, 225, 235, 255), mm_title);

        // Mode bar — toggles the canvas overlay lens. Three dots map to the
        // three overlay modes; the active mode's dot lights up, and clicking it
        // again clears the overlay. No visible effect until later layers add
        // overlay data, but the building block (ui_state.overlay + the overlay
        // pass above) is wired now. See MINIMAP.md (mode bar) and overlay.hpp.
        const float mb_y = mm_origin.y + mm_h - mode_h;
        bdl->AddRectFilled({mm_origin.x, mb_y}, {mm_origin.x + mm_w, mm_origin.y + mm_h}, IM_COL32(20, 22, 30, 255));

        constexpr overlay_mode mode_dots[3] = {
            overlay_mode::supply, overlay_mode::market, overlay_mode::faction };
        constexpr float dot_spacing = 16.0f;
        const float     dot_cy      = mb_y + mode_h * 0.5f;
        const float     dot_cx0     = mm_origin.x + mm_w * 0.5f - dot_spacing; // centre dot is index 1

        // Resolve the dot under the cursor (nearest within half a spacing),
        // gated like the canvases on an ImGui panel not capturing the mouse.
        int hovered_dot = -1;
        if (!panel_blocking && mp.y >= mb_y && mp.y <= mm_origin.y + mm_h &&
            mp.x >= mm_origin.x && mp.x <= mm_origin.x + mm_w)
        {
            float best = dot_spacing * 0.5f;
            for (int i = 0; i < 3; ++i)
            {
                const float dx = std::fabs(mp.x - (dot_cx0 + static_cast<float>(i) * dot_spacing));
                if (dx <= best) { best = dx; hovered_dot = i; }
            }
        }

        for (int i = 0; i < 3; ++i)
        {
            const ImVec2 c       = { dot_cx0 + static_cast<float>(i) * dot_spacing, dot_cy };
            const bool   active  = (m_ui.overlay == mode_dots[i]);
            const ImU32  col     = active        ? IM_COL32(235, 220, 140, 255)
                                 : (i == hovered_dot) ? IM_COL32(150, 156, 175, 255)
                                                      : IM_COL32(70, 75, 90, 255);
            bdl->AddCircleFilled(c, active ? 3.0f : 2.0f, col);
        }

        if (hovered_dot >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            ui::toggle_overlay(m_ui, mode_dots[hovered_dot]);

        // Border around the whole minimap box.
        bdl->AddRect(mm_origin, {mm_origin.x + mm_w, mm_origin.y + mm_h}, IM_COL32(90, 95, 110, 255));
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
        // Player-facing calendar derived from the raw day count, with the
        // in-year quarter alongside the absolute day for continuity.
        const uint64_t            day  = m_sim_loop.day_tick();
        const ui::fmt::calendar_date date = ui::fmt::date_from_day(day);
        ImGui::Text("%s", ui::fmt::short_date(day).c_str());
        ImGui::Text("Q%d  -  Day %llu", date.quarter, static_cast<unsigned long long>(day));
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
        // Pause label flips to a play symbol when paused so it reflects the toggle state.
        const char* labels[] = {m_sim_loop.paused() ? ">" : "II", "1", "2", "3", "4", "5"};
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
            {
                if (speeds[i] == 0)
                {
                    // Pause button toggles: if already paused, restore the
                    // previous speed; otherwise save the current speed and pause.
                    if (m_sim_loop.paused())
                        m_sim_loop.set_speed(m_prev_speed);
                    else
                    {
                        m_prev_speed = m_sim_loop.speed();
                        m_sim_loop.set_speed(0);
                    }
                }
                else
                {
                    // Speed button: remember it so pause can restore it later.
                    m_prev_speed = speeds[i];
                    m_sim_loop.set_speed(speeds[i]);
                }
            }
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
