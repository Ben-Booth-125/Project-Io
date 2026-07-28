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
#include "ui/balance_ledger.hpp"
#include "ui/corporation_panel.hpp"
#include "ui/economy_panel.hpp"
#include "ui/market_ledger.hpp"
#include "ui/chat_panel.hpp"
#include "ui/fonts.hpp"
#include "ui/format.hpp"
#include "ui/header_panel.hpp"
#include "ui/foldout_column.hpp" // shell_column_width — permanent left shell column (BL-122)
#include "ui/nav_pane.hpp"
#include "ui/overlay.hpp"
#include "ui/presentation.hpp"
#include "ui/profile_panel.hpp"
#include "ui/selection.hpp"
#include "ui/selection_card.hpp"
#include "ui/selection_panel.hpp"
#include "ui/solar_system_canvas.hpp"
#include "ui/tech_tree_panel.hpp"
#include "ui/tile_inspector.hpp"
#include "ui/view_nav.hpp"
#include "world/budget_system.hpp"
#include "world/construction.hpp"
#include "world/corp_ai.hpp"
#include "world/placement_rules.hpp"
#include "world/survey_system.hpp"
#include "world/hard_coded_world.hpp"
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

// Verify captures render at a FIXED small size, independent of the interactive
// window default above, so goldens stay renderer-/machine-independent and match the
// documented 1280x720 standard even as the interactive default grows. Growing
// window_w/window_h must NOT silently move the golden resolution — that desynced the
// whole golden set in 6a04ec9 (captures 1720x1080 vs goldens 1280x720). run_verify
// forces the window to this size before capturing. See DEVELOPMENT_PRACTICES
// § Display environment.
static constexpr int verify_w = 1280;
static constexpr int verify_h = 720;

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

/// Map a lens name (as used in the verify scripts) to its overlay_mode. Unknown
/// names fall back to overlay_mode::none.
overlay_mode overlay_from_name(const std::string& s)
{
    if (s == "supply")      return overlay_mode::supply;
    if (s == "market")      return overlay_mode::market;
    if (s == "country")     return overlay_mode::country;
    if (s == "faction")     return overlay_mode::country; // legacy alias (renamed BL-052)
    if (s == "corporation") return overlay_mode::corporation;
    if (s == "resource")    return overlay_mode::resource;
    if (s == "population")  return overlay_mode::population;
    if (s == "opportunity") return overlay_mode::opportunity;
    if (s == "production")  return overlay_mode::production;
    if (s == "scarcity")    return overlay_mode::scarcity;
    if (s == "industry")    return overlay_mode::industry;
    return overlay_mode::none;
}

/// Map a resource enum-slug to its `resource_type`, for the verify lens hooks. The
/// full prototype set so a check can name any good; unknown names fall back to iron_ore.
resource_type resource_from_name(const std::string& s)
{
    static const std::unordered_map<std::string, resource_type> m = {
        {"iron_ore", resource_type::iron_ore},
        {"coal", resource_type::coal},
        {"petroleum", resource_type::petroleum},
        {"silica", resource_type::silica},
        {"copper_ore", resource_type::copper_ore},
        {"rare_earth_ore", resource_type::rare_earth_ore},
        {"agricultural_produce", resource_type::agricultural_produce},
        {"water", resource_type::water},
        {"iron_nickel_ore", resource_type::iron_nickel_ore},
        {"platinum_group_metals", resource_type::platinum_group_metals},
        {"regolith", resource_type::regolith},
        {"stone", resource_type::stone},
        {"timber", resource_type::timber},
        {"sand", resource_type::sand},
        {"clay", resource_type::clay},
        {"peat", resource_type::peat},
        {"steel", resource_type::steel},
        {"refined_fuel", resource_type::refined_fuel},
        {"food_rations", resource_type::food_rations},
    };
    const auto it = m.find(s);
    return it != m.end() ? it->second : resource_type::iron_ore;
}

/// Find a body by case-insensitive name, or the home body for "home". Returns
/// null_entity when no match is found.
entity_id find_body(const world& w, const std::string& name)
{
    if (name == "home")
        return w.home_body;
    auto lower = [](std::string v) {
        for (char& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return v;
    };
    const std::string target = lower(name);
    for (const auto& [id, body] : w.bodies)
        if (lower(body.name) == target)
            return id;
    return null_entity;
}

} // namespace

app::app()
{
    if (!SDL_Init(SDL_INIT_VIDEO))
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());

    m_window   = SDL_CreateWindow("Project Io", window_w, window_h, SDL_WINDOW_RESIZABLE);

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
}

namespace {
constexpr std::array<float, 3> k_ui_scale_px = {16.0f, 20.0f, 24.0f}; ///< BL-063 steps: 1.0 / 1.25 / 1.5 of the 16px base.
} // namespace

void app::apply_ui_scale()
{
    const int step = std::clamp(m_settings.ui_scale_step, 0, static_cast<int>(k_ui_scale_px.size()) - 1);
    ui::reload_ui_font(k_ui_scale_px[static_cast<std::size_t>(step)]);
    ImGui_ImplSDLRenderer3_DestroyFontsTexture();
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

void app::open_new_world_wizard()
{
    // Nothing is generated yet — the wizard runs on a throwaway preview and only
    // commits when the player reaches its last round and presses "Begin".
    m_wiz_round = 0;
    m_wiz_dirty = true;
    m_screen    = app_screen::generating;
}

void app::refresh_wizard_preview()
{
    // Preferences are not parameters, so they are resolved against the seed FIRST.
    // The resolution rejects and rerolls internally until the homeworld clears the
    // strict Earth-like floor, which is why the preview is always a world the
    // campaign could actually start on — and why it also reports what that cost
    // (resolved_world::attempts), which the round surfaces rather than hides.
    m_wiz_resolved = resolve_preferences(m_pending_world_params.preferences,
                                         m_pending_world_params.seed);
    preview_system(m_wiz_resolved.params, m_wiz_resolved.home_orbit_au,
                   m_pending_world_params.seed, m_wiz_preview);

    // The Spend chart needs the endowment BEFORE the industrial drawdown. Drawdown
    // is the chain's last act and consumes no randomness, so a second run with the
    // dial at zero is the same world minus its industrial history — exactly the
    // "before" reference the hollow bars want.
    planetology_params undrawn = m_wiz_resolved.params;
    undrawn.drawdown = 0.0f;
    preview_system(undrawn, m_wiz_resolved.home_orbit_au,
                   m_pending_world_params.seed, m_wiz_undrawn);
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
                 &m_last_econ_report.budgets);
    credit_arrived_convoys(m_world, static_cast<int>(m_sim_loop.day_tick()));

    // Surface this tick's background-corp agency actions (BL-079) as Public
    // comms lines (BL-205) — the chat is the AI-observability surface. Message
    // text is a pure rendering of deterministic report events.
    for (const agency_event& ev : m_last_econ_report.agency_events)
    {
        const auto bit = m_world.buildings.find(ev.building);
        if (bit == m_world.buildings.end())
            continue;
        const building_component& b = bit->second;
        std::string where;
        if (const auto tit = m_world.tiles.find(b.tile); tit != m_world.tiles.end())
            if (const auto boit = m_world.bodies.find(tit->second.body); boit != m_world.bodies.end())
                where = std::string(" on ") + boit->second.name;

        std::string text;
        switch (ev.what)
        {
        case agency_event::kind::recipe_switch:
        {
            const recipe* rc = m_registry.get_recipe(ev.new_recipe);
            text = std::string("Switched ") + ui::building_type_name(b.type) + where +
                   " to the " + (rc ? rc->name.c_str() : "?") + " recipe - output price floored.";
            break;
        }
        case agency_event::kind::idled:
            text = std::string("Idled ") + ui::building_type_name(b.type) + where +
                   " - sustained losses.";
            break;
        case agency_event::kind::built:
            text = std::string("Built a new ") + ui::building_type_name(b.type) + where + ".";
            break;
        case agency_event::kind::demolished:
            text = std::string("Demolished ") + ui::building_type_name(b.type) + where + ".";
            break;
        case agency_event::kind::workforce_set:
            text = std::string("Retargeted ") + ui::building_type_name(b.type) + where +
                   "'s workforce to " + std::to_string(ev.value) + ".";
            break;
        case agency_event::kind::resumed:
            text = std::string("Resumed ") + ui::building_type_name(b.type) + where + ".";
            break;
        case agency_event::kind::road_placed:
            text = "Laid a tier-" + std::to_string(ev.value) + " road.";
            break;
        case agency_event::kind::survey_dispatched:
            text = "Dispatched a survey.";
            break;
        }
        ui::chat_post(m_chat, static_cast<int>(m_sim_loop.day_tick()), ev.corp, 0,
                      std::move(text));
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
    m_generation_report = generation_report{};
    m_world = make_hard_coded_world(params, &m_generation_report);

    // Open the comms log with a deterministic epoch line (BL-205): the Public
    // channel exists from campaign start, so the panel is never an empty shell.
    m_chat = ui::chat_state{};
    ui::chat_post(m_chat, 0, null_entity, 0,
                  std::to_string(m_world.corporations.size()) +
                      " corporations on the public channel.");

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

int app::run_verify(const std::string& script_path, bool bless)
{
    // Deterministic, non-interactive setup: fixed window (resized to verify_w/
    // verify_h below), seeded world, sim left paused so orbits and ticks never
    // advance between captures. The script drives view/overlay state directly.
    setup_world();
    load_economy();
    m_sim_loop.set_speed(0);

    // The harness renders the live world, not the main menu — flip past the launch
    // screen. A menu-verification script re-enters the menu with verify.show_menu.
    m_screen = app_screen::in_game;

    // Force the fixed verify capture size (verify_w × verify_h), decoupled from the
    // interactive window default (window_w × window_h) which is now larger. This
    // keeps captures + committed goldens at the 1280×720 standard regardless of the
    // interactive default. SyncWindow blocks until the resize is applied so the very
    // first capture already renders at the fixed size.
    SDL_SetWindowSize(m_window, verify_w, verify_h);
    SDL_SyncWindow(m_window);

    // Golden-image diffing: goldens live in a "golden" directory beside the verify
    // script, so running against the source script path (the skill's iteration
    // mode) reads/writes the committed source tree, not a stale build copy.
    m_verify_bless = bless;
    m_golden_dir   = (std::filesystem::path{script_path}.parent_path() / "golden").string();

    // Expose the `verify` API. Each function writes ui_state directly — the
    // "direct state manipulation" driver — so captures are reproducible without
    // any synthetic input. capture() renders one frame and saves it as a PNG.
    sol::state& L = m_lua.state();
    sol::table  v = L.create_named_table("verify");

    v.set_function("goto_surface", [this](const std::string& name) {
        const entity_id b = find_body(m_world, name);
        if (b != null_entity)
            ui::focus_on_surface(m_world, m_ui, b);
    });
    // The Selection panel's 'go to' path: routes a named body through the polymorphic
    // focus_on_entity dispatch (not focus_on_surface directly), so a capture proves
    // where 'go to' actually lands for that body — the durable check for the
    // "go to only works for Kepler" symptom.
    v.set_function("go_to", [this](const std::string& name) {
        const entity_id b = find_body(m_world, name);
        if (b != null_entity)
            ui::focus_on_entity(m_world, m_ui, b);
    });
    v.set_function("set_overlay", [this](const std::string& name) {
        m_ui.overlay = overlay_from_name(name);
    });
    // Drive the Resource/Market/Scarcity lens-local selector headlessly so a golden
    // can pick the displayed good.
    v.set_function("set_lens_resource", [this](const std::string& name) {
        m_ui.lens_resource = resource_from_name(name);
    });
    // Obsolete since BL-019 (the Resource lens is always single-resource); retained
    // as a no-op so existing verify scripts that call it keep loading.
    v.set_function("set_resource_mode", [](bool) {});
    v.set_function("set_zoom", [this](float z) { m_ui.planetary_zoom = z; });
    v.set_function("set_pan",  [this](float x, float y) {
        m_ui.planetary_pan_x = x;
        m_ui.planetary_pan_y = y;
    });
    v.set_function("add_pan",  [this](float dx, float dy) {
        m_ui.planetary_pan_x += dx;
        m_ui.planetary_pan_y += dy;
    });
    v.set_function("capture", [this](const std::string& name) { capture_frame(name); });

    // BL-061: scriptable cursor override — sets the app-owned mouse source so a
    // verify script can place the cursor at an exact screen position and trigger
    // hover highlights deterministically, without the OS cursor leaking into goldens.
    v.set_function("mouse", [this](float x, float y) {
        m_ui.mouse = {x, y, true};
    });
    // Convenience: position the cursor over a specific tile (col, row) — the
    // Planetary canvas computes the screen centre of that tile on its next draw,
    // so this is a best-effort "close enough to trigger hover" helper.
    v.set_function("hover_tile", [this](int col, int row) {
        // Request a centre-on-tile so the canvas knows the tile's screen position,
        // then place the mouse at approximately the grid centre. The canvas
        // corrects hover on the following frame once it knows the real position.
        m_ui.planetary_center_pending = true;
        m_ui.planetary_center_col     = col;
        m_ui.planetary_center_row     = row;
        // Approximate screen position: place mouse near the canvas centre so the
        // canvas' per-tile hit-test resolves it after the centre pan settles.
        m_ui.mouse = {ImGui::GetIO().DisplaySize.x * 0.5f,
                      ImGui::GetIO().DisplaySize.y * 0.5f, true};
    });

    // Drive the economy headlessly: run N economy ticks (production → market →
    // budget) and open the economy panel so a capture shows live, populated data.
    // The durable verification method for the Layer 3 economy panel's visual rows.
    v.set_function("econ_step", [this](sol::optional<int> n) {
        const int steps = n.value_or(1);
        for (int i = 0; i < steps; ++i)
            step_economy();
        m_ui.show_economy_panel = true;
    });

    // Open/close a ledger panel by name — lets a lens check run econ ticks (which
    // open the economy panel) and then clear it so the panel does not obscure the
    // canvas capture. Unknown names are ignored.
    // Re-enter (or leave) the main menu so a verify script can capture the launch
    // screen — the harness otherwise starts past it, in-game.
    v.set_function("show_menu", [this](bool on) {
        m_screen = on ? app_screen::menu : app_screen::in_game;
    });

    // Enter (or leave) the BL-167 New World wizard so a script can capture it. It
    // opens on the LAST round, fully populated: the wizard is a walk, and a frame
    // taken part-way along it depends on which preferences a script happened to have
    // set. The final round is the only one that closes the whole chain, so it is the
    // one that diffs stably against a golden.
    // Rebuild the world on a specific seed.
    //
    // Some checks need a world with a particular PROPERTY, not just any world —
    // recipe_workforce.lua needs the player to own a processing facility, which
    // corporation generation only produces on some seeds. Pinning the seed makes
    // such a check deterministic and honest, rather than silently skipping when
    // the default world happens not to oblige.
    //
    // Use sparingly: a check that pins a seed is testing one world, so anything
    // that should hold for EVERY world belongs in a headless harness instead.
    v.set_function("new_world", [this](unsigned seed) {
        world_params p = m_active_world_params;
        p.seed = static_cast<uint32_t>(seed);
        setup_world(p);
        load_economy();
    });

    v.set_function("show_generation", [this](bool on) {
        m_screen    = on ? app_screen::generating : app_screen::in_game;
        m_wiz_round = wizard_round_count - 1;
        m_wiz_dirty = true;
    });

    // Park the wizard on a specific ROUND (0-2) so a visual check can capture each
    // one. Clamped by draw_generation_screen, so an out-of-range index is harmless —
    // the name is kept for the scripts that already call it. Every round is a stable
    // capture: the wizard is driven by the preferences and the seed, not by
    // wall-clock, so there is no animation to race.
    v.set_function("generation_stage", [this](int round) {
        m_screen    = app_screen::generating;
        m_wiz_round = round;
        m_wiz_dirty = true;
    });

    v.set_function("show_panel", [this](const std::string& name, bool open) {
        if (name == "economy")           m_ui.show_economy_panel = open;
        else if (name == "construction") m_ui.show_construction_panel = open;
        else if (name == "tile")         m_ui.show_tile_ledger = open;
        else if (name == "market")       m_ui.show_market_ledger = open;
        else if (name == "balance")      m_ui.show_balance_ledger = open;
        else if (name == "corporation")  m_ui.show_corporation_panel = open;
        else if (name == "build")        m_ui.show_build_ledger = open; // tile construction ledger (BL-162)
    });

    // Open the Layer 4 construction / building-management panel so a capture shows
    // the building surface. The scaffold panel takes no economy state to populate.
    v.set_function("show_construction", [this]() {
        m_ui.show_construction_panel = true;
    });

    // Arm placement mode for a building type (and optional extraction target) so a
    // capture shows the Planetary ghost marker. Names mirror the UI vocabulary;
    // unknown names leave the mode unchanged. Non-mutating — this is the v0.0.5
    // placement scaffold (no construction is committed).
    v.set_function("place_mode",
        [this](const std::string& type, sol::optional<std::string> target) {
            building_type bt = building_type::none;
            if (type == "extraction")      bt = building_type::extraction_site;
            else if (type == "processing") bt = building_type::processing_facility;
            else if (type == "port")       bt = building_type::port;
            if (bt == building_type::none)
                return;
            m_ui.construction.active = true;
            m_ui.construction.type   = bt;
            if (target)
            {
                const std::string& t = *target;
                if (t == "iron_ore")             m_ui.construction.target = resource_type::iron_ore;
                else if (t == "petroleum")       m_ui.construction.target = resource_type::petroleum;
                else if (t == "water")           m_ui.construction.target = resource_type::water;
                else if (t == "agricultural_produce") m_ui.construction.target = resource_type::agricultural_produce;
            }
        });

    // Commit a real placement (verify harness): place the currently-armed building
    // type/target on the first valid, unoccupied tile of the active body via the
    // SAME construct_building path the interactive click uses — so a script can prove
    // a fresh player can actually BUILD, not merely arm placement. This is the
    // coverage the build flow lacked (build_walkthrough armed but never committed;
    // construction_harness uses a hand-built registry without the real material
    // costs), which let the BL-044 construction deadlock ship invisibly. Returns the
    // construction_result name ("placed" on success).
    v.set_function("build_first_valid", [this]() -> std::string {
        const building_type bt  = m_ui.construction.type;
        const resource_type tgt = m_ui.construction.target;
        std::unordered_set<entity_id> occupied;
        for (const auto& [bid, bc] : m_world.buildings)
            occupied.insert(bc.tile);
        for (const auto& [tid, tc] : m_world.tiles)
        {
            if (tc.body != m_ui.active_body || occupied.count(tid))
                continue;
            if (!placement_rules::can_place_in_world(m_world, tid, bt, tgt))
                continue;
            entity_id built = null_entity;
            const construction_result r = construct_building(
                m_world, m_registry, m_world.player_entity, tid, bt, tgt, built);
            const char* name =
                r == construction_result::placed                 ? "placed" :
                r == construction_result::invalid_tile           ? "invalid_tile" :
                r == construction_result::insufficient_funds     ? "insufficient_funds" :
                r == construction_result::no_corp                ? "no_corp" :
                r == construction_result::no_tile                ? "no_tile" :
                r == construction_result::slot_occupied          ? "slot_occupied" :
                r == construction_result::insufficient_materials ? "insufficient_materials" : "failed";
            if (r == construction_result::placed)
            {
                m_ui.selected_entity      = built;
                m_ui.selection_hidden_for = null_entity;
            }
            SDL_Log("verify.build_first_valid: %s at tile (%d,%d)", name, tc.grid_x, tc.grid_y);
            return std::string(name);
        }
        SDL_Log("verify.build_first_valid: no valid unoccupied tile on active body");
        return std::string("no_valid_tile");
    });

    // Build the armed type on a SPECIFIC tile (col, row) of the active body, through
    // the same construct_building path as build_first_valid, and select the result.
    //
    // build_first_valid takes the first valid tile anywhere on the body, which is
    // fine for proving placement works but not for staging a *running* building:
    // construction is material-gated, so a site on a tile whose local market is dry
    // stays paused forever however many ticks the script runs. Choosing the tile lets
    // a check build where the market can actually supply it.
    v.set_function("build_at", [this](int col, int row) -> std::string {
        for (const auto& [tid, tc] : m_world.tiles)
        {
            if (tc.body != m_ui.active_body || tc.grid_x != col || tc.grid_y != row)
                continue;
            entity_id built = null_entity;
            const construction_result r = construct_building(
                m_world, m_registry, m_world.player_entity, tid,
                m_ui.construction.type, m_ui.construction.target, built);
            const char* name =
                r == construction_result::placed                 ? "placed" :
                r == construction_result::invalid_tile           ? "invalid_tile" :
                r == construction_result::insufficient_funds     ? "insufficient_funds" :
                r == construction_result::no_corp                ? "no_corp" :
                r == construction_result::no_tile                ? "no_tile" :
                r == construction_result::slot_occupied          ? "slot_occupied" :
                r == construction_result::insufficient_materials ? "insufficient_materials" : "failed";
            if (r == construction_result::placed)
            {
                m_ui.selected_entity      = built;
                m_ui.selection_hidden_for = null_entity;
            }
            SDL_Log("verify.build_at: %s at tile (%d,%d)", name, col, row);
            return std::string(name);
        }
        SDL_Log("verify.build_at: no tile (%d,%d) on the active body", col, row);
        return std::string("no_tile");
    });

    // Assertion primitive (verify harness): on a false condition, bump the failure
    // count (→ non-zero exit, like a golden miss) and log; so a logic check can
    // PASS/FAIL a script the way a golden diff does.
    v.set_function("expect", [this](bool ok, sol::optional<std::string> msg) {
        if (!ok) ++m_verify_failures;
        SDL_Log("verify.expect %s: %s", ok ? "PASS" : "FAIL",
                msg ? msg->c_str() : "");
    });

    // === BL-113 interactive-flow acceptance primitives ======================
    // Three more real-commit-path mutators mirroring build_first_valid: each drives
    // the SAME state a UI control writes, so an acceptance script proves the player
    // action reaches the sim, then asserts the effect after ticking. See the
    // scripts/verify/{recipe_workforce,sell_order,survey_dispatch}.lua scripts.

    // --- US-007: recipe / workforce change ----------------------------------
    // Locate the player's first processing_facility (the only type with a recipe
    // choice) and report {found, recipes} so a script can decide what to set. Returns
    // recipes = the number of selectable recipes for that building's type.
    v.set_function("first_processing_building", [this]() {
        sol::state& s = m_lua.state();
        sol::table  out = s.create_table();
        const auto cit = m_world.corporations.find(m_world.player_entity);
        if (cit != m_world.corporations.end())
        {
            for (const entity_id bld_id : cit->second.assets)
            {
                const auto bit = m_world.buildings.find(bld_id);
                if (bit == m_world.buildings.end()) continue;
                if (bit->second.type != building_type::processing_facility) continue;
                out["found"]   = true;
                out["tile"]    = static_cast<unsigned>(bit->second.tile);
                out["recipes"] = m_registry.recipe_count(building_type::processing_facility);
                out["index"]   = bit->second.active_recipe_index;
                out["workforce"] = bit->second.workforce_target;
                return out;
            }
        }
        out["found"] = false;
        return out;
    });

    // Set a building's active recipe by index via the SAME writes the construction
    // panel's recipe combo performs (b.active_recipe_index + b.recipe = registry id
    // of the chosen recipe). Keys on tile like the panel does (buildings key on their
    // own id; the panel scans for tile == selection). Returns the recipe name applied,
    // or "" if the tile has no building. Non-economic — the next econ_step honours it.
    v.set_function("set_building_recipe", [this](unsigned tile_u, int index) -> std::string {
        const entity_id tile = static_cast<entity_id>(tile_u);
        for (auto& [bid, b] : m_world.buildings)
        {
            if (b.tile != tile) continue;
            const int n = m_registry.recipe_count(b.type);
            if (n <= 0) return std::string("");
            const int i = std::clamp(index, 0, n - 1);
            const recipe& r = m_registry.recipe_at(b.type, i);
            b.active_recipe_index = i;
            b.recipe              = m_registry.recipe_id(r.name);
            SDL_Log("verify.set_building_recipe: tile=%u index=%d recipe=%s",
                    tile_u, i, r.name.c_str());
            return r.name;
        }
        return std::string("");
    });

    // Set a building's workforce target (0–200 %) via the SAME clamp+write the
    // construction panel's workforce slider performs. Returns the applied value.
    v.set_function("set_building_workforce", [this](unsigned tile_u, int pct) -> int {
        const entity_id tile = static_cast<entity_id>(tile_u);
        for (auto& [bid, b] : m_world.buildings)
        {
            if (b.tile != tile) continue;
            b.workforce_target = std::clamp(pct, 0, 200);
            SDL_Log("verify.set_building_workforce: tile=%u target=%d",
                    tile_u, b.workforce_target);
            return b.workforce_target;
        }
        return -1;
    });

    // Read the last econ report's output for the building on `tile` — the units it
    // credited to the pool this tick. Lets a recipe/workforce script assert the
    // change took effect after econ_step. Returns -1 if no report row for that tile.
    v.set_function("building_output", [this](unsigned tile_u) -> double {
        const entity_id tile = static_cast<entity_id>(tile_u);
        for (const building_report& br : m_last_econ_report.buildings)
        {
            const auto bit = m_world.buildings.find(br.building);
            if (bit != m_world.buildings.end() && bit->second.tile == tile)
                return static_cast<double>(br.output_quantity);
        }
        return -1.0;
    });

    // --- US-008: standing sell-order placement ------------------------------
    // Place a standing sell order through the SAME path the construction panel's
    // "Add sell order" button uses: push a sell_order onto m_ui.sell_orders, the
    // vector step_economy() feeds to clear_markets. Body resolves to the player's
    // home body. Returns the resulting sell_orders count so a script can assert the
    // order registered. Realises the placement half of US-008.
    v.set_function("place_sell_order",
        [this](const std::string& res, double qty, double floor) -> int {
            sell_order o;
            o.corp        = m_world.player_entity;
            o.body        = m_world.home_body;
            o.resource    = resource_from_name(res);
            o.quantity    = static_cast<float>(qty);
            o.floor_price = static_cast<float>(floor);
            m_ui.sell_orders.push_back(o);
            SDL_Log("verify.place_sell_order: %s x%.0f >= %.1f (n=%zu)",
                    res.c_str(), qty, floor, m_ui.sell_orders.size());
            return static_cast<int>(m_ui.sell_orders.size());
        });

    // Read the resolved market price of `res` on the player's home body — lets a
    // sell-order script assert the floor was honoured (a placed floor above the
    // clearing price prevents a below-floor dump; the pool retains stock rather than
    // selling under the floor). Returns -1 if the home body has no market.
    v.set_function("home_market_price", [this](const std::string& res) -> double {
        const resource_type rt = resource_from_name(res);
        for (const auto& [mid, mc] : m_world.markets)
            if (mc.body == m_world.home_body)
                return static_cast<double>(mc.price[static_cast<std::size_t>(rt)]);
        return -1.0;
    });

    // Read the player's home-body pool quantity of `res` — the stock a sell order
    // draws from. A floored order that cannot clear leaves stock in the pool; a
    // script asserts the floor was honoured by comparing pool before/after a tick.
    v.set_function("home_pool", [this](const std::string& res) -> double {
        const resource_type rt = resource_from_name(res);
        const auto it = m_world.corp_body_pools.find(
            std::make_pair(m_world.player_entity, m_world.home_body));
        if (it == m_world.corp_body_pools.end()) return 0.0;
        return static_cast<double>(it->second.quantities[static_cast<std::size_t>(rt)]);
    });

    // --- US-011: survey dispatch --------------------------------------------
    // Dispatch a survey of a named body through the SAME dispatch_survey path
    // app::render runs when it consumes ui.pending_survey_dispatch (the Selection
    // panel's Survey button only enqueues; the real debit + schedule live in
    // dispatch_survey). Returns the survey_dispatch_result name ("success" debits
    // the cost upfront and arms the schedule). Realises the dispatch half of US-011.
    v.set_function("dispatch_survey_of", [this](const std::string& name) -> std::string {
        const entity_id body = find_body(m_world, name);
        if (body == null_entity) return std::string("invalid");
        const survey_dispatch_result r = dispatch_survey(m_world, body);
        const char* rn =
            r == survey_dispatch_result::success             ? "success" :
            r == survey_dispatch_result::insufficient_funds  ? "insufficient_funds" :
            r == survey_dispatch_result::already_surveyed     ? "already_surveyed" :
            r == survey_dispatch_result::in_progress          ? "in_progress" : "invalid";
        SDL_Log("verify.dispatch_survey_of: %s -> %s", name.c_str(), rn);
        return std::string(rn);
    });

    // Advance every in-progress survey by `days` via the real advance_surveys path
    // (the same call app runs on day boundaries). Lets a script tick the survey clock
    // deterministically and then assert the reveal advanced.
    v.set_function("advance_survey_days", [this](int days) {
        advance_surveys(m_world, days);
    });

    // Survey read accessors: the up-front cost preview, the player balance, and the
    // count of regions revealed so far on a named body — so a survey script can assert
    // BOTH that credits were debited (balance drop == cost) AND that reveal advanced
    // (regions_done climbs after ticking). Returns -1 on a missing body/market.
    v.set_function("survey_cost_of", [this](const std::string& name) -> double {
        const entity_id body = find_body(m_world, name);
        if (body == null_entity) return -1.0;
        return static_cast<double>(survey_cost(m_world, body));
    });
    v.set_function("player_balance", [this]() -> double {
        const auto it = m_world.corporations.find(m_world.player_entity);
        return it == m_world.corporations.end() ? 0.0
                                                 : static_cast<double>(it->second.balance);
    });
    v.set_function("survey_regions_done", [this](const std::string& name) -> int {
        const entity_id body = find_body(m_world, name);
        if (body == null_entity) return -1;
        const auto bit = m_world.bodies.find(body);
        if (bit == m_world.bodies.end()) return -1;
        return bit->second.survey.regions_done;
    });

    // Discrete navigation by the shared command vocabulary — the same dispatch the
    // keyboard uses (see canvas_command.hpp), so a script reads as a key sequence.
    // Unknown command names are ignored.
    v.set_function("command", [this](const std::string& name) {
        if (auto cmd = ui::canvas_command_from_name(name))
            ui::apply_canvas_command(m_world, m_ui, *cmd);
    });

    // Centre a Planetary tile without replicating the canvas transform in Lua:
    // record a pending centre request that body_surface_canvas consumes on its
    // next draw, where the exact grid metrics are known. An optional zoom sets the
    // framing first so the request is computed against the intended scale.
    v.set_function("center_tile",
        [this](int col, int row, sol::optional<float> zoom) {
            if (zoom)
                m_ui.planetary_zoom = *zoom;
            m_ui.planetary_center_col     = col;
            m_ui.planetary_center_row     = row;
            m_ui.planetary_center_pending = true;
        });

    // Selection drivers (verify harness): set ui.selected_entity so a capture shows
    // the Selection panel. select_tile picks the tile at (col,row) on the active
    // surface body; select_building picks a building occupying that tile. Both are
    // non-mutating — they only move the selection, exactly as a canvas click would.
    v.set_function("select_tile", [this](int col, int row) {
        for (const auto& [tid, tc] : m_world.tiles)
            if (tc.body == m_ui.active_body && tc.grid_x == col && tc.grid_y == row)
            { m_ui.selected_entity = tid; m_ui.selection_hidden_for = null_entity; break; }
    });
    v.set_function("dismiss_selection", [this]() {
        // Test hook for the sticky card's dismiss path (BL-194) — the x button
        // and Esc both hide the current selection this same way; no key-event
        // injection exists in the headless harness, so this is the equivalent.
        m_ui.selection_hidden_for = m_ui.selected_entity;
    });
    v.set_function("card_drill", [this]() {
        // Drive the sticky card's resource drill-down (BL-196) for the currently
        // selected tile — the equivalent of clicking a resource graph, since no
        // click injection exists headless. Drills the tile's first deposited
        // resource and requests lazy tracking of it (BL-198). Returns the resource
        // index drilled, or -1 if the selection is not a deposit-bearing tile.
        const entity_id sel = m_ui.selected_entity;
        const auto tit = m_world.tiles.find(sel);
        if (tit == m_world.tiles.end())
            return -1;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (tit->second.resource_deposit[r] > 0.0f)
            {
                m_ui.card_stack.push_back({sel, static_cast<int>(r)});
                m_ui.card_track_tile = sel;
                // Acknowledge the selection so render()'s "new selection resets the
                // drill" does not wipe the stack we just pushed. In live play the
                // drill click happens frames after the select, so prev is already
                // current; the harness sets both in one gap, so sync it here.
                m_prev_selection = sel;
                return static_cast<int>(r);
            }
        return -1;
    });
    v.set_function("select_building", [this](int col, int row) {
        for (const auto& [bid, bc] : m_world.buildings)
        {
            const auto tit = m_world.tiles.find(bc.tile);
            if (tit == m_world.tiles.end()) continue;
            if (tit->second.body == m_ui.active_body &&
                tit->second.grid_x == col && tit->second.grid_y == row)
            { m_ui.selected_entity = bid; m_ui.selection_hidden_for = null_entity; break; }
        }
    });
    // Switch the construction panel's sub-view (0 = Construction/queue, 1 = Buildings/
    // management detail) so a script can capture the building-management screen, which
    // otherwise opens on the queue view. UI state only.
    v.set_function("construction_view", [this](int view) {
        m_ui.construction.panel_view = view;
        // Acknowledge the current selection so the new-selection panel-close (render()
        // § "new selection takes the column") does not stomp the panel we're staging
        // for capture. Harness-only convenience; the interactive path never needs it.
        m_prev_selection          = m_ui.selected_entity;
        m_ui.selection_hidden_for = m_ui.selected_entity;
    });

    // select_body picks a body by name, exactly as a single-click on the Solar /
    // Circumplanetary canvas would (sets selected_entity to the body). Lets a
    // script stage the selection-aware descend gesture (BL-165).
    v.set_function("select_body", [this](const std::string& name) {
        for (const auto& [bid, bc] : m_world.bodies)
            if (bc.name == name)
            { m_ui.selected_entity = bid; m_ui.selection_hidden_for = null_entity; break; }
    });

    // Force the player corp balance to an exact value (verify harness): lets a
    // script stage a debt scenario so the BL-073 interest charge and the in-debt
    // affordances (header badge + breakdown interest line) render deterministically.
    // Non-economic — it only moves the number; the next econ tick charges interest.
    v.set_function("set_balance", [this](float value) {
        const auto it = m_world.corporations.find(m_world.player_entity);
        if (it != m_world.corporations.end())
            it->second.balance = value;
    });

    // Data accessor: return every corporation building as a Lua array of
    // {corp, player, body, x, y} records, so a script (or the lib's
    // tour_buildings helper) can centre/capture each one without hard-coding
    // coordinates. Mirrors log_buildings, but returns data instead of logging.
    v.set_function("buildings", [this]() {
        sol::state& s = m_lua.state();
        sol::table  out = s.create_table();
        int idx = 0;
        for (const auto& [corp_id, corp] : m_world.corporations)
        {
            const bool player = (corp_id == m_world.player_entity);
            for (entity_id bld_id : corp.assets)
            {
                const auto bld_it = m_world.buildings.find(bld_id);
                if (bld_it == m_world.buildings.end())
                    continue;
                const auto tile_it = m_world.tiles.find(bld_it->second.tile);
                if (tile_it == m_world.tiles.end())
                    continue;
                sol::table rec = s.create_table();
                rec["corp"]   = static_cast<unsigned>(corp_id);
                rec["player"] = player;
                rec["body"]   = static_cast<unsigned>(tile_it->second.body);
                rec["x"]      = tile_it->second.grid_x;
                rec["y"]      = tile_it->second.grid_y;
                // Type name, so a script can pick the building it actually wants to
                // stage (an extraction site rather than whichever asset happens to
                // come first) instead of hard-coding grid coordinates that a
                // generation change silently invalidates.
                rec["type"]   = ui::building_type_name(bld_it->second.type);
                out[++idx]    = rec;
            }
        }
        return out;
    });

    // Diagnostic: log the economy state the panel surfaces (balances, pools,
    // market supply/demand, building run states) to stderr, so a verify run gives
    // a textual confirmation of the same data the panel renders visually.
    v.set_function("dump_economy", [this]() {
        for (const auto& [cid, cc] : m_world.corporations)
            SDL_Log("economy balance: %-26s %.1f%s",
                    cc.name.c_str(), cc.balance,
                    cc.balance < 0.0f ? "  [NEGATIVE]" : "");
        for (const auto& [key, pool] : m_world.corp_body_pools)
            for (std::size_t r = 0; r < resource_count; ++r)
                if (pool.quantities[r] > 0.0f)
                    SDL_Log("economy pool: corp=%u body=%u %s=%.1f",
                            static_cast<unsigned>(key.first),
                            static_cast<unsigned>(key.second),
                            ui::resource_name(static_cast<resource_type>(r)),
                            pool.quantities[r]);
        for (const auto& [mid, mc] : m_world.markets)
            for (std::size_t r = 0; r < resource_count; ++r)
                if (mc.supply[r] > 0.0f || mc.demand[r] > 0.0f)
                    SDL_Log("economy market: %s supply=%.1f demand=%.1f price=%.2f",
                            ui::resource_name(static_cast<resource_type>(r)),
                            mc.supply[r], mc.demand[r], mc.price[r]);
        for (const building_report& br : m_last_econ_report.buildings)
            SDL_Log("economy building: corp=%u %s out=%.1f %s",
                    static_cast<unsigned>(br.corp),
                    ui::building_type_name(br.type), br.output_quantity,
                    br.active ? "active" : "idle");
    });

    // Data export (verify harness): write the live economy to a set of CSV files for
    // external analysis / ledger mock-ups (e.g. Power BI). Read-only — run after
    // econ_step(N) to snapshot the world. Writes corporations / stockpiles / markets /
    // cashflow / workforce / buildings plus the player balance-trend time series. Names
    // are quoted; the world is deterministic so the export is reproducible. See
    // scripts/verify/export_mockdata.lua.
    v.set_function("export_data", [this](const std::string& dir) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        const auto path = [&](const char* name) { return dir + "/" + name; };
        const auto corp_name = [this](entity_id id) -> std::string {
            const auto it = m_world.corporations.find(id);
            return it != m_world.corporations.end() ? it->second.name
                                                    : "corp#" + std::to_string(id);
        };
        const auto body_name = [this](entity_id id) -> std::string {
            const auto it = m_world.bodies.find(id);
            return it != m_world.bodies.end() ? it->second.name
                                              : "body#" + std::to_string(id);
        };
        const auto focus_name = [](industrial_focus f) {
            return f == industrial_focus::extraction ? "Extraction"
                 : f == industrial_focus::processing ? "Processing" : "Trade";
        };

        // corporations.csv — one row per corp: identity + solvency.
        {
            std::ofstream f(path("corporations.csv"));
            f << "corp_id,name,focus,home_nation,balance,starting_capital,since_start,buildings\n";
            for (const auto& [cid, cc] : m_world.corporations)
            {
                std::string nation = "-";
                const auto nit = m_world.nations.find(cc.home_nation);
                if (nit != m_world.nations.end()) nation = nit->second.name;
                f << cid << ",\"" << cc.name << "\"," << focus_name(cc.focus) << ",\"" << nation
                  << "\"," << cc.balance << "," << cc.starting_capital << ","
                  << (cc.balance - cc.starting_capital) << "," << cc.assets.size() << "\n";
            }
        }
        // stockpiles.csv — one row per (corp, body, resource) with stock > 0.
        {
            std::ofstream f(path("stockpiles.csv"));
            f << "corp_id,corp_name,body_id,body_name,resource,quantity\n";
            for (const auto& [key, pool] : m_world.corp_body_pools)
                for (std::size_t r = 0; r < resource_count; ++r)
                    if (pool.quantities[r] > 0.0f)
                        f << key.first << ",\"" << corp_name(key.first) << "\"," << key.second
                          << ",\"" << body_name(key.second) << "\","
                          << ui::resource_name(static_cast<resource_type>(r)) << ","
                          << pool.quantities[r] << "\n";
        }
        // Per-market display label = its generated city name (population centre anchoring
        // the market's centre tile), or the body name as a fallback. Shared with the
        // market ledger's city selector so the CSV and the game agree.
        const auto market_label = [this](entity_id mid) {
            return ui::market_city_name(m_world, mid);
        };

        // markets.csv — snapshot, one row per (market, tradeable resource). Split by
        // market_id/market_label so multiple markets on a body are distinguishable.
        {
            std::ofstream f(path("markets.csv"));
            f << "market_id,market_label,body_id,body_name,resource,supply,demand,price,base_price\n";
            for (const auto& [mid, mc] : m_world.markets)
                for (std::size_t r = 0; r < resource_count; ++r)
                    if (mc.base_price[r] > 0.0f)
                        f << mid << ",\"" << market_label(mid) << "\"," << mc.body << ",\""
                          << body_name(mc.body) << "\","
                          << ui::resource_name(static_cast<resource_type>(r)) << ","
                          << mc.supply[r] << "," << mc.demand[r] << "," << mc.price[r] << ","
                          << mc.base_price[r] << "\n";
        }
        // market_prices.csv — the price/supply/demand TIME SERIES per (market, resource,
        // tick) from m_market_history, so the "price over time" charts have real curves.
        // tick is the sample index (0 = oldest retained, capped at plot_history_cap).
        {
            std::ofstream f(path("market_prices.csv"));
            f << "market_id,market_label,body_name,resource,tick,price,supply,demand\n";
            for (const auto& [mid, series] : m_market_history)
            {
                const auto mit = m_world.markets.find(mid);
                const std::string bn = mit != m_world.markets.end() ? body_name(mit->second.body) : "-";
                const std::string lbl = market_label(mid);
                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    const ui::resource_plot_series& s = series[r];
                    for (std::size_t t = 0; t < s.price.size(); ++t)
                        f << mid << ",\"" << lbl << "\",\"" << bn << "\","
                          << ui::resource_name(static_cast<resource_type>(r)) << ","
                          << t << "," << s.price[t] << ","
                          << (t < s.supply.size() ? s.supply[t] : 0.0f) << ","
                          << (t < s.demand.size() ? s.demand[t] : 0.0f) << "\n";
                }
            }
        }
        // cashflow.csv — last tick's itemised per-corp budget (the Balance Ledger data).
        {
            std::ofstream f(path("cashflow.csv"));
            f << "corp_id,corp_name,income,expenditure,maintenance,wages,interest,net\n";
            for (const auto& [cid, bud] : m_last_econ_report.budgets)
                f << cid << ",\"" << corp_name(cid) << "\"," << bud.income << ","
                  << bud.expenditure << "," << bud.maintenance << "," << bud.wages << ","
                  << bud.interest << "," << bud.net() << "\n";
        }
        // workforce.csv — per (corp, body) labour staffing this tick (100 = fully staffed).
        {
            std::ofstream f(path("workforce.csv"));
            f << "corp_id,corp_name,body_id,body_name,staffing_pct\n";
            for (const auto& [key, scalar] : m_last_econ_report.workforce_contention)
                f << key.first << ",\"" << corp_name(key.first) << "\"," << key.second
                  << ",\"" << body_name(key.second) << "\"," << (scalar * 100.0f) << "\n";
        }
        // buildings.csv — per-building output + run state this tick.
        {
            std::ofstream f(path("buildings.csv"));
            f << "building_id,corp_id,corp_name,body_name,type,output,active,exhausted\n";
            for (const building_report& br : m_last_econ_report.buildings)
                f << br.building << "," << br.corp << ",\"" << corp_name(br.corp) << "\",\""
                  << body_name(br.body) << "\"," << ui::building_type_name(br.type) << ","
                  << br.output_quantity << "," << (br.active ? 1 : 0) << ","
                  << (br.exhausted ? 1 : 0) << "\n";
        }
        // player_timeseries.csv — the player corp's balance / income / expenditure per
        // econ tick (the trend series the header + economy panel already accumulate).
        {
            std::ofstream f(path("player_timeseries.csv"));
            f << "tick,balance,income,expenditure\n";
            for (std::size_t i = 0; i < m_balance_history.size(); ++i)
                f << i << "," << m_balance_history[i] << ","
                  << (i < m_income_history.size() ? m_income_history[i] : 0.0f) << ","
                  << (i < m_expenditure_history.size() ? m_expenditure_history[i] : 0.0f) << "\n";
        }
        SDL_Log("export_data: wrote 8 CSVs to %s", dir.c_str());
    });

    // Diagnostic: log every corporation building's grid position and owner so a
    // script author (or Claude) can aim the pan/zoom at the corporate tiles.
    v.set_function("log_buildings", [this]() {
        for (const auto& [corp_id, corp] : m_world.corporations)
        {
            const bool player = (corp_id == m_world.player_entity);
            for (entity_id bld_id : corp.assets)
            {
                const auto bld_it = m_world.buildings.find(bld_id);
                if (bld_it == m_world.buildings.end())
                    continue;
                const auto tile_it = m_world.tiles.find(bld_it->second.tile);
                if (tile_it == m_world.tiles.end())
                    continue;
                SDL_Log("verify building: corp=%u%s body=%u grid=(%d,%d)",
                        static_cast<unsigned>(corp_id), player ? " [player]" : "",
                        static_cast<unsigned>(tile_it->second.body),
                        tile_it->second.grid_x, tile_it->second.grid_y);
            }
        }
    });

    // Seed a test convoy in flight between two named bodies — lets visual-verify
    // scripts capture the Supply lens without depending on auto-dispatch (which
    // requires launchpads not present in the generated world). If the destination
    // body has no market a minimal one is created so the Solar route line renders.
    v.set_function("seed_convoy",
        [this](const std::string& src_name, const std::string& dst_name,
               const std::string& res_name, float qty, sol::optional<float> progress_opt)
        {
            const entity_id src_body = find_body(m_world, src_name);
            const entity_id dst_body = find_body(m_world, dst_name);
            if (src_body == null_entity || dst_body == null_entity)
                return;
            // Return the first market on a body, creating a minimal stub if absent.
            auto ensure_market = [this](entity_id body) -> entity_id {
                for (const auto& [mid, mc] : m_world.markets)
                    if (mc.body == body) return mid;
                const entity_id mid = m_world.create_entity();
                market_component mc{};
                mc.body = body;
                m_world.markets[mid] = mc;
                return mid;
            };
            convoy_component cv{};
            cv.source_market  = ensure_market(src_body);
            cv.dest_market    = ensure_market(dst_body);
            cv.cargo_resource = resource_from_name(res_name);
            cv.cargo_qty      = qty;
            cv.mode           = convoy_mode::space;
            cv.progress       = progress_opt.value_or(0.5f);
            cv.arrived        = false;
            cv.corp           = m_world.player_entity;
            cv.speed          = 0.1f;
            m_world.convoys.push_back(cv);
        });

    // Set a body's survey state deterministically for capture (BL-067). regions_done
    // 0 → hidden; >= region total → surveyed; in between → scanning with that many
    // regions revealed. Lets a golden show the unsurveyed / partial / full states
    // and the Solar badge without ticking the (paused) verify clock.
    v.set_function("set_survey",
        [this](const std::string& name, int regions_done)
        {
            const entity_id b = find_body(m_world, name);
            const auto it = m_world.bodies.find(b);
            if (it == m_world.bodies.end())
                return;
            survey_state& s = it->second.survey;
            const int total = survey_region_count(it->second.grid_width, it->second.grid_height);
            s.regions_total = total;
            s.regions_done  = std::clamp(regions_done, 0, total);
            s.ticks_remaining = 0;
            s.phase = (s.regions_done <= 0)     ? survey_phase::hidden
                    : (s.regions_done >= total) ? survey_phase::surveyed
                                                : survey_phase::scanning;
        });

    // Data accessor: return every population centre as a Lua array of
    // {body, x, y, scale, population, habitability} records, so a BL-069 script can
    // centre/select a centre's tile without hard-coding coordinates.
    v.set_function("population_centres", [this]() {
        sol::state& s = m_lua.state();
        sol::table  out = s.create_table();
        int idx = 0;
        for (const auto& [cid, pc] : m_world.population_centres)
        {
            const auto tit = m_world.population_centre_tile.find(cid);
            if (tit == m_world.population_centre_tile.end()) continue;
            const auto tile_it = m_world.tiles.find(tit->second);
            if (tile_it == m_world.tiles.end()) continue;
            sol::table rec = s.create_table();
            rec["body"]         = static_cast<unsigned>(tile_it->second.body);
            rec["x"]            = tile_it->second.grid_x;
            rec["y"]            = tile_it->second.grid_y;
            rec["scale"]        = pc.scale;
            rec["population"]   = pc.population;
            rec["habitability"] = pc.habitability;
            out[++idx]          = rec;
        }
        return out;
    });

    try
    {
        // Auto-load the helper library (scripts/verify/lib.lua) from the script's
        // own directory before running it, so scripts get the high-level helpers
        // (sweep_overlays, tour_buildings, frame_tile) without a `require` (the
        // package lib is not opened). Loading it from the script's directory means
        // iterating against the source path picks up the source lib, not a stale
        // build copy. Skipped when the target *is* the library.
        namespace fs = std::filesystem;
        const fs::path script{script_path};
        const fs::path lib = script.parent_path() / "lib.lua";
        if (fs::exists(lib) &&
            fs::weakly_canonical(lib) != fs::weakly_canonical(script))
            m_lua.load(lib.string());

        m_lua.load(script_path);
    }
    catch (const std::exception& e)
    {
        SDL_Log("verify script failed: %s", e.what());
        return 1;
    }

    // Non-zero exit if any capture failed its golden diff, so the harness/skill
    // reads an advisory PASS/FAIL off the process result as well as the logs.
    if (m_verify_failures > 0)
    {
        SDL_Log("verify: %d capture(s) failed golden diff", m_verify_failures);
        return 1;
    }
    return 0;
}

void app::capture_frame(const std::string& name)
{
    m_capture_name      = name;
    m_capture_requested = true;
    render(); // composits the frame, then save_screenshot() runs before present
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

    // No game bindings on the menu or the generation screen — there is nothing to
    // navigate yet (the generation screen carries its own two buttons).
    if (m_screen != app_screen::in_game)
        return;

    // Esc toggles the in-app system menu (BL-070) — cheap keyboard parity with the
    // corner gear button. Handled before the ImGui keyboard guard so it works even
    // while the popup (or another panel) holds focus. Precedence, highest first: an
    // armed exit-confirm backs out; an open system menu closes; an open sticky
    // detail card unwinds one drill level, then hides (BL-194/196 — Esc reaches the
    // menu only once the card is fully closed, so a single press never both closes
    // the card and opens the menu); otherwise the menu opens.
    if (key.scancode == SDL_SCANCODE_ESCAPE)
    {
        // Mirror the card's own draw gate exactly (selection_card.cpp): a valid,
        // non-dismissed selection whose kind actually resolves — so Esc never acts
        // on an invisible card.
        const bool card_open = m_ui.selected_entity != null_entity &&
                               m_ui.selected_entity != m_ui.selection_hidden_for &&
                               ui::selection_kind_of(m_world, m_ui.selected_entity) !=
                                   ui::selection_kind::none;
        if (m_ui.confirm_exit_pending)
            m_ui.confirm_exit_pending = false;
        else if (m_ui.show_system_menu)
            m_ui.show_system_menu = false;
        else if (card_open && !m_ui.card_stack.empty())
            m_ui.card_stack.pop_back();                       // unwind one drill level
        else if (card_open)
            m_ui.selection_hidden_for = m_ui.selected_entity; // hide, not destroy
        else
            m_ui.show_system_menu = true;
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
            m_show_tech_tree = !m_show_tech_tree;
            return;

        // Everything else is a canvas navigation command.
        default:
            ui::apply_canvas_command(m_world, m_ui, cmd);
    }
}

void app::draw_main_menu()
{
    const ImVec2 disp = ImGui::GetIO().DisplaySize;

    // A dark, centred title card. Borderless, non-interactive-move window sized to
    // its contents; buttons carry the only input. Kept deliberately spare — this is
    // the launch entry point, not a settings hub (no Load/Save in the prototype).
    ImGui::SetNextWindowPos({disp.x * 0.5f, disp.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("##main_menu", nullptr, flags))
    {
        // Title, centred over the button column.
        const char* title = "PROJECT IO";
        const char* tag   = "Near-future corporate 4X";
        auto centre_text = [&](const char* s, ImU32 col) {
            const float w = ImGui::CalcTextSize(s).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (280.0f - w) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(s);
            ImGui::PopStyleColor();
        };
        centre_text(title, IM_COL32(225, 230, 240, 255));
        centre_text(tag,   IM_COL32(120, 128, 145, 255));
        ImGui::Dummy({0.0f, 18.0f});

        // --- New World setup (BL-114). Every widget edits m_pending_world_params,
        //     which the wizard continues to edit and start_new_game() finally
        //     consumes; each carries a unique ##id so it never collides with the
        //     centred buttons below. ---
        world_params& wp = m_pending_world_params;
        ImGui::SeparatorText("New World");

        // Seed — hex entry + a one-shot randomise. The random_device draw feeds ONLY
        // the seed value; no entropy ever enters world generation, which stays a pure
        // function of this seed (same seed + knobs -> identical world).
        ImGui::TextUnformatted("Seed");
        ImGui::SetNextItemWidth(210.0f);
        ImGui::InputScalar("##seed", ImGuiDataType_U32, &wp.seed, nullptr, nullptr,
                           "%08X", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::SameLine();
        if (ImGui::Button("Roll##seedroll", {64.0f, 0.0f}))
        {
            std::random_device rd;
            wp.seed = static_cast<uint32_t>(rd());
        }
        {
            // Copyable readout of the reproducible key.
            char seedhex[16];
            std::snprintf(seedhex, sizeof seedhex, "%08X", wp.seed);
            if (ImGui::Button("Copy seed##seedcopy", {280.0f, 0.0f}))
                ImGui::SetClipboardText(seedhex);
        }

        // Resource abundance — Earth-like 'Standard' is the ceiling; the leaner tiers
        // step down (GENERATION_STRATEGY.md § The resource ceiling).
        ImGui::TextUnformatted("Resources");
        int ab = static_cast<int>(wp.abundance);
        ImGui::RadioButton("Sparse##ab",   &ab, static_cast<int>(abundance_level::sparse));
        ImGui::SameLine();
        ImGui::RadioButton("Lean##ab",     &ab, static_cast<int>(abundance_level::lean));
        ImGui::SameLine();
        ImGui::RadioButton("Standard##ab", &ab, static_cast<int>(abundance_level::standard));
        wp.abundance = static_cast<abundance_level>(ab);

        // No nation knob: the number of nations on the home body is a consequence of
        // its landmass and the minimum-viable-territory floor, not a pre-set target
        // (docs/generation/NATION_GENERATION.md § Pass 1 / Pass 2c).

        // Bodies — the count knob is phased to a later update; shown disabled so the
        // intent reads without implying it works yet.
        ImGui::BeginDisabled();
        int bodies_stub = 5;
        ImGui::SetNextItemWidth(280.0f);
        ImGui::SliderInt("##bodies", &bodies_stub, 5, 5, "Bodies: %d (fixed)");
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("A variable body count is coming in a later update.");

        // The Planetology knobs (BL-167) used to sit here as six sliders. They now
        // live in the New World wizard, one decision per chain stage, where each is
        // taken against a chart of the system as it stands — a slider whose effect
        // you cannot see is a slider you cannot judge.
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 280.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 128, 145, 255));
            ImGui::TextUnformatted("The world's character is chosen during generation.");
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
        }

        ImGui::Dummy({0.0f, 12.0f});

        const ImVec2 btn = {280.0f, 40.0f};
        if (ImGui::Button("New Game", btn))
            open_new_world_wizard();
        ImGui::Dummy({0.0f, 6.0f});
        if (ImGui::Button("Quit", btn))
            m_quit_requested = true;
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// The New World wizard (BL-167)
//
// THREE rounds, not ten stages. The chain still has ten links and the player is
// still shown every one of them — each round stacks its stages' charts and
// explanations into one scroll — but the decisions are batched into three
// thematic groups. Ben's call (2026-07-22): "we don't need so many rounds, it's
// just too slow - try to batch them together thematically."
//
// What the player sets is a PREFERENCE, not a parameter: a named lean per axis
// ("Dimmer", "Metal-rich"), resolved against the seed by resolve_preferences. No
// raw generated value is editable, and none is printed in the decision area — the
// charts above it show what the roll actually produced, which is the only honest
// feedback there is. "If you have preferences you can find them, but really you
// don't get full customization. Just a step more detail than a seeded world."
//
// NOTHING IS GENERATED HERE. resolve_preferences and preview_system run the whole
// chain over the prototype body set whenever a control moves; both are pure,
// throwaway, and never touch m_world. The world is built once, from the finished
// preferences, when the player presses "Begin" at the last round (start_new_game).
//
// Rounds are causal: rerolling round A re-draws B and C downstream. That is
// correct — the chain is causal too — and it is why Back is a plain revision with
// no per-round snapshot to keep.
//
// Charts are drawn with ui::charts — the primitives extracted from the tile
// selection graphs — so the wizard and the in-game surfaces share one visual
// language by construction rather than by imitation.
// ---------------------------------------------------------------------------

namespace {

/// The paragraph shown at each stage: the actual chemistry, in the plainest words
/// that stay true. A player should leave the wizard knowing WHY the gates sit
/// where they do, not just which control moved which bar.
const char* stage_explainer(chain_stage s)
{
    switch (s)
    {
        case chain_stage::system: return
            "Everything heavier than helium is supernova debris, so one nebula hands the "
            "same metallicity to every body it forms. Stellar mass fixes luminosity, and "
            "with it where liquid water is possible at all.";
        case chain_stage::accretion: return
            "Mass sets radius, and the two together set escape velocity. That single "
            "number decides what a body can hold on to for the next four billion years, "
            "so it is chosen here and never revisited.";
        case chain_stage::air: return
            "A body keeps its air if gravity beats sunlight. Escape velocity to the "
            "fourth power over instellation separates every world with an atmosphere "
            "from every world without. No magnetic field required.";
        case chain_stage::engine: return
            "Radiogenic heat is made in the mantle and lost through the surface, so the "
            "interior clock runs on volume over area. A hot interior keeps plates moving, "
            "and plate margins are where copper concentrates.";
        case chain_stage::water: return
            "Liquid water has an irreversible failure on each side of it: a runaway "
            "greenhouse above, an ice-albedo lock below. The band between them is narrow, "
            "and neither edge lets a world back.";
        case chain_stage::spark: return
            "Abiogenesis has happened once, here, at a sample size of one - so its "
            "probability is unconstrained by orders of magnitude. The prototype fires it "
            "wherever the chemistry allows, which is a design choice, not a measurement.";
        case chain_stage::breath: return
            "Photosynthesis and respiration are exact inverses, so a biosphere only "
            "leaves free oxygen behind when carbon is buried out of contact with it. "
            "Until then dissolved iron absorbs every molecule.";
        case chain_stage::green: return
            "About ninety percent of Earth's coal comes from one climatic window: everwet "
            "equatorial mires over subsiding basins. The cause is climate and tectonics, "
            "not anything about the plants themselves.";
        case chain_stage::legacy: return
            "Fossil resources key off the biosphere's PEAK and survive its extinction; "
            "living resources key off what is alive now and die with it. That split is "
            "what leaves a dead world still worth mining.";
        case chain_stage::spend: return
            "A richer world industrialises earlier, so it is further drawn down when you "
            "arrive. The ore is still in the ground; the cheap ore is not. This is the "
            "generated reason a corporation goes to space.";
        default: return "";
    }
}

/// One round of the wizard: a thematic run of chain stages, charted together and
/// closed by the preferences that shape them. A round's stages are contiguous
/// links of the chain, so the span is just a first/last pair.
struct wizard_round
{
    const char* name;     ///< The large header.
    const char* question; ///< What the round settles, in one line.
    chain_stage first;    ///< Inclusive.
    chain_stage last;     ///< Inclusive.
};

/// The A / B / C grouping — the chain's own shape: what kind of world is this,
/// what happened on it, and what did you walk into. Caller clamps the index.
const wizard_round& wizard_round_at(int r)
{
    static const wizard_round rounds[3] = {
        { "The System",  "What kind of world is this, and what is it made of?",
          chain_stage::system, chain_stage::engine },
        { "Life",        "What happened on it, and what did that leave in the rocks?",
          chain_stage::water,  chain_stage::green  },
        { "Inheritance", "What did the era before you already take?",
          chain_stage::legacy, chain_stage::spend  },
    };
    return rounds[r];
}

/// How many preference rows a round owns, and how many dim caption lines sit under
/// them. Both feed the height reserved for the decision block, which is pinned to
/// the bottom so the charts get everything left over.
int round_pref_count(int r) { return (r == 0) ? 4 : (r == 1) ? 3 : 1; }
int round_note_lines(int r) { return (r == 1) ? 3 : 1; } ///< B carries the iron/coal caption.

/// One preference row: a name, then four segmented options with `Any` first.
///
/// Named leans only, and deliberately no number anywhere. A lean narrows the range
/// the seed is sampled from; it never pins a value. The moment a player can read
/// `1.0342` off this screen it is a settings form again rather than a preference,
/// which is the whole distinction the wizard is built on.
///
/// @return true when the player moved it, so the caller can mark the preview dirty.
bool lean_row(const char* id, const char* label, lean& value,
              const char* low, const char* mid, const char* high)
{
    static constexpr lean order[4] = { lean::any, lean::low, lean::mid, lean::high };
    const char* names[4] = { "Any", low, mid, high };

    // PushID scopes the four buttons to this row, so two rows sharing an option
    // name ("Balanced" appears under both Ocean and Oxygen) never collide.
    ImGui::PushID(id);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);

    bool changed = false;
    for (int i = 0; i < 4; ++i)
    {
        ImGui::SameLine(i == 0 ? 170.0f : 0.0f);
        int v = static_cast<int>(value);
        if (ImGui::RadioButton(names[i], &v, static_cast<int>(order[i])))
        {
            value   = order[i];
            changed = true;
        }
    }
    ImGui::PopID();
    return changed;
}

} // namespace

void app::draw_generation_screen()
{
    const ImVec2      disp  = ImGui::GetIO().DisplaySize;
    const ImGuiStyle& style = ImGui::GetStyle();

    // One reroll counter per round; the wizard and the resolver have to agree on
    // how many rounds there are.
    static_assert(sizeof(world_preferences::roll)
                      == sizeof(uint32_t) * static_cast<std::size_t>(wizard_round_count),
                  "world_preferences::roll must carry one counter per wizard round");

    // Clamp first — Back/Continue and verify.generation_stage all write this.
    if (m_wiz_round < 0)                   m_wiz_round = 0;
    if (m_wiz_round >= wizard_round_count) m_wiz_round = wizard_round_count - 1;

    // Recompute only when a control moved. Resolution plus the chain is cheap but
    // not free, and a cached preview keeps the charts still while the player reads
    // them — a chart that twitches every frame cannot be read at all.
    if (m_wiz_dirty || m_wiz_preview.empty())
    {
        refresh_wizard_preview();
        m_wiz_dirty = false;
    }
    if (m_wiz_preview.empty())
        return; // defensive: the preview is the wizard's only data source

    const wizard_round& wr       = wizard_round_at(m_wiz_round);
    const int           n_bodies = std::min(static_cast<int>(m_wiz_preview.size()),
                                            prototype_body_count());

    // The homeworld is the subject of every single-body chart. Located by its
    // authored flag rather than by position, so the body list can be reordered.
    int home = 0;
    for (int i = 0; i < n_bodies; ++i)
        if (prototype_body(i).is_homeworld) { home = i; break; }
    const planetology_state& hs = m_wiz_preview[static_cast<std::size_t>(home)];
    const planetology_state& hu = (static_cast<std::size_t>(home) < m_wiz_undrawn.size())
                                ? m_wiz_undrawn[static_cast<std::size_t>(home)] : hs;

    constexpr ImU32 col_bright = IM_COL32(225, 230, 240, 255);
    constexpr ImU32 col_dim    = IM_COL32(120, 128, 145, 255);
    constexpr ImU32 col_gate   = IM_COL32(220, 170,  90, 255); // a gate reads amber, not as a gridline
    constexpr ImU32 col_home   = IM_COL32(150, 235, 160, 255); // homeworld, in the tile graphs' subject green

    // One colour per body, so a body keeps its identity across every chart.
    static constexpr ImU32 k_body_cols[4] = {
        IM_COL32(150, 160, 190, 255), IM_COL32(190, 175, 140, 255),
        IM_COL32(170, 140, 190, 255), IM_COL32(140, 185, 205, 255),
    };
    auto body_colour = [&](int i) -> ImU32 {
        return prototype_body(i).is_homeworld ? col_home : k_body_cols[i & 3];
    };

    // Scratch column buffer. Every chart draws immediately inside chart_row, so one
    // buffer is enough and no bar outlives the call that filled it.
    ui::charts::bar bars[8];

    // One column per body, reading a single scalar off that body's chain state.
    auto body_bars = [&](auto&& pick) -> std::size_t {
        const int n = std::min(n_bodies, 8);
        for (int i = 0; i < n; ++i)
        {
            bars[i].value  = pick(m_wiz_preview[static_cast<std::size_t>(i)]);
            bars[i].colour = body_colour(i);
            bars[i].label  = prototype_body(i).name;
            bars[i].hollow = false;
        }
        return static_cast<std::size_t>(n);
    };
    auto bars_peak = [&](std::size_t n) {
        float p = 0.0f;
        for (std::size_t i = 0; i < n; ++i)
            p = std::max(p, bars[i].value);
        return p;
    };
    auto endow = [](const planetology_state& s, resource_type r) {
        return s.endowment[static_cast<std::size_t>(r)];
    };

    // Columns from a resource list, skipping anything the body does not have at all.
    // Names and colours come from presentation_of, so the wizard's endowment charts
    // and the in-game ledgers agree on both.
    auto resource_bars = [&](const planetology_state& s,
                             const resource_type* list, std::size_t count) -> std::size_t {
        std::size_t n = 0;
        for (std::size_t i = 0; i < count && n < 8; ++i)
        {
            const float v = endow(s, list[i]);
            if (v <= 0.0f)
                continue;
            const ui::resource_presentation& rp = ui::presentation_of(list[i]);
            bars[n].value  = v;
            bars[n].colour = rp.colour;
            bars[n].label  = rp.name;
            bars[n].hollow = false;
            ++n;
        }
        return n;
    };

    // One dim, wrapping text helper — every subtitle and caption reads in the same
    // colour as the menu's tagline. Declared out here so the per-stage sections can
    // use it as well as the decision block.
    auto dim_text = [&](const char* s) {
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextWrapped("%s", s);
        ImGui::PopStyleColor();
    };

    // The same, wrapped to a readable measure instead of to the panel edge. The
    // panel is as wide as the charts want, which is far too wide for prose.
    auto dim_para = [&](const char* s) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 820.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextUnformatted(s);
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
    };

    // --- Chart cells ---
    // Two charts sit side by side whenever the panel is wide enough. That is what
    // keeps a round of four stages to roughly a screen and a half of scroll rather
    // than three; batching the rounds is pointless if the reading cost just moves
    // from clicks to scrolling. `cols` and `col_w` are measured once the chart
    // region is known; `cell` tracks the column the next chart lands in.
    int   cols  = 1;
    float col_w = 0.0f;
    int   cell  = 0; // 0 == the cursor is at the start of a chart line

    // One bordered chart, laid out exactly like the tile-selection graphs: the
    // header indents to the plot origin, the plot is reserved with a Dummy, and the
    // body draws into the CHILD-LOCAL draw list so everything clips to the box.
    // A @p span of 2 takes the whole line — for a chart that needs the room (eight
    // clustered columns) or is the odd one out in its stage.
    auto chart_row = [&](const char* id, const char* title, float h, int span, auto&& body) {
        const bool full = (span >= cols);
        if (cell != 0 && !full)
            ImGui::SameLine(0.0f, style.ItemSpacing.x);
        ImGui::BeginChild(id, {full ? 0.0f : col_w, ui::charts::chart_row_height(h)}, true,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        ImGui::Indent(ui::charts::gutter);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ui::palette::selection), "%s", title);
        ImGui::Unindent(ui::charts::gutter);
        const ImVec2 p  = ImGui::GetCursorScreenPos();
        const float  cw = ImGui::GetContentRegionAvail().x;
        ImGui::Dummy({cw, h});
        body(cdl, p, ImVec2{p.x + cw, p.y + h});
        ImGui::EndChild();
        cell = full ? 0 : (cell + 1) % cols;
    };

    // Start the next chart on a fresh line. Called at every stage boundary, so one
    // stage's plots never share a row with the next stage's.
    auto chart_break = [&]() { cell = 0; };

    // --- One chain stage, as a section inside its round ---
    // Its name and the question it answers as the heading, the physics it encodes
    // beneath, which bodies it killed, then its charts. The player still watches the
    // chain work link by link — they have just stopped clicking between the links.
    auto draw_stage = [&](chain_stage s) {
        chart_break();

        char head[160];
        std::snprintf(head, sizeof head, "%s - %s", chain_stage_name(s), chain_stage_title(s));
        ImGui::SeparatorText(head);
        dim_para(stage_explainer(s));

        // Which bodies this gate kills. The chain's interesting output is WHICH GATE
        // A BODY DIED AT, so it is named at the gate and not only in the verdict.
        {
            std::string lost;
            for (int i = 0; i < n_bodies; ++i)
            {
                if (m_wiz_preview[static_cast<std::size_t>(i)].died_at != s)
                    continue;
                if (!lost.empty())
                    lost += ", ";
                lost += prototype_body(i).name;
            }
            if (!lost.empty())
                dim_para(("Lost at this gate: " + lost).c_str());
        }
        ImGui::Spacing();

        switch (s)
        {
            case chain_stage::system:
            {
                const std::size_t n = body_bars([](const planetology_state& st) { return st.instellation; });
                const float c = ui::charts::tight_ceil(std::max(bars_peak(n), 1.1f));
                chart_row("##c_inst", "Instellation (S, Earth = 1)", 120.0f, 1,
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    ui::charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
                    ui::charts::threshold_line(dl, mn, mx, 1.0512f, c, col_gate, "runaway greenhouse");
                });

                // The homeworld on its own axis. The two gates that decide whether the
                // world you inherit can hold liquid water sit within a factor of three
                // of each other, and they are unreadable on an axis stretched by an
                // inner body taking six suns.
                ui::charts::bar hb[1];
                hb[0].value  = hs.instellation;
                hb[0].colour = col_home;
                hb[0].label  = prototype_body(home).name;
                hb[0].hollow = false;
                const float cc = std::max(2.0f, ui::charts::tight_ceil(hs.instellation));
                chart_row("##c_corridor", "Homeworld corridor (S, Earth = 1)", 120.0f, 1,
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    ui::charts::draw_bars(dl, mn, mx, hb, 1, cc, "%.2f");
                    ui::charts::threshold_line(dl, mn, mx, 1.0512f, cc, col_gate, "runaway greenhouse");
                    ui::charts::threshold_line(dl, mn, mx, 0.3438f, cc, col_gate, "freeze-out");
                });
                break;
            }

            case chain_stage::accretion:
            {
                const std::size_t n = body_bars([](const planetology_state& st) { return st.v_esc_kms; });
                const float c = ui::charts::tight_ceil(bars_peak(n));
                chart_row("##c_vesc", "Escape velocity (km/s)", 140.0f, 2,
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    ui::charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
                });
                break;
            }

            case chain_stage::air:
            {
                // The losers get their own scale. The shoreline spans three orders of
                // magnitude, so the airless margin is invisible beside a body that
                // cleared the gate seven times over - and the margin is the interesting
                // part, because it says which failure was close. Gathered first because
                // whether it exists decides how wide the chart above it should be.
                ui::charts::bar sub[8];
                std::size_t sn = 0;
                for (int i = 0; i < n_bodies && sn < 8; ++i)
                {
                    const planetology_state& st = m_wiz_preview[static_cast<std::size_t>(i)];
                    if (st.shore >= 1.5f)
                        continue;
                    sub[sn].value  = st.shore;
                    sub[sn].colour = body_colour(i);
                    sub[sn].label  = prototype_body(i).name;
                    sub[sn].hollow = false;
                    ++sn;
                }

                const std::size_t n = body_bars([](const planetology_state& st) { return st.shore; });
                const float c = ui::charts::tight_ceil(std::max(bars_peak(n), 1.6f));
                chart_row("##c_shore", "Retention shoreline (v_esc^4 / S, Mars = 1)",
                          120.0f, sn > 0 ? 1 : 2,
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    ui::charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
                    ui::charts::threshold_line(dl, mn, mx, 1.5f, c, col_gate, "retains air");
                });

                if (sn > 0)
                {
                    float peak = 0.0f;
                    for (std::size_t i = 0; i < sn; ++i)
                        peak = std::max(peak, sub[i].value);
                    const float cs = ui::charts::tight_ceil(std::max(peak, 0.06f));
                    chart_row("##c_shore_lo", "Below the shoreline (same axis, rescaled)", 120.0f, 1,
                              [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                        ui::charts::draw_bars(dl, mn, mx, sub, sn, cs, "%.3f");
                        ui::charts::threshold_line(dl, mn, mx, 0.05f, cs, col_gate, "airless below");
                    });
                }
                break;
            }

            case chain_stage::engine:
            {
                const std::size_t n = body_bars([](const planetology_state& st) { return st.theta; });
                // The ceiling is forced past the upper gate so both edges of the
                // mobile-lid band render; a threshold above the axis top is dropped.
                const float c = ui::charts::tight_ceil(std::max(bars_peak(n), 2.4f));
                chart_row("##c_theta", "Interior heat budget (Earth now = 1)", 140.0f, 2,
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    ui::charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
                    ui::charts::threshold_line(dl, mn, mx, 0.55f, c, col_gate, "mobile lid floor");
                    ui::charts::threshold_line(dl, mn, mx, 2.20f, c, col_gate, "mobile lid ceiling");
                });
                break;
            }

            case chain_stage::water:
            {
                const std::size_t n = body_bars([](const planetology_state& st) { return st.surface_temp_k; });
                const float c = ui::charts::tight_ceil(std::max(bars_peak(n), 400.0f));
                chart_row("##c_temp", "Surface temperature (K)", 120.0f, 1,
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    ui::charts::draw_bars(dl, mn, mx, bars, n, c, "%.0f");
                    ui::charts::threshold_line(dl, mn, mx, 273.0f, c, col_gate, "water freezes");
                    ui::charts::threshold_line(dl, mn, mx, 373.0f, c, col_gate, "water boils");
                });

                // The land/sea split the ocean lean actually produced - it is the
                // generated water fraction, not the preference, that the tile pass reads.
                ui::charts::bar wb[2];
                wb[0].value  = hs.profile.water_fraction * 100.0f;
                wb[0].colour = IM_COL32(90, 150, 210, 255);
                wb[0].label  = "Ocean";
                wb[0].hollow = false;
                wb[1].value  = 100.0f - wb[0].value;
                wb[1].colour = IM_COL32(150, 190, 120, 255);
                wb[1].label  = "Land";
                wb[1].hollow = false;
                chart_row("##c_ocean", "Homeworld surface split", 120.0f, 1,
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    ui::charts::draw_bars(dl, mn, mx, wb, 2, 100.0f, "%.0f%%");
                });
                break;
            }

            case chain_stage::spark:
            {
                // How far up the ladder each body got. Ordinal, so the ceiling is the
                // top of the enum rather than a nice_ceil of the data.
                const std::size_t n = body_bars([](const planetology_state& st) {
                    return static_cast<float>(st.peak);
                });
                chart_row("##c_peak", "Peak biology reached (sterile 0 -> civilised 6)", 140.0f, 2,
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    ui::charts::draw_bars(dl, mn, mx, bars, n, 6.0f, "%.0f");
                });

                std::string ladder;
                for (int i = 0; i < n_bodies; ++i)
                {
                    if (!ladder.empty())
                        ladder += "   ";
                    ladder += prototype_body(i).name;
                    ladder += ": ";
                    ladder += life_stage_name(m_wiz_preview[static_cast<std::size_t>(i)].peak);
                }
                dim_para(ladder.c_str());
                break;
            }

            case chain_stage::breath:
            {
                // The money chart: one lean, two resources, opposite directions. Iron
                // wants a long ferruginous ocean; coal wants the land era that only
                // starts once that ocean has closed.
                static constexpr resource_type k_trade[] = {
                    resource_type::iron_ore, resource_type::coal };
                const std::size_t n = resource_bars(hs, k_trade, 2);
                const float c = ui::charts::tight_ceil(std::max(bars_peak(n), 1.0f));
                chart_row("##c_trade", "The iron / coal trade (homeworld endowment)", 120.0f, 1,
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    ui::charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
                });

                // The two windows those resources are actually measured from.
                ui::charts::bar gb[2];
                gb[0].value  = hs.ferruginous_gyr;
                gb[0].colour = ui::presentation_of(resource_type::iron_ore).colour;
                gb[0].label  = "Ferruginous ocean";
                gb[0].hollow = false;
                gb[1].value  = hs.marine_anoxia_gyr;
                gb[1].colour = ui::presentation_of(resource_type::petroleum).colour;
                gb[1].label  = "Marine anoxia";
                gb[1].hollow = false;
                const float cg = ui::charts::tight_ceil(std::max({gb[0].value, gb[1].value, 1.0f}));
                chart_row("##c_windows", "Anoxic windows (Gyr)", 120.0f, 1,
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    ui::charts::draw_bars(dl, mn, mx, gb, 2, cg, "%.2f");
                });

                dim_para("Iron is banded-iron deposits laid down while the ocean was still "
                         "anoxic; coal needs the land biosphere that only follows once "
                         "oxygen has won. Buying one spends the other.");
                break;
            }

            case chain_stage::green:
            {
                static constexpr resource_type k_green[] = {
                    resource_type::coal, resource_type::timber,
                    resource_type::agricultural_produce };
                const std::size_t n = resource_bars(hs, k_green, 3);
                if (n > 0)
                {
                    const float c = ui::charts::tight_ceil(std::max(bars_peak(n), 1.0f));
                    chart_row("##c_green", "What the land era leaves (homeworld endowment)", 120.0f, 1,
                              [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                        ui::charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
                    });
                }
                else
                {
                    dim_para("No land biosphere - no coal, no timber, no crops.");
                }

                ui::charts::bar ab[1];
                ab[0].value  = hs.arable_share * 100.0f;
                ab[0].colour = ui::presentation_of(resource_type::agricultural_produce).colour;
                ab[0].label  = "Arable";
                ab[0].hollow = false;
                const float ca = ui::charts::tight_ceil(std::max(ab[0].value, 10.0f));
                chart_row("##c_arable", "Arable share of land", 120.0f, n > 0 ? 1 : 2,
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    ui::charts::draw_bars(dl, mn, mx, ab, 1, ca, "%.0f%%");
                });
                break;
            }

            case chain_stage::legacy:
            {
                // The payoff. Every non-zero endowment the homeworld carries, split into
                // three charts only because a single clustered chart's legend cannot
                // stack sixteen rows inside one plot - the coverage is the full set.
                static constexpr resource_type k_metals[] = {
                    resource_type::iron_ore, resource_type::copper_ore,
                    resource_type::rare_earth_ore, resource_type::iron_nickel_ore,
                    resource_type::platinum_group_metals };
                static constexpr resource_type k_carbon[] = {
                    resource_type::coal, resource_type::petroleum, resource_type::peat,
                    resource_type::timber, resource_type::agricultural_produce };
                static constexpr resource_type k_bulk[] = {
                    resource_type::stone, resource_type::silica, resource_type::sand,
                    resource_type::clay, resource_type::regolith, resource_type::water };

                const auto group = [&](const char* id, const char* title, float h, int span,
                                       const resource_type* list, std::size_t count) {
                    const std::size_t n = resource_bars(hs, list, count);
                    if (n == 0)
                        return;
                    const float c = ui::charts::tight_ceil(std::max(bars_peak(n), 1.0f));
                    chart_row(id, title, h, span, [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                        ui::charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
                    });
                };
                group("##c_metals", "Metals (homeworld endowment, 1.0 = Earth-typical)",
                      120.0f, 1, k_metals, sizeof k_metals / sizeof k_metals[0]);
                group("##c_carbon", "Carbon and living resources",
                      120.0f, 1, k_carbon, sizeof k_carbon / sizeof k_carbon[0]);
                // C -> D: what this biosphere evolved that grows nowhere else
                // (BL-191). These do not live in `endowment` like the industrial
                // raws — an endemic good has no global abundance to scale, only an
                // origin — so they are charted from the endemic set itself.
                if (!hs.endemics.empty())
                {
                    std::size_t n = 0;
                    for (const endemic_good& e : hs.endemics)
                    {
                        if (n >= 8) break;
                        const ui::resource_presentation& rp = ui::presentation_of(e.good);
                        bars[n].value  = e.richness;
                        bars[n].colour = rp.colour;
                        bars[n].label  = rp.name;
                        bars[n].hollow = false;
                        ++n;
                    }
                    const float c = ui::charts::tight_ceil(std::max(bars_peak(n), 1.0f));
                    chart_row("##c_endemic",
                              "Endemic trade goods - worth more the further you carry them",
                              130.0f, 2, [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                        ui::charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
                    });

                    // Where each one grows. The band and region ARE the value, so
                    // they are stated rather than left implicit in the bar.
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 128, 145, 255));
                    for (const endemic_good& e : hs.endemics)
                    {
                        const float mid = (e.lat_lo + e.lat_hi) * 0.5f;
                        const char* band = mid < 0.25f ? "tropical"
                                         : mid < 0.45f ? "subtropical"
                                         : mid < 0.60f ? "temperate"
                                         : "polar";
                        ImGui::Text("%s grows only in one %s region - %.0f%% of the way round the globe.",
                                    ui::presentation_of(e.good).name, band,
                                    static_cast<double>(e.sector_centre * 100.0f));
                    }
                    ImGui::PopStyleColor();
                }

                group("##c_bulk",   "Sedimentary and bulk",
                      130.0f, 2, k_bulk,   sizeof k_bulk   / sizeof k_bulk[0]);



                // What each body has and lacks, in one line apiece — the reason to go
                // anywhere other than home, stated before the campaign starts.
                chart_break();
                ImGui::Spacing();
                static constexpr resource_type k_core[] = {
                    resource_type::iron_ore, resource_type::coal, resource_type::petroleum,
                    resource_type::copper_ore, resource_type::water,
                    resource_type::agricultural_produce, resource_type::timber,
                    resource_type::platinum_group_metals };
                for (int i = 0; i < n_bodies; ++i)
                {
                    ImGui::PushID(i);
                    const planetology_state& st = m_wiz_preview[static_cast<std::size_t>(i)];

                    ImGui::PushStyleColor(ImGuiCol_Text, body_colour(i));
                    ImGui::TextUnformatted(prototype_body(i).name);
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Text, col_bright);
                    ImGui::TextUnformatted(archetype_name(st.archetype));
                    ImGui::PopStyleColor();

                    std::string rich, lacks;
                    for (const resource_type rt : k_core)
                    {
                        const float v = endow(st, rt);
                        const char* nm = ui::presentation_of(rt).name;
                        if (v >= 1.3f)      { if (!rich.empty())  rich  += ", "; rich  += nm; }
                        else if (v <= 0.0f) { if (!lacks.empty()) lacks += ", "; lacks += nm; }
                    }
                    std::string strip;
                    if (!rich.empty())  strip  = "Rich: " + rich;
                    if (!lacks.empty()) strip += (strip.empty() ? "" : "   ") + std::string("Lacks: ") + lacks;
                    if (strip.empty())  strip  = "Nothing exceptional either way.";
                    dim_para(strip.c_str());

                    ImGui::PopID();
                }
                break;
            }

            case chain_stage::spend:
            {
                // Before / after, clustered in pairs: the hollow column is the endowment
                // the chain formed, the filled one is what is left after a prior
                // industrial era took the cheap half of it.
                static constexpr resource_type k_spend[] = {
                    resource_type::coal, resource_type::petroleum,
                    resource_type::iron_ore, resource_type::copper_ore };
                ui::charts::bar sb[8];
                char            sl[8][32];
                std::size_t     sn   = 0;
                float           peak = 0.0f;
                for (const resource_type rt : k_spend)
                {
                    const ui::resource_presentation& rp = ui::presentation_of(rt);
                    const float before = endow(hu, rt);
                    const float after  = endow(hs, rt);
                    std::snprintf(sl[sn], sizeof sl[sn], "%s formed", rp.abbrev);
                    sb[sn].value = before; sb[sn].colour = rp.colour;
                    sb[sn].label = sl[sn]; sb[sn].hollow = true;  ++sn;
                    std::snprintf(sl[sn], sizeof sl[sn], "%s left", rp.abbrev);
                    sb[sn].value = after;  sb[sn].colour = rp.colour;
                    sb[sn].label = sl[sn]; sb[sn].hollow = false; ++sn;
                    peak = std::max(peak, before);
                }
                const float c = ui::charts::tight_ceil(std::max(peak, 1.0f));
                chart_row("##c_spend", "Formed against left (homeworld endowment)", 175.0f, 2,
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    ui::charts::draw_bars(dl, mn, mx, sb, sn, c, "%.2f");
                });
                dim_para("The hollow column is what the chain formed; the filled one is what "
                         "is left. Nothing was destroyed - the accessible half was already "
                         "mined, which is why the next tonne has to come from somewhere else.");
                break;
            }

            default:
                break;
        }

        chart_break();
    };

    // A wide centred surface — this is the first thing a player sees, so it takes
    // the screen rather than the menu's 280px column. Same borderless,
    // background-less idiom as draw_main_menu; the render clear colour is the backdrop.
    const float panel_w = std::min(disp.x - 96.0f, 1180.0f);
    const float panel_h = std::max(420.0f, disp.y - 96.0f);
    ImGui::SetNextWindowPos({disp.x * 0.5f, disp.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({panel_w, panel_h}, ImGuiCond_Always);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("##generation", nullptr, flags))
    {
        char buf[256];

        // ── (a) Header: the round name large, what it settles beneath, progress right ──
        {
            ImDrawList*  dl    = ImGui::GetWindowDrawList();
            const float  big   = ImGui::GetFontSize() * 1.7f;
            const ImVec2 p     = ImGui::GetCursorScreenPos();
            const float  avail = ImGui::GetContentRegionAvail().x;

            // The atlas carries a single size, so the title is scaled through the
            // draw list rather than by swapping fonts (there is no second font).
            dl->AddText(ImGui::GetFont(), big, p, col_bright, wr.name);

            std::snprintf(buf, sizeof buf, "Round %d of %d", m_wiz_round + 1, wizard_round_count);
            const ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText({p.x + avail - ts.x, p.y + (big - ts.y) * 0.5f}, col_dim, buf);

            ImGui::Dummy({avail, big + 2.0f});
        }
        dim_text(wr.question);

        // Three pips, the current one lit: past rounds filled dim, future ones hollow.
        {
            ImDrawList*  dl = ImGui::GetWindowDrawList();
            const ImVec2 p  = ImGui::GetCursorScreenPos();
            constexpr float pip = 10.0f, gap = 6.0f;
            for (int i = 0; i < wizard_round_count; ++i)
            {
                const ImVec2 a{p.x + static_cast<float>(i) * (pip + gap), p.y + 4.0f};
                const ImVec2 b{a.x + pip, a.y + pip};
                if (i == m_wiz_round)     dl->AddRectFilled(a, b, col_bright);
                else if (i < m_wiz_round) dl->AddRectFilled(a, b, col_dim);
                else                      dl->AddRect(a, b, col_dim);
            }
            ImGui::Dummy({static_cast<float>(wizard_round_count) * (pip + gap), pip + 8.0f});
        }
        ImGui::Separator();

        // ── (b) The round's stages, charted. Sized to leave the preference block and
        //    the footer pinned below, so the controls never scroll away from the
        //    charts they act on. ──
        const float frame_h  = ImGui::GetFrameHeight();
        const float line_h   = ImGui::GetTextLineHeightWithSpacing();
        const float decide_h = static_cast<float>(round_pref_count(m_wiz_round))
                                   * (frame_h + style.ItemSpacing.y)
                             + line_h * static_cast<float>(round_note_lines(m_wiz_round)
                                                           + (m_wiz_resolved.gave_up ? 2 : 0))
                             + style.ItemSpacing.y * 3.0f;
        const float footer_h = 34.0f + style.ItemSpacing.y * 2.0f;
        ImGui::BeginChild("##wiz_charts", {0.0f, -(decide_h + footer_h)}, false,
                          ImGuiWindowFlags_NoBackground);

        // Column metric for the chart cells, measured once the region (and its
        // scrollbar, if any) is known. A narrow display falls back to one column.
        {
            const float avail = ImGui::GetContentRegionAvail().x;
            cols  = (avail >= 780.0f) ? 2 : 1;
            col_w = std::floor((avail - style.ItemSpacing.x * static_cast<float>(cols - 1))
                               / static_cast<float>(cols)) - 1.0f;
        }

        for (int s = static_cast<int>(wr.first); s <= static_cast<int>(wr.last); ++s)
            draw_stage(static_cast<chain_stage>(s));

        ImGui::EndChild();

        // ── (c) The round's preferences. Named leans and nothing else: no value from
        //    the resolved params is printed or editable here, because the charts above
        //    already show what the roll produced and that is the honest feedback. ──
        world_preferences& pf = m_pending_world_params.preferences;
        ImGui::Separator();

        switch (m_wiz_round)
        {
            case 0:
                if (lean_row("star", "Star", pf.star,
                             "Dimmer", "Sun-like", "Brighter"))                      m_wiz_dirty = true;
                if (lean_row("size", "World", pf.world_size,
                             "Small", "Earth-like", "Large"))                        m_wiz_dirty = true;
                if (lean_row("interior", "Interior", pf.interior,
                             "Old and cold", "Moderate", "Young and vigorous"))      m_wiz_dirty = true;
                if (lean_row("metal", "Metal", pf.metal,
                             "Metal-poor", "Normal", "Metal-rich"))                  m_wiz_dirty = true;
                break;

            case 1:
                if (lean_row("ocean", "Ocean", pf.ocean,
                             "Continental", "Balanced", "Oceanic"))                  m_wiz_dirty = true;
                if (lean_row("oxygen", "Oxygen", pf.oxygen_story,
                             "Oxygenated early", "Balanced", "Oxygenated late"))     m_wiz_dirty = true;
                // The one trade worth spelling out: a single choice moves two resources
                // in opposite directions, with every gate still passed either way.
                dim_text("Oxygenated early -> coal-rich and iron-lean; oxygenated late -> "
                         "iron-rich and coal-lean.");
                if (lean_row("coal", "Coal basins", pf.coal_basins,
                             "Seasonal", "Mixed", "Everwet"))                        m_wiz_dirty = true;
                break;

            case 2:
                if (lean_row("drawdown", "Drawdown", pf.drawdown,
                             "Barely touched", "Worked", "Stripped"))                m_wiz_dirty = true;
                break;

            default:
                break;
        }

        // The reroll cost, told rather than hidden. Resolution rejects and re-draws
        // until the homeworld clears the strict Earth-like floor; how many draws that
        // took is a true thing about the preferences just set, and a narrow set of
        // leans is meant to feel like one.
        if (m_wiz_resolved.gave_up)
        {
            dim_text("These preferences have almost no viable region - no draw cleared the "
                     "Earth-like floor, so this is the closest world found. Loosen one of "
                     "them, or reroll.");
        }
        else if (m_wiz_resolved.attempts > 1)
        {
            std::snprintf(buf, sizeof buf, "Found on attempt %u.",
                          static_cast<unsigned>(m_wiz_resolved.attempts));
            dim_text(buf);
        }

        // ── (d) Navigation. Reroll re-draws THIS round from a fresh number (and
        //    everything downstream of it, because the chain is causal); Back is a plain
        //    revision, since a round's leans feed its own gates and the ones after
        //    them, never a chart the player has already been shown. ──
        const ImVec2 btn  = {150.0f, 34.0f};
        const bool   last = (m_wiz_round == wizard_round_count - 1);

        // Back always steps out one level, and the level outside round 0 is the main
        // menu — a wizard the player cannot leave is a trap (nothing is generated
        // until "Begin", so leaving costs nothing). Preferences survive the trip, so
        // re-entering resumes the same leans from round 0.
        if (ImGui::Button("Back##wizback", btn))
        {
            if (m_wiz_round == 0)
                m_screen = app_screen::menu;
            else
                --m_wiz_round;
        }

        ImGui::SameLine();
        if (ImGui::Button("Reroll##wizroll", btn))
        {
            ++pf.roll[m_wiz_round];
            m_wiz_dirty = true;
        }

        ImGui::SameLine(panel_w - style.WindowPadding.x - btn.x);
        if (ImGui::Button(last ? "Begin##wizgo" : "Continue##wizgo", btn))
        {
            if (last)
                start_new_game(); // the one and only generation call
            else
                ++m_wiz_round;
        }
    }
    ImGui::End();
}

void app::render()
{
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

        ImGui::Render();
        SDL_SetRenderDrawColor(m_renderer, 15, 15, 20, 255);
        SDL_RenderClear(m_renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_renderer);
        if (m_capture_requested)
        {
            save_screenshot();
            m_capture_requested = false;
        }
        SDL_RenderPresent(m_renderer);
        return;
    }

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
    constexpr float margin = 8.0f;
    const float  mm_w   = std::max(336.0f, 0.28f * std::min(disp.x, disp.y)); // ~1.4x the old 240/0.20 band — legible on larger displays
    const float  mm_h   = mm_w * 0.75f; // keep the 4:3 ratio of the 240x180 default
    const ImVec2 mm_origin = {disp.x - margin - mm_w, disp.y - margin - mm_h};

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
                // No rung above solar — the minimap is game branding (game name).
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
                ui::draw_body_surface_canvas(m_world, m_ui, m_registry, m_last_econ_report, {0.0f, 0.0f}, disp, primary_input,
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

        // At the top rung the inset has no canvas — fill it as a dark branding
        // placeholder so the full-window solar canvas does not show through.
        if (m_ui.primary_level == canvas_level::solar)
            bdl->AddRectFilled(inset_origin,
                               {inset_origin.x + inset_size.x, inset_origin.y + inset_size.y},
                               IM_COL32(8, 10, 20, 255));

        // Title bar.
        bdl->AddRectFilled(mm_origin, {mm_origin.x + mm_w, mm_origin.y + title_h}, IM_COL32(28, 30, 40, 255));
        bdl->AddText({mm_origin.x + 5.0f, mm_origin.y + 3.0f}, IM_COL32(220, 225, 235, 255), mm_title);

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
    // proportions revised on Ben's 2026-07-10 review). Four left-aligned stacked
    // rows: the year and the date/quarter line at the SAME size (the year is no
    // longer an oversized centred heading), then a full-width quarter-progress bar
    // aligned with the full-width speed-control row directly below it. The panel
    // takes input (the speed buttons), so it is not flagged NoInputs.
    const float tick_w = mm_w;
    // Height is content-derived (BL-097), not a fraction of the minimap's
    // resolution-scaled mm_h (the BL-093 anti-pattern): year + date lines, the thin
    // progress bar, and the speed-button row, plus the inter-row spacing.
    const float time_line_h    = ImGui::GetTextLineHeightWithSpacing();
    const float time_prog_h    = 10.0f; // thin quarter-progress bar
    const float time_spacing   = ImGui::GetStyle().ItemSpacing.y;
    // Year + date now share one row (Ben's 2026-07-15 review), so the reclaimed
    // line height goes to a taller speed-button row rather than shrinking the panel.
    const float time_btn_h     = ImGui::GetFrameHeight() * 2.0f;
    const float time_content_h = time_line_h + time_prog_h + time_btn_h + time_spacing * 2.0f;
    const float time_h         = time_content_h + ImGui::GetStyle().WindowPadding.y * 2.0f;
    {
        ImGui::SetNextWindowPos({disp.x - margin - tick_w, margin});
        ImGui::SetNextWindowSize({tick_w, time_h});
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

        // --- Year + date/quarter on ONE row (Ben's 2026-07-15 review), left-aligned
        // at the base font size. Sharing a horizontal level frees vertical room for a
        // taller speed-control row below. Quarter is bracketed [Qn].
        ImGui::Text("%s   %s %s [Q%d]", std::to_string(date.year).c_str(),
                    ui::fmt::month_abbrev(date.month),
                    ui::fmt::ordinal_day(date.day).c_str(), date.quarter);

        // --- Quarter-progress bar: full width, so it aligns with the speed-control
        // row directly below it (the economy resolves on the quarter boundary).
        ImGui::ProgressBar(ui::fmt::quarter_progress(day),
                           {ImGui::GetContentRegionAvail().x, time_prog_h}, "");

        // --- Speed controls: a full-width row of pause + speed-tier buttons, aligned
        // with the progress bar above. The active speed is highlighted. When running,
        // the pause slot is a blank button carrying a filled square glyph (drawn
        // below); when paused it flips to a play ">" so it reflects the toggle state.
        // Speed tiers use Roman numerals (I–V); the square avoids "||" reading as II.
        {
            const char* labels[] = {m_sim_loop.paused() ? ">" : "##pause", "I", "II", "III", "IV", "V"};
            const int   speeds[] = { 0,    1,   2,   3,   4,   5 };
            const int   n        = 6;
            const float spacing  = ImGui::GetStyle().ItemSpacing.x;
            const float bw       = (ImGui::GetContentRegionAvail().x - spacing * (n - 1)) / n;

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
                if (i + 1 < n)
                    ImGui::SameLine();
            }
        }

        ImGui::End();
    }

    // In-app system menu (BL-070): a corner gear button opening a small popup with
    // session controls — Pause/Resume (mirroring the Space hotkey via the shared
    // pause_toggle path) and Exit Game (inline "Really quit?" confirm, since there
    // is no save). Sits at the top-right, just left of the time column; Esc toggles
    // the same popup (handle_key_down). See docs/ui/MENU.md.
    {
        constexpr float gear = 26.0f;
        const ImVec2 gear_pos{ (disp.x - margin - mm_w) - margin - gear, margin };

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
        const float header_right = (disp.x - margin - tick_w) - margin;
        ui::draw_header_panel(m_world, m_balance_history, header_left, header_right);
    }

    // Comms chat log (BL-205, replaces the Explorer placeholder) — right edge,
    // between the time panel and the minimap.
    {
        const float column_bottom = margin + time_h;
        const float exp_x         = mm_origin.x;
        const float exp_y         = column_bottom + margin;
        const float exp_w         = mm_w;
        const float exp_h         = (mm_origin.y - margin) - exp_y;
        ui::draw_chat_panel(m_world, m_chat, static_cast<int>(m_sim_loop.day_tick()),
                            exp_x, exp_y, exp_w, exp_h);
    }

    // Left navigation pane and the menus it opens. Starts below the profile.
    ui::draw_nav_pane(m_ui, ui::profile_panel_height);

    // A *new* entity selection closes any open fold-out ledger so the selection takes
    // the column (SELECTION.md). Run BEFORE the ledgers draw this frame — otherwise a
    // selection made while a ledger is open double-draws both (ledger, then selection
    // after the close) for one frame, a visible ghost.
    if (m_ui.selected_entity != m_prev_selection)
    {
        if (m_ui.selected_entity != null_entity &&
            m_ui.selected_entity != m_ui.selection_hidden_for)
            ui::close_all_panels(m_ui); // new selection takes the column
        m_ui.card_stack.clear();        // a new selection resets any drill-down (BL-196)
        m_ui.card_resource_page = 0;    // ...and its resource accordion page
        m_prev_selection = m_ui.selected_entity;
    }

    ui::draw_tile_inspector(m_world, m_ui, &m_ui.show_tile_ledger);
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
    ui::draw_corporation_panel(m_world, m_ui, m_ui.show_corporation_panel);

    // Sticky detail card (BL-194/195) — click-opened, canvas-confined; the SOLE
    // home of the Selection content now (the former fold-out Selection panel is
    // gone; the shell column is ledgers-only). Coexists with the transient hover
    // card. Drawn after the other chrome so it z-orders on top of it. Confined to
    // the free canvas rect — clear of the shell column (left), the header (top),
    // and the right chrome column (time panel / minimap) — so it never overlaps
    // chrome (SELECTION.md § Placement).
    {
        const float  col_w        = ui::shell_column_width(disp.x);
        const float  right_edge   = disp.x - margin - mm_w; // left edge of the right chrome column
        const ImVec2 card_origin  = { col_w, ui::profile_panel_height };
        const ImVec2 card_region  = { std::max(0.0f, right_edge - col_w),
                                      std::max(0.0f, disp.y - ui::profile_panel_height) };
        const ui::resource_history_view rhist{ &m_body_resource_hist,
                                               &m_tile_resource_hist,
                                               &m_resource_hist_days };
        ui::draw_selection_card(m_world, m_registry, m_last_econ_report, rhist, m_ui,
                                card_origin, card_region);
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
            m_ui.construction.pending_target, built);
        switch (r)
        {
            case construction_result::placed:
                // BL-095: placement now only *starts* a durative, material-gated build
                // (ticks_remaining > 0), so the toast reflects that rather than claiming
                // it is done — the Selection card carries the live rate / ETA / paused
                // status from here on.
                m_ui.construction.last_message    = "Construction started.";
                m_ui.selected_entity              = built;        // inspect the new building
                m_ui.selection_hidden_for         = null_entity;  // re-show the panel
                break;
            case construction_result::invalid_tile:
                m_ui.construction.last_message = "Can't build there."; break;
            case construction_result::insufficient_funds:
                m_ui.construction.last_message = "Can't afford it."; break;
            case construction_result::slot_occupied:
                m_ui.construction.last_message = "Already placed on this body."; break;
            case construction_result::insufficient_materials:
                m_ui.construction.last_message = "Not enough materials."; break;
            default:
                m_ui.construction.last_message = "Construction failed."; break;
        }
        m_ui.construction.pending_tile = null_entity; // consume the request
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
                // F12 stays outside the table (it needs the renderer)
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

    // F9 mock tech-tree viewer (BL-087). Read-only design aid over
    // scripts/tech_tree.lua; no simulation coupling.
    ui::draw_tech_tree_panel(m_tech_tree, m_show_tech_tree);

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

    // Clamp to a sane floor so a corrupt file can't produce an unusable window.
    m_settings.window_w = std::max(640, m_settings.window_w);
    m_settings.window_h = std::max(480, m_settings.window_h);
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

void app::save_screenshot()
{
    SDL_Surface* surface = SDL_RenderReadPixels(m_renderer, nullptr);
    if (!surface)
    {
        SDL_Log("Screenshot failed: %s", SDL_GetError());
        m_capture_name.clear();
        return;
    }

    // Normalise to a known RGBA byte layout so the PNG writer can consume the
    // pixels directly regardless of the renderer's native backbuffer format.
    SDL_Surface* rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surface);
    if (!rgba)
    {
        SDL_Log("Screenshot convert failed: %s", SDL_GetError());
        m_capture_name.clear();
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories("screenshots", ec);

    char path[256];
    if (!m_capture_name.empty())
        std::snprintf(path, sizeof(path), "screenshots/%s.png", m_capture_name.c_str());
    else
        std::snprintf(path, sizeof(path), "screenshots/io_%llu.png",
            static_cast<unsigned long long>(SDL_GetTicks()));

    if (write_png_rgba(path, rgba->w, rgba->h,
                       static_cast<const unsigned char*>(rgba->pixels), rgba->pitch))
        SDL_Log("Screenshot saved: %s", path);
    else
        SDL_Log("Screenshot save failed: %s", path);

    // Golden-image diffing (run_verify only — m_golden_dir is empty for an
    // interactive F12 capture). Bless overwrites the golden; otherwise, if a
    // golden exists, compare against it and emit an advisory PASS/FAIL + a diff
    // image. No golden present = capture-only, today's behaviour.
    if (!m_golden_dir.empty() && !m_capture_name.empty())
        compare_to_golden(m_capture_name, rgba);

    SDL_DestroySurface(rgba);
    m_capture_name.clear();
}

void app::compare_to_golden(const std::string& name, SDL_Surface* rgba)
{
    // Tolerance knobs (see DEVELOPMENT_PRACTICES.md § Visual verification): a pixel differs when its max R/G/B
    // channel delta exceeds T; the capture fails when the differing fraction
    // exceeds F. Per-check overrides via the Lua API are a future follow-on.
    constexpr int   k_pixel_threshold    = 8;      // T (0..255)
    constexpr float k_fail_fraction      = 0.005f; // F (0.5 %)

    const std::filesystem::path golden = std::filesystem::path{m_golden_dir} / (name + ".png");

    // Pack the captured surface into a contiguous stride = width*4 buffer (the
    // backbuffer pitch may carry row padding the diff/writer do not expect).
    const int w = rgba->w;
    const int h = rgba->h;
    std::vector<unsigned char> packed(static_cast<std::size_t>(w) * h * 4);
    const auto* src = static_cast<const unsigned char*>(rgba->pixels);
    for (int y = 0; y < h; ++y)
        std::memcpy(packed.data() + static_cast<std::size_t>(y) * w * 4,
                    src + static_cast<std::size_t>(y) * rgba->pitch,
                    static_cast<std::size_t>(w) * 4);

    if (m_verify_bless)
    {
        std::error_code ec;
        std::filesystem::create_directories(m_golden_dir, ec);
        if (write_png_rgba(golden.string(), w, h, packed.data(), w * 4))
            SDL_Log("Golden blessed: %s", golden.string().c_str());
        else
            SDL_Log("Golden bless failed: %s", golden.string().c_str());
        return;
    }

    std::vector<unsigned char> ref;
    int rw = 0, rh = 0;
    if (!read_png_rgba(golden.string(), ref, rw, rh))
    {
        SDL_Log("Golden compare: no golden for '%s' (capture-only)", name.c_str());
        return;
    }
    if (rw != w || rh != h)
    {
        SDL_Log("Golden FAIL %s: size mismatch (capture %dx%d vs golden %dx%d)",
                name.c_str(), w, h, rw, rh);
        ++m_verify_failures;
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories("screenshots/diff", ec);
    const std::string diff_path = "screenshots/diff/" + name + ".png";
    const float frac = diff_rgba(packed.data(), ref.data(), w, h, k_pixel_threshold, diff_path);

    if (frac > k_fail_fraction)
    {
        SDL_Log("Golden FAIL %s: %.4f%% differing (> %.4f%%); diff: %s",
                name.c_str(), frac * 100.0f, k_fail_fraction * 100.0f, diff_path.c_str());
        ++m_verify_failures;
    }
    else
    {
        SDL_Log("Golden PASS %s: %.4f%% differing (<= %.4f%%)",
                name.c_str(), frac * 100.0f, k_fail_fraction * 100.0f);
    }
}
