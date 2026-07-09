#include "selection_panel.hpp"

#include "entity_summary.hpp"
#include "foldout_column.hpp" // shell fold-out column host (shared with the ledgers)
#include "icons.hpp"
#include "presentation.hpp"
#include "selection.hpp"
#include "view_nav.hpp"

#include "world/building_profit.hpp" // per-building profitability estimate (BL-074)
#include "world/economy_system.hpp" // economy_report (workforce cap, BL-069)
#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp"
#include "world/survey_system.hpp"
#include "world/workforce.hpp"      // workforce_efficiency (BL-069)

#include <imgui.h>

#include <algorithm> // std::max (bar-width clamp), std::min/std::clamp (build rate, BL-095)
#include <cmath>     // std::ceil (construction ETA, BL-095)
#include <cstring>   // std::strcmp (header title/kind de-dup)
#include <string>    // affordance-row labels (BL-071)
#include <utility>   // std::move
#include <vector>    // affordance groupings (BL-071)

namespace ui {

namespace {

// --- Selected-tile affordance readout (BL-071) -------------------------------
// The inverse of the placement-suitability surface: given a *tile*, which building
// types suit it? Always-on for any selected tile (docs/ui/SELECTION.md), so the
// player can read a tile before arming a building. Shows the tile's territory owner
// and a thrives / valid / invalid grouping over the prototype-buildable types,
// reading the same placement_rules seam the build front door and the armed hover
// card use.
void draw_tile_affordances(const world& w, entity_id tile_id)
{
    const auto tit = w.tiles.find(tile_id);
    if (tit == w.tiles.end())
        return;
    const tile_component& tc = tit->second;

    ImGui::Separator();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Suited for");

    // Territory owner: the nation whose territory contains this tile, if any.
    const char* territory = "unclaimed";
    for (const auto& [nid, nc] : w.nations)
    {
        bool found = false;
        for (entity_id t : nc.tiles)
            if (t == tile_id) { found = true; break; }
        if (found) { territory = nc.name.c_str(); break; }
    }
    ImGui::TextDisabled("Territory: %s", territory);

    struct fit { std::string label; const char* reason; };
    std::vector<fit> thrives, valid, invalid;

    // Extraction: one entry per extractable resource actually deposited here; a
    // rich deposit 'thrives', a thinner one is merely 'valid'. When nothing
    // extractable is present, a single invalid line names the reason.
    bool any_ext = false;
    for (const resource_type r : placement_rules::k_extractable)
    {
        const float dep = tc.resource_deposit[static_cast<std::size_t>(r)];
        if (dep <= 0.0f)
            continue;
        any_ext = true;
        fit f{std::string("Extraction: ") + resource_name(r), nullptr};
        if (dep >= 0.6f) thrives.push_back(std::move(f));
        else             valid.push_back(std::move(f));
    }
    if (!any_ext)
        invalid.push_back({"Extraction",
            placement_rules::placement_reason_text(placement_rules::placement_reason::no_deposit)});

    // Processing facility + Port: the world-level check (a port needs a coast).
    const auto classify = [&](building_type type, const char* label) {
        const placement_rules::placement_result pr =
            placement_rules::can_place_in_world(w, tile_id, type, resource_type::iron_ore);
        if (pr) valid.push_back({label, nullptr});
        else    invalid.push_back({label, pr.message()});
    };
    classify(building_type::processing_facility, "Processing facility");
    classify(building_type::port, "Port");

    // Render the three groups; skip an empty group. Only 'invalid' carries a reason.
    const auto group = [](const char* head, const ImVec4& col,
                          const std::vector<fit>& rows, bool with_reason) {
        if (rows.empty())
            return;
        ImGui::TextColored(col, "%s", head);
        for (const fit& f : rows)
        {
            if (with_reason && f.reason)
                ImGui::BulletText("%s - %s", f.label.c_str(), f.reason);
            else
                ImGui::BulletText("%s", f.label.c_str());
        }
    };
    group("Thrives", ImVec4{0.55f, 0.90f, 0.55f, 1.0f}, thrives, false);
    group("Valid",   ImVec4{0.80f, 0.80f, 0.80f, 1.0f}, valid,   false);
    group("Invalid", ImVec4{0.90f, 0.55f, 0.55f, 1.0f}, invalid, true);
}

// BL-069: population-centre scale label (1–5 → village … metropolis).
const char* scale_label(int scale)
{
    switch (scale)
    {
        case 1:  return "village";
        case 2:  return "town";
        case 3:  return "city";
        case 4:  return "conurbation";
        case 5:  return "metropolis";
        default: return "settlement";
    }
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

// --- Build front door (tile Selection element) -------------------------------
// Compact cost + material annotation for a build button (BL-044/BL-095-lite
// legibility): the credit cost plus every required material, priced at the
// tile's local market and folded into the credit total. `ok` gates the button
// on balance alone — materials are bought from the market, not drawn from the
// corp's own pool, so the label communicates the requirement without a
// separate pool-availability gate (construction.cpp mirrors this).
struct build_afford { std::string label; bool ok; };
build_afford build_cost_annotation(const world& w, const recipe_registry& reg,
                                   building_type type, entity_id tile, float balance)
{
    const building_economics& e = reg.economics(type);

    const market_component* mkt = nullptr;
    {
        const entity_id mid = market_for_tile(w, tile);
        if (mid != null_entity)
        {
            const auto mit = w.markets.find(mid);
            if (mit != w.markets.end())
                mkt = &mit->second;
        }
    }

    float material_cost = 0.0f;
    std::string s = std::to_string(static_cast<int>(e.build_cost)) + " cr";
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const float need = e.resource_build_cost[r];
        if (need <= 0.0f)
            continue;
        const float p = mkt ? (mkt->price[r] > 0.0f ? mkt->price[r] : mkt->base_price[r]) : 0.0f;
        material_cost += need * p;
        s += " · " + std::to_string(static_cast<int>(need)) + ' '
           + resource_name(static_cast<resource_type>(r));
    }
    const float total = e.build_cost + material_cost;
    return { s, balance >= total };
}

// BL-095 task E: the prospective build's *starting* material status, rendered under a
// front-door build button. Recomputes the same rate run_construction will apply on
// the first tick — the per-tick material need vs the candidate tile's local market
// supply — so the player reads "materials scarce here" (or "paused") before
// committing, in place of the old binary yes/no. A build with no material cost has
// nothing to gate, so its status line is omitted. On schedule stays muted (the cost
// annotation already carries the detail); scarce/paused speak up.
void draw_prospective_build_status(const world& w, const recipe_registry& reg,
                                   building_type type, entity_id tile)
{
    const building_economics& econ = reg.economics(type);
    bool needs_material = false;
    for (std::size_t r = 0; r < resource_count; ++r)
        if (econ.resource_build_cost[r] > 0.0f) { needs_material = true; break; }
    if (!needs_material)
        return;

    const float rate = construction_rate(w, reg, type, tile);
    // Match construct_building's ticks_remaining seeding (truncated build_duration).
    const int   base = static_cast<int>(econ.build_duration_ticks);
    const std::string msg = construction_status(rate, base);
    if (rate >= 1.0f)
        ImGui::TextDisabled("%s", msg.c_str());
    else
        ImGui::TextColored(construction_status_colour(rate), "%s", msg.c_str());
}

// The per-tile entry to construction (docs/ui/SELECTION.md): offers the buildable
// types + cost and, on click, enqueues a construction request on this tile for the
// player corporation. The request is executed by app against the mutable world —
// here we only read and enqueue. Reached only for a selected tile.
void draw_build_front_door(const world& w, const recipe_registry& reg,
                           ui_state& ui, entity_id tile)
{
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return;
    const tile_component& tc = tit->second;

    ImGui::Separator();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Build here");

    // A whole-tile blocker (ocean) means no building type is offerable here; show
    // the reason-code text rather than a bare hardcoded string (BL-071). A generic
    // non-extraction building surfaces the tile-level verdict.
    if (const placement_rules::placement_result r =
            placement_rules::can_place(tc, building_type::processing_facility,
                                       resource_type::iron_ore);
        !r)
    {
        ImGui::TextDisabled("%s.", r.message());
        return;
    }

    // Player balance gates affordability (a building the corp cannot pay for is
    // offered disabled, so the cost is still legible).
    float balance = 0.0f;
    const auto pit = w.corporations.find(w.player_entity);
    if (pit != w.corporations.end())
        balance = pit->second.balance;

    // Enqueue helper — sets the pending request app executes this frame.
    auto enqueue = [&ui, tile](building_type type, resource_type target) {
        ui.construction.pending_tile   = tile;
        ui.construction.pending_type   = type;
        ui.construction.pending_target = target;
    };

    // --- Extraction: only the extractable resources actually deposited here ---
    bool any_extractable = false;
    for (const resource_type r : placement_rules::k_extractable)
        if (tc.resource_deposit[static_cast<std::size_t>(r)] > 0.0f)
            any_extractable = true;

    if (any_extractable)
    {
        // A compact target picker over the deposits present, writing construction.target.
        if (!placement_rules::is_extractable(ui.construction.target) ||
            tc.resource_deposit[static_cast<std::size_t>(ui.construction.target)] <= 0.0f)
        {
            // Default the target to the first present extractable so the button is valid.
            for (const resource_type r : placement_rules::k_extractable)
                if (tc.resource_deposit[static_cast<std::size_t>(r)] > 0.0f)
                { ui.construction.target = r; break; }
        }
        for (const resource_type r : placement_rules::k_extractable)
        {
            if (tc.resource_deposit[static_cast<std::size_t>(r)] <= 0.0f)
                continue;
            const bool sel = (ui.construction.target == r);
            if (ImGui::RadioButton(resource_name(r), sel))
                ui.construction.target = r;
            ImGui::SameLine();
        }
        ImGui::NewLine();

        const build_afford ext =
            build_cost_annotation(w, reg, building_type::extraction_site, tile, balance);
        ImGui::BeginDisabled(!ext.ok);
        if (ImGui::Button("Build extraction site"))
            enqueue(building_type::extraction_site, ui.construction.target);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("%s", ext.label.c_str());
        draw_prospective_build_status(w, reg, building_type::extraction_site, tile);
    }
    else
    {
        ImGui::TextDisabled("No extractable deposit here.");
    }

    // --- Processing facility + Port: any non-ocean land tile ---
    const build_afford proc =
        build_cost_annotation(w, reg, building_type::processing_facility, tile, balance);
    ImGui::BeginDisabled(!proc.ok);
    if (ImGui::Button("Build processing facility"))
        enqueue(building_type::processing_facility, resource_type::iron_ore);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", proc.label.c_str());
    draw_prospective_build_status(w, reg, building_type::processing_facility, tile);

    const build_afford prt =
        build_cost_annotation(w, reg, building_type::port, tile, balance);
    ImGui::BeginDisabled(!prt.ok);
    if (ImGui::Button("Build port"))
        enqueue(building_type::port, resource_type::iron_ore);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", prt.label.c_str());
    draw_prospective_build_status(w, reg, building_type::port, tile);

    // Outcome of the most recent attempt, if any.
    if (!ui.construction.last_message.empty())
        ImGui::TextDisabled("%s", ui.construction.last_message.c_str());
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
        case selection_kind::tile:
            // Build front door — the buildable types + cost, a tile's primary move.
            draw_build_front_door(w, reg, ui, sel);
            break;

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
                // "still building" rather than an empty card, before the Manage control.
                if (const auto bit = w.buildings.find(sel);
                    bit != w.buildings.end() && bit->second.ticks_remaining > 0)
                {
                    const building_component& b = bit->second;
                    const float rate = construction_rate(w, reg, b.type, b.tile);
                    ImGui::TextColored(construction_status_colour(rate), "%s",
                        construction_status(rate, b.ticks_remaining).c_str());
                }
                // Manage — routes to the building-management panel (construction_panel),
                // which owns the workforce / recipe / decommission controls.
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Manage");
                if (ImGui::Button("Manage building"))
                    ui.show_construction_panel = true;
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
        case selection_kind::tile:
            draw_tile_affordances(w, sel);          // BL-071: what this tile is good for
            break;
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

} // namespace

void draw_selection_panel(const world& w, const recipe_registry& reg,
                          const economy_report& report, ui_state& ui)
{
    const selection_kind kind = selection_kind_of(w, ui.selected_entity);

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

        const float btn = frame_h;
        ImGui::SameLine(bar_w - style.WindowPadding.x - 2.0f * btn - style.ItemSpacing.x);
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

} // namespace ui
