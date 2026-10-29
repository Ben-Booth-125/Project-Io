#include "wing_card.hpp"

#include "icons.hpp"
#include "player_side.hpp" // is_player_side
#include "presentation.hpp"

#include "world/air_combat.hpp"
#include "world/province.hpp" // province_holder_for
#include "world/stance.hpp"   // is_hostile — the strategic refusal names the province

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace ui {

namespace {

/// A province in the glossary's words: the population centre it holds (a province
/// has no name of its own — GLOSSARY.md § Province), "Province N" where none.
std::string province_label(const world& w, uint32_t pid)
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

std::string base_label(const world& w, const air_wing& wg)
{
    const entity_id tile = wing_base_tile(w, wg);
    if (tile == null_entity)
        return "a lost airbase";
    const uint32_t pid = w.provinces.province_of(tile);
    return pid != 0 ? province_label(w, pid) : std::string("an airbase");
}

/// Why `air_mission_check` refused (@p mission over @p provinces), in the player's words.
std::string mission_refusal(const world& w, const recipe_registry& reg, const air_wing& wg,
                            air_mission mission, const std::vector<uint32_t>& provinces,
                            corp_command_result r)
{
    switch (r)
    {
        case corp_command_result::applied: return {};
        case corp_command_result::rejected_placement:
        {
            const air_wing_stats st = wing_stats(reg, wg);
            const entity_id from = wing_base_tile(w, wg);
            for (const uint32_t p : provinces)
                if (!air_province_in_range(w, reg, wg, p))
                {
                    char buf[160];
                    std::snprintf(buf, sizeof buf, "Out of range: %s is %d tiles away, range %d",
                                  province_label(w, p).c_str(), air_province_distance(w, from, p), st.range);
                    return buf;
                }
            return "Out of range";
        }
        case corp_command_result::rejected_state:
            // Two causes share the code (air_mission_check): STRATEGIC bombing over a
            // province not held by a nation you are hostile to — tactical bombing may
            // fly anywhere, its hostility rule is on the target — and an assignment
            // that changes nothing.
            if (mission == air_mission::strategic)
                for (const uint32_t p : provinces)
                {
                    const entity_id holder = province_holder_for(w, p);
                    if (holder == null_entity || holder == wg.owner || !is_hostile(w, wg.owner, holder))
                        return "Strategic bombing only over a nation you are hostile to: "
                             + province_label(w, p) + " is not";
                }
            return "Nothing to change";
        case corp_command_result::rejected_not_owner: return "Not your wing";
        default:
            return std::string(air_class_label(wing_class(wg))) + " cannot fly " + air_mission_label(mission);
    }
}

} // namespace

const char* air_class_label(air_class c)
{
    static const char* const k[air_class_count] = {
        "Fighter", "Strike / multirole", "Strategic bomber",
        "Attack helicopter", "Strike drone", "Recon / AWACS",
    };
    const int i = static_cast<int>(c);
    return (i >= 0 && i < air_class_count) ? k[i] : "Unknown";
}

const char* air_mission_label(air_mission m)
{
    static const char* const k[air_mission_count] = {
        "None", "Air superiority", "Tactical bombing", "Strategic bombing", "Recon",
    };
    const int i = static_cast<int>(m);
    return (i >= 0 && i < air_mission_count) ? k[i] : "Unknown";
}

const char* wing_display_name(const world& w, entity_id wing)
{
    const auto it = w.wings.find(wing);
    if (it == w.wings.end())
        return "Wing";
    const auto& t = air_roster_table();
    return it->second.type < t.size() ? t[it->second.type].name : "Wing";
}

bool wing_is_players(const world& w, entity_id wing)
{
    const auto it = w.wings.find(wing);
    return it != w.wings.end() && is_player_side(w, it->second.owner);
}

void prune_wing_intents(const world& w, ui_state& s)
{
    // A held mission is the card's only while its wing exists and flies nothing:
    // once the wing flies, its real mission is the answer, and a stale intent must
    // never come back to override it after a stand-down elsewhere.
    for (auto it = s.wing.intent.begin(); it != s.wing.intent.end();)
    {
        const auto wit = w.wings.find(it->first);
        if (wit == w.wings.end() || wit->second.mission != air_mission::none)
            it = s.wing.intent.erase(it);
        else
            ++it;
    }
}

void reset_wing_ui(ui_state& s)
{
    s.wing.intent.clear();
    s.wing.pending_wing = null_entity;
    s.wing.pending_provinces.clear();
    s.wing.last_message.clear();
    s.military.pending_wing_tile = null_entity;
    s.military.air_message.clear();
}

air_mission wing_current_mission(const ui_state& s, const world& w, entity_id wing)
{
    const auto it = w.wings.find(wing);
    if (it == w.wings.end())
        return air_mission::none;
    if (it->second.mission != air_mission::none)
        return it->second.mission;
    if (const auto in = s.wing.intent.find(wing); in != s.wing.intent.end())
        return static_cast<air_mission>(in->second);
    return air_mission::none;
}

wing_assign_preview preview_wing_assign(const world& w, const recipe_registry& reg,
                                        const ui_state& s, entity_id wing, uint32_t province)
{
    wing_assign_preview p;
    const auto it = w.wings.find(wing);
    if (it == w.wings.end() || province == 0 || !is_player_side(w, it->second.owner))
        return p;
    const air_wing& wg = it->second;
    const std::string where = province_label(w, province);
    p.mission = wing_current_mission(s, w, wing);
    if (p.mission == air_mission::none)
    {
        p.kind = 2;
        p.text = "Choose the wing's mission on its card first";
        return p;
    }
    // On its current mission: the provinces it flies, plus this one. A wing that
    // flies none yet starts its list here.
    if (wg.mission == p.mission)
        p.provinces = wg.provinces;
    if (std::find(p.provinces.begin(), p.provinces.end(), province) != p.provinces.end())
    {
        p.kind = 2;
        p.text = where + " is already assigned";
        p.provinces.clear();
        return p;
    }
    if (p.provinces.size() >= k_air_mission_provinces_max)
    {
        p.kind = 2;
        p.text = "Eight provinces already assigned: remove one on the wing card";
        p.provinces.clear();
        return p;
    }
    p.provinces.push_back(province);
    const corp_command_result r = air_mission_check(w, reg, wg.owner, wing,
                                                    static_cast<uint32_t>(p.mission),
                                                    p.provinces.data(), p.provinces.size());
    if (r == corp_command_result::applied)
    {
        const air_wing_stats st = wing_stats(reg, wg);
        char buf[200];
        std::snprintf(buf, sizeof buf, "Right-click: assign %s (%s) - %d of %d tiles", where.c_str(),
                      air_mission_label(p.mission),
                      air_province_distance(w, wing_base_tile(w, wg), province), st.range);
        p.kind = 1;
        p.text = buf;
        return p;
    }
    p.kind = 2;
    p.text = mission_refusal(w, reg, wg, p.mission, p.provinces, r);
    p.provinces.clear();
    return p;
}

bool draw_wing_selection(const world& w, const recipe_registry& reg, ui_state& s)
{
    wing_ui_state& u = s.wing;
    u.missions.clear();
    u.removes.clear();
    u.drawn_wing = null_entity;
    const entity_id id = s.selected_entity;
    const auto it = w.wings.find(id);
    if (it == w.wings.end())
        return false;
    const air_wing&       wg   = it->second;
    const air_class       cls  = wing_class(wg);
    const air_wing_stats  st   = wing_stats(reg, wg);
    const bool            mine = is_player_side(w, wg.owner);
    const ImGuiStyle&     style = ImGui::GetStyle();
    ImDrawList*           dl    = ImGui::GetWindowDrawList();
    u.drawn_wing = id;
    prune_wing_intents(w, s);

    // ── Header: [aircraft] name · Wing ··· class, airbase ──
    {
        const float  fh = ImGui::GetFrameHeight();
        const ImVec2 hc = ImGui::GetCursorScreenPos();
        const float  ir = fh * 0.40f;
        icons::aircraft(dl, {hc.x + ir, hc.y + fh * 0.5f}, ir,
                        palette::corp_identity_colour(wg.owner, w.player_entity));
        ImGui::SetCursorScreenPos({hc.x + ir * 2.0f + style.ItemSpacing.x, hc.y});
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s",
                           wing_display_name(w, id));
        ImGui::SameLine();
        ImGui::TextDisabled("Wing");
        ImGui::SameLine();
        ImGui::TextDisabled("- %s, based at %s", air_class_label(cls), base_label(w, wg).c_str());
    }
    ImGui::Separator();

    const float total_w = ImGui::GetContentRegionAvail().x;
    const float col_w   = (total_w - 2.0f * style.ItemSpacing.x) / 3.0f;
    const ImGuiWindowFlags cf = ImGuiWindowFlags_NoSavedSettings;

    // A RIVAL's wing (BL-068): what it is and whose, nothing of its condition, its
    // mission or where it flies. The card stops at the owner line.
    if (!mine)
    {
        const char* who = owner_display_name(w, wg.owner);
        ImGui::TextDisabled("Owner: %s", who != nullptr ? who : "a rival");
        return true;
    }

    // ── Left: its condition ──
    ImGui::BeginChild("##wing_state", {col_w, 0.0f}, false, cf);
    ImGui::PushTextWrapPos(0.0f);
    {
        const float hp = static_cast<float>(wg.hp_ppm) / static_cast<float>(k_wing_hp_full);
        char ov[32];
        std::snprintf(ov, sizeof ov, "HP %d%%", static_cast<int>(hp * 100.0f + 0.5f));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImGui::ColorConvertU32ToFloat4(palette::unit_hp));
        ImGui::ProgressBar(std::clamp(hp, 0.0f, 1.0f), {-1.0f, 0.0f}, ov);
        ImGui::PopStyleColor();
        if (st.has_morale)
            ImGui::TextDisabled("Morale %d%%   Supply %d%%", (wg.morale_permille + 5) / 10,
                                (wg.supply_factor_permille + 5) / 10);
        else
            ImGui::TextDisabled("No pilot, no morale   Supply %d%%", (wg.supply_factor_permille + 5) / 10);
        if (wg.muster_days > 0)
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::unit_readiness),
                               "Mustering: %d days before it flies", wg.muster_days);
        ImGui::TextDisabled("Range %d tiles", st.range);
        ImGui::TextDisabled("Air %d  soft %d  hard %d  strategic %d  recon %d",
                            st.air, st.soft, st.hard, st.strategic, st.recon);
    }
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::SameLine();

    // ── Middle: the mission ──
    ImGui::BeginChild("##wing_mission", {col_w, 0.0f}, false, cf);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Mission");
    const air_mission cur = wing_current_mission(s, w, id);
    for (int mi = 0; mi < air_mission_count; ++mi)
    {
        const air_mission m = static_cast<air_mission>(mi);
        if (m != air_mission::none && !air_class_flies(reg, cls, m))
            continue; // only the missions its class flies
        const bool active = (m == cur);
        // What the press sends. An ACTIVE mission's press undoes it (the toggle
        // rule): back to None, which clears the provinces. A mission with no
        // province yet is held on the card until a right-click names one.
        const air_mission to = active ? air_mission::none : m;
        bool        enabled = mine;
        std::string reason  = mine ? std::string() : std::string("Not your wing");
        if (mine && to != air_mission::none && !wg.provinces.empty())
        {
            const corp_command_result r = air_mission_check(w, reg, wg.owner, id, static_cast<uint32_t>(to),
                                                            wg.provinces.data(), wg.provinces.size());
            if (r != corp_command_result::applied)
            {
                enabled = false;
                reason  = mission_refusal(w, reg, wg, to, wg.provinces, r);
            }
        }
        if (mine && m == air_mission::none && active)
            enabled = false; // None lit: nothing beneath it

        ImGui::PushID(mi);
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(palette::hover));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));
        }
        ImGui::BeginDisabled(!enabled);
        const bool pressed = ImGui::Button(air_mission_label(m), {-1.0f, 0.0f});
        ImGui::EndDisabled();
        if (active)
            ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            if (!reason.empty())
                ImGui::SetTooltip("%s", reason.c_str());
            else if (active && m != air_mission::none)
                ImGui::SetTooltip("Flying %s - press to stand the wing down", air_mission_label(m));
            else if (m != air_mission::none && wg.provinces.empty())
                ImGui::SetTooltip("%s - then right-click a province on the map to assign it",
                                  air_mission_label(m));
            else if (m != air_mission::none)
                ImGui::SetTooltip("Fly %s over the assigned provinces", air_mission_label(m));
        }
        u.missions.push_back({mi, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                              (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f,
                              enabled, active, reason});
        ImGui::PopID();
        if (pressed && enabled)
        {
            if (to == air_mission::none)
            {
                u.intent.erase(id);
                if (wg.mission != air_mission::none)
                {
                    u.pending_wing = id;
                    u.pending_mission = 0;
                    u.pending_provinces.clear();
                }
            }
            else if (wg.provinces.empty())
                u.intent[id] = static_cast<std::uint8_t>(to); // held until a province is named
            else
            {
                u.pending_wing      = id;
                u.pending_mission   = static_cast<std::uint8_t>(to);
                u.pending_provinces = wg.provinces;
            }
        }
    }
    if (mine)
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled(cur == air_mission::none ? "Choose a mission, then right-click provinces on the map."
                                                     : "Right-click a province on the map to assign it.");
        if (!u.last_message.empty())
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", u.last_message.c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ── Right: the provinces assigned ──
    ImGui::BeginChild("##wing_provinces", {0.0f, 0.0f}, false, cf);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Provinces (%d of %d)",
                       static_cast<int>(wg.provinces.size()), static_cast<int>(k_air_mission_provinces_max));
    if (wg.provinces.empty())
        ImGui::TextDisabled("None assigned.");
    const float xw = ImGui::GetFrameHeight();
    for (const uint32_t p : wg.provinces)
    {
        ImGui::PushID(static_cast<int>(p));
        const float x0 = ImGui::GetCursorPosX();
        ImGui::AlignTextToFramePadding();
        const entity_id holder = province_holder_for(w, p);
        const char* who = holder != null_entity ? owner_display_name(w, holder) : nullptr;
        ImGui::TextUnformatted(province_label(w, p).c_str());
        if (who != nullptr)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", who);
        }
        if (mine)
        {
            ImGui::SameLine(x0 + ImGui::GetContentRegionAvail().x - xw);
            if (ImGui::Button("x", {xw, xw}))
            {
                // Remove: the mission over the rest; the last one stands it down,
                // keeping the mission on the card for the next right-click.
                std::vector<uint32_t> rest;
                for (const uint32_t q : wg.provinces)
                    if (q != p)
                        rest.push_back(q);
                u.pending_wing = id;
                if (rest.empty())
                {
                    u.intent[id]      = static_cast<std::uint8_t>(wg.mission);
                    u.pending_mission = 0;
                }
                else
                    u.pending_mission = static_cast<std::uint8_t>(wg.mission);
                u.pending_provinces = std::move(rest);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Remove this province from the wing's mission");
            u.removes.push_back({p, (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
                                 (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f});
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    return true;
}

} // namespace ui
