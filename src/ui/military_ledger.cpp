#include "military_ledger.hpp"

#include "construction_sites.hpp" // site_options — the no-base door onto the site finder
#include "foldout_column.hpp"
#include "format.hpp"             // fmt::credits
#include "nav_pane.hpp"           // close_all_panels — the no-base door takes the column
#include "presentation.hpp"
#include "unit_designer.hpp"      // the Design tab's class rows open it

#include "world/unit_designs.hpp" // design_stats — a class row shows its live design
#include "world/unit_roster.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace ui {

namespace {

/// The land classes' display names, in `land_class` order (MILITARY.md § Land —
/// the classes). The seven fighting classes, then `none`.
const char* land_class_label(land_class c)
{
    static const char* const k_names[land_class_count] = {
        "Infantry", "Motorised infantry", "Mechanised infantry", "Armour",
        "Light armour", "Artillery", "Air defence", "Not a land fighter",
    };
    const int i = static_cast<int>(c);
    return (i >= 0 && i < land_class_count) ? k_names[i] : "Unknown";
}

/// One line of a design's numbers: soft, hard, hardness (as a share), defence.
void stats_line(char* out, std::size_t n, const land_combat_stats& st)
{
    std::snprintf(out, n, "soft %d  hard %d  hardness %d%%  defence %d",
                  st.soft, st.hard, (st.hardness + 5) / 10, st.defence);
}

/// Where a tile is, in the glossary's words: the population centre its province
/// holds (a province has no name of its own — GLOSSARY.md § Province), as the
/// site finder names a row; "Province N" where no centre stands in it.
std::string place_of(const world& w, entity_id tile)
{
    const std::uint32_t pid = w.provinces.province_of(tile);
    if (pid != 0)
    {
        // The lowest centre id in the province, so the name is one answer.
        entity_id best = null_entity;
        for (const auto& [cid, ctile] : w.population_centre_tile)
            if (w.provinces.province_of(ctile) == pid && (best == null_entity || cid < best))
                best = cid;
        if (best != null_entity)
            if (const auto nm = w.population_centre_name.find(best);
                nm != w.population_centre_name.end() && !nm->second.empty())
                return nm->second;
        return "Province " + std::to_string(pid);
    }
    if (const auto t = w.tiles.find(tile); t != w.tiles.end())
        return "Tile " + std::to_string(t->second.grid_x) + "," + std::to_string(t->second.grid_y);
    return "Unknown ground";
}

/// Why the nation cannot field @p r: the gate axes it lacks, in the shelf's words
/// (the hire-axis table's candidates; `port_q` is a building). Empty when every
/// gated axis is met.
std::string missing_gate(const roster_row& r, const campaign_roster_gate_input& in)
{
    std::vector<const char*> lacks;
    if (r.gate.ore_q    > in.ore_q)    lacks.push_back("steel or iron ore");
    if (r.gate.farm_q   > in.farm_q)   lacks.push_back("food");
    if (r.gate.energy_q > in.energy_q) lacks.push_back("coal or fuel");
    if (r.gate.port_q   > in.port_q)   lacks.push_back("a port");
    if (lacks.empty())
        return {};
    std::string s = "Needs ";
    for (std::size_t i = 0; i < lacks.size(); ++i)
    {
        if (i > 0)
            s += (i + 1 == lacks.size()) ? " and " : ", ";
        s += lacks[i];
    }
    return s + " on the shelf.";
}

struct base_row
{
    entity_id id   = null_entity;
    entity_id tile = null_entity;
    int       units = 0;
};

void draw_recruit(const world& w, const recipe_registry& reg, ui_state& s)
{
    military_ledger_state& m = s.military;
    const entity_id actor = player_actor(w);
    const auto mine = [&](entity_id owner) { return owner == actor || owner == w.player_entity; };

    // ── Bases ───────────────────────────────────────────────────────────────
    std::vector<base_row> bases;
    for (const auto& [bid, bc] : w.buildings)
        if (bc.type == building_type::military_base && bc.ticks_remaining <= 0
            && !bc.decommissioned && owner_holds(w, actor, bid))
            bases.push_back({bid, bc.tile, 0});
    std::sort(bases.begin(), bases.end(),
              [](const base_row& a, const base_row& b) { return a.id < b.id; });
    for (base_row& b : bases)
        for (const auto& [uid, u] : w.units)
            if (u.position == b.tile && mine(u.owner))
                ++b.units;

    if (bases.empty())
    {
        m.base = null_entity;
        ImGui::TextUnformatted("No military base");
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("Every unit is raised at a base. Build one on your own ground first.");
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        if (ImGui::Button("Find a site for a base"))
        {
            // The Construction ledger's site finder, pre-set to the military base
            // (military.md § The empty states) — the same two fields its picker writes.
            close_all_panels(s);
            s.show_construction_panel   = true;
            s.construction.panel_view   = 0; // Construction: the queue, the finder, the build bar
            for (const site_option& o : site_options(w, reg))
                if (o.type == building_type::military_base)
                {
                    s.site_finder.armed  = true;
                    s.site_finder.choice = o;
                    break;
                }
        }
        m.no_base_x = (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f;
        m.no_base_y = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
        ImGui::Separator();
    }
    else
    {
        if (std::none_of(bases.begin(), bases.end(),
                         [&](const base_row& b) { return b.id == m.base; }))
            m.base = bases.front().id;

        ImGui::TextDisabled("Raise at");
        for (const base_row& b : bases)
        {
            ImGui::PushID(static_cast<int>(b.id));
            char label[160];
            std::snprintf(label, sizeof label, "%s  -  %d unit%s standing", place_of(w, b.tile).c_str(),
                          b.units, b.units == 1 ? "" : "s");
            // A target selector (exempt from the toggle rule, as the body and market
            // combos are): pressing the chosen base keeps it chosen.
            if (ImGui::Selectable(label, m.base == b.id))
                m.base = b.id;
            m.bases.push_back({b.id, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                               (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f, b.units});
            ImGui::PopID();
        }
        ImGui::Separator();
    }

    entity_id base_tile = null_entity;
    for (const base_row& b : bases)
        if (b.id == m.base)
            base_tile = b.tile;

    // The hire seam's answer to the last Raise press (app writes it).
    if (m.raise_pressed && !s.construction.last_message.empty())
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s",
                           s.construction.last_message.c_str());
    }

    // ── Roster ──────────────────────────────────────────────────────────────
    const float room = purse_room_of(w, actor);
    ImGui::TextDisabled("Treasury room %s", fmt::credits(room).c_str());

    const std::vector<roster_row>& table = unit_roster_table();
    const roster_band band = campaign_roster_band_for(reg.era());
    const auto avail = available_rows(w, actor, band);
    const campaign_roster_gate_input gate_in = campaign_gate_input(w, actor);
    const military_capability_params& mil = reg.military();

    // Newest band first: the rows a nation raises now lead, and the older rungs
    // follow below them, so the list still shows the whole ladder (military.md § 2).
    std::vector<std::uint16_t> order;
    for (std::size_t i = 0; i < table.size(); ++i)
        if (static_cast<int>(table[i].band) <= static_cast<int>(band))
            order.push_back(static_cast<std::uint16_t>(i));
    std::stable_sort(order.begin(), order.end(), [&](std::uint16_t a, std::uint16_t b) {
        return static_cast<int>(table[a].band) > static_cast<int>(table[b].band);
    });

    // BL-1437: obsolete unit types are hidden unless the player asks to see them
    // (MILITARY.md § Land — the classes; by default every row below the modern band).
    int hidden = 0;
    for (const std::uint16_t idx : order)
        if (roster_row_obsolete(w, actor, idx))
            ++hidden;
    {
        char label[64];
        std::snprintf(label, sizeof label, "Show obsolete (%d)", hidden);
        ImGui::Checkbox(label, &m.show_obsolete);
    }

    const float btn_w = ImGui::CalcTextSize("Raise").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    for (const std::uint16_t idx : order)
    {
        const roster_row& row = table[idx];
        const bool obsolete = roster_row_obsolete(w, actor, idx);
        if (obsolete && !m.show_obsolete)
            continue;
        ImGui::PushID(static_cast<int>(idx));

        const bool fieldable = std::find(avail.begin(), avail.end(), &row) != avail.end();
        const float price = mil.hire_base_cost + mil.hire_cost_per_power * static_cast<float>(row.power_mod);

        std::string reason;
        if (!fieldable)
        {
            reason = missing_gate(row, gate_in);
            if (reason.empty())
                reason = "Not fieldable in this age.";
        }
        else if (base_tile == null_entity)
            reason = "No military base to raise it at.";
        else if (price > room)
            reason = "The treasury cannot cover it.";
        const bool enabled = reason.empty();

        // Line 1: name, then price and Raise at the right edge.
        const float line_x = ImGui::GetCursorPosX();
        const float avail_w = ImGui::GetContentRegionAvail().x;
        if (fieldable && !obsolete) ImGui::TextUnformatted(row.name);
        else if (obsolete)          ImGui::TextDisabled("%s (obsolete)", row.name);
        else                        ImGui::TextDisabled("%s", row.name);
        const std::string price_s = fmt::credits(price);
        const float price_w = ImGui::CalcTextSize(price_s.c_str()).x;
        ImGui::SameLine(line_x + avail_w - btn_w - price_w - ImGui::GetStyle().ItemSpacing.x);
        if (enabled && price <= room) ImGui::TextUnformatted(price_s.c_str());
        else                          ImGui::TextDisabled("%s", price_s.c_str());
        ImGui::SameLine(line_x + avail_w - btn_w);
        ImGui::BeginDisabled(!enabled);
        if (ImGui::Button("Raise", {btn_w, 0.0f}) && enabled)
        {
            // The ledger is a door onto the hire verb, never a second hire path:
            // app::render executes this request through `hire_unit`.
            s.construction.pending_hire_tile      = base_tile;
            s.construction.pending_hire_unit_type = idx;
            s.construction.last_message.clear();
            m.raise_pressed = true;
        }
        ImGui::EndDisabled();
        if (!enabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", reason.c_str());
        m.raises.push_back({idx, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                            (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f,
                            fieldable, enabled, price, reason});

        // Line 2: the land class and its numbers on a unit of this type.
        unit_component probe{};
        probe.type = idx;
        char stats[128];
        stats_line(stats, sizeof stats, unit_land_stats(probe));
        // Wrapped: the fold-out column's policy is WRAP (LAYOUT.md container 1),
        // and at its narrowest width the numbers do not fit one line.
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s   %s", land_class_label(land_class_for(row)), stats);

        // Line 3: why it is dimmed, or why Raise is off. Not repeated per row when
        // there is no base at all: the empty state above already says so once.
        if (!enabled && base_tile != null_entity)
            ImGui::TextDisabled("%s", reason.c_str());
        else if (!fieldable)
            ImGui::TextDisabled("%s", reason.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        ImGui::PopID();
    }

    // ── Muster queue ────────────────────────────────────────────────────────
    ImGui::Separator();
    ImGui::TextDisabled("Mustering");
    std::vector<const muster_order*> queue;
    for (const muster_order& o : w.musters)
        if (mine(o.owner))
            queue.push_back(&o);
    std::sort(queue.begin(), queue.end(),
              [](const muster_order* a, const muster_order* b) { return a->id < b->id; });
    if (queue.empty())
        ImGui::TextDisabled("Nothing mustering.");
    for (const muster_order* o : queue)
    {
        ImGui::PushID(static_cast<int>(o->id));
        const char* name = o->unit_type < table.size() ? table[o->unit_type].name : "Unknown unit";
        const auto bit = w.buildings.find(o->base);
        const std::string where = bit != w.buildings.end() ? place_of(w, bit->second.tile) : "a lost base";
        ImGui::Text("%s", name);
        ImGui::SameLine();
        ImGui::TextDisabled("at %s", where.c_str());
        const int   total = std::max(1, static_cast<int>(o->days_total));
        const int   left  = std::max(0, static_cast<int>(o->days_remaining));
        const float frac  = std::clamp(static_cast<float>(total - left) / static_cast<float>(total), 0.0f, 1.0f);
        char overlay[48];
        std::snprintf(overlay, sizeof overlay, "%d d left", left);
        ImGui::ProgressBar(frac, ImVec2{-1.0f, 0.0f}, overlay);
        ++m.queue_rows;
        ImGui::PopID();
    }
}

void draw_design(const world& w, const recipe_registry& reg, ui_state& s)
{
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Press a class to open its designer. Kit choices open as military research lands.");
    ImGui::PopTextWrapPos();
    ImGui::Separator();
    const entity_id nation = player_nation(w);
    for (int c = 0; c < land_class_count; ++c)
    {
        const land_class lc = static_cast<land_class>(c);
        if (lc == land_class::none)
            continue;
        // The design's numbers — the class's plus its live kit's (BL-1434).
        char stats[128];
        stats_line(stats, sizeof stats, design_stats(w, nation, lc));
        // The designer's door (military.md § The designer). Lit while the designer
        // is open on this class, and a toggle: pressing the lit row closes it.
        ImGui::PushID(c + 0x20000);
        if (ImGui::Selectable(land_class_label(lc), unit_designer_open_on(s, lc)))
            toggle_unit_designer(s, lc);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Open the designer for %s", land_class_label(lc));
        s.military.class_rows.push_back({c, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                                         (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f});
        ImGui::PopID();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", stats);
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
    }

    // BL-1437: UNIT TYPES — which the nation still raises. A ticked type is
    // obsolete and hidden from Recruit; the defaults mark every type below the
    // modern band. The press queues `set_unit_obsolete`, which app executes.
    ImGui::Separator();
    ImGui::TextUnformatted("Unit types");
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Tick a type to mark it obsolete: Recruit hides it and it is no longer raised.");
    ImGui::PopTextWrapPos();
    const entity_id actor = player_actor(w);
    const std::vector<roster_row>& table = unit_roster_table();
    const roster_band band = campaign_roster_band_for(reg.era());
    std::vector<std::uint16_t> order;
    for (std::size_t i = 0; i < table.size(); ++i)
        if (static_cast<int>(table[i].band) <= static_cast<int>(band))
            order.push_back(static_cast<std::uint16_t>(i));
    std::stable_sort(order.begin(), order.end(), [&](std::uint16_t a, std::uint16_t b) {
        return static_cast<int>(table[a].band) > static_cast<int>(table[b].band);
    });
    military_ledger_state& m = s.military;
    for (const std::uint16_t idx : order)
    {
        ImGui::PushID(static_cast<int>(idx) + 0x10000);
        bool obsolete = roster_row_obsolete(w, actor, idx);
        if (m.pending_obsolete_row == static_cast<int>(idx))
            obsolete = m.pending_obsolete_value; // shows the press before the tick applies it
        if (ImGui::Checkbox(table[idx].name, &obsolete))
        {
            m.pending_obsolete_row   = static_cast<int>(idx);
            m.pending_obsolete_value = obsolete;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", land_class_label(land_class_for(table[idx])));
        ImGui::PopID();
    }
}

} // namespace

void draw_military_ledger(const world& w, const recipe_registry& reg, ui_state& s, bool& open)
{
    military_ledger_state& m = s.military;
    m.tab_x[0] = m.tab_x[1] = m.tab_y[0] = m.tab_y[1] = -1.0f;
    m.no_base_x = m.no_base_y = -1.0f;
    m.bases.clear();
    m.raises.clear();
    m.class_rows.clear();
    m.queue_rows = 0;

    if (!open)
    {
        m.raise_pressed = false;
        return;
    }

    if (!ui::foldout_begin("Military"))
    {
        ui::foldout_end();
        return;
    }

    // The tab strip; `nav_button` carries the toggle rule — re-clicking the active
    // tab closes the ledger, the other tab is an ordinary view change.
    ui::nav_button("Recruit", 0, m.view, &open);
    m.tab_x[0] = (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f;
    m.tab_y[0] = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
    ImGui::SameLine();
    ui::nav_button("Design", 1, m.view, &open);
    m.tab_x[1] = (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f;
    m.tab_y[1] = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
    if (m.view < 0 || m.view > 1)
        m.view = 0;
    ImGui::Separator();
    ImGui::Spacing();

    if (open)
    {
        if (m.view == 0) draw_recruit(w, reg, s);
        else             draw_design(w, reg, s);
    }
    else
        m.raise_pressed = false;

    ui::foldout_end();
}

} // namespace ui
