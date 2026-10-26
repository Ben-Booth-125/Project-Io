#include "construction_panel.hpp"

#include <algorithm>
#include <array>   // the per-tick need row the sim's mirror builds (BL-1342)
#include <cmath>   // std::ceil (construction ETA, BL-095)
#include <string>  // status strings (BL-095)

#include "charts.hpp"          // draw_value_bar - the queue row's progress column (BL-1365)
#include "construction_sites.hpp" // the site finder: the building-first lens and ledger (Ben, 2026-10-25)
#include "foldout_column.hpp"  // shell fold-out column host (BL-122) + nav_button (the tab strip)
#include "format.hpp"          // fmt::signed_delta / sign_of - the roster's profit column
#include "hover_card.hpp"      // draw_hover_card - the SHARED hover frame (BL-1365)
#include "icons.hpp"           // the glyph vocabulary the one-line rows read in (BL-1365)
#include "text_fit.hpp"        // fit_text - the name column's elision
#include "presentation.hpp"    // building_type_name / extraction_building_name — the shared roster vocabulary
#include "selection_panel.hpp" // the build bar and the two relocated levers
#include "ui_state.hpp"

#include "world/budget_system.hpp"     // building_opex / compute_building_opex, body_mean_habitability (BL-143)
#include "world/building_profit.hpp"   // estimate_building_profit — the per-building figure the roster sums
#include "world/economy_system.hpp"    // economy_report (BL-143 status column)
#include "world/construction.hpp"      // construction_capex / construction_build_ticks (the progress denominator, BL-1343)
#include "world/market_clearing.hpp"   // market_for_tile (build-rate read, BL-095)
#include "world/placement_rules.hpp"   // is_well_site - the Well's roster name (BL-1198)
#include "world/recipe_registry.hpp"

#include <cstdio>
#include <map>
#include <imgui.h>

namespace ui {

// --- THE construction mirror (BL-095 task E; corrected + unified by BL-1342) --
//
// `construction.md` § "Construction is durative and material-gated" is the
// contract: these "mirror run_construction's formula so the panel and the
// Selection front door show the same number". Read it against
// `run_construction` / `site_rate` in src/world/economy_system.cpp — the two
// formulas are meant to be read side by side, which is why this one is here
// ONCE and selection_panel.cpp now calls it rather than keeping its own copy.
//
// The four ways the old mirrors lied (Ben, playtest 2026-10-16: "ETA 2 qtr, but
// no progress for multiple quarters"):
//   1. they read `m->supply[r]`, the per-tick FLOW the clear resets, where the
//      sim draws `m->inventory[r]`, the persistent STOCK;
//   2. they left `construction().capacity_per_build_tick` out of the need row,
//      so a capacity-starved build read as on schedule;
//   3. they ignored `shelf_admits` — BL-1172's fair-price ceiling — so a good
//      priced over `reservation_mult x base` read as available when the sim
//      sees nothing there at all;
//   4. they knew nothing of `state_idle`, so an unfunded state site read as
//      building.
// What is still NOT mirrored: BL-1209's pro-rata rationing of a contended shelf
// (see the header's note).
construction_read read_construction(const world& w, const recipe_registry& reg,
                                    building_type type, resource_type target,
                                    std::uint16_t recipe, entity_id tile,
                                    bool state_idle, int days_left)
{
    construction_read out;
    // Ben, 2026-10-24: a site in its last quarter needs that share of a quarter's row.
    const float qf = construction_quarter_fraction(days_left);

    const building_economics& econ = reg.economics(type);
    const float duration = econ.build_duration_ticks;
    if (duration <= 0.0f)
        return out; // instant build — never material-gated (rate 1)

    if (state_idle) // BL-1296: it draws nothing and advances not at all this tick
    {
        out.rate   = 0.0f;
        out.paused = true;
        out.waiting_on_funding = true;
        return out;
    }

    // THE NEED ROW, exactly `need_row_for`: the material cost of THIS named
    // building (BL-590) spread over its duration, plus a FLAT per-tick draw of
    // construction capacity (BL-709 — not divided by the duration, because a
    // longer build occupies the sector for longer).
    const auto& material_cost_row = reg.resource_build_cost_for(type, target, recipe);
    const std::size_t cap_index = static_cast<std::size_t>(resource_type::construction_capacity);
    const float capacity_rate   = reg.construction().capacity_per_build_tick;
    std::array<float, resource_count> need{};
    for (std::size_t r = 0; r < resource_count; ++r)
        need[r] = material_cost_row[r] / duration * qf;
    if (capacity_rate > 0.0f)
        need[cap_index] += capacity_rate * qf;

    const market_component* m = nullptr;
    const entity_id mid = market_for_tile(w, tile);
    if (mid != null_entity)
    {
        const auto mit = w.markets.find(mid);
        if (mit != w.markets.end())
            m = &mit->second;
    }
    out.no_market = (m == nullptr);

    // `admitted` + the inventory read, exactly as the sim's own lambdas: a good
    // over the fair-price ceiling reads 0, and a site with no market can draw
    // nothing at all (the sim's `admitted` is false when `m` is null).
    const float res_mult = reg.price_band().reservation_mult;
    const auto admitted  = [&](std::size_t r) {
        return m != nullptr && shelf_admits(*m, r, res_mult, /*off_buys=*/true);
    };
    const auto avail_of = [&](std::size_t r) {
        return admitted(r) ? std::max(0.0f, m->inventory[r]) : 0.0f;
    };

    const float max_stretch = reg.construction().max_stretch;
    const float pause_below = (max_stretch > 1.0f) ? (1.0f / max_stretch) : 0.0f;

    // `site_rate`: the scarcest MATERIAL sets the rate and can pause the build;
    // CAPACITY only stretches it, its coverage floored at `pause_below`.
    float rate     = 1.0f;
    float worst    = 1.0f; // the lowest coverage seen, for the binding good
    bool  have_bad = false;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (r == cap_index || !(need[r] > 0.0f))
            continue;
        const float cov = avail_of(r) / need[r];
        rate = std::min(rate, cov);
        if (cov < worst || !have_bad)
        {
            if (cov < 1.0f)
            {
                worst                 = cov;
                have_bad              = true;
                out.has_binding       = true;
                out.binding           = static_cast<resource_type>(r);
                // Priced out rather than short: the shelf HAS it, the ceiling refuses it.
                out.binding_priced_out = (m != nullptr && !admitted(r) && m->inventory[r] > 0.0f);
            }
        }
    }
    if (capacity_rate > 0.0f)
    {
        const float cov = (need[cap_index] > 0.0f) ? (avail_of(cap_index) / need[cap_index]) : 1.0f;
        const float eff = std::max(cov, pause_below);
        if (eff < rate)
        {
            rate                   = eff;
            out.has_binding        = true;
            out.binding            = resource_type::construction_capacity;
            out.binding_priced_out = (m != nullptr && !admitted(cap_index)
                                      && m->inventory[cap_index] > 0.0f);
        }
    }
    rate = std::clamp(rate, 0.0f, 1.0f);
    if (rate < pause_below)
        rate = 0.0f; // paused: even the max-stretched rate can't be supplied

    out.rate   = rate;
    out.paused = (rate <= 0.0f);
    return out;
}

float construction_rate(const world& w, const recipe_registry& reg,
                        building_type type, resource_type target, std::uint16_t recipe,
                        entity_id tile)
{
    return read_construction(w, reg, type, target, recipe, tile).rate;
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

// Construction is measured in DAYS (Ben, 2026-10-24; PRODUCTION.md § Construction is
// measured in days): a building finishes between quarters, so its readout counts days.
std::string days_label(int days)
{
    return std::to_string(days) + (days == 1 ? " day" : " days");
}

int construction_eta_days(const construction_read& rd, int days_left)
{
    if (rd.rate <= 0.0f)
        return -1; // a paused build has no honest ETA
    if (rd.rate >= 1.0f)
        return days_left;
    return static_cast<int>(std::ceil(static_cast<float>(days_left) / rd.rate));
}

std::string construction_blocker_phrase(const construction_read& rd)
{
    if (rd.waiting_on_funding)
        return "unfunded";
    if (!rd.has_binding)
        return rd.no_market ? "no market" : std::string();
    const std::string good = resource_name(rd.binding);
    return rd.binding_priced_out ? good + " priced out" : "no " + good;
}

// The stall's reason clause, authored ONCE (BL-1365). `construction_status`
// below says "Paused - <clause>" about a site; the build door's hover card says
// "Will stall - <clause>" about a candidate. Two tenses, one set of words.
std::string construction_stall_reason(const construction_read& rd)
{
    const std::string good = rd.has_binding ? resource_name(rd.binding) : std::string();
    if (rd.waiting_on_funding)
        return "its treasury could not fund it this quarter";
    if (rd.has_binding && rd.binding_priced_out)
        return good + " is priced above the build ceiling";
    if (rd.has_binding)
        return "no " + good + " at the market";
    if (rd.no_market)
        return "no market supplies this tile";
    return "market can't supply materials";
}

// The status line — a stall NAMES the good holding it (BL-1342), because "Paused"
// alone tells a player nothing they can act on.
std::string construction_status(const construction_read& rd, int days_left)
{
    const std::string good = rd.has_binding ? resource_name(rd.binding) : std::string();
    if (rd.waiting_on_funding || rd.paused)
        return "Paused - " + construction_stall_reason(rd);
    const int eta = construction_eta_days(rd, days_left);
    if (rd.rate >= 1.0f)
        return "Building... ~" + days_label(eta);
    if (rd.has_binding)
        return "Building... ~" + days_label(eta)
             + (rd.binding_priced_out ? " (" + good + " priced out)" : " (short of " + good + ")");
    return "Building... ~" + days_label(eta) + " (materials scarce)";
}

// Colour for an analog build status: red paused, amber scarce, green on-schedule.
ImVec4 construction_rate_colour(float rate)
{
    return (rate <= 0.0f) ? ImVec4{0.90f, 0.55f, 0.55f, 1.0f}   // paused
         : (rate < 1.0f)  ? ImVec4{0.85f, 0.75f, 0.45f, 1.0f}   // materials scarce
                          : ImVec4{0.55f, 0.80f, 0.55f, 1.0f};  // on schedule
}

ImVec4 construction_status_colour(const construction_read& rd)
{
    return construction_rate_colour(rd.rate);
}

// --- The one-line GRID row and its hover card (BL-1365) ----------------------
// construction.md § "One line a row, and the words live in the hover" / § "The
// line is a grid, not a sentence". One metric and one card frame, shared by all
// four construction surfaces, so "the same treatment" is literally the same code
// rather than four surfaces that merely resemble each other.

float construction_row_height()
{
    // TWO lines, since BL-1371 (construction.md § "A row is two lines tall, and
    // text wraps rather than truncating"; Ben, 2026-10-17: *"we are going to have
    // to use two row spaces for each building, this is so that text can wrap to
    // the second row. Many rows truncate the text"*). The columns and their widths
    // are unchanged — what the second line buys is the stretch NAME column, which
    // resolves to ~70 px at the fold-out's narrowest and was eliding most names.
    //
    // A row that needs only one line still occupies two, so the grid stays a
    // grid: this is a BUDGET, not a measurement of the text in hand.
    return ImGui::GetTextLineHeight() * static_cast<float>(construction_grid::lines)
         + ImGui::GetStyle().CellPadding.y * 2.0f;
}

int construction_grid_text(const char* site, const char* text, float max_w, bool disabled,
                           bool* elided)
{
    return fit_wrapped_text(text_box::table_cell, site, text, max_w,
                            construction_grid::lines, disabled, elided);
}

void construction_grid_bar(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float frac, ImU32 colour)
{
    dl->AddRectFilled(mn, mx, IM_COL32(48, 50, 58, 255), 2.0f); // track
    const float w    = mx.x - mn.x;
    const float fill = std::min(std::max(w * std::clamp(frac, 0.0f, 1.0f), 2.0f), std::max(w, 0.0f));
    if (w > 0.0f)
        dl->AddRectFilled(mn, {mn.x + fill, mx.y}, colour, 2.0f);
}

bool construction_row_hovered(ImVec2 mn, ImVec2 mx)
{
    return ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)
        && ImGui::IsMouseHoveringRect(mn, mx, /*clip=*/false);
}

void construction_row_card(const char* title, const std::function<void()>& content)
{
    // The SHARED primitive (TOOLTIP.md § The frame), not a bespoke tooltip: one
    // frame, one max width, one position rule, so a ledger row's read looks like
    // every other read in the game. `NoInputs` keeps it out of the hit-test, so
    // the card cannot cover the Build press it is describing.
    draw_hover_card(ImGui::GetMousePos(), [&]() {
        if (title != nullptr && title[0] != '\0')
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", title);
        content();
    });
}

// --- Per-column explainers (BL-1368) ----------------------------------------
// construction.md § "Each element explains itself, and only itself". One cell,
// one subject, one or two sentences. The hit-test is a rect per cell so the Build
// press keeps its click (the same reason the row hit-test was a rect).

grid_explainer construction_cell(const char* key, ImVec2 cursor, float w, std::string text,
                                 ImU32 ink)
{
    const float py = ImGui::GetStyle().CellPadding.y;
    grid_explainer e;
    e.key  = key;
    // The cell is the WHOLE row's height (BL-1371: two text lines), not one
    // line of it — a column's explainer answers for the column, so pointing at
    // the blank half of a one-line row must still reach it.
    e.mn   = {cursor.x - construction_grid::pad, cursor.y - py};
    e.mx   = {cursor.x + w + construction_grid::pad,
              cursor.y - py + construction_row_height()};
    e.text = std::move(text);
    e.ink  = ink;
    return e;
}

int construction_column_card(const char* title, const std::vector<grid_explainer>& cells)
{
    for (std::size_t i = 0; i < cells.size(); ++i)
    {
        const grid_explainer& c = cells[i];
        if (c.text.empty() || !construction_row_hovered(c.mn, c.mx))
            continue;
        construction_row_card(title, [&]() {
            ImGui::PushTextWrapPos(0.0f);
            if (c.ink != 0)
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(c.ink), "%s", c.text.c_str());
            else
                ImGui::TextDisabled("%s", c.text.c_str());
            ImGui::PopTextWrapPos();
        });
        return static_cast<int>(i);
    }
    return -1;
}

std::string construction_cells_text(const std::vector<grid_explainer>& cells)
{
    std::string out;
    for (const grid_explainer& c : cells)
    {
        if (c.text.empty())
            continue;
        if (!out.empty())
            out += '\n';
        out += c.text;
    }
    return out;
}

bool construction_begin_build_grid(const char* id)
{
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg
                                    | ImGuiTableFlags_NoPadInnerX
                                    | ImGuiTableFlags_SizingFixedFit;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                        {construction_grid::pad, ImGui::GetStyle().CellPadding.y});
    if (!ImGui::BeginTable(id, 8, flags))
    {
        ImGui::PopStyleVar();
        return false;
    }
    ImGui::TableSetupColumn("##glyph", ImGuiTableColumnFlags_WidthFixed, construction_grid::glyph);
    ImGui::TableSetupColumn("##name",  ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("##cost",  ImGuiTableColumnFlags_WidthFixed, construction_grid::cost);
    ImGui::TableSetupColumn("##makes", ImGuiTableColumnFlags_WidthFixed, construction_grid::makes);
    ImGui::TableSetupColumn("##stack", ImGuiTableColumnFlags_WidthFixed, construction_grid::stack);
    ImGui::TableSetupColumn("##state", ImGuiTableColumnFlags_WidthFixed, construction_grid::state);
    ImGui::TableSetupColumn("##bar",   ImGuiTableColumnFlags_WidthFixed, construction_grid::bar);
    ImGui::TableSetupColumn("##act",   ImGuiTableColumnFlags_WidthFixed, construction_grid::act);
    return true;
}

void construction_end_grid()
{
    ImGui::EndTable();
    ImGui::PopStyleVar();
}

float construction_progress_fraction(const world& w, const recipe_registry& reg,
                                     const building_component& b)
{
    // The DAYS ratio (Ben, 2026-10-24), live denominator and all. The flat cost is
    // paid whole at placement and the materials in arrears, so the credit fraction
    // reads the flat share on day one: it is no measure of progress.
    (void)w;
    const int total = construction_build_days(w, reg, b.tile, b.type, b.target_resource);
    if (total <= 0)
        return 0.0f;
    const float done = static_cast<float>(total - construction_days_left(b));
    return std::clamp(done / static_cast<float>(total), 0.0f, 1.0f);
}

namespace {

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
        const building_opex opex = compute_building_opex(b, reg.economics(b.type), 1.0f, mean_hab,
                                                         reg.idle_maintenance_floor());
        total += opex.maintenance + opex.wages;
    }
    return total;
}

// --- Queue overview table ----------------------------------------------------
// WHAT IS IN FLIGHT, read from the buildings themselves (BL-1343). Ben, playtest
// 2026-10-16: "the construction queue is showing 'No active construction', when
// there are multiple buildings being constructed". It did, because the table was
// dead behind a hard-coded `any_items = false` while it waited on a
// `w.construction_queue` that does not exist and is not needed: a building with
// `ticks_remaining > 0` IS the queue item, and `estimated_quarterly_construction_cost`
// above was already walking exactly that set to price the header.
//
// One row per player building under construction, NEAREST DONE FIRST (progress
// descending, entity id breaking ties so an unordered `world::buildings` cannot
// vary what a player or a check sees) — "what lands next" is what a player asks a
// queue. The Progress cell keeps the authored colour bands: dim (<25 %) /
// mid (25-75 %) / high (>75 %).
//
// FOUR columns, not the five the dead header declared: the fold-out column is
// ~380 px and Body never fitted beside the name, the progress, the ETA and the
// cost. The body and the tile ride the row's hover instead, with the stall reason.
void draw_queue_section(const world& w, const recipe_registry& reg, ui_state& ui)
{
    // COLLAPSIBLE, and collapsed by default: the queue is empty most of the time,
    // and an empty queue that owns the surface is the empty room BL-176 diagnosed.
    // Closed it costs one line, and the build bar below it is what the player meets.
    // A CollapsingHeader is a toggle by construction, so the standing Toggle rule is
    // satisfied without a second control. Its open state is held in ui_state rather
    // than ImGui's own storage so it is stable and drivable by a check; the label
    // carries a `###` id so the live cost figure cannot churn the widget's identity.
    ImGui::SetNextItemOpen(ui.construction.queue_open, ImGuiCond_Always);

    const float est_cost = estimated_quarterly_construction_cost(w, reg);
    char hdr[96];
    std::snprintf(hdr, sizeof hdr, "Queue - %.0f cr / qtr###queue_section",
                  static_cast<double>(est_cost));
    const bool open = ImGui::CollapsingHeader(hdr);
    if (open != ui.construction.queue_open)
        ui.construction.queue_open = open;
    ui.construction_ui.queue_rows.clear(); // published per frame; a shut queue has none
    ui.construction_ui.row_h  = construction_row_height();
    ui.construction_ui.line_h = ImGui::GetTextLineHeight();
    if (!open)
        return;

    struct queue_row
    {
        entity_id         id = null_entity;
        std::string       name;
        std::string       where;           ///< "Body [x, y]" — the hover.
        float             progress  = 0.0f; ///< construction_paid / capex.
        float             cost_left = 0.0f;
        construction_read rd;
        int               days_left = 0; ///< Days of work left (Ben, 2026-10-24).
        // BL-1365: the row reads in glyphs, so it needs what the glyph and the
        // pip key on as well as the words.
        building_type type  = building_type::none;
        resource_type ident = resource_type::iron_ore; ///< The glyph's identity (§ 1c).
        bool          makes = false;                   ///< Does it make a good at all?
        resource_type good  = resource_type::iron_ore; ///< That good — the pip.
    };
    std::vector<queue_row> rows;
    for (const auto& [id, b] : w.buildings)
    {
        if (b.ticks_remaining <= 0 || !is_player_owned(w, id))
            continue;
        queue_row row;
        row.id         = id;
        row.name       = building_group_name(w, reg, b);
        row.progress   = construction_progress_fraction(w, reg, b);
        row.days_left  = construction_days_left(b);
        row.rd         = read_construction(w, reg, b.type, b.target_resource, b.recipe, b.tile,
                                           b.state_idle, row.days_left);
        const float capex = construction_capex(w, reg, b.tile, b.type, b.target_resource, b.recipe);
        row.cost_left     = std::max(0.0f, capex - b.construction_paid);
        row.type          = b.type;
        if (b.type == building_type::extraction_site)
        {
            row.makes = true;
            row.good  = b.target_resource;
        }
        else if (b.type == building_type::processing_facility)
            if (const recipe* rc = reg.get_recipe(b.recipe); rc != nullptr)
            {
                row.makes = true;
                row.good  = primary_output_resource(*rc);
            }
        // ICONS.md § 1c: the named-building shape keys on what the site works or
        // makes, so the glyph matches the name the row carries.
        row.ident = row.makes ? row.good : b.target_resource;
        if (const auto tit = w.tiles.find(b.tile); tit != w.tiles.end())
        {
            const auto bod = w.bodies.find(tit->second.body);
            row.where = (bod != w.bodies.end() ? bod->second.name : std::string("off map"))
                      + " [" + std::to_string(tit->second.grid_x) + ", "
                      + std::to_string(tit->second.grid_y) + "]";
        }
        rows.push_back(std::move(row));
    }
    std::sort(rows.begin(), rows.end(), [](const queue_row& a, const queue_row& b) {
        if (a.progress != b.progress)
            return a.progress > b.progress; // nearest done first
        return a.id < b.id;                 // total order, so the walk is reproducible
    });

    if (rows.empty())
    {
        ImGui::TextDisabled("No active construction.");
        return;
    }

    // SIX FIXED COLUMNS (BL-1365), one line a row. The widths are
    // `construction_grid`'s, shared with the build list below, so the glyph, the
    // bar, the quarters, the cost and the state mark land in the same place on
    // both surfaces — the alignment IS the compaction (construction.md § The line
    // is a grid, not a sentence). `BordersInnerV` is gone: at these widths the
    // rules cost ~7 px of name column and the alignment already separates the
    // columns.
    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_NoPadInnerX |
        ImGuiTableFlags_SizingFixedFit;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,
                        {construction_grid::pad, ImGui::GetStyle().CellPadding.y});
    if (!ImGui::BeginTable("##construction_queue", 6, table_flags))
    {
        ImGui::PopStyleVar();
        return;
    }

    ImGui::TableSetupColumn("##glyph", ImGuiTableColumnFlags_WidthFixed, construction_grid::glyph);
    ImGui::TableSetupColumn("Building", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    // "Done", not "Progress": measured at 1720x1080 the column is ~46 px and
    // elided the longer header to "Progre...", which is a header that has stopped
    // naming its column. The cell is now a BAR carrying its own percentage, the
    // authored 25/75 % colour bands moving from the cell background onto the fill.
    ImGui::TableSetupColumn("Done",  ImGuiTableColumnFlags_WidthFixed, construction_grid::bar);
    ImGui::TableSetupColumn("Days",  ImGuiTableColumnFlags_WidthFixed, construction_grid::eta);
    ImGui::TableSetupColumn("Cost",  ImGuiTableColumnFlags_WidthFixed, construction_grid::cost);
    ImGui::TableSetupColumn("##state", ImGuiTableColumnFlags_WidthFixed, construction_grid::state);
    ImGui::TableHeadersRow();

    const float row_h = construction_row_height();
    ImDrawList* dl    = ImGui::GetWindowDrawList();
    for (const queue_row& row : rows)
    {
        ImGui::TableNextRow(ImGuiTableRowFlags_None, row_h);
        ImGui::TableSetColumnIndex(0);
        const float  line_h = ImGui::GetTextLineHeight();
        const ImVec2 gp     = ImGui::GetCursorScreenPos();
        icons::building(dl, {gp.x + construction_grid::glyph * 0.5f, gp.y + line_h * 0.5f},
                        construction_grid::glyph * 0.5f, row.type, row.ident,
                        IM_COL32(150, 235, 160, 255));
        ImGui::Dummy({construction_grid::glyph, line_h});

        // BL-1368: one explainer per COLUMN, collected as the row draws and
        // hit-tested after it — the row-wide card said all six at once.
        std::vector<grid_explainer> cells;

        ImGui::TableSetColumnIndex(1);
        const float name_w = std::max(16.0f, ImGui::GetContentRegionAvail().x);
        ImGui::PushID(static_cast<int>(row.id));
        // Pressing a row SELECTS the building — the same navigation the group
        // member rows below offer, and it decides nothing for the player.
        if (ImGui::Selectable("##qrow", ui.selected_entity == row.id,
                              ImGuiSelectableFlags_SpanAllColumns))
            ui.selected_entity = row.id;
        const bool hovered = ImGui::IsItemHovered();
        // Drawn over the Selectable rather than as its label, so the name can be
        // ELIDED to its column instead of pushing the columns right.
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, palette::selection);
        // BL-1371: the name WRAPS into the row's second line rather than eliding.
        bool      name_elided = false;
        const int name_lines  = construction_grid_text("construction.queue.name",
                                                       row.name.c_str(), name_w,
                                                       /*disabled=*/false, &name_elided);
        ImGui::PopStyleColor();
        ImGui::PopID();
        // The glyph and the name are one subject: WHICH site this is, where it
        // stands, and that the row is a press. The glyph's identity keys on what
        // the site will make (ICONS.md § 1c), so that belongs here too.
        cells.push_back(construction_cell(
            "name", gp, construction_grid::glyph + construction_grid::pad * 2.0f + name_w,
            (row.where.empty() ? std::string("Off the map") : "Stands at " + row.where) + ". "
                + (row.makes ? std::string("Will make ") + resource_name(row.good) + ". "
                             : std::string())
                + "Press the row to select it."));

        ImGui::TableSetColumnIndex(2);
        // The authored bands, now the bar's FILL colour: dim (<25 %) / mid
        // (25-75 %) / high (>75 %).
        const ImU32 band = (row.progress < 0.25f) ? IM_COL32(120, 120, 120, 255)
                         : (row.progress < 0.75f) ? IM_COL32(200, 180, 0, 255)
                                                  : IM_COL32(0, 180, 95, 255);
        {
            const ImVec2 bp = ImGui::GetCursorScreenPos();
            construction_grid_bar(dl, {bp.x, bp.y + 3.0f},
                                  {bp.x + construction_grid::bar, bp.y + line_h - 3.0f},
                                  row.progress, band);
            ImGui::Dummy({construction_grid::bar, line_h});
            char buf[200];
            std::snprintf(buf, sizeof buf,
                          "%.0f%% built - the days of work done against the build's whole "
                          "days.", static_cast<double>(row.progress * 100.0f));
            cells.push_back(construction_cell("bar", bp, construction_grid::bar, buf, band));
        }

        ImGui::TableSetColumnIndex(3);
        // The ETA the status line gives, not the raw tick count: a stretched build
        // takes longer than its ticks say, and a paused one has no honest ETA — so
        // a paused row prints an em dash and leaves the saying-why to the card.
        const ImVec2 ep  = ImGui::GetCursorScreenPos();
        const int    eta = construction_eta_days(row.rd, row.days_left);
        if (eta < 0)
            ImGui::TextColored(construction_status_colour(row.rd), "\xe2\x80\x94");
        else
            ImGui::TextColored(construction_status_colour(row.rd), "%d", eta);
        {
            // The days column explains the COUNT, and an em dash is a reading
            // too: a paused build has no honest ETA, which is why the cell is blank
            // rather than optimistic. WHY it is paused is the state mark's.
            char buf[220];
            if (eta < 0)
                std::snprintf(buf, sizeof buf,
                              "No honest estimate while it is paused, so the column shows a dash.");
            else
                std::snprintf(buf, sizeof buf,
                              "About %s left at the rate it is building now.",
                              days_label(eta).c_str());
            cells.push_back(construction_cell("eta", ep, construction_grid::eta, buf,
                                              ImGui::ColorConvertFloat4ToU32(
                                                  construction_status_colour(row.rd))));
        }

        ImGui::TableSetColumnIndex(4);
        const ImVec2 cp = ImGui::GetCursorScreenPos();
        ImGui::TextDisabled("%s", fmt::abbreviate(static_cast<double>(row.cost_left)).c_str());
        {
            char buf[200];
            std::snprintf(buf, sizeof buf,
                          "%.0f cr still to pay: the materials it has left to buy, at today's "
                          "prices. Its build cost was paid when it was placed.",
                          static_cast<double>(row.cost_left));
            cells.push_back(construction_cell("cost", cp, construction_grid::cost, buf));
        }

        ImGui::TableSetColumnIndex(5);
        // THE STATE MARK — `icons::value_mark`, the vocabulary's single filled dot
        // (ICONS.md § 1): quiet grey while the build is on schedule, amber while it
        // is stretching, red while it is stalled. Colour is the ordered channel
        // here, which is what the catalogue asks of an ordered axis.
        {
            const ImVec2 sp = ImGui::GetCursorScreenPos();
            const ImU32  c  = (row.rd.rate >= 1.0f)
                                 ? IM_COL32(110, 116, 128, 255)
                                 : ImGui::ColorConvertFloat4ToU32(construction_status_colour(row.rd));
            icons::value_mark(dl, {sp.x + construction_grid::state * 0.5f, sp.y + line_h * 0.5f},
                              3.0f, c);
            ImGui::Dummy({construction_grid::state, line_h});
            // THE MARK NAMES THE GOOD. `construction_status` is the single author
            // of that clause (through `construction_stall_reason`), so this
            // explainer and the status line on the site card cannot drift.
            cells.push_back(construction_cell(
                "state", sp, construction_grid::state, construction_status(row.rd, row.days_left),
                ImGui::ColorConvertFloat4ToU32(construction_status_colour(row.rd))));
        }

        // ONE CARD, FOR THE COLUMN UNDER THE POINTER (BL-1368). The six cells say
        // between them what the row-wide card said at once — where it stands, what
        // it makes, the figures the columns abbreviate, and the stall's named good
        // in the authored sentence — but one column at a time.
        (void)hovered; // the press is the Selectable's; the cards hit-test their cells
        construction_column_card(row.name.c_str(), cells);

        // The surface reporting its own rows: a check hovers each CELL's centre and
        // asserts that column's own explainer, and `hover` stays the union of them.
        construction_controls::grid_row pub;
        pub.name  = row.name;
        pub.hover = construction_cells_text(cells);
        pub.x     = gp.x + 24.0f;
        pub.y     = gp.y + line_h * 0.5f;
        pub.h           = row_h;
        pub.name_lines  = name_lines;
        pub.name_elided = name_elided;
        for (const grid_explainer& e : cells)
            pub.cols.push_back({e.key, e.text, (e.mn.x + e.mx.x) * 0.5f, (e.mn.y + e.mx.y) * 0.5f});
        ui.construction_ui.queue_rows.push_back(std::move(pub));
    }

    ImGui::EndTable();
    ImGui::PopStyleVar();
}


// --- The Buildings view: the player's estate, grouped by type -----------------

// One line per building inside an expanded group: its tile and its own net, with a
// press that SELECTS it. Returns true when pressed.
//
// Selecting from a row is the existing idiom, not a new one — the corporations table
// sets `selected_entity` from a row press in exactly this way. It is also the reason
// this view's empty state can be better than the Construction view's: listing the
// buildings behind a count the header has already asserted is NAVIGATION, and decides
// nothing. Ranking tiles the player could build on would have been the other thing.
bool draw_group_member_row(const world& w, entity_id id, float net, bool known,
                           bool selected, float name_w)
{
    const auto bit = w.buildings.find(id);
    if (bit == w.buildings.end())
        return false;

    char label[96];
    const auto tit = w.tiles.find(bit->second.tile);
    if (tit != w.tiles.end())
        std::snprintf(label, sizeof label, "[%d, %d]###m%u",
                      tit->second.grid_x, tit->second.grid_y, static_cast<unsigned>(id));
    else
        std::snprintf(label, sizeof label, "(off map)###m%u", static_cast<unsigned>(id));

    const bool pressed = ImGui::Selectable(label, selected, 0, {name_w, 0.0f});

    // The member's own net, right-aligned against the group total above it: the
    // column exists so the header's total is VISIBLY the sum of these, not a figure
    // the player has to take on trust.
    ImGui::SameLine();
    if (known)
    {
        const auto sg = fmt::sign_of(static_cast<double>(net));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(value_colour(sg)),
                           "%s", fmt::signed_delta(static_cast<double>(net)).c_str());
    }
    else if (bit->second.ticks_remaining > 0)
        ImGui::TextDisabled("building");
    else
        ImGui::TextDisabled("-");

    return pressed;
}

// The levers, drawn for whichever building is selected. THE SAME FUNCTION BODIES the
// Selection card's centre used to host (declared in selection_panel.hpp) — relocated,
// never reimplemented, because two sets of controls writing `workforce_target` and
// `try_switch_recipe` is how two surfaces come to disagree.
//
// The two selectors COMPOSE: this reads `ui.selected_entity`, so a building selected
// from a row here and a building selected on the map both land on the same levers.
void draw_building_levers(world& w, const recipe_registry& reg, ui_state& ui)
{
    const entity_id sel = ui.selected_entity;
    const auto bit = w.buildings.find(sel);
    if (bit == w.buildings.end() || !is_player_owned(w, sel))
        return;

    ui.construction_ui.levers_for = sel; // what was actually drawn for, not what was asked

    ImGui::Separator();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s",
                       building_group_name(w, reg, bit->second).c_str());
    if (const auto tit = w.tiles.find(bit->second.tile); tit != w.tiles.end())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("[%d, %d]", tit->second.grid_x, tit->second.grid_y);
    }

    if (bit->second.ticks_remaining > 0)
    {
        // Under construction: there is no method to switch and no workforce to set
        // yet. Saying so beats drawing dials that do nothing.
        ImGui::TextDisabled("Under construction.");
        return;
    }

    draw_production_method_section(w, reg, sel); // draws nothing for a non-processing type
    ImGui::Spacing();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Workforce");
    // BL-1322: read-only — the game sets it (the slider retired).
    (void)draw_workforce_bar(bit->second);
}

void draw_buildings_view(world& w, const recipe_registry& reg,
                         const economy_report& report, ui_state& ui)
{
    const std::vector<building_group> groups = player_building_groups(w, reg, report);
    if (groups.empty())
    {
        ImGui::TextDisabled("You own no buildings.");
        return;
    }

    // AT MOST ~9 rows visible, the rest reached by scrolling (Ben's "roughly 9
    // buildings with a vertical scroll"), and no more than there is to show — a
    // roster of four types should not reserve nine rows of empty column. Measured
    // off the live frame height rather than a pixel constant, so the row count
    // survives a font or display that is not the one this was written on.
    //
    // The levers sit BELOW the scroller. The scroller's height therefore depends on
    // the roster's row count, which changes when a group expands — but never on
    // which building is SELECTED, so the press that selects one cannot move the
    // thing the player was reaching for.
    int rows_shown = static_cast<int>(groups.size());
    for (const building_group& g : groups)
        if (ui.construction.buildings_expanded == g.name)
            rows_shown += static_cast<int>(g.members.size());

    const float row_h  = ImGui::GetFrameHeightWithSpacing();
    const float want_h = row_h * static_cast<float>(std::min(rows_shown, 9)) + 4.0f;
    const float roster_h = std::min(want_h,
                                    std::max(row_h * 3.0f,
                                             ImGui::GetContentRegionAvail().y * 0.60f));

    ImGui::BeginChild("##buildings_roster", {0.0f, roster_h}, false,
                      ImGuiWindowFlags_NoSavedSettings);

    // Width budget, measured off the host column rather than assumed (NR-709): the
    // net figure takes what it needs and the NAME takes the rest, so the name can
    // never be squeezed out by a fixed sibling.
    const float avail  = ImGui::GetContentRegionAvail().x;
    const float net_w  = ImGui::CalcTextSize("-000.0k").x + ImGui::GetStyle().ItemSpacing.x;
    const float name_w = std::max(40.0f, avail - net_w);

    bool published_group = false;
    for (const building_group& g : groups)
    {
        ImGui::PushID(g.name.c_str());

        const bool want_open = (ui.construction.buildings_expanded == g.name);
        ImGui::SetNextItemOpen(want_open, ImGuiCond_Always);

        // The count lives in the label; the TOTAL is drawn beside it so it can carry
        // its own sign colour. `###` keeps the widget id stable while the count moves.
        char hdr[128];
        std::snprintf(hdr, sizeof hdr, "%s  (%d)###grp", g.name.c_str(), g.count);

        const ImVec2 hp = ImGui::GetCursorScreenPos();
        const bool open = ImGui::CollapsingHeader(hdr);
        if (!published_group)
        {
            // First header's press point, published for a check to click the REAL
            // control (ui_state.hpp § construction_controls).
            ui.construction_ui.group_x = hp.x + ImGui::GetFrameHeight() * 0.5f;
            ui.construction_ui.group_y = hp.y + ImGui::GetFrameHeight() * 0.5f;
            published_group = true;
        }

        // One group open at a time — an accordion. Re-pressing the open header
        // closes it, which is the Toggle rule a header obeys by construction.
        if (open != want_open)
            ui.construction.buildings_expanded = open ? g.name : std::string{};

        ImGui::SameLine(name_w);
        {
            const auto sg = fmt::sign_of(static_cast<double>(g.total));
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(value_colour(sg)),
                               "%s", fmt::signed_delta(static_cast<double>(g.total)).c_str());
        }

        if (open)
        {
            ImGui::Indent();
            bool published_member = false;
            for (const entity_id id : g.members)
            {
                const building_profit p = estimate_building_reading(w, reg, report, id);
                const ImVec2 mp = ImGui::GetCursorScreenPos();
                const bool pressed = draw_group_member_row(
                    w, id, p.has_data ? p.net() : 0.0f, p.has_data,
                    ui.selected_entity == id,
                    std::max(40.0f, name_w - ImGui::GetStyle().IndentSpacing));
                if (!published_member)
                {
                    ui.construction_ui.member_x = mp.x + 20.0f;
                    ui.construction_ui.member_y = mp.y + ImGui::GetTextLineHeight() * 0.5f;
                    ui.construction_ui.member   = id;
                    published_member = true;
                }
                if (pressed)
                    ui.selected_entity = id;
            }
            ImGui::Unindent();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();

    draw_building_levers(w, reg, ui);
}

} // namespace

std::string building_group_name(const world& w, const recipe_registry& reg, const building_component& b)
{
    if (b.type == building_type::extraction_site)
    {
        // The Build door names this candidate "Farm" rather than by its resource, so
        // the roster does too — one vocabulary (presentation.hpp).
        // BL-1199: a coastal no-deposit produce site is a Fishing Wharf, by
        // the name the Build door offers it under.
        if (placement_rules::is_wharf_site(w, b.tile, b.target_resource))
            return "Fishing Wharf";
        if (b.target_resource == resource_type::agricultural_produce)
            return "Farm";
        // BL-1198: the Build door offers a Well by that name; the roster agrees.
        if (placement_rules::is_well_site(w, b.tile, b.target_resource))
            return "Well";
        return extraction_building_name(b.target_resource);
    }
    if (b.type == building_type::processing_facility)
    {
        // The recipe GROUP, not the recipe: "Metal Foundry" once, rather than a row
        // per method — BL-434's grouping, which is also the axis the Build door
        // collapses its processing candidates onto.
        if (const recipe* rc = reg.get_recipe(b.recipe); rc != nullptr && !rc->group.empty())
            return rc->group;
        return building_type_name(b.type);
    }
    return building_type_name(b.type);
}

std::vector<building_group> player_building_groups(const world& w,
                                                   const recipe_registry& reg,
                                                   const economy_report& report)
{
    // std::map, walked in name order — the source container (`world::buildings`) is
    // unordered, so an ordered accumulator is what makes the roster the same roster
    // on every platform and every run.
    std::map<std::string, building_group> by_name;
    for (const auto& [id, b] : w.buildings)
    {
        if (!is_player_owned(w, id))
            continue;
        const std::string name = building_group_name(w, reg, b);
        building_group& g = by_name[name];
        g.name = name;
        ++g.count;
        g.members.push_back(id);
        // A building the report has not priced contributes ZERO, never a guess: the
        // total must be the visible sum of the rows behind it.
        const building_profit p = estimate_building_reading(w, reg, report, id);
        if (p.has_data)
            g.total += p.net();
    }

    std::vector<building_group> out;
    out.reserve(by_name.size());
    for (auto& [key, g] : by_name)
    {
        std::sort(g.members.begin(), g.members.end(),
                  [&](entity_id a, entity_id b)
                  {
                      const building_profit pa = estimate_building_reading(w, reg, report, a);
                      const building_profit pb = estimate_building_reading(w, reg, report, b);
                      const float na = pa.has_data ? pa.net() : 0.0f;
                      const float nb = pb.has_data ? pb.net() : 0.0f;
                      if (na != nb)
                          return na > nb;
                      return a < b; // total order, so the walk is reproducible
                  });
        out.push_back(std::move(g));
    }
    return out;
}

// Two views, one construction element (Ben, 2026-08-29). Buildings is the default —
// the queue is empty most of the time and the player always owns buildings, so
// opening on the queue makes this ledger's front door an empty room (the fix BL-176
// made and the 2026-08-15 rework deleted along with the tab it lived on).
//
// The Construction view draws the collapsible queue and then the SAME build bar the
// tile Selection element's Construct button opens — that button opens this view
// rather than a second surface, so there is one build bar with two doors.
void draw_construction_panel(world& w,
                             const recipe_registry& reg,
                             const economy_report& report,
                             ui_state& ui,
                             bool* p_open)
{
    // Probe fields describe THIS frame; a frame that draws nothing publishes nothing,
    // so a stale position cannot be clicked after the surface is gone.
    ui.construction_ui.tab_x[0] = ui.construction_ui.tab_x[1] = -1.0f;
    ui.construction_ui.tab_y[0] = ui.construction_ui.tab_y[1] = -1.0f;
    ui.construction_ui.group_x  = ui.construction_ui.group_y  = -1.0f;
    ui.construction_ui.member_x = ui.construction_ui.member_y = -1.0f;
    ui.construction_ui.member   = null_entity;
    ui.construction_ui.levers_for = null_entity;
    ui.construction_ui.door_tile  = null_entity;
    ui.construction_ui.door_recipes.clear();
    ui.construction_ui.rail_track_x = ui.construction_ui.rail_track_y = -1.0f;   // BL-1255
    ui.construction_ui.rail_station_x = ui.construction_ui.rail_station_y = -1.0f;

    // The site finder's lens describes THIS frame: it is live only while the
    // Construction view draws it below, so a closed ledger or the other tab leaves
    // the map plain the same frame.
    ui.site_finder.live = false;
    ui.site_finder.drawn.clear();
    ui.site_finder.picker_x = ui.site_finder.picker_y = -1.0f;

    if (p_open && !*p_open)
    {
        // DISARM ON CLOSE (LENSES.md § The other direction: a ledger's lens disarms
        // on its close edge, whatever closed it). The chosen building goes with it,
        // so reopening the ledger does not re-skin the map unasked.
        ui.site_finder.armed = false;
        return;
    }

    // Re-hosted into the shell fold-out column (BL-122): pinned + borderless, no floating
    // spawn. The BL-082 height-cap that kept the old floating window clear of the
    // bottom-left Selection element is gone — the column sits entirely left of Selection.
    ui::foldout_begin("Construction");

    // The tab strip. `nav_button` carries the standing Toggle rule for us: re-clicking
    // the ACTIVE tab closes the whole ledger (it does not collapse to an overview),
    // and switching to the other tab is an ordinary view change.
    int& view = ui.construction.panel_view;
    {
        const float  h  = ImGui::GetFrameHeight();
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        ui::nav_button("Buildings", 1, view, p_open);
        ui.construction_ui.tab_x[1] = p0.x + ImGui::GetItemRectSize().x * 0.5f;
        ui.construction_ui.tab_y[1] = p0.y + h * 0.5f;
        ImGui::SameLine();
        const ImVec2 p1 = ImGui::GetCursorScreenPos();
        ui::nav_button("Construction", 0, view, p_open);
        ui.construction_ui.tab_x[0] = p1.x + ImGui::GetItemRectSize().x * 0.5f;
        ui.construction_ui.tab_y[0] = p1.y + h * 0.5f;
    }
    ImGui::Separator();

    // nav_button may have closed the ledger under us (the active-tab re-click). Draw
    // no body in that case — the column releases this frame, not the next.
    if (p_open == nullptr || *p_open)
    {
        if (view == 0)
        {
            draw_queue_section(w, reg, ui);
            ImGui::Spacing();
            ImGui::Separator();
            // BUILDING FIRST (Ben, 2026-10-25): pick a building, see every tile that
            // takes it on the map and in a ledger sorted by profit, press a row to
            // select the tile - and the tile's Build door below places it.
            draw_site_finder(w, reg, ui);
            ImGui::Spacing();
            ImGui::Separator();
            // The door prices the selected tile at its own Logistic Point cover, the
            // figure the site ledger used, so a row and the door agree.
            const float door_cover = w.tiles.count(ui.selected_entity) != 0
                                         ? site_lp_cover(w, ui.selected_entity)
                                         : 1.0f;
            draw_construction_ledger_body(w, reg, ui, door_cover);
        }
        else
            draw_buildings_view(w, reg, report, ui);
    }

    ui::foldout_end();
}

} // namespace ui
