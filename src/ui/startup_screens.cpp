/// @file startup_screens.cpp
/// The app's entry screens (docs/ui/STARTUP.md): the main menu, the New World
/// wizard (BL-167), and the wizard's preview plumbing. Extracted verbatim from
/// app.cpp (BL-361); behaviour unchanged — these remain app member functions,
/// they simply live beside the other ui surfaces now.

#include "core/app.hpp"

#include <imgui.h>

#include "ui/detail_level.hpp"
#include "ui/generation_charts.hpp"
#include "ui/generation_preview.hpp"
#include "ui/history_lapse.hpp"      // BL-829/BL-830: round 4's map and its board
#include "world/era_timelapse.hpp"   // owner_slice_at — the whole replay substrate

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

void app::open_new_world_wizard()
{
    // Nothing is generated yet — the wizard runs on a throwaway preview and only
    // commits when the player reaches its last round and presses "Begin".
    m_wiz_round = 0;
    m_wiz_dirty = true;
    m_screen    = app_screen::generating;

    // THE FULL FOUR THOUSAND YEARS (BL-846; Ben, 2026-09-09). The struct default
    // is 400, which is a SCOPE knob rather than a design one — it exists so a
    // harness that does not test the era can skip paying for it. A player's
    // world is not a harness, and round 4's whole subject is the span.
    //
    // WHY IT IS SET HERE AND NOT AS THE STRUCT DEFAULT. Every headless harness
    // in the project builds worlds from `world_params{}`, and the era at 4000
    // years costs ~6.5 s against ~60 ms at 400. Moving the default would put
    // that on every check in the repo to serve one screen. So the wizard asks
    // for the long span and everything else keeps the cheap one.
    //
    // IT REACHES "BEGIN" TOO, AND THAT IS DELIBERATE RATHER THAN INCIDENTAL:
    // `begin_new_game` builds from these same params, so the campaign gets the
    // history the player actually watched. A wizard that showed a 4000-year
    // history and then dealt a 400-year world would be lying about the world it
    // was selling.
    m_pending_world_params.prehistory_years = 4000;
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
    // The system's coined catalogue for this seed (BL-257) — the wizard must
    // name the bodies the campaign will actually name them.
    m_wiz_names = generate_body_names(m_pending_world_params.seed);
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

    // The globe's real surface. Synchronous under --verify (a capture must never
    // race the worker); asynchronous in play, marked stale if already in flight.
    if (!m_golden_dir.empty())
    {
        world scratch;
        const entity_id probe = scratch.create_entity();
        const auto tiles = generate_home_surface_preview(scratch, probe,
                                                         m_pending_world_params);
        m_wiz_surface.resize(tiles.size());
        for (std::size_t i = 0; i < tiles.size(); ++i)
            m_wiz_surface[i] = ui::preview_pack(scratch.tiles.at(tiles[i]).substrate,
                                                scratch.tiles.at(tiles[i]).cover);
    }
    else if (m_wiz_surface_future.valid())
        m_wiz_surface_stale = true;
    else
        launch_wizard_surface_build();
}

void app::launch_wizard_surface_build()
{
    m_wiz_surface_future = std::async(std::launch::async,
        [params = m_pending_world_params]() {
            world scratch;
            const entity_id probe = scratch.create_entity();
            const auto tiles = generate_home_surface_preview(scratch, probe, params);
            std::vector<uint8_t> comp(tiles.size());
            for (std::size_t i = 0; i < tiles.size(); ++i)
                comp[i] = ui::preview_pack(scratch.tiles.at(tiles[i]).substrate,
                                           scratch.tiles.at(tiles[i]).cover);
            return comp;
        });
}

namespace {

/// Lift the recorded era out of a finished generation report.
///
/// EVERYTHING THE ROUND DRAWS COMES FROM HERE, and it is all a reading: the
/// change list, the settled regions' positions and names, and generation's own
/// three era counters. Nothing is re-simulated and nothing is re-derived — see
/// `app::launch_wizard_history_run` for why that matters more than it looks.
ui::history_lapse lapse_from_report(const generation_report& rep)
{
    ui::history_lapse h;

    // The homeworld by its authored flag, not by name or position: names are
    // generated and display-only (BL-257), and the body list can be reordered.
    const generation_report::body_entry* home = nullptr;
    for (const generation_report::body_entry& b : rep.bodies)
        if (b.is_homeworld) { home = &b; break; }
    if (home == nullptr) return h;

    h.lapse  = home->prehistory_timelapse;
    h.grid_w = home_grid_width;
    h.grid_h = home_grid_height;

    h.region_col.reserve(home->settlement.regions.size());
    h.region_row.reserve(home->settlement.regions.size());
    h.region_name.reserve(home->settlement.regions.size());
    for (const region& r : home->settlement.regions)
    {
        h.region_col.push_back(r.col);
        h.region_row.push_back(r.row);
        h.region_name.push_back(r.name);
    }

    h.battles   = rep.prehistory_battles;
    h.conquests = rep.prehistory_conquests;
    h.foundings = rep.prehistory_foundings;
    return h;
}

} // namespace

void app::launch_wizard_history_run()
{
    if (m_wiz_history_future.valid()) return; // already running; Run is disabled meanwhile

    m_wiz_history = ui::history_lapse{};
    m_wiz_history_playing = false;
    m_wiz_history_carry   = 0.0f;

    // The wait's own content. Cleared FIRST so no frame can read the previous
    // run's pass split as this one's — the same ordering begin_new_game keeps.
    m_wiz_history_progress.stage.store(0, std::memory_order_relaxed);
    m_wiz_history_progress.label.store(0, std::memory_order_relaxed);
    m_wiz_history_progress.stage_count.store(generation_stage_label_count,
                                             std::memory_order_relaxed);
    m_wiz_history_progress.sub_progress.store(0, std::memory_order_relaxed);
    m_wiz_history_progress.sub_total.store(0, std::memory_order_relaxed);

    // UNDER --verify, ADOPT THE WORLD THE HARNESS ALREADY BUILT. run_verify opens
    // in_game on a generated world, so `m_generation_report` already holds this
    // very record — running the pass a second time would cost a Debug harness
    // minutes to reproduce a record it is already holding, and would produce the
    // same one. Nothing is faked: it is generation's report either way.
    if (!m_golden_dir.empty())
    {
        ui::history_lapse adopted = lapse_from_report(m_generation_report);
        if (!adopted.empty())
        {
            m_wiz_history      = std::move(adopted);
            m_wiz_history_year = m_wiz_history.lapse.start_year;
            return;
        }
    }

    // Lua and the works table are read on THIS thread before the worker starts:
    // sol2 is not thread-safe, and `m_works` is generation's input (BL-321).
    m_lua.load("scripts/world_gen.lua");
    world_gen_config cfg{};
    cfg.load_from_lua(m_lua);
    ensure_works_loaded();

    // STOP ONCE THE ERA HAS RUN. Everything this round draws comes out of the
    // report by the end of stage 8; stages 9-12 (borders, roads, companies,
    // finishing) were being computed and thrown away, which measured 10,805 ms
    // of 11,316 — about 95% of the wait — and is why the round visibly hung on
    // "Laying roads" (Ben, 2026-09-09).
    //
    // Note this is set on the COPY the worker takes, never on the campaign's:
    // `begin_new_game` builds a whole world from its own config, and a world
    // stopped at stage 8 has no nations, roads or corporations in it.
    world_gen_config hist_cfg = cfg;
    hist_cfg.stop_after_ancient_era = true;

    auto run = [this, hist_cfg, params = m_pending_world_params]() {
        generation_report rep;
        // The world itself is DISCARDED. What the round wants is the era it
        // recorded, and holding the world would only invite a second, divergent
        // copy of the campaign's own.
        (void)make_hard_coded_world(params, &rep, hist_cfg, &m_wiz_history_progress,
                                    &m_works);
        return lapse_from_report(rep);
    };

    if (!m_golden_dir.empty())
        m_wiz_history_future = std::async(std::launch::deferred, run);
    else
        m_wiz_history_future = std::async(std::launch::async, run);

    // A deferred future never becomes ready on its own, so a capture path
    // resolves it here and now rather than spinning forever in poll.
    if (!m_golden_dir.empty())
        poll_wizard_history();
}

void app::poll_wizard_history()
{
    if (!m_wiz_history_future.valid())
        return;
    if (m_golden_dir.empty()
        && m_wiz_history_future.wait_for(std::chrono::seconds(0))
               != std::future_status::ready)
        return;
    ui::history_lapse landed = m_wiz_history_future.get();
    if (m_wiz_history_stale)
    {
        // The ground moved while this ran: it is a true history of a world the
        // player has already rerolled away from. Dropped rather than drawn.
        m_wiz_history_stale   = false;
        m_wiz_history         = ui::history_lapse{};
        m_wiz_history_playing = false;
        return;
    }
    m_wiz_history      = std::move(landed);
    m_wiz_history_year = m_wiz_history.lapse.start_year;
    // It plays the moment it lands: the run was the wait, and the playback is
    // what the player pressed Run for. Frozen under --verify, where the year is
    // set by the script instead (verify.history_year).
    m_wiz_history_playing = m_golden_dir.empty();
    m_wiz_history_carry   = 0.0f;
}

void app::poll_wizard_surface()
{
    if (!m_wiz_surface_future.valid())
        return;
    if (m_wiz_surface_future.wait_for(std::chrono::seconds(0))
            != std::future_status::ready)
        return;
    m_wiz_surface = m_wiz_surface_future.get();
    if (m_wiz_surface_stale)
    {
        // Preferences moved while that build ran: it is already the wrong
        // world, so go straight around again with the current params.
        m_wiz_surface_stale = false;
        launch_wizard_surface_build();
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

// The stage explainers, the round table, and every stage chart moved out of this
// file into ui::generation_charts (src/ui/generation_charts.hpp), so the History
// ledger can redraw the same plots from the persisted generation_report. The
// wizard is no longer the only place the chain is ever visible (BL-211).

/// How many preference rows a round owns, and how many dim caption lines sit under
/// them. Both feed the height reserved for the decision block, which is pinned to
/// the bottom so the charts get everything left over.
/// File-local mirrors of app's round counts: those are private to `app`, and these
/// helpers are free functions. draw_generation_screen static_asserts the pair against
/// the real constants, so a drift here is a compile error, not a wrong layout.
constexpr int planetology_rounds = 3;
constexpr int pass_rounds        = 2;

/// The pass rounds take no preference rows yet — their leans arrive with the passes
/// themselves (BL-829 round 4, BL-819 round 5) — so they reserve nothing but the
/// placeholder caption.
int round_pref_count(int r)
{
    if (r >= planetology_rounds) return 0;
    return (r == 0) ? 4 : (r == 1) ? 3 : 1;
}
int round_note_lines(int r)
{
    if (r >= planetology_rounds) return 0;
    return (r == 1) ? 3 : 1; ///< B carries the iron/coal caption.
}

/// A round's header text. The planetology rounds take theirs from the shared chain
/// table (so the wizard and the History ledger name them identically); the two pass
/// rounds are not chain rounds and carry their own, from STARTUP.md § Rounds 4 and 5.
struct wizard_round_head
{
    const char* name;
    const char* question;
};

wizard_round_head wizard_round_head_at(int r)
{
    if (r < planetology_rounds)
    {
        const ui::chain_round& cr = ui::chain_round_at(r);
        return { cr.name, cr.question };
    }
    static const wizard_round_head passes[pass_rounds] = {
        { "The History",
          // THE SPAN NAMED HERE IS THE SPAN THE RECORD ACTUALLY COVERS, and it
          // is not yet the four thousand years the design asks for (NR-810).
          // `run_settlement` places and dates every region BEFORE
          // `run_history_sim` starts, so the record opens with the whole map
          // already claimed and only the sim's own span is watchable. The
          // subtitle said "four thousand years" and played four hundred.
          //
          // Retitled rather than left aspirational, on the rule that a surface
          // must not assert something the code has not delivered. It goes back
          // when BL-846 (colonisation span) makes the foundings happen INSIDE
          // the recorded span — at which point this line is the smallest part
          // of that change.
          "Who claimed this ground, and who lost it, in the age before the epoch?" },
        { "The Substrate",
          "What does that ground produce, and who trades it?" },
    };
    int i = r - planetology_rounds;
    if (i < 0)                              i = 0;
    if (i >= pass_rounds)  i = pass_rounds - 1;
    return passes[i];
}

/// The honest placeholder a pass round rests as until its pass is built. Labelled as
/// a placeholder in as many words: an unlabelled empty pane reads as a finished
/// surface, and the next session believes it.
void draw_pass_round_placeholder(int pass_index)
{
    constexpr ImU32 col_dim   = IM_COL32(120, 128, 145, 255);
    constexpr ImU32 col_label = IM_COL32(205, 170, 90, 255);

    ImGui::PushStyleColor(ImGuiCol_Text, col_label);
    ImGui::TextUnformatted("PLACEHOLDER - this round is not built yet");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
    if (pass_index == 0)
        ImGui::TextWrapped(
            "Round 4 will run the history pass: four thousand years to 1200 CE, drawn as a "
            "time-lapse on a 2D map in place of the globe, with a leaderboard here on the "
            "left. Nothing runs yet; the globe beside this is still the planetology globe.");
    else
        ImGui::TextWrapped(
            "Round 5 will run the economy pass: 1560 to 1960, then the substrate carve - "
            "metros, colonial reach, firms and their charters, and the market carve. "
            "Nothing runs yet; the globe beside this is still the planetology globe.");
    ImGui::Spacing();
    ImGui::TextWrapped("Reroll and Back work. Nothing on this round changes the world yet.");
    ImGui::PopStyleColor();
}

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
    // Label on its own line, options in a 2x2 grid beneath — the wizard's
    // control column is a third of the panel now, and four radios in a row
    // ("Old and cold ... Young and vigorous") no longer fit one line.
    ImGui::PushID(id);
    ImGui::TextUnformatted(label);

    const float col2 = ImGui::GetContentRegionAvail().x * 0.5f;
    bool changed = false;
    for (int i = 0; i < 4; ++i)
    {
        if (i % 2 == 1) ImGui::SameLine(col2);
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

    // One reroll counter per PLANETOLOGY round: `roll` is a planetology input and
    // reaches resolve_preferences, so it is keyed to the chain's rounds, not to the
    // wizard's total. The wizard's two pass rounds carry their own counters
    // (m_wiz_pass_roll) because they are not planetology inputs at all.
    static_assert(sizeof(world_preferences::roll)
                      == sizeof(uint32_t)
                             * static_cast<std::size_t>(wizard_planetology_round_count),
                  "world_preferences::roll must carry one counter per planetology round");

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
        // The planetology chain just moved, so both pass rounds are stale: the
        // history runs ON this world. Causality flows one way, downstream only.
        invalidate_wizard_rounds_below(wizard_planetology_round_count - 1);
    }
    if (m_wiz_preview.empty())
        return; // defensive: the preview is the wizard's only data source

    // Adopt a finished real-surface build (and chain a relaunch if the params
    // moved mid-build). Cheap zero-wait probe; runs every wizard frame.
    poll_wizard_surface();
    // The same probe for round 4's history run (BL-829). Also every frame, and
    // also cheap: the wizard must keep repainting while the pass works, because
    // on this round the wait IS the content.
    poll_wizard_history();

    // What this GUARDED, before the wizard grew past the chain (BL-816): that every
    // wizard round the code hands to ui::chain_round_at has a chart-round entry
    // behind it. It was written as an equality only because the two numbers happened
    // to be the same when the wizard was planetology and nothing else. They are not
    // the same number any more — the wizard walks five rounds, the chart chain still
    // covers three — so the equality is re-expressed as the two facts it stood for:
    // the chart chain covers exactly the planetology rounds, and the planetology
    // rounds are a strict prefix of the wizard. Every chain_round_at call below is
    // gated on `planetology_round` accordingly.
    static_assert(ui::chain_round_count == wizard_planetology_round_count,
                  "the chart chain must cover exactly the wizard's planetology rounds");
    static_assert(wizard_planetology_round_count < wizard_round_count,
                  "the planetology rounds are a strict prefix of the wizard's rounds");
    static_assert(planetology_rounds == wizard_planetology_round_count
                      && pass_rounds == wizard_pass_round_count,
                  "the file-local round-count mirrors must track app's constants");

    // Which kind of round is on screen. The planetology rounds preview a pure chain
    // per keystroke; the pass rounds cannot (STARTUP.md § The wait is the round).
    const bool planetology_round = (m_wiz_round < wizard_planetology_round_count);
    const int  pass_index        = m_wiz_round - wizard_planetology_round_count;

    // ── Round 4's playback, advanced once per frame and read TWICE — the board on
    //    the left and the map on the right must show the same instant, so the
    //    slice is materialised here rather than in each of them. ──
    const bool history_round = (m_wiz_round == wizard_planetology_round_count);
    std::vector<uint16_t> hist_slice, hist_lagged;
    if (history_round && !m_wiz_history.empty())
    {
        // The land mask comes from the wizard's OWN packed surface — the same
        // raster the globe samples and the same one "Begin" builds — so the
        // coastline a frontier stalls at is the coastline the campaign has. It is
        // a no-op until that async build lands; the round simply retries.
        ui::finish_history_lapse(m_wiz_history,
                                 m_wiz_surface.empty() ? nullptr : m_wiz_surface.data(),
                                 m_wiz_surface.size());

        const int first = m_wiz_history.lapse.start_year;
        const int last  = first + m_wiz_history.lapse.years;

        // FROZEN UNDER --verify, for the reason the globe's rotation is: a capture
        // must never race an animation. The year is then whatever the script set.
        if (m_wiz_history_playing && m_golden_dir.empty())
        {
            // The rate follows the SPAN rather than being a constant, the same
            // derivation the History ledger's Ages view makes: the recorded era is
            // 400 years today and the round is written against 4000, and a fixed
            // years-per-second would empty the transport in a blink on one of them.
            const float span = static_cast<float>(last - first);
            constexpr float run_secs = 30.0f;
            const float rate = span > 0.0f ? span / run_secs : 1.0f;
            m_wiz_history_carry += ImGui::GetIO().DeltaTime * rate;
            const int whole = static_cast<int>(m_wiz_history_carry);
            if (whole > 0)
            {
                m_wiz_history_carry -= static_cast<float>(whole);
                m_wiz_history_year  += whole;
            }
            if (m_wiz_history_year >= last)
            {
                m_wiz_history_year    = last;
                m_wiz_history_playing = false;
            }
        }
        if (m_wiz_history_year < first) m_wiz_history_year = first;
        if (m_wiz_history_year > last)  m_wiz_history_year = last;

        hist_slice = owner_slice_at(m_wiz_history.lapse, m_wiz_history_year);
        // The lagged board, for the entry/exit marks. A twelfth of the span back:
        // far enough that a rank move means something, near enough that the marks
        // are not permanently lit.
        const int lag = std::max(1, (last - first) / 12);
        hist_lagged = owner_slice_at(m_wiz_history.lapse, m_wiz_history_year - lag);
    }

    const wizard_round_head wr       = wizard_round_head_at(m_wiz_round);
    const int               n_bodies = std::min(static_cast<int>(m_wiz_preview.size()),
                                               prototype_body_count());

    // The homeworld is the subject of every single-body chart. Located by its
    // authored flag rather than by position, so the body list can be reordered.
    std::size_t home = 0;
    for (int i = 0; i < n_bodies; ++i)
        if (prototype_body(i).is_homeworld) { home = static_cast<std::size_t>(i); break; }

    // The charts themselves live in ui::generation_charts, shared with the History
    // ledger so the plots a player decided against are the same plots they can
    // reopen mid-campaign. The wizard hands in the live preview plus its
    // zero-drawdown twin — the "formed" reference the Spend chart's hollow columns
    // are measured against.
    std::vector<ui::generation_chart_body> chart_bodies;
    chart_bodies.reserve(static_cast<std::size_t>(n_bodies));
    for (int i = 0; i < n_bodies; ++i)
    {
        const std::size_t k = static_cast<std::size_t>(i);
        chart_bodies.push_back(ui::generation_chart_body{
            m_wiz_names.bodies[k].c_str(), // generated, not the table placeholder (BL-257)
            &m_wiz_preview[k],
            (k < m_wiz_undrawn.size()) ? &m_wiz_undrawn[k] : nullptr });
    }
    const ui::generation_chart_source chart_src{
        chart_bodies.data(), chart_bodies.size(), home };

    constexpr ImU32 col_bright = IM_COL32(225, 230, 240, 255);
    constexpr ImU32 col_dim    = IM_COL32(120, 128, 145, 255);

    // One dim, wrapping text helper — every subtitle and caption reads in the same
    // colour as the menu's tagline.
    auto dim_text = [&](const char* t) {
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextWrapped("%s", t);
        ImGui::PopStyleColor();
    };


    // A wide centred surface — this is the first thing a player sees, so it takes
    // the screen rather than the menu's 280px column. Same borderless,
    // background-less idiom as draw_main_menu; the render clear colour is the backdrop.
    // Split 1/3 : 2/3 — controls (stage folds, leans, reroll) left, the round's
    // painted preview right, so every reroll is SEEN, not just re-plotted.
    const float panel_w = std::min(disp.x - 96.0f, 1440.0f);
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

        // ── Left third: everything the player DOES — header, stage folds, leans,
        //    navigation. The stage folds still expand to their full chart views
        //    (the fold idiom is unchanged); they simply live in a column now. ──
        const float col_w = (panel_w - style.WindowPadding.x * 2.0f
                             - style.ItemSpacing.x) / 3.0f;
        ImGui::BeginChild("##wiz_left", {col_w, 0.0f}, false,
                          ImGuiWindowFlags_NoBackground);

        // ── (a) Header: the round name large, what it settles beneath, progress right ──
        {
            ImDrawList*  dl    = ImGui::GetWindowDrawList();
            const float  big   = ImGui::GetFontSize() * 1.7f;
            const ImVec2 p     = ImGui::GetCursorScreenPos();
            const float  avail = ImGui::GetContentRegionAvail().x;

            // The atlas carries a single size, so the title is scaled through the
            // draw list rather than by swapping fonts (there is no second font).
            dl->AddText(ImGui::GetFont(), big, p, col_bright, wr.name); // fit-exempt: chrome strip authored to fit at the 1280x720 floor

            std::snprintf(buf, sizeof buf, "Round %d of %d", m_wiz_round + 1, wizard_round_count);
            const ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText({p.x + avail - ts.x, p.y + (big - ts.y) * 0.5f}, col_dim, buf); // fit-exempt: chrome strip authored to fit at the 1280x720 floor

            ImGui::Dummy({avail, big + 2.0f});
        }
        dim_text(wr.question);

        // One pip per wizard round, the current one lit: past rounds filled dim,
        // future ones hollow. Drives off wizard_round_count, so it grew with it.
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
        // Each lean row is now a label line plus a 2x2 radio grid (three lines).
        const float decide_h = static_cast<float>(round_pref_count(m_wiz_round))
                                   * (line_h + 2.0f * (frame_h + style.ItemSpacing.y))
                             + line_h * static_cast<float>(round_note_lines(m_wiz_round)
                                                           + (m_wiz_resolved.gave_up ? 2 : 0))
                             + style.ItemSpacing.y * 3.0f;
        // Two button rows now: Reroll full-width above, Back / Continue below.
        const float footer_h = 34.0f * 2.0f + style.ItemSpacing.y * 3.0f;
        ImGui::BeginChild("##wiz_charts", {0.0f, -(decide_h + footer_h)}, false,
                          ImGuiWindowFlags_NoBackground);

        // The player still watches the chain work link by link — they have just
        // stopped clicking between the links. Each stage measures its own column
        // metric from the region it is handed, so the same call fits here and in the
        // History ledger's much narrower fold-out.
        // Each stage rests as one verdict line and expands to its full view (Ben,
        // 2026-08-01). The wizard is where the fold idiom is TAUGHT — it is the first
        // surface a player meets, so the gesture is learned before the first ledger
        // opens. It also turns a long scroll into a readable chain: the round's
        // stages fit on one screen as verdicts, and the player opens the ones the
        // roll made interesting.
        if (planetology_round)
        {
            const ui::chain_round& cr = ui::chain_round_at(m_wiz_round);
            for (int s = static_cast<int>(cr.first); s <= static_cast<int>(cr.last); ++s)
                ui::draw_stage_fold(chart_src, static_cast<chain_stage>(s), m_ui,
                                    detail_surface::generation_stage);
        }
        else if (history_round)
        {
            // ── Round 4: Run, the wait, then the board (BL-829 / BL-830) ──
            //
            // THE TRANSPORT IS DELIBERATELY PLAIN. The wizard's standing premise —
            // *you set conditions here, you do not steer* — and the globe's own
            // no-input ruling both argue for the plainer answer, and BL-829 files
            // scrub/pause as a question to be settled by WATCHING rather than in
            // advance. So: it runs, and it can be run again from the start. No
            // pause and no scrub until Ben has watched one.
            if (m_wiz_history_future.valid())
            {
                // The wait, with its own content. A frozen pane for ninety seconds
                // is worse than a bar, not better (STARTUP.md § The wait is the
                // round) — so the pass says which link it is on, and the era
                // reports its own year counter while it runs.
                ImGui::BeginDisabled();
                ImGui::Button("Running the history...",
                              {ImGui::GetContentRegionAvail().x, 30.0f});
                ImGui::EndDisabled();
                ImGui::Spacing();

                int li = m_wiz_history_progress.label.load(std::memory_order_relaxed);
                if (li < 0 || li >= generation_stage_label_count) li = 0;
                dim_text(generation_stage_labels[li]);

                const int sub_total = m_wiz_history_progress.sub_total.load(
                    std::memory_order_relaxed);
                if (sub_total > 0)
                {
                    const int sub_done = m_wiz_history_progress.sub_progress.load(
                        std::memory_order_relaxed);
                    ImGui::ProgressBar(
                        std::clamp(static_cast<float>(sub_done)
                                       / static_cast<float>(sub_total), 0.0f, 1.0f),
                        {ImGui::GetContentRegionAvail().x, 10.0f}, "");
                    std::snprintf(buf, sizeof buf, "year %d of %d", sub_done, sub_total);
                    dim_text(buf);
                }
            }
            else if (m_wiz_history.empty())
            {
                // NO RUN BUTTON (Ben, 2026-09-09: "we can retire the 'run'
                // button. Wire that to auto start when next is clicked in phase
                // 3"). Arriving on the round IS the instruction to run it —
                // there was never a second thing the player might have wanted
                // here, so the button asked a question with one answer.
                //
                // The launch itself is on the round-3 Next press rather than
                // here, so the pass is already under way by the time this frame
                // draws; see the navigation block below. This branch is only
                // reached if a run has not been started or has been cleared.
                dim_text("Four thousand years of claim and counter-claim, run here rather "
                         "than previewed: the history is the most expensive pass in the "
                         "project, and it cannot be re-rolled on every keystroke the way "
                         "the planetology rounds are.");
            }
            else
            {
                // Restart is the whole transport. It is not a toggle — it has no
                // active state to undo; it re-parks the playhead and plays.
                if (ImGui::Button("Restart##wizhistrestart",
                                  {ImGui::GetContentRegionAvail().x, 30.0f}))
                {
                    m_wiz_history_year    = m_wiz_history.lapse.start_year;
                    m_wiz_history_carry   = 0.0f;
                    m_wiz_history_playing = m_golden_dir.empty();
                }
                ImGui::Spacing();

                std::snprintf(buf, sizeof buf, "%s  -  %lld battles, %lld conquests, "
                                               "%lld foundings in the full run",
                              ui::lapse_year_label(m_wiz_history_year).c_str(),
                              static_cast<long long>(m_wiz_history.battles),
                              static_cast<long long>(m_wiz_history.conquests),
                              static_cast<long long>(m_wiz_history.foundings));
                dim_text(buf);
                ImGui::Separator();

                ui::draw_lapse_scoreboard(m_wiz_history, hist_slice, hist_lagged);

                ImGui::Spacing();
                // THE ONE THING THIS ROUND CANNOT DO YET, said in as many words
                // rather than left for a player to discover. `world_params` carries
                // no per-pass era seed, so re-running the history off the same
                // planetology reproduces it exactly; folding the round's reroll into
                // `params.seed` would re-draw the planetology rounds above it, which
                // rounds-are-causal forbids in that direction.
                dim_text("Reroll plays the same ground through again. The land, the seas "
                         "and the peoples are the ones you chose; what changes is the "
                         "four thousand years that ran over them.");
            }
        }
        else
        {
            // Round 5's real chart surface — the substrate readout — arrives with its
            // pass. Until then the round says so in as many words rather than showing
            // an empty column.
            draw_pass_round_placeholder(pass_index);
        }

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
        //    everything downstream of it, because the chain is causal); the preview
        //    pane repaints from the same roll, so what changed is SEEN. The seed
        //    still names a determinate family: (seed, leans, roll counters) is the
        //    whole input, and the same triple always returns the same world. ──
        const bool  last  = (m_wiz_round == wizard_round_count - 1);
        const float bar_w = ImGui::GetContentRegionAvail().x;

        // Reroll full-width and first — it is the wizard's main verb now.
        if (ImGui::Button("Reroll##wizroll", {bar_w, 34.0f}))
        {
            if (planetology_round)
            {
                ++pf.roll[m_wiz_round];
                m_wiz_dirty = true;
            }
            else
            {
                // Each pass round keeps its OWN reroll (Ben, 2026-09-08): the 4000
                // years can be rerolled, and the focused 400-year economy pass is its
                // own page with its own run and reroll.
                ++m_wiz_pass_roll[pass_index];
                m_wiz_pass_current[pass_index] = false; // re-run, not yet accepted
                invalidate_wizard_rounds_below(m_wiz_round);
                // Round 4 rerolls by RE-RUNNING the pass, not by re-drawing a
                // cached one (STARTUP.md § The 4000 years can be rerolled) — which
                // is the whole reason the wait had to be worth watching rather than
                // merely tolerable. See the caption on the round for the one thing
                // this cannot yet vary.
                if (history_round && !m_wiz_history_future.valid())
                {
                    // A DIFFERENT FOUR THOUSAND YEARS OVER THE SAME GROUND
                    // (Ben, 2026-09-09: "reroll should produce differences
                    // regardless"). The era carries its own seed now, so this
                    // moves the history without touching the planetology rounds
                    // above it — folding the roll into `params.seed` would
                    // re-draw the star and the surface, which rounds-are-causal
                    // forbids in that direction. See world_params::era_seed.
                    ++m_pending_world_params.era_seed;
                    m_wiz_history = ui::history_lapse{};
                    launch_wizard_history_run();
                }
            }
        }

        // Back always steps out one level, and the level outside round 0 is the main
        // menu — a wizard the player cannot leave is a trap (nothing is generated
        // until "Begin", so leaving costs nothing). Preferences survive the trip, so
        // re-entering resumes the same leans from round 0.
        const float half = (bar_w - style.ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Back##wizback", {half, 34.0f}))
        {
            if (m_wiz_round == 0)
                m_screen = app_screen::menu;
            else
                --m_wiz_round;
        }
        ImGui::SameLine();
        // "Begin" is the wizard's ONLY generating press and it belongs on the LAST
        // round alone (Ben, 2026-09-08: "in place of begin we should see next").
        // Rounds 1-4 advance; only round 5 commits.
        if (ImGui::Button(last ? "Begin##wizgo" : "Next##wizgo", {half, 34.0f}))
        {
            if (last)
                begin_new_game(); // async since 2026-08-12 — see app::begin_new_game
            else
            {
                ++m_wiz_round;
                // THE HISTORY STARTS WHEN THE PLAYER ARRIVES, NOT WHEN THEY ASK
                // (Ben, 2026-09-09: "wire that to auto start when next is
                // clicked in phase 3"). Retiring the Run button means the press
                // that MOVES ONTO the round is the press that begins it, so the
                // pass is already under way while the round's first frame draws
                // — which is the difference between a wait that started when
                // you arrived and one that started when you found the button.
                //
                // Guarded on there being nothing already in flight or landed, so
                // stepping Back to round 3 and forward again does not throw away
                // a finished history and re-run it.
                if (m_wiz_round == wizard_planetology_round_count
                    && m_wiz_history.empty() && !m_wiz_history_future.valid())
                    launch_wizard_history_run();
            }
        }
        ImGui::EndChild(); // ##wiz_left

        // ── Right two-thirds: what this roll LOOKS like. A stylised painting read
        //    straight from the preview states — the system in round 0, the homeworld
        //    surface in round 1, its industrial history in round 2. ──
        ImGui::SameLine();
        ImGui::BeginChild("##wiz_preview", {0.0f, 0.0f}, false,
                          ImGuiWindowFlags_NoBackground);
        if (history_round)
        {
            // ── Round 4 replaces the globe with a 2D MAP (Ben, 2026-09-08, at the
            //    live app). A globe shows a world; a map shows a FRONTIER, and the
            //    frontier is this round's whole subject — a border that stalls at a
            //    strait reads as a stall on a map and as foreshortening on a sphere.
            //    It inherits the globe's no-input rule: nothing here is a widget. ──
            if (m_wiz_history.empty())
            {
                ImGui::Dummy({0.0f, ImGui::GetContentRegionAvail().y * 0.45f});
                ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
                ImGui::TextWrapped(
                    m_wiz_history_future.valid()
                        ? "  The history is running. The map fills in when it lands."
                        : "  The history has not been run for this world yet.");
                ImGui::PopStyleColor();
            }
            else
            {
                ui::draw_lapse_map(m_wiz_history, hist_slice, m_wiz_history_year);
            }
        }
        else
        {
            std::vector<ui::preview_body> pv;
            pv.reserve(static_cast<std::size_t>(n_bodies));
            for (int i = 0; i < n_bodies; ++i)
            {
                const body_inputs& bi = prototype_body(i);
                pv.push_back(ui::preview_body{
                    m_wiz_names.bodies[static_cast<std::size_t>(i)].c_str(), // BL-257
                    bi.orbit_au, bi.mass_earths, bi.parent_orbit_au,
                    bi.is_homeworld, &m_wiz_preview[static_cast<std::size_t>(i)] });
            }
            // Kepler turns slowly — one revolution per minute, wall-clock — so
            // the far hemisphere can be read too. Frozen under --verify
            // (m_golden_dir set): a golden capture must never race an animation.
            const float rot = m_golden_dir.empty()
                ? static_cast<float>(std::fmod(ImGui::GetTime() / 60.0, 1.0)) * 6.2831853f
                : 0.0f;
            const ui::preview_surface_view surf{
                home_grid_width, home_grid_height,
                m_wiz_surface.size() == static_cast<std::size_t>(home_grid_width)
                                            * static_cast<std::size_t>(home_grid_height)
                    ? m_wiz_surface.data() : nullptr };
            ui::draw_generation_preview(pv.data(), pv.size(),
                                        m_wiz_resolved.params, m_wiz_round, rot, surf);

            // The honest wait note: while a build is in flight the globe still
            // shows the PREVIOUS roll's surface (or the stylised stand-in on
            // the very first frames).
            if (m_wiz_surface_future.valid() && m_wiz_round > 0)
            {
                ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 28.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 128, 145, 255));
                ImGui::TextUnformatted("  resolving the surface...");
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();

    // --autostart-windowed: walk the wizard as a player would — dwell ~20 frames
    // a round (so refresh_wizard_preview and the async surface build both run for
    // every round), then press Begin from HERE, inside the wizard's own frame,
    // which is the call site the real button uses.
    if (m_autostart_wizard >= 0)
    {
        ++m_autostart_wizard;
        // A player rerolls; a walk that never does stays inside the default
        // seed's resolved family and misses every reroll-dependent world.
        if (m_autostart_wizard % 20 == 10)
        {
            // GATED ON THE PLANETOLOGY ROUNDS, exactly as the Reroll button is.
            // `roll` is uint32_t[3] and is the last member of world_preferences,
            // itself the last member of world_params — so indexing it with a
            // round of 3 or 4 wrote past the end of m_pending_world_params into
            // whatever followed it. The Reroll path was gated when the wizard
            // grew to five rounds (BL-816); this driver was missed, and it is
            // the path nobody eyeballs, which is why it survived.
            if (m_wiz_round < wizard_planetology_round_count)
            {
                ++m_pending_world_params.preferences.roll[m_wiz_round];
                m_wiz_dirty = true;
            }
        }
        if (m_autostart_wizard % 20 == 0)
        {
            if (m_wiz_round < wizard_round_count - 1)
            {
                ++m_wiz_round;
                m_wiz_dirty = true;
            }
            else
            {
                std::printf("[autostart-windowed] wizard walked; pressing Begin\n");
                std::fflush(stdout);
                m_autostart_wizard = -1;
                begin_new_game();
            }
        }
    }
}
