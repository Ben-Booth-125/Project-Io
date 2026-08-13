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
            m_wiz_surface[i] = static_cast<uint8_t>(scratch.tiles.at(tiles[i]).composition);
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
                comp[i] = static_cast<uint8_t>(scratch.tiles.at(tiles[i]).composition);
            return comp;
        });
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

    // Adopt a finished real-surface build (and chain a relaunch if the params
    // moved mid-build). Cheap zero-wait probe; runs every wizard frame.
    poll_wizard_surface();

    static_assert(ui::chain_round_count == wizard_round_count,
                  "the wizard's round count and the shared chain-round table must agree");

    const ui::chain_round& wr       = ui::chain_round_at(m_wiz_round);
    const int              n_bodies = std::min(static_cast<int>(m_wiz_preview.size()),
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
        for (int s = static_cast<int>(wr.first); s <= static_cast<int>(wr.last); ++s)
            ui::draw_stage_fold(chart_src, static_cast<chain_stage>(s), m_ui,
                                detail_surface::generation_stage);

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
            ++pf.roll[m_wiz_round];
            m_wiz_dirty = true;
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
        if (ImGui::Button(last ? "Begin##wizgo" : "Continue##wizgo", {half, 34.0f}))
        {
            if (last)
                begin_new_game(); // async since 2026-08-12 — see app::begin_new_game
            else
                ++m_wiz_round;
        }
        ImGui::EndChild(); // ##wiz_left

        // ── Right two-thirds: what this roll LOOKS like. A stylised painting read
        //    straight from the preview states — the system in round 0, the homeworld
        //    surface in round 1, its industrial history in round 2. ──
        ImGui::SameLine();
        ImGui::BeginChild("##wiz_preview", {0.0f, 0.0f}, false,
                          ImGuiWindowFlags_NoBackground);
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
            ++m_pending_world_params.preferences.roll[m_wiz_round];
            m_wiz_dirty = true;
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
