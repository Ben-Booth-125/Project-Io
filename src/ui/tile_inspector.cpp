#include "tile_inspector.hpp"
#include "text_fit.hpp"

#include "foldout_column.hpp"    // shell fold-out column host (BL-122/BL-144)
#include "format.hpp"            // campaign_epoch_year, cross-checked below (BL-220)
#include "generation_charts.hpp" // the chain-stage charts, shared with the wizard (BL-211)
#include "presentation.hpp"
#include "world/components.hpp"
#include "world/era_timelapse.hpp" // the recorded Era -1 replay the Ages view folds over

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace ui {

// BL-705 retires the static_assert that stood here. It read
//
//     static_assert(fmt::campaign_epoch_year == (int)::campaign_epoch_year)
//
// and was described as forcing "the tick calendar and the generated history" to
// count from the same year. They do not count from the same year, and never
// needed to: `::campaign_epoch_year` (now `::history_datum_year`) is the FIXED,
// SERIALISED datum `history_event::years_before_epoch` is measured against,
// while `fmt::campaign_epoch_year()` is the year the LIVE CAMPAIGN opens on —
// `world_params::epoch_year`, which is 0 by default and 1960 on the industrial
// start, and both of which are supported (ERAS.md § Where the ladder starts).
//
// Asserting them equal is what pinned the tick calendar at 1960 while every
// default campaign generated at 0 CE. The two are now separately named, the
// calendar reads the live world, and there is nothing left here to guard: the
// history side renders absolute calendar years off its own datum, so no
// agreement between the two is required for either to be right.

/// A calendar year as the Ages transport should read it.
///
/// The era runs 400 BCE -> 0 CE, so a bare "%d CE" prints "-400 CE" — a year
/// that does not exist, on the one control whose whole job is saying WHEN.
/// Kept local because it is the only surface in the project that scrubs a
/// signed calendar year; the moment a second one appears this belongs in fmt.
static std::string ages_year_label(int year)
{
    char buf[32];
    if (year < 0) std::snprintf(buf, sizeof buf, "%d BCE", -year);
    else          std::snprintf(buf, sizeof buf, "%d CE",   year);
    return buf;
}

void draw_tile_inspector(const world& w, ui_state& s,
                         const generation_report& report,
                         const world_params& gen_params, bool* p_open)
{
    // WHICH VIEW THIS PANEL LAST DREW, for the Tectonics lens arm below. File-static
    // rather than ui_state because it is a frame-to-frame edge detector, not state a
    // save or a verify script has any business reading.
    static int last_drawn_view = -1;

    // Honour the open flag; when closed the window draws nothing at all.
    if (p_open && !*p_open)
    {
        // Closed: forget the view we were on, so REOPENING onto Tectonics counts as
        // entering it and arms the lens again. Without this, a player who closed the
        // ledger on Tectonics, changed lens, and came back would get no arm.
        last_drawn_view = -1;
        return;
    }

    // Collect body IDs into a stable order for the combo box.
    std::vector<entity_id> body_ids;
    body_ids.reserve(w.bodies.size());
    for (const auto& [id, _] : w.bodies)
        body_ids.push_back(id);
    std::sort(body_ids.begin(), body_ids.end());

    // Re-hosted into the shell fold-out column (BL-144); closed via the nav rail.
    if (!ui::foldout_begin("Tile Ledger"))
    {
        ui::foldout_end();
        return;
    }

    if (body_ids.empty())
    {
        ImGui::TextDisabled("No bodies in world.");
        ui::foldout_end();
        return;
    }

    // --- Body selector ---
    // Default: prefer the canvas's active surface body, then fall back to lowest id.
    static entity_id selected_body = null_entity;
    if (selected_body == null_entity || w.bodies.find(selected_body) == w.bodies.end())
    {
        // 1. Use ui_state.active_body when it refers to a surface body.
        if (s.active_body != null_entity)
        {
            auto ab_it = w.bodies.find(s.active_body);
            if (ab_it != w.bodies.end() && ab_it->second.type != body_type::star)
            {
                selected_body = s.active_body;
            }
        }
        // 2. Fall back to the lowest-id surface body (existing default).
        if (selected_body == null_entity || w.bodies.find(selected_body) == w.bodies.end())
            selected_body = body_ids.front();
    }

    // --- Top-level view strip (BL-211, narrowed by BL-281) ---
    // Three questions, one per tab, sharing one premise — how this world came to be:
    // what happened here (Story), how the chain got there (Chain), and how the borders
    // moved to get here (Ages).
    //
    // A fourth tab, Tiles, was RETIRED 2026-08-03 (Ben, NEEDS_REVIEW NR-020 option 2):
    // it was a current-state readout inside a ledger about the past, and renaming it
    // would have fixed the label while keeping the defect. Its two sections already had
    // homes — buildings on the canvas and in the Selection element, market data in the
    // market surfaces.
    //
    // AGES SURVIVES THE NARROWING, though BL-281 says "drops to two views". The item was
    // designed 2026-08-03 and the Ages view landed 2026-08-05, so it is work the design
    // predates rather than work it judged: two thousand years of scrubbable borders is
    // squarely the ledger's own premise, and retiring it would be destroying a feature
    // nobody asked to remove.
    // BL-660 adds Tectonics as a fourth view — the destination a plate press
    // under the Continent lens routes to. Appended rather than inserted: the index
    // is parked in ui_state and read by verify scripts, so renumbering the
    // existing three would silently re-point every saved and scripted view (the
    // trap the Ages clamp below exists because of).
    // The indices live in tile_inspector.hpp so the canvas can route to one by
    // name; these aliases keep the branches below reading as they always have.
    constexpr int view_story     = history_view_story;
    constexpr int view_chain     = history_view_chain;
    constexpr int view_ages      = history_view_ages;
    constexpr int view_tectonics = history_view_tectonics;
    int& view = s.history_view; // in ui_state so a verify script can park a view

    // Clamp: retiring Tiles renumbered Ages from 3 to 2, so a stale saved index or an
    // un-updated script can still hand us an index no branch below draws. Land on Story
    // rather than on nothing.
    if (view < view_story || view > view_tectonics)
        view = view_story;

    nav_button("Story", view_story, view, p_open);
    ImGui::SameLine();
    nav_button("Chain", view_chain, view, p_open);
    ImGui::SameLine();
    nav_button("Ages", view_ages, view, p_open);
    ImGui::SameLine();
    nav_button("Tectonics", view_tectonics, view, p_open);

    // ARM THE CONTINENT LENS ON ENTERING TECTONICS (Ben's ruling on NR-742,
    // 2026-08-30). LENSES.md's routing table already sends a plate press to this
    // view; this closes the loop the other way.
    //
    // THIS IS A SECOND ARMING RULE, and it is deliberately not the one slots 6 and 7
    // use. Those arm a FIXED lens when the ledger opens, which works because their
    // ledgers answer one question on every tab. History answers four, and only this
    // one has a map twin - a fixed arm would hand the Continent lens to a player who
    // opened on Story to read a deep-time biography.
    //
    // ON ENTERING, not while on. The edge is what is armed, so a player who reaches
    // Tectonics and then deliberately picks another lens keeps it; re-arming every
    // frame would make the lens strip unusable on this view. Entering covers both
    // routes in: pressing the tab, and opening the ledger onto a parked Tectonics
    // (the reset above is what makes the second one an edge).
    //
    // Closing the ledger does NOT disarm, which is the one thing this shares with
    // slots 6 and 7 - see LENSES.md, and NR-722 for what is still unowned.
    if (view == view_tectonics && last_drawn_view != view_tectonics)
        s.overlay = overlay_mode::continent;
    last_drawn_view = view;

    // The view-level disclosure control (BL-214, revised BL-265). Story is a single
    // block that already shows its content in the column, so there is nothing for the
    // in-place control to expand — it takes the full-canvas control alone. It still
    // lands in the SAME rightmost column as every paired control elsewhere:
    // `disclosure_controls` leaves the gutter's left slot empty rather than letting the
    // lone control slide sideways.
    //
    // Chain does NOT take one: its stages carry their own, and a second control
    // governing all of them at once would re-merge what the per-stage disclosure
    // separates. Ages is excluded for a different reason — its map sizes itself to
    // whatever column it is given, so the control would govern a surface that already
    // fills its space.
    //
    // BL-265's own text pairs Story with Tiles here; Tiles was retired by BL-281 in the
    // same batch, and `detail_surface::history_tiles` no longer exists.
    if (view == view_story)
        disclosure_controls(s, detail_surface::history_story, 0, /*in_place=*/false);
    ImGui::Separator();

    // ── Tectonics (BL-660) ────────────────────────────────────────────────
    // "click to the relevant history section, describing collisions and the
    // opposite" (Ben, 2026-08-28). A plate press under the Continent lens opens
    // this ledger on this view, with `ui_state::selected_plate` naming which.
    //
    // A DECLARED PLACEHOLDER: it lists the plate set and both boundary masks, and
    // it does not yet tell the per-boundary STORY — which pair met, when, and what
    // the meeting raised or opened. That narrative exists, in the body's Story
    // view, and moving it here is the follow-on.
    //
    // "and the opposite" — the RIFTS — is real now. The drift pass classified both
    // kinds all along and recorded only collisions; BL-660's data half made the
    // divergent mask an output, so this reads it rather than inventing it.
    //
    // THE TWO MASKS ARE NOT EXCLUSIVE, which is the obvious reading and is wrong.
    // The boundary walk visits each tile against two neighbours, so a tile at a
    // junction between a closing pair and an opening pair is genuinely on both —
    // `height_bias` has always accumulated the uplift AND the subsidence there.
    // Measured at 1 tile in 15120 on the fixture seed. The table shows the two
    // counts separately rather than a single "boundary" number for that reason.
    if (view == view_tectonics)
    {
        const continent_state* cs = nullptr;
        for (const auto& be : report.bodies)
            if (be.id == selected_body) { cs = &be.continents; break; }

        if (cs == nullptr || cs->plate_id.empty())
        {
            ImGui::TextDisabled("No tectonic record for this body.");
            ImGui::TextDisabled("A stagnant-lid world has one plate and no boundaries.");
            ui::foldout_end();
            return;
        }

        // Per-plate tile and collision counts, computed here rather than cached:
        // this view is open only when someone is looking at it, and the raster is
        // one pass over the body.
        const std::size_t n = cs->plate_id.size();
        const bool have_conv = cs->convergent.size() == n;
        const bool have_div = cs->divergent.size() == n;
        std::vector<int> tiles(cs->plates.size(), 0);
        std::vector<int> collide(cs->plates.size(), 0);
        std::vector<int> rift(cs->plates.size(), 0);
        int collide_total = 0;
        int rift_total    = 0;
        for (std::size_t k = 0; k < n; ++k)
        {
            const int pid = cs->plate_id[k];
            if (pid < 0 || static_cast<std::size_t>(pid) >= tiles.size())
                continue;
            ++tiles[static_cast<std::size_t>(pid)];
            if (have_conv && cs->convergent[k] != 0u)
            {
                ++collide[static_cast<std::size_t>(pid)];
                ++collide_total;
            }
            if (have_div && cs->divergent[k] != 0u)
            {
                ++rift[static_cast<std::size_t>(pid)];
                ++rift_total;
            }
        }

        ImGui::SeparatorText("Plates");
        ImGui::TextDisabled("%zu plates - %d tiles on a collision, %d on a rift",
                            cs->plates.size(), collide_total, rift_total);
        ImGui::Spacing();

        if (ImGui::BeginTable("##tectonics", 5,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Plate");
            ImGui::TableSetupColumn("Kind");
            ImGui::TableSetupColumn("Tiles");
            ImGui::TableSetupColumn("Collision");
            ImGui::TableSetupColumn("Rift");
            ImGui::TableHeadersRow();
            for (std::size_t p = 0; p < cs->plates.size(); ++p)
            {
                ImGui::TableNextRow();
                // The pressed plate is highlighted, which is what makes this a
                // DESTINATION rather than a table that happens to contain it.
                if (s.selected_plate == static_cast<int>(p))
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                           IM_COL32(255, 255, 255, 40));
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%zu", p);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled(cs->plates[p].oceanic ? "Oceanic" : "Continental");
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d", tiles[p]);
                ImGui::TableSetColumnIndex(3);
                if (collide[p] > 0)
                    ImGui::Text("%d", collide[p]);
                else
                    ImGui::TextDisabled("-");
                ImGui::TableSetColumnIndex(4);
                if (rift[p] > 0)
                    ImGui::Text("%d", rift[p]);
                else
                    ImGui::TextDisabled("-");
            }
            ImGui::EndTable();
        }

        ImGui::Spacing();
        ImGui::SeparatorText("Placeholder surface");
        ImGui::TextWrapped(
            "First cut. What is missing is the per-boundary NARRATIVE - which "
            "pair met, in which epoch, and what the meeting raised or opened. Those lines "
            "exist and are readable in this body's Story view; moving them here, keyed to "
            "the plate you pressed, is the follow-on. A tile can appear in both columns: "
            "a junction between a closing pair and an opening pair is on both boundaries.");

        ui::foldout_end();
        return;
    }

    const body_component& sel_body = w.bodies.at(selected_body);

    // The Chain view charts every body side by side — that comparison IS the view —
    // so the per-body selector belongs to Story, the view that is about one body.
    if (view == view_story && ImGui::BeginCombo("Body", sel_body.name.c_str()))
    {
        for (entity_id id : body_ids)
        {
            const body_component& b = w.bodies.at(id);
            const bool is_sel = (id == selected_body);
            if (ImGui::Selectable(b.name.c_str(), is_sel))
                selected_body = id;
            if (is_sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Body summary line
    if (view == view_story)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s  |  %.2f AU  |  %dx%d tiles",
            body_type_name(sel_body.type),
            sel_body.orbital_radius_au,
            sel_body.grid_width,
            sel_body.grid_height);
        ImGui::Separator();
    }

    // The report is matched by ENTITY ID (BL-257): body_entry names its own world
    // entity, so the biography is keyed on identity rather than on the display
    // name, which is generated.
    const generation_report::body_entry* entry = nullptr;
    for (const auto& be : report.bodies)
        if (be.id == selected_body) { entry = &be; break; }

    // --- Story: the oral-history biography (BL-211) ---
    // GENERATION_LEDGER.md flagged this as an open item ("player-facing
    // variant... out of scope") and MENU.md named this slot as its destination
    // ("generation history, not a live event log").
    if (view == view_story)
    {
        // Drawn into whichever host is active — the column at rest, the full-canvas
        // takeover when expanded — rather than twice. One body, two hosts, so the two
        // cannot show different things.
        auto story_body = [&]() {
            if (!entry || entry->state.history.empty())
            {
                ImGui::TextDisabled("No recorded history for this body.");
                return;
            }
            ImGui::TextDisabled("%s - %s to present",
                archetype_name(entry->state.archetype),
                format_history_date(entry->state.history.front().years_before_epoch).c_str());
            ImGui::Spacing();
            for (const history_event& ev : entry->state.history)
            {
                // The date's unit comes from its own magnitude, so a deep-time
                // line and a historical one share this loop (BL-220).
                ImGui::TextWrapped("%s  %s",
                    format_history_date(ev.years_before_epoch).c_str(), ev.event.c_str());
                if (!ev.consequence.empty())
                {
                    // Wrapped, not TextDisabled: the consequence is the longer half
                    // of the pair and the column clipped it mid-word.
                    ImGui::Indent();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                    ImGui::TextWrapped("%s", ev.consequence.c_str());
                    ImGui::PopStyleColor();
                    ImGui::Unindent();
                }
            }
        };

        if (ui::fold_overlay_begin(s, detail_surface::history_story, 0, sel_body.name.c_str()))
        {
            story_body();
            ui::fold_overlay_end(s);
        }
        else
        {
            story_body();
        }
        ui::foldout_end();
        return;
    }

    // --- Ages: the Era -1 political time-lapse (BL-277) -------------------
    //
    // A REPLAY, NOT A RE-RUN (NR-733, Ben's ruling 2026-08-30). Generation
    // records the era's ownership history into the body's own report entry
    // (`prehistory_timelapse`), and this view folds over it. There is no sim
    // here, no cache to invalidate and no settlement to copy.
    //
    // WHAT THAT REPLACED, because the shape it fixes is worth keeping in view.
    // This block used to re-run `run_history_sim` itself, lazily, over a copy of
    // the body's settlement. That made it a SEVENTH CALLER of an invocation
    // `world/era_minus_one.hpp` exists to keep singular, and it diverged from
    // generation on all six of BL-462's axes at once. Three (span, clock, seed)
    // were closable at the call site and were closed. The other three were not:
    // the report's `settlement` is the state AFTER generation's sim mutated it,
    // so a re-run started the era from its own ending and reported 0 battles and
    // 0 conquests no matter how carefully it was parameterised — a settlement
    // time-lapse wearing a political one's label.
    //
    // Replaying deletes the second caller, which closes all six divergences at
    // once, and turns minutes on the drawing thread into a fold over a change
    // list. The cost of that is a save-format field; `save_game_version` is 3.
    if (view == view_ages)
    {
        if (!entry)
        {
            ImGui::TextDisabled("No generation record for this body.");
            ui::foldout_end();
            return;
        }

        const era_timelapse&    lapse   = entry->prehistory_timelapse;
        const settlement_state& settled = entry->settlement;

        if (lapse.empty())
        {
            // EMPTY IS THE COMMON CASE, not an error: generation runs the era for
            // the cradle alone, and for no body at all in a 1960-era world.
            ImGui::TextDisabled("%s was never settled - no political history to replay.",
                                sel_body.name.c_str());
            ui::foldout_end();
            return;
        }

        // Park the scrubber at the run's own first year the first time this
        // body's record is shown. Keyed on the body so switching bodies
        // re-parks, while scrubbing within one body is left alone.
        static entity_id parked_for = null_entity;
        if (parked_for != selected_body)
        {
            parked_for   = selected_body;
            s.ages_year  = lapse.start_year;
            s.ages_playing = false;
        }

        const int first_year = lapse.start_year;
        const int last_year  = lapse.start_year + lapse.years;

        // --- Transport -----------------------------------------------------
        if (ImGui::Button(s.ages_playing ? "Pause" : "Play"))
            s.ages_playing = !s.ages_playing;
        ImGui::SameLine();
        if (ImGui::Button("Restart")) { s.ages_year = first_year; s.ages_playing = true; }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        // The "format" is a finished string, not a printf template, because a
        // signed calendar year has no single printf form ("%d CE" prints
        // "-400 CE"). ImGui supports this deliberately: ImParseFormatTrimDecorations
        // returns "" for a format with no specifier and TempInputScalar then falls
        // back to "%d", so ctrl+click editing still works on the raw year.
        ImGui::SliderInt("##ages_year", &s.ages_year, first_year, last_year,
                         ages_year_label(s.ages_year).c_str());

        if (s.ages_playing)
        {
            // The playback RATE follows the span rather than being a constant.
            // 120 years a second was tuned when this replayed 2000 years; on
            // generation's real 400-year era it empties the transport in three
            // seconds. Deriving it keeps the stated intent — long enough to
            // watch a frontier move, short enough to sit through — if the era
            // length (world_params::prehistory_years) is ever retuned.
            const float span     = static_cast<float>(last_year - first_year);
            const float run_secs = 16.0f;
            const float rate     = span > 0.0f ? span / run_secs : 1.0f;
            static float carry = 0.0f;
            carry += ImGui::GetIO().DeltaTime * rate;
            const int whole = static_cast<int>(carry);
            if (whole > 0) { carry -= static_cast<float>(whole); s.ages_year += whole; }
            if (s.ages_year >= last_year) { s.ages_year = last_year; s.ages_playing = false; }
        }

        // --- The map -------------------------------------------------------
        const std::vector<uint16_t> slice = owner_slice_at(lapse, s.ages_year);

        int live = 0;
        for (uint16_t o : slice) if (o != owner_none) ++live;

        // Distinct owners on the map right now — the multipolarity read, and the
        // number BL-224's non-hegemony invariant is ultimately about.
        std::vector<uint16_t> seen;
        for (uint16_t o : slice)
            if (o != owner_none && std::find(seen.begin(), seen.end(), o) == seen.end())
                seen.push_back(o);

        ImGui::TextDisabled("%s  |  %d regions  |  %d powers",
                            ages_year_label(s.ages_year).c_str(),
                            live, static_cast<int>(seen.size()));

        const float avail = ImGui::GetContentRegionAvail().x;
        const int   gw    = sel_body.grid_width  > 0 ? sel_body.grid_width  : 1;
        const int   gh    = sel_body.grid_height > 0 ? sel_body.grid_height : 1;
        const float scale = avail / static_cast<float>(gw);
        const float mh    = scale * static_cast<float>(gh);

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImDrawList*  dl     = ImGui::GetWindowDrawList();
        dl->AddRectFilled(origin, ImVec2(origin.x + avail, origin.y + mh),
                          IM_COL32(18, 20, 26, 255));

        // One dot per region, at its anchor's grid position, in its owner's
        // colour. Region granularity is what makes this cheap to draw as well
        // as cheap to store — there is no tile loop here at all.
        const float r = scale * 1.6f < 2.5f ? 2.5f : scale * 1.6f;
        for (std::size_t i = 0; i < slice.size() && i < settled.regions.size(); ++i)
        {
            if (slice[i] == owner_none) continue;
            const region& p = settled.regions[i];
            const ImVec2 at(origin.x + (static_cast<float>(p.col) + 0.5f) * scale,
                            origin.y + (static_cast<float>(p.row) + 0.5f) * scale);
            dl->AddCircleFilled(at, r,
                                palette::nation_colour(static_cast<entity_id>(slice[i] + 1)));
        }
        ImGui::Dummy(ImVec2(avail, mh));

        // --- What the run produced -----------------------------------------
        ImGui::Separator();
        // Wrapped, not TextDisabled: the counters line overran the 380 px column
        // and clipped mid-word ("371 foun"), the same failure the Story view's
        // consequence text already fixed this way.
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        // The counters are GENERATION'S OWN, read off the report rather than off
        // a local re-run — which is the whole difference this change made: they
        // now describe the era the world actually has.
        ImGui::TextWrapped("Full run: %lld battles, %lld conquests, %lld foundings",
                           static_cast<long long>(report.prehistory_battles),
                           static_cast<long long>(report.prehistory_conquests),
                           static_cast<long long>(report.prehistory_foundings));
        ImGui::TextWrapped("Time-lapse: %lld changes over %d regions",
                           static_cast<long long>(lapse.changes.size()),
                           lapse.region_stride);
        ImGui::PopStyleColor();

        ui::foldout_end();
        return;
    }

    // --- Chain: the generation charts, one accordion per stage (BL-211) ---
    // These are the wizard's own plots (ui::generation_charts), redrawn from the
    // persisted report. Before this they existed only on the screen the player
    // clicks through once — the richest visuals in the game, discarded at the
    // moment the world opened. The round strip is the wizard's grouping, so a
    // player who read the chain at generation finds it filed the way they learned
    // it; the stages inside collapse so one question is open at a time.
    if (view == view_chain)
    {
        if (report.bodies.empty())
        {
            ImGui::TextDisabled("No generation report - this world predates the chain.");
            ui::foldout_end();
            return;
        }

        int& round = s.history_round;
        nav_button("System", 0, round);
        ImGui::SameLine();
        nav_button("Life", 1, round);
        ImGui::SameLine();
        nav_button("Legacy", 2, round);

        const ui::chain_round& cr = ui::chain_round_at(round);
        // Wrapping container (1): the round question clipped mid-word (BL-215).
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ui::wrap_text(ui::text_box::foldout_column, "tile_inspector.round_question",
                      cr.question, ImGui::GetContentRegionAvail().x);
        ImGui::PopStyleColor();
        ImGui::Separator();

        // Rebuilt per frame from the report: a handful of pointers, no copies of the
        // states themselves.
        std::vector<ui::generation_chart_body> chart_bodies;
        chart_bodies.reserve(report.bodies.size());
        std::size_t home = 0;
        for (std::size_t i = 0; i < report.bodies.size(); ++i)
        {
            const generation_report::body_entry& be = report.bodies[i];
            chart_bodies.push_back(ui::generation_chart_body{
                be.name.c_str(), &be.state, &be.undrawn });
            // The homeworld is the only body S9 runs on, so a non-zero drawdown is
            // what identifies it without the report carrying a flag.
            if (be.state.drawdown > 0.0f)
                home = i;
        }
        const ui::generation_chart_source src{
            chart_bodies.data(), chart_bodies.size(), home };

        // Each stage rests as its verdict line, grows in place on `⌄`, and gives the
        // WHOLE ROUND the canvas on `›` (BL-214, revised BL-265). This replaces the
        // per-stage CollapsingHeader, which was this surface's own private disclosure
        // idiom — the fourth one the item exists to retire. The takeover is what
        // fixes the thing the accordion could not: a round's charts never fit this
        // 380 px column, so "open" here always meant "scroll", while the canvas gives
        // every chart the width it was drawn for.
        for (int si = static_cast<int>(cr.first); si <= static_cast<int>(cr.last); ++si)
            ui::draw_stage_fold(src, static_cast<chain_stage>(si), s,
                                detail_surface::history_chain);

        ui::foldout_end();
    }
}

} // namespace ui
