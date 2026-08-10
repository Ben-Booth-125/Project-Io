/// @file verify_api.cpp
/// The `verify` Lua API — app::run_verify and its script-facing bindings.
/// Extracted verbatim from app.cpp (BL-361); behaviour and registered function
/// names are unchanged (scripts/verify/*.lua call them by name).

#include "app.hpp"

#include <imgui.h>
#include <SDL3/SDL.h>

#include "ui/canvas_command.hpp"
#include "ui/detail_level.hpp"
#include "ui/frame_stats.hpp"
#include "ui/market_ledger.hpp"
#include "ui/presentation.hpp"
#include "ui/selection.hpp"
#include "ui/text_fit.hpp"
#include "ui/view_nav.hpp"
#include "world/construction.hpp"
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
    if (s == "country")     return overlay_mode::country;
    if (s == "faction")     return overlay_mode::country; // legacy alias (renamed BL-052)
    if (s == "corporation") return overlay_mode::corporation;
    if (s == "resource")    return overlay_mode::resource;
    if (s == "population")  return overlay_mode::population;
    if (s == "opportunity") return overlay_mode::opportunity;
    if (s == "production")  return overlay_mode::production;
    if (s == "scarcity")    return overlay_mode::scarcity;
    if (s == "industry")    return overlay_mode::industry;
    if (s == "reach")       return overlay_mode::reach;
    if (s == "continent")   return overlay_mode::continent;
    if (s == "supply_routes") return overlay_mode::supply_routes;
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
        {"propellant", resource_type::propellant},
    };
    const auto it = m.find(s);
    return it != m.end() ? it->second : resource_type::iron_ore;
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

} // namespace

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

    // BL-215: the text-overflow ledger records unconditionally under --verify,
    // whatever the build configuration.
    ui::set_overflow_recording(true);
    ui::clear_overflows();

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
    // Render N frames WITHOUT capturing (BL-228). capture() composits exactly one
    // frame, so any UI gated on elapsed frames — the hover-card delays
    // (kHoverAppearDelay = 30, kHoverStickDelay = 150), and anything animated —
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
        else if (name == "frame_hud")    m_ui.show_frame_hud = open;    // frame-budget HUD (BL-249)
        else if (name == "tech_tree")    m_ui.show_tech_tree = open;    // F9 mock viewer (BL-087)
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
        else if (n == "history_story")    s = detail_surface::history_story;
        else if (n == "history_chain")    s = detail_surface::history_chain;
        else if (n == "generation_stage") s = detail_surface::generation_stage;
        else if (n == "corp_rollup")      s = detail_surface::corp_rollup;
        if (s == detail_surface::none) ui::fold(m_ui);
        else                           ui::expand(m_ui, s, key.value_or(0));
    });

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
                r == construction_result::tech_locked            ? "tech_locked" : "failed";
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
                r == construction_result::tech_locked            ? "tech_locked" : "failed";
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
            { m_ui.selected_entity = tid; break; }
    });
    v.set_function("clear_selection", [this]() {
        // Deselect (BL-266): the band never hides — with no selection it rests on
        // the player's own corporation. This is the empty-space-click equivalent;
        // no click/key injection exists in the headless harness.
        m_ui.selected_entity = null_entity;
    });
    v.set_function("card_drill", [this]() {
        // Drive the Selection band's resource drill-down (BL-196) for the currently
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
            { m_ui.selected_entity = bid; break; }
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
