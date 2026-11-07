#include "military_ledger.hpp"

#include "construction_sites.hpp" // site_options — the no-base door onto the site finder
#include "detail_level.hpp"       // fold — closing the ledger closes the designer
#include "foldout_column.hpp"
#include "hover_card.hpp"         // freezable_tooltip: a row's card freezes for its cells
#include "format.hpp"             // fmt::credits
#include "nav_pane.hpp"           // close_all_panels — the no-base door takes the column
#include "presentation.hpp"
#include "unit_designer.hpp"      // the Design tab's class rows open it
#include "wing_card.hpp"          // BL-1450: air class / mission names
#include "ship_card.hpp" // BL-1534: the Sea tab's labels and rows
#include "stat_grid.hpp" // a row's name opens its card on hover
#include "world/corp_command.hpp" // read_*_hire_goods: the Raise's goods leg
#include "world/sea_combat.hpp"

#include "world/air_combat.hpp"   // BL-1450: the air roster, hire_wing_check
#include "world/recruits.hpp"     // BL-1463: the recruit pools a Raise spends
#include "world/unit_designs.hpp" // design_stats — a class row shows its live design
#include "world/unit_roster.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
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

/// A goods quantity as the shelves count it: whole where it is whole.
std::string goods_amount(float v)
{
    char b[32];
    if (std::fabs(v - std::round(v)) < 0.05f) std::snprintf(b, sizeof b, "%.0f", v);
    else                                       std::snprintf(b, sizeof b, "%.1f", v);
    return b;
}

/// Why a hire's goods leg refuses, naming what is short: "Short of steel (0 of 6)
/// and machinery (0 of 2)." The seam's quantity gate is `g.goods_ok`.
std::string goods_short_text(const hire_goods_reading& g)
{
    std::vector<std::string> parts;
    for (const hire_goods_line& l : g.lines)
        if (l.have < l.need)
            parts.push_back(std::string(resource_name(l.res)) + " (" + goods_amount(l.have) + " of "
                            + goods_amount(l.need) + ")");
    if (parts.empty())
        return "The shelves cannot supply its goods.";
    std::string out = "Short of ";
    for (std::size_t i = 0; i < parts.size(); ++i)
    {
        if (i > 0) out += i + 1 == parts.size() ? " and " : ", ";
        out += parts[i];
    }
    return out + ".";
}

/// The credit and goods legs a Raise checks, after its placement and recruit checks:
/// the seam's own order (`hire_unit` / `hire_wing` / `hire_ship`), so the row cannot
/// disagree with the press. Empty when both pass.
std::string payment_reason(float price, float room, const hire_goods_reading& g)
{
    if (price > room)
        return "The treasury cannot cover it.";
    if (!g.goods_ok)
        return goods_short_text(g);
    if (price + g.price_bound > room)
        return "The treasury cannot cover it and its goods.";
    return {};
}

/// The Raise button's tooltip: why it is off, or why its last press was refused (@p why),
/// then what the hire takes against what the nation holds - credits, then each good,
/// a short one in the loss colour.
void raise_tooltip(const std::string& why, float price, float room, const hire_goods_reading& g)
{
    ImGui::BeginTooltip();
    if (!why.empty())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 22.0f);
        ImGui::TextUnformatted(why.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Separator();
    }
    if (ImGui::BeginTable("##raise_takes", 3, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Takes");
        ImGui::TableSetupColumn("Needs");
        ImGui::TableSetupColumn("Held");
        ImGui::TableHeadersRow();
        const auto row = [](const char* name, const std::string& need, const std::string& have, bool short_) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(name);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(need.c_str());
            ImGui::TableSetColumnIndex(2);
            if (short_) ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative), "%s", have.c_str());
            else        ImGui::TextUnformatted(have.c_str());
        };
        row("Credits", fmt::credits(price), fmt::credits(room), price > room);
        for (const hire_goods_line& l : g.lines)
            row(resource_name(l.res), goods_amount(l.need), goods_amount(l.have), l.have < l.need);
        ImGui::EndTable();
    }
    ImGui::EndTooltip();
}

/// The frame's dwell step for a freezable tooltip: the fixed verify step under `--verify`,
/// else the frame's time, capped so a stall cannot freeze a card at once (hover_card.hpp).
float dwell_step(const ui_state& s)
{
    return s.fixed_frame_clock ? kVerifyFrameSeconds
                               : std::min(ImGui::GetIO().DeltaTime, kHoverMaxFrameSeconds);
}

/// A row's card, on hovering its name: the type, what it is, and the stat grid the unit
/// card and the designer draw (stat_grid.hpp), so a row reads as the thing it raises.
/// A freezable tooltip: 2 s on, it freezes and each grid cell names its stat on hover.
/// @p door, when named, is a last dim line saying what the press does.
void row_card_tooltip(const ui_state& s, const char* grid_id, const char* name, const std::string& what,
                      const stat_bands& bands, const char* door = nullptr)
{
    freezable_tooltip("##row_card", dwell_step(s), [&]() {
        ImGui::TextUnformatted(name);
        ImGui::TextDisabled("%s", what.c_str());
        ImGui::Spacing();
        draw_stat_grid(grid_id, bands, ImGui::GetFontSize() * 20.0f);
        if (door != nullptr)
        {
            ImGui::Spacing();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 20.0f);
            ImGui::TextDisabled("%s", door);
            ImGui::PopTextWrapPos();
        }
    });
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

/// The gate axes an AIR row lacks, in the shelf's words (the land rows' binary gate).
std::string missing_air_gate(const roster_gate& g, const campaign_roster_gate_input& in)
{
    roster_row probe{};
    probe.gate = g;
    return missing_gate(probe, in);
}

/// The theatre tabs under Recruit / Design (military.md § Grouped by theatre; Ben,
/// 2026-11-02): Land, Air, Sea, each its name and count. A theatre tab switches what
/// the view lists, so it is a SELECTOR, not a toggle: `nav_button` without a close
/// target leaves a re-press of the active tab a no-op (the Recruit / Design tabs keep
/// the toggle rule). @p tab is 0 Recruit, 1 Design.
void theatre_tabs(military_ledger_state& m, int tab, const int (&counts)[3])
{
    static const char* const k_names[3] = {"Land", "Air", "Sea"};
    if (m.theatre[tab] < 0 || m.theatre[tab] > 2)
        m.theatre[tab] = 0;
    for (int t = 0; t < 3; ++t)
    {
        if (t > 0)
            ImGui::SameLine();
        char label[64];
        std::snprintf(label, sizeof label, "%s  %d###theatre_%d_%d", k_names[t], counts[t], tab, t);
        ui::nav_button(label, t, m.theatre[tab]); // no close target: a selector
        m.theatre_tabs.push_back({tab, t, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                                  (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f,
                                  m.theatre[tab] == t, counts[t]});
    }
    ImGui::Spacing();
}

/// The nation's completed ports, ascending id: the Sea tab's picker.
std::vector<entity_id> player_ports(const world& w, entity_id actor)
{
    std::vector<entity_id> out;
    for (const auto& [bid, bc] : w.buildings)
        if (bc.type == building_type::port && bc.ticks_remaining <= 0 && !bc.decommissioned
            && owner_holds(w, actor, bid))
            out.push_back(bid);
    std::sort(out.begin(), out.end());
    return out;
}

/// An air design's numbers: the class's plus its live kit's, as the air designer totals them.
air_combat_stats air_design_totals(const recipe_registry& reg, air_class c, const unit_design& d)
{
    const air_class_stats& cs = reg.military().air.cls[static_cast<std::size_t>(c)];
    const air_combat_stats k  = air_kit_delta(c, d);
    air_combat_stats t;
    t.hp = cs.hp + k.hp;  t.air = cs.air + k.air;  t.soft = cs.soft + k.soft;  t.hard = cs.hard + k.hard;
    t.strategic = cs.strategic + k.strategic;  t.recon = cs.recon + k.recon;  t.range = cs.range + k.range;
    return t;
}

/// The missions class @p c flies, in words: "Tactical bombing, Recon".
std::string missions_of(const recipe_registry& reg, air_class c)
{
    std::string out;
    for (int mi = 1; mi < air_mission_count; ++mi)
        if (air_class_flies(reg, c, static_cast<air_mission>(mi)))
        {
            if (!out.empty()) out += ", ";
            out += air_mission_label(static_cast<air_mission>(mi));
        }
    return out;
}

/// The "Find a site for ..." door: the Construction ledger's site finder pre-set
/// to @p type — the same two fields its picker writes.
void open_site_finder_for(const world& w, const recipe_registry& reg, ui_state& s, building_type type)
{
    close_all_panels(s);
    s.show_construction_panel = true;
    s.construction.panel_view = 0; // Construction: the queue, the finder, the build bar
    for (const site_option& o : site_options(w, reg))
        if (o.type == type)
        {
            s.site_finder.armed  = true;
            s.site_finder.choice = o;
            break;
        }
}

struct base_row
{
    entity_id id   = null_entity;
    entity_id tile = null_entity;
    int       units = 0;
};

void draw_recruit_air(const world& w, const recipe_registry& reg, ui_state& s);
void draw_recruit_sea(const world& w, const recipe_registry& reg, ui_state& s);
void draw_design_sea(const world& w, const recipe_registry& reg, ui_state& s);

void draw_recruit(const world& w, const recipe_registry& reg, ui_state& s)
{
    military_ledger_state& m = s.military;
    const entity_id actor = player_actor(w);
    const auto mine = [&](entity_id owner) { return owner == actor || owner == w.player_entity; };

    // The hire seam's RESULT for the last Raise press (app writes it), above both
    // theatres: the press that asked may sit in either. A REFUSAL is not printed
    // here (Ben, 2026-10-30): it is the pressed row's Raise tooltip.
    if (m.raise_pressed && !m.raise_refused && !s.construction.last_message.empty())
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s",
                           s.construction.last_message.c_str());
    if (m.raise_pressed && !m.air_raise_refused && !m.air_message.empty())
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", m.air_message.c_str());
    const float room = purse_room_of(w, actor);
    ImGui::TextDisabled("Treasury room %s", fmt::credits(room).c_str());

    // ── LAND ────────────────────────────────────────────────────────────────
    const std::vector<roster_row>& table = unit_roster_table();
    const roster_band band = campaign_roster_band_for(reg.era());
    std::vector<std::uint16_t> order;
    for (std::size_t i = 0; i < table.size(); ++i)
        if (static_cast<int>(table[i].band) <= static_cast<int>(band))
            order.push_back(static_cast<std::uint16_t>(i));
    int land_rows = 0;
    for (const std::uint16_t idx : order)
        if (m.show_obsolete || !roster_row_obsolete(w, actor, idx))
            ++land_rows;
    {
        const int counts[3] = {land_rows, static_cast<int>(air_roster_table().size()),
                               static_cast<int>(ship_roster_table().size())};
        theatre_tabs(m, 0, counts);
    }
    if (m.theatre[0] == 1)
    {
        draw_recruit_air(w, reg, s);
        return;
    }
    if (m.theatre[0] == 2)
    {
        draw_recruit_sea(w, reg, s);
        return;
    }

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
        // The explanation is the heading's and the press's tooltip, not ledger text
        // (Ben, 2026-10-30): the ledger shows controls and data only.
        static const char* const k_no_base_why =
            "Every unit is raised at a base. Build one on your own ground first.";
        ImGui::TextUnformatted("No military base");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", k_no_base_why);
        ImGui::Spacing();
        if (ImGui::Button("Find a site for a base"))
            // The Construction ledger's site finder, pre-set to the military base
            // (military.md § The empty states).
            open_site_finder_for(w, reg, s, building_type::military_base);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", k_no_base_why);
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

    // ── Roster ──────────────────────────────────────────────────────────────
    const auto avail = available_rows(w, actor, band);
    const campaign_roster_gate_input gate_in = campaign_gate_input(w, actor);
    const military_capability_params& mil = reg.military();

    // Newest band first: the rows a nation raises now lead, and the older rungs
    // follow below them, so the list still shows the whole ladder (military.md § 2).
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
        else if (recruit_pool(w, actor, recruit_theatre::land) < hire_unit_recruits(idx))
            // BL-1463 (MILITARY.md § Recruits): the seam's own refusal, in its words.
            reason = recruit_shortfall_text(hire_unit_recruits(idx),
                                            recruit_pool(w, actor, recruit_theatre::land)) + ".";
        const hire_goods_reading goods = read_land_hire_goods(w, actor, idx);
        if (reason.empty())
            reason = payment_reason(price, room, goods);
        const bool enabled = reason.empty();

        // One line: the name (its card on hover), then price and Raise at the right edge.
        const float line_x = ImGui::GetCursorPosX();
        const float avail_w = ImGui::GetContentRegionAvail().x;
        if (fieldable && !obsolete) ImGui::TextUnformatted(row.name);
        else if (obsolete)          ImGui::TextDisabled("%s (obsolete)", row.name);
        else                        ImGui::TextDisabled("%s", row.name);
        const ImVec2 name_c = {(ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                               (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f};
        {
            // The unit this row raises: its type with its class's live design kit.
            unit_component probe{};
            probe.type = idx;
            probe.kit  = design_of(w, actor, land_class_for(row));
            const land_combat_stats st = unit_land_stats(probe);
            row_card_tooltip(s, "raise_card", row.name, land_class_label(land_class_for(row)),
                             land_design_stat_bands(reg, land_class_for(row), st, st));
        }
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
            m.raise_row     = static_cast<int>(idx);
            m.raise_refused = false;
            m.air_raise_row = -1;
        }
        ImGui::EndDisabled();
        // Why Raise is off, or why the seam refused this row's last press, is the
        // button's tooltip - never ledger text (Ben, 2026-10-30).
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            std::string why = reason;
            if (enabled && m.raise_pressed && m.raise_refused && m.raise_row == static_cast<int>(idx))
                why = s.construction.last_message;
            raise_tooltip(why, price, room, goods);
        }
        m.raises.push_back({idx, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                            (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f,
                            fieldable, enabled, price, reason, name_c.x, name_c.y});
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

/// Recruit's AIR theatre (military.md § Grouped by theatre; MILITARY.md § Air): the
/// airbase picker, one row per air class raising a wing through `hire_wing`, and
/// the player's wings, each a press that selects it.
void draw_recruit_air(const world& w, const recipe_registry& reg, ui_state& s)
{
    military_ledger_state& m = s.military;
    const entity_id actor = player_actor(w);
    const std::vector<air_roster_row>& table = air_roster_table();

    // ── Airbases ── the one door a wing is hired at (a military base hosts none).
    std::vector<base_row> bases;
    for (const auto& [bid, bc] : w.buildings)
        if (bc.type == building_type::airbase && bc.ticks_remaining <= 0
            && !bc.decommissioned && owner_holds(w, actor, bid))
            bases.push_back({bid, bc.tile, 0});
    std::sort(bases.begin(), bases.end(),
              [](const base_row& a, const base_row& b) { return a.id < b.id; });
    for (base_row& b : bases)
        for (const auto& [wid, wg] : w.wings)
            if (wg.airbase == b.id)
                ++b.units;

    if (bases.empty())
    {
        m.air_base = null_entity;
        static const char* const k_no_airbase_why =
            "Every wing is raised at an airbase. A military base hosts none.";
        ImGui::TextUnformatted("No airbase");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", k_no_airbase_why);
        if (ImGui::Button("Find a site for an airbase"))
            open_site_finder_for(w, reg, s, building_type::airbase);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", k_no_airbase_why);
        ImGui::Separator();
    }
    else
    {
        if (std::none_of(bases.begin(), bases.end(),
                         [&](const base_row& b) { return b.id == m.air_base; }))
            m.air_base = bases.front().id;
        ImGui::TextDisabled("Raise at");
        for (const base_row& b : bases)
        {
            ImGui::PushID(static_cast<int>(b.id) + 0x30000);
            char label[160];
            std::snprintf(label, sizeof label, "%s  -  %d wing%s based", place_of(w, b.tile).c_str(),
                          b.units, b.units == 1 ? "" : "s");
            // A target selector, exempt from the toggle rule, as the land picker is.
            if (ImGui::Selectable(label, m.air_base == b.id))
                m.air_base = b.id;
            m.air_bases.push_back({b.id, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                                   (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f, b.units});
            ImGui::PopID();
        }
        ImGui::Separator();
    }
    entity_id base_tile = null_entity;
    for (const base_row& b : bases)
        if (b.id == m.air_base)
            base_tile = b.tile;

    // ── Air roster ──
    const float room  = purse_room_of(w, actor);
    const auto  avail = available_air_rows(w, actor);
    const campaign_roster_gate_input gate_in = campaign_gate_input(w, actor);
    const entity_id nation = player_nation(w);
    const float btn_w = ImGui::CalcTextSize("Raise").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    for (std::size_t i = 0; i < table.size(); ++i)
    {
        const air_roster_row&  row = table[i];
        const std::uint16_t    idx = static_cast<std::uint16_t>(i);
        const air_class_stats& cs  = reg.military().air.cls[static_cast<std::size_t>(row.cls)];
        const bool  fieldable = std::find(avail.begin(), avail.end(), idx) != avail.end();
        const float price     = cs.hire_cost;
        ImGui::PushID(static_cast<int>(idx) + 0x40000);

        // The seam's own placement and fielding checks (`hire_wing_check`), then the
        // credit leg; the goods leg is the seam's answer on the press.
        std::string reason;
        if (!fieldable)
        {
            reason = missing_air_gate(row.gate, gate_in);
            if (reason.empty())
                reason = "Not fieldable now.";
        }
        else if (base_tile == null_entity)
            reason = "No airbase to raise it at.";
        else if (const corp_command_result hc = hire_wing_check(w, reg, actor, base_tile, idx);
                 hc == corp_command_result::rejected_no_recruits)
            // BL-1463 (MILITARY.md § Recruits): the crew the air pool cannot fill.
            reason = recruit_shortfall_text(wing_crew(reg, idx),
                                            recruit_pool(w, actor, recruit_theatre::air)) + ".";
        else if (hc != corp_command_result::applied)
            reason = "Cannot be raised at this airbase.";
        const hire_goods_reading goods = read_air_hire_goods(w, reg, actor, idx);
        if (reason.empty())
            reason = payment_reason(price, room, goods);
        const bool enabled = reason.empty();

        const float line_x  = ImGui::GetCursorPosX();
        const float avail_w = ImGui::GetContentRegionAvail().x;
        if (fieldable) ImGui::TextUnformatted(row.name);
        else           ImGui::TextDisabled("%s", row.name);
        const ImVec2 name_c = {(ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                               (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f};
        {
            const air_combat_stats st = air_design_totals(reg, row.cls, air_design_of(w, nation, row.cls));
            row_card_tooltip(s, "raise_card", row.name,
                             std::string(air_class_label(row.cls)) + " - " + missions_of(reg, row.cls),
                             air_design_stat_bands(st, st));
        }
        const std::string price_s = fmt::credits(price);
        const float price_w = ImGui::CalcTextSize(price_s.c_str()).x;
        const float gap_x   = ImGui::GetStyle().ItemSpacing.x;
        // A long name ("Airborne Early Warning Wing") would run under the price at
        // the column's narrow width: the price then moves to the class line below.
        const bool price_inline =
            ImGui::CalcTextSize(row.name).x + 2.0f * gap_x + price_w + btn_w <= avail_w;
        if (price_inline)
        {
            ImGui::SameLine(line_x + avail_w - btn_w - price_w - gap_x);
            if (price <= room) ImGui::TextUnformatted(price_s.c_str());
            else               ImGui::TextDisabled("%s", price_s.c_str());
        }
        ImGui::SameLine(line_x + avail_w - btn_w);
        ImGui::BeginDisabled(!enabled);
        if (ImGui::Button("Raise", {btn_w, 0.0f}) && enabled)
        {
            // A door onto the hire verb: app::render sends `hire_wing`.
            m.pending_wing_tile = base_tile;
            m.pending_wing_row  = idx;
            m.air_message.clear();
            s.construction.last_message.clear();
            m.raise_pressed     = true;
            m.air_raise_row     = static_cast<int>(idx);
            m.air_raise_refused = false;
            m.raise_row         = -1;
        }
        ImGui::EndDisabled();
        // As the land rows: a refusal is the Raise tooltip, never ledger text.
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            std::string why = reason;
            if (enabled && m.raise_pressed && m.air_raise_refused && m.air_raise_row == static_cast<int>(idx))
                why = m.air_message;
            raise_tooltip(why, price, room, goods);
        }
        m.air_raises.push_back({idx, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                                (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f,
                                fieldable, enabled, price, reason, name_c.x, name_c.y});

        // A long name pushes the price under it; the numbers are the name's card.
        if (!price_inline)
            ImGui::TextDisabled("%s", price_s.c_str());
        ImGui::Spacing();
        ImGui::PopID();
    }

    // ── Your wings ── each a press that selects it (its card sets the mission).
    ImGui::Separator();
    ImGui::TextDisabled("Your wings");
    int shown = 0;
    for (const auto& [wid, wg] : w.wings)
    {
        if (!(wg.owner == actor || wg.owner == w.player_entity))
            continue;
        ++shown;
        ImGui::PushID(static_cast<int>(wid) + 0x50000);
        char label[200];
        const entity_id tile = wing_base_tile(w, wg);
        if (wg.muster_days > 0)
            std::snprintf(label, sizeof label, "%s  -  mustering, %d d left", wing_display_name(w, wid),
                          wg.muster_days);
        else if (wg.mission == air_mission::none)
            std::snprintf(label, sizeof label, "%s  -  no mission", wing_display_name(w, wid));
        else
            std::snprintf(label, sizeof label, "%s  -  %s over %d province%s", wing_display_name(w, wid),
                          air_mission_label(wg.mission), static_cast<int>(wg.provinces.size()),
                          wg.provinces.size() == 1 ? "" : "s");
        if (ImGui::Selectable(label, s.selected_entity == wid))
        {
            s.clear_battle_selection();
            s.clear_front_selection();
            s.clear_lens_region_selection();
            s.selected_entity      = wid;
            s.selection_unit_group = false;
            s.selected_province    = 0;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select this wing (based at %s)",
                              tile != null_entity ? place_of(w, tile).c_str() : "a lost airbase");
        m.wing_rows.push_back({wid, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                               (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f});
        ImGui::PopID();
    }
    if (shown == 0)
        ImGui::TextDisabled("No wings yet.");
}

/// A ship design's card bands: the class's numbers plus its live kit's, as the ship
/// designer totals them.
stat_bands ship_row_bands(const recipe_registry& reg, ship_class c, const unit_design& d)
{
    sea_ship probe;
    probe.type = static_cast<std::uint16_t>(c); // the sea rows are one per class, in class order
    probe.kit  = d;
    const ship_live_stats st = ship_stats(reg, probe);
    return ship_design_stat_bands(st, st, reg.military().sea.cls[static_cast<std::size_t>(c)].range);
}

/// What class @p c does, in words: "Patrol, Escort, Raid, Blockade"; a transport carries.
std::string sea_missions_of(const recipe_registry& reg, ship_class c)
{
    std::string out;
    for (int mi = 1; mi < sea_mission_count; ++mi)
        if (ship_class_works(reg, c, static_cast<sea_mission>(mi)))
        {
            if (!out.empty()) out += ", ";
            out += sea_mission_label(static_cast<sea_mission>(mi));
        }
    return out.empty() ? std::string("Carries land units") : out;
}

/// The SEA theatre of Recruit (military.md § Grouped by theatre): the nation's ports under a
/// picker, the four ship rows raising at the chosen one, and its ships, each a press that
/// selects it. As the Air theatre, with the sea pool in the header.
void draw_recruit_sea(const world& w, const recipe_registry& reg, ui_state& s)
{
    military_ledger_state& m = s.military;
    const entity_id actor = player_actor(w);
    const std::vector<ship_roster_row>& table = ship_roster_table();

    if (m.raise_pressed && m.raise_row < 0 && m.air_raise_row < 0 && !m.sea_raise_refused
        && !m.sea_message.empty())
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", m.sea_message.c_str());

    // ── Ports ──
    const std::vector<entity_id> ports = player_ports(w, actor);
    if (ports.empty())
    {
        m.sea_base = null_entity;
        ImGui::TextUnformatted("No port");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Every ship is raised at a port, and a port makes sea recruits.");
    }
    else
    {
        if (std::find(ports.begin(), ports.end(), m.sea_base) == ports.end())
            m.sea_base = ports.front();
        ImGui::TextDisabled("Ports");
        for (const entity_id pid : ports)
        {
            ImGui::PushID(static_cast<int>(pid) + 0x70000);
            const auto bit = w.buildings.find(pid);
            int homed = 0;
            for (const auto& [sid, sh] : w.ships)
                if (sh.home_port == pid)
                    ++homed;
            char label[200];
            std::snprintf(label, sizeof label, "%s  -  %d ship%s",
                          bit != w.buildings.end() ? place_of(w, bit->second.tile).c_str() : "Unknown ground",
                          homed, homed == 1 ? "" : "s");
            // A target selector, exempt from the toggle rule, as the base pickers are.
            if (ImGui::Selectable(label, m.sea_base == pid))
                m.sea_base = pid;
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Raise ships here. This port makes sea recruits every quarter.");
            m.ports.push_back({pid, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                               (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f, homed});
            ImGui::PopID();
        }
        ImGui::Separator();
    }
    entity_id port_tile = null_entity;
    if (const auto bit = w.buildings.find(m.sea_base); bit != w.buildings.end())
        port_tile = bit->second.tile;

    // ── The sea roster ──
    const float room  = purse_room_of(w, actor);
    const auto  avail = available_ship_rows(w, actor);
    const campaign_roster_gate_input gate_in = campaign_gate_input(w, actor);
    const entity_id nation = player_nation(w);
    const float btn_w = ImGui::CalcTextSize("Raise").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    for (std::size_t i = 0; i < table.size(); ++i)
    {
        const ship_roster_row&  row = table[i];
        const std::uint16_t     idx = static_cast<std::uint16_t>(i);
        const ship_class_stats& cs  = reg.military().sea.cls[static_cast<std::size_t>(row.cls)];
        const bool  fieldable = std::find(avail.begin(), avail.end(), idx) != avail.end();
        const float price     = cs.hire_cost;
        ImGui::PushID(static_cast<int>(idx) + 0x80000);

        // The seam's own checks (`hire_ship_check`), then the credit leg; the goods leg is
        // the seam's answer on the press.
        std::string reason;
        if (!fieldable)
        {
            reason = missing_air_gate(row.gate, gate_in);
            if (reason.empty())
                reason = "Not fieldable now.";
        }
        else if (port_tile == null_entity)
            reason = "No port to raise it at.";
        else if (const corp_command_result hc = hire_ship_check(w, reg, actor, port_tile, idx);
                 hc == corp_command_result::rejected_no_recruits)
            reason = recruit_shortfall_text(ship_crew(reg, idx), recruit_pool(w, actor, recruit_theatre::sea)) + ".";
        else if (hc == corp_command_result::rejected_placement)
            reason = "This port opens on no sea."; // a lake's shore: a mouth is sea water
        else if (hc != corp_command_result::applied)
            reason = "Cannot be raised at this port.";
        const hire_goods_reading goods = read_ship_hire_goods(w, reg, actor, idx);
        if (reason.empty())
            reason = payment_reason(price, room, goods);
        const bool enabled = reason.empty();

        const float line_x  = ImGui::GetCursorPosX();
        const float avail_w = ImGui::GetContentRegionAvail().x;
        if (fieldable) ImGui::TextUnformatted(row.name);
        else           ImGui::TextDisabled("%s", row.name);
        const ImVec2 name_c = {(ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                               (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f};
        row_card_tooltip(s, "raise_card", row.name,
                         std::string(ship_class_label(row.cls)) + " - " + sea_missions_of(reg, row.cls),
                         ship_row_bands(reg, row.cls, ship_design_of(w, nation, row.cls)));
        const std::string price_s = fmt::credits(price);
        const float price_w = ImGui::CalcTextSize(price_s.c_str()).x;
        const float gap_x   = ImGui::GetStyle().ItemSpacing.x;
        const bool price_inline = ImGui::CalcTextSize(row.name).x + 2.0f * gap_x + price_w + btn_w <= avail_w;
        if (price_inline)
        {
            ImGui::SameLine(line_x + avail_w - btn_w - price_w - gap_x);
            if (price <= room) ImGui::TextUnformatted(price_s.c_str());
            else               ImGui::TextDisabled("%s", price_s.c_str());
        }
        ImGui::SameLine(line_x + avail_w - btn_w);
        ImGui::BeginDisabled(!enabled);
        if (ImGui::Button("Raise", {btn_w, 0.0f}) && enabled)
        {
            m.pending_ship_tile = port_tile;
            m.pending_ship_row  = idx;
            m.sea_message.clear();
            s.construction.last_message.clear();
            m.air_message.clear();
            m.raise_pressed     = true;
            m.sea_raise_row     = static_cast<int>(idx);
            m.sea_raise_refused = false;
            m.raise_row         = -1;
            m.air_raise_row     = -1;
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            std::string why = reason;
            if (enabled && m.raise_pressed && m.sea_raise_refused && m.sea_raise_row == static_cast<int>(idx))
                why = m.sea_message;
            raise_tooltip(why, price, room, goods);
        }
        m.sea_raises.push_back({idx, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                                (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f,
                                fieldable, enabled, price, reason, name_c.x, name_c.y});

        // A long name pushes the price under it; the numbers are the name's card.
        if (!price_inline)
            ImGui::TextDisabled("%s", price_s.c_str());
        ImGui::Spacing();
        ImGui::PopID();
    }

    // ── Your ships ── each a press that selects it (its card gives its orders).
    ImGui::Separator();
    ImGui::TextDisabled("Your ships");
    int shown = 0;
    for (const auto& [sid, sh] : w.ships)
    {
        if (!(sh.owner == actor || sh.owner == w.player_entity))
            continue;
        ++shown;
        ImGui::PushID(static_cast<int>(sid) + 0x90000);
        char label[240];
        std::snprintf(label, sizeof label, "%s  -  %s", ship_display_name(w, sid), ship_status_text(w, reg, sid).c_str());
        if (ImGui::Selectable(label, s.selected_entity == sid))
        {
            s.clear_battle_selection();
            s.clear_front_selection();
            s.clear_lens_region_selection();
            s.selected_entity      = sid;
            s.selection_unit_group = false;
            s.selected_province    = 0;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Select this ship");
        m.ship_rows.push_back({sid, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                               (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f});
        ImGui::PopID();
    }
    if (shown == 0)
        ImGui::TextDisabled("No ships yet.");
}

/// The SEA theatre of Design: the four ship classes, each opening the designer on its own
/// slots (hull, armament, sensors; a transport has no armament).
void draw_design_sea(const world& w, const recipe_registry& reg, ui_state& s)
{
    military_ledger_state& m = s.military;
    const entity_id nation = player_nation(w);
    for (int c = 0; c < ship_class_count; ++c)
    {
        const ship_class sc = static_cast<ship_class>(c);
        ImGui::PushID(c + 0xA0000);
        if (ImGui::Selectable(ship_class_label(sc), ship_designer_open_on(s, sc)))
            toggle_ship_designer(s, sc);
        row_card_tooltip(s, "design_card", ship_class_label(sc), sea_missions_of(reg, sc),
                         ship_row_bands(reg, sc, ship_design_of(w, nation, sc)),
                         "Press to open the designer. Kit choices open as Naval research lands.");
        m.sea_class_rows.push_back({c, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                                    (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f});
        ImGui::PopID();
    }
}

void draw_design(const world& w, const recipe_registry& reg, ui_state& s)
{
    // The tab's intro copy is the class rows' tooltip and the Design tab's
    // (Ben, 2026-10-30): the ledger shows controls and data only.
    const entity_id nation = player_nation(w);
    military_ledger_state& m = s.military;
    {
        const int counts[3] = {land_class_count - 1, air_class_count, ship_class_count}; // every land class but `none`
        theatre_tabs(m, 1, counts);
    }
    if (m.theatre[1] == 2)
    {
        draw_design_sea(w, reg, s);
        return;
    }
    if (m.theatre[1] == 0)
    {
        for (int c = 0; c < land_class_count; ++c)
        {
            const land_class lc = static_cast<land_class>(c);
            if (lc == land_class::none)
                continue;
            // The designer's door (military.md § The designer). Lit while the designer
            // is open on this class, and a toggle: pressing the lit row closes it. Its
            // card - the class's plus its live kit's numbers (BL-1434) - is its hover.
            ImGui::PushID(c + 0x20000);
            if (ImGui::Selectable(land_class_label(lc), unit_designer_open_on(s, lc)))
                toggle_unit_designer(s, lc);
            {
                const land_combat_stats st = design_stats(w, nation, lc);
                row_card_tooltip(s, "design_card", land_class_label(lc), "Land class - its live design",
                                 land_design_stat_bands(reg, lc, st, st),
                                 "Press to open the designer. Kit choices open as military research lands.");
            }
            s.military.class_rows.push_back({c, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                                             (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f});
            ImGui::PopID();
        }

        // BL-1437: UNIT TYPES — which the nation still raises. A ticked type is
        // obsolete and hidden from Recruit; the defaults mark every type below the
        // modern band. The press queues `set_unit_obsolete`, which app executes.
        ImGui::Separator();
        static const char* const k_obsolete_why =
            "Tick a type to mark it obsolete: Recruit hides it and it is no longer raised.";
        ImGui::TextUnformatted("Unit types");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", k_obsolete_why);
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
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", k_obsolete_why);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", land_class_label(land_class_for(table[idx])));
            ImGui::PopID();
        }
        return;
    }

    // ── AIR (BL-1450): the six air classes beside the land ones, each opening the
    //    same full-canvas designer on its own slots (airframe, armament, avionics).
    {
        for (int c = 0; c < air_class_count; ++c)
        {
            const air_class ac = static_cast<air_class>(c);
            ImGui::PushID(c + 0x60000);
            if (ImGui::Selectable(air_class_label(ac), air_designer_open_on(s, ac)))
                toggle_air_designer(s, ac);
            {
                const air_combat_stats st = air_design_totals(reg, ac, air_design_of(w, nation, ac));
                row_card_tooltip(s, "design_card", air_class_label(ac), missions_of(reg, ac),
                                 air_design_stat_bands(st, st),
                                 "Press to open the designer. Kit choices open as military research lands.");
            }
            m.air_class_rows.push_back({c, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                                        (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f});
            ImGui::PopID();
        }
    }
}

/// The ledger's close closes the unit designer (Ben, 2026-10-30): on the open ->
/// closed edge, whatever path closed it (the rail toggle, the active tab's re-press,
/// another ledger opening), a designer that is the open takeover is folded. The fold
/// goes through the designer's close catch, so an unsaved draft still asks first.
/// Edge, not level: a designer opened from a unit card with the ledger shut stays.
void close_designer_on_ledger_close(ui_state& s, bool open)
{
    military_ledger_state& m = s.military;
    if (m.was_open && !open
        && (s.expanded.surface == detail_surface::unit_designer
            || s.expanded.surface == detail_surface::air_designer
            || s.expanded.surface == detail_surface::ship_designer))
        fold(s);
    m.was_open = open;
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
    m.theatre_tabs.clear();
    m.ports.clear();
    m.air_bases.clear();
    m.air_raises.clear();
    m.wing_rows.clear();
    m.air_class_rows.clear();
    m.sea_raises.clear();
    m.ship_rows.clear();
    m.sea_class_rows.clear();
    m.queue_rows = 0;

    if (!open)
    {
        m.raise_pressed = false;
        close_designer_on_ledger_close(s, open);
        return;
    }

    if (!ui::foldout_begin("Military"))
    {
        ui::foldout_end();
        close_designer_on_ledger_close(s, open);
        return;
    }

    // The tab strip; `nav_button` carries the toggle rule — re-clicking the active
    // tab closes the ledger, the other tab is an ordinary view change.
    ui::nav_button("Recruit", 0, m.view, &open);
    m.tab_x[0] = (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f;
    m.tab_y[0] = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
    ImGui::SameLine();
    ui::nav_button("Design", 1, m.view, &open);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Each class's design. Press a class to open its designer; kit choices open as "
                          "military research lands.");
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
    close_designer_on_ledger_close(s, open);
}

} // namespace ui
