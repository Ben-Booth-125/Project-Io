#include "space_ledger.hpp"

#include "circum_nodes.hpp"   // orbital_node_display_name
#include "foldout_column.hpp"
#include "format.hpp"
#include "presentation.hpp"

#include "world/nation_tech.hpp"    // nation_has_mechanic — the module, role and upgrade keys
#include "world/orbital_graph.hpp"
#include "world/space_coverage.hpp"
#include "world/space_tech_keys.hpp"
#include "world/stations.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace ui {

namespace {

// ── Names (docs/economy/SPACE_ASSETS.md's words) ────────────────────────────

/// The class picker's order: satellite, station core, crewed mission, rover.
constexpr int k_launch_classes = 4;
const char* const k_class_label[k_launch_classes] = {"Satellite", "Station core", "Crewed mission", "Rover"};

space_asset_class launch_class_of(int pick)
{
    switch (pick)
    {
        case 1:  return space_asset_class::station_core;
        case 2:  return space_asset_class::crewed_mission;
        case 3:  return space_asset_class::rover;
        default: return space_asset_class::satellite;
    }
}

const char* class_name(space_asset_class c)
{
    switch (c)
    {
        case space_asset_class::satellite:      return "Satellite";
        case space_asset_class::rover:          return "Rover";
        case space_asset_class::crewed_mission: return "Crewed mission";
        case space_asset_class::station_module: return "Station module";
        case space_asset_class::station_core:   return "Station";
    }
    return "?";
}

const char* role_name(int role)
{
    switch (role)
    {
        case 1: return "Comms";
        case 2: return "Navigation";
        case 3: return "Observation";
        case 4: return "Survey";
        default: return "-";
    }
}

std::string_view role_key(int role)
{
    switch (role)
    {
        case 1: return space_keys::role_comms;
        case 2: return space_keys::role_navigation;
        case 3: return space_keys::role_observation;
        default: return space_keys::role_survey;
    }
}

const char* module_name(int k)
{
    static const char* const names[station_module_kind_count] = {
        "Research", "Defence", "Power", "Dock", "Habitation", "Life support", "Fuel depot",
        "Shipyard", "Microgravity factory", "Observatory", "Comms relay", "Cargo hold", "Market", "Truss"};
    return (k >= 0 && k < static_cast<int>(station_module_kind_count)) ? names[k] : "?";
}

/// The grid cell's short tag: the slot is ~40-56 px, too narrow for the name.
const char* module_tag(int k)
{
    static const char* const tags[station_module_kind_count] = {
        "Res", "Def", "Pwr", "Dock", "Hab", "LS", "Fuel", "Yard", "Fac", "Obs", "Rly", "Crg", "Mkt", "Tr"};
    return (k >= 0 && k < static_cast<int>(station_module_kind_count)) ? tags[k] : "?";
}

/// The key a fittable module kind needs (the station layer's own table, stations.cpp).
std::string_view module_key(int k)
{
    switch (static_cast<station_module_kind>(k))
    {
        case station_module_kind::research:             return space_keys::module_research;
        case station_module_kind::defence:              return space_keys::module_defence;
        case station_module_kind::power:                return space_keys::module_power;
        case station_module_kind::dock:                 return space_keys::module_dock;
        case station_module_kind::habitation:           return space_keys::module_habitation;
        case station_module_kind::life_support:         return space_keys::module_life_support;
        case station_module_kind::fuel_depot:           return space_keys::module_fuel_depot;
        case station_module_kind::shipyard:             return space_keys::module_shipyard;
        case station_module_kind::microgravity_factory: return space_keys::module_microgravity_factory;
        case station_module_kind::observatory:          return space_keys::module_observatory;
        case station_module_kind::comms_relay:          return space_keys::module_comms_relay;
        case station_module_kind::cargo_hold:           return space_keys::module_cargo_hold;
        case station_module_kind::market:               return space_keys::module_market;
        default:                                        return {};
    }
}

const char* launch_refusal(space_launch_result r)
{
    switch (r)
    {
        case space_launch_result::launched:                       return "";
        case space_launch_result::rejected_invalid:               return "That launch is not valid.";
        case space_launch_result::rejected_unknown_nation:        return "You hold no nation.";
        case space_launch_result::rejected_unknown_body:          return "Your nation has no home body.";
        case space_launch_result::rejected_illegal_destination:   return "This class cannot go to that node.";
        case space_launch_result::rejected_insufficient_treasury: return "The treasury cannot pay for it.";
        case space_launch_result::rejected_tech_locked:           return "Not researched yet - Rocketry, or this payload's own technology.";
        case space_launch_result::rejected_no_launchpad:          return "No staffed Launchpad on your home body.";
        case space_launch_result::rejected_no_slot:               return "Every slot at that node is held.";
        case space_launch_result::rejected_no_route:              return "No path reaches that node from your home surface.";
        case space_launch_result::rejected_shelf:                 return "The home market's shelf cannot fill the goods.";
    }
    return "Refused.";
}

const char* station_refusal(station_result r)
{
    switch (r)
    {
        case station_result::ok:                        return "";
        case station_result::rejected_invalid:          return "That order is not valid.";
        case station_result::rejected_unknown_nation:   return "You hold no nation.";
        case station_result::rejected_unknown_station:  return "That station is gone.";
        case station_result::rejected_not_owner:        return "That station is not yours.";
        case station_result::rejected_slot_occupied:    return "That slot is already filled.";
        case station_result::rejected_slot_empty:       return "Nothing is in that slot.";
        case station_result::rejected_edge_only:        return "Needs an edge slot.";
        case station_result::rejected_no_crew:          return "Not enough astronauts to crew it.";
        case station_result::rejected_launch:           return "The launch was refused.";
        case station_result::rejected_max_level:        return "The grid is at its largest.";
        case station_result::rejected_not_in_place:     return "The module has not arrived.";
        case station_result::rejected_unknown_template: return "No template by that name.";
        case station_result::rejected_template_full:    return "You hold the most templates you may.";
        case station_result::rejected_tech_locked:      return "Not researched yet.";
    }
    return "Refused.";
}

const char* verb_name(uint8_t v)
{
    switch (static_cast<space_verb>(v))
    {
        case space_verb::launch:           return "Launch";
        case space_verb::found_station:    return "Found station";
        case space_verb::fit_module:       return "Fit module";
        case space_verb::remove_module:    return "Remove module";
        case space_verb::refit_module:     return "Move module";
        case space_verb::apply_template:   return "Apply template";
        case space_verb::save_template:    return "Save template";
        case space_verb::delete_template:  return "Delete template";
        case space_verb::upgrade_station:  return "Upgrade station";
        case space_verb::retask_satellite: return "Retask satellite";
        case space_verb::move_satellite:   return "Move satellite";
        case space_verb::found_base:       return "Found base";       // BL-1601
        case space_verb::embark_colonists: return "Embark colonists"; // BL-1601
    }
    return "Order";
}

// ── Helpers ─────────────────────────────────────────────────────────────────

/// The space-asset dials as the seam prices them: the fair-price ceiling copied in.
space_asset_params seam_params(const recipe_registry& reg)
{
    space_asset_params sp = reg.space_assets();
    sp.reservation_mult = reg.price_band().reservation_mult;
    return sp;
}

void centre_of_last_item(float& x, float& y)
{
    x = (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f;
    y = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
}

void hover_reason(const std::string& reason)
{
    if (!reason.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", reason.c_str());
}

/// A press: a button, disabled with its reason on hover when @p enabled is false.
/// Publishes where it landed into @p out. Returns true on a live press.
bool press(const char* label, bool enabled, const std::string& reason, space_ledger_state::drawn_press& out)
{
    if (!enabled)
        ImGui::BeginDisabled();
    const bool hit = ImGui::Button(label);
    if (!enabled)
        ImGui::EndDisabled();
    hover_reason(reason);
    centre_of_last_item(out.x, out.y);
    out.enabled = enabled;
    out.reason  = reason;
    return hit && enabled;
}

/// SameLine, unless the next button (@p label) would run past the column's edge.
void same_line_if_fits(const char* label)
{
    const ImGuiStyle& st = ImGui::GetStyle();
    const float next = ImGui::CalcTextSize(label, nullptr, true).x + st.FramePadding.x * 2.0f;
    const float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    if (ImGui::GetItemRectMax().x + st.ItemSpacing.x + next <= right)
        ImGui::SameLine();
}

int pending_for(const world& w, entity_id nation, entity_id subject)
{
    int n = 0;
    for (const space_order& o : w.space_orders)
        if (o.cmd.nation == nation && (subject == null_entity || o.cmd.subject == subject))
            ++n;
    return n;
}

std::vector<entity_id> player_stations(const world& w, entity_id nation)
{
    std::vector<entity_id> out;
    for (const auto& [id, st] : w.stations) // std::map: ascending
        if (st.owner == nation)
            out.push_back(id);
    return out;
}

// ── The pending line ────────────────────────────────────────────────────────

void draw_pending_line(const world& w, entity_id nation, space_ledger_state& m)
{
    m.pending_orders = pending_for(w, nation, null_entity);
    if (m.pending_orders == 0)
    {
        ImGui::TextDisabled("No orders waiting on a signal.");
        return;
    }
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::station_idle),
                       "%d order%s pending - no signal", m.pending_orders, m.pending_orders == 1 ? "" : "s");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted("Outside comms coverage: each waits a quarter.");
        for (const space_order& o : w.space_orders)
            if (o.cmd.nation == nation)
                ImGui::Text("%s - due quarter %d", verb_name(o.cmd.verb), o.due_tick);
        ImGui::EndTooltip();
    }
}

// ── Launch ──────────────────────────────────────────────────────────────────

void draw_launch(const world& w, const recipe_registry& reg, entity_id nation, space_ledger_state& m)
{
    const space_asset_params sp = seam_params(reg);
    const entity_id home = space_home_body_of(w, nation);

    // The class picker: a SELECTOR — re-pressing the active class does nothing.
    // The buttons wrap to a second line rather than run off the column.
    for (int c = 0; c < k_launch_classes; ++c)
    {
        char label[48];
        std::snprintf(label, sizeof(label), "%s##cls", k_class_label[c]);
        if (c > 0)
            same_line_if_fits(label);
        const int before = m.launch_class;
        ui::nav_button(label, c, m.launch_class);
        space_ledger_state::drawn_press p;
        centre_of_last_item(p.x, p.y);
        p.enabled = true;
        p.id = c;
        m.class_presses.push_back(p);
        if (m.launch_class != before)
            m.launch_node_set = false; // a different class may not go where the last one could
    }
    const space_asset_class cls = launch_class_of(m.launch_class);

    if (cls == space_asset_class::satellite)
    {
        for (int r = 1; r < static_cast<int>(space_satellite_role_count); ++r)
        {
            char label[32];
            std::snprintf(label, sizeof(label), "%s##role", role_name(r));
            if (r > 1)
                same_line_if_fits(label);
            ui::nav_button(label, r, m.launch_role);
            space_ledger_state::drawn_press p;
            centre_of_last_item(p.x, p.y);
            p.id      = r;
            p.enabled = nation_has_mechanic(w, nation, role_key(r));
            if (!p.enabled)
                p.reason = "Not researched yet.";
            hover_reason(p.reason.empty() ? std::string("The satellite's role.") : p.reason);
            m.role_presses.push_back(p);
        }
        if (m.launch_role < 1 || m.launch_role >= static_cast<int>(space_satellite_role_count))
            m.launch_role = 1;
    }

    // The destination picker: every node the class may go to, with free slots
    // (a slot-holding class lists only nodes with one free), the path's thrust
    // cost and its days from the home surface.
    std::vector<orbital_node> legal;
    const bool holds_slot = space_class_holds_slot(cls);
    for (const orbital_node& n : orbital_nodes(w))
    {
        if (!space_launch_legal_node(cls, n.kind, n.body == home))
            continue;
        if (holds_slot && orbital_slot_capacity(w, n) - orbital_slots_held(w, n) <= 0)
            continue;
        legal.push_back(n);
    }
    // The home body's nodes first — the nearest and cheapest — then the rest in node order.
    std::stable_partition(legal.begin(), legal.end(), [home](const orbital_node& n) { return n.body == home; });
    const orbital_node chosen{m.launch_body, static_cast<orbital_node_kind>(m.launch_kind)};
    if (m.launch_node_set && std::find(legal.begin(), legal.end(), chosen) == legal.end())
        m.launch_node_set = false;

    // The quote and the press come FIRST, under the pickers, so the act stays on
    // screen in the column's short height; the list it is chosen from follows.
    const space_satellite_role role = cls == space_asset_class::satellite
                                          ? static_cast<space_satellite_role>(m.launch_role)
                                          : space_satellite_role::none;
    std::string reason = "Choose a destination first.";
    space_launch_quote q;
    int crew = 0;
    if (m.launch_node_set)
    {
        q = quote_space_launch(w, sp, nation, cls, chosen, role);
        reason = launch_refusal(q.result);
        if (cls == space_asset_class::station_core)
        {
            crew = reg.stations().core_crew_needed;
            const station_op op = quote_found_station(w, sp, reg.stations(), nation, chosen);
            if (op.result != station_result::ok && op.result != station_result::rejected_launch)
                reason = station_refusal(op.result);
        }
    }
    const bool ok = reason.empty();
    if (press("Launch##go", ok, reason, m.launch_press)) // ##go: the tab is "Launch" too
    {
        space_command c;
        c.nation    = nation;
        c.node_body = chosen.body;
        c.node_kind = static_cast<uint8_t>(chosen.kind);
        if (cls == space_asset_class::station_core)
            c.verb = static_cast<uint8_t>(space_verb::found_station);
        else
        {
            c.verb = static_cast<uint8_t>(space_verb::launch);
            c.cls  = static_cast<uint8_t>(cls);
            c.role = static_cast<uint8_t>(role);
        }
        m.pending.push_back(c);
    }
    ImGui::SameLine();
    const bool priced = m.launch_node_set
        && (q.result == space_launch_result::launched || q.result == space_launch_result::rejected_insufficient_treasury
            || q.result == space_launch_result::rejected_shelf || q.cost > 0.0f);
    if (!m.launch_node_set)
        ImGui::TextDisabled("Choose a destination.");
    else if (priced)
    {
        m.launch_cost = q.cost;
        ImGui::Text("%s to %s", fmt::credits(q.cost).c_str(), orbital_node_display_name(w, chosen).c_str());
    }
    else
        ImGui::Text("To %s", orbital_node_display_name(w, chosen).c_str());
    if (m.launch_node_set && !ok)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(palette::negative));
        ImGui::TextWrapped("%s", reason.c_str());
        ImGui::PopStyleColor();
    }
    if (priced)
    {
        // The quote's parts, wrapped to the column: goods off the home shelf, the
        // launch and thrust credits, the travel time and the crew drawn.
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        char crew_part[48] = "";
        if (crew > 0)
            std::snprintf(crew_part, sizeof(crew_part), ", %d astronaut%s aboard", crew, crew == 1 ? "" : "s");
        ImGui::TextWrapped("Goods %s (%.0f components, %.0f propellant); launch and thrust %s; %d qtr (%.0f days)%s.",
                           fmt::credits(q.goods_value).c_str(), static_cast<double>(q.components),
                           static_cast<double>(q.propellant), fmt::credits(q.credits).c_str(), q.travel_quarters,
                           q.days, crew_part);
        ImGui::PopStyleColor();
    }

    const orbital_node home_surface{home, orbital_node_kind::surface};
    const float row_h = ImGui::GetTextLineHeightWithSpacing();
    if (legal.empty())
    {
        ImGui::TextDisabled(holds_slot ? "No node this class may go to has a free slot."
                                       : "No node this class may go to.");
        return;
    }
    if (ImGui::BeginChild("##space_nodes", {0.0f, row_h * 6.5f}, ImGuiChildFlags_Borders)
        && ImGui::BeginTable("##space_node_table", 3, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("node", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("slots");
        ImGui::TableSetupColumn("path");
        ImGuiListClipper clip;
        clip.Begin(static_cast<int>(legal.size()));
        while (clip.Step())
            for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i)
            {
                const orbital_node& n = legal[static_cast<std::size_t>(i)];
                const bool is_chosen = m.launch_node_set && n == chosen;
                const std::string name = orbital_node_display_name(w, n);
                char label[160];
                std::snprintf(label, sizeof(label), "%s##node%d", name.c_str(), i);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                // The node row is a toggle: pressing the chosen one un-chooses it.
                if (ImGui::Selectable(label, is_chosen, ImGuiSelectableFlags_SpanAllColumns))
                {
                    if (is_chosen)
                        m.launch_node_set = false;
                    else
                    {
                        m.launch_node_set = true;
                        m.launch_body     = n.body;
                        m.launch_kind     = static_cast<int>(n.kind);
                    }
                }
                space_ledger_state::drawn_node d;
                d.x = ImGui::GetItemRectMin().x + 30.0f; // the row's left part: the node's name
                d.y = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
                d.body     = n.body;
                d.kind     = static_cast<int>(n.kind);
                d.capacity = orbital_slot_capacity(w, n);
                d.free     = d.capacity - orbital_slots_held(w, n);
                d.chosen   = is_chosen;
                m.node_rows.push_back(d);
                ImGui::TableNextColumn();
                if (holds_slot)
                {
                    ImGui::Text("%d/%d", d.free, d.capacity);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%d of the node's %d slots free.", d.free, d.capacity);
                }
                else
                    ImGui::TextDisabled("-");
                ImGui::TableNextColumn();
                const orbital_path_result path = orbital_path(w, orbital_graph_params{}, home_surface, n);
                if (path.ok)
                    ImGui::TextDisabled("\xE2\x88\x86v %.1f, %.0f d", path.dv, path.days);
                else
                    ImGui::TextDisabled("no route");
            }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

// ── Assets ──────────────────────────────────────────────────────────────────

void draw_assets(const world& w, entity_id nation, space_ledger_state& m)
{
    int rows = 0;
    for (const auto& [id, a] : w.space_assets) // std::map: ascending
    {
        if (a.owner != nation || a.state == space_asset_state::lost
            || a.cls == space_asset_class::station_module)
            continue;
        ++rows;
        const bool covered = space_node_covered(w, nation, a.node);
        const int  waiting = pending_for(w, nation, id);
        char label[200];
        const std::string node = orbital_node_display_name(w, a.node);
        if (a.cls == space_asset_class::satellite)
            std::snprintf(label, sizeof(label), "%s (%s) - %s##asset%u", class_name(a.cls),
                          role_name(static_cast<int>(a.role)), node.c_str(), static_cast<unsigned>(id));
        else
            std::snprintf(label, sizeof(label), "%s - %s##asset%u", class_name(a.cls), node.c_str(),
                          static_cast<unsigned>(id));
        const bool expanded = m.expanded == id;
        if (ImGui::Selectable(label, expanded))
        {
            if (a.cls == space_asset_class::station_core)
            {
                m.station = id; // the designer for this station
                m.slot    = -1;
                m.moving  = false;
                m.view    = 2;
            }
            else
                m.expanded = expanded ? null_entity : id; // expand in place, a toggle
        }
        space_ledger_state::drawn_asset d;
        centre_of_last_item(d.x, d.y);
        d.id = id;
        d.cls = static_cast<int>(a.cls);
        d.role = static_cast<int>(a.role);
        d.state = static_cast<int>(a.state);
        d.covered = covered;
        d.pending = waiting > 0;
        m.asset_rows.push_back(d);

        // The row's second line: state, upkeep, coverage, a pending order.
        const char* state = a.state == space_asset_state::in_transit ? "in transit"
                          : a.state == space_asset_state::decaying   ? "decaying"
                                                                     : "active";
        ImGui::Indent();
        if (a.state == space_asset_state::in_transit)
        {
            const int to_go = a.arrival_tick - w.current_econ_tick;
            if (to_go <= 0)
                ImGui::TextDisabled("in transit, arrives this quarter");
            else
                ImGui::TextDisabled("in transit, %d qtr to go", to_go);
        }
        else
            ImGui::TextDisabled("%s", state);
        ImGui::SameLine();
        ImGui::TextDisabled("| %s/qtr |", fmt::credits(a.upkeep).c_str());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Upkeep per quarter, paid from the treasury.");
        ImGui::SameLine();
        if (covered)
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::positive), "in coverage");
        else
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::station_idle), "no signal");
        if (waiting > 0)
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::station_idle),
                               "pending - no signal (%d)", waiting);

        if (expanded && a.cls == space_asset_class::satellite)
        {
            ImGui::TextDisabled("Retask");
            for (int r = 1; r < static_cast<int>(space_satellite_role_count); ++r)
            {
                if (r > 1)
                    ImGui::SameLine();
                std::string why;
                if (r == static_cast<int>(a.role))
                    why = "It already holds this role.";
                else if (!nation_has_mechanic(w, nation, role_key(r)))
                    why = "Not researched yet.";
                char rl[48];
                std::snprintf(rl, sizeof(rl), "%s##retask%d", role_name(r), r);
                space_ledger_state::drawn_press p;
                p.id = r;
                if (press(rl, why.empty(), why, p))
                {
                    space_command c;
                    c.nation  = nation;
                    c.verb    = static_cast<uint8_t>(space_verb::retask_satellite);
                    c.subject = id;
                    c.role    = static_cast<uint8_t>(r);
                    m.pending.push_back(c);
                }
                m.retask_presses.push_back(p);
            }
        }
        ImGui::Unindent();
    }
    if (rows == 0)
        ImGui::TextDisabled("Nothing in space yet. Launch from the Launch view.");
}

// ── Stations: the designer ──────────────────────────────────────────────────

const char* slot_state_of(const station_budget& b, entity_id a)
{
    const auto has = [a](const std::vector<entity_id>& v) { return std::find(v.begin(), v.end(), a) != v.end(); };
    if (has(b.working))   return "working";
    if (has(b.stopped))   return "stopped";
    if (has(b.idle_edge)) return "idle_edge";
    if (has(b.refitting)) return "refitting";
    if (has(b.not_ready)) return "not_ready";
    return "";
}

/// One budget, supply / demand, red when short. Two sit on a line: the second
/// starts at half the column so the four read as a grid.
void draw_budget_cell(const char* what, double supply, double demand, bool integral, bool second, const char* tip)
{
    if (second)
        ImGui::SameLine(ImGui::GetContentRegionMax().x * 0.5f);
    const bool short_ = supply + 1e-6 < demand;
    const ImVec4 col = ImGui::ColorConvertU32ToFloat4(short_ ? palette::negative : palette::neutral);
    if (integral)
        ImGui::TextColored(col, "%s %.0f / %.0f", what, supply, demand);
    else
        ImGui::TextColored(col, "%s %.1f / %.1f", what, supply, demand);
    if (tip != nullptr && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tip);
}

void draw_designer(const world& w, const recipe_registry& reg, entity_id nation, const space_station& st,
                   space_ledger_state& m)
{
    const station_params&    stp = reg.stations();
    const space_asset_params sp  = seam_params(reg);
    const station_budget b = evaluate_station_budget(w, stp, st.id, w.current_econ_tick);

    // The budgets, supply against demand, above the grid.
    m.budget.drawn        = true;
    m.budget.power_supply = b.power_supply;
    m.budget.power_draw   = b.power_draw;
    m.budget.crew_room    = b.crew_room;
    m.budget.life_support = b.life_support;
    m.budget.crew_aboard  = b.crew_aboard;
    m.budget.crew_needed  = b.crew_needed;
    m.budget.working      = static_cast<int>(b.working.size());
    m.budget.stopped      = static_cast<int>(b.stopped.size());
    m.budget.idle_edge    = static_cast<int>(b.idle_edge.size());
    char power_tip[96];
    std::snprintf(power_tip, sizeof(power_tip), "Power supplied / drawn. Solar power here is %.0f%% of the home world's.",
                  100.0 * static_cast<double>(b.solar_factor));
    draw_budget_cell("Power", b.power_supply, b.power_draw, false, false, power_tip);
    draw_budget_cell("Crew room", b.crew_room, b.crew_aboard, true, true, "Crew room / crew aboard.");
    draw_budget_cell("Life support", b.life_support, b.crew_aboard, true, false, "Life support / crew aboard.");
    draw_budget_cell("Crew", b.crew_aboard, b.crew_needed, true, true, "Crew aboard / crew the working modules need.");
    if (!b.core_in_place)
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::station_idle),
                           "The core is in transit: nothing works until it arrives.");

    // The grid, drawn as it is.
    const int gw = st.width(), gh = st.height();
    m.grid_w = gw;
    m.grid_h = gh;
    if (m.slot >= static_cast<int>(st.slots.size()))
    {
        m.slot   = -1;
        m.moving = false;
    }
    const float gap  = 4.0f;
    const float cell = std::clamp((ImGui::GetContentRegionAvail().x - gap * static_cast<float>(gw - 1))
                                      / static_cast<float>(gw), 24.0f, 44.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::Spacing();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    for (int row = 0; row < gh; ++row)
        for (int col = 0; col < gw; ++col)
        {
            const int idx = row * gw + col;
            const ImVec2 p0{origin.x + static_cast<float>(col) * (cell + gap),
                            origin.y + static_cast<float>(row) * (cell + gap)};
            const ImVec2 p1{p0.x + cell, p0.y + cell};
            ImGui::SetCursorScreenPos(p0);
            char id[24];
            std::snprintf(id, sizeof(id), "##slot%d", idx);
            const bool hit = ImGui::InvisibleButton(id, {cell, cell});

            const entity_id a  = st.slots[static_cast<std::size_t>(idx)];
            const bool core    = idx == st.centre();
            const bool edge    = station_slot_on_edge(st.level, idx);
            space_ledger_state::drawn_slot d;
            d.idx  = idx;
            d.core = core;
            d.edge = edge;
            d.x    = (p0.x + p1.x) * 0.5f;
            d.y    = (p0.y + p1.y) * 0.5f;
            int kind = -1;
            if (!core && a != null_entity)
                if (const auto ait = w.space_assets.find(a); ait != w.space_assets.end())
                    kind = ait->second.module == k_no_station_module ? -1 : ait->second.module;
            d.kind  = kind;
            d.state = (core || a == null_entity) ? "" : slot_state_of(b, a);
            m.slots.push_back(d);

            // Fill: empty, module, core. A stopped or idle module is outlined in its state's colour.
            const ImU32 fill = core ? palette::station_core : kind >= 0 ? palette::station_module : palette::station_slot;
            dl->AddRectFilled(p0, p1, fill, 3.0f);
            if (edge)
                dl->AddRect(p0, p1, palette::station_edge, 3.0f, 0, 1.0f);
            if (d.state == "stopped")
                dl->AddRect(p0, p1, palette::station_stopped, 3.0f, 0, 2.5f);
            else if (d.state == "idle_edge")
                dl->AddRect(p0, p1, palette::station_idle, 3.0f, 0, 2.5f);
            if (idx == m.slot)
                dl->AddRect({p0.x - 2.0f, p0.y - 2.0f}, {p1.x + 2.0f, p1.y + 2.0f}, palette::selection, 3.0f, 0, 2.0f);
            else if (ImGui::IsItemHovered())
                dl->AddRect(p0, p1, palette::hover, 3.0f, 0, 1.5f);
            const char* tag = core ? "Core" : kind >= 0 ? module_tag(kind) : "";
            if (tag[0] != '\0')
            {
                const ImVec2 ts = ImGui::CalcTextSize(tag);
                const bool dim = d.state == "not_ready" || d.state == "refitting";
                dl->AddText({d.x - ts.x * 0.5f, d.y - ts.y * 0.5f},
                            core ? IM_COL32(20, 24, 32, 255) : dim ? IM_COL32(140, 146, 160, 255)
                                                                   : IM_COL32(235, 238, 245, 255),
                            tag);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                if (core)
                    ImGui::TextUnformatted("The core");
                else if (kind >= 0)
                {
                    ImGui::TextUnformatted(module_name(kind));
                    if (d.state == "stopped")        ImGui::TextUnformatted("Stopped - a budget falls short.");
                    else if (d.state == "idle_edge") ImGui::TextUnformatted("Idle - needs an edge slot.");
                    else if (d.state == "refitting") ImGui::TextUnformatted("Refitting.");
                    else if (d.state == "not_ready") ImGui::TextUnformatted("In transit.");
                    else if (d.state == "working")   ImGui::TextUnformatted("Working.");
                }
                else
                    ImGui::TextUnformatted("Empty slot");
                if (edge)
                    ImGui::TextDisabled("Edge slot");
                ImGui::EndTooltip();
            }

            if (hit && !core)
            {
                if (m.moving && m.slot >= 0 && a == null_entity)
                {
                    space_command c;
                    c.nation  = nation;
                    c.verb    = static_cast<uint8_t>(space_verb::refit_module);
                    c.subject = st.id;
                    c.slot    = m.slot;
                    c.slot_to = idx;
                    m.pending.push_back(c);
                    m.moving = false;
                    m.slot   = idx;
                }
                else if (m.slot == idx)
                {
                    m.slot   = -1; // a toggle: the picked slot un-picks
                    m.moving = false;
                }
                else
                {
                    m.slot   = idx;
                    m.moving = false;
                }
            }
        }
    ImGui::SetCursorScreenPos({origin.x, origin.y + static_cast<float>(gh) * (cell + gap)});
    ImGui::Dummy({0.0f, 0.0f});
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("Outlined slots are the edge. Press a slot to fit, remove or move.");
    ImGui::PopStyleColor();

    // The pressed slot: the module list for an empty one, Remove / Move for a filled one.
    if (m.slot >= 0)
    {
        const entity_id a = st.slots[static_cast<std::size_t>(m.slot)];
        const bool edge = station_slot_on_edge(st.level, m.slot);
        ImGui::Separator();
        if (a == null_entity)
        {
            // One launch price for every module kind (the payload class is the same),
            // so it heads the list once; the table carries what differs.
            const int pool = w.nations.count(nation) ? w.nations.at(nation).astronauts : 0;
            const space_launch_quote q = quote_space_launch(w, sp, nation, space_asset_class::station_module, st.node);
            ImGui::TextDisabled("Fit a module - each launch %s", fmt::credits(q.cost).c_str());
            if (ImGui::BeginTable("##space_modules", 5, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("Module", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Pwr");
                ImGui::TableSetupColumn("Crew");
                ImGui::TableSetupColumn("LS");
                ImGui::TableSetupColumn("Room");
                ImGui::TableHeadersRow();
                for (int k = 0; k < static_cast<int>(station_fittable_kind_count); ++k)
                {
                    if (!nation_has_mechanic(w, nation, module_key(k)))
                        continue; // the list is what research has unlocked
                    const station_module_spec& spec = stp.modules[static_cast<std::size_t>(k)];
                    std::string why;
                    if (spec.edge_only && !edge)
                        why = "Needs an edge slot.";
                    else if (spec.crew_needed > pool)
                        why = station_refusal(station_result::rejected_no_crew);
                    else if (q.result != space_launch_result::launched)
                        why = launch_refusal(q.result);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    char label[64];
                    std::snprintf(label, sizeof(label), "%s%s##fit%d", module_name(k), spec.edge_only ? " (edge)" : "", k);
                    space_ledger_state::drawn_press p;
                    p.id = k;
                    if (press(label, why.empty(), why, p))
                    {
                        space_command c;
                        c.nation  = nation;
                        c.verb    = static_cast<uint8_t>(space_verb::fit_module);
                        c.subject = st.id;
                        c.slot    = m.slot;
                        c.module  = static_cast<uint8_t>(k);
                        m.pending.push_back(c);
                    }
                    m.module_presses.push_back(p);
                    ImGui::TableNextColumn();
                    const float pw = spec.power_supply > 0.0f ? spec.power_supply : -spec.power_draw;
                    ImGui::TextDisabled("%+.0f", static_cast<double>(pw));
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%d", spec.crew_needed);
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%d", spec.life_support);
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("%d", spec.crew_room);
                }
                ImGui::EndTable();
            }
            if (m.module_presses.empty())
                ImGui::TextDisabled("No module is researched yet.");
        }
        else if (m.slot != st.centre())
        {
            int kind = -1;
            if (const auto ait = w.space_assets.find(a); ait != w.space_assets.end())
                kind = ait->second.module;
            ImGui::Text("%s", module_name(kind));
            if (press("Remove##slot", true, "Frees the slot; nothing is refunded.", m.remove_press))
            {
                space_command c;
                c.nation  = nation;
                c.verb    = static_cast<uint8_t>(space_verb::remove_module);
                c.subject = st.id;
                c.slot    = m.slot;
                m.pending.push_back(c);
                m.moving = false;
            }
            ImGui::SameLine();
            // Move is a toggle: armed, the next empty slot pressed takes the module.
            const char* mv = m.moving ? "Moving - press an empty slot##mv" : "Move##mv";
            if (press(mv, true, "A launch-free refit: it works again a quarter later.", m.move_press))
                m.moving = !m.moving;
        }
    }

    // Templates: save the current layout, or apply a saved one.
    ImGui::Separator();
    ImGui::TextDisabled("Templates");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
    ImGui::InputTextWithHint("##tplname", "Template name", m.template_name, sizeof(m.template_name));
    ImGui::SameLine();
    const std::size_t name_len = std::strlen(m.template_name);
    if (press("Save layout", name_len > 0, name_len > 0 ? "" : "Name the template first.", m.save_press))
    {
        space_command c;
        c.nation       = nation;
        c.verb         = static_cast<uint8_t>(space_verb::save_template);
        c.name         = m.template_name;
        c.layout.level = st.level;
        c.layout.kinds.assign(st.slots.size(), k_no_station_module);
        for (std::size_t i = 0; i < st.slots.size(); ++i)
        {
            if (static_cast<int>(i) == st.centre() || st.slots[i] == null_entity)
                continue;
            if (const auto ait = w.space_assets.find(st.slots[i]); ait != w.space_assets.end())
                c.layout.kinds[i] = ait->second.module;
        }
        m.pending.push_back(c);
    }
    if (const auto tit = w.station_templates.find(nation); tit != w.station_templates.end() && !tit->second.empty())
    {
        for (const auto& [name, t] : tit->second) // std::map: by name
        {
            char label[96];
            std::snprintf(label, sizeof(label), "Apply##tpl_%s", name.c_str());
            space_ledger_state::drawn_press p;
            if (press(label, true, "Launches a module into every slot not yet filled to match.", p))
            {
                space_command c;
                c.nation  = nation;
                c.verb    = static_cast<uint8_t>(space_verb::apply_template);
                c.subject = st.id;
                c.name    = name;
                m.pending.push_back(c);
            }
            p.reason = name;
            m.template_presses.push_back(p);
            ImGui::SameLine();
            int filled = 0;
            for (const uint8_t k : t.kinds)
                if (k != k_no_station_module)
                    ++filled;
            ImGui::Text("%s  (%dx%d, %d modules)", name.c_str(), station_grid_width(t.level),
                        station_grid_height(t.level), filled);
            ImGui::SameLine();
            char dl_label[96];
            std::snprintf(dl_label, sizeof(dl_label), "Delete##tpl_%s", name.c_str());
            if (ImGui::SmallButton(dl_label))
            {
                space_command c;
                c.nation = nation;
                c.verb   = static_cast<uint8_t>(space_verb::delete_template);
                c.name   = name;
                m.pending.push_back(c);
            }
        }
    }
    else
        ImGui::TextDisabled("No saved templates.");

    // Upgrade: grows the grid one ring, quoted like any launch.
    ImGui::Separator();
    if (st.level >= k_station_max_level)
        ImGui::TextDisabled("The grid is at its largest (%dx%d).", gw, gh);
    else
    {
        const std::string_view key = st.level == 0 ? space_keys::grid_7x5 : space_keys::grid_9x7;
        const space_launch_quote q = quote_space_launch(w, sp, nation, space_asset_class::station_module, st.node);
        std::string why;
        if (!nation_has_mechanic(w, nation, key))
            why = "Not researched yet.";
        else if (q.result != space_launch_result::launched)
            why = launch_refusal(q.result);
        char label[48];
        std::snprintf(label, sizeof(label), "Upgrade to %dx%d", station_grid_width(st.level + 1),
                      station_grid_height(st.level + 1));
        if (press(label, why.empty(), why, m.upgrade_press))
        {
            space_command c;
            c.nation  = nation;
            c.verb    = static_cast<uint8_t>(space_verb::upgrade_station);
            c.subject = st.id;
            m.pending.push_back(c);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("a truss launch, %s", fmt::credits(q.cost).c_str());
    }
}

void draw_stations(const world& w, const recipe_registry& reg, entity_id nation, space_ledger_state& m)
{
    const std::vector<entity_id> mine = player_stations(w, nation);
    if (mine.empty())
    {
        ImGui::TextDisabled("No stations. Launch a station core from the Launch view.");
        m.station = null_entity;
        return;
    }
    if (std::find(mine.begin(), mine.end(), m.station) == mine.end())
    {
        m.station = mine.front();
        m.slot    = -1;
        m.moving  = false;
    }
    // The station picker: a target selector (exempt from the toggle rule).
    for (const entity_id id : mine)
    {
        const space_station& st = w.stations.at(id);
        char label[160];
        std::snprintf(label, sizeof(label), "Station - %s (%dx%d)##stn%u",
                      orbital_node_display_name(w, st.node).c_str(), st.width(), st.height(),
                      static_cast<unsigned>(id));
        if (ImGui::Selectable(label, m.station == id) && m.station != id)
        {
            m.station = id;
            m.slot    = -1;
            m.moving  = false;
        }
        space_ledger_state::drawn_press p;
        centre_of_last_item(p.x, p.y);
        p.enabled = true;
        p.id = static_cast<int>(id);
        m.station_rows.push_back(p);
    }
    ImGui::Separator();
    draw_designer(w, reg, nation, w.stations.at(m.station), m);
}

// ── Crew ────────────────────────────────────────────────────────────────────

void draw_crew(const world& w, const recipe_registry& reg, entity_id nation, space_ledger_state& m)
{
    const station_params& stp = reg.stations();
    const auto nit = w.nations.find(nation);
    m.crew.drawn = true;
    m.crew.pool  = nit != w.nations.end() ? nit->second.astronauts : 0;
    m.crew.cap   = astronaut_cap(w, stp, nation);
    int centres = 0;
    if (nit != w.nations.end())
        for (const entity_id bid : nit->second.state_assets)
            if (const auto bit = w.buildings.find(bid);
                bit != w.buildings.end() && bit->second.type == building_type::astronaut_training_centre)
                ++centres;
    m.crew.centres = centres;
    m.crew.running = stp.astronaut_cap_per_centre > 0 ? m.crew.cap / stp.astronaut_cap_per_centre : 0;
    m.crew.rate    = m.crew.pool >= m.crew.cap ? 0 : m.crew.running * std::max(0, stp.astronauts_per_centre);

    ImGui::Text("Astronauts: %d", m.crew.pool);
    ImGui::TextDisabled("Pool cap %d (%d per running centre)", m.crew.cap, stp.astronaut_cap_per_centre);
    ImGui::Text("Training centres: %d (%d running)", m.crew.centres, m.crew.running);
    if (m.crew.running > 0 && m.crew.rate == 0)
        ImGui::TextDisabled("Trained per quarter: 0 - the pool is at its cap");
    else
        ImGui::TextDisabled("Trained per quarter: %d", m.crew.rate);
    if (m.crew.centres == 0)
        ImGui::TextDisabled("Build an Astronaut Training Centre to train crew.");

    // Need against supply: crew aboard every station against what its working set needs.
    ImGui::Separator();
    ImGui::TextDisabled("Where they are");
    int needed = 0, aboard = 0;
    for (const entity_id id : player_stations(w, nation))
    {
        const station_budget b = evaluate_station_budget(w, stp, id, w.current_econ_tick);
        needed += b.crew_needed;
        aboard += b.crew_aboard;
        const bool short_ = b.crew_aboard < b.crew_needed;
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(short_ ? palette::negative : palette::neutral));
        ImGui::TextWrapped("Station - %s: %d aboard, %d needed",
                           orbital_node_display_name(w, w.stations.at(id).node).c_str(), b.crew_aboard,
                           b.crew_needed);
        ImGui::PopStyleColor();
    }
    for (const auto& [id, a] : w.space_assets)
        if (a.owner == nation && a.cls == space_asset_class::crewed_mission && a.state != space_asset_state::lost)
        {
            aboard += a.crew;
            ImGui::TextWrapped("Crewed mission - %s: %d aboard", orbital_node_display_name(w, a.node).c_str(), a.crew);
        }
    m.crew.needed = needed;
    m.crew.aboard = aboard;
    ImGui::TextWrapped("In space: %d aboard, %d needed; %d on the ground", aboard, needed, m.crew.pool);
}

} // namespace

std::string space_outcome_text(const space_command& c, const space_command_outcome& o)
{
    const char* what = verb_name(c.verb);
    switch (o.result)
    {
        case space_command_result::applied:
            if (static_cast<space_verb>(c.verb) == space_verb::apply_template)
                return std::string(what) + ": " + std::to_string(o.fitted) + " launched, "
                     + std::to_string(o.skipped) + " skipped.";
            return std::string(what) + ": done.";
        case space_command_result::queued:
            return std::string(what) + ": pending - no signal. It waits a quarter.";
        case space_command_result::rejected_queue_full:
            return std::string(what) + ": refused - too many orders are waiting.";
        case space_command_result::rejected_not_owner:
            return std::string(what) + ": refused - not yours.";
        case space_command_result::rejected_unknown_nation:
            return std::string(what) + ": refused - you hold no nation.";
        case space_command_result::rejected_invalid:
            return std::string(what) + ": refused - not a valid order.";
        case space_command_result::rejected_refused:
            break;
    }
    const space_verb v = static_cast<space_verb>(c.verb);
    if (v == space_verb::launch)
        return std::string(what) + ": refused - " + launch_refusal(o.launch);
    if (v == space_verb::retask_satellite || v == space_verb::move_satellite)
        return std::string(what) + ": refused.";
    if (o.station == station_result::rejected_launch)
        return std::string(what) + ": refused - " + launch_refusal(o.launch);
    return std::string(what) + ": refused - " + station_refusal(o.station);
}

void draw_space_ledger(const world& w, const recipe_registry& reg, ui_state& s, bool& open)
{
    space_ledger_state& m = s.space;
    for (int i = 0; i < 4; ++i)
        m.tab_x[i] = m.tab_y[i] = -1.0f;
    m.class_presses.clear();
    m.role_presses.clear();
    m.node_rows.clear();
    m.launch_press = {};
    m.launch_cost  = 0.0f;
    m.asset_rows.clear();
    m.retask_presses.clear();
    m.station_rows.clear();
    m.slots.clear();
    m.grid_w = m.grid_h = 0;
    m.module_presses.clear();
    m.remove_press = m.move_press = m.save_press = m.upgrade_press = {};
    m.template_presses.clear();
    m.budget = {};
    m.crew   = {};
    m.pending_orders = 0;
    m.view_top = m.view_bottom = -1.0f;

    if (!open)
    {
        m.slot   = -1;
        m.moving = false;
        return;
    }
    if (!ui::foldout_begin("Space"))
    {
        ui::foldout_end();
        return;
    }
    m.view_top    = ImGui::GetWindowPos().y;
    m.view_bottom = ImGui::GetWindowPos().y + ImGui::GetWindowSize().y;

    // The tab strip; `nav_button` carries the toggle rule — re-clicking the active
    // tab closes the ledger, another tab is an ordinary view change.
    static const char* const k_tabs[4] = {"Launch", "Assets", "Stations", "Crew"};
    for (int i = 0; i < 4; ++i)
    {
        if (i > 0)
            ImGui::SameLine();
        ui::nav_button(k_tabs[i], i, m.view, &open);
        centre_of_last_item(m.tab_x[i], m.tab_y[i]);
    }
    if (m.view < 0 || m.view > 3)
        m.view = 0;

    const entity_id nation = player_nation(w);
    draw_pending_line(w, nation, m);
    if (!m.message.empty())
        ImGui::TextWrapped("%s", m.message.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    if (open)
    {
        if (nation == null_entity)
            ImGui::TextDisabled("You hold no nation.");
        else if (m.view == 0) draw_launch(w, reg, nation, m);
        else if (m.view == 1) draw_assets(w, nation, m);
        else if (m.view == 2) draw_stations(w, reg, nation, m);
        else                  draw_crew(w, reg, nation, m);
    }
    ui::foldout_end();
}

} // namespace ui
