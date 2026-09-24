#include "app.hpp"

#include "png_writer.hpp"
#include "save_game.hpp" // BL-536 quick save / quick load
#include "session_history.hpp" // step_economy's post-step presentation (BL-361)

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>

#include "ui/body_surface_canvas.hpp"
#include "ui/canvas_command.hpp"
#include "ui/charts.hpp"
#include "ui/circumplanetary_canvas.hpp"
#include "ui/construction_panel.hpp"
#include "ui/generation_wait.hpp" // BL-1072: the one wait surface
#include "ui/acquisitions_ledger.hpp" // nav slot 5, the Acquisitions ledger + its profitability fold-out
#include "ui/company_ledger.hpp"   // Company-lens click destination, no rail slot (BL-666)
#include "ui/detail_level.hpp" // the drill-through fold idiom (BL-214)
#include "ui/balance_ledger.hpp"
#include "ui/corporation_dashboard.hpp" // nav slot 1, the four roll-ups (BL-248)
#include "ui/corporation_panel.hpp"     // all-corporations table, restored to slot 8 (NR-012)
#include "ui/convoys_ledger.hpp"
#include "ui/market_ledger.hpp"
#include "ui/chat_panel.hpp"
#include "ui/fonts.hpp"
#include "ui/generation_charts.hpp" // the shared chain-stage charts (BL-211)
#include "ui/decision_feed.hpp"     // the AI decision feed (BL-407)
#include "ui/generation_ledger.hpp" // the generation tuning ledger (BL-303)
#include "ui/generation_preview.hpp" // the wizard's painted right pane
#include "ui/format.hpp"
#include "ui/frame_stats.hpp" // frame-budget HUD (BL-249, v0.1.0 quality audit)
#include "ui/header_panel.hpp"
#include "ui/foldout_column.hpp" // shell_column_width — permanent left shell column (BL-122)
#include "ui/shell_metrics.hpp"  // the shell's rect algebra, one owner (BL-216)
#include "ui/nav_pane.hpp"
#include "ui/overlay.hpp"
#include "ui/presentation.hpp"
#include "ui/profile_panel.hpp"
#include "ui/selection.hpp"
#include "ui/selection_card.hpp"
#include "ui/selection_panel.hpp"
#include "ui/solar_system_canvas.hpp"
#include "ui/star_map_view.hpp"
#include "ui/tech_tree_panel.hpp"
#include "ui/time_panel.hpp" // time panel + system menu, extracted from render() (BL-361)
#include "ui/text_fit.hpp" // overflow ledger + verify.clipping bindings (BL-215)
#include "ui/tile_inspector.hpp"
#include "ui/view_nav.hpp"
#include "world/budget_system.hpp"
#include "world/nation_step.hpp"
#include "world/construction.hpp"
#include "world/tech_gate.hpp" // BL-344: gating_tech_for (refusal names the missing tech)
#include "world/corp_ai.hpp"
#include "world/corporation_generation.hpp"
#include "world/history_log.hpp"
#include "world/placement_rules.hpp"
#include "world/survey_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/landscape_search.hpp"
#include "world/stockpile_budget.hpp" // BL-1042: the new-game charter budget

#include <chrono> // poll_wizard_surface's zero-wait future probe
#include <thread> // join_workers' wait (BL-1108)
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/orbital_system.hpp"
#include "world/resource_names.hpp" // BL-652: name the good in the unpriced-basket warning
#include "world/supply_system.hpp"

#include <algorithm>
#include <future>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

static constexpr int window_w = 1720;
static constexpr int window_h = 1080;

// ---------------------------------------------------------------------------
// Unified key-binding table (BL-062).
// Every keyboard shortcut is defined once here.  handle_key_down loops over
// this table and routes through dispatch_action, so the F1 help overlay is
// generated from the same table and can never drift.  F12 is excluded because
// it needs the renderer and is handled first in handle_key_down.
// ---------------------------------------------------------------------------
struct key_binding
{
    SDL_Scancode       scancode;
    bool               shift;    ///< true = requires Shift modifier
    ui::canvas_command cmd;
    const char*        label;    ///< Human-readable action name (F1 overlay)
    const char*        key_name; ///< Human-readable key string (F1 overlay)
};

static const std::array<key_binding, 24> s_bindings = {{
    // Canvas navigation
    {SDL_SCANCODE_RETURN,       false, ui::canvas_command::descend,      "Descend rung",      "Enter"},
    {SDL_SCANCODE_BACKSPACE,    false, ui::canvas_command::ascend,       "Ascend rung",       "Backspace"},
    {SDL_SCANCODE_RIGHTBRACKET, false, ui::canvas_command::body_next,    "Next body",         "]"},
    {SDL_SCANCODE_LEFTBRACKET,  false, ui::canvas_command::body_prev,    "Previous body",     "["},
    {SDL_SCANCODE_LEFT,         false, ui::canvas_command::pan_left,     "Pan left",          "←"},
    {SDL_SCANCODE_RIGHT,        false, ui::canvas_command::pan_right,    "Pan right",         "→"},
    {SDL_SCANCODE_UP,           false, ui::canvas_command::pan_up,       "Pan up",            "↑"},
    {SDL_SCANCODE_DOWN,         false, ui::canvas_command::pan_down,     "Pan down",          "↓"},
    {SDL_SCANCODE_EQUALS,       false, ui::canvas_command::zoom_in,      "Zoom in",           "="},
    {SDL_SCANCODE_MINUS,        false, ui::canvas_command::zoom_out,     "Zoom out",          "-"},
    // Lens
    {SDL_SCANCODE_L,            false, ui::canvas_command::lens_next,    "Next lens",         "L"},
    {SDL_SCANCODE_L,            true,  ui::canvas_command::lens_prev,    "Previous lens",     "Shift+L"},
    {SDL_SCANCODE_0,            false, ui::canvas_command::lens_clear,   "Clear lens",        "0"},
    // Time controls
    {SDL_SCANCODE_SPACE,        false, ui::canvas_command::pause_toggle, "Pause / Resume",    "Space"},
    {SDL_SCANCODE_1,            false, ui::canvas_command::speed_1,      "Speed I  (0.25×)",  "1"},
    {SDL_SCANCODE_2,            false, ui::canvas_command::speed_2,      "Speed II (0.5×)",   "2"},
    {SDL_SCANCODE_3,            false, ui::canvas_command::speed_3,      "Speed III (1×)",    "3"},
    {SDL_SCANCODE_4,            false, ui::canvas_command::speed_4,      "Speed IV (4×)",     "4"},
    {SDL_SCANCODE_5,            false, ui::canvas_command::speed_5,      "Speed V  (16×)",    "5"},
    // UI
    {SDL_SCANCODE_F5,           false, ui::canvas_command::quick_save,   "Quick save",        "F5"},
    {SDL_SCANCODE_F6,           false, ui::canvas_command::quick_load,   "Quick load",        "F6"},
    {SDL_SCANCODE_F1,           false, ui::canvas_command::help_toggle,  "Key bindings",      "F1"},
    {SDL_SCANCODE_F9,           false, ui::canvas_command::tech_tree_toggle, "Tech tree (mock)", "F9"},
    {SDL_SCANCODE_F10,          false, ui::canvas_command::options_toggle, "Options",         "F10"},
}};

// main.cpp declares `app a;` as a local, so the whole object lives on the main
// thread's 1 MB stack. BL-305's carve sink put ~92 KB of atomics inside it
// (312 x 145 int16 owners), which is fine but is the kind of growth that ends
// in a stack overflow at startup rather than a compile error. Half the stack is
// the bar; if this ever trips, heap-allocate the sink rather than raising it.
//
// THIS BAR IS LOOSER THAN THE REAL BUDGET, measured 2026-09-09 (BL-860). `main`
// declares `app a;` in SIX separate scopes, and MSVC in Debug does not overlap
// the frames of sibling scopes — so every byte here is charged to the 1 MB stack
// six times over. A second inline `generation_progress` (66,080 bytes) took
// `sizeof(app)` from 152,728 to 218,808 and the process died at startup with
// 0xC00000FD, having printed nothing, while this assert stayed green. The repair
// was the one the paragraph above prescribes — the wizard's progress blocks moved
// to the heap (app.hpp, m_wiz_history_progress) — but the assert is left as it
// stands rather than re-derived from a `main` this file cannot see. Read it as a
// smoke alarm, not a budget: the true ceiling is nearer 1 MB / 6.
static_assert(sizeof(app) < 512u * 1024u,
              "app is approaching the main thread's stack budget");

app::app()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());

    m_window   = SDL_CreateWindow("Project Io", window_w, window_h, SDL_WINDOW_RESIZABLE);

    // BL-215 display contract: the smallest supported display is 1280x720 at UI
    // scale 1.0x (DEVELOPMENT_PRACTICES.md § Display environment). Below it the
    // shell fails soft (zero-width Selection band, header early-out), so the OS
    // is told not to go there. apply_ui_scale re-applies this scaled by the
    // active UI-scale step.
    SDL_SetWindowMinimumSize(m_window, 1280, 720);

    // Dev-machine preference (2026-07-06): Ben runs a two-monitor setup and wants
    // the game on the secondary display, keeping the primary free. Pick the first
    // enumerated display that is NOT the primary (SDL_WINDOWPOS_CENTERED_DISPLAY
    // didn't reliably land on the physical second monitor - Windows' monitor
    // numbering doesn't necessarily match SDL's enumeration order) and place the
    // window at that display's bounds origin directly. No-op on one monitor.
    {
        int display_count = 0;
        SDL_DisplayID* displays = SDL_GetDisplays(&display_count);
        const SDL_DisplayID primary = SDL_GetPrimaryDisplay();
        if (displays)
        {
            for (int i = 0; i < display_count; ++i)
            {
                if (displays[i] == primary)
                    continue;
                SDL_Rect bounds{};
                if (SDL_GetDisplayBounds(displays[i], &bounds))
                    SDL_SetWindowPosition(m_window, bounds.x + 40, bounds.y + 40);
                break;
            }
        }
        SDL_free(displays);
    }

    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    SDL_SetRenderVSync(m_renderer, 1);

    // Record the display environment on startup so the runtime resolution is on the
    // log. Verify captures render at the fixed verify_w×verify_h (run_verify resizes
    // the window); the interactive window is resizable and the desktop may be far
    // larger, so UI chrome must stay
    // resolution-robust (BL-093 sized the Selection element to its content for this
    // reason). See docs/development/DEVELOPMENT_PRACTICES.md § Display environment.
    {
        int win_w = 0, win_h = 0;
        SDL_GetWindowSize(m_window, &win_w, &win_h);
        const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
        SDL_Log("Display: window %dx%d, desktop %dx%d @ %.0fHz, content-scale %.2f",
                win_w, win_h,
                dm ? dm->w : 0, dm ? dm->h : 0,
                dm ? static_cast<double>(dm->refresh_rate) : 0.0,
                static_cast<double>(SDL_GetWindowDisplayScale(m_window)));
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(m_window, m_renderer);
    ImGui_ImplSDLRenderer3_Init(m_renderer);

    // Load the UI font with oversampling so on-canvas labels stay crisp at the
    // fractional positions moving bodies produce. The renderer backend uploads
    // the atlas on the first NewFrame.
    ui::load_ui_font();

#ifndef NDEBUG
    // BL-215: the text-overflow ledger records in Debug builds (and always under
    // --verify, armed in run_verify); off in Release interactive.
    ui::set_overflow_recording(true);
#endif
}

namespace {
constexpr std::array<float, 3> k_ui_scale_px = {16.0f, 20.0f, 24.0f}; ///< BL-063 steps: 1.0 / 1.25 / 1.5 of the 16px base.
} // namespace

void app::apply_ui_scale()
{
    const int step = std::clamp(m_settings.ui_scale_step, 0, static_cast<int>(k_ui_scale_px.size()) - 1);
    ui::reload_ui_font(k_ui_scale_px[static_cast<std::size_t>(step)]);
    ImGui_ImplSDLRenderer3_DestroyFontsTexture();

    // The display floor scales with the font (BL-215): BL-063 grows the atlas
    // without ScaleAllSizes, so the hard-coded px chrome stays at 1.0x while
    // text grows — at 1.5x the same layout honestly needs 1920x1080.
    const float s = k_ui_scale_px[static_cast<std::size_t>(step)] / k_ui_scale_px[0];
    SDL_SetWindowMinimumSize(m_window,
                             static_cast<int>(1280.0f * s),
                             static_cast<int>(720.0f * s));
}

app::~app()
{
    // BL-1108: FIRST, before any member a worker reads is gone. Members destruct
    // in reverse declaration order, so `m_works` and the progress sinks died
    // BEFORE the futures blocked on the workers still reading them -- the
    // 0xC0000005 on closing the window mid-build (2026-09-24, twice). run()
    // joins with the window up; this covers every path that never ran it.
    join_workers(/*on_screen=*/false);

    m_ground.shutdown(); // baked-ground textures die before their renderer
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void app::join_workers(bool on_screen)
{
    // Every future a worker may still be running behind: the cold build, the
    // four wizard rounds, the surface preview. `wait()` on a DEFERRED future
    // would run it here, so those (the --verify capture path's) are skipped:
    // a deferred future never runs on its own and its destructor never blocks.
    const auto pending = [](const auto& f) {
        return f.valid() && f.wait_for(std::chrono::seconds(0)) == std::future_status::timeout;
    };
    const auto any_pending = [&] {
        if (pending(m_worldgen_future) || pending(m_wiz_surface_future)) return true;
        for (int i = 0; i < wizard_lapse_round_count; ++i)
            if (pending(m_wiz_history_future[i])) return true;
        return false;
    };
    if (!any_pending())
        return;

    std::printf("[exit] finishing the build before quitting (a worker is still running)\n");
    std::fflush(stdout);
    const auto t0 = std::chrono::steady_clock::now();
    while (any_pending())
    {
        if (on_screen && m_window != nullptr && m_renderer != nullptr)
        {
            // A plain frame with one line on it, so the wait is not a hang:
            // events are pumped (and ignored -- a second close changes nothing)
            // and the window repaints, which is what Windows judges us by.
            SDL_Event ev;
            while (SDL_PollEvent(&ev))
                ImGui_ImplSDL3_ProcessEvent(&ev);
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            const ImVec2 disp = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos({disp.x * 0.5f, disp.y * 0.5f}, ImGuiCond_Always,
                                    {0.5f, 0.5f});
            if (ImGui::Begin("##quitwait", nullptr,
                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                                 | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
                                 | ImGuiWindowFlags_AlwaysAutoResize
                                 | ImGuiWindowFlags_NoBackground))
            {
                const long long secs = std::chrono::duration_cast<std::chrono::seconds>(
                                           std::chrono::steady_clock::now() - t0).count();
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(225, 230, 240, 255));
                ImGui::Text("Finishing the build before quitting... %lld s", secs);
                ImGui::PopStyleColor();
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 128, 142, 255));
                ImGui::TextUnformatted("A world build cannot be stopped mid-pass; "
                                       "the window closes when it lands.");
                ImGui::PopStyleColor();
            }
            ImGui::End();
            ImGui::Render();
            SDL_SetRenderDrawColor(m_renderer, 15, 15, 20, 255);
            SDL_RenderClear(m_renderer);
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_renderer);
            SDL_RenderPresent(m_renderer);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    std::printf("[exit] workers joined after %lld ms\n",
                static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - t0).count()));
    std::fflush(stdout);
}

int app::run(autostart_mode autostart)
{
    const bool windowed_autostart = (autostart != autostart_mode::none);
    // BL-1076: only a person at the plain interactive path is asked which
    // corporation they are; the windowed autostart walks the wizard for
    // nobody, so its tail draws (and says so).
    m_player_picks_seat = !windowed_autostart;
    // Apply the player's persisted display settings before anything renders, so
    // the window opens at their last size/mode. Interactive-only: run_verify()
    // never touches settings, keeping golden captures at the fixed default size.
    load_settings();
    apply_display_settings();
    apply_ui_scale();

    m_lua.load("scripts/init.lua");

    // Open on the main menu — the deliberate entry point. "New Game" only opens the
    // New World wizard; the world, economy and sim clock are not built until the
    // player finishes it (start_new_game), so nothing simulates behind either screen
    // and the clock starts when play does.
    m_screen = app_screen::menu;

    // --load <path>: open straight into a saved campaign, skipping the menu, the
    // wizard and the ~25-38 s generation plus the landscape search and the
    // winner's validation run that follow it. THE
    // reason BL-536 was raised -- a capture run that iterates on one surface pays
    // that cold start on every launch, and this is what stops it. A failed load
    // falls through to the menu with the reason posted to the comms log, rather
    // than exiting: a missing file should not look like a crash.
    if (!m_pending_load.empty())
    {
        const std::string path = m_pending_load;
        m_pending_load.clear();
        if (load_game_from(path))
            std::printf("[load] opened %s\n", path.c_str());
        else
            std::printf("[load] FAILED to open %s - falling back to the menu\n", path.c_str());
        std::fflush(stdout);
    }

    // --autostart-windowed: press "Begin" as the wizard would — surface build left
    // in flight, generation on the worker — but with the REAL frame loop below
    // drawing the loading screen and making the mid-frame in_game transition. The
    // headless --autostart passes; this covers what it cannot: the ImGui frames.
    if (windowed_autostart)
    {
        m_pending_world_params = fresh_world_params();
        open_new_world_wizard();
        m_autostart_wizard = 0; // the wizard's own draw advances rounds + presses Begin
        std::printf("[autostart-%s] walking the wizard, then Begin\n",
                    autostart == autostart_mode::play ? "play" : "windowed");
        std::fflush(stdout);
    }
    int autostart_ingame_frames = 0;

    bool running = true;
    while (running && !m_quit_requested)
    {
        process_events(running);

        // Nothing simulates on the menu or the staged generation screen — just pump
        // events and draw. The sim clock is rebased when the generation screen hands
        // over, so time spent reading it never lands as elapsed in-game days.
        if (m_screen != app_screen::in_game)
        {
            render();
            continue;
        }

        // BL-412: the live agent control seam. Opens once a campaign exists
        // (the session actor is the player corp — the seat the agent
        // occupies), then pumps the loopback socket once per frame. The
        // agent GATES THE CLOCK: attaching pauses the sim, and each TICK
        // request releases exactly one econ tick — so the schedule of
        // boundaries is agent-driven, not wall-clock-driven, and the command
        // transcript is a replay artifact. The human keeps override: the
        // speed keys still work, and with the clock running a TICK simply
        // waits for the next natural boundary. Commands never apply here —
        // only at the econ boundary drain below.
        if (m_agent_port != 0)
        {
            if (!m_agent_seam.listening())
            {
                std::string err;
                if (m_agent_seam.listen(m_agent_port, m_world.player_entity,
                                        /*as_any=*/false, &err))
                {
                    std::printf("[agent-seam] listening on 127.0.0.1:%u\n",
                                static_cast<unsigned>(m_agent_seam.port()));
                    std::fflush(stdout);
                }
                else
                {
                    std::fprintf(stderr, "[agent-seam] %s — not hosting\n", err.c_str());
                    m_agent_port = 0; // report once, then stay a normal session
                }
            }
            if (m_agent_seam.listening())
            {
                m_agent_seam.set_actor(m_world.player_entity);
                const agent_seam::pump_events ev =
                    m_agent_seam.pump(m_world, m_registry,
                                      static_cast<int>(m_last_econ_tick));
                if (ev.attached)
                {
                    // The agent takes the clock. Remember the speed so the
                    // pause keys behave as they always have.
                    if (m_sim_loop.speed() > 0)
                        m_prev_speed = m_sim_loop.speed();
                    m_sim_loop.set_speed(0);
                }
                if (ev.detached)
                {
                    // Stay paused — a world that starts moving the moment its
                    // player process dies is a surprise, not a convenience.
                    // Keep the replay artifact next to the exe.
                    m_agent_seam.write_transcript("agent_transcript.log");
                }
                if (ev.tick_requested && m_sim_loop.paused())
                    m_sim_loop.advance_days(sim_loop::econ_tick_days);
            }
        }

        m_sim_loop.tick();

        // Advance orbital motion by the in-game days elapsed this frame. Freezes
        // automatically while paused, since elapsed_days() stops advancing.
        const double now_days = m_sim_loop.elapsed_days();
        advance_orbits(m_world, now_days - m_last_orbit_days);
        m_last_orbit_days = now_days;

        // Advance in-progress surveys on whole-day boundaries (one sim tick = one
        // day). Crossing whole days — rather than fractional per-frame deltas —
        // keeps the region-reveal schedule deterministic and frame-rate-independent,
        // mirroring the econ-tick crossing below. Freezes while paused.
        const int survey_day = static_cast<int>(now_days);
        if (survey_day > m_last_survey_day)
        {
            advance_surveys(m_world, survey_day - m_last_survey_day);
            m_last_survey_day = survey_day;
        }

        // Mirror the day tick onto the world so read-only UI surfaces can age trade
        // routes for the activity fog (BL-089) without threading the tick everywhere.
        m_world.current_day_tick = static_cast<int>(m_sim_loop.day_tick());

        // Continuous sim time for the intra-body vision beam (BL-152/154). The actual
        // vision refresh (update_body_vision) runs in render(), so both the live loop
        // and the --verify capture path (capture_frame -> render) get it.
        m_ui.sim_now_days = now_days;

        // Resolve the economy on each econ-tick (quarter) boundary the clock crosses.
        const uint64_t econ = m_sim_loop.econ_tick();
        while (m_last_econ_tick < econ)
        {
            // BL-412: agent commands land HERE and only here — at the econ
            // boundary, in arrival order, against the post-step world of the
            // tick just completed, each stamped with that tick and recorded.
            // Mid-interval application would tie the outcome to the wall-clock
            // day the bytes arrived, which is the property the transcript
            // exists to exclude.
            m_agent_seam.drain_boundary(m_world, m_registry,
                                        static_cast<int>(m_last_econ_tick));
            step_economy();
            ++m_last_econ_tick;
            m_agent_seam.on_tick_stepped(static_cast<int>(m_last_econ_tick));
        }

        // Windowed autostart: the impatient-player click. During the seconds
        // start_new_game stalls the UI a real player clicks the frozen window;
        // those queued clicks are delivered onto the FIRST in-game frames, at
        // the screen centre where the loading bar was — i.e. onto the canvas.
        // Synthesise exactly that: motion + a click on each of the first five
        // in-game frames. SDL_PushEvent feeds the same queue real input uses.
        if (windowed_autostart && autostart_ingame_frames < 5)
        {
            int ww = 0, wh = 0;
            SDL_GetWindowSize(m_window, &ww, &wh);
            const float cx = static_cast<float>(ww) * 0.5f;
            const float cy = static_cast<float>(wh) * 0.5f;
            SDL_Event ev{};
            ev.type = SDL_EVENT_MOUSE_MOTION;
            ev.motion.x = cx; ev.motion.y = cy;
            SDL_PushEvent(&ev);
            SDL_Event dn{};
            dn.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            dn.button.button = SDL_BUTTON_LEFT; dn.button.clicks = 1;
            dn.button.x = cx; dn.button.y = cy; dn.button.down = true;
            SDL_PushEvent(&dn);
            SDL_Event up = dn;
            up.type = SDL_EVENT_MOUSE_BUTTON_UP;
            up.button.down = false;
            SDL_PushEvent(&up);
        }

        render();

        // Windowed autostart: a handful of real in-game frames is the coverage —
        // the first frame builds the raster caches and every shell surface; if
        // those survive, exit clean rather than sitting on an open window.
        // The SMOKE mode's frame cap. `play` deliberately has none: it walked the
        // wizard purely so a human could look at the running game, and exiting
        // under them is indistinguishable from a crash.
        if (autostart == autostart_mode::smoke && ++autostart_ingame_frames >= 120)
        {
            std::printf("[autostart-windowed] OK  %d in-game frames rendered\n",
                        autostart_ingame_frames);
            std::fflush(stdout);
            break;
        }

        // NR-256: the third exit path, named like the other two. Checked here
        // rather than at the loop head so it prints before the loop drops out.
        if (m_quit_requested)
        {
            std::printf("[exit] m_quit_requested\n");
            std::fflush(stdout);
        }
    }

    // BL-1108: a build still running on a worker reads the progress sinks and
    // the works table, which die with this object; join it here, with the
    // window still up to say so, before anything it reads is destroyed.
    join_workers(/*on_screen=*/true);

    // Persist any free drag-resize captured this session (toggles/presets already
    // saved on change). Fullscreen: keep the last windowed size, don't overwrite it.
    save_settings();

    // BL-412: leave the replay artifact behind before the session ends (also
    // written on client detach; both writes carry the same full transcript).
    if (!m_agent_seam.transcript().empty())
        m_agent_seam.write_transcript("agent_transcript.log");
    m_agent_seam.shutdown();
    return 0;
}

// ---------------------------------------------------------------------------
// Async world generation (2026-08-12)
// ---------------------------------------------------------------------------
//
// Generation takes ~25 s on the 312x145 grid with the Era -1 sim wired in, and
// `start_new_game` was called from inside an ImGui frame. The UI therefore never
// pumped for the whole of it and Windows painted the spinner and reported "not
// responding" — which is exactly how it was first reported: as a crash.
//
// The split is at the Lua boundary. sol2 is not thread-safe, so the world-gen
// config is loaded HERE, on the render thread, and only make_hard_coded_world —
// which is pure C++ — goes to the worker.

namespace {

/// BL-1073: every field of `world_params`, compared. The guard behind the
/// wizard's world cache: invalidation is what keeps the cache honest, and this
/// is the check that it did. A new field on `world_params` must join this list,
/// or a world built under a different value could be adopted.
bool same_world_params(const world_params& a, const world_params& b)
{
    const world_preferences& x = a.preferences;
    const world_preferences& y = b.preferences;
    return a.seed == b.seed && a.era_seed == b.era_seed && a.abundance == b.abundance
        && a.epoch_year == b.epoch_year && a.prehistory_years == b.prehistory_years
        && a.empires_start_year == b.empires_start_year
        && a.empires_stop_year == b.empires_stop_year
        && a.exploration_sim_enabled == b.exploration_sim_enabled
        && a.exploration_stop_year == b.exploration_stop_year
        && a.industrialisation_span_enabled == b.industrialisation_span_enabled
        && a.industrialisation_stop_year == b.industrialisation_stop_year
        && a.resume_seeds_corridor_tier == b.resume_seeds_corridor_tier
        && a.body_count == b.body_count
        && x.star == y.star && x.world_size == y.world_size && x.interior == y.interior
        && x.metal == y.metal && x.ocean == y.ocean && x.oxygen_story == y.oxygen_story
        && x.coal_basins == y.coal_basins && x.drawdown == y.drawdown
        && x.history_turbulence == y.history_turbulence
        && x.roll[0] == y.roll[0] && x.roll[1] == y.roll[1] && x.roll[2] == y.roll[2];
}

} // namespace

void app::begin_new_game()
{
    m_lua.load("scripts/world_gen.lua");
    m_worldgen_cfg = world_gen_config{};
    m_worldgen_cfg.load_from_lua(m_lua);

    m_worldgen_params = m_pending_world_params;
    m_generation_report = generation_report{};

    // --- BL-1085: BEGIN WAITS FOR ROUND 6, THEN ADOPTS ----------------------
    //
    // Round 6's worker runs the whole build -- generation, then
    // finish_campaign_world's search and settle (STARTUP.md § Handoff) -- so
    // Begin has three cases and none of them builds twice:
    //   1. the worker is STILL RUNNING: wait on it behind the wait surface
    //      (never a second concurrent build -- BL-1078's memory-pressure case);
    //   2. it LANDED and the cache is valid: adopt it;
    //   3. there is nothing to adopt (menu -> Begin, --autostart, a stale
    //      landing, params that moved): the cold worker makes the same calls.
    const int last = wizard_lapse_round_count - 1;
    if (m_wiz_history_future[last].valid())
    {
        m_begin_waits_round6 = true;
        std::printf("[begin] round 6 is still building: waiting on its worker "
                    "(no second build)\n");
        std::fflush(stdout);
        m_screen = app_screen::building;
        return;
    }
    if (try_adopt_wizard_world())
        return;
    launch_cold_build();
}

bool app::try_adopt_wizard_world()
{
    // --- BL-1073: BEGIN ADOPTS THE WIZARD'S WORLD ----------------------------
    //
    // Round 6 already ran this exact build and finished it (STARTUP.md § The
    // world cache), so a valid cache is handed to poll_worldgen, which MOVES
    // it into play and runs the tail -- the presentation half, the seat --
    // instead of building the same world a second time. A cache whose params
    // differ from the pending ones is released and the build runs cold.
    if (m_wiz_world && m_wiz_world->ready
        && !same_world_params(m_wiz_world->params, m_pending_world_params))
        drop_wizard_world("its params no longer match the wizard's");
    if (!(m_wiz_world && m_wiz_world->ready))
        return false;

    m_worldgen_slot = std::move(m_wiz_world);
    m_wiz_world.reset(); // one world at most: the cache is spent

    // The wait surface reads a finished build: the outer bar full, nothing
    // left to caption but the seat to come.
    m_worldgen_progress.begin_wait();
    m_worldgen_progress.stage_count.store(1, std::memory_order_relaxed);
    m_worldgen_progress.stage.store(1, std::memory_order_relaxed);
    m_worldgen_progress.label.store(12, std::memory_order_relaxed); // "Finishing"
    m_worldgen_progress.weight_total.store(1, std::memory_order_relaxed);
    m_worldgen_progress.weight_done.store(1, std::memory_order_relaxed);
    m_worldgen_progress.budget_ready.store(false, std::memory_order_relaxed);
    m_worldgen_progress.grid_w.store(0, std::memory_order_relaxed); // no carve to draw
    m_worldgen_progress.grid_h.store(0, std::memory_order_relaxed);
    m_carve_view.clear();

    std::printf("[begin] adopted the wizard's world (seed %u, era seed %u): "
                "make_hard_coded_world not called\n",
                m_pending_world_params.seed, m_pending_world_params.era_seed);
    std::fflush(stdout);

    m_worldgen_adopted = true;
    m_screen           = app_screen::building;
    return true;
}

generation_progress& app::building_sink()
{
    return m_begin_waits_round6 ? m_wiz_history_progress[wizard_lapse_round_count - 1]
                                : m_worldgen_progress;
}

void app::launch_cold_build()
{
    std::printf("[begin] no wizard world to adopt: building (make_hard_coded_world, "
                "then finish_campaign_world)\n");
    std::fflush(stdout);

    // BL-1072: every field the wait reads, and the elapsed clock's start.
    m_worldgen_progress.begin_wait();
    // BL-1053: the stages THIS run will report, never the label count --
    // generation publishes the same figure at its first line, and a total
    // taken from the label table left the bar short of its end.
    m_worldgen_progress.stage_count.store(generation_stage_count(m_worldgen_cfg),
                                          std::memory_order_relaxed);

    // BL-754: the budget is a member too, so a second campaign start would
    // otherwise show the previous world's numbers for the whole of its wait.
    // Clear `budget_ready` FIRST so no frame can read a stale pass split as a
    // fresh one.
    m_worldgen_progress.budget_ready.store(false, std::memory_order_relaxed);
    m_worldgen_progress.ms_world_total.store(0, std::memory_order_relaxed);
    m_worldgen_progress.ms_before_settlement.store(0, std::memory_order_relaxed);
    m_worldgen_progress.ms_settlement.store(0, std::memory_order_relaxed);
    m_worldgen_progress.ms_era.store(0, std::memory_order_relaxed);
    m_worldgen_progress.ms_after_era.store(0, std::memory_order_relaxed);

    // The carve sink is a member, so a SECOND campaign start would otherwise
    // open on the previous world's borders and charter marks. begin_carve
    // clears the map itself; these are the fields it does not own. The view is
    // dropped too — the epoch is monotonic across runs, so a stale copy would
    // survive until the worker's first publish.
    m_worldgen_progress.grid_w.store(0, std::memory_order_relaxed);
    m_worldgen_progress.grid_h.store(0, std::memory_order_relaxed);
    m_worldgen_progress.nation_count.store(0, std::memory_order_relaxed);
    m_worldgen_progress.nation_id_base.store(0, std::memory_order_relaxed);
    m_worldgen_progress.asset_mark_count.store(0, std::memory_order_relaxed);
    m_worldgen_progress.corp_row_count.store(0, std::memory_order_relaxed);
    m_worldgen_progress.player_slot.store(-1, std::memory_order_relaxed);
    m_carve_view.clear();
    m_carve_seen = m_worldgen_progress.carve_epoch.load(std::memory_order_relaxed);

    // The works table is an input to generation now (BL-321), so it must be
    // loaded on THIS thread before the worker starts reading it. So is the
    // recipe registry (BL-1085): the finish bands it and settles on it, and
    // sol2 is not thread-safe, so it is loaded here and a COPY goes over.
    ensure_works_loaded();
    load_recipe_registry();
    auto slot      = std::make_shared<wizard_world_cache>();
    slot->params   = m_worldgen_params;
    slot->registry = m_registry;
    // The finish's two steps are in the bar's plan before the build starts
    // (generation adds `weight_after` into its total; BL-1085).
    m_worldgen_progress.weight_after.store(finish_campaign_weight_ms(m_worldgen_params),
                                           std::memory_order_relaxed);

    // The worker writes only into its slot plus the atomics; the slot is
    // handed across by the future, so nothing is shared mutable state.
    m_worldgen_future = std::async(std::launch::async, [this, slot] {
        // m_works is loaded once at startup and never mutated after, so handing
        // the worker a pointer to it shares nothing mutable (BL-321).
        slot->w = make_hard_coded_world(m_worldgen_params, &slot->report,
                                        m_worldgen_cfg, &m_worldgen_progress, &m_works);
        // THE SAME CALL ROUND 6 MAKES, in the same order (STARTUP.md
        // § Handoff): an adopted world and a cold build open on one state hash.
        slot->finish = finish_campaign_world(slot->w, slot->report, slot->registry,
                                             m_worldgen_params, m_worldgen_cfg,
                                             &m_worldgen_progress);
        slot->ready  = true;
        return slot;
    });

    m_screen = app_screen::building;
}

void app::poll_worldgen()
{
    // --- BL-1085: Begin pressed while round 6 was still building ------------
    //
    // Keep polling the wizard's futures (the wizard's own draw is not running
    // on this screen) until round 6 lands; then adopt what it cached, or --
    // if the landing was stale and dropped its world -- build cold. Either
    // way the second build only ever starts after the first has finished.
    if (m_begin_waits_round6)
    {
        poll_wizard_history();
        if (m_wiz_history_future[wizard_lapse_round_count - 1].valid())
            return; // still running
        m_begin_waits_round6 = false;
        std::printf("[begin] round 6 landed\n");
        std::fflush(stdout);
        if (!try_adopt_wizard_world())
        {
            launch_cold_build();
            return; // the cold worker's future is polled from the next call
        }
        // adopted: fall through and take the slot now
    }

    // --- The finished world: adopted from the wizard, or the cold worker's ---
    std::shared_ptr<wizard_world_cache> built;
    if (m_worldgen_adopted)
    {
        m_worldgen_adopted = false;
        built = std::move(m_worldgen_slot);
    }
    else
    {
        if (!m_worldgen_future.valid())
            return;
        if (m_worldgen_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return;
        built = m_worldgen_future.get();
    }
    if (!built || !built->ready)
    {
        // Cannot happen by construction (`ready` is the worker's last write and
        // the future is the synchronisation); said out loud rather than played.
        std::printf("[begin] FAILED: the build landed no world\n");
        std::fflush(stdout);
        m_screen = app_screen::menu;
        return;
    }

    // BL-754: the generation budget, on the app's own console alongside the
    // `[start_new_game]` phase lines it already prints. Deliberately the same
    // words and the same order as the `[gen budget]` line the harness tier
    // emits, so an app run and a `world_determinism` run can be read against
    // each other digit for digit. The on-screen half is in
    // draw_building_screen; this is the half that survives into a log. The
    // COLD build's sink only: an adopted world was built on round 6's sink,
    // and its absence here is begin_adopts_check.js's proof that Begin never
    // built.
    if (m_worldgen_progress.budget_ready.load(std::memory_order_acquire))
    {
        std::printf("[gen budget] total %lld ms  (pre-settlement %lld, settlement %lld, "
                    "era-1 %lld, post-era %lld)\n",
                    static_cast<long long>(m_worldgen_progress.ms_world_total.load(
                        std::memory_order_relaxed)),
                    static_cast<long long>(m_worldgen_progress.ms_before_settlement.load(
                        std::memory_order_relaxed)),
                    static_cast<long long>(m_worldgen_progress.ms_settlement.load(
                        std::memory_order_relaxed)),
                    static_cast<long long>(m_worldgen_progress.ms_era.load(
                        std::memory_order_relaxed)),
                    static_cast<long long>(m_worldgen_progress.ms_after_era.load(
                        std::memory_order_relaxed)));
        // BL-1072: the per-step split the loading bar's weights are read from
        // (`generation_step_cost_ms`), label index : milliseconds -- the same
        // line the harness tier prints, so a Release app run can re-weigh them.
        std::printf("[gen steps]");
        for (int l = 0; l < generation_stage_label_count; ++l)
            if (const int32_t ms = m_worldgen_progress.ms_step[static_cast<std::size_t>(l)].load(
                    std::memory_order_relaxed); ms > 0)
                std::printf(" %d:%d", l, static_cast<int>(ms));
        std::printf("\n");
        std::fflush(stdout);
    }

    // --- INTO PLAY: the world, its report, the registry the settle ran on, the
    // winner's score (the seat's floor and rank, BL-1020) -- MOVED, never
    // copied (a copied world does not iterate as its original; the slot is
    // spent). Then the presentation half of Begin (STARTUP.md § Handoff, item
    // 2) and the seat (item 3).
    m_world                  = std::move(built->w);
    m_generation_report      = std::move(built->report);
    m_registry               = std::move(built->registry);
    m_landscape_winner_score = built->finish.search.winner_score;
    built.reset();
    start_new_game_prelude();
    // BL-568: the cadence key continues from the settle's twelve steps, which
    // the worker stamped 0..11 (`run_settle`); the first live quarter is 12.
    m_econ_steps = static_cast<uint64_t>(k_campaign_settle_ticks);

    // BL-630: the settle is over, so the trailing figures the seat card shows
    // exist (the floor itself reads phase 6's static score — BL-1020). Seat
    // BEFORE finish_new_game, which is what flips to `in_game` — the first
    // drawn frame must already have a player.
    //
    // BL-1076: THE PLAYER PICKS. Three routes, in precedence order:
    //   1. `--seat <id>` — a pick made at launch (an agent's route);
    //   2. the interactive path — open the selection canvas, whose
    //      Confirm calls finish_new_game itself;
    //   3. no player to ask (--autostart*, windowed autostart) — the
    //      weighted draw, said out loud.
    if (m_seat_pick_arg >= 0)
    {
        m_seat_result = rank_spawn_candidates(m_world, m_landscape_winner_score);
        const corp_command_result r =
            take_seat_pick(static_cast<entity_id>(m_seat_pick_arg));
        if (r == corp_command_result::applied)
        {
            finish_new_game();
            return;
        }
        std::printf("[seat] --seat %lld REJECTED (%s): nothing seated\n",
                    m_seat_pick_arg,
                    r == corp_command_result::rejected_no_corp
                        ? "rejected_no_corp: no such corporation"
                        : "rejected_invalid: not a ranked specialist");
        std::fflush(stdout);
        if (!m_player_picks_seat)
        {
            // Headless: a pick that was refused is a failed run, never
            // a silent fall-back to the draw (the AI-facing-seam rule).
            m_seat_pick_failed = true;
            return;
        }
        open_seat_screen(); // a person is here: let them choose
        return;
    }
    if (m_player_picks_seat)
    {
        open_seat_screen();
        return;
    }
    m_seat_drawn_because = "no player to ask (autostart)";
    seat_player();
    finish_new_game();
}

void app::draw_building_screen()
{
    poll_worldgen();

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({disp.x * 0.5f, disp.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoBackground;

    if (ImGui::Begin("##building", nullptr, flags))
    {
        const char* title = "BUILDING THE WORLD";
        const float tw = ImGui::CalcTextSize(title).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (420.0f - tw) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(225, 230, 240, 255));
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        ImGui::Dummy({420.0f, 10.0f});

        // THE ONE WAIT SURFACE (BL-1072): the same bars, caption and elapsed
        // count every wizard loading round draws -- the outer bar weighted by
        // measured step cost, the inner bar inside every long step. The
        // search and the settle are steps of the same run now (BL-1085), so
        // the caption names them from the label table like every other step;
        // while Begin waits on round 6 the sink drawn is that round's.
        generation_progress& sink = building_sink();
        ui::draw_generation_wait(sink, 420.0f, nullptr, m_golden_dir.empty());

        ImGui::Dummy({420.0f, 8.0f});
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 128, 142, 255));
        const int step = sink.label.load(std::memory_order_relaxed);
        ImGui::TextUnformatted(step == 16 ? "The landscape is searched for the field play opens on."
                               : step == 17 ? "Three years of commerce prove the chosen landscape."
                               : m_begin_waits_round6 ? "Round 6 is still building this world."
                                                      : "Four hundred years of history are being lived through.");
        ImGui::PopStyleColor();

        // --- The generation budget, on the screen that spends it (BL-754) ---
        //
        // Work item (1) of BL-754 named the loading screen's progress sink as
        // the place to instrument, and until now the only reader was a
        // harness: the `[gen budget]` line lives behind `if (fixture)` and the
        // app passes none, so the game never reported its own budget and the
        // screen said nothing about where its wait went.
        //
        // It appears the moment generation finishes and stays for the rest of
        // the screen — which is the honest placement, because the world build
        // is over and its split is final, while the pass still running below
        // it is not this measurement's subject. Seconds to two places: this is
        // read by a human deciding whether a pass is affordable, and a
        // millisecond of a six-second build is noise dressed as precision.
        if (m_worldgen_progress.budget_ready.load(std::memory_order_acquire))
        {
            const auto secs = [](const std::atomic<int64_t>& ms) {
                return static_cast<double>(ms.load(std::memory_order_relaxed)) / 1000.0;
            };
            ImGui::Dummy({420.0f, 6.0f});
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(104, 112, 126, 255));
            ImGui::Text("world built in %.2f s", secs(m_worldgen_progress.ms_world_total));
            // Same four passes, same order and same names as the harness line,
            // so a screen and a `world_determinism` run can be read together.
            ImGui::Text("pre-settlement %.2f  settlement %.2f  era-1 %.2f  post-era %.2f",
                        secs(m_worldgen_progress.ms_before_settlement),
                        secs(m_worldgen_progress.ms_settlement),
                        secs(m_worldgen_progress.ms_era),
                        secs(m_worldgen_progress.ms_after_era));
            ImGui::PopStyleColor();
        }

        draw_building_carve();
    }
    ImGui::End();
}

/// The live carve (BL-305): the world's politics being DECIDED rather than
/// handed over. It appears when generate_nations opens the carve and stays for
/// the rest of the screen, so the borders grow, settle, and are then staked by
/// the charters — all in the order the passes actually do it.
///
/// Reads `m_worldgen_progress` and NOTHING else. The world belongs to the
/// worker thread until poll_worldgen adopts it, so every read here is an atomic
/// load; there is no lock, no callback into the generation pass, and no
/// re-entrancy. See generation_progress in world/hard_coded_world.hpp for the
/// publish side and the encoding.
void app::draw_building_carve()
{
    constexpr float map_w = 420.0f;

    const int gw = m_worldgen_progress.grid_w.load(std::memory_order_relaxed);
    const int gh = m_worldgen_progress.grid_h.load(std::memory_order_relaxed);
    if (gw <= 0 || gh <= 0)
        return; // planetology and deep time are not maps; nothing to show yet

    // Adopt a moved carve at most once a frame. The epoch is the whole point of
    // the copy: a still frame costs zero atomic loads rather than 45,000.
    const uint32_t epoch = m_worldgen_progress.carve_epoch.load(std::memory_order_acquire);
    if (epoch != m_carve_seen)
    {
        m_carve_seen = epoch;
        m_carve_view.resize(static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh));
        for (std::size_t i = 0; i < m_carve_view.size(); ++i)
            m_carve_view[i] = m_worldgen_progress.owner[i].load(std::memory_order_relaxed);
    }
    if (m_carve_view.size() != static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
        return;

    // Index -> entity id once the nations exist (see nation_id_base); until then
    // the index IS the key, which is the same palette in a different order.
    const uint32_t id_base = m_worldgen_progress.nation_id_base.load(std::memory_order_relaxed);

    const float cell = map_w / static_cast<float>(gw);
    const float map_h = cell * static_cast<float>(gh);

    ImGui::Dummy({map_w, 8.0f});
    ImDrawList*  dl = ImGui::GetWindowDrawList();
    const ImVec2 tl = ImGui::GetCursorScreenPos();
    ImGui::Dummy({map_w, map_h});

    // Unclaimed reads as sea: it is ocean, or land no seed has reached yet.
    dl->AddRectFilled(tl, {tl.x + map_w, tl.y + map_h}, IM_COL32(22, 30, 44, 255));

    // Run-length merged per row. The carve produces large contiguous
    // territories, so one quad per tile would be 45,000 quads a frame to draw a
    // few hundred distinct spans.
    for (int row = 0; row < gh; ++row)
    {
        const std::size_t base = static_cast<std::size_t>(row) * static_cast<std::size_t>(gw);
        int     run_start = 0;
        int16_t run_val   = m_carve_view[base];
        for (int col = 1; col <= gw; ++col)
        {
            // col == gw is a sentinel that always closes the last run.
            const int16_t v = (col < gw)
                ? m_carve_view[base + static_cast<std::size_t>(col)]
                : static_cast<int16_t>(-1);
            if (v == run_val)
                continue;

            if (run_val > 0) // 0 == unclaimed; the sink stores index + 1
            {
                // The SAME palette the in-game Country lens uses, so the map the
                // player watches being carved is the map they meet again under
                // the lens (ui/presentation.hpp).
                dl->AddRectFilled(
                    {tl.x + static_cast<float>(run_start) * cell,
                     tl.y + static_cast<float>(row) * cell},
                    {tl.x + static_cast<float>(col) * cell,
                     tl.y + static_cast<float>(row + 1) * cell},
                    ui::palette::nation_colour(
                        static_cast<entity_id>(id_base + static_cast<uint32_t>(run_val - 1))));
            }
            run_start = col;
            run_val   = v;
        }
    }

    // The charters' holdings, as they are staked — over the finished carve, in
    // the corporation palette, so a marker reads as a different KIND of claim
    // from the territory under it.
    const int marks = m_worldgen_progress.asset_mark_count.load(std::memory_order_acquire);
    for (int i = 0; i < marks && i < generation_progress::max_asset_marks; ++i)
    {
        const uint32_t m =
            m_worldgen_progress.asset_mark[static_cast<std::size_t>(i)].load(std::memory_order_relaxed);
        const int mc   = static_cast<int>( m        & 0x1FFu);
        const int mr   = static_cast<int>((m >>  9) & 0x1FFu);
        const int slot = static_cast<int>((m >> 18) & 0x3Fu);
        if (mc >= gw || mr >= gh) continue;

        const ImVec2 ctr{tl.x + (static_cast<float>(mc) + 0.5f) * cell,
                         tl.y + (static_cast<float>(mr) + 0.5f) * cell};
        dl->AddCircleFilled(ctr, 3.5f, IM_COL32(10, 12, 18, 220), 8);
        dl->AddCircleFilled(ctr, 2.5f, ui::palette::corp_colour(slot), 8);
    }

    dl->AddRect(tl, {tl.x + map_w, tl.y + map_h}, IM_COL32(52, 60, 76, 255));

    // The carve's own count, stated only once it is true: before Pass 2c there
    // is no final nation count, and the pre-merge seed count is a different
    // (and misleading) number. It moves once, when the borders settle.
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 128, 142, 255));
    ImGui::Text("%d nations", m_worldgen_progress.nation_count.load(std::memory_order_relaxed));
    ImGui::PopStyleColor();

    // The charter ledger — the NON-POSITIONAL half of the corporate pass. A
    // capital figure has nowhere on a map to be, so it goes in a column rather
    // than as an overlay, swatch-matched to its markers above. No name column:
    // the generated name is a std::string decided in Pass 5 and does not cross
    // the atomics-only seam; the names arrive with the finished world.
    const int rows = m_worldgen_progress.corp_row_count.load(std::memory_order_acquire);
    if (rows <= 0)
        return;

    static const char* const focus_name[3] = { "Extraction", "Processing", "Trade" };
    const int player = m_worldgen_progress.player_slot.load(std::memory_order_relaxed);

    ImGui::Dummy({map_w, 4.0f});
    for (int c = 0; c < rows && c < generation_progress::max_corp_slots; ++c)
    {
        const int   f   = m_worldgen_progress.corp_focus  [static_cast<std::size_t>(c)].load(std::memory_order_relaxed);
        const int   n   = m_worldgen_progress.corp_assets [static_cast<std::size_t>(c)].load(std::memory_order_relaxed);
        const float cap = m_worldgen_progress.corp_capital[static_cast<std::size_t>(c)].load(std::memory_order_relaxed);

        const ImVec2 sp = ImGui::GetCursorScreenPos();
        dl->AddRectFilled({sp.x, sp.y + 4.0f}, {sp.x + 8.0f, sp.y + 12.0f},
                          ui::palette::corp_colour(c));
        ImGui::Dummy({14.0f, ImGui::GetTextLineHeightWithSpacing()});
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, c == player ? IM_COL32(225, 230, 240, 255)
                                                         : IM_COL32(120, 128, 142, 255));
        ImGui::Text("%-11s %2d holdings  %8.0f cr%s",
                    (f >= 0 && f < 3) ? focus_name[f] : "?",
                    n, static_cast<double>(cap), c == player ? "   (you)" : "");
        ImGui::PopStyleColor();
    }
}

// ---------------------------------------------------------------------------
// The seat (BL-630, 2026-08-26)
// ---------------------------------------------------------------------------
//
// Which corporation the player runs is DRAWN from a viability shortlist, not
// picked from a screen. The mechanism lives in world/spawn_seat.cpp so the
// headless sweep and the app read one implementation; this function is the
// app-side half — call it, log it, and repair the player-scoped UI caches the
// validation run filled against the provisional pick.
//
// Authority: CORPORATION_GENERATION.md § The spawn shortlist, and the seat.

/// The launch view (see setup_world): the opening frame and the first selection,
/// both read off the PLAYER'S corporation. Called by setup_world, and again once
/// the player is final — after the seat (seat_player) and after the search-less
/// verify path's spend — because the landscape search and the charter budget
/// REPLACE world-gen's roster (BL-977, BL-1042): framed only at setup, the view
/// opened on ground a removed corporation had held (BL-1044 cold review, finding 5).
void app::frame_launch_view()
{
    // START FROM THE PLAIN SURFACE VIEW, every call (cold review round 2): a
    // re-frame must not leave an earlier call's frame and selection standing
    // when the player it now reads holds nothing here — the no-player residual
    // (NR-911), or a player with no building on the launch body — or the game
    // would open framed on a removed corporation's ground. focus_on_surface is
    // what setup_world opens with, so the first call changes nothing.
    if (m_launch_body != null_entity)
        ui::focus_on_surface(m_world, m_ui, m_launch_body);
    m_ui.planetary_center_pending = false;
    m_ui.selected_entity          = null_entity;

    // Frame the opening view on the player's holdings so "where am I" is answered
    // the moment the surface appears, rather than dropping the player onto the whole
    // surface with their few tiles lost in it. Centre on the centroid of the player
    // corp's buildings that sit on the start body and zoom in to a regional framing;
    // if the player has no building there, keep the default whole-surface view.
    // Consumed by body_surface_canvas on its next draw; a later focus_on_surface
    // (deliberate navigation) cancels it via planetary_center_pending.
    {
        const auto pit = m_world.corporations.find(m_world.player_entity);
        if (pit != m_world.corporations.end())
        {
            long long sum_col = 0, sum_row = 0;
            int n = 0;
            for (entity_id bld_id : pit->second.assets)
            {
                const auto bit = m_world.buildings.find(bld_id);
                if (bit == m_world.buildings.end())
                    continue;
                const auto tit = m_world.tiles.find(bit->second.tile);
                if (tit == m_world.tiles.end() || tit->second.body != m_launch_body)
                    continue;
                sum_col += tit->second.grid_x;
                sum_row += tit->second.grid_y;
                ++n;
            }
            if (n > 0)
            {
                m_ui.planetary_center_col     = static_cast<int>(sum_col / n);
                m_ui.planetary_center_row     = static_cast<int>(sum_row / n);
                m_ui.planetary_center_pending = true;
                m_ui.planetary_zoom           = 11.0f;
            }
        }
    }

    // BL-174 strand 2 — a legible first move. The launch view framed who and where
    // but suggested nothing to DO, and the Selection band is not drawn at all with
    // nothing selected, so there was no surface on which to suggest anything.
    // Seeding the selection to the HQ's tile gives the band something to show at
    // launch: the player's own ground, with Construct primed on it (see
    // draw_tile_selection). Derived entirely from world state — no tutorial flag,
    // no timer, nothing persisted, and nothing that can go stale or lie.
    {
        const auto pit = m_world.corporations.find(m_world.player_entity);
        if (pit != m_world.corporations.end())
        {
            const auto hq = m_world.buildings.find(pit->second.hq_building);
            if (hq != m_world.buildings.end())
            {
                const auto tit = m_world.tiles.find(hq->second.tile);
                if (tit != m_world.tiles.end() && tit->second.body == m_launch_body)
                    m_ui.selected_entity = hq->second.tile;
            }
        }
    }

}

void app::seat_player()
{
    // BL-1020: the floor and the rank read phase 6's static score of the winner,
    // kept from start_new_game_prelude's search.
    m_seat_result = seat_player_corporation(m_world, m_active_world_params.seed,
                                            m_landscape_winner_score);
    finish_seat(/*picked=*/false);
}

void app::open_seat_screen()
{
    // BL-1076: rank, seat nobody, and hand the choice to the player. The
    // ranking is the draw's own (rank_spawn_candidates), so the order the
    // canvas offers is the order the shortlist has always been walked in.
    m_seat_result = rank_spawn_candidates(m_world, m_landscape_winner_score);
    m_seat_ui     = seat_screen_state{};
    m_screen      = app_screen::choosing_seat;
    std::printf("[seat] the selection canvas: %d specialists, %d above the floor%s\n",
                m_seat_result.specialist_count, m_seat_result.shortlist_size,
                m_seat_result.floor_unmet ? " (FLOOR UNMET: every firm is marked)" : "");
    std::fflush(stdout);
}

corp_command_result app::take_seat_pick(entity_id corp)
{
    // THE PICK IS A GAME ACT (Ben, 2026-09-24): it goes through the command
    // seam as `corp_verb::take_seat`, the same record an agent issues. This is
    // the PRE-PLAY host, so it owns the phase half of the validation and the
    // one the seam cannot do — the firm must be one the canvas OFFERS, a
    // ranked specialist of this world. Everything is checked before anything
    // moves; a refusal leaves the world as it was.
    if (m_world.corporations.find(corp) == m_world.corporations.end())
        return corp_command_result::rejected_no_corp;
    bool offered = false;
    for (const spawn_seat_candidate& c : m_seat_result.candidates)
        if (c.corp == corp)
            offered = true;
    if (!offered)
        return corp_command_result::rejected_invalid;

    corp_command cmd;
    cmd.tick = static_cast<int>(m_world.current_econ_tick);
    cmd.corp = corp;
    cmd.verb = corp_verb::take_seat;
    const corp_command_result r = apply_corp_command(m_world, m_registry, cmd);
    if (r != corp_command_result::applied)
        return r;
    m_seat_result.seated = corp;
    finish_seat(/*picked=*/true);
    return r;
}

void app::finish_seat(bool picked)
{
    // The player-scoped history caches were filled through the validation run
    // against whichever corp the GENERATOR provisionally flagged, so at this
    // instant they belong to somebody else. Rebuild the balance series from the
    // seated corp's own filed returns (BL-626 made those world state, which is
    // exactly what lets this be a rebuild rather than a loss) and drop the two
    // flow series, which have no per-quarter record to rebuild from and would
    // otherwise chart a stranger's money.
    m_balance_history.clear();
    m_income_history.clear();
    m_expenditure_history.clear();
    if (const auto cit = m_world.corporations.find(m_world.player_entity);
        cit != m_world.corporations.end())
    {
        for (const quarterly_return& q : cit->second.returns)
            ui::push_capped(m_balance_history, q.balance);
        if (m_balance_history.empty())
            ui::push_capped(m_balance_history, cit->second.balance);
    }

    // Printed alongside the other start_new_game phase lines. AN UNMET FLOOR IS
    // SAID OUT LOUD (CORPORATION_GENERATION.md: "a viability signal to be read,
    // not a failure to be hidden") — nothing here patches a corp to make the
    // shortlist non-empty. The static score is what gated; the balance and the
    // trailing net are printed beside it as the information they now are.
    const char* name = "(none)";
    if (const auto cit = m_world.corporations.find(m_seat_result.seated);
        cit != m_world.corporations.end())
        name = cit->second.name.c_str();
    // THE ONE-is_player INVARIANT (world.hpp), stated where it binds (BL-1044):
    // counted, not assumed. The one path that can break it — a charter walk
    // whose affording centres all failed to place a specialist — is said out
    // loud here and never patched.
    int players = 0;
    for (const auto& kv : m_world.corporations)
        if (kv.second.is_player)
            ++players;
    // BL-1076: say HOW the seat was taken. A pick names its firm id so a log
    // reader can re-issue it (`--seat <id>`); a draw names why nobody was asked.
    // The ranking the canvas offers, in its order, so a log reader (and
    // tools/verify/seat_pick_check.js) can name another firm to pick.
    for (const spawn_seat_candidate& c : m_seat_result.candidates)
        std::printf("[seat] ranked %u landscape %.4f%s\n", static_cast<unsigned>(c.corp),
                    c.landscape, c.shortlisted ? "" : "  (below the floor)");
    if (picked)
        std::printf("[seat] picked %u (take_seat)\n", static_cast<unsigned>(m_seat_result.seated));
    else
        std::printf("[seat] drawn %u — %s\n", static_cast<unsigned>(m_seat_result.seated),
                    m_seat_drawn_because ? m_seat_drawn_because : "no player to ask");
    std::printf("[start_new_game] seat: %s — shortlist %d of %d specialists; is_player on %d "
                "corporation(s)%s%s\n",
                name, m_seat_result.shortlist_size, m_seat_result.specialist_count, players,
                m_seat_result.floor_unmet
                    ? (picked ? "   <-- VIABILITY FLOOR UNMET (every firm marked; the pick stands)"
                              : "   <-- VIABILITY FLOOR UNMET (no seat on scored ground; lowest id seated)")
                    : "",
                players != 1 ? "   <-- ONE-is_player INVARIANT BROKEN (world.hpp)" : "");
    for (const spawn_seat_candidate& c : m_seat_result.candidates)
        if (c.corp == m_seat_result.seated)
            std::printf("[start_new_game] seat card: landscape %.4f (%d of %d holdings on "
                        "scored ground)  balance %.0f cr  trailing net %.0f cr over %d "
                        "quarter(s)  [information, not the gate]\n",
                        c.landscape, c.holdings_scored, c.holdings,
                        static_cast<double>(c.balance), static_cast<double>(c.trailing_net),
                        c.quarters_read);
    std::fflush(stdout);

    // The player is final now: frame the launch view on THEIR holdings.
    frame_launch_view();
}

void app::start_new_game_prelude()
{
    // Phase timing, printed once per campaign start. Added while diagnosing the
    // AppHangB1 kills (NR-209/NR-210): this whole function runs on the main
    // thread with the UI frozen, so its wall clock IS the unresponsive window
    // Windows judges the app by. Keep it — the cost moves as the world grows.
    const auto t0 = std::chrono::steady_clock::now();
    auto mark = [last = t0](const char* phase) mutable {
        const auto now = std::chrono::steady_clock::now();
        std::printf("[start_new_game] %-24s %6lld ms\n", phase,
                    static_cast<long long>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count()));
        std::fflush(stdout);
        last = now;
    };

    // Reset the sim clock so the campaign starts now, not at app construction —
    // constructing a fresh sim_loop rebases its internal timer to the current wall
    // clock. Speed comes from the Lua config loaded by init.lua in run().
    {
        // A user-edited init.lua that omits or renames `config` must fail with a
        // clean message (BL-110 / BL-363), not an unchecked nil->table conversion.
        sol::object cfg_obj = m_lua.state()["config"];
        if (cfg_obj.get_type() != sol::type::table)
            throw std::runtime_error(
                "scripts/init.lua: expected a global table named 'config' "
                "(e.g. config = { default_speed = 2 }); found "
                + std::string(cfg_obj.get_type() == sol::type::lua_nil
                              ? "nothing" : "a non-table value"));
        sol::table cfg = cfg_obj;
        m_sim_loop = sim_loop();
        m_sim_loop.set_speed(cfg.get_or("default_speed", 2));
    }

    // --- BL-1085: THE PRESENTATION HALF, AND NOTHING THAT MOVES THE WORLD ----
    //
    // The world in m_world is FINISHED: the worker that built it (round 6's,
    // or the cold one) ran `finish_campaign_world` -- the genesis history, the
    // survey states, the band, the recipe passes, the landscape search and the
    // winner's apply, the twelve-tick settle -- so the search block that sat
    // here until 2026-09-24 (~20 s on the UI thread, BL-1078's freeze) and the
    // batched validation run poll_worldgen ran after it are gone, obsoleted
    // (Ben, 2026-09-24). What remains is what STARTUP.md § Handoff item 2
    // names: the epoch formatter, the chat line, zoom/focus/frame, the
    // registry's presentation half (reach budget, works, tech tree, the
    // unpriced-basket report, the persona bench). The balance series from the
    // filed returns is seeded in finish_new_game, once the seat is known.
    setup_presentation(m_pending_world_params);
    mark("setup_presentation");
    finish_economy_presentation();
    mark("economy_presentation");
}

void app::finish_new_game()
{
    // THE BALANCE SERIES FROM THE RETURNS THE SETTLE FILED (STARTUP.md
    // § Handoff, item 2; BL-1085). The twelve pre-game ticks ran inside the
    // worker with no presentation half of their own -- no agency comms, no
    // history samples, no last report (Ben's delegated reading, 2026-09-24:
    // the ticks have no calendar and no seated corp, so nothing they emitted
    // had a reader) -- so the header sparkline and the Budget ledger's profit
    // chart are seeded HERE, from the seated corp's own filed returns: the
    // opening capital, then each settle quarter's closing balance, income and
    // expenditure, oldest first, exactly the samples the old validation run
    // recorded live. Seeded after the seat because the series is the player's.
    {
        m_balance_history.clear();
        m_income_history.clear();
        m_expenditure_history.clear();
        const auto pit = m_world.corporations.find(m_world.player_entity);
        if (pit != m_world.corporations.end())
        {
            const std::vector<quarterly_return>& rs = pit->second.returns;
            ui::push_capped(m_balance_history,
                            rs.empty() ? pit->second.balance : rs.front().balance - rs.front().net);
            for (const quarterly_return& r : rs)
            {
                ui::push_capped(m_balance_history, r.balance);
                ui::push_capped(m_income_history, r.income);
                ui::push_capped(m_expenditure_history, r.expenditure);
            }
        }
    }

    // Rebase the clock one last time. sim_loop measures from construction, so without
    // this the wall-clock cost of building the world and running the settle
    // would land as elapsed in-game days on the first frame of play.
    {
        const int speed = m_sim_loop.speed();
        m_sim_loop = sim_loop();
        m_sim_loop.set_speed(speed);
        m_last_orbit_days = 0.0;
        m_last_survey_day = 0;
    }

    m_screen = app_screen::in_game;
}

void app::ensure_works_loaded()
{
    if (m_works.size() > 0) return;
    // A malformed works.lua fails HERE, alongside the other data-layer errors,
    // rather than midway through world generation. load_from_lua validates the
    // table and throws; works_registry.cpp says what it enforces.
    m_lua.load("scripts/works.lua");
    m_works.load_from_lua(m_lua);
}

void app::load_recipe_registry()
{
    // Load the Lua data layer, then build the registry from it (protected calls).
    m_lua.load("scripts/recipes.lua");
    m_lua.load("scripts/economy.lua");
    m_registry.load_from_lua(m_lua);
}

void app::load_economy()
{
    // THE VERIFY PATH (run_verify, its `new_world`): a world built inline by
    // setup_world, unsearched and unsettled, given its economy here. Begin no
    // longer comes this way (BL-1085): its world arrives finished from the
    // worker, registry included, and runs only `finish_economy_presentation`.
    load_recipe_registry();

    // BL-433: gate the roster on the campaign's era band, derived from the epoch
    // year the live world was actually built from. Must happen HERE, after the
    // load (which resets the band to `any`) and before anything browses recipes —
    // the default-recipe authoring below is the first such reader.
    //
    // A 0 CE campaign therefore never sees the Launchpad or the petroleum,
    // propellant and spacecraft chains; a 1960 one (the default since BL-1047)
    // sees everything. Ids are untouched either way: the filter masks, it does
    // not remove. The band is read HERE, after generation, and generation never
    // reads it -- so the epoch that picks it moves no generated world. ONE
    // function names it (`campaign_band_from_epoch`, world/finish_campaign_world):
    // the round-6 worker and the harness mirror band through the same body.
    m_registry.set_era(campaign_band_from_epoch(m_world, m_active_world_params));

    // Author processing recipes onto generated assets. The recipe id is a registry
    // index, unknown at generation time, so it is assigned here once the registry
    // exists.
    //
    // The loop that used to sit here now lives in world/ as assign_default_recipes
    // (2026-08-17). It was a world-generation invariant enforced by the UI's
    // startup sequence, so every path that builds a world WITHOUT app — the
    // headless harnesses, --serve, --verify — ran generated processors that could
    // never produce, reported as ordinary idleness. 20.3% of processing
    // building-ticks in tier_margin, silently dragging the BL-436 calibration.
    assign_default_recipes(m_world, m_registry);

    finish_economy_presentation();
}

void app::finish_economy_presentation()
{
    // BL-323 S2b: mirror the reach budget onto ui_state so every placement surface
    // filters on the same number the authoritative gate uses. Done here, once, right
    // after the registry is in hand — a surface that filtered on a different budget
    // would offer tiles construct_building then refuses.
    m_ui.max_logistics_reach = m_registry.construction().max_logistics_reach;

    // BL-321 Era -1 works table. Usually already loaded by now — world
    // generation needs it and runs first — but kept here so the ordering is
    // stated in both places and neither can quietly stop loading it.
    ensure_works_loaded();

    // BL-087 mock tech/quest tree — display data for the F9 viewer only; no
    // simulation system reads it (the tech system is post-prototype).
    m_lua.load("scripts/tech_tree.lua");
    m_tech_tree.load_from_lua(m_lua);

    // BL-652 — AN INJECTOR THAT CANNOT PRICE A BASKET ENTRY SAYS SO.
    //
    // Both demand-basket injectors skip a resource whose market base_price is 0,
    // and that skip is silent: an authored-but-unpriced basket entry is
    // indistinguishable, from every total downstream, from a channel nobody ever
    // wrote. Two separate bugs hid behind that silence on one day in August 2026,
    // and NEITHER FAILED — each quietly answered a question about a different
    // world. It is always a missing script or an authoring error, never intent.
    //
    // Reported HERE because this is the first moment both halves exist together:
    // the era-resolved baskets (set_era, above) and the generated markets that
    // carry base_price. Once per campaign start, one line per (channel,
    // resource), never per market or per tick. The headless counterpart is
    // demand_census's R5 row, which FAILS rather than warns.
    {
        const std::vector<unpriced_basket_entry> unpriced =
            unpriced_basket_entries(m_world, m_registry);
        for (const unpriced_basket_entry& e : unpriced)
            std::fprintf(stderr,
                         "ProjectIo: [demand-basket] the %s basket wants '%s', which NO market "
                         "prices (base_price 0) - the injector will discard it in silence. "
                         "Either scripts/world_gen.lua omits it from kepler_market.base_price, "
                         "or the basket names a good it should not.\n",
                         e.channel, resource_names::name_of(e.resource).c_str());
        if (!unpriced.empty())
            std::fflush(stderr);
    }

    // Seat the persona counsel mountain bench (BL-207 slice 1). Every non-player
    // corp seats the same bench this slice (bench diversity by industrial focus
    // is later polish); a load failure (missing/malformed pack) disables counsel
    // rather than aborting the session — the chat's agency-event feed (BL-205)
    // stands on its own regardless.
    try
    {
        m_persona_bench = persona::load_bench();
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "ProjectIo: persona counsel packs disabled: %s\n", e.what());
        m_persona_bench.clear();
    }
}

void app::step_economy()
{
    // Per-phase accumulators over the process lifetime, dumped by the
    // validation-run timing in finish_new_game. File-local plumbing for the 2026-08-12 stall
    // hunt; costs two clock reads per phase when nothing reads the totals.
    using clk = std::chrono::steady_clock;
    auto& acc = step_economy_phase_ms();
    auto t = clk::now();
    auto lap = [&](int i) {
        const auto now = clk::now();
        acc[static_cast<std::size_t>(i)] +=
            std::chrono::duration_cast<std::chrono::microseconds>(now - t).count() / 1000.0;
        t = now;
    };

    // THE WORLD HALF IS ONE FUNCTION (BL-1085): `run_settle_tick`, the tick the
    // worker's settle ran twelve times and every harness runs -- convoys,
    // the economy step and dispatch, the clear, the budget and nation step,
    // the tech gates, the standings and the exits, in that order and for the
    // reasons its own header gives. The cadence key (BL-568): this counter
    // advances by exactly one per step across the settle, live play and
    // --verify, which is the schedule every harness certifies. BL-409: under
    // spectate the session has no human seat, so the strategic tier evaluates
    // every corp -- the player's included; default false, so an ordinary
    // played session runs exactly as before.
    std::array<clk::time_point, k_campaign_settle_lap_count + 1> lap_clock{};
    settle_tick_hooks hooks;
    hooks.lap_clock = &lap_clock;
    settle_tick_result tick = run_settle_tick(m_world, m_registry,
                                              static_cast<int>(m_econ_steps++),
                                              static_cast<int>(m_sim_loop.day_tick()),
                                              m_ui.spectating, &hooks);
    for (int i = 0; i < k_campaign_settle_lap_count; ++i)
        acc[static_cast<std::size_t>(i)] +=
            std::chrono::duration_cast<std::chrono::microseconds>(
                lap_clock[static_cast<std::size_t>(i + 1)] - lap_clock[static_cast<std::size_t>(i)])
                .count() / 1000.0;
    t = lap_clock[k_campaign_settle_lap_count];
    // BL-262: this tick's standing profile for the Corporations panel; the
    // report for the ledgers (transient runtime caches, not serialised).
    m_last_econ_report    = std::move(tick.report);
    m_last_corp_standings = std::move(tick.standings);
    const auto& flows     = tick.flows;

    // Post-step presentation (BL-361: extracted to core/session_history.cpp):
    // the nation-voiced agency comms (BL-212), the persona counsel posts
    // (BL-207), and the per-tick history recorders behind the header sparkline,
    // ledger graphs and resource drill-downs. Nothing here feeds back into the
    // simulation.
    {
        const int day = static_cast<int>(m_sim_loop.day_tick());
        session_history::post_nation_agency_comms(m_world, m_last_econ_report, m_chat, day);
        lap(6); // agency comms
        // BL-468: battle traffic to the Field channel. The settle never comes
        // through here (it runs inside the worker, BL-1085), so nothing
        // pre-game posts lines the player never saw.
        session_history::post_battle_dispatches(m_world, m_last_econ_report, m_chat, day);
        // Persona counsel: ~1.05 s/tick measured 2026-08-12 (NR-210 records
        // the open in-game hitch). It was suppressed through the old validation
        // run; the settle now has no presentation half at all.
        session_history::post_persona_counsel(m_world, m_persona_bench,
                                              m_counsel_channel, m_chat, day);
        lap(7); // persona counsel
        session_history::history_stores h{
            m_balance_history, m_income_history, m_expenditure_history,
            m_market_history, m_building_rank_hist, m_body_resource_hist,
            m_tile_resource_hist, m_tracked_tiles, m_resource_hist_days,
            m_resource_sample_index };
        session_history::record_histories(m_world, m_registry, m_last_econ_report,
                                          flows, m_ui, h);
        // Strategy readout (BL-411): consume this tick's new decision-ring
        // entries and advance the rolling window one quarter. Same recorder
        // pattern as record_histories — once per econ tick, never per frame —
        // so the ledger's draw path only ever reads prebuilt aggregates.
        ui::record_strategy_readout(m_world, m_strategy_readout);
    }
    lap(8); // history recorders
}

world_params app::fresh_world_params() const
{
    world_params p{};
    if (m_epoch_year_set) p.epoch_year = m_epoch_year_override;
    return p;
}

void app::setup_world(world_params params)
{
    // THE VERIFY PATH (run_verify, its `new_world`; BL-1085). Begin no longer
    // comes through here: its world arrives from the worker with the genesis
    // history and survey states already in it (`finish_campaign_world`), and
    // Begin runs `setup_presentation` alone. A world is absent here only when
    // the caller did not go through a worker, so it is generated inline.
    // World-gen balance values (BL-236): loaded here, ahead of load_economy's
    // recipes/economy pass, since make_hard_coded_world runs before it. A missing
    // or malformed world_gen.lua throws (BL-110's protected-call-throws-on-error
    // rule), so a broken mod script fails loudly rather than silently reverting.
    if (m_world.bodies.empty())
    {
        world_gen_config gen_cfg;
        m_lua.load("scripts/world_gen.lua");
        gen_cfg.load_from_lua(m_lua);
        m_generation_report = generation_report{};
        ensure_works_loaded(); // BL-321: an input to generation, not to the economy.
        m_world = make_hard_coded_world(params, &m_generation_report, gen_cfg,
                                        nullptr, &m_works);
    }

    // Bridge PLANETOLOGY's per-body dated history + checkpoints into the world
    // history log's genesis + checkpoint chapters (BL-208). generation_report is
    // presentation-only and never otherwise reaches `world` — this is that bridge,
    // and it must run exactly once per generation (not idempotent).
    seed_genesis_history(m_world, m_generation_report);

    setup_presentation(params);

    // Seed survey states (BL-067): home (and the star) open surveyed; every other
    // body opens hidden until the player dispatches a survey.
    init_survey_states(m_world);
    m_last_survey_day = 0;
}

void app::setup_presentation(const world_params& params)
{
    m_active_world_params = params;          // remember the descriptor the live world was built from

    // BL-705: the tick calendar counts from the year the world was GENERATED at,
    // not from a constant. This and the load path below are the only two places
    // m_active_world_params is assigned, so they are the two places that publish
    // it. Display only — nothing simulated reads it back.
    ui::fmt::set_campaign_epoch_year(static_cast<int>(params.epoch_year));

    // Open the comms log with a deterministic epoch line (BL-205): the Public
    // channel exists from campaign start, so the panel is never an empty shell.
    // BL-212: the channel is nation-voiced, so the epoch line counts nations,
    // not the corporations operating quietly beneath them.
    m_chat = ui::chat_state{};
    ui::chat_post(m_chat, 0, null_entity, 0,
                  std::to_string(m_world.nations.size()) +
                      " nations on the public channel.");

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

    // The launch view: framed on the player's holdings, the HQ's tile selected.
    m_launch_body = start_body;
    frame_launch_view();

    // Open on plain terrain — no lens imposed at campaign start. A click only ever
    // updates the Selection element; it never re-skins the canvas, so the canvas
    // should likewise start unskinned and let the player pick a lens deliberately
    // from the strip. (Reverses BL-013's Corporation-default.) Single-select with a
    // null state — re-clicking the active lens clears back here. See LENSES.md.
    m_ui.overlay = overlay_mode::none;

    // The survey day counter restarts with the view (the survey STATES are
    // world state: `init_survey_states`, run once by whoever built the world).
    m_last_survey_day = 0;
}

/// Frame-budget instrument (BL-249), shared by render()'s phase marks and the
/// verify API's frame_csv tap (verify_api.cpp — declared in app.hpp now that the
/// two live in different translation units). A process-wide static rather than an
/// app member for the same reason it was a function-local one: the instrument
/// stays one include plus a handful of calls, liftable after the v0.1.0 audit
/// without touching app's declared state.
ui::frame_stats& frame_stats_instance()
{
    static ui::frame_stats s;
    return s;
}

std::array<double, 11>& step_economy_phase_ms()
{
    static std::array<double, 11> acc{};
    return acc;
}

void app::process_events(bool& running)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ImGui_ImplSDL3_ProcessEvent(&event);
        // Quit on the app-level quit OR the window's close (X) button. SDL3 delivers
        // the title-bar close as SDL_EVENT_WINDOW_CLOSE_REQUESTED, distinct from
        // SDL_EVENT_QUIT; without handling it, clicking X leaves the process running
        // with no visible window. Treat either as "shut the app down".
        if (event.type == SDL_EVENT_QUIT ||
            event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        {
            // NR-256 diagnostic: name which exit fired, so an unattended
            // termination separates "stray close event" from "shell reaped us".
            std::printf("[exit] %s\n", event.type == SDL_EVENT_QUIT
                                           ? "SDL_EVENT_QUIT"
                                           : "SDL_EVENT_WINDOW_CLOSE_REQUESTED");
            std::fflush(stdout);
            running = false;
        }
        else if (event.type == SDL_EVENT_KEY_DOWN)
            handle_key_down(event.key);
        else if (event.type == SDL_EVENT_WINDOW_RESIZED && !m_settings.fullscreen)
        {
            // Remember a free drag-resize so it persists across launches. Only
            // in-memory here; save_settings() runs once on clean exit (run()).
            m_settings.window_w = event.window.data1;
            m_settings.window_h = event.window.data2;
        }
    }
}

void app::handle_key_down(const SDL_KeyboardEvent& key)
{
    // F12 captures regardless of focus — needs the renderer, so it stays outside
    // the binding table.
    if (key.scancode == SDL_SCANCODE_F12)
    {
        m_capture_requested = true;
        return;
    }

    // F11 toggles the frame-budget HUD (BL-249). Outside the binding table for the
    // same reason F12 is: the table routes through ui::canvas_command, and this is a
    // dev instrument, not part of the shared canvas vocabulary the verify scripts
    // drive. Toggle rule — pressing F11 while the HUD is up puts it away, exactly as
    // its own close button does. Handled before the screen guard so the frame budget
    // can be watched on the menu and the generation screen too.
    if (key.scancode == SDL_SCANCODE_F11)
    {
        m_ui.show_frame_hud = !m_ui.show_frame_hud;
        return;
    }

    // No game bindings on the menu or the generation screen — there is nothing to
    // navigate yet (the generation screen carries its own two buttons).
    if (m_screen != app_screen::in_game)
        return;

    // Esc toggles the in-app system menu (BL-070) — cheap keyboard parity with the
    // corner gear button. Handled before the ImGui keyboard guard so it works even
    // while the popup (or another panel) holds focus. Precedence, highest first: an
    // armed exit-confirm backs out; an open system menu closes; an open sticky
    // detail card unwinds one drill level; an open fold overlay folds up (BL-214);
    // otherwise the menu opens. The Selection band never hides (BL-266) — its
    // former hide rung is gone, so the system menu is the terminal rung.
    //
    // The fold rung is a deliberate departure from BL-214's Decision 10, which kept
    // depth off this ladder. That decision reasoned about an in-place stepper, where
    // a level is not a dismissal and `card_stack` already owned the unwind. The
    // binary model made expanded a full-screen MODE, and a mode with no keyboard exit
    // is a defect rather than a principle. It sits BELOW the drill so one press never
    // both unwinds a drill and closes the overlay hosting it — the same rule that put
    // the drill above the card.
    if (key.scancode == SDL_SCANCODE_ESCAPE)
    {
        // A drill can only exist over a real selection: a valid selection whose
        // kind actually resolves — so Esc never unwinds an invisible drill. The
        // band itself is always open (BL-266) and has no hide rung.
        const bool card_open = m_ui.selected_entity != null_entity &&
                               ui::selection_kind_of(m_world, m_ui.selected_entity) !=
                                   ui::selection_kind::none;
        if (m_ui.confirm_exit_pending)
            m_ui.confirm_exit_pending = false;
        else if (m_ui.show_system_menu)
            m_ui.show_system_menu = false;
        else if (card_open && !m_ui.card_stack.empty())
            m_ui.card_stack.pop_back();                       // unwind one drill level
        else if (m_ui.corp_rollup_drill >= 0)
            m_ui.corp_rollup_drill = -1;                      // back to the roll-up (BL-248)
        else if (ui::any_expanded(m_ui))
            ui::fold(m_ui);                                   // fold the full-screen overlay
        else
            m_ui.show_system_menu = true;                     // terminal rung (BL-266): the band never hides
        return;
    }

    // All other bindings are suppressed while ImGui owns the keyboard.
    if (ImGui::GetIO().WantCaptureKeyboard)
        return;

    const bool shift = (key.mod & SDL_KMOD_SHIFT) != 0;
    for (const auto& b : s_bindings)
    {
        if (key.scancode == b.scancode && shift == b.shift)
        {
            dispatch_action(b.cmd);
            return;
        }
    }
}

void app::dispatch_action(ui::canvas_command cmd)
{
    switch (cmd)
    {
        // Time controls — need sim_loop, handled here rather than in apply_canvas_command.
        case ui::canvas_command::pause_toggle:
            if (m_sim_loop.paused())
                m_sim_loop.set_speed(m_prev_speed);
            else
            {
                m_prev_speed = m_sim_loop.speed();
                m_sim_loop.set_speed(0);
            }
            return;
        case ui::canvas_command::speed_1: m_prev_speed = 1; m_sim_loop.set_speed(1); return;
        case ui::canvas_command::speed_2: m_prev_speed = 2; m_sim_loop.set_speed(2); return;
        case ui::canvas_command::speed_3: m_prev_speed = 3; m_sim_loop.set_speed(3); return;
        case ui::canvas_command::speed_4: m_prev_speed = 4; m_sim_loop.set_speed(4); return;
        case ui::canvas_command::speed_5: m_prev_speed = 5; m_sim_loop.set_speed(5); return;

        // Save / load (BL-536). Here rather than in apply_canvas_command for
        // the same reason the time controls are: they touch app members that
        // function never sees. The quick-slot path is deliberately the SIMPLEST
        // thing that works -- one fixed file beside the executable. Slots, named
        // saves and an overwrite prompt are a UI item, not this one.
        case ui::canvas_command::quick_save:
            save_game_to(quick_save_path());
            return;
        case ui::canvas_command::quick_load:
            load_game_from(quick_save_path());
            return;

        // UI toggles.
        case ui::canvas_command::help_toggle:
            m_show_help = !m_show_help;
            return;
        case ui::canvas_command::options_toggle:
            m_show_options = !m_show_options;
            return;
        case ui::canvas_command::tech_tree_toggle:
            m_ui.show_tech_tree = !m_ui.show_tech_tree;
            return;

        // Everything else is a canvas navigation command.
        default:
            ui::apply_canvas_command(m_world, m_ui, cmd);
    }
}

// ---------------------------------------------------------------------------
// Save / load (BL-536)
// ---------------------------------------------------------------------------

std::string app::quick_save_path() const
{
    return std::string("quicksave") + save_game_extension;
}

bool app::save_game_to(const std::string& path)
{
    save_envelope env;

    env.sim_tick     = m_sim_loop.sim_tick();
    env.day_tick     = m_sim_loop.day_tick();
    env.econ_tick    = m_sim_loop.econ_tick();
    env.elapsed_days = m_sim_loop.elapsed_days();
    env.speed        = m_sim_loop.speed();

    env.params = m_active_world_params;
    env.report = m_generation_report;

    env.balance_history     = m_balance_history;
    env.income_history      = m_income_history;
    env.expenditure_history = m_expenditure_history;
    env.market_history      = m_market_history;
    // deque -> vector on the seam; the container choice is an app detail.
    env.building_rank_hist.assign(m_building_rank_hist.begin(), m_building_rank_hist.end());

    env.primary_level     = m_ui.primary_level;
    env.overlay           = m_ui.overlay;
    env.active_body       = m_ui.active_body;
    env.selected_entity   = m_ui.selected_entity;
    env.selected_province = m_ui.selected_province;
    env.solar_zoom        = m_ui.solar_zoom;
    env.solar_pan_x       = m_ui.solar_pan_x;
    env.solar_pan_y       = m_ui.solar_pan_y;
    env.circum_zoom       = m_ui.circum_zoom;
    env.circum_pan_x      = m_ui.circum_pan_x;
    env.circum_pan_y      = m_ui.circum_pan_y;
    env.planetary_zoom    = m_ui.planetary_zoom;
    env.planetary_pan_x   = m_ui.planetary_pan_x;
    env.planetary_pan_y   = m_ui.planetary_pan_y;

    // Make the destination directory before writing. `write_save_game` opens an
    // ofstream, which does NOT create intervening directories, so a save aimed at
    // a path like "build_gen/verify/<name>.iosave" returned false for a reason
    // that reads as "the save is broken" rather than "the folder is missing" —
    // which is exactly how save_load.lua's own fixture path behaved on a tree
    // where nothing had created build_gen/verify yet.
    if (const auto parent = std::filesystem::path{path}.parent_path(); !parent.empty())
    {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }

    const bool ok = write_save_game(path, m_world, env);

    ui::chat_post(m_chat, static_cast<int>(m_sim_loop.day_tick()), null_entity,
                  ui::chat_state::k_field_channel,
                  ok ? ("Saved to " + path) : ("SAVE FAILED: could not write " + path));
    return ok;
}

bool app::load_game_from(const std::string& path)
{
    world         w;
    save_envelope env;

    // Reads into scratch, so a rejected file leaves the running campaign whole.
    if (!read_save_game(path, w, env))
    {
        ui::chat_post(m_chat, static_cast<int>(m_sim_loop.day_tick()), null_entity,
                      ui::chat_state::k_field_channel,
                      "LOAD FAILED: " + path + " is missing, corrupt, or from another version");
        return false;
    }

    m_world               = std::move(w);
    m_generation_report   = std::move(env.report);
    m_active_world_params = env.params;
    // BL-705: a save carries the epoch its world was generated at, so the
    // calendar follows the loaded world rather than the process's --epoch flag.
    ui::fmt::set_campaign_epoch_year(static_cast<int>(env.params.epoch_year));

    m_sim_loop.restore(env.sim_tick, env.day_tick, env.econ_tick, env.elapsed_days, env.speed);
    m_prev_speed     = (env.speed > 0) ? env.speed : 1;
    m_last_econ_tick = env.econ_tick;
    // `world::current_day_tick` is the mirror app refreshes each frame; seeding
    // it here means the activity fog reads the right age on the FIRST frame
    // after a load rather than on the second.
    m_world.current_day_tick = static_cast<int>(env.day_tick);
    // The cadence counter resumes where an unsaved campaign would be: the
    // settle's steps plus every live quarter since (BL-568).
    m_econ_steps = static_cast<uint64_t>(k_campaign_settle_ticks) + env.econ_tick;
    m_world.current_econ_tick = static_cast<int>(m_econ_steps);

    m_balance_history     = std::move(env.balance_history);
    m_income_history      = std::move(env.income_history);
    m_expenditure_history = std::move(env.expenditure_history);
    m_market_history      = std::move(env.market_history);
    m_building_rank_hist.assign(env.building_rank_hist.begin(), env.building_rank_hist.end());

    m_ui.primary_level     = env.primary_level;
    m_ui.overlay           = env.overlay;
    m_ui.active_body       = env.active_body;
    m_ui.selected_entity   = env.selected_entity;
    m_ui.selected_province = env.selected_province;
    m_ui.solar_zoom        = env.solar_zoom;
    m_ui.solar_pan_x       = env.solar_pan_x;
    m_ui.solar_pan_y       = env.solar_pan_y;
    m_ui.circum_zoom       = env.circum_zoom;
    m_ui.circum_pan_x      = env.circum_pan_x;
    m_ui.circum_pan_y      = env.circum_pan_y;
    m_ui.planetary_zoom    = env.planetary_zoom;
    m_ui.planetary_pan_x   = env.planetary_pan_x;
    m_ui.planetary_pan_y   = env.planetary_pan_y;

    // App-side transients that describe the world just replaced. Left alone
    // they would keep drawing the PREVIOUS campaign's numbers until the next
    // econ tick overwrote them -- a stale panel is worse than an empty one.
    m_last_econ_report = economy_report{};
    m_last_corp_standings.clear();
    m_body_resource_hist.clear();
    m_tile_resource_hist.clear();

    // Selection state that indexes into the replaced world.
    m_ui.card_stack.clear();
    m_ui.clear_battle_selection();

    m_screen = app_screen::in_game;

    ui::chat_post(m_chat, static_cast<int>(env.day_tick), null_entity,
                  ui::chat_state::k_field_channel, "Loaded " + path);
    return true;
}

void app::render()
{
    // Frame-budget instrument (BL-249) — the file-local instance above run_verify,
    // shared with the verify frame_csv tap. begin_frame() closes out the PREVIOUS
    // frame, so the period it stamps covers the event pump and the sim step as
    // well as this function; see ui/frame_stats.hpp § Sampling model.
    ui::frame_stats& s_frame_stats = frame_stats_instance();
    s_frame_stats.begin_frame();

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    // BL-521: synthetic pointer input, queued LAST so it outranks whatever the SDL
    // backend fed from the OS cursor above (ImGui applies queued events in order,
    // last write wins). A no-op outside the verify harness — see app.hpp
    // § Synthetic pointer input.
    pump_injected_input();
    ImGui::NewFrame();

    // Main menu and the staged generation screen — drawn instead of the canvases
    // when play has not started. Both share the Render/clear/capture tail below, so
    // either is capturable like any frame.
    if (m_screen != app_screen::in_game)
    {
        if (m_screen == app_screen::building)
            draw_building_screen();
        else if (m_screen == app_screen::choosing_seat)
            draw_seat_screen(); // BL-1076: the corporation selection canvas
        else if (m_screen == app_screen::generating)
            draw_generation_screen();
        else
            draw_main_menu();

        // The pre-play screens are frames like any other, so the instrument follows
        // them too — the generation screen's chain preview is exactly the kind of
        // per-frame work worth watching. Nothing here reserves a shell column, so the
        // HUD's first-use anchor is a plain top-left inset.
        constexpr float hud_inset = 16.0f;
        ui::draw_frame_budget_hud(s_frame_stats, {hud_inset, hud_inset}, m_ui.show_frame_hud);

        s_frame_stats.mark_build_end();
        ImGui::Render();
        SDL_SetRenderDrawColor(m_renderer, 15, 15, 20, 255);
        SDL_RenderClear(m_renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_renderer);
        s_frame_stats.mark_submit_end(*ImGui::GetDrawData());
        if (m_capture_requested)
        {
            save_screenshot();
            m_capture_requested = false;
        }
        s_frame_stats.mark_present_begin();
        SDL_RenderPresent(m_renderer);
        s_frame_stats.mark_present_end();
        return;
    }

    // BL-268 (planetary canvas cull + cache): make sure logistics' per-body raster
    // cache covers the active body before the canvas draws. The canvas holds
    // const world& and reads world.body_tile_index directly, so the one mutable
    // ensure lives here — after the first build this is a single hash lookup.
    if (m_ui.active_body != null_entity)
        body_tile_grid(m_world, m_ui.active_body);

    // Live-mode mouse feed: copy the real OS cursor into ui_state so canvases read
    // a single app-owned source (BL-061). Skipped in --verify (m_golden_dir set) so
    // hover is suppressed by default; a script opts in with verify.mouse(x,y).
    if (m_golden_dir.empty())
    {
        const ImVec2 mp = ImGui::GetIO().MousePos;
        m_ui.mouse = {mp.x, mp.y, true};
    }

    // Shared window geometry. The minimap and the top-right time panels are all
    // sized off the minimap width so the right-hand column stays aligned.
    ImGuiIO&     io     = ImGui::GetIO();
    const ImVec2 disp   = io.DisplaySize;
    constexpr float margin = ui::shell_margin;
    // Single owner for the shell's rect algebra (BL-216, ui/shell_metrics.hpp).
    // This block used to hand-roll `disp.x - margin - mm_w`, and so did four other
    // places -- the time panel, the system gear, the header's right bound and the
    // Selection band each re-derived the same right-chrome edge independently.
    // That is how BL-312's flush-right minimap came to disagree with the four that
    // did not follow it. Every site below now asks shell_metrics for a rect.
    const ui::shell_rect mm        = ui::minimap_rect(disp);
    const float          mm_w      = mm.w;
    const float          mm_h      = mm.h;
    const ImVec2         mm_origin = {mm.x, mm.y};

    // --- Primary canvases (Layer 2) — the zoom ladder ---
    // The primary rung fills the window; the rung one step *out* renders in the
    // minimap inset (bottom-right), framed by chrome (title bar + mode bar).
    // Both draw to the ImGui background draw list, so the panels below render on
    // top. See CANVASES.md / MINIMAP.md.
    {
        // Minimap chrome reserves a title bar above the inset; the neighbouring
        // canvas occupies the rest. The overlay-lens controls used to sit in a
        // mode bar below the inset — they now live in a bottom-left strip
        // (ui::draw_overlay_controls), so the inset takes the full height here.
        const float  title_h      = ImGui::GetTextLineHeight() + 6.0f;
        // BL-093: a lens mode bar occupies the bottom row of the minimap box; the
        // inset canvas is carved down to leave room for it.
        const float  lens_bar_h   = ImGui::GetFrameHeight() + 6.0f;
        const ImVec2 inset_origin = { mm_origin.x, mm_origin.y + title_h };
        const ImVec2 inset_size   = { mm_w, mm_h - title_h - lens_bar_h };

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
                // No CANVAS rung above solar, but there is a rung: the galaxy the
                // system sits in. Replaces the flat branding fill MINIMAP.md
                // § The top rung recorded as a placeholder. Fixed and authored —
                // every campaign is a different world under the same sky.
                ui::draw_star_map(inset_origin, inset_size, static_cast<float>(m_ui.sim_now_days));
                mm_title = "Galaxy";
                break;

            case canvas_level::circumplanetary:
                ui::draw_circumplanetary_canvas(m_world, m_ui, {0.0f, 0.0f}, disp, primary_input, false);
                ui::draw_solar_system_canvas(m_world, m_ui, inset_origin, inset_size, minimap_input, true);
                if (m_world.star_body != null_entity && m_world.bodies.count(m_world.star_body))
                    mm_title = m_world.bodies.at(m_world.star_body).name.c_str();
                break;

            case canvas_level::planetary:
                // Refresh the intra-body vision model (BL-151/152/154) for the active
                // body just before drawing it: permanent building pockets + corp-centre→
                // market corridors, and the live convoy paths the beam head glides along.
                // Here (not the run loop) so the --verify capture path gets it too. Needs
                // a non-const world for the route-cached pathfinder, so it cannot live in
                // the const-world draw. Derived VIEW state — no feedback into the sim.
                ui::update_body_vision(m_world, m_ui, m_ui.sim_now_days);
                // BL-323 S2b: build the logistics reach field for the body about to be
                // drawn, for the same reason and in the same place as the vision update
                // above — the Dijkstra needs a mutable world, the draw is const. Cached,
                // so this is a map lookup on every frame but the first after an
                // invalidation (a road laid, a building placed or demolished).
                body_reach_field(m_world, m_ui.active_body);
                // BL-606: this tick's active-LP anchor pools for the Throughput
                // lens, in the same place and for the same reason — the pool
                // enumeration wants a mutable world, the draw is const. A no-op
                // unless that lens is active, and never cached: LP is a per-tick
                // rate, so the map is rebuilt every frame rather than banked.
                // Ordered AFTER body_reach_field because the lens draws the reach
                // envelope under the anchors and both must describe one frame.
                ui::update_body_throughput(m_world, m_ui, m_registry);
                // BL-732: the baked-ground cache. Bakes the far page on a body
                // switch and a budgeted few full-res chunks against last frame's
                // ground_request, then publishes ui_state::ground for the canvas
                // draw below. Texture work on the render thread, before the
                // draw data is submitted.
                m_ground.tick(m_renderer, m_world, m_ui, m_ground_bake_all);
                // No lens-key anchor argument (Sprint 17b, one chrome home):
                // every legend now asks ui::lens_chrome_rect for the one region —
                // the minimap's header, top right — rather than being handed a
                // point derived here. That derivation was the last hand-rolled
                // shell rect in this block.
                ui::draw_body_surface_canvas(m_world, m_ui, m_registry, m_last_econ_report,
                                             m_generation_report, {0.0f, 0.0f}, disp,
                                             primary_input);
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

        // The top rung used to fill the inset with a flat dark placeholder here,
        // so the full-window solar canvas did not show through. draw_star_map
        // above now owns that rectangle and paints its own opaque background —
        // this fill would land on top of it and erase the sky.

        // Title bar.
        bdl->AddRectFilled(mm_origin, {mm_origin.x + mm_w, mm_origin.y + title_h}, IM_COL32(28, 30, 40, 255));
        bdl->AddText({mm_origin.x + 5.0f, mm_origin.y + 3.0f}, IM_COL32(220, 225, 235, 255), mm_title); // fit-exempt: chrome strip authored to fit at the 1280x720 floor

        // Lens mode bar along the bottom of the minimap box (BL-093). The bar fill
        // is chrome (drawn here); the interactive seven-glyph row is an ImGui window
        // positioned over it. Relocated from the former bottom-left strip so the
        // selection element can own the whole bottom-left corner.
        const float lens_bar_y = mm_origin.y + mm_h - lens_bar_h;
        bdl->AddRectFilled({mm_origin.x, lens_bar_y},
                           {mm_origin.x + mm_w, mm_origin.y + mm_h}, IM_COL32(28, 30, 40, 255));
        bdl->AddLine({mm_origin.x, lens_bar_y}, {mm_origin.x + mm_w, lens_bar_y},
                     IM_COL32(90, 95, 110, 255));

        // Border around the whole minimap box.
        bdl->AddRect(mm_origin, {mm_origin.x + mm_w, mm_origin.y + mm_h}, IM_COL32(90, 95, 110, 255));

        ui::draw_overlay_controls(m_ui, mm_origin.x, lens_bar_y, mm_w);
    }

    // Time panel — top-right (BL-138/BL-313), extracted to ui::draw_time_panel
    // (BL-361). Shares m_prev_speed with the keyboard speed bindings so pause
    // restores whichever tier was last active, wherever it was set.
    ui::draw_time_panel(m_ui, m_sim_loop, m_prev_speed, disp);

    // In-app system menu (BL-070) — the corner gear + popup, extracted to
    // ui::draw_system_menu (BL-361). Esc toggles the same popup (handle_key_down).
    ui::draw_system_menu(m_ui, m_sim_loop, m_prev_speed, m_quit_requested, disp);

    // Corporation profile — top-left corner, above the navigation pane.
    ui::draw_profile_panel(m_world);

    // Budget + resource header — spans the top between the identity tile and the
    // time column, clear of both. Starts at the shell column's right edge (x = W):
    // the identity tile / balance bar / Selection all clear the same permanent W (BL-122).
    {
        const float header_left  = ui::shell_column_width(disp.x);
        // Flush to the right chrome column, no gutter (Ben, 2026-08-24: widen the
        // time controls "to the same x/y as the corner of our main balance bar").
        // The `- margin` that used to stand here was the top band's only internal
        // gap, and the BOTTOM band has never had one — selection_band_rect is
        // deliberately flush against both its neighbours on the argument that
        // "introducing the shell's only gap here would read as a mistake". It was
        // the top band that carried the mistake.
        const float header_right = ui::right_chrome_left(disp);
        ui::draw_header_panel(m_world, m_balance_history, m_last_econ_report, header_left, header_right);
    }

    // Comms chat log (BL-205) — BL-227 re-homed it from the right chrome column
    // (where it sat between the time panel and the minimap) to the BOTTOM-LEFT,
    // sharing the Selection band's top edge and exact height so the bottom of the
    // screen reads as one horizontal strip: [comms][selection band].
    //
    // Comms is ambient — nation-voiced public chatter you read when you notice it
    // (BL-212), not a decision surface — so it does not need the prime right-edge
    // space directly under the time panel. The fold-out column shortens to clear
    // it, which is why every menu and ledger is now permanently shorter.
    {
        const ui::foldout_rect c = ui::comms_dock_rect();
        ui::draw_chat_panel(m_world, m_chat, static_cast<int>(m_sim_loop.day_tick()),
                            c.x, c.y, c.w, c.h);
    }

    // Left navigation pane and the menus it opens. Starts below the profile.
    ui::draw_nav_pane(m_ui, ui::profile_panel_height);

    // A *new* entity selection no longer competes with the fold-out ledgers for
    // screen space (BL-213 — the Selection band lives in its own fixed rect at
    // the bottom of the screen, sandwiched between the shell column and the
    // right chrome column), so selecting something leaves whatever ledger is
    // open untouched. Only the drill-down stack still resets on a new selection.
    if (m_ui.selected_entity != m_prev_selection)
    {
        m_ui.card_stack.clear();        // a new selection resets any drill-down (BL-196)
        m_ui.card_resource_page = 0;    // ...and its tile resource accordion page
        m_ui.selection_building_page = 0; // ...and the building card's accordion page
        m_prev_selection = m_ui.selected_entity;
    }

    ui::draw_tile_inspector(m_world, m_ui, m_generation_report, m_active_world_params,
                            &m_ui.show_tile_ledger);
    // Generation Ledger (BL-303) — regenerates the per-pass record on demand from
    // the report's tile-pass inputs; nothing it reads is held on the world.
    ui::draw_generation_ledger(m_world, m_ui, m_generation_report, &m_ui.show_generation_ledger);
    // AI decision feed (BL-407) — a reader over world::ai_decisions and the
    // history log's `decision` topic. NOT `agency`: the strategic agency entries
    // are a second narration of the same decisions (pushed one-for-one in the
    // same block in corp_ai.cpp), so merging them would double every row, and
    // the BL-079 reflex-tier entries are not scorer decisions at all — they
    // carry neither reason nor score. See NR-227.
    // Read-only: const world in, and it writes nothing but its own ui_state filters.
    ui::draw_decision_feed(m_world, m_ui, &m_ui.show_decision_feed);
    // Strategy readout (BL-411) — the feed's aggregate companion: verb mix,
    // spend buckets and reason tally per corp over the rolling window kept by
    // record_strategy_readout (step_economy). Read-only over world and state;
    // score/margin fields are deliberately absent from it (NR-226 fence).
    ui::draw_strategy_readout(m_world, m_strategy_readout, m_ui,
                              &m_ui.show_strategy_readout);
    // Construction panel — an ordinary fold-out tab in the shell column (BL-122),
    // one of the mutually-exclusive column occupants (ledgers + Selection).
    ui::draw_construction_panel(m_world, m_registry, m_last_econ_report, m_ui, &m_ui.show_construction_panel);
    // Market ledger — Goods and Trades. It takes a NON-CONST world and the recipe
    // registry because the Trades tab's potential-trade read prices real convoy
    // legs through `price_convoy_leg` (which warms the A* cache and mutates no
    // game state), rather than inventing a second haulage model that could
    // disagree with the one that actually bills the player.
    ui::draw_market_ledger(m_world, m_registry, m_ui, m_market_history, m_ui.show_market_ledger);
    // Convoys ledger (BL-689) — nav slot 7, directly after Market. Was the Market
    // ledger's third tab until the Goods flattening deleted that strip; it left
    // rather than being re-homed because a convoy is cargo in transit and belongs
    // to SUPPLY.md, not to the doc that owns clearing and the order book.
    ui::draw_convoys_ledger(m_world, m_ui, m_ui.show_convoys_ledger);
    {
        // Budget ledger (BL-171): profit chart reads the income/expenditure series;
        // the rank table's change column reads the ranking from ~4 econ ticks back.
        const ui::player_plot_history bhist{m_balance_history, m_income_history, m_expenditure_history};
        static const std::unordered_map<entity_id, int> k_no_prior;
        const auto& prior_rank = (m_building_rank_hist.size() >= 5)
                                     ? m_building_rank_hist.front() : k_no_prior;
        ui::draw_balance_ledger(m_world, m_registry, m_last_econ_report, bhist,
                                prior_rank, m_ui, m_ui.show_balance_ledger);
    }
    // Acquisitions ledger — nav slot 5, and the Company lens's click
    // destination. Which firms can be bought outright and at what price, plus
    // the full-canvas profitability fold-out over every corporation's filed
    // return. Every ledger here is guarded by its own open flag and at most one
    // is set.
    ui::draw_acquisitions_ledger(m_world, m_registry, m_ui,
                                 m_ui.show_acquisitions_ledger);
    // NO CONTRACTS LEDGER, AND NO MERCENARY CONTRACT AT ALL (BL-693, then
    // NR-731 on 2026-08-30). The SELL side of CONTRACTS.md is gone to the
    // world layer: records, passes, serialisation and comms traffic. What is
    // left of the word "contract" here is PROCUREMENT, the BUY side, which is
    // live and untouched.
    // Company ledger (BL-666) — where a Company-lens click lands. Drawn with the
    // rail ledgers because it occupies the same fold-out column, but it has no
    // rail slot: only a canvas click opens it. A declared placeholder for now.
    ui::draw_company_ledger(m_world, m_ui, m_ui.show_company_ledger);
    // Corporation dashboard (BL-248) — nav slot 1, MENU.md's long-named surface.
    // Replaces the all-corporations balance table that used to occupy this slot: a
    // comparison table is not "the player corporation at a glance", and the Economy
    // panel's Corps view already carries it.
    ui::draw_corporation_dashboard(m_world, m_registry, m_last_econ_report, m_ui,
                                   m_ui.show_corporation_panel);

    // The displaced table itself, restored (NR-012). BL-248 deleted it as a duplicate
    // of a second aggregate view; Ben did not intend a deletion, so it is back
    // and reachable from nav slot 8 until its real home is chosen. Deleting a file
    // because a similar view exists is the call that was wrong here — dormant beats
    // deleted, since intent is not recoverable from a diff.
    ui::draw_corporation_panel(m_world, m_last_corp_standings, m_ui, m_ui.show_corporations_table);

    // Tech tree era-selector menu (BL-310 round 4) — nav slot 4 (Research), a
    // real fold-out ledger like every other menu now, not chrome bolted onto
    // the canvas takeover. draw_tech_tree_panel (below, F11 HUD's neighbour)
    // is the canvas itself; this is where the era is actually chosen.
    ui::draw_tech_tree_menu(m_tech_tree, m_ui);

    // Selection band (BL-213 — supersedes the BL-194/195 Selection band) — a FIXED
    // rect at the bottom of the screen, sandwiched between the shell column and
    // the right chrome column, which both run the full screen height either
    // side of it (Ben, 2026-07-28). It no longer follows the click position and
    // no longer competes with the fold-out ledgers for the column. Drawn after
    // the other chrome so it z-orders on top of it.
    {
        // The band starts at the COMMS DOCK's right edge, not the shell column
        // edge: the dock narrowed to 3/4 of the column (2026-07-30) and the band
        // takes the quarter it gave back, so the bottom strip stays solid rather
        // than showing a canvas sliver between the two. The whole rect -- both
        // flush edges and the shared strip height -- comes from shell_metrics
        // (BL-216), so the band and the dock cannot disagree about where the
        // seam between them falls.
        const ui::shell_rect band = ui::selection_band_rect(disp);
        const ImVec2 band_origin = { band.x, band.y };
        const ImVec2 band_size   = { band.w, band.h };
        const ui::resource_history_view rhist{ &m_body_resource_hist,
                                               &m_tile_resource_hist,
                                               &m_resource_hist_days };
        ui::draw_selection_band(m_world, m_registry, m_last_econ_report,
                                rhist, m_ui, band_origin, band_size);
    }

    // The shell fold-out column is ledgers-only (BL-195), and it now really is: the
    // tile-contextual build bar used to be a second, non-ledger tenant drawn here, and
    // is a SECTION of the Construction ledger's Construction view since 2026-08-29.
    // The card's Construct button opens that ledger on that view; nothing is drawn
    // beside the ledgers any more.

    // Execute any construction request queued this frame by the build front door
    // (tile Selection element) or a placement-mode canvas click. Centralised here
    // so the const-world UI surfaces only enqueue; the world mutation happens once,
    // against app's mutable world. See construction.hpp / SELECTION.md.
    if (m_ui.construction.pending_tile != null_entity)
    {
        entity_id built = null_entity;
        const construction_result r = construct_building(
            m_world, m_registry, m_world.player_entity,
            m_ui.construction.pending_tile, m_ui.construction.pending_type,
            m_ui.construction.pending_target, built, m_ui.construction.pending_recipe);
        switch (r)
        {
            case construction_result::placed:
                // BL-095: placement now only *starts* a durative, material-gated build
                // (ticks_remaining > 0), so the toast reflects that rather than claiming
                // it is done — the Selection card carries the live rate / ETA / paused
                // status from here on.
                m_ui.construction.last_message    = "Construction started.";
                // BL-162: the build bar must SURVIVE the build it initiated. It reads
                // `selected_entity` as its target tile, so reselecting the new BUILDING
                // makes the selection non-tile and the bar falls back to "Select a tile" —
                // the player would have to reselect the tile to place a second building
                // on it (a rich tile stacks up to 4 extraction sites), and would lose
                // "Construction started.", which is drawn inside the bar. So reselect
                // only when some OTHER surface (a placement-mode canvas click) initiated
                // the build, i.e. when the build bar is not the surface on screen.
                {
                    const bool bar_showing = m_ui.show_construction_panel &&
                                             m_ui.construction.panel_view == 0;
                    if (!bar_showing)
                        m_ui.selected_entity = built;         // inspect the new building
                }
                break;
            case construction_result::invalid_tile:
                m_ui.construction.last_message = "Can't build there."; break;
            case construction_result::insufficient_funds:
                m_ui.construction.last_message = "Can't afford it."; break;
            case construction_result::slot_occupied:
                m_ui.construction.last_message = "Already placed on this body."; break;
            case construction_result::insufficient_materials:
                m_ui.construction.last_message = "Not enough materials."; break;
            case construction_result::tech_locked:
                // BL-344: name the missing technology rather than just refusing —
                // BL-071's teach-the-player-why rule, applied to the tech gate.
                m_ui.construction.last_message =
                    "Not researched yet: " +
                    gating_tech_for(m_ui.construction.pending_type) + "."; break;
            case construction_result::era_locked:
                // BL-433: distinct from tech_locked on purpose — no amount of
                // research reaches this one, so saying "not researched yet" would
                // send the player looking for a tech that does not exist.
                m_ui.construction.last_message = "Not in this era."; break;
            default:
                m_ui.construction.last_message = "Construction failed."; break;
        }
        m_ui.construction.pending_tile   = null_entity; // consume the request
        m_ui.construction.pending_recipe = no_recipe;   // and the recipe it carried
    }

    // Execute any road-placement request queued this frame by the build front door's Track/Road/
    // Highway affordances (BL-147 core, BL-172 tier). A road is a per-tile mutation (raises
    // road_level, lowers A* cost), not a building, so it routes through place_road.
    // BL-469: the battle card's Withdraw press. Deferred like every other press,
    // because UI surfaces hold only `const world&` — and additionally because the
    // request is HONOURED at the next tick boundary rather than applied here, so
    // the withdrawal window stays a window (battle_system.cpp).
    if (m_ui.construction.pending_withdraw_province != 0)
    {
        corp_command cmd;
        cmd.tick         = static_cast<int>(m_sim_loop.day_tick());
        cmd.corp         = m_world.player_entity;
        cmd.verb         = corp_verb::withdraw_from_battle;
        cmd.province     = m_ui.construction.pending_withdraw_province;
        cmd.counterparty = m_ui.construction.pending_withdraw_against;
        const auto r = apply_corp_command(m_world, m_registry, cmd);
        m_ui.construction.last_message =
            (r == corp_command_result::applied)
                ? "Breaking off — it takes effect next tick."
                : "Cannot break off from that battle.";
        m_ui.construction.pending_withdraw_province = 0; // consume the request
        m_ui.construction.pending_withdraw_against  = null_entity;
    }

    if (m_ui.construction.pending_road_tile != null_entity)
    {
        const construction_result r = place_road(
            m_world, m_registry, m_world.player_entity, m_ui.construction.pending_road_tile,
            m_ui.construction.pending_road_tier);
        switch (r)
        {
            case construction_result::placed:
            {
                static const char* const kName[3] = { "Track", "Road", "Highway" };
                const std::uint8_t t = m_ui.construction.pending_road_tier;
                const std::size_t  i = (t < 1u ? 1u : (t > 3u ? 3u : t)) - 1u;
                m_ui.construction.last_message = std::string(kName[i]) + " built.";
                break;
            }
            case construction_result::invalid_tile:
                m_ui.construction.last_message = "Can't build a road there."; break;
            case construction_result::insufficient_funds:
                m_ui.construction.last_message = "Can't afford the road."; break;
            default:
                m_ui.construction.last_message = "Road placement failed."; break;
        }
        m_ui.construction.pending_road_tile = null_entity; // consume the request
    }

    // Execute any hire-unit request queued this frame by the build front door's
    // Hire affordance (BL-324). Routes through the same corp_verb seam corp_ai
    // scores for rival corps — the player takes no shortcut around it.
    if (m_ui.construction.pending_hire_tile != null_entity)
    {
        corp_command cmd;
        cmd.corp      = m_world.player_entity;
        cmd.verb      = corp_verb::hire_unit;
        cmd.tile      = m_ui.construction.pending_hire_tile;
        cmd.unit_type = m_ui.construction.pending_hire_unit_type;
        entity_id hired = null_entity;
        const corp_command_result r = apply_corp_command(m_world, m_registry, cmd, &hired);
        switch (r)
        {
            case corp_command_result::applied:
                m_ui.construction.last_message = "Unit raised.";
                m_ui.selected_entity           = hired; // inspect the new unit
                break;
            case corp_command_result::rejected_funds:
                // Either leg: the BL-394 credit cost or the gated-axis
                // resource draw — the seam refuses both through one result.
                m_ui.construction.last_message = "Can't afford or supply it."; break;
            default:
                m_ui.construction.last_message = "Hiring failed."; break;
        }
        m_ui.construction.pending_hire_tile = null_entity; // consume the request
    }

    // Execute a convoy-dispatch request queued this frame by the market
    // Selection card's dispatch form (BL-607). Routes through the same
    // `dispatch_convoy` corp_verb the AI's own directed dispatch and the wire
    // seam use (SUPPLY.md § Dispatch trigger: "the auto-dispatch body above
    // with the shortfall scan removed") — the player's convoy costs, travels
    // and picks a mode exactly like a rival's. Unlike the order-book presses
    // above, the result IS surfaced: a dispatch can fail for reasons the form
    // cannot fully pre-check (no viable route, insufficient funds at commit
    // time), and a rejection mutates nothing, so the player needs to be told why.
    if (m_ui.construction.pending_dispatch_source != null_entity)
    {
        corp_command cmd;
        cmd.tick         = static_cast<int>(m_sim_loop.day_tick());
        cmd.corp         = m_world.player_entity;
        cmd.verb         = corp_verb::dispatch_convoy;
        cmd.subject      = m_ui.construction.pending_dispatch_source;
        cmd.counterparty = m_ui.construction.pending_dispatch_dest;
        cmd.target       = m_ui.construction.pending_dispatch_good;
        cmd.quantity     = m_ui.construction.pending_dispatch_qty;
        const corp_command_result r = apply_corp_command(m_world, m_registry, cmd);
        switch (r)
        {
            case corp_command_result::applied:
                m_ui.construction.last_message = "Convoy dispatched."; break;
            case corp_command_result::rejected_state:
                m_ui.construction.last_message = "Not enough stock on hand to dispatch that much."; break;
            case corp_command_result::rejected_placement:
                m_ui.construction.last_message = "No viable route to that market."; break;
            case corp_command_result::rejected_funds:
                m_ui.construction.last_message = "Can't afford the haul."; break;
            case corp_command_result::rejected_no_lp:
                m_ui.construction.last_message =
                    "No Logistic Points left at the source anchor this tick."; break;
            default:
                m_ui.construction.last_message = "Dispatch failed."; break;
        }
        m_ui.construction.pending_dispatch_source = null_entity; // consume the request
    }

    // Execute a demolition queued this frame by the building Selection element. The
    // selection is cleared on success: the entity it pointed at no longer exists, and
    // leaving it dangling would leave the panel resolving a dead id.
    if (m_ui.construction.pending_demolish != null_entity)
    {
        const entity_id doomed = m_ui.construction.pending_demolish;
        if (demolish_building(m_world, m_world.player_entity, doomed))
        {
            m_ui.construction.last_message = "Demolished.";
            if (m_ui.selected_entity == doomed)
                m_ui.selected_entity = null_entity;
        }
        else
        {
            m_ui.construction.last_message = "Couldn't demolish that.";
        }
        m_ui.construction.pending_demolish = null_entity; // consume the request
    }

    // (BL-343's law enact/repeal executor lived here until BL-480: a law has an
    // author, and enactment is the author nation's act — no player surface
    // enqueues a law flip any more.)

    // Execute any survey dispatch queued this frame by the Selection-panel Survey
    // button. Centralised here (like construction) so the const-world UI surfaces
    // only enqueue; the balance debit + schedule arming happen once against app's
    // mutable world. See survey_system.hpp / SOLAR.md § Survey.
    if (m_ui.pending_survey_dispatch != null_entity)
    {
        dispatch_survey(m_world, m_ui.pending_survey_dispatch);
        m_ui.pending_survey_dispatch = null_entity; // consume the request
    }

    // March / Halt / Disband (BL-575) — the unit card's three presses, all
    // routed through the SAME corp_verb seam corp_ai scores for rival units
    // (MILITARY.md § Marching); the player takes no shortcut around it. March
    // is two-step: the card press only ARMS `pending_march_unit`, and the
    // Planetary canvas fills `pending_march_dest_province` on the qualifying
    // province click (body_surface_canvas.cpp), so this only fires once both
    // halves are in.
    if (m_ui.pending_march_unit != null_entity && m_ui.pending_march_dest_province != 0)
    {
        corp_command cmd;
        cmd.tick     = static_cast<int>(m_sim_loop.day_tick());
        cmd.corp     = m_world.player_entity;
        cmd.verb     = corp_verb::march_unit;
        cmd.subject  = m_ui.pending_march_unit;
        cmd.province = m_ui.pending_march_dest_province;
        const auto r = apply_corp_command(m_world, m_registry, cmd);
        switch (r)
        {
            case corp_command_result::applied:
                m_ui.construction.last_message = "Marching."; break;
            case corp_command_result::rejected_state:
                // Already there, or in a battle — MILITARY.md § Marching's
                // "walking away from contact is a priced withdrawal, never a
                // march" (withdraw_from_battle is the card's separate press).
                m_ui.construction.last_message = "Can't march — already there, or in a fight.";
                break;
            default:
                m_ui.construction.last_message = "Can't march there."; break;
        }
        m_ui.pending_march_unit          = null_entity; // consume the request
        m_ui.pending_march_dest_province = 0;
    }

    if (m_ui.pending_halt_unit != null_entity)
    {
        corp_command cmd;
        cmd.tick    = static_cast<int>(m_sim_loop.day_tick());
        cmd.corp    = m_world.player_entity;
        cmd.verb    = corp_verb::halt_unit;
        cmd.subject = m_ui.pending_halt_unit;
        const auto r = apply_corp_command(m_world, m_registry, cmd);
        m_ui.construction.last_message =
            (r == corp_command_result::applied) ? "Halted." : "Already halted.";
        m_ui.pending_halt_unit = null_entity; // consume the request
    }

    if (m_ui.pending_disband_unit != null_entity)
    {
        const entity_id doomed = m_ui.pending_disband_unit;
        corp_command    cmd;
        cmd.tick    = static_cast<int>(m_sim_loop.day_tick());
        cmd.corp    = m_world.player_entity;
        cmd.verb    = corp_verb::disband_unit;
        cmd.subject = doomed;
        const auto r = apply_corp_command(m_world, m_registry, cmd);
        if (r == corp_command_result::applied)
        {
            m_ui.construction.last_message = "Disbanded.";
            // Same dangling-selection guard as the demolish path just above:
            // the entity no longer exists, and manpower gets no refund
            // (MILITARY.md § Marching), so there is nothing left to inspect.
            if (m_ui.selected_entity == doomed)
                m_ui.selected_entity = null_entity;
        }
        else
        {
            m_ui.construction.last_message = "Couldn't disband that.";
        }
        m_ui.pending_disband_unit = null_entity; // consume the request
    }

    // Order-book presses (BL-293) — same seam-consuming shape as the survey
    // dispatch above, applied through apply_corp_command so the player's press
    // and a rival corp's command share one implementation. A rejection is not
    // reported to the surface: the ledger's own form already enforces the same
    // preconditions, so a rejection here means a race (the body's market gone,
    // the order already removed) and the correct response is to do nothing.
    for (corp_command& cmd : m_ui.pending_order_commands)
    {
        cmd.tick = static_cast<int>(m_sim_loop.day_tick());
        apply_corp_command(m_world, m_registry, cmd);
    }
    m_ui.pending_order_commands.clear(); // consume the requests

    // F1 key-binding cheat-sheet — generated from s_bindings so it never drifts.
    if (m_show_help)
    {
        ImGui::SetNextWindowPos(
            ImVec2{disp.x * 0.5f, disp.y * 0.5f}, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
        ImGui::SetNextWindowSize(ImVec2{340.0f, 0.0f}, ImGuiCond_Appearing);
        if (ImGui::Begin("Key Bindings", &m_show_help,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
        {
            if (ImGui::BeginTable("##keys", 2,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Action");
                ImGui::TableSetupColumn("Key");
                ImGui::TableHeadersRow();
                for (const auto& b : s_bindings)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(b.label);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextDisabled("%s", b.key_name);
                }
                // F11/F12 stay outside the table (F12 needs the renderer; F11 is a
                // dev instrument, not part of the shared canvas command vocabulary),
                // so they are listed by hand to keep the cheat-sheet complete.
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Frame budget HUD");
                ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("F11");
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Screenshot");
                ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("F12");
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    // F10 display / options window (BL-076). Self-contained; changes apply live to
    // the SDL window and persist to options.cfg immediately.
    if (m_show_options)
    {
        ImGui::SetNextWindowPos(
            ImVec2{disp.x * 0.5f, disp.y * 0.5f}, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
        ImGui::SetNextWindowSize(ImVec2{320.0f, 0.0f}, ImGuiCond_Appearing);
        if (ImGui::Begin("Options", &m_show_options,
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
        {
            ImGui::SeparatorText("Display");

            struct preset { int w, h; const char* label; };
            static const preset presets[] = {
                {1280, 720,  "1280 x 720"},
                {1600, 900,  "1600 x 900"},
                {1720, 1080, "1720 x 1080"},
                {1920, 1080, "1920 x 1080"},
                {2560, 1440, "2560 x 1440"},
            };
            // Selection = the matching preset, or "Custom" for a free-dragged size.
            int cur = -1;
            for (int i = 0; i < IM_ARRAYSIZE(presets); ++i)
                if (presets[i].w == m_settings.window_w && presets[i].h == m_settings.window_h)
                    cur = i;
            const char* preview = (cur >= 0) ? presets[cur].label : "Custom";

            ImGui::BeginDisabled(m_settings.fullscreen);
            if (ImGui::BeginCombo("Resolution", preview))
            {
                for (int i = 0; i < IM_ARRAYSIZE(presets); ++i)
                {
                    if (ImGui::Selectable(presets[i].label, i == cur))
                    {
                        m_settings.window_w = presets[i].w;
                        m_settings.window_h = presets[i].h;
                        apply_display_settings();
                        save_settings();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();

            if (ImGui::Checkbox("Fullscreen", &m_settings.fullscreen))
            {
                apply_display_settings();
                save_settings();
            }
            if (ImGui::Checkbox("VSync", &m_settings.vsync))
            {
                apply_display_settings();
                save_settings();
            }

            ImGui::SeparatorText("Accessibility");
            static const char* ui_scale_labels[] = {"1.0x", "1.25x", "1.5x"};
            if (ImGui::BeginCombo("UI Scale", ui_scale_labels[m_settings.ui_scale_step]))
            {
                for (int i = 0; i < IM_ARRAYSIZE(ui_scale_labels); ++i)
                {
                    if (ImGui::Selectable(ui_scale_labels[i], i == m_settings.ui_scale_step))
                    {
                        m_settings.ui_scale_step = i;
                        apply_ui_scale();
                        save_settings();
                    }
                }
                ImGui::EndCombo();
            }

            // Live window size (also reflects a free drag-resize of the frame).
            int win_w = 0, win_h = 0;
            SDL_GetWindowSize(m_window, &win_w, &win_h);
            ImGui::TextDisabled("Window: %d x %d", win_w, win_h);

            ImGui::Separator();
            if (ImGui::Button("Close"))
                m_show_options = false;
        }
        ImGui::End();
    }

    // F9 mock tech-tree viewer (BL-087), also reachable from nav rail slot 4
    // (BL-310). Read-only design aid over scripts/tech_tree.lua; no simulation
    // coupling.
    ui::draw_tech_tree_panel(m_tech_tree, m_world, m_world.player_entity,
                             m_ui.show_tech_tree, m_ui.tech_tree_view,
                              m_ui.tech_tree_pan_x, m_ui.tech_tree_pan_y, m_ui.tech_tree_zoom);

    // F11 frame-budget HUD (BL-249) — the v0.1.0 audit instrument. Drawn last so it
    // measures a full frame's worth of panels, and anchored (first use only; it is
    // movable after) into the canvas area's top-left corner, clear of the shell
    // column and the header strip rather than at a fixed pixel offset.
    ui::draw_frame_budget_hud(
        s_frame_stats,
        {ui::shell_column_width(disp.x) + margin, ui::header_panel_height + margin},
        m_ui.show_frame_hud);

    s_frame_stats.mark_build_end();
    ImGui::Render();
    SDL_SetRenderDrawColor(m_renderer, 15, 15, 20, 255);
    SDL_RenderClear(m_renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_renderer);
    s_frame_stats.mark_submit_end(*ImGui::GetDrawData());

    // Capture before present so the screenshot is the exact composited frame.
    if (m_capture_requested)
    {
        save_screenshot();
        m_capture_requested = false;
    }

    s_frame_stats.mark_present_begin();
    SDL_RenderPresent(m_renderer);
    s_frame_stats.mark_present_end();
}

namespace {
constexpr const char* k_settings_path = "options.cfg";
} // namespace

void app::load_settings()
{
    std::ifstream in(k_settings_path);
    if (!in)
        return; // no file yet — keep defaults

    std::string line;
    while (std::getline(in, line))
    {
        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = line.substr(0, eq);
        const std::string val = line.substr(eq + 1);
        try
        {
            if      (key == "window_w")   m_settings.window_w   = std::stoi(val);
            else if (key == "window_h")   m_settings.window_h   = std::stoi(val);
            else if (key == "fullscreen") m_settings.fullscreen = (std::stoi(val) != 0);
            else if (key == "vsync")      m_settings.vsync      = (std::stoi(val) != 0);
            else if (key == "ui_scale_step") m_settings.ui_scale_step = std::stoi(val);
        }
        catch (const std::exception&) { /* skip malformed value */ }
    }

    // Clamp to the supported display floor (BL-215: 1280x720 @ 1.0x is the
    // smallest supported display) so a corrupt or stale file can't open the
    // shell below the size it is authored against.
    m_settings.window_w = std::max(1280, m_settings.window_w);
    m_settings.window_h = std::max(720, m_settings.window_h);
    m_settings.ui_scale_step = std::clamp(m_settings.ui_scale_step, 0, 2);
}

void app::save_settings() const
{
    std::ofstream out(k_settings_path, std::ios::trunc);
    if (!out)
    {
        SDL_Log("Options save failed: could not open %s", k_settings_path);
        return;
    }
    out << "window_w="   << m_settings.window_w        << '\n'
        << "window_h="   << m_settings.window_h        << '\n'
        << "fullscreen=" << (m_settings.fullscreen ? 1 : 0) << '\n'
        << "vsync="      << (m_settings.vsync ? 1 : 0)      << '\n'
        << "ui_scale_step=" << m_settings.ui_scale_step     << '\n';
}

void app::apply_display_settings()
{
    if (!m_window)
        return;

    // Windowed size only applies out of fullscreen; set it first so leaving
    // fullscreen later restores the intended dimensions.
    SDL_SetWindowSize(m_window, m_settings.window_w, m_settings.window_h);
    SDL_SetWindowFullscreen(m_window, m_settings.fullscreen);
    if (m_renderer)
        SDL_SetRenderVSync(m_renderer, m_settings.vsync ? 1 : 0);
}
