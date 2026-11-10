#include "circum_assets.hpp"

#include "highlight.hpp"
#include "hover_card.hpp"
#include "icons.hpp"
#include "presentation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ui {

namespace {

// Constant pixel sizes (CIRCUMPLANETARY.md § Orbital nodes): none scale with zoom.
constexpr float k_sat_r        = 5.5f;  ///< A satellite plate's radius.
constexpr float k_station_r0   = 8.5f;  ///< A level-0 station's half-extent...
constexpr float k_station_step = 2.0f;  ///< ...and what each grid ring adds.
constexpr float k_gap          = 3.0f;  ///< Between neighbouring marks.
constexpr float k_hit_pad      = 2.0f;

const char* role_name(space_satellite_role r)
{
    switch (r)
    {
        case space_satellite_role::comms:       return "Comms";
        case space_satellite_role::navigation:  return "Navigation";
        case space_satellite_role::observation: return "Observation";
        case space_satellite_role::survey:      return "Survey";
        case space_satellite_role::none:        break;
    }
    return "";
}

/// How wide a mark is along its row: a satellite's wings, a station's panels.
float mark_half_width(const circum_asset_mark& m)
{
    return m.station ? m.r : m.r * 1.55f;
}

const space_asset* find_asset(const world& w, entity_id id)
{
    const auto it = w.space_assets.find(id);
    return it != w.space_assets.end() ? &it->second : nullptr;
}

std::string node_name_or(const world& w, const orbital_node& n)
{
    return orbital_node_display_name(w, n);
}

} // namespace

std::vector<circum_asset_mark> layout_circum_assets(const world& w,
                                                    const std::vector<circum_node_mark>& nodes)
{
    std::vector<circum_asset_mark> out;
    if (w.space_assets.empty())
        return out;

    for (std::size_t ni = 0; ni < nodes.size(); ++ni)
    {
        const circum_node_mark& nm = nodes[ni];
        std::vector<circum_asset_mark> here;

        // Stations first (std::map: ascending id). A station's id is its core's.
        for (const auto& [sid, st] : w.stations)
        {
            if (!(st.node == nm.node))
                continue;
            const space_asset* core = find_asset(w, st.core);
            if (core == nullptr || core->state == space_asset_state::lost)
                continue;
            circum_asset_mark m;
            m.id         = sid;
            m.owner      = st.owner;
            m.station    = true;
            m.in_transit = core->state == space_asset_state::in_transit;
            m.node_index = static_cast<int>(ni);
            m.r          = k_station_r0 + k_station_step * static_cast<float>(st.level);
            here.push_back(m);
        }
        for (const auto& [aid, a] : w.space_assets)
        {
            if (a.cls != space_asset_class::satellite || a.state == space_asset_state::lost
                || !(a.node == nm.node))
                continue;
            circum_asset_mark m;
            m.id         = aid;
            m.owner      = a.owner;
            m.role       = a.role;
            m.in_transit = a.state == space_asset_state::in_transit;
            m.node_index = static_cast<int>(ni);
            m.r          = k_sat_r;
            here.push_back(m);
        }
        if (here.empty())
            continue;

        if (nm.ring_r > 0.0f)
        {
            // Down the ring's side from its equator: the low orbit's right, the
            // high orbit's left (the pip rows ride the top, the labels the foot).
            const bool  low  = nm.node.kind == orbital_node_kind::low_orbit;
            const float sgn  = low ? 1.0f : -1.0f;
            float       arc  = 0.0f; // px walked along the ring
            for (std::size_t i = 0; i < here.size(); ++i)
            {
                const float hw = mark_half_width(here[i]);
                if (i > 0)
                    arc += mark_half_width(here[i - 1]) + k_gap + hw;
                const float th = arc / nm.ring_r;
                here[i].pos = { nm.pos.x + sgn * std::cos(th) * nm.ring_r,
                                nm.pos.y + std::sin(th) * nm.ring_r };
            }
        }
        else
        {
            // A row centred below the diamond.
            float total = 0.0f, tallest = 0.0f;
            for (std::size_t i = 0; i < here.size(); ++i)
            {
                total  += 2.0f * mark_half_width(here[i]) + (i > 0 ? k_gap : 0.0f);
                tallest = std::max(tallest, here[i].r);
            }
            float x = nm.pos.x - total * 0.5f;
            const float y = nm.pos.y + 5.0f + 3.0f + tallest;
            for (circum_asset_mark& m : here)
            {
                const float hw = mark_half_width(m);
                m.pos = { x + hw, y };
                x += 2.0f * hw + k_gap;
            }
        }
        out.insert(out.end(), here.begin(), here.end());
    }
    return out;
}

void draw_circum_assets(ImDrawList* dl, const std::vector<circum_asset_mark>& marks,
                        entity_id hovered, entity_id selected)
{
    for (const circum_asset_mark& m : marks)
    {
        const ImU32 fill = palette::nation_colour(m.owner);
        if (m.station)
            icons::station_marker(dl, m.pos, m.r, fill, m.in_transit);
        else
            icons::satellite_marker(dl, m.pos, m.r, m.role, fill, m.in_transit);
        const highlight h = resolve_highlight(m.id == selected, m.id == hovered, false);
        draw_body_highlight(dl, m.pos, m.r + 1.0f, h);
    }
}

int hit_circum_asset(const std::vector<circum_asset_mark>& marks, ImVec2 mouse)
{
    int   best   = -1;
    float best_d = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < marks.size(); ++i)
    {
        const circum_asset_mark& m = marks[i];
        const float dx = mouse.x - m.pos.x, dy = mouse.y - m.pos.y;
        const float d  = std::sqrt(dx * dx + dy * dy);
        if (d <= m.r + k_hit_pad && d < best_d)
        {
            best   = static_cast<int>(i);
            best_d = d;
        }
    }
    return best;
}

std::string space_asset_title(const world& w, entity_id id)
{
    if (w.stations.count(id))
        return "Station";
    const space_asset* a = find_asset(w, id);
    if (a == nullptr || a->cls != space_asset_class::satellite)
        return {};
    const char* role = role_name(a->role);
    return role[0] != '\0' ? std::string(role) + " satellite" : std::string("Satellite");
}

void draw_space_asset_summary(const world& w, entity_id id)
{
    const auto sit = w.stations.find(id);
    const space_station* st = sit != w.stations.end() ? &sit->second : nullptr;
    const space_asset* a = find_asset(w, st ? st->core : id);
    if (a == nullptr)
        return;

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s",
                       space_asset_title(w, id).c_str());

    const auto nit = w.nations.find(a->owner);
    ImGui::TextUnformatted("Owner:");
    ImGui::SameLine();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::nation_colour(a->owner)), "%s",
                       nit != w.nations.end() ? nit->second.name.c_str() : "?");

    if (st != nullptr)
    {
        int modules = 0, crew = 0;
        for (std::size_t s = 0; s < st->slots.size(); ++s)
        {
            const entity_id part = st->slots[s];
            if (part == null_entity)
                continue;
            if (static_cast<int>(s) != st->centre())
                ++modules;
            if (const space_asset* pa = find_asset(w, part))
                crew += pa->crew;
        }
        const int room = std::max(0, st->width() * st->height() - 1);
        ImGui::Text("Modules: %d of %d (%dx%d grid)", modules, room, st->width(), st->height());
        ImGui::Text("Crew: %d", crew);
    }
    else
    {
        ImGui::Text("Role: %s", role_name(a->role));
    }

    const std::string node = node_name_or(w, a->node);
    switch (a->state)
    {
        case space_asset_state::in_transit:
        {
            const int q = std::max(0, a->arrival_tick - w.current_econ_tick);
            ImGui::Text("Bound for: %s", node.c_str());
            if (q == 0)
                ImGui::TextUnformatted("State: in transit, arrives this quarter");
            else
                ImGui::Text("State: in transit, arrives in %d quarter%s", q, q == 1 ? "" : "s");
            break;
        }
        case space_asset_state::active:
            ImGui::Text("Node: %s", node.c_str());
            ImGui::TextUnformatted("State: in place");
            break;
        case space_asset_state::decaying:
            ImGui::Text("Node: %s", node.c_str());
            ImGui::Text("State: decaying, upkeep unpaid %d quarter%s", a->unpaid_quarters,
                        a->unpaid_quarters == 1 ? "" : "s");
            break;
        case space_asset_state::lost:
            ImGui::TextDisabled("State: lost");
            break;
    }
}

void draw_circum_asset_card(const world& w, entity_id id, ImVec2 mouse)
{
    draw_hover_card(mouse, [&]() { draw_space_asset_summary(w, id); });
}

bool draw_space_asset_selection(const world& w, const ui_state& ui)
{
    const entity_id id = ui.selected_entity;
    if (space_asset_title(w, id).empty())
        return false;
    draw_space_asset_summary(w, id);
    return true;
}

} // namespace ui
