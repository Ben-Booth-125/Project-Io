#pragma once
// The cue watcher (docs/ui/SOUND.md §§ 3-5): once a frame, diff what the frame already shows
// and name the cues it earns. A pure observer of the world and the UI state — it writes
// nothing to either, and draws no number from the world's seeded streams.

#include "world/world.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

struct ui_state;
class recipe_registry;

namespace audio
{

class mixer;

struct frame_view
{
    const world*                       w   = nullptr;
    const ui_state*                    ui  = nullptr;
    const recipe_registry*             reg = nullptr;
    int                                speed = 0;     ///< sim_loop::speed(); 0 = paused
};

class cue_watch
{
public:
    /// Forget every baseline: the next observe() only records state and fires nothing. Call
    /// on entering a game (new, load, quickstart) so what already stands stays silent.
    void rearm() { m_armed = false; m_day = -1; }

    /// Diff the frame and request cues. `now_ms` is wall clock, for the mixer's merge window.
    void observe(const frame_view& f, mixer& mx, std::uint64_t now_ms);

    /// The press half (an ImGui read); call after the shell has drawn, before ImGui::Render.
    /// Separate so it can run where ImGui's frame state is still current.
    static bool generic_press_this_frame();
    static bool disabled_click_this_frame();

private:
    bool on_canvas(const world& w, const ui_state& ui, entity_id body) const;

    bool                                   m_armed = false;
    std::int64_t                           m_day = -1;
    int                                    m_speed = 0;
    std::uint32_t                          m_ledgers = 0;
    int                                    m_overlay = 0;
    std::set<int>                          m_alerts;
    std::set<std::pair<entity_id, entity_id>> m_wars;
    std::set<std::tuple<entity_id, entity_id, entity_id>> m_battles;
    std::set<entity_id>                    m_building;   ///< own sites still under construction
    std::set<entity_id>                    m_stalled;    ///< own sites paused, per the stall read
    std::size_t                            m_tech_held = 0;
    std::map<entity_id, int>               m_assets;     ///< own asset -> state
    std::map<entity_id, int>               m_units;      ///< own space unit -> days remaining
};

} // namespace audio
