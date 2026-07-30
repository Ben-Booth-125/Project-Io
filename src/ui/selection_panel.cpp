#include "selection_panel.hpp"

#include "charts.hpp"

#include "foldout_column.hpp" // shell fold-out column host (shared with the ledgers)
#include "hex_render.hpp"      // draw_tile_neighbourhood — the card's zoomed tile view
#include "icons.hpp"
#include "presentation.hpp"
#include "selection.hpp"
#include "view_nav.hpp"

#include "world/building_profit.hpp" // per-building profitability estimate (BL-074)
#include "world/economy_system.hpp" // economy_report (workforce cap, BL-069)
#include "world/market_clearing.hpp"
#include "world/construction.hpp"    // demolish path (the building element's Demolish)
#include "world/placement_rules.hpp" // buildable-type validity + stack capacity
#include "world/recipe_registry.hpp" // recipe/economics lookups for the building element
#include "world/survey_system.hpp"

#include <imgui.h>

#include <algorithm> // std::min/clamp (build rate), std::nth_element (top-decile production)
#include <cmath>     // std::ceil (construction ETA), std::pow/log10/floor (nice_ceil axis)
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

// nice_ceil and dotted_hline now live in ui/charts.hpp — every chart in the app
// shares one implementation rather than imitating this one.
using ui::charts::nice_ceil;

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
                           ImU32 tile_col, ImU32 pct_col,
                           const char* ref_label = "Top 10%")
{
    const ui::charts::bar bars[2] = {
        { tile_val, tile_col, "Tile",    false },
        { pct_val,  pct_col,  ref_label, false },
    };
    ui::charts::draw_bars(dl, mn, mx, bars, 2, ceiling, "%.1f", 34.0f);
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
// --- Tile-card action-strip glyphs -------------------------------------------
// Compact glyphs for the right-hand icon strip (Ben's 2026-07-23 layout). Drawn
// centred at @p c within radius @p r, matching the (dl, centre, r, colour) contract
// the ui::icons vocabulary uses. Construct/Manage/History are drawn here; Supply
// reuses ui::icons::supply. Kept local for now; promote to ui::icons if they stick.
constexpr float kTwoPiLocal = 6.2831853f;

void glyph_hammer(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddRectFilled({c.x - r * 0.55f, c.y - r * 0.7f}, {c.x + r * 0.7f, c.y - r * 0.28f}, col, 1.5f);
    dl->AddLine({c.x - r * 0.05f, c.y - r * 0.35f}, {c.x - r * 0.45f, c.y + r * 0.75f}, col, 2.2f);
}

void glyph_gear(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    for (int i = 0; i < 8; ++i)
    {
        const float a = kTwoPiLocal * static_cast<float>(i) / 8.0f;
        const ImVec2 d{std::cos(a), std::sin(a)};
        dl->AddLine({c.x + d.x * r * 0.42f, c.y + d.y * r * 0.42f},
                    {c.x + d.x * r * 0.72f, c.y + d.y * r * 0.72f}, col, 2.0f);
    }
    dl->AddCircle(c, r * 0.42f, col, 14, 2.0f);
    dl->AddCircleFilled(c, r * 0.16f, col);
}

void glyph_clock(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddCircle(c, r * 0.72f, col, 18, 2.0f);
    dl->AddLine(c, {c.x, c.y - r * 0.5f}, col, 2.0f);
    dl->AddLine(c, {c.x + r * 0.42f, c.y + r * 0.08f}, col, 2.0f);
}

// A faint dot — the neutral "nothing assigned yet" glyph for a reserved slot
// in the 3x2 action grid (BL-213 follow-up, 2026-07-28: the grid grew from 4
// actions to 6 slots; two have no action yet).
void glyph_reserved(ImDrawList* dl, ImVec2 c, float r, ImU32 col)
{
    dl->AddCircle(c, r * 0.30f, col, 12, 1.5f);
}

// A square icon button: an ImGui::Button frame with a glyph drawn over it and a
// hover tooltip (Ben's call: icons, text only on hover). Disabled buttons dim the
// glyph and show the reason. Returns true only on an enabled click.
bool tile_icon_button(const char* id, ImVec2 sz, bool enabled, const char* tip,
                      void (*glyph)(ImDrawList*, ImVec2, float, ImU32))
{
    ImGui::BeginDisabled(!enabled);
    const ImVec2 p       = ImGui::GetCursorScreenPos();
    const bool   clicked = ImGui::Button(id, sz);
    ImGui::EndDisabled();
    const ImU32 gcol = enabled ? IM_COL32(215, 215, 220, 255) : IM_COL32(110, 110, 116, 255);
    glyph(ImGui::GetWindowDrawList(), {p.x + sz.x * 0.5f, p.y + sz.y * 0.5f},
          std::min(sz.x, sz.y) * 0.5f, gcol);
    if (ImGui::IsItemHovered() && tip)
        ImGui::SetTooltip("%s", tip);
    return clicked && enabled;
}

// The tile card (Ben's 2026-07-23 layout). Three regions: a header (icon + Tile
// [x,y] + close); a middle band split into a zoomed hex-neighbourhood render (left)
// and a thin vertical icon action strip (right); and a bottom third holding the
// per-resource production graphs (each click-drillable into its time series, BL-196).
// The zoomed render is the point — the card now SHOWS the tile it floats over, so it
// no longer merely obscures it.
void draw_tile_selection(world& w, ui_state& ui)
{
    const entity_id sel = ui.selected_entity;
    const auto tit = w.tiles.find(sel);
    if (tit == w.tiles.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    const tile_component& tile   = tit->second;
    const ImGuiStyle&     style  = ImGui::GetStyle();
    ImDrawList*           dl     = ImGui::GetWindowDrawList();
    const float           avail  = ImGui::GetContentRegionAvail().x;
    const float           frame_h = ImGui::GetFrameHeight();

    // ── Header: [icon] Tile [x, y] ............................ [x] ──
    {
        const ImVec2 hc = ImGui::GetCursorScreenPos();
        const float  ir = frame_h * 0.40f;
        draw_selection_icon(w, dl, selection_kind::tile, sel,
                            {hc.x + ir, hc.y + frame_h * 0.5f}, ir);
        ImGui::SetCursorScreenPos({hc.x + ir * 2.0f + style.ItemSpacing.x, hc.y});
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                           "Tile [%d, %d]", tile.grid_x, tile.grid_y);
        ImGui::SameLine(avail - frame_h);
        if (ImGui::Button("x", {frame_h, frame_h}))
            ui.selection_hidden_for = sel;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Close");
    }
    ImGui::Separator();

    // Three columns spanning the full band width (Ben, 2026-07-28): 1/4 for the
    // zoomed hex neighbourhood, 1/2 for a paged metric ACCORDION (one titled
    // graph at a time — every deposited resource, plus the tile's own
    // habitability/hazard scalars, so there's more to see here than a single
    // resource), and 1/4 for a bigger 3x2 action-button grid. Supersedes the
    // earlier layout (hex+strip band over a full-width bottom accordion).
    const float total_h  = ImGui::GetContentRegionAvail().y;
    const float spacing  = style.ItemSpacing.x;
    const float left_w   = avail * 0.25f;
    const float right_w  = avail * 0.25f;
    const float center_w = std::max(80.0f, avail - left_w - right_w - 2.0f * spacing);

    constexpr ImU32 tile_col = IM_COL32(150, 235, 160, 255); // this tile's value (green)
    constexpr ImU32 ref_col  = IM_COL32(150, 160, 190, 255); // reference value (muted)

    // ── Left quarter: zoomed hex neighbourhood ──
    {
        const ImVec2 p  = ImGui::GetCursorScreenPos();
        const ImVec2 mx = {p.x + left_w, p.y + total_h};
        dl->AddRectFilled(p, mx, IM_COL32(16, 18, 24, 255), 3.0f);
        draw_tile_neighbourhood(dl, w, sel, p, {left_w, total_h}, /*radius=*/2);
        dl->AddRect(p, mx, IM_COL32(90, 90, 100, 255), 3.0f);
        ImGui::Dummy({left_w, total_h});
    }
    ImGui::SameLine();

    // ── Centre half: paged metric accordion ──
    // Pages: every deposited resource (tile production vs. the top-decile tile
    // for that resource, as before), THEN the tile's own habitability and hazard
    // (already tracked per-tile — tile_inspector.cpp's table carries the same
    // two columns — vs. this body's average, so a barren tile still has
    // something to page through). Atmospheric pollution and per-tile population
    // are NOT modelled today (population lives on population centres, not
    // arbitrary tiles), so they have no page here yet — noted, not faked.
    {
        struct metric_page
        {
            std::string label;
            float       tile_val;
            float       ref_val;
            const char* ref_label;
            float       ceiling;
            int         resource_index; // -1 = not drillable (no time-series history kept)
        };
        std::vector<metric_page> pages;

        std::vector<std::size_t> deposits;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (tile.resource_deposit[r] > 0.0f)
                deposits.push_back(r);
        for (std::size_t r : deposits)
        {
            const float a   = tile_production(tile, r);
            const float p90 = top_decile_production(w, r);
            const resource_presentation& rp = presentation_of(static_cast<resource_type>(r));
            pages.push_back({ rp.name, a, p90, "Top 10%",
                              nice_ceil(std::max(a, p90) > 0.0f ? std::max(a, p90) : 1.0f),
                              static_cast<int>(r) });
        }
        {
            float sum_hab = 0.0f, sum_haz = 0.0f;
            int   n = 0;
            for (const auto& [id, t] : w.tiles)
                if (t.body == tile.body) { sum_hab += t.habitability; sum_haz += t.hazard_level; ++n; }
            const float avg_hab = n > 0 ? sum_hab / static_cast<float>(n) : 0.0f;
            const float avg_haz = n > 0 ? sum_haz / static_cast<float>(n) : 0.0f;
            pages.push_back({ "Habitability", tile.habitability, avg_hab, "Body avg", 1.0f, -1 });
            pages.push_back({ "Hazard",       tile.hazard_level, avg_haz, "Body avg", 1.0f, -1 });
        }

        ImGui::BeginChild("##tile_accordion", {center_w, total_h}, true,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

        int&      page = ui.card_resource_page;
        const int n    = static_cast<int>(pages.size());
        page = std::clamp(page, 0, n - 1);
        const metric_page& mp = pages[static_cast<std::size_t>(page)];

        // Pager row: ‹  Metric (i/N)  ›
        const float aw = ImGui::GetContentRegionAvail().x;
        ImGui::BeginDisabled(page == 0);
        if (ImGui::ArrowButton("##res_prev", ImGuiDir_Left)) --page;
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered() && page > 0)
            ImGui::SetTooltip("Previous");

        char hdr[64];
        std::snprintf(hdr, sizeof hdr, "%s  (%d/%d)", mp.label.c_str(), page + 1, n);
        const float name_w = ImGui::CalcTextSize(hdr).x;
        ImGui::SameLine(std::max(frame_h + style.ItemSpacing.x, (aw - name_w) * 0.5f));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", hdr);

        ImGui::SameLine(aw - frame_h);
        ImGui::BeginDisabled(page == n - 1);
        if (ImGui::ArrowButton("##res_next", ImGuiDir_Right)) ++page;
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered() && page < n - 1)
            ImGui::SetTooltip("Next");

        // The current page's graph, filling the rest of the container. Deposited
        // resources are click-drillable into their time series (BL-196);
        // habitability/hazard are not — no per-tile history is tracked for them
        // yet, so the click target is skipped rather than faked.
        ImGui::Spacing();
        const ImVec2 p  = ImGui::GetCursorScreenPos();
        const float  cw = ImGui::GetContentRegionAvail().x;
        const float  gh = std::max(48.0f, ImGui::GetContentRegionAvail().y - 4.0f);
        const bool   drillable = mp.resource_index >= 0;
        if (drillable)
        {
            if (ImGui::InvisibleButton("##res_chart", {cw, gh}))
            {
                const bool dup = !ui.card_stack.empty() &&
                                 ui.card_stack.back().tile == sel &&
                                 ui.card_stack.back().resource == mp.resource_index;
                if (!dup && ui.card_stack.size() < 20)
                    ui.card_stack.push_back({sel, mp.resource_index});
                ui.card_track_tile = sel;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s: this tile yields %.1f vs %.1f for a top-10%% tile.\n"
                                  "Click for its history over time.",
                                  mp.label.c_str(), static_cast<double>(mp.tile_val),
                                  static_cast<double>(mp.ref_val));
        }
        else
        {
            ImGui::Dummy({cw, gh}); // reserve the same space; no drill-down yet
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s: %.2f vs %.2f %s.\nNo time-series history tracked for this yet.",
                                  mp.label.c_str(), static_cast<double>(mp.tile_val),
                                  static_cast<double>(mp.ref_val), mp.ref_label);
        }
        draw_production_chart(ImGui::GetWindowDrawList(), p, {p.x + cw, p.y + gh},
                              mp.tile_val, mp.ref_val, mp.ceiling, tile_col, ref_col,
                              mp.ref_label);

        ImGui::EndChild();
    }
    ImGui::SameLine();

    // ── Right quarter: bigger 2x3 action-button grid (Ben, 2026-07-28 — up from
    // a thin vertical icon strip; 2 columns x 3 rows rather than 3x2, since the
    // quarter-width column is too narrow for 3 across to read as "bigger" —
    // 2 wide gives chunkier buttons in the same slot count). Two slots are
    // reserved: only four actions exist today (Construct, Manage, History,
    // Supply), and the grid's shape should not have to change when a
    // fifth/sixth lands. ──
    {
        ImGui::BeginChild("##tile_actions", {right_w, total_h}, false,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
        const float  bw  = (right_w - spacing) / 2.0f;
        const float  bh  = (total_h - 2.0f * spacing) / 3.0f;
        const ImVec2 bsz = {bw, bh};

        // Construct — opens the tile construction ledger (BL-162).
        if (tile_icon_button("##act_construct", bsz, /*enabled=*/true,
                             "Construct buildings", glyph_hammer))
            ui.show_build_ledger = true;
        ImGui::SameLine();

        // Manage — enabled only when a building occupies this tile; selecting it
        // switches the band to the building layout next frame.
        entity_id bld_here = null_entity;
        for (const auto& [bid, bc] : w.buildings)
            if (bc.tile == sel) { bld_here = bid; break; }
        if (tile_icon_button("##act_manage", bsz, bld_here != null_entity,
                             bld_here != null_entity ? "Manage building" : "No building here",
                             glyph_gear) &&
            bld_here != null_entity)
        {
            ui.selected_entity      = bld_here;
            ui.selection_hidden_for = null_entity;
        }

        tile_icon_button("##act_history", bsz, /*enabled=*/false,
                         "History (not yet available)", glyph_clock);
        ImGui::SameLine();
        tile_icon_button("##act_supply", bsz, /*enabled=*/false,
                         "Supply (not yet available)", ui::icons::supply);

        tile_icon_button("##act_reserved1", bsz, /*enabled=*/false, "Reserved", glyph_reserved);
        ImGui::SameLine();
        tile_icon_button("##act_reserved2", bsz, /*enabled=*/false, "Reserved", glyph_reserved);

        ImGui::EndChild();
    }
}

// --- The building Selection element (Ben's 2026-07-22 review) ----------------
// The old card showed four numbers and a Net line, and nothing else — Ben's verdict
// was that the screen is useless. The four questions it now answers, in his words:
// how profitable is this building; what can I do to make it more profitable; how
// many more of this can I build; how do I remove it and build something else.
//
// Output leads, not profit (his call). Profit is a consequence you read second; the
// physical fact of what the thing is producing, and whether that is anywhere near
// what it could produce, is what you act on.

/// The primary output resource of a recipe — the argmax of its outputs. Tags a
/// production method with a resource pip. (construction_panel.cpp keeps its own
/// copy for the Buildings-tab combo; both are three lines over the same array.)
resource_type primary_output_resource(const recipe& r)
{
    std::size_t best = 0;
    float best_v = -1.0f;
    for (std::size_t i = 0; i < resource_count; ++i)
        if (r.outputs[i] > best_v) { best_v = r.outputs[i]; best = i; }
    return static_cast<resource_type>(best);
}

/// Whether this building type produces anything on the economy tick. Ports,
/// logistics hubs and launchpads are passive infrastructure — economy_system runs
/// extraction and processing only, so the rest have no output, no recipe and no
/// report row. Showing them an output figure would invent one (a port would read
/// "0.0 Iron Ore / tick" purely because iron ore is the default target).
bool produces_output(building_type t)
{
    return t == building_type::extraction_site ||
           t == building_type::processing_facility;
}

/// The building's own report row from the last economy step, or nullptr if it did
/// not appear (never ticked, or built since).
const building_report* report_row_for(const economy_report& report, entity_id id)
{
    for (const building_report& br : report.buildings)
        if (br.building == id)
            return &br;
    return nullptr;
}

/// Output the building would credit this tick at a 100 % workforce target with no
/// labour contention — the ceiling its realised output is read against.
///
/// Derived from the same constants the economy tick uses (economy_system.cpp
/// run_extraction / run_processing) rather than an independent guess, so the ratio
/// means something: below 1 is contention, depletion taper, or a missing input;
/// above 1 is a workforce target pushed past nominal.
float output_ceiling(const world& w, const recipe_registry& reg,
                     const building_component& b)
{
    const float base = reg.economics(b.type).base_rate;

    if (b.type == building_type::extraction_site)
    {
        const auto tit = w.tiles.find(b.tile);
        if (tit == w.tiles.end())
            return 0.0f;
        const tile_component& tc = tit->second;
        const float richness = tc.resource_deposit[static_cast<std::size_t>(b.target_resource)];
        return base * richness * b.workforce_assigned * (1.0f - tc.hazard_level);
    }

    if (b.type == building_type::processing_facility)
    {
        const recipe* rcp = reg.get_recipe(b.recipe);
        if (rcp == nullptr)
            return 0.0f;
        // Report output_quantity sums a processor's outputs, so the ceiling has to
        // be in the same units: batches at nominal x the units one batch yields.
        float per_batch = 0.0f;
        for (std::size_t i = 0; i < resource_count; ++i)
            per_batch += rcp->outputs[i];
        return base * b.workforce_assigned * per_batch;
    }

    return 0.0f; // ports / hubs are passive infrastructure — no L3 production
}

/// One-line verdict on what the building is doing, and why it isn't doing more.
/// Returns the colour to print it in via @p col.
const char* production_status(const building_component& b,
                              const building_report* row, ImU32& col)
{
    if (b.decommissioned) { col = palette::neutral;  return "Idled - producing nothing"; }
    if (row == nullptr)   { col = palette::neutral;  return "No data yet - run an economy tick"; }
    if (row->exhausted)   { col = palette::negative; return "Deposit exhausted"; }
    if (row->idle)        { col = palette::negative; return "Idle - unstaffed or misconfigured"; }
    if (row->has_limiting)
    {
        col = palette::negative;
        return "Input-limited";
    }
    col = palette::positive;
    return "Active";
}

void draw_building_selection(world& w, const recipe_registry& reg,
                             const economy_report& report, ui_state& ui)
{
    const entity_id sel = ui.selected_entity;
    const auto bit = w.buildings.find(sel);
    if (bit == w.buildings.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    building_component& b = bit->second;

    const ImGuiStyle& style     = ImGui::GetStyle();
    ImDrawList*       dl        = ImGui::GetWindowDrawList();
    const float       content_w = ImGui::GetContentRegionAvail().x;
    const float       frame_h   = ImGui::GetFrameHeight();

    // ── Under construction: nothing to operate yet, so the build status IS the
    //    panel. Showing dials for a building that does not exist yet is noise. ──
    if (b.ticks_remaining > 0)
    {
        const float rate = construction_rate(w, reg, b.type, b.tile);
        ImGui::TextColored(construction_status_colour(rate), "%s",
                           construction_status(rate, b.ticks_remaining).c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("Controls unlock when construction completes.");
        return;
    }

    const building_report* row = report_row_for(report, sel);

    // ── (1) Output & rate — the lead. Passive infrastructure has no output to
    //    lead with, so it says what it is instead of printing a hollow zero. ──
    if (!produces_output(b.type))
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::neutral),
                           "Infrastructure - produces nothing directly");
        ImGui::TextDisabled("It earns through what it enables, not what it makes.");
    }
    else
    {
        // What it makes: the extraction target, or the recipe's primary output.
        resource_type made = b.target_resource;
        if (b.type == building_type::processing_facility)
            if (const recipe* rcp = reg.get_recipe(b.recipe); rcp != nullptr)
                made = primary_output_resource(*rcp);

        const float produced = (row != nullptr) ? row->output_quantity : 0.0f;
        const float ceiling  = output_ceiling(w, reg, b);

        // Big figure + resource pip, the one number worth reading at a glance.
        const ImVec2 p   = ImGui::GetCursorScreenPos();
        const float  pip = frame_h * 0.34f;
        icons::resource(dl, {p.x + pip, p.y + frame_h * 0.5f}, pip, made);
        ImGui::Dummy({pip * 2.0f + 6.0f, frame_h});
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                           "%.1f %s / tick", produced, presentation_of(made).name);

        // Rate against the uncontended nominal ceiling, as a bar plus its percentage.
        if (ceiling > 0.0f)
        {
            const float frac = produced / ceiling;
            const ImVec2 bp  = ImGui::GetCursorScreenPos();
            const float  bh  = frame_h * 0.42f;
            const float  bw  = content_w;
            dl->AddRectFilled(bp, {bp.x + bw, bp.y + bh}, IM_COL32(52, 52, 58, 255), 2.0f);
            // A target above 100 % can push past nominal, so the bar clamps at full
            // while the printed percentage keeps telling the truth.
            const float fill_frac = frac > 1.0f ? 1.0f : frac;
            const ImU32 bar_col = (frac >= 0.98f) ? palette::positive
                                : (frac >= 0.50f) ? palette::neutral
                                                  : palette::negative;
            dl->AddRectFilled(bp, {bp.x + bw * fill_frac, bp.y + bh}, bar_col, 2.0f);
            ImGui::Dummy({bw, bh});
            ImGui::TextDisabled("%.0f%% of nominal (%.1f)", frac * 100.0f, ceiling);
        }

        ImU32 scol = palette::neutral;
        const char* status = production_status(b, row, scol);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(scol), "%s", status);
        // Name the binding input rather than just flagging that one exists — the
        // whole point of the line is telling the player what to go fix.
        if (row != nullptr && row->has_limiting && !b.decommissioned)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", presentation_of(row->limiting_input).name);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // ── (2) Profitability — the consequence. ──
    draw_building_profit(w, reg, report, sel);

    ImGui::Spacing();
    ImGui::Separator();

    // ── (3) Production method — the first lever on profitability. Nothing to
    //    choose on a building that produces nothing, so the section is absent
    //    rather than present-and-empty. ──
    if (produces_output(b.type))
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Production method");
        const int   n_recipes = reg.recipe_count(b.type);
        const float pipr      = ImGui::GetFontSize() * 0.42f;
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
            icons::resource(dl, {gp.x + pipr, gp.y + frame_h * 0.5f}, pipr,
                            method_res(b.active_recipe_index));
            ImGui::Dummy({pipr * 2.0f + 6.0f, frame_h});
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::BeginCombo("##sel_method", preview))
            {
                ImDrawList* pdl = ImGui::GetWindowDrawList(); // the popup's own list
                for (int i = 0; i < n_recipes; ++i)
                {
                    const recipe& ri  = reg.recipe_at(b.type, i);
                    const bool    on  = (i == b.active_recipe_index);
                    const ImVec2  ip  = ImGui::GetCursorScreenPos();
                    const std::string lbl = std::string("     ") + (ri.name.empty() ? "-" : ri.name);
                    if (ImGui::Selectable(lbl.c_str(), on))
                    {
                        b.active_recipe_index = i;
                        b.recipe = reg.recipe_id(ri.name);
                    }
                    icons::resource(pdl, {ip.x + pipr + 2.0f,
                                          ip.y + ImGui::GetTextLineHeight() * 0.5f},
                                    pipr, method_res(i));
                    if (on)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        else
            ImGui::TextDisabled("Single method - nothing to switch");

        ImGui::Spacing();
    }

    // ── (4) Workforce — the second lever. Auto (BL-181) owns the dial unless the
    //    player takes it; moving the slider is itself the act of taking it. ──
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Workforce");
    {
        bool  autos = b.workforce_auto;
        if (ImGui::Checkbox("Auto##sel_wf_auto", &autos))
            b.workforce_auto = autos;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Solve the target each tick for maximum profit.");

        ImGui::SameLine();
        ImGui::BeginDisabled(b.workforce_auto);
        int target = b.workforce_target;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::SliderInt("##sel_wf", &target, 0, 200, "%d%% of nominal"))
        {
            b.workforce_target = target;
            b.workforce_auto   = false; // a manual move pins the dial
        }
        ImGui::EndDisabled();

        // ── BL-179: the habitability→workforce reason, where the player decides.
        //    The slider says what was ASKED for; this says what the body actually
        //    ALLOWS, and why. The chain is target → assigned → x contention →
        //    effective, and contention is set by the corp's labour demand against
        //    a pool that habitability sizes (POPULATION.md § Workforce model).
        //    Phrasing comes from the shared ui::fmt::labour_contention helper, so
        //    this and the Economy panel's staffing table are the same ceiling in
        //    the same words - the "one consistent story" BL-179 requires.
        const auto tile_it = w.tiles.find(b.tile);
        if (tile_it != w.tiles.end())
        {
            const entity_id body = tile_it->second.body;
            const entity_id corp = owner_corp_of(w, sel);
            const auto      cit  = report.workforce_contention.find({corp, body});
            const float     scal = (cit != report.workforce_contention.end()) ? cit->second : 1.0f;
            const std::string shortfall = fmt::labour_contention(scal);

            if (!shortfall.empty())
            {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                                   "Body allows %s", shortfall.c_str());
                const auto hit = report.body_habitability.find(body);
                if (hit != report.body_habitability.end())
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("- habitability %.2f", hit->second);
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip(
                        "Your labour demand on this body exceeds the workforce pool,\n"
                        "so every building you own here is throttled by the same\n"
                        "fraction. The pool is sized by population and habitability -\n"
                        "raising the target asks for labour that is not there.");
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    // ── (5) How many more of this can I build here (Ben's question C). Buildings
    //    stack on a tile up to a ceiling the deposit sets — see
    //    placement_rules::stack_capacity, which the placement gate reads too, so
    //    this readout and what construction actually permits cannot drift. ──
    {
        const auto tit = w.tiles.find(b.tile);
        if (tit != w.tiles.end())
        {
            const int cap  = placement_rules::stack_capacity(tit->second, b.type, b.target_resource);
            const int here = placement_rules::buildings_on_tile(w, b.tile, b.type, b.target_resource);
            const int more = (cap > here) ? cap - here : 0;

            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "On this tile");
            ImGui::Text("%d of %d", here, cap);
            ImGui::SameLine();
            if (more > 0)
                ImGui::TextDisabled("- room for %d more", more);
            else
                ImGui::TextDisabled("- tile is at capacity");

            ImGui::BeginDisabled(more <= 0);
            if (ImGui::Button("Build another here", {content_w, frame_h * 1.4f}))
            {
                ui.construction.pending_tile   = b.tile;
                ui.construction.pending_type   = b.type;
                ui.construction.pending_target = b.target_resource;
            }
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("The stack ceiling is provisional - the rule for how "
                                  "richness sets it is not settled yet.");
        }
    }

    ImGui::Spacing();

    // ── (6) Stop / remove. Two different acts, kept visibly different: idling is
    //    reversible and keeps the asset (wages stop, material upkeep continues);
    //    demolition frees the tile and refunds nothing. Demolish therefore asks. ──
    {
        const float bw = (content_w - style.ItemSpacing.x) * 0.5f;

        if (b.decommissioned)
        {
            if (ImGui::Button("Resume##sel_idle", {bw, frame_h * 1.4f}))
                b.decommissioned = false;
        }
        else
        {
            if (ImGui::Button("Idle##sel_idle", {bw, frame_h * 1.4f}))
                b.decommissioned = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(b.decommissioned
                ? "Put it back to work."
                : "Stop production and wages. Keeps the building and its tile.");

        ImGui::SameLine();
        if (ImGui::Button("Demolish##sel_demolish", {bw, frame_h * 1.4f}))
            ImGui::OpenPopup("##sel_demolish_confirm");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Remove it and free the tile. No refund.");

        if (ImGui::BeginPopup("##sel_demolish_confirm"))
        {
            ImGui::TextUnformatted("Demolish this building?");
            ImGui::TextDisabled("The tile is freed. The build cost is not returned.");
            ImGui::Spacing();
            if (ImGui::Button("Demolish", {110.0f, 0.0f}))
            {
                // Deferred: app executes it against the mutable world after the
                // draw completes (erasing here would invalidate this iteration).
                ui.construction.pending_demolish = sel;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", {110.0f, 0.0f}))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
}

} // namespace

void draw_selection_content(world& w, const recipe_registry& reg,
                            const economy_report& report, ui_state& ui)
{
    const selection_kind kind = selection_kind_of(w, ui.selected_entity);

    // Nothing valid selected — draw nothing. (The card frame owns the open/hidden
    // gate on selection_hidden_for; this content function only draws the entity.)
    if (kind == selection_kind::none)
        return;

    // A selected tile owns its WHOLE card layout, header included (Ben's 2026-07-23
    // three-region design: header / zoomed hex neighbourhood + action strip / graphs),
    // so it is dispatched before the generic header the other kinds share.
    if (kind == selection_kind::tile)
    {
        draw_tile_selection(w, ui);
        return;
    }

    // Frame-agnostic: this draws into whatever window the caller opened (the sticky
    // card, BL-195). Width comes from the live content region, not a fixed column —
    // so the same content renders in a card of any width. The header + per-kind
    // dispatch below is unchanged from the former fold-out Selection panel.
    const float       bar_w   = ImGui::GetContentRegionAvail().x;
    const float       frame_h = ImGui::GetFrameHeight();
    const ImGuiStyle& style   = ImGui::GetStyle();
    ImDrawList*       dl      = ImGui::GetWindowDrawList();

    // ── Header: [icon] Name · type ............................. [>] [x] ──
    {
        const ImVec2 hc = ImGui::GetCursorScreenPos();
        const float  ir = frame_h * 0.40f;
        draw_selection_icon(w, dl, kind, ui.selected_entity,
                            {hc.x + ir, hc.y + frame_h * 0.5f}, ir);
        ImGui::SetCursorScreenPos({hc.x + ir * 2.0f + style.ItemSpacing.x, hc.y});

        const char* title = selection_title(w, kind, ui.selected_entity);
        const char* kname = selection_kind_name(kind);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", title);
        // Suppress the redundant type label when the title is already the kind name
        // (e.g. a tile titles as "Tile"), avoiding "Tile · Tile".
        if (std::strcmp(title, kname) != 0)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", kname);
        }

        // Right-align the two chrome buttons at the content region's right edge.
        // bar_w is the available content width (padding already excluded), so the
        // offset is measured from the content-left, no WindowPadding term needed.
        const float btn = frame_h;
        ImGui::SameLine(bar_w - 2.0f * btn - style.ItemSpacing.x);
        if (ImGui::Button(">", {btn, btn}))
            focus_on_entity(w, ui, ui.selected_entity);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Go to");
        ImGui::SameLine();
        if (ImGui::Button("x", {btn, btn}))
            ui.selection_hidden_for = ui.selected_entity;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Close");
    }

    ImGui::Separator();

    // A selected player building takes its own vertical layout (Ben's 2026-07-22
    // review: the old four-numbers card "is useless"). Rival buildings stay on the
    // action|facts split — intel only, nothing to operate.
    if (kind == selection_kind::building && is_player_owned(w, ui.selected_entity))
    {
        draw_building_selection(w, reg, report, ui);
        return;
    }

    // ── Action (left, dominant) │ Facts (right, muted) ──
    const float content_w = bar_w;
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
