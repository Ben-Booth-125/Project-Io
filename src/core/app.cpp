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
#include "ui/nav_pane.hpp"
#include "ui/overlay.hpp"
#include "ui/presentation.hpp"
#include "ui/profile_panel.hpp"
#include "ui/selection_panel.hpp"
#include "ui/solar_system_canvas.hpp"
#include "ui/tile_inspector.hpp"
#include "ui/view_nav.hpp"
#include "world/budget_system.hpp"
#include "world/construction.hpp"
#include "world/hard_coded_world.hpp"
#include "world/market_clearing.hpp"
#include "world/orbital_system.hpp"
#include "world/supply_system.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

static constexpr int window_w = 1280;
static constexpr int window_h = 720;

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

    setup_world();
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
        m_balance_history.push_back(pit != m_world.corporations.end() ? pit->second.balance : 0.0f);
    }
    for (int t = 0; t < pre_game_ticks; ++t)
        step_economy();

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

        // Resolve the economy on each econ-tick (quarter) boundary the clock crosses.
        const uint64_t econ = m_sim_loop.econ_tick();
        while (m_last_econ_tick < econ)
        {
            step_economy();
            ++m_last_econ_tick;
        }

        render();
    }
    return 0;
}

void app::load_economy()
{
    // Load the Lua data layer, then build the registry from it (protected calls).
    m_lua.load("scripts/recipes.lua");
    m_lua.load("scripts/economy.lua");
    m_registry.load_from_lua(m_lua);

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
    apply_budget(m_world, m_registry, flows, m_last_econ_report.workforce_contention);
    credit_arrived_convoys(m_world);

    // Record the player's post-tick balance for the header net figure + sparkline.
    // Capped so the buffer stays small; the sparkline shows the most recent window.
    {
        const auto cit = m_world.corporations.find(m_world.player_entity);
        m_balance_history.push_back(cit != m_world.corporations.end() ? cit->second.balance : 0.0f);
        constexpr std::size_t max_history = 64;
        if (m_balance_history.size() > max_history)
            m_balance_history.erase(m_balance_history.begin());
    }
}

void app::setup_world()
{
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

    // Open with the Corporation lens active (BL-013): the player reads their own
    // footprint first. Single-select with a null state — re-clicking the active
    // lens clears to overlay_mode::none. See LENSES.md, docs/ui § lens strip.
    m_ui.overlay = overlay_mode::corporation;
}

int app::run_verify(const std::string& script_path, bool bless)
{
    // Deterministic, non-interactive setup: fixed window (the window_w/window_h
    // constants), seeded world, sim left paused so orbits and ticks never advance
    // between captures. The script drives view/overlay state directly.
    setup_world();
    load_economy();
    m_sim_loop.set_speed(0);

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
    v.set_function("show_panel", [this](const std::string& name, bool open) {
        if (name == "economy")           m_ui.show_economy_panel = open;
        else if (name == "construction") m_ui.show_construction_panel = open;
        else if (name == "tile")         m_ui.show_tile_ledger = open;
        else if (name == "market")       m_ui.show_market_ledger = open;
        else if (name == "balance")      m_ui.show_balance_ledger = open;
        else if (name == "corporation")  m_ui.show_corporation_panel = open;
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
        if (event.type == SDL_EVENT_QUIT)
            running = false;
        else if (event.type == SDL_EVENT_KEY_DOWN)
            handle_key_down(event.key);
    }
}

void app::handle_key_down(const SDL_KeyboardEvent& key)
{
    // F12 captures regardless of focus — the screenshot is an app concern (it
    // needs the renderer), not a canvas_command.
    if (key.scancode == SDL_SCANCODE_F12)
    {
        m_capture_requested = true;
        return;
    }

    // Canvas navigation keys are suppressed while ImGui owns the keyboard (a text
    // field has focus), so typing into a panel never steers the canvas.
    if (ImGui::GetIO().WantCaptureKeyboard)
        return;

    // Map the keybinding table (CANVASES.md § Keyboard) onto the shared command
    // vocabulary; apply_canvas_command is the same dispatch verify.command uses.
    const bool shift = (key.mod & SDL_KMOD_SHIFT) != 0;
    auto dispatch = [this](ui::canvas_command cmd) {
        ui::apply_canvas_command(m_world, m_ui, cmd);
    };

    switch (key.scancode)
    {
        case SDL_SCANCODE_RETURN:       dispatch(ui::canvas_command::descend);   break;
        case SDL_SCANCODE_BACKSPACE:    dispatch(ui::canvas_command::ascend);    break;
        case SDL_SCANCODE_RIGHTBRACKET: dispatch(ui::canvas_command::body_next); break;
        case SDL_SCANCODE_LEFTBRACKET:  dispatch(ui::canvas_command::body_prev); break;
        case SDL_SCANCODE_LEFT:         dispatch(ui::canvas_command::pan_left);  break;
        case SDL_SCANCODE_RIGHT:        dispatch(ui::canvas_command::pan_right); break;
        case SDL_SCANCODE_UP:           dispatch(ui::canvas_command::pan_up);    break;
        case SDL_SCANCODE_DOWN:         dispatch(ui::canvas_command::pan_down);  break;
        case SDL_SCANCODE_EQUALS:       dispatch(ui::canvas_command::zoom_in);   break;
        case SDL_SCANCODE_MINUS:        dispatch(ui::canvas_command::zoom_out);  break;
        case SDL_SCANCODE_L:
            dispatch(shift ? ui::canvas_command::lens_prev : ui::canvas_command::lens_next);
            break;
        case SDL_SCANCODE_0:            dispatch(ui::canvas_command::lens_clear); break;
        default: break;
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
        // Minimap chrome reserves a title bar above the inset; the neighbouring
        // canvas occupies the rest. The overlay-lens controls used to sit in a
        // mode bar below the inset — they now live in a bottom-left strip
        // (ui::draw_overlay_controls), so the inset takes the full height here.
        const float  title_h      = ImGui::GetTextLineHeight() + 6.0f;
        const ImVec2 inset_origin = { mm_origin.x, mm_origin.y + title_h };
        const ImVec2 inset_size   = { mm_w, mm_h - title_h };

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
                ui::draw_body_surface_canvas(m_world, m_ui, m_registry, m_last_econ_report, {0.0f, 0.0f}, disp, primary_input);
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

        // Border around the whole minimap box.
        bdl->AddRect(mm_origin, {mm_origin.x + mm_w, mm_origin.y + mm_h}, IM_COL32(90, 95, 110, 255));
    }

    // Time panel — top-right, same width as the minimap. Two columns: a compact
    // date/quarter block (left, 25%) and the speed controls (right, 75%). The
    // panel takes input (the speed buttons), so it is not flagged NoInputs.
    const float tick_w = mm_w;
    const float time_h = mm_h * 0.5f;
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

        // 25% / 75% split via a two-column stretch table.
        if (ImGui::BeginTable("##time_cols", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("##date", ImGuiTableColumnFlags_WidthStretch, 0.25f);
            ImGui::TableSetupColumn("##ctrl", ImGuiTableColumnFlags_WidthStretch, 0.75f);
            ImGui::TableNextRow();

            // --- Left: a date/quarter block in three rows. The progress bar
            // shows how far through the current quarter the campaign is (the
            // economy resolves on the quarter boundary).
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d Q%d", date.year, date.quarter);
            ImGui::Text("%s %02d", ui::fmt::month_abbrev(date.month), date.day);
            ImGui::ProgressBar(ui::fmt::quarter_progress(day), {-1.0f, 0.0f}, "");

            // --- Right: the compressed speed controls.
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("Sim %llu", m_sim_loop.sim_tick());
            ImGui::SameLine();
            if (m_sim_loop.paused())
                ImGui::TextDisabled("(paused)");
            else
            {
                static constexpr const char* mult_labels[] = {"", "1/4x", "1/2x", "1x", "4x", "16x"};
                const int s = m_sim_loop.speed();
                ImGui::TextDisabled("(%s)", (s >= 1 && s <= sim_loop::max_speed) ? mult_labels[s] : "?");
            }

            // Pause plus speed buttons. The active speed is highlighted. The pause
            // label flips to a play symbol when paused so it reflects the toggle state.
            const char* labels[] = {m_sim_loop.paused() ? ">" : "II", "1/4x", "1/2x", "1x", "4x", "16x"};
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

            ImGui::EndTable();
        }

        ImGui::End();
    }

    // Corporation profile — top-left corner, above the navigation pane.
    ui::draw_profile_panel();

    // Budget + resource header — spans the top between the profile and the
    // time column, clear of both. Starts at the profile's right edge (the profile
    // keeps its own width, wider than the narrow icon nav rail below it).
    {
        const float header_left  = ui::profile_panel_width;
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
    ui::draw_economy_panel(m_world, m_registry, m_last_econ_report, &m_ui.show_economy_panel);
    ui::draw_construction_panel(m_world, m_registry, m_ui, &m_ui.show_construction_panel);
    ui::draw_market_ledger(m_world, m_ui, m_ui.show_market_ledger);
    ui::draw_balance_ledger(m_world, m_ui, m_ui.show_balance_ledger);
    ui::draw_corporation_panel(m_world, m_ui, m_ui.show_corporation_panel);

    // Overlay-lens controls — a bottom-left strip from the nav-pane edge inward,
    // clear of the centred scale/zoom control. Replaces the old minimap mode bar.
    ui::draw_overlay_controls(m_ui, ui::nav_pane_width, disp.y - margin);

    // Selection info element — pinned bottom-left, stacked directly above the
    // overlay strip. Hidden until the player selects an entity. See SELECTION.md.
    constexpr float overlay_strip_h = 40.0f; // approx height of the lens strip below
    ui::draw_selection_panel(m_world, m_registry, m_ui, ui::nav_pane_width,
                             disp.y - margin - overlay_strip_h);

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
                m_ui.construction.last_message    = "Built.";
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
    // Tolerance knobs (OPENS § Canvas [F3]): a pixel differs when its max R/G/B
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
