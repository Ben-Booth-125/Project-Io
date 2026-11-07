#include "ship_card.hpp"

#include "icons.hpp"
#include "player_side.hpp" // is_player_side
#include "presentation.hpp"
#include "stat_grid.hpp"

#include "world/province.hpp"
#include "world/sea_combat.hpp"
#include "world/stance.hpp"
#include "world/unit_designs.hpp"
#include "world/unit_roster.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace ui {

namespace {

/// A land province in the glossary's words: the population centre it holds.
std::string land_label(const world& w, uint32_t pid)
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

/// A sea province has no centre to name it: its number is what the map's outline carries.
std::string sea_label(uint32_t pid)
{
    return "Sea province " + std::to_string(pid);
}

bool is_water_province(const world& w, uint32_t pid)
{
    const province* pr = w.provinces.find(pid);
    return pr != nullptr && province_kind_of(w, *pr) != province_kind::land;
}

std::string home_label(const world& w, const sea_ship& s)
{
    const auto bit = w.buildings.find(s.home_port);
    if (bit == w.buildings.end())
        return "a lost port";
    const uint32_t pid = w.provinces.province_of(bit->second.tile);
    return pid != 0 ? land_label(w, pid) : std::string("a port");
}

const char* unit_row_name(const unit_component& u)
{
    const auto& t = unit_roster_table();
    return u.type < t.size() ? t[u.type].name : "Unit";
}

/// "steel 2, electronics 1": a non-zero goods vector in the player's words.
std::string goods_line(const std::array<float, resource_count>& g)
{
    std::string out;
    char        buf[64];
    for (std::size_t i = 0; i < g.size(); ++i)
        if (g[i] > 0.0f)
        {
            std::snprintf(buf, sizeof buf, "%s%s %g", out.empty() ? "" : ", ",
                          presentation_of(static_cast<resource_type>(i)).name, static_cast<double>(g[i]));
            out += buf;
        }
    return out.empty() ? std::string("no goods") : out;
}

std::string mission_refusal(const world& w, const recipe_registry& reg, const sea_ship& s, sea_mission mission,
                            const std::vector<uint32_t>& provinces, corp_command_result r)
{
    const ship_class cls = ship_class_of(s);
    switch (r)
    {
        case corp_command_result::applied: return {};
        case corp_command_result::rejected_placement:
        {
            const int range = reg.military().sea.cls[static_cast<std::size_t>(cls)].range;
            for (const uint32_t p : provinces)
                if (!sea_province_in_range(w, s.owner, p, range))
                {
                    char buf[160];
                    std::snprintf(buf, sizeof buf, "Out of range: %s is past %d tiles of water from your ports",
                                  sea_label(p).c_str(), range);
                    return buf;
                }
            if (mission == sea_mission::blockade)
                return "A blockade lies at the mouth of a port of a nation you have declared hostile, in water this ship can reach";
            return "This ship cannot sail there";
        }
        case corp_command_result::rejected_state:     return "Nothing to change";
        case corp_command_result::rejected_not_owner: return "Not your ship";
        default:
            return std::string(ship_class_label(cls)) + " cannot work " + sea_mission_label(mission);
    }
}

sea_mission current_mission(const ui_state& u, const sea_ship& s, entity_id id)
{
    if (s.mission != sea_mission::none)
        return s.mission;
    if (const auto in = u.ship.intent.find(id); in != u.ship.intent.end())
        return static_cast<sea_mission>(in->second);
    return sea_mission::none;
}

corp_command mission_cmd(const world& w, entity_id ship, sea_mission m, const std::vector<uint32_t>& provinces)
{
    corp_command cmd;
    cmd.corp    = w.player_entity;
    cmd.verb    = corp_verb::assign_sea_mission;
    cmd.subject = ship;
    cmd.order   = static_cast<uint32_t>(m);
    cmd.air_provinces = no_air_provinces();
    for (std::size_t i = 0; i < provinces.size() && i < cmd.air_provinces.size(); ++i)
        cmd.air_provinces[i] = provinces[i];
    return cmd;
}

int surf_count(const world& w, entity_id ship)
{
    int n = 0;
    for (const auto& [uid, u] : w.units)
        if (u.landing_ship == ship)
            ++n;
    return n;
}

ImVec2 item_centre()
{
    return {(ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f,
            (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f};
}

/// The refit press: the seam's own check, its reason the tooltip, its goods the answer.
void draw_refit_press(const world& w, ship_ui_state& u, const sea_ship& s, entity_id id)
{
    const corp_command_result r = refit_ship_check(w, s.owner, id);
    const unit_design live = ship_design_of(w, s.owner, ship_class_of(s));
    std::string reason;
    if (r == corp_command_result::rejected_state)
        reason = s.kit == live ? "It already carries the live design."
                               : "It must have no mission, and not be mustering or refitting already.";
    else if (r == corp_command_result::rejected_placement)
        reason = "It refits lying at its home port: recall it first.";
    else if (r != corp_command_result::applied)
        reason = "It cannot refit.";
    const bool enabled = reason.empty();
    ImGui::BeginDisabled(!enabled);
    const bool pressed = ImGui::Button("Refit", {-1.0f, 0.0f});
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        if (!enabled)
            ImGui::SetTooltip("%s", reason.c_str());
        else
            ImGui::SetTooltip("Refit to the live design. Buys %s, out of action for its muster days.",
                              goods_line(ship_kit_refit_goods(ship_class_of(s), s.kit, live)).c_str());
    }
    const ImVec2 c = item_centre();
    u.refit_x = c.x; u.refit_y = c.y; u.refit_enabled = enabled; u.refit_reason = reason;
    if (pressed && enabled)
    {
        corp_command cmd;
        cmd.corp    = w.player_entity;
        cmd.verb    = corp_verb::refit_ship;
        cmd.subject = id;
        u.pending.push_back(cmd);
    }
}

} // namespace

const char* ship_class_label(ship_class c)
{
    static const char* const k[ship_class_count] = {"Submarine", "Destroyer", "Capital ship", "Transport"};
    const int i = static_cast<int>(c);
    return (i >= 0 && i < ship_class_count) ? k[i] : "Unknown";
}

const char* sea_mission_label(sea_mission m)
{
    static const char* const k[sea_mission_count] = {"None", "Patrol", "Escort", "Raid", "Blockade"};
    const int i = static_cast<int>(m);
    return (i >= 0 && i < sea_mission_count) ? k[i] : "Unknown";
}

const char* ship_display_name(const world& w, entity_id ship)
{
    const auto it = w.ships.find(ship);
    if (it == w.ships.end())
        return "Ship";
    const auto& t = ship_roster_table();
    return it->second.type < t.size() ? t[it->second.type].name : "Ship";
}

bool ship_is_players(const world& w, entity_id ship)
{
    const auto it = w.ships.find(ship);
    return it != w.ships.end() && is_player_side(w, it->second.owner);
}

std::string ship_status_text(const world& w, const recipe_registry& reg, entity_id ship)
{
    const auto it = w.ships.find(ship);
    if (it == w.ships.end())
        return {};
    const sea_ship& s = it->second;
    char buf[160];
    if (s.muster_days > 0)
    {
        std::snprintf(buf, sizeof buf, "%s: %d days", s.refitting ? "Refitting" : "Mustering", s.muster_days);
        return buf;
    }
    if (s.returning)
        return "Broken off: making for home";
    if (surf_count(w, ship) > 0)
        return "Fighting in the surf";
    if (s.landing_tile != null_entity)
    {
        const std::string where = land_label(w, w.provinces.province_of(s.landing_tile));
        if (!s.route.empty())
            return "Sailing to land at " + where;
        const entity_id holder = province_holder_for(w, w.provinces.province_of(s.landing_tile));
        if (holder != null_entity && holder != s.owner
            && (is_hostile(w, s.owner, holder) || is_hostile(w, holder, s.owner))
            && !sea_holds(w, reg, w.provinces.province_of(s.tile), s.owner))
            return "Lying off " + where + ": your side must hold this water";
        std::snprintf(buf, sizeof buf, "Landing at %s: %d days", where.c_str(), s.landing_days);
        return buf;
    }
    if (!s.route.empty())
        return s.mission == sea_mission::none ? "Sailing home" : "Sailing to its zone";
    if (s.mission != sea_mission::none)
    {
        std::snprintf(buf, sizeof buf, "On station: %s", sea_mission_label(s.mission));
        return buf;
    }
    return "Lying at its home port";
}

bool ship_seen_by_player(const world& w, const ui_state& s, entity_id ship)
{
    return ship_is_players(w, ship) || s.spectating || w.ships_found_by_player.count(ship) != 0;
}

void prune_ship_intents(const world& w, ui_state& s)
{
    for (auto it = s.ship.intent.begin(); it != s.ship.intent.end();)
    {
        const auto sit = w.ships.find(it->first);
        if (sit == w.ships.end() || sit->second.mission != sea_mission::none)
            it = s.ship.intent.erase(it);
        else
            ++it;
    }
}

void reset_ship_ui(ui_state& s)
{
    s.ship.intent.clear();
    s.ship.pending.clear();
    s.ship.last_message.clear();
    s.ship.message_ship = null_entity;
    s.military.pending_ship_tile = null_entity;
    s.military.sea_message.clear();
    s.convoy_losses.clear();
}

ship_order_preview preview_ship_order(const world& w, const recipe_registry& reg, const ui_state& ui,
                                      entity_id ship, entity_id tile)
{
    ship_order_preview p;
    const auto it = w.ships.find(ship);
    if (it == w.ships.end() || tile == null_entity || !is_player_side(w, it->second.owner))
        return p;
    const sea_ship& s = it->second;
    const uint32_t province = w.provinces.province_of(tile);
    if (province == 0)
        return p;
    const bool water = is_water_province(w, province);

    if (ship_class_of(s) == ship_class::transport)
    {
        // A transport's one order is its landing, so only the land answers it.
        if (water)
            return p;
        const std::string where = land_label(w, province);
        const corp_command_result r = land_units_check(w, reg, s.owner, ship, tile);
        switch (r)
        {
            case corp_command_result::applied:
            {
                char buf[200];
                std::snprintf(buf, sizeof buf, "Right-click: land %d unit%s at %s", static_cast<int>(s.aboard.size()),
                              s.aboard.size() == 1 ? "" : "s", where.c_str());
                p.kind = 1;
                p.text = buf;
                const entity_id holder = province_holder_for(w, province);
                if (holder != null_entity && holder != s.owner
                    && (is_hostile(w, s.owner, holder) || is_hostile(w, holder, s.owner)))
                    p.text += " (hostile: it waits off the coast until your side holds the water)";
                p.cmd.corp    = w.player_entity;
                p.cmd.verb    = corp_verb::land_units;
                p.cmd.subject = ship;
                p.cmd.tile    = tile;
                return p;
            }
            case corp_command_result::rejected_state:
                p.text = s.aboard.empty() ? "Nothing aboard: board units at a port first"
                                          : "It cannot take a landing order now";
                break;
            case corp_command_result::rejected_terrain:
                p.text = "A unit aboard cannot stand there";
                break;
            default:
                p.text = "Cannot land there: a coast on your, a friend's, a hostile or no one's ground, in range";
                break;
        }
        p.kind = 2;
        return p;
    }

    // A warship's order is a zone, so only the water answers it.
    if (!water)
        return p;
    const std::string where = sea_label(province);
    const sea_mission mission = current_mission(ui, s, ship);
    if (mission == sea_mission::none)
    {
        p.kind = 2;
        p.text = "Choose the ship's mission on its card first";
        return p;
    }
    std::vector<uint32_t> set;
    if (s.mission == mission)
        set = s.provinces;
    if (std::find(set.begin(), set.end(), province) != set.end())
    {
        p.kind = 2;
        p.text = where + " is already assigned";
        return p;
    }
    if (set.size() >= k_sea_mission_provinces_max)
    {
        p.kind = 2;
        p.text = "Eight provinces already assigned: remove one on the ship card";
        return p;
    }
    set.push_back(province);
    const corp_command_result r = sea_mission_check(w, reg, s.owner, ship, static_cast<uint32_t>(mission), set.data(), set.size());
    if (r == corp_command_result::applied)
    {
        p.kind = 1;
        p.text = "Right-click: assign " + where + " (" + sea_mission_label(mission) + ")";
        p.cmd  = mission_cmd(w, ship, mission, set);
        return p;
    }
    p.kind = 2;
    p.text = mission_refusal(w, reg, s, mission, set, r);
    return p;
}

std::string ship_order_answer(corp_verb verb, uint32_t order, corp_command_result r)
{
    const bool ok = r == corp_command_result::applied;
    switch (verb)
    {
        case corp_verb::assign_sea_mission:
            if (ok) return order == 0 ? "Ship recalled." : "Mission assigned.";
            return r == corp_command_result::rejected_placement ? "It cannot work that water." : "The ship can't do that.";
        case corp_verb::refit_ship:
            if (ok) return "Refitting.";
            return r == corp_command_result::rejected_funds ? "Can't afford or supply the refit." : "It can't refit now.";
        case corp_verb::embark_unit:
            return ok ? "Unit aboard." : "That unit can't board now.";
        case corp_verb::land_units:
            if (ok) return order == 0 ? "Sailing to land." : "Landing recalled.";
            return "It can't land there.";
        default:
            return ok ? "Done." : "Refused.";
    }
}

bool draw_ship_selection(const world& w, const recipe_registry& reg, ui_state& ui)
{
    ship_ui_state& u = ui.ship;
    u.missions.clear();
    u.removes.clear();
    u.boards.clear();
    u.aboard_rows = 0;
    u.drawn_ship = null_entity;
    u.refit_x = u.refit_y = u.recall_x = u.recall_y = -1.0f;
    u.refit_enabled = false;
    u.refit_reason.clear();
    const entity_id id = ui.selected_entity;
    const auto it = w.ships.find(id);
    if (it == w.ships.end())
        return false;
    const sea_ship&   s    = it->second;
    const ship_class  cls  = ship_class_of(s);
    const bool        mine = is_player_side(w, s.owner);
    const ImGuiStyle& style = ImGui::GetStyle();
    ImDrawList*       dl    = ImGui::GetWindowDrawList();
    u.drawn_ship = id;
    if (u.message_ship != id) // an answer belongs to the ship it was about
    {
        u.message_ship = id;
        u.last_message.clear();
    }
    prune_ship_intents(w, ui);

    // ── Header: [ship] name · class, home port ──
    {
        const float  fh = ImGui::GetFrameHeight();
        const ImVec2 hc = ImGui::GetCursorScreenPos();
        const float  ir = fh * 0.46f;
        icons::ship(dl, {hc.x + ir, hc.y + fh * 0.5f}, ir, cls,
                    palette::corp_identity_colour(s.owner, w.player_entity));
        ImGui::SetCursorScreenPos({hc.x + ir * 2.0f + style.ItemSpacing.x, hc.y});
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", ship_display_name(w, id));
        ImGui::SameLine();
        if (mine)
            ImGui::TextDisabled("- %s, home at %s", ship_class_label(cls), home_label(w, s).c_str());
        else
            ImGui::TextDisabled("- %s", ship_class_label(cls));
    }
    ImGui::Separator();

    const float total_w = ImGui::GetContentRegionAvail().x;

    // A RIVAL's ship (BL-068): what it is and whose, nothing of its condition or orders.
    if (!mine)
    {
        const char* who = owner_display_name(w, s.owner);
        ImGui::TextDisabled("Owner: %s", who != nullptr ? who : "a rival");
        draw_stat_grid("ship_card", ship_stat_bands(w, reg, id), std::min(total_w, 420.0f));
        return true;
    }

    const float grid_col_w = (total_w - 2.0f * style.ItemSpacing.x) * 0.44f;
    const float col_w      = (total_w - 2.0f * style.ItemSpacing.x - grid_col_w) * 0.5f;
    const ImGuiWindowFlags cf = ImGuiWindowFlags_NoSavedSettings;

    // ── Left: its condition ──
    ImGui::BeginChild("##ship_state", {grid_col_w, 0.0f}, false, cf);
    ImGui::PushTextWrapPos(0.0f);
    draw_stat_grid("ship_card", ship_stat_bands(w, reg, id), ImGui::GetContentRegionAvail().x);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", ship_status_text(w, reg, id).c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndChild();
    ImGui::SameLine();

    // ── Middle: the mission, or a transport's landing ──
    ImGui::BeginChild("##ship_mission", {col_w, 0.0f}, false, cf);
    if (cls == ship_class::transport)
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Landing");
        const corp_command_result rc = land_units_check(w, reg, s.owner, id, null_entity);
        const bool can_recall = rc == corp_command_result::applied;
        if (s.landing_tile != null_entity)
        {
            ImGui::BeginDisabled(!can_recall);
            const bool pressed = ImGui::Button("Recall", {-1.0f, 0.0f});
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", can_recall ? "Call off the landing and sail home."
                                                   : "Its units are in the surf: the fight decides it.");
            const ImVec2 c = item_centre();
            u.recall_x = c.x; u.recall_y = c.y;
            if (pressed && can_recall)
            {
                corp_command cmd;
                cmd.corp    = w.player_entity;
                cmd.verb    = corp_verb::land_units;
                cmd.subject = id;
                cmd.tile    = null_entity;
                cmd.order   = 1; // for the answer's words only: the seam reads the null tile
                u.pending.push_back(cmd);
            }
        }
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled(s.aboard.empty() ? "Board units at a port, then right-click a coast."
                                             : "Right-click a coast on the map to land.");
        ImGui::PopTextWrapPos();
    }
    else
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Mission");
        const sea_mission cur = current_mission(ui, s, id);
        for (int mi = 0; mi < sea_mission_count; ++mi)
        {
            const sea_mission m = static_cast<sea_mission>(mi);
            if (m != sea_mission::none && !ship_class_works(reg, cls, m))
                continue;
            const bool active = (m == cur);
            const sea_mission to = active ? sea_mission::none : m;
            bool        enabled = true;
            std::string reason;
            if (to != sea_mission::none && !s.provinces.empty())
            {
                const corp_command_result r = sea_mission_check(w, reg, s.owner, id, static_cast<uint32_t>(to),
                                                                s.provinces.data(), s.provinces.size());
                if (r != corp_command_result::applied)
                {
                    enabled = false;
                    reason  = mission_refusal(w, reg, s, to, s.provinces, r);
                }
            }
            if (m == sea_mission::none && active)
                enabled = false; // None lit: nothing beneath it

            ImGui::PushID(mi);
            if (active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(palette::hover));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));
            }
            ImGui::BeginDisabled(!enabled);
            const bool pressed = ImGui::Button(sea_mission_label(m), {-1.0f, 0.0f});
            ImGui::EndDisabled();
            if (active)
                ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                if (!reason.empty())
                    ImGui::SetTooltip("%s", reason.c_str());
                else if (active && m != sea_mission::none)
                    ImGui::SetTooltip("Working %s - press to recall the ship", sea_mission_label(m));
                else if (m != sea_mission::none && s.provinces.empty())
                    ImGui::SetTooltip("%s - then right-click a sea province on the map to assign it",
                                      sea_mission_label(m));
                else if (m != sea_mission::none)
                    ImGui::SetTooltip("Work %s over the assigned provinces", sea_mission_label(m));
            }
            const ImVec2 c = item_centre();
            u.missions.push_back({mi, c.x, c.y, enabled, active, reason});
            ImGui::PopID();
            if (pressed && enabled)
            {
                if (to == sea_mission::none)
                {
                    u.intent.erase(id);
                    if (s.mission != sea_mission::none)
                        u.pending.push_back(mission_cmd(w, id, sea_mission::none, {}));
                }
                else if (s.provinces.empty())
                    u.intent[id] = static_cast<std::uint8_t>(to); // held until a province is named
                else
                    u.pending.push_back(mission_cmd(w, id, to, s.provinces));
            }
        }
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled(cur == sea_mission::none ? "Choose a mission, then right-click sea provinces."
                                                     : "Right-click a sea province to assign it.");
        ImGui::PopTextWrapPos();
    }
    draw_refit_press(w, u, s, id);
    if (!u.last_message.empty())
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", u.last_message.c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ── Right: the zone, or who is aboard and who may board ──
    ImGui::BeginChild("##ship_list", {0.0f, 0.0f}, false, cf);
    if (cls == ship_class::transport)
    {
        const int capacity = ship_stats(reg, s).capacity;
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Aboard (%d of %d)",
                           static_cast<int>(s.aboard.size()), capacity);
        if (s.aboard.empty())
            ImGui::TextDisabled("No one aboard.");
        for (const auto& [uid, un] : s.aboard)
        {
            ImGui::TextUnformatted(unit_row_name(un));
            ++u.aboard_rows;
        }
        for (const auto& [uid, un] : w.units)
            if (un.landing_ship == id)
            {
                ImGui::TextUnformatted(unit_row_name(un));
                ImGui::SameLine();
                ImGui::TextDisabled("(in the surf)");
            }
        ImGui::Spacing();
        ImGui::TextDisabled("Ready to board");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("A land unit on or beside your open port boards a transport lying at that port's mouth.");
        const float bw = ImGui::CalcTextSize("Board").x + style.FramePadding.x * 2.0f;
        int ready = 0;
        for (const auto& [uid, un] : w.units)
        {
            if (embark_check(w, reg, s.owner, uid, id) != corp_command_result::applied)
                continue;
            ++ready;
            ImGui::PushID(static_cast<int>(uid));
            const float x0 = ImGui::GetCursorPosX();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(unit_row_name(un));
            ImGui::SameLine(x0 + ImGui::GetContentRegionAvail().x - bw);
            const bool pressed = ImGui::Button("Board", {bw, 0.0f});
            const ImVec2 c = item_centre();
            u.boards.push_back({uid, c.x, c.y});
            if (pressed)
            {
                corp_command cmd;
                cmd.corp    = w.player_entity;
                cmd.verb    = corp_verb::embark_unit;
                cmd.subject = uid;
                cmd.order   = static_cast<uint32_t>(id);
                u.pending.push_back(cmd);
            }
            ImGui::PopID();
        }
        if (ready == 0)
            ImGui::TextDisabled("No unit at this port.");
    }
    else
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Zone (%d of %d)",
                           static_cast<int>(s.provinces.size()), static_cast<int>(k_sea_mission_provinces_max));
        if (s.provinces.empty())
            ImGui::TextDisabled("None assigned.");
        const float xw = ImGui::GetFrameHeight();
        for (const uint32_t p : s.provinces)
        {
            ImGui::PushID(static_cast<int>(p));
            const float x0 = ImGui::GetCursorPosX();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(sea_label(p).c_str());
            if (sea_holds(w, reg, p, s.owner))
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(held)");
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Your share of the surface attack here: %d%%",
                                  sea_control_share_permille(w, reg, p, s.owner) / 10);
            ImGui::SameLine(x0 + ImGui::GetContentRegionAvail().x - xw);
            if (ImGui::Button("x", {xw, xw}))
            {
                std::vector<uint32_t> rest;
                for (const uint32_t q : s.provinces)
                    if (q != p)
                        rest.push_back(q);
                if (rest.empty())
                {
                    u.intent[id] = static_cast<std::uint8_t>(s.mission);
                    u.pending.push_back(mission_cmd(w, id, sea_mission::none, {}));
                }
                else
                    u.pending.push_back(mission_cmd(w, id, s.mission, rest));
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Remove this province from the ship's zone");
            const ImVec2 c = item_centre();
            u.removes.push_back({p, c.x, c.y});
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    return true;
}

} // namespace ui
