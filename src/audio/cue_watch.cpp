#include "cue_watch.hpp"

#include "mixer.hpp"
#include "ui_cues.hpp"

#include "ui/alerts.hpp"
#include "ui/construction_panel.hpp"
#include "ui/ui_state.hpp"
#include "world/space_assets.hpp"
#include "world/space_units.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <tuple>

namespace audio
{
namespace
{

/// The ledger flags as one mask, so an open or a close is one diff (nav_pane.cpp's
/// close_all_panels lists the same set).
std::uint32_t ledger_mask(const ui_state& ui)
{
    const bool flags[] = {
        ui.show_corporation_panel, ui.show_balance_ledger, ui.show_market_ledger,
        ui.show_convoys_ledger, ui.show_military_ledger, ui.show_diplomacy_ledger,
        ui.show_space_ledger, ui.show_construction_panel, ui.show_tile_ledger,
        ui.show_generation_ledger, ui.show_decision_feed, ui.show_strategy_readout,
        ui.show_tech_tree,
    };
    std::uint32_t m = 0;
    for (std::size_t i = 0; i < std::size(flags); ++i)
        if (flags[i]) m |= 1u << i;
    return m;
}

/// The alert's severity tier (SOUND.md § 3: per tier, not per kind).
const char* alert_cue(ui::alert_kind k)
{
    switch (k)
    {
    case ui::alert_kind::wars:
    case ui::alert_kind::low_runway:           return "alert_high";
    case ui::alert_kind::research_not_set:     return "alert_low";
    default:                                   return "alert_mid";
    }
}

entity_id tile_body(const world& w, entity_id tile)
{
    auto it = w.tiles.find(tile);
    return it == w.tiles.end() ? null_entity : it->second.body;
}

std::size_t tech_held(const world& w, entity_id nation)
{
    auto it = w.nation_tech.find(nation);
    return it == w.nation_tech.end() ? 0 : it->second.held.count() + it->second.es_held.count();
}

} // namespace

bool cue_watch::on_canvas(const world& w, const ui_state& ui, entity_id body) const
{
    if (body == null_entity) return false;
    switch (ui.primary_level)
    {
    case canvas_level::solar: return true;
    case canvas_level::circumplanetary:
    {
        if (body == ui.active_body) return true;
        auto it = w.bodies.find(body);
        return it != w.bodies.end() && it->second.parent == ui.active_body;
    }
    default: return body == ui.active_body;
    }
}

bool cue_watch::generic_press_this_frame()
{
    // A button fires on release: last frame's active item, released this frame while still
    // hovered. Canvas input is not an ImGui item, so the map's own clicks never count.
    ImGuiContext& g = *ImGui::GetCurrentContext();
    return ImGui::IsMouseReleased(ImGuiMouseButton_Left)
        && g.ActiveIdPreviousFrame != 0 && g.ActiveId == 0
        && g.HoveredId == g.ActiveIdPreviousFrame;
}

bool cue_watch::disabled_click_this_frame()
{
    // A click on a disabled button: ImGui swallows it, but flags the item it discarded.
    ImGuiContext& g = *ImGui::GetCurrentContext();
    return ImGui::IsMouseClicked(ImGuiMouseButton_Left) && g.HoveredIdIsDisabled;
}

void cue_watch::observe(const frame_view& f, mixer& mx, std::uint64_t now_ms)
{
    if (!f.w || !f.ui) return;
    const world&    w  = *f.w;
    const ui_state& ui = *f.ui;
    const entity_id me = player_nation(w);
    std::vector<const char*> cues;
    auto fire = [&](const char* c) { if (m_armed) cues.push_back(c); };

    // ---- UI: ledgers, the overlay toggle, the presses only the UI can name ----------------
    std::vector<std::string> ui_notes;
    ui_notes.swap(ui_cue_notes());
    bool ui_claimed = !ui_notes.empty();
    const std::uint32_t ledgers = ledger_mask(ui);
    if (ledgers != m_ledgers)
    {
        // A switch from one ledger to another is an open; only "none left open" is a close.
        if (ledgers & ~m_ledgers) fire("ui_ledger_open");
        else if (ledgers == 0)    fire("ui_ledger_close");
        ui_claimed = true;
        m_ledgers = ledgers;
    }
    const int overlay = static_cast<int>(ui.overlay);
    if (overlay != m_overlay)
    {
        fire(overlay != 0 ? "ui_toggle_on" : "ui_toggle_off");
        ui_claimed = true;
        m_overlay = overlay;
    }

    // ---- Time ------------------------------------------------------------------------------
    if (f.speed != m_speed)
    {
        if (m_speed == 0)      fire("time_resume");
        else if (f.speed == 0) fire("time_pause");
        else                   fire(f.speed > m_speed ? "time_faster" : "time_slower");
        ui_claimed = true;
        m_speed = f.speed;
    }
    if (m_armed)
        for (const std::string& n : ui_notes) cues.push_back(n.c_str());
    if (!ui_claimed && generic_press_this_frame()) fire("ui_press");
    else if (!ui_claimed && disabled_click_this_frame()) fire("ui_refused");

    // ---- Wars the player is a party to (global) -------------------------------------------
    for (const auto& [key, war] : w.wars)
    {
        if (me == null_entity || (war.declarer != me && war.defender != me)) continue;
        if (m_wars.insert(key).second) fire("war_declared");
    }
    for (auto it = m_wars.begin(); it != m_wars.end();)
        it = w.wars.count(*it) ? std::next(it) : m_wars.erase(it);

    // ---- Battles: one cue as a battle begins (on canvas). Won and lost are silent (Ben,
    //      2026-11-22: "drop the sound for a battle being won or lost").
    std::set<std::tuple<entity_id, entity_id, entity_id>> battles;
    for (const active_battle& b : w.battles)
    {
        if (me == null_entity || (b.attacker != me && b.defender != me)) continue;
        auto key = std::make_tuple(b.tile, b.attacker, b.defender);
        battles.insert(key);
        if (!m_battles.count(key) && on_canvas(w, ui, tile_body(w, b.tile))) fire("battle_begin");
    }
    m_battles = std::move(battles);

    // ---- Once a day: construction, stalls, research, space --------------------------------
    bool stall_fired = false;
    if (w.current_day_tick != m_day)
    {
        m_day = w.current_day_tick;
        std::set<entity_id> building, stalled;
        for (const auto& [id, b] : w.buildings)
        {
            if (!is_player_owned(w, id)) continue;
            if (b.ticks_remaining > 0)
            {
                building.insert(id);
                if (f.reg && ui::read_construction(w, *f.reg, b.type, b.target_resource, b.recipe, b.tile,
                                               b.state_idle, construction_days_left(b)).paused)
                {
                    stalled.insert(id);
                    if (!m_stalled.count(id) && on_canvas(w, ui, tile_body(w, b.tile)))
                    {
                        fire("construction_stalled");
                        stall_fired = true;
                    }
                }
            }
            else if (m_building.count(id) && on_canvas(w, ui, tile_body(w, b.tile)))
                fire("construction_complete");   // was building, still exists, now done
        }
        m_building = std::move(building);
        m_stalled  = std::move(stalled);

        const std::size_t held = tech_held(w, me);
        if (held > m_tech_held) fire("research_complete");
        m_tech_held = held;

        std::map<entity_id, int> assets;
        for (const auto& [id, a] : w.space_assets)
        {
            if (a.owner != me || me == null_entity) continue;
            const int st = static_cast<int>(a.state);
            auto prev = m_assets.find(id);
            if (prev == m_assets.end() && a.state == space_asset_state::in_transit
                && on_canvas(w, ui, a.dest.body))
                fire("space_launch");
            else if (prev != m_assets.end() && prev->second == int(space_asset_state::in_transit)
                     && a.state == space_asset_state::active && on_canvas(w, ui, a.dest.body))
                fire("space_arrival");
            assets[id] = st;
        }
        m_assets = std::move(assets);

        std::map<entity_id, int> units;
        for (const auto& [id, u] : w.space_units)
        {
            if (u.owner != me || me == null_entity) continue;
            auto prev = m_units.find(id);
            if (prev == m_units.end() && u.days_remaining > 0 && on_canvas(w, ui, u.node.body))
                fire("space_launch");
            else if (prev != m_units.end() && prev->second > 0 && u.days_remaining == 0
                     && on_canvas(w, ui, u.dest.body))
                fire("space_arrival");
            units[id] = u.days_remaining;
        }
        m_units = std::move(units);
    }

    // ---- Alerts: a kind first appearing (global) ------------------------------------------
    std::set<int> alerts;
    for (const ui::alert_drawn& a : ui::alerts_drawn()) alerts.insert(static_cast<int>(a.kind));
    for (int k : alerts)
    {
        if (m_alerts.count(k)) continue;
        const auto kind = static_cast<ui::alert_kind>(k);
        // A stall heard on the canvas this frame already speaks for its alert.
        if (kind == ui::alert_kind::construction_stalled && stall_fired) continue;
        fire(alert_cue(kind));
    }
    m_alerts = std::move(alerts);

    for (const char* c : cues) mx.request(c, now_ms);
    m_armed = true;
}

} // namespace audio
