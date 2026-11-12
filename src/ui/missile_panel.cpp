#include "missile_panel.hpp"

#include "player_side.hpp"  // is_player_side
#include "presentation.hpp" // palette, building_type_name

#include "world/missiles.hpp"
#include "world/unit_designs.hpp" // unit_land_class

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace ui {

namespace {

const char* kind_label(missile_kind k)
{
    switch (k)
    {
        case missile_kind::short_range:      return "Short-range missiles";
        case missile_kind::long_range:       return "Long-range missiles";
        case missile_kind::tactical_nuclear: return "Tactical nuclear warheads";
        default:                             return "Strategic weapons";
    }
}

/// A province in the glossary's words: the population centre it holds, "Province N" where none.
std::string province_words(const world& w, uint32_t pid)
{
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

std::string target_words(const world& w, const missile_target& t)
{
    const std::string p = province_words(w, t.province);
    if (t.building == null_entity)
        return "Forces in " + p;
    const auto bit = w.buildings.find(t.building);
    const char* nm = bit != w.buildings.end() ? building_type_name(bit->second.type) : "Building";
    return std::string(nm) + " in " + p;
}

entity_id owner_of(const world& w, entity_id e)
{
    if (const auto it = w.units.find(e); it != w.units.end()) return it->second.owner;
    // BL-1643: a ship, wing or silo answers through the launcher's own owner rule.
    return missile_launcher_owner(w, e);
}

corp_command base_cmd(const world& w, corp_verb v)
{
    corp_command c;
    c.corp = w.player_entity;
    c.verb = v;
    return c;
}

} // namespace

std::string missile_order_answer(corp_verb verb, corp_command_result r)
{
    const bool ok = r == corp_command_result::applied;
    switch (verb)
    {
        case corp_verb::stock_missiles:
            if (ok) return "Missiles stocked.";
            if (r == corp_command_result::rejected_tech_locked) return "Warheads need Uranium Enrichment.";
            if (r == corp_command_result::rejected_state) return "The magazine is full.";
            return "Can't afford or supply the missiles.";
        case corp_verb::stock_interceptors:
            if (ok) return "Interceptors stocked.";
            if (r == corp_command_result::rejected_state) return "Its interceptor stock is full.";
            return "Can't afford or supply the interceptors.";
        case corp_verb::fire_missile:
            if (ok) return "Missiles away.";
            if (r == corp_command_result::rejected_placement) return "Out of range.";
            if (r == corp_command_result::rejected_funds) return "The magazine is short.";
            return "It can't strike that now.";
        default:
            return ok ? "Done." : "Refused.";
    }
}

bool draw_missile_section(const world& w, const recipe_registry& reg, ui_state& ui, entity_id e)
{
    const entity_id owner = owner_of(w, e);
    if (owner == null_entity || !is_player_side(w, owner))
        return false;
    missile_ui_state& m = ui.missile;
    const missile_params& P = reg.military().missile;

    bool carries_any = false;
    for (int ki = 0; ki < missile_fireable_count; ++ki)
        carries_any = carries_any || missile_launcher_carries(w, e, static_cast<missile_kind>(ki));
    const auto uit = w.units.find(e);
    const bool air_defence = uit != w.units.end() && unit_land_class(uit->second) == land_class::air_defence;
    if (!carries_any && !air_defence)
        return false;

    ImGui::PushID("missiles");
    ImGui::Separator();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Missiles");

    if (air_defence)
    {
        const auto sit = w.interceptor_stocks.find(e);
        const int have = sit == w.interceptor_stocks.end() ? 0 : sit->second;
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Interceptors: %d of %d", have, static_cast<int>(k_interceptor_cap));
        ImGui::SameLine();
        const int n = std::min(4, static_cast<int>(k_interceptor_cap) - have);
        ImGui::BeginDisabled(n <= 0);
        if (ImGui::SmallButton("Stock 4##interceptors"))
        {
            corp_command c = base_cmd(w, corp_verb::stock_interceptors);
            c.subject   = e;
            c.unit_type = static_cast<uint16_t>(std::max(1, n));
            m.pending.push_back(c);
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Interceptors thin a missile strike on this unit's province: short range well, "
                              "long range poorly. Each costs more than much of what it stops.");
    }

    for (int ki = 0; ki < missile_fireable_count; ++ki)
    {
        const missile_kind k = static_cast<missile_kind>(ki);
        if (!missile_launcher_carries(w, e, k))
            continue;
        ImGui::PushID(ki);
        const int32_t stock = missile_stock(w, owner, k);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s: %d", kind_label(k), static_cast<int>(stock));
        ImGui::SameLine();
        if (ImGui::SmallButton(k == missile_kind::tactical_nuclear ? "Buy 1" : "Buy 4"))
        {
            corp_command c = base_cmd(w, corp_verb::stock_missiles);
            c.order     = static_cast<uint32_t>(ki);
            c.unit_type = static_cast<uint16_t>(k == missile_kind::tactical_nuclear ? 1 : 4);
            m.pending.push_back(c);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Buy into the magazine; the goods come off your shelves at their prices.");

        // BL-1431 review: cached per (launcher, kind); refreshed as the day moves, every
        // k_missile_target_refresh_frames frames, or when app sends the section's orders.
        std::vector<missile_target> targets;
        {
            missile_ui_state::target_cache& tc = m.targets[{e, ki}];
            const int frame = ImGui::GetFrameCount();
            if (tc.day != w.current_day_tick || tc.frame < 0 || frame - tc.frame >= k_missile_target_refresh_frames
                || frame < tc.frame)
            {
                tc.targets.clear();
                for (const missile_target& t : missile_targets(w, reg, owner, e, k))
                    tc.targets.push_back({t.province, t.building});
                tc.day   = w.current_day_tick;
                tc.frame = frame;
            }
            for (const auto& [p, b] : tc.targets)
                targets.push_back({p, b});
        }
        // BL-1643: the reach the launcher fires at now (a silo's 90, a wing's flight added).
        int reach = -1;
        const bool can_fire = missile_launch_tile(w, reg, e, k, &reach) != null_entity;
        if (!can_fire && missile_launcher_of(w, e) == missile_launcher_type::silo)
            ImGui::TextDisabled("The silo cannot fire: it is unbuilt or at half condition or below (%d%%).",
                                P.silo_min_condition_permille / 10);
        else if (targets.empty())
            ImGui::TextDisabled("No sighted hostile target in reach (%d tiles).",
                                can_fire ? reach : P.kind[static_cast<std::size_t>(ki)].range);
        else
        {
            int& pick = m.target_pick[{e, ki}];
            pick = std::clamp(pick, 0, static_cast<int>(targets.size()) - 1);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
            if (ImGui::BeginCombo("##target", target_words(w, targets[static_cast<std::size_t>(pick)]).c_str()))
            {
                for (int i = 0; i < static_cast<int>(targets.size()); ++i)
                    if (ImGui::Selectable((target_words(w, targets[static_cast<std::size_t>(i)]) + "##t" + std::to_string(i)).c_str(),
                                          i == pick))
                        pick = i;
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(stock <= 0);
            const bool fire = ImGui::Button("Fire");
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip(k == missile_kind::tactical_nuclear
                                      ? "Fire one warhead. Every nation's Alarm toward you rises sharply, the target's most."
                                      : "Fire one missile from the magazine.");
            if (fire && stock > 0)
            {
                const missile_target& t = targets[static_cast<std::size_t>(pick)];
                corp_command c = base_cmd(w, corp_verb::fire_missile);
                c.subject      = e;
                c.order        = static_cast<uint32_t>(ki);
                c.unit_type    = 1;
                c.province     = t.province;
                c.counterparty = t.building;
                m.pending.push_back(c);
            }
        }
        ImGui::PopID();
    }
    if (!m.last_message.empty())
        ImGui::TextDisabled("%s", m.last_message.c_str());
    ImGui::PopID();
    return true;
}

} // namespace ui
