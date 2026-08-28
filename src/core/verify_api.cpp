/// @file verify_api.cpp
/// The `verify` Lua API — app::run_verify and its script-facing bindings.
/// Extracted verbatim from app.cpp (BL-361); behaviour and registered function
/// names are unchanged (scripts/verify/*.lua call them by name).

#include <cfloat>
#include <cstdio>
#include <iterator>
#include <chrono>
#include <thread>
#include "app.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "ui/canvas_command.hpp"
#include "ui/detail_level.hpp"
#include "ui/foldout_column.hpp"
#include "ui/fonts.hpp"
#include "ui/frame_stats.hpp"
#include "ui/market_ledger.hpp"
#include "ui/presentation.hpp"
#include "ui/selection.hpp"
#include "ui/text_fit.hpp"
#include "ui/view_nav.hpp"
#include "world/construction.hpp"
#include "world/corporation_generation.hpp"
#include "world/logistics.hpp"
#include "world/placement_rules.hpp"
#include "world/survey_system.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Verify captures render at a FIXED small size, independent of the interactive
// window default above, so goldens stay renderer-/machine-independent and match the
// documented 1280x720 standard even as the interactive default grows. Growing
// window_w/window_h must NOT silently move the golden resolution — that desynced the
// whole golden set in 6a04ec9 (captures 1720x1080 vs goldens 1280x720). run_verify
// forces the window to this size before capturing. See DEVELOPMENT_PRACTICES
// § Display environment.
static constexpr int verify_w = 1280;
static constexpr int verify_h = 720;

namespace {

/// Map a lens name (as used in the verify scripts) to its overlay_mode. Unknown
/// names fall back to overlay_mode::none.
overlay_mode overlay_from_name(const std::string& s)
{
    if (s == "supply")      return overlay_mode::supply;
    if (s == "market")      return overlay_mode::market;
    // "country" / "faction" ARE GONE (Ben, 2026-08-28), not aliased.
    //
    // They mapped to overlay_mode::none from BL-601 until now, on the reasoning
    // that a script naming the retired lens still captured the borders where they
    // had moved to. Measured on 2026-08-28, that was true of the PICTURE and false
    // of the CHECK: country_lens.lua went on asserting "the lens is named Country,
    // strip glyph = shield, territory tint identical to the prior Faction lens" —
    // three claims about a lens that no longer exists — and its two captures were
    // the plain canvas under a lens name (NR-690). Six more scripts swept
    // "country" as one leg and so captured the default view twice under two names.
    // An alias that silently answers a question nobody can ask any more is worse
    // than an error, so the name now takes the unknown-name fallback like any
    // other typo, and the scripts that used it are retired or re-pointed.
    if (s == "corporation") return overlay_mode::corporation;
    // Background firms — a "company" as distinct from a "corporation" since Ben's
    // 2026-08-28 terminology ruling (GLOSSARY.md).
    if (s == "company")     return overlay_mode::company;
    if (s == "resource")    return overlay_mode::resource;
    if (s == "population")  return overlay_mode::population;
    // "opportunity" and "production" were names here until BL-604 retired both
    // lenses. They now take the unknown-name fallback below, so a stale script
    // captures the plain canvas rather than failing to build.
    if (s == "scarcity")    return overlay_mode::scarcity;
    if (s == "industry")    return overlay_mode::industry;
    if (s == "reach")       return overlay_mode::reach;
    if (s == "continent")   return overlay_mode::continent;
    if (s == "supply_routes") return overlay_mode::supply_routes;
    if (s == "throughput")  return overlay_mode::throughput;
    return overlay_mode::none;
}

/// Every `resource_type` as its enum slug, INDEXED BY THE ENUM VALUE — so this is a
/// positional transcription, not a lookup table that can quietly disagree. The
/// static_assert below is the tripwire: adding a resource to `resource_type` fails
/// this translation unit until the slug is appended here.
///
/// This table went stale once already (20 of 38 values, 2026-08-18), and the failure
/// was silent because `resource_from_name` fell back to `iron_ore`: a verify script
/// naming `ordnance` seeded an IRON ORE convoy and still passed. That is worse than
/// an error, because the check certifies the wrong good. Hence both the assert and
/// the warning on the fallback path.
constexpr const char* k_resource_slugs[] = {
    "iron_ore",                // 0
    "coal",                    // 1
    "petroleum",               // 2
    "silica",                  // 3
    "copper_ore",              // 4
    "rare_earth_ore",          // 5
    "agricultural_produce",    // 6
    "water",                   // 7
    "iron_nickel_ore",         // 8
    "platinum_group_metals",   // 9
    "regolith",                // 10
    "stone",                   // 11
    "timber",                  // 12
    "sand",                    // 13
    "clay",                    // 14
    "peat",                    // 15
    "tobacco",                 // 16
    "spices",                  // 17
    "coffee",                  // 18
    "furs",                    // 19
    "steel",                   // 20
    "refined_fuel",            // 21
    "food_rations",            // 22
    "charcoal",                // 23
    "iron_blooms",             // 24
    "trade_goods_misc",        // 25
    "propellant",              // 26
    "silicon",                 // 27
    "refined_copper",          // 28
    "ree_alloy",               // 29
    "machinery",               // 30
    "alloys",                  // 31
    "electronics",             // 32
    "spacecraft_components",   // 33
    "clean_water",             // 34
    "consumer_goods",          // 35
    "medical_supplies",        // 36
    "ordnance",                // 37
    "ceramics",                // 38
    "dressed_stone",           // 39
    "planks",                  // 40
    "tools",                   // 41
    "hides",                   // 42
    "fibre",                   // 43
    "leather",                 // 44
    "cloth",                   // 45
    "rigging",                 // 46
};
static_assert(std::size(k_resource_slugs) == static_cast<std::size_t>(resource_type::count),
              "resource_type grew - append its slug to k_resource_slugs (and keep the order)");

/// Map a resource enum-slug to its `resource_type`, for the verify lens hooks. The
/// full roster, so a check can name any good. An unknown name still falls back to
/// iron_ore (a script naming a good that does not exist should not abort a capture
/// run) but SAYS SO, because a silent fallback reads as a passing check.
resource_type resource_from_name(const std::string& s)
{
    for (std::size_t i = 0; i < std::size(k_resource_slugs); ++i)
        if (s == k_resource_slugs[i])
            return static_cast<resource_type>(i);

    SDL_Log("verify: unknown resource '%s' - falling back to iron_ore", s.c_str());
    return resource_type::iron_ore;
}

/// Find a body by ROLE, or failing that by case-insensitive name. Returns
/// null_entity when no match is found.
///
/// The roles are the stable handle (BL-257): body names are generated per seed,
/// so a verify script naming a body by its display string would break the moment
/// the seed changed. Every script should use a role; the name path remains for
/// interactive/ad-hoc use.
///
///   home     - the homeworld
///   star     - the central star
///   moon     - the lowest-id moon of the homeworld
///   asteroid - the lowest-id asteroid
///   inner    - the innermost planet that is not the homeworld
entity_id find_body(const world& w, const std::string& name)
{
    if (name == "home")
        return w.home_body;
    if (name == "star")
        return w.star_body;
    if (name == "moon" || name == "asteroid" || name == "inner")
    {
        entity_id best = null_entity;
        float     best_orbit = 0.0f;
        for (const auto& [id, body] : w.bodies)
        {
            const bool match =
                  (name == "moon")     ? (body.type == body_type::moon && body.parent == w.home_body)
                : (name == "asteroid") ? (body.type == body_type::asteroid)
                                       : (body.type == body_type::planet && id != w.home_body);
            if (!match) continue;
            // Lowest id wins for moon/asteroid (creation order is deterministic);
            // "inner" wants the smallest orbit, which is the meaningful ordering.
            if (best == null_entity
                || (name == "inner" ? body.orbital_radius_au < best_orbit : id < best))
            {
                best = id;
                best_orbit = body.orbital_radius_au;
            }
        }
        return best;
    }
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

/// Map a mouse-button name (as used in the verify scripts) to its ImGui button.
/// Unknown names fall back to the left button — the overwhelmingly common case,
/// and a typo that silently did nothing would be worse than one that clicks.
int mouse_button_from_name(const std::string& s)
{
    if (s == "right")  return ImGuiMouseButton_Right;
    if (s == "middle") return ImGuiMouseButton_Middle;
    return ImGuiMouseButton_Left;
}

} // namespace

int app::run_autostart()
{
    // Headless coverage for the path --verify has never reached.
    //
    // run_verify (below) calls setup_world + load_economy and stops. It never
    // calls start_new_game, so generate_background_firms and the pre-game warm
    // start have NO automated coverage — which is how a crash in "placing
    // companies" reached a player build. This runs the real interactive tail,
    // headlessly, and reports which step it died on.
    // run() loads init.lua before anything else; start_new_game reads `config`
    // from it, so autostart has to do the same.
    m_lua.load("scripts/init.lua");

    std::printf("[autostart] step 1: generating world (this is the ~25s one)\n");
    std::fflush(stdout);
    m_pending_world_params = world_params{};

    // Mirror the INTERACTIVE path, not a convenient synchronous stand-in.
    // The wizard leaves its own surface build in flight when the player presses
    // Begin, so generation runs alongside it — two concurrent world generations,
    // which is the one structural difference between this and a plain
    // setup_world call. Reproducing it here is the whole point.
    std::printf("[autostart] step 2: wizard surface build (concurrent, as in the app)\n");
    std::fflush(stdout);
    launch_wizard_surface_build();

    std::printf("[autostart] step 3: begin_new_game (async worker + poll)\n");
    std::fflush(stdout);
    begin_new_game();

    // Poll on a wall clock, not a spin count — the interactive app polls once
    // per frame, and a tight loop finishes 600k iterations long before a 25 s
    // generation does (which is exactly how the first cut of this reported a
    // false failure).
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(300);
    while (m_screen != app_screen::in_game && std::chrono::steady_clock::now() < deadline)
    {
        // BL-630: no fork to take. poll_worldgen drives generation, then the
        // warm start, then the seat, and lands on in_game by itself.
        poll_worldgen();
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 Hz, as the app polls
    }
    if (m_screen != app_screen::in_game)
    {
        std::printf("[autostart] FAILED: generation did not complete within 300s\n");
        return 1;
    }

    std::printf("[autostart] OK  bodies=%zu nations=%zu corps=%zu markets=%zu tiles=%zu\n",
                m_world.bodies.size(), m_world.nations.size(), m_world.corporations.size(),
                m_world.markets.size(), m_world.tiles.size());
    std::fflush(stdout);
    return 0;
}

int app::run_verify(const std::string& script_path, bool bless)
{
    return run_verify_scripts({script_path}, bless);
}

int app::run_verify_all(const std::string& dir, bool bless)
{
    // Every committed check in one launch (BL-423). Sorted for a deterministic
    // run order; lib.lua is the helper library, not a check.
    namespace fs = std::filesystem;
    std::vector<std::string> scripts;
    for (const auto& e : fs::directory_iterator(dir))
    {
        if (!e.is_regular_file() || e.path().extension() != ".lua") continue;
        if (e.path().filename() == "lib.lua") continue;
        scripts.push_back(e.path().string());
    }
    std::sort(scripts.begin(), scripts.end());
    if (scripts.empty())
    {
        SDL_Log("verify-all: no scripts under %s", dir.c_str());
        return 1;
    }
    SDL_Log("verify-all: %zu script(s) under %s", scripts.size(), dir.c_str());
    return run_verify_scripts(scripts, bless);
}

void app::reset_verify_transients()
{
    m_last_econ_report = {};
    m_last_corp_standings.clear();
    m_last_econ_tick = 0;
    m_balance_history.clear();
    m_income_history.clear();
    m_expenditure_history.clear();
    m_market_history = {};
    m_building_rank_hist.clear();
    m_body_resource_hist = {};
    m_tile_resource_hist = {};
    m_tracked_tiles.clear();
    m_resource_hist_days.clear();
    m_resource_sample_index = 0;
    m_strategy_readout = {};
    m_prev_selection = null_entity;
    m_last_orbit_days = 0.0;
    m_last_survey_day = 0;
    // BL-521: the synthetic cursor is per-script state. Left held, a click at the
    // end of one script would still be pinning the pointer during the next one's
    // captures — exactly the cross-script leak the pristine restore exists to stop.
    m_pointer_injected = false;
    m_pointer_x = 0.0f;
    m_pointer_y = 0.0f;
    m_inject_button = -1;
    m_inject_down = false;
}

// --- Synthetic pointer input (BL-521) ---------------------------------------
//
// Why this exists: before it, the verify API could only WRITE ui_state, so a
// scripted check could stage a selection but never PRESS anything. Every
// interactive surface therefore shipped with its live half owed to a human
// (NR-416, NR-424 on BL-511 alone). These drive the real path instead — ImGui's
// event queue, the canvases' own hit-tests, the real click handlers — so
// "the press lands where the picture says it does" becomes checkable.
//
// Determinism is the binding constraint. The sequence is expressed entirely in
// FRAMES; nothing here reads a clock, and the one place ImGui itself would have
// (double-click promotion) is neutralised below.

void app::pump_injected_input()
{
    if (!m_pointer_injected)
        return;

    ImGuiIO& io = ImGui::GetIO();

    // Re-posted EVERY frame, not just on the frame that moved: the SDL3 backend
    // feeds the real OS cursor into the queue on any frame the window takes
    // focus, and ImGui applies queued events in order. Holding the position means
    // settle frames and captures see the same cursor the click did.
    io.AddMousePosEvent(m_pointer_x, m_pointer_y);

    // At most one button transition per frame. ImGui's trickling rule applies a
    // second change to the same button on the FOLLOWING frame anyway, so a
    // press+release pair queued together would silently become a two-frame
    // gesture with a frame of drawing between them that the script did not ask
    // for. Explicit is better: one transition, one frame, driven by the caller.
    if (m_inject_button >= 0)
    {
        io.AddMouseButtonEvent(m_inject_button, m_inject_down);
        m_inject_button = -1;
    }
}

ImVec2 app::resolve_tile_screen(int col, int row)
{
    if (m_ui.primary_level != canvas_level::planetary)
        return {-1.0f, -1.0f};
    m_ui.planetary_center_screen  = {-1.0f, -1.0f};
    m_ui.planetary_center_col     = col;
    m_ui.planetary_center_row     = row;
    m_ui.planetary_center_pending = true;
    render(); // the canvas consumes the request and publishes the screen point
    return m_ui.planetary_center_screen;
}

void app::inject_move(float x, float y, int frames)
{
    m_pointer_injected = true;
    m_pointer_x = x;
    m_pointer_y = y;
    // Both pointer sources, or the gesture disagrees with itself: the canvases
    // hit-test with ui_state.mouse (BL-061, and it is the only source that works
    // with a hidden window), while the shell decides minimap-vs-primary routing
    // and ImGui panel capture from io.MousePos.
    m_ui.mouse = {x, y, true};
    const int count = std::max(1, frames);
    for (int i = 0; i < count; ++i)
        render();
}

void app::inject_pointer(float x, float y, int button, int clicks)
{
    // Move and settle ONE frame before pressing. Two things resolve from the
    // position ImGui saw last frame — io.WantCaptureMouse (which decides whether
    // the canvas gets the click at all) and the canvas's own hovered_tile, which
    // the click handler consumes rather than re-deriving. A press in the same
    // frame as the move would be hit-tested against where the cursor used to be.
    inject_move(x, y, 1);

    ImGuiIO&    io                = ImGui::GetIO();
    const float saved_double_time = io.MouseDoubleClickTime;

    for (int c = 0; c < std::max(1, clicks); ++c)
    {
        // The one clock ImGui would consult, forced. UpdateMouseInputs promotes a
        // press to a repeat click by comparing g.Time (accumulated DeltaTime —
        // the wall clock) against MouseDoubleClickTime, so left alone, whether an
        // injected pair read as two singles or as one double would depend on how
        // long two frames took to render. That is precisely the non-determinism
        // this harness exists to exclude, and on a Debug build with a cold cache
        // it is not a theoretical margin. So the window is set per press: 0 can
        // never chain, FLT_MAX always does. The position is identical across the
        // pair by construction, so MouseDoubleClickMaxDist is satisfied.
        io.MouseDoubleClickTime = (c == 0) ? 0.0f : FLT_MAX;

        m_inject_button = button;
        m_inject_down   = true;
        render();               // press frame: IsMouseClicked fires here

        m_inject_button = button;
        m_inject_down   = false;
        render();               // release frame: IsMouseReleased, ImGui buttons fire
    }

    io.MouseDoubleClickTime = saved_double_time;
}

int app::run_verify_scripts(const std::vector<std::string>& scripts, bool bless)
{
    // Deterministic, non-interactive setup: fixed window (resized to verify_w/
    // verify_h below), seeded world, sim left paused so orbits and ticks never
    // advance between captures. The script drives view/overlay state directly.
    //
    // Hide BEFORE the ~40 s synchronous generation, not after — hidden-after
    // meant the window sat on the desktop for exactly the generation, which is
    // both the annoyance and the hang-ghost exposure window.
    SDL_HideWindow(m_window);

    // Setup runs ONCE for the whole batch (BL-423) — make_hard_coded_world costs
    // ~38 s on the Debug build, which is the entire reason batch mode exists. The
    // pristine snapshot taken after setup is what makes once safe: it is restored
    // before every script, so script order cannot leak state.
    setup_world();
    load_economy();

    // BL-365's background firms belong in the verified world too (added 2026-08-13).
    //
    // Without this the harness renders a world with NO background corporations,
    // which is not the game — and it made the re-pointed Industry lens (BL-373)
    // capture a picture of an EMPTY map that a golden would then have blessed as
    // correct. A check that renders nothing still writes a PNG and still passes.
    //
    // Same ordering as start_new_game: after load_economy, because the firms' stop
    // condition reads real recipe outputs. The seed fold matches the interactive
    // path so the verified world is the world the player would get. NOT warmed up —
    // run_verify stays deterministically cold, as its own comment below says.
    generate_background_firms(m_world, m_registry, /*seed=*/0x8A21F00Du);

    m_sim_loop.set_speed(0);

    // The harness renders the live world, not the main menu — flip past the launch
    // screen. A menu-verification script re-enters the menu with verify.show_menu.
    m_screen = app_screen::in_game;

    // BL-215: the text-overflow ledger records unconditionally under --verify,
    // whatever the build configuration.
    ui::set_overflow_recording(true);
    ui::clear_overflows();

    // BL-362: dwell clocks step a fixed 1/60 s per presentation frame here.
    // Scripts drive dwell by frame COUNT (verify.frames), and verify frames run
    // on the vsynced wall clock, so real deltas would put a seconds-based
    // threshold inside the jitter band and flake the hover goldens.
    m_ui.fixed_frame_clock = true;

    // Force the fixed verify capture size (verify_w × verify_h), decoupled from the
    // interactive window default (window_w × window_h) which is now larger. This
    // keeps captures + committed goldens at the 1280×720 standard regardless of the
    // interactive default. SyncWindow blocks until the resize is applied so the very
    // first capture already renders at the fixed size.
    SDL_SetWindowSize(m_window, verify_w, verify_h);

    // A verify run is headless work that happens to hold a window. It pumps no
    // events during the ~40 s synchronous generation and multi-second Debug
    // econ steps, so Windows declares it hung (five Application Hang 1002
    // events across 2026-08-14/15, one per silently truncated pass), ghosts
    // it, and the ghost's close path kills the whole batch. Three measures:
    // hide the window (nothing to click, nothing on the desktop while a pass
    // runs; captures read the render target, not the screen), stop the ghost
    // takeover outright on Windows, and pump events at every loop boundary we
    // control (per script, per econ tick — generation itself stays synchronous
    // and is covered by the first two).
    SDL_HideWindow(m_window);
#ifdef _WIN32
    DisableProcessWindowsGhosting();
#endif
    SDL_SyncWindow(m_window);

    m_verify_bless = bless;

    // The pristine state every script starts from (BL-423). Both are plain
    // value types (flat containers, entity ids, no pointers into each other),
    // so assignment is a complete, safe reset — the property the whole batch
    // rests on. Taken AFTER setup so the reach-budget mirror and window sizing
    // are part of what gets restored.
    const world    pristine_world = m_world;
    const ui_state pristine_ui    = m_ui;
    // The comms log is snapshotted rather than value-reset: its channel list is
    // seeded at setup and must survive; only the accumulated messages must not.
    const ui::chat_state pristine_chat = m_chat;

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
    // Save / load (BL-536), exposed so a capture script can pay the ~30-45 s
    // cold start ONCE and then open from a snapshot on every later run --
    // which is the development motive that raised the item. `save` returns
    // true on success; `load` returns false rather than throwing when the file
    // is missing or rejected, so a script can branch on "generate or load"
    // in one line:
    //
    //   if not verify.load("golden.iosave") then verify.save("golden.iosave") end
    //
    v.set_function("save", [this](const std::string& path) {
        return save_game_to(path);
    });
    v.set_function("load", [this](const std::string& path) {
        return load_game_from(path);
    });

    // The BL-204 tick-boundary checksum, exposed so a script (or a batch-mode
    // equivalence hunt — how BL-423's restore was proven) can print whether two
    // runs hold the same world without comparing pixels.
    v.set_function("state_hash", [this]() {
        char buf[24];
        std::snprintf(buf, sizeof buf, "%016llX",
                      static_cast<unsigned long long>(
                          m_world.state_hash(m_world.current_day_tick)));
        return std::string(buf);
    });
    // Dump the comms log as strings (channel:day:from:text), so an equivalence
    // hunt can diff feed CONTENT rather than pixels.
    v.set_function("chat_lines", [this]() {
        sol::state& s = m_lua.state();
        sol::table out = s.create_table();
        int idx = 0;
        for (const auto& msg : m_chat.messages)
        {
            char head[48];
            std::snprintf(head, sizeof head, "%d:d%d:%llu: ", msg.channel, msg.day,
                          static_cast<unsigned long long>(msg.from));
            out[++idx] = std::string(head) + msg.text;
        }
        return out;
    });
    v.set_function("capture", [this](const std::string& name) { capture_frame(name); });
    // Render N frames WITHOUT capturing (BL-228). capture() composits exactly one
    // frame, so any UI gated on elapsed dwell — the hover-card delays
    // (0.5 s / 2.5 s, i.e. 30 / 150 frames at the fixed verify step), and anything animated —
    // could never be reached from a script, which is why hover behaviour had no
    // saved check at all. Deterministic: the sim stays paused, so these are pure
    // presentation frames.
    v.set_function("frames", [this](sol::optional<int> n) {
        const int count = std::max(1, n.value_or(1));
        for (int i = 0; i < count; ++i)
            render();
    });

    // Perf-measurement tap (BL-249's instrument, scripted): reset the retained
    // frame window, then dump it as CSV after a scripted pan, so pan cost is
    // measured on the real renderer with the build's real vsync setting rather
    // than eyeballed off the HUD. Not a golden check — no captures involved.
    v.set_function("frame_reset", []() { frame_stats_instance().reset(); });
    v.set_function("frame_csv", [](const std::string& out_path) {
        const ui::frame_stats& fs = frame_stats_instance();
        std::ofstream f(out_path);
        f << "total_ms,build_ms,submit_ms,present_ms,other_ms,work_ms,"
             "vertices,draw_cmds\n";
        for (std::size_t i = 0; i < fs.count(); ++i)
        {
            const ui::frame_sample& s = fs.sample(i);
            f << s.total_ms << ',' << s.build_ms << ',' << s.submit_ms << ','
              << s.present_ms << ',' << s.other_ms() << ',' << s.work_ms() << ','
              << s.vertices << ',' << s.draw_cmds << '\n';
        }
    });
    // Resize the live window mid-script, so a perf run can measure at the real
    // interactive resolution instead of the fixed verify capture size.
    v.set_function("window", [this](int w, int h) {
        SDL_SetWindowSize(m_window, w, h);
        SDL_SyncWindow(m_window);
    });

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

    // --- Click injection (BL-521) -------------------------------------------
    // The API above this line only WRITES state; these PRESS. A scripted check can
    // now perform the gesture a player performs and capture what it produced, so a
    // "live" requirement no longer has to wait on a human. Each of these renders
    // its own frames and returns with the gesture complete — a following
    // capture()/shot() shows the result, with no settle dance of the caller's own.
    //
    // Every one takes a screen position or a grid coordinate. None takes, returns
    // or exposes tile internals: the standing rule (no individual tile data in
    // Lua) is unaffected — click_tile below hands the canvas a (col,row) exactly
    // as center_tile and select_tile already do, and gets back nothing but "did it
    // land".

    // Move the synthetic cursor and dwell there for `frames` frames. The dwell is
    // the point: the hover card appears at 30 frames and sticks at 150 (the fixed
    // 1/60 s verify clock), so hover behaviour is reachable by COUNT, never by
    // sleeping. Equivalent to mouse() + frames(), except that it also moves
    // ImGui's own cursor, so panels and buttons hover too — not just canvases.
    v.set_function("hover", [this](float x, float y, sol::optional<int> frames) {
        inject_move(x, y, frames.value_or(1));
    });

    // A full press+release at a screen position, through the real input path.
    // Button: "left" (default), "right", "middle".
    v.set_function("click", [this](float x, float y, sol::optional<std::string> button) {
        inject_pointer(x, y, mouse_button_from_name(button.value_or("left")), 1);
    });

    // Two press+release pairs that ImGui is FORCED to read as one double-click
    // (see inject_pointer). This is the descend gesture on the Solar and
    // Circumplanetary canvases, which is otherwise reachable only by a human.
    v.set_function("double_click", [this](float x, float y, sol::optional<std::string> button) {
        inject_pointer(x, y, mouse_button_from_name(button.value_or("left")), 2);
    });

    // Click the Planetary tile at (col, row) — the gesture BL-511's province
    // selection is made of, and the one that had no headless equivalent.
    //
    // It centres the tile first and clicks the canvas centre, because the canvas
    // is the only honest source for "the screen point that is tile (col,row)": it
    // publishes that point (planetary_center_screen) at the moment it applies the
    // pan, so the harness never re-derives title_h / hex_size / pan in Lua. The
    // view therefore MOVES — that is a real pan, and a capture after this call
    // shows the tile centred. Use click(x, y) where the framing must not change.
    //
    // Returns false (clicking nothing) when the Planetary canvas is not the
    // primary rung, or when it has not yet resolved the request.
    v.set_function("click_tile", [this](int col, int row, sol::optional<std::string> button) -> bool {
        const ImVec2 p = resolve_tile_screen(col, row);
        if (p.x < 0.0f)
            return false;
        inject_pointer(p.x, p.y, mouse_button_from_name(button.value_or("left")), 1);
        return true;
    });

    // The screen point that IS tile (col, row), for a script that wants to hover
    // it, click it twice, or click a few pixels off it. Same centring caveat as
    // click_tile: asking MOVES the view. Returns { ok, x, y }; ok = false when the
    // Planetary canvas is not the primary rung / the request did not resolve.
    v.set_function("tile_screen", [this](int col, int row) {
        sol::state&  st = m_lua.state();
        const ImVec2 p  = resolve_tile_screen(col, row);
        sol::table out = st.create_table();
        out["ok"] = (p.x >= 0.0f);
        out["x"]  = p.x;
        out["y"]  = p.y;
        return out;
    });

    // What the canvas's own hit-test currently resolves under the synthetic
    // cursor, as ids only — the province the last click would land on, and
    // whether anything is selected. The ASSERTION half of a live check: a capture
    // shows a card, this proves the click chose the subject the script meant.
    // Ids, not contents; no tile data crosses the seam.
    v.set_function("pointer_target", [this]() {
        sol::state& st = m_lua.state();
        sol::table  out = st.create_table();
        out["hovered_province"]  = static_cast<unsigned int>(m_ui.hovered_province);
        out["selected_province"] = static_cast<unsigned int>(m_ui.selected_province);
        out["has_selection"]     = (m_ui.selected_entity != null_entity);
        // WHAT is selected, not merely whether (added at Sprint 17b's integration).
        // `has_selection` cannot tell a nation from a building, and a structure
        // press and a marker press both clear the province mirror — so a check on
        // structure-grain selection (BL-601, and BL-603 after it) could assert the
        // side effect and never the subject. Exposed as the same word the Selection
        // element renders, so a script asserts on what the player reads.
        {
            const char* k = "none";
            switch (ui::selection_kind_of(m_world, m_ui.selected_entity))
            {
            case ui::selection_kind::body:        k = "body";        break;
            case ui::selection_kind::tile:        k = "tile";        break;
            case ui::selection_kind::building:    k = "building";    break;
            case ui::selection_kind::market:      k = "market";      break;
            case ui::selection_kind::unit:        k = "unit";        break;
            case ui::selection_kind::nation:      k = "nation";      break;
            case ui::selection_kind::corporation: k = "corporation"; break;
            default:                              k = "none";        break;
            }
            out["selection_kind"] = std::string(k);
        }
        // BL-469: the battle selection is a third channel into the same element,
        // so the assertion half needs it too — without these every expect() about
        // a battle card would be a proxy for something else.
        out["selected_battle_province"] = static_cast<unsigned int>(m_ui.selected_battle_province);
        out["selected_battle_attacker"] = static_cast<unsigned int>(m_ui.selected_battle_attacker);
        out["selected_battle_defender"] = static_cast<unsigned int>(m_ui.selected_battle_defender);
        // The hover half of the same seam: dwell is reached by frame COUNT
        // (hover(x, y, frames)), and these say whether the glance card came up
        // and whether it has stuck. Booleans, not contents.
        // Which fold-out ledger is open (BL-603's other half: a structure click
        // must OPEN its ledger, not merely select the thing). One string rather
        // than a flag per panel, because the column shows one at a time.
        {
            const char* panel = "none";
            if (m_ui.show_market_ledger)           panel = "market";
            else if (m_ui.show_balance_ledger)     panel = "balance";
            else if (m_ui.show_corporation_panel)  panel = "corporation";
            else if (m_ui.show_economy_panel)      panel = "economy";
            else if (m_ui.show_construction_panel) panel = "construction";
            else if (m_ui.show_tile_ledger)        panel = "tile";
            else if (m_ui.show_contracts_ledger)   panel = "contracts";
            out["open_panel"] = std::string(panel);
        }
        // Which section of the tile Selection element's top nav is showing, so a
        // check can assert the chevrons MOVED it rather than assuming they did.
        out["selection_section"] = m_ui.card_tile_view;
        out["hover_card"]        = (m_ui.hover_card_entity != null_entity);
        out["hover_card_stuck"]  = m_ui.hover_card_stuck;
        out["x"]                 = m_pointer_x;
        out["y"]                 = m_pointer_y;
        return out;
    });

    // Drive the economy headlessly: run N economy ticks (production → market →
    // budget) and open the economy panel so a capture shows live, populated data.
    // The durable verification method for the Layer 3 economy panel's visual rows.
    v.set_function("econ_step", [this](sol::optional<int> n) {
        const int steps = n.value_or(1);
        for (int i = 0; i < steps; ++i)
        {
            // Advance the day tick with the step. The interactive loop maintains
            // this from sim_loop (app.cpp), but --verify drives step_economy
            // directly with the sim paused, so without this the field stays 0 for
            // the whole session — and anything stamped on it (the BL-362 caches,
            // economy_system's own per-tick bookkeeping) silently never
            // invalidates, freezing derived values across every capture.
            ++m_world.current_day_tick;
            step_economy();
            // Heartbeat: a Debug econ tick runs seconds; without this the
            // window is hang-declared mid-script (see the run header above).
            SDL_PumpEvents();
        }
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
        else if (name == "frame_hud")    m_ui.show_frame_hud = open;    // frame-budget HUD (BL-249)
        else if (name == "tech_tree")    m_ui.show_tech_tree = open;    // F9 mock viewer (BL-087)
        else if (name == "decisions")    m_ui.show_decision_feed = open; // AI decision feed (BL-407)
        else if (name == "strategy")     m_ui.show_strategy_readout = open; // Strategy readout (BL-411)
        else if (name == "contracts")    m_ui.show_contracts_ledger = open; // Contracts ledger (BL-576)
        // Nav slot 8's all-corporations table. It had no name here, so the one
        // rail slot whose panel a script could not open was also the only one
        // with no capture — green-but-blind by omission rather than by design.
        else if (name == "corporations_table") m_ui.show_corporations_table = open;
        // The in-session system menu (the header-corner popup, time_panel.cpp).
        // `show_menu` sounds like this and is not: it re-enters the LAUNCH screen.
        // The popup that holds save/load/options/quit had no hook at all, so the
        // one surface a player must reach to leave a session was uncapturable.
        else if (name == "system_menu")         m_ui.show_system_menu = open;
        // BL-536: the Generation Ledger had no name here, so no script could open
        // it. That mattered the moment a save had to prove it restores the
        // generation_report — this ledger is one of the two surfaces that read it.
        else if (name == "generation_ledger") m_ui.show_generation_ledger = open;
    });

    // Spectator mode (BL-409). Set BEFORE econ_step: step_economy reads
    // m_ui.spectating on each call and forwards it to the strategic tier, so
    // flipping it after the ticks have run would capture a feed populated by an
    // ordinary played session while claiming to show a spectated one.
    v.set_function("spectate", [this](bool on) { m_ui.spectating = on; });

    // Spectator god view (BL-408). Pure ui_state, read at the draw call only —
    // it never feeds the sim, so unlike spectate() there is no before/after
    // ordering constraint against econ_step. Meaningful only when spectate(true)
    // is also set: every read-site gates on the PAIR, exactly as the live
    // system-menu toggle (time_panel.cpp) does.
    v.set_function("god_view", [this](bool on) { m_ui.god_view = on; });

    // Select a corporation entity, so a capture can show the Selection band's
    // corp card (BL-408's god-facts readout lives there). Ids come from
    // verify.buildings()' `corp` field; an unknown id is a no-op, matching
    // select_building's silent-miss behaviour.
    v.set_function("select_corp", [this](unsigned id) {
        if (m_world.corporations.count(static_cast<entity_id>(id)))
            m_ui.selected_entity = static_cast<entity_id>(id);
    });

    // Park the AI decision feed's filters (BL-407 R2) so a capture can show a
    // FILTERED list — the resting state is "all", and a filter that only ever
    // appears under a live cursor is a filter no check can see.
    //
    // `reason` takes a corp_decision_reason value, or -1 for "every reason";
    // `corp` takes an entity id, or 0 for "every corporation". Both are passed
    // through unvalidated on purpose: the feed clamps its own filters, and a
    // script that parks an out-of-range value should see the feed's real
    // fallback rather than a silently corrected one.
    v.set_function("decision_filter", [this](int reason, sol::optional<int> corp) {
        m_ui.decision_feed_reason = reason;
        m_ui.decision_feed_corp   = static_cast<entity_id>(corp.value_or(0));
    });

    // Park the Strategy readout's corp selector (BL-411) so a capture can show
    // the single-corp profile view — the resting state is the all-corporations
    // comparison. `corp` takes an entity id, 0 for "all", or -1 for the player
    // corp: a script cannot know a generated corp's id, but the player entity
    // always exists and (under spectate) always has decisions to show.
    v.set_function("strategy_filter", [this](int corp) {
        m_ui.strategy_readout_corp =
            (corp == -1) ? m_world.player_entity : static_cast<entity_id>(corp);
    });

    // Park a fold-out ledger on one of its button-strip views (BL-117 sweep), so a
    // capture can reach a sub-view a click would otherwise be needed for. Unknown
    // names are ignored; each panel clamps its own index.
    v.set_function("panel_view", [this](const std::string& name, int view) {
        if (name == "history")            m_ui.history_view = view;
        else if (name == "history_round") m_ui.history_round = view;
        // The Ages time-lapse is a scrubber, so its YEAR is the thing a capture
        // needs to park — parking only the view would pin every shot to year 0,
        // the one frame where no history has happened yet (BL-277).
        else if (name == "ages_year")     m_ui.ages_year = view;
        else if (name == "economy")       m_ui.economy_view = view;
        else if (name == "market")        m_ui.market_ledger_view = view;
        else if (name == "tech_tree")     m_ui.tech_tree_view = view;
        else if (name == "contracts")     m_ui.contracts_ledger_view = view; // BL-576: 0 Offers, 1 Active, 2 History
    });

    // Park a fold-out ledger's SCROLL at a fraction of its extent (0 = top,
    // 1 = foot), so a capture can reach content the column clips. Without this the
    // verify API could open a panel and pick its view but never see past the fold —
    // the Generation History biography was captured at its first ~8 lines, so a
    // rewrite of everything below it diffed at 0.0000% against the golden.
    //
    // Sticky, landing on the FOLLOWING frame and against the previous frame's
    // content height, so a script must render at least one more frame before
    // capturing — and another after resetting to 0, or the next capture inherits it:
    //   verify.scroll_panel("history", 1.0); verify.frames(2); verify.capture(...)
    // Names mirror show_panel's vocabulary; an unknown name clears the request.
    v.set_function("scroll_panel", [](const std::string& name, double fraction) {
        const char* window = "";
        if (name == "tile" || name == "history") window = "Tile Ledger";
        else if (name == "market")               window = "Market Ledger";
        else if (name == "economy")              window = "Economy";
        else if (name == "balance")              window = "Balance Ledger";
        else if (name == "corporation")          window = "Corporations";
        else if (name == "construction")         window = "Building";
        ui::foldout_request_scroll(window, static_cast<float>(fraction));
    });

    // Park a surface's drill-through disclosure state (BL-214) so a capture can show
    // the EXPANDED full-screen view, not just the folded verdict line. Without this
    // the fold idiom is unverifiable — every capture would show the resting state.
    // Names mirror the detail_surface enumerators; an unknown name folds everything,
    // which is also what fold() with no argument means.
    v.set_function("fold", [this](sol::optional<std::string> name, sol::optional<int> key) {
        if (!name)
        {
            ui::fold(m_ui);
            return;
        }
        const std::string& n = *name;
        detail_surface s = detail_surface::none;
        if      (n == "selection_metric") s = detail_surface::selection_metric;
        else if (n == "building_metric")  s = detail_surface::building_metric;
        else if (n == "history_story")    s = detail_surface::history_story;
        else if (n == "history_chain")    s = detail_surface::history_chain;
        else if (n == "generation_stage") s = detail_surface::generation_stage;
        else if (n == "corp_rollup")      s = detail_surface::corp_rollup;
        if (s == detail_surface::none) ui::fold(m_ui);
        else                           ui::expand(m_ui, s, key.value_or(0));
    });

    // Park the building Selection card's pager on one of its pages (NR-246,
    // 2026-08-16). `fold("building_metric", k)` sets the drill KEY, not the page,
    // so before this a script had no way to reach the Method or Workforce page and
    // every capture showed page 1 — which is exactly how the old
    // building_management_shell.lua came to claim it verified a surface it never
    // photographed. The page index is clamped by building_pages() at draw time, so
    // an out-of-range value parks on the last page rather than drawing nothing.
    v.set_function("building_page", [this](int page) { m_ui.selection_building_page = page; });

    // Drill one row into the expanded Corporation-dashboard roll-up (BL-248), or
    // -1 to return to the roll-up itself.
    v.set_function("rollup_drill", [this](int row) { m_ui.corp_rollup_drill = row; });

    // `verify.why_note` lived here, arming BL-247's question-log sentinel. Removed
    // 2026-08-02 with the log's draw path (NEEDS_REVIEW NR-018) — the log is
    // development documentation now, not an in-game surface, so there is nothing on
    // screen for a script to open. Scripts calling it must drop the call.

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
            else if (type == "launchpad")  bt = building_type::launchpad;
            else if (type == "logistics_hub") bt = building_type::inland_logistics_hub;
            else if (type == "military_base") bt = building_type::military_base; // BL-325 S1
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
            // BL-323 S2: filter on the SAME reach budget construct_building will
            // enforce. Without this the scan offers a tile the authoritative gate
            // then refuses, which reads as a broken build rather than a rule.
            body_reach_field(m_world, tc.body);
            if (!placement_rules::can_place_in_world(m_world, tid, bt, tgt,
                                                     m_registry.construction().max_logistics_reach))
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
                r == construction_result::insufficient_materials ? "insufficient_materials" :
                r == construction_result::tech_locked            ? "tech_locked" :
                r == construction_result::era_locked             ? "era_locked" :
                r == construction_result::depth_locked           ? "depth_locked" : "failed";
            if (r == construction_result::placed)
                m_ui.selected_entity = built;
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
                r == construction_result::insufficient_materials ? "insufficient_materials" :
                r == construction_result::tech_locked            ? "tech_locked" :
                r == construction_result::era_locked             ? "era_locked" :
                r == construction_result::depth_locked           ? "depth_locked" : "failed";
            if (r == construction_result::placed)
                m_ui.selected_entity = built;
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

    // BL-215 text-overflow ledger. clipping() reads the running FAIL count
    // (clipped + unfittable); expect_no_clipping(label) logs every FAIL record,
    // adds them to m_verify_failures, and writes screenshots/text_overflow.txt.
    // Sanctioned elide-with-tooltip records stay WARN-only.
    v.set_function("clipping", [this]() -> int {
        return static_cast<int>(ui::overflow_failures());
    });
    v.set_function("expect_no_clipping", [this](sol::optional<std::string> label) -> int {
        const std::size_t fails = ui::overflow_failures();
        for (const ui::text_overflow& r : ui::overflows())
        {
            if (r.sev == ui::overflow_severity::elided)
                continue;
            SDL_Log("verify.expect_no_clipping FAIL [%s] %s: \"%s\" "
                    "(needed %.0fpx, available %.0fpx, frame %s)",
                    label ? label->c_str() : "", r.site, r.text.c_str(),
                    static_cast<double>(r.needed), static_cast<double>(r.available),
                    r.frame.empty() ? "-" : r.frame.c_str());
            ++m_verify_failures;
        }
        std::error_code ec;
        std::filesystem::create_directories("screenshots", ec);
        ui::write_overflow_report("screenshots/text_overflow.txt");
        SDL_Log("verify.expect_no_clipping %s: %zu failure(s), %zu record(s) total",
                fails == 0 ? "PASS" : "FAIL", fails, ui::overflows().size());
        return static_cast<int>(fails);
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
    // Place a standing sell order through the SAME path the Market Ledger's
    // "Add sell order" button uses — which since BL-293 is the corp-command seam,
    // not a push onto a vector. Applied immediately here rather than enqueued: a
    // verify script is a caller with a mutable world in hand, so it takes the same
    // route app::render takes when it drains m_ui.pending_order_commands. Body
    // resolves to the player's home body. Returns the resulting count of the
    // player's standing orders, so a script can assert the order registered — and
    // a REJECTED order now returns the unchanged count rather than silently
    // registering, which the old vector path could not express at all.
    // THE BINDING THAT MAKES A BATTLE REACHABLE AT ALL (BL-469's rider, NR-345).
    //
    // Before this, `verify_api` called `apply_corp_command` exactly once, hard-
    // wired to `place_sell_order`. There was NO path from a script to a fight:
    // battle discovery is gated on `corp_hostile_pairs`, which only
    // `declare_hostile` writes, which only the live corporation panel reaches —
    // and `corp_ai` never declares. So every visual requirement on the battle
    // card was green-but-blind by construction, which is precisely the failure
    // NR-345 records. One generic binding subsumes hostility, hiring, marching
    // and withdrawal rather than growing four narrow ones.
    //
    // IT IS AN AI-FACING-SEAM-SHAPED SURFACE, so it follows that standing rule
    // even though it is only reachable from a local script: every field is
    // validated as the value that LANDS in the destination, the whole command is
    // rejected on violation, and a rejection mutates nothing. The verb is range-
    // gated against `corp_verb_count` exactly as `run_serve` gates the wire.
    //
    // Returns the corp_command_result as a string, so a script asserts the
    // OUTCOME rather than inferring it from a side effect.
    v.set_function("corp_command", [this](sol::table t) -> std::string {
        const int verb_i = t.get_or("verb", -1);
        if (verb_i < 0 || verb_i >= static_cast<int>(corp_verb_count))
            return "rejected_invalid"; // out of range: never narrow-cast it into a real verb

        corp_command cmd;
        cmd.tick = static_cast<int>(m_sim_loop.day_tick());
        cmd.corp = static_cast<entity_id>(t.get_or("corp",
                       static_cast<unsigned int>(m_world.player_entity)));
        cmd.verb = static_cast<corp_verb>(verb_i);

        cmd.subject      = static_cast<entity_id>(t.get_or("subject", 0u));
        cmd.counterparty = static_cast<entity_id>(t.get_or("counterparty", 0u));
        cmd.tile         = static_cast<entity_id>(t.get_or("tile", 0u));
        cmd.province     = static_cast<uint32_t>(t.get_or("province",
                               static_cast<unsigned int>(no_province)));
        cmd.unit_type    = static_cast<uint16_t>(t.get_or("unit_type", 0));

        // BL-576: accept_offer's/abandon_contract's `order` (a mercenary_offer
        // or mercenary_contract id) and accept_offer's committed `units` array
        // — unreachable from every verify script until now, the same gap
        // NR-345 named for hostility/march/withdraw before `corp_command`
        // widened to cover them.
        cmd.order = static_cast<uint32_t>(t.get_or("order", 0u));
        sol::optional<sol::table> units_tbl = t.get<sol::optional<sol::table>>("units");
        if (units_tbl)
        {
            std::size_t i = 0;
            for (auto& kv : *units_tbl)
            {
                if (i >= mercenary_contract_max_units)
                    break;
                cmd.units[i++] = static_cast<entity_id>(kv.second.as<unsigned int>());
            }
        }

        // Range-check BEFORE the narrowing cast, not after — a value that fits a
        // Lua number can still be outside the destination's domain.
        const int wf = t.get_or("workforce", 100);
        if (wf < 0 || wf > 200) return "rejected_invalid";
        cmd.workforce = wf;

        const auto r = apply_corp_command(m_world, m_registry, cmd);
        switch (r)
        {
            case corp_command_result::applied:              return "applied";
            case corp_command_result::rejected_no_corp:     return "rejected_no_corp";
            case corp_command_result::rejected_not_owner:   return "rejected_not_owner";
            case corp_command_result::rejected_invalid:     return "rejected_invalid";
            case corp_command_result::rejected_placement:   return "rejected_placement";
            case corp_command_result::rejected_funds:       return "rejected_funds";
            case corp_command_result::rejected_state:       return "rejected_state";
            case corp_command_result::rejected_tech_locked: return "rejected_tech_locked";
            case corp_command_result::rejected_era_locked:  return "rejected_era_locked";
            case corp_command_result::rejected_depth_locked:return "rejected_depth_locked";
            case corp_command_result::rejected_cooldown:    return "rejected_cooldown";
            case corp_command_result::rejected_embargo:     return "rejected_embargo";
            case corp_command_result::rejected_no_capacity: return "rejected_no_capacity";
            case corp_command_result::rejected_no_input_access: return "rejected_no_input_access";
            case corp_command_result::rejected_reputation:  return "rejected_reputation";
        }
        return "rejected_invalid";
    });

    // BL-576: seed a `mercenary_offer` directly. TEST-ONLY FIXTURE HOOK — there
    // is no player-reachable verb that CREATES an offer (offers come from
    // `derive_contract_offers`, a nation-budget-side pass with no corp_verb of
    // its own), so a script cannot organically produce one the way battle_card.lua
    // drives a battle into existence through real verbs. Fully escrows by
    // default (`offer_escrow = fee`), so the offer is immediately acceptable —
    // a script wanting to exercise the "still filling" state overrides `escrow`.
    // Returns the new offer's id, or 0 on a bad province / no nation to bill it to.
    v.set_function("inject_offer", [this](sol::table t) -> unsigned int {
        const uint32_t province_id = static_cast<uint32_t>(t.get_or("province", 0u));
        const province* pr = m_world.provinces.find(province_id);
        if (pr == nullptr)
            return 0;

        // Client nation: whichever nation owns the province's own ground, if
        // any, else the first nation in the world — this is fixture plumbing,
        // not a claim about who SHOULD hold the target province.
        entity_id client = null_entity;
        if (!pr->tiles.empty())
        {
            const auto tnit = m_world.tile_to_nation.find(pr->tiles.front());
            if (tnit != m_world.tile_to_nation.end())
                client = tnit->second;
        }
        if (client == null_entity && !m_world.nations.empty())
            client = m_world.nations.begin()->first;
        if (client == null_entity)
            return 0;

        mercenary_offer o;
        o.id               = m_world.allocate_offer_id();
        o.client           = client;
        o.target_province  = province_id;
        o.template_index   = t.get_or("template", 0);
        o.fee              = t.get_or("fee", 500.0f);
        o.issued_tick      = m_world.current_econ_tick;
        o.deadline         = m_world.current_econ_tick + t.get_or("deadline_in", 200);
        o.offer_escrow     = t.get_or("escrow", o.fee);
        m_world.mercenary_offers.push_back(o);
        return o.id;
    });

    // BL-576: read every open mercenary offer — the id/client/target/fee/escrow
    // a script needs to drive `corp_command{verb=accept_offer, order=..., ...}`
    // against an offer it (or the live nation AI) produced, mirroring the
    // units()/buildings() reader idiom rather than making a script guess ids.
    v.set_function("offers", [this]() {
        sol::table out = m_lua.state().create_table();
        int i = 1;
        for (const mercenary_offer& o : m_world.mercenary_offers)
        {
            sol::table row = m_lua.state().create_table();
            row["id"]       = o.id;
            row["client"]   = static_cast<unsigned int>(o.client);
            row["province"] = o.target_province;
            row["fee"]      = o.fee;
            row["escrow"]   = o.offer_escrow;
            row["deadline"] = o.deadline;
            row["template"] = o.template_index;
            out[i++] = row;
        }
        return out;
    });

    // BL-576: read every one of the PLAYER's own mercenary contracts — active
    // or terminal — the same reader idiom as offers() above. `state` is a
    // string ("active"/"completed"/"failed"/"abandoned") so a script asserts
    // on the same words the History view renders, not a raw enum ordinal.
    v.set_function("contracts", [this]() {
        sol::table out = m_lua.state().create_table();
        int i = 1;
        for (const mercenary_contract& c : m_world.mercenary_contracts)
        {
            if (c.contractor != m_world.player_entity)
                continue;
            sol::table row = m_lua.state().create_table();
            row["id"]           = c.id;
            row["client"]       = static_cast<unsigned int>(c.client);
            row["province"]     = c.province;
            row["fee"]          = c.fee;
            row["deposit_paid"] = c.deposit_paid;
            row["deadline"]     = c.deadline;
            const char* state_word =
                (c.state == mercenary_contract_state::active)    ? "active"    :
                (c.state == mercenary_contract_state::completed) ? "completed" :
                (c.state == mercenary_contract_state::failed)    ? "failed"    :
                                                                    "abandoned";
            row["state"] = state_word;
            out[i++] = row;
        }
        return out;
    });

    // Where every unit stands, as ids only — the read half NR-345 also names.
    // Without it a script can inject a click but cannot find a unit to click at,
    // so the battle card stays unreachable however good the injection is.
    v.set_function("units", [this]() {
        sol::state& st = m_lua.state();
        sol::table  out = st.create_table();
        int i = 1;
        std::vector<entity_id> ids;
        ids.reserve(m_world.units.size());
        for (const auto& kv : m_world.units) ids.push_back(kv.first);
        std::sort(ids.begin(), ids.end()); // ascending: w.units is unordered
        for (const entity_id uid : ids)
        {
            const unit_component& u = m_world.units.at(uid);
            sol::table row = st.create_table();
            row["id"]    = static_cast<unsigned int>(uid);
            row["owner"] = static_cast<unsigned int>(u.owner);
            row["count"] = u.count;
            row["type"]  = static_cast<int>(u.type);
            if (const auto tit = m_world.tiles.find(u.position); tit != m_world.tiles.end())
            {
                row["col"] = tit->second.grid_x;
                row["row"] = tit->second.grid_y;
            }
            out[i++] = row;
        }
        return out;
    });

    v.set_function("place_sell_order",
        [this](const std::string& res, double qty, double floor) -> int {
            corp_command cmd;
            cmd.tick        = static_cast<int>(m_sim_loop.day_tick());
            cmd.corp        = m_world.player_entity;
            cmd.verb        = corp_verb::place_sell_order;
            cmd.subject     = m_world.home_body;
            cmd.target      = resource_from_name(res);
            cmd.quantity    = static_cast<float>(qty);
            cmd.floor_price = static_cast<float>(floor);
            const corp_command_result r = apply_corp_command(m_world, m_registry, cmd);

            int n = 0;
            for (const sell_order& o : m_world.sell_orders)
                if (o.corp == m_world.player_entity) ++n;
            SDL_Log("verify.place_sell_order: %s x%.0f >= %.1f (applied=%d, n=%d)",
                    res.c_str(), qty, floor,
                    r == corp_command_result::applied ? 1 : 0, n);
            return n;
        });

    // Read the resolved market price of `res` on the player's home body — lets a
    // sell-order script assert the floor was honoured (a placed floor above the
    // clearing price prevents a below-floor dump; the pool retains stock rather than
    // selling under the floor). Returns -1 if the home body has no market.
    v.set_function("econ_tick", [this]() -> int { return m_world.current_econ_tick; });

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
    //
    // ROUTED THROUGH dispatch_action SINCE BL-536, not apply_canvas_command. The
    // comment above always claimed parity with the keyboard and did not have it:
    // a key press goes through app::dispatch_action, which handles the commands
    // that need app members — the time controls, and now quick save/load — before
    // falling through to apply_canvas_command for everything else. Calling the
    // inner function directly meant `verify.command("pause_toggle")` silently did
    // nothing, and would have meant the same for the two save bindings.
    //
    // Behaviour-identical for every existing caller: the committed scripts drive
    // only ascend/descend/zoom_in, which dispatch_action forwards unchanged.
    v.set_function("command", [this](const std::string& name) {
        if (auto cmd = ui::canvas_command_from_name(name))
            dispatch_action(*cmd);
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
            {
                m_ui.selected_entity = tid;
                // BL-598: write the province MIRROR the gesture writes, so a
                // staged tile selection renders exactly as a clicked one does —
                // province outline included. The canvas re-derives it anyway on
                // its next frame, but a capture taken before that frame (or on a
                // rung with no Planetary canvas) would otherwise differ from the
                // gesture this shortcut stands in for.
                m_ui.selected_province    = m_world.provinces.province_of(tid);
                m_ui.province_sync_entity = tid;
                break;
            }
    });
    // BL-598: THE HOOK FOLLOWS THE GESTURE. This used to write the province-only
    // selection tuple — `selected_province` set, `selected_entity` null — which
    // was exactly what a click on bare ground produced. It no longer is: the
    // province folded into the tile Selection element, so a click selects the
    // TILE and carries its province as a mirror, and no gesture reaches the old
    // tuple at all. A hook that still wrote it would be staging a state a player
    // cannot get to, and every script asserting on it would be testing fiction.
    //
    // So this now does what `select_tile` does and RETURNS the province id, which
    // is what its callers outside this file actually want it for (staging a
    // battle, a contract, a march target). Kept rather than deleted for that
    // return value and for the name's readability at the call site: "select the
    // tile, and tell me the province it stands in".
    //
    // Still a SHORTCUT: since BL-521 verify.click_tile(col,row) performs the real
    // gesture, and click_injection.lua asserts the two agree. Prefer the click
    // where the check is about the gesture; this stays for staging a selection
    // cheaply, without the pan click_tile performs. Returns 0 when the tile is
    // ocean / unpartitioned / absent.
    v.set_function("select_province", [this](int col, int row) -> unsigned int {
        for (const auto& [tid, tc] : m_world.tiles)
        {
            if (tc.body != m_ui.active_body || tc.grid_x != col || tc.grid_y != row)
                continue;
            const uint32_t pid = m_world.provinces.province_of(tid);
            m_ui.selected_entity      = tid;
            m_ui.province_sync_entity = tid;
            m_ui.selected_province    = pid;
            return pid;
        }
        return 0u;
    });
    v.set_function("select_battle", [this](int col, int row) -> bool {
        // BL-469's rider (NR-345). BL-521's click injection CAN reach a battle
        // now that rung 0 exists — a click on any envelope tile lands on it — so
        // this is a shortcut rather than the only road. It earns its place the
        // way select_province does: it writes the whole selection tuple itself,
        // including province_sync_entity, so the next planetary frame's
        // reconciliation does not immediately undo it.
        for (const auto& [tid, tc] : m_world.tiles)
        {
            if (tc.body != m_ui.active_body || tc.grid_x != col || tc.grid_y != row)
                continue;
            const uint32_t pid = m_world.provinces.province_of(tid);
            const active_battle* b = (pid != 0) ? first_battle_in(m_world, pid) : nullptr;
            if (b == nullptr)
                return false;
            m_ui.selected_entity      = null_entity;
            m_ui.selected_province    = 0;
            m_ui.province_sync_entity = null_entity;
            m_ui.selected_battle_province = b->province;
            m_ui.selected_battle_attacker = b->attacker;
            m_ui.selected_battle_defender = b->defender;
            return true;
        }
        return false;
    });
    v.set_function("clear_selection", [this]() {
        // Deselect (BL-266): the band never hides — with no selection it rests on
        // the player's own corporation. This is the empty-space-click equivalent,
        // without the pan a real click on empty ground would need staging for
        // (verify.click(x, y) is the gesture itself, since BL-521).
        //
        // BL-511: clears the PROVINCE too. An empty-space click on the real canvas
        // resolves no hovered tile, so it writes both fields null; leaving the
        // province set here would make this shortcut diverge from the gesture it
        // stands in for, and a capture after it would show a stale province card.
        m_ui.selected_entity      = null_entity;
        m_ui.selected_province    = 0;
        m_ui.province_sync_entity = null_entity;
        // BL-469, same reason BL-511 had to add the province: a shortcut that
        // leaves one of the three selection channels set diverges from the
        // gesture it stands in for, and a capture after it shows a stale card.
        m_ui.clear_battle_selection();
    });
    v.set_function("card_drill", [this]() {
        // Drive the Selection band's resource drill-down (BL-196) for the currently
        // selected tile — the equivalent of clicking a resource graph, which
        // BL-521's injection cannot reach without the graph's screen rect being
        // published first (see its open question). Drills the tile's first deposited
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
            {
                m_ui.selected_entity = bid;
                // BL-598: a marker hit carries NO province mirror — a building
                // card has no province section for the outline to be about — so
                // this shortcut clears it, exactly as the click handler does.
                m_ui.selected_province    = 0;
                m_ui.province_sync_entity = bid;
                break;
            }
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
        m_prev_selection = m_ui.selected_entity;
    });

    // Press the construction ledger's Build button on the CURRENT tile selection
    // (verify harness). Writes exactly the fields draw_construction_ledger's button
    // writes — pending_tile / pending_type / pending_target / pending_recipe — and
    // nothing else, so the request drains through app::render's real construct_building
    // path on the next frame, including the BL-162 rule that the ledger survives the
    // build it initiated. build_first_valid / build_at deliberately BYPASS this seam
    // (they call construct_building directly and reselect the result), so neither can
    // exercise it.
    //
    // @param type   "extraction" / "processing" / "port" / "launchpad" / "hub".
    // @param target Extraction target resource name (ignored for other types).
    // @param recipe Processing recipe name, or "" for construct_building's default.
    v.set_function("ledger_build", [this](const std::string& type, const std::string& target,
                                          const std::string& recipe) {
        if (m_ui.selected_entity == null_entity || m_world.tiles.count(m_ui.selected_entity) == 0)
        {
            SDL_Log("verify.ledger_build: selection is not a tile");
            return;
        }
        // Unrecognised input REFUSES rather than falling through to a default. A typo'd
        // type would otherwise silently build an extraction site, and a typo'd recipe
        // would resolve to no_recipe and be turned into steel by construct_building —
        // so the script would report a green pass while proving nothing about the very
        // seam this hook exists to exercise.
        building_type bt = building_type::extraction_site;
        if      (type == "extraction") bt = building_type::extraction_site;
        else if (type == "processing") bt = building_type::processing_facility;
        else if (type == "port")       bt = building_type::port;
        else if (type == "launchpad")  bt = building_type::launchpad;
        else if (type == "hub")        bt = building_type::inland_logistics_hub;
        else
        {
            SDL_Log("verify.ledger_build: unrecognised type '%s' — refusing", type.c_str());
            return;
        }

        const resource_type tgt =
            target.empty() ? resource_type::iron_ore : resource_from_name(target);

        std::uint16_t rec = no_recipe;
        if (!recipe.empty())
        {
            rec = m_registry.recipe_id(recipe);
            if (rec == no_recipe)
            {
                SDL_Log("verify.ledger_build: unrecognised recipe '%s' — refusing "
                        "(it would have silently become the construct_building default)",
                        recipe.c_str());
                return;
            }
        }

        m_ui.construction.pending_tile   = m_ui.selected_entity;
        m_ui.construction.pending_type   = bt;
        m_ui.construction.pending_target = tgt;
        m_ui.construction.pending_recipe = rec;
        SDL_Log("verify.ledger_build: queued %s (recipe id %u) on the selected tile",
                type.c_str(), static_cast<unsigned>(m_ui.construction.pending_recipe));
    });

    // select_body picks a body by name, exactly as a single-click on the Solar /
    // Circumplanetary canvas would (sets selected_entity to the body). Lets a
    // script stage the selection-aware descend gesture (BL-165).
    v.set_function("select_body", [this](const std::string& name) {
        // Same resolver as goto_surface/go_to, so a script can name a body by
        // ROLE ("home", "inner", "moon") rather than by its generated display
        // name (BL-257).
        const entity_id b = find_body(m_world, name);
        if (b != null_entity)
            m_ui.selected_entity = b;
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
        // Sorted corp order (review 2026-08-19 #11): corporations is an
        // unordered_map, and which rival a script's "first non-player row"
        // lands on must not vary by platform or stdlib.
        std::vector<entity_id> sorted_corp_ids;
        sorted_corp_ids.reserve(m_world.corporations.size());
        for (const auto& [cid, c] : m_world.corporations)
            sorted_corp_ids.push_back(cid);
        std::sort(sorted_corp_ids.begin(), sorted_corp_ids.end());
        for (entity_id corp_id : sorted_corp_ids)
        {
            const auto& corp   = m_world.corporations.at(corp_id);
            const bool  player = (corp_id == m_world.player_entity);
            // BL-365 `is_background`: a "company" rather than a "corporation"
            // since Ben's 2026-08-28 split. Exposed so a script can ASSERT the
            // two lenses are disjoint, which is the property that makes the pair
            // correct and which no single capture can show.
            const bool  background = corp.is_background;
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
                rec["background"] = background;
                rec["body"]   = static_cast<unsigned>(tile_it->second.body);
                rec["x"]      = tile_it->second.grid_x;
                rec["y"]      = tile_it->second.grid_y;
                // Type name, so a script can pick the building it actually wants to
                // stage (an extraction site rather than whichever asset happens to
                // come first) instead of hard-coding grid coordinates that a
                // generation change silently invalidates.
                rec["type"]   = ui::building_type_name(bld_it->second.type);
                // The tile ENTITY ID, and whether the building is finished.
                //
                // Both exist for NR-696 option C: a fixture that raises its own
                // force the way a player does needs to name the tile its muster
                // base stands on (hire_unit's `tile` is an entity id, and x/y
                // above cannot be turned into one from Lua), and needs to know
                // when that base has actually completed — BL-325 S2 refuses a
                // muster onto a base still under construction, so a script must
                // be able to wait for it rather than guess a tick count.
                rec["tile"]   = static_cast<unsigned>(bld_it->second.tile);
                rec["complete"] = (bld_it->second.ticks_remaining == 0);
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

    int script_errors = 0;
    for (const std::string& script_path : scripts)
    {
        SDL_PumpEvents(); // heartbeat between scripts (see the run header above)

        // Re-assert the fixed capture size: a menu/wizard script re-enters the
        // interactive sizing path (measured: one resize mid-batch left 69
        // goldens blessed at 1720x1080), and the batch must hand every script
        // the 1280x720 the goldens standardise on. Hidden stays hidden.
        SDL_SetWindowSize(m_window, verify_w, verify_h);
        SDL_SyncWindow(m_window);
        // Every script sees the state a solo run gives it: pristine world and
        // ui, the in-game screen (a menu script re-enters via verify.show_menu
        // and must not strand its successor there), and a clean overflow
        // ledger. What this deliberately does NOT reset is the shared Lua
        // state: a script's leaked GLOBAL survives into the next script.
        // Accepted — the committed scripts use locals — and lib.lua is
        // reloaded each iteration so the helper globals a script might clobber
        // come back.
        // A cold ImGui context per script. ImGui derives per-window layout
        // (scrollbar visibility, sizes) from the PREVIOUS frame, and solo runs
        // bless every golden on cold frame-1 windows — a warm window carries
        // the prior script's content shape into this script's first capture.
        // Measured: with everything else proven identical (per-tick state_hash,
        // the full comms message list), the comms child still rendered with the
        // previous script's scrollbar/wrap, diffing 1.2% against its solo
        // golden. Rebuilding the context reproduces frame-1 conditions exactly;
        // the font-atlas rebuild it costs is milliseconds against the ~38 s
        // generation this mode exists to amortise.
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplSDL3_InitForSDLRenderer(m_window, m_renderer);
        ImGui_ImplSDLRenderer3_Init(m_renderer);
        ui::load_ui_font();
        ui::set_overflow_recording(true);

        // Restore via copy-construct + move-assign, NOT plain copy-assign.
        // operator= into a container that a previous script's ticks have grown
        // keeps the grown bucket count, and unordered_map iteration order can
        // depend on bucket count — belt-and-braces beside the context reset
        // above; a fresh copy's storage derives from the pristine contents
        // alone, and the move replaces the old storage wholesale.
        {
            world fresh_world = pristine_world;
            m_world = std::move(fresh_world);
            ui_state fresh_ui = pristine_ui;
            m_ui = std::move(fresh_ui);
            ui::chat_state fresh_chat = pristine_chat;
            m_chat = std::move(fresh_chat);
        }
        m_screen = app_screen::in_game;
        reset_verify_transients();
        ui::clear_overflows();

        // Goldens live in a "golden" directory beside each verify script, so
        // running against the source script path (the skill's iteration mode)
        // reads/writes the committed source tree, not a stale build copy.
        m_golden_dir = (std::filesystem::path{script_path}.parent_path() / "golden").string();

        const int failures_before = m_verify_failures;
        try
        {
            // Auto-load the helper library (scripts/verify/lib.lua) from the
            // script's own directory before running it, so scripts get the
            // high-level helpers (sweep_overlays, tour_buildings, frame_tile)
            // without a `require` (the package lib is not opened).
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
            // A batch does not abort on one broken script (BL-423 R3): count
            // it, say so, and keep going — the remaining 78 checks are exactly
            // as informative as they were before this one broke.
            SDL_Log("verify script FAILED: %s: %s", script_path.c_str(), e.what());
            ++script_errors;
            continue;
        }
        SDL_Log("verify: %s — %d golden failure(s)",
                script_path.c_str(), m_verify_failures - failures_before);
    }

    // Non-zero exit if any capture failed its golden diff or any script threw,
    // so the harness/skill reads an advisory PASS/FAIL off the process result
    // as well as the logs.
    if (m_verify_failures > 0 || script_errors > 0)
    {
        SDL_Log("verify: %d capture(s) failed golden diff, %d script error(s)",
                m_verify_failures, script_errors);
        return 1;
    }
    return 0;
}
