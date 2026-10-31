#include "unit_stat_grid.hpp"

#include "player_side.hpp"
#include "presentation.hpp"

#include "world/economy_system.hpp" // march_days_per_tile — a unit's speed
#include "world/unit_roster.hpp"    // unit_land_stats

#include <algorithm>
#include <cstdio>

namespace ui {

namespace {

constexpr const char* k_dash = "\xe2\x80\x94";

std::string fmt_int(int v)
{
    char b[24];
    std::snprintf(b, sizeof b, "%d", v);
    return b;
}

std::string fmt_pct(int permille)
{
    char b[24];
    std::snprintf(b, sizeof b, "%d%%", (permille + 5) / 10);
    return b;
}

stat_cell stored(const char* label, int permille, ImU32 col)
{
    stat_cell c;
    c.label   = label;
    c.value   = fmt_pct(permille);
    c.bar     = std::clamp(permille / 1000.0f, 0.0f, 1.0f);
    c.bar_col = col;
    return c;
}

stat_cell hidden(const char* label, bool bar)
{
    stat_cell c;
    c.label = label;
    c.value = k_dash;
    if (bar)
    {
        c.bar     = 0.0f;
        c.bar_col = IM_COL32(70, 72, 80, 255);
    }
    return c;
}

} // namespace

void draw_stat_grid(const char* id, const std::vector<stat_cell>& cells, float width)
{
    if (cells.empty())
        return;
    const int cols = std::clamp(static_cast<int>(width / 118.0f), 2, 4);
    if (!ImGui::BeginTable(id, cols, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings,
                           {width, 0.0f}))
        return;
    for (const stat_cell& c : cells)
    {
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", c.label.c_str());
        ImGui::Text("%s", c.value.c_str());
        if (c.has_delta && c.delta != 0)
        {
            ImGui::SameLine(0.0f, 4.0f);
            // A signed delta: U+2212 MINUS for a fall, so "+12" and "−5" read alike.
            char d[24];
            if (c.delta > 0) std::snprintf(d, sizeof d, "+%d%s", c.delta, c.delta_pct ? "%" : "");
            else             std::snprintf(d, sizeof d, "\xe2\x88\x92%d%s", -c.delta, c.delta_pct ? "%" : "");
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(c.delta > 0 ? palette::positive : palette::negative),
                               "%s", d);
        }
        if (c.bar >= 0.0f)
        {
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::ColorConvertU32ToFloat4(c.bar_col));
            ImGui::ProgressBar(c.bar, ImVec2(-1.0f, 5.0f), "");
            ImGui::PopStyleColor();
        }
    }
    ImGui::EndTable();
}

std::vector<stat_cell> land_design_cells(const land_combat_stats& s, float march_days,
                                         const land_combat_stats* from)
{
    struct row { const char* label; int v; int f; bool pct; };
    const row rows[] = {
        {"Soft attack", s.soft, from ? from->soft : 0, false},
        {"Hard attack", s.hard, from ? from->hard : 0, false},
        {"Defence", s.defence, from ? from->defence : 0, false},
        {"Breakthrough", s.breakthrough, from ? from->breakthrough : 0, false},
        {"Hardness", s.hardness, from ? from->hardness : 0, true},
        {"Recon", s.recon, from ? from->recon : 0, false},
        {"Stealth", s.stealth, from ? from->stealth : 0, false},
    };
    std::vector<stat_cell> out;
    for (const row& r : rows)
    {
        stat_cell c;
        c.label = r.label;
        c.value = r.pct ? fmt_pct(r.v) : fmt_int(r.v);
        if (from != nullptr)
        {
            c.has_delta = true;
            c.delta_pct = r.pct;
            c.delta     = r.pct ? (r.v + 5) / 10 - (r.f + 5) / 10 : r.v - r.f;
        }
        out.push_back(c);
    }
    stat_cell sp;
    sp.label = "Speed";
    if (march_days > 0.0f)
    {
        char b[32];
        std::snprintf(b, sizeof b, "%g d/tile", static_cast<double>(march_days));
        sp.value = b;
    }
    else
        sp.value = k_dash;
    out.push_back(sp);
    return out;
}

bool rival_unit_seen(const world& w, entity_id unit)
{
    const auto uit = w.units.find(unit);
    if (uit == w.units.end())
        return false;
    if (is_player_side(w, uit->second.owner))
        return true;
    const entity_id me = player_nation(w);
    if (me == null_entity)
        return false;
    if (w.air_revealed.count({me, unit}) != 0)
        return true;
    for (const active_battle& b : w.battles)
    {
        const bool att = std::find(b.attacker_units.begin(), b.attacker_units.end(), unit) != b.attacker_units.end();
        const bool def = std::find(b.defender_units.begin(), b.defender_units.end(), unit) != b.defender_units.end();
        if ((att && is_player_side(w, b.defender)) || (def && is_player_side(w, b.attacker)))
            return true;
    }
    return false;
}

std::vector<stat_cell> unit_card_cells(const world& w, const recipe_registry& reg, const unit_component& u,
                                       entity_id unit_id)
{
    std::vector<stat_cell> out;
    const bool own = is_player_side(w, u.owner);
    if (own)
    {
        const int est = unit_establishment(u);
        stat_cell hp;
        hp.label = "HP";
        char b[48];
        std::snprintf(b, sizeof b, "%d / %d", u.count, est);
        hp.value   = b;
        hp.bar     = est > 0 ? std::clamp(static_cast<float>(u.count) / est, 0.0f, 1.0f) : 0.0f;
        hp.bar_col = palette::unit_hp;
        out.push_back(hp);
        out.push_back(stored("Readiness", u.readiness_permille, palette::unit_readiness));
        out.push_back(stored("Morale", u.morale_permille, palette::unit_morale));
        out.push_back(stored("Food", u.food_permille, palette::unit_food));
        out.push_back(stored("Ammunition", u.ammo_permille, palette::unit_ammo));
        out.push_back(stored("Equipment", u.equipment_permille, palette::unit_equipment));
    }
    else
    {
        // A rival's internals stay private (BL-068), seen or not.
        for (const char* l : {"HP", "Readiness", "Morale", "Food", "Ammunition", "Equipment"})
            out.push_back(hidden(l, true));
    }

    std::vector<stat_cell> design =
        land_design_cells(unit_land_stats(u), march_days_per_tile(reg.military(), u.type));
    if (!own && !rival_unit_seen(w, unit_id))
        for (stat_cell& c : design)
            c.value = k_dash; // unseen: design stats read "—" too
    out.insert(out.end(), design.begin(), design.end());
    return out;
}

} // namespace ui
