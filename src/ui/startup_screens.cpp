/// @file startup_screens.cpp
/// The app's entry screens (docs/ui/STARTUP.md): the main menu, the New World
/// wizard (BL-167), and the wizard's preview plumbing. Extracted verbatim from
/// app.cpp (BL-361); behaviour unchanged — these remain app member functions,
/// they simply live beside the other ui surfaces now.

#include "core/app.hpp"

#include <imgui.h>

#include "ui/detail_level.hpp"
#include "ui/foldout_column.hpp"     // foldout_scroll_child — BL-904's wizard-column scroll verb
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

    // THE EMPIRES ROUND STARTS AT 400 BCE (BL-871, revising BL-846's 4000;
    // Ben, 2026-09-09). `era_minus_one_sim_params`'s single-span branch
    // (`era_minus_one.cpp`) derives the sim's own start year as
    // `epoch_year - prehistory_years`; at this wizard's epoch (0 CE — the
    // "ancient refocus" default, NR-177) that means `prehistory_years` IS the
    // number of years before 0 CE the sim starts at. 400 lands it at exactly
    // 400 BCE, the year `CIVILISATION.md` § The span is 400 BCE to 1200 CE
    // hands the Empires round.
    //
    // NOT SIXTEEN HUNDRED YEARS, AND THAT IS A KNOWN GAP, NOT AN OVERSIGHT.
    // The design's full arithmetic is 400 BCE -> 1200 CE, 1,600 years — but
    // reaching 1200 CE needs an epoch past it, and this wizard's epoch is
    // still 0 CE: round 5 (Industrialisation) and pass 2's 1560 -> 1960 span
    // (`GENERATION_STRATEGY.md` § Pass 2 is the economy pass) are not built
    // yet (`draw_pass_round_placeholder`), so nothing today can watch the sim
    // run past 0 CE. CIVILISATION.md's own words: "sixteen hundred years is
    // the constraint on the phase going forward, not something to solve in
    // this item." What BL-871 owes is the SPLIT and the STARTING YEAR the
    // Culture round hands off at; the full depth is follow-on work once the
    // epoch moves.
    //
    // THE MIGRATION IS NOT COUNTED HERE. It used to be: the OLD figure (4000)
    // put the sim's own start at roughly 4000 BCE, so a single continuous run
    // covered colonisation AND conquest and both wizard rounds replayed it.
    // BL-871 splits them — the migration now runs to its own derived end year
    // (`colonisation_start_year`, colonisation.hpp, -2400) and the world
    // COASTS from there to 400 BCE holding what it left behind, so the sim
    // itself only ever needs to start where the Empires round does.
    //
    // THIS IS ALSO WHAT "BEGIN" BUILDS (unchanged from before BL-846): the
    // struct default already IS 400, so this line is written for clarity
    // against the new arithmetic rather than for a numeric change.
    m_pending_world_params.prehistory_years = 400;
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

void app::launch_wizard_history_run(int lapse_index)
{
    // Out-of-range is a caller bug, not a display state: the array index below is
    // the one thing here that cannot be clamped away silently.
    if (lapse_index < 0 || lapse_index >= wizard_lapse_round_count) return;
    if (m_wiz_history_future[lapse_index].valid()) return; // already running on this round

    m_wiz_history[lapse_index]         = ui::history_lapse{};
    m_wiz_history_playing[lapse_index] = false;
    m_wiz_history_carry[lapse_index]   = 0.0f;

    // The wait's own content. Cleared FIRST so no frame can read the previous
    // run's pass split as this one's — the same ordering begin_new_game keeps.
    generation_progress& prog = m_wiz_history_progress[lapse_index];
    prog.stage.store(0, std::memory_order_relaxed);
    prog.label.store(0, std::memory_order_relaxed);
    prog.stage_count.store(generation_stage_label_count, std::memory_order_relaxed);
    prog.sub_progress.store(0, std::memory_order_relaxed);
    prog.sub_total.store(0, std::memory_order_relaxed);

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
            m_wiz_history[lapse_index]      = std::move(adopted);
            m_wiz_history_year[lapse_index] = m_wiz_history[lapse_index].lapse.start_year;
            return;
        }
    }

    // Lua and the works table are read on THIS thread before the worker starts:
    // sol2 is not thread-safe, and `m_works` is generation's input (BL-321).
    m_lua.load("scripts/world_gen.lua");
    world_gen_config cfg{};
    cfg.load_from_lua(m_lua);
    ensure_works_loaded();

    // STOP WHERE THIS ROUND'S OWN SPAN ENDS, AND NO FURTHER (BL-871). The two
    // rounds are no longer one fused pass replayed twice: round 3 (Culture,
    // lapse_index 0) wants the migration's own record and must stop BEFORE
    // the Empires round's history sim ever starts, while round 4 (Empires,
    // lapse_index 1) wants that sim's record and stops once IT has run, before
    // borders, roads and companies — stages 9-12 — are computed and thrown
    // away (measured 10,805 ms of 11,316, about 95% of the wait, and why the
    // round visibly hung on "Laying roads", Ben, 2026-09-09).
    //
    // Note this is set on the COPY the worker takes, never on the campaign's:
    // `begin_new_game` builds a whole world from its own config, and a world
    // stopped at either point has no nations, roads or corporations in it.
    world_gen_config hist_cfg = cfg;
    if (lapse_index == 0) hist_cfg.stop_after_migration   = true;
    else                  hist_cfg.stop_after_ancient_era = true;

    auto run = [this, hist_cfg, lapse_index, params = m_pending_world_params]() {
        generation_report rep;
        // The world itself is DISCARDED. What the round wants is the era it
        // recorded, and holding the world would only invite a second, divergent
        // copy of the campaign's own.
        (void)make_hard_coded_world(params, &rep, hist_cfg,
                                    &m_wiz_history_progress[lapse_index], &m_works);
        return lapse_from_report(rep);
    };

    if (!m_golden_dir.empty())
        m_wiz_history_future[lapse_index] = std::async(std::launch::deferred, run);
    else
        m_wiz_history_future[lapse_index] = std::async(std::launch::async, run);

    // A deferred future never becomes ready on its own, so a capture path
    // resolves it here and now rather than spinning forever in poll.
    if (!m_golden_dir.empty())
        poll_wizard_history();
}

void app::poll_wizard_history()
{
    // Every lapse round, not just the one on screen: a player who presses Next
    // while round 4 is still running must not strand its worker's result — the
    // future is adopted wherever the wizard has got to by the time it lands.
    for (int i = 0; i < wizard_lapse_round_count; ++i)
    {
        if (!m_wiz_history_future[i].valid())
            continue;
        if (m_golden_dir.empty()
            && m_wiz_history_future[i].wait_for(std::chrono::seconds(0))
                   != std::future_status::ready)
            continue;
        ui::history_lapse landed = m_wiz_history_future[i].get();
        if (m_wiz_history_stale[i])
        {
            // The ground moved while this ran: it is a true history of a world the
            // player has already rerolled away from. Dropped rather than drawn.
            m_wiz_history_stale[i]   = false;
            m_wiz_history[i]         = ui::history_lapse{};
            m_wiz_history_playing[i] = false;
            continue;
        }
        m_wiz_history[i]      = std::move(landed);
        m_wiz_history_year[i] = m_wiz_history[i].lapse.start_year;
        // It plays the moment it lands: the run was the wait, and the playback is
        // what arriving on the round asked for. Frozen under --verify, where the
        // year is set by the script instead (verify.history_year).
        m_wiz_history_playing[i] = m_golden_dir.empty();
        m_wiz_history_carry[i]   = 0.0f;
    }
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

        // THE MENU OPENS ON A ROLLED SEED (BL-890; Ben, 2026-09-10). The
        // default of 0 made every new game the same reference world unless the
        // player thought to press Roll, which turns rerolling into a thing you
        // must know to do rather than the ordinary way in. STARTUP.md carries
        // the ruling.
        //
        // ROLLED HERE, AND ONCE, FOR TWO REASONS THE LIVE CHECK FOUND. Rolling
        // per frame would make the field impossible to type into. Rolling when
        // the WIZARD opens -- the first place this was tried -- silently threw
        // away a seed the player had just typed or pasted into this very field,
        // because New Game runs that path on the way out of this screen. The
        // latch keeps the draw idempotent: the number is fresh on arrival and
        // is then the player's.
        //
        // THE ENTROPY STOPS HERE, exactly as the Roll button's below already
        // does. world/* stays a pure function of the seed, so save, replay and
        // the multiplayer argument are untouched; seed 0 still names the
        // reference world, it is simply no longer what you get by accident.
        //
        // NOT UNDER --verify. Every scripted capture reaches this menu, and a
        // rolled seed would make each run a different world and turn the visual
        // suite non-deterministic. `m_golden_dir` non-empty is this file's own
        // test for "a harness is driving".
        if (!m_seed_rolled)
        {
            m_seed_rolled = true;
            if (m_golden_dir.empty())
            {
                std::random_device rd_seed;
                wp.seed = static_cast<uint32_t>(rd_seed());
            }
        }

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
constexpr int planetology_rounds = 2;  // System, Life (BL-863)
constexpr int pass_rounds        = 3;  // Culture, Empires, Industrialisation
constexpr int lapse_rounds       = 2;

/// Empires' historical-turbulence caption, shared between the layout-height
/// estimate below and the actual draw call in the round switch, so the two
/// can never drift apart the way the fixed-line-count guess did (BL-904).
constexpr const char* kTurbulenceCaption =
    "Calm: peoples differ less, neighbours let a riser rise, and "
    "distance is cheap to hold. Turbulent: the warlike are more so, "
    "a riser draws a coalition, and an over-reached empire cannot "
    "feed what it took. It leans the forces - it sets no number of "
    "realms, and either setting can surprise you.";

/// Culture (round 2) and Empires (round 3) are PASS rounds, not planetology
/// rounds, but each still carries exactly one lean row of its own -- Drawdown
/// (moved here by BL-863) and the historical-turbulence lean (BL-839). The old
/// `r >= planetology_rounds` guard zeroed both out, under-reserving the layout
/// by one row and pushing each round's preference block and Next/Back footer
/// below the visible fold at 1080p (BL-904, found via history_lapse_press.lua
/// and round4_arc_reach.lua going red).
int round_pref_count(int r)
{
    if (r == 0) return 4;
    if (r == 1) return 3;
    if (r == 2 || r == 3) return 1;
    return 0;
}
int round_note_lines(int r)
{
    if (r == 1) return 3; ///< B carries the iron/coal caption.
    if (r == 0) return 1;
    // Culture's Drawdown carries no caption. Empires' turbulence caption is
    // far longer than a fixed line count can safely predict, so its height is
    // measured directly against kTurbulenceCaption where decide_h is built.
    return 0;
}

/// A round's header text. The planetology rounds take theirs from the shared chain
/// table (so the wizard and the History ledger name them identically); the three
/// pass rounds are not chain rounds and carry their own, from STARTUP.md
/// § Rounds 4, 5 and 6.
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
        // THE ROUNDS ARE NOT CONTINUOUS (Ben, 2026-09-09). Round 4 used to fuse
        // the peopling of the world with the empires that followed, and that cut
        // failed twice over: it drew conquest with the migration already finished
        // off-screen, then — once migration moved inside it — migration with no
        // conquest at all. Two subjects, two rounds.
        { "Culture",
          "Who reached this ground first, and by which routes?" },
        { "Empires",
          // THIS ROUND'S OWN SPAN, SEPARATE FROM ROUND 3's (BL-871). Before the
          // split both rounds replayed the same fused record — colonisation and
          // conquest run together — so round 4 opened with the whole map
          // already claimed and had nothing left to show but the tail of one
          // pass. Now round 3 stops at the migration's own end and round 4
          // starts the Empires sim at 400 BCE
          // (`docs/generation/CIVILISATION.md` § The span is 400 BCE to
          // 1200 CE), so this round watches conquest from a world that is
          // freshly peopled rather than one already settled off-screen.
          //
          // Titled to what it plays rather than left aspirational, on the rule
          // that a surface must not assert something the code has not delivered.
          "Who claimed this ground, and who lost it, in the age before the epoch?" },
        { "Industrialisation",
          "What does that ground produce, and who trades it?" },
    };
    int i = r - planetology_rounds;
    if (i < 0)             i = 0;
    if (i >= pass_rounds)  i = pass_rounds - 1;
    return passes[i];
}

/// The honest placeholder a pass round rests as until its pass is built. Labelled as
/// a placeholder in as many words: an unlabelled empty pane reads as a finished
/// surface, and the next session believes it.
///
/// Only round 6 reaches this now — rounds 4 and 5 both play a real record — so it
/// no longer branches on which pass round asked.
void draw_pass_round_placeholder()
{
    constexpr ImU32 col_dim   = IM_COL32(120, 128, 145, 255);
    constexpr ImU32 col_label = IM_COL32(205, 170, 90, 255);

    ImGui::PushStyleColor(ImGuiCol_Text, col_label);
    ImGui::TextUnformatted("PLACEHOLDER - this round is not built yet");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
    ImGui::TextWrapped(
        "Round 6 will run the economy pass: 1560 to 1960, then the substrate carve - "
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

/// The turbulence lean's own row (BL-839), and NOT `lean_row` above.
///
/// THREE OPTIONS, NOT FOUR, and that is the whole reason this is a separate
/// function. `lean_row` leads with "Any", which means "sample the whole viable
/// range" -- honest for a planetology preference, which names a VALUE drawn
/// from a distribution. This axis names a FORCE (`world/history_sim.hpp` sec
/// THE HISTORICAL TURBULENCE LEAN), and there is no distribution of forces to
/// sample, so an "Any" the resolver would silently read as "Ordinary" would be
/// a control that lies about what it does.
///
/// NAMED SETTINGS RATHER THAN A SLIDER, for the reason STARTUP.md sec Rounds 4,
/// 5 and 6 gives the wizard as a whole: the player has to be able to tell what
/// they rolled. "Turbulent" is a thing you can look for in the arc readout
/// above; a per-mille dial on a force nobody can see is not.
bool turbulence_row(lean& value)
{
    static constexpr lean order[3] = { lean::low, lean::mid, lean::high };
    const char* names[3] = { "Calm", "Ordinary", "Turbulent" };

    ImGui::PushID("turbulence");
    ImGui::TextUnformatted("History");

    bool changed = false;
    for (int i = 0; i < 3; ++i)
    {
        if (i > 0) ImGui::SameLine();
        int v = static_cast<int>(value);
        // `any` can arrive here from an old save or a bare `world_params`; the
        // resolver reads it as ordinary, so the row shows it that way rather
        // than lighting nothing and implying a fourth state.
        if (value == lean::any) v = static_cast<int>(lean::mid);
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
    // AT LEAST ONE COUNTER PER ROUND (BL-863). `roll` keeps THREE rather than
    // shrinking with the round count: it is on the save format
    // (save_envelope_roundtrip asserts 8 leans + roll[3] survive), and shrinking
    // it would make a UI reorder a save-format change. The spare counter is not
    // dead -- it is the drawdown lean's, for when round 5 takes it.
    static_assert(sizeof(world_preferences::roll)
                      >= sizeof(uint32_t)
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
    // COVERS AT LEAST, NOT EXACTLY (BL-863). The chart chain keeps all THREE
    // groups because tile_inspector.cpp's History ledger reads the same table --
    // dropping the third would delete the legacy/spend charts from the in-game
    // ledger, which is not what retiring a WIZARD round asked for. The wizard
    // walks the first two; the invariant that matters is that every round it
    // hands to chain_round_at has an entry.
    static_assert(ui::chain_round_count >= wizard_planetology_round_count,
                  "the chart chain must cover at least the wizard's planetology rounds");
    static_assert(wizard_planetology_round_count < wizard_round_count,
                  "the planetology rounds are a strict prefix of the wizard's rounds");
    static_assert(planetology_rounds == wizard_planetology_round_count
                      && pass_rounds == wizard_pass_round_count
                      && lapse_rounds == wizard_lapse_round_count,
                  "the file-local round-count mirrors must track app's constants");
    static_assert(wizard_lapse_round_count <= wizard_pass_round_count,
                  "the lapse rounds are a prefix of the pass rounds");

    // Which kind of round is on screen. The planetology rounds preview a pure chain
    // per keystroke; the pass rounds cannot (STARTUP.md § The wait is the round).
    const bool planetology_round = (m_wiz_round < wizard_planetology_round_count);
    const int  pass_index        = m_wiz_round - wizard_planetology_round_count;

    // ── A LAPSE ROUND'S playback, advanced once per frame and read TWICE — the
    //    board on the left and the map on the right must show the same instant, so
    //    the slice is materialised here rather than in each of them. Rounds 4 and 5
    //    are both lapse rounds and each owns its own record slot. ──
    const bool lapse_round =
        (!planetology_round && pass_index < wizard_lapse_round_count);
    const int  lapse_index = lapse_round ? pass_index : 0;
    std::vector<uint16_t> hist_slice, hist_lagged;
    if (lapse_round && !m_wiz_history[lapse_index].empty())
    {
        // The land mask comes from the wizard's OWN packed surface — the same
        // raster the globe samples and the same one "Begin" builds — so the
        // coastline a frontier stalls at is the coastline the campaign has. It is
        // a no-op until that async build lands; the round simply retries.
        ui::history_lapse& rec = m_wiz_history[lapse_index];
        ui::finish_history_lapse(rec,
                                 m_wiz_surface.empty() ? nullptr : m_wiz_surface.data(),
                                 m_wiz_surface.size());

        const int first = rec.lapse.start_year;
        const int last  = first + rec.lapse.years;

        // FROZEN UNDER --verify, for the reason the globe's rotation is: a capture
        // must never race an animation. The year is then whatever the script set.
        if (m_wiz_history_playing[lapse_index] && m_golden_dir.empty())
        {
            // The rate follows the SPAN rather than being a constant, the same
            // derivation the History ledger's Ages view makes: the recorded era is
            // 400 years today and the round is written against 4000, and a fixed
            // years-per-second would empty the transport in a blink on one of them.
            const float span = static_cast<float>(last - first);
            constexpr float run_secs = 30.0f;
            const float rate = span > 0.0f ? span / run_secs : 1.0f;
            m_wiz_history_carry[lapse_index] += ImGui::GetIO().DeltaTime * rate;
            const int whole = static_cast<int>(m_wiz_history_carry[lapse_index]);
            if (whole > 0)
            {
                m_wiz_history_carry[lapse_index] -= static_cast<float>(whole);
                m_wiz_history_year[lapse_index]  += whole;
            }
            if (m_wiz_history_year[lapse_index] >= last)
            {
                m_wiz_history_year[lapse_index]    = last;
                m_wiz_history_playing[lapse_index] = false;
            }
        }
        int& year = m_wiz_history_year[lapse_index];
        if (year < first) year = first;
        if (year > last)  year = last;

        hist_slice = owner_slice_at(rec.lapse, year);
        // The lagged board, for the entry/exit marks. A twelfth of the span back:
        // far enough that a rank move means something, near enough that the marks
        // are not permanently lit.
        const int lag = std::max(1, (last - first) / 12);
        hist_lagged = owner_slice_at(rec.lapse, year - lag);
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
        // BL-904: gives verify.scroll_panel("wizard", ...) a real scroller to
        // aim at. The column is a plain BeginChild, not a foldout ledger, but
        // `foldout_scroll_child` only matches on the id string it is handed,
        // so it works here unchanged.
        ui::foldout_scroll_child("##wiz_left");

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
        float decide_h = static_cast<float>(round_pref_count(m_wiz_round))
                             * (line_h + 2.0f * (frame_h + style.ItemSpacing.y))
                       + line_h * static_cast<float>(round_note_lines(m_wiz_round)
                                                     + (m_wiz_resolved.gave_up ? 2 : 0))
                       + style.ItemSpacing.y * 3.0f;
        // Empires' turbulence lean is `turbulence_row`, not `lean_row` — ONE row
        // of three radios (Calm/Ordinary/Turbulent), not the 2x2 grid
        // round_pref_count's generic multiplier assumes for every other lean.
        // Correct that one row back out, then add the caption's real height,
        // measured rather than guessed (BL-904): it runs to five sentences,
        // well past what a fixed line count could safely predict at every
        // column width the wizard can be shown at.
        if (m_wiz_round == 3)
            decide_h += ImGui::CalcTextSize(kTurbulenceCaption, nullptr, false,
                                             ImGui::GetContentRegionAvail().x).y
                      + style.ItemSpacing.y
                      - (frame_h + style.ItemSpacing.y);
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
        else if (lapse_round)
        {
            // ── Rounds 4 and 5: the wait, then the board (BL-829 / BL-830 / BL-860) ──
            //
            // THE TRANSPORT IS DELIBERATELY PLAIN. The wizard's standing premise —
            // *you set conditions here, you do not steer* — and the globe's own
            // no-input ruling both argue for the plainer answer, and BL-829 files
            // scrub/pause as a question to be settled by WATCHING rather than in
            // advance. So: it runs, and it can be run again from the start. No
            // pause and no scrub until Ben has watched one.
            ui::history_lapse& rec = m_wiz_history[lapse_index];
            if (m_wiz_history_future[lapse_index].valid())
            {
                // The wait, with its own content. A frozen pane for ninety seconds
                // is worse than a bar, not better (STARTUP.md § The wait is the
                // round) — so the pass says which link it is on, and the era
                // reports its own year counter while it runs.
                ImGui::BeginDisabled();
                ImGui::Button(lapse_index == 0 ? "Running the migration..."
                                               : "Running the history...",
                              {ImGui::GetContentRegionAvail().x, 30.0f});
                ImGui::EndDisabled();
                ImGui::Spacing();

                const generation_progress& prog = m_wiz_history_progress[lapse_index];
                int li = prog.label.load(std::memory_order_relaxed);
                if (li < 0 || li >= generation_stage_label_count) li = 0;
                dim_text(generation_stage_labels[li]);

                const int sub_total = prog.sub_total.load(std::memory_order_relaxed);
                if (sub_total > 0)
                {
                    const int sub_done = prog.sub_progress.load(
                        std::memory_order_relaxed);
                    ImGui::ProgressBar(
                        std::clamp(static_cast<float>(sub_done)
                                       / static_cast<float>(sub_total), 0.0f, 1.0f),
                        {ImGui::GetContentRegionAvail().x, 10.0f}, "");
                    std::snprintf(buf, sizeof buf, "year %d of %d", sub_done, sub_total);
                    dim_text(buf);
                }
            }
            else if (rec.empty())
            {
                // NO RUN BUTTON (Ben, 2026-09-09: "we can retire the 'run'
                // button. Wire that to auto start when next is clicked in phase
                // 3"). Arriving on the round IS the instruction to run it —
                // there was never a second thing the player might have wanted
                // here, so the button asked a question with one answer.
                //
                // The launch itself is on the PREVIOUS round's Next press rather
                // than here, so the pass is already under way by the time this
                // frame draws; see the navigation block below. This branch is
                // only reached if a run has not been started or has been cleared.
                dim_text(lapse_index == 0
                    ? "The peopling of an empty world, run here rather than previewed: "
                      "the pass behind it is the most expensive in the project, and it "
                      "cannot be re-rolled on every keystroke the way the planetology "
                      "rounds are."
                    : "Claim and counter-claim from 400 BCE, run here rather than "
                      "previewed: the history is the most expensive pass in the "
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
                    m_wiz_history_year[lapse_index]    = rec.lapse.start_year;
                    m_wiz_history_carry[lapse_index]   = 0.0f;
                    m_wiz_history_playing[lapse_index] = m_golden_dir.empty();
                }
                ImGui::Spacing();

                std::snprintf(buf, sizeof buf, "%s  -  %lld battles, %lld conquests, "
                                               "%lld foundings in the full run",
                              ui::lapse_year_label(m_wiz_history_year[lapse_index]).c_str(),
                              static_cast<long long>(rec.battles),
                              static_cast<long long>(rec.conquests),
                              static_cast<long long>(rec.foundings));
                dim_text(buf);
                ImGui::Separator();

                ui::draw_lapse_scoreboard(rec, hist_slice, hist_lagged);
                // BL-891: the arc, so a rolled world can be judged without
                //         watching the whole replay.
                ui::draw_lapse_arc(rec);

                ImGui::Spacing();
                // WHAT THIS ROUND IS AND IS NOT (BL-871, then BL-906). The two
                // rounds stop at different points — the migration at its own
                // derived end year, the Empires sim at 1200 CE — so they do
                // not replay the same recorded age. BL-906 (2026-09-11) closed
                // the gap this note used to name: the Empires round now runs
                // its full 400 BCE -> 1200 CE span, decoupled from the epoch
                // (`CIVILISATION.md` § The span is 400 BCE to 1200 CE). Round
                // 5 / Industrialisation is still a placeholder, but that no
                // longer bears on this round's own span.
                if (lapse_index == 1)
                    dim_text("This round's own span, separate from round 3's migration — "
                             "the full 400 BCE to 1200 CE the design asks for.");
            }
        }
        else
        {
            // Round 6's real chart surface — the substrate readout — arrives with its
            // pass. Until then the round says so in as many words rather than showing
            // an empty column.
            draw_pass_round_placeholder();
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

            // THE EMPIRES ROUND TAKES THE TURBULENCE LEAN (BL-839; Ben,
            // 2026-09-08). It is sited on the round whose own pass it leans, so
            // the control and the thing it moves are on the same screen: set it,
            // roll, and the arc readout beside it is the answer.
            //
            // IT SETS CONDITIONS, IT DOES NOT STEER. The three settings move the
            // spread of culture aggression, how sharply neighbours coalesce
            // against a riser, and how fast reach decays -- and not one of them
            // names, targets or corrects a number of realms. A calm world that
            // fragments anyway is a correct calm world. See
            // `world/history_sim.hpp` sec THE HISTORICAL TURBULENCE LEAN.
            case 3:
                if (turbulence_row(pf.history_turbulence))
                {
                    m_wiz_dirty = true;
                    // THIS ROUND'S OWN RECORD GOES TOO, which is why the
                    // argument is `m_wiz_round - 1` and not `m_wiz_round`. The
                    // setting is an INPUT to the pass this round runs, so a
                    // record made under the previous setting is an account of a
                    // history the player has just stopped asking for -- exactly
                    // the silent failure `invalidate_wizard_rounds_below` was
                    // wired ahead of the passes to prevent, one round earlier
                    // than the reroll button needs it.
                    invalidate_wizard_rounds_below(m_wiz_round - 1);
                }
                dim_text(kTurbulenceCaption);
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
                // A lapse round rerolls by RE-RUNNING its pass, not by re-drawing
                // a cached one (STARTUP.md § Each pass round is rerollable) —
                // which is the whole reason the wait had to be worth watching
                // rather than merely tolerable.
                if (lapse_round && !m_wiz_history_future[lapse_index].valid())
                {
                    // A DIFFERENT AGE OVER THE SAME GROUND (Ben, 2026-09-09:
                    // "reroll should produce differences regardless"). The era
                    // carries its own seed now, so this moves the recorded age
                    // without touching the planetology rounds above it — folding
                    // the roll into `params.seed` would re-draw the star and the
                    // surface, which rounds-are-causal forbids in that direction.
                    // See world_params::era_seed.
                    ++m_pending_world_params.era_seed;
                    m_wiz_history[lapse_index] = ui::history_lapse{};
                    launch_wizard_history_run(lapse_index);
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
        // Rounds 1-5 advance; only round 6 commits.
        if (ImGui::Button(last ? "Begin##wizgo" : "Next##wizgo", {half, 34.0f}))
        {
            if (last)
                begin_new_game(); // async since 2026-08-12 — see app::begin_new_game
            else
            {
                ++m_wiz_round;
                // A PASS STARTS WHEN THE PLAYER ARRIVES, NOT WHEN THEY ASK
                // (Ben, 2026-09-09: "wire that to auto start when next is
                // clicked in phase 3"). Retiring the Run button means the press
                // that MOVES ONTO a round is the press that begins it, so the
                // pass is already under way while the round's first frame draws
                // — which is the difference between a wait that started when
                // you arrived and one that started when you found the button.
                //
                // EACH LAPSE ROUND STARTS ITS OWN (BL-860), rather than round 4
                // alone: the arrival is generic, so round 5 gets the same
                // treatment without a second special case.
                //
                // Guarded on there being nothing already in flight or landed on
                // THAT round, so stepping Back and forward again does not throw
                // away a finished record and re-run it.
                const int arrived = m_wiz_round - wizard_planetology_round_count;
                if (arrived >= 0 && arrived < wizard_lapse_round_count
                    && m_wiz_history[arrived].empty()
                    && !m_wiz_history_future[arrived].valid())
                    launch_wizard_history_run(arrived);
            }
        }
        ImGui::EndChild(); // ##wiz_left

        // ── Right two-thirds: what this roll LOOKS like. A stylised painting read
        //    straight from the preview states — the system in round 0, the homeworld
        //    surface in round 1, its industrial history in round 2. ──
        ImGui::SameLine();
        ImGui::BeginChild("##wiz_preview", {0.0f, 0.0f}, false,
                          ImGuiWindowFlags_NoBackground);
        if (lapse_round)
        {
            // ── The lapse rounds replace the globe with a 2D MAP (Ben, 2026-09-08,
            //    at the live app). A globe shows a world; a map shows a FRONTIER,
            //    and the frontier is these rounds' whole subject — a border that
            //    stalls at a strait reads as a stall on a map and as foreshortening
            //    on a sphere. It inherits the globe's no-input rule: nothing here is
            //    a widget. ──
            if (m_wiz_history[lapse_index].empty())
            {
                ImGui::Dummy({0.0f, ImGui::GetContentRegionAvail().y * 0.45f});
                ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
                const bool running = m_wiz_history_future[lapse_index].valid();
                if (lapse_index == 0)
                    ImGui::TextWrapped(running
                        ? "  The migration is running. The map fills in when it lands."
                        : "  The migration has not been run for this world yet.");
                else
                    ImGui::TextWrapped(running
                        ? "  The history is running. The map fills in when it lands."
                        : "  The history has not been run for this world yet.");
                ImGui::PopStyleColor();
            }
            else
            {
                ui::draw_lapse_map(m_wiz_history[lapse_index], hist_slice,
                                   m_wiz_history_year[lapse_index]);
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
