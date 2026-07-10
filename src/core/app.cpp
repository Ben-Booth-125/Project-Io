#include "app.hpp"

#include "png_writer.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>

#include "ui/body_surface_canvas.hpp"
#include "ui/canvas_command.hpp"
#include "ui/circumplanetary_canvas.hpp"
#include "ui/construction_panel.hpp"
#include "ui/balance_ledger.hpp"
#include "ui/corporation_panel.hpp"
#include "ui/economy_panel.hpp"
#include "ui/market_ledger.hpp"
#include "ui/explorer_panel.hpp"
#include "ui/fonts.hpp"
#include "ui/format.hpp"
#include "ui/header_panel.hpp"
#include "ui/foldout_column.hpp" // shell_column_width — permanent left shell column (BL-122)
#include "ui/nav_pane.hpp"
#include "ui/overlay.hpp"
#include "ui/presentation.hpp"
#include "ui/profile_panel.hpp"
#include "ui/selection_panel.hpp"
#include "ui/solar_system_canvas.hpp"
#include "ui/tech_tree_panel.hpp"
#include "ui/tile_inspector.hpp"
#include "ui/view_nav.hpp"
#include "world/budget_system.hpp"
#include "world/construction.hpp"
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

    // Open on the main menu — the deliberate entry point. The world, economy, and
    // sim clock are not built until the player picks "New Game" (start_new_game),
    // so nothing simulates behind the menu and the clock starts when play does.
    m_screen = app_screen::menu;

    bool running = true;
    while (running && !m_quit_requested)
    {
        process_events(running);

        // On the menu, nothing simulates — just pump events and draw the menu.
        if (m_screen == app_screen::menu)
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
}

void app::setup_world(world_params params)
{
    m_active_world_params = params;          // remember the descriptor the live world was built from
    m_world = make_hard_coded_world(params);

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

    // No game bindings while on the main menu — there is no world to navigate.
    if (m_screen == app_screen::menu)
        return;

    // Esc toggles the in-app system menu (BL-070) — cheap keyboard parity with the
    // corner gear button. Handled before the ImGui keyboard guard so it works even
    // while the popup (or another panel) holds focus. An armed exit-confirm backs
    // out first, so Esc cancels the "Really quit?" rather than closing the menu.
    if (key.scancode == SDL_SCANCODE_ESCAPE)
    {
        if (m_ui.confirm_exit_pending)
            m_ui.confirm_exit_pending = false;
        else if (m_ui.show_system_menu)
            m_ui.show_system_menu = false;
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
        //     which start_new_game() consumes; each carries a unique ##id so it never
        //     collides with the centred buttons below. ---
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

        // Nations on the home body (the Voronoi merge target).
        ImGui::SetNextItemWidth(280.0f);
        ImGui::SliderInt("##nations", &wp.nation_count, 6, 30, "Nations: %d");

        // Bodies — the count knob is phased to a later update; shown disabled so the
        // intent reads without implying it works yet.
        ImGui::BeginDisabled();
        int bodies_stub = 5;
        ImGui::SetNextItemWidth(280.0f);
        ImGui::SliderInt("##bodies", &bodies_stub, 5, 5, "Bodies: %d (fixed)");
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("A variable body count is coming in a later update.");

        ImGui::Dummy({0.0f, 12.0f});

        const ImVec2 btn = {280.0f, 40.0f};
        if (ImGui::Button("New Game", btn))
            start_new_game();
        ImGui::Dummy({0.0f, 6.0f});
        if (ImGui::Button("Quit", btn))
            m_quit_requested = true;
    }
    ImGui::End();
}

void app::render()
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Main menu — drawn instead of the canvases when no game is loaded. Shares the
    // Render/clear/capture tail below so the menu is capturable like any frame.
    if (m_screen == app_screen::menu)
    {
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
    const float time_frame_h   = ImGui::GetFrameHeight();
    const float time_prog_h    = 10.0f; // thin quarter-progress bar
    const float time_spacing   = ImGui::GetStyle().ItemSpacing.y;
    const float time_content_h = time_line_h * 2.0f + time_prog_h + time_frame_h + time_spacing * 3.0f;
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

        // --- Year and date/quarter: the SAME size, left-aligned as a stacked pair
        // (Ben's 2026-07-10 review). The year is no longer an oversized centred
        // heading — both read at the base font size, aligned on the panel's left edge.
        ImGui::TextUnformatted(std::to_string(date.year).c_str());
        ImGui::Text("%s %s (Q%d)", ui::fmt::month_abbrev(date.month),
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

    // Explorer — right edge, between the time panel and the minimap.
    {
        const float column_bottom = margin + time_h;
        const float exp_x         = mm_origin.x;
        const float exp_y         = column_bottom + margin;
        const float exp_w         = mm_w;
        const float exp_h         = (mm_origin.y - margin) - exp_y;
        ui::draw_explorer_panel(exp_x, exp_y, exp_w, exp_h);
    }

    // Left navigation pane and the menus it opens. Starts below the profile.
    ui::draw_nav_pane(m_ui, ui::profile_panel_height);
    ui::draw_tile_inspector(m_world, m_ui, &m_ui.show_tile_ledger);
    {
        const ui::player_plot_history phist{m_balance_history, m_income_history, m_expenditure_history};
        ui::draw_economy_panel(m_world, m_registry, m_last_econ_report, phist, m_ui, &m_ui.show_economy_panel);
    }
    // Construction panel — an ordinary fold-out tab in the shell column (BL-122),
    // one of the mutually-exclusive column occupants (ledgers + Selection).
    ui::draw_construction_panel(m_world, m_registry, m_last_econ_report, m_ui, &m_ui.show_construction_panel);
    ui::draw_market_ledger(m_world, m_ui, m_market_history, m_ui.show_market_ledger);
    ui::draw_balance_ledger(m_world, m_last_econ_report, m_balance_history, m_ui.show_balance_ledger);
    ui::draw_corporation_panel(m_world, m_ui, m_ui.show_corporation_panel);

    // Selection info element — now docked in the shell fold-out column, mutually
    // exclusive with the ledgers (SELECTION.md). A *new* entity selection closes any
    // open ledger so the selection takes the column; while a ledger owns the column
    // the Selection is not drawn (the ledger wins the shared slot). Selection state
    // persists behind an open ledger, so closing the ledger reveals it again.
    if (m_ui.selected_entity != m_prev_selection)
    {
        if (m_ui.selected_entity != null_entity &&
            m_ui.selected_entity != m_ui.selection_hidden_for)
            ui::close_all_panels(m_ui); // new selection takes the column
        m_prev_selection = m_ui.selected_entity;
    }
    // The fold-out column, when no nav ledger owns it, shows either the tile
    // construction ledger (BL-162, when the player opened it from a tile) or the
    // Selection element. The build ledger only applies to a selected tile.
    if (!ui::any_panel_open(m_ui))
    {
        const bool sel_is_tile = m_ui.selected_entity != null_entity &&
                                 m_world.tiles.count(m_ui.selected_entity) > 0;
        if (m_ui.show_build_ledger && sel_is_tile)
            ui::draw_construction_ledger(m_world, m_registry, m_ui);
        else
        {
            m_ui.show_build_ledger = false; // not a tile → no build ledger
            ui::draw_selection_panel(m_world, m_registry, m_last_econ_report, m_ui);
        }
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
