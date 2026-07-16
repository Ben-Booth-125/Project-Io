#include "selection_panel.hpp"

#include "foldout_column.hpp" // shell fold-out column host (shared with the ledgers)
#include "icons.hpp"
#include "presentation.hpp"
#include "selection.hpp"
#include "view_nav.hpp"

#include "world/building_profit.hpp" // per-building profitability estimate (BL-074)
#include "world/economy_system.hpp" // economy_report (workforce cap, BL-069)
#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp" // buildable-type validity for the build ledger (BL-162)
#include "world/survey_system.hpp"

#include <imgui.h>

#include <algorithm> // std::min/clamp (build rate), std::nth_element (top-decile production)
#include <cmath>     // std::ceil (construction ETA), std::pow/log10/floor (nice_ceil axis)
#include <cfloat>    // FLT_MAX (PlotLines auto-scale, building management view)
#include <cstddef>   // std::ptrdiff_t (nth_element iterator offset)
#include <cstdio>    // std::snprintf (tile coord caption + chart labels)
#include <cstring>   // std::strcmp (header title/kind de-dup)
#include <string>    // ticks_label / construction_status
#include <vector>    // top-decile production sample (top_decile_production)

namespace ui {

namespace {

// --- Tile production bar-chart helpers (BL-123) ------------------------------
// The redesigned tile Selection element (docs/ui/SELECTION.md, Ben's mockup) plots,
// per resource deposited on the tile, a CLUSTERED column pair reading two numbers: (A)
// how much this tile produces, and (B) what a top-decile (90th-percentile) tile produces.
// Comparing A against the top-decile reference tells the player how close this tile is to
// a great one for that resource. Each graph sits in its own bordered container with its
// header, and the resource list always shows a scrollbar (a tile can carry more than fit).
// The placement-affordance readout (BL-071) and the build front door moved off this panel to
// the owed tile-construction panel (backlog); this panel is now tile detail + navigation.
//
// "Production" is the tile's hazard-adjusted extraction yield — deposit richness scaled
// by (1 - hazard), the same two factors run_extraction multiplies (economy_system.cpp).
// The uniform base_rate/workforce scalars are dropped: they cancel in the tile-vs-benchmark
// comparison, so this keeps the deposit-magnitude numbers Ben's mockup showed.

// Round @p v up to a 'nice' axis ceiling (1 / 2 / 5 x a power of ten): 45->50,
// 18->20, 130->200. Gives the bar a legible top gridline instead of a ragged max.
float nice_ceil(float v)
{
    if (v <= 0.0f)
        return 1.0f;
    const float mag = std::pow(10.0f, std::floor(std::log10(v)));
    const float n   = v / mag; // 1..10
    const float step = (n <= 1.0f) ? 1.0f : (n <= 2.0f) ? 2.0f : (n <= 5.0f) ? 5.0f : 10.0f;
    return step * mag;
}

// A faint dotted horizontal rule across a chart's gridline, drawn as short dashes
// (ImDrawList has no native dashed line). Matches the mockup's dotted scale lines.
void dotted_hline(ImDrawList* dl, float x0, float x1, float y, ImU32 col)
{
    for (float x = x0; x < x1; x += 6.0f)
        dl->AddLine({x, y}, {std::min(x + 3.0f, x1), y}, col, 1.0f);
}

// A tile's production for resource @p r: hazard-adjusted deposit richness (the two
// tile-local factors of run_extraction's yield). Zero when the tile has no deposit.
float tile_production(const tile_component& t, std::size_t r)
{
    return t.resource_deposit[r] * (1.0f - t.hazard_level);
}

// The 90th-percentile production across every tile that carries resource @p r — the
// top-decile reference the player compares a tile against ("how close is this tile to
// a great one for this resource?"). The bottom-decile floor almost every tile beats
// was uninformative (Ben's 2026-07-15 review); the aspirational ceiling makes the
// headroom the signal. nth_element (O(N)) picks the percentile without a full sort.
float top_decile_production(const world& w, std::size_t r)
{
    std::vector<float> vals;
    vals.reserve(w.tiles.size());
    for (const auto& [id, t] : w.tiles)
        if (t.resource_deposit[r] > 0.0f)
            vals.push_back(tile_production(t, r));
    if (vals.empty())
        return 0.0f;
    const std::size_t k = static_cast<std::size_t>(0.90f * static_cast<float>(vals.size() - 1));
    std::nth_element(vals.begin(), vals.begin() + static_cast<std::ptrdiff_t>(k), vals.end());
    return vals[k];
}

// One resource's chart inside [mn, mx]: a left gutter of tick labels (0 / ceiling, plus
// a mid tick when the ceiling >= 100) with dotted gridlines, a CLUSTERED column pair
// (this tile's production beside the top-decile reference, sharing a baseline so
// the two heights compare directly), and a small two-row legend naming each value. @p
// ceiling spans the taller column.
void draw_production_chart(ImDrawList* dl, ImVec2 mn, ImVec2 mx,
                           float tile_val, float pct_val, float ceiling,
                           ImU32 tile_col, ImU32 pct_col)
{
    constexpr float gutter = 40.0f; // room for a tick label
    const float plot_x0 = mn.x + gutter;
    const float y0 = mn.y, y1 = mx.y;
    const ImU32 grid_col  = IM_COL32(120, 120, 120, 150);
    const ImU32 label_col = IM_COL32(150, 150, 150, 255);

    const auto y_of = [&](float v) { return y1 - (v / ceiling) * (y1 - y0); };

    const auto tick = [&](float v) {
        const float ty = y_of(v);
        dotted_hline(dl, plot_x0, mx.x, ty, grid_col);
        char buf[16];
        std::snprintf(buf, sizeof buf, "%g", static_cast<double>(v));
        const ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText({plot_x0 - 6.0f - ts.x, ty - ts.y * 0.5f}, label_col, buf);
    };
    tick(0.0f);
    if (ceiling >= 100.0f)
        tick(ceiling * 0.5f);
    tick(ceiling);

    // Clustered columns: the tile's production and the top-decile reference side by
    // side, sharing the baseline so their heights read as a direct comparison.
    constexpr float bar_w = 34.0f;
    constexpr float gap   = 10.0f;
    const float bar_x0 = plot_x0 + 6.0f;
    const float bar_x1 = bar_x0 + bar_w + gap;
    dl->AddRectFilled({bar_x0, y_of(tile_val)}, {bar_x0 + bar_w, y1}, tile_col); // this tile
    dl->AddRectFilled({bar_x1, y_of(pct_val)},  {bar_x1 + bar_w, y1}, pct_col);  // top-decile reference

    // Legend to the right of the columns: swatch + label + value, one row each.
    const float lx = bar_x1 + bar_w + 14.0f;
    const auto legend = [&](float ly, ImU32 col, const char* name, float val) {
        dl->AddRectFilled({lx, ly + 2.0f}, {lx + 10.0f, ly + 12.0f}, col);
        char buf[40];
        std::snprintf(buf, sizeof buf, "%s %.1f", name, static_cast<double>(val));
        dl->AddText({lx + 16.0f, ly}, IM_COL32(210, 210, 210, 255), buf);
    };
    legend(y0 + 2.0f,  tile_col, "Tile",     tile_val);
    legend(y0 + 22.0f, pct_col,  "Top 10%",  pct_val);
}

// --- Analog construction status (BL-095 task E) ------------------------------
// Construction is now durative *and* material-gated: each economy tick a building
// under construction advances by a RATE in [0,1] set by how much of its per-tick
// material need the local market can supply (economy_system.cpp § run_construction).
// The front door used to read this as a binary yes/no; these helpers surface it as
// an analog rate / ETA / paused status instead.

// Recompute a build's current material rate exactly as run_construction does — the
// fraction of this tick's per-material need the local market can supply, set by the
// scarcest required material and forced to 0 (paused) below 1/max_stretch. Shared by
// the prospective front door (a candidate tile + type) and the in-progress building
// card. A local mirror of the world-side formula, not a call into it, because the UI
// is const and only needs the read.
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

// A compact tick-count label. A Tick is ~3 months, so 4 ticks make a year (BL-095);
// years surface once the span reads better than a raw count.
std::string ticks_label(int ticks)
{
    std::string s = std::to_string(ticks) + (ticks == 1 ? " tick" : " ticks");
    if (ticks >= 4)
        s += " (~" + std::to_string(ticks / 4) + " yr)";
    return s;
}

// The human status for a build rate + the whole ticks of work left: paused when the
// market can't feed even the max-stretched rate; otherwise "Building..." with an ETA
// a sub-1 rate stretches (and flags as scarce).
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

// Headline label for a selected entity. Bodies carry a name; the other kinds
// have none, so their kind doubles as the title and the stat block supplies the
// distinguishing detail (coordinates, host body, etc.).
const char* selection_title(const world& w, selection_kind kind, entity_id id)
{
    switch (kind)
    {
        case selection_kind::body:        return w.bodies.at(id).name.c_str();
        case selection_kind::building:    return building_type_name(w.buildings.at(id).type);
        case selection_kind::tile:        return "Tile";
        case selection_kind::market:      return "Market";
        case selection_kind::unit:        return "Unit";
        case selection_kind::nation:      return w.nations.at(id).name.c_str();
        case selection_kind::corporation: return w.corporations.at(id).name.c_str();
        case selection_kind::none:        return "Nothing";
    }
    return "?";
}

// BL-068 competitor layout: the Selection panel for a *rival* building. Public
// facts only (type, owner, location) plus explicit teaching rows that render the
// withheld channels (production, stockpile) as visible 'private' placeholders, so
// the asymmetry is legible rather than a silent omission. The panel never leaks
// more than the hover card's facts; markets remain the sanctioned public channel.
void draw_rival_building_summary(const world& w, entity_id id)
{
    const auto it = w.buildings.find(id);
    if (it == w.buildings.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    const building_component& b = it->second;

    // Owner corporation (public) — the building type already heads the panel.
    const auto cit = w.corporations.find(owner_corp_of(w, id));
    if (cit != w.corporations.end())
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                           "Owner: %s", cit->second.name.c_str());

    // Tile location (public — the marker already sits on the surveyed map).
    const auto tile_it = w.tiles.find(b.tile);
    if (tile_it != w.tiles.end())
        ImGui::Text("Tile [%d, %d]  %s", tile_it->second.grid_x, tile_it->second.grid_y,
                    composition_name(tile_it->second.composition));

    // Withheld channels as explicit 'private' teaching rows: the grey value makes
    // the asymmetry legible rather than a silent omission. Markets stay the
    // sanctioned public channel, so the player infers output from price, not here.
    ImGui::TextDisabled("Production:  private");
    ImGui::TextDisabled("Stockpile:   private");
}

// Survey section for a selected body (BL-067). Keyed on the body's survey phase:
// hidden → a Dispatch button with the cost + ETA preview (disabled, with a reason,
// when the player cannot afford it); in_transit / scanning → live progress + ETA;
// surveyed → a settled note. The button only enqueues ui.pending_survey_dispatch;
// app::render performs the debit + schedule against its mutable world.
void draw_survey_section(const world& w, ui_state& ui, entity_id body_id)
{
    const auto bit = w.bodies.find(body_id);
    if (bit == w.bodies.end() || bit->second.type == body_type::star)
        return;
    const survey_state& s = bit->second.survey;

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Survey");

    switch (s.phase)
    {
        case survey_phase::surveyed:
            ImGui::TextDisabled("Surveyed.");
            break;
        case survey_phase::in_transit:
            ImGui::Text("En route - ETA %d d", survey_eta_days(w, body_id));
            break;
        case survey_phase::scanning:
            ImGui::Text("Surveying %d/%d - ETA %d d",
                        s.regions_done, s.regions_total, survey_eta_days(w, body_id));
            break;
        case survey_phase::hidden:
        {
            const float cost = survey_cost(w, body_id);
            const int   eta  = survey_eta_days(w, body_id);
            float balance = 0.0f;
            const auto pit = w.corporations.find(w.player_entity);
            if (pit != w.corporations.end())
                balance = pit->second.balance;
            const bool affordable = balance >= cost;

            ImGui::BeginDisabled(!affordable);
            if (ImGui::Button("Dispatch Survey"))
                ui.pending_survey_dispatch = body_id;
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("%.0f cr - ETA %d d", static_cast<double>(cost), eta);
            if (!affordable)
                ImGui::TextDisabled("Insufficient funds.");
            break;
        }
    }
}

// Commercial-activity section for a selected body (BL-089). The activity fog is
// independent of the survey fog: this reads the player's *commercial* reach, keyed
// on body_activity_visibility. Unknown → outside the network; known/visible → a
// coarse public market pulse (no internals — production/stockpiles stay private);
// known_stale → a greyed "gone cold" note.
void draw_activity_section(const world& w, entity_id body_id)
{
    const auto bit = w.bodies.find(body_id);
    if (bit == w.bodies.end() || bit->second.type == body_type::star)
        return;

    const activity_vis av = body_activity_visibility(w, body_id, w.current_day_tick);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Commercial activity");

    if (av == activity_vis::unknown)
    {
        ImGui::TextDisabled("Outside your trade network - no market data.");
        return;
    }
    if (av == activity_vis::known_stale)
    {
        ImGui::TextDisabled("Route gone cold - last market read is stale.");
        return;
    }

    // Known / visible: a coarse public market pulse from throughput (supply+demand)
    // across the body's markets. Deliberately imprecise — a strategy read, not a
    // ledger; internals stay private (BL-068).
    float throughput = 0.0f;
    for (const auto& [mid, mk] : w.markets)
    {
        if (mk.body != body_id)
            continue;
        for (std::size_t r = 0; r < resource_count; ++r)
            throughput += mk.supply[r] + mk.demand[r];
    }
    const char* level = (throughput > 400.0f) ? "busy"
                      : (throughput > 120.0f) ? "steady"
                                              : "quiet";
    ImGui::Text("Market pulse: %s", level);
    if (av == activity_vis::visible)
        ImGui::TextDisabled("Live lane / your presence.");
}

} // namespace


// Per-building profitability readout (BL-074): the selected player building's
// estimated net per-tick contribution and its component lines. Realised last-tick
// figures; revenue/inputs are estimates (the pooled market resists exact per-building
// attribution — see building_profit.hpp).
void draw_building_profit(const world& w, const recipe_registry& reg,
                          const economy_report& report, entity_id id)
{
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                       "Profitability (est. / tick)");

    const building_profit p = estimate_building_profit(w, reg, report, id);
    if (!p.has_data)
    {
        ImGui::TextDisabled("Run an economy tick to estimate.");
        return;
    }

    // Paired two-column layout — four component cells over two rows, then Net —
    // so revenue/inputs/wages/maintenance + net all fit the fixed bar height.
    const float v1 = 68.0f, l2 = 150.0f, v2 = 210.0f;
    const auto val = [](float value, ImU32 col)
    { ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%+.2f", value); };
    const auto pair = [&](const char* la, float va, ImU32 ca,
                          const char* lb, float vb, ImU32 cb)
    {
        ImGui::TextUnformatted(la);      ImGui::SameLine(v1); val(va, ca);
        ImGui::SameLine(l2);
        ImGui::TextUnformatted(lb);      ImGui::SameLine(v2); val(vb, cb);
    };

    pair("Revenue", +p.revenue,    palette::positive,
         "Inputs",  -p.input_cost, palette::negative);
    pair("Wages",   -p.wages,      palette::negative,
         "Maint",   -p.maintenance,palette::negative);

    const float net = p.net();
    const ImU32 nc = (net < 0.0f) ? palette::negative
                   : (net > 0.0f) ? palette::positive
                                  : palette::neutral;
    ImGui::TextUnformatted("Net");   ImGui::SameLine(v1); val(net, nc);
}

namespace {

// A small kind glyph for the header (BL-093) — a coloured primitive so each
// selection kind reads at a glance in place of the former placeholder portrait
// column. BL-090 promotes the corporation and building kinds to the shared corp
// emblem (shape + identity colour), so the header reads *whose* it is and matches
// the identity card and the on-canvas markers; body/tile keep their plain
// primitives. @p id is the selected entity, used to resolve the owning corp.
void draw_selection_icon(const world& w, ImDrawList* dl, selection_kind kind,
                         entity_id id, ImVec2 c, float r)
{
    const ImU32 col = palette::selection;
    switch (kind)
    {
        case selection_kind::body:
            dl->AddCircleFilled(c, r, col, 20);
            break;
        case selection_kind::corporation:
            // The selected entity *is* the corporation.
            icons::corp_emblem(dl, c, r, palette::corp_emblem_shape(id),
                               palette::corp_identity_colour(id, w.player_entity));
            break;
        case selection_kind::building:
        {
            // The owning corp's emblem (player or rival — ownership is public,
            // BL-068). Falls back to the plain square if the owner won't resolve.
            const entity_id owner = owner_corp_of(w, id);
            if (owner != null_entity)
                icons::corp_emblem(dl, c, r, palette::corp_emblem_shape(owner),
                                   palette::corp_identity_colour(owner, w.player_entity));
            else
                dl->AddRectFilled({c.x - r, c.y - r}, {c.x + r, c.y + r}, col, 2.0f);
            break;
        }
        case selection_kind::tile:
            dl->AddRect({c.x - r, c.y - r}, {c.x + r, c.y + r}, col, 2.0f, 0, 2.0f);
            break;
        default:
            dl->AddNgonFilled(c, r, col, 5);
            break;
    }
}

// The hero: the ONE primary action for this selection kind (BL-093). Actions are
// the panel's reason to exist, so they lead; reference detail is the ledgers' job.
void draw_selection_action(const world& w, const recipe_registry& reg,
                           ui_state& ui, selection_kind kind)
{
    const entity_id sel = ui.selected_entity;
    switch (kind)
    {
        // NOTE: selection_kind::tile no longer routes here — a selected tile takes
        // the dedicated vertical layout (draw_tile_selection, BL-123), not the
        // action|facts split. The remaining kinds keep the action|facts form until
        // they get their own mockups.
        case selection_kind::body:
        {
            const auto bit = w.bodies.find(sel);
            const bool star = (bit != w.bodies.end() && bit->second.type == body_type::star);
            if (!star && bit != w.bodies.end() &&
                bit->second.survey.phase != survey_phase::surveyed)
            {
                draw_survey_section(w, ui, sel);   // the move is to survey it
            }
            else if (!star)
            {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Surface");
                if (ImGui::Button("Go to surface"))
                    focus_on_entity(w, ui, sel);   // surveyed — the move is to descend
            }
            break;
        }

        case selection_kind::building:
            if (is_player_owned(w, sel))
            {
                // Under construction (BL-095 task E): lead with the live analog build
                // status — rate / ETA / paused — so a freshly-placed building reads as
                // "still building" rather than an empty card.
                if (const auto bit = w.buildings.find(sel);
                    bit != w.buildings.end() && bit->second.ticks_remaining > 0)
                {
                    const building_component& b = bit->second;
                    const float rate = construction_rate(w, reg, b.type, b.tile);
                    ImGui::TextColored(construction_status_colour(rate), "%s",
                        construction_status(rate, b.ticks_remaining).c_str());
                }
                // The tile-scoped Manage front door was removed (Ben's 2026-07-15
                // review) — a selected building shows its facts / build status only.
            }
            else
            {
                ImGui::TextDisabled("Competitor building - intel only.");
            }
            break;

        case selection_kind::market:
        case selection_kind::unit:
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Locate");
            if (ImGui::Button("Go to"))
                focus_on_entity(w, ui, sel);
            break;

        case selection_kind::nation:
        case selection_kind::corporation:
            ImGui::TextDisabled("Open its ledger via [>].");
            break;

        default:
            break;
    }
}

// The facts: only what informs the action (BL-093) — slim and muted. Everything
// encyclopedic (orbit, composition, deposits, prices) lives in the ledgers, one
// 'go to' away.
void draw_selection_facts(const world& w, const recipe_registry& reg,
                          const economy_report& report, ui_state& ui, selection_kind kind)
{
    const entity_id sel = ui.selected_entity;
    switch (kind)
    {
        // selection_kind::tile is handled by draw_tile_selection (BL-123), not here.
        case selection_kind::body:
            draw_activity_section(w, sel);          // BL-089: commercial pulse
            break;
        case selection_kind::building:
            if (is_player_owned(w, sel))
                draw_building_profit(w, reg, report, sel);   // BL-074: is it profitable?
            else
                draw_rival_building_summary(w, sel);         // owner + private rows
            break;
        default:
            break;
    }
}

// The redesigned tile Selection element (BL-123, Ben's mockup): a vertical stack
// rather than the action|facts split — a placeholder image + [x, y] caption, the
// tile's deposits as world-max-relative bar charts, and a 2x2 action button grid.
// Tile-only for now; the other kinds keep the action|facts form (draw_selection_
// action / facts) until they get their own mockups. Construct Buildings stubs onto
// the existing Construction panel (the dedicated tile-construction panel is owed);
// History and Supply are drawn but not yet wired.
void draw_tile_selection(const world& w, ui_state& ui)
{
    const entity_id sel = ui.selected_entity;
    const auto tit = w.tiles.find(sel);
    if (tit == w.tiles.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    const tile_component& tile = tit->second;
    const ImGuiStyle&     style = ImGui::GetStyle();
    ImDrawList*           dl    = ImGui::GetWindowDrawList();

    const auto centred = [&](ImVec2 mn, ImVec2 mx, const char* s, ImU32 col) {
        const ImVec2 ts = ImGui::CalcTextSize(s);
        dl->AddText({(mn.x + mx.x - ts.x) * 0.5f, (mn.y + mx.y - ts.y) * 0.5f}, col, s);
    };

    const float content_w = ImGui::GetContentRegionAvail().x;

    // ── Placeholder image (fixed top) ──
    {
        const float img_h = content_w * 0.28f;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const ImVec2 mx = {p.x + content_w, p.y + img_h};
        dl->AddRectFilled(p, mx, IM_COL32(72, 72, 72, 255), 3.0f);
        dl->AddRect(p, mx, IM_COL32(110, 110, 110, 255), 3.0f);
        centred(p, mx, "PLACEHOLDER IMAGE", IM_COL32(180, 180, 180, 255));
        ImGui::Dummy({content_w, img_h});
    }
    // ── [x, y] coordinate caption strip ──
    {
        const float cap_h = ImGui::GetFrameHeight();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const ImVec2 mx = {p.x + content_w, p.y + cap_h};
        dl->AddRectFilled(p, mx, IM_COL32(55, 55, 55, 255), 2.0f);
        char buf[32];
        std::snprintf(buf, sizeof buf, "[%d, %d]", tile.grid_x, tile.grid_y);
        centred(p, mx, buf, IM_COL32(200, 200, 200, 255));
        ImGui::Dummy({content_w, cap_h});
    }
    ImGui::Spacing();

    // Reserve the 2x2 button grid at the bottom; the bar list fills the space between.
    const float frame_h = ImGui::GetFrameHeight();
    const float btn_h   = frame_h * 2.2f;
    const float grid_h  = btn_h * 2.0f + style.ItemSpacing.y;
    float bars_h = ImGui::GetContentRegionAvail().y - grid_h - style.ItemSpacing.y * 2.0f;
    if (bars_h < 60.0f)
        bars_h = 60.0f;

    // ── Production graphs: one bordered container per deposited resource, each a
    // clustered column pair (tile production vs the top-decile reference). The list ALWAYS
    // carries a vertical scrollbar — a tile can hold more resources than fit — so the
    // player can scroll through every one. ──
    constexpr float chart_h = 80.0f;
    const float row_h = frame_h + chart_h + style.ItemSpacing.y + style.WindowPadding.y * 2.0f + 4.0f;
    constexpr ImU32 tile_col = IM_COL32(150, 235, 160, 255); // this tile's production (green)
    constexpr ImU32 p90_col  = IM_COL32(150, 160, 190, 255); // top-decile reference (muted)
    constexpr float gutter   = 40.0f;                        // == draw_production_chart gutter

    ImGui::BeginChild("##tile_graphs", {content_w, bars_h}, false,
                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    bool any_deposit = false;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (tile.resource_deposit[r] <= 0.0f)
            continue;
        any_deposit = true;
        const resource_presentation& rp = presentation_of(static_cast<resource_type>(r));

        // Each graph + its header live in one bordered container, so the header reads
        // as that graph's title rather than floating above the list.
        ImGui::BeginChild(rp.name, {0.0f, row_h}, true,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
        ImDrawList* cdl = ImGui::GetWindowDrawList(); // child-local: clips to this box

        const float a       = tile_production(tile, r);
        const float p90     = top_decile_production(w, r);
        const float peak    = std::max(a, p90);
        const float ceiling = nice_ceil(peak > 0.0f ? peak : 1.0f);

        // Header indented to the plot origin, so the name sits above its bar.
        ImGui::Indent(gutter);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", rp.name);
        ImGui::Unindent(gutter);

        const ImVec2 p  = ImGui::GetCursorScreenPos();
        const float  cw = ImGui::GetContentRegionAvail().x;
        ImGui::Dummy({cw, chart_h});
        draw_production_chart(cdl, p, {p.x + cw, p.y + chart_h}, a, p90, ceiling,
                              tile_col, p90_col);
        ImGui::EndChild();
        ImGui::Spacing();
    }
    if (!any_deposit)
        ImGui::TextDisabled("No deposits");
    ImGui::EndChild();

    // ── Action buttons ──
    // This element is the *unbuilt-tile* prospecting view (Ben's 2026-07-15 review):
    // its job is "is this tile worth building on?", so Construct is the primary action
    // and leads full-width. The tile-scoped Manage front door is gone — a built tile's
    // management is reached elsewhere, not from the prospecting element.
    const float bw  = (content_w - style.ItemSpacing.x) * 0.5f;
    const ImVec2 bsz = {bw, btn_h};

    if (ImGui::Button("Construct\nBuildings", {content_w, btn_h}))
        ui.show_build_ledger = true; // opens the tile construction ledger (BL-162)

    // History / Supply: drawn for layout completeness, wired to nothing yet
    // (BL-123 § stubs). The tooltip keeps the no-op honest rather than silent.
    const auto stub = [&](const char* label) {
        ImGui::Button(label, bsz);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Not yet available");
    };
    stub("History");
    ImGui::SameLine();
    stub("Supply");
}

// Resolve the (at most one) building occupying a tile — the standing invariant that a
// tile carries zero or one building means this never needs a list. Lets a selected
// *built* tile read as its building (Ben's 2026-07-16 steer): the tile element is the
// unbuilt-tile prospecting view; anything built shows the management view.
entity_id building_on_tile(const world& w, entity_id tile)
{
    for (const auto& [id, b] : w.buildings)
        if (b.tile == tile)
            return id;
    return null_entity;
}

// The primary output resource of a recipe — the argmax of its outputs. Tags a
// production method with a resource pip glyph (proper per-method glyphs are owed).
resource_type primary_output_resource(const recipe& r)
{
    std::size_t best = 0;
    float best_v = -1.0f;
    for (std::size_t i = 0; i < resource_count; ++i)
        if (r.outputs[i] > best_v) { best_v = r.outputs[i]; best = i; }
    return static_cast<resource_type>(best);
}

// The building management view (Ben's 2026-07-15 mockup; moved here from the Building
// ledger's Buildings tab on his 2026-07-16 steer, so the *selection* carries it — the
// ledger is now Construction-only). A placeholder image, the production-method
// dropdown, profit + workforce history, and the workforce-target control. The panel
// header already names the building, so this starts at the image rather than repeating
// a title.
//
// The graphs read PLACEHOLDER deterministic series — no per-building history is
// recorded yet (owed with BL-181); the profit line anchors to the live estimate. The
// profit series is coloured by its own trend (green when the latest quarter beats the
// oldest sample, ~3 years back at 12 quarters; red otherwise); workforce is amber.
void draw_building_selection(world& w, const recipe_registry& reg,
                             const economy_report& report, entity_id building_id)
{
    const auto bit = w.buildings.find(building_id);
    if (bit == w.buildings.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    building_component& b = bit->second;

    ImDrawList* dl        = ImGui::GetWindowDrawList();
    const float content_w = ImGui::GetContentRegionAvail().x;

    // ── Placeholder image ──
    {
        const float  img_h = content_w * 0.30f;
        const ImVec2 p     = ImGui::GetCursorScreenPos();
        const ImVec2 mx    = {p.x + content_w, p.y + img_h};
        dl->AddRectFilled(p, mx, IM_COL32(72, 72, 72, 255), 3.0f);
        dl->AddRect(p, mx, IM_COL32(110, 110, 110, 255), 3.0f);
        const char*  ph = "PLACEHOLDER IMAGE";
        const ImVec2 ts = ImGui::CalcTextSize(ph);
        dl->AddText({(p.x + mx.x - ts.x) * 0.5f, (p.y + mx.y - ts.y) * 0.5f},
                    IM_COL32(180, 180, 180, 255), ph);
        ImGui::Dummy({content_w, img_h});
    }

    // ── Production Methods: the building's recipes, each tagged with its output pip ──
    ImGui::SeparatorText("Production Methods");
    const int   n_recipes  = reg.recipe_count(b.type);
    const float pipr       = ImGui::GetFontSize() * 0.42f;
    const auto  method_res = [&](int i) -> resource_type {
        if (b.type == building_type::extraction_site) return b.target_resource;
        return primary_output_resource(reg.recipe_at(b.type, i));
    };
    if (n_recipes >= 1)
    {
        b.active_recipe_index = std::clamp(b.active_recipe_index, 0, n_recipes - 1);
        const recipe& cur     = reg.recipe_at(b.type, b.active_recipe_index);
        const char*   preview = cur.name.empty() ? "-" : cur.name.c_str();

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
    else
        ImGui::TextDisabled("Single method");

    // ── Profit + Workforce history (placeholder series) ──
    // 12 quarters == 3 years, so sample 0 is the "up to 3 years ago" reference the
    // trend colour compares the latest quarter against.
    constexpr int N       = 12;
    const float   graph_w = ImGui::GetContentRegionAvail().x;
    const building_profit prof = estimate_building_profit(w, reg, report, building_id);

    float profit_series[N];
    const float p_end = prof.has_data ? prof.net() : 0.0f;
    for (int i = 0; i < N; ++i)
    {
        const float t = static_cast<float>(i) / (N - 1);
        profit_series[i] = (p_end - 35.0f) + t * 35.0f; // gentle ramp to the current estimate
    }
    const bool  profit_up  = profit_series[N - 1] >= profit_series[0];
    const ImU32 profit_col = profit_up ? palette::positive : palette::negative;
    ImGui::SeparatorText("Profit");
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImGui::ColorConvertU32ToFloat4(profit_col));
    ImGui::PlotLines("##profit", profit_series, N, 0, nullptr, FLT_MAX, FLT_MAX, {graph_w, 60.0f});
    ImGui::PopStyleColor();

    float wf_series[N];
    const float wf = static_cast<float>(b.workforce_target);
    for (int i = 0; i < N; ++i)
    {
        const float t   = static_cast<float>(i) / (N - 1);
        const float dip = -20.0f * std::sin(t * 3.14159265f); // placeholder dip-and-recover
        wf_series[i]    = std::clamp(wf + dip, 0.0f, 200.0f);
    }
    ImGui::SeparatorText("Workforce");
    ImGui::PushStyleColor(ImGuiCol_PlotLines, ImGui::ColorConvertU32ToFloat4(palette::workforce));
    // Scale over the workforce target's FULL range (0–200 %), not 0–120: the BL-181
    // solver routinely picks 200 % on a strong site, which a 120 ceiling clipped off the
    // top — the line read as flat/absent (caught in the 2026-07-16 scarcity playtest).
    ImGui::PlotLines("##workforce", wf_series, N, 0, nullptr, 0.0f, 200.0f, {graph_w, 60.0f});
    ImGui::PopStyleColor();

    // ── Workforce Target: Auto (the economy tick solves the profit-max target each
    // tick, BL-181) plus a manual 0–100 grid. A manual tier pins the target and clears
    // Auto; the Auto button re-enables it and shows the current solved value. ──
    ImGui::SeparatorText("Workforce Target");
    {
        if (b.workforce_auto)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        char autolbl[32];
        if (b.workforce_auto)
            std::snprintf(autolbl, sizeof autolbl, "Auto  (%d%%)", b.workforce_target);
        else
            std::snprintf(autolbl, sizeof autolbl, "Auto");
        if (ImGui::Button(autolbl, {ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() * 1.3f}))
            b.workforce_auto = true;
        if (b.workforce_auto)
            ImGui::PopStyleColor();

        const int   tiers[] = {0, 20, 40, 60, 80, 100};
        const float sp = ImGui::GetStyle().ItemSpacing.x;
        const float bw = (ImGui::GetContentRegionAvail().x - sp * 2.0f) / 3.0f;
        const float bh = ImGui::GetFrameHeight() * 1.4f;
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

    // Under-construction status + decommission (not in the mockup, but load-bearing).
    if (b.ticks_remaining > 0)
    {
        const float rate = construction_rate(w, reg, b.type, b.tile);
        ImGui::Spacing();
        ImGui::SeparatorText("Under construction");
        ImGui::TextColored(construction_status_colour(rate), "%s",
                           construction_status(rate, b.ticks_remaining).c_str());
    }

    ImGui::Spacing();
    if (b.decommissioned)
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative), "DECOMMISSIONED");
    else if (ImGui::Button("Decommission"))
        b.decommissioned = true;
}

} // namespace

void draw_selection_panel(world& w, const recipe_registry& reg,
                          const economy_report& report, ui_state& ui)
{
    // A selected *built* tile reads as its building (Ben's 2026-07-16 steer) — so the
    // header names the building and the content is its management view, while the tile
    // element stays the unbuilt-tile prospecting view. Remapped before the header so
    // both agree; the hide/close checks stay keyed on the player's actual selection.
    entity_id      sel  = ui.selected_entity;
    selection_kind kind = selection_kind_of(w, sel);
    if (kind == selection_kind::tile)
    {
        const entity_id on_tile = building_on_tile(w, sel);
        if (on_tile != null_entity)
        {
            sel  = on_tile;
            kind = selection_kind::building;
        }
    }

    // Hidden when nothing valid is selected, or when the player dismissed this
    // exact selection (close hides until the next selection re-shows it).
    if (kind == selection_kind::none)
        return;
    if (ui.selected_entity == ui.selection_hidden_for)
        return;

    // --- Layout ---
    // The Selection element now fills the shell fold-out column (foldout_column_rect),
    // the same slot the ledgers use, and is mutually exclusive with them (app closes
    // any open ledger on a new selection and gates this draw on !any_panel_open).
    // NOTE: the content below still uses the wide-bottom-bar action|facts split;
    // its re-lay-out for this narrower, taller column is BL-123 (Ben to mock).
    const foldout_rect r       = foldout_column_rect();
    const float        bar_w   = r.w;
    const float        frame_h = ImGui::GetFrameHeight();
    const ImGuiStyle&  style   = ImGui::GetStyle();

    ImGui::SetNextWindowPos({r.x, r.y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({r.w, r.h}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.90f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar            |
        ImGuiWindowFlags_NoResize              |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoCollapse            |
        ImGuiWindowFlags_NoNav                 |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar           |
        ImGuiWindowFlags_NoScrollWithMouse     |
        ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##selection_info", nullptr, flags);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ── Header: [icon] Name · type ............................. [>] [x] ──
    {
        const ImVec2 hc = ImGui::GetCursorScreenPos();
        const float  ir = frame_h * 0.40f;
        draw_selection_icon(w, dl, kind, sel,
                            {hc.x + ir, hc.y + frame_h * 0.5f}, ir);
        ImGui::SetCursorScreenPos({hc.x + ir * 2.0f + style.ItemSpacing.x, hc.y});

        const char* title = selection_title(w, kind, sel);
        const char* kname = selection_kind_name(kind);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", title);
        // Suppress the redundant type label when the title is already the kind name
        // (e.g. a tile titles as "Tile"), avoiding "Tile · Tile".
        if (std::strcmp(title, kname) != 0)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", kname);
        }

        const float btn = frame_h;
        ImGui::SameLine(bar_w - style.WindowPadding.x - 2.0f * btn - style.ItemSpacing.x);
        if (ImGui::Button(">", {btn, btn}))
            focus_on_entity(w, ui, sel);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Go to");
        ImGui::SameLine();
        if (ImGui::Button("x", {btn, btn}))
            ui.selection_hidden_for = ui.selected_entity;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Close");
    }

    ImGui::Separator();

    // Tile and building each take a dedicated vertical layout (Ben's mockups); the
    // other kinds keep the action|facts split until they get their own. A tile only
    // reaches here unbuilt — a built one was remapped to its building above.
    if (kind == selection_kind::tile)
    {
        draw_tile_selection(w, ui);
        ImGui::End();
        return;
    }
    if (kind == selection_kind::building)
    {
        draw_building_selection(w, reg, report, sel);
        ImGui::End();
        return;
    }

    // ── Action (left, dominant) │ Facts (right, muted) ──
    const float content_w = bar_w - style.WindowPadding.x * 2.0f;
    const float action_w  = (content_w - style.ItemSpacing.x) * 0.58f;

    // Two fill-height columns: action leads at a fixed split, facts to its right.
    // Container policy (LAYOUT.md): the Selection element wraps text rather than
    // clipping it, with vertical scroll as the overflow behaviour — so each column
    // wraps at its own width (PushTextWrapPos(0.0f) wraps at the current window's
    // right edge) and keeps its scrollbar instead of suppressing it.
    ImGui::BeginChild("##sel_action", {action_w, 0.0f}, false,
                      ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushTextWrapPos(0.0f);
    draw_selection_action(w, reg, ui, kind);
    ImGui::PopTextWrapPos();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##sel_facts", {0.0f, 0.0f}, false,
                      ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushTextWrapPos(0.0f);
    draw_selection_facts(w, reg, report, ui, kind);
    ImGui::PopTextWrapPos();
    ImGui::EndChild();

    ImGui::End();
}

// The tile construction ledger (BL-162): the tile-contextual surface that actually
// lets the player build. Lists every building type placeable on the selected tile —
// each in a bordered container (a placeholder image + name + full cost + a reason-coded
// validity read + a Build action) — and enqueues the chosen build on the tile via the
// construction.pending_tile seam app executes. First pass; per BL-162 the deposit
// graphs' place is eventually taken by an expected-profit chart, and the images are
// placeholders.
void draw_construction_ledger(const world& w, const recipe_registry& reg, ui_state& ui)
{
    const entity_id tile_id = ui.selected_entity;
    const auto tit = w.tiles.find(tile_id);
    if (tit == w.tiles.end())
    {
        ui.show_build_ledger = false; // selection is not a tile — nothing to build on
        return;
    }
    const tile_component& tile = tit->second;

    const foldout_rect r       = foldout_column_rect();
    const float        bar_w   = r.w;
    const float        frame_h = ImGui::GetFrameHeight();
    const ImGuiStyle&  style   = ImGui::GetStyle();

    ImGui::SetNextWindowPos({r.x, r.y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({r.w, r.h}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.90f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar            |
        ImGuiWindowFlags_NoResize              |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoCollapse            |
        ImGuiWindowFlags_NoNav                 |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar           |
        ImGuiWindowFlags_NoScrollWithMouse     |
        ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##build_ledger", nullptr, flags);

    // ── Header: Construct · [x, y] ............................... [x] ──
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Construct");
    ImGui::SameLine();
    ImGui::TextDisabled("[%d, %d]", tile.grid_x, tile.grid_y);
    const float btn = frame_h;
    ImGui::SameLine(bar_w - style.WindowPadding.x - btn);
    if (ImGui::Button("x", {btn, btn}))
        ui.show_build_ledger = false; // back to the tile Selection element
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Close");

    ImGui::Separator();

    // Player balance — the affordability context for every Build button below.
    float balance = 0.0f;
    if (const auto pit = w.corporations.find(w.player_entity); pit != w.corporations.end())
        balance = pit->second.balance;
    ImGui::TextDisabled("Balance: %.0f cr", static_cast<double>(balance));

    // Last construction outcome (set by app after executing a request).
    if (!ui.construction.last_message.empty())
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::neutral), "%s",
                           ui.construction.last_message.c_str());

    // Candidate placements for this tile: one extraction option per extractable
    // resource actually deposited here, then the fixed processing / port / launchpad
    // types. Validity + reason come from the shared placement_rules seam.
    struct candidate { building_type type; resource_type target; std::string name; };
    std::vector<candidate> cands;
    for (const resource_type er : placement_rules::k_extractable)
        if (tile.resource_deposit[static_cast<std::size_t>(er)] > 0.0f)
            cands.push_back({building_type::extraction_site, er,
                             std::string("Extraction: ") + resource_name(er)});
    cands.push_back({building_type::processing_facility,  resource_type::iron_ore, "Processing Facility"});
    cands.push_back({building_type::port,                 resource_type::iron_ore, "Port"});
    cands.push_back({building_type::launchpad,            resource_type::iron_ore, "Launchpad"});
    cands.push_back({building_type::inland_logistics_hub, resource_type::iron_ore, "Inland Logistics Hub"}); // BL-149

    constexpr float img   = 56.0f;
    const float     row_h = img + style.WindowPadding.y * 2.0f + 8.0f;

    ImGui::BeginChild("##build_list", {0.0f, 0.0f}, false,
                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    for (const candidate& c : cands)
    {
        const building_economics& econ = reg.economics(c.type);
        const placement_rules::placement_result pr =
            placement_rules::can_place_in_world(w, tile_id, c.type, c.target);
        const bool affordable = balance >= econ.build_cost;

        ImGui::BeginChild(c.name.c_str(), {0.0f, row_h}, true,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
        ImDrawList* cdl = ImGui::GetWindowDrawList();

        // Placeholder image: a grey box carrying the building's marker glyph.
        const ImVec2 ip = ImGui::GetCursorScreenPos();
        const ImVec2 imx = {ip.x + img, ip.y + img};
        cdl->AddRectFilled(ip, imx, IM_COL32(72, 72, 72, 255), 3.0f);
        cdl->AddRect(ip, imx, IM_COL32(110, 110, 110, 255), 3.0f);
        icons::building(cdl, {ip.x + img * 0.5f, ip.y + img * 0.5f}, img * 0.28f, c.type,
                        IM_COL32(150, 235, 160, 255));
        ImGui::Dummy({img, img});
        ImGui::SameLine();

        ImGui::BeginGroup();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", c.name.c_str());

        // Cost: budget then any material requirements.
        std::string cost = std::to_string(static_cast<int>(econ.build_cost)) + " cr";
        for (std::size_t i = 0; i < resource_count; ++i)
            if (econ.resource_build_cost[i] > 0.0f)
                cost += ", " + std::to_string(static_cast<int>(econ.resource_build_cost[i]))
                      + " " + presentation_of(static_cast<resource_type>(i)).abbrev;
        ImGui::TextDisabled("%s", cost.c_str());

        // Action: Build when valid (disabled + noted when unaffordable); else the reason.
        if (pr.ok())
        {
            ImGui::BeginDisabled(!affordable);
            if (ImGui::Button("Build"))
            {
                ui.construction.pending_tile   = tile_id;
                ui.construction.pending_type   = c.type;
                ui.construction.pending_target = c.target;
            }
            ImGui::EndDisabled();
            if (!affordable)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("Can't afford");
            }
        }
        else
        {
            ImGui::TextColored(ImVec4{0.90f, 0.55f, 0.55f, 1.0f}, "%s", pr.message());
        }
        ImGui::EndGroup();

        ImGui::EndChild();
        ImGui::Spacing();
    }

    // Road placement (BL-147 core, BL-172 tier ladder) — a per-tile mutation, not a building, so
    // it takes its own affordances + the pending_road_tile/tier path (place_road). Three tiers:
    // Track (1) / Road (2) / Highway (3); the glyph weight + brightness mirror the on-canvas
    // render, and each shows its own cost + validity (upgrade-in-place is allowed, so a tile that
    // already carries an equal-or-better road greys out only the tiers it meets or exceeds).
    {
        struct road_tier_affordance
        {
            const char*  name;
            std::uint8_t level;
            float        glyph_thick; // matches the canvas weight ladder
            ImU32        glyph_col;
        };
        static const road_tier_affordance road_tiers[3] = {
            { "Track",   1, 2.0f, IM_COL32(175, 158, 120, 255) },
            { "Road",    2, 3.0f, IM_COL32(205, 188, 140, 255) },
            { "Highway", 3, 4.0f, IM_COL32(225, 205, 150, 255) },
        };

        for (const road_tier_affordance& rt : road_tiers)
        {
            ImGui::PushID(static_cast<int>(rt.level)); // unique ImGui ids per tier

            const road_economics& re = reg.road_econ(rt.level);
            const placement_rules::placement_result pr =
                placement_rules::can_place_road(tile, rt.level);
            const bool affordable = balance >= re.build_cost;

            ImGui::BeginChild("road##build", {0.0f, row_h}, true,
                              ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
            ImDrawList* cdl = ImGui::GetWindowDrawList();

            const ImVec2 ip = ImGui::GetCursorScreenPos();
            const ImVec2 imx = {ip.x + img, ip.y + img};
            cdl->AddRectFilled(ip, imx, IM_COL32(72, 72, 72, 255), 3.0f);
            cdl->AddRect(ip, imx, IM_COL32(110, 110, 110, 255), 3.0f);
            // A short road glyph: a segment across the box, weighted by tier (matches the canvas).
            cdl->AddLine({ip.x + img * 0.2f, ip.y + img * 0.7f},
                         {ip.x + img * 0.8f, ip.y + img * 0.3f}, rt.glyph_col, rt.glyph_thick);
            ImGui::Dummy({img, img});
            ImGui::SameLine();

            ImGui::BeginGroup();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", rt.name);

            std::string cost = std::to_string(static_cast<int>(re.build_cost)) + " cr";
            for (std::size_t i = 0; i < resource_count; ++i)
                if (re.resource_build_cost[i] > 0.0f)
                    cost += ", " + std::to_string(static_cast<int>(re.resource_build_cost[i]))
                          + " " + presentation_of(static_cast<resource_type>(i)).abbrev;
            ImGui::TextDisabled("%s", cost.c_str());

            if (pr.ok())
            {
                ImGui::BeginDisabled(!affordable);
                if (ImGui::Button("Build"))
                {
                    ui.construction.pending_road_tile = tile_id;
                    ui.construction.pending_road_tier = rt.level;
                }
                ImGui::EndDisabled();
                if (!affordable)
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("Can't afford");
                }
            }
            else
            {
                ImGui::TextColored(ImVec4{0.90f, 0.55f, 0.55f, 1.0f}, "%s", pr.message());
            }
            ImGui::EndGroup();

            ImGui::EndChild();
            ImGui::Spacing();
            ImGui::PopID();
        }
    }

    ImGui::EndChild();

    ImGui::End();
}

} // namespace ui
