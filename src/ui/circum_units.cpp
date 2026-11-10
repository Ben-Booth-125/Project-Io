#include "circum_units.hpp"

#include "highlight.hpp"
#include "hover_card.hpp"
#include "icons.hpp"
#include "presentation.hpp"

#include "world/debris.hpp"
#include "world/offworld_base.hpp"  // BL-1610: the base orders (embark, found) and their dials
#include "world/space_assets.hpp"   // space_home_body_of: where colonists board
#include "world/space_convoy.hpp"   // space_haul: the base kit held aboard a landed Lander
#include "world/space_tech_keys.hpp" // space_keys::base_founding
#include "world/survey_system.hpp"  // survey_tile_mapped / _prospected: a site's two survey steps
#include "world/nation_tech.hpp"   // nation_has_mechanic: which items the designer offers
#include "world/province.hpp"      // province_holder_for: whose ground a bombard row strikes
#include "world/orbital_graph.hpp"
#include "world/solar_storm.hpp"
#include "world/space_bombard.hpp"
#include "world/space_command.hpp"
#include "world/space_units.hpp"
#include "world/stations.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <set>
#include <tuple>

namespace ui {

namespace {

constexpr float k_fleet_r   = 6.5f;  ///< A fleet mark's radius, px (constant at any zoom).
constexpr float k_gap       = 4.0f;
constexpr float k_hit_pad   = 2.0f;
#define IO_DV "\xE2\x88\x86v" // U+2206, the glyph the space ledger prints for delta-v

const space_unit* find_unit(const world& w, entity_id id)
{
    const auto it = w.space_units.find(id);
    return it != w.space_units.end() ? &it->second : nullptr;
}

const char* state_label(space_unit_state s)
{
    switch (s)
    {
        case space_unit_state::idle:       return "idle in orbit";
        case space_unit_state::docked:     return "docked";
        case space_unit_state::in_transit: return "in transit";
        case space_unit_state::landed:     return "landed";
    }
    return "?";
}

/// The point on a ring node where fleets ride: the low orbit's upper left, the
/// high orbit's upper right, walked along the ring by @p arc px.
ImVec2 ring_fleet_point(const circum_node_mark& m, float arc)
{
    const bool  low = m.node.kind == orbital_node_kind::low_orbit;
    const float th0 = low ? 3.14159265f * 1.25f : 3.14159265f * 1.75f; // screen angles: up is negative y
    const float th  = th0 + (low ? -1.0f : 1.0f) * arc / std::max(1.0f, m.ring_r);
    return { m.pos.x + std::cos(th) * m.ring_r, m.pos.y + std::sin(th) * m.ring_r };
}

const circum_node_mark* mark_of(const std::vector<circum_node_mark>& nodes, orbital_node n)
{
    for (const circum_node_mark& m : nodes)
        if (m.node == n)
            return &m;
    return nullptr;
}

bool on_rung(const std::vector<circum_node_mark>& nodes, orbital_node n)
{
    ImVec2 p{};
    return circum_node_endpoint(nodes, n, {0.0f, 0.0f}, &p);
}

} // namespace

const char* space_hull_label(int hull)
{
    if (hull >= 0 && static_cast<std::size_t>(hull) < space_hull_count)
        return space_hull_table()[static_cast<std::size_t>(hull)].name;
    return "?";
}

float space_unit_transit_share(const world& w, const space_unit& u)
{
    if (u.state != space_unit_state::in_transit)
        return 1.0f;
    const orbital_path_result p = orbital_path(w, orbital_graph_params{}, u.node, u.dest);
    const double days = p.ok && std::isfinite(p.days) ? std::max(1.0, std::ceil(p.days)) : 1.0;
    const double total = std::max(days, static_cast<double>(u.days_remaining));
    return std::clamp(static_cast<float>(1.0 - static_cast<double>(u.days_remaining) / total), 0.0f, 1.0f);
}

bool space_asset_storm_struck(const world& w, const space_asset& a)
{
    if (a.cls == space_asset_class::satellite)
        return solar_storm_satellite_dark(w, a, w.current_econ_tick);
    if (a.cls == space_asset_class::station_core)
        return a.state != space_asset_state::in_transit && a.state != space_asset_state::lost
            && solar_storm_strikes(w, a.node, w.current_econ_tick);
    return false;
}

std::vector<circum_fleet_mark> layout_circum_fleets(const world& w, const std::vector<circum_node_mark>& nodes)
{
    std::vector<circum_fleet_mark> out;
    if (w.space_units.empty() || nodes.empty())
        return out;

    // At a node: one mark per (node, owner), lead = the lowest id (std::map: ascending).
    std::map<std::pair<orbital_node, entity_id>, circum_fleet_mark> at;
    // In transit: one mark per (owner, origin, destination, days remaining).
    std::map<std::tuple<entity_id, orbital_node, orbital_node, int32_t>, circum_fleet_mark> moving;
    for (const auto& [uid, u] : w.space_units)
    {
        if (u.state == space_unit_state::in_transit)
        {
            auto key = std::make_tuple(u.owner, u.node, u.dest, u.days_remaining);
            auto [it, fresh] = moving.try_emplace(key);
            circum_fleet_mark& m = it->second;
            if (fresh)
            {
                m.lead = uid;
                m.owner = u.owner;
                m.hull = static_cast<int>(u.hull);
                m.in_transit = true;
                m.share = space_unit_transit_share(w, u);
                m.node = orbital_node_display_name(w, u.dest);
            }
            ++m.count;
            continue;
        }
        auto [it, fresh] = at.try_emplace(std::make_pair(u.node, u.owner));
        circum_fleet_mark& m = it->second;
        if (fresh)
        {
            m.lead = uid;
            m.owner = u.owner;
            m.hull = static_cast<int>(u.hull);
            m.landed = u.state == space_unit_state::landed;
            m.node = orbital_node_display_name(w, u.node);
        }
        ++m.count;
    }

    // Place the node fleets, walking each node's row.
    std::map<orbital_node, float> walked;
    for (auto& [key, m] : at)
    {
        const orbital_node n = key.first;
        m.r = k_fleet_r;
        ImVec2 p{};
        if (n.kind == orbital_node_kind::surface)
        {
            if (!circum_node_endpoint(nodes, n, {0.0f, 0.0f}, &p))
                continue;
            float& arc = walked[n];
            m.pos = { p.x - 14.0f - arc, p.y - 10.0f };
            arc += 2.0f * m.r + k_gap;
        }
        else
        {
            const circum_node_mark* nm = mark_of(nodes, n);
            if (nm == nullptr)
                continue;
            float& arc = walked[n];
            if (nm->ring_r > 0.0f)
                m.pos = ring_fleet_point(*nm, arc);
            else
                m.pos = { nm->pos.x - arc, nm->pos.y - 5.0f - 4.0f - m.r - 10.0f };
            arc += 2.0f * m.r + k_gap;
        }
        out.push_back(m);
    }
    // Place the travelling groups on their segments.
    for (auto& [key, m] : moving)
    {
        const orbital_node from = std::get<1>(key), to = std::get<2>(key);
        if (!on_rung(nodes, from) || !on_rung(nodes, to))
            continue;
        ImVec2 a{}, b{}, mid{};
        // Aim each end at the other's centre, then re-aim once at the found ends.
        (void)circum_node_endpoint(nodes, to, {0.0f, 0.0f}, &mid);
        (void)circum_node_endpoint(nodes, from, mid, &a);
        (void)circum_node_endpoint(nodes, to, a, &b);
        m.from = a;
        m.to   = b;
        m.r    = k_fleet_r;
        m.pos  = { a.x + (b.x - a.x) * m.share, a.y + (b.y - a.y) * m.share };
        out.push_back(m);
    }
    return out;
}

std::vector<circum_fleet_mark> layout_solar_fleets(const world& w,
                                                   const std::map<entity_id, std::pair<ImVec2, float>>& bodies)
{
    std::vector<circum_fleet_mark> out;
    if (w.space_units.empty())
        return out;
    std::map<std::pair<entity_id, entity_id>, circum_fleet_mark> at; // (body, owner)
    std::map<std::tuple<entity_id, entity_id, entity_id, int32_t>, circum_fleet_mark> moving;
    for (const auto& [uid, u] : w.space_units)
    {
        const bool between = u.state == space_unit_state::in_transit && u.node.body != u.dest.body;
        circum_fleet_mark* m = nullptr;
        bool fresh = false;
        if (between)
        {
            auto r = moving.try_emplace(std::make_tuple(u.owner, u.node.body, u.dest.body, u.days_remaining));
            m = &r.first->second;
            fresh = r.second;
        }
        else
        {
            // At a body (a move within one body's nodes reads as the body here).
            const entity_id b = u.state == space_unit_state::in_transit ? u.dest.body : u.node.body;
            auto r = at.try_emplace(std::make_pair(b, u.owner));
            m = &r.first->second;
            fresh = r.second;
        }
        if (fresh)
        {
            m->lead = uid;
            m->owner = u.owner;
            m->hull = static_cast<int>(u.hull);
            m->in_transit = between;
            m->share = between ? space_unit_transit_share(w, u) : 1.0f;
            m->node = between ? orbital_node_display_name(w, u.dest) : orbital_node_display_name(w, u.node);
        }
        ++m->count;
    }
    std::map<entity_id, float> walked;
    for (auto& [key, m] : at)
    {
        const auto it = bodies.find(key.first);
        if (it == bodies.end())
            continue;
        const ImVec2 c = it->second.first;
        const float  r = it->second.second;
        float& arc = walked[key.first];
        m.r   = k_fleet_r;
        m.pos = { c.x - r - 4.0f - m.r - arc, c.y - r - 4.0f - m.r };
        arc += 2.0f * m.r + k_gap;
        out.push_back(m);
    }
    for (auto& [key, m] : moving)
    {
        const auto a = bodies.find(std::get<1>(key));
        const auto b = bodies.find(std::get<2>(key));
        if (a == bodies.end() || b == bodies.end())
            continue;
        m.from = a->second.first;
        m.to   = b->second.first;
        m.r    = k_fleet_r;
        m.pos  = { m.from.x + (m.to.x - m.from.x) * m.share, m.from.y + (m.to.y - m.from.y) * m.share };
        out.push_back(m);
    }
    return out;
}

std::vector<circum_debris_mark> layout_circum_debris(const world& w, const std::vector<circum_node_mark>& nodes)
{
    std::vector<circum_debris_mark> out;
    if (w.space_debris.empty())
        return out;
    for (const circum_node_mark& nm : nodes)
    {
        const auto it = w.space_debris.find(nm.node);
        if (it == w.space_debris.end() || !(it->second > k_space_debris_floor))
            continue;
        circum_debris_mark d;
        d.node  = nm.node;
        d.level = it->second;
        d.r     = std::clamp(4.0f + 1.5f * std::log10(1.0f + it->second), 4.0f, 9.0f);
        if (nm.ring_r > 0.0f)
        {
            // The ring's foot, a little off its lowest point, clear of the pips (top),
            // the assets (the equator's sides) and the fleets (the upper side).
            const float th = nm.node.kind == orbital_node_kind::low_orbit ? 1.9f : 1.25f;
            d.pos = { nm.pos.x + std::cos(th) * nm.ring_r, nm.pos.y + std::sin(th) * nm.ring_r };
        }
        else
        {
            d.pos = { nm.pos.x + 5.0f + 3.0f + d.r, nm.pos.y };
        }
        out.push_back(d);
    }
    return out;
}

void draw_circum_debris(ImDrawList* dl, const std::vector<circum_debris_mark>& marks)
{
    for (const circum_debris_mark& d : marks)
        icons::debris(dl, d.pos, d.r, palette::space_debris);
}

void draw_circum_fleets(ImDrawList* dl, const std::vector<circum_fleet_mark>& marks, entity_id hovered,
                        entity_id selected)
{
    for (const circum_fleet_mark& m : marks)
        if (m.in_transit)
            draw_space_transit(dl, m.from, m.to, m.share, palette::nation_colour(m.owner));
    for (const circum_fleet_mark& m : marks)
    {
        icons::space_fleet_marker(dl, m.pos, m.r, m.hull, palette::nation_colour(m.owner), m.count, m.in_transit);
        draw_body_highlight(dl, m.pos, m.r + 1.5f, resolve_highlight(m.lead == selected, m.lead == hovered, false));
    }
}

int hit_circum_fleet(const std::vector<circum_fleet_mark>& marks, ImVec2 mouse)
{
    int   best   = -1;
    float best_d = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < marks.size(); ++i)
    {
        const float dx = mouse.x - marks[i].pos.x, dy = mouse.y - marks[i].pos.y;
        const float d  = std::sqrt(dx * dx + dy * dy);
        if (d <= marks[i].r + k_hit_pad && d < best_d)
        {
            best   = static_cast<int>(i);
            best_d = d;
        }
    }
    return best;
}

void draw_space_unit_summary(const world& w, entity_id id)
{
    const space_unit* u = find_unit(w, id);
    if (u == nullptr)
        return;
    const space_design_stats st = evaluate_space_design(u->hull, u->design);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", space_hull_label(static_cast<int>(u->hull)));
    const auto nit = w.nations.find(u->owner);
    ImGui::TextUnformatted("Owner:");
    ImGui::SameLine();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::nation_colour(u->owner)), "%s",
                       nit != w.nations.end() ? nit->second.name.c_str() : "?");
    if (u->state == space_unit_state::in_transit)
    {
        ImGui::Text("Left from: %s", orbital_node_display_name(w, u->node).c_str());
        ImGui::Text("Bound for: %s, %d day%s", orbital_node_display_name(w, u->dest).c_str(), u->days_remaining,
                    u->days_remaining == 1 ? "" : "s");
    }
    else
    {
        ImGui::Text("Node: %s", orbital_node_display_name(w, u->node).c_str());
        if (u->state == space_unit_state::docked)
            ImGui::Text("State: docked at %s", w.stations.count(u->station) ? "its station" : "a station");
        else
            ImGui::Text("State: %s", state_label(u->state));
    }
    ImGui::Text(IO_DV " budget: %.1f of %.1f", static_cast<double>(space_unit_dv_remaining(*u)),
                static_cast<double>(st.dv_capacity));
    ImGui::Text("Propellant: %.0f of %.0f", static_cast<double>(u->propellant), static_cast<double>(st.tank));
    ImGui::Text("Hull: %.0f of %.0f", static_cast<double>(u->hull_points),
                static_cast<double>(space_unit_max_hull_points(*u)));
    if (st.cargo > 0.0f)
        ImGui::Text("Cargo: %.0f of %.0f", static_cast<double>(u->cargo), static_cast<double>(st.cargo));
    if (st.passengers > 0) // BL-1610: a passenger bay's colonists, in thousands
        ImGui::Text("Colonists: %d thousand of %d berths", static_cast<int>(space_unit_colonists(w, id)),
                    static_cast<int>(st.passengers));
    if (st.firepower > 0.0f)
        ImGui::Text("Firepower %.0f, armour %.0f", static_cast<double>(st.firepower), static_cast<double>(st.armour));
    const auto bo = w.space_bombard_orders.find(id);
    if (bo != w.space_bombard_orders.end())
        ImGui::TextUnformatted("Bombarding: a standing order");
}

void draw_circum_fleet_card(const world& w, entity_id unit, ImVec2 mouse)
{
    draw_hover_card(mouse, [&]() { draw_space_unit_summary(w, unit); });
}

bool draw_space_unit_selection(const world& w, ui_state& ui)
{
    const entity_id id = ui.selected_entity;
    const space_unit* u = find_unit(w, id);
    if (u == nullptr)
        return false;
    space_fleet_ui_state& f = ui.fleet;
    if (f.unit != id)
    {
        // A new selection drops what was armed for the last one.
        f = space_fleet_ui_state{};
        f.unit = id;
    }
    f.presses.clear();
    f.dest_rows.clear();
    f.target_rows.clear();
    f.site_rows.clear();
    f.preview_dv = f.preview_budget = -1.0f;
    f.colonists_aboard = f.free_berths = f.colonists_here = f.colonists_need = -1;
    f.kit_here = f.kit_need = -1.0f;

    // Two columns: the summary (and the fleet here) on the left, the orders on the
    // right, so an armed list has the band's height rather than what the summary
    // leaves below it. A table, because a wrapped line, a separator or a selectable
    // spans its whole container: a group would grow to the band's width and push the
    // orders off it.
    struct table_guard
    {
        bool on = false;
        ~table_guard() { if (on) ImGui::EndTable(); }
    } table;
    table.on = ImGui::BeginTable("##space_unit_card", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings);
    if (table.on)
    {
        ImGui::TableSetupColumn("summary", ImGuiTableColumnFlags_WidthStretch, 0.42f);
        ImGui::TableSetupColumn("orders", ImGuiTableColumnFlags_WidthStretch, 0.58f);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
    }
    ImGui::PushTextWrapPos(0.0f); // the summary's lines wrap at its column, never clip
    draw_space_unit_summary(w, id);
    ImGui::PopTextWrapPos();

    // The fleet: the owner's units at this node, not in transit. A row selects its unit.
    std::vector<entity_id> fleet;
    if (u->state != space_unit_state::in_transit)
        for (const auto& [uid, o] : w.space_units)
            if (o.owner == u->owner && o.node == u->node && o.state != space_unit_state::in_transit)
                fleet.push_back(uid);
    if (fleet.size() > 1)
    {
        ImGui::Separator();
        ImGui::Text("Fleet here: %d units", static_cast<int>(fleet.size()));
        for (const entity_id fid : fleet)
        {
            const space_unit& o = w.space_units.at(fid);
            ImGui::PushID(static_cast<int>(fid));
            char label[96];
            std::snprintf(label, sizeof label, "%s  " IO_DV " %.1f", space_hull_label(static_cast<int>(o.hull)),
                          static_cast<double>(space_unit_dv_remaining(o)));
            if (ImGui::Selectable(label, fid == id))
                ui.selected_entity = fid; // a selector: the Selection element is exempt from the toggle rule
            ImGui::PopID();
        }
    }

    const entity_id me = player_nation(w);
    if (me == null_entity || u->owner != me || ui.spectating)
        return true;

    if (table.on)
        ImGui::TableSetColumnIndex(1);
    else
        ImGui::Separator();
    // The room an armed list may take: what the band has left under it, less a
    // preview line and its press.
    const auto list_height = [](int reserve_rows) {
        const float room = ImGui::GetContentRegionAvail().y
                         - static_cast<float>(reserve_rows) * ImGui::GetFrameHeightWithSpacing();
        return std::max(3.0f * ImGui::GetTextLineHeightWithSpacing(), room);
    };
    const auto list_width = []() { return std::max(120.0f, ImGui::GetContentRegionAvail().x); };
    const auto publish = [&](const char* name, bool enabled, const char* reason) {
        space_fleet_ui_state::press p;
        p.name    = name;
        p.x       = (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f;
        p.y       = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
        p.enabled = enabled;
        p.reason  = reason != nullptr ? reason : "";
        f.presses.push_back(p);
    };
    const auto queue = [&](space_verb v, auto&& fill) {
        space_command c;
        c.nation  = me;
        c.verb    = static_cast<uint8_t>(v);
        c.subject = id;
        fill(c);
        ui.space.pending.push_back(c);
    };
    const bool moving = u->state == space_unit_state::in_transit;
    if (moving)
    {
        ImGui::TextDisabled("In transit: it cannot be ordered until it arrives.");
        return true;
    }

    // ── Move: a toggle that arms the destination list. ──
    {
        const bool lit = f.move_armed;
        if (lit) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button("Move"))
        {
            f.move_armed = !f.move_armed;
            f.dest_set   = false;
            f.bombard_armed = false;
            f.embark_armed = f.found_armed = false; // BL-1610: one armed list at a time
        }
        if (lit) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Choose a destination; its " IO_DV " cost shows against the budget before you confirm.");
        publish("move", true, nullptr);
    }

    // ── Dock / Undock: a station of its own at this node with a Dock. ──
    const station_params stp{};
    if (u->state == space_unit_state::docked)
    {
        ImGui::SameLine();
        if (ImGui::Button("Undock"))
            queue(space_verb::undock_unit, [](space_command&) {});
        publish("undock", true, nullptr);
    }
    else
    {
        entity_id dock_at = null_entity;
        for (const auto& [sid, s] : w.stations)
            if (s.owner == me && s.node == u->node)
            {
                dock_at = sid;
                break;
            }
        ImGui::SameLine();
        ImGui::BeginDisabled(dock_at == null_entity);
        if (ImGui::Button("Dock"))
            queue(space_verb::dock_unit, [&](space_command& c) { c.target = dock_at; });
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(dock_at == null_entity ? "No station of yours at this node."
                                                     : "Berth at your station here: it needs a working Dock with a free berth.");
        publish("dock", dock_at != null_entity, dock_at == null_entity ? "no station of yours here" : nullptr);
    }

    // ── Refuel ──
    {
        ImGui::SameLine();
        if (ImGui::Button("Refuel"))
            queue(space_verb::refuel_unit, [](space_command&) {});
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Fill the tanks: docked at a station's Fuel depot, or in a low orbit over your Launchpad.");
        publish("refuel", true, nullptr);
    }

    // ── Bombard: a toggle that arms the target list (an orbital-missile armament, low orbit). ──
    const bool can_bombard = space_unit_can_bombard(*u) && u->node.kind == orbital_node_kind::low_orbit;
    const bool has_order   = w.space_bombard_orders.count(id) != 0;
    {
        ImGui::SameLine();
        const bool lit = f.bombard_armed || has_order;
        ImGui::BeginDisabled(!can_bombard && !has_order);
        if (lit) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button("Bombard"))
        {
            if (has_order)
                queue(space_verb::bombard, [](space_command& c) { c.clear = true; }); // the toggle: lifts the order
            else
                f.bombard_armed = !f.bombard_armed;
            f.move_armed = false;
            f.embark_armed = f.found_armed = false; // BL-1610: one armed list at a time
        }
        if (lit) ImGui::PopStyleColor();
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(has_order ? "Bombarding: press to stop."
                              : can_bombard ? "Choose ground below to strike once a day."
                                            : "Needs an orbital-missile armament, in a body's low orbit.");
        publish("bombard", can_bombard || has_order,
                can_bombard || has_order ? nullptr : "no orbital-missile armament in low orbit");
    }

    // ── BL-1610 (NATIONS.md § Off-world bases; SELECTION.md § The space-unit element):
    // a Lander's base orders, each a toggle that arms its own list. EMBARK COLONISTS
    // boards people from the nation's home centres into a passenger bay's berths, a
    // thousand to a berth, at its home body (idle in the low orbit, or landed). FOUND
    // BASE, landed on another body's surface, lists the ground a founding could take
    // there - a Lander lands at the body's surface node, not on a tile, so the player
    // picks the site - and confirms it through the seam, refused with its reason.
    if (u->hull == space_hull::lander)
    {
        const offworld_base_params bp{};
        const space_design_stats ust = evaluate_space_design(u->hull, u->design);
        const int       berths = ust.passengers;
        const int       aboard = static_cast<int>(space_unit_colonists(w, id));
        const entity_id home   = space_home_body_of(w, me);
        const entity_id body   = u->node.body;
        const bool      landed = u->state == space_unit_state::landed && u->node.kind == orbital_node_kind::surface;

        if (berths > 0)
        {
            const int free = std::max(0, berths - aboard);
            f.colonists_aboard = aboard;
            f.free_berths      = free;
            const bool at_home = home != null_entity && body == home
                && ((u->state == space_unit_state::idle && u->node.kind == orbital_node_kind::low_orbit) || landed);
            const bool pad = at_home && space_staffed_pad_on(w, me, home) != null_entity;
            const char* why = !at_home  ? "board at your home body: idle in its low orbit, or landed"
                            : !pad      ? "no staffed Launchpad of yours on your home body"
                            : free == 0 ? "every berth is taken"
                                        : nullptr;
            if (why != nullptr)
                f.embark_armed = false; // nothing to confirm: the list cannot stay lit
            const bool lit = f.embark_armed;
            ImGui::BeginDisabled(why != nullptr);
            if (lit) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button("Embark colonists"))
            {
                f.embark_armed = !f.embark_armed;
                f.embark_count = std::max(1, free); // every free berth, the most a founding can use
                f.found_armed = f.move_armed = f.bombard_armed = false;
            }
            if (lit) ImGui::PopStyleColor();
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", why != nullptr ? why
                                        : "Board colonists from your most populous home centre, a thousand to a berth.");
            publish("embark", why == nullptr, why);
            if (f.embark_armed)
            {
                f.embark_count = std::clamp(f.embark_count, 1, std::max(1, free));
                ImGui::SetNextItemWidth(std::min(160.0f, list_width()));
                ImGui::SliderInt("thousand##embark_n", &f.embark_count, 1, std::max(1, free));
                f.embark_count = std::clamp(f.embark_count, 1, std::max(1, free));
                char lbl[64];
                std::snprintf(lbl, sizeof lbl, "Confirm embark %d thousand", f.embark_count);
                if (ImGui::Button(lbl))
                {
                    queue(space_verb::embark_colonists, [&](space_command& c) { c.slot = f.embark_count; });
                    f.embark_armed = false;
                }
                publish("confirm_embark", true, nullptr);
            }
        }

        if (landed && body != home && body != w.home_body)
        {
            // What the nation's Landers landed here carry, as the founding counts it
            // (offworld_base.cpp): every colonist aboard, and the kit good held aboard.
            int   here = 0;
            float kit  = 0.0f;
            for (const auto& [oid, o] : w.space_units)
            {
                if (o.owner != me || o.hull != space_hull::lander || o.state != space_unit_state::landed
                    || o.node.body != body || o.node.kind != orbital_node_kind::surface)
                    continue;
                here += static_cast<int>(space_unit_colonists(w, oid));
                const auto hit = w.space_hauls.find(oid);
                if (hit != w.space_hauls.end() && hit->second.phase == space_haul_phase::held
                    && hit->second.cargo_res == bp.kit_res)
                    kit += std::max(0.0f, o.cargo);
            }
            const int need = std::max<int32_t>(1, bp.min_colonists);
            f.colonists_here = here;
            f.colonists_need = need;
            f.kit_here       = kit;
            f.kit_need       = bp.kit_qty;
            const bool tech = nation_has_mechanic(w, me, space_keys::base_founding);
            if (!tech)
                f.found_armed = false;
            const bool lit = f.found_armed;
            if (berths > 0)
                ImGui::SameLine();
            ImGui::BeginDisabled(!tech);
            if (lit) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button("Found base"))
            {
                f.found_armed = !f.found_armed;
                f.found_tile  = null_entity;
                f.embark_armed = f.move_armed = f.bombard_armed = false;
            }
            if (lit) ImGui::PopStyleColor();
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", tech ? "Choose mapped ground here for a base: it claims its province."
                                             : "Needs Off-World Bases researched.");
            publish("found_base", tech, tech ? nullptr : "needs Off-World Bases researched");

            if (f.found_armed)
            {
                const ImU32 ok_col  = palette::positive;
                const ImU32 bad_col = palette::negative;
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(here >= need ? ok_col : bad_col),
                                   "Colonists landed here: %d of %d thousand", here, need);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kit >= bp.kit_qty ? ok_col : bad_col),
                                   "Base kit landed here: %.0f of %.0f %s", static_cast<double>(kit),
                                   static_cast<double>(bp.kit_qty), presentation_of(bp.kit_res).name);

                // The ground a founding could take: land this nation has MAPPED here,
                // nobody's, with no centre or building on it, in a province no nation
                // holds. One row per province - the founding claims the whole of it -
                // at its most habitable such tile (ties to the lower row, then column).
                std::set<entity_id> occupied;
                for (const auto& [cid, t] : w.population_centre_tile)
                    occupied.insert(t);
                for (const auto& [bid, b] : w.buildings)
                    occupied.insert(b.tile);
                struct site { uint32_t pid; entity_id tile; int col, row; float hab; bool prospected; };
                std::map<uint32_t, site> best;
                for (const auto& [tid, t] : w.tiles)
                {
                    if (t.body != body || is_water(t.substrate)
                        || !survey_tile_mapped(w, me, body, t.grid_x, t.grid_y)
                        || w.tile_to_nation.count(tid) != 0 || occupied.count(tid) != 0)
                        continue;
                    const uint32_t pid = w.provinces.province_of(tid);
                    if (pid != 0 && province_holder_for(w, pid) != null_entity)
                        continue;
                    const site s{pid, tid, t.grid_x, t.grid_y, t.habitability,
                                 survey_tile_prospected(w, me, body, t.grid_x, t.grid_y)};
                    const auto it = best.find(pid);
                    if (it == best.end() || s.hab > it->second.hab
                        || (s.hab == it->second.hab
                            && std::tie(s.row, s.col) < std::tie(it->second.row, it->second.col)))
                        best[pid] = s;
                }
                std::vector<site> sites;
                for (const auto& [pid, s] : best)
                    sites.push_back(s);
                std::stable_sort(sites.begin(), sites.end(),
                                 [](const site& a, const site& b) { return a.hab > b.hab; });
                bool chosen_listed = false;
                for (const site& s : sites)
                    chosen_listed = chosen_listed || s.tile == f.found_tile;
                if (!chosen_listed)
                    f.found_tile = null_entity; // the ground was taken, or the list moved

                ImGui::TextDisabled("Mapped ground to found on:");
                ImGui::BeginChild("##fleet_found", ImVec2(list_width(), list_height(f.found_tile != null_entity ? 2 : 0)),
                                  ImGuiChildFlags_Borders);
                for (const site& s : sites)
                {
                    // The tile names the row (a province id means nothing to the
                    // player); the state that decides the pick follows it.
                    char label[160];
                    std::snprintf(label, sizeof label, "(%d, %d)  habitability %.2f  %s%s", s.col, s.row,
                                  static_cast<double>(s.hab), s.prospected ? "prospected" : "deposits unknown",
                                  s.pid == 0 ? "  unpartitioned" : "");
                    const bool chosen = s.tile == f.found_tile;
                    ImGui::PushID(static_cast<int>(s.tile));
                    if (ImGui::Selectable(label, chosen))
                        f.found_tile = chosen ? null_entity : s.tile; // the toggle: a chosen row un-picks
                    ImGui::PopID();
                    space_fleet_ui_state::site_row r;
                    r.name = label;
                    r.x = (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f;
                    r.y = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
                    r.col = s.col;
                    r.row = s.row;
                    r.chosen = chosen;
                    r.prospected = s.prospected;
                    f.site_rows.push_back(r);
                }
                if (sites.empty())
                    ImGui::TextDisabled("No mapped, unclaimed ground here.");
                ImGui::EndChild();

                if (f.found_tile != null_entity)
                {
                    const char* why = here < need ? "too few colonists landed here"
                                    : !(kit >= bp.kit_qty) ? "too little base kit landed here"
                                                           : nullptr;
                    ImGui::BeginDisabled(why != nullptr);
                    if (ImGui::Button("Confirm found base"))
                    {
                        const entity_id tile = f.found_tile;
                        queue(space_verb::found_base, [&](space_command& c) { c.subject = tile; });
                        f.found_armed = false;
                        f.found_tile  = null_entity;
                    }
                    ImGui::EndDisabled();
                    if (why != nullptr && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                        ImGui::SetTooltip("%s", why);
                    publish("confirm_found", why == nullptr, why);
                    if (why != nullptr)
                        ImGui::TextDisabled("Refused: %s.", why);
                }
            }
        }
    }

    if (f.move_armed)
    {
        ImGui::Checkbox("Whole fleet", &f.move_fleet);
        const float budget = space_unit_dv_remaining(*u);
        ImGui::TextDisabled("Destination (" IO_DV " cost against the budget %.1f):", static_cast<double>(budget));
        // Every node but its own (a surface is a Lander's only) that a route reaches,
        // cheapest burn first: the top of the list is what the budget reaches.
        std::vector<std::pair<orbital_node, orbital_path_result>> reach;
        for (const orbital_node n : orbital_nodes(w))
        {
            if (n == u->node || (n.kind == orbital_node_kind::surface && u->hull != space_hull::lander))
                continue;
            orbital_path_result p = orbital_path(w, orbital_graph_params{}, u->node, n);
            if (p.ok)
                reach.emplace_back(n, std::move(p));
        }
        std::stable_sort(reach.begin(), reach.end(),
                         [](const auto& a, const auto& b) { return a.second.dv < b.second.dv; });
        ImGui::BeginChild("##fleet_dest", ImVec2(list_width(), list_height(f.dest_set ? 3 : 1)), ImGuiChildFlags_Borders);
        for (const auto& [n, p] : reach)
        {
            // The whole fleet is held to its weakest budget.
            bool  afford = true;
            float worst  = budget;
            for (const entity_id fid : (f.move_fleet ? fleet : std::vector<entity_id>{id}))
            {
                const space_unit& o = w.space_units.at(fid);
                const float burn = space_unit_burn_for(o, static_cast<float>(p.dv));
                afford = afford && std::isfinite(burn) && burn <= o.propellant;
                worst  = std::min(worst, space_unit_dv_remaining(o));
            }
            const bool chosen = f.dest_set && f.dest_body == n.body && f.dest_kind == static_cast<int>(n.kind);
            char label[160];
            std::snprintf(label, sizeof label, "%s   " IO_DV " %.1f%s   %.0f d", orbital_node_display_name(w, n).c_str(),
                          p.dv, afford ? "" : " (over budget)", std::ceil(p.days));
            ImGui::PushID(static_cast<int>(n.body) * 8 + static_cast<int>(n.kind));
            if (!afford) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            if (ImGui::Selectable(label, chosen))
            {
                if (chosen)
                    f.dest_set = false; // the toggle: a chosen row un-picks
                else
                {
                    f.dest_set  = true;
                    f.dest_body = n.body;
                    f.dest_kind = static_cast<int>(n.kind);
                }
            }
            if (!afford) ImGui::PopStyleColor();
            ImGui::PopID();
            space_fleet_ui_state::dest_row r;
            r.node = orbital_node_display_name(w, n);
            r.x = (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f;
            r.y = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
            r.dv = static_cast<float>(p.dv);
            r.budget = worst;
            r.affordable = afford;
            r.chosen = chosen;
            f.dest_rows.push_back(r);
            if (chosen)
            {
                f.preview_dv = r.dv;
                f.preview_budget = worst;
                f.preview_ok = afford;
            }
        }
        ImGui::EndChild();
        if (f.dest_set && f.preview_dv >= 0.0f)
        {
            ImGui::Text("Move costs " IO_DV " %.1f of %.1f.", static_cast<double>(f.preview_dv),
                        static_cast<double>(f.preview_budget));
            ImGui::BeginDisabled(!f.preview_ok);
            if (ImGui::Button("Confirm move"))
            {
                const orbital_node to{f.dest_body, static_cast<orbital_node_kind>(f.dest_kind)};
                queue(f.move_fleet ? space_verb::move_fleet : space_verb::move_unit, [&](space_command& c) {
                    c.node_body = to.body;
                    c.node_kind = static_cast<uint8_t>(to.kind);
                });
                f.move_armed = false;
                f.dest_set   = false;
            }
            ImGui::EndDisabled();
            publish("confirm_move", f.preview_ok, f.preview_ok ? nullptr : "the budget cannot pay this move");
        }
    }

    if (f.bombard_armed && can_bombard)
    {
        ImGui::TextDisabled("Ground below to strike:");
        ImGui::BeginChild("##fleet_bombard", ImVec2(list_width(), list_height(1)), ImGuiChildFlags_Borders);
        int shown = 0;
        for (const province& pr : w.provinces.provinces)
        {
            if (pr.body != u->node.body)
                continue;
            const space_bombard_order o{space_bombard_target::province, pr.id};
            const entity_id hit = space_bombard_pick(w, me, o);
            if (hit == null_entity)
                continue;
            const entity_id holder = province_holder_for(w, pr.id);
            const auto hn = w.nations.find(holder);
            const auto bit = w.buildings.find(hit);
            char label[160];
            std::snprintf(label, sizeof label, "%s ground: %s", hn != w.nations.end() ? hn->second.name.c_str() : "Hostile",
                          bit != w.buildings.end() ? building_type_name(bit->second.type) : "a building");
            ImGui::PushID(static_cast<int>(pr.id));
            if (ImGui::Selectable(label, false))
            {
                queue(space_verb::bombard, [&](space_command& c) {
                    c.target_kind = static_cast<uint8_t>(space_bombard_target::province);
                    c.target      = pr.id;
                });
                f.bombard_armed = false;
            }
            ImGui::PopID();
            space_fleet_ui_state::press p;
            p.name = label;
            p.x = (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f;
            p.y = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
            p.enabled = true;
            f.target_rows.push_back(p);
            if (++shown >= 12)
                break;
        }
        if (shown == 0)
            ImGui::TextDisabled("Nothing hostile below to strike.");
        ImGui::EndChild();
    }
    if (!ui.space.message.empty())
        ImGui::TextWrapped("%s", ui.space.message.c_str()); // wraps at the orders column
    return true;
}

void draw_space_hull_designs(const world& w, ui_state& ui)
{
    space_fleet_ui_state& f = ui.fleet;
    f.hull_rows.clear();
    f.design_presses.clear();
    const entity_id me = player_nation(w);
    const auto& hulls = space_hull_table();
    const auto& cat   = space_item_catalogue();
    for (std::size_t hi = 0; hi < space_hull_count; ++hi)
    {
        const space_hull h = static_cast<space_hull>(hi);
        const space_design* live = me != null_entity ? space_unit_design_of(w, me, h) : nullptr;
        const bool open = f.design_hull == static_cast<int>(hi);
        ImGui::PushID(static_cast<int>(hi) + 0x70000);
        // The row is the door: a toggle that expands the hull's slots in place.
        if (ImGui::Selectable(hulls[hi].name, open))
        {
            if (open)
                f.design_hull = -1;
            else
            {
                f.design_hull = static_cast<int>(hi);
                f.draft.fill(k_no_space_item);
                if (live != nullptr)
                    for (std::size_t s = 0; s < space_unit_slot_count; ++s)
                        f.draft[s] = (*live)[s];
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Press to design the %s: one item per slot. Items open as far-space research lands.",
                              hulls[hi].name);
        space_fleet_ui_state::press row;
        row.name = hulls[hi].name;
        row.x = (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f;
        row.y = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
        row.enabled = live != nullptr; // "has a live design"
        f.hull_rows.push_back(row);
        if (live != nullptr)
        {
            const space_design_stats st = evaluate_space_design(h, *live);
            ImGui::TextDisabled(IO_DV " %.1f, cargo %.0f, firepower %.0f, armour %.0f, hull %.0f",
                                static_cast<double>(st.dv_capacity), static_cast<double>(st.cargo),
                                static_cast<double>(st.firepower), static_cast<double>(st.armour),
                                static_cast<double>(st.hull_points));
        }
        else
        {
            ImGui::TextDisabled("No live design%s", hulls[hi].surface_launch ? "" : " - built at a Shipyard only");
        }
        if (open)
        {
            ImGui::Indent();
            static const char* const k_slot_names[space_unit_slot_count] = {"Drive", "Tanks", "Armament",
                                                                             "Armour", "Bay", "Debris arm"};
            for (std::size_t s = 0; s < space_unit_slot_count; ++s)
            {
                if (!space_hull_has_slot(h, static_cast<space_unit_slot>(s)))
                    continue;
                const uint8_t cur = f.draft[s];
                const char* cur_name = cur < cat.size() ? cat[cur].name : "(empty)";
                ImGui::PushID(static_cast<int>(s));
                // Leave the slot's label room at the right of a narrow ledger.
                ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x
                                                        - ImGui::CalcTextSize("Debris arm").x
                                                        - ImGui::GetStyle().ItemInnerSpacing.x));
                if (ImGui::BeginCombo(k_slot_names[s], cur_name)) // a selector: switches what fills the slot
                {
                    for (std::size_t i = 0; i < cat.size(); ++i)
                    {
                        const space_item& it = cat[i];
                        if (static_cast<std::size_t>(it.slot) != s || (it.hull_mask & (1u << hi)) == 0)
                            continue;
                        const bool held = me != null_entity && (it.key.empty() || nation_has_mechanic(w, me, it.key));
                        ImGui::BeginDisabled(!held);
                        if (ImGui::Selectable(it.name, cur == i))
                            f.draft[s] = static_cast<uint8_t>(i);
                        ImGui::EndDisabled();
                        if (!held && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                            ImGui::SetTooltip("Not yet researched.");
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopID();
            }
            space_design d{};
            for (std::size_t s = 0; s < space_unit_slot_count; ++s)
                d[s] = f.draft[s];
            const space_design_stats st = evaluate_space_design(h, d);
            if (st.valid)
                ImGui::TextDisabled("This design: " IO_DV " %.1f, cargo %.0f, firepower %.0f, armour %.0f",
                                    static_cast<double>(st.dv_capacity), static_cast<double>(st.cargo),
                                    static_cast<double>(st.firepower), static_cast<double>(st.armour));
            else
                ImGui::TextDisabled("Fill every slot to save.");
            const bool hull_held = me != null_entity && (hulls[hi].key.empty() || nation_has_mechanic(w, me, hulls[hi].key));
            const bool can_save = st.valid && hull_held && !ui.spectating;
            ImGui::BeginDisabled(!can_save);
            if (ImGui::Button("Save design"))
            {
                space_command c;
                c.nation = me;
                c.verb   = static_cast<uint8_t>(space_verb::set_unit_design);
                c.cls    = static_cast<uint8_t>(hi);
                for (std::size_t s = 0; s < space_unit_slot_count; ++s)
                    c.design[s] = f.draft[s];
                ui.space.pending.push_back(c);
            }
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip(!hull_held ? "This hull is not yet researched."
                                  : !st.valid ? "Every slot must hold an item." : "Make this the hull's live design.");
            space_fleet_ui_state::press save;
            save.name = "save";
            save.x = (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f;
            save.y = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
            save.enabled = can_save;
            save.reason = !hull_held ? "hull not researched" : !st.valid ? "a slot is empty" : "";
            f.design_presses.push_back(save);
            if (!ui.space.message.empty())
                ImGui::TextWrapped("%s", ui.space.message.c_str());
            ImGui::Unindent();
        }
        ImGui::PopID();
        ImGui::Spacing();
    }
}

} // namespace ui
