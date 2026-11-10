#include "alerts.hpp"

#include "construction_panel.hpp" // read_construction: the site's own stall test
#include "diplomacy_ledger.hpp"   // open_declaration: the war notice's Answer
#include "header_panel.hpp"       // player_runway_quarters: one runway, in two places
#include "market_ledger.hpp"      // collect_trade_points: the Trades tab's own read
#include "nav_pane.hpp"           // close_all_panels: a click-through yields the column
#include "presentation.hpp"
#include "ui_state.hpp"

#include "world/economy_system.hpp"
#include "world/nation_ai.hpp"    // research_queue_idle
#include "world/war.hpp"          // at_war
#include "world/world.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <set>

namespace ui {

namespace {

std::vector<alert_drawn>& drawn_rows()
{
    static std::vector<alert_drawn> rows;
    return rows;
}

/// Under this many quarters of cushion the runway is the player's problem: the same
/// bar the header colours red (HEADER.md § Contents, Runway).
constexpr int k_low_runway_quarters = 8;

constexpr ImU32 k_alert_fill  = IM_COL32(138, 84, 26, 245);
constexpr ImU32 k_alert_hover = IM_COL32(170, 106, 34, 250);
constexpr ImU32 k_alert_press = IM_COL32(194, 124, 44, 255);

const char* alert_why(alert_kind k)
{
    switch (k)
    {
    case alert_kind::research_not_set:
        return "Your nation has nothing in its research queue, so its research points buy nothing.\n"
               "Opens Research.";
    case alert_kind::idle_buildings:
        return "These buildings produced nothing last quarter. Opens the Buildings tab, which says why.";
    case alert_kind::wars:
        return "Your nation is at war. Opens the Wars tab.";
    case alert_kind::low_runway:
        return "Under two years of cushion at last quarter's burn. Opens the Budget.";
    case alert_kind::construction_stalled:
        return "These sites made no progress: a material is short, or the treasury cannot fund them.\n"
               "Opens Construction.";
    case alert_kind::deposit_exhausted:
        return "These extraction sites have spent their deposit. Opens the Buildings tab.";
    case alert_kind::trades_exceed_points:
        return "Your manual trades ask for more trade points than you have reserved, so some will not\n"
               "run in full. Opens the Trades tab.";
    }
    return "";
}

/// The surface that answers the alert. The same doors the rail and the tabs are: the
/// column yields first, and the Market ledger arms its lens as its slot does.
void open_answer(alert_kind k, ui_state& ui)
{
    close_all_panels(ui);
    switch (k)
    {
    case alert_kind::research_not_set:
        ui.show_tech_tree = true;
        break;
    case alert_kind::idle_buildings:
    case alert_kind::deposit_exhausted:
        ui.show_construction_panel = true;
        ui.construction.panel_view = 1; // Buildings
        break;
    case alert_kind::construction_stalled:
        ui.show_construction_panel = true;
        ui.construction.panel_view = 0; // Construction
        break;
    case alert_kind::wars:
        ui.show_diplomacy_ledger = true;
        ui.diplomacy.view = 1; // Wars
        break;
    case alert_kind::low_runway:
        ui.show_balance_ledger = true;
        break;
    case alert_kind::trades_exceed_points:
        ui.show_market_ledger = true;
        ui.market_ledger_view = 1; // Trades
        ui.overlay = overlay_mode::market;
        break;
    }
}

std::string counted(int n, const char* one, const char* many)
{
    char buf[64];
    std::snprintf(buf, sizeof buf, "%d %s", n, n == 1 ? one : many);
    return buf;
}

} // namespace

std::vector<alert_drawn> read_alerts(const world& w, const recipe_registry& reg,
                                     const std::vector<float>& balance_history,
                                     const economy_report& report)
{
    std::vector<alert_drawn> out;
    const entity_id nation = player_nation(w);
    if (nation == null_entity)
        return out; // nobody seated, or a world with no nation: nothing is the player's

    const auto add = [&out](alert_kind k, std::string text) {
        alert_drawn a;
        a.kind = k;
        a.text = std::move(text);
        out.push_back(std::move(a));
    };

    if (research_queue_idle(w, nation))
        add(alert_kind::research_not_set, "Research not set");

    // The last quarter's verdicts, aggregated. A building the player idled on purpose
    // is a decision, not a surprise, so it does not count; a spent deposit is its own
    // alert rather than an idle one.
    const entity_id actor = player_actor(w);
    int idle = 0, exhausted = 0;
    for (const building_report& r : report.buildings)
    {
        if (r.corp != actor && r.corp != w.player_entity)
            continue;
        const auto bit = w.buildings.find(r.building);
        if (bit == w.buildings.end())
            continue;
        if (r.exhausted)
            ++exhausted;
        else if (r.idle && !bit->second.decommissioned)
            ++idle;
    }
    if (idle > 0)
        add(alert_kind::idle_buildings, counted(idle, "idle building", "idle buildings"));

    int wars = 0;
    for (const auto& [other, n] : w.nations)
    {
        (void)n;
        if (other != nation && at_war(w, nation, other))
            ++wars;
    }
    if (wars > 0)
        add(alert_kind::wars, wars == 1 ? std::string("At war") : counted(wars, "war", "wars"));

    const int runway = player_runway_quarters(w, balance_history);
    if (runway >= 0 && runway < k_low_runway_quarters)
    {
        char buf[32];
        std::snprintf(buf, sizeof buf, "Runway %dq", runway);
        add(alert_kind::low_runway, buf);
    }

    int stalled = 0;
    for (const auto& [id, b] : w.buildings)
    {
        if (b.ticks_remaining <= 0 || !is_player_owned(w, id))
            continue;
        if (read_construction(w, reg, b.type, b.target_resource, b.recipe, b.tile, b.state_idle,
                              construction_days_left(b)).paused)
            ++stalled;
    }
    if (stalled > 0)
        add(alert_kind::construction_stalled, counted(stalled, "site stalled", "sites stalled"));

    if (exhausted > 0)
        add(alert_kind::deposit_exhausted, counted(exhausted, "deposit exhausted", "deposits exhausted"));

    const trade_points_record tp = collect_trade_points(w);
    if (tp.manual_asked > tp.reserve_used + 1e-3f)
        add(alert_kind::trades_exceed_points, "Trades exceed trade points");

    return out;
}

void draw_alert_strip(const world& w, const recipe_registry& reg,
                      const std::vector<float>& balance_history, const economy_report& report,
                      ui_state& ui, float left, float right)
{
    // Recomputed per in-game day, and a few times a second while paused, so an answer
    // the player gives at a stopped clock clears the alert without a tick. Its reads
    // walk the player's estate, which a frame need not pay for.
    static int cached_day   = -1;
    static int cached_frame = -1000;
    static std::vector<alert_drawn> cached;
    const int frame = ImGui::GetFrameCount();
    if (cached_day != w.current_day_tick || frame - cached_frame >= 15 || frame < cached_frame)
    {
        cached       = read_alerts(w, reg, balance_history, report);
        cached_day   = w.current_day_tick;
        cached_frame = frame;
    }

    std::vector<alert_drawn>& rows = drawn_rows();
    rows = cached;
    if (rows.empty())
        return;

    const ImGuiStyle& style = ImGui::GetStyle();
    const float       pad_x = style.FramePadding.x;
    const float       gap   = style.ItemSpacing.x;
    const float       room  = std::max(0.0f, right - left);

    // One row while it fits, wrapped when it does not: the strip is a guaranteed-fit
    // surface, and an alert clipped off its end is an alert unsaid.
    float row_w = 0.0f, widest = 0.0f;
    int   lines = 1;
    for (const alert_drawn& a : rows)
    {
        const float bw = ImGui::CalcTextSize(a.text.c_str()).x + pad_x * 2.0f;
        const float wp = style.WindowPadding.x * 2.0f;
        if (row_w > 0.0f && row_w + gap + bw + wp > room)
        {
            widest = std::max(widest, row_w);
            row_w  = 0.0f;
            ++lines;
        }
        row_w += (row_w > 0.0f ? gap : 0.0f) + bw;
    }
    widest = std::max(widest, row_w);

    const float frame_h = ImGui::GetFrameHeight();
    const ImVec2 size{widest + style.WindowPadding.x * 2.0f,
                      static_cast<float>(lines) * frame_h
                          + static_cast<float>(lines - 1) * style.ItemSpacing.y
                          + style.WindowPadding.y * 2.0f};
    ImGui::SetNextWindowPos({(left + right - size.x) * 0.5f, header_panel_height});
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowBgAlpha(0.0f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##alert_strip", nullptr, flags);

    ImGui::PushStyleColor(ImGuiCol_Button, k_alert_fill);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, k_alert_hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, k_alert_press);
    row_w = 0.0f;
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        alert_drawn& a = rows[i];
        const float bw = ImGui::CalcTextSize(a.text.c_str()).x + pad_x * 2.0f;
        if (row_w > 0.0f && row_w + gap + bw + style.WindowPadding.x * 2.0f <= room)
        {
            ImGui::SameLine();
            row_w += gap + bw;
        }
        else
            row_w = bw;
        ImGui::PushID(static_cast<int>(a.kind));
        if (ImGui::Button(a.text.c_str()))
            open_answer(a.kind, ui);
        ImGui::PopID();
        const ImVec2 mn = ImGui::GetItemRectMin();
        const ImVec2 mx = ImGui::GetItemRectMax();
        a.x = (mn.x + mx.x) * 0.5f;
        a.y = (mn.y + mx.y) * 0.5f;
        ImGui::SetItemTooltip("%s", alert_why(a.kind));
    }
    ImGui::PopStyleColor(3);
    ImGui::End();
}

const std::vector<alert_drawn>& alerts_drawn() { return drawn_rows(); }

void draw_war_notices(const world& w, ui_state& ui, float left, float right)
{
    war_notice_state& ns = ui.war_notices;
    const entity_id me = player_nation(w);
    if (me == null_entity)
        return; // nobody seated (spectating): no war is the player's
    if (!ns.armed)
    {
        // The wars already standing on a new world or a load are not news; nor is a
        // nation already ended there.
        ns.seen.clear();
        ns.open.clear();
        ns.ended_seen.clear();
        ns.ended_open.clear();
        for (const auto& [key, war] : w.wars)
            ns.seen.insert(key);
        for (const auto& [nid, nc] : w.nations)
            if (nc.ended)
                ns.ended_seen.insert(nid);
        ns.armed = true;
        return;
    }
    for (const auto& [key, war] : w.wars)
    {
        const bool mine = war.declarer == me || war.defender == me;
        if (ns.seen.insert(key).second && mine)
            ns.open.push_back(key);
    }
    // A nation's end raises the notice when the player's nation took part in its last
    // war, or is the nation that ended (Ben, 2026-11-09; LAYOUT.md § War notice). Its last
    // war is the latest closed war it was party to (the cession that ended it).
    for (const auto& [nid, nc] : w.nations)
    {
        if (!nc.ended || !ns.ended_seen.insert(nid).second)
            continue;
        bool took_part = nid == me;
        for (auto it = w.war_history.rbegin(); it != w.war_history.rend() && !took_part; ++it)
            if (it->declarer == nid || it->defender == nid)
            {
                if (it->gainer != null_entity) // the cession that took its last province
                {
                    took_part = it->declarer == me || it->defender == me;
                    break;
                }
            }
        if (took_part)
            ns.ended_open.push_back(nid);
    }
    // A war that ended leaves nothing to notice, and may be declared afresh.
    for (auto it = ns.seen.begin(); it != ns.seen.end();)
        it = w.wars.count(*it) == 0 ? ns.seen.erase(it) : std::next(it);
    ns.open.erase(std::remove_if(ns.open.begin(), ns.open.end(),
                                 [&](const std::pair<entity_id, entity_id>& k) { return w.wars.count(k) == 0; }),
                  ns.open.end());
    if (ns.open.empty() && ns.ended_open.empty())
        return;

    const auto name_of = [&](entity_id n) {
        const auto it = w.nations.find(n);
        std::string s = it != w.nations.end() && !it->second.name.empty() ? it->second.name : std::string("A nation");
        if (n == me)
            s += " (you)";
        return s;
    };
    const float width = 420.0f;
    ImGui::SetNextWindowPos({(left + right - width) * 0.5f, header_panel_height + ImGui::GetFrameHeight() * 2.2f});
    ImGui::SetNextWindowSize({width, 0.0f});
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                       ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin("##war_notice", nullptr, flags);
    // A nation's end (Ben, 2026-11-09). The player's own: the campaign is lost, and the
    // session goes on as a spectator or returns to the menu.
    entity_id ended_dismissed = null_entity;
    for (std::size_t i = 0; i < ns.ended_open.size(); ++i)
    {
        const entity_id nid = ns.ended_open[i];
        ImGui::PushID(static_cast<int>(10000 + i));
        if (nid == me)
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::front_hostile), "The campaign is lost");
            ImGui::TextWrapped("%s has ceded its last province and ended.", name_of(nid).c_str());
            if (ImGui::Button("Spectate"))
            {
                ui.spectating   = true; // nobody holds the seat: every nation is scored as a rival
                ended_dismissed = nid;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Watch the rest of the campaign. The rivals play on.");
            ImGui::SameLine();
            if (ImGui::Button("Return to menu"))
            {
                ns.request_menu = true;
                ended_dismissed = nid;
            }
        }
        else
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::front_hostile), "A nation has ended");
            ImGui::TextWrapped("%s has ceded its last province and ended.", name_of(nid).c_str());
            if (ImGui::Button("Dismiss"))
                ended_dismissed = nid;
        }
        if (i + 1 < ns.ended_open.size() || !ns.open.empty())
            ImGui::Separator();
        ImGui::PopID();
    }
    if (ended_dismissed != null_entity)
        ns.ended_open.erase(std::remove(ns.ended_open.begin(), ns.ended_open.end(), ended_dismissed),
                            ns.ended_open.end());
    std::pair<entity_id, entity_id> dismissed{null_entity, null_entity};
    for (std::size_t i = 0; i < ns.open.size(); ++i)
    {
        const auto key = ns.open[i];
        const war_record& war = w.wars.at(key);
        const entity_id other = war.declarer == me ? war.defender : war.declarer;
        ImGui::PushID(static_cast<int>(i));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::front_hostile), "War declared");
        ImGui::TextWrapped("%s has declared war on %s.", name_of(war.declarer).c_str(), name_of(war.defender).c_str());
        const bool to_answer = war.defender == me && !war.answered;
        if (ImGui::Button(to_answer ? "Answer" : "Open Wars"))
        {
            if (to_answer)
                open_declaration(w, ui, other);
            else
            {
                close_all_panels(ui);
                ui.show_diplomacy_ledger = true;
                ui.diplomacy.view = 1; // Wars
            }
            dismissed = key;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(to_answer ? "Name goals among their provinces. Answering is free; your fronts may push already."
                                        : "Opens the Wars tab.");
        ImGui::SameLine();
        if (ImGui::Button("Dismiss"))
            dismissed = key;
        if (i + 1 < ns.open.size())
            ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::End();
    if (dismissed.first != null_entity)
        ns.open.erase(std::remove(ns.open.begin(), ns.open.end(), dismissed), ns.open.end());
}

} // namespace ui
