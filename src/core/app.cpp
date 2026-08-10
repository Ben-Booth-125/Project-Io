#include "app.hpp"

#include "png_writer.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>

#include "ui/body_surface_canvas.hpp"
#include "ui/canvas_command.hpp"
#include "ui/charts.hpp"
#include "ui/circumplanetary_canvas.hpp"
#include "ui/construction_panel.hpp"
#include "ui/detail_level.hpp" // the drill-through fold idiom (BL-214)
#include "ui/balance_ledger.hpp"
#include "ui/corporation_dashboard.hpp" // nav slot 1, the four roll-ups (BL-248)
#include "ui/corporation_panel.hpp"     // all-corporations table, restored to slot 8 (NR-012)
#include "ui/economy_panel.hpp"
#include "ui/market_ledger.hpp"
#include "ui/chat_panel.hpp"
#include "ui/fonts.hpp"
#include "ui/generation_charts.hpp" // the shared chain-stage charts (BL-211)
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
#include "ui/text_fit.hpp" // overflow ledger + verify.clipping bindings (BL-215)
#include "ui/tile_inspector.hpp"
#include "ui/view_nav.hpp"
#include "world/budget_system.hpp"
#include "world/construction.hpp"
#include "world/tech_gate.hpp" // BL-344: gating_tech_for (refusal names the missing tech)
#include "world/corp_ai.hpp"
#include "world/history_log.hpp"
#include "world/placement_rules.hpp"
#include "world/survey_system.hpp"
#include "world/hard_coded_world.hpp"

#include <chrono> // poll_wizard_surface's zero-wait future probe
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/orbital_system.hpp"
#include "world/supply_system.hpp"

#include <algorithm>
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

static const std::array<key_binding, 22> s_bindings = {{
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
    {SDL_SCANCODE_F1,           false, ui::canvas_command::help_toggle,  "Key bindings",      "F1"},
    {SDL_SCANCODE_F9,           false, ui::canvas_command::tech_tree_toggle, "Tech tree (mock)", "F9"},
    {SDL_SCANCODE_F10,          false, ui::canvas_command::options_toggle, "Options",         "F10"},
}};

namespace {

/// The speed tier's rate as a multiplier string ("0.25×" … "16×"), derived from
/// `sim_loop::speed_multiplier` so the label can never drift from the curve it
/// describes (BL-178). Returns "Paused" for tier 0.
const char* speed_rate_label(int speed)
{
    switch (speed)
    {
    case 1:  return "0.25×";
    case 2:  return "0.5×";
    case 3:  return "1×";
    case 4:  return "4×";
    case 5:  return "16×";
    default: return "Paused";
    }
}

/// The speed tier's real-time cost of one economic quarter, as a compact human
/// string (BL-178). Computed from the sim-loop constants rather than authored, so
/// retuning `seconds_per_day_1x` or the multiplier curve updates the label:
/// one quarter is `econ_tick_days` in-game days, and one in-game day costs
/// `seconds_per_day_1x / multiplier` real seconds.
const char* speed_quarter_label(int speed)
{
    const double mult = sim_loop::speed_multiplier(speed);
    if (mult <= 0.0) return "—";

    const double secs = (sim_loop::econ_tick_days * sim_loop::seconds_per_day_1x) / mult;

    // Static buffers per tier: the caller treats the result as a stable literal,
    // and there are only five tiers, so this stays trivially safe.
    static char buf[sim_loop::max_speed + 1][24] = {};
    char* out = buf[speed < 0 ? 0 : (speed > sim_loop::max_speed ? 0 : speed)];
    if (secs < 60.0)
        std::snprintf(out, 24, "~%ds", static_cast<int>(std::lround(secs)));
    else
        std::snprintf(out, 24, "~%dm", static_cast<int>(std::lround(secs / 60.0)));
    return out;
}

} // namespace

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
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(m_renderer);
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

int app::run()
{
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
            step_economy();
            ++m_last_econ_tick;
        }

        render();
    }

    // Persist any free drag-resize captured this session (toggles/presets already
    // saved on change). Fullscreen: keep the last windowed size, don't overwrite it.
    save_settings();
    return 0;
}

void app::start_new_game()
{
    // Reset the sim clock so the campaign starts now, not at app construction —
    // constructing a fresh sim_loop rebases its internal timer to the current wall
    // clock. Speed comes from the Lua config loaded by init.lua in run().
    {
        sol::table cfg = m_lua.state()["config"];
        m_sim_loop = sim_loop();
        m_sim_loop.set_speed(cfg.get_or("default_speed", 2));
    }

    setup_world(m_pending_world_params);
    load_economy();

    // Pre-game warm start ([C3] pre-game profit): seed the balance history with the
    // opening capital, then run the real economy loop forward a notional operating
    // history before the first frame, so every corp opens onto non-empty pools,
    // moved balances, and live market figures rather than a cold zero state. Run
    // here (after load_economy) so it reuses the loaded registry rather than a
    // duplicated one; run_verify stays deterministically cold and does not warm up.
    // ~3 in-game years of quarterly econ ticks — long enough for a plausible history,
    // short enough not to diverge under the prototype's un-tuned economy.
    constexpr int pre_game_ticks = 12;
    {
        const auto pit = m_world.corporations.find(m_world.player_entity);
        ui::push_capped(m_balance_history,
            pit != m_world.corporations.end() ? pit->second.balance : 0.0f);
    }
    for (int t = 0; t < pre_game_ticks; ++t)
        step_economy();

    // Rebase the clock one last time. sim_loop measures from construction, so without
    // this the wall-clock cost of building the world and running the warm start would
    // land as elapsed in-game days on the first frame of play.
    {
        const int speed = m_sim_loop.speed();
        m_sim_loop = sim_loop();
        m_sim_loop.set_speed(speed);
        m_last_orbit_days = 0.0;
        m_last_survey_day = 0;
    }

    m_screen = app_screen::in_game;
}

void app::load_economy()
{
    // Load the Lua data layer, then build the registry from it (protected calls).
    m_lua.load("scripts/recipes.lua");
    m_lua.load("scripts/economy.lua");
    m_registry.load_from_lua(m_lua);

    // BL-323 S2b: mirror the reach budget onto ui_state so every placement surface
    // filters on the same number the authoritative gate uses. Done here, once, right
    // after the registry is loaded — a surface that filtered on a different budget
    // would offer tiles construct_building then refuses.
    m_ui.max_logistics_reach = m_registry.construction().max_logistics_reach;

    // BL-321 Era -1 works table. Loaded here rather than at generation time so a
    // malformed works.lua fails at startup, alongside every other data-layer
    // error, instead of midway through world generation. load_from_lua validates
    // the table and throws; see works_registry.cpp for what it enforces.
    m_lua.load("scripts/works.lua");
    m_works.load_from_lua(m_lua);

    // BL-087 mock tech/quest tree — display data for the F9 viewer only; no
    // simulation system reads it (the tech system is post-prototype).
    m_lua.load("scripts/tech_tree.lua");
    m_tech_tree.load_from_lua(m_lua);

    // Author processing recipes onto generated assets. The recipe id is a registry
    // index, unknown at generation time, so it is assigned here once the registry
    // exists. Every unconfigured processor defaults to the steel recipe.
    const uint16_t default_recipe = m_registry.recipe_id("steel");
    for (auto& [id, b] : m_world.buildings)
        if (b.type == building_type::processing_facility && b.recipe == no_recipe)
            b.recipe = default_recipe;

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
    dispatch_convoys(m_world, m_registry,
                     m_registry.logistics_cost(convoy_mode::land),
                     m_registry.logistics_cost(convoy_mode::space));
    advance_convoys(m_world);
    m_last_econ_report = run_economy_step(m_world, m_registry);
    auto flows = clear_markets(m_world, m_registry, m_last_econ_report, m_ui.sell_orders);
    apply_budget(m_world, m_registry, flows, m_last_econ_report.workforce_contention,
                 &m_last_econ_report.budgets,
                 &m_last_econ_report.buildings); // BL-343: law enforcement seam
    // BL-344: evaluate the tech gates once per economy tick, after the money loop
    // has moved balances (a `surplus` gate should read this quarter's balance, not
    // last quarter's). Monotonic and deterministic; a no-op once everything
    // earnable is earned.
    advance_tech_gates(m_world);

    // BL-262 first slice: cache this tick's standing profile for the Corporations panel
    // (transient runtime cache, not serialised — same treatment as m_last_econ_report).
    m_last_corp_standings = compute_corp_standings(m_world, flows);
    credit_arrived_convoys(m_world, static_cast<int>(m_sim_loop.day_tick()));

    // Surface this tick's background-corp agency actions (BL-079) as NATION-
    // voiced Public comms lines (BL-212). A rival's internals stay private —
    // the old per-corp, per-building text here broke DISCOVERY.md's already-
    // settled competitor-visibility rule ("rival internals are private... a
    // public AGGREGATE signal is the deliberate one"), the same shape as the
    // market layer already follows. Only the corp's HOME NATION ever posts,
    // phrased as its own first-person statement, and only the tick's single
    // heaviest event per nation — never a full account of everything that
    // happened underneath it.
    {
        auto severity_of = [](agency_event::kind k) -> int {
            switch (k)
            {
            case agency_event::kind::idled:            return 4;
            case agency_event::kind::demolished:       return 4;
            case agency_event::kind::built:             return 3;
            case agency_event::kind::recipe_switch:     return 2;
            case agency_event::kind::resumed:           return 2;
            case agency_event::kind::hired:             return 3;
            case agency_event::kind::road_placed:       return 1;
            case agency_event::kind::workforce_set:     return 1;
            case agency_event::kind::survey_dispatched: return 1;
            }
            return 0;
        };

        std::unordered_map<entity_id, const agency_event*> best_per_nation;
        for (const agency_event& ev : m_last_econ_report.agency_events)
        {
            const auto cit = m_world.corporations.find(ev.corp);
            if (cit == m_world.corporations.end() || cit->second.home_nation == null_entity)
                continue; // no home nation to speak on this corp's behalf
            const entity_id nation = cit->second.home_nation;
            const auto it = best_per_nation.find(nation);
            if (it == best_per_nation.end() || severity_of(ev.what) > severity_of(it->second->what))
                best_per_nation[nation] = &ev;
        }

        // Sorted ids so the post order is deterministic (unordered_map iteration
        // order is not) — mirrors the sorted corp_ids pattern in the counsel
        // block just below.
        std::vector<entity_id> nation_ids;
        nation_ids.reserve(best_per_nation.size());
        for (const auto& [nid, ev] : best_per_nation)
            nation_ids.push_back(nid);
        std::sort(nation_ids.begin(), nation_ids.end());

        for (const entity_id nid : nation_ids)
        {
            std::string text;
            switch (best_per_nation[nid]->what)
            {
            case agency_event::kind::recipe_switch:
                text = "We are adjusting output priorities in a domestic processing sector.";
                break;
            case agency_event::kind::idled:
                text = "We confirm an easing of activity in a strained sector.";
                break;
            case agency_event::kind::built:
                text = "We welcome new private investment within our borders.";
                break;
            case agency_event::kind::demolished:
                text = "We note the retirement of an aging facility.";
                break;
            case agency_event::kind::workforce_set:
                text = "We are adjusting domestic labour allocation.";
                break;
            case agency_event::kind::resumed:
                text = "We report resumed operations in a recovering sector.";
                break;
            case agency_event::kind::road_placed:
                text = "We announce new infrastructure investment.";
                break;
            case agency_event::kind::survey_dispatched:
                text = "We confirm new exploratory activity within our claims.";
                break;
            case agency_event::kind::hired:
                text = "We acknowledge the mustering of a private security formation on our soil.";
                break;
            }
            ui::chat_post(m_chat, static_cast<int>(m_sim_loop.day_tick()), nid, 0, std::move(text));
        }
    }

    // Persona counsel (BL-207 slice 1): every corp due at this strategic-eval
    // boundary gets its seated bench's read of its own blackboard, posted to a
    // per-corp Counsel channel (lazily created on first use). Counsel is
    // advisory only — it never touches the world, only the chat log.
    if (!m_persona_bench.empty())
    {
        const int tick = static_cast<int>(m_sim_loop.day_tick());
        std::vector<entity_id> corp_ids;
        corp_ids.reserve(m_world.corporations.size());
        for (const auto& [id, cc] : m_world.corporations)
            corp_ids.push_back(id);
        std::sort(corp_ids.begin(), corp_ids.end());

        for (const entity_id corp : corp_ids)
        {
            if (!corp_strategic_eval_due(m_world, corp, tick))
                continue;

            const corp_blackboard bb = export_corp_blackboard(m_world, corp, tick);
            int channel = -1;
            if (const auto it = m_counsel_channel.find(corp); it != m_counsel_channel.end())
                channel = it->second;
            else
            {
                ui::chat_channel ch;
                const auto cnit = m_world.corporations.find(corp);
                ch.name = std::string("Counsel: ")
                        + (cnit != m_world.corporations.end() ? cnit->second.name : "(unknown)");
                ch.members = { corp, m_world.player_entity };
                m_chat.channels.push_back(std::move(ch));
                channel = static_cast<int>(m_chat.channels.size()) - 1;
                m_counsel_channel[corp] = channel;
            }

            try
            {
                for (const persona::pack& p : m_persona_bench)
                {
                    if (p.is_verdict_bench())
                        continue; // slice 1: renders the hunting benches' reads, not aggregated verdicts yet
                    const std::vector<persona::opinion_record> ops = p.evaluate(bb);
                    if (ops.empty())
                        continue;
                    // Bound the chat to one line per pack per eval: the heaviest opinion.
                    const persona::opinion_record* top = &ops.front();
                    for (const auto& op : ops)
                        if (op.w > top->w) top = &op;
                    ui::chat_post(m_chat, tick, corp, channel,
                                 p.id() + ": " + p.phrase_for(*top));
                }
            }
            catch (const std::exception& e)
            {
                // BL-353: same policy as the load-time guard in load_economy — a
                // pack that loads clean but throws on live data (sol2 errors
                // surface as std::runtime_error) disables counsel rather than
                // killing the session mid-tick. One visible line in the counsel
                // channel; the detail goes to stderr like the load failure does.
                std::fprintf(stderr, "ProjectIo: persona counsel packs disabled: %s\n", e.what());
                ui::chat_post(m_chat, tick, corp, channel,
                              "The counsel bench has been dismissed: an advisory pack failed.");
                m_persona_bench.clear();
                break; // bench gone; nothing left to evaluate for the remaining corps
            }
        }
    }

    // Record player balance, income, and expenditure for the header sparkline and
    // economy panel graphs (BL-063).  All three are capped at plot_history_cap.
    {
        const auto cit = m_world.corporations.find(m_world.player_entity);
        ui::push_capped(m_balance_history,
            cit != m_world.corporations.end() ? cit->second.balance : 0.0f);

        const auto fit = flows.find(m_world.player_entity);
        ui::push_capped(m_income_history,
            fit != flows.end() ? fit->second.income : 0.0f);
        ui::push_capped(m_expenditure_history,
            fit != flows.end() ? fit->second.expenditure : 0.0f);
    }

    // Snapshot the player-building profit ranking for the Budget ledger's rank-change
    // column (BL-171): keep the last 5 (this tick + the 4 prior = ~a year back).
    {
        std::unordered_map<entity_id, int> ranks;
        const auto ranking = ui::rank_player_buildings_by_profit(m_world, m_registry, m_last_econ_report);
        for (int i = 0; i < static_cast<int>(ranking.size()); ++i)
            ranks[ranking[i].first] = i;
        m_building_rank_hist.push_back(std::move(ranks));
        if (m_building_rank_hist.size() > 5)
            m_building_rank_hist.pop_front();
    }

    // Record market price / supply / demand snapshots for the market ledger graphs.
    for (const auto& [mid, mc] : m_world.markets)
    {
        auto& mh = m_market_history[mid];
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (mc.base_price[r] <= 0.0f)
                continue; // resource not traded here; skip
            ui::push_capped(mh[r].price,  mc.price[r]);
            ui::push_capped(mh[r].supply, mc.supply[r]);
            ui::push_capped(mh[r].demand, mc.demand[r]);
        }
    }

    // Resource-deposit time series (BL-198): the aggregate (Σ remaining deposit per
    // body per resource) every tick; a tracked tile's own series lazily, from the
    // first drill-down into it. All share the sample-day axis (X), capped in
    // lockstep. One pass over the tiles accumulates the body totals and snapshots
    // any tracked tile — O(tiles·resources)/tick, the same order as the econ step.
    {
        // Drain the card's lazy-tracking request (a tile whose resource drill is
        // open) so the tile starts recording THIS tick, aligned to today's sample.
        if (m_ui.card_track_tile != null_entity)
        {
            m_tracked_tiles.insert(m_ui.card_track_tile);
            m_ui.card_track_tile = null_entity;
        }

        // Date the sample by its quarter index (econ ticks are quarterly). This
        // equals day_tick in live play but also advances when the verify harness
        // steps the economy without turning the sim clock, so the Year/Quarter axis
        // always progresses. See m_resource_sample_index.
        ui::push_capped(m_resource_hist_days,
                        m_resource_sample_index * static_cast<std::uint64_t>(sim_loop::econ_tick_days));
        ++m_resource_sample_index;

        std::unordered_map<entity_id, std::array<float, resource_count>> body_sum;
        for (const auto& [tid, tc] : m_world.tiles)
        {
            auto& acc = body_sum[tc.body];
            for (std::size_t r = 0; r < resource_count; ++r)
                acc[r] += tc.resource_deposit[r];

            if (m_tracked_tiles.count(tid))
            {
                auto& th = m_tile_resource_hist[tid];
                for (std::size_t r = 0; r < resource_count; ++r)
                    ui::push_capped(th[r], tc.resource_deposit[r]);
            }
        }
        for (const auto& [bid, acc] : body_sum)
        {
            auto& bh = m_body_resource_hist[bid];
            for (std::size_t r = 0; r < resource_count; ++r)
                ui::push_capped(bh[r], acc[r]);
        }
    }
}

void app::setup_world(world_params params)
{
    m_active_world_params = params;          // remember the descriptor the live world was built from

    // Capture the Planetology report alongside the world (BL-167). This is the one
    // call site, so filling it here gives both run() and run_verify() a report to
    // show without a second generation pass. It is presentation data only — the app
    // holds it, the `world` struct never sees it.
    // World-gen balance values (BL-236): loaded here, ahead of load_economy's
    // recipes/economy pass, since make_hard_coded_world runs before it. A missing
    // or malformed world_gen.lua throws (BL-110's protected-call-throws-on-error
    // rule), so a broken mod script fails loudly rather than silently reverting.
    world_gen_config gen_cfg;
    m_lua.load("scripts/world_gen.lua");
    gen_cfg.load_from_lua(m_lua);

    m_generation_report = generation_report{};
    m_world = make_hard_coded_world(params, &m_generation_report, gen_cfg);

    // Bridge PLANETOLOGY's per-body dated history + checkpoints into the world
    // history log's genesis + checkpoint chapters (BL-208). generation_report is
    // presentation-only and never otherwise reaches `world` — this is that bridge,
    // and it must run exactly once per generation (not idempotent).
    seed_genesis_history(m_world, m_generation_report);

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
                if (tit == m_world.tiles.end() || tit->second.body != start_body)
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
                if (tit != m_world.tiles.end() && tit->second.body == start_body)
                    m_ui.selected_entity = hq->second.tile;
            }
        }
    }

    // Open on plain terrain — no lens imposed at campaign start. A click only ever
    // updates the Selection element; it never re-skins the canvas, so the canvas
    // should likewise start unskinned and let the player pick a lens deliberately
    // from the strip. (Reverses BL-013's Corporation-default.) Single-select with a
    // null state — re-clicking the active lens clears back here. See LENSES.md.
    m_ui.overlay = overlay_mode::none;

    // Seed survey states (BL-067): home (and the star) open surveyed; every other
    // body opens hidden until the player dispatches a survey. Shared by run() and
    // run_verify() so both start from the same deterministic survey state.
    init_survey_states(m_world);
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
            running = false;
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
    ImGui::NewFrame();

    // Main menu and the staged generation screen — drawn instead of the canvases
    // when play has not started. Both share the Render/clear/capture tail below, so
    // either is capturable like any frame.
    if (m_screen != app_screen::in_game)
    {
        if (m_screen == app_screen::generating)
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
                ui::draw_body_surface_canvas(m_world, m_ui, m_registry, m_last_econ_report,
                                             m_generation_report, {0.0f, 0.0f}, disp, primary_input,
                                             {mm_origin.x, mm_origin.y + mm_h * 0.5f});
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

    // Time panel — top-right, same width as the minimap (BL-138 compact redesign,
    // proportions revised on Ben's 2026-07-10 review). REFLOWED (BL-313, Ben
    // 2026-08-06: "make sure the time bar is within the header bound ... left
    // 1/3 for the progress, right 2/3 for time controls") — was four stacked
    // full-width rows whose total height exceeded header_panel_height and
    // overflowed the header strip. Now two COLUMNS sharing one row-band: left
    // third is "the progress" (date line + quarter-progress bar), right two
    // thirds is "time controls" (the pause/speed-tier buttons + active-rate
    // label) — half the row count, so it fits. The panel takes input (the
    // speed buttons), so it is not flagged NoInputs.
    const float tick_w = ui::right_chrome_width(disp);
    const float col_gap  = ImGui::GetStyle().ItemSpacing.x;
    const float prog_w   = tick_w / 3.0f - col_gap * 0.5f;
    const float ctrl_w   = tick_w - prog_w - col_gap;
    const float time_line_h    = ImGui::GetTextLineHeightWithSpacing();
    // BL-178: the 10 px bar was easy to miss and carried no label. Tall enough to
    // seat a centred overlay ("58 d to Q2"), which is what actually makes the
    // next-resolution distance read.
    const float time_prog_h    = ImGui::GetTextLineHeight() + 4.0f;
    const float time_spacing   = ImGui::GetStyle().ItemSpacing.y;
    // BL-178: one always-visible line under the speed row naming the ACTIVE
    // tier's real rate. A tooltip cannot be seen without hovering (the same
    // critique BL-174 made of the nav rail), so the current speed's meaning is
    // stated on screen; the per-button tooltips carry the full ladder.
    const float time_rate_h    = ImGui::GetTextLineHeightWithSpacing();
    // BL-313: one frame height, not two — the two-column layout gives the
    // button row half the width it used to have, and a half-width row of six
    // buttons reads fine at the ordinary control height; the extra height the
    // old single-column layout spent here is exactly what put the panel over
    // header_panel_height.
    const float time_btn_h     = ImGui::GetFrameHeight();
    // Date line full-width, then one shared row where the left third (progress
    // bar) and right two-thirds (buttons + rate label) are measured
    // independently and the taller of the two sets the row's height.
    const float prog_col_h     = time_prog_h;
    const float ctrl_col_h     = time_btn_h + time_rate_h + time_spacing;
    const float time_content_h = time_line_h + std::max(prog_col_h, ctrl_col_h);
    const float time_h         = time_content_h + ImGui::GetStyle().WindowPadding.y * 2.0f;
    {
        // Head of the right chrome column (BL-216: shell_metrics owns the edge).
        const ui::shell_rect tp = ui::time_panel_rect(disp, time_h);
        ImGui::SetNextWindowPos({tp.x, tp.y});
        ImGui::SetNextWindowSize({tp.w, tp.h});
        constexpr ImGuiWindowFlags time_flags =
            ImGuiWindowFlags_NoTitleBar          |
            ImGuiWindowFlags_NoResize            |
            ImGuiWindowFlags_NoMove              |
            ImGuiWindowFlags_NoCollapse          |
            ImGuiWindowFlags_NoScrollbar         |
            ImGuiWindowFlags_NoNav               |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoSavedSettings;
        ImGui::Begin("##time_panel", nullptr, time_flags);

        const uint64_t day = m_sim_loop.day_tick();
        const ui::fmt::calendar_date date = ui::fmt::date_from_day(day);

        // --- Date/quarter line, full width — NOT confined to the left third:
        // "1960 Jan 1st [Q1]" runs wider than a 1/3 column at this panel's
        // width, and a fixed-width column has no wrap/clip of its own (a
        // BeginGroup is not a clip region), so an early version of this
        // reflow let the text bleed into the button row below it. The
        // 1/3-vs-2/3 split applies to the row underneath, where both sides
        // ARE sized elements (a progress bar, a button row) that actually
        // respect a width argument.
        ImGui::Text("%s %s %s [Q%d]", std::to_string(date.year).c_str(),
                    ui::fmt::month_abbrev(date.month),
                    ui::fmt::ordinal_day(date.day).c_str(), date.quarter);

        const ImVec2 row_start = ImGui::GetCursorPos();

        // --- LEFT THIRD: "the progress" — the quarter-progress bar. BL-178:
        // text-height, carries a centred overlay naming the distance to the
        // next resolution ("how close am I to the economy resolving").
        ImGui::BeginGroup();
        const float quarter_frac = ui::fmt::quarter_progress(day);
        char         prog_label[32];
        const int    days_left = static_cast<int>(
            std::lround((1.0f - quarter_frac) * static_cast<float>(sim_loop::econ_tick_days)));
        std::snprintf(prog_label, sizeof(prog_label), "%d d to Q%d",
                      days_left, (date.quarter % 4) + 1);
        ImGui::ProgressBar(quarter_frac, {prog_w, time_prog_h}, prog_label);
        ImGui::SetItemTooltip(
            "Quarter progress. The economy resolves on the quarter boundary:\n"
            "prices clear, production banks, and the budget settles.");
        ImGui::EndGroup();

        // --- RIGHT TWO-THIRDS: "time controls" — pause/speed-tier buttons +
        // the active-rate label, top-aligned with the progress bar (explicit
        // cursor, not SameLine — SameLine would anchor to the left group's
        // LAST item, not its top; harmless here since the bar is the only
        // item, but explicit stays correct if that ever changes).
        ImGui::SetCursorPos({row_start.x + prog_w + col_gap, row_start.y});
        ImGui::BeginGroup();

        // Speed controls: pause + speed-tier buttons, sized to the control
        // column's own width (NOT GetContentRegionAvail — a group is not a
        // clipping region, so that would still measure the whole window).
        // The active speed is highlighted. When running, the pause slot is a
        // blank button carrying a filled square glyph (drawn below); when
        // paused it flips to a play ">" so it reflects the toggle state.
        // Speed tiers use Roman numerals (I–V); the square avoids "||"
        // reading as II.
        {
            const char* labels[] = {m_sim_loop.paused() ? ">" : "##pause", "I", "II", "III", "IV", "V"};
            const int   speeds[] = { 0,    1,   2,   3,   4,   5 };
            const int   n        = 6;
            const float spacing  = ImGui::GetStyle().ItemSpacing.x;
            const float bw       = (ctrl_w - spacing * (n - 1)) / n;

            for (int i = 0; i < n; ++i)
            {
                const bool active = (m_sim_loop.speed() == speeds[i]);
                if (active)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                if (ImGui::Button(labels[i], {bw, time_btn_h}))
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
                // Pause glyph: a filled square centred on the blank "##pause" button —
                // clearer than "||", which read as the numeral II beside the tiers.
                if (speeds[i] == 0 && !m_sim_loop.paused())
                {
                    const ImVec2 mn = ImGui::GetItemRectMin();
                    const ImVec2 mx = ImGui::GetItemRectMax();
                    const ImVec2 c  = {(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f};
                    const float  h  = ImGui::GetFontSize() * 0.31f;
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        {c.x - h, c.y - h}, {c.x + h, c.y + h},
                        ImGui::GetColorU32(ImGuiCol_Text), 1.0f);
                }
                if (active)
                    ImGui::PopStyleColor();
                // BL-178: name each tier's REAL rate on hover. The multipliers were
                // already documented in the F1 hotkey sheet but never on the control
                // itself, so the ladder was unreadable where the player uses it.
                if (speeds[i] == 0)
                    ImGui::SetItemTooltip("%s (Space)", m_sim_loop.paused() ? "Resume" : "Pause");
                else
                    ImGui::SetItemTooltip("Speed %s — %s\n%s per quarter (%d)",
                                          labels[i],
                                          speed_rate_label(speeds[i]),
                                          speed_quarter_label(speeds[i]),
                                          speeds[i]);
                if (i + 1 < n)
                    ImGui::SameLine();
            }
        }

        // --- BL-178: the active tier's rate, always visible. Guaranteed-fit per
        // LAYOUT.md container 5 (the time panel is authored to fit): the string is
        // measured against the control column's own width — a group is not a
        // clipping region, so GetContentRegionAvail() would still measure the
        // whole window (the BL-313 bug this whole block was rewritten to avoid).
        {
            char rate[64];
            if (m_sim_loop.paused())
                std::snprintf(rate, sizeof(rate), "Paused");
            else
                std::snprintf(rate, sizeof(rate), "%s  ·  %s per quarter",
                              speed_rate_label(m_sim_loop.speed()),
                              speed_quarter_label(m_sim_loop.speed()));

            if (!m_sim_loop.paused() && ImGui::CalcTextSize(rate).x > ctrl_w)
                std::snprintf(rate, sizeof(rate), "%s",
                              speed_rate_label(m_sim_loop.speed()));
            ImGui::TextDisabled("%s", rate);
        }
        ImGui::EndGroup();

        ImGui::End();
    }

    // In-app system menu (BL-070): a corner gear button opening a small popup with
    // session controls — Pause/Resume (mirroring the Space hotkey via the shared
    // pause_toggle path) and Exit Game (inline "Really quit?" confirm, since there
    // is no save). Sits at the top-right, just left of the time column; Esc toggles
    // the same popup (handle_key_down). See docs/ui/MENU.md.
    {
        constexpr float gear = 26.0f;
        // One margin left of the right chrome column (BL-216).
        const ImVec2 gear_pos{ ui::right_chrome_left(disp) - margin - gear, margin };

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
        ImGui::SetNextWindowPos(gear_pos);
        ImGui::SetNextWindowSize({gear, gear});
        constexpr ImGuiWindowFlags gear_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings;
        ImGui::Begin("##sysmenu_gear", nullptr, gear_flags);
        {
            ImDrawList*  gdl = ImGui::GetWindowDrawList();
            const ImVec2 g0  = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##sysmenu_btn", {gear, gear}))
            {
                m_ui.show_system_menu = !m_ui.show_system_menu;
                if (!m_ui.show_system_menu)
                    m_ui.confirm_exit_pending = false;
            }
            const bool  hov = ImGui::IsItemHovered() || m_ui.show_system_menu;
            const ImU32 gc  = hov ? IM_COL32(232, 236, 246, 255) : IM_COL32(170, 178, 192, 255);
            const float cx  = g0.x + gear * 0.5f;
            const float cy  = g0.y + gear * 0.5f;
            const float hw  = gear * 0.28f;
            for (int i = -1; i <= 1; ++i)
                gdl->AddLine({cx - hw, cy + static_cast<float>(i) * 6.0f},
                             {cx + hw, cy + static_cast<float>(i) * 6.0f}, gc, 2.0f);
        }
        ImGui::End();
        ImGui::PopStyleVar();

        if (m_ui.show_system_menu)
        {
            ImGui::SetNextWindowPos({gear_pos.x + gear, gear_pos.y + gear + 4.0f},
                                    ImGuiCond_Always, {1.0f, 0.0f});
            constexpr ImGuiWindowFlags menu_flags =
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
            if (ImGui::Begin("##system_menu", nullptr, menu_flags))
            {
                constexpr float bw = 150.0f;
                if (ImGui::Button(m_sim_loop.paused() ? "Resume" : "Pause", {bw, 0.0f}))
                    dispatch_action(ui::canvas_command::pause_toggle);

                ImGui::Separator();

                if (!m_ui.confirm_exit_pending)
                {
                    if (ImGui::Button("Exit Game", {bw, 0.0f}))
                        m_ui.confirm_exit_pending = true;
                }
                else
                {
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(IM_COL32(232, 150, 90, 255)),
                                       "Really quit?");
                    if (ImGui::Button("Yes, quit", {bw, 0.0f}))
                        m_quit_requested = true;
                    if (ImGui::Button("Cancel", {bw, 0.0f}))
                        m_ui.confirm_exit_pending = false;
                }
            }
            ImGui::End();
        }
    }

    // Corporation profile — top-left corner, above the navigation pane.
    ui::draw_profile_panel(m_world);

    // Budget + resource header — spans the top between the identity tile and the
    // time column, clear of both. Starts at the shell column's right edge (x = W):
    // the identity tile / balance bar / Selection all clear the same permanent W (BL-122).
    {
        const float header_left  = ui::shell_column_width(disp.x);
        const float header_right = ui::right_chrome_left(disp) - margin;
        ui::draw_header_panel(m_world, m_balance_history, header_left, header_right);
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
        m_ui.card_resource_page = 0;    // ...and its resource accordion page
        m_prev_selection = m_ui.selected_entity;
    }

    ui::draw_tile_inspector(m_world, m_ui, m_generation_report, &m_ui.show_tile_ledger);
    {
        const ui::player_plot_history phist{m_balance_history, m_income_history, m_expenditure_history};
        ui::draw_economy_panel(m_world, m_registry, m_last_econ_report, phist, m_ui, &m_ui.show_economy_panel);
    }
    // Construction panel — an ordinary fold-out tab in the shell column (BL-122),
    // one of the mutually-exclusive column occupants (ledgers + Selection).
    ui::draw_construction_panel(m_world, m_registry, m_last_econ_report, m_ui, &m_ui.show_construction_panel);
    ui::draw_market_ledger(m_world, m_ui, m_market_history, m_ui.show_market_ledger);
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
    // Corporation dashboard (BL-248) — nav slot 1, MENU.md's long-named surface.
    // Replaces the all-corporations balance table that used to occupy this slot: a
    // comparison table is not "the player corporation at a glance", and the Economy
    // panel's Corps view already carries it.
    ui::draw_corporation_dashboard(m_world, m_registry, m_last_econ_report, m_ui,
                                   m_ui.show_corporation_panel);

    // The displaced table itself, restored (NR-012). BL-248 deleted it as a duplicate
    // of the Economy panel's Corps view; Ben did not intend a deletion, so it is back
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
        ui::draw_selection_band(m_world, m_registry, m_last_econ_report, rhist, m_ui,
                                band_origin, band_size);
    }

    // The shell fold-out column is ledgers-only (BL-195). The one contextual, per-
    // tile surface it still hosts is the tile construction ledger (BL-162), opened
    // from the card's "Construct Buildings" button; it draws only when no nav ledger
    // owns the column and the selection is a tile.
    if (!ui::any_panel_open(m_ui))
    {
        const entity_id sel         = m_ui.selected_entity;
        const bool      sel_is_tile = sel != null_entity && m_world.tiles.count(sel) > 0;

        if (m_ui.show_build_ledger && sel_is_tile)
            ui::draw_construction_ledger(m_world, m_registry, m_ui);
        else
            m_ui.show_build_ledger = false; // not a tile → no build ledger
    }

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
                // BL-162: the construction ledger must SURVIVE the build it initiated.
                // Reselecting the new building makes the selection non-tile, which forces
                // show_build_ledger false above — the ledger vanishes and the player has to
                // reselect the tile to place a second building on it (a rich tile stacks up
                // to 4 extraction sites). It also hid "Construction started.", which is drawn
                // inside the ledger. So reselect only when some OTHER surface (a placement-mode
                // canvas click) initiated the build.
                if (!m_ui.show_build_ledger)
                    m_ui.selected_entity = built;             // inspect the new building
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
            default:
                m_ui.construction.last_message = "Construction failed."; break;
        }
        m_ui.construction.pending_tile   = null_entity; // consume the request
        m_ui.construction.pending_recipe = no_recipe;   // and the recipe it carried
    }

    // Execute any road-placement request queued this frame by the build front door's Track/Road/
    // Highway affordances (BL-147 core, BL-172 tier). A road is a per-tile mutation (raises
    // road_level, lowers A* cost), not a building, so it routes through place_road.
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
                m_ui.construction.last_message = "Can't supply it."; break;
            default:
                m_ui.construction.last_message = "Hiring failed."; break;
        }
        m_ui.construction.pending_hire_tile = null_entity; // consume the request
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

    // Execute a law enact/repeal queued this frame by the Budget ledger (BL-343).
    // The flip takes effect on the NEXT economy tick, not retroactively on the one
    // already accounted — apply_budget resolves the enacted set once per tick.
    if (m_ui.construction.pending_law_toggle >= 0)
    {
        const std::size_t idx = static_cast<std::size_t>(m_ui.construction.pending_law_toggle);
        if (idx < m_world.laws.size())
        {
            law& l = m_world.laws[idx];
            l.enacted = !l.enacted;
            m_ui.construction.last_message =
                l.name + (l.enacted ? " enacted." : " repealed.");
        }
        m_ui.construction.pending_law_toggle = -1; // consume the request
    }

    // Execute any survey dispatch queued this frame by the Selection-panel Survey
    // button. Centralised here (like construction) so the const-world UI surfaces
    // only enqueue; the balance debit + schedule arming happen once against app's
    // mutable world. See survey_system.hpp / SOLAR.md § Survey.
    if (m_ui.pending_survey_dispatch != null_entity)
    {
        dispatch_survey(m_world, m_ui.pending_survey_dispatch);
        m_ui.pending_survey_dispatch = null_entity; // consume the request
    }

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
