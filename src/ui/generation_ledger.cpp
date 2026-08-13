#include "generation_ledger.hpp"

#include "foldout_column.hpp" // the shell-column host + nav_button's toggle rule
#include "presentation.hpp"   // composition_name / landform_name / resource_name
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

// Pass 4's moisture column: the three-way split the (band x moisture) table is
// indexed by. Mirrors moisture_column() in tile_generation.cpp.
int moisture_col(float m) { return m < 0.35f ? 0 : (m < 0.55f ? 1 : 2); }
const char* moisture_col_name(int c) { return c == 0 ? "dry" : (c == 1 ? "mid" : "wet"); }

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

/// One histogram row: a label, a count, and the share of the total.
void histogram_row(const char* label, int count, int total)
{
    if (count == 0) return; // an absent category says nothing; the zero rows crowd out the signal
    const float pct = (total > 0) ? 100.0f * static_cast<float>(count) / static_cast<float>(total) : 0.0f;
    ImGui::Text("%-12s %6d", label, count);
    ImGui::SameLine();
    ImGui::TextDisabled("%5.2f%%", static_cast<double>(pct));
}

} // namespace

// ---------------------------------------------------------------------------
// The per-tile derivation breadcrumb
// ---------------------------------------------------------------------------

void draw_tile_derivation(const world& w, const generation_record& rec,
                          const generation_report::body_entry& entry,
                          entity_id tile_id)
{
    const auto it = w.tiles.find(tile_id);
    if (it == w.tiles.end())
    {
        ImGui::TextDisabled("That entity is not a tile.");
        return;
    }
    const tile_component& t = it->second;
    if (rec.gw <= 0 || t.grid_x >= rec.gw || t.grid_y >= rec.gh)
    {
        ImGui::TextDisabled("Tile lies outside the regenerated grid.");
        return;
    }

    const std::size_t idx = static_cast<std::size_t>(t.grid_y) * static_cast<std::size_t>(rec.gw)
                          + static_cast<std::size_t>(t.grid_x);
    const body_profile& p  = entry.state.profile;
    const float  height    = rec.height[idx];
    const float  moisture  = rec.moisture[idx];
    const std::uint8_t bnd = rec.band[idx];
    const bool   is_ocean  = (t.composition == terrain_composition::ocean);

    ImGui::Text("Tile %d, %d", t.grid_x, t.grid_y);
    ImGui::Separator();

    // --- 1. Height, and the first fork -------------------------------------
    ImGui::TextDisabled("1  HEIGHT");
    ImGui::Text("height  %.4f", static_cast<double>(height));
    if (rec.ocean_score.empty())
    {
        ImGui::TextWrapped("No ocean pass ran (hydrology = %s), so every tile is land "
                           "and height decides nothing here on its own.",
                           hydrology_name(p.hydrology));
    }
    else
    {
        const float score = rec.ocean_score[idx];
        ImGui::Text("sea score  %.4f  vs threshold  %.4f",
                    static_cast<double>(score), static_cast<double>(rec.ocean_threshold));
        ImGui::TextWrapped("%s. The score is the height after Pass 2's equatorial "
                           "down-bias; the threshold is the %.0f%% percentile of it.",
                           score < rec.ocean_threshold ? "Below - OCEAN" : "Above - LAND",
                           static_cast<double>(p.water_fraction * 100.0f));
    }
    ImGui::Spacing();

    // --- 2. Latitude band and moisture -------------------------------------
    ImGui::TextDisabled("2  BAND & MOISTURE");
    ImGui::Text("band  %s  (row %d of %d, %s widths)",
                band_name(bnd), t.grid_y, rec.gh, temperature_name(p.temperature));
    ImGui::Text("moisture  %.4f  ->  %s column",
                static_cast<double>(moisture), moisture_col_name(moisture_col(moisture)));
    ImGui::Spacing();

    // --- 3. Composition, and which branch chose it -------------------------
    ImGui::TextDisabled("3  COMPOSITION");
    ImGui::Text("%s", composition_name(t.composition));
    if (is_ocean)
    {
        ImGui::TextWrapped("Set by Pass 2, not the climate table - an ocean tile never "
                           "reaches Pass 4.");
    }
    else
    {
        const bool airless = p.atmosphere == atmosphere_class::none
                          || p.atmosphere == atmosphere_class::thin;
        const bool biotic  = entry.state.stage >= life_stage::land;

        if (p.bias == composition_bias::metallic)
        {
            ImGui::TextWrapped("Metallic bias overrides the table entirely: "
                               "55/30/15 metallic / rocky / regolith.");
        }
        else if (airless)
        {
            if (p.hydrology == hydrological_state::polar_frozen && bnd == 0)
                ImGui::TextWrapped("Airless body, polar band, frozen poles: the polar "
                                   "override sets icy without a draw.");
            else if (p.temperature == temperature_class::scorching
                  || p.temperature == temperature_class::hot)
                ImGui::TextWrapped("Airless and hot: 45/40/15 volcanic / barren / rocky.");
            else
                ImGui::TextWrapped("Airless and cool: 65/30 regolith / rocky.");
        }
        else if (t.composition == terrain_composition::volcanic && (bnd == 3 || bnd == 4))
        {
            ImGui::TextWrapped("The volcanic roll fired: only the subtropical and tropical "
                               "bands offer it, at p = %.3f for %s geological activity.",
                               static_cast<double>(
                                   p.geology == geological_activity::high     ? 0.375f :
                                   p.geology == geological_activity::moderate ? 0.125f :
                                   p.geology == geological_activity::low      ? 0.040f : 0.0f),
                               geology_name(p.geology));
        }
        else if (t.composition == terrain_composition::wetland && bnd == 2)
        {
            // The table's wet cell yields wetland in the subtropical and tropical
            // bands ONLY, so a temperate marsh can only have come from Pass 4b.
            ImGui::TextWrapped("Pass 4b drainage: the temperate band's climate table has no "
                               "wetland cell, so this is low-lying, high-moisture ground "
                               "that carried a biotic cover and could not drain.");
        }
        else
        {
            ImGui::TextWrapped("The %s climate table, cell (%s, %s).%s",
                               biotic ? "biotic" : "abiotic",
                               band_name(bnd), moisture_col_name(moisture_col(moisture)),
                               t.composition == terrain_composition::wetland
                                   ? " Pass 4b drainage can also produce wetland here; the "
                                     "record does not distinguish the two in this band."
                                   : "");
            if (!biotic)
                ImGui::TextWrapped("Abiotic because the biosphere never reached land "
                                   "(life stage %s) - grassland, forest, wetland and tundra "
                                   "are masked out.", life_stage_name(entry.state.stage));
        }
    }
    ImGui::Spacing();

    // --- 4. Landform --------------------------------------------------------
    ImGui::TextDisabled("4  LANDFORM");
    ImGui::Text("%s", landform_name(t.landform));
    switch (t.landform)
    {
        case terrain_landform::mountain:
        case terrain_landform::rift:
        case terrain_landform::crater:
            ImGui::TextWrapped("A Pass 5 cluster CORE - ring 0 or 1 of a %s seed. WHICH "
                               "seed is not recoverable: the record does not capture seed "
                               "positions (GENERATION_LEDGER.md, open items).",
                               t.landform == terrain_landform::mountain ? "mountain range"
                             : t.landform == terrain_landform::rift     ? "rift zone"
                                                                       : "crater");
            break;
        // Highland and canyon are never seeded directly - they are what a cluster's
        // outer rings decay into (landform_at_ring), so reading them as a separate
        // feature would misattribute the shoulder of a range to a rule of its own.
        case terrain_landform::highland:
            ImGui::TextWrapped("A cluster SHOULDER: the outer rings of a mountain range "
                               "or crater decay to highland before dissolving to plains.");
            break;
        case terrain_landform::canyon:
            ImGui::TextWrapped("A cluster SHOULDER: ring 1+ of a rift zone, which grows "
                               "east-west so the fault reads as a linear feature.");
            break;
        case terrain_landform::valley:
            ImGui::TextWrapped("The valley fill: land no cluster claimed, sitting below "
                               "height 0.35 (this tile is %.4f).", static_cast<double>(height));
            break;
        default: // plains
            ImGui::TextWrapped("Plains - either untouched by Pass 5, or the outermost ring "
                               "of a cluster that decayed back to flat ground. The valley "
                               "fill did not take it: %s.",
                               height < 0.35f
                                   ? "it is below the 0.35 cut, so a cluster must have "
                                     "claimed it first"
                                   : "it sits above the 0.35 cut");
            break;
    }
    ImGui::Spacing();

    // --- 5. Deposits --------------------------------------------------------
    ImGui::TextDisabled("5  DEPOSITS");
    if (is_ocean)
    {
        ImGui::TextWrapped("Ocean carries no land deposits.");
        return;
    }

    int shown = 0;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const float richness = t.resource_deposit[r];
        if (richness <= 0.0f) continue;
        ++shown;
        ImGui::Text("%-18s %8.1f", resource_name(static_cast<resource_type>(r)),
                    static_cast<double>(richness));
        ImGui::SameLine();
        ImGui::TextDisabled("endowment x%.2f",
                            static_cast<double>(entry.state.endowment[r]));
    }
    if (shown == 0)
        ImGui::TextDisabled("None - the %s / %s profile rolls no deposit here.",
                            composition_name(t.composition), landform_name(t.landform));

    ImGui::TextWrapped("Rolled from the (%s, %s) deposit profile, then post-multiplied by "
                       "the abundance scalar (x%.2f), the Planetology endowment, and the ore-"
                       "province field. Those multiplies draw no RNG, so the figure above is "
                       "the roll times a known set of factors - not a separate draw.",
                       composition_name(t.composition), landform_name(t.landform),
                       static_cast<double>(entry.tiles.deposit_scalar));
}

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

    enum view_id { view_body = 0, view_tile };
    int& view = s.generation_ledger_view;
    if (view < view_body || view > view_tile)
        view = view_body;

    // Toggle rule: re-clicking the ACTIVE tab closes the ledger (p_open), which is
    // also what the rail slot does. Switching tabs is an ordinary view change.
    nav_button("Body", view_body, view, p_open);
    ImGui::SameLine();
    nav_button("Tile", view_tile, view, p_open);
    ImGui::Separator();

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

    // --- Tile: the derivation breadcrumb ------------------------------------
    if (view == view_tile)
    {
        const auto sel = w.tiles.find(s.selected_entity);
        if (sel == w.tiles.end())
            ImGui::TextWrapped("Select a tile on the Planetary canvas to see how it "
                               "was derived.");
        else if (sel->second.body != subject)
            ImGui::TextWrapped("The selected tile belongs to another body. Switch the "
                               "selector above, or select a tile on %s.",
                               entry->name.c_str());
        else
            draw_tile_derivation(w, *rec, *entry, s.selected_entity);

        ui::foldout_end();
        return;
    }

    // --- Body: the aggregate shape ------------------------------------------
    const body_profile& p = entry->state.profile;
    const int total = rec->gw * rec->gh;

    ImGui::TextDisabled("PROFILE");
    ImGui::Text("Temperature  %s", temperature_name(p.temperature));
    ImGui::Text("Atmosphere   %s", atmosphere_name(p.atmosphere));
    ImGui::Text("Hydrology    %s", hydrology_name(p.hydrology));
    ImGui::Text("Geology      %s", geology_name(p.geology));
    ImGui::Text("Bias         %s",
                p.bias == composition_bias::metallic ? "Metallic" : "Standard");
    ImGui::Text("Grid         %dx%d  (%d tiles)", rec->gw, rec->gh, total);
    ImGui::Spacing();

    // Thresholds: the tuning read. A histogram that surprises is traced back to
    // these, which is the whole reason the profile is echoed above it.
    ImGui::TextDisabled("THRESHOLDS");
    if (rec->ocean_score.empty())
    {
        ImGui::TextWrapped("No ocean pass (hydrology = %s).", hydrology_name(p.hydrology));
    }
    else
    {
        const float actual = (total > 0)
            ? static_cast<float>(rec->ocean_tiles) / static_cast<float>(total) : 0.0f;
        ImGui::Text("ocean threshold  %.4f", static_cast<double>(rec->ocean_threshold));
        ImGui::Text("ocean tiles      %d", rec->ocean_tiles);
        ImGui::Text("water fraction   %.3f  (target %.3f)",
                    static_cast<double>(actual), static_cast<double>(p.water_fraction));
    }
    ImGui::Spacing();

    // Band boundaries, read off the record rather than restated from the
    // generator's table — so this cannot drift from the widths actually used.
    ImGui::TextDisabled("LATITUDE BANDS");
    if (!rec->band.empty())
    {
        int run_start = 0;
        for (int row = 1; row <= rec->gh; ++row)
        {
            const std::uint8_t prev = rec->band[static_cast<std::size_t>(run_start) * static_cast<std::size_t>(rec->gw)];
            const bool end_of_run = (row == rec->gh)
                || rec->band[static_cast<std::size_t>(row) * static_cast<std::size_t>(rec->gw)] != prev;
            if (end_of_run)
            {
                ImGui::Text("%-12s rows %3d - %3d  (%d)", band_name(prev),
                            run_start, row - 1, row - run_start);
                run_start = row;
            }
        }
    }
    ImGui::Spacing();

    // Histograms over the LIVE tiles: the record holds the intermediates, the
    // world holds the outcome, and the outcome is what a balance question is about.
    const std::vector<entity_id> tiles = raster_index(w, subject, rec->gw, rec->gh);
    std::array<int, 12> comp_counts{};
    std::array<int, 7>  land_counts{};
    int counted = 0;
    for (entity_id id : tiles)
    {
        if (id == null_entity) continue;
        const tile_component& t = w.tiles.at(id);
        const std::size_t c = static_cast<std::size_t>(t.composition);
        const std::size_t l = static_cast<std::size_t>(t.landform);
        if (c < comp_counts.size()) ++comp_counts[c];
        if (l < land_counts.size()) ++land_counts[l];
        ++counted;
    }

    ImGui::TextDisabled("COMPOSITION  (%d tiles)", counted);
    for (std::size_t c = 0; c < comp_counts.size(); ++c)
        histogram_row(composition_name(static_cast<terrain_composition>(c)),
                      comp_counts[c], counted);
    ImGui::Spacing();

    ImGui::TextDisabled("LANDFORM");
    for (std::size_t l = 0; l < land_counts.size(); ++l)
        histogram_row(landform_name(static_cast<terrain_landform>(l)),
                      land_counts[l], counted);

    ui::foldout_end();
}

} // namespace ui
