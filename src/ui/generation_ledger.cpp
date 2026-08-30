#include "generation_ledger.hpp"

#include "foldout_column.hpp" // the shell-column host + nav_button's toggle rule
#include "presentation.hpp"   // substrate_name / cover_name / landform_name
#include "world/components.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace ui {

namespace {

// The record stores the latitude band as a raw uint8_t because the band enum is
// private to tile_generation.cpp. These names mirror that enum's declaration
// order (polar, subpolar, temperate, subtropical, tropical) — the one coupling
// this file has to the generator's internals, kept in a single place.
const char* band_name(std::uint8_t b)
{
    static const char* k_names[] = { "Polar", "Subpolar", "Temperate", "Subtropical", "Tropical" };
    return (b < 5) ? k_names[b] : "?";
}

const char* temperature_name(temperature_class t)
{
    switch (t)
    {
        case temperature_class::scorching: return "Scorching";
        case temperature_class::hot:       return "Hot";
        case temperature_class::temperate: return "Temperate";
        case temperature_class::cold:      return "Cold";
        case temperature_class::frozen:    return "Frozen";
    }
    return "?";
}

const char* atmosphere_name(atmosphere_class a)
{
    switch (a)
    {
        case atmosphere_class::none:     return "None";
        case atmosphere_class::thin:     return "Thin";
        case atmosphere_class::moderate: return "Moderate";
        case atmosphere_class::thick:    return "Thick";
    }
    return "?";
}

const char* hydrology_name(hydrological_state h)
{
    switch (h)
    {
        case hydrological_state::none:         return "None";
        case hydrological_state::polar_frozen: return "Polar frozen";
        case hydrological_state::liquid:       return "Liquid";
    }
    return "?";
}

const char* geology_name(geological_activity g)
{
    switch (g)
    {
        case geological_activity::none:     return "None";
        case geological_activity::low:      return "Low";
        case geological_activity::moderate: return "Moderate";
        case geological_activity::high:     return "High";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Regenerate on demand, do not persist (GENERATION_LEDGER.md § Data lifetime)
// ---------------------------------------------------------------------------
//
// Generation is deterministic in (seed, profile, gw, gh, bias), so the record is
// a VIEW, not state: it is rebuilt for whichever body the ledger is looking at
// and thrown away when the subject changes. Storing one per tile for every body
// would be ~15k height/moisture/band triples on the homeworld alone, serialised
// into every save, to hold what one function call reproduces exactly.
//
// The cache is keyed on (body, tile seed): the seed changes with the world, so
// regenerating a campaign cannot leave the previous world's record on screen —
// the stale-cache defect the Ages view was fixed for.
struct ledger_cache
{
    entity_id         body = null_entity;
    std::uint32_t     seed = 0;
    generation_record rec;
};

const generation_record* record_for(const generation_report::body_entry& e)
{
    static ledger_cache cache;

    if (!e.tiles.valid || e.tiles.gw <= 0 || e.tiles.gh <= 0)
        return nullptr;
    if (cache.body == e.id && cache.seed == e.tiles.seed && cache.rec.gw == e.tiles.gw)
        return &cache.rec;

    // A scratch world, because generate_body_tiles creates tile entities as it
    // goes and the LIVE world already has this body's tiles. Only the record is
    // kept; the scratch tiles die with the local. Same shape as the acceptance
    // gate's probe generations in hard_coded_world.cpp.
    world scratch;
    const entity_id probe = scratch.create_entity();

    generation_record rec;
    generate_body_tiles(scratch, probe, e.tiles.gw, e.tiles.gh, e.state.profile,
                        e.tiles.seed, e.tiles.deposit_scalar, &e.state, &rec,
                        &e.continents.height_bias,
                        e.tiles.used_convergent ? &e.continents.convergent : nullptr);

    cache.rec  = std::move(rec);
    cache.body = e.id;
    cache.seed = e.tiles.seed;
    return &cache.rec;
}

/// The body's tiles indexed in raster order (grid_y*gw + grid_x), so the record's
/// intermediates and the world's final tiles can be read at the same index.
///
/// Built here rather than through `body_tile_grid` (logistics.hpp) because that
/// one takes a MUTABLE world to populate its cache, and every UI surface holds a
/// const one. Independent of the tiles-map iteration order, so it is stable.
std::vector<entity_id> raster_index(const world& w, entity_id body, int gw, int gh)
{
    std::vector<entity_id> out(static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh),
                               null_entity);
    for (const auto& [id, t] : w.tiles)
    {
        if (t.body != body) continue;
        if (t.grid_x < 0 || t.grid_x >= gw || t.grid_y < 0 || t.grid_y >= gh) continue;
        out[static_cast<std::size_t>(t.grid_y) * static_cast<std::size_t>(gw)
            + static_cast<std::size_t>(t.grid_x)] = id;
    }
    return out;
}

// --- Section and table furniture ------------------------------------------
//
// Every section on this surface is a collapsing header over a table (Ben,
// 2026-08-30: "reformat each section in 'Body' to a dropdown header showing a
// table"). Two helpers rather than six hand-rolled blocks, so the six cannot
// drift into six slightly different table styles - which is exactly what the
// hand-aligned `%-12s %6d` rows had already started to do.

constexpr ImGuiTableFlags k_table_flags =
    ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
    ImGuiTableFlags_SizingStretchProp;

/// One collapsing section header. `open` lives in `ui_state` rather than ImGui's
/// own storage for the `construction_panel` reason: it is then stable across a
/// rebuild and drivable by a verify script. The `###id` suffix keeps the widget's
/// identity fixed while the visible label carries a live count.
///
/// A `CollapsingHeader` is a toggle by construction, so the standing Toggle rule
/// is satisfied without a second control.
bool section(const char* label, const char* id, bool& open)
{
    ImGui::SetNextItemOpen(open, ImGuiCond_Always);
    char hdr[128];
    std::snprintf(hdr, sizeof hdr, "%s###%s", label, id);
    const bool now = ImGui::CollapsingHeader(hdr);
    if (now != open)
        open = now;
    return now;
}

/// A two-column Field / Value table - the shape Profile and Thresholds share.
struct field_row { const char* label; std::string value; };

void field_table(const char* id, const std::vector<field_row>& rows)
{
    if (!ImGui::BeginTable(id, 2, k_table_flags))
        return;
    // NO HEADER ROW. "Field | Value" over a list of Temperature / Atmosphere /
    // Hydrology names nothing the rows do not already say, and it repeats on every
    // key-value section. The distribution tables DO carry one, because "Tiles" and
    // "Share" are not inferable from the numbers under them.
    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch, 1.6f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    for (const field_row& r : rows)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(r.label);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(r.value.c_str());
    }
    ImGui::EndTable();
}

/// A category / count / share table - the shape the three distributions share.
struct dist_row { const char* label; int count; };

/// ZERO ROWS ARE SHOWN, which reverses this file's earlier rule ("an absent
/// category says nothing; the zero rows crowd out the signal"). On a TUNING
/// surface an absent category says a great deal: `valley` never fires on the
/// default body, and under the old rule that was indistinguishable from a
/// landform that does not exist. A bordered table of seven rows has room for the
/// zeroes that a hand-aligned text block did not.
void dist_table(const char* id, const std::vector<dist_row>& rows, int total)
{
    if (!ImGui::BeginTable(id, 3, k_table_flags))
        return;
    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthStretch, 2.2f);
    ImGui::TableSetupColumn("Tiles",    ImGuiTableColumnFlags_WidthStretch, 1.2f);
    ImGui::TableSetupColumn("Share",    ImGuiTableColumnFlags_WidthStretch, 1.2f);
    ImGui::TableHeadersRow();
    for (const dist_row& r : rows)
    {
        const float pct = (total > 0)
            ? 100.0f * static_cast<float>(r.count) / static_cast<float>(total) : 0.0f;
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(r.label);
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%d", r.count);
        ImGui::TableSetColumnIndex(2);
        if (r.count == 0) ImGui::TextDisabled("-");
        else              ImGui::TextDisabled("%.2f%%", static_cast<double>(pct));
    }
    ImGui::EndTable();
}

} // namespace

// ---------------------------------------------------------------------------
// The ledger window
// ---------------------------------------------------------------------------

void draw_generation_ledger(const world& w, ui_state& s,
                            const generation_report& report, bool* p_open)
{
    if (p_open && !*p_open)
        return;

    if (!ui::foldout_begin("Generation Ledger"))
    {
        ui::foldout_end();
        return;
    }

    if (report.bodies.empty())
    {
        ImGui::TextDisabled("No generation report - nothing to explain.");
        ui::foldout_end();
        return;
    }

    // --- Subject ------------------------------------------------------------
    // Defaults to the canvas's active body, mirroring the Tile Ledger: the body
    // you are looking at is the body you are asking about.
    static entity_id subject = null_entity;
    auto known = [&](entity_id id) {
        for (const auto& be : report.bodies) if (be.id == id) return true;
        return false;
    };
    if (!known(subject))
        subject = known(s.active_body) ? s.active_body : report.bodies.front().id;

    const generation_report::body_entry* entry = nullptr;
    for (const auto& be : report.bodies)
        if (be.id == subject) { entry = &be; break; }

    // ONE FLAT PANEL OF STACKED SECTIONS, not a tab strip (Ben, 2026-08-30). The
    // Tile view - the per-tile derivation breadcrumb - was retired with the strip
    // that carried it, leaving the Body selector below as the only cross-cutting
    // control. The breadcrumb itself survives as `draw_tile_derivation`; see the
    // note on its declaration in generation_ledger.hpp.
    //
    // With no tabs there is no active-tab press to close the ledger. That is the
    // Balance ledger's shape and needs no extra control: the rail slot toggles the
    // surface, and each section header toggles itself.
    if (ImGui::BeginCombo("Body", entry ? entry->name.c_str() : "-"))
    {
        for (const auto& be : report.bodies)
        {
            const bool is_sel = (be.id == subject);
            if (ImGui::Selectable(be.name.c_str(), is_sel))
                subject = be.id;
            if (is_sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (!entry)
    {
        ImGui::TextDisabled("No report entry for that body.");
        ui::foldout_end();
        return;
    }

    const generation_record* rec = record_for(*entry);
    if (!rec)
    {
        ImGui::TextDisabled("This body's tile pass recorded no inputs, so its "
                            "intermediates cannot be reproduced.");
        ui::foldout_end();
        return;
    }
    ImGui::Separator();

    // --- The body's aggregate shape -----------------------------------------
    // Six sections, each a collapsing header over a table. What was asked for
    // (Profile, Thresholds) reads above what came out (the three distributions),
    // because a histogram that surprises is traced back to the profile above it.
    const body_profile& p = entry->state.profile;
    const int total = rec->gw * rec->gh;

    char buf[128];

    // --- Profile -------------------------------------------------------------
    if (section("Profile", "gen_profile", s.gen_profile_open))
    {
        std::snprintf(buf, sizeof buf, "%dx%d  (%d tiles)", rec->gw, rec->gh, total);
        field_table("##gen_profile_tbl", {
            {"Temperature", temperature_name(p.temperature)},
            {"Atmosphere",  atmosphere_name(p.atmosphere)},
            {"Hydrology",   hydrology_name(p.hydrology)},
            {"Geology",     geology_name(p.geology)},
            {"Bias",        p.bias == composition_bias::metallic ? "Metallic" : "Standard"},
            {"Grid",        buf},
        });
    }

    // --- Thresholds ----------------------------------------------------------
    // The tuning read. A histogram that surprises is traced back to these, which
    // is the whole reason the profile is echoed above it.
    if (section("Thresholds", "gen_thresholds", s.gen_thresholds_open))
    {
        if (rec->ocean_score.empty())
        {
            std::snprintf(buf, sizeof buf, "no ocean pass (hydrology = %s)",
                          hydrology_name(p.hydrology));
            field_table("##gen_thresh_tbl", {{"Ocean pass", buf}});
        }
        else
        {
            const float actual = (total > 0)
                ? static_cast<float>(rec->ocean_tiles) / static_cast<float>(total) : 0.0f;
            std::vector<field_row> rows;
            char t0[64], t1[64], t2[96];
            std::snprintf(t0, sizeof t0, "%.4f", static_cast<double>(rec->ocean_threshold));
            std::snprintf(t1, sizeof t1, "%d", rec->ocean_tiles);
            std::snprintf(t2, sizeof t2, "%.3f   (target %.3f)",
                          static_cast<double>(actual),
                          static_cast<double>(p.water_fraction));
            rows.push_back({"Ocean threshold", t0});
            rows.push_back({"Ocean tiles",     t1});
            rows.push_back({"Water fraction",  t2});
            field_table("##gen_thresh_tbl", rows);
        }
    }

    // --- Latitude bands ------------------------------------------------------
    // Read off the record rather than restated from the generator's table - so
    // this cannot drift from the widths actually used.
    if (section("Latitude bands", "gen_bands", s.gen_bands_open))
    {
        if (rec->band.empty())
        {
            ImGui::TextDisabled("No band data recorded.");
        }
        else if (ImGui::BeginTable("##gen_bands_tbl", 3, k_table_flags))
        {
            ImGui::TableSetupColumn("Band", ImGuiTableColumnFlags_WidthStretch, 2.2f);
            ImGui::TableSetupColumn("Rows", ImGuiTableColumnFlags_WidthStretch, 1.6f);
            ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthStretch, 0.9f);
            ImGui::TableHeadersRow();

            int run_start = 0;
            for (int row = 1; row <= rec->gh; ++row)
            {
                const std::uint8_t prev = rec->band[static_cast<std::size_t>(run_start) * static_cast<std::size_t>(rec->gw)];
                const bool end_of_run = (row == rec->gh)
                    || rec->band[static_cast<std::size_t>(row) * static_cast<std::size_t>(rec->gw)] != prev;
                if (end_of_run)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(band_name(prev));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d - %d", run_start, row - 1);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextDisabled("%d", row - run_start);
                    run_start = row;
                }
            }
            ImGui::EndTable();
        }
    }

    // --- The three distributions ---------------------------------------------
    // Histograms over the LIVE tiles: the record holds the intermediates, the
    // world holds the outcome, and the outcome is what a balance question is about.
    //
    // EVERY DENOMINATOR IS NAMED IN ITS HEADER, and it is the same one for all
    // three: the whole grid, ocean included. That matters most on Landform, where
    // water carries `plains` and so inflates the plains share far above its share
    // of LAND. Naming the denominator does not fix that - it makes it visible,
    // which is the least a tuning surface owes its reader.
    const std::vector<entity_id> tiles = raster_index(w, subject, rec->gw, rec->gh);
    // BL-519: two histograms where there was one. Collapsing them back into a
    // single 12-row table is exactly the overloading the axis split undid.
    std::array<int, 10> sub_counts{}; // BL-516: 8 -> 10, lake and coast appended
    std::array<int, 10> cov_counts{};
    std::array<int, 7>  land_counts{};
    int counted = 0;      // every tile on the body - Substrate's and Cover's denominator
    int land_counted = 0; // land only - Landform's, see the note at that section
    for (entity_id id : tiles)
    {
        if (id == null_entity) continue;
        const tile_component& t = w.tiles.at(id);
        const std::size_t su = static_cast<std::size_t>(t.substrate);
        const std::size_t cv = static_cast<std::size_t>(t.cover);
        const std::size_t l  = static_cast<std::size_t>(t.landform);
        if (su < sub_counts.size()) ++sub_counts[su];
        if (cv < cov_counts.size()) ++cov_counts[cv];
        ++counted;
        if (is_water(t.substrate)) continue; // NR-740: water has no landform
        if (l < land_counts.size()) ++land_counts[l];
        ++land_counted;
    }

    std::snprintf(buf, sizeof buf, "Substrate  (%d tiles)", counted);
    if (section(buf, "gen_substrate", s.gen_substrate_open))
    {
        std::vector<dist_row> rows;
        for (std::size_t c = 0; c < sub_counts.size(); ++c)
            rows.push_back({substrate_name(static_cast<terrain_substrate>(c)), sub_counts[c]});
        dist_table("##gen_sub_tbl", rows, counted);
    }

    std::snprintf(buf, sizeof buf, "Cover  (%d tiles)", counted);
    if (section(buf, "gen_cover", s.gen_cover_open))
    {
        std::vector<dist_row> rows;
        for (std::size_t c = 0; c < cov_counts.size(); ++c)
            rows.push_back({cover_name(static_cast<terrain_cover>(c)), cov_counts[c]});
        dist_table("##gen_cov_tbl", rows, counted);
    }

    // LANDFORM IS THE ONE TABLE TAKEN OVER LAND, not over the grid (Ben's
    // ruling on NR-740, 2026-08-30). Water carries `terrain_landform::plains` -
    // there is no water landform - and the grid is more than half ocean, so a
    // whole-grid denominator reported 95.27% plains and 0.72% mountain where the
    // answer over land is nearer 88% and 1.8%. Every share on the table was a
    // share of a question nobody asks.
    //
    // Substrate and Cover keep the whole grid deliberately, and the asymmetry is
    // the point rather than an inconsistency: ocean is one of Substrate's OWN
    // categories, so excluding it there would delete a real row. Each header
    // names which denominator it used, so the two never have to be inferred.
    //
    // `is_water` (components.hpp) is the single definition, and it settles the
    // question this ruling turned on: COAST IS WATER, with lake and ocean.
    std::snprintf(buf, sizeof buf, "Landform  (%d land tiles)", land_counted);
    if (section(buf, "gen_landform", s.gen_landform_open))
    {
        std::vector<dist_row> rows;
        for (std::size_t l = 0; l < land_counts.size(); ++l)
            rows.push_back({landform_name(static_cast<terrain_landform>(l)), land_counts[l]});
        dist_table("##gen_land_tbl", rows, land_counted);
    }

    ui::foldout_end();
}

} // namespace ui
