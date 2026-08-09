#include "construction_panel.hpp"

#include <algorithm>
#include <cmath>   // std::ceil (construction ETA, BL-095)
#include <cstdio>  // std::snprintf (buildings-table row ids, BL-143)
#include <string>  // status strings (BL-095)

#include "foldout_column.hpp" // shell fold-out column host (BL-122)
#include "detail_level.hpp"   // the BL-265 disclosure idiom, reused by BL-229's sections
#include "icons.hpp"          // production-method pip glyph + the identity plate (BL-229)
#include "presentation.hpp"
#include "ui_state.hpp"

#include <cfloat>  // FLT_MAX (PlotLines auto-scale)

#include "world/budget_system.hpp"     // building_opex / compute_building_opex, body_mean_habitability (BL-143)
#include "world/logistics.hpp"         // invalidate_logistics_caches (decommission flips the anchor set)
#include "world/building_profit.hpp"   // estimate_building_profit (BL-143 Buildings tab)
#include "world/components.hpp"
#include "world/construction.hpp"     // demolish_building (BL-229 Dismantle)
#include "world/economy_system.hpp"    // economy_report (BL-143 status column)
#include "world/market_clearing.hpp"   // market_for_tile (build-rate read, BL-095)
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"

#include <imgui.h>

namespace ui {

namespace {

// --- Analog construction status (BL-095 task E) ------------------------------
// Construction is durative *and* material-gated: each economy tick a building under
// construction advances by a RATE in [0,1] set by how much of its per-tick material
// need the local market can supply (economy_system.cpp § run_construction). These
// mirror that formula (const-read only) so the management view can show the same
// analog rate / ETA / paused status the Selection front door does. (Kept a local copy
// rather than shared — the UI has no common header seam to hang it on.)
float construction_rate(const world& w, const recipe_registry& reg,
                        building_type type, entity_id tile)
{
    const building_economics& econ = reg.economics(type);
    const float duration = econ.build_duration_ticks;
    if (duration <= 0.0f)
        return 1.0f; // instant build — never material-gated

    const market_component* m = nullptr;
    const entity_id mid = market_for_tile(w, tile);
    if (mid != null_entity)
    {
        const auto mit = w.markets.find(mid);
        if (mit != w.markets.end())
            m = &mit->second;
    }

    float rate = 1.0f;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const float need = econ.resource_build_cost[r] / duration;
        if (need <= 0.0f)
            continue;
        const float avail = m ? m->supply[r] : 0.0f;
        rate = std::min(rate, avail / need);
    }
    rate = std::clamp(rate, 0.0f, 1.0f);

    const float max_stretch = reg.construction().max_stretch;
    const float pause_below  = (max_stretch > 1.0f) ? (1.0f / max_stretch) : 0.0f;
    if (rate < pause_below)
        rate = 0.0f; // paused: even the max-stretched rate can't be supplied
    return rate;
}

// Compact quarter-count label; a Tick is ~3 months, so 4 make a year (BL-095).
//
// The DISPLAY word is "qtr", not "tick" (NR-002, Ben 2026-08-01): Tick is the internal
// term for the economy step and reads as engineering to a player. It is also the less
// accurate of the two here — the step is literally a calendar quarter, which is why this
// function could already gloss it in years.
std::string ticks_label(int ticks)
{
    std::string s = std::to_string(ticks) + (ticks == 1 ? " qtr" : " qtrs");
    if (ticks >= 4)
        s += " (~" + std::to_string(ticks / 4) + " yr)";
    return s;
}

// Human status for a build rate + the whole ticks of work left.
std::string construction_status(float rate, int ticks_left)
{
    if (rate <= 0.0f)
        return "Paused - market can't supply materials";
    if (rate >= 1.0f)
        return "Building... ~" + ticks_label(ticks_left);
    const int eta = static_cast<int>(std::ceil(static_cast<float>(ticks_left) / rate));
    return "Building... ~" + ticks_label(eta) + " (materials scarce)";
}

// Colour for an analog build status: red paused, amber scarce, green on-schedule.
ImVec4 construction_status_colour(float rate)
{
    return (rate <= 0.0f) ? ImVec4{0.90f, 0.55f, 0.55f, 1.0f}   // paused
         : (rate < 1.0f)  ? ImVec4{0.85f, 0.75f, 0.45f, 1.0f}   // materials scarce
                          : ImVec4{0.55f, 0.80f, 0.55f, 1.0f};  // on schedule
}

// --- Estimated quarterly cost of in-progress construction (BL-143) ----------
// Sums compute_building_opex's maintenance + wages (budget_system.hpp — the
// same formula the live budget loop and building_profit use, so no drift)
// over every player building still under construction. One economy tick is
// one quarter (k_ticks_per_year = 4), so a single tick's opex *is* the
// per-quarter figure — no further scaling needed.
float estimated_quarterly_construction_cost(const world& w, const recipe_registry& reg)
{
    float total = 0.0f;
    for (const auto& [id, b] : w.buildings)
    {
        if (b.ticks_remaining <= 0)
            continue; // completed — not "in progress"
        if (!is_player_owned(w, id))
            continue;

        const entity_id body = (w.tiles.count(b.tile) != 0) ? w.tiles.at(b.tile).body : null_entity;
        const float mean_hab = (body != null_entity) ? body_mean_habitability(w, body) : 1.0f;
        const building_opex opex = compute_building_opex(b, reg.economics(b.type), 1.0f, mean_hab);
        total += opex.maintenance + opex.wages;
    }
    return total;
}

// --- Queue overview table ----------------------------------------------------
// All active construction items across all bodies. Progress cell is
// colour-coded: dim (<25%) grey, mid (25–75%) yellow, high (>75%) green.
// The construction queue data model is not yet authored (BL-029 precedes the
// queue backend); until that lands, this always shows the empty-state message.
// Own tab ("Construction", BL-143 rename): "what's happening with my
// construction?" — now paired with an estimated per-quarter cost of every
// in-progress build.
void draw_queue_section(const world& w, const recipe_registry& reg)
{
    ImGui::SeparatorText("Queue");

    const float est_cost = estimated_quarterly_construction_cost(w, reg);
    ImGui::Text("Estimated cost: %.1f / quarter", static_cast<double>(est_cost));
    ImGui::Spacing();

    // When the queue backend lands, iterate the queue here.
    // For now the queue is always empty — show the placeholder.
    const bool any_items = false;

    if (!any_items)
    {
        ImGui::TextDisabled("No active construction.");
        return;
    }

    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingStretchProp;

    if (!ImGui::BeginTable("##construction_queue", 5, table_flags))
        return;

    ImGui::TableSetupColumn("Body",             ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableSetupColumn("Building",         ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("Progress",         ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableSetupColumn("Est. qtrs left",  ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableSetupColumn("Cost remaining",   ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableHeadersRow();

    // Placeholder loop — replace with real queue iteration when the backend lands.
    // Example shape:
    //   for (const auto& item : w.construction_queue) {
    //       const float pct = item.progress_0_to_1;
    //       ImGui::TableNextRow();
    //       ImGui::TableSetColumnIndex(0);
    //       ImGui::TextUnformatted(w.bodies.at(item.body).name.c_str());
    //       ImGui::TableSetColumnIndex(1);
    //       ImGui::TextUnformatted(building_type_name(item.type));
    //       ImGui::TableSetColumnIndex(2);
    //       ImU32 cell_colour;
    //       if      (pct < 0.25f) cell_colour = IM_COL32(80, 80, 80, 160);
    //       else if (pct < 0.75f) cell_colour = IM_COL32(200, 180, 0, 160);
    //       else                  cell_colour = IM_COL32(0, 160, 80, 160);
    //       ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, cell_colour);
    //       ImGui::Text("%.0f%%", pct * 100.0f);
    //       ImGui::TableSetColumnIndex(3);
    //       ImGui::Text("%d", item.ticks_remaining);
    //       ImGui::TableSetColumnIndex(4);
    //       ImGui::Text("%.1f", item.cost_remaining);
    //   }

    ImGui::EndTable();
}

// --- Selected-building section -----------------------------------------------
// Resolves the building the current selection refers to and shows its config,
// plus live management controls: workforce slider, recipe selector, decommission.
// `selected_entity` is a generic entity reference, not always a tile: normal
// tile-click selection (SELECTION.md) sets it to the tile, but committing a
// build (both the interactive click-to-build path, app.cpp's pending_tile
// handler, and the verify harness's build_first_valid) sets it directly to the
// newly-built building's own id so its Selection-panel card is right there.
// Only matching on tile == selected_entity (the prior behaviour) missed that
// second case entirely — right after placing a building, this section showed
// "Select a building's tile to manage it." even though one was just built and
// selected, forcing the player to click away and reselect the tile before they
// could reach the recipe combo (found while investigating why processing-facility
// management felt clunky, 2026-07-06 — every processor defaults to the steel
// recipe at construction, so this was blocking the very first thing a player
// would want to change).
//
// The primary output resource of a recipe — the argmax of its outputs. Used to tag a
// production method with a resource pip glyph (proper per-method glyphs are owed).
resource_type primary_output_resource(const recipe& r)
{
    std::size_t best = 0;
    float best_v = -1.0f;
    for (std::size_t i = 0; i < resource_count; ++i)
        if (r.outputs[i] > best_v) { best_v = r.outputs[i]; best = i; }
    return static_cast<resource_type>(best);
}

// Inline detail under the Buildings tab: answers "how do I configure the thing I
// have" for whichever row the player selected in the buildings table above. Ben's
// 2026-07-15 building-management mockup (UI shell): title + placeholder image, a
// production-method dropdown (each method glyph-tagged), profit + workforce graphs,
// and a workforce-target button grid. The graphs read PLACEHOLDER deterministic
// series (no per-building history recorded yet) and the target is still a manual
// field — the auto-solver that treats it as an editable heuristic is a separate item.
// Takes ui_state MUTABLE (was const): BL-229's sections use the BL-265 disclosure
// idiom, and which sections a player has opened lives in ui_state.expanded.
void draw_selected_section(world& w, const recipe_registry& reg,
                           const economy_report& report, ui_state& state)
{
    building_component* found = nullptr;
    if (const auto bit = w.buildings.find(state.selected_entity); bit != w.buildings.end())
        found = &bit->second;
    else
    {
        // Fall back to tile-match for the normal "select a tile, see its building" case.
        for (auto& [id, bld] : w.buildings)
        {
            if (bld.tile == state.selected_entity)
            {
                found = &bld;
                break;
            }
        }
    }

    if (found == nullptr)
    {
        ImGui::TextDisabled("Select a building's tile to manage it.");
        return;
    }

    building_component& b = *found;

    // --- Identity plate: the building's own glyph, its name, and where it stands ---
    //
    // BL-229 (Ben, 2026-08-09): the image is DRAWN FROM `ui::icons`, not an authored
    // asset. The glyph vocabulary already carries one silhouette per building_type and
    // is the same mark the canvas puts on the tile, so the plate and the map agree by
    // construction rather than by an artist keeping two things in sync. It also costs
    // no asset pipeline, which the prototype does not have.
    //
    // The plate is a SQUARE beside the text rather than a full-width banner: Menu Space
    // is 380 px at its floor, and a banner at 34% of that spends 130 px of vertical
    // budget on decoration in a column whose scarce axis is height.
    ImDrawList*  dl        = ImGui::GetWindowDrawList();
    const float  content_w = ImGui::GetContentRegionAvail().x;
    {
        const float  plate = ImGui::GetFrameHeight() * 2.6f;
        const ImVec2 p     = ImGui::GetCursorScreenPos();
        const ImVec2 mx    = {p.x + plate, p.y + plate};
        dl->AddRectFilled(p, mx, IM_COL32(26, 30, 40, 255), 3.0f);
        dl->AddRect(p, mx, IM_COL32(58, 66, 84, 255), 3.0f);
        // The Buildings tab lists the player's own holdings, so the identity colour is
        // theirs — no owner lookup needed here (ownership lives in corp.assets, not on
        // the building).
        icons::building(dl, {(p.x + mx.x) * 0.5f, (p.y + mx.y) * 0.5f}, plate * 0.30f,
                        b.type, palette::corp_identity_colour(w.player_entity, w.player_entity));
        ImGui::Dummy({plate, plate});

        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s",
                           building_type_name(b.type));
        if (const auto tit = w.tiles.find(b.tile); tit != w.tiles.end())
            ImGui::TextDisabled("Tile [%d, %d]", tit->second.grid_x, tit->second.grid_y);
        if (b.ticks_remaining > 0)
            ImGui::TextDisabled("Under construction - %d ticks", b.ticks_remaining);
        else
            ImGui::TextDisabled("%s", b.decommissioned ? "Closed" : "Operating");
        ImGui::EndGroup();
    }

    // --- The four sections, folded to a verdict line each (BL-229, Ben's variant C) --
    //
    // Each section rests as ONE line carrying its own answer and opens in place. This
    // is deliberately the BL-265 idiom rather than a second one: the player already
    // learned it on the History Chain and the dashboard roll-ups the same day, and
    // reusing it means the full-canvas control comes free on every section.
    //
    // PRODUCTION IS OPEN BY DEFAULT (see `default_open` below). The mockup's own
    // objection to variant C was that the controls a player touches constantly sit
    // behind a click; the method combo is the one that earns being open at rest.
    //
    // Lifecycle is LAST and its two controls are the only irreversible ones on the
    // panel. Keeping them inside a section that rests closed is what stops Dismantle
    // sitting a mis-click away from a workforce button — the one recommendation the
    // mockups made independently of which variant was chosen.
    enum building_view_section { sec_production = 0, sec_workforce, sec_profit, sec_lifecycle };

    // A section's resting line: title, its verdict in the right gutter, then `⌄ ›`.
    // Mirrors corporation_dashboard.cpp's `card_row` so the two read identically.
    const auto section_row = [&](int key, const char* title, ImU32 col, const char* verdict)
    {
        ImGui::PushID(key);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", title);
        ImGui::SameLine();
        gutter_text(col, verdict);
        disclosure_controls(state, detail_surface::building_section, key);
        ImGui::PopID();
        return is_open_in_place(state, detail_surface::building_section, key);
    };

    // --- Production: a dropdown of the building's recipes, each tagged with a
    // resource pip for its primary output. ---
    const int   n_recipes = reg.recipe_count(b.type);
    const float pipr      = ImGui::GetFontSize() * 0.42f;
    const auto  method_res = [&](int i) -> resource_type {
        if (b.type == building_type::extraction_site) return b.target_resource;
        return primary_output_resource(reg.recipe_at(b.type, i));
    };

    // Production rests OPEN — see the note above. `set_open_in_place` is called once,
    // guarded on the section never having been touched, so a player who folds it away
    // keeps it folded.
    if (!state.building_section_seeded)
    {
        set_open_in_place(state, detail_surface::building_section, sec_production, true);
        state.building_section_seeded = true;
    }

    const char* method_verdict =
        (n_recipes >= 1) ? reg.recipe_at(b.type, std::clamp(b.active_recipe_index, 0, n_recipes - 1)).name.c_str()
                         : "Single method";
    const bool open_production =
        section_row(sec_production, "Production", IM_COL32(150, 158, 172, 255), method_verdict);

    if (open_production && n_recipes >= 1)
    {
        b.active_recipe_index = std::clamp(b.active_recipe_index, 0, n_recipes - 1);
        const recipe& cur     = reg.recipe_at(b.type, b.active_recipe_index);
        const char*   preview = cur.name.empty() ? "-" : cur.name.c_str();

        // Current-method pip, then the combo.
        const ImVec2 gp = ImGui::GetCursorScreenPos();
        const float  rh = ImGui::GetFrameHeight();
        icons::resource(dl, {gp.x + pipr, gp.y + rh * 0.5f}, pipr, method_res(b.active_recipe_index));
        ImGui::Dummy({pipr * 2.0f + 6.0f, rh});
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::BeginCombo("##method", preview))
        {
            ImDrawList* pdl = ImGui::GetWindowDrawList(); // popup's own draw list
            for (int i = 0; i < n_recipes; ++i)
            {
                const recipe& ri  = reg.recipe_at(b.type, i);
                const bool    sel = (i == b.active_recipe_index);
                const ImVec2  ip  = ImGui::GetCursorScreenPos();
                const std::string lbl = std::string("     ") + (ri.name.empty() ? "-" : ri.name);
                if (ImGui::Selectable(lbl.c_str(), sel))
                {
                    b.active_recipe_index = i;
                    b.recipe = reg.recipe_id(ri.name);
                }
                icons::resource(pdl, {ip.x + pipr + 2.0f, ip.y + ImGui::GetTextLineHeight() * 0.5f},
                                pipr, method_res(i));
                if (sel)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    // --- Where this site sits in its tile's stack (BL-193) ---------------------
    // Sites stack on one tile up to a ceiling the deposit's richness sets, which the
    // placement gate already enforces. BL-193 adds the half that decides whether
    // stacking is worth DOING: where in the stack this site sits, and what that costs
    // it. Without the line the decay is invisible — the player sees a second site
    // produce less than the first and has nothing telling them that is the rule
    // rather than a fault.
    //
    // Homed here rather than in the Selection element: the rich building card that
    // used to carry it was deleted by BL-339 (it had been parked and unreachable), and
    // Manage routes to this tab (NR-093). A readout in a dead function is not a readout.
    if (const auto tit = w.tiles.find(b.tile); open_production && tit != w.tiles.end())
    {
        const int cap  = placement_rules::stack_capacity(tit->second, b.type, b.target_resource);
        const int here = placement_rules::buildings_on_tile(w, b.tile, b.type, b.target_resource);
        const int rank = placement_rules::stack_rank(w, state.selected_entity);

        ImGui::TextDisabled("%d of %d site%s", here, cap, cap == 1 ? "" : "s");
        if (here < cap)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("- room for %d more", cap - here);
        }

        // Extraction only: every other type is capacity 1, so there is no position in
        // a stack to report and the line would be noise.
        if (b.type == building_type::extraction_site && rank > 0)
        {
            if (rank == 1)
                ImGui::TextDisabled("First site here - full rate. Each later site yields %.0f%% of the one before.",
                                    placement_rules::k_stack_output_decay * 100.0f);
            else
                ImGui::TextDisabled("Site %d of the stack - %.0f%% of a lone site's rate.",
                                    rank, placement_rules::stack_output_scalar(rank) * 100.0f);
            if (here > 1)
                ImGui::TextDisabled("All %d share one deposit, so it runs out sooner.", here);
        }
    }

    // --- Profit + Workforce graphs (PLACEHOLDER deterministic series until real
    // per-building history is recorded; the profit line anchors to the live estimate). ---
    const building_profit prof    = estimate_building_profit(w, reg, report, state.selected_entity);
    constexpr int         N       = 9;
    const float           graph_w = ImGui::GetContentRegionAvail().x;

    float profit_series[N];
    const float p_end = prof.has_data ? prof.net() : 0.0f;
    for (int i = 0; i < N; ++i)
    {
        const float t = static_cast<float>(i) / (N - 1);
        profit_series[i] = (p_end - 35.0f) + t * 35.0f; // gentle ramp to the current estimate
    }
    // Profit: the verdict line carries the number, so the graph is what opening buys.
    char profit_verdict[48];
    if (prof.has_data)
        std::snprintf(profit_verdict, sizeof profit_verdict, "%+.0f / qtr", static_cast<double>(p_end));
    else
        std::snprintf(profit_verdict, sizeof profit_verdict, "no data yet");
    const ImU32 profit_col = !prof.has_data ? IM_COL32(150, 158, 172, 255)
                           : (p_end >= 0.0f) ? IM_COL32(140, 205, 140, 255)
                                             : IM_COL32(215, 130, 120, 255);
    if (section_row(sec_profit, "Profit", profit_col, profit_verdict))
        ImGui::PlotLines("##profit", profit_series, N, 0, nullptr, FLT_MAX, FLT_MAX, {graph_w, 60.0f});

    float wf_series[N];
    const float wf = static_cast<float>(b.workforce_target);
    for (int i = 0; i < N; ++i)
    {
        const float t   = static_cast<float>(i) / (N - 1);
        const float dip = -20.0f * std::sin(t * 3.14159265f); // placeholder dip-and-recover
        wf_series[i]    = std::clamp(wf + dip, 0.0f, 200.0f);
    }
    // --- Workforce: the trend graph, then Auto (the economy tick solves the profit-max
    // target each tick, BL-181) plus a manual 0–100 button grid. A manual tier pins the
    // target and clears Auto; the Auto button re-enables it and shows the solved value.
    char wf_verdict[48];
    std::snprintf(wf_verdict, sizeof wf_verdict, "%d%%%s", b.workforce_target,
                  b.workforce_auto ? " (auto)" : "");
    if (section_row(sec_workforce, "Workforce", IM_COL32(150, 158, 172, 255), wf_verdict))
    {
        ImGui::PlotLines("##workforce", wf_series, N, 0, nullptr, 0.0f, 120.0f, {graph_w, 60.0f});
        if (b.workforce_auto)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        char autolbl[32];
        if (b.workforce_auto)
            std::snprintf(autolbl, sizeof autolbl, "Auto  (%d%%)", b.workforce_target);
        else
            std::snprintf(autolbl, sizeof autolbl, "Auto");
        if (ImGui::Button(autolbl, {ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() * 1.4f}))
            b.workforce_auto = true;
        if (b.workforce_auto)
            ImGui::PopStyleColor();

        const int   tiers[] = {0, 20, 40, 60, 80, 100};
        const float sp = ImGui::GetStyle().ItemSpacing.x;
        const float bw = (ImGui::GetContentRegionAvail().x - sp * 2.0f) / 3.0f;
        const float bh = ImGui::GetFrameHeight() * 1.6f;
        for (int i = 0; i < 6; ++i)
        {
            const bool active = (!b.workforce_auto && b.workforce_target == tiers[i]);
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            char lbl[16];
            std::snprintf(lbl, sizeof lbl, "%d##wf%d", tiers[i], i);
            if (ImGui::Button(lbl, {bw, bh}))
            {
                b.workforce_target = tiers[i];
                b.workforce_auto   = false; // manual override pins the target
            }
            if (active)
                ImGui::PopStyleColor();
            if (i % 3 != 2)
                ImGui::SameLine();
        }
    }

    // --- Lifecycle: closing and dismantling (BL-229) --------------------------
    // LAST, and rests CLOSED, on purpose. These are the only two irreversible-ish
    // controls on the panel — one stops a building earning, the other destroys it —
    // and the mockups' standing recommendation was that neither should sit adjacent
    // to a workforce button at the same size. A section that rests folded is the
    // cheapest way to honour that without inventing a confirm dialog idiom the rest
    // of the UI does not have.
    //
    // Closing is REVERSIBLE (idle/resume in the command seam) and says so; dismantling
    // is not, so it asks once. Both mirror verbs an agent can already issue, so the
    // player and a text-driven player have the same powers here.
    {
        const char* life_verdict = (b.ticks_remaining > 0) ? "under construction"
                                 : b.decommissioned        ? "closed"
                                                           : "operating";
        const ImU32 life_col = (b.ticks_remaining > 0) ? IM_COL32(215, 190, 115, 255)
                             : b.decommissioned        ? IM_COL32(215, 130, 120, 255)
                                                       : IM_COL32(140, 205, 140, 255);
        if (section_row(sec_lifecycle, "Lifecycle", life_col, life_verdict))
        {
            if (b.ticks_remaining > 0)
            {
                const float rate = construction_rate(w, reg, b.type, b.tile);
                ImGui::TextColored(construction_status_colour(rate), "%s",
                                   construction_status(rate, b.ticks_remaining).c_str());
            }

            if (b.decommissioned)
            {
                if (ImGui::Button("Reopen"))
                {
                    b.decommissioned = false;
                    // Reopening a port/hub restores its anchor — reach field stale.
                    invalidate_logistics_caches(w);
                }
                ImGui::SameLine();
                ImGui::TextDisabled("Closed: no output, no wages.");
            }
            else
            {
                if (ImGui::Button("Close"))
                {
                    b.decommissioned = true;
                    // A decommissioned port/hub stops anchoring supply — reach field stale.
                    invalidate_logistics_caches(w);
                }
                ImGui::SameLine();
                ImGui::TextDisabled("Stops output and wages. Reversible.");
            }

            ImGui::Spacing();
            if (ImGui::Button("Dismantle..."))
                ImGui::OpenPopup("confirm_dismantle");
            ImGui::SameLine();
            ImGui::TextDisabled("Permanent. The tile keeps its deposit.");

            if (ImGui::BeginPopup("confirm_dismantle"))
            {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                                   "Dismantle this %s?", building_type_name(b.type));
                ImGui::TextDisabled("This cannot be undone.");
                ImGui::Separator();
                if (ImGui::Button("Dismantle"))
                {
                    // Through the same seam an agent uses, so the two cannot diverge.
                    demolish_building(w, w.player_entity, state.selected_entity);
                    state.selected_entity = null_entity;
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                    return; // `b` dangles past this point — the building is gone.
                }
                ImGui::SameLine();
                if (ImGui::Button("Keep"))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
    }
}

// --- Sell orders (player) ----------------------------------------------------
// --- Buildings tab (BL-143) ---------------------------------------------------
// Answers "what do I own, and how's it doing?" — every player building in one
// table (name/type, body/tile, workforce, per-quarter profit, status), row
// selection driving the same selected-entity idiom used elsewhere (e.g.
// corporation_dashboard.cpp's roll-ups); the inline detail below reuses
// draw_selected_section for whichever building that leaves selected. "Group
// policies" is a stub only — same disabled-TODO treatment as BL-142.
void draw_buildings_tab(world& w, const recipe_registry& reg,
                        const economy_report& report, ui_state& state)
{
    ImGui::SeparatorText("Buildings");

    ImGui::TextDisabled("Group policies");
    ImGui::TextDisabled("(not implemented)");
    ImGui::Spacing();

    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_ScrollY;

    // The table takes what its ROWS need, up to a ceiling — not a flat 220 px.
    //
    // BL-229: 220 px was spent whether the player owned four buildings or forty, and
    // in a 380 px column that pushed the management view below the comms dock's top
    // edge, so the surface the Manage button routes to was mostly invisible. A fixed
    // height is the wrong shape for a list that starts empty and grows.
    //
    // Ceiling rather than pure content-fit because the list is unbounded: a corp with
    // thirty holdings would otherwise take the whole column and push the detail off the
    // bottom again, which is the same defect from the other direction.
    const int   n_rows    = static_cast<int>(std::count_if(
        w.buildings.begin(), w.buildings.end(),
        [&](const auto& kv) { return is_player_owned(w, kv.first); }));
    const float row_h     = ImGui::GetFrameHeight();
    const float table_h   = std::clamp((static_cast<float>(n_rows) + 1.4f) * row_h,
                                       row_h * 3.0f, 220.0f);
    const bool has_table = ImGui::BeginTable("##buildings", 5, table_flags, {0.0f, table_h});
    if (has_table)
    {
        ImGui::TableSetupColumn("Building",   ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Body/Tile",  ImGuiTableColumnFlags_WidthStretch, 1.8f);
        ImGui::TableSetupColumn("Workforce",  ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Profit/qtr", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Status",     ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableHeadersRow();

        bool any = false;
        for (const auto& [id, b] : w.buildings)
        {
            if (!is_player_owned(w, id))
                continue;
            any = true;

            const building_profit profit = estimate_building_profit(w, reg, report, id);

            bool idle = b.decommissioned;
            if (!idle)
            {
                for (const building_report& br : report.buildings)
                {
                    if (br.building == id) { idle = br.idle; break; }
                }
            }
            const char* status = b.decommissioned ? "Decommissioned" : (idle ? "Idle" : "Active");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const bool selected = (state.selected_entity == id);
            char row_label[64];
            std::snprintf(row_label, sizeof(row_label), "%s##bld%llu",
                          building_type_name(b.type), static_cast<unsigned long long>(id));
            if (ImGui::Selectable(row_label, selected, ImGuiSelectableFlags_SpanAllColumns))
            {
                state.selected_entity = id;
            }

            ImGui::TableSetColumnIndex(1);
            const auto tit = w.tiles.find(b.tile);
            if (tit != w.tiles.end())
            {
                const auto bit = w.bodies.find(tit->second.body);
                const char* body_name = (bit != w.bodies.end()) ? bit->second.name.c_str() : "?";
                ImGui::Text("%s [%d, %d]", body_name, tit->second.grid_x, tit->second.grid_y);
            }
            else
                ImGui::TextDisabled("-");

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d%%", b.workforce_target);

            ImGui::TableSetColumnIndex(3);
            if (profit.has_data)
                ImGui::Text("%.1f", static_cast<double>(profit.net()));
            else
                ImGui::TextDisabled("-");

            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(status);
        }
        if (!any)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("No buildings yet.");
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    draw_selected_section(w, reg, report, state);
}

} // namespace

// Tabbed redesign (BL-143, following the 2026-07-06 Ben's-steer precedent):
// "each view must address one question; a menu allows navigation between
// views." Slimmed from three tabs to two — the Build front door moved to the
// tile Selection element (BL-139, draw_build_front_door in selection_panel.cpp)
// and Sell Orders moved to the Market Ledger (BL-159, a market question, not a
// building one). What remains: "what's building?" (Construction) and "what do
// I own?" (Buildings, which also folds in the former Manage detail for
// whichever row is selected). Still plain Selectable-style buttons rather than
// ImGui::BeginTabBar/TabItem — the tab-bar header mysteriously never rendered
// in this build/environment (see the prior redesign note this replaces).
void draw_construction_panel(world& w,
                             const recipe_registry& reg,
                             const economy_report& report,
                             ui_state& state,
                             bool* p_open)
{
    if (p_open && !*p_open)
        return;

    // Re-hosted into the shell fold-out column (BL-122): pinned + borderless, no floating
    // spawn. The BL-082 height-cap that kept the old floating window clear of the
    // bottom-left Selection element is gone — the column sits entirely left of Selection.
    ui::foldout_begin("Building");

    // BL-176 — land on the building's controls, not an empty queue. Selecting a
    // building (on the canvas, or via the tile element's Manage action) snaps the
    // panel to the Buildings view, whose inline detail carries that building's
    // recipe and workforce controls. Edge-triggered on the selection changing, so
    // the player can still switch to the queue and stay there.
    if (state.selected_entity != state.construction.panel_focus_building)
    {
        state.construction.panel_focus_building = state.selected_entity;
        if (state.selected_entity != null_entity &&
            w.buildings.find(state.selected_entity) != w.buildings.end() &&
            is_player_owned(w, state.selected_entity))
        {
            state.construction.panel_view = 1;
        }
    }

    int& view = state.construction.panel_view;
    ui::nav_button("Construction", 0, view, p_open);
    ImGui::SameLine();
    ui::nav_button("Buildings", 1, view, p_open);
    ImGui::Separator();
    ImGui::Spacing();

    switch (view)
    {
        case 0:
            draw_queue_section(w, reg);
            break;
        case 1:
            draw_buildings_tab(w, reg, report, state);
            break;
        default:
            view = 0;
            break;
    }

    ui::foldout_end();
}

} // namespace ui
